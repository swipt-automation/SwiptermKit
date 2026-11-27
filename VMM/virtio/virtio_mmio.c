#include "virtio_internal.h"

#include <errno.h>
#include <unistd.h>

enum : uint32_t {
    VIRTIO_MMIO_MAGIC_VALUE = 0x000,
    VIRTIO_MMIO_VERSION = 0x004,
    VIRTIO_MMIO_DEVICE_ID = 0x008,
    VIRTIO_MMIO_VENDOR_ID = 0x00c,
    VIRTIO_MMIO_DEVICE_FEATURES = 0x010,
    VIRTIO_MMIO_DEVICE_FEATURES_SEL = 0x014,
    VIRTIO_MMIO_DRIVER_FEATURES = 0x020,
    VIRTIO_MMIO_DRIVER_FEATURES_SEL = 0x024,
    VIRTIO_MMIO_QUEUE_SEL = 0x030,
    VIRTIO_MMIO_QUEUE_NUM_MAX = 0x034,
    VIRTIO_MMIO_QUEUE_NUM = 0x038,
    VIRTIO_MMIO_QUEUE_READY = 0x044,
    VIRTIO_MMIO_QUEUE_NOTIFY = 0x050,
    VIRTIO_MMIO_INTERRUPT_STATUS = 0x060,
    VIRTIO_MMIO_INTERRUPT_ACK = 0x064,
    VIRTIO_MMIO_STATUS = 0x070,
    VIRTIO_MMIO_QUEUE_DESC_LOW = 0x080,
    VIRTIO_MMIO_QUEUE_DESC_HIGH = 0x084,
    VIRTIO_MMIO_QUEUE_DRIVER_LOW = 0x090,
    VIRTIO_MMIO_QUEUE_DRIVER_HIGH = 0x094,
    VIRTIO_MMIO_QUEUE_DEVICE_LOW = 0x0a0,
    VIRTIO_MMIO_QUEUE_DEVICE_HIGH = 0x0a4,
    VIRTIO_MMIO_SHM_SEL = 0x0ac,
    VIRTIO_MMIO_SHM_LEN_LOW = 0x0b0,
    VIRTIO_MMIO_SHM_LEN_HIGH = 0x0b4,
    VIRTIO_MMIO_SHM_BASE_LOW = 0x0b8,
    VIRTIO_MMIO_SHM_BASE_HIGH = 0x0bc,
    VIRTIO_MMIO_QUEUE_RESET = 0x0c0,
    VIRTIO_MMIO_CONFIG_GENERATION = 0x0fc,
    VIRTIO_MMIO_CONFIG = 0x100,
};

#define VIRTIO_MMIO_MAGIC 0x74726976U
#define VIRTIO_MMIO_VERSION_2 2U
#define VIRTIO_VENDOR_SWIPTERM 0x53574950U

#define VIRTIO_F_INDIRECT_DESC (1ULL << 28)
#define VIRTIO_F_RING_EVENT_IDX (1ULL << 29)
#define VIRTIO_F_VERSION_1 (1ULL << 32)

enum : uint16_t {
    VRING_DESC_F_NEXT = 1,
    VRING_DESC_F_WRITE = 2,
    VRING_DESC_F_INDIRECT = 4,
};

typedef struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

typedef struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail_t;

typedef struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct vring_used {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[];
} vring_used_t;

virtio_dev_t *virtio_dev_alloc(vmm_machine_t *vm, uint32_t slot, uint32_t device_id, uint32_t num_queues,
                               const uint32_t *queue_sizes, const virtio_dev_ops_t *ops, void *priv) {
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
    dev->device_id = device_id;
    dev->irq_intid = VMM_IRQ_VIRTIO_BASE + slot;
    dev->num_queues = num_queues;
    pthread_mutex_init(&dev->irq_lock, NULL);
    dev->ops = ops;
    dev->priv = priv;
    for (uint32_t i = 0; i < num_queues; i++) {
        dev->queues[i].num_max = queue_sizes[i];
    }
    return dev;
}

static uint64_t virtio_offered_features(virtio_dev_t *dev) {
    uint64_t feats = dev->ops->get_features ? dev->ops->get_features(dev) : 0;
    return feats | VIRTIO_F_VERSION_1 | VIRTIO_F_INDIRECT_DESC | VIRTIO_F_RING_EVENT_IDX;
}

