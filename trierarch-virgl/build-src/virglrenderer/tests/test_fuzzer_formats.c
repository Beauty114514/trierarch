/**************************************************************************
 *
 * Copyright (C) 2019 Collabora Ltd
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
  This file contains tests that triggered bugs revealed by fuzzying
  Thanks Matthew Shao for reporting these.
*/

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "virgl_hw.h"
#include "vrend/vrend_iov.h"
#include "vrend/vrend_winsys_egl.h"
#include "virglrenderer.h"
#include "virgl_protocol.h"
#include <epoxy/egl.h>


struct fuzzer_cookie
{
   int dummy;
};

static struct fuzzer_cookie cookie;
static const uint32_t ctx_id = 1;
static struct virgl_egl *test_egl;

static void fuzzer_write_fence(UNUSED void *opaque, UNUSED uint32_t fence) {}

static virgl_renderer_gl_context
fuzzer_create_gl_context(UNUSED void *cookie, UNUSED int scanout_idx,
                         struct virgl_renderer_gl_ctx_param *param)
{
   struct virgl_gl_ctx_param vparams = {0};
   vparams.shared = false;
   vparams.major_ver = param->major_ver;
   vparams.minor_ver = param->minor_ver;
   return virgl_egl_create_context(test_egl, &vparams);
}

static void fuzzer_destory_gl_context(UNUSED void *cookie, virgl_renderer_gl_context ctx)
{
   virgl_egl_destroy_context(test_egl, ctx);
}

static int fuzzer_make_current(UNUSED void *cookie, UNUSED int scanout_idx,
                               virgl_renderer_gl_context ctx)
{
   return virgl_egl_make_context_current(test_egl, ctx);
}


static struct virgl_renderer_callbacks fuzzer_cbs = {
   .version = 1,
   .write_fence = fuzzer_write_fence,
   .create_gl_context = fuzzer_create_gl_context,
   .destroy_gl_context = fuzzer_destory_gl_context,
   .make_current = fuzzer_make_current,
};

#ifdef _WIN32
static int setenv(const char *name, const char *value, int overwrite)
{
    int errcode = 0;
    if(!overwrite) {
        size_t envsize = 0;
        errcode = getenv_s(&envsize, NULL, 0, name);
        if(errcode || envsize) return errcode;
    }
    return _putenv_s(name, value);
}
#endif

static void initialize_environment(void)
{
   setenv("LIBGL_ALWAYS_SOFTWARE", "true", 0);
   setenv("GALLIUM_DRIVER", "softpipe", 0);

#ifdef ENABLE_GBM
   test_egl = virgl_egl_init(NULL, true, true);
#else
   test_egl = virgl_egl_init(EGL_DEFAULT_DISPLAY, true, true);
#endif
   assert(test_egl);

   virgl_renderer_init(&cookie, VIRGL_RENDERER_USE_GLES|
                       VIRGL_RENDERER_USE_SURFACELESS, &fuzzer_cbs);

   const char *name = "fuzzctx";
   virgl_renderer_context_create(ctx_id, (unsigned)strlen(name), name);
}

static void test_format_wrong_size(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 10;
   args.target = 3;
   args.format = 10;
   args.bind = 10;
   args.width = 2;
   args.height = 0;
   args.depth = 0;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 0x8000001; // s0
   cmd[i++] = 0; // minxy
   cmd[i++] = 0; // maxxy
   cmd[i++] = 10; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = 0x1000029; //dformat
   cmd[i++] = 0; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 0; //dw
   cmd[i++] = 0; // dh
   cmd[i++] = 0; // dd
   cmd[i++] = 10; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = 0; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 0; // sz
   cmd[i++] = 0; // sw
   cmd[i++] = 0; // sh
   cmd[i++] = 0; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}

static void test_format_fail_and_double_free(void)
{
   struct virgl_renderer_resource_create_args args;

   args.handle = 1;
   args.target = 3;
   args.format = 191;
   args.bind = 10;
   args.width = 49;
   args.height = 0;
   args.depth = 0;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);
}




