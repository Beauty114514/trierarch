/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_command_buffer.h"

#include "vkr_command_buffer_gen.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define VKR_CMD_CALL(cmd_name, args, ...)                                                \
   do {                                                                                  \
      struct vkr_command_buffer *_cmd =                                                  \
         vkr_command_buffer_from_handle(args->commandBuffer);                            \
      struct vn_device_proc_table *_vk = &_cmd->device->proc_table;                      \
                                                                                         \
      vn_replace_vk##cmd_name##_args_handle(args);                                       \
      _vk->cmd_name(args->commandBuffer, ##__VA_ARGS__);                                 \
   } while (0)

static void
vkr_dispatch_vkCreateCommandPool(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCreateCommandPool *args)
{
   struct vkr_command_pool *pool = vkr_command_pool_create_and_add(dispatch->data, args);
   if (!pool)
      return;

   list_inithead(&pool->command_buffers);
}

static void
vkr_dispatch_vkDestroyCommandPool(struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkDestroyCommandPool *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vkr_command_pool *pool = vkr_command_pool_from_handle(args->commandPool);

   if (!pool)
      return;

   vkr_command_pool_release(ctx, pool);
   vkr_command_pool_destroy_and_remove(ctx, args);
}

static void
vkr_dispatch_vkResetCommandPool(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkResetCommandPool *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkResetCommandPool_args_handle(args);
   args->ret = vk->ResetCommandPool(args->device, args->commandPool, args->flags);
}

static void
vkr_dispatch_vkTrimCommandPool(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkTrimCommandPool *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkTrimCommandPool_args_handle(args);
   vk->TrimCommandPool(args->device, args->commandPool, args->flags);
}

static void
vkr_dispatch_vkAllocateCommandBuffers(struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkAllocateCommandBuffers *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vkr_command_pool *pool =
      vkr_command_pool_from_handle(args->pAllocateInfo->commandPool);
   struct object_array arr;

   if (!pool) {
      vkr_context_set_fatal(ctx);
      return;
   }

   if (vkr_command_buffer_create_array(ctx, args, &arr) != VK_SUCCESS)
      return;

   vkr_command_buffer_add_array(ctx, dev, pool, &arr);
}

static void
vkr_dispatch_vkFreeCommandBuffers(struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkFreeCommandBuffers *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct list_head free_list;

   /* args->pCommandBuffers is marked noautovalidity="true" */
   if (args->commandBufferCount && !args->pCommandBuffers) {
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_command_buffer_destroy_driver_handles(ctx, args, &free_list);
   vkr_context_remove_objects(ctx, &free_list);
}

static void
vkr_dispatch_vkResetCommandBuffer(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkResetCommandBuffer *args)
{
   struct vkr_command_buffer *cmd = vkr_command_buffer_from_handle(args->commandBuffer);
   struct vn_device_proc_table *vk = &cmd->device->proc_table;

   vn_replace_vkResetCommandBuffer_args_handle(args);
   args->ret = vk->ResetCommandBuffer(args->commandBuffer, args->flags);
}

static void
vkr_dispatch_vkBeginCommandBuffer(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkBeginCommandBuffer *args)
{
   TRACE_FUNC();
   struct vkr_command_buffer *cmd = vkr_command_buffer_from_handle(args->commandBuffer);
   struct vn_device_proc_table *vk = &cmd->device->proc_table;

   vn_replace_vkBeginCommandBuffer_args_handle(args);
   args->ret = vk->BeginCommandBuffer(args->commandBuffer, args->pBeginInfo);
}

static void
vkr_dispatch_vkEndCommandBuffer(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkEndCommandBuffer *args)
{
   TRACE_FUNC();
   struct vkr_command_buffer *cmd = vkr_command_buffer_from_handle(args->commandBuffer);
   struct vn_device_proc_table *vk = &cmd->device->proc_table;

   vn_replace_vkEndCommandBuffer_args_handle(args);
   args->ret = vk->EndCommandBuffer(args->commandBuffer);
}

static void
vkr_dispatch_vkCmdBindPipeline(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdBindPipeline *args)
{
   VKR_CMD_CALL(CmdBindPipeline, args, args->pipelineBindPoint, args->pipeline);
}

static void
vkr_dispatch_vkCmdSetViewport(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCmdSetViewport *args)
{
   VKR_CMD_CALL(CmdSetViewport, args, args->firstViewport, args->viewportCount,
                args->pViewports);
}

static void
vkr_dispatch_vkCmdSetScissor(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdSetScissor *args)
{
   VKR_CMD_CALL(CmdSetScissor, args, args->firstScissor, args->scissorCount,
                args->pScissors);
}

static void
vkr_dispatch_vkCmdSetLineWidth(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdSetLineWidth *args)
{
   VKR_CMD_CALL(CmdSetLineWidth, args, args->lineWidth);
}

static void
vkr_dispatch_vkCmdSetDepthBias(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdSetDepthBias *args)
{
   VKR_CMD_CALL(CmdSetDepthBias, args, args->depthBiasConstantFactor,
                args->depthBiasClamp, args->depthBiasSlopeFactor);
}

static void
vkr_dispatch_vkCmdSetBlendConstants(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdSetBlendConstants *args)
{
   VKR_CMD_CALL(CmdSetBlendConstants, args, args->blendConstants);
}

static void
vkr_dispatch_vkCmdSetDepthBounds(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdSetDepthBounds *args)
{
   VKR_CMD_CALL(CmdSetDepthBounds, args, args->minDepthBounds, args->maxDepthBounds);
}

static void
vkr_dispatch_vkCmdSetStencilCompareMask(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkCmdSetStencilCompareMask *args)
{
   VKR_CMD_CALL(CmdSetStencilCompareMask, args, args->faceMask, args->compareMask);
}

static void
vkr_dispatch_vkCmdSetStencilWriteMask(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdSetStencilWriteMask *args)
{
   VKR_CMD_CALL(CmdSetStencilWriteMask, args, args->faceMask, args->writeMask);
}

static void
vkr_dispatch_vkCmdSetStencilReference(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdSetStencilReference *args)
{
   VKR_CMD_CALL(CmdSetStencilReference, args, args->faceMask, args->reference);
}

static void
vkr_dispatch_vkCmdBindDescriptorSets(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdBindDescriptorSets *args)
{
   VKR_CMD_CALL(CmdBindDescriptorSets, args, args->pipelineBindPoint, args->layout,
                args->firstSet, args->descriptorSetCount, args->pDescriptorSets,
                args->dynamicOffsetCount, args->pDynamicOffsets);
}

static void
vkr_dispatch_vkCmdBindIndexBuffer(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCmdBindIndexBuffer *args)
{
   VKR_CMD_CALL(CmdBindIndexBuffer, args, args->buffer, args->offset, args->indexType);
}

static void
vkr_dispatch_vkCmdBindVertexBuffers(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdBindVertexBuffers *args)
{
   VKR_CMD_CALL(CmdBindVertexBuffers, args, args->firstBinding, args->bindingCount,
                args->pBuffers, args->pOffsets);
}

static void
vkr_dispatch_vkCmdDraw(UNUSED struct vn_dispatch_context *dispatch,
                       struct vn_command_vkCmdDraw *args)
{
   VKR_CMD_CALL(CmdDraw, args, args->vertexCount, args->instanceCount, args->firstVertex,
                args->firstInstance);
}

static void
vkr_dispatch_vkCmdDrawIndexed(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCmdDrawIndexed *args)
{
   VKR_CMD_CALL(CmdDrawIndexed, args, args->indexCount, args->instanceCount,
                args->firstIndex, args->vertexOffset, args->firstInstance);
}

static void
vkr_dispatch_vkCmdDrawIndirect(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdDrawIndirect *args)
{
   VKR_CMD_CALL(CmdDrawIndirect, args, args->buffer, args->offset, args->drawCount,
                args->stride);
}

static void
vkr_dispatch_vkCmdDrawIndexedIndirect(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdDrawIndexedIndirect *args)
{
   VKR_CMD_CALL(CmdDrawIndexedIndirect, args, args->buffer, args->offset, args->drawCount,
                args->stride);
}

static void
vkr_dispatch_vkCmdDispatch(UNUSED struct vn_dispatch_context *dispatch,
                           struct vn_command_vkCmdDispatch *args)
{
   VKR_CMD_CALL(CmdDispatch, args, args->groupCountX, args->groupCountY,
                args->groupCountZ);
}

static void
vkr_dispatch_vkCmdDispatchIndirect(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdDispatchIndirect *args)
{
   VKR_CMD_CALL(CmdDispatchIndirect, args, args->buffer, args->offset);
}

static void
vkr_dispatch_vkCmdCopyBuffer(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdCopyBuffer *args)
{
   VKR_CMD_CALL(CmdCopyBuffer, args, args->srcBuffer, args->dstBuffer, args->regionCount,
                args->pRegions);
}

static void
vkr_dispatch_vkCmdCopyBuffer2(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCmdCopyBuffer2 *args)
{
   VKR_CMD_CALL(CmdCopyBuffer2, args, args->pCopyBufferInfo);
}

static void
vkr_dispatch_vkCmdCopyImage(UNUSED struct vn_dispatch_context *dispatch,
                            struct vn_command_vkCmdCopyImage *args)
{
   VKR_CMD_CALL(CmdCopyImage, args, args->srcImage, args->srcImageLayout, args->dstImage,
                args->dstImageLayout, args->regionCount, args->pRegions);
}

static void
vkr_dispatch_vkCmdCopyImage2(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdCopyImage2 *args)
{
   VKR_CMD_CALL(CmdCopyImage2, args, args->pCopyImageInfo);
}

static void
vkr_dispatch_vkCmdBlitImage(UNUSED struct vn_dispatch_context *dispatch,
                            struct vn_command_vkCmdBlitImage *args)
{
   VKR_CMD_CALL(CmdBlitImage, args, args->srcImage, args->srcImageLayout, args->dstImage,
                args->dstImageLayout, args->regionCount, args->pRegions, args->filter);
}

static void
vkr_dispatch_vkCmdBlitImage2(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdBlitImage2 *args)
{
   VKR_CMD_CALL(CmdBlitImage2, args, args->pBlitImageInfo);
}

static void
vkr_dispatch_vkCmdCopyBufferToImage(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdCopyBufferToImage *args)
{
   VKR_CMD_CALL(CmdCopyBufferToImage, args, args->srcBuffer, args->dstImage,
                args->dstImageLayout, args->regionCount, args->pRegions);
}

static void
vkr_dispatch_vkCmdCopyBufferToImage2(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdCopyBufferToImage2 *args)
{
   VKR_CMD_CALL(CmdCopyBufferToImage2, args, args->pCopyBufferToImageInfo);
}

static void
vkr_dispatch_vkCmdCopyImageToBuffer(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdCopyImageToBuffer *args)
{
   VKR_CMD_CALL(CmdCopyImageToBuffer, args, args->srcImage, args->srcImageLayout,
                args->dstBuffer, args->regionCount, args->pRegions);
}

static void
vkr_dispatch_vkCmdCopyImageToBuffer2(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdCopyImageToBuffer2 *args)
{
   VKR_CMD_CALL(CmdCopyImageToBuffer2, args, args->pCopyImageToBufferInfo);
}

static void
vkr_dispatch_vkCmdUpdateBuffer(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdUpdateBuffer *args)
{
   VKR_CMD_CALL(CmdUpdateBuffer, args, args->dstBuffer, args->dstOffset, args->dataSize,
                args->pData);
}

static void
vkr_dispatch_vkCmdFillBuffer(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdFillBuffer *args)
{
   VKR_CMD_CALL(CmdFillBuffer, args, args->dstBuffer, args->dstOffset, args->size,
                args->data);
}

static void
vkr_dispatch_vkCmdClearColorImage(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCmdClearColorImage *args)
{
   VKR_CMD_CALL(CmdClearColorImage, args, args->image, args->imageLayout, args->pColor,
                args->rangeCount, args->pRanges);
}

static void
vkr_dispatch_vkCmdClearDepthStencilImage(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdClearDepthStencilImage *args)
{
   VKR_CMD_CALL(CmdClearDepthStencilImage, args, args->image, args->imageLayout,
                args->pDepthStencil, args->rangeCount, args->pRanges);
}

static void
vkr_dispatch_vkCmdClearAttachments(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdClearAttachments *args)
{
   VKR_CMD_CALL(CmdClearAttachments, args, args->attachmentCount, args->pAttachments,
                args->rectCount, args->pRects);
}

static void
vkr_dispatch_vkCmdResolveImage(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdResolveImage *args)
{
   VKR_CMD_CALL(CmdResolveImage, args, args->srcImage, args->srcImageLayout,
                args->dstImage, args->dstImageLayout, args->regionCount, args->pRegions);
}

static void
vkr_dispatch_vkCmdResolveImage2(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCmdResolveImage2 *args)
{
   VKR_CMD_CALL(CmdResolveImage2, args, args->pResolveImageInfo);
}

static void
vkr_dispatch_vkCmdSetEvent(UNUSED struct vn_dispatch_context *dispatch,
                           struct vn_command_vkCmdSetEvent *args)
{
   VKR_CMD_CALL(CmdSetEvent, args, args->event, args->stageMask);
}

static void
vkr_dispatch_vkCmdResetEvent(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdResetEvent *args)
{
   VKR_CMD_CALL(CmdResetEvent, args, args->event, args->stageMask);
}

static void
vkr_dispatch_vkCmdWaitEvents(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdWaitEvents *args)
{
   VKR_CMD_CALL(CmdWaitEvents, args, args->eventCount, args->pEvents, args->srcStageMask,
                args->dstStageMask, args->memoryBarrierCount, args->pMemoryBarriers,
                args->bufferMemoryBarrierCount, args->pBufferMemoryBarriers,
                args->imageMemoryBarrierCount, args->pImageMemoryBarriers);
}

static void
vkr_dispatch_vkCmdPipelineBarrier(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCmdPipelineBarrier *args)
{
   VKR_CMD_CALL(CmdPipelineBarrier, args, args->srcStageMask, args->dstStageMask,
                args->dependencyFlags, args->memoryBarrierCount, args->pMemoryBarriers,
                args->bufferMemoryBarrierCount, args->pBufferMemoryBarriers,
                args->imageMemoryBarrierCount, args->pImageMemoryBarriers);
}

static void
vkr_dispatch_vkCmdBeginQuery(UNUSED struct vn_dispatch_context *dispatch,
                             struct vn_command_vkCmdBeginQuery *args)
{
   VKR_CMD_CALL(CmdBeginQuery, args, args->queryPool, args->query, args->flags);
}

static void
vkr_dispatch_vkCmdEndQuery(UNUSED struct vn_dispatch_context *dispatch,
                           struct vn_command_vkCmdEndQuery *args)
{
   VKR_CMD_CALL(CmdEndQuery, args, args->queryPool, args->query);
}

static void
vkr_dispatch_vkCmdResetQueryPool(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdResetQueryPool *args)
{
   VKR_CMD_CALL(CmdResetQueryPool, args, args->queryPool, args->firstQuery,
                args->queryCount);
}

static void
vkr_dispatch_vkCmdWriteTimestamp(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdWriteTimestamp *args)
{
   VKR_CMD_CALL(CmdWriteTimestamp, args, args->pipelineStage, args->queryPool,
                args->query);
}

static void
vkr_dispatch_vkCmdCopyQueryPoolResults(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdCopyQueryPoolResults *args)
{
   VKR_CMD_CALL(CmdCopyQueryPoolResults, args, args->queryPool, args->firstQuery,
                args->queryCount, args->dstBuffer, args->dstOffset, args->stride,
                args->flags);
}

static void
vkr_dispatch_vkCmdPushConstants(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCmdPushConstants *args)
{
   VKR_CMD_CALL(CmdPushConstants, args, args->layout, args->stageFlags, args->offset,
                args->size, args->pValues);
}

static void
vkr_dispatch_vkCmdBeginRenderPass(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCmdBeginRenderPass *args)
{
   VKR_CMD_CALL(CmdBeginRenderPass, args, args->pRenderPassBegin, args->contents);
}

static void
vkr_dispatch_vkCmdNextSubpass(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCmdNextSubpass *args)
{
   VKR_CMD_CALL(CmdNextSubpass, args, args->contents);
}

static void
vkr_dispatch_vkCmdEndRenderPass(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCmdEndRenderPass *args)
{
   VKR_CMD_CALL(CmdEndRenderPass, args);
}

static void
vkr_dispatch_vkCmdExecuteCommands(UNUSED struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCmdExecuteCommands *args)
{
   VKR_CMD_CALL(CmdExecuteCommands, args, args->commandBufferCount,
                args->pCommandBuffers);
}

static void
vkr_dispatch_vkCmdSetDeviceMask(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCmdSetDeviceMask *args)
{
   VKR_CMD_CALL(CmdSetDeviceMask, args, args->deviceMask);
}

static void
vkr_dispatch_vkCmdDispatchBase(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdDispatchBase *args)
{
   VKR_CMD_CALL(CmdDispatchBase, args, args->baseGroupX, args->baseGroupY,
                args->baseGroupZ, args->groupCountX, args->groupCountY,
                args->groupCountZ);
}

static void
vkr_dispatch_vkCmdBeginRenderPass2(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdBeginRenderPass2 *args)
{
   VKR_CMD_CALL(CmdBeginRenderPass2, args, args->pRenderPassBegin,
                args->pSubpassBeginInfo);
}

static void
vkr_dispatch_vkCmdNextSubpass2(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdNextSubpass2 *args)
{
   VKR_CMD_CALL(CmdNextSubpass2, args, args->pSubpassBeginInfo, args->pSubpassEndInfo);
}

static void
vkr_dispatch_vkCmdEndRenderPass2(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdEndRenderPass2 *args)
{
   VKR_CMD_CALL(CmdEndRenderPass2, args, args->pSubpassEndInfo);
}

static void
vkr_dispatch_vkCmdDrawIndirectCount(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdDrawIndirectCount *args)
{
   VKR_CMD_CALL(CmdDrawIndirectCount, args, args->buffer, args->offset, args->countBuffer,
                args->countBufferOffset, args->maxDrawCount, args->stride);
}

static void
vkr_dispatch_vkCmdDrawIndexedIndirectCount(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdDrawIndexedIndirectCount *args)
{
   VKR_CMD_CALL(CmdDrawIndexedIndirectCount, args, args->buffer, args->offset,
                args->countBuffer, args->countBufferOffset, args->maxDrawCount,
                args->stride);
}

static void
vkr_dispatch_vkCmdSetLineStipple(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdSetLineStipple *args)
{
   VKR_CMD_CALL(CmdSetLineStipple, args, args->lineStippleFactor,
                args->lineStipplePattern);
}

static void
vkr_dispatch_vkCmdBindTransformFeedbackBuffersEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdBindTransformFeedbackBuffersEXT *args)
{
   VKR_CMD_CALL(CmdBindTransformFeedbackBuffersEXT, args, args->firstBinding,
                args->bindingCount, args->pBuffers, args->pOffsets, args->pSizes);
}

static void
vkr_dispatch_vkCmdBeginTransformFeedbackEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdBeginTransformFeedbackEXT *args)
{
   VKR_CMD_CALL(CmdBeginTransformFeedbackEXT, args, args->firstCounterBuffer,
                args->counterBufferCount, args->pCounterBuffers,
                args->pCounterBufferOffsets);
}

