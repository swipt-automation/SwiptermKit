#include "virtio_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

enum {
    VSOCK_HOST_CID = 2,
    VSOCK_VQ_RX = 0,
    VSOCK_VQ_TX = 1,
    VSOCK_VQ_EVENT = 2,
    VSOCK_QUEUE_SIZE = 256,
    VSOCK_EVENT_QUEUE_SIZE = 16,
    VSOCK_MAX_IOV = 64,
    VSOCK_BUF_ALLOC = 256 * 1024,
    VSOCK_INITIAL_CAPACITY = 64,
    VSOCK_ACCEPT_BACKLOG = 32,
    VSOCK_RW_CHUNK = 65536,
    VSOCK_EPHEMERAL_PORT = 49152,
};

enum : uint16_t {
    VIRTIO_VSOCK_TYPE_STREAM = 1,
};

enum : uint16_t {
    VIRTIO_VSOCK_OP_INVALID = 0,
    VIRTIO_VSOCK_OP_REQUEST = 1,
    VIRTIO_VSOCK_OP_RESPONSE = 2,
    VIRTIO_VSOCK_OP_RST = 3,
    VIRTIO_VSOCK_OP_SHUTDOWN = 4,
    VIRTIO_VSOCK_OP_RW = 5,
    VIRTIO_VSOCK_OP_CREDIT_UPDATE = 6,
    VIRTIO_VSOCK_OP_CREDIT_REQUEST = 7,
};

enum : uint32_t {
    VIRTIO_VSOCK_SHUTDOWN_RCV = 1,
    VIRTIO_VSOCK_SHUTDOWN_SEND = 2,
};

typedef struct __attribute__((packed)) virtio_vsock_config {
    uint64_t guest_cid;
} virtio_vsock_config_t;

typedef struct __attribute__((packed)) virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} virtio_vsock_hdr_t;

static_assert(sizeof(virtio_vsock_hdr_t) == 44);

typedef enum vsock_stream_state : uint8_t {
    VSOCK_STATE_CLOSED = 0,
    VSOCK_STATE_CONNECTING = 1,
    VSOCK_STATE_ACCEPTING = 2,
    VSOCK_STATE_ESTABLISHED = 3,
} vsock_stream_state_t;

typedef struct vsock_stream {
    TAILQ_ENTRY(vsock_stream) entry;
    TAILQ_ENTRY(vsock_stream) ctrl_entry;
    struct vsock_stream *hash_next;
    vsock_stream_state_t state;
    uint16_t ctrl_op;
    uint32_t host_port;
    uint32_t guest_port;
    int host_fd;
    bool host_eof;
    bool guest_shut_send;
    bool guest_shut_rcv;
    bool connect_waiter;
    int connect_err;

    uint32_t peer_buf_alloc;
    uint32_t peer_fwd_cnt;
    uint32_t tx_cnt;

    uint32_t fwd_cnt;
    uint32_t last_fwd_sent;

    uint8_t *g2h_buf;
    size_t g2h_head;
    size_t g2h_len;

    size_t prefix_off;
    size_t prefix_len;
    uint8_t prefix[];
} vsock_stream_t;

typedef struct vsock_key {
    uint32_t host_port;
    uint32_t guest_port;
} vsock_key_t;

typedef struct vsock_listener {
    uint32_t host_port;
    virtio_vsock_accept_cb cb;
    void *ctx;
    int pending_fds[VSOCK_ACCEPT_BACKLOG];
    uint32_t pending_head;
    uint32_t pending_count;
} vsock_listener_t;

typedef struct virtio_vsock {
    virtio_dev_t *dev;
    virtio_vsock_config_t config;
    int wake_pipe[2];
    pthread_t thread;
    bool thread_started;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    _Atomic bool stop;
    _Atomic bool halted;
    _Atomic bool wake_pending;
    uint32_t next_ephemeral_port;

    TAILQ_HEAD(, vsock_stream) streams;
    TAILQ_HEAD(, vsock_stream) ctrl;
    vsock_stream_t **buckets;
    size_t bucket_count;
    size_t stream_count;

    vsock_listener_t *listeners;
    size_t listener_count;

    struct pollfd *pfds;
    vsock_key_t *poll_keys;
    size_t poll_capacity;
} virtio_vsock_t;

static void vsock_wake(virtio_vsock_t *vs) {
    if (!atomic_exchange_explicit(&vs->wake_pending, true, memory_order_acq_rel)) {
        uint8_t b = 1;
        ssize_t r;
        do {
            r = write(vs->wake_pipe[1], &b, 1);
        } while (r < 0 && errno == EINTR);
    }
}

static uint32_t vsock_peer_credit(const vsock_stream_t *s) {
    uint32_t in_flight = s->tx_cnt - s->peer_fwd_cnt;
    if (in_flight >= s->peer_buf_alloc) {
        return 0;
    }
    return s->peer_buf_alloc - in_flight;
}