static void virtqueue_reset_state(virtqueue_t *vq) {
    atomic_store_explicit(&vq->ready, 0, memory_order_release);
    vq->num = 0;
    vq->desc_gpa = 0;
    vq->avail_gpa = 0;
    vq->used_gpa = 0;
    vq->last_avail_idx = 0;
    vq->used_idx = 0;
}

uint64_t virtio_mmio_read(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    if (offset >= VIRTIO_MMIO_CONFIG) {
        if (dev->ops->read_config) {
            return dev->ops->read_config(dev, offset - VIRTIO_MMIO_CONFIG, size);
        }
        return 0;
    }

    switch (offset) {
    case VIRTIO_MMIO_MAGIC_VALUE:
        return VIRTIO_MMIO_MAGIC;
    case VIRTIO_MMIO_VERSION:
        return VIRTIO_MMIO_VERSION_2;
    case VIRTIO_MMIO_DEVICE_ID:
        return dev->device_id;
    case VIRTIO_MMIO_VENDOR_ID:
        return VIRTIO_VENDOR_SWIPTERM;
    case VIRTIO_MMIO_DEVICE_FEATURES: {
        uint64_t feats = virtio_offered_features(dev);
        if (dev->device_features_sel == 0) {
            return (uint32_t)feats;
        }
        if (dev->device_features_sel == 1) {
            return (uint32_t)(feats >> 32);
        }
        return 0;
    }
    case VIRTIO_MMIO_QUEUE_NUM_MAX:
        return dev->queue_sel < dev->num_queues ? dev->queues[dev->queue_sel].num_max : 0;
    case VIRTIO_MMIO_QUEUE_READY:
        return dev->queue_sel < dev->num_queues
                   ? atomic_load_explicit(&dev->queues[dev->queue_sel].ready, memory_order_acquire)
                   : 0;
    case VIRTIO_MMIO_INTERRUPT_STATUS:
        return atomic_load_explicit(&dev->interrupt_status, memory_order_acquire);
    case VIRTIO_MMIO_STATUS:
        return atomic_load_explicit(&dev->status, memory_order_acquire);
    case VIRTIO_MMIO_SHM_LEN_LOW:
    case VIRTIO_MMIO_SHM_LEN_HIGH:
    case VIRTIO_MMIO_SHM_BASE_LOW:
    case VIRTIO_MMIO_SHM_BASE_HIGH: {
        uint64_t base = 0;
        uint64_t len = 0;
        if (!dev->ops->shm_region || !dev->ops->shm_region(dev, dev->shm_sel, &base, &len)) {
            return 0xFFFFFFFFU;
        }
        uint64_t value = offset == VIRTIO_MMIO_SHM_LEN_LOW || offset == VIRTIO_MMIO_SHM_LEN_HIGH ? len : base;
        return offset == VIRTIO_MMIO_SHM_LEN_LOW || offset == VIRTIO_MMIO_SHM_BASE_LOW ? (uint32_t)value
                                                                                       : (uint32_t)(value >> 32);
    }
    default:
        return 0;
    }
}

