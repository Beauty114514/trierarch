/**************************************************************************
 *
 * Copyright (C) 2024 Collabora Ltd
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
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/u_format.h"
#include "virgl_hw.h"
#include "vrend/vrend_iov.h"
#include "vrend/vrend_winsys.h"
#include "virglrenderer.h"
#include "virgl_protocol.h"
#include "testvirgl_encode.h"
#include "virgl_hw.h"
#include <epoxy/egl.h>

static void
common_ctx_init(struct virgl_context *ctx)
{
   int ret = testvirgl_init_ctx_cmdbuf(ctx, context_flags);
   ck_assert_int_eq(ret, 0);
}


static void wait_finish_commands(struct virgl_context *ctx)
{
   /* create a fence */
   testvirgl_reset_fence();
   int ret = virgl_renderer_create_fence(1, ctx->ctx_id);
   ck_assert_int_eq(ret, 0);

   do {
      int fence;

      virgl_renderer_poll();
      fence = testvirgl_get_last_fence();
      if (fence >= 1)
         break;
      nanosleep((struct timespec[]){{0, 50000}}, NULL);
   } while(1);
}

static void
create_backed_scanout_resource(struct virgl_context *ctx,
                               uint32_t tex_handle, uint32_t width, uint32_t height,
                               enum pipe_format format,
                               uint32_t bind,
                               struct virgl_resource *res)
{

   /* Adding VIRGL_BIND_SAMPLER_VIEW may result in gbm read mapping to fail with
    * mesa/gbm because the texture is then allocated as encrypted on radeonsi */
   struct virgl_renderer_resource_create_args args = {
       .handle = tex_handle,
       .target = PIPE_TEXTURE_2D,
       .format = format,
       .bind = bind,
       .width = width,
       .height = height,
       .depth = 1,
       .array_size = 1,
   };
   int ret = virgl_renderer_resource_create(&args, NULL, 0);
   ck_assert_int_eq(ret, 0);

   /* attach resource to context */
   virgl_renderer_ctx_attach_resource(ctx->ctx_id, tex_handle);

   memset(res, 0, sizeof(struct virgl_resource));
   res->handle = tex_handle;
   res->base.target = (enum pipe_texture_target)args.target;
   res->base.format = (enum pipe_format)args.format;

   res->stride = args.width * util_format_get_blocksize(res->base.format);
   uint32_t backing_size = res->stride * args.height;

   res->iovs = (struct iovec*) malloc(sizeof(struct iovec));
   res->iovs[0].iov_base = calloc(1, backing_size);
   res->iovs[0].iov_len = backing_size;
   res->niovs = 1;
   virgl_renderer_resource_attach_iov(res->handle, res->iovs, res->niovs);

}

#define COPY_TO_IOV(IOV_BASE, WIDTH, HEIGHT, VALUES, TYPE) \
   { \
      TYPE *t = (TYPE *) IOV_BASE; \
      for (uint32_t row = 0; row < HEIGHT; ++row) { \
         for (uint32_t col = 0; col < WIDTH; ++col, t += 4) { \
            t[0] = (TYPE) VALUES[0]; \
            t[1] = (TYPE) VALUES[1]; \
            t[2] = (TYPE) VALUES[2]; \
            t[3] = (TYPE) VALUES[3]; \
         } \
      } \
   }

#define CHECK_IOV(IOV_BASE, WIDTH, HEIGHT, FORMAT, VALUES, TYPE) \
   { \
      TYPE *t = (TYPE *) IOV_BASE; \
      for (uint32_t row = 0; row < HEIGHT; ++row) { \
         for (uint32_t col = 0; col < WIDTH; ++col, t += 4) { \
            ck_assert_int_eq(t[0], VALUES[0]); \
            ck_assert_int_eq(t[1], VALUES[1]); \
            ck_assert_int_eq(t[2], VALUES[2]); \
            if (util_format_has_alpha(FORMAT)) \
               ck_assert_int_eq(t[3], VALUES[3]); \
         } \
      } \
   }

