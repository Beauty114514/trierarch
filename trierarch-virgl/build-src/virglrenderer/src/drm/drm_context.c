/*
 * Copyright 2022 Google LLC
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "util/anon_file.h"
#include "util/hash_table.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/u_atomic.h"

#include "drm_context.h"
#include "drm_util.h"

static int
drm_context_get_fencing_fd(UNUSED struct virgl_context *vctx)
{
   /* VIRGL_RENDERER_ASYNC_FENCE_CB is required */
   return -1;
}

static void
drm_context_retire_fences(UNUSED struct virgl_context *vctx)
{
   /* No-op as VIRGL_RENDERER_ASYNC_FENCE_CB is required */
}

static int
drm_context_transfer_3d(UNUSED struct virgl_context *vctx,
                        UNUSED struct virgl_resource *res,
                        UNUSED const struct vrend_transfer_info *info,
                        UNUSED int transfer_mode)
{
   drm_log("unsupported");
   return -1;
}

static void
drm_context_unmap_shmem_blob(struct drm_context *dctx)
{
   if (!dctx->shmem)
      return;

   uint32_t blob_size = dctx->rsp_mem_sz + dctx->shmem->rsp_mem_offset;

   munmap(dctx->shmem, blob_size);

   dctx->shmem = NULL;
   dctx->rsp_mem = NULL;
   dctx->rsp_mem_sz = 0;
}

static int
drm_context_submit_cmd_dispatch(struct drm_context *dctx, const struct vdrm_ccmd_req *hdr)
{
   int ret;

   if (hdr->cmd >= dctx->dispatch_size) {
      drm_err("invalid cmd: %u", hdr->cmd);
      return -EINVAL;
   }

   const struct drm_ccmd *ccmd = &dctx->ccmd_dispatch[hdr->cmd];

   if (!ccmd->handler) {
      drm_err("no handler: %u", hdr->cmd);
      return -EINVAL;
   }

   drm_dbg("%s: hdr={cmd=%u, len=%u, seqno=%u, rsp_off=0x%x)", ccmd->name, hdr->cmd,
           hdr->len, hdr->seqno, hdr->rsp_off);

   /* copy request to let ccmd handler patch command in-place */
   size_t ccmd_size = MAX2(ccmd->size, hdr->len);
   uint8_t *buf = malloc(ccmd_size);
   if (!buf)
      return -ENOMEM;

   memcpy(&buf[0], hdr, hdr->len);

   /* Request length from the guest can be smaller than the expected
    * size, ie. newer host and older guest, we need to zero initialize
    * the new fields at the end.
    */
   if (ccmd->size > hdr->len)
      memset(&buf[hdr->len], 0, ccmd->size - hdr->len);

   struct vdrm_ccmd_req *ccmd_hdr = (struct vdrm_ccmd_req *)buf;

   void *trace_scope = TRACE_SCOPE_BEGIN(ccmd->name);

   ret = ccmd->handler(dctx, ccmd_hdr);

   TRACE_SCOPE_END(trace_scope);

   free(buf);

   if (ret) {
      drm_err("%s: dispatch failed: %d (%s)", ccmd->name, ret, strerror(errno));
      return ret;
   }

   /* Commands with no response, like SET_DEBUGINFO, could be sent before
    * the shmem buffer is allocated.
    */
   if (!dctx->shmem)
      return 0;

   /* If the response length from the guest is smaller than the
    * expected size, ie. newer host and older guest, then a shadow
    * copy is used, and we need to copy back to the actual rsp
    * buffer.
    */
   struct vdrm_ccmd_rsp *rsp = (struct vdrm_ccmd_rsp *)&dctx->rsp_mem[hdr->rsp_off];
   if (dctx->current_rsp) {
      uint32_t len = *(volatile uint32_t *)&rsp->len;
      len = MIN2(len, dctx->current_rsp->len);
      memcpy(rsp, dctx->current_rsp, len);
      rsp->len = len;
      free(dctx->current_rsp);
   }
   dctx->current_rsp = NULL;

   p_atomic_xchg(&dctx->shmem->seqno, hdr->seqno);

   return 0;
}

