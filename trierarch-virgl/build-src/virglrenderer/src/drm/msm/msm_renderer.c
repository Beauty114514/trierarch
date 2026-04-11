/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <xf86drm.h>

#include "virgl_context.h"
#include "virgl_util.h"
#include "virglrenderer.h"

#include "util/anon_file.h"
#include "util/hash_table.h"
#include "util/libsync.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/u_atomic.h"
#include "util/u_thread.h"

#include "drm_context.h"
#include "drm_fence.h"
#include "drm_hw.h"

#include "msm_drm.h"
#include "msm_proto.h"
#include "msm_renderer.h"

static unsigned nr_timelines;
static uint32_t uabi_version;

/**
 * A single context (from the PoV of the virtio-gpu protocol) maps to
 * a single drm device open.  Other drm/msm constructs (ie. submitqueue)
 * are opaque to the protocol.
 *
 * Typically each guest process will open a single virtio-gpu "context".
 * The single drm device open maps to an individual msm_gem_address_space
 * on the kernel side, providing GPU address space isolation between
 * guest processes.
 *
 * GEM buffer objects are tracked via one of two id's:
 *  - resource-id:  global, assigned by guest kernel
 *  - blob-id:      context specific, assigned by guest userspace
 *
 * The blob-id is used to link the bo created via MSM_CCMD_GEM_NEW and
 * the get_blob() cb.  It is unused in the case of a bo that is imported
 * from another context.  An object is added to the blob table in GEM_NEW
 * and removed in ctx->get_blob() (where it is added to resource_table).
 * By avoiding having an obj in both tables, we can safely free remaining
 * entries in either hashtable at context teardown.
 */
struct msm_context {
   struct drm_context base;

   struct msm_shmem *shmem;

   /**
    * Maps submit-queue id to ring_idx
    */
   struct hash_table *sq_to_ring_idx_table;

   /**
    * Indexed by ring_idx-1, which is the same as the submitqueue priority+1.
    * On the kernel side, there is some drm_sched_entity per {drm_file, prio}
    * tuple, and the sched entity determines the fence timeline, ie. submits
    * against a single sched entity complete in fifo order.
    */
   struct drm_timeline timelines[];
};
DEFINE_CAST(drm_context, msm_context)

#define valid_payload_len(req) ((req)->len <= ((req)->hdr.len - sizeof(*(req))))

static int
gem_info(struct msm_context *mctx, uint32_t handle, uint32_t param, uint64_t *val)
{
   struct drm_msm_gem_info args = {
      .handle = handle,
      .info = param,
      .value = *val,
   };
   int ret;

   ret = drmCommandWriteRead(mctx->base.fd, DRM_MSM_GEM_INFO, &args, sizeof(args));
   if (ret)
      return ret;

   *val = args.value;
   return 0;
}

static int
gem_close(int fd, uint32_t handle)
{
   struct drm_gem_close close_req = {
      .handle = handle,
   };
   return drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close_req);
}

struct msm_object {
   struct drm_object base;
   uint32_t flags;
   bool exported   : 1;
   bool exportable : 1;
   uint8_t *map;
};
DEFINE_CAST(drm_object, msm_object)

static struct msm_object *
msm_object_create(uint32_t handle, uint32_t flags, uint64_t size)
{
   struct msm_object *obj = calloc(1, sizeof(*obj));

   if (!obj)
      return NULL;

   obj->base.handle = handle;
   obj->base.size = size;
   obj->flags = flags;

   return obj;
}

static struct msm_object *
msm_retrieve_object_from_blob_id(struct msm_context *mctx, uint64_t blob_id)
{
   struct drm_object *dobj = drm_context_retrieve_object_from_blob_id(&mctx->base, blob_id);
   if (!dobj)
      return NULL;

   return to_msm_object(dobj);
}

static struct msm_object *
msm_get_object_from_res_id(struct msm_context *mctx, uint32_t res_id)
{
   struct drm_object *dobj = drm_context_get_object_from_res_id(&mctx->base, res_id);
   if (!dobj)
      return NULL;

   return to_msm_object(dobj);
}

static uint32_t
handle_from_res_id(struct msm_context *mctx, uint32_t res_id)
{
   struct msm_object *obj = msm_get_object_from_res_id(mctx, res_id);
   if (!obj)
      return 0;    /* zero is an invalid GEM handle */
   return obj->base.handle;
}

