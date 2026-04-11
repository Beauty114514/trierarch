/**************************************************************************
 *
 * Copyright (C) 2022 Collabora Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "vkr_allocator.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/list.h"
#include "util/macros.h"
#include "venus-protocol/vulkan.h"
#include "virgl_resource.h"

#include "vkr_library.h"

/* Assume that we will deal with at most 4 devices.
 *  This is to avoid per-device resource dynamic allocations.
 *  For now, `vkr_allocator` is designed for Mesa CI use which
 *  uses lavapipe as the only Vulkan driver, but allow logic to
 *  assume more for some leeway and felxibilty; especially if
 *  this allocator is expanded to use whatever devices available.
 */
#define VKR_ALLOCATOR_MAX_DEVICE_COUNT 4

struct vkr_inst_proc_table {
   PFN_vkCreateInstance CreateInstance;
   PFN_vkDestroyInstance DestroyInstance;
   PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
   PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2;
   PFN_vkCreateDevice CreateDevice;
   PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
};

struct vkr_dev_proc_table {
   PFN_vkDestroyDevice DestroyDevice;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkFreeMemory FreeMemory;
   PFN_vkMapMemory MapMemory;
   PFN_vkUnmapMemory UnmapMemory;
};

struct vkr_opaque_fd_mem_info {
   struct vkr_dev_proc_table *vk;

   VkDevice device;
   VkDeviceMemory device_memory;
   uint32_t res_id;
   uint64_t size;

   struct list_head head;
};

static struct vkr_allocator {
   struct vkr_inst_proc_table proc_table;
   VkInstance instance;

   struct vkr_dev_proc_table proc_tables[VKR_ALLOCATOR_MAX_DEVICE_COUNT];
   VkPhysicalDevice physical_devices[VKR_ALLOCATOR_MAX_DEVICE_COUNT];
   VkDevice devices[VKR_ALLOCATOR_MAX_DEVICE_COUNT];
   uint8_t device_uuids[VKR_ALLOCATOR_MAX_DEVICE_COUNT][VK_UUID_SIZE];
   uint32_t device_count;

   struct list_head memories;
   struct vulkan_library vulkan_library;
} vkr_allocator;

static bool vkr_allocator_initialized;

static void
vkr_allocator_free_memory(struct vkr_opaque_fd_mem_info *mem_info)
{
   mem_info->vk->FreeMemory(mem_info->device, mem_info->device_memory, NULL);
   list_del(&mem_info->head);
   free(mem_info);
}

static uint32_t
vkr_allocator_get_dev_idx(struct virgl_resource *res)
{
   for (uint32_t i = 0; i < vkr_allocator.device_count; ++i) {
      if (memcmp(vkr_allocator.device_uuids[i], res->vulkan_info.device_uuid,
                 VK_UUID_SIZE) == 0)
         return i;
   }

   return VKR_ALLOCATOR_MAX_DEVICE_COUNT;
}

static struct vkr_opaque_fd_mem_info *
vkr_allocator_allocate_memory(struct virgl_resource *res)
{
   const uint32_t idx = vkr_allocator_get_dev_idx(res);
   if (idx == VKR_ALLOCATOR_MAX_DEVICE_COUNT)
      return NULL;

   VkDevice dev_handle = vkr_allocator.devices[idx];
   if (dev_handle == VK_NULL_HANDLE)
      return NULL;

   struct vkr_dev_proc_table *vk = &vkr_allocator.proc_tables[idx];

   int fd = -1;
   if (virgl_resource_export_fd(res, &fd) != VIRGL_RESOURCE_FD_OPAQUE) {
      if (fd >= 0)
         close(fd);
      return NULL;
   }

   VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext =
         &(VkImportMemoryFdInfoKHR){ .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                     .handleType =
                                        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                     .fd = fd },
      .allocationSize = res->vulkan_info.allocation_size,
      .memoryTypeIndex = res->vulkan_info.memory_type_index
   };

   VkDeviceMemory mem_handle;
   if (vk->AllocateMemory(dev_handle, &alloc_info, NULL, &mem_handle) != VK_SUCCESS) {
      close(fd);
      return NULL;
   }

   struct vkr_opaque_fd_mem_info *mem_info = calloc(1, sizeof(*mem_info));
   if (!mem_info) {
      vk->FreeMemory(dev_handle, mem_handle, NULL);
      return NULL;
   }

   mem_info->vk = vk;
   mem_info->device = dev_handle;
   mem_info->device_memory = mem_handle;
   mem_info->res_id = res->res_id;
   mem_info->size = res->vulkan_info.allocation_size;

   list_addtail(&mem_info->head, &vkr_allocator.memories);

   return mem_info;
}

