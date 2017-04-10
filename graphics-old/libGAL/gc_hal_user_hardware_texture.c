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


#include "gc_hal_user_hardware_precomp.h"

#if gcdENABLE_3D

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE

/*
 * SuperTileMode 2:
 * 21'{ X[21-1:6], Y[5],X[5],Y[4],X[4], Y[3],X[3],Y[2],X[2],Y[1:0],X[1:0] }
 *
 * SuperTileMode 1:
 * 21'{ X[21-1:6], Y[5:4],X[5:3],Y[3:2], X[2],Y[1:0],X[1:0] }
 *
 * SuperTileMode 0:
 * 21'{ X[21-1:6], Y[5:2],X[5:2], Y[1:0],X[1:0] }
 */
#define gcmSUPERTILE_OFFSET_X(X, Y, SuperTileMode) \
        (SuperTileMode == 2) ? \
            (((X) &  0x03) << 0) | \
            (((Y) &  0x03) << 2) | \
            (((X) &  0x04) << 2) | \
            (((Y) &  0x04) << 3) | \
            (((X) &  0x08) << 3) | \
            (((Y) &  0x08) << 4) | \
            (((X) &  0x10) << 4) | \
            (((Y) &  0x10) << 5) | \
            (((X) &  0x20) << 5) | \
            (((Y) &  0x20) << 6) | \
            (((X) & ~0x3F) << 6)   \
        : \
        (SuperTileMode == 1) ? \
            (((X) &  0x03) << 0) | \
            (((Y) &  0x03) << 2) | \
            (((X) &  0x04) << 2) | \
            (((Y) &  0x0C) << 3) | \
            (((X) &  0x38) << 4) | \
            (((Y) &  0x30) << 6) | \
            (((X) & ~0x3F) << 6)   \
        : \
            (((X) &  0x03) << 0) | \
            (((Y) &  0x03) << 2) | \
            (((X) &  0x3C) << 2) | \
            (((Y) &  0x3C) << 6) | \
            (((X) & ~0x3F) << 6)

#define gcmSUPERTILE_OFFSET_Y(X, Y) \
        ((Y) & ~0x3F)


static gctINT _GetTextureSwizzle(
    IN  gcoHARDWARE Hardware,
    IN gcsSAMPLER_PTR SamplerInfo,
    IN const gceTEXTURE_SWIZZLE *baseComponents
    )
{
    gctINT component;
    gceTEXTURE_SWIZZLE swizzledComponents[gcvTEXTURE_COMPONENT_NUM];

    /* If HW doesn't support the feature, no need to program the state */
    if (!Hardware->features[gcvFEATURE_TEXTURE_SWIZZLE])
    {
        return 0;
    }

    for (component = gcvTEXTURE_COMPONENT_R; component < gcvTEXTURE_COMPONENT_NUM; ++component)
    {
        switch (SamplerInfo->textureInfo->swizzle[component])
        {
        case gcvTEXTURE_SWIZZLE_R:
            swizzledComponents[component] = baseComponents[gcvTEXTURE_COMPONENT_R];
            break;
        case gcvTEXTURE_SWIZZLE_G:
            swizzledComponents[component] = baseComponents[gcvTEXTURE_COMPONENT_G];
            break;
        case gcvTEXTURE_SWIZZLE_B:
            swizzledComponents[component] = baseComponents[gcvTEXTURE_COMPONENT_B];
            break;
        case gcvTEXTURE_SWIZZLE_A:
            swizzledComponents[component] = baseComponents[gcvTEXTURE_COMPONENT_A];
            break;
        case gcvTEXTURE_SWIZZLE_0:
            swizzledComponents[component] = gcvTEXTURE_SWIZZLE_0;
            break;
        case gcvTEXTURE_SWIZZLE_1:
            swizzledComponents[component] = gcvTEXTURE_SWIZZLE_1;
            break;
        default:
            swizzledComponents[component] = gcvTEXTURE_SWIZZLE_INVALID;
        }
        gcmASSERT(swizzledComponents[component] < gcvTEXTURE_SWIZZLE_INVALID);
    }

    return ((swizzledComponents[gcvTEXTURE_COMPONENT_R] << 20) |
            (swizzledComponents[gcvTEXTURE_COMPONENT_G] << 23) |
            (swizzledComponents[gcvTEXTURE_COMPONENT_B] << 26) |
            (swizzledComponents[gcvTEXTURE_COMPONENT_A] << 29));
}

static gctINT _GetTextureFormat(
    IN  gcoHARDWARE Hardware,
    IN  gcsSAMPLER_PTR SamplerInfo,
    OUT gctBOOL *IntegerFormat
    )
{
    if ((SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_YUV) &&
        (SamplerInfo->tiling == gcvLINEAR) &&
        (SamplerInfo->lodNum == 3) &&
        (Hardware->features[gcvFEATURE_TEXTURE_YUV_ASSEMBLER]))
    {
        /* Use YUV Assembler module. */
        static const gceTEXTURE_SWIZZLE yuvSwizzle[] =
        {
            gcvTEXTURE_SWIZZLE_R,
            gcvTEXTURE_SWIZZLE_G,
            gcvTEXTURE_SWIZZLE_B,
            gcvTEXTURE_SWIZZLE_1
        };

        *IntegerFormat = gcvTRUE;

        return (0x13 << 12) |
               _GetTextureSwizzle(Hardware, SamplerInfo, yuvSwizzle);
    }

    if (SamplerInfo->formatInfo->txFormat == gcvINVALID_TEXTURE_FORMAT)
    {
        gcmTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_TEXTURE,
                      "Unknown color format.");

        /* Format is not supported. */
        return -1;
    }

    *IntegerFormat = SamplerInfo->formatInfo->txIntFilter;

    if (SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_DEPTH)
    {
        static const gceTEXTURE_SWIZZLE baseComponents_r001[] =
        {
            gcvTEXTURE_SWIZZLE_R,
            gcvTEXTURE_SWIZZLE_0,
            gcvTEXTURE_SWIZZLE_0,
            gcvTEXTURE_SWIZZLE_1
        };

        static const gceTEXTURE_SWIZZLE baseComponents_rgba[] =
        {
            gcvTEXTURE_SWIZZLE_R,
            gcvTEXTURE_SWIZZLE_G,
            gcvTEXTURE_SWIZZLE_B,
            gcvTEXTURE_SWIZZLE_A
        };

        static const gceTEXTURE_SWIZZLE baseComponents_rg00[] =
        {
            gcvTEXTURE_SWIZZLE_R,
            gcvTEXTURE_SWIZZLE_G,
            gcvTEXTURE_SWIZZLE_0,
            gcvTEXTURE_SWIZZLE_0
        };

        /* GL_OES_depth_texture specify depth texture with unsized internal format, the value should (d,d,d,1.0).
        ** OES3.0 core using sized internal format which produce (d,0,0,1.0).
        ** For gcvSURF_S8D32F_1_G32R32F, it's defined OES3.0, and we need clear G channel.
        */
        const gceTEXTURE_SWIZZLE *baseComponents_depth;

        if (SamplerInfo->formatInfo->format == gcvSURF_S8D32F_1_G32R32F)
        {
            if (Hardware->features[gcvFEATURE_32BPP_COMPONENT_TEXTURE_CHANNEL_SWIZZLE])
            {
                baseComponents_depth = baseComponents_r001;
            }
            else
            {
                baseComponents_depth = baseComponents_rg00;
            }
        }
        else if (SamplerInfo->unsizedDepthTexture)
        {
            baseComponents_depth = SamplerInfo->formatInfo->txSwizzle;
        }
        else if ((SamplerInfo->formatInfo->format != gcvSURF_S8D32F_2_A8R8G8B8) &&
                 (SamplerInfo->formatInfo->format != gcvSURF_D24S8_1_A8R8G8B8))
        {
            baseComponents_depth = baseComponents_r001;
        }
        else
        {
            baseComponents_depth = baseComponents_rgba;
        }

        return SamplerInfo->formatInfo->txFormat |
               _GetTextureSwizzle(Hardware, SamplerInfo, baseComponents_depth);
    }

    /*
    ** There are 2 steps of texture swizzle:
    ** 1. implicit: filter texture value to texture base color Cb. Since HW cannot support it natively, it need
    **              driver help to program the swizzle state.
    ** 2. explicit: which is specified by app, and will swizzle the component of Cb.
    ** The register programmed here should combined the 2 steps.
    */

    return SamplerInfo->formatInfo->txFormat |
           _GetTextureSwizzle(Hardware, SamplerInfo, SamplerInfo->formatInfo->txSwizzle);
}

/* YUV-assembler for linear YUV texture. */
static gctINT _GetYUVControl(
    gceSURF_FORMAT Format,
    gctUINT_PTR YOffset,
    gctUINT_PTR UOffset,
    gctUINT_PTR VOffset
    )
{
    switch (Format)
    {
    case gcvSURF_YV12:
        /* Y... */
        *YOffset = 0;
        /* V... */
        *VOffset = 0;
        /* U... */
        *UOffset = 0;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    case gcvSURF_I420:
        /* Y... */
        *YOffset = 0;
        /* U... */
        *UOffset = 0;
        /* V... */
        *VOffset = 0;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    case gcvSURF_NV12:
        /* Y... */
        *YOffset = 0;
        /* UV... */
        *UOffset = 0;
        *VOffset = 1;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    case gcvSURF_NV21:
        /* Y... */
        *YOffset = 0;
        /* VU... */
        *VOffset = 0;
        *UOffset = 1;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    case gcvSURF_NV16:
        /* Y... */
        *YOffset = 0;
        /* UV... */
        *UOffset = 0;
        *VOffset = 1;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    case gcvSURF_NV61:
        /* Y... */
        *YOffset = 0;
        /* VU... */
        *VOffset = 0;
        *UOffset = 1;

        return ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));

    default:
        gcmTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_TEXTURE,
                      "Unknown yuv format.");
        break;
    }

    /* Format is not supported. */
    return -1;
}


/******************************************************************************\
|*************************** Texture Management Code **************************|
\******************************************************************************/

/*******************************************************************************
**
**  gcoHARDWARE_QueryTexture
**
**  Query if the requested texture is supported.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gceSURF_FORMAT Format
**          Format of texture.
**
**      gceTILING Tiling
**          Tiling of texture, specify linear or tile texture.
**
**      gctUINT Level
**          Level of texture map.
**
**      gctUINT Width
**          Width of texture.
**
**      gctUINT Height
**          Height of texture.
**
**      gctUINT Depth
**          Depth of texture.
**
**      gctUINT Faces
**          Number of faces for texture.
**
**  OUTPUT:
**
**      gctUINT * BlockWidth
**          Pointer to a variable receiving the width of a block of texels.
**
**      gctUINT * BlockHeight
**          Pointer to a variable receiving the height of a block of texels.
**
*/
gceSTATUS
gcoHARDWARE_QueryTexture(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT Format,
    IN gceTILING Tiling,
    IN gctUINT Level,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gctUINT Faces,
    OUT gctUINT * BlockWidth,
    OUT gctUINT * BlockHeight
    )
{
    gceSTATUS status;
    gcsSURF_FORMAT_INFO_PTR formatInfo;

    gcmHEADER_ARG("Hardware=0x%x Format=%d Tiling=%d Level=%u Width=%u Height=%u "
                  "Depth=%u Faces=%u",
                  Hardware, Format, Tiling, Level, Width, Height, Depth, Faces);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(BlockWidth != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(BlockHeight != gcvNULL);

    gcmGETHARDWARE(Hardware);

    /* Non-power of two support. */
#if !gcdUSE_NPOT_PATCH
    if (!Hardware->features[gcvFEATURE_NON_POWER_OF_TWO])
    {
        /* We don't support non-power of two textures beyond level 0. */
        if ((Level > 0)
        &&  ((Width  & (Width  - 1))
            || (Height & (Height - 1))
            )
        )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }
#endif

    /* Check maximum width and height. */
    if (Hardware->features[gcvFEATURE_TEXTURE_8K])
    {
        if ((Width > 8192) || (Height > 8192))
        {
            gcmONERROR(gcvSTATUS_MIPMAP_TOO_LARGE);
        }
    }
    else
    {
        if ((Width > 2048) || (Height > 2048))
        {
            gcmONERROR(gcvSTATUS_MIPMAP_TOO_LARGE);
        }
    }

    /* Check linear texture. */
    if (Tiling == gcvLINEAR)
    {
        /* Is HW support linear texture? */
        if (!Hardware->features[gcvFEATURE_TEXTURE_LINEAR])
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Linear YUV texture need YUV-assembler support in HW. */
        if (((Format == gcvSURF_YV12) ||
              (Format == gcvSURF_I420) ||
              (Format == gcvSURF_NV12) ||
              (Format == gcvSURF_NV21) ||
              (Format == gcvSURF_NV16) ||
              (Format == gcvSURF_NV61)) &&
            (!Hardware->features[gcvFEATURE_TEXTURE_YUV_ASSEMBLER]))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    /* Find the format record. */
    gcmONERROR(gcoHARDWARE_QueryFormat(Format, &formatInfo));

    /* Set the output. */
    *BlockWidth  = formatInfo->blockWidth;
    *BlockHeight = formatInfo->blockHeight;

    /* Success. */
    gcmFOOTER_ARG("*BlockWidth=%u *BlockHeight=%u",
                  *BlockWidth, *BlockHeight);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_DisableTextureSampler
**
**  Disable a specific sampler.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctINT Sampler
**          Sampler number.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_DisableTextureSampler(
    IN gcoHARDWARE Hardware,
    IN gctINT Sampler
    )
{
    gceSTATUS status;
    gctUINT32 hwMode;

    gcmHEADER_ARG("Hardware=0x%x Sampler=%d",
                  Hardware, Sampler);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    if (((((gctUINT32) (Hardware->config->chipMinorFeatures2)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))))
    {
        /* Make sure the sampler index is within range. */
        if ((Sampler < 0) || (Sampler >= gcdSAMPLERS))
        {
            /* Invalid sampler. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }
    else
    {
        /* Make sure the sampler index is within range. */
        if ((Sampler < 0) || (Sampler >= 16))
        {
            /* Invalid sampler. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    /* Setting the Texture Sampler mode to 0, shall disable the sampler. */
    hwMode = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

    if (hwMode != Hardware->hwTxSamplerMode[Sampler])
    {
        /* Save dirty sampler mode register. */
        Hardware->hwTxSamplerMode[Sampler] = hwMode;
        Hardware->hwTxSamplerModeDirty    |= 1 << Sampler;

        Hardware->samplerDirty |= Hardware->hwTxSamplerModeDirty ;
        Hardware->textureDirty = gcvTRUE;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static void
_UploadA8toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = srcLine[0] << 24;
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[0] << 24;
                ((gctUINT32_PTR) trgLine)[1] = srcLine[1] << 24;
                ((gctUINT32_PTR) trgLine)[2] = srcLine[2] << 24;
                ((gctUINT32_PTR) trgLine)[3] = srcLine[3] << 24;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = srcLine[SourceStride * 0] << 24;
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* A8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = src[0] << 24;
            ((gctUINT32_PTR) trgLine)[ 1] = src[1] << 24;
            ((gctUINT32_PTR) trgLine)[ 2] = src[2] << 24;
            ((gctUINT32_PTR) trgLine)[ 3] = src[3] << 24;

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = src[0] << 24;
            ((gctUINT32_PTR) trgLine)[ 5] = src[1] << 24;
            ((gctUINT32_PTR) trgLine)[ 6] = src[2] << 24;
            ((gctUINT32_PTR) trgLine)[ 7] = src[3] << 24;

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = src[0] << 24;
            ((gctUINT32_PTR) trgLine)[ 9] = src[1] << 24;
            ((gctUINT32_PTR) trgLine)[ 0] = src[2] << 24;
            ((gctUINT32_PTR) trgLine)[11] = src[3] << 24;

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = src[0] << 24;
            ((gctUINT32_PTR) trgLine)[13] = src[1] << 24;
            ((gctUINT32_PTR) trgLine)[14] = src[2] << 24;
            ((gctUINT32_PTR) trgLine)[15] = src[3] << 24;

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1;
        }
    }
}

static void
_UploadA8toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (gctUINT32) srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (gctUINT32) srcLine[0];
                ((gctUINT32_PTR) trgLine)[1] = (gctUINT32) srcLine[1];
                ((gctUINT32_PTR) trgLine)[2] = (gctUINT32) srcLine[2];
                ((gctUINT32_PTR) trgLine)[3] = (gctUINT32) srcLine[3];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = (gctUINT32) srcLine[SourceStride * 0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* A8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (gctUINT32) src[0];
            ((gctUINT32_PTR) trgLine)[ 1] = (gctUINT32) src[1];
            ((gctUINT32_PTR) trgLine)[ 2] = (gctUINT32) src[2];
            ((gctUINT32_PTR) trgLine)[ 3] = (gctUINT32) src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (gctUINT32) src[0];
            ((gctUINT32_PTR) trgLine)[ 5] = (gctUINT32) src[1];
            ((gctUINT32_PTR) trgLine)[ 6] = (gctUINT32) src[2];
            ((gctUINT32_PTR) trgLine)[ 7] = (gctUINT32) src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (gctUINT32) src[0];
            ((gctUINT32_PTR) trgLine)[ 9] = (gctUINT32) src[1];
            ((gctUINT32_PTR) trgLine)[ 0] = (gctUINT32) src[2];
            ((gctUINT32_PTR) trgLine)[11] = (gctUINT32) src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (gctUINT32) src[0];
            ((gctUINT32_PTR) trgLine)[13] = (gctUINT32) src[1];
            ((gctUINT32_PTR) trgLine)[14] = (gctUINT32) src[2];
            ((gctUINT32_PTR) trgLine)[15] = (gctUINT32) src[3];

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1;
        }
    }
}

static void
_UploadBGRtoARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2];
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[ 4] << 8) | srcLine[ 5];
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | (srcLine[6] << 16) | (srcLine[ 7] << 8) | srcLine[ 8];
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | (srcLine[9] << 16) | (srcLine[10] << 8) | srcLine[11];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 3;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* BGR to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (src[0] << 16) | (src[ 1] << 8) | src[ 2];
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | (src[3] << 16) | (src[ 4] << 8) | src[ 5];
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | (src[6] << 16) | (src[ 7] << 8) | src[ 8];
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | (src[9] << 16) | (src[10] << 8) | src[11];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF000000 | (src[0] << 16) | (src[ 1] << 8) | src[ 2];
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF000000 | (src[3] << 16) | (src[ 4] << 8) | src[ 5];
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF000000 | (src[6] << 16) | (src[ 7] << 8) | src[ 8];
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF000000 | (src[9] << 16) | (src[10] << 8) | src[11];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF000000 | (src[0] << 16) | (src[ 1] << 8) | src[ 2];
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF000000 | (src[3] << 16) | (src[ 4] << 8) | src[ 5];
            ((gctUINT32_PTR) trgLine)[10] = 0xFF000000 | (src[6] << 16) | (src[ 7] << 8) | src[ 8];
            ((gctUINT32_PTR) trgLine)[11] = 0xFF000000 | (src[9] << 16) | (src[10] << 8) | src[11];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = 0xFF000000 | (src[0] << 16) | (src[ 1] << 8) | src[ 2];
            ((gctUINT32_PTR) trgLine)[13] = 0xFF000000 | (src[3] << 16) | (src[ 4] << 8) | src[ 5];
            ((gctUINT32_PTR) trgLine)[14] = 0xFF000000 | (src[6] << 16) | (src[ 7] << 8) | src[ 8];
            ((gctUINT32_PTR) trgLine)[15] = 0xFF000000 | (src[9] << 16) | (src[10] << 8) | src[11];

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 3;
        }
    }
}

static void
_UploadBGRtoARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[1] << 16) | (srcLine[2] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[3] << 8) | (srcLine[ 4] << 16) | (srcLine[ 5] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[6] << 8) | (srcLine[ 7] << 16) | (srcLine[ 8] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 3;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* BGR to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[0] << 8) | (src[ 1] << 16) | (src[ 2] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (src[3] << 8) | (src[ 4] << 16) | (src[ 5] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (src[6] << 8) | (src[ 7] << 16) | (src[ 8] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (src[9] << 8) | (src[10] << 16) | (src[11] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[0] << 8) | (src[ 1] << 16) | (src[ 2] << 24);
            ((gctUINT32_PTR) trgLine)[ 5] = (src[3] << 8) | (src[ 4] << 16) | (src[ 5] << 24);
            ((gctUINT32_PTR) trgLine)[ 6] = (src[6] << 8) | (src[ 7] << 16) | (src[ 8] << 24);
            ((gctUINT32_PTR) trgLine)[ 7] = (src[9] << 8) | (src[10] << 16) | (src[11] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[0] << 8) | (src[ 1] << 16) | (src[ 2] << 24);
            ((gctUINT32_PTR) trgLine)[ 9] = (src[3] << 8) | (src[ 4] << 16) | (src[ 5] << 24);
            ((gctUINT32_PTR) trgLine)[10] = (src[6] << 8) | (src[ 7] << 16) | (src[ 8] << 24);
            ((gctUINT32_PTR) trgLine)[11] = (src[9] << 8) | (src[10] << 16) | (src[11] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[0] << 8) | (src[ 1] << 16) | (src[ 2] << 24);
            ((gctUINT32_PTR) trgLine)[13] = (src[3] << 8) | (src[ 4] << 16) | (src[ 5] << 24);
            ((gctUINT32_PTR) trgLine)[14] = (src[6] << 8) | (src[ 7] << 16) | (src[ 8] << 24);
            ((gctUINT32_PTR) trgLine)[15] = (src[9] << 8) | (src[10] << 16) | (src[11] << 24);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 3;
        }
    }
}

static void
_UploadBGRtoABGR(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | srcLine[3] | (srcLine[ 4] << 8) | (srcLine[ 5] << 16);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | srcLine[6] | (srcLine[ 7] << 8) | (srcLine[ 8] << 16);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | srcLine[9] | (srcLine[10] << 8) | (srcLine[11] << 16);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 3;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* BGR to ABGR: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | src[0] | (src[ 1] << 8) | (src[ 2] << 16);
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | src[3] | (src[ 4] << 8) | (src[ 5] << 16);
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | src[6] | (src[ 7] << 8) | (src[ 8] << 16);
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | src[9] | (src[10] << 8) | (src[11] << 16);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF000000 | src[0] | (src[ 1] << 8) | (src[ 2] << 16);
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF000000 | src[3] | (src[ 4] << 8) | (src[ 5] << 16);
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF000000 | src[6] | (src[ 7] << 8) | (src[ 8] << 16);
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF000000 | src[9] | (src[10] << 8) | (src[11] << 16);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF000000 | src[0] | (src[ 1] << 8) | (src[ 2] << 16);
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF000000 | src[3] | (src[ 4] << 8) | (src[ 5] << 16);
            ((gctUINT32_PTR) trgLine)[10] = 0xFF000000 | src[6] | (src[ 7] << 8) | (src[ 8] << 16);
            ((gctUINT32_PTR) trgLine)[11] = 0xFF000000 | src[9] | (src[10] << 8) | (src[11] << 16);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = 0xFF000000 | src[0] | (src[ 1] << 8) | (src[ 2] << 16);
            ((gctUINT32_PTR) trgLine)[13] = 0xFF000000 | src[3] | (src[ 4] << 8) | (src[ 5] << 16);
            ((gctUINT32_PTR) trgLine)[14] = 0xFF000000 | src[6] | (src[ 7] << 8) | (src[ 8] << 16);
            ((gctUINT32_PTR) trgLine)[15] = 0xFF000000 | src[9] | (src[10] << 8) | (src[11] << 16);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 3;
        }
    }
}

static void
_UploadBGRtoABGRBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[3] << 24) | (srcLine[ 4] << 16) | (srcLine[ 5] << 8);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[6] << 24) | (srcLine[ 7] << 16) | (srcLine[ 8] << 8);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[9] << 24) | (srcLine[10] << 16) | (srcLine[11] << 8);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 3;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* BGR to ABGR: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[0] << 24) | (src[ 1] << 16) | (src[ 2] << 8);
            ((gctUINT32_PTR) trgLine)[ 1] = (src[3] << 24) | (src[ 4] << 16) | (src[ 5] << 8);
            ((gctUINT32_PTR) trgLine)[ 2] = (src[6] << 24) | (src[ 7] << 16) | (src[ 8] << 8);
            ((gctUINT32_PTR) trgLine)[ 3] = (src[9] << 24) | (src[10] << 16) | (src[11] << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[0] << 24) | (src[ 1] << 16) | (src[ 2] << 8);
            ((gctUINT32_PTR) trgLine)[ 5] = (src[3] << 24) | (src[ 4] << 16) | (src[ 5] << 8);
            ((gctUINT32_PTR) trgLine)[ 6] = (src[6] << 24) | (src[ 7] << 16) | (src[ 8] << 8);
            ((gctUINT32_PTR) trgLine)[ 7] = (src[9] << 24) | (src[10] << 16) | (src[11] << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[0] << 24) | (src[ 1] << 16) | (src[ 2] << 8);
            ((gctUINT32_PTR) trgLine)[ 9] = (src[3] << 24) | (src[ 4] << 16) | (src[ 5] << 8);
            ((gctUINT32_PTR) trgLine)[10] = (src[6] << 24) | (src[ 7] << 16) | (src[ 8] << 8);
            ((gctUINT32_PTR) trgLine)[11] = (src[9] << 24) | (src[10] << 16) | (src[11] << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[0] << 24) | (src[ 1] << 16) | (src[ 2] << 8);
            ((gctUINT32_PTR) trgLine)[13] = (src[3] << 24) | (src[ 4] << 16) | (src[ 5] << 8);
            ((gctUINT32_PTR) trgLine)[14] = (src[6] << 24) | (src[ 7] << 16) | (src[ 8] << 8);
            ((gctUINT32_PTR) trgLine)[15] = (src[9] << 24) | (src[10] << 16) | (src[11] << 8);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 3;
        }
    }
}

static void
_UploadABGRtoARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2] | (srcLine[3] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2] | (srcLine[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 16) | (srcLine[ 5] << 8) | srcLine[ 6] | (srcLine[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 16) | (srcLine[ 9] << 8) | srcLine[10] | (srcLine[11] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 16) | (srcLine[13] << 8) | srcLine[14] | (srcLine[15] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2] | (srcLine[ 3] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 4;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* ABGR to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[ 0] << 16) | (src[ 1] << 8) | src[ 2] | (src[ 3] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (src[ 4] << 16) | (src[ 5] << 8) | src[ 6] | (src[ 7] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (src[ 8] << 16) | (src[ 9] << 8) | src[10] | (src[11] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (src[12] << 16) | (src[13] << 8) | src[14] | (src[15] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[ 0] << 16) | (src[ 1] << 8) | src[ 2] | (src[ 3] << 24);
            ((gctUINT32_PTR) trgLine)[ 5] = (src[ 4] << 16) | (src[ 5] << 8) | src[ 6] | (src[ 7] << 24);
            ((gctUINT32_PTR) trgLine)[ 6] = (src[ 8] << 16) | (src[ 9] << 8) | src[10] | (src[11] << 24);
            ((gctUINT32_PTR) trgLine)[ 7] = (src[12] << 16) | (src[13] << 8) | src[14] | (src[15] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[ 0] << 16) | (src[ 1] << 8) | src[ 2] | (src[ 3] << 24);
            ((gctUINT32_PTR) trgLine)[ 9] = (src[ 4] << 16) | (src[ 5] << 8) | src[ 6] | (src[ 7] << 24);
            ((gctUINT32_PTR) trgLine)[10] = (src[ 8] << 16) | (src[ 9] << 8) | src[10] | (src[11] << 24);
            ((gctUINT32_PTR) trgLine)[11] = (src[12] << 16) | (src[13] << 8) | src[14] | (src[15] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[ 0] << 16) | (src[ 1] << 8) | src[ 2] | (src[ 3] << 24);
            ((gctUINT32_PTR) trgLine)[13] = (src[ 4] << 16) | (src[ 5] << 8) | src[ 6] | (src[ 7] << 24);
            ((gctUINT32_PTR) trgLine)[14] = (src[ 8] << 16) | (src[ 9] << 8) | src[10] | (src[11] << 24);
            ((gctUINT32_PTR) trgLine)[15] = (src[12] << 16) | (src[13] << 8) | src[14] | (src[15] << 24);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 4;
        }
    }
}

static void
_UploadABGRtoARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[1] << 16) | (srcLine[2] << 24) | srcLine[3];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24) | srcLine[ 3];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 8) | (srcLine[ 5] << 16) | (srcLine[ 6] << 24) | srcLine[ 7];
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 8) | (srcLine[ 9] << 16) | (srcLine[10] << 24) | srcLine[11];
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 8) | (srcLine[13] << 16) | (srcLine[14] << 24) | srcLine[15];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24) | srcLine[ 3];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 4;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* ABGR to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[ 0] << 8) | (src[ 1] << 16) | (src[ 2] << 24) | src[ 3];
            ((gctUINT32_PTR) trgLine)[ 1] = (src[ 4] << 8) | (src[ 5] << 16) | (src[ 6] << 24) | src[ 7];
            ((gctUINT32_PTR) trgLine)[ 2] = (src[ 8] << 8) | (src[ 9] << 16) | (src[10] << 24) | src[11];
            ((gctUINT32_PTR) trgLine)[ 3] = (src[12] << 8) | (src[13] << 16) | (src[14] << 24) | src[15];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[ 0] << 8) | (src[ 1] << 16) | (src[ 2] << 24) | src[ 3];
            ((gctUINT32_PTR) trgLine)[ 5] = (src[ 4] << 8) | (src[ 5] << 16) | (src[ 6] << 24) | src[ 7];
            ((gctUINT32_PTR) trgLine)[ 6] = (src[ 8] << 8) | (src[ 9] << 16) | (src[10] << 24) | src[11];
            ((gctUINT32_PTR) trgLine)[ 7] = (src[12] << 8) | (src[13] << 16) | (src[14] << 24) | src[15];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[ 0] << 8) | (src[ 1] << 16) | (src[ 2] << 24) | src[ 3];
            ((gctUINT32_PTR) trgLine)[ 9] = (src[ 4] << 8) | (src[ 5] << 16) | (src[ 6] << 24) | src[ 7];
            ((gctUINT32_PTR) trgLine)[10] = (src[ 8] << 8) | (src[ 9] << 16) | (src[10] << 24) | src[11];
            ((gctUINT32_PTR) trgLine)[11] = (src[12] << 8) | (src[13] << 16) | (src[14] << 24) | src[15];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[ 0] << 8) | (src[ 1] << 16) | (src[ 2] << 24) | src[ 3];
            ((gctUINT32_PTR) trgLine)[13] = (src[ 4] << 8) | (src[ 5] << 16) | (src[ 6] << 24) | src[ 7];
            ((gctUINT32_PTR) trgLine)[14] = (src[ 8] << 8) | (src[ 9] << 16) | (src[10] << 24) | src[11];
            ((gctUINT32_PTR) trgLine)[15] = (src[12] << 8) | (src[13] << 16) | (src[14] << 24) | src[15];

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 4;
        }
    }
}

