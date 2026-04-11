/*
 * Copyright 2023 Collabora Ltd
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <epoxy/egl.h>

#include "util/bitscan.h"
#include "util/macros.h"
#include "util/u_memory.h"
#include "util/u_format.h"

#include "../testvirgl_encode.h"
#include "virgl_protocol.h"
#include "vrend/vrend_iov.h"
#include "vrend/vrend_debug.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static struct virgl_context ctx;

struct vertex {
   float position[4];
   float color[4];
};

static struct vertex vertices[3] =
{
   {
      { 0.0f, -0.9f, 0.0f, 1.0f },
      { 1.0f, 0.0f, 0.0f, 1.0f }
   },
   {
      { -0.9f, 0.9f, 0.0f, 1.0f },
      { 0.0f, 1.0f, 0.0f, 1.0f }
   },
   {
      { 0.9f, 0.9f, 0.0f, 1.0f },
      { 0.0f, 0.0f, 1.0f, 1.0f }
   }
};

#define MAX_OBJECT_ID_BITS 5
#define MAX_OBJECTS_OF_TYPE (1u << MAX_OBJECT_ID_BITS)
#define MAX_OBJECTS_MASK (MAX_OBJECTS_OF_TYPE - 1)

const uint32_t buffer_bindings[] = {
    VIRGL_BIND_CUSTOM,
    VIRGL_BIND_STAGING,
    VIRGL_BIND_INDEX_BUFFER,
    VIRGL_BIND_STREAM_OUTPUT,
    VIRGL_BIND_VERTEX_BUFFER,
    VIRGL_BIND_CONSTANT_BUFFER,
    VIRGL_BIND_SHADER_BUFFER,
    VIRGL_BIND_QUERY_BUFFER,
    VIRGL_BIND_COMMAND_ARGS,
    VIRGL_BIND_SAMPLER_VIEW,
};

const uint32_t texture_bind[4] = {
   VIRGL_BIND_DEPTH_STENCIL,
   VIRGL_BIND_SAMPLER_VIEW | VIRGL_BIND_RENDER_TARGET,
   VIRGL_BIND_SAMPLER_VIEW,
   VIRGL_BIND_CURSOR,
};

const enum pipe_texture_target texture_target[8] = {
   PIPE_TEXTURE_1D,
   PIPE_TEXTURE_2D,
   PIPE_TEXTURE_3D,
   PIPE_TEXTURE_CUBE,
   PIPE_TEXTURE_RECT,
   PIPE_TEXTURE_1D_ARRAY,
   PIPE_TEXTURE_2D_ARRAY,
   PIPE_TEXTURE_CUBE_ARRAY,
};

const uint32_t texture_formats[4] =  {
   PIPE_FORMAT_S8_UINT_Z24_UNORM,
   PIPE_FORMAT_B8G8R8A8_UNORM,
   PIPE_FORMAT_R32_FLOAT,
   PIPE_FORMAT_R16G16_UINT,
};


static struct virgl_resource *buffers[MAX_OBJECTS_OF_TYPE] = {0};
static struct virgl_resource *textures[MAX_OBJECTS_OF_TYPE] = {0};
#define MAX_EXTRA_RESOURCE_SLOTS 3
static struct virgl_resource *resources[MAX_EXTRA_RESOURCE_SLOTS] = {0};
uint32_t next_resource_handle = 1;
uint32_t next_object_handle = 1;

static inline uint32_t next_resource_slot_impl(bool reset)
{
   static uint32_t current_slot = 0;
   if (reset) {
      current_slot = 0;
      return 0;
   } else
      assert(current_slot < MAX_EXTRA_RESOURCE_SLOTS - 1);
      return current_slot++;
}

static inline uint32_t next_resource_slot(void)
{
   return next_resource_slot_impl(false);
}

static inline void reset_next_resource_slot(void)
{
   next_resource_slot_impl(true);
}

static void create_initial_gfx_state(void)
{
   struct virgl_surface surf;
   struct pipe_framebuffer_state fb_state;
   struct pipe_vertex_element ve[2];
   struct pipe_vertex_buffer vbuf;
   int ve_handle, vs_handle, fs_handle, tcs_handle, tes_handle;
   union pipe_color_union color;
   struct virgl_box box;
   int tw = 300, th = 300;

   /* init and create simple 2D resource */
   struct virgl_resource *res = CALLOC_STRUCT(virgl_resource);
   resources[next_resource_slot()] = res;

   testvirgl_create_backed_simple_2d_res(res, next_resource_handle++, tw, th);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res->handle);

   /* create a surface for the resource */
   memset(&surf, 0, sizeof(surf));
   surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
   surf.handle = next_object_handle++;
   surf.base.texture = &res->base;

   virgl_encoder_create_surface(&ctx, surf.handle, res, &surf.base);

   /* set the framebuffer state */
   fb_state.nr_cbufs = 1;
   fb_state.zsbuf = NULL;
   fb_state.cbufs[0] = &surf.base;
   virgl_encoder_set_framebuffer_state(&ctx, &fb_state);

   /* clear the resource */
   /* clear buffer to green */
   color.f[0] = 0.0;
   color.f[1] = 1.0;
   color.f[2] = 0.0;
   color.f[3] = 1.0;
   virgl_encode_clear(&ctx, PIPE_CLEAR_COLOR0, &color, 0.0, 0);

   /* create vertex elements */
   ve_handle = next_object_handle++;
   memset(ve, 0, sizeof(ve));
   ve[0].src_offset = Offset(struct vertex, position);
   ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   ve[1].src_offset = Offset(struct vertex, color);
   ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   virgl_encoder_create_vertex_elements(&ctx, ve_handle, 2, ve);

   virgl_encode_bind_object(&ctx, ve_handle, VIRGL_OBJECT_VERTEX_ELEMENTS);

   /* create vbo */
   struct virgl_resource *vbo = CALLOC_STRUCT(virgl_resource);
   resources[next_resource_slot()] = vbo;
   int vb_handle = next_resource_handle++;
   testvirgl_create_backed_simple_buffer(vbo, vb_handle, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo->handle);

   /* inline write the data to it */
   box.x = 0;
   box.y = 0;
   box.z = 0;
   box.w = sizeof(vertices);
   box.h = 1;
   box.d = 1;
   virgl_encoder_inline_write(&ctx, vbo, 0, 0, (struct pipe_box *)&box, &vertices, box.w, 0);

   vbuf.stride = sizeof(struct vertex);
   vbuf.buffer_offset = 0;
   vbuf.buffer = &vbo->base;
   virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

   /* create vertex shader */
   {
       struct pipe_shader_state vs;
       static const char *text =
              "VERT\n"
              "DCL SV[0], VERTEXID\n"
              "DCL OUT[0], POSITION\n"
              "DCL CONST[0..9]\n"
              "DCL TEMP[0..5]\n"
              "DCL ADDR[0]\n"
              "IMM[0] UINT32 {4, 64, 268435455, 0}\n"
              "IMM[1] UINT32 {0, 1065353216, 0, 0}\n"
              "DCL TEMP[6..9]\n"
              "  0: SHL TEMP[0].x, SV[0].xxxx, IMM[0].xxxx\n"
              "  1: UADD TEMP[1].x, TEMP[0].xxxx, IMM[0].yyyy\n"
              "  2: USHR TEMP[2].x, TEMP[1].xxxx, IMM[0].xxxx\n"
              "  3: UARL ADDR[0].x, TEMP[2].xxxx\n"
              "  4: MOV TEMP[3].x, CONST[ADDR[0].x].xxxx\n"
              "  5: AND TEMP[4].x, TEMP[3].xxxx, IMM[0].zzzz\n"
              "  6: UARL ADDR[0].x, TEMP[4].xxxx\n"
              "  7: MOV TEMP[5].xy, CONST[ADDR[0].x].xyyy\n"
              "  8: MOV OUT[0].xy, TEMP[5].xyxx\n"
              "  9: MOV OUT[0].zw, IMM[1].xxxy\n"
              " 10: END\n";

        memset(&vs, 0, sizeof(vs));
        vs_handle = next_object_handle++;
        virgl_encode_shader_state(&ctx, vs_handle, PIPE_SHADER_VERTEX,
                                  &vs, text);
        virgl_encode_bind_shader(&ctx, vs_handle, PIPE_SHADER_VERTEX);
   }

   /* create tcs shader */
   {
       struct pipe_shader_state tcs;
       static const char *text =
              "TESS_CTRL\n"
              "PROPERTY TCS_VERTICES_OUT 3\n"
              "DCL IN[][0], POSITION\n"
              "DCL SV[0], INVOCATIONID\n"
              "DCL OUT[][0], POSITION\n"
              "DCL OUT[1].xyz, TESSOUTER\n"
              "DCL OUT[2].xy, TESSINNER\n"
              "DCL TEMP[0]\n"
              "DCL ADDR[0..1]\n"
              "IMM[0] UINT32 {1065353216, 0, 0, 0}\n"
              "DCL TEMP[1..4]\n"
              "  0: MOV OUT[2].x, IMM[0].xxxx\n"
              "  1: MOV OUT[2].y, IMM[0].xxxx\n"
              "  2: MOV OUT[1].x, IMM[0].xxxx\n"
              "  3: MOV OUT[1].y, IMM[0].xxxx\n"
              "  4: MOV OUT[1].z, IMM[0].xxxx\n"
              "  5: UARL ADDR[1].x, SV[0].xxxx\n"
              "  6: MOV TEMP[0], IN[ADDR[1].x][0]\n"
              "  7: UARL ADDR[1].x, SV[0].xxxx\n"
              "  8: MOV OUT[ADDR[1].x][0], TEMP[0]\n"
              "  9: END\n";

        memset(&tcs, 0, sizeof(tcs));
        tcs_handle = next_object_handle++;
        virgl_encode_shader_state(&ctx, tcs_handle, PIPE_SHADER_TESS_CTRL,
                                  &tcs, text);
        virgl_encode_bind_shader(&ctx, tcs_handle, PIPE_SHADER_TESS_CTRL);
   }

   /* create tes shader */
   {
       struct pipe_shader_state tes;
       static const char *text =
              "TESS_EVAL\n"
              "PROPERTY TES_PRIM_MODE 4\n"
              "PROPERTY TES_SPACING 2\n"
              "PROPERTY TES_VERTEX_ORDER_CW 0\n"
              "PROPERTY TES_POINT_MODE 0\n"
              "DCL IN[][0], POSITION\n"
              "DCL SV[0], TESSCOORD\n"
              "DCL OUT[0], POSITION\n"
              "DCL TEMP[0..3]\n"
              "DCL ADDR[0..1]\n"
              "IMM[0] UINT32 {1073741824, 0, 0, 0}\n"
              "DCL TEMP[4..7]\n"
              "  0: MUL TEMP[0].x, IMM[0].xxxx, SV[0].zxxx\n"
              "  1: ADD TEMP[1].x, SV[0].yxxx, TEMP[0].xxxx\n"
              "  2: F2I TEMP[2].x, TEMP[1].xxxx\n"
              "  3: UARL ADDR[1].x, TEMP[2].xxxx\n"
              "  4: MOV TEMP[3], IN[ADDR[1].x][0]\n"
              "  5: MOV OUT[0], TEMP[3]\n"
              "  6: END\n";

        memset(&tes, 0, sizeof(tes));
        tes_handle = next_object_handle++;
        virgl_encode_shader_state(&ctx, tes_handle, PIPE_SHADER_TESS_EVAL,
                                  &tes, text);
        virgl_encode_bind_shader(&ctx, tes_handle, PIPE_SHADER_TESS_EVAL);
   }

   /* create fragment shader */
   {
       struct pipe_shader_state fs;
       static const char *text =
           "FRAG\n"
           "DCL OUT[0], COLOR\n"
           "IMM[0] UINT32 {0, 1065353216, 0, 0}\n"
           "DCL TEMP[0..3]\n"
           "  0: MOV OUT[0], IMM[0].xyxy\n"
           "  1: END\n";
       memset(&fs, 0, sizeof(fs));
       fs_handle = next_object_handle++;
       virgl_encode_shader_state(&ctx, fs_handle, PIPE_SHADER_FRAGMENT,
                                  &fs, text);

       virgl_encode_bind_shader(&ctx, fs_handle, PIPE_SHADER_FRAGMENT);
   }

   /* link shader */
   {
       uint32_t handles[PIPE_SHADER_TYPES];
       memset(handles, 0, sizeof(handles));
       handles[PIPE_SHADER_VERTEX] = vs_handle;
       handles[PIPE_SHADER_FRAGMENT] = fs_handle;
       handles[PIPE_SHADER_TESS_CTRL] = tcs_handle;
       handles[PIPE_SHADER_TESS_EVAL] = tes_handle;
       virgl_encode_link_shader(&ctx, handles);
   }

   /* set blend state */
   {
       struct pipe_blend_state blend;
       int blend_handle = next_object_handle++;
       memset(&blend, 0, sizeof(blend));
       blend.rt[0].colormask = PIPE_MASK_RGBA;
       virgl_encode_blend_state(&ctx, blend_handle, &blend);
       virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
   }

   /* set depth stencil alpha state */
   {
       struct pipe_depth_stencil_alpha_state dsa;
       int dsa_handle = next_object_handle++;
       memset(&dsa, 0, sizeof(dsa));
       dsa.depth.writemask = 1;
       dsa.depth.func = PIPE_FUNC_LESS;
       virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
       virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
   }

   /* set rasterizer state */
   {
       struct pipe_rasterizer_state rasterizer;
       int rs_handle = next_object_handle++;
       memset(&rasterizer, 0, sizeof(rasterizer));
       rasterizer.cull_face = PIPE_FACE_NONE;
       rasterizer.half_pixel_center = 1;
       rasterizer.bottom_edge_rule = 1;
       rasterizer.depth_clip = 1;
       virgl_encode_rasterizer_state(&ctx, rs_handle, &rasterizer);
       virgl_encode_bind_object(&ctx, rs_handle, VIRGL_OBJECT_RASTERIZER);
   }

   /* set viewport state */
   {
       struct pipe_viewport_state vp;
       float znear = 0, zfar = 1.0;
       float half_w = tw / 2.0f;
       float half_h = th / 2.0f;
       float half_d = (zfar - znear) / 2.0f;

       vp.scale[0] = half_w;
       vp.scale[1] = half_h;
       vp.scale[2] = half_d;

       vp.translate[0] = half_w + 0;
       vp.translate[1] = half_h + 0;
       vp.translate[2] = half_d + znear;
       virgl_encoder_set_viewport_states(&ctx, 0, 1, &vp);
   }

   struct pipe_draw_info info;
   memset(&info, 0, sizeof(info));
   info.count = 6;
   info.mode = PIPE_PRIM_PATCHES;
   virgl_encoder_draw_vbo(&ctx, &info);
   testvirgl_ctx_send_cmdbuf(&ctx);
}

