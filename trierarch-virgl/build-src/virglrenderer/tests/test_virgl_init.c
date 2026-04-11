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
 * basic library initialisation, teardown, reset
 * and context creation tests.
 */

#include "config.h"

#include <check.h>
#include <stdlib.h>
#include <errno.h>
#include <virglrenderer.h>
#ifdef ENABLE_GBM
#include <gbm.h>
#endif
#include "testvirgl.h"
#include "virgl_hw.h"
#include "vrend/vrend_iov.h"
struct myinfo_struct {
  uint32_t test;
};

struct myinfo_struct mystruct;

static struct virgl_renderer_callbacks test_cbs;

START_TEST(virgl_init_no_cbs)
{
  int ret;
  ret = virgl_renderer_init(&mystruct, 0, NULL);
  ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(virgl_init_no_cookie)
{
  int ret;
  ret = virgl_renderer_init(NULL, 0, &test_cbs);
  ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(virgl_init_cbs_wrong_ver)
{
  int ret;
  struct virgl_renderer_callbacks testcbs;
  memset(&testcbs, 0, sizeof(testcbs));
  testcbs.version = VIRGL_RENDERER_CALLBACKS_VERSION + 1;
  ret = virgl_renderer_init(&mystruct, 0, &testcbs);
  ck_assert_int_eq(ret, -1);
}
END_TEST

START_TEST(virgl_init_cleanup_without_init)
{
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_reset_without_init)
{
  virgl_renderer_reset();
}
END_TEST

START_TEST(virgl_init_egl)
{
  int ret;
  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);
  virgl_renderer_cleanup(&mystruct);
}

END_TEST

START_TEST(virgl_init_egl_double_init)
{
  int ret;
  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_double_init_conflict_args)
{
  struct myinfo_struct local_struct;
  struct virgl_renderer_callbacks local_cbs;
  int ret;

  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  ret = virgl_renderer_init(&local_struct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, -EBUSY);

  ret = virgl_renderer_init(&mystruct, 0, &test_cbs);
  ck_assert_int_eq(ret, -EBUSY);

  memset(&local_cbs, 0, sizeof(local_cbs));
  local_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &local_cbs);
  ck_assert_int_eq(ret, -EBUSY);

  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx)
{
  int ret;
  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);
  ret = virgl_renderer_context_create(1, strlen("test1"), "test1");
  ck_assert_int_eq(ret, 0);

  virgl_renderer_context_destroy(1);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_0)
{
  int ret;

  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);
  ret = virgl_renderer_context_create(0, strlen("test1"), "test1");
  ck_assert_int_eq(ret, EINVAL);

  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_destroy_ctx_illegal)
{
  int ret;
  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_context_destroy(1);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_leak)
{
  int ret = testvirgl_init_single_ctx(context_flags);
  ck_assert_int_eq(ret, 0);

  /* don't destroy the context - leak it make sure cleanup catches it */
  /*virgl_renderer_context_destroy(1);*/
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_bind_res)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_bind_res_illegal_ctx)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(2, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST


START_TEST(virgl_init_egl_create_ctx_create_bind_res_illegal_res)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, 2);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_unbind_no_bind)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_unbind_illegal_ctx)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_detach_resource(2, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST


START_TEST(virgl_init_egl_create_ctx_create_bind_res_leak)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_single_ctx_nr();

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  /*virgl_renderer_ctx_detach_resource(1, res.handle);*/

  /*virgl_renderer_resource_unref(1);*/
  /* don't detach or destroy resource - it should still get cleanedup */
  testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_reset)
{
  int ret;

  ret = testvirgl_init_single_ctx(context_flags);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_reset();

  /* reset should have destroyed the context */
  ret = virgl_renderer_context_create(1, strlen("test1"), "test1");
  ck_assert_int_eq(ret, 0);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_get_caps_set0)
{
  int ret;
  uint32_t max_ver, max_size;

  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_get_cap_set(0, &max_ver, &max_size);
  ck_assert_int_eq(max_ver, 0);
  ck_assert_int_eq(max_size, 0);

  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_get_caps_set1)
{
  int ret;
  uint32_t max_ver, max_size;
  void *caps;
  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_get_cap_set(1, &max_ver, &max_size);
  ck_assert_int_ge(max_ver, 1);
  ck_assert_int_ne(max_size, 0);
  ck_assert_int_ge(max_size, sizeof(struct virgl_caps_v1));

  caps = malloc(max_size);

  virgl_renderer_fill_caps(0, 0, caps);

  free(caps);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_init_get_caps_null)
{
  int ret;
  uint32_t max_ver, max_size;

  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_get_cap_set(1, &max_ver, &max_size);
  ck_assert_int_ge(max_ver, 1);
  ck_assert_int_ne(max_size, 0);
  ck_assert_int_ge(max_size, sizeof(struct virgl_caps_v1));

  virgl_renderer_fill_caps(0, 0, NULL);

  virgl_renderer_cleanup(&mystruct);
}
END_TEST

