/*
 * Copyright 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "virglrenderer.h"

#include "i915_ccmd.h"
#include "i915_object.h"
#include "i915_resource.h"
#include "i915_renderer.h"

/**
 * Probe capset params.
 */
int
i915_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset)
{
   drmDevicePtr drmdev = NULL;

   if (drmGetDevice2(fd, DRM_DEVICE_GET_PCI_REVISION, &drmdev)) {
      drm_dbg("failed to query drm device: %d", -errno);
      return -EINVAL;
   }

   capset->u.intel.pci_bus = drmdev->businfo.pci->bus;
   capset->u.intel.pci_dev = drmdev->businfo.pci->dev;
   capset->u.intel.pci_func = drmdev->businfo.pci->func;
   capset->u.intel.pci_domain = drmdev->businfo.pci->domain;
   capset->u.intel.pci_device_id = drmdev->deviceinfo.pci->device_id;
   capset->u.intel.pci_revision_id = drmdev->deviceinfo.pci->revision_id;

   capset->wire_format_version = 1;

   drm_dbg("wire_format_version: %u", capset->wire_format_version);
   drm_dbg("version_major:       %u", capset->version_major);
   drm_dbg("version_minor:       %u", capset->version_minor);
   drm_dbg("version_patchlevel:  %u", capset->version_patchlevel);
   drm_dbg("pci_bus:             %u", capset->u.intel.pci_bus);
   drm_dbg("pci_dev:             %u", capset->u.intel.pci_dev);
   drm_dbg("pci_func:            %u", capset->u.intel.pci_func);
   drm_dbg("pci_domain:          %u", capset->u.intel.pci_domain);
   drm_dbg("pci_device_id:       %u", capset->u.intel.pci_device_id);
   drm_dbg("pci_revision_id:     %u", capset->u.intel.pci_revision_id);

   drmFreeDevice(&drmdev);

   return 0;
}

static void
i915_renderer_destroy(struct virgl_context *vctx)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct i915_context *ictx = to_i915_context(dctx);

   for (unsigned i = 0; i < ARRAY_SIZE(ictx->timelines); i++) {
      if (ictx->timelines[i]) {
         drm_timeline_fini(ictx->timelines[i]);
         free(ictx->timelines[i]);
      }
   }

   drm_context_deinit(dctx);

   free(ictx);
}

static int
i915_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags,
                           uint32_t queue_id, uint64_t fence_id)
{
   struct drm_context *dctx = to_drm_context(vctx);
   struct i915_context *ictx = to_i915_context(dctx);

   if (queue_id >= ARRAY_SIZE(ictx->timelines)) {
      drm_err("invalid queue_id: %u", queue_id);
      return -EINVAL;
   }

   if (!ictx->timelines[queue_id] || ictx->timelines[queue_id]->last_fence_fd < 0) {
      vctx->fence_retire(vctx, queue_id, fence_id);
      return 0;
   }

   return drm_timeline_submit_fence(ictx->timelines[queue_id], flags, fence_id);
}

struct virgl_context *
i915_renderer_create(int fd, UNUSED size_t debug_len, UNUSED const char *debug_name)
{
   struct i915_context *ictx;

   ictx = calloc(1, sizeof(*ictx));
   if (!ictx)
      return NULL;

   drm_context_init(&ictx->base, fd, i915_ccmd_dispatch, i915_ccmd_dispatch_size);

   ictx->base.base.destroy = i915_renderer_destroy;
   ictx->base.base.attach_resource = i915_renderer_attach_resource;
   ictx->base.base.export_opaque_handle = i915_renderer_export_opaque_handle;
   ictx->base.base.get_blob = i915_renderer_get_blob;
   ictx->base.base.submit_fence = i915_renderer_submit_fence;
   ictx->base.base.supports_fence_sharing = true;
   ictx->base.free_object = i915_renderer_free_object;
   ictx->base.ccmd_alignment = 4;

   return &ictx->base.base;
}