/* Issue #141 */
static void test_blit_info_format_check(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 10;
   args.target = 3;
   args.format = 10;
   args.bind = 10;
   args.width = 2;
   args.height = 1;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 0x8000001; // s0
   cmd[i++] = 0; // minxy
   cmd[i++] = 0; // maxxy
   cmd[i++] = 10; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = 0x1000029; //dformat
   cmd[i++] = 0; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 0; //dw
   cmd[i++] = 0; // dh
   cmd[i++] = 0; // dd
   cmd[i++] = 10; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = 10; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 0; // sz
   cmd[i++] = 0; // sw
   cmd[i++] = 0; // sh
   cmd[i++] = 0; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}

static void test_blit_info_format_check_null_format(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 10;
   args.target = 3;
   args.format = 10;
   args.bind = 10;
   args.width = 2;
   args.height = 1;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 0x8000001; // s0
   cmd[i++] = 0; // minxy
   cmd[i++] = 0; // maxxy
   cmd[i++] = 10; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = 1; //dformat
   cmd[i++] = 0; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 0; //dw
   cmd[i++] = 0; // dh
   cmd[i++] = 0; // dd
   cmd[i++] = 10; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = 0; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 0; // sz
   cmd[i++] = 0; // sw
   cmd[i++] = 0; // sh
   cmd[i++] = 0; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}

/* #142 */
static void  test_format_is_plain_nullptr_deref_trigger(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 10;
   args.target = 0;
   args.format = 126;
   args.bind = 2;
   args.width = 10;
   args.height = 10;
   args.depth = 10;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 0; // s0
   cmd[i++] = 0; // minxy
   cmd[i++] = 0; // maxxy
   cmd[i++] = 10; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = 445382656; //dformat
   cmd[i++] = 3; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 0; //dw
   cmd[i++] = 0; // dh
   cmd[i++] = 0; // dd
   cmd[i++] = 10; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = 126; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 0; // sz
   cmd[i++] = 0; // sw
   cmd[i++] = 3; // sh
   cmd[i++] = 0; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}

/* Issue #143 */
static void test_format_util_format_is_rgb_nullptr_deref_trigger_illegal_resource(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 8;
   args.target = 0;
   args.format = 109;
   args.bind = 8;
   args.width = 2;
   args.height = 0;
   args.depth = 0;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_OBJ_SAMPLER_VIEW_SIZE << 16 | VIRGL_OBJECT_SAMPLER_VIEW << 8 | VIRGL_CCMD_CREATE_OBJECT;
   cmd[i++] = 35; // handle
   cmd[i++] = 8; // res_handle
   cmd[i++] = 3107; //format
   cmd[i++] = 0; //first element
   cmd[i++] = 0; // last element
   cmd[i++] = 0; //swizzle

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1);
}

static void test_format_util_format_is_rgb_nullptr_deref_trigger(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 8;
   args.target = 1;
   args.format = 109;
   args.bind = 8;
   args.width = 2;
   args.height = 2;
   args.depth = 0;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_OBJ_SAMPLER_VIEW_SIZE << 16 | VIRGL_OBJECT_SAMPLER_VIEW << 8 | VIRGL_CCMD_CREATE_OBJECT;
   cmd[i++] = 35; // handle
   cmd[i++] = 8; // res_handle
   cmd[i++] = 3107; //format
   cmd[i++] = 0; //first element
   cmd[i++] = 0; // last element
   cmd[i++] = 0; //swizzle

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1);
}

