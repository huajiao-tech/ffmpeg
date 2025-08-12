/* Copyright (c) 2019, iQIYI CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <string.h>

#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_cuda_internal.h"
#include "libavutil/cuda_check.h"
#include "libavutil/internal.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/eval.h"

#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

#include "cuda/load_helper.h" //CS by lzy

static const char *const var_names[] = {
    "in_w", "iw",   ///< width  of the input video
    "in_h", "ih",   ///< height of the input video
    "out_w", "ow",  ///< width  of the cropped video
    "out_h", "oh",  ///< height of the cropped video
    "x",
    "y",
    NULL
};

enum var_name {
    VAR_IN_W,  VAR_IW,
    VAR_IN_H,  VAR_IH,
    VAR_OUT_W, VAR_OW,
    VAR_OUT_H, VAR_OH,
    VAR_X,
    VAR_Y,
    VAR_VARS_NB
};

static const enum AVPixelFormat supported_formats[] = {
    AV_PIX_FMT_YUV420P,
    AV_PIX_FMT_NV12,
    AV_PIX_FMT_YUV444P,
    AV_PIX_FMT_P010,
    AV_PIX_FMT_P016
};

#define DIV_UP(a, b) ( ((a) + (b) - 1) / (b) )
#define ALIGN_UP(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define NUM_BUFFERS 2
#define BLOCKX 32
#define BLOCKY 16

#define CHECK_CU(x) FF_CUDA_CHECK_DL(ctx, s->hwctx->internal->cuda_dl, x)

typedef struct CUDACropContext {
    const AVClass *class;
    AVCUDADeviceContext *hwctx;
    enum AVPixelFormat in_fmt;
    enum AVPixelFormat out_fmt;

    struct {
        int width;
        int height;
        int left;
        int top;
    } planes_in[3], planes_out[3];

    AVBufferRef *frames_ctx;
    AVFrame     *frame;

    AVFrame *tmp_frame;
    int passthrough;

    /**
     * Output sw format. AV_PIX_FMT_NONE for no conversion.
     */
    enum AVPixelFormat format;

    int w,h,x,y;
    char *w_expr, *h_expr, *x_expr, *y_expr;
    double var_values[VAR_VARS_NB];

    CUcontext   cu_ctx;
    CUmodule    cu_module;
    CUfunction  cu_func_uchar;
    CUfunction  cu_func_uchar2;
    CUfunction  cu_func_uchar4;
    CUfunction  cu_func_ushort;
    CUfunction  cu_func_ushort2;
    CUfunction  cu_func_ushort4;
    CUstream    cu_stream;

    CUdeviceptr srcBuffer;
    CUdeviceptr dstBuffer;
    int         tex_alignment;
} CUDACropContext;

static av_cold int cudacrop_init(AVFilterContext *ctx)
{
    CUDACropContext *s = ctx->priv;

    s->format = AV_PIX_FMT_NONE;
    s->frame = av_frame_alloc();
    if (!s->frame)
        return AVERROR(ENOMEM);

    s->tmp_frame = av_frame_alloc();
    if (!s->tmp_frame)
        return AVERROR(ENOMEM);

    return 0;
}

static av_cold void cudacrop_uninit(AVFilterContext *ctx)
{
    CUDACropContext *s = ctx->priv;

	//cs by lfs
	if (s->hwctx && s->cu_module) {
		CudaFunctions* cu = s->hwctx->internal->cuda_dl;
		CUcontext dummy;

		CHECK_CU(cu->cuCtxPushCurrent(s->hwctx->cuda_ctx));
		CHECK_CU(cu->cuModuleUnload(s->cu_module));
		s->cu_module = NULL;
		CHECK_CU(cu->cuCtxPopCurrent(&dummy));
	}

    av_frame_free(&s->frame);
    av_buffer_unref(&s->frames_ctx);
    av_frame_free(&s->tmp_frame);
}

static int cudacrop_query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pixel_formats[] = {
        AV_PIX_FMT_CUDA, AV_PIX_FMT_NONE,
    };
    AVFilterFormats *pix_fmts = ff_make_format_list(pixel_formats);

    return ff_set_common_formats(ctx, pix_fmts);
}

