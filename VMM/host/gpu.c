#include "gpu.h"

#include "venus/vkr_swipterm.h"
#include "virglrenderer.h"
#include "vulkan/vulkan_core.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

enum {
    GPU_BACKEND_COUNT = 3,
    GPU_ICD_INTERFACE_VERSION = 7,
    GPU_SUSPEND_TIMEOUT_MS = 2000,
};

typedef VkResult (*gpu_negotiate_fn)(uint32_t *version);

static const char *const gpu_backend_names[GPU_BACKEND_COUNT] = {
    [SWIPTERM_GPU_BACKEND_MOLTENVK] = "MoltenVK",
    [SWIPTERM_GPU_BACKEND_KOSMICKRISP] = "KosmicKrisp",
};

static pthread_mutex_t g_renderer_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_retire_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_retire_owner;
static gpu_retire_fn g_retire;
static bool g_renderer_ready;

static void gpu_write_fence(void *cookie, uint32_t fence) {
    (void)cookie;
    (void)fence;
}

static void gpu_write_context_fence(void *cookie, uint32_t ctx_id, uint32_t ring_idx, uint64_t fence_id) {
    (void)cookie;
    pthread_mutex_lock(&g_retire_lock);
    if (g_retire) {
        g_retire(g_retire_owner, ctx_id, ring_idx, fence_id);
    }
    pthread_mutex_unlock(&g_retire_lock);
}

static struct virgl_renderer_callbacks g_callbacks = {
    .version = 3,
    .write_fence = gpu_write_fence,
    .write_context_fence = gpu_write_context_fence,
};

static PFN_vkGetInstanceProcAddr gpu_load(swipterm_gpu_backend backend) {
    if (backend == SWIPTERM_GPU_BACKEND_KOSMICKRISP && !gpu_metal4_supported()) {
        return NULL;
    }
    const char *name = gpu_backend_names[backend];
    char path[128];
    snprintf(path, sizeof(path), "@rpath/%s.framework/%s", name, name);
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return NULL;
    }
    gpu_negotiate_fn negotiate = (gpu_negotiate_fn)dlsym(handle, "vk_icdNegotiateLoaderICDInterfaceVersion");
    uint32_t version = GPU_ICD_INTERFACE_VERSION;
    negotiate(&version);
    return (PFN_vkGetInstanceProcAddr)dlsym(handle, "vk_icdGetInstanceProcAddr");
}

uint32_t swipterm_gpu_available_backends(void) {
    uint32_t mask = 0;
    for (int backend = SWIPTERM_GPU_BACKEND_MOLTENVK; backend < GPU_BACKEND_COUNT; backend++) {
        if (gpu_framework_embedded(gpu_backend_names[backend]) &&
            (backend != SWIPTERM_GPU_BACKEND_KOSMICKRISP || gpu_metal4_supported())) {
            mask |= 1U << backend;
        }
    }
    return mask;
}

bool gpu_acquire(swipterm_gpu_backend requested) {
    static const swipterm_gpu_backend automatic[] = {SWIPTERM_GPU_BACKEND_KOSMICKRISP, SWIPTERM_GPU_BACKEND_MOLTENVK};
    const swipterm_gpu_backend *order = requested == SWIPTERM_GPU_BACKEND_AUTOMATIC ? automatic : &requested;
    const size_t order_count = requested == SWIPTERM_GPU_BACKEND_AUTOMATIC ? 2 : 1;
    PFN_vkGetInstanceProcAddr gipa = NULL;
    for (size_t i = 0; i < order_count && !gipa; i++) {
        gipa = gpu_load(order[i]);
    }
    if (!gipa) {
        return false;
    }
    vkr_swipterm_set_loader((void *)gipa);
    if (!g_renderer_ready) {
        static const int flags = VIRGL_RENDERER_VENUS | VIRGL_RENDERER_NO_VIRGL | VIRGL_RENDERER_RENDER_SERVER |
                                 VIRGL_RENDERER_THREAD_SYNC | VIRGL_RENDERER_ASYNC_FENCE_CB;
        if (virgl_renderer_init(&g_callbacks, flags, &g_callbacks) != 0) {
            return false;
        }
        g_renderer_ready = true;
    }
    return true;
}

void gpu_lock(void) {
    pthread_mutex_lock(&g_renderer_lock);
}

void gpu_unlock(void) {
    pthread_mutex_unlock(&g_renderer_lock);
}

void gpu_set_retire(void *owner, gpu_retire_fn retire) {
    pthread_mutex_lock(&g_retire_lock);
    g_retire_owner = owner;
    g_retire = retire;
    pthread_mutex_unlock(&g_retire_lock);
}

bool swipterm_gpu_suspend(void) {
    return vkr_swipterm_suspend(GPU_SUSPEND_TIMEOUT_MS);
}

void swipterm_gpu_resume(void) {
    vkr_swipterm_resume();
}