/* Test as reported in #139 */
static void test_double_free_in_vrend_renderer_blit_int_trigger_invalid_formats(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 1;
   args.target = 0;
   args.format = 262144;
   args.bind = 131072;
   args.width = 1;
   args.height = 1;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   args.handle = 6;
   args.target = 4;
   args.format = 1;
   args.bind = 2;
   args.width = 2;
   args.height = 0;
   args.depth = 1;
   args.array_size = 6;
   args.last_level = 2;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   args.handle = 1;
   args.target = 7;
   args.format = 237;
   args.bind = 1;
   args.width = 6;
   args.height = 0;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 6;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 17113104; // s0
   cmd[i++] = 1; // minxy
   cmd[i++] = 36; // maxxy
   cmd[i++] = 6; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = 0; //dformat
   cmd[i++] = 0; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 6; //dw
   cmd[i++] = 0; // dh
   cmd[i++] = 0; // dd
   cmd[i++] = 1; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = 0; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 268435456; // sz
   cmd[i++] = 0; // sw
   cmd[i++] = 0; // sh
   cmd[i++] = 0; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}

static void test_double_free_in_vrend_renderer_blit_int_trigger(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 1;
   args.target = 2;
   args.format = VIRGL_FORMAT_Z32_UNORM;
   args.bind = VIRGL_BIND_SAMPLER_VIEW;
   args.width = 2;
   args.height = 2;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 1;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   args.handle = 6;
   args.target = 2;
   args.format = VIRGL_FORMAT_Z32_UNORM;
   args.bind = VIRGL_BIND_SAMPLER_VIEW;
   args.width = 2;
   args.height = 2;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   args.handle = 1;
   args.target = 7;
   args.format = VIRGL_FORMAT_Z32_UNORM;
   args.bind = 1;
   args.width = 6;
   args.height = 1;
   args.depth = 1;
   args.array_size = 2;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_CMD_BLIT_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_CMD_BLIT_SIZE << 16 | 0 << 8 | VIRGL_CCMD_BLIT;
   cmd[i++] = 0x30 ; // s0
   cmd[i++] = 1; // minxy
   cmd[i++] = 36; // maxxy
   cmd[i++] = 6; //dhandle
   cmd[i++] = 0; // dlevel
   cmd[i++] = VIRGL_FORMAT_Z32_UNORM; //dformat
   cmd[i++] = 0; //dx
   cmd[i++] = 0; // dy
   cmd[i++] = 0; // dz
   cmd[i++] = 6; //dw
   cmd[i++] = 1; // dh
   cmd[i++] = 1; // dd
   cmd[i++] = 1; //shandle
   cmd[i++] = 0; //slevel
   cmd[i++] = VIRGL_FORMAT_Z32_UNORM; //sformat
   cmd[i++] = 0; //sx
   cmd[i++] = 0; // sy
   cmd[i++] = 0; // sz
   cmd[i++] = 1; // sw
   cmd[i++] = 2; // sh
   cmd[i++] = 1; // sd

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_CMD_BLIT_SIZE + 1);
}


static void test_format_is_has_alpha_nullptr_deref_trigger_original(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 8;
   args.target = 0;
   args.format = 10;
   args.bind = 8;
   args.width = 0;
   args.height = 45;
   args.depth = 35;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;
   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_OBJ_SAMPLER_VIEW_SIZE << 16 | VIRGL_OBJECT_SAMPLER_VIEW << 8 | VIRGL_CCMD_CREATE_OBJECT;
   cmd[i++] = 35; //handle
   cmd[i++] = 8; // res_handle
   cmd[i++] = 524288; //format
   cmd[i++] = 0; //first_ele
   cmd[i++] = 0; //last_ele
   cmd[i++] = 10; //swizzle

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1);
}


static void test_format_is_has_alpha_nullptr_deref_trigger_legal_resource(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 8;
   args.target = 2;
   args.format = 10;
   args.bind = 8;
   args.width = 10;
   args.height = 45;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;
   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1];

   int i = 0;
   cmd[i++] = VIRGL_OBJ_SAMPLER_VIEW_SIZE << 16 | VIRGL_OBJECT_SAMPLER_VIEW << 8 | VIRGL_CCMD_CREATE_OBJECT;
   cmd[i++] = 35; //handle
   cmd[i++] = 8; // res_handle
   cmd[i++] = 524288; //format
   cmd[i++] = 0; //first_ele
   cmd[i++] = 0; //last_ele
   cmd[i++] = 10; //swizzle

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, VIRGL_OBJ_SAMPLER_VIEW_SIZE + 1);
}