static void create_initial_compute_state(void)
{
   /* create compute shader */
   struct pipe_shader_state cs;
   static const char *text =
      "COMP\n"
      "PROPERTY CS_FIXED_BLOCK_WIDTH 2\n"
      "PROPERTY CS_FIXED_BLOCK_HEIGHT 4\n"
      "PROPERTY CS_FIXED_BLOCK_DEPTH 1\n"
      "DCL SV[0], GRID_SIZE\n"
      "DCL SV[1], BLOCK_ID\n"
      "DCL SV[2], THREAD_ID\n"
      "DCL IMAGE[0], 2D, PIPE_FORMAT_R32_UINT\n"
      "DCL BUFFER[0]\n"
      "DCL TEMP[0..9]\n"
      "IMM[0] UINT32 {1, 2, 4, 0}\n"
      "DCL TEMP[10..13]\n"
      "DCL TEMP[14]\n"
      "0: MOV TEMP[14].xyz, SV[1].xyzz\n"
      "1: SHL TEMP[1].x, SV[0].xxxx, IMM[0].xxxx\n"
      "2: SHL TEMP[2].xy, TEMP[14].xyzx, IMM[0].xyxx\n"
      "3: UADD TEMP[3].xy, TEMP[2].xyxx, SV[2].xyxx\n"
      "4: MOV TEMP[0].xy, TEMP[3].xyxx\n"
      "5: LOAD TEMP[5], IMAGE[0], TEMP[0], 2D, PIPE_FORMAT_R32_UINT\n"
      "6: UMUL TEMP[6].x, TEMP[3].yxxx, TEMP[1].xxxx\n"
      "7: UADD TEMP[7].x, TEMP[6].xxxx, TEMP[3].xxxx\n"
      "8: SHL TEMP[8].x, TEMP[7].xxxx, IMM[0].zzzz\n"
      "9: MOV TEMP[9].x, TEMP[5].xxxx\n"
      "10: STORE BUFFER[0].x, TEMP[8].xxxx, TEMP[9].xxxx\n"
      "11: END\n";

       memset(&cs, 0, sizeof(cs));
   int cs_handle = next_object_handle++;
   virgl_encode_shader_state(&ctx, cs_handle, PIPE_SHADER_COMPUTE,
                             &cs, text);
   virgl_encode_bind_shader(&ctx, cs_handle, PIPE_SHADER_COMPUTE);

   uint32_t handles[PIPE_SHADER_TYPES];
   memset(handles, 0, sizeof(handles));
   handles[PIPE_SHADER_COMPUTE] = cs_handle;
   virgl_encode_link_shader(&ctx, handles);

   struct virgl_resource *image = CALLOC_STRUCT(virgl_resource);
   resources[next_resource_slot()] = image;

   testvirgl_create_backed_simple_2d_res(image, next_resource_handle++, 256, 128);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, image->handle);

   struct virgl_shader_image image_view = {
       .format = PIPE_FORMAT_R32_UINT,
       .access = PIPE_IMAGE_ACCESS_READ,
       .handle = image->handle,
       .layer_offset = 0,
       .level_size = 1
   };

   virgl_encode_set_shader_images(&ctx, PIPE_SHADER_COMPUTE, 0, 1, &image_view);

   struct virgl_resource *buffer = CALLOC_STRUCT(virgl_resource);
   resources[next_resource_slot()] = buffer;
   testvirgl_create_backed_simple_buffer(buffer, next_resource_handle++, 256, VIRGL_BIND_SHADER_BUFFER);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, buffer->handle);

   struct virgl_shader_buffer buffer_view = {
       .buf_len = 256,
       .offset = 0,
       .handle = buffer->handle
   };

   virgl_encode_set_shader_buffers(&ctx, PIPE_SHADER_COMPUTE, 0, 1, &buffer_view);
   uint32_t grid[3] = {2, 4, 1};
   virgl_encode_simple_launch_grid(&ctx, grid);
   testvirgl_ctx_send_cmdbuf(&ctx);
}