void virtio_mmio_write(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val) {
    if (offset >= VIRTIO_MMIO_CONFIG) {
        if (dev->ops->write_config) {
            dev->ops->write_config(dev, offset - VIRTIO_MMIO_CONFIG, size, val);
        }
        return;
    }

    uint32_t v = (uint32_t)val;
    switch (offset) {
    case VIRTIO_MMIO_DEVICE_FEATURES_SEL:
        dev->device_features_sel = v;
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES:
        if (dev->driver_features_sel == 0) {
            dev->driver_features = (dev->driver_features & 0xFFFFFFFF00000000ULL) | (uint64_t)v;
        } else if (dev->driver_features_sel == 1) {
            dev->driver_features = (dev->driver_features & 0x00000000FFFFFFFFULL) | ((uint64_t)v << 32);
        }
        break;
    case VIRTIO_MMIO_DRIVER_FEATURES_SEL:
        dev->driver_features_sel = v;
        break;
    case VIRTIO_MMIO_QUEUE_SEL:
        dev->queue_sel = v;
        break;
    case VIRTIO_MMIO_SHM_SEL:
        dev->shm_sel = v;
        break;
    case VIRTIO_MMIO_QUEUE_NUM:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->num = v <= vq->num_max ? v : vq->num_max;
        }
        break;
    case VIRTIO_MMIO_QUEUE_READY:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            uint32_t ready = v ? 1U : 0U;
            if (!ready) {
                atomic_store_explicit(&vq->ready, 0, memory_order_release);
                vq->last_avail_idx = 0;
                vq->used_idx = 0;
            } else {
                atomic_store_explicit(&vq->ready, 1U, memory_order_release);
            }
        }
        break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
        if (v < dev->num_queues && virtqueue_active(dev, &dev->queues[v])) {
            dev->ops->queue_notify(dev, v);
        }
        break;
    case VIRTIO_MMIO_INTERRUPT_ACK: {
        pthread_mutex_lock(&dev->irq_lock);
        uint32_t prev = atomic_fetch_and_explicit(&dev->interrupt_status, ~v, memory_order_acq_rel);
        if ((prev & ~v) == 0) {
            gicv2_set_irq(dev->vm, dev->irq_intid, false);
        }
        pthread_mutex_unlock(&dev->irq_lock);
        break;
    }
    case VIRTIO_MMIO_STATUS:
        if (v == 0) {
            atomic_store_explicit(&dev->status, 0, memory_order_release);
            for (uint32_t i = 0; i < dev->num_queues; i++) {
                atomic_store_explicit(&dev->queues[i].ready, 0, memory_order_release);
            }
            if (dev->ops->reset) {
                dev->ops->reset(dev);
            }
            dev->device_features_sel = 0;
            dev->driver_features_sel = 0;
            dev->driver_features = 0;
            dev->queue_sel = 0;
            pthread_mutex_lock(&dev->irq_lock);
            atomic_store_explicit(&dev->interrupt_status, 0, memory_order_release);
            gicv2_set_irq(dev->vm, dev->irq_intid, false);
            pthread_mutex_unlock(&dev->irq_lock);
            for (uint32_t i = 0; i < dev->num_queues; i++) {
                virtqueue_reset_state(&dev->queues[i]);
            }
            break;
        }
        uint32_t prev_status = atomic_load_explicit(&dev->status, memory_order_acquire);
        bool features_ok = (v & VIRTIO_STATUS_FEATURES_OK) && !(prev_status & VIRTIO_STATUS_FEATURES_OK);
        if (features_ok) {
            dev->driver_features &= virtio_offered_features(dev);
            if (!(dev->driver_features & VIRTIO_F_VERSION_1)) {
                v &= ~VIRTIO_STATUS_FEATURES_OK;
                features_ok = false;
            }
        }
        atomic_store_explicit(&dev->status, v, memory_order_release);
        if (features_ok && dev->ops->set_features) {
            dev->ops->set_features(dev, dev->driver_features);
        }
        break;
    case VIRTIO_MMIO_QUEUE_DESC_LOW:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->desc_gpa = (vq->desc_gpa & 0xFFFFFFFF00000000ULL) | (uint64_t)v;
        }
        break;
    case VIRTIO_MMIO_QUEUE_DESC_HIGH:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->desc_gpa = (vq->desc_gpa & 0x00000000FFFFFFFFULL) | ((uint64_t)v << 32);
        }
        break;
    case VIRTIO_MMIO_QUEUE_DRIVER_LOW:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->avail_gpa = (vq->avail_gpa & 0xFFFFFFFF00000000ULL) | (uint64_t)v;
        }
        break;
    case VIRTIO_MMIO_QUEUE_DRIVER_HIGH:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->avail_gpa = (vq->avail_gpa & 0x00000000FFFFFFFFULL) | ((uint64_t)v << 32);
        }
        break;
    case VIRTIO_MMIO_QUEUE_DEVICE_LOW:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->used_gpa = (vq->used_gpa & 0xFFFFFFFF00000000ULL) | (uint64_t)v;
        }
        break;
    case VIRTIO_MMIO_QUEUE_DEVICE_HIGH:
        if (dev->queue_sel < dev->num_queues) {
            virtqueue_t *vq = &dev->queues[dev->queue_sel];
            vq->used_gpa = (vq->used_gpa & 0x00000000FFFFFFFFULL) | ((uint64_t)v << 32);
        }
        break;
    case VIRTIO_MMIO_QUEUE_RESET:
        if (v == 1 && dev->queue_sel < dev->num_queues) {
            virtqueue_reset_state(&dev->queues[dev->queue_sel]);
        }
        break;
    default:
        break;
    }
}

