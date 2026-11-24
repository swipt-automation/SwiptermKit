#include "swiptermd.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

enum {
    CLIP_MAX_MIMES = 64,
    CLIP_MAX_MIME_LEN = 256,
    CLIP_MAX_DATA_BYTES = 64U * 1024U * 1024U,
    CLIP_RELAY_BUF_CAP = 64 * 1024,

    WL_SERVER_ID_BASE = 0xFF000000U,
    WL_MAX_MSG_SIZE = 4096,
    WL_FD_QUEUE_CAP = 32,

    WL_GLOBAL_SEAT = 1,
    WL_GLOBAL_ZWLR_DATA_CONTROL = 2,
    WL_GLOBAL_EXT_DATA_CONTROL = 3,

    CLIP_SOCK_LIST = 1,
    CLIP_SOCK_READ = 2,
    CLIP_SOCK_WRITE = 3,
};

typedef enum wl_obj_kind {
    WL_OBJ_REGISTRY = 1,
    WL_OBJ_SEAT = 2,
    WL_OBJ_MANAGER = 3,
    WL_OBJ_DEVICE = 4,
    WL_OBJ_SOURCE = 5,
    WL_OBJ_OFFER = 6,
} wl_obj_kind_t;

typedef struct wl_client wl_client_t;
typedef struct wl_obj wl_obj_t;
typedef struct clip_selection clip_selection_t;

typedef struct clip_item {
    char *mime;
    uint8_t *data;
    size_t len;
} clip_item_t;

struct clip_selection {
    unsigned refcount;
    bool from_host;
    bool harvesting;
    char **mimes;
    size_t mime_count;
    clip_item_t *items;
    size_t item_count;
    wl_obj_t *live_source;
};

struct wl_obj {
    uint32_t id;
    wl_obj_kind_t kind;
    uint32_t version;
    bool is_ext;
    bool supports_primary;
    bool used;
    wl_client_t *client;
    char **mimes;
    size_t mime_count;
    clip_selection_t *sel;
    wl_obj_t *next;
};

struct wl_client {
    int fd;
    ev_node_t ev;
    uint32_t next_server_id;

    uint8_t rx_buf[WL_MAX_MSG_SIZE * 2];
    size_t rx_len;
    int rx_fds[WL_FD_QUEUE_CAP];
    size_t rx_fd_count;

    uint8_t *tx_buf;
    size_t tx_len;
    size_t tx_cap;
    size_t tx_off;
    int tx_fds[WL_FD_QUEUE_CAP];
    size_t tx_fd_count;

    wl_obj_t *objs;
    bool dead;
    wl_client_t *next;
};

typedef struct mem_writer {
    int fd;
    ev_node_t ev;
    clip_selection_t *sel;
    const uint8_t *data;
    size_t len;
    size_t off;
    struct mem_writer *next;
} mem_writer_t;

typedef struct host_reader {
    int vsock_fd;
    int dst_fd;
    ev_node_t vsock_ev;
    ev_node_t dst_ev;
    uint8_t status_hdr[4];
    size_t status_got;
    bool status_ok;
    bool vsock_eof;
    uint8_t buf[CLIP_RELAY_BUF_CAP];
    size_t off;
    size_t len;
    struct host_reader *next;
} host_reader_t;

typedef struct host_writer {
    int vsock_fd;
    ev_node_t ev;
    uint8_t *data;
    size_t len;
    size_t off;
    struct host_writer *next;
} host_writer_t;

typedef struct harvest_pipe {
    int fd;
    ev_node_t ev;
    char *mime;
    uint8_t *data;
    size_t len;
    size_t cap;
    bool done;
    struct harvest_job *job;
} harvest_pipe_t;

typedef struct harvest_waiter {
    char *mime;
    int fd;
    struct harvest_waiter *next;
} harvest_waiter_t;

typedef struct harvest_job {
    clip_selection_t *sel;
    harvest_pipe_t pipes[3];
    size_t count;
    size_t remaining;
    harvest_waiter_t *waiters;
    struct harvest_job *next;
} harvest_job_t;

typedef struct clip_sock_conn {
    int fd;
    ev_node_t ev;
    uint8_t hdr[8];
    size_t hdr_got;
    uint8_t cmd;
    bool primary;
    uint16_t mime_len;
    uint32_t data_len;
    char mime[CLIP_MAX_MIME_LEN];
    size_t mime_got;
    uint8_t *data;
    size_t data_got;
    struct clip_sock_conn *next;
} clip_sock_conn_t;

static int g_wl_listen_fd = -1;
static ev_node_t g_wl_listen_ev;
static int g_clip_listen_fd = -1;
static ev_node_t g_clip_listen_ev;

static wl_client_t *g_wl_clients;
static clip_selection_t *g_regular_sel;
static clip_selection_t *g_primary_sel;
static mem_writer_t *g_mem_writers;
static host_reader_t *g_host_readers;
static host_writer_t *g_host_writers;
static harvest_job_t *g_harvest_jobs;
static clip_sock_conn_t *g_clip_conns;
static uint64_t g_host_write_seq;

static const char *const plain_text_mimes[] = {
    "text/plain;charset=utf-8",
    "UTF8_STRING",
    "text/plain",
    "STRING",
    "TEXT",
};

static bool is_plain_text_alias(const char *mime) {
    for (size_t i = 0; i < sizeof(plain_text_mimes) / sizeof(plain_text_mimes[0]); i++) {
        if (strcmp(mime, plain_text_mimes[i]) == 0)
            return true;
    }
    return false;
}

static bool is_text_mime(const char *mime) {
    return is_plain_text_alias(mime) || strncmp(mime, "text/", 5) == 0;
}

static clip_selection_t *clip_sel_ref(clip_selection_t *sel) {
    if (sel)
        sel->refcount++;
    return sel;
}

static void clip_sel_unref(clip_selection_t *sel) {
    if (!sel || --sel->refcount > 0)
        return;
    if (sel->live_source)
        sel->live_source->sel = NULL;
    for (size_t i = 0; i < sel->mime_count; i++)
        free(sel->mimes[i]);
    free(sel->mimes);
    for (size_t i = 0; i < sel->item_count; i++) {
        free(sel->items[i].mime);
        free(sel->items[i].data);
    }
    free(sel->items);
    free(sel);
}

static bool mime_list_contains(char *const *list, size_t count, const char *mime) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i], mime) == 0)
            return true;
    }
    return false;
}

static bool mime_list_add(char ***list, size_t *count, const char *mime) {
    if (!mime || mime[0] == '\0' || strlen(mime) >= CLIP_MAX_MIME_LEN)
        return false;
    if (mime_list_contains(*list, *count, mime))
        return true;
    if (*count >= CLIP_MAX_MIMES)
        return false;
    char *dup = strdup(mime);
    if (!dup)
        return false;
    char **grown = realloc(*list, (*count + 1) * sizeof(**list));
    if (!grown) {
        free(dup);
        return false;
    }
    *list = grown;
    (*list)[(*count)++] = dup;
    return true;
}

static bool sel_add_item(clip_selection_t *sel, const char *mime, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < sel->item_count; i++) {
        if (strcmp(sel->items[i].mime, mime) == 0)
            return true;
    }
    char *mdup = strdup(mime);
    uint8_t *ddup = malloc(len > 0 ? len : 1);
    if (!mdup || !ddup) {
        free(mdup);
        free(ddup);
        return false;
    }
    if (len > 0)
        memcpy(ddup, data, len);
    clip_item_t *grown = realloc(sel->items, (sel->item_count + 1) * sizeof(*sel->items));
    if (!grown) {
        free(mdup);
        free(ddup);
        return false;
    }
    sel->items = grown;
    sel->items[sel->item_count++] = (clip_item_t){.mime = mdup, .data = ddup, .len = len};
    return true;
}

