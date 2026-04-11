/*
 * Copyright 2024 Sergio Lopez
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/dma-buf.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <xf86drm.h>

#include "virgl_context.h"
#include "virgl_util.h"
#include "virglrenderer.h"

#include "util/anon_file.h"
#include "util/hash_table.h"
#include "util/u_atomic.h"

#include "drm_context.h"
#include "drm_fence.h"
#include "drm_hw.h"

#include "asahi_drm.h"
#include "asahi_proto.h"
#include "asahi_renderer.h"

/* We don't currently support high-priority queues. Could be lifted later. */
#define MAX_PRIORITY (DRM_ASAHI_PRIORITY_MEDIUM)
#define NR_TIMELINES (MAX_PRIORITY + 1)

/**
 * A single context (from the PoV of the virtio-gpu protocol) maps to
 * a single drm device open.  Other drm constructs (ie. submitqueue) are
 * opaque to the protocol.
 *
 * Typically each guest process will open a single virtio-gpu "context".
 * The single drm device open maps to an individual GEM address_space on
 * the kernel side, providing GPU address space isolation between guest
 * processes.
 *
 * GEM buffer objects are tracked via one of two id's:
 *  - resource-id:  global, assigned by guest kernel
 *  - blob-id:      context specific, assigned by guest userspace
 *
 * The blob-id is used to link the bo created via the corresponding ioctl
 * and the get_blob() cb. It is unused in the case of a bo that is
 * imported from another context.  An object is added to the blob table
 * in GEM_NEW and removed in ctx->get_blob() (where it is added to
 * resource_table). By avoiding having an obj in both tables, we can
 * safely free remaining entries in either hashtable at context teardown.
 */
struct asahi_context {
   struct drm_context base;
   struct asahi_shmem *shmem;

   /**
    * Maps queue ID to ring_idx
    */
   struct hash_table *queue_to_ring_idx;

   /**
    * Indexed by ring_idx-1, which is the same as the submitqueue priority.
    * On the kernel side, there is some drm_sched_entity per {drm_file, prio}
    * tuple, and the sched entity determines the fence timeline, ie. submits
    * against a single sched entity complete in fifo order.
    */
   struct drm_timeline timelines[NR_TIMELINES];
};
DEFINE_CAST(drm_context, asahi_context)

/*
 * Returning a nonzero error code from a CCMD handler would wedge the context,
 * so we return zero and instead update the async error count. This helper makes
 * that convenient.
 */
static int
async_ret(struct asahi_context *actx, int ret)
{
   if (unlikely(ret)) {
      if (actx->shmem)
         actx->shmem->async_error++;
   }

   return 0;
}

static int
gem_close(int fd, uint32_t handle)
{
   struct drm_gem_close args = { .handle = handle };
   return drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &args);
}

struct asahi_object {
   struct drm_object base;
   uint32_t flags;
   bool exported   : 1;
   bool exportable : 1;
   uint8_t *map;
};
DEFINE_CAST(drm_object, asahi_object)

static struct asahi_object *
asahi_object_create(uint32_t handle, uint32_t flags, uint32_t size)
{
   struct asahi_object *obj = calloc(1, sizeof(*obj));

   if (!obj)
      return NULL;

   obj->base.handle = handle;
   obj->base.size = size;
   obj->flags = flags;

   return obj;
}

static uint32_t
handle_from_res_id(struct drm_context *dctx, uint32_t res_id)
{
   struct drm_object *obj = drm_context_get_object_from_res_id(dctx, res_id);
   if (!obj)
      return 0; /* zero is an invalid GEM handle */
   return obj->handle;
}

/**
 * Probe capset params.
 */
int
asahi_renderer_probe(UNUSED int fd, struct virgl_renderer_capset_drm *capset)
{
   capset->wire_format_version = 2;
   return 0;
}

static void
asahi_renderer_destroy(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct asahi_context *actx = to_asahi_context(dctx);

   for (unsigned i = 0; i < NR_TIMELINES; ++i) {
      drm_timeline_fini(&actx->timelines[i]);
   }

   drm_context_deinit(dctx);

   _mesa_hash_table_destroy(actx->queue_to_ring_idx, NULL);

   free(actx);
}

