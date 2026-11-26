#include "virtio_internal.h"

#include "../host/gpu.h"
#include "virgl_hw.h"
#include "virglrenderer.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

enum : uint64_t {
    VIRTIO_GPU_F_VIRGL = 1ULL << 0,
    VIRTIO_GPU_F_RESOURCE_BLOB = 1ULL << 3,
    VIRTIO_GPU_F_CONTEXT_INIT = 1ULL << 4,
};

enum : uint32_t {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D = 0x0101,
    VIRTIO_GPU_CMD_RESOURCE_UNREF = 0x0102,
    VIRTIO_GPU_CMD_SET_SCANOUT = 0x0103,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH = 0x0104,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D = 0x0105,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING = 0x0106,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING = 0x0107,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO = 0x0108,
    VIRTIO_GPU_CMD_GET_CAPSET = 0x0109,
    VIRTIO_GPU_CMD_GET_EDID = 0x010a,
    VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID = 0x010b,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB = 0x010c,
    VIRTIO_GPU_CMD_SET_SCANOUT_BLOB = 0x010d,
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY = 0x0201,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE = 0x0202,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE = 0x0203,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D = 0x0204,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D = 0x0205,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D = 0x0206,
    VIRTIO_GPU_CMD_SUBMIT_3D = 0x0207,
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB = 0x0208,
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB = 0x0209,

    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO = 0x1101,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO = 0x1102,
    VIRTIO_GPU_RESP_OK_CAPSET = 0x1103,
    VIRTIO_GPU_RESP_OK_MAP_INFO = 0x1106,

    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY = 0x1201,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID = 0x1202,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID = 0x1203,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID = 0x1204,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER = 0x1205,

    VIRTIO_GPU_FLAG_FENCE = 1U << 0,
    VIRTIO_GPU_FLAG_INFO_RING_IDX = 1U << 1,
};

enum {
    GPU_QUEUE_CTRL = 0,
    GPU_QUEUE_CURSOR = 1,
    GPU_QUEUE_COUNT = 2,
    GPU_QUEUE_SIZE = 256,
    GPU_MAX_IOV = 1024,
    GPU_MAX_REQUEST = 64 << 20,
    GPU_MAX_RESPONSE = 64 << 10,
    GPU_MAX_SCANOUTS = 16,
    GPU_MAX_BACKING_ENTRIES = 1 << 20,
    GPU_RESOURCE_BUCKETS = 1024,
    GPU_CONTEXT_BUCKETS = 64,
    GPU_SHM_ID_HOST_VISIBLE = 1,
    GPU_CAPSET_VENUS = 4,
    GPU_CONTEXT_INIT_CAPSET_MASK = 0xff,
    GPU_KICK_CTRL = 1U << GPU_QUEUE_CTRL,
    GPU_KICK_CURSOR = 1U << GPU_QUEUE_CURSOR,
};

typedef struct gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t ring_idx;
    uint8_t padding[3];
} gpu_ctrl_hdr_t;
static_assert(sizeof(gpu_ctrl_hdr_t) == 24);

typedef struct gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} gpu_mem_entry_t;

typedef struct gpu_resource_unref {
    gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t padding;
} gpu_resource_cmd_t;

typedef struct gpu_attach_backing {
    gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} gpu_attach_backing_t;

typedef struct gpu_get_capset_info {
    gpu_ctrl_hdr_t hdr;
    uint32_t capset_index;
    uint32_t padding;
} gpu_get_capset_info_t;

typedef struct gpu_resp_capset_info {
    gpu_ctrl_hdr_t hdr;
    uint32_t capset_id;
    uint32_t capset_max_version;
    uint32_t capset_max_size;
    uint32_t padding;
} gpu_resp_capset_info_t;

typedef struct gpu_get_capset {
    gpu_ctrl_hdr_t hdr;
    uint32_t capset_id;
    uint32_t capset_version;
} gpu_get_capset_t;

typedef struct gpu_ctx_create {
    gpu_ctrl_hdr_t hdr;
    uint32_t nlen;
    uint32_t context_init;
    char debug_name[64];
} gpu_ctx_create_t;

typedef struct gpu_resource_create_3d {
    gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t target;
    uint32_t format;
    uint32_t bind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t padding;
} gpu_resource_create_3d_t;

typedef struct gpu_transfer_host_3d {
    gpu_ctrl_hdr_t hdr;
    struct virgl_box box;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t level;
    uint32_t stride;
    uint32_t layer_stride;
} gpu_transfer_host_3d_t;

typedef struct gpu_cmd_submit {
    gpu_ctrl_hdr_t hdr;
    uint32_t size;
    uint32_t padding;
} gpu_cmd_submit_t;

typedef struct gpu_resource_create_blob {
    gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t blob_mem;
    uint32_t blob_flags;
    uint32_t nr_entries;
    uint64_t blob_id;
    uint64_t size;
} gpu_resource_create_blob_t;

typedef struct gpu_resource_map_blob {
    gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t padding;
    uint64_t offset;
} gpu_resource_map_blob_t;

