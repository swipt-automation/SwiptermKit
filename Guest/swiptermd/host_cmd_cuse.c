#include "swiptermd.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fuse.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

enum {
    STUB_MSG_EXIT = 1,
    STUB_MSG_CANCEL = 2,
};

struct stub_ctl_msg {
    uint32_t type;
    int32_t value;
};

#define CMD_BUF_CAP 32768
#define CUSE_IO_MAX (128 * 1024)

typedef struct cmd_chan {
    int guest_fd;
    int vsock_fd;
    bool guest_is_source;
    bool guest_epoll;
    bool eof;
    ev_node_t guest_ev;
    ev_node_t vsock_ev;
    uint8_t buf[CMD_BUF_CAP];
    size_t off;
    size_t len;
    struct cmd_invocation *inv;
} cmd_chan_t;

typedef struct cmd_invocation {
    uint32_t invocation_id;
    uint32_t start_req_id;
    int stub_fd;
    ev_node_t stub_ev;
    cmd_chan_t chans[3];
    bool finish_received;
    int32_t exit_status;
    bool cancelled;
    struct cmd_invocation *next;
} cmd_invocation_t;

typedef struct cuse_device {
    uint32_t device_id;
    int cuse_fd;
    int parent_fd;
    dev_t rdev;
    char leaf[NAME_MAX + 1];
    ev_node_t ev;
    struct cuse_device *next;
} cuse_device_t;

typedef struct cuse_pending {
    uint32_t req_id;
    uint16_t op;
    uint64_t fuse_unique;
    cuse_device_t *dev;
    struct cuse_pending *next;
} cuse_pending_t;

static int g_cmd_listen_fd = -1;
static ev_node_t g_cmd_listen_ev;
static char **g_cmd_names;
static size_t g_cmd_count;
static uint32_t g_next_inv_id = 1;
static cmd_invocation_t *g_invocations;

static cuse_device_t *g_cuse_devs;
static cuse_pending_t *g_cuse_pending;

static void cmd_invocation_destroy(cmd_invocation_t *inv);
static void cmd_check_done(cmd_invocation_t *inv);

static void cmd_chan_close(cmd_chan_t *ch) {
    ev_del(&ch->guest_ev);
    ev_del(&ch->vsock_ev);
    if (ch->guest_fd >= 0) {
        close(ch->guest_fd);
        ch->guest_fd = -1;
    }
    if (ch->vsock_fd >= 0) {
        close(ch->vsock_fd);
        ch->vsock_fd = -1;
    }
    ch->eof = true;
    ch->off = ch->len = 0;
}

