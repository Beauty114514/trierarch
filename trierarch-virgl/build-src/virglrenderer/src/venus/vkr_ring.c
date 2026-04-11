/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_ring.h"

#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

#include "venus-protocol/vn_protocol_renderer_dispatches.h"

#include "vkr_context.h"

static inline void *
get_resource_pointer(const struct vkr_resource *res, size_t offset)
{
   assert(offset < res->size);
   return res->u.data + offset;
}

static void
vkr_ring_init_extra(struct vkr_ring *ring, const struct vkr_ring_layout *layout)
{
   struct vkr_ring_extra *extra = &ring->extra;

   extra->offset = layout->extra.begin;
   extra->region = vkr_region_make_relative(&layout->extra);
}

static void
vkr_ring_init_buffer(struct vkr_ring *ring, const struct vkr_ring_layout *layout)
{
   struct vkr_ring_buffer *buf = &ring->buffer;

   buf->size = vkr_region_size(&layout->buffer);
   assert(util_is_power_of_two_nonzero(buf->size));
   buf->mask = buf->size - 1;
   buf->cur = 0;
   buf->data = get_resource_pointer(layout->resource, layout->buffer.begin);
}

static bool
vkr_ring_init_control(struct vkr_ring *ring, const struct vkr_ring_layout *layout)
{
   struct vkr_ring_control *ctrl = &ring->control;

   ctrl->head = get_resource_pointer(layout->resource, layout->head.begin);
   ctrl->tail = get_resource_pointer(layout->resource, layout->tail.begin);
   ctrl->status = get_resource_pointer(layout->resource, layout->status.begin);

   /* we will manage head and status, and we expect them to be 0 initially */
   if (*ctrl->head || *ctrl->status)
      return false;

   return true;
}

static void
vkr_ring_store_head(struct vkr_ring *ring, uint32_t ring_head)
{
   /* the renderer is expected to load the head with memory_order_acquire,
    * forming a release-acquire ordering
    */
   atomic_store_explicit(ring->control.head, ring_head, memory_order_release);
}

static uint32_t
vkr_ring_load_tail(const struct vkr_ring *ring)
{
   /* the driver is expected to store the tail with memory_order_release,
    * forming a release-acquire ordering
    */
   return atomic_load_explicit(ring->control.tail, memory_order_acquire);
}

static void
vkr_ring_unset_status_bits(struct vkr_ring *ring, uint32_t mask)
{
   atomic_fetch_and_explicit(ring->control.status, ~mask, memory_order_seq_cst);
}

static void
vkr_ring_read_buffer(struct vkr_ring *ring, void *data, uint32_t size)
{
   struct vkr_ring_buffer *buf = &ring->buffer;

   const size_t offset = buf->cur & buf->mask;
   assert(size <= buf->size);
   if (offset + size <= buf->size) {
      memcpy(data, buf->data + offset, size);
   } else {
      const size_t s = buf->size - offset;
      memcpy(data, buf->data + offset, s);
      memcpy((uint8_t *)data + s, buf->data, size - s);
   }

   /* advance cur */
   buf->cur += size;
}

static inline void
vkr_ring_init_dispatch(struct vkr_ring *ring, struct vkr_context *ctx)
{
   ring->dispatch = ctx->dispatch;
   ring->dispatch.encoder = (struct vn_cs_encoder *)&ring->encoder;
   ring->dispatch.decoder = (struct vn_cs_decoder *)&ring->decoder;
}

struct vkr_ring *
vkr_ring_create(const struct vkr_ring_layout *layout,
                struct vkr_context *ctx,
                uint64_t idle_timeout)
{
   struct vkr_ring *ring = calloc(1, sizeof(*ring));
   if (!ring)
      return NULL;

   ring->resource = layout->resource;

   if (!vkr_ring_init_control(ring, layout))
      goto err_init_control;

   vkr_ring_init_buffer(ring, layout);
   vkr_ring_init_extra(ring, layout);

   ring->cmd = malloc(ring->buffer.size);
   if (!ring->cmd)
      goto err_cmd_malloc;

   if (vkr_cs_decoder_init(&ring->decoder, ctx))
      goto err_cs_decoder_init;

   if (vkr_cs_encoder_init(&ring->encoder, &ctx->cs_fatal_error))
      goto err_cs_encoder_init;

   vkr_ring_init_dispatch(ring, ctx);

   ring->idle_timeout = idle_timeout;

   if (mtx_init(&ring->mutex, mtx_plain) != thrd_success)
      goto err_mtx_init;

   if (cnd_init(&ring->cond) != thrd_success)
      goto err_cond_init;

   return ring;

err_cond_init:
   mtx_destroy(&ring->mutex);
err_mtx_init:
   vkr_cs_encoder_fini(&ring->encoder);
err_cs_encoder_init:
   vkr_cs_decoder_fini(&ring->decoder);
err_cs_decoder_init:
   free(ring->cmd);
err_cmd_malloc:
err_init_control:
   free(ring);
   return NULL;
}

