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

/******************************************************************************\
****************************** gcoHARDWARE API Code *****************************
\******************************************************************************/

#if gcdENABLE_2D
/*******************************************************************************
**
**  _SetTargetCompression
**
**  Configure compression related for destination.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the destination surface descriptor.
**
**      gctUINT32_PTR Config
**          Pointer to the original config.
**
**  OUTPUT:
**
**      status.
*/
static gceSTATUS _SetTargetCompression(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN OUT gctUINT32_PTR Config
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 config = 0;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Config=0x%x", Hardware, Surface, Config);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Config != gcvNULL)
        config = *Config;

    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        if (Surface->tileStatusConfig == gcv2D_TSC_2D_COMPRESSED)
        {
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)));
            if (Surface->tileStatusFormat == gcvSURF_A8R8G8B8)
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18)));
            }
            else
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18)));
            }

            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01384,
                Surface->tileStatusGpuAddress
                ));

            gcmDUMP_2D_SURFACE(gcvFALSE, Surface->tileStatusGpuAddress);

        }
        else
        {
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:17) - (0 ? 17:17) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:17) - (0 ? 17:17) + 1))))))) << (0 ? 17:17)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 18:18) - (0 ? 18:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 18:18) - (0 ? 18:18) + 1))))))) << (0 ? 18:18)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));
        }
    }
#if gcdENABLE_THIRD_PARTY_OPERATION
    else if (Hardware->features[gcvFEATURE_TPC_COMPRESSION])
    {
        gctUINT32 len;
        gceSURF_FORMAT tpcFormat;

        if (Surface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED)
        {
            gcmONERROR(gcoTPHARDWARE_CheckSurface(
                Surface->node.physical,
                Surface->tileStatusGpuAddress,
                Surface->tileStatusFormat,
                Surface->alignedWidth,
                Surface->alignedHeight,
                Surface->stride,
                Surface->rotation
                ));

            gcmONERROR(gcoHARDWARE_TranslateXRGBFormat(
                Hardware,
                Surface->tileStatusFormat,
                &tpcFormat
                ));

            gcmONERROR(gcoTPHARDWARE_SetDstTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvTRUE,
                        Surface->node.physical,
                        Surface->tileStatusGpuAddress,
                        tpcFormat,
                        Surface->alignedWidth,
                        Surface->alignedHeight,
                        Surface->stride,
                        Surface->rotation
                        ));

            Hardware->hw2DCmdIndex += len;

            gcmDUMP_2D_SURFACE(gcvFALSE, Surface->tileStatusGpuAddress);
        }
        else
        {
            gcmONERROR(gcoTPHARDWARE_SetDstTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvFALSE,
                        0, 0, 0, 0, 0, 0, 0
                        ));

            Hardware->hw2DCmdIndex += len;
        }
    }
#endif

    if (Config != gcvNULL)
        *Config = config;

