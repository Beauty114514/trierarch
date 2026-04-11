/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "proxy_context.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include "server/render_protocol.h"
#include "util/anon_file.h"
#include "util/bitscan.h"

#include "proxy_client.h"

struct proxy_fence {
   uint32_t flags;
   uint32_t seqno;
   uint64_t fence_id;
   struct list_head head;
};

static inline void
proxy_context_resource_add(struct proxy_context *ctx, uint32_t res_id)
{
   assert(!_mesa_hash_table_search(ctx->resource_table, (void *)(uintptr_t)res_id));
   _mesa_hash_table_insert(ctx->resource_table, (void *)(uintptr_t)res_id, NULL);
}

static inline bool
proxy_context_resource_find(struct proxy_context *ctx, uint32_t res_id)
{
   return _mesa_hash_table_search(ctx->resource_table, (void *)(uintptr_t)res_id);
}

static inline void
proxy_context_resource_remove(struct proxy_context *ctx, uint32_t res_id)
{
   _mesa_hash_table_remove_key(ctx->resource_table, (void *)(uintptr_t)res_id);
}

static inline bool
proxy_context_resource_table_init(struct proxy_context *ctx)
{
   ctx->resource_table = _mesa_hash_table_create_u32_keys(NULL);
   return ctx->resource_table;
}

static inline void
proxy_context_resource_table_fini(struct proxy_context *ctx)
{
   _mesa_hash_table_destroy(ctx->resource_table, NULL);
}

static bool
proxy_fence_is_signaled(const struct proxy_fence *fence, uint32_t cur_seqno)
{
   /* takes wrapping into account */
   const uint32_t d = cur_seqno - fence->seqno;
   return d < INT32_MAX;
}

static struct proxy_fence *
proxy_context_alloc_fence(struct proxy_context *ctx)
{
   struct proxy_fence *fence = NULL;

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_lock(&ctx->free_fences_mutex);

   if (!list_is_empty(&ctx->free_fences)) {
      fence = list_first_entry(&ctx->free_fences, struct proxy_fence, head);
      list_del(&fence->head);
   }

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_unlock(&ctx->free_fences_mutex);

   return fence ? fence : malloc(sizeof(*fence));
}

static void
proxy_context_free_fence(struct proxy_context *ctx, struct proxy_fence *fence)
{
   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_lock(&ctx->free_fences_mutex);

   list_add(&fence->head, &ctx->free_fences);

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_unlock(&ctx->free_fences_mutex);
}

static uint32_t
proxy_context_load_timeline_seqno(struct proxy_context *ctx, uint32_t ring_idx)
{
   return atomic_load(&ctx->timeline_seqnos[ring_idx]);
}

static bool
proxy_context_retire_timeline_fences_locked(struct proxy_context *ctx,
                                            uint32_t ring_idx,
                                            uint32_t cur_seqno)
{
   struct proxy_timeline *timeline = &ctx->timelines[ring_idx];
   bool force_retire_all = false;

   /* check if the socket has been disconnected (i.e., the other end has
    * crashed) if no progress is made after a while
    */
   if (timeline->cur_seqno == cur_seqno && !list_is_empty(&timeline->fences)) {
      timeline->cur_seqno_stall_count++;
      if (timeline->cur_seqno_stall_count < 100 ||
          proxy_socket_is_connected(&ctx->socket))
         return false;

      /* socket has been disconnected */
      force_retire_all = true;
   }

   timeline->cur_seqno = cur_seqno;
   timeline->cur_seqno_stall_count = 0;

   list_for_each_entry_safe (struct proxy_fence, fence, &timeline->fences, head) {
      if (!proxy_fence_is_signaled(fence, timeline->cur_seqno) && !force_retire_all)
         return false;

      ctx->base.fence_retire(&ctx->base, ring_idx, fence->fence_id);

      list_del(&fence->head);
      proxy_context_free_fence(ctx, fence);
   }

   return true;
}

static void
proxy_context_retire_fences_internal(struct proxy_context *ctx)
{
   if (ctx->sync_thread.fence_eventfd >= 0)
      flush_eventfd(ctx->sync_thread.fence_eventfd);

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_lock(&ctx->timeline_mutex);

   uint64_t new_busy_mask = 0;
   uint64_t old_busy_mask = ctx->timeline_busy_mask;
   while (old_busy_mask) {
      const uint32_t ring_idx = u_bit_scan64(&old_busy_mask);
      const uint32_t cur_seqno = proxy_context_load_timeline_seqno(ctx, ring_idx);
      if (!proxy_context_retire_timeline_fences_locked(ctx, ring_idx, cur_seqno))
         new_busy_mask |= 1ull << ring_idx;
   }

   ctx->timeline_busy_mask = new_busy_mask;

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_unlock(&ctx->timeline_mutex);
}