static int
drm_context_submit_cmd(struct virgl_context *vctx, const void *_buffer, size_t size)
{
   struct drm_context *dctx = to_drm_context(vctx);
   unsigned int alignment = dctx->ccmd_alignment;
   const uint8_t *buffer = _buffer;

   /* Check for bad alignment requirements in debug builds only */
   assert(alignment == 4 || alignment == 8);

   if (size > UINT32_MAX) {
      drm_err("bad size, %zu too big", size);
      return -EINVAL;
   }

   while (size >= sizeof(struct vdrm_ccmd_req)) {
      const struct vdrm_ccmd_req *hdr = (const struct vdrm_ccmd_req *)buffer;

      /* Sanity check first: */
      if ((hdr->len > size) || (hdr->len < sizeof(*hdr)) ||
          (hdr->len & (alignment - 1))) {
         drm_err("bad size, %u vs %zu (cmd %u, min alignment %u)",
                 hdr->len, size, hdr->cmd, alignment);
         return -EINVAL;
      }

      if (hdr->rsp_off & (alignment - 1)) {
         drm_err("bad rsp_off, %u, min alignment %u",
                 hdr->rsp_off, alignment);
         return -EINVAL;
      }

      int ret = drm_context_submit_cmd_dispatch(dctx, hdr);
      if (ret) {
         drm_err("dispatch failed: %d (%u)", ret, hdr->cmd);
         return ret;
      }

      buffer += hdr->len;
      size -= hdr->len;
   }

   if (size > 0) {
      drm_err("bad size, %zu trailing bytes", size);
      return -EINVAL;
   }

   return 0;
}

static void
drm_context_remove_object(struct drm_context *dctx, struct drm_object *obj)
{
   if (drm_context_res_id_unused(dctx, obj->res_id))
      return;

   _mesa_hash_table_remove_key(dctx->resource_table, (void *)(uintptr_t)obj->res_id);
}

static void
drm_context_free_object(struct drm_context *dctx, struct drm_object *dobj)
{
   drm_context_remove_object(dctx, dobj);

   dctx->free_object(dctx, dobj);
}

static void
drm_context_detach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct drm_object *obj = drm_context_get_object_from_res_id(dctx, res->res_id);

   drm_dbg("obj=%p, res_id=%u", (void*)obj, res->res_id);

   if (!obj) {
      /* If this context doesn't know about this resource id there's nothing to do. */
       return;
   }

   drm_dbg("obj=%p, blob_id=%u, res_id=%u", (void*)obj, obj->blob_id, obj->res_id);

   drm_context_free_object(dctx, obj);
}

static int
drm_context_get_device_fd(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);

   return dctx->fd;
}

bool
drm_context_init(struct drm_context *dctx, int fd,
                 const struct drm_ccmd *ccmd_dispatch, unsigned int dispatch_size)
{
   /* Indexed by res_id: */
   dctx->resource_table = _mesa_hash_table_create_u32_keys(NULL);
   if (dctx->resource_table == NULL)
      return false;

   /* Indexed by blob_id, but only lower 32b of blob_id are used: */
   dctx->blob_table = _mesa_hash_table_create_u32_keys(NULL);
   if (dctx->blob_table == NULL)
      goto fail_blob_table;

   dctx->ccmd_dispatch = ccmd_dispatch;
   dctx->dispatch_size = dispatch_size;
   dctx->fd = fd;

   /* 8 bytes by default */
   dctx->ccmd_alignment = 8;

   dctx->base.submit_cmd = drm_context_submit_cmd;
   dctx->base.transfer_3d = drm_context_transfer_3d;
   dctx->base.get_fencing_fd = drm_context_get_fencing_fd;
   dctx->base.retire_fences = drm_context_retire_fences;
   dctx->base.detach_resource = drm_context_detach_resource;
   dctx->base.get_device_fd = drm_context_get_device_fd;

   return true;
fail_blob_table:
   _mesa_hash_table_destroy(dctx->resource_table, NULL);
   dctx->resource_table = NULL;
   return false;
}

static void
free_blob(const void *blob_id, UNUSED void *_obj, void *_dctx)
{
   struct drm_context *dctx = _dctx;
   struct drm_object *obj = drm_context_retrieve_object_from_blob_id(dctx,
                                                                     (uintptr_t)blob_id);

   assert(obj == _obj);

   if (obj)
      drm_context_free_object(dctx, obj);
}

static void
free_resource(const void *res_id, UNUSED void *_obj, void *_dctx)
{
   struct drm_context *dctx = _dctx;
   struct drm_object *obj = drm_context_get_object_from_res_id(dctx, (uintptr_t)res_id);

   assert(obj == _obj);

   if (obj)
      drm_context_free_object(dctx, obj);
}

void
drm_context_deinit(struct drm_context *dctx)
{
   drm_context_unmap_shmem_blob(dctx);

   if (dctx->free_object) {
      hash_table_call_foreach(dctx->blob_table, free_blob, dctx);
      hash_table_call_foreach(dctx->resource_table, free_resource, dctx);
   }

   /* sanity-check for a leaked resources */
   assert(!dctx->resource_table->entries);
   assert(!dctx->blob_table->entries);

   _mesa_hash_table_destroy(dctx->resource_table, NULL);
   _mesa_hash_table_destroy(dctx->blob_table, NULL);

   close(dctx->fd);
}