static void cmd_chan_pump(cmd_chan_t *ch) {
    if (ch->eof)
        return;

    if (ch->guest_is_source) {
        for (;;) {
            while (ch->off < ch->len) {
                ssize_t w = send(ch->vsock_fd, ch->buf + ch->off,
                                 ch->len - ch->off, MSG_NOSIGNAL);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (ch->guest_epoll)
                            ev_mod(&ch->guest_ev, 0);
                        ev_mod(&ch->vsock_ev, EPOLLOUT);
                        return;
                    }
                    cmd_chan_close(ch);
                    cmd_check_done(ch->inv);
                    return;
                }
                ch->off += (size_t)w;
            }
            ch->off = ch->len = 0;

            ssize_t n = read(ch->guest_fd, ch->buf, sizeof(ch->buf));
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (ch->guest_epoll)
                        ev_mod(&ch->guest_ev, EPOLLIN);
                    ev_mod(&ch->vsock_ev, 0);
                    return;
                }
                cmd_chan_close(ch);
                cmd_check_done(ch->inv);
                return;
            }
            if (n == 0) {
                cmd_chan_close(ch);
                cmd_check_done(ch->inv);
                return;
            }
            ch->len = (size_t)n;
            if (ch->guest_epoll) {
                ssize_t w = send(ch->vsock_fd, ch->buf, ch->len, MSG_NOSIGNAL);
                if (w > 0)
                    ch->off = (size_t)w;
                if (ch->off < ch->len) {
                    ev_mod(&ch->guest_ev, 0);
                    ev_mod(&ch->vsock_ev, EPOLLOUT);
                    return;
                }
                ch->off = ch->len = 0;
                ev_mod(&ch->guest_ev, EPOLLIN);
                ev_mod(&ch->vsock_ev, 0);
                return;
            }
        }
    } else {
        for (;;) {
            while (ch->off < ch->len) {
                size_t chunk = ch->len - ch->off;
                if (ch->guest_epoll && chunk > 4096)
                    chunk = 4096;
                ssize_t w = write(ch->guest_fd, ch->buf + ch->off, chunk);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ev_mod(&ch->vsock_ev, 0);
                        if (ch->guest_epoll)
                            ev_mod(&ch->guest_ev, EPOLLOUT);
                        return;
                    }
                    cmd_chan_close(ch);
                    cmd_check_done(ch->inv);
                    return;
                }
                ch->off += (size_t)w;
                if (ch->guest_epoll && ch->off < ch->len) {
                    ev_mod(&ch->vsock_ev, 0);
                    ev_mod(&ch->guest_ev, EPOLLOUT);
                    return;
                }
            }
            ch->off = ch->len = 0;

            ssize_t n = recv(ch->vsock_fd, ch->buf, sizeof(ch->buf), 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    ev_mod(&ch->vsock_ev, EPOLLIN);
                    if (ch->guest_epoll)
                        ev_mod(&ch->guest_ev, 0);
                    return;
                }
                cmd_chan_close(ch);
                cmd_check_done(ch->inv);
                return;
            }
            if (n == 0) {
                cmd_chan_close(ch);
                cmd_check_done(ch->inv);
                return;
            }
            ch->len = (size_t)n;
            if (ch->guest_epoll) {
                ev_mod(&ch->vsock_ev, 0);
                ev_mod(&ch->guest_ev, EPOLLOUT);
                return;
            }
        }
    }
}

static void on_cmd_chan_event(ev_node_t *node, uint32_t events) {
    (void)events;
    cmd_chan_t *ch = node->ctx;
    cmd_chan_pump(ch);
}

static void cmd_check_done(cmd_invocation_t *inv) {
    if (!inv || !inv->finish_received)
        return;
    if (!inv->chans[1].eof || !inv->chans[2].eof)
        return;

    struct stub_ctl_msg msg = {
        .type = STUB_MSG_EXIT,
        .value = inv->exit_status,
    };
    if (inv->stub_fd >= 0)
        (void)write_all_blocking(inv->stub_fd, &msg, sizeof(msg));
    cmd_invocation_destroy(inv);
}

static void cmd_invocation_destroy(cmd_invocation_t *inv) {
    if (!inv)
        return;
    cmd_invocation_t **pp = &g_invocations;
    while (*pp) {
        if (*pp == inv) {
            *pp = inv->next;
            break;
        }
        pp = &(*pp)->next;
    }
    ev_del(&inv->stub_ev);
    if (inv->stub_fd >= 0) {
        close(inv->stub_fd);
        inv->stub_fd = -1;
    }
    for (int i = 0; i < 3; i++)
        cmd_chan_close(&inv->chans[i]);
    free(inv);
}

static void cmd_send_cancel(cmd_invocation_t *inv, int32_t sig) {
    if (inv->cancelled)
        return;
    inv->cancelled = true;
    uint8_t msg[SWIPTERMD_CMD_CANCEL_SIZE];
    swiptermd_enc_cmd_cancel(msg, inv->invocation_id, sig);
    ctrl_send_frame(SWIPTERMD_OP_CMD_CANCEL, 0, msg, sizeof(msg));
}

static void on_stub_event(ev_node_t *node, uint32_t events) {
    cmd_invocation_t *inv = node->ctx;
    if (events & EPOLLIN) {
        struct stub_ctl_msg msg;
        ssize_t n = read(inv->stub_fd, &msg, sizeof(msg));
        if (n == (ssize_t)sizeof(msg) && msg.type == STUB_MSG_CANCEL) {
            cmd_send_cancel(inv, msg.value);
            cmd_invocation_destroy(inv);
            return;
        }
        if (n <= 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            if (!inv->finish_received)
                cmd_send_cancel(inv, SIGKILL);
            cmd_invocation_destroy(inv);
            return;
        }
    }
    if (events & (EPOLLHUP | EPOLLERR)) {
        if (!inv->finish_received)
            cmd_send_cancel(inv, SIGKILL);
        cmd_invocation_destroy(inv);
    }
}

