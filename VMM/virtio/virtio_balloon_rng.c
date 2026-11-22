#include "virtio_internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

enum : uint64_t {
    VIRTIO_BALLOON_F_REPORTING = 1ULL << 5,
};

enum {
    VIRTIO_RNG_QUEUE_SIZE = 64,
    VIRTIO_RNG_MAX_IOV = 64,

    VIRTIO_BALLOON_VQ_INFLATE = 0,
    VIRTIO_BALLOON_VQ_DEFLATE = 1,
    VIRTIO_BALLOON_VQ_REPORTING = 2,
    VIRTIO_BALLOON_QUEUE_SIZE = 128,
    VIRTIO_BALLOON_MAX_IOV = 256,
    VIRTIO_BALLOON_HOST_PAGE = 0x4000,
    VIRTIO_BALLOON_PFN_SHIFT = 12,
};

typedef struct __attribute__((packed)) virtio_balloon_config {
    uint32_t num_pages;
    uint32_t actual;
} virtio_balloon_config_t;

typedef struct virtio_balloon {
    virtio_balloon_config_t config;
} virtio_balloon_t;

typedef struct balloon_span {
    uintptr_t start;
    uintptr_t end;
} balloon_span_t;

static void virtio_rng_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    if (queue_idx != 0) {
        return;
    }
    virtqueue_t *vq = &dev->queues[0];
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

    for (;;) {
        virtio_iov_t iov[VIRTIO_RNG_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VIRTIO_RNG_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }
        uint32_t written = 0;
        for (size_t i = 0; i < out_cnt + in_cnt; i++) {
            if (iov[i].write && iov[i].len > 0) {
                arc4random_buf(iov[i].base, iov[i].len);
                written += (uint32_t)iov[i].len;
            }
        }
        virtqueue_push(dev, vq, head, written);
        pushed = true;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static const virtio_dev_ops_t virtio_rng_ops = {
    .queue_notify = virtio_rng_queue_notify,
};

virtio_dev_t *virtio_rng_create(vmm_machine_t *vm, uint32_t slot) {
    if (!vm) {
        return NULL;
    }
    const uint32_t qsizes[1] = { VIRTIO_RNG_QUEUE_SIZE };
    return virtio_dev_alloc(vm, slot, VIRTIO_ID_RNG, 1, qsizes, &virtio_rng_ops, NULL);
}

static int balloon_span_cmp(const void *a, const void *b) {
    const balloon_span_t *sa = a;
    const balloon_span_t *sb = b;
    if (sa->start < sb->start) {
        return -1;
    }
    if (sa->start > sb->start) {
        return 1;
    }
    return 0;
}

static void balloon_reclaim_spans(const vmm_machine_t *vm, balloon_span_t *spans, size_t count) {
    if (!vm || !vm->ram || vm->ram_size == 0 || count == 0) {
        return;
    }
    uintptr_t ram_lo = (uintptr_t)vm->ram;
    uintptr_t ram_hi = ram_lo + vm->ram_size;

    if (count > 1) {
        qsort(spans, count, sizeof(*spans), balloon_span_cmp);
    }

    size_t merged = 0;
    for (size_t i = 0; i < count; i++) {
        if (spans[i].start < ram_lo || spans[i].end > ram_hi || spans[i].end <= spans[i].start) {
            continue;
        }
        if (merged > 0 && spans[i].start <= spans[merged - 1].end) {
            if (spans[i].end > spans[merged - 1].end) {
                spans[merged - 1].end = spans[i].end;
            }
        } else {
            spans[merged++] = spans[i];
        }
    }

    const uintptr_t mask = (uintptr_t)(VIRTIO_BALLOON_HOST_PAGE - 1);
    for (size_t i = 0; i < merged; i++) {
        uintptr_t aligned_start = (spans[i].start + mask) & ~mask;
        uintptr_t aligned_end = spans[i].end & ~mask;
        if (aligned_end > aligned_start && aligned_start >= ram_lo && aligned_end <= ram_hi) {
            size_t aligned_len = (size_t)(aligned_end - aligned_start);
            mmap((void *)aligned_start, aligned_len, PROT_READ | PROT_WRITE,
                 MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
        }
    }
}

static uint64_t virtio_balloon_get_features(virtio_dev_t *dev) {
    (void)dev;
    return VIRTIO_BALLOON_F_REPORTING;
}

static uint64_t virtio_balloon_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_balloon_t *b = dev->priv;
    return virtio_config_read_buf(&b->config, sizeof(b->config), offset, size);
}

static void virtio_balloon_write_config(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val) {
    virtio_balloon_t *b = dev->priv;
    if (offset == offsetof(virtio_balloon_config_t, actual) && size == 4) {
        b->config.actual = (uint32_t)val;
    }
}

static void virtio_balloon_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    if (queue_idx >= dev->num_queues) {
        return;
    }
    virtqueue_t *vq = &dev->queues[queue_idx];
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

    for (;;) {
        virtio_iov_t iov[VIRTIO_BALLOON_MAX_IOV];
        uint16_t head = 0;
        size_t out_cnt = 0;
        size_t in_cnt = 0;
        if (virtqueue_pop(dev, vq, iov, VIRTIO_BALLOON_MAX_IOV, &head, &out_cnt, &in_cnt) <= 0) {
            break;
        }

        if (queue_idx == VIRTIO_BALLOON_VQ_REPORTING) {
            balloon_span_t spans[VIRTIO_BALLOON_MAX_IOV];
            size_t nspans = 0;
            for (size_t i = 0; i < out_cnt + in_cnt; i++) {
                if (iov[i].write && iov[i].len > 0) {
                    uintptr_t base = (uintptr_t)iov[i].base;
                    spans[nspans++] = (balloon_span_t){
                        .start = base,
                        .end = base + iov[i].len,
                    };
                }
            }
            balloon_reclaim_spans(dev->vm, spans, nspans);
        } else if (queue_idx == VIRTIO_BALLOON_VQ_INFLATE) {
            size_t out_bytes = virtio_iov_total(iov, out_cnt + in_cnt, false);
            size_t npfns = out_bytes / sizeof(uint32_t);
            if (npfns > 0) {
                uint32_t *pfns = calloc(npfns, sizeof(uint32_t));
                balloon_span_t *spans = malloc(npfns * sizeof(balloon_span_t));
                if (pfns && spans) {
                    size_t got = virtio_iov_read(iov, out_cnt + in_cnt, 0, pfns, npfns * sizeof(uint32_t));
                    size_t valid_pfns = got / sizeof(uint32_t);
                    size_t nspans = 0;
                    for (size_t i = 0; i < valid_pfns; i++) {
                        uint64_t gpa = (uint64_t)pfns[i] << VIRTIO_BALLOON_PFN_SHIFT;
                        void *hva = mmu_gpa_to_hva(dev->vm, gpa, VMM_PAGE_SIZE);
                        if (hva) {
                            spans[nspans++] = (balloon_span_t){
                                .start = (uintptr_t)hva,
                                .end = (uintptr_t)hva + VMM_PAGE_SIZE,
                            };
                        }
                    }
                    balloon_reclaim_spans(dev->vm, spans, nspans);
                }
                free(spans);
                free(pfns);
            }
        }

        virtqueue_push(dev, vq, head, 0);
        pushed = true;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static void virtio_balloon_destroy(virtio_dev_t *dev) {
    free(dev->priv);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_balloon_ops = {
    .get_features = virtio_balloon_get_features,
    .read_config = virtio_balloon_read_config,
    .write_config = virtio_balloon_write_config,
    .queue_notify = virtio_balloon_queue_notify,
    .destroy = virtio_balloon_destroy,
};

virtio_dev_t *virtio_balloon_create(vmm_machine_t *vm, uint32_t slot) {
    if (!vm) {
        return NULL;
    }
    virtio_balloon_t *b = calloc(1, sizeof(*b));
    if (!b) {
        return NULL;
    }
    const uint32_t qsizes[3] = {
        VIRTIO_BALLOON_QUEUE_SIZE,
        VIRTIO_BALLOON_QUEUE_SIZE,
        VIRTIO_BALLOON_QUEUE_SIZE,
    };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_BALLOON, 3, qsizes, &virtio_balloon_ops, b);
    if (!dev) {
        free(b);
        return NULL;
    }
    return dev;
}
