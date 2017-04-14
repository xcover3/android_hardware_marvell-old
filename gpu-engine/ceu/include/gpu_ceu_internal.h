/***********************************************************************************
 *
 *    Copyright (c) 2012 - 2013 by Marvell International Ltd. and its affiliates.
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

#ifndef _GPU_CEU_INTERNAL_H_
#define _GPU_CEU_INTERNAL_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "gpu_ceu_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
**************************** Internal Function Prototype ****************************
\******************************************************************************/

/* Set the dest buffer for pixels read function. */
ceuSTATUS _gpu_ceu_init_read_buffer(
    CEUContext *pContext
    );

/* stretch factor is the indicator of storage size for each format.*/
ceuSTATUS _gpu_ceu_get_stretch_factor(
    gpu_format_t format,
    CEUfloat *pStretchX,
    CEUfloat *pStretchY
    );

/*
**  Compute the block number needed, and calculate the texture
**  coordinate for each block.
*/
ceuSTATUS _gpu_ceu_init_block_tex_coords(
    CEUContext *pGpuContext
    );

/* Upload source image data to hardware. */
ceuSTATUS _gpu_ceu_upload_image_data(
    CEUContext* pContext,
    CEUvoid* imgDataLogical,
    CEUvoid* imgDataPhysical,
    GPU_FORMAT srcFormat,
    CEUuint srcWidth,
    CEUuint srcHeight,
    CEUuint srcStride
    );

/* Set effect type and parameters. */
ceuSTATUS _gpu_ceu_set_effect(
    CEUContext *pContext,
    gpu_effect_t effect
    );

/*Render function for GPU CEU framework.*/
ceuSTATUS _gpu_ceu_render(
    CEUContext *pContext,
    GPUSurface* src
    );

/* Common drawing function. */
ceuSTATUS gpu_ceu_render_common(
    CEUContext* pContext
    );

/******************************************************************************\
************************** Function Prototype for each effect*************************
\******************************************************************************/

/*No effect.*/
ceuSTATUS gpu_ceu_effect_null_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_null_deinit(CEUContext* pContext);

/*Old Movie effect.*/
ceuSTATUS gpu_ceu_effect_oldmovie_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_oldmovie_deinit(CEUContext* pContext);

/*Toon Shading effect*/
ceuSTATUS gpu_ceu_effect_toonshading_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_toonshading_deinit(CEUContext* pContext);

/*Hatching effect*/
ceuSTATUS gpu_ceu_effect_hatching_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_hatching_deinit(CEUContext* pContext);

/*rgb2uyvy*/
ceuSTATUS gpu_ceu_rgb2uyvy_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_rgb2uyvy_deinit(CEUContext* pContext);

/*rgb2YUYV*/
ceuSTATUS gpu_ceu_rgb2yuyv_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_rgb2yuyv_deinit(CEUContext* pContext);

/*Pencil Sketch Effect.*/
ceuSTATUS gpu_ceu_effect_pencilsketch_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_pencilsketch_deinit(CEUContext* pContext);
ceuSTATUS gpu_ceu_render_pencilsketch(CEUContext* pContext);

/*Glow Effect.*/
ceuSTATUS gpu_ceu_effect_glow_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_glow_deinit(CEUContext* pContext);
ceuSTATUS gpu_ceu_render_glow(CEUContext* pContext);

/*Twist effect.*/
ceuSTATUS gpu_ceu_effect_twist_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_twist_deinit(CEUContext* pContext);

/*Frame effect*/
ceuSTATUS gpu_ceu_effect_frame_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_frame_deinit(CEUContext* pContext);

/*Sunshine effect*/
ceuSTATUS gpu_ceu_effect_sunshine_init(CEUContext* pContext);
ceuSTATUS gpu_ceu_effect_sunshine_deinit(CEUContext* pContext);

#ifdef __cplusplus
}
#endif

#endif