static void
vkr_dispatch_vkCmdEndTransformFeedbackEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdEndTransformFeedbackEXT *args)
{
   VKR_CMD_CALL(CmdEndTransformFeedbackEXT, args, args->firstCounterBuffer,
                args->counterBufferCount, args->pCounterBuffers,
                args->pCounterBufferOffsets);
}

static void
vkr_dispatch_vkCmdBeginQueryIndexedEXT(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdBeginQueryIndexedEXT *args)
{
   VKR_CMD_CALL(CmdBeginQueryIndexedEXT, args, args->queryPool, args->query, args->flags,
                args->index);
}

static void
vkr_dispatch_vkCmdEndQueryIndexedEXT(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdEndQueryIndexedEXT *args)
{
   VKR_CMD_CALL(CmdEndQueryIndexedEXT, args, args->queryPool, args->query, args->index);
}

static void
vkr_dispatch_vkCmdDrawIndirectByteCountEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdDrawIndirectByteCountEXT *args)
{
   VKR_CMD_CALL(CmdDrawIndirectByteCountEXT, args, args->instanceCount,
                args->firstInstance, args->counterBuffer, args->counterBufferOffset,
                args->counterOffset, args->vertexStride);
}

static void
vkr_dispatch_vkCmdBindVertexBuffers2(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdBindVertexBuffers2 *args)
{
   VKR_CMD_CALL(CmdBindVertexBuffers2, args, args->firstBinding, args->bindingCount,
                args->pBuffers, args->pOffsets, args->pSizes, args->pStrides);
}

