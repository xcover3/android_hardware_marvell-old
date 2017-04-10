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
/*******************************************************************************
**
**  _SetSourceTileStatus
**
**  Configure the source tile status.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface descriptor.
**
**  OUTPUT:
**
**      gctBOOL CacheMode
**          If gcvTRUE, need to enable the cache mode in source config.
*/
static gceSTATUS
_SetSourceTileStatus(
    IN gcoHARDWARE Hardware,
    IN gctUINT RegGroupIndex,
    IN gcsSURF_INFO_PTR Source,
    OUT gctBOOL *CacheMode
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT regOffset = RegGroupIndex << 2;
    gctBOOL cacheMode = gcvFALSE;
    gceHARDWARE_TYPE hwType;

    gcmHEADER_ARG("Hardware=0x%x RegGroupIndex=%d Source=0x%x CacheMode=0x%x",
                    Hardware, RegGroupIndex, Source, CacheMode);

    if (!Hardware->features[gcvFEATURE_2D_FC_SOURCE])
    {
        gcmFOOTER_NO();
        return gcvSTATUS_SKIP;
    }

    gcmGETCURRENTHARDWARE(hwType);
    if (hwType == gcvHARDWARE_3D2D)
    {
        gcmPRINT("WARN: Skip set shared register!");

        gcmFOOTER_NO();
        return gcvSTATUS_SKIP;
    }

    if ((Source->tileStatusConfig == gcv2D_TSC_DISABLE)
        || (Source->tileStatusConfig == gcv2D_TSC_2D_COMPRESSED)
        || (Source->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED))
    {
        /* Disable tile status. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01720 + regOffset,
            0x00000000
            ));
    }
    else
    {
        gctUINT32 config;

        if (Source->node.physical & 0xFF)
        {
            gcmONERROR(gcvSTATUS_NOT_ALIGNED);
        }

        if (Source->tileStatusConfig & gcv2D_TSC_ENABLE)
        {
            config = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
        }
        else if (Source->tileStatusConfig & gcv2D_TSC_COMPRESSED)
        {
            config = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
        }
        else
        {
            config = 0;
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        if (Source->tileStatusConfig & (gcv2D_TSC_COMPRESSED | gcv2D_TSC_DOWN_SAMPLER))
        {
            gctUINT32 format = 0;

            switch (Source->tileStatusFormat)
            {
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

            case gcvSURF_A8R8G8B8:
                format = 0x3;
                break;

            case gcvSURF_X8R8G8B8:
                format = 0x4;
                break;

            default:
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 7:4) - (0 ? 7:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:4) - (0 ? 7:4) + 1))))))) << (0 ? 7:4)));
        }

        if (Source->tileStatusConfig & gcv2D_TSC_DOWN_SAMPLER)
        {
            cacheMode = gcvTRUE;
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }
        else
        {
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01720 + regOffset,
            config
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01740 + regOffset,
            Source->tileStatusGpuAddress
            ));

        gcmDUMP_2D_SURFACE(gcvTRUE, Source->tileStatusGpuAddress);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01760 + regOffset,
            Source->tileStatusClearValue
            ));
    }

    if (CacheMode != gcvNULL)
    {
        *CacheMode = cacheMode;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;

}

/*******************************************************************************
**
**  _SetSourceCompression
**
**  Configure compression related for source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the destination surface descriptor.
**
**      gctUINT32_PTR Index
**          Surface index in source group.
**
**      gctUINT32_PTR Config
**          Pointer to the original config.
**
**  OUTPUT:
**
**      status.
*/
static gceSTATUS
_SetSourceCompression(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 Index,
    IN gctBOOL MultiSrc,
    IN OUT gctUINT32_PTR Config
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 config = 0;
    gctUINT32 regOffset = Index << 2;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x Index=0x%x Config=0x%x", Hardware, Surface, Index, Config);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Config != gcvNULL)
        config = *Config;

    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        if (Surface->tileStatusConfig == gcv2D_TSC_2D_COMPRESSED)
        {
            if (!MultiSrc)
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));
                if (Surface->tileStatusFormat == gcvSURF_A8R8G8B8)
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }
                else
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }

                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x12EE0,
                    Surface->tileStatusGpuAddress
                    ));
            }
            else
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));
                if (Surface->tileStatusFormat == gcvSURF_A8R8G8B8)
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }
                else
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }

                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x12EE0 + regOffset,
                    Surface->tileStatusGpuAddress
                    ));
            }
        }
        else
        {
            if (!MultiSrc)
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));
            }
            else
            {
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 13:13) - (0 ? 13:13) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:13) - (0 ? 13:13) + 1))))))) << (0 ? 13:13)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1))))))) << (0 ? 14:14)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));
            }
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

            gcmONERROR(gcoTPHARDWARE_SetSrcTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvTRUE, Index,
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
            gcmONERROR(gcoTPHARDWARE_SetSrcTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvFALSE, Index,
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
**  gcoHARDWARE_GetCompressionCmdSize
**
**  Get command size of compression.
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
**      gctUINT CompressNum
**          Compressed source surface number.
**
**      gce2D_COMMAND Command
**          2D command type.
**
**  OUTPUT:
**
**      gctUINT32 CmdSize
**          Command size.
*/
gceSTATUS
gcoHARDWARE_GetCompressionCmdSize(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gctUINT CompressNum,
    IN gce2D_COMMAND Command,
    OUT gctUINT32 *CmdSize
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT size = 0;

    gcmHEADER_ARG("Hardware=0x%x, State=0x%x, SrcSurf=0x%x DstSurf=0x%x Num=%d Command=%d",
        Hardware, State, CompressNum, SrcSurface, DstSurface, Command);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        if (Command != gcv2D_FILTER_BLT)
        {
            size += 10 + 2 + Hardware->hw2DCacheFlushAfterCompress;
        }
        else
        {
            Hardware->hw2DCmdIndex += 10 + 2;
        }
    }
#if gcdENABLE_THIRD_PARTY_OPERATION
    else if (Hardware->features[gcvFEATURE_TPC_COMPRESSION])
    {
        gctUINT len, tpcSize = 0;

        if (Command != gcv2D_FILTER_BLT)
        {
            if (!CompressNum &&
                State->dstSurface.tileStatusConfig != gcv2D_TSC_TPC_COMPRESSED)
            {
                gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_NO_COMPRESSION, 1, &tpcSize));
            }
            else
            {
                gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_HAS_COMPRESSION, 1, &len));
                tpcSize += len;

                if (CompressNum)
                {
                    gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_SRC_COMPRESSION, CompressNum, &len));
                    tpcSize += len;
                }
                if (State->dstSurface.tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED)
                {
                    gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_DST_COMPRESSION, 1, &len));
                    tpcSize += len;
                }
            }
        }
        else
        {
            if (SrcSurface->tileStatusConfig != gcv2D_TSC_TPC_COMPRESSED &&
                DstSurface->tileStatusConfig != gcv2D_TSC_TPC_COMPRESSED)
            {
                gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_NO_COMPRESSION, 1, &tpcSize));
            }
            else
            {
                gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_HAS_COMPRESSION, 1, &len));
                tpcSize += len;

                if (SrcSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED)
                {
                    gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_SRC_COMPRESSION, 1, &len));
                    tpcSize += len;
                }
                if (DstSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED)
                {
                    gcmONERROR(gcoTPHARDWARE_QueryStateCmdLen(gcvTP_CMD_DST_COMPRESSION, 1, &len));
                    tpcSize += len;
                }
            }
        }

        /* 2 bytes for destination fetch. */
        size += tpcSize + 2;
    }
#endif

    if (CmdSize != gcvNULL)
        *CmdSize = size;

OnError:
    /* Return status. */
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetCompression
**
**  Configure the source tile status.
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
**      gce2D_COMMAND Command
**          2D command type.
**
**  OUTPUT:
**
**      None
*/
gceSTATUS
gcoHARDWARE_SetCompression(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gce2D_COMMAND Command,
    IN gctBOOL AnyCompress
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x, State=0x%x, SrcSurf=0x%x DstSurf=0x%x Command=%d Compressed=%d",
        Hardware, State, SrcSurface, DstSurface, Command, AnyCompress);

    gcmGETHARDWARE(Hardware);

    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        gctUINT32 config = ~0U;

        if (Command != gcv2D_FILTER_BLT)
        {
            if (AnyCompress)
            {
                config = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))));
            }
            else
            {
                config = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))));
            }

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x01328,
                config
                ));
        }
    }
#if gcdENABLE_THIRD_PARTY_OPERATION
    else if (Hardware->features[gcvFEATURE_TPC_COMPRESSION])
    {
        gctUINT32 len;
        gcs2D_MULTI_SOURCE_PTR curSrc = &State->multiSrc[State->currentSrcIndex];

        if (AnyCompress)
        {
            gcmONERROR(gcoTPHARDWARE_EnableTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvTRUE));

            Hardware->hw2DCmdIndex += len;
        }
        else
        {
            gcmONERROR(gcoTPHARDWARE_EnableTPCCompression(
                        &Hardware->hw2DCmdBuffer[Hardware->hw2DCmdIndex],
                        &len, gcvFALSE));

            Hardware->hw2DCmdIndex += len;
        }

        if (Command == gcv2D_FILTER_BLT &&
            DstSurface->tileStatusConfig == gcv2D_TSC_TPC_COMPRESSED &&
            (curSrc->horMirror || curSrc->verMirror))
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012B0,
                (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))))
                ));
        }
        else
        {
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012B0,
                (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))))
                ));
        }
    }
#endif