static void iov_up_and_download(enum pipe_format format, uint32_t bind, const uint32_t values[4])
{
   struct virgl_context ctx;
   common_ctx_init(&ctx);

   const uint32_t tex_handle = 21;
   const uint32_t width = 4;
   const uint32_t height = 2;
   int ret;

   struct virgl_box box = {
       .w = width,
       .h = height,
       .d = 1
   };

   struct virgl_resource res;
   create_backed_scanout_resource(&ctx,tex_handle,
                                       width, height, format, bind, &res);

   int red_channel_bits = util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 0);
   switch (red_channel_bits) {
   case 8:
       COPY_TO_IOV(res.iovs[0].iov_base, width, height, values, uint8_t);
       break;
   case 16:
       COPY_TO_IOV(res.iovs[0].iov_base, width, height, values, uint16_t);
       break;
   default:
       assert(0);
   }

   virgl_renderer_transfer_write_iov(tex_handle, ctx.ctx_id,
                                     0,
                                     res.stride,
                                     0,
                                     &box,
                                     0,
                                     NULL,
                                     0);


   memset(res.iovs[0].iov_base, 0 , res.iovs[0].iov_len);
   ret = virgl_renderer_transfer_read_iov(res.handle, ctx.ctx_id, 0,
                                          res.stride, 0, &box, 0, NULL, 0);
   ck_assert_int_eq(ret, 0);

   switch (red_channel_bits) {
   case 8:
       CHECK_IOV(res.iovs[0].iov_base, width, height, format, values, uint8_t);
       break;
   case 16:
       CHECK_IOV(res.iovs[0].iov_base, width, height, format, values, uint16_t);
       break;
   }

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);
   testvirgl_destroy_backed_res(&res);
   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(iov_up_and_download_b8g8r8x8_unorm)
{
   const uint32_t values[4] = {128, 10, 192, 255};
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   iov_up_and_download(PIPE_FORMAT_B8G8R8X8_UNORM, bind, values);
}
END_TEST

START_TEST(iov_up_and_download_b8g8r8a8_unorm)
{
   const uint32_t values[4] = {128, 10, 192, 255};
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   iov_up_and_download(PIPE_FORMAT_B8G8R8A8_UNORM, bind, values);
}
END_TEST


START_TEST(iov_up_and_download_r8g8b8x8_unorm)
{
   const uint32_t values[4] = {128, 10, 192, 255};
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   iov_up_and_download(PIPE_FORMAT_R8G8B8X8_UNORM, bind, values);
}
END_TEST

START_TEST(iov_up_and_download_r8g8b8a8_unorm)
{
   const uint32_t values[4] = {128, 10, 192, 112};
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   iov_up_and_download(PIPE_FORMAT_R8G8B8A8_UNORM, bind, values);
}
END_TEST

START_TEST(iov_up_and_download_r16g16b16x16_float)
{
    const uint32_t values[4] = {0x3880, 0x3900, 0x3a00, 0x3c00};
    uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
    iov_up_and_download(PIPE_FORMAT_R16G16B16X16_FLOAT, bind, values);
}
END_TEST


enum test_gbm_method {
   ALWAYS_TRY_GBM,
   UPLOAD_USE_GL,
   DOWNLOAD_USE_GL
};

#define FILL_BUFFER(BUFFER, SIZE, TYPE) \
   { \
      TYPE *w = (TYPE *) BUFFER; \
      for (uint32_t i = 0; i < SIZE; ++i, ++w) \
         *w = i; \
   }

#define CHECK_BUFFER(BUFFER, SIZE, TYPE, HAS_ALPHA) \
   { \
      TYPE *r = (TYPE *) BUFFER; \
      for (uint32_t i = 0; i < SIZE; ++i, ++r) \
         if ((i & 3) != 3 || HAS_ALPHA) \
            ck_assert_int_eq(*r, i); \
   }

static void staging_transfer_up_and_download(enum pipe_format format,
                                              uint32_t bind,
                                              enum test_gbm_method method)
{
   struct virgl_context ctx;
   common_ctx_init(&ctx);

   const uint32_t tex_handle = 21;
   const uint32_t width = 4;
   const uint32_t height = 2;
   int ret = 0;

   struct virgl_resource res;
   create_backed_scanout_resource(&ctx, tex_handle, width, height,
                                  format, bind, &res);

   uint32_t buffer_size = res.iovs[0].iov_len;

