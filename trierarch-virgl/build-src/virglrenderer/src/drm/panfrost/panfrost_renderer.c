/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#include <xf86drm.h>
#include <errno.h>

#include "drm_context.h"
#include "drm_fence.h"
#include "drm_hw.h"
#include "drm_util.h"

#include "panfrost_renderer.h"
#include "panfrost_resource.h"
#include "panfrost_object.h"
#include "panfrost_ccmd.h"
#include "panfrost_drm.h"

/**
 * Probe capset params.
 */
int
panfrost_renderer_probe(UNUSED int fd, struct virgl_renderer_capset_drm *capset)
{
   capset->wire_format_version = 0;

   drm_dbg("Panfrost vdrm loaded");

   return 0;
}

static void
panfrost_renderer_destroy(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   drmSyncobjDestroy(dctx->fd, pan_ctx->out_sync);
   drm_timeline_fini(&pan_ctx->timeline);
   drm_context_deinit(dctx);
   free(pan_ctx);
}

static int
panfrost_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags,
                               uint32_t queue_id, uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   if (queue_id != 0) {
      drm_err("invalid queue_id: %u", queue_id);
      return -EINVAL;
   }

   if (pan_ctx->timeline.last_fence_fd < 0) {
      vctx->fence_retire(vctx, queue_id, fence_id);
      return 0;
   }

   return drm_timeline_submit_fence(&pan_ctx->timeline, flags, fence_id);
}

struct virgl_context *
panfrost_renderer_create(int fd, UNUSED size_t debug_len, UNUSED const char *debug_name)
{
   struct panfrost_context *pan_ctx;

   pan_ctx = calloc(1, sizeof(*pan_ctx));
   if (!pan_ctx)
      return NULL;

   if (!drm_context_init(&pan_ctx->base, fd, panfrost_ccmd_dispatch, panfrost_ccmd_dispatch_size))
      goto ctx_init_failure;

   if (drmSyncobjCreate(fd, 0, &pan_ctx->out_sync))
      goto syncobj_failure;

   pan_ctx->base.base.destroy = panfrost_renderer_destroy;
   pan_ctx->base.base.attach_resource = panfrost_renderer_attach_resource;
   pan_ctx->base.base.export_opaque_handle = panfrost_renderer_export_opaque_handle;
   pan_ctx->base.base.get_blob = panfrost_renderer_get_blob;
   pan_ctx->base.base.submit_fence = panfrost_renderer_submit_fence;
   pan_ctx->base.base.supports_fence_sharing = true;
   pan_ctx->base.free_object = panfrost_renderer_free_object;
   pan_ctx->base.ccmd_alignment = 4;

   drm_timeline_init(&pan_ctx->timeline, &pan_ctx->base.base, "panfrost-sync", 0,
                     drm_context_fence_retire);

   return &pan_ctx->base.base;

syncobj_failure:
   drm_context_deinit(&pan_ctx->base);
ctx_init_failure:
   free(pan_ctx);
   return NULL;
}
