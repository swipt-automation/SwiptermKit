/*
 * Copyright 2026 nekohasekai
 * SPDX-License-Identifier: MIT
 */

#ifndef VKR_SWIPTERM_H
#define VKR_SWIPTERM_H

#include <stdbool.h>
#include <stdint.h>

void
vkr_swipterm_set_loader(void *get_instance_proc_addr);

bool
vkr_swipterm_suspend(uint32_t timeout_ms);

void
vkr_swipterm_resume(void);

#endif /* VKR_SWIPTERM_H */
