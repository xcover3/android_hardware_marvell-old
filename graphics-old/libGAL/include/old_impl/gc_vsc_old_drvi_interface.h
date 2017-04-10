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
**    Include file the defines the front- and back-end compilers, as well as the
**    objects they use.
*/

#ifndef __gc_vsc_old_drvi_interface_h_
#define __gc_vsc_old_drvi_interface_h_

#include "gc_hal_engine.h"
#include "old_impl/gc_vsc_old_gcsl.h"

BEGIN_EXTERN_C()

#ifndef GC_ENABLE_LOADTIME_OPT
#if !DX_SHADER
#define GC_ENABLE_LOADTIME_OPT      1
#endif
#endif

#define TEMP_SHADER_PATCH            1

#ifndef GC_ENABLE_DUAL_FP16
#define GC_ENABLE_DUAL_FP16          1
#endif


/*
 *   Re-compilation & Dynamic Linker data sturctures
 */

enum gceRecompileKind
{
    gceRK_PATCH_NONE = 0,
    gceRK_PATCH_TEXLD_FORMAT_CONVERSION,
    gceRK_PATCH_OUTPUT_FORMAT_CONVERSION,
    gceRK_PATCH_DEPTH_COMPARISON,
    gceRK_PATCH_CONSTANT_CONDITION,
    gceRK_PATCH_CONSTANT_TEXLD,
    gceRK_PATCH_COLOR_FACTORING,
    gceRK_PATCH_ALPHA_BLENDING,
    gceRK_PATCH_DEPTH_BIAS,
    gceRK_PATCH_PRE_ROTATION,
    gceRK_PATCH_NP2TEXTURE,
    gceRK_PATCH_GLOBAL_WORK_SIZE,
    gceRK_PATCH_READ_IMAGE,
    gceRK_PATCH_WRITE_IMAGE,
    gceRK_PATCH_Y_FLIPPED_TEXTURE,
    gceRK_PATCH_REMOVE_ASSIGNMENT_FOR_ALPHA,
    gceRK_PATCH_Y_FLIPPED_SHADER,
    gceRK_PATCH_SAMPLE_MASK
};

typedef enum _gceConvertFunctionKind
{
    gceCF_UNKNOWN,
    gceCF_RGBA32,
    gceCF_RGBA32I,
    gceCF_RGBA32UI,
    gceCF_RGBA16I,
    gceCF_RGBA16UI,
    gceCF_RGBA8I,
    gceCF_RGBA8UI,
    gceCF_SRGB8_ALPHA8,
    gceCF_RGB10_A2,
    gceCF_RGB10_A2UI,
/* ... */
}
gceConvertFunctionKind;

typedef enum _gceTexldFlavor
{
    gceTF_NONE,
    gceTF_TEXLD = gceTF_NONE,
    gceTF_PROJ,
    gceTF_PCF,
    gceTF_PCFPROJ,
    gceTF_BIAS_TEXLD,
    gceTF_BIAS_PROJ,
    gceTF_BIAS_PCF,
    gceTF_BIAS_PCFPROJ,
    gceTF_LOD_TEXLD,
    gceTF_LOD_PROJ,
    gceTF_LOD_PCF,
    gceTF_LOD_PCFPROJ,
    gceTF_GRAD_TEXLD,
    gceTF_GRAD_PROJ,
    gceTF_GRAD_PCF,
    gceTF_GRAD_PCFPROJ,
    gceTF_COUNT
}
gceTexldFlavor;

extern const gctCONST_STRING gcTexldFlavor[gceTF_COUNT];

typedef enum NP2_ADDRESS_MODE
{
    NP2_ADDRESS_MODE_CLAMP  = 0,
    NP2_ADDRESS_MODE_REPEAT = 1,
    NP2_ADDRESS_MODE_MIRROR = 2
}
NP2_ADDRESS_MODE;

typedef struct _gcNPOT_PATCH_PARAM
{
    gctINT               samplerSlot;
    NP2_ADDRESS_MODE     addressMode[3];
    gctINT               texDimension;    /* 2 or 3 */
}
gcNPOT_PATCH_PARAM, *gcNPOT_PATCH_PARAM_PTR;

typedef struct _gcsInputConversion
{
    gctINT                  layers;       /* numberof layers the input format
                                             represented internally (up to 4) */
    gcUNIFORM               samplers[4];
    gcsSURF_FORMAT_INFO     samplerInfo;  /* */
    gctCONST_STRING         sourceFormat;
    gceTEXTURE_SWIZZLE      swizzle[gcvTEXTURE_COMPONENT_NUM];
}
gcsInputConversion;

typedef struct _gcsOutputConversion
{
    gctINT                  layers;       /* numberof layers the input format
                                             represented internally (up to 4) */
    gcOUTPUT                outputs[4];
    gcsSURF_FORMAT_INFO     formatInfo;  /* */
    gctINT                  outputLocation;
}
gcsOutputConversion;

typedef struct _gcsDepthComparison
{
    gcsSURF_FORMAT_INFO     formatInfo;  /* */
    gcUNIFORM               sampler;
    gctUINT                 compMode;
    gctUINT                 compFunction;
    gctBOOL                 convertD32F;   /* the texture format is D32F and
                                              needs to be converted (halti 0) */
} gcsDepthComparison;

typedef struct _gcsConstantCondition
{
    /* */
    gctUINT   uniformCount;
    gctUINT * uniformIdx;       /* a list of index of the uniform which is
                                   used in constant expression */
    gctUINT   conditionCount;   /* the number of conditions depended on
                                   the uniforms */
}
gcsConstantCondition;

typedef struct _gcsConstantTexld
{
    gctUINT   samplerIndex;
    gctUINT   instId;            /* the instruction id which has the constant
                                    coordinate */
    gctFLOAT  value[4];          /* vec4 value of the texld */
}
gcsConstantTexld;

typedef struct _gcsPatchOutputValue
{
    gctSTRING   outputName;
    gctUINT     outputFormatConversion;
}
gcsPatchOutputValue;

typedef struct _gcsPatchColorFactoring
{
    gctINT    outputLocation;    /* location of render target need to patch */
    gctFLOAT  value[4];          /* vec4 value of the color factor */
}
gcsPatchColorFactoring;

typedef struct _gcsPatchAlphaBlending
{
    gctINT    outputLocation;    /* location of render target need to patch */
}
gcsPatchAlphaBlending;

typedef struct _gcsPatchDepthBias
{
    gcUNIFORM depthBiasUniform;  /* uniform holding the value of depth bias */
}
gcsPatchDepthBias;

typedef struct _gcsPatchPreRotation
{
    gceSURF_ROTATION rotation;  /* pre-rotation kind */
    gcUNIFORM rotationUniformCol0;  /* uniform holding the value of rotationUniformCol0 */
    gcUNIFORM rotationUniformCol1;  /* uniform holding the value of rotationUniformCol1 */
}
gcsPatchPreRotation;

typedef struct _gcsPatchNP2Texture
{
    gctINT textureCount;
    gcNPOT_PATCH_PARAM_PTR np2Texture;
}
gcsPatchNP2Texture;

typedef struct _gcsPatchGlobalWorkSize
{
    gcUNIFORM  globalWidth;
    gcUNIFORM  groupWidth;
}
gcsPatchGlobalWorkSize;

typedef struct _gcsPatchReadImage
{
    gctUINT                 samplerNum;
    gctUINT                 imageType;
    gctUINT                 imageDataIndex;
    gctUINT                 imageSizeIndex;
    gctUINT                 samplerValue;
    gctUINT                 channelDataType;
    gctUINT                 channelOrder;
}
gcsPatchReadImage;

typedef struct _gcsPatchWriteImage
{
    gctUINT                 samplerNum;
    gctUINT                 imageDataIndex;
    gctUINT                 imageSizeIndex;
    gctUINT                 channelDataType;
    gctUINT                 channelOrder;
}
gcsPatchWriteImage;

typedef struct _gcsPatchYFilppedTexture
{
    gcUNIFORM yFlippedTexture;  /* uniform need to filp y component. */
}
gcsPatchYFilppedTexture;

typedef struct _gcsPatchYFilppedShader
{
    gcUNIFORM rtHeight;  /* uniform contains render target height to filp y component. */
}
gcsPatchYFilppedShader;

typedef struct _gcsPatchRemoveAssignmentForAlphaChannel
{
    gctBOOL removeOutputAlpha[gcdMAX_DRAW_BUFFERS]; /* Flag whether this output need this patch.*/
}
gcsPatchRemoveAssignmentForAlphaChannel;

typedef struct _gcsPatchSampleMask
{
    /* alpha to converage */
    gctBOOL                 alphaToConverageEnabled;
    /* sample coverage */
    gctBOOL                 sampleConverageEnabled;
    gcUNIFORM               sampleCoverageValue_Invert; /* float32 x: value,
                                                         *         y: invert */
    /* sample mask */
    gctBOOL                 sampleMaskEnabled;
    gcUNIFORM               sampleMaskValue;     /* UINT type, the fragment coverage is ANDed
                                                    with coverage value SAMPLE_MASK_VALUE */
    /* internal data */
    gctUINT                 _finalSampleMask;
    gctINT                  _implicitMaskRegIndex;
}
gcsPatchSampleMask;

typedef struct _gcRecompileDirective * gcPatchDirective_PTR;
typedef struct _gcRecompileDirective
{
    enum gceRecompileKind   kind;
    union {
        gcsInputConversion  *     formatConversion;
        gcsOutputConversion *     outputConversion;
        gcsConstantCondition *    constCondition;
        gcsDepthComparison  *     depthComparison;
        gcsConstantTexld    *     constTexld;
        gcsPatchColorFactoring *  colorFactoring;
        gcsPatchAlphaBlending *   alphaBlending;
        gcsPatchDepthBias *       depthBias;
        gcsPatchPreRotation *     preRotation;
        gcsPatchNP2Texture *      np2Texture;
        gcsPatchGlobalWorkSize *  globalWorkSize;
        gcsPatchReadImage *       readImage;
        gcsPatchWriteImage *      writeImage;
        gcsPatchYFilppedTexture * yFilppedTexture;
        gcsPatchRemoveAssignmentForAlphaChannel * removeOutputAlpha;
        gcsPatchYFilppedShader *  yFilppedShader;
        gcsPatchSampleMask *      sampleMask;
    } patchValue;
    gcPatchDirective_PTR    next;  /* pointer to next patch directive */
}
gcPatchDirective;

typedef struct _gcDynamicPatchInfo
{
    gctINT                patchDirectiveCount;
    gcPatchDirective      patchDirective[1];
}
gcDynamicPatchInfo;

typedef enum _gcGL_DRIVER_VERSION {
    gcGL_DRIVER_ES11, /* OpenGL ES 1.1 */
    gcGL_DRIVER_ES20, /* OpenGL ES 2.0 */
    gcGL_DRIVER_ES30     /* OpenGL ES 3.0 */
}
gcGL_DRIVER_VERSION;

/* gcSHADER objects. */


typedef struct _gcsPROGRAM_UNIFIED_STATUS
{
    gctBOOL     useIcache;          /* Icache enabled or not */

    gctBOOL     instruction; /* unified instruction enabled or not */
    gctBOOL     constant;    /* unified const enabled or not */
    gctBOOL     sampler;     /* unified sampler enabled or not */

    gctINT     instVSEnd;       /* VS instr end for unified instruction */
    gctINT     instPSStart;     /* PS instr start for unified instruction */
    gctINT     constVSEnd;      /* VS const end for unified constant */
    gctINT     constPSStart;    /* PS const start for unified constant */
    gctINT     samplerVSEnd;    /* VS sampler end for unified sampler */
    gctINT     samplerPSStart;  /* PS sampler start for unified sampler */
}
gcsPROGRAM_UNIFIED_STATUS;



typedef struct _gcsHINT *               gcsHINT_PTR;
typedef struct _gcSHADER_PROFILER *     gcSHADER_PROFILER;

struct _gcsHINT
{
    /* Numbr of data transfers for Vertex Shader output. */
    gctUINT32   vsOutputCount;

    /* Flag whether the VS has point size or not. */
    gctBOOL     vsHasPointSize;

#if gcdUSE_WCLIP_PATCH
    /* Flag whether the VS gl_position.z depends on gl_position.w
       it's a hint for wclipping */
    gctBOOL     vsPositionZDependsOnW;
#endif

    /* Flag whether or not the shader has a KILL instruction. */
    gctBOOL     hasKill;

    /* Element count. */
    gctUINT32   elementCount;

    /* Component count. */
    gctUINT32   componentCount;

    /* Number of data transfers for Fragment Shader input. */
    gctUINT32   fsInputCount;

    /* the start physical address for uniforms only used in LTC expression */
    gctUINT32   fsLTCUsedUniformStartAddress;
    gctUINT32   vsLTCUsedUniformStartAddress;

    /* Maximum number of temporary registers used in FS. */
    gctUINT32   fsMaxTemp;

    /* Maximum number of temporary registers used in VS. */
    gctUINT32   vsMaxTemp;

    /* Balance minimum. */
    gctUINT32   balanceMin;

    /* Balance maximum. */
    gctUINT32   balanceMax;

    /* Auto-shift balancing. */
    gctBOOL     autoShift;

    /* Flag whether the PS outputs the depth value or not. */
    gctBOOL     psHasFragDepthOut;

    /* Flag whether the PS code has discard. */
    gctBOOL     psHasDiscard;

    /* Flag whether the ThreadWalker is in PS. */
    gctBOOL     threadWalkerInPS;

    /* SURF Node for instruction buffer for I-Cache. */
    gctPOINTER  surfNode;

#if gcdALPHA_KILL_IN_SHADER
    /* States to set when alpha kill is enabled. */
    gctUINT32   killStateAddress;
    gctUINT32   alphaKillStateValue;
    gctUINT32   colorKillStateValue;

    /* Shader instructiuon. */
    gctUINT32   killInstructionAddress;
    gctUINT32   alphaKillInstruction[3];
    gctUINT32   colorKillInstruction[3];
#endif
    gctUINT     vsInstCount;
    gctUINT     fsInstCount;

#if TEMP_SHADER_PATCH
    gctUINT32   pachedShaderIdentifier;
#endif

    gcsPROGRAM_UNIFIED_STATUS unifiedStatus;

    gctUINT     maxInstCount;
    gctBOOL     fsIsDual16;
    gctBOOL     useLoadStore;
    gctUINT     psInputControlHighpPosition;
    gctINT32    psOutput2RtIndex[gcdMAX_DRAW_BUFFERS];

    /* vertex and fragment shader id, to help identifying
     * shaders used by draw commands */
    gctUINT     vertexShaderId;
    gctUINT     fragmentShaderId;

    /* Flag whether program is smooth or flat. */
    gceSHADING  shaderMode;

    /* Shader uniform registers. */
    gctUINT     maxConstCount;
    gctUINT     vsConstCount;
    gctUINT     fsConstCount;

