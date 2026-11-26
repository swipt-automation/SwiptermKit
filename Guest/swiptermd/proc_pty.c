#include "swiptermd.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define PTY_OUT_BUF_CAP (32 * 1024)
#define PTY_IN_BUF_CAP 16384
#define PIPE_RELAY_BUF_CAP (128 * 1024)
#define STREAM_BATCH (128 * 1024)
#define STREAM_PIPE_CAP (256 * 1024)

typedef struct pty_session {
    uint32_t pty_id;
    int pts_num;
    int master_fd;
    int vsock_fd;
    int guard_fd;
    ev_node_t master_ev;
    ev_node_t vsock_ev;
    bool in_use;
    bool closed;
    bool master_eof;
    bool vsock_eof;

    uint8_t out_buf[PTY_OUT_BUF_CAP];
    size_t out_off;
    size_t out_len;

    uint8_t in_buf[PTY_IN_BUF_CAP];
    size_t in_off;
    size_t in_len;

    struct pty_session *next;
} pty_session_t;

typedef struct pipe_relay {
    int src_fd;
    int dst_fd;
    bool dst_is_socket;
    bool src_eof;
    ev_node_t src_ev;
    ev_node_t dst_ev;
    uint8_t buf[PIPE_RELAY_BUF_CAP];
    size_t off;
    size_t len;
} pipe_relay_t;

typedef struct proc_entry {
    uint32_t process_id;
    pid_t outer_pid;
    pid_t container_pid;
    int exit_fd;
    ev_node_t exit_ev;
    bool exited;
    uint32_t bound_pty_ids[3];
    int bound_pty_count;
    void (*exit_handler)(int status);
    struct proc_entry *next;
} proc_entry_t;

typedef struct spawn_sync_msg {
    int32_t err;
    int32_t outer_pid;
    int32_t container_pid;
} spawn_sync_msg_t;

static pty_session_t *g_ptys;
static proc_entry_t *g_procs;

void proc_pty_init(void) {
    g_ptys = NULL;
    g_procs = NULL;
}

static void pty_release_guard(pty_session_t *p) {
    if (p->guard_fd >= 0) {
        close(p->guard_fd);
        p->guard_fd = -1;
    }
}

void proc_pty_shutdown(void) {
    for (pty_session_t *p = g_ptys; p; p = p->next) {
        pty_release_guard(p);
        ev_del(&p->master_ev);
        ev_del(&p->vsock_ev);
        if (p->master_fd >= 0) {
            close(p->master_fd);
            p->master_fd = -1;
        }
        if (p->vsock_fd >= 0) {
            close(p->vsock_fd);
            p->vsock_fd = -1;
        }
    }
    for (proc_entry_t *proc = g_procs; proc; proc = proc->next) {
        ev_del(&proc->exit_ev);
        if (proc->exit_fd >= 0) {
            close(proc->exit_fd);
            proc->exit_fd = -1;
        }
    }
}

static pty_session_t *pty_find(uint32_t pty_id) {
    for (pty_session_t *p = g_ptys; p; p = p->next) {
        if (p->pty_id == pty_id)
            return p;
    }
    return NULL;
}

static void pty_update_events(pty_session_t *p) {
    if (p->master_fd >= 0) {
        uint32_t mev = 0;
        if (p->out_len == 0 && p->vsock_fd >= 0)
            mev |= EPOLLIN;
        if (p->in_off < p->in_len)
            mev |= EPOLLOUT;
        ev_mod(&p->master_ev, mev);
    }
    if (p->vsock_fd >= 0) {
        uint32_t vev = 0;
        if (!p->vsock_eof && p->in_len == 0 && p->master_fd >= 0)
            vev |= EPOLLIN;
        if (p->out_off < p->out_len)
            vev |= EPOLLOUT;
        ev_mod(&p->vsock_ev, vev);
    }
}

static void pty_close_streams(pty_session_t *p) {
    pty_release_guard(p);
    ev_del(&p->master_ev);
    ev_del(&p->vsock_ev);
    if (p->master_fd >= 0) {
        close(p->master_fd);
        p->master_fd = -1;
    }
    if (p->vsock_fd >= 0) {
        close(p->vsock_fd);
        p->vsock_fd = -1;
    }
    p->out_off = p->out_len = 0;
    p->in_off = p->in_len = 0;
}

