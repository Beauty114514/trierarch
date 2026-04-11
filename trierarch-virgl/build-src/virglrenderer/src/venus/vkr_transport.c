/*
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_transport.h"

#include "venus-protocol/vn_protocol_renderer_dispatches.h"
#include "venus-protocol/vn_protocol_renderer_transport.h"

#include "vkr_context.h"
#include "vkr_ring.h"

static void
vkr_dispatch_vkSetReplyCommandStreamMESA(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkSetReplyCommandStreamMESA *args)
{
   struct vkr_context *ctx = dispatch->data;
   struct vkr_resource *res = vkr_context_get_resource(ctx, args->pStream->resourceId);
   if (!res || res->fd_type != VIRGL_RESOURCE_FD_SHM) {
      vkr_log("failed to set reply stream: invalid res_id %u", args->pStream->resourceId);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_cs_encoder *enc = (struct vkr_cs_encoder *)dispatch->encoder;
   vkr_cs_encoder_set_stream(enc, res, args->pStream->offset, args->pStream->size);
}

static void
vkr_dispatch_vkSeekReplyCommandStreamMESA(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkSeekReplyCommandStreamMESA *args)
{
   struct vkr_cs_encoder *enc = (struct vkr_cs_encoder *)dispatch->encoder;
   vkr_cs_encoder_seek_stream(enc, args->position);
}

static void
vkr_dispatch_vkExecuteCommandStreamsMESA(
   struct vn_dispatch_context *dispatch,
   struct vn_command_vkExecuteCommandStreamsMESA *args)
{
   TRACE_FUNC();
   struct vkr_context *ctx = dispatch->data;
   struct vkr_cs_decoder *dec = (struct vkr_cs_decoder *)dispatch->decoder;
   struct vkr_cs_encoder *enc = (struct vkr_cs_encoder *)dispatch->encoder;

   if (unlikely(!args->streamCount)) {
      vkr_log("failed to execute command streams: no stream specified");
      vkr_context_set_fatal(ctx);
      return;
   }

   /* note that nested vkExecuteCommandStreamsMESA is not allowed */
   if (unlikely(vkr_cs_decoder_has_saved_state(dec))) {
      vkr_log("failed to execute command streams: nested execution");
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_cs_decoder_save_state(dec);

   for (uint32_t i = 0; i < args->streamCount; i++) {
      const VkCommandStreamDescriptionMESA *stream = &args->pStreams[i];

      if (args->pReplyPositions)
         vkr_cs_encoder_seek_stream(enc, args->pReplyPositions[i]);

      if (!stream->size)
         continue;

      if (unlikely(!vkr_cs_decoder_set_resource_stream(dec, ctx, stream->resourceId,
                                                       stream->offset, stream->size))) {
         vkr_log("failed to execute command streams: invalid stream %u res_id %u", i,
                 stream->resourceId);
         vkr_context_set_fatal(ctx);
         break;
      }

      while (vkr_cs_decoder_has_command(dec)) {
         vn_dispatch_command(dispatch);
         if (vkr_context_get_fatal(ctx))
            break;
      }

      if (vkr_context_get_fatal(ctx))
         break;
   }

   /* restore state unsets the last nested stream */
   vkr_cs_decoder_restore_state(dec);
}

static struct vkr_ring *
lookup_ring(struct vkr_context *ctx, uint64_t ring_id)
{
   mtx_lock(&ctx->ring_mutex);
   list_for_each_entry (struct vkr_ring, ring, &ctx->rings, head) {
      if (ring->id == ring_id) {
         mtx_unlock(&ctx->ring_mutex);
         return ring;
      }
   }
   mtx_unlock(&ctx->ring_mutex);
   return NULL;
}

