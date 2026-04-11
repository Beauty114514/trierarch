/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_acceleration_structure.h"

#include "venus-protocol/vn_protocol_renderer_acceleration_structure.h"

#include "vkr_acceleration_structure_gen.h"
#include "vkr_context.h"
#include "vkr_device.h"

static void
vkr_dispatch_vkCreateAccelerationStructureKHR(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkCreateAccelerationStructureKHR *args)
{
   vkr_acceleration_structure_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyAccelerationStructureKHR(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkDestroyAccelerationStructureKHR *args)
{
   vkr_acceleration_structure_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetAccelerationStructureBuildSizesKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetAccelerationStructureBuildSizesKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetAccelerationStructureBuildSizesKHR_args_handle(args);
   vk->GetAccelerationStructureBuildSizesKHR(args->device, args->buildType,
                                             args->pBuildInfo, args->pMaxPrimitiveCounts,
                                             args->pSizeInfo);
}

static void
vkr_dispatch_vkGetAccelerationStructureDeviceAddressKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetAccelerationStructureDeviceAddressKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetAccelerationStructureDeviceAddressKHR_args_handle(args);
   args->ret = vk->GetAccelerationStructureDeviceAddressKHR(args->device, args->pInfo);
}

static void
vkr_dispatch_vkGetDeviceAccelerationStructureCompatibilityKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetDeviceAccelerationStructureCompatibilityKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetDeviceAccelerationStructureCompatibilityKHR_args_handle(args);
   vk->GetDeviceAccelerationStructureCompatibilityKHR(args->device, args->pVersionInfo,
                                                      args->pCompatibility);
}

void
vkr_context_init_acceleration_structure_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateAccelerationStructureKHR =
      vkr_dispatch_vkCreateAccelerationStructureKHR;
   dispatch->dispatch_vkDestroyAccelerationStructureKHR =
      vkr_dispatch_vkDestroyAccelerationStructureKHR;
   dispatch->dispatch_vkGetAccelerationStructureBuildSizesKHR =
      vkr_dispatch_vkGetAccelerationStructureBuildSizesKHR;
   dispatch->dispatch_vkGetAccelerationStructureDeviceAddressKHR =
      vkr_dispatch_vkGetAccelerationStructureDeviceAddressKHR;
   dispatch->dispatch_vkGetDeviceAccelerationStructureCompatibilityKHR =
      vkr_dispatch_vkGetDeviceAccelerationStructureCompatibilityKHR;

   dispatch->dispatch_vkBuildAccelerationStructuresKHR = NULL;
   dispatch->dispatch_vkCopyAccelerationStructureKHR = NULL;
   dispatch->dispatch_vkCopyAccelerationStructureToMemoryKHR = NULL;
   dispatch->dispatch_vkCopyMemoryToAccelerationStructureKHR = NULL;
   dispatch->dispatch_vkWriteAccelerationStructuresPropertiesKHR = NULL;
}
