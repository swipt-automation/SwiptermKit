/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_image.h"

#include "vkr_device.h"
#include "vkr_image_gen.h"
#include "vkr_physical_device.h"

#define VKR_MEMORY_PLANE_ASPECTS                                                         \
   (VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT | VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT |   \
    VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT | VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT)

static VkResult
vkr_image_fix_create_info(struct vkr_device *dev,
                          VkImageCreateInfo *info,
                          bool check_explicit_layout)
{
   const struct vkr_physical_device *physical_dev = dev->physical_device;
   if (!physical_dev->emulated_dma_buf)
      return VK_SUCCESS;

   vkr_physical_device_strip_emulated_handle_types(
      physical_dev, info, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
   if (info->tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
      return VK_SUCCESS;

   void *prev =
      vkr_find_prev_struct(info, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT);
   if (prev) {
      const VkImageDrmFormatModifierListCreateInfoEXT *list = vkr_pnext_get_next(prev);
      bool linear = false;
      for (uint32_t i = 0; i < list->drmFormatModifierCount; i++)
         linear |= list->pDrmFormatModifiers[i] == 0;
      if (!linear)
         return VK_ERROR_FORMAT_NOT_SUPPORTED;
      vkr_pnext_set_next(prev, list->pNext);
   }

   VkDeviceSize row_pitch = 0;
   prev = vkr_find_prev_struct(
      info, VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT);
   if (prev) {
      const VkImageDrmFormatModifierExplicitCreateInfoEXT *explicit_info =
         vkr_pnext_get_next(prev);
      if (explicit_info->drmFormatModifier != 0 ||
          explicit_info->drmFormatModifierPlaneCount != 1 ||
          explicit_info->pPlaneLayouts[0].offset != 0)
         return VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
      row_pitch = explicit_info->pPlaneLayouts[0].rowPitch;
      vkr_pnext_set_next(prev, explicit_info->pNext);
   }

   info->tiling = VK_IMAGE_TILING_LINEAR;
   if (!row_pitch || !check_explicit_layout)
      return VK_SUCCESS;

   /* the driver picks the linear row pitch; an imported layout must match it */
   struct vn_device_proc_table *vk = &dev->proc_table;
   VkImage probe;
   VkResult result = vk->CreateImage(dev->base.handle.device, info, NULL, &probe);
   if (result != VK_SUCCESS)
      return result;
   const VkImageSubresource subresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };
   VkSubresourceLayout layout;
   vk->GetImageSubresourceLayout(dev->base.handle.device, probe, &subresource, &layout);
   vk->DestroyImage(dev->base.handle.device, probe, NULL);
   return layout.rowPitch == row_pitch ? VK_SUCCESS
                                       : VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
}

static void
vkr_image_fix_aspect(const struct vkr_device *dev, VkImageAspectFlags *aspect)
{
   if (dev->physical_device->emulated_dma_buf && (*aspect & VKR_MEMORY_PLANE_ASPECTS))
      *aspect = VK_IMAGE_ASPECT_COLOR_BIT;
}

static void
vkr_dispatch_vkCreateImage(struct vn_dispatch_context *dispatch,
                           struct vn_command_vkCreateImage *args)
{
   /* XXX If VkExternalMemoryImageCreateInfo is chained by the app, all is
    * good.  If it is not chained, we might still bind an external memory to
    * the image, because vkr_dispatch_vkAllocateMemory makes any HOST_VISIBLE
    * memory external.  That is a spec violation.
    *
    * The discussions in vkr_dispatch_vkCreateBuffer are applicable to both
    * buffers and images.  Additionally, drivers usually use
    * VkExternalMemoryImageCreateInfo to pick a well-defined image layout for
    * interoperability with foreign queues.  However, a well-defined layout
    * might not exist for some images.  When it does, it might still require a
    * dedicated allocation or might have a degraded performance.
    *
    * On the other hand, binding an external memory to an image created
    * without VkExternalMemoryImageCreateInfo usually works.  Yes, it will
    * explode if the external memory is accessed by foreign queues due to the
    * lack of a well-defined image layout.  But we never end up in that
    * situation because the app does not consider the memory external.
    */

   struct vkr_device *dev = vkr_device_from_handle(args->device);
   args->ret = vkr_image_fix_create_info(dev, (VkImageCreateInfo *)args->pCreateInfo, true);
   if (args->ret != VK_SUCCESS)
      return;

   vkr_image_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyImage(struct vn_dispatch_context *dispatch,
                            struct vn_command_vkDestroyImage *args)
{
   vkr_image_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetImageMemoryRequirements(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageMemoryRequirements_args_handle(args);
   vk->GetImageMemoryRequirements(args->device, args->image, args->pMemoryRequirements);
   vkr_physical_device_mem_type_spoof(dev->physical_device,
                                      &args->pMemoryRequirements->memoryTypeBits);
}

static void
vkr_dispatch_vkGetImageMemoryRequirements2(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageMemoryRequirements2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageMemoryRequirements2_args_handle(args);
   vk->GetImageMemoryRequirements2(args->device, args->pInfo, args->pMemoryRequirements);
   vkr_physical_device_mem_type_spoof(
      dev->physical_device,
      &args->pMemoryRequirements->memoryRequirements.memoryTypeBits);
}

static void
vkr_dispatch_vkGetImageSparseMemoryRequirements(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageSparseMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageSparseMemoryRequirements_args_handle(args);
   vk->GetImageSparseMemoryRequirements(args->device, args->image,
                                        args->pSparseMemoryRequirementCount,
                                        args->pSparseMemoryRequirements);
}

static void
vkr_dispatch_vkGetImageSparseMemoryRequirements2(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageSparseMemoryRequirements2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageSparseMemoryRequirements2_args_handle(args);
   vk->GetImageSparseMemoryRequirements2(args->device, args->pInfo,
                                         args->pSparseMemoryRequirementCount,
                                         args->pSparseMemoryRequirements);
}

static void
vkr_dispatch_vkBindImageMemory(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkBindImageMemory *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkBindImageMemory_args_handle(args);
   args->ret =
      vk->BindImageMemory(args->device, args->image, args->memory, args->memoryOffset);
}

static void
vkr_dispatch_vkBindImageMemory2(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkBindImageMemory2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkBindImageMemory2_args_handle(args);
   args->ret = vk->BindImageMemory2(args->device, args->bindInfoCount, args->pBindInfos);
}

static void
vkr_dispatch_vkGetImageSubresourceLayout(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageSubresourceLayout *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageSubresourceLayout_args_handle(args);
   vkr_image_fix_aspect(dev, &((VkImageSubresource *)args->pSubresource)->aspectMask);
   vk->GetImageSubresourceLayout(args->device, args->image, args->pSubresource,
                                 args->pLayout);
}

static void
vkr_dispatch_vkGetImageSubresourceLayout2(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageSubresourceLayout2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageSubresourceLayout2_args_handle(args);
   vkr_image_fix_aspect(
      dev, &((VkImageSubresource2 *)args->pSubresource)->imageSubresource.aspectMask);
   vk->GetImageSubresourceLayout2(args->device, args->image, args->pSubresource,
                                  args->pLayout);
}

static void
vkr_dispatch_vkGetDeviceImageSubresourceLayout(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetDeviceImageSubresourceLayout *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetDeviceImageSubresourceLayout_args_handle(args);
   VkDeviceImageSubresourceInfo *info = (VkDeviceImageSubresourceInfo *)args->pInfo;
   if (vkr_image_fix_create_info(dev, (VkImageCreateInfo *)info->pCreateInfo, false) !=
       VK_SUCCESS) {
      args->pLayout->subresourceLayout = (VkSubresourceLayout){ 0 };
      return;
   }
   vkr_image_fix_aspect(
      dev, &((VkImageSubresource2 *)info->pSubresource)->imageSubresource.aspectMask);
   vk->GetDeviceImageSubresourceLayout(args->device, args->pInfo, args->pLayout);
}

static void
vkr_dispatch_vkGetImageDrmFormatModifierPropertiesEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageDrmFormatModifierPropertiesEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   if (dev->physical_device->emulated_dma_buf) {
      args->pProperties->drmFormatModifier = 0;
      args->ret = VK_SUCCESS;
      return;
   }

   vn_replace_vkGetImageDrmFormatModifierPropertiesEXT_args_handle(args);
   args->ret = vk->GetImageDrmFormatModifierPropertiesEXT(args->device, args->image,
                                                          args->pProperties);
}

static void
vkr_dispatch_vkCreateImageView(struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCreateImageView *args)
{
   vkr_image_view_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyImageView(struct vn_dispatch_context *dispatch,
                                struct vn_command_vkDestroyImageView *args)
{
   vkr_image_view_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreateSampler(struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCreateSampler *args)
{
   vkr_sampler_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroySampler(struct vn_dispatch_context *dispatch,
                              struct vn_command_vkDestroySampler *args)
{
   vkr_sampler_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreateSamplerYcbcrConversion(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkCreateSamplerYcbcrConversion *args)
{
   vkr_sampler_ycbcr_conversion_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroySamplerYcbcrConversion(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkDestroySamplerYcbcrConversion *args)
{
   vkr_sampler_ycbcr_conversion_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetDeviceImageMemoryRequirements(
   UNUSED struct vn_dispatch_context *ctx,
   struct vn_command_vkGetDeviceImageMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetDeviceImageMemoryRequirements_args_handle(args);
   vkr_image_fix_create_info(dev, (VkImageCreateInfo *)args->pInfo->pCreateInfo, false);
   vk->GetDeviceImageMemoryRequirements(args->device, args->pInfo,
                                        args->pMemoryRequirements);
   vkr_physical_device_mem_type_spoof(
      dev->physical_device,
      &args->pMemoryRequirements->memoryRequirements.memoryTypeBits);
}

static void
vkr_dispatch_vkGetDeviceImageSparseMemoryRequirements(
   UNUSED struct vn_dispatch_context *ctx,
   struct vn_command_vkGetDeviceImageSparseMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetDeviceImageSparseMemoryRequirements_args_handle(args);
   vkr_image_fix_create_info(dev, (VkImageCreateInfo *)args->pInfo->pCreateInfo, false);
   vk->GetDeviceImageSparseMemoryRequirements(args->device, args->pInfo,
                                              args->pSparseMemoryRequirementCount,
                                              args->pSparseMemoryRequirements);
}

void
vkr_context_init_image_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateImage = vkr_dispatch_vkCreateImage;
   dispatch->dispatch_vkDestroyImage = vkr_dispatch_vkDestroyImage;
   dispatch->dispatch_vkGetImageMemoryRequirements =
      vkr_dispatch_vkGetImageMemoryRequirements;
   dispatch->dispatch_vkGetImageMemoryRequirements2 =
      vkr_dispatch_vkGetImageMemoryRequirements2;
   dispatch->dispatch_vkGetImageSparseMemoryRequirements =
      vkr_dispatch_vkGetImageSparseMemoryRequirements;
   dispatch->dispatch_vkGetImageSparseMemoryRequirements2 =
      vkr_dispatch_vkGetImageSparseMemoryRequirements2;
   dispatch->dispatch_vkBindImageMemory = vkr_dispatch_vkBindImageMemory;
   dispatch->dispatch_vkBindImageMemory2 = vkr_dispatch_vkBindImageMemory2;
   dispatch->dispatch_vkGetImageSubresourceLayout =
      vkr_dispatch_vkGetImageSubresourceLayout;
   dispatch->dispatch_vkGetImageSubresourceLayout2 =
      vkr_dispatch_vkGetImageSubresourceLayout2;
   dispatch->dispatch_vkGetDeviceImageSubresourceLayout =
      vkr_dispatch_vkGetDeviceImageSubresourceLayout;

   dispatch->dispatch_vkGetImageDrmFormatModifierPropertiesEXT =
      vkr_dispatch_vkGetImageDrmFormatModifierPropertiesEXT;
   dispatch->dispatch_vkGetDeviceImageMemoryRequirements =
      vkr_dispatch_vkGetDeviceImageMemoryRequirements;
   dispatch->dispatch_vkGetDeviceImageSparseMemoryRequirements =
      vkr_dispatch_vkGetDeviceImageSparseMemoryRequirements;
}

void
vkr_context_init_image_view_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateImageView = vkr_dispatch_vkCreateImageView;
   dispatch->dispatch_vkDestroyImageView = vkr_dispatch_vkDestroyImageView;
}

void
vkr_context_init_sampler_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateSampler = vkr_dispatch_vkCreateSampler;
   dispatch->dispatch_vkDestroySampler = vkr_dispatch_vkDestroySampler;
}

void
vkr_context_init_sampler_ycbcr_conversion_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateSamplerYcbcrConversion =
      vkr_dispatch_vkCreateSamplerYcbcrConversion;
   dispatch->dispatch_vkDestroySamplerYcbcrConversion =
      vkr_dispatch_vkDestroySamplerYcbcrConversion;
}
