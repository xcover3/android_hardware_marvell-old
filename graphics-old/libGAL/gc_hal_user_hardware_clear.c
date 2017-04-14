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

/******************************************************************************\
****************************** gcoHARDWARE API Code *****************************
\******************************************************************************/

static gceSTATUS  _GetClearDestinationFormat(gcoHARDWARE hardware, gcsSURF_FORMAT_INFO *fmtInfo, gctUINT32 *HWvalue, gcsPOINT *rectSize)
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmASSERT(fmtInfo);
    gcmASSERT(rectSize);
    gcmASSERT(HWvalue);

    switch (fmtInfo->bitsPerPixel/fmtInfo->layers)
    {
        case 16:
            *HWvalue = 0x01;
            break;
        case 32:
            *HWvalue = 0x06;
            break;
        case 64:
            gcmASSERT(hardware->features[gcvFEATURE_64BPP_HW_CLEAR_SUPPORT]);
            *HWvalue = 0x15;
            break;
        default:
            status = gcvSTATUS_NOT_SUPPORTED;
            break;

    }

    return status;
}


static gceSTATUS
_ClearHardware(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gctINT32 TileWidth,
    IN gctINT32 TileHeight
    )
{
    gceSTATUS status;
    gctUINT32 leftMask, topMask;
    gctUINT32 stride;
    gcsPOINT rectSize;
    gctUINT32 dstFormat;
    gctUINT32 srcStride = 0;

    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%08x Left=%d Top=%d "
                  "Right=%d Bottom=%d ClearValue=0x%08x "
                  "ClearMask=0x%02x TileWidth=%u TileHeight=%u",
                  Hardware, Surface, Left, Top, Right, Bottom,
                  ClearValue, ClearMask, TileWidth, TileHeight);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /*
    ** Bail out for 64bpp surface if RS can't support it.
    */
    if (!Hardware->features[gcvFEATURE_64BPP_HW_CLEAR_SUPPORT] &&
        ((Surface->bitsPerPixel/Surface->formatInfo.layers) > 32))
    {
        status = gcvSTATUS_NOT_SUPPORTED;
        goto OnError;
    }

    /* All sides must be tile aligned. */
    leftMask = TileWidth - 1;
    topMask  = TileHeight - 1;

    /* Increase clear region to aligned region. */
    if (Right == Surface->rect.right)
    {
        Right = Surface->alignedWidth;
    }

    if (Bottom == Surface->rect.bottom)
    {
        Bottom = Surface->alignedHeight;
    }

    rectSize.x = Right  - Left;
    rectSize.y = Bottom - Top;

    /* For resolve clear, we need to be 4x1 tile aligned. */
    if (((Left       & leftMask)   == 0)
    &&  ((Top        & topMask)    == 0)
    &&  ((rectSize.x & (Hardware->resolveAlignmentX - 1)) == 0)
    &&  ((rectSize.y & (Hardware->resolveAlignmentY - 1)) == 0)
    )
    {
        gctUINT32 config, control;
        gctUINT32 dither[2] = { ~0U, ~0U };
        gctUINT32 offset, address, bitsPerPixel;
        gctBOOL  halti2Support = Hardware->features[gcvFEATURE_HALTI2];
        gctUINT32 layers = Surface->formatInfo.layers;
        gctBOOL multiPipe = (Surface->tiling & gcvTILING_SPLIT_BUFFER) || Hardware->multiPipeResolve;

        /* Determine Fast MSAA mode. */
        if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
            Hardware->features[gcvFEATURE_SMALL_MSAA])
        {
            if ((Surface->samples.x > 1)
             && (Surface->samples.y > 1))
            {
                /* Currently we will not clear part of msaa surface with RS-clear,
                ** as it will disable FC, but we need msaa FC always on.
                */
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }
            else
            {
                srcStride = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
            }
        }

        /*
        ** Adjust MSAA setting for RS clear. Here Surface must be non-msaa.
        */
        gcmONERROR(gcoHARDWARE_AdjustCacheMode(Hardware,Surface));

        /* Set up the starting address of clear rectangle. */
        gcmONERROR(
            gcoHARDWARE_ConvertFormat(Surface->format,
                                      &bitsPerPixel,
                                      gcvNULL));

        /* Compute the origin offset. */
        gcmONERROR(
            gcoHARDWARE_ComputeOffset(Hardware,
                                      Left, Top,
                                      Surface->stride,
                                      (bitsPerPixel / 8)/layers,
                                      Surface->tiling, &offset));

        /* Determine the starting address. */
        address = Surface->node.physical + offset + Surface->offset;

        gcmONERROR(_GetClearDestinationFormat(Hardware, &Surface->formatInfo, &dstFormat, &rectSize));

        /* Build AQRsConfig register. src format is useless for clear
        ** Only dword size should be correct for destination surface
        */
        config = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) ((gctUINT32) (dstFormat) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) ((gctUINT32) (dstFormat) & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));

        if (halti2Support)
        {
            /* Build AQRsClearControl register. */
            control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((ClearMask | (ClearMask << 4))) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
        }
        else
        {
            /* Build AQRsClearControl register. */
            control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (ClearMask) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
        }

        /* Determine the stride. */
        stride = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:0) - (0 ? 19:0) + 1))))))) << (0 ? 19:0))) | (((gctUINT32) ((gctUINT32) ((Surface->stride << 2)) & ((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:0) - (0 ? 19:0) + 1))))))) << (0 ? 19:0)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (((Surface->tiling & gcvSUPERTILED) > 0)) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)))
               | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (((Surface->tiling & gcvTILING_SPLIT_BUFFER) > 0)) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));

        /* Switch to 3D pipe. */
        gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

        /* Flush cache. */
        gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

        reserveSize = /* Config */
                      gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                      /* Dither table */
                      gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8) +
                      /* Dest Stride */
                      gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                       /* Clear control state */
                      gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                      /* extra RS config state */
                      gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

        /* cache mode programming */
        if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
            Hardware->features[gcvFEATURE_SMALL_MSAA])
        {
            reserveSize +=
                    /* Src Stride */
                    gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }


        if (halti2Support)
        {
            reserveSize += /* 64bit clear value */
                           gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
        }
        else
        {
            reserveSize += /* 32bit clear value */
                           gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }

        if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
        {
            reserveSize += /* dual dest address */
                           gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
        }
        else
        {
            reserveSize += /* single dest address */
                           gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                 reserveSize += /* new single dest address */
                               gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
            }
        }

        gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0581, config);     gcmENDSTATEBATCH(reserve, memory);};

        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058C, 2);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058C, dither[0]);

            gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058C+1, dither[1]);

            gcmSETFILLER(reserve, memory);

        gcmENDSTATEBATCH(reserve, memory);

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0585, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0585) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0585, stride);     gcmENDSTATEBATCH(reserve, memory);};

        /* cache mode programming */
        if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
            Hardware->features[gcvFEATURE_SMALL_MSAA])
        {
           {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0583, srcStride);     gcmENDSTATEBATCH(reserve, memory);};
        }

        if (halti2Support)
        {
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0590, 2);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0590) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0590, ClearValue);

                gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0590+1, ClearValueUpper);

                gcmSETFILLER(reserve, memory);

            gcmENDSTATEBATCH(reserve, memory);
        }
        else
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0590, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0590) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0590, ClearValue);     gcmENDSTATEBATCH(reserve, memory);};
        }

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058F, control);     gcmENDSTATEBATCH(reserve, memory);};

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05A8, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (!Hardware->multiPipeResolve) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))));     gcmENDSTATEBATCH(reserve, memory);};


        if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
        {
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE,
                                0x05B8,
                                address);

                gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE,
                                0x05B8+1,
                                address + Surface->bottomBufferOffset);

                gcmSETFILLER(reserve, memory);

            gcmENDSTATEBATCH(reserve, memory);
        }
        else
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0584, address);     gcmENDSTATEBATCH(reserve, memory);};

            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B8, address);     gcmENDSTATEBATCH(reserve, memory);};
            }
        }

        gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

        gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware,
                                              rectSize,
                                              multiPipe,
                                              gcvMSAA_DOWNSAMPLE_AVERAGE));


    }
    else
    {
        /* Removed 2D clear as it does not work for tiled buffers. */
        status = gcvSTATUS_NOT_ALIGNED;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_ClearSoftware(
    IN gctUINT8_PTR Address1,
    IN gctUINT8_PTR Address2,
    IN gctUINT32 Stride,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom,
    IN gctUINT32 channelMask[4],
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gctUINT32 bitsPerPixel,
    IN gctBOOL FastMSAA,
    IN gceTILING Tiling,
    IN gceSURF_TYPE Type,
    IN gctUINT8 BitMask
    )
{
    gctINT32 x, y;
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 offset, tLeft;
    gctBOOL MultiPipe = gcvFALSE;

    gcmASSERT(Top < Bottom);
    gcmASSERT(Left < Right);

    /* Multitiled destination? */
    MultiPipe = (Tiling & gcvTILING_SPLIT_BUFFER);

    tLeft = (bitsPerPixel == 16) ? gcmALIGN(Left - 1, 2) : Left;

    for (y = Top;  y < Bottom; y++)
    {
        for (x = tLeft; x < Right; x++)
        {
            gctUINT8_PTR address = gcvNULL;

            if (MultiPipe)
            {
                /* Get base source address. */
                address = (((x >> 3) ^ (y >> 2)) & 0x01) ? Address2 : Address1;
            }
            else
            {
                address = Address1;
            }

            /* Compute the origin offset. */
            gcmONERROR(
                gcoHARDWARE_ComputeOffset(gcvNULL,
                                          x, y,
                                          Stride,
                                          bitsPerPixel / 8,
                                          Tiling, &offset));

            address += offset;

            /* Draw only if x,y within clear rectangle. */
            if (bitsPerPixel == 64)
            {
                gctUINT16_PTR addr16 = (gctUINT16_PTR)address;
                if (ClearMask & 0x1)
                {
                    /* Clear byte 0. */
                    addr16[0] = (gctUINT16) ClearValue;
                }

                if (ClearMask & 0x2)
                {
                    /* Clear byte 1. */
                    addr16[1] = (gctUINT16) (ClearValue >> 16);
                }

                if (ClearMask & 0x4)
                {
                    /* Clear byte 2. */
                    addr16[2] = (gctUINT16) (ClearValueUpper);
                }

                if (ClearMask & 0x8)
                {
                    /* Clear byte 3. */
                    addr16[3] = (gctUINT16) (ClearValueUpper >> 16);
                }
            }

            else if (bitsPerPixel == 32)
            {
                switch (ClearMask)
                {
                case 0x1:
                    /* Common: Clear stencil only. */
                    if (Type == gcvSURF_DEPTH)
                    {
                        (*address) = (*address) & (~BitMask);
                        (*address) = (*address) | (ClearValue & BitMask);
                    }
                    else
                        * address = (gctUINT8) ClearValue;
                    break;

                case 0xE:
                    /* Common: Clear depth only. */
                                       address[1] = (gctUINT8)  (ClearValue >> 8);
                    * (gctUINT16_PTR) &address[2] = (gctUINT16) (ClearValue >> 16);
                    break;

                case 0xF:
                    /* Common: Clear everything. */
                    if (Type == gcvSURF_DEPTH)
                    {
                        gctUINT32 mask = 0xffffff00 | BitMask;
                        (*(gctUINT32_PTR) address) = (*(gctUINT32_PTR) address) & (~mask);
                        (*(gctUINT32_PTR) address) = (*(gctUINT32_PTR) address) | (ClearValue & mask);
                    }
                    else
                        * (gctUINT32_PTR) address = ClearValue;
                    break;

                default:
                    if (ClearMask & 0x1)
                    {
                        /* Clear channel 0. */
                        *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[0])
                                                 | (ClearValue &  channelMask[0]);
                    }

                    if (ClearMask & 0x2)
                    {
                        /* Clear channel 1. */
                        *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[1])
                                                 | (ClearValue &  channelMask[1]);
                    }

                    if (ClearMask & 0x4)
                    {
                        /* Clear channel 2. */
                        *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[2])
                                                 | (ClearValue &  channelMask[2]);
                    }

                    if (ClearMask & 0x8)
                    {
                        /* Clear channel 3. */
                        *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[3])
                                                 | (ClearValue &  channelMask[3]);
                    }
                }
            }

            else if (bitsPerPixel == 16)
            {
                if ((x + 1) == Right)
                {
                    /* Dont write on Right pixel. */
                    channelMask[0] = channelMask[0] & 0x0000FFFF;
                    channelMask[1] = channelMask[1] & 0x0000FFFF;
                    channelMask[2] = channelMask[2] & 0x0000FFFF;
                    channelMask[3] = channelMask[3] & 0x0000FFFF;
                }

                if ((x + 1) == Left)
                {
                    /* Dont write on Left pixel. */
                    channelMask[0] = channelMask[0] & 0xFFFF0000;
                    channelMask[1] = channelMask[1] & 0xFFFF0000;
                    channelMask[2] = channelMask[2] & 0xFFFF0000;
                    channelMask[3] = channelMask[3] & 0xFFFF0000;
                }

                if (ClearMask & 0x1)
                {
                    /* Clear channel 0. */
                    *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[0])
                                             | (ClearValue &  channelMask[0]);
                }

                if (ClearMask & 0x2)
                {
                    /* Clear channel 1. */
                    *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[1])
                                             | (ClearValue &  channelMask[1]);
                }

                if (ClearMask & 0x4)
                {
                    /* Clear channel 2. */
                    *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[2])
                                             | (ClearValue &  channelMask[2]);
                }

                if (ClearMask & 0x8)
                {
                    /* Clear channel 3. */
                    *(gctUINT32_PTR) address = (*(gctUINT32_PTR) address & ~channelMask[3])
                                             | (ClearValue &  channelMask[3]);
                }

                if ((x + 1) == Left)
                {
                    /* Restore channel mask. */
                    channelMask[0] = channelMask[0] | (channelMask[0] >> 16);
                    channelMask[1] = channelMask[1] | (channelMask[1] >> 16);
                    channelMask[2] = channelMask[2] | (channelMask[2] >> 16);
                    channelMask[3] = channelMask[3] | (channelMask[3] >> 16);
                }

                if ((x + 1) == Right)
                {
                    /* Restore channel mask. */
                    channelMask[0] = channelMask[0] | (channelMask[0] << 16);
                    channelMask[1] = channelMask[1] | (channelMask[1] << 16);
                    channelMask[2] = channelMask[2] | (channelMask[2] << 16);
                    channelMask[3] = channelMask[3] | (channelMask[3] << 16);
                }

                /* 16bpp pixels clear 1 extra pixel at a time. */
                x++;
            }

            else if (bitsPerPixel == 8)
            {
                (*address) = (gctUINT8) (((*address) & (~BitMask)) | (ClearValue & BitMask));
            }
        }
    }

