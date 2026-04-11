/*
 * Copyright 2024 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "amdgpu_renderer.h"

#include "pipe/p_state.h"
#include "util/anon_file.h"
#include "util/bitscan.h"
#include "util/hash_table.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/u_atomic.h"
#include "util/u_math.h"
#include "vrend/vrend_renderer.h"
#include "amdgpu_virtio_proto.h"
#include "virgl_context.h"
#include "virglrenderer.h"

#include <stdalign.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>

#include <amdgpu.h>
#include "drm_context.h"
#include "drm_util.h"
#include "drm_fence.h"

#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

#if 0
#define print(level, fmt, ...) do {       \
   if (ctx->debug >= level) { \
      unsigned c = (unsigned)((uintptr_t)ctx >> 8) % 256; \
      printf("\033[0;38;5;%dm", c);     \
      printf("[%d|%s] %s: " fmt "\n", ctx->base.base.ctx_id, ctx->debug_name, __FUNCTION__, ##__VA_ARGS__); \
      printf("\033[0m");     \
   } \
 } while (false)
#else
#define print(log_level, fmt, ...) do {       \
   if (log_level == 0) \
      drm_err("[%d|%s]: " fmt, ctx->base.base.ctx_id, ctx->debug_name, ##__VA_ARGS__); \
   else if (log_level == 1) \
      drm_log("[%d|%s]: " fmt, ctx->base.base.ctx_id, ctx->debug_name, ##__VA_ARGS__); \
   else \
      drm_dbg("[%d|%s]: " fmt, ctx->base.base.ctx_id, ctx->debug_name, ##__VA_ARGS__); \
 } while (false)
#endif

struct amdgpu_context {
   struct drm_context base;

   const char *debug_name;

   struct amdvgpu_shmem *shmem;

   struct amdgpu_ccmd_rsp *current_rsp;

   amdgpu_device_handle dev;
   int debug;

   struct hash_table_u64 *id_to_ctx;

   uint32_t timeline_count;
   struct drm_timeline timelines[];
};
DEFINE_CAST(drm_context, amdgpu_context)

static
int close_fd(struct amdgpu_context *ctx, int fd, const char *from) {
   print(2, "close_fd %d (%s)", fd, from);
   return close(fd);
}

int
amdgpu_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset)
{
   amdgpu_device_handle dev;
   uint32_t drm_major, drm_minor;
   int r;

   r = amdgpu_device_initialize2(fd, false, &drm_major, &drm_minor, &dev);
   if (r)
      return -ENOTSUP;
   amdgpu_query_sw_info(dev, amdgpu_sw_info_address32_hi,
                        &capset->u.amdgpu.address32_hi);
   amdgpu_query_buffer_size_alignment(dev,
                                      &capset->u.amdgpu.alignments);
   amdgpu_query_gpu_info(dev,
                         &capset->u.amdgpu.gpu_info);
   strncpy(capset->u.amdgpu.marketing_name,
           amdgpu_get_marketing_name(dev),
           sizeof(capset->u.amdgpu.marketing_name) - 1);
   capset->u.amdgpu.has_vm_always_valid = 1;

   amdgpu_device_deinitialize(dev);

   return 0;
}

/* Imported objects will use this blob id. */
#define UNKOWN_BLOB_ID 0xffffffff

struct amdgpu_object {
   struct drm_object base;
   /* amdgpu_drm handle to the object. */
   amdgpu_bo_handle bo;

   bool has_metadata    :1;
   bool exported        :1;
   bool enable_cache_wc :1;
};
DEFINE_CAST(drm_object, amdgpu_object)

static void free_id_to_ctx(struct hash_entry *entry)
{
   amdgpu_cs_ctx_free(entry->data);
}

static void
amdgpu_renderer_destroy(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);

   for (unsigned i = 0; i < ctx->timeline_count; i++) {
      drm_timeline_fini(&ctx->timelines[i]);
      free((char*)ctx->timelines[i].name);
   }

   drm_context_deinit(&ctx->base);

   if (ctx->id_to_ctx)
      _mesa_hash_table_u64_destroy(ctx->id_to_ctx, free_id_to_ctx);

   amdgpu_device_deinitialize(ctx->dev);

   free((void*)ctx->debug_name);
   memset(ctx, 0, sizeof(struct amdgpu_context));
   free(ctx);
}

static struct amdgpu_object *
amdgpu_object_create(amdgpu_bo_handle handle, uint64_t size)
{
   struct amdgpu_object *obj = calloc(1, sizeof(*obj));

   if (!obj)
      return NULL;

   obj->base.blob_id = UNKOWN_BLOB_ID;
   obj->base.size = size;
   obj->bo = handle;

   return obj;
}

static struct amdgpu_object *
amdgpu_retrieve_object_from_blob_id(struct amdgpu_context *ctx, uint64_t blob_id)
{
   struct drm_object *dobj = drm_context_retrieve_object_from_blob_id(&ctx->base, blob_id);
   if (!dobj)
      return NULL;

   return to_amdgpu_object(dobj);
}

static struct amdgpu_object *
amdgpu_get_object_from_res_id(struct amdgpu_context *ctx, uint32_t res_id, const char *from)
{
   struct drm_object *dobj = drm_context_get_object_from_res_id(&ctx->base, res_id);
   if (likely(dobj)) {
      return to_amdgpu_object(dobj);
   } else {
      if (from) {
         print(0, "Couldn't find res_id: %u [%s]", res_id, from);
         hash_table_foreach (ctx->base.resource_table, entry) {
            struct amdgpu_object *o = entry->data;
            print(1, "  * blob_id: %u res_id: %u", o->base.blob_id, o->base.res_id);
         }
      }
      return NULL;
   }
}

