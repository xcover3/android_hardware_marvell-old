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


#ifndef __gc_hal_user_hardware_h_
#define __gc_hal_user_hardware_h_

#include "gc_hal_user_buffer.h"

#if gcdENABLE_VG
#include "gc_hal_user_vg.h"
#include "gc_hal_user_hardware_vg.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define gcdSAMPLERS                 32
#define gcdSAMPLER_TS               8
#define gcdLOD_LEVELS               14
#define gcdTEMP_SURFACE_NUMBER      3

#define gcvINVALID_TEXTURE_FORMAT   ((gctUINT)(gceSURF_FORMAT) -1)
#define gcvINVALID_RENDER_FORMAT    ((gctUINT)(gceSURF_FORMAT) -1)

#if gcdMULTI_GPU
#define gcmENABLE3DCORE(Memory, CoreId) \
{ \
    *Memory++ = gcmSETFIELDVALUE(0, GCCMD_CHIP_ENABLE_COMMAND, OPCODE, CHIP_ENABLE) \
              | CoreId; \
    \
    Memory++; \
    \
    gcmDUMP(gcvNULL, "#[chip.enable 0x%04X]", CoreId); \
}
#endif

/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/

typedef enum
{
    gcvFILTER_BLIT_KERNEL_UNIFIED,
    gcvFILTER_BLIT_KERNEL_VERTICAL,
    gcvFILTER_BLIT_KERNEL_HORIZONTAL,
    gcvFILTER_BLIT_KERNEL_NUM,
}
gceFILTER_BLIT_KERNEL;

#if gcdENABLE_3D

typedef enum
{
    gcvMSAA_DOWNSAMPLE_AVERAGE = 0,
    gcvMSAA_DOWNSAMPLE_SAMPLE  = 1,
}
gceMSAA_DOWNSAMPLE_MODE;


typedef struct _gcsCOLOR_TARGET
{
    gctUINT32                   format;
    gctBOOL                     superTiled;
    gcsSURF_INFO_PTR            surface;
    /* offset to indicate where slice is */
    gctUINT32                   offset;
    /* layerIndx to indicate where layer is */
    gctUINT32                   layerIndex;
}
gcsCOLOR_TARGET;

typedef struct _gcsCOLOR_INFO
{
    /* per-RT state */
    gcsCOLOR_TARGET             target[gcdMAX_DRAW_BUFFERS];

    /* cacheMode depend on MSAA, consistent between RTs. */
    gctUINT32                   cacheMode;

    /*True if either RT need compression*/
    gctBOOL                     colorCompression;

    /* global color state */
    gctUINT8                    rop;
    gctBOOL                     colorWrite;
    gctUINT32                   destinationRead;
}
gcsCOLOR_INFO;

typedef struct _gcsDEPTH_INFO
{
    gcsSURF_INFO_PTR            surface;
    gctUINT32                   offset;
    gctUINT32                   cacheMode;

    /* Some depth related register value */
    gctUINT32                   regDepthConfig;
    gctUINT32                   regControlHZ;
    gctUINT32                   regRAControl;
    gctUINT32                   regRAControlHZ;

    /* Depth config components. */
    gceDEPTH_MODE               mode;
    gctBOOL                     only;
    gctBOOL                     realOnly;
    gctBOOL                     early;
    gctBOOL                     write;
    gceCOMPARE                  compare;

    /* Depth range. */
    gctFLOAT                    near;
    gctFLOAT                    far;
}
gcsDEPTH_INFO;

typedef struct _gcsPA_INFO
{
    gctBOOL                     aaLine;
    gctUINT                     aaLineTexSlot;
    gctFLOAT                    aaLineWidth;

    gceSHADING                  shading;
    gceCULL                     culling;
    gctBOOL                     pointSize;
    gctBOOL                     pointSprite;
    gceFILL                     fillMode;

    gctBOOL                        wclip;
}
gcsPA_INFO;

typedef struct _gcsSHADER_INFO
{
    gctSIZE_T                   stateBufferSize;
    gctUINT32_PTR               stateBuffer;
    gcsHINT_PTR                 hints;
}
gcsSHADER_INFO;

typedef struct _gcsCENTROIDS * gcsCENTROIDS_PTR;
typedef struct _gcsCENTROIDS
{
    gctUINT32                   value[4];
}
gcsCENTROIDS;

typedef enum
{
    gcvVAA_NONE,
    gcvVAA_COVERAGE_16,
    gcvVAA_COVERAGE_8,
}
gceVAA;

/* Composition state buffer definitions. */
#define gcdCOMP_BUFFER_COUNT        8
#define gcdCOMP_BUFFER_SIZE         (16 * 1024)

/* Composition layer descriptor. */
typedef struct _gcsCOMPOSITION_LAYER * gcsCOMPOSITION_LAYER_PTR;
typedef struct _gcsCOMPOSITION_LAYER
{
    /* Input parameters. */
    gcsCOMPOSITION                  input;

    /* Surface parameters. */
    gcsSURF_INFO_PTR                surface;
    gceSURF_TYPE                    type;
    gctUINT                         stride;
    gctUINT32                       swizzle;
    gctUINT32                       physical;

    gctUINT                         sizeX;
    gctUINT                         sizeY;

    gctUINT8                        samplesX;
    gctUINT8                        samplesY;

    gctUINT32                       format;
    gctBOOL                         hasAlpha;

    gctBOOL                         flip;

    /* Blending parameters. */
    gctBOOL                         needPrevious;
    gctBOOL                         replaceAlpha;
    gctBOOL                         modulateAlpha;

    /* Allocated resources. */
    gctUINT                         constRegister;
    gctUINT                         samplerNumber;
}
gcsCOMPOSITION_LAYER;

/* Composition layer node. */
typedef struct _gcsCOMPOSITION_NODE * gcsCOMPOSITION_NODE_PTR;
typedef struct _gcsCOMPOSITION_NODE
{
    /* Pointer to the layer structure. */
    gcsCOMPOSITION_LAYER_PTR        layer;

    /* Next layer node. */
    gcsCOMPOSITION_NODE_PTR         next;
}
gcsCOMPOSITION_NODE;

/* Set of overlapping layers. */
typedef struct _gcsCOMPOSITION_SET * gcsCOMPOSITION_SET_PTR;
typedef struct _gcsCOMPOSITION_SET
{
    /* Blurring layer. */
    gctBOOL                         blur;

    /* Bounding box. */
    gcsRECT                         trgRect;

    /* List of layer nodes. */
    gcsCOMPOSITION_NODE_PTR         nodeHead;
    gcsCOMPOSITION_NODE_PTR         nodeTail;

    /* Pointer to the previous/next sets. */
    gcsCOMPOSITION_SET_PTR          prev;
    gcsCOMPOSITION_SET_PTR          next;
}
gcsCOMPOSITION_SET;