static int size_create_object(uint32_t obj, uint32_t available_size)
{
   switch (obj) {
   case VIRGL_OBJECT_BLEND:
      return VIRGL_OBJ_BLEND_SIZE;
   case VIRGL_OBJECT_DSA:
      return VIRGL_OBJ_DSA_SIZE;
   case VIRGL_OBJECT_RASTERIZER:
      return VIRGL_OBJ_RS_SIZE;
   case VIRGL_OBJECT_VERTEX_ELEMENTS:
      return VIRGL_OBJ_VERTEX_ELEMENTS_SIZE(available_size & 0x7);
   case VIRGL_OBJECT_SURFACE:
      return VIRGL_OBJ_SURFACE_SIZE;
   case VIRGL_OBJECT_SAMPLER_VIEW:
      return VIRGL_OBJ_SAMPLER_VIEW_SIZE;
   case VIRGL_OBJECT_SAMPLER_STATE:
      return VIRGL_OBJ_SAMPLER_STATE_SIZE;
   case VIRGL_OBJECT_QUERY:
      return VIRGL_OBJ_QUERY_SIZE;
   case VIRGL_OBJECT_STREAMOUT_TARGET:
      return VIRGL_OBJ_STREAMOUT_SIZE;
   case VIRGL_OBJECT_MSAA_SURFACE:
      return VIRGL_OBJ_MSAA_SURFACE_SIZE;
   case VIRGL_OBJECT_SHADER:
      return available_size;
   default:
      return -1;
   }
}