static void
amdgpu_object_set_res_id(struct amdgpu_context *ctx, struct amdgpu_object *obj,
                         uint32_t res_id)
{
   drm_context_object_set_res_id(&ctx->base, &obj->base, res_id);
   print(2, "blob_id=%u, res_id: %u", obj->base.blob_id, obj->base.res_id);
}

static void
amdgpu_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, NULL);

   if (!obj) {
      if (res->fd_type == VIRGL_RESOURCE_FD_SHM) {
         return;
      }

      int fd;
      enum virgl_resource_fd_type fd_type = virgl_resource_export_fd(res, &fd);
      if (fd_type == VIRGL_RESOURCE_FD_DMABUF) {
         struct amdgpu_bo_info info = {0};
         struct amdgpu_bo_import_result import;
         int ret;

         ret = amdgpu_bo_import(ctx->dev, amdgpu_bo_handle_type_dma_buf_fd, fd,
                                &import);

         close_fd(ctx, fd, __FUNCTION__);
         if (ret) {
            print(0, "Could not import fd=%d: %s", fd, strerror(errno));
            return;
         }

         ret = amdgpu_bo_query_info(import.buf_handle, &info);
         if (ret) {
            print(0, "amdgpu_bo_query_info failed\n");
            return;
         }

         obj = amdgpu_object_create(import.buf_handle, import.alloc_size);
         if (!obj)
            return;

         obj->bo = import.buf_handle;
         amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_kms, &obj->base.handle);
         amdgpu_object_set_res_id(ctx, obj, res->res_id);
         print(1, "imported dmabuf -> res_id=%u" PRIx64, res->res_id);
      } else {
         print(2, "Ignored res_id: %d (fd_type = %d)", res->res_id, fd_type);
         if (fd_type != VIRGL_RESOURCE_FD_INVALID)
            close_fd(ctx, fd, __FUNCTION__);
         return;
      }
   }
}

static void
amdgpu_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_object *obj = to_amdgpu_object(dobj);

   print(2, "free obj res_id: %d", dobj->res_id);

   amdgpu_bo_free(obj->bo);
   free(obj);
}

static enum virgl_resource_fd_type
amdgpu_renderer_export_opaque_handle(struct virgl_context *vctx,
                                     struct virgl_resource *res, int *out_fd)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, __FUNCTION__);
   int ret;

   print(2, "obj=%p, res_id=%u", (void *)obj, res->res_id);

   if (!obj) {
      print(0, "invalid res_id %u", res->res_id);
      return VIRGL_RESOURCE_FD_INVALID;
   }

   ret = amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_dma_buf_fd, (uint32_t *)out_fd);

   if (ret) {
      print(0, "failed to get dmabuf fd: %s", strerror(errno));
      return VIRGL_RESOURCE_FD_INVALID;
   }

   char dmabufname[32] = { 0 };
   snprintf(dmabufname, sizeof(dmabufname) - 1, "e:%d-%s",
            res->res_id, ctx->debug_name);
   set_dmabuf_name(*out_fd, dmabufname);

   if (res->fd_type == VIRGL_RESOURCE_OPAQUE_HANDLE && obj->has_metadata) {
      /* Interpret set_metadata as lazy VIRTGPU_BLOB_FLAG_USE_SHAREABLE. */
      res->fd = os_dupfd_cloexec(*out_fd);
      res->fd_type = VIRGL_RESOURCE_FD_DMABUF;
      print(2, "res_id: %d became VIRGL_RESOURCE_FD_DMABUF", res->res_id);
   } else {
      print(3, "res_id: %d one time export", res->res_id);
   }

   return VIRGL_RESOURCE_FD_DMABUF;
}

static void * amdgpu_renderer_resource_map(struct virgl_context *vctx,
                                           struct virgl_resource *res,
                                           void *addr,
                                           int32_t prot,
                                           int32_t flags)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, __FUNCTION__);
   union drm_amdgpu_gem_mmap args = { 0 };
   int r;

   print(2, "obj=%p, res_id=%u", (void *)obj, res->res_id);

   if (!obj) {
      print(0, "invalid res_id %u", res->res_id);
      return NULL;
   }

   args.in.handle = obj->base.handle;

   r = drmCommandWriteRead(amdgpu_device_get_fd(ctx->dev), DRM_AMDGPU_GEM_MMAP, &args,
                           sizeof(args));

   if (r) {
      print(0, "DRM_AMDGPU_GEM_MMAP failed with r=%d", r);
      return NULL;
   }

   return mmap(addr, obj->base.size, prot, flags,
               amdgpu_device_get_fd(ctx->dev), args.out.addr_ptr);
}

static void
update_heap_info_in_shmem(struct amdgpu_context *ctx)
{
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_VRAM,
                       AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
                       &ctx->shmem->vis_vram);
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_VRAM,
                          0,
                          &ctx->shmem->vram);
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_GTT,
                          0,
                          &ctx->shmem->gtt);
}

