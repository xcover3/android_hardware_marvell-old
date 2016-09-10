/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#ifndef __gc_hal_user_hardware_config_h_
#define __gc_hal_user_hardware_config_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _gcsHARDWARE_CONFIG_TABLE
{
    gceCHIPMODEL                chipModel;
    gctUINT32                   chipRevision;
    gcsHARDWARE_CONFIG *        config;
    gctBOOL *                   features;
    gctBOOL *                   swwas;
}
gcsHARDWARE_CONFIG_TABLE;

/******************************************************************************\
************************ Hardware Configuration Tables *************************
\******************************************************************************/

/* Configuration tables for gc2000 rev5108. */
gcsHARDWARE_CONFIG gcHARDWARE_Config_gc2000_rev5108 =
{
    gcv2000, /* chipModel */
    0x5108, /* chipRevision */
    0xE0296CAD, /* chipFeatures */
    0xC9799EFF, /* chipMinorFeatures */
    0x2EFBF2D9, /* chipMinorFeatures1 */
    0x00000000, /* chipMinorFeatures2 */
    0x00000000, /* chipMinorFeatures3 */
    0x00000000, /* chipMinorFeatures4 */
    0x94410a68, /* chipSpecs */
    0x00a80000, /* chipSpecs2 */
    0x00000000, /* chipSpecs3 */
    0x00000000, /* chipSpecs4 */
#if gcdENABLE_3D
    8, /* streamCount */
    64, /* registerMax */
    1024, /* threadCount */
    16, /* vertexCacheSize */
    4, /* shaderCoreCount */
#endif
    2, /* pixelPipes */
#if gcdENABLE_3D
    512, /* vertexOutputBufferSize */
    0, /* bufferSize */
    512, /* instructionCount */
    168, /* numConstants */
#if gcdMULTI_GPU
    1, /* gpuCount */
#endif
    12, /* varyingsCount */
    0, /* localStorageSize */
    4, /* l1CacheSize */
    0, /* instructionMemory */
    0, /* shaderPCLength */
    0, /* unifiedConst */
    0x1400, /* vsConstBase */
    0x1C00, /* psConstBase */
    168, /* vsConstMax */
    64, /* psConstMax */
    232, /* constMax */
    1, /* unifiedShader */
    0x3000, /* vsInstBase */
    0x2000, /* psInstBase */
    512, /* vsInstMax */
    512, /* psInstMax */
    512, /* instMax */
    4, /* renderTargets */
    1, /* superTileMode */
#endif
};