static void pty_pump(pty_session_t *p) {
    if (p->master_fd >= 0 && p->in_off < p->in_len) {
        while (p->in_off < p->in_len) {
            ssize_t n = write(p->master_fd, p->in_buf + p->in_off, p->in_len - p->in_off);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                p->in_off = p->in_len = 0;
                break;
            }
            p->in_off += (size_t)n;
        }
        if (p->in_off == p->in_len)
            p->in_off = p->in_len = 0;
    }

    if (p->vsock_fd >= 0 && p->master_fd >= 0 && !p->vsock_eof && p->in_len == 0) {
        for (size_t bytes = 0; bytes < STREAM_BATCH;) {
            ssize_t n = recv(p->vsock_fd, p->in_buf, sizeof(p->in_buf), 0);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                p->vsock_eof = true;
                break;
            }
            if (n == 0) {
                p->vsock_eof = true;
                break;
            }
            p->in_off = 0;
            p->in_len = (size_t)n;
            bytes += (size_t)n;
            while (p->in_off < p->in_len) {
                ssize_t w = write(p->master_fd, p->in_buf + p->in_off, p->in_len - p->in_off);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    p->in_off = p->in_len = 0;
                    break;
                }
                p->in_off += (size_t)w;
            }
            if (p->in_off < p->in_len)
                break;
            p->in_off = p->in_len = 0;
        }
    }

    for (size_t sent = 0; p->vsock_fd >= 0 && sent < STREAM_BATCH;) {
        if (p->out_len == 0 && p->master_fd >= 0) {
            while (p->out_len < sizeof(p->out_buf)) {
                ssize_t n = read(p->master_fd, p->out_buf + p->out_len,
                                 sizeof(p->out_buf) - p->out_len);
                if (n > 0) {
                    p->out_len += (size_t)n;
                    continue;
                }
                if (n < 0 && errno == EINTR)
                    continue;
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    break;
                p->master_eof = true;
                ev_del(&p->master_ev);
                close(p->master_fd);
                p->master_fd = -1;
                break;
            }
        }

        if (p->out_len == 0)
            break;
        ssize_t n = send(p->vsock_fd, p->out_buf + p->out_off,
                         p->out_len - p->out_off, MSG_NOSIGNAL);
        if (n > 0) {
            p->out_off += (size_t)n;
            sent += (size_t)n;
            if (p->out_off == p->out_len)
                p->out_off = p->out_len = 0;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        ev_del(&p->vsock_ev);
        close(p->vsock_fd);
        p->vsock_fd = -1;
        p->out_off = p->out_len = 0;
    }

    if (p->master_eof && p->out_len == 0 && p->vsock_fd >= 0) {
        ev_del(&p->vsock_ev);
        close(p->vsock_fd);
        p->vsock_fd = -1;
    }

    pty_update_events(p);
}

static void on_pty_event(ev_node_t *node, uint32_t events) {
    (void)events;
    pty_session_t *p = node->ctx;
    pty_pump(p);
}