static av_cold int init_stage(CUDACropContext *s, AVBufferRef *device_ctx)
{
    AVBufferRef *out_ref = NULL;
    AVHWFramesContext *out_ctx;
    int in_sw, in_sh, out_sw, out_sh;
    int ret, i;

    av_pix_fmt_get_chroma_sub_sample(s->in_fmt,  &in_sw,  &in_sh);
    av_pix_fmt_get_chroma_sub_sample(s->out_fmt, &out_sw, &out_sh);
    if (!s->planes_out[0].width) {
        s->planes_out[0].width  = s->planes_in[0].width;
        s->planes_out[0].height = s->planes_in[0].height;
        s->planes_out[0].left = s->planes_in[0].left;
        s->planes_out[0].top  = s->planes_in[0].top;
    }

    for (i = 1; i < FF_ARRAY_ELEMS(s->planes_in); i++) {
        s->planes_in[i].width   = s->planes_in[0].width   >> in_sw;
        s->planes_in[i].height  = s->planes_in[0].height  >> in_sh;
        s->planes_in[i].left    = s->planes_in[0].left    >> in_sw;
        s->planes_in[i].top     = s->planes_in[0].top     >> in_sh;
        s->planes_out[i].width  = s->planes_out[0].width  >> out_sw;
        s->planes_out[i].height = s->planes_out[0].height >> out_sh;
        s->planes_out[i].left   = 0;
        s->planes_out[i].top    = 0;

    }

    out_ref = av_hwframe_ctx_alloc(device_ctx);
    if (!out_ref)
        return AVERROR(ENOMEM);
    out_ctx = (AVHWFramesContext*)out_ref->data;

    out_ctx->format    = AV_PIX_FMT_CUDA;
    out_ctx->sw_format = s->out_fmt;
    out_ctx->width     = FFALIGN(s->planes_out[0].width,  32);
    out_ctx->height    = FFALIGN(s->planes_out[0].height, 32);

    ret = av_hwframe_ctx_init(out_ref);
    if (ret < 0)
        goto fail;

    av_frame_unref(s->frame);
    ret = av_hwframe_get_buffer(out_ref, s->frame, 0);
    if (ret < 0)
        goto fail;

    s->frame->width  = s->planes_out[0].width;
    s->frame->height = s->planes_out[0].height;

    av_buffer_unref(&s->frames_ctx);
    s->frames_ctx = out_ref;

    return 0;
fail:
    av_buffer_unref(&out_ref);
    return ret;
}

static int format_is_supported(enum AVPixelFormat fmt)
{
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(supported_formats); i++)
        if (supported_formats[i] == fmt)
            return 1;
    return 0;
}

static av_cold int init_processing_chain(AVFilterContext *ctx, int in_width, int in_height,
                                         int out_width, int out_height,
                                         int left, int top)
{
    CUDACropContext *s = ctx->priv;

    AVHWFramesContext *in_frames_ctx;

    enum AVPixelFormat in_format;
    enum AVPixelFormat out_format;
    int ret;

    /* check that we have a hw context */
    if (!ctx->inputs[0]->hw_frames_ctx) {
        av_log(ctx, AV_LOG_ERROR, "No hw context provided on input\n");
        return AVERROR(EINVAL);
    }
    in_frames_ctx = (AVHWFramesContext*)ctx->inputs[0]->hw_frames_ctx->data;
    in_format     = in_frames_ctx->sw_format;
    out_format    = (s->format == AV_PIX_FMT_NONE) ? in_format : s->format;

    if (!format_is_supported(in_format)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported input format: %s\n",
               av_get_pix_fmt_name(in_format));
        return AVERROR(ENOSYS);
    }
    if (!format_is_supported(out_format)) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported output format: %s\n",
               av_get_pix_fmt_name(out_format));
        return AVERROR(ENOSYS);
    }

    if (in_width == out_width && in_height == out_height)
        s->passthrough = 1;

    s->in_fmt = in_format;
    s->out_fmt = out_format;

    s->planes_in[0].width   = in_width;
    s->planes_in[0].height  = in_height;
    s->planes_out[0].width  = out_width;
    s->planes_out[0].height = out_height;
    s->planes_in[0].left = left;
    s->planes_in[0].top = top;
    s->planes_out[0].left = 0;
    s->planes_out[0].top = 0;

    ret = init_stage(s, in_frames_ctx->device_ref);
    if (ret < 0)
        return ret;

    ctx->outputs[0]->hw_frames_ctx = av_buffer_ref(s->frames_ctx);
    if (!ctx->outputs[0]->hw_frames_ctx)
        return AVERROR(ENOMEM);

    return 0;
}

