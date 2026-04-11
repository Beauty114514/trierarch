/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#include <xf86drm.h>

#include "panfrost_ccmd.h"
#include "panfrost_object.h"

struct panfrost_object *
panfrost_object_create(uint32_t handle, uint32_t size, uint64_t offset, uint32_t flags)
{
   struct panfrost_object *obj = calloc(1, sizeof(*obj));
   if (!obj)
      return NULL;

   obj->base.handle = handle;
   obj->base.size = size;
   obj->offset = offset;
   obj->flags = flags;

   return obj;
}

void
panfrost_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   drmCloseBufferHandle(dctx->fd, dobj->handle);
   free(dobj);
}

struct panfrost_object *
panfrost_object_from_blob_id(struct panfrost_context *pan_ctx, uint64_t blob_id)
{
   struct drm_object *dobj = drm_context_retrieve_object_from_blob_id(&pan_ctx->base, blob_id);
   if (!dobj)
      return NULL;

   return to_panfrost_object(dobj);
}

struct panfrost_object *
panfrost_get_object_from_res_id(struct panfrost_context *pan_ctx, uint32_t res_id)
{
   struct drm_object *dobj = drm_context_get_object_from_res_id(&pan_ctx->base, res_id);
   if (!dobj)
      return NULL;

   return to_panfrost_object(dobj);
}


uint32_t
handle_from_res_id(struct panfrost_context *pan_ctx, uint32_t res_id)
{
   struct panfrost_object *obj = panfrost_get_object_from_res_id(pan_ctx, res_id);
   if (!obj)
      return 0;    /* zero is an invalid GEM handle */
   return obj->base.handle;
}