static bool
has_cached_coherent(int fd)
{
   struct drm_msm_gem_new new_req = {
      .size = 0x1000,
      .flags = MSM_BO_CACHED_COHERENT,
   };

   /* Do a test allocation to see if cached-coherent is supported: */
   if (!drmCommandWriteRead(fd, DRM_MSM_GEM_NEW, &new_req, sizeof(new_req))) {
      gem_close(fd, new_req.handle);
      return true;
   }

   return false;
}

static bool
has_preemption(int fd, uint32_t priorities)
{
   struct drm_msm_submitqueue req = {
      .flags = MSM_SUBMITQUEUE_ALLOW_PREEMPT,
      .prio = priorities / 2,
   };

   /* Attempt to create a submitqueue with preemption is enabled, to see
    * if preemption is supported:
    */
   if (!drmCommandWriteRead(fd, DRM_MSM_SUBMITQUEUE_NEW, &req, sizeof(req))) {
      drmCommandWrite(fd, DRM_MSM_SUBMITQUEUE_CLOSE, &req.id, sizeof(req.id));
      return true;
   }

   return false;
}

static int
get_param64(int fd, uint32_t param, uint64_t *value)
{
   struct drm_msm_param req = {
      .pipe = MSM_PIPE_3D0,
      .param = param,
   };
   int ret;

   *value = 0;

   ret = drmCommandWriteRead(fd, DRM_MSM_GET_PARAM, &req, sizeof(req));
   if (ret)
      return ret;

   *value = req.value;

   return 0;
}

static int
get_param32(int fd, uint32_t param, uint32_t *value)
{
   uint64_t v64;
   int ret = get_param64(fd, param, &v64);
   *value = v64;
   return ret;
}

static void
opt_cap_bool(uint32_t *val)
{
   *val = *val ? VIRTGPU_CAP_BOOL_TRUE : VIRTGPU_CAP_BOOL_FALSE;
}

/**
 * Probe capset params.
 */
int
msm_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset)
{
   drm_log("");

   /* Require MSM_SUBMIT_FENCE_SN_IN: */
   if (capset->version_minor < 9) {
      drm_log("Host kernel too old");
      return -ENOTSUP;
   }

   capset->wire_format_version = 2;

   get_param32(fd, MSM_PARAM_PRIORITIES, &capset->u.msm.priorities);
   get_param64(fd, MSM_PARAM_VA_START,   &capset->u.msm.va_start);
   get_param64(fd, MSM_PARAM_VA_SIZE,    &capset->u.msm.va_size);
   get_param32(fd, MSM_PARAM_GPU_ID,     &capset->u.msm.gpu_id);
   get_param32(fd, MSM_PARAM_GMEM_SIZE,  &capset->u.msm.gmem_size);
   get_param64(fd, MSM_PARAM_GMEM_BASE,  &capset->u.msm.gmem_base);
   get_param64(fd, MSM_PARAM_CHIP_ID,    &capset->u.msm.chip_id);
   get_param32(fd, MSM_PARAM_MAX_FREQ,   &capset->u.msm.max_freq);
   get_param32(fd, MSM_PARAM_HIGHEST_BANK_BIT, &capset->u.msm.highest_bank_bit);
   get_param64(fd, MSM_PARAM_UBWC_SWIZZLE, &capset->u.msm.ubwc_swizzle);
   get_param64(fd, MSM_PARAM_MACROTILE_MODE, &capset->u.msm.macrotile_mode);
   get_param32(fd, MSM_PARAM_RAYTRACING, &capset->u.msm.has_raytracing);
   get_param64(fd, MSM_PARAM_UCHE_TRAP_BASE, &capset->u.msm.uche_trap_base);

   capset->u.msm.has_cached_coherent = has_cached_coherent(fd);
   capset->u.msm.has_preemption = has_preemption(fd, capset->u.msm.priorities);

   /* NOTE: on the guest side, with new guest userspace and old host
    * virglrenderer, zero means "cap not supported".  So use 1 vs ~0
    * to indicate true vs false.  The exception is bool caps that are
    * present since the first version of the protocol.
    */
   opt_cap_bool(&capset->u.msm.has_preemption);
   opt_cap_bool(&capset->u.msm.has_raytracing);

   nr_timelines = capset->u.msm.priorities;
   uabi_version = capset->version_minor;

   drm_log("wire_format_version: %u", capset->wire_format_version);
   drm_log("version_major:       %u", capset->version_major);
   drm_log("version_minor:       %u", capset->version_minor);
   drm_log("version_patchlevel:  %u", capset->version_patchlevel);
   drm_log("has_cached_coherent: %u", capset->u.msm.has_cached_coherent);
   drm_log("priorities:          %u", capset->u.msm.priorities);
   drm_log("va_start:            0x%0" PRIx64, capset->u.msm.va_start);
   drm_log("va_size:             0x%0" PRIx64, capset->u.msm.va_size);
   drm_log("gpu_id:              %u", capset->u.msm.gpu_id);
   drm_log("gmem_size:           %u", capset->u.msm.gmem_size);
   drm_log("gmem_base:           0x%0" PRIx64, capset->u.msm.gmem_base);
   drm_log("chip_id:             0x%0" PRIx64, capset->u.msm.chip_id);
   drm_log("max_freq:            %u", capset->u.msm.max_freq);
   drm_log("highest_bank_bit:    %u", capset->u.msm.highest_bank_bit);
   drm_log("ubwc_swizzle:        0x%" PRIx64, capset->u.msm.ubwc_swizzle);
   drm_log("macrotile_mode:      %" PRIu64, capset->u.msm.macrotile_mode);
   drm_log("has_raytracing:      %x", capset->u.msm.has_raytracing);
   drm_log("has_preemption:      %d", capset->u.msm.has_preemption);
   drm_log("uche_trap_base:      0x%" PRIx64, capset->u.msm.uche_trap_base);

   if (!capset->u.msm.va_size) {
      drm_log("Host kernel does not support userspace allocated IOVA");
      return -ENOTSUP;
   }

   return 0;
}

