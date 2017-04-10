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
** Main interface header that outside world uses
*/

#ifndef __gc_vsc_drvi_interface_h_
#define __gc_vsc_drvi_interface_h_

#include "gc_vsc_precomp.h"

#if !DX_SHADER
#include "drvi/gc_vsc_drvi_shader_profile.h"
#include "drvi/gc_vsc_drvi_program_profile.h"
#endif

BEGIN_EXTERN_C()

/* VSC hardware (chip) configuration that poses on (re)-compiling */
typedef struct _VSC_HW_CONFIG
{
    struct
    {
        gctUINT          hasHalti0              : 1;
        gctUINT          hasHalti1              : 1;
        gctUINT          hasHalti2              : 1;
        gctUINT          hasSignFloorCeil       : 1;
        gctUINT          hasSqrtTrig            : 1;
        gctUINT          hasNewSinCosLogDiv     : 1;
        gctUINT          canBranchOnImm         : 1;
        gctUINT          supportDual16          : 1;
        gctUINT          hasBugFix8             : 1;
        gctUINT          hasBugFix10            : 1;
        gctUINT          hasBugFix11            : 1;
        gctUINT          hasSHEnhance2          : 1;
        gctUINT          hasMediumPrecision     : 1;
        gctUINT          hasInstCache           : 1;
        gctUINT          instBufferUnified      : 1;
        gctUINT          constRegFileUnified    : 1;
        gctUINT          bigEndianMI            : 1;
        gctUINT          raPushPosW             : 1;
        gctUINT          vtxInstanceIdAsAttr    : 1;
        gctUINT          vtxInstanceIdAsInteger : 1;

        /* Followings will be removed after shader programming is removed out of VSC */
        gctUINT          hasSHEnhance3          : 1;
        gctUINT          rtneRoundingEnabled    : 1;
        gctUINT          hasThreadWalkerInPS    : 1;

        gctUINT          reserved               : 9;
    } hwFeatureFlags;

    gctUINT              chipModel;
    gctUINT              chipRevision;
    gctUINT              maxCoreCount;
    gctUINT              maxThreadCountPerCore;
    gctUINT              maxVaryingCount;
    gctUINT              maxAttributeCount;
    gctUINT              maxGPRCountPerThread;
    gctUINT              maxHwNativeTotalInstCount;
    gctUINT              maxTotalInstCount;
    gctUINT              maxVSInstCount;
    gctUINT              maxPSInstCount;
    gctUINT              vsInstBase;
    gctUINT              psInstBase;
    gctUINT              maxHwNativeTotalConstRegCount;
    gctUINT              maxTotalConstRegCount;
    gctUINT              maxVSConstRegCount;
    gctUINT              maxPSConstRegCount;
    gctUINT              vsConstRegBase;
    gctUINT              psConstRegBase;
    gctUINT              vsSamplerBase;
    gctUINT              psSamplerBase;
    gctUINT              maxVSSamplerCount;
    gctUINT              maxPSSamplerCount;

    /* Followings will be removed after shader programming is removed out of VSC */
    gctUINT              vertexOutputBufferSize;
    gctUINT              vertexCacheSize;
    gctUINT              ctxStateCount;
}VSC_HW_CONFIG, *PVSC_HW_CONFIG;

/* VSC compiler configuration */
typedef struct _VSC_COMPILER_CONFIG
{
    gceAPI                clientAPI;
    PVSC_HW_CONFIG        hwCfg;
    gctUINT               cFlags;
    gctUINT64             optFlags;
}VSC_COMPILER_CONFIG, *PVSC_COMPILER_CONFIG;

/* VSC recompiler configuration */
typedef struct _VSC_RECOMPILER_CONFIG
{
    gceAPI                clientAPI;
    PVSC_HW_CONFIG        hwCfg;
    gctUINT               rcFlags;
}VSC_RECOMPILER_CONFIG, *PVSC_RECOMPILER_CONFIG;

/* Shader compiler parameter for VSC compiler entrance. It is incompleted, only for future
   use to generate SEP. See my SEP design. DON'T USE CURRENTLY!!!! */
typedef struct _VSC_SHADER_COMPILER_PARAM
{
    VSC_COMPILER_CONFIG   cfg;
}VSC_SHADER_COMPILER_PARAM, *PVSC_SHADER_COMPILER_PARAM;

/* Program linker parameter for VSC linker entrance. It is incompleted, only for future use
   to generate PEP. See my PEP design. Functions use VSC_PROGRAM_LINKER_PARAM may call funcs
   that use VSC_SHADER_COMPILER_PARAM due to program is conssisted of multiple shaders. DON'T
   USE CURRENTLY!!!! */
typedef struct _VSC_PROGRAM_LINKER_PARAM
{
    VSC_COMPILER_CONFIG   cfg;
}VSC_PROGRAM_LINKER_PARAM, *PVSC_PROGRAM_LINKER_PARAM;

/* Shader recompiler parameter for VSC recompiler entrance. It is incompleted, only for future
   use to generate SEI. See my SEI design. DON'T USE CURRENTLY!!!! */
typedef struct _VSC_SHADER_RECOMPILER_PARAM
{
    VSC_RECOMPILER_CONFIG cfg;
}VSC_SHADER_RECOMPILER_PARAM, *PVSC_SHADER_RECOMPILER_PARAM;

typedef struct _VIR_SHADER VIR_Shader;

/* !!!!!!!!!! A temp interface that works for current ugent request since we cut VIR path in old
   state-generator function. It must be refined to normal interface if we move VIR path forward
   to first stage of BE!!!!!!! */
gceSTATUS vscCompileShader(VIR_Shader* pShader);

END_EXTERN_C();

/* It will be fully removed after VIR totally replaces of gcSL */
#include "old_impl/gc_vsc_old_drvi_interface.h"

#endif /*__gc_vsc_drvi_interface_h_ */


