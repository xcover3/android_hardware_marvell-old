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
#include "gc_hal_engine.h"

#if gcdENABLE_3D

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE

#define gcdAUTO_SHIFT 0

#define gcmMAKE_F16(Sign, Exponent, Mantissa)  (gctUINT16)(((Sign) << 15) | ((Exponent) << 10) | (Mantissa))
static gctUINT16 _Float2Float16(IN gctUINT32 Value)
{
    gctUINT16 result;
    gctINT32 exponent = ((Value & 0x7f800000) >> 23) - 127;
    gctINT32 sign  = (Value & 0x80000000) >> 31;

    if (exponent <= -15)
    {
        /* Undeflow; set to zero. */
        result = (gctUINT16)gcmMAKE_F16(sign, 0, 0);
    }
    else if (exponent > 15)
    {
        /* Overflow; set to infinity. */
        result = (gctUINT16)gcmMAKE_F16(sign, 31, 0);
    }
    else
    {
        /* Strip a number of bits. */
        result = (gctUINT16)gcmMAKE_F16(sign, exponent + 15, (Value >> 13) & 0x03ff);
    }

    /* Return result. */
    return result;
}

static gceSTATUS
_AutoSetEarlyDepth(
    IN gcoHARDWARE Hardware,
    OUT gctBOOL_PTR Enable OPTIONAL
    )
{
    gctBOOL enable = Hardware->earlyDepth;

    if (!Hardware->features[gcvFEATURE_EARLY_Z]
        && !Hardware->features[gcvFEATURE_NEW_RA])
    {
        enable = gcvFALSE;
    }

    /* GC500 does not support early-depth when tile status is enabled at D16. */
    else
    if ((Hardware->config->chipModel == gcv500) &&
        (Hardware->config->chipRevision < 3) &&
        (Hardware->depthStates.surface != gcvNULL) &&
        (Hardware->depthStates.surface->format == gcvSURF_D16))
    {
        enable = gcvFALSE;
    }

    /* Early-depth doesn't work if depth compare is NOT EQUAL. */
    else
    if (Hardware->depthStates.compare == gcvCOMPARE_NOT_EQUAL)
    {
        enable = gcvFALSE;
    }

    /* Early-depth should be disabled when stencil mode is not set to keep. */
    else
    if ((Hardware->stencilStates.mode != gcvSTENCIL_NONE) &&
        (!Hardware->stencilKeepFront[0] ||
         !Hardware->stencilKeepFront[1] ||
         !Hardware->stencilKeepFront[2]))
    {
        enable = gcvFALSE;
    }

    /* Early-depth should be disabled when two-sided stencil mode is not set
    ** keep. */
    else
    if ((Hardware->stencilStates.mode == gcvSTENCIL_DOUBLE_SIDED) &&
        (!Hardware->stencilKeepBack[0] ||
         !Hardware->stencilKeepBack[1] ||
         !Hardware->stencilKeepBack[2]))
    {
        enable = gcvFALSE;
    }

    else
    if ((Hardware->depthStates.surface != gcvNULL) &&
        (!Hardware->depthStates.surface->hzDisabled))
    {
        enable = gcvFALSE;
    }

#if defined(ANDROID)
    if (Hardware->patchID == gcvPATCH_FISHNOODLE)
    {
        enable = gcvFALSE;
    }
#endif

    if (Enable != gcvNULL)
    {
        *Enable = enable;
    }

    if (Hardware->depthStates.early != enable)
    {
        Hardware->depthStates.early = enable;
        Hardware->depthConfigDirty  = gcvTRUE;
    }

    /* Success. */
    return gcvSTATUS_OK;
}

static gceSTATUS
_AutoSetColorAddressing(
    IN gcoHARDWARE Hardware
    )
{
    gctUINT i;
    gctBOOL singleBuffer8x4 = gcvFALSE;

    if ((Hardware->depthStates.surface) &&
        (Hardware->depthStates.surface->formatInfo.bitsPerPixel <= 16))
    {
        singleBuffer8x4 = gcvTRUE;
    }
    for (i = 0; i < Hardware->config->renderTargets; i++)
    {
        gcsSURF_INFO_PTR surface = Hardware->colorStates.target[i].surface;
        if ((surface != gcvNULL) &&
            (surface->formatInfo.bitsPerPixel <= 16))
        {
            singleBuffer8x4 = gcvTRUE;
            break;
        }
    }

    Hardware->singleBuffer8x4 = singleBuffer8x4;

    return gcvSTATUS_OK;
}



gceSTATUS
gcoHARDWARE_SetPSOutputMapping(
    IN gcoHARDWARE Hardware,
    IN gctINT32 * psOutputMapping
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x psOutputMapping=0x%x", Hardware, psOutputMapping);

    gcmGETHARDWARE(Hardware);

    gcoOS_MemCopy(Hardware->psOutputMapping, psOutputMapping, Hardware->config->renderTargets * sizeof(gctINT32));

    /* Dirty shader to reprogram ps output control */
    Hardware->shaderDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoHARDWARE_SetRenderTargetOffset(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 TargetIndex,
    IN gctUINT32 Offset
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x TargetIndex=%d Offset=0x%x", Hardware, TargetIndex, Offset);

    gcmASSERT(TargetIndex < Hardware->config->renderTargets);

    gcmGETHARDWARE(Hardware);

    Hardware->colorStates.target[TargetIndex].offset = Offset;

    /* Schedule the surface to be reprogrammed. */
    Hardware->colorConfigDirty = gcvTRUE;
    Hardware->colorTargetDirty = gcvTRUE;


OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoHARDWARE_SetRenderTarget(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 TargetIndex,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 LayerIndex
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x TargetIndex=%d Surface=0x%x LayerIndex=%d",
                   Hardware, TargetIndex, Surface, LayerIndex);

    gcmASSERT(TargetIndex < Hardware->config->renderTargets);

    gcmGETHARDWARE(Hardware);

    /* Set current render target. */
    Hardware->colorStates.target[TargetIndex].surface = Surface;
    Hardware->colorStates.target[TargetIndex].layerIndex = LayerIndex;

    if (Surface != gcvNULL)
    {
        /* Make sure the surface is locked. */
        gcmASSERT(Surface->node.valid);

        /* Save configuration. */
        Hardware->samples = Surface->samples;

        Hardware->vaa
            = Surface->vaa
                ? (Surface->format == gcvSURF_A8R8G8B8)
                    ? gcvVAA_COVERAGE_8
                    : gcvVAA_COVERAGE_16
                : gcvVAA_NONE;


        /* Alpha blend should be turned off explicitly by driver for integer RT*/
        if (Hardware->features[gcvFEATURE_HALTI2])
        {
            Hardware->alphaDirty = gcvTRUE;
        }
    }

    _AutoSetColorAddressing(Hardware);

    /* Schedule the surface to be reprogrammed. */
    Hardware->colorTargetDirty = gcvTRUE;
    Hardware->colorConfigDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthBufferOffset(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Offset
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Offset=0x%x", Hardware, Offset);

    gcmGETHARDWARE(Hardware);

    Hardware->depthStates.offset = Offset;

    Hardware->depthTargetDirty        = gcvTRUE;
    Hardware->depthConfigDirty        = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;

}


gceSTATUS
gcoHARDWARE_SetDepthBuffer(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x", Hardware, Surface);

    gcmGETHARDWARE(Hardware);

    /* Set current depth buffer. */
    Hardware->depthStates.surface = Surface;
    if (Surface)
    {
        /* Save configuration. */
        Hardware->samples = Surface->samples;
    }
    /* Set super-tiling. */
    Hardware->depthStates.regDepthConfig =
        ((((gctUINT32) (Hardware->depthStates.regDepthConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) ((Surface == gcvNULL) ? 0 : Surface->superTiled) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)));

    /* Enable/disable Early Depth. */
    gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

    gcmONERROR(_AutoSetColorAddressing(Hardware));

    /* Make depth and stencil dirty. */
    Hardware->depthConfigDirty        = gcvTRUE;
    Hardware->depthTargetDirty        = gcvTRUE;
    Hardware->depthNormalizationDirty = gcvTRUE;
    Hardware->stencilDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetViewport(
    IN gcoHARDWARE Hardware,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Left=%d Top=%d Right=%d Bottom=%d",
                  Hardware, Left, Top, Right, Bottom);

    gcmGETHARDWARE(Hardware);

    /* Save the states. */
    Hardware->viewportStates.left   = Left;
    Hardware->viewportStates.top    = Top;
    Hardware->viewportStates.right  = Right;
    Hardware->viewportStates.bottom = Bottom;
    Hardware->viewportDirty         = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetScissors(
    IN gcoHARDWARE Hardware,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Left=%d Top=%d Right=%d Bottom=%d",
                  Hardware, Left, Top, Right, Bottom);

    gcmGETHARDWARE(Hardware);

    /* Save the states. */
    Hardware->scissorStates.left   = Left;
    Hardware->scissorStates.top    = Top;
    Hardware->scissorStates.right  = Right;
    Hardware->scissorStates.bottom = Bottom;
    Hardware->scissorDirty         = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*------------------------------------------------------------------------------
**----- Alpha Blend States ---------------------------------------------------*/

gceSTATUS
gcoHARDWARE_SetBlendEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enabled
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enabled=%d", Hardware, Enabled);

    gcmGETHARDWARE(Hardware);

    /* Save the state. */
    Hardware->alphaStates.blend = Enabled;
    Hardware->alphaDirty        = gcvTRUE;
    Hardware->colorConfigDirty  = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendFunctionSource(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x FunctionRGB=%d FunctionAlpha=%d",
                  Hardware, FunctionRGB, FunctionAlpha);

    gcmGETHARDWARE(Hardware);

    /* Save the states. */
    Hardware->alphaStates.srcFuncColor = FunctionRGB;
    Hardware->alphaStates.srcFuncAlpha = FunctionAlpha;
    Hardware->alphaDirty               = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendFunctionTarget(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x FunctionRGB=%d FunctionAlpha=%d",
                  Hardware, FunctionRGB, FunctionAlpha);

    gcmGETHARDWARE(Hardware);

    /* Save the states. */
    Hardware->alphaStates.trgFuncColor = FunctionRGB;
    Hardware->alphaStates.trgFuncAlpha = FunctionAlpha;
    Hardware->alphaDirty               = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendMode(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_MODE ModeRGB,
    IN gceBLEND_MODE ModeAlpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x ModeRGB=%d ModeAlpha=%d",
                  Hardware, ModeRGB, ModeAlpha);

    gcmGETHARDWARE(Hardware);

    /* Save the states. */
    Hardware->alphaStates.modeColor = ModeRGB;
    Hardware->alphaStates.modeAlpha = ModeAlpha;
    Hardware->alphaDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Red,
    IN gctUINT8 Green,
    IN gctUINT8 Blue,
    IN gctUINT8 Alpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Red=%d Green=%d Blue=%d Alpha=%d",
                  Hardware, Red, Green, Blue, Alpha);

    gcmGETHARDWARE(Hardware);

    /* Save the state. */
    Hardware->alphaStates.color
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (Red) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (Green) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (Blue) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (Alpha) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)));
    Hardware->alphaDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendColorX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT8 red, green, blue, alpha;
    gctFIXED_POINT redSat, greenSat, blueSat, alphaSat;

    gcmHEADER_ARG("Hardware=0x%x Red=%d Green=%d Blue=%d Alpha=%d",
                  Hardware, Red, Green, Blue, Alpha);

    gcmGETHARDWARE(Hardware);

    /* Saturate fixed point values. */
    redSat   = gcmMIN(gcmMAX(Red, 0), gcvONE_X);
    greenSat = gcmMIN(gcmMAX(Green, 0), gcvONE_X);
    blueSat  = gcmMIN(gcmMAX(Blue, 0), gcvONE_X);
    alphaSat = gcmMIN(gcmMAX(Alpha, 0), gcvONE_X);

    /* Convert fixed point to 8-bit. */
    red   = (gctUINT8) (gcmXMultiply(redSat, (255 << 16)) >> 16);
    green = (gctUINT8) (gcmXMultiply(greenSat, (255 << 16)) >> 16);
    blue  = (gctUINT8) (gcmXMultiply(blueSat, (255 << 16)) >> 16);
    alpha = (gctUINT8) (gcmXMultiply(alphaSat, (255 << 16)) >> 16);

    /* Save the state. */
    Hardware->alphaStates.color
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (red) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (green) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (blue) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (alpha) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)));
    Hardware->alphaDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetBlendColorF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT8 red, green, blue, alpha;

    gcmHEADER_ARG("Hardware=0x%x Red=%f Green=%f Blue=%f Alpha=%f",
                  Hardware, Red, Green, Blue, Alpha);

    gcmGETHARDWARE(Hardware);

    /* Convert floating point to 8-bit. */
    red   = gcoMATH_Float2NormalizedUInt8(gcmMIN(gcmMAX(Red, 0.0f), 1.0f));
    green = gcoMATH_Float2NormalizedUInt8(gcmMIN(gcmMAX(Green, 0.0f), 1.0f));
    blue  = gcoMATH_Float2NormalizedUInt8(gcmMIN(gcmMAX(Blue, 0.0f), 1.0f));
    alpha = gcoMATH_Float2NormalizedUInt8(gcmMIN(gcmMAX(Alpha, 0.0f), 1.0f));

    /* Save the state. */
    Hardware->alphaStates.color
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (red) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (green) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (blue) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (alpha) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)));
    Hardware->alphaDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*------------------------------------------------------------------------------
**----- Depth States. --------------------------------------------------------*/

