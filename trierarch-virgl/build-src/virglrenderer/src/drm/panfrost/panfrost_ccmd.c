/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <xf86drm.h>
#include <errno.h>

#include "drm_context.h"
#include "drm_util.h"

#include "panfrost_object.h"
#include "panfrost_proto.h"
#include "panfrost_drm.h"

static int
panfrost_ccmd_submit(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_submit_req *req = to_panfrost_ccmd_submit_req(hdr);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);
   uint32_t *res = (uint32_t *) &req->payload;
   uint32_t bo_handlers[req->res_id_count];
   int ret, out_sync_fd, in_fence_fd;
   uint32_t in_sync = 0;

   for (size_t i = 0; i < req->res_id_count; i++) {
      struct panfrost_object *pan_obj = panfrost_get_object_from_res_id(pan_ctx, res[i]);
      if (!pan_obj) {
         drm_err("invalid blob_id %u", res[i]);
         return 0;
      };
      bo_handlers[i] = pan_obj->base.handle;
   }

   in_fence_fd = virgl_context_take_in_fence_fd(&dctx->base);
   if (in_fence_fd >= 0) {
      ret = drmSyncobjCreate(dctx->fd, 0, &in_sync);
      if (ret)
         return ret;

      ret = drmSyncobjImportSyncFile(dctx->fd, in_sync, in_fence_fd);
      if (ret)
         return ret;

      close(in_fence_fd);
   }

   struct drm_panfrost_submit submit = {
      .jc = req->jc,
      .in_syncs = (__u64)(char *) &in_sync,
      .in_sync_count = !!in_sync,
      .out_sync = pan_ctx->out_sync,
      .bo_handles = (uint64_t) bo_handlers,
      .bo_handle_count = req->res_id_count,
      .requirements = req->requirements,
   };

   drm_dbg("jc=%llu, out_sync=%u, requirements=0x%X, bo_handle_count=%u",
           submit.jc, submit.out_sync, submit.requirements, submit.bo_handle_count);

   ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_SUBMIT, &submit);
   if (ret)
      return 0;

   ret = drmSyncobjExportSyncFile(dctx->fd, pan_ctx->out_sync, &out_sync_fd);
   if (ret)
      return 0;

   drm_timeline_set_last_fence_fd(&pan_ctx->timeline, out_sync_fd);

   return 0;
}

static int
panfrost_ccmd_wait_bo(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_wait_bo_req *req = to_panfrost_ccmd_wait_bo_req(hdr);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   struct panfrost_object *pan_obj = panfrost_get_object_from_res_id(pan_ctx, req->res_id);
   if (!pan_obj) {
      drm_err("invalid res_id %u", req->res_id);
      return 0;
   }

   struct panfrost_ccmd_wait_bo_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_panfrost_wait_bo wait_bo = {
      .handle = pan_obj->base.handle,
      .timeout_ns = req->timeout_ns,
   };

   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_WAIT_BO, &wait_bo);

   return 0;
}

static int
panfrost_ccmd_create_bo(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_create_bo_req *req = to_panfrost_ccmd_create_bo_req(hdr);
   struct panfrost_object *pan_obj;
   struct drm_panfrost_create_bo create_bo = {
      .size = req->size,
      .flags = req->flags,
   };

   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      drm_err("Invalid blob_id %u", req->blob_id);
      return -EINVAL;
   }

   drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_CREATE_BO, &create_bo);

   pan_obj = panfrost_object_create(create_bo.handle, req->size, create_bo.offset, req->flags);
   if (!pan_obj)
      return -ENOMEM;

   drm_context_object_set_blob_id(dctx, &pan_obj->base, req->blob_id);

   drm_dbg("obj=%p, blob_id=%u, handle=%u, size=%u, offset=%llu, flags=0x%X",
           (void*)pan_obj, pan_obj->base.blob_id, pan_obj->base.handle, req->size,
           create_bo.offset, pan_obj->flags);

   return 0;
}

static int
panfrost_ccmd_mmap_bo(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_mmap_bo_req *req = to_panfrost_ccmd_mmap_bo_req(hdr);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   struct panfrost_object *pan_obj = panfrost_get_object_from_res_id(pan_ctx, req->res_id);
   if (!pan_obj) {
      drm_err("invalid res_id %u", req->res_id);
      return 0;
   }

   struct panfrost_ccmd_mmap_bo_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_panfrost_mmap_bo mmap_bo = {
      .handle = pan_obj->base.handle,
      .flags = req->flags,
   };

   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_MMAP_BO, &mmap_bo);
   rsp->offset = mmap_bo.offset;

   drm_dbg("obj=%p, res_id=%u, handle=%u, bo.offset=%lu, mmap.offset=%lu, flags=0x%X",
           (void*)pan_obj, pan_obj->base.res_id, pan_obj->base.handle,
           pan_obj->offset, rsp->offset, req->flags);

   return 0;
}

static int
panfrost_ccmd_get_param(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_get_param_req *req = to_panfrost_ccmd_get_param_req(hdr);
   struct drm_panfrost_get_param param = { .param = req->param };

   struct panfrost_ccmd_get_param_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_GET_PARAM, &param);
   rsp->value = param.value;

   return 0;
}

static int
panfrost_ccmd_get_bo_offset(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_get_bo_offset_req *req = to_panfrost_ccmd_get_bo_offset_req(hdr);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   struct panfrost_object *pan_obj = panfrost_get_object_from_res_id(pan_ctx, req->res_id);
   if (!pan_obj) {
      drm_err("invalid res_id %u", req->res_id);
      return 0;
   }

   struct panfrost_ccmd_get_bo_offset_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_panfrost_get_bo_offset bo_offset = { .handle = pan_obj->base.handle };

   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_GET_BO_OFFSET, &bo_offset);
   pan_obj->offset = rsp->offset = bo_offset.offset;

   drm_dbg("obj=%p, res_id=%u, handle=%u, offset=%lu", (void*)pan_obj,
           pan_obj->base.res_id, pan_obj->base.handle, rsp->offset);

   return 0;
}

static int
panfrost_ccmd_madvise(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct panfrost_ccmd_madvise_req *req = to_panfrost_ccmd_madvise_req(hdr);
   struct panfrost_context *pan_ctx = to_panfrost_context(dctx);

   struct panfrost_object *pan_obj = panfrost_get_object_from_res_id(pan_ctx, req->res_id);
   if (!pan_obj) {
      drm_err("invalid res_id %u", req->res_id);
      return 0;
   };

   struct panfrost_ccmd_madvise_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_panfrost_madvise madvise = {
      .handle = pan_obj->base.handle,
      .madv = req->madv,
   };

   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_PANFROST_MADVISE, &madvise);
   rsp->retained = madvise.retained;

   return 0;
}

const struct drm_ccmd panfrost_ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [PANFROST_CCMD_##N] = {#N, panfrost_ccmd_##n, sizeof(struct panfrost_ccmd_##n##_req)}
   HANDLER(SUBMIT, submit),
   HANDLER(WAIT_BO, wait_bo),
   HANDLER(CREATE_BO, create_bo),
   HANDLER(MMAP_BO, mmap_bo),
   HANDLER(GET_PARAM, get_param),
   HANDLER(GET_BO_OFFSET, get_bo_offset),
   HANDLER(MADVISE, madvise),
};

const unsigned int panfrost_ccmd_dispatch_size = ARRAY_SIZE(panfrost_ccmd_dispatch);
