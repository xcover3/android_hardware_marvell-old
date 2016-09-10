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

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE

#if gcdENABLE_3D

/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/

typedef struct _gcsSWIZZLE
{
    gctINT8 r;
    gctINT8 g;
    gctINT8 b;
    gctINT8 a;
}
gcsSWIZZLE;

/*
* Information for different trigger register.
*/
typedef union _gcu3DBLITCOMMAND_INFO
{
    gctBOOL dither;
}
gcu3DBLITCOMMAND_INFO;

/*
* Commands of 3DBLIT
*/
typedef enum gce3DBLITCOMMAND
{
    gcv3DBLIT_CLEAR,
    gcv3DBLIT_BLT,
    gcv3DBLIT_COPY,
    gcv3DBLIT_TILEFILL,
    gcv3DBLIT_420TILER,
}
gce3DBLITCOMMAND;

/******************************************************************************\
********************************* Support Code *********************************
\******************************************************************************/

#define gcm3DBLIT_CACHE_FLUSH() \
        gcmSETSINGLECTRLSTATE(\
            stateDelta, reserve, memory, gcvFALSE, 0x0E03, \
            gcmSETFIELDVALUE(0, AQ_FLUSH, BLT3_DCACHE, ENABLE) \
        ); \
        gcmSETSINGLECTRLSTATE(\
            stateDelta, reserve, memory, gcvFALSE, 0x0594, \
            gcmSETFIELDVALUE(0, GC_TILE_CACHE_FLUSH, FLUSH, ENABLE) \
        );

static gctUINT32
_CaculateReserveBytes(
    IN gctUINT32 NumStates
    )
{
    return gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * NumStates, 8);
}

static gctUINT32
_CaculateBatchReserveBytes(
    IN gctUINT32 NumStates
    )
{
    return gcmALIGN((1 + NumStates) * gcmSIZEOF(gctUINT32), 8);
}