/* Composition state buffer. */
typedef struct _gcsCOMPOSITION_STATE_BUFFER * gcsCOMPOSITION_STATE_BUFFER_PTR;
typedef struct _gcsCOMPOSITION_STATE_BUFFER
{
    gctSIGNAL                       signal;

    gctSIZE_T                       size;
    gctPHYS_ADDR                    physical;
    gctUINT32                       address;
    gctUINT32_PTR                   logical;
    gctUINT                         reserve;

    gctUINT32_PTR                   head;
    gctUINT32_PTR                   tail;
    gctSIZE_T                       available;
    gctUINT                         count;
    gctUINT32_PTR                   rectangle;

    gcsCOMPOSITION_STATE_BUFFER_PTR next;
}
gcsCOMPOSITION_STATE_BUFFER;

/* Composition states. */
typedef struct _gcsCOMPOSITION_STATE
{
    /* State that indicates whether we are in the composition mode. */
    gctBOOL                         enabled;

    /* Shader parameters. */
    gctUINT                         maxConstCount;
    gctUINT                         maxShaderLength;
    gctUINT                         constBase;
    gctUINT                         instBase;

    /* Composition state buffer. */
    gcsCOMPOSITION_STATE_BUFFER     compStateBuffer[gcdCOMP_BUFFER_COUNT];
    gcsCOMPOSITION_STATE_BUFFER_PTR compStateBufferCurrent;

    /* Allocated structures. */
    gcsCONTAINER                    freeSets;
    gcsCONTAINER                    freeNodes;
    gcsCONTAINER                    freeLayers;

    /* User signals. */
    gctHANDLE                       process;
    gctSIGNAL                       signal1;
    gctSIGNAL                       signal2;

    /* Current states. */
    gctBOOL                         synchronous;
    gctBOOL                         initDone;
    gcsSURF_INFO_PTR                target;

    /* Size of hardware event command. */
    gctUINT                         eventSize;

    /* Temporary surface for blurring. */
    gcsSURF_INFO                    tempBuffer;
    gcsCOMPOSITION_LAYER            tempLayer;

    /* The list of layer sets to be composed. */
    gcsCOMPOSITION_SET              head;
}
gcsCOMPOSITION_STATE;
#endif

#if gcdSYNC
typedef enum{
    gcvFENCE_RLV,
    gcvFENCE_OQ,
    gcvFENCE_HW,
}gctFENCE;

struct _gcoFENCE
{
    gctBOOL                     fenceEnable;
    gctUINT64                   fenceID;
    gctUINT64                   fenceIDSend;
    gctUINT64                   commitID;
    gctBOOL                     addSync;
    gctUINT64                   resetTimeStamp;
    gctUINT                     loopCount;
    gctUINT                     delayCount;

    gctFENCE                    type;
    union{
        struct{
            gcoSURF             fenceSurface;
            gcoSURF             srcIDSurface;
            gctUINT32           srcWidth,srcHeight;
            gctUINT32           srcX,srcY;
            gctUINT32           srcOffset;
        }rlvFence;

        struct{
            gcoSURF             dstFenceSurface;
            gctUINT32           dstWidth;
            gctUINT32           dstHeight;
            gctUINT32           dstSlotSize;
            gctUINT32           nextSlot;
            gctUINT32           commandSlot;
            gctUINT32           commitSlot;
        }oqFence;

        struct{
            gcoSURF             dstSurface;
            gctPOINTER          dstAddr;
            gctUINT32           dstPhysic;
        }hwFence;
    }u;
};
#endif

typedef struct _gcsHARDWARE_CONFIG
{
    /* Chip registers. */
    gceCHIPMODEL                chipModel;
    gctUINT32                   chipRevision;
    gctUINT32                   chipFeatures;
    gctUINT32                   chipMinorFeatures;
    gctUINT32                   chipMinorFeatures1;
    gctUINT32                   chipMinorFeatures2;
    gctUINT32                   chipMinorFeatures3;
    gctUINT32                   chipMinorFeatures4;
    gctUINT32                   chipMinorFeatures5;
    gctUINT32                   chipSpecs;
    gctUINT32                   chipSpecs2;
    gctUINT32                   chipSpecs3;
    gctUINT32                   chipSpecs4;

    /* Data extracted from specs bits. */
#if gcdENABLE_3D
    /* gcChipSpecs. */
    gctUINT32                   streamCount;    /* number of vertex streams */
    gctUINT32                   registerMax;
    gctUINT32                   threadCount;
    gctUINT32                   vertexCacheSize;
    gctUINT32                   shaderCoreCount;
#endif
    gctUINT32                   pixelPipes;
#if gcdENABLE_3D
    gctUINT32                   vertexOutputBufferSize;
    /* gcChipSpecs2. */
    gctUINT32                   bufferSize;     /* not used? */
    gctUINT32                   instructionCount;
    gctUINT32                   numConstants;
    /* gcChipSpecs3. */
#if gcdMULTI_GPU
    gctUINT32                   gpuCoreCount;
#endif
    gctUINT32                   varyingsCount;
    gctUINT32                   localStorageSize;
    gctUINT32                   l1CacheSize;
    /* gcChipSpecs4. */
    gctUINT32                   instructionMemory;
    gctUINT32                   shaderPCLength;
    /*gctUINT32                   streamCount1;     number of streams */

    /* Derived data. */
    gctUINT                     unifiedConst;
    gctUINT                     vsConstBase;
    gctUINT                     psConstBase;
    gctUINT                     vsConstMax;
    gctUINT                     psConstMax;
    gctUINT                     constMax;

    gctUINT                     unifiedShader;
    gctUINT                     vsInstBase;
    gctUINT                     psInstBase;
    gctUINT                     vsInstMax;
    gctUINT                     psInstMax;
    gctUINT                     instMax;

    gctUINT32                   superTileMode;

    /* Info not in features/specs. */
#endif
#if gcdENABLE_3D || gcdENABLE_VG
    gctUINT32                   renderTargets; /* num of mRT */
#endif
    /* Special control bits for 2D chip. */
    gctUINT32                   chip2DControl;

    gctUINT32                   productID;

    gceECO_FLAG                 ecoFlags;
}
gcsHARDWARE_CONFIG;


#if gcdENABLE_3D

#define gcmMAX_VARIATION_NUM 32

#if !gcdSTATIC_LINK