static void
_UploadL8toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | (srcLine[1] << 16) | (srcLine[1] << 8) | srcLine[1];
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2];
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[3] << 8) | srcLine[3];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* L8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (src[0] << 16) | (src[0] << 8) | src[0];
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | (src[1] << 16) | (src[1] << 8) | src[1];
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | (src[2] << 16) | (src[2] << 8) | src[2];
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | (src[3] << 16) | (src[3] << 8) | src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF000000 | (src[0] << 16) | (src[0] << 8) | src[0];
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF000000 | (src[1] << 16) | (src[1] << 8) | src[1];
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF000000 | (src[2] << 16) | (src[2] << 8) | src[2];
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF000000 | (src[3] << 16) | (src[3] << 8) | src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF000000 | (src[0] << 16) | (src[0] << 8) | src[0];
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF000000 | (src[1] << 16) | (src[1] << 8) | src[1];
            ((gctUINT32_PTR) trgLine)[10] = 0xFF000000 | (src[2] << 16) | (src[2] << 8) | src[2];
            ((gctUINT32_PTR) trgLine)[11] = 0xFF000000 | (src[3] << 16) | (src[3] << 8) | src[3];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = 0xFF000000 | (src[0] << 16) | (src[0] << 8) | src[0];
            ((gctUINT32_PTR) trgLine)[13] = 0xFF000000 | (src[1] << 16) | (src[1] << 8) | src[1];
            ((gctUINT32_PTR) trgLine)[14] = 0xFF000000 | (src[2] << 16) | (src[2] << 8) | src[2];
            ((gctUINT32_PTR) trgLine)[15] = 0xFF000000 | (src[3] << 16) | (src[3] << 8) | src[3];

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1;
        }
    }
}

static void
_UploadL8toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF | (srcLine[1] << 8) | (srcLine[1] << 16) | (srcLine[1] << 24);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF | (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF | (srcLine[3] << 8) | (srcLine[3] << 16) | (srcLine[3] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* L8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF | (src[0] << 8) | (src[0] << 16) | (src[0] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF | (src[1] << 8) | (src[1] << 16) | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF | (src[2] << 8) | (src[2] << 16) | (src[2] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF | (src[3] << 8) | (src[3] << 16) | (src[3] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF | (src[0] << 8) | (src[0] << 16) | (src[0] << 24);
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF | (src[1] << 8) | (src[1] << 16) | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF | (src[2] << 8) | (src[2] << 16) | (src[2] << 24);
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF | (src[3] << 8) | (src[3] << 16) | (src[3] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF | (src[0] << 8) | (src[0] << 16) | (src[0] << 24);
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF | (src[1] << 8) | (src[1] << 16) | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[10] = 0xFF | (src[2] << 8) | (src[2] << 16) | (src[2] << 24);
            ((gctUINT32_PTR) trgLine)[11] = 0xFF | (src[3] << 8) | (src[3] << 16) | (src[3] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = 0xFF | (src[0] << 8) | (src[0] << 16) | (src[0] << 24);
            ((gctUINT32_PTR) trgLine)[13] = 0xFF | (src[1] << 8) | (src[1] << 16) | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[14] = 0xFF | (src[2] << 8) | (src[2] << 16) | (src[2] << 24);
            ((gctUINT32_PTR) trgLine)[15] = 0xFF | (src[3] << 8) | (src[3] << 16) | (src[3] << 24);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1;
        }
    }
}

static void
_UploadA8L8toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2] | (srcLine[3] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[4] << 16) | (srcLine[4] << 8) | srcLine[4] | (srcLine[5] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[6] << 16) | (srcLine[6] << 8) | srcLine[6] | (srcLine[7] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 2;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* A8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[0] << 16) | (src[0] << 8) | src[0] | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (src[2] << 16) | (src[2] << 8) | src[2] | (src[3] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (src[4] << 16) | (src[4] << 8) | src[4] | (src[5] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (src[6] << 16) | (src[6] << 8) | src[6] | (src[7] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[0] << 16) | (src[0] << 8) | src[0] | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 5] = (src[2] << 16) | (src[2] << 8) | src[2] | (src[3] << 24);
            ((gctUINT32_PTR) trgLine)[ 6] = (src[4] << 16) | (src[4] << 8) | src[4] | (src[5] << 24);
            ((gctUINT32_PTR) trgLine)[ 7] = (src[6] << 16) | (src[6] << 8) | src[6] | (src[7] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[0] << 16) | (src[0] << 8) | src[0] | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 9] = (src[2] << 16) | (src[2] << 8) | src[2] | (src[3] << 24);
            ((gctUINT32_PTR) trgLine)[10] = (src[4] << 16) | (src[4] << 8) | src[4] | (src[5] << 24);
            ((gctUINT32_PTR) trgLine)[11] = (src[6] << 16) | (src[6] << 8) | src[6] | (src[7] << 24);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[0] << 16) | (src[0] << 8) | src[0] | (src[1] << 24);
            ((gctUINT32_PTR) trgLine)[13] = (src[2] << 16) | (src[2] << 8) | src[2] | (src[3] << 24);
            ((gctUINT32_PTR) trgLine)[14] = (src[4] << 16) | (src[4] << 8) | src[4] | (src[5] << 24);
            ((gctUINT32_PTR) trgLine)[15] = (src[6] << 16) | (src[6] << 8) | src[6] | (src[7] << 24);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 2;
        }
    }
}

static void
_UploadA8L8toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24) | srcLine[3];
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[4] << 8) | (srcLine[4] << 16) | (srcLine[4] << 24) | srcLine[5];
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[6] << 8) | (srcLine[6] << 16) | (srcLine[6] << 24) | srcLine[7];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 2;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* A8 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (src[0] << 8) | (src[0] << 16) | (src[0] << 24) | src[1];
            ((gctUINT32_PTR) trgLine)[ 1] = (src[2] << 8) | (src[2] << 16) | (src[2] << 24) | src[3];
            ((gctUINT32_PTR) trgLine)[ 2] = (src[4] << 8) | (src[4] << 16) | (src[4] << 24) | src[5];
            ((gctUINT32_PTR) trgLine)[ 3] = (src[6] << 8) | (src[6] << 16) | (src[6] << 24) | src[7];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = (src[0] << 8) | (src[0] << 16) | (src[0] << 24) | src[1];
            ((gctUINT32_PTR) trgLine)[ 5] = (src[2] << 8) | (src[2] << 16) | (src[2] << 24) | src[3];
            ((gctUINT32_PTR) trgLine)[ 6] = (src[4] << 8) | (src[4] << 16) | (src[4] << 24) | src[5];
            ((gctUINT32_PTR) trgLine)[ 7] = (src[6] << 8) | (src[6] << 16) | (src[6] << 24) | src[7];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = (src[0] << 8) | (src[0] << 16) | (src[0] << 24) | src[1];
            ((gctUINT32_PTR) trgLine)[ 9] = (src[2] << 8) | (src[2] << 16) | (src[2] << 24) | src[3];
            ((gctUINT32_PTR) trgLine)[10] = (src[4] << 8) | (src[4] << 16) | (src[4] << 24) | src[5];
            ((gctUINT32_PTR) trgLine)[11] = (src[6] << 8) | (src[6] << 16) | (src[6] << 24) | src[7];

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = (src[0] << 8) | (src[0] << 16) | (src[0] << 24) | src[1];
            ((gctUINT32_PTR) trgLine)[13] = (src[2] << 8) | (src[2] << 16) | (src[2] << 24) | src[3];
            ((gctUINT32_PTR) trgLine)[14] = (src[4] << 8) | (src[4] << 16) | (src[4] << 24) | src[5];
            ((gctUINT32_PTR) trgLine)[15] = (src[6] << 8) | (src[6] << 16) | (src[6] << 24) | src[7];

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 2;
        }
    }
}

static void
_Upload32bppto32bpp(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* 32bpp to 32bpp. */
            ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16) | (srcLine[3] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: one tile row. */
                if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
                    ((gctUINT32_PTR) trgLine)[1] = srcLine[ 4] | (srcLine[ 5] << 8) | (srcLine[ 6] << 16) | (srcLine[ 7] << 24);
                    ((gctUINT32_PTR) trgLine)[2] = srcLine[ 8] | (srcLine[ 9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
                    ((gctUINT32_PTR) trgLine)[3] = srcLine[12] | (srcLine[13] << 8) | (srcLine[14] << 16) | (srcLine[15] << 24);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 4;

        if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 32 bpp to 32 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 4] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 5] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 6] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 7] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 9] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[10] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[11] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[12] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[13] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[14] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[15] = ((gctUINT32_PTR) src)[3];

                /* Move to next tile. */
                trgLine += 16 * 4;
                srcLine +=  4 * 4;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 32 bpp to 32 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 1] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[ 2] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[ 3] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 4] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 5] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[ 6] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[ 7] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 9] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[10] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[11] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[12] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[13] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[14] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[15] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);

                /* Move to next tile. */
                trgLine += 16 * 4;
                srcLine +=  4 * 4;
            }
        }
    }
}

static void
_Upload32bppto32bppBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* 32bpp to 32bpp. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
                    ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 24) | (srcLine[ 5] << 16) | (srcLine[ 6] << 8) | srcLine[ 7];
                    ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 24) | (srcLine[ 9] << 16) | (srcLine[10] << 8) | srcLine[11];
                    ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 24) | (srcLine[13] << 16) | (srcLine[14] << 8) | srcLine[15];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 4;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 32 bpp to 32 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 4] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 5] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 6] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 7] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 9] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[10] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[11] = ((gctUINT32_PTR) src)[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[12] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[13] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[14] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[15] = ((gctUINT32_PTR) src)[3];

                /* Move to next tile. */
                trgLine += 16 * 4;
                srcLine +=  4 * 4;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 32 bpp to 32 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[ 1] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[ 2] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[ 3] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 4] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[ 5] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[ 6] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[ 7] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[ 9] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[10] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[11] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[12] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[13] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[14] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[15] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];

                /* Move to next tile. */
                trgLine += 16 * 4;
                srcLine +=  4 * 4;
            }
        }
    }
}

static void
_Upload64bppto64bpp(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 8);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

            /* 64bpp to 64bpp. */
            ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16) | (srcLine[3] << 24);
            ((gctUINT32_PTR) trgLine)[1] = srcLine[4] | (srcLine[5] << 8) | (srcLine[6] << 16) | (srcLine[7] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

                /* 64 bpp to 64 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                    ((gctUINT32_PTR) trgLine)[4] = ((gctUINT32_PTR) srcLine)[4];
                    ((gctUINT32_PTR) trgLine)[5] = ((gctUINT32_PTR) srcLine)[5];
                    ((gctUINT32_PTR) trgLine)[6] = ((gctUINT32_PTR) srcLine)[6];
                    ((gctUINT32_PTR) trgLine)[7] = ((gctUINT32_PTR) srcLine)[7];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
                    ((gctUINT32_PTR) trgLine)[1] = srcLine[ 4] | (srcLine[ 5] << 8) | (srcLine[ 6] << 16) | (srcLine[ 7] << 24);
                    ((gctUINT32_PTR) trgLine)[2] = srcLine[ 8] | (srcLine[ 9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
                    ((gctUINT32_PTR) trgLine)[3] = srcLine[12] | (srcLine[13] << 8) | (srcLine[14] << 16) | (srcLine[15] << 24);
                    ((gctUINT32_PTR) trgLine)[4] = srcLine[16] | (srcLine[17] << 8) | (srcLine[18] << 16) | (srcLine[19] << 24);
                    ((gctUINT32_PTR) trgLine)[5] = srcLine[20] | (srcLine[21] << 8) | (srcLine[22] << 16) | (srcLine[23] << 24);
                    ((gctUINT32_PTR) trgLine)[6] = srcLine[24] | (srcLine[25] << 8) | (srcLine[26] << 16) | (srcLine[27] << 24);
                    ((gctUINT32_PTR) trgLine)[7] = srcLine[28] | (srcLine[29] << 8) | (srcLine[30] << 16) | (srcLine[31] << 24);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

                /* 64 bpp to 64 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[1] = srcLine[ 4] | (srcLine[ 5] << 8) | (srcLine[ 6] << 16) | (srcLine[ 7] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 8;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 64 bpp to 64 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[ 4] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[ 5] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[ 6] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[ 7] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 9] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[10] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[11] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[12] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[13] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[14] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[15] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[16] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[17] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[18] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[19] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[20] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[21] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[22] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[23] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[24] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[25] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[26] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[27] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[28] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[29] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[30] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[31] = ((gctUINT32_PTR) src)[7];

                /* Move to next tile. */
                trgLine += 16 * 8;
                srcLine +=  4 * 8;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 64 bpp to 64 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 1] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[ 2] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[ 3] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);
                ((gctUINT32_PTR) trgLine)[ 4] = src[16] | (src[17] << 8) | (src[18] << 16) | (src[19] << 24);
                ((gctUINT32_PTR) trgLine)[ 5] = src[20] | (src[21] << 8) | (src[22] << 16) | (src[23] << 24);
                ((gctUINT32_PTR) trgLine)[ 6] = src[24] | (src[25] << 8) | (src[26] << 16) | (src[27] << 24);
                ((gctUINT32_PTR) trgLine)[ 7] = src[28] | (src[29] << 8) | (src[30] << 16) | (src[31] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 9] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[10] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[11] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);
                ((gctUINT32_PTR) trgLine)[12] = src[16] | (src[17] << 8) | (src[18] << 16) | (src[19] << 24);
                ((gctUINT32_PTR) trgLine)[13] = src[20] | (src[21] << 8) | (src[22] << 16) | (src[23] << 24);
                ((gctUINT32_PTR) trgLine)[14] = src[24] | (src[25] << 8) | (src[26] << 16) | (src[27] << 24);
                ((gctUINT32_PTR) trgLine)[15] = src[28] | (src[29] << 8) | (src[30] << 16) | (src[31] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[16] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[17] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[18] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[19] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);
                ((gctUINT32_PTR) trgLine)[20] = src[16] | (src[17] << 8) | (src[18] << 16) | (src[19] << 24);
                ((gctUINT32_PTR) trgLine)[21] = src[20] | (src[21] << 8) | (src[22] << 16) | (src[23] << 24);
                ((gctUINT32_PTR) trgLine)[22] = src[24] | (src[25] << 8) | (src[26] << 16) | (src[27] << 24);
                ((gctUINT32_PTR) trgLine)[23] = src[28] | (src[29] << 8) | (src[30] << 16) | (src[31] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[24] = src[ 0] | (src[ 1] << 8) | (src[ 2] << 16) | (src[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[25] = src[ 4] | (src[ 5] << 8) | (src[ 6] << 16) | (src[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[26] = src[ 8] | (src[ 9] << 8) | (src[10] << 16) | (src[11] << 24);
                ((gctUINT32_PTR) trgLine)[27] = src[12] | (src[13] << 8) | (src[14] << 16) | (src[15] << 24);
                ((gctUINT32_PTR) trgLine)[28] = src[16] | (src[17] << 8) | (src[18] << 16) | (src[19] << 24);
                ((gctUINT32_PTR) trgLine)[29] = src[20] | (src[21] << 8) | (src[22] << 16) | (src[23] << 24);
                ((gctUINT32_PTR) trgLine)[30] = src[24] | (src[25] << 8) | (src[26] << 16) | (src[27] << 24);
                ((gctUINT32_PTR) trgLine)[31] = src[28] | (src[29] << 8) | (src[30] << 16) | (src[31] << 24);

                /* Move to next tile. */
                trgLine += 16 * 8;
                srcLine +=  4 * 8;
            }
        }
    }
}

static void
_Upload64bppto64bppBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 8);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

            /* 64bpp to 64bpp. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
            ((gctUINT32_PTR) trgLine)[1] = (srcLine[4] << 24) | (srcLine[5] << 16) | (srcLine[6] << 8) | srcLine[7];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

                /* 64 bpp to 64 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                    ((gctUINT32_PTR) trgLine)[4] = ((gctUINT32_PTR) srcLine)[4];
                    ((gctUINT32_PTR) trgLine)[5] = ((gctUINT32_PTR) srcLine)[5];
                    ((gctUINT32_PTR) trgLine)[6] = ((gctUINT32_PTR) srcLine)[6];
                    ((gctUINT32_PTR) trgLine)[7] = ((gctUINT32_PTR) srcLine)[7];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
                    ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 24) | (srcLine[ 5] << 16) | (srcLine[ 6] << 8) | srcLine[ 7];
                    ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 24) | (srcLine[ 9] << 16) | (srcLine[10] << 8) | srcLine[11];
                    ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 24) | (srcLine[13] << 16) | (srcLine[14] << 8) | srcLine[15];
                    ((gctUINT32_PTR) trgLine)[4] = (srcLine[16] << 24) | (srcLine[17] << 16) | (srcLine[18] << 8) | srcLine[19];
                    ((gctUINT32_PTR) trgLine)[5] = (srcLine[20] << 24) | (srcLine[21] << 16) | (srcLine[22] << 8) | srcLine[23];
                    ((gctUINT32_PTR) trgLine)[6] = (srcLine[24] << 24) | (srcLine[25] << 16) | (srcLine[26] << 8) | srcLine[27];
                    ((gctUINT32_PTR) trgLine)[7] = (srcLine[28] << 24) | (srcLine[29] << 16) | (srcLine[30] << 8) | srcLine[31];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 8;

                /* 64 bpp to 64 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 24) | (srcLine[ 5] << 16) | (srcLine[ 6] << 8) | srcLine[ 7];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 8;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 8;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 64 bpp to 64 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[ 4] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[ 5] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[ 6] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[ 7] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[ 9] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[10] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[11] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[12] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[13] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[14] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[15] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[16] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[17] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[18] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[19] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[20] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[21] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[22] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[23] = ((gctUINT32_PTR) src)[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[24] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[25] = ((gctUINT32_PTR) src)[1];
                ((gctUINT32_PTR) trgLine)[26] = ((gctUINT32_PTR) src)[2];
                ((gctUINT32_PTR) trgLine)[27] = ((gctUINT32_PTR) src)[3];
                ((gctUINT32_PTR) trgLine)[28] = ((gctUINT32_PTR) src)[4];
                ((gctUINT32_PTR) trgLine)[29] = ((gctUINT32_PTR) src)[5];
                ((gctUINT32_PTR) trgLine)[30] = ((gctUINT32_PTR) src)[6];
                ((gctUINT32_PTR) trgLine)[31] = ((gctUINT32_PTR) src)[7];

                /* Move to next tile. */
                trgLine += 16 * 8;
                srcLine +=  4 * 8;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 64 bpp to 64 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[1] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[2] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[3] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];
                ((gctUINT32_PTR) trgLine)[4] = (src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19];
                ((gctUINT32_PTR) trgLine)[5] = (src[20] << 24) | (src[21] << 16) | (src[22] << 8) | src[23];
                ((gctUINT32_PTR) trgLine)[6] = (src[24] << 24) | (src[25] << 16) | (src[26] << 8) | src[27];
                ((gctUINT32_PTR) trgLine)[7] = (src[28] << 24) | (src[29] << 16) | (src[30] << 8) | src[31];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[ 8] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[ 9] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[10] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[11] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];
                ((gctUINT32_PTR) trgLine)[12] = (src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19];
                ((gctUINT32_PTR) trgLine)[13] = (src[20] << 24) | (src[21] << 16) | (src[22] << 8) | src[23];
                ((gctUINT32_PTR) trgLine)[14] = (src[24] << 24) | (src[25] << 16) | (src[26] << 8) | src[27];
                ((gctUINT32_PTR) trgLine)[15] = (src[28] << 24) | (src[29] << 16) | (src[30] << 8) | src[31];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[16] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[17] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[18] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[19] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];
                ((gctUINT32_PTR) trgLine)[20] = (src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19];
                ((gctUINT32_PTR) trgLine)[21] = (src[20] << 24) | (src[21] << 16) | (src[22] << 8) | src[23];
                ((gctUINT32_PTR) trgLine)[22] = (src[24] << 24) | (src[25] << 16) | (src[26] << 8) | src[27];
                ((gctUINT32_PTR) trgLine)[23] = (src[28] << 24) | (src[29] << 16) | (src[30] << 8) | src[31];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[24] = (src[ 0] << 24) | (src[ 1] << 16) | (src[ 2] << 8) | src[ 3];
                ((gctUINT32_PTR) trgLine)[25] = (src[ 4] << 24) | (src[ 5] << 16) | (src[ 6] << 8) | src[ 7];
                ((gctUINT32_PTR) trgLine)[26] = (src[ 8] << 24) | (src[ 9] << 16) | (src[10] << 8) | src[11];
                ((gctUINT32_PTR) trgLine)[27] = (src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15];
                ((gctUINT32_PTR) trgLine)[28] = (src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19];
                ((gctUINT32_PTR) trgLine)[29] = (src[20] << 24) | (src[21] << 16) | (src[22] << 8) | src[23];
                ((gctUINT32_PTR) trgLine)[30] = (src[24] << 24) | (src[25] << 16) | (src[26] << 8) | src[27];
                ((gctUINT32_PTR) trgLine)[31] = (src[28] << 24) | (src[29] << 16) | (src[30] << 8) | src[31];

                /* Move to next tile. */
                trgLine += 16 * 8;
                srcLine +=  4 * 8;
            }
        }
    }
}