static int recv_stub_header(int fd, uint32_t *payload_len_out, int fds_out[3]) {
    uint8_t hdr[4];
    struct iovec iov = {.iov_base = hdr, .iov_len = sizeof(hdr)};
    union {
        struct cmsghdr align;
        uint8_t buf[CMSG_SPACE(sizeof(int) * 3)];
    } cmsg_u;
    memset(&cmsg_u, 0, sizeof(cmsg_u));

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_u.buf;
    msg.msg_controllen = sizeof(cmsg_u.buf);

    ssize_t n;
    do {
        n = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
    } while (n < 0 && errno == EINTR);
    if (n != (ssize_t)sizeof(hdr))
        return -EIO;

    fds_out[0] = fds_out[1] = fds_out[2] = -1;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS &&
            cmsg->cmsg_len >= CMSG_LEN(sizeof(int) * 3)) {
            memcpy(fds_out, CMSG_DATA(cmsg), sizeof(int) * 3);
            break;
        }
    }
    if (fds_out[0] < 0 || fds_out[1] < 0 || fds_out[2] < 0)
        return -EIO;

    *payload_len_out = swiptermd_get_be32(hdr);
    return 0;
}

static void on_cmd_accept(ev_node_t *node, uint32_t events) {
    (void)node;
    if (!(events & EPOLLIN))
        return;

    for (;;) {
        int cfd = accept4(g_cmd_listen_fd, NULL, NULL, SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }

        uint32_t plen = 0;
        int guest_fds[3] = {-1, -1, -1};
        if (recv_stub_header(cfd, &plen, guest_fds) < 0 || plen == 0 ||
            plen > SWIPTERMD_MAX_BODY_LEN - 64) {
            for (int i = 0; i < 3; i++) {
                if (guest_fds[i] >= 0)
                    close(guest_fds[i]);
            }
            close(cfd);
            continue;
        }

        uint8_t *payload = malloc(plen);
        if (!payload || read_all_blocking(cfd, payload, plen) < 0) {
            free(payload);
            for (int i = 0; i < 3; i++)
                close(guest_fds[i]);
            close(cfd);
            continue;
        }

        swiptermd_buf_r_t pr = swiptermd_buf_r_init(payload, plen);
        char name[256];
        if (!swiptermd_r_str_copy(&pr, name, sizeof(name))) {
            free(payload);
            for (int i = 0; i < 3; i++)
                close(guest_fds[i]);
            close(cfd);
            continue;
        }

        if (strcmp(name, "open") == 0 || strcmp(name, "xdg-open") == 0)
            sock_mirror_scan_now();

        uint32_t inv_id = g_next_inv_id++;
        if (inv_id == 0)
            inv_id = g_next_inv_id++;
        uint64_t token = 0;
        (void)getrandom(&token, sizeof(token), 0);

        static const uint32_t roles[3] = {
            SWIPTERMD_STREAM_CMD_IN,
            SWIPTERMD_STREAM_CMD_OUT,
            SWIPTERMD_STREAM_CMD_ERR,
        };
        int vsock_fds[3] = {-1, -1, -1};
        bool vsock_ok = true;
        for (int i = 0; i < 3; i++) {
            vsock_fds[i] = vsock_connect_stream(roles[i], inv_id, token);
            if (vsock_fds[i] < 0) {
                vsock_ok = false;
                break;
            }
        }
        if (!vsock_ok) {
            for (int i = 0; i < 3; i++) {
                if (vsock_fds[i] >= 0)
                    close(vsock_fds[i]);
                close(guest_fds[i]);
            }
            free(payload);
            close(cfd);
            continue;
        }

        cmd_invocation_t *inv = calloc(1, sizeof(*inv));
        if (!inv) {
            for (int i = 0; i < 3; i++) {
                close(vsock_fds[i]);
                close(guest_fds[i]);
            }
            free(payload);
            close(cfd);
            continue;
        }

        inv->invocation_id = inv_id;
        inv->start_req_id = ctrl_alloc_req_id();
        inv->stub_fd = cfd;
        (void)set_nonblock(cfd);
        ev_add(&inv->stub_ev, cfd, EPOLLIN, on_stub_event, inv);

        for (int i = 0; i < 3; i++) {
            cmd_chan_t *ch = &inv->chans[i];
            ch->guest_fd = guest_fds[i];
            ch->vsock_fd = vsock_fds[i];
            ch->guest_is_source = (i == 0);
            ch->inv = inv;

            struct stat st;
            bool is_reg = (fstat(guest_fds[i], &st) == 0 && S_ISREG(st.st_mode));
            if (!is_reg && ev_add(&ch->guest_ev, guest_fds[i],
                                  ch->guest_is_source ? EPOLLIN : 0,
                                  on_cmd_chan_event, ch) == 0) {
                ch->guest_epoll = true;
            } else {
                ch->guest_epoll = false;
            }

            ev_add(&ch->vsock_ev, vsock_fds[i], ch->guest_is_source ? 0 : EPOLLIN,
                   on_cmd_chan_event, ch);
        }

        inv->next = g_invocations;
        g_invocations = inv;

        uint8_t *frame = malloc(12 + plen);
        if (frame) {
            swiptermd_put_be32(frame + 0, inv_id);
            swiptermd_put_be64(frame + 4, token);
            memcpy(frame + 12, payload, plen);
            ctrl_send_frame(SWIPTERMD_OP_CMD_START, inv->start_req_id, frame, 12 + plen);
            free(frame);
        }
        free(payload);

        if (!inv->chans[0].guest_epoll)
            cmd_chan_pump(&inv->chans[0]);
    }
}

