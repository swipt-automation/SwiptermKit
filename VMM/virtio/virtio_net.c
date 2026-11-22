#include "virtio_internal.h"
#include "../deps/libslirp/src/libslirp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum : uint64_t {
    VIRTIO_NET_F_CSUM = 1ULL << 0,
    VIRTIO_NET_F_GUEST_CSUM = 1ULL << 1,
    VIRTIO_NET_F_MTU = 1ULL << 3,
    VIRTIO_NET_F_MAC = 1ULL << 5,
    VIRTIO_NET_F_HOST_TSO4 = 1ULL << 11,
    VIRTIO_NET_F_MRG_RXBUF = 1ULL << 15,
    VIRTIO_NET_F_STATUS = 1ULL << 16,
};

enum : uint8_t {
    VIRTIO_NET_HDR_F_NEEDS_CSUM = 1,
    VIRTIO_NET_HDR_F_DATA_VALID = 2,
};

enum : uint8_t {
    VIRTIO_NET_HDR_GSO_NONE = 0,
    VIRTIO_NET_HDR_GSO_TCPV4 = 1,
};

enum {
    VIRTIO_NET_S_LINK_UP = 1,
    VIRTIO_NET_MTU = 16384,
    VIRTIO_NET_VQ_RX = 0,
    VIRTIO_NET_VQ_TX = 1,
    VIRTIO_NET_QUEUE_SIZE = 256,
    VIRTIO_NET_MAX_IOV = 64,
    VIRTIO_NET_MAX_MRG_BUFS = 32,
    VIRTIO_NET_MAX_FRAME = 65550,
    VIRTIO_NET_MAX_POLL_FDS = 4096,
    VIRTIO_NET_RX_BACKLOG = 64,
    VIRTIO_NET_TX_BATCH = 4,
};

typedef struct __attribute__((packed)) virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} virtio_net_config_t;

typedef struct __attribute__((packed)) virtio_net_hdr_v1 {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} virtio_net_hdr_v1_t;

static_assert(sizeof(virtio_net_hdr_v1_t) == 12);

typedef struct net_rx_pkt {
    uint8_t *data;
    size_t len;
} net_rx_pkt_t;

typedef struct net_timer {
    SlirpTimerId id;
    void *cookie;
    int64_t expires_ms;
    bool active;
} net_timer_t;

typedef struct virtio_net {
    virtio_dev_t *dev;
    virtio_net_config_t config;
    Slirp *slirp;
    int wake_pipe[2];
    pthread_t thread;
    bool thread_started;
    pthread_mutex_t vq_lock;
    _Atomic bool stop;
    _Atomic bool wake_pending;

    struct pollfd poll_fds[VIRTIO_NET_MAX_POLL_FDS];
    nfds_t poll_count;
    net_timer_t timers[SLIRP_TIMER_NUM];

    uint8_t tx_buf[VIRTIO_NET_TX_BATCH][sizeof(virtio_net_hdr_v1_t) + VIRTIO_NET_MAX_FRAME];
    uint8_t gso_buf[VIRTIO_NET_MAX_FRAME];

    net_rx_pkt_t rx_backlog[VIRTIO_NET_RX_BACKLOG];
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_count;
    uint16_t rx_old_used;
    bool rx_batch_open;
} virtio_net_t;

static uint16_t net_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t net_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void net_put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void net_put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t net_cksum_acc(const uint8_t *b, size_t n, uint32_t sum) {
    while (n >= 2) {
        sum += net_be16(b);
        b += 2;
        n -= 2;
    }
    if (n) {
        sum += (uint16_t)*b << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }
    return sum;
}

static void net_wake(virtio_net_t *net) {
    if (!atomic_exchange_explicit(&net->wake_pending, true, memory_order_acq_rel)) {
        uint8_t b = 1;
        ssize_t r;
        do {
            r = write(net->wake_pipe[1], &b, 1);
        } while (r < 0 && errno == EINTR);
    }
}

static void net_backlog_clear(virtio_net_t *net) {
    while (net->rx_count > 0) {
        free(net->rx_backlog[net->rx_head].data);
        net->rx_backlog[net->rx_head].data = NULL;
        net->rx_backlog[net->rx_head].len = 0;
        net->rx_head = (net->rx_head + 1) % VIRTIO_NET_RX_BACKLOG;
        net->rx_count--;
    }
}

