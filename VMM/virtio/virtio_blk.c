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
    VIRTIO_BLK_F_FLUSH = 1ULL << 9,
    VIRTIO_BLK_F_TOPOLOGY = 1ULL << 10,
    VIRTIO_BLK_F_DISCARD = 1ULL << 13,
    VIRTIO_BLK_F_WRITE_ZEROES = 1ULL << 14,
};

enum : uint32_t {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1,
    VIRTIO_BLK_T_FLUSH = 4,
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
    VIRTIO_BLK_MAX_IOV = 128,
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
    pthread_t worker;
    bool worker_started;
    pthread_mutex_t lock;
    pthread_cond_t work_cond;
    pthread_cond_t idle_cond;
    bool pending;
    bool busy;
    bool stop;
    _Atomic bool dirty;
} virtio_blk_t;

static int blk_flush(int fd) {
    if (fcntl(fd, F_BARRIERFSYNC) == 0) {
        return 0;
    }
    return fsync(fd);
}

static uint8_t blk_punch_hole(int fd, uint64_t offset, uint64_t len) {
    uint64_t aligned_start = (offset + (VIRTIO_BLK_BLOCK_SIZE - 1)) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
    uint64_t aligned_end = (offset + len) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
    if (aligned_end <= aligned_start) {
        return VIRTIO_BLK_S_OK;
    }
    struct fpunchhole ph = {
        .fp_flags = 0,
        .reserved = 0,
        .fp_offset = (off_t)aligned_start,
        .fp_length = (off_t)(aligned_end - aligned_start),
    };
    if (fcntl(fd, F_PUNCHHOLE, &ph) < 0) {
        if (errno == ENOTSUP || errno == EINVAL || errno == ENOSYS) {
            return VIRTIO_BLK_S_OK;
        }
        return VIRTIO_BLK_S_IOERR;
    }
    return VIRTIO_BLK_S_OK;
}

static uint8_t blk_zero_range(int fd, uint64_t offset, uint64_t len) {
    static const uint8_t zeros[65536];
    while (len > 0) {
        size_t chunk = len > sizeof(zeros) ? sizeof(zeros) : (size_t)len;
        ssize_t n;
        do {
            n = pwrite(fd, zeros, chunk, (off_t)offset);
        } while (n < 0 && errno == EINTR);
        if (n <= 0) {
            return VIRTIO_BLK_S_IOERR;
        }
        offset += (uint64_t)n;
        len -= (uint64_t)n;
    }
    return VIRTIO_BLK_S_OK;
}