static int copy_cmd_stub(const char *dst_path) {
    int sfd = open("/cmd_stub", O_RDONLY | O_CLOEXEC);
    if (sfd < 0)
        return -errno;
    int dfd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0755);
    if (dfd < 0) {
        int err = -errno;
        close(sfd);
        return err;
    }
    uint8_t buf[16384];
    int err = 0;
    for (;;) {
        ssize_t n = read(sfd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            err = -errno;
            break;
        }
        if (n == 0)
            break;
        err = write_all_blocking(dfd, buf, (size_t)n);
        if (err < 0)
            break;
    }
    if (err == 0)
        (void)fchmod(dfd, 0755);
    close(dfd);
    close(sfd);
    return err;
}

int host_cmd_setup(const char *guest_dir) {
    if (!guest_dir || guest_dir[0] != '/')
        return -EINVAL;
    int err = fs_mkdir_p_jailed(guest_dir, 0755);
    if (err < 0)
        return err;

    char stub_path[PATH_MAX];
    char sock_path[PATH_MAX];
    if (snprintf(stub_path, sizeof(stub_path), "%s%s/.stub", GUEST_ROOT, guest_dir) >=
            (int)sizeof(stub_path) ||
        snprintf(sock_path, sizeof(sock_path), "%s%s/.sock", GUEST_ROOT, guest_dir) >=
            (int)sizeof(sock_path))
        return -ENAMETOOLONG;

    err = copy_cmd_stub(stub_path);
    if (err < 0)
        return err;

    if (g_cmd_listen_fd >= 0) {
        ev_del(&g_cmd_listen_ev);
        close(g_cmd_listen_fd);
        g_cmd_listen_fd = -1;
    }

    int sfd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sfd < 0)
        return -errno;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(sock_path) >= sizeof(addr.sun_path)) {
        close(sfd);
        return -ENAMETOOLONG;
    }
    strcpy(addr.sun_path, sock_path);
    unlink(sock_path);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(sfd, 64) < 0) {
        err = -errno;
        close(sfd);
        return err;
    }
    chmod(sock_path, 0666);

    g_cmd_listen_fd = sfd;
    ev_add(&g_cmd_listen_ev, sfd, EPOLLIN, on_cmd_accept, NULL);
    return 0;
}