START_TEST(virgl_test_get_resource_info)
{
  int ret;
  struct virgl_renderer_resource_create_args res;
  struct virgl_renderer_resource_info info;

  testvirgl_init_simple_2d_resource(&res, 1);
  res.format = VIRGL_FORMAT_B8G8R8X8_UNORM;
  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_resource_get_info(res.handle, &info);
  ck_assert_int_eq(ret, 0);

#ifdef ENABLE_GBM
  ck_assert(info.drm_fourcc == GBM_FORMAT_ABGR8888 ||
            info.drm_fourcc == GBM_FORMAT_XBGR8888 ||
            info.drm_fourcc == GBM_FORMAT_ARGB8888 ||
            info.drm_fourcc == GBM_FORMAT_XRGB8888);
#endif
  ck_assert_int_eq(info.virgl_format, res.format);
  ck_assert_int_eq(res.width, info.width);
  ck_assert_int_eq(res.height, info.height);
  ck_assert_int_eq(res.depth, info.depth);
  ck_assert_int_eq(res.flags, info.flags);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_get_resource_info_no_info)
{
  int ret;
  struct virgl_renderer_resource_create_args res;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  ret = virgl_renderer_resource_get_info(1, NULL);
  ck_assert_int_eq(ret, EINVAL);

  virgl_renderer_ctx_detach_resource(1, res.handle);
  virgl_renderer_resource_unref(1);
}
END_TEST


START_TEST(virgl_test_get_resource_info_no_res)
{
  int ret;
  struct virgl_renderer_resource_info info;

  ret = virgl_renderer_resource_get_info(1, &info);
  ck_assert_int_eq(ret, EINVAL);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_get_resource_info_ext)
{
  int ret;
  struct virgl_renderer_resource_create_args res;
  struct virgl_renderer_resource_info_ext info_ext;

  testvirgl_init_simple_2d_resource(&res, 1);
  res.format = VIRGL_FORMAT_B8G8R8X8_UNORM;
  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  info_ext.version = VIRGL_RENDERER_RESOURCE_INFO_EXT_VERSION;
  ret = virgl_renderer_resource_get_info_ext(res.handle, &info_ext);
  ck_assert_int_eq(ret, 0);
  ck_assert_int_eq(info_ext.version, VIRGL_RENDERER_RESOURCE_INFO_EXT_VERSION);

#ifdef ENABLE_GBM
  ck_assert(info_ext.base.drm_fourcc == GBM_FORMAT_ABGR8888 ||
            info_ext.base.drm_fourcc == GBM_FORMAT_XBGR8888 ||
            info_ext.base.drm_fourcc == GBM_FORMAT_ARGB8888 ||
            info_ext.base.drm_fourcc == GBM_FORMAT_XRGB8888);
#endif
  if (info_ext.has_dmabuf_export) {
    /* we can't check the value of the field "modifiers" since it depends on the system */
    ck_assert_int_ge(info_ext.planes, 1);
  }
  ck_assert_int_eq(info_ext.base.virgl_format, res.format);
  ck_assert_int_eq(res.width, info_ext.base.width);
  ck_assert_int_eq(res.height, info_ext.base.height);
  ck_assert_int_eq(res.depth, info_ext.base.depth);
  ck_assert_int_eq(res.flags, info_ext.base.flags);
  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_test_get_resource_info_ext_newer_version)
{
  int ret;
  struct virgl_renderer_resource_create_args res;
  struct virgl_renderer_resource_info_ext info_ext;

  testvirgl_init_simple_2d_resource(&res, 1);
  res.format = VIRGL_FORMAT_B8G8R8X8_UNORM;
  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_ctx_attach_resource(1, res.handle);

  info_ext.version = VIRGL_RENDERER_RESOURCE_INFO_EXT_VERSION + 1;
  ret = virgl_renderer_resource_get_info_ext(res.handle, &info_ext);
  ck_assert_int_eq(ret, 0);
  ck_assert_int_eq(info_ext.version, VIRGL_RENDERER_RESOURCE_INFO_EXT_VERSION);

  virgl_renderer_ctx_detach_resource(1, res.handle);

  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_attach_res)
{
  int ret;
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  struct iovec *iovs_r;
  int num_r;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  iovs[0].iov_base = malloc(4096);
  iovs[0].iov_len = 4096;

  ret = virgl_renderer_resource_attach_iov(1, iovs, 1);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_resource_detach_iov(1, &iovs_r, &num_r);

  free(iovs[0].iov_base);
  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_attach_res_detach_no_iovs)
{
  int ret;
  struct virgl_renderer_resource_create_args res;
  struct iovec iovs[1];
  int num_r;

  testvirgl_init_simple_1d_resource(&res, 1);

  ret = virgl_renderer_resource_create(&res, NULL, 0);
  ck_assert_int_eq(ret, 0);

  iovs[0].iov_base = malloc(4096);
  iovs[0].iov_len = 4096;

  ret = virgl_renderer_resource_attach_iov(1, iovs, 1);
  ck_assert_int_eq(ret, 0);

  virgl_renderer_resource_detach_iov(1, NULL, &num_r);

  free(iovs[0].iov_base);
  virgl_renderer_resource_unref(1);
}
END_TEST