OnError:
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ClearRect
**
**  Append a command buffer with a CLEAR command.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Base address of surface to clear.
**
**      gctPOINTER Memory
**          Base address of surface to clear.
**
**      gctUINT32 Stride
**          Stride of surface.
**
**      gctINT32 Left
**          Left coordinate of rectangle to clear.
**
**      gctINT32 Top
**          Top coordinate of rectangle to clear.
**
**      gctINT32 Right
**          Right coordinate of rectangle to clear.
**
**      gctINT32 Bottom
**          Bottom coordinate of rectangle to clear.
**
**      gceSURF_FORMAT Format
**          Format of surface to clear.
**
**      gctUINT32 ClearValue
**          Value to be used for clearing the surface.
**
**      gctUINT8 ClearMask
**          Byte-mask to be used for clearing the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Clear(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask
    )
{
    gceSTATUS status;
    gctINT32 tileWidth = 0, tileHeight = 0;

    gcmHEADER_ARG("Surface=0x%08X Left=%d "
                  "Top=%d Right=%d Bottom=%d ClearValue=0x%08x "
                  "ClearMask=0x%02x",
                  Surface, Left, Top, Right, Bottom,
                  ClearValue, ClearMask);

    switch (Surface->format)
    {
    case gcvSURF_X4R4G4B4:
    case gcvSURF_X1R5G5B5:
    case gcvSURF_R5G6B5:
    case gcvSURF_X4B4G4R4:
    case gcvSURF_X1B5G5R5:
        if (ClearMask == 0x7)
        {
            /* When the format has no alpha channel, fake the ClearMask to
            ** include alpha channel clearing.   This will allow us to use
            ** resolve clear. */
            ClearMask = 0xF;
        }
        break;

    default:
        break;
    }

    if ((ClearMask != 0xF)
    &&  (Surface->format != gcvSURF_X8R8G8B8)
    &&  (Surface->format != gcvSURF_A8R8G8B8)
    &&  (Surface->format != gcvSURF_D24S8)
    &&  (Surface->format != gcvSURF_D24X8)
    &&  (Surface->format != gcvSURF_D16)
    &&  (Surface->format != gcvSURF_S8)
    )
    {
        /* Don't clear with mask when channels are not byte sized. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    switch (Surface->tiling)
    {
    case gcvTILED:
        /* Query the tile size. */
        gcmONERROR(
            gcoHARDWARE_QueryTileSize(gcvNULL, gcvNULL,
                                      &tileWidth, &tileHeight,
                                      gcvNULL));
        break;

    case gcvMULTI_TILED:
        /* When multi tile the tile size is 8x4 and the tile is interleaved */
        /* So it needs 4 8x4 tile aligned. */
        tileWidth = 16;
        tileHeight = 8;
        break;

    case gcvSUPERTILED:
        tileWidth = 64;
        tileHeight = 64;
        break;

    case gcvMULTI_SUPERTILED:
        tileWidth = 64;
        tileHeight = 128;
        break;
    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Try hardware clear. */
    status = _ClearHardware(Hardware,
                            Surface,
                            Left,
                            Top,
                            Right,
                            Bottom,
                            ClearValue,
                            ClearValueUpper,
                            ClearMask,
                            tileWidth,
                            tileHeight);

    if (gcmIS_ERROR(status))
    {
        status = gcvSTATUS_NOT_ALIGNED;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ClearSoftware
**
**  Clear the buffer with software implementation. Buffer is assumed to be
**  tiled.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctPOINTER LogicalAddress
**          Base address of surface to clear.
**
**      gctUINT32 Stride
**          Stride of surface.
**
**      gctINT32 Left
**          Left coordinate of rectangle to clear.
**
**      gctINT32 Top
**          Top coordinate of rectangle to clear.
**
**      gctINT32 Right
**          Right coordinate of rectangle to clear.
**
**      gctINT32 Bottom
**          Bottom coordinate of rectangle to clear.
**
**      gceSURF_FORMAT Format
**          Format of surface to clear.
**
**      gctUINT32 ClearValue
**          Value to be used for clearing the surface.
**
**      gctUINT8 ClearMask
**          Byte-mask to be used for clearing the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ClearSoftware(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gctUINT8 StencilWriteMask
    )
{
    gceSTATUS status;
    gctUINT32 bitsPerPixel   = 0;
    gctUINT32 channelMask[4] = {0};
    static gctBOOL printed   = gcvFALSE;
    gctBOOL fastMSAA         = gcvFALSE;
    gceSURF_TYPE type        = gcvSURF_TYPE_UNKNOWN;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%08X Left=%d "
                  "Top=%d Right=%d Bottom=%d ClearValue=0x%08x "
                  "ClearMask=0x%02x",
                  Hardware, Surface, Left, Top, Right, Bottom,
                  ClearValue, ClearMask);

    /* For a clear that is not tile aligned, our hardware might not be able to
       do it.  So here is the software implementation. */
    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (!printed)
    {
        printed = gcvTRUE;

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HARDWARE,
                      "%s: Performing a software clear!",
                      __FUNCTION__);
    }

    /* Flush the pipe. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Commit the command queue. */
    gcmONERROR(gcoHARDWARE_Commit(Hardware));

    /* Stall the hardware. */
    gcmONERROR(gcoHARDWARE_Stall(Hardware));

    fastMSAA = (Hardware->features[gcvFEATURE_FAST_MSAA] ||
                Hardware->features[gcvFEATURE_SMALL_MSAA]);

    /* Query pixel depth. */
    gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                         &bitsPerPixel,
                                         gcvNULL));

    switch (Surface->format)
    {
    case gcvSURF_X4R4G4B4: /* 12-bit RGB color without alpha channel. */
        channelMask[0] = 0x000F;
        channelMask[1] = 0x00F0;
        channelMask[2] = 0x0F00;
        channelMask[3] = 0x0;
        break;

    case gcvSURF_D16:      /* 16-bit Depth. */
    case gcvSURF_A4R4G4B4: /* 12-bit RGB color with alpha channel. */
        channelMask[0] = 0x000F;
        channelMask[1] = 0x00F0;
        channelMask[2] = 0x0F00;
        channelMask[3] = 0xF000;
        break;

    case gcvSURF_X1R5G5B5: /* 15-bit RGB color without alpha channel. */
        channelMask[0] = 0x001F;
        channelMask[1] = 0x03E0;
        channelMask[2] = 0x7C00;
        channelMask[3] = 0x0;
        break;

    case gcvSURF_A1R5G5B5: /* 15-bit RGB color with alpha channel. */
        channelMask[0] = 0x001F;
        channelMask[1] = 0x03E0;
        channelMask[2] = 0x7C00;
        channelMask[3] = 0x8000;
        break;

    case gcvSURF_R5G6B5: /* 16-bit RGB color without alpha channel. */
        channelMask[0] = 0x001F;
        channelMask[1] = 0x07E0;
        channelMask[2] = 0xF800;
        channelMask[3] = 0x0;
        break;

    case gcvSURF_D24S8:    /* 24-bit Depth with 8 bit Stencil. */
    case gcvSURF_D24X8:    /* 24-bit Depth with 8 bit Stencil. */
    case gcvSURF_X8R8G8B8: /* 24-bit RGB without alpha channel. */
    case gcvSURF_A8R8G8B8: /* 24-bit RGB with alpha channel. */
    case gcvSURF_G8R8:     /* The clear value do not have channel 2 and 3 */
    case gcvSURF_R8_1_X8R8G8B8:
    case gcvSURF_G8R8_1_X8R8G8B8:
        channelMask[0] = 0x000000FF;
        channelMask[1] = 0x0000FF00;
        channelMask[2] = 0x00FF0000;
        channelMask[3] = 0xFF000000;
        break;

    case gcvSURF_X2B10G10R10:
        channelMask[0] = 0x000003FF;
        channelMask[1] = 0x000FFC00;
        channelMask[2] = 0x3FF00000;
        channelMask[3] = 0x0;
        break;

    case gcvSURF_A2B10G10R10:
        channelMask[0] = 0x000003FF;
        channelMask[1] = 0x000FFC00;
        channelMask[2] = 0x3FF00000;
        channelMask[3] = 0xC0000000;
        break;

    case gcvSURF_X16B16G16R16:
    case gcvSURF_A16B16G16R16:
    case gcvSURF_X16B16G16R16F:
    case gcvSURF_A16B16G16R16F:
        break;

    case gcvSURF_S8:
        break;

    default:
        status = gcvSTATUS_NOT_SUPPORTED;
        gcmFOOTER();
        return status;
    }

    /* Expand 16-bit mask into 32-bit mask. */
    if (bitsPerPixel == 16)
    {
        channelMask[0] = channelMask[0] | (channelMask[0] << 16);
        channelMask[1] = channelMask[1] | (channelMask[1] << 16);
        channelMask[2] = channelMask[2] | (channelMask[2] << 16);
        channelMask[3] = channelMask[3] | (channelMask[3] << 16);
    }

    type = gcvSURF_RENDER_TARGET;

    if ((Surface->format == gcvSURF_D16)
        || (Surface->format == gcvSURF_D24S8)
        || (Surface->format == gcvSURF_D32)
        || (Surface->format == gcvSURF_D24X8)
        )
    {
        type = gcvSURF_DEPTH;
    }

    gcmONERROR(gcoSURF_NODE_Cache(&Surface->node,
                                   Surface->node.logical,
                                   Surface->size,
                                   gcvCACHE_INVALIDATE));

    gcmONERROR(
        _ClearSoftware(Surface->node.logical, Surface->node.logicalBottom,
                       Surface->stride, Left, Top, Right, Bottom, channelMask,
                       ClearValue, ClearValueUpper, ClearMask, bitsPerPixel,
                       fastMSAA, Surface->tiling, type, StencilWriteMask));

    gcmONERROR(gcoSURF_NODE_Cache(&Surface->node,
                                   Surface->node.logical,
                                   Surface->size,
                                   gcvCACHE_CLEAN));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


