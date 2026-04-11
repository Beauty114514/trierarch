/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "i915_ccmd.h"
#include "i915_object.h"
#include "i915_resource.h"

struct i915_object *
i915_object_create(uint32_t handle, uint64_t size)
{
   struct i915_object *obj = calloc(1, sizeof(*obj));

   if (!obj)
      return NULL;

   obj->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;
   obj->base.handle = handle;
   obj->base.size = size;

   return obj;
}

void
i915_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   struct drm_gem_close close = {
      .handle = dobj->handle,
   };

   intel_ioctl(dctx->fd, DRM_IOCTL_GEM_CLOSE, &close);

   free(dobj);
}