gceSTATUS
gcoHARDWARE_SetDepthCompare(
    IN gcoHARDWARE Hardware,
    IN gceCOMPARE DepthCompare
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x DepthCompare=%d", Hardware, DepthCompare);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the depth compare. */
    if (Hardware->depthStates.compare != DepthCompare)
    {
        Hardware->depthStates.compare = DepthCompare;

        /* Enable/disable Eearly Depth. */
        gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

        Hardware->depthConfigDirty    = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthWrite(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save write enable. */
    if (Hardware->depthStates.write != Enable)
    {
        Hardware->depthStates.write = Enable;
        Hardware->depthConfigDirty  = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthMode(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x DepthMode=%d", Hardware, DepthMode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save depth type. */
    if (Hardware->depthStates.mode != DepthMode)
    {
        Hardware->depthStates.mode = DepthMode;
        Hardware->depthConfigDirty = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthRangeX(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode,
    IN gctFIXED_POINT Near,
    IN gctFIXED_POINT Far
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x DepthMode=%d Near=%f Far=%f",
                  Hardware, DepthMode,
                  gcoMATH_Fixed2Float(Near),
                  gcoMATH_Fixed2Float(Far));

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save depth type. */
    Hardware->depthStates.mode = DepthMode;
    Hardware->depthStates.near = gcoMATH_Fixed2Float(Near);
    Hardware->depthStates.far  = gcoMATH_Fixed2Float(Far);
    Hardware->depthConfigDirty = gcvTRUE;
    Hardware->depthRangeDirty  = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthRangeF(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode,
    IN gctFLOAT Near,
    IN gctFLOAT Far
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x DepthMode=%d Near=%f Far=%f",
                  Hardware, DepthMode, Near, Far);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save depth type. */
    if (Hardware->depthStates.mode != DepthMode)
    {
        Hardware->depthStates.mode = DepthMode;
        Hardware->depthConfigDirty = gcvTRUE;
    }

    if ((Hardware->depthStates.near != Near)
     || (Hardware->depthStates.far  != Far))
    {
        Hardware->depthStates.near = Near;
        Hardware->depthStates.far  = Far;
        Hardware->depthRangeDirty  = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthPlaneF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Near,
    IN gctFLOAT Far
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Near=%f Far=%f",
                  Hardware, Near, Far);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save depth plane. */
    Hardware->depthNear.f      = Near;
    Hardware->depthFar.f       = Far;
    Hardware->depthConfigDirty = gcvTRUE;
    Hardware->depthRangeDirty  = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetLastPixelEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);


    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));
    gcmONERROR(gcoHARDWARE_LoadState32(
        Hardware, 0x00C18, Enable
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT DepthScale,
    IN gctFIXED_POINT DepthBias
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x DepthScale=%d DepthBias=%d",
                  Hardware, DepthScale, DepthBias);

    gcmGETHARDWARE(Hardware);

    if (!Hardware->features[gcvFEATURE_DEPTH_BIAS_FIX])
    {
        DepthScale = 0x0;
        DepthBias  = 0x0;
    }

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Program the scale register. */
    gcmONERROR(gcoHARDWARE_LoadState32x(
        Hardware, 0x00C10, DepthScale
        ));

    /* Program the bias register. */
    gcmONERROR(gcoHARDWARE_LoadState32x(
        Hardware, 0x00C14, DepthBias
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT DepthScale,
    IN gctFLOAT DepthBias
    )
{
    gceSTATUS status;
    gcuFLOAT_UINT32 scale, bias;

    gcmHEADER_ARG("Hardware=0x%x DepthScale=%f DepthBias=%f",
                  Hardware, DepthScale, DepthBias);

    gcmGETHARDWARE(Hardware);

    if (!Hardware->features[gcvFEATURE_DEPTH_BIAS_FIX])
    {
        scale.f = 0.0f;
        bias.f  = 0.0f;
    }
    else
    {
        scale.f = DepthScale;
        bias.f  = DepthBias;
    }

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Program the scale register. */
    gcmONERROR(gcoHARDWARE_LoadState32(
        gcvNULL, 0x00C10, scale.u
        ));

    /* Program the bias register. */
    gcmONERROR(gcoHARDWARE_LoadState32(
        gcvNULL, 0x00C14, bias.u
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetColorWrite(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Enable
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    if (Hardware->colorStates.colorWrite != Enable)
    {
        /* Reprogram PE dither table. */
        Hardware->peDitherDirty        = gcvTRUE;
    }

    /* Save the color write. */
    Hardware->colorStates.colorWrite = Enable;
    Hardware->colorConfigDirty       = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetEarlyDepth(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Only handle when the IP has Early Depth or NEW_RA. */
    if (Hardware->features[gcvFEATURE_EARLY_Z]
        || Hardware->features[gcvFEATURE_NEW_RA])
    {
        /* Set early depth flag. */
        Hardware->earlyDepth = Enable;

        /* Auto enable/disable early depth. */
        gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/* Used to disable all early-z culling modes
   and manage raster Z control states. */
gceSTATUS
gcoHARDWARE_SetAllEarlyDepthModes(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Disable,
    IN gctBOOL DisableRAModifyZ,
    IN gctBOOL DisableRAPassZ
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Disable=%d DisableRAModifyZ=%d DisableRAPassZ=%d ",
                   Hardware, Disable, DisableRAModifyZ, DisableRAPassZ);

    gcmGETHARDWARE(Hardware);

    if ((Hardware->disableAllEarlyDepth != Disable)
     || (Hardware->disableRAModifyZ     != DisableRAModifyZ)
     || (Hardware->disableRAPassZ       != DisableRAPassZ))
    {
        /* Set disable all early depth flag. */
        Hardware->disableAllEarlyDepth = Disable;
        Hardware->disableRAModifyZ     = DisableRAModifyZ;
        Hardware->disableRAPassZ       = DisableRAPassZ;
        Hardware->depthConfigDirty  = gcvTRUE;
        Hardware->depthTargetDirty  = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/* Used to switch early-z culling mode. */
gceSTATUS
gcoHARDWARE_SwitchDynamicEarlyDepthMode(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Set disable all early depth flag. */
    Hardware->disableAllEarlyDepthFromStatistics = !Hardware->disableAllEarlyDepthFromStatistics;
    Hardware->depthConfigDirty  = gcvTRUE;
    Hardware->depthTargetDirty  = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/* Used to set dynamic early-z culling mode. */
gceSTATUS
gcoHARDWARE_DisableDynamicEarlyDepthMode(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Disable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Disable=%d", Hardware, Disable);

    gcmGETHARDWARE(Hardware);

    if (Hardware->disableAllEarlyDepthFromStatistics != Disable)
    {
        /* Set disable all early depth flag. */
        Hardware->disableAllEarlyDepthFromStatistics = Disable;
        Hardware->depthConfigDirty  = gcvTRUE;
        Hardware->depthTargetDirty  = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}



/*------------------------------------------------------------------------------
**----- Stencil States -------------------------------------------------------*/

gceSTATUS
gcoHARDWARE_SetStencilMode(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_MODE Mode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Mode=%d", Hardware, Mode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save stencil mode. */
    Hardware->stencilStates.mode = Mode;
    Hardware->stencilDirty       = gcvTRUE;

    /* Enable/disable Eearly Depth. */
    gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mask=0x%x", Hardware, Mask);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil mask. */
    Hardware->stencilStates.maskFront = Mask;
    Hardware->stencilDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilMaskBack(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mask=0x%x", Hardware, Mask);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil mask. */
    Hardware->stencilStates.maskBack = Mask;
    Hardware->stencilDirty           = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilWriteMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mask=0x%x", Hardware, Mask);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil write mask. */
    Hardware->stencilStates.writeMaskFront = Mask;
    Hardware->stencilDirty                 = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilWriteMaskBack(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mask=0x%x", Hardware, Mask);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil write mask. */
    Hardware->stencilStates.writeMaskBack = Mask;
    Hardware->stencilDirty                = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilReference(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Reference,
    IN gctBOOL Front
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Reference=0x%x Front=%d",
                  Hardware, Reference, Front);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil reference. */
    if (Front)
    {
        Hardware->stencilStates.referenceFront = Reference;
    }
    else
    {
        Hardware->stencilStates.referenceBack = Reference;
    }
    Hardware->stencilDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilCompare(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceCOMPARE Compare
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Where=%d Compare=%d",
                  Hardware, Where, Compare);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil compare. */
    if (Where == gcvSTENCIL_FRONT)
    {
        Hardware->stencilStates.compareFront = Compare;
    }
    else
    {
        Hardware->stencilStates.compareBack = Compare;
    }
    Hardware->stencilDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilPass(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Where=%d Operation=%d",
                  Hardware, Where, Operation);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil pass operation. */
    if (Where == gcvSTENCIL_FRONT)
    {
        Hardware->stencilStates.passFront = Operation;
        Hardware->stencilKeepFront[0]     = (Operation == gcvSTENCIL_KEEP);
    }
    else
    {
        Hardware->stencilStates.passBack = Operation;
        Hardware->stencilKeepBack[0]     = (Operation == gcvSTENCIL_KEEP);
    }

    /* Enable/disable Eearly Depth. */
    gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

    /* Invalidate stencil. */
    Hardware->stencilDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilFail(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Where=%d Operation=%d",
                  Hardware, Where, Operation);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil fail operation. */
    if (Where == gcvSTENCIL_FRONT)
    {
        Hardware->stencilStates.failFront = Operation;
        Hardware->stencilKeepFront[1]     = (Operation == gcvSTENCIL_KEEP);
    }
    else
    {
        Hardware->stencilStates.failBack = Operation;
        Hardware->stencilKeepBack[1]     = (Operation == gcvSTENCIL_KEEP);
    }

    /* Enable/disable Eearly Depth. */
    gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

    /* Invalidate stencil. */
    Hardware->stencilDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilDepthFail(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Where=%d Operation=%d",
                  Hardware, Where, Operation);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the stencil depth fail operation. */
    if (Where == gcvSTENCIL_FRONT)
    {
        Hardware->stencilStates.depthFailFront = Operation;
        Hardware->stencilKeepFront[2]          = (Operation == gcvSTENCIL_KEEP);
    }
    else
    {
        Hardware->stencilStates.depthFailBack = Operation;
        Hardware->stencilKeepBack[2]          = (Operation == gcvSTENCIL_KEEP);
    }

    /* Enable/disable Eearly Depth. */
    gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

    /* Invalidate stencil. */
    Hardware->stencilDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetStencilAll(
    IN gcoHARDWARE Hardware,
    IN gcsSTENCIL_INFO_PTR Info
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Info=0x%x", Hardware, Info);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Info != gcvNULL);

    /* We only need to update stencil states if there is a stencil buffer. */
    if ((Hardware->depthStates.surface         != gcvNULL)
    &&  (Hardware->depthStates.surface->format == gcvSURF_D24S8 ||
         Hardware->depthStates.surface->format == gcvSURF_S8)
    )
    {
        /* Copy the states. */
       Hardware->stencilStates = *Info;

        /* Some more updated based on a few states. */
        Hardware->stencilKeepFront[0]
            = (Info->passFront      == gcvSTENCIL_KEEP);
        Hardware->stencilKeepFront[1]
            = (Info->failFront      == gcvSTENCIL_KEEP);
        Hardware->stencilKeepFront[2]
            = (Info->depthFailFront == gcvSTENCIL_KEEP);

        Hardware->stencilKeepBack[0]
            = (Info->passBack      == gcvSTENCIL_KEEP);
        Hardware->stencilKeepBack[1]
            = (Info->failBack      == gcvSTENCIL_KEEP);
        Hardware->stencilKeepBack[2]
            = (Info->depthFailBack == gcvSTENCIL_KEEP);

        /* Enable/disable Eearly Depth. */
        gcmONERROR(_AutoSetEarlyDepth(Hardware, gcvNULL));

        /* Mark stencil states as dirty. */
        Hardware->stencilDirty = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*------------------------------------------------------------------------------
**----- Alpha Test States ----------------------------------------------------*/

gceSTATUS
gcoHARDWARE_SetAlphaTest(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the state. */
    Hardware->alphaStates.test = Enable;
    Hardware->alphaDirty       = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAlphaCompare(
    IN gcoHARDWARE Hardware,
    IN gceCOMPARE Compare
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Compare=%d", Hardware, Compare);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the state. */
    Hardware->alphaStates.compare = Compare;
    Hardware->alphaDirty          = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAlphaReference(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Reference,
    IN gctFLOAT FloatReference
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Reference=%u FloatReference=%f", Hardware, Reference, FloatReference);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the state. */
    Hardware->alphaStates.reference = Reference;
    Hardware->alphaStates.floatReference = FloatReference;
    Hardware->alphaDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAlphaReferenceX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Reference
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctFIXED_POINT saturated;
    gctUINT8 reference;

    gcmHEADER_ARG("Hardware=0x%x Reference=%f",
                  Hardware, gcoMATH_Fixed2Float(Reference));

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Saturate fixed point between 0 and 1. */
    saturated = gcmMIN(gcmMAX(Reference, 0), gcvONE_X);

    /* Convert fixed point to gctUINT8. */
    reference = (gctUINT8) (gcmXMultiply(saturated, 255 << 16) >> 16);

    /* Save the state. */
    Hardware->alphaStates.reference = reference;
    *(gctUINT32 *)&(Hardware->alphaStates.floatReference) = 0xFFFFFFFF; /*Set a Invalid value for fixed pipe*/
    Hardware->alphaDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAlphaReferenceF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Reference
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctFLOAT saturated;
    gctUINT8 reference;

    gcmHEADER_ARG("Hardware=0x%x Reference=%f", Hardware, Reference);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Saturate floating point between 0 and 1. */
    saturated = gcmMIN(gcmMAX(Reference, 0.0f), 1.0f);

    /* Convert floating point to gctUINT8. */
    reference = gcoMATH_Float2NormalizedUInt8(saturated);

    /* Save the state. */
    Hardware->alphaStates.reference = reference;
    Hardware->alphaStates.floatReference = Reference;
    Hardware->alphaDirty            = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAlphaAll(
    IN gcoHARDWARE Hardware,
    IN gcsALPHA_INFO_PTR Info
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Info=0x%x", Hardware, Info);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Info != gcvNULL);

    /* Copy the states. */
    gcoOS_MemCopy(&Hardware->alphaStates, Info, gcmSIZEOF(Hardware->alphaStates));

    /* Mark alpha states as dirty. */
    Hardware->alphaDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*------------------------------------------------------------------------------
**----------------------------------------------------------------------------*/

gceSTATUS
gcoHARDWARE_BindIndex(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 HeadAddress,
    IN gctUINT32 TailAddress,
    IN gceINDEX_TYPE IndexType,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x HeadAddress=0x%x TailAddress=0x%x IndexType=%d",
                  Hardware, HeadAddress, TailAddress, IndexType);

    gcmGETHARDWARE(Hardware);

    Hardware->indexEndian = 0x0;

    switch (IndexType)
    {
    case gcvINDEX_8:
        Hardware->indexFormat = 0x0;
        Hardware->restartElement = 0xFF;
        break;

    case gcvINDEX_16:
        Hardware->indexFormat = 0x1;
        Hardware->restartElement = 0xFFFF;

        if (Hardware->bigEndian)
        {
            Hardware->indexEndian = 0x1;
        }
        break;

    case gcvINDEX_32:
        if (((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 31:31) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))))
        {
            Hardware->indexFormat = 0x2;
            Hardware->restartElement = 0xFFFFFFFF;

            if (Hardware->bigEndian)
            {
                Hardware->indexEndian = 0x1;
            }
            break;
        }

        /* Fall through. */
    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Save AQ_INDEX_STREAM_CTRL register value. */
    if(HeadAddress)
    {
        Hardware->indexHeadAddress = HeadAddress;
        Hardware->indexHeadDirty = gcvTRUE;
    }
    if(TailAddress)
    {
        Hardware->indexTailAddress = TailAddress;
        Hardware->indexTailDirty = gcvTRUE;
    }

    Hardware->indexDirty = Hardware->indexHeadDirty | Hardware->indexTailDirty;

#if gcdSTREAM_OUT_BUFFER

    /* Allocate memory for stream out indices */
    if (Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_1
        || Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_1_PREPASS)
    {
        /* Allocate and Push new space for stream out indices */
        gcoHARDWARE_AddStreamOutIndex (Hardware, Bytes);

        /* Set stream out dirty */
        Hardware->streamOutDirty = gcvTRUE;
    }
#endif

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAPI(
    IN gcoHARDWARE Hardware,
    IN gceAPI Api
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 system = 0;

    gcmHEADER_ARG("Hardware=0x%x Api=%d", Hardware, Api);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Don't do anything if the pipe has already been selected. */
    if (Hardware->currentApi != Api)
    {
        /* Set new pipe. */
        Hardware->currentApi = Api;

        /* Set PA System Mode: D3D or OPENGL. */
        switch (Api)
        {
        case gcvAPI_OPENGL_ES11:
        case gcvAPI_OPENGL_ES20:
        case gcvAPI_OPENGL_ES30:
        case gcvAPI_OPENGL:
        case gcvAPI_OPENVG:
        case gcvAPI_OPENCL:
            Hardware->api = gcvAPI_OPENGL;
            system = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                   | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));
            break;

        case gcvAPI_D3D:
            Hardware->api = gcvAPI_D3D;
            system = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                   | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));
            break;

        default:
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        Hardware->centroidsDirty = gcvTRUE;

        gcmONERROR(gcoHARDWARE_LoadState32(Hardware, 0x00A28, system));

        /* Set Common API Mode: OpenGL, OpenVG, or OpenCL.
           This is for VS and PS to handle texel conversion. */
        switch (Api)
        {
        case gcvAPI_OPENGL_ES11:
        case gcvAPI_OPENGL_ES20:
        case gcvAPI_OPENGL_ES30:
        case gcvAPI_OPENGL:
            system = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcvAPI_OPENVG:
            system = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcvAPI_OPENCL:
            Hardware->api = gcvAPI_OPENCL;
            system = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        default:
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        gcmONERROR(gcoHARDWARE_LoadState32(Hardware, 0x0384C, system));
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetAPI(
    IN gcoHARDWARE Hardware,
    OUT gceAPI * CurrentApi,
    OUT gceAPI * Api
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (CurrentApi)
    {
        *CurrentApi = Hardware->currentApi;
    }

    if (Api)
    {
        *Api = Hardware->api;
    }

    /* Success. */
    gcmFOOTER_ARG("*CurrentApi=0x%x *Api=0x%x",
                  gcmOPT_VALUE(CurrentApi), gcmOPT_VALUE(Api));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*------------------------------------------------------------------------------
**----- Primitive assembly states. -------------------------------------------*/

gceSTATUS
gcoHARDWARE_SetAntiAliasLine(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.aaLine = Enable;
    Hardware->paConfigDirty   = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAALineTexSlot(
    IN gcoHARDWARE Hardware,
    IN gctUINT TexSlot
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x TexSlot=%d", Hardware, TexSlot);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.aaLineTexSlot = TexSlot;
    Hardware->paConfigDirty          = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetShading(
    IN gcoHARDWARE Hardware,
    IN gceSHADING Shading
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Shading=%d", Hardware, Shading);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.shading = Shading;
    Hardware->paConfigDirty    = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetCulling(
    IN gcoHARDWARE Hardware,
    IN gceCULL Mode
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mode=%d", Hardware, Mode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    if (Hardware->paStates.culling != Mode)
    {
        Hardware->paStates.culling = Mode;
        Hardware->paConfigDirty    = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetPointSizeEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.pointSize = Enable;
    Hardware->paConfigDirty      = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetPointSprite(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.pointSprite = Enable;
    Hardware->paConfigDirty        = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetFill(
    IN gcoHARDWARE Hardware,
    IN gceFILL Mode
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mode=%d", Hardware, Mode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.fillMode = Mode;
    Hardware->paConfigDirty     = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetAALineWidth(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Width
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Width=%f", Hardware, Width);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    Hardware->paStates.aaLineWidth = Width;
    Hardware->paLineDirty          = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdMULTI_GPU
gceSTATUS
gcoHARDWARE_SetMultiGPURenderingMode(
    IN gcoHARDWARE Hardware,
    IN gceMULTI_GPU_RENDERING_MODE Mode
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Mode=%d", Hardware, Mode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the multi GPU rendering mode. */
    Hardware->gpuRenderingMode = Mode;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_SetMultiGPURenderingMode(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gceMULTI_GPU_RENDERING_MODE Mode,
    INOUT gctPOINTER * Memory
    )
{
    gceSTATUS    status = gcvSTATUS_OK;
    gctUINT32    gpuCoreCount;
    gcsRECT     *renderWindow = gcvNULL;

    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Mode=%d Memory=0x%x", Hardware, Surface, Mode, Memory);

    gpuCoreCount = Hardware->config->gpuCoreCount;

    if (Surface == gcvNULL)
    {
        Mode = gcvMULTI_GPU_RENDERING_MODE_OFF;
    }

    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    switch (Mode)
    {
    case gcvMULTI_GPU_RENDERING_MODE_OFF:
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E80) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E80, 0x0);    gcmENDSTATEBATCH_NEW(reserve, memory);};
        break;

    case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_64x64:
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E80) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E80, 0x4);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        break;

    case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_128x64:
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E80) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E80, 0x5);    gcmENDSTATEBATCH_NEW(reserve, memory);};
        break;

    case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_128x128:
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E80) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E80, 0x6);    gcmENDSTATEBATCH_NEW(reserve, memory);};
        break;
    case gcvMULTI_GPU_RENDERING_MODE_SPLIT_WIDTH:
    case gcvMULTI_GPU_RENDERING_MODE_SPLIT_HEIGHT:
    default:
        {
            gctUINT32 i;

            gcmASSERT(Surface);

            gcmVERIFY_OK(
                gcoOS_AllocateMemory(gcvNULL,
                                     sizeof(gcsRECT) * gpuCoreCount,
                                     (gctPOINTER *)&renderWindow));

            if (Mode == gcvMULTI_GPU_RENDERING_MODE_SPLIT_WIDTH)
            {
                for (i = 0; i < gpuCoreCount; i++)
                {
                    if (i == 0)
                    {
                        renderWindow[i].left   = 0;
                        renderWindow[i].top    = 0;
                        renderWindow[i].right  = gcmALIGN(Surface->alignedWidth
                                                          / gpuCoreCount, 64);
                        renderWindow[i].bottom = Surface->alignedHeight;
                    }
                    else
                    {
                        renderWindow[i].left   = renderWindow[i - 1].right;
                        renderWindow[i].top    = 0;
                        renderWindow[i].right  =
                            (i == (gpuCoreCount - 1)) ? Surface->alignedWidth
                                                      : (gcmALIGN(Surface->alignedWidth
                                                         / gpuCoreCount, 64)
                                                        * (i + 1));
                        renderWindow[i].bottom = Surface->alignedHeight;
                    }
                }
            }
            else if (Mode == gcvMULTI_GPU_RENDERING_MODE_SPLIT_HEIGHT)
            {
                for (i = 0; i < gpuCoreCount; i++)
                {
                    if (i == 0)
                    {
                        renderWindow[i].left   = 0;
                        renderWindow[i].top    = 0;
                        renderWindow[i].right  = Surface->alignedWidth;
                        renderWindow[i].bottom = gcmALIGN(Surface->alignedHeight
                                                          / gpuCoreCount, 64);
                    }
                    else
                    {
                        renderWindow[i].left   = 0;
                        renderWindow[i].top    = renderWindow[i - 1].bottom;
                        renderWindow[i].right  = Surface->alignedWidth;
                        renderWindow[i].bottom =
                            (i == (gpuCoreCount - 1)) ? Surface->alignedHeight
                                                      : (gcmALIGN(Surface->alignedHeight
                                                                  / gpuCoreCount, 64)
                                                        * (i + 1));
                    }
                }
            }

             {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E80) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E80, 0x1);    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* Set render windows for each 3D core. */
            for (i = 0; i < gpuCoreCount; i++)
            {
                {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (1 << i); memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", (1 << i)); };


                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E81) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E81, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (renderWindow[i].left) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (renderWindow[i].top) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))));    gcmENDSTATEBATCH_NEW(reserve, memory);};

                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E82) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E82, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (renderWindow[i].right) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (renderWindow[i].bottom) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))));    gcmENDSTATEBATCH_NEW(reserve, memory);};

            }

            /* Enable all 3D cores. */
            {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };

            break;
        }
    }

    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    if (renderWindow != gcvNULL)
    {
        gcoOS_FreeMemory(gcvNULL, (gctPOINTER)renderWindow);
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*------------------------------------------------------------------------------
**----------------------------------------------------------------------------*/

gceSTATUS
gcoHARDWARE_Initialize3D(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;
    gctFLOAT limit;
    gctUINT32 paControl = 0x00000000;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* AQVertexElementConfig. */
    gcmONERROR(gcoHARDWARE_LoadState32(
        Hardware, 0x03814,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
        ));

    /* AQRasterControl. */
    gcmONERROR(gcoHARDWARE_LoadState32(
        Hardware, 0x00E00,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        ));

    /* W Clip limit. */
    limit = 1.0f / 8388607.0f;
    gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
                                       0x00A2C,
                                       *(gctUINT32 *) &limit));

    /* Color conversion control initial state. */
    gcmONERROR(gcoHARDWARE_LoadState32WithMask(Hardware,
        0x014A4,
        (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))),
        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))))
        ));

    if (Hardware->config->chipModel == gcv1000 && Hardware->config->chipRevision < 0x5035)
    {
        /* gcregRAControlRegAddrs. */
        gcmONERROR(gcoHARDWARE_LoadState32(
            gcvNULL, 0x00E08,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            ));
    }

    if (Hardware->features[gcvFEATURE_HALTI2])
    {
        /* 0x0383 */
        gcmONERROR(gcoHARDWARE_LoadState32(
            Hardware, 0x00E0C, 0));
    }

    if (Hardware->features[gcvFEATURE_PA_FARZCLIPPING_FIX])
    {
        Hardware->depthStates.regDepthConfig = ((((gctUINT32) (Hardware->depthStates.regDepthConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18)));

    }
    else
    {
        paControl = ((((gctUINT32) (paControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)));
        /* Disable Far Z clipping. */
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
                                           0x00A88,
                                           paControl));
    }

    if (Hardware->features[gcvFEATURE_ZSCALE_FIX] &&
        gcoHAL_GetOption(gcvNULL, gcvOPTION_PREFER_ZCONVERT_BYPASS))
    {
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
                                           0x00A88,
                                           ((((gctUINT32) (paControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))));
    }


OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetDepthOnly(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Save the state. */
    Hardware->depthStates.only = Enable;
    Hardware->depthConfigDirty = gcvTRUE;

    /* Mark shaders as dirty. */
    Hardware->shaderDirty = gcvTRUE;

    if (Enable)
    {
        gcmASSERT(Hardware->depthStates.surface != gcvNULL);

        Hardware->vaa             = gcvVAA_NONE;
        Hardware->samples         = Hardware->depthStates.surface->samples;
        Hardware->msaaConfigDirty = gcvTRUE;
        Hardware->msaaModeDirty   = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetCentroids(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Index,
    IN gctPOINTER Centroids
    )
{
    gceSTATUS status;
    /*gcoHARDWARE hardware;*/
    gctUINT32 data[4], i;
    gctUINT8_PTR inputCentroids = (gctUINT8_PTR) Centroids;

    gcmHEADER_ARG("Index=%d Centroids=0x%x",
                    Index, Centroids);

    /*gcmGETHARDWARE(hardware);*/

    /* Verify the arguments. */
    /*gcmVERIFY_OBJECT(hardware, gcvOBJ_HARDWARE);*/

    gcoOS_ZeroMemory(data, sizeof(data));

    for (i = 0; i < 16; ++i)
    {
        switch (i & 3)
        {
        case 0:
            /* Set for centroid 0, 4, 8, or 12. */
            data[i / 4] = ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2]) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                             | ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2 + 1]) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
            break;

        case 1:
            /* Set for centroid 1, 5, 9, or 13. */
            data[i / 4] = ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                             | ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2 + 1]) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)));
            break;

        case 2:
            /* Set for centroid 2, 6, 10, or 14. */
            data[i / 4] = ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2]) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
                             | ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2 + 1]) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)));
            break;

        case 3:
            /* Set for centroid 3, 7, 11, or 15. */
            data[i / 4] = ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2]) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
                             | ((((gctUINT32) (data[i / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (inputCentroids[i * 2 + 1]) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));
            break;
        }
    }

    /* Program the centroid table. */
    gcmONERROR(gcoHARDWARE_LoadState(
                                     Hardware,
                                     0x00E40 + Index * 16, 4, data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_QuerySurfaceRenderable(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    )
{
    gceSTATUS status;
    gctUINT32 format;
    gcsSURF_FORMAT_INFO_PTR info[2];

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x", Hardware, Surface);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    gcmONERROR(gcoSURF_QueryFormat(Surface->format, info));

    do
    {
        if ((Surface->type == gcvSURF_TEXTURE) &&
            (!(Surface->tiling & gcvTILING_SPLIT_BUFFER)) &&
            (Hardware->config->pixelPipes > 1) &&
            (!Hardware->features[gcvFEATURE_SUPERTILED_TEXTURE]))
        {
            status = gcvSTATUS_NOT_ALIGNED;
            break;
        }

        if ((Hardware->config->pixelPipes > 1)
        && (!(Surface->tiling & gcvTILING_SPLIT_BUFFER))
        && !Hardware->features[gcvFEATURE_SINGLE_BUFFER])
        {
            status = gcvSTATUS_NOT_MULTI_PIPE_ALIGNED;
            break;
        }

        if ((Surface->tiling == gcvLINEAR)
        && !Hardware->features[gcvFEATURE_LINEAR_RENDER_TARGET])
        {
            /* Linear render target not supported. */
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* If surface is 16-bit, it has to be 8x4 aligned. */
        if (Surface->is16Bit)
        {
            if ((Surface->alignedWidth  & 7)
            ||  (Surface->alignedHeight & 3)
            )
            {
                 status = gcvSTATUS_NOT_ALIGNED;
                 break;
            }
        }

        /* Otherwise, the surface has to be 16x4 aligned. */
        else
        {
            if ((Surface->alignedWidth  & 15)
            ||  (Surface->alignedHeight &  3)
            )
            {
                 status = gcvSTATUS_NOT_ALIGNED;
                 break;
            }
        }

        /* Convert the surface format to a hardware format. */
        format = Surface->formatInfo.renderFormat;

        if (format == gcvINVALID_RENDER_FORMAT)
        {
            switch (Surface->format)
            {
            case gcvSURF_D16:
            case gcvSURF_D24X8:
            case gcvSURF_D24S8:
            case gcvSURF_D32:
            case gcvSURF_S8:
                status = gcvSTATUS_OK;
                break;

            default:
                status = gcvSTATUS_NOT_SUPPORTED;
                break;
            }
        }
    } while (gcvFALSE);

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetLogicOp(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Rop
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Rop=%d", Hardware, Rop);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Update ROP2 code. */
    Hardware->colorStates.rop = Rop & 0xF;

    /* Schedule the surface to be reprogrammed. */
    if (Hardware->colorStates.rop != 0xC)
    {
        Hardware->colorConfigDirty = gcvTRUE;
    }

    /* Program the scale register. */
    gcmONERROR(gcoHARDWARE_LoadState32WithMask(
        gcvNULL,
        0x014A4,
        (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))),
        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.rop) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))))
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/******************************************************************************\
******************************** State Flushers ********************************
\******************************************************************************/

gceSTATUS
gcoHARDWARE_FlushViewport(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS   status;
    gcsRECT     viewport;
    gctUINT32   xScale, yScale, xOffset, yOffset;
    gctFLOAT    wClip;
    gctFLOAT    wSmall;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if(Hardware->features[gcvFEATURE_PA_FARZCLIPPING_FIX])
    {
        wSmall = 0.0f;
    }
    else
    {
        wSmall = 1.0f / 32768.0f;
    }


    if (Hardware->viewportDirty)
    {
        if (((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))))
        {
            viewport.left   = Hardware->viewportStates.left   * Hardware->samples.x;
            viewport.top    = Hardware->viewportStates.top    * Hardware->samples.y;
            viewport.right  = Hardware->viewportStates.right  * Hardware->samples.x;
            viewport.bottom = Hardware->viewportStates.bottom * Hardware->samples.y;
        }
        else
        {
            viewport.left   = Hardware->viewportStates.left;
            viewport.top    = Hardware->viewportStates.top;
            viewport.right  = Hardware->viewportStates.right;
            viewport.bottom = Hardware->viewportStates.bottom;
        }

        /* Compute viewport states. */
        xScale  = (viewport.right - viewport.left) << 15;
        xOffset = (viewport.left << 16) + xScale;

        if (viewport.top < viewport.bottom)
        {
            yScale = (Hardware->api == gcvAPI_OPENGL)
                         ? (-(viewport.bottom - viewport.top) << 15)
                         : ((viewport.bottom - viewport.top) << 15);
            yOffset = (Hardware->api == gcvAPI_OPENGL)
                          ? (viewport.top << 16) - yScale
                          : (viewport.top << 16) + yScale;
        }
        else
        {
            yScale = (Hardware->api == gcvAPI_OPENGL)
                         ? ((viewport.top - viewport.bottom) << 15)
                         : (-(viewport.top - viewport.bottom) << 15);
            yOffset = (Hardware->api == gcvAPI_OPENGL)
                          ? (viewport.bottom << 16) + yScale
                          : (viewport.bottom << 16) - yScale;
        }

        if ((Hardware->config->chipModel == gcv500) &&
             (Hardware->api == gcvAPI_OPENGL) )
        {
            xOffset -= 0x8000;
            yOffset -= 0x8000;
        }

        wClip = gcmMAX((Hardware->viewportStates.right - Hardware->viewportStates.left),
                       (Hardware->viewportStates.bottom - Hardware->viewportStates.top
                        )) / (2.0f * 8388607.0f - 8192.0f);


        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0280) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0280,
                xScale
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0281,
                yScale
                );

            gcmSETFILLER_NEW(
                reserve, memory
                );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0283) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0283,
                xOffset
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0284,
                yOffset
                );

            gcmSETFILLER_NEW(
                reserve, memory
                );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* New PA clip registers. */
        if (Hardware->config->chipModel == gcv4000 && Hardware->config->chipRevision == 0x5222)
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x02A0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x02A0, 0xFFFFFFFF);    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
        else
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x02A0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x02A0, *(gctUINT32_PTR) &wClip);    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x02A1) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvTRUE, 0x02A1, 8192 << 16);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x02A3) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x02A3, *(gctUINT32_PTR) &wSmall);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

        /* Mark viewport as updated. */
        Hardware->viewportDirty = gcvFALSE;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushScissor(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status;
    gcsRECT scissor;
    gctINT fixRightClip;
    gctINT fixBottomClip;
    gcePATCH_ID patchId = gcvPATCH_INVALID;
    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    if (Hardware->scissorDirty)
    {
        if (((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))))
        {
            scissor.left   = Hardware->scissorStates.left   * Hardware->samples.x;
            scissor.top    = Hardware->scissorStates.top    * Hardware->samples.y;
            scissor.right  = Hardware->scissorStates.right  * Hardware->samples.x;
            scissor.bottom = Hardware->scissorStates.bottom * Hardware->samples.y;
        }
        else
        {
            scissor.left   = Hardware->scissorStates.left;
            scissor.top    = Hardware->scissorStates.top;
            scissor.right  = Hardware->scissorStates.right;
            scissor.bottom = Hardware->scissorStates.bottom;
        }

        /* Trivial case. */
        if ((scissor.left >= scissor.right) || (scissor.top >= scissor.bottom))
        {
            scissor.left   = 1;
            scissor.top    = 1;
            scissor.right  = 1;
            scissor.bottom = 1;
        }

        gcoHARDWARE_GetPatchID(gcvNULL, &patchId);

        if(patchId == gcvPATCH_GLBM21)
        {
            fixRightClip    = 0;
        }
        else
        {
            fixRightClip    = (Hardware->vaa == gcvVAA_NONE) ? 0x1119 : 0;
        }

        fixBottomClip   = (Hardware->vaa == gcvVAA_NONE) ? 0x1111 : 0;

        /* If right or bottom equal 8192, hw will overflow.
           We should decrease the size of Scissor.*/
        if(scissor.right == 8192)
        {
            fixRightClip = -fixRightClip;
        }

        if (scissor.bottom == 8192)
        {
            fixBottomClip = -fixBottomClip;
        }

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)4  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (4 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0300) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0300,
                scissor.left << 16
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0301,
                scissor.top << 16
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0302,
                (scissor.right << 16) + fixRightClip
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvTRUE, 0x0303,
                (scissor.bottom << 16) + fixBottomClip
                );

            gcmSETFILLER_NEW(
                reserve, memory
                );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* Program new states for setup clipping. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0308) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvTRUE, 0x0308, (scissor.right << 16) + 0xFFFF);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0309) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvTRUE, 0x0309, (scissor.bottom << 16) + 0xFFFF);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

        /* Mark viewport as updated. */
        Hardware->scissorDirty = gcvFALSE;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gctBOOL _canEnableAlphaBlend(gcoHARDWARE Hardware, gcsSURF_INFO_PTR rtSurf, gctBOOL originAlphaBlend)
{
    gctBOOL canAlphaBlend;

    if (rtSurf == gcvNULL)
        return originAlphaBlend;

    switch (rtSurf->formatInfo.fmtDataType)
    {
    case gcvFORMAT_DATATYPE_UNSIGNED_INTEGER:
    case gcvFORMAT_DATATYPE_SIGNED_INTEGER:
        canAlphaBlend = gcvFALSE;
        break;
    default:
        canAlphaBlend = originAlphaBlend;
        break;
    }


    return canAlphaBlend;

}