static void
asahi_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct asahi_object *obj =
      to_asahi_object(drm_context_get_object_from_res_id(dctx, res->res_id));

   drm_dbg("obj=%p, res_id=%u", (void *)obj, res->res_id);

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
            drm_log("Could not import: %s", strerror(errno));
            close(fd);
            return;
         }

         /* lseek() to get bo size */
         int size = lseek(fd, 0, SEEK_END);
         if (size < 0)
            drm_log("lseek failed: %d (%s)", size, strerror(errno));
         close(fd);

         obj = asahi_object_create(handle, 0, size);
         if (!obj)
            return;

         drm_context_object_set_res_id(dctx, &obj->base, res->res_id);

         obj->exportable = 1;
      } else {
         if (fd_type != VIRGL_RESOURCE_FD_INVALID)
            close(fd);
         return;
      }
   }
}

static enum virgl_resource_fd_type
asahi_renderer_export_opaque_handle(struct virgl_context *vctx,
                                    struct virgl_resource *res, int *out_fd)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct drm_object *obj = drm_context_get_object_from_res_id(dctx, res->res_id);
   int ret;

   drm_dbg("obj=%p, res_id=%u", (void *)obj, res->res_id);

   if (!obj) {
      drm_log("invalid res_id %u", res->res_id);
      return VIRGL_RESOURCE_FD_INVALID;
   }

   if (!to_asahi_object(obj)->exportable)
      return VIRGL_RESOURCE_FD_INVALID;

   ret = drmPrimeHandleToFD(dctx->fd, obj->handle, DRM_CLOEXEC | DRM_RDWR, out_fd);
   if (ret) {
      drm_log("failed to get dmabuf fd: %s", strerror(errno));
      return VIRGL_RESOURCE_FD_INVALID;
   }

   return VIRGL_RESOURCE_FD_DMABUF;
}

static void
asahi_renderer_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   struct asahi_object *obj = to_asahi_object(dobj);

   if (obj->map)
      munmap(obj->map, obj->base.size);

   gem_close(dctx->fd, obj->base.handle);

   free(obj);
}

static int
asahi_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                        uint64_t blob_size, uint32_t blob_flags,
                        struct virgl_context_blob *blob)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct asahi_context *actx = to_asahi_context(dctx);

   drm_dbg("blob_id=%" PRIu64 ", res_id=%u, blob_size=%" PRIu64 ", blob_flags=0x%x",
           blob_id, res_id, blob_size, blob_flags);

   if ((blob_id >> 32) != 0) {
      drm_log("invalid blob_id: %" PRIu64, blob_id);
      return -EINVAL;
   }

   /* blob_id of zero is reserved for the shmem buffer: */
   if (blob_id == 0) {
      int ret = drm_context_get_shmem_blob(dctx, "asahi-shmem", sizeof(*actx->shmem),
                                           blob_size, blob_flags, blob);
      if (ret)
         return ret;

      actx->shmem = to_asahi_shmem(dctx->shmem);

      return 0;
   }

   if (!drm_context_res_id_unused(dctx, res_id)) {
      drm_log("Invalid res_id %u", res_id);
      return -EINVAL;
   }

   struct asahi_object *obj =
      to_asahi_object(drm_context_retrieve_object_from_blob_id(dctx, blob_id));

   /* If GEM_NEW fails, we can end up here without a backing obj: */
   if (!obj) {
      drm_log("No object");
      return -ENOENT;
   }

   /* a memory can only be exported once; we don't want two resources to point
    * to the same storage.
    */
   if (obj->exported) {
      drm_log("Already exported!");
      return -EINVAL;
   }

   /* The size we get from guest userspace is not necessarily rounded up to the
    * nearest page size, but the actual GEM buffer allocation is, as is the
    * guest GEM buffer (and therefore the blob_size value we get from the guest
    * kernel).
    */
   if (ALIGN_POT(obj->base.size, getpagesize()) != blob_size) {
      drm_log("Invalid blob size");
      return -EINVAL;
   }

   drm_context_object_set_res_id(dctx, &obj->base, res_id);

   if (blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE) {
      int fd, ret;

      ret = drmPrimeHandleToFD(dctx->fd, obj->base.handle, DRM_CLOEXEC | DRM_RDWR, &fd);
      if (ret) {
         drm_log("Export to fd failed");
         return -EINVAL;
      }

      blob->type = VIRGL_RESOURCE_FD_DMABUF;
      blob->u.fd = fd;
   } else {
      blob->type = VIRGL_RESOURCE_OPAQUE_HANDLE;
      blob->u.opaque_handle = obj->base.handle;
   }

   if (obj->flags & DRM_ASAHI_GEM_WRITEBACK) {
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;
   } else {
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
   }

   obj->exported = true;
   obj->exportable = !!(blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE);

   return 0;
}