static void
vkr_dispatch_vkCmdBindIndexBuffer2(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdBindIndexBuffer2 *args)
{
   VKR_CMD_CALL(CmdBindIndexBuffer2, args, args->buffer, args->offset, args->size,
                args->indexType);
}

static void
vkr_dispatch_vkCmdSetCullMode(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCmdSetCullMode *args)
{
   VKR_CMD_CALL(CmdSetCullMode, args, args->cullMode);
}

static void
vkr_dispatch_vkCmdSetDepthBoundsTestEnable(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetDepthBoundsTestEnable *args)
{
   VKR_CMD_CALL(CmdSetDepthBoundsTestEnable, args, args->depthBoundsTestEnable);
}

static void
vkr_dispatch_vkCmdSetDepthCompareOp(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdSetDepthCompareOp *args)
{
   VKR_CMD_CALL(CmdSetDepthCompareOp, args, args->depthCompareOp);
}

static void
vkr_dispatch_vkCmdSetDepthTestEnable(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdSetDepthTestEnable *args)
{
   VKR_CMD_CALL(CmdSetDepthTestEnable, args, args->depthTestEnable);
}

static void
vkr_dispatch_vkCmdSetDepthWriteEnable(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdSetDepthWriteEnable *args)
{
   VKR_CMD_CALL(CmdSetDepthWriteEnable, args, args->depthWriteEnable);
}

static void
vkr_dispatch_vkCmdSetFrontFace(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdSetFrontFace *args)
{
   VKR_CMD_CALL(CmdSetFrontFace, args, args->frontFace);
}

static void
vkr_dispatch_vkCmdSetPrimitiveTopology(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdSetPrimitiveTopology *args)
{
   VKR_CMD_CALL(CmdSetPrimitiveTopology, args, args->primitiveTopology);
}

static void
vkr_dispatch_vkCmdSetScissorWithCount(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdSetScissorWithCount *args)
{
   VKR_CMD_CALL(CmdSetScissorWithCount, args, args->scissorCount, args->pScissors);
}

