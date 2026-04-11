/*
 * Copyright 2024 Sergio Lopez
 * SPDX-License-Identifier: MIT
 */

#ifndef ASAHI_RENDERER_H_
#define ASAHI_RENDERER_H_

#include "drm_util.h"

int asahi_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset);

struct virgl_context *asahi_renderer_create(int fd, size_t debug_len,
                                            const char *debug_name);

#endif // ASAHI_RENDERER_H_