static gceSTATUS
_ConvertBlitFormat(
    IN gceSURF_FORMAT Format,
    OUT gctUINT32 * HardwareFormat,
    OUT gcsSWIZZLE *Swizzle
    )
{
    gctUINT32 format;
    gcsSWIZZLE swizzle;

    static const gcsSWIZZLE swizzle_rgba = {
        0x0,
        0x1,
        0x2,
        0x3
    };

    static const gcsSWIZZLE swizzle_bgra = {
        0x2,
        0x1,
        0x0,
        0x3
    };

    switch (Format)
    {
    case gcvSURF_X4R4G4B4:
        format = 0x00;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A4R4G4B4:
        format = 0x01;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_X1R5G5B5:
        format = 0x02;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A1R5G5B5:
        format = 0x03;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_X1B5G5R5:
        format = 0x02;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A1B5G5R5:
        format = 0x03;
        swizzle = swizzle_bgra;
        break;

    case gcvSURF_R5G6B5:
        format = 0x04;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_B5G6R5:
        format = 0x04;
        swizzle = swizzle_bgra;
        break;

    case gcvSURF_X8R8G8B8:
    case gcvSURF_R8_1_X8R8G8B8:
    case gcvSURF_G8R8_1_X8R8G8B8:
        format = 0x05;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A8R8G8B8:
        format = 0x06;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_X8B8G8R8:
        format = 0x05;
        swizzle = swizzle_bgra;
        break;

    case gcvSURF_A8B8G8R8:
        format = 0x06;
        swizzle = swizzle_bgra;
        break;

    case gcvSURF_YUY2:
        format = 0x07;
        swizzle = swizzle_rgba;
        break;

    /* Fake 16-bit formats. */
    case gcvSURF_D16:
    case gcvSURF_UYVY:
        format = 0x01;
        swizzle = swizzle_rgba;
        break;

    /* Fake 32-bit formats. */
    case gcvSURF_D24S8:
    case gcvSURF_D24X8:
    case gcvSURF_D32:
        format = 0x06;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A8:
        format = 0x10;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_A2R10G10B10:
        format = 0x16;
        swizzle = swizzle_rgba;
        break;

     case gcvSURF_A2B10G10R10:
        format = 0x16;
        swizzle = swizzle_bgra;
        break;

    case gcvSURF_A8L8:
        format = 0x20;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_L8:
        format = 0x21;
        swizzle = swizzle_rgba;
        break;

    case gcvSURF_R8G8B8:
        format = 0x22;
        swizzle = swizzle_rgba;
        break;

    default:
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    *HardwareFormat = format;

    if (Swizzle != gcvNULL)
    {
        *Swizzle = swizzle;
    }

    return gcvSTATUS_OK;
}

static void
_ConfigMSAA(
    IN gcsSAMPLES *Samples,
    OUT gctUINT32_PTR MSAA
    )
{
    if (Samples->x == 2 && Samples->y == 2)
    {
        * MSAA = 0x3;
    }
    else
    if (Samples->x == 2 && Samples->y == 1)
    {
        * MSAA = 0x1;
    }
    else
    if (Samples->x == 1 && Samples->y == 2)
    {
        * MSAA = 0x2;
    }
    else
    {
        * MSAA = 0x0;
    }
}

static void
_ConfigTiling(
    IN gceTILING Info,
    OUT gctUINT32_PTR SuperTiling,
    OUT gctUINT32_PTR MultiTiling,
    OUT gctUINT32_PTR Tiling
    )
{
    *SuperTiling = (Info & gcvSUPERTILED) ? gcvTRUE : gcvFALSE;
    *MultiTiling = (Info & gcvMULTI_TILED) ? gcvTRUE : gcvFALSE;
    *Tiling      = (Info & gcvTILED) | *SuperTiling | *MultiTiling;
}

extern gctUINT32
_ByteMaskToBitMask(
    gctUINT32 ClearMask
    );

gctBOOL
_CanDoFastClear(
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info
    )
{
    gctUINT32       destTiling;
    gctUINT32       destsuperTiling;
    gctUINT32       destmultiTiling;
    gctUINT32       destMSAA;

    _ConfigTiling(DestInfo->tiling, &destsuperTiling, &destmultiTiling, &destTiling);

    _ConfigMSAA(&DestInfo->samples, &destMSAA);

    if (!((Info->origin->x == 0)
     &&  (Info->origin->y == 0)
     &&  (Info->rect->x >= (DestInfo->rect.right / DestInfo->samples.x))
     &&  (Info->rect->y >= (DestInfo->rect.bottom / DestInfo->samples.x)))
    )
    {
        return gcvFALSE;
    }

    if (Info->clearMask != 0xFFFFFFFF)
    {
        gctBOOL bailOut = gcvTRUE;

        if (((Info->clearMask == _ByteMaskToBitMask(0x7))
            && ((DestInfo->format == gcvSURF_X8R8G8B8)
            || (DestInfo->format == gcvSURF_R5G6B5))))
        {
            bailOut = gcvFALSE;
        }
        else if (((Info->clearMask == _ByteMaskToBitMask((0xE))
            && (DestInfo->hasStencilComponent)
            && (DestInfo->canDropStencilPlane)))
            )
        {
           bailOut = gcvFALSE;
        }

        if (bailOut)
        {
            return gcvFALSE;
        }
    }

    if (!Info->destTileStatusAddress)
    {
        return gcvFALSE;
    }

    if (Info->destAddress & 63)
    {
        return gcvFALSE;
    }

    if (DestInfo->stride & 63)
    {
        return gcvFALSE;
    }

    if (destmultiTiling)
    {
        return gcvFALSE;
    }

    if (destMSAA == 0x3
     && ((Info->destAddress & 255) || (Info->destTileStatusAddress & 255))
    )
    {
        return gcvFALSE;
    }

    return gcvTRUE;
}

gctBOOL
_CanDoClear(
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info
    )
{
    gctUINT32       destTiling;
    gctUINT32       destsuperTiling;
    gctUINT32       destmultiTiling;
    gctUINT32       destMSAA;

    _ConfigTiling(DestInfo->tiling, &destsuperTiling, &destmultiTiling, &destTiling);

    if (destmultiTiling
     && ((Info->destAddress & 63) || (Info->destTileStatusAddress & 63))
    )
    {
        return gcvFALSE;
    }

    _ConfigMSAA(&DestInfo->samples, &destMSAA);

    if (destMSAA
     && ((gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_FAST_MSAA) == gcvTRUE) ||
         (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_SMALL_MSAA) == gcvTRUE)))
    {
        return gcvFALSE;
    }

    return gcvTRUE;
}