static const clip_item_t *sel_find_item(const clip_selection_t *sel, const char *mime) {
    for (size_t i = 0; i < sel->item_count; i++) {
        if (strcmp(sel->items[i].mime, mime) == 0)
            return &sel->items[i];
    }
    if (is_plain_text_alias(mime)) {
        for (size_t i = 0; i < sel->item_count; i++) {
            if (is_plain_text_alias(sel->items[i].mime))
                return &sel->items[i];
        }
    }
    return NULL;
}

static const char *sel_resolve_mime(const clip_selection_t *sel, const char *requested) {
    if (!sel || sel->mime_count == 0)
        return NULL;
    if (!requested || requested[0] == '\0' || strcmp(requested, "text") == 0) {
        for (size_t p = 0; p < sizeof(plain_text_mimes) / sizeof(plain_text_mimes[0]); p++) {
            if (mime_list_contains(sel->mimes, sel->mime_count, plain_text_mimes[p]))
                return plain_text_mimes[p];
        }
        for (size_t i = 0; i < sel->mime_count; i++) {
            if (is_text_mime(sel->mimes[i]))
                return sel->mimes[i];
        }
        if (!requested || requested[0] == '\0') {
            for (size_t i = 0; i < sel->mime_count; i++) {
                if (strcmp(sel->mimes[i], "x-kde-passwordManagerHint") != 0)
                    return sel->mimes[i];
            }
            return sel->mimes[0];
        }
        return NULL;
    }
    if (strcmp(requested, "image") == 0) {
        for (size_t i = 0; i < sel->mime_count; i++) {
            if (strncmp(sel->mimes[i], "image/", 6) == 0)
                return sel->mimes[i];
        }
        return NULL;
    }
    for (size_t i = 0; i < sel->mime_count; i++) {
        if (strcmp(sel->mimes[i], requested) == 0)
            return sel->mimes[i];
    }
    if (is_plain_text_alias(requested)) {
        for (size_t p = 0; p < sizeof(plain_text_mimes) / sizeof(plain_text_mimes[0]); p++) {
            if (mime_list_contains(sel->mimes, sel->mime_count, plain_text_mimes[p]))
                return plain_text_mimes[p];
        }
    }
    return NULL;
}

static void mem_writer_destroy(mem_writer_t *mw) {
    for (mem_writer_t **pp = &g_mem_writers; *pp; pp = &(*pp)->next) {
        if (*pp == mw) {
            *pp = mw->next;
            break;
        }
    }
    ev_del(&mw->ev);
    if (mw->fd >= 0)
        close(mw->fd);
    clip_sel_unref(mw->sel);
    free(mw);
}

static void on_mem_writer_event(ev_node_t *node, uint32_t events) {
    (void)events;
    mem_writer_t *mw = node->ctx;
    while (mw->off < mw->len) {
        ssize_t n = write(mw->fd, mw->data + mw->off, mw->len - mw->off);
        if (n > 0) {
            mw->off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ev_mod(&mw->ev, EPOLLOUT);
            return;
        }
        mem_writer_destroy(mw);
        return;
    }
    mem_writer_destroy(mw);
}

static void mem_writer_start(clip_selection_t *sel, const uint8_t *data, size_t len, int fd) {
    if (len == 0 || set_nonblock(fd) < 0) {
        if (len > 0)
            (void)write_all_blocking(fd, data, len);
        close(fd);
        return;
    }
    ssize_t n;
    size_t off = 0;
    while (off < len) {
        n = write(fd, data + off, len - off);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        close(fd);
        return;
    }
    if (off == len) {
        close(fd);
        return;
    }
    mem_writer_t *mw = calloc(1, sizeof(*mw));
    if (!mw) {
        close(fd);
        return;
    }
    mw->fd = fd;
    mw->sel = clip_sel_ref(sel);
    mw->data = data;
    mw->len = len;
    mw->off = off;
    if (ev_add(&mw->ev, fd, EPOLLOUT, on_mem_writer_event, mw) < 0) {
        clip_sel_unref(mw->sel);
        close(fd);
        free(mw);
        return;
    }
    mw->next = g_mem_writers;
    g_mem_writers = mw;
}

static void host_reader_destroy(host_reader_t *hr) {
    for (host_reader_t **pp = &g_host_readers; *pp; pp = &(*pp)->next) {
        if (*pp == hr) {
            *pp = hr->next;
            break;
        }
    }
    ev_del(&hr->vsock_ev);
    ev_del(&hr->dst_ev);
    if (hr->vsock_fd >= 0)
        close(hr->vsock_fd);
    if (hr->dst_fd >= 0)
        close(hr->dst_fd);
    free(hr);
}

static void host_reader_pump(host_reader_t *hr) {
    while (!hr->status_ok) {
        ssize_t n = recv(hr->vsock_fd, hr->status_hdr + hr->status_got, 4 - hr->status_got, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ev_mod(&hr->vsock_ev, EPOLLIN);
                ev_mod(&hr->dst_ev, 0);
                return;
            }
            host_reader_destroy(hr);
            return;
        }
        if (n == 0) {
            host_reader_destroy(hr);
            return;
        }
        hr->status_got += (size_t)n;
        if (hr->status_got == 4) {
            int32_t st = (int32_t)swiptermd_get_be32(hr->status_hdr);
            if (st < 0) {
                host_reader_destroy(hr);
                return;
            }
            hr->status_ok = true;
        }
    }

    for (;;) {
        while (hr->off < hr->len) {
            ssize_t w = write(hr->dst_fd, hr->buf + hr->off, hr->len - hr->off);
            if (w > 0) {
                hr->off += (size_t)w;
                continue;
            }
            if (w < 0 && errno == EINTR)
                continue;
            if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                ev_mod(&hr->vsock_ev, 0);
                ev_mod(&hr->dst_ev, EPOLLOUT);
                return;
            }
            host_reader_destroy(hr);
            return;
        }
        hr->off = hr->len = 0;
        if (hr->vsock_eof) {
            host_reader_destroy(hr);
            return;
        }
        ssize_t n = recv(hr->vsock_fd, hr->buf, sizeof(hr->buf), 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ev_mod(&hr->vsock_ev, EPOLLIN);
                ev_mod(&hr->dst_ev, 0);
                return;
            }
            host_reader_destroy(hr);
            return;
        }
        if (n == 0) {
            host_reader_destroy(hr);
            return;
        }
        hr->len = (size_t)n;
    }
}

static void on_host_reader_event(ev_node_t *node, uint32_t events) {
    (void)events;
    host_reader_pump(node->ctx);
}

static void host_reader_start(const char *mime, int dst_fd) {
    size_t mlen = strlen(mime);
    if (mlen == 0 || mlen >= CLIP_MAX_MIME_LEN || set_nonblock(dst_fd) < 0) {
        close(dst_fd);
        return;
    }
    int vfd = vsock_connect_stream(SWIPTERMD_STREAM_CLIPBOARD_READ, 0, 0);
    if (vfd < 0) {
        close(dst_fd);
        return;
    }
    uint8_t req[4 + CLIP_MAX_MIME_LEN];
    swiptermd_put_be32(req, (uint32_t)mlen);
    memcpy(req + 4, mime, mlen);
    ssize_t sent = send(vfd, req, 4 + mlen, MSG_NOSIGNAL);
    if (sent != (ssize_t)(4 + mlen)) {
        close(vfd);
        close(dst_fd);
        return;
    }
    (void)shutdown(vfd, SHUT_WR);

    host_reader_t *hr = calloc(1, sizeof(*hr));
    if (!hr) {
        close(vfd);
        close(dst_fd);
        return;
    }
    hr->vsock_fd = vfd;
    hr->dst_fd = dst_fd;
    if (ev_add(&hr->vsock_ev, vfd, EPOLLIN, on_host_reader_event, hr) < 0 ||
        ev_add(&hr->dst_ev, dst_fd, 0, on_host_reader_event, hr) < 0) {
        ev_del(&hr->vsock_ev);
        ev_del(&hr->dst_ev);
        close(vfd);
        close(dst_fd);
        free(hr);
        return;
    }
    hr->next = g_host_readers;
    g_host_readers = hr;
    host_reader_pump(hr);
}