static vsock_stream_t **vsock_bucket(virtio_vsock_t *vs, uint32_t host_port, uint32_t guest_port) {
    uint64_t hash = (((uint64_t)host_port << 32) | guest_port) * 0x9e3779b97f4a7c15ULL;
    return &vs->buckets[(hash >> 32) & (vs->bucket_count - 1)];
}

static vsock_stream_t *vsock_find_stream_locked(virtio_vsock_t *vs, uint32_t host_port, uint32_t guest_port) {
    vsock_stream_t *s = *vsock_bucket(vs, host_port, guest_port);
    while (s && (s->host_port != host_port || s->guest_port != guest_port)) {
        s = s->hash_next;
    }
    return s;
}

static vsock_stream_t *vsock_new_stream_locked(virtio_vsock_t *vs, uint32_t host_port, uint32_t guest_port,
                                               const void *prefix, size_t prefix_length) {
    if (vs->stream_count >= vs->bucket_count) {
        vsock_stream_t **buckets = calloc(vs->bucket_count * 2, sizeof(*buckets));
        if (buckets) {
            free(vs->buckets);
            vs->buckets = buckets;
            vs->bucket_count *= 2;
            vsock_stream_t *t;
            TAILQ_FOREACH(t, &vs->streams, entry) {
                vsock_stream_t **bucket = vsock_bucket(vs, t->host_port, t->guest_port);
                t->hash_next = *bucket;
                *bucket = t;
            }
        }
    }
    vsock_stream_t *s = calloc(1, sizeof(*s) + prefix_length);
    if (!s) {
        return NULL;
    }
    s->host_port = host_port;
    s->guest_port = guest_port;
    s->host_fd = -1;
    if (prefix_length > 0) {
        memcpy(s->prefix, prefix, prefix_length);
    }
    s->prefix_len = prefix_length;

    vsock_stream_t **bucket = vsock_bucket(vs, host_port, guest_port);
    s->hash_next = *bucket;
    *bucket = s;
    TAILQ_INSERT_TAIL(&vs->streams, s, entry);
    vs->stream_count++;
    return s;
}

static void vsock_free_stream_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    vsock_stream_t **link = vsock_bucket(vs, s->host_port, s->guest_port);
    while (*link != s) {
        link = &(*link)->hash_next;
    }
    *link = s->hash_next;
    TAILQ_REMOVE(&vs->streams, s, entry);
    vs->stream_count--;
    free(s);
}

static void vsock_queue_ctrl_locked(virtio_vsock_t *vs, vsock_stream_t *s, uint16_t op) {
    if (s->ctrl_op == VIRTIO_VSOCK_OP_INVALID) {
        TAILQ_INSERT_TAIL(&vs->ctrl, s, ctrl_entry);
        s->ctrl_op = op;
    } else if (op == VIRTIO_VSOCK_OP_RST) {
        s->ctrl_op = op;
    }
}

static void vsock_close_stream_locked(virtio_vsock_t *vs, vsock_stream_t *s, bool send_rst, int err) {
    if (s->state != VSOCK_STATE_CLOSED) {
        if (s->host_fd >= 0 && !s->connect_waiter) {
            close(s->host_fd);
        }
        s->host_fd = -1;
        free(s->g2h_buf);
        s->g2h_buf = NULL;
        s->g2h_len = 0;
        s->state = VSOCK_STATE_CLOSED;
        if (s->connect_waiter) {
            s->connect_err = err ? err : ECONNREFUSED;
            pthread_cond_broadcast(&vs->cond);
        }
    }
    if (send_rst) {
        vsock_queue_ctrl_locked(vs, s, VIRTIO_VSOCK_OP_RST);
        return;
    }
    if (s->ctrl_op != VIRTIO_VSOCK_OP_INVALID) {
        TAILQ_REMOVE(&vs->ctrl, s, ctrl_entry);
        s->ctrl_op = VIRTIO_VSOCK_OP_INVALID;
    }
    if (!s->connect_waiter) {
        vsock_free_stream_locked(vs, s);
    }
}

static void vsock_send_rst_locked(virtio_vsock_t *vs, vsock_stream_t *s, uint32_t host_port, uint32_t guest_port) {
    if (!s) {
        s = vsock_new_stream_locked(vs, host_port, guest_port, NULL, 0);
        if (!s) {
            return;
        }
    }
    vsock_close_stream_locked(vs, s, true, ECONNRESET);
}

static vsock_listener_t *vsock_find_listener_locked(virtio_vsock_t *vs, uint32_t host_port) {
    for (size_t i = 0; i < vs->listener_count; i++) {
        if (vs->listeners[i].host_port == host_port) {
            return &vs->listeners[i];
        }
    }
    return NULL;
}

