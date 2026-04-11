/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#include <xf86drm.h>
#include <fcntl.h>
#include <errno.h>

#include "util/anon_file.h"

#include "panfrost_ccmd.h"
#include "panfrost_object.h"
#include "panfrost_resource.h"
#include "drm_util.h"


void
panfrost_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, res->res_id);

   if (!dobj) {
      int fd;
      enum virgl_resource_fd_type fd_type = virgl_resource_export_fd(res, &fd);

      /* If importing a dmabuf resource created by another context (or
       * externally), then import it to create a gem obj handle in our
       * context:
       */
      if (fd_type == VIRGL_RESOURCE_FD_DMABUF) {
         uint32_t handle;
         int ret;

         ret = drmPrimeFDToHandle(dctx->fd, fd, &handle);
         if (ret) {
            drm_err("Could not import: %s", strerror(errno));
            close(fd);
            return;
         }

         /* lseek() to get bo size */
         off_t size = lseek(fd, 0, SEEK_END);
         close(fd);
         if (size < 0) {
            drm_err("lseek failed: %" PRId64 " (%s)", size, strerror(errno));
            drmCloseBufferHandle(fd, handle);
            return;
         }

         struct panfrost_object *obj = panfrost_object_create(handle, size, 0, 0);
         if (!obj) {
            drmCloseBufferHandle(fd, handle);
            return;
         }

         dobj = &obj->base;

         drm_context_object_set_res_id(dctx, dobj, res->res_id);

         drm_dbg("obj=%p, res_id=%u, handle=%u", (void*)obj, dobj->res_id, dobj->handle);
      } else {
         if (fd_type != VIRGL_RESOURCE_FD_INVALID) {
            assert(fd_type == VIRGL_RESOURCE_OPAQUE_HANDLE ||
                   fd_type == VIRGL_RESOURCE_FD_SHM);
            close(fd);
         }
         return;
      }
   }
}

enum virgl_resource_fd_type
panfrost_renderer_export_opaque_handle(struct virgl_context *vctx,
                                       struct virgl_resource *res,
                                       int *out_fd)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, res->res_id);
   int ret;

   drm_dbg("obj=%p, res_id=%u", (void*)dobj, res->res_id);

   if (!dobj) {
      drm_err("invalid res_id %u", res->res_id);
      return VIRGL_RESOURCE_FD_INVALID;
   }

   ret = drmPrimeHandleToFD(dctx->fd, dobj->handle, DRM_CLOEXEC | DRM_RDWR, out_fd);
   if (ret) {
      drm_err("failed to get dmabuf fd: %s", strerror(errno));
      return VIRGL_RESOURCE_FD_INVALID;
   }

   return VIRGL_RESOURCE_FD_DMABUF;
}

int
panfrost_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                           uint64_t blob_size, uint32_t blob_flags,
                           struct virgl_context_blob *blob)
{
   struct drm_context *dctx = to_drm_context(vctx);

   if ((blob_id >> 32) != 0) {
      drm_err("invalid blob_id: %" PRIu64, blob_id);
      return -EINVAL;
   }

   /* blob_id of zero is reserved for the shmem buffer: */
   if (blob_id == 0)
      return drm_context_get_shmem_blob(dctx, "panfrost-shmem", sizeof(*dctx->shmem),
                                        blob_size, blob_flags, blob);

   if (!drm_context_res_id_unused(dctx, res_id)) {
      drm_err("res_id %u already in use", res_id);
      return -EINVAL;
   }

   struct drm_object *dobj = drm_context_retrieve_object_from_blob_id(dctx, blob_id);

   /* If GEM_NEW fails, we can end up here without a backing obj: */
   if (!dobj) {
      drm_err("no object");
      return -ENOENT;
   }

   if (dobj->size != blob_size) {
      drm_err("Invalid blob size");
      return -EINVAL;
   }

   if (blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE) {
      int fd, ret;

      ret = drmPrimeHandleToFD(dctx->fd, dobj->handle, DRM_CLOEXEC | DRM_RDWR, &fd);
      if (ret) {
         drm_err("export to fd failed");
         return -EINVAL;
      }

      blob->type = VIRGL_RESOURCE_FD_DMABUF;
      blob->u.fd = fd;
   } else {
      blob->type = VIRGL_RESOURCE_OPAQUE_HANDLE;
      blob->u.opaque_handle = dobj->handle;
   }

   blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;

   drm_context_object_set_res_id(dctx, dobj, res_id);

   return 0;
}