gceSTATUS
gcoHARDWARE_FlushAlpha(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gctBOOL floatPipe = gcvFALSE;

    static const gctINT32 xlateCompare[] =
    {
        /* gcvCOMPARE_INVALID */
        -1,
        /* gcvCOMPARE_NEVER */
        0x0,
        /* gcvCOMPARE_NOT_EQUAL */
        0x5,
        /* gcvCOMPARE_LESS */
        0x1,
        /* gcvCOMPARE_LESS_OR_EQUAL */
        0x3,
        /* gcvCOMPARE_EQUAL */
        0x2,
        /* gcvCOMPARE_GREATER */
        0x4,
        /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6,
        /* gcvCOMPARE_ALWAYS */
        0x7,
    };

    static const gctUINT32 xlateFuncSource[] =
    {
        /* gcvBLEND_ZERO */
        0x0,
        /* gcvBLEND_ONE */
        0x1,
        /* gcvBLEND_SOURCE_COLOR */
        0x2,
        /* gcvBLEND_INV_SOURCE_COLOR */
        0x3,
        /* gcvBLEND_SOURCE_ALPHA */
        0x4,
        /* gcvBLEND_INV_SOURCE_ALPHA */
        0x5,
        /* gcvBLEND_TARGET_COLOR */
        0x8,
        /* gcvBLEND_INV_TARGET_COLOR */
        0x9,
        /* gcvBLEND_TARGET_ALPHA */
        0x6,
        /* gcvBLEND_INV_TARGET_ALPHA */
        0x7,
        /* gcvBLEND_SOURCE_ALPHA_SATURATE */
        0xA,
        /* gcvBLEND_CONST_COLOR */
        0xD,
        /* gcvBLEND_INV_CONST_COLOR */
        0xE,
        /* gcvBLEND_CONST_ALPHA */
        0xB,
        /* gcvBLEND_INV_CONST_ALPHA */
        0xC,
    };

    static const gctUINT32 xlateFuncTarget[] =
    {
        /* gcvBLEND_ZERO */
        0x0,
        /* gcvBLEND_ONE */
        0x1,
        /* gcvBLEND_SOURCE_COLOR */
        0x2,
        /* gcvBLEND_INV_SOURCE_COLOR */
        0x3,
        /* gcvBLEND_SOURCE_ALPHA */
        0x4,
        /* gcvBLEND_INV_SOURCE_ALPHA */
        0x5,
        /* gcvBLEND_TARGET_COLOR */
        0x8,
        /* gcvBLEND_INV_TARGET_COLOR */
        0x9,
        /* gcvBLEND_TARGET_ALPHA */
        0x6,
        /* gcvBLEND_INV_TARGET_ALPHA */
        0x7,
        /* gcvBLEND_SOURCE_ALPHA_SATURATE */
        0xA,
        /* gcvBLEND_CONST_COLOR */
        0xD,
        /* gcvBLEND_INV_CONST_COLOR */
        0xE,
        /* gcvBLEND_CONST_ALPHA */
        0xB,
        /* gcvBLEND_INV_CONST_ALPHA */
        0xC,
    };

    static const gctUINT32 xlateMode[] =
    {
        /* gcvBLEND_ADD */
        0x0,
        /* gcvBLEND_SUBTRACT */
        0x1,
        /* gcvBLEND_REVERSE_SUBTRACT */
        0x2,
        /* gcvBLEND_MIN */
        0x3,
        /* gcvBLEND_MAX */
        0x4,
    };

    gceSTATUS status;

    gctBOOL canEnableAlphaBlend;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->alphaDirty)
    {
        /* Determine the size of the buffer to reserve. */
        floatPipe = Hardware->features[gcvFEATURE_HALF_FLOAT_PIPE];

        canEnableAlphaBlend = _canEnableAlphaBlend(Hardware,
                                                   Hardware->colorStates.target[0].surface,
                                                   Hardware->alphaStates.blend);
        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)3  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0508) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0508,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (Hardware->alphaStates.test) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4))) | (((gctUINT32) ((gctUINT32) (xlateCompare[Hardware->alphaStates.compare]) & ((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (Hardware->alphaStates.reference) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0509,
                Hardware->alphaStates.color
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x050A,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (canEnableAlphaBlend) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (canEnableAlphaBlend) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (xlateFuncSource[Hardware->alphaStates.srcFuncColor]) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (xlateFuncSource[Hardware->alphaStates.srcFuncAlpha]) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (xlateFuncTarget[Hardware->alphaStates.trgFuncColor]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (xlateFuncTarget[Hardware->alphaStates.trgFuncAlpha]) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (xlateMode[Hardware->alphaStates.modeColor]) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (xlateMode[Hardware->alphaStates.modeAlpha]) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)))
                );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

            if (floatPipe)
            {
                gctUINT32 colorLow;
                gctUINT32 colorHeigh;
                gctUINT16 colorR;
                gctUINT16 colorG;
                gctUINT16 colorB;
                gctUINT16 colorA;

                colorR = gcoMATH_UInt8AsFloat16((((((gctUINT32) (Hardware->alphaStates.color)) >> (0 ? 23:16)) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1)))))) ));
                colorG = gcoMATH_UInt8AsFloat16((((((gctUINT32) (Hardware->alphaStates.color)) >> (0 ? 15:8)) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1)))))) ));
                colorB = gcoMATH_UInt8AsFloat16((((((gctUINT32) (Hardware->alphaStates.color)) >> (0 ? 7:0)) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1)))))) ));
                colorA = gcoMATH_UInt8AsFloat16((((((gctUINT32) (Hardware->alphaStates.color)) >> (0 ? 31:24)) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1)))))) ));
                colorLow = colorR | (colorG << 16);
                colorHeigh = colorB | (colorA << 16);

                {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x052C,
                    colorLow
                    );

                gcmENDSTATEBATCH_NEW(
                    reserve, memory
                    );

                {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x052D,
                    colorHeigh
                    );

                gcmENDSTATEBATCH_NEW(
                    reserve, memory
                    );
            }

        {
            gctUINT16 reference;

            if (floatPipe)
            {
                gctUINT32 value = *(gctUINT32 *) &(Hardware->alphaStates.floatReference);

                if (value == 0xFFFFFFFF)
                {
                    reference = gcoMATH_UInt8AsFloat16(Hardware->alphaStates.reference);
                }
                else
                {
                    reference = _Float2Float16(value);
                }
            }
            else
            {
                /* Change to 12bit. */
                reference = (Hardware->alphaStates.reference << 4) | (Hardware->alphaStates.reference >> 4);
            }

            /* Setup the extra reference state. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0528) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0528, ((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) & ((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (reference) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

        /* Color conversion control. */
        if (Hardware->alphaStates.blend
            && (Hardware->alphaStates.srcFuncColor == gcvBLEND_SOURCE_ALPHA)
            && (Hardware->alphaStates.trgFuncColor == gcvBLEND_INV_SOURCE_ALPHA))
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
        else
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

        /* Mark alpha as updated. */
        Hardware->alphaDirty = gcvFALSE;

#if gcdALPHA_KILL_IN_SHADER
        /* Mark shaders as dirty. */
        Hardware->shaderDirty = gcvTRUE;
#endif
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushStencil(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    static const gctINT8 xlateCompare[] =
    {
        /* gcvCOMPARE_INVALID */
        -1,
        /* gcvCOMPARE_NEVER */
        0x0,
        /* gcvCOMPARE_NOT_EQUAL */
        0x5,
        /* gcvCOMPARE_LESS */
        0x1,
        /* gcvCOMPARE_LESS_OR_EQUAL */
        0x3,
        /* gcvCOMPARE_EQUAL */
        0x2,
        /* gcvCOMPARE_GREATER */
        0x4,
        /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6,
        /* gcvCOMPARE_ALWAYS */
        0x7
    };

    static const gctUINT8 xlateOperation[] =
    {
        /* gcvSTENCIL_KEEP */
        0x0,
        /* gcvSTENCIL_REPLACE */
        0x2,
        /* gcvSTENCIL_ZERO */
        0x1,
        /* gcvSTENCIL_INVERT */
        0x5,
        /* gcvSTENCIL_INCREMENT */
        0x6,
        /* gcvSTENCIL_DECREMENT */
        0x7,
        /* gcvSTENCIL_INCREMENT_SATURATE */
        0x3,
        /* gcvSTENCIL_DECREMENT_SATURATE */
        0x4,
    };

    gceSTATUS status;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->stencilDirty)
    {
        if (Hardware->stencilEnabled)
        {
            /* Setup the stencil config state.
            ** Here we ignored stencilStates.mode but always program TWO_SIDE,
            ** As if Hardware->stencilEnabled is true, we need tell MC to handle stencil plane
            ** as it exsited, or MC decoding will wrong if D24S8 is compressed.
            ** we don't set stencilStates.mode always to be TWO_SIDE as this state also affect earlyZ
            */
            gctINT stencilMode = (Hardware->depthStates.mode == gcvDEPTH_Z)
                                 ? gcvSTENCIL_DOUBLE_SIDED : gcvSTENCIL_NONE;

            if (Hardware->stencilStates.mode != gcvSTENCIL_NONE)
            {
                Hardware->depthStates.surface->canDropStencilPlane = gcvFALSE;
            }
            /* Reserve space in the command buffer. */
            gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

            if (Hardware->prevStencilMode != stencilMode)
            {
                if (!Hardware->flushedDepth)
                {
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                    Hardware->flushedDepth = gcvTRUE;
                }
                Hardware->prevStencilMode = stencilMode;
            }

            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0506) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                /* Setup the stencil operation state. */
                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0506,
                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (xlateCompare[Hardware->stencilStates.compareFront]) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.passFront]) & ((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.failFront]) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.depthFailFront]) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (xlateCompare[Hardware->stencilStates.compareBack]) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.passBack]) & ((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.failBack]) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (xlateOperation[Hardware->stencilStates.depthFailBack]) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)))
                    );

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0507,
                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (stencilMode) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.referenceFront) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.maskFront) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.writeMaskFront) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)))
                    );

                gcmSETFILLER_NEW(
                    reserve, memory
                    );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );

            /* Setup the extra reference state. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0528) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0528, ((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) & ((((gctUINT32) (~0U)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.referenceBack) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* Setup the extra reference state. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052E) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x052E, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.writeMaskBack) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (Hardware->stencilStates.maskBack) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* No need to flush depth cache bc earlyZ was only enabled when all
            ** depth op are KEEP.
            */

            /* Validate the state buffer. */
            gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
        }

        else
        {
            /* Reserve space in the command buffer. */
            gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

            /* Disable stencil. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0507) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0507, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* Validate the state buffer. */
            gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
        }

        /* Mark stencil as updated. */
        Hardware->stencilDirty = gcvFALSE;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
