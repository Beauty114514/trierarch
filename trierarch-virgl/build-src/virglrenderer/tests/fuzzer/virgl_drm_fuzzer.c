/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "util/macros.h"
#include "virglrenderer.h"
#include "virtgpu_drm.h"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

struct fuzz_renderer {
   bool initialized;
};

static void
fuzz_atexit_callback(void)
{
   virgl_renderer_cleanup(NULL);
}

static void
fuzz_debug_callback(UNUSED const char *fmt, UNUSED va_list ap)
{
   /* no logging */
}

static void
fuzz_write_context_fence(UNUSED void *cookie,
                         UNUSED uint32_t ctx_id,
                         UNUSED uint32_t ring_idx,
                         UNUSED uint64_t fence_id)
{

}


static struct virgl_renderer_callbacks callbacks = {
      .version = 3,
      .write_context_fence = fuzz_write_context_fence,
};

static struct fuzz_renderer *
fuzz_renderer_get(void)
{
   static struct fuzz_renderer renderer;
   if (renderer.initialized)
      return &renderer;

   int flags = VIRGL_RENDERER_NO_VIRGL | VIRGL_RENDERER_DRM |
         VIRGL_RENDERER_ASYNC_FENCE_CB;
   int ret =
      virgl_renderer_init(NULL, flags, &callbacks);
   if (ret)
      abort();

   virgl_set_debug_callback(fuzz_debug_callback);

   atexit(fuzz_atexit_callback);

   renderer.initialized = true;
   return &renderer;
}

static uint32_t
fuzz_context_create(UNUSED struct fuzz_renderer *renderer)
{
   const uint32_t ctx_id = 1;
   const char name[] = "virgl_drm_fuzzer";
   int ret = virgl_renderer_context_create_with_flags(ctx_id, VIRTGPU_DRM_CAPSET_DRM,
                                                      sizeof(name), name);
   if (ret)
      abort();

   return ctx_id;
}

static void
fuzz_context_destroy(UNUSED struct fuzz_renderer *renderer, uint32_t ctx_id)
{
   virgl_renderer_context_destroy(ctx_id);
}

static void
fuzz_context_submit(UNUSED struct fuzz_renderer *renderer,
                    uint32_t ctx_id,
                    const uint8_t *data,
                    size_t size)
{
   /* We'll not be able to hit some codepaths without shmem buffer setup..
    * but we'd also like to hit any potential errors that could come from
    * malicious input before shmem is setup.  So run the same input twice,
    * once before and once after shmem setup.
    */
   virgl_renderer_submit_cmd((void *)data, ctx_id, size / 4);
   virgl_renderer_resource_create_blob(&(struct virgl_renderer_resource_create_blob_args){
      .res_handle = 1,
      .ctx_id = ctx_id,
      .size = 0x1000,
   });
   virgl_renderer_submit_cmd((void *)data, ctx_id, size / 4);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   struct fuzz_renderer *renderer = fuzz_renderer_get();

   const uint32_t ctx_id = fuzz_context_create(renderer);
   fuzz_context_submit(renderer, ctx_id, data, size);
   fuzz_context_destroy(renderer, ctx_id);

   return 0;
}
