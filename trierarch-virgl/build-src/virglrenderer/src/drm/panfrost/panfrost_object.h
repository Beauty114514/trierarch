/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#ifndef PANFROST_RENDERER_OBJECT_H_
#define PANFROST_RENDERER_OBJECT_H_

#include "panfrost_renderer.h"

struct panfrost_object {
   struct drm_object base;
   uint32_t flags;
   uint64_t offset;
};
DEFINE_CAST(drm_object, panfrost_object)

struct panfrost_object *
panfrost_object_create(uint32_t handle, uint32_t size, uint64_t offset, uint32_t flags);

void panfrost_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj);

struct panfrost_object *
panfrost_object_from_blob_id(struct panfrost_context *pan_ctx, uint64_t blob_id);

struct panfrost_object *
panfrost_get_object_from_res_id(struct panfrost_context *pan_ctx, uint32_t res_id);

uint32_t
handle_from_res_id(struct panfrost_context *pan_ctx, uint32_t res_id);

#endif /* PANFROST_RENDERER_OBJECT_H_ */