_FlushMultiTarget(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gctUINT32 i;
    gceSTATUS status = gcvSTATUS_OK;

    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    for (i = 1; i < Hardware->colorOutCount; ++i)
    {
        gcsSURF_INFO_PTR surface = Hardware->colorStates.target[i].surface;
        gctUINT32 physicalBaseAddr0 = 0, physicalBaseAddr1 = 0;

        gcmASSERT(surface);

        Hardware->colorStates.target[i].format = surface->formatInfo.renderFormat;

        if (Hardware->colorStates.target[i].format == gcvINVALID_RENDER_FORMAT)
        {
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        physicalBaseAddr0 = surface->node.physical + /* surface base */
                            Hardware->colorStates.target[i].offset +  /* slice offset inside one surface or texture mipmap */
                            Hardware->colorStates.target[i].layerIndex * surface->layerSize; /* layer offset inside one slice */

        physicalBaseAddr1 = surface->node.physical + /* surface base */
                            Hardware->colorStates.target[i].offset + /* slice offset inside one surface or texture mipmap */
                            Hardware->colorStates.target[i].layerIndex * surface->layerSize + /* layer offset inside one slice */
                            surface->bottomBufferOffset; /* bottom offset inside one layer */

        if (Hardware->features[gcvFEATURE_HALTI2])
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x5200 + (i-1)*8)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x5200 + (i-1)*8), physicalBaseAddr0);    gcmENDSTATEBATCH_NEW(reserve, memory);};

            /* even for single buffer PE, we still need to program two same address */
            if (Hardware->config->pixelPipes > 1)
            {
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x5200 + (i-1)*8+1)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x5200 + (i-1)*8+1), physicalBaseAddr1);    gcmENDSTATEBATCH_NEW(reserve, memory);};
            }

            if (Hardware->features[gcvFEATURE_PE_MULTI_RT_BLEND_ENABLE_CONTROL])
            {
                gctBOOL canEnableAlphaBlend = _canEnableAlphaBlend(Hardware, surface, Hardware->alphaStates.blend);

                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x5240 + (i-1))) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x5240 + (i-1)), ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | (((gctUINT32) ((gctUINT32) (surface->stride) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.target[i].format) & ((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) ((gctUINT32) (surface->superTiled) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (canEnableAlphaBlend) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))));    gcmENDSTATEBATCH_NEW(reserve, memory);};

            }
            else
            {
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x5240 + (i-1))) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x5240 + (i-1)), ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | (((gctUINT32) ((gctUINT32) (surface->stride) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.target[i].format) & ((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) ((gctUINT32) (surface->superTiled) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))));    gcmENDSTATEBATCH_NEW(reserve, memory);};


            }
        }
        else
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x0540 + (i-1)*8)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x0540 + (i-1)*8), physicalBaseAddr0);    gcmENDSTATEBATCH_NEW(reserve, memory);};

            if (Hardware->config->pixelPipes > 1)
            {
               {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x0540 + (i-1)*8+1)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x0540 + (i-1)*8+1), physicalBaseAddr1);    gcmENDSTATEBATCH_NEW(reserve, memory);};

            }
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((0x0560 + (i-1))) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x0560 + (i-1)), ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | (((gctUINT32) ((gctUINT32) (surface->stride) & ((gctUINT32) ((((1 ? 17:0) - (0 ? 17:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:0) - (0 ? 17:0) + 1))))))) << (0 ? 17:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.target[i].format) & ((gctUINT32) ((((1 ? 25:20) - (0 ? 25:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:20) - (0 ? 25:20) + 1))))))) << (0 ? 25:20))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) ((gctUINT32) (surface->superTiled) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))));    gcmENDSTATEBATCH_NEW(reserve, memory);};

        }
    }

    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushTarget(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status;
    gcsSURF_INFO_PTR surface = gcvNULL;
    gctUINT32 destinationRead = 0;
    gctUINT32 samples = 0;
    gctBOOL flushNeeded = gcvFALSE;
    gctUINT batchCount;
    gctBOOL colorBatchLoad;
    gctBOOL colorSplitLoad;
    gctINT colorAddressMode = -1;
    gctUINT physicalBaseAddr0 = 0, physicalBaseAddr1 = 0;
    gctBOOL colorWrite = Hardware->colorStates.colorWrite;
    gctBOOL  superTiled = Hardware->colorStates.target[0].superTiled;
    gctUINT format = Hardware->colorStates.target[0].format;
    gctUINT32 stride = 0;
    gctBOOL colorPipeEnable = (Hardware->colorStates.colorWrite != 0) || (Hardware->alphaStates.test != 0);

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* At least configuration has to be dirty. */
    gcmASSERT(Hardware->colorConfigDirty);

    /* Determine destination read control. */
    destinationRead = (!Hardware->depthStates.realOnly)
                      &&
                      (Hardware->colorStates.colorCompression ||
                       Hardware->alphaStates.blend ||
                       Hardware->colorStates.colorWrite != 0xF ||
                       Hardware->colorStates.rop != 0xC
                      )
                    ? 0x0
                    : 0x1;

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE,
             "Read control: compression=%d alpha=%d writeEnable=%X ==> %d",
             Hardware->colorStates.colorCompression,
             Hardware->alphaStates.blend,
             Hardware->colorStates.colorWrite,
             destinationRead);

    if ((Hardware->samples.x * Hardware->samples.y > 1) &&
         Hardware->features[gcvFEATURE_COMPRESSION_V2]  &&
        !Hardware->features[gcvFEATURE_V2_MSAA_COMP_FIX])
    {
        destinationRead = 0x0;
    }

    /* Determine whether the flush is needed. */
    if (destinationRead != Hardware->colorStates.destinationRead)
    {
        /* Flush the cache. */
        flushNeeded = !Hardware->flushedColor;

        /* Update the read control value. */
        Hardware->colorStates.destinationRead = destinationRead;
    }


    if (Hardware->colorTargetDirty)
    {
        gctUINT32 cacheMode;

        gcmASSERT(Hardware->colorConfigDirty);

        /* Get a shortcut to the surface. */
        surface = Hardware->colorStates.target[0].surface;

#if gcdMULTI_GPU
        if (Hardware->config->gpuCoreCount > 1)
        {
            gcmONERROR(
                _SetMultiGPURenderingMode(Hardware, surface, Hardware->gpuRenderingMode, Memory));
        }
#endif

        if (surface == gcvNULL)
        {
            /* Set the batch count. */
            batchCount = 1;

            if (Hardware->features[gcvFEATURE_HALTI2])
            {
                /* Setup load flags. */
                colorBatchLoad = gcvFALSE;
                colorSplitLoad = gcvFALSE;
                colorPipeEnable = gcvFALSE;
            }
            else
            {
                /* Setup load flags. */
                colorBatchLoad = gcvFALSE;
                colorSplitLoad = gcvTRUE;

                physicalBaseAddr0 = physicalBaseAddr1
                                  = Hardware->depthStates.realOnly ? 0 : Hardware->tempRT.node.physical;

                stride = 0;

                /* When RT is NULL, we need set colorMask zero to not touch surface.
                */
                colorWrite = 0;
                superTiled = gcvFALSE;
                format = 0x06;

                /* Single buffer mode. */
                if (Hardware->features[gcvFEATURE_SINGLE_BUFFER])
                {

                    if (Hardware->tempRT.formatInfo.bitsPerPixel <= 16 || Hardware->singleBuffer8x4)
                    {
                        colorAddressMode =
                            0x3;
                    }
                    else
                    {
                        colorAddressMode =
                            0x2;
                    }
                }
                /* tile */
                else
                {
                    colorAddressMode = 0x0;
                }
            }
        }
        else
        {
            /* Convert format to pixel engine format. */
            format                                 =
            Hardware->colorStates.target[0].format = surface->formatInfo.renderFormat;

            physicalBaseAddr0 = surface->node.physical + /* surface base */
                                Hardware->colorStates.target[0].offset +  /* slice offset inside one surface or texture mipmap */
                                Hardware->colorStates.target[0].layerIndex * surface->layerSize; /* layer offset inside one slice */

            physicalBaseAddr1 = surface->node.physical + /* surface base */
                                Hardware->colorStates.target[0].offset + /* slice offset inside one surface or texture mipmap */
                                Hardware->colorStates.target[0].layerIndex * surface->layerSize + /* layer offset inside one slice */
                                surface->bottomBufferOffset; /* bottom offset inside one layer */

            stride = surface->stride;

            if (surface->tiling == gcvLINEAR)
            {
                physicalBaseAddr1 = physicalBaseAddr0;
            }

            if (format == gcvINVALID_RENDER_FORMAT)
            {
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            /* Set the supertiled state. */
            superTiled                                  =
            Hardware->colorStates.target[0].superTiled  = Hardware->colorStates.target[0].surface->superTiled;

            /* Compute the numnber of samples.
            ** MSAA is always consistent between MRTs.
            */
            samples = surface->samples.x * surface->samples.y;

            /* Determine the cache mode (equation is taken from RTL). */
            if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
                Hardware->features[gcvFEATURE_SMALL_MSAA])
            {
                cacheMode = (samples == 4);
            }
            else
            {
                cacheMode = (samples == 4) && !surface->is16Bit;
            }


            /* Determine whether the flush is needed. */
            if (cacheMode != Hardware->colorStates.cacheMode)
            {
                /* Flush the cache. */
                flushNeeded = !Hardware->flushedColor;

                /* Update the cache mode. */
                Hardware->colorStates.cacheMode = cacheMode;
            }

            if (Hardware->config->pixelPipes > 1
             || Hardware->config->renderTargets > 1)
            {
                /* Set the batch count. */
                batchCount = 1;

                /* Setup load flags. */
                colorBatchLoad = gcvFALSE;
                colorSplitLoad = gcvTRUE;
            }
            else
            {
                /* Set the batch count. */
                batchCount = 3;

                /* Setup load flags. */
                colorBatchLoad = gcvTRUE;
                colorSplitLoad = gcvFALSE;
            }

            if (surface->tiling == gcvLINEAR)
            {
                /* Linear render target. */
                gcmASSERT(Hardware->features[gcvFEATURE_LINEAR_RENDER_TARGET]);

                colorAddressMode = 0x1;
            }
            /* Single buffer mode. */
            else if (Hardware->features[gcvFEATURE_SINGLE_BUFFER])
            {

                colorAddressMode = Hardware->singleBuffer8x4 ?
                                   0x3 :
                                   0x2;

            }
            /* tile */
            else
            {
                colorAddressMode = 0x0;
            }
        }
    }
    else
    {
        /* Set the batch count. */
        batchCount = 1;

        /* Setup load flags. */
        colorBatchLoad = gcvFALSE;
        colorSplitLoad = gcvFALSE;
    }

    if (flushNeeded)
    {
        /* Mark color cache as flushed. */
        Hardware->flushedColor = gcvTRUE;
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    /* Flush must be issued before changing DESTINATION_READ. */
    if (flushNeeded)
    {
        /* Flush the cache. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)batchCount  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (batchCount ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x050B) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        /* Program AQPixelConfig.Format field. */
        if (Hardware->features[gcvFEATURE_HALTI0] ||
            ((Hardware->config->chipModel == gcv1000) && (Hardware->config->chipRevision >= 0x5039)))
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x050B,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (superTiled) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.destinationRead) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (colorWrite) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) ((gctUINT32) (!colorPipeEnable) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)))
                );
        }
        else
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x050B,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (superTiled) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (Hardware->colorStates.destinationRead) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (colorWrite) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                );
        }

        if (colorBatchLoad)
        {
            /* Program AQPixelAddress register. */
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x050C,
                physicalBaseAddr0
                );

            /* Program AQPixelStride register. */
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x050D,
                stride
                );
        }

    gcmENDSTATEBATCH_NEW(
        reserve, memory
        );

    if (Hardware->features[gcvFEATURE_PE_DITHER_FIX])
    {
        if (Hardware->peDitherDirty)
        {
            gctUINT32 *pDitherTable;

            if (Hardware->colorStates.colorWrite != 0xF)
            {
                /* Reset dither table, if any color channel is disabled. */
                pDitherTable = &Hardware->ditherTable[gcvFALSE][0];

                /* Cannot handled now
                gcmASSERT(!Hardware->ditherEnable);
                */
            }
            else
            {
                pDitherTable = &Hardware->ditherTable[Hardware->ditherEnable][0];
            }

            /* Append RESOLVE_DITHER commands. */
            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x052A,
                    pDitherTable[0]
                    );

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x052B,
                    pDitherTable[1]
                    );

                gcmSETFILLER_NEW(
                    reserve, memory
                    );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );

            /* Reset dirty flags. */
            Hardware->peDitherDirty = gcvFALSE;
        }
    }
    else
    {
        /* If the draw required dither, while PE cannot support, defer until resolve time.
        ** If the draw didn't require, do not reset the flag, unless we are sure it's the
        ** draw can overwrite all of the surface.
        */
        if (Hardware->ditherEnable)
        {
            gctUINT32 i;

            if (Hardware->colorStates.target[0].surface)
            {
                Hardware->colorStates.target[0].surface->deferDither3D = gcvTRUE;
            }

            for (i = 1; i < Hardware->colorOutCount; ++i)
            {
                gcmASSERT(Hardware->colorStates.target[i].surface);
                Hardware->colorStates.target[i].surface->deferDither3D = gcvTRUE;
            }
        }
    }

    if (colorSplitLoad)
    {
       if(Hardware->config->pixelPipes > 1)
       {
            gcmASSERT(Hardware->config->pixelPipes == 2);

            /* even for single buffer PE, we still need to program two same address */
            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0518) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0518,
                    physicalBaseAddr0
                    );

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0518 + 1,
                    physicalBaseAddr1
                    );

                gcmSETFILLER_NEW(
                    reserve, memory
                    );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );
       }
       else
       {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0518) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0518, physicalBaseAddr0 );    gcmENDSTATEBATCH_NEW(reserve, memory);};
            /* Program AQPixelAddress register. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x050C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x050C, physicalBaseAddr0 );    gcmENDSTATEBATCH_NEW(reserve, memory);};
       }

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x050D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x050D, stride );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    if (colorAddressMode != -1)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (colorAddressMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    if (Hardware->colorTargetDirty && (surface != gcvNULL))
    {
        /* Setup multi-sampling. */
        Hardware->msaaConfigDirty = gcvTRUE;
        Hardware->msaaModeDirty   = gcvTRUE;
    }

    if (Hardware->colorOutCount > 1)
    {
        _FlushMultiTarget(Hardware, Memory);
    }

    /* Reset dirty flags. */
    Hardware->colorConfigDirty = gcvFALSE;
    Hardware->colorTargetDirty = gcvFALSE;

    /* HALTI2 integer RT need dirty shader to re-program ps output mode and ps control register
    ** Without this check, it maybe degrade the CPU performance.
    */
    if (Hardware->features[gcvFEATURE_HALTI2])
    {
        Hardware->shaderDirty = gcvTRUE;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushDepth(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    static const gctINT32 xlateCompare[] =
    {
        /* gcvCOMPARE_INVALID */
        -1,
        /* gcvCOMPARE_NEVER */
        0x0,
        /* gcvCOMPARE_NOT_EQUAL */
        0x5,
        /* gcvCOMPARE_LESS */
        0x1,
        /* gcvCOMPARE_LESS_OR_EQUAL */
        0x3,
        /* gcvCOMPARE_EQUAL */
        0x2,
        /* gcvCOMPARE_GREATER */
        0x4,
        /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6,
        /* gcvCOMPARE_ALWAYS */
        0x7,
    };

    static const gctUINT32 xlateMode[] =
    {
        /* gcvDEPTH_NONE */
        0x0,
        /* gcvDEPTH_Z */
        0x1,
        /* gcvDEPTH_W */
        0x2,
    };

    gceSTATUS status;
    gcsDEPTH_INFO *depthInfo;
    gcsSURF_INFO_PTR surface;

    gctUINT depthBatchAddress;
    gctUINT depthBatchCount;
    gctBOOL depthBatchLoad;
    gctBOOL depthSplitLoad;
    gctBOOL needFiller;

    gctUINT hzBatchCount = 0;

    gctBOOL flushNeeded = gcvFALSE;
    gctBOOL stallNeeded = gcvFALSE;

    gctINT colorSingleBufferMode = -1;
    gctUINT stencilFormat = 0x0;

    gctBOOL colorSingleBuffer = gcvFALSE;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    depthInfo = &Hardware->depthStates;
    surface   = depthInfo->surface;

    /* If depthTarget dirtied, depthConfig must dirtied. */
    gcmASSERT(!Hardware->depthTargetDirty || Hardware->depthConfigDirty);

    /* Test if configuration is dirty. */
    if (Hardware->depthConfigDirty)
    {
        gceDEPTH_MODE depthMode = surface ? depthInfo->mode : gcvDEPTH_NONE;
        gceCOMPARE compare = surface ? depthInfo->compare : gcvCOMPARE_ALWAYS;

        gctBOOL ezEnabled = depthInfo->early &&
                             !Hardware->disableAllEarlyDepth &&
                             !Hardware->disableAllEarlyDepthFromStatistics;

        gctBOOL peDepth = depthInfo->write ||
                           !ezEnabled ||
                           (Hardware->stencilStates.mode != gcvSTENCIL_NONE) ||
                           /* If RA doesn't modify Z, then PE needs to do Z. */
                           Hardware->disableRAModifyZ;

        /* HZ in fact depends on both depthTargetDirty and depthConfigDirty */
        gctBOOL hzEnabled = surface &&
                            !surface->hzDisabled &&
                            (depthInfo->mode != gcvDEPTH_NONE) &&
                            !Hardware->disableAllEarlyDepth &&
                            !Hardware->disableAllEarlyDepthFromStatistics;

        if (Hardware->previousPEDepth != peDepth)
        {
            /* Need flush and stall when switching from PE Depth ON to PE Depth Off? */
            if (Hardware->previousPEDepth)
            {
                stallNeeded = gcvTRUE;
            }

            Hardware->previousPEDepth = peDepth;
        }

        /* If earlyZ or depth compare changed, and earlyZ is enabled, need to flush Zcache and stall */
        /* No need to initialize previous values bc no PE depth write before first draw. */
        if (Hardware->prevEarlyZ != ezEnabled ||
            Hardware->prevDepthCompare != depthInfo->compare)
        {
            if (ezEnabled || hzEnabled)
            {
                flushNeeded = !Hardware->flushedDepth;
                stallNeeded = gcvTRUE;
            }

            Hardware->prevEarlyZ = ezEnabled;
            Hardware->prevDepthCompare = depthInfo->compare;
        }

        depthInfo->regDepthConfig &= ~(((((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) |
                                       ((((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) |
                                       ((((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) |
                                       ((((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) |
                                       ((((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) |
                                       ((((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
                                       );

        depthInfo->regDepthConfig |= (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (xlateMode[depthMode]) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) |
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (depthInfo->realOnly) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) |
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (ezEnabled) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) |
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (depthInfo->write) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) |
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (xlateCompare[compare]) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) |
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (!peDepth) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
                                      );

        depthInfo->regRAControlHZ = ((((gctUINT32) (depthInfo->regRAControlHZ)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (xlateCompare[compare] ) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)));

        /* Depth config change re-program Depth mode, stencil mode depend it. */
        Hardware->stencilDirty = gcvTRUE;

        if (Hardware->depthTargetDirty)
        {
#if gcdMULTI_GPU
            if ((Hardware->colorStates.target[0].surface == gcvNULL) && (Hardware->config->gpuCoreCount > 1))
            {
                /* Set split mode based on depth surface, if rendering in depth only. */
                gcmONERROR(
                    _SetMultiGPURenderingMode(Hardware, surface, Hardware->gpuRenderingMode, Memory));
            }
#endif

            /* If depth write is disabled and compare is always, we can turn off HZ as well. */
            if (!depthInfo->write && depthInfo->compare == gcvCOMPARE_ALWAYS)
            {
                hzEnabled = gcvFALSE;
            }

            if (hzEnabled)
            {
                hzBatchCount = 2;

                if (surface->is16Bit)
                {
                    depthInfo->regRAControlHZ = ((((gctUINT32) (depthInfo->regRAControlHZ)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

                    depthInfo->regControlHZ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0x5 & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                                            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) (0x5 & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
                }
                else
                {
                    depthInfo->regRAControlHZ = ((((gctUINT32) (depthInfo->regRAControlHZ)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

                    depthInfo->regControlHZ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0x8 & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                                            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) (0x8 & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
                }

                /* RA HZ depends on MC HZBase to access tilestatus, must wait PE->MC received the addr */
                if ((((((gctUINT32) (Hardware->memoryConfig)) >> (0 ? 12:12)) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) ))
                {
                    stallNeeded = gcvTRUE;
                }
            }
            else
            {
                hzBatchCount = 1;

                depthInfo->regControlHZ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));

                depthInfo->regRAControlHZ = ((((gctUINT32) (depthInfo->regRAControlHZ)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)));
            }

            /* Program RA Z control states. */
            /* Early early z has issue in calculating polygon offset on PXA988. we just disable early early z on PXA988 */
            if ((Hardware->config->chipModel == gcv1000 && Hardware->config->chipRevision < 0x5035) ||
                (Hardware->config->chipModel == gcv400 && Hardware->config->chipRevision >= 0x4645))
            {
                depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
            }
            else  if (surface && surface->format == gcvSURF_S8)
            {
                depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));
            }
            else
            {
                gctUINT32 eezEnabled = !Hardware->disableAllEarlyDepth;

                if (Hardware->patchID == gcvPATCH_DEQP &&
                    (Hardware->features[gcvFEATURE_NEW_HZ] || Hardware->features[gcvFEATURE_FAST_MSAA])
                   )
                {
                    eezEnabled = 0;
                }

                depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (eezEnabled) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
            }

            if (Hardware->features[gcvFEATURE_NEW_RA])
            {
                gctBOOL msaa = (Hardware->samples.x * Hardware->samples.y) > 1;
                gctUINT32 shaderZ = !Hardware->disableRAPassZ ||    /* Pass Z value or not */
                                    ((msaa & peDepth) << 1);        /* Pass sub samples values or not */

                depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (shaderZ) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)));

                if (!ezEnabled)
                {
                    depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)));
                }
                else if (Hardware->disableRAModifyZ)
                {
                    depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)));
                }
                else
                {
                    depthInfo->regRAControl = ((((gctUINT32) (depthInfo->regRAControl)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)));
                }
            }

            if (surface)
            {
                gctUINT32 samples, cacheMode;

                if (surface->is16Bit)
                {
                    /* 16-bit depth buffer. */
                    depthInfo->regDepthConfig = ((((gctUINT32) (depthInfo->regDepthConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

                    Hardware->maxDepth = 0xFFFF;
                    Hardware->stencilEnabled = gcvFALSE;
                }
                else
                {
                    /* 24-bit depth buffer. */
                    depthInfo->regDepthConfig = ((((gctUINT32) (depthInfo->regDepthConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

                    Hardware->maxDepth = 0xFFFFFF;
                    Hardware->stencilEnabled = ((surface->format == gcvSURF_D24S8) || (surface->format == gcvSURF_S8));
                }

                /* Compute the number of samples. */
                samples = surface->samples.x * surface->samples.y;

                /* Determine the cache mode (equation is taken from RTL). */
                if (Hardware->features[gcvFEATURE_FAST_MSAA] || Hardware->features[gcvFEATURE_SMALL_MSAA])
                {
                    cacheMode = (samples == 4);
                }
                else
                {
                    cacheMode = (samples == 4) && !surface->is16Bit;
                }

                /* We need to flush the cache if the cache mode is changing. */
                if (cacheMode != depthInfo->cacheMode)
                {
                    /* Flush the cache. */
                    flushNeeded = !Hardware->flushedDepth;

                    /* Update the cache mode. */
                    depthInfo->cacheMode = cacheMode;
                }
                if (Hardware->features[gcvFEATURE_S8_ONLY_RENDERING] &&
                    surface->format == gcvSURF_S8)
                {
                    stencilFormat = 0x1;
                }
            }
            else
            {
                Hardware->stencilEnabled = gcvFALSE;
            }
        }
    }

    if (Hardware->depthTargetDirty)
    {
        gcmASSERT(Hardware->depthConfigDirty);

        /* Configuration is always set when the surface is changed. */
        depthBatchAddress = 0x0500;

        if (surface == gcvNULL)
        {
            /* Define depth state load parameters. */
            if (Hardware->depthRangeDirty || Hardware->depthNormalizationDirty)
            {
                depthBatchCount = Hardware->depthRangeDirty ? 4 : 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvFALSE;
                needFiller      = Hardware->depthRangeDirty ? gcvTRUE : gcvFALSE;
            }
            else
            {
                depthBatchCount = 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvFALSE;
                needFiller      = gcvFALSE;
            }

            /* No stencil. */
            Hardware->stencilEnabled = gcvFALSE;
        }
        else
        {
            /* Define depth state load parameters. */
            if (Hardware->depthRangeDirty || Hardware->depthNormalizationDirty)
            {
                if (Hardware->config->pixelPipes > 1
                 || Hardware->config->renderTargets > 1)
                {
                    depthBatchCount = Hardware->depthRangeDirty ? 4 : 1;
                    depthBatchLoad  = gcvFALSE;
                    depthSplitLoad  = gcvTRUE;
                    needFiller      = Hardware->depthRangeDirty ? gcvTRUE : gcvFALSE;
                }
                else
                {
                    depthBatchCount = Hardware->depthRangeDirty ? 6 : 1;
                    depthBatchLoad  = gcvTRUE;
                    depthSplitLoad  = gcvFALSE;
                    needFiller      = Hardware->depthRangeDirty ? gcvTRUE : gcvFALSE;
                }
            }
            else
            {
                depthBatchCount = 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvTRUE;
                needFiller      = gcvFALSE;
            }

            /* Single buffer mode. */
            if (depthSplitLoad && Hardware->features[gcvFEATURE_SINGLE_BUFFER])
            {
                /* Program 8x4 if any of the surfaces is 16bit,
                   else program 4x4. */
                colorSingleBuffer = gcvTRUE;

                colorSingleBufferMode = Hardware->singleBuffer8x4 ?
                                            0x3 :
                                            0x2;
            }
        }
    }
    /* Depth surface is not dirty. */
    else
    {
        if (Hardware->depthConfigDirty)
        {
            /* The batch starts with the configuration. */
            depthBatchAddress = 0x0500;

            /* Define depth state load parameters. */
            if (Hardware->depthRangeDirty || Hardware->depthNormalizationDirty)
            {
                depthBatchCount = Hardware->depthRangeDirty ? 4 : 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvFALSE;
                needFiller      = Hardware->depthRangeDirty ? gcvTRUE : gcvFALSE;

            }
            else
            {
                depthBatchCount = 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvFALSE;
                needFiller      = gcvFALSE;
            }
        }
        else
        {
            /* Define depth state load parameters. */
            if (Hardware->depthRangeDirty || Hardware->depthNormalizationDirty)
            {
                depthBatchAddress = Hardware->depthRangeDirty ? 0x0501 : 0x0503;
                depthBatchCount = Hardware->depthRangeDirty ? 3 : 1;
                depthBatchLoad  = gcvFALSE;
                depthSplitLoad  = gcvFALSE;
                needFiller      = gcvFALSE;
            }
            else
            {
                /* Nothing to be done. */
                gcmFOOTER_NO();
                return gcvSTATUS_OK;
            }
        }
    }

    /* Add the flush state. */
    if (flushNeeded || stallNeeded)
    {
        /* Mark depth cache as flushed. */
        Hardware->flushedDepth = gcvTRUE;
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    if (flushNeeded || stallNeeded)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)depthBatchCount  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (depthBatchCount ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (depthBatchAddress) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

    if (Hardware->depthConfigDirty)
    {
        gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0500, depthInfo->regDepthConfig);
    }

    if (Hardware->depthRangeDirty || Hardware->depthNormalizationDirty)
    {
        gcuFLOAT_UINT32 depthNear, depthFar;
        gcuFLOAT_UINT32 depthNormalize;

        if (depthInfo->mode == gcvDEPTH_W)
        {
            depthNear.f = depthInfo->near;
            depthFar.f  = depthInfo->far;

            depthNormalize.f = (depthFar.u != depthNear.u)
                ? gcoMATH_Divide(
                    gcoMATH_UInt2Float(Hardware->maxDepth),
                    gcoMATH_Add(depthFar.f, -depthNear.f)
                    )
                : gcoMATH_UInt2Float(Hardware->maxDepth);
        }
        else
        {
            depthNear.f = Hardware->depthNear.f;
            depthFar.f  = Hardware->depthFar.f;

            depthNormalize.f = gcoMATH_UInt2Float(Hardware->maxDepth);
        }

        if(Hardware->depthRangeDirty)
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0501,
                depthNear.u
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0502,
                depthFar.u
                );
        }

        if((!Hardware->depthRangeDirty) && Hardware->depthNormalizationDirty)
        {
            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );

            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0503) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0503, depthNormalize.u );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
        else
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0503,
                depthNormalize.u
                );
        }
    }

    if (depthBatchLoad && (!Hardware->depthRangeDirty) && Hardware->depthNormalizationDirty)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0504) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0504, surface->node.physical + depthInfo->offset );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0505) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0505, surface->stride );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }
    else if(depthBatchLoad)
    {
        gcmSETSTATEDATA_NEW(
            stateDelta, reserve, memory, gcvFALSE, 0x0504,
            surface->node.physical + depthInfo->offset
            );

        gcmSETSTATEDATA_NEW(
            stateDelta, reserve, memory, gcvFALSE, 0x0505,
            surface->stride
            );
    }

    if (needFiller)
    {
        gcmSETFILLER_NEW(
            reserve, memory
            );
    }

    if(Hardware->depthRangeDirty || (!Hardware->depthNormalizationDirty))
    {
        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );
    }

    if (depthSplitLoad)
    {
        if (Hardware->config->pixelPipes > 1
         || Hardware->config->renderTargets > 1)
        {
            if (Hardware->config->pixelPipes > 1)
            {
                gcmASSERT(Hardware->config->pixelPipes == 2);

                /* Program depth addresses. */
                {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0520) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0520,
                        surface->node.physical + depthInfo->offset
                        );

                    /* TODO: for singleBuffer+MultiPipe case, no need to program the 2nd address */
                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0520 + 1,
                        surface->node.physicalBottom + depthInfo->offset
                        );

                    gcmSETFILLER_NEW(
                        reserve, memory
                        );

                gcmENDSTATEBATCH_NEW(
                    reserve, memory
                    );

                if (colorSingleBuffer)
                {
                    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (colorSingleBufferMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
                }
            }
            else /* renderTargets > 1. */
            {
                /* Program depth addresses. */
                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0520) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0520, surface->node.physical );    gcmENDSTATEBATCH_NEW(reserve, memory);};

                {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0504) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0504, surface->node.physical + depthInfo->offset );    gcmENDSTATEBATCH_NEW(reserve, memory);};
            }

            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0505) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0505, surface->stride );    gcmENDSTATEBATCH_NEW(reserve, memory);};

            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (stencilFormat) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};


        }
        else
        {
            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0504) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0504,
                    surface->node.physical + depthInfo->offset
                    );

                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0505,
                    surface->stride
                    );

                gcmSETFILLER_NEW(
                    reserve, memory
                    );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );
        }
    }

    if (Hardware->depthConfigDirty)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0382) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0382, depthInfo->regRAControl );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    if (hzBatchCount != 0)
    {
        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)hzBatchCount  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (hzBatchCount ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0515) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0515,
                depthInfo->regControlHZ
                );

            if (hzBatchCount == 2)
            {
                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, 0x0516,
                    surface->hzNode.physical
                    );

                gcmSETFILLER_NEW(
                    reserve, memory
                    );
            }

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0388) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0388, depthInfo->regRAControlHZ );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        if (hzBatchCount == 2)
        {
            /* NEW_RA register. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0389) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0389, surface->hzNode.physical );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
    }

    if (Hardware->depthRangeDirty)
    {
        gcuFLOAT_UINT32 zOffset, zScale;

        if (Hardware->features[gcvFEATURE_ZSCALE_FIX] &&
            gcoHAL_GetOption(gcvNULL, gcvOPTION_PREFER_ZCONVERT_BYPASS))
        {
            zOffset.f = gcoMATH_Add(
                depthInfo->far, depthInfo->near
                ) / 2.0f;

            zScale.f  = gcoMATH_Add(
                depthInfo->far, -depthInfo->near
                ) / 2.0f;
         }
         else
         {
            zOffset.f = depthInfo->near;
            zScale.f  = gcoMATH_Add(
                depthInfo->far, -depthInfo->near
                );
         }

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0282) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0282, zScale.u );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0285) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0285, zOffset.u );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    if (stallNeeded)
    {
        /* Send semaphore-stall from RA to PE. */
        gcmONERROR(gcoHARDWARE_Semaphore(Hardware,
                                         gcvWHERE_RASTER,
                                         gcvWHERE_PIXEL,
                                         gcvHOW_SEMAPHORE,
                                         Memory));
    }

    if ((Hardware->depthTargetDirty) &&
        (Hardware->colorStates.target[0].surface == gcvNULL) &&
        (depthInfo->surface != gcvNULL))
    {
        Hardware->vaa             = gcvVAA_NONE;
        Hardware->msaaConfigDirty = gcvTRUE;
        Hardware->msaaModeDirty   = gcvTRUE;
    }

    /* Reset dirty flags. */
    Hardware->depthConfigDirty        = gcvFALSE;
    Hardware->depthRangeDirty         = gcvFALSE;
    Hardware->depthTargetDirty        = gcvFALSE;
    Hardware->depthNormalizationDirty = gcvFALSE;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushSampling(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status;
    gctUINT samples = 0;
    gctBOOL programTables = gcvFALSE, programFastMSAA = gcvFALSE;
    gctUINT32 msaaMode = 0;
    gctUINT32 vaaMode = 0;
    gctUINT msaaEnable;
    gctUINT32_PTR sampleCoords = gcvNULL;
    gcsCENTROIDS_PTR centroids = gcvNULL;
    gctUINT32 jitterIndex = 0;
    gctINT i, tables = 0;
    gctBOOL yInverted = gcvFALSE;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Configuration is always loaded. */
    gcmASSERT(Hardware->msaaConfigDirty);

    samples = Hardware->samples.x * Hardware->samples.y;

#if gcdYINVERTED_RENDERING

    if (Hardware->colorStates.target[0].surface != gcvNULL)
    {
        yInverted = (Hardware->colorStates.target[0].surface->flags &
                     gcvSURF_FLAG_CONTENT_YINVERTED);
    }
    else if (Hardware->depthStates.surface)
    {
        yInverted = (Hardware->depthStates.surface->flags &
                     gcvSURF_FLAG_CONTENT_YINVERTED);
    }
#endif

    if (Hardware->vaa == gcvVAA_NONE)
    {
        /* Compute centroids. */
        if (Hardware->centroidsDirty)
        {
            gcmONERROR(gcoHARDWARE_ComputeCentroids(
                Hardware, 1, &Hardware->sampleCoords2, &Hardware->centroids2
                ));

            gcmONERROR(gcoHARDWARE_ComputeCentroids(
                Hardware, 3, Hardware->sampleCoords4, Hardware->centroids4
                ));

            Hardware->centroidsDirty = gcvFALSE;
        }

        switch (samples)
        {
        case 1:
            vaaMode  = 0x0;
            msaaMode = 0x0;
            Hardware->sampleEnable = 0x0;
            break;

        case 2:
            vaaMode  = 0x0;
            msaaMode = 0x1;
            Hardware->sampleEnable = 0x3;

            if (Hardware->msaaModeDirty)
            {
                programTables = gcvTRUE;
                sampleCoords  = &Hardware->sampleCoords2;
                centroids     = &Hardware->centroids2;
                tables        = 1;
            }
            break;

        case 4:
            vaaMode  = 0x0;
            msaaMode = 0x2;
            Hardware->sampleEnable = 0xF;

            if (Hardware->msaaModeDirty)
            {
                programTables = gcvTRUE;
                sampleCoords  = Hardware->sampleCoords4;
                centroids     = Hardware->centroids4;
                if (yInverted)
                {
                    /* Pick index 1, for FastMSAA, jitter/sampleCord programming will be ignored.*/
                    jitterIndex = 0x55555555;
                }
                else
                {
                    jitterIndex   = Hardware->jitterIndex;
                }
                tables        = 3;
            }
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }
    else
    {
        vaaMode = ((Hardware->config->chipModel < gcv600) || (Hardware->vaa == gcvVAA_COVERAGE_8))
            ? 0x2
            : 0x1;

        msaaMode = 0x0;

        Hardware->sampleEnable = 0x1;

        if (Hardware->msaaModeDirty)
        {
            programTables = gcvTRUE;

            sampleCoords = (vaaMode == 0x2)
                ? &Hardware->vaa8SampleCoords
                : &Hardware->vaa16SampleCoords;
        }
    }

    /* Determine enable value. */
    msaaEnable = Hardware->sampleMask & Hardware->sampleEnable;

    if (Hardware->msaaModeDirty &&
        (Hardware->features[gcvFEATURE_FAST_MSAA] ||
         Hardware->features[gcvFEATURE_SMALL_MSAA]))
    {

        programFastMSAA = gcvTRUE;
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    /* Set multi-sample mode. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E06) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E06, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (msaaMode) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (vaaMode) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (msaaEnable) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

    if (programTables)
    {
        /* Set multi-sample jitter. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0381) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0381, jitterIndex );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        if (Hardware->vaa == gcvVAA_NONE)
        {
            /* Program sample coordinates. */
            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)tables  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (tables ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0384) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                for (i = 0; i < tables; i += 1)
                {
                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0384 + i,
                        sampleCoords[i]
                        );
                }

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );

            /* Program the centroid table. */
            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)tables * 4  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (tables * 4 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0390) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                for (i = 0; i < tables; i += 1)
                {
                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0390 + i * 4 + 0,
                        centroids[i].value[0]
                        );

                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0390 + i * 4 + 1,
                        centroids[i].value[1]
                        );

                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0390 + i * 4 + 2,
                        centroids[i].value[2]
                        );

                    gcmSETSTATEDATA_NEW(
                        stateDelta, reserve, memory, gcvFALSE, 0x0390 + i * 4 + 3,
                        centroids[i].value[3]
                        );
                }

                gcmSETFILLER_NEW(
                    reserve, memory
                    );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );
        }
        else
        {
            /* Program coverage. */
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0384) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0384, sampleCoords[0] );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
    }

    /* Flush C/Z and Stall. */
    if (programFastMSAA)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* We also have to sema stall between FE and PE to make sure the flush
        ** has happened. */
        {    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));    gcmENDSTATEBATCH_NEW(reserve, memory);};    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]", ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))), ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));    gcmSKIPSECUREUSER();};

        /* We need to program fast msaa config if the cache mode is changing. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)))) | (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) ((samples == 4) ? 1 : 0) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)))) & (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) ((samples == 4) ? 1 : 0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27)))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* msaaConfigDirty will affect PS input control programming,
    ** we should dirty shader, but shader dirty has been dirtied
    ** when RT switch, which also affect msaaConfigDirty.
    */

    /* Reset dirty flags. */
    Hardware->msaaConfigDirty = gcvFALSE;
    Hardware->msaaModeDirty   = gcvFALSE;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_FlushPA(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    static const gctUINT32 xlateCulling[] =
    {
        /* gcvCULL_NONE */
        0x0,
        /* gcvCULL_CCW*/
        0x2,
        /* gcvCULL_CW*/
        0x1
    };

    static const gctUINT32 xlateFill[] =
    {
        /* gcvFILL_POINT */
        0x0,
        /* gcvFILL_WIRE_FRAME */
        0x1,
        /* gcvFILL_SOLID */
        0x2
    };

    static const gctUINT32 xlateShade[] =
    {
        /* gcvSHADING_SMOOTH */
        0x1,
        /* gcvSHADING_FLAT_D3D */
        0x0,
        /* gcvSHADING_FLAT_OPENGL */
        0x0
    };

    gceSTATUS status;
    gctUINT batchAddress;
    gctUINT batchCount;
    gctBOOL needFiller;

    gcuFLOAT_UINT32 width;
    gcuFLOAT_UINT32 adjustSub;
    gcuFLOAT_UINT32 adjustAdd;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->paLineDirty)
    {
        if (Hardware->paConfigDirty)
        {
            batchAddress = 0x028D;
            batchCount   = 3;
            needFiller   = gcvFALSE;
        }
        else
        {
            batchAddress = 0x028E;
            batchCount   = 2;
            needFiller   = gcvTRUE;
        }
    }
    else
    {
        if (Hardware->paConfigDirty)
        {
            batchAddress = 0x028D;
            batchCount   = 1;
            needFiller   = gcvFALSE;
        }
        else
        {
            /* Nothing to be done. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;
        }
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    if (Hardware->paLineDirty)
    {
        width.f = Hardware->paStates.aaLineWidth / 2.0f;

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0286) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0286, width.u );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)batchCount  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (batchCount ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (batchAddress) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        if (Hardware->paConfigDirty)
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x028D,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) ((gctUINT32) (Hardware->paStates.pointSize) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (Hardware->paStates.pointSprite) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (xlateCulling[Hardware->paStates.culling]) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (xlateFill[Hardware->paStates.fillMode]) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (xlateShade[Hardware->paStates.shading]) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22))) | (((gctUINT32) ((gctUINT32) (Hardware->paStates.aaLine) & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (Hardware->paStates.aaLineTexSlot) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (Hardware->paStates.wclip) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
                );
            Hardware->paConfigDirty = gcvFALSE;
        }

        if (Hardware->paLineDirty)
        {
            adjustSub.f = Hardware->paStates.aaLineWidth / 2.0f;
            adjustAdd.f = -adjustSub.f + Hardware->paStates.aaLineWidth;

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x028E,
                adjustSub.u
                );

            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x028F,
                adjustAdd.u
                );

            Hardware->paLineDirty = gcvFALSE;
        }

        if (needFiller)
        {
            gcmSETFILLER_NEW(
                reserve, memory
                );
        }

    gcmENDSTATEBATCH_NEW(
        reserve, memory
        );

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
_ResizeTempRT(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR dSurface
    );

gceSTATUS
gcoHARDWARE_FlushDepthOnly(
    IN gcoHARDWARE Hardware
    )
{
    gctBOOL depthOnly = Hardware->depthStates.only;
    gctBOOL msaa = (Hardware->samples.x * Hardware->samples.y > 1) && (Hardware->sampleMask != 0);
    gcsHINT_PTR hints = Hardware->shaderStates.hints;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    if ((!(Hardware->features[gcvFEATURE_FAST_MSAA] || Hardware->features[gcvFEATURE_SMALL_MSAA]) && msaa) ||
        (hints && (hints->hasKill || hints->psHasFragDepthOut)) ||
        (Hardware->alphaStates.test))
    {
        /* Cannot enable depth only b/c ps cannot be bypassed now */
        if (depthOnly)
        {
            _ResizeTempRT(Hardware, Hardware->depthStates.surface);
            Hardware->colorTargetDirty = gcvTRUE;
            Hardware->colorConfigDirty = gcvTRUE;
            depthOnly = gcvFALSE;
        }
    }
    else if ((Hardware->colorStates.colorWrite == 0) &&
             (Hardware->colorStates.colorCompression == gcvFALSE))
    {
         /* Enable depth only if no actually color be written */
         depthOnly = gcvTRUE;
    }

    if (Hardware->depthStates.realOnly != depthOnly)
    {
        Hardware->depthStates.realOnly  = depthOnly;

        /* depthOnly affects color/depth/shader programmings */
        Hardware->colorConfigDirty = gcvTRUE;
        Hardware->depthConfigDirty = gcvTRUE;
        Hardware->shaderDirty = gcvTRUE;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}


#if gcdSTREAM_OUT_BUFFER

gceSTATUS
gcoHARDWARE_FlushStreamOut(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gctUINT32 writeTrough;

#if gcdMULTI_GPU
    gctUINT coreCount = Hardware->config->gpuCoreCount;
    gctUINT i;
    gceMULTI_GPU_RENDERING_MODE hwRenderMode = Hardware->gpuRenderingMode;
    gctUINT32 renderMode;
#else
    gctUINT coreCount = 1;
    gctUINT i;
    gceMULTI_GPU_RENDERING_MODE hwRenderMode = 0;
    gctUINT32 renderMode;
#endif

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=%p", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Reset unused pointers. */
    reserve = gcvNULL;
    stateDelta = gcvNULL;

    /* Decide on write trough */
    if (Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_1_PREPASS)
    {
        writeTrough = 0x1;
    }
    else
    {
        writeTrough = 0x0;
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + coreCount) * gcmSIZEOF(gctUINT32), 8);

#if gcdMULTI_GPU
    /* Add space for core enable/disable */
    reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

#if gcdMULTI_GPU
    /* Enable core 0 */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };
#endif

    /* Begin Batch */
    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)coreCount <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (coreCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5110) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        for(i = 0; i < coreCount; i++)
        {
            /* Program stream out buffers */
            gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, (0x5110 + i),
                            Hardware->streamOutIndexCache.tail->streamOutIndexAddress[i]);
        }

        if (coreCount % 2 == 0)
        {
            /* Add Filler */
            gcmSETFILLER_NEW(reserve, memory);
        }

    /* End batch */
    gcmENDSTATEBATCH_NEW(reserve, memory);

    if (coreCount > 1)
    {
        switch (hwRenderMode)
        {
            case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_64x64:
            {
                renderMode = 0x1;
                break;
            }

            case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_128x64:
            {
                renderMode = 0x2;
                break;
            }

            case gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_128x128:
            {
                renderMode = 0x3;
                break;
            }

            default:
            {
                /* Other types are not supported for now */
                renderMode = 0x1;
                gcmASSERT(gcvFALSE);
                break;
            }
        }

        /* Enable stream out for Multi GPU*/
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5100) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x5100, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) ((coreCount - 1)) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (writeTrough) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (Hardware->indexFormat) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (renderMode) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }
    else
    {
        /* Enable stream out */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5100) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x5100, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (writeTrough) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (Hardware->indexFormat) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    /* Start collecting */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5101) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x5101, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))));    gcmENDSTATEBATCH_NEW(reserve, memory);};


#if gcdMULTI_GPU
    /* Enable all cores */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Reset stream out dirty */
    Hardware->streamOutDirty = gcvFALSE;

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Initialize stream out and start recording (WriteThrough:%d) (%s)",
                  writeTrough,
                  __FUNCTION__);

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_StopSignalStreamOut(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;

#if gcdMULTI_GPU
    gctUINT numberOfGPUs = Hardware->config->gpuCoreCount;
#else
    gctUINT numberOfGPUs = 1;
#endif

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=%p", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Reset unused pointers. */
    reserve = gcvNULL;
    stateDelta = gcvNULL;

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

#if gcdMULTI_GPU
    /* Add extra space for core enable/disable */
    reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

#if gcdMULTI_GPU
    /* Enable core 0 */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };
#endif

    if (numberOfGPUs > 1)
    {
        /* Disable stream out */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5100, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5100) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5100, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) ((numberOfGPUs - 1)) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))));     gcmENDSTATEBATCH(reserve, memory);};
    }
    else
    {
        /* Disable stream out */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5100, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5100) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5100, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))));     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Flush indices */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5101, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5101) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5101, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))));     gcmENDSTATEBATCH(reserve, memory);};

#if gcdMULTI_GPU
    /* Enable all cores */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Stop signal stream out and spit out new indices (%s)",
                  __FUNCTION__);

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SyncStreamOut (
    IN gcoHARDWARE Hardware
)
{
    gceSTATUS status;

#if !gcdSTREAM_OUT_NAIVE_SYNC
    gctUINT32 source, destination;
#endif

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

#if gcdMULTI_GPU

#if gcdSTREAM_OUT_NAIVE_SYNC

    gcmONERROR(gcoHARDWARE_MultiGPUSync(Hardware, gcvTRUE, gcvNULL));

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Multi GPU stream out NAIVE sync (%s)",
                  __FUNCTION__);

#else /* gcdSTREAM_OUT_NAIVE_SYNC */

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Make compiler happy */
    stateDelta = stateDelta;

    /* Select core that performed the first phase */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };

    /* Send FE->PA semaphore to let core to spit out indices */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
    destination =  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    *memory++ = source | destination;

    /* Dump the semaphore. */
    gcmDUMP(gcvNULL, "#[semaphore 0x%08X 0x%08X]", source, destination);

    /* Send FE - PA stall to let core to spit out indices */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
    destination =  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    *memory++ = source | destination;

    /* Dump the stall. */
    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]", source, destination);

    /* Set interGPU semaphore for the other core */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (gcvCORE_3D_1_ID) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.semaphore 0x%04X]", gcvCORE_3D_1_ID);

    /* Stall the core for the same semaphore */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    /* Advance */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (gcvCORE_3D_0_ID) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.stall 0x%04X]", gcvCORE_3D_0_ID);

    /* Select the other core */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_1_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_1_MASK); };

    /* Set interGPU semaphore for the other core */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (gcvCORE_3D_0_ID) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.semaphore 0x%04X]", gcvCORE_3D_0_ID);

    /* Stall the core for the same semaphore */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    /* Advance */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (gcvCORE_3D_1_ID) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.stall 0x%04X]", gcvCORE_3D_1_ID);

    /* Select both cores */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Multi GPU stream out sync (%s)",
                  __FUNCTION__);

#endif /* gcdSTREAM_OUT_NAIVE_SYNC */

#else /* gcdMULTI_GPU */

#if gcdSTREAM_OUT_NAIVE_SYNC

    /* Add semaphore stall. Wait for PA to spit out the indices. */
    gcmONERROR(gcoHARDWARE_Semaphore(Hardware,
                                     gcvWHERE_COMMAND,
                                     gcvWHERE_PIXEL,
                                     gcvHOW_SEMAPHORE_STALL,
                                     gcvNULL));

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Single GPU stream out NAIVE sync (sema/stall) (%s)",
                  __FUNCTION__);

#else /* gcdSTREAM_OUT_NAIVE_SYNC */

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    /* Add semaphore stall. Wait for PA to spit out the indices. */
    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1+1) * gcmSIZEOF(gctUINT32), 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Make compiler happy */
    stateDelta = stateDelta;

    /* Send FE->PA semaphore to let core to spit out indices */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
    destination =  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    *memory++ = source | destination;

    /* Dump the semaphore. */
    gcmDUMP(gcvNULL, "#[semaphore 0x%08X 0x%08X]", source, destination);

    /* Send FE - PA stall to let core to spit out indices */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
    destination =  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    *memory++ = source | destination;

    /* Dump the stall. */
    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]", source, destination);

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Single GPU stream out sync (sema/stall) (%s)",
                  __FUNCTION__);

#endif /* gcdSTREAM_OUT_NAIVE_SYNC */
#endif /* gcdMULTI_GPU */

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#endif

/*
** For PS outmode, we can't use map table as RI32/RF32, RI332GI32/RF32GF32 have different setting.
*/
static gctINT32 _GetPsOutPutMode(gcsSURF_INFO_PTR surface)
{
    /* set default value */
    gctINT32 outputmode = 0x0;
    gceSURF_FORMAT format;

    if (!surface)
    {
        return outputmode;
    }

    format = surface->format;
    if ((format >= gcvSURF_R8I) && (format <= gcvSURF_A2B10G10R10UI))
    {
        if (((format >= gcvSURF_R8I) && (format <= gcvSURF_R16UI)) ||
            ((format >= gcvSURF_X8R8I) && (format <= gcvSURF_G16R16UI)) ||
            ((format >= gcvSURF_X8G8R8I) && (format <= gcvSURF_B16G16R16UI)) ||
            ((format >= gcvSURF_X8B8G8R8I) && (format <= gcvSURF_A16B16G16R16UI)) ||
            ((format == gcvSURF_A2B10G10R10UI)))
        {
            outputmode = 0x1;
        }
        else
        {
            outputmode = 0x2;
        }
    }

    if (format == gcvSURF_X32B32G32R32I_2_G32R32I ||
        format == gcvSURF_A32B32G32R32I_2_G32R32I ||
        format == gcvSURF_X32B32G32R32UI_2_G32R32UI ||
        format == gcvSURF_A32B32G32R32UI_2_G32R32UI)
    {
        outputmode = 0x2;
    }

    return outputmode;
}

static gceSTATUS _RemapPsOutput(gcoHARDWARE Hardware, gcsHINT_PTR psHints, gctUINT32 oldValue, gctUINT32 *newValue)
{
    gctUINT32 output[gcdMAX_DRAW_BUFFERS];
    gctUINT32 stateValue = 0;
    gctUINT index;
    gctUINT rtIndex;
    output[0] = (((((gctUINT32) (oldValue)) >> (0 ? 5:0)) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1)))))) );
    output[1] = (((((gctUINT32) (oldValue)) >> (0 ? 13:8)) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1)))))) );
    output[2] = (((((gctUINT32) (oldValue)) >> (0 ? 21:16)) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1)))))) );
    output[3] = (((((gctUINT32) (oldValue)) >> (0 ? 29:24)) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1)))))) );
    output[4] = (((((gctUINT32) (oldValue)) >> (0 ? 5:0)) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1)))))) );
    output[5] = (((((gctUINT32) (oldValue)) >> (0 ? 13:8)) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1)))))) );
    output[6] = (((((gctUINT32) (oldValue)) >> (0 ? 21:16)) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1)))))) );
    output[7] = (((((gctUINT32) (oldValue)) >> (0 ? 29:24)) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1)))))) );