static void test_heap_overflow_vrend_renderer_transfer_write_iov(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 4;
   args.target = 0;
   args.format = 4;
   args.bind = 131072;
   args.width = 0;
   args.height = 1;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 0;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   char data[16];
   memset(data, 'A', 16);
   uint32_t cmd[11 + 4 +1];

   int i = 0;
   cmd[i++] = (11+4) << 16 | 0 << 8 | VIRGL_CCMD_RESOURCE_INLINE_WRITE;
   cmd[i++] = 4; // handle
   cmd[i++] = 0; // level
   cmd[i++] = 0; // usage
   cmd[i++] = 0; // stride
   cmd[i++] = 0; // layer_stride
   cmd[i++] = 0; // x
   cmd[i++] = 0; // y
   cmd[i++] = 0; // z
   cmd[i++] = 0x80000000; // w
   cmd[i++] = 0; // h
   cmd[i++] = 0; // d
   memcpy(&cmd[i], data, 16);

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, 11 + 4 + 1);
}

static void test_heap_overflow_vrend_renderer_transfer_write_iov_compressed_tex(void)
{
   struct virgl_renderer_resource_create_args args;
   args.handle = 1;
   args.target = 5;
   args.format = 203;
   args.bind = 1;
   args.width = 100;
   args.height = 1;
   args.depth = 1;
   args.array_size = 0;
   args.last_level = 0;
   args.nr_samples = 0;
   args.flags = 1;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   char data[16];
   memset(data, 'A', 16);
   uint32_t cmd[11 + 4 +1];

   int i = 0;
   cmd[i++] = (11+4) << 16 | 0 << 8 | VIRGL_CCMD_RESOURCE_INLINE_WRITE;
   cmd[i++] = 1; // handle
   cmd[i++] = 0; // level
   cmd[i++] = 0; // usage
   cmd[i++] = 135168; // stride
   cmd[i++] = 655361; // layer_stride
   cmd[i++] = 1; // x
   cmd[i++] = 0; // y
   cmd[i++] = 0; // z
   cmd[i++] = 5; // w
   cmd[i++] = 1; // h
   cmd[i++] = 0; // d
   memcpy(&cmd[i], data, 16);

   virgl_renderer_submit_cmd((void *) cmd, ctx_id, 11 + 4 + 1);
}


static void test_cs_nullpointer_deference(void)
{

   struct virgl_renderer_resource_create_args args;
   args.handle = 0x6e735f72;
   args.target = 2;
   args.format = 0x101;
   args.bind = 0x19191919;
   args.width = 0x19191919;
   args.height = 0x19191919;
   args.depth = 0x411959;
   args.array_size = 0;
   args.last_level = 0x19190000;
   args.nr_samples = 0;
   args.flags = 0x31313100;

   virgl_renderer_resource_create(&args, NULL, 0);
   virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

   uint32_t cmd[9];
   int i = 0;
   cmd[i++] = 0x0083925;
   cmd[i++] = 0x00313131;
   cmd[i++] = 0;
   cmd[i++] = 0;
   cmd[i++] = 0;
   cmd[i++] = 0x25313131;
   cmd[i++] = 0x39;
   cmd[i++] = 0x0001370b;
   cmd[i++] = 0x00340000;

  virgl_renderer_submit_cmd((void *) cmd, ctx_id, 9);
}

static void test_vrend_set_signle_abo_heap_overflow(void) {

    struct virgl_renderer_resource_create_args args;
    args.handle = 0x4c474572;
    args.target = 0;
    args.format = 0x43;
    args.bind = 0x80000;
    args.width = 0x5f5f616d;
    args.height = 0x69667562;
    args.depth = 0x726f706d;
    args.array_size = 0xbbbbbb74;
    args.last_level = 0xbbbbbbbb;
    args.nr_samples = 0xbbbbbbbb;
    args.flags = 0xff;

    virgl_renderer_resource_create(&args, NULL, 0);
    virgl_renderer_ctx_attach_resource(ctx_id, args.handle);

    uint32_t cmd[0xde];
    int i = 0;

    cmd[i++] = 0x000e1919;
    cmd[i++] = 0x00003f00;
    cmd[i++] = 0xc7cf3000;
    cmd[i++] = 0x00083907;
    cmd[i++] = 0x6e73735f;
    cmd[i++] = 0x32323232;
    cmd[i++] = 0x19312161;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0xffbe1959;
    cmd[i++] = 0xbbbbbbff;
    cmd[i++] = 0xbbbbbb29;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x000e1928;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x4111d000;
    cmd[i++] = 0xfe010000;
    cmd[i++] = 0x00000172;
    cmd[i++] = 0x32323200;
    cmd[i++] = 0xe6cedea2;
    cmd[i++] = 0xe6e6e6e6;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0xffbe1959;
    cmd[i++] = 0xbbbbbbff;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x000e1919;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xc7cfa400;
    cmd[i++] = 0x00083907;
    cmd[i++] = 0x6e73735f;
    cmd[i++] = 0x32323232;
    cmd[i++] = 0x19312161;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x00000159;
    cmd[i++] = 0xbbbbbb00;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x006e1928;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbeee3000;
    cmd[i++] = 0xe6e6ffff;
    cmd[i++] = 0x19e6e6e6;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x59191919;
    cmd[i++] = 0xffffbe19;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xffbbbbbb;
    cmd[i++] = 0x19000000;
    cmd[i++] = 0x00000e19;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x07c7cfa4;
    cmd[i++] = 0x5f000839;
    cmd[i++] = 0x326e7373;
    cmd[i++] = 0x00390732;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x4111d000;
    cmd[i++] = 0xfe010000;
    cmd[i++] = 0x00000172;
    cmd[i++] = 0x32323200;
    cmd[i++] = 0xe6cedea2;
    cmd[i++] = 0xe6e6e6e6;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0xffbe1959;
    cmd[i++] = 0xbbbbbbff;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x000e1919;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xc7cfa400;
    cmd[i++] = 0x00083907;
    cmd[i++] = 0x6e73735f;
    cmd[i++] = 0x32323232;
    cmd[i++] = 0x19312161;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x00000159;
    cmd[i++] = 0xbbbbbb00;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x002e1928;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbeee3000;
    cmd[i++] = 0xe6e6ffff;
    cmd[i++] = 0x19e6e6e6;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x59191919;
    cmd[i++] = 0xffffbe19;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xffbbbbbb;
    cmd[i++] = 0x19000000;
    cmd[i++] = 0x00000a19;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x07c7cfa4;
    cmd[i++] = 0x5f000839;
    cmd[i++] = 0x326e7373;
    cmd[i++] = 0x08390732;
    cmd[i++] = 0x73735f00;
    cmd[i++] = 0x3232326e;
    cmd[i++] = 0x31216132;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x00015919;
    cmd[i++] = 0xbbbb0000;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x00bbbbbb;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbbbb0000;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x002e1928;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x08ee3000;
    cmd[i++] = 0x73735f00;
    cmd[i++] = 0x3232326e;
    cmd[i++] = 0x31216132;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x00015919;
    cmd[i++] = 0xbbbb0000;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x00bbbbbb;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbbbb0000;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x002e1928;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbeee3000;
    cmd[i++] = 0xe6e6ffff;
    cmd[i++] = 0x19e6e6e6;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x59191919;
    cmd[i++] = 0xffffbe19;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xffbbbbbb;
    cmd[i++] = 0x19000000;
    cmd[i++] = 0x61323219;
    cmd[i++] = 0x19193121;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0x19191919;
    cmd[i++] = 0xbbbbbb19;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0xffbbbbbb;
    cmd[i++] = 0x28000000;
    cmd[i++] = 0x00002e19;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xffbeee30;
    cmd[i++] = 0x00cffeff;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00006161;
    cmd[i++] = 0x315d3100;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xbb000000;
    cmd[i++] = 0xbbbbbbbb;
    cmd[i++] = 0x000000ff;
    cmd[i++] = 0x000e1919;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0xc7cfa400;
    cmd[i++] = 0x7865745f;
    cmd[i++] = 0x00000000;
    cmd[i++] = 0x65727574;
    cmd[i++] = 0x0b87765f;
    cmd[i++] = 0x40000137;
    cmd[i++] = 0x00004000;
    cmd[i++] = 0x00340034;

    virgl_renderer_submit_cmd((void *) cmd, ctx_id, 0xde);
}