typedef struct _gscVSC_API
{
    gceSTATUS (*gcSHADER_Construct)(gcoHAL, gctINT, gcSHADER *);

    gceSTATUS (*gcLinkShaders)(gcSHADER, gcSHADER, gceSHADER_FLAGS,
                               gctUINT32 *, gctPOINTER *, gcsHINT_PTR * );

    gceSTATUS (*gcSHADER_AddAttribute)(gcSHADER Shader, gctCONST_STRING Name,
                                       gcSHADER_TYPE Type, gctSIZE_T Length,
                                       gctBOOL IsTexture, gcSHADER_SHADERMODE ShaderMode,
                                       gcATTRIBUTE * Attribute);

    gceSTATUS (*gcSHADER_AddUniform)(gcSHADER Shader, gctCONST_STRING Name,
                                     gcSHADER_TYPE Type, gctSIZE_T Length,
                                     gcUNIFORM * Uniform);

    gceSTATUS (*gcSHADER_AddOpcode)(gcSHADER Shader, gcSL_OPCODE Opcode,
                                    gctUINT16 TempRegister, gctUINT8 Enable,
                                    gcSL_FORMAT Format);

    gceSTATUS (*gcSHADER_AddOpcodeConditional)(gcSHADER Shader, gcSL_OPCODE Opcode,
                                               gcSL_CONDITION Condition, gctUINT Label);

    gceSTATUS (*gcSHADER_AddSourceUniform)(gcSHADER Shader, gcUNIFORM Uniform,
                                        gctUINT8 Swizzle, gctINT Index);

    gceSTATUS (*gcSHADER_AddSourceAttribute)(gcSHADER Shader, gcATTRIBUTE Attribute,
                                          gctUINT8 Swizzle, gctINT Index);

    gceSTATUS (*gcSHADER_AddSourceConstant)(gcSHADER Shader, gctFLOAT Constant);

    gceSTATUS (*gcSHADER_AddOutput)(gcSHADER Shader, gctCONST_STRING Name,
                                 gcSHADER_TYPE Type, gctSIZE_T Length,
                                 gctUINT16 TempRegister);

    gceSTATUS (*gcSHADER_AddLocation)(gcSHADER Shader, gctINT Location, gctSIZE_T Length);

    gceSTATUS (*gcSHADER_SetCompilerVersion)(gcSHADER Shader, gctUINT32 *Version);

    gceSTATUS (*gcSHADER_Pack)(gcSHADER);

    gceSTATUS (*gcSHADER_Destroy)(gcSHADER);

    gceSTATUS (*gcSHADER_Copy)(gcSHADER, gcSHADER);

    gceSTATUS (*gcSHADER_DynamicPatch)(gcSHADER, gcPatchDirective *);

    gceSTATUS (*gcCreateOutputConversionDirective)(gctINT, gcsSURF_FORMAT_INFO_PTR, gcPatchDirective  **);

    gceSTATUS (*gcCreateInputConversionDirective)(gcUNIFORM,
                                                  gcsSURF_FORMAT_INFO_PTR,
                                                  gctCONST_STRING,
                                                  gceTEXTURE_SWIZZLE *,
                                                  gcPatchDirective  **);

    void (*gcSetGLSLCompiler)(gctGLSLCompiler Compiler);

    gceSTATUS (*gcCompileShader)(gcoHAL Hal,
                                 gctINT ShaderType,
                                 gctUINT SourceSize,
                                 gctCONST_STRING Source,
                                 gcSHADER * Binary,
                                 gctSTRING * Log);
}gcsVSC_API;

#endif


typedef struct _gcsPROGRAM_STATE
{
    /* Shader program state buffer. */
    gctUINT32                programSize;
    gctPOINTER               programBuffer;
    gcsHINT_PTR              hints;
}
gcsPROGRAM_STATE;

/* All after-link state should be here */
typedef struct _gcsPROGRAM_STATE_VARIATION
{
    gcsPROGRAM_STATE programState;
    /* key for this variation */
    gceSURF_FORMAT srcFmt;
    gceSURF_FORMAT destFmt;

    /* LRU list, not used for now */
    gctINT32 prev;
    gctINT32 next;
}
gcsPROGRAM_STATE_VARIATION;

typedef struct _gcsHARDWARE_BLITDRAW
{
    gcSHADER vsShader[gcvBLITDRAW_NUM_TYPE];

    gcSHADER psShader[gcvBLITDRAW_NUM_TYPE];

    /* sampler uniform for blit shader */
    gcUNIFORM  bliterSampler;

    /* color uniform for clear shader */
    gcUNIFORM  clearColorUnfiorm;

    /* stream pointer.*/
    gcoSTREAM dynamicStream;

    gcsPROGRAM_STATE_VARIATION programState[gcvBLITDRAW_NUM_TYPE][gcmMAX_VARIATION_NUM];

#if !gcdSTATIC_LINK

    gcsVSC_API vscAPI;

    gctHANDLE vscLib;
    gctHANDLE glslcLib;
#endif
}
gcsHARDWARE_BLITDRAW, * gcsHARDWARE_BLITDRAW_PTR;
#endif


#if gcdSTREAM_OUT_BUFFER

typedef enum {

    gcvSTREAM_OUT_STATUS_DISABLED = 0,
    gcvSTREAM_OUT_STATUS_PHASE_1,
    gcvSTREAM_OUT_STATUS_PHASE_1_PREPASS,
    gcvSTREAM_OUT_STATUS_PHASE_2

} gceSTREAM_OUT_STATUS;

typedef enum {

    gcvSTREAM_OUT_CHIP_LAYER_STATUS_DISABLED = 0,
    gcvSTREAM_OUT_CHIP_LAYER_STATUS_PREPASS,
    gcvSTREAM_OUT_CHIP_LAYER_STATUS_REPLAY,
    gcvSTREAM_OUT_CHIP_LAYER_STATUS_PREPASS_AND_REPLAY

} gceSTREAM_OUT_CHIP_LAYER_STATUS;

typedef struct _gcoSTREAM_OUT_INDEX
{
    gctUINT32                       streamOutIndexAddress[(gcdMULTI_GPU > 0 ? gcdMULTI_GPU : 1)];
    gcsSURF_NODE                    streamOutIndexNode[(gcdMULTI_GPU > 0 ? gcdMULTI_GPU : 1)];
    gctUINT32                       originalAddress;
    gctUINT32                       originalCount;
    gctUINT32                       originalOffset;
    gctBOOL                         usedOnce;
    struct _gcoSTREAM_OUT_INDEX *   next;

} gcoSTREAM_OUT_INDEX;

typedef struct _gcoSTREAM_OUT_INDEX_CACHE
{
    gcoSTREAM_OUT_INDEX *       head;
    gcoSTREAM_OUT_INDEX *       tail;
    gctUINT                     count;

} gcoSTREAM_OUT_INDEX_CACHE;

#endif

/* gcoHARDWARE object. */
struct _gcoHARDWARE
{
    /* Object. */
    gcsOBJECT                   object;

    /* Default hardware object or not for this thread */
    gctBOOL                     threadDefault;

    /* Handle of gckCONTEXT object. */
    gctUINT32                   context;

    /* Number of states managed by the context manager. */
    gctUINT32                   stateCount;

