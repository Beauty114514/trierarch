/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#ifndef PANFROST_RENDERER_H_
#define PANFROST_RENDERER_H_

#include "config.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "pipe/p_defines.h"

#include "drm_context.h"
#include "drm_fence.h"
#include "drm_hw.h"

struct panfrost_context {
   struct drm_context base;
   struct drm_timeline timeline;
   uint32_t out_sync;
};
DEFINE_CAST(drm_context, panfrost_context)

int
panfrost_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset);

struct virgl_context *
panfrost_renderer_create(int fd, size_t debug_len, const char *debug_name);

#endif /* PANFROST_RENDERER_H_ */