static int
proxy_context_sync_thread(void *arg)
{
   struct proxy_context *ctx = arg;
   struct pollfd poll_fds[2] = {
      [0] = {
         .fd = ctx->sync_thread.fence_eventfd,
         .events = POLLIN,
      },
      [1] = {
         .fd = ctx->socket.fd,
      },
   };

   assert(proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB);

   while (!ctx->sync_thread.stop) {
      const int ret = poll(poll_fds, ARRAY_SIZE(poll_fds), -1);
      if (ret <= 0) {
         if (ret < 0 && (errno == EINTR || errno == EAGAIN))
            continue;

         proxy_log("failed to poll fence eventfd");
         break;
      }

      proxy_context_retire_fences_internal(ctx);
   }

   return 0;
}

static int
proxy_context_submit_fence(struct virgl_context *base,
                           uint32_t flags,
                           uint32_t ring_idx,
                           uint64_t fence_id)
{
   struct proxy_context *ctx = (struct proxy_context *)base;
   const uint64_t old_busy_mask = ctx->timeline_busy_mask;

   if (ring_idx >= PROXY_CONTEXT_TIMELINE_COUNT)
      return -EINVAL;

   struct proxy_timeline *timeline = &ctx->timelines[ring_idx];
   struct proxy_fence *fence = proxy_context_alloc_fence(ctx);
   if (!fence)
      return -ENOMEM;

   fence->flags = flags;
   fence->seqno = timeline->next_seqno++;
   fence->fence_id = fence_id;

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_lock(&ctx->timeline_mutex);

   list_addtail(&fence->head, &timeline->fences);
   ctx->timeline_busy_mask |= 1ull << ring_idx;

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_unlock(&ctx->timeline_mutex);

   const struct render_context_op_submit_fence_request req = {
      .header.op = RENDER_CONTEXT_OP_SUBMIT_FENCE,
      .flags = flags,
      .ring_index = ring_idx,
      .seqno = fence->seqno,
   };
   if (proxy_socket_send_request(&ctx->socket, &req, sizeof(req)))
      return 0;

   /* recover timeline fences and busy_mask on submit_fence request failure */
   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_lock(&ctx->timeline_mutex);

   list_del(&fence->head);
   ctx->timeline_busy_mask = old_busy_mask;

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB)
      mtx_unlock(&ctx->timeline_mutex);

   proxy_context_free_fence(ctx, fence);
   proxy_log("failed to submit fence");
   return -1;
}

static void
proxy_context_retire_fences(struct virgl_context *base)
{
   struct proxy_context *ctx = (struct proxy_context *)base;

   assert(!(proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB));
   proxy_context_retire_fences_internal(ctx);
}

static int
proxy_context_get_fencing_fd(struct virgl_context *base)
{
   struct proxy_context *ctx = (struct proxy_context *)base;

   assert(!(proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB));
   return ctx->sync_thread.fence_eventfd;
}

static int
proxy_context_submit_cmd(struct virgl_context *base, const void *buffer, size_t size)
{
   struct proxy_context *ctx = (struct proxy_context *)base;

   if (!size)
      return 0;

   struct render_context_op_submit_cmd_request req = {
      .header.op = RENDER_CONTEXT_OP_SUBMIT_CMD,
      .size = size,
   };

   const size_t inlined = MIN2(size, sizeof(req.cmd));
   memcpy(req.cmd, buffer, inlined);

   if (!proxy_socket_send_request(&ctx->socket, &req, sizeof(req))) {
      proxy_log("failed to submit cmd");
      return -1;
   }

   if (size > inlined) {
      if (!proxy_socket_send_request(&ctx->socket, (const char *)buffer + inlined,
                                     size - inlined)) {
         proxy_log("failed to submit large cmd buffer");
         return -1;
      }
   }

   return 0;
}

static bool
validate_resource_fd_shm(int fd, uint64_t expected_size)
{
#if defined(HAVE_MEMFD_CREATE)
   static const int blocked_seals = F_SEAL_WRITE;

   const int seals = fcntl(fd, F_GET_SEALS);
   if (seals & blocked_seals) {
      proxy_log("failed to validate shm seals(%d): blocked(%d)", seals, blocked_seals);
      return false;
   }
#endif

   const uint64_t size = lseek(fd, 0, SEEK_END);
   if (size != expected_size) {
      proxy_log("failed to validate shm size(%" PRIu64 ") expected(%" PRIu64 ")", size,
                expected_size);
      return false;
   }

   return true;
}