static vsock_listener_t *vsock_add_listener_locked(virtio_vsock_t *vs, uint32_t host_port,
                                                   virtio_vsock_accept_cb cb, void *ctx) {
    vsock_listener_t *listeners = realloc(vs->listeners, (vs->listener_count + 1) * sizeof(*listeners));
    if (!listeners) {
        return NULL;
    }
    vs->listeners = listeners;
    vsock_listener_t *lst = &listeners[vs->listener_count++];
    *lst = (vsock_listener_t){ .host_port = host_port, .cb = cb, .ctx = ctx };
    return lst;
}

static int vsock_make_socketpair(int sv[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        return -1;
    }
    int one = 1;
    int capacity = VSOCK_BUF_ALLOC;
    for (int i = 0; i < 2; i++) {
        fcntl(sv[i], F_SETFD, FD_CLOEXEC);
        setsockopt(sv[i], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
        (void)setsockopt(sv[i], SOL_SOCKET, SO_SNDBUF, &capacity, sizeof(capacity));
    }
    int fl = fcntl(sv[0], F_GETFL, 0);
    if (fl >= 0) {
        fcntl(sv[0], F_SETFL, fl | O_NONBLOCK);
    }
    return 0;
}

static void vsock_maybe_send_credit_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    if (s->fwd_cnt - s->last_fwd_sent >= VSOCK_BUF_ALLOC / 4) {
        vsock_queue_ctrl_locked(vs, s, VIRTIO_VSOCK_OP_CREDIT_UPDATE);
    }
}

static void vsock_drain_g2h_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    while (s->g2h_len > 0) {
        size_t contig = VSOCK_BUF_ALLOC - s->g2h_head;
        if (contig > s->g2h_len) {
            contig = s->g2h_len;
        }
        struct iovec iov[] = {
            {.iov_base = s->g2h_buf + s->g2h_head, .iov_len = contig},
            {.iov_base = s->g2h_buf, .iov_len = s->g2h_len - contig},
        };
        ssize_t n;
        do {
            n = writev(s->host_fd, iov, iov[1].iov_len ? 2 : 1);
        } while (n < 0 && errno == EINTR);
        if (n > 0) {
            s->g2h_head = (s->g2h_head + (size_t)n) % VSOCK_BUF_ALLOC;
            s->g2h_len -= (size_t)n;
            s->fwd_cnt += (uint32_t)n;
            vsock_maybe_send_credit_locked(vs, s);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        vsock_close_stream_locked(vs, s, true, EPIPE);
        return;
    }
    free(s->g2h_buf);
    s->g2h_buf = NULL;
    if (s->guest_shut_send) {
        shutdown(s->host_fd, SHUT_WR);
        if (s->host_eof || s->guest_shut_rcv) {
            vsock_close_stream_locked(vs, s, true, 0);
        }
    }
}

static void vsock_append_g2h_locked(virtio_vsock_t *vs, vsock_stream_t *s,
                                    const virtio_iov_t *iov, size_t count, size_t len) {
    size_t off = 0;
    if (s->g2h_len == 0) {
        struct iovec spans[VSOCK_MAX_IOV];
        int used = virtio_iov_spans(iov, count, false, sizeof(virtio_vsock_hdr_t), len, spans);
        ssize_t n;
        do {
            n = writev(s->host_fd, spans, used);
        } while (n < 0 && errno == EINTR);
        if (n > 0) {
            off = (size_t)n;
            s->fwd_cnt += (uint32_t)n;
            vsock_maybe_send_credit_locked(vs, s);
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            vsock_close_stream_locked(vs, s, true, EPIPE);
            return;
        }
    }

    size_t rem = len - off;
    if (rem == 0) {
        return;
    }
    if (rem > VSOCK_BUF_ALLOC - s->g2h_len) {
        vsock_close_stream_locked(vs, s, true, ENOBUFS);
        return;
    }
    if (!s->g2h_buf) {
        s->g2h_buf = malloc(VSOCK_BUF_ALLOC);
        if (!s->g2h_buf) {
            vsock_close_stream_locked(vs, s, true, ENOMEM);
            return;
        }
        s->g2h_head = 0;
    }
    size_t tail = (s->g2h_head + s->g2h_len) % VSOCK_BUF_ALLOC;
    size_t first = VSOCK_BUF_ALLOC - tail;
    if (first > rem) {
        first = rem;
    }
    size_t offset = sizeof(virtio_vsock_hdr_t) + off;
    virtio_iov_read(iov, count, offset, s->g2h_buf + tail, first);
    if (rem > first) {
        virtio_iov_read(iov, count, offset + first, s->g2h_buf, rem - first);
    }
    s->g2h_len += rem;
}