    /* Command buffer. */
    gcoBUFFER                   buffer;

#if gcdSTREAM_OUT_BUFFER
    gctBOOL                     streamOutInit;
    gcoQUEUE                    streamOutEventQueue;
    gcoSTREAM_OUT_INDEX_CACHE   streamOutIndexCache;
    gceSTREAM_OUT_STATUS        streamOutStatus;
    gctUINT32                   streamOutOriginalIndexAddess;
    gctUINT32                   streamOutOriginalIndexOffset;
    gctUINT32                   streamOutOriginalIndexCount;
    gctBOOL                     streamOutDirty;
    gctBOOL                     streamOutNeedSync;
#endif

    /* Context buffer. */
    gcePIPE_SELECT              currentPipe;

    /* Event queue. */
    gcoQUEUE                    queue;

    /* List of state delta buffers. */
    gcsSTATE_DELTA_PTR          delta;

    /* Chip configuration. */
#if gcdUSE_HARDWARE_CONFIGURATION_TABLES
    gcsHARDWARE_CONFIG *        config;
    gctBOOL *                   features;
    gctBOOL *                   swwas;
#else
    gcsHARDWARE_CONFIG          _config;
    gcsHARDWARE_CONFIG *        config;
    gctBOOL                     features[gcvFEATURE_COUNT];
    gctBOOL                     swwas[gcvSWWA_COUNT];
#endif

#if gcdENABLE_3D
    gctUINT                     unifiedConst;
    gctUINT                     vsConstBase;
    gctUINT                     psConstBase;
    gctUINT                     vsConstMax;
    gctUINT                     psConstMax;
    gctUINT                     constMax;

    gctUINT                     unifiedShader;
    gctUINT                     vsInstBase;
    gctUINT                     psInstBase;
    gctUINT                     vsInstMax;
    gctUINT                     psInstMax;
    gctUINT                     instMax;
#if gcdMULTI_GPU
    gctUINT32                   gpuCoreCount;
    gceMULTI_GPU_RENDERING_MODE gpuRenderingMode;
    gceCORE_3D_MASK             chipEnable;
    gceMULTI_GPU_MODE           gpuMode;
    gctUINT                     interGpuSemaphoreId;
#endif
#endif

    gctINT                      needStriping;
    gctBOOL                     mixedStreams;

    gctUINT32                   colorOutCount; /* num of mRT */

    /* Big endian */
    gctBOOL                     bigEndian;

    /* Special hint flag */
    gctUINT32                   specialHint;

    gctINT                      specialHintData;

#if gcdENABLE_3D
    /* API type. */
    gceAPI                      api;

    /* Current API type. */
    gceAPI                      currentApi;
#endif

    /* Temporary buffer parameters. */
    struct _gcsSURF_INFO        tempBuffer;

    /* Temporary render target for MSAA/depth-only case */
    struct _gcsSURF_INFO        tempRT;

    /* Temporary surface for 2D. */
    gcsSURF_INFO_PTR            temp2DSurf[gcdTEMP_SURFACE_NUMBER];

    /* Filter blit. */
    struct __gcsLOADED_KERNEL_INFO
    {
        gceFILTER_TYPE              type;
        gctUINT8                    kernelSize;
        gctUINT32                   scaleFactor;
        gctUINT32                   kernelAddress;
    } loadedKernel[gcvFILTER_BLIT_KERNEL_NUM];

#if gcdENABLE_3D
    /* Flush state. */
    gctBOOL                     flushedColor;
    gctBOOL                     flushedDepth;

    /* Render target states. */
    gctBOOL                     colorConfigDirty;
    gctBOOL                     colorTargetDirty;
    gcsCOLOR_INFO               colorStates;

    /* Depth states. */
    gctBOOL                     depthConfigDirty;
    gctBOOL                     depthRangeDirty;
    gctBOOL                     depthNormalizationDirty;
    gctBOOL                     depthTargetDirty;
    gcsDEPTH_INFO               depthStates;
    gctBOOL                     disableAllEarlyDepth;
    gctBOOL                     disableRAModifyZ;
    gctBOOL                     disableRAPassZ;
    gctBOOL                     disableAllEarlyDepthFromStatistics;
    gctBOOL                     previousPEDepth;
    gctBOOL                     prevEarlyZ;
    gceCOMPARE                  prevDepthCompare;

    /* True if either one RT or DS bpp is <= 16 */
    gctBOOL                     singleBuffer8x4;

    /* Primitive assembly states. */
    gctBOOL                     paConfigDirty;
    gctBOOL                     paLineDirty;
    gcsPA_INFO                  paStates;

    /* Depth clipping plane in the clipping space. */
    gcuFLOAT_UINT32             depthNear;
    gcuFLOAT_UINT32             depthFar;

    /* Maximum depth value. */
    gctUINT32                   maxDepth;
    gctBOOL                     earlyDepth;

    /* Stencil states. */
    gctBOOL                     stencilEnabled;
    gctBOOL                     stencilKeepFront[3];
    gctBOOL                     stencilKeepBack[3];

    gctBOOL                     stencilDirty;
    gcsSTENCIL_INFO             stencilStates;
    gctINT                      prevStencilMode;

    /* Alpha states. */
    gctBOOL                     alphaDirty;
    gcsALPHA_INFO               alphaStates;

    /* Viewport/scissor states. */
    gctBOOL                     viewportDirty;
    gcsRECT                     viewportStates;

    gctBOOL                     scissorDirty;
    gcsRECT                     scissorStates;

    /* Shader states. */
    gctBOOL                     shaderDirty;
    gcsSHADER_INFO              shaderStates;
    gcsPROGRAM_UNIFIED_STATUS   prevProgramUnfiedStatus;

    gctUINT32                   samplerDirty;
    gctBOOL                     textureDirty;

    /* Rounding mode */
    gctBOOL                     rtneRounding;

    gcsHARDWARE_BLITDRAW_PTR    blitDraw;

#endif /* gcdENABLE_3D */

    /* Stall before rendingering triangles. */
    gctBOOL                     stallPrimitive;

#if gcdENABLE_3D
    /* Tile status information. */
    gctUINT32                   memoryConfig;
    gctUINT32                   memoryConfigMRT[gcdMAX_DRAW_BUFFERS];
    gctBOOL                     paused;
    gctBOOL                     cacheDirty;
    gctBOOL                     inFlush;

    /* Anti-alias mode. */
    gctBOOL                     msaaConfigDirty;
    gctBOOL                     msaaModeDirty;
    gctUINT32                   sampleMask;
    gctUINT32                   sampleEnable;
    gcsSAMPLES                  samples;
    gceVAA                      vaa;
    gctUINT32                   vaa8SampleCoords;
    gctUINT32                   vaa16SampleCoords;
    gctUINT32                   jitterIndex;
    gctUINT32                   sampleCoords2;
    gctUINT32                   sampleCoords4[3];
    gcsCENTROIDS                centroids2;
    gcsCENTROIDS                centroids4[3];
    gctBOOL                     centroidsDirty;

    /* Dither. */
    gctUINT32                   ditherTable[2][2];
    gctBOOL                     peDitherDirty;
    gctUINT32                   ditherEnable;