static void
_UploadRGB565toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGB565 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x1F) << 3);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x1F) << 3);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | ((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0x7E0) << 5) | ((srcLine[1] & 0x1F) << 3);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | ((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0x7E0) << 5) | ((srcLine[2] & 0x1F) << 3);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | ((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0x7E0) << 5) | ((srcLine[3] & 0x1F) << 3);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x1F) << 3);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGB565 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7E0) << 5) | ((src[0] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7E0) << 5) | ((src[1] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7E0) << 5) | ((src[2] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7E0) << 5) | ((src[3] & 0x1F) << 3);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF000000 | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7E0) << 5) | ((src[0] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF000000 | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7E0) << 5) | ((src[1] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF000000 | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7E0) << 5) | ((src[2] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF000000 | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7E0) << 5) | ((src[3] & 0x1F) << 3);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF000000 | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7E0) << 5) | ((src[0] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF000000 | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7E0) << 5) | ((src[1] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[10] = 0xFF000000 | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7E0) << 5) | ((src[2] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[11] = 0xFF000000 | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7E0) << 5) | ((src[3] & 0x1F) << 3);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[12] = 0xFF000000 | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7E0) << 5) | ((src[0] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[13] = 0xFF000000 | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7E0) << 5) | ((src[1] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[14] = 0xFF000000 | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7E0) << 5) | ((src[2] & 0x1F) << 3);
            ((gctUINT32_PTR) trgLine)[15] = 0xFF000000 | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7E0) << 5) | ((src[3] & 0x1F) << 3);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGB565toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* RGB565 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* RGB565 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF | ((srcLine[2] & 0x1F) << 27) | ((srcLine[2] & 0xE0) << 5) | ((srcLine[3] & 0x07) << 13) | ((srcLine[3] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF | ((srcLine[4] & 0x1F) << 27) | ((srcLine[4] & 0xE0) << 5) | ((srcLine[5] & 0x07) << 13) | ((srcLine[5] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF | ((srcLine[6] & 0x1F) << 27) | ((srcLine[6] & 0xE0) << 5) | ((srcLine[7] & 0x07) << 13) | ((srcLine[7] & 0xF8) << 8);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* RGB565 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X *  2;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* RGB565 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF | ((src[0] & 0x1F) << 27) | ((src[0] & 0xE0) << 5) | ((src[1] & 0x07) << 13) | ((src[1] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF | ((src[2] & 0x1F) << 27) | ((src[2] & 0xE0) << 5) | ((src[3] & 0x07) << 13) | ((src[3] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF | ((src[4] & 0x1F) << 27) | ((src[4] & 0xE0) << 5) | ((src[5] & 0x07) << 13) | ((src[5] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF | ((src[6] & 0x1F) << 27) | ((src[6] & 0xE0) << 5) | ((src[7] & 0x07) << 13) | ((src[7] & 0xF8) << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = 0xFF | ((src[0] & 0x1F) << 27) | ((src[0] & 0xE0) << 5) | ((src[1] & 0x07) << 13) | ((src[1] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 5] = 0xFF | ((src[2] & 0x1F) << 27) | ((src[2] & 0xE0) << 5) | ((src[3] & 0x07) << 13) | ((src[3] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 6] = 0xFF | ((src[4] & 0x1F) << 27) | ((src[4] & 0xE0) << 5) | ((src[5] & 0x07) << 13) | ((src[5] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 7] = 0xFF | ((src[6] & 0x1F) << 27) | ((src[6] & 0xE0) << 5) | ((src[7] & 0x07) << 13) | ((src[7] & 0xF8) << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = 0xFF | ((src[0] & 0x1F) << 27) | ((src[0] & 0xE0) << 5) | ((src[1] & 0x07) << 13) | ((src[1] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[ 9] = 0xFF | ((src[2] & 0x1F) << 27) | ((src[2] & 0xE0) << 5) | ((src[3] & 0x07) << 13) | ((src[3] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[10] = 0xFF | ((src[4] & 0x1F) << 27) | ((src[4] & 0xE0) << 5) | ((src[5] & 0x07) << 13) | ((src[5] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[11] = 0xFF | ((src[6] & 0x1F) << 27) | ((src[6] & 0xE0) << 5) | ((src[7] & 0x07) << 13) | ((src[7] & 0xF8) << 8);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = 0xFF | ((src[0] & 0x1F) << 27) | ((src[0] & 0xE0) << 5) | ((src[1] & 0x07) << 13) | ((src[1] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[13] = 0xFF | ((src[2] & 0x1F) << 27) | ((src[2] & 0xE0) << 5) | ((src[3] & 0x07) << 13) | ((src[3] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[14] = 0xFF | ((src[4] & 0x1F) << 27) | ((src[4] & 0xE0) << 5) | ((src[5] & 0x07) << 13) | ((src[5] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[15] = 0xFF | ((src[6] & 0x1F) << 27) | ((src[6] & 0xE0) << 5) | ((src[7] & 0x07) << 13) | ((src[7] & 0xF8) << 8);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 2;
        }
    }
}

static void
_UploadRGBA4444toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[1] & 0xF) << 28) | ((srcLine[1] & 0xF000) << 8) | ((srcLine[1] & 0xF00) << 4) | (srcLine[1] & 0xF0);
                ((gctUINT32_PTR) trgLine)[2] = ((srcLine[2] & 0xF) << 28) | ((srcLine[2] & 0xF000) << 8) | ((srcLine[2] & 0xF00) << 4) | (srcLine[2] & 0xF0);
                ((gctUINT32_PTR) trgLine)[3] = ((srcLine[3] & 0xF) << 28) | ((srcLine[3] & 0xF000) << 8) | ((srcLine[3] & 0xF00) << 4) | (srcLine[3] & 0xF0);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA4444 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = ((src[0] & 0xF) << 28) | ((src[0] & 0xF000) << 8) | ((src[0] & 0xF00) << 4) | (src[0] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 1] = ((src[1] & 0xF) << 28) | ((src[1] & 0xF000) << 8) | ((src[1] & 0xF00) << 4) | (src[1] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 2] = ((src[2] & 0xF) << 28) | ((src[2] & 0xF000) << 8) | ((src[2] & 0xF00) << 4) | (src[2] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 3] = ((src[3] & 0xF) << 28) | ((src[3] & 0xF000) << 8) | ((src[3] & 0xF00) << 4) | (src[3] & 0xF0);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 4] = ((src[0] & 0xF) << 28) | ((src[0] & 0xF000) << 8) | ((src[0] & 0xF00) << 4) | (src[0] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 5] = ((src[1] & 0xF) << 28) | ((src[1] & 0xF000) << 8) | ((src[1] & 0xF00) << 4) | (src[1] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 6] = ((src[2] & 0xF) << 28) | ((src[2] & 0xF000) << 8) | ((src[2] & 0xF00) << 4) | (src[2] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 7] = ((src[3] & 0xF) << 28) | ((src[3] & 0xF000) << 8) | ((src[3] & 0xF00) << 4) | (src[3] & 0xF0);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 8] = ((src[0] & 0xF) << 28) | ((src[0] & 0xF000) << 8) | ((src[0] & 0xF00) << 4) | (src[0] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 9] = ((src[1] & 0xF) << 28) | ((src[1] & 0xF000) << 8) | ((src[1] & 0xF00) << 4) | (src[1] & 0xF0);
            ((gctUINT32_PTR) trgLine)[10] = ((src[2] & 0xF) << 28) | ((src[2] & 0xF000) << 8) | ((src[2] & 0xF00) << 4) | (src[2] & 0xF0);
            ((gctUINT32_PTR) trgLine)[11] = ((src[3] & 0xF) << 28) | ((src[3] & 0xF000) << 8) | ((src[3] & 0xF00) << 4) | (src[3] & 0xF0);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[12] = ((src[0] & 0xF) << 28) | ((src[0] & 0xF000) << 8) | ((src[0] & 0xF00) << 4) | (src[0] & 0xF0);
            ((gctUINT32_PTR) trgLine)[13] = ((src[1] & 0xF) << 28) | ((src[1] & 0xF000) << 8) | ((src[1] & 0xF00) << 4) | (src[1] & 0xF0);
            ((gctUINT32_PTR) trgLine)[14] = ((src[2] & 0xF) << 28) | ((src[2] & 0xF000) << 8) | ((src[2] & 0xF00) << 4) | (src[2] & 0xF0);
            ((gctUINT32_PTR) trgLine)[15] = ((src[3] & 0xF) << 28) | ((src[3] & 0xF000) << 8) | ((src[3] & 0xF00) << 4) | (src[3] & 0xF0);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA4444toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[1] & 0xF) << 20) | (srcLine[1] & 0xF0) | ((srcLine[1] & 0xF00) >> 4) | ((srcLine[1] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[2] = ((srcLine[2] & 0xF) << 20) | (srcLine[2] & 0xF0) | ((srcLine[2] & 0xF00) >> 4) | ((srcLine[2] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[3] = ((srcLine[3] & 0xF) << 20) | (srcLine[3] & 0xF0) | ((srcLine[3] & 0xF00) >> 4) | ((srcLine[3] & 0xF000) << 16);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA4444 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = ((src[0] & 0xF) << 20) | (src[0] & 0xF0) | ((src[0] & 0xF00) >> 4) | ((src[0] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 1] = ((src[1] & 0xF) << 20) | (src[1] & 0xF0) | ((src[1] & 0xF00) >> 4) | ((src[1] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 2] = ((src[2] & 0xF) << 20) | (src[2] & 0xF0) | ((src[2] & 0xF00) >> 4) | ((src[2] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 3] = ((src[3] & 0xF) << 20) | (src[3] & 0xF0) | ((src[3] & 0xF00) >> 4) | ((src[3] & 0xF000) << 16);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 4] = ((src[0] & 0xF) << 20) | (src[0] & 0xF0) | ((src[0] & 0xF00) >> 4) | ((src[0] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 5] = ((src[1] & 0xF) << 20) | (src[1] & 0xF0) | ((src[1] & 0xF00) >> 4) | ((src[1] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 6] = ((src[2] & 0xF) << 20) | (src[2] & 0xF0) | ((src[2] & 0xF00) >> 4) | ((src[2] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 7] = ((src[3] & 0xF) << 20) | (src[3] & 0xF0) | ((src[3] & 0xF00) >> 4) | ((src[3] & 0xF000) << 16);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 8] = ((src[0] & 0xF) << 20) | (src[0] & 0xF0) | ((src[0] & 0xF00) >> 4) | ((src[0] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 9] = ((src[1] & 0xF) << 20) | (src[1] & 0xF0) | ((src[1] & 0xF00) >> 4) | ((src[1] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[10] = ((src[2] & 0xF) << 20) | (src[2] & 0xF0) | ((src[2] & 0xF00) >> 4) | ((src[2] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[11] = ((src[3] & 0xF) << 20) | (src[3] & 0xF0) | ((src[3] & 0xF00) >> 4) | ((src[3] & 0xF000) << 16);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[12] = ((src[0] & 0xF) << 20) | (src[0] & 0xF0) | ((src[0] & 0xF00) >> 4) | ((src[0] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[13] = ((src[1] & 0xF) << 20) | (src[1] & 0xF0) | ((src[1] & 0xF00) >> 4) | ((src[1] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[14] = ((src[2] & 0xF) << 20) | (src[2] & 0xF0) | ((src[2] & 0xF00) >> 4) | ((src[2] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[15] = ((src[3] & 0xF) << 20) | (src[3] & 0xF0) | ((src[3] & 0xF00) >> 4) | ((src[3] & 0xF000) << 16);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA5551toARGB(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0x7C0) << 5) | ((srcLine[1] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0x7C0) << 5) | ((srcLine[2] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0x7C0) << 5) | ((srcLine[3] & 0x3E) << 2);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA5551 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7C0) << 5) | ((src[0] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7C0) << 5) | ((src[1] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7C0) << 5) | ((src[2] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7C0) << 5) | ((src[3] & 0x3E) << 2);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 4] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7C0) << 5) | ((src[0] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 5] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7C0) << 5) | ((src[1] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 6] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7C0) << 5) | ((src[2] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 7] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7C0) << 5) | ((src[3] & 0x3E) << 2);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[ 8] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7C0) << 5) | ((src[0] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 9] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7C0) << 5) | ((src[1] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[10] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7C0) << 5) | ((src[2] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[11] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7C0) << 5) | ((src[3] & 0x3E) << 2);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[12] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((src[0] & 0xF800) << 8) | ((src[0] & 0x7C0) << 5) | ((src[0] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[13] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((src[1] & 0xF800) << 8) | ((src[1] & 0x7C0) << 5) | ((src[1] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[14] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((src[2] & 0xF800) << 8) | ((src[2] & 0x7C0) << 5) | ((src[2] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[15] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((src[3] & 0xF800) << 8) | ((src[3] & 0x7C0) << 5) | ((src[3] & 0x3E) << 2);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA5551toARGBBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* RGBA5551 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* RGBA5551 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0x1) << 7) | ((srcLine[3] & 0xF8) << 8) | ((srcLine[2] & 0xC0) << 13) | ((srcLine[3] & 0x07) << 21) | ((srcLine[2] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[2] = ((srcLine[4] & 0x1) << 7) | ((srcLine[5] & 0xF8) << 8) | ((srcLine[4] & 0xC0) << 13) | ((srcLine[5] & 0x07) << 21) | ((srcLine[4] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[3] = ((srcLine[6] & 0x1) << 7) | ((srcLine[7] & 0xF8) << 8) | ((srcLine[6] & 0xC0) << 13) | ((srcLine[7] & 0x07) << 21) | ((srcLine[6] & 0x3E) << 26);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* RGBA5551 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 2;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR src;

            /* RGBA5551 to ARGB: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[ 0] = ((src[0] & 0x1) << 7) | ((src[1] & 0xF8) << 8) | ((src[0] & 0xC0) << 13) | ((src[1] & 0x07) << 21) | ((src[0] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 1] = ((src[2] & 0x1) << 7) | ((src[3] & 0xF8) << 8) | ((src[2] & 0xC0) << 13) | ((src[3] & 0x07) << 21) | ((src[2] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 2] = ((src[4] & 0x1) << 7) | ((src[5] & 0xF8) << 8) | ((src[4] & 0xC0) << 13) | ((src[5] & 0x07) << 21) | ((src[4] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 3] = ((src[6] & 0x1) << 7) | ((src[7] & 0xF8) << 8) | ((src[6] & 0xC0) << 13) | ((src[7] & 0x07) << 21) | ((src[6] & 0x3E) << 26);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 4] = ((src[0] & 0x1) << 7) | ((src[1] & 0xF8) << 8) | ((src[0] & 0xC0) << 13) | ((src[1] & 0x07) << 21) | ((src[0] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 5] = ((src[2] & 0x1) << 7) | ((src[3] & 0xF8) << 8) | ((src[2] & 0xC0) << 13) | ((src[3] & 0x07) << 21) | ((src[2] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 6] = ((src[4] & 0x1) << 7) | ((src[5] & 0xF8) << 8) | ((src[4] & 0xC0) << 13) | ((src[5] & 0x07) << 21) | ((src[4] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 7] = ((src[6] & 0x1) << 7) | ((src[7] & 0xF8) << 8) | ((src[6] & 0xC0) << 13) | ((src[7] & 0x07) << 21) | ((src[6] & 0x3E) << 26);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[ 8] = ((src[0] & 0x1) << 7) | ((src[1] & 0xF8) << 8) | ((src[0] & 0xC0) << 13) | ((src[1] & 0x07) << 21) | ((src[0] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[ 9] = ((src[2] & 0x1) << 7) | ((src[3] & 0xF8) << 8) | ((src[2] & 0xC0) << 13) | ((src[3] & 0x07) << 21) | ((src[2] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[10] = ((src[4] & 0x1) << 7) | ((src[5] & 0xF8) << 8) | ((src[4] & 0xC0) << 13) | ((src[5] & 0x07) << 21) | ((src[4] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[11] = ((src[6] & 0x1) << 7) | ((src[7] & 0xF8) << 8) | ((src[6] & 0xC0) << 13) | ((src[7] & 0x07) << 21) | ((src[6] & 0x3E) << 26);

            src += SourceStride;
            ((gctUINT32_PTR) trgLine)[12] = ((src[0] & 0x1) << 7) | ((src[1] & 0xF8) << 8) | ((src[0] & 0xC0) << 13) | ((src[1] & 0x07) << 21) | ((src[0] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[13] = ((src[2] & 0x1) << 7) | ((src[3] & 0xF8) << 8) | ((src[2] & 0xC0) << 13) | ((src[3] & 0x07) << 21) | ((src[2] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[14] = ((src[4] & 0x1) << 7) | ((src[5] & 0xF8) << 8) | ((src[4] & 0xC0) << 13) | ((src[5] & 0x07) << 21) | ((src[4] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[15] = ((src[6] & 0x1) << 7) | ((src[7] & 0xF8) << 8) | ((src[6] & 0xC0) << 13) | ((src[7] & 0x07) << 21) | ((src[6] & 0x3E) << 26);

            /* Move to next tile. */
            trgLine += 16 * 4;
            srcLine +=  4 * 2;
        }
    }
}

static void
_Upload8bppto8bpp(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* 8 bpp to 8 bpp. */
            trgLine[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR)srcLine)[0];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16) | (srcLine[3] << 24);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: part tile row. */
                trgLine[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) (srcLine + SourceStride * 0))[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) (srcLine + SourceStride * 1))[0];
                ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) (srcLine + SourceStride * 2))[0];
                ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) (srcLine + SourceStride * 3))[0];

                /* Move to next tile. */
                trgLine += 16 * 1;
                srcLine +=  4 * 1;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 8 bpp to 8 bpp: one tile. */
                src  = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[1] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[2] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[3] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                /* Move to next tile. */
                trgLine += 16 * 1;
                srcLine +=  4 * 1;
            }
        }
    }
}

static void
_Upload8bppto8bppBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* 8 bpp to 8 bpp. */
            trgLine[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR)srcLine)[0];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: part tile row. */
                trgLine[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 1;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned source. */
            for (x = X; x < Right; x += 4)
            {
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) (srcLine + SourceStride * 0))[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) (srcLine + SourceStride * 1))[0];
                ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) (srcLine + SourceStride * 2))[0];
                ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) (srcLine + SourceStride * 3))[0];

                /* Move to next tile. */
                trgLine += 16 * 1;
                srcLine +=  4 * 1;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 8 bpp to 8 bpp: one tile. */
                src  = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[1] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[2] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[3] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

                /* Move to next tile. */
                trgLine += 16 * 1;
                srcLine +=  4 * 1;
            }
        }
    }
}

static void
_Upload16bppto16bpp(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* 16 bpp to 16 bpp. */
            ((gctUINT16_PTR) trgLine)[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned case. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 16);
                    ((gctUINT32_PTR) trgLine)[1] = srcLine[2] | (srcLine[3] << 16);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned case. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT16_PTR src;

                /* 16 bpp to 16 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[4] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[5] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[6] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[7] = ((gctUINT32_PTR) src)[1];

                /* Move to next tile. */
                trgLine += 16 * 2;
                srcLine +=  4 * 1; /* srcLine is in 16 bits. */
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT16_PTR src;

                /* 16 bpp to 16 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = src[0] | (src[1] << 16);
                ((gctUINT32_PTR) trgLine)[1] = src[2] | (src[3] << 16);

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[2] = src[0] | (src[1] << 16);
                ((gctUINT32_PTR) trgLine)[3] = src[2] | (src[3] << 16);

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[4] = src[0] | (src[1] << 16);
                ((gctUINT32_PTR) trgLine)[5] = src[2] | (src[3] << 16);

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[6] = src[0] | (src[1] << 16);
                ((gctUINT32_PTR) trgLine)[7] = src[2] | (src[3] << 16);

                /* Move to next tile. */
                trgLine += 16 * 2;
                srcLine +=  4 * 1; /* srcLine is in 16 bits. */
            }
        }
    }
}

static void
_Upload16bppto16bppBE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* 16 bpp to 16 bpp. */
            ((gctUINT16_PTR) trgLine)[0] = ((gctUINT16_PTR) srcLine)[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* 16 bpp to 16 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned case. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
                    ((gctUINT32_PTR) trgLine)[1] = (srcLine[4] << 24) | (srcLine[5] << 16) | (srcLine[6] << 8) | srcLine[7];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* 16 bpp to 16 bpp: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = ((gctUINT16_PTR) srcLine)[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + X  * 2;

        if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
        {
            /* Special optimization for aligned case. */
            for (x = X; x < Right; x += 4)
            {
                gctUINT16_PTR src;

                /* 16 bpp to 16 bpp: one tile. */
                src = (gctUINT16_PTR) srcLine;
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[4] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[5] = ((gctUINT32_PTR) src)[1];

                src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
                ((gctUINT32_PTR) trgLine)[6] = ((gctUINT32_PTR) src)[0];
                ((gctUINT32_PTR) trgLine)[7] = ((gctUINT32_PTR) src)[1];

                /* Move to next tile. */
                trgLine += 16 * 2;
                srcLine +=  4 * 2;
            }
        }
        else
        {
            for (x = X; x < Right; x += 4)
            {
                gctUINT8_PTR src;

                /* 16 bpp to 16 bpp: one tile. */
                src = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                ((gctUINT32_PTR) trgLine)[1] = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[2] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                ((gctUINT32_PTR) trgLine)[3] = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[4] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                ((gctUINT32_PTR) trgLine)[5] = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[6] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                ((gctUINT32_PTR) trgLine)[7] = (src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7];

                /* Move to next tile. */
                trgLine += 16 * 2;
                srcLine +=  4 * 2;
            }
        }
    }
}

static void
_UploadRGBA4444toARGB4444(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB4444. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12) | ((srcLine[1] & 0xFFF0) << 12) | ((srcLine[1] & 0xF) << 28) ;
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 4) | ((srcLine[2] & 0xF) << 12) | ((srcLine[3] & 0xFFF0) << 12) | ((srcLine[3] & 0xF) << 28) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA4444 to ARGB4444: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[0] = (src[0] >> 4) | ((src[0] & 0xF) << 12) | ((src[1] & 0xFFF0) << 12) | ((src[1] & 0xF) << 28) ;
            ((gctUINT32_PTR) trgLine)[1] = (src[2] >> 4) | ((src[2] & 0xF) << 12) | ((src[3] & 0xFFF0) << 12) | ((src[3] & 0xF) << 28) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[2] = (src[0] >> 4) | ((src[0] & 0xF) << 12) | ((src[1] & 0xFFF0) << 12) | ((src[1] & 0xF) << 28) ;
            ((gctUINT32_PTR) trgLine)[3] = (src[2] >> 4) | ((src[2] & 0xF) << 12) | ((src[3] & 0xFFF0) << 12) | ((src[3] & 0xF) << 28) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[4] = (src[0] >> 4) | ((src[0] & 0xF) << 12) | ((src[1] & 0xFFF0) << 12) | ((src[1] & 0xF) << 28) ;
            ((gctUINT32_PTR) trgLine)[5] = (src[2] >> 4) | ((src[2] & 0xF) << 12) | ((src[3] & 0xFFF0) << 12) | ((src[3] & 0xF) << 28) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[6] = (src[0] >> 4) | ((src[0] & 0xF) << 12) | ((src[1] & 0xFFF0) << 12) | ((src[1] & 0xF) << 28) ;
            ((gctUINT32_PTR) trgLine)[7] = (src[2] >> 4) | ((src[2] & 0xF) << 12) | ((src[3] & 0xFFF0) << 12) | ((src[3] & 0xF) << 28) ;

            /* Move to next tile. */
            trgLine += 16 * 2;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA4444toARGB4444BE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB4444. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFF0) << 12) | ((srcLine[0] & 0xF) << 28) | (srcLine[1] >> 4) | ((srcLine[1] & 0xF) << 12) ;
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFF0) << 12) | ((srcLine[2] & 0xF) << 28) | (srcLine[3] >> 4) | ((srcLine[3] & 0xF) << 12) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA4444 to ARGB4444: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[0] = ((src[0] & 0xFFF0) << 12) | ((src[0] & 0xF) << 28) | (src[1] >> 4) | ((src[1] & 0xF) << 12) ;
            ((gctUINT32_PTR) trgLine)[1] = ((src[2] & 0xFFF0) << 12) | ((src[2] & 0xF) << 28) | (src[3] >> 4) | ((src[3] & 0xF) << 12) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[2] = ((src[0] & 0xFFF0) << 12) | ((src[0] & 0xF) << 28) | (src[1] >> 4) | ((src[1] & 0xF) << 12) ;
            ((gctUINT32_PTR) trgLine)[3] = ((src[2] & 0xFFF0) << 12) | ((src[2] & 0xF) << 28) | (src[3] >> 4) | ((src[3] & 0xF) << 12) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[4] = ((src[0] & 0xFFF0) << 12) | ((src[0] & 0xF) << 28) | (src[1] >> 4) | ((src[1] & 0xF) << 12) ;
            ((gctUINT32_PTR) trgLine)[5] = ((src[2] & 0xFFF0) << 12) | ((src[2] & 0xF) << 28) | (src[3] >> 4) | ((src[3] & 0xF) << 12) ;

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[6] = ((src[0] & 0xFFF0) << 12) | ((src[0] & 0xF) << 28) | (src[1] >> 4) | ((src[1] & 0xF) << 12) ;
            ((gctUINT32_PTR) trgLine)[7] = ((src[2] & 0xFFF0) << 12) | ((src[2] & 0xF) << 28) | (src[3] >> 4) | ((src[3] & 0xF) << 12) ;

            /* Move to next tile. */
            trgLine += 16 * 2;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA5551toARGB1555(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15) | ((srcLine[1] & 0xFFFE) << 15) | ((srcLine[1] & 0x1) << 31);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 1) | ((srcLine[2] & 0x1) << 15) | ((srcLine[3] & 0xFFFE) << 15) | ((srcLine[3] & 0x1) << 31) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA5551 to ARGB1555: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[0] = (src[0] >> 1) | ((src[0] & 0x1) << 15) | ((src[1] & 0xFFFE) << 15) | ((src[1] & 0x1) << 31);
            ((gctUINT32_PTR) trgLine)[1] = (src[2] >> 1) | ((src[2] & 0x1) << 15) | ((src[3] & 0xFFFE) << 15) | ((src[3] & 0x1) << 31);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[2] = (src[0] >> 1) | ((src[0] & 0x1) << 15) | ((src[1] & 0xFFFE) << 15) | ((src[1] & 0x1) << 31);
            ((gctUINT32_PTR) trgLine)[3] = (src[2] >> 1) | ((src[2] & 0x1) << 15) | ((src[3] & 0xFFFE) << 15) | ((src[3] & 0x1) << 31);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[4] = (src[0] >> 1) | ((src[0] & 0x1) << 15) | ((src[1] & 0xFFFE) << 15) | ((src[1] & 0x1) << 31);
            ((gctUINT32_PTR) trgLine)[5] = (src[2] >> 1) | ((src[2] & 0x1) << 15) | ((src[3] & 0xFFFE) << 15) | ((src[3] & 0x1) << 31);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[6] = (src[0] >> 1) | ((src[0] & 0x1) << 15) | ((src[1] & 0xFFFE) << 15) | ((src[1] & 0x1) << 31);
            ((gctUINT32_PTR) trgLine)[7] = (src[2] >> 1) | ((src[2] & 0x1) << 15) | ((src[3] & 0xFFFE) << 15) | ((src[3] & 0x1) << 31);

            /* Move to next tile. */
            trgLine += 16 * 2;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static void
_UploadRGBA5551toARGB1555BE(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFFE) << 15) | ((srcLine[0] & 0x1) << 31) | (srcLine[1] >> 1) | ((srcLine[1] & 0x1) << 15);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFFE) << 15) | ((srcLine[2] & 0x1) << 31) | (srcLine[3] >> 1) | ((srcLine[3] & 0x1) << 15);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + X * 2);

        for (x = X; x < Right; x += 4)
        {
            gctUINT16_PTR src;

            /* RGBA5551 to ARGB1555: one tile. */
            src = srcLine;
            ((gctUINT32_PTR) trgLine)[0] = ((src[0] & 0xFFFE) << 15) | ((src[0] & 0x1) << 31) | (src[1] >> 1) | ((src[1] & 0x1) << 15);
            ((gctUINT32_PTR) trgLine)[1] = ((src[2] & 0xFFFE) << 15) | ((src[2] & 0x1) << 31) | (src[3] >> 1) | ((src[3] & 0x1) << 15);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[2] = ((src[0] & 0xFFFE) << 15) | ((src[0] & 0x1) << 31) | (src[1] >> 1) | ((src[1] & 0x1) << 15);
            ((gctUINT32_PTR) trgLine)[3] = ((src[2] & 0xFFFE) << 15) | ((src[2] & 0x1) << 31) | (src[3] >> 1) | ((src[3] & 0x1) << 15);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[4] = ((src[0] & 0xFFFE) << 15) | ((src[0] & 0x1) << 31) | (src[1] >> 1) | ((src[1] & 0x1) << 15);
            ((gctUINT32_PTR) trgLine)[5] = ((src[2] & 0xFFFE) << 15) | ((src[2] & 0x1) << 31) | (src[3] >> 1) | ((src[3] & 0x1) << 15);

            src = (gctUINT16_PTR) ((gctUINT8_PTR) src + SourceStride);
            ((gctUINT32_PTR) trgLine)[6] = ((src[0] & 0xFFFE) << 15) | ((src[0] & 0x1) << 31) | (src[1] >> 1) | ((src[1] & 0x1) << 15);
            ((gctUINT32_PTR) trgLine)[7] = ((src[2] & 0xFFFE) << 15) | ((src[2] & 0x1) << 31) | (src[3] >> 1) | ((src[3] & 0x1) << 15);

            /* Move to next tile. */
            trgLine += 16 * 2;
            srcLine +=  4 * 1; /* srcLine is in 16 bits. */
        }
    }
}

static gceSTATUS
_UploadTextureTiled(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT32 Offset,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    )
{
    gceSTATUS status;
    gctUINT x, y;
    gctUINT edgeX[6];
    gctUINT edgeY[6];
    gctUINT countX = 0;
    gctUINT countY = 0;
    gctUINT32 bitsPerPixel;
    gctUINT32 bytesPerLine;
    gctUINT32 bytesPerTile;
    gctBOOL srcOdd, trgOdd;
    gctUINT right  = X + Width;
    gctUINT bottom = Y + Height;
    gctUINT srcBit;
    gctUINT srcBitStep;
    gctINT srcStride;
    gctUINT8* srcPixel;
    gctUINT8* srcLine;
    gcsSURF_FORMAT_INFO_PTR srcFormatInfo;
    gcsSURF_FORMAT_INFO_PTR trgFormatInfo;
    gctUINT trgBit;
    gctUINT trgBitStep;
    gctUINT8* trgPixel;
    gctUINT8* trgLine;
    gctUINT8* trgTile;
    /*gctUINT32 trgAddress;*/
    gctPOINTER trgLogical;
    gctINT trgStride;
    gceSURF_FORMAT trgFormat;
    gctBOOL oclSrcFormat;

    gcmHEADER_ARG("Hardware=0x%x TargetInfo=0x%x Offset=%u "
                  "X=%u Y=%u Width=%u Height=%u "
                  "Memory=0x%x SourceStride=%d SourceFormat=%d",
                  Hardware, TargetInfo, Offset, X, Y,
                  Width, Height, Memory, SourceStride, SourceFormat);

    gcmGETHARDWARE(Hardware);

    /* Handle the OCL bit. */
    oclSrcFormat = (SourceFormat & gcvSURF_FORMAT_OCL) != 0;
    SourceFormat = (gceSURF_FORMAT) (SourceFormat & ~gcvSURF_FORMAT_OCL);

    trgFormat  = TargetInfo->format;
    /*trgAddress = TargetInfo->node.physical;*/
    trgLogical = TargetInfo->node.logical;
    trgStride  = TargetInfo->stride;

    /* Compute dest logical. */
    trgLogical = (gctPOINTER) ((gctUINT8_PTR) trgLogical + Offset);

    /* Compute edge coordinates. */
    if (Width < 4)
    {
        for (x = X; x < right; x++)
        {
           edgeX[countX++] = x;
        }
    }
    else
    {
        for (x = X; x < ((X + 3) & ~3); x++)
        {
            edgeX[countX++] = x;
        }

        for (x = (right & ~3); x < right; x++)
        {
            edgeX[countX++] = x;
        }
    }

    if (Height < 4)
    {
       for (y = Y; y < bottom; y++)
       {
           edgeY[countY++] = y;
       }
    }
    else
    {
        for (y = Y; y < ((Y + 3) & ~3); y++)
        {
           edgeY[countY++] = y;
        }

        for (y = (bottom & ~3); y < bottom; y++)
        {
           edgeY[countY++] = y;
        }
    }

    /* Fast path(s) for all common OpenGL ES formats. */
    if ((trgFormat == gcvSURF_A8R8G8B8)
    ||  (trgFormat == gcvSURF_X8R8G8B8)
    )
    {
        switch (SourceFormat)
        {
        case gcvSURF_A8:
            if (Hardware->bigEndian)
            {
                _UploadA8toARGBBE(trgLogical,
                                  trgStride,
                                  X, Y,
                                  right, bottom,
                                  edgeX, edgeY,
                                  countX, countY,
                                  Memory,
                                  (SourceStride == 0)
                                  ? (gctINT) Width
                                  : SourceStride);

            }
            else
            {
                _UploadA8toARGB(trgLogical,
                                trgStride,
                                X, Y,
                                right, bottom,
                                edgeX, edgeY,
                                countX, countY,
                                Memory,
                                (SourceStride == 0)
                                ? (gctINT) Width
                                : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_B8G8R8:
            if (Hardware->bigEndian)
            {
                _UploadBGRtoARGBBE(trgLogical,
                                   trgStride,
                                   X, Y,
                                   right, bottom,
                                   edgeX, edgeY,
                                   countX, countY,
                                   Memory,
                                   (SourceStride == 0)
                                   ? (gctINT) Width * 3
                                   : SourceStride);
            }
            else
            {
                _UploadBGRtoARGB(trgLogical,
                                 trgStride,
                                 X, Y,
                                 right, bottom,
                                 edgeX, edgeY,
                                 countX, countY,
                                 Memory,
                                 (SourceStride == 0)
                                 ? (gctINT) Width * 3
                                 : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_A8B8G8R8:
            if (Hardware->bigEndian)
            {
                _UploadABGRtoARGBBE(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 4
                                    : SourceStride);
            }
            else
            {
                _UploadABGRtoARGB(trgLogical,
                                  trgStride,
                                  X, Y,
                                  right, bottom,
                                  edgeX, edgeY,
                                  countX, countY,
                                  Memory,
                                  (SourceStride == 0)
                                  ? (gctINT) Width * 4
                                  : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_L8:
            if (Hardware->bigEndian)
            {
                _UploadL8toARGBBE(trgLogical,
                                  trgStride,
                                  X, Y,
                                  right, bottom,
                                  edgeX, edgeY,
                                  countX, countY,
                                  Memory,
                                  (SourceStride == 0)
                                  ? (gctINT) Width
                                  : SourceStride);
            }
            else
            {
                _UploadL8toARGB(trgLogical,
                                trgStride,
                                X, Y,
                                right, bottom,
                                edgeX, edgeY,
                                countX, countY,
                                Memory,
                                (SourceStride == 0)
                                ? (gctINT) Width
                                : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_A8L8:
            if (Hardware->bigEndian)
            {
                _UploadA8L8toARGBBE(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 2
                                    : SourceStride);
            }
            else
            {
                _UploadA8L8toARGB(trgLogical,
                                  trgStride,
                                  X, Y,
                                  right, bottom,
                                  edgeX, edgeY,
                                  countX, countY,
                                  Memory,
                                  (SourceStride == 0)
                                  ? (gctINT) Width * 2
                                  : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_A8R8G8B8:
            if (Hardware->bigEndian)
            {
                _Upload32bppto32bppBE(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 4
                                      : SourceStride);
            }
            else
            {
                _Upload32bppto32bpp(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 4
                                    : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_R5G6B5:
            if (Hardware->bigEndian)
            {
                _UploadRGB565toARGBBE(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                _UploadRGB565toARGB(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 2
                                    : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_R4G4B4A4:
            if (Hardware->bigEndian)
            {
                 _UploadRGBA4444toARGBBE(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width * 2
                                        : SourceStride);
            }
            else
            {
                 _UploadRGBA4444toARGB(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case gcvSURF_R5G5B5A1:
            if (Hardware->bigEndian)
            {
                _UploadRGBA5551toARGBBE(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width * 2
                                        : SourceStride);
            }
            else
            {
                _UploadRGBA5551toARGB(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }

            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        default:
            break;
        }
    }

    else
    {
        switch (SourceFormat)
        {
        case gcvSURF_A8:
            if (trgFormat == gcvSURF_A8)
            {
                if (Hardware->bigEndian)
                {
                    _Upload8bppto8bppBE(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width
                                        : SourceStride);
                }
                else
                {
                    _Upload8bppto8bpp(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_B8G8R8:
            if (trgFormat == gcvSURF_X8R8G8B8)
            {
               /* Same as BGR to ARGB. */
                if (Hardware->bigEndian)
                {
                    _UploadBGRtoARGBBE(trgLogical,
                                       trgStride,
                                       X, Y,
                                       right, bottom,
                                       edgeX, edgeY,
                                       countX, countY,
                                       Memory,
                                       (SourceStride == 0)
                                       ? (gctINT) Width * 3
                                       : SourceStride);
                }
                else
                {
                    _UploadBGRtoARGB(trgLogical,
                                     trgStride,
                                     X, Y,
                                     right, bottom,
                                     edgeX, edgeY,
                                     countX, countY,
                                     Memory,
                                     (SourceStride == 0)
                                     ? (gctINT) Width * 3
                                     : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            else if (trgFormat == gcvSURF_X8B8G8R8)
            {
                if (Hardware->bigEndian)
                {
                    _UploadBGRtoABGRBE(trgLogical,
                                       trgStride,
                                       X, Y,
                                       right, bottom,
                                       edgeX, edgeY,
                                       countX, countY,
                                       Memory,
                                       (SourceStride == 0)
                                       ? (gctINT) Width * 3
                                       : SourceStride);
                }
                else
                {
                    _UploadBGRtoABGR(trgLogical,
                                     trgStride,
                                     X, Y,
                                     right, bottom,
                                     edgeX, edgeY,
                                     countX, countY,
                                     Memory,
                                     (SourceStride == 0)
                                     ? (gctINT) Width * 3
                                     : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }

            break;

        /* case gcvSURF_A8B8G8R8: */

        case gcvSURF_L8:
            if (trgFormat == gcvSURF_L8)
            {
                if (Hardware->bigEndian)
                {
                    _Upload8bppto8bppBE(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width
                                        : SourceStride);
                }
                else
                {
                    _Upload8bppto8bpp(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_A8L8:
            if (trgFormat == gcvSURF_A8L8)
            {
                if (Hardware->bigEndian)
                {
                    _Upload16bppto16bppBE(trgLogical,
                                          trgStride,
                                          X, Y,
                                          right, bottom,
                                          edgeX, edgeY,
                                          countX, countY,
                                          Memory,
                                          (SourceStride == 0)
                                          ? (gctINT) Width * 2
                                          : SourceStride);
                }
                else
                {
                    _Upload16bppto16bpp(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width * 2
                                        : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        /* case gcvSURF_A8R8G8B8: */

        case gcvSURF_R5G6B5:
            if (trgFormat == gcvSURF_R5G6B5)
            {
                if (Hardware->bigEndian)
                {
                    _Upload16bppto16bppBE(trgLogical,
                                          trgStride,
                                          X, Y,
                                          right, bottom,
                                          edgeX, edgeY,
                                          countX, countY,
                                          Memory,
                                          (SourceStride == 0)
                                          ? (gctINT) Width * 2
                                          : SourceStride);
                }
                else
                {
                    _Upload16bppto16bpp(trgLogical,
                                        trgStride,
                                        X, Y,
                                        right, bottom,
                                        edgeX, edgeY,
                                        countX, countY,
                                        Memory,
                                        (SourceStride == 0)
                                        ? (gctINT) Width * 2
                                        : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_R4G4B4A4:
            if (trgFormat == gcvSURF_A4R4G4B4)
            {
                if (Hardware->bigEndian)
                {
                    _UploadRGBA4444toARGB4444BE(trgLogical,
                                                trgStride,
                                                X, Y,
                                                right, bottom,
                                                edgeX, edgeY,
                                                countX, countY,
                                                Memory,
                                                (SourceStride == 0)
                                                ? (gctINT) Width * 2
                                                : SourceStride);
                }
                else
                {
                    _UploadRGBA4444toARGB4444(trgLogical,
                                              trgStride,
                                              X, Y,
                                              right, bottom,
                                              edgeX, edgeY,
                                              countX, countY,
                                              Memory,
                                              (SourceStride == 0)
                                              ? (gctINT) Width * 2
                                              : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_R5G5B5A1:
            if (trgFormat == gcvSURF_A1R5G5B5)
            {
                if (Hardware->bigEndian)
                {
                    _UploadRGBA5551toARGB1555BE(trgLogical,
                                                trgStride,
                                                X, Y,
                                                right, bottom,
                                                edgeX, edgeY,
                                                countX, countY,
                                                Memory,
                                                (SourceStride == 0)
                                                ? (gctINT) Width * 2
                                                : SourceStride);
                }
                else
                {
                    _UploadRGBA5551toARGB1555(trgLogical,
                                              trgStride,
                                              X, Y,
                                              right, bottom,
                                              edgeX, edgeY,
                                              countX, countY,
                                              Memory,
                                              (SourceStride == 0)
                                              ? (gctINT) Width * 2
                                              : SourceStride);
                }

                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
            break;

        default:
            break;
        }
    }

    /* Get number of bytes per pixel and tile for the format. */
    gcmONERROR(gcoHARDWARE_ConvertFormat(trgFormat,
                                         &bitsPerPixel,
                                         &bytesPerTile));

    /* Check for OpenCL image. */
    if (oclSrcFormat)
    {
        /* OpenCL image has to be linear without alignment. */
        /* There is no format conversion dur to OpenCL Map API. */
        gctUINT32 bytesPerPixel = bitsPerPixel / 8;

        /* Compute proper memory stride. */
        srcStride = (SourceStride == 0)
                  ? (gctINT) (Width * bytesPerPixel)
                  : SourceStride;

        srcLine = (gctUINT8_PTR) Memory + bytesPerPixel * X;
        trgLine = (gctUINT8_PTR) trgLogical + Offset + bytesPerPixel * X;
        for (y = Y; y < Height; y++)
        {
            gcoOS_MemCopy(trgLine, srcLine, bytesPerPixel * Width);

            srcLine += SourceStride;
            trgLine += SourceStride;
        }

        /* Success. */
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /* Compute number of bytes per tile line. */
    bytesPerLine = bitsPerPixel * 4 / 8;

    /* See if the memory format equals the texture format (easy case). */
    if ((SourceFormat == trgFormat)
    ||  ((SourceFormat == gcvSURF_A8R8G8B8) && (trgFormat == gcvSURF_X8R8G8B8))
    ||  ((SourceFormat == gcvSURF_A8B8G8R8) && (trgFormat == gcvSURF_X8B8G8R8)))
    {
        switch (bitsPerPixel)
        {
        case 8:
            if (Hardware->bigEndian)
            {
                _Upload8bppto8bppBE(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width
                                    : SourceStride);
            }
            else
            {
                _Upload8bppto8bpp(trgLogical,
                                  trgStride,
                                  X, Y,
                                  right, bottom,
                                  edgeX, edgeY,
                                  countX, countY,
                                  Memory,
                                  (SourceStride == 0)
                                  ? (gctINT) Width
                                  : SourceStride);
            }

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case 16:
            if (Hardware->bigEndian)
            {
                _Upload16bppto16bppBE(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                _Upload16bppto16bpp(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 2
                                    : SourceStride);
            }

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case 32:
            if (Hardware->bigEndian)
            {
                _Upload32bppto32bppBE(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 4
                                      : SourceStride);
            }
            else
            {
                _Upload32bppto32bpp(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 4
                                    : SourceStride);
            }

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        case 64:
            if (Hardware->bigEndian)
            {
                _Upload64bppto64bppBE(trgLogical,
                                      trgStride,
                                      X, Y,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 8
                                      : SourceStride);
            }
            else
            {
                _Upload64bppto64bpp(trgLogical,
                                    trgStride,
                                    X, Y,
                                    right, bottom,
                                    edgeX, edgeY,
                                    countX, countY,
                                    Memory,
                                    (SourceStride == 0)
                                    ? (gctINT) Width * 8
                                    : SourceStride);
            }

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;

        default:
            /* Invalid format. */
            gcmASSERT(gcvTRUE);
        }

        /* Compute proper memory stride. */
        srcStride = (SourceStride == 0)
                  ? (gctINT) (Width * bitsPerPixel / 8)
                  : SourceStride;

        if (SourceFormat == gcvSURF_DXT1)
        {
            gcmASSERT((X == 0) && (Y == 0));

            srcLine      = (gctUINT8_PTR) Memory;
            trgLine      = (gctUINT8_PTR) trgLogical;
            bytesPerLine = Width * bytesPerTile / 4;

            for (y = Y; y < Height; y += 4)
            {
                gcoOS_MemCopy(trgLine, srcLine, bytesPerLine);

                trgLine += trgStride;
                srcLine += srcStride * 4;
            }

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;
        }

        /* Success. */
        gcmFOOTER_ARG("bitsPerPixel=%d not supported", bitsPerPixel);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    gcmTRACE_ZONE(
        gcvLEVEL_WARNING, gcvZONE_TEXTURE,
        "Slow path: SourceFormat=%d trgFormat=%d",
        SourceFormat, trgFormat
        );

    /* Query format specifics. */
    trgFormatInfo = &TargetInfo->formatInfo;
    gcmONERROR(gcoSURF_QueryFormat(SourceFormat, &srcFormatInfo));

    /* Determine bit step. */
    srcBitStep = srcFormatInfo->interleaved
               ? srcFormatInfo->bitsPerPixel * 2
               : srcFormatInfo->bitsPerPixel;

    trgBitStep = trgFormatInfo->interleaved
               ? trgFormatInfo->bitsPerPixel * 2
               : trgFormatInfo->bitsPerPixel;

    /* Compute proper memory stride. */
    srcStride = (SourceStride == 0)
              ? (gctINT) Width * ((srcFormatInfo->bitsPerPixel + 7) >> 3)
              : SourceStride;

    /* Compute the starting source and target addresses. */
    srcLine = (gctUINT8_PTR) Memory;

    /* Compute Address of line inside the tile in which X,Y reside. */
    trgLine = (gctUINT8_PTR) trgLogical
            + (Y & ~3) * trgStride + (Y & 3) * bytesPerLine
            + (X >> 2) * bytesPerTile;

    /* Walk all vertical lines. */
    for (y = Y; y < bottom; y++)
    {
        /* Get starting source and target addresses. */
        srcPixel = srcLine;
        trgTile  = trgLine;
        trgPixel = trgTile + (X & 3) * bitsPerPixel / 8;

        /* Reset bit positions. */
        srcBit = 0;
        trgBit = 0;

        /* Walk all horizontal pixels. */
        for (x = X; x < right; x++)
        {
            /* Determine whether to use odd format descriptor for the current
            ** pixel. */
            srcOdd = x & srcFormatInfo->interleaved;
            trgOdd = x & trgFormatInfo->interleaved;

            /* Convert the current pixel. */
            gcmONERROR(gcoHARDWARE_ConvertPixel(srcPixel,
                                                trgPixel,
                                                srcBit,
                                                trgBit,
                                                srcFormatInfo,
                                                trgFormatInfo,
                                                gcvNULL,
                                                gcvNULL,
                                                srcOdd,
                                                trgOdd));

            /* Move to the next pixel in the source. */
            if (!srcFormatInfo->interleaved || srcOdd)
            {
                srcBit   += srcBitStep;
                srcPixel += srcBit >> 3;
                srcBit   &= 7;
            }

            /* Move to the next pixel in the target. */
            if ((x & 3) < 3)
            {
                /* Still within a tile, update to next pixel. */
                if (!trgFormatInfo->interleaved || trgOdd)
                {
                    trgBit   += trgBitStep;
                    trgPixel += trgBit >> 3;
                    trgBit   &= 7;
                }
            }
            else
            {
                /* Move to next tiled address in target. */
                trgTile += bytesPerTile;
                trgPixel = trgTile;
                trgBit = 0;
            }
        }

        /* Move to next line. */
        srcLine += srcStride;

        if ((y & 3) < 3)
        {
            /* Still within a tile, update to next line inside the tile. */
            trgLine += bytesPerLine;
        }
        else
        {
            /* New tile, move to beginning of new tile. */
            trgLine += trgStride * 4 - 3 * bytesPerLine;
        }
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if !defined(UNDER_CE)
static void
_UploadSuperTiledL8toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | (srcLine[1] << 16) | (srcLine[1] << 8) | srcLine[1];
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2];
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[3] << 8) | srcLine[3];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0];
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | (srcLine[1] << 16) | (srcLine[1] << 8) | srcLine[1];
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2];
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[3] << 8) | srcLine[3];
        }
    }
}

static void
_UploadSuperTiledL8toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF | (srcLine[1] << 8) | (srcLine[1] << 16) | (srcLine[1] << 24);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF | (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF | (srcLine[3] << 8) | (srcLine[3] << 16) | (srcLine[3] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* L8 to ARGB: part tile row. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* L8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24);
            ((gctUINT32_PTR) trgLine)[1] = 0xFF | (srcLine[1] << 8) | (srcLine[1] << 16) | (srcLine[1] << 24);
            ((gctUINT32_PTR) trgLine)[2] = 0xFF | (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24);
            ((gctUINT32_PTR) trgLine)[3] = 0xFF | (srcLine[3] << 8) | (srcLine[3] << 16) | (srcLine[3] << 24);
        }
    }
}


static void
_UploadSuperTiledA8L8toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2] | (srcLine[3] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[4] << 16) | (srcLine[4] << 8) | srcLine[4] | (srcLine[5] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[6] << 16) | (srcLine[6] << 8) | srcLine[6] | (srcLine[7] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[0] << 16) | (srcLine[0] << 8) | srcLine[0] | (srcLine[1] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[2] << 16) | (srcLine[2] << 8) | srcLine[2] | (srcLine[3] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[4] << 16) | (srcLine[4] << 8) | srcLine[4] | (srcLine[5] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[6] << 16) | (srcLine[6] << 8) | srcLine[6] | (srcLine[7] << 24);
        }
    }
}

static void
_UploadSuperTiledA8L8toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8L8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24) | srcLine[3];
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[4] << 8) | (srcLine[4] << 16) | (srcLine[4] << 24) | srcLine[5];
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[6] << 8) | (srcLine[6] << 16) | (srcLine[6] << 24) | srcLine[7];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

                /* A8L8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 2;

            /* A8L8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[0] << 16) | (srcLine[0] << 24) | srcLine[1];
            ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] << 8) | (srcLine[2] << 16) | (srcLine[2] << 24) | srcLine[3];
            ((gctUINT32_PTR) trgLine)[2] = (srcLine[4] << 8) | (srcLine[4] << 16) | (srcLine[4] << 24) | srcLine[5];
            ((gctUINT32_PTR) trgLine)[3] = (srcLine[6] << 8) | (srcLine[6] << 16) | (srcLine[6] << 24) | srcLine[7];
        }
    }
}

static void
_UploadSuperTiledA8toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = srcLine[0] << 24;
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[0] << 24;
                ((gctUINT32_PTR) trgLine)[1] = srcLine[1] << 24;
                ((gctUINT32_PTR) trgLine)[2] = srcLine[2] << 24;
                ((gctUINT32_PTR) trgLine)[3] = srcLine[3] << 24;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = srcLine[0] << 24;
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = srcLine[0] << 24;
            ((gctUINT32_PTR) trgLine)[ 1] = srcLine[1] << 24;
            ((gctUINT32_PTR) trgLine)[ 2] = srcLine[2] << 24;
            ((gctUINT32_PTR) trgLine)[ 3] = srcLine[3] << 24;
        }
    }
}

static void
_UploadSuperTiledA8toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (gctUINT32) srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (gctUINT32) srcLine[0];
                ((gctUINT32_PTR) trgLine)[1] = (gctUINT32) srcLine[1];
                ((gctUINT32_PTR) trgLine)[2] = (gctUINT32) srcLine[2];
                ((gctUINT32_PTR) trgLine)[3] = (gctUINT32) srcLine[3];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* A8 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (gctUINT32) srcLine[0];
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* A8 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = (gctUINT32) srcLine[0];
            ((gctUINT32_PTR) trgLine)[ 1] = (gctUINT32) srcLine[1];
            ((gctUINT32_PTR) trgLine)[ 2] = (gctUINT32) srcLine[2];
            ((gctUINT32_PTR) trgLine)[ 3] = (gctUINT32) srcLine[3];
        }
    }
}

static void
_UploadSuperTiledRGB565toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGB565 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000
                                           | (((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0xE000) << 3))
                                           | (((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x600) >> 1))
                                           | (((srcLine[0] & 0x1F) << 3) | ((srcLine[0] & 0x1C) >> 2));
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000
                                               | (((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0xE000) << 3))
                                               | (((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x600) >> 1))
                                               | (((srcLine[0] & 0x1F) << 3) | ((srcLine[0] & 0x1C) >> 2));
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000
                                               | (((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0xE000) << 3))
                                               | (((srcLine[1] & 0x7E0) << 5) | ((srcLine[1] & 0x600) >> 1))
                                               | (((srcLine[1] & 0x1F) << 3) | ((srcLine[1] & 0x1C) >> 2));
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000
                                               | (((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0xE000) << 3))
                                               | (((srcLine[2] & 0x7E0) << 5) | ((srcLine[2] & 0x600) >> 1))
                                               | (((srcLine[2] & 0x1F) << 3) | ((srcLine[2] & 0x1C) >> 2));
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000
                                               | (((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0xE000) << 3))
                                               | (((srcLine[3] & 0x7E0) << 5) | ((srcLine[3] & 0x600) >> 1))
                                               | (((srcLine[3] & 0x1F) << 3) | ((srcLine[3] & 0x1C) >> 2));
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000
                                               | (((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0xE000) << 3))
                                               | (((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x600) >> 1))
                                               | (((srcLine[0] & 0x1F) << 3) | ((srcLine[0] & 0x1C) >> 2));
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGB565 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000
                                           | (((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0xE000) << 3))
                                           | (((srcLine[0] & 0x7E0) << 5) | ((srcLine[0] & 0x600) >> 1))
                                           | (((srcLine[0] & 0x1F) << 3) | ((srcLine[0] & 0x1C) >> 2));
            ((gctUINT32_PTR) trgLine)[1] = 0xFF000000
                                           | (((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0xE000) << 3))
                                           | (((srcLine[1] & 0x7E0) << 5) | ((srcLine[1] & 0x600) >> 1))
                                           | (((srcLine[1] & 0x1F) << 3) | ((srcLine[1] & 0x1C) >> 2));
            ((gctUINT32_PTR) trgLine)[2] = 0xFF000000
                                           | (((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0xE000) << 3))
                                           | (((srcLine[2] & 0x7E0) << 5) | ((srcLine[2] & 0x600) >> 1))
                                           | (((srcLine[2] & 0x1F) << 3) | ((srcLine[2] & 0x1C) >> 2));
            ((gctUINT32_PTR) trgLine)[3] = 0xFF000000
                                           | (((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0xE000) << 3))
                                           | (((srcLine[3] & 0x7E0) << 5) | ((srcLine[3] & 0x600) >> 1))
                                           | (((srcLine[3] & 0x1F) << 3) | ((srcLine[3] & 0x1C) >> 2));
        }
    }
}

static void
_UploadSuperTiledRGB565toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGB565 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF | ((srcLine[2] & 0x1F) << 27) | ((srcLine[2] & 0xE0) << 5) | ((srcLine[3] & 0x07) << 13) | ((srcLine[3] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF | ((srcLine[4] & 0x1F) << 27) | ((srcLine[4] & 0xE0) << 5) | ((srcLine[5] & 0x07) << 13) | ((srcLine[5] & 0xF8) << 8);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF | ((srcLine[6] & 0x1F) << 27) | ((srcLine[6] & 0xE0) << 5) | ((srcLine[7] & 0x07) << 13) | ((srcLine[7] & 0xF8) << 8);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGB565 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGB565 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF | ((srcLine[0] & 0x1F) << 27) | ((srcLine[0] & 0xE0) << 5) | ((srcLine[1] & 0x07) << 13) | ((srcLine[1] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[1] = 0xFF | ((srcLine[2] & 0x1F) << 27) | ((srcLine[2] & 0xE0) << 5) | ((srcLine[3] & 0x07) << 13) | ((srcLine[3] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[2] = 0xFF | ((srcLine[4] & 0x1F) << 27) | ((srcLine[4] & 0xE0) << 5) | ((srcLine[5] & 0x07) << 13) | ((srcLine[5] & 0xF8) << 8);
            ((gctUINT32_PTR) trgLine)[3] = 0xFF | ((srcLine[6] & 0x1F) << 27) | ((srcLine[6] & 0xE0) << 5) | ((srcLine[7] & 0x07) << 13) | ((srcLine[7] & 0xF8) << 8);
        }
    }
}

static void
_UploadSuperTiledRGBA4444toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[1] & 0xF) << 28) | ((srcLine[1] & 0xF000) << 8) | ((srcLine[1] & 0xF00) << 4) | (srcLine[1] & 0xF0);
                ((gctUINT32_PTR) trgLine)[2] = ((srcLine[2] & 0xF) << 28) | ((srcLine[2] & 0xF000) << 8) | ((srcLine[2] & 0xF00) << 4) | (srcLine[2] & 0xF0);
                ((gctUINT32_PTR) trgLine)[3] = ((srcLine[3] & 0xF) << 28) | ((srcLine[3] & 0xF000) << 8) | ((srcLine[3] & 0xF00) << 4) | (srcLine[3] & 0xF0);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                 yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = ((srcLine[0] & 0xF) << 28) | ((srcLine[0] & 0xF000) << 8) | ((srcLine[0] & 0xF00) << 4) | (srcLine[0] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 1] = ((srcLine[1] & 0xF) << 28) | ((srcLine[1] & 0xF000) << 8) | ((srcLine[1] & 0xF00) << 4) | (srcLine[1] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 2] = ((srcLine[2] & 0xF) << 28) | ((srcLine[2] & 0xF000) << 8) | ((srcLine[2] & 0xF00) << 4) | (srcLine[2] & 0xF0);
            ((gctUINT32_PTR) trgLine)[ 3] = ((srcLine[3] & 0xF) << 28) | ((srcLine[3] & 0xF000) << 8) | ((srcLine[3] & 0xF00) << 4) | (srcLine[3] & 0xF0);
        }
    }
}


static void
_UploadSuperTiledRGBA4444toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB. */
            ((gctUINT32_PTR) trgLine)[ 0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                 yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[ 1] = ((srcLine[1] & 0xF) << 20) | (srcLine[1] & 0xF0) | ((srcLine[1] & 0xF00) >> 4) | ((srcLine[1] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[ 2] = ((srcLine[2] & 0xF) << 20) | (srcLine[2] & 0xF0) | ((srcLine[2] & 0xF00) >> 4) | ((srcLine[2] & 0xF000) << 16);
                ((gctUINT32_PTR) trgLine)[ 3] = ((srcLine[3] & 0xF) << 20) | (srcLine[3] & 0xF0) | ((srcLine[3] & 0xF00) >> 4) | ((srcLine[3] & 0xF000) << 16);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                 yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB: one tile. */
            ((gctUINT32_PTR) trgLine)[ 0] = ((srcLine[0] & 0xF) << 20) | (srcLine[0] & 0xF0) | ((srcLine[0] & 0xF00) >> 4) | ((srcLine[0] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 1] = ((srcLine[1] & 0xF) << 20) | (srcLine[1] & 0xF0) | ((srcLine[1] & 0xF00) >> 4) | ((srcLine[1] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 2] = ((srcLine[2] & 0xF) << 20) | (srcLine[2] & 0xF0) | ((srcLine[2] & 0xF00) >> 4) | ((srcLine[2] & 0xF000) << 16);
            ((gctUINT32_PTR) trgLine)[ 3] = ((srcLine[3] & 0xF) << 20) | (srcLine[3] & 0xF0) | ((srcLine[3] & 0xF00) >> 4) | ((srcLine[3] & 0xF000) << 16);
        }
    }
}

static void
_UploadSuperTiledRGBA5551toARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0x7C0) << 5) | ((srcLine[1] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0x7C0) << 5) | ((srcLine[2] & 0x3E) << 2);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0x7C0) << 5) | ((srcLine[3] & 0x3E) << 2);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[0] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[0] & 0xF800) << 8) | ((srcLine[0] & 0x7C0) << 5) | ((srcLine[0] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[1] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[1] & 0xF800) << 8) | ((srcLine[1] & 0x7C0) << 5) | ((srcLine[1] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[2] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[2] & 0xF800) << 8) | ((srcLine[2] & 0x7C0) << 5) | ((srcLine[2] & 0x3E) << 2);
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[3] & 0x1 ? 0xFF000000 : 0x0) | ((srcLine[3] & 0xF800) << 8) | ((srcLine[3] & 0x7C0) << 5) | ((srcLine[3] & 0x3E) << 2);
         }
    }
}

static void
_UploadSuperTiledRGBA5551toARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0x1) << 7) | ((srcLine[3] & 0xF8) << 8) | ((srcLine[2] & 0xC0) << 13) | ((srcLine[3] & 0x07) << 21) | ((srcLine[2] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[2] = ((srcLine[4] & 0x1) << 7) | ((srcLine[5] & 0xF8) << 8) | ((srcLine[4] & 0xC0) << 13) | ((srcLine[5] & 0x07) << 21) | ((srcLine[4] & 0x3E) << 26);
                ((gctUINT32_PTR) trgLine)[3] = ((srcLine[6] & 0x1) << 7) | ((srcLine[7] & 0xF8) << 8) | ((srcLine[6] & 0xC0) << 13) | ((srcLine[7] & 0x07) << 21) | ((srcLine[6] & 0x3E) << 26);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0x1) << 7) | ((srcLine[1] & 0xF8) << 8) | ((srcLine[0] & 0xC0) << 13) | ((srcLine[1] & 0x07) << 21) | ((srcLine[0] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0x1) << 7) | ((srcLine[3] & 0xF8) << 8) | ((srcLine[2] & 0xC0) << 13) | ((srcLine[3] & 0x07) << 21) | ((srcLine[2] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[2] = ((srcLine[4] & 0x1) << 7) | ((srcLine[5] & 0xF8) << 8) | ((srcLine[4] & 0xC0) << 13) | ((srcLine[5] & 0x07) << 21) | ((srcLine[4] & 0x3E) << 26);
            ((gctUINT32_PTR) trgLine)[3] = ((srcLine[6] & 0x1) << 7) | ((srcLine[7] & 0xF8) << 8) | ((srcLine[6] & 0xC0) << 13) | ((srcLine[7] & 0x07) << 21) | ((srcLine[6] & 0x3E) << 26);
         }
    }
}

static void
_UploadSuperTiledBGRtoABGR(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | srcLine[3] | (srcLine[ 4] << 8) | (srcLine[ 5] << 16);
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | srcLine[6] | (srcLine[ 7] << 8) | (srcLine[ 8] << 16);
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | srcLine[9] | (srcLine[10] << 8) | (srcLine[11] << 16);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: part tile row. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR: 4 pixels. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | srcLine[0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16);
            ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | srcLine[3] | (srcLine[ 4] << 8) | (srcLine[ 5] << 16);
            ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | srcLine[6] | (srcLine[ 7] << 8) | (srcLine[ 8] << 16);
            ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | srcLine[9] | (srcLine[10] << 8) | (srcLine[11] << 16);
        }
    }
}

static void
_UploadSuperTiledBGRtoABGRBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[3] << 24) | (srcLine[ 4] << 16) | (srcLine[ 5] << 8);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[6] << 24) | (srcLine[ 7] << 16) | (srcLine[ 8] << 8);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[9] << 24) | (srcLine[10] << 16) | (srcLine[11] << 8);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ABGR: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ABGR: 4 pixels. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8);
            ((gctUINT32_PTR) trgLine)[1] = (srcLine[3] << 24) | (srcLine[ 4] << 16) | (srcLine[ 5] << 8);
            ((gctUINT32_PTR) trgLine)[2] = (srcLine[6] << 24) | (srcLine[ 7] << 16) | (srcLine[ 8] << 8);
            ((gctUINT32_PTR) trgLine)[3] = (srcLine[9] << 24) | (srcLine[10] << 16) | (srcLine[11] << 8);
        }
    }
}

static void
_UploadSuperTiledBGRtoARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2];
                ((gctUINT32_PTR) trgLine)[1] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[ 4] << 8) | srcLine[ 5];
                ((gctUINT32_PTR) trgLine)[2] = 0xFF000000 | (srcLine[6] << 16) | (srcLine[ 7] << 8) | srcLine[ 8];
                ((gctUINT32_PTR) trgLine)[3] = 0xFF000000 | (srcLine[9] << 16) | (srcLine[10] << 8) | srcLine[11];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2];
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB: 4 pixels. */
            ((gctUINT32_PTR) trgLine)[ 0] = 0xFF000000 | (srcLine[0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2];
            ((gctUINT32_PTR) trgLine)[ 1] = 0xFF000000 | (srcLine[3] << 16) | (srcLine[ 4] << 8) | srcLine[ 5];
            ((gctUINT32_PTR) trgLine)[ 2] = 0xFF000000 | (srcLine[6] << 16) | (srcLine[ 7] << 8) | srcLine[ 8];
            ((gctUINT32_PTR) trgLine)[ 3] = 0xFF000000 | (srcLine[9] << 16) | (srcLine[10] << 8) | srcLine[11];
        }
    }
}

static void
_UploadSuperTiledBGRtoARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 3);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[1] << 16) | (srcLine[2] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[3] << 8) | (srcLine[ 4] << 16) | (srcLine[ 5] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[6] << 8) | (srcLine[ 7] << 16) | (srcLine[ 8] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

                /* BGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24);
             }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 3;

            /* BGR to ARGB: 4 pixels. */
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[3] << 8) | (srcLine[ 4] << 16) | (srcLine[ 5] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[6] << 8) | (srcLine[ 7] << 16) | (srcLine[ 8] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
        }
    }
}

