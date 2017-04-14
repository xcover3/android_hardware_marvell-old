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


#ifndef __gc_hal_user_h_
#define __gc_hal_user_h_

#include "gc_hal.h"
#include "gc_hal_driver.h"
#include "gc_hal_enum.h"
#include "gc_hal_dump.h"
#include "gc_hal_base.h"
#include "gc_hal_raster.h"
#include "gc_hal_user_math.h"
#include "gc_hal_user_os_memory.h"
#include "gc_hal_user_debug.h"

#if gcdENABLE_VG
#include "gc_hal_vg.h"
#include "gc_hal_user_vg.h"
#endif

#if gcdENABLE_3D
#include "gc_hal_engine.h"
#include "gc_vsc_drvi_interface.h"
#include "gc_hal_user_shader.h"
#endif

#define OPT_VERTEX_ARRAY                  1

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
********************************* Private Macro ********************************
\******************************************************************************/
#define gcmGETHARDWARE(Hardware) \
{ \
    if (Hardware == gcvNULL)  \
    {\
        gcsTLS_PTR __tls__; \
        \
        gcmONERROR(gcoOS_GetTLS(&__tls__)); \
        \
        if (__tls__->currentType == gcvHARDWARE_2D \
            && gcvSTATUS_TRUE == gcoHAL_QuerySeparated2D(gcvNULL) \
            && gcvSTATUS_TRUE == gcoHAL_Is3DAvailable(gcvNULL) \
            ) \
        { \
            if (__tls__->hardware2D == gcvNULL) \
            { \
                gcmONERROR(gcoHARDWARE_Construct(gcPLS.hal, gcvTRUE, &__tls__->hardware2D)); \
                gcmTRACE_ZONE( \
                    gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
                    "%s(%d): hardware object 0x%08X constructed.\n", \
                    __FUNCTION__, __LINE__, __tls__->hardware2D \
                    ); \
            } \
            \
            Hardware = __tls__->hardware2D; \
        } \
        else \
        if (__tls__->currentType == gcvHARDWARE_VG) \
        {\
            Hardware = gcvNULL;\
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);\
        }\
        else \
        { \
            if (__tls__->defaultHardware == gcvNULL) \
            { \
                gcmONERROR(gcoHARDWARE_Construct(gcPLS.hal, gcvTRUE, &__tls__->defaultHardware)); \
                \
                if (__tls__->currentType != gcvHARDWARE_2D) \
                { \
                    gcmONERROR(gcoHARDWARE_SelectPipe(__tls__->defaultHardware, gcvPIPE_3D, gcvNULL)); \
                    gcmONERROR(gcoHARDWARE_InvalidateCache(__tls__->defaultHardware, gcvTRUE)); \
                } \
                \
                gcmTRACE_ZONE( \
                    gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
                    "%s(%d): hardware object 0x%08X constructed.\n", \
                    __FUNCTION__, __LINE__, __tls__->defaultHardware \
                    ); \
            } \
            \
            if (__tls__->currentHardware == gcvNULL)\
            {\
               __tls__->currentHardware = __tls__->defaultHardware; \
            }\
            Hardware = __tls__->currentHardware; \
            \
        } \
        gcmTRACE_ZONE( \
                    gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
                    "%s(%d): TLS current hardware object is acquired.\n", \
                    __FUNCTION__, __LINE__\
                    ); \
    } \
}

#define gcmGETVGHARDWARE(Hardware) \
{ \
    gcsTLS_PTR __tls__; \
    gceSTATUS verifyStatus; \
    \
    verifyStatus = gcoOS_GetTLS(&__tls__); \
    if (gcmIS_ERROR(verifyStatus)) \
    { \
        gcmFOOTER_ARG("status = %d", verifyStatus); \
        return verifyStatus;                  \
    }                                   \
    \
    if (__tls__->vg == gcvNULL) \
    { \
        verifyStatus = gcoVGHARDWARE_Construct(gcPLS.hal, &__tls__->vg); \
        if (gcmIS_ERROR(verifyStatus))            \
        {                                   \
            gcmFOOTER_ARG("status = %d", verifyStatus); \
            return verifyStatus;                  \
        }                                   \
        \
        gcmTRACE_ZONE( \
            gcvLEVEL_VERBOSE, gcvZONE_HARDWARE, \
            "%s(%d): hardware object 0x%08X constructed.\n", \
            __FUNCTION__, __LINE__, __tls__->vg \
            ); \
    } \
    \
    Hardware = __tls__->vg; \
}

#define gcmGETCURRENTHARDWARE(hw) \
{ \
    gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL, &hw));\
    gcmASSERT(hw != gcvHARDWARE_INVALID);\
}


/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/

#if gcdENABLE_3D
/* gco3D object. */
typedef struct _gco3D
{
    /* Object. */
    gcsOBJECT                   object;

    /* Render target. */
    gcoSURF                     mRT[gcdMAX_DRAW_BUFFERS];
    gctPOINTER                  mRTMemory[gcdMAX_DRAW_BUFFERS];
    gctUINT32                   mRTOffset[gcdMAX_DRAW_BUFFERS];

    /* Depth buffer. */
    gcoSURF                     depth;
    gctPOINTER                  depthMemory;
    gctUINT32                   depthOffset;

    /* Clear color. */
    gctBOOL                     clearColorDirty;
    gceVALUE_TYPE               clearColorType;
    gcuVALUE                    clearColorRed;
    gcuVALUE                    clearColorGreen;
    gcuVALUE                    clearColorBlue;
    gcuVALUE                    clearColorAlpha;

    /* Clear depth value. */
    gctBOOL                     clearDepthDirty;
    gceVALUE_TYPE               clearDepthType;
    gcuVALUE                    clearDepth;

    /* Clear stencil value. */
    gctBOOL                     clearStencilDirty;
    gctUINT                     clearStencil;

    /* API type. */
    gceAPI                      apiType;

    gctBOOL                     wClipEnable;

    gctBOOL                     mRTtileStatus;


    /* Hardware object for this 3D engine */
    gcoHARDWARE                 hardware;
}_gco3D;

/******************************************************************************\
*******************************Clear Range of HZ ******************************
\******************************************************************************/
typedef enum _gceHZCLEAR_RANGE
{
    gcvHZCLEAR_ALL   = 0x1,
    gcvHZCLEAR_RECT  = 0x2,
}
gceHZCLEAR_TYPE;

/******************************************************************************\
*******************************Tile status type  ******************************
\******************************************************************************/
typedef enum _gceTILESTATUS_TYPE
{
    gcvTILESTATUS_COLOR = 0x1,
    gcvTILESTATUS_DEPTH = 0x2,
}
gceTILESTATUS_TYPE;

/******************************************************************************\
***************************** gcoSAMPLER Structure *****************************
\******************************************************************************/

/* Sampler structure. */
typedef struct _gcsSAMPLER
{
    gctUINT32               width;
    gctUINT32               height;
    gctUINT32               depth;
    gctUINT32               faces;

    gceTEXTURE_TYPE         texType;

    gceSURF_FORMAT          format;
    gctBOOL                 unsizedDepthTexture;
    gceTILING               tiling;

    gctBOOL                 endianHint;

    gctUINT32               lodNum;
    gctUINT32               baseLod;
    gctUINT32               lodAddr[30];
    gctUINT32               lodStride[30];

    gcsTEXTURE_PTR          textureInfo;

    gceSURF_ALIGNMENT       hAlignment;
    gceSURF_ADDRESSING      addressing;

    gctBOOL                 hasTileStatus;
    gctUINT32               tileStatusAddr;
    gctUINT32               tileStatusClearValue;
    gctUINT32               tileStatusClearValueUpper;
    gctBOOL                 compressed;
    gctINT32                compressedFormat;

    gctUINT32               astcSize[30];
    gctUINT32               astcSRGB[30];

    gcsSURF_FORMAT_INFO_PTR formatInfo;

    gctBOOL                 filterable;
}
gcsSAMPLER;

typedef gcsSAMPLER * gcsSAMPLER_PTR;
#endif

/******************************************************************************\
***************************** gcsSAMPLES Structure *****************************
\******************************************************************************/

typedef struct _gcsSAMPLES
{
    gctUINT8 x;
    gctUINT8 y;
}
gcsSAMPLES;

#if gcdENABLE_3D
typedef struct _gcsFAST_FLUSH_UNIFORM
{
    gcUNIFORM           halUniform[2];
    gctUINT32           physicalAddress[2];

    gctUINT32           columns;
    gctUINT32           rows;

    gctSIZE_T           arrayStride;
    gctSIZE_T           matrixStride;

    gctPOINTER          data;
    gctBOOL             dirty;
}
gcsFAST_FLUSH_UNIFORM;

typedef struct _gcsFAST_FLUSH_VERTEX_ARRAY
{
    gctINT              size;
    gctINT              stride;
    gctPOINTER          pointer;
    gctUINT             divisor;
}
gcsFAST_FLUSH_VERTEX_ARRAY;

typedef struct _gcsFAST_FLUSH
{
    /* Uniform */
    gctBOOL                         programValid;
    gctINT                          userDefUniformCount;
    gcsFAST_FLUSH_UNIFORM           uniforms[9];

    /* Stream */
    gctUINT                         vsInputMask;
    gctUINT                         vertexArrayEnable;
    gcsFAST_FLUSH_VERTEX_ARRAY      attribute[gcdATTRIBUTE_COUNT];
    gcoBUFOBJ                       boundObjInfo[gcdATTRIBUTE_COUNT];
    gctUINT                         attribLinkage[gcdATTRIBUTE_COUNT];

    /* Index */
    gcoBUFOBJ                       bufObj;
    gctPOINTER                      indices;
    gctUINT32                       indexFormat;

    /* Alpha */
    gctBOOL                         blend;
    gctUINT32                       color;
    gceBLEND_MODE                   modeAlpha;
    gceBLEND_MODE                   modeColor;
    gceBLEND_FUNCTION               srcFuncColor;
    gceBLEND_FUNCTION               srcFuncAlpha;
    gceBLEND_FUNCTION               trgFuncColor;
    gceBLEND_FUNCTION               trgFuncAlpha;

    /* Depth compare */
    gceDEPTH_MODE                   depthMode;
    gceCOMPARE                      depthInfoCompare;
    gceCOMPARE                      compare;

    /* Draw index info */
    gctUINT                         drawCount;
    gctINT                          indexCount;
    gctINT                          instanceCount;
    gctBOOL                         hasHalti;
}
gcsFAST_FLUSH;
#endif /* gcdENABLE_3D */
/******************************************************************************\
****************************** Object Declarations *****************************
\******************************************************************************/