#if gcdPACKED_OUTPUT_ADDRESS
    for (index = 0; index < Hardware->colorOutCount; index++)
    {
        gctUINT j;
        rtIndex = Hardware->psOutputMapping[index];

        for (j = 0; j < Hardware->config->renderTargets; j++)
        {
            if (psHints->psOutput2RtIndex[j] == (gctINT)rtIndex)
            {
                break;
            }
        }

        if (j >= Hardware->config->renderTargets)
        {
            continue;
        }

        switch (index)
        {
        case 0:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)));
            break;

        case 1:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8)));
            break;

        case 2:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16)));
            break;

        case 3:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)));
            break;

        case 4:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)));
            break;

        case 5:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8)));
            break;

        case 6:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16)));
            break;

        case 7:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (output[j]) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)));
            break;

        default:
            gcmASSERT(0);
        }
    }

#else
    for (index = 0; index < Hardware->colorOutCount; index++)
    {
        rtIndex = Hardware->psOutputMapping[index];

        switch (index)
        {
        case 0:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (output[rtIndex]) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)));
            break;

        case 1:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (output[rtIndex]) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8)));
            break;

        case 2:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (output[rtIndex]) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16)));
            break;

        case 3:
            stateValue = ((((gctUINT32) (stateValue)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (output[rtIndex]) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)));
            break;

        default:
            gcmASSERT(0);
        }


    }
#endif
    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE,
             "Remapping ps output, before=0x%x, after=0x%x",*newValue, stateValue);

    /* overwrite with remapping data before sending to command buffer*/
    *newValue = stateValue;

    return gcvSTATUS_OK;

}


static gceSTATUS
_GetPsOutputSetting(
    gcoHARDWARE Hardware,
    gctUINT32 *outputMode,
    gctUINT32 *saturationMode,
    gctUINT32 *psOutCnl4to7)
{
    gctUINT i;

    static const gctUINT32 colorOutSaturation[] =
    {
        gcvTRUE, /* 0x00: X4R4G4B4           */
        gcvTRUE, /* 0x01: A4R4G4B4           */
        gcvTRUE, /* 0x02: X1R5G5B5           */
        gcvTRUE, /* 0x03: A1R5G5B5           */
        gcvTRUE, /* 0x04: R5G6B5             */
        gcvTRUE, /* 0x05: X8R8G8B8           */
        gcvTRUE, /* 0x06: A8R8G8B8           */
        gcvTRUE, /* 0x07: YUY2               */
        gcvTRUE, /* 0x08: UYVY               */
        gcvTRUE, /* 0x09: INDEX8             */
        gcvTRUE, /* 0x0A: MONOCHROME         */
        gcvTRUE, /* 0x0B: HDR7E3             */
        gcvTRUE, /* 0x0C: HDR6E4             */
        gcvTRUE, /* 0x0D: HDR5E5             */
        gcvTRUE, /* 0x0E: HDR6E5             */
        gcvTRUE, /* 0x0F: YV12               */
        gcvTRUE, /* 0x10: A8                 */
        gcvFALSE, /* 0x11: RF16               */
        gcvFALSE, /* 0x12: RF16GF16           */
        gcvFALSE, /* 0x13: RF16GF16BF16AF16   */
        gcvFALSE, /* 0x14: RF32, RI32         */
        gcvFALSE, /* 0x15: RF32GF32, RI32GI32 */
        gcvTRUE, /* 0x16: R10G10B10A2        */
        gcvFALSE, /* 0x17: RI8                */
        gcvFALSE, /* 0x18: RI8GI8             */
        gcvFALSE, /* 0x19: RI8GI8BI8AI8       */
        gcvFALSE, /* 0x1A: RI16               */
        gcvFALSE, /* 0x1B: RI16GI16           */
        gcvFALSE, /* 0x1C: RI16GI16BI16AI16   */
        gcvFALSE, /* 0x1D: RF11GF11BF10       */
        gcvFALSE, /* 0x1E: RI10GI10BI10AI2    */
        gcvTRUE, /* 0x1F: R8G8               */
        gcvFALSE, /* 0x20: placeholder        */
        gcvFALSE, /* 0x21: placeholder        */
        gcvFALSE, /* 0x22: placeholder        */
        gcvTRUE, /* 0x23: R8                 */
    };

    gcmASSERT(outputMode);
    gcmASSERT(saturationMode);

    *outputMode =
    *saturationMode = 0;

    for (i = 0 ; i < Hardware->colorOutCount; i++)
    {
        gcmASSERT(Hardware->colorStates.target[i].format < gcmCOUNTOF(colorOutSaturation));
        switch (i)
        {
        case 0:
        *saturationMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));
        break;

        case 1:
        *saturationMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4)));
        break;

        case 2:
        *saturationMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)));
        break;

        case 3:
        *saturationMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)));
        break;

        case 4:
        *psOutCnl4to7 |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 18:16) - (0 ? 18:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:16) - (0 ? 18:16) + 1))))))) << (0 ? 18:16)));
        break;

        case 5:
        *psOutCnl4to7 |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20)));
        break;

        case 6:
        *psOutCnl4to7 |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)));
        break;

        case 7:
        *psOutCnl4to7 |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (colorOutSaturation[Hardware->colorStates.target[i].format]) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));

        *outputMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (_GetPsOutPutMode(Hardware->colorStates.target[i].surface)) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)));
        break;

        default:
            gcmASSERT(0);

        }
    }

    return gcvSTATUS_OK;
}

static gctBOOL
_NeedStallHw(
    gcoHARDWARE Hardware
    )
{
    gctBOOL semaStall = gcvFALSE;
    gcsHINT_PTR hints = Hardware->shaderStates.hints;
    gcsPROGRAM_UNIFIED_STATUS *prevProgUnifiedStatus = &Hardware->prevProgramUnfiedStatus;

    do
    {
        /* Special case for shared instruction memory. */
        if (((Hardware->config->instructionCount > 256) &&
             (Hardware->config->instructionCount <= 1024) &&
             (Hardware->features[gcvFEATURE_BUG_FIXES7] != gcvTRUE))
            )
        {
            semaStall = gcvTRUE;
            break;
        }

        if (prevProgUnifiedStatus->useIcache != hints->unifiedStatus.useIcache)
        {
            semaStall = gcvTRUE;
            break;
        }

        if (prevProgUnifiedStatus->instruction != hints->unifiedStatus.instruction)
        {
            gcmASSERT(prevProgUnifiedStatus->useIcache != hints->unifiedStatus.useIcache);
        }

        if ((hints->unifiedStatus.instruction) && (!hints->unifiedStatus.useIcache))
        {
            if ((hints->unifiedStatus.instVSEnd >= prevProgUnifiedStatus->instPSStart) ||
                (hints->unifiedStatus.instPSStart <= prevProgUnifiedStatus->instVSEnd))
            {
                semaStall = gcvTRUE;
                break;
            }
        }

        if (hints->unifiedStatus.constant)
        {
            if ((hints->unifiedStatus.constVSEnd >= prevProgUnifiedStatus->constPSStart) ||
                (hints->unifiedStatus.constPSStart <= prevProgUnifiedStatus->constVSEnd))
            {
                semaStall = gcvTRUE;
                break;
            }
        }

        if (hints->unifiedStatus.sampler)
        {
            if ((hints->unifiedStatus.samplerVSEnd >= prevProgUnifiedStatus->samplerPSStart) ||
                (hints->unifiedStatus.samplerPSStart <= prevProgUnifiedStatus->samplerVSEnd))
            {
                semaStall = gcvTRUE;
                break;
            }
        }

    }while (gcvFALSE);

    /* overwrite to previous one */
    gcoOS_MemCopy(&Hardware->prevProgramUnfiedStatus,
                  &hints->unifiedStatus,
                  gcmSIZEOF(gcsPROGRAM_UNIFIED_STATUS));

    return semaStall;
}


gceSTATUS
gcoHARDWARE_FlushShaders(
    IN gcoHARDWARE Hardware,
    IN gcePRIMITIVE PrimitiveType,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL pointList, bypass, semaStall = gcvFALSE;
    gctUINT32 msaa, fastMsaa = 0, scale;
    gctUINT32 output;
    gctSIZE_T bufferCount;
    gctUINT32 i, address, count;
    gctUINT32_PTR buffer;
    gctSIZE_T offset;
    gcsHINT_PTR hints;
    gctUINT32 outputMode, saturationMode0to3;
    gctUINT32 msaaExtraTemp = 0;
    gctUINT32 msasExtraInput = 0;
    gcePATCH_ID patchId = gcvPATCH_INVALID;
#if gcdALPHA_KILL_IN_SHADER
    gctBOOL alphaKill;
    gctBOOL colorKill;
#endif
    gctUINT32 numNops = 0;
    gctBOOL  remapPSout = gcvFALSE;
    gctUINT32 psOutCntl4to7 = ~0U;
    gctINT32 psOuts = 0;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x Type=%d", Hardware, PrimitiveType);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    patchId = Hardware->patchID;

    /* Shortcut to hints. */
    hints = Hardware->shaderStates.hints;

    semaStall = _NeedStallHw(Hardware);

    if (Hardware->swwas[gcvSWWA_1163])
    {
        numNops = 8;
    }

#if gcdPACKED_OUTPUT_ADDRESS
    if (hints->stageBits & gcvPROGRAM_STAGE_FRAGMENT_BIT)
    {
        for (i = 0; i < gcdMAX_DRAW_BUFFERS; ++i)
        {
            psOuts = gcmMAX(psOuts, hints->psOutput2RtIndex[i]);
        }
        psOuts = gcmMIN(psOuts, (gctINT32)(Hardware->colorOutCount - 1));
        psOuts = gcmMAX(psOuts, 0);

        for (i = 0; i < Hardware->colorOutCount; i++)
        {
            if (Hardware->psOutputMapping[i] != hints->psOutput2RtIndex[i])
            {
                remapPSout = gcvTRUE;
#if gcdDEBUG
                gcmPRINT("Shader output mismatch with MRT setting");
#endif
                break;
            }
        }
    }
#else
    for (i = 0; i < Hardware->colorOutCount; i++)
    {
        if (((gctUINT32)Hardware->psOutputMapping[i] != i) &&
            hints->stageBits & gcvPROGRAM_STAGE_FRAGMENT_BIT)
        {
            remapPSout = gcvTRUE;
            break;
        }
    }
#endif


#if gcdALPHA_KILL_IN_SHADER
    /* Determine alpka kill. */
    alphaKill = (Hardware->alphaStates.blend
                 &&
                 (hints->killStateAddress != 0)
                 &&
                 (Hardware->alphaStates.srcFuncColor == gcvBLEND_SOURCE_ALPHA)
                 &&
                 (Hardware->alphaStates.srcFuncAlpha == gcvBLEND_SOURCE_ALPHA)
                 &&
                 (Hardware->alphaStates.trgFuncColor == gcvBLEND_INV_SOURCE_ALPHA)
                 &&
                 (Hardware->alphaStates.trgFuncAlpha == gcvBLEND_INV_SOURCE_ALPHA)
                 &&
                 (Hardware->alphaStates.modeColor == gcvBLEND_ADD)
                 &&
                 (Hardware->alphaStates.modeAlpha == gcvBLEND_ADD)
                 );

    /* Determine color kill. */
    colorKill = (Hardware->alphaStates.blend
                 &&
                 (hints->killStateAddress != 0)
                 &&
                 (Hardware->alphaStates.srcFuncColor == gcvBLEND_ONE)
                 &&
                 (Hardware->alphaStates.srcFuncAlpha == gcvBLEND_ONE)
                 &&
                 (Hardware->alphaStates.trgFuncColor == gcvBLEND_ONE)
                 &&
                 (Hardware->alphaStates.trgFuncAlpha == gcvBLEND_ONE)
                 &&
                 (Hardware->alphaStates.modeColor == gcvBLEND_ADD)
                 &&
                 (Hardware->alphaStates.modeAlpha == gcvBLEND_ADD)
                 &&
                 !Hardware->depthStates.write
                 );

#endif

    if (hints->stageBits & gcvPROGRAM_STAGE_VERTEX_BIT)
    {
        gcmDUMP(gcvNULL, "#[vsid %d]", hints->vertexShaderId);
    }

    if (hints->stageBits & gcvPROGRAM_STAGE_FRAGMENT_BIT)
    {
        gcmDUMP(gcvNULL, "#[psid %d]", hints->fragmentShaderId);
    }


    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    if (semaStall)
    {
        {    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));    gcmENDSTATEBATCH_NEW(reserve, memory);};    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));    gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]", ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))), ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));    gcmSKIPSECUREUSER();};
    }

    /* Init the state buffer. */
    bufferCount = Hardware->shaderStates.stateBufferSize;
    buffer      = Hardware->shaderStates.stateBuffer;

    /* Loop through all bytes. */
    for (offset = 0; offset < bufferCount;)
    {
        gctUINT32 saveValue = 0;
        gctUINT32_PTR savePos;
        /* Extract fields. */
        address = (((((gctUINT32) (*buffer)) >> (0 ? 15:0)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1)))))) );
        count   = (((((gctUINT32) (*buffer)) >> (0 ? 25:16)) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1)))))) );

        /* Skip the command. */
        buffer += 1;

        /* By default no savePos */
        savePos = gcvNULL;

        if (remapPSout &&
            ((address == 0x0401) ||
             (address == 0x040B)))
        {
            saveValue = *buffer;
            savePos = buffer;
            /* overwrite with remapping data before sending to command buffer*/
            _RemapPsOutput(Hardware, hints, *buffer, buffer);
        }

        if (address == 0x040B)
        {
            psOutCntl4to7 = *buffer;
        }

        /* Load the states. */
        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)count <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (count) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            for (i = 0; i < count; i += 1)
            {
                gcmSETSTATEDATA_NEW(
                    stateDelta, reserve, memory, gcvFALSE, address + i,
                    *buffer++
                    );
            }

            if ((count & 1) == 0)
            {
                gcmSETFILLER_NEW(reserve, memory);

                buffer += 1;
                offset += 4 * (1 + count + 1);
            }
            else
            {
                offset += 4 * (1 + count);
            }

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* restore the state buffer to be original one */
        if (savePos)
        {
            *savePos = saveValue;
        }
    }

    /* Determine MSAA mode. */
    msaa = ((Hardware->samples.x * Hardware->samples.y > 1)
         && (Hardware->sampleMask != 0)
         && !Hardware->vaa
         )
         ? 1
         : 0;

    /* real depth only need to set bypass mode */
    bypass = Hardware->depthStates.realOnly;

    /* Determine Fast MSAA mode. */
    if ((Hardware->samples.x > 1)
     && (Hardware->samples.y > 1)
     && Hardware->features[gcvFEATURE_FAST_MSAA])
    {
        /* No extra info needed for msaa. */
        fastMsaa = 1;
    }

    /* For Non-FAST MSAA case, need extra temp register to pass sampleDepth infor.
    ** For Dual16, need another one.
    */
    if (msaa && !fastMsaa)
    {
        msaaExtraTemp = Hardware->shaderStates.hints->fsIsDual16 ? 2 : 1;
        msasExtraInput = 1;
    }

    /* Shortcut to hints. */
    hints = Hardware->shaderStates.hints;

    /* Point list primitive? */
    pointList = (PrimitiveType == gcvPRIMITIVE_POINT_LIST);

    _GetPsOutputSetting(Hardware, &outputMode, &saturationMode0to3, &psOutCntl4to7);

    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)3  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0402) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        /* Set input count. */
        gcmSETSTATEDATA_NEW(
            stateDelta, reserve, memory, gcvFALSE, 0x0402,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (hints->fsInputCount + msasExtraInput) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (hints->psInputControlHighpPosition) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
            );
#if TEMP_SHADER_PATCH
        if (hints != gcvNULL)
        {
            switch (hints->pachedShaderIdentifier)
            {
            case gcvMACHINECODE_GLB27_RELEASE_0:
                hints->fsMaxTemp = 0x00000004;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_0:
                hints->fsMaxTemp = 0x00000008;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_1:
                hints->fsMaxTemp = 0x00000008;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_2:
                hints->fsMaxTemp = 0x00000004;
                break;

            default:
                break;
            }
        }
#endif

        /* Set temporary register control. */
        gcmSETSTATEDATA_NEW(
            stateDelta, reserve, memory, gcvFALSE, 0x0403,
            hints->fsMaxTemp + msaaExtraTemp);

        /* Set bypass for depth-only modes. */
        /* COLOR_OUT_COUNT = numRT -1 */
        gcmSETSTATEDATA_NEW(
            stateDelta, reserve, memory, gcvFALSE, 0x0404,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (bypass) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            | saturationMode0to3
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (fastMsaa) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) ((gctUINT32)psOuts) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8)))
            );

    gcmENDSTATEBATCH_NEW(
        reserve, memory
        );

    /* Set element count. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x028C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x028C, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (bypass ? 0 : hints->elementCount) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
#if TEMP_SHADER_PATCH
    if (hints && hints->pachedShaderIdentifier == gcvMACHINECODE_ANTUTU0)
    {
        hints->componentCount = 0x00000006;
    }
#endif

    /* Set component count. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E07) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E07, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:0) - (0 ? 6:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:0) - (0 ? 6:0) + 1))))))) << (0 ? 6:0))) | (((gctUINT32) ((gctUINT32) (bypass ? 0 : hints->componentCount) & ((gctUINT32) ((((1 ? 6:0) - (0 ? 6:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:0) - (0 ? 6:0) + 1))))))) << (0 ? 6:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

    while (numNops--)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E07) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0E07, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:0) - (0 ? 6:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:0) - (0 ? 6:0) + 1))))))) << (0 ? 6:0))) | (((gctUINT32) ((gctUINT32) (bypass ? 0 : hints->componentCount) & ((gctUINT32) ((((1 ? 6:0) - (0 ? 6:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:0) - (0 ? 6:0) + 1))))))) << (0 ? 6:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x040C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x040C, outputMode );    gcmENDSTATEBATCH_NEW(reserve, memory);};

    if (psOutCntl4to7 != ~0U)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x040B) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x040B, psOutCntl4to7 );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }

#if gcmIS_DEBUG(gcdDEBUG_ASSERT)
    if (Hardware->config->chipModel < gcv1000)
    {
        gcmASSERT(hints->componentCount == (hints->elementCount * 4));
    }
#endif

    if (patchId == gcvPATCH_GLBM27 || patchId == gcvPATCH_GFXBENCH)
    {
        scale = hints->balanceMax;
    }
    /* Scale to 50%. */
    else
    {
        scale
            = ((Hardware->config->chipModel == gcv400) || (Hardware->config->chipRevision >= 0x5240))
                ?  hints->balanceMax
                : (hints->balanceMin + hints->balanceMax) / 2;
    }

    gcmASSERT(scale > 0 ||
        hints->stageBits & (gcvPROGRAM_STAGE_OPENCL_BIT | gcvPROGRAM_STAGE_COMPUTE_BIT));

    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x020C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x020C, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (scale) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (hints->balanceMin) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

#if gcdAUTO_SHIFT
    if (hints->autoShift)
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0216) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0216, ((((gctUINT32) (0x00001005)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (gcmMIN(hints->balanceMax * 2, 255)) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }
    else
    {
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0216) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0216, 0x00001005);    gcmENDSTATEBATCH_NEW(reserve, memory);};
    }