static void
_UploadSuperTiledABGRtoARGB(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT32 x, y;
    gctUINT32 xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;
    gctUINT i, j;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 16) | (srcLine[1] << 8) | srcLine[2] | (srcLine[3] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2] | (srcLine[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 16) | (srcLine[ 5] << 8) | srcLine[ 6] | (srcLine[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 16) | (srcLine[ 9] << 8) | srcLine[10] | (srcLine[11] << 24);
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 16) | (srcLine[13] << 8) | srcLine[14] | (srcLine[15] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2] | (srcLine[ 3] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB: 4 pixels. */
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[ 0] << 16) | (srcLine[ 1] << 8) | srcLine[ 2] | (srcLine[ 3] << 24);
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[ 4] << 16) | (srcLine[ 5] << 8) | srcLine[ 6] | (srcLine[ 7] << 24);
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[ 8] << 16) | (srcLine[ 9] << 8) | srcLine[10] | (srcLine[11] << 24);
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[12] << 16) | (srcLine[13] << 8) | srcLine[14] | (srcLine[15] << 24);
        }
    }
}

static void
_UploadSuperTiledABGRtoARGBBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctSIZE_T x, y;
    gctUINT i, j;
    gctSIZE_T xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 8) | (srcLine[1] << 16) | (srcLine[2] << 24) | srcLine[3];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24) | srcLine[ 3];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 8) | (srcLine[ 5] << 16) | (srcLine[ 6] << 24) | srcLine[ 7];
                ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 8) | (srcLine[ 9] << 16) | (srcLine[10] << 24) | srcLine[11];
                ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 8) | (srcLine[13] << 16) | (srcLine[14] << 24) | srcLine[15];
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* ABGR to ARGB: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24) | srcLine[ 3];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* ABGR to ARGB: one tile row. */
            ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[ 0] << 8) | (srcLine[ 1] << 16) | (srcLine[ 2] << 24) | srcLine[ 3];
            ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[ 4] << 8) | (srcLine[ 5] << 16) | (srcLine[ 6] << 24) | srcLine[ 7];
            ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[ 8] << 8) | (srcLine[ 9] << 16) | (srcLine[10] << 24) | srcLine[11];
            ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[12] << 8) | (srcLine[13] << 16) | (srcLine[14] << 24) | srcLine[15];
        }
    }
}
#endif

static gceSTATUS
_UploadSuperTiledABGR32ToABGR32_2Layer(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT8_PTR Target,
    IN gctUINT XOffset,
    IN gctUINT YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT8_PTR Source,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    )
{
    gctUINT x, y;
    gctUINT8_PTR target2, source;
    gctUINT32 offset = 0;

    target2 = Target + TargetInfo->layerSize;

    for (y = YOffset; y < Height + YOffset; y++)
    {
        source = Source + (y - YOffset) * SourceStride;

        for (x = XOffset; x < Width + XOffset; x++)
        {
            gcoHARDWARE_ComputeOffset(gcvNULL,
                                      x,
                                      y,
                                      TargetInfo->stride,
                                      8,
                                      TargetInfo->tiling,
                                      &offset);

            /* Copy the GR pixel to layer 0. */
            gcoOS_MemCopy(Target + offset, source, 8);

            source += 8;

            /* Copy the AB pixel to layer 1. */
            gcoOS_MemCopy(target2 + offset, source, 8);

            source += 8;
        }
    }

    return gcvSTATUS_OK;
}

static gceSTATUS
_UploadSuperTiledBGR32ToABGR32_2Layer(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT8_PTR Target,
    IN gctUINT XOffset,
    IN gctUINT YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT8_PTR Source,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    )
{
    gctUINT x, y;
    gctUINT8_PTR target2, source;
    gctUINT32 offset = 0;

    /* Note: Not assigning anything to alpha channel,
             as not needed. If needed, it needs to be set
             based on target format info type. */

    target2 = Target + TargetInfo->layerSize;

    for (y = YOffset; y < Height + YOffset; y++)
    {
        source = Source + (y - YOffset) * SourceStride;

        for (x = XOffset; x < Width + XOffset; x++)
        {
            gcoHARDWARE_ComputeOffset(gcvNULL,
                                      x,
                                      y,
                                      TargetInfo->stride,
                                      8,
                                      TargetInfo->tiling,
                                      &offset);

            /* Copy the GR pixel to layer 0. */
            gcoOS_MemCopy(Target + offset, source, 8);

            source += 8;

            /* Copy the B pixel to layer 1 as XB. */
            gcoOS_MemCopy(target2 + offset, source, 4);

            source += 4;
        }
    }

    return gcvSTATUS_OK;
}

#if !defined(UNDER_CE)
static void
_UploadSuperTiled8bppto8bpp(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* 8 bpp to 8 bpp. */
            trgLine[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR)srcLine)[0];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16) | (srcLine[3] << 24);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: part tile row. */
                trgLine[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) (srcLine + SourceStride * 0))[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) (srcLine + SourceStride * 1))[0];
                ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) (srcLine + SourceStride * 2))[0];
                ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) (srcLine + SourceStride * 3))[0];
            }
            else
            {
                gctUINT8_PTR src;

                /* 8 bpp to 8 bpp: one tile. */
                src  = srcLine;
                ((gctUINT32_PTR) trgLine)[0] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[1] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[2] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);

                src += SourceStride;
                ((gctUINT32_PTR) trgLine)[3] = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
            }
        }
    }
}

static void
_UploadSuperTiled8bppto8bppBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 1);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            /* 8 bpp to 8 bpp. */
            trgLine[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR)srcLine)[0];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

                /* 8 bpp to 8 bpp: part tile row. */
                trgLine[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 1;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 1;

            if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                /* 8 bpp to 8 bpp: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) (srcLine))[0];
            }
            else
            {
                /* 8 bpp to 8 bpp: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
            }
        }
    }
}

static void
_UploadSuperTiled16bppto16bpp(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* 16 bpp to 16 bpp. */
            ((gctUINT16_PTR) trgLine)[0] = srcLine[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned case. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 16);
                    ((gctUINT32_PTR) trgLine)[1] = srcLine[2] | (srcLine[3] << 16);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = srcLine[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

			trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                /* 16 bpp to 16 bpp: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
            }
            else
            {
                /* 16 bpp to 16 bpp: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 16);
                ((gctUINT32_PTR) trgLine)[1] = srcLine[2] | (srcLine[3] << 16);
            }
        }
    }
}

static void
_UploadSuperTiled16bppto16bppBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* 16 bpp to 16 bpp. */
            ((gctUINT16_PTR) trgLine)[0] = ((gctUINT16_PTR) srcLine)[0];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned case. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
                    ((gctUINT32_PTR) trgLine)[1] = (srcLine[4] << 24) | (srcLine[5] << 16) | (srcLine[6] << 8) | srcLine[7];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* 16 bpp to 16 bpp: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = ((gctUINT16_PTR) srcLine)[0];
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                /* 16 bpp to 16 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
            }
            else
            {
                /* 16 bpp to 16 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[4] << 24) | (srcLine[5] << 16) | (srcLine[6] << 8) | srcLine[7];
            }
        }
    }
}

static void
_UploadSuperTiled32bppto32bpp(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* 32bpp to 32bpp. */
            ((gctUINT32_PTR) trgLine)[0] = srcLine[0] | (srcLine[1] << 8) | (srcLine[2] << 16) | (srcLine[3] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: one tile row. */
                if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
                    ((gctUINT32_PTR) trgLine)[1] = srcLine[ 4] | (srcLine[ 5] << 8) | (srcLine[ 6] << 16) | (srcLine[ 7] << 24);
                    ((gctUINT32_PTR) trgLine)[2] = srcLine[ 8] | (srcLine[ 9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
                    ((gctUINT32_PTR) trgLine)[3] = srcLine[12] | (srcLine[13] << 8) | (srcLine[14] << 16) | (srcLine[15] << 24);
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
            }
        }
    }


    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                /* 32 bpp to 32 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) srcLine)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) srcLine)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) srcLine)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) srcLine)[3];
            }
            else
            {
                /* 32 bpp to 32 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[ 0] = srcLine[ 0] | (srcLine[ 1] << 8) | (srcLine[ 2] << 16) | (srcLine[ 3] << 24);
                ((gctUINT32_PTR) trgLine)[ 1] = srcLine[ 4] | (srcLine[ 5] << 8) | (srcLine[ 6] << 16) | (srcLine[ 7] << 24);
                ((gctUINT32_PTR) trgLine)[ 2] = srcLine[ 8] | (srcLine[ 9] << 8) | (srcLine[10] << 16) | (srcLine[11] << 24);
                ((gctUINT32_PTR) trgLine)[ 3] = srcLine[12] | (srcLine[13] << 8) | (srcLine[14] << 16) | (srcLine[15] << 24);
            }
        }
    }
}

static void
_UploadSuperTiled32bppto32bppBE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT8_PTR srcLine;
    gctUINT8_PTR trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 4);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            /* 32bpp to 32bpp. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] << 24) | (srcLine[1] << 16) | (srcLine[2] << 8) | srcLine[3];
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: one tile row. */
                if ((((gctUINTPTR_T) srcLine & 3) == 0) && ((SourceStride & 3) == 0))
                {
                    /* Special optimization for aligned source. */
                    ((gctUINT32_PTR) trgLine)[0] = ((gctUINT32_PTR) srcLine)[0];
                    ((gctUINT32_PTR) trgLine)[1] = ((gctUINT32_PTR) srcLine)[1];
                    ((gctUINT32_PTR) trgLine)[2] = ((gctUINT32_PTR) srcLine)[2];
                    ((gctUINT32_PTR) trgLine)[3] = ((gctUINT32_PTR) srcLine)[3];
                }
                else
                {
                    ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
                    ((gctUINT32_PTR) trgLine)[1] = (srcLine[ 4] << 24) | (srcLine[ 5] << 16) | (srcLine[ 6] << 8) | srcLine[ 7];
                    ((gctUINT32_PTR) trgLine)[2] = (srcLine[ 8] << 24) | (srcLine[ 9] << 16) | (srcLine[10] << 8) | srcLine[11];
                    ((gctUINT32_PTR) trgLine)[3] = (srcLine[12] << 24) | (srcLine[13] << 16) | (srcLine[14] << 8) | srcLine[15];
                }
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
                srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

                /* 32 bpp to 32 bpp: part tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
            }
        }
    }


    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 4;
            srcLine = (gctUINT8_PTR) Memory  + y  * SourceStride + x  * 4;

            if ((((gctUINTPTR_T)srcLine & 3) == 0) && ((SourceStride & 3) == 0))
            {
                /* Special optimization for aligned source. */
                /* 32 bpp to 32 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[ 0] = ((gctUINT32_PTR) srcLine)[0];
                ((gctUINT32_PTR) trgLine)[ 1] = ((gctUINT32_PTR) srcLine)[1];
                ((gctUINT32_PTR) trgLine)[ 2] = ((gctUINT32_PTR) srcLine)[2];
                ((gctUINT32_PTR) trgLine)[ 3] = ((gctUINT32_PTR) srcLine)[3];
            }
            else
            {
                /* 32 bpp to 32 bpp: one tile. */
                ((gctUINT32_PTR) trgLine)[ 0] = (srcLine[ 0] << 24) | (srcLine[ 1] << 16) | (srcLine[ 2] << 8) | srcLine[ 3];
                ((gctUINT32_PTR) trgLine)[ 1] = (srcLine[ 4] << 24) | (srcLine[ 5] << 16) | (srcLine[ 6] << 8) | srcLine[ 7];
                ((gctUINT32_PTR) trgLine)[ 2] = (srcLine[ 8] << 24) | (srcLine[ 9] << 16) | (srcLine[10] << 8) | srcLine[11];
                ((gctUINT32_PTR) trgLine)[ 3] = (srcLine[12] << 24) | (srcLine[13] << 16) | (srcLine[14] << 8) | srcLine[15];
            }
        }
    }
}

static void
_UploadSuperTiledRGBA4444toARGB4444(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB4444. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12) | ((srcLine[1] & 0xFFF0) << 12) | ((srcLine[1] & 0xF) << 28) ;
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 4) | ((srcLine[2] & 0xF) << 12) | ((srcLine[3] & 0xFFF0) << 12) | ((srcLine[3] & 0xF) << 28) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB4444: one tile. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12) | ((srcLine[1] & 0xFFF0) << 12) | ((srcLine[1] & 0xF) << 28) ;
            ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 4) | ((srcLine[2] & 0xF) << 12) | ((srcLine[3] & 0xFFF0) << 12) | ((srcLine[3] & 0xF) << 28) ;
        }
    }
}

static void
_UploadSuperTiledRGBA4444toARGB4444BE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA4444 to ARGB4444. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFF0) << 12) | ((srcLine[0] & 0xF) << 28) | (srcLine[1] >> 4) | ((srcLine[1] & 0xF) << 12) ;
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFF0) << 12) | ((srcLine[2] & 0xF) << 28) | (srcLine[3] >> 4) | ((srcLine[3] & 0xF) << 12) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA4444 to ARGB4444: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 4) | ((srcLine[0] & 0xF) << 12);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);


            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFF0) << 12) | ((srcLine[0] & 0xF) << 28) | (srcLine[1] >> 4) | ((srcLine[1] & 0xF) << 12) ;
            ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFF0) << 12) | ((srcLine[2] & 0xF) << 28) | (srcLine[3] >> 4) | ((srcLine[3] & 0xF) << 12) ;
        }
    }
}

static void
_UploadSuperTiledRGBA5551toARGB1555(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
             xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15) | ((srcLine[1] & 0xFFFE) << 15) | ((srcLine[1] & 0x1) << 31);
                ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 1) | ((srcLine[2] & 0x1) << 15) | ((srcLine[3] & 0xFFFE) << 15) | ((srcLine[3] & 0x1) << 31) ;
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: part tile row. */
                ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555: one tile. */
            ((gctUINT32_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15) | ((srcLine[1] & 0xFFFE) << 15) | ((srcLine[1] & 0x1) << 31);
            ((gctUINT32_PTR) trgLine)[1] = (srcLine[2] >> 1) | ((srcLine[2] & 0x1) << 15) | ((srcLine[3] & 0xFFFE) << 15) | ((srcLine[3] & 0x1) << 31);
        }
    }
}

static void
_UploadSuperTiledRGBA5551toARGB1555BE(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT16_PTR srcLine;
    gctUINT8_PTR  trgLine;

    /* Move linear Memory to [0,0] origin. */
    Memory = (gctPOINTER) ((gctUINT8_PTR) Memory - SourceStride * Y - X * 2);

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;
    xt = 0;
    yt = 0;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];
             xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: one tile row. */
                ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFFE) << 15) | ((srcLine[0] & 0x1) << 31) | (srcLine[1] >> 1) | ((srcLine[1] & 0x1) << 15);
                ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFFE) << 15) | ((srcLine[2] & 0x1) << 31) | (srcLine[3] >> 1) | ((srcLine[3] & 0x1) << 15);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];
                xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
                yt = gcmSUPERTILE_OFFSET_Y(x, y);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

                /* RGBA5551 to ARGB1555: part tile row. */
            ((gctUINT16_PTR) trgLine)[0] = (srcLine[0] >> 1) | ((srcLine[0] & 0x1) << 15);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y ++)
    {
        for (x = X; x < Right; x += 4)
        {
            xt = gcmSUPERTILE_OFFSET_X(x, y, Hardware->config->superTileMode);
            yt = gcmSUPERTILE_OFFSET_Y(x, y);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            srcLine = (gctUINT16_PTR) ((gctUINT8_PTR) Memory  + y  * SourceStride + x * 2);

            /* RGBA5551 to ARGB1555: one tile row. */
            ((gctUINT32_PTR) trgLine)[0] = ((srcLine[0] & 0xFFFE) << 15) | ((srcLine[0] & 0x1) << 31) | (srcLine[1] >> 1) | ((srcLine[1] & 0x1) << 15);
            ((gctUINT32_PTR) trgLine)[1] = ((srcLine[2] & 0xFFFE) << 15) | ((srcLine[2] & 0x1) << 31) | (srcLine[3] >> 1) | ((srcLine[3] & 0x1) << 15);
        }
    }
}
#endif

