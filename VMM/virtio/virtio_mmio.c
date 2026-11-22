#include "virtio_internal.h"

#include <stdatomic.h>

static uint64_t virtio_offered_features(virtio_dev_t *dev) {
    uint64_t feats = (dev->ops && dev->ops->get_features)
                         ? dev->ops->get_features(dev)
                         : dev->device_features;
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
    if (!dev) {
        return 0;
    }
    if (offset >= VIRTIO_MMIO_CONFIG) {
        if (dev->ops && dev->ops->read_config) {
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
        return dev->vendor_id;
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
    case VIRTIO_MMIO_SHM_BASE_HIGH:
        return 0xFFFFFFFFU;
    case VIRTIO_MMIO_QUEUE_RESET:
        return 0;
    case VIRTIO_MMIO_CONFIG_GENERATION:
        return dev->config_generation;
    default:
        return 0;
    }
}

void virtio_mmio_write(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val) {
    if (!dev) {
        return;
    }
    if (offset >= VIRTIO_MMIO_CONFIG) {
        if (dev->ops && dev->ops->write_config) {
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
        if (v < dev->num_queues &&
            atomic_load_explicit(&dev->queues[v].ready, memory_order_acquire) &&
            (atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK) &&
            dev->ops && dev->ops->queue_notify) {
            dev->ops->queue_notify(dev, v);
        }
        break;
    case VIRTIO_MMIO_INTERRUPT_ACK: {
        pthread_mutex_lock(&dev->irq_lock);
        uint32_t prev = atomic_fetch_and_explicit(&dev->interrupt_status, ~v, memory_order_acq_rel);
        if ((prev & ~v) == 0 && dev->vm) {
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
            if (dev->ops && dev->ops->reset) {
                dev->ops->reset(dev);
            }
            dev->device_features_sel = 0;
            dev->driver_features_sel = 0;
            dev->driver_features = 0;
            dev->queue_sel = 0;
            pthread_mutex_lock(&dev->irq_lock);
            atomic_store_explicit(&dev->interrupt_status, 0, memory_order_release);
            if (dev->vm) {
                gicv2_set_irq(dev->vm, dev->irq_intid, false);
            }
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
        if (features_ok && dev->ops && dev->ops->set_features) {
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
    if (!dev || isr_bits == 0) {
        return;
    }
    pthread_mutex_lock(&dev->irq_lock);
    uint32_t old_status = atomic_fetch_or_explicit(&dev->interrupt_status, isr_bits, memory_order_release);
    if (old_status == 0 && dev->vm) {
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

int virtqueue_pop(virtio_dev_t *dev, virtqueue_t *vq, virtio_iov_t *iov, size_t max_iov,
                  uint16_t *head_out, size_t *out_count, size_t *in_count) {
    if (!dev || !dev->vm || !vq ||
        !atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        vq->num == 0 || vq->num > vq->num_max) {
        return 0;
    }

    size_t avail_sz = sizeof(vring_avail_t) + (size_t)vq->num * sizeof(uint16_t);
    vring_avail_t *avail = mmu_gpa_to_hva(dev->vm, vq->avail_gpa, avail_sz);
    if (!avail) {
        return -1;
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
        return -1;
    }

    size_t desc_sz = (size_t)vq->num * sizeof(vring_desc_t);
    const vring_desc_t *desc_tbl = mmu_gpa_to_hva(dev->vm, vq->desc_gpa, desc_sz);
    if (!desc_tbl) {
        return -1;
    }

    vq->last_avail_idx++;

    if (dev->driver_features & VIRTIO_F_RING_EVENT_IDX) {
        size_t used_event_off = sizeof(vring_used_t) + (size_t)vq->num * sizeof(vring_used_elem_t);
        uint16_t *avail_event = mmu_gpa_to_hva(dev->vm, vq->used_gpa + used_event_off, sizeof(uint16_t));
        if (avail_event) {
            atomic_store_explicit((_Atomic uint16_t *)avail_event, vq->last_avail_idx, memory_order_seq_cst);
        }
    }

    size_t out_cnt = 0;
    size_t in_cnt = 0;
    uint16_t idx = head;
    uint32_t steps = 0;

    for (;;) {
        if (steps++ >= vq->num || idx >= vq->num) {
            return -1;
        }
        vring_desc_t d = desc_tbl[idx];

        if (d.flags & VRING_DESC_F_INDIRECT) {
            if ((d.flags & VRING_DESC_F_NEXT) || d.len == 0 || (d.len % sizeof(vring_desc_t)) != 0) {
                return -1;
            }
            uint32_t ind_max = d.len / (uint32_t)sizeof(vring_desc_t);
            const vring_desc_t *ind_tbl = mmu_gpa_to_hva(dev->vm, d.addr, d.len);
            if (!ind_tbl) {
                return -1;
            }
            uint16_t ind_idx = 0;
            uint32_t ind_steps = 0;
            for (;;) {
                if (ind_steps++ >= ind_max || ind_idx >= ind_max) {
                    return -1;
                }
                vring_desc_t id = ind_tbl[ind_idx];
                if (id.flags & VRING_DESC_F_INDIRECT) {
                    return -1;
                }
                if (virtqueue_append_desc(dev, &id, iov, max_iov, &out_cnt, &in_cnt) < 0) {
                    return -1;
                }
                if (!(id.flags & VRING_DESC_F_NEXT)) {
                    break;
                }
                ind_idx = id.next;
            }
            break;
        }

        if (virtqueue_append_desc(dev, &d, iov, max_iov, &out_cnt, &in_cnt) < 0) {
            return -1;
        }
        if (!(d.flags & VRING_DESC_F_NEXT)) {
            break;
        }
        idx = d.next;
    }

    if (head_out) {
        *head_out = head;
    }
    if (out_count) {
        *out_count = out_cnt;
    }
    if (in_count) {
        *in_count = in_cnt;
    }
    return 1;
}

void virtqueue_push(virtio_dev_t *dev, virtqueue_t *vq, uint16_t head, uint32_t len) {
    if (!dev || !dev->vm || !vq ||
        !atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        vq->num == 0) {
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
    if (!dev || !dev->vm || !vq ||
        !atomic_load_explicit(&vq->ready, memory_order_acquire) ||
        vq->num == 0) {
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
    if (!dev || !vq) {
        return;
    }
    vq->last_avail_idx = saved_avail;
    if (dev->vm &&
        atomic_load_explicit(&vq->ready, memory_order_acquire) &&
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

void virtio_dev_destroy(virtio_dev_t *dev) {
    if (!dev) {
        return;
    }
    if (dev->ops && dev->ops->destroy) {
        dev->ops->destroy(dev);
    }
    pthread_mutex_destroy(&dev->irq_lock);
    free(dev->queues);
    free(dev);
}
