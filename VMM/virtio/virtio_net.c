#include "virtio_internal.h"
#include "../host/network.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum {
    VIRTIO_NET_S_LINK_UP = 1,
    VIRTIO_NET_VQ_RX = 0,
    VIRTIO_NET_VQ_TX = 1,
    VIRTIO_NET_QUEUE_SIZE = 256,
    VIRTIO_NET_MAX_IOV = 64,
    VIRTIO_NET_MAX_MRG_BUFS = 64,
    VIRTIO_NET_NUM_BUFFERS_OFFSET = 10,
};

typedef struct __attribute__((packed)) virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} virtio_net_config_t;

typedef struct virtio_net_rx_buf {
    uint16_t head;
    size_t iov_offset;
    size_t iov_count;
    size_t capacity;
} virtio_net_rx_buf_t;

typedef struct virtio_net {
    virtio_dev_t *dev;
    swipterm_network *network;
    virtio_net_config_t config;

    pthread_mutex_t tx_lock;
    virtio_iov_t tx_iov[VIRTIO_NET_MAX_IOV];

    pthread_mutex_t rx_lock;
    uint16_t rx_old_used;
    bool rx_batch_open;
    virtio_net_rx_buf_t rx_bufs[VIRTIO_NET_MAX_MRG_BUFS];
    virtio_iov_t rx_iov[VIRTIO_NET_QUEUE_SIZE];
} virtio_net_t;

static bool net_queue_ready(virtio_dev_t *dev, virtqueue_t *vq) {
    return atomic_load_explicit(&vq->ready, memory_order_acquire) &&
           (atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK);
}

size_t virtio_net_pop_tx(virtio_dev_t *dev, uint8_t *arena, size_t capacity, uint32_t *lengths, size_t max_frames) {
    virtio_net_t *net = dev->priv;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_TX];
    pthread_mutex_lock(&net->tx_lock);
    if (!net_queue_ready(dev, vq)) {
        pthread_mutex_unlock(&net->tx_lock);
        return 0;
    }
    uint16_t old_used = vq->used_idx;
    size_t count = 0;
    size_t offset = 0;
    while (count < max_frames && capacity - offset >= SWIPTERM_NETWORK_MAX_FRAME) {
        uint16_t head = 0;
        size_t out_count = 0;
        size_t in_count = 0;
        if (virtqueue_pop(dev, vq, net->tx_iov, VIRTIO_NET_MAX_IOV, &head, &out_count, &in_count) <= 0) {
            break;
        }
        size_t iov_count = out_count + in_count;
        size_t length = virtio_iov_total(net->tx_iov, iov_count, false);
        if (length > SWIPTERM_NETWORK_HEADER_SIZE && length <= SWIPTERM_NETWORK_MAX_FRAME) {
            virtio_iov_read(net->tx_iov, iov_count, 0, arena + offset, length);
            lengths[count++] = (uint32_t)length;
            offset += length;
        }
        virtqueue_push(dev, vq, head, 0);
    }
    if (vq->used_idx != old_used && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
    pthread_mutex_unlock(&net->tx_lock);
    return count;
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

static int net_deliver_single_locked(virtio_net_t *net, const uint8_t *frame, size_t length) {
    virtio_dev_t *dev = net->dev;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_RX];
    uint16_t saved_avail = vq->last_avail_idx;
    uint16_t head = 0;
    size_t out_count = 0;
    size_t in_count = 0;
    int popped = virtqueue_pop(dev, vq, net->rx_iov, VIRTIO_NET_QUEUE_SIZE, &head, &out_count, &in_count);
    if (popped == 0) {
        return -EAGAIN;
    }
    if (popped < 0) {
        return 0;
    }
    size_t iov_count = out_count + in_count;
    if (virtio_iov_total(net->rx_iov, iov_count, true) < length) {
        virtqueue_rewind(dev, vq, saved_avail);
        return 0;
    }
    virtio_iov_write(net->rx_iov, iov_count, 0, frame, length);
    virtqueue_push(dev, vq, head, (uint32_t)length);
    return 0;
}

