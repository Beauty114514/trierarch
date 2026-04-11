/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "virgl_fence.h"

#include "util/libsync.h"
#include "util/u_atomic.h"

#include "i915_ccmd.h"
#include "i915_object.h"

int intel_ioctl(int fd, unsigned long cmd, void *req)
{
   int err = drmIoctl(fd, cmd, req);

   if (err) {
      err = errno;

      drm_dbg("failed: cmd=0x%08lx ioc=0x%lx err=%d",
              cmd, _IOC_NR(cmd) - DRM_COMMAND_BASE, -err);
   }

   return err;
}

static int
i915_ccmd_ioctl_simple(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct i915_ccmd_ioctl_simple_req *req = to_i915_ccmd_ioctl_simple_req(hdr);
   struct i915_shmem *shmem = to_i915_shmem(dctx->shmem);
   uint32_t payload_len = _IOC_SIZE(req->cmd);
   size_t req_len = size_add(sizeof(*req), payload_len);

   if (hdr->len != req_len) {
      drm_err("%u != %zu", hdr->len, req_len);
      return -EINVAL;
   }

   /* Apply a reasonable upper bound on ioctl size: */
   if (payload_len > 128) {
      drm_err("invalid ioctl payload length: %u", payload_len);
      return -EINVAL;
   }

   struct i915_ccmd_ioctl_simple_rsp *rsp;
   size_t rsp_len = sizeof(*rsp);

   if (req->cmd & IOC_OUT)
      rsp_len = size_add(rsp_len, payload_len);

   rsp = drm_context_rsp(dctx, hdr, rsp_len);

   if (!rsp)
      return -ENOMEM;

   /* Allow-list of supported ioctls: */
   unsigned iocnr = _IOC_NR(req->cmd) - DRM_COMMAND_BASE;
   switch (req->cmd) {
   case DRM_IOCTL_I915_REG_READ:
   case DRM_IOCTL_I915_GEM_GET_APERTURE:
      break;
   case DRM_IOCTL_I915_GEM_SET_TILING:
   case DRM_IOCTL_I915_GEM_GET_TILING:
   case DRM_IOCTL_I915_GEM_SET_DOMAIN: {
      uint32_t *handle = (uint32_t*)req->payload;

      struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, *handle);
      if (!dobj) {
         drm_err("invalid res_id %u", *handle);
         rsp->ret = EINVAL;
         return 0;
      }

      *handle = dobj->handle;
      break;
   }
   case DRM_IOCTL_I915_GET_RESET_STATS:
   case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
      break;
   case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY: {
      uint32_t ctx_id = *(uint32_t *)req->payload;

      if (ctx_id < 64)
         p_atomic_set(&shmem->banned_ctx_mask,
                      shmem->banned_ctx_mask & ~(1ULL << ctx_id));
      break;
   }
   case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
   case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM: {
      const struct drm_i915_gem_context_param *param = (void *)req->payload;

      /* size of these parameters is fixed, param->size is unused for them */
      switch (param->param) {
      case I915_CONTEXT_PARAM_PRIORITY:
         if (param->value > I915_CONTEXT_DEFAULT_PRIORITY) {
            rsp->ret = EPERM;
            return 0;
         }
         break;
      case I915_CONTEXT_PARAM_RECOVERABLE:
      case I915_CONTEXT_PARAM_GTT_SIZE:
      case I915_CONTEXT_PARAM_VM:
         break;

      default:
         drm_err("unsupported ioctl param: %08x (%u)", req->cmd, iocnr);
         return EINVAL;
      }
      break;
   }
   case DRM_IOCTL_I915_GEM_VM_CREATE:
   case DRM_IOCTL_I915_GEM_VM_DESTROY:
      break;
   default:
      drm_err("invalid ioctl: %08x (0x%x)", req->cmd, iocnr);
      return -EINVAL;
   }

   rsp->ret = intel_ioctl(dctx->fd, req->cmd, req->payload);

   if (req->cmd & IOC_OUT)
      memcpy(rsp->payload, req->payload, payload_len);

   return 0;
}