static bool
vkr_ring_layout_init(struct vkr_ring_layout *layout,
                     const struct vkr_resource *res,
                     const VkRingCreateInfoMESA *info)
{
   *layout = (struct vkr_ring_layout){
      .resource = res,
      .head = VKR_REGION_INIT(info->offset + info->headOffset, sizeof(uint32_t)),
      .tail = VKR_REGION_INIT(info->offset + info->tailOffset, sizeof(uint32_t)),
      .status = VKR_REGION_INIT(info->offset + info->statusOffset, sizeof(uint32_t)),
      .buffer = VKR_REGION_INIT(info->offset + info->bufferOffset, info->bufferSize),
      .extra = VKR_REGION_INIT(info->offset + info->extraOffset, info->extraSize),
   };

   const struct vkr_region res_region = VKR_REGION_INIT(info->offset, info->size);
   const struct vkr_region *regions[] = {
      &layout->head, &layout->tail, &layout->status, &layout->buffer, &layout->extra,
   };

   const struct vkr_region res_size = VKR_REGION_INIT(0, res->size);
   if (!vkr_region_is_valid(&res_region) || !vkr_region_is_within(&res_region, &res_size))
      return false;

   for (size_t i = 0; i < ARRAY_SIZE(regions); i++) {
      const struct vkr_region *region = regions[i];

      if (!vkr_region_is_valid(region) || !vkr_region_is_within(region, &res_region)) {
         vkr_log("ring buffer control variable (begin=%lu, end=%lu) placed"
                 " out-of-bounds in shared memory layout",
                 region->begin, region->end);
         return false;
      }

      if (!vkr_region_is_aligned(region, 4)) {
         vkr_log("ring buffer control variable (begin=%lu, end=%lu) must be"
                 " 32-bit aligned in shared memory layout",
                 region->begin, region->end);
         return false;
      }
   }

   /* assumes region->size == 0 is valid */
   for (size_t i = 0; i < ARRAY_SIZE(regions); i++) {
      const struct vkr_region *region = regions[i];

      for (size_t j = i + 1; j < ARRAY_SIZE(regions); j++) {
         const struct vkr_region *other = regions[j];

         if (!vkr_region_is_disjoint(region, other)) {
            vkr_log("ring buffer control variable (begin=%lu, end=%lu)"
                    " overlaps with control variable (begin=%lu, end=%lu)",
                    other->begin, other->end, region->begin, region->end);
            return false;
         }
      }
   }

   const size_t buf_size = vkr_region_size(&layout->buffer);
   if (buf_size > VKR_RING_BUFFER_MAX_SIZE || !util_is_power_of_two_nonzero(buf_size)) {
      vkr_log("ring buffer size (%z) must be a power of two and not exceed %lu", buf_size,
              VKR_RING_BUFFER_MAX_SIZE);
      return false;
   }

   return true;
}

static inline bool
is_dispatched_from_vkr_context(const struct vn_dispatch_context *dispatch)
{
   /* this is for -Wgnu-statement-expression-from-macro-expansion */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   return dispatch->data == container_of(dispatch, struct vkr_context, dispatch);
#pragma GCC diagnostic pop
}