static gceSTATUS
_3DBlitExecute(
    gcoHARDWARE     Hardware,
    gce3DBLITCOMMAND Command,
    gcu3DBLITCOMMAND_INFO *Info
    )
{
    gceSTATUS   status;
    gctUINT32   data = 0;

    static gctUINT32 commands[] = {
        0x1,
        0x2,
        0x3,
        0x4,
        0x5
    };

    static gctUINT32 dithers[] = {
        0x0,
        0x1,
    };

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Command=%d", Hardware, Command);
    gcmDEBUG_VERIFY_ARGUMENT(Command <= gcv3DBLIT_420TILER);

    gcmGETHARDWARE(Hardware);

    data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (commands[Command]) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)))
         | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (dithers[Info->dither]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
         ;

    reserveSize = _CaculateReserveBytes(5);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))));gcmENDSTATEBATCH(reserve, memory);};{{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))));gcmENDSTATEBATCH(reserve, memory);};;

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5018, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5018) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x5018, data );gcmENDSTATEBATCH(reserve, memory);};

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))));gcmENDSTATEBATCH(reserve, memory);};{{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))));gcmENDSTATEBATCH(reserve, memory);};;

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Avoid build warning. */
    stateDelta = gcvNULL;
    stateDelta = stateDelta;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_CopyData