static void
msm_renderer_destroy(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);

   for (unsigned i = 0; i < nr_timelines; i++)
      drm_timeline_fini(&mctx->timelines[i]);

   drm_context_deinit(dctx);

   _mesa_hash_table_destroy(mctx->sq_to_ring_idx_table, NULL);

   free(mctx);
}

static void
msm_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_object *obj = msm_get_object_from_res_id(mctx, res->res_id);

   drm_dbg("obj=%p, res_id=%u", (void*)obj, res->res_id);

   if (!obj) {
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
            gem_close(fd, handle);
            return;
         }

         obj = msm_object_create(handle, 0, size);
         if (!obj) {
            gem_close(fd, handle);
            return;
         }

         drm_context_object_set_res_id(dctx, &obj->base, res->res_id);

         drm_dbg("obj=%p, res_id=%u, handle=%u", (void*)obj, obj->base.res_id, obj->base.handle);
      } else {
         if (fd_type != VIRGL_RESOURCE_FD_INVALID)
            close(fd);
         return;
      }
   }
}

static void
msm_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   struct msm_object *obj = to_msm_object(dobj);

   if (obj->map)
      munmap(obj->map, obj->base.size);

   gem_close(dctx->fd, obj->base.handle);

   free(obj);
}

static enum virgl_resource_fd_type
msm_renderer_export_opaque_handle(struct virgl_context *vctx, struct virgl_resource *res,
                                  int *out_fd)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_object *obj = msm_get_object_from_res_id(mctx, res->res_id);
   int ret;

   drm_dbg("obj=%p, res_id=%u", (void*)obj, res->res_id);

   if (!obj) {
      drm_err("invalid res_id %u", res->res_id);
      return VIRGL_RESOURCE_FD_INVALID;
   }

   if (!obj->exportable) {
      /* crosvm seems to like to export things it doesn't actually need an
       * fd for.. don't let it spam our fd table!
       */
      return VIRGL_RESOURCE_FD_INVALID;
   }

   ret = drmPrimeHandleToFD(dctx->fd, obj->base.handle, DRM_CLOEXEC | DRM_RDWR, out_fd);
   if (ret) {
      drm_err("failed to get dmabuf fd: %s", strerror(errno));
      return VIRGL_RESOURCE_FD_INVALID;
   }

   return VIRGL_RESOURCE_FD_DMABUF;
}