static void
vkr_dispatch_vkCmdSetStencilOp(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdSetStencilOp *args)
{
   VKR_CMD_CALL(CmdSetStencilOp, args, args->faceMask, args->failOp, args->passOp,
                args->depthFailOp, args->compareOp);
}

static void
vkr_dispatch_vkCmdSetStencilTestEnable(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdSetStencilTestEnable *args)
{
   VKR_CMD_CALL(CmdSetStencilTestEnable, args, args->stencilTestEnable);
}

static void
vkr_dispatch_vkCmdSetViewportWithCount(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdSetViewportWithCount *args)
{
   VKR_CMD_CALL(CmdSetViewportWithCount, args, args->viewportCount, args->pViewports);
}

static void
vkr_dispatch_vkCmdSetDepthBiasEnable(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdSetDepthBiasEnable *args)
{
   VKR_CMD_CALL(CmdSetDepthBiasEnable, args, args->depthBiasEnable);
}

static void
vkr_dispatch_vkCmdSetLogicOpEXT(UNUSED struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCmdSetLogicOpEXT *args)
{
   VKR_CMD_CALL(CmdSetLogicOpEXT, args, args->logicOp);
}

static void
vkr_dispatch_vkCmdSetPatchControlPointsEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetPatchControlPointsEXT *args)
{
   VKR_CMD_CALL(CmdSetPatchControlPointsEXT, args, args->patchControlPoints);
}

static void
vkr_dispatch_vkCmdSetPrimitiveRestartEnable(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetPrimitiveRestartEnable *args)
{
   VKR_CMD_CALL(CmdSetPrimitiveRestartEnable, args, args->primitiveRestartEnable);
}

static void
vkr_dispatch_vkCmdSetRasterizerDiscardEnable(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRasterizerDiscardEnable *args)
{
   VKR_CMD_CALL(CmdSetRasterizerDiscardEnable, args, args->rasterizerDiscardEnable);
}

static void
vkr_dispatch_vkCmdBeginConditionalRenderingEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdBeginConditionalRenderingEXT *args)
{
   VKR_CMD_CALL(CmdBeginConditionalRenderingEXT, args, args->pConditionalRenderingBegin);
}

static void
vkr_dispatch_vkCmdEndConditionalRenderingEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdEndConditionalRenderingEXT *args)
{
   VKR_CMD_CALL(CmdEndConditionalRenderingEXT, args);
}

static void
vkr_dispatch_vkCmdBeginRendering(UNUSED struct vn_dispatch_context *ctx,
                                 struct vn_command_vkCmdBeginRendering *args)
{
   VKR_CMD_CALL(CmdBeginRendering, args, args->pRenderingInfo);
}

static void
vkr_dispatch_vkCmdEndRendering(UNUSED struct vn_dispatch_context *ctx,
                               struct vn_command_vkCmdEndRendering *args)
{
   VKR_CMD_CALL(CmdEndRendering, args);
}

static void
vkr_dispatch_vkCmdPipelineBarrier2(UNUSED struct vn_dispatch_context *ctx,
                                   struct vn_command_vkCmdPipelineBarrier2 *args)
{
   VKR_CMD_CALL(CmdPipelineBarrier2, args, args->pDependencyInfo);
}

static void
vkr_dispatch_vkCmdResetEvent2(UNUSED struct vn_dispatch_context *ctx,
                              struct vn_command_vkCmdResetEvent2 *args)
{
   VKR_CMD_CALL(CmdResetEvent2, args, args->event, args->stageMask);
}

static void
vkr_dispatch_vkCmdSetEvent2(UNUSED struct vn_dispatch_context *ctx,
                            struct vn_command_vkCmdSetEvent2 *args)
{
   VKR_CMD_CALL(CmdSetEvent2, args, args->event, args->pDependencyInfo);
}

static void
vkr_dispatch_vkCmdWaitEvents2(UNUSED struct vn_dispatch_context *ctx,
                              struct vn_command_vkCmdWaitEvents2 *args)
{
   VKR_CMD_CALL(CmdWaitEvents2, args, args->eventCount, args->pEvents,
                args->pDependencyInfos);
}

static void
vkr_dispatch_vkCmdWriteTimestamp2(UNUSED struct vn_dispatch_context *ctx,
                                  struct vn_command_vkCmdWriteTimestamp2 *args)
{
   VKR_CMD_CALL(CmdWriteTimestamp2, args, args->stage, args->queryPool, args->query);
}

static void
vkr_dispatch_vkCmdDrawMultiEXT(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdDrawMultiEXT *args)
{
   VKR_CMD_CALL(CmdDrawMultiEXT, args, args->drawCount, args->pVertexInfo,
                args->instanceCount, args->firstInstance, args->stride);
}

static void
vkr_dispatch_vkCmdDrawMultiIndexedEXT(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdDrawMultiIndexedEXT *args)
{
   VKR_CMD_CALL(CmdDrawMultiIndexedEXT, args, args->drawCount, args->pIndexInfo,
                args->instanceCount, args->firstInstance, args->stride,
                args->pVertexOffset);
}

static void
vkr_dispatch_vkCmdPushDescriptorSet(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdPushDescriptorSet *args)
{
   VKR_CMD_CALL(CmdPushDescriptorSet, args, args->pipelineBindPoint, args->layout,
                args->set, args->descriptorWriteCount, args->pDescriptorWrites);
}

static void
vkr_dispatch_vkCmdSetColorWriteEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetColorWriteEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetColorWriteEnableEXT, args, args->attachmentCount,
                args->pColorWriteEnables);
}

static void
vkr_dispatch_vkCmdSetVertexInputEXT(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdSetVertexInputEXT *args)
{
   VKR_CMD_CALL(CmdSetVertexInputEXT, args, args->vertexBindingDescriptionCount,
                args->pVertexBindingDescriptions, args->vertexAttributeDescriptionCount,
                args->pVertexAttributeDescriptions);
}

static void
vkr_dispatch_vkCmdSetAlphaToCoverageEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetAlphaToCoverageEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetAlphaToCoverageEnableEXT, args, args->alphaToCoverageEnable);
}

static void
vkr_dispatch_vkCmdSetAlphaToOneEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetAlphaToOneEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetAlphaToOneEnableEXT, args, args->alphaToOneEnable);
}

static void
vkr_dispatch_vkCmdSetColorBlendAdvancedEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetColorBlendAdvancedEXT *args)
{
   VKR_CMD_CALL(CmdSetColorBlendAdvancedEXT, args, args->firstAttachment,
                args->attachmentCount, args->pColorBlendAdvanced);
}

static void
vkr_dispatch_vkCmdSetColorBlendEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetColorBlendEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetColorBlendEnableEXT, args, args->firstAttachment,
                args->attachmentCount, args->pColorBlendEnables);
}

static void
vkr_dispatch_vkCmdSetColorBlendEquationEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetColorBlendEquationEXT *args)
{
   VKR_CMD_CALL(CmdSetColorBlendEquationEXT, args, args->firstAttachment,
                args->attachmentCount, args->pColorBlendEquations);
}

static void
vkr_dispatch_vkCmdSetColorWriteMaskEXT(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdSetColorWriteMaskEXT *args)
{
   VKR_CMD_CALL(CmdSetColorWriteMaskEXT, args, args->firstAttachment,
                args->attachmentCount, args->pColorWriteMasks);
}

static void
vkr_dispatch_vkCmdSetConservativeRasterizationModeEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetConservativeRasterizationModeEXT *args)
{
   VKR_CMD_CALL(CmdSetConservativeRasterizationModeEXT, args,
                args->conservativeRasterizationMode);
}

static void
vkr_dispatch_vkCmdSetDepthClampEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetDepthClampEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetDepthClampEnableEXT, args, args->depthClampEnable);
}

