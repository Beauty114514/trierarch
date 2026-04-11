/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_common.h"

#include "venus-protocol/vn_protocol_renderer_info.h"
#include "virtgpu_drm.h"
#include "venus_hw.h"

#include "vkr_context.h"

struct vkr_renderer_state {
   const struct vkr_renderer_callbacks *cbs;

   /* track the vkr_context */
   struct list_head contexts;
};

struct vkr_renderer_state vkr_state;

size_t
vkr_get_capset(void *capset, uint32_t flags)
{
   struct virgl_renderer_capset_venus *c = capset;
   if (c) {
      memset(c, 0, sizeof(*c));
      c->wire_format_version = vn_info_wire_format_version();
      c->vk_xml_version = vn_info_vk_xml_version();
      c->vk_ext_command_serialization_spec_version =
         vkr_extension_get_spec_version("VK_EXT_command_serialization");
      c->vk_mesa_venus_protocol_spec_version =
         vkr_extension_get_spec_version("VK_MESA_venus_protocol");
      /* After https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/688,
       * this flag is used to indicate render server config.
       */
      c->supports_blob_id_0 = true;

      uint32_t ext_mask[VN_INFO_EXTENSION_MAX_NUMBER / 32 + 1] = { 0 };
      vn_info_extension_mask_init(ext_mask);

      static_assert(sizeof(ext_mask) <= sizeof(c->vk_extension_mask1),
                    "Time to extend venus capset with vk_extension_mask2");
      memcpy(c->vk_extension_mask1, ext_mask, sizeof(ext_mask));

      /* set bit 0 to enable the extension mask(s) */
      assert(!(c->vk_extension_mask1[0] & 0x1u));
      c->vk_extension_mask1[0] |= 0x1u;

      c->allow_vk_wait_syncs = 1;
      c->supports_multiple_timelines = 1;

      c->use_guest_vram = (bool)(flags & VIRGL_RENDERER_USE_GUEST_VRAM);
   }

   return sizeof(*c);
}

bool
vkr_renderer_init(uint32_t flags, const struct vkr_renderer_callbacks *cbs)
{
   TRACE_INIT();
   TRACE_FUNC();

   static const uint32_t required_flags =
      VKR_RENDERER_THREAD_SYNC | VKR_RENDERER_ASYNC_FENCE_CB;
   if ((flags & required_flags) != required_flags)
      return false;

   vkr_debug_init();
   virgl_log_set_handler(cbs->debug_logger, NULL, NULL);

   vkr_state.cbs = cbs;
   list_inithead(&vkr_state.contexts);

   return true;
}

void
vkr_renderer_fini(void)
{
   list_for_each_entry_safe (struct vkr_context, ctx, &vkr_state.contexts, head)
      vkr_context_destroy(ctx);

   list_inithead(&vkr_state.contexts);

   vkr_state.cbs = NULL;
}

static struct vkr_context *
vkr_renderer_lookup_context(uint32_t ctx_id)
{
   list_for_each_entry (struct vkr_context, ctx, &vkr_state.contexts, head) {
      if (ctx->ctx_id == ctx_id)
         return ctx;
   }

   return NULL;
}

bool
vkr_renderer_create_context(uint32_t ctx_id,
                            uint32_t ctx_flags,
                            uint32_t nlen,
                            const char *name)
{
   TRACE_FUNC();

   assert(ctx_id);
   assert(!(ctx_flags & ~VIRGL_RENDERER_CONTEXT_FLAG_CAPSET_ID_MASK));

   if ((ctx_flags & VIRGL_RENDERER_CONTEXT_FLAG_CAPSET_ID_MASK) !=
       VIRTGPU_DRM_CAPSET_VENUS)
      return false;

   /* duplicate ctx creation between server and vkr is invalid */
   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (ctx)
      return false;

   ctx = vkr_context_create(ctx_id, vkr_state.cbs->retire_fence, nlen, name);
   if (!ctx)
      return false;

   list_addtail(&ctx->head, &vkr_state.contexts);

   return true;
}

void
vkr_renderer_destroy_context(uint32_t ctx_id)
{
   TRACE_FUNC();

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (!ctx)
      return;

   list_del(&ctx->head);
   vkr_context_destroy(ctx);
}

bool
vkr_renderer_submit_cmd(uint32_t ctx_id, void *cmd, uint32_t size)
{
   TRACE_FUNC();

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (!ctx)
      return false;

   return vkr_context_submit_cmd(ctx, cmd, size);
}

bool
vkr_renderer_submit_fence(uint32_t ctx_id,
                          uint32_t flags,
                          uint64_t ring_idx,
                          uint64_t fence_id)
{
   TRACE_FUNC();

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (!ctx)
      return false;

   assert(vkr_state.cbs->retire_fence);
   return vkr_context_submit_fence(ctx, flags, ring_idx, fence_id);
}

bool
vkr_renderer_create_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             uint64_t blob_id,
                             uint64_t blob_size,
                             uint32_t blob_flags,
                             enum virgl_resource_fd_type *out_fd_type,
                             int *out_res_fd,
                             uint32_t *out_map_info,
                             struct virgl_resource_vulkan_info *out_vulkan_info)
{
   TRACE_FUNC();

   assert(res_id);
   assert(blob_size);

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (!ctx)
      return false;

   struct virgl_context_blob blob;
   if (!vkr_context_create_resource(ctx, res_id, blob_id, blob_size, blob_flags, &blob))
      return false;

   assert(blob.type == VIRGL_RESOURCE_FD_SHM || blob.type == VIRGL_RESOURCE_FD_DMABUF ||
          blob.type == VIRGL_RESOURCE_FD_OPAQUE);

   *out_fd_type = blob.type;
   *out_res_fd = blob.u.fd;
   *out_map_info = blob.map_info;

   if (blob.type == VIRGL_RESOURCE_FD_OPAQUE) {
      assert(out_vulkan_info);
      *out_vulkan_info = blob.vulkan_info;
   }

   return true;
}

bool
vkr_renderer_import_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             enum virgl_resource_fd_type fd_type,
                             int fd,
                             uint64_t size)
{
   TRACE_FUNC();

   assert(res_id);
   assert(fd_type == VIRGL_RESOURCE_FD_DMABUF || fd_type == VIRGL_RESOURCE_FD_OPAQUE);
   assert(fd >= 0);
   assert(size);

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (!ctx)
      return false;

   return vkr_context_import_resource(ctx, res_id, fd_type, fd, size);
}

void
vkr_renderer_destroy_resource(uint32_t ctx_id, uint32_t res_id)
{
   TRACE_FUNC();

   struct vkr_context *ctx = vkr_renderer_lookup_context(ctx_id);
   if (ctx)
      vkr_context_destroy_resource(ctx, res_id);
}
