#include "virtio_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
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
    VSOCK_MAX_STREAMS = 256,
    VSOCK_MAX_LISTENERS = 64,
    VSOCK_MAX_CTRL_QUEUE = 512,
    VSOCK_ACCEPT_BACKLOG = 32,
    VSOCK_RW_CHUNK = 65536,
    VSOCK_MAX_PREFIX = 64,
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
    VSOCK_STATE_FREE = 0,
    VSOCK_STATE_CONNECTING = 1,
    VSOCK_STATE_ESTABLISHED = 2,
    VSOCK_STATE_CLOSED = 3,
} vsock_stream_state_t;

typedef struct vsock_stream {
    vsock_stream_state_t state;
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

    uint32_t buf_alloc;
    uint32_t fwd_cnt;
    uint32_t last_fwd_sent;

    uint8_t *g2h_buf;
    size_t g2h_head;
    size_t g2h_len;

    uint8_t prefix[VSOCK_MAX_PREFIX];
    size_t prefix_len;
} vsock_stream_t;

typedef struct vsock_listener {
    bool active;
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

    vsock_stream_t streams[VSOCK_MAX_STREAMS];
    vsock_listener_t listeners[VSOCK_MAX_LISTENERS];

    virtio_vsock_hdr_t ctrl_q[VSOCK_MAX_CTRL_QUEUE];
    uint32_t ctrl_head;
    uint32_t ctrl_tail;
    uint32_t ctrl_count;
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

static void vsock_enqueue_ctrl_locked(virtio_vsock_t *vs, uint32_t host_port, uint32_t guest_port,
                                      uint16_t op, uint32_t flags, uint32_t buf_alloc, uint32_t fwd_cnt) {
    if (vs->ctrl_count >= VSOCK_MAX_CTRL_QUEUE) {
        return;
    }
    vs->ctrl_q[vs->ctrl_tail] = (virtio_vsock_hdr_t){
        .src_cid = VSOCK_HOST_CID,
        .dst_cid = vs->config.guest_cid,
        .src_port = host_port,
        .dst_port = guest_port,
        .len = 0,
        .type = VIRTIO_VSOCK_TYPE_STREAM,
        .op = op,
        .flags = flags,
        .buf_alloc = buf_alloc,
        .fwd_cnt = fwd_cnt,
    };
    vs->ctrl_tail = (vs->ctrl_tail + 1) % VSOCK_MAX_CTRL_QUEUE;
    vs->ctrl_count++;
}

static vsock_stream_t *vsock_find_stream_locked(virtio_vsock_t *vs, uint32_t host_port, uint32_t guest_port) {
    for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
        vsock_stream_t *s = &vs->streams[i];
        if ((s->state == VSOCK_STATE_CONNECTING || s->state == VSOCK_STATE_ESTABLISHED) &&
            s->host_port == host_port && s->guest_port == guest_port) {
            return s;
        }
    }
    return NULL;
}

static vsock_stream_t *vsock_alloc_stream_locked(virtio_vsock_t *vs) {
    for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
        if (vs->streams[i].state == VSOCK_STATE_FREE) {
            uint8_t *buf = malloc(VSOCK_BUF_ALLOC);
            if (!buf) {
                return NULL;
            }
            memset(&vs->streams[i], 0, sizeof(vs->streams[i]));
            vs->streams[i].host_fd = -1;
            vs->streams[i].buf_alloc = VSOCK_BUF_ALLOC;
            vs->streams[i].g2h_buf = buf;
            return &vs->streams[i];
        }
    }
    return NULL;
}

static void vsock_close_stream_locked(virtio_vsock_t *vs, vsock_stream_t *s, bool send_rst, int err) {
    if (s->state == VSOCK_STATE_FREE) {
        return;
    }
    if (send_rst) {
        vsock_enqueue_ctrl_locked(vs, s->host_port, s->guest_port, VIRTIO_VSOCK_OP_RST,
                                  0, s->buf_alloc, s->fwd_cnt);
    }
    if (s->host_fd >= 0 && !s->connect_waiter) {
        close(s->host_fd);
    }
    s->host_fd = -1;
    free(s->g2h_buf);
    s->g2h_buf = NULL;
    s->g2h_len = 0;
    s->g2h_head = 0;

    if (s->connect_waiter) {
        s->connect_err = err ? err : ECONNREFUSED;
        s->state = VSOCK_STATE_CLOSED;
        pthread_cond_broadcast(&vs->cond);
    } else {
        s->state = VSOCK_STATE_FREE;
    }
}