static inline int normalize_double(int *n, double d)
{
    int ret = 0;

    if (isnan(d))
        ret = AVERROR(EINVAL);
    else if (d > INT_MAX || d < INT_MIN) {
        *n = d > INT_MAX ? INT_MAX : INT_MIN;
        ret = AVERROR(EINVAL);
    } else
        *n = lrint(d);

    return ret;
}

static av_cold int cudacrop_config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    CUDACropContext *s = ctx->priv;
    double res;
    int ret;

    s->var_values[VAR_IN_W] = s->var_values[VAR_IW] = inlink->w;
    s->var_values[VAR_IN_H] = s->var_values[VAR_IH] = inlink->h;
    s->var_values[VAR_OUT_W] = s->var_values[VAR_OW] = NAN;
    s->var_values[VAR_OUT_H] = s->var_values[VAR_OH] = NAN;
    s->var_values[VAR_X] = NAN;
    s->var_values[VAR_Y] = NAN;
    if ((ret = av_expr_parse_and_eval(&res, s->w_expr,
                                      var_names, s->var_values,
                                      NULL, NULL, NULL, NULL, NULL, 0, ctx)) < 0)
        goto fail;
    s->var_values[VAR_OUT_W] = s->var_values[VAR_OW] = res;
    if ((ret = av_expr_parse_and_eval(&res, s->h_expr,
                                      var_names, s->var_values,
                                      NULL, NULL, NULL, NULL, NULL, 0, ctx)) < 0)
        goto fail;
    s->var_values[VAR_OUT_H] = s->var_values[VAR_OH] = res;
    if ((ret = av_expr_parse_and_eval(&res, s->x_expr,
                                      var_names, s->var_values,
                                      NULL, NULL, NULL, NULL, NULL, 0, ctx)) < 0)
        goto fail;
    s->var_values[VAR_X] = res;
    if ((ret = av_expr_parse_and_eval(&res, s->y_expr,
                                      var_names, s->var_values,
                                      NULL, NULL, NULL, NULL, NULL, 0, ctx)) < 0)
        goto fail;
    s->var_values[VAR_Y] = res;
    if (normalize_double(&s->w, s->var_values[VAR_OW]) < 0 ||
        normalize_double(&s->h, s->var_values[VAR_OH]) < 0 ||
        normalize_double(&s->x, s->var_values[VAR_X]) < 0 ||
        normalize_double(&s->y, s->var_values[VAR_Y]) < 0) {
        av_log(ctx, AV_LOG_ERROR,
               "Too big value or invalid expression for out_w/ow or out_h/oh or x or y");
        return AVERROR(EINVAL);
    }

fail:
    return ret;
}

static av_cold int cudacrop_config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink = outlink->src->inputs[0];
    CUDACropContext *s  = ctx->priv;
    AVHWFramesContext     *frames_ctx = (AVHWFramesContext*)inlink->hw_frames_ctx->data;
    AVCUDADeviceContext *device_hwctx = frames_ctx->device_ctx->hwctx;
    CUcontext dummy, cuda_ctx = device_hwctx->cuda_ctx;
    CudaFunctions *cu = device_hwctx->internal->cuda_dl;
    int ret;

	//CS by lzy
    extern const unsigned char ff_vf_crop_cuda_ptx_data[];
    extern const unsigned int ff_vf_crop_cuda_ptx_len;

    s->hwctx = device_hwctx;
    s->cu_stream = s->hwctx->stream;

    ret = CHECK_CU(cu->cuCtxPushCurrent(cuda_ctx));
    if (ret < 0)
        goto fail;

	//CS by lzy
    //ret = CHECK_CU(cu->cuModuleLoadData(&s->cu_module, vf_crop_cuda_ptx));
    ret = ff_cuda_load_module(s, s->hwctx, &s->cu_module, ff_vf_crop_cuda_ptx_data, ff_vf_crop_cuda_ptx_len);
    if (ret < 0)
        goto fail;

    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_uchar, s->cu_module, "Crop_uchar"));
    if (ret < 0)
        goto fail;
    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_uchar2, s->cu_module, "Crop_uchar2"));
    if (ret < 0)
        goto fail;
    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_uchar4, s->cu_module, "Crop_uchar4"));
    if (ret < 0)
        goto fail;
    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_ushort, s->cu_module, "Crop_ushort"));
    if (ret < 0)
        goto fail;
    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_ushort2, s->cu_module, "Crop_ushort2"));
    if (ret < 0)
        goto fail;
    CHECK_CU(cu->cuModuleGetFunction(&s->cu_func_ushort4, s->cu_module, "Crop_ushort4"));
    if (ret < 0)
        goto fail;

    CHECK_CU(cu->cuCtxPopCurrent(&dummy));

    outlink->w = s->w;
    outlink->h = s->h;

    ret = init_processing_chain(ctx, inlink->w, inlink->h, s->w, s->h, s->x, s->y);
    if (ret < 0)
        return ret;

    if (inlink->sample_aspect_ratio.num) {
        outlink->sample_aspect_ratio = av_mul_q((AVRational){outlink->h*inlink->w,
                                                             outlink->w*inlink->h},
                                                inlink->sample_aspect_ratio);
    } else {
        outlink->sample_aspect_ratio = inlink->sample_aspect_ratio;
    }

    return 0;

fail:
    return ret;
}

static int call_crop_kernel(AVFilterContext *ctx, CUfunction func, int channels,
                              uint8_t *src_dptr, int src_width, int src_height, int src_pitch,
                              uint8_t *dst_dptr, int dst_width, int dst_height, int dst_pitch,
                              int left, int top, int pixel_size)
{
    CUDACropContext *s = ctx->priv;
    CudaFunctions *cu = s->hwctx->internal->cuda_dl;
    CUdeviceptr dst_devptr = (CUdeviceptr)dst_dptr;
    CUtexObject tex = 0;
    void *args_uchar[] = { &tex, &dst_devptr, &dst_width, &dst_height, &dst_pitch, &left, &top };
    int ret;

    CUDA_TEXTURE_DESC tex_desc = {
        .filterMode = CU_TR_FILTER_MODE_LINEAR,
        .flags = CU_TRSF_READ_AS_INTEGER,
    };

    CUDA_RESOURCE_DESC res_desc = {
        .resType = CU_RESOURCE_TYPE_PITCH2D,
        .res.pitch2D.format = pixel_size == 1 ?
                              CU_AD_FORMAT_UNSIGNED_INT8 :
                              CU_AD_FORMAT_UNSIGNED_INT16,
        .res.pitch2D.numChannels = channels,
        .res.pitch2D.width = src_width,
        .res.pitch2D.height = src_height,
        .res.pitch2D.pitchInBytes = src_pitch,
        .res.pitch2D.devPtr = (CUdeviceptr)src_dptr,
    };

    ret = CHECK_CU(cu->cuTexObjectCreate(&tex, &res_desc, &tex_desc, NULL));
    if (ret < 0)
        goto exit;
    
    ret = CHECK_CU(cu->cuLaunchKernel(func, DIV_UP(dst_width, BLOCKX), DIV_UP(dst_height, BLOCKY), 1, BLOCKX, BLOCKY, 1, 0, s->cu_stream, args_uchar, NULL));

exit:
    if (tex)
        CHECK_CU(cu->cuTexObjectDestroy(tex));
    return ret;
}

