/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef MSM_RENDERER_H_
#define MSM_RENDERER_H_

#include "config.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "pipe/p_defines.h"

#include "drm_util.h"
#include "msm_drm.h"

int msm_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset);

struct virgl_context *msm_renderer_create(int fd, size_t debug_len, const char *debug_name);

#endif /* MSM_RENDERER_H_ */