static int
msm_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                      uint64_t blob_size, uint32_t blob_flags,
                      struct virgl_context_blob *blob)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);

   drm_dbg("blob_id=%" PRIu64 ", res_id=%u, blob_size=%" PRIu64 ", blob_flags=0x%x",
           blob_id, res_id, blob_size, blob_flags);

   if ((blob_id >> 32) != 0) {
      drm_err("invalid blob_id: %" PRIu64, blob_id);
      return -EINVAL;
   }

   /* blob_id of zero is reserved for the shmem buffer: */
   if (blob_id == 0) {
      int ret = drm_context_get_shmem_blob(dctx, "msm-shmem", sizeof(*mctx->shmem),
                                           blob_size, blob_flags, blob);
      if (ret)
         return ret;

      mctx->shmem = to_msm_shmem(dctx->shmem);

      return 0;
   }

   if (!drm_context_res_id_unused(dctx, res_id)) {
      drm_err("Invalid res_id %u", res_id);
      return -EINVAL;
   }

   struct msm_object *obj = msm_retrieve_object_from_blob_id(mctx, blob_id);

   /* If GEM_NEW fails, we can end up here without a backing obj: */
   if (!obj) {
      drm_err("No object");
      return -ENOENT;
   }

   /* a memory can only be exported once; we don't want two resources to point
    * to the same storage.
    */
   if (obj->exported) {
      drm_err("Already exported!");
      return -EINVAL;
   }

   /* The size we get from guest userspace is not necessarily rounded up to the
    * nearest page size, but the actual GEM buffer allocation is, as is the
    * guest GEM buffer (and therefore the blob_size value we get from the guest
    * kernel).
    */
   if (ALIGN_POT(obj->base.size, getpagesize()) != blob_size) {
      drm_err("Invalid blob size");
      return -EINVAL;
   }

   drm_context_object_set_res_id(dctx, &obj->base, res_id);

   if (blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE) {
      int fd, ret;

      ret = drmPrimeHandleToFD(dctx->fd, obj->base.handle, DRM_CLOEXEC | DRM_RDWR, &fd);
      if (ret) {
         drm_err("Export to fd failed: %s", strerror(errno));
         return -EINVAL;
      }

      blob->type = VIRGL_RESOURCE_FD_DMABUF;
      blob->u.fd = fd;
   } else {
      blob->type = VIRGL_RESOURCE_OPAQUE_HANDLE;
      blob->u.opaque_handle = obj->base.handle;
   }

   if (obj->flags & MSM_BO_CACHED_COHERENT) {
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;
   } else {
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
   }

   obj->exported = true;
   obj->exportable = !!(blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_MAPPABLE);

   return 0;
}

static void *
msm_context_rsp(struct msm_context *mctx, const struct vdrm_ccmd_req *hdr, size_t len)
{
   return drm_context_rsp(&mctx->base, hdr, len);
}

static int
msm_ccmd_nop(UNUSED struct drm_context *dctx, UNUSED struct vdrm_ccmd_req *hdr)
{
   return 0;
}

static int
msm_ccmd_ioctl_simple(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_ioctl_simple_req *req = to_msm_ccmd_ioctl_simple_req(hdr);
   unsigned payload_len = _IOC_SIZE(req->cmd);
   size_t req_len = size_add(sizeof(*req), payload_len);
   struct msm_context *mctx = to_msm_context(dctx);

   if (hdr->len != req_len) {
      drm_err("%u != %zu", hdr->len, req_len);
      return -EINVAL;
   }

   /* Apply a reasonable upper bound on ioctl size: */
   if (payload_len > 128) {
      drm_err("invalid ioctl payload length: %u", payload_len);
      return -EINVAL;
   }

   /* Allow-list of supported ioctls: */
   unsigned iocnr = _IOC_NR(req->cmd) - DRM_COMMAND_BASE;
   switch (iocnr) {
   case DRM_MSM_GET_PARAM:
   case DRM_MSM_SUBMITQUEUE_NEW:
   case DRM_MSM_SUBMITQUEUE_CLOSE:
      break;
   default:
      drm_err("invalid ioctl: %08x (%u)", req->cmd, iocnr);
      return -EINVAL;
   }

   struct msm_ccmd_ioctl_simple_rsp *rsp;
   size_t rsp_len = sizeof(*rsp);

   if (req->cmd & IOC_OUT)
      rsp_len = size_add(rsp_len, payload_len);

   rsp = msm_context_rsp(mctx, hdr, rsp_len);

   if (!rsp)
      return -ENOMEM;

   /* Copy the payload because the kernel can write (if IOC_OUT bit
    * is set) and to avoid casting away the const:
    */
   char payload[payload_len];
   memcpy(payload, req->payload, payload_len);

   rsp->ret = drmIoctl(dctx->fd, req->cmd, payload);

   if (req->cmd & IOC_OUT)
      memcpy(rsp->payload, payload, payload_len);

   if (iocnr == DRM_MSM_SUBMITQUEUE_NEW && !rsp->ret) {
      struct drm_msm_submitqueue *args = (void *)payload;

      drm_dbg("submitqueue %u, prio %u", args->id, args->prio);

      _mesa_hash_table_insert(mctx->sq_to_ring_idx_table, (void *)(uintptr_t)args->id,
                              (void *)(uintptr_t)args->prio);
   }

   return 0;
}