    /* Data for register: gcregShaderConfigRegAddrs. */
    /* For vertex shader, only save the bit that would be covered on fragment shader.*/
    gctUINT32   shaderConfigData;

    /* Flag whether this program can remove alpha assignment*/
    gctBOOL     removeAlphaAssignment;
    /* flag if the shader uses gl_FragCoord, gl_FrontFacing, gl_PointCoord */
    gctBOOL     useFragCoord;
    gctBOOL     useFrontFacing;
    gctBOOL     usePointCoord;
    gctBOOL     useDSY;
    gctBOOL     yInvertAware;

    /* Disable EarlyZ for this program. */
    gctBOOL     disableEarlyZ;

#if gcdUSE_WCLIP_PATCH
    /* Strict WClip match. */
    gctBOOL     strictWClipMatch;
    gctINT      MVPCount;
#endif

    gcePROGRAM_STAGE_BIT  stageBits;

    gctUINT     usedSamplerMask;
    gctUINT     usedRTMask;

    /* Deferred-program when flushing as they are in VS output ctrl reg */
    gctINT      vsOutput16RegNo;
    gctINT      vsOutput17RegNo;
    gctINT      vsOutput18RegNo;

	gctUINT32   vsConstBase;
	gctUINT32   psConstBase;
};

#define gcsHINT_SetProgramStageBit(Hint, Stage) do { (Hint)->stageBits |= 1 << Stage;} while (0)


typedef enum _gcSHADER_TYPE_KIND
{
    gceTK_UNKOWN,
    gceTK_FLOAT,
    gceTK_INT,
    gceTK_UINT,
    gceTK_BOOL,
    gceTK_FIXED,
    gceTK_SAMPLER,
    gceTK_IMAGE,
    gceTK_OTHER
} gcSHADER_TYPE_KIND;

typedef struct _gcSHADER_TYPEINFO
{
    gcSHADER_TYPE      type;              /* e.g. gcSHADER_FLOAT_2X4 */
    gctUINT            components;        /* e.g. 4 components each row */
    gctUINT            rows;              /* e.g. 2 rows             */
    gcSHADER_TYPE      rowType;           /* e.g. gcSHADER_FLOAT_X4  */
    gcSHADER_TYPE      componentType;     /* e.g. gcSHADER_FLOAT_X1  */
    gcSHADER_TYPE_KIND kind;              /* e.g. gceTK_FLOAT */
    gctCONST_STRING    name;              /* e.g. "FLOAT_2X4" */
} gcSHADER_TYPEINFO;

extern const gcSHADER_TYPEINFO gcvShaderTypeInfo[];

#define gcmType_Comonents(Type)    (gcvShaderTypeInfo[Type].components)
#define gcmType_Rows(Type)         (gcvShaderTypeInfo[Type].rows)
#define gcmType_RowType(Type)      (gcvShaderTypeInfo[Type].rowType)
#define gcmType_ComonentType(Type) (gcvShaderTypeInfo[Type].componentType)
#define gcmType_Kind(Type)         (gcvShaderTypeInfo[Type].kind)
#define gcmType_Name(Type)         (gcvShaderTypeInfo[Type].name)

#define gcmType_ComponentByteSize       4

#define gcmType_isMatrix(type) (gcmType_Rows(type) > 1)

#if TEMP_SHADER_PATCH
typedef struct _gcSHADER_TYPE_INFO
{
    gcSHADER_TYPE    type;        /* eg. gcSHADER_FLOAT_2X3 is the type */
    gctCONST_STRING  name;        /* the name of the type: "gcSHADER_FLOAT_2X3" */
    gcSHADER_TYPE    baseType;    /* its base type is gcSHADER_FLOAT_2 */
    gctINT           components;  /* it has 2 components */
    gctINT           rows;        /* and 3 rows */
    gctINT           size;        /* the size in byte */
} gcSHADER_TYPE_INFO;
extern gcSHADER_TYPE_INFO shader_type_info[];
#endif

enum gceLTCDumpOption {
    gceLTC_DUMP_UNIFORM      = 0x0001,
    gceLTC_DUMP_EVALUATION   = 0x0002,
    gceLTC_DUMP_EXPESSION    = 0x0004,
    gceLTC_DUMP_COLLECTING   = 0x0008,
};

/* single precision floating point NaN, Infinity, etc. constant */
#define SINGLEFLOATPOSITIVENAN      0x7fc00000
#define SINGLEFLOATNEGTIVENAN       0xffc00000
#define SINGLEFLOATPOSITIVEINF      0x7f800000
#define SINGLEFLOATNEGTIVEINF       0xff800000
#define SINGLEFLOATPOSITIVEZERO     0x00000000
#define SINGLEFLOATNEGTIVEZERO      0x80000000

#define isF32PositiveNaN(f)  (*(gctUINT *)&(f) == SINGLEFLOATPOSITIVENAN)
#define isF32NegativeNaN(f)  (*(gctUINT *)&(f) == SINGLEFLOATNEGTIVENAN)
#define isF32NaN(f)  (isF32PositiveNaN(f) || isF32NegativeNaN(f))

#define FLOAT_NaN   (SINGLEFLOATPOSITIVENAN)
#ifndef INT32_MAX
#define INT32_MAX   (0x7fffffff)
#endif
#ifndef INT32_MIN
#define INT32_MIN   (-0x7fffffff-1)
#endif

#define MAX_LTC_COMPONENTS   4

typedef union _ConstantValueUnion
{
    gctBOOL        b;
    gctUINT8       u8;
    gctUINT16      u16;
    gctUINT32      u32;
    gctUINT64      u64;
    gctINT8        i8;
    gctINT16       i16;
    gctINT32       i32;
    gctINT64       i64;
    gctFLOAT       f32;
} ConstantValueUnion;

typedef struct _LoadtimeConstantValue
{
    gcSL_ENABLE          enable;               /* the components enabled, for target value */
    gctSOURCE_t          sourceInfo;           /* source type, indexed, format and swizzle info */
    gcSL_FORMAT          elementType;          /* the format of element */
    gctINT               instructionIndex;     /* the instruction index to the LTC expression in Shader */
    ConstantValueUnion   v[MAX_LTC_COMPONENTS];
} LTCValue, *PLTCValue;

gceSTATUS gcOPT_GetUniformSrcLTC(
    IN gcSHADER              Shader,
    IN gctUINT               ltcInstIdx,
    IN gctINT                SourceId,
    IN PLTCValue             Results,
    OUT gcUNIFORM*           RetUniform,
    OUT gctINT*              RetCombinedOffset,
    OUT gctINT*              RetConstOffset,
    OUT gctINT*              RetIndexedOffset,
    OUT PLTCValue            SourceValue
    );

gceSTATUS gcOPT_DoConstantFoldingLTC(
    IN gcSHADER              Shader,
    IN gctUINT               ltcInstIdx,
    IN PLTCValue             source0Value, /* set by driver if src0 is app's uniform */
    IN PLTCValue             source1Value, /* set by driver if src1 is app's uniform */
    IN PLTCValue             source2Value, /* set by driver if src2 is app's uniform */
    OUT PLTCValue            resultValue, /* regarded as register file */
    IN OUT PLTCValue         Results
    );

gctBOOL gcDumpOption(gctINT Opt);

/* Shader flags. */
typedef enum _gceSHADER_FLAGS
{
    gcvSHADER_NO_OPTIMIZATION           = 0x00,
    gcvSHADER_DEAD_CODE                 = 0x01,
    gcvSHADER_RESOURCE_USAGE            = 0x02,
    gcvSHADER_OPTIMIZER                 = 0x04,
    gcvSHADER_USE_GL_Z                  = 0x08,
    /*
        The GC family of GPU cores model GC860 and under require the Z
        to be from 0 <= z <= w.
        However, OpenGL specifies the Z to be from -w <= z <= w.  So we
        have to a conversion here:

            z = (z + w) / 2.

        So here we append two instructions to the vertex shader.
    */
    gcvSHADER_USE_GL_POSITION           = 0x10,
    gcvSHADER_USE_GL_FACE               = 0x20,
    gcvSHADER_USE_GL_POINT_COORD        = 0x40,
    gcvSHADER_LOADTIME_OPTIMIZER        = 0x80,
#if gcdALPHA_KILL_IN_SHADER
    gcvSHADER_USE_ALPHA_KILL            = 0x100,
#endif

    gcvSHADER_I_AM_FREE_FOR_USE         = 0x200,

    gcvSHADER_TEXLD_W                   = 0x400,

    gcvSHADER_INT_ATTRIBUTE_W           = 0x800,

    /* The Shader is patched by recompilation. */
    gcvSHADER_IMAGE_PATCHING            = 0x1000,

    /* Remove unused uniforms on shader, only enable for es20 shader. */
    gcvSHADER_REMOVE_UNUSED_UNIFORMS    = 0x2000,

    /* All optimizer without this flag would not be done on recompiler.  */
    gcvSHADER_ONCE_OPTIMIZER            = 0x4000,

    /* Force linking when either vertex or fragment shader not present */
    gcvSHADER_FORCE_LINKING             = 0x8000,

    /* This shader comes from recompiler. */
    gcvSHADER_RECOMPILER                = 0x10000,

    /* Need to check API level resources for this shader. */
    gcvSHADER_CHECK_API_LEVEL_RESOURCE  = 0x20000,
}
gceSHADER_FLAGS;

#if gcdUSE_WCLIP_PATCH
gceSTATUS
gcSHADER_CheckClipW(
    IN gctCONST_STRING VertexSource,
    IN gctCONST_STRING FragmentSource,
    OUT gctBOOL * clipW);
#endif

/*******************************************************************************
**                            gcOptimizer Data Structures
*******************************************************************************/
typedef enum _gceSHADER_OPTIMIZATION
{
    /*  No optimization. */
    gcvOPTIMIZATION_NONE,

    /*  Flow graph construction. */
    gcvOPTIMIZATION_CONSTRUCTION                = 1 << 0,

    /*  Dead code elimination. */
    gcvOPTIMIZATION_DEAD_CODE                   = 1 << 1,

    /*  Redundant move instruction elimination. */
    gcvOPTIMIZATION_REDUNDANT_MOVE              = 1 << 2,

    /*  Inline expansion. */
    gcvOPTIMIZATION_INLINE_EXPANSION            = 1 << 3,

    /*  Constant propagation. */
    gcvOPTIMIZATION_CONSTANT_PROPAGATION        = 1 << 4,

    /*  Redundant bounds/checking elimination. */
    gcvOPTIMIZATION_REDUNDANT_CHECKING          = 1 << 5,

    /*  Loop invariant movement. */
    gcvOPTIMIZATION_LOOP_INVARIANT              = 1 << 6,

    /*  Induction variable removal. */
    gcvOPTIMIZATION_INDUCTION_VARIABLE          = 1 << 7,

    /*  Common subexpression elimination. */
    gcvOPTIMIZATION_COMMON_SUBEXPRESSION        = 1 << 8,

    /*  Control flow/banch optimization. */
    gcvOPTIMIZATION_CONTROL_FLOW                = 1 << 9,

    /*  Vector component operation merge. */
    gcvOPTIMIZATION_VECTOR_INSTRUCTION_MERGE    = 1 << 10,

    /*  Algebra simplificaton. */
    gcvOPTIMIZATION_ALGEBRAIC_SIMPLIFICATION    = 1 << 11,

    /*  Pattern matching and replacing. */
    gcvOPTIMIZATION_PATTERN_MATCHING            = 1 << 12,

    /*  Interprocedural constant propagation. */
    gcvOPTIMIZATION_IP_CONSTANT_PROPAGATION     = 1 << 13,

    /*  Interprecedural register optimization. */
    gcvOPTIMIZATION_IP_REGISTRATION             = 1 << 14,

    /*  Optimization option number. */
    gcvOPTIMIZATION_OPTION_NUMBER               = 1 << 15,

    /*  Loadtime constant. */
    gcvOPTIMIZATION_LOADTIME_CONSTANT           = 1 << 16,

    /*  MAD instruction optimization. */
    gcvOPTIMIZATION_MAD_INSTRUCTION             = 1 << 17,

    gcvOPTIMIZATION_LOAD_SW_W          = 1 << 18,

    /*  Move code into conditional block if possile */
    gcvOPTIMIZATION_CONDITIONALIZE              = 1 << 19,

    /*  Expriemental: power optimization mode
        1. add extra dummy texld to tune performance
        2. insert NOP after high power instrucitons
        3. split high power vec3/vec4 instruciton to vec2/vec1 operation
        4. ...
     */
    gcvOPTIMIZATION_POWER_OPTIMIZATION          = 1 << 20,

    /*  Optimize intrinsics */
    gcvOPTIMIZATION_INTRINSICS                  = 1 << 21,

    /*  Optimize varying packing */
    gcvOPTIMIZATION_VARYINGPACKING              = 1 << 22,

    /*  Loop rerolling */
    gcvOPTIMIZATION_LOOP_REROLLING              = 1 << 23,

    /*  Image patching: */
    /*     Inline functions using IMAGE_READ or IMAGE_WRITE. */
    gcvOPTIMIZATION_IMAGE_PATCHING              = 1 << 24,

    /*  Full optimization. */
    /*  Note that gcvOPTIMIZATION_LOAD_SW_W is off. */
    gcvOPTIMIZATION_FULL                        = 0x7FFFFFFF &
                                                  ~gcvOPTIMIZATION_LOAD_SW_W &
                                                  ~gcvOPTIMIZATION_POWER_OPTIMIZATION &
                                                  ~gcvOPTIMIZATION_IMAGE_PATCHING,

    /* Optimization Unit Test flag. */
    gcvOPTIMIZATION_UNIT_TEST                   = 1 << 31
}
gceSHADER_OPTIMIZATION;

typedef enum _gceOPTIMIZATION_VaryingPaking
{
    gcvOPTIMIZATION_VARYINGPACKING_NONE = 0,
    gcvOPTIMIZATION_VARYINGPACKING_NOSPLIT,
    gcvOPTIMIZATION_VARYINGPACKING_SPLIT
} gceOPTIMIZATION_VaryingPaking;

typedef struct _ShaderSourceList ShaderSourceList;
struct _ShaderSourceList
{
    gctINT             shaderId;
    gctINT             sourceSize;
    gctCHAR *          src;
    gctSTRING          fileName;
    ShaderSourceList * next;
};
enum MacroDefineKind
{
    MDK_Define,
    MDK_Undef
};

typedef struct _MacroDefineList MacroDefineList;
struct _MacroDefineList
{
    enum MacroDefineKind kind;
    gctCHAR *            str;    /* name[=value] */
    MacroDefineList *    next;
};

enum ForceInlineKind
{
    FIK_None,
    FIK_Inline,
    FIK_NotInline
};

typedef struct _InlineStringList InlineStringList;
struct _InlineStringList
{
    enum ForceInlineKind kind;
    gctCHAR *            func;   /* function name to force inline/notInline */
    InlineStringList *   next;
};