**
**  Copy linear data from user memory to video memory.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR Memory
**          Pointer to the gcsSURF_NODE structure that defines the video memory
**          to copy the user data into.
**
**      gctUINT32 Offset
**          Offset into video memory to start copying data into.
**
**      gctCONST_POINTER Buffer
**          Pointer to user data to copy.
**
**      gctSIZE_T Bytes
**          Number of byte to copy.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoHARDWARE_CopyData(
    IN gcsSURF_NODE_PTR Memory,
    IN gctSIZE_T Offset,
    IN gctCONST_POINTER Buffer,
    IN gctSIZE_T Bytes
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Memory=0x%x Offset=%u Buffer=0x%x Bytes=%d",
                  Memory, Offset, Buffer, Bytes);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Memory != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(Buffer != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(Bytes > 0);

    do
    {
        /* Verify that the surface is locked. */
        gcmVERIFY_NODE_LOCK(Memory);

        /* Copy the memory using the CPU. */
        gcoOS_MemCopy(Memory->logical + Offset, Buffer, Bytes);

        /* Flush the CPU cache. */
        gcmERR_BREAK(gcoSURF_NODE_Cache(Memory,
                                      Memory->logical + Offset,
                                      Bytes,
                                      gcvCACHE_CLEAN));
    }
    while (gcvFALSE);

    /* Return result. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHARDWARE_3DBlitCopy
**
**  Copy arbitrary size of video memory.
**  SrcAddress and DestAddress is byte-aligned.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 SrcAddress
**          GPU address of source.
**
**      gctUINT32 DestAddress
**          GPU address of destination.
**
**      gctSIZE_T Bytes
**          Number of byte to copy.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoHARDWARE_3DBlitCopy(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 SrcAddress,
    IN gctUINT32 DestAddress,
    IN gctUINT32 CopySize
    )
{
    gceSTATUS   status;
    gcu3DBLITCOMMAND_INFO commandInfo;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmGETHARDWARE(Hardware);

    reserveSize = _CaculateReserveBytes(3);

    /* Flush the pipe. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* BltSrcAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5000, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5000) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5000, SrcAddress );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006, DestAddress );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltCopySize. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5015, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5015) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5015, CopySize );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    commandInfo.dither = gcvFALSE;

    /* Trigger 3D Blit. */
    gcmONERROR(_3DBlitExecute(Hardware, gcv3DBLIT_COPY, &commandInfo));

    return gcvSTATUS_OK;

OnError:
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_3DBlitBlt
**
**  Blit a rectangular area of a surface to another surface.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcInfo
**          Pointer to the source surface descriptor.
**
**      gcsSURF_INFO_PTR DestInfo
**          Pointer to the destination surface descriptor.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin of the source area to be blitted.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be blitted.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be blitted.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoHARDWARE_3DBlitBlt(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS   status;
    gctUINT32   srcFormat, destFormat;
    gctUINT32   srcStride, destStride;
    gctUINT32   srcTiling, destTiling;
    gctUINT32   srcsuperTiling, destsuperTiling;
    gctUINT32   srcmultiTiling, destmultiTiling;
    gctUINT32   srcMSAA, destMSAA;
    gctUINT32   srcConfig, destConfig;
    gctUINT32   srcAddress, destAddress;
    gctUINT32   srcAddress1, destAddress1;
    gctUINT32   srcConfigEx = 0, destConfigEx = 0;
    gctUINT32   srcTileStatusPhysical = 0;
    gctBOOL     srcFastClear = gcvFALSE;
    gctBOOL     destFastClear = gcvFALSE;
    gctBOOL     flipY = gcvFALSE;
    gctUINT32   width = RectSize->x;
    gctUINT32   height = RectSize->y;
    gctINT32    srcCompressionFormat = -1;
    gctBOOL     srcCompression = gcvFALSE;
    gctUINT32   color64;
    gcsSWIZZLE  srcSwizzle;
    gcsSWIZZLE  destSwizzle;
    gctBOOL     srcCacheMode = gcvFALSE;
    gctBOOL     destCacheMode = gcvFALSE;
    gcu3DBLITCOMMAND_INFO commandInfo;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmGETHARDWARE(Hardware);

    /***********************************************
    * Prepare arguements.
    */

    gcmONERROR(_ConvertBlitFormat(SrcInfo->format, &srcFormat, &srcSwizzle));
    gcmONERROR(_ConvertBlitFormat(DestInfo->format, &destFormat, &destSwizzle));

    /* Source stride. */
    srcStride = SrcInfo->stride;

    /* Destination stride. */
    destStride = DestInfo->stride;

    /* Source tiling. */
    _ConfigTiling(SrcInfo->tiling, &srcsuperTiling, &srcmultiTiling, &srcTiling);

    /* Destination tiling. */
    _ConfigTiling(DestInfo->tiling, &destsuperTiling, &destmultiTiling, &destTiling);

    _ConfigMSAA(&SrcInfo->samples, &srcMSAA);

    _ConfigMSAA(&DestInfo->samples, &destMSAA);


    if (srcMSAA)
    {
        gcmASSERT(destMSAA == 0 || destMSAA == srcMSAA);

        width *= SrcInfo->samples.x;
        height *= SrcInfo->samples.y;

        if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
            Hardware->features[gcvFEATURE_SMALL_MSAA])
        {
            if (srcMSAA == 0x3
             && ((SrcInfo->type == gcvSURF_RENDER_TARGET) || (SrcInfo->type == gcvSURF_DEPTH)))
            {
                srcCacheMode = gcvTRUE;

            }

            if (destMSAA == 0x3
             && ((DestInfo->type == gcvSURF_RENDER_TARGET) || (DestInfo->type == gcvSURF_DEPTH)))
            {

                destCacheMode = gcvTRUE;
            }
        }
    }

    flipY = SrcInfo->orientation != DestInfo->orientation;

    color64 = 0;

    srcConfig
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0))) | (((gctUINT32) ((gctUINT32) (srcStride) & ((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21))) | (((gctUINT32) ((gctUINT32) (srcFormat) & ((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27))) | (((gctUINT32) ((gctUINT32) (srcMSAA) & ((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (srcTiling) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (srcsuperTiling) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (srcmultiTiling) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)))
            ;

    destConfig
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0))) | (((gctUINT32) ((gctUINT32) (destStride) & ((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21))) | (((gctUINT32) ((gctUINT32) (destFormat) & ((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27))) | (((gctUINT32) ((gctUINT32) (destMSAA) & ((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (destTiling) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (destsuperTiling) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (destmultiTiling) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)))
            ;

    srcTileStatusPhysical = SrcInfo->tileStatusNode.physical;

    if (srcTileStatusPhysical)
    {
        srcFastClear = !SrcInfo->tileStatusDisabled;
    }

    destFastClear = gcvFALSE;
    DestInfo->tileStatusDisabled = gcvTRUE;

    if (srcFastClear)
    {
        srcCompressionFormat = SrcInfo->compressFormat;
        srcCompression = SrcInfo->compressed;
    }

    srcConfigEx
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (srcFastClear) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (srcCompression) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (srcCompressionFormat) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9))) | (((gctUINT32) ((gctUINT32) (srcSwizzle.r) & ((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11))) | (((gctUINT32) ((gctUINT32) (srcSwizzle.g) & ((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:13) - (0 ? 14:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:13) - (0 ? 14:13) + 1))))))) << (0 ? 14:13))) | (((gctUINT32) ((gctUINT32) (srcSwizzle.b) & ((gctUINT32) ((((1 ? 14:13) - (0 ? 14:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:13) - (0 ? 14:13) + 1))))))) << (0 ? 14:13)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:15) - (0 ? 16:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:15) - (0 ? 16:15) + 1))))))) << (0 ? 16:15))) | (((gctUINT32) ((gctUINT32) (srcSwizzle.a) & ((gctUINT32) ((((1 ? 16:15) - (0 ? 16:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:15) - (0 ? 16:15) + 1))))))) << (0 ? 16:15)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) ((gctUINT32) (srcCacheMode) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) ((gctUINT32) (color64) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));
        ;

    destConfigEx
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (destFastClear) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) ((gctUINT32) (flipY) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9))) | (((gctUINT32) ((gctUINT32) (destSwizzle.r) & ((gctUINT32) ((((1 ? 10:9) - (0 ? 10:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:9) - (0 ? 10:9) + 1))))))) << (0 ? 10:9)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11))) | (((gctUINT32) ((gctUINT32) (destSwizzle.g) & ((gctUINT32) ((((1 ? 12:11) - (0 ? 12:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:11) - (0 ? 12:11) + 1))))))) << (0 ? 12:11)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:13) - (0 ? 14:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:13) - (0 ? 14:13) + 1))))))) << (0 ? 14:13))) | (((gctUINT32) ((gctUINT32) (destSwizzle.b) & ((gctUINT32) ((((1 ? 14:13) - (0 ? 14:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:13) - (0 ? 14:13) + 1))))))) << (0 ? 14:13)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:15) - (0 ? 16:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:15) - (0 ? 16:15) + 1))))))) << (0 ? 16:15))) | (((gctUINT32) ((gctUINT32) (destSwizzle.a) & ((gctUINT32) ((((1 ? 16:15) - (0 ? 16:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:15) - (0 ? 16:15) + 1))))))) << (0 ? 16:15)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) ((gctUINT32) (destCacheMode) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)))
        ;

    /* Source address. */
    srcAddress = SrcInfo->node.physical + SrcInfo->offset;
    srcAddress1 = srcAddress + SrcInfo->bottomBufferOffset;

    /* Destination address. */
    destAddress = DestInfo->node.physical + DestInfo->offset;
    destAddress1 = destAddress + DestInfo->bottomBufferOffset;

    /***********************************************
    * Detemine reserve size.
    */

    reserveSize = _CaculateReserveBytes(9);

    if (srcmultiTiling)
    {
        reserveSize = _CaculateReserveBytes(1);
    }

    if (destmultiTiling)
    {
        reserveSize = _CaculateReserveBytes(1);
    }

    if (srcFastClear)
    {
        reserveSize += _CaculateReserveBytes(2);

        if (color64)
        {
            /* For ClearValue64. */
            reserveSize += _CaculateReserveBytes(1);
        }
    }

    /* Dither table */
    reserveSize += _CaculateReserveBytes(2);

    /***********************************************
    * Program states.
    */

    /* Flush the pipe. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* BltSrcConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5002, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5002) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5002, srcConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltSrcConfigEx. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5003, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5003) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5003, srcConfigEx );     gcmENDSTATEBATCH(reserve, memory);};

    if (srcFastClear)
    {
        /* BltSrcTileStatusAddress. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5004, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5004) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5004, srcTileStatusPhysical );     gcmENDSTATEBATCH(reserve, memory);};

        /* BltSrcClearValue. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500D, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500D, SrcInfo->fcValue );     gcmENDSTATEBATCH(reserve, memory);};

        if (color64)
        {
            /* BltSrcClearValue64. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500E, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500E) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500E, SrcInfo->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};
        }
    }

    /* BltSrcAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5000, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5000) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5000, srcAddress );     gcmENDSTATEBATCH(reserve, memory);};

    if (srcmultiTiling)
    {
        /* BltSrcAddress + 1. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5000 + 1, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5000 + 1) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5000 + 1, srcAddress1 );     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* BltSrcOrigin. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5005, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5005) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5005, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcOrigin->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcOrigin->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5009, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5009) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5009, destConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestConfigEx. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500A, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500A, destConfigEx );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006, destAddress );     gcmENDSTATEBATCH(reserve, memory);};

    if (srcmultiTiling)
    {
        /* BltSrcAddress + 1. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006 + 1, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006 + 1) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006 + 1, destAddress1 );     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* BltDestOrigin. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500B, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500B) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500B, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (DestOrigin->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (DestOrigin->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltWindowSize. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500C, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500C, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (width) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (height) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* If support 3DBlit, PE dither must be fixed. So always disable 3DBlit dither. */
    gcmASSERT(Hardware->features[gcvFEATURE_PE_DITHER_FIX]);

    /* BltDitherLow. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5016, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5016) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5016, ~0U );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDitherHigh. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5017, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5017) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5017, ~0U );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    commandInfo.dither = gcvFALSE;

    gcmONERROR(_3DBlitExecute(Hardware, gcv3DBLIT_BLT, &commandInfo));

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gcoHARDWARE_3DBlitClearQuery(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info,
    OUT gctBOOL * FastClear
    )
{
    *FastClear = gcvTRUE;

    /* Check limitation. */
    if (_CanDoFastClear(DestInfo, Info) == gcvFALSE)
    {
        *FastClear = gcvFALSE;

        if(_CanDoClear(DestInfo, Info) == gcvFALSE)
        {
            return gcvSTATUS_NOT_SUPPORTED;
        }
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_3DBlitClear
**
**  Clear a rectangular area of a surface.
**
**  Alignement requirements for clear:
**        FC     destination and stride    origin  width   height
**    -----------------------------------------------------------
**        W/     64 bytes                  no      no      no
**        W/O    byte                      no      no      no
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcInfo
**          Pointer to the source surface descriptor.
**
**      gcs3DBLIT_INFO_PTR Info
**          Pointer to the clear arguement.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the source area to be cleared.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be cleared.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoHARDWARE_3DBlitClear(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS       status;
    gctUINT32       destStride;
    gctUINT32       destAddress;
    gctUINT32       destAddress1;
    gctUINT32       destConfig;
    gctUINT32       destMSAA;
    gctUINT32       destTiling;
    gctUINT32       destsuperTiling;
    gctUINT32       destmultiTiling;
    gctUINT32       tileStatusPhysical;
    gctBOOL         destFastClear = gcvTRUE;
    gctBOOL         destCompression = gcvFALSE;
    gctINT32        destCompressionFormat = -1;
    gctUINT32       destConfigEx, srcConfigEx;
    gctUINT32       generalConfig;
    gctUINT32       color64;
    gctUINT32       clearValue[2] = {0, 0};
    gctBOOL         destCacheMode = gcvFALSE;
    gcu3DBLITCOMMAND_INFO commandInfo;
    gctUINT32       width = RectSize->x;
    gctUINT32       height = RectSize->y;

     /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x DestInfo=0x%x Info=0x%x DestOrigin=0x%x RectSize=0x%x",
                   Hardware, DestInfo, Info, DestOrigin, RectSize);

    gcmDEBUG_VERIFY_ARGUMENT(Info != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(RectSize != gcvNULL);

    gcmGETHARDWARE(Hardware);

    destFastClear = (Info->destTileStatusAddress != 0);

    commandInfo.dither = gcvFALSE;

    /* Destination stride. */
    destStride = DestInfo->stride;

    /* Destination tiling. */
    _ConfigTiling(DestInfo->tiling, &destsuperTiling, &destmultiTiling, &destTiling);

    _ConfigMSAA(&DestInfo->samples, &destMSAA);

    if (destMSAA)
    {
        if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
            Hardware->features[gcvFEATURE_SMALL_MSAA])
        {
            if (destMSAA == 0x3
             && ((DestInfo->type == gcvSURF_RENDER_TARGET) || (DestInfo->type == gcvSURF_DEPTH))
            )
            {
                destCacheMode = gcvTRUE;
            }
        }

        width *= DestInfo->samples.x;
        height *= DestInfo->samples.y;
    }
    else
    {
         gcmONERROR(gcoHARDWARE_AdjustCacheMode(Hardware,DestInfo));
    }

    destConfig
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0))) | (((gctUINT32) ((gctUINT32) (destStride) & ((gctUINT32) ((((1 ? 20:0) - (0 ? 20:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:0) - (0 ? 20:0) + 1))))))) << (0 ? 20:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 26:21) - (0 ? 26:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:21) - (0 ? 26:21) + 1))))))) << (0 ? 26:21)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27))) | (((gctUINT32) ((gctUINT32) (destMSAA) & ((gctUINT32) ((((1 ? 28:27) - (0 ? 28:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:27) - (0 ? 28:27) + 1))))))) << (0 ? 28:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (destTiling) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (destsuperTiling) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (destmultiTiling) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)))
            ;

    /* Destination address. */
    destAddress = Info->destAddress;
    destAddress1 = destAddress + DestInfo->bottomBufferOffset;

    /* Destination tile status buffer address. */
    tileStatusPhysical = Info->destTileStatusAddress;

    if (destFastClear)
    {
        destCompressionFormat = DestInfo->compressFormat;
        destCompression = DestInfo->compressed;
    }

    color64 = Info->clearValue[0] != Info->clearValue[1];

    destConfigEx
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (destFastClear) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (destCompression) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (destCompressionFormat) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (color64) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) ((gctUINT32) (destCacheMode) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)))
        ;

    srcConfigEx
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (destFastClear) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (destCompression) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (destCompressionFormat) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) ((gctUINT32) (color64) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) ((gctUINT32) (destCacheMode) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)))
        ;

    generalConfig
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:7) - (0 ? 9:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:7) - (0 ? 9:7) + 1))))))) << (0 ? 9:7))) | (((gctUINT32) ((gctUINT32) ((DestInfo->bitsPerPixel >> 3) - 1) & ((gctUINT32) ((((1 ? 9:7) - (0 ? 9:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:7) - (0 ? 9:7) + 1))))))) << (0 ? 9:7)));

    /* Determine fast clear value. */
    if (destFastClear)
    {
        /* If tile status is disabled, force same clearValue and newClearValue
         * to use Mode 0 to clear. */
        clearValue[0] = Info->clearValue[0];
        clearValue[1] = Info->clearValue[1];

        /* Now tile status buffer hold some clear bits.
         * Render pipe must read / write it when operate on surface.
         * TODO move it to upper layer. */
        DestInfo->tileStatusDisabled = gcvFALSE;
    }

    /***********************************************
    * Determine reserve size.
    */

    reserveSize = _CaculateReserveBytes(11);

    if (destmultiTiling)
    {
        reserveSize += _CaculateReserveBytes(2);
    }

    if (destFastClear)
    {
        reserveSize += _CaculateReserveBytes(3);
    }

    if (color64)
    {
        reserveSize += _CaculateReserveBytes(2);

        if (destFastClear)
        {
            reserveSize += _CaculateReserveBytes(1);
        }
    }

    /************************************************
    * Program states.
    */

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* BltGeneralConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5019, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5019) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5019, generalConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5009, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5009) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5009, destConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltDestConfigEx. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500A, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500A, destConfigEx );     gcmENDSTATEBATCH(reserve, memory);};

    /* DestAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006, destAddress );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltSrcConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5002, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5002) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5002, destConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltSrcConfigEx. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5003, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5003) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5003, srcConfigEx );     gcmENDSTATEBATCH(reserve, memory);};

    /* Set SrcAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5000, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5000) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5000, destAddress );     gcmENDSTATEBATCH(reserve, memory);};

    if (destmultiTiling)
    {
        /* DestAddress + 1. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006 + 1, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006 + 1) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006 + 1, destAddress1 );     gcmENDSTATEBATCH(reserve, memory);};

        /* SrcAddress + 1. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5000 + 1, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5000 + 1) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5000 + 1, destAddress1 );     gcmENDSTATEBATCH(reserve, memory);};
    }

    if (destFastClear)
    {
        /* DestTileStatusAddress. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5008, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5008) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5008, tileStatusPhysical );     gcmENDSTATEBATCH(reserve, memory);};

        /* Set SrcTileStatusAddress. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5004, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5004) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5004, tileStatusPhysical );     gcmENDSTATEBATCH(reserve, memory);};

        /* DstClearValue. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500F, clearValue[0] );     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Set NewClearValue. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5011, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5011) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5011, Info->clearValue[0] );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltClearMask. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5013, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5013) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5013, Info->clearMask );     gcmENDSTATEBATCH(reserve, memory);};

    if (color64)
    {
        /* NewClearValue64. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5012, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5012) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5012, Info->clearValue[1] );     gcmENDSTATEBATCH(reserve, memory);};

        /* BltClearMask64. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5014, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5014) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5014, Info->clearMask );     gcmENDSTATEBATCH(reserve, memory);};

        if (destFastClear)
        {
            /* BltDstClearValue64. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5010, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5010) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5010, clearValue[1] );     gcmENDSTATEBATCH(reserve, memory);};
        }
    }

    /* DestOrigin. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500B, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500B) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500B, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (DestOrigin->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (DestOrigin->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* BltWindowSize. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500C, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500C, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (width) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (height) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmONERROR(_3DBlitExecute(Hardware, gcv3DBLIT_CLEAR, &commandInfo));

    /* Target has dirty pixels. */
    DestInfo->dirty = gcvTRUE;

    /* Mark fast clear is used. */
    Info->useFastClear = destFastClear;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_3DBlitTileFill
