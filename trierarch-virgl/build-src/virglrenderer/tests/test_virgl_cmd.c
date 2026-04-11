/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#include <check.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <virglrenderer.h>
#include "virgl_hw.h"
#include "vrend/vrend_iov.h"
#include "util/u_formats.h"
#include "testvirgl_encode.h"
#include "virgl_protocol.h"
#include "util/u_memory.h"

#include "large_shader.h"
/* test creating objects with same ID causes context err */
START_TEST(virgl_test_overlap_obj_id)
{
    int ret;
    struct virgl_context ctx;
    int ctx_handle = 1;
    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* set blend state */
    {
        struct pipe_blend_state blend;
        int blend_handle = ctx_handle;
        memset(&blend, 0, sizeof(blend));
        blend.rt[0].colormask = PIPE_MASK_RGBA;
        virgl_encode_blend_state(&ctx, blend_handle, &blend);
        virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
    }

    /* set depth stencil alpha state */
    {
        struct pipe_depth_stencil_alpha_state dsa;
        int dsa_handle = ctx_handle;
        memset(&dsa, 0, sizeof(dsa));
        dsa.depth.writemask = 1;
        dsa.depth.func = PIPE_FUNC_LESS;
        virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
        virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
    }
    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

#if UTIL_ARCH_LITTLE_ENDIAN
static const uint32_t test_green = 0xff00ff00;
#else
static const uint32_t test_green = 0x00ff00ff;
#endif

/* create a resource - clear it to a color, do a transfer */
START_TEST(virgl_test_clear)
{
    struct virgl_context ctx;
    struct virgl_resource res;
    struct virgl_surface surf;
    struct pipe_framebuffer_state fb_state;
    union pipe_color_union color;
    struct virgl_box box;
    int ret;
    int i;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

    /* create a surface for the resource */
    memset(&surf, 0, sizeof(surf));
    surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    surf.handle = 1;
    surf.base.texture = &res.base;

    virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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

    /* submit the cmd stream */
    testvirgl_ctx_send_cmdbuf(&ctx);

    /* read back the cleared values in the resource */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = 5;
    box.h = 1;
    box.d = 1;
    ret = virgl_renderer_transfer_read_iov(res.handle, ctx.ctx_id, 0, 50, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    /* check the returned values */
    for (i = 0; i < 5; i++) {
        uint32_t *ptr = res.iovs[0].iov_base;
        ck_assert_int_eq(ptr[i], test_green);
    }

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&res);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_blit_simple)
{
    struct virgl_context ctx;
    struct virgl_resource res, res2;
    struct virgl_surface surf;
    struct pipe_framebuffer_state fb_state;
    union pipe_color_union color;
    struct pipe_blit_info blit;
    struct virgl_box box;
    int ret;
    int i;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res2, 2, 50, 50);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res2.handle);

        /* create a surface for the resource */
    memset(&surf, 0, sizeof(surf));
    surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    surf.handle = 1;
    surf.base.texture = &res.base;

    virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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

    memset(&blit, 0, sizeof(blit));
    blit.mask = PIPE_MASK_RGBA;
    blit.dst.format = res2.base.format;
    blit.dst.box.width = 10;
    blit.dst.box.height = 1;
    blit.dst.box.depth = 1;
    blit.src.format = res.base.format;
    blit.src.box.width = 10;
    blit.src.box.height = 1;
    blit.src.box.depth = 1;
    virgl_encode_blit(&ctx, &res2, &res, &blit);

    /* submit the cmd stream */
    testvirgl_ctx_send_cmdbuf(&ctx);

    /* read back the cleared values in the resource */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = 5;
    box.h = 1;
    box.d = 1;
    ret = virgl_renderer_transfer_read_iov(res2.handle, ctx.ctx_id, 0, 50, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    /* check the returned values */
    for (i = 0; i < 5; i++) {
        uint32_t *ptr = res2.iovs[0].iov_base;
        ck_assert_int_eq(ptr[i], test_green);
    }

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res2.handle);
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&res);
    testvirgl_destroy_backed_res(&res2);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

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