typedef struct gpu_resp_map_info {
    gpu_ctrl_hdr_t hdr;
    uint32_t map_info;
    uint32_t padding;
} gpu_resp_map_info_t;

typedef struct gpu_display_one {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t enabled;
    uint32_t flags;
} gpu_display_one_t;

typedef struct gpu_resp_display_info {
    gpu_ctrl_hdr_t hdr;
    gpu_display_one_t pmodes[GPU_MAX_SCANOUTS];
} gpu_resp_display_info_t;

typedef struct gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
} gpu_config_t;

typedef struct gpu_resource {
    uint32_t id;
    struct iovec *iov;
    uint32_t iov_count;
    bool mapped;
    uint64_t map_offset;
    uint64_t map_size;
    struct gpu_resource *next;
} gpu_resource_t;

typedef struct gpu_context {
    uint32_t id;
    struct gpu_context *next;
} gpu_context_t;

typedef struct gpu_pending {
    uint16_t head;
    uint32_t len;
    uint32_t ctx_id;
    uint32_t ring_idx;
    uint64_t fence_id;
    bool signaled;
    struct gpu_pending *next;
} gpu_pending_t;

typedef struct gpu_retired {
    uint32_t ctx_id;
    uint32_t ring_idx;
    uint64_t fence_id;
    struct gpu_retired *next;
} gpu_retired_t;

typedef struct gpu_batch {
    uint32_t queue;
    uint16_t old_used;
    bool pushed;
} gpu_batch_t;

typedef struct virtio_gpu {
    virtio_dev_t *dev;
    gpu_config_t config;

    pthread_t worker;
    bool worker_started;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool stop;
    uint32_t kicks;
    gpu_retired_t *retired;

    pthread_mutex_t vq_lock;
    uint64_t generation;

    uint64_t seen_generation;
    gpu_resource_t *resources[GPU_RESOURCE_BUCKETS];
    gpu_context_t *contexts[GPU_CONTEXT_BUCKETS];
    gpu_pending_t *timeline;
    gpu_pending_t *fifo;
    virtio_iov_t iov[GPU_MAX_IOV];
    uint8_t *request;
    size_t request_cap;
    uint8_t response[GPU_MAX_RESPONSE];
} virtio_gpu_t;

static gpu_resource_t **gpu_resource_slot(virtio_gpu_t *gpu, uint32_t id) {
    gpu_resource_t **link = &gpu->resources[id % GPU_RESOURCE_BUCKETS];
    while (*link && (*link)->id != id) {
        link = &(*link)->next;
    }
    return link;
}

static gpu_context_t **gpu_context_slot(virtio_gpu_t *gpu, uint32_t id) {
    gpu_context_t **link = &gpu->contexts[id % GPU_CONTEXT_BUCKETS];
    while (*link && (*link)->id != id) {
        link = &(*link)->next;
    }
    return link;
}

static void gpu_on_retire(void *owner, uint32_t ctx_id, uint32_t ring_idx, uint64_t fence_id) {
    virtio_gpu_t *gpu = owner;
    gpu_retired_t *retired = malloc(sizeof(*retired));
    if (!retired) {
        return;
    }
    *retired = (gpu_retired_t){.ctx_id = ctx_id, .ring_idx = ring_idx, .fence_id = fence_id};
    pthread_mutex_lock(&gpu->lock);
    retired->next = gpu->retired;
    gpu->retired = retired;
    pthread_cond_signal(&gpu->cond);
    pthread_mutex_unlock(&gpu->lock);
}

static int gpu_pop(virtio_gpu_t *gpu, uint32_t queue, uint16_t *head, size_t *out_count, size_t *in_count) {
    pthread_mutex_lock(&gpu->vq_lock);
    int rc = gpu->generation == gpu->seen_generation
                 ? virtqueue_pop(gpu->dev, &gpu->dev->queues[queue], gpu->iov, GPU_MAX_IOV, head, out_count, in_count)
                 : 0;
    pthread_mutex_unlock(&gpu->vq_lock);
    return rc;
}

static void gpu_complete(virtio_gpu_t *gpu, gpu_batch_t *batch, uint16_t head, uint32_t len) {
    virtqueue_t *vq = &gpu->dev->queues[batch->queue];
    pthread_mutex_lock(&gpu->vq_lock);
    if (gpu->generation == gpu->seen_generation) {
        if (!batch->pushed) {
            batch->old_used = vq->used_idx;
            batch->pushed = true;
        }
        virtqueue_push(gpu->dev, vq, head, len);
    }
    pthread_mutex_unlock(&gpu->vq_lock);
}

static void gpu_flush(virtio_gpu_t *gpu, gpu_batch_t *batch) {
    if (!batch->pushed) {
        return;
    }
    batch->pushed = false;
    pthread_mutex_lock(&gpu->vq_lock);
    bool notify = gpu->generation == gpu->seen_generation &&
                  virtqueue_should_notify(gpu->dev, &gpu->dev->queues[batch->queue], batch->old_used);
    pthread_mutex_unlock(&gpu->vq_lock);
    if (notify) {
        virtio_raise_irq(gpu->dev, VIRTIO_MMIO_INT_VRING);
    }
}