   struct virgl_resource readback_buffer;
   const uint32_t readback_handle = 22;
   ret = testvirgl_create_backed_simple_buffer(&readback_buffer, readback_handle, buffer_size, VIRGL_BIND_STAGING);
   ck_assert_int_eq(ret, 0);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, readback_handle);

   struct virgl_resource write_buffer;
   const uint32_t write_handle = 23;
   ret = testvirgl_create_backed_simple_buffer(&write_buffer, write_handle, buffer_size, VIRGL_BIND_STAGING);
   ck_assert_int_eq(ret, 0);
   virgl_renderer_ctx_attach_resource(ctx.ctx_id, write_handle);

   int red_channel_bits = util_format_get_component_bits(format, UTIL_FORMAT_COLORSPACE_RGB, 0);
   switch (red_channel_bits) {
   case 8:
       FILL_BUFFER(write_buffer.iovs[0].iov_base, buffer_size, uint8_t);
       break;
   case 16:
       FILL_BUFFER(write_buffer.iovs[0].iov_base, buffer_size/2, uint16_t);
       break;
   default:
       assert(0);
   };

   struct pipe_box transfer_box = {
       .x = 0, .y = 0, .z = 0,
       .width = width, .height = height, .depth = 1
   };

   virgl_encoder_copy_transfer(&ctx, &res, 0, 0, &transfer_box, &write_buffer, 0,
                               method == UPLOAD_USE_GL ? VIRGL_COPY_TRANSFER3D_FLAGS_DEBUG_TEST_NO_GBM_MAPPING : 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   if (method == UPLOAD_USE_GL)
      wait_finish_commands(&ctx);

   virgl_encoder_copy_transfer(&ctx, &res, 0, 0, &transfer_box, &readback_buffer, 0,
                               (method == DOWNLOAD_USE_GL ? VIRGL_COPY_TRANSFER3D_FLAGS_DEBUG_TEST_NO_GBM_MAPPING : 0) |
                                   VIRGL_COPY_TRANSFER3D_FLAGS_READ_FROM_HOST);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   bool has_alpha = util_format_has_alpha(format);

   switch (red_channel_bits) {
   case 8:
       CHECK_BUFFER(readback_buffer.iovs[0].iov_base, buffer_size, uint8_t, has_alpha);
       break;
   case 16:
       CHECK_BUFFER(readback_buffer.iovs[0].iov_base, buffer_size/2, uint16_t, has_alpha);
       break;
   }

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, write_buffer.handle);
   testvirgl_destroy_backed_res(&write_buffer);

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, readback_buffer.handle);
   testvirgl_destroy_backed_res(&readback_buffer);

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);
   testvirgl_destroy_backed_res(&res);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}

START_TEST(transfer_up_and_download_b8g8r8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8A8_UNORM, bind, ALWAYS_TRY_GBM);
}
END_TEST


START_TEST(transfer_up_and_download_r8g8b8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8A8_UNORM, bind, ALWAYS_TRY_GBM);
}
END_TEST

START_TEST(transfer_up_and_download_b8g8r8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8X8_UNORM, bind, ALWAYS_TRY_GBM);
}
END_TEST


START_TEST(transfer_up_and_download_r8g8b8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8X8_UNORM, bind, ALWAYS_TRY_GBM);
}
END_TEST

START_TEST(transfer_gl_up_gbm_download_b8g8r8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8A8_UNORM, bind, UPLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_gl_up_gbm_download_r8g8b8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8A8_UNORM, bind, UPLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_gl_up_gbm_download_b8g8r8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8X8_UNORM, bind, UPLOAD_USE_GL);
}
END_TEST


START_TEST(transfer_gl_up_gbm_download_r8g8b8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8X8_UNORM, bind, UPLOAD_USE_GL);
}
END_TEST


START_TEST(transfer_gbm_up_gl_download_b8g8r8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8A8_UNORM, bind, DOWNLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_gbm_up_gl_download_r8g8b8a8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8A8_UNORM, bind, DOWNLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_gbm_up_gl_download_b8g8r8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_B8G8R8X8_UNORM, bind, DOWNLOAD_USE_GL);
}
END_TEST


