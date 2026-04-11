/**************************************************************************
 *
 * Copyright (C) 2019 Red Hat Inc.
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
#include <stdio.h>
#include "vrend/vrend_strbuf.h"

/* Test the vrend strbuf implementation */

START_TEST(strbuf_init)
{
   struct vrend_strbuf sb;
   bool ret;
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);
   ck_assert_int_eq(sb.alloc_size, 1024);
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_add_small_string)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[27];
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 26; i++)
      str[i] = 'a' + i;
   str[26] = 0;
   strbuf_append(&sb, str);
   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(sb.size, strlen(sb.buf));
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_add_small_string_twice)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[27];
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 26; i++)
      str[i] = 'a' + i;
   str[26] = 0;
   strbuf_append(&sb, str);
   strbuf_append(&sb, str);
   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(strbuf_get_len(&sb), strlen(sb.buf));
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_add_large_string)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[256];
   ret = strbuf_alloc(&sb, 128);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 255; i++)
      str[i] = 'a' + (i % 26);
   str[255] = 0;
   strbuf_append(&sb, str);

   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(strbuf_get_len(&sb), strlen(sb.buf));
   ck_assert_int_eq(sb.alloc_size, 128 + STRBUF_MIN_MALLOC);
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_add_huge_string)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[2048];
   ret = strbuf_alloc(&sb, 128);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 2047; i++)
      str[i] = 'a' + (i % 26);
   str[2047] = 0;
   strbuf_append(&sb, str);

   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(strbuf_get_len(&sb), strlen(sb.buf));
   ck_assert_int_eq(sb.alloc_size, 2048);
   ck_assert_int_ge(sb.alloc_size, strbuf_get_len(&sb) + 1);
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_test_boundary)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[128];
   ret = strbuf_alloc(&sb, 128);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 127; i++)
      str[i] = 'a' + (i % 26);
   str[127] = 0;
   strbuf_append(&sb, str);
   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(strbuf_get_len(&sb), strlen(sb.buf));
   ck_assert_int_eq(sb.alloc_size, 128);
   ck_assert_int_ge(sb.alloc_size, strbuf_get_len(&sb) + 1);
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_test_boundary2)
{
   struct vrend_strbuf sb;
   bool ret;
   char str[513];
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);

   for (int i = 0; i < 512; i++)
      str[i] = 'a' + (i % 26);
   str[512] = 0;
   strbuf_append(&sb, str);
   strbuf_append(&sb, str);
   ck_assert_int_eq(strbuf_get_error(&sb), false);
   ck_assert_int_eq(strbuf_get_len(&sb), strlen(sb.buf));
   /* we should have 512 + 512 + 1 at least */
   ck_assert_int_ge(sb.alloc_size, strbuf_get_len(&sb) + 1);
   ck_assert_int_gt(sb.alloc_size, 1024);

   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_test_appendf)
{
   struct vrend_strbuf sb;
   bool ret;
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);
   strbuf_appendf(&sb, "%d", 5);
   ck_assert_str_eq(sb.buf, "5");
   strbuf_free(&sb);
}
END_TEST

START_TEST(strbuf_test_appendf_str)
{
   struct vrend_strbuf sb = { 0, };
   bool ret;
   ret = strbuf_alloc(&sb, 1024);
   ck_assert_int_eq(ret, true);
   ck_assert_int_eq(sb.external_buffer, false);
   strbuf_appendf(&sb, "%s5", "hello");
   ck_assert_str_eq(sb.buf, "hello5");
   strbuf_free(&sb);
}
END_TEST


START_TEST(strbuf_test_fixed_string)
{
   struct vrend_strbuf sb;
   bool ret;
   char buf[1024];
   ret = strbuf_alloc_fixed(&sb, buf, 1024);
   ck_assert_int_eq(ret, true);
   ck_assert_int_eq(sb.external_buffer, true);
   strbuf_appendf(&sb, "%s5", "hello");
   ck_assert_str_eq(sb.buf, "hello5");
   strbuf_free(&sb);
}
END_TEST


static Suite *init_suite(void)
{
  Suite *s;
  TCase *tc_core;

  s = suite_create("vrend_strbuf");
  tc_core = tcase_create("strbuf");

  suite_add_tcase(s, tc_core);

  tcase_add_test(tc_core, strbuf_init);
  tcase_add_test(tc_core, strbuf_add_small_string);
  tcase_add_test(tc_core, strbuf_add_small_string_twice);
  tcase_add_test(tc_core, strbuf_add_large_string);
  tcase_add_test(tc_core, strbuf_add_huge_string);
  tcase_add_test(tc_core, strbuf_test_boundary);
  tcase_add_test(tc_core, strbuf_test_boundary2);
  tcase_add_test(tc_core, strbuf_test_appendf);
  tcase_add_test(tc_core, strbuf_test_appendf_str);
  tcase_add_test(tc_core, strbuf_test_fixed_string);
  return s;
}

int main(void)
{
   Suite *s;
   SRunner *sr;
   int number_failed;

   s = init_suite();
   sr = srunner_create(s);

   srunner_run_all(sr, CK_NORMAL);
   number_failed = srunner_ntests_failed(sr);
   srunner_free(sr);
   return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