    /* Occlusion Query */
    gctBOOL                     enableOQ;

    gctBOOL                     indexDirty;
    gctBOOL                     indexHeadDirty;
    gctBOOL                     indexTailDirty;
    gctUINT32                   indexHeadAddress;
    gctUINT32                   indexTailAddress;
    gctUINT32                   indexFormat;
    gctUINT32                   indexEndian;
    gctBOOL                     primitiveRestart;
    gctUINT32                   restartElement;

    /* ps output-> RT mapping
    ** in:  sequential number from 0 to numRT-1;
    ** out: RT index set up by APP.
    */
    gctINT32                    psOutputMapping[gcdMAX_DRAW_BUFFERS];


    /* Flush shader L1 cache state */
    gctBOOL                     flushSHL1;
#endif

    /* Stall signal. */
    gctSIGNAL                   stallSignal;

#if gcdENABLE_3D
    /* Multi-pixel pipe information. */
    gctUINT32                   resolveAlignmentX;
    gctUINT32                   resolveAlignmentY;
    gctBOOL                     multiPipeResolve;

    /* Texture states. */
    gctBOOL                     hwTxDirty;
    gctBOOL                     hwTxFlushVS;
    gctBOOL                     hwTxFlushPS;

#if gcdSECURITY
    gctUINT32                   txLodNums[gcdSAMPLERS];
#endif