static gceSTATUS
_UploadSuperTiled(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT32 Offset,
    IN gctUINT XOffset,
    IN gctUINT YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    )
{
    gctUINT8_PTR target                   = gcvNULL;
    gctUINT8_PTR source                   = gcvNULL;
    gctUINT32 offset                      = 0;
    gctUINT32 srcBytesPerPixel            = 0;
    gctUINT32 trgBytesPerPixel            = 0;
    gctUINT x                             = 0;
    gctUINT y                             = 0;
    gceSTATUS status;
    gcsSURF_FORMAT_INFO_PTR srcFormatInfo = gcvNULL;

    gcoHARDWARE Hardware                  = gcvNULL;
    gctUINT trgStride                     = 0;
    gctPOINTER trgLogical                 = gcvNULL;
    gctUINT edgeX[6];
    gctUINT edgeY[6];
    gctUINT countX = 0;
    gctUINT countY = 0;
    gctUINT right  = XOffset + Width;
    gctUINT bottom = YOffset + Height;

    gcmGETHARDWARE(Hardware);

    target = TargetInfo->node.logical + Offset;

    trgStride  = TargetInfo->stride;
    trgLogical = TargetInfo->node.logical;

    /* Compute dest logical. */
    trgLogical = (gctPOINTER) ((gctUINT8_PTR) trgLogical + Offset);

    /* Compute edge coordinates. */
    if (Width < 4)
    {
        for (x = XOffset; x < right; x++)
        {
           edgeX[countX++] = x;
        }
    }
    else
    {
        for (x = XOffset; x < ((XOffset + 3) & ~3); x++)
        {
            edgeX[countX++] = x;
        }

        for (x = (right & ~3); x < right; x++)
        {
            edgeX[countX++] = x;
        }
    }

    if (Height < 4)
    {
       for (y = YOffset; y < bottom; y++)
       {
           edgeY[countY++] = y;
       }
    }
    else
    {
        for (y = YOffset; y < ((YOffset + 3) & ~3); y++)
        {
           edgeY[countY++] = y;
        }

        for (y = (bottom & ~3); y < bottom; y++)
        {
           edgeY[countY++] = y;
        }
    }

#if !defined(UNDER_CE)
    /* Fast path(s) for all common OpenGL ES formats. */
    if ((TargetInfo->format == gcvSURF_A8R8G8B8)
    ||  (TargetInfo->format == gcvSURF_X8R8G8B8)
    )
    {
        switch (SourceFormat)
        {
        case gcvSURF_A8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledA8toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);

            }
            else
            {
                _UploadSuperTiledA8toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_B8G8R8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledBGRtoARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledBGRtoARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_A8B8G8R8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledABGRtoARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 4
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledABGRtoARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_L8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledL8toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledL8toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_A8L8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledA8L8toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledA8L8toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_A8R8G8B8:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiled32bppto32bppBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 4
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiled32bppto32bpp(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 4
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_R5G6B5:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledRGB565toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledRGB565toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_R4G4B4A4:
            if (Hardware->bigEndian)
            {
                 _UploadSuperTiledRGBA4444toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                 _UploadSuperTiledRGBA4444toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        case gcvSURF_R5G5B5A1:
            if (Hardware->bigEndian)
            {
                _UploadSuperTiledRGBA5551toARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            else
            {
                _UploadSuperTiledRGBA5551toARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
            }
            return gcvSTATUS_OK;

        default:
            break;
        }
    }

    else
    {
        switch (SourceFormat)
        {
        case gcvSURF_A8:
            if (TargetInfo->format == gcvSURF_A8)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled8bppto8bppBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiled8bppto8bpp(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_B8G8R8:
            if (TargetInfo->format == gcvSURF_X8R8G8B8)
            {
               /* Same as BGR to ARGB. */
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiledBGRtoARGBBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiledBGRtoARGB(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            else if (TargetInfo->format == gcvSURF_X8B8G8R8)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiledBGRtoABGRBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiledBGRtoABGR(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 3
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }

            break;

        /* case gcvSURF_A8B8G8R8: */

        case gcvSURF_L8:
            if (TargetInfo->format == gcvSURF_L8)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled8bppto8bppBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiled8bppto8bpp(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_A8L8:
            if (TargetInfo->format == gcvSURF_A8L8)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled16bppto16bppBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiled16bppto16bpp(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        /* case gcvSURF_A8R8G8B8: */

        case gcvSURF_R5G6B5:
            if (TargetInfo->format == gcvSURF_R5G6B5)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled16bppto16bppBE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiled16bppto16bpp(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_R4G4B4A4:
            if (TargetInfo->format == gcvSURF_A4R4G4B4)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiledRGBA4444toARGB4444BE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiledRGBA4444toARGB4444(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        case gcvSURF_R5G5B5A1:
            if (TargetInfo->format == gcvSURF_A1R5G5B5)
            {
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiledRGBA5551toARGB1555BE(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                else
                {
                    _UploadSuperTiledRGBA5551toARGB1555(Hardware,
                                      trgLogical,
                                      trgStride,
                                      XOffset, YOffset,
                                      right, bottom,
                                      edgeX, edgeY,
                                      countX, countY,
                                      Memory,
                                      (SourceStride == 0)
                                      ? (gctINT) Width * 2
                                      : SourceStride);
                }
                return gcvSTATUS_OK;
            }
            break;

        default:
            break;
        }
    }

    /* More fast path(s) for same source/dest format. */
    if (SourceFormat == TargetInfo->format)
    {
        gcsSURF_FORMAT_INFO_PTR formatInfo;
        gcmONERROR(gcoHARDWARE_QueryFormat(SourceFormat, &formatInfo));

        if ((formatInfo->fmtClass == gcvFORMAT_CLASS_RGBA) &&
            (formatInfo->layers == 1))
        {
            switch (formatInfo->bitsPerPixel)
            {
            case 8:
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled8bppto8bppBE(Hardware,
                                                  trgLogical,
                                                  trgStride,
                                                  XOffset, YOffset,
                                                  right, bottom,
                                                  edgeX, edgeY,
                                                  countX, countY,
                                                  Memory,
                                                  (SourceStride == 0)
                                                  ? (gctINT) Width
                                                  : SourceStride);
                }
                else
                {
                    _UploadSuperTiled8bppto8bpp(Hardware,
                                                trgLogical,
                                                trgStride,
                                                XOffset, YOffset,
                                                right, bottom,
                                                edgeX, edgeY,
                                                countX, countY,
                                                Memory,
                                                (SourceStride == 0)
                                                ? (gctINT) Width
                                                : SourceStride);
                }
                return gcvSTATUS_OK;

            case 16:
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled16bppto16bppBE(Hardware,
                                                    trgLogical,
                                                    trgStride,
                                                    XOffset, YOffset,
                                                    right, bottom,
                                                    edgeX, edgeY,
                                                    countX, countY,
                                                    Memory,
                                                    (SourceStride == 0)
                                                    ? (gctINT) Width * 2
                                                    : SourceStride);
                }
                else
                {
                    _UploadSuperTiled16bppto16bpp(Hardware,
                                                  trgLogical,
                                                  trgStride,
                                                  XOffset, YOffset,
                                                  right, bottom,
                                                  edgeX, edgeY,
                                                  countX, countY,
                                                  Memory,
                                                  (SourceStride == 0)
                                                  ? (gctINT) Width * 2
                                                  : SourceStride);
                }
                return gcvSTATUS_OK;

            case 32:
                if (Hardware->bigEndian)
                {
                    _UploadSuperTiled32bppto32bppBE(Hardware,
                                                    trgLogical,
                                                    trgStride,
                                                    XOffset, YOffset,
                                                    right, bottom,
                                                    edgeX, edgeY,
                                                    countX, countY,
                                                    Memory,
                                                    (SourceStride == 0)
                                                    ? (gctINT) Width * 4
                                                    : SourceStride);
                }
                else
                {
                    _UploadSuperTiled32bppto32bpp(Hardware,
                                                  trgLogical,
                                                  trgStride,
                                                  XOffset, YOffset,
                                                  right, bottom,
                                                  edgeX, edgeY,
                                                  countX, countY,
                                                  Memory,
                                                  (SourceStride == 0)
                                                  ? (gctINT) Width * 4
                                                  : SourceStride);
                }
                return gcvSTATUS_OK;

            case 64:
            default:
                break;
            }
        }
    }

#endif
    /* Get number of bytes per pixel and tile for the format. */
    gcmONERROR(gcoHARDWARE_ConvertFormat(SourceFormat,
                                         &srcBytesPerPixel,
                                         gcvNULL));
    srcBytesPerPixel /= 8;

    trgBytesPerPixel = TargetInfo->bitsPerPixel / 8;

    /* Auto-stride. */
    if (SourceStride == 0)
    {
        SourceStride = srcBytesPerPixel * Width;
    }

    /* Query format specifics. */
    gcmONERROR(gcoSURF_QueryFormat(SourceFormat, &srcFormatInfo));

    if (TargetInfo->formatInfo.layers > 1)
    {
        if ((TargetInfo->formatInfo.layers == 2)
            && (srcBytesPerPixel == 12) && (trgBytesPerPixel == 16))
        {
            _UploadSuperTiledBGR32ToABGR32_2Layer(TargetInfo,
                                                   target,
                                                   XOffset,
                                                   YOffset,
                                                   Width,
                                                   Height,
                                                   (gctUINT8_PTR)Memory,
                                                   SourceStride,
                                                   SourceFormat);
        }
        else if ((TargetInfo->formatInfo.layers == 2)
            && (srcBytesPerPixel == 16) && (trgBytesPerPixel == 16))
        {
            _UploadSuperTiledABGR32ToABGR32_2Layer(TargetInfo,
                                                   target,
                                                   XOffset,
                                                   YOffset,
                                                   Width,
                                                   Height,
                                                   (gctUINT8_PTR)Memory,
                                                   SourceStride,
                                                   SourceFormat);
        }
        else
        {
            /* TODO. */
            gcmASSERT(0);
        }
    }
    else
    {
        for (y = YOffset; y < Height + YOffset; y++)
        {
            source = (gctUINT8_PTR)Memory + (y - YOffset) * SourceStride;

            for (x = XOffset; x < Width + XOffset; x++)
            {
                gcoHARDWARE_ComputeOffset(gcvNULL,
                                          x,
                                          y,
                                          TargetInfo->stride,
                                          trgBytesPerPixel,
                                          TargetInfo->tiling,
                                          &offset);

                /* Copy the pixel. */
                /* Convert the current pixel. */
                gcmONERROR(gcoHARDWARE_ConvertPixel(source,
                                                    (target + offset),
                                                    0, 0,
                                                    srcFormatInfo,
                                                    &TargetInfo->formatInfo,
                                                    gcvNULL, gcvNULL,
                                                    0, 0));

                source += srcBytesPerPixel;
            }
        }
    }

    return gcvSTATUS_OK;
OnError:
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_UploadTexture
**
**  Upload one slice into a texture.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gceSURF_FORMAT TargetFormat
**          Destination texture format.
**
**      gctUINT32 Address
**          Hardware specific base address for mip map.
**
**      gctPOINTER Logical
**          Logical base address for mip map.
**
**      gctUINT32 Offset
**          Offset into mip map to upload.
**
**      gctINT TargetStride
**          Stride of the destination texture.
**
**      gctUINT X
**          X origin of data to upload.
**
**      gctUINT Y
**          Y origin of data to upload.
**
**      gctUINT Width
**          Width of texture to upload.
**
**      gctUINT Height
**          Height of texture to upload.
**
**      gctCONST_POINTER Memory
**          Pointer to user buffer containing the data to upload.
**
**      gctINT SourceStride
**          Stride of user buffer.
**
**      gceSURF_FORMAT SourceFormat
**          Format of the source texture to upload.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_UploadTexture(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT32 Offset,
    IN gctUINT XOffset,
    IN gctUINT YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Target=%x  Offset=%u "
                  "X=%u Y=%u Width=%u Height=%u "
                  "Memory=0x%x SourceStride=%d SourceFormat=%d",
                  TargetInfo, Offset, XOffset, YOffset, Width, Height,
                  Memory, SourceStride, SourceFormat);

    if (!TargetInfo->tileStatusDisabled &&
         TargetInfo->tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Disable tile status buffer in case it's there */
        gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, TargetInfo, gcvTRUE));
        gcmONERROR(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));
        gcmONERROR(gcoHARDWARE_Commit(gcvNULL));
        gcmONERROR(gcoHARDWARE_Stall(gcvNULL));
    }

    if (TargetInfo->superTiled && ((SourceFormat & gcvSURF_FORMAT_OCL) == 0))
    {
        gcmONERROR(_UploadSuperTiled(TargetInfo, Offset, XOffset, YOffset,
                                     Width, Height, Memory, SourceStride, SourceFormat));
    }
    else
    {
        gcmONERROR(_UploadTextureTiled(gcvNULL, TargetInfo, Offset, XOffset, YOffset,
                                       Width, Height, Memory, SourceStride, SourceFormat));
    }

    /* If dst surface was fully overwritten, reset the garbagePadded flag. */
    if (gcmIS_SUCCESS(status) && TargetInfo->paddingFormat &&
        TargetInfo->rect.left == 0 && (gctINT32)Width >= TargetInfo->rect.right / TargetInfo->samples.x &&
        TargetInfo->rect.top == 0 && (gctINT32)Height >= TargetInfo->rect.bottom / TargetInfo->samples.y)
    {
        TargetInfo->garbagePadded = gcvFALSE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static void
_UploadNV16toYUY2(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctPOINTER Memory[3],
    IN gctINT SourceStride[3]
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT yStride, uvStride;
    gctUINT8_PTR yPlane  = gcvNULL;
    gctUINT8_PTR uvPlane = gcvNULL;
    gctUINT8_PTR yLine   = gcvNULL;
    gctUINT8_PTR uvLine  = gcvNULL;
    gctUINT8_PTR trgLine = gcvNULL;

    if (SourceStride != 0)
    {
        yStride  = SourceStride[0];
        uvStride = SourceStride[1];
    }
    else
    {
        yStride  = Right - X;
        uvStride = Right - X;
    }

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Move linear Memory to [0,0] origin. */
    yPlane  = (gctUINT8_PTR) Memory[0] -  yStride * Y - X * 1;
    uvPlane = (gctUINT8_PTR) Memory[1] - uvStride * Y - X * 1;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            /* Skip odd x. */
            if (x & 1) continue;

            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            yLine   = yPlane  + y * yStride  + x * 1;
            uvLine  = uvPlane + y * uvStride + x * 1;

            /* NV16 to YUY2: a package - 2 pixels. */
            ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[0] << 8) | (yLine[1] << 16) | (uvLine[1] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                yLine   = yPlane  + y * yStride  + x * 1;
                uvLine  = uvPlane + y * uvStride + x * 1;

                /* NV16 to YUY2: one tile row - 2 packages - 4 pixels. */
                ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[0] << 8) | (yLine[1] << 16) | (uvLine[1] << 24);
                ((gctUINT32_PTR) trgLine)[1] =  (yLine[2]) | (uvLine[2] << 8) | (yLine[3] << 16) | (uvLine[3] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                /* Skip odd x. */
                if (x & 1) continue;

                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                yLine   = yPlane  + y * yStride  + x * 1;
                uvLine  = uvPlane + y * uvStride + x * 1;

                ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[0] << 8) | (yLine[1] << 16) | (uvLine[1] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        yLine   = yPlane  + y * yStride  + X * 1;
        uvLine  = uvPlane + y * uvStride + X * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR srcY, srcUV;

            /* NV16 to YUY2: one block - 4 pixels. */
            srcY  = yLine;
            srcUV = uvLine;

            ((gctUINT32_PTR) trgLine)[0] =  (srcY[0]) | (srcUV[0] << 8) | (srcY[1] << 16) | (srcUV[1] << 24);
            ((gctUINT32_PTR) trgLine)[1] =  (srcY[2]) | (srcUV[2] << 8) | (srcY[3] << 16) | (srcUV[3] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[2] =  (srcY[0]) | (srcUV[0] << 8) | (srcY[1] << 16) | (srcUV[1] << 24);
            ((gctUINT32_PTR) trgLine)[3] =  (srcY[2]) | (srcUV[2] << 8) | (srcY[3] << 16) | (srcUV[3] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[4] =  (srcY[0]) | (srcUV[0] << 8) | (srcY[1] << 16) | (srcUV[1] << 24);
            ((gctUINT32_PTR) trgLine)[5] =  (srcY[2]) | (srcUV[2] << 8) | (srcY[3] << 16) | (srcUV[3] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[6] =  (srcY[0]) | (srcUV[0] << 8) | (srcY[1] << 16) | (srcUV[1] << 24);
            ((gctUINT32_PTR) trgLine)[7] =  (srcY[2]) | (srcUV[2] << 8) | (srcY[3] << 16) | (srcUV[3] << 24);

            /* Move to next tile. */
            trgLine += 16 * 2;
            yLine   +=  4 * 1;
            uvLine  +=  4 * 1;
        }
    }
}

static void
_UploadNV61toYUY2(
    IN gctPOINTER Logical,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Right,
    IN gctUINT Bottom,
    IN gctUINT EdgeX[],
    IN gctUINT EdgeY[],
    IN gctUINT CountX,
    IN gctUINT CountY,
    IN gctPOINTER Memory[3],
    IN gctINT SourceStride[3]
    )
{
    gctUINT x, y;
    gctUINT i, j;
    gctUINT xt, yt;
    gctUINT yStride, uvStride;
    gctUINT8_PTR yPlane  = gcvNULL;
    gctUINT8_PTR uvPlane = gcvNULL;
    gctUINT8_PTR yLine   = gcvNULL;
    gctUINT8_PTR uvLine  = gcvNULL;
    gctUINT8_PTR trgLine = gcvNULL;

    if (SourceStride != 0)
    {
        yStride  = SourceStride[0];
        uvStride = SourceStride[1];
    }
    else
    {
        yStride  = Right - X;
        uvStride = Right - X;
    }

    /* Compute aligned area. */
    X       = (X + 3) & ~0x03;
    Y       = (Y + 3) & ~0x03;
    Right  &= ~0x03;
    Bottom &= ~0x03;

    /* Move linear Memory to [0,0] origin. */
    yPlane  = (gctUINT8_PTR) Memory[0] -  yStride * Y - X * 1;
    uvPlane = (gctUINT8_PTR) Memory[1] - uvStride * Y - X * 1;

    /* Y edge - X edge. */
    for (j = 0; j < CountY; j++)
    {
        y  = EdgeY[j];
        yt = y & ~0x03;

        for (i = 0; i < CountX; i++)
        {
            x  = EdgeX[i];

            /* Skip odd x. */
            if (x & 1) continue;

            xt = ((x &  0x03) << 0) +
                 ((y &  0x03) << 2) +
                 ((x & ~0x03) << 2);

            trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
            yLine   = yPlane  + y * yStride  + x * 1;
            uvLine  = uvPlane + y * uvStride + x * 1;

            /* NV61 to YUY2: a package - 2 pixels. */
            ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[1] << 8) | (yLine[1] << 16) | (uvLine[0] << 24);
        }
    }

    if (CountY)
    {
        /* Y edge - X middle. */
        for (x = X; x < Right; x += 4)
        {
            for (j = 0; j < CountY; j++)
            {
                y  = EdgeY[j];

                xt = ((y & 0x03) << 2) + (x << 2);
                yt = y & ~0x03;

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                yLine   = yPlane  + y * yStride  + x * 1;
                uvLine  = uvPlane + y * uvStride + x * 1;

                /* NV61 to YUY2: one tile row - 2 packages - 4 pixels. */
                ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[1] << 8) | (yLine[1] << 16) | (uvLine[0] << 24);
                ((gctUINT32_PTR) trgLine)[1] =  (yLine[2]) | (uvLine[3] << 8) | (yLine[3] << 16) | (uvLine[2] << 24);
            }
        }
    }

    if (CountX)
    {
        /* Y middle - X edge. */
        for (y = Y; y < Bottom; y++)
        {
            yt = y & ~0x03;

            for (i = 0; i < CountX; i++)
            {
                x  = EdgeX[i];

                /* Skip odd x. */
                if (x & 1) continue;

                xt = ((x &  0x03) << 0) +
                     ((y &  0x03) << 2) +
                     ((x & ~0x03) << 2);

                trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
                yLine   = yPlane  + y * yStride  + x * 1;
                uvLine  = uvPlane + y * uvStride + x * 1;

                ((gctUINT32_PTR) trgLine)[0] =  (yLine[0]) | (uvLine[1] << 8) | (yLine[1] << 16) | (uvLine[0] << 24);
            }
        }
    }

    /* Y middle - X middle. */
    for (y = Y; y < Bottom; y += 4)
    {
        xt = X << 2;
        yt = y;

        trgLine = (gctUINT8_PTR) Logical + yt * TargetStride + xt * 2;
        yLine   = yPlane  + y * yStride  + X * 1;
        uvLine  = uvPlane + y * uvStride + X * 1;

        for (x = X; x < Right; x += 4)
        {
            gctUINT8_PTR srcY, srcUV;

            /* NV61 to YUY2: one block - 4 pixels. */
            srcY  = yLine;
            srcUV = uvLine;

            ((gctUINT32_PTR) trgLine)[0] =  (srcY[0]) | (srcUV[1] << 8) | (srcY[1] << 16) | (srcUV[0] << 24);
            ((gctUINT32_PTR) trgLine)[1] =  (srcY[2]) | (srcUV[3] << 8) | (srcY[3] << 16) | (srcUV[2] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[2] =  (srcY[0]) | (srcUV[1] << 8) | (srcY[1] << 16) | (srcUV[0] << 24);
            ((gctUINT32_PTR) trgLine)[3] =  (srcY[2]) | (srcUV[3] << 8) | (srcY[3] << 16) | (srcUV[2] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[4] =  (srcY[0]) | (srcUV[1] << 8) | (srcY[1] << 16) | (srcUV[0] << 24);
            ((gctUINT32_PTR) trgLine)[5] =  (srcY[2]) | (srcUV[3] << 8) | (srcY[3] << 16) | (srcUV[2] << 24);

            srcY  += yStride;
            srcUV += uvStride;
            ((gctUINT32_PTR) trgLine)[6] =  (srcY[0]) | (srcUV[1] << 8) | (srcY[1] << 16) | (srcUV[0] << 24);
            ((gctUINT32_PTR) trgLine)[7] =  (srcY[2]) | (srcUV[3] << 8) | (srcY[3] << 16) | (srcUV[2] << 24);

            /* Move to next tile. */
            trgLine += 16 * 2;
            yLine   +=  4 * 1;
            uvLine  +=  4 * 1;
        }
    }
}

/*******************************************************************************
**
**  gcoHARDWARE_UploadTextureYUV
**
**  Upload one slice into a texture.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gceSURF_FORMAT TargetFormat
**          Destination texture format. MUST be YUY2.
**
**      gctUINT32 Address
**          Hardware specific base address for mip map.
**
**      gctPOINTER Logical
**          Logical base address for mip map.
**
**      gctUINT32 Offset
**          Offset into mip map to upload.
**
**      gctINT TargetStride
**          Stride of the destination texture.
**
**      gctUINT X
**          X origin of data to upload.
**
**      gctUINT Y
**          Y origin of data to upload.
**
**      gctUINT Width
**          Width of texture to upload.
**
**      gctUINT Height
**          Height of texture to upload.
**
**      gctCONST_POINTER Memory[3]
**          Pointer to user buffer containing the data to upload.
**
**      gctINT SourceStride[3]
**          Stride of user buffer.
**
**      gceSURF_FORMAT SourceFormat
**          Format of the source texture to upload.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_UploadTextureYUV(
    IN gceSURF_FORMAT TargetFormat,
    IN gctUINT32 Address,
    IN gctPOINTER Logical,
    IN gctUINT32 Offset,
    IN gctINT TargetStride,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctPOINTER Memory[3],
    IN gctINT SourceStride[3],
    IN gceSURF_FORMAT SourceFormat
    )
{
    gceSTATUS status;
    gcoHARDWARE Hardware = gcvNULL;
    gctUINT x, y;
    gctUINT edgeX[6];
    gctUINT edgeY[6];
    gctUINT countX = 0;
    gctUINT countY = 0;
    gctUINT right  = X + Width;
    gctUINT bottom = Y + Height;

    gcmHEADER_ARG("TargetFormat=%d Address=%08x Logical=0x%x "
                  "Offset=%u TargetStride=%d X=%u Y=%u Width=%u Height=%u "
                  "Memory=0x%x SourceStride=%d SourceFormat=%d",
                  TargetFormat, Address, Logical, Offset,
                  TargetStride, X, Y, Width, Height, Memory, SourceStride,
                  SourceFormat);

    gcmGETHARDWARE(Hardware);

    (void) TargetFormat;

    /* Compute dest logical. */
    Logical = (gctPOINTER) ((gctUINT8_PTR) Logical + Offset);

    /* Compute edge coordinates. */
    if (Width < 4)
    {
        for (x = X; x < right; x++)
        {
           edgeX[countX++] = x;
        }
    }
    else
    {
        for (x = X; x < ((X + 3) & ~3); x++)
        {
            edgeX[countX++] = x;
        }

        for (x = (right & ~3); x < right; x++)
        {
            edgeX[countX++] = x;
        }
    }

    if (Height < 4)
    {
       for (y = Y; y < bottom; y++)
       {
           edgeY[countY++] = y;
       }
    }
    else
    {
        for (y = Y; y < ((Y + 3) & ~3); y++)
        {
           edgeY[countY++] = y;
        }

        for (y = (bottom & ~3); y < bottom; y++)
        {
           edgeY[countY++] = y;
        }
    }

    switch (SourceFormat)
    {
    case gcvSURF_NV16:
        if (Hardware->bigEndian)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        else
        {
            _UploadNV16toYUY2(Logical,
                            TargetStride,
                            X, Y,
                            right, bottom,
                            edgeX, edgeY,
                            countX, countY,
                            Memory,
                            SourceStride);
        }
        gcmFOOTER_NO();
        return gcvSTATUS_OK;

    case gcvSURF_NV61:
        if (Hardware->bigEndian)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        else
        {
            _UploadNV61toYUY2(Logical,
                            TargetStride,
                            X, Y,
                            right, bottom,
                            edgeX, edgeY,
                            countX, countY,
                            Memory,
                            SourceStride);
        }
        gcmFOOTER_NO();
        return gcvSTATUS_OK;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        break;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gctUINT
_ComputeBlockOffset(
    gctUINT X,
    gctUINT Y
    )
{
    /* Compute offset in blocks, in a supertiled target, for source x,y.
       Target is compressed 4x4 blocks. */
    gctUINT x = X >> 2;
    gctUINT y = Y >> 2;

    return (x & 0x1)
         + ((y & 0x1) << 1)
         + ((x & 0x2) << 1)
         + ((y & 0x2) << 2)
         + ((x & 0x4) << 2)
         + ((y & 0x4) << 3)
         + ((x & 0x8) << 3)
         + ((y & 0x8) << 4);
}

/* Assuming 4x4 block size. */
static gceSTATUS
_UploadCompressedSuperTiled(
    gctCONST_POINTER Target,
    gctCONST_POINTER Source,
    IN gctUINT32 TrgWidth,
    IN gctUINT32 BytesPerTile,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT XOffset,
    IN gctUINT YOffset
    )
{
    gctUINT32_PTR source = gcvNULL;
    gctUINT32 sourceStride, targetStride;
    gctUINT32 offset, rowOffset;
    gctUINT x, y;

    /* 4 bits/pixel. */
    sourceStride = gcmALIGN(Width, 4) * BytesPerTile / 16;

    targetStride = gcmALIGN(TrgWidth, 64) * BytesPerTile / 16;

    gcmASSERT(sourceStride != 0);
    gcmASSERT(targetStride != 0);

    for (y = YOffset; y < Height + YOffset; y+=4)
    {
        rowOffset = (y & ~63) * targetStride;
        source = (gctUINT32_PTR)((gctUINT8_PTR) Source + (y - YOffset) * sourceStride);

        for (x = XOffset; x < Width + XOffset; x+=4)
        {
            offset = _ComputeBlockOffset(x & 63, y & 63) * BytesPerTile;

            /* Move to this supertile row start. */
            offset += rowOffset;

            /* Move to this supertile. */
            offset += (x & ~63) * (4 * BytesPerTile);

            /* Copy the 4x4 compressed block. */
            gcoOS_MemCopy(
                (gctPOINTER)((gctUINT8_PTR)Target + offset),
                source,
                BytesPerTile);
            source += BytesPerTile / 4;
        }
    }

    return gcvSTATUS_OK;
}

void _ConvertETC2(
    gctCONST_POINTER Target,
    gctCONST_POINTER Source,
    IN gctUINT32 BytesPerTile,
    IN gctBOOL PunchThrough
    )
{
    gctUINT8_PTR source = (gctUINT8_PTR)Source;
    gctUINT8_PTR target = (gctUINT8_PTR)Target;
    gctUINT8 index;
    gctINT8  baseColR1, baseColR2d;
    static gctUINT8 lookupTable[16] = { 0x4, 0x4, 0x4, 0x4,
                                        0x4, 0x4, 0x4, 0xE0,
                                        0x4, 0x4, 0xE0, 0xE0,
                                        0x4, 0xE0, 0xE0, 0xE0 };

    gcmASSERT(BytesPerTile == 8);

    /* Skip individual mode. */
    if (PunchThrough || (source[3] & 0x2))
    {
        /* Detect T-mode in big-endian. */
        baseColR1  = source[0] >> 3;
        baseColR2d = source[0] & 0x7;
        baseColR2d = (baseColR2d << 5);

        /* Sign extension. */
        baseColR2d = (baseColR2d >> 5);
        baseColR1 += baseColR2d;

        if (baseColR1 & 0x20)
        {
            /* Swap C0/C1 from below while copying source to target,
               and fill in the // bits with lookup table bit so as
               to ensure T mode decoding.

              63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32
              -----------------------------------------------------------------------------------------------
              // // //|R0a  |//|R0b  |G0         |B0         |R1         |G1         |B1          |da  |df|db|
            */

            index = source[2] >> 4;

            target[0] = lookupTable[index]
                      | ((index & 0x0C) << 1)
                      | ((index & 0x03) << 0);

            target[1] = ((source[2] & 0x0F) << 4)
                      | ((source[3] & 0xF0) >> 4);

            target[2] = ((source[0] & 0x18) << 3)
                      | ((source[0] & 0x03) << 4)
                      | ((source[1] & 0xF0) >> 4);

            target[3] = ((source[1] & 0x0F) << 4)
                      |  (source[3] & 0x0F);


            /* Detect T-mode in big-endian. */
            baseColR1  = target[0] >> 3;
            baseColR2d = target[0] & 0x7;
            baseColR2d = (baseColR2d << 5);

            /* Sign extension. */
            baseColR2d = (baseColR2d >> 5);
            baseColR1 += baseColR2d;
            if (!(baseColR1 & 0x20))
            {
                /* BUG. */
                baseColR1 = 0;
            }
        }
        else
        {
            target[0] = source[0];
            target[1] = source[1];
            target[2] = source[2];
            target[3] = source[3];
        }
    }
    else
    {
        target[0] = source[0];
        target[1] = source[1];
        target[2] = source[2];
        target[3] = source[3];
    }

    target[4] = source[4];
    target[5] = source[5];
    target[6] = source[6];
    target[7] = source[7];
}

static gceSTATUS
_UploadCompressedSuperTiledETC2(
    IN gctCONST_POINTER Target,
    IN gctCONST_POINTER Source,
    IN gctUINT32 TrgWidth,
    IN gctUINT32 BytesPerTile,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT XOffset,
    IN gctUINT YOffset,
    IN gctBOOL PatchEvenDword,
    IN gctBOOL PunchThrough
    )
{
    gctPOINTER source;
    gctUINT32 sourceStride, targetStride;
    gctUINT32 offset, rowOffset;
    gctUINT x, y;

    /* 4 bits/pixel. */
    sourceStride = gcmALIGN(Width, 4) * BytesPerTile / 16;

    targetStride = gcmALIGN(TrgWidth, 64) * BytesPerTile / 16;

    for (y = YOffset; y < Height + YOffset; y+=4)
    {
        rowOffset = (y & ~63) * targetStride;
        source = (gctPOINTER)((gctUINTPTR_T) Source + (y - YOffset) * sourceStride);

        for (x = XOffset; x < Width + XOffset; x+=4)
        {
            offset = _ComputeBlockOffset(x & 63, y & 63) * BytesPerTile;

            /* Move to this supertile row start. */
            offset += rowOffset;

            /* Move to this supertile. */
            offset += (x & ~63) * (4 * BytesPerTile);

            /* Copy the 4x4 compressed block. */
            if (PatchEvenDword && (BytesPerTile == 0x10))
            {
                gcoOS_MemCopy(
                    (gctPOINTER)((gctUINTPTR_T)Target + offset),
                    (gctPOINTER)((gctUINTPTR_T)source),
                    8);

                _ConvertETC2(
                    (gctPOINTER)((gctUINTPTR_T)Target + offset + 8),
                    (gctPOINTER)((gctUINTPTR_T)source + 8),
                    8,
                    PunchThrough);
            }
            else
            {
                _ConvertETC2(
                    (gctPOINTER)((gctUINTPTR_T)Target + offset),
                    source,
                    BytesPerTile,
                    PunchThrough);
            }

            source = (gctPOINTER) ((gctUINTPTR_T)source + BytesPerTile);
        }
    }

    return gcvSTATUS_OK;
}

static gceSTATUS
_UploadCompressedETC2(
    IN gctCONST_POINTER Target,
    IN gctCONST_POINTER Source,
    IN gctUINT ImageSize,
    IN gctBOOL PatchEvenDword,
    IN gctBOOL PunchThrough
    )
{
    gctUINT offset;

    for (offset = 0; offset < ImageSize; offset+=8)
    {
        if (PatchEvenDword && !(offset & 0x8))
        {
            gcoOS_MemCopy(
                (gctPOINTER)((gctUINTPTR_T)Target + offset),
                (gctPOINTER)((gctUINTPTR_T)Source + offset),
                8);
        }
        else
        {
            /* Copy the 4x4 compressed block. */
            _ConvertETC2(
                (gctPOINTER)((gctUINTPTR_T)Target + offset),
                (gctPOINTER)((gctUINTPTR_T)Source + offset),
                8,
                PunchThrough);
        }
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_UploadCompressedTexture
**
**  Upload one slice into a texture.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR TargetInfo
**          Destination texture format.
**
**      gctPOINTER Logical
**          Logical base address for mip map.
**
**      gctUINT32 Offset
**          Offset into mip map to upload.
**
**      gctUINT Width
**          Width of texture to upload.
**
**      gctUINT Height
**          Height of texture to upload.
**
**      gctINT SourceStride
**          Stride of user buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_UploadCompressedTexture(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctCONST_POINTER Logical,
    IN gctUINT32 Offset,
    IN gctUINT32 XOffset,
    IN gctUINT32 YOffset,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT ImageSize
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL needFlushCPUCache = gcvTRUE;
    gctBOOL patchEvenDword = gcvFALSE;
    gctBOOL patch = gcvFALSE;

    gcmHEADER_ARG("TargetInfo=0x%x Logical=0x%x "
                  "Offset=%u XOffset=%u YOffset=%u Width=%u Height=%u "
                  "ImageSize=%u",
                  TargetInfo, Logical, Offset, XOffset, YOffset,
                  Width, Height,
                  ImageSize);

    switch (TargetInfo->format)
    {
    case gcvSURF_RGBA8_ETC2_EAC:
        /* Fall through */
    case gcvSURF_SRGB8_ALPHA8_ETC2_EAC:
        /* Fall through */
        patchEvenDword = gcvTRUE;

    case gcvSURF_RGB8_ETC2:
        /* Fall through */
    case gcvSURF_SRGB8_ETC2:
        /* Fall through */
    case gcvSURF_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        /* Fall through */
    case gcvSURF_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        /* Fall through */

        /* Verify that the surface is locked. */
        gcmVERIFY_NODE_LOCK(&TargetInfo->node);

        patch = gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_HALTI2) == gcvFALSE;

        if (patch)
        {
            gctBOOL punchThrough = gcvFALSE;

            if ((TargetInfo->format == gcvSURF_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
             || (TargetInfo->format == gcvSURF_RGB8_PUNCHTHROUGH_ALPHA1_ETC2))
            {
                punchThrough = gcvTRUE;
            }

            if (TargetInfo->superTiled)
            {
                gcmONERROR(
                    _UploadCompressedSuperTiledETC2(
                        TargetInfo->node.logical + Offset,
                        Logical,
                        TargetInfo->alignedWidth,
                        /* 4x4 block. */
                        (TargetInfo->bitsPerPixel * 4 * 4) / 8,
                        Width,
                        Height,
                        XOffset,
                        YOffset,
                        patchEvenDword,
                        punchThrough));
            }
            else
            {
                gcmONERROR(
                    _UploadCompressedETC2(
                        TargetInfo->node.logical + Offset,
                        Logical,
                        ImageSize,
                        patchEvenDword,
                        punchThrough));
            }
            break;
        }
        /* else case, fall through. */


    case gcvSURF_DXT1:
        /* Fall through */
    case gcvSURF_ETC1:
        /* Fall through */
    case gcvSURF_DXT2:
        /* Fall through */
    case gcvSURF_DXT3:
        /* Fall through */
    case gcvSURF_DXT4:
        /* Fall through */
    case gcvSURF_DXT5:
        /* Fall through */
    case gcvSURF_R11_EAC:
        /* Fall through */
    case gcvSURF_SIGNED_R11_EAC:
        /* Fall through */
    case gcvSURF_RG11_EAC:
        /* Fall through */
    case gcvSURF_SIGNED_RG11_EAC:
        /* Fall through */

    case gcvSURF_ASTC4x4:
    case gcvSURF_ASTC5x4:
    case gcvSURF_ASTC5x5:
    case gcvSURF_ASTC6x5:
    case gcvSURF_ASTC6x6:
    case gcvSURF_ASTC8x5:
    case gcvSURF_ASTC8x6:
    case gcvSURF_ASTC8x8:
    case gcvSURF_ASTC10x5:
    case gcvSURF_ASTC10x6:
    case gcvSURF_ASTC10x8:
    case gcvSURF_ASTC10x10:
    case gcvSURF_ASTC12x10:
    case gcvSURF_ASTC12x12:
        /* Fall through */

    case gcvSURF_ASTC4x4_SRGB:
    case gcvSURF_ASTC5x4_SRGB:
    case gcvSURF_ASTC5x5_SRGB:
    case gcvSURF_ASTC6x5_SRGB:
    case gcvSURF_ASTC6x6_SRGB:
    case gcvSURF_ASTC8x5_SRGB:
    case gcvSURF_ASTC8x6_SRGB:
    case gcvSURF_ASTC8x8_SRGB:
    case gcvSURF_ASTC10x5_SRGB:
    case gcvSURF_ASTC10x6_SRGB:
    case gcvSURF_ASTC10x8_SRGB:
    case gcvSURF_ASTC10x10_SRGB:
    case gcvSURF_ASTC12x10_SRGB:
    case gcvSURF_ASTC12x12_SRGB:
        /* Fall through */

        /* Verify that the surface is locked. */
        gcmVERIFY_NODE_LOCK(&TargetInfo->node);

        if (TargetInfo->superTiled)
        {
            gcmONERROR(
                _UploadCompressedSuperTiled(
                    TargetInfo->node.logical + Offset,
                    Logical,
                    TargetInfo->alignedWidth,
                    /* 4x4 block. */
                    (TargetInfo->bitsPerPixel * 4 * 4) / 8,
                    Width,
                    Height,
                    XOffset,
                    YOffset));
        }
        else
        {
            switch (TargetInfo->format)
            {
            case gcvSURF_DXT1:
                /* Fall through */
            case gcvSURF_DXT2:
                /* Fall through */
            case gcvSURF_DXT3:
                /* Fall through */
            case gcvSURF_DXT4:
                /* Fall through */
            case gcvSURF_DXT5:
                /* Fall through */
                {
                    /* Uploaded texture in linear fashion. */
                    gctUINT height, width, leftOffset, bottomOffset, destOffset, srcOffset;
                    gctUINT blockSize =  (TargetInfo->bitsPerPixel * 4 * 4) / 8;

                    leftOffset = (XOffset + 3) / 4;
                    bottomOffset = ((YOffset + 3) / 4) * ((TargetInfo->alignedWidth + 3) / 4);

                    for (height = 0; height < ((Height + 3) / 4); height++)
                    {
                        for (width = 0; width < ((Width + 3) / 4); width++)
                        {
                            destOffset = (bottomOffset + leftOffset + height *  ((TargetInfo->alignedWidth + 3) / 4) + width) * blockSize;
                            srcOffset = (height * ((Width + 3) / 4) + width)* blockSize;
                            gcoOS_MemCopy(
                                (gctPOINTER)((gctUINT8_PTR)TargetInfo->node.logical + destOffset + Offset),
                                (gctPOINTER)((gctUINT8_PTR)Logical + srcOffset),
                                blockSize);
                        }
                    }
                }
                break;

            default:
                /* Uploaded texture in linear fashion. */
                gcmONERROR(
                           gcoHARDWARE_CopyData(&TargetInfo->node,
                                                Offset,
                                                Logical,
                                                ImageSize));
                /* gcoHARDWARE_CopyData already flushed the CPU cache */
                needFlushCPUCache = gcvFALSE;
                break;
            }
        }
        break;

    default:
        /* Non compressed textures cannot be uploaded supertiled here. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (needFlushCPUCache)
    {
        /* Flush the CPU cache. */
        gcmONERROR(gcoSURF_NODE_Cache(&TargetInfo->node,
                                      TargetInfo->node.logical,
                                      TargetInfo->node.size,
                                      gcvCACHE_CLEAN));
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/****************************************************************************************************
**
** Following 8 functions are for calc addr of different memory layouts
** The surface are already locked before calling these function. Even no need to check their lockness
** insider because the function will be called lots of times.
**
****************************************************************************************************/

void _CalcPixelAddr_Linear(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    for (index = 0; index < layers; index++)
    {
        addr[index] = surf->info.node.logical
                    + surf->info.layerSize * index
                    + surf->info.sliceSize * z
                    + (x * surf->info.bitsPerPixel / 8 + y * surf->info.stride) / layers;
    }

    return;
}

void _CalcPixelAddr_Tiled(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* Tiled 0: { X[21-3], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((x & 0x03) << 0) | ((y & 0x03) << 2) | ((x & ~0x03) << 2);
    gctSIZE_T yt = (y & ~0x03);
    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;
    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    for (index = 0; index < layers; index++)
    {
        addr[index] = surf->info.node.logical  +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }

    return;
}


/****************************************************************************************************
** Following 3 functions are for SuperTile w/o split.
****************************************************************************************************/

void _CalcPixelAddr_SuperTiled_Mode0(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* SuperTileMode 0: { X[21-6], Y[5:2],X[5:2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((x &  0x03) << 0) |
                   ((y &  0x03) << 2) |
                   ((x &  0x3C) << 2) |
                   ((y &  0x3C) << 6) |
                   ((x & ~0x3F) << 6);
    gctSIZE_T yt = (y & ~0x3F);
    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;
    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    for (index = 0; index < layers; index++)
    {
        addr[index] = surf->info.node.logical  +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }

    return;
}

void _CalcPixelAddr_SuperTiled_Mode1(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* SuperTileMode 1: { X[21-1:6], Y[5:4],X[5:3], Y[3:2],X[2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((x &  0x03) << 0) |
                   ((y &  0x03) << 2) |
                   ((x &  0x04) << 2) |
                   ((y &  0x0C) << 3) |
                   ((x &  0x38) << 4) |
                   ((y &  0x30) << 6) |
                   ((x & ~0x3F) << 6);
    gctSIZE_T yt = (y & ~0x3F);
    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;

    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    for (index = 0; index < layers; index++)
    {
        addr[index] = surf->info.node.logical  +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }

    return;
}

void _CalcPixelAddr_SuperTiled_Mode2(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* SuperTileMode 2: { X[21-1:6], Y[5],X[5], Y[4],X[4], Y[3],X[3], Y[2],X[2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((x &  0x03) << 0) |
                   ((y &  0x03) << 2) |
                   ((x &  0x04) << 2) |
                   ((y &  0x04) << 3) |
                   ((x &  0x08) << 3) |
                   ((y &  0x08) << 4) |
                   ((x &  0x10) << 4) |
                   ((y &  0x10) << 5) |
                   ((x &  0x20) << 5) |
                   ((y &  0x20) << 6) |
                   ((x & ~0x3F) << 6);
    gctSIZE_T yt = (y & ~0x3F);
    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;

    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    for (index = 0; index < layers; index++)
    {
        addr[index] = surf->info.node.logical  +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }

    return;
}


/****************************************************************************************************
** Following 3 functions are for MultiSuperTile
** They do NOT support 3D, because cannot program 3D texture with split layout .
****************************************************************************************************/

void _CalcPixelAddr_MultiSuperTiled_Mode0(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* determine which part? top buffer or bottom. */
    gctUINT8_PTR baseAddr = (((x >> 3) ^ (y >> 2)) & 0x01)
                          ? surf->info.node.logicalBottom
                          : surf->info.node.logical;

    /* Calc coord in split surface */
    gctSIZE_T xs = (x & ~0x8) | ((y & 0x4) << 1);
    gctSIZE_T ys = ((y & ~0x7) >> 1) | (y & 0x3);

    /* SuperTileMode 0: { X[21-6], Y[5:2],X[5:2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((xs &  0x03) << 0) |
                   ((ys &  0x03) << 2) |
                   ((xs &  0x3C) << 2) |
                   ((ys &  0x3C) << 6) |
                   ((xs & ~0x3F) << 6);
    gctSIZE_T yt = (ys & ~0x3F);

    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;

    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    gcmASSERT(z==0);

    for (index = 0; index < layers; index++)
    {
        addr[index] = baseAddr +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }
    return;
}

void _CalcPixelAddr_MultiSuperTiled_Mode1(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* determine which part? top buffer or bottom. */
    gctUINT8_PTR baseAddr = (((x >> 3) ^ (y >> 2)) & 0x01)
                          ? surf->info.node.logicalBottom
                          : surf->info.node.logical;

    /* Calc coord in split surface */
    gctSIZE_T xs = (x & ~0x8) | ((y & 0x4) << 1);
    gctSIZE_T ys = ((y & ~0x7) >> 1) | (y & 0x3);

    /* SuperTileMode 1: { X[21-1:6], Y[5:4],X[5:3], Y[3:2],X[2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((xs &  0x03) << 0) |
                   ((ys &  0x03) << 2) |
                   ((xs &  0x04) << 2) |
                   ((ys &  0x0C) << 3) |
                   ((xs &  0x38) << 4) |
                   ((ys &  0x30) << 6) |
                   ((xs & ~0x3F) << 6);

    gctSIZE_T yt = (ys & ~0x3F);

    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;

    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    gcmASSERT(z==0);

    for (index = 0; index < layers; index++)
    {
        addr[index] = baseAddr +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }

    return;
}

void _CalcPixelAddr_MultiSuperTiled_Mode2(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4])
{
    /* determine which part? top buffer or bottom. */
    gctUINT8_PTR baseAddr = (((x >> 3) ^ (y >> 2)) & 0x01)
                          ? surf->info.node.logicalBottom
                          : surf->info.node.logical;

    /* Calc coord in split surface */
    gctSIZE_T xs = (x & ~0x8) | ((y & 0x4) << 1);
    gctSIZE_T ys = ((y & ~0x7) >> 1) | (y & 0x3);

    /* SuperTileMode 2: { X[21-1:6], Y[5],X[5], Y[4],X[4], Y[3],X[3], Y[2],X[2], Y[1:0],X[1:0] } */
    gctSIZE_T xt = ((xs &  0x03) << 0) |
                   ((ys &  0x03) << 2) |
                   ((xs &  0x04) << 2) |
                   ((ys &  0x04) << 3) |
                   ((xs &  0x08) << 3) |
                   ((ys &  0x08) << 4) |
                   ((xs &  0x10) << 4) |
                   ((ys &  0x10) << 5) |
                   ((xs &  0x20) << 5) |
                   ((ys &  0x20) << 6) |
                   ((xs & ~0x3F) << 6);
    gctSIZE_T yt = (ys & ~0x3F);

    gctSIZE_T offsetInPixels = xt + yt * surf->info.alignedWidth;

    gctUINT32 index;
    gctUINT32 layers = surf->info.formatInfo.layers;

    gcmASSERT(z==0);

    for (index = 0; index < layers; index++)
    {
        addr[index] = baseAddr +
                  surf->info.layerSize * index +
                  surf->info.sliceSize * z     +
                  offsetInPixels * (surf->info.bitsPerPixel / 8)/layers;
    }
    return;
}


_PFNcalcPixelAddr gcoHARDWARE_GetProcCalcPixelAddr(
        IN gcoHARDWARE Hardware,
        IN gcoSURF surf)
{
    gceSTATUS status;

    switch (surf->info.tiling)
    {
    case gcvLINEAR:
    case gcvINVALIDTILED:   /* If surface is a wrapper surface, its tiling is not initialized. So it's treat as linear. */
        return _CalcPixelAddr_Linear;

    case gcvTILED:
        return _CalcPixelAddr_Tiled;

    case gcvSUPERTILED:
        gcmGETHARDWARE(Hardware);
        switch (Hardware->config->superTileMode)
        {
        case 0:
            return _CalcPixelAddr_SuperTiled_Mode0;
        case 1:
            return _CalcPixelAddr_SuperTiled_Mode1;
        case 2:
            return _CalcPixelAddr_SuperTiled_Mode2;
        default:
            gcmASSERT(0);
        }
        break;

    case gcvMULTI_SUPERTILED:
        gcmGETHARDWARE(Hardware);
        switch (Hardware->config->superTileMode)
        {
        case 0:
            return _CalcPixelAddr_MultiSuperTiled_Mode0;
        case 1:
            return _CalcPixelAddr_MultiSuperTiled_Mode1;
        case 2:
            return _CalcPixelAddr_MultiSuperTiled_Mode2;
        default:
            gcmASSERT(0);
        }
        break;

    default:
        gcmASSERT(0);
    }

OnError:
    return gcvNULL;
}

/*******************************************************************************
**
**  gcoHARDWARE_FlushTexture
**
**  Flish the texture cache.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL vsUsed
**          Used by vertex shader or pixel shader.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_FlushTexture(
    IN gcoHARDWARE Hardware,
    IN gctBOOL vsUsed
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set texture flush flag. */
    Hardware->hwTxFlushPS = (vsUsed == gcvFALSE);
    Hardware->hwTxFlushVS = vsUsed;


    Hardware->textureDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/***************************************************************
* convert a input floating point number to an fixed point
* 5.5 format. The value is clamped to (-16, 16)
****************************************************************/
gctFIXED_POINT _Float2SignedFixed5Dot5(gctFLOAT x)
{
    gctFIXED_POINT outInteger;

    gctUINT32 in = *((gctUINT32*)&x);

    const gctINT8   truncBits = 5;
    const gctINT8   fracBits  = 5;
    const gctUINT16 fixedMask = (1 << (truncBits + fracBits)) - 1;
    const gctUINT16 fixedMax  = (1 << (truncBits + fracBits - 1)) - 1;
    const gctUINT16 fixedMin  = 1 << (truncBits + fracBits - 1);

    gctBOOL   signIn = (in & 0x80000000) ? gcvTRUE : gcvFALSE;
    /* minus 7F to get signed exponent */
    gctINT16  expIn  = ((in >> 23) & 0xFF) - 0x7F;
    /* There is implicit "1" before the 23 mantissa bits */
    gctUINT32 manIn  = (in & 0x7FFFFF) | 0x800000;


    if (expIn < -fracBits)
    {
        outInteger = 0;
    }
    else if (expIn >= truncBits - 1)
    {
        outInteger = signIn ? fixedMin : fixedMax;
    }
    else
    {
        gctFIXED_POINT manShift = 23 - (expIn + fracBits);
        outInteger = manIn >> manShift;
        if (signIn)
        {
            outInteger = (~outInteger + 1) & fixedMask;
        }
    }

    return outInteger;
}

/*******************************************************************************
**
**  gcoHARDWARE_BindTexture
**
**  Bind texture info to a sampler.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctINT Sampler
**          Sampler number.
**
**      gcsSAMPLER_PTR SamplerInfo
**          Pointer to a gcsSAMPLER structure.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_BindTexture(
    IN gcoHARDWARE Hardware,
    IN gctINT Sampler,
    IN gcsSAMPLER_PTR SamplerInfo
    )
{
    gceSTATUS status;
    gctBOOL   type, prevType;
    gctUINT32 width, height, depth;
    gctUINT32 endian;
    gctFIXED_POINT bias = 0, lodmin = 0, lodmax = 0;
    gctINT    format;
    gctUINT32 i, anisoLog;
    gctUINT32 sampleMode, sampleModeEx;
    gctUINT32 sampleMode2 =  0x00030000;
    gctUINT32 sampleWH, sampleLogWH, sample3D, sampleLOD, baseLOD;
    gctUINT32 sampleLinearStride;
    gctUINT32 sampleYUVControl, sampleYUVStride;
    gctUINT32 yOffset = 0, uOffset = 0, vOffset = 0;
    gctUINT32 sampleTSConfig, sampleTSBuffer, sampleTSClearValue, sampleTSClearValueUpper;
    gctUINT32 samplerHeight;
    gctUINT32 samplerDepth;
    gceTEXTURE_FILTER minFilter;
    gceTEXTURE_FILTER mipFilter;
    gceTEXTURE_FILTER magFilter;
    gctUINT32 filterConstant;
    gctINT swapUV;
    gctINT roundUV;
    gctBOOL integerFormat;
    /*Always enable NPOT as either hw support it or not, it can't be dynamically switched as cmodel indicated*/
    gctUINT32 npot = 1;
    gctBOOL halti1Avail;
    gctBOOL halti2Avail;
    gctBOOL isTexArray = gcvFALSE;
    gctBOOL isASTCtex = (SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_ASTC);
    gctBOOL texTSMapping;

#if gcdDEBUG_OPTION && gcdDEBUG_OPTION_NONE_TEXTURE
    gcePATCH_ID patchId = gcvPATCH_INVALID;
#endif

    static const gctINT32 addressXlate[] =
    {
        /* gcvTEXTURE_INVALID */
        -1,
        /* gcvTEXTURE_CLAMP */
        0x2,
        /* gcvTEXTURE_WRAP */
        0x0,
        /* gcvTEXTURE_MIRROR */
        0x1,
        /* gcvTEXTURE_BORDER */
        0x3,
        /* gcvTEXTURE_MIRROR_ONCE */
        -1,
    };

    static const gctINT32 magXlate[] =
    {
        /* gcvTEXTURE_NONE */
        0x0,
        /* gcvTEXTURE_POINT */
        0x1,
        /* gcvTEXTURE_LINEAR */
        0x2,
        /* gcvTEXTURE_ANISOTROPIC */
        0x3,
    };

    static const gctINT32 minXlate[] =
    {
        /* gcvTEXTURE_NONE */
        0x0,
        /* gcvTEXTURE_POINT */
        0x1,
        /* gcvTEXTURE_LINEAR */
        0x2,
        /* gcvTEXTURE_ANISOTROPIC */
        0x3,
    };

    static const gctINT32 mipXlate[] =
    {
        /* gcvTEXTURE_NONE */
        0x0,
        /* gcvTEXTURE_POINT */
        0x1,
        /* gcvTEXTURE_LINEAR */
        0x2,
        /* gcvTEXTURE_ANISOTROPIC */
        0x3,
    };

    static const gctINT32 alignXlate[] =
    {
        /* gcvSURF_FOUR */
        0x0,
        /* gcvSURF_SIXTEEN */
        0x1,
        /* gcvSURF_SUPER_TILED */
        0x2,
        /* gcvSURF_SPLIT_TILED */
        0x3,
        /* gcvSURF_SPLIT_SUPER_TILED */
        0x4,
    };

    static const gctINT32 addressingXlate[] =
    {
        /* gcvSURF_NO_STRIDE_TILED. */
        0x0,
        /* gcvSURF_NO_STRIDE_LINEAR. */
        0x1,
        /* gcvSURF_STRIDE_TILED. */
        0x2,
        /* gcvSURF_STRIDE_LINEAR. */
        0x3,
    };


    static const gctINT32 modeXlate[] =
    {
        -1, /* gcvTEXTURE_COMPARE_MODE_INVALID */
        0x0, /* gcvTEXTURE_COMPARE_MODE_NONE */
        0x1, /* gcvTEXTURE_COMPARE_MODE_REF */
    };

    static const gctINT32 funcXlate[] =
    {
        -1, /* gcvCOMPARE_INVALID */
        0x7, /* gcvCOMPARE_NEVER */
        0x5, /* gcvCOMPARE_NOT_EQUAL */
        0x2, /* gcvCOMPARE_LESS */
        0x0, /* gcvCOMPARE_LESS_OR_EQUAL */
        0x4, /* gcvCOMPARE_EQUAL */
        0x3, /* gcvCOMPARE_GREATER */
        0x1, /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6, /* gcvCOMPARE_ALWAYS */
    };

    gcmHEADER_ARG("Hardware=0x%x Sampler=%d SamplerInfo=0x%x",
                  Hardware, Sampler, SamplerInfo);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    halti1Avail = Hardware->features[gcvFEATURE_HALTI1];
    halti2Avail = Hardware->features[gcvFEATURE_HALTI2];
    texTSMapping = Hardware->features[gcvFEATURE_TX_TILE_STATUS_MAPPING];

    if (halti1Avail)
    {
        gcmDEBUG_VERIFY_ARGUMENT((Sampler >= 0) && (Sampler < gcdSAMPLERS));
    }
    else
    {
        gcmDEBUG_VERIFY_ARGUMENT((Sampler >= 0) && (Sampler < 16));
    }
    gcmDEBUG_VERIFY_ARGUMENT(SamplerInfo != gcvNULL);



    samplerHeight = SamplerInfo->height;
    samplerDepth  = SamplerInfo->depth;
    minFilter     = SamplerInfo->textureInfo->minFilter;
    mipFilter     = SamplerInfo->textureInfo->mipFilter;
    magFilter     = SamplerInfo->textureInfo->magFilter;

    if (!SamplerInfo->filterable)
    {
        if ((minFilter == gcvTEXTURE_LINEAR) || (minFilter == gcvTEXTURE_ANISOTROPIC))
        {
            minFilter = gcvTEXTURE_POINT;
        }

        if ((magFilter == gcvTEXTURE_LINEAR) || (magFilter == gcvTEXTURE_ANISOTROPIC))
        {
            magFilter = gcvTEXTURE_POINT;
        }

        if ((mipFilter == gcvTEXTURE_LINEAR) || (mipFilter == gcvTEXTURE_ANISOTROPIC))
        {
            mipFilter = gcvTEXTURE_POINT;
        }
    }

    if (mipFilter == gcvTEXTURE_LINEAR &&
        SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_DEPTH)
    {
        mipFilter = gcvTEXTURE_POINT;
    }

#if gcdDEBUG_OPTION && gcdDEBUG_OPTION_NONE_TEXTURE
    gcoHAL_GetPatchID(gcvNULL, &patchId);

    if (patchId == gcvPATCH_DEBUG)
    {
        type = 0x0;
    }
    else
#endif

    if (SamplerInfo->width == 0)
    {
        type = 0x0;
    }
    else
    {
        if (samplerHeight <= 1)
        {
            samplerHeight = 1;
        }
        else if (Hardware->specialHint & (1 << 1))
        {
            if (mipFilter == gcvTEXTURE_LINEAR
                && (--Hardware->specialHintData < 0)
                )
            {
#if gcdFORCE_BILINEAR
                mipFilter = gcvTEXTURE_POINT;
#endif
            }
        }

        switch (SamplerInfo->texType)
        {
        case gcvTEXTURE_2D:
            type = 0x2;
            break;

        case gcvTEXTURE_3D:
            type = 0x3;
            break;

        case gcvTEXTURE_2D_ARRAY:
            type = 0x3;
            isTexArray = gcvTRUE;
            break;

        case gcvTEXTURE_CUBEMAP:
            type = 0x5;
            break;

        case gcvTEXTURE_1D:
            type = 0x1;
            break;

        case gcvTEXTURE_1D_ARRAY:
            type = 0x2;
            isTexArray = gcvTRUE;
            break;

        /* For legacy API, just use old logic to set type, which is not correct for array texture
        ** But it's ok for ES1.0.
        */
        case gcvTEXTURE_UNKNOWN:
        default:
             type = (SamplerInfo->faces == 6)
             ?  0x5
             : (SamplerInfo->depth == 1) ? 0x2
                                         : 0x3;
             break;

        }
    }

    switch (SamplerInfo->endianHint)
    {
    case gcvENDIAN_SWAP_WORD:
        endian = Hardware->bigEndian
               ? 0x1
               : 0x0;
        break;

    case gcvENDIAN_SWAP_DWORD:
        endian = Hardware->bigEndian
               ? 0x2
               : 0x0;
        break;

    default:
        endian = 0x0;
        break;
    }

    /* Convert the format. */
    format = _GetTextureFormat(Hardware, SamplerInfo, &integerFormat);
    if (format == -1)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Swap U,V for YVYU and VYUY. */
    swapUV = SamplerInfo->format == gcvSURF_YVYU
          || SamplerInfo->format == gcvSURF_VYUY;

    if (Hardware->currentApi == gcvAPI_OPENVG)
    {
        roundUV = 0;
    }
    else if (Hardware->patchID == gcvPATCH_CTGL20 || Hardware->patchID == gcvPATCH_CTGL11)
    {
        roundUV = (SamplerInfo->textureInfo->magFilter == gcvTEXTURE_POINT && SamplerInfo->width <= 32 && SamplerInfo->height <= 32 ) ? 0 : 1;
    }
    else
    {
        roundUV = (SamplerInfo->textureInfo->minFilter == gcvTEXTURE_POINT || SamplerInfo->textureInfo->magFilter == gcvTEXTURE_POINT) ? 0 : 1;
    }

    if ((SamplerInfo->width  & (SamplerInfo->width  - 1)) ||
        (samplerHeight & (samplerHeight - 1)) ||
        (samplerDepth & (samplerDepth  - 1)))
    {
        /* HW sampling non-power of two textures only work correct
           under clamp to edge mode if HW didn't support NPOT feature */
#if !gcdUSE_NPOT_PATCH
        if(Hardware->features[gcvFEATURE_NON_POWER_OF_TWO] != gcvSTATUS_TRUE)
        {
            SamplerInfo->textureInfo->s = gcvTEXTURE_CLAMP;
            SamplerInfo->textureInfo->t = gcvTEXTURE_CLAMP;
            SamplerInfo->textureInfo->r = gcvTEXTURE_CLAMP;
        }
#endif
    }

    anisoLog = (SamplerInfo->textureInfo->anisoFilter == 1) ?
               0 : gcoMATH_Log2in5dot5(SamplerInfo->textureInfo->anisoFilter);

    filterConstant = 0;

    if (SamplerInfo->hasTileStatus
        && (SamplerInfo->width >= 1024 && samplerHeight >= 1024)
        && (Hardware->specialHintData < 0)
        && (Hardware->patchID == gcvPATCH_GLBM25 ||
            Hardware->patchID == gcvPATCH_GLBM27 ||
            Hardware->patchID == gcvPATCH_GFXBENCH)
        )
    {
        minFilter = magFilter = gcvTEXTURE_POINT;
    }

    /* Set the gcregTXConfig register value. */
    sampleMode =
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (type) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:13) - (0 ? 17:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:13) - (0 ? 17:13) + 1))))))) << (0 ? 17:13))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 17:13) - (0 ? 17:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:13) - (0 ? 17:13) + 1))))))) << (0 ? 17:13)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:3) - (0 ? 4:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:3) - (0 ? 4:3) + 1))))))) << (0 ? 4:3))) | (((gctUINT32) ((gctUINT32) (addressXlate[SamplerInfo->textureInfo->s]) & ((gctUINT32) ((((1 ? 4:3) - (0 ? 4:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:3) - (0 ? 4:3) + 1))))))) << (0 ? 4:3)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (addressXlate[SamplerInfo->textureInfo->t]) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:7) - (0 ? 8:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:7) - (0 ? 8:7) + 1))))))) << (0 ? 8:7))) | (((gctUINT32) ((gctUINT32) (minXlate[minFilter]) & ((gctUINT32) ((((1 ? 8:7) - (0 ? 8:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:7) - (0 ? 8:7) + 1))))))) << (0 ? 8:7)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11))) | (((gctUINT32) ((gctUINT32) (magXlate[magFilter]) & ((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9))) | (((gctUINT32) ((gctUINT32) (mipXlate[mipFilter]) & ((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) ((gctUINT32) (filterConstant) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) ((gctUINT32) (roundUV) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (addressingXlate[SamplerInfo->addressing]) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
        /* Compute log2 in 5.5 format. */
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (anisoLog) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22))) | (((gctUINT32) ((gctUINT32) (endian) & ((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22)));

    /* Set the gcregTXExtConfig register value. */
    sampleModeEx =
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) ((format >> 12)) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) ((format >> 20)) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) ((format >> 23)) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) ((format >> 26)) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20))) | (((gctUINT32) ((gctUINT32) ((format >> 29)) & ((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) ((gctUINT32) (swapUV) & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->hasTileStatus) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (isTexArray) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:26) - (0 ? 28:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:26) - (0 ? 28:26) + 1))))))) << (0 ? 28:26))) | (((gctUINT32) ((gctUINT32) (alignXlate[SamplerInfo->hAlignment]) & ((gctUINT32) ((((1 ? 28:26) - (0 ? 28:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:26) - (0 ? 28:26) + 1))))))) << (0 ? 28:26)));

    /* Always enable seamless cubemap for es30 requires that. */
    if (halti2Avail)
    {
        sampleModeEx |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)));
    }

    if (texTSMapping && (SamplerInfo->hasTileStatus))
    {
        gcmASSERT(Sampler < gcdSAMPLER_TS);
        sampleMode2 |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (Sampler) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)));
    }

    /* Set the gcregTXSize register value. */
    sampleWH =
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->width) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (samplerHeight) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

    /* Compute log2 in 5.5 format for width and height. */
    width  = gcoMATH_Log2in5dot5(SamplerInfo->width);
    height = gcoMATH_Log2in5dot5(samplerHeight);
    depth  = gcoMATH_Log2in5dot5(samplerDepth);

    /* Can integer filter be used? */
    integerFormat = integerFormat
                    && (type != 0x3)
                    && (anisoLog == 0);

    /* Set the gcregTXLogSize register value. */
    sampleLogWH =
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (width) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:10) - (0 ? 19:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:10) - (0 ? 19:10) + 1))))))) << (0 ? 19:10))) | (((gctUINT32) ((gctUINT32) (height) & ((gctUINT32) ((((1 ? 19:10) - (0 ? 19:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:10) - (0 ? 19:10) + 1))))))) << (0 ? 19:10)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (integerFormat) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (!npot) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (((format >> 18) & 1)) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27))) | (((gctUINT32) ((gctUINT32) ((isASTCtex ? 2 : 0)) & ((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27)));

    sample3D = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:0) - (0 ? 13:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:0) - (0 ? 13:0) + 1))))))) << (0 ? 13:0))) | (((gctUINT32) ((gctUINT32) (samplerDepth) & ((gctUINT32) ((((1 ? 13:0) - (0 ? 13:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:0) - (0 ? 13:0) + 1))))))) << (0 ? 13:0)))
             | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (depth) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
             | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) ((gctUINT32) (addressXlate[SamplerInfo->textureInfo->r]) & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)));

    bias   = _Float2SignedFixed5Dot5(SamplerInfo->textureInfo->lodBias);
    lodmax = _Float2SignedFixed5Dot5(SamplerInfo->textureInfo->lodMax);
    lodmin = _Float2SignedFixed5Dot5(SamplerInfo->textureInfo->lodMin);

    /* Set gcregTXLod register value. */
    sampleLOD = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) ((bias == 0 ? 0 : 1)) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:1) - (0 ? 10:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:1) - (0 ? 10:1) + 1))))))) << (0 ? 10:1))) | (((gctUINT32) ((gctUINT32) (lodmax) & ((gctUINT32) ((((1 ? 10:1) - (0 ? 10:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:1) - (0 ? 10:1) + 1))))))) << (0 ? 10:1)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:11) - (0 ? 20:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:11) - (0 ? 20:11) + 1))))))) << (0 ? 20:11))) | (((gctUINT32) ((gctUINT32) (lodmin) & ((gctUINT32) ((((1 ? 20:11) - (0 ? 20:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:11) - (0 ? 20:11) + 1))))))) << (0 ? 20:11)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:21) - (0 ? 30:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:21) - (0 ? 30:21) + 1))))))) << (0 ? 30:21))) | (((gctUINT32) ((gctUINT32) (bias) & ((gctUINT32) ((((1 ? 30:21) - (0 ? 30:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:21) - (0 ? 30:21) + 1))))))) << (0 ? 30:21)));

    if (halti2Avail)
    {
        baseLOD = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->textureInfo->baseLevel) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->textureInfo->maxLevel) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (modeXlate[SamplerInfo->textureInfo->compareMode]) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20))) | (((gctUINT32) ((gctUINT32) (funcXlate[SamplerInfo->textureInfo->compareFunc]) & ((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:23) - (0 ? 24:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:23) - (0 ? 24:23) + 1))))))) << (0 ? 24:23))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 24:23) - (0 ? 24:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:23) - (0 ? 24:23) + 1))))))) << (0 ? 24:23)));
    }
    else
    {
        baseLOD = 0;
    }


    /* Program tile status information. */
    if (SamplerInfo->hasTileStatus)
    {
        gctUINT control;

        if (!SamplerInfo->compressed)
        {
            control = 1; /* Enable */
        }
        else
        {
            /*
            ** 1, invalid compression format to tx decoding.
            ** 2, v1 tx can't support z16 decompression.
            */
            if ((SamplerInfo->compressedFormat == -1) ||
                ((SamplerInfo->compressedFormat == 0x8) &&
                 (Hardware->features[gcvFEATURE_COMPRESSION_V1])))
            {
                gcmFATAL("tx decoder is taking invalid compression format, surface format=%d",
                          SamplerInfo->format);
            }

            /* Enabled and compressed */
            control = 3;
        }

        sampleTSConfig     = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (control) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->compressedFormat) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
                           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->formatInfo->bitsPerPixel == 64) & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9)));

        sampleTSClearValue = SamplerInfo->tileStatusClearValue;
        sampleTSClearValueUpper = SamplerInfo->tileStatusClearValueUpper;
        sampleTSBuffer     = SamplerInfo->tileStatusAddr;
    }
    else
    {
        sampleTSConfig     = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
        sampleTSClearValue = 0x0;
        sampleTSClearValueUpper = 0x0;
        sampleTSBuffer     = 0x00C0FFEE;
    }

    /* Program linear texture information. */
    if (SamplerInfo->addressing == gcvSURF_STRIDE_LINEAR)
    {
        /* Linear texture stride. */
        sampleLinearStride = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->lodStride[SamplerInfo->baseLod]) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0)));
    }
    else
    {
        sampleLinearStride = 0;
    }

    /* Program YUV-assembler information. */
    if ((SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_YUV) &&
        (SamplerInfo->tiling == gcvLINEAR))
    {
        /* YUV-assembler can only use LOD 0.
         * Y plane address is from LOD 0,
         * U plane address is from LOD 1,
         * V plane address is from LOD 2,
         * Set stride from lodStride. */
        sampleYUVControl = _GetYUVControl(
            SamplerInfo->format,
            &yOffset,
            &uOffset,
            &vOffset
            );

        /* YUV strides. */
        sampleLinearStride = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->lodStride[0]) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0)));

        sampleYUVStride  =
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->lodStride[1]) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SamplerInfo->lodStride[2]) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));
    }
    else
    {
        sampleYUVControl = 0;
        sampleYUVStride  = 0;
    }

