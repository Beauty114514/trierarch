/*
 * Copyright 2025 Collabora Limited
 * SPDX-License-Identifier: MIT
 */

#ifndef PANFROST_VIRTIO_PROTO_H_
#define PANFROST_VIRTIO_PROTO_H_

#include <stdint.h>

#include "drm_hw.h"

#define PANFROST_STATIC_ASSERT_SIZE(t) \
   static_assert(sizeof(struct t) % 8 == 0, "sizeof(struct " #t ") not multiple of 8"); \
   static_assert(alignof(struct t) <= 8, "alignof(struct " #t ") too large");

enum panfrost_ccmd {
   PANFROST_CCMD_SUBMIT = 1,
   PANFROST_CCMD_WAIT_BO,
   PANFROST_CCMD_CREATE_BO,
   PANFROST_CCMD_MMAP_BO,
   PANFROST_CCMD_GET_PARAM,
   PANFROST_CCMD_GET_BO_OFFSET,
   PANFROST_CCMD_MADVISE,
};

/*
 * PANFROST_CCMD_SUBMIT
 */
struct panfrost_ccmd_submit_req {
   struct vdrm_ccmd_req hdr;

   uint64_t jc;
   uint32_t requirements;
   uint32_t res_id_count;
   uint32_t payload[];
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_submit_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_submit_req)

/*
 * PANFROST_CCMD_WAIT_BO
 */
struct panfrost_ccmd_wait_bo_req {
   struct vdrm_ccmd_req hdr;

   uint32_t res_id;
   int64_t timeout_ns;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_wait_bo_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_wait_bo_req)

struct panfrost_ccmd_wait_bo_rsp {
   struct vdrm_ccmd_rsp hdr;

   int ret;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_wait_bo_rsp)

/*
 * PANFROST_CCMD_CREATE_BO
 */
struct panfrost_ccmd_create_bo_req {
   struct vdrm_ccmd_req hdr;

   uint32_t pad;
   uint32_t size;
   uint32_t blob_id;
   uint32_t flags;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_create_bo_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_create_bo_req)

/*
 * PANFROST_CCMD_MMAP_BO
 */
struct panfrost_ccmd_mmap_bo_req {
   struct vdrm_ccmd_req hdr;

   uint32_t res_id;
   uint32_t flags;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_mmap_bo_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_mmap_bo_req)

struct panfrost_ccmd_mmap_bo_rsp {
   struct vdrm_ccmd_rsp hdr;

   int ret;
   uint64_t offset;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_mmap_bo_rsp)

/*
 * PANFROST_CCMD_GET_PARAM
 */
struct panfrost_ccmd_get_param_req {
   struct vdrm_ccmd_req hdr;

   uint32_t pad;
   uint32_t param;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_get_param_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_get_param_req)

struct panfrost_ccmd_get_param_rsp {
   struct vdrm_ccmd_rsp hdr;

   int ret;
   uint64_t value;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_get_param_rsp)

/*
 * PANFROST_CCMD_GET_BO_OFFSET
 */
struct panfrost_ccmd_get_bo_offset_req {
   struct vdrm_ccmd_req hdr;

   uint32_t pad;
   uint32_t res_id;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_get_bo_offset_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_get_bo_offset_req)

struct panfrost_ccmd_get_bo_offset_rsp {
   struct vdrm_ccmd_rsp hdr;

   int ret;
   uint64_t offset;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_get_bo_offset_rsp)

/*
 * PANFROST_CCMD_MADVISE
 */
struct panfrost_ccmd_madvise_req {
   struct vdrm_ccmd_req hdr;

   uint32_t res_id;
   uint32_t madv;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_madvise_req)
DEFINE_CAST(vdrm_ccmd_req, panfrost_ccmd_madvise_req)

struct panfrost_ccmd_madvise_rsp {
   struct vdrm_ccmd_rsp hdr;

   uint32_t pad;
   int ret;
   uint32_t retained;
};
PANFROST_STATIC_ASSERT_SIZE(panfrost_ccmd_madvise_rsp)

#endif /* PANFROST_VIRTIO_PROTO_H_ */
