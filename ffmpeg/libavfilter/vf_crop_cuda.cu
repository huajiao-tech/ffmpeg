/*
 * Copyright (c) 2019, iQIYI CORPORATION. All rights reserved.
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

extern "C" {

__global__ void Crop_uchar(cudaTextureObject_t uchar_tex,
                           unsigned char *dst,
                           int dst_width, int dst_height, int dst_pitch,
                           int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (unsigned char) tex2D<unsigned char>(uchar_tex, xi, yi);
}

__global__ void Crop_uchar2(cudaTextureObject_t uchar2_tex,
                            uchar2 *dst,
                            int dst_width, int dst_height, int dst_pitch,
                            int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (uchar2) tex2D<uchar2>(uchar2_tex, xi, yi);
}

__global__ void Crop_uchar4(cudaTextureObject_t uchar4_tex,
                            uchar4 *dst,
                            int dst_width, int dst_height, int dst_pitch,
                            int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (uchar4) tex2D<uchar4>(uchar4_tex, xi, yi);
}

__global__ void Crop_ushort(cudaTextureObject_t ushort_tex,
                            unsigned short *dst,
                            int dst_width, int dst_height, int dst_pitch,
                            int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (unsigned short) tex2D<unsigned short>(ushort_tex, xi, yi);
}

__global__ void Crop_ushort2(cudaTextureObject_t ushort2_tex,
                             ushort2 *dst,
                             int dst_width, int dst_height, int dst_pitch,
                             int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (ushort2) tex2D<ushort2>(ushort2_tex, xi, yi);
}

__global__ void Crop_ushort4(cudaTextureObject_t ushort4_tex,
                             ushort4 *dst,
                             int dst_width, int dst_height, int dst_pitch,
                             int left, int top)
{
    int xo = blockIdx.x * blockDim.x + threadIdx.x;
    int yo = blockIdx.y * blockDim.y + threadIdx.y;
    int xi = xo + left;
    int yi = yo + top;

    if (yo < dst_height && xo < dst_width)
        dst[yo*dst_pitch+xo] = (ushort4) tex2D<ushort4>(ushort4_tex, xi, yi);
}

}