void pty_handle_create(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t pty_id;
    uint16_t cols, rows;
    uint64_t token;
    if (!swiptermd_dec_pty_create_req(body, len, &pty_id, &cols, &rows, &token)) {
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, -EINVAL, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    int mfd = open(GUEST_ROOT "/dev/pts/ptmx", O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (mfd < 0) {
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, -errno, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    int unlock = 0;
    int pts_num = -1;
    if (ioctl(mfd, TIOCSPTLCK, &unlock) < 0 || ioctl(mfd, TIOCGPTN, &pts_num) < 0) {
        int err = -errno;
        close(mfd);
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, err, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    struct winsize ws = {.ws_row = rows ? rows : 24, .ws_col = cols ? cols : 80};
    (void)ioctl(mfd, TIOCSWINSZ, &ws);

    int guard = ioctl(mfd, TIOCGPTPEER, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (guard < 0) {
        int err = -errno;
        close(mfd);
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, err, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    int vfd = vsock_connect_stream(SWIPTERMD_STREAM_PTY, pty_id, token);
    if (vfd < 0) {
        close(guard);
        close(mfd);
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, vfd, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    pty_session_t *p = calloc(1, sizeof(*p));
    if (!p) {
        close(vfd);
        close(guard);
        close(mfd);
        uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
        swiptermd_enc_pty_create_reply(out, -ENOMEM, 0);
        ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                        req_id, out, sizeof(out));
        return;
    }

    p->pty_id = pty_id;
    p->pts_num = pts_num;
    p->master_fd = mfd;
    p->vsock_fd = vfd;
    p->guard_fd = guard;
    p->next = g_ptys;
    g_ptys = p;

    ev_add(&p->master_ev, mfd, 0, on_pty_event, p);
    ev_add(&p->vsock_ev, vfd, EPOLLIN, on_pty_event, p);

    uint8_t out[SWIPTERMD_PTY_CREATE_REPLY_SIZE];
    swiptermd_enc_pty_create_reply(out, 0, (uint32_t)pts_num);
    ctrl_send_frame((uint16_t)(SWIPTERMD_OP_PTY_CREATE | SWIPTERMD_OP_REPLY_FLAG),
                    req_id, out, sizeof(out));
}

void pty_handle_resize(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t pty_id;
    uint16_t cols, rows;
    if (!swiptermd_dec_pty_resize(body, len, &pty_id, &cols, &rows)) {
        ctrl_send_err_reply(SWIPTERMD_OP_PTY_RESIZE, req_id, -EINVAL);
        return;
    }
    pty_session_t *p = pty_find(pty_id);
    if (!p || p->closed || p->master_fd < 0) {
        ctrl_send_err_reply(SWIPTERMD_OP_PTY_RESIZE, req_id, -EIO);
        return;
    }
    struct winsize ws = {.ws_row = rows, .ws_col = cols};
    int err = ioctl(p->master_fd, TIOCSWINSZ, &ws) < 0 ? -errno : 0;
    ctrl_send_err_reply(SWIPTERMD_OP_PTY_RESIZE, req_id, err);
}

void pty_handle_close(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t pty_id;
    if (!swiptermd_dec_pty_close(body, len, &pty_id)) {
        ctrl_send_err_reply(SWIPTERMD_OP_PTY_CLOSE, req_id, -EINVAL);
        return;
    }
    pty_session_t *p = pty_find(pty_id);
    if (p) {
        p->closed = true;
        pty_close_streams(p);
    }
    ctrl_send_err_reply(SWIPTERMD_OP_PTY_CLOSE, req_id, 0);
}

static void pipe_relay_destroy(pipe_relay_t *r) {
    if (!r)
        return;
    ev_del(&r->src_ev);
    ev_del(&r->dst_ev);
    if (r->src_fd >= 0)
        close(r->src_fd);
    if (r->dst_fd >= 0)
        close(r->dst_fd);
    free(r);
}

static void on_pipe_relay_event(ev_node_t *node, uint32_t events) {
    (void)events;
    pipe_relay_t *r = node->ctx;

    for (size_t sent = 0; sent < STREAM_BATCH;) {
        while (r->off < r->len) {
            ssize_t w = r->dst_is_socket
                ? send(r->dst_fd, r->buf + r->off, r->len - r->off, MSG_NOSIGNAL)
                : write(r->dst_fd, r->buf + r->off, r->len - r->off);
            if (w <= 0) {
                if (w < 0 && errno == EINTR)
                    continue;
                if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    ev_mod(&r->src_ev, 0);
                    ev_mod(&r->dst_ev, EPOLLOUT);
                    return;
                }
                pipe_relay_destroy(r);
                return;
            }
            r->off += (size_t)w;
            sent += (size_t)w;
        }
        r->off = r->len = 0;

        if (r->src_eof) {
            pipe_relay_destroy(r);
            return;
        }

        while (r->len < sizeof(r->buf)) {
            ssize_t n = read(r->src_fd, r->buf + r->len, sizeof(r->buf) - r->len);
            if (n > 0) {
                r->len += (size_t)n;
                if (!r->dst_is_socket)
                    break;
                continue;
            }
            if (n < 0 && errno == EINTR)
                continue;
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                r->src_eof = true;
            break;
        }
        if (r->len == 0) {
            if (r->src_eof) {
                pipe_relay_destroy(r);
                return;
            }
            break;
        }
    }

    if (r->src_eof && r->len == 0) {
        pipe_relay_destroy(r);
        return;
    }
    ev_mod(&r->src_ev, r->len == 0 ? EPOLLIN : 0);
    ev_mod(&r->dst_ev, r->len != 0 ? EPOLLOUT : 0);
}

static pipe_relay_t *pipe_relay_start(int src_fd, int dst_fd, bool dst_is_socket) {
    pipe_relay_t *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->src_fd = src_fd;
    r->dst_fd = dst_fd;
    r->dst_is_socket = dst_is_socket;
    ev_add(&r->src_ev, src_fd, EPOLLIN, on_pipe_relay_event, r);
    ev_add(&r->dst_ev, dst_fd, 0, on_pipe_relay_event, r);
    return r;
}

static void on_proc_exit_event(ev_node_t *node, uint32_t events) {
    (void)events;
    proc_entry_t *proc = node->ctx;
    int32_t status = 0;
    ssize_t n = read(proc->exit_fd, &status, sizeof(status));
    if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
        return;

    ev_del(&proc->exit_ev);
    close(proc->exit_fd);
    proc->exit_fd = -1;
    proc->exited = true;

    for (int i = 0; i < proc->bound_pty_count; i++) {
        pty_session_t *p = pty_find(proc->bound_pty_ids[i]);
        if (p) {
            p->in_use = false;
            pty_pump(p);
            pty_release_guard(p);
        }
    }

    proc_entry_t **pp = &g_procs;
    while (*pp) {
        if (*pp == proc) {
            *pp = proc->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (proc->exit_handler) {
        proc->exit_handler(status);
    } else {
        uint8_t msg[SWIPTERMD_PROCESS_EXIT_SIZE];
        swiptermd_enc_process_exit(msg, proc->process_id, proc->container_pid, status);
        ctrl_send_frame(SWIPTERMD_OP_PROCESS_EXIT, 0, msg, sizeof(msg));
    }
    free(proc);
}

static char **decode_strv_spawn(swiptermd_buf_r_t *r) {
    uint32_t count = swiptermd_r_u32(r);
    if (r->underflow || count > (r->len - r->pos) / sizeof(uint32_t))
        return NULL;
    char **vec = calloc((size_t)count + 1, sizeof(char *));
    if (!vec)
        return NULL;
    for (uint32_t i = 0; i < count; i++) {
        swiptermd_slice_t s = swiptermd_r_bytes(r);
        vec[i] = r->underflow ? NULL : malloc((size_t)s.len + 1);
        if (!vec[i]) {
            for (uint32_t j = 0; j < i; j++)
                free(vec[j]);
            free(vec);
            return NULL;
        }
        if (s.len > 0)
            memcpy(vec[i], s.ptr, s.len);
        vec[i][s.len] = '\0';
    }
    return vec;
}

static char **gpu_environment(char **envp) {
    if (!envp || !g_daemon.gpu)
        return envp;
    size_t count = 0;
    for (; envp[count]; count++) {
        if (strncmp(envp[count], "VN_DEBUG=", 9) == 0)
            return envp;
    }
    char *entry = strdup(GPU_VENUS_ENV);
    char **grown = entry ? realloc(envp, (count + 2) * sizeof(*envp)) : NULL;
    if (!grown) {
        free(entry);
        return envp;
    }
    grown[count] = entry;
    grown[count + 1] = NULL;
    return grown;
}

static void free_strv_spawn(char **vec) {
    if (!vec)
        return;
    for (size_t i = 0; vec[i]; i++)
        free(vec[i]);
    free(vec);
}

void close_other_fds(const int *keep, int nkeep) {
    DIR *d = opendir("/proc/self/fd");
    if (!d)
        return;
    int dfd = dirfd(d);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9')
            continue;
        int fd = atoi(de->d_name);
        if (fd < 3 || fd == dfd)
            continue;
        bool kept = false;
        for (int i = 0; i < nkeep; i++) {
            if (keep[i] == fd) {
                kept = true;
                break;
            }
        }
        if (!kept)
            close(fd);
    }
    closedir(d);
}

static void send_spawn_reply(uint16_t opcode, uint32_t req_id, int32_t err, int32_t pid) {
    uint8_t out[SWIPTERMD_SPAWN_REPLY_SIZE];
    swiptermd_enc_spawn_reply(out, err, pid);
    ctrl_send_frame((uint16_t)(opcode | SWIPTERMD_OP_REPLY_FLAG), req_id, out, sizeof(out));
}

typedef enum launch_payload {
    LAUNCH_EXEC,
    LAUNCH_LOGIN,
    LAUNCH_COMMAND,
} launch_payload_t;

typedef struct launch_spec {
    launch_payload_t payload;
    uint32_t process_id;
    uint8_t stdio_mode[3];
    uint32_t stdio_pty_id[3];
    uint64_t stdio_token[3];
    const char *path;
    const char *cwd;
    const char *user;
    char *const *argv;
    char *const *envp;
    void (*exit_handler)(int status);
} launch_spec_t;

static void child_report(int fd, int32_t err) {
    (void)write(fd, &err, sizeof(err));
    _exit(127);
}

static void launch_payload_exec(const launch_spec_t *spec, const int child_pipe_fds[3],
                                const int pts_nums[3], int primary_pts, int err_fd) {
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
    signal(SIGPIPE, SIG_DFL);

    const char *target_cwd = spec->cwd && spec->cwd[0] ? spec->cwd : "/";
    if (chdir(target_cwd) < 0)
        child_report(err_fd, -errno);

    int pty_fd = -1;
    if (primary_pts >= 0) {
        (void)setsid();
        char pts_path[64];
        snprintf(pts_path, sizeof(pts_path), "/dev/pts/%d", primary_pts);
        pty_fd = open(pts_path, O_RDWR);
        if (pty_fd < 0 || ioctl(pty_fd, TIOCSCTTY, 0) < 0)
            child_report(err_fd, -errno);
    } else {
        (void)setpgid(0, 0);
    }

    int null_fd = -1;
    for (int i = 0; i < 3; i++) {
        if (spec->stdio_mode[i] == SWIPTERMD_STDIO_PIPE) {
            int fl = fcntl(child_pipe_fds[i], F_GETFL, 0);
            if (fl >= 0)
                fcntl(child_pipe_fds[i], F_SETFL, fl & ~O_NONBLOCK);
            if (child_pipe_fds[i] != i)
                dup2(child_pipe_fds[i], i);
        } else if (spec->stdio_mode[i] == SWIPTERMD_STDIO_PTY) {
            int sfd = pty_fd;
            if (pts_nums[i] != primary_pts) {
                char pts_path[64];
                snprintf(pts_path, sizeof(pts_path), "/dev/pts/%d", pts_nums[i]);
                sfd = open(pts_path, O_RDWR | O_NOCTTY);
            }
            if (sfd < 0)
                child_report(err_fd, -errno);
            if (sfd != i)
                dup2(sfd, i);
            if (sfd != pty_fd && sfd > 2)
                close(sfd);
        } else {
            if (null_fd < 0)
                null_fd = open("/dev/null", O_RDWR);
            if (null_fd >= 0 && null_fd != i)
                dup2(null_fd, i);
        }
    }
    if (pty_fd > 2)
        close(pty_fd);
    if (null_fd > 2)
        close(null_fd);
    for (int i = 0; i < 3; i++) {
        if (child_pipe_fds[i] > 2)
            close(child_pipe_fds[i]);
    }

    umask(0022);
    switch (spec->payload) {
    case LAUNCH_EXEC:
        execve(spec->path, spec->argv, spec->envp);
        child_report(err_fd, -errno);
        break;
    case LAUNCH_LOGIN:
        child_report(err_fd, login_exec(spec->user, spec->envp, g_daemon.init_mode == SWIPTERMD_INIT_MANAGED));
        break;
    case LAUNCH_COMMAND: {
        static char path_env[] = "PATH=" TRUSTED_PATH;
        char *const env[] = {path_env, NULL};
        execvpe(spec->argv[0], spec->argv, env);
        child_report(err_fd, -errno);
        break;
    }
    }
}

static void launch_helper(const launch_spec_t *spec, int sync_fd, int exit_fd, int procs_fd,
                          const int child_pipe_fds[3], const int pts_nums[3], int primary_pts) {
    const int ns_fds[] = {
        network_guest_fd(),
        g_daemon.container_ipc_ns_fd,
        g_daemon.container_uts_ns_fd,
        g_daemon.container_mnt_ns_fd,
        g_daemon.container_pid_ns_fd,
    };
    static const int ns_types[] = {CLONE_NEWNET, CLONE_NEWIPC, CLONE_NEWUTS, CLONE_NEWNS, CLONE_NEWPID};
    int cgroup_ns_fd = g_daemon.container_cgroup_ns_fd;
    int keep[] = {
        sync_fd, exit_fd, procs_fd, cgroup_ns_fd,
        ns_fds[0], ns_fds[1], ns_fds[2], ns_fds[3], ns_fds[4],
        child_pipe_fds[0], child_pipe_fds[1], child_pipe_fds[2],
    };
    close_other_fds(keep, (int)(sizeof(keep) / sizeof(keep[0])));

    spawn_sync_msg_t msg = {0};
    for (size_t i = 0; i < sizeof(ns_fds) / sizeof(ns_fds[0]) && msg.err == 0; i++) {
        if (setns(ns_fds[i], ns_types[i]) < 0)
            msg.err = -errno;
    }
    int err_pipe[2] = {-1, -1};
    int pid_pipe[2] = {-1, -1};
    if (msg.err == 0 && (pipe2(err_pipe, O_CLOEXEC) < 0 || pipe2(pid_pipe, O_CLOEXEC) < 0))
        msg.err = -errno;
    pid_t child = msg.err == 0 ? fork() : -1;
    if (msg.err == 0 && child < 0)
        msg.err = -errno;
    if (msg.err < 0) {
        (void)write_all_blocking(sync_fd, &msg, sizeof(msg));
        _exit(0);
    }

    if (child == 0) {
        close(err_pipe[0]);
        close(pid_pipe[0]);
        close(sync_fd);
        close(exit_fd);
        if (write(procs_fd, "0", 1) != 1)
            child_report(err_pipe[1], -errno);
        close(procs_fd);
        if (setns(cgroup_ns_fd, CLONE_NEWCGROUP) < 0)
            child_report(err_pipe[1], -errno);
        close(cgroup_ns_fd);
        for (size_t i = 0; i < sizeof(ns_fds) / sizeof(ns_fds[0]); i++)
            close(ns_fds[i]);
        int32_t cpid = (int32_t)getpid();
        (void)write_all_blocking(pid_pipe[1], &cpid, sizeof(cpid));
        close(pid_pipe[1]);
        launch_payload_exec(spec, child_pipe_fds, pts_nums, primary_pts, err_pipe[1]);
        _exit(127);
    }

    close(err_pipe[1]);
    close(pid_pipe[1]);
    for (int i = 0; i < 3; i++) {
        if (child_pipe_fds[i] >= 0)
            close(child_pipe_fds[i]);
    }

    int32_t container_pid = 0;
    (void)read_all_blocking(pid_pipe[0], &container_pid, sizeof(container_pid));
    close(pid_pipe[0]);

    int32_t child_err = 0;
    ssize_t en = read(err_pipe[0], &child_err, sizeof(child_err));
    close(err_pipe[0]);

    if (en == (ssize_t)sizeof(child_err) && child_err < 0) {
        int st;
        (void)waitpid(child, &st, 0);
        msg.err = child_err;
        (void)write_all_blocking(sync_fd, &msg, sizeof(msg));
        _exit(0);
    }

    msg.outer_pid = (int32_t)child;
    msg.container_pid = container_pid;
    (void)write_all_blocking(sync_fd, &msg, sizeof(msg));
    close(sync_fd);

    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    int32_t wire_st = (int32_t)status;
    (void)write_all_blocking(exit_fd, &wire_st, sizeof(wire_st));
    _exit(0);
}

static int launch(const launch_spec_t *spec, int32_t *container_pid_out) {
    if (!g_daemon.init_started || g_daemon.halted || g_daemon.container_pid_ns_fd < 0)
        return -ESRCH;
    if (g_daemon.stopping && spec->payload != LAUNCH_COMMAND)
        return -ESHUTDOWN;

    pty_session_t *bound_ptys[3] = {NULL, NULL, NULL};
    int bound_count = 0;
    int pts_nums[3] = {-1, -1, -1};
    int primary_pts = -1;
    int err = 0;

    for (int i = 0; i < 3 && err == 0; i++) {
        if (spec->stdio_mode[i] != SWIPTERMD_STDIO_PTY)
            continue;
        pty_session_t *p = pty_find(spec->stdio_pty_id[i]);
        if (!p || p->closed || p->master_fd < 0) {
            err = -EIO;
            break;
        }
        bool already_ours = false;
        for (int j = 0; j < bound_count; j++) {
            if (bound_ptys[j] == p)
                already_ours = true;
        }
        if (!already_ours) {
            if (p->in_use) {
                err = -EBUSY;
                break;
            }
            p->in_use = true;
            bound_ptys[bound_count++] = p;
        }
        pts_nums[i] = p->pts_num;
        if (primary_pts < 0)
            primary_pts = p->pts_num;
    }

    int procs_fd = err == 0 ? sessions_procs_fd() : -1;
    if (err == 0 && procs_fd < 0)
        err = procs_fd;

    int child_pipe_fds[3] = {-1, -1, -1};
    int daemon_pipe_fds[3] = {-1, -1, -1};
    int vsock_fds[3] = {-1, -1, -1};
    static const uint32_t stdio_roles[3] = {
        SWIPTERMD_STREAM_STDIO_IN,
        SWIPTERMD_STREAM_STDIO_OUT,
        SWIPTERMD_STREAM_STDIO_ERR,
    };
    for (int i = 0; i < 3 && err == 0; i++) {
        if (spec->stdio_mode[i] != SWIPTERMD_STDIO_PIPE)
            continue;
        int pfd[2];
        if (pipe2(pfd, O_NONBLOCK | O_CLOEXEC) < 0) {
            err = -errno;
            break;
        }
        if (i != 0)
            (void)fcntl(pfd[0], F_SETPIPE_SZ, STREAM_PIPE_CAP);
        int vfd = vsock_connect_stream(stdio_roles[i], spec->process_id, spec->stdio_token[i]);
        if (vfd < 0) {
            close(pfd[0]);
            close(pfd[1]);
            err = vfd;
            break;
        }
        vsock_fds[i] = vfd;
        child_pipe_fds[i] = i == 0 ? pfd[0] : pfd[1];
        daemon_pipe_fds[i] = i == 0 ? pfd[1] : pfd[0];
    }

    int sync_pipe[2] = {-1, -1};
    int exit_pipe[2] = {-1, -1};
    if (err == 0 && (pipe2(sync_pipe, O_CLOEXEC) < 0 || pipe2(exit_pipe, O_CLOEXEC) < 0))
        err = -errno;

    pid_t helper_pid = -1;
    if (err == 0) {
        helper_pid = fork();
        if (helper_pid < 0)
            err = -errno;
        else if (helper_pid == 0)
            launch_helper(spec, sync_pipe[1], exit_pipe[1], procs_fd, child_pipe_fds, pts_nums, primary_pts);
    }

    if (procs_fd >= 0)
        close(procs_fd);
    if (sync_pipe[1] >= 0)
        close(sync_pipe[1]);
    if (exit_pipe[1] >= 0)
        close(exit_pipe[1]);
    for (int i = 0; i < 3; i++) {
        if (child_pipe_fds[i] >= 0)
            close(child_pipe_fds[i]);
    }

    spawn_sync_msg_t sync_msg = {.err = err};
    if (err == 0 && read_all_blocking(sync_pipe[0], &sync_msg, sizeof(sync_msg)) < 0)
        sync_msg.err = -EIO;
    if (sync_pipe[0] >= 0)
        close(sync_pipe[0]);

    if (sync_msg.err < 0) {
        for (int j = 0; j < bound_count; j++)
            bound_ptys[j]->in_use = false;
        for (int i = 0; i < 3; i++) {
            if (daemon_pipe_fds[i] >= 0)
                close(daemon_pipe_fds[i]);
            if (vsock_fds[i] >= 0)
                close(vsock_fds[i]);
        }
        if (exit_pipe[0] >= 0)
            close(exit_pipe[0]);
        return sync_msg.err;
    }

    for (int i = 0; i < 3; i++) {
        if (spec->stdio_mode[i] != SWIPTERMD_STDIO_PIPE)
            continue;
        if (i == 0)
            (void)pipe_relay_start(vsock_fds[0], daemon_pipe_fds[0], false);
        else
            (void)pipe_relay_start(daemon_pipe_fds[i], vsock_fds[i], true);
    }

    for (int j = 0; j < bound_count; j++)
        pty_update_events(bound_ptys[j]);

    proc_entry_t *proc = calloc(1, sizeof(*proc));
    if (proc) {
        proc->process_id = spec->process_id;
        proc->outer_pid = (pid_t)sync_msg.outer_pid;
        proc->container_pid = (pid_t)sync_msg.container_pid;
        proc->exit_fd = exit_pipe[0];
        proc->exit_handler = spec->exit_handler;
        proc->bound_pty_count = bound_count;
        for (int j = 0; j < bound_count; j++)
            proc->bound_pty_ids[j] = bound_ptys[j]->pty_id;
        (void)set_nonblock(exit_pipe[0]);
        proc->next = g_procs;
        g_procs = proc;
        ev_add(&proc->exit_ev, exit_pipe[0], EPOLLIN, on_proc_exit_event, proc);
    } else {
        close(exit_pipe[0]);
    }
    *container_pid_out = sync_msg.container_pid;
    return 0;
}

void proc_handle_spawn(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    launch_spec_t spec = {.payload = LAUNCH_EXEC};
    spec.process_id = swiptermd_r_u32(&r);
    for (int i = 0; i < 3; i++)
        spec.stdio_mode[i] = swiptermd_r_u8(&r);
    (void)swiptermd_r_u8(&r);
    for (int i = 0; i < 3; i++)
        spec.stdio_pty_id[i] = swiptermd_r_u32(&r);
    for (int i = 0; i < 3; i++)
        spec.stdio_token[i] = swiptermd_r_u64(&r);

    char path[PATH_MAX];
    char cwd[PATH_MAX];
    if (!swiptermd_r_str_copy(&r, path, sizeof(path)) || !swiptermd_r_str_copy(&r, cwd, sizeof(cwd))) {
        send_spawn_reply(SWIPTERMD_OP_SPAWN, req_id, -EINVAL, 0);
        return;
    }
    char **argv = decode_strv_spawn(&r);
    char **envp = argv ? gpu_environment(decode_strv_spawn(&r)) : NULL;
    int32_t pid = 0;
    int err = -EINVAL;
    if (argv && envp && spec.process_id != 0) {
        spec.path = path;
        spec.cwd = cwd;
        spec.argv = argv;
        spec.envp = envp;
        err = launch(&spec, &pid);
    }
    free_strv_spawn(argv);
    free_strv_spawn(envp);
    send_spawn_reply(SWIPTERMD_OP_SPAWN, req_id, err, err == 0 ? pid : 0);
}

void proc_handle_login(uint32_t req_id, const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    launch_spec_t spec = {.payload = LAUNCH_LOGIN};
    spec.process_id = swiptermd_r_u32(&r);
    uint32_t pty_id = swiptermd_r_u32(&r);
    for (int i = 0; i < 3; i++) {
        spec.stdio_mode[i] = SWIPTERMD_STDIO_PTY;
        spec.stdio_pty_id[i] = pty_id;
    }
    char user[LOGIN_USER_CAP];
    bool user_ok = swiptermd_r_str_copy(&r, user, sizeof(user)) && login_name_valid(user);
    char **envp = user_ok ? decode_strv_spawn(&r) : NULL;
    int32_t pid = 0;
    int err = -EINVAL;
    if (envp && spec.process_id != 0) {
        spec.user = user;
        spec.envp = envp;
        err = launch(&spec, &pid);
    }
    free_strv_spawn(envp);
    send_spawn_reply(SWIPTERMD_OP_LOGIN, req_id, err, err == 0 ? pid : 0);
}

int proc_run_in_container(char *const argv[], void (*exit_handler)(int status)) {
    launch_spec_t spec = {
        .payload = LAUNCH_COMMAND,
        .argv = argv,
        .exit_handler = exit_handler,
    };
    int32_t pid = 0;
    return launch(&spec, &pid);
}

void proc_handle_signal(uint32_t req_id, const uint8_t *body, uint32_t len) {
    uint32_t process_id;
    int32_t sig;
    bool to_group;
    if (!swiptermd_dec_process_signal(body, len, &process_id, &sig, &to_group)) {
        ctrl_send_err_reply(SWIPTERMD_OP_PROCESS_SIGNAL, req_id, -EINVAL);
        return;
    }
    proc_entry_t *proc = NULL;
    for (proc_entry_t *p = g_procs; p; p = p->next) {
        if (p->process_id == process_id) {
            proc = p;
            break;
        }
    }
    if (!proc || proc->exited) {
        ctrl_send_err_reply(SWIPTERMD_OP_PROCESS_SIGNAL, req_id, -ESRCH);
        return;
    }
    pid_t target = to_group ? -proc->outer_pid : proc->outer_pid;
    int err = kill(target, sig) < 0 ? -errno : 0;
    ctrl_send_err_reply(SWIPTERMD_OP_PROCESS_SIGNAL, req_id, err);
}