static int cropcuda_crop_internal(AVFilterContext *ctx,
                            AVFrame *out, AVFrame *in)
{
    AVHWFramesContext *in_frames_ctx = (AVHWFramesContext*)in->hw_frames_ctx->data;
    CUDACropContext *s = ctx->priv;

    switch (in_frames_ctx->sw_format) {
    case AV_PIX_FMT_YUV420P:
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0], in->width, in->height, in->linesize[0],
                           out->data[0], out->width, out->height, out->linesize[0],
                           s->planes_in[0].left, s->planes_in[0].top, 1);
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0]+in->linesize[0]*in->height, in->width/2, in->height/2, in->linesize[0]/2,
                           out->data[0]+out->linesize[0]*out->height, out->width/2, out->height/2, out->linesize[0]/2,
                           s->planes_in[1].left, s->planes_in[1].top, 1);
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0]+ ALIGN_UP((in->linesize[0]*in->height*5)/4, s->tex_alignment), in->width/2, in->height/2, in->linesize[0]/2,
                           out->data[0]+(out->linesize[0]*out->height*5)/4, out->width/2, out->height/2, out->linesize[0]/2,
                           s->planes_in[2].left, s->planes_in[2].top, 1);
        break;
    case AV_PIX_FMT_YUV444P:
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0], in->width, in->height, in->linesize[0],
                           out->data[0], out->width, out->height, out->linesize[0],
                           s->planes_in[0].left, s->planes_in[0].top, 1);
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0]+in->linesize[0]*in->height, in->width, in->height, in->linesize[0],
                           out->data[0]+out->linesize[0]*out->height, out->width, out->height, out->linesize[0],
                           s->planes_in[1].left, s->planes_in[1].top, 1);
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0]+in->linesize[0]*in->height*2, in->width, in->height, in->linesize[0],
                           out->data[0]+out->linesize[0]*out->height*2, out->width, out->height, out->linesize[0],
                           s->planes_in[2].left, s->planes_in[2].top, 1);
        break;
    case AV_PIX_FMT_NV12:
        call_crop_kernel(ctx, s->cu_func_uchar, 1,
                           in->data[0], in->width, in->height, in->linesize[0],
                           out->data[0], out->width, out->height, out->linesize[0],
                           s->planes_in[0].left, s->planes_in[0].top, 1);
        call_crop_kernel(ctx, s->cu_func_uchar2, 2,
                           in->data[1], in->width/2, in->height/2, in->linesize[1],
                           out->data[0] + out->linesize[0] * ((out->height + 31) & ~0x1f), out->width/2, out->height/2, out->linesize[1]/2,
                           s->planes_in[1].left, s->planes_in[1].top, 1);
        break;
    case AV_PIX_FMT_P010LE:
        call_crop_kernel(ctx, s->cu_func_ushort, 1,
                           in->data[0], in->width, in->height, in->linesize[0]/2,
                           out->data[0], out->width, out->height, out->linesize[0]/2,
                           s->planes_in[0].left, s->planes_in[0].top, 2);
        call_crop_kernel(ctx, s->cu_func_ushort2, 2,
                           in->data[1], in->width / 2, in->height / 2, in->linesize[1]/2,
                           out->data[0] + out->linesize[0] * ((out->height + 31) & ~0x1f), out->width / 2, out->height / 2, out->linesize[1] / 4,
                           s->planes_in[1].left, s->planes_in[1].top, 2);
        break;
    case AV_PIX_FMT_P016LE:
        call_crop_kernel(ctx, s->cu_func_ushort, 1,
                           in->data[0], in->width, in->height, in->linesize[0] / 2,
                           out->data[0], out->width, out->height, out->linesize[0] / 2,
                           s->planes_in[0].left, s->planes_in[0].top, 2);
        call_crop_kernel(ctx, s->cu_func_ushort2, 2,
                           in->data[1], in->width / 2, in->height / 2, in->linesize[1] / 2,
                           out->data[0] + out->linesize[0] * ((out->height + 31) & ~0x1f), out->width / 2, out->height / 2, out->linesize[1] / 4,
                           s->planes_in[1].left, s->planes_in[1].top, 2);
        break;
    default:
        return AVERROR_BUG;
    }

    return 0;
}

