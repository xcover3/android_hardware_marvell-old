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


/*
**  Shader programming..
*/

#include "gc_hal_user_hardware_precomp.h"

#if gcdENABLE_3D

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HARDWARE

#define IS_HW_SUPPORT(feature) gcoHARDWARE_IsFeatureAvailable(gcvNULL, (feature))

gceSTATUS
gcoHARDWARE_QueryShaderCompilerHwCfg(
    IN gcoHARDWARE Hardware,
    OUT PVSC_HW_CONFIG pVscHwCfg
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT32 maxVaryingCount, vsSamplerCount, psSamplerCount, maxAttribs;
    gctINT    vsSamplerBase, psSamplerBase;

    gcmHEADER_ARG("Hardware=0x%x pVscHwCfg=%d", Hardware, pVscHwCfg);

    gcmGETHARDWARE(Hardware );

    gcmONERROR(gcoHARDWARE_QueryShaderCaps(gcvNULL,
                                           gcvNULL,
                                           gcvNULL,
                                           gcvNULL,
                                           &maxVaryingCount,
                                           gcvNULL,
                                           gcvNULL,
                                           gcvNULL,
                                           gcvNULL));

    gcmONERROR(gcoHARDWARE_QuerySamplerBase(gcvNULL,
                                            &vsSamplerCount,
                                            &vsSamplerBase,
                                            &psSamplerCount,
                                            &psSamplerBase));

    gcmONERROR(gcoHARDWARE_QueryStreamCaps(gcvNULL,
                                           &maxAttribs,
                                           gcvNULL,
                                           gcvNULL,
                                           gcvNULL));

    pVscHwCfg->chipModel                             = Hardware->config->chipModel;
    pVscHwCfg->chipRevision                          = Hardware->config->chipRevision;
    pVscHwCfg->maxCoreCount                          = Hardware->config->shaderCoreCount;
    pVscHwCfg->maxThreadCountPerCore                 = Hardware->config->threadCount;
    pVscHwCfg->maxVaryingCount                       = maxVaryingCount;
    pVscHwCfg->maxAttributeCount                     = maxAttribs;
    pVscHwCfg->maxGPRCountPerThread                  = Hardware->config->registerMax;
    pVscHwCfg->maxHwNativeTotalInstCount             = Hardware->config->instructionCount;
    pVscHwCfg->maxTotalInstCount                     = Hardware->config->instMax;
    pVscHwCfg->maxVSInstCount                        = Hardware->config->vsInstMax;
    pVscHwCfg->maxPSInstCount                        = Hardware->config->psInstMax;
    pVscHwCfg->vsInstBase                            = Hardware->config->vsInstBase;
    pVscHwCfg->psInstBase                            = Hardware->config->psInstBase;
    pVscHwCfg->maxHwNativeTotalConstRegCount         = Hardware->config->numConstants;
    pVscHwCfg->maxTotalConstRegCount                 = Hardware->config->constMax;
    pVscHwCfg->maxVSConstRegCount                    = Hardware->config->vsConstMax;
    pVscHwCfg->maxPSConstRegCount                    = Hardware->config->psConstMax;
    pVscHwCfg->vsConstRegBase                        = Hardware->config->vsConstBase;
    pVscHwCfg->psConstRegBase                        = Hardware->config->psConstBase;
    pVscHwCfg->vsSamplerBase                         = vsSamplerBase;
    pVscHwCfg->psSamplerBase                         = psSamplerBase;
    pVscHwCfg->maxVSSamplerCount                     = vsSamplerCount;
    pVscHwCfg->maxPSSamplerCount                     = psSamplerCount;
    pVscHwCfg->vertexOutputBufferSize                = Hardware->config->vertexOutputBufferSize;
    pVscHwCfg->vertexCacheSize                       = Hardware->config->vertexCacheSize;
    pVscHwCfg->ctxStateCount                         = Hardware->stateCount;

    pVscHwCfg->hwFeatureFlags.hasHalti0              = IS_HW_SUPPORT(gcvFEATURE_HALTI0);
    pVscHwCfg->hwFeatureFlags.hasHalti1              = IS_HW_SUPPORT(gcvFEATURE_HALTI1);
    pVscHwCfg->hwFeatureFlags.hasHalti2              = IS_HW_SUPPORT(gcvFEATURE_HALTI2);
    pVscHwCfg->hwFeatureFlags.hasSignFloorCeil       = ((((gctUINT32) (Hardware->config->chipMinorFeatures)) >> (0 ? 16:16) & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 16:16) - (0 ? 16:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 16:16) - (0 ? 16:16) + 1)))))));
    pVscHwCfg->hwFeatureFlags.hasSqrtTrig            = ((((gctUINT32) (Hardware->config->chipMinorFeatures)) >> (0 ? 20:20) & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 20:20) - (0 ? 20:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 20:20) - (0 ? 20:20) + 1)))))));
    pVscHwCfg->hwFeatureFlags.hasNewSinCosLogDiv     = ((((gctUINT32) (Hardware->config->chipMinorFeatures3)) >> (0 ? 14:14) & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 14:14) - (0 ? 14:14) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 14:14) - (0 ? 14:14) + 1)))))));
    pVscHwCfg->hwFeatureFlags.hasMediumPrecision     = ((((gctUINT32) (Hardware->config->chipMinorFeatures4)) >> (0 ? 28:28) & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))) == (0x1 & ((gctUINT32) ((((1 ? 28:28) - (0 ? 28:28) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 28:28) - (0 ? 28:28) + 1)))))));
    pVscHwCfg->hwFeatureFlags.canBranchOnImm         = IS_HW_SUPPORT(gcvFEATURE_BRANCH_ON_IMMEDIATE_REG);
    pVscHwCfg->hwFeatureFlags.supportDual16          = IS_HW_SUPPORT(gcvFEATURE_DUAL_16);
    pVscHwCfg->hwFeatureFlags.hasBugFix8             = IS_HW_SUPPORT(gcvFEATURE_BUG_FIXES8);
    pVscHwCfg->hwFeatureFlags.hasBugFix10            = IS_HW_SUPPORT(gcvFEATURE_BUG_FIXES10);
    pVscHwCfg->hwFeatureFlags.hasBugFix11            = IS_HW_SUPPORT(gcvFEATURE_BUG_FIXES11);
    pVscHwCfg->hwFeatureFlags.hasSHEnhance2          = IS_HW_SUPPORT(gcvFEATURE_SHADER_ENHANCEMENTS2);
    pVscHwCfg->hwFeatureFlags.hasInstCache           = IS_HW_SUPPORT(gcvFEATURE_SHADER_HAS_INSTRUCTION_CACHE);
    pVscHwCfg->hwFeatureFlags.instBufferUnified      = Hardware->config->unifiedShader;
    pVscHwCfg->hwFeatureFlags.constRegFileUnified    = Hardware->config->unifiedConst;
    pVscHwCfg->hwFeatureFlags.bigEndianMI            = Hardware->bigEndian;
    pVscHwCfg->hwFeatureFlags.raPushPosW             = IS_HW_SUPPORT(gcvFEATURE_SHADER_HAS_W);
    pVscHwCfg->hwFeatureFlags.vtxInstanceIdAsAttr    = IS_HW_SUPPORT(gcvFEATURE_VERTEX_INST_ID_AS_ATTRIBUTE);
    pVscHwCfg->hwFeatureFlags.vtxInstanceIdAsInteger = IS_HW_SUPPORT(gcvFEATURE_VERTEX_INST_ID_AS_INTEGER);
    pVscHwCfg->hwFeatureFlags.hasSHEnhance3          = IS_HW_SUPPORT(gcvFEATURE_SHADER_ENHANCEMENTS3);
    pVscHwCfg->hwFeatureFlags.rtneRoundingEnabled    = IS_HW_SUPPORT(gcvFEATURE_SHADER_HAS_RTNE);
    pVscHwCfg->hwFeatureFlags.hasThreadWalkerInPS    = Hardware->threadWalkerInPS;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHARDWARE_LoadShaders