static int
i915_ccmd_queryparam(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_queryparam_req *req = to_i915_ccmd_queryparam_req(hdr);
   struct i915_ccmd_queryparam_rsp *rsp;
   void *buffer = NULL;

   rsp = drm_context_rsp(dctx, hdr, size_add(sizeof(*rsp), req->length));
   if (!rsp)
      return -ENOMEM;

   if (req->length)
      buffer = rsp->payload;

   struct drm_i915_query_item item = {
      .query_id = req->query_id,
      .length = req->length,
      .flags = req->flags,
      .data_ptr = (uintptr_t)buffer,
   };

   struct drm_i915_query query = {
      .num_items = 1,
      .flags = 0,
      .items_ptr = (uintptr_t)&item,
   };

   rsp->ret = intel_ioctl(dctx->fd, DRM_IOCTL_I915_QUERY, &query);
   rsp->length = item.length;

   return 0;
}

static int
i915_ccmd_getparam(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_getparam_req *req = to_i915_ccmd_getparam_req(hdr);
   struct i915_ccmd_getparam_rsp *rsp;
   struct drm_i915_getparam gp;
   int value = -1;

   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   gp.param = req->param;
   gp.value = &value;

   rsp->ret = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GETPARAM, &gp);

   /* disable partial mapping unsupported by virtio-gpu */
   if (gp.param == I915_PARAM_MMAP_GTT_VERSION && value >= 5)
      value = 4;

   rsp->value = value;

   return 0;
}

static int
i915_ccmd_gem_create(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_gem_create_req *req = to_i915_ccmd_gem_create_req(hdr);

   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      drm_err("invalid blob_id %u", req->blob_id);
      return -EINVAL;
   }

   struct drm_i915_gem_create create = {
      .size = req->size,
   };

   int ret = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GEM_CREATE, &create);
   if (ret)
      return -ret;

   struct i915_object *obj = i915_object_create(create.handle, req->size);

   if (!obj)
      return -ENOMEM;

   drm_context_object_set_blob_id(dctx, &obj->base, req->blob_id);

   return 0;
}

static int
i915_ccmd_gem_create_ext(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_gem_create_ext_req *req = to_i915_ccmd_gem_create_ext_req(hdr);
   struct i915_user_extension *extension = (void *)req->payload;
   uintptr_t ptr = (uintptr_t)req->payload;
   int64_t ext_size = req->ext_size;
   size_t req_len = size_add(sizeof(*req), ext_size);

   if (hdr->len != req_len) {
      drm_err("%u != %zu", hdr->len, req_len);
      return -EINVAL;
   }

   if (!drm_context_blob_id_valid(dctx, req->blob_id)) {
      drm_err("invalid blob_id %u", req->blob_id);
      return -EINVAL;
   }

   struct drm_i915_gem_create_ext create = {
      .size = req->size,
      .flags = req->gem_flags,
      .extensions = ext_size ? ptr : 0,
   };

   bool wc_mapping = false;
   int pat_index = -1;

   while (ext_size > 0) {
      if (ext_size < (int)sizeof(*extension)) {
         drm_err("invalid extension size");
         return -EINVAL;
      }

      switch (extension->name) {
      case I915_GEM_CREATE_EXT_MEMORY_REGIONS:
      {
         struct drm_i915_gem_create_ext_memory_regions *mem_regions = (void*)extension;
         ptr += sizeof(*mem_regions);
         ext_size -= sizeof(*mem_regions);

         if (ext_size < 0)
            break;

         mem_regions->regions = ptr;
         ptr += size_mul(sizeof(struct drm_i915_gem_memory_class_instance), mem_regions->num_regions);
         ext_size -= size_mul(sizeof(struct drm_i915_gem_memory_class_instance), mem_regions->num_regions);

         if (ext_size < 0)
            break;

         struct drm_i915_gem_memory_class_instance *classes = (void*)mem_regions->regions;

         for (unsigned i = 0; i < mem_regions->num_regions; i++) {
            struct drm_i915_gem_memory_class_instance *class = classes + i;

            if (class->memory_class == I915_MEMORY_CLASS_DEVICE) {
               wc_mapping = true;
               break;
            }
         }
         break;
      }

      case I915_GEM_CREATE_EXT_PROTECTED_CONTENT:
         ext_size -= sizeof(struct drm_i915_gem_create_ext_protected_content);
         ptr += sizeof(struct drm_i915_gem_create_ext_protected_content);
         break;

      case I915_GEM_CREATE_EXT_SET_PAT: {
         struct drm_i915_gem_create_ext_set_pat *pat = (void*)ptr;

         ext_size -= sizeof(struct drm_i915_gem_create_ext_set_pat);
         ptr += sizeof(struct drm_i915_gem_create_ext_set_pat);

         if (ext_size >= 0)
            pat_index = pat->pat_index;
         break;
      }

      default:
         drm_err("invalid extension %u", extension->name);
         return -EINVAL;
      }

      if (ext_size > 0) {
         extension->next_extension = ptr;
         extension = (void*)ptr;
      } else {
         extension->next_extension = 0;
      }
   }

   if (ext_size) {
      drm_err("invalid extension size");
      return -EINVAL;
   }

   int ret = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GEM_CREATE_EXT, &create);
   if (ret)
      return -ret;

   struct i915_object *obj = i915_object_create(create.handle, req->size);

   if (!obj)
      return -ENOMEM;

   drm_context_object_set_blob_id(dctx, &obj->base, req->blob_id);

   if (pat_index > -1) {
      obj->mmap_configured = true;

      switch (pat_index) {
      default: {
         static bool warned;

         if (!warned) {
            drm_err("unsupported pat_index=%d, falling back to WB mapping", pat_index);
            warned = true;
         }
      }
      /* fallthrough */
      case 0:
         obj->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;
         break;
      case 1:
         obj->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
         break;
      }
   } else if (wc_mapping) {
      obj->mmap_configured = true;
      obj->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
   }

   return 0;
}