#if gcdSECURITY
    Hardware->txLodNums[Sampler] = SamplerInfo->lodNum;
#endif

    prevType = (((((gctUINT32) (Hardware->hwTxSamplerMode[Sampler])) >> (0 ? 2:0)) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1)))))) );

    /* Save changed sampler mode register. */
    if (Hardware->hwTxSamplerMode[Sampler] != sampleMode)
    {
        Hardware->hwTxSamplerMode[Sampler] = sampleMode;
        Hardware->hwTxSamplerModeDirty    |= 1 << Sampler;
    }

    if (Hardware->hwTxSamplerModeEx[Sampler] != sampleModeEx)
    {
        Hardware->hwTxSamplerModeEx[Sampler] = sampleModeEx;
        Hardware->hwTxSamplerModeDirty      |= 1 << Sampler;
    }

    if (Hardware->hwTxSamplerMode2[Sampler] != sampleMode2)
    {
        Hardware->hwTxSamplerMode2[Sampler] = sampleMode2;
        Hardware->hwTxSamplerModeDirty      |= 1 << Sampler;
    }

    /* Save changed sampler size register. */
    if (Hardware->hwTxSamplerSize[Sampler] != sampleWH)
    {
        Hardware->hwTxSamplerSize[Sampler] = sampleWH;
        Hardware->hwTxSamplerSizeDirty    |= 1 << Sampler;
    }

    /* Save changed sampler log2 size register. */
    if (Hardware->hwTxSamplerSizeLog[Sampler] != sampleLogWH)
    {
        Hardware->hwTxSamplerSizeLog[Sampler] = sampleLogWH;
        Hardware->hwTxSamplerSizeLogDirty    |= 1 << Sampler;
    }

    /* Save changed sampler 3D register. */
    /* GCREG_TX3_D did not take effect when texture type is not 3D, need to reprogram if switched. */
    if ((Hardware->hwTxSampler3D[Sampler] != sample3D) ||
        (type == 0x3 && prevType != 0x3))
    {
        Hardware->hwTxSampler3D[Sampler]  = sample3D;
        Hardware->hwTxSampler3DDirty     |= 1 << Sampler;
    }

    /* Save changed sampler LOD register. */
    if (Hardware->hwTxSamplerLOD[Sampler] != sampleLOD)
    {
        Hardware->hwTxSamplerLOD[Sampler] = sampleLOD;
        Hardware->hwTxSamplerLODDirty    |= 1 << Sampler;
    }

    /* Save changed base LOD register. */
    if (Hardware->hwTxBaseLOD[Sampler] != baseLOD)
    {
        Hardware->hwTxBaseLOD[Sampler]  = baseLOD;
        Hardware->hwTxBaseLODDirty     |= 1 << Sampler;
    }

    /* Save changed sampler linear stride register. */
    if (Hardware->hwTxSamplerLinearStride[Sampler] != sampleLinearStride)
    {
        Hardware->hwTxSamplerLinearStride[Sampler] = sampleLinearStride;
        Hardware->hwTxSamplerLinearStrideDirty    |= 1 << Sampler;
    }

    /* Save changed sampler YUV-assembler control register. */
    if (Hardware->hwTxSamplerYUVControl[Sampler] != sampleYUVControl)
    {
        Hardware->hwTxSamplerYUVControl[Sampler] = sampleYUVControl;
        Hardware->hwTxSamplerYUVControlDirty    |= 1 << Sampler;
    }

    /* Save changed sampler YUV stride register. */
    if (Hardware->hwTxSamplerYUVStride[Sampler] != sampleYUVStride)
    {
        Hardware->hwTxSamplerYUVStride[Sampler] = sampleYUVStride;
        Hardware->hwTxSamplerYUVStrideDirty    |= 1 << Sampler;
    }

    /* Set each LOD. */
    for (i = SamplerInfo->baseLod; i < SamplerInfo->lodNum; i++)
    {
        if (Hardware->hwTxSamplerAddress[i][Sampler] != SamplerInfo->lodAddr[i])
        {
            Hardware->hwTxSamplerAddress[i][Sampler] = SamplerInfo->lodAddr[i];
            Hardware->hwTxSamplerAddressDirty[i]    |= 1 << Sampler;

            if (!halti1Avail)
            {
                /* we should always set stride if address changes. Otherwise, hw doesn't use
                ** correct stride to get texture data. This fix is for bug7155. please refer to CL6130.
                */
                Hardware->hwTxSamplerLinearStrideDirty  |= 1 << Sampler;
            }
        }
    }

    /* Need adjust LOD addresses for YUV-assembler. */
    if ((SamplerInfo->formatInfo->fmtClass == gcvFORMAT_CLASS_YUV) &&
        (SamplerInfo->tiling == gcvLINEAR))
    {
        Hardware->hwTxSamplerAddress[0][Sampler] += yOffset;
        Hardware->hwTxSamplerAddress[1][Sampler] += uOffset;
        Hardware->hwTxSamplerAddress[2][Sampler] += vOffset;
    }

    /* Save changed sampler TS registers. */
    /* HW only has 8 TS configures for texture.
       Do we need another map from sampler to TS? */
    if (Sampler < gcdSAMPLER_TS)
    {
        if ((Hardware->hwTXSampleTSConfig[Sampler]     != sampleTSConfig)
        ||  (Hardware->hwTXSampleTSBuffer[Sampler]     != sampleTSBuffer)
        ||  (Hardware->hwTXSampleTSClearValue[Sampler] != sampleTSClearValue)
        ||  (Hardware->hwTXSampleTSClearValueUpper[Sampler] != sampleTSClearValueUpper)
        ||  (Hardware->hwTxSamplerTxBaseBuffer[Sampler] != SamplerInfo->lodAddr[0]))
        {
            Hardware->hwTXSampleTSConfig[Sampler]     = sampleTSConfig;
            Hardware->hwTXSampleTSBuffer[Sampler]     = sampleTSBuffer;
            Hardware->hwTXSampleTSClearValue[Sampler] = sampleTSClearValue;
            Hardware->hwTXSampleTSClearValueUpper[Sampler] = sampleTSClearValueUpper;
            Hardware->hwTxSamplerTxBaseBuffer[Sampler] = SamplerInfo->lodAddr[0];
            Hardware->hwTxSamplerTSDirty             |= 1 << Sampler;
        }
    }

    /* ASTC textures. */
    if (Hardware->features[gcvFEATURE_TEXTURE_ASTC])
    {
        for (i = SamplerInfo->baseLod; i < SamplerInfo->lodNum; i += 1)
        {
            if ((Hardware->hwTxSamplerASTCSize[i][Sampler] != SamplerInfo->astcSize[i])
            ||  (Hardware->hwTxSamplerASTCSRGB[i][Sampler] != SamplerInfo->astcSRGB[i]))
            {
                Hardware->hwTxSamplerASTCSize[i][Sampler] = SamplerInfo->astcSize[i];
                Hardware->hwTxSamplerASTCSRGB[i][Sampler] = SamplerInfo->astcSRGB[i];
                Hardware->hwTxSamplerASTCDirty[i / 4]    |= 1 << Sampler;
            }
        }
    }

    Hardware->samplerDirty |=
                   Hardware->hwTxSamplerModeDirty         |
                   Hardware->hwTxSamplerSizeDirty         |
                   Hardware->hwTxSamplerSizeLogDirty      |
                   Hardware->hwTxSampler3DDirty           |
                   Hardware->hwTxSamplerLODDirty          |
                   Hardware->hwTxBaseLODDirty             |
                   Hardware->hwTxSamplerYUVControlDirty   |
                   Hardware->hwTxSamplerYUVStrideDirty    |
                   Hardware->hwTxSamplerLinearStrideDirty |
                   Hardware->hwTxSamplerTSDirty;

    for (i = 0; i < gcdLOD_LEVELS; i++)
    {
        Hardware->samplerDirty |= Hardware->hwTxSamplerAddressDirty[i] |
                                  Hardware->hwTxSamplerASTCDirty[i];
    }

    if (Hardware->samplerDirty)
    {
        Hardware->textureDirty = gcvTRUE;
    }


    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHARDWARE_ProgramTexture