gctBOOL gcHARDWARE_Feature_gc2000_rev5108[gcvFEATURE_COUNT] =
{
    0, /* gcvFEATURE_PIPE_2D */
    1, /* gcvFEATURE_PIPE_3D */
    0, /* gcvFEATURE_PIPE_VG */
    0, /* gcvFEATURE_DC */
    0, /* gcvFEATURE_HIGH_DYNAMIC_RANGE */
    1, /* gcvFEATURE_MODULE_CG */
    0, /* gcvFEATURE_MIN_AREA */
    1, /* gcvFEATURE_BUFFER_INTERLEAVING */
    1, /* gcvFEATURE_BYTE_WRITE_2D */
    1, /* gcvFEATURE_ENDIANNESS_CONFIG */
    1, /* gcvFEATURE_DUAL_RETURN_BUS */
    1, /* gcvFEATURE_DEBUG_MODE */
    0, /* gcvFEATURE_YUY2_RENDER_TARGET */
    0, /* gcvFEATURE_FRAGMENT_PROCESSOR */
    0, /* gcvFEATURE_2DPE20 */
    1, /* gcvFEATURE_FAST_CLEAR */
    1, /* gcvFEATURE_YUV420_TILER */
    1, /* gcvFEATURE_YUY2_AVERAGING */
    1, /* gcvFEATURE_FLIP_Y */
    0, /* gcvFEATURE_EARLY_Z */
    1, /* gcvFEATURE_Z_COMPRESSION */
    1, /* gcvFEATURE_MSAA */
    0, /* gcvFEATURE_SPECIAL_ANTI_ALIASING */
    1, /* gcvFEATURE_SPECIAL_MSAA_LOD */
    1, /* gcvFEATURE_422_TEXTURE_COMPRESSION */
    1, /* gcvFEATURE_DXT_TEXTURE_COMPRESSION */
    1, /* gcvFEATURE_ETC1_TEXTURE_COMPRESSION */
    1, /* gcvFEATURE_CORRECT_TEXTURE_CONVERTER */
    1, /* gcvFEATURE_TEXTURE_8K */
    1, /* gcvFEATURE_SCALER */
    1, /* gcvFEATURE_YUV420_SCALER */
    1, /* gcvFEATURE_SHADER_HAS_W */
    0, /* gcvFEATURE_SHADER_HAS_SIGN */
    0, /* gcvFEATURE_SHADER_HAS_FLOOR */
    0, /* gcvFEATURE_SHADER_HAS_CEIL */
    0, /* gcvFEATURE_SHADER_HAS_SQRT */
    0, /* gcvFEATURE_SHADER_HAS_TRIG */
    1, /* gcvFEATURE_VAA */
    1, /* gcvFEATURE_HZ */
    1, /* gcvFEATURE_CORRECT_STENCIL */
    0, /* gcvFEATURE_VG20 */
    0, /* gcvFEATURE_VG_FILTER */
    0, /* gcvFEATURE_VG21 */
    0, /* gcvFEATURE_VG_DOUBLE_BUFFER */
    1, /* gcvFEATURE_MC20 */
    1, /* gcvFEATURE_SUPER_TILED */
    0, /* gcvFEATURE_2D_FILTERBLIT_PLUS_ALPHABLEND */
    0, /* gcvFEATURE_2D_DITHER */
    0, /* gcvFEATURE_2D_A8_TARGET */
    0, /* gcvFEATURE_2D_FILTERBLIT_FULLROTATION */
    0, /* gcvFEATURE_2D_BITBLIT_FULLROTATION */
    1, /* gcvFEATURE_WIDE_LINE */
    0, /* gcvFEATURE_FC_FLUSH_STALL */
    0, /* gcvFEATURE_FULL_DIRECTFB */
    0, /* gcvFEATURE_HALF_FLOAT_PIPE */
    0, /* gcvFEATURE_LINE_LOOP */
    0, /* gcvFEATURE_2D_YUV_BLIT */
    0, /* gcvFEATURE_2D_TILING */
    0, /* gcvFEATURE_NON_POWER_OF_TWO */
    1, /* gcvFEATURE_3D_TEXTURE */
    1, /* gcvFEATURE_TEXTURE_ARRAY */
    0, /* gcvFEATURE_TILE_FILLER */
    0, /* gcvFEATURE_LOGIC_OP */
    0, /* gcvFEATURE_COMPOSITION */
    0, /* gcvFEATURE_MIXED_STREAMS */
    0, /* gcvFEATURE_2D_MULTI_SOURCE_BLT */
    0, /* gcvFEATURE_END_EVENT */
    1, /* gcvFEATURE_VERTEX_10_10_10_2 */
    1, /* gcvFEATURE_TEXTURE_10_10_10_2 */
    1, /* gcvFEATURE_TEXTURE_ANISOTROPIC_FILTERING */
    0, /* gcvFEATURE_TEXTURE_FLOAT_HALF_FLOAT */
    0, /* gcvFEATURE_2D_ROTATION_STALL_FIX */
    0, /* gcvFEATURE_2D_MULTI_SOURCE_BLT_EX */
    0, /* gcvFEATURE_BUG_FIXES10 */
    0, /* gcvFEATURE_2D_MINOR_TILING */
    0, /* gcvFEATURE_TEX_COMPRRESSION_SUPERTILED */
    0, /* gcvFEATURE_FAST_MSAA */
    0, /* gcvFEATURE_BUG_FIXED_INDEXED_TRIANGLE_STRIP */
    0, /* gcvFEATURE_TEXTURE_TILED_READ */
    0, /* gcvFEATURE_DEPTH_BIAS_FIX */
    0, /* gcvFEATURE_RECT_PRIMITIVE */
    0, /* gcvFEATURE_BUG_FIXES11 */
    0, /* gcvFEATURE_SUPERTILED_TEXTURE */
    0, /* gcvFEATURE_2D_NO_COLORBRUSH_INDEX8 */
    0, /* gcvFEATURE_RS_YUV_TARGET */
    0, /* gcvFEATURE_2D_FC_SOURCE */
    0, /* gcvFEATURE_PE_DITHER_FIX */
    0, /* gcvFEATURE_2D_YUV_SEPARATE_STRIDE */
    0, /* gcvFEATURE_FRUSTUM_CLIP_FIX */
    1, /* gcvFEATURE_TEXTURE_SWIZZLE */
    1, /* gcvFEATURE_PRIMITIVE_RESTART */
    0, /* gcvFEATURE_TEXTURE_LINEAR */
    0, /* gcvFEATURE_TEXTURE_YUV_ASSEMBLER */
    0, /* gcvFEATURE_SHADER_HAS_ATOMIC */
    0, /* gcvFEATURE_SHADER_HAS_INSTRUCTION_CACHE */
    0, /* gcvFEATURE_SHADER_ENHANCEMENTS2 */
    0, /* gcvFEATURE_BUG_FIXES7 */
    0, /* gcvFEATURE_SHADER_HAS_RTNE */
    0, /* gcvFEATURE_SHADER_HAS_EXTRA_INSTRUCTIONS2 */
    0, /* gcvFEATURE_SHADER_ENHANCEMENTS3 */
    0, /* gcvFEATURE_DYNAMIC_FREQUENCY_SCALING */
    0, /* gcvFEATURE_SINGLE_BUFFER */
    1, /* gcvFEATURE_OCCLUSION_QUERY */
    0, /* gcvFEATURE_2D_GAMMA */
    0, /* gcvFEATURE_2D_COLOR_SPACE_CONVERSION */
    0, /* gcvFEATURE_2D_SUPER_TILE_VERSION */
    1, /* gcvFEATURE_HALTI0 */
    0, /* gcvFEATURE_HALTI1 */
    0, /* gcvFEATURE_HALTI2 */
    0, /* gcvFEATURE_2D_MIRROR_EXTENSION */
    0, /* gcvFEATURE_TEXTURE_ASTC */
    0, /* gcvFEATURE_2D_SUPER_TILE_V1 */
    0, /* gcvFEATURE_2D_SUPER_TILE_V2 */
    0, /* gcvFEATURE_2D_SUPER_TILE_V3 */
    0, /* gcvFEATURE_2D_MULTI_SOURCE_BLT_EX2 */
    0, /* gcvFEATURE_NEW_RA */
    0, /* gcvFEATURE_BUG_FIXED_IMPLICIT_PRIMITIVE_RESTART */
};

gctBOOL gcHARDWARE_SWWA_gc2000_rev5108[gcvSWWA_COUNT] =
{
    1, /* SWWA 601 */
    1, /* SWWA 706 */
};

gcsHARDWARE_CONFIG_TABLE gcHARDWARE_Config_Tables[] =
{
    {
        gcv2000, /* chipModel */
        0x5108, /* chipRevision */
        &gcHARDWARE_Config_gc2000_rev5108, /* config */
        gcHARDWARE_Feature_gc2000_rev5108, /* features */
        gcHARDWARE_SWWA_gc2000_rev5108      /* swwas */
    },

    /* Last entry. */
    {
        0x0000FFFF,
        0x0000FFFF,
        gcvNULL,
        gcvNULL,
        gcvNULL
    }
};

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_user_hardware_config_h_ */

