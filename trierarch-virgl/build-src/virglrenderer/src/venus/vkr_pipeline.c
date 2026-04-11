/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_pipeline.h"

#include "vkr_pipeline_gen.h"

static void
vkr_dispatch_vkCreateShaderModule(struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkCreateShaderModule *args)
{
   struct vkr_context *ctx = dispatch->data;

   /* Reject invalid codeSize.
    *
    * VkShaderModuleCreateInfo is unique in the Vulkan API (as of 2023-08-22).
    * Except in rare cases, (see the `altlen` attribute in vk.xml), for each
    * typed non-void array in Vulkan, the api specifies the array length as the
    * count of array elements. But VkShaderModuleCreateInfo has a typed array
    * (uint32_t *pCode) whose length (codeSize) is specified in bytes, not as
    * a count of uint32_t.
    *
    * Also, the Vulkan 1.3.261 spec seems confused about the size of `pCode`.
    * The spec says "codeSize is the size, in bytes, of the code pointed to by
    * pCode", and then later says "pCode must be a valid pointer to an array of
    * codeSize/4 uint32_t values".
    *
    * (FWIW, VkShaderCreateInfoEXT learned from this mistake and declared the
    * array to be typeless, `void *pCode`).
    *
    * The venus encoder/decoder believes the array size is `4 * (codeSize / 4)`
    * because the vk.xml says so. For example, if codeSize is 259, then venus
    * encodes/decodes only 256 bytes. But the native driver may try to read all
    * 259 bytes, leading to out-of-bound access. Prevent the oob access by
    * validating codeSize here.
    */
   if (args->pCreateInfo->codeSize % 4 != 0) {
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_shader_module_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyShaderModule(struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkDestroyShaderModule *args)
{
   vkr_shader_module_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreatePipelineLayout(struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkCreatePipelineLayout *args)
{
   vkr_pipeline_layout_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyPipelineLayout(struct vn_dispatch_context *dispatch,
                                     struct vn_command_vkDestroyPipelineLayout *args)
{
   vkr_pipeline_layout_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreatePipelineCache(struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkCreatePipelineCache *args)
{
   vkr_pipeline_cache_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyPipelineCache(struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkDestroyPipelineCache *args)
{
   vkr_pipeline_cache_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetPipelineCacheData(UNUSED struct vn_dispatch_context *dispatch,
                                    struct vn_command_vkGetPipelineCacheData *args)
{
   TRACE_FUNC();
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetPipelineCacheData_args_handle(args);
   args->ret = vk->GetPipelineCacheData(args->device, args->pipelineCache,
                                        args->pDataSize, args->pData);
}

static void
vkr_dispatch_vkMergePipelineCaches(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkMergePipelineCaches *args)
{
   TRACE_FUNC();
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkMergePipelineCaches_args_handle(args);
   args->ret = vk->MergePipelineCaches(args->device, args->dstCache, args->srcCacheCount,
                                       args->pSrcCaches);
}

static void
vkr_dispatch_vkCreateGraphicsPipelines(struct vn_dispatch_context *dispatch,
                                       struct vn_command_vkCreateGraphicsPipelines *args)
{
   TRACE_FUNC();
   struct vkr_context *ctx = dispatch->data;
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct object_array arr;

   if (vkr_graphics_pipeline_create_array(ctx, args, &arr) < VK_SUCCESS)
      return;

   vkr_pipeline_add_array(ctx, dev, &arr, args->pPipelines);
}

static void
vkr_dispatch_vkCreateComputePipelines(struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkCreateComputePipelines *args)
{
   TRACE_FUNC();
   struct vkr_context *ctx = dispatch->data;
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct object_array arr;

   if (vkr_compute_pipeline_create_array(ctx, args, &arr) < VK_SUCCESS)
      return;

   vkr_pipeline_add_array(ctx, dev, &arr, args->pPipelines);
}

static void
vkr_dispatch_vkDestroyPipeline(struct vn_dispatch_context *dispatch,
                               struct vn_command_vkDestroyPipeline *args)
{
   vkr_pipeline_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkCreateRayTracingPipelinesKHR(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkCreateRayTracingPipelinesKHR *args)
{
   TRACE_FUNC();
   struct vkr_context *ctx = dispatch->data;
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct object_array arr;

   if (vkr_ray_tracing_pipeline_create_array(ctx, args, &arr) < VK_SUCCESS)
      return;

   vkr_pipeline_add_array(ctx, dev, &arr, args->pPipelines);
}

static void
vkr_dispatch_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR_args_handle(args);
   args->ret = vk->GetRayTracingCaptureReplayShaderGroupHandlesKHR(
      args->device, args->pipeline, args->firstGroup, args->groupCount, args->dataSize,
      args->pData);
}

static void
vkr_dispatch_vkGetRayTracingShaderGroupHandlesKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetRayTracingShaderGroupHandlesKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetRayTracingShaderGroupHandlesKHR_args_handle(args);
   args->ret = vk->GetRayTracingShaderGroupHandlesKHR(args->device, args->pipeline,
                                                      args->firstGroup, args->groupCount,
                                                      args->dataSize, args->pData);
}

static void
vkr_dispatch_vkGetRayTracingShaderGroupStackSizeKHR(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetRayTracingShaderGroupStackSizeKHR *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetRayTracingShaderGroupStackSizeKHR_args_handle(args);
   args->ret = vk->GetRayTracingShaderGroupStackSizeKHR(args->device, args->pipeline,
                                                        args->group, args->groupShader);
}

void
vkr_context_init_shader_module_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateShaderModule = vkr_dispatch_vkCreateShaderModule;
   dispatch->dispatch_vkDestroyShaderModule = vkr_dispatch_vkDestroyShaderModule;
}

void
vkr_context_init_pipeline_layout_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreatePipelineLayout = vkr_dispatch_vkCreatePipelineLayout;
   dispatch->dispatch_vkDestroyPipelineLayout = vkr_dispatch_vkDestroyPipelineLayout;
}

void
vkr_context_init_pipeline_cache_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreatePipelineCache = vkr_dispatch_vkCreatePipelineCache;
   dispatch->dispatch_vkDestroyPipelineCache = vkr_dispatch_vkDestroyPipelineCache;
   dispatch->dispatch_vkGetPipelineCacheData = vkr_dispatch_vkGetPipelineCacheData;
   dispatch->dispatch_vkMergePipelineCaches = vkr_dispatch_vkMergePipelineCaches;
}

void
vkr_context_init_pipeline_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateGraphicsPipelines = vkr_dispatch_vkCreateGraphicsPipelines;
   dispatch->dispatch_vkCreateComputePipelines = vkr_dispatch_vkCreateComputePipelines;
   dispatch->dispatch_vkDestroyPipeline = vkr_dispatch_vkDestroyPipeline;

   /* VK_KHR_ray_tracing_pipeline */
   dispatch->dispatch_vkCreateRayTracingPipelinesKHR =
      vkr_dispatch_vkCreateRayTracingPipelinesKHR;
   dispatch->dispatch_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR =
      vkr_dispatch_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
   dispatch->dispatch_vkGetRayTracingShaderGroupHandlesKHR =
      vkr_dispatch_vkGetRayTracingShaderGroupHandlesKHR;
   dispatch->dispatch_vkGetRayTracingShaderGroupStackSizeKHR =
      vkr_dispatch_vkGetRayTracingShaderGroupStackSizeKHR;
}
