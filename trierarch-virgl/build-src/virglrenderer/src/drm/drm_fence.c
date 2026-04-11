/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <poll.h>
#include <string.h>

#include "virgl_context.h"
#include "virgl_fence.h"
#include "virgl_util.h"

#include "util/os_file.h"
#include "util/u_atomic.h"
#include "util/u_thread.h"

#include "drm_fence.h"
#include "drm_util.h"

/**
 * Tracking for a single fence on a timeline
 */
struct drm_fence {
   int fd;
   uint32_t flags;
   uint64_t fence_id;
   struct list_head node;
};

static void
drm_fence_destroy(struct drm_fence *fence)
{
   close(fence->fd);
   list_del(&fence->node);
   free(fence);
}

static struct drm_fence *
drm_fence_create(int fd, uint32_t flags, uint64_t fence_id)
{
   struct drm_fence *fence = calloc(1, sizeof(*fence));

   if (!fence)
      return NULL;

   fence->fd = os_dupfd_cloexec(fd);

   if (fence->fd < 0) {
      free(fence);
      return NULL;
   }

   fence->flags = flags;
   fence->fence_id = fence_id;

   return fence;
}

static int
thread_sync(void *arg)
{
   struct drm_timeline *timeline = arg;

   u_thread_setname(timeline->name);

   mtx_lock(&timeline->fence_mutex);
   while (!timeline->stop_sync_thread) {
      if (list_is_empty(&timeline->pending_fences)) {
         if (cnd_wait(&timeline->fence_cond, &timeline->fence_mutex))
            drm_err("error waiting on fence condition");
         continue;
      }

      struct drm_fence *fence =
         list_first_entry(&timeline->pending_fences, struct drm_fence, node);
      int ret;

      mtx_unlock(&timeline->fence_mutex);
      ret = poll(&(struct pollfd){fence->fd, POLLIN}, 1, -1); /* wait forever */
      mtx_lock(&timeline->fence_mutex);

      if (ret == 1) {
         drm_dbg("fence signaled: %p (%" PRIu64 ")", (void*)fence, fence->fence_id);
         timeline->fence_retire(timeline->vctx, timeline->ring_idx, fence->fence_id);
         drm_fence_destroy(fence);
      } else if (ret != 0) {
         drm_err("poll failed: %s", strerror(errno));
      }
   }
   mtx_unlock(&timeline->fence_mutex);

   return 0;
}

void
drm_timeline_init(struct drm_timeline *timeline, struct virgl_context *vctx,
                  const char *name, int ring_idx,
                  virgl_context_fence_retire fence_retire)
{
   timeline->vctx = vctx;
   timeline->name = name;
   timeline->ring_idx = ring_idx;
   timeline->fence_retire = fence_retire;

   timeline->last_fence_fd = -1;

   list_inithead(&timeline->pending_fences);

   mtx_init(&timeline->fence_mutex, mtx_plain);
   cnd_init(&timeline->fence_cond);

   timeline->sync_thread = u_thread_create(thread_sync, timeline);
}

void
drm_timeline_fini(struct drm_timeline *timeline)
{
   /* signal thread_sync to shutdown: */
   mtx_lock(&timeline->fence_mutex);
   timeline->stop_sync_thread = true;
   cnd_signal(&timeline->fence_cond);
   mtx_unlock(&timeline->fence_mutex);

   /* wait for thread_sync to exit: */
   thrd_join(timeline->sync_thread, NULL);

   if (timeline->last_fence_fd != -1)
      close(timeline->last_fence_fd);

   /* cleanup remaining fences: */
   list_for_each_entry_safe (struct drm_fence, fence, &timeline->pending_fences, node) {
      drm_fence_destroy(fence);
   }

   cnd_destroy(&timeline->fence_cond);
   mtx_destroy(&timeline->fence_mutex);
}

int
drm_timeline_submit_fence(struct drm_timeline *timeline, uint32_t flags,
                          uint64_t fence_id)
{
   if (timeline->last_fence_fd == -1)
      return -EINVAL;

   struct drm_fence *fence =
      drm_fence_create(timeline->last_fence_fd, flags, fence_id);

   if (!fence)
      return -ENOMEM;

   drm_dbg("fence: %p (%" PRIu64 ")", (void*)fence, fence->fence_id);

   virgl_fence_set_fd(fence_id, fence->fd);

   mtx_lock(&timeline->fence_mutex);
   list_addtail(&fence->node, &timeline->pending_fences);
   cnd_signal(&timeline->fence_cond);
   mtx_unlock(&timeline->fence_mutex);

   close(timeline->last_fence_fd);
   timeline->last_fence_fd = -1;

   return 0;
}

/* takes ownership of the fd */
void
drm_timeline_set_last_fence_fd(struct drm_timeline *timeline, int fd)
{
   if (timeline->last_fence_fd != -1)
      close(timeline->last_fence_fd);
   timeline->last_fence_fd = fd;
}
