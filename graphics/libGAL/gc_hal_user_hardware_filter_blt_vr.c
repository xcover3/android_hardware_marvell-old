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


#if gcdENABLE_2D
#include "gc_hal_user.h"

/******************************************************************************\
***************************** Filter Blit Defines *****************************
\******************************************************************************/

typedef enum
{
    gceFILTER_BLIT_TYPE_VERTICAL,
    gceFILTER_BLIT_TYPE_HORIZONTAL,
    gceFILTER_BLIT_TYPE_ONE_PASS,
}
gceFILTER_BLIT_TYPE;

/******************************************************************************\
********************************* Support Code *********************************
\******************************************************************************/

/*******************************************************************************
**
**  _SincFilter
**
**  Sinc filter function.
**
**  INPUT:
**
**      gctFLOAT x
**          x coordinate.
**
**      gctINT32 radius
**          Radius of the filter.
**
**  OUTPUT:
**
**      Nothing.
**
**  RETURN:
**
**      gctFLOAT
**          Function value at x.
*/
static gctFLOAT _SincFilter(
    gctFLOAT x,
    gctINT32 radius
    )
{
    gctFLOAT pit, pitd, f1, f2, result;
    gctFLOAT fRadius = gcoMATH_Int2Float(radius);

    if (x == 0.0f)
    {
        result = 1.0f;
    }
    else if ((x < -fRadius) || (x > fRadius))
    {
        result = 0.0f;
    }
    else
    {
        pit  = gcoMATH_Multiply(gcdPI, x);
        pitd = gcoMATH_Divide(pit, fRadius);

        f1 = gcoMATH_Divide(gcoMATH_Sine(pit), pit);
        f2 = gcoMATH_Divide(gcoMATH_Sine(pitd), pitd);

        result = gcoMATH_Multiply(f1, f2);
    }

    return result;
}

/*******************************************************************************
**
**  _CalculateSyncTable
**
**  Calculate weight array for sync filter.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctUINT8 KernelSize
**          New kernel size.
**
**      gctUINT32 SrcSize
**          The size in pixels of a source dimension (width or height).
**
**      gctUINT32 DestSize
**          The size in pixels of a destination dimension (width or height).
**
**  OUTPUT:
**
**      gcsFILTER_BLIT_ARRAY_PTR KernelInfo
**          Updated kernel structure and table.
*/
static gceSTATUS _CalculateSyncTable(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 KernelSize,
    IN gctUINT32 SrcSize,
    IN gctUINT32 DestSize,
    OUT gcsFILTER_BLIT_ARRAY_PTR KernelInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x KernelSize=%u SrcSize=%u DestSize=%u "
                  "KernelInfo=0x%x",
                  Hardware, KernelSize, SrcSize, DestSize, KernelInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(KernelInfo != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(KernelInfo->filterType == gcvFILTER_SYNC);
    gcmDEBUG_VERIFY_ARGUMENT(SrcSize != 0);
    gcmDEBUG_VERIFY_ARGUMENT(DestSize != 0);

    do
    {
        gctUINT32 scaleFactor;
        gctFLOAT fScale;
        gctINT kernelHalf;
        gctFLOAT fSubpixelStep;
        gctFLOAT fSubpixelOffset;
        gctUINT32 subpixelPos;
        gctINT kernelPos;
        gctINT padding;
        gctUINT16_PTR kernelArray;
        gctPOINTER pointer = gcvNULL;

        /* Compute the scale factor. */
        scaleFactor = gcoHARDWARE_GetStretchFactor(gcvFALSE, SrcSize, DestSize);

        /* Same kernel size and ratio as before? */
        if ((KernelInfo->kernelSize  == KernelSize) &&
            (KernelInfo->scaleFactor == scaleFactor))
        {
            break;
        }

        /* Allocate the array if not allocated yet. */
        if (KernelInfo->kernelStates == gcvNULL)
        {
            /* Allocate the array. */
            gcmERR_BREAK(
                gcoOS_Allocate(gcvNULL,
                               gcvKERNELSTATES,
                               &pointer));

            KernelInfo->kernelStates = pointer;
        }

        /* Store new parameters. */
        KernelInfo->kernelSize  = KernelSize;
        KernelInfo->scaleFactor = scaleFactor;

        /* Compute the scale factor. */
        fScale = gcoMATH_DivideFromUInteger(DestSize, SrcSize);

        /* Adjust the factor for magnification. */
        if (fScale > 1.0f)
        {
            fScale = 1.0f;
        }

        /* Calculate the kernel half. */
        kernelHalf = (gctINT) (KernelInfo->kernelSize >> 1);

        /* Calculate the subpixel step. */
        fSubpixelStep = gcoMATH_Divide(1.0f,
                                       gcoMATH_Int2Float(gcvSUBPIXELCOUNT));

        /* Init the subpixel offset. */
        fSubpixelOffset = 0.5f;

        /* Determine kernel padding size. */
        padding = (gcvMAXKERNELSIZE - KernelInfo->kernelSize) / 2;

        /* Set initial kernel array pointer. */
        kernelArray = (gctUINT16_PTR) (KernelInfo->kernelStates + 1);

        /* Loop through each subpixel. */
        for (subpixelPos = 0; subpixelPos < gcvSUBPIXELLOADCOUNT; subpixelPos++)
        {
            /* Define a temporary set of weights. */
            gctFLOAT fSubpixelSet[gcvMAXKERNELSIZE];

            /* Init the sum of all weights for the current subpixel. */
            gctFLOAT fWeightSum = 0.0f;
            gctUINT16 weightSum = 0;
            gctINT16 adjustCount, adjustFrom;
            gctINT16 adjustment;

            /* Compute weights. */
            for (kernelPos = 0; kernelPos < gcvMAXKERNELSIZE; kernelPos++)
            {
                /* Determine the current index. */
                gctINT index = kernelPos - padding;

                /* Pad with zeros. */
                if ((index < 0) || (index >= KernelInfo->kernelSize))
                {
                    fSubpixelSet[kernelPos] = 0.0f;
                }
                else
                {
                    if (KernelInfo->kernelSize == 1)
                    {
                        fSubpixelSet[kernelPos] = 1.0f;
                    }
                    else
                    {
                        /* Compute the x position for filter function. */
                        gctFLOAT fX =
                            gcoMATH_Multiply(
                                gcoMATH_Add(
                                    gcoMATH_Int2Float(index - kernelHalf),
                                    fSubpixelOffset),
                                fScale);

                        /* Compute the weight. */
                        fSubpixelSet[kernelPos] = _SincFilter(fX, kernelHalf);
                    }

                    /* Update the sum of weights. */
                    fWeightSum = gcoMATH_Add(fWeightSum,
                                             fSubpixelSet[kernelPos]);
                }
            }

            /* Adjust weights so that the sum will be 1.0. */
            for (kernelPos = 0; kernelPos < gcvMAXKERNELSIZE; kernelPos++)
            {
                /* Normalize the current weight. */
                gctFLOAT fWeight = gcoMATH_Divide(fSubpixelSet[kernelPos],
                                                  fWeightSum);

                /* Convert the weight to fixed point and store in the table. */
                if (fWeight == 0.0f)
                {
                    kernelArray[kernelPos] = 0x0000;
                }
                else if (fWeight >= 1.0f)
                {
                    kernelArray[kernelPos] = 0x4000;
                }
                else if (fWeight <= -1.0f)
                {
                    kernelArray[kernelPos] = 0xC000;
                }
                else
                {
                    kernelArray[kernelPos] = (gctINT16)
                        gcoMATH_Multiply(fWeight, 16384.0f);
                }

                weightSum += kernelArray[kernelPos];
            }

            /* Adjust the fixed point coefficients. */
            adjustCount = 0x4000 - weightSum;
            if (adjustCount < 0)
            {
                adjustCount = -adjustCount;
                adjustment = -1;
            }
            else
            {
                adjustment = 1;
            }

            adjustFrom = (gcvMAXKERNELSIZE - adjustCount) / 2;

            for (kernelPos = 0; kernelPos < adjustCount; kernelPos++)
            {
                kernelArray[adjustFrom + kernelPos] += adjustment;
            }

            kernelArray += gcvMAXKERNELSIZE;

            /* Advance to the next subpixel. */
            fSubpixelOffset = gcoMATH_Add(fSubpixelOffset, -fSubpixelStep);
        }

        KernelInfo->kernelChanged = gcvTRUE;
    }
    while (gcvFALSE);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  _CalculateBlurTable
**
**  Calculate weight array for blur filter.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctUINT8 KernelSize
**          New kernel size.
**
**      gctUINT32 SrcSize
**          The size in pixels of a source dimension (width or height).
**
**      gctUINT32 DestSize
**          The size in pixels of a destination dimension (width or height).
**
**  OUTPUT:
**
**      gcsFILTER_BLIT_ARRAY_PTR KernelInfo
**          Updated kernel structure and table.
*/
static gceSTATUS _CalculateBlurTable(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 KernelSize,
    IN gctUINT32 SrcSize,
    IN gctUINT32 DestSize,
    OUT gcsFILTER_BLIT_ARRAY_PTR KernelInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x KernelSize=%u SrcSize=%u DestSize=%u "
                  "KernelInfo=0x%x",
                  Hardware, KernelSize, SrcSize, DestSize, KernelInfo);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(KernelInfo != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(KernelInfo->filterType == gcvFILTER_BLUR);
    gcmDEBUG_VERIFY_ARGUMENT(SrcSize != 0);
    gcmDEBUG_VERIFY_ARGUMENT(DestSize != 0);

    do
    {
        gctUINT32 scaleFactor;
        gctUINT32 subpixelPos;
        gctINT kernelPos;
        gctINT padding;
        gctUINT16_PTR kernelArray;
        gctPOINTER pointer = gcvNULL;

        /* Compute the scale factor. */
        scaleFactor = gcoHARDWARE_GetStretchFactor(gcvFALSE, SrcSize, DestSize);

        /* Same kernel size and ratio as before? */
        if ((KernelInfo->kernelSize  == KernelSize) &&
            (KernelInfo->scaleFactor == scaleFactor))
        {
            break;
        }

        /* Allocate the array if not allocated yet. */
        if (KernelInfo->kernelStates == gcvNULL)
        {
            /* Allocate the array. */
            gcmERR_BREAK(
                gcoOS_Allocate(gcvNULL,
                               gcvKERNELSTATES,
                               &pointer));

            KernelInfo->kernelStates = pointer;
        }

        /* Store new parameters. */
        KernelInfo->kernelSize  = KernelSize;
        KernelInfo->scaleFactor = scaleFactor;

        /* Determine kernel padding size. */
        padding = (gcvMAXKERNELSIZE - KernelInfo->kernelSize) / 2;

        /* Set initial kernel array pointer. */
        kernelArray = (gctUINT16_PTR) (KernelInfo->kernelStates + 1);

        /* Loop through each subpixel. */
        for (subpixelPos = 0; subpixelPos < gcvSUBPIXELLOADCOUNT; subpixelPos++)
        {
            /* Compute weights. */
            for (kernelPos = 0; kernelPos < gcvMAXKERNELSIZE; kernelPos++)
            {
                /* Determine the current index. */
                gctINT index = kernelPos - padding;

                /* Pad with zeros. */
                if ((index < 0) || (index >= KernelInfo->kernelSize))
                {
                    *kernelArray++ = 0x0000;
                }
                else
                {
                    if (KernelInfo->kernelSize == 1)
                    {
                        *kernelArray++ = 0x4000;
                    }
                    else
                    {
                        gctFLOAT fWeight;

                        /* Compute the weight. */
                        fWeight = gcoMATH_Divide(
                            1.0f,
                            gcoMATH_UInt2Float(KernelInfo->kernelSize));
                        *kernelArray++ = (gctINT16) gcoMATH_Multiply(fWeight,
                                                                     16384.0f);
                    }
                }
            }
        }

        KernelInfo->kernelChanged = gcvTRUE;
    }
    while (gcvFALSE);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  _LoadKernel
**
**  Program kernel size and kernel weight table.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsFILTER_BLIT_ARRAY_PTR Kernel
**          Pointer to kernel array info structure.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS _LoadKernel(
    IN gcoHARDWARE Hardware,
    IN gceFILTER_BLIT_KERNEL type,
    IN gcsFILTER_BLIT_ARRAY_PTR Kernel
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Kernel=0x%x", Hardware, Kernel);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(type < gcvFILTER_BLIT_KERNEL_NUM);
    gcmDEBUG_VERIFY_ARGUMENT(Hardware->loadedKernel[type].kernelAddress != (gctUINT32) ~0);

    do
    {
        /* LoadState(AQDE_FILTER_KERNEL) */
        if (Hardware->bigEndian)
        {
            gctUINT32 data[gcvWEIGHTSTATECOUNT];
            gctUINT32 i;
            gctUINT32_PTR p;

            p = Kernel->kernelStates + 1;

            for (i = 0; i < gcmCOUNTOF(data); i++)
            {
                data[i] = (p[i] << 16) | (p[i] >> 16);
            }

            gcmERR_BREAK(gcoHARDWARE_Load2DState(
                Hardware,
                Hardware->loadedKernel[type].kernelAddress, gcvWEIGHTSTATECOUNT,
                data
                ));
        }
        else
        {
            gcmERR_BREAK(gcoHARDWARE_Load2DState(
                Hardware,
                Hardware->loadedKernel[type].kernelAddress, gcvWEIGHTSTATECOUNT,
                Kernel->kernelStates + 1
                ));
        }

        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

    /* Return status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS _CheckOPF(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gcs2D_State_PTR State,
    OUT gctBOOL_PTR UseOPF
    )
{
    gctBOOL res = gcvFALSE;

    if (SrcSurface->rotation != gcvSURF_0_DEGREE)
    {
        res = gcvFALSE;
    }
    else if (Hardware->features[gcvFEATURE_2D_ONE_PASS_FILTER_TAP] &&
             SrcSurface->tiling == gcvLINEAR)
    {
        res = gcvTRUE;
    }
    else if (Hardware->hw2DQuad &&
             (State->newHorKernelSize == 3 || State->newHorKernelSize == 5) &&
             (State->newVerKernelSize == 3 || State->newVerKernelSize == 5))
    {
        res = gcvTRUE;
    }
    else if (Hardware->features[gcvFEATURE_ANDROID_ONLY] &&
             State->newHorKernelSize != 7 && State->newHorKernelSize != 9 &&
             State->newVerKernelSize != 7 && State->newVerKernelSize != 9)
    {
        res = gcvTRUE;
    }

    if (UseOPF != gcvNULL)
        *UseOPF = res;

    return gcvSTATUS_OK;
}

static gceSTATUS _CheckOPFBlock(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gcs2D_State_PTR State,
    OUT gctBOOL_PTR UseOPFBlock
    )
{
    gctBOOL res = gcvFALSE;
    gctUINT32 srcBPP, dstBPP;

#if gcdENABLE_THIRD_PARTY_OPERATION
    if (Hardware->features[gcvFEATURE_TPC_COMPRESSION] &&
        (DstSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED ||
        SrcSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED))
    {
        res = gcvTRUE;
    }
    else
#endif
    if (Hardware->features[gcvFEATURE_ANDROID_ONLY])
    {
        res = gcvFALSE;
    }
    else if (Hardware->features[gcvFEATURE_2D_ALL_QUAD])
    {
        if (DstSurface->tiling == gcvLINEAR &&
            SrcSurface->tiling == gcvLINEAR)
        {
            gcoHARDWARE_ConvertFormat(SrcSurface->format, &srcBPP, gcvNULL);
            gcoHARDWARE_ConvertFormat(DstSurface->format, &dstBPP, gcvNULL);

            switch (SrcSurface->format)
            {
                case gcvSURF_NV12:
                case gcvSURF_NV21:
                case gcvSURF_NV16:
                case gcvSURF_NV61:
                case gcvSURF_YV12:
                case gcvSURF_I420:
                    if (dstBPP == 16 || dstBPP == 32)
                        res = gcvTRUE;
                    break;

                case gcvSURF_UYVY:
                case gcvSURF_YUY2:
                case gcvSURF_VYUY:
                case gcvSURF_YVYU:
                    res = gcvFALSE;
                    break;

                default:
                    if ((srcBPP == 16 || srcBPP == 32) &&
                        (dstBPP == 16 || dstBPP == 32))
                        res = gcvTRUE;
                    break;
            }
        }
    }

    if (UseOPFBlock != gcvNULL)
        *UseOPFBlock = res;

    return gcvSTATUS_OK;
}

 /*******************************************************************************
 **
**  _SetOPFBlockSize
**
**  Set bolck size for one-pass-filter.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 HorTap/VerTap
**          Set to tap value.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS _SetOPFBlockSize(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DstRect,
    IN gcsFILTER_BLIT_ARRAY_PTR HorKernel,
    IN gcsFILTER_BLIT_ARRAY_PTR VerKernel,
    IN gctUINT32 HorTap,
    IN gctUINT32 VerTap)
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 vfactor, srcBPP, destBPP, tap, comb = 0;
    gctUINT32 fvalue, adjust;
    gctUINT32 configEx = ~0U;
    gceSURF_ROTATION dstRot;
    gctBOOL srcYUV420 = gcvFALSE;
    gctUINT32 width = 0, height = 0;
    gctUINT32 horizontal = 0xFFFF, vertical = 0xFFFF;
    gcsOPF_BLOCKSIZE_TABLE_PTR table;

    do
    {
        gcmONERROR(gcoHARDWARE_ConvertFormat(SrcSurface->format, &srcBPP, gcvNULL));
        gcmONERROR(gcoHARDWARE_ConvertFormat(DstSurface->format, &destBPP, gcvNULL));

        tap = gcmMIN(HorTap, VerTap);
        dstRot = DstSurface->rotation;

        switch (SrcSurface->format)
        {
            case gcvSURF_NV12:
            case gcvSURF_NV21:
            case gcvSURF_NV16:
            case gcvSURF_NV61:
            case gcvSURF_YV12:
            case gcvSURF_I420:
                srcYUV420 = gcvTRUE;
                break;

            default:
                srcYUV420 = gcvFALSE;
                break;
        }

        vfactor = gcoHARDWARE_GetStretchFactor(
                    gcvTRUE,
                    SrcRect->bottom - SrcRect->top,
                    DstRect->bottom - DstRect->top);

        adjust = vfactor & 0x1FFF;
        fvalue = (vfactor & 0x7E000) >> 13;
        if ((vfactor >> 16) >= 8)
            fvalue = 0x3F;
        else if (adjust != 0 && fvalue < 0x3F)
            fvalue += 0x1;

        if (!fvalue)
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);

        /* Need to revert fvalue to get info from driver table. */
        fvalue = 0x3F - fvalue;

        if (dstRot == gcvSURF_90_DEGREE || dstRot == gcvSURF_270_DEGREE)
            comb |= 0x1;
        if (tap == 5)
            comb |= (0x1 << 1);
        if (destBPP == 32)
            comb |= (0x1 << 2);
        if (srcYUV420)
            comb |= (0x2 << 3);
        else if (srcBPP == 32)
            comb |= (0x1 << 3);

        table = gcsOPF_TABLE_TABLE_ONEPIPE[comb];

        configEx = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))));

#if gcdENABLE_THIRD_PARTY_OPERATION
        if (Hardware->features[gcvFEATURE_TPC_COMPRESSION] &&
            (DstSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED ||
             SrcSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED))
        {
            configEx &= (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))) &
                         (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)))) &
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))));

            width = height = 16;
            horizontal = vertical = 0xFFF0;
        }
        else