static gceSTATUS
_ClearTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 Address,
    IN gctSIZE_T Bytes,
    IN gceSURF_TYPE Type,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gctBOOL  ClearAsDirty
    )
{
    gceSTATUS status;
    gctSIZE_T bytes;
    gcsPOINT rectSize        = {0, 0};
    gctUINT32 config, control;
    gctUINT32 dither[2]      = { ~0U, ~0U };
    gctUINT32 fillColor      = 0;
    gcsSAMPLES originSamples = {0, 0};
    gctUINT32 srcStride      = 0;
    gctBOOL multiPipe;

    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Address=0x%x "
                  "Bytes=%d Type=%d ClearValue=0x%x ClearMask=0x%x",
                  Hardware, Surface, Address,
                  Bytes, Type, ClearValue, ClearMask);

    if (ClearMask != 0xF)
    {
        gctBOOL bailOut = gcvTRUE;

        /* Allow ClearMask of 0x7, when Alpha is not needed. */
        if (((ClearMask == 0x7)
            && ((Surface->format == gcvSURF_X8R8G8B8)
            || (Surface->format == gcvSURF_R5G6B5))))
        {
            bailOut = gcvFALSE;
        }
        else if (((ClearMask == 0xE)
            && (Surface->hasStencilComponent)
            && (Surface->canDropStencilPlane)))
        {
           bailOut = gcvFALSE;
        }
        else if (Surface->format == gcvSURF_S8)
        {
            bailOut = gcvFALSE;
        }

        if (bailOut)
        {
            status = (gcvSTATUS_NOT_SUPPORTED);
            goto OnError;
        }
    }

    /* Query the tile status size. */
    gcmONERROR(
        gcoHARDWARE_QueryTileStatus(Hardware,
                                    Surface->alignedWidth,
                                    Surface->alignedHeight,
                                    Surface->size,
                                    &bytes,
                                    gcvNULL,
                                    &fillColor));

    if (ClearAsDirty)
    {
        /* Clear tile status as Request64. */
        fillColor = 0x0;
    }

    if (Bytes > 0)
    {
        bytes = Bytes;
    }

    multiPipe = (Surface->tiling & gcvTILING_SPLIT_BUFFER) || Hardware->multiPipeResolve;

    /* Compute the hw specific clear window. */
    gcmONERROR(
        gcoHARDWARE_ComputeClearWindow(Hardware,
                                       bytes,
                                       (gctUINT32_PTR)&rectSize.x,
                                       (gctUINT32_PTR)&rectSize.y));

    /* Disable MSAA setting for MSAA tile status buffer clear */
    originSamples.x = Surface->samples.x;
    originSamples.y = Surface->samples.y;

    Surface->samples.x = Surface->samples.y = 1;

    gcmONERROR(gcoHARDWARE_AdjustCacheMode(Hardware, Surface));

    /* restore sample information */
    Surface->samples.x = originSamples.x;
    Surface->samples.y = originSamples.y;
    /* clear flag*/
    originSamples.x = 0;

    switch(Type)
    {

    case gcvSURF_HIERARCHICAL_DEPTH:
        Surface->fcValueHz = ClearValue;
        break;

    default:
        Surface->fcValue = ClearValue;
        Surface->fcValueUpper = ClearValueUpper;
        break;
    }

    /* Build AQRsConfig register. */
    config = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));

    /* Build AQRsClearControl register. */
    control = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));

    /* Cache mode programming. */
    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        /* Disable 256B cache always for tile status buffer clear */
        srcStride = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
    }

    /* Switch to 3D pipe. */
    gcmONERROR(
        gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Flush cache. */
    gcmONERROR(
        gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Flush tile status cache. */
    gcmONERROR(
        gcoHARDWARE_FlushTileStatus(Hardware, Surface, gcvFALSE));

    if ((Hardware->config->chipModel != gcv4000)
        || (Hardware->config->chipRevision != 0x5222))
    {
        gcmONERROR(
            gcoHARDWARE_Semaphore(Hardware,
                                  gcvWHERE_RASTER,
                                  gcvWHERE_PIXEL,
                                  gcvHOW_SEMAPHORE_STALL,
                                  gcvNULL));
    }

    reserveSize = /* config state */
                  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                  /* dither state */
                  gcmALIGN((2 + 1) * gcmSIZEOF(gctUINT32), 8) +
                  /* Dest stride */
                  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                  /* Clear value */
                  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                  /* Clear control */
                  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) +
                  /* extra config state */
                  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

    /* Cache mode programming. */
    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        reserveSize += /* Src stride */
                       gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

    }

    if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
    {
        reserveSize += /* dual dest address */
                       gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
    }
    else
    {
        reserveSize += /* single dest address */
                       gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

        if ((Hardware->config->pixelPipes > 1) ||
             Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
        {
             reserveSize += /* new single dest address */
                           gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }
    }

    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0581, config);     gcmENDSTATEBATCH(reserve, memory);};

    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058C, 2);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058C, dither[0]);

        gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058C+1, dither[1]);

        gcmSETFILLER(reserve, memory);

    gcmENDSTATEBATCH(reserve, memory);


    if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
    {
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE,
                            0x05B8,
                            Address);

            gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE,
                            0x05B8+1,
                            Address + (bytes / 2));

            gcmSETFILLER(reserve, memory);

        gcmENDSTATEBATCH(reserve, memory);
    }
    else
    {
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0584, Address);     gcmENDSTATEBATCH(reserve, memory);};

        if ((Hardware->config->pixelPipes > 1) ||
             Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B8, Address);     gcmENDSTATEBATCH(reserve, memory);};
        }

    }
    /* Cache mode programming. */
    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0583, srcStride);gcmENDSTATEBATCH(reserve, memory);};
    }

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0585, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0585) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0585, rectSize.x * 4);     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0590, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0590) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0590, fillColor);     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058F, control);     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05A8, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (!Hardware->multiPipeResolve) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))));     gcmENDSTATEBATCH(reserve, memory);};


    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);


    gcmONERROR(
        gcoHARDWARE_ProgramResolve(Hardware,
                                   rectSize,
                                   multiPipe,
                                   gcvMSAA_DOWNSAMPLE_AVERAGE));
