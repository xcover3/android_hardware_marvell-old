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

#include "gpu_ceu_internal.h"

gpu_effect_func_t g_effects[] = {
    {
         /*No effect*/
        gpu_ceu_effect_null_init,
        gpu_ceu_effect_null_deinit,
        gpu_ceu_render_common,
    },

    {
        /*Old movie effect*/
        gpu_ceu_effect_oldmovie_init,
        gpu_ceu_effect_oldmovie_deinit,
        gpu_ceu_render_common,
    },

    {
        /*Toon shading effect*/
        gpu_ceu_effect_toonshading_init,
        gpu_ceu_effect_toonshading_deinit,
        gpu_ceu_render_common,
    },

    {
        /*Hatching effect*/
        gpu_ceu_effect_hatching_init,
        gpu_ceu_effect_hatching_deinit,
        gpu_ceu_render_common,
    },

    {
        /*rgb2uyvy*/
        gpu_ceu_rgb2uyvy_init,
        gpu_ceu_rgb2uyvy_deinit,
        gpu_ceu_render_common,
    },

    {
        /*rgb2yuyv*/
        gpu_ceu_rgb2yuyv_init,
        gpu_ceu_rgb2yuyv_deinit,
        gpu_ceu_render_common,
    },

    {
        /*Pencil Sketch Effect*/
        gpu_ceu_effect_pencilsketch_init,
        gpu_ceu_effect_pencilsketch_deinit,
        gpu_ceu_render_pencilsketch,
    },

    {
        /*Glow Effect*/
        gpu_ceu_effect_glow_init,
        gpu_ceu_effect_glow_deinit,
        gpu_ceu_render_glow,
    },

    {
        /*Twist Effect*/
        gpu_ceu_effect_twist_init,
        gpu_ceu_effect_twist_deinit,
        gpu_ceu_render_common,
    },

    {
        /* Frame effect. */
        gpu_ceu_effect_frame_init,
        gpu_ceu_effect_frame_deinit,
        gpu_ceu_render_common,
    },

    {
        /* Sunshine effect. */
        gpu_ceu_effect_sunshine_init,
        gpu_ceu_effect_sunshine_deinit,
        gpu_ceu_render_common,
    },
};