#endif
        {
            if (table[fvalue].blockDirect)
            {
                configEx &= (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))) &
                             (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))));
            }
            else
            {
                configEx &= (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)))) &
                             (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))));
            }
            if (table[fvalue].pixelDirect)
                configEx &= (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))));
            else
                configEx &= (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))));

            width = table[fvalue].width;
            height = table[fvalue].height;
            horizontal = table[fvalue].horizontal;
            vertical = table[fvalue].vertical;

            /* width under DUAL PIPE should be even number */
            if (Hardware->features[gcvFEATURE_2D_OPF_YUV_OUTPUT])
            {
                if (width & 0x1)
                {
                    width = (width == 1 ? 2 : width - 1);
                }
            }
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012F0,
            configEx)
            );

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012F4,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (width) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (height) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))))
            );

        gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012FC,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (horizontal) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) |
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (vertical) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))))
                );
    }
    while (gcvFALSE);

OnError:
    /* Return status. */
    return status;
}

/*******************************************************************************
**
**  _StartVR
**
**  Start video raster engine.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL Horizontal
**          Set to gcvTRUE to start horizontal blit.
**          Set to gcvFALSE to start vertical blit.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS _StartVR(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gceFILTER_BLIT_TYPE type,
    IN gcsFILTER_BLIT_ARRAY_PTR HorKernel,
    IN gcsFILTER_BLIT_ARRAY_PTR VerKernel,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gcsRECT_PTR DstRect,
    IN gctBOOL PrePass
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    do
    {
        gcs2D_MULTI_SOURCE_PTR curSrc = &State->multiSrc[State->currentSrcIndex];
        gctUINT32 blitType = 0, getCmdSize = 0;
        gctBOOL inCSC = gcvFALSE, outCSC = gcvFALSE, anyCompress;

        Hardware->enableXRGB = State->enableXRGB;

        Hardware->hw2DCmdBuffer = gcvNULL;
        Hardware->hw2DCmdSize = 0;
        Hardware->hw2DCmdIndex = (type == gceFILTER_BLIT_TYPE_ONE_PASS)? 270 : 170;

        if (Hardware->hw3DEngine)
        {
            Hardware->hw2DCmdIndex += 16;
        }

        if (Hardware->features[gcvFEATURE_2D_FC_SOURCE])
        {
            Hardware->hw2DCmdIndex += 10;
        }

        if (Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
        {
            inCSC = outCSC = gcoHARDWARE_NeedUserCSC(State->dstYUVMode, DstSurface->format);
            inCSC = inCSC || gcoHARDWARE_NeedUserCSC(curSrc->srcYUVMode, curSrc->srcSurface.format);

            if (inCSC)
            {
                Hardware->hw2DCmdIndex += 10;
            }

            if (outCSC)
            {
                Hardware->hw2DCmdIndex += 12;
            }
        }

        if (Hardware->features[gcvFEATURE_2D_GAMMA])
        {
            if (curSrc->srcDeGamma)
            {
                Hardware->hw2DCmdIndex += 258;
            }

            if (State->dstEnGamma)
            {
                Hardware->hw2DCmdIndex += 258;
            }
        }

        gcmONERROR(gcoHARDWARE_GetCompressionCmdSize(
            Hardware,
            State,
            SrcSurface, DstSurface,
            0,
            gcvTRUE,
            &getCmdSize));

        Hardware->hw2DCmdIndex += getCmdSize;

        if (SrcSurface->format == gcvSURF_INDEX8)
        {
            Hardware->hw2DCmdIndex += gcmALIGN(State->paletteIndexCount + 1, 2);
        }

        while (Hardware->hw2DCmdBuffer == gcvNULL)
        {
            gctUINT32 memory[4];

            if (Hardware->hw2DCmdIndex)
            {
                gcoCMDBUF reserve;

                gcmONERROR(gcoBUFFER_Reserve(
                    Hardware->buffer,
                    Hardware->hw2DCmdIndex * gcmSIZEOF(gctUINT32),
                    gcvTRUE,
                    gcvCOMMAND_2D,
                    &reserve
                    ));

                Hardware->hw2DCmdBuffer = gcmUINT64_TO_PTR(reserve->lastReserve);
                Hardware->hw2DCmdSize = Hardware->hw2DCmdIndex;
                Hardware->hw2DCmdIndex = 0;

                reserve->using2D = gcvTRUE;
            }

        /*******************************************************************
        ** Select 2D pipe.
        */
        gcmONERROR(gcoHARDWARE_Begin2DRender(
             Hardware,
             State));

        if (Hardware->features[gcvFEATURE_2D_A8_NO_ALPHA] &&
            SrcSurface->format == gcvSURF_A8 &&
            curSrc->enableAlpha)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012D8,
            (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))))
        ));

        /* Set alphablend states: if filter plus alpha is not supported, make
           sure the alphablend is disabled.
        */
        if (!PrePass && curSrc->enableAlpha)
        {
            if (Hardware->features[gcvFEATURE_2D_FILTERBLIT_PLUS_ALPHABLEND] == gcvFALSE)
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }

            /* Set alphablend states */
            gcmONERROR(gcoHARDWARE_EnableAlphaBlend(
                    Hardware,
                    curSrc->srcAlphaMode,
                    curSrc->dstAlphaMode,
                    curSrc->srcGlobalAlphaMode,
                    curSrc->dstGlobalAlphaMode,
                    curSrc->srcFactorMode,
                    curSrc->dstFactorMode,
                    curSrc->srcColorMode,
                    curSrc->dstColorMode,
                    curSrc->srcGlobalColor,
                    curSrc->dstGlobalColor
                    ));
        }
        else
        {
            /* LoadState(AQDE_ALPHA_CONTROL, AlphaOff). */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0127C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))));
        }

        /* Set mirror state. */
        if (!PrePass)
        {
            if ((!Hardware->features[gcvFEATURE_2D_MIRROR_EXTENSION])
                && (curSrc->horMirror || curSrc->verMirror))
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }

            gcmONERROR(gcoHARDWARE_SetBitBlitMirror(
                Hardware,
                curSrc->horMirror,
                curSrc->verMirror,
                gcvTRUE
                ));

            /* Set multiply modes. */
            gcmONERROR(gcoHARDWARE_SetMultiplyModes(
                Hardware,
                curSrc->srcPremultiplyMode,
                curSrc->dstPremultiplyMode,
                curSrc->srcPremultiplyGlobalMode,
                curSrc->dstDemultiplyMode,
                curSrc->srcGlobalColor
                ));
        }
        else
        {
            gcmONERROR(gcoHARDWARE_SetBitBlitMirror(
                Hardware,
                gcvFALSE,
                gcvFALSE,
                gcvTRUE
                ));

            /* Disable multiply. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012D0,
                0x00000000));
        }

        /* Set 2D dithering. */
        gcmONERROR(gcoHARDWARE_SetDither2D(
            Hardware,
            State->dither
            ));

        if (Hardware->features[gcvFEATURE_2D_OPF_YUV_OUTPUT])
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012E4,
                (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))))
                ));
        }
        else
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012E4,
                (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))))
                ));
        }

        Hardware->needOffL2Cache = gcvFALSE;

        switch (type)
        {
        case gceFILTER_BLIT_TYPE_VERTICAL:
            blitType = 0x1;

            if (SrcSurface->format == gcvSURF_I420 ||
                SrcSurface->format == gcvSURF_YV12 ||
                SrcSurface->format == gcvSURF_NV12 ||
                SrcSurface->format == gcvSURF_NV16 ||
                SrcSurface->format == gcvSURF_NV21 ||
                SrcSurface->format == gcvSURF_NV12)
            {
                Hardware->needOffL2Cache = gcvTRUE;
            }

            gcmONERROR(_LoadKernel(
                Hardware,
                gcvFILTER_BLIT_KERNEL_UNIFIED,
                VerKernel
                ));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01224,
                VerKernel->scaleFactor
                ));

            {
                gcsSURF_FORMAT_INFO_PTR srcFormat[2], dstFormat[2];
                gctUINT32 configEx;
                gcmONERROR(gcoSURF_QueryFormat(DstSurface->format, dstFormat));
                gcmONERROR(gcoSURF_QueryFormat(SrcSurface->format, srcFormat));

                if (((gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_90_DEGREE)
                    || (gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_270_DEGREE)
                    || (gcmGET_PRE_ROTATION(DstSurface->rotation) == gcvSURF_90_DEGREE)
                    || (gcmGET_PRE_ROTATION(DstSurface->rotation) == gcvSURF_270_DEGREE))
                    && (srcFormat[0]->bitsPerPixel != 32 || dstFormat[0]->bitsPerPixel != 32 ))
                {
                    configEx = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))));
                }
                else
                {
                    configEx = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))));
                }

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x012E4,
                    configEx
                    ));
            }
            break;

        case gceFILTER_BLIT_TYPE_HORIZONTAL:
            blitType = 0x0;

            gcmONERROR(_LoadKernel(
                Hardware,
                gcvFILTER_BLIT_KERNEL_UNIFIED,
                HorKernel
                ));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01220,
                HorKernel->scaleFactor
                ));

            break;

        case gceFILTER_BLIT_TYPE_ONE_PASS:
            {
                gctUINT8 horTap, verTap;
                gctUINT32 configEx;
                gctBOOL useOPFBlock = gcvFALSE;

                if (Hardware->features[gcvFEATURE_2D_OPF_YUV_OUTPUT])
                {
                    horTap = (HorKernel->kernelSize == 1 ? 3 : HorKernel->kernelSize);
                    verTap = (VerKernel->kernelSize == 1 ? 3 : VerKernel->kernelSize);
                }
                else
                {
                    horTap = HorKernel->kernelSize;
                    verTap = VerKernel->kernelSize;
                }

                if (SrcSurface->tiling != gcvLINEAR &&
                    Hardware->features[gcvFEATURE_SEPARATE_SRC_DST])
                {
                    if (horTap == 7 || horTap == 9)
                        horTap = 5;

                    if (verTap == 7 || verTap == 9)
                        verTap = 5;
                }

                gcmONERROR(_CheckOPFBlock(Hardware, SrcSurface, DstSurface, State, &useOPFBlock));

                if (useOPFBlock)
                {
                    gcmONERROR(_SetOPFBlockSize(
                        Hardware,
                        SrcSurface, DstSurface,
                        SrcRect, DstRect,
                        HorKernel, VerKernel,
                        horTap, verTap));
                }
                else
                {
                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x012F0,
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))))
                        ));
                }

                if (Hardware->features[gcvFEATURE_2D_ONE_PASS_FILTER_TAP])
                {
                    configEx =
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)))) &
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:15) - (0 ? 18:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:15) - (0 ? 18:15) + 1))))))) << (0 ? 18:15))) | (((gctUINT32) ((gctUINT32) (horTap) & ((gctUINT32) ((((1 ? 18:15) - (0 ? 18:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:15) - (0 ? 18:15) + 1))))))) << (0 ? 18:15))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)))) &
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (verTap) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))));
                }
                else
                {
                    configEx =
                        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (gcmMAX(horTap, verTap)) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))));
                }

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x012E4,
                    configEx
                    ));
            }

            blitType = 0x2;

            /* Uploading two kernel tables. */
            gcmONERROR(_LoadKernel(
                Hardware,
                gcvFILTER_BLIT_KERNEL_HORIZONTAL,
                HorKernel
                ));

            gcmONERROR(_LoadKernel(
                Hardware,
                gcvFILTER_BLIT_KERNEL_VERTICAL,
                VerKernel
                ));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01220,
                HorKernel->scaleFactor
                ));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01224,
                VerKernel->scaleFactor
                ));

            break;

        default:
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        if (SrcSurface->format == gcvSURF_INDEX8)
        {
            gcmONERROR(gcoHARDWARE_LoadPalette(
                Hardware,
                State->paletteFirstIndex,
                State->paletteIndexCount,
                State->paletteTable,
                State->paletteConvert,
                DstSurface->format,
                &State->paletteProgram,
                &State->paletteConvertFormat
                ));
        }

        /* Set source. */
        gcmONERROR(gcoHARDWARE_SetColorSource(
            Hardware,
            SrcSurface,
            gcvFALSE,
            0,
            curSrc->srcYUVMode,
            curSrc->srcDeGamma,
            gcvTRUE
            ));

        /* Set src global color for A8 source. */
        if (curSrc->srcSurface.format == gcvSURF_A8)
        {
            gcmONERROR(gcoHARDWARE_SetSourceGlobalColor(
                Hardware,
                curSrc->srcGlobalColor
                ));
        }

        /*--------------------------------------------------------------------*/
        memory[0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcRect->left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcRect->top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));
        memory[1]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcRect->right) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcRect->bottom) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));
        memory[2] = SrcOrigin->x;
        memory[3] = SrcOrigin->y;

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01298,
            4,
            memory
            ));

        if (curSrc->srcDeGamma && Hardware->features[gcvFEATURE_2D_GAMMA])
        {
            gcmONERROR(gcoHARDWARE_Load2DState(
                Hardware,
                0x13800,
                256,
                State->deGamma
                ));
        }

        /* Set destination. */
        gcmONERROR(gcoHARDWARE_SetTarget(
            Hardware,
            DstSurface,
            gcvTRUE,
            State->dstYUVMode,
            State->cscRGB2YUV,
            State->dstEnGamma ? State->enGamma : gcvNULL,
            curSrc->enableGDIStretch,
            memory
            ));

        anyCompress = (SrcSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED
                       || DstSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED);
        gcmONERROR(gcoHARDWARE_SetCompression(
            Hardware,
            State,
            SrcSurface,
            DstSurface,
            gcv2D_FILTER_BLT,
            anyCompress));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01234,
            memory[0]
            ));

        if (inCSC)
        {
            gcmONERROR(gcoHARDWARE_UploadCSCTable(
                Hardware,
                gcvTRUE,
                State->cscYUV2RGB
                ));
        }

        memory[0]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (DstRect->left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (DstRect->top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));
        memory[1]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (DstRect->right) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (DstRect->bottom) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x012A8,
            2,
            memory
            ));

        gcmONERROR(gcoHARDWARE_SetSuperTileVersion(
            Hardware,
            State->superTileVersion
            ));

        /*******************************************************************
        ** Setup ROP.
        */

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0125C,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (0xCC) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (0xCC) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01294,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (blitType) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            ));

        gcmONERROR(gcoHARDWARE_End2DRender(Hardware));

        }
    }
    while (gcvFALSE);

OnError:

    gcoHARDWARE_Reset2DCmdBuffer(Hardware, gcmIS_ERROR(status));

    /* Return status. */
    return status;
}

/******************************************************************************\
****************************** gcoHARDWARE API Code *****************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoHARDWARE_FreeFilterBuffer
**
**  Frees the temporary buffer allocated by filter blit operation.
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
gceSTATUS gcoHARDWARE_FreeFilterBuffer(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT i;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    for (i = 0; i < gcdTEMP_SURFACE_NUMBER; i += 1)
    {
        if (Hardware->temp2DSurf[i] != gcvNULL)
        {
            if (Hardware->temp2DSurf[i]->node.valid)
            {
                gcmONERROR(gcoHARDWARE_Unlock(
                    &Hardware->temp2DSurf[i]->node,
                    gcvSURF_BITMAP
                    ));
            }

            /* Free the video memory by event. */
            gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&Hardware->temp2DSurf[i]->node));

            gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Hardware->temp2DSurf[i]));
        }
    }

OnError:

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SplitFilterBlit
**
**  Split filter blit in specific case when factor is greater than 8.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcSurface
**          Pointer to the source surface descriptor.
**
**      gcsSURF_INFO_PTR DestSurface
**          Pointer to the destination surface descriptor.
**
**      gcsRECT_PTR SrcRect
**          Coorinates of the entire source image.
**
**      gcsRECT_PTR DestRect
**          Coorinates of the entire destination image.
**
**      gcsRECT_PTR DestSubRect
**          Coordinates of a sub area within the destination to render.
**          The coordinates are relative to the coordinates represented
**          by DestRect. If DestSubRect is gcvNULL, the complete image will
**          be rendered based on DestRect.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_SplitFilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcs2D_MULTI_SOURCE_PTR curSrc;
    gcsSURF_INFO_PTR srcSurf = gcvNULL;
    gcsSURF_INFO_PTR dstSurf = gcvNULL;
    gcsRECT srcRect, dstRect, dsRect;
    gctINT32 srcWidth, srcHeight, dstWidth, dstHeight;
    gctUINT32 factorInt, i, power = 0;
    gctUINT32 deltaHor, deltaVer;
    gctUINT32 mid, verFactor;
    gctBOOL alphaSave;

    gcmHEADER_ARG("Hardware=0x%x SrcSurface=0x%x DestSurface=0x%x "
                    "SrcRect=0x%x DestRect=0x%x DestSubRect=0x%x",
                    Hardware, SrcSurface, DestSurface,
                    SrcRect, DestRect, DestSubRect);

    gcmGETHARDWARE(Hardware);

    if (!Hardware->hw2DAppendCacheFlush)
    {
        gcmFOOTER();
        return gcvSTATUS_TRUE;
    }

    if (DestSubRect != gcvNULL)
    {
        if (DestSubRect->left >= DestRect->right
            || DestSubRect->right > DestRect->right
            || DestSubRect->top >= DestRect->bottom
            || DestSubRect->bottom > DestRect->bottom)
        {
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        dsRect.left   = DestRect->left + DestSubRect->left;
        dsRect.top    = DestRect->top  + DestSubRect->top;
        dsRect.right  = dsRect.left + DestSubRect->right;
        dsRect.bottom = dsRect.top  + DestSubRect->bottom;
    }
    else
    {
        dsRect = *DestRect;
    }

    gcmONERROR(gcsRECT_Width(SrcRect, &srcWidth));
    gcmONERROR(gcsRECT_Width(&dsRect, &dstWidth));
    gcmONERROR(gcsRECT_Height(SrcRect, &srcHeight));
    gcmONERROR(gcsRECT_Height(&dsRect, &dstHeight));

    if ((SrcSurface->rotation == gcvSURF_90_DEGREE
          || SrcSurface->rotation == gcvSURF_270_DEGREE) &&
         (DestSurface->rotation != gcvSURF_90_DEGREE
          && DestSurface->rotation != gcvSURF_270_DEGREE) )
    {
        verFactor = (dstHeight << 16) / srcWidth;
    }
    else if ((DestSurface->rotation == gcvSURF_90_DEGREE
               || DestSurface->rotation == gcvSURF_270_DEGREE) &&
              (SrcSurface->rotation != gcvSURF_90_DEGREE
               && SrcSurface->rotation != gcvSURF_270_DEGREE) )
    {
        verFactor = (dstWidth << 16) / srcHeight;
    }
    else if ((DestSurface->rotation == gcvSURF_90_DEGREE
               || DestSurface->rotation == gcvSURF_270_DEGREE) &&
              (SrcSurface->rotation == gcvSURF_90_DEGREE
               || SrcSurface->rotation == gcvSURF_270_DEGREE) )
    {
        verFactor = (dstWidth << 16) / srcWidth;
    }
    else
    {
        verFactor = (dstHeight << 16) / srcHeight;
    }

    mid = verFactor >> 16;
    /* If (dst vertical size / src vertical size) > 8, split filterblit. */
    if (mid < 8 || (mid == 8 && !(verFactor & 0xFFFF)))
    {
        gcmFOOTER();
        return gcvSTATUS_TRUE;
    }

    /* Disable alpha first */
    curSrc = State->multiSrc + State->currentSrcIndex;
    alphaSave = curSrc->enableAlpha;
    curSrc->enableAlpha = gcvFALSE;
    Hardware->notAdjustRotation = gcvTRUE;

    factorInt = verFactor >> 16;
    while (factorInt >>= 3)
    {
        ++power;
    }

    if (power == 0)
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);

    /* Initialize src surface. */
    srcSurf = SrcSurface;

    srcRect = *SrcRect;
    dstRect = *SrcRect;

    if (DestSurface->rotation == gcvSURF_90_DEGREE ||
        DestSurface->rotation == gcvSURF_270_DEGREE)
    {
        deltaHor = (dstHeight << 16) / (8 << (3 * (power - 1)));
        deltaVer = (dstWidth << 16) / (8 << (3 * (power - 1)));
    }
    else
    {
        deltaHor = (dstWidth << 16) / (8 << (3 * (power - 1)));
        deltaVer = (dstHeight << 16) / (8 << (3 * (power - 1)));
    }

    if (srcSurf->rotation == gcvSURF_90_DEGREE ||
        srcSurf->rotation == gcvSURF_270_DEGREE)
    {
        dstRect.right =
            srcRect.left + (deltaHor & 0xFFFF ? (deltaVer >> 16) + 1 : (deltaVer >> 16)) ;
        dstRect.bottom =
            srcRect.top + (deltaVer & 0xFFFF ? (deltaHor >> 16) + 1 : (deltaHor >> 16));
    }
    else
    {
        dstRect.right =
            srcRect.left + (deltaHor & 0xFFFF ? (deltaHor >> 16) + 1 : (deltaHor >> 16)) ;
        dstRect.bottom =
            srcRect.top + (deltaVer & 0xFFFF ? (deltaVer >> 16) + 1 : (deltaVer >> 16));
    }

    for (i = 0; i < power; i++)
    {
        gcmONERROR(gcoHARDWARE_Alloc2DSurface(
            Hardware,
            dstRect.right,
            dstRect.bottom,
            DestSurface->format,
            DestSurface->flags,
            &dstSurf));

        gcmONERROR(gcoHARDWARE_FilterBlit(
            Hardware,
            State,
            srcSurf,
            dstSurf,
            &srcRect,
            &dstRect,
            gcvNULL
            ));

        if (i)
            gcmONERROR(gcoHARDWARE_Free2DSurface(Hardware, srcSurf));

        /* Move on to next. */
        srcSurf = dstSurf;
        srcRect = dstRect;

        dstRect.left = srcRect.left;
        dstRect.top = srcRect.top;
        dstRect.right =
            srcRect.left + ((srcRect.right - srcRect.left) << 3);
        dstRect.bottom =
            srcRect.top + ((srcRect.bottom - srcRect.top) << 3);
    }

    /* Let's return to normal filter blit. */

    /* Fill in the source structure. */
    SrcSurface                   = srcSurf;
    SrcSurface->type             = gcvSURF_BITMAP;
    SrcSurface->format           = DestSurface->format;
    SrcSurface->alignedWidth     = srcSurf->alignedWidth;
    SrcSurface->alignedHeight    = srcSurf->alignedHeight;
    SrcSurface->rotation         = gcvSURF_0_DEGREE;
    SrcSurface->stride           = srcSurf->stride;
    SrcSurface->node.physical    = srcSurf->node.physical;
    SrcSurface->uStride          = srcSurf->uStride;
    SrcSurface->node.physical2   = srcSurf->node.physical2;
    SrcSurface->vStride          = srcSurf->vStride;
    SrcSurface->node.physical3   = srcSurf->node.physical3;
    SrcSurface->tiling           = gcvLINEAR;

    /* Adjust source rectangle. */
    SrcRect->right = srcRect.right;
    SrcRect->bottom = srcRect.bottom;

    /* Restore alpha. */
    curSrc->enableAlpha = alphaSave;

    status = gcoHARDWARE_FilterBlit(
        Hardware,
        State,
        SrcSurface,
        DestSurface,
        SrcRect,
        DestRect,
        DestSubRect
        );

    Hardware->notAdjustRotation = gcvFALSE;