    gctUINT32                   hwTxSamplerMode[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerModeEx[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerMode2[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerModeDirty;

    gctUINT32                   hwTxSamplerSize[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerSizeDirty;

    gctUINT32                   hwTxSamplerSizeLog[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerSizeLogDirty;

    gctUINT32                   hwTxSampler3D[gcdSAMPLERS];
    gctUINT32                   hwTxSampler3DDirty;

    gctUINT32                   hwTxSamplerLOD[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerLODDirty;

    gctUINT32                   hwTxBaseLOD[gcdSAMPLERS];
    gctUINT32                   hwTxBaseLODDirty;

    gctUINT32                   hwTxSamplerAddress[gcdLOD_LEVELS][gcdSAMPLERS];
    gctUINT32                   hwTxSamplerAddressDirty[gcdLOD_LEVELS];

    gctUINT32                   hwTxSamplerYUVControl[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerYUVControlDirty;

    gctUINT32                   hwTxSamplerYUVStride[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerYUVStrideDirty;

    gctUINT32                   hwTxSamplerLinearStride[gcdSAMPLERS];
    gctUINT32                   hwTxSamplerLinearStrideDirty;

    gctUINT32                   hwTXSampleTSConfig[gcdSAMPLER_TS];
    gctUINT32                   hwTXSampleTSBuffer[gcdSAMPLER_TS];
    gctUINT32                   hwTXSampleTSClearValue[gcdSAMPLER_TS];
    gctUINT32                   hwTXSampleTSClearValueUpper[gcdSAMPLER_TS];
    gctUINT32                   hwTxSamplerTxBaseBuffer[gcdSAMPLER_TS];
    gctUINT32                   hwTxSamplerTSDirty;

    gctUINT32                   hwTxSamplerASTCSize[gcdLOD_LEVELS][gcdSAMPLERS];
    gctUINT32                   hwTxSamplerASTCSRGB[gcdLOD_LEVELS][gcdSAMPLERS];
    gctUINT32                   hwTxSamplerASTCDirty[gcdLOD_LEVELS];

    /* State to collect flushL2 requests, and process it at draw time. */
    gctBOOL                     flushL2;

#endif

    /***************************************************************************
    ** 2D states.
    */

    /* 2D hardware availability flag. */
    gctBOOL                     hw2DEngine;

    /* 3D hardware availability flag. */
    gctBOOL                     hw3DEngine;

    /* Software 2D force flag. */
    gctBOOL                     sw2DEngine;

    /* Byte write capability. */
    gctBOOL                     byteWrite;

    /* Need to shadow RotAngleReg? */
    gctBOOL                     shadowRotAngleReg;

    /* The shadow value. */
    gctUINT32                   rotAngleRegShadow;

    /* Support L2 cache for 2D 420 input. */
    gctBOOL                     hw2D420L2Cache;
    gctBOOL                     needOffL2Cache;

#if gcdENABLE_3D
    /* OpenCL threadwalker in PS. */
    gctBOOL                     threadWalkerInPS;

    /* Composition states. */
    gcsCOMPOSITION_STATE        composition;

    /* Composition engine support. */
    gctBOOL                     hwComposition;
#endif

    /* Temp surface for fast clear */
    gcoSURF                     tempSurface;

    /* Blt optimization. */
    gceSURF_ROTATION            srcRot;
    gceSURF_ROTATION            dstRot;
    gctBOOL                     horMirror;
    gctBOOL                     verMirror;
    gcsRECT                     clipRect;
    gctBOOL                     srcRelated;

    gctUINT32_PTR               hw2DCmdBuffer;
    gctUINT32                   hw2DCmdIndex;
    gctUINT32                   hw2DCmdSize;

    gctBOOL                     hw2DSplitRect;

    gctUINT32                   hw2DAppendCacheFlush;
    gctUINT32                   hw2DCacheFlushAfterCompress;
    gctUINT32_PTR               hw2DCacheFlushCmd;
    gcsSURF_INFO_PTR            hw2DCacheFlushSurf;

    gcsSURF_INFO_PTR            hw2DClearDummySurf;
    gcsRECT_PTR                 hw2DClearDestRect;

    gctBOOL                     hw2DCurrentRenderCompressed;
    gcsSURF_INFO_PTR            hw2DDummySurf;

    gctBOOL                     hw2DDoMultiDst;

    gctBOOL                     hw2DBlockSize;

    gctBOOL                     hw2DQuad;

    gctUINT32                   mmuVersion;
#if gcdENABLE_3D
    gcePATCH_ID                 patchID;
#endif

#if gcdSYNC
    gcoFENCE                    fence;
    gctBOOL                     fenceEnabled;
#endif

   /* XRGB */
    gctBOOL                     enableXRGB;

    gcsSURF_INFO_PTR            clearSrcSurf;
    gcsRECT                     clearSrcRect;

    gctBOOL                     notAdjustRotation;

#if gcdDUMP
    gctUINT32                   contextPhysical[gcdCONTEXT_BUFFER_COUNT];
    gctPOINTER                  contextLogical[gcdCONTEXT_BUFFER_COUNT];
    gctUINT32                   contextBytes;
    gctUINT8                    currentContext;
#endif
};

#if gcdENABLE_3D
gceSTATUS
gcoHARDWARE_ComputeCentroids(
    IN gcoHARDWARE Hardware,
    IN gctUINT Count,
    IN gctUINT32_PTR SampleCoords,
    OUT gcsCENTROIDS_PTR Centroids
    );

#if gcdMULTI_GPU

#if gcdSTREAM_OUT_BUFFER
gceSTATUS
gcoHARDWARE_MultiGPUSemaphoreId (
    IN gcoHARDWARE Hardware,
    OUT gctUINT8_PTR SemaphoreId
    );
#endif

#endif

/* Flush the vertex caches. */
gceSTATUS
gcoHARDWARE_FlushL2Cache(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushViewport(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushScissor(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushAlpha(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushStencil(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushTarget(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushDepth(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushSampling(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushPA(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FlushDepthOnly(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_FlushShaders(
    IN gcoHARDWARE Hardware,
    IN gcePRIMITIVE PrimitiveType,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastFlushUniforms(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastFlushStream(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastProgramIndex(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastFlushAlpha(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastFlushDepthCompare(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_FastDrawIndexedPrimitive(
    IN gcoHARDWARE Hardware,
    IN gcsFAST_FLUSH_PTR FastFlushInfo,
    INOUT gctPOINTER *Memory
    );

#if gcdSTREAM_OUT_BUFFER

gceSTATUS
gcoHARDWARE_QueryStreamOutIndex(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 IndexAddress,
    IN gctUINT32 IndexOffset,
    IN gctUINT32 IndexCount,
    OUT gctBOOL_PTR Found
);

gceSTATUS gcoHARDWARE_FlushStreamOut(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS gcoHARDWARE_StopSignalStreamOut(
    IN gcoHARDWARE Hardware
    );

gceSTATUS gcoHARDWARE_ProgramIndexStreamOut(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 CoreIndex,
    IN gcoSTREAM_OUT_INDEX * Index,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_SyncStreamOut (
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_AddStreamOutIndex (
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Bytes
    );

gceSTATUS
gcoHARDWARE_GetStreamOutIndex (
    IN gcoHARDWARE Hardware,
    OUT gcoSTREAM_OUT_INDEX ** Index
    );

gceSTATUS
gcoHARDWARE_FreeStreamOutIndex (
    IN gcoHARDWARE Hardware
    );

#endif

gceSTATUS gcoHARDWARE_ProgramIndex(
    IN gcoHARDWARE Hardware,
    INOUT gctPOINTER *Memory
    );

gceSTATUS
gcoHARDWARE_InitializeComposition(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_DestroyComposition(
    IN gcoHARDWARE Hardware
    );
#endif

gceSTATUS
gcoHARDWARE_Load2DState32(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

gceSTATUS
gcoHARDWARE_Load2DState(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Count,
    IN gctPOINTER Data
    );

/* Set clipping rectangle. */
gceSTATUS
gcoHARDWARE_SetClipping(
    IN gcoHARDWARE Hardware,
    IN gcsRECT_PTR Rect
    );

/* Translate API source color format to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateSourceFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT APIValue,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    );

/* Translate API format to internal format . */
gceSTATUS gcoHARDWARE_TranslateXRGBFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT InputFormat,
    OUT gceSURF_FORMAT* OutputFormat
    );

/* Translate API pattern color format to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslatePatternFormat(
    IN gcoHARDWARE Hardware,
    IN gceSURF_FORMAT APIValue,
    OUT gctUINT32* HwValue,
    OUT gctUINT32* HwSwizzleValue,
    OUT gctUINT32* HwIsYUVValue
    );

/* Translate API transparency mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateTransparency(
    IN gceSURF_TRANSPARENCY APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API transparency mode to its PE 1.0 hardware value. */
gceSTATUS
gcoHARDWARE_TranslateTransparencies(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 srcTransparency,
    IN gctUINT32 dstTransparency,
    IN gctUINT32 patTransparency,
    OUT gctUINT32* HwValue
    );

/* Translate API transparency mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateSourceTransparency(
    IN gce2D_TRANSPARENCY APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API rotation mode to its hardware value. */
gceSTATUS gcoHARDWARE_TranslateSourceRotation(
    IN gceSURF_ROTATION APIValue,
    OUT gctUINT32 * HwValue
    );

gceSTATUS gcoHARDWARE_TranslateDestinationRotation(
    IN gceSURF_ROTATION APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API transparency mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateDestinationTransparency(
    IN gce2D_TRANSPARENCY APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API transparency mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslatePatternTransparency(
    IN gce2D_TRANSPARENCY APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API DFB color key mode to its hardware value. */
gceSTATUS gcoHARDWARE_TranslateDFBColorKeyMode(
    IN  gctBOOL APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API pixel color multiply mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslatePixelColorMultiplyMode(
    IN  gce2D_PIXEL_COLOR_MULTIPLY_MODE APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API global color multiply mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateGlobalColorMultiplyMode(
    IN  gce2D_GLOBAL_COLOR_MULTIPLY_MODE APIValue,
    OUT gctUINT32 * HwValue
    );

/* Translate API mono packing mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateMonoPack(
    IN gceSURF_MONOPACK APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API 2D command to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateCommand(
    IN gce2D_COMMAND APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API per-pixel alpha mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslatePixelAlphaMode(
    IN gceSURF_PIXEL_ALPHA_MODE APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API global alpha mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateGlobalAlphaMode(
    IN gceSURF_GLOBAL_ALPHA_MODE APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API per-pixel color mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslatePixelColorMode(
    IN gceSURF_PIXEL_COLOR_MODE APIValue,
    OUT gctUINT32* HwValue
    );

/* Translate API alpha factor mode to its hardware value. */
gceSTATUS
gcoHARDWARE_TranslateAlphaFactorMode(
    IN gcoHARDWARE Hardware,
    IN  gctBOOL IsSrcFactor,
    IN  gceSURF_BLEND_FACTOR_MODE APIValue,
    OUT gctUINT32_PTR HwValue,
    OUT gctUINT32_PTR FactorExpansion
    );

/* Configure monochrome source. */
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
    );

/* Configure color source. */
gceSTATUS
gcoHARDWARE_SetColorSource(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CoordRelative,
    IN gctUINT32 Transparency,
    IN gce2D_YUV_COLOR_MODE Mode,
    IN gctBOOL DeGamma,
    IN gctBOOL Filter
    );

/* Configure masked color source. */
gceSTATUS
gcoHARDWARE_SetMaskedSource(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL CoordRelative,
    IN gceSURF_MONOPACK MaskPack,
    IN gctUINT32 Transparency
    );

/* Setup the source rectangle. */
gceSTATUS
gcoHARDWARE_SetSource(
    IN gcoHARDWARE Hardware,
    IN gcsRECT_PTR SrcRect
    );

/* Setup the fraction of the source origin for filter blit. */
gceSTATUS
gcoHARDWARE_SetOriginFraction(
    IN gcoHARDWARE Hardware,
    IN gctUINT16 HorFraction,
    IN gctUINT16 VerFraction
    );

/* Load 256-entry color table for INDEX8 source surfaces. */
gceSTATUS
gcoHARDWARE_LoadPalette(
    IN gcoHARDWARE Hardware,
    IN gctUINT FirstIndex,
    IN gctUINT IndexCount,
    IN gctPOINTER ColorTable,
    IN gctBOOL ColorConvert,
    IN gceSURF_FORMAT DstFormat,
    IN OUT gctBOOL *Program,
    IN OUT gceSURF_FORMAT *ConvertFormat
    );

/* Setup the source global color value in ARGB8 format. */
gceSTATUS
gcoHARDWARE_SetSourceGlobalColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Color
    );

/* Setup the target global color value in ARGB8 format. */
gceSTATUS
gcoHARDWARE_SetTargetGlobalColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Color
    );

/* Setup the source and target pixel multiply modes. */
gceSTATUS
gcoHARDWARE_SetMultiplyModes(
    IN gcoHARDWARE Hardware,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE SrcPremultiplySrcAlpha,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE DstPremultiplyDstAlpha,
    IN gce2D_GLOBAL_COLOR_MULTIPLY_MODE SrcPremultiplyGlobalMode,
    IN gce2D_PIXEL_COLOR_MULTIPLY_MODE DstDemultiplyDstAlpha,
    IN gctUINT32 SrcGlobalColor
    );

/*
 * Set the transparency for source, destination and pattern.
 * It also enable or disable the DFB color key mode.
 */
gceSTATUS
gcoHARDWARE_SetTransparencyModesEx(
    IN gcoHARDWARE Hardware,
    IN gce2D_TRANSPARENCY SrcTransparency,
    IN gce2D_TRANSPARENCY DstTransparency,
    IN gce2D_TRANSPARENCY PatTransparency,
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN gctBOOL EnableDFBColorKeyMode
    );

/* Setup the source, target and pattern transparency modes.
   Used only for have backward compatibility.
*/
gceSTATUS
gcoHARDWARE_SetAutoTransparency(
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop
    );

/* Setup the source color key value in ARGB8 format. */
gceSTATUS
gcoHARDWARE_SetSourceColorKeyRange(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorLow,
    IN gctUINT32 ColorHigh,
    IN gctBOOL ColorPack,
    IN gceSURF_FORMAT SrcFormat
    );

gceSTATUS
gcoHARDWARE_SetMultiSource(
    IN gcoHARDWARE Hardware,
    IN gctUINT RegGroupIndex,
    IN gctUINT SrcIndex,
    IN gcs2D_State_PTR State
    );

gceSTATUS
gcoHARDWARE_SetMultiSourceEx(
    IN gcoHARDWARE Hardware,
    IN gctUINT RegGroupIndex,
    IN gctUINT SrcIndex,
    IN gcs2D_State_PTR State,
    IN gctBOOL MultiDstRect
    );

/* Configure destination. */
gceSTATUS
gcoHARDWARE_SetTarget(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gctBOOL Filter,
    IN gce2D_YUV_COLOR_MODE Mode,
    IN gctINT32_PTR CscRGB2YUV,
    IN gctUINT32_PTR GammaTable,
    IN gctBOOL GdiStretch,
    OUT gctUINT32_PTR DestConfig
    );

gceSTATUS gcoHARDWARE_SetMultiTarget(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR Surface,
    IN gceSURF_FORMAT SrcFormat
    );

/* Setup the destination color key value in ARGB8 format. */
gceSTATUS
gcoHARDWARE_SetTargetColorKeyRange(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 ColorLow,
    IN gctUINT32 ColorHigh
    );

/* Load solid (single) color pattern. */
gceSTATUS
gcoHARDWARE_LoadSolidColorPattern(
    IN gcoHARDWARE Hardware,
    IN gctBOOL ColorConvert,
    IN gctUINT32 Color,
    IN gctUINT64 Mask,
    IN gceSURF_FORMAT DstFormat
    );

/* Load monochrome pattern. */
gceSTATUS
gcoHARDWARE_LoadMonochromePattern(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 OriginX,
    IN gctUINT32 OriginY,
    IN gctBOOL ColorConvert,
    IN gctUINT32 FgColor,
    IN gctUINT32 BgColor,
    IN gctUINT64 Bits,
    IN gctUINT64 Mask,
    IN gceSURF_FORMAT DstFormat
    );

/* Load color pattern. */
gceSTATUS
gcoHARDWARE_LoadColorPattern(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 OriginX,
    IN gctUINT32 OriginY,
    IN gctUINT32 Address,
    IN gceSURF_FORMAT Format,
    IN gctUINT64 Mask
    );

/* Calculate and program the stretch factors. */
gceSTATUS
gcoHARDWARE_SetStretchFactors(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 HorFactor,
    IN gctUINT32 VerFactor
    );

/* Set 2D clear color in A8R8G8B8 format. */
gceSTATUS
gcoHARDWARE_Set2DClearColor(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Color,
    IN gceSURF_FORMAT DstFormat
    );

/* Enable/disable 2D BitBlt mirrorring. */
gceSTATUS
gcoHARDWARE_SetBitBlitMirror(
    IN gcoHARDWARE Hardware,
    IN gctBOOL HorizontalMirror,
    IN gctBOOL VerticalMirror,
    IN gctBOOL DstMirror
    );

/* Enable alpha blending engine in the hardware and disengage the ROP engine. */
gceSTATUS
gcoHARDWARE_EnableAlphaBlend(
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
    );

/* Disable alpha blending engine in the hardware and engage the ROP engine. */
gceSTATUS
gcoHARDWARE_DisableAlphaBlend(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_ColorConvertToARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 NumColors,
    IN gctUINT32_PTR Color,
    OUT gctUINT32_PTR Color32
    );

gceSTATUS
gcoHARDWARE_ColorConvertFromARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 NumColors,
    IN gctUINT32_PTR Color32,
    OUT gctUINT32_PTR Color
    );

gceSTATUS
gcoHARDWARE_ColorPackFromARGB8(
    IN gceSURF_FORMAT Format,
    IN gctUINT32 Color32,
    OUT gctUINT32_PTR Color
    );

/* Convert pixel format. */
gceSTATUS
gcoHARDWARE_ConvertPixel(
    IN gctPOINTER SrcPixel,
    OUT gctPOINTER TrgPixel,
    IN gctUINT SrcBitOffset,
    IN gctUINT TrgBitOffset,
    IN gcsSURF_FORMAT_INFO_PTR SrcFormat,
    IN gcsSURF_FORMAT_INFO_PTR TrgFormat,
    IN OPTIONAL gcsBOUNDARY_PTR SrcBoundary,
    IN OPTIONAL gcsBOUNDARY_PTR TrgBoundary,
    IN gctBOOL SrcPixelOdd,
    IN gctBOOL TrgPixelOdd
    );

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

#if gcdENABLE_3D
typedef enum _gceTILE_STATUS_CONTROL
{
    gcvTILE_STATUS_PAUSE,
    gcvTILE_STATUS_RESUME,
}
gceTILE_STATUS_CONTROL;

/* Pause or resume tile status. */
gceSTATUS
gcoHARDWARE_PauseTileStatus(
    IN gcoHARDWARE Hardware,
    IN gceTILE_STATUS_CONTROL Control
    );

/* Compute the offset of the specified pixel location. */
gceSTATUS
gcoHARDWARE_ComputeOffset(
    IN gcoHARDWARE Hardware,
    IN gctINT32 X,
    IN gctINT32 Y,
    IN gctUINT Stride,
    IN gctINT BytesPerPixel,
    IN gceTILING Tiling,
    OUT gctUINT32_PTR Offset
    );

/* Adjust cache mode to make RS work fine with MSAA config switch*/
gceSTATUS
gcoHARDWARE_AdjustCacheMode(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SrcInfo
    );


/* Query the tile size of the given surface. */
gceSTATUS
gcoHARDWARE_GetSurfaceTileSize(
    IN gcsSURF_INFO_PTR Surface,
    OUT gctINT32 * TileWidth,
    OUT gctINT32 * TileHeight
    );

/* Program the Resolve offset, Window and then trigger the resolve. */
gceSTATUS
gcoHARDWARE_ProgramResolve(
    IN gcoHARDWARE Hardware,
    IN gcsPOINT RectSize,
    IN gctBOOL  MultiPipe,
    IN gceMSAA_DOWNSAMPLE_MODE DownsampleMode
    );
#endif /* VIVANTE_NO_3D */

/* Load one 32-bit state. */
gceSTATUS
gcoHARDWARE_LoadState32(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Data
    );

gceSTATUS
gcoHARDWARE_LoadState32WithMask(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT32 Mask,
    IN gctUINT32 Data
    );

/* Load one 32-bit load state. */
gceSTATUS
gcoHARDWARE_LoadState32x(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctFIXED_POINT Data
    );

/* Load one 64-bit load state. */
gceSTATUS
gcoHARDWARE_LoadState64(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Address,
    IN gctUINT64 Data
    );

gceSTATUS
gcoHARDWARE_SetDither2D(
    IN gcoHARDWARE Hardware,
    IN gctBOOL Enable
    );

gceSTATUS gcoHARDWARE_Begin2DRender(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State
    );

gceSTATUS gcoHARDWARE_End2DRender(
     IN gcoHARDWARE Hardware
     );

gceSTATUS gcoHARDWARE_UploadCSCTable(
    IN gcoHARDWARE Hardware,
    IN gctBOOL YUV2RGB,
    IN gctINT32_PTR Table
    );

/* About Compression. */
gceSTATUS
gcoHARDWARE_GetCompressionCmdSize(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gctUINT CompressNum,
    IN gce2D_COMMAND Command,
    OUT gctUINT32 *CmdSize
    );

gceSTATUS
gcoHARDWARE_SetCompression(
    IN gcoHARDWARE Hardware,
    IN gcs2D_State_PTR State,
    IN gcsSURF_INFO_PTR SrcSurface,
    IN gcsSURF_INFO_PTR DstSurface,
    IN gce2D_COMMAND Command,
    IN gctBOOL AnyCompress
    );

/* Update the state delta. */
static gcmINLINE void gcoHARDWARE_UpdateDelta(
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gctUINT32 Address,
    IN gctUINT32 Mask,
    IN gctUINT32 Data
    )
{
    gcsSTATE_DELTA_RECORD_PTR recordArray;
    gcsSTATE_DELTA_RECORD_PTR recordEntry;
    gctUINT32_PTR mapEntryID;
    gctUINT32_PTR mapEntryIndex;
    gctUINT deltaID;

    /* Get the current record array. */
    recordArray = gcmUINT64_TO_PTR(StateDelta->recordArray);

    /* Get shortcuts to the fields. */
    deltaID       = StateDelta->id;
    mapEntryID    = gcmUINT64_TO_PTR(StateDelta->mapEntryID);
    mapEntryIndex = gcmUINT64_TO_PTR(StateDelta->mapEntryIndex);

    gcmASSERT(Address < (StateDelta->mapEntryIDSize / gcmSIZEOF(gctUINT)));

    /* Has the entry been initialized? */
    if (mapEntryID[Address] != deltaID)
    {
        /* No, initialize the map entry. */
        mapEntryID    [Address] = deltaID;
        mapEntryIndex [Address] = StateDelta->recordCount;

        /* Get the current record. */
        recordEntry = &recordArray[mapEntryIndex[Address]];

        /* Add the state to the list. */
        recordEntry->address = Address;
        recordEntry->mask    = Mask;
        recordEntry->data    = Data;

        /* Update the number of valid records. */
        StateDelta->recordCount += 1;
    }

    /* Regular (not masked) states. */
    else if (Mask == 0)
    {
        /* Get the current record. */
        recordEntry = &recordArray[mapEntryIndex[Address]];

        /* Update the state record. */
        recordEntry->mask = 0;
        recordEntry->data = Data;
    }

    /* Masked states. */
    else
    {
        /* Get the current record. */
        recordEntry = &recordArray[mapEntryIndex[Address]];

        /* Update the state record. */
        recordEntry->mask |=  Mask;
        recordEntry->data &= ~Mask;
        recordEntry->data |= (Data & Mask);
    }
}

gceSTATUS gcoHARDWARE_InitializeFormatArrayTable(
    IN gcoHARDWARE Hardware
    );

gceSTATUS
gcoHARDWARE_3DBlitCopy(
    IN gcoHARDWARE Hardware,
    IN gctUINT32 SrcAddress,
    IN gctUINT32 DestAddress,
    IN gctUINT32 CopySize
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
gcoHARDWARE_2DAppendNop(
    IN gcoHARDWARE Hardware
    );

gceSTATUS gcoHARDWARE_Reset2DCmdBuffer(
    IN gcoHARDWARE Hardware,
    IN gctBOOL CleanCmd
    );

gctBOOL
gcoHARDWARE_NeedUserCSC(
    IN gce2D_YUV_COLOR_MODE Mode,
    IN gceSURF_FORMAT Format);

gceSTATUS
gcoHARDWARE_SetSuperTileVersion(
    IN gcoHARDWARE Hardware,
    IN gce2D_SUPER_TILE_VERSION Version
    );

gceSTATUS
gcoHARDWARE_Alloc2DSurface(
    IN gcoHARDWARE Hardware,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gceSURF_FORMAT Format,
    IN gceSURF_FLAG Flags,
    OUT gcsSURF_INFO_PTR *SurfInfo
    );

gceSTATUS
gcoHARDWARE_Free2DSurface(
    IN gcoHARDWARE Hardware,
    IN gcsSURF_INFO_PTR SurfInfo
    );

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_user_hardware_h_ */