static int
i915_ccmd_gem_context_create(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct i915_ccmd_gem_context_create_req *req = to_i915_ccmd_gem_context_create_req(hdr);
   struct drm_i915_gem_context_create_ext_setparam *setparam = (void *)req->payload;
   uintptr_t ptr = (uintptr_t)req->payload;
   int64_t params_size = req->params_size;
   size_t req_len;

   req_len = size_add(sizeof(*req), params_size);

   if (hdr->len != req_len) {
      drm_err("%u != %zu", hdr->len, req_len);
      return -EINVAL;
   }

   struct i915_ccmd_gem_context_create_rsp *rsp;

   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_i915_gem_context_create_ext create = {
      .flags = req->flags,
      .extensions = (uintptr_t)setparam,
   };

   while (params_size > 0) {
      if (params_size < (int)sizeof(*setparam)) {
         drm_err("invalid params_size");
         return -EINVAL;
      }

      switch (setparam->param.param) {
      case I915_CONTEXT_PARAM_PRIORITY:
         if (setparam->param.value > I915_CONTEXT_DEFAULT_PRIORITY) {
            rsp->ret = EPERM;
            return 0;
         }
         break;
      case I915_CONTEXT_PARAM_NO_ERROR_CAPTURE:
      case I915_CONTEXT_PARAM_BANNABLE:
      case I915_CONTEXT_PARAM_SSEU:
      case I915_CONTEXT_PARAM_RECOVERABLE:
      case I915_CONTEXT_PARAM_VM:
      case I915_CONTEXT_PARAM_ENGINES:
      case I915_CONTEXT_PARAM_PERSISTENCE:
      case I915_CONTEXT_PARAM_PROTECTED_CONTENT:
         break;

      default:
         drm_err("invalid param %llu", setparam->param.param);
         return -EINVAL;
      }

      ptr += sizeof(*setparam);

      if ((setparam->param.size % 4) || setparam->param.size > 128) {
         drm_err("invalid setparam->param.size");
         return -EINVAL;
      }

      if (setparam->param.size) {
         setparam->param.value = ptr;
         ptr += setparam->param.size;
      }

      params_size -= sizeof(*setparam) + setparam->param.size;

      if (params_size > 0) {
         setparam->base.next_extension = ptr;
         setparam = (void*)ptr;
      } else {
         setparam->base.next_extension = 0;
      }
   }

   if (params_size) {
      drm_err("invalid params_size");
      return -EINVAL;
   }

   rsp->ret = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &create);
   rsp->ctx_id = create.ctx_id;

   return 0;
}