OnError:
    /* restore sample information */
    if (originSamples.x != 0)
    {
        Surface->samples.x = originSamples.x;
        Surface->samples.y = originSamples.y;
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHARDWARE_ClearTileStatus
**
**  Append a command buffer with a CLEAR TILE STATUS command.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer of the surface to clear.
**
**      gctUINT32 Address
**          Base address of tile status to clear.
**
**      gceSURF_TYPE Type
**          Type of surface to clear.
**
**      gctUINT32 ClearValue
**          Value to be used for clearing the surface.
**
**      gctUINT32 ClearValueUpper
**          Upper Value to be used for clearing the surface.
**          For depth/hz, it's same as ClearValue always
**
**
**      gctUINT8 ClearMask
**          Byte-mask to be used for clearing the surface.
**
**      gctBOOL ClearAsDirty
**          Clear tile status as dirty (Reqest64) state.
**          Otherwise cleared as FastClear state.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ClearTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 Address,
    IN gctSIZE_T Bytes,
    IN gceSURF_TYPE Type,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Address=0x%x "
                  "Bytes=%d Type=%d ClearValue=0x%x ClearMask=0x%x",
                  Hardware, Surface, Address,
                  Bytes, Type, ClearValue, ClearMask);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Do the tile status clear. */
    status = _ClearTileStatus(Hardware,
                              Surface,
                              Address,
                              Bytes,
                              Type,
                              ClearValue,
                              ClearValueUpper,
                              ClearMask,
                              gcvFALSE);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ComputeClearWindow
