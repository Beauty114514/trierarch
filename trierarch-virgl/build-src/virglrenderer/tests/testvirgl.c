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

/* helper functions for testing purposes */
#include "config.h"

#include <check.h>
#include <errno.h>
#include "pipe/p_defines.h"
#include "util/u_formats.h"
#include "util/u_memory.h"
#include "util/u_format.h"
#include "testvirgl.h"

#include "virgl_hw.h"
#include "vrend/vrend_iov.h"
#include "virglrenderer.h"

int context_flags = VIRGL_RENDERER_USE_EGL;

void testvirgl_init_simple_1d_resource(struct virgl_renderer_resource_create_args *res, int handle)
{
    res->handle = handle;
    res->target = PIPE_TEXTURE_1D;
    res->format = PIPE_FORMAT_B8G8R8X8_UNORM;
    res->width = 50;
    res->height = 1;
    res->depth = 1;
    res->array_size = 1;
    res->last_level = 0;
    res->nr_samples = 0;
    res->bind = PIPE_BIND_SAMPLER_VIEW;
    res->flags = 0;
}

void testvirgl_init_simple_buffer_sized(struct virgl_renderer_resource_create_args *res, int handle, int width)
{
    res->handle = handle;
    res->target = PIPE_BUFFER;
    res->format = PIPE_FORMAT_R8_UNORM;
    res->width = width;
    res->height = 1;
    res->depth = 1;
    res->array_size = 1;
    res->last_level = 0;
    res->nr_samples = 0;
    res->bind = 0;
    res->flags = 0;
}

void testvirgl_init_simple_buffer(struct virgl_renderer_resource_create_args *res, int handle)
{
    testvirgl_init_simple_buffer_sized(res, handle, 50);
}

void testvirgl_init_simple_2d_resource(struct virgl_renderer_resource_create_args *res, int handle)
{
    res->handle = handle;
    res->target = PIPE_TEXTURE_2D;
    res->format = PIPE_FORMAT_B8G8R8X8_UNORM;
    res->width = 50;
    res->height = 50;
    res->depth = 1;
    res->array_size = 1;
    res->last_level = 0;
    res->nr_samples = 0;
    res->bind = PIPE_BIND_SAMPLER_VIEW;
    res->flags = 0;
}


struct myinfo_struct {
  uint32_t test;
};

static struct myinfo_struct mystruct;

static struct virgl_renderer_callbacks test_cbs;

static uint32_t testvirgl_last_fence;
static void testvirgl_write_fence(UNUSED void *cookie, uint32_t fence)
{
   testvirgl_last_fence = fence;
}

uint32_t testvirgl_get_last_fence(void)
{
    return testvirgl_last_fence;
}

void testvirgl_reset_fence(void)
{
   testvirgl_last_fence = 0;
}

int testvirgl_init_single_ctx(uint32_t flags)
{
    int ret;

    test_cbs.version = 1;
    test_cbs.write_fence = testvirgl_write_fence;
    ret = virgl_renderer_init(&mystruct, flags, &test_cbs);
    if (ret)
        return ret;
    return virgl_renderer_context_create(1, strlen("test1"), "test1");
}

void testvirgl_init_single_ctx_nr(void)
{
    int ret = testvirgl_init_single_ctx(context_flags);
    ck_assert_int_eq(ret, 0);
}

void testvirgl_fini_single_ctx(void)
{
    virgl_renderer_context_destroy(1);
    virgl_renderer_cleanup(&mystruct);
}

static void testvirgl_flush(struct virgl_context *ctx)
{
    virgl_renderer_submit_cmd(ctx->cbuf->buf, ctx->ctx_id, ctx->cbuf->cdw);
    ctx->cbuf->cdw = 0;
}

int testvirgl_init_ctx_cmdbuf(struct virgl_context *ctx, uint32_t flags)
{
    int ret;
    ret = testvirgl_init_single_ctx(flags);
    if (ret)
        return ret;

    ctx->flush = testvirgl_flush;
    ctx->ctx_id = 1;
    ctx->cbuf = CALLOC_STRUCT(virgl_cmd_buf);
    if (!ctx->cbuf) {
        testvirgl_fini_single_ctx();
        return ENOMEM;
    }

    ctx->cbuf->buf = CALLOC(1, VIRGL_MAX_CMDBUF_DWORDS * 4);
    if (!ctx->cbuf->buf) {
        FREE(ctx->cbuf);
        testvirgl_fini_single_ctx();
        return ENOMEM;
    }
    return 0;
}

void testvirgl_fini_ctx_cmdbuf(struct virgl_context *ctx)
{
    FREE(ctx->cbuf->buf);
    FREE(ctx->cbuf);
    testvirgl_fini_single_ctx();
}