static void *
asahi_context_rsp(struct asahi_context *actx, const struct vdrm_ccmd_req *hdr,
                  unsigned len)
{
   return drm_context_rsp(&actx->base, hdr, len);
}

static int
asahi_ccmd_nop(UNUSED struct drm_context *dctx, UNUSED struct vdrm_ccmd_req *hdr)
{
   return 0;
}

/* Reasonable upper bound for simple ioctl payloads */
#define MAX_SIMPLE_PAYLOAD_SIZE (128)

static int
asahi_ccmd_ioctl_simple(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   const struct asahi_ccmd_ioctl_simple_req *req = to_asahi_ccmd_ioctl_simple_req(hdr);
   unsigned payload_len = _IOC_SIZE(req->cmd);
   unsigned req_len = size_add(sizeof(*req), payload_len);

   if (hdr->len != req_len) {
      drm_log("%u != %u", hdr->len, req_len);
      return -EINVAL;
   }

   if (payload_len > MAX_SIMPLE_PAYLOAD_SIZE) {
      drm_log("invalid ioctl payload length: %u", payload_len);
      return -EINVAL;
   }

   /* Allow-list of supported ioctls: */
   unsigned iocnr = _IOC_NR(req->cmd) - DRM_COMMAND_BASE;
   switch (iocnr) {
   case DRM_ASAHI_VM_CREATE:
   case DRM_ASAHI_VM_DESTROY:
   case DRM_ASAHI_QUEUE_CREATE:
   case DRM_ASAHI_QUEUE_DESTROY:
   case DRM_ASAHI_GET_TIME:
      break;
   default:
      drm_log("invalid ioctl: %08x (%u)", req->cmd, iocnr);
      return -EINVAL;
   }

   if (iocnr == DRM_ASAHI_QUEUE_CREATE) {
      struct drm_asahi_queue_create *args = (void *)req->payload;
      if (args->priority > MAX_PRIORITY) {
         drm_err("unexpected priority %u\n", args->priority);
         return -EINVAL;
      }
   }

   struct asahi_ccmd_ioctl_simple_rsp *rsp;
   unsigned rsp_len = sizeof(*rsp);

   if (req->cmd & IOC_OUT)
      rsp_len = size_add(rsp_len, payload_len);

   rsp = asahi_context_rsp(actx, hdr, rsp_len);
   if (!rsp)
      return -ENOMEM;

   /* Copy the payload because the kernel can write (if IOC_OUT bit
    * is set) and to avoid casting away the const:
    */
   char payload[MAX_SIMPLE_PAYLOAD_SIZE];
   memcpy(payload, req->payload, payload_len);

   rsp->ret = drmIoctl(dctx->fd, req->cmd, payload);

   if (req->cmd & IOC_OUT)
      memcpy(rsp->payload, payload, payload_len);

   if (iocnr == DRM_ASAHI_QUEUE_CREATE && !rsp->ret) {
      struct drm_asahi_queue_create *args = (void *)payload;

      drm_dbg("submitqueue %u, prio %u", args->queue_id, args->priority);

      unsigned ring_idx = args->priority + 1; /* ring_idx 0 is host CPU */

      _mesa_hash_table_insert(actx->queue_to_ring_idx, (void *)(uintptr_t)args->queue_id,
                              (void *)(uintptr_t)ring_idx);
   }

   return 0;
}