static int cudacrop_crop(AVFilterContext *ctx, AVFrame *out, AVFrame *in)
{
    CUDACropContext *s = ctx->priv;
    AVFrame *src = in;
    int ret;

    ret = cropcuda_crop_internal(ctx, s->frame, src);
    if (ret < 0)
        return ret;

    src = s->frame;
    ret = av_hwframe_get_buffer(src->hw_frames_ctx, s->tmp_frame, 0);
    if (ret < 0)
        return ret;

    //fixme lfs, not use allign 32
    s->tmp_frame->width  = src->width;
    s->tmp_frame->height = src->height;

    av_frame_move_ref(out, s->frame);
    av_frame_move_ref(s->frame, s->tmp_frame);

    ret = av_frame_copy_props(out, in);
    if (ret < 0)
        return ret;

    return 0;
}

static int cudacrop_filter_frame(AVFilterLink *link, AVFrame *in)
{
    AVFilterContext              *ctx = link->dst;
    CUDACropContext              *s = ctx->priv;
    AVFilterLink             *outlink = ctx->outputs[0];
    CudaFunctions *cu = s->hwctx->internal->cuda_dl;

    AVFrame *out = NULL;
    CUcontext dummy;
    int ret = 0;

    out = av_frame_alloc();
    if (!out) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    ret = CHECK_CU(cu->cuCtxPushCurrent(s->hwctx->cuda_ctx));
    if (ret < 0)
        goto fail;

    ret = cudacrop_crop(ctx, out, in);

    CHECK_CU(cu->cuCtxPopCurrent(&dummy));
    if (ret < 0)
        goto fail;

    av_reduce(&out->sample_aspect_ratio.num, &out->sample_aspect_ratio.den,
              (int64_t)in->sample_aspect_ratio.num * outlink->h * link->w,
              (int64_t)in->sample_aspect_ratio.den * outlink->w * link->h,
              INT_MAX);

    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
fail:
    av_frame_free(&in);
    av_frame_free(&out);
    return ret;
}

#define OFFSET(x) offsetof(CUDACropContext, x)
#define FLAGS (AV_OPT_FLAG_FILTERING_PARAM|AV_OPT_FLAG_VIDEO_PARAM)
static const AVOption options[] = {
    { "w",      "set the width crop area expression",  OFFSET(w_expr),     AV_OPT_TYPE_STRING, { .str = "iw"   }, .flags = FLAGS },
    { "h",      "set the height crop area expression", OFFSET(h_expr),     AV_OPT_TYPE_STRING, { .str = "ih"   }, .flags = FLAGS },
    { "x",      "set the x crop area expression",      OFFSET(x_expr),     AV_OPT_TYPE_STRING, { .str = "(in_w-out_w)/2"}, .flags = FLAGS },
    { "y",      "set the y crop area expression",      OFFSET(y_expr),     AV_OPT_TYPE_STRING, { .str = "(in_h-out_h)/2"}, .flags = FLAGS },
    { NULL },
};

static const AVClass cudacrop_class = {
    .class_name = "cudacrop",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVFilterPad cudacrop_inputs[] = {
    {
        .name        = "default",
        .type        = AVMEDIA_TYPE_VIDEO,
        .filter_frame = cudacrop_filter_frame,
        .config_props = cudacrop_config_input,
    },
    //{ NULL } //fixme lfs
};

static const AVFilterPad cudacrop_outputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
        .config_props = cudacrop_config_output,
    },
    //{ NULL } //fixme lfs
};

AVFilter ff_vf_crop_cuda = {.name = "crop_cuda",
    .description = NULL_IF_CONFIG_SMALL("GPU accelerated video crop"),

    .init = cudacrop_init,
    .uninit = cudacrop_uninit,
    //.query_formats = cudacrop_query_formats,
    FILTER_QUERY_FUNC(cudacrop_query_formats),
    .priv_size = sizeof(CUDACropContext),
    .priv_class = &cudacrop_class,

    //.inputs    = cudacrop_inputs,
    //.outputs   = cudacrop_outputs,
    FILTER_INPUTS(cudacrop_inputs),
    FILTER_OUTPUTS(cudacrop_outputs),

    .flags_internal = FF_FILTER_FLAG_HWFRAME_AWARE,
};