**
**  Load the vertex and pixel shaders.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**      gctSIZE_T StateBufferSize
**          The number of bytes in the 'StateBuffer'.
**
**      gctPOINTER StateBuffer
**          Pointer to the states that make up the shader program.
**
**      gcsHINT_PTR Hints
**          Pointer to a gcsHINT structure that contains information required
**          when loading the shader states.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHARDWARE_LoadShaders(
                        IN gcoHARDWARE Hardware,
                        IN gctSIZE_T StateBufferSize,
                        IN gctPOINTER StateBuffer,
                        IN gcsHINT_PTR Hints
                        )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Hardware=0x%x StateBufferSize=%d StateBuffer=0x%x Hints=0x%x",
        Hardware, StateBufferSize, StateBuffer, Hints);

    gcmGETHARDWARE(Hardware );

    /* Store shader states. */
    if (!((StateBufferSize == 0 || StateBuffer == gcvNULL) && Hardware->shaderDirty))
    {
        Hardware->shaderStates.stateBufferSize = StateBufferSize;
        Hardware->shaderStates.stateBuffer     = (gctUINT32_PTR) StateBuffer;
    }

    Hardware->shaderStates.hints = Hints;

    /* Invalidate the states. */
    Hardware->shaderDirty = gcvTRUE;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_LoadKernel(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x StateBufferSize=%d StateBuffer=0x%x Hints=0x%x",
        Hardware, StateBufferSize, StateBuffer, Hints);

    gcmGETHARDWARE(Hardware);

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(Hardware, gcvPIPE_3D, gcvNULL));

    /* Load shaders. */
    if (StateBufferSize > 0)
    {
        gcmONERROR(gcoHARDWARE_LoadShaders(Hardware,
            StateBufferSize,
            StateBuffer,
            Hints));
    }

    if (Hardware->threadWalkerInPS)
    {
        /* Set input count. */
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
            0x01008,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (Hints->fsInputCount) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24))) | (((gctUINT32) ((gctUINT32) (Hints->psInputControlHighpPosition) & ((gctUINT32) ((((1 ? 24:24) - (0 ? 24:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 24:24) - (0 ? 24:24) + 1))))))) << (0 ? 24:24)))));