static int
asahi_ccmd_get_params(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   struct asahi_ccmd_get_params_req *req = to_asahi_ccmd_get_params_req(hdr);
   unsigned req_len = sizeof(*req);

   if (hdr->len != req_len) {
      drm_err("asahi_ccmd_get_params: %u != %u", hdr->len, req_len);
      return -EINVAL;
   }

   struct asahi_ccmd_get_params_rsp *rsp;
   unsigned rsp_len = sizeof(*rsp) + req->params.size;

   rsp = asahi_context_rsp(actx, hdr, rsp_len);
   if (!rsp)
      return -ENOMEM;

   if (req->params.param_group != 0) {
      rsp->ret = -EINVAL;
      return 0;
   }

   req->params.pointer = (uint64_t)(uintptr_t)rsp->payload;
   rsp->ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_GET_PARAMS, &req->params);
   return 0;
}

static int
asahi_ccmd_gem_new(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   const struct asahi_ccmd_gem_new_req *req = to_asahi_ccmd_gem_new_req(hdr);

   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      drm_log("Invalid blob_id %u", req->blob_id);
      return async_ret(actx, -EINVAL);
   }

   int create_vm_id = req->vm_id;
   if (!(req->flags & DRM_ASAHI_GEM_VM_PRIVATE)) {
      create_vm_id = 0;
   }

   /*
    * First part, allocate the GEM bo:
    */
   struct drm_asahi_gem_create gem_create = {
      .flags = req->flags,
      .vm_id = create_vm_id,
      .size = req->size,
   };

   int ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_GEM_CREATE, &gem_create);
   if (ret) {
      drm_log("GEM_CREATE failed: %d (%s)", ret, strerror(errno));
      return async_ret(actx, ret);
   }

   /*
    * Second part, bind:
    */
   if (req->addr) {
      struct drm_asahi_gem_bind_op op = {
         .flags = req->bind_flags,
         .handle = gem_create.handle,
         .offset = 0,
         .range = req->size,
         .addr = req->addr,
      };

      struct drm_asahi_vm_bind bind = {
         .num_binds = 1,
         .stride = sizeof(op),
         .userptr = (uintptr_t)&op,
         .vm_id = req->vm_id,
      };

      ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_VM_BIND, &bind);
      if (ret) {
         drm_log("DRM_IOCTL_ASAHI_VM_BIND failed: (handle=%d)", gem_create.handle);
         goto out_close;
      }
   }

   /*
    * And then finally create our asahi_object for tracking the resource,
    * and add to blob table:
    */
   struct asahi_object *obj =
      asahi_object_create(gem_create.handle, req->flags, req->size);

   if (!obj) {
      ret = -ENOMEM;
      goto out_close;
   }

   drm_context_object_set_blob_id(dctx, &obj->base, req->blob_id);

   drm_dbg("obj=%p, blob_id=%u, handle=%u", (void *)obj, obj->base.blob_id,
           obj->base.handle);

   return 0;

out_close:
   gem_close(dctx->fd, gem_create.handle);
   return async_ret(actx, ret);
}