static int
amdgpu_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                         uint64_t blob_size, uint32_t blob_flags,
                         struct virgl_context_blob *blob)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);

   print(2, "blob_id=%" PRIu64 ", res_id=%u, blob_size=%" PRIu64
         ", blob_flags=0x%x",
           blob_id, res_id, blob_size, blob_flags);

   if ((blob_id >> 32) != 0) {
      print(0, "invalid blob_id: %" PRIu64, blob_id);
      return -EINVAL;
   }

   /* blob_id of zero is reserved for the shmem buffer: */
   if (blob_id == 0) {
      char name[64];
      snprintf(name, 64, "amdgpu-shmem-%s", ctx->debug_name);

      int ret = drm_context_get_shmem_blob(dctx, name, sizeof(*ctx->shmem),
                                           blob_size, blob_flags, blob);
      if (ret)
         return ret;

      ctx->shmem = to_amdvgpu_shmem(dctx->shmem);

      update_heap_info_in_shmem(ctx);

      return 0;
   }

   if (!drm_context_res_id_unused(dctx, res_id)) {
      print(0, "Invalid res_id %u", res_id);
      return -EINVAL;
   }

   struct amdgpu_object *obj = amdgpu_retrieve_object_from_blob_id(ctx, blob_id);

   /* If GEM_NEW fails, we can end up here without a backing obj or if it's a dumb buffer. */
   if (!obj) {
      print(0, "No object with blob_id=%ld", blob_id);
      return -ENOENT;
   }

   if (obj->enable_cache_wc)
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
   else
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;

   /* a memory can only be exported once; we don't want two resources to point
    * to the same storage.
    */
   if (obj->exported) {
      print(0, "Already exported! blob_id:%ld", blob_id);
      return -EINVAL;
   }

   amdgpu_object_set_res_id(ctx, obj, res_id);

   if (blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE) {
      int fd, ret;

      ret = amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_dma_buf_fd, (uint32_t *)&fd);

      if (ret) {
         print(0, "Export to fd failed for blob_id:%ld r=%d (%s)", blob_id, ret, strerror(errno));
         return ret;
      }

      char dmabufname[32] = { 0 };
      snprintf(dmabufname, sizeof(dmabufname) - 1, "r:%d-%s",
               res_id, ctx->debug_name);
      set_dmabuf_name(fd, dmabufname);

      print(2, "dmabuf created: %d for res_id: %d", fd, res_id);

      blob->type = VIRGL_RESOURCE_FD_DMABUF;
      blob->u.fd = fd;
   } else {
      blob->type = VIRGL_RESOURCE_OPAQUE_HANDLE;
      blob->u.opaque_handle = obj->base.handle;
   }

   obj->exported = true;

   /* Update usage (should probably be done on alloc/import instead). */
   update_heap_info_in_shmem(ctx);

   return 0;
}

static int
amdgpu_ccmd_query_info(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_query_info_req *req = to_amdgpu_ccmd_query_info_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_ccmd_query_info_rsp *rsp;
   unsigned rsp_len;
   if (__builtin_add_overflow(sizeof(*rsp), req->info.return_size, &rsp_len)) {
      print(1, "%s: Request size overflow: %zu + %" PRIu32 " > %u",
            __FUNCTION__, sizeof(*rsp), req->info.return_size, UINT_MAX);
      return -EINVAL;
   }

   rsp = drm_context_rsp(dctx, hdr, rsp_len);

   if (!rsp)
      return -ENOMEM;

   size_t return_size = req->info.return_size;
   void *value = calloc(return_size, 1);
   struct drm_amdgpu_info request;
   memcpy(&request, &req->info, sizeof(request));
   request.return_pointer = (uintptr_t)value;

   int r = drmCommandWrite(amdgpu_device_get_fd(ctx->dev), DRM_AMDGPU_INFO, &request, sizeof(request));

   rsp->hdr.ret = r;

   if (rsp->hdr.ret < 0 && request.query != AMDGPU_INFO_HW_IP_INFO)
      print(request.query <= AMDGPU_INFO_RAS_ENABLED_FEATURES ? 0 : 2,
            "ioctl error: fd: %d request.query: 0x%x r: %d|%d %s",
            amdgpu_device_get_fd(ctx->dev), request.query, rsp->hdr.ret, r, strerror(errno));

   memcpy(rsp->payload, value, req->info.return_size);
   free(value);

   return 0;
}

static int
amdgpu_ccmd_gem_new(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_gem_new_req *req = to_amdgpu_ccmd_gem_new_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   int ret = 0;

   if (req->r.__pad) {
      print(0, "Invalid value for struct %s_req::r::__pad: "
            "0x%" PRIx32, __FUNCTION__, req->r.__pad);
      ret = -EINVAL;
      goto alloc_failed;
   }
   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      print(0, "Invalid blob_id %ld", req->blob_id);
      ret = -EINVAL;
      goto alloc_failed;
   }

   struct amdgpu_bo_alloc_request r = {
      .alloc_size = req->r.alloc_size,
      .phys_alignment = req->r.phys_alignment,
      .preferred_heap = req->r.preferred_heap,
      .flags = req->r.flags,
   };
   amdgpu_bo_handle bo_handle;
   ret = amdgpu_bo_alloc(ctx->dev, &r, &bo_handle);

   if (ret) {
      print(0, "amdgpu_bo_alloc failed: %d (%s)", ret, strerror(errno));
      goto alloc_failed;
   }

   uint32_t gem_handle;
   ret = amdgpu_bo_export(bo_handle, amdgpu_bo_handle_type_kms, &gem_handle);
   if (ret) {
      print(0, "Failed to get kms handle");
      goto va_map_failed;
   }

   struct amdgpu_object *obj = amdgpu_object_create(bo_handle, req->r.alloc_size);
   if (obj == NULL)
      goto va_map_failed;

   obj->base.handle = gem_handle;
   /* Enable Write-Combine except for GTT buffers with WC disabled. */
   obj->enable_cache_wc =
      (req->r.preferred_heap != AMDGPU_GEM_DOMAIN_GTT) ||
      (req->r.flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC);

   drm_context_object_set_blob_id(dctx, &obj->base, req->blob_id);

   print(2, "new object blob_id: %ld heap: %08x flags: %lx size: %ld",
         req->blob_id, req->r.preferred_heap, req->r.flags, req->r.alloc_size);

   return 0;

