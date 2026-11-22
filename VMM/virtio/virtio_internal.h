#ifndef VMM_VIRTIO_VIRTIO_INTERNAL_H
#define VMM_VIRTIO_VIRTIO_INTERNAL_H

#include "virtio_mmio.h"
#include "../board/machine.h"
#include "../mmu/mmu.h"

#include <stdlib.h>
#include <string.h>

static inline virtio_dev_t *virtio_dev_alloc(vmm_machine_t *vm, uint32_t slot,
                                             uint32_t device_id, uint32_t num_queues,
                                             const uint32_t *queue_sizes,
                                             const virtio_dev_ops_t *ops, void *priv) {
    virtio_dev_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return NULL;
    }
    dev->queues = calloc(num_queues, sizeof(*dev->queues));
    if (!dev->queues) {
        free(dev);
        return NULL;
    }
    dev->vm = vm;
    dev->slot = slot;
    dev->device_id = device_id;
    dev->vendor_id = VIRTIO_VENDOR_SWIPTERM;
    dev->irq_intid = VMM_IRQ_VIRTIO_BASE + slot;
    dev->num_queues = num_queues;
    pthread_mutex_init(&dev->irq_lock, NULL);
    dev->ops = ops;
    dev->priv = priv;
    for (uint32_t i = 0; i < num_queues; i++) {
        dev->queues[i].index = i;
        dev->queues[i].num_max = queue_sizes[i];
    }
    return dev;
}

static inline uint64_t virtio_config_read_buf(const void *cfg, size_t cfg_size,
                                              uint32_t offset, uint32_t size) {
    if (size == 0 || size > 8 || (size_t)offset >= cfg_size) {
        return 0;
    }
    size_t avail = cfg_size - (size_t)offset;
    size_t n = size < avail ? size : avail;
    uint64_t val = 0;
    memcpy(&val, (const uint8_t *)cfg + offset, n);
    return val;
}

static inline size_t virtio_iov_read(const virtio_iov_t *iov, size_t iov_cnt,
                                     size_t skip, void *dst, size_t len) {
    uint8_t *out = dst;
    size_t copied = 0;
    for (size_t i = 0; i < iov_cnt && copied < len; i++) {
        if (iov[i].write) {
            continue;
        }
        if (skip >= iov[i].len) {
            skip -= iov[i].len;
            continue;
        }
        size_t take = iov[i].len - skip;
        if (take > len - copied) {
            take = len - copied;
        }
        memcpy(out + copied, (const uint8_t *)iov[i].base + skip, take);
        copied += take;
        skip = 0;
    }
    return copied;
}

static inline size_t virtio_iov_write(const virtio_iov_t *iov, size_t iov_cnt,
                                      size_t skip, const void *src, size_t len) {
    const uint8_t *in = src;
    size_t copied = 0;
    for (size_t i = 0; i < iov_cnt && copied < len; i++) {
        if (!iov[i].write) {
            continue;
        }
        if (skip >= iov[i].len) {
            skip -= iov[i].len;
            continue;
        }
        size_t take = iov[i].len - skip;
        if (take > len - copied) {
            take = len - copied;
        }
        memcpy((uint8_t *)iov[i].base + skip, in + copied, take);
        copied += take;
        skip = 0;
    }
    return copied;
}

static inline size_t virtio_iov_total(const virtio_iov_t *iov, size_t iov_cnt, bool write) {
    size_t total = 0;
    for (size_t i = 0; i < iov_cnt; i++) {
        if (iov[i].write == write) {
            total += iov[i].len;
        }
    }
    return total;
}

bool virtqueue_should_notify(virtio_dev_t *dev, virtqueue_t *vq, uint16_t old_used);
void virtqueue_rewind(virtio_dev_t *dev, virtqueue_t *vq, uint16_t saved_avail);
bool virtqueue_request_avail(virtio_dev_t *dev, virtqueue_t *vq);

#endif
