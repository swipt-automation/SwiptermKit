#include "virtio_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

enum : uint64_t {
    VIRTIO_BLK_F_SIZE_MAX = 1ULL << 1,
    VIRTIO_BLK_F_SEG_MAX = 1ULL << 2,
    VIRTIO_BLK_F_RO = 1ULL << 5,
    VIRTIO_BLK_F_BLK_SIZE = 1ULL << 6,
    VIRTIO_BLK_F_TOPOLOGY = 1ULL << 10,
    VIRTIO_BLK_F_DISCARD = 1ULL << 13,
    VIRTIO_BLK_F_WRITE_ZEROES = 1ULL << 14,
};

enum : uint32_t {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1,
    VIRTIO_BLK_T_GET_ID = 8,
    VIRTIO_BLK_T_DISCARD = 11,
    VIRTIO_BLK_T_WRITE_ZEROES = 13,
};

enum : uint8_t {
    VIRTIO_BLK_S_OK = 0,
    VIRTIO_BLK_S_IOERR = 1,
    VIRTIO_BLK_S_UNSUPP = 2,
};

enum {
    VIRTIO_BLK_MAX_IOV = 512,
    VIRTIO_BLK_MAX_DWZ_SEG = 256,
    VIRTIO_BLK_QUEUE_SIZE = 256,
    VIRTIO_BLK_SECTOR_SIZE = 512,
    VIRTIO_BLK_BLOCK_SIZE = 4096,
    VIRTIO_BLK_ID_BYTES = 20,
    VIRTIO_BLK_WRITE_ZEROES_UNMAP = 1,
};

typedef struct __attribute__((packed)) virtio_blk_config {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t unused0;
    uint16_t num_queues;
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
} virtio_blk_config_t;

typedef struct __attribute__((packed)) virtio_blk_req_hdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_blk_req_hdr_t;

typedef struct __attribute__((packed)) virtio_blk_discard_write_zeroes {
    uint64_t sector;
    uint32_t num_sectors;
    uint32_t flags;
} virtio_blk_dwz_t;

typedef struct virtio_blk {
    virtio_dev_t *dev;
    int fd;
    bool read_only;
    virtio_blk_config_t config;
    char serial[VIRTIO_BLK_ID_BYTES];
    virtio_worker_t worker;
    _Atomic bool dirty;
} virtio_blk_t;

static int blk_flush(int fd) {
    if (fcntl(fd, F_BARRIERFSYNC) == 0) {
        return 0;
    }
    return fsync(fd);
}

static uint8_t blk_handle_rw(virtio_blk_t *blk, uint32_t type, uint64_t sector,
                             const virtio_iov_t *iov, size_t total_iov, size_t in_total,
                             uint32_t *written_out) {
    if (type == VIRTIO_BLK_T_OUT && blk->read_only) {
        return VIRTIO_BLK_S_IOERR;
    }
    struct iovec vecs[VIRTIO_BLK_MAX_IOV];
    size_t data_bytes;
    int vec_cnt;
    if (type == VIRTIO_BLK_T_IN) {
        data_bytes = in_total - 1;
        vec_cnt = virtio_iov_spans(iov, total_iov, true, 0, data_bytes, vecs);
    } else {
        data_bytes = virtio_iov_total(iov, total_iov, false) - sizeof(virtio_blk_req_hdr_t);
        vec_cnt = virtio_iov_spans(iov, total_iov, false, sizeof(virtio_blk_req_hdr_t), data_bytes, vecs);
    }

    if ((data_bytes % VIRTIO_BLK_SECTOR_SIZE) != 0) {
        return VIRTIO_BLK_S_IOERR;
    }
    uint64_t sectors = data_bytes / VIRTIO_BLK_SECTOR_SIZE;
    if (sector > blk->config.capacity || sectors > blk->config.capacity - sector) {
        return VIRTIO_BLK_S_IOERR;
    }
    if (data_bytes == 0) {
        return VIRTIO_BLK_S_OK;
    }

    off_t offset = (off_t)(sector * VIRTIO_BLK_SECTOR_SIZE);
    size_t done = 0;
    int idx = 0;
    while (done < data_bytes && idx < vec_cnt) {
        ssize_t n;
        do {
            n = (type == VIRTIO_BLK_T_IN)
                    ? preadv(blk->fd, &vecs[idx], vec_cnt - idx, offset)
                    : pwritev(blk->fd, &vecs[idx], vec_cnt - idx, offset);
        } while (n < 0 && errno == EINTR);
        if (n <= 0) {
            return VIRTIO_BLK_S_IOERR;
        }
        done += (size_t)n;
        offset += n;
        size_t adv = (size_t)n;
        while (idx < vec_cnt && adv >= vecs[idx].iov_len) {
            adv -= vecs[idx].iov_len;
            idx++;
        }
        if (idx < vec_cnt && adv > 0) {
            vecs[idx].iov_base = (uint8_t *)vecs[idx].iov_base + adv;
            vecs[idx].iov_len -= adv;
        }
    }

    if (type == VIRTIO_BLK_T_IN) {
        *written_out = (uint32_t)done;
    }
    return VIRTIO_BLK_S_OK;
}