static void gpu_flush_fifo(virtio_gpu_t *gpu, gpu_batch_t *batch) {
    while (gpu->fifo && gpu->fifo->signaled) {
        gpu_pending_t *pending = gpu->fifo;
        gpu->fifo = pending->next;
        gpu_complete(gpu, batch, pending->head, pending->len);
        free(pending);
    }
}

static void gpu_retire(virtio_gpu_t *gpu, gpu_batch_t *batch, uint32_t ctx_id, uint32_t ring_idx,
                       uint64_t fence_id, bool any_fence) {
    for (gpu_pending_t **link = &gpu->timeline; *link;) {
        gpu_pending_t *pending = *link;
        if (pending->ctx_id == ctx_id && (any_fence || (pending->ring_idx == ring_idx && pending->fence_id <= fence_id))) {
            *link = pending->next;
            gpu_complete(gpu, batch, pending->head, pending->len);
            free(pending);
        } else {
            link = &pending->next;
        }
    }
    for (gpu_pending_t *pending = gpu->fifo; pending; pending = pending->next) {
        if (!pending->signaled && pending->ctx_id == ctx_id &&
            (any_fence || (pending->ring_idx == ring_idx && pending->fence_id <= fence_id))) {
            pending->signaled = true;
        }
    }
    gpu_flush_fifo(gpu, batch);
}

static struct iovec *gpu_build_iov(virtio_gpu_t *gpu, const uint8_t *entries, uint32_t count) {
    struct iovec *iov = calloc(count ? count : 1, sizeof(*iov));
    if (!iov) {
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        gpu_mem_entry_t entry;
        memcpy(&entry, entries + (size_t)i * sizeof(entry), sizeof(entry));
        void *hva = mmu_gpa_to_hva(gpu->dev->vm, entry.addr, entry.length);
        if (!hva) {
            free(iov);
            return NULL;
        }
        iov[i] = (struct iovec){.iov_base = hva, .iov_len = entry.length};
    }
    return iov;
}

static void gpu_resource_unmap(gpu_resource_t *res) {
    gpu_lock();
    virgl_renderer_resource_unmap(res->id);
    gpu_unlock();
    res->mapped = false;
}

static void gpu_resource_free(gpu_resource_t *res) {
    gpu_lock();
    virgl_renderer_resource_unref(res->id);
    gpu_unlock();
    free(res->iov);
    free(res);
}

static void gpu_context_free(gpu_context_t *ctx) {
    gpu_lock();
    virgl_renderer_context_destroy(ctx->id);
    gpu_unlock();
    free(ctx);
}

static void gpu_reset_state(virtio_gpu_t *gpu) {
    for (gpu_pending_t *pending = gpu->timeline; pending;) {
        gpu_pending_t *next = pending->next;
        free(pending);
        pending = next;
    }
    for (gpu_pending_t *pending = gpu->fifo; pending;) {
        gpu_pending_t *next = pending->next;
        free(pending);
        pending = next;
    }
    gpu->timeline = NULL;
    gpu->fifo = NULL;

    for (uint32_t i = 0; i < GPU_CONTEXT_BUCKETS; i++) {
        while (gpu->contexts[i]) {
            gpu_context_t *ctx = gpu->contexts[i];
            gpu->contexts[i] = ctx->next;
            gpu_context_free(ctx);
        }
    }

    bool flush = false;
    for (uint32_t i = 0; i < GPU_RESOURCE_BUCKETS; i++) {
        for (gpu_resource_t *res = gpu->resources[i]; res; res = res->next) {
            if (res->mapped) {
                mmu_shm_unmap(gpu->dev->vm, res->map_offset, res->map_size);
                flush = true;
            }
        }
    }
    if (flush) {
        mmu_host_flush(gpu->dev->vm);
    }
    for (uint32_t i = 0; i < GPU_RESOURCE_BUCKETS; i++) {
        while (gpu->resources[i]) {
            gpu_resource_t *res = gpu->resources[i];
            gpu->resources[i] = res->next;
            if (res->mapped) {
                gpu_resource_unmap(res);
            }
            gpu_resource_free(res);
        }
    }
    gpu->config.events_read = 0;
}

static uint32_t gpu_nodata(uint8_t *response, uint32_t type) {
    gpu_ctrl_hdr_t hdr = {.type = type};
    memcpy(response, &hdr, sizeof(hdr));
    return sizeof(hdr);
}

static uint32_t gpu_cmd_get_display_info(uint8_t *response) {
    const gpu_resp_display_info_t resp = {.hdr.type = VIRTIO_GPU_RESP_OK_DISPLAY_INFO};
    memcpy(response, &resp, sizeof(resp));
    return sizeof(resp);
}