static bool net_deliver_rx_locked(virtio_net_t *net, const uint8_t *pkt, size_t pkt_len) {
    virtio_dev_t *dev = net->dev;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_RX];
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
        return false;
    }

    virtio_net_hdr_v1_t vh = {0};
    if ((dev->driver_features & VIRTIO_NET_F_GUEST_CSUM) &&
        pkt_len >= 54 && pkt[12] == 0x08 && pkt[13] == 0x00 &&
        pkt[23] == 6 && !(pkt[20] & 0x3F) && !pkt[21]) {
        vh.flags = VIRTIO_NET_HDR_F_DATA_VALID;
    }

    size_t total_needed = sizeof(vh) + pkt_len;
    bool mrg = (dev->driver_features & VIRTIO_NET_F_MRG_RXBUF) != 0;
    if (!net->rx_batch_open) {
        net->rx_old_used = vq->used_idx;
        net->rx_batch_open = true;
    }

    if (!mrg) {
        virtio_iov_t iov[VIRTIO_NET_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VIRTIO_NET_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            return false;
        }
        size_t cap = virtio_iov_total(iov, out_cnt + in_cnt, true);
        if (cap < total_needed) {
            virtqueue_push(dev, vq, head, 0);
            return true;
        }
        virtio_iov_write(iov, out_cnt + in_cnt, 0, &vh, sizeof(vh));
        virtio_iov_write(iov, out_cnt + in_cnt, sizeof(vh), pkt, pkt_len);
        virtqueue_push(dev, vq, head, (uint32_t)total_needed);
        return true;
    }

    typedef struct mrg_buf {
        uint16_t head;
        size_t total_iov;
        size_t cap;
        virtio_iov_t iov[VIRTIO_NET_MAX_IOV];
    } mrg_buf_t;

    mrg_buf_t bufs[VIRTIO_NET_MAX_MRG_BUFS];
    uint16_t saved_avail = vq->last_avail_idx;
    size_t nbufs = 0;
    size_t total_cap = 0;

    while (total_cap < total_needed && nbufs < VIRTIO_NET_MAX_MRG_BUFS) {
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, bufs[nbufs].iov, VIRTIO_NET_MAX_IOV,
                          &bufs[nbufs].head, &out_cnt, &in_cnt) <= 0) {
            break;
        }
        bufs[nbufs].total_iov = out_cnt + in_cnt;
        bufs[nbufs].cap = virtio_iov_total(bufs[nbufs].iov, out_cnt + in_cnt, true);
        total_cap += bufs[nbufs].cap;
        nbufs++;
    }

    if (total_cap < total_needed) {
        virtqueue_rewind(dev, vq, saved_avail);
        return false;
    }

    vh.num_buffers = (uint16_t)nbufs;
    size_t src_off = 0;
    for (size_t i = 0; i < nbufs; i++) {
        size_t dst_off = 0;
        size_t avail = bufs[i].cap;
        if (i == 0) {
            size_t hdr_w = virtio_iov_write(bufs[i].iov, bufs[i].total_iov, 0, &vh, sizeof(vh));
            dst_off += hdr_w;
            avail -= hdr_w;
        }
        size_t rem = pkt_len - src_off;
        size_t chunk = avail < rem ? avail : rem;
        if (chunk > 0) {
            virtio_iov_write(bufs[i].iov, bufs[i].total_iov, dst_off, pkt + src_off, chunk);
            src_off += chunk;
            dst_off += chunk;
        }
        virtqueue_push(dev, vq, bufs[i].head, (uint32_t)dst_off);
    }

    return true;
}