static int
asahi_ccmd_vm_bind(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   struct asahi_ccmd_vm_bind_req *req = to_asahi_ccmd_vm_bind_req(hdr);
   int ret = 0;
   uint8_t *payload = (uint8_t *)req->payload;

   /* Offset is right after handle, this ensures the handle fixup is sound. */
   if (req->stride < offsetof(struct drm_asahi_gem_bind_op, offset)) {
      drm_err("Invalid VM_BIND stride");
      return -EINVAL;
   }

   if (req->hdr.len != (sizeof(*req) + (req->stride * req->count))) {
      drm_err("Invalid VM bind length");
      return -EINVAL;
   }

   struct drm_asahi_gem_bind_op *ops = calloc(req->count, req->stride);
   struct drm_asahi_vm_bind bind = {
      .vm_id = req->vm_id,
      .stride = sizeof(*ops),
      .num_binds = req->count,
      .userptr = (uint64_t)(uintptr_t)ops,
   };

   for (unsigned i = 0; i < req->count; ++i) {
      memcpy(&ops[i], payload + (i * req->stride), req->stride);
      ops[i].handle = handle_from_res_id(dctx, ops[i].handle);
   }

   ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_VM_BIND, &bind);
   if (ret) {
      drm_err("DRM_IOCTL_ASAHI_GEM_BIND failed");
   }

   free(ops);
   return async_ret(actx, ret);
}

static int
asahi_ccmd_gem_bind_object(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   struct asahi_ccmd_gem_bind_object_req *req = to_asahi_ccmd_gem_bind_object_req(hdr);
   struct drm_asahi_gem_bind_object *gem_bind = &req->bind;
   int ret = 0;

   struct asahi_ccmd_gem_bind_object_rsp *rsp = NULL;
   unsigned rsp_len = sizeof(*rsp);

   if (req->bind.op == DRM_ASAHI_BIND_OBJECT_OP_BIND) {
      /* Only bind has a response */
      rsp = asahi_context_rsp(actx, hdr, rsp_len);
      if (!rsp)
         return -ENOMEM;
   }

   if (gem_bind->handle) {
      struct drm_object *obj = drm_context_get_object_from_res_id(dctx, gem_bind->handle);

      if (!obj) {
         drm_err("Could not lookup obj: res_id=%u", gem_bind->handle);
         ret = -ENOENT;
         goto exit_rsp;
      }

      drm_dbg("gem_bind: handle=%d", obj->handle);
      gem_bind->handle = obj->handle;
   }

   ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_GEM_BIND_OBJECT, gem_bind);
   if (ret) {
      drm_err("DRM_IOCTL_ASAHI_GEM_BIND_OBJECT failed: (handle=%d)", gem_bind->handle);
   }

exit_rsp:
   if (rsp) {
      rsp->object_handle = gem_bind->object_handle;
      rsp->ret = ret;
      return 0;
   } else {
      return ret;
   }
}