OnError:
    /* Return status. */
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetMonochromeSource
**
**  Configure color source for the PE 2.0 engine.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT8 MonoTransparency
**          This value is used in gcvSURF_SOURCE_MATCH transparency mode.  The
**          value can be either 0 or 1 and is compared against each mono pixel
**          to determine transparency of the pixel.  If the values found are
**          equal, the pixel is transparent; otherwise it is opaque.
**
**      gceSURF_MONOPACK DataPack
**          Determines how many horizontal pixels are there per each 32-bit
**          chunk of monochrome bitmap.  For example, if set to gcvSURF_PACKED8,
**          each 32-bit chunk is 8-pixel wide, which also means that it defines
**          4 vertical lines of pixels.
**
**      gctBOOL CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**      gctUINT32 FgColor32
**      gctUINT32 BgColor32
**          The values are used to represent foreground and background colors
**          of the source.  The values should be specified in A8R8G8B8 format.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetMonochromeSource(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 MonoTransparency,
    IN gceSURF_MONOPACK DataPack,
    IN gctBOOL CoordRelative,
    IN gctUINT32 FgColor32,
    IN gctUINT32 BgColor32,
    IN gctBOOL ColorConvert,
    IN gceSURF_FORMAT DstFormat,
    IN gctBOOL Stream,
    IN gctUINT32 Transparency
    )
{
    gceSTATUS status;
    gctUINT32 datapack;

    gcmHEADER_ARG("Hardware=0x%x MonoTransparency=%d DataPack=%d "
                    "CoordRelative=%d FgColor32=%x BgColor32=%x",
                    Hardware, MonoTransparency, DataPack,
                    CoordRelative, FgColor32, BgColor32);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Convert the data packing. */
    gcmONERROR(gcoHARDWARE_TranslateMonoPack(
        DataPack, &datapack
        ));

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        gctUINT32 config;

        if (ColorConvert == gcvFALSE)
        {
            gcmONERROR(gcoHARDWARE_ColorConvertToARGB8(
                      DstFormat,
                      1,
                      &FgColor32,
                      &FgColor32
                      ));

            gcmONERROR(gcoHARDWARE_ColorConvertToARGB8(
                      DstFormat,
                      1,
                      &BgColor32,
                      &BgColor32
                      ));
        }

        /* LoadState(0x01200, 1), 0. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01200, 0
            ));

        /* Setup source configuration. transparency field is obsolete for PE 2.0. */
        config
            = (Stream ?
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
            : ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) (0xA & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) (0x0A & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (datapack) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (Transparency) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) ((gctUINT32) (CoordRelative) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)))
            | (MonoTransparency
                ? ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)))
                : ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))));

        /* LoadState(AQDE_SRC_CONFIG, 1), config. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0120C, config
            ));

        /* LoadState(AQDE_SRC_COLOR_BG, 1), BgColor. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01218, BgColor32
            ));

        /* LoadState(AQDE_SRC_COLOR_FG, 1), FgColor. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0121C, FgColor32
            ));
    }
    else
    {
        /* Monochrome operations are not currently supported. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetColorSource
**
**  Configure color source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface descriptor.
**
**      gctBOOL CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetColorSource(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CoordRelative,
    IN gctUINT32 Transparency,
    IN gce2D_YUV_COLOR_MODE Mode,
    IN gctBOOL DeGamma,
    IN gctBOOL Filter
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUV;
    gctUINT32 data[4], configEx = 0;
    gctUINT32 rotated = 0;
    gctBOOL cacheMode = gcvFALSE;
    gctUINT32 rgbaSwizzle, uvSwizzle;
    gctBOOL fullRot;

    gcmHEADER_ARG("Surface=0x%x CoordRelative=%d",
                    Surface, CoordRelative);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    fullRot = Filter ? Hardware->features[gcvFEATURE_2D_FILTERBLIT_FULLROTATION]
                     : Hardware->features[gcvFEATURE_2D_BITBLIT_FULLROTATION];

    /* Check the rotation capability. */
    if (!fullRot && gcmGET_PRE_ROTATION(Surface->rotation) != gcvSURF_0_DEGREE)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Convert the format. */
    gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
        Hardware, Surface->format, &format, &swizzle, &isYUV
        ));

    /* Determine color swizzle. */
    if (isYUV)
    {
        rgbaSwizzle = 0x0;
        uvSwizzle   = swizzle;
    }
    else
    {
        rgbaSwizzle = swizzle;
        uvSwizzle   = 0x0;
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
    gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical);

    /* 0x01200 */
    data[0]
        = Surface->node.physical;

    /* 0x01204 */
    data[1]
        = Surface->stride;

    /* 0x01208 */
    data[2]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedWidth) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (rotated) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

    /* 0x0120C; transparency field is obsolete for PE 2.0. */
    data[3]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (Transparency) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (rgbaSwizzle) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) ((gctUINT32) (CoordRelative) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));

    /* Set endian control. */
    if (Hardware->bigEndian &&
        Surface->tileStatusConfig == gcv2D_TSC_DISABLE &&
        Surface->format != gcvSURF_RG16 &&
        Surface->format != gcvSURF_NV16 && Surface->format != gcvSURF_NV61)
    {
        gctUINT32 bpp;

        if (Surface->format == gcvSURF_NV12 || Surface->format == gcvSURF_NV16
            || Surface->format == gcvSURF_NV21 || Surface->format == gcvSURF_NV61)
        {
            uvSwizzle = !uvSwizzle;
        }
        else
        {
            /* Compute bits per pixel. */
            gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                                 &bpp,
                                                 gcvNULL));

            if (bpp == 16)
            {
                data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
            }
            else if (bpp == 32)
            {
                data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
            }
        }
    }

    switch (Surface->tiling)
    {
    case gcvLINEAR:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = 0x00000000;

        break;

    case gcvTILED:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = 0x00000000;

        break;

    case gcvSUPERTILED:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        break;

    case gcvMULTI_TILED:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01304,
            Surface->node.physical2
            ));

        break;

    case gcvMULTI_SUPERTILED:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                 | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01304,
            Surface->node.physical2
            ));

        break;

    case gcvMINORTILED:
        data[3] = ((((gctUINT32) (data[3])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        configEx = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }


    gcmONERROR(_SetSourceTileStatus(
        Hardware,
        0,
        Surface,
        &cacheMode
        ));

    /* Set compression related config for source. */
    gcmONERROR(_SetSourceCompression(Hardware, Surface, 0, gcvFALSE, &configEx));

    if (Hardware->features[gcvFEATURE_2D_FC_SOURCE])
    {
        configEx = cacheMode ? ((((gctUINT32) (configEx)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                             : ((((gctUINT32) (configEx)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x01300,
        configEx
        ));

    if (Filter && Hardware->hw2D420L2Cache && Hardware->needOffL2Cache)
    {
        data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
    }

    if (DeGamma)
    {
        data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)));
    }

    /* Load source states. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware,
        0x01200, 4,
        data
        ));

    switch (Surface->format)
    {
        case gcvSURF_I420:
        case gcvSURF_YV12:
            /* Dump the memory. */
            gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical3);

            data[0] = Surface->node.physical3;
            data[1] = Surface->vStride;
            gcmONERROR(gcoHARDWARE_Load2DState(
                Hardware,
                0x0128C,
                2,
                data
                ));

        case gcvSURF_NV16:
        case gcvSURF_NV12:
        case gcvSURF_NV61:
        case gcvSURF_NV21:
            /* Dump the memory. */
            gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

            data[0] = Surface->node.physical2;
            data[1] = Surface->uStride;

            gcmONERROR(gcoHARDWARE_Load2DState(
                Hardware,
                0x01284,
                2,
                data
                ));

            break;

        default:
            break;
    }

    if (fullRot)
    {
        gctUINT32 srcRot = 0;
        gctUINT32 value;

        gcmONERROR(gcoHARDWARE_TranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &srcRot));

        if (!Filter)
        {
            /* Flush the 2D pipe before writing to the rotation register. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0380C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))));
        }

        /* Load source height. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012B8,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedHeight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            ));

        /* 0x012BC */
        if (Hardware->shadowRotAngleReg)
        {
            value = ((((gctUINT32) (Hardware->rotAngleRegShadow)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (srcRot) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

            /* Save the shadow value. */
            Hardware->rotAngleRegShadow = value;
        }
        else
        {
            /* Enable src mask. */
            value = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

            /* Set src rotation. */
            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (srcRot) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));
        }

        if (Hardware->features[gcvFEATURE_2D_POST_FLIP])
        {
            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (gcmGET_POST_ROTATION(Surface->rotation) >> 30) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));

            value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22)));
        }
        else if (gcmGET_POST_ROTATION(Surface->rotation))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    0x012BC,
                    value
                    ));
    }

    /* Load source UV swizzle and YUV mode state. */
    uvSwizzle = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (uvSwizzle) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

    switch (Mode)
    {
    case gcv2D_YUV_601:
        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
        break;

    case gcv2D_YUV_709:
        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
        break;

    case gcv2D_YUV_USER_DEFINED:
    case gcv2D_YUV_USER_DEFINED_CLAMP:
        if (Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
        {
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));

            if (Mode == gcv2D_YUV_USER_DEFINED_CLAMP)
            {
                uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
                uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
            }
        }
        else
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)));

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x012D8,
        uvSwizzle
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetMaskedSource
**
**  Configure masked color source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface descriptor.
**
**      gctBOOL CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**      gceSURF_MONOPACK MaskPack
**          Determines how many horizontal pixels are there per each 32-bit
**          chunk of monochrome mask.  For example, if set to gcvSURF_PACKED8,
**          each 32-bit chunk is 8-pixel wide, which also means that it defines
**          4 vertical lines of pixel mask.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetMaskedSource(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CoordRelative,
    IN gceSURF_MONOPACK MaskPack,
    IN gctUINT32 Transparency
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUV, maskpack;

    gcmHEADER_ARG("Hardware=0x%x Surface=0x%x CoordRelative=%d MaskPack=%d",
                    Hardware, Surface, CoordRelative, MaskPack);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Convert the format. */
    gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
        Hardware, Surface->format, &format, &swizzle, &isYUV
        ));

    /* Convert the data packing. */
    gcmONERROR(gcoHARDWARE_TranslateMonoPack(MaskPack, &maskpack));

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        gctUINT32 data[4];

        /* Determine color swizzle. */
        gctUINT32 rgbaSwizzle;

        if (isYUV)
        {
            rgbaSwizzle = 0x0;
        }
        else
        {
            rgbaSwizzle = swizzle;
        }

        if (!Hardware->features[gcvFEATURE_2D_BITBLIT_FULLROTATION] &&
            gcmGET_PRE_ROTATION(Surface->rotation) != gcvSURF_0_DEGREE)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical);

        /* 0x01200 */
        data[0]
            = Surface->node.physical;

        /* 0x01204 */
        data[1]
            = Surface->stride;

        /* 0x01208 */
        data[2]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedWidth) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)));

        /* AQDE_SRC_CONFIG_Address. transparency field is obsolete for PE 2.0. */
        data[3]
            = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (Transparency) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 3:0) - (0 ? 3:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:0) - (0 ? 3:0) + 1))))))) << (0 ? 3:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (rgbaSwizzle) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (maskpack) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) ((gctUINT32) (CoordRelative) & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));

        /* Set endian control */
        if (Hardware->bigEndian &&
            Surface->tileStatusConfig == gcv2D_TSC_DISABLE)
        {
            gctUINT32 bpp;

            /* Compute bits per pixel. */
            gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                                 &bpp,
                                                 gcvNULL));

            if (bpp == 16)
            {
                data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
            }
            else if (bpp == 32)
            {
                data[3] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
            }
        }

        /* Load source states. */
        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware,
            0x01200, 4,
            data
            ));

        if (Hardware->features[gcvFEATURE_2D_BITBLIT_FULLROTATION])
        {
            gctUINT32 srcRot = 0;
            gctUINT32 value;

            gcmONERROR(gcoHARDWARE_TranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &srcRot));

            /* Check errors. */
            gcmONERROR(status);

            /* Flush the 2D pipe before writing to the rotation register. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x0380C,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                ));

            /* Load source height. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x012B8,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedHeight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                ));

            /* 0x012BC */
            if (Hardware->shadowRotAngleReg)
            {
                value = ((((gctUINT32) (Hardware->rotAngleRegShadow)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (srcRot) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

                /* Save the shadow value. */
                Hardware->rotAngleRegShadow = value;
            }
            else
            {
                /* Enable src mask. */
                value = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

                /* Set src rotation. */
                value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (srcRot) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));
            }

            if (Hardware->features[gcvFEATURE_2D_POST_FLIP])
            {
                value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (gcmGET_POST_ROTATION(Surface->rotation) >> 30) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));

                value = ((((gctUINT32) (value)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22)));
            }
            else if (gcmGET_POST_ROTATION(Surface->rotation))
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware, 0x012BC, value
                ));
        }
    }
    else
    {
        /* Masked source is not currently supported by
           the software renderer. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetSource
**
**  Setup the source rectangle.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsRECT_PTR SrcRect
**          Pointer to a valid source rectangle.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetSource(
    IN gcoHARDWARE Hardware,
    IN gcsRECT_PTR SrcRect
    )
{
    gceSTATUS status;
    gctUINT32 data[2];

    gcmHEADER_ARG("Hardware=0x%x SrcRect=0x%x", Hardware, SrcRect);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(SrcRect != gcvNULL);

    /* 0x01210 */
    data[0]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcRect->left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcRect->top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

    /* 0x01214 */
    data[1]
        = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (SrcRect->right  - SrcRect->left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (SrcRect->bottom - SrcRect->top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)));

    /* LoadState(AQDE_SRC_ORIGIN, 2), origin, size. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware,
        0x01210, 2,
        data
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetOriginFraction
**
**  Setup the fraction of the source origin for filter blit.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT16 HorFraction
**      gctUINT16 VerFraction
**          Source origin fractions.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetOriginFraction(
    IN gcoHARDWARE Hardware,
    IN gctUINT16 HorFraction,
    IN gctUINT16 VerFraction
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x HorFraction=%d VerFraction=%d",
                    Hardware, HorFraction, VerFraction);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        /* LoadState(AQDE_SRC_ORIGIN_FRACTION, HorFraction, VerFraction. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x01278,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (HorFraction) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (VerFraction) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
            ));
    }
    else
    {
        /* Not supported by the software renderer. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_LoadPalette
**
**  Load 256-entry color table for INDEX8 source surfaces.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to the gcoHARDWARE object.
**
**      gctUINT FirstIndex
**          The index to start loading from (0..255).
**
**      gctUINT IndexCount
**          The number of indices to load (FirstIndex + IndexCount <= 256).
**
**      gctPOINTER ColorTable
**          Pointer to the color table to load. The value of the pointer should
**          be set to the first value to load no matter what the value of
**          FirstIndex is. The table must consist of 32-bit entries that contain
**          color values in either ARGB8 or the destination color format
**          (see ColorConvert). For PE 2.0, ColorTable values need to be in ARGB8.
**
**      gctBOOL ColorConvert
**          If set to gcvTRUE, the 32-bit values in the table are assumed to be
**          in ARGB8 format.
**          If set to gcvFALSE, the 32-bit values in the table are assumed to be
**          in destination format.
**          For old PE, the palette is assumed to be in destination format.
**          For new PE, the palette is assumed to be in ARGB8 format.
**          Thus, it is recommended to pass the palette accordingly, to avoid
**          internal color conversion.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_LoadPalette(
    IN gcoHARDWARE Hardware,
    IN gctUINT FirstIndex,
    IN gctUINT IndexCount,
    IN gctPOINTER ColorTable,
    IN gctBOOL ColorConvert,
    IN gceSURF_FORMAT DstFormat,
    IN OUT gctBOOL *Program,
    IN OUT gceSURF_FORMAT *ConvertFormat
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x FirstIndex=%d IndexCount=%d "
                    "ColorTable=0x%x ColorConvert=%d",
                    Hardware, FirstIndex, IndexCount,
                    ColorTable, ColorConvert);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->features[gcvFEATURE_2D_NO_COLORBRUSH_INDEX8])
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (Hardware->hw2DEngine && !Hardware->sw2DEngine)
    {
        gctUINT32 address;

        if (*ConvertFormat != DstFormat)
        {
            *Program = gcvTRUE;
        }

        if ((*Program) && ((Hardware->features[gcvFEATURE_2DPE20] && (ColorConvert == gcvFALSE)) ||
            (!Hardware->features[gcvFEATURE_2DPE20] && (ColorConvert == gcvTRUE))))
        {
            if (Hardware->features[gcvFEATURE_2DPE20])
            {
                /* Pattern table is in destination format,
                   convert it into ARGB8 format. */
                gcmONERROR(gcoHARDWARE_ColorConvertToARGB8(
                    DstFormat,
                    IndexCount,
                    ColorTable,
                    ColorTable
                    ));
            }
            else
            {
                /* Pattern table is in ARGB8 format,
                   convert it into destination format. */
                gcmONERROR(gcoHARDWARE_ColorConvertFromARGB8(
                    DstFormat,
                    IndexCount,
                    ColorTable,
                    ColorTable
                    ));
            }

            *Program = gcvFALSE;
            *ConvertFormat = DstFormat;
        }

        /* Determine first address. */
        if (Hardware->features[gcvFEATURE_2DPE20])
        {
            address = 0x0D00 + FirstIndex;
        }
        else
        {
            address = 0x0700 + FirstIndex;
        }

        /* Upload the palette table. */
        gcmONERROR(gcoHARDWARE_Load2DState(
            Hardware, address << 2, IndexCount, ColorTable
            ));
    }
    else
    {
        /* Not supported by the software renderer. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetSourceGlobalColor
**
**  Setup the source global color value in ARGB8 format.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 Color
**          Source color.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetSourceGlobalColor(
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
            0x012C8,
            Color
            ));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslatePixelColorMultiplyMode
**
**  Translate API pixel color multiply mode to its hardware value.
**
**  INPUT:
**
**      gce2D_PIXEL_COLOR_MULTIPLY_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslatePixelColorMultiplyMode(
    IN  gce2D_PIXEL_COLOR_MULTIPLY_MODE APIValue,
    OUT gctUINT32 * HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcv2D_COLOR_MULTIPLY_DISABLE:
        *HwValue = 0x0;
        break;

    case gcv2D_COLOR_MULTIPLY_ENABLE:
        *HwValue = 0x1;
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateGlobalColorMultiplyMode
**
**  Translate API global color multiply mode to its hardware value.
**
**  INPUT:
**
**      gce2D_GLOBAL_COLOR_MULTIPLY_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateGlobalColorMultiplyMode(
    IN  gce2D_GLOBAL_COLOR_MULTIPLY_MODE APIValue,
    OUT gctUINT32 * HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcv2D_GLOBAL_COLOR_MULTIPLY_DISABLE:
        *HwValue = 0x0;
        break;

    case gcv2D_GLOBAL_COLOR_MULTIPLY_ALPHA:
        *HwValue = 0x1;
        break;

    case gcv2D_GLOBAL_COLOR_MULTIPLY_COLOR:
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

/*******************************************************************************
**
**  gcoHARDWARE_SetMultiplyModes
**
**  Setup the source and target pixel multiply modes.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gce2D_PIXEL_COLOR_MULTIPLY_MODE SrcPremultiplySrcAlpha
**          Source color premultiply with Source Alpha.
**
**      gce2D_PIXEL_COLOR_MULTIPLY_MODE DstPremultiplyDstAlpha
**          Destination color premultiply with Destination Alpha.
**
**      gce2D_GLOBAL_COLOR_MULTIPLY_MODE SrcPremultiplyGlobalMode
**          Source color premultiply with Global color's Alpha.
**
**      gce2D_PIXEL_COLOR_MULTIPLY_MODE DstDemultiplyDstAlpha
**          Destination color demultiply with Destination Alpha.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetMultiplyModes(
    IN gcoHARDWARE Hardware,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE SrcPremultiplySrcAlpha,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE DstPremultiplyDstAlpha,
    IN gce2D_GLOBAL_COLOR_MULTIPLY_MODE SrcPremultiplyGlobalMode,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE DstDemultiplyDstAlpha,
    IN gctUINT32 SrcGlobalColor
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x SrcPremultiplySrcAlpha=%d DstPremultiplyDstAlpha=%d "
                    "SrcPremultiplyGlobalMode=%d DstDemultiplyDstAlpha=%d",
                    Hardware, SrcPremultiplySrcAlpha, DstPremultiplyDstAlpha,
                    SrcPremultiplyGlobalMode, DstDemultiplyDstAlpha);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->hw2DEngine && Hardware->features[gcvFEATURE_2DPE20] && !Hardware->sw2DEngine)
    {
        gctUINT32 srcPremultiplySrcAlpha;
        gctUINT32 dstPremultiplyDstAlpha;
        gctUINT32 srcPremultiplyGlobalMode;
        gctUINT32 dstDemultiplyDstAlpha;

        /* Convert the multiply modes. */
        gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
            SrcPremultiplySrcAlpha, &srcPremultiplySrcAlpha
            ));

        gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
            DstPremultiplyDstAlpha, &dstPremultiplyDstAlpha
            ));

        gcmONERROR(gcoHARDWARE_TranslateGlobalColorMultiplyMode(
            SrcPremultiplyGlobalMode, &srcPremultiplyGlobalMode
            ));

        gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
            DstDemultiplyDstAlpha, &dstDemultiplyDstAlpha
            ));

        /* LoadState pixel multiply modes. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012D0,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (srcPremultiplySrcAlpha) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (dstPremultiplyDstAlpha) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (srcPremultiplyGlobalMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (dstDemultiplyDstAlpha) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
            ));

        if (SrcPremultiplyGlobalMode != gcv2D_GLOBAL_COLOR_MULTIPLY_DISABLE)
        {
            /* Set source global color. */
            gcmONERROR(gcoHARDWARE_SetSourceGlobalColor(
                Hardware,
                SrcGlobalColor
                ));
        }
    }
    else
    {
        /* Not supported by the PE1.0 hardware.*/
        gcmONERROR(gcvSTATUS_SKIP);
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetTransparencyModesEx
**
**  Setup the source, target and pattern transparency modes.
**  It also enable or disable DFB color key mode.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gceSURF_TRANSPARENCY SrcTransparency
**          Source Transparency.
**
**      gceSURF_TRANSPARENCY DstTransparency
**          Destination Transparency.
**
**      gceSURF_TRANSPARENCY PatTransparency
**          Pattern Transparency.
**
**      gctBOOL EnableDFBColorKeyMode
**          Enable/disable DFB color key mode.
**          The transparent pixels will be bypassed when
**          enabling DFB color key mode. Otherwise those
**          pixels maybe processed by the following pipes.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetTransparencyModesEx(
    IN gcoHARDWARE Hardware,
    IN gce2D_TRANSPARENCY SrcTransparency,
    IN gce2D_TRANSPARENCY DstTransparency,
    IN gce2D_TRANSPARENCY PatTransparency,
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN gctBOOL EnableDFBColorKeyMode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x SrcTransparency=%d DstTransparency=%d PatTransparency=%d",
                    Hardware, SrcTransparency, DstTransparency, PatTransparency);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (Hardware->features[gcvFEATURE_2DPE20])
    {
        gctUINT32 srcTransparency;
        gctUINT32 dstTransparency;
        gctUINT32 patTransparency;
        gctUINT32 dfbColorKeyMode = 0;
        gctUINT32 transparencyMode;

        /* Compatible with PE1.0. */
        if (!Hardware->features[gcvFEATURE_ANDROID_ONLY]
            && (PatTransparency == gcv2D_OPAQUE)
            && ((((FgRop >> 4) & 0x0F) != (FgRop & 0x0F))
            || (((BgRop >> 4) & 0x0F) != (BgRop & 0x0F))))
        {
            PatTransparency = gcv2D_MASKED;
        }

        /* Convert the transparency modes. */
        gcmONERROR(gcoHARDWARE_TranslateSourceTransparency(
            SrcTransparency, &srcTransparency
            ));

        gcmONERROR(gcoHARDWARE_TranslateDestinationTransparency(
            DstTransparency, &dstTransparency
            ));

        gcmONERROR(gcoHARDWARE_TranslatePatternTransparency(
            PatTransparency, &patTransparency
            ));

        if (Hardware->features[gcvFEATURE_FULL_DIRECTFB])
        {
            gcmONERROR(gcoHARDWARE_TranslateDFBColorKeyMode(
                EnableDFBColorKeyMode, &dfbColorKeyMode
                ));
        }

        /* LoadState transparency modes.
           Enable Source or Destination read when
           respective Color key is turned on. */
        transparencyMode = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (srcTransparency) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                         | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (dstTransparency) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
                         | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (patTransparency) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
                         | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) ((srcTransparency == 0x2)) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
                         | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) ((gctUINT32) ((dstTransparency == 0x2)) & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)));

        if (Hardware->features[gcvFEATURE_FULL_DIRECTFB])
        {
            transparencyMode |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29))) | (((gctUINT32) ((gctUINT32) (dfbColorKeyMode) & ((gctUINT32) ((((1 ? 29:29) - (0 ? 29:29) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:29) - (0 ? 29:29) + 1))))))) << (0 ? 29:29)));
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012D4,
            transparencyMode
            ));
    }
    else
    {
        gctUINT32 transparency;

        /* Get PE 1.0 transparency from new transparency modes. */
        gcmONERROR(gcoHARDWARE_TranslateTransparencies(
            Hardware,
            SrcTransparency,
            DstTransparency,
            PatTransparency,
            &transparency
            ));

        /* LoadState(AQDE_SRC_CONFIG, 1), config. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x0120C,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (transparency) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
            ));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetSourceColorKeyRange
**
**  Setup the source color key value in source format.
**  Source pixels matching specified color range become transparent.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 ColorLow
**      gctUINT32 ColorHigh
**          Transparency low and high color.
**
**      gctBOOL ColorPack
**          If set to gcvTRUE, the 32-bit values in the table are assumed to be
**          in ARGB8 format.
**          If set to gcvFALSE, the 32-bit values in the table are assumed to be
**          in source format.
**          For old API calls, the color key is assumed to be in source format.
**          For new PE, the color is in ARGB8 format and needs to be packed.
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetSourceColorKeyRange(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorLow,
    IN gctUINT32 ColorHigh,
    IN gctBOOL ColorPack,
    IN gceSURF_FORMAT SrcFormat
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x ColorLow=%x ColorHigh=%x ColorPack=%d",
                    Hardware, ColorLow, ColorHigh, ColorPack);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (!Hardware->features[gcvFEATURE_2DPE20])
    {
        if (ColorPack && (SrcFormat != gcvSURF_INDEX8))
        {
            gcmONERROR(gcoHARDWARE_ColorPackFromARGB8(
                SrcFormat,
                ColorLow,
                &ColorLow
                ));
        }
    }
    else if (SrcFormat == gcvSURF_INDEX8)
    {
        ColorLow = (ColorLow & 0xFF) << 24;
        ColorHigh = (ColorHigh & 0xFF) << 24;
    }

    /* LoadState source color key. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x01218,
        ColorLow
        ));

    /* LoadState source color key. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x012DC,
        ColorHigh
        ));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetMultiSource
**
**  Configure 4x color source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface descriptor.
**
**      gctBOOL CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_SetMultiSource(
    IN gcoHARDWARE Hardware,
    IN gctUINT RegGroupIndex,
    IN gctUINT SrcIndex,
    IN gcs2D_State_PTR State
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUV;
    gctUINT32 data[4];
    gctUINT32 rgbaSwizzle, uvSwizzle; /* Determine color swizzle. */
    gcs2D_MULTI_SOURCE_PTR src = &State->multiSrc[SrcIndex];
    gcsSURF_INFO_PTR Surface = &src->srcSurface;
    gctUINT regOffset = RegGroupIndex << 2;
    gctBOOL cacheMode = gcvFALSE;

    gcmHEADER_ARG("Hardware=0x%x RegGroupIndex=%d SrcIndex=%d State=0x%x",
                    Hardware, RegGroupIndex, SrcIndex, State);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Convert the format. */
    gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
        Hardware, Surface->format, &format, &swizzle, &isYUV
        ));

    if (isYUV)
    {
        rgbaSwizzle = 0x0;
        uvSwizzle   = swizzle;
    }
    else
    {
        rgbaSwizzle = swizzle;
        uvSwizzle   = 0x0;
    }

