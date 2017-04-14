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
#include "gc_hal_user.h"
#include "gc_hal_user_brush.h"

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE

/* Multi-sample grid selection:
**
**  0   Square grid only.
**  1   Rotated diamonds but no jitter.
**  2   Rotated diamonds in a 2x2 jitter matrix.
*/
#define gcdGRID_QUALITY 1

#if gcdUSE_HARDWARE_CONFIGURATION_TABLES
#include "gc_hal_user_hardware_config.h"

gcsHARDWARE_CONFIG *    gcConfig   = gcvNULL;
gctBOOL *               gcFeatures = gcvNULL;
gctBOOL *               gcSWWAs    = gcvNULL;

gcsHARDWARE_CONFIG      _gcConfig;
gctBOOL                 _gcFeatures[gcvFEATURE_COUNT];
gctBOOL                 _gcSWWAs[gcvSWWA_COUNT];
#endif

/******************************************************************************\
********************************* Support Code *********************************
\******************************************************************************/

#define gcvINVALID_VALUE 0xCCCCCCCC

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
#define gcmTILE_OFFSET_X(X, Y, SuperTiled, SuperTileMode) \
        ((SuperTiled) ? \
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
                    (((X) & ~0x3F) << 6)   \
        : \
            ((X &  0x03) << 0) | \
            ((Y &  0x03) << 2) | \
            ((X & ~0x03) << 2) \
            )

#define gcmTILE_OFFSET_Y(X, Y, SuperTiled) \
    ((SuperTiled) ? ((Y) & ~0x3F) : ((Y) & ~0x03))

#if (gcdENABLE_3D || gcdENABLE_2D)
static gceSTATUS _ResetDelta(
    IN gcsSTATE_DELTA_PTR StateDelta
    )
{
    /* The delta should not be attached to any context. */
    gcmASSERT(StateDelta->refCount == 0);

    /* Not attached yet, advance the ID. */
    StateDelta->id += 1;

    /* Did ID overflow? */
    if (StateDelta->id == 0)
    {
        /* Reset the map to avoid erroneous ID matches. */
        gcoOS_ZeroMemory(gcmUINT64_TO_PTR(StateDelta->mapEntryID), StateDelta->mapEntryIDSize);

        /* Increment the main ID to avoid matches after reset. */
        StateDelta->id += 1;
    }

    /* Reset the vertex element count. */
    StateDelta->elementCount = 0;

    /* Reset the record count. */
    StateDelta->recordCount = 0;

    /* Success. */
    return gcvSTATUS_OK;
}

static gceSTATUS _MergeDelta(
    IN gcsSTATE_DELTA_PTR StateDelta
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSTATE_DELTA_PTR prevDelta;
    gcsSTATE_DELTA_RECORD_PTR record;
    gctUINT i, count;

    /* Get the record count. */
    count = StateDelta->recordCount;

    /* Set the first record. */
    record = gcmUINT64_TO_PTR(StateDelta->recordArray);

    /* Get the previous delta. */
    prevDelta = gcmUINT64_TO_PTR(StateDelta->prev);

    /* Go through all records. */
    for (i = 0; i < count; i += 1)
    {
        /* Update the delta. */
        gcoHARDWARE_UpdateDelta(
            prevDelta, record->address, record->mask, record->data
            );

        /* Advance to the next state. */
        record += 1;
    }

    /* Update the element count. */
    if (StateDelta->elementCount != 0)
    {
        prevDelta->elementCount = StateDelta->elementCount;
    }

    /* Return the status. */
    return status;
}
#endif


#if (gcdENABLE_3D || gcdENABLE_2D)
static gceSTATUS _LoadStates(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctBOOL FixedPoint,
    IN gctUINT32 Count,
    IN gctUINT32 Mask,
    IN gctPOINTER Data
    )
{
    gceSTATUS status;
    gctUINT32_PTR source;
    gctUINT i;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Address=0x%x Count=%d Data=0x%x",
                  Hardware, Address, Count, Data);

    /* Verify the arguments. */
    gcmGETHARDWARE(Hardware);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Check address range. */
    gcmASSERT(Address + Count < Hardware->stateCount);

    /* Cast the pointers. */
    source = (gctUINT32_PTR) Data;

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + Count) * gcmSIZEOF(gctUINT32), 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)Count  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, Address, Count );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (FixedPoint) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (Count ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

    for (i = 0; i < Count; i ++)
    {
        gcmSETSTATEDATAWITHMASK(
            stateDelta, reserve, memory, FixedPoint, Address + i, Mask,
            *source++
            );
    }

    if ((Count & 1) == 0)
    {
        gcmSETFILLER(
            reserve, memory
            );
    }

    gcmENDSTATEBATCH(
        reserve, memory
        );

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*
 * Support loading more than 1024 states in a batch.
 */
static gceSTATUS _LoadStatesEx(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctBOOL FixedPoint,
    IN gctUINT32 Count,
    IN gctUINT32 Mask,
    IN gctPOINTER Data
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 i = 0;
    gctUINT32 count;
    gctUINT32 limit;

    gcmHEADER_ARG("Hardware=0x%x, Address=%x Count=%d Data=0x%x",
                  Hardware, Address, Count, Data);

    limit = 2 << (25 - 16);

    while (Count)
    {
        count = gcmMIN(Count, limit);

        gcmONERROR(
            _LoadStates(Hardware,
                        Address + i,
                        FixedPoint,
                        count,
                        Mask,
                        (gctUINT8_PTR)Data + i));

        i += count;
        Count -= count;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gctUINT32 _SetBitWidth(
    gctUINT32 Value,
    gctINT8 CurWidth,
    gctINT8 NewWidth
    )
{
    gctUINT32 result;
    gctINT8 widthDiff;

    /* Mask source bits. */
    Value &= ((gctUINT64) 1 << CurWidth) - 1;

    /* Init the result. */
    result = Value;

    /* Determine the difference in width. */
    widthDiff = NewWidth - CurWidth;

    /* Until the difference is not zero... */
    while (widthDiff)
    {
        /* New value is thiner then current? */
        if (widthDiff < 0)
        {
            result >>= -widthDiff;
            widthDiff = 0;
        }

        /* Full source replication? */
        else if (widthDiff >= CurWidth)
        {
            result = (CurWidth == 32) ? Value
                                      : ((result << CurWidth) | Value);
            widthDiff -= CurWidth;
        }

        /* Partial source replication. */
        else
        {
            result = (result << widthDiff) | (Value >> (CurWidth - widthDiff));
            widthDiff = 0;
        }
    }

    /* Return result. */
    return result;
}

static gceSTATUS
_ConvertComponent(
    gctUINT8* SrcPixel,
    gctUINT8* DstPixel,
    gctUINT SrcBit,
    gctUINT DstBit,
    gcsFORMAT_COMPONENT* SrcComponent,
    gcsFORMAT_COMPONENT* DstComponent,
    gcsBOUNDARY_PTR SrcBoundary,
    gcsBOUNDARY_PTR DstBoundary,
    gctUINT32 Default
    )
{
    gctUINT32 srcValue;
    gctUINT8 srcWidth;
    gctUINT8 dstWidth;
    gctUINT32 dstMask;
    gctUINT32 bits;

    /* Exit if target is beyond the boundary. */
    if ((DstBoundary != gcvNULL) &&
        ((DstBoundary->x < 0) || (DstBoundary->x >= DstBoundary->width) ||
         (DstBoundary->y < 0) || (DstBoundary->y >= DstBoundary->height)))
    {
        return gcvSTATUS_SKIP;
    }

    /* Exit if target component is not present. */
    if (DstComponent->width == gcvCOMPONENT_NOTPRESENT)
    {
        return gcvSTATUS_SKIP;
    }

    /* Extract target width. */
    dstWidth = DstComponent->width & gcvCOMPONENT_WIDTHMASK;

    /* Extract the source. */
    if ((SrcComponent == gcvNULL) ||
        (SrcComponent->width == gcvCOMPONENT_NOTPRESENT) ||
        (SrcComponent->width &  gcvCOMPONENT_DONTCARE)   ||
        ((SrcBoundary != gcvNULL) &&
         ((SrcBoundary->x < 0) || (SrcBoundary->x >= SrcBoundary->width) ||
          (SrcBoundary->y < 0) || (SrcBoundary->y >= SrcBoundary->height))))
    {
        srcValue = Default;
        srcWidth = 32;
    }
    else
    {
        /* Extract source width. */
        srcWidth = SrcComponent->width & gcvCOMPONENT_WIDTHMASK;

        /* Compute source position. */
        SrcBit += SrcComponent->start;
        SrcPixel += SrcBit >> 3;
        SrcBit &= 7;

        /* Compute number of bits to read from source. */
        bits = SrcBit + srcWidth;

        /* Read the value. */
        srcValue = SrcPixel[0] >> SrcBit;

        if (bits > 8)
        {
            /* Read up to 16 bits. */
            srcValue |= SrcPixel[1] << (8 - SrcBit);
        }

        if (bits > 16)
        {
            /* Read up to 24 bits. */
            srcValue |= SrcPixel[2] << (16 - SrcBit);
        }

        if (bits > 24)
        {
            /* Read up to 32 bits. */
            srcValue |= SrcPixel[3] << (24 - SrcBit);
        }
    }

    /* Make the source component the same width as the target. */
    srcValue = _SetBitWidth(srcValue, srcWidth, dstWidth);

    /* Compute destination position. */
    DstBit += DstComponent->start;
    DstPixel += DstBit >> 3;
    DstBit &= 7;

    /* Determine the target mask. */
    dstMask = (gctUINT32) (((gctUINT64) 1 << dstWidth) - 1);
    dstMask <<= DstBit;

    /* Align the source value. */
    srcValue <<= DstBit;

    /* Loop while there are bits to set. */
    while (dstMask != 0)
    {
        /* Set 8 bits of the pixel value. */
        if ((dstMask & 0xFF) == 0xFF)
        {
            /* Set all 8 bits. */
            *DstPixel = (gctUINT8) srcValue;
        }
        else
        {
            /* Set the required bits. */
            *DstPixel = (gctUINT8) ((*DstPixel & ~dstMask) | srcValue);
        }

        /* Next 8 bits. */
        DstPixel ++;
        dstMask  >>= 8;
        srcValue >>= 8;
    }

    return gcvSTATUS_OK;
}
#endif  /* gcdENABLE_3D/2D */

#if gcdENABLE_3D
static gctUINT32 _Average2Colors(
    gctUINT32 Color1,
    gctUINT32 Color2
    )
{
    gctUINT32 byte102 =  Color1 & 0x00FF00FF;
    gctUINT32 byte113 =  (Color1 & 0xFF00FF00) >> 1;

    gctUINT32 byte202 =  Color2 & 0x00FF00FF;
    gctUINT32 byte213 =  (Color2 & 0xFF00FF00) >> 1;

    gctUINT32 sum02 = (byte102 + byte202) >> 1;
    gctUINT32 sum13 = (byte113 + byte213);

    gctUINT32 average
        = (sum02 & 0x00FF00FF)
        | (sum13 & 0xFF00FF00);

    return average;
}

static gctUINT32 _Average4Colors(
    gctUINT32 Color1,
    gctUINT32 Color2,
    gctUINT32 Color3,
    gctUINT32 Color4
    )
{
    gctUINT32 byte102 =  Color1 & 0x00FF00FF;
    gctUINT32 byte113 = (Color1 & 0xFF00FF00) >> 2;

    gctUINT32 byte202 =  Color2 & 0x00FF00FF;
    gctUINT32 byte213 = (Color2 & 0xFF00FF00) >> 2;

    gctUINT32 byte302 =  Color3 & 0x00FF00FF;
    gctUINT32 byte313 = (Color3 & 0xFF00FF00) >> 2;

    gctUINT32 byte402 =  Color4 & 0x00FF00FF;
    gctUINT32 byte413 = (Color4 & 0xFF00FF00) >> 2;

    gctUINT32 sum02 = (byte102 + byte202 + byte302 + byte402) >> 2;
    gctUINT32 sum13 = (byte113 + byte213 + byte313 + byte413);

    gctUINT32 average
        = (sum02 & 0x00FF00FF)
        | (sum13 & 0xFF00FF00);

    return average;
}

gceSTATUS
_DisableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gceTILESTATUS_TYPE Type
    )
{
    gceSTATUS status;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    /* Determine the size of the buffer to reserve. */
    /* FLUSH_HACK */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 3, 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    if (Type == gcvTILESTATUS_DEPTH)
    {
        /* Flush the depth cache. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

        /* FLUSH_HACK */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

        /* Disable depth tile status. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Make sure auto-disable is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        /* Make sure compression is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));

        /* Make sure hierarchical turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));
    }
    else
    {
        /* Flush the color cache. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );gcmENDSTATEBATCH(reserve, memory);};

        /* FLUSH_HACK */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

        /* Disable color tile status. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

        /* Make sure auto-disable is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));

        /* Make sure compression is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));
    }

    /* Program memory configuration register. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, Hardware->memoryConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

     /* Make sure raster won't start reading Z values until tile status is
     ** actually disabled. */
     gcmONERROR(gcoHARDWARE_Semaphore(
         Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE, gcvNULL
         ));


OnError:
    return status;
}
gceSTATUS
_DisableTileStatusMRT(
    IN gcoHARDWARE Hardware,
    IN gceTILESTATUS_TYPE Type,
    IN gctUINT RtIndex
    )
{
    gceSTATUS status;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    /* Determine the size of the buffer to reserve. */
    /* FLUSH_HACK */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 3, 8);

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    gcmASSERT (Type == gcvTILESTATUS_COLOR);

    /* Flush the color cache. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );gcmENDSTATEBATCH(reserve, memory);};

    /* FLUSH_HACK */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

    /* Disable color tile status. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

    /* Make sure auto-disable is turned off as well. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

    /* Make sure compression is turned off as well. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)));
    /*
    ** 0x05E8 slot 0 is useless on HW.
    */
    if (RtIndex == 0)
    {
        /* Disable color tile status. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

        /* Make sure auto-disable is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));

        /* Make sure compression is turned off as well. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));
        /* Program memory configuration register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, Hardware->memoryConfig );     gcmENDSTATEBATCH(reserve, memory);};

    }
    else
    {
        /* Program memory configuration register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05E8 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05E8 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05E8 + RtIndex, Hardware->memoryConfigMRT[RtIndex] );     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

     /* Make sure raster won't start reading Z values until tile status is
     ** actually disabled. */
     gcmONERROR(gcoHARDWARE_Semaphore(
         Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE, gcvNULL
         ));


OnError:
    return status;
}

gceSTATUS
_DestroyTempRT(
    IN gcoHARDWARE Hardware)
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Is there a surface allocated? */
    if (Hardware->tempRT.node.pool != gcvPOOL_UNKNOWN)
    {
        /* Unlock the surface. */
        gcmONERROR(gcoHARDWARE_Unlock(&Hardware->tempRT.node,
                               Hardware->tempRT.type));

        gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&Hardware->tempRT.node));

        /* Reset the temporary surface. */
        gcoOS_ZeroMemory(&Hardware->tempRT, sizeof(Hardware->tempRT));

        Hardware->tempRT.node.pool = gcvPOOL_UNKNOWN;
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
_ResizeTempRT(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR dSurface
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_INFO_PTR tempRT = &Hardware->tempRT;

    gcmHEADER_ARG("Hardware=0x%x dSurface=0x%x", Hardware, dSurface);

    if (!dSurface)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (tempRT->alignedWidth < dSurface->alignedWidth)
    {
        gctUINT width;
        const gceSURF_FORMAT format = gcvSURF_A8R8G8B8;
        gcsSURF_FORMAT_INFO_PTR formatInfo;


        /* Destroy temp RT if ever allocated */
        gcmONERROR(_DestroyTempRT(Hardware));


        /* Recreate new temp RT */
        gcmONERROR(gcoHARDWARE_QueryFormat(format, &formatInfo));

        tempRT->rect.left       = 0;
        tempRT->rect.top        = 0;
        tempRT->rect.right      = dSurface->rect.right;
        tempRT->rect.bottom     = 4 * 2;
        tempRT->samples         = dSurface->samples;
        tempRT->type            = gcvSURF_RENDER_TARGET_NO_TILE_STATUS;
        tempRT->format          = format;
        tempRT->formatInfo      = *formatInfo;
        tempRT->is16Bit         = gcvFALSE;
        tempRT->tiling          = gcvTILED;
        tempRT->superTiled      = 0;

        width = gcoMATH_DivideUInt(tempRT->rect.right, tempRT->samples.x);
        width = gcmALIGN_NP2(width, 4);

        tempRT->alignedWidth    = width * tempRT->samples.x;
        tempRT->alignedHeight   = tempRT->rect.bottom;
        tempRT->stride          = tempRT->alignedWidth * formatInfo->bitsPerPixel / 8;
        tempRT->size            = tempRT->stride * tempRT->alignedHeight;

        gcmONERROR(gcsSURF_NODE_Construct(
            &tempRT->node,
            tempRT->size,
            256,
            tempRT->type,
            gcvALLOC_FLAG_NONE,
            gcvPOOL_DEFAULT
            ));

        gcmONERROR(gcoHARDWARE_Lock(&tempRT->node, gcvNULL, gcvNULL));
    }
    else
    {
        status = gcvSTATUS_CACHED;
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
#if !gcdSTATIC_LINK

#if defined(__APPLE__)
static const char * vscDll = "libVSC.dylib";
#elif defined(WIN32)
static const char * vscDll = "libVSC";
#else
static const char * vscDll = "libVSC.so";
#endif

#if defined(__APPLE__)
static const char * glslcDll = "libGLSLC.dylib";
#elif defined(WIN32)
static const char * glslcDll = "libGLSLC";
#else
static const char * glslcDll = "libGLSLC.so";
#endif


static gceSTATUS _DestroyBlitDraw(
    gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw;
    gcsVSC_API *vscAPI;
    gctINT32 i, j;

    if (!Hardware->threadDefault)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    blitDraw = Hardware->blitDraw;

    if (!blitDraw)
    {
        return gcvSTATUS_OK;
    }

    vscAPI = &blitDraw->vscAPI;

    for (i = 0; i < gcvBLITDRAW_NUM_TYPE; i++)
    {
        for (j = 0; j < gcmMAX_VARIATION_NUM; j++)
        {
            gcsPROGRAM_STATE_VARIATION *temp = &blitDraw->programState[i][j];
            if (temp->programState.hints)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.hints);
                temp->programState.hints = gcvNULL;
            }

            if (temp->programState.programBuffer)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.programBuffer);
                temp->programState.programBuffer = gcvNULL;
            }
        }

        if (blitDraw->psShader[i])
        {
            (*vscAPI->gcSHADER_Destroy)(blitDraw->psShader[i]);
            blitDraw->psShader[i] = gcvNULL;
        }

        if (blitDraw->vsShader[i])
        {
            (*vscAPI->gcSHADER_Destroy)(blitDraw->vsShader[i]);
            blitDraw->vsShader[i] = gcvNULL;
        }
    }

    if (blitDraw->dynamicStream != gcvNULL)
    {
        /* Destroy the dynamic stream. */
        gcmVERIFY_OK(gcoSTREAM_Destroy(blitDraw->dynamicStream));
        blitDraw->dynamicStream = gcvNULL;
    }

    if (blitDraw->vscLib)
    {
        gcmVERIFY_OK(gcoOS_FreeLibrary(gcvNULL, blitDraw->vscLib));
    }

    if (blitDraw->glslcLib)
    {
        gcmVERIFY_OK(gcoOS_FreeLibrary(gcvNULL, blitDraw->glslcLib));
    }

    gcmOS_SAFE_FREE(gcvNULL, blitDraw);

    Hardware->blitDraw = gcvNULL;



    return gcvSTATUS_OK;

 OnError:

    return status;

}

static gceSTATUS _InitBlitProgram(
    gcoHARDWARE Hardware,
    gceBLITDRAW_TYPE type,
    gctBOOL NeedRecompile
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw = gcvNULL;
    gcsVSC_API *vscAPI;
    gctUINT32 compilerVersion = NeedRecompile ? gcmCC('\0', '\0', '\0', '\3') : gcmCC('\0', '\0', '\1', '\1');
    gctUINT32 vsShaderVersion[2] = {(gcmCC('E', 'S', '\0', '\0') |  gcSHADER_TYPE_VERTEX << 16), compilerVersion};
    gctUINT32 psShaderVersion[2] = {(gcmCC('E', 'S', '\0', '\0') |  gcSHADER_TYPE_FRAGMENT << 16), compilerVersion};
    const gctSTRING outputColorName = NeedRecompile ? "Color" : "#Color";

    gcmASSERT(Hardware->blitDraw);

    blitDraw = Hardware->blitDraw;

    vscAPI = &blitDraw->vscAPI;

    /* Initial blit program */
    if ((type == gcvBLITDRAW_BLIT) && (blitDraw->vsShader[gcvBLITDRAW_BLIT] == gcvNULL))
    {
        gcmONERROR((*vscAPI->gcSHADER_Construct)(gcvNULL, gcSHADER_TYPE_VERTEX, &blitDraw->vsShader[gcvBLITDRAW_BLIT]));
        do
        {
            gcATTRIBUTE pos;
            gcATTRIBUTE texCoord;

            gcSHADER blitVSShader = blitDraw->vsShader[gcvBLITDRAW_BLIT];
            gcmONERROR((*vscAPI->gcSHADER_AddAttribute)(blitVSShader, "in_position", gcSHADER_FLOAT_X3, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &pos));
            gcmONERROR((*vscAPI->gcSHADER_AddAttribute)(blitVSShader, "in_texCoord", gcSHADER_FLOAT_X2, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &texCoord));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(blitVSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZ, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(blitVSShader, pos, gcSL_SWIZZLE_XYZZ, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(blitVSShader, gcSL_MOV, 1, gcSL_ENABLE_W, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceConstant)(blitVSShader, 1.0));
            gcmONERROR((*vscAPI->gcSHADER_AddOutput)(blitVSShader, "#Position", gcSHADER_FLOAT_X4, 1, 1));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(blitVSShader, gcSL_MOV, 2, gcSL_ENABLE_XY, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(blitVSShader, texCoord, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddOutput)(blitVSShader, "vTexCoord", gcSHADER_FLOAT_X2, 1, 2));
            gcmONERROR((*vscAPI->gcSHADER_Pack)(blitVSShader));

            gcmONERROR((*vscAPI->gcSHADER_SetCompilerVersion)(blitVSShader, vsShaderVersion));
        } while(gcvFALSE);
    }

    if ((type == gcvBLITDRAW_BLIT) && (blitDraw->psShader[gcvBLITDRAW_BLIT] == gcvNULL))
    {
        gcATTRIBUTE texcoord0;

        gcmONERROR((*vscAPI->gcSHADER_Construct)(gcvNULL, gcSHADER_TYPE_FRAGMENT, &blitDraw->psShader[gcvBLITDRAW_BLIT]));
        do
        {
            gcSHADER blitPSShader = blitDraw->psShader[gcvBLITDRAW_BLIT];
            gcmONERROR((*vscAPI->gcSHADER_AddAttribute)(blitPSShader, "vTexCoord", gcSHADER_FLOAT_X2, 1, gcvTRUE, gcSHADER_SHADER_DEFAULT, &texcoord0));
            gcmONERROR((*vscAPI->gcSHADER_AddUniform)(blitPSShader, "unit0", gcSHADER_SAMPLER_2D, 1, &blitDraw->bliterSampler));

            gcmONERROR((*vscAPI->gcSHADER_AddOpcodeConditional)(blitPSShader, gcSL_KILL, gcSL_LESS, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceConstant)(blitPSShader, 0.0f));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcodeConditional)(blitPSShader, gcSL_KILL, gcSL_GREATER, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceConstant)(blitPSShader, 1.0f));

            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(blitPSShader, gcSL_TEXLD, 1, gcSL_ENABLE_XYZW, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceUniform)(blitPSShader, blitDraw->bliterSampler, gcSL_SWIZZLE_XYZW, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddOutput)(blitPSShader, outputColorName, gcSHADER_FLOAT_X4, 1, 1));
            if (NeedRecompile)
            {
                gcmONERROR((*vscAPI->gcSHADER_AddLocation)(blitPSShader, 0, 1));
            }
            gcmONERROR((*vscAPI->gcSHADER_Pack)(blitPSShader));

            gcmONERROR((*vscAPI->gcSHADER_SetCompilerVersion)(blitPSShader, psShaderVersion));
        } while(gcvFALSE);
    }

    /* Initial clear program */
    if ((type == gcvBLITDRAW_CLEAR) && (blitDraw->vsShader[gcvBLITDRAW_CLEAR] == gcvNULL))
    {
        gcmONERROR((*vscAPI->gcSHADER_Construct)(gcvNULL, gcSHADER_TYPE_VERTEX, &blitDraw->vsShader[gcvBLITDRAW_CLEAR]));
        do
        {
            gcATTRIBUTE pos;
            gcSHADER clearVSShader = blitDraw->vsShader[gcvBLITDRAW_CLEAR];

            gcmONERROR((*vscAPI->gcSHADER_AddAttribute)(clearVSShader, "in_position", gcSHADER_FLOAT_X3, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &pos));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(clearVSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZ, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceAttribute)(clearVSShader, pos, gcSL_SWIZZLE_XYZZ, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(clearVSShader, gcSL_MOV, 1, gcSL_ENABLE_W, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceConstant)(clearVSShader, 1.0));
            gcmONERROR((*vscAPI->gcSHADER_AddOutput)(clearVSShader, "#Position", gcSHADER_FLOAT_X4, 1, 1));
            gcmONERROR((*vscAPI->gcSHADER_Pack)(clearVSShader));

            gcmONERROR((*vscAPI->gcSHADER_SetCompilerVersion)(clearVSShader, vsShaderVersion));
        } while(gcvFALSE);
    }

    if ((type == gcvBLITDRAW_CLEAR) && (blitDraw->psShader[gcvBLITDRAW_CLEAR] == gcvNULL))
    {
        gcmONERROR((*vscAPI->gcSHADER_Construct)(gcvNULL, gcSHADER_TYPE_FRAGMENT, &blitDraw->psShader[gcvBLITDRAW_CLEAR]));
        do
        {
            gcSHADER clearPSShader = blitDraw->psShader[gcvBLITDRAW_CLEAR];
            gcmONERROR((*vscAPI->gcSHADER_AddUniform)(clearPSShader, "uColor", gcSHADER_FLOAT_X4, 1, &blitDraw->clearColorUnfiorm));
            gcmONERROR((*vscAPI->gcSHADER_AddOpcode)(clearPSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZW, gcSL_FLOAT));
            gcmONERROR((*vscAPI->gcSHADER_AddSourceUniform)(clearPSShader, blitDraw->clearColorUnfiorm, gcSL_SWIZZLE_XYZW, 0));
            gcmONERROR((*vscAPI->gcSHADER_AddOutput)(clearPSShader, outputColorName, gcSHADER_FLOAT_X4, 1, 1));
            if (NeedRecompile)
            {
                gcmONERROR((*vscAPI->gcSHADER_AddLocation)(clearPSShader, 0, 1));
            }
            gcmONERROR((*vscAPI->gcSHADER_Pack)(clearPSShader));

            gcmONERROR((*vscAPI->gcSHADER_SetCompilerVersion)(clearPSShader, psShaderVersion));
        } while(gcvFALSE);
    }

    return status;

OnError:

    return status;

}

static gceSTATUS _InitBlitDraw(
    gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER pointer = gcvNULL;
    gcsHARDWARE_BLITDRAW_PTR blitDraw = gcvNULL;
    gcsVSC_API *vscAPI;

    if (!Hardware->threadDefault)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (!Hardware->blitDraw)
    {
        gcmONERROR(gcoHARDWARE_Initialize3D(Hardware));
        gcmONERROR(gcoHARDWARE_SetAPI(Hardware, gcvAPI_OPENGL_ES30));

        gcmONERROR(gcoOS_Allocate(gcvNULL,
                                  gcmSIZEOF(gcsHARDWARE_BLITDRAW),
                                  &pointer));

        blitDraw = (gcsHARDWARE_BLITDRAW *)pointer;
        gcoOS_ZeroMemory(pointer, gcmSIZEOF(gcsHARDWARE_BLITDRAW));

        /* Construct a cacheable stream. */
        gcmONERROR(gcoSTREAM_Construct(gcvNULL, &blitDraw->dynamicStream));

        vscAPI = &blitDraw->vscAPI;

        if (gcmIS_ERROR(gcoOS_LoadLibrary(gcvNULL, vscDll, &blitDraw->vscLib)))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        if (gcmIS_ERROR(gcoOS_LoadLibrary(gcvNULL, glslcDll, &blitDraw->glslcLib)))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->glslcLib,
                                        "gcCompileShader",
                                        (gctPOINTER*) &vscAPI->gcCompileShader));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSetGLSLCompiler",
                                        (gctPOINTER*) &vscAPI->gcSetGLSLCompiler));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_Construct",
                                        (gctPOINTER*) &vscAPI->gcSHADER_Construct));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcLinkShaders",
                                        (gctPOINTER*) &vscAPI->gcLinkShaders));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddAttribute",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddAttribute));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddUniform",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddUniform));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddOpcode",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddOpcode));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddOpcodeConditional",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddOpcodeConditional));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddSourceUniform",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddSourceUniform));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddSourceAttribute",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddSourceAttribute));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddSourceConstant",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddSourceConstant));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddOutput",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddOutput));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_AddLocation",
                                        (gctPOINTER*) &vscAPI->gcSHADER_AddLocation));


        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_SetCompilerVersion",
                                        (gctPOINTER*) &vscAPI->gcSHADER_SetCompilerVersion));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_Pack",
                                        (gctPOINTER*) &vscAPI->gcSHADER_Pack));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_Destroy",
                                        (gctPOINTER*) &vscAPI->gcSHADER_Destroy));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_Copy",
                                        (gctPOINTER*) &vscAPI->gcSHADER_Copy));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcSHADER_DynamicPatch",
                                        (gctPOINTER*) &vscAPI->gcSHADER_DynamicPatch));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcCreateOutputConversionDirective",
                                        (gctPOINTER*) &vscAPI->gcCreateOutputConversionDirective));

        gcmONERROR(gcoOS_GetProcAddress(gcvNULL,
                                        blitDraw->vscLib,
                                        "gcCreateInputConversionDirective",
                                        (gctPOINTER*) &vscAPI->gcCreateInputConversionDirective));

        Hardware->blitDraw = blitDraw;
    }

    gcmASSERT(Hardware->blitDraw);

    return status;

OnError:

    gcmVERIFY_OK(_DestroyBlitDraw(Hardware));

    return status;

}


gceSTATUS
_PickBlitDrawShader(
    gcoHARDWARE Hardware,
    gceBLITDRAW_TYPE type,
    gcsSURF_FORMAT_INFO_PTR srcFmtInfo,
    gcsSURF_FORMAT_INFO_PTR dstFmtInfo,
    gcsPROGRAM_STATE ** programState
    )
{

    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw = Hardware->blitDraw;
    gcsVSC_API *vscAPI;
    gceSURF_FORMAT requestSrcFmt = gcvSURF_UNKNOWN;
    gceSURF_FORMAT requestDstFmt = gcvSURF_UNKNOWN;
    gctINT32 i;
    gcsPROGRAM_STATE_VARIATION *variation = gcvNULL;

    if (srcFmtInfo && srcFmtInfo->fakedFormat &&
        srcFmtInfo->format != gcvSURF_R8_1_X8R8G8B8 &&
        srcFmtInfo->format != gcvSURF_G8R8_1_X8R8G8B8
       )
    {
        requestSrcFmt = srcFmtInfo->format;
    }

    if (dstFmtInfo && dstFmtInfo->fakedFormat)
    {
        requestDstFmt = dstFmtInfo->format;
    }

    gcmASSERT(blitDraw);

    vscAPI = &blitDraw->vscAPI;

    for (i = 0; i < gcmMAX_VARIATION_NUM; i++)
    {
        gcsPROGRAM_STATE_VARIATION *var = &blitDraw->programState[type][i];

        if ((var->srcFmt == requestSrcFmt) &&
            (var->destFmt == requestDstFmt) &&
            (var->programState.programBuffer != gcvNULL))
        {
            variation = var;
            break;
        }
    }

    if (variation)
    {
        /* found one */
        *programState = &variation->programState;
        return gcvSTATUS_OK;
    }
    else
    /* new format pair */
    {
        gcsPROGRAM_STATE_VARIATION *temp = gcvNULL;
        gcPatchDirective *patchDirective = gcvNULL;
        gctBOOL needRecompile = gcvFALSE;

        for (i = 0; i < gcmMAX_VARIATION_NUM; i++)
        {
            temp = &blitDraw->programState[type][i];
            if (temp->programState.programBuffer == gcvNULL)
            {
                break;
            }
        }

        if (i >= gcmMAX_VARIATION_NUM)
        {
            gcmASSERT(temp);
            /* TODO: add LRU support, now just free the last one */
            if (temp->programState.hints)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.hints);
                temp->programState.hints = gcvNULL;
            }

            if (temp->programState.programBuffer)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.programBuffer);
                temp->programState.programBuffer = gcvNULL;
            }
        }

        gcmASSERT(temp->programState.programBuffer == gcvNULL);

        /* start to generate program variation for new format pair
        */
        if(blitDraw->vsShader[type] != gcvNULL)
        {
            gcmONERROR((*vscAPI->gcSHADER_Destroy)(blitDraw->vsShader[type]));
            blitDraw->vsShader[type] = gcvNULL;
        }

        if(blitDraw->psShader[type] != gcvNULL)
        {
            gcmONERROR((*vscAPI->gcSHADER_Destroy)(blitDraw->psShader[type]));
            blitDraw->psShader[type] = gcvNULL;
        }

        if (requestSrcFmt != gcvSURF_UNKNOWN ||
            requestDstFmt != gcvSURF_UNKNOWN)
        {
            needRecompile = gcvTRUE;
        }

        gcmONERROR(_InitBlitProgram(Hardware, type, needRecompile));

        if (requestSrcFmt != gcvSURF_UNKNOWN)
        {
            gceTEXTURE_SWIZZLE baseComponents_rgba[] =
            {
                gcvTEXTURE_SWIZZLE_R,
                gcvTEXTURE_SWIZZLE_G,
                gcvTEXTURE_SWIZZLE_B,
                gcvTEXTURE_SWIZZLE_A
            };

            gctCONST_STRING txName;

            gcmASSERT(type == gcvBLITDRAW_BLIT);
            gcmASSERT(srcFmtInfo);

            gcmONERROR(gcoTEXTURE_GetTextureFormatName(srcFmtInfo, &txName));
            gcmONERROR((*vscAPI->gcCreateInputConversionDirective)(blitDraw->bliterSampler,
                                                                   srcFmtInfo,
                                                                   txName,
                                                                   baseComponents_rgba,
                                                                   &patchDirective));


        }

        if (requestDstFmt != gcvSURF_UNKNOWN)
        {

            gcmASSERT(dstFmtInfo);
            gcmONERROR((*vscAPI->gcCreateOutputConversionDirective)(0, dstFmtInfo, &patchDirective));
        }


        /* patch vertex and fragment shader */
        if (patchDirective)
        {
            (*vscAPI->gcSetGLSLCompiler)(vscAPI->gcCompileShader);
            gcmONERROR((*vscAPI->gcSHADER_DynamicPatch)(blitDraw->vsShader[type], patchDirective));
            gcmONERROR((*vscAPI->gcSHADER_DynamicPatch)(blitDraw->psShader[type], patchDirective));
        }

        gcmONERROR((*vscAPI->gcLinkShaders)(blitDraw->vsShader[type],
                                            blitDraw->psShader[type],
                                            (gceSHADER_FLAGS)
                                            (gcvSHADER_DEAD_CODE            |
                                             gcvSHADER_RESOURCE_USAGE       |
                                             gcvSHADER_OPTIMIZER            |
                                             gcvSHADER_USE_GL_Z             |
                                             gcvSHADER_USE_GL_POINT_COORD   |
                                             gcvSHADER_USE_GL_POSITION      |
                                             gcvSHADER_REMOVE_UNUSED_UNIFORMS),
                                             &temp->programState.programSize,
                                             &temp->programState.programBuffer,
                                             &temp->programState.hints));

        *programState = &temp->programState;
        temp->destFmt = requestDstFmt;
        temp->srcFmt = requestSrcFmt;
        return gcvSTATUS_OK;
    }

OnError:
    return status;


}

#else
static gceSTATUS _DestroyBlitDraw(
    gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw;
    gctINT32 i, j;

    if (!Hardware->threadDefault)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    blitDraw = Hardware->blitDraw;

    if (!blitDraw)
    {
        return gcvSTATUS_OK;
    }

    for (i = 0; i < gcvBLITDRAW_NUM_TYPE; i++)
    {
        for (j = 0; j < gcmMAX_VARIATION_NUM; j++)
        {
            gcsPROGRAM_STATE_VARIATION *temp = &blitDraw->programState[i][j];
            if (temp->programState.hints)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.hints);
                temp->programState.hints = gcvNULL;
            }

            if (temp->programState.programBuffer)
            {
                gcmOS_SAFE_FREE(gcvNULL, temp->programState.programBuffer);
                temp->programState.programBuffer = gcvNULL;
            }
        }

        if (blitDraw->psShader[i])
        {
            gcSHADER_Destroy(blitDraw->psShader[i]);
            blitDraw->psShader[i] = gcvNULL;
        }

        if (blitDraw->vsShader[i])
        {
            gcSHADER_Destroy(blitDraw->vsShader[i]);
            blitDraw->vsShader[i] = gcvNULL;
        }
    }

    if (blitDraw->dynamicStream != gcvNULL)
    {
        /* Destroy the dynamic stream. */
        gcmVERIFY_OK(gcoSTREAM_Destroy(blitDraw->dynamicStream));
        blitDraw->dynamicStream = gcvNULL;
    }

    gcmOS_SAFE_FREE(gcvNULL, blitDraw);

    Hardware->blitDraw = gcvNULL;



    return gcvSTATUS_OK;

 OnError:

    return status;

}

static gceSTATUS _InitBlitProgram(
    gcoHARDWARE Hardware,
    gceBLITDRAW_TYPE type,
    gctBOOL NeedRecompile
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw = gcvNULL;
    gctUINT32 compilerVersion = NeedRecompile ? gcmCC('\0', '\0', '\0', '\3') : gcmCC('\0', '\0', '\1', '\1');
    gctUINT32 vsShaderVersion[2] = {(gcmCC('E', 'S', '\0', '\0') |  gcSHADER_TYPE_VERTEX << 16), compilerVersion};
    gctUINT32 psShaderVersion[2] = {(gcmCC('E', 'S', '\0', '\0') |  gcSHADER_TYPE_FRAGMENT << 16), compilerVersion};
    const gctSTRING outputColorName = NeedRecompile ? "Color" : "#Color";

    gcmASSERT(Hardware->blitDraw);

    blitDraw = Hardware->blitDraw;

    /* Initial blit program */
    if ((type == gcvBLITDRAW_BLIT) && (blitDraw->vsShader[gcvBLITDRAW_BLIT] == gcvNULL))
    {
        gcmONERROR(gcSHADER_Construct(gcvNULL, gcSHADER_TYPE_VERTEX, &blitDraw->vsShader[gcvBLITDRAW_BLIT]));
        do
        {
            gcATTRIBUTE pos;
            gcATTRIBUTE texCoord;

            gcSHADER blitVSShader = blitDraw->vsShader[gcvBLITDRAW_BLIT];
            gcmONERROR(gcSHADER_AddAttribute(blitVSShader, "in_position", gcSHADER_FLOAT_X3, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &pos));
            gcmONERROR(gcSHADER_AddAttribute(blitVSShader, "in_texCoord", gcSHADER_FLOAT_X2, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &texCoord));
            gcmONERROR(gcSHADER_AddOpcode(blitVSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZ, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceAttribute(blitVSShader, pos, gcSL_SWIZZLE_XYZZ, 0));
            gcmONERROR(gcSHADER_AddOpcode(blitVSShader, gcSL_MOV, 1, gcSL_ENABLE_W, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceConstant(blitVSShader, 1.0));
            gcmONERROR(gcSHADER_AddOutput(blitVSShader, "#Position", gcSHADER_FLOAT_X4, 1, 1));
            gcmONERROR(gcSHADER_AddOpcode(blitVSShader, gcSL_MOV, 2, gcSL_ENABLE_XY, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceAttribute(blitVSShader, texCoord, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR(gcSHADER_AddOutput(blitVSShader, "vTexCoord", gcSHADER_FLOAT_X2, 1, 2));
            gcmONERROR(gcSHADER_Pack(blitVSShader));

            gcmONERROR(gcSHADER_SetCompilerVersion(blitVSShader, vsShaderVersion));
        } while(gcvFALSE);
    }

    if ((type == gcvBLITDRAW_BLIT) && (blitDraw->psShader[gcvBLITDRAW_BLIT] == gcvNULL))
    {
        gcATTRIBUTE texcoord0;

        gcmONERROR(gcSHADER_Construct(gcvNULL, gcSHADER_TYPE_FRAGMENT, &blitDraw->psShader[gcvBLITDRAW_BLIT]));
        do
        {
            gcSHADER blitPSShader = blitDraw->psShader[gcvBLITDRAW_BLIT];
            gcmONERROR(gcSHADER_AddAttribute(blitPSShader, "vTexCoord", gcSHADER_FLOAT_X2, 1, gcvTRUE, gcSHADER_SHADER_DEFAULT, &texcoord0));
            gcmONERROR(gcSHADER_AddUniform(blitPSShader, "unit0", gcSHADER_SAMPLER_2D, 1, &blitDraw->bliterSampler));

            gcmONERROR(gcSHADER_AddOpcodeConditional(blitPSShader, gcSL_KILL, gcSL_LESS, 0));
            gcmONERROR(gcSHADER_AddSourceAttribute(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR(gcSHADER_AddSourceConstant(blitPSShader, 0.0f));
            gcmONERROR(gcSHADER_AddOpcodeConditional(blitPSShader, gcSL_KILL, gcSL_GREATER, 0));
            gcmONERROR(gcSHADER_AddSourceAttribute(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR(gcSHADER_AddSourceConstant(blitPSShader, 1.0f));

            gcmONERROR(gcSHADER_AddOpcode(blitPSShader, gcSL_TEXLD, 1, gcSL_ENABLE_XYZW, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceUniform(blitPSShader, blitDraw->bliterSampler, gcSL_SWIZZLE_XYZW, 0));
            gcmONERROR(gcSHADER_AddSourceAttribute(blitPSShader, texcoord0, gcSL_SWIZZLE_XYYY, 0));
            gcmONERROR(gcSHADER_AddOutput(blitPSShader, outputColorName, gcSHADER_FLOAT_X4, 1, 1));
            if (NeedRecompile)
            {
                gcmONERROR(gcSHADER_AddLocation(blitPSShader, 0, 1));
            }
            gcmONERROR(gcSHADER_Pack(blitPSShader));

            gcmONERROR(gcSHADER_SetCompilerVersion(blitPSShader, psShaderVersion));
        } while(gcvFALSE);
    }

    /* Initial clear program */
    if ((type == gcvBLITDRAW_CLEAR) && (blitDraw->vsShader[gcvBLITDRAW_CLEAR] == gcvNULL))
    {
        gcmONERROR(gcSHADER_Construct(gcvNULL, gcSHADER_TYPE_VERTEX, &blitDraw->vsShader[gcvBLITDRAW_CLEAR]));
        do
        {
            gcATTRIBUTE pos;
            gcSHADER clearVSShader = blitDraw->vsShader[gcvBLITDRAW_CLEAR];

            gcmONERROR(gcSHADER_AddAttribute(clearVSShader, "in_position", gcSHADER_FLOAT_X3, 1, gcvFALSE, gcSHADER_SHADER_DEFAULT, &pos));
            gcmONERROR(gcSHADER_AddOpcode(clearVSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZ, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceAttribute(clearVSShader, pos, gcSL_SWIZZLE_XYZZ, 0));
            gcmONERROR(gcSHADER_AddOpcode(clearVSShader, gcSL_MOV, 1, gcSL_ENABLE_W, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceConstant(clearVSShader, 1.0));
            gcmONERROR(gcSHADER_AddOutput(clearVSShader, "#Position", gcSHADER_FLOAT_X4, 1, 1));
            gcmONERROR(gcSHADER_Pack(clearVSShader));

            gcmONERROR(gcSHADER_SetCompilerVersion(clearVSShader, vsShaderVersion));
        } while(gcvFALSE);
    }

    if ((type == gcvBLITDRAW_CLEAR) && (blitDraw->psShader[gcvBLITDRAW_CLEAR] == gcvNULL))
    {
        gcmONERROR(gcSHADER_Construct(gcvNULL, gcSHADER_TYPE_FRAGMENT, &blitDraw->psShader[gcvBLITDRAW_CLEAR]));
        do
        {
            gcSHADER clearPSShader = blitDraw->psShader[gcvBLITDRAW_CLEAR];
            gcmONERROR(gcSHADER_AddUniform(clearPSShader, "uColor", gcSHADER_FLOAT_X4, 1, &blitDraw->clearColorUnfiorm));
            gcmONERROR(gcSHADER_AddOpcode(clearPSShader, gcSL_MOV, 1, gcSL_ENABLE_XYZW, gcSL_FLOAT));
            gcmONERROR(gcSHADER_AddSourceUniform(clearPSShader, blitDraw->clearColorUnfiorm, gcSL_SWIZZLE_XYZW, 0));
            gcmONERROR(gcSHADER_AddOutput(clearPSShader, outputColorName, gcSHADER_FLOAT_X4, 1, 1));
            if (NeedRecompile)
            {
                gcmONERROR(gcSHADER_AddLocation(clearPSShader, 0, 1));
            }
            gcmONERROR(gcSHADER_Pack(clearPSShader));

            gcmONERROR(gcSHADER_SetCompilerVersion(clearPSShader, psShaderVersion));
        } while(gcvFALSE);
    }

    return status;

OnError:

    return status;

}

static gceSTATUS _InitBlitDraw(
    gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER pointer = gcvNULL;

    if (!Hardware->threadDefault)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (!Hardware->blitDraw)
    {
        gcmONERROR(gcoHARDWARE_Initialize3D(Hardware));
        gcmONERROR(gcoHARDWARE_SetAPI(Hardware, gcvAPI_OPENGL_ES30));

        gcmONERROR(gcoOS_Allocate(gcvNULL,
                                  gcmSIZEOF(gcsHARDWARE_BLITDRAW),
                                  &pointer));

        Hardware->blitDraw = (gcsHARDWARE_BLITDRAW *)pointer;
        gcoOS_ZeroMemory(pointer, gcmSIZEOF(gcsHARDWARE_BLITDRAW));

        /* Construct a cacheable stream. */
        gcmONERROR(gcoSTREAM_Construct(gcvNULL, &Hardware->blitDraw->dynamicStream));
    }

    gcmASSERT(Hardware->blitDraw);

    return status;

OnError:

    gcmVERIFY_OK(_DestroyBlitDraw(Hardware));

    return status;

}


gceSTATUS
_PickBlitDrawShader(
    gcoHARDWARE Hardware,
    gceBLITDRAW_TYPE type,
    gcsSURF_FORMAT_INFO_PTR srcFmtInfo,
    gcsSURF_FORMAT_INFO_PTR dstFmtInfo,
    gcsPROGRAM_STATE ** programState
    )
{

    gceSTATUS status = gcvSTATUS_OK;
    gcsHARDWARE_BLITDRAW_PTR blitDraw = Hardware->blitDraw;
    gceSURF_FORMAT requestSrcFmt = gcvSURF_UNKNOWN;
    gceSURF_FORMAT requestDstFmt = gcvSURF_UNKNOWN;

    if (srcFmtInfo && srcFmtInfo->fakedFormat &&
        srcFmtInfo->format != gcvSURF_R8_1_X8R8G8B8 &&
        srcFmtInfo->format != gcvSURF_G8R8_1_X8R8G8B8
       )
    {
        requestSrcFmt = srcFmtInfo->format;
    }

    if (dstFmtInfo && dstFmtInfo->fakedFormat &&
        dstFmtInfo->format != gcvSURF_R8_1_X8R8G8B8 &&
        dstFmtInfo->format != gcvSURF_G8R8_1_X8R8G8B8
       )
    {
        requestDstFmt = dstFmtInfo->format;
    }


    if (blitDraw)
    {
        gctINT32 i;
        gcsPROGRAM_STATE_VARIATION *variation = gcvNULL;

        for (i = 0; i < gcmMAX_VARIATION_NUM; i++)
        {
            variation = &blitDraw->programState[type][i];
            if ((variation->srcFmt == requestSrcFmt) &&
                (variation->destFmt == requestDstFmt) &&
                (variation->programState.programBuffer != gcvNULL))
            {
                break;
            }
            variation = gcvNULL;
        }

        /* new format pair */
        if (variation == gcvNULL)
        {
            gcsPROGRAM_STATE_VARIATION *temp;
            gcPatchDirective *patchDirective = gcvNULL;
            gctBOOL needRecompile = gcvFALSE;

            for (i = 0; i < gcmMAX_VARIATION_NUM; i++)
            {
                temp = &blitDraw->programState[type][i];
                if (temp->programState.programBuffer == gcvNULL)
                {
                    break;
                }
            }

            if (i >= gcmMAX_VARIATION_NUM)
            {
                /* TODO: add LRU support, now just free the last one */
                if (temp->programState.hints)
                {
                    gcmOS_SAFE_FREE(gcvNULL, temp->programState.hints);
                    temp->programState.hints = gcvNULL;
                }

                if (temp->programState.programBuffer)
                {
                    gcmOS_SAFE_FREE(gcvNULL, temp->programState.programBuffer);
                    temp->programState.programBuffer = gcvNULL;
                }
            }

            gcmASSERT(temp->programState.programBuffer == gcvNULL);

            /* start to generate program variation for new format pair
            */
            if(blitDraw->vsShader[type] != gcvNULL)
            {
                gcmONERROR(gcSHADER_Destroy(blitDraw->vsShader[type]));
                blitDraw->vsShader[type] = gcvNULL;
            }

            if(blitDraw->psShader[type] != gcvNULL)
            {
                gcmONERROR(gcSHADER_Destroy(blitDraw->psShader[type]));
                blitDraw->psShader[type] = gcvNULL;
            }

            if (requestSrcFmt != gcvSURF_UNKNOWN ||
                requestDstFmt != gcvSURF_UNKNOWN)
            {
                needRecompile = gcvTRUE;
            }

            gcmONERROR(_InitBlitProgram(Hardware, type, needRecompile));

            if (requestSrcFmt != gcvSURF_UNKNOWN)
            {
                gceTEXTURE_SWIZZLE baseComponents_rgba[] =
                {
                    gcvTEXTURE_SWIZZLE_R,
                    gcvTEXTURE_SWIZZLE_G,
                    gcvTEXTURE_SWIZZLE_B,
                    gcvTEXTURE_SWIZZLE_A
                };

                gctCONST_STRING txName;

                gcmASSERT(type == gcvBLITDRAW_BLIT);
                gcmASSERT(srcFmtInfo);

                gcmONERROR(gcoTEXTURE_GetTextureFormatName(srcFmtInfo, &txName));
                gcmONERROR(gcCreateInputConversionDirective(blitDraw->bliterSampler,
                                                            srcFmtInfo,
                                                            txName,
                                                            baseComponents_rgba,
                                                            &patchDirective));


            }

            if (requestDstFmt != gcvSURF_UNKNOWN)
            {

                gcmASSERT(dstFmtInfo);
                gcmONERROR(gcCreateOutputConversionDirective(0, dstFmtInfo, &patchDirective));
            }


            /* patch vertex and fragment shader */
            if (patchDirective)
            {
                gcSetGLSLCompiler(gcCompileShader);
                gcmONERROR(gcSHADER_DynamicPatch(blitDraw->vsShader[type], patchDirective));
                gcmONERROR(gcSHADER_DynamicPatch(blitDraw->psShader[type], patchDirective));
            }

            gcmONERROR(gcLinkShaders(blitDraw->vsShader[type],
                                     blitDraw->psShader[type],
                                     (gceSHADER_FLAGS)
                                     (gcvSHADER_DEAD_CODE            |
                                      gcvSHADER_RESOURCE_USAGE       |
                                      gcvSHADER_OPTIMIZER            |
                                      gcvSHADER_USE_GL_Z             |
                                      gcvSHADER_USE_GL_POINT_COORD   |
                                      gcvSHADER_USE_GL_POSITION      |
                                      gcvSHADER_REMOVE_UNUSED_UNIFORMS),
                                      &temp->programState.programSize,
                                      &temp->programState.programBuffer,
                                      &temp->programState.hints));

            *programState = &temp->programState;
            temp->destFmt = requestDstFmt;
            temp->srcFmt = requestSrcFmt;
            return gcvSTATUS_OK;

        }
        else
        {
            /* found one */
            *programState = &variation->programState;
            return gcvSTATUS_OK;
        }
    }
    else
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    return status;
}
#endif
#endif
#endif

#if gcdENABLE_2D
/* Init 2D related members in gcoHARDWARE. */
static gceSTATUS
_Init2D(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT i;

    /***********************************************************************
    ** Determine available features for 2D Hardware.
    */
    /* Determine whether 2D Hardware is present. */
    Hardware->hw2DEngine = ((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 9:9) & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))));

    /* Don't force software by default. */
    Hardware->sw2DEngine = gcvFALSE;
    Hardware->hw3DEngine = gcvFALSE;

    Hardware->hw2DAppendCacheFlush = 0;
    Hardware->hw2DCacheFlushAfterCompress = 0;
    Hardware->hw2DCacheFlushCmd    = gcvNULL;
    Hardware->hw2DCacheFlushSurf   = gcvNULL;

    /* Default enable splitting rectangle.*/
    Hardware->hw2DSplitRect = gcvTRUE;

    /* Determine whether byte write feature is present in the chip. */
    Hardware->byteWrite = ((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 19:19) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))));

    /* MASK register is missing on 4.3.1_rc0. */
    if (Hardware->config->chipRevision == 0x4310)
    {
        Hardware->shadowRotAngleReg = gcvTRUE;
        Hardware->rotAngleRegShadow = 0x00000000;
    }
    else
    {
        Hardware->shadowRotAngleReg = gcvFALSE;
    }

    Hardware->hw2D420L2Cache = ((((gctUINT32) (Hardware->config->chipMinorFeatures2)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))));

    Hardware->mmuVersion
        = (((((gctUINT32) (Hardware->config->chipMinorFeatures1)) >> (0 ? 28:28 )) & ((gctUINT32) ((((1 ? 28:28 ) - (0 ? 28:28 ) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28 ) - (0 ? 28:28 ) + 1)))))) );

    Hardware->hw2DDoMultiDst = gcvFALSE;

    for (i = 0; i < gcdTEMP_SURFACE_NUMBER; i += 1)
    {
        Hardware->temp2DSurf[i] = gcvNULL;
    }

    /***********************************************************************
    ** Initialize filter blit states.
    */
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].type = gcvFILTER_SYNC;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].kernelSize = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].scaleFactor = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_UNIFIED].kernelAddress = 0x01800;

    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].type = gcvFILTER_SYNC;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].kernelSize = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].scaleFactor = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_VERTICAL].kernelAddress = 0x02A00;

    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].type = gcvFILTER_SYNC;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].kernelSize = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].scaleFactor = 0;
    Hardware->loadedKernel[gcvFILTER_BLIT_KERNEL_HORIZONTAL].kernelAddress = 0x02800;

    /* Check to enable splitting rectangle. */
    Hardware->hw2DSplitRect = !(Hardware->config->chip2DControl & 0x100);

    /* Check to enable 2D Flush. */
    Hardware->hw2DAppendCacheFlush = Hardware->config->chip2DControl & 0x200;

    /* gcvFEATURE_2D_COMPRESSION */
    if (((((gctUINT32) (Hardware->config->chipMinorFeatures4)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))))
    {
        const gctUINT32 _2DCacheFlushCmd[] =
        {
            0x08010483,
            0x00000000,
            0x080104B0,
            0x00000000,
            0x080104C9,
            0x00030003,
            0x080104AD,
            0x00000000,
            0x0804048A,
            0x00000000,
            0x00000020,
            0x00000010,
            0xA0000004,
            0x00000000,
            0x08030497,
            0x0030CCCC,
            0x00000000,
            0x00100010,
            0x0801049F,
            0x00000000,
            0x080104AF,
            0x00000CC0,
            0x080104CA,
            0xFFFFFFCF,
            0x080104CB,
            0xFFF0FFFF,
            0x080204B4,
            0x00000000,
            0x00000000,
            0x00000000,
            0x20000100,
            0x00000000,
            0x00000000,
            0x00040004,
            0x08010E03,
            0x00000008,
        };

        gcmONERROR(gcoOS_Allocate(gcvNULL, gcmSIZEOF(_2DCacheFlushCmd), (gctPOINTER*)&Hardware->hw2DCacheFlushCmd));
        gcoOS_MemCopy(Hardware->hw2DCacheFlushCmd, _2DCacheFlushCmd, gcmSIZEOF(_2DCacheFlushCmd));
        Hardware->hw2DCacheFlushAfterCompress = gcmCOUNTOF(_2DCacheFlushCmd);

        if (Hardware->hw2DCacheFlushSurf == gcvNULL)
        {
            gctUINT i;

            gcmONERROR(gcoHARDWARE_Alloc2DSurface(Hardware,
                16, 16, gcvSURF_R5G6B5, gcvSURF_FLAG_NONE, &Hardware->hw2DCacheFlushSurf));

            for (i = 0; i < gcmCOUNTOF(_2DCacheFlushCmd); ++i)
            {
                if (Hardware->hw2DCacheFlushCmd[i] == 0x0804048A)
                {
                    Hardware->hw2DCacheFlushCmd[i + 1] = Hardware->hw2DCacheFlushSurf->node.physical;
                }
            }
        }
    }

OnError:
    return status;
}

static void
Fill2DFeatures(
    IN gcoHARDWARE  Hardware,
    OUT gctBOOL *    Features
    )
{
    gceCHIPMODEL    chipModel          = Hardware->config->chipModel;
    gctUINT32       chipRevision       = Hardware->config->chipRevision;
    gctUINT32       chipFeatures       = Hardware->config->chipFeatures;
    gctUINT32       chipMinorFeatures  = Hardware->config->chipMinorFeatures;
    gctUINT32       chipMinorFeatures1 = Hardware->config->chipMinorFeatures1;
    gctUINT32       chipMinorFeatures2 = Hardware->config->chipMinorFeatures2;
    gctUINT32       chipMinorFeatures3 = Hardware->config->chipMinorFeatures3;
    gctUINT32       chipMinorFeatures4 = Hardware->config->chipMinorFeatures4;
    gctUINT32       chipMinorFeatures5 = Hardware->config->chipMinorFeatures5;

    /* 2D hardware Pixel Engine 2.0 availability flag. */
    gctBOOL hw2DPE20 = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))));

    /* Support one pass filter blit and tiled/YUV input&output for Blit/StretchBlit. */
    gctBOOL hw2DOPF = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 17:17) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))));

    /* Support 9tap one pass filter blit. */
    gctBOOL hw2DOPFTAP = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))));

    gctBOOL hw2DSuperTile = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 29:29) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))));

    /* Support Multi-source blit (4 src). */
    gctBOOL hw2DMultiSrcBlit = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))));

    /* This feature including 8 multi source, 2x2 minor tile, U/V separate stride,
        AXI reorder, RGB non-8-pixel alignement. */
    gctBOOL hw2DNewFeature0 =((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 2:2) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))));

    gctBOOL hw2DDEEnhance1 = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 17:17) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))));

    gctBOOL hw2DDEEnhance5 = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 12:12) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))));

    gctBOOL hw2DCompression = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))));

    /*************************************************************************/
    /* 2D Features.                                                          */
    Features[gcvFEATURE_PIPE_2D] = ((((gctUINT32) (chipFeatures)) >> (0 ? 9:9) & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))));

    Features[gcvFEATURE_2DPE20] = hw2DPE20;

    /* Determine whether we support full DFB features. */
    Features[gcvFEATURE_FULL_DIRECTFB] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));
    Features[gcvFEATURE_YUV420_SCALER] = ((((gctUINT32) (chipFeatures)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    Features[gcvFEATURE_2D_TILING]   = hw2DOPF || hw2DSuperTile;
    Features[gcvFEATURE_2D_ONE_PASS_FILTER] =
    Features[gcvFEATURE_2D_YUV_BLIT] = hw2DOPF;
    Features[gcvFEATURE_2D_ONE_PASS_FILTER_TAP] = hw2DOPFTAP;

    Features[gcvFEATURE_2D_MULTI_SOURCE_BLT] = hw2DMultiSrcBlit || hw2DNewFeature0;

    Features[gcvFEATURE_2D_MULTI_SOURCE_BLT_EX] = hw2DNewFeature0;
    Features[gcvFEATURE_2D_PIXEL_ALIGNMENT] =
    Features[gcvFEATURE_2D_MINOR_TILING] =
    Features[gcvFEATURE_2D_YUV_SEPARATE_STRIDE] = hw2DNewFeature0;

    Features[gcvFEATURE_2D_FILTERBLIT_PLUS_ALPHABLEND] =
    Features[gcvFEATURE_2D_DITHER] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));

    Features[gcvFEATURE_2D_A8_TARGET] =
        ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 29:29) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))))
     || ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 22:22) & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))));

    /* Determine whether full rotation is present in the chip. */
    Features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION] =
    Features[gcvFEATURE_2D_BITBLIT_FULLROTATION] = hw2DPE20;
    if (chipRevision < 0x4310)
    {
        Features[gcvFEATURE_2D_BITBLIT_FULLROTATION]    = gcvFALSE;
        Features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION] = gcvFALSE;
    }
    else if (chipRevision == 0x4310)
    {
        Features[gcvFEATURE_2D_BITBLIT_FULLROTATION]    = gcvTRUE;
        Features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION] = gcvFALSE;
    }
    else if (chipRevision >= 0x4400)
    {
        Features[gcvFEATURE_2D_BITBLIT_FULLROTATION]    = gcvTRUE;
        Features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION] = gcvTRUE;
    }

    Features[gcvFEATURE_2D_NO_COLORBRUSH_INDEX8] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 28:28) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))));

    Features[gcvFEATURE_2D_FC_SOURCE] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))));

    if (chipModel > gcv520 && Features[gcvFEATURE_2D_FC_SOURCE])
    {
        Features[gcvFEATURE_2D_CC_NOAA_SOURCE] = gcvTRUE;
    }

    Features[gcvFEATURE_2D_GAMMA]= ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 5:5) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))));

    Features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION] = hw2DDEEnhance5;

    Features[gcvFEATURE_2D_SUPER_TILE_VERSION] = hw2DDEEnhance5 || hw2DSuperTile;

    Features[gcvFEATURE_2D_MIRROR_EXTENSION] =
        ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 18:18) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))))
     || ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 17:17) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))));

    if (hw2DDEEnhance5 || hw2DSuperTile)
    {
        Features[gcvFEATURE_2D_SUPER_TILE_V1] =
        Features[gcvFEATURE_2D_SUPER_TILE_V2] =
        Features[gcvFEATURE_2D_SUPER_TILE_V3] = gcvTRUE;
    }
    else if (hw2DOPF)
    {
        if (hw2DDEEnhance1
            || ((chipModel == gcv320)
            && (chipRevision >= 0x5303)))
        {
            Features[gcvFEATURE_2D_SUPER_TILE_V1] = gcvFALSE;
            Features[gcvFEATURE_2D_SUPER_TILE_V2] = gcvFALSE;
            Features[gcvFEATURE_2D_SUPER_TILE_V3] = gcvTRUE;
        }
        else
        {
            Features[gcvFEATURE_2D_SUPER_TILE_V1] = gcvFALSE;
            Features[gcvFEATURE_2D_SUPER_TILE_V2] = gcvTRUE;
            Features[gcvFEATURE_2D_SUPER_TILE_V3] = gcvFALSE;
        }
    }
    else
    {
        Features[gcvFEATURE_2D_SUPER_TILE_V1] =
        Features[gcvFEATURE_2D_SUPER_TILE_V2] =
        Features[gcvFEATURE_2D_SUPER_TILE_V3] = gcvFALSE;
    }

    Features[gcvFEATURE_2D_POST_FLIP] =
    Features[gcvFEATURE_2D_MULTI_SOURCE_BLT_EX2] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 22:22) & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))));

    /* Rotation stall fix. */
    Features[gcvFEATURE_2D_ROTATION_STALL_FIX] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 0:0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))));

    Features[gcvFEATURE_2D_MULTI_SRC_BLT_TO_UNIFIED_DST_RECT] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))))
        || hw2DCompression;

    Features[gcvFEATURE_2D_COMPRESSION] = hw2DCompression;

#if gcdENABLE_THIRD_PARTY_OPERATION
    Features[gcvFEATURE_TPC_COMPRESSION] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))));
#endif

    Features[gcvFEATURE_2D_OPF_YUV_OUTPUT] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))));

    Features[gcvFEATURE_2D_YUV_MODE] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))));

    Hardware->hw2DQuad =
    Hardware->hw2DBlockSize = hw2DDEEnhance1 || hw2DCompression;

    Features[gcvFEATURE_2D_FILTERBLIT_A8_ALPHA] = hw2DDEEnhance1;

    Features[gcvFEATURE_2D_ALL_QUAD] = Hardware->hw2DQuad;

    Features[gcvFEATURE_SEPARATE_SRC_DST] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 13:13) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))));

    Features[gcvFEATURE_NO_USER_CSC] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))));

    Features[gcvFEATURE_ANDROID_ONLY] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));

    Features[gcvFEATURE_2D_MULTI_SRC_BLT_TO_UNIFIED_DST_RECT] &= !Features[gcvFEATURE_ANDROID_ONLY];
}
#endif

#if gcdENABLE_2D || gcdENABLE_3D
/*******************************************************************************
**
**  _FillInFeatureTable
**
**  Verifies whether the specified feature is available in hardware.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to Hardware object.
**
**      gctBOOL * Features
**          Features to be filled.
*/
static gceSTATUS
_FillInFeatureTable(
    IN gcoHARDWARE  Hardware,
    IN gctBOOL *    Features
    )
{
    gceCHIPMODEL    chipModel          = Hardware->config->chipModel;
    gctUINT32       chipRevision       = Hardware->config->chipRevision;
    gctUINT32       chipFeatures       = Hardware->config->chipFeatures;
#if gcdENABLE_3D
    gctUINT32       chipMinorFeatures  = Hardware->config->chipMinorFeatures;
#endif
    gctUINT32       chipMinorFeatures1 = Hardware->config->chipMinorFeatures1;
    gctUINT32       chipMinorFeatures2 = Hardware->config->chipMinorFeatures2;
    gctUINT32       chipMinorFeatures3 = Hardware->config->chipMinorFeatures3;
    gctUINT32       chipMinorFeatures4 = Hardware->config->chipMinorFeatures4;
    gctUINT32       chipMinorFeatures5 = Hardware->config->chipMinorFeatures5;

    gctBOOL         useHZ = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x Features=0x%x", Hardware, Features);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

#if gcdENABLE_3D
    /* 3D and VG features. */
    Features[gcvFEATURE_PIPE_3D] = ((((gctUINT32) (chipFeatures)) >> (0 ? 2:2) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))));

    Features[gcvFEATURE_PIPE_VG] = ((((gctUINT32) (chipFeatures)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))));

    Features[gcvFEATURE_DC] = ((((gctUINT32) (chipFeatures)) >> (0 ? 8:8) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1)))))));

    Features[gcvFEATURE_HIGH_DYNAMIC_RANGE] = ((((gctUINT32) (chipFeatures)) >> (0 ? 12:12) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))));

    Features[gcvFEATURE_MODULE_CG] = ((((gctUINT32) (chipFeatures)) >> (0 ? 14:14) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))));

    Features[gcvFEATURE_MIN_AREA] = ((((gctUINT32) (chipFeatures)) >> (0 ? 15:15) & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1)))))));

    Features[gcvFEATURE_BUFFER_INTERLEAVING] = ((((gctUINT32) (chipFeatures)) >> (0 ? 18:18) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))));

    Features[gcvFEATURE_BYTE_WRITE_2D] = Hardware->byteWrite;

    Features[gcvFEATURE_ENDIANNESS_CONFIG] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 2:2) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))));

    Features[gcvFEATURE_DUAL_RETURN_BUS] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 1:1) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))));

    Features[gcvFEATURE_DEBUG_MODE] = ((((gctUINT32) (chipFeatures)) >> (0 ? 4:4) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))));

    Features[gcvFEATURE_FAST_CLEAR] = ((((gctUINT32) (chipFeatures)) >> (0 ? 0:0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))));


    Features[gcvFEATURE_YUV420_TILER] = ((((gctUINT32) (chipFeatures)) >> (0 ? 13:13) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))));

    Features[gcvFEATURE_YUY2_AVERAGING] = ((((gctUINT32) (chipFeatures)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))));

    Features[gcvFEATURE_FLIP_Y] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 0:0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))));

    Features[gcvFEATURE_EARLY_Z] = ((((gctUINT32) (chipFeatures)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));

    Features[gcvFEATURE_COMPRESSION] = ((((gctUINT32) (chipFeatures)) >> (0 ? 5:5) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))));

    Features[gcvFEATURE_MSAA] = ((((gctUINT32) (chipFeatures)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))));

    Features[gcvFEATURE_SPECIAL_ANTI_ALIASING] = ((((gctUINT32) (chipFeatures)) >> (0 ? 1:1) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))));

    Features[gcvFEATURE_SPECIAL_MSAA_LOD] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 5:5) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))));

    Features[gcvFEATURE_422_TEXTURE_COMPRESSION] = ((((gctUINT32) (chipFeatures)) >> (0 ? 17:17) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))) == (0x0 & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))));

    Features[gcvFEATURE_DXT_TEXTURE_COMPRESSION] = ((((gctUINT32) (chipFeatures)) >> (0 ? 3:3) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))));

    Features[gcvFEATURE_ETC1_TEXTURE_COMPRESSION] = ((((gctUINT32) (chipFeatures)) >> (0 ? 10:10) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))));

    Features[gcvFEATURE_CORRECT_TEXTURE_CONVERTER] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 4:4) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))));

    Features[gcvFEATURE_TEXTURE_8K] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 3:3) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))));

    Features[gcvFEATURE_SHADER_HAS_W] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 19:19) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))));

    Features[gcvFEATURE_VAA] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 25:25) & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))));

    Features[gcvFEATURE_HZ] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))));

    Features[gcvFEATURE_CORRECT_STENCIL] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 30:30) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))));

    Features[gcvFEATURE_MC20] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 22:22) & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))));

    Features[gcvFEATURE_SUPER_TILED] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 12:12) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))));
    Features[gcvFEATURE_FAST_CLEAR_FLUSH] = ((((gctUINT32) (chipMinorFeatures)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    Features[gcvFEATURE_WIDE_LINE] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 29:29) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))));

    Features[gcvFEATURE_FC_FLUSH_STALL] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 31:31) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))));

    if ((chipModel == gcv2000) && (chipRevision == 0x5118))
    {
         Features[gcvFEATURE_FC_FLUSH_STALL] = gcvTRUE;
    }

    Features[gcvFEATURE_TEXTURE_FLOAT_HALF_FLOAT] =
    Features[gcvFEATURE_HALF_FLOAT_PIPE] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))));

    Features[gcvFEATURE_NON_POWER_OF_TWO] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))));

    Features[gcvFEATURE_VERTEX_10_10_10_2] =
    Features[gcvFEATURE_TEXTURE_ANISOTROPIC_FILTERING] =
    Features[gcvFEATURE_TEXTURE_10_10_10_2] =
    Features[gcvFEATURE_3D_TEXTURE] =
    Features[gcvFEATURE_TEXTURE_ARRAY] =
    Features[gcvFEATURE_TEXTURE_SWIZZLE] =
    Features[gcvFEATURE_HALTI0] =
    Features[gcvFEATURE_OCCLUSION_QUERY] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))));

    if ((chipModel == gcv2000) && (chipRevision == 0x5118))
    {
        Features[gcvFEATURE_HALTI0] = gcvFALSE;
    }

    Features[gcvFEATURE_HALTI1] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))));

    Features[gcvFEATURE_PRIMITIVE_RESTART] =
    Features[gcvFEATURE_BUG_FIXED_IMPLICIT_PRIMITIVE_RESTART] =
    Features[gcvFEATURE_HALTI2] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));

    Features[gcvFEATURE_LINE_LOOP] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 0:0) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))));

    Features[gcvFEATURE_TILE_FILLER] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 19:19) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1)))))));

    Features[gcvFEATURE_LOGIC_OP] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 1:1) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))));

    Features[gcvFEATURE_COMPOSITION] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    Features[gcvFEATURE_MIXED_STREAMS] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 25:25) & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))));

    Features[gcvFEATURE_TEX_COMPRRESSION_SUPERTILED] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 5:5) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))));

    if ((chipModel == gcv1000) && (chipRevision == 0x5036))
    {
        Features[gcvFEATURE_TEX_COMPRRESSION_SUPERTILED] = gcvFALSE;
    }

    Features[gcvFEATURE_TEXTURE_TILE_STATUS_READ] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 29:29) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))))
                                                | ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    if (((chipModel == gcv2000)
         && ((chipRevision == 0x5020)
            || (chipRevision == 0x5026)
            || (chipRevision == 0x5113)
            || (chipRevision == 0x5108)))

       || ((chipModel == gcv2100)
           && ((chipRevision == 0x5108)
              || (chipRevision == 0x5113)))

       || ((chipModel == gcv4000)
           && ((chipRevision == 0x5222)
              || (chipRevision == 0x5208)))
       )
    {
        Features[gcvFEATURE_DEPTH_BIAS_FIX] = gcvFALSE;
    }
    else
    {
        Features[gcvFEATURE_DEPTH_BIAS_FIX] = gcvTRUE;
    }

    Features[gcvFEATURE_RECT_PRIMITIVE] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 5:5) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1)))))));

    if ((chipModel == gcv880) && (chipRevision == 0x5106))
    {
        Features[gcvFEATURE_RECT_PRIMITIVE] = gcvFALSE;
    }

    Features[gcvFEATURE_SUPERTILED_TEXTURE] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 3:3) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))));

    Features[gcvFEATURE_RS_YUV_TARGET] = ((((gctUINT32) (chipFeatures)) >> (0 ? 30:30) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))));

    /* We dont have a specific feature bit for this,
       there is a disable index cache feature in bug fix 8,
       but it's best also to check the chip revisions for this. */
    if (((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 31:31) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))))
    {
        Features[gcvFEATURE_BUG_FIXED_INDEXED_TRIANGLE_STRIP] = gcvTRUE;
    }
    else if ((chipModel == gcv880 && chipRevision >= 0x5100 && chipRevision < 0x5130)
    || (chipModel == gcv1000 && chipRevision >= 0x5100 && chipRevision < 0x5130)
    || (chipModel == gcv2000 && chipRevision >= 0x5100 && chipRevision < 0x5130)
    || (chipModel == gcv4000 && chipRevision >= 0x5200 && chipRevision < 0x5220))
    {
        Features[gcvFEATURE_BUG_FIXED_INDEXED_TRIANGLE_STRIP] = gcvFALSE;
    }
    else
    {
        Features[gcvFEATURE_BUG_FIXED_INDEXED_TRIANGLE_STRIP] = gcvTRUE;
    }

    if (((chipModel == gcv5000) && (chipRevision == 0x5309))
     || ((chipModel == gcv5000) && (chipRevision == 0x5434))
     || ((chipModel == gcv3000) && (chipRevision == 0x5435))
     || ((chipModel == gcv1500) && (chipRevision == 0x5246))
        )
    {
        Features[gcvFEATURE_INDEX_FETCH_FIX] = gcvFALSE;
    }
    else
    {
        Features[gcvFEATURE_INDEX_FETCH_FIX] = gcvTRUE;
    }


    Features[gcvFEATURE_PE_DITHER_FIX] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))));

    if (Hardware->specialHint & (1 << 2))
    {
        Features[gcvFEATURE_PE_DITHER_FIX] = gcvTRUE;
    }

    Features[gcvFEATURE_FRUSTUM_CLIP_FIX] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 2:2) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1)))))));

    Features[gcvFEATURE_TEXTURE_LINEAR] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 22:22) & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1)))))));

    Features[gcvFEATURE_TEXTURE_YUV_ASSEMBLER] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 13:13) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))));

    Features[gcvFEATURE_LINEAR_RENDER_TARGET] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 4:4) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))));

    Features[gcvFEATURE_SHADER_HAS_ATOMIC] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))));

    Features[gcvFEATURE_SHADER_ENHANCEMENTS2] =
    Features[gcvFEATURE_SHADER_HAS_RTNE] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))));

    Features[gcvFEATURE_SHADER_HAS_INSTRUCTION_CACHE] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 3:3) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1)))))));

    Features[gcvFEATURE_SHADER_ENHANCEMENTS3] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 29:29) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1)))))));

    Features[gcvFEATURE_BUG_FIXES7] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))));

    Features[gcvFEATURE_SHADER_HAS_EXTRA_INSTRUCTIONS2] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 14:14) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))));

    Features[gcvFEATURE_FAST_MSAA] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 8:8) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1)))))));

    Features[gcvFEATURE_SMALL_MSAA] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 18:18) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))));

    Features[gcvFEATURE_SINGLE_BUFFER] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    /* Load/Store L1 cache hang fix. */
    Features[gcvFEATURE_BUG_FIXES10] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 10:10) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1)))))));

    Features[gcvFEATURE_BUG_FIXES11] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 12:12) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))));

    Features[gcvFEATURE_TEXTURE_ASTC] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 13:13) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))));
#endif


    Features[gcvFEATURE_YUY2_RENDER_TARGET] = ((((gctUINT32) (chipFeatures)) >> (0 ? 24:24) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1)))))));

    Features[gcvFEATURE_FRAGMENT_PROCESSOR] = gcvFALSE;


    /* Filter Blit. */
    Features[gcvFEATURE_SCALER] = ((((gctUINT32) (chipFeatures)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x0 & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))));

    Features[gcvFEATURE_NEW_RA] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))));

    if (Features[gcvFEATURE_HALTI2])
    {
        Features[gcvFEATURE_32BPP_COMPONENT_TEXTURE_CHANNEL_SWIZZLE] = gcvTRUE;
        Features[gcvFEATURE_64BPP_HW_CLEAR_SUPPORT] = gcvTRUE;
        Features[gcvFEATURE_PE_MULTI_RT_BLEND_ENABLE_CONTROL] = gcvTRUE;
        Features[gcvFEATURE_VERTEX_INST_ID_AS_ATTRIBUTE] = gcvTRUE;
        Features[gcvFEATURE_VERTEX_INST_ID_AS_INTEGER]   = gcvTRUE;
        Features[gcvFEATURE_DUAL_16] = gcvTRUE;
        Features[gcvFEATURE_BRANCH_ON_IMMEDIATE_REG] = gcvTRUE;
        Features[gcvFEATURE_V2_COMPRESSION_Z16_FIX] = gcvTRUE;
        Features[gcvFEATURE_MRT_TILE_STATUS_BUFFER] = gcvTRUE;
        Features[gcvFEATURE_FE_START_VERTEX_SUPPORT] = gcvTRUE;
    }

    /* For test purpose only now */
    Features[gcvFEATURE_COLOR_COMPRESSION] = gcvFALSE;

    Features[gcvFEATURE_TX_LERP_PRECISION_FIX] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))));
    Features[gcvFEATURE_COMPRESSION_V2] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 1:1) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))));

    /* For evaluation only now */
    Features[gcvFEATURE_COMPRESSION_V3] = gcvFALSE;

    if (!Features[gcvFEATURE_COMPRESSION_V3] &&
        !Features[gcvFEATURE_COMPRESSION_V2] &&
        Features[gcvFEATURE_COMPRESSION])
    {
        Features[gcvFEATURE_COMPRESSION_V1] = gcvTRUE;
    }

    Features[gcvFEATURE_MMU] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 28:28) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))));

    /* v1 decompressor support TX read if tx support tile status buffer  */
    Features[gcvFEATURE_TX_DECOMPRESSOR] =  Features[gcvFEATURE_TEXTURE_TILE_STATUS_READ] &&
                                            Features[gcvFEATURE_COMPRESSION_V1] ;

    Features[gcvFEATURE_V1_COMPRESSION_Z16_DECOMPRESS_FIX] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 30:30) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1)))))));

    /* TODO: replace RTT check with surface check later rather than feature bit.
    */
    if (Hardware->config->pixelPipes == 1)
    {
        if (((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))))
        {
            Features[gcvFEATURE_RTT] = gcvTRUE;
        }

    }
    else
    {
        /*
        ** gcvFEATURE_TEXTURE_TILE_STATUS_READ for HISI special case.
        */
        if ((Hardware->features[gcvFEATURE_SINGLE_BUFFER]) ||
            (Hardware->features[gcvFEATURE_TEXTURE_TILE_STATUS_READ]))
        {
            Features[gcvFEATURE_RTT] = gcvTRUE;
        }
    }

    Features[gcvFEATURE_GENERICS] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    Features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT] = ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))));

    Features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT_WIDTH] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 7:7) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1)))))));

    Features[gcvFEATURE_HALTI3] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 9:9) & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1)))))));

    /* Please don't use it, it's only for very latest hardware and NOT ready */
    Features[gcvFEATURE_EEZ] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 4:4) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))));

    if (((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 8:8) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) &&
        ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 24:24) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1)))))))
                            )
    {
        Features[gcvFEATURE_PA_FARZCLIPPING_FIX] = gcvTRUE;
        Features[gcvFEATURE_ZSCALE_FIX] = gcvTRUE;
    }


    if (Features[gcvFEATURE_HALTI3])
    {
        Features[gcvFEATURE_INTEGER_SIGNEXT_FIX] = gcvTRUE;
        Features[gcvFEATURE_INTEGER_PIPE_FIX] = gcvTRUE;
        Features[gcvFEATURE_PSOUTPUT_MAPPING] = gcvTRUE;
        Features[gcvFEATURE_8K_RT_FIX] = gcvTRUE;
        Features[gcvFEATURE_TX_TILE_STATUS_MAPPING] = gcvTRUE;
        Features[gcvFEATURE_SRGB_RT_SUPPORT] = gcvTRUE;
        Features[gcvFEATURE_UNIFORM_APERTURE] = gcvTRUE;
        Features[gcvFEATURE_TEXTURE_16K] = gcvTRUE;
        Features[gcvFEATURE_PE_DITHER_COLORMASK_FIX] = gcvTRUE;
        Features[gcvFEATURE_TX_FRAC_PRECISION_6BIT] = gcvTRUE;
        Features[gcvFEATURE_SH_INSTRUCTION_PREFETCH] = gcvTRUE;
    }

    Features[gcvFEATURE_PROBE] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))));

    Features[gcvFEATURE_MULTI_PIXELPIPES] = (Hardware->config->pixelPipes > 1);

    Features[gcvFEATURE_PIPE_CL] = ((chipModel != gcv880) && ((((gctUINT32) (chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))));

    Features[gcvFEATURE_BUG_FIXES18] = ((((gctUINT32) (chipMinorFeatures4)) >> (0 ? 25:25) & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1)))))));

    Features[gcvFEATURE_BUG_FIXES8] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 31:31) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))));

    Features[gcvFEATURE_UNIFIED_SAMPLERS] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 11:11) & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1)))))));

    Features[gcvFEATURE_CL_PS_WALKER] = ((((gctUINT32) (chipMinorFeatures2)) >> (0 ? 18:18) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1)))))));

    Features[gcvFEATURE_NEW_HZ] = ((((gctUINT32) (chipMinorFeatures3)) >> (0 ? 26:26) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1)))))));

    /* Need original value of gcvFEATURE_NEW_HZ, gcvFEATURE_FAST_MAAA
     * and gcvFEATURE_EARLY_Z, don't change them before here.  */
    if (((chipModel == gcv4000) && (chipRevision == 0x5245))
     || ((chipModel == gcv5000) && (chipRevision == 0x5309))
     || ((chipModel == gcv2500) && (chipRevision == 0x5410))
     || ((chipModel == gcv5200) && (chipRevision == 0x5410))
     || ((chipModel == gcv6400) && (chipRevision == 0x5420))
     || ((chipModel == gcv6400) && (chipRevision == 0x5422))
     || ((chipModel == gcv5000) && (chipRevision == 0x5434))
       )
    {
        useHZ = Features[gcvFEATURE_NEW_HZ] || Features[gcvFEATURE_FAST_MSAA];
    }

    /* If new HZ is available, disable other early z modes. */
    if (useHZ)
    {
        /* Disable EZ. */
        Features[gcvFEATURE_EARLY_Z] = gcvFALSE;
    }

    /* Disable HZ when EZ is present for older chips. */
    else if (Features[gcvFEATURE_EARLY_Z])
    {
        /* Disable HIERARCHICAL_Z. */
        Features[gcvFEATURE_HZ] = gcvFALSE;
    }

    if (chipModel == gcv320 && chipRevision <= 0x5340)
    {
        Features[gcvFEATURE_2D_A8_NO_ALPHA] = gcvTRUE;
    }

#if gcdENABLE_3D
#if !VIV_STAT_ENABLE_STATISTICS
    if ((chipModel == gcv2000) && (chipRevision == 0x5108) && (Hardware->patchID == gcvPATCH_QUADRANT))
    {
        Features[gcvFEATURE_HZ] = gcvFALSE;
    }
#endif

#if defined(ANDROID)
    if (Hardware->patchID == gcvPATCH_FISHNOODLE)
    {
        Features[gcvFEATURE_HZ] = gcvFALSE;
    }
#endif

#if !VIV_STAT_ENABLE_STATISTICS
    /* Disable EZ for gc880. */
    if ((chipModel == gcv880) && (chipRevision == 0x5106) && (Hardware->patchID == gcvPATCH_QUADRANT))
    {
        Features[gcvFEATURE_EARLY_Z] = gcvFALSE;
    }
#endif
#endif

    /* single pipe with halti1 feature set (gc1500)*/
    if (Features[gcvFEATURE_HALTI1] && (Hardware->config->pixelPipes == 1))
    {
        Features[gcvFEATURE_SINGLE_PIPE_HALTI1] = gcvTRUE;
    }

    if (((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))))
    {
        Features[gcvFEATURE_S8_ONLY_RENDERING] = gcvTRUE;

        Features[gcvFEATURE_RS_DEPTHSTENCIL_NATIVE_SUPPORT] = gcvTRUE;
    }
    else if ((chipModel == gcv1500 && chipRevision == 0x5246) ||
             (chipRevision == 0x5450))
    {
        Features[gcvFEATURE_S8_ONLY_RENDERING] = gcvTRUE;
    }

    if ((chipModel == gcv420) && (chipRevision == 0x5337))
    {
         Features[gcvFEATURE_BLOCK_SIZE_16x16] = gcvTRUE;
    }

    Features[gcvFEATURE_HAS_PRODUCTID] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 17:17) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1)))))));

    Features[gcvFEATURE_HALTI4] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 14:14) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))));

    if (Features[gcvFEATURE_HALTI4])
    {
        Features[gcvFEATURE_MSAA_FRAGMENT_OPERATION] = gcvTRUE;
        Features[gcvFEATURE_ZERO_ATTRIB_SUPPORT] = gcvTRUE;
    }

    Features[gcvFEATURE_V2_MSAA_COMP_FIX] = ((((gctUINT32) (chipMinorFeatures5)) >> (0 ? 28:28) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))))
                                              || (Hardware->config->ecoFlags & gcvECO_FLAG_MSAA_COHERENCEY);


#if gcdENABLE_2D
    Fill2DFeatures(Hardware, Features);
#endif

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}


static gceSTATUS
_FillInConfigTable(
    IN gcoHARDWARE          Hardware,
    IN gcsHARDWARE_CONFIG * Config
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Hardware=0x%x Config=0x%x", Hardware, Config);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /***********************************************************************
    ** Query chip identity.
    */

    iface.command = gcvHAL_QUERY_CHIP_IDENTITY;

    gcmONERROR(gcoOS_DeviceControl(
        gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        &iface, gcmSIZEOF(iface),
        &iface, gcmSIZEOF(iface)
        ));

    Config->chipModel              = iface.u.QueryChipIdentity.chipModel;
    Config->chipRevision           = iface.u.QueryChipIdentity.chipRevision;
    Config->chipFeatures           = iface.u.QueryChipIdentity.chipFeatures;
    Config->chipMinorFeatures      = iface.u.QueryChipIdentity.chipMinorFeatures;
    Config->chipMinorFeatures1     = iface.u.QueryChipIdentity.chipMinorFeatures1;
    Config->chipMinorFeatures2     = iface.u.QueryChipIdentity.chipMinorFeatures2;
    Config->chipMinorFeatures3     = iface.u.QueryChipIdentity.chipMinorFeatures3;
    Config->chipMinorFeatures4     = iface.u.QueryChipIdentity.chipMinorFeatures4;
    Config->chipMinorFeatures5     = iface.u.QueryChipIdentity.chipMinorFeatures5;
    Config->pixelPipes             = iface.u.QueryChipIdentity.pixelPipes;
    Config->chip2DControl          = iface.u.QueryChipIdentity.chip2DControl;
    Config->productID              = iface.u.QueryChipIdentity.productID;
    Config->ecoFlags               = iface.u.QueryChipIdentity.ecoFlags;

#if gcdENABLE_3D
    if (Hardware->patchID == gcvPATCH_DUOKAN)
    {
        /*Disable super-tile */
        Config->chipMinorFeatures =
            ((((gctUINT32) (Config->chipMinorFeatures)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
    }


    Config->varyingsCount          = iface.u.QueryChipIdentity.varyingsCount;
    Config->superTileMode          = iface.u.QueryChipIdentity.superTileMode;

#if gcdMULTI_GPU
    Config->gpuCoreCount           = iface.u.QueryChipIdentity.gpuCoreCount;
#endif

    /* Multi render target. */
    if (((((gctUINT32) (Config->chipMinorFeatures4)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))))
    {
        Config->renderTargets = 8;
    }
    else if (((((gctUINT32) (Config->chipMinorFeatures1)) >> (0 ? 23:23) & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))))
    {
        Config->renderTargets = 4;
    }
    else
    {
        Config->renderTargets = 1;
    }

    Config->streamCount            = iface.u.QueryChipIdentity.streamCount;
    Config->registerMax            = iface.u.QueryChipIdentity.registerMax;
    Config->threadCount            = iface.u.QueryChipIdentity.threadCount;
    Config->shaderCoreCount        = iface.u.QueryChipIdentity.shaderCoreCount;
    Config->vertexCacheSize        = iface.u.QueryChipIdentity.vertexCacheSize;
    Config->vertexOutputBufferSize = iface.u.QueryChipIdentity.vertexOutputBufferSize;
    Config->instructionCount       = iface.u.QueryChipIdentity.instructionCount;
    Config->numConstants           = iface.u.QueryChipIdentity.numConstants;
    Config->bufferSize             = iface.u.QueryChipIdentity.bufferSize;

    /* Determine shader parameters. */

    /* Determine constant parameters. */
    {if (Config->numConstants > 256){    Config->unifiedConst = gcvTRUE;    Config->vsConstBase  = 0xC000;    Config->psConstBase  = 0xC000;    Config->vsConstMax   = gcmMIN(512, Config->numConstants - 64);    Config->psConstMax   = gcmMIN(512, Config->numConstants - 64);    Config->constMax     = Config->numConstants;}else if (Config->numConstants == 256){    if (Config->chipModel == gcv2000 && Config->chipRevision == 0x5118)    {        Config->unifiedConst = gcvFALSE;        Config->vsConstBase  = 0x1400;        Config->psConstBase  = 0x1C00;        Config->vsConstMax   = 256;        Config->psConstMax   = 64;        Config->constMax     = 320;    }    else    {        Config->unifiedConst = gcvFALSE;        Config->vsConstBase  = 0x1400;        Config->psConstBase  = 0x1C00;        Config->vsConstMax   = 256;        Config->psConstMax   = 256;        Config->constMax     = 512;    }}else{    Config->unifiedConst = gcvFALSE;    Config->vsConstBase  = 0x1400;    Config->psConstBase  = 0x1C00;    Config->vsConstMax   = 168;    Config->psConstMax   = 64;    Config->constMax     = 232;}};

    /* Determine instruction parameters. */
    /* TODO - need to add code for iCache. */
    if (Config->instructionCount > 1024)
    {
        Config->unifiedShader = gcvTRUE;
        Config->vsInstBase    = 0x8000;
        Config->psInstBase    = 0x8000;
        Config->vsInstMax     =
        Config->psInstMax     =
        Config->instMax       = Config->instructionCount;
    }
    else if (Config->instructionCount > 256)
    {
        Config->unifiedShader = gcvTRUE;
        Config->vsInstBase    = 0x3000;
        Config->psInstBase    = 0x2000;
        Config->vsInstMax     =
        Config->psInstMax     =
        Config->instMax       = Config->instructionCount;
    }
    else
    {
        gcmASSERT(Config->instructionCount == 256);
        Config->unifiedShader = gcvFALSE;
        Config->vsInstBase    = 0x1000;
        Config->psInstBase    = 0x1800;
        Config->vsInstMax     = 256;
        Config->psInstMax     = 256;
        Config->instMax       = 256 + 256;
    }
#endif

    gcmFOOTER_NO();
    return status;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_FillInSWWATable(
    IN gcoHARDWARE  Hardware,
    IN gctBOOL *    SWWA
    )
{
    gceCHIPMODEL    chipModel          = Hardware->config->chipModel;
    gctUINT32       chipRevision       = Hardware->config->chipRevision;

    gcmHEADER_ARG("Hardware=0x%x SWWA=0x%x", Hardware, SWWA);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if ((chipModel == gcv2000) && (chipRevision == 0x5118))
    {
        SWWA[gcvSWWA_1165] = gcvTRUE;
    }

    if ((chipModel == gcv4000) &&
        (((chipRevision & 0xFFF0) == 0x5240) ||
         ((chipRevision & 0xFFF0) == 0x5300) ||
         ((chipRevision & 0xFFF0) == 0x5310) ||
         ((chipRevision & 0xFFF0) == 0x5410) ||
         (chipRevision == 0x5421)))
    {
        SWWA[gcvSWWA_1163] = gcvTRUE;
    }

    if  ((chipModel == gcv2200 && chipRevision == 0x5244) ||
         (chipModel == gcv1500 && chipRevision == 0x5246)
        )
    {
        SWWA[gcvSWWA_1163] = gcvTRUE;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

#if gcdUSE_HARDWARE_CONFIGURATION_TABLES
static gceSTATUS
_InitHardwareTables(
    IN gcoHARDWARE  Hardware
    )
{
    gceSTATUS       status;
    gceCHIPMODEL    chipModel;
    gctUINT32       chipRevision;
    gcsHARDWARE_CONFIG_TABLE * table;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Hardware=%d", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmASSERT(gcConfig   == gcvNULL);
    gcmASSERT(gcFeatures == gcvNULL);
    gcmASSERT(gcSWWAs    == gcvNULL);

    /* Get chip id and revision. */
    iface.command = gcvHAL_QUERY_CHIP_IDENTITY;
    gcmONERROR(gcoOS_DeviceControl(
        gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        &iface, gcmSIZEOF(iface),
        &iface, gcmSIZEOF(iface)
        ));

    chipModel    = iface.u.QueryChipIdentity.chipModel;
    chipRevision = iface.u.QueryChipIdentity.chipRevision;

    /* Get the predefined tables. */
    table = gcHARDWARE_Config_Tables;
    while (table->chipModel < chipModel)
    {
        table++;
    }

    if (table->chipModel == chipModel)
    {
        if (chipRevision != 0x0)
        {
            while (table->chipModel == chipModel
                && table->chipRevision < chipRevision)
            {
                table++;
            }
        }
    }

    if (table->chipModel == chipModel
    &&  table->chipRevision < chipRevision)
    {
        Hardware->config   = gcConfig   = table->config;
        Hardware->features = gcFeatures = table->features;
        Hardware->swwas    = gcSWWAs    = table->swwas;
    }
    else
    {
        /* If no predefined tables, fill in the tables. */
        gcmVERIFY_OK(_FillInConfigTable(Hardware, &_gcConfig));
        Hardware->config   = gcConfig   = &_gcConfig;
#if gcdENABLE_2D
        gcmVERIFY_OK(_Init2D(Hardware));
#endif
        gcmVERIFY_OK(_FillInFeatureTable(Hardware, _gcFeatures));
        Hardware->features = gcFeatures = _gcFeatures;
        gcmVERIFY_OK(_FillInSWWATable(Hardware, _gcSWWAs));
        Hardware->swwas    = gcSWWAs    = _gcSWWAs;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#if gcdENABLE_3D
typedef struct _gcoPATCH_BENCH
{
    gcePATCH_ID patchId;
    gctCONST_STRING name;

    /* If replaced by symbol checking, please set gcvTRUE(just android platform), else gcvFALSE. */
    gctBOOL symbolFlag;
}
gcoPATCH_BENCH;
#endif

gceSTATUS
gcoHARDWARE_DetectProcess(
    IN gcoHARDWARE Hardware
    )
{
#if !gcdENABLE_3D
    return gcvSTATUS_OK;
#else
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT i = 0;

    gcoPATCH_BENCH benchList[] =
    {
        /* Benchmark List */
        {
            gcvPATCH_GLBM21,
 "\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xce",
            gcvFALSE
        },
        {
            gcvPATCH_GLBM25,
 "\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xca",
            gcvFALSE
        },
        {
            gcvPATCH_GLBM27,
 "\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xc8",
            gcvFALSE
        },


#if defined(ANDROID)
        {
            gcvPATCH_GLBM21,
   "\x9c\x90\x92\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xce",
            gcvTRUE
        },
        {
            gcvPATCH_GLBM25,
   "\x9c\x90\x92\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xca",
            gcvTRUE
        },
        {
            gcvPATCH_GLBM27,
   "\x9c\x90\x92\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xc8",
            gcvTRUE
        },
        {
            gcvPATCH_GFXBENCH,
           "\x91\x9a\x8b\xd1\x94\x96\x8c\x97\x90\x91\x8b\x96\xd1\x98\x99\x87\x9d\x9a\x91\x9c\x97",
            gcvTRUE
        },
        {
            gcvPATCH_GLBMGUI,
 "\x9c\x90\x92\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xb8\xaa\xb6",
            gcvTRUE
        },
        {
            gcvPATCH_RIPTIDEGP2,
 "\x9c\x90\x92\xd1\x89\x9a\x9c\x8b\x90\x8d\x8a\x91\x96\x8b\xd1\x8d\x9a\x9b",
            gcvFALSE
        },
#endif
        {
            gcvPATCH_GTFES30,
                      "\x9c\x8b\x8c\xd2\x8d\x8a\x91\x91\x9a\x8d",
            gcvFALSE
        },
        {
            gcvPATCH_GTFES30,
                           "\x98\x93\x9c\x8b\x8c",
            gcvFALSE
        },
        {
            gcvPATCH_GTFES30,
              "\x90\x8d\x98\xd1\x94\x97\x8d\x90\x91\x90\x8c\xd1\x98\x93\xa0\x9c\x8b\x8c",
            gcvFALSE
        },
        {
            gcvPATCH_GTFES30,
                             "\xb8\xab\xb9",
            gcvFALSE
        },
        {
            gcvPATCH_GTFES30,
     "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x8e\x9e\xd1\x90\x9a\x8c\xcc\xcf\x9c\x90\x91\x99\x90\x8d\x92",
            gcvFALSE
        },
        {
            gcvPATCH_CTGL20,
                 "\x9c\x8b\x98\x93\xaf\x93\x9e\x86\x9a\x8d\xb3\xb1\xa7\x92\x8f",
            gcvFALSE
        },
        {
            gcvPATCH_CTGL20,
              "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x8b\x98\x93\xcd\xcf",
            gcvFALSE
        },
        {
            gcvPATCH_CTGL11,
                   "\x9c\x8b\x98\x93\xaf\x93\x9e\x86\x9a\x8d\xb3\xb1\xa7",
            gcvFALSE
        },
        {
            gcvPATCH_CTGL11,
              "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x8b\x98\x93\xce\xce",
            gcvFALSE
        },
#if !defined(ANDROID)
        {
            gcvPATCH_BM21,
 "\x9d\x9e\x8c\x9a\x92\x9e\x8d\x94",
            gcvFALSE
        },
#endif
        {
            gcvPATCH_BM21,
 "\x9c\x90\x92\xd1\x8d\x96\x98\x97\x8b\x88\x9e\x8d\x9a\xd1\x8b\x9b\x92\x92\xcd",
            gcvTRUE
        },
        {
            gcvPATCH_MM06,
 "\x92\x92\xcf\xc9",
            gcvFALSE
        },
        {
            gcvPATCH_MM06,
 "\x9c\x90\x92\xd1\x89\x96\x89\x9e\x91\x8b\x9a\x9c\x90\x8d\x8f\xd1\x98\x8d\x9e\x8f\x97\x96\x9c\x8c\xd1\x92\x92\xcf\xc9",
            gcvTRUE
        },
        {
            gcvPATCH_MM07,
 "\x92\x92\xcf\xc8",
            gcvFALSE
        },
        {
            gcvPATCH_MM07,
 "\x9c\x90\x92\xd1\x89\x96\x89\x9e\x91\x8b\x9a\x9c\x90\x8d\x8f\xd1\x98\x8d\x9e\x8f\x97\x96\x9c\x8c\xd1\x92\x92\xcf\xc8",
            gcvTRUE
        },
        {
            gcvPATCH_QUADRANT,
 "\x9c\x90\x92\xd1\x9e\x8a\x8d\x90\x8d\x9e\x8c\x90\x99\x8b\x88\x90\x8d\x94\x8c\xd1\x8e\x8a\x9e\x9b\x8d\x9e\x91\x8b",
            gcvFALSE
        },
        {
            gcvPATCH_ANTUTU,
          "\x9c\x90\x92\xd1\x9e\x91\x8b\x8a\x8b\x8a\xd1\xbe\xbd\x9a\x91\x9c\x97\xb2\x9e\x8d\x94",
            gcvTRUE
        },
        {
            gcvPATCH_GPUBENCH,
                       "\x98\x8f\x8a\xbd\x9a\x91\x9c\x97",
            gcvFALSE
        },
        {
            gcvPATCH_SMARTBENCH,
 "\x9c\x90\x92\xd1\x8c\x92\x9e\x8d\x8b\x9d\x9a\x91\x9c\x97",
            gcvFALSE
        },
        {
            gcvPATCH_JPCT,
 "\x9c\x90\x92\xd1\x8b\x97\x8d\x9a\x9a\x9b\xd1\x95\x8f\x9c\x8b\xd1\x9d\x9a\x91\x9c\x97",
            gcvFALSE
        },
        {
            gcvPATCH_FISHNOODLE,
 "\x99\x96\x8c\x97\x91\x90\x90\x9b\x93\x9a\xd1\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94",
            gcvTRUE
        },
        {
            gcvPATCH_NENAMARK2,
 "\x8c\x9a\xd1\x91\x9a\x91\x9e\xd1\x91\x9a\x91\x9e\x92\x9e\x8d\x94\xcd",
            gcvTRUE
        },
        {
            gcvPATCH_NENAMARK,
 "\x8c\x9a\xd1\x91\x9a\x91\x9e\xd1\x91\x9a\x91\x9e\x92\x9e\x8d\x94\xce",
            gcvTRUE
        },
        {
            gcvPATCH_NEOCORE,
 "\x9c\x90\x92\xd1\x8e\x8a\x9e\x93\x9c\x90\x92\x92\xd1\x8e\x87\xd1\x91\x9a\x90\x9c\x90\x8d\x9a",
            gcvTRUE
        },
        {
            gcvPATCH_GLBM11,
 "\x9c\x90\x92\xd1\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xd1\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xce\xce",
            gcvTRUE
        },
        {
            gcvPATCH_BMGUI,
 "\x9c\x90\x92\xd1\x8d\x96\x98\x97\x8b\x88\x9e\x8d\x9a\xd1\x9d\x9e\x8c\x9a\x92\x9e\x8d\x94\x98\x8a\x96",
            gcvTRUE
        },
        {
            gcvPATCH_RTESTVA,
 "\x9c\x90\x92\xd1\x8b\x97\x96\x91\x94\x88\x9e\x8d\x9a\xd1\xad\xab\x9a\x8c\x8b\xa9\xbe",
            gcvFALSE
        },
        {
            gcvPATCH_BUSPARKING3D,
 "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x8c\x97\x9a\x93\x93\xd1\x9d\x8a\x8c\x8f\x9e\x8d\x94\x96\x91\x98\xcc\x9b",
            gcvFALSE
        },
        {
            gcvPATCH_PREMIUM,
 "\x9c\x90\x92\xd1\x9d\x8a\x93\x94\x86\xd1\xa6\x9a\x8c\x8b\x9a\x8d\x9b\x9e\x86\xd1\x8f\x8d\x9a\x92\x96\x8a\x92",
            gcvFALSE
        },
        {
            gcvPATCH_RACEILLEGAL,
 "\x9c\x90\x92\xd1\x97\x9a\x8d\x90\x9c\x8d\x9e\x99\x8b\xd1\x98\x9e\x92\x9a\xd1\x8d\x9e\x9c\x9a\x96\x93\x93\x9a\x98\x9e\x93",
            gcvFALSE
        },
        {
            gcvPATCH_MEGARUN,
 "\x9c\x90\x92\xd1\x98\x9a\x8b\x8c\x9a\x8b\x98\x9e\x92\x9a\x8c\xd1\x92\x9a\x98\x9e\x8d\x8a\x91",
            gcvFALSE
        },
        {
            gcvPATCH_GLOFTSXHM,
 "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xbe\xb1\xb2\xaf\xd1\xb8\x93\x90\x99\x8b\xac\xa7\xb7\xb2",
            gcvFALSE
        },
        {
            gcvPATCH_XRUNNER,
 "\x9c\x90\x92\xd1\x9b\x8d\x90\x96\x9b\x97\x9a\x91\xd1\x98\x9e\x92\x9a\xd1\x87\x8d\x8a\x91\x91\x9a\x8d",
            gcvFALSE
        },
        {
            gcvPATCH_SIEGECRAFT,
 "\x9c\x90\x92\xd1\x9d\x93\x90\x88\x99\x96\x8c\x97\x8c\x8b\x8a\x9b\x96\x90\x8c\xd1\x8c\x96\x9a\x98\x9a\x9c\x8d\x9e\x99\x8b",
            gcvFALSE
        },
        {
            gcvPATCH_DUOKAN,
 "\x9c\x90\x92\xd1\x9b\x8a\x90\x94\x9e\x91\xd1\x9b\x8a\x90\x94\x9e\x91\x8b\x89\x8b\x9a\x8c\x8b",
            gcvFALSE
        },
        {
            gcvPATCH_NBA2013,
 "\x9c\x90\x92\xd1\x8b\xcd\x94\x8c\x8f\x90\x8d\x8b\x8c\xd1\x91\x9d\x9e\xcd\x94\xce\xcc\x9e\x91\x9b\x8d\x90\x96\x9b",
            gcvFALSE
        },
        {
            gcvPATCH_BARDTALE,
 "\x9c\x90\x92\xd1\x96\x91\x87\x96\x93\x9a\xd1\xbd\x9e\x8d\x9b\xab\x9e\x93\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_F18,
 "\x96\x8b\xd1\x8d\x90\x8d\x8b\x90\x8c\xd1\x99\xce\xc7\x9c\x9e\x8d\x8d\x96\x9a\x8d\x93\x9e\x91\x9b\x96\x91\x98\x93\x96\x8b\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_CARPARK,
 "\x9c\x90\x92\xd1\x96\x9c\x93\x90\x8a\x9b\x85\x90\x91\x9a\xd1\xbc\x9e\x8d\xaf\x9e\x8d\x94",
            gcvFALSE
        },
        {
            gcvPATCH_CARCHALLENGE,
 "\x91\x9a\x8b\xd1\x99\x96\x8c\x97\x93\x9e\x9d\x8c\xd1\xac\x8f\x90\x8d\x8b\x8c\xbc\x9e\x8d\xbc\x97\x9e\x93\x93\x9a\x91\x98\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_HEROESCALL,
 "\x9c\x90\x92\xd1\xbb\x9a\x99\x96\x9e\x91\x8b\xbb\x9a\x89\xd1\x97\x9a\x8d\x90\x9a\x8c\x9c\x9e\x93\x93\xd1\xab\xb7\xbb",
            gcvFALSE
        },
        {
            gcvPATCH_GLOFTF3HM,
 "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xbe\xb1\xb2\xaf\xd1\xb8\x93\x90\x99\x8b\xb9\xcc\xb7\xb2",
            gcvFALSE
        },
        {
            gcvPATCH_CRAZYRACING,
 "\x9c\x90\x92\xd1\x88\x96\x91\x9b\xd1\xbc\x8d\x9e\x85\x86\xad\x9e\x9c\x96\x91\x98",
            gcvFALSE
        },
        {
            gcvPATCH_CTS_TEXTUREVIEW,
            /* "com.android.cts.textureview" */
            "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x8b\x8c\xd1"
            "\x8b\x9a\x87\x8b\x8a\x8d\x9a\x89\x96\x9a\x88",
            gcvFALSE
        },
        {
            gcvPATCH_FIREFOX,
#if defined(ANDROID)
 "\x90\x8d\x98\xd1\x92\x90\x85\x96\x93\x93\x9e\xd1\x99\x96\x8d\x9a\x99\x90\x87",
#else
 "\x99\x96\x8d\x9a\x99\x90\x87",
#endif
            gcvFALSE
        },
        {
            gcvPATCH_CHROME,
#if defined(ANDROID)
 "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x97\x8d\x90\x92\x9a",
#else
 "\x9c\x97\x8d\x90\x92\x9a",
#endif
            gcvFALSE
        },
        {
            gcvPATCH_CHROME,
 "\x9c\x90\x92\xd1\x9c\x97\x8d\x90\x92\x9a\xd1\x8b\x89\xd1\x8c\x8b\x9e\x9d\x93\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_CHROME,
 "\x9c\x90\x92\xd1\x9c\x97\x8d\x90\x92\x9a\xd1\x9d\x9a\x8b\x9e",
            gcvFALSE
        },
        {
            gcvPATCH_CHROME,
 "\x98\x90\x90\x98\x93\x9a\xd2\x9c\x97\x8d\x90\x92\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_MONOPOLY,
 "\x9c\x90\x92\xd1\x9a\x9e\xd1\x92\x90\x91\x90\x8f\x90\x93\x86\x92\x96\x93\x93\x96\x90\x91\x9e\x96\x8d\x9a\xa0\x91\x9e",
            gcvFALSE
        },
        {
            gcvPATCH_SNOWCOLD,
 "\x9c\x90\x92\xd1\x8c\x91\x90\x88\x9c\x90\x93\x9b\xd1\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94",
            gcvFALSE
        },
        {
            gcvPATCH_BM3,
 "\x9c\x90\x92\xd1\x8d\x96\x98\x97\x8b\x88\x9e\x8d\x9a\xd1\x94\x9e\x91\x85\x96\xd1\x9d\x9e\x8c\x9a\x92\x9e\x8d\x94\x9a\x8c\xcc",
            gcvTRUE
        },
        {
            gcvPATCH_DEQP,
 "\x9c\x90\x92\xd1\x9b\x8d\x9e\x88\x9a\x93\x9a\x92\x9a\x91\x8b\x8c\xd1\x9b\x9a\x8e\x8f\xd1\x9a\x87\x9a\x9c\x8c\x9a\x8d\x89\x9a\x8d\xc5\x8b\x9a\x8c\x8b\x9a\x8d\x9c\x90\x8d\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_DEQP,
 "\x9c\x90\x92\xd1\x9b\x8d\x9e\x88\x9a\x93\x9a\x92\x9a\x91\x8b\x8c\xd1\x9b\x9a\x8e\x8f\xc5\x8b\x9a\x8c\x8b\x9a\x8d\x9c\x90\x8d\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_DEQP,
 "\x9b\x9a\x8e\x8f\xd2\x98\x93\x9a\x8c\xcc\xd1\x9a\x87\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_DEQP,
 "\x9b\x9a\x8e\x8f\xd2\x98\x93\x9a\x8c\xcc\xce\xd1\x9a\x87\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_SF4,
 "\x95\x8f\xd1\x9c\x90\xd1\x9c\x9e\x8f\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x8c\x99\xcb\xa0\x91\x90\xa0\x9c\x90\x92\x8f\x8d\x9a\x8c\x8c\xa0\x98\x90\x90\x98\x93\x9a\x8f\x93\x9e\x86",
            gcvFALSE
        },

        {
            gcePATCH_MGOHEAVEN2,
 "\x9c\x90\x92\xd1\x9b\x9e\x89\x96\x9b\x9e\x92\x9e\x9b\x90\xd1\xb2\x9a\x8b\x9e\x93\xb8\x9a\x9e\x8d\xb0\x8a\x8b\x9a\x8d\xb7\x9a\x9e\x89\x9a\x91\xcd",
            gcvFALSE
        },
        {
            gcePATCH_SILIBILI,
 "\x9c\x90\x92\xd1\x8c\x96\x93\x96\xd1\x9d\x96\x93\x96",
            gcvFALSE
        },
        {
            gcePATCH_ELEMENTSDEF,
 "\x9c\x90\x92\xd1\x99\x9a\x9e\x92\x9d\x9a\x8d\xd1\x9a\x93\x9a\x92\x9a\x91\x8b\x8c\x9b\x9a\x99",
            gcvFALSE
        },
        {
            gcePATCH_GLOFTKRHM,
 "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xbe\xb1\xb2\xaf\xd1\xb8\x93\x90\x99\x8b\xb4\xad\xb7\xb2",
            gcvFALSE
        },
        {
            gcvPATCH_AIRNAVY,
 "\x96\x8b\xd1\x8d\x90\x8d\x8b\x90\x8c\xd1\x9e\x96\x8d\x91\x9e\x89\x86\x99\x96\x98\x97\x8b\x9a\x8d\x8c",
            gcvFALSE
        },
        {
            gcvPATCH_F18NEW,
 "\x96\x8b\xd1\x8d\x90\x8d\x8b\x90\x8c\xd1\x99\xce\xc7\x9c\x9e\x8d\x8d\x96\x9a\x8d\x93\x9e\x91\x9b\x96\x91\x98",
            gcvFALSE
        },
        {
            gcvPATCH_SUMSUNG_BENCH,
 "\x9c\x90\x92\xd1\x8c\x9e\x92\x8c\x8a\x91\x98\xd1\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\x8c\xc5\x9a\x8c\xce",
            gcvFALSE
        },

        {
             gcvPATCH_ROCKSTAR_MAXPAYNE,
              /* "com.rockstar.maxpayne" */ "\x9c\x90\x92\xd1\x8d\x90\x9c\x94\x8c\x8b\x9e\x8d\xd1\x92\x9e\x87\x8f\x9e\x86\x91\x9a",
             gcvFALSE
        },
        {
            gcvPATCH_TITANPACKING,
             /* "com.vascogames.titanicparking" */ "\x9c\x90\x92\xd1\x89\x9e\x8c\x9c\x90\x98\x9e\x92\x9a\x8c\xd1\x8b\x96\x8b\x9e\x91\x96\x9c\x8f\x9e\x8d\x94\x96\x91\x98",
            gcvFALSE
        },
        {
            gcvPATCH_OES20SFT,
#if defined(ANDROID)
 "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x8e\x9e\xd1\x90\x9a\x8c\xcd\xcf\x8c\x99\x8b",
#elif defined(UNDER_CE)
          "\xac\xb9\xab\xa0\xcd\xd1\xcf\xa0\xbc\xba\xd1\x9a\x87\x9a",
#else /* #elif defined(__QNXNTO__) */
                  "\xac\xb9\xab\xcd\xd1\xcf",
#endif
            gcvFALSE
        },
        {
            gcvPATCH_OES30SFT,
            /* "com.android.qa.oes30sft" */
            "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x8e\x9e\xd1\x90"
            "\x9a\x8c\xcc\xcf\x8c\x99\x8b",
            gcvFALSE
        },
        {
            gcvPATCH_FRUITNINJA,
            /* "com.halfbrick.fruitninja" */ "\x9c\x90\x92\xd1\x97\x9e\x93\x99\x9d\x8d\x96\x9c\x94\xd1\x99\x8d\x8a\x96\x8b\x91\x96\x91\x95\x9e",
            gcvFALSE
        },

#if defined(UNDER_CE)
        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9e\x93\x93\x90\x9c\x9e\x8b\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9e\x8f\x96",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9e\x8b\x90\x92\x96\x9c\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9d\x9e\x8c\x96\x9c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9d\x9e\x8c\x96\x9c\xa0\x94\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9d\x8a\x99\x99\x9a\x8d\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x93\xbc\x90\x8f\x86\xb6\x92\x9e\x98\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x93\xb8\x9a\x8b\xb6\x91\x99\x90",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x93\xad\x9a\x9e\x9b\xa8\x8d\x96\x8b\x9a\xb6\x92\x9e\x98\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x90\x92\x92\x90\x91\x99\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x90\x92\x8f\x96\x93\x9a\x8d",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x90\x92\x8f\x8a\x8b\x9a\x96\x91\x99\x90",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x90\x91\x8b\x8d\x9e\x9c\x8b\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9c\x90\x91\x89\x9a\x8d\x8c\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x9a\x89\x9a\x91\x8b\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x98\x9a\x90\x92\x9a\x8b\x8d\x96\x9c\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x97\x9e\x93\x99",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x96\x91\x8b\x9a\x98\x9a\x8d\xa0\x90\x8f\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x94\x9a\x8d\x91\x9a\x93\xa0\x96\x92\x9e\x98\x9a\xa0\x92\x9a\x8b\x97\x90\x9b\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x94\x9a\x8d\x91\x9a\x93\xa0\x8d\x9a\x9e\x9b\xa0\x88\x8d\x96\x8b\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x92\x9e\x8b\x97\xa0\x9d\x8d\x8a\x8b\x9a\xa0\x99\x90\x8d\x9c\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x92\x8a\x93\x8b\x96\x8f\x93\x9a\xa0\x9b\x9a\x89\x96\x9c\x9a\xa0\x9c\x90\x91\x8b\x9a\x87\x8b",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x91\x9a\x88\xa0\x8b\x9a\x8c\x8b\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8f\x8d\x90\x99\x96\x93\x96\x91\x98",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8d\x9a\x93\x9e\x8b\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8c\x9a\x93\x9a\x9c\x8b",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x98\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x8f\x93\x9e\x8b\x99\x90\x8d\x92\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x9a\x8c\x8b\xa0\x97\x9a\x9e\x9b\x9a\x8d\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x9a\x8c\x8b\xa0\x90\x8f\x9a\x91\x9c\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x8b\x97\x8d\x9a\x9e\x9b\xa0\x9b\x96\x92\x9a\x91\x8c\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x89\x9a\x9c\xa0\x9e\x93\x96\x98\x91",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x93\xce\xce\xa0\x9c\x90\x91\x99\x90\x8d\x92\xa0\x89\x9a\x9c\xa0\x8c\x8b\x9a\x8f",
            gcvFALSE
        },
        {
            gcvPATCH_FM_OES_PLAYER,
            /* "fm_oes_player.exe" */ "\x99\x92\xa0\x90\x9a\x8c\xa0\x8f\x93\x9e\x86\x9a\x8d\xd1\x9a\x87\x9a",
            gcvFALSE
        },
#else
        {
            gcvPATCH_OCLCTS,
 "\x9c\x90\x92\x8f\x8a\x8b\x9a\x96\x91\x99\x90",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9d\x9e\x8c\x96\x9c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9e\x8f\x96",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x90\x92\x8f\x96\x93\x9a\x8d",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x90\x92\x92\x90\x91\x99\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x98\x9a\x90\x92\x9a\x8b\x8d\x96\x9c\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x8d\x9a\x93\x9e\x8b\x96\x90\x91\x9e\x93\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x8b\x97\x8d\x9a\x9e\x9b\xa0\x9b\x96\x92\x9a\x91\x8c\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x92\x8a\x93\x8b\x96\x8f\x93\x9a\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9e\x8b\x90\x92\x96\x9c\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x8f\x8d\x90\x99\x96\x93\x96\x91\x98",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9a\x89\x9a\x91\x8b\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9e\x93\x93\x90\x9c\x9e\x8b\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x89\x9a\x9c\x9e\x93\x96\x98\x91",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x89\x9a\x9c\x8c\x8b\x9a\x8f",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9d\x8a\x99\x99\x9a\x8d\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x98\x9a\x8b\xa0\x96\x91\x99\x90",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x94\x9a\x8d\x91\x9a\x93\xa0\x96\x92\x9e\x98\x9a\xa0\x92\x9a\x8b\x97\x90\x9b\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x96\x92\x9e\x98\x9a\xa0\x8c\x8b\x8d\x9a\x9e\x92\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x9c\x90\x8f\x86\xa0\x96\x92\x9e\x98\x9a\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x8d\x9a\x9e\x9b\xa0\x88\x8d\x96\x8b\x9a\xa0\x96\x92\x9e\x98\x9a\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x97\x9a\x9e\x9b\x9a\x8d\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x8f\x93\x9e\x8b\x99\x90\x8d\x92\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x93\xa0\x98\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x90\x8f\x9a\x91\x9c\x93\xa0\x97",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x98\x93",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x8c\x9a\x93\x9a\x9c\x8b",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x9c\x90\x91\x89\x9a\x8d\x8c\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9c\x90\x91\x8b\x8d\x9e\x9c\x8b\x96\x90\x91\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x9d\x8d\x8a\x8b\x9a\x99\x90\x8d\x9c\x9a",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\x8b\x9a\x8c\x8b\xa0\x96\x91\x8b\x9a\x98\x9a\x8d\xa0\x90\x8f\x8c",
            gcvFALSE
        },

        {
            gcvPATCH_OCLCTS,
 "\xab\x9a\x8c\x8b\xa0\x97\x9e\x93\x99",
            gcvFALSE
        },
#endif
        {
            gcvPATCH_A8HP,
            "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xb8\xbe\xb1\xbb\xd1\xb8\x93\x90\x99\x8b\xbe\xc7\xb7\xaf",
            gcvFALSE
        },
        {
            gcvPATCH_BASEMARKX,
 "\xbd\x9e\x8c\x9a\x92\x9e\x8d\x94\xa7",
            gcvFALSE
        },
        {
            gcvPATCH_WISTONESG,
 "\x9c\x91\xd1\x88\x96\x8c\x8b\x90\x91\x9a\xd1\x8c\x98",
            gcvFALSE
        },
        {
            gcvPATCH_SPEEDRACE,
 "\x9a\x8a\xd1\x9b\x8d\x9a\x9e\x92\x8a\x8f\xd1\x8c\x8f\x9a\x9a\x9b\x8d\x9e\x9c\x96\x91\x98\x8a\x93\x8b\x96\x92\x9e\x8b\x9a\x99\x8d\x9a\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_FSBHAWAIIF,
 "\x9c\x90\x92\xd1\x8b\x97\x9a\x8b\x96\x8c\x98\x9e\x92\x9a\x8c\xd1\x99\x8c\x9d\x90\x9a\x96\x91\x98\x97\x9e\x88\x9e\x96\x96\x99\x8d\x9a\x9a",
            gcvFALSE
        },
        {
            gcvPATCH_CKZOMBIES2,
 "\x9c\x90\x92\xd1\x98\x93\x8a\xd1\x9c\x94\x85\x90\x92\x9d\x96\x9a\x8c\xcd",
            gcvFALSE
        },
        {
            gcvPATCH_A8CN,
 "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xbe\xb1\xb2\xaf\xd1\xb8\x93\x90\x99\x8b\xbe\xc7\xbc\xb1",
            gcvFALSE
        },
        {
            gcvPATCH_EADGKEEPER,
 "\x9c\x90\x92\xd1\x9a\x9e\xd1\x98\x9e\x92\x9a\xd1\x9b\x8a\x91\x98\x9a\x90\x91\x94\x9a\x9a\x8f\x9a\x8d\xa0\x91\x9e",
            gcvFALSE
        },
        {
            gcvPATCH_OESCTS,
            /*"com.android.cts.stub" */ "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x8b\x8c\xd1\x8c\x8b\x8a\x9d",
            gcvFALSE
        },
        {
            gcvPATCH_GANGSTAR,
            /* "com.gameloft.android.ANMP.GloftGGHM" */ "\x9c\x90\x92\xd1\x98\x9e\x92\x9a\x93\x90\x99\x8b\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\xbe\xb1\xb2\xaf\xd1\xb8\x93\x90\x99\x8b\xb8\xb8\xb7\xb2",
            gcvFALSE
        },
        {
             gcvPATCH_WHRKYZIXOVAN,
             /* "com.thetisgames.whrkyzixovan" */ "\x9c\x90\x92\xd1\x8b\x97\x9a\x8b\x96\x8c\x98\x9e\x92\x9a\x8c\xd1\x88\x97\x8d\x94\x86\x85\x96\x87\x90\x89\x9e\x91",
             gcvFALSE
        },
        {
            gcvPATCH_NAMESGAS,
            /* "com.namcobandaigames.sgas" */ "\x9c\x90\x92\xd1\x91\x9e\x92\x9c\x90\x9d\x9e\x91\x9b\x9e\x96\x98\x9e\x92\x9a\x8c\xd1\x8c\x98\x9e\x8c",
            gcvFALSE
        },
        {
            gcvPATCH_AFTERBURNER,
            /* "com.sega.afterburner" */ "\x9c\x90\x92\xd1\x8c\x9a\x98\x9e\xd1\x9e\x99\x8b\x9a\x8d\x9d\x8a\x8d\x91\x9a\x8d",
            gcvFALSE
        },
        {
            gcvPATCH_UIMARK,
             /* "com.rightware.uimark" */ "\x9c\x90\x92\xd1\x8d\x96\x98\x97\x8b\x88\x9e\x8d\x9a\xd1\x8a\x96\x92\x9e\x8d\x94",
            gcvFALSE
        },
        {
             gcvPATCH_BASEMARKOSIICN,
             /* "com.rightware.BasemarkOSIICN" */ "\x9c\x90\x92\xd1\x8d\x96\x98\x97\x8b\x88\x9e\x8d\x9a\xd1\xbd\x9e\x8c\x9a\x92\x9e\x8d\x94\xb0\xac\xb6\xb6\xbc\xb1",
             gcvFALSE
        },
#if defined(ANDROID)
        {
            gcePATCH_ANDROID_CTS_MEDIA_PRESENTATIONTIME,
            /* "com.android.cts.media" */ "\x9c\x90\x92\xd1\x9e\x91\x9b\x8d\x90\x96\x9b\xd1\x9c\x8b\x8c\xd1\x92\x9a\x9b\x96\x9e",
             gcvFALSE
        },
#endif

#if defined(ANDROID)
        {
            gcvPATCH_ANDROID_COMPOSITOR,
            /* "surfaceflinger" */ "\x8c\x8a\x8d\x99\x9e\x9c\x9a\x99\x93\x96\x91\x98\x9a\x8d",
             gcvFALSE
        },
        {
             gcvPATCH_WATER2_CHUKONG,
             /* "com.chukong.watertwo_chukong.wdj" */
              "\x9c\x90\x92\xd1\x9c\x97\x8a\x94\x90\x91\x98\xd1\x88\x9e\x8b\x9a\x8d\x8b\x88\x90\xa0\x9c\x97\x8a\x94\x90\x91\x98\xd1\x88\x9b\x95",
             gcvFALSE
        }
#endif
    };

#if defined(ANDROID)
    gcoOS_SymbolsList programList[] =
    {
        {
            gcvPATCH_GLBM21,
            {
 "\xb5\x9e\x89\x9e\xa0\x9c\x90\x92\xa0\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xa0\x9c\x90\x92\x92\x90\x91\xa0\xb8\xb3\xbd\xb1\x9e\x8b\x96\x89\x9a\xb9\x8a\x91\x9c\x8b\x96\x90\x91\x8c\xa0\x92\x9e\x96\x91",
 "\xa0\xa5\xb1\xce\xc7\xab\xba\xac\xab\xa0\xb6\xb1\xab\xba\xad\xb9\xbe\xbc\xba\xa0\xcd\xa0\xce\xce\xcf\xbc\x8d\x9a\x9e\x8b\x9a\xab\x9a\x8c\x8b\xba\xad\xb4\xc9\xab\x9a\x8c\x8b\xb6\x9b",
 "\xa0\xa5\xb1\xce\xc7\xab\xba\xac\xab\xa0\xb6\xb1\xab\xba\xad\xb9\xbe\xbc\xba\xa0\xcd\xa0\xce\xce\xcc\xac\x9a\x8b\xad\x9a\x8c\x90\x93\x8a\x8b\x96\x90\x91\xba\x96\x96",
 "\xa0\xa5\xb1\xb4\xcc\xb8\xb3\xbd\xce\xce\xbe\x8f\x8f\x93\x96\x9c\x9e\x8b\x96\x90\x91\xcd\xc7\xbb\x9a\x99\x9e\x8a\x93\x8b\xb0\x91\x8c\x9c\x8d\x9a\x9a\x91\xa9\x96\x9a\x88\x8f\x90\x8d\x8b\xa8\x96\x9b\x8b\x97\xba\x89",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xc8\xab\x9a\x87\x8b\x8a\x8d\x9a\xce\xca\x8c\x9a\x8b\xb2\x96\x91\xb2\x9e\x98\xb9\x96\x93\x8b\x9a\x8d\xba\x95\x95",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xc9\xb0\xac\xb6\x92\x8f\x93\xce\xcd\xb8\x9a\x8b\xb8\x93\x90\x9d\x9e\x93\xb9\xbd\xb0\xba\x89",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xcc\xb9\xbd\xb0\xce\xca\x96\x91\x96\x8b\xa0\x9b\x9a\x8f\x8b\x97\xa0\x90\x91\x93\x86\xba\x95\x95",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xce\xbe\x8f\x8f\x93\x96\x9c\x9e\x8b\x96\x90\x91\xce\xcf\xbc\x93\x9a\x9e\x8d\xab\x9a\x8c\x8b\x8c\xba\x89",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xcf\xae\x8a\x9e\x8b\x9a\x8d\x91\x96\x90\x91\xce\xcc\x99\x8d\x90\x92\xbe\x91\x98\x93\x9a\xbe\x87\x96\x8c\xba\x99\xad\xb4\xb1\xac\xa0\xc7\xa9\x9a\x9c\x8b\x90\x8d\xcc\xbb\xba",
            }
        },

        {
            gcvPATCH_GLBM25,
            {
 "\xb5\x9e\x89\x9e\xa0\x9c\x90\x92\xa0\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xa0\x9c\x90\x92\x92\x90\x91\xa0\xb8\xb3\xbd\xb1\x9e\x8b\x96\x89\x9a\xb9\x8a\x91\x9c\x8b\x96\x90\x91\x8c\xa0\x92\x9e\x96\x91",
 "\xb5\x9e\x89\x9e\xa0\x9c\x90\x92\xa0\x98\x93\x9d\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xa0\x9c\x90\x92\x92\x90\x91\xa0\xb8\xb3\xbd\xb1\x9e\x8b\x96\x89\x9a\xb9\x8a\x91\x9c\x8b\x96\x90\x91\x8c\xa0\xb6\x8c\xbd\x9e\x8b\x8b\x9a\x8d\x86\xab\x9a\x8c\x8b\xaa\x8f\x93\x90\x9e\x9b\x9e\x9d\x93\x9a",
 "\xab\x8d\x96\x9e\x91\x98\x93\x9a\xab\x9a\x87\xb9\x8d\x9e\x98\x92\x9a\x91\x8b\xb3\x96\x8b\xab\x9a\x8c\x8b\xa0\x99\x8d\x9e\x98\x92\x9a\x91\x8b\xac\x97\x9e\x9b\x9a\x8d",
 "\xa0\xa5\xb1\xcd\xcb\xab\x8d\x96\x9e\x91\x98\x93\x9a\xab\x9a\x87\xa9\x9a\x8d\x8b\x9a\x87\xb3\x96\x8b\xab\x9a\x8c\x8b\xce\xc6\x98\x9a\x8b\xaa\x91\x96\x99\x90\x8d\x92\xb3\x90\x9c\x9e\x8b\x96\x90\x91\x8c\xba\x89",
            }
        },

        {
            gcvPATCH_GLBM27,
            {
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xcc\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xc8\xbc\xce\xba\x89",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xcc\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xc8\xbc\xcd\xba\x89",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xcc\xb8\xb3\xbd\x9a\x91\x9c\x97\x92\x9e\x8d\x94\xcd\xc8\xce\xcf\xbc\x8d\x9a\x9e\x8b\x9a\xab\x9a\x8c\x8b\xba\x96",
 "\xab\x8d\x96\x9e\x91\x98\x93\x9a\xab\x9a\x87\xb9\x8d\x9e\x98\x92\x9a\x91\x8b\xb3\x96\x8b\xab\x9a\x8c\x8b\xa0\x99\x8d\x9e\x98\x92\x9a\x91\x8b\xac\x97\x9e\x9b\x9a\x8d",
            }
        },

        {
            gcvPATCH_GFXBENCH,
            {
 "\xa0\xa5\xb1\xce\xce\xbd\x9e\x8b\x8b\x9a\x8d\x86\xab\x9a\x8c\x8b\xbc\xcd\xba\xad\xb4\xce\xcb\xab\x9a\x8c\x8b\xbb\x9a\x8c\x9c\x8d\x96\x8f\x8b\x90\x8d\xaf\xb1\xcc\xb8\xb3\xbd\xc7\xab\x9a\x8c\x8b\xbd\x9e\x8c\x9a\xba\x95",
 "\xa0\xa5\xb1\xce\xce\xab\x9a\x8c\x8b\xa8\x8d\x9e\x8f\x8f\x9a\x8d\xbc\xce\xba\xad\xb4\xce\xcb\xab\x9a\x8c\x8b\xbb\x9a\x8c\x9c\x8d\x96\x8f\x8b\x90\x8d\xaf\xb1\xcc\xb8\xb3\xbd\xc7\xab\x9a\x8c\x8b\xbd\x9e\x8c\x9a\xba",
 "\xa0\xa5\xb1\xce\xcd\xae\x8a\x9e\x93\x96\x8b\x86\xb2\x9e\x8b\x9c\x97\xbc\xce\xba\xad\xb4\xce\xcb\xab\x9a\x8c\x8b\xbb\x9a\x8c\x9c\x8d\x96\x8f\x8b\x90\x8d\xaf\xb1\xcc\xb8\xb3\xbd\xc7\xab\x9a\x8c\x8b\xbd\x9e\x8c\x9a\xba",
 "\xa0\xa5\xb1\xce\xcc\xb8\xb3\xbd\xa0\xac\x9c\x9a\x91\x9a\xa0\xba\xac\xcc\xce\xcc\xb2\x90\x89\x9a\xaf\x9e\x8d\x8b\x96\x9c\x93\x9a\x8c\xba\x89",
 "\xa0\xa5\xb1\xce\xca\xbc\xaf\xaa\xb0\x89\x9a\x8d\x97\x9a\x9e\x9b\xab\x9a\x8c\x8b\xbc\xce\xba\xad\xb4\xce\xcb\xab\x9a\x8c\x8b\xbb\x9a\x8c\x9c\x8d\x96\x8f\x8b\x90\x8d",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xce\xcc\xb8\xb3\xbd\xab\x9a\x87\x8b\x8a\x8d\x9a\xba\xac\xcd\xbc\xce\xba\xaf\xb4\xb1\xcc\xb4\xbc\xb3\xca\xb6\x92\x9e\x98\x9a\xba\xb1\xac\xce\xa0\xce\xcd\xa0\xab\x9a\x87\x8b\x8a\x8d\x9a\xab\x86\x8f\x9a\xba\x9d",
 "\xa0\xa5\xb1\xcc\xb8\xb3\xbd\xcc\xb9\xbd\xb0\xce\xc7\xb8\x9a\x8b\xac\x9c\x8d\x9a\x9a\x91\x8c\x97\x90\x8b\xb6\x92\x9e\x98\x9a\xba\xad\xb1\xcc\xb4\xbc\xb3\xca\xb6\x92\x9e\x98\x9a\xba",
            }
        },

        /* Support GFXBench 3.0.16 */
        {
            gcvPATCH_GFXBENCH,
            {
 "\xb5\x9e\x89\x9e\xa0\x91\x9a\x8b\xa0\x94\x96\x8c\x97\x90\x91\x8b\x96\xa0\x8c\x88\x96\x98\xa0\x8b\x9a\x8c\x8b\x99\x88\xb5\xb1\xb6\xa0\xba\xb8\xb3\xb8\x8d\x9e\x8f\x97\x96\x9c\x8c\xbc\x90\x91\x8b\x9a\x87\x8b\xa0\xce\x89\x9a\x8d\x8c\x96\x90\x91\xac\x8a\x8f\x8f\x90\x8d\x8b\x9a\x9b",
 "\xa0\xa5\xb1\xc7\x94\x96\x8c\x97\x90\x91\x8b\x96\xc8\x9e\x91\x9b\x8d\x90\x96\x9b\xc7\xaf\x93\x9e\x8b\x99\x90\x8d\x92\xce\xcd\x9d\x9e\x8b\x8b\x9a\x8d\x86\xb3\x9a\x89\x9a\x93\xba\x89",
 "\xa0\xa5\xac\x8b\xcb\x9a\x91\x9b\x93\xb6\x9c\xac\x8b\xce\xce\x9c\x97\x9e\x8d\xa0\x8b\x8d\x9e\x96\x8b\x8c\xb6\x9c\xba\xba\xad\xac\x8b\xce\xcc\x9d\x9e\x8c\x96\x9c\xa0\x90\x8c\x8b\x8d\x9a\x9e\x92\xb6\xab\xa0\xab\xcf\xa0\xba\xac\xc9\xa0",
 "\xb5\x9e\x89\x9e\xa0\x91\x9a\x8b\xa0\x94\x96\x8c\x97\x90\x91\x8b\x96\xa0\x8c\x88\x96\x98\xa0\x8b\x9a\x8c\x8b\x99\x88\xb5\xb1\xb6\xa0\xab\x9a\x8c\x8b\xb9\x9e\x9c\x8b\x90\x8d\x86\xa0\xce\x9c\x8d\x9a\x9e\x8b\x9a\xa0\xce\x8b\x9a\x8c\x8b",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x9a\x98\x86\x8f\x8b",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x9b\x8d\x96\x89\x9a\x8d",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x9e\x93\x8a",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x8b\x8d\x9a\x87",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x92\x9e\x91\x97\x9e\x8b\x8b\x9e\x91\xcc\xce",
 "\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x98\x93\xa0\x92\x9e\x91\x97\x9e\x8b\x8b\x9e\x91",
            }
        },

        {
            gcvPATCH_BM21,
            {
 "\x9c\x90\x93\x93\x9a\x9c\x8b\xa0\x8a\x91\x96\x99\x96\x9a\x9b\xa0\x8c\x97\x9e\x9b\x9a\x8d\xa0\x8b\x9a\x8c\x8b\xa0\x8f\x9e\x8d\x9e\x92\x9a\x8b\x9a\x8d\x8c\xa0\x99\x8d\x90\x92\xa0\x93\x8a\x9e",
 "\x8c\x9a\x8b\x8a\x8f\xa0\x8b\x8d\x96\x9e\x91\x98\x93\x9a\xa0\x9c\x90\x8a\x91\x8b\xa0\x8b\x9a\x8c\x8b\xa0\x8d\x9a\x91\x9b\x9a\x8d\x8c\x8b\x9e\x8b\x9a",
 "\x90\x9a\x8c\xa0\x8c\x9c\x9a\x91\x9a\xa0\x8f\x93\x9e\x86\x9a\x8d\xa0\x9d\x9a\x98\x96\x91\xa0\x8d\x9a\x91\x9b\x9a\x8d\xa0\x8f\x9e\x8c\x8c",
 "\x92\x9e\x94\x9a\xa0\x9e\x9b\x89\x9e\x91\x9c\x9a\x9b\xa0\x88\x90\x8d\x93\x9b\xa0\x8b\x9a\x8c\x8b\xa0\x98\x9a\x90\x92\x9a\x8b\x8d\x86",
 "\x8a\x8f\x9b\x9e\x8b\x9a\xa0\x8b\x8d\x96\x9e\x91\x98\x93\x9a\xa0\x9c\x90\x8a\x91\x8b\xa0\x8b\x9a\x8c\x8b",
 "\xa0\xa5\xcd\xcc\x8c\x8b\x8d\x9c\x90\x91\x89\xa0\x96\x8c\xa0\x97\x96\x9d\x96\x8b\xa0\x8c\x8b\x8d\x96\x91\x98\xaf\xb4\x9c",
 "\x90\x9a\x8c\xa0\x8c\x9c\x9a\x91\x9a\xa0\x8f\x93\x9e\x86\x9a\x8d\xa0\x9c\x8d\x9a\x9e\x8b\x9a",
            }
        },

        {
            gcvPATCH_MM07,
            {
 "\x9c\x90\x93\x93\x9a\x9c\x8b\xa0\x8a\x91\x96\x99\x96\x9a\x9b\xa0\x8c\x97\x9e\x9b\x9a\x8d\xa0\x8b\x9a\x8c\x8b\xa0\x8f\x9e\x8d\x9e\x92\x9a\x8b\x9a\x8d\x8c\xa0\x99\x8d\x90\x92\xa0\x93\x8a\x9e",
 "\x8c\x9a\x8b\x8a\x8f\xa0\x8b\x8d\x96\x9e\x91\x98\x93\x9a\xa0\x9c\x90\x8a\x91\x8b\xa0\x8b\x9a\x8c\x8b\xa0\x8d\x9a\x91\x9b\x9a\x8d\x8c\x8b\x9e\x8b\x9a",
 "\x90\x9a\x8c\xa0\x8c\x9c\x9a\x91\x9a\xa0\x8f\x93\x9e\x86\x9a\x8d\xa0\x9d\x9a\x98\x96\x91\xa0\x8d\x9a\x91\x9b\x9a\x8d\xa0\x8f\x9e\x8c\x8c",
 "\x92\x9e\x94\x9a\xa0\x9e\x9b\x89\x9e\x91\x9c\x9a\x9b\xa0\x88\x90\x8d\x93\x9b\xa0\x8b\x9a\x8c\x8b\xa0\x98\x9a\x90\x92\x9a\x8b\x8d\x86",
 "\x8a\x8f\x9b\x9e\x8b\x9a\xa0\x8b\x8d\x96\x9e\x91\x98\x93\x9a\xa0\x9c\x90\x8a\x91\x8b\xa0\x8b\x9a\x8c\x8b",
 "\x90\x9a\x8c\xa0\x8c\x9c\x9a\x91\x9a\xa0\x8f\x93\x9e\x86\x9a\x8d\xa0\x9c\x8d\x9a\x9e\x8b\x9a",
 "\x8c\x96\xa0\x90\x8f\x9a\x91\xa0\x9c\x90\x91\x8c\x90\x93\x9a",
 "\x8c\x8b\x8d\x9c\x90\x91\x89\xa0\x96\x8c\xa0\x97\x96\x9d\x96\x8b\xa0\x8c\x8b\x8d\x96\x91\x98",
            }
        },

        {
            gcvPATCH_MM06,
            {
 "\x8c\x9c\x9a\x91\x9a\xa0\x8f\x93\x9e\x86\x9a\x8d\xa0\x8c\x9a\x8b\xa0\x98\x9e\x92\x9a\x8b\x9a\x8c\x8b",
 "\x89\x96\x9a\x88\xa0\x9c\x9e\x9c\x97\x9a\x9b\xa0\x9e\x93\x8f\x97\x9e\xa0\x8b\x9a\x8c\x8b",
 "\x8c\x96\xa0\x9b\x9a\x99\x9e\x8a\x93\x8b\xa0\x9b\x96\x8c\x8f\x93\x9e\x86\xa0\x9c\x90\x91\x99\x96\x98\x8a\x8d\x9e\x8b\x96\x90\x91",
 "\xa0\xa5\xce\xc6\x9a\x87\x8b\x9a\x91\x8c\x96\x90\x91\xa0\x8c\x8a\x8f\x8f\x90\x8d\x8b\x9a\x9b\xaf\xb4\x9c",
 "\xa0\xa5\xcd\xc7\x8f\x9e\x8d\x8c\x9a\xa0\x9c\x90\x92\x92\x9e\x91\x9b\xa0\x93\x96\x91\x9a\xa0\x99\x8d\x90\x92\xa0\x92\x9e\x96\x91\x96\xaf\xaf\x9c",
 "\x98\x9a\x90\x92\x9a\x8b\x8d\x86\xa0\x9c\x8d\x9a\x9e\x8b\x9a\xa0\x99\x8a\x93\x93\x8c\x9c\x8d\x9a\x9a\x91\xa0\x8b\x8d\x96\x9e\x91\x98\x93\x9a\x8c",
 "\x93\x96\x98\x97\x8b\xa0\x8c\x9a\x8b\xa0\x9b\x96\x99\x99\x8a\x8c\x9a\xa0\x8c\x8f\x9a\x9c\x8a\x93\x9e\x8d",
            }
        },

        {
            gcvPATCH_BMGUI,
            {
 "\x9d\x9a\x8c\x8b\xac\x9c\x90\x8d\x9a\xac\x9c\x9a\x91\x9a\xb2\x9a\x91\x8a\xbd\xb8\xac\x8b\x8d\x96\x8f\xb2\x9e\x8b\x9a\x8d\x96\x9e\x93\x8c",
 "\x9d\x99\xb2\x9e\x8b\x9a\x8d\x96\x9e\x93\xbc\x90\x93\x93\x9a\x9c\x8b\x96\x90\x91\xb8\x9a\x8b\xbc\x90\x91\x99\x96\x98\x8a\x8d\x9e\x8b\x96\x90\x91\xaf\x9e\x8d\x9e\x92\x9a\x8b\x9a\x8d\x8c\xa0\x96\x91\x8b\x9a\x8d\x91\x9e\x93",
 "\x98\x9a\x90\x92\x9a\x8b\x8d\x86\xbc\x97\x8a\x91\x94\xb6\x91\x96\x8b\x96\x9e\x93\x96\x85\x9a\xb8\x9a\x90\x92\x9a\x8b\x8d\x86\xb9\x8d\x90\x92\xac\x8b\x8d\x9a\x9e\x92\xbd\x8a\x99\x99\x9a\x8d\x8c",
 "\x94\x85\x9e\xbe\x8f\x8f\x93\x96\x9c\x9e\x8b\x96\x90\x91\xac\x9a\x8b\xbd\x90\x8a\x91\x9b\x96\x91\x98\xbd\x90\x87\xa9\x96\x8c\x8a\x9e\x93\x96\x85\x9e\x8b\x96\x90\x91\xba\x91\x9e\x9d\x93\x9a\x9b",
 "\x94\x85\x9c\xbb\x86\x91\x9e\x92\x96\x9c\xbe\x8d\x8d\x9e\x86\xb2\x8a\x8b\x9e\x9d\x93\x9a\xb6\x8b\x9a\x8d\x9e\x8b\x90\x8d\xb8\x9a\x8b\xa9\x9e\x93\x8a\x9a\xa0\x8f\x8d\x96\x89\x9e\x8b\x9a",
 "\x94\x85\x8a\xbe\x87\x96\x8c\xbe\x93\x96\x98\x91\x9a\x9b\xbd\x90\x8a\x91\x9b\x96\x91\x98\xbd\x90\x87\xb9\x8d\x90\x92\xab\x8d\x9e\x91\x8c\x99\x90\x8d\x92\x9a\x9b\xbe\xbe\xbd\xbd",
 "\x92\x9e\x96\x91\xbe\x8f\x8f\x93\x96\x9c\x9e\x8b\x96\x90\x91\xaa\x96\xba\x89\x9a\x91\x8b\xb7\x9e\x91\x9b\x93\x9a\x8d",
 "\x8d\x9a\x9d\x8a\x96\x93\x9b\xaf\x93\x9e\x8b\x8b\x9a\x8d\xbe\x91\x9b\xbe\x8c\x8c\x96\x98\x91\xba\x91\x8b\x8d\x86\xbe\x91\x96\x92\x9e\x8b\x96\x90\x91",
 "\x8c\x9c\x8d\x9a\x9a\x91\xbc\x9e\x8f\x8b\x8a\x8d\x9a\xac\x9a\x8b\x8b\x96\x91\x98\x8c\xb9\x90\x8d\x88\x9e\x8d\x9b\xb1\x9a\x87\x8b\xb6\x91\x9b\x9a\x87",
 "\x8c\x90\x8d\x8b\xac\x9a\x8e\x8a\x9a\x91\x9c\x9a\xaf\x90\x96\x91\x8b\x9a\x8d\x8c\xbe\x9c\x9c\x90\x8d\x9b\x96\x91\x98\xab\x90\xac\x96\x85\x9a\xb3\x9e\x8d\x98\x9a\x8c\x8b\xb9\x96\x8d\x8c\x8b",
            }
        },

        {
            gcvPATCH_FISHNOODLE,
            {
 "\xa0\xa5\xce\xcf\xba\xb8\xb3\xba\x87\x8b\xb6\x91\x96\x8b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89",
 "\xa0\xa5\xce\xce\xb8\xb3\xcd\xcf\xb5\xb1\xb6\xb6\x91\x96\x8b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89",
 "\xa0\xa5\xce\xce\xaa\x8b\x96\x93\x96\x8b\x86\xb6\x91\x96\x8b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89",
 "\xa0\xa5\xc7\xba\xab\xbc\xce\xb6\x91\x96\x8b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xce\xcc\xbc\x9e\x93\x93\xb6\x91\x8b\xb2\x9a\x8b\x97\x90\x9b\xba\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xce\xc9\xbc\x9e\x93\x93\xb0\x9d\x95\x9a\x9c\x8b\xb2\x9a\x8b\x97\x90\x9b\xba\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xce\xc6\xbc\x9e\x93\x93\xac\x8b\x9e\x8b\x96\x9c\xb6\x91\x8b\xb2\x9a\x8b\x97\x90\x9b\xba\xaf\xc8\xa0\x95\x9c\x93\x9e\x8c\x8c\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xcd\xcf\xbc\x9e\x93\x93\xac\x8b\x9e\x8b\x96\x9c\xb3\x90\x91\x98\xb2\x9a\x8b\x97\x90\x9b\xba\xaf\xc8\xa0\x95\x9c\x93\x9e\x8c\x8c\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xcd\xcd\xbc\x9e\x93\x93\xac\x8b\x9e\x8b\x96\x9c\xb0\x9d\x95\x9a\x9c\x8b\xb2\x9a\x8b\x97\x90\x9b\xba\xaf\xc8\xa0\x95\x9c\x93\x9e\x8c\x8c\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xc7\xab\x97\x8d\x90\x88\xb1\x9a\x88\xba\xaf\xc8\xa0\x95\x9c\x93\x9e\x8c\x8c\xaf\xb4\x9c",
            }
        },

        /* Symbol list for Antutu 3.x */
        {
            gcvPATCH_ANTUTU,
            {
 "\xa0\xa5\xb1\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xcd\xcc\xa0\xa0\x99\x8a\x91\x9b\x9e\x92\x9a\x91\x8b\x9e\x93\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xbb\xcd\xba\x89",
 "\xa0\xa5\xb1\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xce\xc6\xa0\xa0\x8f\x90\x96\x91\x8b\x9a\x8d\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xbb\xcd\xba\x89",
 "\xa0\xa5\xb1\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xce\xc8\xa0\xa0\x9c\x93\x9e\x8c\x8c\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xbb\xce\xba\x89",
 "\xa0\xa5\xb1\xcc\xb9\xcc\xbb\xcc\xb9\x90\x98\xce\xcc\x8c\x9a\x8b\xb9\x90\x98\xbb\x9a\x91\x8c\x96\x8b\x86\xba\x99",
 "\xa0\xa5\xb1\xcc\xb9\xcc\xbb\xcb\xb9\x90\x91\x8b\xce\xcf\x9b\x8d\x9e\x88\xac\x8b\x8d\x96\x91\x98\xba\x96\x96\x96\x96\xaf\xb4\x9c\xce\xcf\xbb\x8d\x9e\x88\xbe\x91\x9c\x97\x90\x8d",
 "\xa0\xa5\xb1\xcc\xb9\xcc\xbb\xca\xb6\x92\x9e\x98\x9a\xce\xcd\x99\x9a\x8b\x9c\x97\xaf\x9e\x93\x93\x9a\x8b\x9a\xba\xaf\xc8\xa0\xa0\x8c\xb9\xb6\xb3\xba\xaf\xca\xbc\x90\x93\x90\x8d\x96",
 "\xa0\xa5\xb1\xb4\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xcd\xce\xa0\xa0\x89\x92\x96\xa0\x9c\x93\x9e\x8c\x8c\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xce\xcd\xa0\xa0\x9b\x90\xa0\x9b\x86\x91\x9c\x9e\x8c\x8b\xba\x96\xb1\xac\xa0\xce\xc8\xa0\xa0\x9c\x93\x9e\x8c\x8c\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xce\xcf\xa0\xa0\x8c\x8a\x9d\xa0\x94\x96\x91\x9b\xba\xaf\xb4\xac\xce\xa0\xaf\xb4\x89\xac\xcb\xa0\xac\xc9\xa0\xad\xb1\xac\xce\xa0\xce\xc9\xa0\xa0\x9b\x86\x91\x9c\x9e\x8c\x8b\xa0\x8d\x9a\x8c\x8a\x93\x8b\xba",
 "\xa0\xa5\xb1\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xcd\xcd\xb8\x9a\x8b\xbb\x96\x8d\x9a\x9c\x8b\xbd\x8a\x99\x99\x9a\x8d\xbe\x9b\x9b\x8d\x9a\x8c\x8c\xba\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b",
 "\xa0\xa5\xb1\xb4\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xce\xc8\xa0\xa0\x9c\x93\x9e\x8c\x8c\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xcd\xcf\xa0\xa0\x9b\x90\xa0\x99\x96\x91\x9b\xa0\x8f\x8a\x9d\x93\x96\x9c\xa0\x8c\x8d\x9c\xba\x96\xaf\xb4\x89\xaf\xb4\xac\xcf\xa0\xac\xcd\xa0",
 "\xa0\xa5\xac\x8b\xce\xcb\xa0\xa0\x9c\x90\x91\x89\x9a\x8d\x8b\xa0\x8b\x90\xa0\x89\xb6\x9b\xba\x89\xaf\xb4\x9c\xad\xab\xa0\xad\xac\x8b\xce\xcd\xa0\xb6\x90\x8c\xa0\xb6\x90\x8c\x8b\x9e\x8b\x9a\xad\xb4\xaf\x96",
            }
        },

        /* Symbol list for Antutu 4.x */
        {
            gcvPATCH_ANTUTU4X,
            {
 "\x9d\x9a\x91\x9c\x97\xa0\x9b\x9e\x8b\x9e\xa0\x8f\x8d\x90\x9c\x9a\x8c\x8c\x96\x91\x98",
 "\x9e\x91\x9b\x8d\x90\x96\x9b\xa0\x98\x9a\x8b\xbc\x8f\x8a\xb6\x9b\xbe\x8d\x92",
 "\x9d\x9a\x91\x9c\x97\xa0\x8c\x9c\x90\x8d\x9a\xa0\x97\x86\x9d\x8d\x96\x9b",
 "\x9b\x9a\x9c\xa0\x8c\x8b\x8d\x96\x91\x98\xa0\x90\x8f\x9a\x91\x98\x93\x9a\x8c\xcc",
 "\x9b\x9a\x8c\xa0\x9a\x91\x9c\x8d\x86\x8f\x8b\x96\x90\x91",
 "\x98\x9a\x91\x9a\x8d\x9e\x8b\x9a\xa0\x8b\x9a\x8c\x8b\xa0\x9b\x9e\x8b\x9e\xa0\x99\x96\x93\x9a",
 "\xb5\x9e\x89\x9e\xa0\x9c\x90\x92\xa0\x9d\x9e\x9b\x93\x90\x98\x96\x9c\xa0\x98\x9b\x87\xa0\x98\x8d\x9e\x8f\x97\x96\x9c\x8c\xa0\x98\xcd\x9b\xa0\xb8\x9b\x87\xcd\xbb\xaf\x96\x87\x92\x9e\x8f\xa0\x9b\x8d\x9e\x88\xbc\x96\x8d\x9c\x93\x9a",
 "\xa0\xa5\xab\xa9\xb1\xce\xcf\xa0\xa0\x9c\x87\x87\x9e\x9d\x96\x89\xce\xcd\xcc\xa0\xa0\x99\x8a\x91\x9b\x9e\x92\x9a\x91\x8b\x9e\x93\xa0\x8b\x86\x8f\x9a\xa0\x96\x91\x99\x90\xba",
            }
        },

        {
            gcvPATCH_NENAMARK2,
            {
 "\xa0\xa5\xce\xcc\x91\x9a\x88\xb5\x9e\x89\x9e\xb0\x9d\x95\x9a\x9c\x8b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xaf\xc8\xa0\x95\x9c\x93\x9e\x8c\x8c\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x85",
 "\xa0\xa5\xce\xca\x8c\x9a\x8b\xb8\x93\x90\x9d\x9e\x93\xb3\x90\x98\x98\x9a\x8d\xaf\xb9\x89\xaf\x89\xc7\xb3\x90\x98\xb3\x9a\x89\x9a\x93\xaf\xb4\x9c\xac\xcd\xa0\xac\x8b\xc6\xa0\xa0\x89\x9e\xa0\x93\x96\x8c\x8b\xba\xac\xa0",
 "\xa0\xa5\xcd\xcd\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xb5\xb1\xb6\xa0\x90\x91\xb9\x96\x91\x96\x8c\x97\x9a\x9b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x99\xad\xb4\xc8\xb7\x9e\x8c\x97\xb2\x9e\x8f\xb6\xc9\xac\x8b\x8d\x96\x91\x98\xac\xc9\xa0\xba\xad\xb4\xce\xce\xad\x9a\x91\x9b\x9a\x8d\xab\x8d\x9e\x9c\x9a",
 "\xa0\xa5\xc6\x9d\x8a\x96\x93\x9b\xab\x8d\x9a\x9a\xad\xce\xca\xad\x9e\x91\x9b\x90\x92\xb8\x9a\x91\x9a\x8d\x9e\x8b\x90\x8d\xaf\xb4\xce\xcf\xbd\x8d\x9e\x91\x9c\x97\xac\x8f\x9a\x9c\x96\xad\xb4\xc8\xb2\x9e\x8b\xcb\x87\xcb\x99\xad\xc9\xa9\x9a\x9c\x8b\x90\x8d\xb6\xce\xcf\xbd\x8d\x9e\x91\x9c\x97\xb6\x91\x99\x90\xb3\x96\xce\xcf\xba\xba\xad\xac\xc8\xa0\xb6\xac\xcb\xa0\xb3\x96\xce\xcf\xba\xba",
 "\xa0\xa5\xb1\xce\xcf\xb9\x9b\xad\x9a\x8c\x90\x8a\x8d\x9c\x9a\xce\xce\x98\x9a\x8b\xb6\x91\x8b\x9a\x8d\x91\x9e\x93\xba\xaf\x96\xaf\x95\xac\xce\xa0",
 "\xa0\xa5\xb1\xce\xce\xbd\x8a\x8b\x8b\x90\x91\xb8\x8d\x90\x8a\x8f\xcb\x96\x91\x96\x8b\xba\xaf\xc9\xac\x86\x8c\x8b\x9a\x92\xaf\xc6\xb8\xb3\xbc\x90\x91\x8b\x9a\x87\x8b\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xad\xb4\xc9\xa9\x9a\x9c\x8b\x90\x8d\xb6\xce\xcf\xbd\x8a\x8b\x8b\x90\x91\xac\x8f\x9a\x9c\xb3\x96\xce\xcf\xba\xba",
 "\xa0\xa5\xb1\xce\xce\xbd\x8a\x8b\x8b\x90\x91\xb8\x8d\x90\x8a\x8f\xc8\x96\x91\x96\x8b\xac\x9a\x8b\xba\xaf\xc9\xac\x86\x8c\x8b\x9a\x92\xaf\xc6\xb8\xb3\xbc\x90\x91\x8b\x9a\x87\x8b\xad\xb4\xc9\xa9\x9a\x9c\x8b\x90\x8d\xb6\xce\xcf\xbd\x8a\x8b\x8b\x90\x91\xac\x8f\x9a\x9c\xb3\x96\xce\xcf\xba\xba",
 "\xa0\xa5\xb1\xce\xce\xbb\x8a\x92\x92\x86\xac\x86\x8c\x8b\x9a\x92\xcd\xcf\x8d\x9a\x98\x96\x8c\x8b\x9a\x8d\xb6\x92\x9e\x98\x9a\xbb\x9a\x9c\x90\x9b\x9a\x8d\xba\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xaf\xb9\xaf\xce\xcd\xb6\x92\x9e\x98\x9a\xbb\x9a\x9c\x90\x9b\x9a\x8d\xc9\xad\x9a\x99\xaf\x8b\x8d\xb6\xc7\xad\x9a\x8c\x90\x8a\x8d\x9c\x9a\xba\xba",
 "\xa0\xa5\xb1\xce\xce\xb8\xb3\xac\x8b\x9e\x8b\x9a\xb6\x92\x8f\x93\xce\xce\x93\x90\x9e\x9b\xab\x9a\x87\x8b\x8a\x8d\x9a\xba\xaf\xc9\xac\x86\x8c\x8b\x9a\x92\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xce\xcd\xab\x9a\x87\x8b\x8a\x8d\x9a\xb9\x93\x9e\x98\x8c\xac\xcb\xa0",
 "\xa0\xa5\xb1\xce\xce\xac\x9a\x8b\x8a\x8f\xac\x97\x9e\x9b\x9a\x8d\xb6\xb1\xce\xce\xbc\x93\x90\x8a\x9b\xac\x86\x8c\x8b\x9a\x92\xce\xce\xbc\x93\x90\x8a\x9b\xac\x97\x9e\x9b\x9a\x8d\xba\xba\xcb\x9e\x8b\x8b\x8d\xb6\xce\xca\xac\x97\x9e\x9b\x9a\x8d\xbe\x8b\x8b\x8d\x96\x9d\x8a\x8b\x9a\xb6\xb3\x96\xca\xce\xcd\xce\xba\xb3\x96\xcb\xba\xb3\x96\xcf\xba\xba\xba\xba\xad\xac\xcd\xa0\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xad\xab\xa0",
            }
        },

        {
            gcvPATCH_BM3,
            {
 "\x8b\x8d\x86\x9e\x93\x93\x8b\x9e\x9d\x93\x9a\x8c\xa0\xcc\x9d\x96\x8b\x8b\x9e\x9d\x93\x9a\xa0\x9e\x93\x93\xa0\x8c\x8a\x9d\x9d\x93\x90\x9c\x94\x8c\xa0\x8a\x8c\x96\x91\x98\xa0\x8f\x8d\x9a\x9c\x9e\x93\x9c\xa0\x8f\x9a\x8d\x9c\x9a\x8f\x8b\x8a\x9e\x93\xce\xcf\xcf\xcf",
 "\x94\x85\x8a\xba\x91\x98\x96\x91\x9a\xb6\x8c\xbe\x93\x93\x90\x9c\x9e\x8b\x9a\x9b\xac\x96\x85\x9a\xad\x9a\x91\x9b\x9a\x8d\x96\x91\x98\xba\x91\x9e\x9d\x93\x9a\x9b",
 "\x94\x85\x8a\xbb\x96\x8c\x8f\x9e\x8b\x9c\x97\xb2\x9a\x8c\x8c\x9e\x98\x9a\xbe\x9c\x8b\x96\x90\x91\xbe\x9b\x9b\xb0\x9d\x95\x9a\x9c\x8b\xb1\x90\x9b\x9a\xaf\x8d\x90\x8f\x9a\x8d\x8b\x86\xbd\x96\x91\x9b\x96\x91\x98",
 "\x94\x85\x8c\xac\x8a\x8d\x99\x9e\x9c\x9a\xb8\x9a\x8b\xaa\x8c\x9a\x9b\xac\x8a\x8d\x99\x9e\x9c\x9a\xb1\x9e\x8b\x96\x89\x9a\xaf\x8d\x90\x8f\x9a\x8d\x8b\x96\x9a\x8c",
 "\x94\x85\x8c\xb6\x91\x8f\x8a\x8b\xba\x89\x9a\x91\x8b\xae\x8a\x9a\x8a\x9a\xb6\x8b\x9a\x8d\x9e\x8b\x90\x8d\xb8\x9a\x8b\xa9\x9e\x93\x8a\x9a\xa0\x8f\x8d\x96\x89\x9e\x8b\x9a",
 "\x94\x85\x9c\xad\x9a\x8c\x90\x8a\x8d\x9c\x9a\xb2\x9e\x91\x9e\x98\x9a\x8d\xac\x9a\x8b\xb6\x92\x92\x9a\x9b\x96\x9e\x8b\x9a\xb8\xaf\xaa\xbb\x9a\x8f\x93\x90\x86\x92\x9a\x91\x8b\xba\x91\x9e\x9d\x93\x9a\x9b",
 "\x94\x85\x9c\xad\x9a\x91\x9b\x9a\x8d\x9a\x8d\xb6\x8c\xbc\x90\x89\x9a\x8d\x9e\x98\x9a\xbd\x8a\x99\x99\x9a\x8d\xac\x8a\x8f\x8f\x90\x8d\x8b\x9a\x9b",
 "\x94\x85\x9c\xad\x9e\x86\xbd\x90\x8a\x91\x9b\x96\x91\x98\xbd\x90\x87\xbd\x9e\x9c\x94\x99\x9e\x9c\x9a\xb6\x91\x8b\x9a\x8d\x8c\x9a\x9c\x8b\x96\x90\x91",
 "\x94\x85\x9e\xbe\x8f\x8f\x93\x96\x9c\x9e\x8b\x96\x90\x91\xac\x9a\x8b\xbe\x93\x93\x90\x9c\x9e\x8b\x9a\x9b\xb3\x9e\x86\x90\x8a\x8b\xbd\x90\x8a\x91\x9b\x96\x91\x98\xbd\x90\x87\xa9\x96\x8c\x8a\x9e\x93\x96\x85\x9e\x8b\x96\x90\x91\xba\x91\x9e\x9d\x93\x9a\x9b",
 "\x9d\x99\xac\x9c\x9a\x91\x9a\xbc\x90\x91\x99\x96\x98\x8a\x8d\x9e\x8b\x96\x90\x91\xb6\x8c\xa9\x9e\x93\x96\x9b",
            }
        },

        {
            gcvPATCH_NENAMARK,
            {
 "\xa0\xa5\xce\xce\x8c\x9a\x8b\x8a\x8f\xac\x97\x9e\x9b\x9a\x8d\xb6\xb1\xce\xce\xbc\x93\x90\x8a\x9b\xac\x86\x8c\x8b\x9a\x92\xce\xce\xbc\x93\x90\x8a\x9b\xac\x97\x9e\x9b\x9a\x8d\xba\xba\xce\xce\xac\x9a\x8b\x8a\x8f\xac\x97\x9e\x9b\x9a\x8d\xb6\xab\xa0\xba\xaf\xc6\xb8\xb3\xbc\x90\x91\x8b\x9a\x87\x8b\xad\xac\xcc\xa0\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98",
 "\xa0\xa5\xce\xca\x8c\x9a\x8b\xb8\x93\x90\x9d\x9e\x93\xb3\x90\x98\x98\x9a\x8d\xaf\xb9\x89\xaf\x89\xc7\xb3\x90\x98\xb3\x9a\x89\x9a\x93\xaf\xb4\x9c\xac\xcd\xa0\xac\x8b\xc6\xa0\xa0\x89\x9e\xa0\x93\x96\x8c\x8b\xba\xac\xa0",
 "\xa0\xa5\xcd\xcd\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xb5\xb1\xb6\xa0\x90\x91\xb9\x96\x91\x96\x8c\x97\x9a\x9b\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b\xaf\xce\xcf\xa0\x95\x92\x9a\x8b\x97\x90\x9b\xb6\xbb\x99\xad\xb4\xc8\xb7\x9e\x8c\x97\xb2\x9e\x8f\xb6\xc9\xac\x8b\x8d\x96\x91\x98\xac\xc9\xa0\xba\xad\xb4\xce\xce\xad\x9a\x91\x9b\x9a\x8d\xab\x8d\x9e\x9c\x9a",
 "\xa0\xa5\xb1\xce\xcf\xbe\x8f\x8f\xa8\x8d\x9e\x8f\x8f\x9a\x8d\xb6\xc7\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xce\xc9\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xbc\x9e\x93\x93\x9d\x9e\x9c\x94\xaf\xce\xcc\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xac\x9c\x9a\x91\x9a\xba\xce\xcd\x96\x91\x96\x8b\x96\x9e\x93\x96\x85\x9a\xb8\xb3\xba\x89",
 "\xa0\xa5\xb1\xce\xce\xbd\x8a\x8b\x8b\x90\x91\xb8\x8d\x90\x8a\x8f\xcb\x96\x91\x96\x8b\xba\xaf\xc9\xac\x86\x8c\x8b\x9a\x92\xaf\xc6\xb8\xb3\xbc\x90\x91\x8b\x9a\x87\x8b\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xad\xb4\xc9\xa9\x9a\x9c\x8b\x90\x8d\xb6\xce\xcf\xbd\x8a\x8b\x8b\x90\x91\xac\x8f\x9a\x9c\xba",
 "\xa0\xa5\xb1\xce\xce\xac\x9a\x8b\x8a\x8f\xac\x97\x9e\x9b\x9a\x8d\xb6\xb1\xce\xce\xbc\x93\x90\x8a\x9b\xac\x86\x8c\x8b\x9a\x92\xce\xce\xbc\x93\x90\x8a\x9b\xac\x97\x9e\x9b\x9a\x8d\xba\xba\xcb\x9e\x8b\x8b\x8d\xb6\xce\xca\xac\x97\x9e\x9b\x9a\x8d\xbe\x8b\x8b\x8d\x96\x9d\x8a\x8b\x9a\xb6\xb3\x96\xca\xce\xcd\xce\xba\xb3\x96\xcb\xba\xba\xba\xba\xad\xac\xcd\xa0\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98\xad\xab\xa0",
 "\xa0\xa5\xb1\xc7\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xce\xce\x98\x9a\x8b\xac\x9a\x8b\x8b\x96\x91\x98\x8c\xba\xad\xc8\xb7\x9e\x8c\x97\xb2\x9e\x8f\xb6\xc9\xac\x8b\x8d\x96\x91\x98\xce\xcc\xac\x9a\x8b\x8b\x96\x91\x98\x8c\xa9\x9e\x93\x8a\x9a\xba",
 "\xa0\xa5\xb1\xc7\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xbc\xce\xba\xaf\xc9\xac\x86\x8c\x8b\x9a\x92\xaf\xce\xc9\xb1\x9a\x91\x9e\xb2\x9e\x8d\x94\xbc\x9e\x93\x93\x9d\x9e\x9c\x94\xaf\xc6\xb8\xb3\xbc\x90\x91\x8b\x9a\x87\x8b\xaf\xce\xcc\xb8\x8d\x9e\x8f\x97\x96\x9c\x8c\xbc\x9e\x9c\x97\x9a",
 "\xa0\xa5\xb1\xc6\xbd\x93\x9a\x91\x9b\xb2\x9a\x8c\x97\xcb\xb2\x9a\x8c\x97\xce\xcf\x8c\x9a\x8b\xab\x9a\x87\x8b\x8a\x8d\x9a\xba\xad\xb4\xc9\xac\x8b\x8d\x96\x91\x98",
            }
        },

        {
            gcvPATCH_NEOCORE,
            {
 "\xa0\xa5\xcc\xc7\xbe\xab\xb6\xa0\xab\xbc\xa0\xba\x91\x9c\x90\x9b\x9a\xb6\x92\x9e\x98\x9a\xb9\x9e\x8c\x8b\xa0\xac\x9a\x8f\x9a\x8d\x9e\x8b\x9a\xa0\xb7\x9a\x9e\x9b\x9a\x8d\xaf\xb4\x97\x95\x95\x95\x95\x96\x96\x96\x95\x95\xaf\xce\xcc\xa0\xbe\xab\xb6\xab\xbc\xa0\xb7\xba\xbe\xbb\xba\xad\xaf\x97\xaf\x95",
 "\xa0\xa5\xb1\xce\xc7\xae\xa7\xab\x9a\x87\x8b\x8a\x8d\x9a\xbc\x90\x91\x89\x9a\x8d\x8b\x9a\x8d\xcd\xce\xbd\x8a\x92\x8f\x92\x9e\x8f\xa0\xb0\x99\x99\x8c\x9a\x8b\xbe\x91\x9b\xbd\x96\x9e\x8c\xba\x99",
 "\xa0\xa5\xb1\xc8\x9e\x91\x9b\x8d\x90\x96\x9b\xcd\xca\x9e\x8c\x8c\x9a\x8b\xb2\x9e\x91\x9e\x98\x9a\x8d\xb9\x90\x8d\xb5\x9e\x89\x9e\xb0\x9d\x95\x9a\x9c\x8b\xba\xaf\xc8\xa0\xb5\xb1\xb6\xba\x91\x89\xaf\xc7\xa0\x95\x90\x9d\x95\x9a\x9c\x8b",
 "\xbe\xab\xb6\xa0\xab\xbc\xa0\xbb\x9a\x9c\x90\x9b\x9a\xb6\x92\x9e\x98\x9a\xa0\xac\x9a\x8f\x9a\x8d\x9e\x8b\x9a\xa0\xb7\x9a\x9e\x9b\x9a\x8d",
 "\xbc\x90\x91\x99\x96\x98\xb0\x8f\x8b\x96\x90\x91\x8c\xa0\xb3\x90\x98\xbc\x90\x91\x99\x96\x98\x8a\x8d\x9e\x8b\x96\x90\x91",
 "\xb1\xbc\xbc\x90\x91\x99\x96\x98\xb2\x9a\x91\x8a\xa0\xbe\x9b\x9b\xad\x9a\x8c\x90\x93\x8a\x8b\x96\x90\x91",
 "\xb1\xbc\xb3\x90\x9e\x9b\x96\x91\x98\xac\x9c\x8d\x9a\x9a\x91\xa0\xbc\x8d\x9a\x9e\x8b\x9a",
 "\xb1\xbc\xa8\x90\x8d\x93\x9b\xa0\xac\x9a\x8b\xb6\x91\x8b\x9a\x8d\x9e\x9c\x8b\x96\x89\x9a\xb2\x90\x9b\x9a\xad\x90\x8b\x9e\x8b\x96\x90\x91",
            }
        },

        {
            gcvPATCH_GLBM11,
            {
 "\x8d\x9a\x91\x9b\x9a\x8d\xaf\x9e\x8d\x8b\x96\x9c\x93\x9a\xac\x86\x8c\x8b\x9a\x92\xa8\x96\x8b\x97\xaf\x90\x96\x91\x8b\xac\x96\x85\x9a\xbe\x8d\x8d\x9e\x86",
 "\x98\x93\x9d\xa0\x8b\x9a\x8c\x8b\x8d\x9a\x8c\x8a\x93\x8b\xa0\x91\x9a\x88\xa0\x99\x8d\x90\x92\xa0\x9b\x9e\x8b\x9e",
 "\x98\x93\x9d\xa0\x92\xcc\x98\x9c\x90\x91\x99\x96\x98\xa0\x91\x9a\x88\xa0\x99\x8d\x90\x92\xa0\x9b\x9e\x8b\x9e",
 "\x98\x93\x9d\xa0\x92\xcc\x98\x8c\x9a\x8b\x8b\x96\x91\x98\x8c\xa0\x96\x91\x96\x8b",
 "\x9c\x90\x91\x89\x9a\x8d\x8b\xae\x8a\x9e\x8b\x9a\x8d\x91\x96\x90\x91\xab\x90\xbe\x91\x98\x93\x9a\xbe\x87\x96\x8c\xa7",
 "\x9e\x9b\x9b\xab\x9a\x87\x8b\x8a\x8d\x9a\xab\x90\xad\x9a\x91\x9b\x9a\x8d\xb3\x96\x8c\x8b",
 "\xa0\x98\x93\x9d\xa0\x9a\x91\x98\x96\x91\x9a\xa0\x96\x91\x96\x8b\xa0\x8f\x8d\x90\x95\x9a\x9c\x8b\x96\x90\x91\xa0\x92\x9e\x8b\x8d\x96\x87",
 "\x9c\x90\x92\x8f\x8a\x8b\x9a\xb6\x91\x96\x8b\x96\x9e\x93\xa8\x90\x8d\x93\x9b\xaf\x90\x8c\x96\x8b\x96\x90\x91",
            }
        },

    };
#endif

    gcmHEADER();

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->patchID = gcvPATCH_INVALID;

    /* If global patch is set, pick it */
    if (gcPLS.patchID > gcvPATCH_NOTINIT)
    {
        Hardware->patchID = gcPLS.patchID;

        gcmFOOTER();
        return status;
    }

#if gcdDEBUG_OPTION
    if (gcoOS_DetectProcessByName(gcdDEBUG_OPTION_KEY))
    {
        gcPLS.patchID     =
        Hardware->patchID = gcvPATCH_DEBUG;

        gcmFOOTER();
        return status;
    }
#endif

#if defined(ANDROID)

    for (i = 0; i < gcmCOUNTOF(programList); i++)
    {
        status = gcoOS_DetectProgrameByEncryptedSymbols(programList[i]);
        if (status == gcvSTATUS_SKIP)
        {
            gcPLS.patchID     =
            Hardware->patchID = gcvPATCH_INVALID;

            break;
        }
        else if (status)
        {
            gcPLS.patchID     =
            Hardware->patchID = programList[i].patchId;
            gcmFOOTER();
            return status;
        }
    }

#endif

    for (i = 0; i < gcmCOUNTOF(benchList); i++)
    {
        if (gcoOS_DetectProcessByEncryptedName(benchList[i].name))
        {
            gcPLS.patchID     =
            Hardware->patchID = benchList[i].patchId;

            if (benchList[i].symbolFlag)
            {
                gcmPRINT(" Symbol checking %d is invalid!", benchList[i].patchId);
            }

            gcmFOOTER();
            return status;
        }
    }

    gcPLS.patchID = gcvPATCH_INVALID;

    gcmFOOTER();
    return status;
#endif
}

#if gcdENABLE_3D
gceSTATUS
gcoHARDWARE_SetPatchID(
    IN gcoHARDWARE Hardware,
    IN gcePATCH_ID   PatchID
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER();

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmVERIFY_ARGUMENT(PatchID);

    gcPLS.patchID     =
    Hardware->patchID = PatchID;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetPatchID(
    IN gcoHARDWARE Hardware,
    OUT gcePATCH_ID *PatchID
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER();

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmVERIFY_ARGUMENT(PatchID);

    *PatchID = Hardware->patchID;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetSpecialHintData(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER();

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->specialHintData = 0;

    switch(Hardware->patchID)
    {
        case gcvPATCH_GLBM27:
        Hardware->specialHintData = 200;
            break;
        case gcvPATCH_GFXBENCH:
            Hardware->specialHintData = 420;
            break;
        default:
            break;
    }

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetSpecialHintData(
    IN gcoHARDWARE Hardware,
    OUT gctINT * Hint
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER();

    gcmGETHARDWARE(Hardware);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    *Hint = (Hardware->specialHintData < 0) ? (Hardware->specialHintData) : (-- Hardware->specialHintData);

OnError:
    gcmFOOTER();
    return status;
}
#endif

/******************************************************************************\
****************************** gcoHARDWARE API code ****************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoHARDWARE_Construct
**
**  Construct a new gcoHARDWARE object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctBOOL ThreadDefault
**          It's default hardware object of current thread or not.
**
**  OUTPUT:
**
**      gcoHARDWARE * Hardware
**          Pointer to a variable that will hold the gcoHARDWARE object.
*/
gceSTATUS
gcoHARDWARE_Construct(
    IN gcoHAL Hal,
    IN gctBOOL ThreadDefault,
    OUT gcoHARDWARE * Hardware
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

#if gcdENABLE_3D || gcdENABLE_2D
    gcoHARDWARE hardware = gcvNULL;
    gcsHAL_INTERFACE iface;
    gctUINT16 data = 0xff00;
    gctPOINTER pointer;
    gctUINT i;
#if gcdENABLE_3D
    gctUINT j;
#endif

    gcmHEADER_ARG("Hal=0x%x", Hal);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hal, gcvOBJ_HAL);
    gcmDEBUG_VERIFY_ARGUMENT(Hardware != gcvNULL);

    /***************************************************************************
    ** Allocate and reset the gcoHARDWARE object.
    */

    gcmONERROR(gcoOS_Allocate(gcvNULL,
                              gcmSIZEOF(struct _gcoHARDWARE),
                              &pointer));
    hardware = pointer;

    /* Reset the object. */
    gcoOS_ZeroMemory(hardware, gcmSIZEOF(struct _gcoHARDWARE));

    /* Initialize the gcoHARDWARE object. */
    hardware->object.type = gcvOBJ_HARDWARE;

    hardware->buffer = gcvNULL;
    hardware->queue  = gcvNULL;

    /* Check if big endian */
    hardware->bigEndian = (*(gctUINT8 *)&data == 0xff);

    /* Don't stall before primitive. */
    hardware->stallPrimitive = gcvFALSE;

    gcmONERROR(gcoHARDWARE_DetectProcess(hardware));

    hardware->threadDefault = ThreadDefault;

#if gcdENABLE_3D
    gcmONERROR(gcSHADER_SpecialHint(hardware->patchID, &hardware->specialHint));

    gcmONERROR(gcoHARDWARE_SetSpecialHintData(hardware));

    /* Set default API. */
    hardware->api = gcvAPI_OPENGL;

    hardware->stencilStates.mode = gcvSTENCIL_NONE;
    hardware->stencilEnabled     = gcvFALSE;

    hardware->earlyDepth = gcvFALSE;

    hardware->rtneRounding = gcvFALSE;

    /* Disable anti-alias. */
    hardware->sampleMask   = 0xF;
    hardware->sampleEnable = 0xF;
    hardware->samples.x    = 1;
    hardware->samples.y    = 1;
    hardware->vaa          = gcvVAA_NONE;

    /* Table for disable dithering. */
    hardware->ditherTable[0][0] = hardware->ditherTable[0][1] = ~0U;
    /* Table for enable dithering. */
    hardware->ditherTable[1][0] = 0x6E4CA280;
    hardware->ditherTable[1][1] = 0x5D7F91B3;
    hardware->peDitherDirty = gcvTRUE;
    hardware->ditherEnable = gcvFALSE;

    hardware->enableOQ = gcvFALSE;

    /***************************************************************************
    ** Determine multi-sampling constants.
    */

    /* Two color buffers with 8-bit coverage. Used for Flash applications.

          0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      0 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      1 |   |   |   |   |   | X |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      2 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      3 |   |   |   |   |   |   |   |   |   |   |   |   |   | C |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      4 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      5 |   |   |   | X |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      6 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      7 |   |   |   |   |   |   |   |   |   |   |   | C |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      8 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      9 |   |   |   |   |   |   |   | X |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     10 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     11 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   | C |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     12 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     13 |   | X |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     14 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     15 |   |   |   |   |   |   |   |   |   | C |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    */

    hardware->vaa8SampleCoords
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (5) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (3) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (5) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (7) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (9) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (13) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));


    /* Two color buffers with 16-bit coverage. Used for Flash applications.

          0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      0 |   | C |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      1 |   |   |   |   |   | X |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      2 |   |   |   |   |   |   |   |   |   | C |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      3 |   |   |   |   |   |   |   |   |   |   |   |   |   | C |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      4 |   |   |   | C |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      5 |   |   |   |   |   |   |   | X |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      6 |   |   |   |   |   |   |   |   |   |   |   | C |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      7 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   | C |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      8 |   |   | C |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
      9 |   |   |   |   |   |   | X |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     10 |   |   |   |   |   |   |   |   |   |   | C |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     11 |   |   |   |   |   |   |   |   |   |   |   |   |   |   | C |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     12 | C |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     13 |   |   |   |   | X |   |   |   |   |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     14 |   |   |   |   |   |   |   |   | C |   |   |   |   |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
     15 |   |   |   |   |   |   |   |   |   |   |   |   | C |   |   |   |
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
    */

    hardware->vaa16SampleCoords
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (5) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (7) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (5) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (6) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (9) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (4) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (13) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));


    /* 4x MSAA jitter index. */

#if gcdGRID_QUALITY == 0

    /* Square only. */
    hardware->jitterIndex
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:26) - (0 ? 27:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:26) - (0 ? 27:26) + 1))))))) << (0 ? 27:26))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 27:26) - (0 ? 27:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:26) - (0 ? 27:26) + 1))))))) << (0 ? 27:26)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));

#elif gcdGRID_QUALITY == 1

    /* No jitter. */
    hardware->jitterIndex = 0;

#else

    /* Rotated diamonds. */
    hardware->jitterIndex
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 15:14) - (0 ? 15:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:14) - (0 ? 15:14) + 1))))))) << (0 ? 15:14)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 23:22) - (0 ? 23:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:22) - (0 ? 23:22) + 1))))))) << (0 ? 23:22)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:26) - (0 ? 27:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:26) - (0 ? 27:26) + 1))))))) << (0 ? 27:26))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 27:26) - (0 ? 27:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:26) - (0 ? 27:26) + 1))))))) << (0 ? 27:26)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 29:28) - (0 ? 29:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:28) - (0 ? 29:28) + 1))))))) << (0 ? 29:28)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));

#endif


    /* 2x MSAA sample coordinates. */

    hardware->sampleCoords2
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));


    /* 4x MSAA sample coordinates.
    **
    **                        1 1 1 1 1 1                        1 1 1 1 1 1
    **    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  0| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  1| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  2| | | | | | |X| | | | | | | | | |  | | | | | | | | | | |X| | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  3| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  4| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  5| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  6| | | | | | | | | | | | | | |X| |  | | |X| | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  7| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  8| | | | | | | | |o| | | | | | | |  | | | | | | | | |o| | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    **  9| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 10| | |X| | | | | | | | | | | | | |  | | | | | | | | | | | | | | |X| |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 11| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 12| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 13| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 14| | | | | | | | | | |X| | | | | |  | | | | | | |X| | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ** 15| | | | | | | | | | | | | | | | |  | | | | | | | | | | | | | | | | |
    **   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    /* Diamond. */
    hardware->sampleCoords4[0]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (6) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (14) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (6) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (14) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));

    /* Mirrored diamond. */
    hardware->sampleCoords4[1]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (6) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (14) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (6) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (14) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));

    /* Square. */
    hardware->sampleCoords4[2]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (10) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));


    /* Compute centroids. */
    gcmONERROR(gcoHARDWARE_ComputeCentroids(
        hardware,
        1, &hardware->sampleCoords2, &hardware->centroids2
        ));

    gcmONERROR(gcoHARDWARE_ComputeCentroids(
        hardware,
        3, hardware->sampleCoords4, hardware->centroids4
        ));
#endif


#if gcdUSE_HARDWARE_CONFIGURATION_TABLES
    if (gcConfig == gcvNULL)
    {
        gcmVERIFY_OK(_InitHardwareTables(hardware));
    }
    else
    {
        gcmASSERT(gcConfig);
        gcmASSERT(gcFeatures);
        gcmASSERT(gcSWWAs);
        hardware->config   = gcConfig;
        hardware->features = gcFeatures;
        hardware->swwas    = gcSWWAs;
    }
#else
    hardware->config   = &hardware->_config;
    gcmVERIFY_OK(_FillInConfigTable(hardware, hardware->config));
#if gcdENABLE_2D
    gcmVERIFY_OK(_Init2D(hardware));
#endif
    gcmVERIFY_OK(_FillInFeatureTable(hardware, hardware->features));
    gcmVERIFY_OK(_FillInSWWATable(hardware, hardware->swwas));
#endif

#if gcdENABLE_3D
    hardware->unifiedConst = hardware->config->unifiedConst;
    hardware->vsConstBase  = hardware->config->vsConstBase;
    hardware->psConstBase  = hardware->config->psConstBase;
    hardware->vsConstMax   = hardware->config->vsConstMax;
    hardware->psConstMax   = hardware->config->psConstMax;
    hardware->constMax     = hardware->config->constMax;

    hardware->unifiedShader = hardware->config->unifiedShader;
    hardware->vsInstBase    = hardware->config->vsInstBase;
    hardware->psInstBase    = hardware->config->psInstBase;
    hardware->vsInstMax     = hardware->config->vsInstMax;
    hardware->psInstMax     = hardware->config->psInstMax;
    hardware->instMax       = hardware->config->instMax;

    /* Initialize reset status for unified settings */
    hardware->prevProgramUnfiedStatus.useIcache = hardware->features[gcvFEATURE_SH_INSTRUCTION_PREFETCH];

    hardware->prevProgramUnfiedStatus.instruction = hardware->prevProgramUnfiedStatus.useIcache ?
                                                    gcvFALSE :
                                                    hardware->unifiedShader;

    hardware->prevProgramUnfiedStatus.constant = hardware->unifiedConst;
    hardware->prevProgramUnfiedStatus.sampler = gcvFALSE;
    hardware->prevProgramUnfiedStatus.instVSEnd      = -1;
    hardware->prevProgramUnfiedStatus.instPSStart    = -1;
    hardware->prevProgramUnfiedStatus.constVSEnd     = -1;
    hardware->prevProgramUnfiedStatus.constPSStart   = -1;
    hardware->prevProgramUnfiedStatus.samplerVSEnd   = -1;
    hardware->prevProgramUnfiedStatus.samplerPSStart = -1;

#if gcdMULTI_GPU
    hardware->gpuCoreCount     = hardware->config->gpuCoreCount;
    /* Set the default multi GPU rendering mode. */
    hardware->gpuRenderingMode = gcvMULTI_GPU_RENDERING_MODE_INTERLEAVED_128x64;
    /* Set default multi-GPU operating mode as combined. */
    hardware->gpuMode          = gcvMULTI_GPU_MODE_COMBINED;
    /* In the combined mode, the core ID is ignored. */
    hardware->chipEnable       = gcvCORE_3D_ALL_MASK;
    /* This is the semaphore ID
       TODO: We need to move this out of hardware layer. Because all processes
             need to share this id. It may be proper to move this counter to
             kernel layer and add an ioctl to fetch the next available id instead of
             gcoHARDWARE_MultiGPUSemaphoreId.
     */
    hardware->interGpuSemaphoreId   = 0;
#endif

    /* Default depth near and far plane. */
    hardware->depthNear.f = 0.0f;
    hardware->depthFar.f  = 1.0f;

    /* Compute alignment. */
#if gcdMULTI_GPU
    if (hardware->config->gpuCoreCount > 1)
    {
        hardware->resolveAlignmentX = 16 * 2;
    }
    else
    {
        hardware->resolveAlignmentX = 16;
    }
#else
    hardware->resolveAlignmentX = 16;
#endif
    hardware->resolveAlignmentY = (hardware->config->pixelPipes > 1) ? 8 : 4;

    if (hardware->config->pixelPipes > 1)
    {
        if ((!hardware->features[gcvFEATURE_SINGLE_BUFFER]) ||
             gcoHAL_GetOption(gcvNULL, gcvOPTION_PREFER_MULTIPIPE_RS))
        {
            hardware->multiPipeResolve = gcvTRUE;
        }
        else
        {
            hardware->multiPipeResolve = gcvFALSE;
        }
    }
    else
    {
        hardware->multiPipeResolve = gcvFALSE;
    }

#endif


    /***************************************************************************
    ** Allocate the gckCONTEXT object.
    */

    iface.command = gcvHAL_ATTACH;

#if gcdDUMP
    iface.u.Attach.map = gcvTRUE;
#else
    iface.u.Attach.map = gcvFALSE;
#endif

    gcmONERROR(gcoOS_DeviceControl(
        gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        &iface, gcmSIZEOF(iface),
        &iface, gcmSIZEOF(iface)
        ));

    gcmONERROR(iface.status);

    /* Store the allocated context buffer object. */
    hardware->context = iface.u.Attach.context;
    gcmASSERT(hardware->context != 0);

    /* Store the number of states in the context. */
    hardware->stateCount = (gctUINT32) iface.u.Attach.stateCount;

#if gcdDUMP
    hardware->currentContext = 0;
    hardware->contextBytes = iface.u.Attach.bytes;

    for (i = 0; i < gcdCONTEXT_BUFFER_COUNT; i++)
    {
        hardware->contextPhysical[i] = iface.u.Attach.physicals[i];
        hardware->contextLogical[i] = gcmUINT64_TO_PTR(iface.u.Attach.logicals[i]);
    }
#endif

    /**************************************************************************/
    /* Allocate the context and state delta buffers. **************************/

    for (i = 0; i < gcdCONTEXT_BUFFER_COUNT + 1; i += 1)
    {
        /* Allocate a state delta. */
        gcsSTATE_DELTA_PTR delta;
        gcsSTATE_DELTA_PTR prev;
        gctUINT32 bytes;

        /* Allocate the state delta structure. */
        gcmONERROR(gcoOS_AllocateSharedMemory(
            gcvNULL, gcmSIZEOF(gcsSTATE_DELTA), (gctPOINTER *) &delta
            ));

        /* Reset the context buffer structure. */
        gcoOS_ZeroMemory(delta, gcmSIZEOF(gcsSTATE_DELTA));

        /* Append to the list. */
        if (hardware->delta == gcvNULL)
        {
            delta->prev     = gcmPTR_TO_UINT64(delta);
            delta->next     = gcmPTR_TO_UINT64(delta);
            hardware->delta = delta;
        }
        else
        {
            delta->next = gcmPTR_TO_UINT64(hardware->delta);
            delta->prev = hardware->delta->prev;

            prev = gcmUINT64_TO_PTR(hardware->delta->prev);
            prev->next = gcmPTR_TO_UINT64(delta);
            hardware->delta->prev = gcmPTR_TO_UINT64(delta);

        }

        /* Set the number of delta in the order of creation. */
#if gcmIS_DEBUG(gcdDEBUG_CODE)
        delta->num = i;
#endif
        if (hardware->stateCount > 0)
        {
            /* Allocate state record array. */
            gcmONERROR(gcoOS_AllocateSharedMemory(
                gcvNULL,
                gcmSIZEOF(gcsSTATE_DELTA_RECORD) * hardware->stateCount,
                &pointer
                ));

            delta->recordArray = gcmPTR_TO_UINT64(pointer);

            /* Compute UINT array size. */
            bytes = gcmSIZEOF(gctUINT) * hardware->stateCount;

            /* Allocate map ID array. */
            gcmONERROR(gcoOS_AllocateSharedMemory(
                gcvNULL, bytes, &pointer
                ));

            delta->mapEntryID = gcmPTR_TO_UINT64(pointer);

            /* Set the map ID size. */
            delta->mapEntryIDSize = bytes;

            /* Reset the record map. */
            gcoOS_ZeroMemory(gcmUINT64_TO_PTR(delta->mapEntryID), bytes);

            /* Allocate map index array. */
            gcmONERROR(gcoOS_AllocateSharedMemory(
                gcvNULL, bytes, &pointer
                ));

            delta->mapEntryIndex = gcmPTR_TO_UINT64(pointer);
        }

        /* Reset the new state delta. */
        _ResetDelta(delta);
    }

    /***********************************************************************
    ** Construct the command buffer and event queue.
    */

    gcmONERROR(gcoBUFFER_Construct(Hal, hardware, hardware->context,
                                   gcdCMD_BUFFER_SIZE,
                                   hardware->threadDefault, &hardware->buffer));

    gcmONERROR(gcoQUEUE_Construct(gcvNULL, &hardware->queue));

#if gcdENABLE_3D
    /***********************************************************************
    ** Reset the temporary surface.
    */

    gcoOS_ZeroMemory(&hardware->tempBuffer, sizeof(hardware->tempBuffer));

    hardware->tempBuffer.node.pool = gcvPOOL_UNKNOWN;

    /***********************************************************************
    ** Initialize texture states.
    */

    for (i = 0; i < gcdSAMPLERS; i += 1)
    {
        hardware->hwTxSamplerMode[i]         = gcvINVALID_VALUE;
        hardware->hwTxSamplerModeEx[i]       = gcvINVALID_VALUE;
        hardware->hwTxSamplerMode2[i]        = gcvINVALID_VALUE;
        hardware->hwTxSamplerSize[i]         = gcvINVALID_VALUE;
        hardware->hwTxSamplerSizeLog[i]      = gcvINVALID_VALUE;
        hardware->hwTxSampler3D[i]           = gcvINVALID_VALUE;
        hardware->hwTxSamplerLOD[i]          = gcvINVALID_VALUE;
        hardware->hwTxBaseLOD[i]             = gcvINVALID_VALUE;
        hardware->hwTxSamplerYUVControl[i]   = gcvINVALID_VALUE;
        hardware->hwTxSamplerYUVStride[i]    = gcvINVALID_VALUE;
        hardware->hwTxSamplerLinearStride[i] = gcvINVALID_VALUE;

        for (j = 0; j < gcdLOD_LEVELS; j += 1)
        {
            hardware->hwTxSamplerAddress[j][i]  = gcvINVALID_VALUE;
            hardware->hwTxSamplerASTCSize[j][i] = gcvINVALID_VALUE;
            hardware->hwTxSamplerASTCSRGB[j][i] = gcvINVALID_VALUE;
        }
    }

    for (i = 0; i < gcdSAMPLER_TS; i += 1)
    {
        hardware->hwTXSampleTSConfig[i]          = gcvINVALID_VALUE;
        hardware->hwTXSampleTSBuffer[i]          = gcvINVALID_VALUE;
        hardware->hwTXSampleTSClearValue[i]      = gcvINVALID_VALUE;
        hardware->hwTXSampleTSClearValueUpper[i] = gcvINVALID_VALUE;
        hardware->hwTxSamplerTxBaseBuffer[i]     = gcvINVALID_VALUE;
    }

    /***********************************************************************
    ** Determine available features for 3D hardware.
    */

    hardware->hw3DEngine = hardware->features[gcvFEATURE_PIPE_3D];

    /* Determine whether threadwalker is in PS for OpenCL. */
    hardware->threadWalkerInPS = hardware->features[gcvFEATURE_CL_PS_WALKER];

    /* Determine whether composition engine is supported. */
    hardware->hwComposition = ((((gctUINT32) (hardware->config->chipMinorFeatures2)) >> (0 ? 6:6) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1)))))));

    /* Initialize composition shader parameters. */
    if (hardware->hwComposition)
    {
        gcmONERROR(gcoHARDWARE_InitializeComposition(hardware));
    }

    /* Initialize variables for bandwidth optimization. */
    hardware->colorStates.colorWrite       = 0xF;
    hardware->colorStates.colorCompression = gcvFALSE;
    hardware->colorStates.destinationRead  = ~0U;
    hardware->colorStates.rop              = 0xC;

    /* Determine striping. */
    if ((hardware->config->chipModel >= gcv860)
    &&  !((((gctUINT32) (hardware->config->chipMinorFeatures1)) >> (0 ? 4:4) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1)))))))
    )
    {
        /* Get number of cache lines. */
        hardware->needStriping = hardware->hw2DEngine ? 32 : 16;
    }
    else
    {
        /* No need for striping. */
        hardware->needStriping = 0;
    }

    /* Initialize virtual stream state. */
    if ((hardware->config->streamCount == 1)
        ||  hardware->features[gcvFEATURE_MIXED_STREAMS])
    {
        hardware->mixedStreams = gcvTRUE;
    }
    else
    {
        hardware->mixedStreams = gcvFALSE;
    }

    for (i = 0; i < 4; i++)
    {
        hardware->psOutputMapping[i] = i;
    }
#endif

    hardware->currentPipe = (hardware->hw2DEngine) ? gcvPIPE_2D : gcvPIPE_3D;

    gcmONERROR(gcoOS_CreateSignal(gcvNULL, gcvFALSE, &hardware->stallSignal));

    gcmVERIFY_OK(gcoHARDWARE_InitializeFormatArrayTable(hardware));

#if gcdENABLE_3D
    /*Set a invalid value when init*/
    *(gctUINT32 *)&(hardware->alphaStates.floatReference) = 0xFFFFFFFF;
#endif

#if gcdSYNC
    hardware->fence = gcvNULL;
#endif

    gcmTRACE_ZONE(
        gcvLEVEL_INFO, gcvZONE_SIGNAL,
        "%s(%d): stall created signal 0x%08X\n",
        __FUNCTION__, __LINE__,
        hardware->stallSignal);

    /* Return pointer to the gcoHARDWARE object. */
    *Hardware = hardware;

    /* Success. */
    gcmFOOTER_ARG("*Hardware=0x%x", *Hardware);
    return gcvSTATUS_OK;

OnError:
    if (hardware != gcvNULL)
    {
        gcmVERIFY_OK(gcoHARDWARE_Destroy(hardware, ThreadDefault));
    }

    /* Return pointer to the gcoHARDWARE object. */
    *Hardware = gcvNULL;

    /* Return the status. */
    gcmFOOTER();
#endif  /* gcdENABLE_3D*/

    return status;
}

#if gcdENABLE_3D
#if gcdSYNC
static gceSTATUS _DestroyFence(gcoFENCE fence);
#endif
#endif

/*******************************************************************************
**
**  gcoHARDWARE_Destroy
**
**  Destroy an gcoHARDWARE object.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object that needs to be destroyed.
**
**      gctBOOL ThreadDefault
**          Indicate it's thread default hardware or not.
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_Destroy(
    IN gcoHARDWARE Hardware,
    IN gctBOOL    ThreadDefault
    )
{
    gceSTATUS status;
    gcsSTATE_DELTA_PTR deltaHead;
    gctINT i;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DDummySurf)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(Hardware, Hardware->hw2DDummySurf));
        Hardware->hw2DDummySurf = gcvNULL;
    }

    if (Hardware->hw2DClearDummySurf)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(Hardware, Hardware->hw2DClearDummySurf));
        Hardware->hw2DClearDummySurf = gcvNULL;
    }

#if gcdENABLE_3D
#if gcdSYNC
    _DestroyFence(Hardware->fence);
    Hardware->fence = gcvNULL;
#endif
#endif

    gcmONERROR(gcoHARDWARE_Stall(Hardware));

    gcmASSERT(ThreadDefault == Hardware->threadDefault);

    if (Hardware->hw2DCacheFlushSurf)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(
            Hardware,
            Hardware->hw2DCacheFlushSurf));

        Hardware->hw2DCacheFlushSurf = gcvNULL;
    }

    if (Hardware->hw2DCacheFlushCmd)
    {
        gcmONERROR(gcoOS_Free(gcvNULL, Hardware->hw2DCacheFlushCmd));
    }

#if gcdENABLE_3D
    _DestroyTempRT(Hardware);
#endif

    if (Hardware->clearSrcSurf)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(
            Hardware, Hardware->clearSrcSurf));

        Hardware->clearSrcSurf = gcvNULL;
    }

    /* Destroy temporary surface */
    if (Hardware->tempSurface != gcvNULL)
    {
        gcmONERROR(gcoSURF_Destroy(Hardware->tempSurface));
        Hardware->tempSurface = gcvNULL;
    }

    for (i = 0; i < gcdTEMP_SURFACE_NUMBER; i += 1)
    {
        gcsSURF_INFO_PTR surf = Hardware->temp2DSurf[i];

        if (surf != gcvNULL)
        {
            if (surf->node.valid)
            {
                gcmONERROR(gcoHARDWARE_Unlock(
                    &surf->node, gcvSURF_BITMAP
                    ));
            }

            /* Free the video memory by event. */
            if (surf->node.u.normal.node != 0)
            {
                gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(
                    &Hardware->temp2DSurf[i]->node
                    ));

                surf->node.u.normal.node = 0;
            }

            gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Hardware->temp2DSurf[i]));
        }
    }

#if gcdENABLE_3D
    /* Destroy compositon objects. */
    gcmONERROR(gcoHARDWARE_DestroyComposition(Hardware));

    if (Hardware->blitDraw)
    {
        /* Only default hardware has blit draw */
        gcmASSERT(Hardware->threadDefault);

        gcmONERROR(_DestroyBlitDraw(Hardware));
    }

#endif

#if (gcdENABLE_3D || gcdENABLE_2D)
    /* Destroy the command buffer. */
    if (Hardware->buffer != gcvNULL)
    {
        gcmONERROR(gcoBUFFER_Destroy(Hardware->buffer));
        Hardware->buffer = gcvNULL;
    }

    if (Hardware->queue != gcvNULL)
    {
        gcmONERROR(gcoQUEUE_Destroy(Hardware->queue));
        Hardware->queue = gcvNULL;
    }
#endif

#if gcdSTREAM_OUT_BUFFER
    {
        gcoSTREAM_OUT_INDEX * index;
        gcoSTREAM_OUT_INDEX * tempIndex;
#if gcdMULTI_GPU
        gctUINT coreCount = Hardware->config->gpuCoreCount;
#else
        gctUINT coreCount = 1;
#endif
        for(index = Hardware->streamOutIndexCache.head; index != gcvNULL;)
        {
            /* Clear all residual memory */
            for (i = 0; i < coreCount; i++)
            {
                if (index->streamOutIndexAddress[i] != 0)
                {
                    gcmONERROR(gcoHARDWARE_Unlock(&index->streamOutIndexNode[i], gcvSURF_INDEX));
                    gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&index->streamOutIndexNode[i]));
                    index->streamOutIndexNode[i].pool  = gcvPOOL_UNKNOWN;
                    index->streamOutIndexNode[i].valid = gcvFALSE;
                    index->streamOutIndexAddress[i] = 0;
                }
            }

            tempIndex = index;
            index = index->next;

            /* Free the node */
            gcoOS_Free(gcvNULL, tempIndex);
            tempIndex = gcvNULL;
        }
    }

    /* Destroy the event queue. */
    if (Hardware->streamOutEventQueue != gcvNULL)
    {
        gcmONERROR(gcoQUEUE_Destroy(Hardware->streamOutEventQueue));
        Hardware->streamOutEventQueue = gcvNULL;
    }

#endif

    /* Free state deltas. */
    for (deltaHead = Hardware->delta; Hardware->delta != gcvNULL;)
    {
        /* Get a shortcut to the current delta. */
        gcsSTATE_DELTA_PTR delta = Hardware->delta;
        gctUINT_PTR mapEntryIndex = gcmUINT64_TO_PTR(delta->mapEntryIndex);
        gctUINT_PTR mapEntryID = gcmUINT64_TO_PTR(delta->mapEntryID);
        gcsSTATE_DELTA_RECORD_PTR recordArray = gcmUINT64_TO_PTR(delta->recordArray);

        /* Get the next delta. */
        gcsSTATE_DELTA_PTR next = gcmUINT64_TO_PTR(delta->next);

        /* Last item? */
        if (next == deltaHead)
        {
            next = gcvNULL;
        }

        /* Free map index array. */
        if (mapEntryIndex != gcvNULL)
        {
            gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, mapEntryIndex));
        }

        /* Allocate map ID array. */
        if (mapEntryID != gcvNULL)
        {
            gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, mapEntryID));
        }

        /* Free state record array. */
        if (recordArray != gcvNULL)
        {
            gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, recordArray));
        }

        /* Free state delta. */
        gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, delta));

        /* Remove from the list. */
        Hardware->delta = next;
    }

    /* Detach the process. */
    if (Hardware->context != 0)
    {
        gcsHAL_INTERFACE iface;
        iface.command = gcvHAL_DETACH;
        iface.u.Detach.context = Hardware->context;

        gcmONERROR(gcoOS_DeviceControl(
            gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface)
            ));

        Hardware->context = 0;
    }

    /* Free temporary buffer allocated by filter blit operation. */
    /* Is there a surface allocated? */
    if (Hardware->tempBuffer.node.pool != gcvPOOL_UNKNOWN)
    {
        gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&Hardware->tempBuffer.node));

        /* Reset the temporary surface. */
        gcoOS_ZeroMemory(&Hardware->tempBuffer, sizeof(Hardware->tempBuffer));

        Hardware->tempBuffer.node.pool = gcvPOOL_UNKNOWN;
    }

    /* Destroy the stall signal. */
    if (Hardware->stallSignal != gcvNULL)
    {
        gcmONERROR(gcoOS_DestroySignal(gcvNULL, Hardware->stallSignal));
        Hardware->stallSignal = gcvNULL;
    }

    /* Mark the gcoHARDWARE object as unknown. */
    Hardware->object.type = gcvOBJ_UNKNOWN;

    /* Free the gcoHARDWARE object. */
    gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Hardware));

OnError:
    /* Return the status. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_LoadState
**
**  Load a state buffer.
**
**  INPUT:
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Starting register address of the state buffer.
**
**      gctUINT32 Count
**          Number of states in state buffer.
**
**      gctPOINTER Data
**          Pointer to state buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
#if (gcdENABLE_3D || gcdENABLE_2D)
gceSTATUS
gcoHARDWARE_LoadState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Address=%x Count=%d Data=0x%x",
                  Hardware, Address, Count, Data);

    /* Call generic function. */
    gcmONERROR(_LoadStatesEx(Hardware, Address >> 2, gcvFALSE, Count, 0, Data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#if gcdENABLE_3D
gceSTATUS
gcoHARDWARE_LoadCtrlState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    gceSTATUS status;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Address=0x%x Data=0x%x",
                  Hardware, Address, Data);

    /* Verify the arguments. */
    gcmGETHARDWARE(Hardware);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

    /* Determine the size of the buffer to reserve. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, (Address >> 2), 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) ((Address >> 2)) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, (Address >> 2), Data );gcmENDSTATEBATCH(reserve, memory);};

    stateDelta = stateDelta; /* Keep the compiler happy. */

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);


OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_LoadStateX
**
**  Load a state buffer with fixed point states.  The states are meant for the
**  3D pipe.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Starting register address of the state buffer.
**
**      gctUINT32 Count
**          Number of states in state buffer.
**
**      gctPOINTER Data
**          Pointer to state buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_LoadStateX(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Address=%x Count=%d Data=0x%x",
                  Hardware, Address, Count, Data);

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    /* Call LoadState function. */
    gcmONERROR(_LoadStatesEx(Hardware, Address >> 2, gcvTRUE, Count, 0, Data));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_LoadState32
**
**  Load one 32-bit state.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Register address of the state.
**
**      gctUINT32 Data
**          Value of the state.
**
**  OUTPUT:
**
**      Nothing.
*/
#if (gcdENABLE_3D || gcdENABLE_2D)
gceSTATUS
gcoHARDWARE_LoadState32(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Address=%x Data=0x%x",
                  Address, Data);

    /* Call generic function. */
    gcmONERROR(_LoadStates(Hardware, Address >> 2, gcvFALSE, 1, 0, &Data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_LoadState32WithMask
**
**  Load one 32-bit state.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Register address of the state.
**
**      gctUINT32 Mask
**          Register mask of the state
**
**      gctUINT32 Data
**          Value of the state.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_LoadState32WithMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Mask,
    IN gctUINT32 Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Address=%x Data=0x%x",
                  Address, Data);

    /* Call generic function. */
    gcmONERROR(_LoadStates(Hardware, Address >> 2, gcvFALSE, 1, Mask, &Data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_LoadState32x
**
**  Load one 32-bit state, which is represented in 16.16 fixed point.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Address
**          Register address of the state.
**
**      gctFIXED_POINT Data
**          Value of the state in 16.16 fixed point format.
**
**  OUTPUT:
**
**      Nothing.
*/
#if (gcdENABLE_3D || gcdENABLE_2D)
gceSTATUS
gcoHARDWARE_LoadState32x(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctFIXED_POINT Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Address=%x Data=0x%x",
                  Hardware, Address, Data);

    /* Call generic function. */
    gcmONERROR(_LoadStates(Hardware, Address >> 2, gcvTRUE, 1, 0, &Data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_LoadState64
**
**  Load one 64-bit state.
**
**  INPUT:
**
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gctUINT32 Address
**          Register address of the state.
**
**      gctUINT64 Data
**          Value of the state.
**
**  OUTPUT:
**
**      Nothing.
*/
#if (gcdENABLE_3D || gcdENABLE_2D)
gceSTATUS
gcoHARDWARE_LoadState64(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT64 Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Address=%x Data=0x%x",
                  Hardware, Address, Data);

    /* Call generic function. */
    gcmONERROR(_LoadStates(Hardware, Address >> 2, gcvFALSE, 2, 0, &Data));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#if gcdENABLE_3D
#if gcdSYNC
static gctBOOL  _GetFenceCtx(
                gcsSYNC_CONTEXT_PTR head,
                gcoFENCE fence,
                gcsSYNC_CONTEXT_PTR * ctx
                )
{
     gcsSYNC_CONTEXT_PTR ptr;

     if (fence == gcvNULL ||
         head == gcvNULL  ||
         ctx == gcvNULL)
     {
         return gcvFALSE;
     }

     ptr = head;
     while(ptr)
     {
         if (ptr->fence == fence)
         {
             *ctx = ptr;
             return gcvTRUE;
         }

         ptr = ptr->next;
     }

     return gcvFALSE;
}

static gctBOOL _SetFenceCtx(
                gcsSYNC_CONTEXT_PTR * head,
                gcoFENCE fence
                )
{
     gcsSYNC_CONTEXT_PTR ptr;
     gctUINT64 * oqPtr = gcvNULL;

     if (fence == gcvNULL ||
         head  == gcvNULL)
     {
         return gcvFALSE;
     }

     ptr = *head;

     if (fence->type == gcvFENCE_OQ)
     {
         oqPtr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical
             + fence->u.oqFence.nextSlot;
         *oqPtr = 0x12345678;
         gcoSURF_CPUCacheOperation(fence->u.oqFence.dstFenceSurface, gcvCACHE_CLEAN);
     }

     while(ptr)
     {
         if (ptr->fence == fence)
         {
             ptr->fenceID = fence->fenceID;
             ptr->mark    = gcvTRUE;
             ptr->oqSlot= fence->u.oqFence.nextSlot;

             return gcvTRUE;
         }

         ptr = ptr->next;
     }

     if(gcmIS_ERROR(gcoOS_Allocate(gcvNULL,sizeof(gcsSYNC_CONTEXT),(gctPOINTER *)&ptr)))
     {
         fence->fenceEnable = gcvFALSE;
         return gcvFALSE;
     }

     ptr->fence     = fence;
     ptr->fenceID   = fence->fenceID;
     ptr->oqSlot  = fence->u.oqFence.nextSlot;
     ptr->mark      = gcvTRUE;
     ptr->next      = *head;
     *head           = ptr;

     return gcvTRUE;
}


static gceSTATUS
_DestroyFence(gcoFENCE fence)
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER();

    if (fence)
    {
        if (fence->type == gcvFENCE_RLV)
        {
            if (fence->u.rlvFence.fenceSurface != gcvNULL)
            {
                gcmONERROR(gcoSURF_Unlock(fence->u.rlvFence.fenceSurface,gcvNULL));
                gcmONERROR(gcoSURF_Destroy(fence->u.rlvFence.fenceSurface));
                fence->u.rlvFence.fenceSurface = gcvNULL;
            }

            if (fence->u.rlvFence.srcIDSurface != gcvNULL)
            {
                gcmONERROR(gcoSURF_Unlock(fence->u.rlvFence.srcIDSurface,gcvNULL));
                gcmONERROR(gcoSURF_Destroy(fence->u.rlvFence.srcIDSurface));
                fence->u.rlvFence.srcIDSurface = gcvNULL;
            }
        }
        else if (fence->type == gcvFENCE_OQ)
        {
            if (fence->u.oqFence.dstFenceSurface != gcvNULL)
            {
                gcmONERROR(gcoSURF_Unlock(fence->u.oqFence.dstFenceSurface,gcvNULL));
                gcmONERROR(gcoSURF_Destroy(fence->u.oqFence.dstFenceSurface));
                fence->u.oqFence.dstFenceSurface = gcvNULL;
            }
        }
        else if (fence->type == gcvFENCE_HW)
        {
            if (fence->u.hwFence.dstSurface != gcvNULL)
            {
                gcmONERROR(gcoSURF_Unlock(fence->u.hwFence.dstSurface,gcvNULL));
                gcmONERROR(gcoSURF_Destroy(fence->u.hwFence.dstSurface));
                fence->u.hwFence.dstSurface = gcvNULL;
            }
        }

        gcmONERROR(gcoOS_Free(gcvNULL,fence));
    }

OnError:
    /* Return the status. */
    gcmFOOTER_NO();
    return status;
}

static gceSTATUS
_ConstructFence(
     IN gcoHARDWARE Hardware,
     IN OUT gcoFENCE * Fence
     )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoHARDWARE hardware;
    gctPOINTER ptr[3];
    gcoFENCE fence = gcvNULL;
    gcsHAL_INTERFACE iface;

    gcmHEADER();

    hardware = Hardware;


    if (hardware == gcvNULL)
    {
        gcmFOOTER();
        return gcvSTATUS_FALSE;
    }

    gcmONERROR(gcoOS_Allocate(gcvNULL, sizeof(struct _gcoFENCE), (gctPOINTER *)&fence));

    gcoOS_ZeroMemory(fence,sizeof(struct _gcoFENCE));

    if (Hardware->features[gcvFEATURE_HALTI2])
    {
        fence->type = gcvFENCE_HW;
    }
    else if (Hardware->features[gcvFEATURE_HALTI0]
    && !Hardware->features[gcvFEATURE_HZ]
    )
    {
        fence->type = gcvFENCE_OQ;
    }
    else
    {
        fence->type = gcvFENCE_RLV;
    }

    if (fence->type == gcvFENCE_RLV)
    {
        fence->u.rlvFence.srcWidth  = hardware->resolveAlignmentX * 16;
        fence->u.rlvFence.srcHeight = hardware->resolveAlignmentY * 16;

        /* Aligned to super tile */
        fence->u.rlvFence.srcWidth = gcmALIGN(fence->u.rlvFence.srcWidth, 64);
        fence->u.rlvFence.srcHeight = gcmALIGN(fence->u.rlvFence.srcHeight, 64);

        fence->u.rlvFence.srcX      = 0;
        fence->u.rlvFence.srcY      = 0;
        fence->u.rlvFence.srcOffset = 0;

        gcmONERROR(gcoSURF_Construct(gcvNULL,
                                     hardware->resolveAlignmentX, hardware->resolveAlignmentY, 1,
                                     gcvSURF_TEXTURE,
                                     gcvSURF_A8R8G8B8,
                                     gcvPOOL_DEFAULT,
                                     &fence->u.rlvFence.fenceSurface));

        gcmONERROR(gcoSURF_Construct(gcvNULL,
                                     fence->u.rlvFence.srcWidth, fence->u.rlvFence.srcHeight, 1,
                                     gcvSURF_TEXTURE,
                                     gcvSURF_A8R8G8B8,
                                     gcvPOOL_DEFAULT,
                                     &fence->u.rlvFence.srcIDSurface));

        fence->u.rlvFence.fenceSurface->info.tiling = gcvTILED;
        fence->u.rlvFence.srcIDSurface->info.tiling = gcvTILED;

        gcmONERROR(gcoSURF_Lock(fence->u.rlvFence.srcIDSurface,gcvNULL, ptr));

        gcoOS_ZeroMemory(ptr[0],fence->u.rlvFence.srcWidth * fence->u.rlvFence.srcHeight * 4);

        gcmONERROR(gcoSURF_Lock(fence->u.rlvFence.fenceSurface,gcvNULL, ptr));

        gcoOS_ZeroMemory(ptr[0],hardware->resolveAlignmentX * hardware->resolveAlignmentY * 4);
    }
    else if(fence->type == gcvFENCE_OQ)
    {
        fence->u.oqFence.dstWidth  = hardware->resolveAlignmentX * 4;
        fence->u.oqFence.dstHeight = hardware->resolveAlignmentY * 8;

        /* Aligned to super tile */
        fence->u.oqFence.dstWidth = gcmALIGN(fence->u.oqFence.dstWidth, 64);
        fence->u.oqFence.dstHeight = gcmALIGN(fence->u.oqFence.dstHeight,64);

        gcmONERROR(gcoSURF_Construct(gcvNULL,
                                     fence->u.oqFence.dstWidth, fence->u.oqFence.dstHeight, 1,
                                     gcvSURF_TEXTURE,
                                     gcvSURF_A8R8G8B8,
                                     gcvPOOL_DEFAULT,
                                     &fence->u.oqFence.dstFenceSurface));

        gcmONERROR(gcoSURF_Lock(fence->u.oqFence.dstFenceSurface,gcvNULL, ptr));

        gcoOS_ZeroMemory(ptr[0],fence->u.oqFence.dstWidth * fence->u.oqFence.dstHeight * 4);

        fence->u.oqFence.nextSlot = 0;

        fence->u.oqFence.dstSlotSize = fence->u.oqFence.dstWidth * fence->u.oqFence.dstHeight / 2;

        fence->u.oqFence.commitSlot = 0xffffffff;

        fence->u.oqFence.commandSlot = 0xffffffff;

        fence->u.oqFence.nextSlot = 0;
    }
    else if (fence->type == gcvFENCE_HW)
    {
        gctSIZE_T alignment = 0;
        gctUINT32 physical[3] = {~0U};

        /* Allocate dst, try allocate 8 byte. Actually
        ** hardware will align to 16x4 or 16x8.
         */
        gcmONERROR(gcoSURF_Construct(gcvNULL,
                                     2, 2, 1,
                                     gcvSURF_LINEAR,
                                     gcvSURF_A8R8G8B8,
                                     gcvPOOL_DEFAULT,
                                     &fence->u.hwFence.dstSurface));

        gcmONERROR(gcoSURF_Lock(fence->u.hwFence.dstSurface, physical, ptr));
        /* Align to 32 bit.*/
        alignment = gcmALIGN(gcmPTR2INT(ptr[0]), 4) - gcmPTR2INT(ptr[0]);

        fence->u.hwFence.dstAddr = (gctUINT8_PTR)ptr[0] + alignment;
        fence->u.hwFence.dstPhysic = gcmPTR2INT32(physical[0] + alignment);
        /* Only need 8 byte.*/
        gcoOS_ZeroMemory(ptr[0],8);
    }

    fence->fenceID   = 1;
    fence->commitID  = 0;

    fence->fenceIDSend = 0;

    fence->addSync      = gcvFALSE;
    fence->fenceEnable  = gcvTRUE;

    if (hardware->patchID == gcvPATCH_BASEMARK2V2)
    {
        if ((hardware->config->chipModel == gcv2000) &&
            (hardware->config->chipRevision == 0x5108))
        {
            fence->loopCount    = 100;
        }
        else
        {
            fence->loopCount    = 20000;
        }
    }
    else
    {
        fence->loopCount    = gcdFENCE_WAIT_LOOP_COUNT;
    }

    fence->delayCount   = 20000;

    iface.u.QueryResetTimeStamp.timeStamp = 0;

    iface.command = gcvHAL_QUERY_RESET_TIME_STAMP;

    gcoOS_DeviceControl(gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        &iface, gcmSIZEOF(iface),
        &iface, gcmSIZEOF(iface));

    fence->resetTimeStamp = iface.u.QueryResetTimeStamp.timeStamp;

    *Fence = fence;

    gcmFOOTER();
    return status;

OnError:
    _DestroyFence(fence);

    *Fence = gcvNULL;
    /* Return result. */
    gcmFOOTER();
    return status;

}

static void _ResetFence(gcoFENCE fence)
{
    if (fence == gcvNULL)
        return;

    gcmPRINT("Reset Fence!");
    if (fence->type == gcvFENCE_RLV)
    {
        /* Clear the Rlv dst to the hight sendID */
        *(gctUINT64 *)fence->u.rlvFence.fenceSurface->info.node.logical = fence->fenceID;
    }
    else if (fence->type == gcvFENCE_OQ)
    {
        /* Clear all dst slot to zero */
        gcoOS_MemFill(fence->u.oqFence.dstFenceSurface->info.node.logical,
            0,fence->u.oqFence.dstSlotSize * 4);
    }
    else if (fence->type == gcvFENCE_HW)
    {
        /* Clear the Rlv dst to the hight sendID */
        *(gctUINT32 *)fence->u.hwFence.dstAddr = (gctUINT32)fence->fenceID;
    }

    fence->fenceIDSend = fence->fenceID;
    fence->addSync = gcvFALSE;
    fence->commitID = fence->fenceID;

    return;
}

#define OQ_FENCE_BACK(ptr) ((*(gctUINT32*)ptr) == 0)

static gctBOOL _IsRlvFenceBack(gcsSYNC_CONTEXT_PTR ctx, gcoFENCE fence)
{
    gctUINT64   *backFenceAddr;
    backFenceAddr = (gctUINT64 *)fence->u.rlvFence.fenceSurface->info.node.logical;

    if (ctx->fenceID <= *backFenceAddr)
    {
        return gcvTRUE;
    }
    else
    {
        return gcvFALSE;
    }
}

/* When next slot is not 0, have it been commited? */
static gctBOOL _IsOQNextSlotCommited(gcoFENCE fence)
{
    /* Full slot */
    if (fence->u.oqFence.commitSlot == 0xffffffff ||
        ((fence->u.oqFence.commitSlot + 1) % fence->u.oqFence.dstSlotSize )
        == fence->u.oqFence.nextSlot)
    {
        return gcvFALSE;
    }
    else
    {
        return gcvTRUE;
    }
}

static gctBOOL _IsOQFenceBack(gcsSYNC_CONTEXT_PTR ctx, gcoFENCE fence)
{
    gctUINT64 * oqAddr = (gctUINT64 *)(fence->u.oqFence.dstFenceSurface->info.node.logical)
                       + ctx->oqSlot;

    /* id > fence->commitID, not commit yet */
    if (ctx->fenceID > fence->commitID)
    {
        return gcvFALSE;
    }
    /* Yes, we are within slot range */
    else if (ctx->fenceID + fence->u.oqFence.dstSlotSize - 1 >= fence->commitID)
    {
        return OQ_FENCE_BACK(oqAddr);
    }
    else
    {
        /* Too old fence, already back */
        return gcvTRUE;
    }
}

#define HW_FENCE_LESS_EQUAL(id0,id1) (((id0 <= id1) || ((id0 > id1) && ((id0 - id1) > 0xf0000000))) ? gcvTRUE : gcvFALSE)

static gctBOOL _IsHWFenceBack(gcsSYNC_CONTEXT_PTR ctx, gcoFENCE fence)
{
    gctUINT32   backID;

    /* HW only have 32bit data, there is no way that we take all 2^32 times send without query once.
       So, compare with SendID to get the 64bit back ID
    */
    backID = *(gctUINT32 *)fence->u.hwFence.dstAddr;

    if (HW_FENCE_LESS_EQUAL((gctUINT32)(ctx->fenceID), backID))
    {
        return gcvTRUE;
    }
    else
    {
        return gcvFALSE;
    }
}

static gctBOOL _IsFenceBack(gcsSYNC_CONTEXT_PTR ctx, gcoFENCE fence)
{
    if (fence->type == gcvFENCE_RLV)
    {
        return _IsRlvFenceBack(ctx,fence);
    }
    else if (fence->type == gcvFENCE_OQ)
    {
        return _IsOQFenceBack(ctx,fence);
    }
    else if (fence->type == gcvFENCE_HW)
    {
        return _IsHWFenceBack(ctx,fence);
    }

    return gcvTRUE;
}

static void _WaitRlvFenceBack(gctUINT64 id, gcoFENCE fence)
{
#if !gcdNULL_DRIVER
    gctUINT32 i,delayCount;
    gctUINT64 * backAddr;

    backAddr = (gctUINT64 *)fence->u.rlvFence.fenceSurface->info.node.logical;
    delayCount = fence->delayCount;

    while(gcvTRUE)
    {
        i = fence->loopCount;

        do {
            /* Flush cach when read */
            gcoSURF_CPUCacheOperation(fence->u.rlvFence.fenceSurface, gcvCACHE_INVALIDATE);

            if (id <= *backAddr)
            {
                return;
            }
        }
        while(i--);

        gcoOS_Delay(gcvNULL,1);

        delayCount--;

        if (delayCount == 0)
            break;
    }

    /* Time Out */
    gcmPRINT("Fence Wait TimeOut!");
    {
        gcsHAL_INTERFACE iface;
        iface.u.QueryResetTimeStamp.timeStamp = 0;

        iface.command = gcvHAL_QUERY_RESET_TIME_STAMP;

        gcoOS_DeviceControl(gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface));

        if (iface.u.QueryResetTimeStamp.timeStamp != fence->resetTimeStamp)
        {
            fence->resetTimeStamp = iface.u.QueryResetTimeStamp.timeStamp;
            _ResetFence(fence);
        }
    }
#endif
    return;
}

static void _WaitOQFenceBack(gctUINT32 oqSlot,gcoFENCE fence)
{
#if !gcdNULL_DRIVER
    gctUINT32 i,delayCount;
    gctUINT32 slotBegin,slotEnd,slot,slot1;
    gctUINT64 * slotAddr;
    gctUINT64 * oqAddr;

    slotEnd   = (fence->u.oqFence.commitSlot + 1) % fence->u.oqFence.dstSlotSize;
    slotBegin = oqSlot;

    oqAddr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical + oqSlot;
    delayCount = fence->delayCount;

    while(gcvTRUE)
    {
        i = fence->loopCount;

        do {
            /* Flush cach when read */
            gcoSURF_CPUCacheOperation(fence->u.oqFence.dstFenceSurface, gcvCACHE_INVALIDATE);

            if (OQ_FENCE_BACK(oqAddr))
            {
                return;
            }
            /* Scan all OQ slot from last commit to this commit
               If any slot is back, we are back, clear the slot
               from last commit to the zero slot to zero
            */
            slot = slotBegin;
            while(slot != slotEnd)
            {
                slotAddr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical
                         + slot;
                if (*slotAddr == 0)
                {
                    slot1 = slotBegin;

                    while(slot1 != slot)
                    {
                        slotAddr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical
                                 + slot1;
                        *slotAddr = 0;

                        slot1++;
                        slot1 %= fence->u.oqFence.dstSlotSize;
                    }

                    return;
                }

                slot++;
                slot %= fence->u.oqFence.dstSlotSize;
            }
        }
        while(i--);

        gcoOS_Delay(gcvNULL,1);

        delayCount--;

        if (delayCount == 0)
            break;
    }

    /* Time Out */
    gcmPRINT("Fence Wait TimeOut!");
    {
        gcsHAL_INTERFACE iface;
        iface.u.QueryResetTimeStamp.timeStamp = 0;

        iface.command = gcvHAL_QUERY_RESET_TIME_STAMP;

        gcoOS_DeviceControl(gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface));

        if (iface.u.QueryResetTimeStamp.timeStamp != fence->resetTimeStamp)
        {
            fence->resetTimeStamp = iface.u.QueryResetTimeStamp.timeStamp;
            _ResetFence(fence);
        }
    }
#endif

    return;
}

static void _WaitHWFenceBack(gctUINT64 id, gcoFENCE fence)
{
#if !gcdNULL_DRIVER
    gctUINT32 i,delayCount;
    gctUINT32 * backAddr;
    gctUINT32 backID;

    backAddr = (gctUINT32 *)fence->u.hwFence.dstAddr;
    delayCount = fence->delayCount;

    while(gcvTRUE)
    {
        i = fence->loopCount;

        do {
            /* Flush cach when read */
            gcoSURF_CPUCacheOperation(fence->u.hwFence.dstSurface, gcvCACHE_INVALIDATE);

            backID = *backAddr;

            if (HW_FENCE_LESS_EQUAL((gctUINT32)id ,backID))
            {
                return;
            }
        }
        while(i--);

        gcoOS_Delay(gcvNULL,1);

        delayCount--;

        if (delayCount == 0)
            break;
    }

    /* Time Out */
    gcmPRINT("Fence Wait TimeOut!");
    {
        gcsHAL_INTERFACE iface;
        iface.u.QueryResetTimeStamp.timeStamp = 0;

        iface.command = gcvHAL_QUERY_RESET_TIME_STAMP;

        gcoOS_DeviceControl(gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface));

        if (iface.u.QueryResetTimeStamp.timeStamp != fence->resetTimeStamp)
        {
            fence->resetTimeStamp = iface.u.QueryResetTimeStamp.timeStamp;
            _ResetFence(fence);
        }
    }
#endif
    return;
}

static gceSTATUS _WaitFenceBack(gcsSYNC_CONTEXT_PTR ctx, gcoFENCE fence,gctBOOL commit)
{
    gceSTATUS status = gcvSTATUS_OK;

    if (commit)
    {
        gcmONERROR(gcoHARDWARE_SendFence(gcvNULL));
        /* We have a stupid commit in Resolve, I am not commiting again here */
        gcmONERROR(gcoHARDWARE_Commit(gcvNULL));
    }

    if (fence->type == gcvFENCE_RLV)
    {
        _WaitRlvFenceBack(ctx->fenceID,fence);
    }
    else if (fence->type == gcvFENCE_OQ)
    {
        _WaitOQFenceBack(ctx->oqSlot,fence);
    }
    else if (fence->type == gcvFENCE_HW)
    {
        _WaitHWFenceBack(ctx->fenceID, fence);
    }

OnError:
    return status;
}

static gceSTATUS
_SendOQFence(
                gcoHARDWARE Hardware,
                gctUINT32 physic)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32_PTR memory;
    gcoCMDBUF reserve;

    gcmHEADER_ARG("Hardware=0x%x Physic=%d", Hardware, physic);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->enableOQ)
    {
        gcmFATAL("!!!Occlusion query is enabled when sending fence through it");
    }

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Reserve command buffer space. */
    gcmONERROR(gcoBUFFER_Reserve(
        Hardware->buffer,
        2 * 8,
        gcvTRUE,
        gcvCOMMAND_3D,
        &reserve
        ));

    memory = gcmUINT64_TO_PTR(reserve->lastReserve);

    /* Send sempahore token. */
    memory[0]
    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E09) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    memory[1] = physic;

    /* Point to stall command is required. */
    memory += 2;

    memory[0]
    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E0C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    memory[1] = 0;

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_SendHWFence(
                gcoHARDWARE Hardware,
                gctUINT32 physic,
                gctUINT32 data)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32_PTR memory;
    gcoCMDBUF reserve;

    gcmHEADER_ARG("Hardware=0x%x Physic=%d", Hardware, physic);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Reserve command buffer space. */
    gcmONERROR(gcoBUFFER_Reserve(
        Hardware->buffer,
        2 * 8,
        gcvTRUE,
        gcvCOMMAND_3D,
        &reserve
        ));

    memory = gcmUINT64_TO_PTR(reserve->lastReserve);

    /* Send sempahore token. */
    memory[0]
    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E1A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    memory[1] = physic;

    /* Point to stall command is required. */
    memory += 2;

    memory[0]
    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E1B) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    memory[1] = data;

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetFence(
    IN gcoHARDWARE Hardware,
    IN gcsSYNC_CONTEXT_PTR *Ctx
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoFENCE fence;

    gcmHEADER_ARG("Hardware=0x%x Ctx=0x%x",Hardware, Ctx);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->fence == gcvNULL)
    {
        _ConstructFence(Hardware,&Hardware->fence);
    }

    fence = Hardware->fence;
    if (!fence || !fence->fenceEnable || (Ctx == gcvNULL))
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    if (!_SetFenceCtx(Ctx,fence))
    {
        gcmFOOTER();
        return gcvSTATUS_FALSE;
    }

    fence->addSync = gcvTRUE;

OnError:
    /* Return result. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SendFence(gcoHARDWARE Hardware)
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoFENCE   fence = gcvNULL;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->fence == gcvNULL)
    {
        _ConstructFence(Hardware,&Hardware->fence);
    }

    fence = Hardware->fence;

    if (!fence || !fence->fenceEnable)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    /*Send fence/OQ/(Set commit update id)*/
    if (fence->addSync)
    {
        if (fence->type == gcvFENCE_RLV)
        {
            gcsPOINT srcOrigin, dstOrigin, RectSize;
            gctUINT64 * ptr;
            gctUINT64 * ptrFence;
            gctUINT64 * srcAddr;

            /* fill ID to srcIDSurface */
            ptr = (gctUINT64 *)fence->u.rlvFence.srcIDSurface->info.node.logical;
            ptrFence = (gctUINT64 *)fence->u.rlvFence.fenceSurface->info.node.logical;

            srcAddr = (gctUINT64 *)((gctUINT8_PTR)ptr + fence->u.rlvFence.srcOffset * 4);

            /* flush cache when read */
             gcoSURF_CPUCacheOperation(fence->u.rlvFence.srcIDSurface, gcvCACHE_INVALIDATE);
             gcoSURF_CPUCacheOperation(fence->u.rlvFence.fenceSurface, gcvCACHE_INVALIDATE);
            /* Can we use the src slot? */
            if (*srcAddr > *ptrFence)
            {
                if(*srcAddr > fence->commitID)
                {
                    gcoHARDWARE_Commit(Hardware);
                }

                _WaitRlvFenceBack(*srcAddr,fence);
            }

            *srcAddr = fence->fenceID;

            /*Flush after write */
            gcoSURF_CPUCacheOperation(fence->u.rlvFence.srcIDSurface, gcvCACHE_CLEAN);
            srcOrigin.x = fence->u.rlvFence.srcX;
            srcOrigin.y = fence->u.rlvFence.srcY;
            dstOrigin.x = 0;
            dstOrigin.y = 0;
            RectSize.x = Hardware->resolveAlignmentX;
            RectSize.y = Hardware->resolveAlignmentY;

            gcmONERROR(gcoSURF_ResolveRect(fence->u.rlvFence.srcIDSurface,
                fence->u.rlvFence.fenceSurface,
                &srcOrigin,
                &dstOrigin,
                &RectSize));

            fence->fenceIDSend = fence->fenceID;

            fence->u.rlvFence.srcX += Hardware->resolveAlignmentX;
            if (fence->u.rlvFence.srcX >= fence->u.rlvFence.srcWidth)
            {
                fence->u.rlvFence.srcX = 0;
                fence->u.rlvFence.srcY += Hardware->resolveAlignmentY;
                if (fence->u.rlvFence.srcY >= fence->u.rlvFence.srcHeight)
                {
                    fence->u.rlvFence.srcY = 0;
                }
            }

            fence->u.rlvFence.srcOffset = fence->u.rlvFence.srcWidth * fence->u.rlvFence.srcY
                + (fence->u.rlvFence.srcX / 4 * 16);

        }
        else if (fence->type == gcvFENCE_OQ)
        {
            gctUINT32 physic;
            gctUINT64 * ptr;

            ptr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical
                + fence->u.oqFence.nextSlot;

            physic = (gctUINT32)fence->u.oqFence.dstFenceSurface->info.node.physical
                + fence->u.oqFence.nextSlot * sizeof(gctUINT64);

            /*Send OQ command Here*/
            _SendOQFence(Hardware, physic);

            fence->u.oqFence.commandSlot = fence->u.oqFence.nextSlot;

            fence->fenceIDSend = fence->fenceID;

            fence->u.oqFence.nextSlot++;

            fence->u.oqFence.nextSlot %= fence->u.oqFence.dstSlotSize;

            ptr = (gctUINT64 *)fence->u.oqFence.dstFenceSurface->info.node.logical
                + fence->u.oqFence.nextSlot;

            gcoSURF_CPUCacheOperation(fence->u.oqFence.dstFenceSurface, gcvCACHE_INVALIDATE);
            if (!OQ_FENCE_BACK(ptr))
            {
                if(!_IsOQNextSlotCommited(fence))
                {
                    gcmONERROR(gcoHARDWARE_Commit(Hardware));
                }

                _WaitOQFenceBack(fence->u.oqFence.nextSlot,fence);
            }
        }
        else if (fence->type == gcvFENCE_HW)
        {
            _SendHWFence(Hardware,fence->u.hwFence.dstPhysic, (gctUINT32)fence->fenceID);

            fence->fenceIDSend = fence->fenceID;
        }

        fence->addSync = gcvFALSE;
        fence->fenceID++;
    }

    status = gcvSTATUS_OK;

    /* Return result. */
    gcmFOOTER();
    return status;

OnError:
    /* Return result. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_WaitFence(
    IN gcoHARDWARE Hardware,
    IN gcsSYNC_CONTEXT_PTR Ctx
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoFENCE    fence = gcvNULL;
    gcsSYNC_CONTEXT_PTR fenceCtx = gcvNULL;

    gcmHEADER_ARG("Hardware=0x%x Ctx=0x%x",Hardware, Ctx);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->fence == gcvNULL)
    {
        _ConstructFence(Hardware, &Hardware->fence);
    }

    fence = Hardware->fence;

    if (!fence || !fence->fenceEnable || (Ctx == gcvNULL))
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    if (!_GetFenceCtx(Ctx,fence,&fenceCtx))
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    if (!fenceCtx->mark)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    /* no matter what happend, mark to false*/
    fenceCtx->mark = gcvFALSE;

    if(fence->type == gcvFENCE_OQ)
    {
        gcoSURF_CPUCacheOperation(fence->u.oqFence.dstFenceSurface, gcvCACHE_INVALIDATE);
    }
    else if(fence->type == gcvFENCE_RLV)
    {
         gcoSURF_CPUCacheOperation(fence->u.rlvFence.fenceSurface, gcvCACHE_INVALIDATE);
    }
    else if(fence->type == gcvFENCE_HW)
    {
         gcoSURF_CPUCacheOperation(fence->u.hwFence.dstSurface, gcvCACHE_INVALIDATE);
    }

    if (_IsFenceBack(fenceCtx,
                     fence))
    {
        gcmFOOTER();
        return gcvTRUE;
    }
    else if(fenceCtx->fenceID < fence->commitID)
    {
        gcmONERROR(_WaitFenceBack(fenceCtx,fence,gcvFALSE));
    }
    else
    {
        gcmONERROR(_WaitFenceBack(fenceCtx,fence,gcvTRUE));
    }

OnError:
    /* Return result. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetFenceEnabled(
    IN gcoHARDWARE Hardware,
    OUT gctBOOL_PTR Enabled
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enabled=0x%x",Hardware, Enabled);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmVERIFY_ARGUMENT(Enabled != gcvNULL);

    *Enabled = Hardware->fenceEnabled;

OnError:
    /* Return result. */
    gcmFOOTER();
    return status;

}

gceSTATUS
gcoHARDWARE_SetFenceEnabled(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enabled
    )

{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enabled=%d",Hardware, Enabled);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->fenceEnabled = Enabled;

OnError:

    /* Return result. */
    gcmFOOTER();
    return status;

}

#endif


#endif

/*******************************************************************************
**
**  gcoHARDWARE_Commit
**
**  Commit the current command buffer to the hardware.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Commit(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

#if (gcdENABLE_3D || gcdENABLE_2D)
#if gcdDUMP
    gctPOINTER dumpCommandLogical;
    gctUINT32 dumpCommandBytes = 0;
#endif

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Commit command buffer and return status. */
#if gcdDUMP
    status = gcoBUFFER_Commit(
        Hardware->buffer,
        Hardware->currentPipe,
        Hardware->delta,
        Hardware->queue,
        &dumpCommandLogical,
        &dumpCommandBytes
        );
#else
    status = gcoBUFFER_Commit(
        Hardware->buffer,
        Hardware->currentPipe,
        Hardware->delta,
        Hardware->queue
        );
#endif

    /* Did the delta become associated? */
    if (Hardware->delta->refCount == 0)
    {
        /* No, merge with the previous. */
        _MergeDelta(Hardware->delta);
    }
    else
    {
        /* The delta got associated, move to the next one. */
        Hardware->delta = gcmUINT64_TO_PTR(Hardware->delta->next);
        gcmASSERT(Hardware->delta->refCount == 0);

#if gcdDUMP
        /* Dump current context buffer. */
        gcmDUMP_BUFFER(gcvNULL,
                       "context",
                       0,
                       Hardware->contextLogical[Hardware->currentContext],
                       0,
                       Hardware->contextBytes);

        /* Advance to next context buffer. */
        Hardware->currentContext = (Hardware->currentContext + 1) % gcdCONTEXT_BUFFER_COUNT;
#endif
    }

#if gcdDUMP
    if (dumpCommandBytes)
    {
        /* Dump current command buffer. */
        gcmDUMP_BUFFER(gcvNULL,
                       "command",
                       0,
                       dumpCommandLogical,
                       0,
                       dumpCommandBytes);
    }
#endif

    /* Dump the commit. */
    gcmDUMP(gcvNULL, "@[commit]");

    /* Reset the current. */
    _ResetDelta(Hardware->delta);

#if gcdSYNC
    if (!gcmIS_ERROR(status))
    {
        if (Hardware->fence)
        {
            if (Hardware->fence->type == gcvFENCE_OQ)
            {
                Hardware->fence->u.oqFence.commitSlot = Hardware->fence->u.oqFence.commandSlot;
            }
            Hardware->fence->commitID = Hardware->fence->fenceIDSend;
        }
    }
#endif

OnError:
    /* Return the status. */
    gcmFOOTER();
#endif  /* (gcdENABLE_3D || gcdENABLE_2D) */

    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Stall
**
**  Stall the thread until the hardware is finished.
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
gcoHARDWARE_Stall(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

#if (gcdENABLE_3D || gcdENABLE_2D)
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Dump the stall. */
    gcmDUMP(gcvNULL, "@[stall]");

    /* Create a signal event. */
    iface.command            = gcvHAL_SIGNAL;
    iface.u.Signal.signal    = gcmPTR_TO_UINT64(Hardware->stallSignal);
    iface.u.Signal.auxSignal = 0;
    iface.u.Signal.process   = gcmPTR_TO_UINT64(gcoOS_GetCurrentProcessID());
    iface.u.Signal.fromWhere = gcvKERNEL_PIXEL;

    /* Send the event. */
    gcmONERROR(gcoHARDWARE_CallEvent(Hardware, &iface));

    /* Commit the event queue. */
    gcmONERROR(gcoQUEUE_Commit(Hardware->queue, gcvFALSE));

    /* Wait for the signal. */
    gcmONERROR(gcoOS_WaitSignal(gcvNULL, Hardware->stallSignal, gcvINFINITE));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
#endif  /* (gcdENABLE_3D || gcdENABLE_2D) */
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  _ConvertResolveFormat
**
**  Converts HAL resolve format into its hardware equivalent.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gceSURF_FORMAT Format
**          HAL format value.
**
**  OUTPUT:
**
**      gctUINT32 * HardwareFormat
**          Hardware format value.
**
**      gctBOOL * Flip
**          RGB component flip flag.
**
**      OUT gceMSAA_DOWNSAMPLE_MODE *DownsampleMode
**          Multisample downsample mode
*/
static gceSTATUS _ConvertResolveFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT Format,
    OUT gctUINT32 * HardwareFormat,
    OUT gctBOOL * Flip,
    OUT gceMSAA_DOWNSAMPLE_MODE *DownsampleMode
    )
{
    gctUINT32 format;
    gctBOOL flip;
    gceMSAA_DOWNSAMPLE_MODE downsampleMode = gcvMSAA_DOWNSAMPLE_AVERAGE;

    switch (Format)
    {
    case gcvSURF_X4R4G4B4:
        format = 0x0;
        flip   = gcvFALSE;
        break;

    case gcvSURF_A4R4G4B4:
        format = 0x1;
        flip   = gcvFALSE;
        break;

    case gcvSURF_X1R5G5B5:
        format = 0x2;
        flip   = gcvFALSE;
        break;

    case gcvSURF_A1R5G5B5:
        format = 0x3;
        flip   = gcvFALSE;
        break;

    case gcvSURF_X1B5G5R5:
        format = 0x2;
        flip   = gcvTRUE;
        break;

    case gcvSURF_A1B5G5R5:
        format = 0x3;
        flip   = gcvTRUE;
        break;

    case gcvSURF_R5G6B5:
        format = 0x4;
        flip   = gcvFALSE;
        break;

    case gcvSURF_X8R8G8B8:
    case gcvSURF_R8_1_X8R8G8B8:
    case gcvSURF_G8R8_1_X8R8G8B8:
        format = 0x5;
        flip   = gcvFALSE;
        break;

    case gcvSURF_A8R8G8B8:
        format = 0x6;
        flip   = gcvFALSE;
        break;

    case gcvSURF_X8B8G8R8:
        format = 0x5;
        flip   = gcvTRUE;
        break;

    case gcvSURF_A8B8G8R8:
        format = 0x6;
        flip   = gcvTRUE;
        break;

    case gcvSURF_YUY2:
        if (((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 21:21) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1))))))))
        {
            format = 0x7;
            flip   = gcvFALSE;
            break;
        }
        return gcvSTATUS_INVALID_ARGUMENT;

    /* Fake 16-bit formats. */
    case gcvSURF_D16:
        if (Hardware->features[gcvFEATURE_RS_DEPTHSTENCIL_NATIVE_SUPPORT])
        {
            format = 0x18;
            flip = gcvFALSE;
            break;
        }
    /* else fall through */
    case gcvSURF_UYVY:
    case gcvSURF_YVYU:
    case gcvSURF_VYUY:
        format = 0x1;
        flip   = gcvFALSE;
        break;

    /* Fake 32-bit formats. */
    case gcvSURF_D24S8:
    case gcvSURF_D24X8:
    case gcvSURF_D32:
        if (Hardware->features[gcvFEATURE_RS_DEPTHSTENCIL_NATIVE_SUPPORT])
        {
            format = 0x17;
            flip = gcvFALSE;
        }
        else
        {
            format = 0x6;
            flip   = gcvFALSE;
        }
        break;

    case gcvSURF_A2B10G10R10:

        /* RS has decoding error for this format, which swapped RB channel, But PE is correct.
        */
        format = 0x16;
        flip = gcvTRUE;
        break;

    /*
    ** we still keep tile status buffer for 1-layer faked formats.
    ** only for disable tile status buffer.
    ** i.e.resolve to itself. other RS-blit will not work,
    ** which should go gcoSURF_CopyPixels path.
    */
    case gcvSURF_A2B10G10R10UI_1_A8R8G8B8:
    case gcvSURF_A8B8G8R8I_1_A8R8G8B8:
    case gcvSURF_A8B8G8R8UI_1_A8R8G8B8:
    case gcvSURF_G16R16I_1_A8R8G8B8:
    case gcvSURF_G16R16UI_1_A8R8G8B8:
    case gcvSURF_X32R32I_1_A8R8G8B8:
    case gcvSURF_X32R32UI_1_A8R8G8B8:
    case gcvSURF_B8G8R8I_1_A8R8G8B8:
    case gcvSURF_B8G8R8UI_1_A8R8G8B8:
    case gcvSURF_R32I_1_A8R8G8B8:
    case gcvSURF_R32UI_1_A8R8G8B8:
        format = 0x6;
        flip   = gcvFALSE;
        break;

    case gcvSURF_R8I_1_A4R4G4B4:
    case gcvSURF_R8UI_1_A4R4G4B4:
    case gcvSURF_R16I_1_A4R4G4B4:
    case gcvSURF_R16UI_1_A4R4G4B4:
    case gcvSURF_X8R8I_1_A4R4G4B4:
    case gcvSURF_X8R8UI_1_A4R4G4B4:
    case gcvSURF_G8R8I_1_A4R4G4B4:
    case gcvSURF_G8R8UI_1_A4R4G4B4:
    case gcvSURF_X16R16I_1_A4R4G4B4:
    case gcvSURF_X16R16UI_1_A4R4G4B4:
    case gcvSURF_X8G8R8I_1_A4R4G4B4:
    case gcvSURF_X8G8R8UI_1_A4R4G4B4:
        format = 0x1;
        flip   = gcvFALSE;
        break;

    case gcvSURF_S8:
        if (Hardware->features[gcvFEATURE_RS_DEPTHSTENCIL_NATIVE_SUPPORT])
        {
            format = 0x10;
            flip = gcvFALSE;
            downsampleMode = gcvMSAA_DOWNSAMPLE_SAMPLE;
            break;
        }
    /* else fall through */
    default:
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if (HardwareFormat != gcvNULL)
    {
        *HardwareFormat = format;
    }

    if (Flip != gcvNULL)
    {
        *Flip = flip;
    }

    if (DownsampleMode != gcvNULL)
    {
        *DownsampleMode = downsampleMode;
    }

    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  _AlignResolveRect
**
**  Align the specified rectangle to the resolve block requirements.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SurfInfo
**          Surface info.
**
**      gcsPOINT_PTR RectOrigin
**          Unaligned origin of the rectangle.
**
**      gcsPOINT_PTR RectSize
**          Unaligned size of the rectangle.
**
**  OUTPUT:
**
**      gcsPOINT_PTR AlignedOrigin
**          Resolve-aligned origin of the rectangle.
**
**      gcsPOINT_PTR AlignedSize
**          Resolve-aligned size of the rectangle.
*/
static void _AlignResolveRect(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo,
    IN gcsPOINT_PTR RectOrigin,
    IN gcsPOINT_PTR RectSize,
    OUT gcsPOINT_PTR AlignedOrigin,
    OUT gcsPOINT_PTR AlignedSize
    )
{
    gctUINT originX = 0;
    gctUINT originY = 0;
    gctUINT sizeX   = 0;
    gctUINT sizeY   = 0;
    gctUINT32 maskX;
    gctUINT32 maskY;

    /* Determine region's right and bottom coordinates. */
    gctINT32 right  = RectOrigin->x + RectSize->x;
    gctINT32 bottom = RectOrigin->y + RectSize->y;

    /* Determine the outside or "external" coordinates aligned for resolve
       to completely cover the requested rectangle. */
    gcoHARDWARE_GetSurfaceResolveAlignment(Hardware, SurfInfo, &originX, &originY, &sizeX, &sizeY);

    maskX = originX - 1;
    maskY = originY - 1;

    AlignedOrigin->x = RectOrigin->x & ~maskX;
    AlignedOrigin->y = RectOrigin->y & ~maskY;

    AlignedSize->x   = gcmALIGN(right  - AlignedOrigin->x, sizeX);
    AlignedSize->y   = gcmALIGN(bottom - AlignedOrigin->y, sizeY);
}

/*******************************************************************************
**
**  gcoHARDWARE_AlignResolveRect
**
**  Align the specified rectangle to the resolve block requirements.
**
**  INPUT:
**
**      gcsSURF_INFO_PTR SurfInfo
**          Surface info.
**
**      gcsPOINT_PTR RectOrigin
**          Unaligned origin of the rectangle.
**
**      gcsPOINT_PTR RectSize
**          Unaligned size of the rectangle.
**
**  OUTPUT:
**
**      gcsPOINT_PTR AlignedOrigin
**          Resolve-aligned origin of the rectangle.
**
**      gcsPOINT_PTR AlignedSize
**          Resolve-aligned size of the rectangle.
*/
gceSTATUS
gcoHARDWARE_AlignResolveRect(
    IN gcsSURF_INFO_PTR SurfInfo,
    IN gcsPOINT_PTR RectOrigin,
    IN gcsPOINT_PTR RectSize,
    OUT gcsPOINT_PTR AlignedOrigin,
    OUT gcsPOINT_PTR AlignedSize
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoHARDWARE Hardware = gcvNULL;

    gcmHEADER_ARG("SurfInfo=0x%x RectOrigin=0x%x RectSize=0x%x, AlignedOrigin=0x%x AlignedSize=0x%x",
        SurfInfo, RectOrigin, RectSize, AlignedOrigin, AlignedSize);

    gcmGETHARDWARE(Hardware);

    _AlignResolveRect(
            Hardware, SurfInfo, RectOrigin, RectSize, AlignedOrigin, AlignedSize
            );

OnError:
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  _ComputePixelLocation
**
**  Compute the offset of the specified pixel and determine its format.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcInfo
**
**      gctUINT X, Y
**          Pixel coordinates.
**
**      gctUINT Stride
**          Surface stride.
**
**      gcsSURF_FORMAT_INFO_PTR * Format
**          Pointer to the pixel format (even/odd pair).
**
**      gctBOOL Tiled
**          Surface tiled vs. linear flag.
**
**      gctBOOL SuperTiled
**          Surface super tiled vs. normal tiled flag.
**
**  OUTPUT:
**
**      gctUINT32 PixelOffset
**          Offset of the pixel from the beginning of the surface.
**
**      gcsSURF_FORMAT_INFO_PTR * PixelFormat
**          Specific pixel format of this pixel.
*/
static void _ComputePixelLocation(
    IN gcoHARDWARE Hardware,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Stride,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    IN gctBOOL Tiled,
    IN gctBOOL SuperTiled,
    OUT gctUINT32_PTR PixelOffset,
    OUT gctUINT32_PTR OddPixel
    )
{
    gctUINT8 bitsPerPixel = FormatInfo->bitsPerPixel;

    if (FormatInfo->interleaved)
    {
        /* Determine whether the pixel is at even or odd location. */
        *OddPixel = X & 1;

        /* Force to the even location for interleaved pixels. */
        X &= ~1;
    }
    else
    {
        *OddPixel = 0;
    }

    if (Tiled)
    {
        *PixelOffset
            /* Skip full rows of 64x64 tiles to the top. */
            = (Stride
            * gcmTILE_OFFSET_Y(X, Y, SuperTiled))

            + ((bitsPerPixel
               * gcmTILE_OFFSET_X(X, Y, SuperTiled, Hardware->config->superTileMode))
               / 8);
    }
    else
    {
        *PixelOffset
            = Y * Stride
            + X * bitsPerPixel / 8;
    }
}

#if gcdENABLE_2D
/*******************************************************************************
**
**  _BitBlitCopy
**
**  Make a copy of the specified rectangular area using 2D hardware.
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
static gceSTATUS
_BitBlitCopy(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUVformat;
    gctINT destRight, destBottom;

    gctUINT32 reserveSize;
    gcoCMDBUF reserve;

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Verify that the surfaces are locked. */
    gcmVERIFY_LOCK(SrcInfo);
    gcmVERIFY_LOCK(DestInfo);

    Hardware->enableXRGB = gcvTRUE;

    /* Determine the size of the buffer to reserve. */
    reserveSize
        /* Switch to 2D pipes. */
        = gcmALIGN(8 * gcmSIZEOF(gctUINT32), 8)

        /* Source setup. */
        + gcmALIGN((1 + 6) * gcmSIZEOF(gctUINT32), 8)

        /* Destination setup. */
        + gcmALIGN((1 + 4) * gcmSIZEOF(gctUINT32), 8)

        /* Clipping window setup. */
        + gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

        /* ROP setup. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Rotation setup. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Blit commands. */
        + 4 * gcmSIZEOF(gctUINT32)

        /* Flush. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

#if gcdENABLE_3D
        /* Switch to 3D pipes. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)
#endif
        ;

    Hardware->hw2DCmdBuffer = gcvNULL;
    Hardware->hw2DCmdIndex = 0;
    Hardware->hw2DCmdSize = reserveSize >> 2;

    gcmONERROR(gcoBUFFER_Reserve(
        Hardware->buffer,
        reserveSize,
        gcvTRUE,
        gcvCOMMAND_2D,
        &reserve
        ));

    Hardware->hw2DCmdBuffer = gcmUINT64_TO_PTR(reserve->lastReserve);

    reserve->using2D = gcvTRUE;

    /* Flush the 3D pipe. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x0380C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
        ));

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x03808,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
        ));

    Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex++] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
    Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex++] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x03800,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (gcvPIPE_2D) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        ));
    {
        gctUINT32 data[6];

        /***************************************************************************
        ** Setup source.
        */

        /* Convert source format. */
        gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
            Hardware, SrcInfo->format, &format, &swizzle, &isYUVformat
            ));

        data[0] = SrcInfo->node.physical;

        data[1] = SrcInfo->stride;

        data[2] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

        data[3] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));

        data[4] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcOrigin->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcOrigin->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

        data[5] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (RectSize->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (RectSize->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01200,
            6,
            data
            ));

        /***************************************************************************
        ** Setup destination.
        */
        /* Convert destination format. */
        gcmONERROR(gcoHARDWARE_TranslateDestinationFormat(
            Hardware, DestInfo->format, gcvTRUE, &format, &swizzle, &isYUVformat
            ));

        data[0] = DestInfo->node.physical;

        data[1] = DestInfo->stride;

        data[2] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

        data[3] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01228,
            4,
            data
            ));

        /***************************************************************************
        ** Setup clipping window.
        */
        destRight  = DestOrigin->x + RectSize->x;
        destBottom = DestOrigin->y + RectSize->y;

        data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (DestOrigin->x) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (DestOrigin->y) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (destRight) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (destBottom) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01260,
            2,
            data
            ));

        /***************************************************************************
        ** Blit the data.
        */

        /* Set ROP. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0125C,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (0xCC) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
            ));

        /* Set Rotation.*/
        if (Hardware->features[gcvFEATURE_2D_MIRROR_EXTENSION])
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012BC,
                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
                ));
        }
        else
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0126C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                ));
        }

        /* START_DE. */
        Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex++] =
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x04 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:16) - (0 ? 26:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:16) - (0 ? 26:16) + 1))))))) << (0 ? 26:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 26:16) - (0 ? 26:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:16) - (0 ? 26:16) + 1))))))) << (0 ? 26:16)));
        Hardware->hw2DCmdIndex++;

        /* DestRectangle. */
        Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex++] =
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (DestOrigin->x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (DestOrigin->y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

        Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex++] =
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (destRight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (destBottom) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));
    }

    gcmDUMP(gcvNULL, "@[prim2d 1 0x00000000");
    gcmDUMP(gcvNULL,
            "  %d,%d %d,%d",
            DestOrigin->x, DestOrigin->y, destRight, destBottom);
    gcmDUMP(gcvNULL, "] -- prim2d");

    /* Flush the 2D cache. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x0380C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))));

#if gcdENABLE_3D
    /* Select the 3D pipe. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x03800,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (gcvPIPE_3D) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        ));
#endif

    gcmASSERT(Hardware->hw2DCmdSize == Hardware->hw2DCmdIndex);


    /* Commit the command buffer. */
    gcmONERROR(gcoHARDWARE_Commit(Hardware));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    /* Return the status. */
    return status;
}
#endif

/*******************************************************************************
**
**  _SoftwareCopy
**
**  Make a copy of the specified rectangular area using CPU.
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
static gceSTATUS _SoftwareCopy(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(RectSize->x > 0);
    gcmDEBUG_VERIFY_ARGUMENT(RectSize->y > 0);

    do
    {
        gctBOOL srcTiled, dstTiled;
        gcsSURF_FORMAT_INFO_PTR srcFormatInfo;
        gcsSURF_FORMAT_INFO_PTR dstFormatInfo;
        gctUINT srcX, srcY;
        gctUINT dstX, dstY;
        gctUINT srcRight, srcBottom;
        gctUINT32 srcOffset, dstOffset, srcOdd, dstOdd;

        /* Verify that the surfaces are locked. */
        gcmVERIFY_LOCK(SrcInfo);
        gcmVERIFY_LOCK(DestInfo);

        /* Flush and stall the pipe. */
        gcmERR_BREAK(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));
        gcmERR_BREAK(gcoHARDWARE_Commit(Hardware));
        gcmERR_BREAK(gcoHARDWARE_Stall(Hardware));

        /* Invalidate surface cache before CPU memcopy */
        gcmERR_BREAK(
            gcoSURF_NODE_Cache(&SrcInfo->node,
                               SrcInfo->node.logical,
                               SrcInfo->size,
                               gcvCACHE_INVALIDATE));

        gcmERR_BREAK(
            gcoSURF_NODE_Cache(&DestInfo->node,
                               DestInfo->node.logical,
                               DestInfo->size,
                               gcvCACHE_INVALIDATE));

        /* Query format specifics. */
        srcFormatInfo = &SrcInfo->formatInfo;
        dstFormatInfo = &DestInfo->formatInfo;

        /* Determine whether the destination is tiled. */
        srcTiled = (SrcInfo->type  != gcvSURF_BITMAP);
        dstTiled = (DestInfo->type != gcvSURF_BITMAP);

        /* Test for fast copy. */
        if (srcTiled
        &&  dstTiled
        &&  (SrcInfo->format == DestInfo->format)
        &&  (SrcOrigin->x == 0)
        &&  (SrcOrigin->y == 0)
        &&  (RectSize->x == (gctINT) DestInfo->alignedWidth)
        &&  (RectSize->y == (gctINT) DestInfo->alignedHeight)
        )
        {
            gctUINT32 sourceOffset = 0;
            gctUINT32 targetOffset = 0;
            gctINT y;

            for (y = 0; y < RectSize->y; y += 4)
            {
                gcoOS_MemCopy(DestInfo->node.logical + targetOffset,
                              SrcInfo->node.logical  + sourceOffset,
                              DestInfo->stride * 4);

                sourceOffset += SrcInfo->stride  * 4;
                targetOffset += DestInfo->stride * 4;
            }

            break;
        }

        /* Set initial coordinates. */
        srcX = SrcOrigin->x;
        srcY = SrcOrigin->y;
        dstX = DestOrigin->x;
        dstY = DestOrigin->y;

        /* Compute limits. */
        srcRight  = SrcOrigin->x + RectSize->x - 1;
        srcBottom = SrcOrigin->y + RectSize->y - 1;

        if (!srcTiled
         && !dstTiled
         && srcX == 0
         && srcY == 0
         && dstX == 0
         && dstY == 0
         && SrcInfo->format == DestInfo->format
         && SrcInfo->stride == DestInfo->stride
        )
        {
            /* For same logical, it is not necessary to do the next */
            if (DestInfo->node.logical != SrcInfo->node.logical)
            gcoOS_MemCopy(DestInfo->node.logical, SrcInfo->node.logical, DestInfo->stride*RectSize->y);
            break;
        }
        else if (!srcTiled
         && !dstTiled
         && srcX == 0
         && srcY == 0
         && dstX == 0
         && dstY == 0
         && SrcInfo->format == DestInfo->format
        )
        {
            gctUINT32 sourceOffset = 0;
            gctUINT32 targetOffset = 0;
            gctINT y;

            for (y = 0; y < RectSize->y; y++)
            {
                gcoOS_MemCopy(DestInfo->node.logical + targetOffset,
                              SrcInfo->node.logical  + sourceOffset,
                                        (DestInfo->stride > SrcInfo->stride ? SrcInfo->stride : DestInfo->stride));
                sourceOffset += SrcInfo->stride;
                targetOffset += DestInfo->stride;
            }

            break;
        }

        /* Loop through the rectangle. */
        while (gcvTRUE)
        {
            gctUINT8_PTR srcPixel, dstPixel;

            _ComputePixelLocation(
                Hardware, srcX, srcY, SrcInfo->stride,
                srcFormatInfo, srcTiled, SrcInfo->superTiled,
                &srcOffset, &srcOdd
                );

            _ComputePixelLocation(
                Hardware, dstX, dstY, DestInfo->stride,
                dstFormatInfo, dstTiled, DestInfo->superTiled,
                &dstOffset, &dstOdd
                );

            srcPixel = SrcInfo->node.logical  + srcOffset;
            dstPixel = DestInfo->node.logical + dstOffset;

            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                srcPixel,
                dstPixel,
                0, 0,
                srcFormatInfo,
                dstFormatInfo,
                gcvNULL,
                gcvNULL,
                srcOdd,
                dstOdd
                ));

            /* End of line? */
            if (srcX == srcRight)
            {
                /* Last row? */
                if (srcY == srcBottom)
                {
                    break;
                }

                /* Reset to the beginning of the line. */
                srcX = SrcOrigin->x;
                dstX = DestOrigin->x;

                /* Advance to the next line. */
                srcY++;
                dstY++;
            }
            else
            {
                /* Advance to the next pixel. */
                srcX++;
                dstX++;
            }
        }
    }
    while (gcvFALSE);

    gcmVERIFY_OK(
        gcoSURF_NODE_Cache(&DestInfo->node,
                           DestInfo->node.logical,
                           DestInfo->size,
                           gcvCACHE_CLEAN));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  _SourceCopy
**
**  Make a copy of the specified rectangular area.
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
static gceSTATUS _SourceCopy(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    do
    {
        /* Only support BITMAP to BITMAP or TEXTURE to TEXTURE copy for now. */
        if (!(((SrcInfo->type == gcvSURF_BITMAP) && (DestInfo->type == gcvSURF_BITMAP))
        ||    ((SrcInfo->type == gcvSURF_TEXTURE) && (DestInfo->type == gcvSURF_TEXTURE)))
        )
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* Is 2D pipe present? */
        if (Hardware->hw2DEngine
        &&  !Hardware->sw2DEngine
        /* GC500 needs properly aligned surfaces. */
        &&  ((Hardware->config->chipModel != gcv500)
            || ((DestInfo->rect.right & 7) == 0)
            )
        )
        {
#if gcdENABLE_2D
            status = _BitBlitCopy(
                Hardware,
                SrcInfo,
                DestInfo,
                SrcOrigin,
                DestOrigin,
                RectSize
                );
#else
            status = gcvSTATUS_NOT_SUPPORTED;
#endif
        }
        else
        {
            status = _SoftwareCopy(
                Hardware,
                SrcInfo,
                DestInfo,
                SrcOrigin,
                DestOrigin,
                RectSize
                );
        }
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  _Tile420Surface
**
**  Tile linear 4:2:0 source surface.
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
static gceSTATUS _Tile420Surface(
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
    gctBOOL tilerAvailable;
    gctUINT32 uvSwizzle;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Verify that the surfaces are locked. */
    gcmVERIFY_LOCK(SrcInfo);
    gcmVERIFY_LOCK(DestInfo);

    /* Determine hardware support for 4:2:0 tiler. */
    tilerAvailable = ((((gctUINT32) (Hardware->config->chipFeatures)) >> (0 ? 13:13) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1)))))));

    /* Available? */
    if (!tilerAvailable)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Input limitations until more support is required. */
    if ((SrcOrigin->x  != 0) || (SrcOrigin->y  != 0) ||
        (DestOrigin->x != 0) || (DestOrigin->y != 0)
        )

    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Append FLUSH to the command buffer. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Determine the size of the buffer to reserve. */
    reserveSize

        /* Tiler configuration. */
        = gcmALIGN((1 + 10) * gcmSIZEOF(gctUINT32), 8)

        /* Single states. */
        + gcmALIGN((1 + 1)  * gcmSIZEOF(gctUINT32) * 4, 8);

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2;
        reserveSize += 36 * gcmSIZEOF(gctUINT32);
    }
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

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


    /***************************************************************************
    ** Set tiler configuration.
    */

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Sync between GPUs */
        gcoHARDWARE_MultiGPUSync(Hardware, gcvFALSE, &memory);
        /* Select GPU 3D_0. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };
    }
#endif

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


    /***************************************************************************
    ** Disable clear.
    */

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058F, 0 );     gcmENDSTATEBATCH(reserve, memory);};


    /***************************************************************************
    ** Disable 256B cache setting
    */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0583, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) );     gcmENDSTATEBATCH(reserve, memory);};

    /***************************************************************************
    ** Trigger resolve.
    */

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0580, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0580) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0580, 0xBADABEEB );gcmENDSTATEBATCH(reserve, memory);};


    /***************************************************************************
    ** Disable tiler.
    */

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x059E, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x059E) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x059E, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );     gcmENDSTATEBATCH(reserve, memory);};

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Enable all 3D GPUs. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };
        /* Sync between GPUs */
        gcoHARDWARE_MultiGPUSync(Hardware, gcvFALSE, &memory);
    }
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Commit the command buffer. */
    gcmONERROR(gcoHARDWARE_Commit(Hardware));

    /* Return the status. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

typedef struct
{
    gctUINT32 mode;
    gctUINT32 horFactor;
    gctUINT32 verFactor;
}
gcsSUPERENTRY, *gcsSUPERENTRY_PTR;

typedef struct
{
    /* Source information. */
    gcsSURF_FORMAT_INFO_PTR srcFormatInfo;
    gceTILING srcTiling;
    gctBOOL srcMultiPipe;

    /* Destination information. */
    gcsSURF_FORMAT_INFO_PTR dstFormatInfo;
    gceTILING dstTiling;
    gctBOOL dstMultiPipe;

    /* Resolve information. */
    gctBOOL flipY;
    gcsSUPERENTRY_PTR superSampling;
}
gcsRESOLVE_VARS,
* gcsRESOLVE_VARS_PTR;

static gceSTATUS
_StripeResolve(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gcsRESOLVE_VARS_PTR Vars
    )
{
    gceSTATUS status;
    gcsRECT srcRect, dstRect;
    gctINT32 x, xStep, y, yStep;
    gctINT32 width, height;
    gctUINT32 srcOffset, dstOffset;
    gctINT hShift, vShift;
    gctINT32 dstY;
    gctUINT  loopCountX, loopCountY;
    gctUINT resolveCount;
    gctUINT32 srcAddress = 0, dstAddress = 0;
    gctUINT32 windowSize = 0;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    /* Copy super-sampling factor. */
    hShift = (Vars->superSampling->horFactor == 1) ? 0 : 1;
    vShift = (Vars->superSampling->verFactor == 1) ? 0 : 1;

    /* Compute source bounding box. */
    srcRect.left   = SrcOrigin->x & ~15;
    srcRect.right  = gcmALIGN(SrcOrigin->x + (RectSize->x << hShift), 16);
    srcRect.top    = SrcOrigin->y & ~3;
    srcRect.bottom = gcmALIGN(SrcOrigin->y + (RectSize->y << vShift), 4);

    /* Compute destination bounding box. */
    dstRect.left   = DestOrigin->x & ~15;
    dstRect.right  = gcmALIGN(DestOrigin->x + RectSize->x, 16);
    dstRect.top    = DestOrigin->y & ~3;
    dstRect.bottom = gcmALIGN(DestOrigin->y + RectSize->y, 4);

    /* no split buffer fall into this function */
    gcmASSERT((SrcInfo->tiling & gcvTILING_SPLIT_BUFFER) == 0);
    gcmASSERT((DestInfo->tiling & gcvTILING_SPLIT_BUFFER) == 0);

    /* Count the number of resolves needed. */
    resolveCount = 0;

    for (x = srcRect.left, loopCountX =0; (x < srcRect.right) && (loopCountX < MAX_LOOP_COUNT); x += xStep, loopCountX++)
    {
        /* Compute horizontal step. */

        xStep = (x & 63) ? (x & 31) ? 16 : 32 : 64;
        gcmASSERT((x & ~63) == ((x + xStep - 1) & ~63));
        yStep = 16 * Hardware->needStriping / xStep;

        for (y = srcRect.top, loopCountY=0; (y < srcRect.bottom) && (loopCountY < MAX_LOOP_COUNT); y += yStep, loopCountY++)
        {
            /* Compute vertical step. */
            yStep = 16 * Hardware->needStriping / xStep;
            if ((y & ~63) != ((y + yStep - 1) & ~63))
            {
                yStep = gcmALIGN(y, 64) - y;
            }

            /* Update the number of resolves. */
            resolveCount += 1;
        }
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = (1 + 1) * 4 * 4 * resolveCount;

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Walk all stripes horizontally. */
    for (x = srcRect.left, loopCountX = 0; (x < srcRect.right) && (loopCountX < MAX_LOOP_COUNT); x += xStep, loopCountX++)
    {
        /* Compute horizontal step. */
        xStep = (x & 63) ? (x & 31) ? 16 : 32 : 64;
        gcmASSERT((x & ~63) == ((x + xStep - 1) & ~63));
        yStep = 16 * Hardware->needStriping / xStep;

        /* Compute width. */
        width = gcmMIN(srcRect.right - x, xStep);

        /* Walk the stripe vertically. */
        for (y = srcRect.top, loopCountY = 0; (y < srcRect.bottom) && (loopCountY < MAX_LOOP_COUNT); y += yStep, loopCountY++)
        {
            /* Compute vertical step. */
            yStep = 16 * Hardware->needStriping / xStep;
            if ((y & ~63) != ((y + yStep - 1) & ~63))
            {
                /* Don't overflow a super tile. */
                yStep = gcmALIGN(y, 64) - y;
            }

            /* Compute height. */
            height = gcmMIN(srcRect.bottom - y, yStep);

            /* Compute destination y. */
            dstY = Vars->flipY ? (dstRect.bottom - (y >> vShift) - height)
                               : (y >> vShift);

            /* Compute offsets. */
            gcmONERROR(
                gcoHARDWARE_ComputeOffset(Hardware,
                                          x, y,
                                          SrcInfo->stride,
                                          Vars->srcFormatInfo->bitsPerPixel / 8,
                                          Vars->srcTiling, &srcOffset));
            gcmONERROR(
                gcoHARDWARE_ComputeOffset(Hardware,
                                          x >> hShift, dstY,
                                          DestInfo->stride,
                                          Vars->dstFormatInfo->bitsPerPixel / 8,
                                          Vars->dstTiling, &dstOffset));

            /* Determine state values. */
            srcAddress = SrcInfo->node.physical + srcOffset;
            dstAddress = DestInfo->node.physical + dstOffset;
            windowSize = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (width) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                       | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (height) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

            /* Resolve one part of the stripe. */
            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0582, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0582) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0582, srcAddress );gcmENDSTATEBATCH(reserve, memory);};

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0584, dstAddress );gcmENDSTATEBATCH(reserve, memory);};

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0588, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0588) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0588, windowSize );gcmENDSTATEBATCH(reserve, memory);};

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0580, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0580) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0580, 0xBADABEEB );gcmENDSTATEBATCH(reserve, memory);};
        }
    }

    stateDelta = stateDelta; /* keep compiler happy */

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Return the error. */
    return status;
}

/*******************************************************************************
**
**  _ResolveRect
**
**  Perform a resolve on a surface.
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
**          The origin of the source area to be resolved.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be resolved.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be resolved.
**
**      gctBOOL yInverted
**          The flag indicated if we need yInvertedly resolve source to dest.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS _ResolveRect(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gctBOOL yInverted
    )
{
#   define _AA(HorFactor, VerFactor) \
    { \
        AQ_RS_CONFIG_RS_SRC_SUPER_SAMPLE_ENABLE \
            ## HorFactor ## X ## VerFactor, \
        HorFactor, \
        VerFactor \
    }

#   define _VAA \
    { \
        0x1, \
        2, \
        1 \
    }

#   define _NOAA \
    { \
        0x0, \
        1, \
        1 \
    }

#   define _INVALIDAA \
    { \
        ~0U, \
        ~0U, \
        ~0U \
    }

    static gcsSUPERENTRY superSamplingTable[17] =
    {
        /*  SOURCE 1x1                 SOURCE 2x1 */

            /* DEST 1x1  DEST 2x1      DEST 1x1  DEST 2x1 */
            _NOAA, _INVALIDAA, { 0x1, 2, 1}, _NOAA,

            /* DEST 1x2  DEST 2x2      DEST 1x2  DEST 2x2 */
            _INVALIDAA, _INVALIDAA, _INVALIDAA, _INVALIDAA,

        /*  SOURCE 1x2                 SOURCE 2x2 */

            /* DEST 1x1  DEST 2x1      DEST 1x1  DEST 2x1 */
            { 0x2, 1, 2}, _INVALIDAA, { 0x3, 2, 2}, { 0x2, 1, 2},

            /* DEST 1x2  DEST 2x2      DEST 1x2  DEST 2x2 */
            _NOAA, _INVALIDAA, { 0x1, 2, 1}, _NOAA,

            /* VAA */
            _VAA,
    };

    gceSTATUS status;
    gcsRESOLVE_VARS vars;
    gcsPOINT srcOrigin;
    gcsPOINT dstOrigin;
    gcsPOINT srcSize;
    gctUINT32 config;
    gctUINT32 srcAddress, dstAddress;
    gctUINT32 srcAddress2 = 0, dstAddress2 = 0;
    gctUINT32 srcStride, dstStride;
    gctUINT32 srcOffset, dstOffset;
    gctUINT32 srcFormat, dstFormat;
    gctUINT superSamplingIndex;
    gctBOOL srcFlip;
    gctBOOL dstFlip;
    gctUINT32 vaa, endian;
    gctBOOL needStriping;
    gctUINT32 *pDitherTable;
    gctBOOL gammarCorrection = gcvFALSE;
    gceMSAA_DOWNSAMPLE_MODE srcDownsampleMode;

    gctBOOL multiPipe;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x SrcOrigin=%d,%d "
                  "DestOrigin=%d,%d RectSize=%d,%d",
                  Hardware, SrcInfo, DestInfo, SrcOrigin->x, SrcOrigin->y,
                  DestOrigin->x, DestOrigin->y, RectSize->x, RectSize->y);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT((SrcOrigin->x & 3) == 0);
    gcmDEBUG_VERIFY_ARGUMENT((SrcOrigin->y & 3) == 0);

    gcmDEBUG_VERIFY_ARGUMENT((DestOrigin->x & 3) == 0);
    gcmDEBUG_VERIFY_ARGUMENT((DestOrigin->y & 3) == 0);

    gcmDEBUG_VERIFY_ARGUMENT((RectSize->x & 15) == 0);
    gcmDEBUG_VERIFY_ARGUMENT((RectSize->y &  3) == 0);

    /* Verify that the surfaces are locked. */
    gcmVERIFY_LOCK(SrcInfo);
    gcmVERIFY_LOCK(DestInfo);

    /* Convert source and destination formats. */
    gcmONERROR(
        _ConvertResolveFormat(Hardware,
                              SrcInfo->format,
                              &srcFormat,
                              &srcFlip,
                              &srcDownsampleMode));

    if (SrcInfo->formatInfo.format == gcvSURF_UNKNOWN)
    {
        gcmONERROR(
            gcoSURF_QueryFormat(SrcInfo->format, &vars.srcFormatInfo));
    }
    else
    {
        vars.srcFormatInfo = &SrcInfo->formatInfo;
    }

    gcmONERROR(
        _ConvertResolveFormat(Hardware,
                              DestInfo->format,
                              &dstFormat,
                              &dstFlip,
                              gcvNULL));

    if (DestInfo->formatInfo.format == gcvSURF_UNKNOWN)
    {
        gcmONERROR(
            gcoSURF_QueryFormat(DestInfo->format, &vars.dstFormatInfo));
    }
    else
    {
        vars.dstFormatInfo = &DestInfo->formatInfo;
    }

    /*
    ** Actually RS gamma correct has precision issue. so we will not use this feature any more.
    **
    if ((DestInfo->colorSpace == gcvSURF_COLOR_SPACE_NONLINEAR) && (SrcInfo->colorSpace == gcvSURF_COLOR_SPACE_LINEAR))
    {
        gammarCorrection = gcvTRUE;
    }
    */

    /* Determine if y flipping is required. */
    vars.flipY = (SrcInfo->orientation != DestInfo->orientation) || yInverted;


    if (vars.flipY
    &&  !Hardware->features[gcvFEATURE_FLIP_Y])
    {
        DestInfo->orientation = SrcInfo->orientation;
        vars.flipY            = gcvFALSE;
    }

    /* Determine source tiling. */
    vars.srcTiling = SrcInfo->tiling;

    vars.srcMultiPipe = (SrcInfo->tiling & gcvTILING_SPLIT_BUFFER);

    /* Determine destination tiling. */
    vars.dstTiling = DestInfo->tiling;

    vars.dstMultiPipe = (DestInfo->tiling & gcvTILING_SPLIT_BUFFER);

    multiPipe = vars.srcMultiPipe || vars.dstMultiPipe || Hardware->multiPipeResolve;

    /* Determine vaa value. */
    vaa = SrcInfo->vaa
        ? (srcFormat == 0x06)
          ? 0x2
          : 0x1
        : 0x0;

    /* Determine the supersampling mode. */
    if (SrcInfo->vaa && !DestInfo->vaa)
    {
        superSamplingIndex = 16;
    }
    else
    {
        superSamplingIndex = (SrcInfo->samples.y  << 3)
                           + (DestInfo->samples.y << 2)
                           + (SrcInfo->samples.x  << 1)
                           +  DestInfo->samples.x
                           -  15;

        if (SrcInfo->vaa && DestInfo->vaa)
        {
            srcFormat = 0x06;
            dstFormat = 0x06;
            vaa       = 0x0;
        }
    }

    vars.superSampling = &superSamplingTable[superSamplingIndex];

    /* Supported mode? */
    if (vars.superSampling->mode == ~0U)
    {
        gcmTRACE(
            gcvLEVEL_ERROR,
            "Supersampling mode is not defined for the given configuration:\n"
            );

        gcmTRACE(
            gcvLEVEL_ERROR,
            "  SrcInfo->vaa      = %d\n", SrcInfo->vaa
            );

        gcmTRACE(
            gcvLEVEL_ERROR,
            "  SrcInfo->samples  = %dx%d\n",
            SrcInfo->samples.x, SrcInfo->samples.y
            );

        gcmTRACE(
            gcvLEVEL_ERROR,
            "  DestInfo->vaa     = %d\n",
            DestInfo->vaa
            );

        gcmTRACE(
            gcvLEVEL_ERROR,
            "  DestInfo->samples = %dx%d\n",
            DestInfo->samples.x, DestInfo->samples.y
            );

        gcmTRACE(
            gcvLEVEL_ERROR,
            "superSamplingIndex  = %d\n",
            superSamplingIndex
            );

        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Flush the pipe. */
    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    /* Switch to 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Construct configuration state. */
    config

        /* Configure source. */
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) ((gctUINT32) (srcFormat) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (vars.srcTiling != gcvLINEAR) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))

        /* Configure destination. */
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) ((gctUINT32) (dstFormat) & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) ((gctUINT32) (vars.dstTiling != gcvLINEAR) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)))

        /* Configure flipping. */
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) ((srcFlip ^ dstFlip)) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) (vars.flipY) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)))

        /* Configure supersampling enable. */
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) ((gctUINT32) (vars.superSampling->mode) & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5)))
        /* gammar correction */
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) ((gctUINT32) (gammarCorrection) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));

    /* Determine the source stride. */
    srcStride = (vars.srcTiling == gcvLINEAR)

              /* Linear. */
              ? SrcInfo->stride

              /* Tiled. */
              : ((((gctUINT32) (SrcInfo->stride * 4)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (((vars.srcTiling & gcvSUPERTILED) ? 1 : 0)) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));

    if (vars.srcMultiPipe)
    {
        srcStride = ((((gctUINT32) (srcStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));
    }

    /* Determine the destination stride. */
    dstStride = (vars.dstTiling == gcvLINEAR)

              /* Linear. */
              ? DestInfo->stride

              /* Tiled. */
              : ((((gctUINT32) (DestInfo->stride * 4)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (((vars.dstTiling & gcvSUPERTILED) ? 1 : 0)) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));

    if (vars.dstMultiPipe)
    {
        dstStride = ((((gctUINT32) (dstStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));
    }

    /* For Fast MSAA mode, we always need enable 256B cache mode
    ** SMALL MSAA is an upgraded version of Fast MSAA
    */
    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        gctUINT32 srcBaseAddress = SrcInfo->node.physical;

        if ((SrcInfo->samples.x > 1)
          && (SrcInfo->samples.y > 1)
          && ((SrcInfo->type == gcvSURF_RENDER_TARGET)
            || (SrcInfo->type == gcvSURF_DEPTH))
          && (srcBaseAddress == gcmALIGN(srcBaseAddress, 256))
          && (SrcInfo->stride == gcmALIGN(SrcInfo->stride, 256))
           )
        {
            srcStride = ((((gctUINT32) (srcStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
        }
        else
        {
            srcStride = ((((gctUINT32) (srcStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
        }


        if (DestInfo->samples.x * DestInfo->samples.y > SrcInfo->samples.x * SrcInfo->samples.y)
        {
            /* With fast msaa, we cannot resolve onto itself.
               The code path leading to this point should be changed. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

    }

    /* Set endian control */
    endian = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x0  & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));

    if (Hardware->bigEndian &&
        (SrcInfo->type != gcvSURF_TEXTURE) &&
        (DestInfo->type == gcvSURF_BITMAP))
    {
        gctUINT32 destBPP;

        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(
            DestInfo->format, &destBPP, gcvNULL
            ));

        if (destBPP == 16)
        {
            endian = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
        }
        else if (destBPP == 32)
        {
            endian = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2  & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
        }
    }

    /* Determine whether resolve striping is needed. */
    needStriping
        =  Hardware->needStriping
        && (((((gctUINT32) (Hardware->memoryConfig)) >> (0 ? 1:1)) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) );

    /* Determine the size of the buffer to reserve. */
    reserveSize

        = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 3, 8)

        + gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 2, 8);

    if (!needStriping)
    {
        if (SrcInfo->tiling & gcvTILING_SPLIT_BUFFER)
        {
            reserveSize
                /* Source address state */
                += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);

        }
        else
        {
            reserveSize
                /* Source address state */
                += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

            /* New source address */
            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                reserveSize
                    += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
            }
        }

        if (DestInfo->tiling & gcvTILING_SPLIT_BUFFER)
        {
            reserveSize
                /* Source address state */
                += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);

        }
        else
        {
            reserveSize
                /* Target address state */
                += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

            /* New Target address */
            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                reserveSize
                    += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
            }
        }
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Append RESOLVE_CONFIG state. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0581, config );     gcmENDSTATEBATCH(reserve, memory);};

    /* Set source and destination stride. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0583, srcStride );     gcmENDSTATEBATCH(reserve, memory);};

    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0585, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0585) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0585, dstStride );     gcmENDSTATEBATCH(reserve, memory);};

    /* Determine dithering. */
    if (SrcInfo->deferDither3D &&
        (vars.srcFormatInfo->bitsPerPixel > vars.dstFormatInfo->bitsPerPixel))
    {
        pDitherTable = &Hardware->ditherTable[gcvTRUE][0];
    }
    else
    {
        pDitherTable = &Hardware->ditherTable[gcvFALSE][0];
    }

    /* Append RESOLVE_DITHER commands. */
    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058C, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x058C,
            pDitherTable[0]
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x058C + 1,
            pDitherTable[1]
            );

        gcmSETFILLER(
            reserve, memory
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    /* Append RESOLVE_CLEAR_CONTROL state. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x058F, 0 );     gcmENDSTATEBATCH(reserve, memory);};

    /* Append new configuration register. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05A8, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (vaa) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (!Hardware->multiPipeResolve) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | endian );     gcmENDSTATEBATCH(reserve, memory);};

    if (needStriping)
    {

        stateDelta = stateDelta; /* keep compiler happy */
        /* Validate the state buffer. */
        gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

        /* Stripe the resolve. */
        gcmONERROR(_StripeResolve(Hardware,
                                  SrcInfo,
                                  DestInfo,
                                  SrcOrigin,
                                  DestOrigin,
                                  RectSize,
                                  &vars));
    }
    else
    {
        /* Determine the origins. */
        srcOrigin.x = SrcOrigin->x  * SrcInfo->samples.x;
        srcOrigin.y = SrcOrigin->y  * SrcInfo->samples.y;
        dstOrigin.x = DestOrigin->x * DestInfo->samples.x;
        dstOrigin.y = DestOrigin->y * DestInfo->samples.y;

        /* Determine the source base address offset. */
        gcmONERROR(
            gcoHARDWARE_ComputeOffset(Hardware,
                                      srcOrigin.x, srcOrigin.y,
                                      SrcInfo->stride,
                                      vars.srcFormatInfo->bitsPerPixel / 8,
                                      vars.srcTiling, &srcOffset));

        /* Determine the destination base address offset. */
        gcmONERROR(
            gcoHARDWARE_ComputeOffset(Hardware,
                                      dstOrigin.x, dstOrigin.y,
                                      DestInfo->stride,
                                      vars.dstFormatInfo->bitsPerPixel / 8,
                                      vars.dstTiling, &dstOffset));

        /* Determine base addresses. */
        srcAddress = SrcInfo->node.physical  + srcOffset + SrcInfo->offset;
        dstAddress = DestInfo->node.physical + dstOffset + DestInfo->offset;

        /* Determine source rectangle size. */
        srcSize.x = RectSize->x * vars.superSampling->horFactor;
        srcSize.y = RectSize->y * vars.superSampling->verFactor;

        if (SrcInfo->tiling & gcvTILING_SPLIT_BUFFER)
        {
             srcAddress2 = srcAddress + SrcInfo->bottomBufferOffset;

            /* Append RESOLVE_SOURCE addresses. */
             {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B0, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                 gcmSETSTATEDATA(
                     stateDelta, reserve, memory, gcvFALSE, 0x05B0,
                     srcAddress
                     );

                 gcmSETSTATEDATA(
                     stateDelta, reserve, memory, gcvFALSE, 0x05B0 + 1,
                     srcAddress2
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
            /* Append RESOLVE_SOURCE commands. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0582, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0582) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0582, srcAddress );     gcmENDSTATEBATCH(reserve, memory);};

            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                /* Append RESOLVE_SOURCE commands. */
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B0, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B0, srcAddress );     gcmENDSTATEBATCH(reserve, memory);};
            }
        }

        if (DestInfo->tiling & gcvTILING_SPLIT_BUFFER)
        {
            dstAddress2 = dstAddress + DestInfo->bottomBufferOffset;

            /* Append RESOLVE_DESTINATION addresses. */
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8,
                    dstAddress
                    );

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x05B8 + 1,
                    dstAddress2
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
            /* Append RESOLVE_DESTINATION commands. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0584, dstAddress );     gcmENDSTATEBATCH(reserve, memory);};
            if ((Hardware->config->pixelPipes > 1) ||
                 Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
            {
                /* Append RESOLVE_DESTINATION commands. */
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05B8, dstAddress );     gcmENDSTATEBATCH(reserve, memory);};
            }
        }

        /* Validate the state buffer. */
        gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

        if (!SrcInfo->superTiled && !DestInfo->superTiled && (DestInfo->tiling == gcvLINEAR) &&
            (Hardware->config->chipModel == gcv2000) && (Hardware->config->chipRevision == 0x5108) &&
            (!Hardware->features[gcvFEATURE_SUPER_TILED]))
        {
             gctINT32 i, h = 8;
             gctUINT32 dstBufSize = (DestInfo->alignedHeight - h) * DestInfo->stride;
             if(vars.flipY)
             {
                 for (i = 0; i < srcSize.y/h; i++)
                 {
                    gcsPOINT newSize;
                    newSize.x = srcSize.x;
                    newSize.y = h;

                    gcoHARDWARE_LoadState32(Hardware, 0x16C0, srcAddress  + i*(h/2)*SrcInfo->stride);
                    gcoHARDWARE_LoadState32(Hardware, 0x16C4, srcAddress2 + i*(h/2)*SrcInfo->stride);

                    gcoHARDWARE_LoadState32(Hardware, 0x16E0, dstAddress + dstBufSize - (i*h*DestInfo->stride)/SrcInfo->samples.y);
                    gcoHARDWARE_LoadState32(Hardware, 0x16E4, dstAddress + dstBufSize - (i*h*DestInfo->stride + (h/2)*DestInfo->stride)/SrcInfo->samples.y);
                    /* Program the resolve window and trigger it. */
                    gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware, newSize, multiPipe, srcDownsampleMode));
                 }
             }
             else
             {
                 for (i = 0; i < srcSize.y/h; i++)
                 {
                    gcsPOINT newSize;
                    newSize.x = srcSize.x;
                    newSize.y = h;

                    gcoHARDWARE_LoadState32(Hardware, 0x16C0, srcAddress  + i*(h/2)*SrcInfo->stride);
                    gcoHARDWARE_LoadState32(Hardware, 0x16C4, srcAddress2 + i*(h/2)*SrcInfo->stride);

                    gcoHARDWARE_LoadState32(Hardware, 0x16E0, dstAddress + (i*h*DestInfo->stride)/SrcInfo->samples.y);
                    gcoHARDWARE_LoadState32(Hardware, 0x16E4, dstAddress + (i*h*DestInfo->stride + (h/2)*DestInfo->stride)/SrcInfo->samples.y);
                    /* Program the resolve window and trigger it. */
                    gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware, newSize, multiPipe, srcDownsampleMode));
                 }
             }
        }
        else
        {
             /* Program the resolve window and trigger it. */
             gcmONERROR(gcoHARDWARE_ProgramResolve(Hardware, srcSize, multiPipe, srcDownsampleMode));
        }
    }

    /* Tile status cache is dirty. */
    Hardware->cacheDirty = gcvTRUE;

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
**  gcoHARDWARE_ComputeOffset
**
**  Compute the offset of the specified pixel location.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctINT32 X, Y
**          Coordinates of the pixel.
**
**      gctUINT Stride
**          Surface stride.
**
**      gctINT BytesPerPixel
**          The number of bytes per pixel in the surface.
**
**      gctINT Tiling
**          Tiling type.
**
**  OUTPUT:
**
**      Computed pixel offset.
*/
gceSTATUS gcoHARDWARE_ComputeOffset(
    IN gcoHARDWARE Hardware,
    IN gctINT32 X,
    IN gctINT32 Y,
    IN gctUINT Stride,
    IN gctINT BytesPerPixel,
    IN gceTILING Tiling,
    OUT gctUINT32_PTR Offset
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    if ((X == 0) && (Y == 0))
    {
        *Offset = 0;
    }
    else
    {
        if (Tiling == gcvLINEAR)
        {
            /* Linear. */
            * Offset

                /* Skip full rows of pixels. */
                = Y * Stride

                /* Skip pixels to the left. */
                + X * BytesPerPixel;
        }
        else
        {
            gctBOOL superTiled = (Tiling & gcvSUPERTILED) > 1;

            gcmGETHARDWARE(Hardware);

            if (Tiling & gcvTILING_SPLIT_BUFFER)
            {
                /* Calc offset in the tile part of one PE. */
                /* Adjust coordinates from split PE into one PE. */
                X = (X  & ~0x8) | ((Y & 0x4) << 1);
                Y = ((Y & ~0x7) >> 1) | (Y & 0x3);
            }

            * Offset

                /* Skip full rows of 64x64 tiles to the top. */
                = (Stride
                   * gcmTILE_OFFSET_Y(X, Y, superTiled))

                + (BytesPerPixel
                   * gcmTILE_OFFSET_X(X, Y, superTiled, Hardware->config->superTileMode));
        }
    }

OnError:
    /* Return status. */
    return status;
}


/* We need to program those pesky 256-byte cache configuration bits inside
 * the Pixel Engine - because that is where the Tile Status is looking at
 * as well. And who guarantees this is the same surface as the current one?
 * And then - we also have to make the states dirty so we can reprogram them
 * if required.
 */
gceSTATUS
gcoHARDWARE_AdjustCacheMode(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x, SrcInfo=0x%x", Hardware, SrcInfo);

    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        if ((SrcInfo->format <= gcvSURF_YUY2) ||    /* Skip unsupported format by FlushTarget. */
            (SrcInfo->format > gcvSURF_VYUY))
        {
            gctUINT samples;
            gctUINT cacheMode;

            /* Check how many samples the surface is. */
            samples = SrcInfo->samples.x * SrcInfo->samples.y;

            /* Check cache mode. */
            cacheMode = (samples == 4);

            /* Check if the cache mode is different. */
            if (cacheMode != Hardware->colorStates.cacheMode)
            {
                gctUINT32 msaaMode;
                gctBOOL msaaEnable;
                /* Define state buffer variables. */
                gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

                msaaMode = (samples == 4) ? 0x2 :
                                            0x0;

                msaaEnable = (samples == 4) ? gcvTRUE : gcvFALSE;

                reserveSize = /* Flush cache */
                              gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)       +
                              /* semaphore stall */
                              (gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2) +
                              /* cache control */
                              gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)       +
                              /* MSAA mode */
                              gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

                /* Switch to 3D pipe. */
                gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

                /* Reserve space in the command buffer. */
                gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);


                {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );gcmENDSTATEBATCH(reserve, memory);};

                /* We also have to sema stall between FE and PE to make sure the flush
                ** has happened. */
                {     {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E02, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0E02, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));     gcmENDSTATEBATCH(reserve, memory);};    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));    gcmDUMP(gcvNULL, "@[stall 0x%08X 0x%08X]",    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));    gcmSKIPSECUREUSER();    }

                /* We need to program fast msaa config if the cache mode is changing. */
                {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0529, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0529) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATAWITHMASK(stateDelta, reserve, memory, gcvFALSE, 0x0529, (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)))) | (((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) |    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27)))), (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) ((samples == 4) ? 1 : 0) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)))) & (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) ((samples == 4) ? 1 : 0) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27)))));     gcmENDSTATEBATCH(reserve, memory);};

                /* Set multi-sample mode. */
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E06, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E06) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0E06, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (msaaMode) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:12) - (0 ? 14:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:12) - (0 ? 14:12) + 1))))))) << (0 ? 14:12))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (msaaEnable) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) );     gcmENDSTATEBATCH(reserve, memory);};


                gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

                Hardware->colorStates.cacheMode = cacheMode;

                Hardware->msaaModeDirty    = gcvTRUE;
                Hardware->msaaConfigDirty  = gcvTRUE;

                /* Test for error. */
                gcmONERROR(status);
            }
        }
    }
OnError:
    /* Return result. */
    gcmFOOTER();
    return status;

}

/*******************************************************************************
**
**  gcoHARDWARE_ResolveRect
**
**  Resolve a rectangluar area of a surface to another surface.
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
**          The origin of the source area to be resolved.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be resolved.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be resolved.
**
**      gctBOOL yInverted
**          Flag to indicate yInvertedly resolve source to dest.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ResolveRect(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gctBOOL yInverted
    )
{
    gceSTATUS status;
    gctBOOL locked = gcvFALSE;
#if gcdENABLE_3D
    gctBOOL switchedTileStatus = gcvFALSE;
    gctBOOL pausedTileStatus = gcvFALSE;
    gcsSURF_INFO_PTR saved = gcvNULL;
#endif
    gctBOOL srcLocked = gcvFALSE;
    gctBOOL destLocked = gcvFALSE;
    gctBOOL resampling;
    gctBOOL dithering;
    gctBOOL srcTiled;
    gctBOOL destTiled;
    gcsPOINT alignedSrcOrigin, alignedSrcSize;
    gcsPOINT alignedDestOrigin, alignedDestSize;
    gcsPOINT alignedSrcOriginExpand = {0, 0};
    gcsPOINT alignedDestOriginExpand = {0, 0};
    gctBOOL originsMatch;
    gcsPOINT alignedRectSize;
    gcsSURF_FORMAT_INFO_PTR srcFormatInfo;
    gcsSURF_FORMAT_INFO_PTR dstFormatInfo;
    gcsSURF_FORMAT_INFO_PTR tmpFormatInfo;
    gcsPOINT tempOrigin;
    gctBOOL flip = gcvFALSE;
    gctBOOL expandRectangle = gcvTRUE;
    gctUINT32 srcAlignmentX = 0;
    gctUINT32 destAlignmentX = 0;
    gctUINT32 srcFormat, dstFormat;
    gctBOOL srcFlip;
    gctBOOL dstFlip;
    gctBOOL swcopy = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x DestInfo=0x%x "
                    "SrcOrigin=0x%x DestOrigin=0x%x RectSize=0x%x yInverted=%d",
                    Hardware, SrcInfo, DestInfo,
                    SrcOrigin, DestOrigin, RectSize, yInverted);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

#if gcdENABLE_3D

    /* Resolve destination surface and disable its tile status buffer . */
    if (!Hardware->inFlush && !DestInfo->tileStatusDisabled)
    {

        gcmONERROR(gcoHARDWARE_DisableTileStatus(Hardware, DestInfo, gcvTRUE));
    }


    /*
    ** Halti2 RS can look depth FC register, before we add to support it,
    ** RS only look color FC registers. For depth surface, we will fake it
    ** to color surface and program it on color FC registers, which should
    ** be done before calling to this function. So just assert here.
    */
    if (!(SrcInfo->tileStatusDisabled) && (SrcInfo->type == gcvSURF_DEPTH))
    {
        gcmASSERT(0);
    }

    /* RS engine only look tile status buffer for source, so we can enable it for source
    ** instead of resolving it before blit.
    ** If the src surface is not current one, we can try to enable its tile status buffer
    ** for RS blit.
    */
    if ((SrcInfo != Hardware->colorStates.target[0].surface) &&
        (SrcInfo->type == gcvSURF_RENDER_TARGET))
    {
        /* Save the current surface. */
        saved = Hardware->colorStates.target[0].surface;

        /* Start working in the requested surface. */
        Hardware->colorStates.target[0].surface = SrcInfo;

        /* Enable the tile status for the requested surface. */
        status = gcoHARDWARE_EnableTileStatus(
            Hardware,
            SrcInfo,
            SrcInfo->tileStatusNode.physical,
            &SrcInfo->hzTileStatusNode,
            0);

        /* Start working in the requested surface. */
        Hardware->colorStates.target[0].surface = saved;

        switchedTileStatus = gcvTRUE;
    }
    else if (SrcInfo != Hardware->colorStates.target[0].surface)
    {
        gcoHARDWARE_PauseTileStatus(Hardware, gcvTILE_STATUS_PAUSE);
        pausedTileStatus = gcvTRUE;
    }

    /*
    ** Adjust cache mode according to source surface
    */
    gcmONERROR(gcoHARDWARE_AdjustCacheMode(Hardware, SrcInfo));

#endif

    /***********************************************************************
    ** Determine special functions.
    */

    srcTiled  = (SrcInfo->type  != gcvSURF_BITMAP);
    destTiled = (DestInfo->type != gcvSURF_BITMAP);

    resampling =  (SrcInfo->samples.x  != 1)
               || (SrcInfo->samples.y  != 1)
               || (DestInfo->samples.x != 1)
               || (DestInfo->samples.y != 1);

    dithering = SrcInfo->deferDither3D && (SrcInfo->bitsPerPixel > DestInfo->bitsPerPixel);

#if gcdENABLE_3D
    flip = (SrcInfo->orientation != DestInfo->orientation) || yInverted;
#endif

    /* check if the format is supported by hw. */
    if (gcmIS_ERROR(_ConvertResolveFormat(Hardware,
                              SrcInfo->format,
                              &srcFormat,
                              &srcFlip,
                              gcvNULL)) ||
        gcmIS_ERROR(_ConvertResolveFormat(Hardware,
                              DestInfo->format,
                              &dstFormat,
                              &dstFlip,
                              gcvNULL))
     )
    {
        swcopy = gcvTRUE;
    }


    /* RS can't support tile<->tile flip y */
    if (flip &&
        (((SrcInfo->tiling != gcvLINEAR) && (DestInfo->tiling != gcvLINEAR)) ||
         (!Hardware->features[gcvFEATURE_FLIP_Y])
        )
       )
    {
        swcopy = gcvTRUE;
    }
    /***********************************************************************
    ** Since 2D bitmaps live in linear space, we don't need to resolve them.
    ** We can just copy them using the 2D engine.  However, we need to flush
    ** and stall after we are done with the copy since we have to make sure
    ** the bits are there before we blit them to the screen.
    */

    if (!srcTiled && !destTiled && !resampling && !dithering && !flip)
    {
        gcmONERROR(_SourceCopy(
            Hardware,
            SrcInfo, DestInfo,
            SrcOrigin, DestOrigin,
            RectSize
            ));
    }


    /***********************************************************************
    ** YUV 4:2:0 tiling case.
    */

    else if ((SrcInfo->format == gcvSURF_YV12) ||
             (SrcInfo->format == gcvSURF_I420) ||
             (SrcInfo->format == gcvSURF_NV12) ||
             (SrcInfo->format == gcvSURF_NV21))
    {
        if (!srcTiled && destTiled && (DestInfo->format == gcvSURF_YUY2))
        {
            gcmONERROR(_Tile420Surface(
                Hardware,
                SrcInfo, DestInfo,
                SrcOrigin, DestOrigin,
                RectSize
                ));
        }
        else
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }


    /***********************************************************************
    ** Unsupported cases.
    */

    else if (
            /* Upsampling. */
               (SrcInfo->samples.x < DestInfo->samples.x)
            || (SrcInfo->samples.y < DestInfo->samples.y)

            /* the format that hw do not support*/
            || swcopy
    )

    {

        /*Resolve source surface before we call into SW copy*/
        gcmONERROR(gcoHARDWARE_FlushTileStatus(Hardware, SrcInfo,gcvTRUE));

        gcmONERROR(gcoHARDWARE_CopyPixels(
            Hardware,
            SrcInfo, DestInfo,
            SrcOrigin->x, SrcOrigin->y,
            DestOrigin->x, DestOrigin->y,
            RectSize->x, (flip? -RectSize->y : RectSize->y)
            ));
    }


    /***********************************************************************
    ** Calculate the aligned source and destination rectangles; aligned to
    ** completely cover the specified source and destination areas.
    */

    else
    {
        gctBOOL srcMultiPipe = (SrcInfo->tiling & gcvTILING_SPLIT_BUFFER);

        gctBOOL dstMultiPipe = (DestInfo->tiling & gcvTILING_SPLIT_BUFFER);

        _AlignResolveRect(
            Hardware, SrcInfo, SrcOrigin, RectSize, &alignedSrcOrigin, &alignedSrcSize
            );

        _AlignResolveRect(
            Hardware, DestInfo, DestOrigin, RectSize, &alignedDestOrigin, &alignedDestSize
            );

        /* Use the maximum rectangle. */
        alignedRectSize.x = gcmMAX(alignedSrcSize.x, alignedDestSize.x);
        alignedRectSize.y = gcmMAX(alignedSrcSize.y, alignedDestSize.y);

        /***********************************************************************
        ** If specified and aligned rectangles are the same, then the requested
        ** rectangle is perfectly aligned and we can do it in one shot.
        */

        originsMatch
            =  (alignedSrcOrigin.x  == SrcOrigin->x)
            && (alignedSrcOrigin.y  == SrcOrigin->y)
            && (alignedDestOrigin.x == DestOrigin->x)
            && (alignedDestOrigin.y == DestOrigin->y);

        if (!originsMatch)
        {
            /***********************************************************************
            ** The origins are changed.
            */

            gctINT srcDeltaX, srcDeltaY;
            gctINT destDeltaX, destDeltaY;
            gctINT maxDeltaX, maxDeltaY;

            srcDeltaX  = SrcOrigin->x - alignedSrcOrigin.x;
            srcDeltaY  = SrcOrigin->y - alignedSrcOrigin.y;

            destDeltaX = DestOrigin->x - alignedDestOrigin.x;
            destDeltaY = DestOrigin->y - alignedDestOrigin.y;

            maxDeltaX = gcmMAX(srcDeltaX, destDeltaX);
            maxDeltaY = gcmMAX(srcDeltaY, destDeltaY);

            if (srcDeltaX == maxDeltaX)
            {
                /* The X coordinate of dest rectangle is changed. */
                alignedDestOriginExpand.x = DestOrigin->x - maxDeltaX;
                alignedSrcOriginExpand.x  = alignedSrcOrigin.x;
            }
            else
            {
                /* The X coordinate of src rectangle is changed. */
                alignedSrcOriginExpand.x  = SrcOrigin->x - maxDeltaX;
                alignedDestOriginExpand.x = alignedDestOrigin.x;
            }

            if (srcDeltaY == maxDeltaY)
            {
                /* Expand the Y coordinate of dest rectangle. */
                alignedDestOriginExpand.y = DestOrigin->y - maxDeltaY;
                alignedSrcOriginExpand.y  = alignedSrcOrigin.y;
            }
            else
            {
                /* Expand the Y coordinate of src rectangle. */
                alignedSrcOriginExpand.y  = SrcOrigin->y - maxDeltaY;
                alignedDestOriginExpand.y = alignedDestOrigin.y;
            }

            if ((alignedSrcOriginExpand.x < 0)  ||
                (alignedSrcOriginExpand.y < 0)  ||
                (alignedDestOriginExpand.x < 0) ||
                (alignedDestOriginExpand.y < 0))
            {
                expandRectangle = gcvFALSE;
            }
            else
            {
                gcoHARDWARE_GetSurfaceResolveAlignment(Hardware,
                                                       SrcInfo,
                                                       &srcAlignmentX,
                                                       gcvNULL,
                                                       gcvNULL,
                                                       gcvNULL);

                gcoHARDWARE_GetSurfaceResolveAlignment(Hardware,
                                                       DestInfo,
                                                       &destAlignmentX,
                                                       gcvNULL,
                                                       gcvNULL,
                                                       gcvNULL);
            }
        }

        /* Fully aligned Origin and RectSize. */
        if (originsMatch &&
            (alignedRectSize.x == RectSize->x) &&
            (alignedRectSize.y == RectSize->y))
        {

            gcmONERROR(_ResolveRect(
                Hardware,
                SrcInfo, DestInfo,
                SrcOrigin, DestOrigin,
                RectSize, flip
                ));

        }
        else if (resampling)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        /* Handle supertiling */
        else if ((originsMatch) &&
                 (destTiled) &&
                 ((RectSize->x > 64) && !(RectSize->x & 3)) &&
                 (!srcMultiPipe && !dstMultiPipe && !(RectSize->y & 3)) &&
                 (SrcInfo->superTiled && !DestInfo->superTiled) &&
                 (!(SrcOrigin->x  & 63) && !(SrcOrigin->y & 63))
                )
        {
            gcsPOINT srcOriginInterior, srcOriginExtra;
            gcsPOINT destOriginInterior, destOriginExtra;
            gcsPOINT rectSizeInterior, rectSizeExtra;
            gctUINT32 tileSize = 64;

            /* First resolve: Resolve tile aligned interior region. */
            srcOriginInterior.x  = SrcOrigin->x;
            srcOriginInterior.y  = SrcOrigin->y;
            destOriginInterior.x = DestOrigin->x;
            destOriginInterior.y = DestOrigin->y;
            rectSizeInterior.x   = (RectSize->x/tileSize)*tileSize;
            rectSizeInterior.y   = RectSize->y;

            gcmONERROR(_ResolveRect(
                Hardware,
                SrcInfo, DestInfo,
                &srcOriginInterior, &destOriginInterior,
                &rectSizeInterior, flip
                ));

            /* Second resolve: Resolve the right end. */
            srcOriginExtra.x  = SrcOrigin->x + rectSizeInterior.x;
            srcOriginExtra.y  = SrcOrigin->y;
            destOriginExtra.x = DestOrigin->x + rectSizeInterior.x;
            destOriginExtra.y = DestOrigin->y;
            rectSizeExtra.x   = RectSize->x - rectSizeInterior.x;
            /* Resolve entire column. */
            rectSizeExtra.y   = RectSize->y;

            gcmONERROR(gcoHARDWARE_CopyPixels(
                Hardware,
                SrcInfo, DestInfo,
                srcOriginExtra.x, srcOriginExtra.y,
                destOriginExtra.x, destOriginExtra.y,
                rectSizeExtra.x, (flip? -rectSizeExtra.y : rectSizeExtra.y)
                ));
        }
        /* Aligned Origin and tile aligned RectSize. */
        /* Assuming tile size of 4x4 pixels, and resolve size requirement of 4 tiles. */
        else if (originsMatch &&
                 (RectSize->x > 16) &&
                ((RectSize->x & 3) == 0) &&
                (!srcMultiPipe && !dstMultiPipe && ((RectSize->y & 3) == 0)))
        {
            gcsPOINT srcOriginInterior, srcOriginExtra;
            gcsPOINT destOriginInterior, destOriginExtra;
            gcsPOINT rectSizeInterior, rectSizeExtra;
            gctUINT32 extraTiles;
            gctUINT32 tileSize = 4, numTiles = 4;
            gctUINT32 loopCount = 0;

            extraTiles = numTiles - (alignedRectSize.x - RectSize->x) / tileSize;

            /* First resolve: Resolve 4 tile aligned interior region. */
            srcOriginInterior.x  = SrcOrigin->x;
            srcOriginInterior.y  = SrcOrigin->y;
            destOriginInterior.x = DestOrigin->x;
            destOriginInterior.y = DestOrigin->y;
            rectSizeInterior.x   = RectSize->x - extraTiles * tileSize;
            rectSizeInterior.y   = RectSize->y;

            gcmONERROR(_ResolveRect(
                Hardware,
                SrcInfo, DestInfo,
                &srcOriginInterior, &destOriginInterior,
                &rectSizeInterior, flip
                ));

            /* Second resolve: Resolve last 4 tiles at the right end. */
            srcOriginExtra.x  = SrcOrigin->x + RectSize->x - (4 * tileSize);
            srcOriginExtra.y  = SrcOrigin->y;
            destOriginExtra.x = DestOrigin->x + RectSize->x - (4 * tileSize);
            destOriginExtra.y = DestOrigin->y;
            rectSizeExtra.x   = 4 * tileSize;
            rectSizeExtra.y   = 4 * Hardware->config->pixelPipes;

            do
            {
                gcmASSERT(srcOriginExtra.y < (SrcOrigin->y + RectSize->y));
                gcmONERROR(_ResolveRect(
                    Hardware,
                    SrcInfo, DestInfo,
                    &srcOriginExtra, &destOriginExtra,
                    &rectSizeExtra, flip
                    ));

                srcOriginExtra.y  += 4 * Hardware->config->pixelPipes;
                destOriginExtra.y += 4 * Hardware->config->pixelPipes;
                loopCount++;
            }
            while (srcOriginExtra.y != (SrcOrigin->y + RectSize->y) && (loopCount < MAX_LOOP_COUNT));
        }

        /***********************************************************************
        ** Not matched origins.
        ** We expand the rectangle to guarantee the alignment requirements.
        */

        else if (!originsMatch && expandRectangle &&
                 !(alignedSrcOriginExpand.x & (srcAlignmentX -1)) &&
                 !(alignedSrcOriginExpand.y & 3) &&
                 !(alignedDestOriginExpand.x & (destAlignmentX - 1)) &&
                 !(alignedDestOriginExpand.y & 3))
        {
            gcmONERROR(_ResolveRect(
                Hardware,
                SrcInfo, DestInfo,
                &alignedSrcOriginExpand, &alignedDestOriginExpand,
                &alignedRectSize, flip
                ));
        }

        /***********************************************************************
        ** Handle all cases when source and destination are both tiled.
        */

        else if ((SrcInfo->type  != gcvSURF_BITMAP) &&
                 (DestInfo->type != gcvSURF_BITMAP))
        {
            static gctBOOL printed = gcvFALSE;

            /* Flush the pipe. */
            gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

#if gcdENABLE_3D
            /* Disable the tile status. */
            gcmONERROR(gcoHARDWARE_DisableTileStatus(
                Hardware, SrcInfo, gcvTRUE
                ));
#endif

            /* Lock the source surface. */
            gcmONERROR(gcoHARDWARE_Lock(
                &SrcInfo->node, gcvNULL, gcvNULL
                ));

            srcLocked = gcvTRUE;

            /* Lock the destination surface. */
            gcmONERROR(gcoHARDWARE_Lock(
                &DestInfo->node, gcvNULL, gcvNULL
                ));

            destLocked = gcvTRUE;

            if (!printed)
            {
                printed = gcvTRUE;

                gcmPRINT("libGAL: Performing a software resolve!");
            }

            /* Perform software copy. */
            if (Hardware->config->pixelPipes > 1)
            {
                gcmONERROR(gcoHARDWARE_CopyPixels(
                    Hardware,
                    SrcInfo, DestInfo,
                    SrcOrigin->x, SrcOrigin->y,
                    DestOrigin->x, DestOrigin->y,
                    RectSize->x, (flip ? -RectSize->y : RectSize->y)
                    ));
            }
            else
            {
                gcmONERROR(_SoftwareCopy(
                    Hardware,
                    SrcInfo, DestInfo,
                    SrcOrigin, DestOrigin,
                    RectSize
                    ));
            }
        }

        /***********************************************************************
        ** At least one side of the rectangle is not aligned. In this case we
        ** will allocate a temporary buffer to resolve the aligned rectangle
        ** to and then use a source copy to complete the operation.
        */

        else if(!flip)
        {
            gctPOINTER memory = gcvNULL;
            gceORIENTATION orientation;

            /* Query format specifics. */
            srcFormatInfo = &SrcInfo->formatInfo;
            dstFormatInfo = &DestInfo->formatInfo;

            /* Pick the most compact format for the temporary surface. */
            tmpFormatInfo
                = (srcFormatInfo->bitsPerPixel < dstFormatInfo->bitsPerPixel)
                    ? srcFormatInfo
                    : dstFormatInfo;

            /* Allocate the temporary surface. */
            gcmONERROR(gcoHARDWARE_AllocateTemporarySurface(
                Hardware,
                alignedRectSize.x,
                alignedRectSize.y,
                tmpFormatInfo,
                DestInfo->type,
                DestInfo->flags
                ));

            /* Lock the temporary surface. */
            gcmONERROR(gcoHARDWARE_Lock(
                &Hardware->tempBuffer.node,
                gcvNULL,
                &memory
                ));

            /* Mark as locked. */
            locked = gcvTRUE;

            /* Set the temporary buffer origin. */
            tempOrigin.x = 0;
            tempOrigin.y = 0;

            /* Use same orientation as source. */
            orientation = Hardware->tempBuffer.orientation;
            Hardware->tempBuffer.orientation = SrcInfo->orientation;

            /* Resolve the aligned rectangle into the temporary surface. */
            gcmONERROR(_ResolveRect(
                Hardware,
                SrcInfo,
                &Hardware->tempBuffer,
                &alignedSrcOrigin,
                &tempOrigin,
                &alignedRectSize,
                gcvFALSE
                ));

            /* Restore orientation. */
            Hardware->tempBuffer.orientation = orientation;

            /* Compute the temporary buffer origin. */
            tempOrigin.x = SrcOrigin->x - alignedSrcOrigin.x;
            tempOrigin.y = SrcOrigin->y - alignedSrcOrigin.y;

            /* Invalidate temporary surface cache, as it could be being re-used. */
            gcmONERROR(
                gcoSURF_NODE_Cache(&Hardware->tempBuffer.node,
                                 memory,
                                 Hardware->tempBuffer.size,
                                 gcvCACHE_INVALIDATE));

            /* Copy the unaligned area to the final destination. */
            gcmONERROR(_SourceCopy(
                Hardware,
                &Hardware->tempBuffer,
                DestInfo,
                &tempOrigin,
                DestOrigin,
                RectSize
                ));
        }
        else
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

    }

    if (DestInfo->paddingFormat)
    {
        gctBOOL srcHasExChannel = gcvFALSE;

        if (SrcInfo->formatInfo.fmtClass == gcvFORMAT_CLASS_RGBA)
        {
            srcHasExChannel = (SrcInfo->formatInfo.u.rgba.red.width != 0 && DestInfo->formatInfo.u.rgba.red.width == 0) ||
                              (SrcInfo->formatInfo.u.rgba.green.width != 0 && DestInfo->formatInfo.u.rgba.green.width == 0) ||
                              (SrcInfo->formatInfo.u.rgba.blue.width != 0 && DestInfo->formatInfo.u.rgba.blue.width == 0) ||
                              (SrcInfo->formatInfo.u.rgba.alpha.width != 0 && DestInfo->formatInfo.u.rgba.alpha.width == 0);

            /* The source paddings are default value, is not from gcvSURF_G8R8_1_X8R8G8B8 to gcvSURF_R8_1_X8R8G8B8 and dst was full overwritten, the dst will be default value. */
            if (SrcInfo->paddingFormat && !SrcInfo->garbagePadded &&
                !(SrcInfo->format == gcvSURF_G8R8_1_X8R8G8B8 && DestInfo->format == gcvSURF_R8_1_X8R8G8B8) &&
                DestOrigin->x == 0 && RectSize->x >= DestInfo->rect.right / DestInfo->samples.x &&
                DestOrigin->y == 0 && RectSize->y >= DestInfo->rect.bottom / DestInfo->samples.y)
            {
                DestInfo->garbagePadded = gcvFALSE;
            }

            /* The source has extra channel:
            ** 1, Source is not paddingformat,the dst may be garbage value.
            ** 2, source is paddingformat but not default value, the dst will be garbage value.
            */
            if (srcHasExChannel && (!SrcInfo->paddingFormat || SrcInfo->garbagePadded))
            {
                DestInfo->garbagePadded = gcvTRUE;
            }
        }
        else
        {
            DestInfo->garbagePadded = gcvTRUE;
        }
    }

OnError:
    if (Hardware != gcvNULL)
    {
#if gcdENABLE_3D
        /* Resume tile status if it was paused. */
        if (switchedTileStatus)
        {
            if (saved)
            {
                /* Reprogram the tile status to the current surface. */
                gcmVERIFY_OK(gcoHARDWARE_EnableTileStatus(Hardware, saved,
                                                          (saved->tileStatusNode.pool != gcvPOOL_UNKNOWN) ?  saved->tileStatusNode.physical : 0,
                                                          &saved->hzTileStatusNode,
                                                          0));
            }
            else
            {
                gcmVERIFY_OK(
                    gcoHARDWARE_DisableHardwareTileStatus(Hardware,
                                                          (SrcInfo->type == gcvSURF_DEPTH) ?
                                                                        gcvTILESTATUS_DEPTH : gcvTILESTATUS_COLOR,
                                                          0));
            }


        }
        else if (pausedTileStatus)
        {
            gcoHARDWARE_PauseTileStatus(Hardware,gcvTILE_STATUS_RESUME);
        }
#endif

        /* Unlock. */
        if (locked)
        {
            /* Unlock the temporary surface. */
            gcmVERIFY_OK(gcoHARDWARE_Unlock(
                &Hardware->tempBuffer.node, Hardware->tempBuffer.type
                ));
        }

        if (srcLocked)
        {
            /* Unlock the source. */
            gcmVERIFY_OK(gcoHARDWARE_Unlock(
                &SrcInfo->node, SrcInfo->type
                ));
        }

        if (destLocked)
        {
            /* Unlock the source. */
            gcmVERIFY_OK(gcoHARDWARE_Unlock(
                &DestInfo->node, DestInfo->type
                ));
        }
    }

    /* Return result. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_IsHWResolveable(
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 srcFormat, dstFormat;
    gctBOOL srcFlip;
    gctBOOL dstFlip;
    gcsPOINT alignedSrcOrigin, alignedSrcSize;
    gcsPOINT alignedDestOrigin, alignedDestSize;
    gctBOOL originsMatch;
    gcoHARDWARE hardware = gcvNULL;

    gcmHEADER_ARG("SrcInfo=0x%x DestInfo=0x%x "
                    "SrcOrigin=0x%x DestOrigin=0x%x RectSize=0x%x",
                    SrcInfo, DestInfo,
                    SrcOrigin, DestOrigin, RectSize);

    gcmGETHARDWARE(hardware);


    /* check if the format is supported by hw. */
    if (gcmIS_ERROR(_ConvertResolveFormat(hardware,
                              SrcInfo->format,
                              &srcFormat,
                              &srcFlip,
                              gcvNULL)) ||
        gcmIS_ERROR(_ConvertResolveFormat(hardware,
                              DestInfo->format,
                              &dstFormat,
                              &dstFlip,
                              gcvNULL))
     )
    {
        gcmFOOTER();
        return gcvSTATUS_FALSE;
    }

    status = gcvSTATUS_TRUE;

    if ((SrcInfo->format == gcvSURF_YV12) ||
             (SrcInfo->format == gcvSURF_I420) ||
             (SrcInfo->format == gcvSURF_NV12) ||
             (SrcInfo->format == gcvSURF_NV21))
    {
        status = gcvSTATUS_FALSE;
    }
    else if (
            /* Upsampling. */
               (SrcInfo->samples.x < DestInfo->samples.x)
            || (SrcInfo->samples.y < DestInfo->samples.y)
    )
    {
        status = gcvSTATUS_FALSE;
    }
    else
    {
        _AlignResolveRect(
            hardware, SrcInfo, SrcOrigin, RectSize, &alignedSrcOrigin, &alignedSrcSize
            );

        _AlignResolveRect(
            hardware, DestInfo, DestOrigin, RectSize, &alignedDestOrigin, &alignedDestSize
            );

        /* Use the maximum rectangle. */

        originsMatch
            =  (alignedSrcOrigin.x  == SrcOrigin->x)
            && (alignedSrcOrigin.y  == SrcOrigin->y)
            && (alignedDestOrigin.x == DestOrigin->x)
            && (alignedDestOrigin.y == DestOrigin->y);

        if (!originsMatch)
        {
            status = gcvSTATUS_FALSE;
        }
    }

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_PreserveRects(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsRECT Rects[],
    IN gctUINT RectCount
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT i;
    gcsSURF_INFO_PTR savedCurrent = gcvNULL;
    gctBOOL savedDestState = gcvFALSE;
    gctBOOL restoreDestState   = gcvFALSE;
    gctBOOL switchedTileStatus = gcvFALSE;
    gctBOOL pausedTileStatus   = gcvFALSE;

    gcmHEADER_ARG("SrcInfo=0x%x DestInfo=0x%x Rects=0x%x RectCount=%d",
                  SrcInfo, DestInfo, Rects, RectCount);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_ARGUMENT(SrcInfo != gcvNULL);
    gcmVERIFY_ARGUMENT(DestInfo != gcvNULL);

    if (SrcInfo->type == gcvSURF_DEPTH)
    {
        /* Can not handle depth for now. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /***********************************************************************
    ** Stage 1: Diable dest tile status
    */

    /*
     * We are to copy into dest surface. We don't need to care dest tile status
     * and set tile status to disabled state, that is because
     * * If fully copied into dest surface, the raw surface is completely clean.
     * * If partially copied, we do not need to care the not copied pixels (ie,
     *   in masked rectangle) because these pixels will be overwriten by clear.
     */
    if ((DestInfo == Hardware->colorStates.target[0].surface) &&
        (DestInfo->type == gcvSURF_RENDER_TARGET) &&
        (DestInfo->tileStatusDisabled == gcvFALSE))
    {
        /*
         * Dest is current color buffer and tile status is now enabled.
         * Need flush and turn off dest tile status.
         */
        gcmONERROR(gcoHARDWARE_DisableTileStatus(Hardware, DestInfo, gcvFALSE));
    }

    /* Save dest tileStatusDisable state. */
    savedDestState = DestInfo->tileStatusDisabled;

    /* Set dest tile status as disabled. */
    DestInfo->tileStatusDisabled = gcvTRUE;

    /* Tag dest tile status state as changed. */
    restoreDestState = gcvTRUE;

    /***********************************************************************
    ** Stage 2: Enable source tile status
    */
    if ((SrcInfo != Hardware->colorStates.target[0].surface) &&
        (SrcInfo->type == gcvSURF_RENDER_TARGET))
    {
        /* Save the current surface. */
        savedCurrent = Hardware->colorStates.target[0].surface;

        /* Start working in the requested surface. */
        Hardware->colorStates.target[0].surface = SrcInfo;

        /* Enable the tile status for the requested surface. */
        status = gcoHARDWARE_EnableTileStatus(
            Hardware,
            SrcInfo,
            (SrcInfo->tileStatusNode.pool != gcvPOOL_UNKNOWN)
            ? SrcInfo->tileStatusNode.physical : 0,
            &SrcInfo->hzTileStatusNode,
            0);

        /* Start working in the requested surface. */
        Hardware->colorStates.target[0].surface = savedCurrent;

        /* Tag tile status switched as true. */
        switchedTileStatus = gcvTRUE;
    }
    else if (SrcInfo != Hardware->colorStates.target[0].surface)
    {
        /* Src is not current, and source is not render target. */
        gcoHARDWARE_PauseTileStatus(Hardware, gcvTILE_STATUS_PAUSE);
        pausedTileStatus = gcvTRUE;
    }

    /***********************************************************************
    ** Stage 3: Copy rectangles
    */
    for (i = 0; i < RectCount; i++)
    {
        gcsPOINT origin;
        gcsPOINT size;

        origin.x = Rects[i].left;
        origin.y = Rects[i].top;
        size.x   = Rects[i].right  - origin.x;
        size.y   = Rects[i].bottom - origin.y;

        /* Go through all rectangles. */
        gcmONERROR(_ResolveRect(
            Hardware,
            SrcInfo, DestInfo,
            &origin, &origin,
            &size, gcvFALSE
            ));
    }

    /* Success: do NOT restore dest tile status state. */
    restoreDestState = gcvFALSE;

OnError:
    if (switchedTileStatus)
    {
        if (savedCurrent)
        {
            /* Reprogram the tile status to the current surface. */
            gcmVERIFY_OK(
                gcoHARDWARE_EnableTileStatus(
                    Hardware,
                    savedCurrent,
                    (savedCurrent->tileStatusNode.pool != gcvPOOL_UNKNOWN)
                    ?  savedCurrent->tileStatusNode.physical : 0,
                    &savedCurrent->hzTileStatusNode,
                    0));
        }
        else
        {
            /* No current tile status, turn on hardware tile status. */
            gcmVERIFY_OK(
                gcoHARDWARE_DisableHardwareTileStatus(
                    Hardware,
                    (SrcInfo->type == gcvSURF_DEPTH)
                    ? gcvTILESTATUS_DEPTH : gcvTILESTATUS_COLOR,
                    0));
        }
    }
    else if (pausedTileStatus)
    {
        /* Resume tile status. */
        gcmVERIFY_OK(gcoHARDWARE_PauseTileStatus(Hardware, gcvTILE_STATUS_RESUME));
    }

    if (restoreDestState)
    {
        /* Restore dest tile status state. */
        DestInfo->tileStatusDisabled = savedDestState;
    }

    gcmFOOTER();
    return status;
}

#endif

/*******************************************************************************
**
**  gcoHARDWARE_Lock
**
**  Lock video memory.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR Node
**          Pointer to a gcsSURF_NODE structure that describes the video
**          memory to lock.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Physical address of the surface.
**
**      gctPOINTER * Memory
**          Logical address of the surface.
*/
gceSTATUS
gcoHARDWARE_Lock(
    IN gcsSURF_NODE_PTR Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Node=0x%x Address=0x%x Memory=0x%x",
                  Node, Address, Memory);

    if (!Node->valid)
    {
        gcsHAL_INTERFACE iface;

        /* User pools have to be mapped first. */
        if (Node->pool == gcvPOOL_USER)
        {
            gcmONERROR(gcvSTATUS_MEMORY_UNLOCKED);
        }

        /* Fill in the kernel call structure. */
        iface.command = gcvHAL_LOCK_VIDEO_MEMORY;
        iface.u.LockVideoMemory.node = Node->u.normal.node;
        iface.u.LockVideoMemory.cacheable = Node->u.normal.cacheable;

        /* Call the kernel. */
        gcmONERROR(gcoOS_DeviceControl(
            gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, sizeof(iface),
            &iface, sizeof(iface)
            ));

        /* Success? */
        gcmONERROR(iface.status);

        /* Validate the node. */
        Node->valid = gcvTRUE;

        /* Store pointers. */
        Node->physical = iface.u.LockVideoMemory.address;
        Node->logical  = gcmUINT64_TO_PTR(iface.u.LockVideoMemory.memory);

        /* Set locked in the kernel flag. */
        Node->lockedInKernel = gcvTRUE;
        gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL, &Node->lockedHardwareType));
    }

    /* Add the memory info. */
    if (Node->lockCount == 0)
    {
        gcmDUMP_ADD_MEMORY_INFO(Node->physical, Node->logical, gcvINVALID_ADDRESS, Node->size);
    }

    /* Increment the lock count. */
    Node->lockCount++;

    /* Set the result. */
    if (Address != gcvNULL)
    {
        *Address = Node->physical;
    }

    if (Memory != gcvNULL)
    {
        *Memory = Node->logical;
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Unlock
**
**  Unlock video memory.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR Node
**          Pointer to a gcsSURF_NODE structure that describes the video
**          memory to unlock.
**
**      gceSURF_TYPE Type
**          Type of surface to unlock.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Unlock(
    IN gcsSURF_NODE_PTR Node,
    IN gceSURF_TYPE Type
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_INTERFACE iface;
    gctBOOL currentTypeChanged = gcvFALSE;
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;

    gcmHEADER_ARG("Node=0x%x Type=%d",
                   Node, Type);

    /* Verify whether the node is valid. */
    if (!Node->valid || (Node->lockCount <= 0))
    {
        gcmTRACE_ZONE(
            gcvLEVEL_WARNING, gcvZONE_SURFACE,
            "gcoHARDWARE_Unlock: Node=0x%x; unlock called on an unlocked surface.",
            Node
            );
    }

    /* Locked more then once? */
    else if (--Node->lockCount == 0)
    {
        /* Delete the memory info. */
        gcmDUMP_DEL_MEMORY_INFO(Node->physical);

        if (Node->lockedInKernel)
        {
            /* Save the current hardware type */
            gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL, &currentType));

            if (Node->lockedHardwareType != currentType)
            {
                /* Change to the locked hardware type */
                gcmVERIFY_OK(gcoHAL_SetHardwareType(gcvNULL, Node->lockedHardwareType));
                currentTypeChanged = gcvTRUE;
            }

            /* Unlock the video memory node. */
            iface.command = gcvHAL_UNLOCK_VIDEO_MEMORY;
            iface.u.UnlockVideoMemory.node = Node->u.normal.node;
            iface.u.UnlockVideoMemory.type = Type & ~gcvSURF_NO_VIDMEM;

            /* Call the kernel. */
            gcmONERROR(gcoOS_DeviceControl(
                gcvNULL,
                IOCTL_GCHAL_INTERFACE,
                &iface, gcmSIZEOF(iface),
                &iface, gcmSIZEOF(iface)
                ));

            /* Success? */
            gcmONERROR(iface.status);

            /* Schedule an event for the unlock. */
            gcmONERROR(gcoHARDWARE_CallEvent(gcvNULL, &iface));

            /* Reset locked in the kernel flag. */
            Node->lockedInKernel = gcvFALSE;

            if (currentTypeChanged)
            {
                /* Restore the current hardware type */
                gcmVERIFY_OK(gcoHAL_SetHardwareType(gcvNULL, currentType));
                currentTypeChanged = gcvFALSE;
            }
        }

#if !gcdPROCESS_ADDRESS_SPACE
        /* Reset the valid flag. */
        if ((Node->pool == gcvPOOL_CONTIGUOUS) ||
            (Node->pool == gcvPOOL_VIRTUAL))
#endif
        {
            Node->valid = gcvFALSE;
        }
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (currentTypeChanged)
    {
        /* Restore the current hardware type */
        gcmVERIFY_OK(gcoHAL_SetHardwareType(gcvNULL, currentType));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_CallEvent
**
**  Send an event to the kernel and append the required synchronization code to
**  the command buffer..
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsHAL_INTERFACE * Interface
**          Pointer to an gcsHAL_INTERFACE structure the defines the event to
**          send.
**
**  OUTPUT:
**
**      gcsHAL_INTERFACE * Interface
**          Pointer to an gcsHAL_INTERFACE structure the received information
**          from the kernel.
*/
gceSTATUS
gcoHARDWARE_CallEvent(
    IN gcoHARDWARE Hardware,
    IN OUT gcsHAL_INTERFACE * Interface
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

#if (gcdENABLE_3D || gcdENABLE_2D)
    gcoQUEUE queue = gcvNULL;

    gcmHEADER_ARG("Hardware=0x%x Interface=0x%x", Hardware, Interface);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Interface != gcvNULL);

    /* Get the queue */
    queue = Hardware->queue;

#if gcdSTREAM_OUT_BUFFER
    /* Append the event to stream out queue instead of the original one */
    if (Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_1)
    {
        queue = Hardware->streamOutEventQueue;

        gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                      gcvZONE_STREAM_OUT,
                      "Stream out [?:?] - Postpone event: 0x%x (%s)",
                      Interface,
                      __FUNCTION__);
    }
#endif

    /* Append event to queue */
    gcmONERROR(gcoQUEUE_AppendEvent(queue, Interface));

#if gcdIN_QUEUE_RECORD_LIMIT
    if (Hardware->queue->recordCount >= gcdIN_QUEUE_RECORD_LIMIT)
    {
        gcmONERROR(gcoHARDWARE_Commit(Hardware));
    }
#endif

OnError:
    /* Return status. */
    gcmFOOTER();
#endif  /* (gcdENABLE_3D || gcdENABLE_2D) */
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ScheduleVideoMemory
**
**  Schedule destruction for the specified video memory node.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR Node
**          Pointer to the video momory node to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ScheduleVideoMemory(
    IN gcsSURF_NODE_PTR Node
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Node=0x%x", Node);

    if (Node->pool != gcvPOOL_USER)
    {
        gcsHAL_INTERFACE iface;

        gcmTRACE_ZONE(gcvLEVEL_VERBOSE, gcvZONE_HARDWARE,
                      "node=0x%08x",
                      Node->u.normal.node);

        /* Release the allocated video memory asynchronously. */
        iface.command = gcvHAL_RELEASE_VIDEO_MEMORY;
        iface.u.ReleaseVideoMemory.node = Node->u.normal.node;

        /* Call kernel HAL. */
        gcmONERROR(gcoHAL_Call(gcvNULL, &iface));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_AllocateTemporarySurface
**
**  Allocates a temporary surface with specified parameters.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctUINT Width, Height
**          The aligned size of the surface to be allocated.
**
**      gcsSURF_FORMAT_INFO_PTR Format
**          The format of the surface to be allocated.
**
**      gceSURF_TYPE Type
**          The type of the surface to be allocated.
**
**      gceSURF_FLAG Flags
**          The flags of dest surface info.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_AllocateTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    IN gceSURF_TYPE Type,
    IN gceSURF_FLAG Flags
    )
{
    gceSTATUS status;
    gctBOOL superTiled = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x Width=%d Height=%d "
                    "FormatInfo=0x%x Type=%d",
                    Hardware, Width, Height,
                    *FormatInfo, Type);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Do we have a compatible surface? */
    if ((Hardware->tempBuffer.type == Type) &&
        (Hardware->tempBuffer.format == FormatInfo->format) &&
        (Hardware->tempBuffer.rect.right == (gctINT) Width) &&
        (Hardware->tempBuffer.rect.bottom == (gctINT) Height))
    {
        status = gcvSTATUS_OK;
    }
    else
    {
        gctUINT32 size;

        /* Delete existing buffer. */
        gcmONERROR(gcoHARDWARE_FreeTemporarySurface(Hardware, gcvTRUE));

        Hardware->tempBuffer.alignedWidth  = Width;
        Hardware->tempBuffer.alignedHeight = Height;

        /* Align the width and height. */
        gcmONERROR(gcoHARDWARE_AlignToTileCompatible(
            Hardware,
            Type,
            FormatInfo->format,
            &Hardware->tempBuffer.alignedWidth,
            &Hardware->tempBuffer.alignedHeight,
            1,
            &Hardware->tempBuffer.tiling,
            &superTiled
            ));

        size = Hardware->tempBuffer.alignedWidth
             * FormatInfo->bitsPerPixel / 8
             * Hardware->tempBuffer.alignedHeight;

        gcmONERROR(gcsSURF_NODE_Construct(
            &Hardware->tempBuffer.node,
            size,
            64,
            Type,
            Flags & gcvSURF_FLAG_CONTENT_PROTECTED
            ? gcvALLOC_FLAG_SECURITY
            : gcvALLOC_FLAG_NONE,
            gcvPOOL_DEFAULT
            ));

        /* Set the new parameters. */
        Hardware->tempBuffer.type                = Type;
        Hardware->tempBuffer.format              = FormatInfo->format;
        Hardware->tempBuffer.formatInfo          = *FormatInfo;
        Hardware->tempBuffer.stride              = Width * FormatInfo->bitsPerPixel / 8;
        Hardware->tempBuffer.size                = size;

#if gcdENABLE_3D
        Hardware->tempBuffer.samples.x = 1;
        Hardware->tempBuffer.samples.y = 1;
        Hardware->tempBuffer.superTiled = superTiled;
#endif

        Hardware->tempBuffer.rect.left   = 0;
        Hardware->tempBuffer.rect.top    = 0;
        Hardware->tempBuffer.rect.right  = Width;
        Hardware->tempBuffer.rect.bottom = Height;
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_FreeTemporarySurface
**
**  Free the temporary surface.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_FreeTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Synchronized
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Synchronized=%d", Hardware, Synchronized);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Is there a surface allocated? */
    if (Hardware->tempBuffer.node.pool != gcvPOOL_UNKNOWN)
    {
        gcmVERIFY_OK(gcoHARDWARE_ScheduleVideoMemory(&Hardware->tempBuffer.node));

        /* Reset the temporary surface. */
        gcoOS_ZeroMemory(&Hardware->tempBuffer, sizeof(Hardware->tempBuffer));

        Hardware->tempBuffer.node.pool = gcvPOOL_UNKNOWN;
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHARDWARE_Alloc2DSurface
**
**  Allocates a 2D surface with specified parameters.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctUINT Width, Height
**          The aligned size of the surface to be allocated.
**
**      gceSURF_FORMAT Format
**          The format of the surface to be allocated.
**
**      gceSURF_FLAG Flags
**          The flags of dest surface info.
**  OUTPUT:
**
**      gcsSURF_INFO_PTR *SurfInfo
*/
gceSTATUS
gcoHARDWARE_Alloc2DSurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gceSURF_FORMAT Format,
    IN gceSURF_FLAG Flags,
    OUT gcsSURF_INFO_PTR *SurfInfo
    )
{
    gceSTATUS status;
    gcsSURF_INFO_PTR surf = gcvNULL;
    gcsSURF_FORMAT_INFO_PTR formatInfo[2];
    gctUINT alignedWidth, alignedHeight, size;

    gcmHEADER_ARG("Width=%d Height=%d "
                    "Format=%d",
                    Width, Height,
                    Format);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(SurfInfo != gcvNULL);

    alignedWidth = Width;
    alignedHeight = Height;

    /* Align the width and height. */
    gcmONERROR(gcoHARDWARE_AlignToTile(
        Hardware, gcvSURF_BITMAP, Format, &alignedWidth, &alignedHeight, 1, gcvNULL, gcvNULL
        ));

    gcmONERROR(gcoSURF_QueryFormat(Format, formatInfo));

    size = alignedWidth * formatInfo[0]->bitsPerPixel  / 8 * alignedHeight;

    gcmONERROR(gcoOS_Allocate(gcvNULL, sizeof(gcsSURF_INFO), (gctPOINTER *)&surf));
    gcoOS_ZeroMemory(surf, sizeof(gcsSURF_INFO));

    gcmONERROR(gcsSURF_NODE_Construct(
        &surf->node,
        size,
        64,
        gcvSURF_BITMAP,
        Flags & gcvSURF_FLAG_CONTENT_PROTECTED
        ? gcvALLOC_FLAG_SECURITY|gcvALLOC_FLAG_CONTIGUOUS
        : gcvALLOC_FLAG_CONTIGUOUS,
        gcvPOOL_DEFAULT
        ));

    /* Set the new parameters. */
    surf->type                = gcvSURF_BITMAP;
    surf->format              = Format;
    surf->stride              = alignedWidth * formatInfo[0]->bitsPerPixel / 8;
    surf->alignedWidth        = alignedWidth;
    surf->alignedHeight       = alignedHeight;
    surf->size                = size;
    surf->is16Bit             = formatInfo[0]->bitsPerPixel == 16;

#if gcdENABLE_3D
    surf->samples.x           = 1;
    surf->samples.y           = 1;
#endif

    surf->rotation            = gcvSURF_0_DEGREE;
    surf->orientation         = gcvORIENTATION_TOP_BOTTOM;

    surf->rect.left   = 0;
    surf->rect.top    = 0;
    surf->rect.right  = Width;
    surf->rect.bottom = Height;

    surf->tiling              = gcvLINEAR;

    gcmONERROR(gcoHARDWARE_Lock(&surf->node, gcvNULL, gcvNULL));

    *SurfInfo = surf;

OnError:
    if (gcmIS_ERROR(status) && (surf != gcvNULL))
    {
        if (surf->node.valid)
        {
            gcmVERIFY_OK(gcoHARDWARE_Unlock(
                &surf->node,
                gcvSURF_BITMAP
                ));
        }

        if (surf->node.u.normal.node != 0)
        {
            gcmVERIFY_OK(gcoHARDWARE_ScheduleVideoMemory(&surf->node));
        }

        gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, surf));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Free2DSurface
**
**  Free the 2D surface.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SurfInfo
**          The info of the surface to be free'd.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Free2DSurface(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_INFO_PTR surf = SurfInfo;

    gcmHEADER_ARG("SurfInfo=0x%x", SurfInfo);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(SurfInfo != gcvNULL);

    if (surf->node.valid)
    {
        gcmONERROR(gcoHARDWARE_Unlock(
            &surf->node,
            gcvSURF_BITMAP
            ));
    }

    /* Schedule deletion. */
    gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&surf->node));
    gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, surf));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Get2DTempSurface
**
**  Allocates a temporary surface with specified parameters.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT Width, Height
**          The aligned size of the surface to be allocated.
**
**      gceSURF_FORMAT Format
**          The format of the surface to be allocated.
**
**      gceSURF_FLAG Flags
**          The flags of dest surface info.
**  OUTPUT:
**
**      gcsSURF_INFO_PTR *SurfInfo
*/
gceSTATUS
gcoHARDWARE_Get2DTempSurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gceSURF_FORMAT Format,
    IN gceSURF_FLAG Flags,
    OUT gcsSURF_INFO_PTR *SurfInfo
    )
{
    gceSTATUS status;
    gcsSURF_FORMAT_INFO_PTR formatInfo;
    gctUINT alignedWidth, alignedHeight, size;
    gctSIZE_T delta = 0;
    gctINT  i, idx = -1;
    gceSURF_FLAG dstSurfFlags = Flags;

    gcmHEADER_ARG("Hardware=0x%x Width=%d Height=%d "
                    "Format=%d",
                    Hardware, Width, Height,
                    Format);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    alignedWidth = Width;
    alignedHeight = Height;

    /* Align the width and height. */
    gcmONERROR(gcoHARDWARE_AlignToTile(
        Hardware, gcvSURF_BITMAP, Format, &alignedWidth, &alignedHeight, 1, gcvNULL, gcvNULL
        ));

    gcmONERROR(gcoSURF_QueryFormat(Format, &formatInfo));

    size = alignedWidth * formatInfo->bitsPerPixel  / 8 * alignedHeight;

    /* Do we have a fit surface? */
    for (i = 0; i < gcdTEMP_SURFACE_NUMBER; i += 1)
    {
        if ((Hardware->temp2DSurf[i] != gcvNULL)
            && (Hardware->temp2DSurf[i]->node.size >= size)
            && ((Hardware->temp2DSurf[i]->flags & gcvSURF_FLAG_CONTENT_PROTECTED)
                 == (dstSurfFlags & gcvSURF_FLAG_CONTENT_PROTECTED)))
        {
            if (idx == -1)
            {
                delta = Hardware->temp2DSurf[i]->node.size - size;
                idx = i;
            }
            else if (Hardware->temp2DSurf[i]->node.size - size < delta)
            {
                delta = Hardware->temp2DSurf[i]->node.size - size;
                idx = i;
            }
        }
    }

    if (idx != -1)
    {
        gcmASSERT((idx >= 0) && (idx < gcdTEMP_SURFACE_NUMBER));

        *SurfInfo = Hardware->temp2DSurf[idx];
        Hardware->temp2DSurf[idx] = gcvNULL;

        (*SurfInfo)->format              = Format;
        (*SurfInfo)->stride              = alignedWidth * formatInfo->bitsPerPixel / 8;
        (*SurfInfo)->alignedWidth        = alignedWidth;
        (*SurfInfo)->alignedHeight       = alignedHeight;
        (*SurfInfo)->is16Bit             = formatInfo->bitsPerPixel == 16;
        (*SurfInfo)->rotation            = gcvSURF_0_DEGREE;
        (*SurfInfo)->orientation         = gcvORIENTATION_TOP_BOTTOM;
        (*SurfInfo)->tiling              = gcvLINEAR;

        (*SurfInfo)->rect.left   = 0;
        (*SurfInfo)->rect.top    = 0;
        (*SurfInfo)->rect.right  = Width;
        (*SurfInfo)->rect.bottom = Height;
    }
    else
    {
        gcmONERROR(gcoHARDWARE_Alloc2DSurface(Hardware, Width, Height, Format, dstSurfFlags, SurfInfo));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Put2DTempSurface
**
**  Put back the temporary surface.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SurfInfo
**          The info of the surface to be free'd.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Put2DTempSurface(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_INFO_PTR surf = SurfInfo;
    gctINT i;

    gcmHEADER_ARG("Hardware=0x%x SurfInfo=0x%x", Hardware, SurfInfo);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmASSERT(SurfInfo != gcvNULL);

    for (i = 0; i < gcdTEMP_SURFACE_NUMBER; i++)
    {
        /* Is there an empty slot? */
        if (Hardware->temp2DSurf[i] == gcvNULL)
        {
            Hardware->temp2DSurf[i] = surf;
            break;
        }

        /* Swap the smaller one. */
        else if (Hardware->temp2DSurf[i]->node.size < surf->node.size)
        {
            gcsSURF_INFO_PTR temp = surf;

            surf = Hardware->temp2DSurf[i];
            Hardware->temp2DSurf[i] = temp;
        }
    }

    if (i == gcdTEMP_SURFACE_NUMBER)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(Hardware, surf));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ConvertPixel
**
**  Convert source pixel from source format to target pixel' target format.
**  The source format class should be either identical or convertible to
**  the target format class.
**
**  INPUT:
**
**      gctPOINTER SrcPixel, DstPixel,
**          Pointers to source and target pixels.
**
**      gctUINT SrcBitOffset, DstBitOffset
**          Bit offsets of the source and target pixels relative to their
**          respective pointers.
**
**      gcsSURF_FORMAT_INFO_PTR SrcFormat, DstFormat
**          Pointers to source and target pixel format descriptors.
**
**      gcsBOUNDARY_PTR SrcBoundary, DstBoundary
**          Pointers to optional boundary structures to verify the source
**          and target position. If the source is found to be beyond the
**          defined boundary, it will be assumed to be 0. If the target
**          is found to be beyond the defined boundary, the write will
**          be ignored. If boundary checking is not needed, gcvNULL can be
**          passed.
**
**  OUTPUT:
**
**      gctPOINTER DstPixel + DstBitOffset
**          Converted pixel.
*/
#if (gcdENABLE_3D || gcdENABLE_2D)
gceSTATUS
gcoHARDWARE_ConvertPixel(
    IN gctPOINTER SrcPixel,
    OUT gctPOINTER DstPixel,
    IN gctUINT SrcBitOffset,
    IN gctUINT DstBitOffset,
    IN gcsSURF_FORMAT_INFO_PTR SrcFormat,
    IN gcsSURF_FORMAT_INFO_PTR DstFormat,
    IN OPTIONAL gcsBOUNDARY_PTR SrcBoundary,
    IN OPTIONAL gcsBOUNDARY_PTR DstBoundary,
    IN gctBOOL SrcPixelOdd,
    IN gctBOOL DstPixelOdd
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcuPIXEL_FORMAT_CLASS srcFmtClass, dstFmtClass;

    gcmHEADER_ARG("SrcPixel=0x%x DstPixel=0x%x "
                    "SrcBitOffset=%d DstBitOffset=%d SrcFormat=0x%x "
                    "DstFormat=0x%x SrcBoundary=0x%x DstBoundary=0x%x",
                    SrcPixel, DstPixel,
                    SrcBitOffset, DstBitOffset, SrcFormat,
                    DstFormat, SrcBoundary, DstBoundary);

    if (SrcFormat->interleaved && SrcPixelOdd)
    {
        srcFmtClass = SrcFormat->uOdd;
    }
    else
    {
        srcFmtClass = SrcFormat->u;
    }

    if (DstFormat->interleaved && DstPixelOdd)
    {
        dstFmtClass = DstFormat->uOdd;
    }
    else
    {
        dstFmtClass = DstFormat->u;
    }

    if (SrcFormat->fmtClass == gcvFORMAT_CLASS_RGBA)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_RGBA)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.alpha,
                &DstFormat->u.rgba.alpha,
                SrcBoundary, DstBoundary,
                ~0U
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.red,
                &DstFormat->u.rgba.red,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.green,
                &DstFormat->u.rgba.green,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.blue,
                &DstFormat->u.rgba.blue,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else if (DstFormat->fmtClass == gcvFORMAT_CLASS_YUV)
        {
            gctUINT8 r[4] = {0};
            gctUINT8 g[4] = {0};
            gctUINT8 b[4] = {0};
            gctUINT8 y[4] = {0};
            gctUINT8 u[4] = {0};
            gctUINT8 v[4] = {0};

            /*
                Get RGB value.
            */

            gcmONERROR(_ConvertComponent(
                SrcPixel, r, SrcBitOffset, 0,
                &srcFmtClass.rgba.red, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, g, SrcBitOffset, 0,
                &srcFmtClass.rgba.green, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, b, SrcBitOffset, 0,
                &srcFmtClass.rgba.blue, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            /*
                Convert to YUV.
            */

            gcoHARDWARE_RGB2YUV(
                 r[0], g[0], b[0],
                 y, u, v
                );

            /*
                Average Us and Vs for odd pixels.
            */
            if ((DstFormat->interleaved & DstPixelOdd) != 0)
            {
                gctUINT8 curU[4] = {0}, curV[4] = {0};

                gcmONERROR(_ConvertComponent(
                    DstPixel, curU, DstBitOffset, 0,
                    &dstFmtClass.yuv.u, &gcvPIXEL_COMP_XXX8,
                    DstBoundary, gcvNULL, 0
                    ));

                gcmONERROR(_ConvertComponent(
                    DstPixel, curV, DstBitOffset, 0,
                    &dstFmtClass.yuv.v, &gcvPIXEL_COMP_XXX8,
                    DstBoundary, gcvNULL, 0
                    ));


                u[0] = (gctUINT8) (((gctUINT16) u[0] + (gctUINT16) curU[0]) >> 1);
                v[0] = (gctUINT8) (((gctUINT16) v[0] + (gctUINT16) curV[0]) >> 1);
            }

            /*
                Convert to the final format.
            */

            gcmONERROR(_ConvertComponent(
                y, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.yuv.y,
                gcvNULL, DstBoundary, 0
                ));

            gcmONERROR(_ConvertComponent(
                u, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.yuv.u,
                gcvNULL, DstBoundary, 0
                ));

            gcmONERROR(_ConvertComponent(
                v, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.yuv.v,
                gcvNULL, DstBoundary, 0
                ));
        }

        else if (DstFormat->fmtClass == gcvFORMAT_CLASS_LUMINANCE)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.red,
                &DstFormat->u.lum.value,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.rgba.alpha,
                &DstFormat->u.lum.alpha,
                SrcBoundary, DstBoundary,
                ~0U
                ));
        }

        else
        {
            /* Not supported combination. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else if (SrcFormat->fmtClass == gcvFORMAT_CLASS_DEPTH)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_DEPTH)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.depth.depth,
                &DstFormat->u.depth.depth,
                SrcBoundary, DstBoundary,
                ~0U
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.depth.stencil,
                &DstFormat->u.depth.stencil,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else if (SrcFormat->fmtClass == gcvFORMAT_CLASS_YUV)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_YUV)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &srcFmtClass.yuv.y,
                &dstFmtClass.yuv.y,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &srcFmtClass.yuv.u,
                &dstFmtClass.yuv.u,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &srcFmtClass.yuv.v,
                &dstFmtClass.yuv.v,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else if (DstFormat->fmtClass == gcvFORMAT_CLASS_RGBA)
        {
            gctUINT8 y[4]={0}, u[4]={0}, v[4]={0};
            gctUINT8 r[4], g[4], b[4];

            /*
                Get YUV value.
            */

            gcmONERROR(_ConvertComponent(
                SrcPixel, y, SrcBitOffset, 0,
                &srcFmtClass.yuv.y, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, u, SrcBitOffset, 0,
                &srcFmtClass.yuv.u, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, v, SrcBitOffset, 0,
                &srcFmtClass.yuv.v, &gcvPIXEL_COMP_XXX8,
                SrcBoundary, gcvNULL, 0
                ));

            /*
                Convert to RGB.
            */

            gcoHARDWARE_YUV2RGB(
                 y[0], u[0], v[0],
                 r, g, b
                );

            /*
                Convert to the final format.
            */

            gcmONERROR(_ConvertComponent(
                gcvNULL, DstPixel, 0, DstBitOffset,
                gcvNULL, &dstFmtClass.rgba.alpha,
                gcvNULL, DstBoundary, ~0U
                ));

            gcmONERROR(_ConvertComponent(
                r, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.rgba.red,
                gcvNULL, DstBoundary, 0
                ));

            gcmONERROR(_ConvertComponent(
                g, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.rgba.green,
                gcvNULL, DstBoundary, 0
                ));

            gcmONERROR(_ConvertComponent(
                b, DstPixel, 0, DstBitOffset,
                &gcvPIXEL_COMP_XXX8, &dstFmtClass.rgba.blue,
                gcvNULL, DstBoundary, 0
                ));
        }

        else
        {
            /* Not supported combination. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else if (SrcFormat->fmtClass == gcvFORMAT_CLASS_INDEX)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_INDEX)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.index.value,
                &DstFormat->u.index.value,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else
        {
            /* Not supported combination. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else if (SrcFormat->fmtClass == gcvFORMAT_CLASS_LUMINANCE)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_LUMINANCE)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.lum.alpha,
                &DstFormat->u.lum.alpha,
                SrcBoundary, DstBoundary,
                ~0U
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.lum.value,
                &DstFormat->u.lum.value,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else
        {
            /* Not supported combination. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else if (SrcFormat->fmtClass == gcvFORMAT_CLASS_BUMP)
    {
        if (DstFormat->fmtClass == gcvFORMAT_CLASS_BUMP)
        {
            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.alpha,
                &DstFormat->u.bump.alpha,
                SrcBoundary, DstBoundary,
                ~0U
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.l,
                &DstFormat->u.bump.l,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.v,
                &DstFormat->u.bump.v,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.u,
                &DstFormat->u.bump.u,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.q,
                &DstFormat->u.bump.q,
                SrcBoundary, DstBoundary,
                0
                ));

            gcmONERROR(_ConvertComponent(
                SrcPixel, DstPixel,
                SrcBitOffset, DstBitOffset,
                &SrcFormat->u.bump.w,
                &DstFormat->u.bump.w,
                SrcBoundary, DstBoundary,
                0
                ));
        }

        else
        {
            /* Not supported combination. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    else
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Return status. */
    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_3D/2D */

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHARDWARE_CopyPixels
**
**  Copy a rectangular area from one surface to another with format conversion.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Source
**          Pointer to the source surface descriptor.
**
**      gcsSURF_INFO_PTR Target
**          Pointer to the destination surface descriptor.
**
**      gctINT SourceX, SourceY
**          Source surface origin.
**
**      gctINT TargetX, TargetY
**          Target surface origin.
**
**      gctINT Width, Height
**          The size of the area. If Height is negative, the area will
**          be copied with a vertical flip.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_CopyPixels(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Source,
    IN gcsSURF_INFO_PTR Target,
    IN gctINT SourceX,
    IN gctINT SourceY,
    IN gctINT TargetX,
    IN gctINT TargetY,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_INFO_PTR source = gcvNULL;
    gctPOINTER memory = gcvNULL;
    gcsSURF_FORMAT_INFO_PTR srcFormatInfo;
    gcsSURF_FORMAT_INFO_PTR dstFormatInfo;
    gctBOOL fastPathCopy = gcvFALSE;
    gctBOOL hwcopy = gcvTRUE;
    gctBOOL stalled = gcvFALSE;
    gctBOOL    flip = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x Source=0x%x Target=0x%x "
                    "SourceX=%d SourceY=%d TargetX=%d "
                    "TargetY=%d Width=%d Height=%d",
                    Hardware, Source, Target,
                    SourceX, SourceY, TargetX,
                    TargetY, Width, Height);

    gcmGETHARDWARE(Hardware);

    /* Get surface formats. */
    srcFormatInfo = &Source->formatInfo;
    dstFormatInfo = &Target->formatInfo;


    /* Get flip. */
    if (Height < 0)
    {
        flip   = gcvTRUE;
        Height = -Height;
    }

    do
    {
        /***********************************************************************
        ** Fast path.
        */

        gctINT     x;
        gctINT     y;

        gctUINT8_PTR srcAddr0;
        gctUINT8_PTR srcAddr1;

        gctUINT8_PTR dstAddr0;
        gctUINT8_PTR dstAddr1;

        gctUINT    bytesPerPixel;

        gctBOOL    srcTiled;
        gctBOOL    srcMultiPipe;
        gctBOOL    dstTiled;
        gctBOOL    dstMultiPipe;

        /* Invalidate surface cache. */
        gcmVERIFY_OK(
            gcoSURF_NODE_Cache(&Source->node,
                               Source->node.logical,
                               Source->size,
                               gcvCACHE_INVALIDATE));

        gcmVERIFY_OK(
            gcoSURF_NODE_Cache(&Target->node,
                               Target->node.logical,
                               Target->size,
                               gcvCACHE_INVALIDATE));

        if (
        /* If tile status is not disabled we need to go to slow path */
            (!Source->tileStatusDisabled) ||
            (!Target->tileStatusDisabled) ||
        /* Fast path won't process multi-sampling. */
            (Source->samples.x > 1) || (Source->samples.y > 1) ||
        /* Wont's process different bitsPerPixel case. */
            (srcFormatInfo->bitsPerPixel != dstFormatInfo->bitsPerPixel) ||
        /* Wont's process format conversion. */
            ((Source->format != Target->format) &&
               !((Source->format  == gcvSURF_A8R8G8B8) &&
                (Target->format == gcvSURF_X8R8G8B8)) &&
               !((Source->format  == gcvSURF_A8B8G8R8) &&
                (Target->format == gcvSURF_X8B8G8R8)))
        )
        {
            break;
        }

        /* Get shortcut. */
        bytesPerPixel = srcFormatInfo->bitsPerPixel / 8;

        /* Make sure HW finished before CPU copy */
        gcmERR_BREAK(gcoHARDWARE_Commit(Hardware));
        gcmERR_BREAK(gcoHARDWARE_Stall(Hardware));

        /* Lock source surface. */
        if (gcmIS_ERROR(gcoHARDWARE_Lock(&Source->node,
                                         gcvNULL,
                                         (gctPOINTER*)&srcAddr0)))
        {
            break;
        }

        /* Lock target surface. */
        if (gcmIS_ERROR(gcoHARDWARE_Lock(&Target->node,
                                         gcvNULL,
                                         (gctPOINTER*)&dstAddr0)))
        {
            gcmVERIFY_OK(
                gcoHARDWARE_Unlock(&Source->node,
                                   Source->type));

            break;
        }

        /* Get source tiling info. */
        if (Source->type == gcvSURF_BITMAP)
        {
            srcTiled     = gcvFALSE;
            srcMultiPipe = gcvFALSE;
        }
        else
        {
            srcTiled     = gcvTRUE;

            srcMultiPipe = (Source->tiling & gcvTILING_SPLIT_BUFFER);
        }

        /* Get target tiling info. */
        if (Target->type == gcvSURF_BITMAP)
        {
            dstTiled     = gcvFALSE;
            dstMultiPipe = gcvFALSE;
        }
        else
        {
            dstTiled     = gcvTRUE;
            dstMultiPipe = (Target->tiling & gcvTILING_SPLIT_BUFFER);
        }

        srcAddr0 = srcAddr0 + Source->offset;
        srcAddr1 = srcAddr0 + Source->bottomBufferOffset;
        dstAddr0 = dstAddr0 + Target->offset;
        dstAddr1 = dstAddr0 + Target->bottomBufferOffset;

        for (y = 0; y < Height; y++)
        {
            for (x = 0; x < Width; x++)
            {
                gctUINT32 xt, yt;
                gctUINT32 xxt, yyt;

                gctUINT8_PTR srcAddr;
                gctUINT8_PTR dstAddr;

                gctUINT32 xl = SourceX + x;
                gctUINT32 yl = SourceY + y;

                if (srcTiled)
                {
                    if (srcMultiPipe)
                    {
                        /* X' = 15'{ X[14:4], Y[2], X[2:0] } */
                        xt = (xl & ~0x8) | ((yl & 0x4) << 1);
                        /* Y' = 15'{ 0, Y[14:3], Y[1:0] } */
                        yt = ((yl & ~0x7) >> 1) | (yl & 0x3);

                        /* Get base source address. */
                        srcAddr = (((xl >> 3) ^ (yl >> 2)) & 0x01) ? srcAddr1 : srcAddr0;
                    }
                    else
                    {
                        xt = xl;
                        yt = yl;

                        /* Get base source address. */
                        srcAddr = srcAddr0;
                    }

                    xxt = gcmTILE_OFFSET_X(xt, yt, Source->superTiled, Hardware->config->superTileMode);
                    yyt = gcmTILE_OFFSET_Y(xt, yt, Source->superTiled);

                    srcAddr = srcAddr
                            + yyt * Source->stride
                            + xxt * bytesPerPixel;
                }
                else
                {
                    srcAddr = srcAddr0
                            + yl * Source->stride
                            + xl * bytesPerPixel;
                }

                xl = TargetX + x;

                if (flip)
                {
                    yl = (Height - y - 1) + TargetY;
                }
                else
                {
                    yl = TargetY + y;
                }

                if (dstTiled)
                {
                    if (dstMultiPipe)
                    {
                        /* X' = 15'{ X[14:4], Y[2], X[2:0] } */
                        xt = (xl & ~0x8) | ((yl & 0x4) << 1);
                        /* Y' = 15'{ 0, Y[14:3], Y[1:0] } */
                        yt = ((yl & ~0x7) >> 1) | (yl & 0x3);

                        /* Get base target address. */
                        dstAddr = (((xl >> 3) ^ (yl >> 2)) & 0x01) ? dstAddr1 : dstAddr0;
                    }
                    else
                    {
                        xt = xl;
                        yt = yl;

                        /* Get base source address. */
                        dstAddr = dstAddr0;
                    }

                    xxt = gcmTILE_OFFSET_X(xt, yt, Target->superTiled, Hardware->config->superTileMode);
                    yyt = gcmTILE_OFFSET_Y(xt, yt, Target->superTiled);

                    dstAddr = dstAddr
                            + yyt * Target->stride
                            + xxt * bytesPerPixel;
                }
                else
                {

                    dstAddr = dstAddr0
                            + yl * Target->stride
                            + xl * bytesPerPixel;
                }

                switch (bytesPerPixel)
                {
                case 4:
                default:
                    *(gctUINT32_PTR) dstAddr = *(gctUINT32_PTR) srcAddr;
                    break;

                case 8:
                    *(gctUINT64_PTR) dstAddr = *(gctUINT64_PTR) srcAddr;
                    break;

                case 2:
                    *(gctUINT16_PTR) dstAddr = *(gctUINT16_PTR) srcAddr;
                    break;

                case 1:
                    *(gctUINT8_PTR)  dstAddr = *(gctUINT8_PTR)  srcAddr;
                    break;
                }
            }
        }

        gcmVERIFY_OK(
            gcoHARDWARE_Unlock(&Source->node,
                               Source->type));

        gcmVERIFY_OK(
            gcoHARDWARE_Unlock(&Target->node,
                               Target->type));
        goto _Exit;
    }
    while (gcvFALSE);

    /* Check the format to see if hw support*/
    if (gcmIS_ERROR(_ConvertResolveFormat(Hardware,
                                         Source->format,
                                         gcvNULL,
                                         gcvNULL,
                                         gcvNULL)) ||
        gcmIS_ERROR(_ConvertResolveFormat(Hardware,
                                          Target->format,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL))
     )
    {
       hwcopy = gcvFALSE;
    }

    /* Check if the source supports fast path. */
    if (
        /* Check whether the source is multi-sampled and is larger than the
        ** resolve size. */
            (Source->alignedWidth  / Source->samples.x >= Hardware->resolveAlignmentX)
        &&  (Source->alignedHeight / Source->samples.y >= Hardware->resolveAlignmentY)

        /* Check whether this is a multi-pixel render target or depth buffer. */
        &&  ((Source->type == gcvSURF_RENDER_TARGET)
            || (Source->type == gcvSURF_DEPTH)
        )
        && hwcopy
    )
    {
        gcsPOINT zero, origin, size;
        gceORIENTATION orientation;
        gctUINT32 tileWidth, tileHeight;

        /* Find the tile aligned region to resolve. */
        if (Source->superTiled)
        {
            tileWidth = 64;
            tileHeight = 64;

            if (Source->tiling & gcvTILING_SPLIT_BUFFER)
            {
                tileHeight *= Hardware->config->pixelPipes;
            }

            /* Align origin to top right. */
            origin.x = SourceX & ~(tileWidth - 1);
            origin.y = SourceY & ~(tileHeight - 1);

            /* Align size to end of bottom right tile corner. */
            size.x = gcmALIGN((SourceX + Width), tileWidth)
                     - origin.x;
            size.y = gcmALIGN((SourceY + Height), tileHeight)
                     - origin.y;
        }
        else
        {
            /* Align origin to top right. */
            origin.x = gcmALIGN_BASE(SourceX, Hardware->resolveAlignmentX);
            origin.y = gcmALIGN_BASE(SourceY, Hardware->resolveAlignmentY);

            /* Align size to end of bottom right tile corner. */
            size.x = gcmALIGN((SourceX + Width), Hardware->resolveAlignmentX)
                     - origin.x;
            size.y = gcmALIGN((SourceY + Height), Hardware->resolveAlignmentY)
                     - origin.y;
        }

        /* Create a temporary surface. */
        if (Target->type == gcvSURF_BITMAP)
        {
            gcmONERROR(
                gcoHARDWARE_AllocateTemporarySurface(Hardware,
                                                 size.x,
                                                 size.y,
                                                 dstFormatInfo,
                                                 gcvSURF_BITMAP,
                                                 Target->flags));
        }
        else
        {
            gcmONERROR(
                gcoHARDWARE_AllocateTemporarySurface(Hardware,
                                                 size.x,
                                                 size.y,
                                                 srcFormatInfo,
                                                 gcvSURF_BITMAP,
                                                 Target->flags));
        }

        /* Use same orientation as source. */
        orientation = Hardware->tempBuffer.orientation;
        Hardware->tempBuffer.orientation = Source->orientation;

        /* Lock it. */
        gcmONERROR(
            gcoHARDWARE_Lock(&Hardware->tempBuffer.node,
                             gcvNULL,
                             &memory));

        /* Resolve the source into the temporary surface. */
        zero.x = 0;
        zero.y = 0;
        gcmASSERT(size.x == Hardware->tempBuffer.rect.right);
        gcmASSERT(size.y == Hardware->tempBuffer.rect.bottom);

        /* Should NOT call _ResolveRect directly, as we need handle(switch/pause) tile status in gcoHARDWARE_ResolveRect.
        ** In gcoSURF_CopyPixel, we will disable source surface tile status, but which doesn't mean we really
        ** turn off the HW FC as source is probably always FC-disabled.
        */
        gcmONERROR(
            gcoHARDWARE_ResolveRect(
                         Hardware,
                         Source,
                         &Hardware->tempBuffer,
                         &origin,
                         &zero,
                         &size,
                         gcvFALSE));

        /* Stall the hardware. */
        gcmONERROR(
            gcoHARDWARE_Commit(Hardware));

        gcmONERROR(
            gcoHARDWARE_Stall(Hardware));

        stalled = gcvTRUE;

        /* Invalidate CPU cache of temp surface, as gcoHARDWARE_ResolveRect would warm up
        ** CPU cache by CPU reading, but then write by GPU. So need to force the coming CPU
        ** copy to read directly from memory.
        */
        gcmONERROR(
            gcoSURF_NODE_Cache(&Hardware->tempBuffer.node,
                               memory,
                               Hardware->tempBuffer.size,
                               gcvCACHE_INVALIDATE));

        /* Restore orientation. */
        Hardware->tempBuffer.orientation = orientation;

        /* Change SourceX, SourceY, to be relative to tempBuffer. */
        SourceX -= origin.x;
        SourceY -= origin.y;

        if (Target->type == gcvSURF_BITMAP)
        {
            gctINT32 i;
            gctINT srcOffset, dstOffset, bpp;
            gctUINT8_PTR src;
            gctUINT8_PTR dst;
            gctINT copy_size;

            bpp = dstFormatInfo->bitsPerPixel / 8;

            copy_size = Width * bpp;

            srcOffset = SourceY * Hardware->tempBuffer.stride + SourceX * bpp;
            src = Hardware->tempBuffer.node.logical + srcOffset;

            if (flip)
            {
                dstOffset = (Height + TargetY - 1) * Target->stride + TargetX * bpp;
                dst = Target->node.logical + dstOffset;
                for (i = 0; i < Height; i++)
                {
                    gcoOS_MemCopy(dst, src, copy_size);
                    src += Hardware->tempBuffer.stride;
                    dst -= Target->stride;
                }
            }
            else
            {
                dstOffset = TargetY * Target->stride + TargetX * bpp;
                dst = Target->node.logical + dstOffset;

                if (((gctUINT)Target->rect.right == Hardware->tempBuffer.alignedWidth) &&
                    ((gctUINT)Target->rect.bottom == Hardware->tempBuffer.alignedHeight))
                {
                    gcoOS_MemCopy(dst, src, Hardware->tempBuffer.alignedWidth *
                                            Hardware->tempBuffer.alignedHeight * bpp);
                }
                else
                {
                    for (i = 0; i < Height; i++)
                    {
                        gcoOS_MemCopy(dst, src, copy_size);
                        src += Hardware->tempBuffer.stride;
                        dst += Target->stride;
                    }
                }
            }

            if (memory != gcvNULL)
            {
                /* Unlock the temporary buffer. */
                gcmVERIFY_OK(
                    gcoHARDWARE_Unlock(&Hardware->tempBuffer.node,
                                       Hardware->tempBuffer.type));
            }
        }

#if gcdENABLE_3D
        else if ((Target->type == gcvSURF_TEXTURE) && (!flip))
        {
            gctUINT32   srcOffset;
            gctUINT32   srcBitsPerPixel;

            gcmONERROR(
               gcoHARDWARE_ConvertFormat(Source->format,
                                         &srcBitsPerPixel,
                                         gcvNULL));


            srcOffset = (SourceY * Hardware->tempBuffer.alignedWidth + SourceX)* (srcBitsPerPixel / 8);

            /* Try using faster UploadTexture function. */
            status = gcoHARDWARE_UploadTexture(Target,
                                               0,
                                               TargetX,
                                               TargetY,
                                               Width,
                                               Height,
                                               (gctPOINTER)((gctUINTPTR_T)memory + srcOffset),
                                               Hardware->tempBuffer.stride,
                                               Hardware->tempBuffer.format);
        }
#endif

        /* Use temporary buffer as source. */
        source = &Hardware->tempBuffer;

        if (status == gcvSTATUS_OK)
        {
            goto _Exit;
        }
    }
    else
    {
        /* Use source as-is. */
        source = Source;
    }

    if (!stalled)
    {
        /* Synchronize with the GPU. */
        gcmONERROR(gcoHARDWARE_Commit(Hardware));
        gcmONERROR(gcoHARDWARE_Stall(Hardware));
    }

    do
    {
        /***********************************************************************
        ** Slow path
        */

        gctINT     x;
        gctINT     y;

        gctUINT8_PTR srcAddr0;
        gctUINT8_PTR srcAddr1;

        gctUINT8_PTR dstAddr0;
        gctUINT8_PTR dstAddr1;

        gctUINT    srcBytesPerPixel;
        gctUINT    dstBytesPerPixel;

        gctBOOL    srcTiled;
        gctBOOL    srcMultiPipe;
        gctBOOL    dstTiled;
        gctBOOL    dstMultiPipe;

        if ((source->samples.x > 1) || (source->samples.y > 1))
        {
            /* Cannot process multi-sampling. */
            break;
        }

        /* Get shortcut. */
        srcBytesPerPixel = srcFormatInfo->bitsPerPixel / 8;
        dstBytesPerPixel = dstFormatInfo->bitsPerPixel / 8;

        /* Get logical memory. */
        srcAddr0 = source->node.logical + source->offset;
        srcAddr1 = srcAddr0 + source->bottomBufferOffset;
        dstAddr0 = Target->node.logical + Target->offset;
        dstAddr1 = dstAddr0 + Target->bottomBufferOffset;

        /* Get source tiling info. */
        if (source->type == gcvSURF_BITMAP)
        {
            srcTiled     = gcvFALSE;
            srcMultiPipe = gcvFALSE;
        }
        else
        {
            srcTiled     = gcvTRUE;

            srcMultiPipe = (source->tiling & gcvTILING_SPLIT_BUFFER);
        }

        /* Get target tiling info. */
        if (Target->type == gcvSURF_BITMAP)
        {
            dstTiled     = gcvFALSE;
            dstMultiPipe = gcvFALSE;
        }
        else
        {
            dstTiled     = gcvTRUE;
            dstMultiPipe = (Target->tiling & gcvTILING_SPLIT_BUFFER);
        }

        for (y = 0; y < Height; y++)
        {
            for (x = 0; x < Width; x++)
            {
                gctUINT32 xt, yt;
                gctUINT32 xxt, yyt;

                gctUINT8_PTR srcAddr;
                gctUINT8_PTR dstAddr;

                gctUINT32 xl = SourceX + x;
                gctUINT32 yl = SourceY + y;

                if (srcTiled)
                {
                    if (srcMultiPipe)
                    {
                        /* X' = 15'{ X[14:4], Y[2], X[2:0] } */
                        xt = (xl & ~0x8) | ((yl & 0x4) << 1);
                        /* Y' = 15'{ 0, Y[14:3], Y[1:0] } */
                        yt = ((yl & ~0x7) >> 1) | (yl & 0x3);

                        /* Get base source address. */
                        srcAddr = (((xl >> 3) ^ (yl >> 2)) & 0x01) ? srcAddr1
                                : srcAddr0;
                    }
                    else
                    {
                        xt = xl;
                        yt = yl;

                        /* Get base source address. */
                        srcAddr = srcAddr0;
                    }

                    xxt = gcmTILE_OFFSET_X(xt, yt, source->superTiled, Hardware->config->superTileMode);
                    yyt = gcmTILE_OFFSET_Y(xt, yt, source->superTiled);

                    srcAddr = srcAddr
                            + yyt * source->stride
                            + xxt * srcBytesPerPixel;
                }
                else
                {
                    srcAddr = srcAddr0
                            + yl * source->stride
                            + xl * srcBytesPerPixel;
                }

                xl = TargetX + x;

                if (flip)
                {
                    yl = (Height - y - 1) + TargetY;
                }
                else
                {
                    yl = TargetY + y;
                }

                if (dstTiled)
                {
                    if (dstMultiPipe)
                    {
                        /* X' = 15'{ X[14:4], Y[2], X[2:0] } */
                        xt = (xl & ~0x8) | ((yl & 0x4) << 1);
                        /* Y' = 15'{ 0, Y[14:3], Y[1:0] } */
                        yt = ((yl & ~0x7) >> 1) | (yl & 0x3);

                        /* Get base target address. */
                        dstAddr = (((xl >> 3) ^ (yl >> 2)) & 0x01) ? dstAddr1
                                : dstAddr0;
                    }
                    else
                    {
                        xt = xl;
                        yt = yl;

                        /* Get base source address. */
                        dstAddr = dstAddr0;
                    }

                    xxt = gcmTILE_OFFSET_X(xt, yt, Target->superTiled, Hardware->config->superTileMode);
                    yyt = gcmTILE_OFFSET_Y(xt, yt, Target->superTiled);

                    dstAddr = dstAddr
                            + yyt * Target->stride
                            + xxt * dstBytesPerPixel;
                }
                else
                {
                    dstAddr = dstAddr0
                            + yl * Target->stride
                            + xl * dstBytesPerPixel;
                }

                gcmVERIFY_OK(
                    gcoHARDWARE_ConvertPixel(
                        srcAddr, dstAddr,
                        0, 0,
                        srcFormatInfo,
                        dstFormatInfo,
                        gcvNULL, gcvNULL,
                        0, 0
                        ));
            }
        }
        goto _Exit;
    }
    while (gcvFALSE);

    do
    {
        /***********************************************************************
        ** Local variable definitions.
        */

        /* Tile dimensions. */
        gctINT32 srcTileWidth, srcTileHeight, srcTileSize;
        gctINT32 dstTileWidth, dstTileHeight, dstTileSize;

        /* Pixel sizes. */
        gctINT srcPixelSize, dstPixelSize;

        /* Walking boundaries. */
        gctINT dstX1, dstY1, dstX2, dstY2;
        gctINT srcX1, srcY1, srcX2, srcY2;

        /* Source step. */
        gctINT srcStepX, srcStepY;

        /* Coordinate alignment. */
        gctUINT srcAlignX, srcAlignY;
        gctUINT dstAlignX, dstAlignY;

        /* Pixel group sizes. */
        gctUINT srcGroupX, srcGroupY;
        gctUINT dstGroupX, dstGroupY;

        /* Line surface offsets. */
        gctUINT srcLineOffset, dstLineOffset;

        /* Counts to keep track of when to add long vs. short offsets. */
        gctINT srcLeftPixelCount, srcMidPixelCount;
        gctINT srcTopLineCount, srcMidLineCount;
        gctINT dstLeftPixelCount, dstMidPixelCount;
        gctINT dstTopLineCount, dstMidLineCount;
        gctINT srcPixelCount, dstPixelCount;
        gctINT srcLineCount, dstLineCount;

        /* Long and short offsets. */
        gctINT srcShortStride, srcLongStride;
        gctINT srcShortOffset, srcLongOffset;
        gctINT dstShortStride, dstLongStride;
        gctINT dstShortOffset, dstLongOffset;

        /* Direct copy flag and the intermediate format. */
        gctBOOL directCopy;
        gcsSURF_FORMAT_INFO_PTR intFormatInfo = gcvNULL;

        /* Boundary checking. */
        gcsBOUNDARY srcBoundary;
        gcsBOUNDARY dstBoundary;
        gcsBOUNDARY_PTR srcBoundaryPtr;
        gcsBOUNDARY_PTR dstBoundaryPtr;

        gctUINT loopCount = 0;
        /***********************************************************************
        ** Get surface format details.
        */

        /* Compute pixel sizes. */
        srcPixelSize = srcFormatInfo->bitsPerPixel / 8;
        dstPixelSize = dstFormatInfo->bitsPerPixel / 8;

        /***********************************************************************
        ** Validate inputs.
        */

        /* Verify that the surfaces are locked. */
        gcmVERIFY_LOCK(source);
        gcmVERIFY_LOCK(Target);

        /* Check support. */
        if (Width < 0)
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        if ((srcFormatInfo->interleaved) && (source->samples.x != 1))
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        if ((dstFormatInfo->interleaved) && (Target->samples.x != 1))
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* FIXME:
         * Need support super tile, multi-tile, multi-supertile targets. */
        if ((Target->tiling != gcvLINEAR) &&
            (Target->tiling != gcvTILED))
        {
            gcmPRINT("libGAL: Target tiling=%d not supported",
                     Target->tiling);

            /* Break. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /***********************************************************************
        ** Determine the type of operation and the intermediate format.
        */

        directCopy
            =  (source->samples.x == Target->samples.x)
            && (source->samples.y == Target->samples.y);

        if (!directCopy)
        {
            gceSURF_FORMAT intermediateFormat
                = (srcFormatInfo->fmtClass == gcvFORMAT_CLASS_RGBA)
                    ? gcvSURF_A8R8G8B8
                    : srcFormatInfo->format;

            gcmERR_BREAK(gcoSURF_QueryFormat(intermediateFormat, &intFormatInfo));
        }

        /***********************************************************************
        ** Determine the size of the pixel groups.
        */

        srcGroupX = (srcFormatInfo->interleaved || (source->samples.x != 1))
            ? 2
            : 1;

        dstGroupX = (dstFormatInfo->interleaved || (Target->samples.x != 1))
            ? 2
            : 1;

        srcGroupY = source->samples.y;
        dstGroupY = Target->samples.y;

        /* Determine coordinate alignments. */
        srcAlignX = ~(srcGroupX - 1);
        srcAlignY = ~(srcGroupY - 1);
        dstAlignX = ~(dstGroupX - 1);
        dstAlignY = ~(dstGroupY - 1);

        /***********************************************************************
        ** Determine tile parameters.
        */

        gcmERR_BREAK(gcoHARDWARE_GetSurfaceTileSize(
            source, &srcTileWidth, &srcTileHeight
            ));

        gcmERR_BREAK(gcoHARDWARE_GetSurfaceTileSize(
            Target, &dstTileWidth, &dstTileHeight
            ));

        srcTileSize = srcTileWidth * srcTileHeight * srcPixelSize;
        dstTileSize = dstTileWidth * dstTileHeight * dstPixelSize;

        /* Determine pixel and line counts per tile. */
        srcMidPixelCount = (srcTileWidth  == 1) ? 1 : srcTileWidth  / srcGroupX;
        srcMidLineCount  = (srcTileHeight == 1) ? 1 : srcTileHeight / srcGroupY;
        dstMidPixelCount = (dstTileWidth  == 1) ? 1 : dstTileWidth  / dstGroupX;
        dstMidLineCount  = (dstTileHeight == 1) ? 1 : dstTileHeight / dstGroupY;

        /***********************************************************************
        ** Determine the initial horizontal source coordinates.
        */

        srcX1    = source->samples.x *  SourceX;
        srcX2    = source->samples.x * (SourceX + Width);
        srcStepX = source->samples.x;

        /* Pixels left to go before using the long offset. */
        srcLeftPixelCount
            = ((~((gctUINT) srcX1)) & (srcTileWidth - 1)) / srcGroupX + 1;

        srcShortOffset
            = srcPixelSize * srcGroupX;

        srcLongOffset
            = srcTileSize - srcPixelSize * (srcTileWidth - srcGroupX);

        /***********************************************************************
        ** Determine the initial vertical source coordinates.
        */

        /* I guess the flip code is wrong here. SourcY is always from smaller corner */
        gcmASSERT(flip == gcvFALSE);

        if (Height < 0)
        {
            srcY1    =            source->samples.y * (SourceY - Height - 1);
            srcY2    =            source->samples.y * (SourceY          - 1);
            srcStepY = - (gctINT) source->samples.y;

            /* Lines left to go before using the long stride. */
            srcTopLineCount
                = (((gctUINT) srcY1) & (srcTileHeight - 1)) / srcGroupY + 1;

            srcLongStride = - (gctINT) ((srcTileHeight == 1)
                ? source->stride * srcGroupY
                : source->stride * srcTileHeight
                  - srcTileWidth * srcPixelSize * (srcTileHeight - srcGroupY));

            srcShortStride = (srcTileHeight == 1)
                ? srcLongStride
                : - (gctINT) (srcTileWidth * srcPixelSize * srcGroupY);

            /* Determine the vertical target range. */
            dstY1 = Target->samples.y *  TargetY;
            dstY2 = Target->samples.y * (TargetY - Height);
        }
        else
        {
            srcY1    = source->samples.y *  SourceY;
            srcY2    = source->samples.y * (SourceY + Height);
            srcStepY = source->samples.y;

            /* Lines left to go before using the long stride. */
            srcTopLineCount
                = ((~((gctUINT) srcY1)) & (srcTileHeight - 1)) / srcGroupY + 1;

            srcLongStride = (srcTileHeight == 1)
                ? source->stride * srcGroupY
                : source->stride * srcTileHeight
                  - srcTileWidth * srcPixelSize * (srcTileHeight - srcGroupY);

            srcShortStride = (srcTileHeight == 1)
                ? srcLongStride
                : (gctINT) (srcTileWidth * srcPixelSize * srcGroupY);

            /* Determine the vertical target range. */
            dstY1 = Target->samples.y *  TargetY;
            dstY2 = Target->samples.y * (TargetY + Height);
        }

        /***********************************************************************
        ** Determine the initial target coordinates.
        */

        dstX1 = Target->samples.x *  TargetX;
        dstX2 = Target->samples.x * (TargetX + Width);

        /* Pixels left to go before using the long offset. */
        dstLeftPixelCount
            = ((~((gctUINT) dstX1)) & (dstTileWidth - 1)) / dstGroupX + 1;

        /* Lines left to go before using the long stride. */
        dstTopLineCount
            = ((~((gctUINT) dstY1)) & (dstTileHeight - 1)) / dstGroupY + 1;

        dstShortOffset = dstPixelSize * dstGroupX;
        dstLongOffset = dstTileSize - dstPixelSize * (dstTileWidth - dstGroupX);

        dstLongStride = (dstTileHeight == 1)
            ? Target->stride * dstGroupY
            : Target->stride * dstTileHeight
              - dstTileWidth * dstPixelSize * (dstTileHeight - dstGroupY);

        dstShortStride = (dstTileHeight == 1)
            ? dstLongStride
            : (gctINT) (dstTileWidth * dstPixelSize * dstGroupY);

        /***********************************************************************
        ** Setup the boundary checking.
        */

        if ((srcX1 < 0) || (srcX2 >= source->rect.right) ||
            (srcY1 < 0) || (srcY2 >= source->rect.bottom))
        {
            srcBoundaryPtr = &srcBoundary;
            srcBoundary.width  = source->rect.right;
            srcBoundary.height = source->rect.bottom;
        }
        else
        {
            srcBoundaryPtr = gcvNULL;
        }

        if ((dstX1 < 0) || (dstX2 > Target->rect.right) ||
            (dstY1 < 0) || (dstY2 > Target->rect.bottom))
        {
            dstBoundaryPtr = &dstBoundary;
            dstBoundary.width  = Target->rect.right;
            dstBoundary.height = Target->rect.bottom;
        }
        else
        {
            dstBoundaryPtr = gcvNULL;
        }

        /***********************************************************************
        ** Check the fast path for copy
        */
        if ((srcTileHeight == 1) && (dstTileHeight == 1) &&         /*no tile*/
            (source->samples.x == 1) && (source->samples.y == 1) && /*no multisample*/
            (Target->samples.x == 1) && (Target->samples.y == 1) &&
            ((source->format == Target->format) ||
              (source->format == gcvSURF_X8B8G8R8 && Target->format == gcvSURF_A8B8G8R8)))                     /*no format convertion*/
            fastPathCopy = gcvTRUE;

        /***********************************************************************
        ** Compute the source and target initial offsets.
        */

        srcLineOffset

            /* Skip full tile lines. */
            = ((gctINT) (srcY1 & srcAlignY & ~(srcTileHeight - 1)))
                * source->stride

            /* Skip full tiles in the same line. */
            + ((((gctINT) (srcX1 & srcAlignX & ~(srcTileWidth - 1)))
                * srcTileHeight * srcFormatInfo->bitsPerPixel) >> 3)

            /* Skip full rows within the target tile. */
            + ((((gctINT) (srcY1 & srcAlignY & (srcTileHeight - 1)))
                * srcTileWidth * srcFormatInfo->bitsPerPixel) >> 3)

            /* Skip pixels on the target row. */
            + (((gctINT) (srcX1 & srcAlignX & (srcTileWidth - 1)))
                * srcFormatInfo->bitsPerPixel >> 3);

        dstLineOffset

            /* Skip full tile lines. */
            = ((gctINT) (dstY1 & dstAlignY & ~(dstTileHeight - 1)))
                * Target->stride

            /* Skip full tiles in the same line. */
            + ((((gctINT) (dstX1 & dstAlignX & ~(dstTileWidth - 1)))
                * dstTileHeight * dstFormatInfo->bitsPerPixel) >> 3)

            /* Skip full rows within the target tile. */
            + ((((gctINT) (dstY1 & dstAlignY & (dstTileHeight - 1)))
                * dstTileWidth * dstFormatInfo->bitsPerPixel) >> 3)

            /* Skip pixels on the target row. */
            + (((gctINT) (dstX1 & dstAlignX & (dstTileWidth - 1)))
                * dstFormatInfo->bitsPerPixel >> 3);

        /* Initialize line counts. */
        srcLineCount = srcTopLineCount;
        dstLineCount = dstTopLineCount;

        /* Initialize the vertical coordinates. */
        srcBoundary.y = srcY1;
        dstBoundary.y = dstY1;

        /* Go through all lines. */
        while ((srcBoundary.y != srcY2) && (loopCount < MAX_LOOP_COUNT))
        {
            gctUINT loopCountA = 0;

            /* Initialize the initial pixel addresses. */
            gctUINT8_PTR srcPixelAddress = source->node.logical + srcLineOffset;
            gctUINT8_PTR dstPixelAddress = Target->node.logical + dstLineOffset;

            loopCount++;

            if (fastPathCopy)
            {
                gctINT bytePerLine = (srcX2 - srcX1) * srcPixelSize;

                gcoOS_MemCopy(dstPixelAddress, srcPixelAddress, bytePerLine);

                srcLineOffset += srcLongStride;
                dstLineOffset += dstLongStride;
                srcBoundary.y += srcStepY;

                continue;
            }

            /* Initialize pixel counts. */
            srcPixelCount = srcLeftPixelCount;
            dstPixelCount = dstLeftPixelCount;

            /* Initialize the horizontal coordinates. */
            srcBoundary.x = srcX1;
            dstBoundary.x = dstX1;

            /* Go through all columns. */
            while ((srcBoundary.x != srcX2) && (loopCountA < MAX_LOOP_COUNT))
            {
                /* Determine oddity of the pixels. */
                gctINT srcOdd = srcBoundary.x & srcFormatInfo->interleaved;
                gctINT dstOdd = dstBoundary.x & dstFormatInfo->interleaved;

                loopCountA++;

                /* Direct copy without resampling. */
                if (directCopy)
                {
                    gctUINT8 x, y;

                    for (y = 0; y < source->samples.y; y++)
                    {
                        /* Determine the vertical pixel offsets. */
                        gctUINT srcVerOffset = y * srcTileWidth * srcPixelSize;
                        gctUINT dstVerOffset = y * dstTileWidth * dstPixelSize;

                        for (x = 0; x < source->samples.x; x++)
                        {
                            /* Determine the horizontal pixel offsets. */
                            gctUINT srcHorOffset = x * srcPixelSize;
                            gctUINT dstHorOffset = x * dstPixelSize;

                            /* Convert pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                srcPixelAddress + srcVerOffset + srcHorOffset,
                                dstPixelAddress + dstVerOffset + dstHorOffset,
                                0, 0,
                                srcFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                srcOdd,
                                dstOdd
                                ));
                        }

                        /* Error? */
                        if (gcmIS_ERROR(status))
                        {
                            break;
                        }
                    }

                    /* Error? */
                    if (gcmIS_ERROR(status))
                    {
                        break;
                    }
                }

                /* Need to resample. */
                else
                {
                    gctUINT32 data[4];

                    /* Read the top left pixel. */
                    gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                        srcPixelAddress,
                        &data[0],
                        0, 0,
                        srcFormatInfo,
                        intFormatInfo,
                        srcBoundaryPtr,
                        dstBoundaryPtr,
                        0, 0
                        ));

                    /* Replicate horizontally? */
                    if (source->samples.x == 1)
                    {
                        data[1] = data[0];

                        /* Replicate vertically as well? */
                        if (source->samples.y == 1)
                        {
                            data[2] = data[0];
                        }
                        else
                        {
                            /* Read the bottom left pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                srcPixelAddress + srcTileWidth * srcPixelSize,
                                &data[2],
                                0, 0,
                                srcFormatInfo,
                                intFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));
                        }

                        /* Replicate the bottom right. */
                        data[3] = data[2];
                    }

                    else
                    {
                        /* Read the top right pixel. */
                        gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                            srcPixelAddress + srcPixelSize,
                            &data[1],
                            0, 0,
                            srcFormatInfo,
                            intFormatInfo,
                            srcBoundaryPtr,
                            dstBoundaryPtr,
                            0, 0
                            ));

                        /* Replicate vertically as well? */
                        if (source->samples.y == 1)
                        {
                            data[2] = data[0];
                            data[3] = data[1];
                        }
                        else
                        {
                            /* Read the bottom left pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                srcPixelAddress + srcTileWidth * srcPixelSize,
                                &data[2],
                                0, 0,
                                srcFormatInfo,
                                intFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));

                            /* Read the bottom right pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                srcPixelAddress + (srcTileWidth + 1) * srcPixelSize,
                                &data[3],
                                0, 0,
                                srcFormatInfo,
                                intFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));
                        }
                    }

                    /* Compute the destination. */
                    if (Target->samples.x == 1)
                    {
                        if (Target->samples.y == 1)
                        {
                            /* Average four sources. */
                            gctUINT32 dstPixel = _Average4Colors(
                                data[0], data[1], data[2], data[3]
                                );

                            /* Convert pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                &dstPixel,
                                dstPixelAddress,
                                0, 0,
                                intFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));
                        }
                        else
                        {
                            /* Average two horizontal pairs of sources. */
                            gctUINT32 dstTop = _Average2Colors(
                                data[0], data[1]
                                );

                            gctUINT32 dstBottom = _Average2Colors(
                                data[2], data[3]
                                );

                            /* Convert the top pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                &dstTop,
                                dstPixelAddress,
                                0, 0,
                                intFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));

                            /* Convert the bottom pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                &dstBottom,
                                dstPixelAddress + dstTileWidth * dstPixelSize,
                                0, 0,
                                intFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));
                        }
                    }
                    else
                    {
                        if (Target->samples.y == 1)
                        {
                            /* Average two vertical pairs of sources. */
                            gctUINT32 dstLeft = _Average2Colors(
                                data[0], data[2]
                                );

                            gctUINT32 dstRight = _Average2Colors(
                                data[1], data[3]
                                );

                            /* Convert the left pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                &dstLeft,
                                dstPixelAddress,
                                0, 0,
                                intFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));

                            /* Convert the right pixel. */
                            gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                &dstRight,
                                dstPixelAddress + dstPixelSize,
                                0, 0,
                                intFormatInfo,
                                dstFormatInfo,
                                srcBoundaryPtr,
                                dstBoundaryPtr,
                                0, 0
                                ));
                        }
                        else
                        {
                            /* Copy four sources as they are. */
                            gctUINT8 x, y;

                            for (y = 0; y < 2; y++)
                            {
                                /* Determine the vertical pixel offset. */
                                gctUINT dstVerOffset = y * dstTileWidth * dstPixelSize;

                                for (x = 0; x < 2; x++)
                                {
                                    /* Determine the horizontal pixel offset. */
                                    gctUINT dstHorOffset = x * dstPixelSize;

                                    /* Convert pixel. */
                                    gcmERR_BREAK(gcoHARDWARE_ConvertPixel(
                                        &data[x + y * 2],
                                        dstPixelAddress + dstVerOffset + dstHorOffset,
                                        0, 0,
                                        intFormatInfo,
                                        dstFormatInfo,
                                        srcBoundaryPtr,
                                        dstBoundaryPtr,
                                        0, 0
                                        ));
                                }

                                /* Error? */
                                if (gcmIS_ERROR(status))
                                {
                                    break;
                                }
                            }

                            /* Error? */
                            if (gcmIS_ERROR(status))
                            {
                                break;
                            }
                        }
                    }
                }

                /* Advance to the next column. */
                if (srcOdd || !srcFormatInfo->interleaved)
                {
                    if (--srcPixelCount == 0)
                    {
                        srcPixelAddress += srcLongOffset;
                        srcPixelCount    = srcMidPixelCount;
                    }
                    else
                    {
                        srcPixelAddress += srcShortOffset;
                    }
                }

                if (dstOdd || !dstFormatInfo->interleaved)
                {
                    if (--dstPixelCount == 0)
                    {
                        dstPixelAddress += dstLongOffset;
                        dstPixelCount    = dstMidPixelCount;
                    }
                    else
                    {
                        dstPixelAddress += dstShortOffset;
                    }
                }

                srcBoundary.x += srcStepX;
                dstBoundary.x += Target->samples.x;
            }

            /* Error? */
            if (gcmIS_ERROR(status))
            {
                break;
            }

            /* Advance to the next line. */
            if (--srcLineCount == 0)
            {
                srcLineOffset += srcLongStride;
                srcLineCount   = srcMidLineCount;
            }
            else
            {
                srcLineOffset += srcShortStride;
            }

            if (--dstLineCount == 0)
            {
                dstLineOffset += dstLongStride;
                dstLineCount   = dstMidLineCount;
            }
            else
            {
                dstLineOffset += dstShortStride;
            }

            srcBoundary.y += srcStepY;
            dstBoundary.y += Target->samples.y;
        }
    }
    while (gcvFALSE);

_Exit:
    if (Target->paddingFormat)
    {
        gctBOOL srcHasExChannel = gcvFALSE;

        if (Source->formatInfo.fmtClass == gcvFORMAT_CLASS_RGBA)
        {
            srcHasExChannel = (Source->formatInfo.u.rgba.red.width != 0 && Target->formatInfo.u.rgba.red.width == 0) ||
                              (Source->formatInfo.u.rgba.green.width != 0 && Target->formatInfo.u.rgba.green.width == 0) ||
                              (Source->formatInfo.u.rgba.blue.width != 0 && Target->formatInfo.u.rgba.blue.width == 0) ||
                              (Source->formatInfo.u.rgba.alpha.width != 0 && Target->formatInfo.u.rgba.alpha.width == 0);

            /* The source paddings are default value, is not from gcvSURF_G8R8_1_X8R8G8B8 to gcvSURF_R8_1_X8R8G8B8 and dst was full overwritten, the dst will be default value. */
            if (Source->paddingFormat && !Source->garbagePadded &&
                !(Source->format == gcvSURF_G8R8_1_X8R8G8B8 && Target->format == gcvSURF_R8_1_X8R8G8B8) &&
                TargetX == 0 && Width >= Target->rect.right / Target->samples.x &&
                TargetY == 0 && Height >= Target->rect.bottom / Target->samples.y)
            {
                Target->garbagePadded = gcvFALSE;
            }

            /* The source has extra channel:
            ** 1, Source is not paddingformat,the dst may be garbage value.
            ** 2, source is paddingformat but not default value, the dst will be garbage value.
            */
            if (srcHasExChannel && (!Source->paddingFormat || Source->garbagePadded))
            {
                Target->garbagePadded = gcvTRUE;
            }
        }
        else
        {
            Target->garbagePadded = gcvTRUE;
        }
    }

    /* Flush CPU cache after writing */
    gcmVERIFY_OK(
        gcoSURF_NODE_Cache(&Target->node,
                           Target->node.logical,
                           Target->size,
                           gcvCACHE_CLEAN));

#if gcdDUMP
    if (Target->node.pool == gcvPOOL_USER)
    {
        gctUINT offset, bytes, size;
        gctUINT step;

        if (source)
        {
            if (source->type == gcvSURF_BITMAP)
            {
                int bpp = srcFormatInfo->bitsPerPixel / 8;
                offset  = SourceY * source->stride + SourceX * bpp;
                size    = Width * gcmABS(Height) * bpp;
                bytes   = Width * bpp;
                step    = source->stride;

                if (bytes == step)
                {
                    bytes *= gcmABS(Height);
                    step   = 0;
                }
            }
            else
            {
                offset = 0;
                size   = source->stride * source->alignedHeight;
                bytes  = size;
                step   = 0;
            }

            while (size > 0)
            {
                gcmDUMP_BUFFER(gcvNULL,
                               "verify",
                               source->node.physical,
                               source->node.logical,
                               offset,
                               bytes);

                offset += step;
                size   -= bytes;
            }
        }
    }
    else
    {
        /* verify the source if exist */
        if (Source)
        {
            gcmDUMP_BUFFER(gcvNULL,
                           "verify",
                           Source->node.physical,
                           Source->node.logical,
                           0,
                           Source->sliceSize);
        }

        /* upload destination buffer to avoid uninitialized data */
        gcmDUMP_BUFFER(gcvNULL,
                       "memory",
                       Target->node.physical,
                       Target->node.logical,
                       0,
                       Target->sliceSize);
    }
#endif

OnError:
    if ((Hardware != gcvNULL)
     && (memory != gcvNULL))
    {
        /* Unlock the temporary buffer. */
        gcmVERIFY_OK(
            gcoHARDWARE_Unlock(&Hardware->tempBuffer.node,
                               Hardware->tempBuffer.type));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_FlushPipe
**
**  Flush the current graphics pipe.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_FlushPipe(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;
#if (gcdENABLE_3D || gcdENABLE_2D)
    gctUINT32 flush;
    gctBOOL flushFixed;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    if (Hardware->currentPipe == 0x1)
    {
        /* Flush 2D cache. */
        flush = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
    }
    else
    {
        /* Flush Z and Color caches. */
        flush = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) |
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));
    }

    /* RTL bug in older chips - flush leaks the next state.
       There is no actial bit for the fix. */
    flushFixed = ((((gctUINT32) (Hardware->config->chipMinorFeatures)) >> (0 ? 31:31) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1)))))));

    /* FLUSH_HACK */
    flushFixed = gcvFALSE;

    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

        {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, flush );    gcmENDSTATEBATCH_NEW(reserve, memory);};

        if (!flushFixed)
        {
            {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, flush );    gcmENDSTATEBATCH_NEW(reserve, memory);};
        }

#if gcdMULTI_GPU
    flush = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0594, flush );    gcmENDSTATEBATCH_NEW(reserve, memory);};
#endif

    /* Validate the state Hardware. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    stateDelta = stateDelta; /* Keep the compiler happy. */

#if gcdENABLE_3D
    if (Hardware->config->chipModel == gcv700
#if gcdMULTI_GPU
        || 1
#endif
       )
    {
        /* Flush the L2 cache. */
        gcmONERROR(gcoHARDWARE_FlushL2Cache(Hardware, Memory));
    }

    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
        gcvHOW_SEMAPHORE,
        Memory
        ));
#endif

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
#endif  /*(gcdENABLE_3D || gcdENABLE_2D) */
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHARDWARE_Semaphore
**
**  Send sempahore and stall until sempahore is signalled.
**
**  INPUT:
**
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gceWHERE From
**          Semaphore source.
**
**      GCHWERE To
**          Sempahore destination.
**
**      gceHOW How
**          What to do.  Can be a one of the following values:
**
**              gcvHOW_SEMAPHORE            Send sempahore.
**              gcvHOW_STALL                Stall.
**              gcvHOW_SEMAPHORE_STALL  Send semaphore and stall.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_Semaphore(
    IN gcoHARDWARE Hardware,
    IN gceWHERE From,
    IN gceWHERE To,
    IN gceHOW How,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status;
    gctBOOL semaphore, stall;
    gctUINT32 source, destination;

    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x From=%d To=%d How=%d",
                  Hardware, From, To, How);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    semaphore = (How == gcvHOW_SEMAPHORE) || (How == gcvHOW_SEMAPHORE_STALL);
    stall     = (How == gcvHOW_STALL)     || (How == gcvHOW_SEMAPHORE_STALL);

    /* Special case for RASTER to PIEL_ENGINE semaphores. */
    if ((From == gcvWHERE_RASTER) && (To == gcvWHERE_PIXEL))
    {
        if (How == gcvHOW_SEMAPHORE)
        {
            /* Set flag so we can issue a semaphore/stall when required. */
            Hardware->stallPrimitive = gcvTRUE;

            /* Success. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;
        }
        if (How == gcvHOW_STALL)
        {
            /* Make sure we do a semaphore/stall here. */
            semaphore = gcvTRUE;
            stall     = gcvTRUE;
        }
    }

    switch (From)
    {
    case gcvWHERE_COMMAND:
        source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
        break;

    case gcvWHERE_RASTER:
        source = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x05 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));

        /* We need to stall on the next primitive if this is only a
           semaphore. */
        Hardware->stallPrimitive = (How == gcvHOW_SEMAPHORE);
        break;

    default:
        gcmASSERT(gcvFALSE);
        gcmFOOTER_ARG("status=%d", gcvSTATUS_INVALID_ARGUMENT);
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    gcmASSERT(To == gcvWHERE_PIXEL);
    destination = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    if (semaphore)
    {
        /* Send sempahore token. */
        memory[0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        memory[1]
            = source
            | destination;

        /* Dump the semaphore. */
        gcmDUMP(gcvNULL, "#[semaphore 0x%08X 0x%08X]", source, destination);

        /* Point to stall command is required. */
        memory += 2;
    }

    if (stall)
    {
        if (From == gcvWHERE_COMMAND)
        {
            /* Stall command processor until semaphore is signalled. */
            memory[0]
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
        }
        else
        {
            /* Send stall token. */
            memory[0]
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0F00) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
        }

        memory[1]
            = source
            | destination;

        memory += 2;
        /* Dump the stall. */
        gcmDUMP(gcvNULL, "#[stall 0x%08X 0x%08X]", source, destination);
    }

    stateDelta = stateDelta; /* make compiler happy */

    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_PauseTileStatus(
    IN gcoHARDWARE Hardware,
    IN gceTILE_STATUS_CONTROL Control
    )
{
    gceSTATUS status;
    gctUINT32 config;

    gcmHEADER_ARG("Hardware=0x%x Control=%d", Hardware, Control);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Determine configuration. */
    config = (Control == gcvTILE_STATUS_PAUSE) ? 0
                                               : Hardware->memoryConfig;

    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

    gcmONERROR(_LoadStates(
        Hardware, 0x0595, gcvFALSE, 1, 0, &config
        ));

    Hardware->paused = (Control == gcvTILE_STATUS_PAUSE);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the error. */
    gcmFOOTER();
    return status;
}


#if gcdMULTI_GPU_AFFINITY
gceSTATUS
gcoHARDWARE_GetCurrentAPI(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT_PTR Api
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Hardware=0x%x Api=0x%x", Hardware, Api);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    *Api = Hardware->api;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_DisableHardwareTileStatus
**
**  Disable tile status hardware setting
**
**  INPUT:
**
**      IN gcoHARDWARE Hardware,
**          Hardware object
**
**      IN gceTILESTATUS_TYPE Type,
**          depth or color will be set
**
**      IN gctUINT  RtIndex
**          For color slot, which RT index will be set
**
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_DisableHardwareTileStatus(
    IN gcoHARDWARE Hardware,
    IN gceTILESTATUS_TYPE Type,
    IN gctUINT  RtIndex
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Type=%d RtIndex=%d",
                  Hardware, Type, RtIndex);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if ((Type == gcvTILESTATUS_DEPTH) ||
        (!Hardware->features[gcvFEATURE_MRT_TILE_STATUS_BUFFER]))
    {
       gcmONERROR(_DisableTileStatus(Hardware, Type));
    }
    else
    {
       gcmONERROR(_DisableTileStatusMRT(Hardware, Type, RtIndex));
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static
gceSTATUS
_AutoSetColorCompression(
    IN gcoHARDWARE Hardware
    )
{
    gctUINT i;
    gctBOOL compression = gcvFALSE;

    for (i = 0; i < Hardware->config->renderTargets; i++)
    {
        gcsSURF_INFO_PTR surface = Hardware->colorStates.target[i].surface;
        if ((surface != gcvNULL) &&
            (surface->tileStatusDisabled == gcvFALSE) &&
            (surface->tileStatusNode.pool != gcvPOOL_UNKNOWN))
        {
            compression |= surface->compressed;
        }
    }

    if (Hardware->colorStates.colorCompression != compression)
    {
        Hardware->colorStates.colorCompression = compression;
        Hardware->colorConfigDirty = gcvTRUE;
    }

    return gcvSTATUS_OK;
}


static gceSTATUS
_EnableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus
    )
{
    gceSTATUS status;
    gctBOOL autoDisable;
    gctBOOL hierarchicalZ = gcvFALSE;
    gcsSURF_FORMAT_INFO_PTR info;
    gctUINT32 tileCount;
    gctUINT32 physicalBaseAddress;
    gctBOOL   halti2Support;
    gctUINT32 hzNodeSize;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x TileStatusAddress=0x%08x "
                  "HzTileStatus=0x%x",
                  Hardware, Surface, TileStatusAddress, HzTileStatus);


    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    if (Surface->tileStatusDisabled || (TileStatusAddress == 0))
    {
        /* This surface has the tile status disabled - disable tile status. */
        status = _DisableTileStatus(Hardware, (Surface->type == gcvSURF_DEPTH)? gcvTILESTATUS_DEPTH :
                                                                                gcvTILESTATUS_COLOR);

        gcmFOOTER();
        return status;
    }

    halti2Support = Hardware->features[gcvFEATURE_HALTI2];

    /* Verify that the surface is locked. */
    gcmVERIFY_LOCK(Surface);

    /* Convert the format into bits per pixel. */
    info = &Surface->formatInfo;

    if ((info->bitsPerPixel  == 16)
    &&  (Hardware->config->chipModel    == gcv500)
    &&  (Hardware->config->chipRevision < 3)
    )
    {
        /* 32-bytes per tile. */
        tileCount = Surface->size / 32;
    }
    else
    {
        /* 64-bytes per tile. */
        tileCount = Surface->size / 64;
    }

    autoDisable = ((Surface->compressed == gcvFALSE) &&
                   ((Hardware->features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT_WIDTH]) ||
                    ((Hardware->features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT]) &&
                     (tileCount < (1 << 20))
                    )
                   )
                  );

    /* Determine the common (depth and pixel) reserve size. */
    reserveSize

        /* FLUSH_HACK */
        = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2
        /* Memory configuration register. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Fast clear registers. */
        + gcmALIGN((1 + 3) * gcmSIZEOF(gctUINT32), 8);

    if (Surface->type == gcvSURF_DEPTH)
    {
        if (Surface != Hardware->depthStates.surface)
        {
            /* Depth buffer is not current - no need to enable. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;
        }

        /* Cannot auto disable TS for compressed case, because the TS buffer
        ** contains info for compress.
        */
        if (Surface->compressed)
        {
            autoDisable = gcvFALSE;
        }

        if (autoDisable)
        {
            /* Add a state for surface auto disable counter. */
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }

        hierarchicalZ = Surface->hzDisabled
                      ? gcvFALSE
                      : ((((gctUINT32) (Hardware->config->chipMinorFeatures)) >> (0 ? 27:27) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))) == (0x1  & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1)))))));

        if (hierarchicalZ)
        {
            /* Add three states for hierarchical Z. */
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 3, 8);
        }
    }
    else
    {
        if (Surface != Hardware->colorStates.target[0].surface)
        {
            /* Render target is not current - no need to enable. */
            gcmFOOTER_NO();
            return gcvSTATUS_OK;
        }

        /* Cannot auto disable TS for compressed case, because the TS buffer
        ** contains info for compress.
        */
        if (Surface->compressed)
        {
            autoDisable = gcvFALSE;
        }

        if (autoDisable)
        {
            /* Add a state for surface auto disable counter. */
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }

        if (halti2Support)
        {
            /* For upper clear color */
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }
    }

#if gcdSECURE_USER
    physicalBaseAddress = 0;
#else
    if (Hardware->features[gcvFEATURE_MC20])
    {
        physicalBaseAddress = 0;
    }
    else
    {
        gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &physicalBaseAddress));
    }
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Check for depth surfaces. */
    if (Surface->type == gcvSURF_DEPTH)
    {
        /* Flush the depth cache. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

        /* FLUSH_HACK */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

        /* Enable fast clear. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Set depth format. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) ((gctUINT32) (info->bitsPerPixel == 16) & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        /* Enable compression or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) ((gctUINT32) (Surface->compressed) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));

        /* Automatically disable fast clear or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (autoDisable) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        if (autoDisable)
        {
            /* Program surface auto disable counter. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x059C, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x059C) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x059C, tileCount );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Hierarchical Z. */
        if (hierarchicalZ)
        {
            gctBOOL hasHz = (HzTileStatus->pool != gcvPOOL_UNKNOWN);

            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) ((gctUINT32) (hasHz) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));

            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) ((gctUINT32) (hasHz) & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));

            if (Hardware->features[gcvFEATURE_NEW_RA])
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x038A, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x038A) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x038A, HzTileStatus->physical + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};
            }
            else
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A9, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A9) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05A9, HzTileStatus->physical + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};
            }

            gcmSAFECASTSIZET(hzNodeSize, Surface->hzNode.size / 16);

            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AB, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AB) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AB, hzNodeSize );     gcmENDSTATEBATCH(reserve, memory);};

            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AA, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AA) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AA, Surface->fcValueHz );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Program fast clear registers. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)3  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0599, 3 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0599) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            /* Program tile status base address register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0599,
                TileStatusAddress + physicalBaseAddress
                );

            /* Program surface base address register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x059A,
                Surface->node.physical + physicalBaseAddress
                );

            /* Program clear value register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x059B,
                Surface->fcValue
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );
    }

    /* Render target surfaces. */
    else
    {
        /* Flush the color cache. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );gcmENDSTATEBATCH(reserve, memory);};

        /* FLUSH_HACK */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

        /* Enable fast clear or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

            /* Turn on color compression when in MSAA mode. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (Surface->compressed) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        if (Surface->compressed)
        {

            if (Surface->compressFormat == -1)
            {
                gcmFATAL("color pipe is taking invalid compression format for surf=0x%x, format=%d",
                          Surface, Surface->format);
            }
            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Surface->compressFormat) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
        }

        /* Automatically disable fast clear or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) ((gctUINT32) (autoDisable) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));

        if (halti2Support)
        {
            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) ((info->bitsPerPixel == 64)) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));
        }

        if (autoDisable)
        {
            /* Program surface auto disable counter. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x059D, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x059D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x059D, tileCount );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Program fast clear registers. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)3  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 3 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            /* Program tile status base address register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0596,
                TileStatusAddress + physicalBaseAddress
                );

            /* Program surface base address register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0597,
                Surface->node.physical + physicalBaseAddress
                );

            /* Program clear value register. */
            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0598,
                Surface->fcValue
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );

        if (halti2Support)
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AF, Surface->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};
        }

        _AutoSetColorCompression(Hardware);
    }

    /* Program memory configuration register. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, Hardware->memoryConfig );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
        gcvHOW_SEMAPHORE,
        gcvNULL
        ));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
static gceSTATUS
_EnableTileStatusMRT(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus,
    IN gctUINT  RtIndex
    )
{
    gceSTATUS status;
    gctBOOL autoDisable;
    gcsSURF_FORMAT_INFO_PTR info;
    gctUINT32 tileCount;
    gctUINT32 physicalBaseAddress;
    gctBOOL   halti2Support;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x TileStatusAddress=0x%08x "
                  "HzTileStatus=0x%x RtIndex=%d",
                  Hardware, Surface, TileStatusAddress, HzTileStatus, RtIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    gcmASSERT(Hardware->features[gcvFEATURE_MRT_TILE_STATUS_BUFFER]);
    gcmASSERT(RtIndex < Hardware->config->renderTargets);
    gcmASSERT(Surface->type != gcvSURF_DEPTH);

    if (Surface->tileStatusDisabled || (TileStatusAddress == 0))
    {
        /* This surface has the tile status disabled - disable tile status. */
        status = _DisableTileStatusMRT(Hardware, gcvTILESTATUS_COLOR, RtIndex);

        gcmFOOTER();
        return status;
    }

    halti2Support = Hardware->features[gcvFEATURE_HALTI2];

    /* Verify that the surface is locked. */
    gcmVERIFY_LOCK(Surface);

    /* Convert the format into bits per pixel. */
    info = &Surface->formatInfo;

    if ((info->bitsPerPixel  == 16)
    &&  (Hardware->config->chipModel    == gcv500)
    &&  (Hardware->config->chipRevision < 3)
    )
    {
        /* 32-bytes per tile. */
        tileCount = Surface->size / 32;
    }
    else
    {
        /* 64-bytes per tile. */
        tileCount = Surface->size / 64;
    }

    autoDisable = ((Surface->compressed == gcvFALSE) &&
                   ((Hardware->features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT_WIDTH]) ||
                    ((Hardware->features[gcvFEATURE_CORRECT_AUTO_DISABLE_COUNT]) &&
                     (tileCount < (1 << 20))
                    )
                   )
                  );
    /* Determine the common (depth and pixel) reserve size. */
    reserveSize

        /* FLUSH_HACK */
        = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2

        /* Memory configuration register. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Fast clear registers. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 3;

    if (Surface != Hardware->colorStates.target[RtIndex].surface)
    {
        /* Render target is not current - no need to enable. */
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if (autoDisable)
    {
        /* Add a state for surface auto disable counter. */
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    }

    if (halti2Support)
    {
        /* For upper clear color */
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    }


#if gcdSECURE_USER
    physicalBaseAddress = 0;
#else
    if (Hardware->features[gcvFEATURE_MC20])
    {
        physicalBaseAddress = 0;
    }
    else
    {
        gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &physicalBaseAddress));
    }
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Flush the color cache. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) );gcmENDSTATEBATCH(reserve, memory);};

    /* FLUSH_HACK */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0xFFFF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0xFFFF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0xFFFF, 0);gcmENDSTATEBATCH(reserve, memory);};

    /* Enable fast clear or not. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Turn on color compression when in MSAA mode. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) ((gctUINT32) (Surface->compressed) & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)));

    if (Surface->compressed)
    {
        Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:3) - (0 ? 6:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:3) - (0 ? 6:3) + 1))))))) << (0 ? 6:3))) | (((gctUINT32) ((gctUINT32) (Surface->compressFormat) & ((gctUINT32) ((((1 ? 6:3) - (0 ? 6:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:3) - (0 ? 6:3) + 1))))))) << (0 ? 6:3)));
    }

    /* Automatically disable fast clear or not. */
    Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (autoDisable) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

    if (halti2Support)
    {
        Hardware->memoryConfigMRT[RtIndex] = ((((gctUINT32) (Hardware->memoryConfigMRT[RtIndex])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) ((info->bitsPerPixel == 64)) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));
    }

    /*
    ** 1. 0x0690 slot 0 is uesless on RTL impelmetation.
    ** 2. we need keep Hardware->memoryConfig always has correct information for RT0,
    **    as it's combined with depth information.
    */
    if (RtIndex == 0)
    {
            /* Enable fast clear or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

            /* Turn on color compression when in MSAA mode. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) ((gctUINT32) (Surface->compressed) & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        if (Surface->compressed)
        {
            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (Surface->compressFormat) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)));
        }

        /* Automatically disable fast clear or not. */
        Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) ((gctUINT32) (autoDisable) & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));

        if (halti2Support)
        {
            Hardware->memoryConfig = ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) ((gctUINT32) ((info->bitsPerPixel == 64)) & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));
        }

        if (autoDisable)
        {
            /* Program surface auto disable counter. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x059D, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x059D) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x059D, tileCount );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Program tile status base address register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0596, TileStatusAddress + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};

        /* Program surface base address register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0597, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0597) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0597, Surface->node.physical + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};

        /* Program clear value register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0598, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0598) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0598, Surface->fcValue );     gcmENDSTATEBATCH(reserve, memory);};


        if (halti2Support)
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AF, Surface->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Program memory configuration register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, Hardware->memoryConfig );     gcmENDSTATEBATCH(reserve, memory);};

    }
    else
    {
        if (autoDisable)
        {
            /* Program surface auto disable counter. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0690 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0690 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0690 + RtIndex, tileCount );     gcmENDSTATEBATCH(reserve, memory);};
        }

        /* Program tile status base address register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05F0 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05F0 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05F0 + RtIndex, TileStatusAddress + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};

        /* Program surface base address register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05F8 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05F8 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05F8 + RtIndex, Surface->node.physical + physicalBaseAddress );     gcmENDSTATEBATCH(reserve, memory);};

        /* Program clear value register. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0680 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0680 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0680 + RtIndex, Surface->fcValue );     gcmENDSTATEBATCH(reserve, memory);};

        if (halti2Support)
        {
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0688 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0688 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0688 + RtIndex, Surface->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};
        }

         /* Program memory configuration register. */
         {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05E8 + RtIndex, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05E8 + RtIndex) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05E8 + RtIndex, Hardware->memoryConfigMRT[RtIndex] );     gcmENDSTATEBATCH(reserve, memory);};

    }
    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
        gcvHOW_SEMAPHORE,
        gcvNULL
        ));

    _AutoSetColorCompression(Hardware);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_EnableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus,
    IN gctUINT  RtIndex
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x TileStatusAddress=0x%08x "
                  "HzTileStatus=0x%x RtIndex=%d",
                  Hardware, Surface, TileStatusAddress, HzTileStatus, RtIndex);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if ((Surface->type == gcvSURF_DEPTH) ||
        (!Hardware->features[gcvFEATURE_MRT_TILE_STATUS_BUFFER]))
    {
        gcmONERROR(_EnableTileStatus(Hardware, Surface, TileStatusAddress, HzTileStatus));
    }
    else
    {
        gcmONERROR(_EnableTileStatusMRT(Hardware, Surface, TileStatusAddress, HzTileStatus, RtIndex));
    }

OnError:

    gcmFOOTER_NO();
    return status;

}

gceSTATUS
gcoHARDWARE_FillFromTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    )
{
    gceSTATUS status;
    gctUINT32 physicalBaseAddress, tileCount, srcStride;
    gctBOOL halti2Support;
    gcsSURF_INFO_PTR current;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x", Hardware, Surface);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    current = Hardware->colorStates.target[0].surface;

#if gcdSECURE_USER
    physicalBaseAddress = 0;
#else
    /* Get physical base address. */
    if (Hardware->features[gcvFEATURE_MC20])
    {
        physicalBaseAddress = 0;
    }
    else
    {
        gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &physicalBaseAddress));
    }
#endif

    halti2Support = Hardware->features[gcvFEATURE_HALTI2];

    /* Program TileFiller. */
    if ((Surface->node.size & 0x3fff) == 0)
    {
        tileCount = (gctUINT32)(Surface->node.size / 64);
    }
    else
    {
        tileCount = 0;
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Determine the source stride. */
    srcStride = (Surface->tiling == gcvLINEAR)

              /* Linear. */
              ? Surface->stride

              /* Tiled. */
              : ((((gctUINT32) (Surface->stride * 4)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (Surface->superTiled) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));

    if ((Hardware->config->pixelPipes > 1)
        && (Surface->tiling & gcvTILING_SPLIT_BUFFER))
    {
        srcStride = ((((gctUINT32) (srcStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 30:30) - (0 ? 30:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:30) - (0 ? 30:30) + 1))))))) << (0 ? 30:30)));
    }

    /* Determine Fast MSAA mode. */
    if (Hardware->features[gcvFEATURE_FAST_MSAA] ||
        Hardware->features[gcvFEATURE_SMALL_MSAA])
    {
        if ((Surface->samples.x > 1)
         && (Surface->samples.y > 1)
         && ((Surface->type == gcvSURF_RENDER_TARGET)
            || (Surface->type == gcvSURF_DEPTH)))
        {
            /* Tile filler doesnt support MSAA surface. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        else
        {
            srcStride = ((((gctUINT32) (srcStride)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
        }
    }

    /*
    ** Adjust MSAA setting for tile filler.
    */
    gcmONERROR(gcoHARDWARE_AdjustCacheMode(Hardware,Surface));

#if !gcdMULTI_GPU
        gcmONERROR(gcoHARDWARE_Semaphore(
            Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
            gcvHOW_SEMAPHORE_STALL,
            gcvNULL
            ));
#endif


    /* Determine the size of the buffer to reserve. */
    reserveSize

        /* Tile status states. */
        = gcmALIGN((1 + 3) * gcmSIZEOF(gctUINT32), 8)

        /* Source Stride. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Trigger state. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);


    if (halti2Support)
    {
        /*upper 32bit clear color and set filler64*/
        reserveSize += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
    }

    if ((Surface != current) &&
        (current != gcvNULL) &&
        (current->tileStatusNode.pool != gcvPOOL_UNKNOWN))
    {
        reserveSize +=
        /* Restore Tile status states. */
        gcmALIGN((1 + 3) * gcmSIZEOF(gctUINT32), 8);

        if (halti2Support)
        {
            /* restore upper 32bit clear color setting */
            reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
        }
    }

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        reserveSize += (2 + 18 + 2) * 2 * gcmSIZEOF(gctUINT32);
    }
#endif

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Invalidate the tile status cache. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Sync the GPUs. */
        gcmONERROR(gcoHARDWARE_MultiGPUSync(Hardware, gcvFALSE, &memory));

        /* Select core 3D 0 to do the tile fill operation. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };
    }
#endif


    /* Reprogram tile status. */
    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)3  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 3 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0596,
            Surface->tileStatusNode.physical + physicalBaseAddress
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0597,
            Surface->node.physical + physicalBaseAddress
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0598,
            Surface->fcValue
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    if (halti2Support)
    {
        gctUINT32 rsConfig = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1))))))) << (0 ? 28:28)));
        /* Set upper color data. */
        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AF, Surface->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};

        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05A8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05A8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x05A8, rsConfig );gcmENDSTATEBATCH(reserve, memory);};

    }

    /* Set source and destination stride. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0583, srcStride );gcmENDSTATEBATCH(reserve, memory);};

    /* Trigger the fill. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AC, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AC) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x05AC, tileCount );gcmENDSTATEBATCH(reserve, memory);};

    /* Restore tile status. */
    if ((Surface != current) &&
        (current != gcvNULL) &&
        (current->tileStatusNode.pool != gcvPOOL_UNKNOWN))
    {
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)3  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 3 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (3 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0596,
                current->tileStatusNode.physical + physicalBaseAddress
                );

            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0597,
                current->node.physical + physicalBaseAddress
                );

            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0598,
                current->fcValue
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );

        if (halti2Support)
        {
            /* Set upper color data. */
            {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05AF, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AF) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x05AF, current->fcValueUpper );     gcmENDSTATEBATCH(reserve, memory);};
        }

    }

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Enable all 3D GPUs. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };

        /* Invalidate the tile status cache. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Sync the GPUs. */
        gcmONERROR(gcoHARDWARE_MultiGPUSync(Hardware, gcvFALSE, &memory));
    }
#endif

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

#if !gcdMULTI_GPU
    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
        gcvHOW_SEMAPHORE,
        gcvNULL
        ));
#endif

    /* All pixels have been resolved. */
    Surface->dirty = gcvFALSE;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_FlushTileStatusCache(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 physical[3] = {0};
    gctINT stride;
    gctPOINTER logical[3] = {gcvNULL};
    gctUINT32 format;
    gctUINT32 physicalBaseAddress;
    gcsPOINT rectSize;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);
    /* This function can't work with MRT tile status buffer */
    gcmASSERT(Hardware->features[gcvFEATURE_MRT_TILE_STATUS_BUFFER] == gcvFALSE);

    /* Get physical base address. */
    if (Hardware->features[gcvFEATURE_MC20])
    {
        physicalBaseAddress = 0;
    }
    else
    {
        gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &physicalBaseAddress));
    }

    if (Hardware->tempSurface == gcvNULL)
    {
        /* Construct a temporary surface. */
        gcmONERROR(gcoSURF_Construct(gcvNULL,
                                     64, 64, 1,
                                     gcvSURF_RENDER_TARGET,
                                     gcvSURF_A8R8G8B8,
                                     gcvPOOL_DEFAULT,
                                     &Hardware->tempSurface));
    }

    /* Set rect size as 16x8. */
    rectSize.x = 16;
    rectSize.y =  8;

    /* Lock the temporary surface. */
    gcmONERROR(gcoSURF_Lock(Hardware->tempSurface, physical, logical));

    /* Get the stride of the temporary surface. */
    gcmONERROR(gcoSURF_GetAlignedSize(Hardware->tempSurface,
                                      gcvNULL, gcvNULL,
                                      &stride));

    /* Convert the source format. */
    gcmONERROR(_ConvertResolveFormat(Hardware,
                                     Hardware->tempSurface->info.format,
                                     &format,
                                     gcvNULL,
                                     gcvNULL));

    /* Determine the size of the buffer to reserve. */
    reserveSize

        /* Color depth cache flush states. */
        = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Tile status states. */
        + gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

        /* Tile status cache flush states. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8)

        /* Ungrouped resolve states. */
        + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 6;

    if (Hardware->config->pixelPipes == 2)
    {
        reserveSize

            /* Source address states. */
            += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

            /* Target address states. */
            +  gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

            /* Offset address states. */
            +  gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
    }
    else
    {
        reserveSize

            /* Source and target address states. */
            += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2;

        if (Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
        {
            reserveSize
                /* Source and target address states. */
                += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8) * 2;

        }
    }

    if ((Hardware->colorStates.target[0].surface != gcvNULL) &&
        (Hardware->colorStates.target[0].surface->tileStatusNode.pool != gcvPOOL_UNKNOWN))
    {
        /* Add tile status states. */
        reserveSize += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8);
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    /* Flush the depth/color cache of current RT to update ts cache */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0E03, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

    /* Reprogram tile status. */
    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0596,
            Hardware->tempSurface->info.tileStatusNode.physical + physicalBaseAddress
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0597,
            Hardware->tempSurface->info.node.physical + physicalBaseAddress
            );

        gcmSETFILLER(
            reserve, memory
            );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    /* Flush the tile status. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

    /* Append RESOLVE_CONFIG state. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0581, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:5) - (0 ? 6:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:5) - (0 ? 6:5) + 1))))))) << (0 ? 6:5))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) );gcmENDSTATEBATCH(reserve, memory);};

    /* Set source and destination stride. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0583, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0583) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0583, stride << 2 );gcmENDSTATEBATCH(reserve, memory);};

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0585, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0585) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0585, stride << 2 );gcmENDSTATEBATCH(reserve, memory);};

    /* TODO: for singleBuffer+MultiPipe case, no need to program the 2nd address */
    if (Hardware->config->pixelPipes == 2)
    {
        gctUINT32 physical2 = physical[0] + Hardware->tempSurface->info.bottomBufferOffset;

        /* Append RESOLVE_SOURCE addresses. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B0, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05B0,
                physical[0]
                );

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05B0 + 1,
                physical2
                );

            gcmSETFILLER(
                reserve, memory
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );

        /* Append RESOLVE_DESTINATION addresses. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05B8,
                physical[0]
                );

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05B8 + 1,
                physical2
                );

            gcmSETFILLER(
                reserve, memory
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );

        /* Append offset. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05C0, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05C0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05C0,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16)))
                );

            gcmSETCTRLSTATE(
                stateDelta, reserve, memory, 0x05C0 + 1,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16))) | (((gctUINT32) ((gctUINT32) (rectSize.y) & ((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16)))
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
        /* Append RESOLVE_SOURCE commands. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0582, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0582) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0582, physical[0] );gcmENDSTATEBATCH(reserve, memory);};

        /* Append RESOLVE_DESTINATION commands. */
        {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0584, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0584) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0584, physical[0] );gcmENDSTATEBATCH(reserve, memory);};

        if (Hardware->features[gcvFEATURE_SINGLE_PIPE_HALTI1])
        {
            /* Append RESOLVE_SOURCE commands. */
            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B0, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x05B0, physical[0] );gcmENDSTATEBATCH(reserve, memory);};

            /* Append RESOLVE_DESTINATION commands. */
            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x05B8, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05B8) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x05B8, physical[0] );gcmENDSTATEBATCH(reserve, memory);};
        }
    }

    /* Program the window size. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0588, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0588) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0588, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (rectSize.x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (rectSize.y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );gcmENDSTATEBATCH(reserve, memory);};

    /* Make sure it is a resolve and not a clear. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x058F, 0 );gcmENDSTATEBATCH(reserve, memory);};

    /* Start the resolve. */
    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0580, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0580) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0580, 0xBADABEEB );gcmENDSTATEBATCH(reserve, memory);};

    if ((Hardware->colorStates.target[0].surface != gcvNULL) &&
        (Hardware->colorStates.target[0].surface->tileStatusNode.pool != gcvPOOL_UNKNOWN))
    {

        /* Reprogram tile status. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0596,
                Hardware->colorStates.target[0].surface->tileStatusNode.physical + physicalBaseAddress
                );

            gcmSETSTATEDATA(
                stateDelta, reserve, memory, gcvFALSE, 0x0597,
                Hardware->colorStates.target[0].surface->node.physical + physicalBaseAddress
                );

            gcmSETFILLER(
                reserve, memory
                );

        gcmENDSTATEBATCH(
            reserve, memory
            );
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL,
        gcvHOW_SEMAPHORE,
        gcvNULL
        ));

OnError:
    if (logical[0] != gcvNULL)
    {
        /* Unlock the temporary surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(Hardware->tempSurface, logical[0]));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}



gceSTATUS
_FlushAndDisableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Decompress,
    IN gctBOOL Disable
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL flushed            = gcvFALSE;
    gctBOOL flushedTileStatus  = gcvFALSE;
    gctBOOL switchedTileStatus = gcvFALSE;
    gcsSURF_INFO_PTR saved = gcvNULL;
    gctPOINTER logical[3] = {gcvNULL};
    gctBOOL current = gcvTRUE;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Decompress=%d, Disable=%d",
                  Hardware, Surface, Decompress, Disable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);

    /* If tilestatus was already disabled */
    if (Surface->tileStatusDisabled ||
        (Surface->tileStatusNode.pool == gcvPOOL_UNKNOWN))
    {
        goto OnError;
    }

    /* Nothing to do when there is no fast clear. */
    if (Hardware->features[gcvFEATURE_FAST_CLEAR] == gcvFALSE
        /* Already in a flush - don't recurse! */
    ||  Hardware->inFlush
        /* Need to be in 3D pipe. */
    ||  (Hardware->currentPipe != 0x0)
        /* Skip this if the tile status got paused. */
    ||  Hardware->paused
    )
    {
        goto OnError;
    }
    /* Mark that we are in a flush. */
    Hardware->inFlush = gcvTRUE;

    if (Surface->type == gcvSURF_DEPTH)
    {
        if (Surface != Hardware->depthStates.surface)
        {
            current = gcvFALSE;
        }
    }
    else
    {
        if (Hardware->colorStates.target[0].surface != Surface)
        {
            current = gcvFALSE;
        }
    }

    /* The surface has not yet been de-tiled and is not the current one,
    ** enable its tile status buffer first.
    */
    if (Decompress)
    {
        if (Surface->type == gcvSURF_DEPTH)
        {
            if (Surface != Hardware->depthStates.surface)
            {
                saved = Hardware->depthStates.surface;
                Hardware->depthStates.surface = Surface;

                gcmONERROR(
                    gcoHARDWARE_EnableTileStatus(
                    Hardware,
                    Surface,
                    Surface->tileStatusNode.physical,
                    &Surface->hzTileStatusNode,
                        0));

                switchedTileStatus = gcvTRUE;
            }
        }
        else
        {
            if (Hardware->colorStates.target[0].surface != Surface)
            {
                saved = Hardware->colorStates.target[0].surface;

                Hardware->colorStates.target[0].surface = Surface;

                gcmONERROR(
                    gcoHARDWARE_EnableTileStatus(
                        Hardware,
                        Hardware->colorStates.target[0].surface,
                        Surface->tileStatusNode.physical,
                        &Surface->hzTileStatusNode,
                        0));
                switchedTileStatus = gcvTRUE;
            }
        }
    }

    /* If the hardware doesn't have a real flush and the cache is marked as
    ** dirty, we do need to do a partial resolve to make sure the dirty
    ** cache lines are pushed out. */
    if (Hardware->features[gcvFEATURE_FAST_CLEAR_FLUSH] == gcvFALSE)
    {
        /* Flush the cache the hard way. */
        gcsPOINT origin, size;

        gctBOOL fastClearC = ((((gctUINT32) (Hardware->memoryConfig)) >> (0 ? 1:1) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1)))))));

        /* Setup the origin. */
        origin.x = origin.y = 0;

        /* Setup the size. */
        size.x = 16;
        size.y = 16 * 4 * 2;
        /* Below code can't work with MRT tile status buffer */
        gcmASSERT(Hardware->features[gcvFEATURE_MRT_TILE_STATUS_BUFFER] == gcvFALSE);

        if ((Decompress) &&
            (Surface == Hardware->colorStates.target[0].surface) &&
            (Hardware->colorStates.target[0].surface->stride >= 256) &&
            (Hardware->colorStates.target[0].surface->alignedHeight < (gctUINT) size.y))
        {
            size.x     = Hardware->colorStates.target[0].surface->alignedWidth;
            size.y     = Hardware->colorStates.target[0].surface->alignedHeight;
            Decompress = gcvFALSE;
        }
        /* Flush the pipe. */
        gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

        /* The pipe has been flushed. */
        flushed = gcvTRUE;

        if ((Hardware->colorStates.target[0].surface == gcvNULL)                        ||
            (Hardware->colorStates.target[0].surface->stride < 256)                     ||
            (Hardware->colorStates.target[0].surface->alignedHeight < (gctUINT) size.y) ||
            (!fastClearC))
        {
            gctUINT32 address[3] = {0};
            gctUINT32 physicalBaseAddress = 0;
            gcsSURF_INFO_PTR target;
            gctUINT32 stride;

            /* Define state buffer variables. */
            gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

#if !gcdSECURE_USER
            /* Get physical base address. */
            if (Hardware->features[gcvFEATURE_MC20])
            {
                physicalBaseAddress = 0;
            }
            else
            {
                gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &physicalBaseAddress));
            }
#endif

            /* Save current target. */
            target = Hardware->colorStates.target[0].surface;

            /* Construct a temporary surface. */
            if (Hardware->tempSurface == gcvNULL)
            {
                gcmONERROR(gcoSURF_Construct(
                    gcvNULL,
                    64, size.y, 1,
                    gcvSURF_RENDER_TARGET,
                    gcvSURF_A8R8G8B8,
                    gcvPOOL_DEFAULT,
                    &Hardware->tempSurface
                    ));
            }

            /* Lock the surface. */
            gcmONERROR(gcoSURF_Lock(
                Hardware->tempSurface, address, logical
                ));

            /* Stride. */
            gcmONERROR(gcoSURF_GetAlignedSize(
                Hardware->tempSurface, gcvNULL, gcvNULL, (gctINT_PTR) &stride
                ));

            /* Determine the size of the buffer to reserve. */
            reserveSize

                /* Grouped states. */
                = gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)

                /* Cache flush. */
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 2, 8)

                /* Grouped resolve states. */
                + gcmALIGN((1 + 5) * gcmSIZEOF(gctUINT32), 8)

                /* Ungrouped resolve states. */
                + gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 3, 8);

            if (!fastClearC)
            {
                reserveSize
                    += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32) * 2, 8);
            }

            if (target && target->tileStatusNode.pool != gcvPOOL_UNKNOWN)
            {
                reserveSize
                    += gcmALIGN((1 + 2) * gcmSIZEOF(gctUINT32), 8)
                    +  gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
            }

            /* Determine the size of the buffer to reserve. */
            gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

            /* Reprogram tile status. */
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x0596,
                      Hardware->tempSurface->info.tileStatusNode.physical
                    + physicalBaseAddress
                    );

                gcmSETSTATEDATA(
                    stateDelta, reserve, memory, gcvFALSE, 0x0597,
                      Hardware->tempSurface->info.node.physical
                    + physicalBaseAddress
                    );

                gcmSETFILLER(
                    reserve, memory
                    );

            gcmENDSTATEBATCH(
                reserve, memory
                );

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) );gcmENDSTATEBATCH(reserve, memory);};

            if (!fastClearC)
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, ((((gctUINT32) (Hardware->memoryConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))));     gcmENDSTATEBATCH(reserve, memory);};
            }

            /* Resolve this buffer on top of itself. */
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)5  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0581, 5 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (5 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0581) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                gcmSETCTRLSTATE(
                    stateDelta, reserve, memory, 0x0581,
                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x06 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)))
                    );

                gcmSETCTRLSTATE(
                    stateDelta, reserve, memory, 0x0582,
                    address[0]
                    );

                gcmSETCTRLSTATE(
                    stateDelta, reserve, memory, 0x0583,
                    stride << 2
                    );

                gcmSETCTRLSTATE(
                    stateDelta, reserve, memory, 0x0584,
                    address[0]
                    );

                gcmSETCTRLSTATE(
                    stateDelta, reserve, memory, 0x0585,
                    stride << 2
                    );

            gcmENDSTATEBATCH(
                reserve, memory
                );

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0588, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0588) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0588, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (size.x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (size.y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) );gcmENDSTATEBATCH(reserve, memory);};

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x058F, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x058F) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x058F, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) );gcmENDSTATEBATCH(reserve, memory);};

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0580, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0580) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0580, 0xBADABEEB );gcmENDSTATEBATCH(reserve, memory);};

            if (target && target->tileStatusNode.pool != gcvPOOL_UNKNOWN)
            {
                /* Reprogram tile status. */
                {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)2  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0596, 2 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0596) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

                    gcmSETSTATEDATA(
                        stateDelta, reserve, memory, gcvFALSE, 0x0596,
                          target->tileStatusNode.physical + physicalBaseAddress
                        );

                    gcmSETSTATEDATA(
                        stateDelta, reserve, memory, gcvFALSE, 0x0597,
                          target->node.physical + physicalBaseAddress
                        );

                    gcmSETFILLER(
                        reserve, memory
                        );

                gcmENDSTATEBATCH(
                    reserve, memory
                    );

                {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) );gcmENDSTATEBATCH(reserve, memory);};
            }

            if (!fastClearC)
            {
                {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0595, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0595) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0595, Hardware->memoryConfig );     gcmENDSTATEBATCH(reserve, memory);};
            }

            /* Invalidate the tile status cache. */
            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

            /* Validate the state buffer. */
            gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

            /* Unlock the surface. */
            gcmONERROR(gcoSURF_Unlock(Hardware->tempSurface, logical[0]));
            logical[0] = gcvNULL;
        }
        else
        {
            if (Decompress && Hardware->colorStates.target[0].surface->dirty && fastClearC)
            {
                /* We need to decompress as well, so resolve the entire
                ** target on top of itself. */
                size.x = Hardware->colorStates.target[0].surface->alignedWidth;
                size.y = Hardware->colorStates.target[0].surface->alignedHeight;
            }

            /* Resolve the current render target onto itself. */
            gcmONERROR(_ResolveRect(Hardware,
                                    Hardware->colorStates.target[0].surface,
                                    Hardware->colorStates.target[0].surface,
                                    &origin, &origin, &size, gcvFALSE
                       ));

            /* The pipe has been flushed by resolve. */
            flushed = gcvTRUE;
        }

        /* The tile status buffer has been flushed */
        flushedTileStatus = gcvTRUE;
    }

    if (!flushed)
    {
        /* Flush the pipe. */
        gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));
    }

    /* Do we need to explicitly flush?. */
    if (flushedTileStatus ||
        Hardware->features[gcvFEATURE_FC_FLUSH_STALL])
    {
        gcmONERROR(gcoHARDWARE_LoadCtrlState(Hardware,
                                             0x01650,
                                             ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))));
    }
    else
    {
        gcmONERROR(_FlushTileStatusCache(Hardware));
    }

    /* Check for decompression request. */
    if (Decompress && Surface->dirty)
    {
        gcsPOINT origin, size;

        /* Setup the origin. */
        origin.x = origin.y = 0;

        /* Setup the size. */
        size.x = Surface->alignedWidth;
        size.y = Surface->alignedHeight;

        if (Surface->type == gcvSURF_RENDER_TARGET)
        {
            if (Hardware->features[gcvFEATURE_TILE_FILLER] &&
                Hardware->features[gcvFEATURE_FC_FLUSH_STALL] &&
                (!Surface->compressed) &&
                (Surface->samples.x == 1) && (Surface->samples.y == 1) &&
                ((Surface->node.size & 0x3fff) == 0))
            {
                /* Define state buffer variables. */
                gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

                /* Determine the size of the buffer to reserve. */
                reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

                /* Determine the size of the buffer to reserve. */
                gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

                {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

                stateDelta = stateDelta; /* Keep the compiler happy. */

                /* Validate the state buffer. */
                gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

                /* Semaphore-stall before next primitive. */
                gcmONERROR(gcoHARDWARE_Semaphore(
                    Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE,
                    gcvNULL
                    ));

                /* Fast clear fill. */
                gcmONERROR(gcoHARDWARE_FillFromTileStatus(Hardware, Surface));
            }
            else
            {
                /* Resolve the current render target onto itself. */
                gcmONERROR(_ResolveRect(
                    Hardware, Surface, Surface, &origin, &origin, &size, gcvFALSE
                    ));

                gcmONERROR(gcoHARDWARE_Commit(Hardware));
            }
        }
        else if (Surface->type == gcvSURF_DEPTH)
        {
            /* Resolve the current depth onto itself. */
            gcmONERROR(gcoHARDWARE_ResolveDepth(
                Hardware,
                Surface, Surface, &origin, &origin, &size, gcvFALSE
                ));
        }

        Surface->dirty = gcvFALSE;
    }

    /* Semaphore-stall before next primitive. */
    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE, gcvNULL
        ));

    /* Tile status cache is no longer dirty. */
    Hardware->cacheDirty = gcvFALSE;

    if (Disable)
    {
        if (Decompress &&
            Surface->type == gcvSURF_DEPTH &&
            Surface->hzNode.pool != gcvPOOL_UNKNOWN &&
            Surface->hzDisabled == gcvFALSE &&
            (((((gctUINT32) (Hardware->memoryConfig)) >> (0 ? 12:12)) & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1)))))) ))
        {
            Surface->hzDisabled = gcvTRUE;

            /* For current depth, need to toggle dirty for later update. */
            if (current)
            {
                Hardware->depthTargetDirty = gcvTRUE;
                Hardware->depthConfigDirty = gcvTRUE;
            }
        }

        if (current)
        {
            gcmONERROR(gcoHARDWARE_DisableHardwareTileStatus(
                Hardware,
                (Surface->type == gcvSURF_DEPTH) ? gcvTILESTATUS_DEPTH : gcvTILESTATUS_COLOR,
                0));
        }

        /* Disable the tile status for this surface. */
        Surface->tileStatusDisabled = Decompress;
    }

OnError:
    if (Hardware != gcvNULL)
    {
        if (logical[0] != gcvNULL)
        {
            /* Unlock the surface. */
            gcmVERIFY_OK(gcoSURF_Unlock(Hardware->tempSurface, logical[0]));
        }

        /* Resume tile status if it was paused. */
        if (switchedTileStatus)
        {
            if (Surface->type == gcvSURF_DEPTH)
            {
                Hardware->depthStates.surface = saved;
            }
            else
            {
                Hardware->colorStates.target[0].surface = saved;
            }

            if (saved)
            {
                    /* Reprogram the tile status to the current surface. */
                gcmVERIFY_OK(
                    gcoHARDWARE_EnableTileStatus(
                        Hardware,
                        saved,
                        (saved->tileStatusNode.pool != gcvPOOL_UNKNOWN) ?  saved->tileStatusNode.physical : 0,
                        &saved->hzTileStatusNode,
                        0));
            }
            else
            {
                gcmVERIFY_OK(
                    gcoHARDWARE_DisableHardwareTileStatus(
                        Hardware,
                        (Surface->type == gcvSURF_DEPTH) ? gcvTILESTATUS_DEPTH : gcvTILESTATUS_COLOR,
                        0));
            }
        }
        /* Flush finished. */
        Hardware->inFlush = gcvFALSE;
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}
gceSTATUS
gcoHARDWARE_FlushTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Decompress
    )
{
    return _FlushAndDisableTileStatus(Hardware, Surface, Decompress, gcvFALSE);
}

gceSTATUS
gcoHARDWARE_DisableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CpuAccess
    )
{
    return _FlushAndDisableTileStatus(Hardware, Surface, CpuAccess, gcvTRUE);
}


/*******************************************************************************
**
**  gcoHARDWARE_SetAntiAlias
**
**  Enable or disable anti-aliasing.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL Enable
**          Enable anti-aliasing when gcvTRUE and disable anti-aliasing when
**          gcvFALSE.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_SetAntiAlias(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Store value. */
    Hardware->sampleMask      = Enable ? 0xF : 0x0;
    Hardware->msaaConfigDirty = gcvTRUE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetDither
**
**  Enable or disable dithering.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gctBOOL Enable
**          gcvTRUE to enable dithering or gcvFALSE to disable it.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_SetDither(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->ditherEnable = Enable;
    Hardware->peDitherDirty = gcvTRUE;

    if (Hardware->features[gcvFEATURE_PE_DITHER_FIX])
    {
        Hardware->colorConfigDirty  = gcvTRUE;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_InvalidateCache
**
**  Invalidate the cache.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gctBOOL KickOff
**          KickOff command immediately or not.
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_InvalidateCache(
    gcoHARDWARE Hardware,
    gctBOOL KickOff
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x KickOff=%d", Hardware, KickOff);

    gcmGETHARDWARE(Hardware);

    gcmONERROR(gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

#if gcdENABLE_3D
    if (Hardware->currentPipe == 0x0)
    {
        if (Hardware->features[gcvFEATURE_FC_FLUSH_STALL])
        {
            /* Define state buffer variables. */
            gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

            /* Determine the size of the buffer to reserve. */
            reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

            /* Determine the size of the buffer to reserve. */
            gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

            {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0594, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );gcmENDSTATEBATCH(reserve, memory);};

            stateDelta = stateDelta; /* Keep the compiler happy. */

            /* Validate the state buffer. */
            gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);
        }
        else
        {
            gcmONERROR(_FlushTileStatusCache(Hardware));
        }
    }
#endif

    if (KickOff)
    {
        gcmONERROR(gcoHARDWARE_Commit(Hardware));
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
**  gcoHARDWARE_UseSoftware2D
**
**  Sets the software 2D renderer force flag.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL Enable
**          gcvTRUE to enable using software for 2D or
**          gcvFALSE to use underlying hardware.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_UseSoftware2D(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set the force flag. */
    Hardware->sw2DEngine = Enable;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHARDWARE_WriteBuffer
**
**  Write data into the command buffer.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to the gcoHARDWARE object.
**
**      gctCONST_POINTER Data
**          Pointer to a buffer that contains the data to be copied.
**
**      IN gctSIZE_T Bytes
**          Number of bytes to copy.
**
**      IN gctBOOL Aligned
**          gcvTRUE if the data needs to be aligned to 64-bit.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_WriteBuffer(
    IN gcoHARDWARE Hardware,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Data=0x%x Bytes=%d Aligned=%d",
                    Hardware, Data, Bytes, Aligned);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Call hardware. */
    gcmONERROR(gcoBUFFER_Write(Hardware->buffer, Data, Bytes, Aligned));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_RGB2YUV
**
**  Convert RGB8 color value to YUV color space.
**
**  INPUT:
**
**      gctUINT8 R, G, B
**          RGB color value.
**
**  OUTPUT:
**
**      gctUINT8_PTR Y, U, V
**          YUV color value.
*/
void gcoHARDWARE_RGB2YUV(
    gctUINT8 R,
    gctUINT8 G,
    gctUINT8 B,
    gctUINT8_PTR Y,
    gctUINT8_PTR U,
    gctUINT8_PTR V
    )
{
    gcmHEADER_ARG("R=%d G=%d B=%d",
                    R, G, B);

    *Y = (gctUINT8) (((66 * R + 129 * G +  25 * B + 128) >> 8) +  16);
    *U = (gctUINT8) (((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128);
    *V = (gctUINT8) (((112 * R -  94 * G -  18 * B + 128) >> 8) + 128);

    gcmFOOTER_ARG("*Y=%d *U=%d *V=%d",
                    *Y, *U, *V);
}

/*******************************************************************************
**
**  gcoHARDWARE_YUV2RGB
**
**  Convert YUV color value to RGB8 color space.
**
**  INPUT:
**
**      gctUINT8 Y, U, V
**          YUV color value.
**
**  OUTPUT:
**
**      gctUINT8_PTR R, G, B
**          RGB color value.
*/
void gcoHARDWARE_YUV2RGB(
    gctUINT8 Y,
    gctUINT8 U,
    gctUINT8 V,
    gctUINT8_PTR R,
    gctUINT8_PTR G,
    gctUINT8_PTR B
    )
{
    /* Clamp the input values to the legal ranges. */
    gctINT y = (Y < 16) ? 16 : ((Y > 235) ? 235 : Y);
    gctINT u = (U < 16) ? 16 : ((U > 240) ? 240 : U);
    gctINT v = (V < 16) ? 16 : ((V > 240) ? 240 : V);

    /* Shift ranges. */
    gctINT _y = y - 16;
    gctINT _u = u - 128;
    gctINT _v = v - 128;

    /* Convert to RGB. */
    gctINT r = (298 * _y            + 409 * _v + 128) >> 8;
    gctINT g = (298 * _y - 100 * _u - 208 * _v + 128) >> 8;
    gctINT b = (298 * _y + 516 * _u            + 128) >> 8;

    gcmHEADER_ARG("Y=%d U=%d V=%d",
                    Y, U, V);

    /* Clamp the result. */
    *R = (r < 0)? 0 : (r > 255)? 255 : (gctUINT8) r;
    *G = (g < 0)? 0 : (g > 255)? 255 : (gctUINT8) g;
    *B = (b < 0)? 0 : (b > 255)? 255 : (gctUINT8) b;

    gcmFOOTER_ARG("*R=%d *G=%d *B=%d",
                    *R, *G, *B);
}

#if gcdENABLE_3D
#if gcdMULTI_GPU

#if gcdSTREAM_OUT_BUFFER
/**
 * TODO: We need to move Hardware->interGpuSemaphoreId out of hardware layer. Because
 *       all processes need to share this id. It may be proper to move this counter to
 *       kernel layer and add an ioctl to fetch the next available id instead of
 *       this function.
 */
gceSTATUS
gcoHARDWARE_MultiGPUSemaphoreId (
    IN gcoHARDWARE Hardware,
    OUT gctUINT8_PTR SemaphoreId
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%08x, SemaphoreId=0x%08x", Hardware, SemaphoreId);

    /* TODO: This counter must be unique for all processes. So we need to move
     * this counter to kernel to share with all running processes
     */
    if (SemaphoreId == gcvNULL)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }
    else
    {
        /* Set out-going semaphore id */
        *SemaphoreId = Hardware->interGpuSemaphoreId;

        /* Increment semaphore id */
        Hardware->interGpuSemaphoreId += 1;

        /* We only have 16 semaphores. So roll back when 16 */
        Hardware->interGpuSemaphoreId &= 0xF;
    }

    gcmFOOTER_ARG("SemaphoreId=%u", *SemaphoreId);
    return status;

OnError:
    gcmFOOTER();
    return status;
}
#endif

gceSTATUS
gcoHARDWARE_MultiGPUSync(
    IN gcoHARDWARE Hardware,
    IN gctBOOL ReserveBuffer,
    OUT gctUINT32_PTR *Memory
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gctUINT32_PTR memory;
    gcoCMDBUF reserve;

    gcmHEADER_ARG("Hardware=0x%08x, ReserveBuffer=0x%08x", Hardware, ReserveBuffer);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->config->gpuCoreCount <= 1)
    {
        gcmFOOTER();
        return status;
    }

    if ((ReserveBuffer == gcvTRUE) && (Hardware != gcvNULL))
    {
        /* Allocate space in the command buffer. */
        gcmONERROR(
            gcoBUFFER_Reserve(Hardware->buffer,
                              18 * gcmSIZEOF(gctUINT32),
                              gcvTRUE,
                              gcvCOMMAND_3D,
                              &reserve));

        /* Cast the reserve pointer. */
        memory = gcmUINT64_TO_PTR(reserve->lastReserve);
    }
    else if ((ReserveBuffer == gcvFALSE) && (Memory != gcvNULL))
    {
        memory = *Memory;
    }
    else
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Send FE-PE sempahore token. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    gcmDUMP(gcvNULL,
            "#[semaphore 0x%08X 0x%08X]",
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));

    /* Send FE-PE stall token. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)));

    gcmDUMP(gcvNULL,
            "#[stall 0x%08X 0x%08X]",
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))),
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))));

    /* Select core 3D 0. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | gcvCORE_3D_0_MASK;

    memory++;

    gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK);

    /* Send a semaphore token from FE to core 3D 1. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.semaphore 0x%04X]", gcvCORE_3D_1_ID);

    /* Await a semaphore token from core 3D 1. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.stall 0x%04X]", gcvCORE_3D_0_ID);

    /* Select core 3D 1. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | gcvCORE_3D_1_MASK;

    memory++;

    gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_1_MASK);

    /* Send a semaphore token from FE to core 3D 0. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E02) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.semaphore 0x%04X]", gcvCORE_3D_0_ID);

    /* Await a semaphore token from GPU 3D_0. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x0F & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)));

    gcmDUMP(gcvNULL, "#[chip.stall 0x%04X]", gcvCORE_3D_1_ID);

    /* Enable all 3D GPUs. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | gcvCORE_3D_ALL_MASK;

    memory++;

    gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK);

    if (Memory != gcvNULL)
    {
        *Memory = memory;
    }

OnError:
    gcmFOOTER();
    return status;
}
#endif

/*
** Flush shader L1 cache to drain write-back buffer
*/
gceSTATUS
gcoHARDWARE_FlushSHL1Cache(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the argments */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Flush the shader L1 cache. */
    gcmONERROR(
       gcoHARDWARE_LoadState32(Hardware, 0x0380C,
                               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)))));

OnError:
    /* Return the status */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_FlushL2Cache(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER_NEW(reserve, stateDelta, memory);

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

#if gcdMULTI_GPU

    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    /* Flush the L2 cache. */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };

    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0E03, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))));    gcmENDSTATEBATCH_NEW(reserve, memory);};

    /* Enable all 3D cores */
    {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };

    stateDelta = stateDelta; /* Keep the compiler happy. */

    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

#else

    /* Idle the pipe. */
    gcmONERROR(
        gcoHARDWARE_Semaphore(Hardware,
                              gcvWHERE_COMMAND,
                              gcvWHERE_PIXEL,
                              gcvHOW_SEMAPHORE_STALL,
                              Memory));

    /* Determine the size of the buffer to reserve. */
    gcmBEGINSTATEBUFFER_NEW(Hardware, reserve, stateDelta, memory, Memory);

    /* Flush the L2 cache. */
    {    {    gcmVERIFYLOADSTATEALIGNED(reserve, memory);    gcmASSERT((gctUINT32)1 <= 1024);    *memory++        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));    gcmSKIPSECUREUSER();};    gcmSETCTRLSTATE_NEW(stateDelta, reserve, memory, 0x0594, ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) );    gcmENDSTATEBATCH_NEW(reserve, memory);};

    stateDelta = stateDelta; /* Keep the compiler happy. */

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER_NEW(Hardware, reserve, memory, Memory);

    /* Idle the pipe (again). */
    gcmONERROR(
        gcoHARDWARE_Semaphore(Hardware,
                              gcvWHERE_COMMAND,
                              gcvWHERE_PIXEL,
                              gcvHOW_SEMAPHORE_STALL,
                              Memory));
#endif

    Hardware->flushL2 = gcvFALSE;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

/*******************************************************************************
**
**  gcoHARDWARE_SetAutoFlushCycles
**
**  Set the GPU clock cycles after which the idle 2D engine will keep auto-flushing.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      UINT32 Cycles
**          Source color premultiply with Source Alpha.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetAutoFlushCycles(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Cycles
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Cycles=%d", Hardware, Cycles);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        /* LoadState timeout value. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x00670,
            Cycles
            ));
    }
    else
    {
        /* Auto-flush is not supported by the software renderer. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/* Resolve depth buffer. */
gceSTATUS
gcoHARDWARE_ResolveDepth(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gctBOOL yInverted
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gceSURF_TYPE originSurfType = SrcInfo->type;

    gcmHEADER_ARG("Hardware=0x%x SrcInfo=0x%x "
                    "DestInfo=0x%x SrcOrigin=0x%x DestOrigin=0x%x "
                    "RectSize=0x%x yInverted=%d",
                    Hardware, SrcInfo,
                    DestInfo, SrcOrigin, DestOrigin,
                    RectSize, yInverted);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(SrcInfo != gcvNULL);

#if gcdENABLE_3D
    /* If the tile status is disabled for the surface, just do a normal
    ** resolve. */
    if (SrcInfo->tileStatusDisabled || (SrcInfo->tileStatusNode.pool == gcvPOOL_UNKNOWN))
    {

        /* Use normal resolve. */
        status = gcoHARDWARE_ResolveRect(Hardware,
                                         SrcInfo,
                                         DestInfo,
                                         SrcOrigin,
                                         DestOrigin,
                                         RectSize,
                                         yInverted);
        /* Return the status. */
        gcmFOOTER();
        return status;
    }
    else
#endif
    {
        /* Resolve destination surface and disable its tile status buffer . */
        if (!Hardware->inFlush)
        {
            gcmONERROR(gcoHARDWARE_DisableTileStatus(Hardware, DestInfo, gcvTRUE));
        }

        /* fake depth as RT for TS operation */
        SrcInfo->type = gcvSURF_RENDER_TARGET;

        /* Determine color format. */
        switch (SrcInfo->format)
        {
        case gcvSURF_D24X8:
        case gcvSURF_D24S8:
        case gcvSURF_D16:
        case gcvSURF_S8:
            /* Keep as it, gcoHARDWARE_EnableTileStatus can support it */
            break;
        default:
            /* Restore surface type */
            SrcInfo->type = originSurfType;

            gcmFOOTER();
            return gcvSTATUS_NOT_SUPPORTED;
        }

        /* Flush the pipe. */
        gcmONERROR(
            gcoHARDWARE_FlushPipe(Hardware, gcvNULL));

        /* Flush the src tile status. */
        gcmONERROR(
            gcoHARDWARE_FlushTileStatus(Hardware, SrcInfo, gcvFALSE));

        /* Perform the resolve. */
        status = gcoHARDWARE_ResolveRect(Hardware,
                                         SrcInfo,
                                         DestInfo,
                                         SrcOrigin,
                                         DestOrigin,
                                         RectSize,
                                         yInverted);


    }

OnError:
    /* Restore surface type */
    SrcInfo->type = originSurfType;

    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

gceSTATUS
gcoHARDWARE_Load2DState32(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    )
{
    /* Call buffered load state to do it. */
    return gcoHARDWARE_Load2DState(Hardware, Address, 1, &Data);

}

gceSTATUS
gcoHARDWARE_Load2DState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER Data
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Address=0x%08x Count=%lu Data=0x%08x",
                  Hardware, Address, Count, Data);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

     /* Verify the arguments. */
    if (Hardware->hw2DCmdIndex & 1)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if (Hardware->hw2DCmdBuffer != gcvNULL)
    {
        gctUINT32_PTR memory;

        if (Hardware->hw2DCmdSize - Hardware->hw2DCmdIndex < gcmALIGN(1 + Count, 2))
        {
            gcmONERROR(gcvSTATUS_OUT_OF_MEMORY);
        }

        memory = Hardware->hw2DCmdBuffer + Hardware->hw2DCmdIndex;

        /* LOAD_STATE(Count, Address >> 2) */
        memory[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (Count) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Address >> 2) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        /* Copy the state buffer. */
        gcoOS_MemCopy(&memory[1], Data, Count * 4);
    }

    Hardware->hw2DCmdIndex += 1 + Count;

    /* Aligned */
    if (Hardware->hw2DCmdIndex & 1)
    {
        Hardware->hw2DCmdIndex += 1;
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
gcoHARDWARE_2DAppendNop(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    /* Verify the arguments. */
    if (Hardware->hw2DCmdIndex & 1)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    if ((Hardware->hw2DCmdBuffer != gcvNULL)
        && (Hardware->hw2DCmdSize > Hardware->hw2DCmdIndex))
    {
        gctUINT32_PTR memory = Hardware->hw2DCmdBuffer + Hardware->hw2DCmdIndex;
        gctUINT32 i = 0;

        while (i < Hardware->hw2DCmdSize - Hardware->hw2DCmdIndex)
        {
            /* Append NOP. */
            memory[i++] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x03 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
            i++;
        }

        Hardware->hw2DCmdIndex = Hardware->hw2DCmdSize;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D

/*******************************************************************************
**  gcoHARDWARE_ComputeCentroids
**
**  Compute centroids.
**
**  INPUT:
**
*/
gceSTATUS
gcoHARDWARE_ComputeCentroids(
    IN gcoHARDWARE Hardware,
    IN gctUINT Count,
    IN gctUINT32_PTR SampleCoords,
    OUT gcsCENTROIDS_PTR Centroids
    )
{
    gctUINT i, j, count, sumX, sumY;

    gcmHEADER_ARG("Hardware=0x%08X", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    for (i = 0; i < Count; i += 1)
    {
        /* Zero out the centroid table. */
        gcoOS_ZeroMemory(&Centroids[i], sizeof(gcsCENTROIDS));

        /* Set the value for invalid pixels. */
        if (Hardware->api == gcvAPI_OPENGL)
        {
            Centroids[i].value[0]
                = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (8) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (8) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
        }

        /* Compute all centroids. */
        for (j = 1; j < 16; j += 1)
        {
            /* Initializ sums and count. */
            sumX = sumY = 0;
            count = 0;

            if ((j == 0x5) || (j == 0xA)
            ||  (j == 0x7) || (j == 0xB)
            ||  (j == 0xD) || (j == 0xE)
            )
            {
                sumX = sumY = 0x8;
            }
            else
            {
                if (j & 0x1)
                {
                    /* Add sample 1. */
                    sumX += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 3:0)) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1)))))) );
                    sumY += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 7:4)) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1)))))) );
                    ++count;
                }

                if (j & 0x2)
                {
                    /* Add sample 2. */
                    sumX += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 11:8)) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1)))))) );
                    sumY += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 15:12)) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1)))))) );
                    ++count;
                }

                if (j & 0x4)
                {
                    /* Add sample 3. */
                    sumX += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 19:16)) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1)))))) );
                    sumY += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 23:20)) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1)))))) );
                    ++count;
                }

                if (j & 0x8)
                {
                    /* Add sample 4. */
                    sumX += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 27:24)) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1)))))) );
                    sumY += (((((gctUINT32) (SampleCoords[i])) >> (0 ? 31:28)) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1)))))) );
                    ++count;
                }

                /* Compute average. */
                if (count > 0)
                {
                    sumX /= count;
                    sumY /= count;
                }
            }

            switch (j & 3)
            {
            case 0:
                /* Set for centroid 0, 4, 8, or 12. */
                Centroids[i].value[j / 4]
                    = ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (sumX) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
                    | ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (sumY) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
                break;

            case 1:
                /* Set for centroid 1, 5, 9, or 13. */
                Centroids[i].value[j / 4]
                    = ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8))) | (((gctUINT32) ((gctUINT32) (sumX) & ((gctUINT32) ((((1 ? 11:8) - (0 ? 11:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:8) - (0 ? 11:8) + 1))))))) << (0 ? 11:8)))
                    | ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (sumY) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)));
                break;

            case 2:
                /* Set for centroid 2, 6, 10, or 14. */
                Centroids[i].value[j / 4]
                    = ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (sumX) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
                    | ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (sumY) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)));
                break;

            case 3:
                /* Set for centroid 3, 7, 11, or 15. */
                Centroids[i].value[j / 4]
                    = ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24))) | (((gctUINT32) ((gctUINT32) (sumX) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1))))))) << (0 ? 27:24)))
                    | ((((gctUINT32) (Centroids[i].value[j / 4])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28))) | (((gctUINT32) ((gctUINT32) (sumY) & ((gctUINT32) ((((1 ? 31:28) - (0 ? 31:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:28) - (0 ? 31:28) + 1))))))) << (0 ? 31:28)));
                break;
            }
        }
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_ProgramResolve
**
**  Program the Resolve offset, Window and then trigger the resolve.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to the gcoHARDWARE object.
**
**      gcsPOINT RectSize
**          The size of the rectangular area to be resolved.
**
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_ProgramResolve(
    IN gcoHARDWARE Hardware,
    IN gcsPOINT RectSize,
    IN gctBOOL  MultiPipe,
    IN gceMSAA_DOWNSAMPLE_MODE DownsampleMode
    )
{
    gceSTATUS status;
    gcoCMDBUF reserve;
    gctUINT32 * memory, reserveSize = 0;
    gctUINT32 useSingleResolveEngine = 0;
    gctUINT32 resolveEngines;
    gctBOOL programRScontrol = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%08x RectSize=%d,%d, MultiPipe=%d DownsampleMode=%d",
                  Hardware, RectSize.x, RectSize.y, MultiPipe, DownsampleMode);


    if (Hardware->multiPipeResolve)
    {
        gcmASSERT(Hardware->config->pixelPipes > 1);
        resolveEngines = Hardware->config->pixelPipes;
    }
    else
    {
        /* Note: When resolving only from 1 engine,
           the rs config register needs to be set to use full_height. */
        resolveEngines = 1;
    }

    /* Resolve from either both the pixel pipes or 1 pipe. */
    gcmASSERT((resolveEngines == Hardware->config->pixelPipes)
           || (resolveEngines == 1));

    if ((resolveEngines != Hardware->config->pixelPipes) ||
         Hardware->features[gcvFEATURE_RS_DEPTHSTENCIL_NATIVE_SUPPORT])
    {
        /* Resolve engine enable/restore states. */
        useSingleResolveEngine = 1;
        reserveSize += 4;
        programRScontrol = gcvTRUE;
    }

    /* Split the window. */
    if (resolveEngines > 1)
    {
        gcmASSERT((RectSize.y & 7) == 0);
        RectSize.y /= 2;
    }

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        reserveSize += 48;
    }
#endif

    /* Window & Offset state. */
    reserveSize += 2;

    if (resolveEngines == 1)
    {
        reserveSize += 2;
    }
    else
    {
        reserveSize += 4;
    }

    /* Trigger state. */
    reserveSize += 2;

    /* Allocate space in the command buffer. */
    gcmONERROR(gcoBUFFER_Reserve(
        Hardware->buffer,
        reserveSize * gcmSIZEOF(gctUINT32),
        gcvTRUE,
        gcvCOMMAND_3D,
        &reserve));

    /* Cast the reserve pointer. */
    memory = gcmUINT64_TO_PTR(reserve->lastReserve);

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Flush both caches. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Invalidate the tile status cache. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Sync the GPUs. */
        gcmONERROR(gcoHARDWARE_MultiGPUSync(gcvNULL, gcvFALSE, &memory));

        /* Select core 3D 0 to do the resolve operation. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_0_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_0_MASK); };
    }
#endif

    /* Append RESOLVE_WINDOW commands. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0588) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (RectSize.x) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (RectSize.y) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

    gcmDUMP(gcvNULL,
            "#[state 0x%04X 0x%08X]",
            0x0588, memory[-1]);

    if (resolveEngines == 1)
    {
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05C0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16)));

         gcmDUMP(gcvNULL,
             "#[state 0x%04X 0x%08X]",
             0x05C0, memory[-1]);

    }
    else
    {

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05C0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 12:0) - (0 ? 12:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:0) - (0 ? 12:0) + 1))))))) << (0 ? 12:0)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16))) | (((gctUINT32) ((gctUINT32) (RectSize.y) & ((gctUINT32) ((((1 ? 28:16) - (0 ? 28:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:16) - (0 ? 28:16) + 1))))))) << (0 ? 28:16)));
        gcmDUMP(gcvNULL,
                "#[state 0x%04X 0x%08X]",
                0x05C0, memory[-2]);

        gcmDUMP(gcvNULL,
                "#[state 0x%04X 0x%08X]",
                0x05C0+1, memory[-1]);

        memory++;

    }



    if (programRScontrol)
    {
        /*************************************************************/
        /* Enable Resolve Engine 0. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AE) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (useSingleResolveEngine) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) |
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) ((gctUINT32) (DownsampleMode) & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)));

        gcmDUMP(gcvNULL,
                "#[state 0x%04X 0x%08X]",
                0x05AE, memory[-1]);
    }

    /* Trigger resolve. */
    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0580) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

    *memory++ = 0xBADABEEB;

    gcmDUMP(gcvNULL,
            "#[state 0x%04X 0x%08X]",
            0x0580, memory[-1]);

    if (programRScontrol)
    {
        /*************************************************************/
        /* Enable Resolve Engine 0. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x05AE) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        gcmDUMP(gcvNULL,
                "#[state 0x%04X 0x%08X]",
                0x05AE, memory[-1]);
    }

#if gcdMULTI_GPU
    if (Hardware->config->gpuCoreCount > 1)
    {
        /* Enable all 3D GPUs. */
        {  *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x0D & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | gcvCORE_3D_ALL_MASK; memory++;  gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", gcvCORE_3D_ALL_MASK); };

        /* Flush both caches. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0E03) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Invalidate the tile status cache. */
        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16)))
                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0594) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));

        *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Sync the GPUs. */
        gcmONERROR(gcoHARDWARE_MultiGPUSync(gcvNULL, gcvFALSE, &memory));
    }
    else
#endif
    {
        /* RA - PE samaphore stall while resolving. */
        gcmONERROR(gcoHARDWARE_Semaphore(
            Hardware, gcvWHERE_RASTER, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE_STALL, gcvNULL));
    }

OnError:
    gcmFOOTER();
    return status;
}
#endif


#if gcdSTREAM_OUT_BUFFER

gceSTATUS
gcoHARDWARE_SwitchBuffer(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%08x", Hardware);

    if (Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_2)
    {
        if (Hardware->streamOutEventQueue->recordCount > 0)
        {
            /* Commit command buffer */
            gcmONERROR(gcoHARDWARE_Commit(Hardware));

            gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                          gcvZONE_STREAM_OUT,
                          "Stream out - Commit buffer, Phase: %d (%s)",
                          Hardware->streamOutStatus,
                          __FUNCTION__);

            gcmTRACE_ZONE(gcvLEVEL_VERBOSE,
                          gcvZONE_STREAM_OUT,
                          "Stream out - Commit %d postponed event(s) (%s)",
                          Hardware->streamOutEventQueue->recordCount,
                          __FUNCTION__);

            /* Commit pending queue */
            gcmONERROR(gcoQUEUE_Commit(Hardware->streamOutEventQueue, gcvFALSE));
        }

    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER_NO();
    return status;
}


gceSTATUS
gcoHARDWARE_QueryStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount,
    OUT gctBOOL_PTR Found
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x IndexAddress=0x%x IndexOffset=0x%x IndexCount=%u Found=0x%x",
                  Hardware, IndexAddress, IndexOffset, IndexCount, Found);

    /* Output must not be null */
    gcmVERIFY_ARGUMENT(Found != gcvNULL);

    gcmONERROR(gcoHARDWARE_QueryStreamOutIndex(Hardware, IndexAddress, IndexOffset, IndexCount, Found));

    gcmFOOTER_ARG("*Found=", *Found);
    return gcvSTATUS_OK;

OnError:
    /* Set as not found */
    *Found = gcvFALSE;

    gcmFOOTER_ARG("*Found=", *Found);
    return status;
}

gceSTATUS
gcoHARDWARE_StartStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctINT StreamOutStatus,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x StreamOutStatus=%d IndexAddress=0x%x IndexOffset=0x%x IndexCount=%u",
                  Hardware, StreamOutStatus, IndexAddress, IndexOffset, IndexCount);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmTRACE_ZONE(gcvLEVEL_INFO,
                  gcvZONE_STREAM_OUT,
                  "Stream out - HW Start - Status %d (%s)",
                  StreamOutStatus,
                  __FUNCTION__);

    if (!Hardware->streamOutInit)
    {
        /* Construct extra queue */
        gcmONERROR(gcoQUEUE_Construct(gcvNULL,
                                      &Hardware->streamOutEventQueue));

        /* State successfull initialization */
        Hardware->streamOutInit = gcvTRUE;
    }

    switch (StreamOutStatus)
    {
        case gcvSTREAM_OUT_CHIP_LAYER_STATUS_PREPASS:
        {

            Hardware->streamOutStatus = gcvSTREAM_OUT_STATUS_PHASE_1_PREPASS;
            break;
        }
        case gcvSTREAM_OUT_CHIP_LAYER_STATUS_PREPASS_AND_REPLAY:
        {
            Hardware->streamOutStatus = gcvSTREAM_OUT_STATUS_PHASE_1;
            break;
        }
        default:
        {
            gcmASSERT(gcvFALSE);
            break;
        }
    }

    /* Set incoming index */
    Hardware->streamOutOriginalIndexAddess = IndexAddress;
    Hardware->streamOutOriginalIndexCount = IndexCount;
    Hardware->streamOutOriginalIndexOffset = IndexOffset;
    Hardware->streamOutNeedSync = gcvTRUE;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:

    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoHARDWARE_StopStreamOut(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%08x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmTRACE_ZONE(gcvLEVEL_INFO,
                  gcvZONE_STREAM_OUT,
                  "Stream out - HW Stop - (%s)",
                  __FUNCTION__);

    /* Stop stream out */
    gcmONERROR(gcoHARDWARE_StopSignalStreamOut(Hardware));


    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoHARDWARE_ReplayStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x IndexAddress=0x%x IndexOffset=0x%x IndexCount=%u",
                  Hardware, IndexAddress, IndexOffset, IndexCount);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmTRACE_ZONE(gcvLEVEL_INFO,
                  gcvZONE_STREAM_OUT,
                  "Stream out - HW Replay - (%s)",
                  __FUNCTION__);

    /* Set incoming index */
    Hardware->streamOutOriginalIndexAddess = IndexAddress;
    Hardware->streamOutOriginalIndexCount = IndexCount;
    Hardware->streamOutOriginalIndexOffset = IndexOffset;

    /* Advance to next phase */
    Hardware->streamOutStatus = gcvSTREAM_OUT_STATUS_PHASE_2;

    /* Stop stream out */
    if(Hardware->streamOutNeedSync)
    {
        gcmONERROR(gcoHARDWARE_SyncStreamOut(Hardware));
        Hardware->streamOutNeedSync = gcvFALSE;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoHARDWARE_EndStreamOut(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%08x", Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->streamOutStatus == gcvSTREAM_OUT_STATUS_PHASE_2)
    {
        /* Switch command buffer */
        gcmONERROR(gcoHARDWARE_SwitchBuffer(Hardware));

        /* Free previously used stream out index */
        gcmONERROR(gcoHARDWARE_FreeStreamOutIndex(Hardware));
    }

    /* Disable stream out */
    Hardware->streamOutStatus = gcvSTREAM_OUT_STATUS_DISABLED;

    gcmTRACE_ZONE(gcvLEVEL_INFO,
                  gcvZONE_STREAM_OUT,
                  "Stream out - HW End - (%s)",
                  __FUNCTION__);

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER_NO();
    return status;
}

#endif


#if VIVANTE_PROFILER_CONTEXT
gceSTATUS
gcoHARDWARE_GetContext(
    IN gcoHARDWARE Hardware,
    OUT gctUINT32 * Context
    )
{
    *Context = Hardware->context;
    return gcvSTATUS_OK;
}
#endif

#if VIVANTE_PROFILER_NEW
gceSTATUS
gcoHARDWARE_EnableCounters(
    IN gcoHARDWARE Hardware
    )
{
    gctUINT32 i = 0;
    gctUINT32 data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) |
                     ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) |
                     ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) |
                     ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) |
                     ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) |
                     ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1))))))) << (0 ? 21:21))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 21:21) - (0 ? 21:21) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:21) - (0 ? 21:21) + 1))))))) << (0 ? 21:21)));
    gcoHARDWARE_LoadState32(Hardware, 0x03848, data);

    /* reset all counters; seems it doesn't work */
    /* module FE */
    for (i = 0; i < 3; i++)
    {
        data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (i) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
        gcoHARDWARE_LoadState32(Hardware, 0x03878, data);
    }

    /* module PA */
    for (i = 0; i < 7; i++)
    {
        data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (i) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
        gcoHARDWARE_LoadState32(Hardware, 0x03878, data);
    }

    /* module FE */
    for (i = 0; i < 4; i++)
    {
        data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (7) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) |
               ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (i) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
        gcoHARDWARE_LoadState32(Hardware, 0x03878, data);
    }
   return gcvSTATUS_OK;
}

gceSTATUS
gcoHARDWARE_ProbeCounter(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 address,
    IN gctUINT32 module,
    IN gctUINT32 counter
    )
{
    gctUINT32 data;

    /* pass address to store counter */
    gcoHARDWARE_LoadState32(Hardware, 0x03870, (gctUINT32)address);

    /* probe counter and write to the address */
    data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (module) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (counter) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
    gcoHARDWARE_LoadState32(Hardware, 0x03878, data);

    /* reset counter */
    data = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (module) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (counter) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
    gcoHARDWARE_LoadState32(Hardware, 0x03878, data);
    return gcvSTATUS_OK;
}
#endif


/**********************************************************************
*********
**
**  gcoHARDWARE_Get2DHardware
**
**  Get the pointer to the current gcoHARDWARE object of this thread.
**
**
**  OUTPUT:
**
**   gco2D * Hardware
**       Pointer to a variable receiving the gcoHARDWARE object pointer.
*/
gceSTATUS
gcoHARDWARE_Get2DHardware(
     OUT gcoHARDWARE * Hardware
     )
{
     gceSTATUS status;
     gcsTLS_PTR tls;

     gcmHEADER();

     /* Verify the arguments. */
     gcmDEBUG_VERIFY_ARGUMENT(Hardware != gcvNULL);

     gcmONERROR(gcoOS_GetTLS(&tls));

     /* Return pointer to the gco2D object. */
     if (gcPLS.hal->separated2D && gcPLS.hal->is3DAvailable)
        *Hardware = tls->hardware2D;
     else
        *Hardware = tls->currentHardware;

     /* Success. */
     gcmFOOTER_ARG("*Hardware=0x%x", *Hardware);
     return gcvSTATUS_OK;

OnError:
     /* Return the status. */
     gcmFOOTER();
     return status;
}


/**********************************************************************
*********
**
**  gcoHARDWARE_Set2DHardware
**
**  Set the gcoHARDWARE object.
**
**  INPUT:
**
**
**   gcoHARDWARE Hardware
**       The gcoHARDWARE object.
**
**  OUTPUT:
**
*/
gceSTATUS
gcoHARDWARE_Set2DHardware(
     IN gcoHARDWARE Hardware
     )
{
    gceSTATUS status;
    gcsTLS_PTR tls;

    gcmHEADER();

    gcmONERROR(gcoOS_GetTLS(&tls));

    /* Set current hardware as requested */
    if (gcPLS.hal->separated2D && gcPLS.hal->is3DAvailable)
    {
       tls->hardware2D = Hardware;
    }
    else
    {
       tls->currentHardware = Hardware;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/**********************************************************************
*********
**
**  gcoHARDWARE_Get3DHardware
**
**  Get the pointer to the current gcoHARDWARE object of this thread.
**
**
**  OUTPUT:
**
**   gco3D * Hardware
**       Pointer to a variable receiving the gcoHARDWARE object pointer.
*/
gceSTATUS
gcoHARDWARE_Get3DHardware(
     OUT gcoHARDWARE * Hardware
     )
{
     gceSTATUS status;
     gcsTLS_PTR tls;

     gcmHEADER();

     /* Verify the arguments. */
     gcmDEBUG_VERIFY_ARGUMENT(Hardware != gcvNULL);

     gcmONERROR(gcoOS_GetTLS(&tls));

     /* Return pointer to the gco3D object. */
     *Hardware = tls->currentHardware;

     /* Success. */
     gcmFOOTER_ARG("*Hardware=0x%x", *Hardware);
     return gcvSTATUS_OK;

OnError:
     /* Return the status. */
     gcmFOOTER();
     return status;
}


/**********************************************************************
*********
**
**  gcoHARDWARE_Set3DHardware
**
**  Set the gcoHARDWARE object.
**
**  INPUT:
**
**
**   gcoHARDWARE Hardware
**       The gcoHARDWARE object.
**
**  OUTPUT:
**
*/
gceSTATUS
gcoHARDWARE_Set3DHardware(
     IN gcoHARDWARE Hardware
     )
{
    gceSTATUS status;
    gcsTLS_PTR tls;

    gcmHEADER();

    gcmONERROR(gcoOS_GetTLS(&tls));

    if (tls->currentHardware && tls->currentHardware != Hardware)
    {
        gcmDUMP(gcvNULL,
                "#[end contextid %d, default=%d]",
                tls->currentHardware->context,
                tls->currentHardware->threadDefault);

        gcmONERROR(gcoHARDWARE_Commit(tls->currentHardware));
    }

    /* Set current hardware as requested */
    tls->currentHardware = Hardware;

#if gcdDUMP
    if (Hardware)
    {
        gcmDUMP(gcvNULL,
                "#[start contextid %d, default=%d]",
                Hardware->context,
                Hardware->threadDefault);
    }
#endif

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*
** We must make sure below format definiton is exact same.
** 7:4
** 11:8
** 6:3
** 7:4
*/
gceSTATUS
gcoHARDWARE_QueryCompression(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    OUT gctBOOL *Compressed,
    OUT gctINT32 *CompressedFormat
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL compressed;
    gctINT32 format = -1;

    gcmHEADER_ARG("Hardware = 0x%08X, Surface = 0x%08X, Compressed = 0x%08X, CompressedFormat = 0x%08X",
                  Hardware, Surface, Compressed, CompressedFormat);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmVERIFY_ARGUMENT(Compressed != gcvNULL);
    gcmVERIFY_ARGUMENT(CompressedFormat != gcvNULL);

    compressed = Hardware->features[gcvFEATURE_COMPRESSION];

    if (compressed)
    {
        /* Determine color compression format. */
        switch (Surface->format)
        {
        case gcvSURF_D16:
            if (Hardware->features[gcvFEATURE_COMPRESSION_V2] ||
                Hardware->features[gcvFEATURE_COMPRESSION_V3] ||
                (Hardware->features[gcvFEATURE_V1_COMPRESSION_Z16_DECOMPRESS_FIX] &&
                 Hardware->features[gcvFEATURE_COMPRESSION_V1]))
            {
                format = 0x8;
            }
            break;

        case gcvSURF_A4R4G4B4:
        case gcvSURF_X4R4G4B4:
            format = 0x0;
            break;

        case gcvSURF_A1R5G5B5:
        case gcvSURF_X1R5G5B5:
            format = 0x1;
            break;

        case gcvSURF_R5G6B5:
            format = 0x2;
            break;

        case gcvSURF_X8R8G8B8:
        case gcvSURF_R8_1_X8R8G8B8:
        case gcvSURF_G8R8_1_X8R8G8B8:
            format = Surface->vaa
                   ? 0x0
                   : (Hardware->config->chipRevision < 0x4500)
                   ? 0x3
                   : 0x4;
            break;

        case gcvSURF_A8R8G8B8:
            format = 0x3;
            break;

        /* Depth will fake as RT when resolve */
        case gcvSURF_D24X8:
            format = 0x6;
            break;

        case gcvSURF_D24S8:
            format = 0x5;
            break;

        default:
            format = -1;
            break;
        }


        if (Surface->type == gcvSURF_DEPTH)
        {
            /*
            ** Disable D16 compression check
            */
            if ((Surface->format == gcvSURF_D16) &&
                (Hardware->features[gcvFEATURE_COMPRESSION_V2]) &&
                (Surface->fcValue != 0xFFFFFFFF) &&
                (!Hardware->features[gcvFEATURE_V2_COMPRESSION_Z16_FIX]))
            {
                compressed = gcvFALSE;
            }
        }
        else
        {
                gcmASSERT(Surface->formatInfo.fmtClass != gcvFORMAT_CLASS_DEPTH);

                compressed = ((format != -1) &&
                               ((Surface->samples.x * Surface->samples.y > 1) ||
                                (Hardware->features[gcvFEATURE_COLOR_COMPRESSION]))
                             );
        }

        /*
        ** If TX can't support decompressor, prefer to non-compressed as tile filler will help.
        */
        if ((Surface->hints & gcvSURF_CREATE_AS_TEXTURE) &&
            ((!Hardware->features[gcvFEATURE_TX_DECOMPRESSOR] &&
               Hardware->features[gcvFEATURE_TEXTURE_TILE_STATUS_READ]) ||
             (Hardware->swwas[gcvSWWA_1165])))
        {
            compressed = gcvFALSE;
        }

        if ((Surface->samples.x * Surface->samples.y > 1) &&
            Hardware->features[gcvFEATURE_COMPRESSION_V2] &&
            !Hardware->features[gcvFEATURE_V2_MSAA_COMP_FIX])
        {
            compressed = gcvFALSE;
        }

    }

    *Compressed = compressed;
    *CompressedFormat = format;

OnError:
    gcmFOOTER_NO();
    return status;
}

static gceSTATUS _BlitDrawClear(
    gcsSURF_BLITDRAW_ARGS *args
    )
{
    gceSTATUS status;
    gcoHARDWARE hardware = gcvNULL;
    gcsHARDWARE_BLITDRAW_PTR blitDraw;
    gcsSURF_CLEAR_ARGS *clearArg;
    gcsSURF_INFO_PTR rtSurface = &args->uArgs.v1.u.clear.rtSurface->info;
    gcsSURF_INFO_PTR dsSurface = &args->uArgs.v1.u.clear.dsSurface->info;
    gcsSURF_FORMAT_INFO_PTR dstFmtInfo = gcvNULL;
    gcsPROGRAM_STATE *programState = gcvNULL;
    gctINT32 i = 0;

    gctUINT srcWidth  = 1;
    gctUINT srcHeight = 1;

    /* Clear path set-up */
    clearArg = &args->uArgs.v1.u.clear.clearArgs;

    /* Kick off current hardware and switch to default hardware */
    gcmONERROR(gcoHARDWARE_Set3DHardware(gcvNULL));
    gcmGETHARDWARE(hardware);

    /* Currently it's a must that default hardware is current */
    gcmASSERT(hardware->threadDefault);

    gcmONERROR(_InitBlitDraw(hardware));
    blitDraw = hardware->blitDraw;

    if (clearArg->flags & gcvCLEAR_COLOR)
    {
        gcmASSERT(rtSurface);

        dstFmtInfo = &rtSurface->formatInfo;

        for (i = 0; i < dstFmtInfo->layers; i++)
        {
            gcmONERROR(gcoHARDWARE_SetRenderTarget(hardware,
                                                   i,
                                                   rtSurface,
                                                   i));

            gcmONERROR(gcoHARDWARE_SetRenderTargetOffset(hardware,
                                                         i,
                                                         rtSurface->offset));
        }

        gcmONERROR(gcoHARDWARE_EnableTileStatus(hardware,
                                                rtSurface,
                                                (rtSurface->tileStatusNode.pool != gcvPOOL_UNKNOWN) ?
                                                rtSurface->tileStatusNode.physical : 0,
                                                &rtSurface->hzTileStatusNode,
                                                0));



        gcmONERROR(gcoHARDWARE_SetColorWrite(hardware, clearArg->colorMask));
        gcmONERROR(gcoHARDWARE_SetDepthOnly(hardware, gcvFALSE));

        gcmONERROR(gcoHARDWARE_SetViewport(hardware,
                                           rtSurface->rect.left / rtSurface->samples.x,
                                           rtSurface->rect.bottom / rtSurface->samples.y,
                                           rtSurface->rect.right / rtSurface->samples.x,
                                           rtSurface->rect.top / rtSurface->samples.y));

        gcmONERROR(gcoHARDWARE_SetScissors(hardware,
                                           rtSurface->rect.left / rtSurface->samples.x,
                                           rtSurface->rect.top / rtSurface->samples.y,
                                           rtSurface->rect.right / rtSurface->samples.x,
                                           rtSurface->rect.bottom / rtSurface->samples.y));

        srcWidth  = rtSurface->rect.right  / rtSurface->samples.x;
        srcHeight = rtSurface->rect.bottom / rtSurface->samples.y;

        gcmONERROR(gcoHARDWARE_SetColorOutCount(hardware, dstFmtInfo->layers));
    }
    else
    {
        gcmONERROR(gcoHARDWARE_SetColorWrite(hardware, 0));
        gcmONERROR(gcoHARDWARE_SetColorOutCount(hardware, 0));
    }

    if (clearArg->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
    {
        gcsSTENCIL_INFO stencilInfo;
        gcmASSERT(dsSurface);
        dstFmtInfo = &dsSurface->formatInfo;

        gcmONERROR(gcoHARDWARE_SetDepthBuffer(hardware, dsSurface));
        gcmONERROR(gcoHARDWARE_SetDepthBufferOffset(hardware, 0));
        gcmONERROR(gcoHARDWARE_EnableTileStatus(hardware,
                                                dsSurface,
                                                (dsSurface->tileStatusNode.pool != gcvPOOL_UNKNOWN) ?
                                                dsSurface->tileStatusNode.physical : 0,
                                                &dsSurface->hzTileStatusNode,
                                                0));
        gcmONERROR(gcoHARDWARE_SetDepthMode(hardware, gcvDEPTH_Z));
        gcmONERROR(gcoHARDWARE_SetDepthOnly(hardware, gcvTRUE));
        gcmONERROR(gcoHARDWARE_SetDepthRangeF(hardware, gcvDEPTH_Z, 0.0f, 1.0f));
        gcmONERROR(gcoHARDWARE_SetDepthWrite(hardware, (clearArg->flags & gcvCLEAR_DEPTH) ?
                                                        clearArg->depthMask : 0));

        if (clearArg->flags & gcvCLEAR_STENCIL)
        {
            stencilInfo.compareBack =
            stencilInfo.compareFront = gcvCOMPARE_ALWAYS;
            stencilInfo.maskBack =
            stencilInfo.maskFront = 0xff;
            stencilInfo.referenceBack =
            stencilInfo.referenceFront = (gctUINT8)clearArg->stencil;
            stencilInfo.depthFailBack =
            stencilInfo.depthFailFront =
            stencilInfo.failBack =
            stencilInfo.failFront =
            stencilInfo.passBack =
            stencilInfo.passFront = gcvSTENCIL_REPLACE;
            stencilInfo.writeMaskBack =
            stencilInfo.writeMaskFront = clearArg->stencilMask;
            stencilInfo.mode = gcvSTENCIL_DOUBLE_SIDED;
        }
        else
        {
            stencilInfo.mode = gcvSTENCIL_NONE;
            stencilInfo.compareBack =
            stencilInfo.compareFront = gcvCOMPARE_ALWAYS;
            stencilInfo.maskBack =
            stencilInfo.maskFront = 0xff;
            stencilInfo.referenceBack =
            stencilInfo.referenceFront = (gctUINT8)clearArg->stencil;
            stencilInfo.depthFailBack =
            stencilInfo.depthFailFront =
            stencilInfo.failBack =
            stencilInfo.failFront =
            stencilInfo.passBack =
            stencilInfo.passFront = gcvSTENCIL_KEEP;
            stencilInfo.writeMaskBack =
            stencilInfo.writeMaskFront = 0x0;
            stencilInfo.mode = gcvSTENCIL_DOUBLE_SIDED;
        }
        gcmONERROR(gcoHARDWARE_SetStencilAll(hardware, &stencilInfo));

        gcmONERROR(gcoHARDWARE_SetViewport(hardware,
                                           dsSurface->rect.left / dsSurface->samples.x,
                                           dsSurface->rect.bottom / dsSurface->samples.y,
                                           dsSurface->rect.right / dsSurface->samples.x,
                                           dsSurface->rect.top / dsSurface->samples.y));

        gcmONERROR(gcoHARDWARE_SetScissors(hardware,
                                           dsSurface->rect.left / dsSurface->samples.x,
                                           dsSurface->rect.top / dsSurface->samples.y,
                                           dsSurface->rect.right / dsSurface->samples.x,
                                           dsSurface->rect.bottom / dsSurface->samples.y));

        srcWidth  = dsSurface->rect.right  / dsSurface->samples.x;
        srcHeight = dsSurface->rect.bottom / dsSurface->samples.y;
    }
    else
    {
        gcmONERROR(gcoHARDWARE_SetDepthMode(hardware, gcvDEPTH_NONE));
    }

    gcmONERROR(gcoHARDWARE_SetCulling(hardware, gcvCULL_NONE));
    gcmONERROR(gcoHARDWARE_SetFill(hardware, gcvFILL_SOLID));

    gcmONERROR(_PickBlitDrawShader(hardware,
                                   gcvBLITDRAW_CLEAR,
                                   gcvNULL,
                                   dstFmtInfo,
                                   &programState));

    gcmONERROR(gcoHARDWARE_LoadShaders(hardware,
                                       programState->programSize,
                                       programState->programBuffer,
                                       programState->hints));

    /* Flush uniform */
    if (clearArg->flags & gcvCLEAR_COLOR)
    {
        gctFLOAT fColor[4];
        gctUINT32 physicalAddress = 0, baseAddress = 0, shift = 0;

        switch (rtSurface->format)
        {
        case gcvSURF_R8_1_X8R8G8B8:
            fColor[0] = clearArg->color.r.floatValue;
            fColor[1] = 0.0f;
            fColor[2] = 0.0f;
            fColor[3] = 1.0f;
            break;
        case gcvSURF_G8R8_1_X8R8G8B8:
            fColor[0] = clearArg->color.r.floatValue;
            fColor[1] = clearArg->color.g.floatValue;
            fColor[2] = 0.0f;
            fColor[3] = 1.0f;
            break;
        default:
            fColor[0] = clearArg->color.r.floatValue;
            fColor[1] = clearArg->color.g.floatValue;
            fColor[2] = clearArg->color.b.floatValue;
            fColor[3] = clearArg->color.a.floatValue;
            break;
        }

        baseAddress = programState->hints->psConstBase;
        shift = (gctUINT32)gcmExtractSwizzle(blitDraw->clearColorUnfiorm->swizzle, 0);

        physicalAddress = baseAddress + blitDraw->clearColorUnfiorm->physical * 16 + shift * 4;

        gcmONERROR(gcoHARDWARE_ProgramUniform(gcvNULL, physicalAddress,
                                              4, 1, fColor, 0, 0, blitDraw->clearColorUnfiorm->shaderKind));
    }

    /* Set up vertex stream.*/
    do
    {
        gcsVERTEXARRAY_BUFOBJ stream;
        gcsVERTEXARRAY_BUFOBJ_PTR streamPtr = &stream;
        gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE attrib;
        gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR attribPtr = &attrib;
        gctFLOAT left, top, right, bottom;
        gctFLOAT depth = 1.0f;

        gctFLOAT positionData[18];

        if (clearArg->flags & gcvCLEAR_DEPTH)
        {
            depth = (clearArg->depth.floatValue >= 1.0f) ?  1.0f
                  : (clearArg->depth.floatValue <= 0.0f) ? -1.0f
                  : 2 * clearArg->depth.floatValue - 1.0f;
        }

        /*
         * Generate two CCW triangles.
         * Clear Arg coordinate is Top-Bottom.
         * OpenGL vertex coordinate is Bottom-Top.
         */
        left   = 2.0f * clearArg->clearRect->left   / srcWidth  - 1.0f;
        top    = 2.0f * clearArg->clearRect->bottom / srcHeight - 1.0f;
        right  = 2.0f * clearArg->clearRect->right  / srcWidth  - 1.0f;
        bottom = 2.0f * clearArg->clearRect->top    / srcHeight - 1.0f;

        positionData[0]  = left;
        positionData[1]  = bottom;
        positionData[2]  = depth;

        positionData[3]  = right;
        positionData[4]  = bottom;
        positionData[5]  = depth;

        positionData[6]  = left;
        positionData[7]  = top;
        positionData[8]  = depth;

        positionData[9]  = left;
        positionData[10] = top;
        positionData[11] = depth;

        positionData[12] = right;
        positionData[13] = bottom;
        positionData[14] = depth;

        positionData[15] = right;
        positionData[16] = top;
        positionData[17] = depth;

        gcoOS_ZeroMemory(streamPtr, sizeof(gcsVERTEXARRAY_BUFOBJ));

        /* set steam. */
        streamPtr->stride         = 0xc;
        streamPtr->divisor        = 0x0;
        streamPtr->count          = 0x6;
        streamPtr->attributeCount = 0x1;

        /* set attrib.*/
        attribPtr->format     = gcvVERTEX_FLOAT;
        attribPtr->linkage    = 0x0; /* Only one attribute for clear shader*/
        attribPtr->size       = 0x3;
        attribPtr->normalized = 0x0;
        attribPtr->enabled    = gcvTRUE;
        attribPtr->offset     = 0x0;
        attribPtr->pointer    = &positionData;
        attribPtr->bytes      = 0xc;
        attribPtr->isPosition = 0x0;
        attribPtr->stride     = 0xc;
        attribPtr->next       = gcvNULL;

        streamPtr->attributePtr = attribPtr;

        gcmONERROR(gcoSTREAM_CacheAttributesEx(blitDraw->dynamicStream,
                                             0x1,
                                             streamPtr,
                                             0x0,
                                             gcvNULL
                                             ));

        gcmONERROR(gcoHARDWARE_SetVertexArrayEx(gcvNULL,
            1,
            0,
            1,
            streamPtr,
            0,
            0,
            0xffffffff
            ));

        if (hardware->features[gcvFEATURE_HALTI0])
        {
            /* Draw command.*/
            gcmONERROR(gcoHARDWARE_DrawInstancedPrimitives(hardware,
                                                           0,
                                                           gcvPRIMITIVE_TRIANGLE_LIST,
                                                           0,
                                                           0,
                                                           2,
                                                           6,
                                                           1));
        }
        else
        {
            /* Draw command.*/
            gcmONERROR(gcoHARDWARE_DrawPrimitives(hardware,
                                                  gcvPRIMITIVE_TRIANGLE_LIST,
                                                  0,
                                                  2));
        }

        /* reset rt. Should set gcvNULL to rt to triger MSAA read temp rt when clear depth.*/
        if (clearArg->flags & gcvCLEAR_COLOR)
        {
            gcmONERROR(gcoHARDWARE_DisableTileStatus(hardware, rtSurface, gcvFALSE));
            for (i = 0; i < dstFmtInfo->layers; i++)
            {
                gcmONERROR(gcoHARDWARE_SetRenderTarget(hardware,
                                                       i,
                                                       gcvNULL,
                                                       i));
            }
        }

        if (clearArg->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
        {
            gcmONERROR(gcoHARDWARE_DisableTileStatus(hardware, dsSurface, gcvFALSE));
            gcmONERROR(gcoHARDWARE_SetDepthBuffer(hardware, gcvNULL));
        }

    }
    while(gcvFALSE);

OnError:
    return status;
}


static gceSTATUS _BlitDrawBlit(
    gcsSURF_BLITDRAW_ARGS *args
    )
{
    gceSTATUS status;
    gcoHARDWARE hardware = gcvNULL;
    gscSURF_BLITDRAW_BLIT *blitArg;
    gcsSAMPLER samplerInfo = {0};
    gcsTEXTURE texParamInfo = {0};
    gcsSURF_INFO_PTR srcSurf, dstSurf;
    gcsHARDWARE_BLITDRAW_PTR blitDraw;
    gcsSURF_FORMAT_INFO_PTR dstFmtInfo = gcvNULL;
    gcsSURF_FORMAT_INFO_PTR srcFmtInfo = gcvNULL;
    gcsPROGRAM_STATE *programState = gcvNULL;
    gctINT32 i;

    static const gceTEXTURE_SWIZZLE baseComponents_rgba[] =
    {
        gcvTEXTURE_SWIZZLE_R,
        gcvTEXTURE_SWIZZLE_G,
        gcvTEXTURE_SWIZZLE_B,
        gcvTEXTURE_SWIZZLE_A
    };

    /* Get current hardware. */
    gcmGETHARDWARE(hardware);

    /* Blit path set-up */
    blitArg = &args->uArgs.v1.u.blit;

    srcSurf = &blitArg->srcSurface->info;
    dstSurf = &blitArg->dstSurface->info;

    srcFmtInfo = &srcSurf->formatInfo;
    dstFmtInfo = &dstSurf->formatInfo;

    /* explicit unsupport cases for source:
    ** 1, msaa source, We may support it later.
    ** 2, TX can't read it.
    */
    if ((srcSurf->samples.x * srcSurf->samples.y > 1) ||
        (!hardware->features[gcvFEATURE_SUPERTILED_TEXTURE] && (srcSurf->tiling & gcvSUPERTILED)) ||
        (!hardware->features[gcvFEATURE_TEXTURE_LINEAR] && (srcSurf->tiling & gcvLINEAR))
       )
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Bail out if dst surface is not renderable*/
    if (gcvSTATUS_OK != gcoHARDWARE_QuerySurfaceRenderable(hardware, dstSurf))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Disable source tile status if TX can not read. */
    if (
        (srcSurf->tileStatusDisabled == gcvFALSE &&
            !hardware->features[gcvFEATURE_TEXTURE_TILE_STATUS_READ]) ||
        (srcSurf->compressed &&
            !hardware->features[gcvFEATURE_TX_DECOMPRESSOR]) ||
        hardware->swwas[gcvSWWA_1165]
       )
    {
        /*
         * NOTICE: Must call disable tile status in rendering context before
         * switching back to blitdraw context, to make hardware states match in
         * the two contexts.
         */
        gcoHARDWARE_DisableTileStatus(hardware, srcSurf, gcvTRUE);
        samplerInfo.hasTileStatus = gcvFALSE;
    }

    /* Kick off current hardware and switch to default hardware */
    gcmONERROR(gcoHARDWARE_Set3DHardware(gcvNULL));

    hardware = gcvNULL;
    gcmGETHARDWARE(hardware);

    /* Currently it's a must that default hardware is current */
    gcmASSERT(hardware->threadDefault);

    gcmONERROR(_InitBlitDraw(hardware));
    blitDraw = hardware->blitDraw;

    samplerInfo.width = srcSurf->rect.right;
    samplerInfo.height = srcSurf->rect.bottom;
    samplerInfo.depth = 1;
    samplerInfo.faces = 1;

    samplerInfo.endianHint = ((srcFmtInfo->txFormat == 0x05)  ||
                              (srcFmtInfo->txFormat == 0x06)  ||
                              (srcFmtInfo->txFormat == 0x0C) ||
                              (srcFmtInfo->txFormat == 0x0D) ||
                              (srcFmtInfo->txFormat == 0x0B)) ?
                              gcvENDIAN_SWAP_WORD : gcvENDIAN_NO_SWAP;

    samplerInfo.filterable = gcvTRUE;
    samplerInfo.format = srcSurf->format;
    samplerInfo.formatInfo = &srcSurf->formatInfo;

    switch (srcSurf->tiling)
    {
    case gcvLINEAR:
        samplerInfo.hAlignment = gcvSURF_SIXTEEN;
        samplerInfo.addressing = gcvSURF_STRIDE_LINEAR;

        switch (samplerInfo.format)
        {
        case gcvSURF_YV12:
        case gcvSURF_I420:
        case gcvSURF_NV12:
        case gcvSURF_NV21:
        case gcvSURF_NV16:
        case gcvSURF_NV61:
            /* Need YUV assembler. */
            samplerInfo.lodNum = 3;
            break;

        case gcvSURF_YUY2:
        case gcvSURF_UYVY:
        case gcvSURF_YVYU:
        case gcvSURF_VYUY:
        default:
            /* Need linear texture. */
            samplerInfo.lodNum = 1;
            break;
        }
        break;

    case gcvTILED:
       gcmONERROR(gcoHARDWARE_QueryTileAlignment(gcvNULL,
                                                 gcvSURF_TEXTURE,
                                                 srcSurf->format,
                                                 &samplerInfo.hAlignment,
                                                 gcvNULL));
        samplerInfo.lodNum = 1;
        break;

    case gcvSUPERTILED:
        samplerInfo.hAlignment = gcvSURF_SUPER_TILED;
        samplerInfo.lodNum = 1;
        break;

    case gcvMULTI_TILED:
        samplerInfo.hAlignment = gcvSURF_SPLIT_TILED;
        samplerInfo.lodNum = 2;
        break;

    case gcvMULTI_SUPERTILED:
        samplerInfo.hAlignment = gcvSURF_SPLIT_SUPER_TILED;
        samplerInfo.lodNum = 2;
        break;

    default:
        gcmASSERT(gcvFALSE);
        status = gcvSTATUS_NOT_SUPPORTED;
        return status;
    }

    /* Set all the LOD levels. */
    samplerInfo.lodAddr[0] = srcSurf->node.physical;

    if (samplerInfo.lodNum == 3)
    {
        /* YUV-assembler needs 3 lods. */
        samplerInfo.lodAddr[1]   = srcSurf->node.physical + srcSurf->uOffset;
        samplerInfo.lodAddr[2]   = srcSurf->node.physical + srcSurf->vOffset;

        /* Save strides. */
        samplerInfo.lodStride[0] = srcSurf->stride;
        samplerInfo.lodStride[1] = srcSurf->uStride;
        samplerInfo.lodStride[2] = srcSurf->vStride;
    }
    else if (samplerInfo.lodNum == 2)
    {
        samplerInfo.lodAddr[1] = srcSurf->node.physicalBottom;
    }

    samplerInfo.hasTileStatus = !srcSurf->tileStatusDisabled;
    samplerInfo.tileStatusAddr = srcSurf->tileStatusNode.physical;
    samplerInfo.tileStatusClearValue = srcSurf->fcValue;
    samplerInfo.tileStatusClearValueUpper = srcSurf->fcValueUpper;
    samplerInfo.compressed = srcSurf->compressed;
    samplerInfo.compressedFormat = srcSurf->compressFormat;

    samplerInfo.texType = gcvTEXTURE_2D;
    samplerInfo.textureInfo = &texParamInfo;
    texParamInfo.anisoFilter = 1;
    texParamInfo.baseLevel = 0;
    texParamInfo.compareMode = gcvTEXTURE_COMPARE_MODE_NONE;
    texParamInfo.lodBias = 0;
    texParamInfo.lodMin = 0;
    texParamInfo.lodMax = 0;
    texParamInfo.mipFilter = gcvTEXTURE_NONE;
    texParamInfo.minFilter =
    texParamInfo.magFilter = blitArg->filterMode;
    texParamInfo.maxLevel = 0;
    gcoOS_MemCopy(&texParamInfo.swizzle, baseComponents_rgba, sizeof(baseComponents_rgba));
    texParamInfo.r =
    texParamInfo.s =
    texParamInfo.t = gcvTEXTURE_CLAMP;

    samplerInfo.unsizedDepthTexture = gcvTRUE;

    for (i = 0; i < srcFmtInfo->layers; i++)
    {
        samplerInfo.lodAddr[0] += i * srcSurf->layerSize;

        if (samplerInfo.lodNum == 2)
        {
            samplerInfo.lodAddr[1] += i * srcSurf->layerSize;
        }

        gcmONERROR(gcoHARDWARE_BindTexture(hardware, i, &samplerInfo));
    }

    for (i = 0; i < dstFmtInfo->layers; i++)
    {
        gcmONERROR(gcoHARDWARE_SetRenderTarget(hardware, i, dstSurf, i));

        gcmONERROR(gcoHARDWARE_SetRenderTargetOffset(hardware, i, dstSurf->offset));
    }


    gcmONERROR(gcoHARDWARE_EnableTileStatus(hardware,
                                            dstSurf,
                                            (dstSurf->tileStatusNode.pool != gcvPOOL_UNKNOWN) ?
                                            dstSurf->tileStatusNode.physical : 0,
                                            &dstSurf->hzTileStatusNode,
                                            0));


    gcmONERROR(gcoHARDWARE_SetDepthMode(hardware, gcvDEPTH_NONE));
    gcmONERROR(gcoHARDWARE_SetDepthOnly(hardware, gcvFALSE));

    gcmONERROR(gcoHARDWARE_SetColorWrite(hardware, 0xff));
    gcmONERROR(gcoHARDWARE_SetColorOutCount(hardware, dstFmtInfo->layers));

    gcmONERROR(gcoHARDWARE_SetCulling(hardware, gcvCULL_NONE));
    gcmONERROR(gcoHARDWARE_SetFill(hardware, gcvFILL_SOLID));

    gcmONERROR(_PickBlitDrawShader(hardware,
                                   gcvBLITDRAW_BLIT,
                                   srcFmtInfo,
                                   dstFmtInfo,
                                   &programState));

    gcmONERROR(gcoHARDWARE_LoadShaders(hardware,
                                       programState->programSize,
                                       programState->programBuffer,
                                       programState->hints));

    gcmONERROR(gcoHARDWARE_SetViewport(hardware,
                                       dstSurf->rect.left / dstSurf->samples.x,
                                       dstSurf->rect.bottom / dstSurf->samples.y,
                                       dstSurf->rect.right / dstSurf->samples.x,
                                       dstSurf->rect.top / dstSurf->samples.y));
    if (blitArg->scissorEnabled)
    {
        gcmONERROR(
            gcoHARDWARE_SetScissors(hardware,
                                    gcmMAX(blitArg->scissor.left, blitArg->dstRect.left),
                                    gcmMAX(blitArg->scissor.top, blitArg->dstRect.top),
                                    gcmMIN(blitArg->scissor.right, blitArg->dstRect.right),
                                    gcmMIN(blitArg->scissor.bottom, blitArg->dstRect.bottom)));
    }
    else
    {
        gcmONERROR(
            gcoHARDWARE_SetScissors(hardware,
                                    gcmMAX(dstSurf->rect.left / dstSurf->samples.x, blitArg->dstRect.left),
                                    gcmMAX(dstSurf->rect.top / dstSurf->samples.y, blitArg->dstRect.top),
                                    gcmMIN(dstSurf->rect.right / dstSurf->samples.x, blitArg->dstRect.right),
                                    gcmMIN(dstSurf->rect.bottom / dstSurf->samples.y, blitArg->dstRect.bottom)));
    }

    /* Set up vertex stream.*/
    do
    {
        gcsVERTEXARRAY_BUFOBJ streamPos, streamTexCoord;
        gcsVERTEXARRAY_BUFOBJ_PTR streamPosPtr = &streamPos;
        gcsVERTEXARRAY_BUFOBJ_PTR streamTexCoordPtr = &streamTexCoord;
        gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE attribPos, attribTexCoord;
        gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR attribPosPtr = &attribPos;
        gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR attribTexCoordPtr = &attribTexCoord;

        gctFLOAT positionData[3 * 2];
        gctFLOAT texCoordData[3 * 2];

        gctUINT srcWidth, srcHeight, dstWidth, dstHeight;
        /* Rectangle coordinates. */
        gctFLOAT left, top, right, bottom;

        /* Set pos and texcoord data.*/
        gcmONERROR(gcoSURF_GetSize(blitArg->srcSurface, &srcWidth, &srcHeight, gcvNULL));
        gcmONERROR(gcoSURF_GetSize(blitArg->dstSurface, &dstWidth, &dstHeight, gcvNULL));

        /* Transform coordinates. */
        left   = 2.0f * blitArg->dstRect.left   / dstWidth  - 1.0f;
        top    = 2.0f * blitArg->dstRect.top    / dstHeight - 1.0f;
        right  = 2.0f * (2.0f * blitArg->dstRect.right - blitArg->dstRect.left) / dstWidth  - 1.0f;
        bottom = 2.0f * (2.0f * blitArg->dstRect.bottom - blitArg->dstRect.top)/ dstHeight - 1.0f;

        /* Create 2 triangles in a strip. */
        positionData[0] = left;
        positionData[1] = top;

        positionData[2] = right;
        positionData[3] = top;

        positionData[4] = left;
        positionData[5] = bottom;

        /* Normalize coordinates.*/
        left   = (gctFLOAT)blitArg->srcRect.left   / srcWidth;
        top    = (gctFLOAT)blitArg->srcRect.top    / srcHeight;
        right  = (2 *(gctFLOAT)blitArg->srcRect.right - (float)blitArg->srcRect.left)  / srcWidth;
        bottom = (2* (gctFLOAT)blitArg->srcRect.bottom - (float)blitArg->srcRect.top)/ srcHeight;

        if (blitArg->xReverse)
        {
            left  = (gctFLOAT)blitArg->srcRect.right  / srcWidth;
            right   = (2 * (gctFLOAT)blitArg->srcRect.left - (float)blitArg->srcRect.right) / srcWidth;
        }

        if (blitArg->yReverse)
        {
            top    = (gctFLOAT)blitArg->srcRect.bottom / srcHeight;
            bottom = (2 * (gctFLOAT)blitArg->srcRect.top - (float)blitArg->srcRect.bottom) / srcHeight;
        }

        /* Create two triangles. */
        texCoordData[0] = left;
        texCoordData[1] = top;

        texCoordData[2] = right;
        texCoordData[3] = top;

        texCoordData[4] = left;
        texCoordData[5] = bottom;

        gcoOS_ZeroMemory(streamPosPtr, sizeof(gcsVERTEXARRAY_BUFOBJ));
        gcoOS_ZeroMemory(streamTexCoordPtr, sizeof(gcsVERTEXARRAY_BUFOBJ));
        gcoOS_ZeroMemory(attribPosPtr, sizeof(gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE));
        gcoOS_ZeroMemory(attribTexCoordPtr, sizeof(gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE));

        /* set pos steam. */
        streamPosPtr->stride         = 0x8;
        streamPosPtr->divisor        = 0x0;
        streamPosPtr->count          = 0x3;
        /* vertex and texcoord.*/
        streamPosPtr->attributeCount = 0x1;

        /* set attrib.*/
        attribPosPtr->bytes      = 0x8;
        attribPosPtr->format     = gcvVERTEX_FLOAT;
        attribPosPtr->linkage    = 0x0; /* Pos.*/
        attribPosPtr->size       = 0x2;
        attribPosPtr->normalized = 0x0;
        attribPosPtr->offset     = gcmPTR2INT32(positionData);
        attribPosPtr->enabled    = gcvTRUE;
        attribPosPtr->isPosition = 0x0;
        attribPosPtr->stride     = 0x8;
        attribPosPtr->pointer    = &positionData;

        streamPosPtr->attributePtr = attribPosPtr;

        /* Set tex coord stream.*/
        streamTexCoordPtr->stride         = 0x8;
        streamTexCoordPtr->divisor        = 0x0;
        streamTexCoordPtr->count          = 0x3;
        streamTexCoordPtr->attributeCount = 0x1;

        /* set attrib.*/
        attribTexCoordPtr->bytes      = 0x8;
        attribTexCoordPtr->format     = gcvVERTEX_FLOAT;
        attribTexCoordPtr->linkage    = 0x1; /* tex coord */
        attribTexCoordPtr->size       = 0x2;
        attribTexCoordPtr->normalized = 0x0;
        attribTexCoordPtr->offset     = gcmPTR2INT32(texCoordData);
        attribTexCoordPtr->enabled    = gcvTRUE;
        attribTexCoordPtr->isPosition = 0x0;
        attribTexCoordPtr->stride     = 0x8;
        attribTexCoordPtr->pointer    = &texCoordData;

        streamTexCoordPtr->attributePtr = attribTexCoordPtr;
        streamTexCoordPtr->next = streamPosPtr;

        gcmONERROR(
            gcoSTREAM_CacheAttributesEx(blitDraw->dynamicStream,
                                        0x2,
                                        streamTexCoordPtr,
                                        0x0,
                                        gcvNULL));

        gcmONERROR(gcoHARDWARE_SetVertexArrayEx(gcvNULL,
                                                1,
                                                0,
                                                2,
                                                streamTexCoordPtr,
                                                0,
                                                0,
                                                0xffffffff
                                                ));

        if (hardware->features[gcvFEATURE_HALTI0])
        {
            /* Draw command.*/
            gcmONERROR(
                gcoHARDWARE_DrawInstancedPrimitives(hardware,
                                                    0,
                                                    gcvPRIMITIVE_TRIANGLE_STRIP,
                                                    0,
                                                    0,
                                                    1,
                                                    3,
                                                    1));
        }
        else
        {
            /* Draw command.*/
            gcmONERROR(
                gcoHARDWARE_DrawPrimitives(hardware,
                                           gcvPRIMITIVE_TRIANGLE_STRIP,
                                           0,
                                           1));
        }

        /* reset rt.*/
        for (i = 0; i < dstFmtInfo->layers; i++)
        {
            gcmONERROR(gcoHARDWARE_DisableTileStatus(hardware, dstSurf, gcvFALSE));
            gcmONERROR(gcoHARDWARE_SetRenderTarget(hardware, i, gcvNULL, i));
        }

    }
    while(gcvFALSE);

OnError:
    return status;
}


gceSTATUS
gcoHARDWARE_BlitDraw(
    IN gcsSURF_BLITDRAW_ARGS *args
    )
{
    gceSTATUS status;
    gcoHARDWARE savedHardware = gcvNULL;

    gcmHEADER_ARG("args=0x%x", args);

    gcmVERIFY_ARGUMENT(args != gcvNULL);

    /* Save current hardware */
    gcmONERROR(gcoHARDWARE_Get3DHardware(&savedHardware));

    switch (args->uArgs.v1.type)
    {
    case gcvBLITDRAW_CLEAR:
        gcmONERROR(_BlitDrawClear(args));
        break;

    case gcvBLITDRAW_BLIT:
        gcmONERROR(_BlitDrawBlit(args));
        break;

    default:
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        break;
    }

OnError:

    if (savedHardware)
    {
        gcmVERIFY_OK(gcoHARDWARE_Set3DHardware(savedHardware));
    }

    gcmFOOTER_NO();
    return status;
}


#endif

#if gcdMULTI_GPU
gceSTATUS
gcoHARDWARE_SetChipEnable(
    IN gcoHARDWARE Hardware,
    IN gceCORE_3D_MASK ChipEnable
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x ChipEnable=%d",
                  Hardware, ChipEnable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->chipEnable = ChipEnable;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetChipEnable(
    IN gcoHARDWARE Hardware,
    OUT gceCORE_3D_MASK *ChipEnable
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x ChipEnable=%d",
                  Hardware, ChipEnable);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    *ChipEnable = Hardware->chipEnable;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_SetMultiGPUMode(
    IN gcoHARDWARE Hardware,
    IN gceMULTI_GPU_MODE MultiGPUMode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x MultiGPUMode=%d",
                  Hardware, MultiGPUMode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    Hardware->gpuMode = MultiGPUMode;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_GetMultiGPUMode(
    IN gcoHARDWARE Hardware,
    OUT gceMULTI_GPU_MODE *MultiGPUMode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Mode=%d",
                  Hardware, MultiGPUMode);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    *MultiGPUMode = Hardware->gpuMode;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

gceSTATUS
gcoHARDWARE_GetProductName(
    IN gcoHARDWARE Hardware,
    IN OUT gctSTRING *ProductName
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctSTRING chipName;
    gctUINT i;
    gctBOOL foundID = gcvFALSE;
    gctUINT32 chipID;
    gctPOINTER pointer;

    gcmHEADER_ARG("Hardware=0x%x ProductName=0x%x", Hardware, ProductName);
    gcmVERIFY_ARGUMENT(ProductName != gcvNULL);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmONERROR(gcoOS_Allocate(gcvNULL, 32, &pointer));

    gcoOS_ZeroMemory(pointer, 32);

    chipName = (gctSTRING) pointer;

    if (Hardware->features[gcvFEATURE_HAS_PRODUCTID])
    {
        gctUINT32 productID = Hardware->config->productID;
        gctUINT32 type = (((((gctUINT32) (productID)) >> (0 ? 27:24)) & ((gctUINT32) ((((1 ? 27:24) - (0 ? 27:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:24) - (0 ? 27:24) + 1)))))) );
        gctUINT32 grade = (((((gctUINT32) (productID)) >> (0 ? 3:0)) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1)))))) );

        chipID = (((((gctUINT32) (productID)) >> (0 ? 23:4)) & ((gctUINT32) ((((1 ? 23:4) - (0 ? 23:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:4) - (0 ? 23:4) + 1)))))) );

        switch (type)
        {
        case 0:
            *chipName++ = 'G';
            *chipName++ = 'C';
            break;
        case 1:
            *chipName++ = 'D';
            *chipName++ = 'E';
            *chipName++ = 'C';
            break;
        case 2:
            *chipName++ = 'D';
            *chipName++ = 'C';
            break;
        case 3:
            *chipName++ = 'V';
            *chipName++ = 'G';
            break;
        case 4:
            *chipName++ = 'S';
            *chipName++ = 'C';
            break;
        default:
            *chipName++ = '?';
            *chipName++ = '?';
            gcmPRINT("GAL: Invalid product type");
            break;
        }

        /* Translate the ID. */
        for (i = 0; i < 8; i++)
        {
            /* Get the current digit. */
            gctUINT8 digit = (gctUINT8) ((chipID >> 28) & 0xFF);

            /* Append the digit to the string. */
            if (foundID || digit)
            {
                *chipName++ = '0' + digit;
                foundID = gcvTRUE;
            }

            /* Advance to the next digit. */
            chipID <<= 4;
        }

        switch (grade)
        {
        case 0:             /* Normal id */
        default:
            break;

        case 1:
            *chipName++ = 'N'; /* Nano */
            break;

        case 2:
            *chipName++ = 'L'; /* Lite */
            break;

        case 3:
            *chipName++ = 'U'; /* Ultra Lite */
            *chipName++ = 'L';
            break;

        case 4:
            *chipName++ = 'X'; /* TS/GS core */
            *chipName++ = 'S';
            break;

        case 5:
            *chipName++ = 'N'; /* Nano Ultra */
            *chipName++ = 'U';
            break;

        case 6:
            *chipName++ = 'N'; /* Nano Lite */
            *chipName++ = 'L';
            break;
        }

    }
    else
    {
        chipID = Hardware->config->chipModel;

        *chipName++ = 'G';
        *chipName++ = 'C';

        /* Translate the ID. */
        for (i = 0; i < 8; i++)
        {
            /* Get the current digit. */
            gctUINT8 digit = (gctUINT8) ((chipID >> 28) & 0xFF);

            /* Append the digit to the string. */
            if (foundID || digit)
            {
                *chipName++ = '0' + digit;
                foundID = gcvTRUE;
            }

            /* Advance to the next digit. */
            chipID <<= 4;
        }

    }

    gcoOS_StrDup(gcvNULL, pointer, ProductName);

    gcmOS_SAFE_FREE(gcvNULL, pointer);

OnError:
    gcmFOOTER();
    return status;
}