va_map_failed:
   amdgpu_bo_free(bo_handle);

alloc_failed:
   print(2, "ERROR blob_id: %ld heap: %08x flags: %lx",
         req->blob_id, req->r.preferred_heap, req->r.flags);
   if (ctx->shmem)
      ctx->shmem->async_error++;
   return ret;
}

static int
amdgpu_ccmd_bo_va_op(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_bo_va_op_req *req = to_amdgpu_ccmd_bo_va_op_req(hdr);
   struct amdgpu_object *obj;
   struct amdgpu_ccmd_rsp *rsp;
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   if (req->flags2 & ~AMDGPU_CCMD_BO_VA_OP_SPARSE_BO) {
      print(0, "Forbidden flags 0x%" PRIx64 " set in flags", req->flags);
      rsp->ret = -EINVAL;
      return -1;
   }

   if (req->flags2 & AMDGPU_CCMD_BO_VA_OP_SPARSE_BO) {
      obj = NULL;
   } else {
      obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
      if (!obj) {
         print(0, "amdgpu_bo_va_op_raw failed: op: %d res_id: %d offset: 0x%lx size: 0x%lx va: %" PRIx64 " r=%d",
            req->op, obj->base.res_id, req->offset, req->vm_map_size, req->va, rsp->ret);

         /* This is ok. This means the guest closed the GEM already. */
         return -EINVAL;
      }
   }

   rsp->ret = amdgpu_bo_va_op_raw(
      ctx->dev, obj ? obj->bo : NULL, req->offset, req->vm_map_size, req->va,
      req->flags,
      req->op);
   if (rsp->ret) {
      if (ctx->shmem)
         ctx->shmem->async_error++;

      print(0, "amdgpu_bo_va_op_raw failed: op: %d res_id: %d offset: 0x%lx size: 0x%lx va: %" PRIx64 " r=%d",
         req->op, req->res_id, req->offset, req->vm_map_size, req->va, rsp->ret);
   } else {
      print(2, "va_op %d res_id: %u va: [0x%" PRIx64 ", 0x%" PRIx64 "] @offset 0x%" PRIx64,
            req->op, req->res_id, req->va, req->va + req->vm_map_size - 1, req->offset);
   }

   return 0;
}

static int
amdgpu_ccmd_set_metadata(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_set_metadata_req *req = to_amdgpu_ccmd_set_metadata_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_ccmd_rsp *rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
   if (!obj) {
      print(0, "Cannot find object with res_id=%d",
              req->res_id);
      rsp->ret = -EINVAL;
      return -1;
   }

   /* We could also store the metadata here instead of passing them to the host kernel =>
    * actually no because this would only work if the desktop runs on radeonsi-virtio.
    */
   struct amdgpu_bo_metadata metadata = {0};
   metadata.flags = req->flags;
   metadata.tiling_info = req->tiling_info;
   metadata.size_metadata = req->size_metadata;
   if (req->size_metadata) {
      if (req->size_metadata > sizeof(metadata.umd_metadata)) {
         print(0, "Metadata size is too large for target buffer: %" PRIu32 " > %zu",
               req->size_metadata, sizeof(metadata.umd_metadata));
         rsp->ret = -EINVAL;
         return -1;
      }
      size_t requested_size = size_add(req->size_metadata,
                                       offsetof(struct amdgpu_ccmd_set_metadata_req,
                                                umd_metadata));
      if (requested_size > hdr->len) {
         print(0, "Metadata size is too large for source buffer: %zu > %" PRIu32,
               requested_size, hdr->len);
         rsp->ret = -EINVAL;
         return -1;
      }
      memcpy(metadata.umd_metadata, req->umd_metadata, req->size_metadata);
   }

   rsp->ret = amdgpu_bo_set_metadata(obj->bo, &metadata);
   if (rsp->ret) {
      print(0, "amdgpu_bo_set_metadata failed for res: %d", req->res_id);
   }

   obj->has_metadata = true;

   return 0;
}

