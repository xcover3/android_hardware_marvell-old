/***********************************************************************************
 *
 *    Copyright (c) 2013 by Marvell International Ltd. and its affiliates.
 *    All rights reserved.
 *
 *    This software file (the "File") is owned and distributed by Marvell
 *    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
 *    License.
 *
 *    If you received this File from Marvell and you have entered into a commercial
 *    license agreement (a "Commercial License") with Marvell, the File is licensed
 *    to you under the terms of the applicable Commercial License.
 *
 ***********************************************************************************/

#ifndef __GPUTEX_H__
#define __GPUTEX_H__


#define GPU_TEX_TRUE  1
#define GPU_TEX_FALSE 0

#define GPU_MAX_UINT32 0xffffffff


typedef enum _GPU_TEX_FORMAT{
    GPU_TEX_FORMAT_RGB                            = 0x0,
    GPU_TEX_FORMAT_RGBA                           = 0x1,   /* byte order [R G B A] */

    GPU_TEX_FORMAT_RGB8_ETC1_OES                  = 0x8d64,

    GPU_TEX_FORMAT_R11_EAC                        = 0x9270,
    GPU_TEX_FORMAT_SIGNED_R11_EAC                 = 0x9271,
    GPU_TEX_FORMAT_RG11_EAC                       = 0x9272,
    GPU_TEX_FORMAT_SIGNED_RG11_EAC                = 0x9273,
    GPU_TEX_FORMAT_RGB8_ETC2                      = 0x9274,
    GPU_TEX_FORMAT_SRGB8_ETC2                     = 0x9275,
    GPU_TEX_FORMAT_RGB8_PUNCHTHROUGH_ALPHA1_ETC2  = 0x9276,
    GPU_TEX_FORMAT_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277,
    GPU_TEX_FORMAT_RGBA8_ETC2_EAC                 = 0x9278,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_ETC2_EAC          = 0x9279,

    GPU_TEX_FORMAT_RGBA_4x4_ASTC                  = 0x93B0,
    GPU_TEX_FORMAT_RGBA_5x4_ASTC                  = 0x93B1,
    GPU_TEX_FORMAT_RGBA_5x5_ASTC                  = 0x93B2,
    GPU_TEX_FORMAT_RGBA_6x5_ASTC                  = 0x93B3,
    GPU_TEX_FORMAT_RGBA_6x6_ASTC                  = 0x93B4,
    GPU_TEX_FORMAT_RGBA_8x5_ASTC                  = 0x93B5,
    GPU_TEX_FORMAT_RGBA_8x6_ASTC                  = 0x93B6,
    GPU_TEX_FORMAT_RGBA_8x8_ASTC                  = 0x93B7,
    GPU_TEX_FORMAT_RGBA_10x5_ASTC                 = 0x93B8,
    GPU_TEX_FORMAT_RGBA_10x6_ASTC                 = 0x93B9,
    GPU_TEX_FORMAT_RGBA_10x8_ASTC                 = 0x93BA,
    GPU_TEX_FORMAT_RGBA_10x10_ASTC                = 0x93BB,
    GPU_TEX_FORMAT_RGBA_12x10_ASTC                = 0x93BC,
    GPU_TEX_FORMAT_RGBA_12x12_ASTC                = 0x93BD,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_4x4_ASTC          = 0x93D0,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_5x4_ASTC          = 0x93D1,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_5x5_ASTC          = 0x93D2,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_6x5_ASTC          = 0x93D3,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_6x6_ASTC          = 0x93D4,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_8x5_ASTC          = 0x93D5,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_8x6_ASTC          = 0x93D6,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_8x8_ASTC          = 0x93D7,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_10x5_ASTC         = 0x93D8,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_10x6_ASTC         = 0x93D9,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_10x8_ASTC         = 0x93DA,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_10x10_ASTC        = 0x93DB,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_12x10_ASTC        = 0x93DC,
    GPU_TEX_FORMAT_SRGB8_ALPHA8_12x12_ASTC        = 0x93DD,

    GPU_TEX_FORMAT_FORCE_UINT                     = GPU_MAX_UINT32
}GPU_TEX_FORMAT;

typedef enum _GPU_TEX_ASTC_MODE{
    GPU_TEX_DECODE_LDR_SRGB,                     /* 8-bit  depth, LDR */
    GPU_TEX_DECODE_LDR,                          /* 16-bit depth, LDR */
    GPU_TEX_DECODE_HDR,                          /* 16-bit depth, HDR */

    GPU_TEX_ASTC_MODE_FORCE_UINT                  = GPU_MAX_UINT32
}GPU_TEX_ASTC_MODE;

/* check if the compiler is of C++ */
#ifdef __cplusplus
extern "C" {
#endif
int gpu_tex_initialize(void);
void gpu_tex_finalize(void);

int gpu_tex_astc_decoder(void* input_buffer, GPU_TEX_FORMAT input_format, GPU_TEX_ASTC_MODE decode_mode, int width, int height, int stride, GPU_TEX_FORMAT output_format, void* output_buffer);

int gpu_tex_etc2_encoder_fast(void* input_buffer, GPU_TEX_FORMAT input_format, int width, int height, int stride, GPU_TEX_FORMAT output_format, void* output_buffer);
int gpu_tex_etc2_encoder_quality(void* input_buffer, GPU_TEX_FORMAT input_format, int width, int height, int stride, GPU_TEX_FORMAT output_format, void* output_buffer);
int gpu_tex_etc2_decoder(void* input_buffer, GPU_TEX_FORMAT input_format, int width, int height, int stride, GPU_TEX_FORMAT output_format, void* output_buffer);

int gpu_tex_save_tga(void* input_buffer, GPU_TEX_FORMAT input_format, int width, int height, int stride, const char * output_tga_filename);

int gpu_tex_linear2tile_ABGR2ARGB(
     void* pDst, int dstStride,
     const void* pSrc, int srcStride,
     unsigned int x, unsigned int y,
     unsigned int w, unsigned int h);

/* check if the compiler is of C++ */
#ifdef __cplusplus
}
#endif

#endif