OnError:
    /* Return status. */
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetClipping
**
**  Set clipping rectangle.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
*       gcsRECT_PTR Rect
**          Pointer to a valid destination rectangle. The valid range of the
**          coordinates is 0..32768.  A pixel is valid if the following is true:
**              (pixelX >= Left) && (pixelX < Right) &&
**              (pixelY >= Top) && (pixelY < Bottom)
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetClipping(
    IN gcoHARDWARE Hardware,
    IN gcsRECT_PTR Rect
    )
{
    gceSTATUS status;
    gctUINT32 data[2];
    gcsRECT clippedRect;

    gcmHEADER_ARG("Hardware=0x%x Rect=0x%x", Hardware, Rect);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Clip Rect coordinates in positive range. */
    clippedRect.left = gcmMAX(Rect->left, 0);
    clippedRect.top = gcmMAX(Rect->top, 0);
    clippedRect.right = gcmMAX(Rect->right, 0);
    clippedRect.bottom = gcmMAX(Rect->bottom, 0);

    /* 0x01260 */
    data[0]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (clippedRect.left) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (clippedRect.top) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

    /* 0x01264 */
    data[1]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (clippedRect.right) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (clippedRect.bottom) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)));

    /* Load cllipping states. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware, 0x01260, 2, data
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetTarget
**
**  Configure destination.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the destination surface descriptor.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetTarget(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Filter,
    IN gce2D_YUV_COLOR_MODE Mode,
    IN gctINT32_PTR CscRGB2YUV,
    IN gctUINT32_PTR GammaTable,
    IN gctBOOL GdiStretch,
    OUT gctUINT32_PTR DestConfig
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUVformat, destConfig;
    gctUINT32 data[3];
    gctUINT32 rotated = 0;
    gctBOOL fullRot;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x", Hardware, Surface);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    fullRot = Filter ? Hardware->features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION]
                     : Hardware->features[gcvFEATURE_2D_BITBLIT_FULLROTATION];

    /* Check the rotation capability. */
    if (!fullRot && gcmGET_PRE_ROTATION(Surface->rotation) != gcvSURF_0_DEGREE)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (fullRot)
    {
        rotated = gcvFALSE;
    }
    else
    {
        /* Determine 90 degree rotation enable field. */
        if (gcmGET_PRE_ROTATION(Surface->rotation) == gcvSURF_0_DEGREE)
        {
            rotated = gcvFALSE;
        }
        else if (gcmGET_PRE_ROTATION(Surface->rotation) == gcvSURF_90_DEGREE)
        {
            rotated = gcvTRUE;
        }
        else
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    /* Dump the memory. */
    gcmDUMP_2D_SURFACE(gcvFALSE, Surface->node.physical);

    /* 0x01228 */
    data[0]
        = Surface->node.physical;

    /* 0x0122C */
    data[1]
        = Surface->stride;

    /* 0x01230 */
    data[2]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedWidth) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (rotated) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

    /* LoadState(AQDE_DEST_ADDRESS, 3), Address, Stride, rotation. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware,
        0x01228, 3,
        data
        ));

    if (fullRot)
    {
        gctUINT32 dstRot = 0;
        gctUINT32 value;

        gcmONERROR(gcoHARDWARE_TranslateDestinationRotation(gcmGET_PRE_ROTATION(Surface->rotation), &dstRot));

        if (!Filter)
        {
            /* Flush the 2D pipe before writing to the rotation register. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0380C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                ));
        }

        /* Load target height. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012B4,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedHeight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            ));

        /* 0x012BC */
        if (Hardware->shadowRotAngleReg)
        {
            value = ((((gctUINT32) (Hardware->rotAngleRegShadow)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3))) | (((gctUINT32) ((gctUINT32) (dstRot) & ((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3)));

            /* Save the shadow value. */
            Hardware->rotAngleRegShadow = value;
        }
        else
        {
            /* Enable dst mask. */
            value = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9)));

            /* Set dst rotation. */
            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3))) | (((gctUINT32) ((gctUINT32) (dstRot) & ((gctUINT32) ((((1 ? 5:3) - (0 ? 5:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:3) - (0 ? 5:3) + 1))))))) << (0 ? 5:3)));
        }

        if (Hardware->features[gcvFEATURE_2D_POST_FLIP])
        {
            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:23) - (0 ? 24:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:23) - (0 ? 24:23) + 1))))))) << (0 ? 24:23))) | (((gctUINT32) ((gctUINT32) (gcmGET_POST_ROTATION(Surface->rotation) >> 30) & ((gctUINT32) ((((1 ? 24:23) - (0 ? 24:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:23) - (0 ? 24:23) + 1))))))) << (0 ? 24:23)));

            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 25:25) - (0 ? 25:25) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:25) - (0 ? 25:25) + 1))))))) << (0 ? 25:25)));
        }
        else if (gcmGET_POST_ROTATION(Surface->rotation))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware, 0x012BC, value
            ));
    }

    /*******************************************************************
    ** Set the destination configuration register.
    */

    gcmONERROR(gcoHARDWARE_TranslateDestinationFormat(
        Hardware, Surface->format, Hardware->enableXRGB, &format, &swizzle, &isYUVformat
        ));

    /* Set endian control */
    destConfig = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));
    if (Hardware->bigEndian &&
        Surface->format != gcvSURF_NV16 &&
        Surface->format != gcvSURF_NV61)
    {
        gctUINT32 bpp;

        if (Filter)
        {
            /* Flush the current pipe. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0380C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                ));
        }

        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                             &bpp,
                                             gcvNULL));

        if (bpp == 16)
        {
            destConfig = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));
        }
        else if (bpp == 32)
        {
            destConfig = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));
        }
    }

    data[0] = ~0U;

    if (isYUVformat)
    {
        if (Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
        {
            /* Load dest UV swizzle and YUV mode state. */
            swizzle = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (swizzle) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

            switch (Mode)
            {
            case gcv2D_YUV_601:
                swizzle = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
                break;

            case gcv2D_YUV_709:
                swizzle = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
                break;

            case gcv2D_YUV_USER_DEFINED:
            case gcv2D_YUV_USER_DEFINED_CLAMP:
                swizzle = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));

                if (Mode == gcv2D_YUV_USER_DEFINED_CLAMP)
                {
                    swizzle = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
                    swizzle = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }

                gcmONERROR(gcoHARDWARE_UploadCSCTable(
                    Hardware,
                    gcvFALSE,
                    CscRGB2YUV
                    ));
                break;

            default:
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }

            data[0] = ((((gctUINT32) (swizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
        }
    }
    else
    {
        destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (swizzle) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
    }

    /* Set compression related config for target. */
    gcmONERROR(_SetTargetCompression(Hardware, Surface, &data[0]));

    if (data[0] != ~0U)
    {
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware, 0x0132C, data[0]));
    }

    destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 4:0) - (0 ? 4:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:0) - (0 ? 4:0) + 1))))))) << (0 ? 4:0)));

    switch (Surface->tiling)
    {
    case gcvLINEAR:
        destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        break;

    case gcvTILED:
        destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        break;

    case gcvMINORTILED:
        destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26)));
        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (!Filter)
    {
        destConfig |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (GdiStretch ? 0x1 : 0x0) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)));
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x0130C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))));

    if ((GammaTable != gcvNULL) && Hardware->features[gcvFEATURE_2D_GAMMA])
    {
        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x13C00,
            256,
            GammaTable
            ));

        destConfig = ((((gctUINT32) (destConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)));
    }

    destConfig = ((((gctUINT32) (destConfig)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));

    *DestConfig = destConfig;

    if (Hardware->features[gcvFEATURE_2D_FC_SOURCE])
    {
        gceHARDWARE_TYPE hwType;

        gcmGETCURRENTHARDWARE(hwType);
        if (hwType == gcvHARDWARE_3D2D)
        {
            gcmPRINT("WARN: Skip set shared register!");
        }
        else
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x03818,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                ));
        }
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS gcoHARDWARE_SetMultiTarget(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gceSURF_FORMAT SrcFormat
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gceSURF_FORMAT dstFormat = Surface->format;
    gctUINT32 data[5];

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x, SrcFormat=%d", Hardware, Surface, SrcFormat);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    switch (dstFormat)
    {
    case gcvSURF_I420:
    case gcvSURF_YV12:
        switch (SrcFormat)
        {
        case gcvSURF_YUY2:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)));
            break;

        case gcvSURF_UYVY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        case gcvSURF_YVYU:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            break;

        case gcvSURF_VYUY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvFALSE, Surface->node.physical2);
        gcmDUMP_2D_SURFACE(gcvFALSE, Surface->node.physical3);

        data[1] = Surface->node.physical2;
        data[2] = Surface->uStride;
        data[3] = Surface->node.physical3;
        data[4] = Surface->vStride;

        data[0] |=
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 7:6) - (0 ? 7:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:6) - (0 ? 7:6) + 1))))))) << (0 ? 7:6)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x0130C,
            5,
            data
            ));

        break;

    case gcvSURF_NV21:
    case gcvSURF_NV61:
        switch (SrcFormat)
        {
        case gcvSURF_YUY2:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            break;

        case gcvSURF_UYVY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        case gcvSURF_YVYU:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            break;

        case gcvSURF_VYUY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvFALSE, Surface->node.physical2);

        data[1] = Surface->node.physical2;
        data[2] = Surface->uStride;

        data[0] |=
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x0130C,
            3,
            data
            ));

        break;

    case gcvSURF_NV12:
    case gcvSURF_NV16:
        switch (SrcFormat)
        {
        case gcvSURF_YUY2:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            break;

        case gcvSURF_UYVY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        case gcvSURF_YVYU:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            break;

        case gcvSURF_VYUY:
            data[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 19:18) - (0 ? 19:18) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:18) - (0 ? 19:18) + 1))))))) << (0 ? 19:18)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
            data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 11:10) - (0 ? 11:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:10) - (0 ? 11:10) + 1))))))) << (0 ? 11:10)));
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvFALSE, Surface->node.physical2);

        data[1] = Surface->node.physical2;
        data[2] = Surface->uStride;

        data[0] |=
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 3:2) - (0 ? 3:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:2) - (0 ? 3:2) + 1))))))) << (0 ? 3:2)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)));

        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x0130C,
            3,
            data
            ));

        break;

    default:
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0130C,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))));

        break;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetTargetColorKeyRange
**
**  Setup the destination color key value in A8R8G8B8 format.
**  Write to Destination only if the RGB color channels match to the specified color.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Color
**          Destination color.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetTargetColorKeyRange(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorLow,
    IN gctUINT32 ColorHigh
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x ColorLow=%x ColorHigh=%x",
                  Hardware, ColorLow, ColorHigh);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DEngine && Hardware->features[gcvFEATURE_2DPE20] && !Hardware->sw2DEngine)
    {
        /* LoadState global color value. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012C4,
            ColorLow
            ));

        /* LoadState global color value. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012E0,
            ColorHigh
            ));
    }
    else
    {
        /* Not supported by PE1.0 hardware. */
        gcmONERROR(gcvSTATUS_SKIP);
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_ColorConvertToARGB8
**
**  Convert Color in target format to A8R8G8B8 color.
**  Only supports destination formats.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          Format of the destination surface.
**
**      UINT32 NumColors
**          Number of input color values.
**
**      UINT32_PTR Color
**          Color values in destination format.
**
**  OUTPUT:
**
**      gctUINT32_PTR Color32
**          Color values in ARGB8 format.
*/
gceSTATUS gcoHARDWARE_ColorConvertToARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 NumColors,
    IN gctUINT32_PTR Color,
    OUT gctUINT32_PTR Color32
    )
{
    gctUINT32 colorR, colorG, colorB, colorA;
    gctUINT32 i;

    gcmHEADER_ARG("Format=%d NumColors=%d Color=0x%x Color32=0x%x",
                    Format, NumColors, Color, Color32);

    for (i = 0; i < NumColors; i++)
    {
        gctUINT32 color = Color[i];
        gctUINT32_PTR color32 = &Color32[i];

        switch(Format)
        {
        case gcvSURF_X8R8G8B8:
        case gcvSURF_A8R8G8B8:
            /* No color conversion needed. */
            *color32 = color;
            continue;

        case gcvSURF_R8G8B8X8:
        case gcvSURF_R8G8B8A8:
            /* Extract colors. */
            colorB = (color & 0x0000FF00) >>  8;
            colorG = (color & 0x00FF0000) >> 16;
            colorR = (color & 0xFF000000) >> 24;
            colorA = (color & 0x000000FF);
            break;

        case gcvSURF_B8G8R8X8:
        case gcvSURF_B8G8R8A8:
            /* Extract colors. */
            colorB = (color & 0xFF000000) >> 24;
            colorG = (color & 0x00FF0000) >> 16;
            colorR = (color & 0x0000FF00) >>  8;
            colorA = (color & 0x000000FF);
            break;

        case gcvSURF_X8B8G8R8:
        case gcvSURF_A8B8G8R8:
            /* Extract colors. */
            colorB = (color & 0x00FF0000) >> 16;
            colorG = (color & 0x0000FF00) >>  8;
            colorR = (color & 0x000000FF);
            colorA = (color & 0xFF000000 >> 24);
            break;

        case gcvSURF_A1R5G5B5:
            /* Extract colors. */
            colorB = (color &   0x1F);
            colorG = (color &  0x3E0) >>  5;
            colorR = (color & 0x7C00) >> 10;
            colorA = (color & 0x8000) >> 15;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case gcvSURF_X1R5G5B5:
            /* Extract colors. */
            colorB = (color &   0x1F);
            colorG = (color &  0x3E0) >>  5;
            colorR = (color & 0x7C00) >> 10;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case gcvSURF_R5G5B5A1:
            /* Extract colors. */
            colorB = (color & 0x003E) >>  1;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0xF800) >> 11;
            colorA = (color & 0x0001);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case gcvSURF_R5G5B5X1:
            /* Extract colors. */
            colorB = (color & 0x003E) >>  1;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0xF800) >> 11;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case gcvSURF_B5G5R5A1:
            /* Extract colors. */
            colorB = (color & 0xF800) >> 11;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0x003E) >>  1;
            colorA = (color & 0x0001);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case gcvSURF_B5G5R5X1:
            /* Extract colors. */
            colorB = (color & 0xF800) >> 11;
            colorG = (color & 0x07C0) >>  6;
            colorR = (color & 0x003E) >>  1;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case gcvSURF_A1B5G5R5:
            /* Extract colors. */
            colorB = (color & 0x7C00) >> 10;
            colorG = (color &  0x3E0) >>  5;
            colorR = (color &   0x1F);
            colorA = (color & 0x8000) >> 15;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = colorA ? 0xFF : 0x00;
            break;

        case gcvSURF_X1B5G5R5:
            /* Extract colors. */
            colorB = (color & 0x7C00) >> 10;
            colorG = (color &  0x3E0) >>  5;
            colorR = (color &   0x1F);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 3) | (colorG >> 2);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case gcvSURF_A4R4G4B4:
            /* Extract colors. */
            colorB = (color &    0xF);
            colorG = (color &   0xF0) >>  4;
            colorR = (color &  0xF00) >>  8;
            colorA = (color & 0xF000) >> 12;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case gcvSURF_X4R4G4B4:
            /* Extract colors. */
            colorB = (color &    0xF);
            colorG = (color &   0xF0) >>  4;
            colorR = (color &  0xF00) >>  8;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = 0xFF;
            break;

        case gcvSURF_R4G4B4A4:
            /* Extract colors. */
            colorB = (color &   0xF0) >>  4;
            colorG = (color &  0xF00) >>  8;
            colorR = (color & 0xF000) >> 12;
            colorA = (color &    0xF);

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case gcvSURF_R4G4B4X4:
            /* Extract colors. */
            colorB = (color &   0xF0) >>  4;
            colorG = (color &  0xF00) >>  8;
            colorR = (color & 0xF000) >> 12;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = 0xFF;
            break;

        case gcvSURF_B4G4R4A4:
            /* Extract colors. */
            colorB = (color & 0xF000) >> 12;
            colorG = (color &  0xF00) >>  8;
            colorR = (color &   0xF0) >>  4;
            colorA = (color &    0xF);

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case gcvSURF_B4G4R4X4:
            /* Extract colors. */
            colorB = (color & 0xF000) >> 12;
            colorG = (color &  0xF00) >>  8;
            colorR = (color &   0xF0) >>  4;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = 0xFF;
            break;

        case gcvSURF_A4B4G4R4:
            /* Extract colors. */
            colorB = (color &  0xF00) >>  8;
            colorG = (color &   0xF0) >>  4;
            colorR = (color &    0xF);
            colorA = (color & 0xF000) >> 12;

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = colorA | (colorA << 4);
            break;

        case gcvSURF_X4B4G4R4:
            /* Extract colors. */
            colorB = (color &  0xF00) >>  8;
            colorG = (color &   0xF0) >>  4;
            colorR = (color &    0xF);

            /* Expand colors. */
            colorB = colorB | (colorB << 4);
            colorG = colorG | (colorG << 4);
            colorR = colorR | (colorR << 4);
            colorA = 0xFF;
            break;

        case gcvSURF_R5G6B5:
            /* Extract colors. */
            colorB = (color &   0x1F);
            colorG = (color &  0x7E0) >>  5;
            colorR = (color & 0xF800) >> 11;

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 2) | (colorG >> 4);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        case gcvSURF_B5G6R5:
            /* Extract colors. */
            colorB = (color & 0xF800) >> 11;
            colorG = (color &  0x7E0) >>  5;
            colorR = (color &   0x1F);

            /* Expand colors. */
            colorB = (colorB << 3) | (colorB >> 2);
            colorG = (colorG << 2) | (colorG >> 4);
            colorR = (colorR << 3) | (colorR >> 2);
            colorA = 0xFF;
            break;

        default:
            gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
            return gcvSTATUS_NOT_SUPPORTED;
        }

        /* Assemble. */
        *color32 =
           (colorA << 24)
         | (colorR << 16)
         | (colorG <<  8)
         | colorB;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_ColorConvertFromARGB8