OnError:
    if (srcSurf != gcvNULL)
    {
        gcmONERROR(gcoHARDWARE_Free2DSurface(Hardware, srcSurf));
    }

    gcmFOOTER();
    /* Return status. */
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_FilterBlit
**
**  Filter blit.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR SrcSurface
**          Pointer to the source surface descriptor.
**
**      gcsSURF_INFO_PTR DestSurface
**          Pointer to the destination surface descriptor.
**
**      gcsRECT_PTR SrcRect
**          Coorinates of the entire source image.
**
**      gcsRECT_PTR DestRect
**          Coorinates of the entire destination image.
**
**      gcsRECT_PTR DestSubRect
**          Coordinates of a sub area within the destination to render.
**          The coordinates are relative to the coordinates represented
**          by DestRect. If DestSubRect is gcvNULL, the complete image will
**          be rendered based on DestRect.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_FilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    )
{
    gceSTATUS status;
    gcsSURF_INFO_PTR tempSurf = gcvNULL;
    gcsPOINT srcRectSize;
    gcsPOINT destRectSize;

    gctBOOL horPass = gcvFALSE;
    gctBOOL verPass = gcvFALSE;
    gctBOOL useOPF = gcvFALSE;

    gcsSURF_FORMAT_INFO_PTR srcFormat[2];

    gcsRECT ssRect, dRect, sRect, dsRect;

    gcsFILTER_BLIT_ARRAY_PTR horKernel = gcvNULL;
    gcsFILTER_BLIT_ARRAY_PTR verKernel = gcvNULL;

    gceSURF_ROTATION srcBackRot = gcvSURF_0_DEGREE, dstBackRot = gcvSURF_0_DEGREE;
    gctBOOL rev = gcvFALSE, vMirror = gcvFALSE, hMirror = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x SrcSurface=0x%x DestSurface=0x%x "
                    "SrcRect=0x%x DestRect=0x%x DestSubRect=0x%x",
                    Hardware, SrcSurface, DestSurface,
                    SrcRect, DestRect, DestSubRect);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(SrcSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(SrcRect != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestRect != gcvNULL);

/*----------------------------------------------------------------------------*/
/*------------------- Verify the presence of 2D hardware. --------------------*/

    /* Only supported with hardware 2D engine. */
    if (!Hardware->hw2DEngine || Hardware->sw2DEngine)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Determine final destination subrectangle. */
    if (DestSubRect != gcvNULL)
    {
        if (DestSubRect->left >= DestRect->right
            || (DestRect->left >= 0 && DestSubRect->right > DestRect->right)
            || DestSubRect->top >= DestRect->bottom
            || (DestRect->top >= 0 && DestSubRect->bottom > DestRect->bottom))
        {
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        dsRect.left   = DestRect->left + DestSubRect->left;
        dsRect.top    = DestRect->top  + DestSubRect->top;
        dsRect.right  = DestRect->left + DestSubRect->right;
        dsRect.bottom = DestRect->top  + DestSubRect->bottom;
    }
    else
    {
        dsRect = *DestRect;
    }

/*----------------------------------------------------------------------------*/
/*------------------------- Rotation opimization. ----------------------------*/
    /* Determine temporary surface format. */
    gcmONERROR(gcoSURF_QueryFormat(SrcSurface->format, srcFormat));

    gcmONERROR(_CheckOPF(Hardware, SrcSurface, DestSurface, State, &useOPF));

    if (!Hardware->notAdjustRotation && !useOPF
        && Hardware->features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION]
        && (Hardware->features[gcvFEATURE_2D_MIRROR_EXTENSION] ||
            (gcmGET_PRE_ROTATION(DestSurface->rotation) != gcvSURF_FLIP_X
            && gcmGET_PRE_ROTATION(DestSurface->rotation) != gcvSURF_FLIP_Y
            && gcmGET_PRE_ROTATION(SrcSurface->rotation) != gcvSURF_FLIP_X
            && gcmGET_PRE_ROTATION(SrcSurface->rotation) != gcvSURF_FLIP_Y))
        && (State->newFilterType != gcvFILTER_USER))
    {
        gceSURF_ROTATION rot, rot2;
        gctUINT8 horKernelSize, verKernelSize;
        gceSURF_ROTATION dstRot, srcRot;

        dstBackRot = DestSurface->rotation;
        srcBackRot = SrcSurface->rotation;
        dstRot = gcmGET_PRE_ROTATION(DestSurface->rotation);
        srcRot = gcmGET_PRE_ROTATION(SrcSurface->rotation);
        hMirror = State->multiSrc[State->currentSrcIndex].horMirror;
        vMirror = State->multiSrc[State->currentSrcIndex].verMirror;
        rev = gcvTRUE;

        if (dstRot == gcvSURF_FLIP_X)
        {
            gctINT32 temp;

            dRect.left = DestSurface->alignedWidth - DestRect->right;
            dRect.right = DestSurface->alignedWidth - DestRect->left;
            dRect.top = DestRect->top;
            dRect.bottom = DestRect->bottom;

            temp = dsRect.left;
            dsRect.left = DestSurface->alignedWidth - dsRect.right;
            dsRect.right = DestSurface->alignedWidth - temp;

            State->multiSrc[State->currentSrcIndex].horMirror = !State->multiSrc[State->currentSrcIndex].horMirror;
            DestSurface->rotation = gcvSURF_0_DEGREE;
        }
        else if (dstRot == gcvSURF_FLIP_Y)
        {
            gctINT32 temp;

            dRect.left = DestRect->left;
            dRect.right = DestRect->right;
            dRect.top = DestSurface->alignedHeight - DestRect->bottom;
            dRect.bottom = DestSurface->alignedHeight - DestRect->top;

            temp = dsRect.top;
            dsRect.top = DestSurface->alignedHeight - dsRect.bottom;
            dsRect.bottom = DestSurface->alignedHeight - temp;

            State->multiSrc[State->currentSrcIndex].verMirror = !State->multiSrc[State->currentSrcIndex].verMirror;
            DestSurface->rotation = gcvSURF_0_DEGREE;
        }
        else
        {
            dRect = *DestRect;
        }

        if (srcRot == gcvSURF_FLIP_X)
        {
            sRect.left = SrcSurface->alignedWidth - SrcRect->right;
            sRect.right = SrcSurface->alignedWidth - SrcRect->left;
            sRect.top = SrcRect->top;
            sRect.bottom = SrcRect->bottom;

            State->multiSrc[State->currentSrcIndex].horMirror = !State->multiSrc[State->currentSrcIndex].horMirror;
            SrcSurface->rotation = gcvSURF_0_DEGREE;
        }
        else if (srcRot == gcvSURF_FLIP_Y)
        {
            sRect.left = SrcRect->left;
            sRect.right = SrcRect->right;
            sRect.top = SrcSurface->alignedHeight - SrcRect->bottom;
            sRect.bottom = SrcSurface->alignedHeight - SrcRect->top;

            State->multiSrc[State->currentSrcIndex].verMirror = !State->multiSrc[State->currentSrcIndex].verMirror;
            SrcSurface->rotation = gcvSURF_0_DEGREE;
        }
        else
        {
            sRect = *SrcRect;
        }

        horPass = (sRect.right - sRect.left) != (dRect.right - dRect.left);
        verPass = (sRect.bottom - sRect.top) != (dRect.bottom - dRect.top);

        if (State->newFilterType == gcvFILTER_BLUR)
        {
            horKernelSize = State->horBlurFilterKernel.kernelSize;
            verKernelSize = State->verBlurFilterKernel.kernelSize;
        }
        else
        {
            horKernelSize = State->horSyncFilterKernel.kernelSize;
            verKernelSize = State->verSyncFilterKernel.kernelSize;
        }

        rot2 = rot = DestSurface->rotation;
        gcmONERROR(gcsRECT_RelativeRotation(SrcSurface->rotation, &rot));
        gcmONERROR(gcsRECT_RelativeRotation(rot, &rot2));

        if ((horPass && verPass && (((srcFormat[0]->fmtClass == gcvFORMAT_CLASS_YUV)
            && (SrcSurface->format != gcvSURF_NV21)
            && (SrcSurface->format != gcvSURF_NV61))
            || (Hardware->features[gcvFEATURE_2D_ONE_PASS_FILTER] && (horKernelSize == 5 || horKernelSize == 3)
            && (verKernelSize == 5 || verKernelSize == 3)))) ||
            ((rot == gcvSURF_90_DEGREE || rot == gcvSURF_270_DEGREE) &&
            ((horPass && !verPass && (rot2 == gcvSURF_90_DEGREE || rot2 == gcvSURF_270_DEGREE))
            || (!horPass && verPass && (rot2 == gcvSURF_0_DEGREE || rot2 == gcvSURF_180_DEGREE)))))
        {
            rot2 = gcvSURF_0_DEGREE;
        }
        else
        {
            rot = gcvSURF_0_DEGREE;
            rot2 = SrcSurface->rotation;
            gcmONERROR(gcsRECT_RelativeRotation(DestSurface->rotation, &rot2));
        }

        gcmONERROR(gcsRECT_Rotate(
            &sRect,
            SrcSurface->rotation,
            rot2,
            SrcSurface->alignedWidth,
            SrcSurface->alignedHeight));

        SrcSurface->rotation = rot2;

        rot2 = DestSurface->rotation;
        gcmONERROR(gcsRECT_RelativeRotation(rot, &rot2));
        if (rot2 == gcvSURF_90_DEGREE || rot2 == gcvSURF_270_DEGREE)
        {
            gctBOOL tmp = State->multiSrc[State->currentSrcIndex].horMirror;
            State->multiSrc[State->currentSrcIndex].horMirror = State->multiSrc[State->currentSrcIndex].verMirror;
            State->multiSrc[State->currentSrcIndex].verMirror = tmp;
        }

        gcmONERROR(gcsRECT_Rotate(
            &dRect,
            DestSurface->rotation,
            rot,
            DestSurface->alignedWidth,
            DestSurface->alignedHeight));

        gcmONERROR(gcsRECT_Rotate(
            &dsRect,
            DestSurface->rotation,
            rot,
            DestSurface->alignedWidth,
            DestSurface->alignedHeight));

        DestSurface->rotation = rot;

        DestSurface->rotation |= gcmGET_POST_ROTATION(dstBackRot);
        SrcSurface->rotation  |= gcmGET_POST_ROTATION(srcBackRot);
    }
    else
    {
        dRect = *DestRect;
        sRect = *SrcRect;
    }

/*----------------------------------------------------------------------------*/
/*------------------------- Compute rectangle sizes. -------------------------*/

    gcmONERROR(gcsRECT_Width(&sRect, &srcRectSize.x));
    gcmONERROR(gcsRECT_Height(&sRect, &srcRectSize.y));
    gcmONERROR(gcsRECT_Width(&dRect, &destRectSize.x));
    gcmONERROR(gcsRECT_Height(&dRect, &destRectSize.y));

/*----------------------------------------------------------------------------*/
/*--------------------------- Update kernel arrays. --------------------------*/

    if (State->newFilterType == gcvFILTER_SYNC)
    {
        horPass = (srcRectSize.x != destRectSize.x);
        verPass = (srcRectSize.y != destRectSize.y);

        if (!verPass && !horPass)
        {
            if(gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_90_DEGREE
                || gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_270_DEGREE
                || gcmGET_PRE_ROTATION(DestSurface->rotation) == gcvSURF_90_DEGREE
                || gcmGET_PRE_ROTATION(DestSurface->rotation) == gcvSURF_270_DEGREE)
            {
                verPass = gcvTRUE;
            }
            else
            {
                horPass = gcvTRUE;
            }
        }

        /* Set the proper kernel array for sync filter. */
        horKernel = &State->horSyncFilterKernel;
        verKernel = &State->verSyncFilterKernel;

        /* Recompute the table if necessary. */
        gcmONERROR(_CalculateSyncTable(
            Hardware,
            State->newHorKernelSize,
            srcRectSize.x,
            destRectSize.x,
            horKernel
            ));

        gcmONERROR(_CalculateSyncTable(
            Hardware,
            State->newVerKernelSize,
            srcRectSize.y,
            destRectSize.y,
            verKernel
            ));
    }
    else if (State->newFilterType == gcvFILTER_BLUR)
    {
        /* Always do both passes for blur. */
        horPass = verPass = gcvTRUE;

        /* Set the proper kernel array for blur filter. */
        horKernel = &State->horBlurFilterKernel;
        verKernel = &State->verBlurFilterKernel;

        /* Recompute the table if necessary. */
        gcmONERROR(_CalculateBlurTable(
            Hardware,
            State->newHorKernelSize,
            srcRectSize.x,
            destRectSize.x,
            horKernel
            ));

        gcmONERROR(_CalculateBlurTable(
            Hardware,
            State->newVerKernelSize,
            srcRectSize.y,
            destRectSize.y,
            verKernel
            ));
    }
    else if (State->newFilterType == gcvFILTER_USER)
    {
        gctUINT32 scaleFactor;

        /* Do the pass(es) according to user settings. */
        horPass = State->horUserFilterPass;
        verPass = State->verUserFilterPass;

        /* Set the proper kernel array for user defined filter. */
        horKernel = &State->horUserFilterKernel;
        verKernel = &State->verUserFilterKernel;

        /* Set the kernel size and scale factors. */
        scaleFactor = gcoHARDWARE_GetStretchFactor(
            State->multiSrc[State->currentSrcIndex].enableGDIStretch,
            srcRectSize.x, destRectSize.x);
        horKernel->kernelSize  = State->newHorKernelSize;
        horKernel->scaleFactor = scaleFactor;

        scaleFactor = gcoHARDWARE_GetStretchFactor(
            State->multiSrc[State->currentSrcIndex].enableGDIStretch,
            srcRectSize.y, destRectSize.y);
        verKernel->kernelSize  = State->newVerKernelSize;
        verKernel->scaleFactor = scaleFactor;
    }
    else
    {
        gcmTRACE_ZONE(gcvLEVEL_ERROR, gcvZONE_HARDWARE, "Unknown filter type");
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (Hardware->features[gcvFEATURE_ANDROID_ONLY])
    {
        if (!Hardware->features[gcvFEATURE_2D_ONE_PASS_FILTER_TAP] &&
            (horKernel->kernelSize == 7 || horKernel->kernelSize == 9 ||
            verKernel->kernelSize == 7 || verKernel->kernelSize == 9))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

/*----------------------------------------------------------------------------*/
/*------------------- Determine the source sub rectangle. --------------------*/

    /* Compute the source sub rectangle that exactly represents
       the destination sub rectangle. */
    ssRect.left = dsRect.left - dRect.left;
    ssRect.right = dsRect.right - dRect.left;
    ssRect.top = dsRect.top - dRect.top;
    ssRect.bottom = dsRect.bottom - dRect.top;

    if (State->specialFilterMirror)
    {
        gctINT32 tmp;

        if (State->multiSrc[State->currentSrcIndex].horMirror)
        {
            tmp = ssRect.left;
            ssRect.left = destRectSize.x - ssRect.right;
            ssRect.right = destRectSize.x - tmp;
        }

        if (State->multiSrc[State->currentSrcIndex].verMirror)
        {
            tmp = ssRect.top;
            ssRect.top = destRectSize.y - ssRect.bottom;
            ssRect.bottom = destRectSize.y - tmp;
        }
    }

    ssRect.left *= horKernel->scaleFactor;
    ssRect.top *= verKernel->scaleFactor;
    ssRect.right
        = (ssRect.right - 1)
        * horKernel->scaleFactor + (1 << 16);
    ssRect.bottom
        = (ssRect.bottom - 1)
        * verKernel->scaleFactor + (1 << 16);

    if (!State->multiSrc[State->currentSrcIndex].enableGDIStretch)
    {
        /*  Before rendering each destination pixel, the HW will select the
            corresponding source center pixel to apply the kernel around.
            To make this process precise we need to add 0.5 to source initial
            coordinates here; this will make HW pick the next source pixel if
            the fraction is equal or greater then 0.5. */
        ssRect.left   += 0x00008000;
        ssRect.top    += 0x00008000;
        ssRect.right  += 0x00008000;
        ssRect.bottom += 0x00008000;
    }

/*----------------------------------------------------------------------------*/
/*--------------- Process negative rectangle for filterblit. -----------------*/

    if (dRect.left < 0 || dsRect.top < 0 ||
        sRect.left < 0 || sRect.top < 0)
    {
        gctUINT64 fixedTmp;
        gctINT32 deltaDstX=0, deltaDstY=0, deltaSrcX=0, deltaSrcY=0, srcX, srcY, dstX, dstY, tmp=0;
        gctUINT32 reverseFactor;

        if (dsRect.left < 0)
            deltaDstX = 0 - dsRect.left;
        if (dsRect.top < 0)
            deltaDstY = 0 - dsRect.top;
        if (sRect.left < 0)
            deltaSrcX = 0 - sRect.left;
        if (sRect.top < 0)
            deltaSrcY = 0 - sRect.top;

        if (dsRect.left < 0 || sRect.left < 0)
        {
            srcX = deltaDstX * horKernel->scaleFactor;

            if ((srcX >> 16) >= deltaSrcX)
            {
                sRect.left += (srcX >> 16);
                ssRect.left += srcX & 0xFFFF;
                dsRect.left = 0;
            }
            else
            {
                reverseFactor = gcoHARDWARE_GetStretchFactor(gcvFALSE, destRectSize.x, srcRectSize.x);
                dstX = deltaSrcX * reverseFactor;
                if (dstX & 0xFFFF)
                {
                    /* ceil dst rect */
                    tmp =  (dstX + 0x00010000) >> 16;
                    dsRect.left += tmp;
                    fixedTmp = ((tmp << 16) - dstX) * horKernel->scaleFactor;
                    tmp = (gctUINT)((((fixedTmp >> 32) & 0xFFFF) << 16) | ((fixedTmp & 0xFFFFFFFFFFFFULL) >> 16));
                    sRect.left = tmp >> 16;
                }
                else
                {
                    sRect.left = 0;
                }
                ssRect.left += tmp & 0xFFFF;
            }
        }

        if (dsRect.top < 0 || sRect.top < 0)
        {
            srcY = deltaDstY * verKernel->scaleFactor;

            if ((srcY >> 16) >= deltaSrcY)
            {
                sRect.top += (srcY >> 16);
                ssRect.top += srcY & 0xFFFF;
                dsRect.top = 0;
            }
            else
            {
                reverseFactor = gcoHARDWARE_GetStretchFactor(gcvFALSE, destRectSize.y, srcRectSize.y);
                dstY = deltaSrcY * reverseFactor;
                if (dstY & 0xFFFF)
                {
                    /* ceil dst rect */
                    tmp =  (dstY + 0x00010000) >> 16;
                    dsRect.top += tmp;
                    fixedTmp = ((tmp << 16) - dstY) * verKernel->scaleFactor;
                    tmp = (gctUINT)((((fixedTmp >> 32) & 0xFFFF) << 16) | ((fixedTmp & 0xFFFFFFFFFFFFULL) >> 16));
                    sRect.top = tmp >> 16;
                }
                else
                {
                    sRect.top = 0;
                }
                ssRect.top += tmp & 0xFFFF;
            }
        }
    }

/*----------------------------------------------------------------------------*/
/*------------------------- Do the one pass blit. ----------------------------*/

    if ((Hardware->features[gcvFEATURE_2D_ONE_PASS_FILTER]
         && (useOPF ||
             ((horKernel->kernelSize == 5 || horKernel->kernelSize == 3)
             && (verKernel->kernelSize == 5 || verKernel->kernelSize == 3)
             && horPass && verPass))
         && (gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_0_DEGREE)
         && (SrcSurface->format != gcvSURF_INDEX8)
         && (!State->multiSrc[State->currentSrcIndex].srcDeGamma &&
             !State->dstEnGamma)
         && (!State->multiSrc[State->currentSrcIndex].enableAlpha
             || Hardware->features[gcvFEATURE_2D_OPF_YUV_OUTPUT]
             || ((DestSurface->format != gcvSURF_UYVY) &&
                 (DestSurface->format != gcvSURF_YUY2) &&
                 (DestSurface->format != gcvSURF_VYUY) &&
                 (DestSurface->format != gcvSURF_YVYU)))) ||
        Hardware->features[gcvFEATURE_SEPARATE_SRC_DST])
    {
        /* Determine the source origin. */
        gcsPOINT srcOrigin;

        srcOrigin.x = (sRect.left << 16) + ssRect.left;
        srcOrigin.y = (sRect.top  << 16) + ssRect.top;

        /* Start the blit. */
        gcmONERROR(_StartVR(
            Hardware,
            State,
            gceFILTER_BLIT_TYPE_ONE_PASS,
            horKernel,
            verKernel,
            SrcSurface,
            &sRect,
            &srcOrigin,
            DestSurface,
            &dsRect,
            gcvFALSE));
    }

/*----------------------------------------------------------------------------*/
/*------------------ Do the blit with the temporary buffer. ------------------*/

    else if (horPass && verPass)
    {
        gctUINT32 horKernelHalf;
        gctUINT32 leftExtra;
        gctUINT32 rightExtra;
        gcsPOINT srcOrigin;
        gcsPOINT tmpRectSize;
        gcsSURF_FORMAT_INFO_PTR tempFormat[2];
        gcsPOINT tempAlignment;
        gctUINT32 tempHorCoordMask;
        gctUINT32 tempVerCoordMask;
        gcsPOINT tempOrigin;
        gcsRECT tempRect;
        gcsPOINT tmpBufRectSize;

        /* In partial filter blit cases, the vertical pass has to render
           more pixel information to the left and to the right of the
           temporary image so that the horizontal pass has its necessary
           kernel information on the edges of the image. */
        horKernelHalf = horKernel->kernelSize >> 1;

        leftExtra  = ssRect.left >> 16;
        rightExtra = srcRectSize.x - (ssRect.right >> 16);

        if (leftExtra > horKernelHalf)
            leftExtra = horKernelHalf;

        if (rightExtra > horKernelHalf)
            rightExtra = horKernelHalf;

        /* Determine the source origin. */
        srcOrigin.x = ((sRect.left - leftExtra) << 16) + ssRect.left;
        srcOrigin.y = (sRect.top << 16) + ssRect.top;

        /* Determine the size of the temporary image. */
        tmpRectSize.x
            = leftExtra
            + ((ssRect.right >> 16) - (ssRect.left >> 16))
            + rightExtra;

        tmpRectSize.y
            = dsRect.bottom - dsRect.top;

        /* Determine the destination origin. */
        tempRect.left = srcOrigin.x >> 16;
        tempRect.top  = 0;

        if (srcFormat[0]->fmtClass == gcvFORMAT_CLASS_YUV)
        {
            if (Hardware->features[gcvFEATURE_2DPE20]
                && (gcmGET_PRE_ROTATION(SrcSurface->rotation) == gcvSURF_0_DEGREE)
                && (SrcSurface->format != gcvSURF_NV21)
                && (SrcSurface->format != gcvSURF_NV61)
                && (!Hardware->bigEndian
                || ((SrcSurface->format != gcvSURF_NV12)
                && (SrcSurface->format != gcvSURF_NV16))))
            {
                if (SrcSurface->format == gcvSURF_UYVY ||
                    SrcSurface->format == gcvSURF_VYUY ||
                    SrcSurface->format == gcvSURF_YVYU)
                {
                    tempFormat[0] = srcFormat[0];
                    tempFormat[1] = srcFormat[1];
                }
                else
                {
                    gcmONERROR(gcoSURF_QueryFormat(gcvSURF_YUY2, tempFormat));
                }

                tmpRectSize.x = gcmALIGN(tmpRectSize.x, 2);
                tempRect.left = gcmALIGN(tempRect.left, 2);
            }
            else
            {
                gcmONERROR(gcoSURF_QueryFormat(DestSurface->format, tempFormat));
            }
        }
        else
        {
            if ((SrcSurface->format == gcvSURF_INDEX8) || (SrcSurface->format == gcvSURF_A8))
            {
                gcmONERROR(gcoSURF_QueryFormat(gcvSURF_A8R8G8B8, tempFormat));
            }
            else
            {
                tempFormat[0] = srcFormat[0];
                tempFormat[1] = srcFormat[1];
            }
        }

        gcmONERROR(gco2D_GetPixelAlignment(
            tempFormat[0]->format,
            &tempAlignment
            ));

        tempHorCoordMask = tempAlignment.x - 1;
        tempVerCoordMask = tempAlignment.y - 1;

        /* Align the temporary destination. */
        tempRect.left &= tempHorCoordMask;
        tempRect.top  &= tempVerCoordMask;

        /* Determine the bottom right corner of the destination. */
        tempRect.right  = tempRect.left + tmpRectSize.x;
        tempRect.bottom = tempRect.top  + tmpRectSize.y;

        /* Determine the source origin. */
        tempOrigin.x
            = ((leftExtra + tempRect.left) << 16)
            + (ssRect.left & 0xFFFF);
        tempOrigin.y
            = (tempRect.top << 16)
            + (ssRect.top & 0xFFFF);

        /* Determine the size of the temporaty surface. */
        tmpBufRectSize.x = gcmALIGN(tempRect.right, tempAlignment.x);
        tmpBufRectSize.y = gcmALIGN(tempRect.bottom, tempAlignment.y);

        /* Allocate the temporary buffer. */
        gcmONERROR(gcoHARDWARE_Get2DTempSurface(
            Hardware,
            tmpBufRectSize.x,
            tmpBufRectSize.y,
            tempFormat[0]->format,
            DestSurface->flags,
            &tempSurf
            ));

        /*******************************************************************
        ** Program the vertical pass.
        */

        gcmONERROR(_StartVR(
            Hardware,
            State,
            gceFILTER_BLIT_TYPE_VERTICAL,
            gcvNULL,
            verKernel,
            SrcSurface,
            &sRect,
            &srcOrigin,
            tempSurf,
            &tempRect,
            gcvTRUE));

        if (Hardware->hw2DAppendCacheFlush &&
            (SrcSurface->format == gcvSURF_NV12 ||
             SrcSurface->format == gcvSURF_YV12 ||
             SrcSurface->format == gcvSURF_I420 ||
             SrcSurface->format == gcvSURF_NV21 ||
             SrcSurface->format == gcvSURF_NV16 ||
             SrcSurface->format == gcvSURF_NV61))
        {
            gcoHAL_Commit(gcvNULL, gcvFALSE);
        }

        /*******************************************************************
        ** Program the second pass.
        */
        if (Hardware->features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION] &&
            ((gcmGET_PRE_ROTATION(DestSurface->rotation) == gcvSURF_90_DEGREE)
            || (gcmGET_PRE_ROTATION(DestSurface->rotation) == gcvSURF_270_DEGREE)))
        {
            gceSURF_ROTATION rot;
            gctBOOL tmp = State->multiSrc[State->currentSrcIndex].horMirror;

            State->multiSrc[State->currentSrcIndex].horMirror = State->multiSrc[State->currentSrcIndex].verMirror;
            State->multiSrc[State->currentSrcIndex].verMirror = tmp;

            rot = tempSurf->rotation;
            gcmONERROR(gcsRECT_RelativeRotation(DestSurface->rotation, &rot));

            gcmONERROR(gcsRECT_Rotate(
                &tempRect,
                tempSurf->rotation,
                rot,
                tempSurf->alignedWidth,
                tempSurf->alignedHeight));

            if (rot == gcvSURF_90_DEGREE)
            {
                tempOrigin.x
                    = (tempRect.left << 16)
                    + (ssRect.top & 0xFFFF);

                tempOrigin.y
                    = ((rightExtra + tempRect.top) << 16)
                    + (ssRect.right & 0xFFFF);
            }
            else
            {
                tempOrigin.x
                    = (tempRect.left << 16)
                    + (ssRect.bottom & 0xFFFF);

                tempOrigin.y
                    = ((leftExtra + tempRect.top) << 16)
                    + (ssRect.left & 0xFFFF);
            }

            tempSurf->rotation = rot;

            gcmONERROR(gcsRECT_Rotate(
                &dsRect,
                DestSurface->rotation,
                gcvSURF_0_DEGREE,
                DestSurface->alignedWidth,
                DestSurface->alignedHeight));

            DestSurface->rotation = gcvSURF_0_DEGREE | gcmGET_POST_ROTATION(DestSurface->rotation);

            gcmONERROR(_StartVR(
                Hardware,
                State,
                gceFILTER_BLIT_TYPE_VERTICAL,
                gcvNULL,
                horKernel,
                tempSurf,
                &tempRect,
                &tempOrigin,
                DestSurface,
                &dsRect,
                gcvFALSE));
        }
        else
        {
            gcmONERROR(_StartVR(
                Hardware,
                State,
                gceFILTER_BLIT_TYPE_HORIZONTAL,
                horKernel,
                gcvNULL,
                tempSurf,
                &tempRect,
                &tempOrigin,
                DestSurface,
                &dsRect,
                gcvFALSE));
        }
    }

/*----------------------------------------------------------------------------*/
/*---------------------------- One pass only blit. -------------------------*/

    else if (horPass || verPass)
    {
        /* Determine the source origin. */
        gcsPOINT srcOrigin;

        srcOrigin.x = (sRect.left << 16) + ssRect.left;
        srcOrigin.y = (sRect.top  << 16) + ssRect.top;

        /* Start the blit. */
        gcmONERROR(_StartVR(
            Hardware,
            State,
            horPass ? gceFILTER_BLIT_TYPE_HORIZONTAL : gceFILTER_BLIT_TYPE_VERTICAL,
            horKernel,
            verKernel,
            SrcSurface,
            &sRect,
            &srcOrigin,
            DestSurface,
            &dsRect,
            gcvFALSE
            ));
    }
/*----------------------------------------------------------------------------*/
/*---------------------------- Should no be here. ----------------------------*/
    else
    {
        gcmTRACE_ZONE(gcvLEVEL_ERROR,
                      gcvZONE_HARDWARE,
                      "None of the passes is set."
                      );

        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Unlock temporary. */
    if (tempSurf != gcvNULL)
    {
        gceSTATUS statusTemp;

        /* Unlock the temporary surface. */
        statusTemp = gcoHARDWARE_Put2DTempSurface(
            Hardware, tempSurf
            );

        /* Update the returned status if it is gcvSTATUS_OK.
           Or else, keep last error status.
        */
        if (status == gcvSTATUS_OK)
        {
            status = statusTemp;
        }
    }

    if (rev)
    {
        DestSurface->rotation = dstBackRot;
        SrcSurface->rotation = srcBackRot;
        State->multiSrc[State->currentSrcIndex].horMirror = hMirror;
        State->multiSrc[State->currentSrcIndex].verMirror = vMirror;
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_SplitYUVFilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    )
{
    gcsRECT srcRect, dstRect, subDstRect;
    gceSTATUS status = gcvSTATUS_OK;
    gceSURF_FORMAT srcFormat;

    gcmHEADER_ARG("Hardware=0x%x SrcSurface=0x%x DestSurface=0x%x "
            "SrcRect=0x%x DestRect=0x%x DestSubRect=0x%x",
            Hardware, SrcSurface, DestSurface,
            SrcRect, DestRect, DestSubRect);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(SrcSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestRect != gcvNULL);

    srcFormat = SrcSurface->format;

    /* Do A8 for Y channel */
    SrcSurface->format = gcvSURF_A8;
    DestSurface->format = gcvSURF_A8;

    status = gcoHARDWARE_FilterBlit(
            Hardware,
            State,
            SrcSurface,
            DestSurface,
            SrcRect,
            DestRect,
            DestSubRect
            );

    Hardware->hw2DDoMultiDst = gcvTRUE;

    SrcSurface->alignedWidth >>= 1;
    srcRect.left = SrcRect->left >> 1;
    srcRect.top = SrcRect->top;
    srcRect.right = srcRect.left + ((SrcRect->right - SrcRect->left) >> 1);

    DestSurface->alignedWidth >>= 1;
    dstRect.left = DestRect->left >> 1;
    dstRect.top = DestRect->top;
    dstRect.right = dstRect.left + ((DestRect->right - DestRect->left) >> 1);

    subDstRect.left = (DestSubRect == gcvNULL ? 0 : DestSubRect->left >> 1);
    subDstRect.top = (DestSubRect == gcvNULL ? 0 : DestSubRect->top);
    subDstRect.right = (DestSubRect == gcvNULL ? dstRect.right - dstRect.left:
                            subDstRect.left + ((DestSubRect->right - DestSubRect->left) >> 1));

    if (srcFormat == gcvSURF_YV12 || srcFormat == gcvSURF_I420)
    {
        SrcSurface->alignedHeight >>= 1;
        srcRect.top >>= 1;
        srcRect.bottom = srcRect.top + ((SrcRect->bottom - SrcRect->top) >> 1);

        DestSurface->alignedHeight >>= 1;
        dstRect.top >>= 1;
        dstRect.bottom = dstRect.top + ((DestRect->bottom - DestRect->top) >> 1);
        subDstRect.top >>= 1;
        subDstRect.bottom = (DestSubRect == gcvNULL ? dstRect.bottom - dstRect.top:
                                subDstRect.top + ((DestSubRect->bottom - DestSubRect->top) >> 1));

        /* Do A8 for U channel */
        SrcSurface->node.physical = SrcSurface->node.physical2;
        SrcSurface->stride = SrcSurface->uStride;

        DestSurface->node.physical = DestSurface->node.physical2;
        DestSurface->stride = DestSurface->uStride;

        status = gcoHARDWARE_FilterBlit(
                Hardware,
                State,
                SrcSurface,
                DestSurface,
                &srcRect,
                &dstRect,
                &subDstRect
                );

        /* Do A8 for V channel */
        SrcSurface->node.physical = SrcSurface->node.physical3;
        SrcSurface->stride = SrcSurface->vStride;

        DestSurface->node.physical = DestSurface->node.physical3;
        DestSurface->stride = DestSurface->vStride;

        status = gcoHARDWARE_FilterBlit(
                gcvNULL,
                State,
                SrcSurface,
                DestSurface,
                &srcRect,
                &dstRect,
                &subDstRect
                );
    }
    else
    {
        /* Do RG16 for UV channel */
        SrcSurface->format = gcvSURF_RG16;
        SrcSurface->node.physical = SrcSurface->node.physical2;
        SrcSurface->stride = SrcSurface->uStride;

        DestSurface->format = gcvSURF_RG16;
        DestSurface->node.physical = DestSurface->node.physical2;
        DestSurface->stride = DestSurface->uStride;

        if (srcFormat == gcvSURF_NV12 ||
                srcFormat == gcvSURF_NV21)
        {
            SrcSurface->alignedHeight >>= 1;
            srcRect.top >>= 1;
            srcRect.bottom = srcRect.top + ((SrcRect->bottom - SrcRect->top) >> 1);

            DestSurface->alignedHeight >>= 1;
            dstRect.top >>= 1;
            dstRect.bottom = dstRect.top + ((DestRect->bottom - DestRect->top) >> 1);
            subDstRect.top >>= 1;
            subDstRect.bottom = (DestSubRect == gcvNULL ? dstRect.bottom - dstRect.top:
                                    subDstRect.top + ((DestSubRect->bottom - DestSubRect->top) >> 1));
        }
        else /* gcvSURF_NV16 & gcvSURF_NV61 */
        {
            srcRect.bottom = SrcRect->bottom;
            dstRect.bottom = DestRect->bottom;
            subDstRect.bottom = (DestSubRect == gcvNULL ? dstRect.bottom - dstRect.top:
                                                          DestSubRect->bottom);
        }

        status = gcoHARDWARE_FilterBlit(
                gcvNULL,
                State,
                SrcSurface,
                DestSurface,
                &srcRect,
                &dstRect,
                &subDstRect
                );
    }

OnError:
    if(gcvNULL != Hardware)
        Hardware->hw2DDoMultiDst = gcvFALSE;

    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_MultiPlanarYUVConvert(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR DestRect
    )
{
    gceSTATUS status;
    gcs2D_State state;
    gcsSURF_INFO_PTR tempSurf = gcvNULL;
    gctUINT pass = 0;
    gcsRECT rect = {0, 0, 0, 0};

    gcmHEADER_ARG("Hardware=0x%x SrcSurface=0x%x DestSurface=0x%x "
                    "SrcRect=0x%x DestRect=0x%x DestSubRect=0x%x",
                    SrcSurface, DestSurface,
                    DestRect);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(SrcSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestSurface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(DestRect != gcvNULL);

    /* Only supported with hardware 2D engine. */
    if (!Hardware->hw2DEngine || Hardware->sw2DEngine)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    switch (DestSurface->format)
    {
    case gcvSURF_I420:
    case gcvSURF_YV12:
        pass = 3;
        break;

    case gcvSURF_NV12:
    case gcvSURF_NV21:
        pass = 2;
        break;

    case gcvSURF_NV16:
    case gcvSURF_NV61:
        pass = 1;
        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    rect.right = rect.left = DestRect->left >> 1;
    rect.top = DestRect->top;
    rect.right += (DestRect->right - DestRect->left) >> 1;
    rect.bottom = DestRect->bottom;

    gcoOS_MemCopy(&state, State, sizeof(state));

    state.dstSurface = *DestSurface;
    state.currentSrcIndex = 0;
    state.srcMask = 1;
    state.multiSrc[0].enableAlpha = gcvFALSE;
    state.multiSrc[0].srcRect = rect;
    state.multiSrc[0].srcTransparency = gcv2D_OPAQUE;
    state.multiSrc[0].dstTransparency = gcv2D_OPAQUE;
    state.multiSrc[0].patTransparency = gcv2D_OPAQUE;
    state.multiSrc[0].fgRop = 0xCC;
    state.multiSrc[0].bgRop = 0xCC;

    state.multiSrc[0].srcSurface = *SrcSurface;
    state.multiSrc[0].clipRect = rect;
    state.dstClipRect = rect;

    if (pass > 1)
    {
        /* Allocate the temporary buffer. */
        gcmONERROR(gcoHARDWARE_Get2DTempSurface(
            Hardware,
            DestSurface->alignedWidth,
            DestSurface->alignedHeight,
            gcvSURF_R5G6B5,
            DestSurface->flags,
            &tempSurf
            ));

        if (pass == 2)
        {
            state.dstSurface.node.physical2 = tempSurf->node.physical;
            state.dstSurface.uStride = tempSurf->stride;
        }
        else
        {
            state.dstSurface.node.physical2 = tempSurf->node.physical;
            gcmSAFECASTSIZET(state.dstSurface.node.physical3,
                tempSurf->node.physical + (tempSurf->node.size >> 1));

            state.dstSurface.uStride = tempSurf->stride >> 1;
            state.dstSurface.vStride = tempSurf->stride >> 1;
        }
    }

    /* Start multi dest blit. */
    Hardware->hw2DDoMultiDst = gcvTRUE;

    gcmONERROR(gcoHARDWARE_StartDE(
        Hardware,
        &state,
        gcv2D_MULTI_SOURCE_BLT,
        0,
        gcvNULL,
        1,
        &rect
        ));

    if (pass > 1)
    {
        gcsFILTER_BLIT_ARRAY kernelVer, kernelHor;
        gcsPOINT srcOrigin = {0, 0};
        gcsRECT dstRect;
        static const gctUINT16 tableHalf[gcvKERNELSTATES + 2] = {
            0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0,
            0, 0, 0, 0, 0x2000, 0x2000, 0, 0, 0
        };

        static const gctUINT16 tableOne[gcvKERNELSTATES + 2] = {
            0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0,
            0, 0, 0, 0, 0x4000, 0, 0, 0, 0
        };

        dstRect.top = dstRect.bottom = rect.top >> 1;
        dstRect.bottom += (rect.bottom - rect.top) >> 1;
        dstRect.left = rect.left;
        dstRect.right = rect.right;

        srcOrigin.x = rect.left << 16;
        srcOrigin.y = rect.top << 16;
        state.multiSrc[0].srcRect = rect;
        state.multiSrc[0].clipRect = dstRect;
        state.dstClipRect = dstRect;

        kernelVer.filterType = gcvFILTER_USER;
        kernelVer.kernelChanged = gcvTRUE;
        kernelVer.kernelSize = 3;
        kernelVer.scaleFactor = gcoHARDWARE_GetStretchFactor(
            gcvTRUE, rect.bottom - rect.top, dstRect.bottom - dstRect.top);
        kernelVer.kernelStates = (gctPOINTER)tableHalf;

        kernelHor.filterType = gcvFILTER_USER;
        kernelHor.kernelChanged = gcvTRUE;
        kernelHor.kernelSize = 1;
        kernelHor.scaleFactor = gcoHARDWARE_GetStretchFactor(
        gcvTRUE, rect.right - rect.left, dstRect.right - dstRect.left);
        kernelHor.kernelStates = (gctPOINTER)tableOne;

        if (pass == 2)
        {
            state.multiSrc[0].srcSurface = *DestSurface;
            state.multiSrc[0].srcSurface.format = gcvSURF_RG16;
            state.multiSrc[0].srcSurface.node.physical = tempSurf->node.physical;
            state.multiSrc[0].srcSurface.stride = tempSurf->stride;
            state.multiSrc[0].srcSurface.alignedWidth >>= 1;

            state.dstSurface.format = gcvSURF_RG16;
            state.dstSurface.node.physical = DestSurface->node.physical2;
            state.dstSurface.stride = DestSurface->uStride;
            state.dstSurface.alignedWidth >>= 1;
            state.dstSurface.alignedHeight >>= 1;

            gcmONERROR(_StartVR(
                Hardware,
                &state,
                gceFILTER_BLIT_TYPE_ONE_PASS,
                &kernelHor,
                &kernelVer,
                &state.multiSrc[0].srcSurface,
                &rect,
                &srcOrigin,
                &state.dstSurface,
                &dstRect,
                gcvTRUE));
        }
        else
        {
            state.multiSrc[0].srcSurface = *DestSurface;
            state.multiSrc[0].srcSurface.format = gcvSURF_A8;
            state.multiSrc[0].srcSurface.node.physical = tempSurf->node.physical;
            state.multiSrc[0].srcSurface.stride = tempSurf->stride >> 1;
            state.multiSrc[0].srcSurface.alignedWidth >>= 1;

            state.dstSurface.format = gcvSURF_A8;
            state.dstSurface.node.physical = DestSurface->node.physical2;
            state.dstSurface.stride = DestSurface->uStride;
            state.dstSurface.alignedWidth >>= 1;
            state.dstSurface.alignedHeight >>= 1;

            gcmONERROR(_StartVR(
                Hardware,
                &state,
                gceFILTER_BLIT_TYPE_ONE_PASS,
                &kernelHor,
                &kernelVer,
                &state.multiSrc[0].srcSurface,
                &rect,
                &srcOrigin,
                &state.dstSurface,
                &dstRect,
                gcvTRUE));

            gcmSAFECASTSIZET(state.multiSrc[0].srcSurface.node.physical,
                tempSurf->node.physical + (tempSurf->node.size >> 1));
            state.multiSrc[0].srcSurface.stride = tempSurf->stride >> 1;

            state.dstSurface.node.physical = DestSurface->node.physical3;
            state.dstSurface.stride = DestSurface->vStride;

            gcmONERROR(_StartVR(
                Hardware,
                &state,
                gceFILTER_BLIT_TYPE_ONE_PASS,
                &kernelHor,
                &kernelVer,
                &state.multiSrc[0].srcSurface,
                &rect,
                &srcOrigin,
                &state.dstSurface,
                &dstRect,
                gcvTRUE));
        }
    }

OnError:

    if (Hardware)
    {
        Hardware->hw2DDoMultiDst = gcvFALSE;
    }

    /* Unlock temporary. */
    if (tempSurf != gcvNULL)
    {
        gceSTATUS statusTemp;

        /* Unlock the temporary surface. */
        statusTemp = gcoHARDWARE_Put2DTempSurface(
            Hardware, tempSurf
            );

        /* Update the returned status if it is gcvSTATUS_OK.
           Or else, keep last error status.
        */
        if (status == gcvSTATUS_OK)
        {
            status = statusTemp;
        }
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_Begin2DRender(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x State=0x%x",
                    Hardware, State);

    /*******************************************************************
    ** Select 2D pipe.
    */
    if (Hardware->hw3DEngine)
    {
        /* Flush the 3D pipe. */
        gcmONERROR(gcoHARDWARE_Load2DState32(Hardware,
                                      0x0380C,
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                                      | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:1) - (0 ? 1:1) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:1) - (0 ? 1:1) + 1))))))) << (0 ? 1:1)))
                                      ));

        /* Semaphore & Stall. */
        gcmONERROR(gcoHARDWARE_Load2DState32(Hardware,
                                      0x03808,
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                                      | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                                      ));

        if (Hardware->hw2DCmdBuffer != gcvNULL)
        {
            Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
            Hardware->hw2DCmdIndex += 1;

            Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
            Hardware->hw2DCmdIndex += 1;
        }
        else
        {
            Hardware->hw2DCmdIndex += 2;
        }
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x03800,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (gcvPIPE_2D) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        ));

    if (Hardware->hw3DEngine)
    {
        /* Semaphore & Stall. */
        gcmONERROR(gcoHARDWARE_Load2DState32(Hardware,
                                      0x03808,
                                      ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                                      | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                                      ));

        if (Hardware->hw2DCmdBuffer != gcvNULL)
        {
            Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
            Hardware->hw2DCmdIndex += 1;

            Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
            Hardware->hw2DCmdIndex += 1;
        }
        else
        {
            Hardware->hw2DCmdIndex += 2;
        }
    }

    /* Reset all src compression field. */
    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
       gctUINT32 r[8] =
           {0xFFFE1000, 0xFFFE1000, 0xFFFE1000, 0xFFFE1000,
            0xFFFE1000, 0xFFFE1000, 0xFFFE1000, 0xFFFE1000};


       gcmONERROR(gcoHARDWARE_Load2DState(
           Hardware,
           0x12CC0, 8,
           r
           ));
    }

    if (Hardware->features[gcvFEATURE_2D_FC_SOURCE])
    {
        gctUINT32 r[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01720, 8,
            r
            ));
    }