typedef struct _gcOPTIMIZER_OPTION
{
    gceSHADER_OPTIMIZATION     optFlags;

    /* debug & dump options:

         VC_OPTION=-DUMP:SRC|:IR|:OPT|:OPTV|:CG|:CGV|:ALL|:ALLV|:UNIFORM|:T[-]m,n

         SRC:  dump shader source code
         IR:   dump final IR
         OPT:  dump incoming and final IR
         OPTV: dump result IR in each optimization phase
         CG:   dump generated machine code
         CGV:  dump BE tree and optimization detail
         Tm:   turn on dump for shader id m
         Tm,n: turn on dump for shader id is in range of [m, n]
         T-m:  turn off dump for shader id m
         T-m,n: turn off dump for shader id is in range of [m, n]

         ALL = SRC|OPT|CG
         ALLV = SRC|OPT|OPTV|CG|CGV

         UNIFORM: dump uniform value when setting uniform
     */
    gctBOOL     dumpShaderSource;      /* dump shader source code */
    gctBOOL     dumpOptimizer;         /* dump incoming and final IR */
    gctBOOL     dumpOptimizerVerbose;  /* dump result IR in each optimization phase */
    gctBOOL     dumpBEGenertedCode;    /* dump generated machine code */
    gctBOOL     dumpBEVerbose;         /* dump BE tree and optimization detail */
    gctBOOL     dumpBEFinalIR;         /* dump BE final IR */
    gctBOOL     dumpUniform;           /* dump uniform value when setting uniform */
    gctINT      _dumpStart;            /* shader id start to dump */
    gctINT      _dumpEnd;              /* shader id end to dump */

    /* Code generation */

    /* Varying Packing:

          VC_OPTION=-PACKVARYING:[0-2]|:T[-]m[,n]|:LshaderIdx,min,max

          0: turn off varying packing
          1: pack varyings, donot split any varying
          2: pack varyings, may split to make fully packed output

          Tm:    only packing shader pair which vertex shader id is m
          Tm,n:  only packing shader pair which vertex shader id
                   is in range of [m, n]
          T-m:   do not packing shader pair which vertex shader id is m
          T-m,n: do not packing shader pair which vertex shader id
                   is in range of [m, n]

          LshaderIdx,min,max : set  load balance (min, max) for shaderIdx
                               if shaderIdx is -1, all shaders are impacted
                               newMin = origMin * (min/100.);
                               newMax = origMax * (max/100.);
     */
    gceOPTIMIZATION_VaryingPaking    packVarying;
    gctINT                           _triageStart;
    gctINT                           _triageEnd;
    gctINT                           _loadBalanceShaderIdx;
    gctINT                           _loadBalanceMin;
    gctINT                           _loadBalanceMax;

    /* Do not generate immdeiate

          VC_OPTION=-NOIMM

       Force generate immediate even the machine model don't support it,
       for testing purpose only

          VC_OPTION=-FORCEIMM
     */
    gctBOOL     noImmediate;
    gctBOOL     forceImmediate;

    /* Power reduction mode options */
    gctBOOL   needPowerOptimization;

    /* Patch TEXLD instruction by adding dummy texld
       (can be used to tune GPU power usage):
         for every TEXLD we seen, add n dummy TEXLD

        it can be enabled by environment variable:

          VC_OPTION=-PATCH_TEXLD:M:N

        (for each M texld, add N dummy texld)
     */
    gctINT      patchEveryTEXLDs;
    gctINT      patchDummyTEXLDs;

    /* Insert NOP after high power consumption instructions

         VC_OPTION="-INSERTNOP:MUL:MULLO:DP3:DP4:SEENTEXLD"
     */
    gctBOOL     insertNOP;
    gctBOOL     insertNOPAfterMUL;
    gctBOOL     insertNOPAfterMULLO;
    gctBOOL     insertNOPAfterDP3;
    gctBOOL     insertNOPAfterDP4;
    gctBOOL     insertNOPOnlyWhenTexldSeen;

    /* split MAD to MUL and ADD:

         VC_OPTION=-SPLITMAD
     */
    gctBOOL     splitMAD;

    /* Convert vect3/vec4 operations to multiple vec2/vec1 operations

         VC_OPTION=-SPLITVEC:MUL:MULLO:DP3:DP4
     */
    gctBOOL     splitVec;
    gctBOOL     splitVec4MUL;
    gctBOOL     splitVec4MULLO;
    gctBOOL     splitVec4DP3;
    gctBOOL     splitVec4DP4;

    /* turn/off features:

          VC_OPTION=-F:n,[0|1]
          Note: n must be decimal number
     */
    gctUINT     featureBits;

    /* Replace specified shader's source code with the contents in
       specified file:

         VC_OPTION=-SHADER:id1,file1[:id2,file ...]

    */
    ShaderSourceList * shaderSrcList;

    /* Load-time Constant optimization:

        VC_OPTION=-LTC:0|1

    */
    gctBOOL     enableLTC;

    /* VC_OPTION=-Ddef1[=value1] -Ddef2[=value2] -Uundef1 */
    MacroDefineList * macroDefines;

    /* inline level (default 2 at O1):

          VC_OPTION=-INLINELEVEL:[0-4]
             0:  no inline
             1:  only inline the function only called once or small function
             2:  inline functions be called less than 5 times or medium size function
             3:  inline everything possible within inline budget
             4:  inline everything possible disregard inline budget
     */
    gctUINT     inlineLevel;

    /* inline recompilation functions for depth comparison if inline level is not 0.
       (default 1)

          VC_OPTION=-INLINEDEPTHCOMP:[0-3]
             0:  follows inline level
             1:  inline depth comparison functions for halti2
             2:  inline depth comparison functions for halti1
             3:  inline depth comparison functions for halti0
     */
    gctBOOL     inlineDepthComparison;

    /* inline recompilation functions for format conversion if inline level is not 0.
       (default 1)

          VC_OPTION=-INLINEFORMATCONV:[0-3]
             0:  follows inline level
             1:  inline format conversion functions for halti2
             2:  inline format conversion functions for halti1
             3:  inline format conversion functions for halti0
     */
    gctUINT     inlineFormatConversion;

    /* dual 16 mode
     *
     *    VC_OPTION=-DUAL16:[0-2]
     *       0:  auto-on mode for specific benchmarks
     *       1:  auto-on mode for all applications.
     *       2:  force dual16 on for all applications no matter highp is specified or not.
     *       3:  force dual16 off, no dual16 any more.
     */
    gctUINT     dual16Mode;

    /* force inline or not inline a function
     *
     *   VC_OPTION=-FORCEINLINE:func[,func]*
     *
     *   VC_OPTION=-NOTINLINE:func[,func]*
     *
     */
    InlineStringList *   forceInline;

    /* Upload Uniform Block to state buffer if there are space available
     * Doing this may potentially improve the performance as the load
     * instruction for uniform block member can be removed.
     *
     *   VC_OPTION=-UPLOADUBO:0|1
     *
     */
    gctBOOL     uploadUBO;

    /* OpenCL floating point capabilities setting
     * FASTRELAXEDMATH => -cl-fast-relaxed-math option
     * FINITEMATHONLY => -cl-finite-math-only option
     * RTNE => Round To Even
     * RTZ => Round to Zero
     *
     * VC_OPTION=-OCLFPCAPS:FASTRELAXEDMATH:FINITEMATHONLY:RTNE:RTZ
     */
    gctUINT     oclFpCaps;

    /* use VIR code generator:
     *
     *   VC_OPTION=-VIRCG:[0|1]|T[-]m[,n]
     *    Tm:    turn on VIRCG for shader id m
     *    Tm,n:  turn on VIRCG for shader id is in range of [m, n]
     *    T-m:   turn off VIRCG for shader id m
     *    T-m,n: turn off VIRCG for shader id is in range of [m, n]
     *
     */
    gctBOOL     useVIRCodeGen;
    gctINT      _vircgStart;
    gctINT      _vircgEnd;

    /* create default UBO:
     *
     *   VC_OPTION=-CREATEDEAULTUBO:0|1
     *
     */
    gctBOOL     createDefaultUBO;

    /* Specify the log file name
     *
     *   VC_OPTION=-LOG:filename
     */
    gctSTRING   logFileName;
    gctFILE     debugFile;

    /* turn on/off shader patch:
     *   VC_OPTION=-PATCH:[0|1]|T[-]m[,n]
     *    Tm:    turn on shader patch for shader id m
     *    Tm,n:  turn on shader patch for shader id is in range of [m, n]
     *    T-m:   turn off shader patch for shader id m
     *    T-m,n: turn off shader patch for shader id is in range of [m, n]
     */
    gctBOOL     patchShader;
    gctINT      _patchShaderStart;
    gctINT      _patchShaderEnd;

    /* NOTE: when you add a new option, you MUST initialize it with default
       value in theOptimizerOption too */
} gcOPTIMIZER_OPTION;

#define DUAL16_AUTO_BENCH   0
#define DUAL16_AUTO_ALL     1
#define DUAL16_FORCE_ON     2
#define DUAL16_FORCE_OFF    3

#define VC_OPTION_OCLFPCAPS_FASTRELAXEDMATH     (1 << 0 )
#define VC_OPTION_OCLFPCAPS_FINITEMATHONLY      (1 << 1 )
#define VC_OPTION_OCLFPCAPS_ROUNDTOEVEN         (1 << 2 )
#define VC_OPTION_OCLFPCAPS_ROUNDTOZERO         (1 << 3 )
#define VC_OPTION_OCLFPCAPS_NOFASTRELAXEDMATH   (1 << 4 )

extern gcOPTIMIZER_OPTION theOptimizerOption;

#define gcmGetOptimizerOption() gcGetOptimizerOption()

#define gcmOPT_DUMP_Start()     (gcmGetOptimizerOption()->_dumpStart)
#define gcmOPT_DUMP_End()     (gcmGetOptimizerOption()->_dumpEnd)


#define gcmOPT_DUMP_UNIFORM()    \
             (gcmGetOptimizerOption()->dumpUniform != 0)

#define gcmOPT_SET_DUMP_SHADER_SRC(v)   \
             gcmGetOptimizerOption()->dumpShaderSource = (v)

#define gcmOPT_PATCH_TEXLD()  (gcmGetOptimizerOption()->patchDummyTEXLDs != 0)
#define gcmOPT_INSERT_NOP()   (gcmGetOptimizerOption()->insertNOP == gcvTRUE)
#define gcmOPT_SPLITMAD()     (gcmGetOptimizerOption()->splitMAD == gcvTRUE)
#define gcmOPT_SPLITVEC()     (gcmGetOptimizerOption()->splitVec == gcvTRUE)

#define gcmOPT_NOIMMEDIATE()  (gcmGetOptimizerOption()->noImmediate == gcvTRUE)
#define gcmOPT_FORCEIMMEDIATE()  (gcmGetOptimizerOption()->forceImmediate == gcvTRUE)

#define gcmOPT_PACKVARYING()     (gcmGetOptimizerOption()->packVarying)
#define gcmOPT_PACKVARYING_triageStart()   (gcmGetOptimizerOption()->_triageStart)
#define gcmOPT_PACKVARYING_triageEnd()     (gcmGetOptimizerOption()->_triageEnd)
#define gcmOPT_SetVaryingPacking(VP)       do { gcmOPT_PACKVARYING() = VP; } while (0)

#define gcmOPT_LB_ShaderIdx()  (gcmGetOptimizerOption()->_loadBalanceShaderIdx)
#define gcmOPT_LB_Min()        (gcmGetOptimizerOption()->_loadBalanceMin)
#define gcmOPT_LB_Max()        (gcmGetOptimizerOption()->_loadBalanceMax)

#define gcmOPT_ShaderSourceList() (gcmGetOptimizerOption()->shaderSrcList)
#define gcmOPT_MacroDefines()     (gcmGetOptimizerOption()->macroDefines)

#define gcmOPT_EnableLTC()          (gcmGetOptimizerOption()->enableLTC)
#define gcmOPT_INLINELEVEL()        (gcmGetOptimizerOption()->inlineLevel)
#define gcmOPT_INLINEDEPTHCOMP()    (gcmGetOptimizerOption()->inlineDepthComparison)
#define gcmOPT_INLINEFORMATCONV()   (gcmGetOptimizerOption()->inlineFormatConversion)
#define gcmOPT_DualFP16Mode()       (gcmGetOptimizerOption()->dual16Mode)
#define gcmOPT_ForceInline()        (gcmGetOptimizerOption()->forceInline)
#define gcmOPT_UploadUBO()          (gcmGetOptimizerOption()->uploadUBO)
#define gcmOPT_oclFpCaps()          (gcmGetOptimizerOption()->oclFpCaps)
#define gcmOPT_UseVIRCodeGen()      (gcmGetOptimizerOption()->useVIRCodeGen)
#define gcmOPT_VIRCGStart()         (gcmGetOptimizerOption()->_vircgStart)
#define gcmOPT_VIRCGEnd()           (gcmGetOptimizerOption()->_vircgEnd)
#define gcmOPT_CreateDefaultUBO()   (gcmGetOptimizerOption()->createDefaultUBO)
#define gcmOPT_PatchShader()        (gcmGetOptimizerOption()->patchShader)
#define gcmOPT_PatchShaderStart()   (gcmGetOptimizerOption()->_patchShaderStart)
#define gcmOPT_PatchShaderEnd()     (gcmGetOptimizerOption()->_patchShaderEnd)