START_TEST(virgl_init_egl_create_ctx_create_attach_res_illegal_res)
{
  int ret;
  struct iovec iovs[1];

  test_cbs.version = 1;
  ret = virgl_renderer_init(&mystruct, context_flags, &test_cbs);
  ck_assert_int_eq(ret, 0);

  ret = virgl_renderer_resource_attach_iov(1, iovs, 1);
  ck_assert_int_eq(ret, EINVAL);

  virgl_renderer_resource_unref(1);
  virgl_renderer_context_destroy(1);
  virgl_renderer_cleanup(&mystruct);
}
END_TEST

static Suite *virgl_init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("virgl_init");
  tc_core = tcase_create("init");

  tcase_add_test(tc_core, virgl_init_no_cbs);
  tcase_add_test(tc_core, virgl_init_no_cookie);
  tcase_add_test(tc_core, virgl_init_cbs_wrong_ver);
  tcase_add_test(tc_core, virgl_init_cleanup_without_init);
  tcase_add_test(tc_core, virgl_init_reset_without_init);
  tcase_add_test(tc_core, virgl_init_egl);
  tcase_add_test(tc_core, virgl_init_egl_double_init);
  tcase_add_test(tc_core, virgl_init_egl_double_init_conflict_args);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_0);
  tcase_add_test(tc_core, virgl_init_egl_destroy_ctx_illegal);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_leak);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_reset);
  tcase_add_test(tc_core, virgl_init_get_caps_set0);
  tcase_add_test(tc_core, virgl_init_get_caps_set1);
  tcase_add_test(tc_core, virgl_init_get_caps_null);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_attach_res_illegal_res);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_bind_res_leak);

  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("init_std");
  tcase_add_checked_fixture(tc_core, testvirgl_init_single_ctx_nr, testvirgl_fini_single_ctx);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_bind_res);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_bind_res_illegal_ctx);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_bind_res_illegal_res);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_unbind_no_bind);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_unbind_illegal_ctx);

  tcase_add_test(tc_core, virgl_test_get_resource_info);
  tcase_add_test(tc_core, virgl_test_get_resource_info_no_info);
  tcase_add_test(tc_core, virgl_test_get_resource_info_no_res);
  tcase_add_test(tc_core, virgl_test_get_resource_info_ext);
  tcase_add_test(tc_core, virgl_test_get_resource_info_ext_newer_version);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_attach_res);
  tcase_add_test(tc_core, virgl_init_egl_create_ctx_create_attach_res_detach_no_iovs);

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