void
vkr_ring_destroy(struct vkr_ring *ring)
{
   list_del(&ring->head);

   assert(!ring->started);
   vkr_cs_decoder_fini(&ring->decoder);
   vkr_cs_encoder_fini(&ring->encoder);
   mtx_destroy(&ring->mutex);
   cnd_destroy(&ring->cond);
   free(ring->cmd);
   free(ring);
}

static uint64_t
vkr_ring_now(void)
{
   const uint64_t ns_per_sec = 1000000000llu;
   struct timespec now;
   if (clock_gettime(CLOCK_MONOTONIC, &now))
      return 0;
   return ns_per_sec * now.tv_sec + now.tv_nsec;
}

static void
vkr_ring_relax(uint32_t *iter)
{
   /* TODO do better */
   const uint32_t busy_wait_order = 4;
   const uint32_t base_sleep_us = 10;

   (*iter)++;
   if (*iter < (1u << busy_wait_order)) {
      thrd_yield();
      return;
   }

   const uint32_t shift = util_last_bit(*iter) - busy_wait_order - 1;
   const uint32_t us = base_sleep_us << shift;
   const struct timespec ts = {
      .tv_sec = us / 1000000,
      .tv_nsec = (us % 1000000) * 1000,
   };
   clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

static bool
vkr_ring_submit_cmd(struct vkr_ring *ring,
                    const uint8_t *buffer,
                    size_t size,
                    uint32_t ring_head)
{
   struct vkr_cs_decoder *dec = &ring->decoder;
   if (vkr_cs_decoder_get_fatal(dec)) {
      vkr_log("ring_submit_cmd: early bail due to fatal decoder state");
      return false;
   }

   vkr_cs_decoder_set_buffer_stream(dec, buffer, size);

   while (vkr_cs_decoder_has_command(dec)) {
      vn_dispatch_command(&ring->dispatch);
      if (vkr_cs_decoder_get_fatal(dec)) {
         vkr_log("ring_submit_cmd: vn_dispatch_command failed");

         vkr_cs_decoder_reset(dec);
         return false;
      }

      /* update the ring head intra-cs to optimize ring space */
      const uint32_t cur_ring_head = ring_head + (dec->cur - buffer);
      vkr_ring_store_head(ring, cur_ring_head);
      vkr_context_on_ring_seqno_update(ring->dispatch.data, ring->id, cur_ring_head);
   }

   vkr_cs_decoder_reset(dec);
   return true;
}

static int
vkr_ring_thread(void *arg)
{
   struct vkr_ring *ring = arg;
   struct vkr_context *ctx = ring->dispatch.data;
   char thread_name[16];

   snprintf(thread_name, ARRAY_SIZE(thread_name), "vkr-ring-%d", ctx->ctx_id);
   u_thread_setname(thread_name);
   if (ring->prio_valid && setpriority(PRIO_PROCESS, 0, ring->prio)) {
#ifdef DEBUG
      /* Currently venus doesn't forward the CAP_SYS_NICE request upon forking, so
       * requesting a high priority outside of the normal range would be at the best
       * effort. Thus only enable logging on debug build.
       */
      vkr_log("failed to set ring thread priority to %d (%d)", ring->prio, errno);
#endif
   }

   uint64_t last_submit = vkr_ring_now();
   uint32_t relax_iter = 0;
   int ret = 0;
   while (ring->started) {
      bool wait = false;
      if (vkr_ring_now() >= last_submit + ring->idle_timeout) {
         ring->pending_notify = false;
         vkr_ring_set_status_bits(ring, VK_RING_STATUS_IDLE_BIT_MESA);
         wait = ring->buffer.cur == vkr_ring_load_tail(ring);
         if (!wait)
            vkr_ring_unset_status_bits(ring, VK_RING_STATUS_IDLE_BIT_MESA);
      }

      if (wait) {
         TRACE_SCOPE("ring idle");

         mtx_lock(&ring->mutex);
         if (ring->started && !ring->pending_notify)
            cnd_wait(&ring->cond, &ring->mutex);
         vkr_ring_unset_status_bits(ring, VK_RING_STATUS_IDLE_BIT_MESA);
         mtx_unlock(&ring->mutex);

         if (!ring->started)
            break;

         last_submit = vkr_ring_now();
         relax_iter = 0;
      }

      const uint32_t cmd_size = vkr_ring_load_tail(ring) - ring->buffer.cur;
      if (cmd_size) {
         if (cmd_size > ring->buffer.size) {
            vkr_log("%s: cmd_size(%u) > ring->buffer.size(%u)", __func__, cmd_size,
                    ring->buffer.size);
            ret = -EINVAL;
            break;
         }

         const uint32_t ring_head = ring->buffer.cur;
         vkr_ring_read_buffer(ring, ring->cmd, cmd_size);

         if (!vkr_ring_submit_cmd(ring, ring->cmd, cmd_size, ring_head)) {
            ret = -EINVAL;
            break;
         }

         last_submit = vkr_ring_now();
         relax_iter = 0;
      } else {
         /* Get the active wait_ring seqno first to ensure ordering. */
         uint32_t wait_ring_seqno = 0;
         if (vkr_context_get_wait_ring_seqno(ctx, ring->id, &wait_ring_seqno)) {
            /* Error out if the latest ring cmd is unable to signal the virtqueue that is
             * currently waiting for this ring. This happens when the driver emits invalid
             * asynchronous ring wait cmds.
             */
            const uint32_t ring_tail = vkr_ring_load_tail(ring);
            if (unlikely(!vkr_seqno_ge(ring_tail, wait_ring_seqno))) {
               vkr_log("%s: ring seqno(%u) unable to reach wait seqno(%u)", __func__,
                       ring_tail, wait_ring_seqno);
               ret = -EINVAL;
               break;
            }
         }

         vkr_ring_relax(&relax_iter);
      }
   }

   if (ret < 0) {
      vkr_ring_set_status_bits(ring, VK_RING_STATUS_FATAL_BIT_MESA);
      vkr_context_on_ring_fatal(ctx);
   }

   return ret;
}

void
vkr_ring_start(struct vkr_ring *ring)
{
   int ret;

   assert(!ring->started);
   ring->started = true;
   ret = thrd_create(&ring->thread, vkr_ring_thread, ring);
   if (ret != thrd_success)
      ring->started = false;
}

bool
vkr_ring_stop(struct vkr_ring *ring)
{
   mtx_lock(&ring->mutex);
   if (thrd_equal(ring->thread, thrd_current())) {
      mtx_unlock(&ring->mutex);
      return false;
   }
   assert(ring->started);
   ring->started = false;
   cnd_signal(&ring->cond);
   mtx_unlock(&ring->mutex);

   thrd_join(ring->thread, NULL);

   return true;
}

void
vkr_ring_notify(struct vkr_ring *ring)
{
   mtx_lock(&ring->mutex);
   ring->pending_notify = true;
   cnd_signal(&ring->cond);
   mtx_unlock(&ring->mutex);

   {
      TRACE_SCOPE("ring notify done");
   }
}

bool
vkr_ring_write_extra(struct vkr_ring *ring, size_t offset, uint32_t val)
{
   struct vkr_ring_extra *extra = &ring->extra;

   if (unlikely(extra->cached_offset != offset || !extra->cached_data)) {
      const struct vkr_region access = VKR_REGION_INIT(offset, sizeof(val));
      if (!vkr_region_is_valid(&access) || !vkr_region_is_within(&access, &extra->region))
         return false;

      /* Mesa always sets offset to 0 and the cache hit rate will be 100% */
      extra->cached_offset = offset;
      extra->cached_data = get_resource_pointer(ring->resource, extra->offset + offset);
   }

   atomic_store_explicit(extra->cached_data, val, memory_order_release);

   {
      TRACE_SCOPE("ring extra done");
   }

   return true;
}

void
vkr_ring_submit_virtqueue_seqno(struct vkr_ring *ring, uint64_t seqno)
{
   mtx_lock(&ring->mutex);
   ring->virtqueue_seqno = seqno;

   /* There are 3 cases:
    * 1. ring is not waiting on the cond thus no-op
    * 2. ring is idle and then wakes up earlier
    * 3. ring is waiting for roundtrip and then checks seqno again
    */
   cnd_signal(&ring->cond);
   mtx_unlock(&ring->mutex);

   {
      TRACE_SCOPE("submit vq seqno done");
   }
}

bool
vkr_ring_wait_virtqueue_seqno(struct vkr_ring *ring, uint64_t seqno)
{
   TRACE_FUNC();

   bool ok = true;

   mtx_lock(&ring->mutex);
   while (ok && ring->started && ring->virtqueue_seqno < seqno)
      ok = cnd_wait(&ring->cond, &ring->mutex) == thrd_success;
   mtx_unlock(&ring->mutex);

   return ok;
}