extern gctBOOL gcSHADER_GoVIRPass(gcSHADER Shader);
extern gctBOOL gcSHADER_DoPatch(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpSource(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpOptimizer(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpOptimizerVerbose(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpCodeGen(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpCodeGenVerbose(gcSHADER Shader);
extern gctBOOL VirSHADER_DumpCodeGenVerbose(gctINT ShaderId);
extern gctBOOL gcSHADER_DumpFinalIR(gcSHADER Shader);

/* Setters */
#define gcmOPT_SetPatchTexld(m,n) (gcmGetOptimizerOption()->patchEveryTEXLDs = (m),\
                                   gcmGetOptimizerOption()->patchDummyTEXLDs = (n))
#define gcmOPT_SetSplitVecMUL() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4MUL = gcvTRUE)
#define gcmOPT_SetSplitVecMULLO() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                  gcmGetOptimizerOption()->splitVec4MULLO = gcvTRUE)
#define gcmOPT_SetSplitVecDP3() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4DP3 = gcvTRUE)
#define gcmOPT_SetSplitVecDP4() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4DP4 = gcvTRUE)
#define gctOPT_FeatureBit(FBit) (gcmGetOptimizerOption()->featureBits & (FBit))

#define gcmOPT_SetPackVarying(v)     (gcmGetOptimizerOption()->packVarying = v)

#define FB_LIVERANGE_FIX1         0x0001
#define FB_INLINE_RENAMETEMP      0x0002
#define FB_UNLIMITED_INSTRUCTION  0x0004
#define FB_DISABLE_PATCH_CODE     0x0008
#define FB_DISABLE_MERGE_CONST    0x0010

/* temporarily change PredefinedDummySamplerId from 7 to 8 */
#define PredefinedDummySamplerId       8

/* Function argument qualifier */
typedef enum _gceINPUT_OUTPUT
{
    gcvFUNCTION_INPUT,
    gcvFUNCTION_OUTPUT,
    gcvFUNCTION_INOUT
}
gceINPUT_OUTPUT;

typedef enum _gceVARIABLE_UPDATE_FLAGS
{
    gcvVARIABLE_UPDATE_NOUPDATE = 0,
    gcvVARIABLE_UPDATE_TEMPREG,
    gcvVARIABLE_UPDATE_TYPE_QUALIFIER,
} gceVARIABLE_UPDATE_FLAGS;

typedef struct _gcsGLSLCaps
{
    gctUINT maxVertAttribs;
    gctUINT maxDrawBuffers;
    gctUINT maxVertUniformVectors;
    gctUINT maxFragUniformVectors;
    gctUINT maxVaryingVectors;
    gctUINT maxVertOutVectors;
    gctUINT maxFragInVectors;
    gctUINT maxVertTextureImageUnits;
    gctUINT maxFragTextureImageUnits;
    gctUINT maxCombinedTextureImageUnits;
    gctINT  minProgramTexelOffset;
    gctINT  maxProgramTexelOffset;
} gcsGLSLCaps;

extern gcsGLSLCaps *
    gcGetGLSLCaps(
    void
    );

extern gceSTATUS gcInitGLSLCaps(
    IN  gcoHAL Hal,
    OUT gcsGLSLCaps *Caps
    );

#define GetGLMaxVertexAttribs()               (gcGetGLSLCaps()->maxVertAttribs)
#define GetGLMaxDrawBuffers()                 (gcGetGLSLCaps()->maxDrawBuffers)
#define GetGLMaxVertexUniformVectors()        (gcGetGLSLCaps()->maxVertUniformVectors)
#define GetGLMaxFragmentUniformVectors()      (gcGetGLSLCaps()->maxFragUniformVectors)
#define GetGLMaxVaryingVectors()              (gcGetGLSLCaps()->maxVaryingVectors)
#define GetGLMaxVertexOutputVectors()         (gcGetGLSLCaps()->maxVertOutVectors)
#define GetGLMaxFragmentInputVectors()        (gcGetGLSLCaps()->maxFragInVectors)
#define GetGLMaxVertexTextureImageUnits()     (gcGetGLSLCaps()->maxVertTextureImageUnits)
#define GetGLMaxFragTextureImageUnits()       (gcGetGLSLCaps()->maxFragTextureImageUnits)
#define GetGLMaxCombinedTextureImageUnits()   (gcGetGLSLCaps()->maxCombinedTextureImageUnits)
#define GetGLMinProgramTexelOffset()          (gcGetGLSLCaps()->minProgramTexelOffset)
#define GetGLMaxProgramTexelOffset()          (gcGetGLSLCaps()->maxProgramTexelOffset)

void
gcGetOptionFromEnv(
    IN OUT gcOPTIMIZER_OPTION * Option
    );

void
gcSetOptimizerOption(
    IN gceSHADER_FLAGS Flags
    );

gcOPTIMIZER_OPTION *
gcGetOptimizerOption();

typedef gceSTATUS (*gctGLSLCompiler)(IN  gcoHAL Hal,
                                       IN  gctINT ShaderType,
                                       IN  gctUINT SourceSize,
                                       IN  gctCONST_STRING Source,
                                       OUT gcSHADER *Binary,
                                       OUT gctSTRING *Log);

void
gcSetGLSLCompiler(
    IN gctGLSLCompiler Compiler
    );

typedef gceSTATUS (*gctCLCompiler)(IN  gcoHAL Hal,
                                       IN  gctUINT SourceSize,
                                       IN  gctCONST_STRING Source,
                                       IN  gctCONST_STRING Option,
                                       OUT gcSHADER *Binary,
                                       OUT gctSTRING *Log);

void
gcSetCLCompiler(
    IN gctCLCompiler Compiler
    );

/*******************************************************************************
**  gcSHADER_SetDefaultUBO
**
**  Set the compiler enable/disable default UBO.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to gcSHADER object
**
**      gctBOOL Enabled
**          Pointer to enable/disable default UBO
*/
gceSTATUS
gcSHADER_SetDefaultUBO(
    IN gcSHADER Shader,
    IN gctBOOL Enabled
    );

/*******************************************************************************
**  gcSHADER_SetCompilerVersion
**
**  Set the compiler version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to gcSHADER object
**
**      gctINT *Version
**          Pointer to a two word version
*/
gceSTATUS
gcSHADER_SetCompilerVersion(
    IN gcSHADER Shader,
    IN gctUINT32 *Version
    );

/*******************************************************************************
**  gcSHADER_SetClientApiVersion
**
**  Set the client API version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gceAPI API
**          Client API version.
*/
gceSTATUS
gcSHADER_SetClientApiVersion(
    IN gcSHADER Shader,
    IN gceAPI Api
    );

/*******************************************************************************
**  gcSHADER_GetCompilerVersion
**
**  Get the compiler version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32_PTR *CompilerVersion.
**          Pointer to holder of returned compilerVersion pointer
*/
gceSTATUS
gcSHADER_GetCompilerVersion(
    IN gcSHADER Shader,
    OUT gctUINT32_PTR *CompilerVersion
    );

/*******************************************************************************
**  gcSHADER_SetShaderID
**
**  Set a unique id for this shader base on shader source string.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctUINT32 ID
**          The value of shader id.
*/
gceSTATUS
gcSHADER_SetShaderID(
    IN gcSHADER  Shader,
    IN gctUINT32 ID
    );

/*******************************************************************************
**  gcSHADER_GetShaderID
**
**  Get the unique id of this shader.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctUINT32 * ID
**          The value of shader id.
*/
gceSTATUS
gcSHADER_GetShaderID(
    IN gcSHADER  Shader,
    IN gctUINT32 * ID
    );

/*******************************************************************************
**  gcSHADER_GetType
**
**  Get the gcSHADER object's type.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctINT *Type.
**          Pointer to return shader type.
*/
gceSTATUS
gcSHADER_GetType(
    IN gcSHADER Shader,
    OUT gcSHADER_KIND *Type
    );

gctUINT
gcSHADER_NextId();

void
gcSHADER_AlignId();
/*******************************************************************************
**                             gcSHADER_Construct
********************************************************************************
**
**    Construct a new gcSHADER object.
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctINT ShaderType
**            Type of gcSHADER object to cerate.  'ShaderType' can be one of the
**            following:
**
**                gcSHADER_TYPE_VERTEX    Vertex shader.
**                gcSHADER_TYPE_FRAGMENT    Fragment shader.
**
**    OUTPUT:
**
**        gcSHADER * Shader
**            Pointer to a variable receiving the gcSHADER object pointer.
*/
gceSTATUS
gcSHADER_Construct(
    IN gcoHAL Hal,
    IN gctINT ShaderType,
    OUT gcSHADER * Shader
    );

/*******************************************************************************
**                              gcSHADER_Destroy
********************************************************************************
**
**    Destroy a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Destroy(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                              gcSHADER_Copy
********************************************************************************
**
**    Copy a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**      gcSHADER Source
**          Pointer to a gcSHADER object that will be copied.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Copy(
    IN gcSHADER Shader,
    IN gcSHADER Source
    );

/*******************************************************************************
**  gcSHADER_LoadHeader
**
**  Load a gcSHADER object from a binary buffer.  The binary buffer is layed out
**  as follows:
**      // Six word header
**      // Signature, must be 'S','H','D','R'.
**      gctINT8             signature[4];
**      gctUINT32           binFileVersion;
**      gctUINT32           compilerVersion[2];
**      gctUINT32           gcSLVersion;
**      gctUINT32           binarySize;
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**          Shader type will be returned if type in shader object is not gcSHADER_TYPE_PRECOMPILED
**
**      gctPOINTER Buffer
**          Pointer to a binary buffer containing the shader data to load.
**
**      gctUINT32 BufferSize
**          Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**  OUTPUT:
**      nothing
**
*/
gceSTATUS
gcSHADER_LoadHeader(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize,
    OUT gctUINT32 * ShaderVersion
    );

/*******************************************************************************
**  gcSHADER_LoadKernel
**
**  Load a kernel function given by name into gcSHADER object
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctSTRING KernelName
**          Pointer to a kernel function name
**
**  OUTPUT:
**      nothing
**
*/
gceSTATUS
gcSHADER_LoadKernel(
    IN gcSHADER Shader,
    IN gctSTRING KernelName
    );

/*******************************************************************************
**                                gcSHADER_Load
********************************************************************************
**
**    Load a gcSHADER object from a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer containg the shader data to load.
**
**        gctUINT32 BufferSize
**            Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Load(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize
    );

/*******************************************************************************
**                                gcSHADER_Save
********************************************************************************
**
**    Save a gcSHADER object to a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer to be used as storage for the gcSHADER
**            object.  If 'Buffer' is gcvNULL, the gcSHADER object will not be saved,
**            but the number of bytes required to hold the binary output for the
**            gcSHADER object will be returned.
**
**        gctUINT32 * BufferSize
**            Pointer to a variable holding the number of bytes allocated in
**            'Buffer'.  Only valid if 'Buffer' is not gcvNULL.
**
**    OUTPUT:
**
**        gctUINT32 * BufferSize
**            Pointer to a variable receiving the number of bytes required to hold
**            the binary form of the gcSHADER object.
*/
gceSTATUS
gcSHADER_Save(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN OUT gctUINT32 * BufferSize
    );

/*******************************************************************************
**                                gcSHADER_LoadEx
********************************************************************************
**
**    Load a gcSHADER object from a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer containg the shader data to load.
**
**        gctUINT32 BufferSize
**            Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_LoadEx(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize
    );

/*******************************************************************************
**                                gcSHADER_SaveEx
********************************************************************************
**
**    Save a gcSHADER object to a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer to be used as storage for the gcSHADER
**            object.  If 'Buffer' is gcvNULL, the gcSHADER object will not be saved,
**            but the number of bytes required to hold the binary output for the
**            gcSHADER object will be returned.
**
**        gctUINT32 * BufferSize
**            Pointer to a variable holding the number of bytes allocated in
**            'Buffer'.  Only valid if 'Buffer' is not gcvNULL.
**
**    OUTPUT:
**
**        gctUINT32 * BufferSize
**            Pointer to a variable receiving the number of bytes required to hold
**            the binary form of the gcSHADER object.
*/
gceSTATUS
gcSHADER_SaveEx(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN OUT gctUINT32 * BufferSize
    );

/*******************************************************************************
**  gcSHADER_GetLocationCount
**
**  Get the number of input/output locations for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of locations.
*/
gceSTATUS
gcSHADER_GetLocationCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetLocation
**
**  Get the location assocated with an indexed input/output for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of input/output to retreive the location setting for.
**
**  OUTPUT:
**
**      gctINT * Location
**          Pointer to a variable receiving the location value.
*/
gceSTATUS
gcSHADER_GetLocation(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gctINT * Location
    );

/*******************************************************************************
**  gcSHADER_GetBuiltinNameKind
**
**  Get the builtin name kind for the Name.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          String of the name.
**
**  OUTPUT:
**
**      gceBuiltinNameKind * Kind
**          Pointer to a variable receiving the builtin name kind value.
*/
gceSTATUS
gcSHADER_GetBuiltinNameKind(
    IN gcSHADER              Shader,
    IN gctCONST_STRING       Name,
    OUT gctUINT32 *          Kind
    );

/*******************************************************************************
**  gcSHADER_ReallocateAttributes
**
**  Reallocate an array of pointers to gcATTRIBUTE objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateAttributes(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                              gcSHADER_AddAttribute
********************************************************************************
**
**    Add an attribute to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the attribute to add.
**
**        gcSHADER_TYPE Type
**            Type of the attribute to add.
**
**        gctUINT32 Length
**            Array length of the attribute to add.  'Length' must be at least 1.
**
**        gctBOOL IsTexture
**            gcvTRUE if the attribute is used as a texture coordinate, gcvFALSE if not.
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_AddAttribute(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctBOOL IsTexture,
    IN gcSHADER_SHADERMODE ShaderMode,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_AddAttributeWithLocation
**
**  Add an attribute together with a location to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the attribute to add.
**
**      gcSHADER_TYPE Type
**          Type of the attribute to add.
**
**      gcSHADER_PRECISION Precision,
**          Precision of the attribute to add.
**
**      gctUINT32 Length
**          Array length of the attribute to add.  'Length' must be at least 1.
**
**      gctBOOL IsTexture
**          gcvTRUE if the attribute is used as a texture coordinate, gcvFALSE if not.
**
**      gctINT Location
**          Location associated with the attribute.
**
**  OUTPUT:
**
**      gcATTRIBUTE * Attribute
**          Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_AddAttributeWithLocation(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 Length,
    IN gctBOOL IsTexture,
    IN gctBOOL IsInvariant,
    IN gcSHADER_SHADERMODE ShaderMode,
    IN gctINT Location,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_GetVertexInstIDInputIndex
**
**  Get the input index of vertex/instance ID for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
*/
gctINT
gcSHADER_GetVertexInstIdInputIndex(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                         gcSHADER_GetAttributeCount
********************************************************************************
**
**    Get the number of attributes for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of attributes.
*/
gceSTATUS
gcSHADER_GetAttributeCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                            gcSHADER_GetAttribute
********************************************************************************
**
**    Get the gcATTRIBUTE object poniter for an indexed attribute for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of the attribute to retrieve.
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_GetAttribute(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**                            gcSHADER_GetAttributeByName
********************************************************************************
**
**    Get the gcATTRIBUTE object poniter for an name attribute for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctSTRING name
**            Name of output to retrieve.
**
**        gctUINT32 nameLength
**            Length of name to retrieve
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_GetAttributeByName(
    IN gcSHADER Shader,
    IN gctSTRING Name,
    IN gctUINT32 NameLength,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_ReallocateUniforms
**
**  Reallocate an array of pointers to gcUNIFORM objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateUniforms(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/* find the uniform with Name in the Shader,
 * if found, return it in *Uniform
 * otherwise add the uniform to shader
 */
gceSTATUS
gcSHADER_FindAddUniform(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                               gcSHADER_AddUniform
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_AddUniform(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    OUT gcUNIFORM * Uniform
    );


/*******************************************************************************
**                               gcSHADER_AddUniformEx
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**      gcSHADER_PRECISION precision
**          Precision of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_AddUniformEx(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION precision,
    IN gctUINT32 Length,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                               gcSHADER_AddUniformEx1
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**      gcSHADER_PRECISION precision
**          Precision of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**      gcSHADER_VAR_CATEGORY varCategory
**          Variable category, normal or struct.
**
**      gctUINT16 numStructureElement
**          If struct, its element number.
**
**      gctINT16 parent
**          If struct, parent index in gcSHADER.variables.
**
**      gctINT16 prevSibling
**          If struct, previous sibling index in gcSHADER.variables.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
**
**      gctINT16* ThisUniformIndex
**          Returned value about uniform index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddUniformEx1(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION precision,
    IN gctUINT32 Length,
    IN gctINT    IsArray,
    IN gcSHADER_VAR_CATEGORY varCategory,
    IN gctUINT16 numStructureElement,
    IN gctINT16 parent,
    IN gctINT16 prevSibling,
    OUT gctINT16* ThisUniformIndex,
    OUT gcUNIFORM * Uniform
    );

/* create uniform for the constant vector and initialize it with Value */
gceSTATUS
gcSHADER_CreateConstantUniform(
    IN gcSHADER                  Shader,
    IN gcSHADER_TYPE             Type,
    IN gcsValue *                Value,
    OUT gcUNIFORM *              Uniform
    );

gcSL_FORMAT
gcGetFormatFromType(
    IN gcSHADER_TYPE Type
    );

/*******************************************************************************
**                          gcSHADER_GetUniformCount
********************************************************************************
**
**    Get the number of uniforms for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                          gcSHADER_GetSamplerCount
********************************************************************************
**
**    Get the number of samplers for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of samplers.
*/
gceSTATUS
gcSHADER_GetSamplerCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                          gcSHADER_GetKernelUniformCount
********************************************************************************
**
**    Get the number of kernel uniforms for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetKernelUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformVectorCountByCategory
**
**  Get the number of vectors used by uniforms for this shader according to variable
**  category.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSHADER_VAR_CATEGORY Category
**          Category of uniform.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUniformVectorCountByCategory(
    IN gcSHADER Shader,
    IN gcSHADER_VAR_CATEGORY Category,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformVectorCount
**
**  Get the number of vectors used by uniforms for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUniformVectorCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );
/*******************************************************************************
**  gcSHADER_GetUniformBlockCount
**
**  Get the number of uniform blocks for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of uniform blocks.
*/
gceSTATUS
gcSHADER_GetUniformBlockCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockUniformCount
**
**  Get the number of uniforms in a uniform block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to uniform block to retreive the uniform count.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetUniformBlockUniformCount(
    IN gcSHADER Shader,
    gcsUNIFORM_BLOCK UniformBlock,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                             gcSHADER_GetUniform
********************************************************************************
**
**    Get the gcUNIFORM object pointer for an indexed uniform for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of the uniform to retrieve.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniform(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlock
**
**  Get the gcsUNIFORM_BLOCK object pointer for an indexed uniform block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of uniform to retreive the name for.
**
**  OUTPUT:
**
**      gcsUNIFORM_BLOCK * UniformBlock
**          Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
*/
gceSTATUS
gcSHADER_GetUniformBlock(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcsUNIFORM_BLOCK * UniformBlock
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockUniform
**
**  Get the gcUNIFORM object pointer for an indexed uniform of an indexed uniform
**  block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to uniform block to retreive the uniform.
**
**      gctUINT Index
**          Index of uniform to retreive the name for.
**
**  OUTPUT:
**
**      gcUNIFORM * Uniform
**          Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniformBlockUniform(
    IN gcSHADER Shader,
    IN gcsUNIFORM_BLOCK UniformBlock,
    IN gctUINT Index,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                             gcSHADER_GetUniformIndexingRange
********************************************************************************
**
**    Get the gcUNIFORM object pointer for an indexed uniform for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctINT uniformIndex
**            Index of the start uniform.
**
**        gctINT offset
**            Offset to indexing.
**
**    OUTPUT:
**
**        gctINT * LastUniformIndex
**            Pointer to index of last uniform in indexing range.
**
**        gctINT * OffsetUniformIndex
**            Pointer to index of uniform that indexing at offset.
**
**        gctINT * DeviationInOffsetUniform
**            Pointer to offset in uniform picked up.
*/
gceSTATUS
gcSHADER_GetUniformIndexingRange(
    IN gcSHADER Shader,
    IN gctINT uniformIndex,
    IN gctINT offset,
    OUT gctINT * LastUniformIndex,
    OUT gctINT * OffsetUniformIndex,
    OUT gctINT * DeviationInOffsetUniform
    );

/*******************************************************************************
**                             gcSHADER_GetUniformByPhysicalAddress
********************************************************************************
**
**    Get the gcUNIFORM object pointer by physical address for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctINT Index
**            physical address of the uniform.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniformByPhysicalAddress(
    IN gcSHADER Shader,
    IN gctINT PhysicalAddress,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                             gcSHADER_ComputeUniformPhysicalAddress
********************************************************************************
**
**    Compuate the gcUNIFORM object pointer for this shader.
**
**    INPUT:
**
**        gctUINT32 VsBaseAddress
**            Vertex Base physical address for the uniform.
**
**        gctUINT32 PsBaseAddress
**            Fragment Base physical address for the uniform.
**
**        gcUNIFORM Uniform
**            The uniform pointer.
**
**    OUTPUT:
**
**        gctUINT32 * PhysicalAddress
**            Pointer to a variable receiving the physical address.
*/
gceSTATUS
gcSHADER_ComputeUniformPhysicalAddress(
    IN gctUINT32 VsBaseAddress,
    IN gctUINT32 PsBaseAddress,
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * PhysicalAddress
    );

/*******************************************************************************
**                             gcSHADER_GetUserDefinedUniformCount
********************************************************************************
**
**    Get the number of vectors used by user defined uniforms for this shader according to variable
**    category.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * UniformCount
**            Pointer to a variable receiving the number of vectors.
**
**        gctUINT32 * SamplerCount
**            Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUserDefinedUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * UniformCount,
    OUT gctUINT32 * SamplerCount
    );

/*******************************************************************************
**  gcSHADER_ReallocateUniformBlocks
**
**  Reallocate an array of pointers to gcUNIFORM_BLOCK objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateUniformBlocks(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                    gcSHADER_AddUniformBlock
********************************************************************************
**
**    Add a uniform block to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform block to add.
**
**        gcsSHADER_VAR_INFO *BlockInfo
**            block info associated with uniform block to be added.
**
**        gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout;
**             Memory layout qualifier for members in block
**
**    OUTPUT:
**
**        gcsUNIFORM_BLOCK * Uniform
**            Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
**
*/
gceSTATUS
gcSHADER_AddUniformBlock(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcsSHADER_VAR_INFO *BlockInfo,
    gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout,
    OUT gcsUNIFORM_BLOCK * UniformBlock
    );

/*******************************************************************************
**  gcSHADER_GetFunctionByName
**
**  Get the gcFUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionName
**          Name of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcFUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetFunctionByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING FunctionName,
    OUT gcFUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetFunctionByHeadIndex
**
**  Get the gcFUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionHeadIndex
**          Head index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcFUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetFunctionByHeadIndex(
    IN     gcSHADER         Shader,
    IN     gctUINT HeadIndex,
    OUT    gcFUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetKernelFunctionByHeadIndex
**
**  Get the gcKERNEL_FUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionHeadIndex
**          Head index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcKERNEL_FUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetKernelFunctionByHeadIndex(
    IN  gcSHADER            Shader,
    IN  gctUINT             HeadIndex,
    OUT gcKERNEL_FUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetKernelFucntion
**
**  Get the gcKERNEL_FUNCTION object pointer for an indexed kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcKERNEL_FUNCTION * KernelFunction
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetKernelFunction(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

gceSTATUS
gcSHADER_GetKernelFunctionByName(
    IN gcSHADER Shader,
    IN gctSTRING KernelName,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

/*******************************************************************************
**  gcSHADER_GetKernelFunctionCount
**
**  Get the number of kernel functions for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of kernel functions.
*/
gceSTATUS
gcSHADER_GetKernelFunctionCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcSHADER_LinkLibFunction(
    IN OUT gcSHADER         Shader,
    IN     gcSHADER         Library,
    IN     gctCONST_STRING  FunctionName,
    OUT    gcFUNCTION *     Function
    );

/*******************************************************************************
**  gcSHADER_ReallocateOutputs
**
**  Reallocate an array of pointers to gcOUTPUT objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateOutputs(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                               gcSHADER_AddOutput
********************************************************************************
**
**    Add an output to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the output to add.
**
**        gcSHADER_TYPE Type
**            Type of the output to add.
**
**        gctUINT32 Length
**            Array length of the output to add.  'Length' must be at least 1.
**
**        gctUINT16 TempRegister
**            Temporary register index that holds the output value.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOutput(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctUINT16 TempRegister
    );

/*******************************************************************************
**  gcSHADER_AddOutputEx
**
**  Add an output to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the output to add.
**
**      gcSHADER_TYPE Type
**          Type of the output to add.
**
**    gcSHADER_PRECISION Precision
**          Precision of the output.
**
**      gctUINT32 Length
**          Array length of the output to add.  'Length' must be at least 1.
**
**      gctUINT16 TempRegister
**          Temporary register index that holds the output value.
**
**  OUTPUT:
**
**      gcOUTPUT * Output
**          Pointer to an output receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_AddOutputEx(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 Length,
    IN gctUINT16 TempRegister,
    IN gctBOOL IsInvariant,
    IN gcSHADER_SHADERMODE shaderMode,
    OUT gcOUTPUT * Output
    );

/*******************************************************************************
**  gcSHADER_AddOutputWithLocation
**
**  Add an output with an associated location to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the output to add.
**
**      gcSHADER_TYPE Type
**          Type of the output to add.
**
**    gcSHADER_PRECISION Precision
**          Precision of the output.
**
**      gctUINT32 Length
**          Array length of the output to add.  'Length' must be at least 1.
**
**      gctUINT16 TempRegister
**          Temporary register index that holds the output value.
**
**      gctINT Location
**          Location associated with the output.
**
**  OUTPUT:
**
**      gcOUTPUT * Output
**          Pointer to an output receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_AddOutputWithLocation(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 Length,
    IN gctUINT16 TempRegister,
    IN gcSHADER_SHADERMODE shaderMode,
    IN gctINT Location,
    IN gctBOOL IsInvariant,
    OUT gcOUTPUT * Output
    );

gctBOOL
gcSHADER_IsHaltiCompiler(
    IN gcSHADER Shader
    );
gceSTATUS
gcSHADER_AddOutputIndexed(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gctUINT32 Index,
    IN gctUINT16 TempIndex
    );

gceSTATUS
gcSHADER_AddLocation(
    IN gcSHADER Shader,
    IN gctINT Location,
    IN gctSIZE_T Length);
/*******************************************************************************
**  gcOUTPUT_SetType
**
**  Set the type of an output.
**
**  INPUT:
**
**      gcOUTPUT Output
**          Pointer to a gcOUTPUT object.
**
**      gcSHADER_TYPE Type
**          Type of the output.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcOUTPUT_SetType(
    IN gcOUTPUT Output,
    IN gcSHADER_TYPE Type
    );

/*******************************************************************************
**                             gcSHADER_GetOutputCount
********************************************************************************
**
**    Get the number of outputs for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of outputs.
*/
gceSTATUS
gcSHADER_GetOutputCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                               gcSHADER_GetOutput
********************************************************************************
**
**    Get the gcOUTPUT object pointer for an indexed output for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of output to retrieve.
**
**    OUTPUT:
**
**        gcOUTPUT * Output
**            Pointer to a variable receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_GetOutput(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcOUTPUT * Output
    );


/*******************************************************************************
**                               gcSHADER_GetOutputByName
********************************************************************************
**
**    Get the gcOUTPUT object pointer for this shader by output name.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctSTRING name
**            Name of output to retrieve.
**
**      gctUINT32 nameLength
**          Length of name to retrieve
**
**    OUTPUT:
**
**        gcOUTPUT * Output
**            Pointer to a variable receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_GetOutputByName(
    IN gcSHADER Shader,
    IN gctSTRING name,
    IN gctUINT32 nameLength,
    OUT gcOUTPUT * Output
    );

/*******************************************************************************
**  gcSHADER_ReallocateVariables
**
**  Reallocate an array of pointers to gcVARIABLE objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateVariables(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                               gcSHADER_AddVariable
********************************************************************************
**
**    Add a variable to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the variable to add.
**
**        gcSHADER_TYPE Type
**            Type of the variable to add.
**
**        gctUINT32 Length
**            Array length of the variable to add.  'Length' must be at least 1.
**
**        gctUINT16 TempRegister
**            Temporary register index that holds the variable value.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddVariable(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctUINT16 TempRegister
    );


/*******************************************************************************
**  gcSHADER_AddVariableEx
********************************************************************************
**
**  Add a variable to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the variable to add.
**
**      gcSHADER_TYPE Type
**          Type of the variable to add.
**
**      gctUINT32 Length
**          Array length of the variable to add.  'Length' must be at least 1.
**
**      gctUINT16 TempRegister
**          Temporary register index that holds the variable value.
**
**      gcSHADER_VAR_CATEGORY varCategory
**          Variable category, normal or struct.
**
**      gctUINT16 numStructureElement
**          If struct, its element number.
**
**      gctINT16 parent
**          If struct, parent index in gcSHADER.variables.
**
**      gctINT16 prevSibling
**          If struct, previous sibling index in gcSHADER.variables.
**
**  OUTPUT:
**
**      gctINT16* ThisVarIndex
**          Returned value about variable index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddVariableEx(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctUINT16 TempRegister,
    IN gcSHADER_VAR_CATEGORY varCategory,
    IN gctUINT16 numStructureElement,
    IN gctINT16 parent,
    IN gctINT16 prevSibling,
    OUT gctINT16* ThisVarIndex
    );

/*******************************************************************************
**  gcSHADER_AddVariableEx1
**
**  Add a variable to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the variable to add.
**
**      gctUINT16 TempRegister
**          Temporary register index that holds the variable value.
**
**      gcsSHADER_VAR_INFO *VarInfo
**          Variable information struct pointer.
**
**  OUTPUT:
**
**      gctINT16* ThisVarIndex
**          Returned value about variable index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddVariableEx1(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gctUINT16 TempRegister,
    IN gcsSHADER_VAR_INFO *VarInfo,
    OUT gctINT16* ThisVarIndex
    );

/*******************************************************************************
**  gcSHADER_UpdateVariable
********************************************************************************
**
**  Update a variable to a gcSHADER object.
**
**  INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of variable to retrieve.
**
**        gceVARIABLE_UPDATE_FLAGS flag
**            Flag which property of variable will be updated.
**
**      gctUINT newValue
**          New value to update.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_UpdateVariable(
    IN gcSHADER Shader,
    IN gctUINT Index,
    IN gceVARIABLE_UPDATE_FLAGS flag,
    IN gctUINT newValue
    );

/*******************************************************************************
**                             gcSHADER_GetVariableCount
********************************************************************************
**
**    Get the number of variables for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of variables.
*/
gceSTATUS
gcSHADER_GetVariableCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetTempCount
**
**  Get the number of temp register used for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      return the Shader's temp register count, included in
**      variable, output, arguments, temporay in instruciton
 */
gctUINT
gcSHADER_GetTempCount(
    IN gcSHADER        Shader
    );

/*******************************************************************************
**                               gcSHADER_GetVariable
********************************************************************************
**
**    Get the gcVARIABLE object pointer for an indexed variable for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of variable to retrieve.
**
**    OUTPUT:
**
**        gcVARIABLE * Variable
**            Pointer to a variable receiving the gcVARIABLE object pointer.
*/
gceSTATUS
gcSHADER_GetVariable(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcVARIABLE * Variable
    );

/*******************************************************************************
**                               gcSHADER_GetVariableIndexingRange
********************************************************************************
**
**    Get the gcVARIABLE indexing range.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcVARIABLE variable
**            Start variable.
**
**        gctBOOL whole
**            Indicate whether maximum indexing range is queried
**
**    OUTPUT:
**
**        gctUINT *Start
**            Pointer to range start (temp register index).
**
**        gctUINT *End
**            Pointer to range end (temp register index).
*/
gceSTATUS
gcSHADER_GetVariableIndexingRange(
    IN gcSHADER Shader,
    IN gcVARIABLE variable,
    IN gctBOOL whole,
    OUT gctUINT *Start,
    OUT gctUINT *End
    );

/*******************************************************************************
**                               gcSHADER_AddOpcode
********************************************************************************
**
**    Add an opcode to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gctUINT16 TempRegister
**            Temporary register index that acts as the target of the opcode.
**
**        gctUINT8 Enable
**            Write enable bits for the temporary register that acts as the target
**            of the opcode.
**
**        gcSL_FORMAT Format
**            Format of the temporary register.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcode(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_FORMAT Format
    );

gceSTATUS
gcSHADER_AddOpcode2(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**                            gcSHADER_AddOpcodeIndexed
********************************************************************************
**
**    Add an opcode to a gcSHADER object that writes to an dynamically indexed
**    target.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gctUINT16 TempRegister
**            Temporary register index that acts as the target of the opcode.
**
**        gctUINT8 Enable
**            Write enable bits  for the temporary register that acts as the
**            target of the opcode.
**
**        gcSL_INDEXED Mode
**            Location of the dynamic index inside the temporary register.  Valid
**            values can be:
**
**                gcSL_INDEXED_X - Use x component of the temporary register.
**                gcSL_INDEXED_Y - Use y component of the temporary register.
**                gcSL_INDEXED_Z - Use z component of the temporary register.
**                gcSL_INDEXED_W - Use w component of the temporary register.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**        gcSL_FORMAT Format
**            Format of the temporary register.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeIndexed(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeIndexedWithPrecision
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target with precision setting.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gctUINT16 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Mode
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**      gcSHADER_PRECISION Precision
**          Precision of register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionIndexed
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition to check.
**
**      gctUINT16 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Indexed
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionIndexed(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Indexed,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionIndexedWithPrecision
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition to check.
**
**      gctUINT16 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Indexed
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**      gcSHADER_PRECISION Precision
**          Precision of register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT16 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Indexed,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                          gcSHADER_AddOpcodeConditional
********************************************************************************
**
**    Add an conditional opcode to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gcSL_CONDITION Condition
**            Condition that needs to evaluate to gcvTRUE in order for the opcode to
**            execute.
**
**        gctUINT Label
**            Target label if 'Condition' evaluates to gcvTRUE.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditional(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT Label
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionalFormatted
**
**  Add an conditional jump or call opcode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition that needs to evaluate to gcvTRUE in order for the opcode to
**          execute.
**
**      gcSL_FORMAT Format
**          Format of conditional operands
**
**      gctUINT Label
**          Target label if 'Condition' evaluates to gcvTRUE.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionalFormatted(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gcSL_FORMAT Format,
    IN gctUINT Label
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionalFormattedEnable
**
**  Add an conditional jump or call opcode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition that needs to evaluate to gcvTRUE in order for the opcode to
**          execute.
**
**      gcSL_FORMAT Format
**          Format of conditional operands
**
**      gctUINT8 Enable
**          Write enable value for the target of the opcode.
**
**      gctUINT Label
**          Target label if 'Condition' evaluates to gcvTRUE.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionalFormattedEnable(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gcSL_FORMAT Format,
    IN gctUINT8 Enable,
    IN gctUINT Label
    );

/*******************************************************************************
**  gcSHADER_FindNextUsedLabelId
**
**  Find a label id which is not used inside the gcSHADER object
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  RETURN:
**
**      the next unused label id
*/
gctUINT
gcSHADER_FindNextUsedLabelId(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                gcSHADER_AddLabel
********************************************************************************
**
**    Define a label at the current instruction of a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Label
**            Label to define.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddLabel(
    IN gcSHADER Shader,
    IN gctUINT Label
    );

/*******************************************************************************
**  gcSHADER_AddRoundingMode
**
**  Add rounding mode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_ROUND Round
**          Type of the source operand.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddRoundingMode(
    IN gcSHADER Shader,
    IN gcSL_ROUND Round
    );


/*******************************************************************************
**  gcSHADER_AddSaturation
**
**  Add saturation modifier to a gcSHADER instruction.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_MODIFIER_SAT Sat
**          Saturation modifier.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSaturation(
    IN gcSHADER Shader,
    IN gcSL_MODIFIER_SAT  Sat
    );

/*******************************************************************************
**  gcSHADER_NewTempRegs
**
**  Allocate RegCount of temp registers from the shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT RegCount
**          Count of temp register be allocated.
**
**      gcSHADER_TYPE  Type
**          Type of the temp register.
**
**
**  Return:
**
**      The start temp register index, the next available temp register
**      index is the return value plus RegCount.
*/
gctUINT32
gcSHADER_NewTempRegs(
    IN gcSHADER       Shader,
    IN gctUINT        RegCount,
    IN gcSHADER_TYPE  Type
    );

gctUINT32
gcSHADER_UpdateTempRegCount(
    IN gcSHADER       Shader,
    IN gctUINT        RegCount
    );


/*******************************************************************************
**                               gcSHADER_AddSource
********************************************************************************
**
**    Add a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_TYPE Type
**            Type of the source operand.
**
**        gctUINT16 SourceIndex
**            Index of the source operand.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gcSL_FORMAT Format
**            Format of the source operand.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSource(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT16 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**                            gcSHADER_AddSourceIndexed
********************************************************************************
**
**    Add a dynamically indexed source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_TYPE Type
**            Type of the source operand.
**
**        gctUINT16 SourceIndex
**            Index of the source operand.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gcSL_INDEXED Mode
**            Addressing mode for the index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**        gcSL_FORMAT Format
**            Format of the source operand.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceIndexed(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT16 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**  gcSHADER_AddSourceIndexedWithPrecision
**
**  Add a dynamically indexed source operand to a gcSHADER object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_TYPE Type
**          Type of the source operand.
**
**      gctUINT16 SourceIndex
**          Index of the source operand.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gcSL_INDEXED Mode
**          Addressing mode for the index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSHADER_PRECISION Precision
**        Precision of source value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT16 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                           gcSHADER_AddSourceAttribute
********************************************************************************
**
**    Add an attribute as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the attribute in case the attribute is a matrix
**            or array.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttribute(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index
    );

/*******************************************************************************
**                           gcSHADER_AddSourceAttributeIndexed
********************************************************************************
**
**    Add an indexed attribute as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the attribute in case the attribute is a matrix
**            or array.
**
**        gcSL_INDEXED Mode
**            Addressing mode of the dynamic index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttributeIndexed(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

/*******************************************************************************
**                            gcSHADER_AddSourceUniform
********************************************************************************
**
**    Add a uniform as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the uniform in case the uniform is a matrix or
**            array.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniform(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index
    );

/*******************************************************************************
**                        gcSHADER_AddSourceUniformIndexed
********************************************************************************
**
**    Add an indexed uniform as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the uniform in case the uniform is a matrix or
**            array.
**
**        gcSL_INDEXED Mode
**            Addressing mode of the dynamic index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniformIndexed(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

gceSTATUS
gcSHADER_AddSourceSamplerIndexed(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

gceSTATUS
gcSHADER_AddSourceAttributeFormatted(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_FORMAT Format
    );

gceSTATUS
gcSHADER_AddSourceAttributeIndexedFormatted(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**  gcSHADER_AddSourceSamplerIndexedFormattedWithPrecision
**
**  Add a "0-based" formatted indexed sampler as a source operand to a gcSHADER
**  object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of sampler value
**
**    gcSHADER_PRECISION Precision
**        Precision of attribute value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceSamplerIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/********************************************************************************************
**  gcSHADER_AddSourceAttributeIndexedFormattedWithPrecision
**
**  Add a formatted indexed attribute as a source operand to a gcSHADER object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcATTRIBUTE Attribute
**          Pointer to a gcATTRIBUTE object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gctINT Index
**          Static index into the attribute in case the attribute is a matrix
**          or array.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of indexed attribute value
**
**    gcSHADER_PRECISION Precision
**        Precision of attribute value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttributeIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

gceSTATUS
gcSHADER_AddSourceUniformFormatted(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_FORMAT Format
    );

gceSTATUS
gcSHADER_AddSourceUniformIndexedFormatted(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/******************************************************************************************
**  gcSHADER_AddSourceUniformIndexedFormattedWithPrecision
**
**  Add a formatted indexed uniform as a source operand to a gcSHADER object with Precision
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gctINT Index
**          Static index into the uniform in case the uniform is a matrix or
**          array.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of uniform value
**
**    gcSHADER_PRECISION Precision
**        Precision of uniform value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniformIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

gceSTATUS
gcSHADER_AddSourceSamplerIndexedFormatted(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**                           gcSHADER_AddSourceConstant
********************************************************************************
**
**    Add a constant floating point value as a source operand to a gcSHADER
**    object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctFLOAT Constant
**            Floating point constant.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstant(
    IN gcSHADER Shader,
    IN gctFLOAT Constant
    );

/*******************************************************************************
**                               gcSHADER_AddSourceConstantFormatted
********************************************************************************
**
**    Add a constant value as a source operand to a gcSHADER
**    object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        void * Constant
**            Pointer to constant.
**
**        gcSL_FORMAT Format
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstantFormatted(
    IN gcSHADER Shader,
    IN void *Constant,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**    gcSHADER_AddSourceConstantFormattedWithPrecision
**
**    Add a formatted constant value as a source operand to a gcSHADER object
**    with precision.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        void *Constant
**            Pointer to constant value (32 bits).
**
**        gcSL_FORMAT Format
**            Format of constant value
**
**    gcSHADER_PRECISION Precision
**        Precision of constant value
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstantFormattedWithPrecision(
    IN gcSHADER Shader,
    IN void *Constant,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                                  gcSHADER_Pack
********************************************************************************
**
**    Pack a dynamically created gcSHADER object by trimming the allocated arrays
**    and resolving all the labeling.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Pack(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                  gcSHADER_IsRecursion
********************************************************************************
**
** Detect if there is a recursive function in the shader code
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_IsRecursion(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                gcSHADER_SetOptimizationOption
********************************************************************************
**
**    Set optimization option of a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT OptimizationOption
**            Optimization option.  Can be one of the following:
**
**                0                        - No optimization.
**                1                        - Full optimization.
**                Other value                - For optimizer testing.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_SetOptimizationOption(
    IN gcSHADER Shader,
    IN gctUINT OptimizationOption
    );

/*******************************************************************************
**  gcSHADER_ReallocateFunctions
**
**  Reallocate an array of pointers to gcFUNCTION objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateFunctions(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

gceSTATUS
gcSHADER_AddFunction(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    OUT gcFUNCTION * Function
    );

gceSTATUS
gcSHADER_ReallocateKernelFunctions(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

gceSTATUS
gcSHADER_AddKernelFunction(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

gceSTATUS
gcSHADER_BeginFunction(
    IN gcSHADER Shader,
    IN gcFUNCTION Function
    );

gceSTATUS
gcSHADER_EndFunction(
    IN gcSHADER Shader,
    IN gcFUNCTION Function
    );

gceSTATUS
gcSHADER_BeginKernelFunction(
    IN gcSHADER Shader,
    IN gcKERNEL_FUNCTION KernelFunction
    );

gceSTATUS
gcSHADER_EndKernelFunction(
    IN gcSHADER Shader,
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 LocalMemorySize
    );

gceSTATUS
gcSHADER_SetMaxKernelFunctionArgs(
    IN gcSHADER Shader,
    IN gctUINT32 MaxKernelFunctionArgs
    );

/*******************************************************************************
**  gcSHADER_SetConstantMemorySize
**
**  Set the constant memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 ConstantMemorySize
**          Constant memory size in bytes
**
**      gctCHAR *ConstantMemoryBuffer
**          Constant memory buffer
*/
gceSTATUS
gcSHADER_SetConstantMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 ConstantMemorySize,
    IN gctCHAR * ConstantMemoryBuffer
    );

/*******************************************************************************
**  gcSHADER_GetConstantMemorySize
**
**  Set the constant memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * ConstantMemorySize
**          Pointer to a variable receiving constant memory size in bytes
**
**      gctCHAR **ConstantMemoryBuffer.
**          Pointer to a variable for returned shader constant memory buffer.
*/
gceSTATUS
gcSHADER_GetConstantMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * ConstantMemorySize,
    OUT gctCHAR ** ConstantMemoryBuffer
    );

/*******************************************************************************
**  gcSHADER_SetPrivateMemorySize
**
**  Set the private memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 PrivateMemorySize
**          Private memory size in bytes
*/
gceSTATUS
gcSHADER_SetPrivateMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 PrivateMemorySize
    );

/*******************************************************************************
**  gcSHADER_GetPrivateMemorySize
**
**  Set the private memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * PrivateMemorySize
**          Pointer to a variable receiving private memory size in bytes
*/
gceSTATUS
gcSHADER_GetPrivateMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * PrivateMemorySize
    );

/*******************************************************************************
**  gcSHADER_SetLocalMemorySize
**
**  Set the local memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 LocalMemorySize
**          Local memory size in bytes
*/
gceSTATUS
gcSHADER_SetLocalMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 LocalMemorySize
    );

/*******************************************************************************
**  gcSHADER_GetLocalMemorySize
**
**  Set the local memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * LocalMemorySize
**          Pointer to a variable receiving lcoal memory size in bytes
*/
gceSTATUS
gcSHADER_GetLocalMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * LocalMemorySize
    );


/*******************************************************************************
**  gcSHADER_CheckValidity
**
**  Check validity for a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
*/
gceSTATUS
gcSHADER_CheckValidity(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                               gcSHADER_GetVariableTempTypes
********************************************************************************
**
**    Get the gcVARIABLE temp types and save the type to TempTypeArray.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcVARIABLE variable
**            Start variable.
**
**        gctUINT TempTypeArraySize
**            The size of temp type array.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * TempTypeArray
**            Pointer to temp type array
**
*/
gceSTATUS
gcSHADER_GetVariableTempTypes(
    IN gcSHADER            Shader,
    IN gcVARIABLE          Variable,
    IN gctUINT             TempTypeArraySize,
    IN gctINT              FisrtTempIndex,
    OUT gcSHADER_TYPE *    TempTypeArray
    );

/*******************************************************************************
**                                gcSHADER_ConvertBooleanUniform
********************************************************************************
**
**    Convert boolean uniforms to 0 or 1.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_ConvertBooleanUniform(
    IN gcSHADER Shader
    );

gceSTATUS
gcATTRIBUTE_IsPosition(
        IN gcATTRIBUTE Attribute,
        OUT gctBOOL * IsPosition
        );

/*******************************************************************************
**  gcATTRIBUTE_SetPrecision
**
**  Set the precision of an attribute.
**
**  INPUT:
**
**      gcATTRIBUTE Attribute
**          Pointer to a gcATTRIBUTE object.
**
**    gcSHADER_PRECISION Precision
**          Precision of the attribute.
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcATTRIBUTE_SetPrecision(
    IN gcATTRIBUTE Attribute,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                             gcATTRIBUTE_GetType
********************************************************************************
**
**    Get the type and array length of a gcATTRIBUTE object.
**
**    INPUT:
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the attribute.  'Type'
**            can be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            attribute was declared as an array.  If the attribute was not
**            declared as an array, the array length will be 1.  'ArrayLength' can
**            be gcvNULL, in which case no array length will be returned.
*/
gceSTATUS
gcATTRIBUTE_GetType(
    IN gcATTRIBUTE Attribute,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

gceSTATUS
gcATTRIBUTE_GetLocation(
    IN gcATTRIBUTE Attribute,
    OUT gctINT * Location
    );


/*******************************************************************************
**                            gcATTRIBUTE_GetName
********************************************************************************
**
**    Get the name of a gcATTRIBUTE object.
**
**    INPUT:
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the attribute name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the attribute name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcATTRIBUTE_GetName(
    IN gcATTRIBUTE Attribute,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**                            gcATTRIBUTE_IsEnabled
********************************************************************************
**
**    Query the enabled state of a gcATTRIBUTE object.
**
**    INPUT:
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gctBOOL * Enabled
**            Pointer to a variable receiving the enabled state of the attribute.
*/
gceSTATUS
gcATTRIBUTE_IsEnabled(
    IN gcATTRIBUTE Attribute,
    OUT gctBOOL * Enabled
    );

gceSTATUS
gcATTRIBUTE_GetIndex(
    IN gcATTRIBUTE Attribute,
    OUT gctUINT16 * Index
    );

/*******************************************************************************
**                              gcUNIFORM_GetType
********************************************************************************
**
**    Get the type and array length of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the uniform.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            uniform was declared as an array.  If the uniform was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetType(
    IN gcUNIFORM Uniform,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                              gcUNIFORM_GetTypeEx
********************************************************************************
**
**    Get the type and array length of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the uniform.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gcSHADER_PRECISION * Precision
**            Pointer to a variable receiving the precision of the uniform.  'Precision' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            uniform was declared as an array.  If the uniform was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetTypeEx(
    IN gcUNIFORM Uniform,
    OUT gcSHADER_TYPE * Type,
    OUT gcSHADER_PRECISION * Precision,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                              gcUNIFORM_GetFlags
********************************************************************************
**
**    Get the flags of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gceUNIFORM_FLAGS * Flags
**            Pointer to a variable receiving the flags of the uniform.
**
*/
gceSTATUS
gcUNIFORM_GetFlags(
    IN gcUNIFORM Uniform,
    OUT gceUNIFORM_FLAGS * Flags
    );

/*******************************************************************************
**                              gcUNIFORM_SetFlags
********************************************************************************
**
**    Set the flags of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gceUNIFORM_FLAGS Flags
**            Flags of the uniform to be set.
**
**    OUTPUT:
**            Nothing.
**
*/
gceSTATUS
gcUNIFORM_SetFlags(
    IN gcUNIFORM Uniform,
    IN gceUNIFORM_FLAGS Flags
    );

/*******************************************************************************
**                              gcUNIFORM_GetName
********************************************************************************
**
**    Get the name of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the uniform name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the uniform name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcUNIFORM_GetName(
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**  gcUNIFORM_BLOCK_GetName
**
**  Get the name of a gcsUNIFORM_BLOCK object.
**
**  INPUT:
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to a gcsUNIFORM_BLOCK object.
**
**  OUTPUT:
**
**      gctUINT32 * Length
**          Pointer to a variable receiving the length of the uniform block name.
**          'Length' can be gcvNULL, in which case no length will be returned.
**
**      gctCONST_STRING * Name
**          Pointer to a variable receiving the pointer to the uniform block name.
**          'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcUNIFORM_BLOCK_GetName(
    IN gcsUNIFORM_BLOCK UniformBlock,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**                              gcUNIFORM_GetSampler
********************************************************************************
**
**    Get the physical sampler number for a sampler gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gctUINT32 * Sampler
**            Pointer to a variable receiving the physical sampler.
*/
gceSTATUS
gcUNIFORM_GetSampler(
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * Sampler
    );

gceSTATUS
gcUNIFORM_GetIndex(
    IN gcUNIFORM Uniform,
    OUT gctUINT16 * Index
    );

/*******************************************************************************
**  gcUNIFORM_GetFormat
**
**  Get the type and array length of a gcUNIFORM object.
**
**  INPUT:
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**  OUTPUT:
**
**      gcSL_FORMAT * Format
**          Pointer to a variable receiving the format of element of the uniform.
**          'Type' can be gcvNULL, in which case no type will be returned.
**
**      gctBOOL * IsPointer
**          Pointer to a variable receiving the state wheter the uniform is a pointer.
**          'IsPointer' can be gcvNULL, in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetFormat(
    IN gcUNIFORM Uniform,
    OUT gcSL_FORMAT * Format,
    OUT gctBOOL * IsPointer
    );

/*******************************************************************************
**  gcUNIFORM_SetFormat
**
**  Set the format and isPointer of a uniform.
**
**  INPUT:
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**      gcSL_FORMAT Format
**          Format of element of the uniform shaderType.
**
**      gctBOOL IsPointer
**          Wheter the uniform is a pointer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcUNIFORM_SetFormat(
    IN gcUNIFORM Uniform,
    IN gcSL_FORMAT Format,
    IN gctBOOL IsPointer
    );

/*******************************************************************************
**                               gcUNIFORM_SetValue
********************************************************************************
**
**    Set the value of a uniform in integer.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT32 Count
**            Number of entries to program if the uniform has been declared as an
**            array.
**
**        const gctINT * Value
**            Pointer to a buffer holding the integer values for the uniform.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcUNIFORM_SetValue(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN const gctINT * Value
    );

/*******************************************************************************
**                               gcUNIFORM_SetValueF
********************************************************************************
**
**    Set the value of a uniform in floating point.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT32 Count
**            Number of entries to program if the uniform has been declared as an
**            array.
**
**        const gctFLOAT * Value
**            Pointer to a buffer holding the floating point values for the
**            uniform.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcUNIFORM_SetValueF(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN const gctFLOAT * Value
    );

/*******************************************************************************
**                         gcUNIFORM_GetModelViewProjMatrix
********************************************************************************
**
**    Get the value of uniform modelViewProjMatrix ID if present.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        Nothing.
*/
gctUINT
gcUNIFORM_GetModelViewProjMatrix(
    IN gcUNIFORM Uniform
    );

/*******************************************************************************
**                                gcOUTPUT_GetType
********************************************************************************
**
**    Get the type and array length of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the output.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            output was declared as an array.  If the output was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcOUTPUT_GetType(
    IN gcOUTPUT Output,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                               gcOUTPUT_GetIndex
********************************************************************************
**
**    Get the index of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**
**        gctUINT * Index
**            Pointer to a variable receiving the temporary register index of the
**            output.  'Index' can be gcvNULL,. in which case no index will be
**            returned.
*/
gceSTATUS
gcOUTPUT_GetIndex(
    IN gcOUTPUT Output,
    OUT gctUINT * Index
    );

/*******************************************************************************
**                                gcOUTPUT_GetName
********************************************************************************
**
**    Get the name of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the output name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the output name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcOUTPUT_GetName(
    IN gcOUTPUT Output,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**  gcOUTPUT_GetLocation
**
**  Get the Location of a gcOUTPUT object.
**
**  INPUT:
**
**      gcOUTPUT Output
**          Pointer to a gcOUTPUT object.
**
**  OUTPUT:
**
**      gctUINT * Location
**          Pointer to a variable receiving the Location of the
**          output.
*/
gceSTATUS
gcOUTPUT_GetLocation(
    IN gcOUTPUT Output,
    OUT gctUINT * Location
    );


/*******************************************************************************
*********************************************************** F U N C T I O N S **
*******************************************************************************/

/*******************************************************************************
**  gcFUNCTION_ReallocateArguments
**
**  Reallocate an array of gcsFUNCTION_ARGUMENT objects.
**
**  INPUT:
**
**      gcFUNCTION Function
**          Pointer to a gcFUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcFUNCTION_ReallocateArguments(
    IN gcFUNCTION Function,
    IN gctUINT32 Count
    );

gceSTATUS
gcFUNCTION_AddArgument(
    IN gcFUNCTION Function,
    IN gctUINT16 TempIndex,
    IN gctUINT8 Enable,
    IN gctUINT8 Qualifier
    );

gceSTATUS
gcFUNCTION_GetArgument(
    IN gcFUNCTION Function,
    IN gctUINT16 Index,
    OUT gctUINT16_PTR Temp,
    OUT gctUINT8_PTR Enable,
    OUT gctUINT8_PTR Swizzle
    );

gceSTATUS
gcFUNCTION_GetLabel(
    IN gcFUNCTION Function,
    OUT gctUINT_PTR Label
    );

/*******************************************************************************
************************* K E R N E L    P R O P E R T Y    F U N C T I O N S **
*******************************************************************************/
/*******************************************************************************/
gceSTATUS
gcKERNEL_FUNCTION_AddKernelFunctionProperties(
        IN gcKERNEL_FUNCTION KernelFunction,
        IN gctINT propertyType,
        IN gctUINT32 propertySize,
        IN gctINT * values
        );

gceSTATUS
gcKERNEL_FUNCTION_GetPropertyCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetProperty(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gctUINT32 * propertySize,
    OUT gctINT * propertyType,
    OUT gctINT * propertyValues
    );


/*******************************************************************************
*******************************I M A G E   S A M P L E R    F U N C T I O N S **
*******************************************************************************/
/*******************************************************************************
**  gcKERNEL_FUNCTION_ReallocateImageSamplers
**
**  Reallocate an array of pointers to image sampler pair.
**
**  INPUT:
**
**      gcKERNEL_FUNCTION KernelFunction
**          Pointer to a gcKERNEL_FUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcKERNEL_FUNCTION_ReallocateImageSamplers(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddImageSampler(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT8 ImageNum,
    IN gctBOOL IsConstantSamplerType,
    IN gctUINT32 SamplerType
    );

gceSTATUS
gcKERNEL_FUNCTION_GetImageSamplerCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetImageSampler(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gctUINT8 *ImageNum,
    OUT gctBOOL *IsConstantSamplerType,
    OUT gctUINT32 *SamplerType
    );

/*******************************************************************************
*********************************************K E R N E L    F U N C T I O N S **
*******************************************************************************/

/*******************************************************************************
**  gcKERNEL_FUNCTION_ReallocateArguments
**
**  Reallocate an array of gcsFUNCTION_ARGUMENT objects.
**
**  INPUT:
**
**      gcKERNEL_FUNCTION Function
**          Pointer to a gcKERNEL_FUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcKERNEL_FUNCTION_ReallocateArguments(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddArgument(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT16 TempIndex,
    IN gctUINT8 Enable,
    IN gctUINT8 Qualifier
    );

gceSTATUS
gcKERNEL_FUNCTION_GetArgument(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT16 Index,
    OUT gctUINT16_PTR Temp,
    OUT gctUINT8_PTR Enable,
    OUT gctUINT8_PTR Swizzle
    );

gceSTATUS
gcKERNEL_FUNCTION_GetLabel(
    IN gcKERNEL_FUNCTION Function,
    OUT gctUINT_PTR Label
    );

gceSTATUS
gcKERNEL_FUNCTION_GetName(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

gceSTATUS
gcKERNEL_FUNCTION_ReallocateUniformArguments(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddUniformArgument(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    OUT gcUNIFORM * UniformArgument
    );

gceSTATUS
gcKERNEL_FUNCTION_GetUniformArgumentCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetUniformArgument(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gcUNIFORM * UniformArgument
    );

gceSTATUS
gcKERNEL_FUNCTION_SetCodeEnd(
    IN gcKERNEL_FUNCTION KernelFunction
    );

/*******************************************************************************
**                              gcInitializeCompiler
********************************************************************************
**
**  Initialize compiler global variables.
**
**  Input:
**      gcoHAL Hal
**      Pointer to HAL object
**
**      gcsGLSLCaps *Caps
**      Min/Max capabilities filled in by driver and passed to compiler
**
**  Output:
**      Nothing
**
*/
gceSTATUS
gcInitializeCompiler(
    IN gcoHAL Hal,
    IN gcsGLSLCaps *Caps
    );

/*******************************************************************************
**                              gcFinalizeCompiler
********************************************************************************
**  Finalize compiler global variables.
**
**  Input:
**      gcoHAL Hal
**      Pointer to HAL object
**
**  Output:
**      Nothing
**
*/
gceSTATUS
gcFinalizeCompiler(
    IN gcoHAL Hal
    );

/*******************************************************************************
**                              gcCompileShader
********************************************************************************
**
**    Compile a shader.
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctINT ShaderType
**            Shader type to compile.  Can be one of the following values:
**
**                gcSHADER_TYPE_VERTEX
**                    Compile a vertex shader.
**
**                gcSHADER_TYPE_FRAGMENT
**                    Compile a fragment shader.
**
**        gctUINT SourceSize
**            Size of the source buffer in bytes.
**
**        gctCONST_STRING Source
**            Pointer to the buffer containing the shader source code.
**
**    OUTPUT:
**
**        gcSHADER * Binary
**            Pointer to a variable receiving the pointer to a gcSHADER object
**            containg the compiled shader code.
**
**        gctSTRING * Log
**            Pointer to a variable receiving a string pointer containging the
**            compile log.
*/
gceSTATUS
gcCompileShader(
    IN gcoHAL Hal,
    IN gctINT ShaderType,
    IN gctUINT SourceSize,
    IN gctCONST_STRING Source,
    OUT gcSHADER * Binary,
    OUT gctSTRING * Log
    );

/*******************************************************************************
**                              gcOptimizeShader
********************************************************************************
**
**    Optimize a shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object holding information about the compiled
**            shader.
**
**        gctFILE LogFile
**            Pointer to an open FILE object.
*/
gceSTATUS
gcOptimizeShader(
    IN gcSHADER Shader,
    IN gctFILE LogFile
    );

/*******************************************************************************
**                                gcLinkShaders
********************************************************************************
**
**    Link two shaders and generate a harwdare specific state buffer by compiling
**    the compiler generated code through the resource allocator and code
**    generator.
**
**    INPUT:
**
**        gcSHADER VertexShader
**            Pointer to a gcSHADER object holding information about the compiled
**            vertex shader.
**
**        gcSHADER FragmentShader
**            Pointer to a gcSHADER object holding information about the compiled
**            fragment shader.
**
**        gceSHADER_FLAGS Flags
**            Compiler flags.  Can be any of the following:
**
**                gcvSHADER_DEAD_CODE       - Dead code elimination.
**                gcvSHADER_RESOURCE_USAGE  - Resource usage optimizaion.
**                gcvSHADER_OPTIMIZER       - Full optimization.
**                gcvSHADER_USE_GL_Z        - Use OpenGL ES Z coordinate.
**                gcvSHADER_USE_GL_POSITION - Use OpenGL ES gl_Position.
**                gcvSHADER_USE_GL_FACE     - Use OpenGL ES gl_FaceForward.
**
**    OUTPUT:
**
**        gctUINT32 * StateBufferSize
**            Pointer to a variable receicing the number of bytes in the buffer
**            returned in 'StateBuffer'.
**
**        gctPOINTER * StateBuffer
**            Pointer to a variable receiving a buffer pointer that contains the
**            states required to download the shaders into the hardware.
**
**        gcsHINT_PTR * Hints
**            Pointer to a variable receiving a gcsHINT structure pointer that
**            contains information required when loading the shader states.
*/
gceSTATUS
gcLinkShaders(
    IN gcSHADER VertexShader,
    IN gcSHADER FragmentShader,
    IN gceSHADER_FLAGS Flags,
    OUT gctUINT32 * StateBufferSize,
    OUT gctPOINTER * StateBuffer,
    OUT gcsHINT_PTR * Hints
    );

/* dynamic patch functions */
/* utility function to constuct patch info */

/* create a format covertion directive with specified sampler name
 * and sampler info, *PatchDirectivePtr must point to NULL at the
 * first to this routine, each subsequent call to this routine create
 *  a new directive and link with the previous one
 *
 *  Multiple-Layer support:
 *    for some format, it is splited to multiple layers, the  SplitLayers
 *    is the extra layers it splits to, 0 means no extra layer to split,
 *    the multi-layer sampler uniform will be created later when doing
 *    dynmaic shader patch, and the driver need to  bind the split
 *    multi-layer texture objects to the multi-layer sampler uniforms
 */
gceSTATUS
gcCreateInputConversionDirective(
    IN gcUNIFORM               Sampler,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    IN gctCONST_STRING         FormatName,
    IN gceTEXTURE_SWIZZLE *    Swizzle,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateOutputConversionDirective(
    IN gctINT                  OutputLocation,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateDepthComparisonDirective(
    IN gcsSURF_FORMAT_INFO_PTR SamplerInfo,
    IN gcUNIFORM               Sampler,
    IN gctUINT                 CompMode,
    IN gctUINT                 CompFunction,
    OUT gcPatchDirective **    PatchDirectivePtr
    );

gceSTATUS
gcIsSameDepthComparisonDirectiveExist(
    IN gcsSURF_FORMAT_INFO_PTR SamplerInfo,
    IN gcUNIFORM               Sampler,
    IN gctUINT                 CompMode,
    IN gctUINT                 CompFunction,
    IN gcPatchDirective *      PatchDirectivePtr
    );

gceSTATUS
gcCreateColorFactoringDirective(
    IN gctINT                  RenderTagets,
    IN gctFLOAT *              FactorValue,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateAlphaBlendingDirective(
    IN gctINT                  OutputLocation,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateNP2TextureDirective(
    IN gctINT TextureCount,
    IN gcNPOT_PATCH_PARAM_PTR NP2Texture,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreatePreRotationDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr,
    IN  gceSURF_ROTATION       Rotation
    );

gceSTATUS
gcCreateGlobalWorkSizeDirective(
    IN gcUNIFORM                GlobalWidth,
    IN gcUNIFORM                GroupWidth,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateReadImageDirective(
    IN gctUINT                  SamplerNum,
    IN gctUINT                  ImageDataIndex,
    IN gctUINT                  ImageSizeIndex,
    IN gctUINT                  SamplerValue,
    IN gctUINT                  ChannelDataType,
    IN gctUINT                  ChannelOrder,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateWriteImageDirective(
    IN gctUINT                  SamplerNum,
    IN gctUINT                  ImageDataIndex,
    IN gctUINT                  ImageSizeIndex,
    IN gctUINT                  ChannelDataType,
    IN gctUINT                  ChannelOrder,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateRemoveAssignmentForAlphaChannel(
    IN gctBOOL *               RemoveOutputAlpha,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateYFlippedShaderDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateSampleMaskDirective(
    IN gctBOOL                 AlphaToConverageEnabled,
    IN gctBOOL                 SampleConverageEnabled,
    IN gctBOOL                 SampleMaskEnabled,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcDestroyPatchDirective(
    IN OUT gcPatchDirective **  PatchDirectivePtr
    );

gceSTATUS
gcLoadCLPatchLibrary(
    void
    );

/* Query samplers ids for PrimarySamplerID in PatchDirectivePtr
 * if found PrimarySamplerID in the directive, return layers
 * in *Layers, all sampler id (physical) in SamplersID
 */
gceSTATUS
gcQueryFormatConvertionDirectiveSampler(
    IN   gcPatchDirective *   PatchDirectivePtr,
    IN   gctUINT              PrimarySamplerID,
    OUT  gctUINT *            SamplersID,
    OUT  gctUINT *            Layers,
    OUT  gctBOOL *            Swizzled
    );

/* Query compiler generated output locations for PrimaryOutputLocation
 * in PatchDirectivePtr, if PrimaryOutputLocation is found in the directive,
 * return layers in *Layers, all outputs location in OutputsLocation
 */
gceSTATUS
gcQueryOutputConversionDirective(
    IN   gcPatchDirective *   PatchDirectivePtr,
    IN   gctUINT              PrimaryOutputLocation,
    OUT  gctUINT *            OutputsLocation,
    OUT  gctUINT *            Layers
    );

/* daynamic patch shader */
gceSTATUS gcSHADER_DynamicPatch(
    IN OUT gcSHADER         Shader,
    IN gcPatchDirective  *  PatchDirective
    );

/*******************************************************************************
**                                gcSaveProgram
********************************************************************************
**
**    Save pre-compiled shaders and pre-linked programs to a binary file.
**
**    INPUT:
**
**        gcSHADER VertexShader
**            Pointer to vertex shader object.
**
**        gcSHADER FragmentShader
**            Pointer to fragment shader object.
**
**        gctUINT32 ProgramBufferSize
**            Number of bytes in 'ProgramBuffer'.
**
**        gctPOINTER ProgramBuffer
**            Pointer to buffer containing the program states.
**
**        gcsHINT_PTR Hints
**            Pointer to HINTS structure for program states.
**
**    OUTPUT:
**
**        gctPOINTER * Binary
**            Pointer to a variable receiving the binary data to be saved.
**
**        gctUINT32 * BinarySize
**            Pointer to a variable receiving the number of bytes inside 'Binary'.
*/
gceSTATUS
gcSaveProgram(
    IN gcSHADER VertexShader,
    IN gcSHADER FragmentShader,
    IN gctUINT32 ProgramBufferSize,
    IN gctPOINTER ProgramBuffer,
    IN gcsHINT_PTR Hints,
    OUT gctPOINTER * Binary,
    OUT gctUINT32 * BinarySize
    );

/*******************************************************************************
**                                gcLoadProgram
********************************************************************************
**
**    Load pre-compiled shaders and pre-linked programs from a binary file.
**
**    INPUT:
**
**        gctPOINTER Binary
**            Pointer to the binary data loaded.
**
**        gctUINT32 BinarySize
**            Number of bytes in 'Binary'.
**
**    OUTPUT:
**
**        gcSHADER VertexShader
**            Pointer to a vertex shader object.
**
**        gcSHADER FragmentShader
**            Pointer to a fragment shader object.
**
**        gctUINT32 * ProgramBufferSize
**            Pointer to a variable receicing the number of bytes in the buffer
**            returned in 'ProgramBuffer'.
**
**        gctPOINTER * ProgramBuffer
**            Pointer to a variable receiving a buffer pointer that contains the
**            states required to download the shaders into the hardware.
**
**        gcsHINT_PTR * Hints
**            Pointer to a variable receiving a gcsHINT structure pointer that
**            contains information required when loading the shader states.
*/
gceSTATUS
gcLoadProgram(
    IN gctPOINTER Binary,
    IN gctUINT32 BinarySize,
    OUT gcSHADER VertexShader,
    OUT gcSHADER FragmentShader,
    OUT gctUINT32 * ProgramBufferSize,
    OUT gctPOINTER * ProgramBuffer,
    OUT gcsHINT_PTR * Hints
    );

/*******************************************************************************
**                              gcCompileKernel
********************************************************************************
**
**    Compile a OpenCL kernel shader.
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctUINT SourceSize
**            Size of the source buffer in bytes.
**
**        gctCONST_STRING Source
**            Pointer to the buffer containing the shader source code.
**
**    OUTPUT:
**
**        gcSHADER * Binary
**            Pointer to a variable receiving the pointer to a gcSHADER object
**            containg the compiled shader code.
**
**        gctSTRING * Log
**            Pointer to a variable receiving a string pointer containging the
**            compile log.
*/
gceSTATUS
gcCompileKernel(
    IN gcoHAL Hal,
    IN gctUINT SourceSize,
    IN gctCONST_STRING Source,
    IN gctCONST_STRING Options,
    OUT gcSHADER * Binary,
    OUT gctSTRING * Log
    );

/*******************************************************************************
**                                gcLinkKernel
********************************************************************************
**
**    Link OpenCL kernel and generate a harwdare specific state buffer by compiling
**    the compiler generated code through the resource allocator and code
**    generator.
**
**    INPUT:
**
**        gcSHADER Kernel
**            Pointer to a gcSHADER object holding information about the compiled
**            OpenCL kernel.
**
**        gceSHADER_FLAGS Flags
**            Compiler flags.  Can be any of the following:
**
**                gcvSHADER_DEAD_CODE       - Dead code elimination.
**                gcvSHADER_RESOURCE_USAGE  - Resource usage optimizaion.
**                gcvSHADER_OPTIMIZER       - Full optimization.
**                gcvSHADER_USE_GL_Z        - Use OpenGL ES Z coordinate.
**                gcvSHADER_USE_GL_POSITION - Use OpenGL ES gl_Position.
**                gcvSHADER_USE_GL_FACE     - Use OpenGL ES gl_FaceForward.
**
**    OUTPUT:
**
**        gctUINT32 * StateBufferSize
**            Pointer to a variable receiving the number of bytes in the buffer
**            returned in 'StateBuffer'.
**
**        gctPOINTER * StateBuffer
**            Pointer to a variable receiving a buffer pointer that contains the
**            states required to download the shaders into the hardware.
**
**        gcsHINT_PTR * Hints
**            Pointer to a variable receiving a gcsHINT structure pointer that
**            contains information required when loading the shader states.
*/
gceSTATUS
gcLinkKernel(
    IN gcSHADER Kernel,
    IN gceSHADER_FLAGS Flags,
    OUT gctUINT32 * StateBufferSize,
    OUT gctPOINTER * StateBuffer,
    OUT gcsHINT_PTR * Hints
    );

void
gcTYPE_GetTypeInfo(
    IN gcSHADER_TYPE      Type,
    OUT gctUINT32 *       Components,
    OUT gctUINT32 *       Rows,
    OUT gctCONST_STRING * Name
    );

gceSTATUS
gcSHADER_SetTransformFeedbackVarying(
    IN gcSHADER Shader,
    IN gctUINT32 Count,
    IN gctCONST_STRING *Varyings,
    IN gceFEEDBACK_BUFFER_MODE BufferMode,
    OUT gctUINT32 *xfbVaryingNumPtr,
    OUT gctUINT32 *xfbVaryingMaxLenPtr
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVarying(
    IN gcSHADER Shader,
    IN gctUINT32 Index,
    OUT gctCONST_STRING * Name,
    OUT gctUINT *  Length,
    OUT gcSHADER_TYPE * Type,
    OUT gctBOOL * IsArray,
    OUT gctUINT * Size
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingStride(
    IN gcSHADER Shader,
    OUT gctUINT32 * Stride
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingStrideSeparate(
    IN gcSHADER Shader,
    IN gctUINT VaryingIndex,
    OUT gctUINT32 * Stride
    );

gceSTATUS
gcSHADER_FreeTexFormatConvertLibrary(
    void
    );

gceSTATUS
gcSHADER_InsertList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST *             Root,
    IN gctINT                      Index,
    IN gctINT                      Data0,
    IN gctINT                      Data1
    );

gceSTATUS
gcSHADER_UpdateList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Index,
    IN gctINT                      NewIndex
    );

gceSTATUS
gcSHADER_DeleteList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST *             Root,
    IN gctINT                      Index
    );

gceSTATUS
gcSHADER_FindList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Index,
    IN gcSHADER_LIST *             List
    );

gceSTATUS
gcSHADER_FindListByData(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Data0,
    IN gctINT                      Data1,
    IN gcSHADER_LIST *             List
    );

gceSTATUS
gcSHADER_InsertNOP2BeforeCode(
    IN OUT gcSHADER                Shader,
    OUT gctUINT                    CodeIndex,
    OUT gctUINT                    AddCodeCount
    );
gctINT
gcSL_GetName(
    IN gctUINT32 Length,
    IN gctCONST_STRING Name,
    OUT char * Buffer,
    gctUINT32 BufferSize
    );

gcSL_ENABLE
gcSL_ConvertSwizzle2Enable(
    IN gcSL_SWIZZLE X,
    IN gcSL_SWIZZLE Y,
    IN gcSL_SWIZZLE Z,
    IN gcSL_SWIZZLE W
    );

/* dump instruction to stdout */
void dbg_dumpIR(gcSL_INSTRUCTION Inst,gctINT n);

END_EXTERN_C()

#endif /* __gc_vsc_old_drvi_interface_h_ */