OnError:

    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_End2DRender(
    IN gcoHARDWARE Hardware)
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x",
                    Hardware);

    /* Flush the 2D pipe. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0380C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))));

    /* Flush the Tile cache if available. */
    if (Hardware->features[gcvFEATURE_2D_FC_SOURCE]
        || Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x01650,
                    ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))));
    }

    if (Hardware->hw2DCurrentRenderCompressed && Hardware->hw2DCacheFlushAfterCompress)
    {
        if (Hardware->hw2DCmdBuffer != gcvNULL)
        {
            gcoOS_MemCopy(
                Hardware->hw2DCmdBuffer + Hardware->hw2DCmdIndex,
                Hardware->hw2DCacheFlushCmd,
                Hardware->hw2DCacheFlushAfterCompress * gcmSIZEOF(gctUINT32));
        }

        Hardware->hw2DCmdIndex += Hardware->hw2DCacheFlushAfterCompress;
    }

    /* Semaphore & Stall. */
    gcmONERROR(gcoHARDWARE_Load2DState32(Hardware,
                                  0x03808,
                                  ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                                  | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)))
                                  ));

    if (Hardware->hw2DCmdBuffer != gcvNULL)
    {
        Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x09 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27)));
        Hardware->hw2DCmdIndex += 1;

        Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) (0x07 & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))
                          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));
        Hardware->hw2DCmdIndex += 1;
    }
    else
    {
        Hardware->hw2DCmdIndex += 2;
    }

#if gcdENABLE_3D
    if (Hardware->hw3DEngine)
    {
        /* Select the 3D pipe. */
        gcmONERROR(gcoHARDWARE_Load2DState32(Hardware,
                              0x03800,
                              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (gcvPIPE_3D) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                              ));
    }
#endif

    if (Hardware->hw2DCmdBuffer != gcvNULL)
    {
        if (Hardware->hw2DCmdSize < Hardware->hw2DCmdIndex)
        {
            /* Fatal error: full all the buffer with NOP */
            gcmONERROR(gcvSTATUS_OUT_OF_MEMORY);
        }

        /* Fill redundant buffer with NOP */
        if (Hardware->hw2DCmdSize > Hardware->hw2DCmdIndex)
        {
            gcmONERROR(gcoHARDWARE_2DAppendNop(Hardware));
        }

        gcmASSERT(Hardware->hw2DCmdSize == Hardware->hw2DCmdIndex);

        /* Dump the command. */
        gcmDUMP_2D_COMMAND(Hardware->hw2DCmdBuffer, Hardware->hw2DCmdSize);
    }

OnError:

    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif  /*  gcdENABLE_2D*/