**
**  Fill a surface from tile status.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcInfo
**          Pointer to the source surface descriptor.
**
**      gcs3DBLIT_INFO_PTR Info
**          Pointer to the 3DBLIT arguements.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoHARDWARE_3DBlitTileFill(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info
    )
{
    gceSTATUS status;
    gctSIZE_T tileCount = 0;
    gctUINT32 fillerTileSize;
    gctUINT32 fillerFormat;
    gctUINT32 generalConfig;
    gcu3DBLITCOMMAND_INFO commandInfo;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x DestInfo=0x%x Info=0x%x", Hardware, DestInfo, Info);

    gcmGETHARDWARE(Hardware);

    /* Calcluate tile count. */
    tileCount = DestInfo->node.size / 64;

    fillerTileSize = 0x0;

    commandInfo.dither = gcvFALSE;

    fillerFormat = (Info->clearValue[0] == Info->clearValue[1])
                 ?  0x0
                 :  0x1;

    generalConfig
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (fillerTileSize) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) ((gctUINT32) (fillerFormat) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)));

    /***********************************************
    * Determine reserve size.
    */

    reserveSize =
                  _CaculateBatchReserveBytes(3)
                + _CaculateReserveBytes(5);

    if (fillerFormat)
    {
        reserveSize += _CaculateReserveBytes(1);
    }

    /************************************************
    * Program states.
    */

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

     /* Reprogram tile status. */
    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)3  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 3 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0596,
            DestInfo->tileStatusNode.physical
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0597,
            DestInfo->node.physical
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0598,
            DestInfo->clearValue[0]
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    /* GeneralConfig. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5019, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5019) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5019, generalConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* DestAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5006, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5006) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5006, Info->destAddress );     gcmENDSTATEBATCH(reserve, memory);};

    /* DestTileStatusAddress. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5008, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5008) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5008, Info->destTileStatusAddress );     gcmENDSTATEBATCH(reserve, memory);};

    /* DstClearValue. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x500F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x500F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x500F, Info->clearValue[0] );     gcmENDSTATEBATCH(reserve, memory);};

    /* DstClearValue. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x501A, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x501A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x501A, tileCount );     gcmENDSTATEBATCH(reserve, memory);};

    if (fillerFormat)
    {
        /* DstClearValue64. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x5010, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x5010) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x5010, Info->clearValue[1] );     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmONERROR(_3DBlitExecute(Hardware, gcv3DBLIT_TILEFILL, &commandInfo));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_3DBlit420Tiler
**
**  Tile linear 4:2:0 source surface.
**  Use same configure registers as tiler in RS and new 3DBLIT trigger register.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcInfo
**          Pointer to the source surface descriptor.
**
**      gcsSURF_INFO_PTR DestInfo
**          Pointer to the destination surface descriptor.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin of the source area to be copied.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be copied.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be copied.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_3DBlit420Tiler(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gctUINT32 srcFormat;
    gctUINT32 uvSwizzle;
    gcu3DBLITCOMMAND_INFO info;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);


    /* Input limitations until more support is required. */
    if ((SrcOrigin->x  != 0) || (SrcOrigin->y  != 0) ||
        (DestOrigin->x != 0) || (DestOrigin->y != 0)
        )
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    gcmGETHARDWARE(Hardware);

    /* Determine the format. */
    if ((SrcInfo->format == gcvSURF_YV12) ||
        (SrcInfo->format == gcvSURF_I420))
    {
        /* No need to swizzle as we set the U and V addresses correctly. */
        uvSwizzle = 0x0;
        srcFormat = 0x0;
    }
    else if (SrcInfo->format == gcvSURF_NV12)
    {
        uvSwizzle = 0x0;
        srcFormat = 0x1;
    }
    else
    {
        uvSwizzle = 0x1;
        srcFormat = 0x1;
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 10) * gcmSIZEOF(gctUINT32), 8);

    /* Append FLUSH to the command buffer. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)10  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x059E, 10 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (10 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x059E) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        /* Set tiler configuration. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x059E,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (srcFormat) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) ((gctUINT32) (uvSwizzle) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
            );

        /* Set window size. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x059F,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (RectSize->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (RectSize->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
            );

        /* Set Y plane. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A0,
            SrcInfo->node.physical
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A1,
            SrcInfo->stride
            );

        /* Set U plane. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A2,
            SrcInfo->node.physical2
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A3,
            SrcInfo->uStride
            );

        /* Set V plane. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A4,
            SrcInfo->node.physical3
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A5,
            SrcInfo->vStride
            );

        /* Set destination. */
        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A6,
            DestInfo->node.physical
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x05A7,
            DestInfo->stride
            );

        gcmSETFILLER(
            reserve, memory
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Trigger. */
    gcmONERROR(_3DBlitExecute(Hardware, gcv3DBLIT_420TILER, &info));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

#endif