static int net_deliver_mergeable_locked(virtio_net_t *net, const uint8_t *frame, size_t length) {
    virtio_dev_t *dev = net->dev;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_RX];
    uint16_t saved_avail = vq->last_avail_idx;
    size_t buffer_count = 0;
    size_t iov_used = 0;
    size_t capacity = 0;
    int popped = 1;
    while (capacity < length && buffer_count < VIRTIO_NET_MAX_MRG_BUFS && iov_used < VIRTIO_NET_QUEUE_SIZE) {
        virtio_net_rx_buf_t *buffer = &net->rx_bufs[buffer_count];
        size_t out_count = 0;
        size_t in_count = 0;
        popped = virtqueue_pop(dev, vq, net->rx_iov + iov_used, VIRTIO_NET_QUEUE_SIZE - iov_used, &buffer->head,
                               &out_count, &in_count);
        if (popped <= 0) {
            break;
        }
        buffer->iov_offset = iov_used;
        buffer->iov_count = out_count + in_count;
        buffer->capacity = virtio_iov_total(net->rx_iov + iov_used, buffer->iov_count, true);
        iov_used += buffer->iov_count;
        capacity += buffer->capacity;
        buffer_count++;
    }
    if (capacity < length || buffer_count == 0 ||
        net->rx_bufs[0].capacity < SWIPTERM_NETWORK_HEADER_SIZE) {
        virtqueue_rewind(dev, vq, saved_avail);
        return popped == 0 ? -EAGAIN : 0;
    }

    uint8_t header[SWIPTERM_NETWORK_HEADER_SIZE];
    memcpy(header, frame, sizeof(header));
    header[VIRTIO_NET_NUM_BUFFERS_OFFSET] = (uint8_t)buffer_count;
    header[VIRTIO_NET_NUM_BUFFERS_OFFSET + 1] = (uint8_t)(buffer_count >> 8);

    size_t source_offset = 0;
    for (size_t i = 0; i < buffer_count; i++) {
        virtio_net_rx_buf_t *buffer = &net->rx_bufs[i];
        virtio_iov_t *iov = net->rx_iov + buffer->iov_offset;
        size_t written = 0;
        if (i == 0) {
            written = virtio_iov_write(iov, buffer->iov_count, 0, header, sizeof(header));
            source_offset = sizeof(header);
        }
        size_t chunk = buffer->capacity - written;
        if (chunk > length - source_offset) {
            chunk = length - source_offset;
        }
        written += virtio_iov_write(iov, buffer->iov_count, written, frame + source_offset, chunk);
        source_offset += chunk;
        virtqueue_push(dev, vq, buffer->head, (uint32_t)written);
    }
    return 0;
}

int virtio_net_push_rx(virtio_dev_t *dev, const uint8_t *frame, size_t length, bool flush) {
    virtio_net_t *net = dev->priv;
    virtqueue_t *vq = &dev->queues[VIRTIO_NET_VQ_RX];
    pthread_mutex_lock(&net->rx_lock);
    if (!net_queue_ready(dev, vq)) {
        net->rx_batch_open = false;
        pthread_mutex_unlock(&net->rx_lock);
        return 0;
    }
    if (!net->rx_batch_open) {
        net->rx_old_used = vq->used_idx;
        net->rx_batch_open = true;
    }
    int rc = 0;
    if (length >= SWIPTERM_NETWORK_HEADER_SIZE && length <= SWIPTERM_NETWORK_MAX_FRAME) {
        bool mergeable = (dev->driver_features & SWIPTERM_NETWORK_F_MRG_RXBUF) != 0;
        do {
            rc = mergeable ? net_deliver_mergeable_locked(net, frame, length)
                           : net_deliver_single_locked(net, frame, length);
        } while (rc == -EAGAIN && virtqueue_request_avail(dev, vq));
    }
    if (flush || rc == -EAGAIN) {
        net_notify_rx_locked(net);
    }
    pthread_mutex_unlock(&net->rx_lock);
    return rc;
}

uint64_t virtio_net_features(virtio_dev_t *dev) {
    if (!(atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_FEATURES_OK)) {
        return 0;
    }
    return dev->driver_features;
}