**
**  Program all dirty texture states.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ProgramTexture(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    static const gctUINT32 address[gcdLOD_LEVELS] =
    {
        0x0900,
        0x0910,
        0x0920,
        0x0930,
        0x0940,
        0x0950,
        0x0960,
        0x0970,
        0x0980,
        0x0990,
        0x09A0,
        0x09B0,
        0x09C0,
        0x09D0
    };

    gceSTATUS status;
    gctUINT dirty, sampler, i, index, semaStall;
    gctBOOL halti1Avail;
    gctBOOL halti2Avail;
    gctBOOL unifiedSampler;
    gctUINT32 vsSamplerMask;
    gctUINT32 psSamplerMask;
    gctINT vsBase;
    gctBOOL texTSMapping;
    gctUINT32 samplerDirty;
    gctUINT32 shaderConfigData = 0;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    halti1Avail = Hardware->features[gcvFEATURE_HALTI1];
    halti2Avail = Hardware->features[gcvFEATURE_HALTI2];
    unifiedSampler = Hardware->features[gcvFEATURE_UNIFIED_SAMPLERS];
    texTSMapping = Hardware->features[gcvFEATURE_TX_TILE_STATUS_MAPPING];

    gcmONERROR(gcoHARDWARE_QuerySamplerBase(gcvNULL,
                                        gcvNULL,
                                        &vsBase,
                                        gcvNULL,
                                        gcvNULL));

    vsSamplerMask =(~0) << vsBase;
    psSamplerMask = ~vsSamplerMask;

    samplerDirty = Hardware->samplerDirty;

    vsSamplerMask &= samplerDirty;
    psSamplerMask &= samplerDirty;

    if (Hardware->shaderStates.hints)
    {
        shaderConfigData = Hardware->shaderStates.hints->shaderConfigData;
    }

    if (Hardware->hwTxSamplerTSDirty)
    {
        semaStall = 1;
    }
    else
    {
        semaStall = 0;
    }

    {        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        if (Hardware->hwTxFlushVS)
        {
            if (unifiedSampler)
            {
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
            }
            else
            {
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                /* Send FE-PE stall token. */
                *memory++ =
                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

                *memory++ =
                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

                /* Dump the stall. */
                gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]",
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));
            }

            /* Flush the texture unit. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            Hardware->hwTxFlushVS = gcvFALSE;
        }

        if (halti1Avail)
        {
            for (dirty = 1, sampler = 0;
                 samplerDirty != 0;
                 sampler++, dirty <<= 1, samplerDirty >>=1)
            {
                if (!(samplerDirty & 0x1))
                {
                    continue;
                }

                if (unifiedSampler)
                {
                    if (vsSamplerMask && (sampler >= (gctUINT)vsBase))
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        vsSamplerMask = 0;
                    }
                    else if (psSamplerMask && sampler < (gctUINT)vsBase)
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        psSamplerMask = 0;
                    }
                }

                /* Check for dirty sampler mode registers. */
                if (dirty & Hardware->hwTxSamplerModeDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4000 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4000 + sampler, Hardware->hwTxSamplerMode[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    /* Extra state to specify texture alignment. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x40E0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x40E0 + sampler, Hardware->hwTxSamplerModeEx[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                    if (texTSMapping)
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x41E0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x41E0 + sampler, Hardware->hwTxSamplerMode2[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

                /* Check for dirty sampler size registers. */
                if (dirty & Hardware->hwTxSamplerSizeDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4020 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4020 + sampler, Hardware->hwTxSamplerSize[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler log2 size registers. */
                if (dirty & Hardware->hwTxSamplerSizeLogDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4040 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4040 + sampler, Hardware->hwTxSamplerSizeLog[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler 3D size registers. */
                if (dirty & Hardware->hwTxSampler3DDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x40C0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x40C0 + sampler, Hardware->hwTxSampler3D[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler LOD registers. */
                if (dirty & Hardware->hwTxSamplerLODDirty)
                {
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4060 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4060 + sampler, Hardware->hwTxSamplerLOD[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                if (halti2Avail)
                {
                    /* Check for dirty base LOD registers. */
                    if (dirty & Hardware->hwTxBaseLODDirty)
                    {
                        /* Save dirty state in state buffer. */
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x41C0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x41C0 + sampler, Hardware->hwTxBaseLOD[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

#if gcdSECURITY
                {
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                    /* Send FE-PE stall token. */
                    *memory++ =
                          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

                    *memory++ =
                          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

                    /* Dump the stall. */
                    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]",
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));
                }
#endif

                /* Walk all LOD levels. */
                for (i = 0; i < gcdLOD_LEVELS; ++i)
                {
                    /* Check for dirty sampler address registers. */
                    if (dirty & Hardware->hwTxSamplerAddressDirty[i])
                    {
                        gctUINT32 lodAddr = Hardware->hwTxSamplerAddress[i][sampler];

#if gcdSECURITY
                        /* If the sampler or lod was not used by shader */
                        if (!(Hardware->shaderStates.hints->usedSamplerMask & (1 << sampler)) ||
                            i >= Hardware->txLodNums[sampler])
                        {
                            lodAddr = gcvINVALID_ADDRESS;
                        }
#endif

                        /* Save dirty state in state buffer. */
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4200 + (sampler << 4) + i) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4200 + (sampler << 4) + i, lodAddr );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

                if (Hardware->features[gcvFEATURE_TEXTURE_LINEAR])
                {
                    /* Check for dirty linear stride registers. */
                    if (dirty & Hardware->hwTxSamplerLinearStrideDirty)
                    {
                        /* Save dirty state in state buffer. */
                        /* Program level 0 of the sampler. */
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x40A0+ sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x40A0+ sampler, Hardware->hwTxSamplerLinearStride[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

                /* Check for dirty YUV-assembler control registers. */
                if (dirty & Hardware->hwTxSamplerYUVControlDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4100 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4100 + sampler, Hardware->hwTxSamplerYUVControl[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty YUV stride registers. */
                if (dirty & Hardware->hwTxSamplerYUVStrideDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4120 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4120 + sampler, Hardware->hwTxSamplerYUVStride[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Walk LODs. */
                for (i = 0; i < (gcdLOD_LEVELS) / 4; ++i)
                {
                    if (dirty & Hardware->hwTxSamplerASTCDirty[i])
                    {
                        index = i * 4;

                        if (i < 3)
                        {
                            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x4140 + i * 16 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x4140 + i * 16 + sampler, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 0][sampler]) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 0][sampler]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 1][sampler]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 1][sampler]) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 2][sampler]) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 2][sampler]) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 3][sampler]) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 3][sampler]) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        }
                        else
                        {
                            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x41A0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x41A0 + sampler, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[12][sampler]) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[12][sampler]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[13][sampler]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[13][sampler]) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        }
                    }
                }

                /* Check for dirty sampler TS registers. */
                if (dirty & Hardware->hwTxSamplerTSDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05C8 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05C8 + sampler, Hardware->hwTXSampleTSConfig[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05D0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05D0 + sampler, Hardware->hwTXSampleTSBuffer[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05D8 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05D8 + sampler, Hardware->hwTXSampleTSClearValue[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                    if (halti2Avail)
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05E0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05E0 + sampler, Hardware->hwTXSampleTSClearValueUpper[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }

                    if (texTSMapping)
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x06A0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x06A0 + sampler, Hardware->hwTxSamplerTxBaseBuffer[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }
            }
        }
        else
        {
            for (dirty = 1, sampler = 0;
                 samplerDirty != 0;
                 sampler++, dirty <<= 1, samplerDirty >>=1)
            {
                if (!(samplerDirty & 0x1))
                {
                    continue;
                }

                if (unifiedSampler)
                {
                    if (vsSamplerMask && (sampler >= (gctUINT)vsBase))
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        vsSamplerMask = 0;
                    }
                    else if (psSamplerMask && sampler < (gctUINT)vsBase)
                    {
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        psSamplerMask = 0;
                    }
                }

                /* Check for dirty sampler mode registers. */
                if (dirty & Hardware->hwTxSamplerModeDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0800 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0800 + sampler, Hardware->hwTxSamplerMode[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    /* Extra state to specify texture alignment. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0870 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0870 + sampler, Hardware->hwTxSamplerModeEx[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler size registers. */
                if (dirty & Hardware->hwTxSamplerSizeDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0810 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0810 + sampler, Hardware->hwTxSamplerSize[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler log2 size registers. */
                if (dirty & Hardware->hwTxSamplerSizeLogDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0820 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0820 + sampler, Hardware->hwTxSamplerSizeLog[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler 3D size registers. */
                if (dirty & Hardware->hwTxSampler3DDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0860 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0860 + sampler, Hardware->hwTxSampler3D[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

                /* Check for dirty sampler LOD registers. */
                if (dirty & Hardware->hwTxSamplerLODDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0830 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0830 + sampler, Hardware->hwTxSamplerLOD[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }

#if gcdSECURITY
                {
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                    /* Send FE-PE stall token. */
                    *memory++ =
                          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

                    *memory++ =
                          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

                    /* Dump the stall. */
                    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]",
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));
                }
#endif

                /* Walk all LOD levels. */
                for (i = 0; i < gcdLOD_LEVELS; ++i)
                {
                    if (dirty & Hardware->hwTxSamplerAddressDirty[i])
                    {

                        gctUINT32 lodAddr = Hardware->hwTxSamplerAddress[i][sampler];

#if gcdSECURITY
                        /* If the sampler or lod was not used by shader */
                        if (!(Hardware->shaderStates.hints->usedSamplerMask & (1 << sampler)) ||
                            i >= Hardware->txLodNums[sampler])
                        {
                            lodAddr = gcvINVALID_ADDRESS;
                        }
#endif

                        /* Save dirty state in state buffer. */
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address[i] + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, address[i] + sampler, lodAddr );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

                if (Hardware->features[gcvFEATURE_TEXTURE_LINEAR])
                {
                    /* Check for dirty linear stride registers. */
                    if (dirty & Hardware->hwTxSamplerLinearStrideDirty)
                    {
                        /* Save dirty state in state buffer. */
                        /* Program level 0 of the sampler. */
                        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0B00 + (sampler * 16)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0B00 + (sampler * 16), Hardware->hwTxSamplerLinearStride[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    }
                }

                /* Walk LODs. */
                for (i = 0; i < (gcdLOD_LEVELS + 3) / 4; ++i)
                {
                    if (dirty & Hardware->hwTxSamplerASTCDirty[i])
                    {
                        index = i * 4;
                        if (i < 3)
                        {
                            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x08A0 + i * 16 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x08A0 + i * 16 + sampler, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 0][sampler]) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 0][sampler]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 1][sampler]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 1][sampler]) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 2][sampler]) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 2][sampler]) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[index + 3][sampler]) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[index + 3][sampler]) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        }
                        else
                        {
                            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x08D0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x08D0 + sampler, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[12][sampler]) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[12][sampler]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSize[13][sampler]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (Hardware->hwTxSamplerASTCSRGB[13][sampler]) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        }
                    }
                }

                /* Check for dirty sampler TS registers. */
                if (dirty & Hardware->hwTxSamplerTSDirty)
                {
                    /* Save dirty state in state buffer. */
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05C8 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05C8 + sampler, Hardware->hwTXSampleTSConfig[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05D0 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05D0 + sampler, Hardware->hwTXSampleTSBuffer[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05D8 + sampler) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x05D8 + sampler, Hardware->hwTXSampleTSClearValue[sampler] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }
            }
        }

        /* Reset dirty flags. */
        Hardware->hwTxSamplerModeDirty = 0;
        Hardware->hwTxSamplerSizeDirty = 0;
        Hardware->hwTxSamplerSizeLogDirty = 0;
        Hardware->hwTxSampler3DDirty = 0;
        Hardware->hwTxSamplerLODDirty = 0;
        Hardware->hwTxBaseLODDirty = 0;
        Hardware->hwTxSamplerLinearStrideDirty = 0;
        Hardware->hwTxSamplerYUVControlDirty = 0;
        Hardware->hwTxSamplerYUVStrideDirty = 0;
        Hardware->hwTxSamplerTSDirty = 0;

        for (i = 0; i < gcdLOD_LEVELS; ++i)
        {
            Hardware->hwTxSamplerAddressDirty[i] = 0;
            Hardware->hwTxSamplerASTCDirty[i] = 0;
        }

        /* Check for texture cache flush. */
        if (Hardware->hwTxFlushPS)
        {
            if (unifiedSampler)
            {
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0218, ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
            }

            /* Flush the texture unit. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            if (Hardware->config->chipModel == gcv700
#if gcdMULTI_GPU
                || Hardware->config->gpuCoreCount > 1
#endif
                )
            {
                /* Flush the L2 cache as well. */
                Hardware->flushL2 = gcvTRUE;
            }

            /* Flushed the texture cache. */
            Hardware->hwTxFlushPS = gcvFALSE;
        }

        if (semaStall)
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* Send FE-PE stall token. */
            *memory++ =
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

            *memory++ =
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

            /* Dump the stall. */
            gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]",
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));
        }

        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }

    /* Mark texture as updated. */
    Hardware->textureDirty = gcvFALSE;
    Hardware->samplerDirty = 0;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#endif