static struct virgl_resource *create_buffer(uint32_t seed)
{
   struct virgl_resource *res = CALLOC_STRUCT(virgl_resource);
   uint32_t buffer_handle = next_resource_handle++;
   uint32_t binding = buffer_bindings[(seed & 0xffff) % ARRAY_SIZE(buffer_bindings)];
   int size  = (seed >> 16) & 0xffff + 1;
   int ret = testvirgl_create_backed_simple_buffer(res, buffer_handle, size, binding);
   assert(ret == 0);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res->handle);
   return res;
}


inline static struct virgl_resource *get_buffer_at(uint32_t id, uint32_t seed)
{
   if (!buffers[id])
      buffers[id] = create_buffer(seed);

   return buffers[id];
}

static struct virgl_resource *create_texture(uint32_t seed)
{
   struct virgl_resource *res = CALLOC_STRUCT(virgl_resource);
   struct virgl_renderer_resource_create_args args = {0};
   args.handle = next_resource_handle++;
   args.target = texture_target[seed & 0x7];
   args.bind = texture_bind[(seed >> 3) & 0x3];
   args.format = texture_formats[(seed >> 5) & 0x3];

   args.nr_samples = ((seed >> 9) & 1) || args.target != GL_TEXTURE_2D ? 0 : 4;

   args.width = ((seed >> 10) & 0xff) + 1;

   args.height = 1;
   args.depth = 1;
   args.array_size = 1;


   switch (args.target) {
   case PIPE_TEXTURE_1D:
      break;
   case PIPE_TEXTURE_1D_ARRAY:
      args.array_size *= ((seed >> 26) & 0xff) + 1;
      break;
   case PIPE_TEXTURE_2D_ARRAY:
      args.height =((seed >> 18) & 0xff) + 1;
      args.array_size = ((seed >> 26) & 0xff) + 1;
      break;
   case PIPE_TEXTURE_CUBE:
      args.height = args.width;
      args.array_size = 6;
      break;
   case PIPE_TEXTURE_CUBE_ARRAY:
      args.height = args.width;
      args.array_size = 6 * (((seed >> 26) & 0xff) + 1);
      break;
   case PIPE_TEXTURE_3D:
      args.height =((seed >> 18) & 0xff) + 1;
      args.depth = ((seed >> 26) & 0xff) + 1;
      /* fallthrough */
   default:
      args.height =((seed >> 18) & 0xff) + 1;
      break;
   }

   args.last_level = args.target != PIPE_TEXTURE_RECT && args.nr_samples == 0 ?
                         MIN2((seed >> 6) & 0x7, util_last_bit(args.width)) : 0;

   uint32_t backing_size = args.width * args.height * args.depth * args.array_size *
                           util_format_get_blocksize(res->base.format);

   struct iovec *iov = malloc(sizeof(struct iovec));
   iov->iov_base = malloc(backing_size);
   iov->iov_len = backing_size;

   int ret = virgl_renderer_resource_create(&args, iov, 1);
   assert(ret == 0);

   res->handle = args.handle;
   res->base.target = args.target;
   res->base.format = args.format;

   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res->handle);

   return res;
}

inline static uint32_t handle_to_id(uint32_t handle)
{
   uint32_t id = 0;
   for (int shift = 0; shift < 32; shift += MAX_OBJECT_ID_BITS) {
      id ^= (handle >> shift) & MAX_OBJECTS_MASK;
   }
   return id & MAX_OBJECTS_MASK;
}

typedef struct virgl_resource * (*create_resource_cb)(uint32_t handle);

struct update_handle_args {
   uint32_t skip_bits;
   struct virgl_resource **table;
   create_resource_cb create;
};

inline static struct virgl_resource *
get_resource_at(uint32_t id, uint32_t seed, const struct update_handle_args *args)
{
   if (!args->table[id])
      args->table[id] = args->create(seed);
   return args->table[id];
}


inline static void update_resource_handle(uint32_t *handle, const struct update_handle_args *args)
{
   /* Give it a certain probability that the resource needed in a command
    * may not exist */
   if (*handle & args->skip_bits)
      return;

   uint32_t id = handle_to_id(*handle);
   struct virgl_resource *res = get_resource_at(id, *handle, args);

   if (res)
      *handle = res->handle;
}

static const struct update_handle_args buffer_args = {
    .skip_bits = 0x80000000,
    .create = create_buffer,
    .table = buffers
};