void virtio_raise_irq(virtio_dev_t *dev, uint32_t isr_bits) {
    pthread_mutex_lock(&dev->irq_lock);
    uint32_t old_status = atomic_fetch_or_explicit(&dev->interrupt_status, isr_bits, memory_order_release);
    if (old_status == 0) {
        gicv2_set_irq(dev->vm, dev->irq_intid, true);
    }
    pthread_mutex_unlock(&dev->irq_lock);
}

static int virtqueue_append_desc(virtio_dev_t *dev, const vring_desc_t *desc,
                                 virtio_iov_t *iov, size_t max_iov,
                                 size_t *out_cnt, size_t *in_cnt) {
    size_t total = *out_cnt + *in_cnt;
    if (total >= max_iov) {
        return -1;
    }
    bool write = (desc->flags & VRING_DESC_F_WRITE) != 0;
    if (!write && *in_cnt > 0) {
        return -1;
    }
    void *hva = NULL;
    if (desc->len > 0) {
        hva = mmu_gpa_to_hva(dev->vm, desc->addr, desc->len);
        if (!hva) {
            return -1;
        }
    }
    iov[total].base = hva;
    iov[total].len = desc->len;
    iov[total].write = write;
    if (write) {
        (*in_cnt)++;
    } else {
        (*out_cnt)++;
    }
    return 0;
}

static int virtqueue_fail(virtio_dev_t *dev) {
    uint32_t prev = atomic_fetch_or_explicit(&dev->status, VIRTIO_STATUS_DEVICE_NEEDS_RESET, memory_order_acq_rel);
    if (!(prev & VIRTIO_STATUS_DEVICE_NEEDS_RESET)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_CONFIG);
    }
    return -1;
}

int virtqueue_pop(virtio_dev_t *dev, virtqueue_t *vq, virtio_iov_t *iov, size_t max_iov,
                  uint16_t *head_out, size_t *count_out) {
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) || vq->num == 0) {
        return 0;
    }
    if (atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DEVICE_NEEDS_RESET) {
        return -1;
    }

    size_t avail_sz = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
    vring_avail_t *avail = mmu_gpa_to_hva(dev->vm, vq->avail_gpa, avail_sz);
    if (!avail) {
        return virtqueue_fail(dev);
    }

    uint16_t avail_idx = atomic_load_explicit((_Atomic uint16_t *)&avail->idx, memory_order_acquire);
    if (avail_idx == vq->last_avail_idx) {
        atomic_thread_fence(memory_order_seq_cst);
        avail_idx = atomic_load_explicit((_Atomic uint16_t *)&avail->idx, memory_order_acquire);
        if (avail_idx == vq->last_avail_idx) {
            return 0;
        }
    }

    uint16_t head = avail->ring[vq->last_avail_idx % vq->num];
    if (head >= vq->num) {
        return virtqueue_fail(dev);
    }

    size_t desc_sz = (size_t)vq->num * sizeof(vring_desc_t);
    const vring_desc_t *desc_tbl = mmu_gpa_to_hva(dev->vm, vq->desc_gpa, desc_sz);
    if (!desc_tbl) {
        return virtqueue_fail(dev);
    }

    size_t out_cnt = 0;
    size_t in_cnt = 0;
    uint16_t idx = head;
    uint32_t steps = 0;

    for (;;) {
        if (steps++ >= vq->num || idx >= vq->num) {
            return virtqueue_fail(dev);
        }
        vring_desc_t d = desc_tbl[idx];

        if (d.flags & VRING_DESC_F_INDIRECT) {
            if ((d.flags & VRING_DESC_F_NEXT) || d.len == 0 || (d.len % sizeof(vring_desc_t)) != 0) {
                return virtqueue_fail(dev);
            }
            uint32_t ind_max = d.len / (uint32_t)sizeof(vring_desc_t);
            const vring_desc_t *ind_tbl = mmu_gpa_to_hva(dev->vm, d.addr, d.len);
            if (!ind_tbl) {
                return virtqueue_fail(dev);
            }
            uint16_t ind_idx = 0;
            uint32_t ind_steps = 0;
            for (;;) {
                if (ind_steps++ >= ind_max || ind_idx >= ind_max) {
                    return virtqueue_fail(dev);
                }
                vring_desc_t id = ind_tbl[ind_idx];
                if (id.flags & VRING_DESC_F_INDIRECT) {
                    return virtqueue_fail(dev);
                }
                if (virtqueue_append_desc(dev, &id, iov, max_iov, &out_cnt, &in_cnt) < 0) {
                    return virtqueue_fail(dev);
                }
                if (!(id.flags & VRING_DESC_F_NEXT)) {
                    break;
                }
                ind_idx = id.next;
            }
            break;
        }

        if (virtqueue_append_desc(dev, &d, iov, max_iov, &out_cnt, &in_cnt) < 0) {
            return virtqueue_fail(dev);
        }
        if (!(d.flags & VRING_DESC_F_NEXT)) {
            break;
        }
        idx = d.next;
    }

    vq->last_avail_idx++;

    if (dev->driver_features & VIRTIO_F_RING_EVENT_IDX) {
        size_t used_event_off = sizeof(vring_used_t) + (size_t)vq->num * sizeof(vring_used_elem_t);
        uint16_t *avail_event = mmu_gpa_to_hva(dev->vm, vq->used_gpa + used_event_off, sizeof(uint16_t));
        if (avail_event) {
            atomic_store_explicit((_Atomic uint16_t *)avail_event, vq->last_avail_idx, memory_order_seq_cst);
        }
    }

    *head_out = head;
    *count_out = out_cnt + in_cnt;
    return 1;
}