static void host_writer_destroy(host_writer_t *hw) {
    for (host_writer_t **pp = &g_host_writers; *pp; pp = &(*pp)->next) {
        if (*pp == hw) {
            *pp = hw->next;
            break;
        }
    }
    ev_del(&hw->ev);
    if (hw->vsock_fd >= 0)
        close(hw->vsock_fd);
    free(hw->data);
    free(hw);
}

static void on_host_writer_event(ev_node_t *node, uint32_t events) {
    (void)events;
    host_writer_t *hw = node->ctx;
    while (hw->off < hw->len) {
        ssize_t n = send(hw->vsock_fd, hw->data + hw->off, hw->len - hw->off, MSG_NOSIGNAL);
        if (n > 0) {
            hw->off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ev_mod(&hw->ev, EPOLLOUT);
            return;
        }
        host_writer_destroy(hw);
        return;
    }
    host_writer_destroy(hw);
}

static void push_items_to_host(const clip_item_t *items, size_t count) {
    size_t total = 4;
    uint32_t included = 0;
    for (size_t i = 0; i < count; i++) {
        size_t size = 8 + strlen(items[i].mime) + items[i].len;
        if (size > SWIPTERMD_MAX_CLIPBOARD_STREAM_LEN - total)
            continue;
        total += size;
        included++;
    }
    if (count > 0 && included == 0)
        return;
    uint8_t *buf = malloc(total);
    if (!buf)
        return;
    swiptermd_buf_w_t w = swiptermd_buf_w_init(buf, total);
    swiptermd_w_u32(&w, included);
    size_t written = 4;
    for (size_t i = 0; i < count; i++) {
        size_t size = 8 + strlen(items[i].mime) + items[i].len;
        if (size > SWIPTERMD_MAX_CLIPBOARD_STREAM_LEN - written)
            continue;
        written += size;
        swiptermd_w_str(&w, items[i].mime);
        swiptermd_w_bytes(&w, items[i].data, (uint32_t)items[i].len);
    }
    if (w.overflow) {
        free(buf);
        return;
    }
    int vfd = vsock_connect_stream(SWIPTERMD_STREAM_CLIPBOARD_WRITE, 0, ++g_host_write_seq);
    if (vfd < 0) {
        free(buf);
        return;
    }
    size_t off = 0;
    while (off < w.len) {
        ssize_t n = send(vfd, buf + off, w.len - off, MSG_NOSIGNAL);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        close(vfd);
        free(buf);
        return;
    }
    if (off == w.len) {
        close(vfd);
        free(buf);
        return;
    }
    host_writer_t *hw = calloc(1, sizeof(*hw));
    if (!hw) {
        close(vfd);
        free(buf);
        return;
    }
    hw->vsock_fd = vfd;
    hw->data = buf;
    hw->len = w.len;
    hw->off = off;
    if (ev_add(&hw->ev, vfd, EPOLLOUT, on_host_writer_event, hw) < 0) {
        close(vfd);
        free(buf);
        free(hw);
        return;
    }
    hw->next = g_host_writers;
    g_host_writers = hw;
}

static uint16_t wl_get_u16(const uint8_t *p) {
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static uint32_t wl_get_u32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void wl_put_u32(uint8_t *p, uint32_t v) {
    memcpy(p, &v, sizeof(v));
}

static bool wl_tx_reserve(wl_client_t *c, size_t extra) {
    if (c->tx_cap - c->tx_len >= extra)
        return true;
    if (c->tx_off > 0) {
        size_t rem = c->tx_len - c->tx_off;
        if (rem > 0)
            memmove(c->tx_buf, c->tx_buf + c->tx_off, rem);
        c->tx_len = rem;
        c->tx_off = 0;
        if (c->tx_cap - c->tx_len >= extra)
            return true;
    }
    size_t ncap = c->tx_cap ? c->tx_cap * 2 : 4096;
    while (ncap - c->tx_len < extra)
        ncap *= 2;
    uint8_t *nb = realloc(c->tx_buf, ncap);
    if (!nb)
        return false;
    c->tx_buf = nb;
    c->tx_cap = ncap;
    return true;
}

static void wl_client_flush(wl_client_t *c) {
    if (c->dead || c->fd < 0)
        return;
    while (c->tx_off < c->tx_len || c->tx_fd_count > 0) {
        if (c->tx_off == c->tx_len)
            break;
        struct iovec iov = {
            .iov_base = c->tx_buf + c->tx_off,
            .iov_len = c->tx_len - c->tx_off,
        };
        union {
            struct cmsghdr align;
            uint8_t buf[CMSG_SPACE(sizeof(int) * WL_FD_QUEUE_CAP)];
        } cmsg_u;
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        if (c->tx_fd_count > 0) {
            memset(&cmsg_u, 0, sizeof(cmsg_u));
            size_t fds_bytes = sizeof(int) * c->tx_fd_count;
            msg.msg_control = cmsg_u.buf;
            msg.msg_controllen = CMSG_SPACE(fds_bytes);
            struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
            cm->cmsg_level = SOL_SOCKET;
            cm->cmsg_type = SCM_RIGHTS;
            cm->cmsg_len = CMSG_LEN(fds_bytes);
            memcpy(CMSG_DATA(cm), c->tx_fds, fds_bytes);
        }
        ssize_t n = sendmsg(c->fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ev_mod(&c->ev, EPOLLIN | EPOLLOUT);
                return;
            }
            c->dead = true;
            return;
        }
        if (c->tx_fd_count > 0) {
            for (size_t i = 0; i < c->tx_fd_count; i++)
                close(c->tx_fds[i]);
            c->tx_fd_count = 0;
        }
        c->tx_off += (size_t)n;
    }
    if (c->tx_off == c->tx_len) {
        c->tx_off = c->tx_len = 0;
        ev_mod(&c->ev, EPOLLIN);
    } else {
        ev_mod(&c->ev, EPOLLIN | EPOLLOUT);
    }
}

static bool wl_send_raw(wl_client_t *c, uint32_t obj_id, uint16_t opcode, const void *payload, uint16_t plen) {
    uint16_t total = (uint16_t)(8 + plen);
    if (!wl_tx_reserve(c, total)) {
        c->dead = true;
        return false;
    }
    wl_put_u32(c->tx_buf + c->tx_len + 0, obj_id);
    wl_put_u32(c->tx_buf + c->tx_len + 4, ((uint32_t)total << 16) | opcode);
    if (plen > 0)
        memcpy(c->tx_buf + c->tx_len + 8, payload, plen);
    c->tx_len += total;
    return true;
}

static void wl_send_u32(wl_client_t *c, uint32_t obj_id, uint16_t opcode, uint32_t v) {
    uint8_t b[4];
    wl_put_u32(b, v);
    (void)wl_send_raw(c, obj_id, opcode, b, 4);
}

static void wl_send_str(wl_client_t *c, uint32_t obj_id, uint16_t opcode, const char *s) {
    size_t slen = strlen(s) + 1;
    size_t padded = (slen + 3) & ~(size_t)3;
    if (4 + padded > WL_MAX_MSG_SIZE - 8)
        return;
    uint8_t buf[WL_MAX_MSG_SIZE];
    memset(buf, 0, 4 + padded);
    wl_put_u32(buf, (uint32_t)slen);
    memcpy(buf + 4, s, slen);
    (void)wl_send_raw(c, obj_id, opcode, buf, (uint16_t)(4 + padded));
}