typedef struct _gcoBUFFER *             gcoBUFFER;
typedef struct _gcs2D_State*            gcs2D_State_PTR;

#if gcdENABLE_3D

/* Internal dynamic index buffers. */
gceSTATUS
gcoINDEX_UploadDynamicEx(
    IN gcoINDEX Index,
    IN gceINDEX_TYPE IndexType,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL ConvertToIndexedTriangleList
    );

gceSTATUS
gcoINDEX_BindDynamic(
    IN gcoINDEX Index,
    IN gceINDEX_TYPE Type
    );

gceSTATUS
gcoINDEX_GetDynamicIndexRange(
    IN gcoINDEX Index,
    OUT gctUINT32 * MinimumIndex,
    OUT gctUINT32 * MaximumIndex
    );

/******************************************************************************\
********************************** gco3D Object ********************************
\******************************************************************************/

gceSTATUS
gco3D_ClearHzTileStatus(
    IN gco3D Engine,
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_NODE_PTR TileStatus
    );

#endif /* gcdENABLE_3D */

/******************************************************************************\
******************************* gcoHARDWARE Object ******************************
\******************************************************************************/

/*----------------------------------------------------------------------------*/
/*----------------------------- gcoHARDWARE Common ----------------------------*/

/* Construct a new gcoHARDWARE object. */
gceSTATUS
gcoHARDWARE_Construct(
    IN gcoHAL Hal,
    IN gctBOOL ThreadDefault,
    OUT gcoHARDWARE * Hardware
    );

/* Destroy an gcoHARDWARE object. */
gceSTATUS
gcoHARDWARE_Destroy(
    IN gcoHARDWARE Hardware,
    IN gctBOOL  ThreadDefault
    );

/* Query the identity of the hardware. */
gceSTATUS
gcoHARDWARE_QueryChipIdentity(
    IN  gcoHARDWARE Hardware,
    OUT gceCHIPMODEL* ChipModel,
    OUT gctUINT32* ChipRevision,
    OUT gctUINT32* ChipFeatures,
    OUT gctUINT32* ChipMinorFeatures,
    OUT gctUINT32* ChipMinorFeatures1,
    OUT gctUINT32* ChipMinorFeatures2,
    OUT gctUINT32* ChipMinorFeatures3,
    OUT gctUINT32* ChipMinorFeatures4,
    OUT gctUINT32* ChipMinorFeatures5
    );

/* Verify whether the specified feature is available in hardware. */
gceSTATUS
gcoHARDWARE_IsFeatureAvailable(
    IN gcoHARDWARE Hardware,
    IN gceFEATURE Feature
    );

gceSTATUS
gcoHARDWARE_IsSwwaNeeded(
    IN gcoHARDWARE Hardware,
    IN gceSWWA Swwa
    );

/* Query command buffer requirements. */
gceSTATUS
gcoHARDWARE_QueryCommandBuffer(
    IN gcoHARDWARE Hardware,
    OUT gctUINT32 * Alignment,
    OUT gctUINT32 * ReservedHead,
    OUT gctUINT32 * ReservedTail,
    OUT gceCMDBUF_SOURCE *Source
    );

/* Select a graphics pipe. */
gceSTATUS
gcoHARDWARE_SelectPipe(
    IN gcoHARDWARE Hardware,
    IN gcePIPE_SELECT Pipe,
    INOUT gctPOINTER *Memory
    );

/* Flush the current graphics pipe. */
gceSTATUS
gcoHARDWARE_FlushPipe(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

/* Invalidate the cache. */
gceSTATUS
gcoHARDWARE_InvalidateCache(
    IN gcoHARDWARE Hardware,
    IN gctBOOL KickOff
    );

#if gcdENABLE_3D
/* Set patch ID */
gceSTATUS
gcoHARDWARE_SetPatchID(
    IN gcoHARDWARE Hardware,
    IN gcePATCH_ID   PatchID
    );

/* Get patch ID */
gceSTATUS
gcoHARDWARE_GetPatchID(
    IN gcoHARDWARE Hardware,
    OUT gcePATCH_ID *PatchID
    );

/* Send semaphore down the current pipe. */
gceSTATUS
gcoHARDWARE_Semaphore(
    IN gcoHARDWARE Hardware,
    IN gceWHERE From,
    IN gceWHERE To,
    IN gceHOW How,
    INOUT gctPOINTER *Memory
    );
#endif

/* Load a number of load states. */
gceSTATUS
gcoHARDWARE_LoadState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER States
    );

/* Load one ctrl state, which will not be put in state buffer */
gceSTATUS
gcoHARDWARE_LoadCtrlState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data

    );

#if gcdENABLE_3D
/* Load a number of load states in fixed-point (3D pipe). */
gceSTATUS
gcoHARDWARE_LoadStateX(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER States
    );
#endif

/* Commit the current command buffer. */
gceSTATUS
gcoHARDWARE_Commit(
    IN gcoHARDWARE Hardware
    );

/* Stall the pipe. */
gceSTATUS
gcoHARDWARE_Stall(
    IN gcoHARDWARE Hardware
    );

#if gcdMULTI_GPU

gceSTATUS
gcoHARDWARE_SetChipEnable(
    IN gcoHARDWARE Hardware,
    IN gceCORE_3D_MASK ChipEnable
    );

gceSTATUS
gcoHARDWARE_GetChipEnable(
    IN gcoHARDWARE Hardware,
    OUT gceCORE_3D_MASK *ChipEnable
    );

gceSTATUS
gcoHARDWARE_SetMultiGPUMode(
    IN gcoHARDWARE Hardware,
    IN gceMULTI_GPU_MODE MultiGPUMode
    );

gceSTATUS
gcoHARDWARE_GetMultiGPUMode(
    IN gcoHARDWARE Hardware,
    OUT gceMULTI_GPU_MODE *MultiGPUMode
    );

gceSTATUS
gcoHARDWARE_MultiGPUSync(
    IN gcoHARDWARE Hardware,
    IN gctBOOL ReserveBuffer,
    OUT gctUINT32_PTR *Memory
);

#endif

#if gcdENABLE_3D
/* Load the vertex and pixel shaders. */
gceSTATUS
gcoHARDWARE_LoadShaders(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    );

gceSTATUS
gcoHARDWARE_LoadKernel(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T StateBufferSize,
    IN gctPOINTER StateBuffer,
    IN gcsHINT_PTR Hints
    );

/* Resolve. */
gceSTATUS
gcoHARDWARE_ResolveRect(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gctBOOL      yInverted
    );

gceSTATUS
gcoHARDWARE_IsHWResolveable(
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );

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
    );

/* Preserve pixels from source surface. */
gceSTATUS
gcoHARDWARE_PreserveRects(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsRECT Rects[],
    IN gctUINT RectCount
    );
#endif /* gcdENABLE_3D */

/* Query tile sizes. */
gceSTATUS
gcoHARDWARE_QueryTileSize(
    OUT gctINT32 * TileWidth2D,
    OUT gctINT32 * TileHeight2D,
    OUT gctINT32 * TileWidth3D,
    OUT gctINT32 * TileHeight3D,
    OUT gctUINT32 * StrideAlignment
    );

#if gcdENABLE_3D

/* Get tile status sizes for a surface. */
gceSTATUS
gcoHARDWARE_QueryTileStatus(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctSIZE_T Bytes,
    OUT gctSIZE_T_PTR Size,
    OUT gctUINT_PTR Alignment,
    OUT gctUINT32_PTR Filler
    );

/* Disable (Turn off) tile status hardware programming. */
gceSTATUS
gcoHARDWARE_DisableHardwareTileStatus(
    IN gcoHARDWARE Hardware,
    IN gceTILESTATUS_TYPE Type,
    IN gctUINT  RtIndex
    );

/*
 * Enable (turn on) tile status for a surface.
 *
 * If surface tileStatusDisabled or no tile status buffer
 *   then DISABLE (turn off) hardware tile status
 * Else
 *   If surface is current
 *     then ENABLE (turn on) hardware tile status on surface
 *   Else
 *     do not need turn on, skip
 */
gceSTATUS
gcoHARDWARE_EnableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_NODE_PTR HzTileStatus,
    IN gctUINT  RtIndex
    );

/*
 * Flush and disable (turn off) tile status for a surface.
 * Always uses RT0 if it's RT.
 *
 * If CpuAccess,
 *   then decompress the suface either use resolve or fc fill.
 *   (Auto switch surface to current and restore after decompress)
 * Flush the tile status cache after.
 * DISABLE (turn off) hardware tile status.
 */
gceSTATUS
gcoHARDWARE_DisableTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CpuAccess
    );

/*
 * Flush tile status cache.
 *
 * If Decompress,
 *   then decompress the surface either use resolve or fc fill.
 *   (Auto switch surface to current and restore after decompress)
 * FLUSH the tile status cache after.
 */
gceSTATUS
gcoHARDWARE_FlushTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Decompress
    );

/*
 * Fill surface from tile status cache.
 * Auto switch current to the surface and restore after.
 */
gceSTATUS
gcoHARDWARE_FillFromTileStatus(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    );

/* Query resolve alignment requirement to resolve this surface */
gceSTATUS gcoHARDWARE_GetSurfaceResolveAlignment(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    OUT gctUINT *originX,
    OUT gctUINT *originY,
    OUT gctUINT *sizeX,
    OUT gctUINT *sizeY
    );

#endif /* gcdENABLE_3D */

/* Lock a surface. */
gceSTATUS
gcoHARDWARE_Lock(
    IN gcsSURF_NODE_PTR Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    );

/* Unlock a surface. */
gceSTATUS
gcoHARDWARE_Unlock(
    IN gcsSURF_NODE_PTR Node,
    IN gceSURF_TYPE Type
    );

/* Call kernel for event. */
gceSTATUS
gcoHARDWARE_CallEvent(
    IN gcoHARDWARE Hardware,
    IN OUT gcsHAL_INTERFACE * Interface
    );

/* Schedule destruction for the specified video memory node. */
gceSTATUS
gcoHARDWARE_ScheduleVideoMemory(
    IN gcsSURF_NODE_PTR Node
    );

/* Allocate a temporary surface with specified parameters. */
gceSTATUS
gcoHARDWARE_AllocateTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gcsSURF_FORMAT_INFO_PTR Format,
    IN gceSURF_TYPE Type,
    IN gceSURF_FLAG Flags
    );

/* Free the temporary surface. */
gceSTATUS
gcoHARDWARE_FreeTemporarySurface(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Synchronized
    );

