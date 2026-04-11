/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef I915_RENDERER_RESOURCE_H_
#define I915_RENDERER_RESOURCE_H_

#include "i915_renderer.h"

bool i915_res_id_unused(struct i915_context *ictx, uint32_t res_id);
void i915_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res);

enum virgl_resource_fd_type
i915_renderer_export_opaque_handle(struct virgl_context *vctx,
                                   struct virgl_resource *res,
                                   int *out_fd);

int i915_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                            uint64_t blob_size, uint32_t blob_flags,
                            struct virgl_context_blob *blob);

#endif /* I915_RENDERER_RESOURCE_H_ */