/* create a resource - clear it to a color, render something */
START_TEST(virgl_test_render_simple)
{
    struct virgl_context ctx;
    struct virgl_resource res;
    struct virgl_resource vbo;
    struct virgl_surface surf;
    struct pipe_framebuffer_state fb_state;
    struct pipe_vertex_element ve[2];
    struct pipe_vertex_buffer vbuf;
    int ve_handle, vs_handle, fs_handle;
    int ctx_handle = 1;
    union pipe_color_union color;
    struct virgl_box box;
    int ret;
    int tw = 300, th = 300;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

    /* create a surface for the resource */
    memset(&surf, 0, sizeof(surf));
    surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    surf.handle = ctx_handle++;
    surf.base.texture = &res.base;

    virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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
    ve_handle = ctx_handle++;
    memset(ve, 0, sizeof(ve));
    ve[0].src_offset = Offset(struct vertex, position);
    ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    ve[1].src_offset = Offset(struct vertex, color);
    ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    virgl_encoder_create_vertex_elements(&ctx, ve_handle, 2, ve);

    virgl_encode_bind_object(&ctx, ve_handle, VIRGL_OBJECT_VERTEX_ELEMENTS);

    /* create vbo */
    ret = testvirgl_create_backed_simple_buffer(&vbo, 2, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
    ck_assert_int_eq(ret, 0);
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo.handle);

    /* inline write the data to it */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = sizeof(vertices);
    box.h = 1;
    box.d = 1;
    virgl_encoder_inline_write(&ctx, &vbo, 0, 0, (struct pipe_box *)&box, &vertices, box.w, 0);

    vbuf.stride = sizeof(struct vertex);
    vbuf.buffer_offset = 0;
    vbuf.buffer = &vbo.base;
    virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

    /* create vertex shader */
    {
        struct pipe_shader_state vs;
         const char *text =
           "VERT\n"
           "DCL IN[0]\n"
           "DCL IN[1]\n"
           "DCL OUT[0], POSITION\n"
           "DCL OUT[1], COLOR\n"
           "  0: MOV OUT[1], IN[1]\n"
           "  1: MOV OUT[0], IN[0]\n"
           "  2: END\n";
         memset(&vs, 0, sizeof(vs));
         vs_handle = ctx_handle++;
         virgl_encode_shader_state(&ctx, vs_handle, PIPE_SHADER_VERTEX,
                                   &vs, text);
         virgl_encode_bind_shader(&ctx, vs_handle, PIPE_SHADER_VERTEX);
    }

    /* create fragment shader */
    {
        struct pipe_shader_state fs;
        const char *text =
            "FRAG\n"
            "DCL IN[0], COLOR, LINEAR\n"
            "DCL OUT[0], COLOR\n"
            "  0: MOV OUT[0], IN[0]\n"
            "  1: END\n";
        memset(&fs, 0, sizeof(fs));
        fs_handle = ctx_handle++;
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
        virgl_encode_link_shader(&ctx, handles);
    }

    /* set blend state */
    {
        struct pipe_blend_state blend;
        int blend_handle = ctx_handle++;
        memset(&blend, 0, sizeof(blend));
        blend.rt[0].colormask = PIPE_MASK_RGBA;
        virgl_encode_blend_state(&ctx, blend_handle, &blend);
        virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
    }

    /* set depth stencil alpha state */
    {
        struct pipe_depth_stencil_alpha_state dsa;
        int dsa_handle = ctx_handle++;
        memset(&dsa, 0, sizeof(dsa));
        dsa.depth.writemask = 1;
        dsa.depth.func = PIPE_FUNC_LESS;
        virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
        virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
    }

    /* set rasterizer state */
    {
        struct pipe_rasterizer_state rasterizer;
        int rs_handle = ctx_handle++;
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

    /* draw */
    {
        struct pipe_draw_info info;
        memset(&info, 0, sizeof(info));
        info.count = 3;
        info.mode = PIPE_PRIM_TRIANGLES;
        virgl_encoder_draw_vbo(&ctx, &info);
    }

    ret = testvirgl_ctx_send_cmdbuf(&ctx);
    ck_assert_int_eq(ret, 0);

    /* create a fence */
    testvirgl_reset_fence();
    virgl_renderer_create_fence(1, ctx.ctx_id);

    do {
        int fence;

        virgl_renderer_poll();
        fence = testvirgl_get_last_fence();
        if (fence >= 1)
            break;
        nanosleep((struct timespec[]){{0, 50000}}, NULL);
    } while(1);

    /* read back the tri values in the resource */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = tw;
    box.h = th;
    box.d = 1;
    ret = virgl_renderer_transfer_read_iov(res.handle, ctx.ctx_id, 0, 0, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    {
        int w, h;
        bool all_cleared = true;
        uint32_t *ptr = res.iovs[0].iov_base;
        for (h = 0; h < th; h++) {
            for (w = 0; w < tw; w++) {
                if (ptr[h * tw + w] != test_green)
                    all_cleared = false;
            }
        }
        ck_assert_int_eq(all_cleared, false);
    }

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&vbo);
    testvirgl_destroy_backed_res(&res);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

static void virgl_test_bind_images_shader(int first_layer, int last_layer, int expected_error)
{
    struct virgl_context ctx;
    struct virgl_resource res;
    struct virgl_surface surf = {0};
    int ret;
    int tw = 300, th = 300;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

    /* set the framebuffer state */
    struct pipe_framebuffer_state fb_state = {
       .nr_cbufs = 1,
       .zsbuf = NULL,
       .cbufs[0] = &surf.base
    };
    virgl_encoder_set_framebuffer_state(&ctx, &fb_state);

    /* Create shader images */
    struct vrend_image_view images[1] = {0};
    images[0].u.tex.first_layer = first_layer;
    images[0].u.tex.last_layer = last_layer;
    virgl_encoder_set_shader_images(&ctx, PIPE_SHADER_FRAGMENT, 1, 1, images, res.handle);

    ret = testvirgl_ctx_send_cmdbuf(&ctx);
    ck_assert_int_eq(ret, expected_error);

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&res);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_test_bind_images_shader_pass)
{
    virgl_test_bind_images_shader(0, 0, 0);
}
END_TEST

START_TEST(virgl_test_bind_images_shader_fail_layers)
{
    virgl_test_bind_images_shader(1, 0, EINVAL);
}
END_TEST

/* create a resource - clear it to a color, render something */
START_TEST(virgl_test_render_geom_simple)
{
    struct virgl_context ctx;
    struct virgl_resource res;
    struct virgl_resource vbo;
    struct virgl_surface surf;
    struct pipe_framebuffer_state fb_state;
    struct pipe_vertex_element ve[2];
    struct pipe_vertex_buffer vbuf;
    int ve_handle, vs_handle, fs_handle, gs_handle;
    int ctx_handle = 1;
    union pipe_color_union color;
    struct virgl_box box;
    int ret;
    int tw = 300, th = 300;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* Geometry shader are only available since GLSL 150 */
    uint32_t glsl_level = testvirgl_get_glsl_level_from_caps();
    if (glsl_level < 150) {
      testvirgl_fini_ctx_cmdbuf(&ctx);
      return;
    }

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

    /* create a surface for the resource */
    memset(&surf, 0, sizeof(surf));
    surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    surf.handle = ctx_handle++;
    surf.base.texture = &res.base;

    virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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
    ve_handle = ctx_handle++;
    memset(ve, 0, sizeof(ve));
    ve[0].src_offset = Offset(struct vertex, position);
    ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    ve[1].src_offset = Offset(struct vertex, color);
    ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    virgl_encoder_create_vertex_elements(&ctx, ve_handle, 2, ve);

    virgl_encode_bind_object(&ctx, ve_handle, VIRGL_OBJECT_VERTEX_ELEMENTS);

    /* create vbo */
    ret = testvirgl_create_backed_simple_buffer(&vbo, 2, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
    ck_assert_int_eq(ret, 0);
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo.handle);

    /* inline write the data to it */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = sizeof(vertices);
    box.h = 1;
    box.d = 1;
    virgl_encoder_inline_write(&ctx, &vbo, 0, 0, (struct pipe_box *)&box, &vertices, box.w, 0);

    vbuf.stride = sizeof(struct vertex);
    vbuf.buffer_offset = 0;
    vbuf.buffer = &vbo.base;
    virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

    /* create vertex shader */
    {
        struct pipe_shader_state vs;
         const char *text =
           "VERT\n"
           "DCL IN[0]\n"
           "DCL IN[1]\n"
           "DCL OUT[0], POSITION\n"
           "DCL OUT[1], GENERIC[20]\n"
           "  0: MOV OUT[1], IN[1]\n"
           "  1: MOV OUT[0], IN[0]\n"
           "  2: END\n";
         memset(&vs, 0, sizeof(vs));
         vs_handle = ctx_handle++;
         virgl_encode_shader_state(&ctx, vs_handle, PIPE_SHADER_VERTEX,
                                   &vs, text);
         virgl_encode_bind_shader(&ctx, vs_handle, PIPE_SHADER_VERTEX);
    }

    /* create geometry shader */
    {
        struct pipe_shader_state gs;
         const char *text =
           "GEOM\n"
           "PROPERTY GS_INPUT_PRIMITIVE TRIANGLES\n"
           "PROPERTY GS_OUTPUT_PRIMITIVE TRIANGLE_STRIP\n"
           "PROPERTY GS_MAX_OUTPUT_VERTICES 3\n"
           "PROPERTY GS_INVOCATIONS 1\n"
           "DCL IN[][0], POSITION\n"
           "DCL IN[][1], GENERIC[20]\n"
           "DCL OUT[0], POSITION\n"
           "DCL OUT[1], GENERIC[20]\n"
           "IMM[0] INT32 {0, 0, 0, 0}\n"
           "0:MOV OUT[0], IN[0][0]\n"
           "1:MOV OUT[1], IN[0][1]\n"
           "2:EMIT IMM[0].xxxx\n"
           "3:MOV OUT[0], IN[1][0]\n"
           "4:MOV OUT[1], IN[0][1]\n" /* copy color from input vertex 0 */
           "5:EMIT IMM[0].xxxx\n"
           "6:MOV OUT[0], IN[2][0]\n"
           "7:MOV OUT[1], IN[2][1]\n"
           "8:EMIT IMM[0].xxxx\n"
           "9:END\n";
         memset(&gs, 0, sizeof(gs));
         gs_handle = ctx_handle++;
         virgl_encode_shader_state(&ctx, gs_handle, PIPE_SHADER_GEOMETRY,
                                   &gs, text);
         virgl_encode_bind_shader(&ctx, gs_handle, PIPE_SHADER_GEOMETRY);
    }

    /* create fragment shader */
    {
        struct pipe_shader_state fs;
        const char *text =
            "FRAG\n"
            "DCL IN[0], GENERIC[20], LINEAR\n"
            "DCL OUT[0], COLOR\n"
            "  0: MOV OUT[0], IN[0]\n"
            "  1: END\n";
        memset(&fs, 0, sizeof(fs));
        fs_handle = ctx_handle++;
        virgl_encode_shader_state(&ctx, fs_handle, PIPE_SHADER_FRAGMENT,
                                   &fs, text);

        virgl_encode_bind_shader(&ctx, fs_handle, PIPE_SHADER_FRAGMENT);
    }

    /* set blend state */
    {
        struct pipe_blend_state blend;
        int blend_handle = ctx_handle++;
        memset(&blend, 0, sizeof(blend));
        blend.rt[0].colormask = PIPE_MASK_RGBA;
        virgl_encode_blend_state(&ctx, blend_handle, &blend);
        virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
    }

    /* set depth stencil alpha state */
    {
        struct pipe_depth_stencil_alpha_state dsa;
        int dsa_handle = ctx_handle++;
        memset(&dsa, 0, sizeof(dsa));
        dsa.depth.writemask = 1;
        dsa.depth.func = PIPE_FUNC_LESS;
        virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
        virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
    }

    /* set rasterizer state */
    {
        struct pipe_rasterizer_state rasterizer;
        int rs_handle = ctx_handle++;
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

    /* draw */
    {
        struct pipe_draw_info info;
        memset(&info, 0, sizeof(info));
        info.count = 3;
        info.mode = PIPE_PRIM_TRIANGLES;
        virgl_encoder_draw_vbo(&ctx, &info);
    }

    testvirgl_ctx_send_cmdbuf(&ctx);

    /* create a fence */
    testvirgl_reset_fence();
    ret = virgl_renderer_create_fence(1, ctx.ctx_id);
    ck_assert_int_eq(ret, 0);

    do {
        int fence;

        virgl_renderer_poll();
        fence = testvirgl_get_last_fence();
        if (fence >= 1)
            break;
        nanosleep((struct timespec[]){{0, 50000}}, NULL);
    } while(1);

    /* read back the tri values in the resource */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = tw;
    box.h = th;
    box.d = 1;
    ret = virgl_renderer_transfer_read_iov(res.handle, ctx.ctx_id, 0, 0, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    {
        int w, h;
        bool all_cleared = true;
        uint32_t *ptr = res.iovs[0].iov_base;
        for (h = 0; h < th; h++) {
            for (w = 0; w < tw; w++) {
                if (ptr[h * tw + w] != test_green)
                    all_cleared = false;
            }
        }
        ck_assert_int_eq(all_cleared, false);
    }

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&vbo);
    testvirgl_destroy_backed_res(&res);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

/* create a resource - clear it to a color, render something
 * and test transform feedback
 */
START_TEST(virgl_test_render_xfb)
{
    struct virgl_context ctx;
    struct virgl_resource res;
    struct virgl_resource vbo;
    struct virgl_resource xfb;
    struct virgl_surface surf;
    struct virgl_so_target so_target;
    struct virgl_so_target *so_target_ptr;
    struct pipe_framebuffer_state fb_state;
    struct pipe_vertex_element ve[2];
    struct pipe_vertex_buffer vbuf;
    int ve_handle, vs_handle, fs_handle, xfb_handle;
    int ctx_handle = 1;
    union pipe_color_union color;
    struct virgl_box box;
    int ret;
    int tw = 300, th = 300;

    ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
    ck_assert_int_eq(ret, 0);

    /* init and create simple 2D resource */
    ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
    ck_assert_int_eq(ret, 0);

    /* attach resource to context */
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

    /* create a surface for the resource */
    memset(&surf, 0, sizeof(surf));
    surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
    surf.handle = ctx_handle++;
    surf.base.texture = &res.base;

    virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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
    ve_handle = ctx_handle++;
    memset(ve, 0, sizeof(ve));
    ve[0].src_offset = Offset(struct vertex, position);
    ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    ve[1].src_offset = Offset(struct vertex, color);
    ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
    virgl_encoder_create_vertex_elements(&ctx, ve_handle, 2, ve);

    virgl_encode_bind_object(&ctx, ve_handle, VIRGL_OBJECT_VERTEX_ELEMENTS);

    /* create vbo */
    ret = testvirgl_create_backed_simple_buffer(&vbo, 2, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
    ck_assert_int_eq(ret, 0);
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo.handle);

    /* inline write the data to it */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = sizeof(vertices);
    box.h = 1;
    box.d = 1;
    virgl_encoder_inline_write(&ctx, &vbo, 0, 0, (struct pipe_box *)&box, &vertices, box.w, 0);

    vbuf.stride = sizeof(struct vertex);
    vbuf.buffer_offset = 0;
    vbuf.buffer = &vbo.base;
    virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

    /* create stream output buffer */
    ret = testvirgl_create_backed_simple_buffer(&xfb, 3, 3*sizeof(vertices), PIPE_BIND_STREAM_OUTPUT);
    ck_assert_int_eq(ret, 0);
    virgl_renderer_ctx_attach_resource(ctx.ctx_id, xfb.handle);

    /* set streamout target */
    xfb_handle = ctx_handle++;
    virgl_encoder_create_so_target(&ctx, xfb_handle, &xfb, 0, 3*sizeof(vertices));
    so_target.handle = xfb_handle;
    so_target_ptr = &so_target;
    virgl_encoder_set_so_targets(&ctx, 1, (struct pipe_stream_output_target **)&so_target_ptr, 0);

    /* create vertex shader */
    {
        struct pipe_shader_state vs;
         const char *text =
           "VERT\n"
           "DCL IN[0]\n"
           "DCL IN[1]\n"
           "DCL OUT[0], POSITION\n"
           "DCL OUT[1], COLOR\n"
           "  0: MOV OUT[1], IN[1]\n"
           "  1: MOV OUT[0], IN[0]\n"
           "  2: END\n";
         memset(&vs, 0, sizeof(vs));
         vs_handle = ctx_handle++;
         vs.stream_output.num_outputs = 1;
         vs.stream_output.stride[0] = 4;
         vs.stream_output.output[0].num_components = 4;
         virgl_encode_shader_state(&ctx, vs_handle, PIPE_SHADER_VERTEX,
                                   &vs, text);
         virgl_encode_bind_shader(&ctx, vs_handle, PIPE_SHADER_VERTEX);
    }

    /* create fragment shader */
    {
        struct pipe_shader_state fs;
        const char *text =
            "FRAG\n"
            "DCL IN[0], COLOR, LINEAR\n"
            "DCL OUT[0], COLOR\n"
            "  0: MOV OUT[0], IN[0]\n"
            "  1: END\n";
        memset(&fs, 0, sizeof(fs));
        fs_handle = ctx_handle++;
        virgl_encode_shader_state(&ctx, fs_handle, PIPE_SHADER_FRAGMENT,
                                   &fs, text);
        virgl_encode_bind_shader(&ctx, fs_handle, PIPE_SHADER_FRAGMENT);
    }

    /* set blend state */
    {
        struct pipe_blend_state blend;
        int blend_handle = ctx_handle++;
        memset(&blend, 0, sizeof(blend));
        blend.rt[0].colormask = PIPE_MASK_RGBA;
        virgl_encode_blend_state(&ctx, blend_handle, &blend);
        virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
    }

    /* set depth stencil alpha state */
    {
        struct pipe_depth_stencil_alpha_state dsa;
        int dsa_handle = ctx_handle++;
        memset(&dsa, 0, sizeof(dsa));
        dsa.depth.writemask = 1;
        dsa.depth.func = PIPE_FUNC_LESS;
        virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
        virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
    }

    /* set rasterizer state */
    {
        struct pipe_rasterizer_state rasterizer;
        int rs_handle = ctx_handle++;
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

    /* draw */
    {
        struct pipe_draw_info info;
        memset(&info, 0, sizeof(info));
        info.count = 3;
        info.mode = PIPE_PRIM_TRIANGLES;
        virgl_encoder_draw_vbo(&ctx, &info);
    }

    testvirgl_ctx_send_cmdbuf(&ctx);

    /* create a fence */
    testvirgl_reset_fence();
    ret = virgl_renderer_create_fence(1, ctx.ctx_id);
    ck_assert_int_eq(ret, 0);

    do {
        int fence;

        virgl_renderer_poll();
        fence = testvirgl_get_last_fence();
        if (fence >= 1)
            break;
        nanosleep((struct timespec[]){{0, 50000}}, NULL);
    } while(1);

    /* read back the tri values in the resource */
    box.x = 0;
    box.y = 0;
    box.z = 0;
    box.w = tw;
    box.h = th;
    box.d = 1;
    ret = virgl_renderer_transfer_read_iov(res.handle, ctx.ctx_id, 0, 0, 0, &box, 0, NULL, 0);
    ck_assert_int_eq(ret, 0);

    {
        int w, h;
        bool all_cleared = true;
        uint32_t *ptr = res.iovs[0].iov_base;
        for (h = 0; h < th; h++) {
            for (w = 0; w < tw; w++) {
                if (ptr[h * tw + w] != test_green)
                    all_cleared = false;
            }
        }
        ck_assert_int_eq(all_cleared, false);
    }

    /* cleanup */
    virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

    testvirgl_destroy_backed_res(&xfb);
    testvirgl_destroy_backed_res(&vbo);
    testvirgl_destroy_backed_res(&res);

    testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

/* send a large shader across */
START_TEST(virgl_test_large_shader)
{
   int ret;
   struct virgl_context ctx;
   int ctx_handle = 1;
   int fs_handle;
   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* create large fragment shader */
   {
      struct pipe_shader_state fs;
      const char *text = large_frag;

      memset(&fs, 0, sizeof(fs));
      fs_handle = ctx_handle++;
      virgl_encode_shader_state(&ctx, fs_handle, PIPE_SHADER_FRAGMENT,
                                &fs, text);

      virgl_encode_bind_shader(&ctx, fs_handle, PIPE_SHADER_FRAGMENT);
   }

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_set_viewport_state)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_viewport_state vp[PIPE_MAX_VIEWPORTS];
   int start_slot = 0;
   int viewports_count = 1;

   /* Filling the struct with real values is actually not needed, clearing to 0 should suffice if we
       don't want to re-use the context  for more drawing */
   int tw = 300, th = 300;

   float znear = 0, zfar = 1.0;
   float half_w = tw / 2.0f;
   float half_h = th / 2.0f;
   float half_d = (zfar - znear) / 2.0f;

   vp[0].scale[0] = half_w;
   vp[0].scale[1] = half_h;
   vp[0].scale[2] = half_d;

   vp[0].translate[0] = half_w + 0;
   vp[0].translate[1] = half_h + 0;
   vp[0].translate[2] = half_d + znear;

   /* Here we are in range, no error expected */
   ret = virgl_encoder_set_viewport_states(&ctx, start_slot, viewports_count, vp);
   ck_assert_int_eq(ret, 0);

   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Too many viewports */
   viewports_count = PIPE_MAX_VIEWPORTS + 1;

   ret = virgl_encoder_set_viewport_states(&ctx, start_slot, viewports_count, vp);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Using too large index to place viewport */
   start_slot = PIPE_MAX_VIEWPORTS;
   viewports_count = 1;

   ret = virgl_encoder_set_viewport_states(&ctx, start_slot, viewports_count, vp);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Combination of previous conditions */
   start_slot = 2;
   viewports_count = PIPE_MAX_VIEWPORTS + 1;

   ret = virgl_encoder_set_viewport_states(&ctx, start_slot, viewports_count, vp);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_decode_set_scissor_state)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_scissor_state ss[PIPE_MAX_VIEWPORTS + 1] = {0};

   int start_slot = 0;
   int scissors_count = 2;

   /* Here we are using correct values, no error expected */
   ret = virgl_encoder_set_scissor_state(&ctx, start_slot, scissors_count, ss);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Too many scissors for viewports */
   start_slot = 0;
   scissors_count = PIPE_MAX_VIEWPORTS + 1;

   ret = virgl_encoder_set_scissor_state(&ctx, start_slot, scissors_count, ss);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Using too large index to place scissors */
   start_slot = PIPE_MAX_VIEWPORTS;
   scissors_count = 2;

   ret = virgl_encoder_set_scissor_state(&ctx, start_slot, scissors_count, ss);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Combination of previous conditions */
   start_slot = 2;
   scissors_count = PIPE_MAX_VIEWPORTS + 1 - 2;

   ret = virgl_encoder_set_scissor_state(&ctx, start_slot, scissors_count, ss);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_decode_set_constant_buffer)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   uint8_t constant_buffer[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

   /* Here we are using correct values, no error expected */
   ret = virgl_encoder_write_constant_buffer(&ctx, PIPE_SHADER_VERTEX, 1, 10, constant_buffer);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Sending a NULL constant buffer */
   ret = virgl_encoder_write_constant_buffer(&ctx, PIPE_SHADER_FRAGMENT, 2, 0, NULL);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);
   ctx.cbuf->cdw = 0;

   /* Sending constant for non-existing shader type */
   ret = virgl_encoder_write_constant_buffer(&ctx, PIPE_SHADER_INVALID, 4, 10, constant_buffer);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_decode_bind_sampler_states)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   uint32_t handles[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

   /* Here we are using correct values, no error expected */
   ret = virgl_encode_bind_sampler_states(&ctx, PIPE_SHADER_VERTEX, 0, 10, handles);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Sending constant for non-existing shader type */
   ret = virgl_encode_bind_sampler_states(&ctx, PIPE_SHADER_INVALID, 0, 10, handles);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_encode_sampler_view)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_sampler_view state = {
       .format = VIRGL_FORMAT_B8G8R8A8_UNORM
   };

   struct virgl_resource res;
   struct virgl_resource res2;

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res2, 2, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* Here we are using correct values, no error expected */
   ret = virgl_encode_sampler_view(&ctx, res.handle,
                                   &res,
                                   &state);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Sending illegal format */
   state.format = VIRGL_FORMAT_NONE;
   ret = virgl_encode_sampler_view(&ctx, res2.handle,
                                   &res2,
                                   &state);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Sending illegal format */
   state.format = VIRGL_FORMAT_MAX;
   ret = virgl_encode_sampler_view(&ctx, res2.handle,
                                   &res2,
                                   &state);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Sending illegal sampler view target */
   state.format = VIRGL_FORMAT_B8G8R8A8_UNORM | PIPE_MAX_TEXTURE_TYPES << 24;
   ret = virgl_encode_sampler_view(&ctx, res2.handle,
                                   &res2,
                                   &state);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_destroy_backed_res(&res);
   testvirgl_destroy_backed_res(&res2);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_encode_sampler_view_fail_layers)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_sampler_view state1, state2;

   memset(&state1, 0, sizeof(state1));
   memset(&state2, 0, sizeof(state2));

   struct virgl_resource res;
   struct virgl_resource res2;

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res2, 2, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* Try to hit limits of layers */
   state1.format = VIRGL_FORMAT_B8G8R8A8_UNORM;
   state1.u.tex.first_layer = 2;
   state1.u.tex.last_layer = 0;

   ret = virgl_encode_sampler_view(&ctx, res.handle,
                                   &res,
                                   &state1);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* Try to hit limits of levels */
   state2.format = VIRGL_FORMAT_B8G8R8A8_UNORM;
   state2.u.tex.first_level = 2;
   state2.u.tex.last_level = 0;

   ret = virgl_encode_sampler_view(&ctx, res.handle,
                                   &res,
                                   &state2);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_destroy_backed_res(&res);
   testvirgl_destroy_backed_res(&res2);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_encode_sampler_view_texture_buffer)
{
   struct virgl_context ctx;
   int ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_sampler_view state1;

   memset(&state1, 0, sizeof(state1));

   struct virgl_resource res = {0};

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_buffer(&res, 1, 50, PIPE_BIND_SAMPLER_VIEW);
   ck_assert_int_eq(ret, 0);

   state1.format = PIPE_FORMAT_R8_UNORM;
   state1.u.buf.first_element = 0;
   state1.u.buf.last_element = 2;

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   ret = virgl_encode_sampler_view(&ctx, res.handle,
                                   &res,
                                   &state1);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

static void test_create_surface(enum pipe_format format, int expected_error)
{
   struct virgl_context ctx;
   struct virgl_resource res;
   struct virgl_surface surf;
   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* create a surface for the resource */
   memset(&surf, 0, sizeof(surf));
   surf.base.format = format;
   surf.handle = 1;
   surf.base.texture = &res.base;

   ret = virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);
   ck_assert_int_eq(ret, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, expected_error);

   /* cleanup */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   /* Check usage with illegal resource */
   ret = virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);
   ck_assert_int_eq(ret, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_test_create_surface_pass)
{
   test_create_surface(VIRGL_FORMAT_B8G8R8A8_UNORM, 0);
}
END_TEST