#if TEMP_SHADER_PATCH
        if (Hints != gcvNULL)
        {
            switch (Hints->pachedShaderIdentifier)
            {
            case gcvMACHINECODE_GLB27_RELEASE_0:
                Hints->fsMaxTemp = 0x00000004;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_0:
                Hints->fsMaxTemp = 0x00000008;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_1:
                Hints->fsMaxTemp = 0x00000008;
                break;

            case gcvMACHINECODE_GLB25_RELEASE_2:
                Hints->fsMaxTemp = 0x00000004;
                break;

            default:
                break;
            }
        }
#endif

        /* Set temporary register control. */
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
            0x0100C,
            Hints->fsMaxTemp));
    }
    else
    {
        /* Set input count. */
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
            0x00808,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (Hints->fsInputCount) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8))) | (((gctUINT32) ((gctUINT32) (~0) & ((gctUINT32) ((((1 ? 12:8) - (0 ? 12:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 12:8) - (0 ? 12:8) + 1))))))) << (0 ? 12:8)))));

        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
            0x00820,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 13:8) - (0 ? 13:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 13:8) - (0 ? 13:8) + 1))))))) << (0 ? 13:8))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) | (((gctUINT32) ((gctUINT32) (2) & ((gctUINT32) ((((1 ? 21:16) - (0 ? 21:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 21:16) - (0 ? 21:16) + 1))))))) << (0 ? 21:16))) |
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24))) | (((gctUINT32) ((gctUINT32) (3) & ((gctUINT32) ((((1 ? 29:24) - (0 ? 29:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 29:24) - (0 ? 29:24) + 1))))))) << (0 ? 29:24)))));
        gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
            0x0080C,
            Hints->fsMaxTemp));
    }

    /* Set output count. */
    gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
        0x00804,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 5:0) - (0 ? 5:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:0) - (0 ? 5:0) + 1))))))) << (0 ? 5:0)))));

    /* Set local storage register count. */
    /* Set it to 0 since it is not used. */
    gcmONERROR(gcoHARDWARE_LoadState32(Hardware,
        0x00924,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))));

    /* Success. */
    status = gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