static void vsock_push_locked(virtio_vsock_t *vs, vsock_stream_t *s, uint16_t head,
                              const virtio_iov_t *iov, size_t count, uint16_t op, uint32_t flags, size_t len) {
    if (s->ctrl_op == VIRTIO_VSOCK_OP_CREDIT_UPDATE) {
        TAILQ_REMOVE(&vs->ctrl, s, ctrl_entry);
        s->ctrl_op = VIRTIO_VSOCK_OP_INVALID;
    }
    virtio_vsock_hdr_t hdr = {
        .src_cid = VSOCK_HOST_CID,
        .dst_cid = vs->config.guest_cid,
        .src_port = s->host_port,
        .dst_port = s->guest_port,
        .len = (uint32_t)len,
        .type = VIRTIO_VSOCK_TYPE_STREAM,
        .op = op,
        .flags = flags,
        .buf_alloc = VSOCK_BUF_ALLOC,
        .fwd_cnt = s->fwd_cnt,
    };
    s->tx_cnt += (uint32_t)len;
    s->last_fwd_sent = s->fwd_cnt;
    virtio_iov_write(iov, count, 0, &hdr, sizeof(hdr));
    virtqueue_push(vs->dev, &vs->dev->queues[VSOCK_VQ_RX], head, (uint32_t)(sizeof(hdr) + len));
}

static void vsock_flush_ctrl_locked(virtio_vsock_t *vs) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_RX];
    if (!virtqueue_active(dev, vq)) {
        return;
    }
    uint16_t old_used = vq->used_idx;

    while (!TAILQ_EMPTY(&vs->ctrl)) {
        virtio_iov_t iov[VSOCK_MAX_IOV];
        uint16_t head = 0;
        size_t count = 0;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &count) <= 0) {
            break;
        }
        vsock_stream_t *s = TAILQ_FIRST(&vs->ctrl);
        uint16_t op = s->ctrl_op;
        TAILQ_REMOVE(&vs->ctrl, s, ctrl_entry);
        s->ctrl_op = VIRTIO_VSOCK_OP_INVALID;
        vsock_push_locked(vs, s, head, iov, count, op, 0, 0);
        if (op == VIRTIO_VSOCK_OP_RESPONSE) {
            s->state = VSOCK_STATE_ESTABLISHED;
        } else if (op == VIRTIO_VSOCK_OP_RST && !s->connect_waiter) {
            vsock_free_stream_locked(vs, s);
        }
    }

    if (old_used != vq->used_idx && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static void vsock_drain_tx_locked(virtio_vsock_t *vs) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_TX];
    if (!virtqueue_active(dev, vq)) {
        return;
    }

    uint16_t old_used = vq->used_idx;
    unsigned budget = 0;
    for (; budget < 64; budget++) {
        virtio_iov_t iov[VSOCK_MAX_IOV];
        uint16_t head = 0;
        size_t total_iov = 0;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &total_iov) <= 0) {
            break;
        }

        virtio_vsock_hdr_t hdr;
        bool valid_hdr = virtio_iov_read(iov, total_iov, 0, &hdr, sizeof(hdr)) == sizeof(hdr);

        virtio_vsock_accept_cb pending_cb = NULL;
        void *pending_cb_ctx = NULL;
        int pending_client_fd = -1;

        if (valid_hdr && hdr.type == VIRTIO_VSOCK_TYPE_STREAM && hdr.dst_cid == VSOCK_HOST_CID) {
            uint32_t guest_port = hdr.src_port;
            uint32_t host_port = hdr.dst_port;
            vsock_stream_t *s = vsock_find_stream_locked(vs, host_port, guest_port);
            bool live = s && s->state != VSOCK_STATE_CLOSED;

            switch (hdr.op) {
            case VIRTIO_VSOCK_OP_REQUEST: {
                vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
                int sv[2];
                if (s || !lst || (!lst->cb && lst->pending_count >= VSOCK_ACCEPT_BACKLOG) ||
                    vsock_make_socketpair(sv) < 0) {
                    vsock_send_rst_locked(vs, s, host_port, guest_port);
                    break;
                }
                s = vsock_new_stream_locked(vs, host_port, guest_port, NULL, 0);
                if (!s) {
                    close(sv[0]);
                    close(sv[1]);
                    break;
                }
                s->state = VSOCK_STATE_ACCEPTING;
                s->host_fd = sv[0];
                s->peer_buf_alloc = hdr.buf_alloc;
                s->peer_fwd_cnt = hdr.fwd_cnt;
                vsock_queue_ctrl_locked(vs, s, VIRTIO_VSOCK_OP_RESPONSE);

                if (lst->cb) {
                    pending_cb = lst->cb;
                    pending_cb_ctx = lst->ctx;
                    pending_client_fd = sv[1];
                } else {
                    uint32_t tail = (lst->pending_head + lst->pending_count) % VSOCK_ACCEPT_BACKLOG;
                    lst->pending_fds[tail] = sv[1];
                    lst->pending_count++;
                    pthread_cond_broadcast(&vs->cond);
                }
                break;
            }
            case VIRTIO_VSOCK_OP_RESPONSE:
                if (s && s->state == VSOCK_STATE_CONNECTING) {
                    s->state = VSOCK_STATE_ESTABLISHED;
                    s->peer_buf_alloc = hdr.buf_alloc;
                    s->peer_fwd_cnt = hdr.fwd_cnt;
                    s->connect_err = 0;
                    pthread_cond_broadcast(&vs->cond);
                } else {
                    vsock_send_rst_locked(vs, s, host_port, guest_port);
                }
                break;
            case VIRTIO_VSOCK_OP_RST:
                if (s) {
                    vsock_close_stream_locked(vs, s, false, ECONNREFUSED);
                }
                break;
            case VIRTIO_VSOCK_OP_SHUTDOWN:
                if (live) {
                    if (hdr.flags & VIRTIO_VSOCK_SHUTDOWN_RCV) {
                        s->guest_shut_rcv = true;
                    }
                    if (hdr.flags & VIRTIO_VSOCK_SHUTDOWN_SEND) {
                        s->guest_shut_send = true;
                        if (s->g2h_len == 0) {
                            shutdown(s->host_fd, SHUT_WR);
                        }
                    }
                    if (s->g2h_len == 0 && s->guest_shut_send &&
                        (s->guest_shut_rcv || s->host_eof)) {
                        vsock_close_stream_locked(vs, s, true, 0);
                    }
                }
                break;
            case VIRTIO_VSOCK_OP_RW:
                if (!s || s->state != VSOCK_STATE_ESTABLISHED) {
                    vsock_send_rst_locked(vs, s, host_port, guest_port);
                    break;
                }
                s->peer_buf_alloc = hdr.buf_alloc;
                s->peer_fwd_cnt = hdr.fwd_cnt;
                size_t available = virtio_iov_total(iov, total_iov, false) - sizeof(hdr);
                if (hdr.len > available) {
                    vsock_close_stream_locked(vs, s, true, EPROTO);
                } else if (hdr.len > 0) {
                    vsock_append_g2h_locked(vs, s, iov, total_iov, hdr.len);
                }
                break;
            case VIRTIO_VSOCK_OP_CREDIT_UPDATE:
                if (live) {
                    s->peer_buf_alloc = hdr.buf_alloc;
                    s->peer_fwd_cnt = hdr.fwd_cnt;
                }
                break;
            case VIRTIO_VSOCK_OP_CREDIT_REQUEST:
                if (live) {
                    s->peer_buf_alloc = hdr.buf_alloc;
                    s->peer_fwd_cnt = hdr.fwd_cnt;
                    vsock_queue_ctrl_locked(vs, s, VIRTIO_VSOCK_OP_CREDIT_UPDATE);
                } else {
                    vsock_send_rst_locked(vs, s, host_port, guest_port);
                }
                break;
            default:
                break;
            }
        }

        virtqueue_push(dev, vq, head, 0);
        if (pending_cb) {
            if (old_used != vq->used_idx && virtqueue_should_notify(dev, vq, old_used)) {
                virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
            }
            pthread_mutex_unlock(&vs->lock);
            pending_cb(pending_cb_ctx, pending_client_fd);
            pthread_mutex_lock(&vs->lock);
            old_used = vq->used_idx;
        }
    }
    if (old_used != vq->used_idx && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
    if (budget == 64) {
        vsock_wake(vs);
    }
}

