/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "render_state.h"

#include <inttypes.h>

#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
#include "c11/threads.h"
#endif

#include "render_context.h"
#include "vkr_renderer.h"

/* Workers call into vkr renderer.  When they are processes, not much care is
 * required. But when workers are threads, we need to grab a lock to protect
 * vkr renderer.
 */
struct render_state {
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   /* protect renderer interface */
   mtx_t renderer_mutex;
   /* protect the below global states */
   mtx_t state_mutex;
#endif

   /* track and init/fini just once */
   int init_count;

   /* track the render_context */
   struct list_head contexts;
};

struct render_state state = {
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   .renderer_mutex = _MTX_INITIALIZER_NP,
   .state_mutex = _MTX_INITIALIZER_NP,
#endif
   .init_count = 0,
};

#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
static inline mtx_t *
render_state_lock(mtx_t *mtx)
{
   mtx_lock(mtx);
   return mtx;
}

static void
render_state_unlock(mtx_t **mtx)
{
   mtx_unlock(*mtx);
}

#define SCOPE_LOCK_STATE()                                                               \
   mtx_t *_state_mtx __attribute__((cleanup(render_state_unlock), unused)) =             \
      render_state_lock(&state.state_mutex)

#define SCOPE_LOCK_RENDERER()                                                            \
   mtx_t *_renderer_mtx __attribute__((cleanup(render_state_unlock), unused)) =          \
      render_state_lock(&state.renderer_mutex)

#else

#define SCOPE_LOCK_STATE()
#define SCOPE_LOCK_RENDERER()

#endif /* ENABLE_RENDER_SERVER_WORKER_THREAD */

static struct render_context *
render_state_lookup_context(uint32_t ctx_id)
{
   struct render_context *ctx = NULL;

   SCOPE_LOCK_STATE();
#ifdef ENABLE_RENDER_SERVER_WORKER_THREAD
   list_for_each_entry (struct render_context, iter, &state.contexts, head) {
      if (iter->ctx_id == ctx_id) {
         ctx = iter;
         break;
      }
   }
#else
   assert(list_is_singular(&state.contexts));
   ctx = list_first_entry(&state.contexts, struct render_context, head);
   assert(ctx->ctx_id == ctx_id);
   (void)ctx_id;
#endif

   return ctx;
}

static void
render_state_cb_debug_logger(UNUSED enum virgl_log_level_flags log_level,
                             const char *message,
                             UNUSED void* user_data)
{
   render_log(message);
}

static void
render_state_cb_retire_fence(uint32_t ctx_id, uint32_t ring_idx, uint64_t fence_id)
{
   struct render_context *ctx = render_state_lookup_context(ctx_id);
   assert(ctx);

   const uint32_t seqno = (uint32_t)fence_id;
   render_context_update_timeline(ctx, ring_idx, seqno);
}

static const struct vkr_renderer_callbacks render_state_cbs = {
   .debug_logger = render_state_cb_debug_logger,
   .retire_fence = render_state_cb_retire_fence,
};

static void
render_state_add_context(struct render_context *ctx)
{
   SCOPE_LOCK_STATE();
   list_addtail(&ctx->head, &state.contexts);
}

static void
render_state_remove_context(struct render_context *ctx)
{
   SCOPE_LOCK_STATE();
   list_del(&ctx->head);
}

void
render_state_fini(void)
{
   SCOPE_LOCK_STATE();
   if (state.init_count) {
      state.init_count--;
      if (!state.init_count)
         vkr_renderer_fini();
   }
}

bool
render_state_init(uint32_t init_flags)
{
   static const uint32_t required_flags = VIRGL_RENDERER_VENUS | VIRGL_RENDERER_NO_VIRGL;
   if ((init_flags & required_flags) != required_flags)
      return false;

   SCOPE_LOCK_STATE();
   if (!state.init_count) {
      /* always use sync thread and async fence cb for low latency */
      static const uint32_t vkr_flags =
         VKR_RENDERER_THREAD_SYNC | VKR_RENDERER_ASYNC_FENCE_CB;
      if (!vkr_renderer_init(vkr_flags, &render_state_cbs))
         return false;

      list_inithead(&state.contexts);
   }

   state.init_count++;

   return true;
}

bool
render_state_create_context(struct render_context *ctx,
                            uint32_t flags,
                            uint32_t name_len,
                            const char *name)
{
   {
      SCOPE_LOCK_RENDERER();
      if (!vkr_renderer_create_context(ctx->ctx_id, flags, name_len, name))
         return false;
   }

   render_state_add_context(ctx);

   return true;
}

void
render_state_destroy_context(uint32_t ctx_id)
{
   struct render_context *ctx = render_state_lookup_context(ctx_id);
   if (!ctx)
      return;

   {
      SCOPE_LOCK_RENDERER();
      vkr_renderer_destroy_context(ctx_id);
   }

   render_state_remove_context(ctx);
}

bool
render_state_submit_cmd(uint32_t ctx_id, void *cmd, uint32_t size)
{
   SCOPE_LOCK_RENDERER();
   return vkr_renderer_submit_cmd(ctx_id, cmd, size);
}

bool
render_state_submit_fence(uint32_t ctx_id,
                          uint32_t flags,
                          uint64_t ring_idx,
                          uint64_t fence_id)
{
   SCOPE_LOCK_RENDERER();
   return vkr_renderer_submit_fence(ctx_id, flags, ring_idx, fence_id);
}

bool
render_state_create_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             uint64_t blob_id,
                             uint64_t blob_size,
                             uint32_t blob_flags,
                             enum virgl_resource_fd_type *out_fd_type,
                             int *out_res_fd,
                             uint32_t *out_map_info,
                             struct virgl_resource_vulkan_info *out_vulkan_info)
{
   SCOPE_LOCK_RENDERER();
   return vkr_renderer_create_resource(ctx_id, res_id, blob_id, blob_size, blob_flags,
                                       out_fd_type, out_res_fd, out_map_info,
                                       out_vulkan_info);
}

bool
render_state_import_resource(uint32_t ctx_id,
                             uint32_t res_id,
                             enum virgl_resource_fd_type fd_type,
                             int fd,
                             uint64_t size)
{
   SCOPE_LOCK_RENDERER();
   return vkr_renderer_import_resource(ctx_id, res_id, fd_type, fd, size);
}

void
render_state_destroy_resource(uint32_t ctx_id, uint32_t res_id)
{
   SCOPE_LOCK_RENDERER();
   vkr_renderer_destroy_resource(ctx_id, res_id);
}