static void wl_send_global(wl_client_t *c, uint32_t reg_id, uint32_t name, const char *iface, uint32_t ver) {
    size_t slen = strlen(iface) + 1;
    size_t padded = (slen + 3) & ~(size_t)3;
    uint8_t buf[256];
    if (8 + padded + 4 > sizeof(buf))
        return;
    memset(buf, 0, 8 + padded + 4);
    wl_put_u32(buf + 0, name);
    wl_put_u32(buf + 4, (uint32_t)slen);
    memcpy(buf + 8, iface, slen);
    wl_put_u32(buf + 8 + padded, ver);
    (void)wl_send_raw(c, reg_id, 0, buf, (uint16_t)(8 + padded + 4));
}

static bool wl_send_source_send(wl_client_t *c, uint32_t src_id, const char *mime, int fd) {
    if (c->dead)
        return false;
    if (c->tx_fd_count >= WL_FD_QUEUE_CAP) {
        wl_client_flush(c);
        if (c->dead || c->tx_fd_count >= WL_FD_QUEUE_CAP)
            return false;
    }
    int dup_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (dup_fd < 0)
        return false;
    wl_send_str(c, src_id, 0, mime);
    if (c->dead) {
        close(dup_fd);
        return false;
    }
    c->tx_fds[c->tx_fd_count++] = dup_fd;
    wl_client_flush(c);
    return !c->dead;
}

static void wl_send_error(wl_client_t *c, uint32_t obj_id, uint32_t code, const char *msg) {
    size_t slen = strlen(msg) + 1;
    size_t padded = (slen + 3) & ~(size_t)3;
    uint8_t buf[512];
    if (8 + 4 + padded > sizeof(buf))
        return;
    memset(buf, 0, 12 + padded);
    wl_put_u32(buf + 0, obj_id);
    wl_put_u32(buf + 4, code);
    wl_put_u32(buf + 8, (uint32_t)slen);
    memcpy(buf + 12, msg, slen);
    (void)wl_send_raw(c, 1, 0, buf, (uint16_t)(12 + padded));
    wl_client_flush(c);
    c->dead = true;
}

static wl_obj_t *wl_obj_find(wl_client_t *c, uint32_t id) {
    for (wl_obj_t *o = c->objs; o; o = o->next) {
        if (o->id == id)
            return o;
    }
    return NULL;
}

static wl_obj_t *wl_obj_create(wl_client_t *c, uint32_t id, wl_obj_kind_t kind) {
    if (id == 0 || wl_obj_find(c, id))
        return NULL;
    wl_obj_t *o = calloc(1, sizeof(*o));
    if (!o)
        return NULL;
    o->id = id;
    o->kind = kind;
    o->client = c;
    o->next = c->objs;
    c->objs = o;
    return o;
}

static void wl_notify_devices(bool primary);

static void wl_obj_free_fields(wl_obj_t *o) {
    if (o->kind == WL_OBJ_SOURCE && o->sel) {
        clip_selection_t *sel = o->sel;
        sel->live_source = NULL;
        o->sel = NULL;
        if (!sel->harvesting && sel->item_count == 0) {
            if (g_regular_sel == sel) {
                clip_sel_unref(g_regular_sel);
                g_regular_sel = NULL;
                wl_notify_devices(false);
            }
            if (g_primary_sel == sel) {
                clip_sel_unref(g_primary_sel);
                g_primary_sel = NULL;
                wl_notify_devices(true);
            }
        }
    }
    if (o->kind == WL_OBJ_OFFER && o->sel) {
        clip_sel_unref(o->sel);
        o->sel = NULL;
    }
    for (size_t i = 0; i < o->mime_count; i++)
        free(o->mimes[i]);
    free(o->mimes);
}

static void wl_obj_destroy(wl_client_t *c, uint32_t id, bool client_created) {
    for (wl_obj_t **pp = &c->objs; *pp; pp = &(*pp)->next) {
        if ((*pp)->id == id) {
            wl_obj_t *o = *pp;
            *pp = o->next;
            wl_obj_free_fields(o);
            free(o);
            break;
        }
    }
    if (client_created && id < WL_SERVER_ID_BASE)
        wl_send_u32(c, 1, 1, id);
}

static void wl_emit_device_selection(wl_obj_t *dev, bool primary) {
    if (primary && !dev->supports_primary)
        return;
    clip_selection_t *sel = primary ? g_primary_sel : g_regular_sel;
    uint16_t sel_opcode = primary ? 3 : 1;
    if (!sel || sel->mime_count == 0) {
        wl_send_u32(dev->client, dev->id, sel_opcode, 0);
        return;
    }
    uint32_t offer_id = dev->client->next_server_id++;
    if (offer_id < WL_SERVER_ID_BASE) {
        offer_id = WL_SERVER_ID_BASE;
        dev->client->next_server_id = WL_SERVER_ID_BASE + 1;
    }
    wl_obj_t *offer = wl_obj_create(dev->client, offer_id, WL_OBJ_OFFER);
    if (!offer) {
        wl_send_u32(dev->client, dev->id, sel_opcode, 0);
        return;
    }
    offer->sel = clip_sel_ref(sel);
    wl_send_u32(dev->client, dev->id, 0, offer_id);
    for (size_t i = 0; i < sel->mime_count; i++)
        wl_send_str(dev->client, offer_id, 0, sel->mimes[i]);
    wl_send_u32(dev->client, dev->id, sel_opcode, offer_id);
}

static void wl_notify_devices(bool primary) {
    for (wl_client_t *c = g_wl_clients; c; c = c->next) {
        if (c->dead)
            continue;
        bool sent = false;
        for (wl_obj_t *o = c->objs; o; o = o->next) {
            if (o->kind == WL_OBJ_DEVICE) {
                if (!primary || o->supports_primary) {
                    wl_emit_device_selection(o, primary);
                    sent = true;
                }
            }
        }
        if (sent)
            wl_client_flush(c);
    }
}

static void cancel_live_source(clip_selection_t *old_sel, clip_selection_t *other_sel) {
    if (!old_sel || !old_sel->live_source)
        return;
    wl_obj_t *src = old_sel->live_source;
    old_sel->live_source = NULL;
    src->sel = NULL;
    if (other_sel && other_sel->live_source == src)
        return;
    if (!src->client->dead) {
        (void)wl_send_raw(src->client, src->id, 1, NULL, 0);
        wl_client_flush(src->client);
    }
}

static void clip_serve_fd(clip_selection_t *sel, const char *mime, int dst_fd);

static void harvest_job_destroy(harvest_job_t *job) {
    for (harvest_job_t **pp = &g_harvest_jobs; *pp; pp = &(*pp)->next) {
        if (*pp == job) {
            *pp = job->next;
            break;
        }
    }
    while (job->waiters) {
        harvest_waiter_t *w = job->waiters;
        job->waiters = w->next;
        if (w->fd >= 0)
            close(w->fd);
        free(w->mime);
        free(w);
    }
    for (size_t i = 0; i < job->count; i++) {
        ev_del(&job->pipes[i].ev);
        if (job->pipes[i].fd >= 0)
            close(job->pipes[i].fd);
        free(job->pipes[i].mime);
        free(job->pipes[i].data);
    }
    job->sel->harvesting = false;
    clip_sel_unref(job->sel);
    free(job);
}