static int
msm_ccmd_gem_new(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_new_req *req = to_msm_ccmd_gem_new_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   int ret = 0;

   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      drm_err("Invalid blob_id %u", req->blob_id);
      ret = -EINVAL;
      goto out_error;
   }

   /*
    * First part, allocate the GEM bo:
    */
   struct drm_msm_gem_new gem_new = {
      .size = req->size,
      .flags = req->flags,
   };

   ret = drmCommandWriteRead(dctx->fd, DRM_MSM_GEM_NEW, &gem_new, sizeof(gem_new));
   if (ret) {
      drm_err("GEM_NEW failed: %d (%s)", ret, strerror(errno));
      goto out_error;
   }

   /*
    * Second part, set the iova:
    */
   uint64_t iova = req->iova;
   ret = gem_info(mctx, gem_new.handle, MSM_INFO_SET_IOVA, &iova);
   if (ret) {
      drm_err("SET_IOVA failed: %d (%s)", ret, strerror(errno));
      goto out_close;
   }

   /*
    * And then finally create our msm_object for tracking the resource,
    * and add to blob table:
    */
   struct msm_object *obj = msm_object_create(gem_new.handle, req->flags, req->size);

   if (!obj) {
      ret = -ENOMEM;
      goto out_close;
   }

   drm_context_object_set_blob_id(dctx, &obj->base, req->blob_id);

   drm_dbg("obj=%p, blob_id=%u, handle=%u, iova=%" PRIx64,
           (void*)obj, obj->base.blob_id, obj->base.handle, iova);

   return 0;

out_close:
   gem_close(dctx->fd, gem_new.handle);
out_error:
   if (mctx->shmem)
      mctx->shmem->async_error++;
   return ret;
}

static int
msm_ccmd_gem_set_iova(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_set_iova_req *req = to_msm_ccmd_gem_set_iova_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_object *obj = msm_get_object_from_res_id(mctx, req->res_id);
   int ret = 0;

   if (!obj) {
      drm_err("Could not lookup obj: res_id=%u", req->res_id);
      ret = -ENOENT;
      goto out_error;
   }

   uint64_t iova = req->iova;
   if (iova) {
      TRACE_SCOPE("SET_IOVA");
      ret = gem_info(mctx, obj->base.handle, MSM_INFO_SET_IOVA, &iova);
   } else {
      TRACE_SCOPE("CLEAR_IOVA");
      ret = gem_info(mctx, obj->base.handle, MSM_INFO_SET_IOVA, &iova);
   }
   if (ret) {
      drm_err("SET_IOVA failed: %d (%s)", ret, strerror(errno));
      goto out_error;
   }

   drm_dbg("obj=%p, blob_id=%u, handle=%u, iova=%" PRIx64, (void*)obj, obj->base.blob_id,
           obj->base.handle, iova);

   return 0;

out_error:
   if (mctx->shmem)
      mctx->shmem->async_error++;
   return 0;
}

static int
msm_ccmd_gem_cpu_prep(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_cpu_prep_req *req = to_msm_ccmd_gem_cpu_prep_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_ccmd_gem_cpu_prep_rsp *rsp = msm_context_rsp(mctx, hdr, sizeof(*rsp));

   if (!rsp)
      return -ENOMEM;

   struct drm_msm_gem_cpu_prep args = {
      .handle = handle_from_res_id(mctx, req->res_id),
      .op = req->op | MSM_PREP_NOSYNC,
   };

   if (uabi_version >= 11)
      args.op |= MSM_PREP_BOOST;

   rsp->ret = drmCommandWrite(dctx->fd, DRM_MSM_GEM_CPU_PREP, &args, sizeof(args));

   return 0;
}

