/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef I915_RENDERER_CCMD_H_
#define I915_RENDERER_CCMD_H_

#include "i915_renderer.h"

extern const struct drm_ccmd i915_ccmd_dispatch[];
extern const unsigned int i915_ccmd_dispatch_size;

int intel_ioctl(int fd, unsigned long cmd, void *req);

#endif /* I915_RENDERER_CCMD_H_ */
