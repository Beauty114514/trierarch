/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef I915_RENDERER_OBJECT_H_
#define I915_RENDERER_OBJECT_H_

#include "i915_renderer.h"

struct i915_object {
   struct drm_object base;
   bool mmap_configured;
   int map_info;
};
DEFINE_CAST(drm_object, i915_object)

struct i915_object *
i915_object_create(uint32_t handle, uint64_t size);

void i915_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj);

#endif /* I915_RENDERER_OBJECT_H_ */