static int
asahi_ccmd_submit(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct asahi_context *actx = to_asahi_context(dctx);
   const struct asahi_ccmd_submit_req *req = to_asahi_ccmd_submit_req(hdr);
   int ret = 0;

   if (hdr->len < sizeof(struct asahi_ccmd_submit_req)) {
      drm_err("invalid cmd length");
      return -EINVAL;
   }

   const struct hash_entry *entry =
      hash_table_search(actx->queue_to_ring_idx, req->queue_id);
   if (!entry) {
      drm_err("unknown submitqueue: %u", req->queue_id);
      return -EINVAL;
   }

   unsigned ring_idx = (uintptr_t)entry->data;
   uint8_t *ptr = (uint8_t *)req->payload;
   uint8_t *end = ptr + (hdr->len - sizeof(struct asahi_ccmd_submit_req));

   uint8_t *cmdbuf = ptr;
   ptr += req->cmdbuf_size;

   struct asahi_ccmd_submit_res *extres = (struct asahi_ccmd_submit_res *)ptr;
   ptr += req->extres_count * sizeof(struct asahi_ccmd_submit_res);

   if (ptr > end) {
      drm_err("invalid command buffer / extres array");
      return -EINVAL;
   }

   struct drm_asahi_sync *syncs =
      malloc(sizeof(struct drm_asahi_sync) * (2 + req->extres_count));

   struct drm_asahi_submit submit = {
      .flags = req->flags,
      .queue_id = req->queue_id,
      .cmdbuf = (uint64_t)(uintptr_t)cmdbuf,
      .cmdbuf_size = req->cmdbuf_size,
      .syncs = (uint64_t)(uintptr_t)syncs,
   };

   int *extres_fds = malloc(sizeof(int) * req->extres_count);

   int in_fence_fd = virgl_context_take_in_fence_fd(&dctx->base);

   if (in_fence_fd >= 0) {
      struct drm_asahi_sync in_sync = { .sync_type = DRM_ASAHI_SYNC_SYNCOBJ };
      ret = drmSyncobjCreate(dctx->fd, 0, &in_sync.handle);
      assert(ret == 0);
      ret = drmSyncobjImportSyncFile(dctx->fd, in_sync.handle, in_fence_fd);
      if (ret == 0) {
         syncs[submit.in_sync_count++] = in_sync;
      }
      close(in_fence_fd);
   }

   // Do the dance to get the in_syncs populated from external resources
   for (uint32_t i = 0; i < req->extres_count; i++) {
      extres_fds[i] = -1;

      if (!(extres[i].flags & (ASAHI_EXTRES_READ | ASAHI_EXTRES_WRITE)))
         continue;

      struct drm_object *obj = drm_context_get_object_from_res_id(dctx, extres[i].res_id);
      if (!obj || !to_asahi_object(obj)->exportable) {
         drm_log("invalid extres res_id %u (%s)", extres[i].res_id,
                 obj ? "not exportable" : "not found");
         continue;
      }

      ret = drmPrimeHandleToFD(dctx->fd, obj->handle, DRM_CLOEXEC | DRM_RDWR,
                               &extres_fds[i]);
      if (ret < 0 || extres_fds[i] == -1) {
         drm_log("failed to get dmabuf fd: %s", strerror(errno));
         continue;
      }

      if (!(extres[i].flags & ASAHI_EXTRES_READ))
         continue;

      struct dma_buf_export_sync_file export_sync_file_ioctl = {
         .flags = DMA_BUF_SYNC_RW,
         .fd = -1,
      };

      ret =
         drmIoctl(extres_fds[i], DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &export_sync_file_ioctl);
      if (ret < 0 || export_sync_file_ioctl.fd < 0) {
         drm_log("failed to export sync file: %s", strerror(errno));
         continue;
      }

      /* Create a new syncobj */
      uint32_t sync_handle;
      int ret = drmSyncobjCreate(dctx->fd, 0, &sync_handle);
      if (ret < 0) {
         drm_log("failed to create syncobj: %s", strerror(errno));
         close(export_sync_file_ioctl.fd);
         continue;
      }

      /* Import the sync file into it */
      ret = drmSyncobjImportSyncFile(dctx->fd, sync_handle, export_sync_file_ioctl.fd);
      close(export_sync_file_ioctl.fd);
      if (ret < 0) {
         drm_log("failed to import sync file into syncobj");
         drmSyncobjDestroy(dctx->fd, sync_handle);
         continue;
      }

      syncs[submit.in_sync_count++] = (struct drm_asahi_sync){
         .sync_type = DRM_ASAHI_SYNC_SYNCOBJ,
         .handle = sync_handle,
      };
   }

   struct drm_asahi_sync out_sync = { .sync_type = DRM_ASAHI_SYNC_SYNCOBJ };

   ret = drmSyncobjCreate(dctx->fd, 0, &out_sync.handle);
   if (ret == 0) {
      syncs[submit.in_sync_count + (submit.out_sync_count++)] = out_sync;
   } else {
      drm_dbg("out syncobj creation failed");
   }

   ret = drmIoctl(dctx->fd, DRM_IOCTL_ASAHI_SUBMIT, &submit);
   if (ret) {
      drm_log("DRM_IOCTL_ASAHI_SUBMIT failed: %d", ret);
   }

   for (unsigned i = 0; i < submit.in_sync_count; i++) {
      drmSyncobjDestroy(dctx->fd, syncs[i].handle);
   }

   if (ret == 0) {
      int submit_fd;
      ret = drmSyncobjExportSyncFile(dctx->fd, out_sync.handle, &submit_fd);
      if (ret == 0) {
         for (uint32_t i = 0; i < req->extres_count; i++) {
            if (extres_fds[i] < 0 || !(extres[i].flags & ASAHI_EXTRES_WRITE))
               continue;

            struct dma_buf_import_sync_file import_sync_file_ioctl = {
               .flags = DMA_BUF_SYNC_WRITE,
               .fd = submit_fd,
            };

            ret = drmIoctl(extres_fds[i], DMA_BUF_IOCTL_IMPORT_SYNC_FILE,
                           &import_sync_file_ioctl);
            if (ret < 0)
               drm_log("failed to import sync file into dmabuf");
         }

         drm_timeline_set_last_fence_fd(&actx->timelines[ring_idx - 1], submit_fd);
         drm_dbg("set last fd ring_idx: %d", submit_fd);
      } else {
         drm_log("failed to create a FD from the syncobj (%d)", ret);
      }
   } else {
      drm_log("command submission failed");
   }

   for (uint32_t i = 0; i < req->extres_count; i++)
      if (extres_fds[i] >= 0)
         close(extres_fds[i]);

   drmSyncobjDestroy(dctx->fd, out_sync.handle);
   free(syncs);
   free(extres_fds);
   return async_ret(actx, ret);
}