void virtqueue_push(virtio_dev_t *dev, virtqueue_t *vq, uint16_t head, uint32_t len) {
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) || vq->num == 0) {
        return;
    }
    size_t used_sz = sizeof(vring_used_t) + (size_t)vq->num * sizeof(vring_used_elem_t);
    vring_used_t *used = mmu_gpa_to_hva(dev->vm, vq->used_gpa, used_sz);
    if (!used) {
        return;
    }
    used->ring[vq->used_idx % vq->num] = (vring_used_elem_t){
        .id = head,
        .len = len,
    };
    vq->used_idx++;
    atomic_store_explicit((_Atomic uint16_t *)&used->idx, vq->used_idx, memory_order_release);
}

bool virtqueue_should_notify(virtio_dev_t *dev, virtqueue_t *vq, uint16_t old_used) {
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) || vq->num == 0) {
        return false;
    }
    atomic_thread_fence(memory_order_seq_cst);
    if (dev->driver_features & VIRTIO_F_RING_EVENT_IDX) {
        size_t off = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
        const uint16_t *used_event = mmu_gpa_to_hva(dev->vm, vq->avail_gpa + off, sizeof(uint16_t));
        if (!used_event) {
            return true;
        }
        uint16_t ev = atomic_load_explicit((_Atomic uint16_t *)used_event, memory_order_acquire);
        uint16_t new_used = vq->used_idx;
        return (uint16_t)(new_used - ev - 1U) < (uint16_t)(new_used - old_used);
    }
    size_t avail_sz = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
    const vring_avail_t *avail = mmu_gpa_to_hva(dev->vm, vq->avail_gpa, avail_sz);
    if (!avail) {
        return true;
    }
    uint16_t flags = atomic_load_explicit((_Atomic uint16_t *)&avail->flags, memory_order_acquire);
    return (flags & 1U) == 0;
}

void virtqueue_rewind(virtio_dev_t *dev, virtqueue_t *vq, uint16_t saved_avail) {
    vq->last_avail_idx = saved_avail;
    if (atomic_load_explicit(&vq->ready, memory_order_acquire) &&
        vq->num > 0 && (dev->driver_features & VIRTIO_F_RING_EVENT_IDX)) {
        size_t off = sizeof(vring_used_t) + (size_t)vq->num * sizeof(vring_used_elem_t);
        uint16_t *avail_event = mmu_gpa_to_hva(dev->vm, vq->used_gpa + off, sizeof(uint16_t));
        if (avail_event) {
            atomic_store_explicit((_Atomic uint16_t *)avail_event, saved_avail, memory_order_release);
        }
    }
}