static void
vkr_dispatch_vkCmdSetDepthClipEnableEXT(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkCmdSetDepthClipEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetDepthClipEnableEXT, args, args->depthClipEnable);
}

static void
vkr_dispatch_vkCmdSetDepthClipNegativeOneToOneEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetDepthClipNegativeOneToOneEXT *args)
{
   VKR_CMD_CALL(CmdSetDepthClipNegativeOneToOneEXT, args, args->negativeOneToOne);
}

static void
vkr_dispatch_vkCmdSetExtraPrimitiveOverestimationSizeEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetExtraPrimitiveOverestimationSizeEXT *args)
{
   VKR_CMD_CALL(CmdSetExtraPrimitiveOverestimationSizeEXT, args,
                args->extraPrimitiveOverestimationSize);
}

static void
vkr_dispatch_vkCmdSetLineRasterizationModeEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetLineRasterizationModeEXT *args)
{
   VKR_CMD_CALL(CmdSetLineRasterizationModeEXT, args, args->lineRasterizationMode);
}

static void
vkr_dispatch_vkCmdSetLineStippleEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetLineStippleEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetLineStippleEnableEXT, args, args->stippledLineEnable);
}

static void
vkr_dispatch_vkCmdSetLogicOpEnableEXT(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdSetLogicOpEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetLogicOpEnableEXT, args, args->logicOpEnable);
}

static void
vkr_dispatch_vkCmdSetPolygonModeEXT(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCmdSetPolygonModeEXT *args)
{
   VKR_CMD_CALL(CmdSetPolygonModeEXT, args, args->polygonMode);
}

static void
vkr_dispatch_vkCmdSetProvokingVertexModeEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetProvokingVertexModeEXT *args)
{
   VKR_CMD_CALL(CmdSetProvokingVertexModeEXT, args, args->provokingVertexMode);
}

static void
vkr_dispatch_vkCmdSetRasterizationSamplesEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRasterizationSamplesEXT *args)
{
   VKR_CMD_CALL(CmdSetRasterizationSamplesEXT, args, args->rasterizationSamples);
}

static void
vkr_dispatch_vkCmdSetRasterizationStreamEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRasterizationStreamEXT *args)
{
   VKR_CMD_CALL(CmdSetRasterizationStreamEXT, args, args->rasterizationStream);
}

static void
vkr_dispatch_vkCmdSetSampleLocationsEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetSampleLocationsEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetSampleLocationsEnableEXT, args, args->sampleLocationsEnable);
}

static void
vkr_dispatch_vkCmdSetSampleMaskEXT(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdSetSampleMaskEXT *args)
{
   VKR_CMD_CALL(CmdSetSampleMaskEXT, args, args->samples, args->pSampleMask);
}

static void
vkr_dispatch_vkCmdSetTessellationDomainOriginEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetTessellationDomainOriginEXT *args)
{
   VKR_CMD_CALL(CmdSetTessellationDomainOriginEXT, args, args->domainOrigin);
}

static void
vkr_dispatch_vkCmdSetFragmentShadingRateKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetFragmentShadingRateKHR *args)
{
   VKR_CMD_CALL(CmdSetFragmentShadingRateKHR, args, args->pFragmentSize,
                args->combinerOps);
}

static void
vkr_dispatch_vkCmdSetSampleLocationsEXT(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkCmdSetSampleLocationsEXT *args)
{
   VKR_CMD_CALL(CmdSetSampleLocationsEXT, args, args->pSampleLocationsInfo);
}

static void
vkr_dispatch_vkCmdSetRenderingAttachmentLocations(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRenderingAttachmentLocations *args)
{
   VKR_CMD_CALL(CmdSetRenderingAttachmentLocations, args, args->pLocationInfo);
}

static void
vkr_dispatch_vkCmdSetRenderingInputAttachmentIndices(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRenderingInputAttachmentIndices *args)
{
   VKR_CMD_CALL(CmdSetRenderingInputAttachmentIndices, args,
                args->pInputAttachmentIndexInfo);
}

static void
vkr_dispatch_vkCmdBindDescriptorSets2(UNUSED struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCmdBindDescriptorSets2 *args)
{
   VKR_CMD_CALL(CmdBindDescriptorSets2, args, args->pBindDescriptorSetsInfo);
}

static void
vkr_dispatch_vkCmdPushConstants2(UNUSED struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCmdPushConstants2 *args)
{
   VKR_CMD_CALL(CmdPushConstants2, args, args->pPushConstantsInfo);
}

static void
vkr_dispatch_vkCmdPushDescriptorSet2(UNUSED struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkCmdPushDescriptorSet2 *args)
{
   VKR_CMD_CALL(CmdPushDescriptorSet2, args, args->pPushDescriptorSetInfo);
}

static void
vkr_dispatch_vkCmdBuildAccelerationStructuresIndirectKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdBuildAccelerationStructuresIndirectKHR *args)
{
   VKR_CMD_CALL(CmdBuildAccelerationStructuresIndirectKHR, args, args->infoCount,
                args->pInfos, args->pIndirectDeviceAddresses, args->pIndirectStrides,
                args->ppMaxPrimitiveCounts);
}

static void
vkr_dispatch_vkCmdBuildAccelerationStructuresKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdBuildAccelerationStructuresKHR *args)
{
   VKR_CMD_CALL(CmdBuildAccelerationStructuresKHR, args, args->infoCount, args->pInfos,
                args->ppBuildRangeInfos);
}

static void
vkr_dispatch_vkCmdCopyAccelerationStructureKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdCopyAccelerationStructureKHR *args)
{
   VKR_CMD_CALL(CmdCopyAccelerationStructureKHR, args, args->pInfo);
}

static void
vkr_dispatch_vkCmdCopyAccelerationStructureToMemoryKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdCopyAccelerationStructureToMemoryKHR *args)
{
   VKR_CMD_CALL(CmdCopyAccelerationStructureToMemoryKHR, args, args->pInfo);
}

static void
vkr_dispatch_vkCmdCopyMemoryToAccelerationStructureKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdCopyMemoryToAccelerationStructureKHR *args)
{
   VKR_CMD_CALL(CmdCopyMemoryToAccelerationStructureKHR, args, args->pInfo);
}

static void
vkr_dispatch_vkCmdWriteAccelerationStructuresPropertiesKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdWriteAccelerationStructuresPropertiesKHR *args)
{
   VKR_CMD_CALL(CmdWriteAccelerationStructuresPropertiesKHR, args,
                args->accelerationStructureCount, args->pAccelerationStructures,
                args->queryType, args->queryPool, args->firstQuery);
}

static void
vkr_dispatch_vkCmdSetRayTracingPipelineStackSizeKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetRayTracingPipelineStackSizeKHR *args)
{
   VKR_CMD_CALL(CmdSetRayTracingPipelineStackSizeKHR, args, args->pipelineStackSize);
}

static void
vkr_dispatch_vkCmdTraceRaysIndirectKHR(UNUSED struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCmdTraceRaysIndirectKHR *args)
{
   VKR_CMD_CALL(CmdTraceRaysIndirectKHR, args, args->pRaygenShaderBindingTable,
                args->pMissShaderBindingTable, args->pHitShaderBindingTable,
                args->pCallableShaderBindingTable, args->indirectDeviceAddress);
}

static void
vkr_dispatch_vkCmdTraceRaysKHR(UNUSED struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCmdTraceRaysKHR *args)
{
   VKR_CMD_CALL(CmdTraceRaysKHR, args, args->pRaygenShaderBindingTable,
                args->pMissShaderBindingTable, args->pHitShaderBindingTable,
                args->pCallableShaderBindingTable, args->width, args->height,
                args->depth);
}