static int
msm_ccmd_gem_set_name(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_set_name_req *req = to_msm_ccmd_gem_set_name_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);

   struct drm_msm_gem_info args = {
      .handle = handle_from_res_id(mctx, req->res_id),
      .info = MSM_INFO_SET_NAME,
      .value = VOID2U64(req->payload),
      .len = req->len,
   };

   if (!valid_payload_len(req))
      return -EINVAL;

   int ret = drmCommandWrite(dctx->fd, DRM_MSM_GEM_INFO, &args, sizeof(args));
   if (ret)
      drm_err("ret=%d, len=%u, name=%.*s", ret, req->len, req->len, req->payload);

   return 0;
}

static void
msm_dump_submit(struct drm_msm_gem_submit *req)
{
#ifndef NDEBUG
   drm_log("  flags=0x%x, queueid=%u", req->flags, req->queueid);
   for (unsigned i = 0; i < req->nr_bos; i++) {
      struct drm_msm_gem_submit_bo *bos = U642VOID(req->bos);
      struct drm_msm_gem_submit_bo *bo = &bos[i];
      drm_log("  bos[%d]: handle=%u, flags=%x", i, bo->handle, bo->flags);
   }
   for (unsigned i = 0; i < req->nr_cmds; i++) {
      struct drm_msm_gem_submit_cmd *cmds = U642VOID(req->cmds);
      struct drm_msm_gem_submit_cmd *cmd = &cmds[i];
      drm_log("  cmd[%d]: type=%u, submit_idx=%u, submit_offset=%u, size=%u", i,
              cmd->type, cmd->submit_idx, cmd->submit_offset, cmd->size);
   }
#else
   (void)req;
#endif
}

static int
msm_ccmd_gem_submit(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_submit_req *req = to_msm_ccmd_gem_submit_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);

   size_t sz = sizeof(*req);
   sz = size_add(sz, size_mul(req->nr_bos,  sizeof(struct drm_msm_gem_submit_bo)));
   sz = size_add(sz, size_mul(req->nr_cmds, sizeof(struct drm_msm_gem_submit_cmd)));

   /* Normally kernel would validate out of bounds situations and return -EFAULT,
    * but since we are copying the bo handles, we need to validate that the
    * guest can't trigger us to make an out of bounds memory access:
    */
   if (sz > hdr->len) {
      drm_err("out of bounds: nr_bos=%u, nr_cmds=%u", req->nr_bos, req->nr_cmds);
      return -ENOSPC;
   }

   const unsigned bo_limit = 8192 / sizeof(struct drm_msm_gem_submit_bo);
   bool bos_on_stack = req->nr_bos < bo_limit;
   struct drm_msm_gem_submit_bo _bos[bos_on_stack ? req->nr_bos : 0];
   struct drm_msm_gem_submit_bo *bos;

   if (bos_on_stack) {
      bos = _bos;
   } else {
      bos = malloc(req->nr_bos * sizeof(bos[0]));
      if (!bos)
         return -ENOMEM;
   }

   memcpy(bos, req->payload, req->nr_bos * sizeof(bos[0]));

   for (uint32_t i = 0; i < req->nr_bos; i++)
      bos[i].handle = handle_from_res_id(mctx, bos[i].handle);

   uint32_t fence_flags = MSM_SUBMIT_FENCE_FD_OUT | MSM_SUBMIT_FENCE_SN_IN;

   int in_fence_fd = virgl_context_take_in_fence_fd(&dctx->base);
   if (in_fence_fd >= 0)
         fence_flags |= MSM_SUBMIT_FENCE_FD_IN;

   struct drm_msm_gem_submit args = {
      .flags = req->flags | fence_flags,
      .fence = req->fence,
      .fence_fd = in_fence_fd,
      .nr_bos = req->nr_bos,
      .nr_cmds = req->nr_cmds,
      .bos = VOID2U64(bos),
      .cmds = VOID2U64(&req->payload[req->nr_bos * sizeof(struct drm_msm_gem_submit_bo)]),
      .queueid = req->queue_id,
   };

   int ret = drmCommandWriteRead(dctx->fd, DRM_MSM_GEM_SUBMIT, &args, sizeof(args));
   drm_dbg("fence=%u, ret=%d", args.fence, ret);

   if (unlikely(ret)) {
      drm_err("submit failed: %s", strerror(errno));
      msm_dump_submit(&args);
      if (mctx->shmem)
         mctx->shmem->async_error++;
   } else {
      const struct hash_entry *entry =
            hash_table_search(mctx->sq_to_ring_idx_table, args.queueid);

      if (!entry) {
         drm_err("unknown submitqueue: %u", args.queueid);
         goto out;
      }

      unsigned prio = (uintptr_t)entry->data;

      drm_timeline_set_last_fence_fd(&mctx->timelines[prio], args.fence_fd);
   }