static uint8_t blk_handle_dwz(virtio_blk_t *blk, uint32_t type,
                              const virtio_iov_t *iov, size_t total_iov) {
    if (blk->read_only) {
        return VIRTIO_BLK_S_IOERR;
    }
    size_t out_total = virtio_iov_total(iov, total_iov, false);
    if (out_total < sizeof(virtio_blk_req_hdr_t) + sizeof(virtio_blk_dwz_t)) {
        return VIRTIO_BLK_S_IOERR;
    }
    size_t payload = out_total - sizeof(virtio_blk_req_hdr_t);
    if ((payload % sizeof(virtio_blk_dwz_t)) != 0) {
        return VIRTIO_BLK_S_IOERR;
    }
    size_t segs = payload / sizeof(virtio_blk_dwz_t);
    if (segs > VIRTIO_BLK_MAX_DWZ_SEG) {
        return VIRTIO_BLK_S_IOERR;
    }

    virtio_blk_dwz_t reqs[VIRTIO_BLK_MAX_DWZ_SEG];
    if (virtio_iov_read(iov, total_iov, sizeof(virtio_blk_req_hdr_t), reqs, payload) != payload) {
        return VIRTIO_BLK_S_IOERR;
    }

    for (size_t i = 0; i < segs; i++) {
        uint64_t sec = reqs[i].sector;
        uint64_t cnt = reqs[i].num_sectors;
        if (sec > blk->config.capacity || cnt > blk->config.capacity - sec) {
            return VIRTIO_BLK_S_IOERR;
        }
        uint64_t off = sec * VIRTIO_BLK_SECTOR_SIZE;
        uint64_t len = cnt * VIRTIO_BLK_SECTOR_SIZE;
        if (len == 0) {
            continue;
        }
        uint64_t aligned_start = (off + (VIRTIO_BLK_BLOCK_SIZE - 1)) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
        uint64_t aligned_end = (off + len) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
        int punched = -1;
        if (aligned_end > aligned_start &&
            (type == VIRTIO_BLK_T_DISCARD || (reqs[i].flags & VIRTIO_BLK_WRITE_ZEROES_UNMAP))) {
            struct fpunchhole ph = {
                .fp_offset = (off_t)aligned_start,
                .fp_length = (off_t)(aligned_end - aligned_start),
            };
            punched = fcntl(blk->fd, F_PUNCHHOLE, &ph);
            if (punched < 0 && type == VIRTIO_BLK_T_DISCARD &&
                errno != ENOTSUP && errno != EINVAL && errno != ENOSYS) {
                return VIRTIO_BLK_S_IOERR;
            }
        }
        if (type == VIRTIO_BLK_T_DISCARD) {
            continue;
        }
        if (punched == 0) {
            if ((aligned_start > off && virtio_write_zeros(blk->fd, (off_t)off, (off_t)(aligned_start - off)) < 0) ||
                (off + len > aligned_end &&
                 virtio_write_zeros(blk->fd, (off_t)aligned_end, (off_t)(off + len - aligned_end)) < 0)) {
                return VIRTIO_BLK_S_IOERR;
            }
        } else if (virtio_write_zeros(blk->fd, (off_t)off, (off_t)len) < 0) {
            return VIRTIO_BLK_S_IOERR;
        }
    }
    return VIRTIO_BLK_S_OK;
}

static void virtio_blk_run(void *ctx) {
    virtio_blk_t *blk = ctx;
    virtio_dev_t *dev = blk->dev;
    virtqueue_t *vq = &dev->queues[0];
    if (!virtqueue_active(dev, vq)) {
        return;
    }
    uint16_t old_used = vq->used_idx;
    bool pushed = false;

    for (;;) {
        virtio_iov_t iov[VIRTIO_BLK_MAX_IOV];
        uint16_t head = 0;
        size_t total_iov = 0;
        if (virtqueue_pop(dev, vq, iov, VIRTIO_BLK_MAX_IOV, &head, &total_iov) <= 0) {
            break;
        }

        virtio_blk_req_hdr_t hdr = {0};
        uint8_t status = VIRTIO_BLK_S_IOERR;
        uint32_t data_written = 0;

        size_t in_total = virtio_iov_total(iov, total_iov, true);
        if (virtio_iov_read(iov, total_iov, 0, &hdr, sizeof(hdr)) == sizeof(hdr) && in_total >= 1) {
            switch (hdr.type) {
            case VIRTIO_BLK_T_IN:
                status = blk_handle_rw(blk, hdr.type, hdr.sector, iov, total_iov, in_total, &data_written);
                break;
            case VIRTIO_BLK_T_OUT:
                status = blk_handle_rw(blk, hdr.type, hdr.sector, iov, total_iov, in_total, &data_written);
                if (status == VIRTIO_BLK_S_OK) {
                    atomic_store_explicit(&blk->dirty, true, memory_order_relaxed);
                }
                break;
            case VIRTIO_BLK_T_GET_ID: {
                size_t n = in_total - 1;
                if (n > VIRTIO_BLK_ID_BYTES) {
                    n = VIRTIO_BLK_ID_BYTES;
                }
                virtio_iov_write(iov, total_iov, 0, blk->serial, n);
                data_written = (uint32_t)n;
                status = VIRTIO_BLK_S_OK;
                break;
            }
            case VIRTIO_BLK_T_DISCARD:
            case VIRTIO_BLK_T_WRITE_ZEROES:
                status = blk_handle_dwz(blk, hdr.type, iov, total_iov);
                if (status == VIRTIO_BLK_S_OK) {
                    atomic_store_explicit(&blk->dirty, true, memory_order_relaxed);
                }
                break;
            default:
                status = VIRTIO_BLK_S_UNSUPP;
                break;
            }
        }

        if (in_total >= 1) {
            virtio_iov_write(iov, total_iov, in_total - 1, &status, 1);
        }
        virtqueue_push(dev, vq, head, data_written + (in_total >= 1 ? 1U : 0U));
        pushed = true;
    }

    if (pushed && virtqueue_should_notify(dev, vq, old_used)) {
        virtio_raise_irq(dev, VIRTIO_MMIO_INT_VRING);
    }
}

