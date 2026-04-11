/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_buffer.h"

#include "vkr_buffer_gen.h"
#include "vkr_physical_device.h"

static void
vkr_dispatch_vkCreateBuffer(struct vn_dispatch_context *dispatch,
                            struct vn_command_vkCreateBuffer *args)
{
   /* XXX If VkExternalMemoryBufferCreateInfo is chained by the app, all is
    * good.  If it is not chained, we might still bind an external memory to
    * the buffer, because vkr_dispatch_vkAllocateMemory makes any HOST_VISIBLE
    * memory external.  That is a spec violation.
    *
    * We could unconditionally chain VkExternalMemoryBufferCreateInfo.  Or we
    * could call vkGetPhysicalDeviceExternalBufferProperties and fail
    * vkCreateBuffer if the buffer does not support external memory.  But we
    * would still end up with spec violation either way, while having a higher
    * chance of causing compatibility issues.
    *
    * In practice, drivers usually ignore VkExternalMemoryBufferCreateInfo, or
    * use it to filter out memory types in VkMemoryRequirements that do not
    * support external memory.  Binding an external memory to a buffer created
    * without VkExternalMemoryBufferCreateInfo usually works.
    *
    * To formalize this, we are potentially looking for an extension that
    * supports exporting memories without making them external.  Because they
    * are not external, they can be bound to buffers created without
    * VkExternalMemoryBufferCreateInfo.  And because they are not external, we
    * need something that is not vkGetPhysicalDeviceExternalBufferProperties
    * to determine the exportability.  See
    * vkr_physical_device_init_memory_properties as well.
    */

   vkr_buffer_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyBuffer(struct vn_dispatch_context *dispatch,
                             struct vn_command_vkDestroyBuffer *args)
{
   vkr_buffer_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetBufferMemoryRequirements(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetBufferMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetBufferMemoryRequirements_args_handle(args);
   vk->GetBufferMemoryRequirements(args->device, args->buffer, args->pMemoryRequirements);
}

static void
vkr_dispatch_vkGetBufferMemoryRequirements2(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetBufferMemoryRequirements2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetBufferMemoryRequirements2_args_handle(args);
   vk->GetBufferMemoryRequirements2(args->device, args->pInfo, args->pMemoryRequirements);
}

static void
vkr_dispatch_vkBindBufferMemory(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkBindBufferMemory *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkBindBufferMemory_args_handle(args);
   args->ret =
      vk->BindBufferMemory(args->device, args->buffer, args->memory, args->memoryOffset);
}

static void
vkr_dispatch_vkBindBufferMemory2(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkBindBufferMemory2 *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkBindBufferMemory2_args_handle(args);
   args->ret = vk->BindBufferMemory2(args->device, args->bindInfoCount, args->pBindInfos);
}

static void
vkr_dispatch_vkGetBufferOpaqueCaptureAddress(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetBufferOpaqueCaptureAddress *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetBufferOpaqueCaptureAddress_args_handle(args);
   args->ret = vk->GetBufferOpaqueCaptureAddress(args->device, args->pInfo);
}

static void
vkr_dispatch_vkGetBufferDeviceAddress(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkGetBufferDeviceAddress *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetBufferDeviceAddress_args_handle(args);
   args->ret = vk->GetBufferDeviceAddress(args->device, args->pInfo);
}

static void
vkr_dispatch_vkCreateBufferView(struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCreateBufferView *args)
{
   vkr_buffer_view_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyBufferView(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkDestroyBufferView *args)
{
   vkr_buffer_view_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetDeviceBufferMemoryRequirements(
   UNUSED struct vn_dispatch_context *ctx,
   struct vn_command_vkGetDeviceBufferMemoryRequirements *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetDeviceBufferMemoryRequirements_args_handle(args);
   vk->GetDeviceBufferMemoryRequirements(args->device, args->pInfo,
                                         args->pMemoryRequirements);
}

void
vkr_context_init_buffer_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateBuffer = vkr_dispatch_vkCreateBuffer;
   dispatch->dispatch_vkDestroyBuffer = vkr_dispatch_vkDestroyBuffer;
   dispatch->dispatch_vkGetBufferMemoryRequirements =
      vkr_dispatch_vkGetBufferMemoryRequirements;
   dispatch->dispatch_vkGetBufferMemoryRequirements2 =
      vkr_dispatch_vkGetBufferMemoryRequirements2;
   dispatch->dispatch_vkBindBufferMemory = vkr_dispatch_vkBindBufferMemory;
   dispatch->dispatch_vkBindBufferMemory2 = vkr_dispatch_vkBindBufferMemory2;
   dispatch->dispatch_vkGetBufferOpaqueCaptureAddress =
      vkr_dispatch_vkGetBufferOpaqueCaptureAddress;
   dispatch->dispatch_vkGetBufferDeviceAddress = vkr_dispatch_vkGetBufferDeviceAddress;
   dispatch->dispatch_vkGetDeviceBufferMemoryRequirements =
      vkr_dispatch_vkGetDeviceBufferMemoryRequirements;
}

void
vkr_context_init_buffer_view_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateBufferView = vkr_dispatch_vkCreateBufferView;
   dispatch->dispatch_vkDestroyBufferView = vkr_dispatch_vkDestroyBufferView;
}
