/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_query_pool.h"

#include "vkr_query_pool_gen.h"

static void
vkr_dispatch_vkCreateQueryPool(struct vn_dispatch_context *dispatch,
                               struct vn_command_vkCreateQueryPool *args)
{
   vkr_query_pool_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyQueryPool(struct vn_dispatch_context *dispatch,
                                struct vn_command_vkDestroyQueryPool *args)
{
   vkr_query_pool_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetQueryPoolResults(UNUSED struct vn_dispatch_context *dispatch,
                                   struct vn_command_vkGetQueryPoolResults *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetQueryPoolResults_args_handle(args);
   args->ret = vk->GetQueryPoolResults(args->device, args->queryPool, args->firstQuery,
                                       args->queryCount, args->dataSize, args->pData,
                                       args->stride, args->flags);
}

static void
vkr_dispatch_vkResetQueryPool(UNUSED struct vn_dispatch_context *dispatch,
                              struct vn_command_vkResetQueryPool *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkResetQueryPool_args_handle(args);
   vk->ResetQueryPool(args->device, args->queryPool, args->firstQuery, args->queryCount);
}

void
vkr_context_init_query_pool_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateQueryPool = vkr_dispatch_vkCreateQueryPool;
   dispatch->dispatch_vkDestroyQueryPool = vkr_dispatch_vkDestroyQueryPool;
   dispatch->dispatch_vkGetQueryPoolResults = vkr_dispatch_vkGetQueryPoolResults;
   dispatch->dispatch_vkResetQueryPool = vkr_dispatch_vkResetQueryPool;
}
