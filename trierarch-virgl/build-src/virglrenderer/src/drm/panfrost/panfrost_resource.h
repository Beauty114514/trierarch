/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#ifndef PANFROST_RENDERER_RESOURCE_H_
#define PANFROST_RENDERER_RESOURCE_H_

#include "panfrost_renderer.h"

bool
panfrost_res_id_unused(struct panfrost_context *pan_ctx, uint32_t res_id);

void
panfrost_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res);

enum virgl_resource_fd_type
panfrost_renderer_export_opaque_handle(struct virgl_context *vctx,
                                       struct virgl_resource *res,
                                       int *out_fd);

int
panfrost_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                           uint64_t blob_size, uint32_t blob_flags,
                           struct virgl_context_blob *blob);

#endif /* PANFROST_RENDERER_RESOURCE_H_ */