int testvirgl_ctx_send_cmdbuf(struct virgl_context *ctx)
{
   int res = virgl_renderer_submit_cmd(ctx->cbuf->buf, ctx->ctx_id, ctx->cbuf->cdw);
   ctx->cbuf->cdw = 0;
   return res;
}

int testvirgl_create_backed_simple_2d_res(struct virgl_resource *res,
                                          int handle, int w, int h)
{
    struct virgl_renderer_resource_create_args args;
    uint32_t backing_size;
    int ret;

    testvirgl_init_simple_2d_resource(&args, handle);
    args.width = w;
    args.height = h;
    ret = virgl_renderer_resource_create(&args, NULL, 0);
    if (ret)
       return ret;

    res->handle = handle;
    res->base.target = args.target;
    res->base.format = args.format;

    backing_size = args.width * args.height * util_format_get_blocksize(res->base.format);
    res->iovs = malloc(sizeof(struct iovec));

    res->iovs[0].iov_base = malloc(backing_size);
    res->iovs[0].iov_len = backing_size;
    res->niovs = 1;

    virgl_renderer_resource_attach_iov(res->handle, res->iovs, res->niovs);
    return 0;
}

int testvirgl_create_backed_simple_1d_res(struct virgl_resource *res,
                                          int handle)
{
    struct virgl_renderer_resource_create_args args;
    uint32_t backing_size;
    int ret;

    testvirgl_init_simple_1d_resource(&args, handle);
    ret = virgl_renderer_resource_create(&args, NULL, 0);
    if (ret)
       return ret;

    res->handle = handle;
    res->base.target = args.target;
    res->base.format = args.format;

    backing_size = args.width * util_format_get_blocksize(res->base.format);
    res->iovs = malloc(sizeof(struct iovec));

    res->iovs[0].iov_base = malloc(backing_size);
    res->iovs[0].iov_len = backing_size;
    res->niovs = 1;

    virgl_renderer_resource_attach_iov(res->handle, res->iovs, res->niovs);
    return 0;
}

void testvirgl_destroy_backed_res(struct virgl_resource *res)
{
    struct iovec *iovs;
    int niovs;

    virgl_renderer_resource_detach_iov(res->handle, &iovs, &niovs);

    free(iovs[0].iov_base);
    free(iovs);
    virgl_renderer_resource_unref(res->handle);
}

int testvirgl_create_backed_simple_buffer(struct virgl_resource *res,
                                          int handle, int size, int binding)
{
    struct virgl_renderer_resource_create_args args;
    uint32_t backing_size;
    int ret;

    testvirgl_init_simple_buffer_sized(&args, handle, size);
    args.bind = binding;
    ret = virgl_renderer_resource_create(&args, NULL, 0);
    if (ret)
       return ret;

    res->handle = handle;
    res->base.target = args.target;
    res->base.format = args.format;
    res->base.bind = args.bind;
    backing_size = args.width * args.height * util_format_get_blocksize(res->base.format);
    res->iovs = malloc(sizeof(struct iovec));

    res->iovs[0].iov_base = malloc(backing_size);
    res->iovs[0].iov_len = backing_size;
    res->niovs = 1;

    virgl_renderer_resource_attach_iov(res->handle, res->iovs, res->niovs);
    return 0;
}

int testvirgl_create_unbacked_simple_buffer(struct virgl_resource *res,
                                            int handle, int size, int binding)
{
    struct virgl_renderer_resource_create_args args;
    int ret;

    testvirgl_init_simple_buffer_sized(&args, handle, size);
    args.bind = binding;
    ret = virgl_renderer_resource_create(&args, NULL, 0);
    if (ret)
       return ret;

    res->handle = handle;
    res->base.target = args.target;
    res->base.format = args.format;
    res->base.bind = args.bind;

    return 0;
}

static void *get_caps(void)
{
    uint32_t max_ver, max_size;
    void *caps;

    virgl_renderer_get_cap_set(1, &max_ver, &max_size);
    ck_assert_int_ge(max_ver, 1);
    ck_assert_int_ne(max_size, 0);
    ck_assert_int_ge(max_size, sizeof(struct virgl_caps_v1));
    caps = calloc(1, max_size);

    virgl_renderer_fill_caps(0, 0, caps);
    return caps;
}

uint32_t testvirgl_get_glsl_level_from_caps(void)
{
    uint32_t glsl_level;
    void *caps = get_caps();
    struct virgl_caps_v1 *v1 = (struct virgl_caps_v1*) caps;
    glsl_level = v1->glsl_level;

    free(caps);

    return glsl_level;
}

unsigned testvirgl_get_multisample_from_caps(void)
{
    void *caps = get_caps();
    unsigned multisample;

    struct virgl_caps_v1 *v1 = (struct virgl_caps_v1*) caps;
    multisample = v1->bset.texture_multisample;

    free(caps);

    return multisample;
}