static inline int
add_required_seals_to_fd(int fd)
{
#if defined(HAVE_MEMFD_CREATE)
   return fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW);
#else
   // if memfd_create is not available pretend file sealing worked
   (void)fd;
   return 0;
#endif
}

static int
proxy_context_get_blob(struct virgl_context *base,
                       uint32_t res_id,
                       uint64_t blob_id,
                       uint64_t blob_size,
                       uint32_t blob_flags,
                       struct virgl_context_blob *blob)
{
   /* RENDER_CONTEXT_OP_CREATE_RESOURCE implies resource attach, thus proxy tracks
    * resources created here to avoid double attaching the same resource when proxy is on
    * attach_resource callback.
    */
   struct proxy_context *ctx = (struct proxy_context *)base;

   const struct render_context_op_create_resource_request req = {
      .header.op = RENDER_CONTEXT_OP_CREATE_RESOURCE,
      .res_id = res_id,
      .blob_id = blob_id,
      .blob_size = blob_size,
      .blob_flags = blob_flags,
   };
   if (!proxy_socket_send_request(&ctx->socket, &req, sizeof(req))) {
      proxy_log("failed to get blob %" PRIu64, blob_id);
      return -1;
   }

   struct render_context_op_create_resource_reply reply;
   int reply_fd;
   int reply_fd_count;
   if (!proxy_socket_receive_reply_with_fds(&ctx->socket, &reply, sizeof(reply),
                                            &reply_fd, 1, &reply_fd_count)) {
      proxy_log("failed to get reply of blob %" PRIu64, blob_id);
      return -1;
   }

   if (!reply_fd_count) {
      proxy_log("invalid reply for blob %" PRIu64, blob_id);
      return -1;
   }

   bool reply_fd_valid = false;
   switch (reply.fd_type) {
   case VIRGL_RESOURCE_FD_DMABUF:
      /* TODO validate the fd is dmabuf >= blob_size */
      reply_fd_valid = true;
      break;
   case VIRGL_RESOURCE_FD_OPAQUE:
      /* this will be validated when imported by the client */
      reply_fd_valid = true;
      break;
   case VIRGL_RESOURCE_FD_SHM:
      /* validate the seals and size here */
      reply_fd_valid = !add_required_seals_to_fd(reply_fd) &&
                       validate_resource_fd_shm(reply_fd, blob_size);
      break;
   default:
      break;
   }
   if (!reply_fd_valid) {
      proxy_log("invalid fd type %d for blob %" PRIu64, reply.fd_type, blob_id);
      close(reply_fd);
      return -1;
   }

   blob->type = reply.fd_type;
   blob->u.fd = reply_fd;
   blob->map_info = reply.map_info;

   if (reply.fd_type == VIRGL_RESOURCE_FD_OPAQUE)
      blob->vulkan_info = reply.vulkan_info;

   proxy_context_resource_add(ctx, res_id);

   return 0;
}

static int
proxy_context_transfer_3d(struct virgl_context *base,
                          struct virgl_resource *res,
                          UNUSED const struct vrend_transfer_info *info,
                          UNUSED int transfer_mode)
{
   struct proxy_context *ctx = (struct proxy_context *)base;

   proxy_log("no transfer support for ctx %d and res %d", ctx->base.ctx_id, res->res_id);
   return -1;
}

static void
proxy_context_detach_resource(struct virgl_context *base, struct virgl_resource *res)
{
   struct proxy_context *ctx = (struct proxy_context *)base;
   const uint32_t res_id = res->res_id;

   /* avoid detaching resource not belonging to this context */
   if (!proxy_context_resource_find(ctx, res_id))
      return;

   const struct render_context_op_destroy_resource_request req = {
      .header.op = RENDER_CONTEXT_OP_DESTROY_RESOURCE,
      .res_id = res_id,
   };
   if (!proxy_socket_send_request(&ctx->socket, &req, sizeof(req)))
      proxy_log("failed to detach res %d", res_id);

   proxy_context_resource_remove(ctx, res_id);
}