static int
amdgpu_ccmd_bo_query_info(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_bo_query_info_req *req =
      to_amdgpu_ccmd_bo_query_info_req(hdr);
   struct amdgpu_ccmd_bo_query_info_rsp *rsp;
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_bo_query_info_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   if (req->pad != 0) {
      print(0, "Padding not zeroed");
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   /* NOTE: Current implementation of KMS support is incomplete and may result in
    * guest passing vrend dumb buffer resource to native context. In this case, native
    * context should error out the offending resource, but continue execution.
    */
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
   if (!obj) {
      print(0, "Cannot find object");
      rsp->hdr.ret = -EINVAL;
      return 0;
   }

   struct amdgpu_bo_info info = {0};
   rsp->hdr.ret = amdgpu_bo_query_info(obj->bo, &info);
   if (rsp->hdr.ret) {
      print(0, "amdgpu_bo_query_info failed");
      return 0;
   }

   rsp->info.alloc_size = info.alloc_size;
   rsp->info.phys_alignment = info.phys_alignment;
   rsp->info.preferred_heap = info.preferred_heap;
   rsp->info.alloc_flags = info.alloc_flags;
   rsp->info.metadata.flags = info.metadata.flags;
   rsp->info.metadata.tiling_info = info.metadata.tiling_info;
   rsp->info.metadata.size_metadata = info.metadata.size_metadata;
   memcpy(rsp->info.metadata.umd_metadata, info.metadata.umd_metadata,
          info.metadata.size_metadata);

   return 0;
}

static int
amdgpu_ccmd_create_ctx(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_create_ctx_req *req = to_amdgpu_ccmd_create_ctx_req(hdr);
   struct amdgpu_ccmd_create_ctx_rsp *rsp;
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_create_ctx_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   if (req->flags & ~AMDGPU_CCMD_CREATE_CTX_DESTROY) {
      print(0, "Invalid flags 0x%" PRIu32, req->flags);
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   if (!(req->flags & AMDGPU_CCMD_CREATE_CTX_DESTROY)) {
      amdgpu_context_handle ctx_handle;

      int r = amdgpu_cs_ctx_create2(ctx->dev, req->priority, &ctx_handle);
      rsp->hdr.ret = r;
      if (r) {
         print(0, "amdgpu_cs_ctx_create2(prio=%d) failed (%s)", req->priority, strerror(errno));
         return 0;
      }

      print(1, "amdgpu_cs_ctx_create2 dev: %p -> %p", (void*)ctx->dev, (void*)ctx_handle);

      if (!rsp)
         return -ENOMEM;

      /* We need the ctx_id in the guest */
      struct amdgpu_cs_fence f = {
         .context = ctx_handle
      };
      struct drm_amdgpu_cs_chunk_dep d = { 0 };
      amdgpu_cs_chunk_fence_to_dep(&f, &d);
      rsp->ctx_id = d.ctx_id;

      _mesa_hash_table_u64_insert(ctx->id_to_ctx, d.ctx_id,
                                  (void*)(uintptr_t)ctx_handle);

   } else {
      amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx, req->id);
      if (actx == NULL) {
         print(0, "Failed to find ctx_id: %d", req->id);
      } else {
         amdgpu_cs_ctx_free(actx);
         rsp->hdr.ret = 0;
        _mesa_hash_table_u64_remove(ctx->id_to_ctx, req->id);
      }

      print(1, "amdgpu_cs_ctx_free dev: %p -> %p", (void*)ctx->dev, (void*)actx);
   }

   return 0;
}

/* Check that 'offset + len' fits in buffer of size 'max_len', that
 * 'len' is correct for 'count' objects of size 'size',
 * and that 'offset' is aligned to 'align'.  'len' is assumed to not
 * be SIZE_MAX, which is guaranteed at all call-sites.
 */
static bool validate_chunk_inputs(size_t offset, size_t len, struct amdgpu_context *ctx,
                                  size_t count, size_t size, size_t align)
{
   if (offset % align != 0) {
      print(0, "Offset 0x%zx is misaligned (needed 0x%zx)", offset, align);
      return false; /* misaligned */
   }
   size_t total_len = size_mul(size, count);
   if (total_len > len) {
      print(0, "Length 0x%zx cannot hold 0x%zx entries of size 0x%zx",
            len, count, size);
      return false;
   }
   return true;
}

static int
amdgpu_ccmd_cs_submit(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_cs_submit_req *req = to_amdgpu_ccmd_cs_submit_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct drm_amdgpu_bo_list_in bo_list_in = { 0 };
   struct drm_amdgpu_cs_chunk_fence user_fence;
   struct drm_amdgpu_cs_chunk_sem syncobj_in = { 0 };
   const struct drm_amdgpu_bo_list_entry *bo_handles_in = NULL;
   struct drm_amdgpu_bo_list_entry *bo_list = NULL;
   struct drm_amdgpu_cs_chunk *chunks;
   unsigned num_chunks = 0;
   uint64_t seqno = 0;
   int r;

   struct amdgpu_ccmd_rsp *rsp;
   rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }
   /* Do not allocate arbitrarily large buffer. */
   if (req->num_chunks > AMDGPU_CCMD_CS_SUBMIT_MAX_NUM_CHUNKS) {
      print(1, "%s: Invalid num_chunks: %" PRIu32 " > %d",
            __FUNCTION__, req->num_chunks, AMDGPU_CCMD_CS_SUBMIT_MAX_NUM_CHUNKS);
      rsp->ret = -EINVAL;
      return -1;
   }
   if (req->ring_idx == 0 || ctx->timeline_count < req->ring_idx) {
      print(0, "Invalid ring_idx value: %d (must be in [1, %d] range)",
         req->ring_idx,
         ctx->timeline_count);
      rsp->ret = -EINVAL;
      return -1;
   }
   chunks = malloc((req->num_chunks + 1 /* syncobj_in */ + 1 /* syncobj_out */) *
                   sizeof(*chunks));
   if (chunks == NULL) {
      print(0, "Failed to allocate %" PRIu32 " chunks", req->num_chunks + 2);
      r = -EINVAL;
      goto end;
   }

   amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx,
                                                            (uintptr_t)req->ctx_id);

   struct desc {
      uint16_t chunk_id;
      uint16_t length_dw;
      uint32_t offset;
   };
   size_t descriptors_len = size_add(offsetof(struct amdgpu_ccmd_cs_submit_req, payload),
                                     size_mul(req->num_chunks, sizeof(struct desc)));
   if (descriptors_len > hdr->len) {
      print(0, "Descriptors are out of bounds: %zu + %zu * %" PRIu32 " > %" PRIu32,
            offsetof(struct amdgpu_ccmd_cs_submit_req, payload),
            sizeof(struct desc), req->num_chunks, hdr->len);
      r = -EINVAL;
      goto end;
   }
   const struct desc *descriptors = (const void*) req->payload;

   for (size_t i = 0; i < req->num_chunks; i++) {
      unsigned chunk_id = descriptors[i].chunk_id;
      size_t offset = size_add(descriptors_len, descriptors[i].offset);
      size_t len = size_mul(descriptors[i].length_dw, 4);
      size_t end = size_add(offset, len);

      chunks[num_chunks].chunk_id = chunk_id;
      /* Validate input. */
      if (end > hdr->len) {
         print(0, "Descriptors are out of bounds: %zu > %" PRIu32, end, hdr->len);
         r = -EINVAL;
         goto end;
      }
      /* This macro must be used to validate the offset AND count.  Even if
       * the count is trusted, the offset must still be validated! */
#define validate_chunk_inputs(count, type) \
   validate_chunk_inputs(offset, len, ctx, count, sizeof(type), alignof(type))

      const void *input = (const char *)req + offset;

      if (chunk_id == AMDGPU_CHUNK_ID_BO_HANDLES) {
         uint32_t bo_count = len / sizeof(*bo_handles_in);
         if (!validate_chunk_inputs(bo_count, typeof(*bo_handles_in))) {
            r = -EINVAL;
            goto end;
         }

         if (bo_list != NULL) {
            print(0, "Refusing to allocate multiple BO lists");
            r = -EINVAL;
            goto end;
         }

         bo_handles_in = input;
         bo_list = malloc(bo_count * sizeof(struct drm_amdgpu_bo_list_entry));
         if (!bo_list) {
            print(0, "Unable to allocate %zu bytes for bo_list",
                  bo_count * sizeof(struct drm_amdgpu_bo_list_entry));
            r = -ENOMEM;
            goto end;
         }

         bo_list_in.operation = ~0;
         bo_list_in.list_handle = ~0;
         bo_list_in.bo_number = bo_count;
         bo_list_in.bo_info_size = sizeof(struct drm_amdgpu_bo_list_entry);
         bo_list_in.bo_info_ptr = (uint64_t)(uintptr_t)bo_list;

         for (uint32_t j = 0; j < bo_count; j++) {
            struct amdgpu_object *obj =
               amdgpu_get_object_from_res_id(ctx, bo_handles_in[j].bo_handle, __FUNCTION__);
            if (!obj) {
               print(0, "Couldn't retrieve bo with res_id %d", bo_handles_in[j].bo_handle);
               r = -EINVAL;
               goto end;
            }
            bo_list[j].bo_handle = obj->base.handle;
            bo_list[j].bo_priority = bo_handles_in[j].bo_priority;
         }

         chunks[num_chunks].length_dw = sizeof(bo_list_in) / 4;
         chunks[num_chunks].chunk_data = (uintptr_t)&bo_list_in;
      } else if (chunk_id == AMDGPU_CHUNK_ID_FENCE) {
         const struct drm_amdgpu_cs_chunk_fence *in;
         if (!validate_chunk_inputs(1, typeof(*in))) {
            r = -EINVAL;
            goto end;
         }
         in = input;
         if (in->offset % sizeof(uint64_t)) {
            print(0, "Invalid chunk offset %" PRIu32 " (not multiple of 8)", in->offset);
            r = -EINVAL;
            goto end;
         }
         struct amdgpu_object *obj =
               amdgpu_get_object_from_res_id(ctx, in->handle, __FUNCTION__);
         if (!obj) {
            print(0, "Couldn't retrieve user_fence bo with res_id %d", in->handle);
            r = -EINVAL;
            goto end;
         }
         struct amdgpu_cs_fence_info info;
         info.offset = in->offset / sizeof(int64_t);
         info.handle = obj->bo;
         amdgpu_cs_chunk_fence_info_to_data(&info, (void*) &user_fence);
         chunks[num_chunks].length_dw = sizeof(struct drm_amdgpu_cs_chunk_fence) / 4;
         chunks[num_chunks].chunk_data = (uintptr_t)&user_fence;
      } else if (chunk_id == AMDGPU_CHUNK_ID_DEPENDENCIES) {
         chunks[num_chunks].length_dw = descriptors[i].length_dw;
         chunks[num_chunks].chunk_data = (uintptr_t)input;
      } else if (chunk_id == AMDGPU_CHUNK_ID_IB) {
         chunks[num_chunks].length_dw = descriptors[i].length_dw;
         chunks[num_chunks].chunk_data = (uintptr_t)input;
         if (chunks[num_chunks].length_dw != sizeof(struct drm_amdgpu_cs_chunk_ib) / 4) {
            r = -EINVAL;
            goto end;
         }
      } else {
         print(0, "Unsupported chunk_id %d received", chunk_id);
         r = -EINVAL;
         goto end;
      }

      num_chunks++;
   }

   int in_fence_fd = virgl_context_take_in_fence_fd(&ctx->base.base);
   if (in_fence_fd >= 0) {
      chunks[num_chunks].chunk_id = AMDGPU_CHUNK_ID_SYNCOBJ_IN;
      chunks[num_chunks].length_dw = sizeof(syncobj_in) / 4;
      chunks[num_chunks].chunk_data = (uintptr_t)&syncobj_in;
      r = drmSyncobjCreate(amdgpu_device_get_fd(ctx->dev), 0, &syncobj_in.handle);
      if (r != 0) {
         print(0, "input syncobj creation failed");
         goto end;
      }
      r = drmSyncobjImportSyncFile(
         amdgpu_device_get_fd(ctx->dev), syncobj_in.handle, in_fence_fd);
      if (r == 0)
         num_chunks++;
   }

   struct drm_amdgpu_cs_chunk_sem syncobj_out = { 0 };
   chunks[num_chunks].chunk_id = AMDGPU_CHUNK_ID_SYNCOBJ_OUT;
   chunks[num_chunks].length_dw = sizeof(syncobj_out) / 4;
   chunks[num_chunks].chunk_data = (uintptr_t)&syncobj_out;
   r = drmSyncobjCreate(amdgpu_device_get_fd(ctx->dev), 0, &syncobj_out.handle);
   if (r != 0) {
      print(0, "out syncobj creation failed");
      goto end;
   }
   num_chunks++;

   r = amdgpu_cs_submit_raw2(ctx->dev, actx, 0, num_chunks, chunks, &seqno);

   if (in_fence_fd >= 0) {
      close(in_fence_fd);
      drmSyncobjDestroy(amdgpu_device_get_fd(ctx->dev), syncobj_in.handle);
   }

   if (r == 0) {
      int submit_fd;
      r = drmSyncobjExportSyncFile(amdgpu_device_get_fd(ctx->dev), syncobj_out.handle, &submit_fd);
      if (r == 0) {
         drm_timeline_set_last_fence_fd(&ctx->timelines[req->ring_idx - 1], submit_fd);
         print(3, "Set last fd ring_idx: %d: %d", req->ring_idx, submit_fd);
      } else {
         print(0, "Failed to create a FD from the syncobj (%d)", r);
      }
   } else {
      if (ctx->shmem)
         ctx->shmem->async_error++;
      print(0, "command submission failed failed (ring: %d, num_chunks: %d)", req->ring_idx, num_chunks);
   }

   if (r != 0 || ctx->debug >= 4) {
      print(1, "GPU submit used %d BOs:", bo_list_in.bo_number);
      print(1, "Used | Resource ID ");
      print(1, "-----|-------------");
      hash_table_foreach (ctx->base.resource_table, entry) {
         const struct amdgpu_object *o = entry->data;
         bool used = false;
         for (unsigned j = 0; j < bo_list_in.bo_number && !used; j++) {
            if (bo_handles_in[j].bo_handle == o->base.res_id)
               used = true;
         }

         if (r == 0 && !used)
            continue;

         print(1, "%s | %*u ", used ? "  x " : "    ",
               (int)strlen("Resource ID"), o->base.res_id);
      }
   }

   drmSyncobjDestroy(amdgpu_device_get_fd(ctx->dev), syncobj_out.handle);

   print(3, "ctx: %d -> seqno={v=%d a=%ld} r=%d", req->ctx_id, hdr->seqno, seqno, r);