**
**  Compute the Width Height for clearing,
**  when only the bytes to clear are known.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gctSIZE_T Bytes
**          Number of Bytes to clear.
**
**  OUTPUT:
**
**      gctUINT32 Width
**          Width of clear window.
**
**      gctUINT32 Height
**          Height of clear window.
**
*/
gceSTATUS
gcoHARDWARE_ComputeClearWindow(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T Bytes,
    OUT gctUINT32 *Width,
    OUT gctUINT32 *Height
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 width, height, alignY;

    gcmHEADER_ARG("Bytes=%d Width=%d Height=%d",
                  Bytes, gcmOPT_VALUE(Width), gcmOPT_VALUE(Height));

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Compute alignment. */
    alignY = (Hardware->resolveAlignmentY * 2) - 1;

    /* Compute rectangular width and height. */
    width  = Hardware->resolveAlignmentX;
    gcmSAFECASTSIZET(height, Bytes / (width * 4));

    /* Too small? */
    if (height < (Hardware->resolveAlignmentY))
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    while ((width < 8192) && ((height & alignY) == 0))
    {
        width  *= 2;
        height /= 2;
    }

    gcmASSERT((gctSIZE_T) (width * height * 4) == Bytes);

    if ((gctSIZE_T) (width * height * 4) == Bytes)
    {
        *Width  = width;
        *Height = height;
    }
    else
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHARDWARE_ClearTileStatusWindowAligned
**
**  This is core partial fast clear logic.
**  Try to clear aligned rectangle with fast clear, and clear other areas
**  outside the aligned rectangle using 3D draw in caller.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer of the surface to clear.
**
**      gceSURF_TYPE Type
**          Type of surface to clear.
**
**      gctUINT32 ClearValue
**          Value to be used for clearing the surface.
**
**      gctUINT32 ClearValueUpper
**          Upper Value to be used for clearing the surface.
**          For depth/hz, it's same as ClearValue always
**
**      gctUINT8 ClearMask
**          Byte-mask to be used for clearing the surface.
**
**      gcsRECT_PTR Rect
**          The original rectangle to be cleared.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ClearTileStatusWindowAligned(
    IN gcoHARDWARE Hardware,
    gcsSURF_INFO_PTR Surface,
    IN gceSURF_TYPE Type,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gcsRECT_PTR Rect,
    OUT gcsRECT_PTR AlignedRect
    )
{
    gceSTATUS status;
    gctBOOL is2BitPerTile;

    /* Source source parameters. */
    gcsRECT rect;
    gctUINT bytesPerPixel;
    gcsPOINT size;

    /* Source surface alignment limitation. */
    gctUINT alignX, alignY;
    gctINT blockWidth, blockHeight;

    /* Aligned source surface parameters. */
    gcsPOINT clearOrigin;
    gcsPOINT clearSize;

    /* Tile status surface parameters. */
    gctUINT32 bytes;
    gctUINT32 fillColor;
    gctUINT   stride;

    gctBOOL multiPipe;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmGETHARDWARE(Hardware);

    if (ClearMask != 0xF)
    {
        gctBOOL bailOut = gcvTRUE;

        /* Allow ClearMask of 0x7, when Alpha is not needed. */
        if (((ClearMask == 0x7)
            && ((Surface->format == gcvSURF_X8R8G8B8)
            || (Surface->format == gcvSURF_R5G6B5))))
        {
            bailOut = gcvFALSE;
        }
        else if (((ClearMask == 0xE)
            && (Surface->hasStencilComponent)
            && (Surface->canDropStencilPlane)))
        {
           bailOut = gcvFALSE;
        }
        else if (Surface->format == gcvSURF_S8)
        {
            gcmPRINT("TODO:partial fast clear for S8");
        }

        if (bailOut)
        {
            status = (gcvSTATUS_NOT_SUPPORTED);
            goto OnError;
        }
    }

    if (!Surface->superTiled)
    {
        /* TODO: only support supertiled surface for now. */
        status = gcvSTATUS_NOT_SUPPORTED;
        goto OnError;
    }

    multiPipe = (Surface->tiling & gcvTILING_SPLIT_BUFFER) || Hardware->multiPipeResolve;

    /* Get bytes per pixel. */
    bytesPerPixel = Surface->is16Bit ? 2 : 4;

    /* Check tile status size. */
    is2BitPerTile = ((((gctUINT32) (Hardware->config->chipMinorFeatures)) >> (0 ? 10:10) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))));

    /* Query the tile status size for one 64x64 supertile. */
    bytes     = 64 * 64 * bytesPerPixel / (is2BitPerTile ? 256 : 128);
    fillColor = is2BitPerTile ? 0x55555555 : 0x11111111;

    if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
    {
        /*
         * Extra pages needed to offset sub-buffers to different banks.
         * Calculate corresponding ts bytes(32B multiplier) of the offset.
         *  64 x 64 x bytesPerPixel : bytes / 32B
         *  bottomBufferOffset      : ?
         */
        if ((Surface->bottomBufferOffset * bytes / 32) % (64 * 64 * bytesPerPixel))
        {
            /* Corresponding second address in ts is not 32B aligned. */
            status = gcvSTATUS_NOT_SUPPORTED;
            goto OnError;
        }
    }

    /* Shadow clear rectangle. */
    rect = *Rect;

    /* Use aligned size if out of valid rectangle. */
    if (rect.right >= Surface->rect.right / Surface->samples.x)
    {
        rect.right = Surface->alignedWidth;
    }

    if (rect.bottom >= Surface->rect.bottom / Surface->samples.y)
    {
        rect.bottom = Surface->alignedHeight;
    }

    /*
     * Determine minimal resolve size alignment.
     * resolve rectangle must be 16x4 aligned. 16x4 size is 16x4x2B.
     */
    blockWidth  = 64 * (16*4*2 / bytes);
    blockHeight = (Surface->tiling & gcvTILING_SPLIT_BUFFER) ? 128 : 64;

    /*
     * Determine origin alignment.
     * Origin must be 32B aligned for RS dest.
     * 32B in ts = contiguous 128 tile in surface.
     *           = 1x1 supertile for 16bpp (64x64 in coord)
     *           = 0.5x1 supertile for 32bpp (32x64 in coord)
     *          (=>1x1 supertile for 32 bpp (64x64 in coord))
     */
    alignX = 64;
    alignY = (Surface->tiling & gcvTILING_SPLIT_BUFFER) ? 128 : 64;

    /* Calculate aligned origin. */
    clearOrigin.x = gcmALIGN(rect.left, alignX);
    clearOrigin.y = gcmALIGN(rect.top, alignY);

    /* Calculate aligned size. */
    clearSize.x = gcmALIGN_BASE(rect.right  - clearOrigin.x, alignX);
    clearSize.y = gcmALIGN_BASE(rect.bottom - clearOrigin.y, alignY);

    if (clearSize.x <= blockWidth || clearSize.y <= blockHeight)
    {
        /* Clear size too small. */
        status = gcvSTATUS_NOT_SUPPORTED;
        goto OnError;
    }

    /* Output aligned rectangle. */
    AlignedRect->left   = clearOrigin.x;
    AlignedRect->top    = clearOrigin.y;
    AlignedRect->right  = clearOrigin.x + clearSize.x;
    AlignedRect->bottom = clearOrigin.y + clearSize.y;

    if ((AlignedRect->left   != Rect->left) ||
        (AlignedRect->top    != Rect->top) ||
        (AlignedRect->right  != Rect->right) ||
        (AlignedRect->bottom != Rect->bottom))
    {
        /* check renderable. */
        status = gcoHARDWARE_QuerySurfaceRenderable(Hardware, Surface);

        if (gcmIS_ERROR(status))
        {
            /*
             * TODO: refine this check.
             * The unaligned areas will be cleared with 3D draw. If surface is
             * not renderable, return not supported.
             */
            status = gcvSTATUS_NOT_SUPPORTED;
            goto OnError;
        }

#if !gcdHAL_3D_DRAWBLIT
        /*
         * Can not support unaligned clear when 3D drawblit is not enabled.
         *
         * TODO: 3D drawblit will finally be permanently enabled.
         * remove this code after.
         */
        status = gcvSTATUS_NOT_SUPPORTED;
        goto OnError;
#endif
    }

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Flush cache. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    if (Surface->tileStatusDisabled)
    {
        /* Clear tile status as dirty, prepare to enable. */
        gcmVERIFY_OK(
            _ClearTileStatus(Hardware,
                             Surface,
                             Surface->tileStatusNode.physical,
                             0,
                             Type,
                             ClearValue,
                             ClearValueUpper,
                             ClearMask,
                             gcvTRUE));
    }

    /* Calculate tile status surface stride. */
    stride = Surface->alignedWidth / 64 * bytes;

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Program registers. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0581, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) );     gcmENDSTATEBATCH(reserve, memory);};

    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058C, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x058C,
            ~0U
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x058C + 1,
            ~0U
            );

        gcmSETFILLER(
            reserve, memory
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0585, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0585) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0585, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:0) - (0 ? 19:0) + 1))))))) << (0 ? 19:0))) | (((gctUINT32) ((gctUINT32) (stride) & ((gctUINT32) ((((1 ? 19:0) - (0 ? 19:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:0) - (0 ? 19:0) + 1))))))) << (0 ? 19:0))) | ((((gctUINT32) (stride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (((Surface->tiling & gcvTILING_SPLIT_BUFFER) != 0)) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) );     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0590, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0590) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0590, fillColor );     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058F, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (gcvTRUE) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* Append new configuration register. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05A8, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Determine the size of the buffer to reserve, for target address. */
    if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
    {
        reserveSize = gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
    }
    else
    {
        reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

        if (Hardware->config->pixelPipes > 1)
        {
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }
    }

    /*
     * clear left area.
     *
     * +....----------------+----+
     * |x   x   x   x   x   |    |
     * |  x   x   x   x   x |    |
     * |x   x   x   x   x   |    |
     * |  x   x   x   x   x |    |
     * +-....---------------+----+
     * |<-  16x4 aligned  ->|
     */
    {
        gctUINT32 physical[2];
        gcsPOINT rectSize;

        size.x = gcmALIGN_BASE(clearSize.x, blockWidth);
        size.y = clearSize.y;

        physical[0] = Surface->tileStatusNode.physical
                    + (Surface->alignedWidth / 64) * (clearOrigin.y / blockHeight) * bytes
                    + (clearOrigin.x / 64) * bytes;

        physical[1] = physical[0]
                    + bytes * Surface->bottomBufferOffset / 64 / 64 / bytesPerPixel;

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

        if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
        {
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8,
                    physical[0]
                    );

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8 + 1,
                    physical[1]
                    );

                gcmSETFILLER(
                    reserve, memory
                    );

            gcmENDSTATEBATCH(
                reserve, memory
                );
        }
        else
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0584, physical[0] );     gcmENDSTATEBATCH(reserve, memory);};

            if (Hardware->config->pixelPipes > 1)
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B8, physical[0] );     gcmENDSTATEBATCH(reserve, memory);};
            }
        }

        rectSize.x = 16 * (size.x / blockWidth);
        rectSize.y =  4 * (size.y / 64);

        /* Trigger clear. */
        gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware, rectSize, multiPipe, gcvMSAA_DOWNSAMPLE_AVERAGE));
    }

    /*
     * clear right area.
     *
     * +....----------------+----+
     * |        x   x   x   |x   |
     * |      x   x   x   x |  x |
     * |        x   x   x   |x   |
     * |      x   x   x   x |  x |
     * +-....---------------+----+
     *       |<-     16x4      ->|
     */
    if (size.x != clearSize.x)
    {
        gctUINT32 physical[2];
        gcsPOINT rectSize;

        clearOrigin.x = clearOrigin.x + clearSize.x - blockWidth;

        physical[0] = Surface->tileStatusNode.physical
                    + (Surface->alignedWidth / 64) * (clearOrigin.y / blockHeight) * bytes
                    + (clearOrigin.x / 64) * bytes;

        physical[1] = physical[0]
                    + bytes * Surface->bottomBufferOffset / 64 / 64 / bytesPerPixel;

        /* Reserve space in the command buffer. */
        gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

        if (Surface->tiling & gcvTILING_SPLIT_BUFFER)
        {
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8,
                    physical[0]
                    );

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8 + 1,
                    physical[1]
                    );

                gcmSETFILLER(
                    reserve, memory
                    );

            gcmENDSTATEBATCH(
                reserve, memory
                );
        }
        else
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0584, physical[0] );     gcmENDSTATEBATCH(reserve, memory);};

            if (Hardware->config->pixelPipes > 1)
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B8, physical[0] );     gcmENDSTATEBATCH(reserve, memory);};
            }
        }

        gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

        rectSize.x = 16;
        rectSize.y =  4 * (size.y / 64);

        /* Trigger clear. */
        gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware, rectSize, multiPipe, gcvMSAA_DOWNSAMPLE_AVERAGE));
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif


