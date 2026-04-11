/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_COMMON_H
#define RENDER_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/compiler.h"
#include "util/list.h"
#include "util/macros.h"
#include "util/u_pointer.h"

#include "render_protocol.h"
#include "render_socket.h"

struct render_client;
struct render_context;
struct render_context_args;
struct render_server;
struct render_worker;
struct render_worker_jail;

void
render_log_init(void);

void
render_log(const char *fmt, ...);

#endif /* RENDER_COMMON_H */