static void vsock_read_stream_to_rx_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_RX];
    uint16_t old_used = vq->used_idx;

    for (unsigned burst = 0; burst < 16; burst++) {
        if (s->state != VSOCK_STATE_ESTABLISHED || s->host_eof || s->guest_shut_rcv) {
            break;
        }
        uint32_t credit = vsock_peer_credit(s);
        if (credit == 0) {
            break;
        }

        virtio_iov_t iov[VSOCK_MAX_IOV];
        uint16_t head = 0;
        size_t total_iov = 0;
        uint16_t saved_avail = vq->last_avail_idx;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &total_iov) <= 0) {
            break;
        }
        size_t cap = virtio_iov_total(iov, total_iov, true);
        if (cap <= sizeof(virtio_vsock_hdr_t)) {
            virtqueue_push(dev, vq, head, 0);
            continue;
        }
        size_t max_read = cap - sizeof(virtio_vsock_hdr_t);
        if (max_read > credit) {
            max_read = credit;
        }
        if (max_read > VSOCK_RW_CHUNK) {
            max_read = VSOCK_RW_CHUNK;
        }

        if (s->prefix_off < s->prefix_len) {
            size_t length = s->prefix_len - s->prefix_off;
            if (length > max_read) {
                length = max_read;
            }
            virtio_iov_write(iov, total_iov, sizeof(virtio_vsock_hdr_t), s->prefix + s->prefix_off, length);
            s->prefix_off += length;
            vsock_push_locked(vs, s, head, iov, total_iov, VIRTIO_VSOCK_OP_RW, 0, length);
            continue;
        }

        struct iovec spans[VSOCK_MAX_IOV];
        int used = virtio_iov_spans(iov, total_iov, true, sizeof(virtio_vsock_hdr_t), max_read, spans);
        ssize_t n;
        do {
            n = readv(s->host_fd, spans, used);
        } while (n < 0 && errno == EINTR);

        if (n > 0) {
            vsock_push_locked(vs, s, head, iov, total_iov, VIRTIO_VSOCK_OP_RW, 0, (size_t)n);
            if ((size_t)n < max_read) {
                break;
            }
            continue;
        }

        if (n == 0) {
            s->host_eof = true;
            vsock_push_locked(vs, s, head, iov, total_iov, VIRTIO_VSOCK_OP_SHUTDOWN, VIRTIO_VSOCK_SHUTDOWN_SEND, 0);
            if (s->guest_shut_send && s->g2h_len == 0) {
                vsock_close_stream_locked(vs, s, true, 0);
            }
            break;
        }

        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            vsock_close_stream_locked(vs, s, true, ECONNRESET);
        }
        virtqueue_rewind(dev, vq, saved_avail);
        break;
    }

    if (old_used != vq->used_idx && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static bool vsock_reserve_poll(virtio_vsock_t *vs, size_t capacity) {
    struct pollfd *pfds = malloc(capacity * (sizeof(*pfds) + sizeof(*vs->poll_keys)));
    if (!pfds) {
        return false;
    }
    free(vs->pfds);
    vs->pfds = pfds;
    vs->poll_keys = (vsock_key_t *)(pfds + capacity);
    vs->poll_capacity = capacity;
    return true;
}

static void *vsock_io_thread(void *arg) {
    virtio_vsock_t *vs = arg;

    while (!atomic_load_explicit(&vs->stop, memory_order_acquire)) {
        pthread_mutex_lock(&vs->lock);
        vsock_drain_tx_locked(vs);
        vsock_flush_ctrl_locked(vs);

        if (vs->stream_count >= vs->poll_capacity) {
            vsock_reserve_poll(vs, vs->stream_count * 2);
        }
        bool rx_ready = virtqueue_has_avail(vs->dev, &vs->dev->queues[VSOCK_VQ_RX]);
        nfds_t nfds = 0;
        vs->pfds[nfds++] = (struct pollfd){ .fd = vs->wake_pipe[0], .events = POLLIN, .revents = 0 };

        vsock_stream_t *s;
        TAILQ_FOREACH(s, &vs->streams, entry) {
            if (s->state != VSOCK_STATE_ESTABLISHED) {
                continue;
            }
            if (rx_ready && s->prefix_off < s->prefix_len) {
                vsock_read_stream_to_rx_locked(vs, s);
                rx_ready = virtqueue_has_avail(vs->dev, &vs->dev->queues[VSOCK_VQ_RX]);
                if (s->state != VSOCK_STATE_ESTABLISHED) {
                    continue;
                }
            }
            short ev = 0;
            if (s->g2h_len > 0) {
                ev |= POLLOUT;
            }
            if (rx_ready && !s->host_eof && !s->guest_shut_rcv && vsock_peer_credit(s) > 0) {
                ev |= POLLIN;
            }
            if (ev != 0 && nfds < vs->poll_capacity) {
                vs->poll_keys[nfds] = (vsock_key_t){ .host_port = s->host_port, .guest_port = s->guest_port };
                vs->pfds[nfds++] = (struct pollfd){ .fd = s->host_fd, .events = ev, .revents = 0 };
            }
        }
        pthread_mutex_unlock(&vs->lock);

        int ready = poll(vs->pfds, nfds, -1);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (atomic_load_explicit(&vs->stop, memory_order_acquire)) {
            break;
        }

        if (vs->pfds[0].revents & POLLIN) {
            uint8_t drain[64];
            while (read(vs->wake_pipe[0], drain, sizeof(drain)) > 0) {
            }
            atomic_exchange_explicit(&vs->wake_pending, false, memory_order_acq_rel);
        }

        pthread_mutex_lock(&vs->lock);
        for (nfds_t k = 1; k < nfds; k++) {
            short rev = vs->pfds[k].revents;
            if (rev == 0) {
                continue;
            }
            s = vsock_find_stream_locked(vs, vs->poll_keys[k].host_port, vs->poll_keys[k].guest_port);
            if (!s || s->state != VSOCK_STATE_ESTABLISHED || s->host_fd != vs->pfds[k].fd) {
                continue;
            }
            if (rev & POLLOUT) {
                vsock_drain_g2h_locked(vs, s);
            }
            if (s->state == VSOCK_STATE_ESTABLISHED && (rev & (POLLIN | POLLHUP | POLLERR))) {
                vsock_read_stream_to_rx_locked(vs, s);
            }
        }
        vsock_drain_tx_locked(vs);
        vsock_flush_ctrl_locked(vs);
        pthread_mutex_unlock(&vs->lock);
    }

    return NULL;
}

static uint64_t virtio_vsock_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_vsock_t *vs = dev->priv;
    return virtio_config_read_buf(&vs->config, sizeof(vs->config), offset, size);
}