static void harvest_job_finish(harvest_job_t *job) {
    clip_selection_t *sel = job->sel;
    for (size_t i = 0; i < job->count; i++) {
        if (job->pipes[i].data)
            (void)sel_add_item(sel, job->pipes[i].mime, job->pipes[i].data, job->pipes[i].len);
    }
    sel->harvesting = false;
    if (g_regular_sel == sel && sel->item_count > 0)
        push_items_to_host(sel->items, sel->item_count);
    while (job->waiters) {
        harvest_waiter_t *w = job->waiters;
        job->waiters = w->next;
        clip_serve_fd(sel, w->mime, w->fd);
        free(w->mime);
        free(w);
    }
    if (sel->item_count == 0 && !sel->live_source) {
        if (g_regular_sel == sel) {
            clip_sel_unref(g_regular_sel);
            g_regular_sel = NULL;
            wl_notify_devices(false);
        }
        if (g_primary_sel == sel) {
            clip_sel_unref(g_primary_sel);
            g_primary_sel = NULL;
            wl_notify_devices(true);
        }
    }
    harvest_job_destroy(job);
}

static void on_harvest_pipe_event(ev_node_t *node, uint32_t events) {
    (void)events;
    harvest_pipe_t *hp = node->ctx;
    bool complete = false;
    for (;;) {
        if (hp->len >= CLIP_MAX_DATA_BYTES)
            break;
        if (hp->cap - hp->len < 16384) {
            size_t ncap = hp->cap ? hp->cap * 2 : 16384;
            if (ncap > CLIP_MAX_DATA_BYTES)
                ncap = CLIP_MAX_DATA_BYTES;
            uint8_t *nb = realloc(hp->data, ncap);
            if (!nb)
                break;
            hp->data = nb;
            hp->cap = ncap;
        }
        ssize_t n = read(hp->fd, hp->data + hp->len, hp->cap - hp->len);
        if (n > 0) {
            hp->len += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        complete = n == 0 && hp->len > 0;
        break;
    }
    ev_del(&hp->ev);
    close(hp->fd);
    hp->fd = -1;
    hp->done = true;
    if (!complete) {
        free(hp->data);
        hp->data = NULL;
        hp->len = hp->cap = 0;
    }
    if (--hp->job->remaining == 0)
        harvest_job_finish(hp->job);
}

static void harvest_wayland_source(clip_selection_t *sel, wl_obj_t *src, bool push_host) {
    const char *wanted[3] = {NULL, NULL, NULL};
    size_t nwanted = 0;

    for (size_t p = 0; p < sizeof(plain_text_mimes) / sizeof(plain_text_mimes[0]); p++) {
        if (mime_list_contains(src->mimes, src->mime_count, plain_text_mimes[p])) {
            wanted[nwanted++] = plain_text_mimes[p];
            break;
        }
    }
    if (mime_list_contains(src->mimes, src->mime_count, "text/html"))
        wanted[nwanted++] = "text/html";

    static const char *const img_mimes[] = {"image/png", "image/jpeg", "image/gif", "image/webp", "image/svg+xml"};
    for (size_t i = 0; i < sizeof(img_mimes) / sizeof(img_mimes[0]); i++) {
        if (mime_list_contains(src->mimes, src->mime_count, img_mimes[i])) {
            wanted[nwanted++] = img_mimes[i];
            break;
        }
    }
    (void)push_host;
    if (nwanted == 0)
        return;

    harvest_job_t *job = calloc(1, sizeof(*job));
    if (!job)
        return;
    job->sel = clip_sel_ref(sel);
    for (size_t i = 0; i < 3; i++)
        job->pipes[i].fd = -1;

    size_t started = 0;
    for (size_t i = 0; i < nwanted; i++) {
        int pfd[2];
        if (pipe2(pfd, O_CLOEXEC) < 0)
            continue;
        if (set_nonblock(pfd[0]) < 0 || !wl_send_source_send(src->client, src->id, wanted[i], pfd[1])) {
            close(pfd[0]);
            close(pfd[1]);
            continue;
        }
        close(pfd[1]);
        harvest_pipe_t *hp = &job->pipes[started];
        hp->fd = pfd[0];
        hp->mime = strdup(wanted[i]);
        hp->data = malloc(1);
        hp->cap = hp->data ? 1 : 0;
        hp->job = job;
        if (!hp->mime || !hp->data || ev_add(&hp->ev, pfd[0], EPOLLIN, on_harvest_pipe_event, hp) < 0) {
            close(pfd[0]);
            hp->fd = -1;
            free(hp->mime);
            free(hp->data);
            hp->mime = NULL;
            hp->data = NULL;
            continue;
        }
        started++;
    }
    if (started == 0) {
        clip_sel_unref(job->sel);
        free(job);
        return;
    }
    sel->harvesting = true;
    job->count = started;
    job->remaining = started;
    job->next = g_harvest_jobs;
    g_harvest_jobs = job;
}

static bool harvest_try_enqueue_waiter(clip_selection_t *sel, const char *mime, int dst_fd) {
    if (!sel->harvesting)
        return false;
    for (harvest_job_t *job = g_harvest_jobs; job; job = job->next) {
        if (job->sel != sel)
            continue;
        bool match = false;
        for (size_t i = 0; i < job->count; i++) {
            if (!job->pipes[i].mime)
                continue;
            if (strcmp(job->pipes[i].mime, mime) == 0 ||
                (is_plain_text_alias(mime) && is_plain_text_alias(job->pipes[i].mime))) {
                match = true;
                break;
            }
        }
        if (!match)
            return false;
        harvest_waiter_t *w = calloc(1, sizeof(*w));
        if (!w || !(w->mime = strdup(mime))) {
            free(w);
            return false;
        }
        w->fd = dst_fd;
        w->next = job->waiters;
        job->waiters = w;
        return true;
    }
    return false;
}

static void clip_serve_fd(clip_selection_t *sel, const char *mime, int dst_fd) {
    if (!sel || !mime) {
        close(dst_fd);
        return;
    }
    const clip_item_t *item = sel_find_item(sel, mime);
    if (item) {
        mem_writer_start(sel, item->data, item->len, dst_fd);
        return;
    }
    if (harvest_try_enqueue_waiter(sel, mime, dst_fd))
        return;
    if (sel->live_source && !sel->live_source->client->dead &&
        mime_list_contains(sel->mimes, sel->mime_count, mime)) {
        (void)wl_send_source_send(sel->live_source->client, sel->live_source->id, mime, dst_fd);
        close(dst_fd);
        return;
    }
    if (sel->from_host) {
        const char *resolved = sel_resolve_mime(sel, mime);
        if (resolved) {
            host_reader_start(resolved, dst_fd);
            return;
        }
    }
    close(dst_fd);
}

static bool wl_read_str(const uint8_t **p, size_t *rem, const char **out) {
    if (*rem < 4)
        return false;
    uint32_t slen = wl_get_u32(*p);
    *p += 4;
    *rem -= 4;
    if (slen == 0) {
        *out = "";
        return true;
    }
    size_t padded = ((size_t)slen + 3) & ~(size_t)3;
    if (*rem < padded || (*p)[slen - 1] != '\0')
        return false;
    *out = (const char *)*p;
    *p += padded;
    *rem -= padded;
    return true;
}

static int wl_pop_rx_fd(wl_client_t *c) {
    if (c->rx_fd_count == 0)
        return -1;
    int fd = c->rx_fds[0];
    c->rx_fd_count--;
    if (c->rx_fd_count > 0)
        memmove(c->rx_fds, c->rx_fds + 1, sizeof(int) * c->rx_fd_count);
    return fd;
}

static void wl_handle_set_selection(wl_client_t *c, wl_obj_t *dev, uint32_t src_id, bool primary) {
    clip_selection_t **slot = primary ? &g_primary_sel : &g_regular_sel;
    clip_selection_t *other = primary ? g_regular_sel : g_primary_sel;

    if (src_id == 0) {
        cancel_live_source(*slot, other);
        clip_sel_unref(*slot);
        *slot = NULL;
        wl_notify_devices(primary);
        if (!primary)
            push_items_to_host(NULL, 0);
        return;
    }

    wl_obj_t *src = wl_obj_find(c, src_id);
    if (!src || src->kind != WL_OBJ_SOURCE) {
        wl_send_error(c, dev->id, 0, "invalid data source");
        return;
    }
    if (src->used) {
        wl_send_error(c, dev->id, 1, "source already used");
        return;
    }
    src->used = true;

    clip_selection_t *sel = calloc(1, sizeof(*sel));
    if (!sel)
        return;
    sel->refcount = 1;
    for (size_t i = 0; i < src->mime_count; i++)
        (void)mime_list_add(&sel->mimes, &sel->mime_count, src->mimes[i]);
    sel->live_source = src;
    src->sel = sel;

    cancel_live_source(*slot, other);
    clip_sel_unref(*slot);
    *slot = sel;

    wl_notify_devices(primary);
    harvest_wayland_source(sel, src, !primary);
}

static void wl_dispatch_msg(wl_client_t *c, uint32_t obj_id, uint16_t opcode,
                            const uint8_t *body, size_t body_len) {
    const uint8_t *p = body;
    size_t rem = body_len;

    if (obj_id == 1) {
        if (opcode == 0 && rem == 4) {
            uint32_t cb_id = wl_get_u32(p);
            wl_send_u32(c, cb_id, 0, 0);
            wl_send_u32(c, 1, 1, cb_id);
            return;
        }
        if (opcode == 1 && rem == 4) {
            uint32_t reg_id = wl_get_u32(p);
            if (!wl_obj_create(c, reg_id, WL_OBJ_REGISTRY)) {
                wl_send_error(c, 1, 0, "duplicate registry id");
                return;
            }
            wl_send_global(c, reg_id, WL_GLOBAL_SEAT, "wl_seat", 5);
            wl_send_global(c, reg_id, WL_GLOBAL_ZWLR_DATA_CONTROL, "zwlr_data_control_manager_v1", 2);
            wl_send_global(c, reg_id, WL_GLOBAL_EXT_DATA_CONTROL, "ext_data_control_manager_v1", 1);
            return;
        }
        wl_send_error(c, 1, 1, "unknown wl_display request");
        return;
    }

    wl_obj_t *obj = wl_obj_find(c, obj_id);
    if (!obj) {
        if (obj_id >= WL_SERVER_ID_BASE && opcode == 0) {
            const char *dummy = NULL;
            if (wl_read_str(&p, &rem, &dummy)) {
                int fd = wl_pop_rx_fd(c);
                if (fd >= 0)
                    close(fd);
            }
            return;
        }
        if (obj_id >= WL_SERVER_ID_BASE)
            return;
        wl_send_error(c, 1, 0, "invalid object");
        return;
    }

    switch (obj->kind) {
    case WL_OBJ_REGISTRY: {
        if (opcode != 0 || rem < 8) {
            wl_send_error(c, obj_id, 1, "invalid registry request");
            return;
        }
        uint32_t name = wl_get_u32(p);
        p += 4;
        rem -= 4;
        const char *iface = NULL;
        if (!wl_read_str(&p, &rem, &iface) || rem != 8) {
            wl_send_error(c, obj_id, 1, "malformed bind");
            return;
        }
        uint32_t ver = wl_get_u32(p);
        uint32_t new_id = wl_get_u32(p + 4);
        if (name == WL_GLOBAL_SEAT && strcmp(iface, "wl_seat") == 0 && ver >= 1 && ver <= 5) {
            wl_obj_t *seat = wl_obj_create(c, new_id, WL_OBJ_SEAT);
            if (!seat) {
                wl_send_error(c, obj_id, 0, "bind failed");
                return;
            }
            seat->version = ver;
            wl_send_u32(c, new_id, 0, 0);
            if (ver >= 2)
                wl_send_str(c, new_id, 1, "default");
            return;
        }
        if (name == WL_GLOBAL_ZWLR_DATA_CONTROL &&
            strcmp(iface, "zwlr_data_control_manager_v1") == 0 && ver >= 1 && ver <= 2) {
            wl_obj_t *mgr = wl_obj_create(c, new_id, WL_OBJ_MANAGER);
            if (!mgr) {
                wl_send_error(c, obj_id, 0, "bind failed");
                return;
            }
            mgr->version = ver;
            mgr->is_ext = false;
            return;
        }
        if (name == WL_GLOBAL_EXT_DATA_CONTROL &&
            strcmp(iface, "ext_data_control_manager_v1") == 0 && ver == 1) {
            wl_obj_t *mgr = wl_obj_create(c, new_id, WL_OBJ_MANAGER);
            if (!mgr) {
                wl_send_error(c, obj_id, 0, "bind failed");
                return;
            }
            mgr->version = 1;
            mgr->is_ext = true;
            return;
        }
        wl_send_error(c, obj_id, 0, "unknown global");
        return;
    }
    case WL_OBJ_SEAT:
        if (opcode == 3 && rem == 0) {
            wl_obj_destroy(c, obj_id, true);
            return;
        }
        wl_send_error(c, obj_id, 1, "unsupported seat request");
        return;

    case WL_OBJ_MANAGER:
        if (opcode == 0 && rem == 4) {
            uint32_t sid = wl_get_u32(p);
            wl_obj_t *src = wl_obj_create(c, sid, WL_OBJ_SOURCE);
            if (!src) {
                wl_send_error(c, obj_id, 0, "source id failed");
                return;
            }
            src->is_ext = obj->is_ext;
            return;
        }
        if (opcode == 1 && rem == 8) {
            uint32_t did = wl_get_u32(p);
            uint32_t seat_id = wl_get_u32(p + 4);
            wl_obj_t *seat = wl_obj_find(c, seat_id);
            if (!seat || seat->kind != WL_OBJ_SEAT) {
                wl_send_error(c, obj_id, 0, "invalid seat");
                return;
            }
            wl_obj_t *dev = wl_obj_create(c, did, WL_OBJ_DEVICE);
            if (!dev) {
                wl_send_error(c, obj_id, 0, "device id failed");
                return;
            }
            dev->is_ext = obj->is_ext;
            dev->version = obj->version;
            dev->supports_primary = obj->is_ext || obj->version >= 2;
            wl_emit_device_selection(dev, false);
            wl_emit_device_selection(dev, true);
            return;
        }
        if (opcode == 2 && rem == 0) {
            wl_obj_destroy(c, obj_id, true);
            return;
        }
        wl_send_error(c, obj_id, 1, "unknown manager request");
        return;

    case WL_OBJ_DEVICE:
        if (opcode == 0 && rem == 4) {
            wl_handle_set_selection(c, obj, wl_get_u32(p), false);
            return;
        }
        if (opcode == 1 && rem == 0) {
            wl_obj_destroy(c, obj_id, true);
            return;
        }
        if (opcode == 2 && rem == 4 && obj->supports_primary) {
            wl_handle_set_selection(c, obj, wl_get_u32(p), true);
            return;
        }
        wl_send_error(c, obj_id, 1, "unknown device request");
        return;

    case WL_OBJ_SOURCE:
        if (opcode == 0) {
            const char *mime = NULL;
            if (!wl_read_str(&p, &rem, &mime) || rem != 0) {
                wl_send_error(c, obj_id, 1, "malformed offer");
                return;
            }
            if (obj->used) {
                wl_send_error(c, obj_id, 1, "offer after set_selection");
                return;
            }
            (void)mime_list_add(&obj->mimes, &obj->mime_count, mime);
            return;
        }
        if (opcode == 1 && rem == 0) {
            wl_obj_destroy(c, obj_id, true);
            return;
        }
        wl_send_error(c, obj_id, 1, "unknown source request");
        return;

    case WL_OBJ_OFFER:
        if (opcode == 0) {
            const char *mime = NULL;
            if (!wl_read_str(&p, &rem, &mime) || rem != 0) {
                wl_send_error(c, obj_id, 1, "malformed receive");
                return;
            }
            int fd = wl_pop_rx_fd(c);
            if (fd < 0) {
                wl_send_error(c, obj_id, 1, "missing receive fd");
                return;
            }
            // wl-clipboard-rs get_contents_internal (src/paste.rs:399-408) sends
            // offer.receive + wl_display.sync, blocks on roundtrip() before reading
            // a single byte from the pipe, and drops the Wayland socket immediately
            // after roundtrip() returns. Pipe transfer must run asynchronously via
            // epoll and outlive the client connection.
            clip_serve_fd(obj->sel, mime, fd);
            return;
        }
        if (opcode == 1 && rem == 0) {
            wl_obj_destroy(c, obj_id, false);
            return;
        }
        wl_send_error(c, obj_id, 1, "unknown offer request");
        return;
    }
}

static void wl_client_destroy(wl_client_t *c) {
    for (wl_client_t **pp = &g_wl_clients; *pp; pp = &(*pp)->next) {
        if (*pp == c) {
            *pp = c->next;
            break;
        }
    }
    ev_del(&c->ev);
    if (c->fd >= 0)
        close(c->fd);
    for (size_t i = 0; i < c->rx_fd_count; i++)
        close(c->rx_fds[i]);
    for (size_t i = 0; i < c->tx_fd_count; i++)
        close(c->tx_fds[i]);
    while (c->objs) {
        wl_obj_t *o = c->objs;
        c->objs = o->next;
        wl_obj_free_fields(o);
        free(o);
    }
    free(c->tx_buf);
    free(c);
}

static void on_wl_client_event(ev_node_t *node, uint32_t events) {
    wl_client_t *c = node->ctx;
    if (events & EPOLLOUT) {
        wl_client_flush(c);
        if (c->dead) {
            wl_client_destroy(c);
            return;
        }
    }
    if (events & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
        for (;;) {
            if (c->rx_len == sizeof(c->rx_buf)) {
                wl_client_destroy(c);
                return;
            }
            struct iovec iov = {
                .iov_base = c->rx_buf + c->rx_len,
                .iov_len = sizeof(c->rx_buf) - c->rx_len,
            };
            union {
                struct cmsghdr align;
                uint8_t buf[CMSG_SPACE(sizeof(int) * 16)];
            } cmsg_u;
            memset(&cmsg_u, 0, sizeof(cmsg_u));
            struct msghdr msg = {
                .msg_iov = &iov,
                .msg_iovlen = 1,
                .msg_control = cmsg_u.buf,
                .msg_controllen = sizeof(cmsg_u.buf),
            };
            ssize_t n = recvmsg(c->fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                wl_client_destroy(c);
                return;
            }
            for (struct cmsghdr *cm = CMSG_FIRSTHDR(&msg); cm; cm = CMSG_NXTHDR(&msg, cm)) {
                if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
                    size_t nfds = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                    int fds[16];
                    if (nfds > 16)
                        nfds = 16;
                    memcpy(fds, CMSG_DATA(cm), nfds * sizeof(int));
                    for (size_t i = 0; i < nfds; i++) {
                        if (c->rx_fd_count < WL_FD_QUEUE_CAP)
                            c->rx_fds[c->rx_fd_count++] = fds[i];
                        else
                            close(fds[i]);
                    }
                }
            }
            if (msg.msg_flags & MSG_CTRUNC) {
                wl_client_destroy(c);
                return;
            }
            if (n == 0) {
                wl_client_destroy(c);
                return;
            }
            c->rx_len += (size_t)n;

            size_t consumed = 0;
            while (c->rx_len - consumed >= 8) {
                const uint8_t *hdr = c->rx_buf + consumed;
                uint32_t obj_id = wl_get_u32(hdr + 0);
                uint32_t size_op = wl_get_u32(hdr + 4);
                uint16_t size = (uint16_t)(size_op >> 16);
                uint16_t opcode = wl_get_u16(hdr + 4);
                if (size < 8 || (size & 3) != 0 || size > WL_MAX_MSG_SIZE) {
                    wl_client_destroy(c);
                    return;
                }
                if (c->rx_len - consumed < size)
                    break;
                wl_dispatch_msg(c, obj_id, opcode, hdr + 8, (size_t)(size - 8));
                consumed += size;
                if (c->dead) {
                    wl_client_destroy(c);
                    return;
                }
            }
            if (consumed > 0) {
                size_t rem = c->rx_len - consumed;
                if (rem > 0)
                    memmove(c->rx_buf, c->rx_buf + consumed, rem);
                c->rx_len = rem;
            }
        }
        wl_client_flush(c);
        if (c->dead)
            wl_client_destroy(c);
    }
}