static uint64_t virtio_net_get_features(virtio_dev_t *dev) {
    (void)dev;
    return SWIPTERM_NETWORK_F_CSUM | SWIPTERM_NETWORK_F_GUEST_CSUM | SWIPTERM_NETWORK_F_MTU |
           SWIPTERM_NETWORK_F_MAC | SWIPTERM_NETWORK_F_HOST_TSO4 | SWIPTERM_NETWORK_F_HOST_TSO6 |
           SWIPTERM_NETWORK_F_HOST_USO | SWIPTERM_NETWORK_F_GUEST_TSO4 | SWIPTERM_NETWORK_F_GUEST_TSO6 |
           SWIPTERM_NETWORK_F_GUEST_USO4 | SWIPTERM_NETWORK_F_GUEST_USO6 | SWIPTERM_NETWORK_F_MRG_RXBUF |
           SWIPTERM_NETWORK_F_STATUS;
}

static void virtio_net_set_features(virtio_dev_t *dev, uint64_t features) {
    (void)features;
    virtio_net_t *net = dev->priv;
    swipterm_network_signal(net->network, SWIPTERM_NETWORK_WAKE_EVENT);
}

static uint64_t virtio_net_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_net_t *net = dev->priv;
    return virtio_config_read_buf(&net->config, sizeof(net->config), offset, size);
}

static void virtio_net_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    virtio_net_t *net = dev->priv;
    if (queue_idx == VIRTIO_NET_VQ_TX) {
        swipterm_network_signal(net->network, SWIPTERM_NETWORK_WAKE_TX);
    } else if (queue_idx == VIRTIO_NET_VQ_RX) {
        swipterm_network_signal(net->network, SWIPTERM_NETWORK_WAKE_RX);
    }
}

static void virtio_net_reset(virtio_dev_t *dev) {
    virtio_net_t *net = dev->priv;
    pthread_mutex_lock(&net->tx_lock);
    pthread_mutex_lock(&net->rx_lock);
    net->rx_batch_open = false;
    pthread_mutex_unlock(&net->rx_lock);
    pthread_mutex_unlock(&net->tx_lock);
    swipterm_network_signal(net->network, SWIPTERM_NETWORK_WAKE_RX);
    swipterm_network_signal(net->network, SWIPTERM_NETWORK_WAKE_EVENT);
}

static void virtio_net_destroy(virtio_dev_t *dev) {
    virtio_net_t *net = dev->priv;
    if (!net) {
        return;
    }
    swipterm_network_detach_device(net->network, dev);
    swipterm_network_release(net->network);
    pthread_mutex_destroy(&net->tx_lock);
    pthread_mutex_destroy(&net->rx_lock);
    free(net);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_net_ops = {
    .get_features = virtio_net_get_features,
    .set_features = virtio_net_set_features,
    .read_config = virtio_net_read_config,
    .queue_notify = virtio_net_queue_notify,
    .reset = virtio_net_reset,
    .destroy = virtio_net_destroy,
};

virtio_dev_t *virtio_net_create(vmm_machine_t *vm, uint32_t slot, const uint8_t mac[6], swipterm_network *network) {
    virtio_net_t *net = calloc(1, sizeof(*net));
    if (!net) {
        return NULL;
    }
    memcpy(net->config.mac, mac, sizeof(net->config.mac));
    net->config.status = VIRTIO_NET_S_LINK_UP;
    net->config.max_virtqueue_pairs = 1;
    net->config.mtu = SWIPTERM_NETWORK_MTU;
    pthread_mutex_init(&net->tx_lock, NULL);
    pthread_mutex_init(&net->rx_lock, NULL);

    const uint32_t queue_sizes[2] = {VIRTIO_NET_QUEUE_SIZE, VIRTIO_NET_QUEUE_SIZE};
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_NET, 2, queue_sizes, &virtio_net_ops, net);
    if (!dev) {
        pthread_mutex_destroy(&net->tx_lock);
        pthread_mutex_destroy(&net->rx_lock);
        free(net);
        return NULL;
    }
    net->dev = dev;
    net->network = network;
    swipterm_network_retain(network);
    swipterm_network_attach_device(network, dev);
    return dev;
}