static void virtio_vsock_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    (void)queue_idx;
    vsock_wake(dev->priv);
}

static void virtio_vsock_reset(virtio_dev_t *dev) {
    virtio_vsock_t *vs = dev->priv;
    pthread_mutex_lock(&vs->lock);
    vsock_stream_t *s;
    vsock_stream_t *next;
    TAILQ_FOREACH_SAFE(s, &vs->streams, entry, next) {
        vsock_close_stream_locked(vs, s, false, ECONNRESET);
    }
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);
}

static void virtio_vsock_destroy(virtio_dev_t *dev) {
    virtio_vsock_t *vs = dev->priv;
    atomic_store_explicit(&vs->stop, true, memory_order_release);
    vsock_wake(vs);
    pthread_mutex_lock(&vs->lock);
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);

    if (vs->thread_started) {
        pthread_join(vs->thread, NULL);
    }

    pthread_mutex_lock(&vs->lock);
    vsock_stream_t *s;
    vsock_stream_t *next;
    TAILQ_FOREACH_SAFE(s, &vs->streams, entry, next) {
        s->connect_waiter = false;
        vsock_close_stream_locked(vs, s, false, ECANCELED);
    }
    for (size_t i = 0; i < vs->listener_count; i++) {
        vsock_listener_t *lst = &vs->listeners[i];
        for (uint32_t k = 0; k < lst->pending_count; k++) {
            close(lst->pending_fds[(lst->pending_head + k) % VSOCK_ACCEPT_BACKLOG]);
        }
    }
    pthread_mutex_unlock(&vs->lock);

    close(vs->wake_pipe[0]);
    close(vs->wake_pipe[1]);
    pthread_cond_destroy(&vs->cond);
    pthread_mutex_destroy(&vs->lock);
    free(vs->listeners);
    free(vs->buckets);
    free(vs->pfds);
    free(vs);
}

