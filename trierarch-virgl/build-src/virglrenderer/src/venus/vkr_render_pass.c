/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_render_pass.h"

#include "vkr_render_pass_gen.h"

static void
vkr_dispatch_vkCreateRenderPass(struct vn_dispatch_context *dispatch,
                                struct vn_command_vkCreateRenderPass *args)
{
   vkr_render_pass_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkCreateRenderPass2(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCreateRenderPass2 *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   struct vkr_render_pass *pass = vkr_context_alloc_object(
      ctx, sizeof(*pass), VK_OBJECT_TYPE_RENDER_PASS, args->pRenderPass);
   if (!pass) {
      args->ret = VK_ERROR_OUT_OF_HOST_MEMORY;
      return;
   }

   vn_replace_vkCreateRenderPass2_args_handle(args);
   args->ret = vk->CreateRenderPass2(args->device, args->pCreateInfo, NULL,
                                     &pass->base.handle.render_pass);
   if (args->ret != VK_SUCCESS) {
      free(pass);
      return;
   }

   vkr_device_add_object(ctx, dev, &pass->base);
}

static void
vkr_dispatch_vkDestroyRenderPass(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkDestroyRenderPass *args)
{
   vkr_render_pass_destroy_and_remove(dispatch->data, args);
}

static void
vkr_dispatch_vkGetRenderAreaGranularity(UNUSED struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkGetRenderAreaGranularity *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetRenderAreaGranularity_args_handle(args);
   vk->GetRenderAreaGranularity(args->device, args->renderPass, args->pGranularity);
}

static void
vkr_dispatch_vkGetRenderingAreaGranularity(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetRenderingAreaGranularity *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetRenderingAreaGranularity_args_handle(args);
   vk->GetRenderingAreaGranularity(args->device, args->pRenderingAreaInfo,
                                   args->pGranularity);
}

static void
vkr_dispatch_vkCreateFramebuffer(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkCreateFramebuffer *args)
{
   vkr_framebuffer_create_and_add(dispatch->data, args);
}

static void
vkr_dispatch_vkDestroyFramebuffer(struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkDestroyFramebuffer *args)
{
   vkr_framebuffer_destroy_and_remove(dispatch->data, args);
}

void
vkr_context_init_render_pass_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateRenderPass = vkr_dispatch_vkCreateRenderPass;
   dispatch->dispatch_vkCreateRenderPass2 = vkr_dispatch_vkCreateRenderPass2;
   dispatch->dispatch_vkDestroyRenderPass = vkr_dispatch_vkDestroyRenderPass;
   dispatch->dispatch_vkGetRenderAreaGranularity =
      vkr_dispatch_vkGetRenderAreaGranularity;
   dispatch->dispatch_vkGetRenderingAreaGranularity =
      vkr_dispatch_vkGetRenderingAreaGranularity;
}

void
vkr_context_init_framebuffer_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkCreateFramebuffer = vkr_dispatch_vkCreateFramebuffer;
   dispatch->dispatch_vkDestroyFramebuffer = vkr_dispatch_vkDestroyFramebuffer;
}
