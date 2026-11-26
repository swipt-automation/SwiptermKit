#ifndef VMM_HOST_GPU_H
#define VMM_HOST_GPU_H

#include "../include/CSwiptermKit/swipterm.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*gpu_retire_fn)(void *owner, uint32_t ctx_id, uint32_t ring_idx, uint64_t fence_id);

bool gpu_acquire(swipterm_gpu_backend requested);

void gpu_lock(void);
void gpu_unlock(void);

void gpu_set_retire(void *owner, gpu_retire_fn retire);
void gpu_clear_retire(void *owner);

bool gpu_metal4_supported(void);
bool gpu_framework_embedded(const char *name);

#endif