static const virtio_dev_ops_t virtio_vsock_ops = {
    .read_config = virtio_vsock_read_config,
    .queue_notify = virtio_vsock_queue_notify,
    .reset = virtio_vsock_reset,
    .destroy = virtio_vsock_destroy,
};

virtio_dev_t *virtio_vsock_create(vmm_machine_t *vm, uint32_t slot, uint64_t guest_cid) {
    virtio_vsock_t *vs = calloc(1, sizeof(*vs));
    if (!vs) {
        return NULL;
    }
    vs->config.guest_cid = guest_cid;
    vs->next_ephemeral_port = VSOCK_EPHEMERAL_PORT;
    TAILQ_INIT(&vs->streams);
    TAILQ_INIT(&vs->ctrl);

    if (pipe(vs->wake_pipe) < 0) {
        free(vs);
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        fcntl(vs->wake_pipe[i], F_SETFD, FD_CLOEXEC);
        fcntl(vs->wake_pipe[i], F_SETFL, O_NONBLOCK);
    }
    pthread_mutex_init(&vs->lock, NULL);
    pthread_cond_init(&vs->cond, NULL);

    const uint32_t qsizes[3] = {
        [VSOCK_VQ_RX] = VSOCK_QUEUE_SIZE,
        [VSOCK_VQ_TX] = VSOCK_QUEUE_SIZE,
        [VSOCK_VQ_EVENT] = VSOCK_EVENT_QUEUE_SIZE,
    };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_VSOCK, 3, qsizes, &virtio_vsock_ops, vs);
    if (!dev) {
        close(vs->wake_pipe[0]);
        close(vs->wake_pipe[1]);
        pthread_cond_destroy(&vs->cond);
        pthread_mutex_destroy(&vs->lock);
        free(vs);
        return NULL;
    }
    vs->dev = dev;

    vs->bucket_count = VSOCK_INITIAL_CAPACITY;
    vs->buckets = calloc(vs->bucket_count, sizeof(*vs->buckets));
    if (!vs->buckets || !vsock_reserve_poll(vs, VSOCK_INITIAL_CAPACITY) ||
        pthread_create(&vs->thread, NULL, vsock_io_thread, vs) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    vs->thread_started = true;
    return dev;
}

