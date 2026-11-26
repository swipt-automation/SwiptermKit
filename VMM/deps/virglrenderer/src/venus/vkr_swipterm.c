/*
 * Copyright 2026 nekohasekai
 * SPDX-License-Identifier: MIT
 */

#include "vkr_gate.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#include "vkr_context.h"
#include "vkr_device.h"
#include "vkr_queue.h"

static struct {
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   bool suspended;
   uint32_t active;

   pthread_mutex_t registry_mutex;
   struct list_head queues;

   PFN_vkGetInstanceProcAddr loader;
} vkr_swipterm = {
   .mutex = PTHREAD_MUTEX_INITIALIZER,
   .cond = PTHREAD_COND_INITIALIZER,
   .registry_mutex = PTHREAD_MUTEX_INITIALIZER,
   .queues = { &vkr_swipterm.queues, &vkr_swipterm.queues },
};

void
vkr_swipterm_set_loader(void *get_instance_proc_addr)
{
   vkr_swipterm.loader = (PFN_vkGetInstanceProcAddr)get_instance_proc_addr;
}

PFN_vkGetInstanceProcAddr
vkr_swipterm_loader(void)
{
   return vkr_swipterm.loader;
}

bool
vkr_gate_enter(struct vkr_context *ctx)
{
   pthread_mutex_lock(&vkr_swipterm.mutex);
   while (vkr_swipterm.suspended && !ctx->gate_closed)
      pthread_cond_wait(&vkr_swipterm.cond, &vkr_swipterm.mutex);
   const bool entered = !vkr_swipterm.suspended;
   if (entered)
      vkr_swipterm.active++;
   pthread_mutex_unlock(&vkr_swipterm.mutex);
   return entered;
}

void
vkr_gate_leave(void)
{
   pthread_mutex_lock(&vkr_swipterm.mutex);
   if (--vkr_swipterm.active == 0)
      pthread_cond_broadcast(&vkr_swipterm.cond);
   pthread_mutex_unlock(&vkr_swipterm.mutex);
}

void
vkr_gate_close(struct vkr_context *ctx)
{
   pthread_mutex_lock(&vkr_swipterm.mutex);
   ctx->gate_closed = true;
   pthread_cond_broadcast(&vkr_swipterm.cond);
   pthread_mutex_unlock(&vkr_swipterm.mutex);
}

void
vkr_gate_register_queue(struct vkr_queue *queue)
{
   pthread_mutex_lock(&vkr_swipterm.registry_mutex);
   list_addtail(&queue->gate_head, &vkr_swipterm.queues);
   pthread_mutex_unlock(&vkr_swipterm.registry_mutex);
}

void
vkr_gate_unregister_queue(struct vkr_queue *queue)
{
   pthread_mutex_lock(&vkr_swipterm.registry_mutex);
   list_del(&queue->gate_head);
   pthread_mutex_unlock(&vkr_swipterm.registry_mutex);
   if (queue->drain_fence != VK_NULL_HANDLE) {
      struct vkr_device *dev = queue->device;
      dev->proc_table.DestroyFence(dev->base.handle.device, queue->drain_fence, NULL);
   }
}

static uint64_t
vkr_swipterm_remaining_ns(const struct timespec *deadline)
{
   struct timespec now;
   clock_gettime(CLOCK_REALTIME, &now);
   const int64_t remaining = (int64_t)(deadline->tv_sec - now.tv_sec) * 1000000000LL +
                             (deadline->tv_nsec - now.tv_nsec);
   return remaining > 0 ? (uint64_t)remaining : 0;
}

static bool
vkr_swipterm_drain_queue(struct vkr_queue *queue, const struct timespec *deadline)
{
   struct vkr_device *dev = queue->device;
   struct vn_device_proc_table *vk = &dev->proc_table;
   VkDevice device = dev->base.handle.device;
   if (queue->drain_fence == VK_NULL_HANDLE) {
      const VkFenceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
      if (vk->CreateFence(device, &info, NULL, &queue->drain_fence) != VK_SUCCESS) {
         queue->drain_fence = VK_NULL_HANDLE;
         return false;
      }
   }
   for (;;) {
      const bool fresh = !queue->drain_pending;
      VkResult result;
      if (fresh) {
         mtx_lock(&queue->vk_mutex);
         result = vk->QueueSubmit(queue->base.handle.queue, 0, NULL, queue->drain_fence);
         mtx_unlock(&queue->vk_mutex);
         vkr_device_check_lost(dev, result);
         if (result != VK_SUCCESS)
            return false;
         queue->drain_pending = true;
      }
      result = vk->WaitForFences(device, 1, &queue->drain_fence, VK_TRUE,
                                 vkr_swipterm_remaining_ns(deadline));
      vkr_device_check_lost(dev, result);
      if (result != VK_SUCCESS)
         return false;
      vk->ResetFences(device, 1, &queue->drain_fence);
      queue->drain_pending = false;
      if (fresh)
         return true;
   }
}

bool
vkr_swipterm_suspend(uint32_t timeout_ms)
{
   struct timespec deadline;
   clock_gettime(CLOCK_REALTIME, &deadline);
   deadline.tv_sec += timeout_ms / 1000;
   deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
   if (deadline.tv_nsec >= 1000000000L) {
      deadline.tv_sec++;
      deadline.tv_nsec -= 1000000000L;
   }

   pthread_mutex_lock(&vkr_swipterm.mutex);
   vkr_swipterm.suspended = true;
   while (vkr_swipterm.active &&
          pthread_cond_timedwait(&vkr_swipterm.cond, &vkr_swipterm.mutex, &deadline) != ETIMEDOUT)
      ;
   const bool quiet = vkr_swipterm.active == 0;
   pthread_mutex_unlock(&vkr_swipterm.mutex);
   if (!quiet)
      return false;

   bool drained = true;
   pthread_mutex_lock(&vkr_swipterm.registry_mutex);
   list_for_each_entry (struct vkr_queue, queue, &vkr_swipterm.queues, gate_head) {
      if (!vkr_device_is_lost(queue->device) && !vkr_swipterm_drain_queue(queue, &deadline))
         drained = false;
   }
   pthread_mutex_unlock(&vkr_swipterm.registry_mutex);
   return drained;
}

void
vkr_swipterm_resume(void)
{
   pthread_mutex_lock(&vkr_swipterm.mutex);
   vkr_swipterm.suspended = false;
   pthread_cond_broadcast(&vkr_swipterm.cond);
   pthread_mutex_unlock(&vkr_swipterm.mutex);
}

bool
vkr_device_is_lost(const struct vkr_device *dev)
{
   return atomic_load(&dev->lost);
}

void
vkr_device_check_lost(struct vkr_device *dev, VkResult result)
{
   if (result != VK_ERROR_DEVICE_LOST || atomic_exchange(&dev->lost, true))
      return;
   vkr_log("device %p lost; isolating it", (void *)dev);
}