static const struct update_handle_args texture_args = {
    .skip_bits = 0x40000000,
    .create = create_texture,
    .table = textures
};


inline static void update_resource_handles(uint32_t *sub_cmd, UNUSED uint32_t used_size)
{
   uint8_t obj_type = (*sub_cmd >> 8) & 0xff;
   switch (obj_type) {
   case VIRGL_OBJECT_SURFACE:
   case VIRGL_OBJECT_MSAA_SURFACE:
      update_resource_handle(sub_cmd + VIRGL_OBJ_SURFACE_RES_HANDLE, &texture_args);
      break;
   case VIRGL_OBJECT_SAMPLER_VIEW:
      update_resource_handle(sub_cmd + VIRGL_OBJ_SAMPLER_VIEW_RES_HANDLE, &texture_args);
      break;
   case VIRGL_OBJECT_QUERY:
      update_resource_handle(sub_cmd + VIRGL_OBJ_QUERY_RES_HANDLE, &buffer_args);
      break;
   case VIRGL_OBJECT_STREAMOUT_TARGET:
      update_resource_handle(sub_cmd + VIRGL_OBJ_STREAMOUT_RES_HANDLE, &buffer_args);
      break;
   }
}

uint32_t object_handles[VIRGL_MAX_OBJECTS][MAX_OBJECTS_OF_TYPE] = {0};


/* turn a 32 bit value into a 5 bit value taking all bits into account */
inline static uint32_t id_from(uint32_t handle)
{
   uint32_t id = 0;
   for (int i = 0; i < 32; i += MAX_OBJECT_ID_BITS)
      id ^= (handle >> i) & MAX_OBJECTS_MASK;

   return id & MAX_OBJECTS_MASK;
}

static uint32_t create_blend_state(uint32_t id)
{
   struct pipe_blend_state blend = {0};
   int blend_handle = next_object_handle++;
   blend.independent_blend_enable = id & 1;
   blend.logicop_enable = !(id & 1);
   blend.logicop_func = id >> 1;
   blend.rt[0].colormask = PIPE_MASK_RGBA & id;
   blend.rt[0].blend_enable = (id >> 1) & 1;
   blend.rt[0].alpha_func = PIPE_BLEND_ADD;
   blend.rt[0].alpha_src_factor = PIPE_BLENDFACTOR_SRC_ALPHA;
   blend.rt[0].alpha_dst_factor = PIPE_BLENDFACTOR_DST_ALPHA;
   blend.rt[0].rgb_func = PIPE_BLEND_SUBTRACT;
   blend.rt[0].rgb_src_factor = PIPE_BLENDFACTOR_SRC_ALPHA;
   blend.rt[0].rgb_dst_factor = PIPE_BLENDFACTOR_DST_ALPHA;

   virgl_encode_blend_state(&ctx, blend_handle, &blend);
   return blend_handle;
}

static uint32_t create_sampler_view(uint32_t id, uint32_t texture_seed)
{
   struct pipe_sampler_view view = {0};
   view.format = texture_formats[id & 3];
   view.u.tex.first_layer = 0;
   view.u.tex.last_layer = 1;
   view.u.tex.first_level = 0;
   view.u.tex.last_level = 1;
   view.swizzle_r = PIPE_SWIZZLE_X;
   view.swizzle_g = PIPE_SWIZZLE_Y;
   view.swizzle_b = PIPE_SWIZZLE_Z;
   view.swizzle_a = PIPE_SWIZZLE_W;
   uint32_t handle = next_object_handle++;

   virgl_encode_sampler_view(&ctx,
                             handle,
                             get_resource_at(id, texture_seed, &texture_args),
                             &view);
   return handle;
}

static uint32_t create_rasterizer(uint32_t id, uint32_t seed)
{
   static const struct pipe_rasterizer_state rs_templates[2] = {
       {
        .flatshade = 1,
        .light_twoside = 1,
        .scissor = 1,
        .cull_face = PIPE_FACE_FRONT,
        .line_width = 1.0f
       },
       {
        .multisample = 1,
        .light_twoside = 1,
        .half_pixel_center = 1,
        .line_width = 1.0f,
       },
   };

   uint32_t handle = next_object_handle++;

   struct pipe_rasterizer_state rs = rs_templates[id & 1];
   rs.clip_plane_enable = seed >> 24;

   virgl_encode_rasterizer_state(&ctx, handle, &rs);
   return handle;
}

static uint32_t create_streamout_target(uint32_t id, uint32_t seed)
{
   struct virgl_resource *res = get_buffer_at(id, seed);

   unsigned buffer_offest = res->base.width0 >> 3;
   unsigned  buffer_size = (res->base.width0 - buffer_offest) >> 1;

   uint32_t handle = next_object_handle++;

   virgl_encoder_create_so_target(&ctx, handle, res, buffer_offest, buffer_size);
   return handle;
}

static void update_object_handle(uint8_t obj_type, uint32_t *handle)
{
   /* Give it a certain probability that the resource needed in a command
    * may not exist */
   if (*handle & 0x200000000)
      return;

   uint32_t id = id_from(*handle);

   if (!object_handles[obj_type][id]) {
      switch (obj_type) {
      case VIRGL_OBJECT_BLEND:
          object_handles[VIRGL_OBJECT_BLEND][id] = create_blend_state(id);
          break;
      case VIRGL_OBJECT_SAMPLER_VIEW:
          object_handles[VIRGL_OBJECT_SAMPLER_VIEW][id] = create_sampler_view(id, *handle);
          break;
      case VIRGL_OBJECT_RASTERIZER:
          object_handles[VIRGL_OBJECT_RASTERIZER][id] = create_rasterizer(id, *handle);
          break;
      case VIRGL_OBJECT_STREAMOUT_TARGET:
          object_handles[VIRGL_OBJECT_STREAMOUT_TARGET][id] = create_streamout_target(id, *handle);
          break;
      default:
          return;
      }
      testvirgl_ctx_send_cmdbuf(&ctx);
   }
   *handle = object_handles[obj_type][id];
}