static vsock_listener_t *vsock_find_listener_locked(virtio_vsock_t *vs, uint32_t host_port) {
    for (size_t i = 0; i < VSOCK_MAX_LISTENERS; i++) {
        if (vs->listeners[i].active && vs->listeners[i].host_port == host_port) {
            return &vs->listeners[i];
        }
    }
    return NULL;
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
    uint32_t delta = s->fwd_cnt - s->last_fwd_sent;
    if (delta >= s->buf_alloc / 4) {
        s->last_fwd_sent = s->fwd_cnt;
        vsock_enqueue_ctrl_locked(vs, s->host_port, s->guest_port, VIRTIO_VSOCK_OP_CREDIT_UPDATE,
                                  0, s->buf_alloc, s->fwd_cnt);
    }
}

static void vsock_drain_g2h_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    while (s->g2h_len > 0 && s->host_fd >= 0) {
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
    if (s->g2h_len == 0 && s->guest_shut_send && s->host_fd >= 0) {
        shutdown(s->host_fd, SHUT_WR);
        if (s->host_eof || s->guest_shut_rcv) {
            vsock_close_stream_locked(vs, s, true, 0);
        }
    }
}

static int vsock_payload_iov(const virtio_iov_t *iov, size_t count, bool writable,
                             size_t length, struct iovec spans[VSOCK_MAX_IOV]) {
    size_t skip = sizeof(virtio_vsock_hdr_t);
    int used = 0;
    for (size_t i = 0; i < count && length > 0; i++) {
        if (iov[i].write != writable) {
            continue;
        }
        if (skip >= iov[i].len) {
            skip -= iov[i].len;
            continue;
        }
        size_t span = iov[i].len - skip;
        if (span > length) {
            span = length;
        }
        spans[used++] = (struct iovec){.iov_base = (uint8_t *)iov[i].base + skip, .iov_len = span};
        length -= span;
        skip = 0;
    }
    return used;
}

static void vsock_append_g2h_locked(virtio_vsock_t *vs, vsock_stream_t *s,
                                    const virtio_iov_t *iov, size_t count, size_t len) {
    size_t off = 0;
    if (s->g2h_len == 0 && s->host_fd >= 0) {
        struct iovec spans[VSOCK_MAX_IOV];
        int used = vsock_payload_iov(iov, count, false, len, spans);
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

static bool vsock_flush_ctrl_locked(virtio_vsock_t *vs) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_RX];
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
        return false;
    }
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

    while (vs->ctrl_count > 0) {
        virtio_iov_t iov[VSOCK_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }
        virtio_vsock_hdr_t hdr = vs->ctrl_q[vs->ctrl_head];
        vs->ctrl_head = (vs->ctrl_head + 1) % VSOCK_MAX_CTRL_QUEUE;
        vs->ctrl_count--;

        virtio_iov_write(iov, out_cnt + in_cnt, 0, &hdr, sizeof(hdr));
        virtqueue_push(dev, vq, head, (uint32_t)sizeof(hdr));
        pushed = true;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
    return pushed;
}