**
**  Convert Color in A8R8G8B8 format to target format color.
**  Only supports destination formats.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          Format of the destination surface.
**
**      UINT32 NumColors
**          Number of input color values.
**
**      UINT32_PTR Color32
**          Color values in ARGB8 format.
**
**  OUTPUT:
**
**      gctUINT32_PTR Color
**          Color values in destination format.
*/
gceSTATUS gcoHARDWARE_ColorConvertFromARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 NumColors,
    IN gctUINT32_PTR Color32,
    OUT gctUINT32_PTR Color
    )
{
    gctUINT32 i;

    gcmHEADER_ARG("Format=%d NumColors=%d Color32=0x%x Color=0x%x",
                    Format, NumColors, Color32, Color);

    for (i = 0; i < NumColors; i++)
    {
        gctUINT32 color32 = Color32[i];
        gctUINT32 colorR, colorG, colorB, colorA;
        gctUINT32_PTR color = &Color[i];

        /* Extract colors. */
        colorB = (color32 & 0xFF);
        colorG = (color32 & 0xFF00) >>  8;
        colorR = (color32 & 0xFF0000) >> 16;
        colorA = (color32 & 0xFF000000) >> 24;

        switch(Format)
        {
        case gcvSURF_X8R8G8B8:
        case gcvSURF_A8R8G8B8:
            /* No color conversion needed. */
            *color = color32;
            break;

        case gcvSURF_R8G8B8X8:
        case gcvSURF_R8G8B8A8:
            *color =
                (colorA) |
                (colorR << 24) |
                (colorG << 16) |
                (colorB <<  8);
            break;

        case gcvSURF_B8G8R8X8:
        case gcvSURF_B8G8R8A8:
            *color =
                (colorA) |
                (colorR <<  8) |
                (colorG << 16) |
                (colorB << 24);
            break;

        case gcvSURF_X8B8G8R8:
        case gcvSURF_A8B8G8R8:
            *color =
                (colorA << 24) |
                (colorR) |
                (colorG <<  8) |
                (colorB << 16);
            break;

        case gcvSURF_A1R5G5B5:
        case gcvSURF_X1R5G5B5:
            *color =
                ((colorA >> 7) << 15) |
                ((colorR >> 3) << 10) |
                ((colorG >> 3) <<  5) |
                ((colorB >> 3));
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_R5G5B5A1:
        case gcvSURF_R5G5B5X1:
            *color =
                ((colorA >> 7)) |
                ((colorR >> 3) << 11) |
                ((colorG >> 3) <<  6) |
                ((colorB >> 3) <<  1);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_A1B5G5R5:
        case gcvSURF_X1B5G5R5:
            *color =
                ((colorA >> 7) << 15) |
                ((colorR >> 3)) |
                ((colorG >> 3) <<  5) |
                ((colorB >> 3) << 10);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_B5G5R5A1:
        case gcvSURF_B5G5R5X1:
            *color =
                ((colorA >> 7)) |
                ((colorR >> 3) <<  1) |
                ((colorG >> 3) <<  6) |
                ((colorB >> 3) << 11);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_A4R4G4B4:
        case gcvSURF_X4R4G4B4:
            *color =
                ((colorA >> 4) << 12) |
                ((colorR >> 4) <<  8) |
                ((colorG >> 4) <<  4) |
                ((colorB >> 4));
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_R4G4B4A4:
        case gcvSURF_R4G4B4X4:
            *color =
                ((colorA >> 4)) |
                ((colorR >> 4) << 12) |
                ((colorG >> 4) <<  8) |
                ((colorB >> 4) <<  4);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_A4B4G4R4:
        case gcvSURF_X4B4G4R4:
            *color =
                ((colorA >> 4) << 12) |
                ((colorR >> 4)) |
                ((colorG >> 4) <<  4) |
                ((colorB >> 4) <<  8);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_B4G4R4A4:
        case gcvSURF_B4G4R4X4:
            *color =
                ((colorA >> 4)) |
                ((colorR >> 4) <<  4) |
                ((colorG >> 4) <<  8) |
                ((colorB >> 4) << 12);
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_R5G6B5:
            *color =
                ((colorR >> 3) << 11) |
                ((colorG >> 2) <<  5) |
                ((colorB >> 3));
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        case gcvSURF_B5G6R5:
            *color =
                ((colorR >> 3) |
                ((colorG >> 2) <<  5) |
                ((colorB >> 3) << 11));
            /* Expand to 32bit. */
            *color = (*color << 16) | *color;
            break;

        default:
            gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
            return gcvSTATUS_NOT_SUPPORTED;
        }
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_ColorPackFromARGB8
**
**  Pack Color in A8R8G8B8 format to target format color.
**  Needed for backward compatibility.
**  Only supports ARGB source formats existing in old PE.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          Format of the source surface.
**
**      UINT32 Color32
**          Color values in ARGB8 format.
**
**  OUTPUT:
**
**      gctUINT32_PTR Color
**          Color values in destination format.
*/
gceSTATUS gcoHARDWARE_ColorPackFromARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 Color32,
    OUT gctUINT32_PTR Color
    )
{
    gctUINT32 colorR, colorG, colorB, colorA;

    gcmHEADER_ARG("Format=%d Color32=%x Color=0x%x",
                    Format, Color32, Color);

    /* Extract colors. */
    colorB = (Color32 & 0xFF);
    colorG = (Color32 & 0xFF00) >>  8;
    colorR = (Color32 & 0xFF0000) >> 16;
    colorA = (Color32 & 0xFF000000) >> 24;

    switch(Format)
    {
    case gcvSURF_X8R8G8B8:
    case gcvSURF_A8R8G8B8:
        /* No color packing needed. */
        *Color = Color32;
        break;

    case gcvSURF_R8G8B8X8:
    case gcvSURF_R8G8B8A8:
        /* No color packing needed. */
        *Color =
            ((colorA & 0xFF)) |
            ((colorR & 0xFF) << 24) |
            ((colorG & 0xFF) << 16) |
            ((colorB & 0xFF) <<  8);
        break;

    case gcvSURF_X8B8G8R8:
    case gcvSURF_A8B8G8R8:
        /* No color packing needed. */
        *Color =
            ((colorA & 0xFF) << 24) |
            ((colorR & 0xFF)) |
            ((colorG & 0xFF) <<  8) |
            ((colorB & 0xFF) << 16);
        break;

    case gcvSURF_B8G8R8X8:
    case gcvSURF_B8G8R8A8:
        /* No color packing needed. */
        *Color =
            ((colorA & 0xFF)) |
            ((colorR & 0xFF) <<  8) |
            ((colorG & 0xFF) << 16) |
            ((colorB & 0xFF) << 24);
        break;

    case gcvSURF_A1R5G5B5:
    case gcvSURF_X1R5G5B5:
        *Color =
            ((colorA & 0x01) << 15) |
            ((colorR & 0x1F) << 10) |
            ((colorG & 0x1F) <<  5) |
            ((colorB & 0x1F));
        break;

    case gcvSURF_R5G5B5A1:
    case gcvSURF_R5G5B5X1:
        *Color =
            ((colorA & 0x01)) |
            ((colorR & 0x1F) << 11) |
            ((colorG & 0x1F) <<  6) |
            ((colorB & 0x1F) <<  1);
        break;

    case gcvSURF_A1B5G5R5:
    case gcvSURF_X1B5G5R5:
        *Color =
            ((colorA & 0x01) << 15) |
            ((colorR & 0x1F)) |
            ((colorG & 0x1F) <<  5) |
            ((colorB & 0x1F) << 10);
        break;

    case gcvSURF_B5G5R5A1:
    case gcvSURF_B5G5R5X1:
        *Color =
            ((colorA & 0x01)) |
            ((colorR & 0x1F) <<  1) |
            ((colorG & 0x1F) <<  6) |
            ((colorB & 0x1F) << 11);
        break;

    case gcvSURF_A4R4G4B4:
    case gcvSURF_X4R4G4B4:
        *Color =
            ((colorA & 0xF) << 12) |
            ((colorR & 0xF) <<  8) |
            ((colorG & 0xF) <<  4) |
            ((colorB & 0xF));
        break;

    case gcvSURF_R4G4B4A4:
    case gcvSURF_R4G4B4X4:
        *Color =
            ((colorA & 0xF)) |
            ((colorR & 0xF) << 12) |
            ((colorG & 0xF) <<  8) |
            ((colorB & 0xF) <<  4);
        break;

    case gcvSURF_A4B4G4R4:
    case gcvSURF_X4B4G4R4:
        *Color =
            ((colorA & 0xF) << 12) |
            ((colorR & 0xF)) |
            ((colorG & 0xF) <<  4) |
            ((colorB & 0xF) <<  8);
        break;

    case gcvSURF_B4G4R4A4:
    case gcvSURF_B4G4R4X4:
        *Color =
            ((colorA & 0xF)) |
            ((colorR & 0xF) <<  4) |
            ((colorG & 0xF) <<  8) |
            ((colorB & 0xF) << 12);
        break;

    case gcvSURF_R5G6B5:
        *Color =
            ((colorR & 0x1F) << 11) |
            ((colorG & 0x3F) <<  5) |
            ((colorB & 0x1F));
        break;

    case gcvSURF_B5G6R5:
        *Color =
            ((colorR & 0x1F)) |
            ((colorG & 0x3F) <<  5) |
            ((colorB & 0x1F) << 11);
        break;

    case gcvSURF_A8:
        *Color = colorA;
        break;

    default:
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_ColorPackToARGB8
**
**  Pack Color in target format color to A8R8G8B8 format.
**  Needed for backward compatibility.
**  Only supports ARGB source formats existing in old PE.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          Format of the source surface.
**
**      UINT32 Color
**          Color values in Format.
**
**  OUTPUT:
**
**      gctUINT32_PTR Color32
**          Color values in ARGB8 format.
*/
gceSTATUS gcoHARDWARE_ColorPackToARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 Color,
    OUT gctUINT32_PTR Color32
    )
{
    gctUINT32 colorR, colorG, colorB, colorA;

    gcmHEADER_ARG("Format=%d Color=%x Color32=0x%x",
                    Format, Color, Color32);

    switch(Format)
    {
    case gcvSURF_X8R8G8B8:
    case gcvSURF_A8R8G8B8:
        /* Extract colors. */
        colorB = (Color & 0x000000FF);
        colorG = (Color & 0x0000FF00) >>  8;
        colorR = (Color & 0x00FF0000) >> 16;
        colorA = (Color & 0xFF000000) >> 24;
        break;

    case gcvSURF_R8G8B8X8:
    case gcvSURF_R8G8B8A8:
        /* Extract colors. */
        colorB = (Color & 0x0000FF00) >>  8;
        colorG = (Color & 0x00FF0000) >> 16;
        colorR = (Color & 0xFF000000) >> 24;
        colorA = (Color & 0x000000FF);
        break;

    case gcvSURF_B8G8R8X8:
    case gcvSURF_B8G8R8A8:
        /* Extract colors. */
        colorB = (Color & 0xFF000000) >> 24;
        colorG = (Color & 0x00FF0000) >> 16;
        colorR = (Color & 0x0000FF00) >>  8;
        colorA = (Color & 0x000000FF);
        break;

    case gcvSURF_X8B8G8R8:
    case gcvSURF_A8B8G8R8:
        /* Extract colors. */
        colorB = (Color & 0x00FF0000) >> 16;
        colorG = (Color & 0x0000FF00) >>  8;
        colorR = (Color & 0x000000FF);
        colorA = (Color & 0xFF000000 >> 24);
        break;

    case gcvSURF_A1R5G5B5:
        /* Extract colors. */
        colorB = (Color &   0x1F);
        colorG = (Color &  0x3E0) >>  5;
        colorR = (Color & 0x7C00) >> 10;
        colorA = (Color & 0x8000) >> 15;
        break;

    case gcvSURF_X1R5G5B5:
        /* Extract colors. */
        colorB = (Color &   0x1F);
        colorG = (Color &  0x3E0) >>  5;
        colorR = (Color & 0x7C00) >> 10;
        colorA = 0xFF;
        break;

    case gcvSURF_R5G5B5A1:
        /* Extract colors. */
        colorB = (Color & 0x003E) >>  1;
        colorG = (Color & 0x07C0) >>  6;
        colorR = (Color & 0xF800) >> 11;
        colorA = (Color & 0x0001);
        break;

    case gcvSURF_R5G5B5X1:
        /* Extract colors. */
        colorB = (Color & 0x003E) >>  1;
        colorG = (Color & 0x07C0) >>  6;
        colorR = (Color & 0xF800) >> 11;
        colorA = 0xFF;
        break;

    case gcvSURF_B5G5R5A1:
        /* Extract colors. */
        colorB = (Color & 0xF800) >> 11;
        colorG = (Color & 0x07C0) >>  6;
        colorR = (Color & 0x003E) >>  1;
        colorA = (Color & 0x0001);
        break;

    case gcvSURF_B5G5R5X1:
        /* Extract colors. */
        colorB = (Color & 0xF800) >> 11;
        colorG = (Color & 0x07C0) >>  6;
        colorR = (Color & 0x003E) >>  1;
        colorA = 0xFF;
        break;

    case gcvSURF_A1B5G5R5:
        /* Extract colors. */
        colorB = (Color & 0x7C00) >> 10;
        colorG = (Color &  0x3E0) >>  5;
        colorR = (Color &   0x1F);
        colorA = (Color & 0x8000) >> 15;
        break;

    case gcvSURF_X1B5G5R5:
        /* Extract colors. */
        colorB = (Color & 0x7C00) >> 10;
        colorG = (Color &  0x3E0) >>  5;
        colorR = (Color &   0x1F);
        colorA = 0xFF;
        break;

    case gcvSURF_A4R4G4B4:
        /* Extract colors. */
        colorB = (Color &    0xF);
        colorG = (Color &   0xF0) >>  4;
        colorR = (Color &  0xF00) >>  8;
        colorA = (Color & 0xF000) >> 12;
        break;

    case gcvSURF_X4R4G4B4:
        /* Extract colors. */
        colorB = (Color &    0xF);
        colorG = (Color &   0xF0) >>  4;
        colorR = (Color &  0xF00) >>  8;
        colorA = 0xFF;
        break;

    case gcvSURF_R4G4B4A4:
        /* Extract colors. */
        colorB = (Color &   0xF0) >>  4;
        colorG = (Color &  0xF00) >>  8;
        colorR = (Color & 0xF000) >> 12;
        colorA = (Color &    0xF);
        break;

    case gcvSURF_R4G4B4X4:
        /* Extract colors. */
        colorB = (Color &   0xF0) >>  4;
        colorG = (Color &  0xF00) >>  8;
        colorR = (Color & 0xF000) >> 12;
        colorA = 0xFF;
        break;

    case gcvSURF_B4G4R4A4:
        /* Extract colors. */
        colorB = (Color & 0xF000) >> 12;
        colorG = (Color &  0xF00) >>  8;
        colorR = (Color &   0xF0) >>  4;
        colorA = (Color &    0xF);
        break;

    case gcvSURF_B4G4R4X4:
        /* Extract colors. */
        colorB = (Color & 0xF000) >> 12;
        colorG = (Color &  0xF00) >>  8;
        colorR = (Color &   0xF0) >>  4;
        colorA = 0xFF;
        break;

    case gcvSURF_A4B4G4R4:
        /* Extract colors. */
        colorB = (Color &  0xF00) >>  8;
        colorG = (Color &   0xF0) >>  4;
        colorR = (Color &    0xF);
        colorA = (Color & 0xF000) >> 12;
        break;

    case gcvSURF_X4B4G4R4:
        /* Extract colors. */
        colorB = (Color &  0xF00) >>  8;
        colorG = (Color &   0xF0) >>  4;
        colorR = (Color &    0xF);
        colorA = 0xFF;
        break;

    case gcvSURF_R5G6B5:
        /* Extract colors. */
        colorB = (Color &   0x1F);
        colorG = (Color &  0x7E0) >>  5;
        colorR = (Color & 0xF800) >> 11;
        colorA = 0xFF;
        break;

    case gcvSURF_B5G6R5:
        /* Extract colors. */
        colorB = (Color & 0xF800) >> 11;
        colorG = (Color &  0x7E0) >>  5;
        colorR = (Color &   0x1F);
        colorA = 0xFF;
        break;

    case gcvSURF_A8:
        /* Extract colors. */
        colorB = 0;
        colorG = 0;
        colorR = 0;
        colorA = Color & 0xFF;
        break;

    default:
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Assemble. */
    *Color32 =
       (colorA << 24)
     | (colorR << 16)
     | (colorG <<  8)
     |  colorB;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetTargetGlobalColor
**
**  Setup the target global color value in ARGB8 format.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Color
**          Target color.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetTargetGlobalColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Color
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x Color=%x", Hardware, Color);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->features[gcvFEATURE_2DPE20])
    {
        /* LoadState global color value. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012CC,
            Color
            ));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_2D */

/*******************************************************************************
**
**  gcoHARDWARE_TranslateDestinationFormat
**
**  Translate API destination color format to its hardware value.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gceSURF_FORMAT APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateDestinationFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT APIValue,
    IN gctBOOL EnableXRGB,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

#if gcdENABLE_2D
    gcmHEADER_ARG("Hardware=0x%x APIValue=%d HwValue=0x%x "
                    "HwSwizzleValue=0x%x HwIsYUVValue=0x%x",
                    Hardware, APIValue, HwValue,
                    HwSwizzleValue, HwIsYUVValue);

    gcmGETHARDWARE(Hardware);

    Hardware->enableXRGB = EnableXRGB;
    gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
        Hardware, APIValue, HwValue, HwSwizzleValue, HwIsYUVValue
        ));

    /* Check if format is supported as destination. */
    switch (*HwValue)
    {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x10:
    case 0x7:
    case 0x8:
        break;

    case 0x0F:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
        /* Check whether HW support YUV output. */
        if (Hardware->hw2DDoMultiDst)
        {
            *HwSwizzleValue = 0;
            break;
        }

        if (Hardware->features[gcvFEATURE_TPC_COMPRESSION] &&
            (*HwValue == 0x13 ||
             *HwValue == 0x14))
        {
            break;
        }

        *HwValue = *HwSwizzleValue = *HwIsYUVValue = 0;
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        /*fall through*/

    default:
        /* Not supported. */
        *HwValue = *HwSwizzleValue = *HwIsYUVValue = 0;
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
#endif  /* gcdENABLE_2D */
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateDestinationTransparency
**
**  Translate API transparency mode to its hardware value.
**  MASK transparency mode is reserved.
**
**  INPUT:
**
**      gce2D_TRANSPARENCY APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateDestinationTransparency(
    IN gce2D_TRANSPARENCY APIValue,
    OUT gctUINT32 * HwValue
    )
{
#if gcdENABLE_2D
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcv2D_OPAQUE:
        *HwValue = 0x0;
        break;

    case gcv2D_KEYED:
        *HwValue = 0x2;
        break;

    default:
        /* Not supported. */
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Success. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);
    return gcvSTATUS_OK;
#else
    return gcvSTATUS_NOT_SUPPORTED;
#endif  /* gcdENABLE_2D */
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateDestinationRotation
**
**  Translate API transparency mode to its hardware value.
**
**  INPUT:
**
**      gce2D_TRANSPARENCY APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
#if gcdENABLE_2D
gceSTATUS gcoHARDWARE_TranslateDestinationRotation(
    IN gceSURF_ROTATION APIValue,
    OUT gctUINT32 * HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcvSURF_0_DEGREE:
        *HwValue = 0x0;
        break;

    case gcvSURF_90_DEGREE:
        *HwValue = 0x4;
        break;

    case gcvSURF_180_DEGREE:
        *HwValue = 0x5;
        break;

    case gcvSURF_270_DEGREE:
        *HwValue = 0x6;
        break;

    case gcvSURF_FLIP_X:
        *HwValue = 0x1;
        break;

    case gcvSURF_FLIP_Y:
        *HwValue = 0x2;
        break;

    default:
        /* Not supported. */
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Success. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);
    return gcvSTATUS_OK;
}

gceSTATUS
gcoHARDWARE_SetDither2D(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gctUINT32 memory[2];

    gcmHEADER_ARG("Hardware=0x%x Enable=%d", Hardware, Enable);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if ((Hardware->features[gcvFEATURE_2D_DITHER] == gcvFALSE) && Enable)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Set states into temporary buffer. */
    memory[0] = Enable ? 0x6E4CA280 : ~0;
    memory[1] = Enable ? 0x5D7F91B3 : ~0;

    /* Through load state command. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware, 0x012E8, 2, memory
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetBitBlitMirror
**
**  Enable/disable 2D BitBlt mirrorring.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL HorizontalMirror
**          Horizontal mirror enable flag.
**
**      gctBOOL VerticalMirror
**          Vertical mirror enable flag.
**
**  OUTPUT:
**
**      gceSTATUS
**          Returns gcvSTATUS_OK if successful.
*/
gceSTATUS gcoHARDWARE_SetBitBlitMirror(
    IN gcoHARDWARE Hardware,
    IN gctBOOL HorizontalMirror,
    IN gctBOOL VerticalMirror,
    IN gctBOOL DstMirror
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x HorizontalMirror=%d VerticalMirror=%d",
                    Hardware, HorizontalMirror, VerticalMirror);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    do
    {
        if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
        {
            gctUINT32 mirror;
            gctUINT32 config;

            if (Hardware->features[gcvFEATURE_2D_MIRROR_EXTENSION])
            {
                /* Determine the mirror value. */
                if (HorizontalMirror)
                {
                    if (VerticalMirror)
                    {
                        mirror = 0x3;
                    }
                    else
                    {
                        mirror = 0x1;
                    }
                }
                else
                {
                    if (VerticalMirror)
                    {
                        mirror = 0x2;
                    }
                    else
                    {
                        mirror = 0x0;
                    }
                }

                /* Enable the mirror. */
                if (DstMirror)
                {
                    config = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) (mirror) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));

                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)));
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }
                else
                {
                    config = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (mirror) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)));

                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));
                }

                /* Set mirror configuration. */
                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x012BC,
                    config
                    ));
            }
            else
            {
                /* Determine the mirror value. */
                if (HorizontalMirror)
                {
                    if (VerticalMirror)
                    {
                        mirror = 0x3;
                    }
                    else
                    {
                        mirror = 0x1;
                    }
                }
                else
                {
                    if (VerticalMirror)
                    {
                        mirror = 0x2;
                    }
                    else
                    {
                        mirror = 0x0;
                    }
                }

                /* Configure the mirror. */
                config
                    = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (((mirror == 0x0) ? 0x0 : 0x1)) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
                    | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (mirror) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)));

                /* Set mirror configuration. */
                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x0126C,
                    config
                    ));
            }
        }
        else
        {
            /* 2D clear is not currently supported by the software renderer. */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }
    while (gcvFALSE);

    /* Success. */
    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_Set2DClearColor