end:
   if (bo_list)
      free(bo_list);
   free(chunks);
   rsp->ret = r;
   return r;
}

static int amdgpu_ccmd_reserve_vmid(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_reserve_vmid_req *req = to_amdgpu_ccmd_reserve_vmid_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_ccmd_rsp *rsp;
   rsp = drm_context_rsp(dctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   if (req->flags & ~AMDGPU_CCMD_RESERVE_VMID_UNRESERVE) {
      print(0, "Invalid flags 0x%" PRIu64, req->flags);
      rsp->ret = -EINVAL;
      return -1;
   }

   rsp->ret = (req->flags & AMDGPU_CCMD_RESERVE_VMID_UNRESERVE) ?
         amdgpu_vm_unreserve_vmid(ctx->dev, 0) : amdgpu_vm_reserve_vmid(ctx->dev, 0);
   return 0;
}

static int amdgpu_ccmd_set_pstate(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_set_pstate_req *req = to_amdgpu_ccmd_set_pstate_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_ccmd_set_pstate_rsp *rsp;
   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   if (req->pad != 0) {
      print(0, "Padding not zeroed");
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx,
                                                            (uintptr_t)req->ctx_id);
   if (actx == NULL) {
      print(0, "Couldn't find amdgpu_context with id %d", req->ctx_id);
      rsp->hdr.ret = -EINVAL;
      return -1;
   }
   rsp->hdr.ret = amdgpu_cs_ctx_stable_pstate(actx, req->op, req->flags, &rsp->out_flags);
   return 0;
}

static int amdgpu_ccmd_cs_query_fence_status(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_cs_query_fence_status_req *req = to_amdgpu_ccmd_cs_query_fence_status_req(hdr);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);
   struct amdgpu_ccmd_cs_query_fence_status_rsp *rsp;
   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp) {
      print(0, "Cannot alloc response buffer");
      return -ENOMEM;
   }

   amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx,
                                                            (uintptr_t)req->ctx_id);
   if (actx == NULL) {
      print(0, "Couldn't find amdgpu_context with id %d", req->ctx_id);
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   struct amdgpu_cs_fence fence;
   fence.context = actx;
   fence.ip_type = req->ip_type;
   fence.ip_instance = req->ip_instance;
   fence.ring = req->ring;
   fence.fence = req->fence;

   rsp->hdr.ret = amdgpu_cs_query_fence_status(&fence, req->timeout_ns, req->flags, &rsp->expired);

   return 0;
}