static void on_wl_accept(ev_node_t *node, uint32_t events) {
    (void)node;
    if (!(events & EPOLLIN))
        return;
    for (;;) {
        int cfd = accept4(g_wl_listen_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        wl_client_t *c = calloc(1, sizeof(*c));
        if (!c) {
            close(cfd);
            continue;
        }
        c->fd = cfd;
        c->next_server_id = WL_SERVER_ID_BASE;
        if (ev_add(&c->ev, cfd, EPOLLIN, on_wl_client_event, c) < 0) {
            close(cfd);
            free(c);
            continue;
        }
        c->next = g_wl_clients;
        g_wl_clients = c;
    }
}

static void clip_sock_destroy(clip_sock_conn_t *cc) {
    for (clip_sock_conn_t **pp = &g_clip_conns; *pp; pp = &(*pp)->next) {
        if (*pp == cc) {
            *pp = cc->next;
            break;
        }
    }
    ev_del(&cc->ev);
    if (cc->fd >= 0)
        close(cc->fd);
    free(cc->data);
    free(cc);
}

static void clip_sock_handle(clip_sock_conn_t *cc) {
    clip_selection_t **slot = cc->primary ? &g_primary_sel : &g_regular_sel;
    clip_selection_t *other = cc->primary ? g_regular_sel : g_primary_sel;

    if (cc->cmd == CLIP_SOCK_LIST) {
        clip_selection_t *sel = *slot;
        uint32_t count = sel ? (uint32_t)sel->mime_count : 0;
        size_t total = 4;
        for (uint32_t i = 0; i < count; i++)
            total += 4 + strlen(sel->mimes[i]);
        uint8_t *out = malloc(total);
        if (out) {
            swiptermd_buf_w_t w = swiptermd_buf_w_init(out, total);
            swiptermd_w_strv(&w, sel ? (const char *const *)sel->mimes : NULL, count);
            (void)write_all_blocking(cc->fd, out, w.len);
            free(out);
        }
        clip_sock_destroy(cc);
        return;
    }

    if (cc->cmd == CLIP_SOCK_READ) {
        clip_selection_t *sel = *slot;
        uint8_t reply[6 + CLIP_MAX_MIME_LEN];
        if (!sel || sel->mime_count == 0) {
            swiptermd_put_be32(reply, (uint32_t)(-ENODATA));
            (void)write_all_blocking(cc->fd, reply, 4);
            clip_sock_destroy(cc);
            return;
        }
        const char *resolved = sel_resolve_mime(sel, cc->mime);
        if (!resolved) {
            swiptermd_put_be32(reply, (uint32_t)(-EMEDIUMTYPE));
            (void)write_all_blocking(cc->fd, reply, 4);
            clip_sock_destroy(cc);
            return;
        }
        uint16_t rlen = (uint16_t)strlen(resolved);
        swiptermd_put_be32(reply + 0, 0);
        swiptermd_put_be16(reply + 4, rlen);
        memcpy(reply + 6, resolved, rlen);
        if (write_all_blocking(cc->fd, reply, (size_t)(6 + rlen)) < 0) {
            clip_sock_destroy(cc);
            return;
        }
        int fd = cc->fd;
        cc->fd = -1;
        ev_del(&cc->ev);
        clip_sock_destroy(cc);
        clip_serve_fd(sel, resolved, fd);
        return;
    }

    if (cc->cmd == CLIP_SOCK_WRITE) {
        if (cc->mime_len == 0 && cc->data_len == 0) {
            cancel_live_source(*slot, other);
            clip_sel_unref(*slot);
            *slot = NULL;
            wl_notify_devices(cc->primary);
            if (!cc->primary)
                push_items_to_host(NULL, 0);
        } else {
            const char *mime = cc->mime[0] ? cc->mime : "text/plain;charset=utf-8";
            clip_selection_t *sel = calloc(1, sizeof(*sel));
            if (sel) {
                sel->refcount = 1;
                if (is_plain_text_alias(mime)) {
                    for (size_t i = 0; i < sizeof(plain_text_mimes) / sizeof(plain_text_mimes[0]); i++)
                        (void)mime_list_add(&sel->mimes, &sel->mime_count, plain_text_mimes[i]);
                }
                (void)mime_list_add(&sel->mimes, &sel->mime_count, mime);
                (void)sel_add_item(sel, mime, cc->data ? cc->data : (const uint8_t *)"", cc->data_len);
                cancel_live_source(*slot, other);
                clip_sel_unref(*slot);
                *slot = sel;
                wl_notify_devices(cc->primary);
                if (!cc->primary)
                    push_items_to_host(sel->items, sel->item_count);
            }
        }
        uint8_t ok[4];
        swiptermd_put_be32(ok, 0);
        (void)write_all_blocking(cc->fd, ok, sizeof(ok));
        clip_sock_destroy(cc);
        return;
    }

    clip_sock_destroy(cc);
}

static void on_clip_sock_event(ev_node_t *node, uint32_t events) {
    (void)events;
    clip_sock_conn_t *cc = node->ctx;
    while (cc->hdr_got < 8) {
        ssize_t n = read(cc->fd, cc->hdr + cc->hdr_got, 8 - cc->hdr_got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            clip_sock_destroy(cc);
            return;
        }
        if (n == 0) {
            clip_sock_destroy(cc);
            return;
        }
        cc->hdr_got += (size_t)n;
        if (cc->hdr_got == 8) {
            cc->cmd = cc->hdr[0];
            cc->primary = cc->hdr[1] != 0;
            cc->mime_len = swiptermd_get_be16(cc->hdr + 2);
            cc->data_len = swiptermd_get_be32(cc->hdr + 4);
            if (cc->mime_len >= CLIP_MAX_MIME_LEN || cc->data_len > CLIP_MAX_DATA_BYTES ||
                (cc->cmd != CLIP_SOCK_WRITE && cc->data_len != 0)) {
                clip_sock_destroy(cc);
                return;
            }
            if (cc->data_len > 0) {
                cc->data = malloc(cc->data_len);
                if (!cc->data) {
                    clip_sock_destroy(cc);
                    return;
                }
            }
        }
    }
    while (cc->mime_got < cc->mime_len) {
        ssize_t n = read(cc->fd, cc->mime + cc->mime_got, cc->mime_len - cc->mime_got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            clip_sock_destroy(cc);
            return;
        }
        if (n == 0) {
            clip_sock_destroy(cc);
            return;
        }
        cc->mime_got += (size_t)n;
    }
    cc->mime[cc->mime_len] = '\0';

    while (cc->data_got < cc->data_len) {
        ssize_t n = read(cc->fd, cc->data + cc->data_got, cc->data_len - cc->data_got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            clip_sock_destroy(cc);
            return;
        }
        if (n == 0) {
            clip_sock_destroy(cc);
            return;
        }
        cc->data_got += (size_t)n;
    }

    int flags = fcntl(cc->fd, F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(cc->fd, F_SETFL, flags & ~O_NONBLOCK);
    clip_sock_handle(cc);
}

static void on_clip_accept(ev_node_t *node, uint32_t events) {
    (void)node;
    if (!(events & EPOLLIN))
        return;
    for (;;) {
        int cfd = accept4(g_clip_listen_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        clip_sock_conn_t *cc = calloc(1, sizeof(*cc));
        if (!cc) {
            close(cfd);
            continue;
        }
        cc->fd = cfd;
        if (ev_add(&cc->ev, cfd, EPOLLIN, on_clip_sock_event, cc) < 0) {
            close(cfd);
            free(cc);
            continue;
        }
        cc->next = g_clip_conns;
        g_clip_conns = cc;
        on_clip_sock_event(&cc->ev, EPOLLIN);
    }
}

static int listen_unix_socket(const char *path, ev_node_t *ev, ev_cb_t cb) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr.sun_path))
        return -ENAMETOOLONG;
    strcpy(addr.sun_path, path);
    unlink(path);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -errno;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 64) < 0) {
        int err = -errno;
        close(fd);
        return err;
    }
    chmod(path, 0666);
    int rc = ev_add(ev, fd, EPOLLIN, cb, NULL);
    if (rc < 0) {
        close(fd);
        unlink(path);
        return rc;
    }
    return fd;
}

