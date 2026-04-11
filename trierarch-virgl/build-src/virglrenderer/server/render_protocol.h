/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef RENDER_PROTOCOL_H
#define RENDER_PROTOCOL_H

#include <stdint.h>

#include "virgl_resource.h"

/* this covers the command line options and the socket type */
#define RENDER_SERVER_VERSION 0

/* The protocol itself is internal to virglrenderer.  There is no backward
 * compatibility to be kept.
 */

/* client ops, which are handled by the server process */
enum render_client_op {
   RENDER_CLIENT_OP_NOP = 0,
   RENDER_CLIENT_OP_INIT,
   RENDER_CLIENT_OP_RESET,
   RENDER_CLIENT_OP_CREATE_CONTEXT,
   RENDER_CLIENT_OP_DESTROY_CONTEXT,

   RENDER_CLIENT_OP_COUNT,
};

/* context ops, which are handled by workers (subprocesses or threads) created
 * by the server process
 */
enum render_context_op {
   RENDER_CONTEXT_OP_NOP = 0,
   RENDER_CONTEXT_OP_INIT,
   RENDER_CONTEXT_OP_CREATE_RESOURCE,
   RENDER_CONTEXT_OP_IMPORT_RESOURCE,
   RENDER_CONTEXT_OP_DESTROY_RESOURCE,
   RENDER_CONTEXT_OP_SUBMIT_CMD,
   RENDER_CONTEXT_OP_SUBMIT_FENCE,

   RENDER_CONTEXT_OP_COUNT,
};

struct render_client_op_header {
   enum render_client_op op;
};

struct render_client_op_nop_request {
   struct render_client_op_header header;
};

/* Initialize virglrenderer.
 *
 * This roughly corresponds to virgl_renderer_init.
 */
struct render_client_op_init_request {
   struct render_client_op_header header;
   uint32_t flags; /* VIRGL_RENDERER_USE_* and others */
};

/* Remove all contexts.
 *
 * This roughly corresponds to virgl_renderer_reset.
 */
struct render_client_op_reset_request {
   struct render_client_op_header header;
};

/* Create a context, which will be serviced by a worker.
 *
 * See also the comment before main() for the process model.
 *
 * This roughly corresponds to virgl_renderer_context_create_with_flags.
 */
struct render_client_op_create_context_request {
   struct render_client_op_header header;
   uint32_t ctx_id;
   char ctx_name[32];
};

struct render_client_op_create_context_reply {
   bool ok;
   /* followed by 1 socket fd if ok */
};

/* Destroy a context, including the worker.
 *
 * This roughly corresponds to virgl_renderer_context_destroy.
 */
struct render_client_op_destroy_context_request {
   struct render_client_op_header header;
   uint32_t ctx_id;
};

union render_client_op_request {
   struct render_client_op_header header;
   struct render_client_op_nop_request nop;
   struct render_client_op_init_request init;
   struct render_client_op_reset_request reset;
   struct render_client_op_create_context_request create_context;
   struct render_client_op_destroy_context_request destroy_context;
};

struct render_context_op_header {
   enum render_context_op op;
};

struct render_context_op_nop_request {
   struct render_context_op_header header;
};

/* Initialize the context.
 *
 * The shmem is required and currently holds an array of atomic_uint.  Each
 * atomic_uint represents the current sequence number of a ring (as defined by
 * the virtio-gpu spec).
 *
 * The eventfd is optional.  When given, it will be written to when there are
 * changes to any of the sequence numbers.
 *
 * This roughly corresponds to virgl_renderer_context_create_with_flags.
 */
struct render_context_op_init_request {
   struct render_context_op_header header;
   uint32_t flags; /* VIRGL_RENDERER_CONTEXT_FLAG_*/
   size_t shmem_size;
   /* followed by 1 shmem fd and optionally 1 eventfd */
};

/* Export a blob resource from the context
 *
 * This roughly corresponds to:
 * - virgl_renderer_resource_create_blob
 * - virgl_renderer_resource_get_map_info
 * - virgl_renderer_resource_export_blob
 * - virgl_renderer_ctx_attach_resource
 */
struct render_context_op_create_resource_request {
   struct render_context_op_header header;
   uint32_t res_id;
   uint64_t blob_id;
   uint64_t blob_size;
   uint32_t blob_flags; /* VIRGL_RENDERER_BLOB_FLAG_* */
};

struct render_context_op_create_resource_reply {
   enum virgl_resource_fd_type fd_type;
   uint32_t map_info; /* VIRGL_RENDERER_MAP_* */
   /* vulkan_info is set if the fd_type is VIRGL_RESOURCE_FD_OPAQUE */
   struct virgl_resource_vulkan_info vulkan_info;
   /* followed by 1 fd if not VIRGL_RESOURCE_FD_INVALID */
};

/* Import a blob resource to the context
 *
 * This roughly corresponds to:
 * - virgl_renderer_resource_import_blob
 * - virgl_renderer_ctx_attach_resource
 */
struct render_context_op_import_resource_request {
   struct render_context_op_header header;
   uint32_t res_id;
   enum virgl_resource_fd_type fd_type;
   uint64_t size;
   /* followed by 1 fd */
};

/* Free a blob resource from the context
 *
 * This roughly corresponds to:
 * - virgl_renderer_resource_unref
 */
struct render_context_op_destroy_resource_request {
   struct render_context_op_header header;
   uint32_t res_id;
};

/* Submit a small command stream to the context.
 *
 * The size limit depends on the socket type.  Currently, SOCK_SEQPACKET is
 * used and the size limit is best treated as one page.
 *
 * This roughly corresponds to virgl_renderer_submit_cmd.
 */
struct render_context_op_submit_cmd_request {
   struct render_context_op_header header;
   uint32_t size;
   char cmd[256];
   /* if size > sizeof(cmd), followed by (size - sizeof(cmd)) bytes in another
    * message; size still must be small
    */
};

/* Submit a fence to the context.
 *
 * This submits a fence to the specified ring.  When the fence signals, the
 * current sequence number of the ring in the shmem is updated.
 *
 * This roughly corresponds to virgl_renderer_context_create_fence.
 */
struct render_context_op_submit_fence_request {
   struct render_context_op_header header;
   uint32_t flags; /* VIRGL_RENDERER_FENCE_FLAG_* */
   /* TODO fix virgl_renderer_context_create_fence to use ring_index */
   uint32_t ring_index;
   uint32_t seqno;
};

union render_context_op_request {
   struct render_context_op_header header;
   struct render_context_op_nop_request nop;
   struct render_context_op_init_request init;
   struct render_context_op_create_resource_request create_resource;
   struct render_context_op_import_resource_request import_resource;
   struct render_context_op_destroy_resource_request destroy_resource;
   struct render_context_op_submit_cmd_request submit_cmd;
   struct render_context_op_submit_fence_request submit_fence;
};

#endif /* RENDER_PROTOCOL_H */