/* Get a 2D temporary surface with specified parameters. */
gceSTATUS
gcoHARDWARE_Get2DTempSurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gceSURF_FORMAT Format,
    IN gceSURF_FLAG Flags,
    OUT gcsSURF_INFO_PTR *SurfInfo
    );

/* Put back the 2D temporary surface from gcoHARDWARE_Get2DTempSurface. */
gceSTATUS
gcoHARDWARE_Put2DTempSurface(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo
    );

#if gcdENABLE_3D
/* Copy a rectangular area with format conversion. */
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
    );

/* Enable or disable anti-aliasing. */
gceSTATUS
gcoHARDWARE_SetAntiAlias(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

/* Write data into the command buffer. */
gceSTATUS
gcoHARDWARE_WriteBuffer(
    IN gcoHARDWARE Hardware,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );
#endif /* gcdENABLE_3D */

/* Convert RGB8 color value to YUV color space. */
void gcoHARDWARE_RGB2YUV(
    gctUINT8 R,
    gctUINT8 G,
    gctUINT8 B,
    gctUINT8_PTR Y,
    gctUINT8_PTR U,
    gctUINT8_PTR V
    );

/* Convert YUV color value to RGB8 color space. */
void gcoHARDWARE_YUV2RGB(
    gctUINT8 Y,
    gctUINT8 U,
    gctUINT8 V,
    gctUINT8_PTR R,
    gctUINT8_PTR G,
    gctUINT8_PTR B
    );

/* Convert an API format. */
gceSTATUS
gcoHARDWARE_ConvertFormat(
    IN gceSURF_FORMAT Format,
    OUT gctUINT32 * BitsPerPixel,
    OUT gctUINT32 * BytesPerTile
    );

/* Align size to tile boundary. */
gceSTATUS
gcoHARDWARE_AlignToTile(
    IN gcoHARDWARE Hardware,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height,
    IN gctUINT32 Depth,
    OUT gceTILING * Tiling,
    OUT gctBOOL_PTR SuperTiled
    );

/* Align size compatible for all core in hardware. */
gceSTATUS
gcoHARDWARE_AlignToTileCompatible(
    IN gcoHARDWARE Hardware,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height,
    IN gctUINT32 Depth,
    OUT gceTILING * Tiling,
    OUT gctBOOL_PTR SuperTiled
    );

gceSTATUS
gcoHARDWARE_AlignResolveRect(
    IN gcsSURF_INFO_PTR SurfInfo,
    IN gcsPOINT_PTR RectOrigin,
    IN gcsPOINT_PTR RectSize,
    OUT gcsPOINT_PTR AlignedOrigin,
    OUT gcsPOINT_PTR AlignedSize
    );

gceSTATUS
gcoHARDWARE_QueryTileAlignment(
    IN gcoHARDWARE Hardware,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    OUT gceSURF_ALIGNMENT * HAlignment,
    OUT gceSURF_ALIGNMENT * VAlignment
    );

gceSTATUS
gcoHARDWARE_SetDepthOnly(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

/*----------------------------------------------------------------------------*/
/*------------------------------- gcoHARDWARE 2D ------------------------------*/

/* Verifies whether 2D engine is available. */
gceSTATUS
gcoHARDWARE_Is2DAvailable(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_Query2DSurfaceAllocationInfo(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo,
    IN OUT gctUINT* Bytes,
    IN OUT gctUINT* Alignment
    );

/* Check whether primitive restart is enabled. */
gceSTATUS
gcoHARDWARE_IsPrimitiveRestart(
    IN gcoHARDWARE Hardware
    );

/* Check whether hardware need to be programmed as multi pipes */
gceSTATUS
gcoHARDWARE_IsMultiPipes(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_Query3DCoreCount(
    IN gcoHARDWARE Hardware,
    OUT gctUINT32 *Count
    );

/* Translate API destination color format to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateDestinationFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT APIValue,
    IN gctBOOL EnableXRGB,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    );

#if gcdENABLE_3D
gceSTATUS
gcoHARDWARE_SetStream(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Index,
    IN gctUINT32 Address,
    IN gctUINT32 Stride
    );

gceSTATUS
gcoHARDWARE_SetAttributes(
    IN gcoHARDWARE Hardware,
    IN gcsVERTEX_ATTRIBUTES_PTR Attributes,
    IN gctUINT32 AttributeCount
    );

/*----------------------------------------------------------------------------*/
/*----------------------- gcoHARDWARE Fragment Processor ---------------------*/

/* Set the fragment processor configuration. */
gceSTATUS
gcoHARDWARE_SetFragmentConfiguration(
    IN gcoHARDWARE Hardware,
    IN gctBOOL ColorFromStream,
    IN gctBOOL EnableFog,
    IN gctBOOL EnableSmoothPoint,
    IN gctUINT32 ClipPlanes
    );

/* Enable/disable texture stage operation. */
gceSTATUS
gcoHARDWARE_EnableTextureStage(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gctBOOL Enable
    );

/* Program the channel enable masks for the color texture function. */
gceSTATUS
gcoHARDWARE_SetTextureColorMask(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gctBOOL ColorEnabled,
    IN gctBOOL AlphaEnabled
    );

/* Program the channel enable masks for the alpha texture function. */
gceSTATUS
gcoHARDWARE_SetTextureAlphaMask(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gctBOOL ColorEnabled,
    IN gctBOOL AlphaEnabled
    );

/* Program the constant fragment color. */
gceSTATUS
gcoHARDWARE_SetFragmentColorX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetFragmentColorF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Program the constant fog color. */
gceSTATUS
gcoHARDWARE_SetFogColorX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetFogColorF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Program the constant texture color. */
gceSTATUS
gcoHARDWARE_SetTetxureColorX(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetTetxureColorF(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

/* Configure color texture function. */
gceSTATUS
gcoHARDWARE_SetColorTextureFunction(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gceTEXTURE_FUNCTION Function,
    IN gceTEXTURE_SOURCE Source0,
    IN gceTEXTURE_CHANNEL Channel0,
    IN gceTEXTURE_SOURCE Source1,
    IN gceTEXTURE_CHANNEL Channel1,
    IN gceTEXTURE_SOURCE Source2,
    IN gceTEXTURE_CHANNEL Channel2,
    IN gctINT Scale
    );

/* Configure alpha texture function. */
gceSTATUS
gcoHARDWARE_SetAlphaTextureFunction(
    IN gcoHARDWARE Hardware,
    IN gctINT Stage,
    IN gceTEXTURE_FUNCTION Function,
    IN gceTEXTURE_SOURCE Source0,
    IN gceTEXTURE_CHANNEL Channel0,
    IN gceTEXTURE_SOURCE Source1,
    IN gceTEXTURE_CHANNEL Channel1,
    IN gceTEXTURE_SOURCE Source2,
    IN gceTEXTURE_CHANNEL Channel2,
    IN gctINT Scale
    );

/* Flush the evrtex caches. */
gceSTATUS
gcoHARDWARE_FlushVertex(
    IN gcoHARDWARE Hardware
    );

#if gcdUSE_WCLIP_PATCH
gceSTATUS
gcoHARDWARE_SetWClipEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetWPlaneLimit(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Value
    );
#endif

gceSTATUS
gcoHARDWARE_SetRTNERounding(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

/* Draw a number of primitives. */
gceSTATUS
gcoHARDWARE_DrawPrimitives(
    IN gcoHARDWARE Hardware,
    IN gcePRIMITIVE Type,
    IN gctINT StartVertex,
    IN gctSIZE_T PrimitiveCount
    );

gceSTATUS
gcoHARDWARE_DrawInstancedPrimitives(
    IN gcoHARDWARE Hardware,
    IN gctBOOL DrawIndex,
    IN gcePRIMITIVE Type,
    IN gctINT StartVertex,
    IN gctINT StartIndex,
    IN gctSIZE_T PrimitiveCount,
    IN gctSIZE_T VertexCount,
    IN gctSIZE_T InstanceCount
    );

gceSTATUS
gcoHARDWARE_DrawPrimitivesCount(
    IN gcoHARDWARE Hardware,
    IN gcePRIMITIVE Type,
    IN gctINT* StartVertex,
    IN gctSIZE_T* VertexCount,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of primitives using offsets. */
gceSTATUS
gcoHARDWARE_DrawPrimitivesOffset(
    IN gcePRIMITIVE Type,
    IN gctINT32 StartOffset,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of indexed primitives. */
gceSTATUS
gcoHARDWARE_DrawIndexedPrimitives(
    IN gcoHARDWARE Hardware,
    IN gcePRIMITIVE Type,
    IN gctINT BaseVertex,
    IN gctINT StartIndex,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw a number of indexed primitives using offsets. */
gceSTATUS
gcoHARDWARE_DrawIndexedPrimitivesOffset(
    IN gcePRIMITIVE Type,
    IN gctINT32 BaseOffset,
    IN gctINT32 StartOffset,
    IN gctSIZE_T PrimitiveCount
    );

/* Draw elements from a pattern */
gceSTATUS
gcoHARDWARE_DrawPattern(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo
    );

/* Flush the texture cache. */
gceSTATUS
gcoHARDWARE_FlushTexture(
    IN gcoHARDWARE Hardware,
    IN gctBOOL vsUsed
    );

/* Disable a specific texture sampler. */
gceSTATUS
gcoHARDWARE_DisableTextureSampler(
    IN gcoHARDWARE Hardware,
    IN gctINT Sampler
    );

/* Set sampler registers. */
gceSTATUS
gcoHARDWARE_BindTexture(
    IN gcoHARDWARE Hardware,
    IN gctINT Sampler,
    IN gcsSAMPLER_PTR SamplerInfo
    );

/* Query the index capabilities. */
gceSTATUS
gcoHARDWARE_QueryIndexCaps(
    IN  gcoHARDWARE Hardware,
    OUT gctBOOL * Index8,
    OUT gctBOOL * Index16,
    OUT gctBOOL * Index32,
    OUT gctUINT * MaxIndex
    );

/* Query the texture capabilities. */
gceSTATUS
gcoHARDWARE_QueryTextureCaps(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MaxDepth,
    OUT gctBOOL * Cubic,
    OUT gctBOOL * NonPowerOfTwo,
    OUT gctUINT * VertexSamplers,
    OUT gctUINT * PixelSamplers,
    OUT gctUINT * MaxAnisoValue
    );

/* Query the shader support. */
gceSTATUS
gcoHARDWARE_QueryShaderCaps(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT * UnifiedUniforms,
    OUT gctUINT * VertUniforms,
    OUT gctUINT * FragUniforms,
    OUT gctUINT * Varyings,
    OUT gctUINT * ShaderCoreCount,
    OUT gctUINT * ThreadCount,
    OUT gctUINT * VertInstructionCount,
    OUT gctUINT * FragInstructionCount
    );

/* Query the shader support. */
gceSTATUS
gcoHARDWARE_QueryShaderCapsEx(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT64 * LocalMemSize,
    OUT gctUINT * AddressBits,
    OUT gctUINT * GlobalMemCachelineSize,
    OUT gctUINT * GlobalMemCacheSize,
    OUT gctUINT * ClockFrequency
    );

/* Query the texture support. */
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
    OUT gctUINT * WidthAlignment,
    OUT gctUINT * HeightAlignment
    );

/* Query the stream capabilities. */
gceSTATUS
gcoHARDWARE_QueryStreamCaps(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT * MaxAttributes,
    OUT gctUINT * MaxStreamSize,
    OUT gctUINT * NumberOfStreams,
    OUT gctUINT * Alignment
    );

gceSTATUS
gcoHARDWARE_GetClosestTextureFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT InFormat,
    IN gceTEXTURE_TYPE texType,
    OUT gceSURF_FORMAT* OutFormat
    );

gceSTATUS
gcoHARDWARE_GetClosestRenderFormat(
    IN gceSURF_FORMAT InFormat,
    OUT gceSURF_FORMAT* OutFormat
    );

/* Upload data into a texture. */
gceSTATUS
gcoHARDWARE_UploadTexture(
    IN gcsSURF_INFO_PTR TargetInfo,
    IN gctUINT32 Offset,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctCONST_POINTER Memory,
    IN gctINT SourceStride,
    IN gceSURF_FORMAT SourceFormat
    );

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
    );

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
    );

gceSTATUS
gcoHARDWARE_ProgramTexture(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

/* Query if a surface is renderable or not. */
gceSTATUS
gcoHARDWARE_QuerySurfaceRenderable(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    );

#endif /* gcdENABLE_3D */

#if gcdENABLE_3D || gcdENABLE_VG
/* Query the target capabilities. */
gceSTATUS
gcoHARDWARE_QueryTargetCaps(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT * MaxWidth,
    OUT gctUINT * MaxHeight,
    OUT gctUINT * MultiTargetCount,
    OUT gctUINT * MaxSamples
    );
#endif

gceSTATUS
gcoHARDWARE_QueryFormat(
    IN gceSURF_FORMAT Format,
    OUT gcsSURF_FORMAT_INFO_PTR * Info
    );

/* Copy data into video memory. */
gceSTATUS
gcoHARDWARE_CopyData(
    IN gcsSURF_NODE_PTR Memory,
    IN gctSIZE_T Offset,
    IN gctCONST_POINTER Buffer,
    IN gctSIZE_T Bytes
    );

/* Sets the software 2D renderer force flag. */
gceSTATUS
gcoHARDWARE_UseSoftware2D(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

/* Clear one or more rectangular areas. */
gceSTATUS
gcoHARDWARE_Clear2D(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gctUINT32 RectCount,
    IN gcsRECT_PTR Rect
    );

/* Draw one or more Bresenham lines using solid color(s). */
gceSTATUS
gcoHARDWARE_Line2DEx(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gctUINT32 LineCount,
    IN gcsRECT_PTR Position,
    IN gctUINT32 ColorCount,
    IN gctUINT32_PTR Color32
    );

/* Determines the usage of 2D resources (source/pattern/destination). */
void
gcoHARDWARE_Get2DResourceUsage(
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN gce2D_TRANSPARENCY Transparency,
    OUT gctBOOL_PTR UseSource,
    OUT gctBOOL_PTR UsePattern,
    OUT gctBOOL_PTR UseDestination
    );

/* Translate SURF API transparency mode to PE 2.0 transparency values. */
gceSTATUS
gcoHARDWARE_TranslateSurfTransparency(
    IN gceSURF_TRANSPARENCY APIValue,
    OUT gce2D_TRANSPARENCY* srcTransparency,
    OUT gce2D_TRANSPARENCY* dstTransparency,
    OUT gce2D_TRANSPARENCY* patTransparency
    );

gceSTATUS
gcoHARDWARE_ColorPackToARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 Color,
    OUT gctUINT32_PTR Color32
    );

gceSTATUS
gcoHARDWARE_SetDither(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

/* Calculate stretch factor. */
gctUINT32
gcoHARDWARE_GetStretchFactor(
    IN gctBOOL GdiStretch,
    IN gctINT32 SrcSize,
    IN gctINT32 DestSize
    );

/* Calculate the stretch factors. */
gceSTATUS
gcoHARDWARE_GetStretchFactors(
    IN gctBOOL GdiStretch,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    OUT gctUINT32 * HorFactor,
    OUT gctUINT32 * VerFactor
    );

/* Start a DE command. */
gceSTATUS
gcoHARDWARE_StartDE(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN gctUINT32 SrcRectCount,
    IN gcsRECT_PTR SrcRect,
    IN gctUINT32 DestRectCount,
    IN gcsRECT_PTR DestRect
    );

/* Start a DE command to draw one or more Lines,
   with a common or individual color. */
gceSTATUS
gcoHARDWARE_StartDELine(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gce2D_COMMAND Command,
    IN gctUINT32 RectCount,
    IN gcsRECT_PTR DestRect,
    IN gctUINT32 ColorCount,
    IN gctUINT32_PTR Color32
    );

/* Start a DE command with a monochrome stream. */
gceSTATUS
gcoHARDWARE_StartDEStream(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsRECT_PTR DestRect,
    IN gctUINT32 StreamSize,
    OUT gctPOINTER * StreamBits
    );

/* Frees the temporary buffer allocated by filter blit operation. */
gceSTATUS
gcoHARDWARE_FreeFilterBuffer(
    IN gcoHARDWARE Hardware
    );

/* Split filter blit in specific case. */
gceSTATUS
gcoHARDWARE_SplitFilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    );

/* Filter blit. */
gceSTATUS
gcoHARDWARE_FilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    );

gceSTATUS gcoHARDWARE_SplitYUVFilterBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    );

gceSTATUS gcoHARDWARE_MultiPlanarYUVConvert(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DestSurface,
    IN gcsRECT_PTR DestRect
    );

/* Monochrome blit. */
gceSTATUS
gcoHARDWARE_MonoBlit(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gctPOINTER StreamBits,
    IN gcsPOINT_PTR StreamSize,
    IN gcsRECT_PTR StreamRect,
    IN gceSURF_MONOPACK SrcStreamPack,
    IN gceSURF_MONOPACK DestStreamPack,
    IN gcsRECT_PTR DestRect
    );

/* Set the GPU clock cycles, after which the idle 2D engine
   will trigger a flush. */
gceSTATUS
gcoHARDWARE_SetAutoFlushCycles(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Cycles
    );

gceSTATUS
gcoHARDWARE_Set2DHardware(
     IN gcoHARDWARE Hardware
     );

gceSTATUS
gcoHARDWARE_Get2DHardware(
     OUT gcoHARDWARE * Hardware
     );

#if gcdENABLE_3D
/*----------------------------------------------------------------------------*/
/*------------------------------- gcoHARDWARE 3D ------------------------------*/

typedef struct _gcs3DBLIT_INFO * gcs3DBLIT_INFO_PTR;
typedef struct _gcs3DBLIT_INFO
{
    gctUINT32   destAddress;
    gctUINT32   destTileStatusAddress;
    gctUINT32   clearValue[2];
    gctBOOL     useFastClear;
    gctUINT32   clearMask;
    gcsPOINT    *origin;
    gcsPOINT    *rect;
}
gcs3DBLIT_INFO;

gceSTATUS
gcoHARDWARE_BindIndex(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 HeadAddress,
    IN gctUINT32 TailAddress,
    IN gceINDEX_TYPE IndexType,
    IN gctSIZE_T Bytes
    );

/* Initialize the 3D hardware. */
gceSTATUS
gcoHARDWARE_Initialize3D(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_SetPSOutputMapping(
    IN gcoHARDWARE Hardware,
    IN gctINT32 * psOutputMapping
    );

gceSTATUS
gcoHARDWARE_SetRenderTarget(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 TargetIndex,
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 LayerIndex
    );

gceSTATUS
gcoHARDWARE_SetRenderTargetOffset(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 TargetIndex,
    IN gctUINT32 Offset
    );

gceSTATUS
gcoHARDWARE_SetDepthBuffer(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface
    );

gceSTATUS
gcoHARDWARE_SetDepthBufferOffset(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Offset
    );

#if gcdMULTI_GPU_AFFINITY
gceSTATUS
gcoHARDWARE_GetCurrentAPI(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT_PTR Api
    );
#endif

gceSTATUS
gcoHARDWARE_SetAPI(
    IN gcoHARDWARE Hardware,
    IN gceAPI Api
    );

gceSTATUS
gcoHARDWARE_GetAPI(
    IN gcoHARDWARE Hardware,
    OUT gceAPI * CurrentApi,
    OUT gceAPI * Api
    );

/* Resolve(3d) clear. */
gceSTATUS
gcoHARDWARE_Clear(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask
    );

/* Software clear. */
gceSTATUS
gcoHARDWARE_ClearSoftware(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gctUINT32 ClearValue,
    IN gctUINT32 ClearValueUpper,
    IN gctUINT8 ClearMask,
    IN gctUINT8 StencilWriteMask
    );

/* Append a TILE STATUS CLEAR command to a command queue. */
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
    );

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
    );

gceSTATUS
gcoHARDWARE_ComputeClearWindow(
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T Bytes,
    gctUINT32 *Width,
    gctUINT32 *Height
    );

gceSTATUS
gcoHARDWARE_SetViewport(
    IN gcoHARDWARE Hardware,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    );

gceSTATUS
gcoHARDWARE_SetScissors(
    IN gcoHARDWARE Hardware,
    IN gctINT32 Left,
    IN gctINT32 Top,
    IN gctINT32 Right,
    IN gctINT32 Bottom
    );

gceSTATUS
gcoHARDWARE_SetShading(
    IN gcoHARDWARE Hardware,
    IN gceSHADING Shading
    );

gceSTATUS
gcoHARDWARE_SetBlendEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enabled
    );

gceSTATUS
gcoHARDWARE_SetBlendFunctionSource(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendFunctionTarget(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_FUNCTION FunctionRGB,
    IN gceBLEND_FUNCTION FunctionAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendMode(
    IN gcoHARDWARE Hardware,
    IN gceBLEND_MODE ModeRGB,
    IN gceBLEND_MODE ModeAlpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Red,
    IN gctUINT8 Green,
    IN gctUINT8 Blue,
    IN gctUINT8 Alpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColorX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Red,
    IN gctFIXED_POINT Green,
    IN gctFIXED_POINT Blue,
    IN gctFIXED_POINT Alpha
    );

gceSTATUS
gcoHARDWARE_SetBlendColorF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    );

gceSTATUS
gcoHARDWARE_SetCulling(
    IN gcoHARDWARE Hardware,
    IN gceCULL Mode
    );

gceSTATUS
gcoHARDWARE_SetPointSizeEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetPointSprite(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetFill(
    IN gcoHARDWARE Hardware,
    IN gceFILL Mode
    );

gceSTATUS
gcoHARDWARE_SetDepthCompare(
    IN gcoHARDWARE Hardware,
    IN gceCOMPARE DepthCompare
    );

gceSTATUS
gcoHARDWARE_SetDepthWrite(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetDepthMode(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode
    );

gceSTATUS
gcoHARDWARE_SetDepthRangeX(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode,
    IN gctFIXED_POINT Near,
    IN gctFIXED_POINT Far
    );

gceSTATUS
gcoHARDWARE_SetDepthRangeF(
    IN gcoHARDWARE Hardware,
    IN gceDEPTH_MODE DepthMode,
    IN gctFLOAT Near,
    IN gctFLOAT Far
    );

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT DepthScale,
    IN gctFIXED_POINT DepthBias
    );

gceSTATUS
gcoHARDWARE_SetDepthScaleBiasF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT DepthScale,
    IN gctFLOAT DepthBias
    );

gceSTATUS
gcoHARDWARE_SetDepthPlaneF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Near,
    IN gctFLOAT Far
    );

gceSTATUS
gcoHARDWARE_SetColorWrite(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Enable
    );

gceSTATUS
gcoHARDWARE_SetEarlyDepth(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAllEarlyDepthModes(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Disable,
    IN gctBOOL DisableRAModify,
    IN gctBOOL DisableRAPassZ
    );

gceSTATUS
gcoHARDWARE_SwitchDynamicEarlyDepthMode(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_DisableDynamicEarlyDepthMode(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Disable
    );

gceSTATUS
gcoHARDWARE_SetStencilMode(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_MODE Mode
    );

gceSTATUS
gcoHARDWARE_SetStencilMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilMaskBack(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilWriteMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilWriteMaskBack(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Mask
    );

gceSTATUS
gcoHARDWARE_SetStencilReference(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Reference,
    IN gctBOOL  Front
    );

gceSTATUS
gcoHARDWARE_SetStencilCompare(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceCOMPARE Compare
    );

gceSTATUS
gcoHARDWARE_SetStencilPass(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilFail(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilDepthFail(
    IN gcoHARDWARE Hardware,
    IN gceSTENCIL_WHERE Where,
    IN gceSTENCIL_OPERATION Operation
    );

gceSTATUS
gcoHARDWARE_SetStencilAll(
    IN gcoHARDWARE Hardware,
    IN gcsSTENCIL_INFO_PTR Info
    );

gceSTATUS
gcoHARDWARE_SetAlphaTest(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAlphaCompare(
    IN gcoHARDWARE Hardware,
    IN gceCOMPARE Compare
    );

gceSTATUS
gcoHARDWARE_SetAlphaReference(
    IN gcoHARDWARE Hardware,
    IN gctUINT8 Reference,
    IN gctFLOAT FloatReference
    );

gceSTATUS
gcoHARDWARE_SetAlphaReferenceX(
    IN gcoHARDWARE Hardware,
    IN gctFIXED_POINT Reference
    );

gceSTATUS
gcoHARDWARE_SetAlphaReferenceF(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Reference
    );

gceSTATUS
gcoHARDWARE_SetAlphaAll(
    IN gcoHARDWARE Hardware,
    IN gcsALPHA_INFO_PTR Info
    );

gceSTATUS
gcoHARDWARE_SetAntiAliasLine(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetAALineTexSlot(
    IN gcoHARDWARE Hardware,
    IN gctUINT TexSlot
    );

gceSTATUS
gcoHARDWARE_SetAALineWidth(
    IN gcoHARDWARE Hardware,
    IN gctFLOAT Width
    );

gceSTATUS
gcoHARDWARE_SetLastPixelEnable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetLogicOp(
    IN gcoHARDWARE Hardware,
    IN gctUINT8     Rop
    );

gceSTATUS
gcoHARDWARE_SetCentroids(
    IN gcoHARDWARE Hardware,
    IN gctUINT32    Index,
    IN gctPOINTER   Centroids
    );

gceSTATUS
gcoHARDWARE_QuerySamplerBase(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT32 * VertexCount,
    OUT gctINT_PTR VertexBase,
    OUT gctUINT32 * FragmentCount,
    OUT gctINT_PTR FragmentBase
    );

gceSTATUS
gcoHARDWARE_QueryUniformBase(
    IN  gcoHARDWARE Hardware,
    OUT gctUINT32 * VertexBase,
    OUT gctUINT32 * FragmentBase
    );

gceSTATUS
gcoHARDWARE_SetOQ(
    IN gcoHARDWARE Hardware,
    IN gctPOINTER OQ,
    IN gctBOOL Enable
    );

gceSTATUS
gcoHARDWARE_SetColorOutCount(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorOutCount
    );

gceSTATUS
gcoHARDWARE_PrimitiveRestart(
    IN gcoHARDWARE Hardware,
    IN gctBOOL PrimitiveRestart
    );

gceSTATUS
gcoHARDWARE_FlushSHL1Cache(
    IN gcoHARDWARE Hardware
    );

#if gcdSTREAM_OUT_BUFFER

gceSTATUS
gcoHARDWARE_QueryStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount,
    OUT gctBOOL_PTR Found
);

gceSTATUS
gcoHARDWARE_StartStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctINT StreamOutStatus,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount
);

/* Advance stream out. */
gceSTATUS
gcoHARDWARE_StopStreamOut(
    IN gcoHARDWARE Hardware
    );

/* Advance stream out. */
gceSTATUS
gcoHARDWARE_ReplayStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount
    );

/* End stream out. */
gceSTATUS
gcoHARDWARE_EndStreamOut(
    IN gcoHARDWARE Hardware
    );

#endif

/* Invoke OCL thread walker. */
gceSTATUS
gcoHARDWARE_InvokeThreadWalker(
    IN gcoHARDWARE Hardware,
    IN gcsTHREAD_WALKER_INFO_PTR Info
    );

gceSTATUS
gcoHARDWARE_InvokeThreadWalkerWrapper(
    IN gcoHARDWARE Hardware,
    IN gcsTHREAD_WALKER_INFO_PTR Info
    );

#if gcdSYNC
gceSTATUS
gcoHARDWARE_WaitFence(
    IN gcoHARDWARE Hardware,
    IN gcsSYNC_CONTEXT_PTR Ctx
    );

gceSTATUS
gcoHARDWARE_SendFence(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_GetFence(
    IN gcoHARDWARE Hardware,
    IN gcsSYNC_CONTEXT_PTR *Ctx
    );

gceSTATUS
gcoHARDWARE_GetFenceEnabled(
    IN gcoHARDWARE Hardware,
    OUT gctBOOL_PTR Enabled
    );

gceSTATUS
gcoHARDWARE_SetFenceEnabled(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enabled
    );

#endif

#if gcdMULTI_GPU
gceSTATUS
gcoHARDWARE_SetMultiGPURenderingMode(
    IN gcoHARDWARE Hardware,
    IN gceMULTI_GPU_RENDERING_MODE Mode
    );
#endif

gceSTATUS
gcoHARDWARE_3DBlitCopy(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 SrcAddress,
    IN gctUINT32 DestAddress,
    IN gctUINT32 CopySize
    );

gceSTATUS
gcoHARDWARE_3DBlitClearQuery(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info,
    OUT gctBOOL * FastClear
    );

gceSTATUS
gcoHARDWARE_3DBlitClear(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );

gceSTATUS
gcoHARDWARE_3DBlitBlt(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    );

gceSTATUS
gcoHARDWARE_3DBlitTileFill(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR DestInfo,
    IN gcs3DBLIT_INFO_PTR Info
    );

#if VIVANTE_PROFILER_CONTEXT
gceSTATUS
gcoHARDWARE_GetContext(
    IN gcoHARDWARE Hardware,
    OUT gctUINT32 * Context
    );
#endif

#if VIVANTE_PROFILER_NEW
gceSTATUS
gcoHARDWARE_EnableCounters(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_ProbeCounter(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 address,
    IN gctUINT32 module,
    IN gctUINT32 counter
    );
#endif

gceSTATUS
gcoHARDWARE_Set3DHardware(
     IN gcoHARDWARE Hardware
     );

gceSTATUS
gcoHARDWARE_Get3DHardware(
     OUT gcoHARDWARE * Hardware
     );

#endif /* gcdENABLE_3D */
/******************************************************************************\
******************************** gcoBUFFER Object *******************************
\******************************************************************************/

/* Construct a new gcoBUFFER object. */
gceSTATUS
gcoBUFFER_Construct(
    IN gcoHAL Hal,
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Context,
    IN gctSIZE_T MaxSize,
    IN gctBOOL ThreadDefault,
    OUT gcoBUFFER * Buffer
    );

/* Destroy an gcoBUFFER object. */
gceSTATUS
gcoBUFFER_Destroy(
    IN gcoBUFFER Buffer
    );

/* Attach OQ to a gcoBUFFER object */
gceSTATUS
gcoBUFFER_AttachOQ(
    IN gcoBUFFER Buffer,
    IN gctPOINTER OQ
    );

/* Reserve space in a command buffer. */
gceSTATUS
gcoBUFFER_Reserve(
    IN gcoBUFFER Buffer,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned,
    IN gctUINT32 Usage,
    OUT gcoCMDBUF * Reserve
    );


typedef struct _gcsTEMPCMDBUF       * gcsTEMPCMDBUF;

/* start a temp command buffer */
gceSTATUS
gcoBUFFER_StartTEMPCMDBUF(
    IN gcoBUFFER Buffer,
    OUT gcsTEMPCMDBUF *tempCMDBUF
    );

/* end of a temp command buffer */
gceSTATUS
gcoBUFFER_EndTEMPCMDBUF(
    IN gcoBUFFER Buffer
    );

/* Write data into the command buffer. */
gceSTATUS
gcoBUFFER_Write(
    IN gcoBUFFER Buffer,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    );

/* Doesn't have implementation */
/* Write 32-bit data into the command buffer. */
gceSTATUS
gcoBUFFER_Write32(
    IN gcoBUFFER Buffer,
    IN gctUINT32 Data,
    IN gctBOOL Aligned
    );
/* Doesn't have implementation */
/* Write 64-bit data into the command buffer. */
gceSTATUS
gcoBUFFER_Write64(
    IN gcoBUFFER Buffer,
    IN gctUINT64 Data,
    IN gctBOOL Aligned
    );

/* Commit the command buffer. */
#if gcdDUMP
gceSTATUS
gcoBUFFER_Commit(
    IN gcoBUFFER Buffer,
    IN gcePIPE_SELECT CurrentPipe,
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gcoQUEUE Queue,
    OUT gctPOINTER *DumpLogical,
    OUT gctUINT32 *DumpBytes
    );
#else
gceSTATUS
gcoBUFFER_Commit(
    IN gcoBUFFER Buffer,
    IN gcePIPE_SELECT CurrentPipe,
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gcoQUEUE Queue
    );
#endif

#if gcdSYNC
typedef enum _gceFENCE_STATUS
{
    gcvFENCE_DISABLE,
    gcvFENCE_GET,
    gcvFENCE_ENABLE,
}
gceFENCE_STATUS;
#endif

/******************************************************************************\
******************************** gcoCMDBUF Object *******************************
\******************************************************************************/

typedef struct _gcsCOMMAND_INFO * gcsCOMMAND_INFO_PTR;

/* Construct a new gcoCMDBUF object. */
gceSTATUS
gcoCMDBUF_Construct(
    IN gcoOS Os,
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T Bytes,
    IN gcsCOMMAND_INFO_PTR Info,
    OUT gcoCMDBUF * Buffer
    );

/* Destroy an gcoCMDBUF object. */
gceSTATUS
gcoCMDBUF_Destroy(
    IN gcoHARDWARE Hardware,
    IN gcsCOMMAND_INFO_PTR Info,
    IN gcoCMDBUF Buffer
    );

#ifdef LINUX
#define PENDING_FREED_MEMORY_SIZE_LIMIT     (4 * 1024 * 1024)
#endif

/******************************************************************************\
********************************* gcoHAL object *********************************
\******************************************************************************/

struct _gcoHAL
{
    gcsOBJECT               object;

    gcoDUMP                 dump;

#if VIVANTE_PROFILER
    gcsPROFILER             profiler;
#endif

#if gcdFRAME_DB
    gctINT                  frameDBIndex;
    gcsHAL_FRAME_INFO       frameDB[gcdFRAME_DB];
#endif

    gctINT32                chipCount;
    gceHARDWARE_TYPE        chipTypes[gcdCHIP_COUNT];
    gctBOOL                 separated2D;
    gctBOOL                 is3DAvailable;
    gctBOOL                 isGpuBenchSmoothTriangle;

#if gcdENABLE_3D
    gcsSTATISTICS           statistics;
#endif

    gcsUSER_DEBUG_OPTION    *userDebugOption;
};

/******************************************************************************\
********************************* gcoSURF object ********************************
\******************************************************************************/

#if gcdSYNC
typedef struct _gcsSYNC_CONTEXT
{
    gctUINT64               fenceID;
    gctUINT32               oqSlot;
    gctPOINTER              fence;
    gctBOOL                 mark;
    gcsSYNC_CONTEXT_PTR     next;
}
gcsSYNC_CONTEXT;
#endif

typedef struct _gcsSURF_NODE
{
    /* Surface memory pool. */
    gcePOOL                 pool;

    /* Lock count for the surface. */
    gctINT                  lockCount;

    /* If not zero, the node is locked in the kernel. */
    gctBOOL                 lockedInKernel;

    /* Locked hardware type */
    gceHARDWARE_TYPE        lockedHardwareType;

    /* Number of planes in the surface for planar format support. */
    gctUINT                 count;

    /* Node valid flag for the surface pointers. */
    gctBOOL                 valid;

    /* The physical addresses of the surface. */
    gctUINT32               physical;
    gctUINT32               physical2;
    gctUINT32               physical3;

    /* The logical addresses of the surface. */
    gctUINT8_PTR            logical;

    /* If the buffer was split, they record the physical and logical addr
    ** of the bottom buffer.
    */
    gctUINT32               physicalBottom;
    gctUINT8_PTR            logicalBottom;

    /* Linear size for tile status. */
    gctSIZE_T               size;
    gctBOOL                 firstLock;

    union _gcuSURF_NODE_LIST
    {
        /* Allocated through HAL. */
        struct _gcsMEM_NODE_NORMAL
        {
            gctUINT32           node;
            gctBOOL             cacheable;
        }
        normal;

        /* Wrapped around user-allocated surface (gcvPOOL_USER). */
        struct _gcsMEM_NODE_WRAPPED
        {
            gctBOOL             logicalMapped;
            gctPOINTER          mappingInfo;
            gceHARDWARE_TYPE    mappingHardwareType;
        }
        wrapped;
    }
    u;
}
gcsSURF_NODE;

typedef struct _gcsSURF_INFO
{
    /* Type usage and format of surface. */
    gceSURF_TYPE            type;
    gceSURF_TYPE            hints;
    gceSURF_FORMAT          format;
    gceTILING               tiling;

    /* Surface size. */
    gcsRECT                 rect;
    gctUINT                 alignedWidth;
    gctUINT                 alignedHeight;
    gctBOOL                 is16Bit;
    gctUINT32               bitsPerPixel;

    /*
    ** Offset in byte of the bottom buffer from beginning of the top buffer.
    ** If HW does not require to be programmed wtih 2 addresses, it should be 0.
    */
    gctUINT32               bottomBufferOffset;

    /* Rotation flag. */
    gceSURF_ROTATION        rotation;
    gceORIENTATION          orientation;

    /* Dither flag. */
    gctBOOL                 dither2D;

    /* For some chips, PE doesn't support dither. If app request it, we defer it until resolve time. */
    gctBOOL                 deferDither3D;

    /* Surface stride, sliceSize, size in bytes. */

    /*
    ** In one surface(miplevel), there maybe are some layers(patch format),
    ** in every layer, there squeeze multiple slices(cube,3d,array),
    ** sliceSize is to calculate offset for different slice.
    ** layerSize is to calculate offset for different layer.
    ** stride is the pitch of a single layer.
    */
    gctUINT                 stride;
    gctUINT                 sliceSize;
    gctUINT                 layerSize;
    gctUINT                 size;

    /* YUV planar surface parameters. */
    gctUINT                 uOffset;
    gctUINT                 vOffset;
    gctUINT                 uStride;
    gctUINT                 vStride;

    /* Video memory node for surface. */
    gcsSURF_NODE            node;

    /* Surface color type. */
    gceSURF_COLOR_TYPE      colorType;

    /* Surface color space */
    gceSURF_COLOR_SPACE     colorSpace;

    /* Only useful for R8_1_X8R8G8B8/G8R8_1_X8R8G8B8 formats.
    ** garbagePadded=0 means driver is sure the padding channels of whole surface are default values;
    ** garbagePadded=1 means padding channels of some subrect of surface might be garbages.
    */
    gctBOOL                 paddingFormat;
    gctBOOL                 garbagePadded;

    /* Samples. */
    gcsSAMPLES              samples;
    gctBOOL                 vaa;

#if gcdENABLE_3D
    /* Tile status. */
    /* FC clear status and its values */
    gctBOOL                 tileStatusDisabled;
    gctUINT32               fcValue;
    gctUINT32               fcValueUpper;
    /*
    ** Only meaningful when tileStatusDisabled == gcvFALSE
    */
    gctBOOL                 compressed;
    gctINT32                compressFormat;

    gctBOOL                 dirty;

    gctBOOL                 hzDisabled;
    gctBOOL                 superTiled;

    /* Clear value of last time, for value passing and reference */
    gctUINT32               clearValue[gcdMAX_SURF_LAYERS];
    gctUINT32               clearValueUpper[gcdMAX_SURF_LAYERS];
    gctUINT32               clearValueHz;

    /*
    ** 4-bit channel Mask: ARGB:MSB->LSB
    */
    gctUINT8                clearMask[gcdMAX_SURF_LAYERS];

    /*
    ** 32-bit bit Mask
    */
    gctUINT32               clearBitMask[gcdMAX_SURF_LAYERS];

    /* Hierarchical Z buffer pointer and its fc value */
    gcsSURF_NODE            hzNode;
    gctUINT32               fcValueHz;

    /* Video memory node for tile status. */
    gcsSURF_NODE            tileStatusNode;
    gcsSURF_NODE            hzTileStatusNode;
    gctBOOL                 TSDirty;
    gctUINT32               tileStatusFiller;
    gctUINT32               hzTileStatusFiller;

#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
    /* For flip resolve limitation, this is offset to real bitmap pixels. */
    gctSIZE_T               flipBitmapOffset;
#endif

    /* When only clear depth for packed depth stencil surface, we can fast clear
    ** both depth and stencil (to 0) if stencil bits are never initialized.
    */
    gctBOOL                 hasStencilComponent;
    gctBOOL                 canDropStencilPlane;
#endif /* gcdENABLE_3D */

    gctUINT                 offset;

    gceSURF_USAGE           usage;

    /* 2D related members. */
    gce2D_TILE_STATUS_CONFIG    tileStatusConfig;
    gceSURF_FORMAT              tileStatusFormat;
    gctUINT32                   tileStatusClearValue;
    gctUINT32                   tileStatusGpuAddress;

    /* Format information. */
    gcsSURF_FORMAT_INFO         formatInfo;

#if gcdSYNC
    gceFENCE_STATUS             fenceStatus;
    gcsSYNC_CONTEXT_PTR         fenceCtx;
    gctPOINTER                  sharedLock;
#endif

    gceSURF_FLAG                flags;

    /* Timestamp, indicate when surface is changed. */
    gctUINT64                   timeStamp;

    /* Shared buffer. */
    gctSHBUF                    shBuf;
}
gcsSURF_INFO;


#define gcvSURF_SHARED_INFO_MAGIC     gcmCC('s', 'u', 's', 'i')

/* Surface states need share across processes. */
typedef struct _gcsSURF_SHARED_INFO
{
    gctUINT32 magic;
    gctUINT64 timeStamp;

#if gcdENABLE_3D
    gctBOOL   tileStatusDisabled;
    gctBOOL   dirty;
    gctUINT32 fcValue;
    gctUINT32 fcValueUpper;
    gctBOOL   compressed;
#endif
}
gcsSURF_SHARED_INFO;

struct _gcoSURF
{
    /* Object. */
    gcsOBJECT               object;

    /* Surface information structure. */
    struct _gcsSURF_INFO    info;

    /* Depth of the surface in planes. */
    gctUINT                 depth;

#if gcdENABLE_3D
    gctBOOL                 resolvable;

#if defined(ANDROID)
    /* Used for 3D app - SF sync. */
    gctSIGNAL               resolveSubmittedSignal;
#endif
#endif /* gcdENABLE_3D */

    /* Automatic stride calculation. */
    gctBOOL                 autoStride;

    /* User pointers for the surface wrapper. */
    gctPOINTER              logical;
    gctUINT32               physical;

    /* Reference count of surface. */
    gctINT32                referenceCount;
};

typedef void  (* _PFNreadPixel)(gctPOINTER inAddr[4], gcsPIXEL* outPixel);
typedef void  (* _PFNwritePixel)(gcsPIXEL* inPixel, gctPOINTER outAddr[4], gctUINT flags);
typedef void  (* _PFNcalcPixelAddr)(gcoSURF surf, gctSIZE_T x, gctSIZE_T y, gctSIZE_T z, gctPOINTER addr[4]);

_PFNreadPixel gcoSURF_GetReadPixelFunc(gcoSURF surf);
_PFNwritePixel gcoSURF_GetWritePixelFunc(gcoSURF surf);
_PFNcalcPixelAddr gcoHARDWARE_GetProcCalcPixelAddr(gcoHARDWARE Hardware, gcoSURF surf);
void gcoSURF_PixelToNonLinear(gcsPIXEL* inPixel);
void gcoSURF_PixelToLinear(gcsPIXEL* inPixel);

#define gcdMULTI_SOURCE_NUM 8

typedef struct _gcs2D_MULTI_SOURCE
{
    gce2D_SOURCE srcType;
    gcsSURF_INFO srcSurface;
    gceSURF_MONOPACK srcMonoPack;
    gctUINT32 srcMonoTransparencyColor;
    gctBOOL   srcColorConvert;
    gctUINT32 srcFgColor;
    gctUINT32 srcBgColor;
    gctUINT32 srcColorKeyLow;
    gctUINT32 srcColorKeyHigh;
    gctBOOL srcRelativeCoord;
    gctBOOL srcStream;
    gcsRECT srcRect;
    gce2D_YUV_COLOR_MODE srcYUVMode;
    gctBOOL srcDeGamma;

    gce2D_TRANSPARENCY srcTransparency;
    gce2D_TRANSPARENCY dstTransparency;
    gce2D_TRANSPARENCY patTransparency;

    gctBOOL enableDFBColorKeyMode;

    gctUINT8 fgRop;
    gctUINT8 bgRop;

    gctBOOL enableAlpha;
    gceSURF_PIXEL_ALPHA_MODE  srcAlphaMode;
    gceSURF_PIXEL_ALPHA_MODE  dstAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE srcGlobalAlphaMode;
    gceSURF_GLOBAL_ALPHA_MODE dstGlobalAlphaMode;
    gceSURF_BLEND_FACTOR_MODE srcFactorMode;
    gceSURF_BLEND_FACTOR_MODE dstFactorMode;
    gceSURF_PIXEL_COLOR_MODE  srcColorMode;
    gceSURF_PIXEL_COLOR_MODE  dstColorMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE srcPremultiplyMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE dstPremultiplyMode;
    gce2D_GLOBAL_COLOR_MULTIPLY_MODE srcPremultiplyGlobalMode;
    gce2D_PIXEL_COLOR_MULTIPLY_MODE dstDemultiplyMode;
    gctUINT32 srcGlobalColor;
    gctUINT32 dstGlobalColor;

    gcsRECT     clipRect;
    gcsRECT     dstRect;

    /* Stretch factors. */
    gctBOOL     enableGDIStretch;
    gctUINT32   horFactor;
    gctUINT32   verFactor;

    /* Mirror. */
    gctBOOL horMirror;
    gctBOOL verMirror;

} gcs2D_MULTI_SOURCE, *gcs2D_MULTI_SOURCE_PTR;

/* FilterBlt information. */
#define gcvMAXKERNELSIZE        9
#define gcvSUBPIXELINDEXBITS    5

#define gcvSUBPIXELCOUNT \
    (1 << gcvSUBPIXELINDEXBITS)

#define gcvSUBPIXELLOADCOUNT \
    (gcvSUBPIXELCOUNT / 2 + 1)

#define gcvWEIGHTSTATECOUNT \
    (((gcvSUBPIXELLOADCOUNT * gcvMAXKERNELSIZE + 1) & ~1) / 2)

#define gcvKERNELTABLESIZE \
    (gcvSUBPIXELLOADCOUNT * gcvMAXKERNELSIZE * sizeof(gctUINT16))

#define gcvKERNELSTATES \
    (gcmALIGN(gcvKERNELTABLESIZE + 4, 8))

typedef struct _gcsFILTER_BLIT_ARRAY
{
    gceFILTER_TYPE              filterType;
    gctUINT8                    kernelSize;
    gctUINT32                   scaleFactor;
    gctBOOL                     kernelChanged;
    gctUINT32_PTR               kernelStates;
}
gcsFILTER_BLIT_ARRAY;

typedef gcsFILTER_BLIT_ARRAY *  gcsFILTER_BLIT_ARRAY_PTR;

#define gcd2D_CSC_TABLE_SIZE        12
#define gcd2D_GAMMA_TABLE_SIZE      256

typedef struct _gcs2D_State
{
    gctUINT             currentSrcIndex;
    gcs2D_MULTI_SOURCE  multiSrc[gcdMULTI_SOURCE_NUM];
    gctUINT32           srcMask;

    /* dest info. */
    gcsSURF_INFO dstSurface;
    gctUINT32    dstColorKeyLow;
    gctUINT32    dstColorKeyHigh;
    gce2D_YUV_COLOR_MODE dstYUVMode;
    gctBOOL      dstEnGamma;
    gcsRECT      dstClipRect;

    /* brush info. */
    gce2D_PATTERN brushType;
    gctUINT32 brushOriginX;
    gctUINT32 brushOriginY;
    gctUINT32 brushColorConvert;
    gctUINT32 brushFgColor;
    gctUINT32 brushBgColor;
    gctUINT64 brushBits;
    gctUINT64 brushMask;
    gctUINT32 brushAddress;
    gceSURF_FORMAT brushFormat;

    /* Dithering enabled. */
    gctBOOL dither;

    /* Clear color. */
    gctUINT32 clearColor;

    /* Palette Table for source. */
    gctUINT32  paletteIndexCount;
    gctUINT32  paletteFirstIndex;
    gctBOOL    paletteConvert;
    gctBOOL    paletteProgram;
    gctPOINTER paletteTable;
    gceSURF_FORMAT paletteConvertFormat;

    /* Filter blit. */
    gceFILTER_TYPE              newFilterType;
    gctUINT8                    newHorKernelSize;
    gctUINT8                    newVerKernelSize;

    gctBOOL                     horUserFilterPass;
    gctBOOL                     verUserFilterPass;

    gcsFILTER_BLIT_ARRAY        horSyncFilterKernel;
    gcsFILTER_BLIT_ARRAY        verSyncFilterKernel;

    gcsFILTER_BLIT_ARRAY        horBlurFilterKernel;
    gcsFILTER_BLIT_ARRAY        verBlurFilterKernel;

    gcsFILTER_BLIT_ARRAY        horUserFilterKernel;
    gcsFILTER_BLIT_ARRAY        verUserFilterKernel;

    gctBOOL                     specialFilterMirror;

    gctINT32                    cscYUV2RGB[gcd2D_CSC_TABLE_SIZE];
    gctINT32                    cscRGB2YUV[gcd2D_CSC_TABLE_SIZE];

    gctUINT32                   deGamma[gcd2D_GAMMA_TABLE_SIZE];
    gctUINT32                   enGamma[gcd2D_GAMMA_TABLE_SIZE];

    gce2D_SUPER_TILE_VERSION    superTileVersion;
    gctBOOL                     unifiedDstRect;

   /* XRGB */
    gctBOOL                     enableXRGB;

    gctBOOL                     forceHWStuck;

} gcs2D_State;

typedef struct _gcsOPF_BLOCKSIZE_TABLE
{
    gctUINT8 width;
    gctUINT8 height;
    gctUINT8 blockDirect;
    gctUINT8 pixelDirect;
    gctUINT16 horizontal;
    gctUINT16 vertical;
}
gcsOPF_BLOCKSIZE_TABLE;

typedef gcsOPF_BLOCKSIZE_TABLE *  gcsOPF_BLOCKSIZE_TABLE_PTR;

extern gcsOPF_BLOCKSIZE_TABLE_PTR gcsOPF_TABLE_TABLE_ONEPIPE[];

/******************************************************************************\
******************************* gcsOQ Structure *******************************
\******************************************************************************/
typedef enum _gceOQStatus
{
    gcvOQ_Disable,
    gcvOQ_Paused,
    gcvOQ_Enable,
}gceOQStatus;

typedef struct _gcoOQ
{
    gceOQStatus                 oqStatus;
    gctUINT32                   oqSize;
    gcsSURF_NODE                node;
    gctPOINTER                  oqPointer;
    gctUINT32                   oqPhysic;
    gctINT32                    oqIndex;
}gcsOQ;

/******************************************************************************\
******************************** gcoQUEUE Object *******************************
\******************************************************************************/

/* Construct a new gcoQUEUE object. */
gceSTATUS
gcoQUEUE_Construct(
    IN gcoOS Os,
    OUT gcoQUEUE * Queue
    );

/* Destroy a gcoQUEUE object. */
gceSTATUS
gcoQUEUE_Destroy(
    IN gcoQUEUE Queue
    );

/* Append an event to a gcoQUEUE object. */
gceSTATUS
gcoQUEUE_AppendEvent(
    IN gcoQUEUE Queue,
    IN gcsHAL_INTERFACE * Interface
    );

/* Commit and event queue. */
gceSTATUS
gcoQUEUE_Commit(
    IN gcoQUEUE Queue,
    IN gctBOOL Stall
    );

/* Commit and event queue. */
gceSTATUS
gcoQUEUE_Free(
    IN gcoQUEUE Queue
    );

#if gcdENABLE_3D
/*******************************************************************************
***** Vertex Management *******************************************************/

typedef struct _gcsVERTEXARRAY_ATTRIBUTE    gcsVERTEXARRAY_ATTRIBUTE;
typedef struct _gcsVERTEXARRAY_ATTRIBUTE *  gcsVERTEXARRAY_ATTRIBUTE_PTR;

struct _gcsVERTEXARRAY_ATTRIBUTE
{
    /* Pointer to vertex information. */
    gcsVERTEXARRAY_PTR                      vertexPtr;

    /* Pointer to next attribute in a stream. */
    gcsVERTEXARRAY_ATTRIBUTE_PTR            next;

    /* Logical address of this attribute. */
    gctCONST_POINTER                        logical;

    /* Format of this attribute. */
    gceVERTEX_FORMAT                        format;

    /* Offset into the stream of this attribute. */
    gctUINT                                 offset;

    /* Number of bytes of this attribute. */
    gctUINT                                 bytes;
};

typedef struct _gcsSTREAM_SUBSTREAM    gcsSTREAM_SUBSTREAM;
typedef struct _gcsSTREAM_SUBSTREAM *  gcsSTREAM_SUBSTREAM_PTR;
struct _gcsSTREAM_SUBSTREAM
{
    /* Current range for the sub-stream. */
    gctUINT                                 start;
    gctUINT                                 end;

    /* Maximum range of the window for the sub-stream. */
    gctUINT                                 minStart;
    gctUINT                                 maxEnd;

    /* Stride for the sub-stream. */
    gctUINT                                 stride;

    /* Pointer to associated gcoSTREAM object. */
    gcoSTREAM                               stream;

    /* Divisor for instanced drawing */
    gctUINT                                 divisor;

    /* Pointer to next sub-stream. */
    gcsSTREAM_SUBSTREAM_PTR                 next;
};

typedef struct _gcsVERTEXARRAY_STREAM
{
    /* Pointer to gcoSTREAM object. */
    gcoSTREAM                               stream;

    /* Logical address of stream data. */
    gctUINT8_PTR                            logical;

    /* Physical address of the stream data. */
    gctUINT32                               physical;

    /* Pointer to first attribute part of this stream. */
    gcsVERTEXARRAY_ATTRIBUTE_PTR            attribute;

    /* Pointer to sub-streams. */
    gcsSTREAM_SUBSTREAM_PTR                 subStream;
}
gcsVERTEXARRAY_STREAM,
* gcsVERTEXARRAY_STREAM_PTR;

#if OPT_VERTEX_ARRAY
typedef struct _gcsVERTEXARRAY_BUFFER
{
    /* Attribute Maps */
    gctUINT                                 map[gcdATTRIBUTE_COUNT];
    gctUINT                                 numAttribs;

    /* Indicate a buffer is combined from multi-buffer. */
    gctBOOL                                 combined;

    /* Logical address of stream data. */
    gctUINT8_PTR                            start;
    gctUINT8_PTR                            end;

    gctUINT8_PTR                            minStart;
    gctUINT8_PTR                            maxEnd;

    /* Stride for the sub-buffer. */
    gctUINT                                 stride;

    /* How many vertex required by app? DrawInstance should calc separately */
    gctUINT                                 count;

    /* Buffer offset in a stream. */
    gctUINT32                               offset;

    /* Divisor for the buffer */
    gctUINT32                               divisor;

}gcsVERTEXARRAY_BUFFER,
* gcsVERTEXARRAY_BUFFER_PTR;

typedef struct _gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE     gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE;
typedef struct _gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE *   gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR;

struct _gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE
{
    /* Format of this attribute. */
    gceVERTEX_FORMAT                        format;

    gctUINT32                               linkage;

    gctUINT32                               size;

    gctBOOL                                 normalized;

    gctBOOL                                 enabled;

    gctUINT32                               offset;

    gctCONST_POINTER                        pointer;

    gctUINT32                               bytes;

    gctBOOL                                 isPosition;

    gctUINT                                 stride;

    gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR     next;
};

typedef struct _gcsVERTEXARRAY_BUFOBJ   gcsVERTEXARRAY_BUFOBJ;
typedef struct _gcsVERTEXARRAY_BUFOBJ * gcsVERTEXARRAY_BUFOBJ_PTR;

struct _gcsVERTEXARRAY_BUFOBJ
{
    gcoBUFOBJ                               stream;

    gctUINT                                 stride;

    gctUINT                                 divisor;

    gctUINT32                               physical;

    gctUINT8_PTR                            logical;

    gctSIZE_T                               count;

    gctUINT                                 dynamicCacheStride;

    gctBOOL                                 copyAll;

    gctBOOL                                 merged;

    /* Attribute List */
    gctUINT                                 attributeCount;
    gcsVERTEXARRAY_BUFOBJ_ATTRIBUTE_PTR     attributePtr;

    /* pointer to next stream */
    gcsVERTEXARRAY_BUFOBJ_PTR               next;
};

#endif

gceSTATUS
gcoSTREAM_SetAttribute(
    IN gcoSTREAM Stream,
    IN gctUINT Offset,
    IN gctUINT Bytes,
    IN gctUINT Stride,
    IN gctUINT Divisor,
    IN OUT gcsSTREAM_SUBSTREAM_PTR * SubStream
    );

gceSTATUS
gcoSTREAM_QuerySubStreams(
    IN gcoSTREAM Stream,
    IN gcsSTREAM_SUBSTREAM_PTR SubStream,
    OUT gctUINT_PTR SubStreamCount
    );

gceSTATUS
gcoSTREAM_Rebuild(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    OUT gctUINT_PTR SubStreamCount
    );

gceSTATUS
gcoSTREAM_SetCache(
    IN gcoSTREAM Stream
    );

gctSIZE_T
gcoSTREAM_GetSize(
        IN gcoSTREAM Stream
        );

#if OPT_VERTEX_ARRAY

gceSTATUS
gcoSTREAM_CacheAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT TotalBytes,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical
    );

gceSTATUS
gcoSTREAM_CacheAttributesEx(
    IN gcoSTREAM Stream,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_BUFOBJ_PTR Streams,
    IN gctUINT First,
    IN OUT gcoSTREAM * UncacheableStream
    );

gceSTATUS
gcoSTREAM_UploadUnCacheableAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT TotalBytes,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical,
    OUT gcoSTREAM * OutStream
);

#else

gceSTATUS
gcoSTREAM_CacheAttributes(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT Stride,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gctUINT32_PTR Physical
    );

#endif

gceSTATUS
gcoSTREAM_UnAlias(
    IN gcoSTREAM Stream,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    OUT gcsSTREAM_SUBSTREAM_PTR * SubStream,
    OUT gctUINT8_PTR * Logical,
    OUT gctUINT32 * Physical
    );

#if OPT_VERTEX_ARRAY
gceSTATUS
gcoHARDWARE_SetVertexArray(
    IN gcoHARDWARE Hardware,
    IN gctBOOL DrawArraysInstanced,
    IN gctUINT First,
    IN gctUINT32 Physical,
    IN gctUINT BufferCount,
    IN gcsVERTEXARRAY_BUFFER_PTR Buffers,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams
    );

gceSTATUS
gcoHARDWARE_SetVertexArrayEx(
    IN gcoHARDWARE Hardware,
    IN gctBOOL DrawInstanced,
    IN gctBOOL DrawElements,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_BUFOBJ_PTR Streams,
    IN gctUINT StartVertex,
    IN gctUINT FirstCopied,
    IN gctINT VertexInstanceIdLinkage
    );

#else
gceSTATUS
gcoHARDWARE_SetVertexArray(
    IN gcoHARDWARE Hardware,
    IN gctUINT First,
    IN gctUINT32 Physical,
    IN gctUINT Stride,
    IN gctUINT AttributeCount,
    IN gcsVERTEXARRAY_ATTRIBUTE_PTR Attributes,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams
    );
#endif

gceSTATUS
gcoSTREAM_MergeStreams(
    IN gcoSTREAM Stream,
    IN gctUINT First,
    IN gctUINT Count,
    IN gctUINT StreamCount,
    IN gcsVERTEXARRAY_STREAM_PTR Streams,
    OUT gcoSTREAM * MergedStream,
    OUT gctPOINTER * Logical,
    OUT gctUINT32 * Physical,
    OUT gcsVERTEXARRAY_ATTRIBUTE_PTR * Attributes,
    OUT gcsSTREAM_SUBSTREAM_PTR * SubStream
    );

gceSTATUS
gcoHARDWARE_HzClearValueControl(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 ZClearValue,
    OUT gctUINT32 * HzClearValue OPTIONAL,
    OUT gctUINT32 * Control OPTIONAL
    );

gceSTATUS
gcoHARDWARE_QueryShaderCompilerHwCfg(
    IN gcoHARDWARE Hardware,
    OUT PVSC_HW_CONFIG pVscHwCfg
    );

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
    );

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
    );

gceSTATUS
gcoHARDWARE_BindUniformBlock(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Base,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Size,
    IN gcSHADER_KIND Type
    );

gceSTATUS
gcoHARDWARE_QueryCompression(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    OUT gctBOOL *Compressed,
    OUT gctINT32 *CompressedFormat
    );

gceSTATUS
gcoHARDWARE_BlitDraw(
    IN gcsSURF_BLITDRAW_ARGS *args
    );

#endif /* gcdENABLE_3D */

gceSTATUS
gcoHARDWARE_GetSpecialHintData(
    IN gcoHARDWARE Hardware,
    OUT gctINT * Hint
    );

gceSTATUS
gcoOS_PrintStrVSafe(
    OUT gctSTRING String,
    IN gctSIZE_T StringSize,
    IN OUT gctUINT * Offset,
    IN gctCONST_STRING Format,
    IN gctARGUMENTS Arguments
    );

#if gcdENABLE_3D

gceSTATUS
gcoSURF_LockTileStatus(
    IN gcoSURF Surface
    );

gceSTATUS
gcoSURF_LockHzBuffer(
    IN gcoSURF Surface
    );

gceSTATUS
gcoSURF_AllocateTileStatus(
    IN gcoSURF Surface
    );

gceSTATUS
gcoSURF_AllocateHzBuffer(
    IN gcoSURF Surface
    );

gceSTATUS
gcoHARDWARE_ResumeOQ(
    IN gcoHARDWARE Hardware,
    IN gcsOQ *OQ,
    IN gctUINT64 CommandBuffer
    );

gceSTATUS
gcoHARDWARE_PauseOQ(
    IN gcoHARDWARE Hardware,
    IN gcsOQ *OQ,
    IN gctUINT64 CommandBuffer
    );


#endif

gceSTATUS
gcsSURF_NODE_Construct(
    IN gcsSURF_NODE_PTR Node,
    IN gctSIZE_T Bytes,
    IN gctUINT Alignment,
    IN gceSURF_TYPE Type,
    IN gctUINT32 Flag,
    IN gcePOOL Pool
    );

gceSTATUS
gcoHARDWARE_GetProductName(
    IN gcoHARDWARE Hardware,
    IN OUT gctSTRING *ProductName
    );


#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_user_h_ */