static void update_object_handle_for_bind(uint32_t *buf, UNUSED uint32_t used_size)
{
   uint32_t header = buf[VIRGL_OBJ_BIND_HEADER];
   uint8_t obj_type = (header >> 8) & 0xff;

   if (obj_type < VIRGL_MAX_OBJECTS)
      update_object_handle(obj_type, &buf[VIRGL_OBJ_BIND_HANDLE]);
}

static void record_object_handle(uint32_t *buf)
{
   uint32_t header = buf[VIRGL_OBJ_CREATE_HEADER];
   uint32_t handle = buf[VIRGL_OBJ_CREATE_HANDLE];
   uint8_t obj_type = (header >> 8) & 0xff;

   object_handles[obj_type][id_from(handle)] = handle;
}

static inline int get_command_size(uint32_t header, uint32_t available_size)
{
   uint32_t cmd = header & 0xff;

   switch (cmd) {
   case VIRGL_CCMD_NOP:
      return -1;
   case VIRGL_CCMD_CREATE_OBJECT:
      return size_create_object((header >> 8) & 0xff, available_size);
   case VIRGL_CCMD_BIND_OBJECT:
   case VIRGL_CCMD_DESTROY_OBJECT:
   case VIRGL_CCMD_BEGIN_QUERY:
   case VIRGL_CCMD_END_QUERY:
   case VIRGL_CCMD_SET_SUB_CTX:
   case VIRGL_CCMD_CREATE_SUB_CTX:
   case VIRGL_CCMD_DESTROY_SUB_CTX:
   case VIRGL_CCMD_GET_MEMORY_INFO:
      return 1;
   case VIRGL_CCMD_SET_VIEWPORT_STATE: {
      uint32_t size = VIRGL_SET_VIEWPORT_STATE_SIZE(available_size & 0x7);
      return size;
   }
   case VIRGL_CCMD_SET_FRAMEBUFFER_STATE: {
      uint32_t size = VIRGL_SET_FRAMEBUFFER_STATE_SIZE(available_size & 0x7);
      return size;
   }
   case VIRGL_CCMD_SET_VERTEX_BUFFERS: {
      uint32_t size = VIRGL_SET_VERTEX_BUFFERS_SIZE(available_size & 0x3);
      return size;
   }
   case VIRGL_CCMD_CLEAR:
      return VIRGL_OBJ_CLEAR_SIZE;
   case VIRGL_CCMD_DRAW_VBO: {
      switch (available_size & 3) {
      case 0: return VIRGL_DRAW_VBO_SIZE;
      case 1: return VIRGL_DRAW_VBO_SIZE_TESS;
      default:
          return VIRGL_DRAW_VBO_SIZE_INDIRECT;
      }
   }
   case VIRGL_CCMD_RESOURCE_INLINE_WRITE:
         return available_size;
   case VIRGL_CCMD_SET_SAMPLER_VIEWS: {
      uint32_t size = VIRGL_SET_SAMPLER_VIEWS_SIZE(available_size & 0x1f);
      return size;
   }
   case VIRGL_CCMD_SET_INDEX_BUFFER: {
      uint32_t size = VIRGL_SET_INDEX_BUFFER_SIZE(available_size & 0x1);
      return size;
   }
   case VIRGL_CCMD_SET_CONSTANT_BUFFER:
      return available_size >= 2 ? available_size : -1;
   case VIRGL_CCMD_SET_STENCIL_REF:
      return VIRGL_SET_STENCIL_REF_SIZE;
   case VIRGL_CCMD_SET_BLEND_COLOR:
      return VIRGL_SET_BLEND_COLOR_SIZE;
   case VIRGL_CCMD_SET_SCISSOR_STATE: {
      uint32_t size = VIRGL_SET_SCISSOR_STATE_SIZE(available_size & 0x7);
      return size;
   }
   case VIRGL_CCMD_BLIT:
      return VIRGL_CMD_BLIT_SIZE;
   case VIRGL_CCMD_RESOURCE_COPY_REGION:
      return VIRGL_CMD_RESOURCE_COPY_REGION_SIZE;
   case VIRGL_CCMD_BIND_SAMPLER_STATES:
      return available_size > 1 ? (available_size & 0x1f) + 2 : -1;
   case VIRGL_CCMD_GET_QUERY_RESULT:
      return 2;
   case VIRGL_CCMD_SET_POLYGON_STIPPLE:
      return VIRGL_POLYGON_STIPPLE_SIZE;
   case VIRGL_CCMD_SET_CLIP_STATE:
      return VIRGL_SET_CLIP_STATE_SIZE;
   case VIRGL_CCMD_SET_SAMPLE_MASK:
        return VIRGL_SET_SAMPLE_MASK_SIZE;
   case VIRGL_CCMD_SET_STREAMOUT_TARGETS:
      return (available_size > 0) ? (available_size & 0xf) + 1 : -1;
   case VIRGL_CCMD_SET_RENDER_CONDITION:
      return VIRGL_RENDER_CONDITION_SIZE;
   case VIRGL_CCMD_SET_UNIFORM_BUFFER:
      return VIRGL_SET_UNIFORM_BUFFER_SIZE;
   case VIRGL_CCMD_BIND_SHADER:
        return VIRGL_BIND_SHADER_SIZE;
   case VIRGL_CCMD_SET_TESS_STATE:
      return VIRGL_TESS_STATE_SIZE;
   case VIRGL_CCMD_SET_MIN_SAMPLES:
      return VIRGL_SET_MIN_SAMPLES_SIZE;
   case VIRGL_CCMD_SET_SHADER_BUFFERS:
        return (available_size > 2 + VIRGL_SET_SHADER_BUFFER_ELEMENT_SIZE) ?
          VIRGL_SET_SHADER_BUFFER_SIZE(
                 ((available_size - 2) / VIRGL_SET_SHADER_BUFFER_ELEMENT_SIZE) %
                 PIPE_MAX_SHADER_BUFFERS) : -1;
   case VIRGL_CCMD_SET_SHADER_IMAGES:
        return (available_size > 2 + VIRGL_SET_SHADER_IMAGE_ELEMENT_SIZE) ?
            VIRGL_SET_SHADER_IMAGE_SIZE(
                   ((available_size - 2) / VIRGL_SET_SHADER_IMAGE_ELEMENT_SIZE) %
                   PIPE_MAX_SHADER_IMAGES) : -1;
   case VIRGL_CCMD_MEMORY_BARRIER:
        return VIRGL_MEMORY_BARRIER_SIZE;
   case VIRGL_CCMD_LAUNCH_GRID:
      return VIRGL_LAUNCH_GRID_SIZE;
   case VIRGL_CCMD_SET_FRAMEBUFFER_STATE_NO_ATTACH:
      return VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_SIZE;
   case VIRGL_CCMD_TEXTURE_BARRIER:
      return VIRGL_TEXTURE_BARRIER_SIZE;
   case VIRGL_CCMD_SET_ATOMIC_BUFFERS:
      return (available_size > 1 + VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE) ?
                 VIRGL_SET_ATOMIC_BUFFER_SIZE(
                 ((available_size -1 ) / VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE) %
                 PIPE_MAX_HW_ATOMIC_BUFFERS) : -1;
   case VIRGL_CCMD_SET_DEBUG_FLAGS:
      return available_size >= VIRGL_SET_DEBUG_FLAGS_MIN_SIZE ? available_size : -1;
   case VIRGL_CCMD_GET_QUERY_RESULT_QBO:
      return VIRGL_QUERY_RESULT_QBO_SIZE;
   case VIRGL_CCMD_TRANSFER3D:
      return VIRGL_TRANSFER3D_SIZE;
   case VIRGL_CCMD_END_TRANSFERS:
      return 0;
   case VIRGL_CCMD_COPY_TRANSFER3D:
      return VIRGL_COPY_TRANSFER3D_SIZE;
   case VIRGL_CCMD_SET_TWEAKS:
      return VIRGL_SET_TWEAKS_SIZE;
   case VIRGL_CCMD_CLEAR_TEXTURE:
      return VIRGL_CLEAR_TEXTURE_SIZE;
   case VIRGL_CCMD_PIPE_RESOURCE_CREATE:
      return VIRGL_PIPE_RES_CREATE_SIZE;
   case VIRGL_CCMD_PIPE_RESOURCE_SET_TYPE:
      return VIRGL_PIPE_RES_SET_TYPE_SIZE(available_size & 1);
   case VIRGL_CCMD_SEND_STRING_MARKER:
      return available_size >= VIRGL_SEND_STRING_MARKER_MIN_SIZE ? available_size : -1;
   case VIRGL_CCMD_LINK_SHADER:
      return VIRGL_LINK_SHADER_SIZE;

   /* video codec */
#ifdef ENABLE_VIDEO
   case VIRGL_CCMD_CREATE_VIDEO_CODEC:
   case VIRGL_CCMD_DESTROY_VIDEO_CODEC:
   case VIRGL_CCMD_CREATE_VIDEO_BUFFER:
   case VIRGL_CCMD_DESTROY_VIDEO_BUFFER:
   case VIRGL_CCMD_BEGIN_FRAME:
   case VIRGL_CCMD_DECODE_MACROBLOCK:
   case VIRGL_CCMD_DECODE_BITSTREAM:
   case VIRGL_CCMD_ENCODE_BITSTREAM:
   case VIRGL_CCMD_END_FRAME:
#endif
   default:
      return -1;
   }
}