static void
vkr_dispatch_vkCreateRingMESA(struct vn_dispatch_context *dispatch,
                              struct vn_command_vkCreateRingMESA *args)
{
   struct vkr_context *ctx = dispatch->data;
   const VkRingCreateInfoMESA *info = args->pCreateInfo;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   const struct vkr_resource *res = vkr_context_get_resource(ctx, info->resourceId);
   if (!res || res->fd_type != VIRGL_RESOURCE_FD_SHM) {
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring_layout layout;
   if (!vkr_ring_layout_init(&layout, res, info)) {
      vkr_log("vkCreateRingMESA supplied with invalid buffer layout parameters");
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = vkr_ring_create(&layout, ctx, info->idleTimeout);
   if (!ring) {
      vkr_context_set_fatal(ctx);
      return;
   }

   ring->id = args->ring;

   mtx_lock(&ctx->ring_mutex);
   list_addtail(&ring->head, &ctx->rings);
   mtx_unlock(&ctx->ring_mutex);

   const VkRingMonitorInfoMESA *monitor_info =
      vkr_find_struct(info->pNext, VK_STRUCTURE_TYPE_RING_MONITOR_INFO_MESA);
   if (monitor_info) {
      if (!monitor_info->maxReportingPeriodMicroseconds) {
         vkr_log("invalid ring reporting period");
         vkr_context_set_fatal(ctx);
         return;
      }

      /* Start the ring monitoring thread or update the reporting rate of the running
       * thread to the smallest maxReportingPeriodMicroseconds recieved so far, and wake
       * it to begin reporting at the faster rate before the first driver check occurs.
       */
      if (!ctx->ring_monitor.started) {
         if (!vkr_context_ring_monitor_init(
                ctx, monitor_info->maxReportingPeriodMicroseconds)) {
            vkr_context_set_fatal(ctx);
            return;
         }
      } else if (monitor_info->maxReportingPeriodMicroseconds <
                 ctx->ring_monitor.report_period_us) {
         mtx_lock(&ctx->ring_monitor.mutex);
         ctx->ring_monitor.report_period_us =
            monitor_info->maxReportingPeriodMicroseconds;
         cnd_signal(&ctx->ring_monitor.cond);
         mtx_unlock(&ctx->ring_monitor.mutex);
      }

      ring->monitor = true;
   }

   const VkRingPriorityInfoMESA *priority_info =
      vkr_find_struct(info->pNext, VK_STRUCTURE_TYPE_RING_PRIORITY_INFO_MESA);
   if (priority_info) {
      ring->prio_valid = true;
      ring->prio = priority_info->priority;
   }

   vkr_ring_start(ring);
}

static void
vkr_dispatch_vkDestroyRingMESA(struct vn_dispatch_context *dispatch,
                               struct vn_command_vkDestroyRingMESA *args)
{
   struct vkr_context *ctx = dispatch->data;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = lookup_ring(ctx, args->ring);
   if (!ring || !vkr_ring_stop(ring)) {
      vkr_context_set_fatal(ctx);
      return;
   }

   mtx_lock(&ctx->ring_mutex);
   vkr_ring_destroy(ring);
   mtx_unlock(&ctx->ring_mutex);
}

static void
vkr_dispatch_vkNotifyRingMESA(struct vn_dispatch_context *dispatch,
                              struct vn_command_vkNotifyRingMESA *args)
{
   struct vkr_context *ctx = dispatch->data;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = lookup_ring(ctx, args->ring);
   if (!ring) {
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_ring_notify(ring);
}

static void
vkr_dispatch_vkWriteRingExtraMESA(struct vn_dispatch_context *dispatch,
                                  struct vn_command_vkWriteRingExtraMESA *args)
{
   struct vkr_context *ctx = dispatch->data;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = lookup_ring(ctx, args->ring);
   if (!ring) {
      vkr_context_set_fatal(ctx);
      return;
   }

   if (!vkr_ring_write_extra(ring, args->offset, args->value))
      vkr_context_set_fatal(ctx);
}

static void
vkr_dispatch_vkSubmitVirtqueueSeqnoMESA(struct vn_dispatch_context *dispatch,
                                        struct vn_command_vkSubmitVirtqueueSeqnoMESA *args)
{
   struct vkr_context *ctx = dispatch->data;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = lookup_ring(ctx, args->ring);
   if (!ring) {
      vkr_context_set_fatal(ctx);
      return;
   }

   vkr_ring_submit_virtqueue_seqno(ring, args->seqno);
}

static void
vkr_dispatch_vkWaitVirtqueueSeqnoMESA(struct vn_dispatch_context *dispatch,
                                      struct vn_command_vkWaitVirtqueueSeqnoMESA *args)
{
   struct vkr_context *ctx = dispatch->data;
   if (unlikely(is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on ring dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   /* this is for -Wgnu-statement-expression-from-macro-expansion */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   struct vkr_ring *ring = container_of(dispatch, struct vkr_ring, dispatch);
#pragma GCC diagnostic pop
   if (!vkr_ring_wait_virtqueue_seqno(ring, args->seqno))
      vkr_context_set_fatal(ctx);
}

static void
vkr_dispatch_vkWaitRingSeqnoMESA(struct vn_dispatch_context *dispatch,
                                 struct vn_command_vkWaitRingSeqnoMESA *args)
{
   struct vkr_context *ctx = dispatch->data;

   if (unlikely(!is_dispatched_from_vkr_context(dispatch))) {
      vkr_log("%s must be called on context dispatch", __func__);
      vkr_context_set_fatal(ctx);
      return;
   }

   struct vkr_ring *ring = lookup_ring(ctx, args->ring);
   if (!ring) {
      vkr_context_set_fatal(ctx);
      return;
   }

   /* ensure to wake ring up to properly error out invalid ring wait */
   vkr_ring_notify(ring);

   if (!vkr_context_wait_ring_seqno(ctx, ring, args->seqno))
      vkr_context_set_fatal(ctx);
}

void
vkr_context_init_transport_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkSetReplyCommandStreamMESA =
      vkr_dispatch_vkSetReplyCommandStreamMESA;
   dispatch->dispatch_vkSeekReplyCommandStreamMESA =
      vkr_dispatch_vkSeekReplyCommandStreamMESA;
   dispatch->dispatch_vkExecuteCommandStreamsMESA =
      vkr_dispatch_vkExecuteCommandStreamsMESA;
   dispatch->dispatch_vkCreateRingMESA = vkr_dispatch_vkCreateRingMESA;
   dispatch->dispatch_vkDestroyRingMESA = vkr_dispatch_vkDestroyRingMESA;
   dispatch->dispatch_vkNotifyRingMESA = vkr_dispatch_vkNotifyRingMESA;
   dispatch->dispatch_vkWriteRingExtraMESA = vkr_dispatch_vkWriteRingExtraMESA;
   dispatch->dispatch_vkSubmitVirtqueueSeqnoMESA =
      vkr_dispatch_vkSubmitVirtqueueSeqnoMESA;
   dispatch->dispatch_vkWaitVirtqueueSeqnoMESA = vkr_dispatch_vkWaitVirtqueueSeqnoMESA;
   dispatch->dispatch_vkWaitRingSeqnoMESA = vkr_dispatch_vkWaitRingSeqnoMESA;
}