START_TEST(transfer_gbm_up_gl_download_r8g8b8x8_unorm)
{
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   staging_transfer_up_and_download(PIPE_FORMAT_R8G8B8X8_UNORM, bind, DOWNLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_up_and_download_r16g16b16x16_float)
{
    uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
    staging_transfer_up_and_download(PIPE_FORMAT_R16G16B16X16_FLOAT, bind, ALWAYS_TRY_GBM);
}
END_TEST

START_TEST(transfer_gl_up_gbm_download_r16g16b16x16_float)
{
    uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
    staging_transfer_up_and_download(PIPE_FORMAT_R16G16B16X16_FLOAT, bind, UPLOAD_USE_GL);
}
END_TEST

START_TEST(transfer_gbm_up_gl_download_r16g16b16x16_float)
{
    uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
    staging_transfer_up_and_download(PIPE_FORMAT_R16G16B16X16_FLOAT, bind, DOWNLOAD_USE_GL);
}
END_TEST

static void gbm_clear_and_download(enum pipe_format format,
                                   uint32_t bind,
                                   const uint8_t clear_color[4])
{
   /* Precondition, we only test 8 bit 4 chan formats */
   ck_assert_int_eq(util_format_get_blocksize(format), 4);

   struct virgl_context ctx;
   common_ctx_init(&ctx);

   const uint32_t tex_handle = 21;
   const uint32_t readback_handle = 22;
   const uint32_t width = 4;
   const uint32_t height = 2;
   int ret = 0;

   struct virgl_resource res;
   create_backed_scanout_resource(&ctx,tex_handle, width, height,
                                       format, bind, &res);
   struct virgl_resource readback_buffer;
   ret = testvirgl_create_backed_simple_buffer(&readback_buffer, readback_handle, 32, VIRGL_BIND_STAGING);
   ck_assert_int_eq(ret, 0);

   virgl_renderer_ctx_attach_resource(ctx.ctx_id, readback_handle);

   union pipe_color_union color;
   struct virgl_box box;

   // need 8 bit per component
   color.ui[0] = clear_color[0] |
                 clear_color[1] << 8 |
                 clear_color[2] << 16 |
                 clear_color[3] << 24;
   box.x = 0;
   box.y = 0;
   box.z = 0;
   box.w = width;
   box.h = height;
   box.d = 1;

   ret = virgl_encoder_clear_texture(&ctx, res.handle, 0, box, &color);
   ck_assert_int_eq(ret, 0);
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   /* We must not use VIRGL_COPY_TRANSFER3D_FLAGS_SYNCHRONIZED, because this will
    * make the transfer code use the glReadPixel code path in most cases, and we
    * don't really reading back by using GBM mappings */
   wait_finish_commands(&ctx);

   struct pipe_box readback_box = {
       .x = 0, .y = 0, .z = 0,
       .width = width, .height = height, .depth = 1
   };

   virgl_encoder_copy_transfer(&ctx, &res, 0, 0, &readback_box, &readback_buffer, 0,
                               VIRGL_COPY_TRANSFER3D_FLAGS_READ_FROM_HOST);

   /* submit the cmd stream */
   ret = testvirgl_ctx_send_cmdbuf(&ctx);
   ck_assert_int_eq(ret, 0);

   struct virgl_box buffer_box  = {
       .x = 0, .y = 0, .z = 0,
       .w = 32, .h = 1, .d = 1
   };
   ret = virgl_renderer_transfer_read_iov(readback_buffer.handle, ctx.ctx_id, 0,
                                          readback_buffer.stride, 0, &buffer_box, 0, NULL, 0);

   uint8_t *t = (uint8_t *)readback_buffer.iovs[0].iov_base;

   for (uint32_t row = 0; row < height; ++row) {
      for (uint32_t col = 0; col < width; ++col, t += 4) {
         ck_assert_int_eq(t[0], clear_color[0]);
         ck_assert_int_eq(t[1], clear_color[1]);
         ck_assert_int_eq(t[2], clear_color[2]);
         if (util_format_has_alpha(format))
            ck_assert_int_eq(t[3], clear_color[3]);
      }
   }

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, res.handle);
   testvirgl_destroy_backed_res(&res);

   virgl_renderer_ctx_detach_resource(ctx.ctx_id, readback_buffer.handle);
   testvirgl_destroy_backed_res(&readback_buffer);

   testvirgl_fini_ctx_cmdbuf(&ctx);
}


