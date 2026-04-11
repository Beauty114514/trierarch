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

/*
 * resource tests
 * test illegal resource combinations
 - 1D resources with height or depth
 - 2D with depth
*/
#include <check.h>
#include <stdlib.h>
#include <errno.h>
#include <virglrenderer.h>
#include "virgl_hw.h"
#include "testvirgl.h"

#include "pipe/p_defines.h"
#include "util/u_formats.h"

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

struct res_test {
    struct virgl_renderer_resource_create_args args;
    int retval;
};

#define TEST(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tretval) \
  { .args = { .handle = (thandle),                                      \
              .target = (ttarget),                                      \
              .format = (tformat),                                      \
              .bind = (tbind),                                          \
              .width = (twidth),                                        \
              .height = (theight),                                      \
              .depth = (tdepth),                                        \
              .array_size = (tarray_size),                              \
              .last_level = 0,                                          \
              .nr_samples = (tnr_samples),                              \
              .flags = 0 },                                             \
      .retval = (tretval)}

#define TEST_MIP(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tlast_level, tretval) \
  { .args = { .handle = (thandle),                                      \
              .target = (ttarget),                                      \
              .format = (tformat),                                      \
              .bind = (tbind),                                          \
              .width = (twidth),                                        \
              .height = (theight),                                      \
              .depth = (tdepth),                                        \
              .array_size = (tarray_size),                              \
              .last_level = (tlast_level),                              \
              .nr_samples = (tnr_samples),                              \
              .flags = 0 },                                             \
      .retval = (tretval)}

#define TEST_F(thandle, ttarget, tformat, tbind, twidth, theight, tdepth, tarray_size, tnr_samples, tflags, tretval) \
  { .args = { .handle = (thandle),                                      \
              .target = (ttarget),                                      \
              .format = (tformat),                                      \
              .bind = (tbind),                                          \
              .width = (twidth),                                        \
              .height = (theight),                                      \
              .depth = (tdepth),                                        \
              .array_size = (tarray_size),                              \
              .nr_samples = (tnr_samples),                              \
              .flags = (tflags) },                                      \
      .retval = (tretval)}


static struct res_test testlist[] = {
  /* 0 illegal target - FAIL */
  TEST(1, PIPE_MAX_TEXTURE_TYPES + 1, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, EINVAL),

  /* 1 illegal format - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_COUNT + 1, 0, 50, 1, 1, 1, 0, EINVAL),

  /* 2 legal flags - PASS */
  TEST_F(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, 0),
  /* 3 legal flags - PASS */
  TEST_F(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, 0),
  /* 4 illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0xF, EINVAL),
  /* 5 illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, EINVAL),
  /* 6 illegal flags - FAIL */
  TEST_F(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, VIRGL_RESOURCE_Y_0_TOP, EINVAL),