**
**  Set 2D clear color.
**  For PE 2.0, the color is specified in A8R8G8B8 format.
**  For older PE, the color is specified in destination format.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Color
**          32-bit clear color value.
**
**      gctBOOL ColorConvert
**          If set to gcvTRUE, the 32-bit values in the table are assumed to be
**          in ARGB8 format and will be converted by the hardware to the
**          destination format as needed.
**          If set to gcvFALSE, the 32-bit values in the table are assumed to be
**          preconverted to the destination format.
**
**  OUTPUT:
**
**      gceSTATUS
**          Returns gcvSTATUS_OK if successful.
*/
gceSTATUS gcoHARDWARE_Set2DClearColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Color,
    IN gceSURF_FORMAT DstFormat
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Color=%x DstFormat=%d",
                    Hardware, Color, DstFormat);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        if (Hardware->features[gcvFEATURE_2DPE20])
        {
            /* LoadState(AQDE_CLEAR_PIXEL_VALUE_LOW, 1), LoColor. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012C0,
                Color
                ));
        }
        else
        {
            /* Convert color in to destination format. */
            gcmONERROR(gcoHARDWARE_ColorConvertFromARGB8(
                DstFormat,
                1,
                &Color,
                &Color
                ));

            /* LoadState(AQDE_CLEAR_BYTE_MASK, 1), 0xFF. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01268,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
                ));

            /* LoadState(AQDE_CLEAR_PIXEL_VALUE_LOW, 1), LoColor. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01270,
                Color
                ));

            /* LoadState(AQDE_CLEAR_PIXEL_VALUE_HIGH, 1), HiColor. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01274,
                Color
                ));
        }
    }
    else
    {
        /* 2D clear is not currently supported by the software renderer. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return result. */
    gcmFOOTER();
    return status;
}