#if gcdDUMP_2D
    /* Dump the memory. */
    {
        gcsRECT rect = src->srcRect;
        gctUINT32 bpp;

        /* Compute bits per pixel. */
        gcoHARDWARE_ConvertFormat(Surface->format,
                                             &bpp,
                                             gcvNULL);

        gcsRECT_Rotate(&rect,
                       Surface->rotation,
                       gcvSURF_0_DEGREE,
                       Surface->alignedWidth,
                       Surface->alignedHeight);

        gcmDUMP_2D_SURFACE(gcvTRUE,
            Surface->node.physical + rect.top * Surface->stride + ((rect.left * bpp) >> 3));
    }
#endif

    /* Load source address. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12800 + regOffset,
        Surface->node.physical
        ));

    /* Load source stride. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12810 + regOffset,
        Surface->stride
        ));

    /* Load source width; rotation is not supported. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12820 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedWidth) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        ));

    /* Load source config; transparency field is obsolete for PE 2.0. */
    data[0] =((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (rgbaSwizzle) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
           | (src->srcRelativeCoord ?
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)))
             :((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))));

    /* Set endian control */
    if (Hardware->bigEndian &&
        Surface->tileStatusConfig == gcv2D_TSC_DISABLE)
    {
        gctUINT32 bpp;

        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                             &bpp,
                                             gcvNULL));

        if (bpp == 16)
        {
            data[0] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
        }
        else if (bpp == 32)
        {
            data[0] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
        }
    }

    switch (Surface->tiling)
    {
    case gcvLINEAR:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = 0x00000000;

        break;

    case gcvTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = 0x00000000;

        break;

    case gcvSUPERTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        break;

    case gcvMULTI_TILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12970 + regOffset,
            Surface->node.physical2
            ));

        break;

    case gcvMULTI_SUPERTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12970 + regOffset,
            Surface->node.physical2
            ));

        break;

    case gcvMINORTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);

    }

    gcmONERROR(_SetSourceTileStatus(
        Hardware,
        RegGroupIndex,
        Surface,
        &cacheMode
        ));

    /* Set compression related config for source. */
    gcmONERROR(_SetSourceCompression(Hardware, Surface, RegGroupIndex, gcvTRUE, &data[1]));

    if (src->srcDeGamma)
    {
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)));
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12830 + regOffset,
        data[0]
        ));

    if (Hardware->features[gcvFEATURE_2D_COMPRESSION])
    {
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12CC0 + regOffset,
            cacheMode ? ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                      : ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
            ));
     }
     else
     {
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12960 + regOffset,
            cacheMode ? ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                      : ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
            ));
    }

    gcmONERROR(gcoHARDWARE_TranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &data[0]));

    /* Enable src rotation. */
    data[1] = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

    /* Set src rotation. */
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

    if (Hardware->features[gcvFEATURE_2D_POST_FLIP])
    {
        data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (gcmGET_POST_ROTATION(Surface->rotation) >> 30) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));

        data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22)));
    }
    else if (gcmGET_POST_ROTATION(Surface->rotation))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    data[0] = 0x0;
    if (src->horMirror)
    {
        data[0] |= 0x1;
    }

    if (src->verMirror)
    {
        data[0] |= 0x2;
    }

    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)));
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));

    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));

    gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x128F0 + regOffset,
                data[1]
                ));

    /* Load source height. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x128E0 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedHeight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        ));

    if (isYUV)
    {
        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        /* Load source U plane address. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x128A0 + regOffset,
            Surface->node.physical2
            ));

        /* Load source U plane stride. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x128B0 + regOffset,
            Surface->uStride
            ));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical3);

        /* Load source V plane address. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x128C0 + regOffset,
            Surface->node.physical3
            ));

        /* Load source V plane stride. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x128D0 + regOffset,
            Surface->vStride
            ));

        uvSwizzle = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (uvSwizzle) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

        switch (src->srcYUVMode)
        {
        case gcv2D_YUV_601:
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcv2D_YUV_709:
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcv2D_YUV_USER_DEFINED:
        case gcv2D_YUV_USER_DEFINED_CLAMP:
            if (Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
            {
                uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));

                if (src->srcYUVMode == gcv2D_YUV_USER_DEFINED_CLAMP)
                {
                    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
                    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }
            }
            else
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        if (src->enableAlpha)
        {
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }
        else
        {
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }

        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)));

        /* Load source UV swizzle and YUV mode state. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12940 + regOffset,
            uvSwizzle
            ));
    }

    /* load src origin. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12840 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (src->srcRect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (src->srcRect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        ));

    /* load src rect size. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12850 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (src->srcRect.right  - src->srcRect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (src->srcRect.bottom - src->srcRect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        ));

    /* Load source color key. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12860 + regOffset,
        Surface->format == gcvSURF_INDEX8 ?
            ((src->srcColorKeyLow) & 0xFF) << 24
            : src->srcColorKeyLow
        ));

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12950 + regOffset,
        Surface->format == gcvSURF_INDEX8 ?
            ((src->srcColorKeyHigh) & 0xFF) << 24
            : src->srcColorKeyHigh
        ));


    /*******************************************************************
    ** Setup ROP.
    */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12870 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (src->bgRop) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (src->fgRop) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
        ));

    /* Convert the transparency modes. */
    /* Compatible with PE1.0. */
    if (!Hardware->features[gcvFEATURE_ANDROID_ONLY] &&
        (src->patTransparency == gcv2D_OPAQUE)
                && ((((src->fgRop >> 4) & 0x0F) != (src->fgRop & 0x0F))
                || (((src->bgRop >> 4) & 0x0F) != (src->bgRop & 0x0F))))
    {
        data[2] = gcv2D_MASKED;
    }
    else
    {
        data[2] = src->patTransparency;
    }

    gcmONERROR(gcoHARDWARE_TranslateSourceTransparency(
        src->srcTransparency, &data[0]
        ));

    gcmONERROR(gcoHARDWARE_TranslateDestinationTransparency(
        src->dstTransparency, &data[1]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePatternTransparency(
        data[2], &data[2]
        ));

    /* LoadState transparency modes.
       Enable Source or Destination read when
       respective Color key is turned on. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12930 + regOffset,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (data[1]) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (data[2]) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) ((data[0] == 0x2)) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) ((gctUINT32) ((data[1] == 0x2)) & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)))
        ));

    if (src->enableAlpha)
    {
        gctUINT32 srcAlphaMode = 0;
        gctUINT32 srcGlobalAlphaMode = 0;
        gctUINT32 srcFactorMode = 0;
        gctUINT32 srcFactorExpansion = 0;
        gctUINT32 dstAlphaMode = 0;
        gctUINT32 dstGlobalAlphaMode = 0;
        gctUINT32 dstFactorMode = 0;
        gctUINT32 dstFactorExpansion = 0;

        /* Translate inputs. */
        gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
            src->srcAlphaMode, &srcAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
            src->dstAlphaMode, &dstAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
            src->srcGlobalAlphaMode, &srcGlobalAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
            src->dstGlobalAlphaMode, &dstGlobalAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
            Hardware, gcvTRUE, src->srcFactorMode, &srcFactorMode, &srcFactorExpansion
            ));

        gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
            Hardware, gcvFALSE, src->dstFactorMode, &dstFactorMode, &dstFactorExpansion
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12880 + regOffset,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12890 + regOffset,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (srcAlphaMode) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (dstAlphaMode) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (srcGlobalAlphaMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (dstGlobalAlphaMode) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (srcFactorMode) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (dstFactorMode) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)))
            ));
    }
    else
    {
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12880 + regOffset,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            ));
    }

    /* Load global src color value. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12900 + regOffset,
        src->srcGlobalColor
        ));

    /* Load global dst color value. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12910 + regOffset,
        src->dstGlobalColor
        ));

    /* Convert the multiply modes. */
    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->srcPremultiplyMode, &data[0]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->dstPremultiplyMode, &data[1]
        ));

    gcmONERROR(gcoHARDWARE_TranslateGlobalColorMultiplyMode(
        src->srcPremultiplyGlobalMode, &data[2]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->dstDemultiplyMode, &data[3]
        ));

    /* LoadState pixel multiply modes. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x12920 + regOffset,
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (data[1]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (data[2]) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (data[3]) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetMultiSourceEx
**
**  Configure 8x color source.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer to the source surface descriptor.
**
**      gctBOOL CoordRelative
**          If gcvFALSE, the source origin represents absolute pixel coordinate
**          within the source surface.  If gcvTRUE, the source origin represents
**          the offset from the destination origin.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_SetMultiSourceEx(
    IN gcoHARDWARE Hardware,
    IN gctUINT RegGroupIndex,
    IN gctUINT SrcIndex,
    IN gcs2D_State_PTR State,
    IN gctBOOL MultiDstRect
    )
{
    gceSTATUS status;
    gctUINT32 format, swizzle, isYUV;
    gctUINT32 data[4];
    gctUINT32 rgbaSwizzle, uvSwizzle; /* Determine color swizzle. */
    gcs2D_MULTI_SOURCE_PTR src = &State->multiSrc[SrcIndex];
    gcsSURF_INFO_PTR Surface = &src->srcSurface;
    gctUINT regOffset = RegGroupIndex << 2;
    gctBOOL cacheMode = gcvFALSE;
    gctBOOL setSrcRect = gcvTRUE;

    gcmHEADER_ARG("Hardware=0x%x RegGroupIndex=%d SrcIndex=%d State=0x%x",
                    Hardware, RegGroupIndex, SrcIndex, State);

    /* Convert the format. */
    gcmONERROR(gcoHARDWARE_TranslateSourceFormat(
        Hardware, Surface->format, &format, &swizzle, &isYUV
        ));

    if (isYUV)
    {
        rgbaSwizzle = 0x0;
        uvSwizzle   = swizzle;
    }
    else
    {
        rgbaSwizzle = swizzle;
        uvSwizzle   = 0x0;
    }