static void
proxy_context_attach_resource(struct virgl_context *base, struct virgl_resource *res)
{
   struct proxy_context *ctx = (struct proxy_context *)base;
   const uint32_t res_id = res->res_id;

   /* avoid importing resources created from RENDER_CONTEXT_OP_CREATE_RESOURCE */
   if (proxy_context_resource_find(ctx, res_id))
      return;

   /* The current render protocol only supports importing dma-buf, shm or pipe resource
    * that can be exported to dma-buf. A protocol change is needed when there exists use
    * case for importing external Vulkan opaque resource.
    */
   if (res->fd_type != VIRGL_RESOURCE_FD_INVALID &&
       res->fd_type != VIRGL_RESOURCE_FD_DMABUF &&
       res->fd_type != VIRGL_RESOURCE_FD_SHM) {
      proxy_log("failed to attach res %d with fd_type %d", res_id, res->fd_type);
      return;
   }

   enum virgl_resource_fd_type res_fd_type = res->fd_type;
   int res_fd = res->fd;
   uint64_t res_size = res->map_size;
   bool close_res_fd = false;
   if (res_fd_type == VIRGL_RESOURCE_FD_INVALID) {
      /* importable pipe resouce can only export as dma-buf */
      res_fd_type = virgl_resource_export_fd(res, &res_fd);
      if (res_fd_type != VIRGL_RESOURCE_FD_DMABUF) {
         /* close fd for unexpected fd type from succeeded export */
         if (res_fd_type != VIRGL_RESOURCE_FD_INVALID)
            close(res_fd);
         proxy_log("exported res %d to unexpected fd_type %d", res_id, res_fd_type);
         return;
      }

      /* get the actual dma-buf size here because:
       * - pipe resource created by vrend has a zero map_size
       * - blob resource created by vrend can have non-zero fake map_size
       */
      res_size = lseek(res_fd, 0, SEEK_END);

      close_res_fd = true;
   }

   /* the proxy ignores iovs since transfer_3d is not supported */
   const struct render_context_op_import_resource_request req = {
      .header.op = RENDER_CONTEXT_OP_IMPORT_RESOURCE,
      .res_id = res_id,
      .fd_type = res_fd_type,
      .size = res_size,
   };
   if (!proxy_socket_send_request_with_fds(&ctx->socket, &req, sizeof(req), &res_fd, 1))
      proxy_log("failed to attach res %d", res_id);

   if (res_fd >= 0 && close_res_fd)
      close(res_fd);

   proxy_context_resource_add(ctx, res_id);
}

static void
proxy_context_destroy(struct virgl_context *base)
{
   struct proxy_context *ctx = (struct proxy_context *)base;

   /* ask the server process to terminate the context process */
   if (!proxy_client_destroy_context(ctx->client, ctx->base.ctx_id))
      proxy_log("failed to destroy ctx %d", ctx->base.ctx_id);

   if (ctx->sync_thread.fence_eventfd >= 0) {
      if (ctx->sync_thread.created) {
         ctx->sync_thread.stop = true;
         write_eventfd(ctx->sync_thread.fence_eventfd, 1);
         thrd_join(ctx->sync_thread.thread, NULL);
      }

      close(ctx->sync_thread.fence_eventfd);
   }

   if (ctx->shmem.ptr)
      munmap(ctx->shmem.ptr, ctx->shmem.size);
   if (ctx->shmem.fd >= 0)
      close(ctx->shmem.fd);

   if (ctx->timeline_seqnos) {
      for (uint32_t i = 0; i < PROXY_CONTEXT_TIMELINE_COUNT; i++) {
         struct proxy_timeline *timeline = &ctx->timelines[i];
         list_for_each_entry_safe (struct proxy_fence, fence, &timeline->fences, head)
            free(fence);
      }
   }
   mtx_destroy(&ctx->timeline_mutex);

   list_for_each_entry_safe (struct proxy_fence, fence, &ctx->free_fences, head)
      free(fence);
   mtx_destroy(&ctx->free_fences_mutex);

   proxy_context_resource_table_fini(ctx);

   proxy_socket_fini(&ctx->socket);

   free(ctx);
}

static void
proxy_context_init_base(struct proxy_context *ctx)
{
   ctx->base.destroy = proxy_context_destroy;
   ctx->base.attach_resource = proxy_context_attach_resource;
   ctx->base.detach_resource = proxy_context_detach_resource;
   ctx->base.transfer_3d = proxy_context_transfer_3d;
   ctx->base.get_blob = proxy_context_get_blob;
   ctx->base.submit_cmd = proxy_context_submit_cmd;

   ctx->base.get_fencing_fd = proxy_context_get_fencing_fd;
   ctx->base.retire_fences = proxy_context_retire_fences;
   ctx->base.submit_fence = proxy_context_submit_fence;
}