static uint32_t gpu_cmd_get_capset_info(const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_get_capset_info_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_get_capset_info_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    if (cmd.capset_index != 0) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    uint32_t max_version = 0;
    uint32_t max_size = 0;
    gpu_lock();
    virgl_renderer_get_cap_set(GPU_CAPSET_VENUS, &max_version, &max_size);
    gpu_unlock();
    gpu_resp_capset_info_t resp = {
        .hdr.type = VIRTIO_GPU_RESP_OK_CAPSET_INFO,
        .capset_id = GPU_CAPSET_VENUS,
        .capset_max_version = max_version,
        .capset_max_size = max_size,
    };
    memcpy(response, &resp, sizeof(resp));
    return sizeof(resp);
}

static uint32_t gpu_cmd_get_capset(const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_get_capset_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_get_capset_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    uint32_t max_version = 0;
    uint32_t max_size = 0;
    gpu_lock();
    virgl_renderer_get_cap_set(cmd.capset_id, &max_version, &max_size);
    if (cmd.capset_id != GPU_CAPSET_VENUS || max_size == 0 ||
        max_size > GPU_MAX_RESPONSE - sizeof(gpu_ctrl_hdr_t)) {
        gpu_unlock();
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    memset(response + sizeof(gpu_ctrl_hdr_t), 0, max_size);
    virgl_renderer_fill_caps(cmd.capset_id, cmd.capset_version, response + sizeof(gpu_ctrl_hdr_t));
    gpu_unlock();
    gpu_nodata(response, VIRTIO_GPU_RESP_OK_CAPSET);
    return (uint32_t)sizeof(gpu_ctrl_hdr_t) + max_size;
}

static uint32_t gpu_cmd_ctx_create(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_ctx_create_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_ctx_create_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    if ((cmd.context_init & GPU_CONTEXT_INIT_CAPSET_MASK) != GPU_CAPSET_VENUS ||
        cmd.nlen > sizeof(cmd.debug_name)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_context_t **slot = gpu_context_slot(gpu, cmd.hdr.ctx_id);
    if (*slot) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID);
    }
    gpu_context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY);
    }
    ctx->id = cmd.hdr.ctx_id;
    gpu_lock();
    int rc = virgl_renderer_context_create_with_flags(ctx->id, GPU_CAPSET_VENUS, cmd.nlen, cmd.debug_name);
    gpu_unlock();
    if (rc != 0) {
        free(ctx);
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
    *slot = ctx;
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_ctx_destroy(virtio_gpu_t *gpu, gpu_batch_t *batch, uint32_t ctx_id, uint8_t *response) {
    gpu_context_t **slot = gpu_context_slot(gpu, ctx_id);
    gpu_context_t *ctx = *slot;
    if (!ctx) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID);
    }
    *slot = ctx->next;
    gpu_retire(gpu, batch, ctx->id, 0, 0, true);
    gpu_context_free(ctx);
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_ctx_resource(virtio_gpu_t *gpu, const uint8_t *request, size_t len, bool attach,
                                     uint8_t *response) {
    if (len < sizeof(gpu_resource_cmd_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_cmd_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_context_t *ctx = *gpu_context_slot(gpu, cmd.hdr.ctx_id);
    if (!ctx) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID);
    }
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    gpu_lock();
    if (attach) {
        virgl_renderer_ctx_attach_resource((int)ctx->id, (int)res->id);
    } else {
        virgl_renderer_ctx_detach_resource((int)ctx->id, (int)res->id);
    }
    gpu_unlock();
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_resource_create_3d(virtio_gpu_t *gpu, const uint8_t *request, size_t len,
                                           uint8_t *response) {
    if (len < sizeof(gpu_resource_create_3d_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_create_3d_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t **slot = gpu_resource_slot(gpu, cmd.resource_id);
    if (*slot) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    gpu_resource_t *res = calloc(1, sizeof(*res));
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY);
    }
    res->id = cmd.resource_id;
    struct virgl_renderer_resource_create_args args = {
        .handle = res->id,
        .target = cmd.target,
        .format = cmd.format,
        .bind = cmd.bind,
        .width = cmd.width,
        .height = cmd.height,
        .depth = cmd.depth,
        .array_size = cmd.array_size,
        .last_level = cmd.last_level,
        .nr_samples = cmd.nr_samples,
        .flags = cmd.flags,
    };
    gpu_lock();
    int rc = virgl_renderer_resource_create(&args, NULL, 0);
    gpu_unlock();
    if (rc != 0) {
        free(res);
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
    *slot = res;
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_resource_create_blob(virtio_gpu_t *gpu, const uint8_t *request, size_t len,
                                             uint8_t *response) {
    if (len < sizeof(gpu_resource_create_blob_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_create_blob_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t **slot = gpu_resource_slot(gpu, cmd.resource_id);
    if (*slot) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    if (cmd.nr_entries > GPU_MAX_BACKING_ENTRIES ||
        len < sizeof(cmd) + (size_t)cmd.nr_entries * sizeof(gpu_mem_entry_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    uint32_t host_ctx = 0;
    if (cmd.blob_mem != VIRGL_RENDERER_BLOB_MEM_GUEST) {
        gpu_context_t *ctx = *gpu_context_slot(gpu, cmd.hdr.ctx_id);
        if (!ctx) {
            return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID);
        }
        host_ctx = ctx->id;
    }
    gpu_resource_t *res = calloc(1, sizeof(*res));
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY);
    }
    if (cmd.nr_entries > 0) {
        res->iov = gpu_build_iov(gpu, request + sizeof(cmd), cmd.nr_entries);
        if (!res->iov) {
            free(res);
            return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
        }
        res->iov_count = cmd.nr_entries;
    }
    res->id = cmd.resource_id;
    const struct virgl_renderer_resource_create_blob_args args = {
        .res_handle = res->id,
        .ctx_id = host_ctx,
        .blob_mem = cmd.blob_mem,
        .blob_flags = cmd.blob_flags,
        .blob_id = cmd.blob_id,
        .size = cmd.size,
        .iovecs = res->iov,
        .num_iovs = res->iov_count,
    };
    gpu_lock();
    int rc = virgl_renderer_resource_create_blob(&args);
    gpu_unlock();
    if (rc != 0) {
        free(res->iov);
        free(res);
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
    *slot = res;
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_resource_unref(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_resource_cmd_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_cmd_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t **slot = gpu_resource_slot(gpu, cmd.resource_id);
    gpu_resource_t *res = *slot;
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    *slot = res->next;
    if (res->mapped) {
        mmu_shm_unmap(gpu->dev->vm, res->map_offset, res->map_size);
        mmu_host_flush(gpu->dev->vm);
        gpu_resource_unmap(res);
    }
    gpu_resource_free(res);
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_attach_backing(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_attach_backing_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_attach_backing_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    if (res->iov || cmd.nr_entries == 0 || cmd.nr_entries > GPU_MAX_BACKING_ENTRIES ||
        len < sizeof(cmd) + (size_t)cmd.nr_entries * sizeof(gpu_mem_entry_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    struct iovec *iov = gpu_build_iov(gpu, request + sizeof(cmd), cmd.nr_entries);
    if (!iov) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_lock();
    int rc = virgl_renderer_resource_attach_iov((int)res->id, iov, (int)cmd.nr_entries);
    gpu_unlock();
    if (rc != 0) {
        free(iov);
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
    res->iov = iov;
    res->iov_count = cmd.nr_entries;
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_detach_backing(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_resource_cmd_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_cmd_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    gpu_lock();
    virgl_renderer_resource_detach_iov((int)res->id, NULL, NULL);
    gpu_unlock();
    free(res->iov);
    res->iov = NULL;
    res->iov_count = 0;
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_cmd_transfer_3d(virtio_gpu_t *gpu, const uint8_t *request, size_t len, bool to_host,
                                    uint8_t *response) {
    if (len < sizeof(gpu_transfer_host_3d_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_transfer_host_3d_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    gpu_context_t *ctx = *gpu_context_slot(gpu, cmd.hdr.ctx_id);
    uint32_t host_ctx = ctx ? ctx->id : 0;
    gpu_lock();
    int rc = to_host ? virgl_renderer_transfer_write_iov(res->id, host_ctx, (int)cmd.level, cmd.stride,
                                                         cmd.layer_stride, &cmd.box, cmd.offset, NULL, 0)
                     : virgl_renderer_transfer_read_iov(res->id, host_ctx, cmd.level, cmd.stride,
                                                        cmd.layer_stride, &cmd.box, cmd.offset, NULL, 0);
    gpu_unlock();
    return gpu_nodata(response, rc == 0 ? VIRTIO_GPU_RESP_OK_NODATA : VIRTIO_GPU_RESP_ERR_UNSPEC);
}

static uint32_t gpu_cmd_submit_3d(virtio_gpu_t *gpu, uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_cmd_submit_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_cmd_submit_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    if (cmd.size > len - sizeof(cmd) || (cmd.size & 3U) != 0) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_context_t *ctx = *gpu_context_slot(gpu, cmd.hdr.ctx_id);
    if (!ctx) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID);
    }
    gpu_lock();
    int rc = virgl_renderer_submit_cmd(request + sizeof(cmd), (int)ctx->id, (int)(cmd.size / 4));
    gpu_unlock();
    return gpu_nodata(response, rc == 0 ? VIRTIO_GPU_RESP_OK_NODATA : VIRTIO_GPU_RESP_ERR_UNSPEC);
}

static uint32_t gpu_cmd_map_blob(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_resource_map_blob_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_map_blob_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    const vmm_shm_window_t *window = &gpu->dev->vm->gpu_shm;
    if (res->mapped || (cmd.offset & VMM_PAGE_MASK) != 0 || cmd.offset >= window->size) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    void *map = NULL;
    uint64_t size = 0;
    uint32_t map_info = 0;
    gpu_lock();
    int rc = virgl_renderer_resource_map(res->id, &map, &size);
    if (rc == 0 && (virgl_renderer_resource_get_map_info(res->id, &map_info) != 0 ||
                    size > window->size - cmd.offset)) {
        virgl_renderer_resource_unmap(res->id);
        rc = -1;
    }
    gpu_unlock();
    if (rc != 0) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
    mmu_shm_map(gpu->dev->vm, cmd.offset, map, size);
    res->mapped = true;
    res->map_offset = cmd.offset;
    res->map_size = size;
    gpu_resp_map_info_t resp = {
        .hdr.type = VIRTIO_GPU_RESP_OK_MAP_INFO,
        .map_info = map_info,
    };
    memcpy(response, &resp, sizeof(resp));
    return sizeof(resp);
}

static uint32_t gpu_cmd_unmap_blob(virtio_gpu_t *gpu, const uint8_t *request, size_t len, uint8_t *response) {
    if (len < sizeof(gpu_resource_cmd_t)) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    gpu_resource_cmd_t cmd;
    memcpy(&cmd, request, sizeof(cmd));
    gpu_resource_t *res = *gpu_resource_slot(gpu, cmd.resource_id);
    if (!res) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID);
    }
    if (!res->mapped) {
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
    }
    mmu_shm_unmap(gpu->dev->vm, res->map_offset, res->map_size);
    mmu_host_flush(gpu->dev->vm);
    gpu_resource_unmap(res);
    return gpu_nodata(response, VIRTIO_GPU_RESP_OK_NODATA);
}

static uint32_t gpu_handle(virtio_gpu_t *gpu, gpu_batch_t *batch, const gpu_ctrl_hdr_t *hdr, uint8_t *request,
                           size_t len, uint8_t *response) {
    switch (hdr->type) {
    case VIRTIO_GPU_CMD_GET_DISPLAY_INFO:
        return gpu_cmd_get_display_info(response);
    case VIRTIO_GPU_CMD_GET_CAPSET_INFO:
        return gpu_cmd_get_capset_info(request, len, response);
    case VIRTIO_GPU_CMD_GET_CAPSET:
        return gpu_cmd_get_capset(request, len, response);
    case VIRTIO_GPU_CMD_CTX_CREATE:
        return gpu_cmd_ctx_create(gpu, request, len, response);
    case VIRTIO_GPU_CMD_CTX_DESTROY:
        return gpu_cmd_ctx_destroy(gpu, batch, hdr->ctx_id, response);
    case VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE:
        return gpu_cmd_ctx_resource(gpu, request, len, true, response);
    case VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE:
        return gpu_cmd_ctx_resource(gpu, request, len, false, response);
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_3D:
        return gpu_cmd_resource_create_3d(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB:
        return gpu_cmd_resource_create_blob(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_UNREF:
        return gpu_cmd_resource_unref(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING:
        return gpu_cmd_attach_backing(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING:
        return gpu_cmd_detach_backing(gpu, request, len, response);
    case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D:
        return gpu_cmd_transfer_3d(gpu, request, len, true, response);
    case VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D:
        return gpu_cmd_transfer_3d(gpu, request, len, false, response);
    case VIRTIO_GPU_CMD_SUBMIT_3D:
        return gpu_cmd_submit_3d(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB:
        return gpu_cmd_map_blob(gpu, request, len, response);
    case VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB:
        return gpu_cmd_unmap_blob(gpu, request, len, response);
    case VIRTIO_GPU_CMD_SET_SCANOUT:
    case VIRTIO_GPU_CMD_SET_SCANOUT_BLOB:
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID);
    default:
        return gpu_nodata(response, VIRTIO_GPU_RESP_ERR_UNSPEC);
    }
}

static void gpu_fence(virtio_gpu_t *gpu, gpu_batch_t *batch, const gpu_ctrl_hdr_t *hdr, bool ok, uint16_t head,
                      uint32_t len) {
    gpu_pending_t *pending = ok ? calloc(1, sizeof(*pending)) : NULL;
    if (!pending) {
        gpu_complete(gpu, batch, head, len);
        return;
    }
    pending->head = head;
    pending->len = len;
    pending->fence_id = hdr->fence_id;
    gpu_context_t *ctx = *gpu_context_slot(gpu, hdr->ctx_id);

    if (hdr->flags & VIRTIO_GPU_FLAG_INFO_RING_IDX) {
        int rc = -1;
        if (ctx) {
            gpu_lock();
            rc = virgl_renderer_context_create_fence(ctx->id, VIRGL_RENDERER_FENCE_FLAG_MERGEABLE,
                                                     hdr->ring_idx, hdr->fence_id);
            gpu_unlock();
        }
        if (rc != 0) {
            free(pending);
            gpu_complete(gpu, batch, head, len);
            return;
        }
        pending->ctx_id = ctx->id;
        pending->ring_idx = hdr->ring_idx;
        pending->next = gpu->timeline;
        gpu->timeline = pending;
        return;
    }

    pending->signaled = true;
    if (hdr->type == VIRTIO_GPU_CMD_SUBMIT_3D && ctx) {
        gpu_lock();
        int rc = virgl_renderer_context_create_fence(ctx->id, VIRGL_RENDERER_FENCE_FLAG_MERGEABLE, 0,
                                                     hdr->fence_id);
        gpu_unlock();
        if (rc == 0) {
            pending->ctx_id = ctx->id;
            pending->signaled = false;
        }
    }
    gpu_pending_t **tail = &gpu->fifo;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = pending;
    gpu_flush_fifo(gpu, batch);
}

static void gpu_process_ctrl(virtio_gpu_t *gpu, gpu_batch_t *batch) {
    for (;;) {
        uint16_t head = 0;
        size_t out_count = 0;
        size_t in_count = 0;
        if (gpu_pop(gpu, GPU_QUEUE_CTRL, &head, &out_count, &in_count) <= 0) {
            break;
        }
        size_t count = out_count + in_count;
        size_t len = virtio_iov_total(gpu->iov, count, false);
        uint32_t response_len;
        gpu_ctrl_hdr_t hdr = {0};
        bool ok = false;
        if (len < sizeof(hdr) || len > GPU_MAX_REQUEST) {
            response_len = gpu_nodata(gpu->response, VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER);
        } else {
            if (len > gpu->request_cap) {
                uint8_t *request = realloc(gpu->request, len);
                if (request) {
                    gpu->request = request;
                    gpu->request_cap = len;
                }
            }
            if (len > gpu->request_cap) {
                response_len = gpu_nodata(gpu->response, VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY);
            } else {
                virtio_iov_read(gpu->iov, count, 0, gpu->request, len);
                memcpy(&hdr, gpu->request, sizeof(hdr));
                response_len = gpu_handle(gpu, batch, &hdr, gpu->request, len, gpu->response);
                gpu_ctrl_hdr_t response;
                memcpy(&response, gpu->response, sizeof(response));
                ok = response.type < VIRTIO_GPU_RESP_ERR_UNSPEC;
                if (hdr.flags & VIRTIO_GPU_FLAG_FENCE) {
                    response.flags |= VIRTIO_GPU_FLAG_FENCE | (hdr.flags & VIRTIO_GPU_FLAG_INFO_RING_IDX);
                    response.fence_id = hdr.fence_id;
                    response.ctx_id = hdr.ctx_id;
                    response.ring_idx = hdr.ring_idx;
                    memcpy(gpu->response, &response, sizeof(response));
                }
            }
        }
        uint32_t written = 0;
        pthread_mutex_lock(&gpu->vq_lock);
        if (gpu->generation == gpu->seen_generation) {
            written = (uint32_t)virtio_iov_write(gpu->iov, count, 0, gpu->response, response_len);
        }
        pthread_mutex_unlock(&gpu->vq_lock);
        if (hdr.flags & VIRTIO_GPU_FLAG_FENCE) {
            gpu_fence(gpu, batch, &hdr, ok, head, written);
        } else {
            gpu_complete(gpu, batch, head, written);
        }
    }
}

static void gpu_process_cursor(virtio_gpu_t *gpu, gpu_batch_t *batch) {
    for (;;) {
        uint16_t head = 0;
        size_t out_count = 0;
        size_t in_count = 0;
        if (gpu_pop(gpu, GPU_QUEUE_CURSOR, &head, &out_count, &in_count) <= 0) {
            break;
        }
        gpu_complete(gpu, batch, head, 0);
    }
}

static void *gpu_worker(void *arg) {
    virtio_gpu_t *gpu = arg;
    pthread_setname_np("virtio-gpu");
    for (;;) {
        pthread_mutex_lock(&gpu->lock);
        while (!gpu->stop && !gpu->kicks && !gpu->retired) {
            pthread_cond_wait(&gpu->cond, &gpu->lock);
        }
        if (gpu->stop) {
            pthread_mutex_unlock(&gpu->lock);
            break;
        }
        uint32_t kicks = gpu->kicks;
        gpu_retired_t *retired = gpu->retired;
        gpu->kicks = 0;
        gpu->retired = NULL;
        pthread_mutex_unlock(&gpu->lock);

        pthread_mutex_lock(&gpu->vq_lock);
        uint64_t generation = gpu->generation;
        pthread_mutex_unlock(&gpu->vq_lock);
        if (generation != gpu->seen_generation) {
            gpu_reset_state(gpu);
            gpu->seen_generation = generation;
        }

        gpu_batch_t ctrl = {.queue = GPU_QUEUE_CTRL};
        while (retired) {
            gpu_retired_t *next = retired->next;
            gpu_retire(gpu, &ctrl, retired->ctx_id, retired->ring_idx, retired->fence_id, false);
            free(retired);
            retired = next;
        }
        if (kicks & GPU_KICK_CTRL) {
            gpu_process_ctrl(gpu, &ctrl);
        }
        gpu_flush(gpu, &ctrl);
        if (kicks & GPU_KICK_CURSOR) {
            gpu_batch_t cursor = {.queue = GPU_QUEUE_CURSOR};
            gpu_process_cursor(gpu, &cursor);
            gpu_flush(gpu, &cursor);
        }
    }
    gpu_reset_state(gpu);
    return NULL;
}

static uint64_t virtio_gpu_get_features(virtio_dev_t *dev) {
    (void)dev;
    return VIRTIO_GPU_F_VIRGL | VIRTIO_GPU_F_RESOURCE_BLOB | VIRTIO_GPU_F_CONTEXT_INIT;
}

static uint64_t virtio_gpu_read_config(virtio_dev_t *dev, uint32_t offset, uint32_t size) {
    virtio_gpu_t *gpu = dev->priv;
    return virtio_config_read_buf(&gpu->config, sizeof(gpu->config), offset, size);
}

static void virtio_gpu_write_config(virtio_dev_t *dev, uint32_t offset, uint32_t size, uint64_t val) {
    virtio_gpu_t *gpu = dev->priv;
    if (offset == offsetof(gpu_config_t, events_clear) && size == 4) {
        gpu->config.events_read &= ~(uint32_t)val;
    }
}

static void virtio_gpu_queue_notify(virtio_dev_t *dev, uint32_t queue_idx) {
    virtio_gpu_t *gpu = dev->priv;
    pthread_mutex_lock(&gpu->lock);
    gpu->kicks |= 1U << queue_idx;
    pthread_cond_signal(&gpu->cond);
    pthread_mutex_unlock(&gpu->lock);
}

static void virtio_gpu_reset(virtio_dev_t *dev) {
    virtio_gpu_t *gpu = dev->priv;
    pthread_mutex_lock(&gpu->vq_lock);
    gpu->generation++;
    pthread_mutex_unlock(&gpu->vq_lock);
    pthread_mutex_lock(&gpu->lock);
    pthread_cond_signal(&gpu->cond);
    gpu->kicks |= GPU_KICK_CTRL;
    pthread_mutex_unlock(&gpu->lock);
}

static bool virtio_gpu_shm_region(virtio_dev_t *dev, uint32_t id, uint64_t *base, uint64_t *len) {
    if (id != GPU_SHM_ID_HOST_VISIBLE) {
        return false;
    }
    *base = dev->vm->gpu_shm.base;
    *len = dev->vm->gpu_shm.size;
    return true;
}

static void virtio_gpu_destroy(virtio_dev_t *dev) {
    virtio_gpu_t *gpu = dev->priv;
    if (!gpu) {
        return;
    }
    if (gpu->worker_started) {
        pthread_mutex_lock(&gpu->lock);
        gpu->stop = true;
        pthread_cond_signal(&gpu->cond);
        pthread_mutex_unlock(&gpu->lock);
        pthread_join(gpu->worker, NULL);
    }
    gpu_set_retire(NULL, NULL);
    for (gpu_retired_t *retired = gpu->retired; retired;) {
        gpu_retired_t *next = retired->next;
        free(retired);
        retired = next;
    }
    pthread_cond_destroy(&gpu->cond);
    pthread_mutex_destroy(&gpu->lock);
    pthread_mutex_destroy(&gpu->vq_lock);
    free(gpu->request);
    free(gpu);
    dev->priv = NULL;
}

static const virtio_dev_ops_t virtio_gpu_ops = {
    .get_features = virtio_gpu_get_features,
    .read_config = virtio_gpu_read_config,
    .write_config = virtio_gpu_write_config,
    .queue_notify = virtio_gpu_queue_notify,
    .reset = virtio_gpu_reset,
    .destroy = virtio_gpu_destroy,
    .shm_region = virtio_gpu_shm_region,
};

virtio_dev_t *virtio_gpu_create(vmm_machine_t *vm, uint32_t slot) {
    virtio_gpu_t *gpu = calloc(1, sizeof(*gpu));
    if (!gpu) {
        return NULL;
    }
    gpu->config.num_scanouts = 0;
    gpu->config.num_capsets = 1;
    pthread_mutex_init(&gpu->lock, NULL);
    pthread_mutex_init(&gpu->vq_lock, NULL);
    pthread_cond_init(&gpu->cond, NULL);
    const uint32_t queue_sizes[GPU_QUEUE_COUNT] = {GPU_QUEUE_SIZE, GPU_QUEUE_SIZE};
    virtio_dev_t *dev = virtio_dev_alloc(vm, slot, VIRTIO_ID_GPU, GPU_QUEUE_COUNT, queue_sizes, &virtio_gpu_ops, gpu);
    if (!dev) {
        pthread_cond_destroy(&gpu->cond);
        pthread_mutex_destroy(&gpu->lock);
        pthread_mutex_destroy(&gpu->vq_lock);
        free(gpu);
        return NULL;
    }
    gpu->dev = dev;
    gpu_set_retire(gpu, gpu_on_retire);
    if (pthread_create(&gpu->worker, NULL, gpu_worker, gpu) != 0) {
        virtio_dev_destroy(dev);
        return NULL;
    }
    gpu->worker_started = true;
    return dev;
}