static int
i915_ccmd_gem_execbuffer2(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   struct i915_context *ictx = to_i915_context(dctx);
   struct i915_shmem *shmem = to_i915_shmem(dctx->shmem);
   const struct i915_ccmd_gem_execbuffer2_req *req = to_i915_ccmd_gem_execbuffer2_req(hdr);

   if (hdr->len < sizeof(*req)) {
      drm_err("%u < %zu", hdr->len, sizeof(*req));
      return -EINVAL;
   }

   size_t buffers_size = size_mul(sizeof(struct drm_i915_gem_exec_object2),
                                  req->buffer_count);

   size_t relocs_size = size_mul(sizeof(struct drm_i915_gem_relocation_entry),
                                 req->relocs_count);

   size_t req_len = size_add(sizeof(*req), buffers_size);
          req_len = size_add(req_len, relocs_size);

   if (hdr->len != req_len) {
      drm_err("%u != %zu", hdr->len, req_len);
      return -EINVAL;
   }

   size_t payload_len = buffers_size + relocs_size;
   void *payload = malloc(payload_len);
   if (!payload) {
      drm_err("invalid payload_len %zu", payload_len);
      return -EINVAL;
   }

   memcpy(payload, req->payload, payload_len);

   struct drm_i915_gem_relocation_entry *payload_reloc = (void*)((char*)payload + buffers_size);
   struct drm_i915_gem_exec_object2 *buffers = payload;
   struct i915_ccmd_gem_execbuffer2_rsp *rsp;

   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp) {
      free(payload);
      return -ENOMEM;
   }

   unsigned relocs_count = 0;

   for (unsigned i = 0; i < req->buffer_count; i++) {
      struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, buffers[i].handle);
      if (!dobj) {
         drm_err("invalid res_id %u", buffers[i].handle);
         rsp->ret = EINVAL;
         free(payload);
         return 0;
      }

      buffers[i].handle = dobj->handle;

      if (buffers[i].relocation_count) {
         if (size_add(relocs_count, buffers[i].relocation_count) > req->relocs_count) {
            drm_err("invalid relocation_count");
            rsp->ret = EINVAL;
            free(payload);
            return 0;
         }
         buffers[i].relocs_ptr = (uintptr_t)payload_reloc;
         payload_reloc += buffers[i].relocation_count;
         relocs_count += buffers[i].relocation_count;
      }
   }

   struct drm_i915_gem_execbuffer2 exec = {0};

   /*
    * Assume there is one actively used context at a time.
    *
    * If this ever changes, guest kernel VirtIO-GPU UAPI will need to be extended
    * to support logical sub-contexts. VirtIO-GPU supports one context per DRM FD.
    */
   exec.rsvd1 = req->context_id;
   exec.buffers_ptr = (uintptr_t)buffers;
   exec.buffer_count = req->buffer_count;
   exec.batch_start_offset = req->batch_start_offset;
   exec.batch_len = req->batch_len;
   exec.flags = req->flags;

   int ring_idx = 1 + (exec.flags & I915_EXEC_RING_MASK);

   if (ring_idx >= (int)ARRAY_SIZE(ictx->timelines)) {
      free(payload);
      return -EINVAL;
   }

   if (!ictx->timelines[ring_idx]) {
      ictx->timelines[ring_idx] = calloc(1, sizeof(*ictx->timelines[ring_idx]));
      if (!ictx->timelines[ring_idx]) {
         free(payload);
         return -ENOMEM;
      }

      drm_timeline_init(ictx->timelines[ring_idx], &dctx->base, "intel-sync",
                        ring_idx, drm_context_fence_retire);
   }

   int in_fence_fd = virgl_context_take_in_fence_fd(&dctx->base);

   if (in_fence_fd > -1) {
      exec.rsvd2 |= in_fence_fd;
      exec.flags |= I915_EXEC_FENCE_IN;
   }

   exec.flags |= I915_EXEC_FENCE_OUT;

   int err = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, &exec);
   rsp->ret = err;

   if (!err)
      drm_timeline_set_last_fence_fd(ictx->timelines[ring_idx], exec.rsvd2 >> 32);

   if (err == EIO && req->context_id < 64)
      p_atomic_set(&shmem->banned_ctx_mask,
                   shmem->banned_ctx_mask | (1ULL << req->context_id));

   close(in_fence_fd);

   free(payload);

   return 0;
}