static bool
proxy_context_init_fencing(struct proxy_context *ctx)
{
   /* The render server updates the shmem for the current seqnos and
    * optionally notifies using the eventfd.  That means, when only
    * VIRGL_RENDERER_THREAD_SYNC is set, we just need to set up the eventfd.
    * When VIRGL_RENDERER_ASYNC_FENCE_CB is also set, we need to create a sync
    * thread as well.
    *
    * Fence polling can always check the shmem directly.
    */
   if (!(proxy_renderer.flags & VIRGL_RENDERER_THREAD_SYNC))
      return true;

   ctx->sync_thread.fence_eventfd = create_eventfd(0);
   if (ctx->sync_thread.fence_eventfd < 0) {
      proxy_log("failed to create fence eventfd");
      return false;
   }

   if (proxy_renderer.flags & VIRGL_RENDERER_ASYNC_FENCE_CB) {
      int ret = thrd_create(&ctx->sync_thread.thread, proxy_context_sync_thread, ctx);
      if (ret != thrd_success) {
         proxy_log("failed to create sync thread");
         return false;
      }
      ctx->sync_thread.created = true;
   }

   return true;
}

static bool
proxy_context_init_timelines(struct proxy_context *ctx)
{
   atomic_uint *timeline_seqnos = ctx->shmem.ptr;
   for (uint32_t i = 0; i < ARRAY_SIZE(ctx->timelines); i++) {
      atomic_init(&timeline_seqnos[i], 0);

      struct proxy_timeline *timeline = &ctx->timelines[i];
      timeline->cur_seqno = 0;
      timeline->next_seqno = 1;
      list_inithead(&timeline->fences);
   }

   ctx->timeline_seqnos = timeline_seqnos;

   return true;
}

static int
alloc_memfd(const char *name, size_t size, void **out_ptr)
{
   int fd = os_create_anonymous_file(size, name);
   if (fd < 0)
      return -1;

   int ret = add_required_seals_to_fd(fd);
   if (ret)
      goto fail;

   if (!out_ptr)
      return fd;

   void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (ptr == MAP_FAILED)
      goto fail;

   *out_ptr = ptr;
   return fd;

fail:
   close(fd);
   return -1;
}

static bool
proxy_context_init_shmem(struct proxy_context *ctx)
{
   const size_t shmem_size = sizeof(*ctx->timeline_seqnos) * PROXY_CONTEXT_TIMELINE_COUNT;
   ctx->shmem.fd = alloc_memfd("proxy-ctx", shmem_size, &ctx->shmem.ptr);
   if (ctx->shmem.fd < 0)
      return false;

   ctx->shmem.size = shmem_size;

   return true;
}

static bool
proxy_context_init(struct proxy_context *ctx, uint32_t ctx_flags)
{
   if (!proxy_context_init_shmem(ctx) || !proxy_context_init_timelines(ctx) ||
       !proxy_context_init_fencing(ctx) || !proxy_context_resource_table_init(ctx)) {
      proxy_log("failed to pre-initialize context");
      return false;
   }

   const struct render_context_op_init_request req = {
      .header.op = RENDER_CONTEXT_OP_INIT,
      .flags = ctx_flags,
      .shmem_size = ctx->shmem.size,
   };
   const int req_fds[2] = { ctx->shmem.fd, ctx->sync_thread.fence_eventfd };
   const int req_fd_count = req_fds[1] >= 0 ? 2 : 1;
   if (!proxy_socket_send_request_with_fds(&ctx->socket, &req, sizeof(req), req_fds,
                                           req_fd_count)) {
      proxy_log("failed to initialize context");
      return false;
   }

   return true;
}

struct virgl_context *
proxy_context_create(uint32_t ctx_id,
                     uint32_t ctx_flags,
                     size_t debug_len,
                     const char *debug_name)
{
   struct proxy_client *client = proxy_renderer.client;
   struct proxy_context *ctx;

   int ctx_fd;
   if (!proxy_client_create_context(client, ctx_id, debug_len, debug_name, &ctx_fd)) {
      proxy_log("failed to create a context");
      return NULL;
   }

   ctx = calloc(1, sizeof(*ctx));
   if (!ctx) {
      close(ctx_fd);
      return NULL;
   }

   proxy_context_init_base(ctx);
   ctx->client = client;
   proxy_socket_init(&ctx->socket, ctx_fd);
   ctx->shmem.fd = -1;
   mtx_init(&ctx->timeline_mutex, mtx_plain);
   mtx_init(&ctx->free_fences_mutex, mtx_plain);
   list_inithead(&ctx->free_fences);
   ctx->sync_thread.fence_eventfd = -1;

   if (!proxy_context_init(ctx, ctx_flags)) {
      proxy_context_destroy(&ctx->base);
      return NULL;
   }

   return &ctx->base;
}
