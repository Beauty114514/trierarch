/*
 * Copyright 2025 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef I915_VIRTIO_PROTO_H_
#define I915_VIRTIO_PROTO_H_

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic error "-Wpadded"
#endif

#define I915_STATIC_ASSERT_SIZE(t) \
   static_assert(sizeof(struct t) % 8 == 0, "sizeof(struct " #t ") not multiple of 8"); \
   static_assert(alignof(struct t) <= 8, "alignof(struct " #t ") too large");

#define I915_DEFINE_CAST(parent, child)                                        \
   I915_STATIC_ASSERT_SIZE(child)                                              \
   DEFINE_CAST(parent, child)                                                  \

enum i915_ccmd {
   I915_CCMD_IOCTL_SIMPLE = 1,
   I915_CCMD_GETPARAM,
   I915_CCMD_QUERYPARAM,
   I915_CCMD_GEM_CREATE,
   I915_CCMD_GEM_CREATE_EXT,
   I915_CCMD_GEM_CONTEXT_CREATE,
   I915_CCMD_GEM_EXECBUFFER2,
   I915_CCMD_GEM_SET_MMAP_MODE,
   I915_CCMD_GEM_BUSY,
};

#define I915_CCMD(_cmd, _len) (struct vdrm_ccmd_req){    \
       .cmd = I915_CCMD_##_cmd,                          \
       .len = (_len),                                    \
   }

/**
 * Defines the layout of shmem buffer used for host->guest communication.
 */
struct i915_shmem {
   struct vdrm_shmem base;

   /**
    * Bitmask of banned GEM context IDs.
    */
   uint64_t banned_ctx_mask;
};
DEFINE_CAST(vdrm_shmem, i915_shmem)

/*
 * I915_CCMD_IOCTL_SIMPLE
 */
struct i915_ccmd_ioctl_simple_req {
   struct vdrm_ccmd_req hdr;

   uint32_t cmd;
   uint32_t pad;
   uint8_t payload[];
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_ioctl_simple_req)

struct i915_ccmd_ioctl_simple_rsp {
   struct vdrm_ccmd_rsp hdr;

   int32_t ret;
   uint8_t payload[];
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_ioctl_simple_rsp)

/*
 * I915_CCMD_GETPARAM
 */
struct i915_ccmd_getparam_req {
   struct vdrm_ccmd_req hdr;

   uint32_t param;
   uint32_t value;
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_getparam_req)

struct i915_ccmd_getparam_rsp {
   struct vdrm_ccmd_rsp hdr;
   uint32_t pad;

   int32_t ret;
   uint32_t value;
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_getparam_rsp)

/*
 * I915_CCMD_QUERYPARAM
 */
struct i915_ccmd_queryparam_req {
   struct vdrm_ccmd_req hdr;

   uint32_t query_id;
   uint32_t length;
   uint32_t flags;
   uint32_t pad;
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_queryparam_req)

struct i915_ccmd_queryparam_rsp {
   struct vdrm_ccmd_rsp hdr;
   uint32_t pad;

   int32_t ret;
   int32_t length;
   uint8_t payload[];
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_queryparam_rsp)

/*
 * I915_CCMD_GEM_CONTEXT_CREATE
 */
struct i915_ccmd_gem_context_create_req {
   struct vdrm_ccmd_req hdr;

   uint32_t flags;
   uint32_t params_size;
   uint8_t payload[];
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_context_create_req)

struct i915_ccmd_gem_context_create_rsp {
   struct vdrm_ccmd_rsp hdr;
   uint32_t pad;

   int32_t ret;
   uint32_t ctx_id;
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_gem_context_create_rsp)

/*
 * I915_CCMD_GEM_CREATE
 */
struct i915_ccmd_gem_create_req {
   struct vdrm_ccmd_req hdr;

   uint64_t size;
   uint32_t blob_id;
   uint32_t pad;
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_create_req)

/*
 * I915_CCMD_GEM_CREATE_EXT
 */
struct i915_ccmd_gem_create_ext_req {
   struct vdrm_ccmd_req hdr;

   uint64_t size;
   uint32_t blob_id;
   uint32_t gem_flags;
   uint32_t ext_size;
   uint32_t pad;
   uint8_t payload[];
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_create_ext_req)

/*
 * I915_CCMD_GEM_EXECBUFFER2
 */
struct i915_ccmd_gem_execbuffer2_req {
   struct vdrm_ccmd_req hdr;

   uint64_t flags;
   uint64_t context_id;
   uint32_t buffer_count;
   uint32_t batch_start_offset;
   uint32_t batch_len;
   uint32_t relocs_count;
   uint8_t payload[];
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_execbuffer2_req)

struct i915_ccmd_gem_execbuffer2_rsp {
   struct vdrm_ccmd_rsp hdr;
   int32_t ret;
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_gem_execbuffer2_rsp)

/*
 * I915_CCMD_GEM_SET_MMAP_MODE
 */
struct i915_ccmd_gem_set_mmap_mode_req {
   struct vdrm_ccmd_req hdr;

   uint32_t res_id;
   uint32_t flags;
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_set_mmap_mode_req)

/*
 * I915_CCMD_GEM_BUSY
 */
struct i915_ccmd_gem_busy_req {
   struct vdrm_ccmd_req hdr;

   uint32_t res_id;
   uint32_t pad;
};
I915_DEFINE_CAST(vdrm_ccmd_req, i915_ccmd_gem_busy_req)

struct i915_ccmd_gem_busy_rsp {
   struct vdrm_ccmd_rsp hdr;
   uint32_t pad;

   int32_t ret;
   int32_t busy;
};
I915_STATIC_ASSERT_SIZE(i915_ccmd_gem_busy_rsp)

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#endif /* I915_VIRTIO_PROTO_H_ */
