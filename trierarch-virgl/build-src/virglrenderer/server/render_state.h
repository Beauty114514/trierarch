/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_STATE_H
#define RENDER_STATE_H

#include "render_common.h"

bool
render_state_init(uint32_t init_flags);

void
render_state_fini(void);

bool
render_state_create_context(struct render_context *ctx,
                            uint32_t flags,
                            uint32_t name_len,
                            const char *name);

void
render_state_destroy_context(uint32_t ctx_id);

bool
render_state_submit_cmd(uint32_t ctx_id, void *cmd, uint32_t size);

bool
render_state_submit_fence(uint32_t ctx_id,
                          uint32_t flags,
                          uint64_t ring_idx,
                          uint64_t fence_id);

bool
render_state_create_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             uint64_t blob_id,
                             uint64_t blob_size,
                             uint32_t blob_flags,
                             enum virgl_resource_fd_type *out_fd_type,
                             int *out_res_fd,
                             uint32_t *out_map_info,
                             struct virgl_resource_vulkan_info *out_vulkan_info);

bool
render_state_import_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             enum virgl_resource_fd_type fd_type,
                             int fd,
                             uint64_t size);

void
render_state_destroy_resource(uint32_t ctx_id, uint32_t res_id);

#endif /* RENDER_STATE_H */