void *
drm_context_rsp(struct drm_context *dctx, const struct vdrm_ccmd_req *hdr,
                size_t len)
{
   size_t rsp_mem_sz = dctx->rsp_mem_sz;
   size_t off = hdr->rsp_off;

   if ((off > rsp_mem_sz) || (len > rsp_mem_sz - off)) {
      drm_err("invalid shm offset: off=%zu, len=%zu (shmem_size=%zu)",
              off, len, rsp_mem_sz);
      return NULL;
   }

   /* The shared buffer might be writable by the guest.  To avoid TOCTOU,
    * data races, and other security problems, always allocate a shadow buffer.
    *
    * Zero it to ensure that uninitialized heap memory cannot be exposed to guests.
    */
   struct vdrm_ccmd_rsp *rsp = calloc(len, 1);
   if (!rsp)
      return NULL;
   rsp->len = len;
   dctx->current_rsp = rsp;

   return rsp;
}

void
drm_context_fence_retire(struct virgl_context *vctx,
                         uint32_t ring_idx,
                         uint64_t fence_id)
{
   vctx->fence_retire(vctx, ring_idx, fence_id);
}

int
drm_context_get_shmem_blob(struct drm_context *dctx,
                           const char *name, size_t shmem_size, uint64_t blob_size,
                           uint32_t blob_flags, struct virgl_context_blob *blob)
{
   int fd;

   if (blob_flags != VIRGL_RENDERER_BLOB_FLAG_USE_MAPPABLE) {
      drm_err("invalid blob_flags: 0x%x", blob_flags);
      return -EINVAL;
   }

   if (dctx->shmem) {
      drm_err("there can be only one!");
      return -EINVAL;
   }

   if ((blob_size < sizeof(*dctx->shmem)) || (blob_size > UINT32_MAX)) {
      drm_err("invalid blob size 0x%" PRIx64, blob_size);
      return -EINVAL;
   }

   fd = os_create_anonymous_file(blob_size, name);
   if (fd < 0) {
      drm_err("failed to create shmem file: %s", strerror(errno));
      return -ENOMEM;
   }

#if defined(HAVE_MEMFD_CREATE)
   int ret = fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW);
   if (ret) {
      drm_err("fcntl failed: %s", strerror(errno));
      close(fd);
      return -ENOMEM;
   }
#endif

   dctx->shmem = mmap(NULL, blob_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
   if (dctx->shmem == MAP_FAILED) {
      drm_err("shmem mmap failed: %s", strerror(errno));
      dctx->shmem = NULL;
      close(fd);
      return -ENOMEM;
   }

   dctx->shmem->rsp_mem_offset = shmem_size;

   uint8_t *ptr = (uint8_t *)dctx->shmem;
   dctx->rsp_mem = &ptr[dctx->shmem->rsp_mem_offset];
   dctx->rsp_mem_sz = blob_size - dctx->shmem->rsp_mem_offset;

   blob->u.fd = fd;
   blob->type = VIRGL_RESOURCE_FD_SHM;
   blob->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;

   return 0;
}

bool
drm_context_blob_id_valid(struct drm_context *dctx, uint32_t blob_id)
{
   /* must be non-zero: */
   if (blob_id == 0)
      return false;

   /* must not already be in-use: */
   if (hash_table_search(dctx->blob_table, blob_id))
      return false;

   return true;
}

struct drm_object *
drm_context_retrieve_object_from_blob_id(struct drm_context *dctx, uint64_t blob_id)
{
   assert((blob_id >> 32) == 0);

   if ((blob_id >> 32) != 0)
      return NULL;

   uint32_t id = blob_id;
   struct hash_entry *entry = hash_table_search(dctx->blob_table, id);
   if (!entry)
      return NULL;

   struct drm_object *obj = entry->data;
   _mesa_hash_table_remove(dctx->blob_table, entry);

   return obj;
}

void
drm_context_object_set_blob_id(struct drm_context *dctx,
                               struct drm_object *obj,
                               uint32_t blob_id)
{
   assert(drm_context_blob_id_valid(dctx, blob_id));

   obj->blob_id = blob_id;
   _mesa_hash_table_insert(dctx->blob_table, (void *)(uintptr_t)obj->blob_id, obj);
}

void
drm_context_object_set_res_id(struct drm_context *dctx,
                              struct drm_object *obj,
                              uint32_t res_id)
{
   assert(drm_context_res_id_unused(dctx, res_id));

   obj->res_id = res_id;
   _mesa_hash_table_insert(dctx->resource_table, (void *)(uintptr_t)obj->res_id, obj);
}

struct drm_object *
drm_context_get_object_from_res_id(struct drm_context *dctx, uint32_t res_id)
{
   const struct hash_entry *entry = hash_table_search(dctx->resource_table, res_id);
   return likely(entry) ? entry->data : NULL;
}

bool
drm_context_res_id_unused(struct drm_context *dctx, uint32_t res_id)
{
   return !hash_table_search(dctx->resource_table, res_id);
}