static inline void update_handles(uint32_t cmd, uint32_t *buf, uint32_t size)
{
   switch (cmd) {
   case VIRGL_CCMD_CLEAR_TEXTURE:
      update_resource_handle(buf + VIRGL_TEXTURE_HANDLE, &texture_args);
      break;
   case VIRGL_CCMD_SET_VERTEX_BUFFERS:
      for (uint32_t i = 0; i < (size / 3); ++i)
          update_resource_handle(buf + VIRGL_SET_VERTEX_BUFFER_HANDLE(i), &buffer_args);
      break;
   case VIRGL_CCMD_SET_INDEX_BUFFER:
      update_resource_handle(buf + VIRGL_SET_INDEX_BUFFER_HANDLE, &buffer_args);
      break;
   case VIRGL_CCMD_SET_UNIFORM_BUFFER:
      update_resource_handle(buf + VIRGL_SET_UNIFORM_BUFFER_RES_HANDLE, &buffer_args);
      break;
   case VIRGL_CCMD_RESOURCE_COPY_REGION:
      update_resource_handle(buf + VIRGL_CMD_RCR_DST_RES_HANDLE, &texture_args);
      update_resource_handle(buf + VIRGL_CMD_RCR_SRC_RES_HANDLE, &texture_args);
      break;
   case VIRGL_CCMD_COPY_TRANSFER3D:
      update_resource_handle(buf + VIRGL_COPY_TRANSFER3D_SRC_RES_HANDLE, &texture_args);
      break;
   case VIRGL_CCMD_SET_FRAMEBUFFER_STATE:
      update_resource_handle(buf + VIRGL_SET_FRAMEBUFFER_STATE_NR_ZSURF_HANDLE, &texture_args);
      for (uint32_t i = 0; i < size - 3; ++i) {
          update_resource_handle(buf + VIRGL_SET_FRAMEBUFFER_STATE_CBUF_HANDLE(i), &texture_args);
      }
      break;
   case VIRGL_CCMD_BLIT:
      update_resource_handle(buf + VIRGL_CMD_BLIT_DST_RES_HANDLE, &texture_args);
      update_resource_handle(buf + VIRGL_CMD_BLIT_SRC_RES_HANDLE, &texture_args);
      break;
   case VIRGL_CCMD_DRAW_VBO:
      if (VIRGL_DRAW_VBO_SIZE_INDIRECT == size) {
          update_resource_handle(buf + VIRGL_DRAW_VBO_INDIRECT_HANDLE, &buffer_args);
          update_resource_handle(buf + VIRGL_DRAW_VBO_INDIRECT_DRAW_COUNT_HANDLE, &buffer_args);
      }
      break;
   case VIRGL_CCMD_CREATE_OBJECT:
      update_resource_handles(buf, size);
      break;
   case VIRGL_CCMD_BIND_OBJECT:
      update_object_handle_for_bind(buf, size);
      break;
   case VIRGL_CCMD_SET_SHADER_BUFFERS:
      for (uint32_t i = 0; i < (size - 2) / VIRGL_SET_SHADER_BUFFER_ELEMENT_SIZE; ++i)
          update_resource_handle(buf + VIRGL_SET_SHADER_BUFFER_RES_HANDLE(i), &buffer_args);
      break;
   case VIRGL_CCMD_SET_ATOMIC_BUFFERS:
      for (uint32_t i = 0; i < (size - 1) / VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE; ++i)
          update_resource_handle(buf + VIRGL_SET_ATOMIC_BUFFER_RES_HANDLE(i), &buffer_args);
      break;
   case VIRGL_CCMD_SET_SHADER_IMAGES:
      for (uint32_t i = 0; i < (size - 2) / VIRGL_SET_SHADER_IMAGE_ELEMENT_SIZE; ++i)
          update_resource_handle(buf + VIRGL_SET_SHADER_IMAGE_RES_HANDLE(i), &texture_args);
      break;
   case VIRGL_CCMD_GET_MEMORY_INFO:
      update_resource_handle(buf + 1, &buffer_args);
      break;
   case VIRGL_CCMD_SET_SAMPLER_VIEWS:
      for (uint32_t i = 0; i < (size - 2); ++i) {
          update_object_handle(VIRGL_OBJECT_SAMPLER_VIEW,
                               buf + VIRGL_SET_SAMPLER_VIEWS_V0_HANDLE + i);
      }
      break;
   case VIRGL_CCMD_SET_STREAMOUT_TARGETS:
      for (uint32_t i = 0; i < size - 1; ++i)
          update_object_handle(VIRGL_OBJECT_STREAMOUT_TARGET,
                               buf + VIRGL_SET_STREAMOUT_TARGETS_H0 + i);
      break;
   case VIRGL_CCMD_LAUNCH_GRID: {
      uint64_t prev_grid_size = 1;
      for (int i = 0; i < 3; ++i) {
          /* A high work group count triggers a GPU reset that may bring
           * down X11 (Radeonsi) or even result in a hard system lockup (Iris),
           * see https://gitlab.freedesktop.org/mesa/mesa/-/issues/10075.
           * So limit the all-over size of the grid (also to avoid that the
           * shaders run too long) */
          static const uint64_t grid_size_limit = 0x1000000ul;
          uint64_t new_grid_size = prev_grid_size * buf[VIRGL_LAUNCH_GRID_X + i];
          if (new_grid_size > grid_size_limit)
              buf[VIRGL_LAUNCH_GRID_X + i] = grid_size_limit / prev_grid_size;
          prev_grid_size *= buf[VIRGL_LAUNCH_GRID_X + i];
      }
   }
   default:
       break;
   }
}

