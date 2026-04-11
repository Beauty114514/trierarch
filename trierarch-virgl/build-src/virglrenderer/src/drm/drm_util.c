/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <string.h>

#include "drm_util.h"
#include "virgl_util.h"

#include "util/macros.h"

void
_drm_log(enum virgl_log_level_flags level, const char *fmt, ...)
{
   va_list va;

   va_start(va, fmt);
   virgl_prefixed_logv("drm", level, fmt, va);
   va_end(va);
}