out:
   if (in_fence_fd >= 0)
      close(in_fence_fd);
   if (!bos_on_stack)
      free(bos);
   return 0;
}

static int
map_object(struct msm_context *mctx, struct msm_object *obj)
{
   uint64_t offset = 0;
   int ret;

   if (obj->map)
      return 0;

   uint32_t handle = handle_from_res_id(mctx, obj->base.res_id);
   ret = gem_info(mctx, handle, MSM_INFO_GET_OFFSET, &offset);
   if (ret) {
      drm_err("alloc failed: %s", strerror(errno));
      return ret;
   }

   uint8_t *map =
      mmap(0, obj->base.size, PROT_READ | PROT_WRITE, MAP_SHARED, mctx->base.fd, offset);
   if (map == MAP_FAILED) {
      drm_err("mmap failed: %s", strerror(errno));
      return -ENOMEM;
   }

   obj->map = map;

   return 0;
}

static int
msm_ccmd_gem_upload(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_gem_upload_req *req = to_msm_ccmd_gem_upload_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   int ret;

   if (req->pad || !valid_payload_len(req)) {
      drm_err("Invalid upload ccmd");
      return -EINVAL;
   }

   struct msm_object *obj = msm_get_object_from_res_id(mctx, req->res_id);
   if (!obj) {
      drm_err("No obj: res_id=%u", req->res_id);
      return -ENOENT;
   }

   if (size_add(req->off, req->len) > obj->base.size)
      return -EFAULT;

   ret = map_object(mctx, obj);
   if (ret)
      return ret;

   memcpy(&obj->map[req->off], req->payload, req->len);

   return 0;
}

static int
msm_ccmd_submitqueue_query(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_submitqueue_query_req *req =
      to_msm_ccmd_submitqueue_query_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_ccmd_submitqueue_query_rsp *rsp =
      msm_context_rsp(mctx, hdr, size_add(sizeof(*rsp), req->len));

   if (!rsp)
      return -ENOMEM;

   struct drm_msm_submitqueue_query args = {
      .data = VOID2U64(rsp->payload),
      .id = req->queue_id,
      .param = req->param,
      .len = req->len,
   };

   rsp->ret =
      drmCommandWriteRead(dctx->fd, DRM_MSM_SUBMITQUEUE_QUERY, &args, sizeof(args));

   rsp->out_len = args.len;

   return 0;
}

static int
msm_ccmd_wait_fence(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_wait_fence_req *req = to_msm_ccmd_wait_fence_req(hdr);
   struct msm_context *mctx = to_msm_context(dctx);
   struct msm_ccmd_wait_fence_rsp *rsp = msm_context_rsp(mctx, hdr, sizeof(*rsp));

   if (!rsp)
      return -ENOMEM;

   struct timespec t;

   /* Use current time as timeout, to avoid blocking: */
   clock_gettime(CLOCK_MONOTONIC, &t);

   struct drm_msm_wait_fence args = {
      .fence = req->fence,
      .flags = (uabi_version) >= 11 ? MSM_WAIT_FENCE_BOOST : 0,
      .queueid = req->queue_id,
      .timeout =
         {
            .tv_sec = t.tv_sec,
            .tv_nsec = t.tv_nsec,
         },
   };

   rsp->ret = drmCommandWrite(dctx->fd, DRM_MSM_WAIT_FENCE, &args, sizeof(args));

   return 0;
}

static int
msm_ccmd_set_debuginfo(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct msm_ccmd_set_debuginfo_req *req = to_msm_ccmd_set_debuginfo_req(hdr);

   size_t sz = sizeof(*req);
   sz = size_add(sz, req->comm_len);
   sz = size_add(sz, req->cmdline_len);

   if (sz > hdr->len) {
      drm_err("out of bounds: comm_len=%u, cmdline_len=%u", req->comm_len, req->cmdline_len);
      return -ENOSPC;
   }

   struct drm_msm_param set_comm = {
      .pipe = MSM_PIPE_3D0,
      .param = MSM_PARAM_COMM,
      .value = VOID2U64(&req->payload[0]),
      .len = req->comm_len,
   };

   drmCommandWriteRead(dctx->fd, DRM_MSM_SET_PARAM, &set_comm, sizeof(set_comm));

   struct drm_msm_param set_cmdline = {
      .pipe = MSM_PIPE_3D0,
      .param = MSM_PARAM_CMDLINE,
      .value = VOID2U64(&req->payload[req->comm_len]),
      .len = req->cmdline_len,
   };

   drmCommandWriteRead(dctx->fd, DRM_MSM_SET_PARAM, &set_cmdline, sizeof(set_cmdline));

   return 0;
}