#if gcdDUMP_2D
    /* Dump the memory. */
    {
        gcsRECT rect = src->srcRect;
        gctUINT32 bpp;

        /* Compute bits per pixel. */
        gcoHARDWARE_ConvertFormat(Surface->format,
                                             &bpp,
                                             gcvNULL);

        gcsRECT_Rotate(&rect,
                       Surface->rotation,
                       gcvSURF_0_DEGREE,
                       Surface->alignedWidth,
                       Surface->alignedHeight);

        gcmDUMP_2D_SURFACE(gcvTRUE,
            Surface->node.physical + rect.top * Surface->stride + ((rect.left * bpp) >> 3));
    }
#endif

    /* Load source address. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12A00 + regOffset : 0x01200,
        Surface->node.physical
        ));

    /* Load source stride. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12A20 + regOffset : 0x01204,
        Surface->stride
        ));

    /* Load source width; rotation is not supported. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12A40 + regOffset : 0x01208,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedWidth) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        ));

    /* Load source config; transparency field is obsolete for PE 2.0. */
    data[0] =((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24))) | (((gctUINT32) ((gctUINT32) (format) & ((gctUINT32) ((((1 ? 28:24) - (0 ? 28:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:24) - (0 ? 28:24) + 1))))))) << (0 ? 28:24)))
           | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (rgbaSwizzle) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
           | (src->srcRelativeCoord ?
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)))
             :((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))));

    /* Set endian control */
    if (Hardware->bigEndian &&
        Surface->tileStatusConfig == gcv2D_TSC_DISABLE)
    {
        gctUINT32 bpp;

        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(Surface->format,
                                             &bpp,
                                             gcvNULL));

        if (bpp == 16)
        {
            data[0] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
        }
        else if (bpp == 32)
        {
            data[0] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 31:30) - (0 ? 31:30) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:30) - (0 ? 31:30) + 1))))))) << (0 ? 31:30)));
        }
    }

    switch (Surface->tiling)
    {
    case gcvLINEAR:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1]= 0x00000000;

        break;

    case gcvTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = 0x00000000;

        break;

    case gcvSUPERTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        break;

    case gcvMULTI_TILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12CE0 + regOffset : 0x01304,
            Surface->node.physical2
            ));

        break;

    case gcvMULTI_SUPERTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)));

        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12CE0 + regOffset : 0x01304,
            Surface->node.physical2
            ));

        break;

    case gcvMINORTILED:
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

        data[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    gcmONERROR(_SetSourceTileStatus(
        Hardware,
        RegGroupIndex,
        Surface,
        &cacheMode
        ));

    /* Set compression related config for source. */
    gcmONERROR(_SetSourceCompression(Hardware, Surface, RegGroupIndex, gcvTRUE, &data[1]));

    if (src->srcDeGamma)
    {
        data[0] = ((((gctUINT32) (data[0])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23))) | (((gctUINT32) (0x1  & ((gctUINT32) ((((1 ? 23:23) - (0 ? 23:23) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:23) - (0 ? 23:23) + 1))))))) << (0 ? 23:23)));
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12A60 + regOffset : 0x0120C,
        data[0]
        ));

    if (Hardware->features[gcvFEATURE_2D_FC_SOURCE])
    {
        data[1] = cacheMode ? ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)))
                            : ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
    }

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12CC0 + regOffset : 0x01300,
        data[1]
        ));

    gcmONERROR(gcoHARDWARE_TranslateSourceRotation(gcmGET_PRE_ROTATION(Surface->rotation), &data[0]));

    /* Enable src rotation. */
    data[1] = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));

    /* Set src rotation. */
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

    if (Hardware->features[gcvFEATURE_2D_POST_FLIP])
    {
        data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) ((gctUINT32) (gcmGET_POST_ROTATION(Surface->rotation) >> 30) & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)));

        data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 22:22) - (0 ? 22:22) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 22:22) - (0 ? 22:22) + 1))))))) << (0 ? 22:22)));
    }
    else if (gcmGET_POST_ROTATION(Surface->rotation))
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    data[0] = 0x0;
    if (src->horMirror)
    {
        data[0] |= 0x1;
    }

    if (src->verMirror)
    {
        data[0] |= 0x2;
    }

    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)));
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));

    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)));
    data[1] = ((((gctUINT32) (data[1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 19:19) - (0 ? 19:19) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:19) - (0 ? 19:19) + 1))))))) << (0 ? 19:19)));

    gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                RegGroupIndex ? 0x12BE0 + regOffset : 0x012BC,
                data[1]
                ));

    /* Load source height. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12BC0 + regOffset : 0x012B8,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Surface->alignedHeight) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        ));

    if (isYUV)
    {
        /* Dump the memory. */
        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical2);

        /* Load source U plane address. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B40 + regOffset : 0x01284,
            Surface->node.physical2
            ));

        /* Load source U plane stride. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B60 + regOffset : 0x01288,
            Surface->uStride
            ));

        gcmDUMP_2D_SURFACE(gcvTRUE, Surface->node.physical3);

        /* Load source V plane address. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B80 + regOffset : 0x0128C,
            Surface->node.physical3
            ));

        /* Load source V plane stride. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12BA0 + regOffset : 0x01290,
            Surface->vStride
            ));

        uvSwizzle = (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (uvSwizzle) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) &((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))));

        switch (src->srcYUVMode)
        {
        case gcv2D_YUV_601:
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcv2D_YUV_709:
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));
            break;

        case gcv2D_YUV_USER_DEFINED:
        case gcv2D_YUV_USER_DEFINED_CLAMP:
            if (Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
            {
                uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) (0x2 & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)));

                if (src->srcYUVMode == gcv2D_YUV_USER_DEFINED_CLAMP)
                {
                    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 12:12) - (0 ? 12:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:12) - (0 ? 12:12) + 1))))))) << (0 ? 12:12)));
                    uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 15:15) - (0 ? 15:15) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:15) - (0 ? 15:15) + 1))))))) << (0 ? 15:15)));
                }
            }
            else
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }
            break;

        default:
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));

        if (src->enableAlpha)
        {
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }
        else
        {
            uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)));
        }

        uvSwizzle = ((((gctUINT32) (uvSwizzle)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11)));

        /* Load source UV swizzle and YUV mode state. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12C80 + regOffset : 0x012D8,
            uvSwizzle
            ));
    }

    /* Load source color key. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12AC0 + regOffset : 0x01218,
        Surface->format == gcvSURF_INDEX8 ?
            ((src->srcColorKeyLow) & 0xFF) << 24
            : src->srcColorKeyLow
        ));

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12CA0 + regOffset : 0x012DC,
        Surface->format == gcvSURF_INDEX8 ?
            ((src->srcColorKeyHigh) & 0xFF) << 24
            : src->srcColorKeyHigh
        ));


    /*******************************************************************
    ** Setup ROP.
    */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12AE0 + regOffset : 0x0125C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20))) | (((gctUINT32) (0x3 & ((gctUINT32) ((((1 ? 21:20) - (0 ? 21:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:20) - (0 ? 21:20) + 1))))))) << (0 ? 21:20)))
          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8))) | (((gctUINT32) ((gctUINT32) (src->bgRop) & ((gctUINT32) ((((1 ? 15:8) - (0 ? 15:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:8) - (0 ? 15:8) + 1))))))) << (0 ? 15:8)))
          | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0))) | (((gctUINT32) ((gctUINT32) (src->fgRop) & ((gctUINT32) ((((1 ? 7:0) - (0 ? 7:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:0) - (0 ? 7:0) + 1))))))) << (0 ? 7:0)))
        ));

    /* Convert the transparency modes. */
    /* Compatible with PE1.0. */
    if (!Hardware->features[gcvFEATURE_ANDROID_ONLY] &&
        (src->patTransparency == gcv2D_OPAQUE)
                && ((((src->fgRop >> 4) & 0x0F) != (src->fgRop & 0x0F))
                || (((src->bgRop >> 4) & 0x0F) != (src->bgRop & 0x0F))))
    {
        data[2] = gcv2D_MASKED;
    }
    else
    {
        data[2] = src->patTransparency;
    }

    gcmONERROR(gcoHARDWARE_TranslateSourceTransparency(
        src->srcTransparency, &data[0]
        ));

    gcmONERROR(gcoHARDWARE_TranslateDestinationTransparency(
        src->dstTransparency, &data[1]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePatternTransparency(
        data[2], &data[2]
        ));

    /* LoadState transparency modes.
       Enable Source or Destination read when
       respective Color key is turned on. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12C60 + regOffset : 0x012D4,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (data[1]) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4))) | (((gctUINT32) ((gctUINT32) (data[2]) & ((gctUINT32) ((((1 ? 5:4) - (0 ? 5:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:4) - (0 ? 5:4) + 1))))))) << (0 ? 5:4)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16))) | (((gctUINT32) ((gctUINT32) ((data[0] == 0x2)) & ((gctUINT32) ((((1 ? 17:16) - (0 ? 17:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 17:16) - (0 ? 17:16) + 1))))))) << (0 ? 17:16)))
                     | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24))) | (((gctUINT32) ((gctUINT32) ((data[1] == 0x2)) & ((gctUINT32) ((((1 ? 25:24) - (0 ? 25:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:24) - (0 ? 25:24) + 1))))))) << (0 ? 25:24)))
        ));

    if (src->enableAlpha)
    {
        gctUINT32 srcAlphaMode = 0;
        gctUINT32 srcGlobalAlphaMode = 0;
        gctUINT32 srcFactorMode = 0;
        gctUINT32 srcFactorExpansion = 0;
        gctUINT32 dstAlphaMode = 0;
        gctUINT32 dstGlobalAlphaMode = 0;
        gctUINT32 dstFactorMode = 0;
        gctUINT32 dstFactorExpansion = 0;

        /* Translate inputs. */
        gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
            src->srcAlphaMode, &srcAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
            src->dstAlphaMode, &dstAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
            src->srcGlobalAlphaMode, &srcGlobalAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
            src->dstGlobalAlphaMode, &dstGlobalAlphaMode
            ));

        gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
            Hardware, gcvTRUE, src->srcFactorMode, &srcFactorMode, &srcFactorExpansion
            ));

        gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
            Hardware, gcvFALSE, src->dstFactorMode, &dstFactorMode, &dstFactorExpansion
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B00 + regOffset : 0x0127C,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B20 + regOffset : 0x01280,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (srcAlphaMode) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (dstAlphaMode) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (srcGlobalAlphaMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (dstGlobalAlphaMode) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (srcFactorMode) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (dstFactorMode) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)))
            ));
    }
    else
    {
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12B00 + regOffset : 0x0127C,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
            ));
    }

    /* Load global src color value. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12C00 + regOffset : 0x012C8,
        src->srcGlobalColor
        ));

    /* Load global dst color value. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12C20 + regOffset : 0x012CC,
        src->dstGlobalColor
        ));

    /* Convert the multiply modes. */
    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->srcPremultiplyMode, &data[0]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->dstPremultiplyMode, &data[1]
        ));

    gcmONERROR(gcoHARDWARE_TranslateGlobalColorMultiplyMode(
        src->srcPremultiplyGlobalMode, &data[2]
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMultiplyMode(
        src->dstDemultiplyMode, &data[3]
        ));

    /* LoadState pixel multiply modes. */
    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        RegGroupIndex ? 0x12C40 + regOffset : 0x012D0,
          ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (data[0]) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (data[1]) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (data[2]) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (data[3]) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
        ));

    if (Hardware->features[gcvFEATURE_2D_MULTI_SOURCE_BLT_EX2])
    {
        gcsRECT drect, clip;
        gctUINT32 config;
        gctINT w, h;

        /* Clip Rect coordinates in positive range. */
        if (MultiDstRect)
        {
            gctINT32 sw = src->srcRect.right  - src->srcRect.left;
            gctINT32 dw = src->dstRect.right - src->dstRect.left;
            gctINT32 sh = src->srcRect.bottom - src->srcRect.top;
            gctINT32 dh = src->dstRect.bottom - src->dstRect.top;

            clip.left   = gcmMAX(src->clipRect.left, 0);
            clip.top    = gcmMAX(src->clipRect.top, 0);
            clip.right  = gcmMAX(src->clipRect.right, 0);
            clip.bottom = gcmMAX(src->clipRect.bottom, 0);

            if ((sw != dw) || (sh != dh))
            {
                gctBOOL initErr = gcvFALSE;
                gcsRECT srect;
                gctINT32 hfact = gcoHARDWARE_GetStretchFactor(src->enableGDIStretch, sw, dw);
                gctINT32 vfact = gcoHARDWARE_GetStretchFactor(src->enableGDIStretch, sh, dh);
                gctINT32 hInit = 0;
                gctINT32 vInit = 0;

                config = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    RegGroupIndex ? 0x12D80 + regOffset : 0x01220,
                    hfact
                    ));

                gcmONERROR(gcoHARDWARE_Load2DState32(
                    Hardware,
                    RegGroupIndex ? 0x12DA0 + regOffset : 0x01224,
                    vfact
                    ));

                if (src->enableGDIStretch)
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));
                }
                else
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

                    --dw; --dh; --sw; --sh;
                }

                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));

                if (src->dstRect.left < src->clipRect.left)
                {
                    gctINT32 delta = src->clipRect.left - src->dstRect.left;

                    initErr = gcvTRUE;

                    drect.left = src->clipRect.left;

                    if (src->enableGDIStretch)
                    {
                        srect.left = (delta * hfact) >> 16;
                        hInit += srect.left * dw - delta * sw;
                        srect.left +=  src->srcRect.left;
                    }
                    else
                    {
                        srect.right = delta * hfact;

                        if (srect.right & 0x8000)
                        {
                            hInit += dw << 1;
                            srect.left = 1;
                        }
                        else
                        {
                            srect.left = 0;
                        }

                        srect.right >>= 16;
                        hInit += (srect.right * dw - delta * sw) << 1;
                        srect.left +=  src->srcRect.left + srect.right;
                    }
                }
                else
                {
                    drect.left = src->dstRect.left;
                    srect.left = src->srcRect.left;
                }

                if (src->dstRect.right > src->clipRect.right)
                {
                    initErr = gcvTRUE;
                    drect.right = src->clipRect.right;
                    srect.right = src->srcRect.right
                        - (((src->dstRect.right - src->clipRect.right) * hfact) >> 16);
                }
                else
                {
                    drect.right = src->dstRect.right;
                    srect.right = src->srcRect.right;
                }

                if (src->dstRect.top < src->clipRect.top)
                {
                    gctINT32 delta = src->clipRect.top - src->dstRect.top;

                    initErr = gcvTRUE;

                    drect.top = src->clipRect.top;

                    if (src->enableGDIStretch)
                    {
                        srect.top = (delta * vfact) >> 16;
                        vInit += srect.top * dh - delta * sh;
                        srect.top += src->srcRect.top;
                    }
                    else
                    {
                        srect.bottom = delta * vfact;

                        if (srect.bottom & 0x8000)
                        {
                            vInit += dh << 1;
                            srect.top = 1;
                        }
                        else
                        {
                            srect.top = 0;
                        }

                        srect.bottom >>= 16;
                        vInit += (srect.bottom * dh - delta * sh) << 1;
                        srect.top += src->srcRect.top + srect.bottom;
                    }
                }
                else
                {
                    drect.top = src->dstRect.top;
                    srect.top = src->srcRect.top;
                }

                if (src->dstRect.bottom > src->clipRect.bottom)
                {
                    initErr = gcvTRUE;
                    drect.bottom = src->clipRect.bottom;
                    srect.bottom = src->srcRect.bottom
                        - (((src->dstRect.bottom - src->clipRect.bottom) * vfact) >> 16);
                }
                else
                {
                    drect.bottom = src->dstRect.bottom;
                    srect.bottom = src->srcRect.bottom;
                }

                if (initErr)
                {
                    gctINT32 hSou, hNor, vSou, vNor;

                    if (src->enableGDIStretch)
                    {
                        hNor  =  (hfact >> 16) * dw - sw;
                        hSou  =  hNor + dw;
                        hInit += hSou;

                        vNor  =  (vfact >> 16) * dh - sh;
                        vSou  =  vNor + dh;
                        vInit += vSou;
                    }
                    else
                    {
                        hNor  =  ((hfact >> 16) * dw - sw) << 1;
                        hSou  =  hNor + (dw << 1);
                        hInit += hNor + dw;

                        vNor  =  ((vfact >> 16) * dh - sh) << 1;
                        vSou  =  vNor + (dh << 1);
                        vInit += vNor + dh;
                    }

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12DC0 + regOffset,
                        vInit
                        ));

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12DE0 + regOffset,
                        vSou
                        ));

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12E00 + regOffset,
                        vNor
                        ));

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12E20 + regOffset,
                        hInit
                        ));

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12E40 + regOffset,
                        hSou
                        ));

                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        0x12E60 + regOffset,
                        hNor
                        ));

                    /* load src origin. */
                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        RegGroupIndex ? 0x12A80 + regOffset : 0x01210,
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (srect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (srect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
                        ));

                    /* load src rect size. */
                    gcmONERROR(gcoHARDWARE_Load2DState32(
                        Hardware,
                        RegGroupIndex ? 0x12AA0 + regOffset : 0x01214,
                        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (srect.right - srect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
                        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (srect.bottom - srect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
                        ));

                    setSrcRect = gcvFALSE;

                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));
                }
                else
                {
                    config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));
                }

                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));

                clip = drect;
            }
            else
            {
                drect = src->dstRect;
                config = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));

                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));
                config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));
            }

            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
        }
        else
        {
            clip.left   = gcmMAX(State->dstClipRect.left, 0);
            clip.top    = gcmMAX(State->dstClipRect.top, 0);
            clip.right  = gcmMAX(State->dstClipRect.right, 0);
            clip.bottom = gcmMAX(State->dstClipRect.bottom, 0);

            drect = src->srcRect;

            config = ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 2:0) - (0 ? 2:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:0) - (0 ? 2:0) + 1))))))) << (0 ? 2:0)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 3:3) - (0 ? 3:3) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 3:3) - (0 ? 3:3) + 1))))))) << (0 ? 3:3)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 6:6) - (0 ? 6:6) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:6) - (0 ? 6:6) + 1))))))) << (0 ? 6:6)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 7:7) - (0 ? 7:7) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 7:7) - (0 ? 7:7) + 1))))))) << (0 ? 7:7)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));
            config = ((((gctUINT32) (config)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)));
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x12E80 + regOffset,
            config
            ));

        /* Set clipping rect. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12D00 + regOffset : 0x01260,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (clip.left) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (clip.top) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)))
            ));

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12D20 + regOffset : 0x01264,
              ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (clip.right) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (clip.bottom) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)))
            ));

        if (!State->unifiedDstRect)
        {
            if ((gcmGET_PRE_ROTATION(State->dstSurface.rotation) == gcvSURF_90_DEGREE)
                || (gcmGET_PRE_ROTATION(State->dstSurface.rotation) == gcvSURF_270_DEGREE))
            {
                w = State->dstSurface.alignedHeight;
                h = State->dstSurface.alignedWidth;
            }
            else
            {
                w = State->dstSurface.alignedWidth;
                h = State->dstSurface.alignedHeight;
            }

            if ((drect.right < drect.left)
                || (drect.bottom < drect.top)
                || (drect.right > w)
                || (drect.bottom > h))
            {
                gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            /* Set dest rect. */
            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x12D40 + regOffset,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (drect.left) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (drect.top) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)))
                ));

            gcmONERROR(gcoHARDWARE_Load2DState32(
                Hardware,
                0x12D60 + regOffset,
                ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0))) | (((gctUINT32) ((gctUINT32) (drect.right) & ((gctUINT32) ((((1 ? 14:0) - (0 ? 14:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:0) - (0 ? 14:0) + 1))))))) << (0 ? 14:0)))
                | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16))) | (((gctUINT32) ((gctUINT32) (drect.bottom) & ((gctUINT32) ((((1 ? 30:16) - (0 ? 30:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:16) - (0 ? 30:16) + 1))))))) << (0 ? 30:16)))
                ));
        }
    }

    if (setSrcRect)
    {
        /* load src origin. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12A80 + regOffset : 0x01210,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (src->srcRect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (src->srcRect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
            ));

        /* load src rect size. */
        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            RegGroupIndex ? 0x12AA0 + regOffset : 0x01214,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (src->srcRect.right  - src->srcRect.left) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
            | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (src->srcRect.bottom - src->srcRect.top) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
            ));
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateXRGBFormat
**
**  Translate XRGB format according to XRGB hardware setting.
**
**  INPUT:
**
**      gceSURF_FORMAT InputFormat
**          Original format.
**
**  OUTPUT:
**
**      gceSURF_FORMAT* OutputFormat
**          Real format.
*/
gceSTATUS gcoHARDWARE_TranslateXRGBFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT InputFormat,
    OUT gceSURF_FORMAT* OutputFormat
    )
{
    gceSURF_FORMAT format;

    gcmHEADER_ARG("InputFormat=%d OutputFormat=0x%x", InputFormat, OutputFormat);

    format = InputFormat;

    if (!Hardware->enableXRGB)
    {
        switch (format)
        {
            case gcvSURF_X8R8G8B8:
                format = gcvSURF_A8R8G8B8;
                break;

            case gcvSURF_X8B8G8R8:
                format = gcvSURF_A8B8G8R8;
                break;

            case gcvSURF_R8G8B8X8:
                format = gcvSURF_R8G8B8A8;
                break;

            case gcvSURF_B8G8R8X8:
                format = gcvSURF_B8G8R8A8;
                break;

            case gcvSURF_X4R4G4B4:
                format = gcvSURF_A4R4G4B4;
                break;

            case gcvSURF_X4B4G4R4:
                format = gcvSURF_A4B4G4R4;
                break;

            case gcvSURF_R4G4B4X4:
                format = gcvSURF_R4G4B4A4;
                break;

            case gcvSURF_B4G4R4X4:
                format = gcvSURF_B4G4R4A4;
                break;

            case gcvSURF_X1R5G5B5:
                format = gcvSURF_A1R5G5B5;
                break;

            case gcvSURF_X1B5G5R5:
                format = gcvSURF_A1B5G5R5;
                break;

            case gcvSURF_R5G5B5X1:
                format = gcvSURF_R5G5B5A1;
                break;

            case gcvSURF_B5G5R5X1:
                format = gcvSURF_B5G5R5A1;
                break;

            default:
                break;
        }
    }

    *OutputFormat = format;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateTransparency
**
**  Translate API transparency mode to its hardware value.
**  Obsolete function for PE 2.0
**
**  INPUT:
**
**      gceSURF_TRANSPARENCY APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateTransparency(
    IN gceSURF_TRANSPARENCY APIValue,
    OUT gctUINT32* HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcvSURF_OPAQUE:
        *HwValue = 0x0;
        break;

    case gcvSURF_SOURCE_MATCH:
        *HwValue = 0x1;
        break;

    case gcvSURF_SOURCE_MASK:
        *HwValue = 0x2;
        break;

    case gcvSURF_PATTERN_MASK:
        *HwValue = 0x3;
        break;

    default:
        /* Not supported. */
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateSourceFormat
**
**  Translate API source color format to its hardware value.
**  Checks PE2D feature to determine if the format is supported or not.
**
**  INPUT:
**
**      gceSURF_FORMAT APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateSourceFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT APIValue,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    )
{
    gceSTATUS status;
    gctUINT32 swizzle_argb, swizzle_rgba, swizzle_abgr, swizzle_bgra;
    gctUINT32 swizzle_uv, swizzle_vu;

    gcmHEADER_ARG("Hardware=0x%x APIValue=%d HwValue=0x%x "
                    "HwSwizzleValue=0x%x HwIsYUVValue=0x%x",
                    Hardware, APIValue, HwValue,
                    HwSwizzleValue, HwIsYUVValue);

    swizzle_argb = 0x0;
    swizzle_rgba = 0x1;
    swizzle_abgr = 0x2;
    swizzle_bgra = 0x3;

    swizzle_uv = 0x0;
    swizzle_vu = 0x1;

    /* Default values. */
    *HwIsYUVValue = gcvFALSE;
    *HwSwizzleValue = swizzle_argb;

    /* Dispatch on format. */
    switch (APIValue)
    {
    case gcvSURF_INDEX8:
        *HwValue = 0x9;
        break;

    case gcvSURF_X4R4G4B4:
        if (Hardware->enableXRGB)
            *HwValue = 0x0;
        else
            *HwValue = 0x1;
        break;

    case gcvSURF_A4R4G4B4:
        *HwValue = 0x1;
        break;

    case gcvSURF_X1R5G5B5:
        if (Hardware->enableXRGB)
            *HwValue = 0x2;
        else
            *HwValue = 0x3;
        break;

    case gcvSURF_A1R5G5B5:
        *HwValue = 0x3;
        break;

    case gcvSURF_R4G4B4X4:
        if (Hardware->enableXRGB)
            *HwValue = 0x0;
        else
            *HwValue = 0x1;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_R4G4B4A4:
        *HwValue = 0x1;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_B4G4R4X4:
        if (Hardware->enableXRGB)
            *HwValue = 0x0;
        else
            *HwValue = 0x1;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_B4G4R4A4:
        *HwValue = 0x1;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_X4B4G4R4:
        if (Hardware->enableXRGB)
            *HwValue = 0x0;
        else
            *HwValue = 0x1;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_A4B4G4R4:
        *HwValue = 0x1;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_R5G5B5X1:
        if (Hardware->enableXRGB)
            *HwValue = 0x2;
        else
            *HwValue = 0x3;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_R5G5B5A1:
        *HwValue = 0x3;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_B5G5R5X1:
        if (Hardware->enableXRGB)
            *HwValue = 0x2;
        else
            *HwValue = 0x3;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_B5G5R5A1:
        *HwValue = 0x3;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_X1B5G5R5:
        if (Hardware->enableXRGB)
            *HwValue = 0x2;
        else
            *HwValue = 0x3;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_A1B5G5R5:
        *HwValue = 0x3;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_R5G6B5:
    case gcvSURF_D16:
        *HwValue = 0x4;
        break;

    case gcvSURF_B5G6R5:
        *HwValue = 0x4;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_X8R8G8B8:
        if (Hardware->enableXRGB)
            *HwValue = 0x5;
        else
            *HwValue = 0x6;
        break;

    case gcvSURF_A8R8G8B8:
    case gcvSURF_D24S8:
    case gcvSURF_D24X8:
        *HwValue = 0x6;
        break;

    case gcvSURF_R8G8B8X8:
        if (Hardware->enableXRGB)
            *HwValue = 0x5;
        else
            *HwValue = 0x6;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_R8G8B8A8:
        *HwValue = 0x6;
        *HwSwizzleValue = swizzle_rgba;
        break;

    case gcvSURF_B8G8R8X8:
        if (Hardware->enableXRGB)
            *HwValue = 0x5;
        else
            *HwValue = 0x6;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_B8G8R8A8:
        *HwValue = 0x6;
        *HwSwizzleValue = swizzle_bgra;
        break;

    case gcvSURF_X8B8G8R8:
        if (Hardware->enableXRGB)
            *HwValue = 0x5;
        else
            *HwValue = 0x6;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_A8B8G8R8:
        *HwValue = 0x6;
        *HwSwizzleValue = swizzle_abgr;
        break;

    case gcvSURF_YUY2:
        *HwValue = 0x7;
        *HwSwizzleValue = swizzle_uv;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_YVYU:
        *HwValue = 0x7;
        *HwSwizzleValue = swizzle_vu;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_UYVY:
        *HwValue = 0x8;
        *HwSwizzleValue = swizzle_uv;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_VYUY:
        *HwValue = 0x8;
        *HwSwizzleValue = swizzle_vu;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_YV12:
    case gcvSURF_I420:
        *HwValue = 0xF;
        *HwSwizzleValue = swizzle_uv;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_A8:
        *HwValue = 0x10;
        break;

    case gcvSURF_NV12:
        *HwValue = 0x11;
        *HwSwizzleValue = swizzle_uv;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_NV21:
        *HwValue = 0x11;
        *HwSwizzleValue = swizzle_vu;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_NV16:
        *HwValue = 0x12;
        *HwSwizzleValue = swizzle_uv;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_NV61:
        *HwValue = 0x12;
        *HwSwizzleValue = swizzle_vu;
        *HwIsYUVValue = gcvTRUE;
        break;

    case gcvSURF_RG16:
        *HwValue = 0x13;
        break;

    case gcvSURF_R8:
        *HwValue = 0x14;
        break;
    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* For PE 1.0, return error for formats not supported. */
    if (!Hardware->features[gcvFEATURE_2DPE20])
    {
        /* Swizzled formats are not supported. */
        if ((*HwIsYUVValue && (*HwSwizzleValue != swizzle_uv) )
          || (!*HwIsYUVValue && (*HwSwizzleValue != swizzle_argb) ))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Newer added formats are not supported. */
        if (
           (*HwValue == 0x11)
        || (*HwValue == 0x12)
        || (*HwValue == 0x10)
        )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* YUV420 format are supported where available. */
        if (*HwValue == 0xF)
        {
            if (!gcoHARDWARE_IsFeatureAvailable(Hardware, gcvFEATURE_YUV420_SCALER))
            {
                gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
            }
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateSurfTransparency
**
**  Translate SURF API transparency mode to PE 2.0 transparency values.
**
**  INPUT:
**
**      gceSURF_TRANSPARENCY APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateSurfTransparency(
    IN gceSURF_TRANSPARENCY APIValue,
    OUT gce2D_TRANSPARENCY* SrcTransparency,
    OUT gce2D_TRANSPARENCY* DstTransparency,
    OUT gce2D_TRANSPARENCY* PatTransparency
    )
{
    gctUINT32 srcTransparency, patTransparency;

    gcmHEADER_ARG("APIValue=%d SrcTransparency=0x%x DstTransparency=0x%x PatTransparency=0x%x",
                    APIValue, SrcTransparency, DstTransparency, PatTransparency);

    /* Dispatch on transparency. */
    switch (APIValue)
    {
    case gcvSURF_OPAQUE:
        srcTransparency = gcv2D_OPAQUE;
        patTransparency = gcv2D_OPAQUE;
        break;

    case gcvSURF_SOURCE_MATCH:
        srcTransparency = gcv2D_KEYED;
        patTransparency = gcv2D_OPAQUE;
        break;

    case gcvSURF_SOURCE_MASK:
        srcTransparency = gcv2D_MASKED;
        patTransparency = gcv2D_OPAQUE;
        break;

    case gcvSURF_PATTERN_MASK:
        srcTransparency = gcv2D_OPAQUE;
        patTransparency = gcv2D_MASKED;
        break;

    default:
        /* Not supported. */
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    if (SrcTransparency != gcvNULL )
    {
        *SrcTransparency = srcTransparency;
    }

    if (DstTransparency != gcvNULL )
    {
        *DstTransparency = gcv2D_OPAQUE;
    }

    if (PatTransparency != gcvNULL )
    {
        *PatTransparency = patTransparency;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateTransparencies
**
**  Translate API transparency mode to its PE 1.0 hardware value.
**
**  INPUT:
**
**      gceSURF_TRANSPARENCY APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateTransparencies(
    IN gcoHARDWARE  Hardware,
    IN gctUINT32    srcTransparency,
    IN gctUINT32    dstTransparency,
    IN gctUINT32    patTransparency,
    OUT gctUINT32*  HwValue
    )
{
    gcmHEADER_ARG("Hardware=0x%x srcTransparency=%d dstTransparency=%d "
                    "patTransparency=%d HwValue=0x%x",
                    Hardware, srcTransparency, dstTransparency,
                    patTransparency, HwValue);

    /* Dispatch on transparency. */
    if (!Hardware->features[gcvFEATURE_2DPE20])
    {
        if ((srcTransparency == gcv2D_OPAQUE)
        &&  (dstTransparency == gcv2D_OPAQUE)
        &&  (patTransparency == gcv2D_OPAQUE))
        {
            *HwValue = 0x0;
        }
        else
        if ((srcTransparency == gcv2D_KEYED)
        &&  (dstTransparency == gcv2D_OPAQUE)
        &&  (patTransparency == gcv2D_OPAQUE))
        {
            *HwValue = 0x1;
        }
        else
        if ((srcTransparency == gcv2D_MASKED)
        &&  (dstTransparency == gcv2D_OPAQUE)
        &&  (patTransparency == gcv2D_OPAQUE))
        {
            *HwValue = 0x2;
        }
        else
        if ((srcTransparency == gcv2D_OPAQUE)
        &&  (dstTransparency == gcv2D_OPAQUE)
        &&  (patTransparency == gcv2D_MASKED))
        {
            *HwValue = 0x3;
        }
        else
        {
            /* PE 2.0 feature requested on PE 1.0 hardware. */
            *HwValue = 0x0;
        }
    }
    else
    {
        *HwValue = 0x0;
    }

    /* Return the status. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateSourceTransparency
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
gceSTATUS gcoHARDWARE_TranslateSourceTransparency(
    IN gce2D_TRANSPARENCY APIValue,
    OUT gctUINT32 * HwValue
    )
{
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

    case gcv2D_MASKED:
        *HwValue = 0x1;
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateSourceRotation
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
gceSTATUS gcoHARDWARE_TranslateSourceRotation(
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateDFBColorKeyMode
**
**  Translate API DFB color key mode to its hardware value.
**
**  INPUT:
**
**      gctBOOT APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateDFBColorKeyMode(
    IN  gctBOOL APIValue,
    OUT gctUINT32 * HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    if (APIValue == gcvTRUE)
    {
        *HwValue = 0x1;
    }
    else
    {
        *HwValue = 0x0;
    }

    /* Success. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslatePixelMultiplyMode
**
**  Translate API per-pixel multiply mode to its hardware value.
**
**  INPUT:
**
**      gce2D_PIXEL_COLOR_MULTIPLY_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslatePixelMultiplyMode(
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE APIValue,
    OUT gctUINT32* HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcv2D_COLOR_MULTIPLY_DISABLE:
        *HwValue = 0x0;
        break;

    case gcv2D_COLOR_MULTIPLY_ENABLE:
        *HwValue = 0x1;
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateMonoPack
**
**  Translate API mono packing mode to its hardware value.
**
**  INPUT:
**
**      gceSURF_MONOPACK APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateMonoPack(
    IN gceSURF_MONOPACK APIValue,
    OUT gctUINT32 * HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on monochrome packing. */
    switch (APIValue)
    {
    case gcvSURF_PACKED8:
        *HwValue = 0x0;
        break;

    case gcvSURF_PACKED16:
        *HwValue = 0x1;
        break;

    case gcvSURF_PACKED32:
        *HwValue = 0x2;
        break;

    case gcvSURF_UNPACKED:
        *HwValue = 0x3;
        break;

    default:
        /* Not supprted. */
        gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Success. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslateAlphaFactorMode
**
**  Translate API alpha factor source mode to its hardware value.
**
**  INPUT:
**
**      gceSURF_BLEND_FACTOR_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateAlphaFactorMode(
    IN  gcoHARDWARE               Hardware,
    IN  gctBOOL                   IsSrcFactor,
    IN  gceSURF_BLEND_FACTOR_MODE APIValue,
    OUT gctUINT32*                HwValue,
    OUT gctUINT32*                FactorExpansion
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x IsSrcFactor=%d, APIValue=%d HwValue=0x%x, FactorExpansion=0x%x",
                  Hardware, IsSrcFactor, APIValue, HwValue, FactorExpansion);

    /* Set default value for the factor expansion. */
    if (IsSrcFactor)
    {
        *FactorExpansion = 0x0;
    }
    else
    {
        *FactorExpansion = 0x0;
    }

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_BLEND_ZERO:
        *HwValue = 0x0;
        break;

    case gcvSURF_BLEND_ONE:
        *HwValue = 0x1;
        break;

    case gcvSURF_BLEND_STRAIGHT:
        *HwValue = 0x2;
        break;

    case gcvSURF_BLEND_STRAIGHT_NO_CROSS:
        if (!Hardware->features[gcvFEATURE_FULL_DIRECTFB])
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x2;

        if (IsSrcFactor)
        {
            *FactorExpansion = 0x1;
        }
        else
        {
            *FactorExpansion = 0x1;
        }
        break;

    case gcvSURF_BLEND_INVERSED:
        *HwValue = 0x3;
        break;

    case gcvSURF_BLEND_INVERSED_NO_CROSS:
        if (!Hardware->features[gcvFEATURE_FULL_DIRECTFB] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x3;
        if (IsSrcFactor)
        {
            *FactorExpansion = 0x1;
        }
        else
        {
            *FactorExpansion = 0x1;
        }
        break;

    case gcvSURF_BLEND_COLOR:
        if (!Hardware->features[gcvFEATURE_2DPE20] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x4;
        break;

    case gcvSURF_BLEND_COLOR_NO_CROSS:
        if (!Hardware->features[gcvFEATURE_FULL_DIRECTFB] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x4;
        if (IsSrcFactor)
        {
            *FactorExpansion = 0x1;
        }
        else
        {
            *FactorExpansion = 0x1;
        }
        break;

    case gcvSURF_BLEND_COLOR_INVERSED:
        if (!Hardware->features[gcvFEATURE_2DPE20] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x5;
        break;

    case gcvSURF_BLEND_COLOR_INVERSED_NO_CROSS:
        if (!Hardware->features[gcvFEATURE_FULL_DIRECTFB] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x5;

        if (IsSrcFactor)
        {
            *FactorExpansion = 0x1;
        }
        else
        {
            *FactorExpansion = 0x1;
        }
        break;

    case gcvSURF_BLEND_SRC_ALPHA_SATURATED:
        if (!Hardware->features[gcvFEATURE_2DPE20] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x6;
        break;

    case gcvSURF_BLEND_SRC_ALPHA_SATURATED_CROSS:
        if (!Hardware->features[gcvFEATURE_FULL_DIRECTFB] )
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        *HwValue = 0x7;

        if (IsSrcFactor)
        {
            *FactorExpansion = 0x1;
        }
        else
        {
            *FactorExpansion = 0x1;
        }
        break;

    default:
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Success. */
    gcmFOOTER_ARG("*HwValue=%d", *HwValue);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_EnableAlphaBlend
**
**  Enable alpha blending engine in the hardware and disengage the ROP engine.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT8 SrcGlobalAlphaValue
**      gctUINT8 DstGlobalAlphaValue
**          Global alpha value for the color components.
**
**      gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode
**      gceSURF_PIXEL_ALPHA_MODE DstAlphaMode
**          Per-pixel alpha component mode.
**
**      gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode
**      gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode
**          Global/per-pixel alpha values selection.
**
**      gceSURF_BLEND_FACTOR_MODE SrcFactorMode
**      gceSURF_BLEND_FACTOR_MODE DstFactorMode
**          Final blending factor mode.
**
**      gceSURF_PIXEL_COLOR_MODE SrcColorMode
**      gceSURF_PIXEL_COLOR_MODE DstColorMode
**          Per-pixel color component mode.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_EnableAlphaBlend(
    IN gcoHARDWARE Hardware,
    IN gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode,
    IN gceSURF_PIXEL_ALPHA_MODE DstAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode,
    IN gceSURF_BLEND_FACTOR_MODE SrcFactorMode,
    IN gceSURF_BLEND_FACTOR_MODE DstFactorMode,
    IN gceSURF_PIXEL_COLOR_MODE SrcColorMode,
    IN gceSURF_PIXEL_COLOR_MODE DstColorMode,
    IN gctUINT32 SrcGlobalAlphaValue,
    IN gctUINT32 DstGlobalAlphaValue
    )
{
    gceSTATUS status;

    /* Define hardware components. */
    gctUINT32 srcAlphaMode = 0;
    gctUINT32 srcGlobalAlphaMode = 0;
    gctUINT32 srcFactorMode = 0;
    gctUINT32 srcFactorExpansion = 0;
    gctUINT32 srcColorMode = 0;
    gctUINT32 dstAlphaMode = 0;
    gctUINT32 dstGlobalAlphaMode = 0;
    gctUINT32 dstFactorMode = 0;
    gctUINT32 dstFactorExpansion = 0;
    gctUINT32 dstColorMode = 0;

    /* State array. */
    gctUINT32 states[2];

    gcmHEADER_ARG("Hardware=0x%x SrcAlphaMode=%d DstAlphaMode=%d SrcGlobalAlphaMode=%d "
              "DstGlobalAlphaMode=%d SrcFactorMode=%d DstFactorMode=%d "
              "SrcColorMode=%d DstColorMode=%d",
              Hardware, SrcAlphaMode, DstAlphaMode, SrcGlobalAlphaMode,
              DstGlobalAlphaMode, SrcFactorMode, DstFactorMode,
              SrcColorMode, DstColorMode);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Translate inputs. */
    gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
        SrcAlphaMode, &srcAlphaMode
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelAlphaMode(
        DstAlphaMode, &dstAlphaMode
        ));

    gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
        SrcGlobalAlphaMode, &srcGlobalAlphaMode
        ));

    gcmONERROR(gcoHARDWARE_TranslateGlobalAlphaMode(
        DstGlobalAlphaMode, &dstGlobalAlphaMode
        ));

    gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
        Hardware, gcvTRUE, SrcFactorMode, &srcFactorMode, &srcFactorExpansion
        ));

    gcmONERROR(gcoHARDWARE_TranslateAlphaFactorMode(
        Hardware, gcvFALSE, DstFactorMode, &dstFactorMode, &dstFactorExpansion
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMode(
        SrcColorMode, &srcColorMode
        ));

    gcmONERROR(gcoHARDWARE_TranslatePixelColorMode(
        DstColorMode, &dstColorMode
        ));

    /*
        Fill in the states.
    */

    /* Enable alpha blending and set global alpha values. */
    states[0] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16))) | (((gctUINT32) ((gctUINT32) (SrcGlobalAlphaValue) & ((gctUINT32) ((((1 ? 23:16) - (0 ? 23:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:16) - (0 ? 23:16) + 1))))))) << (0 ? 23:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24))) | (((gctUINT32) ((gctUINT32) (DstGlobalAlphaValue) & ((gctUINT32) ((((1 ? 31:24) - (0 ? 31:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:24) - (0 ? 31:24) + 1))))))) << (0 ? 31:24)));

    /* Set alpha blending modes. */
    states[1] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) ((gctUINT32) (srcAlphaMode) & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) (dstAlphaMode) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8))) | (((gctUINT32) ((gctUINT32) (srcGlobalAlphaMode) & ((gctUINT32) ((((1 ? 9:8) - (0 ? 9:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:8) - (0 ? 9:8) + 1))))))) << (0 ? 9:8)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12))) | (((gctUINT32) ((gctUINT32) (dstGlobalAlphaMode) & ((gctUINT32) ((((1 ? 13:12) - (0 ? 13:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:12) - (0 ? 13:12) + 1))))))) << (0 ? 13:12)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16))) | (((gctUINT32) ((gctUINT32) (srcColorMode) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1))))))) << (0 ? 16:16)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20))) | (((gctUINT32) ((gctUINT32) (dstColorMode) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1))))))) << (0 ? 20:20)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (srcFactorMode) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)))
              | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28))) | (((gctUINT32) ((gctUINT32) (dstFactorMode) & ((gctUINT32) ((((1 ? 30:28) - (0 ? 30:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 30:28) - (0 ? 30:28) + 1))))))) << (0 ? 30:28)));
    if (Hardware->features[gcvFEATURE_FULL_DIRECTFB])
    {
        states[1] |= ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27))) | (((gctUINT32) ((gctUINT32) (srcFactorExpansion) & ((gctUINT32) ((((1 ? 27:27) - (0 ? 27:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 27:27) - (0 ? 27:27) + 1))))))) << (0 ? 27:27)))
                   | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31))) | (((gctUINT32) ((gctUINT32) (dstFactorExpansion) & ((gctUINT32) ((((1 ? 31:31) - (0 ? 31:31) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:31) - (0 ? 31:31) + 1))))))) << (0 ? 31:31)));
    }

    /* LoadState(AQDE_ALPHA_CONTROL, 2), states. */
    gcmONERROR(gcoHARDWARE_Load2DState(Hardware, 0x0127C,
                                   2, states));

    if (Hardware->features[gcvFEATURE_2DPE20])
    {
        if (SrcGlobalAlphaMode != gcvSURF_GLOBAL_ALPHA_OFF)
        {
            /* Set source global color. */
            gcmONERROR(gcoHARDWARE_SetSourceGlobalColor(
                Hardware,
                SrcGlobalAlphaValue
                ));
        }

        if (DstGlobalAlphaMode != gcvSURF_GLOBAL_ALPHA_OFF)
        {
            /* Set target global color. */
            gcmONERROR(gcoHARDWARE_SetTargetGlobalColor(
                Hardware,
                DstGlobalAlphaValue
                ));
        }

        gcmONERROR(gcoHARDWARE_Load2DState32(
            Hardware,
            0x012D8,
            (((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) &  ((((gctUINT32) (~0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 11:11) - (0 ? 11:11) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 11:11) - (0 ? 11:11) + 1))))))) << (0 ? 11:11))))
            ));
    }

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_DisableAlphaBlend
**
**  Disable alpha blending engine in the hardware and engage the ROP engine.
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
gceSTATUS gcoHARDWARE_DisableAlphaBlend(
    IN gcoHARDWARE Hardware
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x", Hardware);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    gcmONERROR(gcoHARDWARE_Load2DState32(
        Hardware,
        0x0127C,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0))) | (((gctUINT32) (0x0 & ((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1))))))) << (0 ? 0:0)))
        ));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_TranslatePixelAlphaMode