/*******************************************************************************
**  gcoHARDWARE_InvokeThreadWalker
**
**  Start OCL thread walker.
**
**  INPUT:
**
**      gcoHARDWARE Hardware
**          Pointer to an gcoHARDWARE object.
**
**
**      gcsTHREAD_WALKER_INFO_PTR Info
**          Pointer to the informational structure.
*/
gceSTATUS
gcoHARDWARE_InvokeThreadWalker(
                               IN gcoHARDWARE Hardware,
                               IN gcsTHREAD_WALKER_INFO_PTR Info
                               )
{
    gceSTATUS status;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Info=0x%08X", Hardware, Info);

    gcmGETHARDWARE(Hardware);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Info != gcvNULL);

    reserveSize = gcmALIGN(1 + 10 * gcmSIZEOF(gctUINT32), 8);

    if (Hardware->features[gcvFEATURE_SHADER_ENHANCEMENTS2])
    {
        reserveSize += gcmALIGN(1 + 6 * gcmSIZEOF(gctUINT32), 8);
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)8  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0240, 8 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (8 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0240) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0240,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0))) | (((gctUINT32) ((gctUINT32) (Info->dimensions) & ((gctUINT32) ((((1 ? 1:0) - (0 ? 1:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 1:0) - (0 ? 1:0) + 1))))))) << (0 ? 1:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4))) | (((gctUINT32) ((gctUINT32) (Info->traverseOrder) & ((gctUINT32) ((((1 ? 6:4) - (0 ? 6:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 6:4) - (0 ? 6:4) + 1))))))) << (0 ? 6:4)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8))) | (((gctUINT32) ((gctUINT32) (Info->enableSwathX) & ((gctUINT32) ((((1 ? 8:8) - (0 ? 8:8) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 8:8) - (0 ? 8:8) + 1))))))) << (0 ? 8:8)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9))) | (((gctUINT32) ((gctUINT32) (Info->enableSwathY) & ((gctUINT32) ((((1 ? 9:9) - (0 ? 9:9) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:9) - (0 ? 9:9) + 1))))))) << (0 ? 9:9)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10))) | (((gctUINT32) ((gctUINT32) (Info->enableSwathZ) & ((gctUINT32) ((((1 ? 10:10) - (0 ? 10:10) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 10:10) - (0 ? 10:10) + 1))))))) << (0 ? 10:10)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12))) | (((gctUINT32) ((gctUINT32) (Info->swathSizeX) & ((gctUINT32) ((((1 ? 15:12) - (0 ? 15:12) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:12) - (0 ? 15:12) + 1))))))) << (0 ? 15:12)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16))) | (((gctUINT32) ((gctUINT32) (Info->swathSizeY) & ((gctUINT32) ((((1 ? 19:16) - (0 ? 19:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 19:16) - (0 ? 19:16) + 1))))))) << (0 ? 19:16)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20))) | (((gctUINT32) ((gctUINT32) (Info->swathSizeZ) & ((gctUINT32) ((((1 ? 23:20) - (0 ? 23:20) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 23:20) - (0 ? 23:20) + 1))))))) << (0 ? 23:20)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24))) | (((gctUINT32) ((gctUINT32) (Info->valueOrder) & ((gctUINT32) ((((1 ? 26:24) - (0 ? 26:24) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:24) - (0 ? 26:24) + 1))))))) << (0 ? 26:24)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0241,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Info->globalSizeX) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->globalOffsetX) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0242,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Info->globalSizeY) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->globalOffsetY) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0243,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (Info->globalSizeZ) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->globalOffsetZ) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0244,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeX - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->workGroupCountX - 1) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0245,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeY - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->workGroupCountY - 1) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0246,
        ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeZ - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
        | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16))) | (((gctUINT32) ((gctUINT32) (Info->workGroupCountZ - 1) & ((gctUINT32) ((((1 ? 31:16) - (0 ? 31:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:16) - (0 ? 31:16) + 1))))))) << (0 ? 31:16)))
        );

    gcmSETSTATEDATA(
        stateDelta, reserve, memory, gcvFALSE, 0x0247,
        Info->threadAllocation
        );

    gcmSETFILLER(
        reserve, memory
        );

    gcmENDSTATEBATCH(
        reserve, memory
        );

    if (Hardware->features[gcvFEATURE_SHADER_ENHANCEMENTS2])
    {
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)6  <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0250, 6 );    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (6 ) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0250) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0250,
            Info->workGroupCountX - 1
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0251,
            Info->workGroupCountY - 1
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0252,
            Info->workGroupCountZ - 1
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0253,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeX - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0254,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeY - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
            );

        gcmSETSTATEDATA(
            stateDelta, reserve, memory, gcvFALSE, 0x0255,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0))) | (((gctUINT32) ((gctUINT32) (Info->workGroupSizeZ - 1) & ((gctUINT32) ((((1 ? 9:0) - (0 ? 9:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 9:0) - (0 ? 9:0) + 1))))))) << (0 ? 9:0)))
            );

        gcmSETFILLER(
            reserve, memory
            );

        gcmENDSTATEBATCH(
            reserve, memory
            );
    }

    {{    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0248, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0248) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};gcmSETCTRLSTATE(stateDelta, reserve, memory, 0x0248, 0xBADABEEB );gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Flush the Shader L1 cache. */
    gcmONERROR(gcoHARDWARE_LoadState32(
        Hardware,
        0x0380C,
            ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 5:5) - (0 ? 5:5) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 5:5) - (0 ? 5:5) + 1))))))) << (0 ? 5:5)))
           |((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2))) | (((gctUINT32) (0x1 & ((gctUINT32) ((((1 ? 2:2) - (0 ? 2:2) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 2:2) - (0 ? 2:2) + 1))))))) << (0 ? 2:2)))
        ));

    /* Add FE - PE samaphore stall to prevent unwanted SHL1_CACHE flush. */
    gcmONERROR(gcoHARDWARE_Semaphore(
        Hardware, gcvWHERE_COMMAND, gcvWHERE_PIXEL, gcvHOW_SEMAPHORE_STALL, gcvNULL));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_InvokeThreadWalkerWrapper(
    IN gcoHARDWARE Hardware,
    IN gcsTHREAD_WALKER_INFO_PTR Info
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hardware=0x%x Info=0x%x", Hardware, Info);

    gcmGETHARDWARE(Hardware);

    /* Switch to the 3D pipe. */
    gcmONERROR(gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL));

    if (Hardware->shaderDirty)
    {
        /* Flush shader states. */
        gcmONERROR(gcoHARDWARE_FlushShaders(Hardware, gcvPRIMITIVE_TRIANGLE_LIST, gcvNULL));
    }

    /* Route to hardware. */
    status = gcoHARDWARE_InvokeThreadWalker(Hardware, Info);

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_ProgramUniform(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT Columns,
    IN gctUINT Rows,
    IN gctCONST_POINTER Values,
    IN gctBOOL FixedPoint,
    IN gctBOOL ConvertToFloat,
    IN gcSHADER_KIND Type
    )
{
    gceSTATUS status;
    gctUINT i, j;
    gctUINT32_PTR src = (gctUINT32_PTR) Values;
    gctUINT32 address = Address >> 2;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x, Address=%u Columns=%u Rows=%u Values=%p FixedPoint=%d, Type=%d",
                  Hardware, Address, Columns, Rows, Values, FixedPoint, Type);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Values != gcvNULL);

    /* Get the current hardware object. */
    gcmGETHARDWARE(Hardware);

    if (address >= Hardware->stateCount)
    {
        /* If the uniform (sampler) was not assigned address, cannot call into here */
        gcmONERROR(gcvSTATUS_INVALID_DATA);
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = Rows * gcmALIGN((1 + Columns) * gcmSIZEOF(gctUINT32), 8);

    if (Hardware->unifiedConst)
    {
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    if (Hardware->unifiedConst)
    {
        gctUINT32 shaderConfigData = Hardware->shaderStates.hints ?
                                     Hardware->shaderStates.hints->shaderConfigData : 0;

        shaderConfigData = ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) ((Type != gcSHADER_TYPE_VERTEX)) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0218, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0218, shaderConfigData);     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Walk all rows. */
    for (i = 0; i < Rows; i++)
    {
        /* Begin the state batch. */
        {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)Columns <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, address, Columns);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (FixedPoint) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (Columns) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

        /* Walk all columns. */
        for (j = 0; j < Columns; j++)
        {
            gctINT32 data = *src++;
            if (ConvertToFloat)
            {
                gctFLOAT fData = (gctFLOAT)data;
                data = *((gctUINT32_PTR)&fData);
            }

            /* Set the state value. */
            gcmSETSTATEDATA(stateDelta,
                            reserve,
                            memory,
                            FixedPoint,
                            address + j,
                            data);
        }

        if ((j & 1) == 0)
        {
            /* Align. */
            gcmSETFILLER(reserve, memory);
        }

        /* End the state batch. */
        gcmENDSTATEBATCH(reserve, memory);

        /* Next row. */
        address += 4;
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_ProgramUniformEx(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT Columns,
    IN gctUINT Rows,
    IN gctUINT Arrays,
    IN gctBOOL IsRowMajor,
    IN gctUINT MatrixStride,
    IN gctUINT ArrayStride,
    IN gctCONST_POINTER Values,
    IN gceUNIFORMCVT Convert,
    IN gcSHADER_KIND Type
    )
{
    gceSTATUS status;
    gctUINT arr, row, col;
    gctUINT32 address = Address >> 2;
    gctUINT8_PTR pArray = (gctUINT8_PTR)Values;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x, Address=%u Columns=%u Rows=%u Arrays=%u IsRowMajor=%d "
                  "MatrixStride=%d ArrayStride=%d Values=%p Convert=%d, Type=%d",
                  Hardware, Address, Columns, Rows, Arrays, IsRowMajor,
                  MatrixStride, ArrayStride, Values, Convert, Type);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Values != gcvNULL);

    /* Get the current hardware object. */
    gcmGETHARDWARE(Hardware);

    if (address >= Hardware->stateCount)
    {
        /* If the uniform (sampler) was not assigned address, cannot call into here */
        gcmONERROR(gcvSTATUS_INVALID_DATA);
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = Arrays * Rows * gcmALIGN((1 + Columns) * gcmSIZEOF(gctUINT32), 8);

    if (Hardware->unifiedConst)
    {
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    if (Hardware->unifiedConst)
    {
        gctUINT32 shaderConfigData = Hardware->shaderStates.hints ?
                                     Hardware->shaderStates.hints->shaderConfigData : 0;

        shaderConfigData = ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) ((Type != gcSHADER_TYPE_VERTEX)) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0218, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0218, shaderConfigData);     gcmENDSTATEBATCH(reserve, memory);};
    }

    for (arr = 0; arr < Arrays; ++arr)
    {
        /* Walk all rows. */
        for (row = 0; row < Rows; ++row)
        {
            /* Begin the state batch. */
            {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)Columns <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, address, Columns);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (Columns) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};

            /* Walk all columns. */
            for (col = 0; col < Columns; ++col)
            {
                gctUINT8_PTR pData = IsRowMajor
                                   ? pArray + col * MatrixStride + (row << 2)
                                   : pArray + row * MatrixStride + (col << 2);

                gctUINT data = *(gctUINT_PTR)pData;

                if (Convert == gcvUNIFORMCVT_TO_BOOL)
                {
                    data = (data == 0) ? 0 : 1;
                }
                else if (Convert == gcvUNIFORMCVT_TO_FLOAT)
                {
                    gctFLOAT fData = (gctFLOAT)(gctINT)data;
                    data = *((gctUINT32_PTR)&fData);
                }

                /* Set the state value. */
                gcmSETSTATEDATA(stateDelta,
                                    reserve,
                                    memory,
                                    gcvFALSE,
                                    address + col,
                                    data);
            }

            if ((col & 1) == 0)
            {
                /* Align. */
                gcmSETFILLER(reserve, memory);
            }

            /* End the state batch. */
            gcmENDSTATEBATCH(reserve, memory);

            /* Next row. */
            address += 4;
        }

        pArray += ArrayStride;
    }

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHARDWARE_BindUniformBlock(
        IN gcoHARDWARE Hardware,
        IN gctUINT32 Address,
        IN gctUINT32 Base,
        IN gctSIZE_T Offset,
        IN gctSIZE_T Size,
        IN gcSHADER_KIND Type
        )
{
    gceSTATUS status;
    gctUINT32 address = Address >> 2;

    /* Define state buffer variables. */
    gcmDEFINESTATEBUFFER(reserve, stateDelta, memory, reserveSize);

    gcmHEADER_ARG("Hardware=0x%x Address=%u Base=%u Offset=%lu Size=%lu, Type=%d",
                  Hardware, Address, Base, Offset, Size, Type);

    /* Get the current hardware object. */
    gcmGETHARDWARE(Hardware);

    if (address >= Hardware->stateCount)
    {
        /* If the uniform (sampler) was not assigned address, cannot call into here */
        gcmONERROR(gcvSTATUS_INVALID_DATA);
    }

    /* Determine the size of the buffer to reserve. */
    reserveSize = gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);

    if (Hardware->unifiedConst)
    {
        reserveSize += gcmALIGN((1 + 1) * gcmSIZEOF(gctUINT32), 8);
    }

    /* Reserve space in the command buffer. */
    gcmBEGINSTATEBUFFER(Hardware, reserve, stateDelta, memory, reserveSize);

    if (Hardware->unifiedConst)
    {
        gctUINT32 shaderConfigData = Hardware->shaderStates.hints ?
                                        Hardware->shaderStates.hints->shaderConfigData : 0;

        shaderConfigData = ((((gctUINT32) (shaderConfigData)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4))) | (((gctUINT32) ((gctUINT32) ((Type != gcSHADER_TYPE_VERTEX)) & ((gctUINT32) ((((1 ? 4:4) - (0 ? 4:4) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 4:4) - (0 ? 4:4) + 1))))))) << (0 ? 4:4)));

        {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, 0x0218, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (0x0218) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, 0x0218, shaderConfigData);     gcmENDSTATEBATCH(reserve, memory);};
    }

    /* Set buffer location. */
    {    {    gcmASSERT(((memory - gcmUINT64_TO_TYPE(reserve->lastReserve, gctUINT32_PTR)) & 1) == 0);    gcmASSERT((gctUINT32)1 <= 1024);    gcmVERIFYLOADSTATEDONE(reserve);    gcmSTORELOADSTATE(reserve, memory, address, 1);    *memory++ = ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | (((gctUINT32) (0x01 & ((gctUINT32) ((((1 ? 31:27) - (0 ? 31:27) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 31:27) - (0 ? 31:27) + 1))))))) << (0 ? 31:27))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | (((gctUINT32) ((gctUINT32) (gcvFALSE) & ((gctUINT32) ((((1 ? 26:26) - (0 ? 26:26) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 26:26) - (0 ? 26:26) + 1))))))) << (0 ? 26:26))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | (((gctUINT32) ((gctUINT32) (1) & ((gctUINT32) ((((1 ? 25:16) - (0 ? 25:16) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 25:16) - (0 ? 25:16) + 1))))))) << (0 ? 25:16))) | ((((gctUINT32) (0)) & ~(((gctUINT32) (((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0))) | (((gctUINT32) ((gctUINT32) (address) & ((gctUINT32) ((((1 ? 15:0) - (0 ? 15:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 15:0) - (0 ? 15:0) + 1))))))) << (0 ? 15:0)));gcmSKIPSECUREUSER();};    gcmSETSTATEDATA(stateDelta, reserve, memory, gcvFALSE, address, Base + Offset );     gcmENDSTATEBATCH(reserve, memory);};

    /* Validate the state buffer. */
    gcmENDSTATEBUFFER(Hardware, reserve, memory, reserveSize);


    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#endif