static void net_notify_rx_locked(virtio_net_t *net) {
    virtio_dev_t *dev = net->dev;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_RX];
    if (net->rx_batch_open && net->rx_old_used != vq->used_idx &&
        virtqueue_should_notify(dev, vq, net->rx_old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
    net->rx_batch_open = false;
}

static void net_flush_rx_backlog(virtio_net_t *net) {
    pthread_mutex_lock(&net->vq_lock);
    while (net->rx_count > 0) {
        net_rx_pkt_t *p = &net->rx_backlog[net->rx_head];
        if (!net_deliver_rx_locked(net, p->data, p->len)) {
            break;
        }
        free(p->data);
        p->data = NULL;
        p->len = 0;
        net->rx_head = (net->rx_head + 1) % VIRTIO_NET_RX_BACKLOG;
        net->rx_count--;
    }
    net_notify_rx_locked(net);
    pthread_mutex_unlock(&net->vq_lock);
}

static void net_tx_deliver(virtio_net_t *net, uint8_t *p, size_t n,
                           const virtio_net_hdr_v1_t *h) {
    unsigned start = h->csum_start;
    unsigned off = h->csum_offset;
    unsigned mss = h->gso_size;

    if (h->flags & ~VIRTIO_NET_HDR_F_NEEDS_CSUM) {
        return;
    }

    if (h->gso_type == VIRTIO_NET_HDR_GSO_NONE) {
        if (!(h->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)) {
            slirp_input(net->slirp, p, (int)n);
            return;
        }
        if ((size_t)start + (size_t)off + 2 > n) {
            return;
        }
        uint16_t csum = (uint16_t)~net_cksum_acc(p + start, n - start, 0);
        if (off == 6 && csum == 0) {
            csum = 0xFFFF;
        }
        net_put_be16(p + start + off, csum);
        slirp_input(net->slirp, p, (int)n);
        return;
    }

    if (h->gso_type != VIRTIO_NET_HDR_GSO_TCPV4 || mss == 0 ||
        h->flags != VIRTIO_NET_HDR_F_NEEDS_CSUM || n < 54 ||
        net_be16(p + 12) != 0x0800 || (p[14] >> 4) != 4 || p[23] != 6 ||
        (net_be16(p + 20) & 0x3FFFU) != 0 || off != 16) {
        return;
    }

    size_t ihl = (size_t)(p[14] & 0x0FU) * 4;
    if (ihl < 20 || start != 14 + ihl || start + 20 > n) {
        return;
    }
    size_t thl = (size_t)(p[start + 12] >> 4) * 4;
    size_t hdr = start + thl;
    if (thl < 20 || hdr >= n || hdr - 14 + mss > 65535 || hdr + mss > sizeof(net->gso_buf)) {
        return;
    }

    uint32_t seq = net_be32(p + start + 4);
    uint16_t id = net_be16(p + 18);
    uint8_t *frame = net->gso_buf;

    for (size_t pos = hdr; pos < n; pos += mss) {
        size_t len = (n - pos) < mss ? (n - pos) : mss;
        memcpy(frame, p, hdr);
        memcpy(frame + hdr, p + pos, len);
        net_put_be16(frame + 16, (uint16_t)(hdr + len - 14));
        net_put_be16(frame + 18, id++);
        net_put_be16(frame + 24, 0);
        net_put_be16(frame + 24, (uint16_t)~net_cksum_acc(frame + 14, ihl, 0));
        net_put_be32(frame + start + 4, seq + (uint32_t)(pos - hdr));
        if (pos + len < n) {
            frame[start + 13] &= (uint8_t)~(0x08U | 0x01U);
        }
        if (pos != hdr) {
            frame[start + 13] &= (uint8_t)~0x80U;
        }
        net_put_be16(frame + start + 16, 0);
        uint32_t pseudo = net_cksum_acc(frame + 26, 8, (uint32_t)(6 + thl + len));
        net_put_be16(frame + start + 16, (uint16_t)~net_cksum_acc(frame + start, thl + len, pseudo));
        slirp_input(net->slirp, frame, (int)(hdr + len));
    }
}

static void net_drain_tx(virtio_net_t *net) {
    virtio_dev_t *dev = net->dev;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_TX];

    for (unsigned budget = 0; budget < 64; budget += VIRTIO_NET_TX_BATCH) {
        size_t sizes[VIRTIO_NET_TX_BATCH] = {0};
        pthread_mutex_lock(&net->vq_lock);
        if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
            !(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
            pthread_mutex_unlock(&net->vq_lock);
            return;
        }
        uint16_t old_used = vq->used_idx;
        unsigned count = 0;
        for (; count < VIRTIO_NET_TX_BATCH; count++) {
            virtio_iov_t iov[VIRTIO_NET_MAX_IOV];
            uint16_t head = 0;
            size_t out_cnt = 0;
            size_t in_cnt = 0;
            if (virtqueue_pop(dev, vq, iov, VIRTIO_NET_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
                break;
            }
            size_t total = virtio_iov_total(iov, out_cnt + in_cnt, false);
            if (total >= sizeof(virtio_net_hdr_v1_t) && total <= sizeof(net->tx_buf[count])) {
                virtio_iov_read(iov, out_cnt + in_cnt, 0, net->tx_buf[count], total);
                sizes[count] = total;
            }
            virtqueue_push(dev, vq, head, 0);
        }
        if (count && virtqueue_should_notify(dev, vq, old_used)) {
            virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
        }
        pthread_mutex_unlock(&net->vq_lock);

        for (unsigned i = 0; i < count; i++) {
            if (sizes[i]) {
                virtio_net_hdr_v1_t h;
                memcpy(&h, net->tx_buf[i], sizeof(h));
                net_tx_deliver(net, net->tx_buf[i] + sizeof(h), sizes[i] - sizeof(h), &h);
            }
        }
        pthread_mutex_lock(&net->vq_lock);
        net_notify_rx_locked(net);
        pthread_mutex_unlock(&net->vq_lock);
        if (count < VIRTIO_NET_TX_BATCH) {
            return;
        }
    }
    net_wake(net);
}

static int64_t net_clock_ns(void *opaque) {
    (void)opaque;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * INT64_C(1000000000) + ts.tv_nsec;
}

static slirp_ssize_t net_slirp_send_packet(const void *buf, size_t len, void *opaque) {
    virtio_net_t *net = opaque;
    if (len == 0 || len > VIRTIO_NET_MAX_FRAME) {
        return (slirp_ssize_t)len;
    }

    pthread_mutex_lock(&net->vq_lock);
    if (net->rx_count == 0 && net_deliver_rx_locked(net, buf, len)) {
        pthread_mutex_unlock(&net->vq_lock);
        return (slirp_ssize_t)len;
    }
    if (net->rx_count < VIRTIO_NET_RX_BACKLOG) {
        uint8_t *copy = malloc(len);
        if (copy) {
            memcpy(copy, buf, len);
            net->rx_backlog[net->rx_tail] = (net_rx_pkt_t){ .data = copy, .len = len };
            net->rx_tail = (net->rx_tail + 1) % VIRTIO_NET_RX_BACKLOG;
            net->rx_count++;
        }
    }
    pthread_mutex_unlock(&net->vq_lock);
    return (slirp_ssize_t)len;
}

static void net_slirp_guest_error(const char *msg, void *opaque) {
    (void)msg;
    (void)opaque;
}

static void *net_slirp_timer_new(SlirpTimerId id, void *cb_opaque, void *opaque) {
    virtio_net_t *net = opaque;
    if ((unsigned)id >= SLIRP_TIMER_NUM) {
        return NULL;
    }
    net->timers[id] = (net_timer_t){
        .id = id,
        .cookie = cb_opaque,
        .expires_ms = INT64_MAX,
        .active = true,
    };
    return &net->timers[id];
}

static void net_slirp_timer_free(void *timer, void *opaque) {
    (void)opaque;
    if (timer) {
        net_timer_t *t = timer;
        t->active = false;
        t->expires_ms = INT64_MAX;
    }
}

static void net_slirp_timer_mod(void *timer, int64_t expire_time, void *opaque) {
    (void)opaque;
    if (timer) {
        net_timer_t *t = timer;
        t->expires_ms = expire_time;
    }
}

static void net_slirp_notify(void *opaque) {
    net_wake(opaque);
}

static void net_slirp_reg_socket(slirp_os_socket fd, void *opaque) {
    (void)fd;
    (void)opaque;
}

static int net_slirp_add_poll(slirp_os_socket fd, int events, void *opaque) {
    virtio_net_t *net = opaque;
    if (net->poll_count >= VIRTIO_NET_MAX_POLL_FDS) {
        return -1;
    }
    short mask = (short)(((events & SLIRP_POLL_IN) ? POLLIN : 0) |
                         ((events & SLIRP_POLL_OUT) ? POLLOUT : 0) |
                         ((events & SLIRP_POLL_PRI) ? POLLPRI : 0));
    int idx = (int)net->poll_count++;
    net->poll_fds[idx] = (struct pollfd){ .fd = fd, .events = mask, .revents = 0 };
    return idx;
}

static int net_slirp_get_revents(int idx, void *opaque) {
    virtio_net_t *net = opaque;
    if (idx < 0 || (nfds_t)idx >= net->poll_count) {
        return 0;
    }
    short rev = net->poll_fds[idx].revents;
    return ((rev & POLLIN) ? SLIRP_POLL_IN : 0) |
           ((rev & POLLOUT) ? SLIRP_POLL_OUT : 0) |
           ((rev & POLLPRI) ? SLIRP_POLL_PRI : 0) |
           ((rev & (POLLERR | POLLNVAL)) ? SLIRP_POLL_ERR : 0) |
           ((rev & POLLHUP) ? SLIRP_POLL_HUP : 0);
}

static void *net_reactor_thread(void *arg) {
    virtio_net_t *net = arg;

    while (!atomic_load_explicit(&net->stop, memory_order_acquire)) {
        uint32_t timeout = 500;
        net->poll_count = 1;
        net->poll_fds[0] = (struct pollfd){ .fd = net->wake_pipe[0], .events = POLLIN, .revents = 0 };
        slirp_pollfds_fill_socket(net->slirp, &timeout, net_slirp_add_poll, net);

        int64_t now_ms = net_clock_ns(net) / 1000000;
        for (int i = 0; i < SLIRP_TIMER_NUM; i++) {
            if (net->timers[i].active && net->timers[i].expires_ms != INT64_MAX) {
                int64_t rem = net->timers[i].expires_ms - now_ms;
                if (rem <= 0) {
                    timeout = 0;
                } else if (rem < (int64_t)timeout) {
                    timeout = (uint32_t)rem;
                }
            }
        }

        int poll_ms = timeout > (uint32_t)INT_MAX ? INT_MAX : (int)timeout;
        int ready = poll(net->poll_fds, net->poll_count, poll_ms);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (atomic_load_explicit(&net->stop, memory_order_acquire)) {
            break;
        }

        if (net->poll_fds[0].revents & POLLIN) {
            uint8_t drain[64];
            while (read(net->wake_pipe[0], drain, sizeof(drain)) > 0) {
            }
            atomic_store_explicit(&net->wake_pending, false, memory_order_release);
        }

        slirp_pollfds_poll(net->slirp, ready < 0 ? 1 : 0, net_slirp_get_revents, net);

        now_ms = net_clock_ns(net) / 1000000;
        for (int i = 0; i < SLIRP_TIMER_NUM; i++) {
            if (net->timers[i].active && net->timers[i].expires_ms <= now_ms) {
                net->timers[i].expires_ms = INT64_MAX;
                slirp_handle_timer(net->slirp, net->timers[i].id, net->timers[i].cookie);
            }
        }

        net_flush_rx_backlog(net);
        net_drain_tx(net);
    }

    return NULL;
}

static uint64_t virtio_net_get_features(virtio_dev_t *dev) {
    (void)dev;
    return VIRTIO_NET_F_CSUM | VIRTIO_NET_F_GUEST_CSUM | VIRTIO_NET_F_MTU |
           VIRTIO_NET_F_MAC | VIRTIO_NET_F_HOST_TSO4 | VIRTIO_NET_F_MRG_RXBUF |
           VIRTIO_NET_F_STATUS;
}

static uint64_t virtio_net_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_net_t *net = dev->priv;
    return virtio_config_read_buf(&net->config, sizeof(net->config), offset, size);
}