START_TEST(gbm_clear_and_download_b8g8r8x8)
{
   const uint8_t clear_color[4] = {
       128, 64, 192, 255
   };
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   gbm_clear_and_download(PIPE_FORMAT_B8G8R8X8_UNORM, bind, clear_color);
}
END_TEST

START_TEST(gbm_clear_and_download_b8g8r8a8)
{
   const uint8_t clear_color[4] = {
       128, 64, 192, 32
   };
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   gbm_clear_and_download(PIPE_FORMAT_B8G8R8A8_UNORM, bind, clear_color);
}
END_TEST


START_TEST(gbm_clear_and_download_r8g8b8x8)
{
   const uint8_t clear_color[4] = {
       128, 64, 192, 255
   };
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   gbm_clear_and_download(PIPE_FORMAT_R8G8B8X8_UNORM, bind, clear_color);
}
END_TEST

START_TEST(gbm_clear_and_download_r8g8b8a8)
{
   const uint8_t clear_color[4] = {
       128, 64, 192, 32
   };
   uint32_t bind = VIRGL_BIND_SCANOUT | VIRGL_BIND_RENDER_TARGET;
   gbm_clear_and_download(PIPE_FORMAT_R8G8B8A8_UNORM, bind, clear_color);
}
END_TEST

static Suite *virgl_init_suite(void)
{
   Suite *s = suite_create("virgl_gbm");
   TCase *tc_gbm_rw = tcase_create("gbm_rw");

   tcase_add_test(tc_gbm_rw, iov_up_and_download_b8g8r8x8_unorm);
   tcase_add_test(tc_gbm_rw, iov_up_and_download_b8g8r8a8_unorm);
   tcase_add_test(tc_gbm_rw, iov_up_and_download_r8g8b8x8_unorm);
   tcase_add_test(tc_gbm_rw, iov_up_and_download_r8g8b8a8_unorm);

   tcase_add_test(tc_gbm_rw, transfer_up_and_download_b8g8r8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_up_and_download_b8g8r8x8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_up_and_download_r8g8b8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_up_and_download_r8g8b8x8_unorm);

   tcase_add_test(tc_gbm_rw, transfer_gl_up_gbm_download_b8g8r8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gl_up_gbm_download_r8g8b8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gl_up_gbm_download_b8g8r8x8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gl_up_gbm_download_r8g8b8x8_unorm);

   tcase_add_test(tc_gbm_rw, transfer_gbm_up_gl_download_b8g8r8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gbm_up_gl_download_r8g8b8a8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gbm_up_gl_download_b8g8r8x8_unorm);
   tcase_add_test(tc_gbm_rw, transfer_gbm_up_gl_download_r8g8b8x8_unorm);

   tcase_add_test(tc_gbm_rw, gbm_clear_and_download_b8g8r8x8);
   tcase_add_test(tc_gbm_rw, gbm_clear_and_download_b8g8r8a8);
   tcase_add_test(tc_gbm_rw, gbm_clear_and_download_r8g8b8x8);
   tcase_add_test(tc_gbm_rw, gbm_clear_and_download_r8g8b8a8);

   tcase_add_test(tc_gbm_rw, iov_up_and_download_r16g16b16x16_float);
   tcase_add_test(tc_gbm_rw, transfer_up_and_download_r16g16b16x16_float);
   tcase_add_test(tc_gbm_rw, transfer_gl_up_gbm_download_r16g16b16x16_float);
   tcase_add_test(tc_gbm_rw, transfer_gbm_up_gl_download_r16g16b16x16_float);

   suite_add_tcase(s, tc_gbm_rw);
   return s;

}

int main(void)
{
   Suite *s;
   SRunner *sr;
   int number_failed = 0;

   context_flags |= VIRGL_RENDERER_USE_SURFACELESS | VIRGL_RENDERER_USE_GLES;

   int ret = vrend_winsys_init(context_flags, -1);
   if (!ret && gbm) {
      vrend_winsys_cleanup();

      s = virgl_init_suite();
      sr = srunner_create(s);

      srunner_run_all(sr, CK_NORMAL);
      number_failed = srunner_ntests_failed(sr);
      srunner_free(sr);
   } else if (!gbm) {
      fprintf(stderr, "gbm not initialized, no tests run\n");
   }

   return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