static const struct drm_ccmd ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [AMDGPU_CCMD_##N] = {#N, amdgpu_ccmd_##n, sizeof(struct amdgpu_ccmd_##n##_req)}
   HANDLER(QUERY_INFO, query_info),
   HANDLER(GEM_NEW, gem_new),
   HANDLER(BO_VA_OP, bo_va_op),
   HANDLER(CS_SUBMIT, cs_submit),
   HANDLER(SET_METADATA, set_metadata),
   HANDLER(BO_QUERY_INFO, bo_query_info),
   HANDLER(CREATE_CTX, create_ctx),
   HANDLER(RESERVE_VMID, reserve_vmid),
   HANDLER(SET_PSTATE, set_pstate),
   HANDLER(CS_QUERY_FENCE_STATUS, cs_query_fence_status),
};

static int
amdgpu_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags,
                             uint32_t ring_idx, uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct amdgpu_context *ctx = to_amdgpu_context(dctx);

   /* timeline is ring_idx-1 (because ring_idx 0 is host CPU timeline) */
   if (ring_idx > AMDGPU_HW_IP_NUM) {
      print(0, "invalid ring_idx: %" PRIu32, ring_idx);
      return -EINVAL;
   }
   /* ring_idx zero is used for the guest to synchronize with host CPU,
    * meaning by the time ->submit_fence() is called, the fence has
    * already passed.. so just immediate signal:
    */
   if (ring_idx == 0 || ctx->timelines[ring_idx - 1].last_fence_fd < 0) {
      vctx->fence_retire(vctx, ring_idx, fence_id);
      return 0;
   }

   print(3, "ring_idx: %d fence_id: %lu", ring_idx, fence_id);
   return drm_timeline_submit_fence(&ctx->timelines[ring_idx - 1], flags, fence_id);
}