static uint8_t blk_handle_rw(virtio_blk_t *blk, uint32_t type, uint64_t sector,
                             const virtio_iov_t *iov, size_t total_iov,
                             uint32_t *written_out) {
    if (type == VIRTIO_BLK_T_OUT && blk->read_only) {
        return VIRTIO_BLK_S_IOERR;
    }
    struct iovec vecs[VIRTIO_BLK_MAX_IOV];
    int vec_cnt = 0;
    size_t data_bytes = 0;

    if (type == VIRTIO_BLK_T_IN) {
        size_t in_total = virtio_iov_total(iov, total_iov, true);
        if (in_total < 1) {
            return VIRTIO_BLK_S_IOERR;
        }
        size_t rem = in_total - 1;
        for (size_t i = 0; i < total_iov && rem > 0; i++) {
            if (!iov[i].write) {
                continue;
            }
            size_t take = iov[i].len < rem ? iov[i].len : rem;
            if (take > 0) {
                vecs[vec_cnt++] = (struct iovec){ .iov_base = iov[i].base, .iov_len = take };
                data_bytes += take;
                rem -= take;
            }
        }
    } else {
        size_t skip = sizeof(virtio_blk_req_hdr_t);
        for (size_t i = 0; i < total_iov; i++) {
            if (iov[i].write) {
                continue;
            }
            if (skip >= iov[i].len) {
                skip -= iov[i].len;
                continue;
            }
            size_t take = iov[i].len - skip;
            if (take > 0) {
                vecs[vec_cnt++] = (struct iovec){
                    .iov_base = (uint8_t *)iov[i].base + skip,
                    .iov_len = take,
                };
                data_bytes += take;
            }
            skip = 0;
        }
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
    if (segs > 16) {
        return VIRTIO_BLK_S_IOERR;
    }

    virtio_blk_dwz_t reqs[16] = {0};
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
        if (type == VIRTIO_BLK_T_DISCARD) {
            uint8_t st = blk_punch_hole(blk->fd, off, len);
            if (st != VIRTIO_BLK_S_OK) {
                return st;
            }
        } else {
            uint64_t aligned_start = (off + (VIRTIO_BLK_BLOCK_SIZE - 1)) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
            uint64_t aligned_end = (off + len) & ~(uint64_t)(VIRTIO_BLK_BLOCK_SIZE - 1);
            bool punched = false;
            if ((reqs[i].flags & VIRTIO_BLK_WRITE_ZEROES_UNMAP) && aligned_end > aligned_start) {
                struct fpunchhole ph = {
                    .fp_flags = 0,
                    .reserved = 0,
                    .fp_offset = (off_t)aligned_start,
                    .fp_length = (off_t)(aligned_end - aligned_start),
                };
                if (fcntl(blk->fd, F_PUNCHHOLE, &ph) == 0) {
                    punched = true;
                }
            }
            if (punched) {
                if (aligned_start > off) {
                    uint8_t st = blk_zero_range(blk->fd, off, aligned_start - off);
                    if (st != VIRTIO_BLK_S_OK) {
                        return st;
                    }
                }
                if (off + len > aligned_end) {
                    uint8_t st = blk_zero_range(blk->fd, aligned_end, (off + len) - aligned_end);
                    if (st != VIRTIO_BLK_S_OK) {
                        return st;
                    }
                }
            } else {
                uint8_t st = blk_zero_range(blk->fd, off, len);
                if (st != VIRTIO_BLK_S_OK) {
                    return st;
                }
            }
        }
    }
    return VIRTIO_BLK_S_OK;
}