START_TEST(virgl_test_create_surface_fail_format)
{
   test_create_surface(VIRGL_FORMAT_MAX, EINVAL);
}
END_TEST

START_TEST(virgl_test_create_surface_fail_layers)
{
   struct virgl_context ctx;
   struct virgl_resource res;
   struct virgl_surface surf;
   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* create a surface for the resource */
   memset(&surf, 0, sizeof(surf));
   surf.base.format = VIRGL_FORMAT_B8G8R8A8_UNORM;
   surf.handle = 1;
   surf.base.texture = &res.base;
   surf.base.u.tex.first_layer = 2;
   surf.base.u.tex.last_layer = 0;

   ret = virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);
   ck_assert_int_eq(ret, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* cleanup */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

START_TEST(virgl_test_query)
{
   struct virgl_context ctx;
   struct virgl_resource res = {0};
   struct virgl_resource qbo = {0};
   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_buffer(&res, 1, 50, VIRGL_BIND_CUSTOM);
   ck_assert_int_eq(ret, 0);

   ret = testvirgl_create_backed_simple_buffer(&qbo, 2, 50, VIRGL_BIND_QUERY_BUFFER);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, qbo.handle);

   virgl_encoder_create_query(&ctx, 2000, 0, &res, 0);
   virgl_encoder_begin_query(&ctx, 2000);
   virgl_encoder_end_query(&ctx, 2000);
   virgl_encoder_get_query_result(&ctx, 2000, 1);
   virgl_encoder_get_query_result_qbo(&ctx, 2000, qbo.handle, 1, 4, 100, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   /* Try with invalid resource */
   virgl_encoder_create_query(&ctx, 100, 0, &res, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   virgl_encoder_begin_query(&ctx, 100);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   virgl_encoder_end_query(&ctx, 100);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   virgl_encoder_get_query_result(&ctx, 100, 1);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   virgl_encoder_get_query_result_qbo(&ctx, 100, qbo.handle, 1, 4, 100, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* cleanup */
   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

static void test_vertex_elements(int ve_num, int expected_error)
{
   struct virgl_context ctx;
   struct pipe_vertex_element ve[PIPE_MAX_ATTRIBS+1];

   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* create vertex elements */
   memset(ve, 0, sizeof(ve));

  for (int i = 0; i < ve_num;i++) {
     ve[i].src_offset = sizeof(float[4])* i;
     ve[i].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
  }

   virgl_encoder_create_vertex_elements(&ctx, 2000, ve_num, ve);

   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, expected_error);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_test_create_vertex_elements_pass)
{
   test_vertex_elements(PIPE_MAX_ATTRIBS, 0);
}
END_TEST

START_TEST(virgl_test_create_vertex_elements_fail)
{
   test_vertex_elements(PIPE_MAX_ATTRIBS + 1, EINVAL);
}
END_TEST

static void test_create_shader(enum pipe_shader_type type, int expected_error)
{
   struct virgl_context ctx;

   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   struct pipe_shader_state vs;
   const char *text =
       "VERT\n"
       "DCL IN[0]\n"
       "DCL IN[1]\n"
       "DCL OUT[0], POSITION\n"
       "DCL OUT[1], COLOR\n"
       "  0: MOV OUT[1], IN[1]\n"
       "  1: MOV OUT[0], IN[0]\n"
       "  2: END\n";
   memset(&vs, 0, sizeof(vs));
   vs.stream_output.num_outputs = 1;
   vs.stream_output.stride[0] = 4;
   vs.stream_output.output[0].num_components = 4;
   virgl_encode_shader_state(&ctx, 2000, type,
                             &vs, text);

   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, expected_error);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_test_create_shader_pass)
{
   test_create_shader(PIPE_SHADER_VERTEX, 0);
}
END_TEST

START_TEST(virgl_test_create_shader_fail)
{
   test_create_shader(PIPE_SHADER_TYPES, EINVAL);
}
END_TEST

/* create a resource - clear it to a color, do a transfer */
START_TEST(virgl_test_clear_texture)
{
   struct virgl_context ctx;
   struct virgl_resource res;
   union pipe_color_union color;
   struct virgl_box box;
   int ret;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, 50, 50);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* clear the resource */
   /* clear buffer to green */
   color.f[0] = 0.0;
   color.f[1] = 1.0;
   color.f[2] = 0.0;
   color.f[3] = 1.0;
   box.x = 0;
   box.y = 0;
   box.z = 0;
   box.w = 5;
   box.h = 1;
   box.d = 1;

   ret = virgl_encoder_clear_texture(&ctx, res.handle, 0, box, &color);
   ck_assert_int_eq(ret, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* Check usage with illegal resource */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   ret = virgl_encoder_clear_texture(&ctx, res.handle, 0, box, &color);
   ck_assert_int_eq(ret, 0);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, EINVAL);

   /* cleanup */
   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

static void test_draw_vbo_unified(uint32_t indirect_handle, uint32_t indirect_draw_count_handle, int expected_error)
{
   struct virgl_context ctx;
   struct virgl_resource res;
   struct virgl_resource vbo;
   struct pipe_vertex_buffer vbuf;
   int ret;
   int tw = 300, th = 300;
   int vs_handle, fs_handle;
   int ctx_handle = 1;
   union pipe_color_union color;

   struct virgl_surface surf;
   struct pipe_framebuffer_state fb_state;
   struct virgl_box box;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* create a surface for the resource */
   memset(&surf, 0, sizeof(surf));
   surf.base.format = PIPE_FORMAT_B8G8R8X8_UNORM;
   surf.handle = ctx_handle++;
   surf.base.texture = &res.base;

   virgl_encoder_create_surface(&ctx, surf.handle, &res, &surf.base);

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


   /* create vbo */
   ret = testvirgl_create_backed_simple_buffer(&vbo, 2, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
   ck_assert_int_eq(ret, 0);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo.handle);

   /* inline write the data to it */
   box.x = 0;
   box.y = 0;
   box.z = 0;
   box.w = sizeof(vertices);
   box.h = 1;
   box.d = 1;
   virgl_encoder_inline_write(&ctx, &vbo, 0, 0, (struct pipe_box *)&box, &vertices, box.w, 0);

   vbuf.stride = sizeof(struct vertex);
   vbuf.buffer_offset = 0;
   vbuf.buffer = &vbo.base;
   virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

   /* create vertex shader */
   {
     struct pipe_shader_state vs;
     const char *text =
         "VERT\n"
         "DCL IN[0]\n"
         "DCL IN[1]\n"
         "DCL OUT[0], POSITION\n"
         "DCL OUT[1], COLOR\n"
         "  0: MOV OUT[1], IN[1]\n"
         "  1: MOV OUT[0], IN[0]\n"
         "  2: END\n";
     memset(&vs, 0, sizeof(vs));
     vs_handle = ctx_handle++;
     virgl_encode_shader_state(&ctx, vs_handle, PIPE_SHADER_VERTEX,
                               &vs, text);
     virgl_encode_bind_shader(&ctx, vs_handle, PIPE_SHADER_VERTEX);
   }

   /* create fragment shader */
   {
     struct pipe_shader_state fs;
     const char *text =
         "FRAG\n"
         "DCL IN[0], COLOR, LINEAR\n"
         "DCL OUT[0], COLOR\n"
         "  0: MOV OUT[0], IN[0]\n"
         "  1: END\n";
     memset(&fs, 0, sizeof(fs));
     fs_handle = ctx_handle++;
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
     virgl_encode_link_shader(&ctx, handles);
   }

   /* set blend state */
   {
     struct pipe_blend_state blend;
     int blend_handle = ctx_handle++;
     memset(&blend, 0, sizeof(blend));
     blend.rt[0].colormask = PIPE_MASK_RGBA;
     virgl_encode_blend_state(&ctx, blend_handle, &blend);
     virgl_encode_bind_object(&ctx, blend_handle, VIRGL_OBJECT_BLEND);
   }

   /* set depth stencil alpha state */
   {
     struct pipe_depth_stencil_alpha_state dsa;
     int dsa_handle = ctx_handle++;
     memset(&dsa, 0, sizeof(dsa));
     dsa.depth.writemask = 1;
     dsa.depth.func = PIPE_FUNC_LESS;
     virgl_encode_dsa_state(&ctx, dsa_handle, &dsa);
     virgl_encode_bind_object(&ctx, dsa_handle, VIRGL_OBJECT_DSA);
   }

   /* set rasterizer state */
   {
     struct pipe_rasterizer_state rasterizer;
     int rs_handle = ctx_handle++;
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

   /* draw */
   {
     struct pipe_draw_info info;
     memset(&info, 0, sizeof(info));
     info.count = 3;
     info.mode = PIPE_PRIM_TRIANGLES;
     virgl_encoder_draw_vbo_indirect(&ctx, &info, indirect_handle, indirect_draw_count_handle);
   }

   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, expected_error);

   /* cleanup */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   testvirgl_destroy_backed_res(&vbo);
   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(virgl_test_draw_vbo_pass)
{
   test_draw_vbo_unified(0, 0, 0);
}
END_TEST

START_TEST(virgl_test_draw_vbo_fail_indirect_missing_handle)
{
   test_draw_vbo_unified(1000, 2000, EINVAL);
}
END_TEST

START_TEST(virgl_test_draw_vbo_fail_not_recoverable)
{
   struct virgl_context ctx;
   struct virgl_resource res;
   struct virgl_resource vbo;
   struct pipe_vertex_buffer vbuf;
   int ret;
   int tw = 300, th = 300;

   ret = testvirgl_init_ctx_cmdbuf(&ctx, context_flags);
   ck_assert_int_eq(ret, 0);

   /* init and create simple 2D resource */
   ret = testvirgl_create_backed_simple_2d_res(&res, 1, tw, th);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, res.handle);

   /* create vbo */

   ret = testvirgl_create_backed_simple_buffer(&vbo, 2, sizeof(vertices), PIPE_BIND_VERTEX_BUFFER);
   ck_assert_int_eq(ret, 0);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, vbo.handle);

   vbuf.stride = sizeof(struct vertex);
   vbuf.buffer_offset = 0;
   vbuf.buffer = &vbo.base;
   virgl_encoder_set_vertex_buffers(&ctx, 1, &vbuf);

   /* create not recoverable state */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, vbo.handle);

   /* draw */
   {
     struct pipe_draw_info info;
     memset(&info, 0, sizeof(info));
     info.count = 3;
     info.mode = PIPE_PRIM_TRIANGLES;
     virgl_encoder_draw_vbo_indirect(&ctx, &info, 0, 0);
   }

   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, ENOTRECOVERABLE);

   /* cleanup */
   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);

   testvirgl_destroy_backed_res(&vbo);
   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}