struct virgl_context *
amdgpu_renderer_create(int fd, size_t debug_len, const char *debug_name)
{
   struct amdgpu_context *ctx;
   amdgpu_device_handle dev;

   int r;
   uint32_t drm_major, drm_minor;

   /* Don't use libdrm_amdgpu device deduplication logic. The goal is to
    * get a different drm_file for each application inside the guest so we
    * get inter-application implicit synchronisation handled for us by
    * the kernel.
    * This also makes the application completely separate (eg: each one gets
    * its own VM space).
    * Using dlopen wouldn't work because it returns the same refcounted handle.
    * dlmopen(LM_ID_NEWLM, ...) would work but there can only be 16 namespaces.
    */
   r = amdgpu_device_initialize2(fd, false, &drm_major, &drm_minor, &dev);
   if (r) {
      printf("amdgpu_device_initialize failed (fd=%d, %s)\n", fd, strerror(errno));
      close(fd);
      return NULL;
   }

   uint32_t timeline_count = 0;
   for (unsigned ip_type = 0; ip_type < AMDGPU_HW_IP_NUM; ip_type++) {
      struct drm_amdgpu_info_hw_ip ip_info = {0};

      int r = amdgpu_query_hw_ip_info(dev, ip_type, 0, &ip_info);
      if (r < 0)
         continue;

      if (ip_info.available_rings)
         timeline_count += util_bitcount(ip_info.available_rings);
   }
   if (timeline_count == 0) {
      printf("ERR: No available_rings for dev %d\n", fd);
      amdgpu_device_deinitialize(dev);
      return NULL;
   }

   ctx = calloc(1, sizeof(struct amdgpu_context) + timeline_count * sizeof(ctx->timelines[0]));
   if (ctx == NULL)
      goto fail;
   ctx->debug_name = strndup(debug_name, debug_len);
   if (ctx->debug_name == NULL)
      goto fail;
   ctx->debug = -1;
   ctx->dev = dev;
   const char *d = getenv("DEBUG");
   if (d)
      ctx->debug = atoi(d);

   print(1, "amdgpu_renderer_create name=%s fd=%d (from %d) -> dev=%p", ctx->debug_name, fd,
         amdgpu_device_get_fd(ctx->dev), (void*)ctx->dev);

   if (!drm_context_init(&ctx->base, os_dupfd_cloexec(amdgpu_device_get_fd(ctx->dev)),
                         ccmd_dispatch, ARRAY_SIZE(ccmd_dispatch)))
      goto fail_init;

   ctx->base.base.destroy = amdgpu_renderer_destroy;
   ctx->base.base.attach_resource = amdgpu_renderer_attach_resource;
   ctx->base.base.export_opaque_handle = amdgpu_renderer_export_opaque_handle;
   ctx->base.base.resource_map = amdgpu_renderer_resource_map;
   ctx->base.base.get_blob = amdgpu_renderer_get_blob;
   ctx->base.base.submit_fence = amdgpu_renderer_submit_fence;
   ctx->base.base.supports_fence_sharing = true;
   ctx->base.free_object = amdgpu_renderer_free_object;

   ctx->id_to_ctx = _mesa_hash_table_u64_create(NULL);
   if (ctx->id_to_ctx == NULL)
      goto fail_hash_table;

   /* Ring 0 is for CPU execution. */
   /* TODO: add a setting to control which queues are exposed to the
    * guest.
    */
   uint32_t ring_idx = 1;
   for (unsigned ip_type = 0; ip_type < AMDGPU_HW_IP_NUM; ip_type++) {
      struct drm_amdgpu_info_hw_ip ip_info = {0};

      int r = amdgpu_query_hw_ip_info(dev, ip_type, 0, &ip_info);
      if (r < 0)
         continue;

      int cnt = util_bitcount(ip_info.available_rings);
      for (int i = 0; i < cnt; i++) {
         char *name;
         if (asprintf(&name, "a-%s-%d", ctx->debug_name, ring_idx) < 0)
            goto fail_context_deinit;
         drm_timeline_init(&ctx->timelines[ring_idx - 1], &ctx->base.base,
                           name, ring_idx,
                           drm_context_fence_retire);
         ring_idx += 1;
      }
   }
   ctx->timeline_count = timeline_count;
   assert(ring_idx == timeline_count + 1);

   close(fd);

   return &ctx->base.base;

fail_context_deinit:
   _mesa_hash_table_u64_destroy(ctx->id_to_ctx, NULL);
fail_hash_table:
   drm_context_deinit(&ctx->base);
fail_init:
   free((void*)ctx->debug_name);
fail:
   free(ctx);
   amdgpu_device_deinitialize(dev);
   close(fd);
   return NULL;
}