void host_cmd_handle_register(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    char name[NAME_MAX + 1];
    if (!swiptermd_r_str_copy(&r, name, sizeof(name))) {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, -EINVAL);
        return;
    }
    if (name[0] == '\0' || strchr(name, '/') != NULL || strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0 || strcmp(name, ".stub") == 0 || strcmp(name, ".sock") == 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, -EINVAL);
        return;
    }

    for (size_t i = 0; i < g_cmd_count; i++) {
        if (strcmp(g_cmd_names[i], name) == 0) {
            ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, -EEXIST);
            return;
        }
    }

    char link_path[PATH_MAX];
    if (snprintf(link_path, sizeof(link_path), "%s%s/%s",
                 GUEST_ROOT, g_daemon.host_cmd_dir, name) >= (int)sizeof(link_path)) {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, -ENAMETOOLONG);
        return;
    }

    if (symlink(".stub", link_path) < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, -errno);
        return;
    }

    char **grown = realloc(g_cmd_names, (g_cmd_count + 1) * sizeof(*g_cmd_names));
    if (grown) {
        g_cmd_names = grown;
        g_cmd_names[g_cmd_count++] = strdup(name);
    }

    ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_CMD, req_id, 0);
}

void host_cmd_handle_start_reply(uint32_t req_id, const uint8_t *body, uint32_t len) {
    int32_t err = 0;
    if (!swiptermd_dec_err(body, len, &err) || err == 0)
        return;
    for (cmd_invocation_t *inv = g_invocations; inv; inv = inv->next) {
        if (inv->start_req_id == req_id) {
            inv->finish_received = true;
            inv->exit_status = 127;
            struct stub_ctl_msg msg = {.type = STUB_MSG_EXIT, .value = 127};
            if (inv->stub_fd >= 0)
                (void)write_all_blocking(inv->stub_fd, &msg, sizeof(msg));
            cmd_invocation_destroy(inv);
            return;
        }
    }
}

void host_cmd_handle_finish(const uint8_t *body, uint32_t len) {
    uint32_t inv_id;
    int32_t status;
    if (!swiptermd_dec_cmd_finish(body, len, &inv_id, &status))
        return;
    for (cmd_invocation_t *inv = g_invocations; inv; inv = inv->next) {
        if (inv->invocation_id == inv_id) {
            inv->finish_received = true;
            inv->exit_status = status;
            cmd_chan_close(&inv->chans[0]);
            cmd_check_done(inv);
            return;
        }
    }
}

static void cuse_write_reply(int cuse_fd, uint64_t unique, int32_t error,
                             const void *payload, size_t payload_len) {
    struct fuse_out_header hdr = {
        .len = (uint32_t)(sizeof(hdr) + payload_len),
        .error = error,
        .unique = unique,
    };
    struct iovec iov[2] = {
        {.iov_base = &hdr, .iov_len = sizeof(hdr)},
        {.iov_base = (void *)payload, .iov_len = payload_len},
    };
    int iovcnt = payload_len > 0 ? 2 : 1;
    ssize_t n;
    do {
        n = writev(cuse_fd, iov, iovcnt);
    } while (n < 0 && errno == EINTR);
}

static void cuse_queue_pending(cuse_device_t *dev, uint32_t req_id, uint16_t op, uint64_t unique) {
    cuse_pending_t *p = calloc(1, sizeof(*p));
    if (!p) {
        cuse_write_reply(dev->cuse_fd, unique, -ENOMEM, NULL, 0);
        return;
    }
    p->req_id = req_id;
    p->op = op;
    p->fuse_unique = unique;
    p->dev = dev;
    p->next = g_cuse_pending;
    g_cuse_pending = p;
}

