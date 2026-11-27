#ifndef VMM_VIRTIO_VIRTIO_INTERNAL_H
#define VMM_VIRTIO_VIRTIO_INTERNAL_H

#include "virtio_mmio.h"
#include "../board/machine.h"
#include "../mmu/mmu.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>

enum : uint32_t {
    VIRTIO_ID_NET = 1,
    VIRTIO_ID_BLOCK = 2,
    VIRTIO_ID_RNG = 4,
    VIRTIO_ID_BALLOON = 5,
    VIRTIO_ID_GPU = 16,
    VIRTIO_ID_VSOCK = 19,
    VIRTIO_ID_FS = 26,
};

enum : uint32_t {
    VIRTIO_STATUS_ACKNOWLEDGE = 1U << 0,
    VIRTIO_STATUS_DRIVER = 1U << 1,
    VIRTIO_STATUS_DRIVER_OK = 1U << 2,
    VIRTIO_STATUS_FEATURES_OK = 1U << 3,
    VIRTIO_STATUS_DEVICE_NEEDS_RESET = 1U << 6,
    VIRTIO_STATUS_FAILED = 1U << 7,
};

enum : uint32_t {
    VIRTIO_MMIO_INT_VRING = 1U << 0,
    VIRTIO_MMIO_INT_CONFIG = 1U << 1,
};

typedef struct virtio_iov {
    void *base;
    size_t len;
    bool write;
} virtio_iov_t;

typedef struct virtqueue {
    uint32_t num_max;
    uint32_t num;
    _Atomic uint32_t ready;
    uint64_t desc_gpa;
    uint64_t avail_gpa;
    uint64_t used_gpa;
    uint16_t last_avail_idx;
    uint16_t used_idx;
} virtqueue_t;

typedef struct virtio_dev_ops {
    uint64_t (*get_features)(virtio_dev_t *dev);
    void (*set_features)(virtio_dev_t *dev, uint64_t features);
    uint64_t (*read_config)(virtio_dev_t *dev, uint32_t offset, uint32_t size);
    void (*write_config)(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val);
    void (*queue_notify)(virtio_dev_t *dev, uint32_t queue_idx);
    void (*reset)(virtio_dev_t *dev);
    void (*destroy)(virtio_dev_t *dev);
    bool (*shm_region)(virtio_dev_t *dev, uint32_t id, uint64_t *base, uint64_t *len);
} virtio_dev_ops_t;

struct virtio_dev {
    vmm_machine_t *vm;
    uint32_t device_id;
    uint32_t irq_intid;
    _Atomic uint32_t status;
    uint32_t device_features_sel;
    uint32_t driver_features_sel;
    uint64_t driver_features;
    uint32_t queue_sel;
    uint32_t shm_sel;
    uint32_t num_queues;
    virtqueue_t *queues;
    pthread_mutex_t irq_lock;
    _Atomic uint32_t interrupt_status;
    const virtio_dev_ops_t *ops;
    void *priv;
};

typedef struct virtio_worker {
    pthread_t thread;
    bool started;
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t idle_cond;
    bool pending;
    bool busy;
    bool stop;
    void (*run)(void *ctx);
    void *ctx;
} virtio_worker_t;

virtio_dev_t *virtio_dev_alloc(vmm_machine_t *vm, uint32_t slot, uint32_t device_id, uint32_t num_queues,
                               const uint32_t *queue_sizes, const virtio_dev_ops_t *ops, void *priv);
void virtio_raise_irq(virtio_dev_t *dev, uint32_t isr_bits);
int virtqueue_pop(virtio_dev_t *dev, virtqueue_t *vq, virtio_iov_t *iov, size_t max_iov,
                  uint16_t *head_out, size_t *count_out);
void virtqueue_push(virtio_dev_t *dev, virtqueue_t *vq, uint16_t head, uint32_t len);
bool virtqueue_should_notify(virtio_dev_t *dev, virtqueue_t *vq, uint16_t old_used);
void virtqueue_rewind(virtio_dev_t *dev, virtqueue_t *vq, uint16_t saved_avail);
bool virtqueue_request_avail(virtio_dev_t *dev, virtqueue_t *vq);
bool virtqueue_has_avail(virtio_dev_t *dev, virtqueue_t *vq);

void virtio_worker_init(virtio_worker_t *worker, void (*run)(void *ctx), void *ctx);
int virtio_worker_start(virtio_worker_t *worker);
void virtio_worker_kick(virtio_worker_t *worker);
void virtio_worker_wait_idle(virtio_worker_t *worker);
void virtio_worker_destroy(virtio_worker_t *worker);

int virtio_write_zeros(int fd, off_t offset, off_t length);

static inline bool virtqueue_active(virtio_dev_t *dev, virtqueue_t *vq) {
    return atomic_load_explicit(&vq->ready, memory_order_acquire) &&
           (atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK);
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

static inline int virtio_iov_spans(const virtio_iov_t *iov, size_t iov_cnt, bool write, size_t skip,
                                   size_t len, struct iovec *spans) {
    int used = 0;
    for (size_t i = 0; i < iov_cnt && len > 0; i++) {
        if (iov[i].write != write) {
            continue;
        }
        if (skip >= iov[i].len) {
            skip -= iov[i].len;
            continue;
        }
        size_t span = iov[i].len - skip;
        if (span > len) {
            span = len;
        }
        spans[used++] = (struct iovec){.iov_base = (uint8_t *)iov[i].base + skip, .iov_len = span};
        len -= span;
        skip = 0;
    }
    return used;
}

#endif