static void test_vrend_set_shader_images_overflow(void)
{
    uint32_t num_shaders = PIPE_MAX_SHADER_IMAGES + 1;
    uint32_t size = num_shaders * VIRGL_SET_SHADER_IMAGE_ELEMENT_SIZE + 3;
    uint32_t cmd[size];
    int i = 0;
    cmd[i++] = ((size - 1)<< 16) | 0 << 8 | VIRGL_CCMD_SET_SHADER_IMAGES;
    cmd[i++] = PIPE_SHADER_FRAGMENT;
    memset(&cmd[i], 0, size - i);

    virgl_renderer_submit_cmd((void *) cmd, ctx_id, size);
}

/* Test adapted from yaojun8558363@gmail.com:
 * https://gitlab.freedesktop.org/virgl/virglrenderer/-/issues/250
*/
static void test_vrend_3d_resource_overflow(void) {

    struct virgl_renderer_resource_create_args resource;
    resource.handle = 0x4c474572;
    resource.target = PIPE_TEXTURE_2D_ARRAY;
    resource.format = VIRGL_FORMAT_Z24X8_UNORM;
    resource.nr_samples = 2;
    resource.last_level = 0;
    resource.array_size = 3;
    resource.bind = VIRGL_BIND_SAMPLER_VIEW;
    resource.depth = 1;
    resource.width = 8;
    resource.height = 4;
    resource.flags = 0;

    virgl_renderer_resource_create(&resource, NULL, 0);
    virgl_renderer_ctx_attach_resource(ctx_id, resource.handle);

    uint32_t size = 0x400;
    uint32_t cmd[size];
    int i = 0;
    cmd[i++] = (size - 1) << 16 | 0 << 8 | VIRGL_CCMD_RESOURCE_INLINE_WRITE;
    cmd[i++] = resource.handle;
    cmd[i++] = 0; // level
    cmd[i++] = 0; // usage
    cmd[i++] = 0; // stride
    cmd[i++] = 0; // layer_stride
    cmd[i++] = 0; // x
    cmd[i++] = 0; // y
    cmd[i++] = 0; // z
    cmd[i++] = 8; // w
    cmd[i++] = 4; // h
    cmd[i++] = 3; // d
    memset(&cmd[i], 0, size - i);

    virgl_renderer_submit_cmd((void *) cmd, ctx_id, size);
}


int main(void)
{
   initialize_environment();

   test_format_wrong_size();
   test_format_fail_and_double_free();
   test_blit_info_format_check();
   test_blit_info_format_check_null_format();
   test_format_is_plain_nullptr_deref_trigger();
   test_format_util_format_is_rgb_nullptr_deref_trigger_illegal_resource();
   test_format_util_format_is_rgb_nullptr_deref_trigger();
   test_double_free_in_vrend_renderer_blit_int_trigger_invalid_formats();
   test_double_free_in_vrend_renderer_blit_int_trigger();
   test_format_is_has_alpha_nullptr_deref_trigger_original();
   test_format_is_has_alpha_nullptr_deref_trigger_legal_resource();

   test_heap_overflow_vrend_renderer_transfer_write_iov();
   test_heap_overflow_vrend_renderer_transfer_write_iov_compressed_tex();

   test_cs_nullpointer_deference();
   test_vrend_set_signle_abo_heap_overflow();

   test_vrend_set_shader_images_overflow();
   test_vrend_3d_resource_overflow();

   virgl_renderer_context_destroy(ctx_id);
   virgl_renderer_cleanup(&cookie);
   virgl_egl_destroy(test_egl);

   return 0;
}