END_TEST

static Suite *virgl_init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("virgl_clear");
  tc_core = tcase_create("clear");
  tcase_add_test(tc_core, virgl_test_clear);
  tcase_add_test(tc_core, virgl_test_blit_simple);
  tcase_add_test(tc_core, virgl_test_overlap_obj_id);
  tcase_add_test(tc_core, virgl_test_large_shader);
  tcase_add_test(tc_core, virgl_test_render_simple);
  tcase_add_test(tc_core, virgl_test_render_geom_simple);
  tcase_add_test(tc_core, virgl_test_render_xfb);
  tcase_add_test(tc_core, virgl_test_set_viewport_state);
  tcase_add_test(tc_core, virgl_decode_set_scissor_state);
  tcase_add_test(tc_core, virgl_decode_set_constant_buffer);
  tcase_add_test(tc_core, virgl_decode_bind_sampler_states);
  tcase_add_test(tc_core, virgl_test_encode_sampler_view);
  tcase_add_test(tc_core, virgl_test_encode_sampler_view_fail_layers);
  tcase_add_test(tc_core, virgl_test_encode_sampler_view_texture_buffer);
  tcase_add_test(tc_core, virgl_test_create_surface_pass);
  tcase_add_test(tc_core, virgl_test_create_surface_fail_format);
  tcase_add_test(tc_core, virgl_test_create_surface_fail_layers);
  tcase_add_test(tc_core, virgl_test_create_vertex_elements_pass);
  tcase_add_test(tc_core, virgl_test_create_vertex_elements_fail);
  tcase_add_test(tc_core, virgl_test_create_shader_pass);
  tcase_add_test(tc_core, virgl_test_create_shader_fail);
  tcase_add_test(tc_core, virgl_test_clear_texture);
  tcase_add_test(tc_core, virgl_test_draw_vbo_pass);
  tcase_add_test(tc_core, virgl_test_draw_vbo_fail_indirect_missing_handle);
  tcase_add_test(tc_core, virgl_test_draw_vbo_fail_not_recoverable);
  tcase_add_test(tc_core, virgl_test_bind_images_shader_pass);
  tcase_add_test(tc_core, virgl_test_bind_images_shader_fail_layers);
  tcase_add_test(tc_core, virgl_test_query);

  suite_add_tcase(s, tc_core);
  return s;

}

int main(void)
{
  Suite *s;
  SRunner *sr;
  int number_failed;

  if (getenv("VRENDTEST_USE_EGL_SURFACELESS"))
     context_flags |= VIRGL_RENDERER_USE_SURFACELESS;
   if (getenv("VRENDTEST_USE_EGL_GLES"))
      context_flags |= VIRGL_RENDERER_USE_GLES;

  s = virgl_init_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
