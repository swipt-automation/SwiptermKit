/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_common.h"

#include <dlfcn.h>

#include "vkr_library.h"

#include "vkr_gate.h"

void
vkr_library_preload_icd(void)
{
#ifdef ENABLE_VULKAN_PRELOAD
   struct vulkan_library lib = { 0 };

   if (!vkr_library_load(&lib))
      return;

   /* Get vkGetInstanceProcAddr from libvulkan */
   PFN_vkGetInstanceProcAddr get_proc_addr = lib.GetInstanceProcAddr;

   PFN_vkEnumerateInstanceExtensionProperties enumerate_inst_ext_props =
      (PFN_vkEnumerateInstanceExtensionProperties)get_proc_addr(
         VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
   if (enumerate_inst_ext_props) {
      /* this makes the Vulkan loader loads ICDs */
      uint32_t unused_count;
      enumerate_inst_ext_props(NULL, &unused_count, NULL);
   }

   vkr_library_unload(&lib);
#endif
}

#if defined(ENABLE_VULKAN_DLOAD)

bool
vkr_library_load(struct vulkan_library *lib)
{
   if (lib->GetInstanceProcAddr)
      return true;

   lib->handle = NULL;
   lib->GetInstanceProcAddr = vkr_swipterm_loader();
   if (!lib->GetInstanceProcAddr) {
      vkr_log("no Vulkan backend is loaded");
      return false;
   }
   return true;
}

void
vkr_library_unload(struct vulkan_library *lib)
{
   lib->GetInstanceProcAddr = NULL;
   lib->handle = NULL;
}

#endif /* ENABLE_VULKAN_DLOAD */

bool
vkr_library_has_portability_enumeration(
   PFN_vkEnumerateInstanceExtensionProperties enum_inst_ext_props)
{
   return vkr_library_has_instance_extension(enum_inst_ext_props,
                                             VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
}

bool
vkr_library_has_instance_extension(
   PFN_vkEnumerateInstanceExtensionProperties enum_inst_ext_props, const char *name)
{
   uint32_t property_count = 0;
   VkExtensionProperties *properties;
   bool has_portability_enumeration = false;

   VkResult ret = enum_inst_ext_props(NULL, &property_count, NULL);
   if (ret != VK_SUCCESS)
      return false;

   properties = calloc(property_count, sizeof(*properties));
   if (!properties)
      return false;

   ret = enum_inst_ext_props(NULL, &property_count, properties);
   if (ret != VK_SUCCESS) {
      free(properties);
      return false;
   }

   for (uint32_t i = 0; i < property_count; i++) {
      if (!strcmp(properties[i].extensionName, name)) {
         has_portability_enumeration = true;
         break;
      }
   }
   free(properties);
   return has_portability_enumeration;
}