static inline void destroy_backed_resource(struct virgl_resource *res)
{
   struct iovec *iovs;
   int niovs;

   virgl_renderer_resource_detach_iov(res->handle, &iovs, &niovs);

   free(iovs[0].iov_base);
   free(iovs);
   virgl_renderer_resource_unref(res->handle);

}

static inline void cleanup_resources(void)
{
   for (unsigned i = 0; i < MAX_EXTRA_RESOURCE_SLOTS; ++i) {
      if (resources[i]) {
          virgl_renderer_ctx_detach_resource(ctx.ctx_id, resources[i]->handle);
          destroy_backed_resource(resources[i]);
          free(resources[i]);
          resources[i] = NULL;
      }
   }
   for (unsigned i = 0; i < MAX_OBJECTS_OF_TYPE; ++i) {
      if (textures[i]) {
          virgl_renderer_ctx_detach_resource(ctx.ctx_id, textures[i]->handle);
          destroy_backed_resource(textures[i]);
          free(textures[i]);
          textures[i] = NULL;
      }
      if (buffers[i]) {
          virgl_renderer_ctx_detach_resource(ctx.ctx_id, buffers[i]->handle);
          destroy_backed_resource(buffers[i]);
          free(buffers[i]);
          buffers[i] = NULL;
      }
   }

   memset(object_handles, 0, sizeof(object_handles));
   next_object_handle = 1;
   next_resource_handle = 1;

   reset_next_resource_slot();
}

int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t raw_size)
{
   /* We prefer larger batches because small batches might contain
    * just NULL commands, or we would have transfers to/from resources that
    * don't exists and would have to be created on the fly.
    * Larger batches are also closer to the way virgl actualy submits the
    * commands. */
   if (raw_size < 1024u)
      return 0;

   testvirgl_init_ctx_cmdbuf(&ctx, VIRGL_RENDERER_USE_EGL);
   if (*raw_data & 1)
      create_initial_gfx_state();
   else
      create_initial_compute_state();

#define MAX_COMMAND_SIZE (16 * 1024)
   static_assert(MAX_COMMAND_SIZE <= UINT16_MAX, "size must fit in 16-bit integer");
   uint32_t sub_cmd[MAX_COMMAND_SIZE + 1];

   int64_t ndwords = raw_size / sizeof(uint32_t);
   const uint32_t *data = (const uint32_t *)raw_data;

   /* We assume that the fuzzer is seeded with valid command buffers, so we walk through
    * each data blob as if it were a command buffer and submit the commands one by one */
   while (ndwords >= 2) {
      const uint32_t header = data[0];
      uint32_t cmd = header & 0xff;
      --ndwords;
      ++data;

      if (cmd >= VIRGL_MAX_COMMANDS) {
         continue;
      }

      int32_t available_size = MIN2(ndwords, MAX_COMMAND_SIZE);

      int command_payload_size = get_command_size(header, available_size);
      if (command_payload_size < 0 || command_payload_size > available_size)
         continue;

      sub_cmd[0] = (header & 0xffff) | (command_payload_size << 16);
      memcpy(sub_cmd + 1, data, command_payload_size * sizeof(uint32_t));
      uint32_t command_size = command_payload_size + 1;

      /* Make sure that the handles used in the command stream are backed by some real
       * resource so that the command is not rejected early because a handle was invalid. */
      update_handles(cmd, sub_cmd, command_size);

      int ret = virgl_renderer_submit_cmd((void *)sub_cmd, ctx.ctx_id, command_size);
      if (cmd == VIRGL_CCMD_CREATE_OBJECT && !ret) {
         record_object_handle(sub_cmd);
      }

      data += command_payload_size;
      ndwords -= command_payload_size;
   }

   cleanup_resources();
   testvirgl_fini_ctx_cmdbuf(&ctx);
   return 0;
}