static void
vkr_dispatch_vkCmdTraceRaysIndirect2KHR(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkCmdTraceRaysIndirect2KHR *args)
{
   VKR_CMD_CALL(CmdTraceRaysIndirect2KHR, args, args->indirectDeviceAddress);
}

static void
vkr_dispatch_vkCmdSetDepthBias2EXT(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdSetDepthBias2EXT *args)
{
   VKR_CMD_CALL(CmdSetDepthBias2EXT, args, args->pDepthBiasInfo);
}

static void
vkr_dispatch_vkCmdSetDepthClampRangeEXT(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkCmdSetDepthClampRangeEXT *args)
{
   VKR_CMD_CALL(CmdSetDepthClampRangeEXT, args, args->depthClampMode,
                args->pDepthClampRange);
}

static void
vkr_dispatch_vkCmdSetAttachmentFeedbackLoopEnableEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdSetAttachmentFeedbackLoopEnableEXT *args)
{
   VKR_CMD_CALL(CmdSetAttachmentFeedbackLoopEnableEXT, args, args->aspectMask);
}

static void
vkr_dispatch_vkCmdDrawMeshTasksEXT(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCmdDrawMeshTasksEXT *args)
{
   VKR_CMD_CALL(CmdDrawMeshTasksEXT, args, args->groupCountX, args->groupCountY,
                args->groupCountZ);
}

static void
vkr_dispatch_vkCmdDrawMeshTasksIndirectEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdDrawMeshTasksIndirectEXT *args)
{
   VKR_CMD_CALL(CmdDrawMeshTasksIndirectEXT, args, args->buffer, args->offset,
                args->drawCount, args->stride);
}

static void
vkr_dispatch_vkCmdDrawMeshTasksIndirectCountEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkCmdDrawMeshTasksIndirectCountEXT *args)
{
   VKR_CMD_CALL(CmdDrawMeshTasksIndirectCountEXT, args, args->buffer, args->offset,
                args->countBuffer, args->countBufferOffset, args->maxDrawCount,
                args->stride);
}

void
vkr_context_init_command_pool_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateCommandPool = vkr_dispatch_vkCreateCommandPool;
   dispatch->dispatch_vkDestroyCommandPool = vkr_dispatch_vkDestroyCommandPool;
   dispatch->dispatch_vkResetCommandPool = vkr_dispatch_vkResetCommandPool;
   dispatch->dispatch_vkTrimCommandPool = vkr_dispatch_vkTrimCommandPool;
}