static const struct drm_ccmd ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [ASAHI_CCMD_##N] = { #N, asahi_ccmd_##n, sizeof(struct asahi_ccmd_##n##_req) }
   HANDLER(NOP, nop),
   HANDLER(IOCTL_SIMPLE, ioctl_simple),
   HANDLER(GET_PARAMS, get_params),
   HANDLER(GEM_NEW, gem_new),
   HANDLER(VM_BIND, vm_bind),
   HANDLER(SUBMIT, submit),
   HANDLER(GEM_BIND_OBJECT, gem_bind_object),
};

static int
asahi_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags, uint32_t ring_idx,
                            uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct asahi_context *actx = to_asahi_context(dctx);

   /* timeline is ring_idx-1 (because ring_idx 0 is host CPU timeline) */
   if (ring_idx > NR_TIMELINES) {
      drm_err("invalid ring_idx: %" PRIu32, ring_idx);
      return -EINVAL;
   }

   /* ring_idx zero is used for the guest to synchronize with host CPU,
    * meaning by the time ->submit_fence() is called, the fence has
    * already passed.. so just immediate signal:
    */
   if (ring_idx == 0 || actx->timelines[ring_idx - 1].last_fence_fd < 0) {
      vctx->fence_retire(vctx, ring_idx, fence_id);
      return 0;
   }

   return drm_timeline_submit_fence(&actx->timelines[ring_idx - 1], flags, fence_id);
}

struct virgl_context *
asahi_renderer_create(int fd, UNUSED size_t debug_len, UNUSED const char *debug_name)
{
   struct asahi_context *actx = calloc(1, sizeof(*actx));
   if (!actx)
      return NULL;

   if (!drm_context_init(&actx->base, fd, ccmd_dispatch, ARRAY_SIZE(ccmd_dispatch))) {
      free(actx);
      return NULL;
   }

   /* Indexed by submitqueue-id: */
   actx->queue_to_ring_idx = _mesa_hash_table_create_u32_keys(NULL);

   for (unsigned i = 0; i < NR_TIMELINES; i++) {
      unsigned ring_idx = i + 1; /* ring_idx 0 is host CPU */
      drm_timeline_init(&actx->timelines[i], &actx->base.base, "asahi-sync", ring_idx,
                        drm_context_fence_retire);
   }

   actx->base.base.destroy = asahi_renderer_destroy;
   actx->base.base.attach_resource = asahi_renderer_attach_resource;
   actx->base.base.export_opaque_handle = asahi_renderer_export_opaque_handle;
   actx->base.base.get_blob = asahi_renderer_get_blob;
   actx->base.base.submit_fence = asahi_renderer_submit_fence;
   actx->base.base.supports_fence_sharing = true;
   actx->base.free_object = asahi_renderer_free_object;
   actx->base.ccmd_alignment = 4;

   return &actx->base.base;
}