static void on_cuse_event(ev_node_t *node, uint32_t events) {
    if (!(events & EPOLLIN))
        return;
    cuse_device_t *dev = node->ctx;

    static uint8_t buf[CUSE_IO_MAX + 4096];
    for (;;) {
        ssize_t n = read(dev->cuse_fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if ((size_t)n < sizeof(struct fuse_in_header))
            return;

        struct fuse_in_header *in = (struct fuse_in_header *)buf;
        const uint8_t *arg = buf + sizeof(*in);
        size_t arg_len = (size_t)n - sizeof(*in);

        switch (in->opcode) {
        case FUSE_OPEN: {
            if (arg_len < sizeof(struct fuse_open_in)) {
                cuse_write_reply(dev->cuse_fd, in->unique, -EINVAL, NULL, 0);
                break;
            }
            const struct fuse_open_in *oi = (const struct fuse_open_in *)arg;
            uint32_t rid = ctrl_alloc_req_id();
            cuse_queue_pending(dev, rid, SWIPTERMD_OP_DEV_OPEN, in->unique);
            uint8_t wire[SWIPTERMD_DEV_OPEN_REQ_SIZE];
            swiptermd_enc_dev_open_req(wire, dev->device_id, oi->flags);
            ctrl_send_frame(SWIPTERMD_OP_DEV_OPEN, rid, wire, sizeof(wire));
            break;
        }
        case FUSE_READ: {
            if (arg_len < sizeof(struct fuse_read_in)) {
                cuse_write_reply(dev->cuse_fd, in->unique, -EINVAL, NULL, 0);
                break;
            }
            const struct fuse_read_in *ri = (const struct fuse_read_in *)arg;
            uint32_t rid = ctrl_alloc_req_id();
            cuse_queue_pending(dev, rid, SWIPTERMD_OP_DEV_READ, in->unique);
            uint8_t wire[SWIPTERMD_DEV_READ_REQ_SIZE];
            uint32_t req_size = ri->size > CUSE_IO_MAX ? CUSE_IO_MAX : ri->size;
            swiptermd_enc_dev_read_req(wire, dev->device_id, ri->fh, req_size);
            ctrl_send_frame(SWIPTERMD_OP_DEV_READ, rid, wire, sizeof(wire));
            break;
        }
        case FUSE_WRITE: {
            if (arg_len < sizeof(struct fuse_write_in)) {
                cuse_write_reply(dev->cuse_fd, in->unique, -EINVAL, NULL, 0);
                break;
            }
            const struct fuse_write_in *wi = (const struct fuse_write_in *)arg;
            if (arg_len < sizeof(*wi) + wi->size) {
                cuse_write_reply(dev->cuse_fd, in->unique, -EINVAL, NULL, 0);
                break;
            }
            uint32_t rid = ctrl_alloc_req_id();
            size_t wlen = 16 + (size_t)wi->size;
            uint8_t *wire = malloc(wlen);
            if (!wire) {
                cuse_write_reply(dev->cuse_fd, in->unique, -ENOMEM, NULL, 0);
                break;
            }
            cuse_queue_pending(dev, rid, SWIPTERMD_OP_DEV_WRITE, in->unique);
            swiptermd_buf_w_t w = swiptermd_buf_w_init(wire, wlen);
            swiptermd_enc_dev_write_req(&w, dev->device_id, wi->fh,
                                        arg + sizeof(*wi), wi->size);
            ctrl_send_frame(SWIPTERMD_OP_DEV_WRITE, rid, wire, (uint32_t)w.len);
            free(wire);
            break;
        }
        case FUSE_RELEASE: {
            if (arg_len < sizeof(struct fuse_release_in)) {
                cuse_write_reply(dev->cuse_fd, in->unique, -EINVAL, NULL, 0);
                break;
            }
            const struct fuse_release_in *rli = (const struct fuse_release_in *)arg;
            uint32_t rid = ctrl_alloc_req_id();
            cuse_queue_pending(dev, rid, SWIPTERMD_OP_DEV_CLOSE, in->unique);
            uint8_t wire[SWIPTERMD_DEV_CLOSE_REQ_SIZE];
            swiptermd_enc_dev_close_req(wire, dev->device_id, rli->fh);
            ctrl_send_frame(SWIPTERMD_OP_DEV_CLOSE, rid, wire, sizeof(wire));
            break;
        }
        case FUSE_FLUSH:
        case FUSE_FSYNC:
            cuse_write_reply(dev->cuse_fd, in->unique, 0, NULL, 0);
            break;
        default:
            cuse_write_reply(dev->cuse_fd, in->unique, -ENOSYS, NULL, 0);
            break;
        }
    }
}

bool cuse_handle_reply(uint16_t opcode, uint32_t req_id, const uint8_t *body, uint32_t len) {
    cuse_pending_t **pp = &g_cuse_pending;
    cuse_pending_t *found = NULL;
    while (*pp) {
        if ((*pp)->req_id == req_id && (*pp)->op == opcode) {
            found = *pp;
            *pp = found->next;
            break;
        }
        pp = &(*pp)->next;
    }
    if (!found)
        return false;

    int cuse_fd = found->dev->cuse_fd;
    uint64_t unique = found->fuse_unique;
    free(found);

    switch (opcode) {
    case SWIPTERMD_OP_DEV_OPEN: {
        int32_t err;
        uint64_t fh;
        if (!swiptermd_dec_dev_open_reply(body, len, &err, &fh)) {
            cuse_write_reply(cuse_fd, unique, -EIO, NULL, 0);
            return true;
        }
        if (err < 0) {
            cuse_write_reply(cuse_fd, unique, err, NULL, 0);
            return true;
        }
        struct fuse_open_out oo;
        memset(&oo, 0, sizeof(oo));
        oo.fh = fh;
        oo.open_flags = FOPEN_DIRECT_IO | FOPEN_NONSEEKABLE;
        cuse_write_reply(cuse_fd, unique, 0, &oo, sizeof(oo));
        return true;
    }
    case SWIPTERMD_OP_DEV_READ: {
        int32_t err;
        swiptermd_slice_t data;
        if (!swiptermd_dec_dev_read_reply(body, len, &err, &data)) {
            cuse_write_reply(cuse_fd, unique, -EIO, NULL, 0);
            return true;
        }
        if (err < 0) {
            cuse_write_reply(cuse_fd, unique, err, NULL, 0);
            return true;
        }
        cuse_write_reply(cuse_fd, unique, 0, data.ptr, data.len);
        return true;
    }
    case SWIPTERMD_OP_DEV_WRITE: {
        int32_t err;
        uint32_t written;
        if (!swiptermd_dec_dev_write_reply(body, len, &err, &written)) {
            cuse_write_reply(cuse_fd, unique, -EIO, NULL, 0);
            return true;
        }
        if (err < 0) {
            cuse_write_reply(cuse_fd, unique, err, NULL, 0);
            return true;
        }
        struct fuse_write_out wo = {.size = written, .padding = 0};
        cuse_write_reply(cuse_fd, unique, 0, &wo, sizeof(wo));
        return true;
    }
    case SWIPTERMD_OP_DEV_CLOSE: {
        int32_t err = 0;
        (void)swiptermd_dec_err(body, len, &err);
        cuse_write_reply(cuse_fd, unique, err < 0 ? err : 0, NULL, 0);
        return true;
    }
    default:
        return false;
    }
}

bool cuse_available(void) {
    int fd = open("/dev/cuse", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return false;
    close(fd);
    return true;
}

void cuse_handle_register_device(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    uint32_t device_id = swiptermd_r_u32(&r);
    char path[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path)) || path[0] != '/') {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, -EINVAL);
        return;
    }

    char devname[64];
    snprintf(devname, sizeof(devname), "swipterm-%u", device_id);
    int cuse_fd = open("/dev/cuse", O_RDWR | O_CLOEXEC);
    if (cuse_fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, errno == ENOENT ? -ENODEV : -errno);
        return;
    }

    uint8_t init_req[16384];
    ssize_t n = read(cuse_fd, init_req, sizeof(init_req));
    if (n < (ssize_t)(sizeof(struct fuse_in_header) + sizeof(struct cuse_init_in))) {
        int err = n < 0 ? -errno : -EIO;
        close(cuse_fd);
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, err);
        return;
    }

    struct fuse_in_header *in = (struct fuse_in_header *)init_req;
    struct cuse_init_in *cii = (struct cuse_init_in *)(init_req + sizeof(*in));
    if (in->opcode != CUSE_INIT) {
        close(cuse_fd);
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, -EIO);
        return;
    }

    char info_str[300];
    int info_len = snprintf(info_str, sizeof(info_str), "DEVNAME=%s", devname) + 1;

    struct cuse_init_out cio;
    memset(&cio, 0, sizeof(cio));
    cio.major = FUSE_KERNEL_VERSION;
    cio.minor = cii->minor < FUSE_KERNEL_MINOR_VERSION ? cii->minor : FUSE_KERNEL_MINOR_VERSION;
    cio.flags = cii->flags & CUSE_UNRESTRICTED_IOCTL;
    cio.max_read = CUSE_IO_MAX;
    cio.max_write = CUSE_IO_MAX;

    struct fuse_out_header oh = {
        .len = (uint32_t)(sizeof(oh) + sizeof(cio) + (size_t)info_len),
        .error = 0,
        .unique = in->unique,
    };
    struct iovec iov[3] = {
        {.iov_base = &oh, .iov_len = sizeof(oh)},
        {.iov_base = &cio, .iov_len = sizeof(cio)},
        {.iov_base = info_str, .iov_len = (size_t)info_len},
    };
    if (writev(cuse_fd, iov, 3) < 0) {
        int err = -errno;
        close(cuse_fd);
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, err);
        return;
    }

    cuse_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        close(cuse_fd);
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, -ENOMEM);
        return;
    }
    dev->cuse_fd = cuse_fd;
    dev->parent_fd = -1;
    char devtmp_path[PATH_MAX];
    snprintf(devtmp_path, sizeof(devtmp_path), "/dev/%s", devname);
    struct stat st;
    int err = stat(devtmp_path, &st) < 0 ? -errno : 0;
    if (err == 0 && !S_ISCHR(st.st_mode))
        err = -ENODEV;
    if (err == 0) {
        dev->parent_fd = fs_open_parent(path, dev->leaf, sizeof(dev->leaf));
        if (dev->parent_fd < 0)
            err = dev->parent_fd;
    }
    bool permitted = false;
    bool created = false;
    if (err == 0) {
        err = device_permit(major(st.st_rdev), minor(st.st_rdev));
        permitted = err == 0;
    }
    if (err == 0) {
        struct stat stale;
        if (fstatat(dev->parent_fd, dev->leaf, &stale, AT_SYMLINK_NOFOLLOW) == 0 && S_ISCHR(stale.st_mode))
            (void)unlinkat(dev->parent_fd, dev->leaf, 0);
        err = mknodat(dev->parent_fd, dev->leaf, S_IFCHR | 0666, st.st_rdev) < 0 ? -errno : 0;
        created = err == 0;
    }
    if (err == 0)
        err = set_nonblock(cuse_fd);
    if (err == 0) {
        dev->device_id = device_id;
        dev->rdev = st.st_rdev;
        err = ev_add(&dev->ev, cuse_fd, EPOLLIN, on_cuse_event, dev);
    }
    if (err < 0) {
        if (created)
            (void)unlinkat(dev->parent_fd, dev->leaf, 0);
        if (permitted)
            device_revoke(major(st.st_rdev), minor(st.st_rdev));
        if (dev->parent_fd >= 0)
            close(dev->parent_fd);
        close(cuse_fd);
        free(dev);
        ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, err);
        return;
    }
    dev->next = g_cuse_devs;
    g_cuse_devs = dev;
    ctrl_send_err_reply(SWIPTERMD_OP_REGISTER_DEVICE, req_id, 0);
}