  /* 7 buffer test - PASS */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, 0),
  /* 8 buffer test with height - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 50, 1, 1, 0, EINVAL),
  /* 9 buffer test with depth - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 5, 1, 0, EINVAL),
  /* 10 buffer test with array - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 4, 0, EINVAL),
  /* 11 buffer test with samples - FAIL */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 4, EINVAL),
  /* 12 buffer test with miplevels - FAIL */
  TEST_MIP(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 1, 4, EINVAL),

  /* 13 buffer test - sampler view */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 14 buffer test - custom binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_CUSTOM, 50, 1, 1, 1, 0, 0),
  /* 15 buffer test - vertex binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_VERTEX_BUFFER, 50, 1, 1, 1, 0, 0),
  /* 16 buffer test - index binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_INDEX_BUFFER, 50, 1, 1, 1, 0, 0),
  /* 17 buffer test - constant binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_CONSTANT_BUFFER, 50, 1, 1, 1, 0, 0),
  /* 18 buffer test - stream binding */
  TEST(1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, PIPE_BIND_STREAM_OUTPUT, 50, 1, 1, 1, 0, 0),

  /* 19 1D test - stream binding - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_R8_UNORM, PIPE_BIND_VERTEX_BUFFER, 50, 1, 1, 1, 0, EINVAL),
  /* 20 1D test - no binding - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, EINVAL),

  /* 21 1D texture - PASS */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 22 1D texture with height - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, EINVAL),
  /* 23 1D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 24 1D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 25 1D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 26 1D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_1D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, 0),

  /* 27 1D array texture - PASS */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 28 1D array texture with height - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, EINVAL),
  /* 29 1D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 30 1D texture with array - PASS */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, 0),
  /* 31 1D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 32 1D array texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_1D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, 0),

  /* 33 2D texture - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 34 2D texture - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_CURSOR, 50, 50, 1, 1, 0, 0),
  /* 35 2D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 36 2D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 37 2D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 38 2D texture with samples - PASS */
  TEST(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 0),
  /* 39 2D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, 0),
  /* 40 2D texture with samples and miplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_2D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 4, EINVAL),

  /* 41 RECT texture - PASS */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 42 RECT texture with height - PASS */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 43 RECT texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 44 RECT texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 45 RECT texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 46 RECT texture with miplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_RECT, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, EINVAL),

  /* 47 2D texture array - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 48 2D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 49 2D texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, EINVAL),
  /* 50 2D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, 0),
  /* 51 2D texture with samples - PASS */
  TEST(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 0),
  /* 52 2D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, 0),
  /* 53 2D texture with samplesmiplevels - FAIL */
  TEST_MIP(1, PIPE_TEXTURE_2D_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, 4, EINVAL),

  /* 54 3D texture - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 0),
  /* 55 3D texture with height - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, 0),
  /* 56 3D texture with depth - PASS */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 5, 1, 0, 0),
  /* 57 3D texture with array - FAIL */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 5, 0, EINVAL),
  /* 58 3D texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 4, EINVAL),
  /* 59 3D texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_3D, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 1, 0, 4, 0),

  /* 60 CUBE texture with array size == 6 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 6, 0, 0),
  /* 61 CUBE texture with array size != 6 - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 1, 0, EINVAL),
  /* 62 CUBE texture with depth - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 5, 6, 0, EINVAL),
  /* 63 CUBE texture with samples - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 6, 4, EINVAL),
  /* 64 CUBE texture with width != height */
  TEST(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 1, 1, 6, 0, EINVAL),
  /* 65 CUBE texture with miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_CUBE, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 6, 0, 4, 0),
};

/* separate since these may fail on a GL that doesn't support cube map arrays */
static struct res_test cubemaparray_testlist[] = {
  /* 0 CUBE array with array size = 6 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 6, 0, 0),
  /* 1 CUBE array with array size = 12 - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 0, 0),
  /* 2 CUBE array with array size = 10 - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 10, 0, EINVAL),
  /* 3 CUBE array with array size = 12 and height - PASS */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 0, 0),
  /* 4 CUBE array with array size = 12 and depth - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 5, 12, 0, EINVAL),
  /* 5 CUBE array with array size = 12 and samples - FAIL */
  TEST(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 4, EINVAL),
  /* 6 CUBE array with array size = 12 and miplevels - PASS */
  TEST_MIP(1, PIPE_TEXTURE_CUBE_ARRAY, PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_BIND_SAMPLER_VIEW, 50, 50, 1, 12, 0, 4, 0),
};

START_TEST(virgl_res_tests)
{
  int ret;
  ret = testvirgl_init_single_ctx(context_flags);
  ck_assert_int_eq(ret, 0);

  /* Do not test if multisample is not available */
  unsigned multisample = testvirgl_get_multisample_from_caps();
  if (!multisample) {
    testvirgl_fini_single_ctx();
    return;
  }

  ret = virgl_renderer_resource_create(&testlist[_i].args, NULL, 0);
  ck_assert_int_eq(ret, testlist[_i].retval);

  testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(cubemaparray_res_tests)
{
  int ret;
  ret = testvirgl_init_single_ctx(context_flags);
  ck_assert_int_eq(ret, 0);

  ret = virgl_renderer_resource_create(&cubemaparray_testlist[_i].args, NULL, 0);
  ck_assert_int_eq(ret, cubemaparray_testlist[_i].retval);

  testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(private_ptr)
{
  int ret;
  ret = testvirgl_init_single_ctx(context_flags);
  ck_assert_int_eq(ret, 0);
  struct virgl_renderer_resource_create_args args = { 1, PIPE_BUFFER, PIPE_FORMAT_R8_UNORM, 0, 50, 1, 1, 1, 0, 0, 0 };
  ret = virgl_renderer_resource_create(&args, NULL, 0);
  ck_assert_int_eq(ret, 0);

  void *init_priv = (void*)0xabab;
  virgl_renderer_resource_set_priv(1, init_priv);
  void *priv = virgl_renderer_resource_get_priv(1);
  ck_assert_int_eq((uintptr_t)priv, 0xabab);
  testvirgl_fini_single_ctx();
}
END_TEST

static Suite *virgl_init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("virgl_resource");
  tc_core = tcase_create("resource");

  tcase_add_loop_test(tc_core, virgl_res_tests, 0, ARRAY_SIZE(testlist));
  tcase_add_loop_test(tc_core, cubemaparray_res_tests, 0, ARRAY_SIZE(cubemaparray_testlist));
  tcase_add_test(tc_core, private_ptr);
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