**
**  Translate API per-pixel alpha mode to its hardware value.
**
**  INPUT:
**
**      gceSURF_PIXEL_ALPHA_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslatePixelAlphaMode(
    IN gceSURF_PIXEL_ALPHA_MODE APIValue,
    OUT gctUINT32* HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_PIXEL_ALPHA_STRAIGHT:
        *HwValue = 0x0;
        break;

    case gcvSURF_PIXEL_ALPHA_INVERSED:
        *HwValue = 0x1;
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslateGlobalAlphaMode
**
**  Translate API global alpha mode to its hardware value.
**
**  INPUT:
**
**      gceSURF_GLOBAL_ALPHA_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslateGlobalAlphaMode(
    IN gceSURF_GLOBAL_ALPHA_MODE APIValue,
    OUT gctUINT32* HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_GLOBAL_ALPHA_OFF:
        *HwValue = 0x0;
        break;

    case gcvSURF_GLOBAL_ALPHA_ON:
        *HwValue = 0x1;
        break;

    case gcvSURF_GLOBAL_ALPHA_SCALE:
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

/*******************************************************************************
**
**  gcoHARDWARE_TranslatePixelColorMode
**
**  Translate API per-pixel color mode to its hardware value.
**
**  INPUT:
**
**      gceSURF_PIXEL_COLOR_MODE APIValue
**          API value.
**
**  OUTPUT:
**
**      gctUINT32 * HwValue
**          Corresponding hardware value.
*/
gceSTATUS gcoHARDWARE_TranslatePixelColorMode(
    IN gceSURF_PIXEL_COLOR_MODE APIValue,
    OUT gctUINT32* HwValue
    )
{
    gcmHEADER_ARG("APIValue=%d HwValue=0x%x", APIValue, HwValue);

    /* Dispatch on command. */
    switch (APIValue)
    {
    case gcvSURF_COLOR_STRAIGHT:
        *HwValue = 0x0;
        break;

    case gcvSURF_COLOR_MULTIPLY:
        *HwValue = 0x1;
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

/*******************************************************************************
**
**  gcoHARDWARE_GetStretchFactor
**
**  Calculate stretch factor.
**
**  INPUT:
**
**      gctINT32 SrcSize
**          The size in pixels of a source dimension (width or height).
**
**      gctINT32 DestSize
**          The size in pixels of a destination dimension (width or height).
**
**  OUTPUT:
**
**      Nothing.
**
**  RETURN:
**
**      gctUINT32
**          Stretch factor in 16.16 fixed point format.
*/
gctUINT32 gcoHARDWARE_GetStretchFactor(
    IN gctBOOL GdiStretch,
    IN gctINT32 SrcSize,
    IN gctINT32 DestSize
    )
{
    gctUINT stretchFactor = 0;

    gcmHEADER_ARG("SrcSize=%d DestSize=%d", SrcSize, DestSize);

    if (!GdiStretch && (SrcSize > 1) && (DestSize > 1))
    {
        stretchFactor = ((SrcSize - 1) << 16) / (DestSize - 1);
    }
    else if ((SrcSize > 0) && (DestSize > 0))
    {
        stretchFactor = (SrcSize << 16) / DestSize;
    }

    gcmFOOTER_ARG("return=%d", stretchFactor);
    return stretchFactor;
}

/*******************************************************************************
**
**  gcoHARDWARE_GetStretchFactors
**
**  Calculate the stretch factors.
**
**  INPUT:
**
**      gcsRECT_PTR SrcRect
**          Pointer to a valid source rectangles.
**
**      gcsRECT_PTR DestRect
**          Pointer to a valid destination rectangles.
**
**  OUTPUT:
**
**      gctUINT32 * HorFactor
**          Pointer to a variable that will receive the horizontal stretch
**          factor.
**
**      gctUINT32 * VerFactor
**          Pointer to a variable that will receive the vertical stretch factor.
*/
gceSTATUS gcoHARDWARE_GetStretchFactors(
    IN gctBOOL GdiStretch,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    OUT gctUINT32 * HorFactor,
    OUT gctUINT32 * VerFactor
    )
{
    gcmHEADER_ARG("SrcRect=0x%x DestRect=0x%x HorFactor=0x%x VerFactor=0x%x",
                    SrcRect, DestRect, HorFactor, VerFactor);

    if (HorFactor != gcvNULL)
    {
        gctINT32 src  = 0;
        gctINT32 dest = 0;

        /* Compute width of rectangles. */
        gcmVERIFY_OK(gcsRECT_Width(SrcRect, &src));
        gcmVERIFY_OK(gcsRECT_Width(DestRect, &dest));

        /* Compute and return horizontal stretch factor. */
        *HorFactor = gcoHARDWARE_GetStretchFactor(GdiStretch, src, dest);
    }

    if (VerFactor != gcvNULL)
    {
        gctINT32 src  = 0;
        gctINT32 dest = 0;

        /* Compute height of rectangles. */
        gcmVERIFY_OK(gcsRECT_Height(SrcRect, &src));
        gcmVERIFY_OK(gcsRECT_Height(DestRect, &dest));

        /* Compute and return vertical stretch factor. */
        *VerFactor = gcoHARDWARE_GetStretchFactor(GdiStretch, src, dest);
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHARDWARE_SetStretchFactors
**
**  Calculate and program the stretch factors.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctUINT32 HorFactor
**          Horizontal stretch factor.
**
**      gctUINT32 VerFactor
**          Vertical stretch factor.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_SetStretchFactors(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 HorFactor,
    IN gctUINT32 VerFactor
    )
{
    gceSTATUS status;
    gctUINT32 memory[2];

    gcmHEADER_ARG("Hardware=0x%x HorFactor=%d VerFactor=%d",
                    Hardware, HorFactor, VerFactor);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    /* Set states into temporary buffer. */
    memory[0] = HorFactor;
    memory[1] = VerFactor;

    /* Through load state command. */
    gcmONERROR(gcoHARDWARE_Load2DState(
        Hardware, 0x01220, 2, memory
        ));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_UploadCSCTable
**
**  Update CSC tables.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctBOOL YUV2RGB
**          Whether YUV to RGB; else RGB to YUV;
**
**      gctINT32_PTR Table
**          Coefficient tabl;
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS gcoHARDWARE_UploadCSCTable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL YUV2RGB,
    IN gctINT32_PTR Table
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT coef[8] = {0};
    gctINT i;

    gcmHEADER_ARG("Hardware=0x%x Table=0x%x", Hardware, Table);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);

    if (!Hardware->features[gcvFEATURE_2D_COLOR_SPACE_CONVERSION])
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Through load state command. */
    for (i = 0; i < 12; ++i)
    {
        if (i < 9)
        {
            coef[i >> 1] = (i & 1) ?
                ((((gctUINT32) (coef[i >> 1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Table[i]) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
              : ((((gctUINT32) (coef[i >> 1])) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Table[i]) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));
        }
        else
        {
            coef[i - 4] = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:0) - (0 ? 24:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:0) - (0 ? 24:0) + 1))))))) << (0 ? 24:0))) | (((gctUINT32) ((gctUINT32) (Table[i]) & ((gctUINT32) ((((1 ? 24:0) - (0 ? 24:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:0) - (0 ? 24:0) + 1))))))) << (0 ? 24:0)));
        }
    }

    if (YUV2RGB)
    {
        gcmONERROR(gcoHARDWARE_Load2DState(Hardware, 0x01340,
                                   8, coef));
    }
    else
    {
        gcmONERROR(gcoHARDWARE_Load2DState(Hardware, 0x01360,
                                   8, coef));
    }

OnError:

    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_2D */