#endif

    if (bypass)
    {
        /* In bypass mode, we only need the position output and the point
        ** size if present and the primitive is a point list. */
        output
            = 1 + ((hints->vsHasPointSize && pointList) ? 1 : 0);
    }
    else
    {
        /* If the Vertex Shader generates a point size, and the primitive is
        ** not a point, we don't need to send it down. */
        output
            = (hints->vsHasPointSize && !pointList)
                ? hints->vsOutputCount - 1
                : hints->vsOutputCount;
    }

    /* SW WA for transform feedback, when VS has no output. */
    if (output == 0)
    {
        output = 1;
    }

    /* Set output count. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0201) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0201, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (output) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (hints->vsOutput16RegNo) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (hints->vsOutput17RegNo) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (hints->vsOutput18RegNo) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

#if gcdALPHA_KILL_IN_SHADER
    if (alphaKill || colorKill)
    {
        gcmASSERT(hints->stageBits & gcvPROGRAM_STAGE_FRAGMENT_BIT);
        /* Reprogram endPC address. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (hints->killStateAddress) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, hints->killStateAddress, alphaKill ? hints->alphaKillStateValue : hints->colorKillStateValue);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Reprogram shader instructions. */
        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)3 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (hints->killInstructionAddress) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA_NEW(stateDelta,
                        reserve,
                        memory,
                        gcvFALSE,
                        hints->killInstructionAddress + 0,
                        alphaKill
                        ? hints->alphaKillInstruction[0]
                        : hints->colorKillInstruction[0]);
        gcmSETSTATEDATA_NEW(stateDelta,
                        reserve,
                        memory,
                        gcvFALSE,
                        hints->killInstructionAddress + 1,
                        alphaKill
                        ? hints->alphaKillInstruction[1]
                        : hints->colorKillInstruction[1]);
        gcmSETSTATEDATA_NEW(stateDelta,
                        reserve,
                        memory,
                        gcvFALSE,
                        hints->killInstructionAddress + 2,
                        alphaKill
                        ? hints->alphaKillInstruction[2]
                        : hints->colorKillInstruction[2]);

        gcmENDSTATEBATCH_NEW(reserve, memory);
    }
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Reset dirty. */
    Hardware->shaderDirty = gcvFALSE;

    if (hints->unifiedStatus.constant)
    {
        Hardware->flushSHL1 = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_HzClearValueControl(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 ZClearValue,
    OUT gctUINT32 * HzClearValue OPTIONAL,
    OUT gctUINT32 * HzControl OPTIONAL
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 clearValue = 0, control = 0;

    gcmHEADER_ARG("Format=%d ZClearValue=0x%x", Format, ZClearValue);

    /* Dispatch on format. */
    switch (Format)
    {
    case gcvSURF_D16:
        /* 16-bit Z. */
        clearValue = ZClearValue;
        control    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0x5 & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                   | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) (0x5 & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
        break;

    case gcvSURF_D24X8:
    case gcvSURF_D24S8:
        /* 24-bit Z. */
        clearValue = ZClearValue >> 8;
        control    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0x8 & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                   | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) (0x8 & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
        break;

    case gcvSURF_S8:
        gcmFOOTER();
        return gcvSTATUS_OK;

    default:
        /* Not supported. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (HzClearValue != gcvNULL)
    {
        /* Return clear value. */
        *HzClearValue = clearValue;
    }

    if (HzControl != gcvNULL)
    {
        /* Return control. */
        *HzControl = control;
    }

    /* Success. */
    gcmFOOTER_ARG("*HzClearValue=0x%x *HzControl=0x%x",
                  gcmOPT_VALUE(HzClearValue), gcmOPT_VALUE(HzControl));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetWClipEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Wclip=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the state. */
    if (Hardware->paStates.wclip != Enable)
    {
        Hardware->paStates.wclip = Enable;
        Hardware->paConfigDirty    = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetWPlaneLimit(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Value
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Value=%f", Hardware, Value);

    do
    {
        /* Switch to 3D pipe. */
        gcmERR_BREAK(gcoHARDWARE_SelectPipe(Hardware, 0x0, gcvNULL));

        gcmERR_BREAK(gcoHARDWARE_LoadState(Hardware,
                                           0x00A2C,
                                           1,
                                           &Value));
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetRTNERounding(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x RTNERounding=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Check if rtneRounding mode needs to change. */
    if (Hardware->rtneRounding != Enable)
    {
        if (Enable)
        {
            /* Only handle when the IP has RTNE */
            if (((((gctUINT32) (Hardware->config->chipMinorFeatures3)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1))))))))
            {
                /* Perform a load state. */
                gcmONERROR(gcoHARDWARE_LoadState32(
                    Hardware,
                    0x00860,
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (0x1) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                    ));

                Hardware->rtneRounding = Enable;
            }
        }
        else
        {
            /* Perform a load state. */
            gcmONERROR(gcoHARDWARE_LoadState32(
                Hardware,
                0x00860,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (0x0) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                ));

            Hardware->rtneRounding = Enable;
        }
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetColorOutCount(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorOutCount
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x ColorOutCount=%d", Hardware, ColorOutCount);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Assume
        1) 3D pipe,
        2) HALTI0 features are available,
        3) HW supports ColorOutCount buffers as RT.
     */

    gcmASSERT(ColorOutCount <= Hardware->config->renderTargets);

    Hardware->colorOutCount = ColorOutCount;
    Hardware->shaderDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_ResumeOQ(
    IN gcoHARDWARE Hardware,
    IN gcsOQ *OQ,
    IN gctUINT64 CommandBuffer
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gctUINT32 physic;

    gctUINT32_PTR memory = (gctUINT32_PTR) gcmUINT64_TO_PTR(CommandBuffer);

    gcmHEADER_ARG("Hardware=0x%x OQ=0x%x CommandBuffer=0x%x", Hardware, OQ, CommandBuffer);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmASSERT(Hardware->currentPipe == gcvPIPE_3D);


    OQ->oqIndex++;

    if (OQ->oqIndex >= 64)
    {
        gcmFATAL("commit 64 time, when OQ enable!!!!");
        OQ->oqIndex = 63;
    }

    physic = gcmALL_TO_UINT32(OQ->oqPhysic) + (OQ->oqIndex) * gcmSIZEOF(gctUINT64);

    if (!((((gctUINT32) (Hardware->config->chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    memory[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E09) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    memory[1] = physic;

    OQ->oqStatus = gcvOQ_Enable;

    gcmDUMP(gcvNULL,
            "#[state 0x%04X 0x%08X]",
            0x0E09, memory[1]);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoHARDWARE_PauseOQ(
    IN gcoHARDWARE Hardware,
    IN gcsOQ *OQ,
    IN gctUINT64 CommandBuffer
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gctUINT32_PTR memory = (gctUINT32_PTR) gcmUINT64_TO_PTR(CommandBuffer);

    gcmHEADER_ARG("Hardware=0x%x OQ=0x%x CommandBuffer=0x%x", Hardware, OQ, CommandBuffer);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmASSERT(Hardware->currentPipe == gcvPIPE_3D);

    if (!((((gctUINT32) (Hardware->config->chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }


    memory[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E0C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
    memory[1] = 31415926;

    OQ->oqStatus = gcvOQ_Paused;

    gcmDUMP(gcvNULL,
            "#[state 0x%04X 0x%08X]",
            0x0E0C, memory[1]);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoHARDWARE_SetOQ(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER OQ,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsOQ *oq = OQ;

    gcmHEADER_ARG("Hardware=0x%x OQ = %d, Enable = %u", Hardware, OQ, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (!((((gctUINT32) (Hardware->config->chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }
    if (Hardware->enableOQ != Enable)
    {
        Hardware->enableOQ = Enable;

        /* Switch to 3D pipe. */
        gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

        if (Enable)
        {
            gcmASSERT(oq);
            gcmONERROR(gcoHARDWARE_LoadCtrlState(
                Hardware, 0x03824, (gctUINT32)((gctUINTPTR_T)(oq->oqPhysic))));


            /* Enable atomic as OQ can't support context switch */
            gcmONERROR(gcoBUFFER_AttachOQ(Hardware->buffer, oq));

            if (Hardware->depthStates.surface != gcvNULL &&
                !Hardware->depthStates.surface->hzDisabled)
            {
                Hardware->depthStates.surface->hzDisabled = gcvTRUE;
            }
        }
        else
        {
            gctBOOL hzTSEnabled = gcvFALSE;

            gcmONERROR(gcoHARDWARE_InvalidateCache(Hardware, gcvFALSE));


            hzTSEnabled = (((((gctUINT32) (Hardware->memoryConfig)) >> (0 ? 12:12)) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) );

            /* pause HZ TS */
            if (hzTSEnabled)
            {
                gcoHARDWARE_PauseTileStatus(Hardware,gcvTILE_STATUS_PAUSE);
            }

            if (oq->oqStatus == gcvOQ_Enable)
            {
                gcmONERROR(gcoHARDWARE_LoadCtrlState(
                    Hardware, 0x03830, 31415926
                    ));
            }

            /* Exit atomic state */
            gcmONERROR(gcoBUFFER_AttachOQ(Hardware->buffer, gcvNULL));

             /* resume HZ TS */
            if (hzTSEnabled)
            {
                gcoHARDWARE_PauseTileStatus(Hardware,gcvTILE_STATUS_RESUME);
            }
        }

        /* OQ state change could affect current depth HZ status,
        which depend on depth config dirty and Target dirty now,
        TO_DO, refine*/
        Hardware->depthConfigDirty = gcvTRUE;
        Hardware->depthTargetDirty = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_PrimitiveRestart(
    IN gcoHARDWARE Hardware,
    IN gctBOOL PrimitiveRestart
    )
{
    gceSTATUS   status;

    gcmHEADER_ARG("Hardware=0x%x PrimitiveRestart=%d", Hardware, PrimitiveRestart);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Test if primitive restart is present. */
    if (PrimitiveRestart && !((((gctUINT32) (Hardware->config->chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Set primitive restart options. */
    Hardware->indexDirty       = gcvTRUE;
    Hardware->primitiveRestart = PrimitiveRestart;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FastDrawIndexedPrimitive(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory)
{
    gceSTATUS status;
    gctUINT primCount = FastFlushInfo->drawCount / 3;

    gcmHEADER_ARG("Hardware=0x%x FastFlushInfo=0x%x", Hardware, FastFlushInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Send command */
    if (FastFlushInfo->hasHalti)
    {
        gctSIZE_T vertexCount;
        gctUINT32 drawCommand;
        gctUINT32 drawCount;
        gctUINT32 drawMode = (FastFlushInfo->indexCount != 0) ?
                0x1 :
                0x0;

        /* Define state buffer variables. */
        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

        if (!Hardware->features[gcvFEATURE_HALTI2])
        {
            vertexCount = FastFlushInfo->drawCount - (FastFlushInfo->drawCount % 3);
        }
        else
        {
            vertexCount = FastFlushInfo->drawCount;
        }

        /* Determine draw command. */
        drawCommand = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0C & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20))) | (((gctUINT32) ((gctUINT32) (drawMode) & ((gctUINT32) ((((1 ? 22:20) - (0 ? 22:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:20) - (0 ? 22:20) + 1))))))) << (0 ? 22:20)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (FastFlushInfo->instanceCount) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (0x4) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)));

        drawCount = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:0) - (0 ? 23:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:0) - (0 ? 23:0) + 1))))))) << (0 ? 23:0))) | (((gctUINT32) ((gctUINT32) (vertexCount) & ((gctUINT32) ((((1 ? 23:0) - (0 ? 23:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:0) - (0 ? 23:0) + 1))))))) << (0 ? 23:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) ((FastFlushInfo->instanceCount >> 16)) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)));

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E05) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0E05, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Program the AQCommandDX8Primitive.Command data. */
        *memory++ = drawCommand;
        *memory++ = drawCount;

        if (Hardware->features[gcvFEATURE_HALTI2])
        {
            *memory++ = 0;
            *memory++ = 0;
        }
        else
        {
            /* start index is not set */
            *memory++ = 0;

            gcmSETFILLER_NEW(
                reserve,
                memory
                );
        }

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }
    else
    {
        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E05) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0E05, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Program the AQCommandDX8IndexPrimitive.Command data. */
        memory[0] =
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

        /* Program the AQCommandDX8IndexPrimitive.Primtype data. */
        memory[1] =
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (0x4) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)));

        /* Program the AQCommandDX8IndexPrimitive.Start data. */
        memory[2] = 0;

        /* Program the AQCommandDX8IndexPrimitive.Primcount data. */
        gcmSAFECASTSIZET(memory[3], primCount);

        /* Program the AQCommandDX8IndexPrimitive.Basevertex data. */
        memory[4] = 0;

        memory += 5;

        if (5 & 1)
        {
            gcmSETFILLER_NEW(
                reserve,
                memory
                );
        }

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }

    /* Success */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FastFlushDepthCompare(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory)
{
    static const gctINT32 xlateCompare[] =
    {
        /* gcvCOMPARE_INVALID */
        -1,
        /* gcvCOMPARE_NEVER */
        0x0,
        /* gcvCOMPARE_NOT_EQUAL */
        0x5,
        /* gcvCOMPARE_LESS */
        0x1,
        /* gcvCOMPARE_LESS_OR_EQUAL */
        0x3,
        /* gcvCOMPARE_EQUAL */
        0x2,
        /* gcvCOMPARE_GREATER */
        0x4,
        /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6,
        /* gcvCOMPARE_ALWAYS */
        0x7,
    };

    static const gctUINT32 xlateMode[] =
    {
        /* gcvDEPTH_NONE */
        0x0,
        /* gcvDEPTH_Z */
        0x1,
        /* gcvDEPTH_W */
        0x2,
    };

    gceSTATUS status;
    gcsDEPTH_INFO *depthInfo;

    gctBOOL flushNeeded = gcvFALSE;
    gctBOOL stallNeeded = gcvFALSE;

    gctBOOL ezEnabled = gcvFALSE;
    gctBOOL peDepth = gcvTRUE;

    gcmHEADER_ARG("Hardware=0x%x FastFlushInfo=0x%x", Hardware, FastFlushInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    {
        /* Define state buffer variables. */
        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

        depthInfo = &Hardware->depthStates;

        /* disable EZ, as there plenty of possible to disable it, I don't want to check */
        depthInfo->early = gcvFALSE;

        /* Test if configuration is dirty. */
        depthInfo->compare = FastFlushInfo->depthInfoCompare;

        if (Hardware->previousPEDepth != peDepth)
        {
            /* Need flush and stall when switching from PE Depth ON to PE Depth Off? */
            if (Hardware->previousPEDepth)
            {
                stallNeeded = gcvTRUE;
            }

            Hardware->previousPEDepth = peDepth;
        }

        /* If earlyZ or depth compare changed, and earlyZ is enabled, need to flush Zcache and stall */
        /* No need to initialize previous values bc no PE depth write before first draw. */
        if (Hardware->prevEarlyZ != ezEnabled ||
            Hardware->prevDepthCompare != depthInfo->compare)
        {
            if (ezEnabled)
            {
                flushNeeded = !Hardware->flushedDepth;
                stallNeeded = gcvTRUE;
            }

            Hardware->prevEarlyZ = ezEnabled;
            Hardware->prevDepthCompare = depthInfo->compare;
        }

        depthInfo->regDepthConfig &= ~(((((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) |
            ((((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) |
            ((((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) |
            ((((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
            );

        depthInfo->regDepthConfig |= (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (xlateMode[FastFlushInfo->depthMode]) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (ezEnabled) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) | (((gctUINT32) ((gctUINT32) (xlateCompare[FastFlushInfo->compare]) & ((gctUINT32) ((((1 ? 10:8) - (0 ? 10:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:8) - (0 ? 10:8) + 1))))))) << (0 ? 10:8))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (!peDepth) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))
            );

        /* Add the flush state. */
        if (flushNeeded || stallNeeded)
        {
            /* Mark depth cache as flushed. */
            Hardware->flushedDepth = gcvTRUE;
        }

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

        if (flushNeeded || stallNeeded)
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0500) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0500, depthInfo->regDepthConfig);

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0382) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0382, depthInfo->regRAControl );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }

    /* Success */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FastFlushAlpha(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    )
{
    gctBOOL floatPipe = gcvFALSE;

    static const gctINT32 xlateCompare[] =
    {
        /* gcvCOMPARE_INVALID */
        -1,
        /* gcvCOMPARE_NEVER */
        0x0,
        /* gcvCOMPARE_NOT_EQUAL */
        0x5,
        /* gcvCOMPARE_LESS */
        0x1,
        /* gcvCOMPARE_LESS_OR_EQUAL */
        0x3,
        /* gcvCOMPARE_EQUAL */
        0x2,
        /* gcvCOMPARE_GREATER */
        0x4,
        /* gcvCOMPARE_GREATER_OR_EQUAL */
        0x6,
        /* gcvCOMPARE_ALWAYS */
        0x7,
    };

    static const gctUINT32 xlateFuncSource[] =
    {
        /* gcvBLEND_ZERO */
        0x0,
        /* gcvBLEND_ONE */
        0x1,
        /* gcvBLEND_SOURCE_COLOR */
        0x2,
        /* gcvBLEND_INV_SOURCE_COLOR */
        0x3,
        /* gcvBLEND_SOURCE_ALPHA */
        0x4,
        /* gcvBLEND_INV_SOURCE_ALPHA */
        0x5,
        /* gcvBLEND_TARGET_COLOR */
        0x8,
        /* gcvBLEND_INV_TARGET_COLOR */
        0x9,
        /* gcvBLEND_TARGET_ALPHA */
        0x6,
        /* gcvBLEND_INV_TARGET_ALPHA */
        0x7,
        /* gcvBLEND_SOURCE_ALPHA_SATURATE */
        0xA,
        /* gcvBLEND_CONST_COLOR */
        0xD,
        /* gcvBLEND_INV_CONST_COLOR */
        0xE,
        /* gcvBLEND_CONST_ALPHA */
        0xB,
        /* gcvBLEND_INV_CONST_ALPHA */
        0xC,
    };

    static const gctUINT32 xlateFuncTarget[] =
    {
        /* gcvBLEND_ZERO */
        0x0,
        /* gcvBLEND_ONE */
        0x1,
        /* gcvBLEND_SOURCE_COLOR */
        0x2,
        /* gcvBLEND_INV_SOURCE_COLOR */
        0x3,
        /* gcvBLEND_SOURCE_ALPHA */
        0x4,
        /* gcvBLEND_INV_SOURCE_ALPHA */
        0x5,
        /* gcvBLEND_TARGET_COLOR */
        0x8,
        /* gcvBLEND_INV_TARGET_COLOR */
        0x9,
        /* gcvBLEND_TARGET_ALPHA */
        0x6,
        /* gcvBLEND_INV_TARGET_ALPHA */
        0x7,
        /* gcvBLEND_SOURCE_ALPHA_SATURATE */
        0xA,
        /* gcvBLEND_CONST_COLOR */
        0xD,
        /* gcvBLEND_INV_CONST_COLOR */
        0xE,
        /* gcvBLEND_CONST_ALPHA */
        0xB,
        /* gcvBLEND_INV_CONST_ALPHA */
        0xC,
    };

    static const gctUINT32 xlateMode[] =
    {
        /* gcvBLEND_ADD */
        0x0,
        /* gcvBLEND_SUBTRACT */
        0x1,
        /* gcvBLEND_REVERSE_SUBTRACT */
        0x2,
        /* gcvBLEND_MIN */
        0x3,
        /* gcvBLEND_MAX */
        0x4,
    };
    gceSTATUS status;

    gctBOOL canEnableAlphaBlend;

    gcmHEADER_ARG("Hardware=0x%x FastFlushInfo=0x%x", Hardware, FastFlushInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    {
        /* Define state buffer variables. */
        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

        /* Determine the size of the buffer to reserve. */
        floatPipe = Hardware->features[gcvFEATURE_HALF_FLOAT_PIPE];

        canEnableAlphaBlend = FastFlushInfo->blend;

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)3  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0508) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA_NEW_FAST(
            stateDelta, reserve, memory, gcvFALSE, 0x0508,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4))) | (((gctUINT32) ((gctUINT32) (xlateCompare[0]) & ((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
            );

        gcmSETSTATEDATA_NEW_FAST(
            stateDelta, reserve, memory, gcvFALSE, 0x0509,
            FastFlushInfo->color
            );

        gcmSETSTATEDATA_NEW_FAST(
            stateDelta, reserve, memory, gcvFALSE, 0x050A,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (canEnableAlphaBlend) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (canEnableAlphaBlend) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (xlateFuncSource[FastFlushInfo->srcFuncColor]) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (xlateFuncSource[FastFlushInfo->srcFuncAlpha]) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (xlateFuncTarget[FastFlushInfo->trgFuncColor]) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (xlateFuncTarget[FastFlushInfo->trgFuncAlpha]) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) ((gctUINT32) (xlateMode[FastFlushInfo->modeColor]) & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (xlateMode[FastFlushInfo->modeAlpha]) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)))
            );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        if (floatPipe)
        {
            gctUINT32 colorLow;
            gctUINT32 colorHeigh;
            gctUINT16 colorR;
            gctUINT16 colorG;
            gctUINT16 colorB;
            gctUINT16 colorA;

            colorR = gcoMATH_UInt8AsFloat16((((((gctUINT32) (FastFlushInfo->color)) >> (0 ? 23:16)) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1)))))) ));
            colorG = gcoMATH_UInt8AsFloat16((((((gctUINT32) (FastFlushInfo->color)) >> (0 ? 15:8)) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1)))))) ));
            colorB = gcoMATH_UInt8AsFloat16((((((gctUINT32) (FastFlushInfo->color)) >> (0 ? 7:0)) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1)))))) ));
            colorA = gcoMATH_UInt8AsFloat16((((((gctUINT32) (FastFlushInfo->color)) >> (0 ? 31:24)) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1)))))) ));
            colorLow = colorR | (colorG << 16);
            colorHeigh = colorB | (colorA << 16);

            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW_FAST(
                stateDelta, reserve, memory, gcvFALSE, 0x052C,
                colorLow
                );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );

            {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1  <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x052D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA_NEW_FAST(
                stateDelta, reserve, memory, gcvFALSE, 0x052D,
                colorHeigh
                );

            gcmENDSTATEBATCH_NEW(
                reserve, memory
                );
        }

        /* Color conversion control. */
        if (FastFlushInfo->blend
            && (FastFlushInfo->srcFuncColor == gcvBLEND_SOURCE_ALPHA)
            && (FastFlushInfo->trgFuncColor == gcvBLEND_INV_SOURCE_ALPHA))
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }
        else
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))));    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }

    /* Success */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FastProgramIndex(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory)
{
    gceSTATUS status;
    gcoBUFOBJ index = FastFlushInfo->bufObj;
    gctUINT32 indexAddress;
    gctUINT32 control;
    gctUINT32 indexEndian = 0x0;
    gctUINT32 indexFormat = 0x1;

    gcmHEADER_ARG("Hardware=0x%x FastFlushInfo=0x%x", Hardware, FastFlushInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    {
        /* Define state buffer variables. */
        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

        if (index)
        {
            gcoBUFOBJ_FastLock(index, &indexAddress, gcvNULL);
            indexAddress += (gctUINT32)(gctUINTPTR_T)(FastFlushInfo->indices);
        }
        else
        {
            indexAddress = (gctUINT32)(gctUINTPTR_T)(FastFlushInfo->indices);
        }

        if (Hardware->bigEndian && indexFormat != 0x0)
            indexEndian = 0x0;

        /* Compute control state value. */
        control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (indexFormat) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (indexEndian) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) ((gctUINT32) (Hardware->primitiveRestart) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0191) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA_NEW_FAST(
            stateDelta, reserve, memory, gcvFALSE, 0x0191,
            indexAddress;
        );

        gcmSETSTATEDATA_NEW_FAST(
            stateDelta, reserve, memory, gcvFALSE, 0x0192,
            control
            );

        gcmSETFILLER_NEW(
            reserve, memory
            );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* Set the state. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x019D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x019D, Hardware->restartElement);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
    }

    /* Success */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FastFlushUniforms(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory)
{
    gceSTATUS status;
    gctINT i = 0;

    gcmHEADER_ARG("Hardware=0x%x FastFlushInfo=0x%x", Hardware, FastFlushInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (FastFlushInfo->programValid)
    {
        for (i = 0; i < FastFlushInfo->userDefUniformCount; ++i)
        {
            gcsFAST_FLUSH_UNIFORM *uniform = &FastFlushInfo->uniforms[i];

            if(uniform->dirty)
            {
                gctINT stage;

                for (stage = 0; stage < 2; ++stage)
                {
                    gcUNIFORM halUniform = uniform->halUniform[stage];

                    if (halUniform && isUniformUsedInShader(halUniform))
                    {
                        gctUINT col;
                        gctUINT32 address = uniform->physicalAddress[stage] >> 2;
                        gctUINT8_PTR pArray = (gctUINT8_PTR)uniform->data;

                        /* Define state buffer variables. */
                        gcmDEFINESTATEBUFFER_NEW_FAST(reserve, memory);

                        /* Reserve space in the command buffer. */
                        gcmBEGINSTATEBUFFER_NEW_FAST(Hardware, reserve, memory, Memory);

                        if (Hardware->unifiedConst)
                        {
                            gctUINT32 shaderConfigData = Hardware->shaderStates.hints ?
                                                         Hardware->shaderStates.hints->shaderConfigData : 0;

                            shaderConfigData = ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) ((halUniform->shaderKind != gcSHADER_TYPE_VERTEX)) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

                            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW_FAST(stateDelta, reserve, memory, gcvFALSE, 0x0218, shaderConfigData);    gcmENDSTATEBATCH_NEW(reserve, memory);};
                        }

                        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)uniform->columns <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (uniform->columns) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

                        /* Walk all columns. */
                        for (col = 0; col < uniform->columns; ++col)
                        {
                            gctUINT8_PTR pData = pArray + (col << 2);

                            gctUINT data = *(gctUINT_PTR)pData;

                            /* Set the state value. */
                            gcmSETSTATEDATA_NEW_FAST(stateDelta,
                                reserve,
                                memory,
                                gcvFALSE,
                                address + col,
                                data);
                        }

                        if ((col & 1) == 0)
                        {
                            /* Align. */
                            gcmSETFILLER_NEW(reserve, memory);
                        }

                        /* End the state batch. */
                        gcmENDSTATEBATCH_NEW(reserve, memory);

                        /* Next row. */
                        address += 4;

                        pArray += uniform->arrayStride;

                        /* Validate the state buffer. */
                        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);
                    }
                }
            }
        }
    }

    /* Success */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