void host_cmd_cuse_shutdown(void) {
    ev_del(&g_cmd_listen_ev);
    if (g_cmd_listen_fd >= 0) {
        close(g_cmd_listen_fd);
        g_cmd_listen_fd = -1;
    }
    for (cmd_invocation_t *inv = g_invocations; inv; inv = inv->next) {
        ev_del(&inv->stub_ev);
        if (inv->stub_fd >= 0) {
            close(inv->stub_fd);
            inv->stub_fd = -1;
        }
        for (int i = 0; i < 3; i++)
            cmd_chan_close(&inv->chans[i]);
    }
    for (cuse_device_t *dev = g_cuse_devs; dev; dev = dev->next) {
        ev_del(&dev->ev);
        if (dev->parent_fd >= 0) {
            struct stat current;
            if (fstatat(dev->parent_fd, dev->leaf, &current, AT_SYMLINK_NOFOLLOW) == 0 &&
                S_ISCHR(current.st_mode) && current.st_rdev == dev->rdev)
                (void)unlinkat(dev->parent_fd, dev->leaf, 0);
            close(dev->parent_fd);
            dev->parent_fd = -1;
            device_revoke(major(dev->rdev), minor(dev->rdev));
        }
        if (dev->cuse_fd >= 0) {
            close(dev->cuse_fd);
            dev->cuse_fd = -1;
        }
    }
}
