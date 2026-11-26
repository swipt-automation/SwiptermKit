/*
 * Copyright 2026 nekohasekai
 * SPDX-License-Identifier: MIT
 */

#ifndef VKR_GATE_H
#define VKR_GATE_H

#include "vkr_common.h"

#include "vkr_swipterm.h"

struct vkr_context;
struct vkr_device;
struct vkr_queue;

PFN_vkGetInstanceProcAddr
vkr_swipterm_loader(void);

bool
vkr_gate_enter(struct vkr_context *ctx);

void
vkr_gate_leave(void);

void
vkr_gate_close(struct vkr_context *ctx);

void
vkr_gate_register_queue(struct vkr_queue *queue);

void
vkr_gate_unregister_queue(struct vkr_queue *queue);

bool
vkr_device_is_lost(const struct vkr_device *dev);

void
vkr_device_check_lost(struct vkr_device *dev, VkResult result);

#endif /* VKR_GATE_H */