void clipboard_handle_offer(const uint8_t *body, uint32_t len) {
    swiptermd_buf_r_t r = swiptermd_buf_r_init(body, len);
    uint32_t count = swiptermd_r_u32(&r);
    if (r.underflow || count > CLIP_MAX_MIMES)
        return;

    clip_selection_t *sel = NULL;
    if (count > 0) {
        sel = calloc(1, sizeof(*sel));
        if (!sel)
            return;
        sel->refcount = 1;
        sel->from_host = true;
        for (uint32_t i = 0; i < count; i++) {
            char mime[CLIP_MAX_MIME_LEN];
            if (!swiptermd_r_str_copy(&r, mime, sizeof(mime))) {
                clip_sel_unref(sel);
                return;
            }
            (void)mime_list_add(&sel->mimes, &sel->mime_count, mime);
        }
        if (sel->mime_count == 0) {
            clip_sel_unref(sel);
            sel = NULL;
        }
    }

    cancel_live_source(g_regular_sel, g_primary_sel);
    clip_sel_unref(g_regular_sel);
    g_regular_sel = sel;
    wl_notify_devices(false);
}

int clipboard_setup(void) {
    int wfd = listen_unix_socket(GUEST_ROOT GUEST_WAYLAND_SOCKET, &g_wl_listen_ev, on_wl_accept);
    if (wfd < 0)
        return wfd;
    g_wl_listen_fd = wfd;

    int cfd = listen_unix_socket(GUEST_ROOT GUEST_CLIPBOARD_SOCKET, &g_clip_listen_ev, on_clip_accept);
    if (cfd < 0) {
        ev_del(&g_wl_listen_ev);
        close(g_wl_listen_fd);
        g_wl_listen_fd = -1;
        unlink(GUEST_ROOT GUEST_WAYLAND_SOCKET);
        return cfd;
    }
    g_clip_listen_fd = cfd;
    return 0;
}

void clipboard_shutdown(void) {
    ev_del(&g_wl_listen_ev);
    if (g_wl_listen_fd >= 0) {
        close(g_wl_listen_fd);
        g_wl_listen_fd = -1;
    }
    ev_del(&g_clip_listen_ev);
    if (g_clip_listen_fd >= 0) {
        close(g_clip_listen_fd);
        g_clip_listen_fd = -1;
    }
    while (g_harvest_jobs)
        harvest_job_destroy(g_harvest_jobs);
    while (g_mem_writers)
        mem_writer_destroy(g_mem_writers);
    while (g_host_readers)
        host_reader_destroy(g_host_readers);
    while (g_host_writers)
        host_writer_destroy(g_host_writers);
    while (g_clip_conns)
        clip_sock_destroy(g_clip_conns);
    while (g_wl_clients)
        wl_client_destroy(g_wl_clients);
    clip_sel_unref(g_regular_sel);
    g_regular_sel = NULL;
    clip_sel_unref(g_primary_sel);
    g_primary_sel = NULL;
}