void
vkr_allocator_fini(void)
{
   if (!vkr_allocator_initialized)
      return;

   list_for_each_entry_safe (struct vkr_opaque_fd_mem_info, mem_info, &vkr_allocator.memories, head)
      vkr_allocator_free_memory(mem_info);

   for (uint32_t i = 0; i < vkr_allocator.device_count; i++) {
      struct vkr_dev_proc_table *vk = &vkr_allocator.proc_tables[i];
      vk->DestroyDevice(vkr_allocator.devices[i], NULL);
   }

   struct vkr_inst_proc_table *vk = &vkr_allocator.proc_table;
   vk->DestroyInstance(vkr_allocator.instance, NULL);

   vkr_library_unload(&vkr_allocator.vulkan_library);

   memset(&vkr_allocator, 0, sizeof(vkr_allocator));

   vkr_allocator_initialized = false;
}

static void
vkr_allocator_inst_proc_table_init(VkInstance inst_handle,
                                   PFN_vkGetInstanceProcAddr get_proc_addr,
                                   struct vkr_inst_proc_table *vk)
{
#define VN_GIPA(cmd) (PFN_##cmd) get_proc_addr(inst_handle, #cmd)
   vk->DestroyInstance = VN_GIPA(vkDestroyInstance);
   vk->EnumeratePhysicalDevices = VN_GIPA(vkEnumeratePhysicalDevices);
   vk->GetPhysicalDeviceProperties2 = VN_GIPA(vkGetPhysicalDeviceProperties2);
   vk->CreateDevice = VN_GIPA(vkCreateDevice);
   vk->GetDeviceProcAddr = VN_GIPA(vkGetDeviceProcAddr);
#undef VN_GIPA
}

static void
vkr_allocator_dev_proc_table_init(VkDevice dev_handle,
                                  PFN_vkGetDeviceProcAddr get_proc_addr,
                                  struct vkr_dev_proc_table *vk)
{
#define VN_GDPA(cmd) (PFN_##cmd) get_proc_addr(dev_handle, #cmd)
   vk->DestroyDevice = VN_GDPA(vkDestroyDevice);
   vk->AllocateMemory = VN_GDPA(vkAllocateMemory);
   vk->FreeMemory = VN_GDPA(vkFreeMemory);
   vk->MapMemory = VN_GDPA(vkMapMemory);
   vk->UnmapMemory = VN_GDPA(vkUnmapMemory);
#undef VN_GDPA
}