static void vsock_drain_tx_locked(virtio_vsock_t *vs) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_TX];
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
        return;
    }

    uint16_t old_used = vq->used_idx;
    unsigned budget = 0;
    for (; budget < 64; budget++) {
        virtio_iov_t iov[VSOCK_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }

        size_t total_iov = out_cnt + in_cnt;
        virtio_vsock_hdr_t hdr;
        bool valid_hdr = virtio_iov_read(iov, total_iov, 0, &hdr, sizeof(hdr)) == sizeof(hdr);

        virtio_vsock_accept_cb pending_cb = NULL;
        void *pending_cb_ctx = NULL;
        int pending_client_fd = -1;
        uint32_t pending_guest_port = 0;
        uint32_t pending_host_port = 0;

        if (valid_hdr && hdr.type == VIRTIO_VSOCK_TYPE_STREAM && hdr.dst_cid == VSOCK_HOST_CID) {
            uint32_t guest_port = hdr.src_port;
            uint32_t host_port = hdr.dst_port;
            vsock_stream_t *s = vsock_find_stream_locked(vs, host_port, guest_port);

            switch (hdr.op) {
            case VIRTIO_VSOCK_OP_REQUEST: {
                vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
                if (s || !lst || (!lst->cb && lst->pending_count >= VSOCK_ACCEPT_BACKLOG)) {
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
                    break;
                }
                int sv[2];
                if (vsock_make_socketpair(sv) < 0) {
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
                    break;
                }
                s = vsock_alloc_stream_locked(vs);
                if (!s) {
                    close(sv[0]);
                    close(sv[1]);
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
                    break;
                }
                s->state = VSOCK_STATE_ESTABLISHED;
                s->host_port = host_port;
                s->guest_port = guest_port;
                s->host_fd = sv[0];
                s->peer_buf_alloc = hdr.buf_alloc;
                s->peer_fwd_cnt = hdr.fwd_cnt;

                vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RESPONSE,
                                          0, s->buf_alloc, s->fwd_cnt);
                if (lst->cb) {
                    pending_cb = lst->cb;
                    pending_cb_ctx = lst->ctx;
                    pending_client_fd = sv[1];
                    pending_guest_port = guest_port;
                    pending_host_port = host_port;
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
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
                }
                break;
            case VIRTIO_VSOCK_OP_RST:
                if (s) {
                    vsock_close_stream_locked(vs, s, false, ECONNREFUSED);
                }
                break;
            case VIRTIO_VSOCK_OP_SHUTDOWN:
                if (s) {
                    if (hdr.flags & VIRTIO_VSOCK_SHUTDOWN_RCV) {
                        s->guest_shut_rcv = true;
                    }
                    if (hdr.flags & VIRTIO_VSOCK_SHUTDOWN_SEND) {
                        s->guest_shut_send = true;
                        if (s->g2h_len == 0 && s->host_fd >= 0) {
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
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
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
                if (s) {
                    s->peer_buf_alloc = hdr.buf_alloc;
                    s->peer_fwd_cnt = hdr.fwd_cnt;
                }
                break;
            case VIRTIO_VSOCK_OP_CREDIT_REQUEST:
                if (s) {
                    s->peer_buf_alloc = hdr.buf_alloc;
                    s->peer_fwd_cnt = hdr.fwd_cnt;
                    s->last_fwd_sent = s->fwd_cnt;
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_CREDIT_UPDATE,
                                              0, s->buf_alloc, s->fwd_cnt);
                } else {
                    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_RST, 0, 0, 0);
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
            pending_cb(pending_cb_ctx, pending_client_fd, pending_guest_port, pending_host_port);
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

static bool vsock_rx_has_avail_locked(virtio_vsock_t *vs) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_RX];
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        vq->num == 0 ||
        !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
        return false;
    }
    size_t avail_sz = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
    const vring_avail_t *avail = mmu_gpa_to_hva(dev->vm, vq->avail_gpa, avail_sz);
    if (!avail) {
        return false;
    }
    atomic_thread_fence(memory_order_seq_cst);
    uint16_t avail_idx = atomic_load_explicit((_Atomic uint16_t *)&avail->idx, memory_order_acquire);
    return avail_idx != vq->last_avail_idx;
}

static void vsock_read_stream_to_rx_locked(virtio_vsock_t *vs, vsock_stream_t *s) {
    virtio_dev_t *dev = vs->dev;
    virtqueue_t *vq = &dev->queues[VSOCK_VQ_RX];
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

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
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        uint16_t saved_avail = vq->last_avail_idx;
        if (virtqueue_pop(dev, vq, iov, VSOCK_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }
        size_t total_iov = out_cnt + in_cnt;
        size_t cap = virtio_iov_total(iov, total_iov, true);
        if (cap <= sizeof(virtio_vsock_hdr_t)) {
            virtqueue_push(dev, vq, head, 0);
            pushed = true;
            continue;
        }
        size_t max_read = cap - sizeof(virtio_vsock_hdr_t);
        if (max_read > credit) {
            max_read = credit;
        }
        if (max_read > VSOCK_RW_CHUNK) {
            max_read = VSOCK_RW_CHUNK;
        }

        if (s->prefix_len > 0) {
            size_t length = s->prefix_len < max_read ? s->prefix_len : max_read;
            virtio_iov_write(iov, total_iov, sizeof(virtio_vsock_hdr_t), s->prefix, length);
            s->prefix_len -= length;
            memmove(s->prefix, s->prefix + length, s->prefix_len);
            s->tx_cnt += (uint32_t)length;
            s->last_fwd_sent = s->fwd_cnt;
            virtio_vsock_hdr_t hdr = {
                .src_cid = VSOCK_HOST_CID,
                .dst_cid = vs->config.guest_cid,
                .src_port = s->host_port,
                .dst_port = s->guest_port,
                .len = (uint32_t)length,
                .type = VIRTIO_VSOCK_TYPE_STREAM,
                .op = VIRTIO_VSOCK_OP_RW,
                .flags = 0,
                .buf_alloc = s->buf_alloc,
                .fwd_cnt = s->fwd_cnt,
            };
            virtio_iov_write(iov, total_iov, 0, &hdr, sizeof(hdr));
            virtqueue_push(dev, vq, head, (uint32_t)(sizeof(hdr) + length));
            pushed = true;
            continue;
        }

        struct iovec spans[VSOCK_MAX_IOV];
        int used = vsock_payload_iov(iov, total_iov, true, max_read, spans);
        ssize_t n;
        do {
            n = readv(s->host_fd, spans, used);
        } while (n < 0 && errno == EINTR);

        if (n > 0) {
            s->tx_cnt += (uint32_t)n;
            s->last_fwd_sent = s->fwd_cnt;
            virtio_vsock_hdr_t hdr = {
                .src_cid = VSOCK_HOST_CID,
                .dst_cid = vs->config.guest_cid,
                .src_port = s->host_port,
                .dst_port = s->guest_port,
                .len = (uint32_t)n,
                .type = VIRTIO_VSOCK_TYPE_STREAM,
                .op = VIRTIO_VSOCK_OP_RW,
                .flags = 0,
                .buf_alloc = s->buf_alloc,
                .fwd_cnt = s->fwd_cnt,
            };
            virtio_iov_write(iov, total_iov, 0, &hdr, sizeof(hdr));
            virtqueue_push(dev, vq, head, (uint32_t)(sizeof(hdr) + (size_t)n));
            pushed = true;
            if ((size_t)n < max_read) {
                break;
            }
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            virtqueue_rewind(dev, vq, saved_avail);
            break;
        }

        if (n == 0) {
            s->host_eof = true;
            virtio_vsock_hdr_t hdr = {
                .src_cid = VSOCK_HOST_CID,
                .dst_cid = vs->config.guest_cid,
                .src_port = s->host_port,
                .dst_port = s->guest_port,
                .len = 0,
                .type = VIRTIO_VSOCK_TYPE_STREAM,
                .op = VIRTIO_VSOCK_OP_SHUTDOWN,
                .flags = VIRTIO_VSOCK_SHUTDOWN_SEND,
                .buf_alloc = s->buf_alloc,
                .fwd_cnt = s->fwd_cnt,
            };
            virtio_iov_write(iov, total_iov, 0, &hdr, sizeof(hdr));
            virtqueue_push(dev, vq, head, (uint32_t)sizeof(hdr));
            pushed = true;
            if (s->guest_shut_send && s->g2h_len == 0) {
                vsock_close_stream_locked(vs, s, true, 0);
            }
            break;
        }

        virtio_vsock_hdr_t hdr = {
            .src_cid = VSOCK_HOST_CID,
            .dst_cid = vs->config.guest_cid,
            .src_port = s->host_port,
            .dst_port = s->guest_port,
            .len = 0,
            .type = VIRTIO_VSOCK_TYPE_STREAM,
            .op = VIRTIO_VSOCK_OP_RST,
            .flags = 0,
            .buf_alloc = s->buf_alloc,
            .fwd_cnt = s->fwd_cnt,
        };
        virtio_iov_write(iov, total_iov, 0, &hdr, sizeof(hdr));
        virtqueue_push(dev, vq, head, (uint32_t)sizeof(hdr));
        pushed = true;
        vsock_close_stream_locked(vs, s, false, ECONNRESET);
        break;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static void *vsock_io_thread(void *arg) {
    virtio_vsock_t *vs = arg;
    struct pollfd pfds[1 + VSOCK_MAX_STREAMS];
    size_t stream_map[VSOCK_MAX_STREAMS];

    while (!atomic_load_explicit(&vs->stop, memory_order_acquire)) {
        pthread_mutex_lock(&vs->lock);
        vsock_drain_tx_locked(vs);
        vsock_flush_ctrl_locked(vs);

        bool rx_ready = vsock_rx_has_avail_locked(vs);
        if (rx_ready) {
            for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
                if (vs->streams[i].state == VSOCK_STATE_ESTABLISHED && vs->streams[i].prefix_len > 0) {
                    vsock_read_stream_to_rx_locked(vs, &vs->streams[i]);
                }
            }
            rx_ready = vsock_rx_has_avail_locked(vs);
        }
        nfds_t nfds = 0;
        pfds[nfds++] = (struct pollfd){ .fd = vs->wake_pipe[0], .events = POLLIN, .revents = 0 };

        for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
            vsock_stream_t *s = &vs->streams[i];
            if (s->state != VSOCK_STATE_ESTABLISHED || s->host_fd < 0) {
                continue;
            }
            short ev = 0;
            if (s->g2h_len > 0) {
                ev |= POLLOUT;
            }
            if (rx_ready && !s->host_eof && !s->guest_shut_rcv && vsock_peer_credit(s) > 0) {
                ev |= POLLIN;
            }
            if (ev != 0) {
                stream_map[nfds - 1] = i;
                pfds[nfds++] = (struct pollfd){ .fd = s->host_fd, .events = ev, .revents = 0 };
            }
        }
        pthread_mutex_unlock(&vs->lock);

        int ready = poll(pfds, nfds, -1);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (atomic_load_explicit(&vs->stop, memory_order_acquire)) {
            break;
        }

        if (pfds[0].revents & POLLIN) {
            uint8_t drain[64];
            while (read(vs->wake_pipe[0], drain, sizeof(drain)) > 0) {
            }
            atomic_exchange_explicit(&vs->wake_pending, false, memory_order_acq_rel);
        }

        pthread_mutex_lock(&vs->lock);
        for (nfds_t k = 1; k < nfds; k++) {
            short rev = pfds[k].revents;
            if (rev == 0) {
                continue;
            }
            vsock_stream_t *s = &vs->streams[stream_map[k - 1]];
            if (s->state != VSOCK_STATE_ESTABLISHED || s->host_fd != pfds[k].fd) {
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
    if (queue_idx >= dev->num_queues) {
        return;
    }
    vsock_wake(dev->priv);
}

static void virtio_vsock_reset(virtio_dev_t *dev) {
    virtio_vsock_t *vs = dev->priv;
    pthread_mutex_lock(&vs->lock);
    for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
        vsock_close_stream_locked(vs, &vs->streams[i], false, ECONNRESET);
    }
    vs->ctrl_head = 0;
    vs->ctrl_tail = 0;
    vs->ctrl_count = 0;
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);
}

static void virtio_vsock_destroy(virtio_dev_t *dev) {
    virtio_vsock_t *vs = dev->priv;
    if (!vs) {
        return;
    }
    atomic_store_explicit(&vs->stop, true, memory_order_release);
    if (vs->wake_pipe[1] >= 0) {
        uint8_t b = 1;
        ssize_t r = write(vs->wake_pipe[1], &b, 1);
        (void)r;
    }
    pthread_mutex_lock(&vs->lock);
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);

    if (vs->thread_started) {
        pthread_join(vs->thread, NULL);
    }

    pthread_mutex_lock(&vs->lock);
    for (size_t i = 0; i < VSOCK_MAX_STREAMS; i++) {
        vs->streams[i].connect_waiter = false;
        vsock_close_stream_locked(vs, &vs->streams[i], false, ECANCELED);
    }
    for (size_t i = 0; i < VSOCK_MAX_LISTENERS; i++) {
        vsock_listener_t *lst = &vs->listeners[i];
        while (lst->pending_count > 0) {
            close(lst->pending_fds[lst->pending_head]);
            lst->pending_head = (lst->pending_head + 1) % VSOCK_ACCEPT_BACKLOG;
            lst->pending_count--;
        }
        lst->active = false;
    }
    pthread_mutex_unlock(&vs->lock);

    if (vs->wake_pipe[0] >= 0) {
        close(vs->wake_pipe[0]);
    }
    if (vs->wake_pipe[1] >= 0) {
        close(vs->wake_pipe[1]);
    }
    pthread_cond_destroy(&vs->cond);
    pthread_mutex_destroy(&vs->lock);
    free(vs);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_vsock_ops = {
    .read_config = virtio_vsock_read_config,
    .queue_notify = virtio_vsock_queue_notify,
    .reset = virtio_vsock_reset,
    .destroy = virtio_vsock_destroy,
};

virtio_dev_t *virtio_vsock_create(vmm_machine_t *vm, uint32_t slot, uint64_t guest_cid) {
    if (!vm || guest_cid <= VSOCK_HOST_CID) {
        return NULL;
    }
    virtio_vsock_t *vs = calloc(1, sizeof(*vs));
    if (!vs) {
        return NULL;
    }
    vs->wake_pipe[0] = -1;
    vs->wake_pipe[1] = -1;
    vs->config.guest_cid = guest_cid;
    vs->next_ephemeral_port = 49152;

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
        VSOCK_QUEUE_SIZE,
        VSOCK_QUEUE_SIZE,
        VSOCK_EVENT_QUEUE_SIZE,
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

    if (pthread_create(&vs->thread, NULL, vsock_io_thread, vs) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    vs->thread_started = true;
    return dev;
}

static uint32_t vsock_alloc_port_locked(virtio_vsock_t *vs, uint32_t guest_port) {
    for (uint32_t tries = 0; tries < 16384; tries++) {
        uint32_t p = vs->next_ephemeral_port++;
        if (vs->next_ephemeral_port < 49152 || vs->next_ephemeral_port > 65535) {
            vs->next_ephemeral_port = 49152;
        }
        if (!vsock_find_stream_locked(vs, p, guest_port)) {
            return p;
        }
    }
    return 0;
}

static int vsock_initiate_connect(virtio_vsock_t *vs, uint32_t guest_port, int internal_fd,
                                  const void *prefix, size_t prefix_length) {
    pthread_mutex_lock(&vs->lock);
    uint32_t host_port = vsock_alloc_port_locked(vs, guest_port);
    if (host_port == 0) {
        pthread_mutex_unlock(&vs->lock);
        errno = EADDRNOTAVAIL;
        return -1;
    }
    vsock_stream_t *s = vsock_alloc_stream_locked(vs);
    if (!s) {
        pthread_mutex_unlock(&vs->lock);
        errno = ENOMEM;
        return -1;
    }
    s->state = VSOCK_STATE_CONNECTING;
    s->host_port = host_port;
    s->guest_port = guest_port;
    s->host_fd = internal_fd;
    s->connect_waiter = true;
    s->connect_err = 0;
    if (prefix_length > 0) {
        memcpy(s->prefix, prefix, prefix_length);
    }
    s->prefix_len = prefix_length;

    vsock_enqueue_ctrl_locked(vs, host_port, guest_port, VIRTIO_VSOCK_OP_REQUEST,
                              0, s->buf_alloc, s->fwd_cnt);
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
    } else if (s->state == VSOCK_STATE_CLOSED) {
        s->state = VSOCK_STATE_FREE;
    }
    pthread_mutex_unlock(&vs->lock);
    errno = err;
    return -1;
}

int virtio_vsock_connect_host(virtio_dev_t *vsock, uint32_t guest_port) {
    if (!vsock || !vsock->priv) {
        errno = EINVAL;
        return -1;
    }
    int sv[2];
    if (vsock_make_socketpair(sv) < 0) {
        return -1;
    }
    if (vsock_initiate_connect(vsock->priv, guest_port, sv[0], NULL, 0) < 0) {
        int saved = errno;
        close(sv[0]);
        close(sv[1]);
        errno = saved;
        return -1;
    }
    return sv[1];
}

int virtio_vsock_bridge_fd(virtio_dev_t *vsock, uint32_t guest_port, int peer_fd,
                           const void *prefix, size_t prefix_length) {
    if (!vsock || !vsock->priv || peer_fd < 0) {
        errno = EINVAL;
        return -1;
    }
    int one = 1;
    setsockopt(peer_fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    int fl = fcntl(peer_fd, F_GETFL, 0);
    if (fl >= 0) {
        fcntl(peer_fd, F_SETFL, fl | O_NONBLOCK);
    }
    return vsock_initiate_connect(vsock->priv, guest_port, peer_fd, prefix, prefix_length);
}

int virtio_vsock_listen_host(virtio_dev_t *vsock, uint32_t host_port,
                             virtio_vsock_accept_cb cb, void *ctx) {
    if (!vsock || !vsock->priv) {
        errno = EINVAL;
        return -1;
    }
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);
    if (vsock_find_listener_locked(vs, host_port)) {
        pthread_mutex_unlock(&vs->lock);
        errno = EADDRINUSE;
        return -1;
    }
    for (size_t i = 0; i < VSOCK_MAX_LISTENERS; i++) {
        if (!vs->listeners[i].active) {
            memset(&vs->listeners[i], 0, sizeof(vs->listeners[i]));
            vs->listeners[i].active = true;
            vs->listeners[i].host_port = host_port;
            vs->listeners[i].cb = cb;
            vs->listeners[i].ctx = ctx;
            pthread_mutex_unlock(&vs->lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&vs->lock);
    errno = ENOMEM;
    return -1;
}

void virtio_vsock_unlisten_host(virtio_dev_t *vsock, uint32_t host_port) {
    if (!vsock || !vsock->priv) {
        return;
    }
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);
    vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
    if (lst) {
        while (lst->pending_count > 0) {
            close(lst->pending_fds[lst->pending_head]);
            lst->pending_head = (lst->pending_head + 1) % VSOCK_ACCEPT_BACKLOG;
            lst->pending_count--;
        }
        lst->active = false;
        pthread_cond_broadcast(&vs->cond);
    }
    pthread_mutex_unlock(&vs->lock);
}

int virtio_vsock_accept_control(virtio_dev_t *vsock, uint32_t host_port, int timeout_ms) {
    if (!vsock || !vsock->priv) {
        errno = EINVAL;
        return -1;
    }
    virtio_vsock_t *vs = vsock->priv;
    pthread_mutex_lock(&vs->lock);

    vsock_listener_t *lst = vsock_find_listener_locked(vs, host_port);
    if (!lst) {
        for (size_t i = 0; i < VSOCK_MAX_LISTENERS; i++) {
            if (!vs->listeners[i].active) {
                memset(&vs->listeners[i], 0, sizeof(vs->listeners[i]));
                vs->listeners[i].active = true;
                vs->listeners[i].host_port = host_port;
                lst = &vs->listeners[i];
                break;
            }
        }
    }
    if (!lst) {
        pthread_mutex_unlock(&vs->lock);
        errno = ENOMEM;
        return -1;
    }

    struct timespec deadline = {0};
    if (timeout_ms > 0) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += timeout_ms / 1000;
        deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    int rc = 0;
    while (lst->active && lst->pending_count == 0 &&
           !atomic_load_explicit(&vs->stop, memory_order_acquire) &&
           !atomic_load_explicit(&vs->halted, memory_order_acquire)) {
        if (timeout_ms == 0) {
            rc = ETIMEDOUT;
            break;
        }
        if (timeout_ms < 0) {
            pthread_cond_wait(&vs->cond, &vs->lock);
        } else {
            rc = pthread_cond_timedwait(&vs->cond, &vs->lock, &deadline);
            if (rc == ETIMEDOUT) {
                break;
            }
        }
    }

    if (lst->active && lst->pending_count > 0) {
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
    if (!vsock || !vsock->priv) {
        return;
    }
    virtio_vsock_t *vs = vsock->priv;
    atomic_store_explicit(&vs->halted, true, memory_order_release);
    pthread_mutex_lock(&vs->lock);
    pthread_cond_broadcast(&vs->cond);
    pthread_mutex_unlock(&vs->lock);
}
