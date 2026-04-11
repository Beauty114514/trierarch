/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef I915_RENDERER_H_
#define I915_RENDERER_H_

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <xf86drm.h>

#include "virgl_context.h"
#include "virgl_util.h"

#include "drm_context.h"
#include "drm_fence.h"
#include "drm_util.h"
#include "drm_hw.h"
#include "i915_proto.h"

#pragma GCC diagnostic push
#if !defined(__clang__) && defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpedantic"
#else
#pragma GCC diagnostic ignored "-Wzero-length-array"
#endif

#include "i915_drm.h"

#pragma GCC diagnostic pop

struct i915_context {
   struct drm_context base;
   struct drm_timeline *timelines[65];
};
DEFINE_CAST(drm_context, i915_context)

void *i915_context_rsp_noshadow(struct i915_context *ictx,
                                const struct vdrm_ccmd_req *hdr);

void *i915_context_rsp(struct i915_context *ictx, const struct vdrm_ccmd_req *hdr,
                       size_t len);

int i915_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset);

struct virgl_context *i915_renderer_create(int fd, size_t debug_len, const char *debug_name);

#endif /* I915_RENDERER_H_ */