int
vkr_allocator_init(void)
{
   static const char *required_extensions[] = {
      "VK_KHR_external_memory_fd",
   };
   struct vkr_inst_proc_table *vk = &vkr_allocator.proc_table;
   VkResult res;

   bool ret = vkr_library_load(&vkr_allocator.vulkan_library);
   if (!ret) {
      return -1;
   }

   /* Get vkGetInstanceProcAddr from libvulkan */
   PFN_vkGetInstanceProcAddr get_proc_addr = vkr_allocator.vulkan_library.GetInstanceProcAddr;

   VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion = VK_API_VERSION_1_1,
   };

   VkInstanceCreateInfo inst_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
   };

   vk->CreateInstance =
      (PFN_vkCreateInstance)get_proc_addr(VK_NULL_HANDLE, "vkCreateInstance");
   res = vk->CreateInstance(&inst_info, NULL, &vkr_allocator.instance);
   if (res != VK_SUCCESS)
      goto fail;

   vkr_allocator_inst_proc_table_init(vkr_allocator.instance, get_proc_addr, vk);

   vkr_allocator.device_count = VKR_ALLOCATOR_MAX_DEVICE_COUNT;

   res = vk->EnumeratePhysicalDevices(vkr_allocator.instance, &vkr_allocator.device_count,
                                      vkr_allocator.physical_devices);
   if (res != VK_SUCCESS && res != VK_INCOMPLETE)
      goto fail;

   for (uint32_t i = 0; i < vkr_allocator.device_count; ++i) {
      VkPhysicalDevice physical_dev_handle = vkr_allocator.physical_devices[i];

      VkPhysicalDeviceIDProperties id_props = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES
      };
      VkPhysicalDeviceProperties2 props2 = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &id_props
      };
      vk->GetPhysicalDeviceProperties2(physical_dev_handle, &props2);

      memcpy(vkr_allocator.device_uuids[i], id_props.deviceUUID, VK_UUID_SIZE);

      float priority = 1.0;
      VkDeviceQueueCreateInfo queue_info = {
         .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         /* Use any queue since we dont really need it.
          * We are guaranteed at least one by the spec */
         .queueFamilyIndex = 0,
         .queueCount = 1,
         .pQueuePriorities = &priority
      };

      VkDeviceCreateInfo dev_info = {
         .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
         .queueCreateInfoCount = 1,
         .pQueueCreateInfos = &queue_info,
         .enabledExtensionCount = ARRAY_SIZE(required_extensions),
         .ppEnabledExtensionNames = required_extensions,
      };

      res = vk->CreateDevice(physical_dev_handle, &dev_info, NULL,
                             &vkr_allocator.devices[i]);
      if (res == VK_ERROR_EXTENSION_NOT_PRESENT) {
         vkr_allocator.devices[i] = VK_NULL_HANDLE;
         continue;
      } else if (res != VK_SUCCESS) {
         goto fail;
      }

      vkr_allocator_dev_proc_table_init(vkr_allocator.devices[i], vk->GetDeviceProcAddr,
                                        &vkr_allocator.proc_tables[i]);
   }

   list_inithead(&vkr_allocator.memories);

   return 0;

fail:
   for (uint32_t i = 0; i < vkr_allocator.device_count; ++i) {
      struct vkr_dev_proc_table *dev_vk = &vkr_allocator.proc_tables[i];
      dev_vk->DestroyDevice(vkr_allocator.devices[i], NULL);
   }
   vk->DestroyInstance(vkr_allocator.instance, NULL);

   memset(&vkr_allocator, 0, sizeof(vkr_allocator));

   vkr_library_unload(&vkr_allocator.vulkan_library);

   return -1;
}

int
vkr_allocator_resource_map(struct virgl_resource *res, void **map, uint64_t *out_size)
{
   if (!vkr_allocator_initialized) {
      if (vkr_allocator_init())
         return -EINVAL;
      vkr_allocator_initialized = true;
   }

   assert(vkr_allocator_initialized);

   struct vkr_opaque_fd_mem_info *mem_info = vkr_allocator_allocate_memory(res);
   if (!mem_info)
      return -EINVAL;

   void *ptr;
   if (mem_info->vk->MapMemory(mem_info->device, mem_info->device_memory, 0,
                               mem_info->size, 0, &ptr) != VK_SUCCESS) {
      vkr_allocator_free_memory(mem_info);
      return -EINVAL;
   }

   *map = ptr;
   *out_size = mem_info->size;

   return 0;
}

static struct vkr_opaque_fd_mem_info *
vkr_allocator_get_mem_info(struct virgl_resource *res)
{
   list_for_each_entry_safe (struct vkr_opaque_fd_mem_info, mem_info, &vkr_allocator.memories, head)
      if (mem_info->res_id == res->res_id)
         return mem_info;

   return NULL;
}

int
vkr_allocator_resource_unmap(struct virgl_resource *res)
{
   assert(vkr_allocator_initialized);

   struct vkr_opaque_fd_mem_info *mem_info = vkr_allocator_get_mem_info(res);
   if (!mem_info)
      return -EINVAL;

   mem_info->vk->UnmapMemory(mem_info->device, mem_info->device_memory);

   vkr_allocator_free_memory(mem_info);

   return 0;
}