static const struct drm_ccmd ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [MSM_CCMD_##N] = {#N, msm_ccmd_##n, sizeof(struct msm_ccmd_##n##_req)}
   HANDLER(NOP, nop),
   HANDLER(IOCTL_SIMPLE, ioctl_simple),
   HANDLER(GEM_NEW, gem_new),
   HANDLER(GEM_SET_IOVA, gem_set_iova),
   HANDLER(GEM_CPU_PREP, gem_cpu_prep),
   HANDLER(GEM_SET_NAME, gem_set_name),
   HANDLER(GEM_SUBMIT, gem_submit),
   HANDLER(GEM_UPLOAD, gem_upload),
   HANDLER(SUBMITQUEUE_QUERY, submitqueue_query),
   HANDLER(WAIT_FENCE, wait_fence),
   HANDLER(SET_DEBUGINFO, set_debuginfo),
};

static void
msm_renderer_fence_retire(struct virgl_context *vctx,
                          uint32_t ring_idx,
                          uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);

   get_param32(dctx->fd, MSM_PARAM_FAULTS, &mctx->shmem->global_faults);

   vctx->fence_retire(vctx, ring_idx, fence_id);
}

static int
msm_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags, uint32_t ring_idx,
                          uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct msm_context *mctx = to_msm_context(dctx);

   drm_dbg("flags=0x%x, ring_idx=%" PRIu32 ", fence_id=%" PRIu64, flags,
           ring_idx, fence_id);

   /* timeline is ring_idx-1 (because ring_idx 0 is host CPU timeline) */
   if (ring_idx > nr_timelines) {
      drm_err("invalid ring_idx: %" PRIu32, ring_idx);
      return -EINVAL;
   }

   /* ring_idx zero is used for the guest to synchronize with host CPU,
    * meaning by the time ->submit_fence() is called, the fence has
    * already passed.. so just immediate signal:
    */
   if (ring_idx == 0 || mctx->timelines[ring_idx - 1].last_fence_fd < 0) {
      vctx->fence_retire(vctx, ring_idx, fence_id);
      return 0;
   }

   return drm_timeline_submit_fence(&mctx->timelines[ring_idx - 1], flags, fence_id);
}

struct virgl_context *
msm_renderer_create(int fd, UNUSED size_t debug_len, UNUSED const char *debug_name)
{
   struct msm_context *mctx;

   drm_log("");

   mctx = calloc(1, sizeof(*mctx) + (nr_timelines * sizeof(mctx->timelines[0])));
   if (!mctx)
      return NULL;

   if (!drm_context_init(&mctx->base, fd, ccmd_dispatch, ARRAY_SIZE(ccmd_dispatch))) {
      free(mctx);
      return NULL;
   }

   /* Indexed by submitqueue-id: */
   mctx->sq_to_ring_idx_table = _mesa_hash_table_create_u32_keys(NULL);

   for (unsigned i = 0; i < nr_timelines; i++) {
      unsigned ring_idx = i + 1; /* ring_idx 0 is host CPU */
      drm_timeline_init(&mctx->timelines[i], &mctx->base.base, "msm-sync",
                        ring_idx, msm_renderer_fence_retire);
   }

   mctx->base.base.destroy = msm_renderer_destroy;
   mctx->base.base.attach_resource = msm_renderer_attach_resource;
   mctx->base.base.export_opaque_handle = msm_renderer_export_opaque_handle;
   mctx->base.base.get_blob = msm_renderer_get_blob;
   mctx->base.base.submit_fence = msm_renderer_submit_fence;
   mctx->base.base.supports_fence_sharing = true;
   mctx->base.free_object = msm_renderer_free_object;

   /* Only 4 byte alignment is required for legacy reasons. */
   mctx->base.ccmd_alignment = 4;

   return &mctx->base.base;
}