gctBOOL gcoHARDWARE_NeedUserCSC(gce2D_YUV_COLOR_MODE Mode, gceSURF_FORMAT Format)
{
    if ((Mode == gcv2D_YUV_USER_DEFINED_CLAMP)
        || (Mode == gcv2D_YUV_USER_DEFINED))
    {
        switch (Format)
        {
            case gcvSURF_YUY2:
            case gcvSURF_YVYU:
            case gcvSURF_UYVY:
            case gcvSURF_VYUY:
            case gcvSURF_YV12:
            case gcvSURF_I420:
            case gcvSURF_NV12:
            case gcvSURF_NV21:
            case gcvSURF_NV16:
            case gcvSURF_NV61:
                return gcvTRUE;
            default:
                return gcvFALSE;
        }
    }

    return gcvFALSE;
}

gceSTATUS
gcoHARDWARE_SetSuperTileVersion(
    IN gcoHARDWARE Hardware,
    IN gce2D_SUPER_TILE_VERSION Version
    )
{
    gceSTATUS status;
    gctUINT value = 0;

    gcmHEADER_ARG("Hardware=0x%x Version=%d", Hardware, Version);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (!Hardware->features[gcvFEATURE_2D_SUPER_TILE_VERSION])
    {
        gcmFOOTER_ARG("status = %d", gcvSTATUS_SKIP);
        return gcvSTATUS_SKIP;
    }

    switch (Version)
    {
    case gcv2D_SUPER_TILE_VERSION_V1:
        value = 0x0;
        break;

    case gcv2D_SUPER_TILE_VERSION_V2:
        value = 0x1;
        break;

    case gcv2D_SUPER_TILE_VERSION_V3:
        value = 0x2;
        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Through load state command. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x01328,
        (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (value) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))))
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_Query2DSurfaceAllocationInfo(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo,
    IN OUT gctUINT* Bytes,
    IN OUT gctUINT* Alignment)
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x SurfInfo=%08X", Hardware, SurfInfo);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* MinorFeature3 DE Enhancements1 .*/
    if (Hardware->hw2DBlockSize)
    {
        *Bytes += 256;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_2D */


