/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_host_copy.h"

#include "venus-protocol/vn_protocol_renderer_host_copy.h"

#include "vkr_context.h"
#include "vkr_device.h"

static void
vkr_dispatch_vkCopyImageToImage(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCopyImageToImage *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkCopyImageToImage_args_handle(args);
   args->ret = vk->CopyImageToImage(args->device, args->pCopyImageToImageInfo);
}

static void
vkr_dispatch_vkTransitionImageLayout(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkTransitionImageLayout *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkTransitionImageLayout_args_handle(args);
   args->ret =
      vk->TransitionImageLayout(args->device, args->transitionCount, args->pTransitions);
}

static void
vkr_dispatch_vkCopyImageToMemoryMESA(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCopyImageToMemoryMESA *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkCopyImageToMemoryMESA_args_handle(args);

   const VkCopyImageToMemoryInfoMESA *info = args->pCopyImageToMemoryInfo;
   const VkImageToMemoryCopy local_region = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY,
      .pHostPointer = args->pData,
      .memoryRowLength = info->memoryRowLength,
      .memoryImageHeight = info->memoryImageHeight,
      .imageSubresource = info->imageSubresource,
      .imageOffset = info->imageOffset,
      .imageExtent = info->imageExtent,
   };
   const VkCopyImageToMemoryInfo local_info = {
      .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO,
      .flags = info->flags,
      .srcImage = info->srcImage,
      .srcImageLayout = info->srcImageLayout,
      .regionCount = 1,
      .pRegions = &local_region,
   };
   args->ret = vk->CopyImageToMemory(args->device, &local_info);
}

static void
vkr_dispatch_vkCopyMemoryToImageMESA(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCopyMemoryToImageMESA *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkCopyMemoryToImageMESA_args_handle(args);

   const VkCopyMemoryToImageInfoMESA *info = args->pCopyMemoryToImageInfo;
   const VkMemoryToImageCopyMESA *regions = info->pRegions;

   STACK_ARRAY(VkMemoryToImageCopy, local_regions, info->regionCount);

   for (uint32_t i = 0; i < info->regionCount; i++) {
      local_regions[i] = (VkMemoryToImageCopy){
         .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY,
         .pHostPointer = regions[i].pData,
         .memoryRowLength = regions[i].memoryRowLength,
         .memoryImageHeight = regions[i].memoryImageHeight,
         .imageSubresource = regions[i].imageSubresource,
         .imageOffset = regions[i].imageOffset,
         .imageExtent = regions[i].imageExtent,
      };
   }

   const VkCopyMemoryToImageInfo local_info = {
      .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO,
      .flags = info->flags,
      .dstImage = info->dstImage,
      .dstImageLayout = info->dstImageLayout,
      .regionCount = info->regionCount,
      .pRegions = local_regions,
   };
   args->ret = vk->CopyMemoryToImage(args->device, &local_info);

   STACK_ARRAY_FINISH(local_regions);
}

void
vkr_context_init_host_copy_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCopyImageToImage = vkr_dispatch_vkCopyImageToImage;
   dispatch->dispatch_vkTransitionImageLayout = vkr_dispatch_vkTransitionImageLayout;
   dispatch->dispatch_vkCopyImageToMemoryMESA = vkr_dispatch_vkCopyImageToMemoryMESA;
   dispatch->dispatch_vkCopyMemoryToImageMESA = vkr_dispatch_vkCopyMemoryToImageMESA;
}
