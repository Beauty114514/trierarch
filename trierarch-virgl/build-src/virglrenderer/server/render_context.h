/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#include "render_common.h"

#include <stdatomic.h>

struct render_context {
   uint32_t ctx_id;
   struct render_socket socket;
   struct list_head head;

   char *name;
   size_t name_len;

   int shmem_fd;
   size_t shmem_size;
   void *shmem_ptr;
   atomic_uint *shmem_timelines;

   int timeline_count;

   /* optional */
   int fence_eventfd;
};

struct render_context_args {
   bool valid;

   uint32_t init_flags;

   uint32_t ctx_id;
   char ctx_name[32];

   /* render_context_main always takes ownership even on errors */
   int ctx_fd;
};

bool
render_context_main(const struct render_context_args *args);

void
render_context_update_timeline(struct render_context *ctx,
                               uint32_t ring_idx,
                               uint32_t val);

#endif /* RENDER_CONTEXT_H */