static void *virtio_blk_worker(void *ctx) {
    virtio_blk_t *blk = ctx;
    virtio_dev_t *dev = blk->dev;
    virtqueue_t *vq = &dev->queues[0];

    pthread_mutex_lock(&blk->lock);
    for (;;) {
        while (!blk->pending && !blk->stop) {
            pthread_cond_wait(&blk->work_cond, &blk->lock);
        }
        if (!blk->pending && blk->stop) {
            break;
        }
        blk->pending = false;
        blk->busy = true;
        pthread_mutex_unlock(&blk->lock);

        if (atomic_load_explicit(&vq->ready, memory_order_acquire) &&
            (atomic_load_explicit(&dev->status, memory_order_acquire) & VIRTIO_STATUS_DRIVER_OK)) {
            uint16_t old_used = vq->used_idx;
            bool pushed = false;

            for (;;) {
                virtio_iov_t iov[VIRTIO_BLK_MAX_IOV];
                uint16_t head = 0;
                size_t out_cnt = 0;
                size_t in_cnt = 0;
                int popped = virtqueue_pop(dev, vq, iov, VIRTIO_BLK_MAX_IOV, &head, &out_cnt, &in_cnt);
                if (popped <= 0) {
                    break;
                }

                size_t total_iov = out_cnt + in_cnt;
                virtio_blk_req_hdr_t hdr = {0};
                uint8_t status = VIRTIO_BLK_S_IOERR;
                uint32_t data_written = 0;

                size_t in_total = virtio_iov_total(iov, total_iov, true);
                if (virtio_iov_read(iov, total_iov, 0, &hdr, sizeof(hdr)) == sizeof(hdr) && in_total >= 1) {
                    switch (hdr.type) {
                    case VIRTIO_BLK_T_IN:
                        status = blk_handle_rw(blk, hdr.type, hdr.sector, iov, total_iov, &data_written);
                        break;
                    case VIRTIO_BLK_T_OUT:
                        status = blk_handle_rw(blk, hdr.type, hdr.sector, iov, total_iov, &data_written);
                        if (status == VIRTIO_BLK_S_OK) {
                            atomic_store_explicit(&blk->dirty, true, memory_order_relaxed);
                        }
                        break;
                    case VIRTIO_BLK_T_FLUSH:
                        status = VIRTIO_BLK_S_OK;
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

        pthread_mutex_lock(&blk->lock);
        blk->busy = false;
        if (!blk->pending) {
            pthread_cond_broadcast(&blk->idle_cond);
        }
    }
    pthread_mutex_unlock(&blk->lock);
    return NULL;
}

static uint64_t virtio_blk_get_features(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    uint64_t feats = VIRTIO_BLK_F_SIZE_MAX | VIRTIO_BLK_F_SEG_MAX |
                     VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_FLUSH |
                     VIRTIO_BLK_F_TOPOLOGY;
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
    if (queue_idx != 0) {
        return;
    }
    virtio_blk_t *blk = dev->priv;
    pthread_mutex_lock(&blk->lock);
    if (!blk->stop) {
        blk->pending = true;
        pthread_cond_signal(&blk->work_cond);
    }
    pthread_mutex_unlock(&blk->lock);
}

static void virtio_blk_reset(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    pthread_mutex_lock(&blk->lock);
    while (blk->pending || blk->busy) {
        pthread_cond_wait(&blk->idle_cond, &blk->lock);
    }
    pthread_mutex_unlock(&blk->lock);
}

void virtio_blk_flush_host(virtio_dev_t *dev) {
    if (!dev) {
        return;
    }
    virtio_blk_reset(dev);
    virtio_blk_t *blk = dev->priv;
    if (atomic_exchange_explicit(&blk->dirty, false, memory_order_acq_rel) && blk_flush(blk->fd) != 0) {
        atomic_store_explicit(&blk->dirty, true, memory_order_relaxed);
    }
}

static void virtio_blk_destroy(virtio_dev_t *dev) {
    virtio_blk_t *blk = dev->priv;
    if (!blk) {
        return;
    }
    pthread_mutex_lock(&blk->lock);
    blk->stop = true;
    pthread_cond_signal(&blk->work_cond);
    pthread_mutex_unlock(&blk->lock);
    if (blk->worker_started) {
        pthread_join(blk->worker, NULL);
        blk->worker_started = false;
    }
    pthread_cond_destroy(&blk->idle_cond);
    pthread_cond_destroy(&blk->work_cond);
    pthread_mutex_destroy(&blk->lock);
    if (blk->fd >= 0) {
        if (atomic_load_explicit(&blk->dirty, memory_order_acquire)) {
            (void)blk_flush(blk->fd);
        }
        close(blk->fd);
    }
    free(blk);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_blk_ops = {
    .get_features = virtio_blk_get_features,
    .read_config = virtio_blk_read_config,
    .queue_notify = virtio_blk_queue_notify,
    .reset = virtio_blk_reset,
    .destroy = virtio_blk_destroy,
};

virtio_dev_t *virtio_blk_create(vmm_machine_t *vm, uint32_t slot, int fd, bool read_only) {
    if (!vm || fd < 0) {
        return NULL;
    }
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
    blk->config.topology.physical_block_exp = 3;
    blk->config.topology.min_io_size = 1;
    blk->config.writeback = 1;
    blk->config.num_queues = 1;
    blk->config.max_discard_sectors = 0x200000U;
    blk->config.max_discard_seg = 16;
    blk->config.discard_sector_alignment = VIRTIO_BLK_BLOCK_SIZE / VIRTIO_BLK_SECTOR_SIZE;
    blk->config.max_write_zeroes_sectors = 0x200000U;
    blk->config.max_write_zeroes_seg = 16;
    blk->config.write_zeroes_may_unmap = 1;
    snprintf(blk->serial, sizeof(blk->serial), "swipterm-blk%u", slot);

    pthread_mutex_init(&blk->lock, NULL);
    pthread_cond_init(&blk->work_cond, NULL);
    pthread_cond_init(&blk->idle_cond, NULL);

    const uint32_t qsizes[1] = { VIRTIO_BLK_QUEUE_SIZE };
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_BLOCK, 1, qsizes, &virtio_blk_ops, blk);
    if (!dev) {
        pthread_cond_destroy(&blk->idle_cond);
        pthread_cond_destroy(&blk->work_cond);
        pthread_mutex_destroy(&blk->lock);
        close(dup_fd);
        free(blk);
        return NULL;
    }
    blk->dev = dev;
    if (pthread_create(&blk->worker, NULL, virtio_blk_worker, blk) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    blk->worker_started = true;
    return dev;
}