static int
i915_ccmd_gem_set_mmap_mode(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_gem_set_mmap_mode_req *req = to_i915_ccmd_gem_set_mmap_mode_req(hdr);

   struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, req->res_id);
   if (!dobj) {
      drm_err("invalid res_id %u", req->res_id);
      return 0;
   }

   struct virgl_resource *res = virgl_resource_lookup(req->res_id);
   if (!res) {
      drm_err("invalid res_id %u", req->res_id);
      return -EINVAL;
   }

   struct i915_object *obj = to_i915_object(dobj);

   if (!obj->mmap_configured && res->mapped) {
      drm_dbg("res_id %u already mapped", req->res_id);
      obj->mmap_configured = true;
      return 0;
   }

   int map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;

   switch (req->flags) {
   case I915_MMAP_OFFSET_GTT:
   case I915_MMAP_OFFSET_WC:
   case I915_MMAP_OFFSET_FIXED:
      map_info = VIRGL_RENDERER_MAP_CACHE_WC;
      break;
   case I915_MMAP_OFFSET_WB:
      break;
   case I915_MMAP_OFFSET_UC:
      map_info = VIRGL_RENDERER_MAP_CACHE_UNCACHED;
      break;
   default:
      drm_err("invalid mmap_flags 0x%x", req->flags);
      return -EINVAL;
   }

   if (obj->mmap_configured && obj->map_info != map_info) {
      drm_dbg("mmap_flags mismatch obj->map_info 0x%x map_info 0x%x",
              obj->map_info, map_info);
      return 0;
   }

   obj->mmap_configured = true;
   obj->map_info = map_info;
   res->map_info = map_info;

   return 0;
}

static int
i915_ccmd_gem_busy(struct drm_context *dctx, struct vdrm_ccmd_req *hdr)
{
   const struct i915_ccmd_gem_busy_req *req = to_i915_ccmd_gem_busy_req(hdr);

   struct i915_ccmd_gem_busy_rsp *rsp;

   rsp = drm_context_rsp(dctx, hdr, sizeof(*rsp));
   if (!rsp)
      return -ENOMEM;

   struct drm_object *dobj = drm_context_get_object_from_res_id(dctx, req->res_id);
   if (!dobj) {
      drm_err("invalid res_id %u", req->res_id);
      rsp->ret = EINVAL;
      return 0;
   }

   struct drm_i915_gem_busy gem_busy = { .handle = dobj->handle };

   int err = intel_ioctl(dctx->fd, DRM_IOCTL_I915_GEM_BUSY, &gem_busy);
   if (err) {
      rsp->ret = err;
      return 0;
   }

   rsp->busy = gem_busy.busy;

   return 0;
}

const struct drm_ccmd i915_ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [I915_CCMD_##N] = {#N, i915_ccmd_##n, sizeof(struct i915_ccmd_##n##_req)}
   HANDLER(IOCTL_SIMPLE, ioctl_simple),
   HANDLER(GETPARAM, getparam),
   HANDLER(QUERYPARAM, queryparam),
   HANDLER(GEM_CREATE, gem_create),
   HANDLER(GEM_CREATE_EXT, gem_create_ext),
   HANDLER(GEM_CONTEXT_CREATE, gem_context_create),
   HANDLER(GEM_EXECBUFFER2, gem_execbuffer2),
   HANDLER(GEM_SET_MMAP_MODE, gem_set_mmap_mode),
   HANDLER(GEM_BUSY, gem_busy),
};

const unsigned int i915_ccmd_dispatch_size = ARRAY_SIZE(i915_ccmd_dispatch);