void
vkr_context_init_command_buffer_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkAllocateCommandBuffers = vkr_dispatch_vkAllocateCommandBuffers;
   dispatch->dispatch_vkFreeCommandBuffers = vkr_dispatch_vkFreeCommandBuffers;
   dispatch->dispatch_vkResetCommandBuffer = vkr_dispatch_vkResetCommandBuffer;
   dispatch->dispatch_vkBeginCommandBuffer = vkr_dispatch_vkBeginCommandBuffer;
   dispatch->dispatch_vkEndCommandBuffer = vkr_dispatch_vkEndCommandBuffer;

   dispatch->dispatch_vkCmdBindPipeline = vkr_dispatch_vkCmdBindPipeline;
   dispatch->dispatch_vkCmdSetViewport = vkr_dispatch_vkCmdSetViewport;
   dispatch->dispatch_vkCmdSetScissor = vkr_dispatch_vkCmdSetScissor;
   dispatch->dispatch_vkCmdSetLineWidth = vkr_dispatch_vkCmdSetLineWidth;
   dispatch->dispatch_vkCmdSetDepthBias = vkr_dispatch_vkCmdSetDepthBias;
   dispatch->dispatch_vkCmdSetBlendConstants = vkr_dispatch_vkCmdSetBlendConstants;
   dispatch->dispatch_vkCmdSetDepthBounds = vkr_dispatch_vkCmdSetDepthBounds;
   dispatch->dispatch_vkCmdSetStencilCompareMask =
      vkr_dispatch_vkCmdSetStencilCompareMask;
   dispatch->dispatch_vkCmdSetStencilWriteMask = vkr_dispatch_vkCmdSetStencilWriteMask;
   dispatch->dispatch_vkCmdSetStencilReference = vkr_dispatch_vkCmdSetStencilReference;
   dispatch->dispatch_vkCmdBindDescriptorSets = vkr_dispatch_vkCmdBindDescriptorSets;
   dispatch->dispatch_vkCmdBindIndexBuffer = vkr_dispatch_vkCmdBindIndexBuffer;
   dispatch->dispatch_vkCmdBindVertexBuffers = vkr_dispatch_vkCmdBindVertexBuffers;
   dispatch->dispatch_vkCmdDraw = vkr_dispatch_vkCmdDraw;
   dispatch->dispatch_vkCmdDrawIndexed = vkr_dispatch_vkCmdDrawIndexed;
   dispatch->dispatch_vkCmdDrawIndirect = vkr_dispatch_vkCmdDrawIndirect;
   dispatch->dispatch_vkCmdDrawIndexedIndirect = vkr_dispatch_vkCmdDrawIndexedIndirect;
   dispatch->dispatch_vkCmdDispatch = vkr_dispatch_vkCmdDispatch;
   dispatch->dispatch_vkCmdDispatchIndirect = vkr_dispatch_vkCmdDispatchIndirect;
   dispatch->dispatch_vkCmdCopyBuffer = vkr_dispatch_vkCmdCopyBuffer;
   dispatch->dispatch_vkCmdCopyBuffer2 = vkr_dispatch_vkCmdCopyBuffer2;
   dispatch->dispatch_vkCmdCopyImage = vkr_dispatch_vkCmdCopyImage;
   dispatch->dispatch_vkCmdCopyImage2 = vkr_dispatch_vkCmdCopyImage2;
   dispatch->dispatch_vkCmdBlitImage = vkr_dispatch_vkCmdBlitImage;
   dispatch->dispatch_vkCmdBlitImage2 = vkr_dispatch_vkCmdBlitImage2;
   dispatch->dispatch_vkCmdCopyBufferToImage = vkr_dispatch_vkCmdCopyBufferToImage;
   dispatch->dispatch_vkCmdCopyBufferToImage2 = vkr_dispatch_vkCmdCopyBufferToImage2;
   dispatch->dispatch_vkCmdCopyImageToBuffer = vkr_dispatch_vkCmdCopyImageToBuffer;
   dispatch->dispatch_vkCmdCopyImageToBuffer2 = vkr_dispatch_vkCmdCopyImageToBuffer2;
   dispatch->dispatch_vkCmdUpdateBuffer = vkr_dispatch_vkCmdUpdateBuffer;
   dispatch->dispatch_vkCmdFillBuffer = vkr_dispatch_vkCmdFillBuffer;
   dispatch->dispatch_vkCmdClearColorImage = vkr_dispatch_vkCmdClearColorImage;
   dispatch->dispatch_vkCmdClearDepthStencilImage =
      vkr_dispatch_vkCmdClearDepthStencilImage;
   dispatch->dispatch_vkCmdClearAttachments = vkr_dispatch_vkCmdClearAttachments;
   dispatch->dispatch_vkCmdResolveImage = vkr_dispatch_vkCmdResolveImage;
   dispatch->dispatch_vkCmdResolveImage2 = vkr_dispatch_vkCmdResolveImage2;
   dispatch->dispatch_vkCmdSetEvent = vkr_dispatch_vkCmdSetEvent;
   dispatch->dispatch_vkCmdResetEvent = vkr_dispatch_vkCmdResetEvent;
   dispatch->dispatch_vkCmdWaitEvents = vkr_dispatch_vkCmdWaitEvents;
   dispatch->dispatch_vkCmdPipelineBarrier = vkr_dispatch_vkCmdPipelineBarrier;
   dispatch->dispatch_vkCmdBeginQuery = vkr_dispatch_vkCmdBeginQuery;
   dispatch->dispatch_vkCmdEndQuery = vkr_dispatch_vkCmdEndQuery;
   dispatch->dispatch_vkCmdResetQueryPool = vkr_dispatch_vkCmdResetQueryPool;
   dispatch->dispatch_vkCmdWriteTimestamp = vkr_dispatch_vkCmdWriteTimestamp;
   dispatch->dispatch_vkCmdCopyQueryPoolResults = vkr_dispatch_vkCmdCopyQueryPoolResults;
   dispatch->dispatch_vkCmdPushConstants = vkr_dispatch_vkCmdPushConstants;
   dispatch->dispatch_vkCmdBeginRenderPass = vkr_dispatch_vkCmdBeginRenderPass;
   dispatch->dispatch_vkCmdNextSubpass = vkr_dispatch_vkCmdNextSubpass;
   dispatch->dispatch_vkCmdEndRenderPass = vkr_dispatch_vkCmdEndRenderPass;
   dispatch->dispatch_vkCmdExecuteCommands = vkr_dispatch_vkCmdExecuteCommands;
   dispatch->dispatch_vkCmdSetDeviceMask = vkr_dispatch_vkCmdSetDeviceMask;
   dispatch->dispatch_vkCmdDispatchBase = vkr_dispatch_vkCmdDispatchBase;
   dispatch->dispatch_vkCmdBeginRenderPass2 = vkr_dispatch_vkCmdBeginRenderPass2;
   dispatch->dispatch_vkCmdNextSubpass2 = vkr_dispatch_vkCmdNextSubpass2;
   dispatch->dispatch_vkCmdEndRenderPass2 = vkr_dispatch_vkCmdEndRenderPass2;
   dispatch->dispatch_vkCmdDrawIndirectCount = vkr_dispatch_vkCmdDrawIndirectCount;
   dispatch->dispatch_vkCmdDrawIndexedIndirectCount =
      vkr_dispatch_vkCmdDrawIndexedIndirectCount;

   dispatch->dispatch_vkCmdSetLineStipple = vkr_dispatch_vkCmdSetLineStipple;

   dispatch->dispatch_vkCmdBindTransformFeedbackBuffersEXT =
      vkr_dispatch_vkCmdBindTransformFeedbackBuffersEXT;
   dispatch->dispatch_vkCmdBeginTransformFeedbackEXT =
      vkr_dispatch_vkCmdBeginTransformFeedbackEXT;
   dispatch->dispatch_vkCmdEndTransformFeedbackEXT =
      vkr_dispatch_vkCmdEndTransformFeedbackEXT;
   dispatch->dispatch_vkCmdBeginQueryIndexedEXT = vkr_dispatch_vkCmdBeginQueryIndexedEXT;
   dispatch->dispatch_vkCmdEndQueryIndexedEXT = vkr_dispatch_vkCmdEndQueryIndexedEXT;
   dispatch->dispatch_vkCmdDrawIndirectByteCountEXT =
      vkr_dispatch_vkCmdDrawIndirectByteCountEXT;

   dispatch->dispatch_vkCmdBindVertexBuffers2 = vkr_dispatch_vkCmdBindVertexBuffers2;
   dispatch->dispatch_vkCmdSetCullMode = vkr_dispatch_vkCmdSetCullMode;
   dispatch->dispatch_vkCmdSetDepthBoundsTestEnable =
      vkr_dispatch_vkCmdSetDepthBoundsTestEnable;
   dispatch->dispatch_vkCmdSetDepthCompareOp = vkr_dispatch_vkCmdSetDepthCompareOp;
   dispatch->dispatch_vkCmdSetDepthTestEnable = vkr_dispatch_vkCmdSetDepthTestEnable;
   dispatch->dispatch_vkCmdSetDepthWriteEnable = vkr_dispatch_vkCmdSetDepthWriteEnable;
   dispatch->dispatch_vkCmdSetFrontFace = vkr_dispatch_vkCmdSetFrontFace;
   dispatch->dispatch_vkCmdSetPrimitiveTopology = vkr_dispatch_vkCmdSetPrimitiveTopology;
   dispatch->dispatch_vkCmdSetScissorWithCount = vkr_dispatch_vkCmdSetScissorWithCount;
   dispatch->dispatch_vkCmdSetStencilOp = vkr_dispatch_vkCmdSetStencilOp;
   dispatch->dispatch_vkCmdSetStencilTestEnable = vkr_dispatch_vkCmdSetStencilTestEnable;
   dispatch->dispatch_vkCmdSetViewportWithCount = vkr_dispatch_vkCmdSetViewportWithCount;

   dispatch->dispatch_vkCmdBindIndexBuffer2 = vkr_dispatch_vkCmdBindIndexBuffer2;

   /* VK_KHR_dynamic_rendering */
   dispatch->dispatch_vkCmdBeginRendering = vkr_dispatch_vkCmdBeginRendering;
   dispatch->dispatch_vkCmdEndRendering = vkr_dispatch_vkCmdEndRendering;

   /* VK_KHR_synchronization2 */
   dispatch->dispatch_vkCmdPipelineBarrier2 = vkr_dispatch_vkCmdPipelineBarrier2;
   dispatch->dispatch_vkCmdResetEvent2 = vkr_dispatch_vkCmdResetEvent2;
   dispatch->dispatch_vkCmdSetEvent2 = vkr_dispatch_vkCmdSetEvent2;
   dispatch->dispatch_vkCmdWaitEvents2 = vkr_dispatch_vkCmdWaitEvents2;
   dispatch->dispatch_vkCmdWriteTimestamp2 = vkr_dispatch_vkCmdWriteTimestamp2;

   /* VK_EXT_extended_dynamic_state2 */
   dispatch->dispatch_vkCmdSetRasterizerDiscardEnable =
      vkr_dispatch_vkCmdSetRasterizerDiscardEnable;
   dispatch->dispatch_vkCmdSetPrimitiveRestartEnable =
      vkr_dispatch_vkCmdSetPrimitiveRestartEnable;
   dispatch->dispatch_vkCmdSetPatchControlPointsEXT =
      vkr_dispatch_vkCmdSetPatchControlPointsEXT;
   dispatch->dispatch_vkCmdSetLogicOpEXT = vkr_dispatch_vkCmdSetLogicOpEXT;
   dispatch->dispatch_vkCmdSetDepthBiasEnable = vkr_dispatch_vkCmdSetDepthBiasEnable;

   /* VK_EXT_conditional_rendering */
   dispatch->dispatch_vkCmdBeginConditionalRenderingEXT =
      vkr_dispatch_vkCmdBeginConditionalRenderingEXT;
   dispatch->dispatch_vkCmdEndConditionalRenderingEXT =
      vkr_dispatch_vkCmdEndConditionalRenderingEXT;

   /* VK_EXT_multi_draw */
   dispatch->dispatch_vkCmdDrawMultiEXT = vkr_dispatch_vkCmdDrawMultiEXT;
   dispatch->dispatch_vkCmdDrawMultiIndexedEXT = vkr_dispatch_vkCmdDrawMultiIndexedEXT;

   /* VK_KHR_push_descriptor */
   dispatch->dispatch_vkCmdPushDescriptorSet = vkr_dispatch_vkCmdPushDescriptorSet;
   dispatch->dispatch_vkCmdPushDescriptorSetWithTemplate = NULL;

   /* VK_EXT_color_write_enable */
   dispatch->dispatch_vkCmdSetColorWriteEnableEXT =
      vkr_dispatch_vkCmdSetColorWriteEnableEXT;

   /* VK_EXT_vertex_input_dynamic_state */
   dispatch->dispatch_vkCmdSetVertexInputEXT = vkr_dispatch_vkCmdSetVertexInputEXT;

   /* VK_EXT_extended_dynamic_state3 */
   dispatch->dispatch_vkCmdSetAlphaToCoverageEnableEXT =
      vkr_dispatch_vkCmdSetAlphaToCoverageEnableEXT;
   dispatch->dispatch_vkCmdSetAlphaToOneEnableEXT =
      vkr_dispatch_vkCmdSetAlphaToOneEnableEXT;
   dispatch->dispatch_vkCmdSetColorBlendAdvancedEXT =
      vkr_dispatch_vkCmdSetColorBlendAdvancedEXT;
   dispatch->dispatch_vkCmdSetColorBlendEnableEXT =
      vkr_dispatch_vkCmdSetColorBlendEnableEXT;
   dispatch->dispatch_vkCmdSetColorBlendEquationEXT =
      vkr_dispatch_vkCmdSetColorBlendEquationEXT;
   dispatch->dispatch_vkCmdSetColorWriteMaskEXT = vkr_dispatch_vkCmdSetColorWriteMaskEXT;
   dispatch->dispatch_vkCmdSetConservativeRasterizationModeEXT =
      vkr_dispatch_vkCmdSetConservativeRasterizationModeEXT;
   dispatch->dispatch_vkCmdSetDepthClampEnableEXT =
      vkr_dispatch_vkCmdSetDepthClampEnableEXT;
   dispatch->dispatch_vkCmdSetDepthClipEnableEXT =
      vkr_dispatch_vkCmdSetDepthClipEnableEXT;
   dispatch->dispatch_vkCmdSetDepthClipNegativeOneToOneEXT =
      vkr_dispatch_vkCmdSetDepthClipNegativeOneToOneEXT;
   dispatch->dispatch_vkCmdSetExtraPrimitiveOverestimationSizeEXT =
      vkr_dispatch_vkCmdSetExtraPrimitiveOverestimationSizeEXT;
   dispatch->dispatch_vkCmdSetLineRasterizationModeEXT =
      vkr_dispatch_vkCmdSetLineRasterizationModeEXT;
   dispatch->dispatch_vkCmdSetLineStippleEnableEXT =
      vkr_dispatch_vkCmdSetLineStippleEnableEXT;
   dispatch->dispatch_vkCmdSetLogicOpEnableEXT = vkr_dispatch_vkCmdSetLogicOpEnableEXT;
   dispatch->dispatch_vkCmdSetPolygonModeEXT = vkr_dispatch_vkCmdSetPolygonModeEXT;
   dispatch->dispatch_vkCmdSetProvokingVertexModeEXT =
      vkr_dispatch_vkCmdSetProvokingVertexModeEXT;
   dispatch->dispatch_vkCmdSetRasterizationSamplesEXT =
      vkr_dispatch_vkCmdSetRasterizationSamplesEXT;
   dispatch->dispatch_vkCmdSetRasterizationStreamEXT =
      vkr_dispatch_vkCmdSetRasterizationStreamEXT;
   dispatch->dispatch_vkCmdSetSampleLocationsEnableEXT =
      vkr_dispatch_vkCmdSetSampleLocationsEnableEXT;
   dispatch->dispatch_vkCmdSetSampleMaskEXT = vkr_dispatch_vkCmdSetSampleMaskEXT;
   dispatch->dispatch_vkCmdSetTessellationDomainOriginEXT =
      vkr_dispatch_vkCmdSetTessellationDomainOriginEXT;

   /* VK_KHR_fragment_shading_rate */
   dispatch->dispatch_vkCmdSetFragmentShadingRateKHR =
      vkr_dispatch_vkCmdSetFragmentShadingRateKHR;

   /* VK_EXT_sample_locations */
   dispatch->dispatch_vkCmdSetSampleLocationsEXT =
      vkr_dispatch_vkCmdSetSampleLocationsEXT;

   /* VK_KHR_dynamic_rendering_local_read */
   dispatch->dispatch_vkCmdSetRenderingAttachmentLocations =
      vkr_dispatch_vkCmdSetRenderingAttachmentLocations;
   dispatch->dispatch_vkCmdSetRenderingInputAttachmentIndices =
      vkr_dispatch_vkCmdSetRenderingInputAttachmentIndices;

   /* VK_KHR_maintenance6 */
   dispatch->dispatch_vkCmdBindDescriptorSets2 = vkr_dispatch_vkCmdBindDescriptorSets2;
   dispatch->dispatch_vkCmdPushConstants2 = vkr_dispatch_vkCmdPushConstants2;
   dispatch->dispatch_vkCmdPushDescriptorSet2 = vkr_dispatch_vkCmdPushDescriptorSet2;
   dispatch->dispatch_vkCmdPushDescriptorSetWithTemplate2 = NULL;

   /* VK_KHR_acceleration_structure */
   dispatch->dispatch_vkCmdBuildAccelerationStructuresIndirectKHR =
      vkr_dispatch_vkCmdBuildAccelerationStructuresIndirectKHR;
   dispatch->dispatch_vkCmdBuildAccelerationStructuresKHR =
      vkr_dispatch_vkCmdBuildAccelerationStructuresKHR;
   dispatch->dispatch_vkCmdCopyAccelerationStructureKHR =
      vkr_dispatch_vkCmdCopyAccelerationStructureKHR;
   dispatch->dispatch_vkCmdCopyAccelerationStructureToMemoryKHR =
      vkr_dispatch_vkCmdCopyAccelerationStructureToMemoryKHR;
   dispatch->dispatch_vkCmdCopyMemoryToAccelerationStructureKHR =
      vkr_dispatch_vkCmdCopyMemoryToAccelerationStructureKHR;
   dispatch->dispatch_vkCmdWriteAccelerationStructuresPropertiesKHR =
      vkr_dispatch_vkCmdWriteAccelerationStructuresPropertiesKHR;

   /* VK_KHR_ray_tracing_pipeline */
   dispatch->dispatch_vkCmdSetRayTracingPipelineStackSizeKHR =
      vkr_dispatch_vkCmdSetRayTracingPipelineStackSizeKHR;
   dispatch->dispatch_vkCmdTraceRaysIndirectKHR = vkr_dispatch_vkCmdTraceRaysIndirectKHR;
   dispatch->dispatch_vkCmdTraceRaysKHR = vkr_dispatch_vkCmdTraceRaysKHR;

   /* VK_KHR_ray_tracing_maintenance1 */
   dispatch->dispatch_vkCmdTraceRaysIndirect2KHR =
      vkr_dispatch_vkCmdTraceRaysIndirect2KHR;

   /* VK_EXT_depth_bias_control */
   dispatch->dispatch_vkCmdSetDepthBias2EXT = vkr_dispatch_vkCmdSetDepthBias2EXT;

   /* VK_EXT_depth_clamp_control */
   dispatch->dispatch_vkCmdSetDepthClampRangeEXT =
      vkr_dispatch_vkCmdSetDepthClampRangeEXT;

   /* VK_EXT_attachment_feedback_loop_dynamic_state */
   dispatch->dispatch_vkCmdSetAttachmentFeedbackLoopEnableEXT =
      vkr_dispatch_vkCmdSetAttachmentFeedbackLoopEnableEXT;

   /* VK_EXT_mesh_shader */
   dispatch->dispatch_vkCmdDrawMeshTasksEXT = vkr_dispatch_vkCmdDrawMeshTasksEXT;
   dispatch->dispatch_vkCmdDrawMeshTasksIndirectEXT =
      vkr_dispatch_vkCmdDrawMeshTasksIndirectEXT;
   dispatch->dispatch_vkCmdDrawMeshTasksIndirectCountEXT =
      vkr_dispatch_vkCmdDrawMeshTasksIndirectCountEXT;
}