static uint64_t virtio_blk_get_features(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    uint64_t feats = VIRTIO_BLK_F_SIZE_MAX | VIRTIO_BLK_F_SEG_MAX |
                     VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_TOPOLOGY;
    if (blk->read_only) {
        feats |= VIRTIO_BLK_F_RO;
    } else {
        feats |= VIRTIO_BLK_F_DISCARD | VIRTIO_BLK_F_WRITE_ZEROES;
    }
    return feats;
}

static uint64_t virtio_blk_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_blk_t *blk = dev->priv;
    return virtio_config_read_buf(&blk->config, sizeof(blk->config), offset, size);
}

static void virtio_blk_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    (void)queue_idx;
    virtio_blk_t *blk = dev->priv;
    virtio_worker_kick(&blk->worker);
}

static void virtio_blk_reset(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    virtio_worker_wait_idle(&blk->worker);
}

void virtio_blk_flush_host(virtio_dev_t *dev) {
    if (!dev) {
        return;
    }
    virtio_blk_t *blk = dev->priv;
    virtio_worker_wait_idle(&blk->worker);
    if (atomic_exchange_explicit(&blk->dirty, false, memory_order_acq_rel) && blk_flush(blk->fd) != 0) {
        atomic_store_explicit(&blk->dirty, true, memory_order_relaxed);
    }
}

static void virtio_blk_destroy(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    virtio_worker_destroy(&blk->worker);
    if (atomic_load_explicit(&blk->dirty, memory_order_acquire)) {
        (void)blk_flush(blk->fd);
    }
    close(blk->fd);
    free(blk);
}

static const virtio_dev_ops_t virtio_blk_ops = {
    .get_features = virtio_blk_get_features,
    .read_config = virtio_blk_read_config,
    .queue_notify = virtio_blk_queue_notify,
    .reset = virtio_blk_reset,
    .destroy = virtio_blk_destroy,
};

virtio_dev_t *virtio_blk_create(vmm_machine_t *vm, uint32_t slot, int fd, bool read_only) {
    struct stat st;
    if (fstat(fd, &st) < 0) {
        return NULL;
    }
    int dup_fd = dup(fd);
    if (dup_fd < 0) {
        return NULL;
    }
    fcntl(dup_fd, F_SETFD, FD_CLOEXEC);

    virtio_blk_t *blk = calloc(1, sizeof(*blk));
    if (!blk) {
        close(dup_fd);
        return NULL;
    }
    blk->fd = dup_fd;
    blk->read_only = read_only;
    blk->config.capacity = (uint64_t)(st.st_size / VIRTIO_BLK_SECTOR_SIZE);
    blk->config.size_max = 1U << 20;
    blk->config.seg_max = VIRTIO_BLK_MAX_IOV - 2;
    blk->config.blk_size = VIRTIO_BLK_BLOCK_SIZE;
    blk->config.topology.min_io_size = 1;
    blk->config.num_queues = 1;
    blk->config.max_discard_sectors = 0x200000U;
    blk->config.max_discard_seg = VIRTIO_BLK_MAX_DWZ_SEG;
    blk->config.discard_sector_alignment = VIRTIO_BLK_BLOCK_SIZE / VIRTIO_BLK_SECTOR_SIZE;
    blk->config.max_write_zeroes_sectors = 0x200000U;
    blk->config.max_write_zeroes_seg = VIRTIO_BLK_MAX_DWZ_SEG;
    blk->config.write_zeroes_may_unmap = 1;
    snprintf(blk->serial, sizeof(blk->serial), "swipterm-blk%u", slot);
    virtio_worker_init(&blk->worker, virtio_blk_run, blk);

    const uint32_t qsizes[1] = { VIRTIO_BLK_QUEUE_SIZE };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_BLOCK, 1, qsizes, &virtio_blk_ops, blk);
    if (!dev) {
        virtio_worker_destroy(&blk->worker);
        close(dup_fd);
        free(blk);
        return NULL;
    }
    blk->dev = dev;
    if (virtio_worker_start(&blk->worker) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    return dev;
}