static uint32_t vsock_alloc_port_locked(virtio_vsock_t *vs, uint32_t guest_port) {
    for (;;) {
        uint32_t port = vs->next_ephemeral_port++;
        if (vs->next_ephemeral_port == UINT32_MAX) {
            vs->next_ephemeral_port = VSOCK_EPHEMERAL_PORT;
        }
        if (!vsock_find_stream_locked(vs, port, guest_port)) {
            return port;
        }
    }
}

int virtio_vsock_bridge_fd(virtio_dev_t *vsock, uint32_t guest_port, int peer_fd,
                           const void *prefix, size_t prefix_length) {
    if (peer_fd < 0) {
        errno = EINVAL;
        return -1;
    }
    int one = 1;
    setsockopt(peer_fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    int fl = fcntl(peer_fd, F_GETFL, 0);
    if (fl >= 0) {
        fcntl(peer_fd, F_SETFL, fl | O_NONBLOCK);
    }
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);
    vsock_stream_t *s = vsock_new_stream_locked(vs, vsock_alloc_port_locked(vs, guest_port), guest_port,
                                                prefix, prefix_length);
    if (!s) {
        pthread_mutex_unlock(&vs->lock);
        errno = ENOMEM;
        return -1;
    }
    s->state = VSOCK_STATE_CONNECTING;
    s->host_fd = peer_fd;
    s->connect_waiter = true;
    vsock_queue_ctrl_locked(vs, s, VIRTIO_VSOCK_OP_REQUEST);
    vsock_wake(vs);

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 10;

    int rc = 0;
    while (s->state == VSOCK_STATE_CONNECTING &&
           !atomic_load_explicit(&vs->stop, memory_order_acquire) &&
           !atomic_load_explicit(&vs->halted, memory_order_acquire)) {
        rc = pthread_cond_timedwait(&vs->cond, &vs->lock, &deadline);
        if (rc == ETIMEDOUT) {
            break;
        }
    }

    if (s->state == VSOCK_STATE_ESTABLISHED) {
        s->connect_waiter = false;
        pthread_mutex_unlock(&vs->lock);
        return 0;
    }

    int err = (rc == ETIMEDOUT) ? ETIMEDOUT : (s->connect_err ? s->connect_err : ECONNREFUSED);
    s->connect_waiter = false;
    if (s->state == VSOCK_STATE_CONNECTING) {
        s->host_fd = -1;
        vsock_close_stream_locked(vs, s, true, err);
        vsock_wake(vs);
    } else if (s->ctrl_op == VIRTIO_VSOCK_OP_INVALID) {
        vsock_free_stream_locked(vs, s);
    }
    pthread_mutex_unlock(&vs->lock);
    errno = err;
    return -1;
}

int virtio_vsock_listen_host(virtio_dev_t *vsock, uint32_t host_port,
                             virtio_vsock_accept_cb cb, void *ctx) {
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);
    if (vsock_find_listener_locked(vs, host_port)) {
        pthread_mutex_unlock(&vs->lock);
        errno = EADDRINUSE;
        return -1;
    }
    vsock_listener_t *lst = vsock_add_listener_locked(vs, host_port, cb, ctx);
    pthread_mutex_unlock(&vs->lock);
    if (!lst) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

void virtio_vsock_unlisten_host(virtio_dev_t *vsock, uint32_t host_port) {
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);
    vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
    if (lst) {
        *lst = vs->listeners[--vs->listener_count];
        pthread_cond_broadcast(&vs->cond);
    }
    pthread_mutex_unlock(&vs->lock);
}

int virtio_vsock_accept_control(virtio_dev_t *vsock, uint32_t host_port, int timeout_ms) {
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
    int rc = 0;
    while (lst && lst->pending_count == 0 &&
           !atomic_load_explicit(&vs->stop, memory_order_acquire) &&
           !atomic_load_explicit(&vs->halted, memory_order_acquire)) {
        rc = pthread_cond_timedwait(&vs->cond, &vs->lock, &deadline);
        lst = vsock_find_listener_locked(vs, host_port);
        if (rc == ETIMEDOUT) {
            break;
        }
    }

    if (lst && lst->pending_count > 0) {
        int fd = lst->pending_fds[lst->pending_head];
        lst->pending_head = (lst->pending_head + 1) % VSOCK_ACCEPT_BACKLOG;
        lst->pending_count--;
        pthread_mutex_unlock(&vs->lock);
        return fd;
    }

    pthread_mutex_unlock(&vs->lock);
    errno = (rc == ETIMEDOUT) ? ETIMEDOUT : ECANCELED;
    return -1;
}

void virtio_vsock_notify_halt(virtio_dev_t *vsock) {
    virtio_vsock_t *vs = vsock->priv;
    atomic_store_explicit(&vs->halted, true, memory_order_release);
    pthread_mutex_lock(&vs->lock);
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);
}
