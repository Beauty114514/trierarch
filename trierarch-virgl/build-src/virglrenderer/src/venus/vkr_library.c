/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_common.h"
#include "vkr_library.h"

#include <dlfcn.h>

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
   if (lib->handle)
      return true;

   lib->handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
   if (lib->handle == NULL)
      lib->handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
   if (lib->handle == NULL) {
      vkr_log("failed to open libvulkan: %s", dlerror());
      return false;
   }

   /* Clear any existing error */
   dlerror();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   /* ISO C forbids conversion of object pointer to function pointer type */
   lib->GetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dlsym(lib->handle, "vkGetInstanceProcAddr");
#pragma GCC diagnostic pop

   char *error = dlerror();
   if (error != NULL) {
      vkr_log("dlerror: %s", error);
      goto fail;
   }

   if (lib->GetInstanceProcAddr == NULL) {
      vkr_log("failed to load vkGetInstanceProcAddr: %s", dlerror());
      goto fail;
   }

   return true;

fail:
   dlclose(lib->handle);
   lib->handle = NULL;
   return false;
}

void
vkr_library_unload(struct vulkan_library *lib)
{
   if (lib->handle) {
      dlclose(lib->handle);
      lib->GetInstanceProcAddr = NULL;
      lib->handle = NULL;
   }
}

#endif /* ENABLE_VULKAN_DLOAD */