bool virtqueue_request_avail(virtio_dev_t *dev, virtqueue_t *vq) {
    if (!(dev->driver_features & VIRTIO_F_RING_EVENT_IDX)) {
        return false;
    }
    size_t avail_sz = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
    const vring_avail_t *avail = mmu_gpa_to_hva(dev->vm, vq->avail_gpa, avail_sz);
    size_t off = sizeof(vring_used_t) + (size_t)vq->num * sizeof(vring_used_elem_t);
    uint16_t *avail_event = mmu_gpa_to_hva(dev->vm, vq->used_gpa + off, sizeof(uint16_t));
    if (!avail || !avail_event) {
        return false;
    }
    uint16_t idx = atomic_load_explicit((_Atomic uint16_t *)&avail->idx, memory_order_acquire);
    atomic_store_explicit((_Atomic uint16_t *)avail_event, idx, memory_order_seq_cst);
    atomic_thread_fence(memory_order_seq_cst);
    return atomic_load_explicit((_Atomic uint16_t *)&avail->idx, memory_order_acquire) != idx;
}

bool virtqueue_has_avail(virtio_dev_t *dev, virtqueue_t *vq) {
    uint32_t status = atomic_load_explicit(&dev->status, memory_order_acquire);
    if (!atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        vq->num == 0 ||
        (status & (VIRTIO_STATUS_DRIVER_OK | VIRTIO_STATUS_DEVICE_NEEDS_RESET)) != VIRTIO_STATUS_DRIVER_OK) {
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

void virtio_dev_destroy(virtio_dev_t *dev) {
    if (dev->ops->destroy) {
        dev->ops->destroy(dev);
    }
    pthread_mutex_destroy(&dev->irq_lock);
    free(dev->queues);
    free(dev);
}

static void *virtio_worker_main(void *arg) {
    virtio_worker_t *worker = arg;
    pthread_mutex_lock(&worker->lock);
    for (;;) {
        while (!worker->pending && !worker->stop) {
            pthread_cond_wait(&worker->work_cond, &worker->lock);
        }
        if (!worker->pending && worker->stop) {
            break;
        }
        worker->pending = false;
        worker->busy = true;
        pthread_mutex_unlock(&worker->lock);

        worker->run(worker->ctx);

        pthread_mutex_lock(&worker->lock);
        worker->busy = false;
        if (!worker->pending) {
            pthread_cond_broadcast(&worker->idle_cond);
        }
    }
    pthread_mutex_unlock(&worker->lock);
    return NULL;
}

void virtio_worker_init(virtio_worker_t *worker, void (*run)(void *ctx), void *ctx) {
    pthread_mutex_init(&worker->lock, NULL);
    pthread_cond_init(&worker->work_cond, NULL);
    pthread_cond_init(&worker->idle_cond, NULL);
    worker->run = run;
    worker->ctx = ctx;
}

int virtio_worker_start(virtio_worker_t *worker) {
    if (pthread_create(&worker->thread, NULL, virtio_worker_main, worker) != 0) {
        return -1;
    }
    worker->started = true;
    return 0;
}

void virtio_worker_kick(virtio_worker_t *worker) {
    pthread_mutex_lock(&worker->lock);
    if (!worker->stop) {
        worker->pending = true;
        pthread_cond_signal(&worker->work_cond);
    }
    pthread_mutex_unlock(&worker->lock);
}

void virtio_worker_wait_idle(virtio_worker_t *worker) {
    pthread_mutex_lock(&worker->lock);
    while (worker->pending || worker->busy) {
        pthread_cond_wait(&worker->idle_cond, &worker->lock);
    }
    pthread_mutex_unlock(&worker->lock);
}

void virtio_worker_destroy(virtio_worker_t *worker) {
    pthread_mutex_lock(&worker->lock);
    worker->stop = true;
    pthread_cond_signal(&worker->work_cond);
    pthread_mutex_unlock(&worker->lock);
    if (worker->started) {
        pthread_join(worker->thread, NULL);
    }
    pthread_cond_destroy(&worker->idle_cond);
    pthread_cond_destroy(&worker->work_cond);
    pthread_mutex_destroy(&worker->lock);
}

int virtio_write_zeros(int fd, off_t offset, off_t length) {
    static const uint8_t zeros[65536];
    while (length > 0) {
        size_t chunk = length < (off_t)sizeof(zeros) ? (size_t)length : sizeof(zeros);
        ssize_t n = pwrite(fd, zeros, chunk, offset);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        offset += n;
        length -= n;
    }
    return 0;
}