static void virtio_net_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    if (queue_idx > VIRTIO_NET_VQ_TX) {
        return;
    }
    net_wake(dev->priv);
}

static void virtio_net_reset(virtio_dev_t *dev) {
    virtio_net_t *net = dev->priv;
    pthread_mutex_lock(&net->vq_lock);
    net_backlog_clear(net);
    net->rx_batch_open = false;
    pthread_mutex_unlock(&net->vq_lock);
}

static void virtio_net_destroy(virtio_dev_t *dev) {
    virtio_net_t *net = dev->priv;
    if (!net) {
        return;
    }
    atomic_store_explicit(&net->stop, true, memory_order_release);
    if (net->wake_pipe[1] >= 0) {
        uint8_t b = 1;
        ssize_t r = write(net->wake_pipe[1], &b, 1);
        (void)r;
    }
    if (net->thread_started) {
        pthread_join(net->thread, NULL);
    }
    if (net->slirp) {
        slirp_cleanup(net->slirp);
    }
    net_backlog_clear(net);
    if (net->wake_pipe[0] >= 0) {
        close(net->wake_pipe[0]);
    }
    if (net->wake_pipe[1] >= 0) {
        close(net->wake_pipe[1]);
    }
    pthread_mutex_destroy(&net->vq_lock);
    free(net);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_net_ops = {
    .get_features = virtio_net_get_features,
    .read_config = virtio_net_read_config,
    .queue_notify = virtio_net_queue_notify,
    .reset = virtio_net_reset,
    .destroy = virtio_net_destroy,
};

virtio_dev_t *virtio_net_create(vmm_machine_t *vm, uint32_t slot, const uint8_t mac[6]) {
    if (!vm || !mac) {
        return NULL;
    }
    virtio_net_t *net = calloc(1, sizeof(*net));
    if (!net) {
        return NULL;
    }
    net->wake_pipe[0] = -1;
    net->wake_pipe[1] = -1;
    memcpy(net->config.mac, mac, 6);
    net->config.status = VIRTIO_NET_S_LINK_UP;
    net->config.max_virtqueue_pairs = 1;
    net->config.mtu = VIRTIO_NET_MTU;

    if (pipe(net->wake_pipe) < 0) {
        free(net);
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        fcntl(net->wake_pipe[i], F_SETFD, FD_CLOEXEC);
        fcntl(net->wake_pipe[i], F_SETFL, O_NONBLOCK);
    }
    pthread_mutex_init(&net->vq_lock, NULL);

    SlirpConfig cfg = {
        .version = SLIRP_CONFIG_VERSION_MAX,
        .in_enabled = true,
        .in6_enabled = true,
        .vprefix_len = 64,
        .if_mtu = VIRTIO_NET_MTU,
        .if_mru = VIRTIO_NET_MTU,
        .disable_host_loopback = true,
        .hostfwd_listen_backlog = 32,
    };
    inet_pton(AF_INET, "10.0.2.0", &cfg.vnetwork);
    inet_pton(AF_INET, "255.255.255.0", &cfg.vnetmask);
    inet_pton(AF_INET, "10.0.2.2", &cfg.vhost);
    inet_pton(AF_INET, "10.0.2.15", &cfg.vdhcp_start);
    inet_pton(AF_INET, "10.0.2.3", &cfg.vnameserver);
    inet_pton(AF_INET6, "fd00::", &cfg.vprefix_addr6);
    inet_pton(AF_INET6, "fd00::2", &cfg.vhost6);
    inet_pton(AF_INET6, "fd00::3", &cfg.vnameserver6);

    static const SlirpCb cbs = {
        .send_packet = net_slirp_send_packet,
        .guest_error = net_slirp_guest_error,
        .clock_get_ns = net_clock_ns,
        .timer_new_opaque = net_slirp_timer_new,
        .timer_free = net_slirp_timer_free,
        .timer_mod = net_slirp_timer_mod,
        .notify = net_slirp_notify,
        .register_poll_socket = net_slirp_reg_socket,
        .unregister_poll_socket = net_slirp_reg_socket,
    };

    const uint32_t qsizes[2] = { VIRTIO_NET_QUEUE_SIZE, VIRTIO_NET_QUEUE_SIZE };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_NET, 2, qsizes, &virtio_net_ops, net);
    if (!dev) {
        close(net->wake_pipe[0]);
        close(net->wake_pipe[1]);
        pthread_mutex_destroy(&net->vq_lock);
        free(net);
        return NULL;
    }
    net->dev = dev;

    net->slirp = slirp_new(&cfg, &cbs, net);
    if (!net->slirp) {
        virtio_dev_destroy(dev);
        return NULL;
    }

    if (pthread_create(&net->thread, NULL, net_reactor_thread, net) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    net->thread_started = true;
    return dev;
}
