/**************************************************************************
 *
 * Copyright 2020 Google LLC
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

#include <check.h>
#include <errno.h>
#if defined(__FreeBSD__)
#ifndef ETIME
#define ETIME ETIMEDOUT
#endif
#endif
#ifndef _WIN32
#include <poll.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <virglrenderer.h>

#include "testvirgl.h"

START_TEST(virgl_fence_create)
{
   int ret;

   ret = virgl_renderer_create_fence(1, 0);
   ck_assert_int_eq(ret, EINVAL);

   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   testvirgl_reset_fence();
   ret = virgl_renderer_create_fence(1, 0);
   ck_assert_int_eq(ret, 0);

   testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(virgl_fence_poll)
{
   const int target_seqno = 50;
   int ret;
   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   testvirgl_reset_fence();
   ret = virgl_renderer_create_fence(target_seqno, 0);
   ck_assert_int_eq(ret, 0);

   do {
      int seqno;

      virgl_renderer_poll();
      seqno = testvirgl_get_last_fence();
      if (seqno == target_seqno)
         break;

      ck_assert_int_eq(seqno, 0);
      usleep(1000);
   } while(1);

   testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(virgl_fence_poll_many)
{
   const int fence_count = 100;
   const int base_seqno = 50;
   const int target_seqno = base_seqno + fence_count - 1;
   int last_seqno;
   int ret;
   int i;

   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   testvirgl_reset_fence();
   last_seqno = 0;

   for (i = 0; i < fence_count; i++) {
      ret = virgl_renderer_create_fence(base_seqno + i, 0);
      ck_assert_int_eq(ret, 0);
   }

   do {
      int seqno;

      virgl_renderer_poll();
      seqno = testvirgl_get_last_fence();
      if (seqno == target_seqno)
         break;

      ck_assert(seqno == 0 || (seqno >= base_seqno && seqno < target_seqno));

      /* monotonic increasing */
      ck_assert_int_ge(seqno, last_seqno);
      last_seqno = seqno;

      usleep(1000);
   } while(1);

   testvirgl_fini_single_ctx();
}
END_TEST

#ifndef _WIN32
static int
wait_sync_fd(int fd, int timeout)
{
   struct pollfd pollfd = {
      .fd = fd,
      .events = POLLIN,
   };
   int ret;
   do {
      ret = poll(&pollfd, 1, timeout);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   if (ret < 0)
      return -errno;
   else if (ret > 0 && !(pollfd.revents & POLLIN))
      return -EINVAL;

   return ret ? 0 : -ETIME;
}

START_TEST(virgl_fence_export)
{
   const int target_seqno = 50;
   int fd;
   int ret;

   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   testvirgl_reset_fence();
   ret = virgl_renderer_create_fence(target_seqno, 0);
   ck_assert_int_eq(ret, 0);

   ret = virgl_renderer_export_fence(target_seqno, &fd);
   ck_assert_int_eq(ret, 0);

   ret = wait_sync_fd(fd, -1);
   ck_assert_int_eq(ret, 0);

   virgl_renderer_poll();
   ck_assert_int_eq(testvirgl_get_last_fence(), target_seqno);

   close(fd);

   testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(virgl_fence_export_signaled)
{
   const int target_seqno = 50;
   const int test_range = 10;
   int fd;
   int ret;
   int i;

   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   /* when there is no active fence, a signaled fd is always returned */
   for (i = 0; i < test_range; i++) {
      ret = virgl_renderer_export_fence(target_seqno + 1 + i, &fd);
      ck_assert_int_eq(ret, 0);

      ret = wait_sync_fd(fd, 0);
      ck_assert_int_eq(ret, 0);

      close(fd);
   }

   ret = virgl_renderer_create_fence(target_seqno, 0);
   ck_assert_int_eq(ret, 0);

   /* when there is any active fence, a signaled fd is returned when the
    * requested seqno is smaller than the first active fence
    */
   for (i = 0; i < test_range; i++) {
      ret = virgl_renderer_export_fence(target_seqno - 1 - i, &fd);
      ck_assert_int_eq(ret, 0);

      ret = wait_sync_fd(fd, 0);
      ck_assert_int_eq(ret, 0);

      close(fd);
   }

   testvirgl_fini_single_ctx();
}
END_TEST

START_TEST(virgl_fence_export_invalid)
{
   const int target_seqno = 50;
   const int target_seqno2 = 55;
   int seqno;
   int fd;
   int ret;

   ret = testvirgl_init_single_ctx(context_flags);
   ck_assert_int_eq(ret, 0);

   ret = virgl_renderer_create_fence(target_seqno, 0);
   ck_assert_int_eq(ret, 0);
   ret = virgl_renderer_create_fence(target_seqno2, 0);
   ck_assert_int_eq(ret, 0);

   for (seqno = target_seqno; seqno <= target_seqno2 + 1; seqno++) {
      ret = virgl_renderer_export_fence(seqno, &fd);
      if (seqno == target_seqno || seqno == target_seqno2) {
         ck_assert_int_eq(ret, 0);
         close(fd);
      } else {
         ck_assert_int_eq(ret, -EINVAL);
      }
   }

   testvirgl_fini_single_ctx();
}
END_TEST
#endif

static Suite *virgl_init_suite(bool include_fence_export)
{
   Suite *s;
   TCase *tc_core;

   s = suite_create("virgl_fence");
   tc_core = tcase_create("fence");

   tcase_add_test(tc_core, virgl_fence_create);
   tcase_add_test(tc_core, virgl_fence_poll);
   tcase_add_test(tc_core, virgl_fence_poll_many);

   if (include_fence_export) {
#ifndef _WIN32
      tcase_add_test(tc_core, virgl_fence_export);
      tcase_add_test(tc_core, virgl_fence_export_signaled);
      tcase_add_test(tc_core, virgl_fence_export_invalid);
#endif
   }

   suite_add_tcase(s, tc_core);

   return s;
}

static bool detect_fence_export_support(void)
{
   int dummy_cookie;
   struct virgl_renderer_callbacks dummy_cbs;
   int fd;
   int ret;

   memset(&dummy_cbs, 0, sizeof(dummy_cbs));
   dummy_cbs.version = 1;

   ret = virgl_renderer_init(&dummy_cookie, context_flags, &dummy_cbs);
   if (ret)
      return false;

   ret = virgl_renderer_export_fence(0, &fd);
   if (ret) {
      virgl_renderer_cleanup(&dummy_cookie);
      return false;
   }

   close(fd);
   virgl_renderer_cleanup(&dummy_cookie);
   return true;
}

int main(void)
{
   Suite *s;
   SRunner *sr;
   int number_failed;
   bool include_fence_export = false;

   if (getenv("VRENDTEST_USE_EGL_SURFACELESS"))
      context_flags |= VIRGL_RENDERER_USE_SURFACELESS;
   if (getenv("VRENDTEST_USE_EGL_GLES")) {
      context_flags |= VIRGL_RENDERER_USE_GLES;
      include_fence_export = detect_fence_export_support();
   }

   s = virgl_init_suite(include_fence_export);
   sr = srunner_create(s);

   srunner_run_all(sr, CK_NORMAL);
   number_failed = srunner_ntests_failed(sr);
   srunner_free(sr);

   return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