#define _gcmSETSTATEDATA_FAST(StateDelta, Memory, Address, Data) \
    do \
    { \
        gctUINT32 __temp_data32__ = Data; \
        \
        *Memory++ = __temp_data32__; \
        \
        gcmDUMPSTATEDATA(StateDelta, gcvFALSE, Address, __temp_data32__); \
        \
        Address += 1; \
    } \
    while (gcvFALSE)

gceSTATUS gcoHARDWARE_FastFlushStream(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory)
{
#define __GL_ONE_32 1

    gceSTATUS   status;
    gcsFAST_FLUSH_VERTEX_ARRAY* attArray;
    gctUINT vsInputMask = FastFlushInfo->vsInputMask;
    gctINT arrayIdx = -1;
    gctUINT bitMask;
    gctUINT streamCount = 0;

    gctUINT i, stream;
    gctUINT attributeCount;
    gctUINT stride;
    gctUINT divisor;
    gctUINT linkState;
    gctUINT linkCount;
    gctBOOL halti2Support;

    gctUINT32 lastPhysical;
    gctUINT32 fetchSize;
    gctUINT32 format;
    gctUINT32 endian;
    gctUINT32 link;
    gctUINT32 size;
    gctUINT32 normalize;
    gctUINT32 fetchBreak;
    gctUINT32 base;

    gctSIZE_T vertexCtrlStateCount, vertexCtrlReserveCount;
    gctSIZE_T shaderCtrlStateCount, shaderCtrlReserveCount;
    gctSIZE_T streamAddressStateCount, streamAddressReserveCount;
    gctSIZE_T streamStrideStateCount, streamStrideReserveCount;
    gctSIZE_T streamDivisorStateCount, streamDivisorReserveCount;
    gctUINT32_PTR vertexCtrl;
    gctUINT32_PTR shaderCtrl;
    gctUINT32_PTR streamAddress;
    gctUINT32_PTR streamStride;
    gctUINT32_PTR streamDivisor;

    gctUINT vertexCtrlState;
    gctUINT shaderCtrlState;
    gctUINT streamAddressState;
    gctUINT streamStrideState;
    gctUINT streamDivisorState;
    gctINT VertexInstanceIdLinkage = -1;
    gctUINT bytes;
    gctSIZE_T reserveSize;
    gctUINT32 physical;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=%p", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

        /* Determine hardware capabilities */
    halti2Support = Hardware->features[gcvFEATURE_HALTI2];

    /* Process streams */
    attributeCount = 0;

    /* compute vertex attribte count */
    while (vsInputMask)
    {
        bitMask = (__GL_ONE_32 << ++arrayIdx);
        if (!(vsInputMask & bitMask))
        {
            continue;
        }
        streamCount++;
        vsInputMask &= ~bitMask;
    }

    /*
    if (streamCount == 0)
        return GL_FALSE;
    */
    gcmONERROR(!streamCount);

    attributeCount = streamCount;

    /***************************************************************************
    ** Determine the counts and reserve size.
    */

    /* State counts. */
    vertexCtrlStateCount = attributeCount;

    if (VertexInstanceIdLinkage != -1)
    {
        shaderCtrlStateCount = gcmALIGN((attributeCount+1), 4) >> 2;
    }
    else
    {
        shaderCtrlStateCount = gcmALIGN(attributeCount, 4) >> 2;
    }

    /* Reserve counts. */
    vertexCtrlReserveCount = 1 + (vertexCtrlStateCount | 1);
    shaderCtrlReserveCount = 1 + (shaderCtrlStateCount | 1);

    /* Set initial state addresses. */
    vertexCtrlState = 0x0180;
    shaderCtrlState = 0x0208;

    reserveSize
        = (vertexCtrlReserveCount + shaderCtrlReserveCount)
        *  gcmSIZEOF(gctUINT32);

    /* State counts. */
    streamAddressStateCount = Hardware->mixedStreams
        ? streamCount
        : Hardware->config->streamCount;

    streamStrideStateCount  = streamCount;

    streamAddressReserveCount = 1 + (streamAddressStateCount | 1);
    streamStrideReserveCount  = 1 + (streamStrideStateCount  | 1);

    if (halti2Support)
    {
        streamDivisorStateCount = streamCount;
        streamDivisorReserveCount = 1 + (streamDivisorStateCount  | 1);

        /* Set initial state addresses. */
        streamAddressState = 0x5180;
        streamStrideState  = 0x5190;
        streamDivisorState = 0x51A0;
    }
    else if (Hardware->config->streamCount > 1)
    {
        streamDivisorStateCount = 0;
        streamDivisorReserveCount = 0;

        /* Set initial state addresses. */
        streamAddressState = 0x01A0;
        streamStrideState  = 0x01A8;
        streamDivisorState  = 0;
    }
    else
    {
        streamDivisorStateCount = 0;
        streamDivisorReserveCount = 0;

        /* Set initial state addresses. */
        streamAddressState = 0x0193;
        streamStrideState  = 0x0194;
        streamDivisorState  = 0;
    }

    /* Add stream states. */
    reserveSize
        += (streamAddressReserveCount + streamStrideReserveCount + streamDivisorReserveCount)
        *  gcmSIZEOF(gctUINT32);

    /***************************************************************************
    ** Reserve command buffer state and init state commands.
    */
    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    /* Update the number of the elements. */
    stateDelta->elementCount = attributeCount;

    /* Determine buffer pointers. */
    vertexCtrl    = memory;
    shaderCtrl    = vertexCtrl + vertexCtrlReserveCount;
    streamAddress = shaderCtrl + shaderCtrlReserveCount;
    streamStride  = streamAddress + streamAddressReserveCount;
    streamDivisor = streamStride + streamStrideReserveCount;

    memory = (gctUINT32_PTR) ((gctUINT8_PTR) memory + reserveSize);

    /* Init load state commands. */
    *vertexCtrl++
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (vertexCtrlStateCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (vertexCtrlState) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *shaderCtrl++
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (shaderCtrlStateCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (shaderCtrlState) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *streamAddress++
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (streamAddressStateCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (streamAddressState) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *streamStride++
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (streamStrideStateCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (streamStrideState) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    if (halti2Support)
    {
        *streamDivisor++
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (streamDivisorStateCount) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (streamDivisorState) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
    }

    lastPhysical = 0;
    linkCount = 0;
    linkState = 0;

    vsInputMask = FastFlushInfo->vsInputMask;

    stream = 0;
    arrayIdx = -1;

    /* Program stream/attrib for every mask bit */
    while (vsInputMask)
    {
        bitMask = (__GL_ONE_32 << ++arrayIdx);
        if (!(vsInputMask & bitMask))
        {
            continue;
        }
        vsInputMask &= ~bitMask;

        /* Check whether VS required attribute was enabled by apps */
        if (FastFlushInfo->vertexArrayEnable & (__GL_ONE_32 << arrayIdx))
        {
            attArray = &FastFlushInfo->attribute[arrayIdx];
            /* Get the internal format */
            format = gcvVERTEX_FLOAT;

            gcoBUFOBJ_FastLock(FastFlushInfo->boundObjInfo[arrayIdx], &physical,gcvNULL);

            stride = attArray->stride;
            divisor = attArray->divisor;
            base    = (gctUINT32)(gctUINTPTR_T)attArray->pointer;

            lastPhysical = physical + base;

            /* Store the stream address. */
            _gcmSETSTATEDATA_FAST(stateDelta, streamAddress, streamAddressState, lastPhysical);

            if (halti2Support)
            {
                /* Store the stream stride. */
                _gcmSETSTATEDATA_FAST(
                    stateDelta, streamStride, streamStrideState,
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:0) - (0 ? 11:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:0) - (0 ? 11:0) + 1))))))) << (0 ? 11:0))) | (((gctUINT32) ((gctUINT32) (stride) & ((gctUINT32) ((((1 ? 11:0) - (0 ? 11:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:0) - (0 ? 11:0) + 1))))))) << (0 ? 11:0)))
                    );

                /* Store the stream divisor. */
                _gcmSETSTATEDATA_FAST(
                    stateDelta, streamDivisor, streamDivisorState, divisor
                    );
            }
            else
            {
                /* Store the stream stride. */
                _gcmSETSTATEDATA_FAST(
                    stateDelta, streamStride, streamStrideState,
                    (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:0) - (0 ? 8:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:0) - (0 ? 8:0) + 1))))))) << (0 ? 8:0))) | (((gctUINT32) ((gctUINT32) (stride) & ((gctUINT32) ((((1 ? 8:0) - (0 ? 8:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:0) - (0 ? 8:0) + 1))))))) << (0 ? 8:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (divisor) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))))
                    );
            }

            fetchSize = 0;

            /* every stream have one attribute */

            endian = Hardware->bigEndian
                ? 0x2
                : 0x0;
            bytes = attArray->size * 4;

            /* Get size. */
            size = attArray->size;
            link = FastFlushInfo->attribLinkage[arrayIdx];

            /* Get normalized flag. */
            normalize = 0x0;

            /* Get vertex offset and size. */
            fetchSize  = bytes;
            fetchBreak = 1;

            /* Store the current vertex element control value. */
            _gcmSETSTATEDATA_FAST(
                stateDelta, vertexCtrl, vertexCtrlState,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (size) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14))) | (((gctUINT32) ((gctUINT32) (normalize) & ((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (endian) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (stream) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (fetchSize) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (fetchBreak) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

            /* Set vertex shader input linkage. */
            switch (linkCount & 3)
            {
            case 0:
                linkState = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (link ) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)));
                break;
            case 1:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (link ) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8)));
                break;
            case 2:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (link ) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16)));
                break;
            case 3:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (link ) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)));
                /* Store the current shader input control value. */
                _gcmSETSTATEDATA_FAST(
                    stateDelta, shaderCtrl, shaderCtrlState, linkState
                    );
                break;
            default:
                break;
            }

            /* Next vertex shader linkage. */
            ++linkCount;

            if (fetchBreak)
            {
                /* Reset fetch size on a break. */
                fetchSize = 0;
            }
        }

        stream++;
    }

    /* Check if the IP requires all streams to be programmed. */
    if (!Hardware->mixedStreams)
    {
        for (i = streamCount; i < streamAddressStateCount; ++i)
        {
            /* Replicate the last physical address for unknown stream
            ** addresses. */
            _gcmSETSTATEDATA_FAST(
                stateDelta, streamAddress, streamAddressState,
                lastPhysical
                );
        }
    }

    /* See if there are any attributes left to program in the vertex shader
    ** shader input registers. And also check if we need to add vertex instance linkage
    */
    if (((linkCount & 3) != 0) || (VertexInstanceIdLinkage != -1))
    {
        /* Do we need to add vertex instance linkage? */
        if (VertexInstanceIdLinkage != -1)
        {
            /* Set vertex shader input linkage. */
            switch (linkCount & 3)
            {
            case 0:
                linkState = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (VertexInstanceIdLinkage ) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)));
                break;
            case 1:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (VertexInstanceIdLinkage ) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8)));
                break;
            case 2:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (VertexInstanceIdLinkage ) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16)));
                break;
            case 3:
                linkState = ((((gctUINT32) (linkState)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (VertexInstanceIdLinkage ) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)));
                break;
            default:
                break;
            }
        }

        /* Program attributes */
        _gcmSETSTATEDATA_FAST(
            stateDelta, shaderCtrl, shaderCtrlState, linkState
            );
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_ProgramIndex(IN gcoHARDWARE Hardware, INOUT gctPOINTER *Memory)
{
    gceSTATUS   status;
    gctUINT32   control;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=%p", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->indexDirty)
    {
        /* Compute control state value. */
        control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (Hardware->indexFormat) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (Hardware->indexEndian) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) ((gctUINT32) (Hardware->primitiveRestart) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0191) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

        if (Hardware->indexHeadDirty)
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0191,
                Hardware->indexHeadAddress;
                );

            /* Mark index as updated. */
            Hardware->indexHeadDirty = gcvFALSE;
        }
        else if (Hardware->indexTailDirty)
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0191,
                Hardware->indexTailAddress;
                );

            Hardware->indexTailDirty = gcvFALSE;
        }
        else
        {
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0191,
                Hardware->indexHeadAddress;
                );
        }
            gcmSETSTATEDATA_NEW(
                stateDelta, reserve, memory, gcvFALSE, 0x0192,
                control
                );

            gcmSETFILLER_NEW(
                reserve, memory
                );

        gcmENDSTATEBATCH_NEW(
            reserve, memory
            );

        /* Set the state. */
        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x019D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x019D, Hardware->restartElement);    gcmENDSTATEBATCH_NEW(reserve, memory);};

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

        Hardware->indexDirty = Hardware->indexHeadDirty | Hardware->indexTailDirty;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdSTREAM_OUT_BUFFER


gceSTATUS
gcoHARDWARE_QueryStreamOutIndex(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount,
    OUT gctBOOL_PTR Found
    )
{
    gcoSTREAM_OUT_INDEX * index = gcvNULL;
    gctBOOL found = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x IndexAddress=0x%x IndexOffset=0x%x IndexCount=%u Found=0x%x",
                  Hardware, IndexAddress, IndexOffset, IndexCount, Found);

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Query index address 0x%x offset 0x%x count %d (%s)",
                  IndexAddress,
                  IndexOffset,
                  IndexCount,
                  __FUNCTION__);

    for(index = Hardware->streamOutIndexCache.head; index != gcvNULL; index = index->next)
    {
        if ((index->originalAddress == IndexAddress)
            && (index->originalOffset == IndexOffset)
            && (index->originalCount == IndexCount)
            && (index->usedOnce == gcvFALSE))
        {
            /* We found it */
            found = gcvTRUE;
            break;
        }
    }

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - %s index address 0x%x offset 0x%x count %d (%s)",
                  (found ? "Found" : "DID NOT find"),
                  IndexAddress,
                  IndexOffset,
                  IndexCount,
                  __FUNCTION__);

    /* Set output */
    *Found = found;

    gcmFOOTER_ARG("*Found=", *Found);
    return gcvSTATUS_OK;
}

gceSTATUS gcoHARDWARE_AddStreamOutIndex (IN gcoHARDWARE Hardware, IN gctUINT32 Bytes)
{
    gceSTATUS   status;
    gctUINT i;
    gcoSTREAM_OUT_INDEX * index = gcvNULL;
#if gcdMULTI_GPU
    gctUINT coreCount = Hardware->config->gpuCoreCount;
#else
    gctUINT coreCount = 1;
#endif

    gcmHEADER_ARG("Hardware=0x%08x Bytes=%u", Hardware, Bytes);

    /* Create a new entry */
    gcmONERROR(gcoOS_Allocate(gcvNULL, sizeof(gcoSTREAM_OUT_INDEX), (gctPOINTER) &index));
    gcoOS_MemFill(index, 0, sizeof(gcoSTREAM_OUT_INDEX));

    /* Is the queue initialized? */
    if ((Hardware->streamOutIndexCache.head == gcvNULL)
        || (Hardware->streamOutIndexCache.tail == gcvNULL))
    {
        /* Head points to new index */
        Hardware->streamOutIndexCache.head = index;
        Hardware->streamOutIndexCache.tail = Hardware->streamOutIndexCache.head;
    }
    else
    {
        /* Bind index to the queue */
        Hardware->streamOutIndexCache.tail->next = index;

        /* Advance tail */
        Hardware->streamOutIndexCache.tail = index;
    }

    /* Advance count */

    Hardware->streamOutIndexCache.count++;

    /* Store original index address */
    index->originalAddress = Hardware->streamOutOriginalIndexAddess;
    index->originalCount = Hardware->streamOutOriginalIndexCount;
    index->originalOffset= Hardware->streamOutOriginalIndexOffset;
    index->usedOnce = gcvFALSE;

    /* For all cores available */
    for (i = 0; i < coreCount; i++)
    {
        /* Allocate video memory.
         TODO: Since all primitive types are changed to triangles
         we need to calculate proper size for primitive type
         mismatches.
         */
        gcmONERROR(gcsSURF_NODE_Construct(
            &index->streamOutIndexNode[i],
            Bytes + 64,
            64,
            gcvSURF_INDEX,
            gcvPOOL_DEFAULT
            ));

        /* Lock the index buffer. */
        gcmONERROR(gcoHARDWARE_Lock(&index->streamOutIndexNode[i],
                                    &index->streamOutIndexAddress[i],
                                    gcvNULL));

        gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                      gcvZONE_STREAM_OUT,
                      "Stream out - Allocate memory 0x%x bytes %d (%s)",
                      index->streamOutIndexAddress[i],
                      (Bytes + 64),
                      __FUNCTION__);

    }

    gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                  gcvZONE_STREAM_OUT,
                  "Stream out - Cache[%d] entry 0x%x index address 0x%x offset 0x%x count %d (%s)",
                  Hardware->streamOutIndexCache.count,
                  index,
                  Hardware->streamOutOriginalIndexAddess,
                  Hardware->streamOutOriginalIndexOffset,
                  Hardware->streamOutOriginalIndexCount,
                  __FUNCTION__);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS gcoHARDWARE_GetStreamOutIndex (IN gcoHARDWARE Hardware, OUT gcoSTREAM_OUT_INDEX ** Index)
{
    gceSTATUS   status;
    gcoSTREAM_OUT_INDEX * index = gcvNULL;

    gcmHEADER_ARG("Hardware=0x%08x Index=%p", Hardware, Index);

    if (Index == gcvNULL)
    {
        /* If not bail out */
        gcmONERROR(gcvSTATUS_INVALID_REQUEST);
    }
    else if ((Hardware->streamOutIndexCache.head == gcvNULL)
             || (Hardware->streamOutIndexCache.tail == gcvNULL))
    {
        /* If not bail out */
        gcmONERROR(gcvSTATUS_INVALID_REQUEST);
    }
    else if (Hardware->streamOutIndexCache.count == 0)
    {
        /* If not bail out */
        gcmONERROR(gcvSTATUS_INVALID_REQUEST);
    }
    else
    {
        for(index = Hardware->streamOutIndexCache.head; index != gcvNULL; index = index->next)
        {
            if ((index->originalAddress == Hardware->streamOutOriginalIndexAddess)
                 && (index->originalOffset == Hardware->streamOutOriginalIndexOffset)
                 && (index->originalCount == Hardware->streamOutOriginalIndexCount)
                    && (index->usedOnce == gcvFALSE))
            {
                index->usedOnce = gcvTRUE;
                break;
            }
        }
    }

    if (index != gcvNULL)
    {
        /* Advance count */
        Hardware->streamOutIndexCache.count--;

        /* Set output */
        *Index = index;

        gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                      gcvZONE_STREAM_OUT,
                      "Stream out - Decache[%d] entry 0x%x index address 0x%x offset 0x%x count %d (%s)",
                      Hardware->streamOutIndexCache.count,
                      index,
                      Hardware->streamOutOriginalIndexAddess,
                      Hardware->streamOutOriginalIndexOffset,
                      Hardware->streamOutOriginalIndexCount,
                      __FUNCTION__);
    }
    else
    {
        /* Set output */
        *Index = gcvNULL;
    }

    /* Success. */
    gcmFOOTER_ARG("*Index=%p", *Index);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FreeStreamOutIndex (IN gcoHARDWARE Hardware)
{
    gceSTATUS   status;
    gcoSTREAM_OUT_INDEX * index = gcvNULL;
    gcoSTREAM_OUT_INDEX * prev = gcvNULL;
    gctUINT i;
#if gcdMULTI_GPU
    gctUINT coreCount = Hardware->config->gpuCoreCount;
#else
    gctUINT coreCount = 1;
#endif

    gcmHEADER_ARG("Hardware=0x%08x Index=%p", Hardware);

    if ((Hardware->streamOutIndexCache.head == gcvNULL)
             || (Hardware->streamOutIndexCache.tail == gcvNULL))
    {
        /* If not bail out */
        gcmONERROR(gcvSTATUS_INVALID_REQUEST);
    }
    else
    {
        prev = gcvNULL;
        for(index = Hardware->streamOutIndexCache.head; index != gcvNULL; index = index->next)
        {
            if ((index->originalAddress == Hardware->streamOutOriginalIndexAddess)
                && (index->originalOffset == Hardware->streamOutOriginalIndexOffset)
                && (index->originalCount == Hardware->streamOutOriginalIndexCount)
                && (index->usedOnce == gcvTRUE))
            {
                break;
            }
            /* set as previous */
            prev = index;
        }
    }

    if (index != gcvNULL)
    {
        if (Hardware->streamOutIndexCache.count == 0)
        {
            Hardware->streamOutIndexCache.head = gcvNULL;
            Hardware->streamOutIndexCache.tail = gcvNULL;
        }

        /* Remove index from list */
        if (prev != gcvNULL)
        {
            prev->next = index->next;
        }
        else
        {
            Hardware->streamOutIndexCache.head = index->next;
        }

        /* For all cores available */
        for (i = 0; i < coreCount; i++)
        {
            /* Free memory if allocated before */
            if (index->streamOutIndexAddress[i] != 0)
            {
                gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                              gcvZONE_STREAM_OUT,
                              "Stream out - Free memory 0x%x (%s)",
                              index->streamOutIndexAddress[i],
                              __FUNCTION__);

                gcmONERROR(gcoHARDWARE_Unlock(&index->streamOutIndexNode[i], gcvSURF_INDEX));
                gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&index->streamOutIndexNode[i]));

                index->streamOutIndexNode[i].pool  = gcvPOOL_UNKNOWN;
                index->streamOutIndexNode[i].valid = gcvFALSE;
                index->streamOutIndexAddress[i] = 0;
            }
        }

        /* Free  */
        gcoOS_Free(gcvNULL, index);
        index = gcvNULL;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_ProgramIndexStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 CoreIndex,
    IN gcoSTREAM_OUT_INDEX * Index,
    INOUT gctPOINTER *Memory)
{
    gceSTATUS   status;
    gctUINT32   control;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=%p CoreIndex=%u Index=%p", Hardware, CoreIndex, Index);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmVERIFY_ARGUMENT(Index != gcvNULL);

    /* Compute control state value. */
    control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (Hardware->indexFormat) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (Hardware->indexEndian) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) ((gctUINT32) (Hardware->primitiveRestart) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

#if gcdMULTI_GPU
    /* Switch to proper core */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | 1 << CoreIndex; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", 1 << CoreIndex); };
#endif

    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)2 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0191) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};

    /* Program indices */
    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0191,
                    Index->streamOutIndexAddress[CoreIndex]);

    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x0192,
                    control);

    gcmSETFILLER_NEW(reserve, memory);

    gcmENDSTATEBATCH_NEW(reserve, memory);

    /* Set the state. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x019D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETSTATEDATA_NEW(stateDelta, reserve, memory, gcvFALSE, 0x019D, Hardware->restartElement);    gcmENDSTATEBATCH_NEW(reserve, memory);};

#if gcdMULTI_GPU
    /* Switch back to all */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);


    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#endif


