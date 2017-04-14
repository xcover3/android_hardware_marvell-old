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
** gcSL language definition
*/

#ifndef __gc_vsc_old_gcsl_h_
#define __gc_vsc_old_gcsl_h_

BEGIN_EXTERN_C()

#define TEMP_OPT_CONSTANT_TEXLD_COORD    1

/******************************* IR VERSION ******************/
#define gcdSL_IR_VERSION gcmCC('\0','\0','\0','\1')

/******************************* SHADER BINARY FILE VERSION ******************/
/* the shader file version before Loadtime Constant optimization */
#define gcdSL_SHADER_BINARY_BEFORE_LTC_FILE_VERSION gcmCC(0, 0, 1, 1)

/* bump up version to 1.2 for Loadtime Constant optimization on 1/3/2012 */
/*#define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(0, 0, 1, 2)*/
#define gcdSL_SHADER_BINARY_BEFORE_STRUCT_SYMBOL_FILE_VERSION gcmCC(0, 0, 1, 2)

/* bump up version to 1.3 for struct support for variable and uniform on 3/9/2012 */
/*#define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(0, 0, 1, 3)*/
#define gcdSL_SHADER_BINARY_BEFORE_VARIABLE_TYPE_QUALIFIER_FILE_VERSION gcmCC(0, 0, 1, 3)

/* bump up version to 1.4 for variable type qualifier support on 4/2/2012 */
/* #define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(0, 0, 1, 4)*/
#define gcdSL_SHADER_BINARY_BEFORE_PRECISION_QUALIFIER_FILE_VERSION gcmCC(0, 0, 1, 4)

/* bump up version to 1.5 for precision qualifier support on 8/23/2012 */
/* #define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(0, 0, 1, 5) */
#define gcdSL_SHADER_BINARY_BEFORE_HALTI_FILE_VERSION gcmCC(0, 0, 1, 5)

/* bump up version to 1.6 for halti feature support on 9/10/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_IR_CHANGE_FILE_VERSION gcmCC(0, 0, 1, 6)

/* bump up version to 1.7 for openCL image sampler with TEXLD support on 9/18/2013 */
#define gcdSL_SHADER_BINARY_BEFORE_OPENCL_IMAGE_SAMPLER_BY_TEXLD_FILE_VERSION gcmCC(0, 0, 1, 7)

/* bump up version to 1.8 for add invariant support on 10/20/2014 */
#define gcdSL_SHADER_BINARY_ADD_INVARIANT_VERSION gcmCC(0, 0, 1, 8)

/* bump up version to 1.9 for IR change support on 10/20/2014 */
#define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(0, 0, 1, 9)

#define gcdSL_PROGRAM_BINARY_FILE_VERSION gcmCC('\0','\0','\11','\0')

typedef union _gcsValue
{
    /* vector 4 value */
    gctFLOAT         f32_v4[4];
    gctINT32         i32_v4[4];
    gctUINT32        u32_v4[4];

    /* scalar value */
    gctFLOAT         f32;
    gctINT32         i32;
    gctUINT32        u32;
} gcsValue;

/******************************************************************************\
|******************************* SHADER LANGUAGE ******************************|
\******************************************************************************/

/* Shader types. */
typedef enum _gcSHADER_KIND {
    gcSHADER_TYPE_UNKNOWN = 0,
    gcSHADER_TYPE_VERTEX,
    gcSHADER_TYPE_FRAGMENT,
    gcSHADER_TYPE_CL,
    gcSHADER_TYPE_PRECOMPILED,
    gcSHADER_TYPE_LIBRARY,
    gcSHADER_TYPE_VERTEX_DEFAULT_UBO,
    gcSHADER_TYPE_FRAGMENT_DEFAULT_UBO,
    gcSHADER_TYPE_COMPUTE,
    gcSHADER_KIND_COUNT
} gcSHADER_KIND;

/* Client version. */
typedef enum _gcSL_OPCODE
{
    gcSL_NOP, /* 0x00 */
    gcSL_MOV, /* 0x01 */
    gcSL_SAT, /* 0x02 */
    gcSL_DP3, /* 0x03 */
    gcSL_DP4, /* 0x04 */
    gcSL_ABS, /* 0x05 */
    gcSL_JMP, /* 0x06 */
    gcSL_ADD, /* 0x07 */
    gcSL_MUL, /* 0x08 */
    gcSL_RCP, /* 0x09 */
    gcSL_SUB, /* 0x0A */
    gcSL_KILL, /* 0x0B */
    gcSL_TEXLD, /* 0x0C */
    gcSL_CALL, /* 0x0D */
    gcSL_RET, /* 0x0E */
    gcSL_NORM, /* 0x0F */
    gcSL_MAX, /* 0x10 */
    gcSL_MIN, /* 0x11 */
    gcSL_POW, /* 0x12 */
    gcSL_RSQ, /* 0x13 */
    gcSL_LOG, /* 0x14 */
    gcSL_FRAC, /* 0x15 */
    gcSL_FLOOR, /* 0x16 */
    gcSL_CEIL, /* 0x17 */
    gcSL_CROSS, /* 0x18 */
    gcSL_TEXLDPROJ, /* 0x19 */
    gcSL_TEXBIAS, /* 0x1A */
    gcSL_TEXGRAD, /* 0x1B */
    gcSL_TEXLOD, /* 0x1C */
    gcSL_SIN, /* 0x1D */
    gcSL_COS, /* 0x1E */
    gcSL_TAN, /* 0x1F */
    gcSL_EXP, /* 0x20 */
    gcSL_SIGN, /* 0x21 */
    gcSL_STEP, /* 0x22 */
    gcSL_SQRT, /* 0x23 */
    gcSL_ACOS, /* 0x24 */
    gcSL_ASIN, /* 0x25 */
    gcSL_ATAN, /* 0x26 */
    gcSL_SET, /* 0x27 */
    gcSL_DSX, /* 0x28 */
    gcSL_DSY, /* 0x29 */
    gcSL_FWIDTH, /* 0x2A */
    gcSL_DIV, /* 0x2B */
    gcSL_MOD, /* 0x2C */
    gcSL_AND_BITWISE, /* 0x2D */
    gcSL_OR_BITWISE, /* 0x2E */
    gcSL_XOR_BITWISE, /* 0x2F */
    gcSL_NOT_BITWISE, /* 0x30 */
    gcSL_LSHIFT, /* 0x31 */
    gcSL_RSHIFT, /* 0x32 */
    gcSL_ROTATE, /* 0x33 */
    gcSL_BITSEL, /* 0x34 */
    gcSL_LEADZERO, /* 0x35 */
    gcSL_LOAD, /* 0x36 */
    gcSL_STORE, /* 0x37 */
    gcSL_BARRIER, /* 0x38 */
    gcSL_STORE1, /* 0x39 */
    gcSL_ATOMADD, /* 0x3A */
    gcSL_ATOMSUB, /* 0x3B */
    gcSL_ATOMXCHG, /* 0x3C */
    gcSL_ATOMCMPXCHG, /* 0x3D */
    gcSL_ATOMMIN, /* 0x3E */
    gcSL_ATOMMAX, /* 0x3F */
    gcSL_ATOMOR, /* 0x40 */
    gcSL_ATOMAND, /* 0x41 */
    gcSL_ATOMXOR, /* 0x42 */
    gcSL_TEXLDPCF, /* 0x43 */
    gcSL_TEXLDPCFPROJ, /* 0x44 */
    /*gcSL_UNUSED, 0x45 */
    /*gcSL_UNUSED, 0x46 */
    /*gcSL_UNUSED, 0x47 */
    /*gcSL_UNUSED, 0x48 */
    /*gcSL_UNUSED, 0x49 */
    /*gcSL_UNUSED, 0x4A */
    /*gcSL_UNUSED, 0x4B */
    /*gcSL_UNUSED, 0x4C */
    /*gcSL_UNUSED, 0x4D */
    /*gcSL_UNUSED, 0x4E */
    /*gcSL_UNUSED, 0x4F */
    gcSL_SINPI = 0x50, /* 0x50 */
    gcSL_COSPI, /* 0x51 */
    gcSL_TANPI, /* 0x52 */
    gcSL_ADDLO, /* 0x53 */  /* Float only. */
    gcSL_MULLO, /* 0x54 */  /* Float only. */
    gcSL_CONV, /* 0x55 */
    gcSL_GETEXP, /* 0x56 */
    gcSL_GETMANT, /* 0x57 */
    gcSL_MULHI, /* 0x58 */  /* Integer only. */
    gcSL_CMP, /* 0x59 */
    gcSL_I2F, /* 0x5A */
    gcSL_F2I, /* 0x5B */
    gcSL_ADDSAT, /* 0x5C */  /* Integer only. */
    gcSL_SUBSAT, /* 0x5D */  /* Integer only. */
    gcSL_MULSAT, /* 0x5E */  /* Integer only. */
    gcSL_DP2, /* 0x5F */
    gcSL_UNPACK, /* 0x60 */
    gcSL_IMAGE_WR, /* 0x61 */
    gcSL_SAMPLER_ADD, /* 0x62 */
    gcSL_MOVA, /* 0x63, HW MOVAR/MOVF/MOVI, VIRCG only */
    gcSL_IMAGE_RD, /* 0x64 */
    gcSL_IMAGE_SAMPLER, /* 0x65 */
    gcSL_NORM_MUL, /* 0x66  VIRCG only */
    gcSL_NORM_DP2, /* 0x67  VIRCG only */
    gcSL_NORM_DP3, /* 0x68  VIRCG only */
    gcSL_NORM_DP4, /* 0x69  VIRCG only */
    gcSL_PRE_DIV, /* 0x6A  VIRCG only */
    gcSL_PRE_LOG2, /* 0x6B  VIRCG only */
    gcSL_MAXOPCODE
}
gcSL_OPCODE;

typedef enum _gcSL_FORMAT
{
    gcSL_FLOAT = 0, /* 0 */
    gcSL_INTEGER = 1, /* 1 */
    gcSL_INT32 = 1, /* 1 */
    gcSL_BOOLEAN = 2, /* 2 */
    gcSL_UINT32 = 3, /* 3 */
    gcSL_INT8, /* 4 */
    gcSL_UINT8, /* 5 */
    gcSL_INT16, /* 6 */
    gcSL_UINT16, /* 7 */
    gcSL_INT64, /* 8 */     /* Reserved for future enhancement. */
    gcSL_UINT64, /* 9 */     /* Reserved for future enhancement. */
    gcSL_INT128, /* 10 */    /* Reserved for future enhancement. */
    gcSL_UINT128, /* 11 */    /* Reserved for future enhancement. */
    gcSL_FLOAT16, /* 12 */
    gcSL_FLOAT64, /* 13 */    /* Reserved for future enhancement. */
    gcSL_FLOAT128, /* 14 */    /* Reserved for future enhancement. */
    gcSL_INVALID
}
gcSL_FORMAT;

/* Destination write enable bits. */
typedef enum _gcSL_ENABLE
{
    gcSL_ENABLE_NONE                     = 0x0, /* none is enabled, error/uninitialized state */
    gcSL_ENABLE_X                        = 0x1,
    gcSL_ENABLE_Y                        = 0x2,
    gcSL_ENABLE_Z                        = 0x4,
    gcSL_ENABLE_W                        = 0x8,
    /* Combinations. */
    gcSL_ENABLE_XY                       = gcSL_ENABLE_X | gcSL_ENABLE_Y,
    gcSL_ENABLE_XYZ                      = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_Z,
    gcSL_ENABLE_XYZW                     = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_XYW                      = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_W,
    gcSL_ENABLE_XZ                       = gcSL_ENABLE_X | gcSL_ENABLE_Z,
    gcSL_ENABLE_XZW                      = gcSL_ENABLE_X | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_XW                       = gcSL_ENABLE_X | gcSL_ENABLE_W,
    gcSL_ENABLE_YZ                       = gcSL_ENABLE_Y | gcSL_ENABLE_Z,
    gcSL_ENABLE_YZW                      = gcSL_ENABLE_Y | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_YW                       = gcSL_ENABLE_Y | gcSL_ENABLE_W,
    gcSL_ENABLE_ZW                       = gcSL_ENABLE_Z | gcSL_ENABLE_W,
}
gcSL_ENABLE;

/* Possible indices. */
typedef enum _gcSL_INDEXED
{
    gcSL_NOT_INDEXED, /* 0 */
    gcSL_INDEXED_X, /* 1 */
    gcSL_INDEXED_Y, /* 2 */
    gcSL_INDEXED_Z, /* 3 */
    gcSL_INDEXED_W, /* 4 */
}
gcSL_INDEXED;

/* Opcode conditions. */
typedef enum _gcSL_CONDITION
{
    gcSL_ALWAYS, /* 0x0 */
    gcSL_NOT_EQUAL, /* 0x1 */
    gcSL_LESS_OR_EQUAL, /* 0x2 */
    gcSL_LESS, /* 0x3 */
    gcSL_EQUAL, /* 0x4 */
    gcSL_GREATER, /* 0x5 */
    gcSL_GREATER_OR_EQUAL, /* 0x6 */
    gcSL_AND, /* 0x7 */
    gcSL_OR, /* 0x8 */
    gcSL_XOR, /* 0x9 */
    gcSL_NOT_ZERO, /* 0xA */
    gcSL_ZERO, /* 0xB */
}
gcSL_CONDITION;

/* Possible source operand types. */
typedef enum _gcSL_TYPE
{
    gcSL_NONE, /* 0x0 */
    gcSL_TEMP, /* 0x1 */
    gcSL_ATTRIBUTE, /* 0x2 */
    gcSL_UNIFORM, /* 0x3 */
    gcSL_SAMPLER, /* 0x4 */
    gcSL_CONSTANT, /* 0x5 */
    gcSL_OUTPUT, /* 0x6 */
}
gcSL_TYPE;

/* Swizzle generator macro. */
#define gcmSWIZZLE(Component1, Component2, Component3, Component4) \
(\
    (gcSL_SWIZZLE_ ## Component1 << 0) | \
    (gcSL_SWIZZLE_ ## Component2 << 2) | \
    (gcSL_SWIZZLE_ ## Component3 << 4) | \
    (gcSL_SWIZZLE_ ## Component4 << 6)   \
)

#define gcmExtractSwizzle(Swizzle, Index) \
    ((gcSL_SWIZZLE) ((((Swizzle) >> (Index * 2)) & 0x3)))

#define gcmComposeSwizzle(SwizzleX, SwizzleY, SwizzleZ, SwizzleW) \
(\
    ((SwizzleX) << 0) | \
    ((SwizzleY) << 2) | \
    ((SwizzleZ) << 4) | \
    ((SwizzleW) << 6)   \
)

/* Possible swizzle values. */
typedef enum _gcSL_SWIZZLE
{
    gcSL_SWIZZLE_X, /* 0x0 */
    gcSL_SWIZZLE_Y, /* 0x1 */
    gcSL_SWIZZLE_Z, /* 0x2 */
    gcSL_SWIZZLE_W, /* 0x3 */
    /* Combinations. */
    gcSL_SWIZZLE_XXXX = gcmSWIZZLE(X, X, X, X),
    gcSL_SWIZZLE_YYYY = gcmSWIZZLE(Y, Y, Y, Y),
    gcSL_SWIZZLE_ZZZZ = gcmSWIZZLE(Z, Z, Z, Z),
    gcSL_SWIZZLE_WWWW = gcmSWIZZLE(W, W, W, W),
    gcSL_SWIZZLE_XYYY = gcmSWIZZLE(X, Y, Y, Y),
    gcSL_SWIZZLE_XZZZ = gcmSWIZZLE(X, Z, Z, Z),
    gcSL_SWIZZLE_XWWW = gcmSWIZZLE(X, W, W, W),
    gcSL_SWIZZLE_YZZZ = gcmSWIZZLE(Y, Z, Z, Z),
    gcSL_SWIZZLE_YWWW = gcmSWIZZLE(Y, W, W, W),
    gcSL_SWIZZLE_ZWWW = gcmSWIZZLE(Z, W, W, W),
    gcSL_SWIZZLE_XYZZ = gcmSWIZZLE(X, Y, Z, Z),
    gcSL_SWIZZLE_XYWW = gcmSWIZZLE(X, Y, W, W),
    gcSL_SWIZZLE_XZWW = gcmSWIZZLE(X, Z, W, W),
    gcSL_SWIZZLE_YZWW = gcmSWIZZLE(Y, Z, W, W),
    gcSL_SWIZZLE_XXYZ = gcmSWIZZLE(X, X, Y, Z),
    gcSL_SWIZZLE_XYZW = gcmSWIZZLE(X, Y, Z, W),
    gcSL_SWIZZLE_XYXY = gcmSWIZZLE(X, Y, X, Y),
    gcSL_SWIZZLE_YYZZ = gcmSWIZZLE(Y, Y, Z, Z),
    gcSL_SWIZZLE_YYWW = gcmSWIZZLE(Y, Y, W, W),
    gcSL_SWIZZLE_ZZZW = gcmSWIZZLE(Z, Z, Z, W),
    gcSL_SWIZZLE_XZZW = gcmSWIZZLE(X, Z, Z, W),
    gcSL_SWIZZLE_YYZW = gcmSWIZZLE(Y, Y, Z, W),
    gcSL_SWIZZLE_XXXY = gcmSWIZZLE(X, X, X, Y),
    gcSL_SWIZZLE_XZXZ = gcmSWIZZLE(X, Z, X, Z),
    gcSL_SWIZZLE_YWWY = gcmSWIZZLE(Y, W, W, Y),
    gcSL_SWIZZLE_ZWZW = gcmSWIZZLE(Z, W, Z, W),

    gcSL_SWIZZLE_INVALID = 0x7FFFFFFF
}
gcSL_SWIZZLE;

typedef enum _gcSL_COMPONENT
{
    gcSL_COMPONENT_X, /* 0x0 */
    gcSL_COMPONENT_Y, /* 0x1 */
    gcSL_COMPONENT_Z, /* 0x2 */
    gcSL_COMPONENT_W, /* 0x3 */
    gcSL_COMPONENT_COUNT            /* 0x4 */
} gcSL_COMPONENT;

#define gcmIsComponentEnabled(Enable, Component) (((Enable) & (1 << (Component))) != 0)

/* Rounding modes. */
typedef enum _gcSL_ROUND
{
    gcSL_ROUND_DEFAULT, /* 0x0 */
    gcSL_ROUND_RTZ, /* 0x1 */
    gcSL_ROUND_RTNE, /* 0x2 */
    gcSL_ROUND_RTP, /* 0x3 */
    gcSL_ROUND_RTN                  /* 0x4 */
} gcSL_ROUND;

/* Saturation */
typedef enum _gcSL_MODIFIER_SAT
{
    gcSL_NO_SATURATE, /* 0x0 */
    gcSL_SATURATE            /* 0x1 */
} gcSL_MODIFIER_SAT;

/* Precision setting. */
typedef enum _gcSL_PRECISION
{
    gcSL_PRECISION_MEDIUM, /* 0x0 */
    gcSL_PRECISION_HIGH, /* 0x1 */
} gcSL_PRECISION;

/* gcSHADER_TYPE enumeration. */
typedef enum _gcSHADER_TYPE
{
    gcSHADER_FLOAT_X1   = 0, /* 0x00 */
    gcSHADER_FLOAT_X2, /* 0x01 */
    gcSHADER_FLOAT_X3, /* 0x02 */
    gcSHADER_FLOAT_X4, /* 0x03 */
    gcSHADER_FLOAT_2X2, /* 0x04 */
    gcSHADER_FLOAT_3X3, /* 0x05 */
    gcSHADER_FLOAT_4X4, /* 0x06 */
    gcSHADER_BOOLEAN_X1, /* 0x07 */
    gcSHADER_BOOLEAN_X2, /* 0x08 */
    gcSHADER_BOOLEAN_X3, /* 0x09 */
    gcSHADER_BOOLEAN_X4, /* 0x0A */
    gcSHADER_INTEGER_X1, /* 0x0B */
    gcSHADER_INTEGER_X2, /* 0x0C */
    gcSHADER_INTEGER_X3, /* 0x0D */
    gcSHADER_INTEGER_X4, /* 0x0E */
    gcSHADER_SAMPLER_1D, /* 0x0F */
    gcSHADER_SAMPLER_2D, /* 0x10 */
    gcSHADER_SAMPLER_3D, /* 0x11 */
    gcSHADER_SAMPLER_CUBIC, /* 0x12 */
    gcSHADER_FIXED_X1, /* 0x13 */
    gcSHADER_FIXED_X2, /* 0x14 */
    gcSHADER_FIXED_X3, /* 0x15 */
    gcSHADER_FIXED_X4, /* 0x16 */
    gcSHADER_IMAGE_2D, /* 0x17 */  /* For OCL. */
    gcSHADER_IMAGE_3D, /* 0x18 */  /* For OCL. */
    gcSHADER_SAMPLER, /* 0x19 */  /* For OCL. */
    gcSHADER_FLOAT_2X3, /* 0x1A */
    gcSHADER_FLOAT_2X4, /* 0x1B */
    gcSHADER_FLOAT_3X2, /* 0x1C */
    gcSHADER_FLOAT_3X4, /* 0x1D */
    gcSHADER_FLOAT_4X2, /* 0x1E */
    gcSHADER_FLOAT_4X3, /* 0x1F */
    gcSHADER_ISAMPLER_2D, /* 0x20 */
    gcSHADER_ISAMPLER_3D, /* 0x21 */
    gcSHADER_ISAMPLER_CUBIC, /* 0x22 */
    gcSHADER_USAMPLER_2D, /* 0x23 */
    gcSHADER_USAMPLER_3D, /* 0x24 */
    gcSHADER_USAMPLER_CUBIC, /* 0x25 */
    gcSHADER_SAMPLER_EXTERNAL_OES, /* 0x26 */

    gcSHADER_UINT_X1, /* 0x27 */
    gcSHADER_UINT_X2, /* 0x28 */
    gcSHADER_UINT_X3, /* 0x29 */
    gcSHADER_UINT_X4, /* 0x2A */

    gcSHADER_SAMPLER_2D_SHADOW, /* 0x2B */
    gcSHADER_SAMPLER_CUBE_SHADOW, /* 0x2C */

    gcSHADER_SAMPLER_1D_ARRAY, /* 0x2D */
    gcSHADER_SAMPLER_1D_ARRAY_SHADOW, /* 0x2E */
    gcSHADER_SAMPLER_2D_ARRAY, /* 0x2F */
    gcSHADER_ISAMPLER_2D_ARRAY, /* 0x30 */
    gcSHADER_USAMPLER_2D_ARRAY, /* 0x31 */
    gcSHADER_SAMPLER_2D_ARRAY_SHADOW, /* 0x32 */

    gcSHADER_UNKONWN_TYPE, /* do not add type after this */
    gcSHADER_TYPE_COUNT         /* must to change gcvShaderTypeInfo at the
                                 * same time if you add any new type! */
}
gcSHADER_TYPE;

#define isSamplerType(Type)                               (gcmType_Kind(Type) == gceTK_SAMPLER)
#define IsMatType(Type)                                   (gcmType_Rows(Type) > 1)

typedef enum _gcSHADER_VAR_CATEGORY
{
    gcSHADER_VAR_CATEGORY_NORMAL  =  0, /* primitive type and its array */
    gcSHADER_VAR_CATEGORY_STRUCT, /* structure */
    gcSHADER_VAR_CATEGORY_BLOCK, /* uniform block */
    gcSHADER_VAR_CATEGORY_BLOCK_MEMBER, /* block member */
    gcSHADER_VAR_CATEGORY_BLOCK_ADDRESS, /* uniform block address */
    gcSHADER_VAR_CATEGORY_LOD_MIN_MAX, /* lod min max */
    gcSHADER_VAR_CATEGORY_LEVEL_BASE_SIZE, /* base level */
    gcSHADER_VAR_CATEGORY_FUNCTION_INPUT_ARGUMENT, /* function input argument */
    gcSHADER_VAR_CATEGORY_FUNCTION_OUTPUT_ARGUMENT, /* function output argument */
    gcSHADER_VAR_CATEGORY_FUNCTION_INOUT_ARGUMENT, /* function inout argument */
}
gcSHADER_VAR_CATEGORY;

typedef enum _gceTYPE_QUALIFIER
{
    gcvTYPE_QUALIFIER_NONE         = 0x0, /* unqualified */
    gcvTYPE_QUALIFIER_VOLATILE     = 0x1, /* volatile */
}gceTYPE_QUALIFIER;

typedef gctUINT16  gctTYPE_QUALIFIER;

/* gcSHADER_PRECISION enumeration. */
typedef enum _gcSHADER_PRECISION
{
    gcSHADER_PRECISION_DEFAULT, /* 0x00 */
    gcSHADER_PRECISION_HIGH, /* 0x01 */
    gcSHADER_PRECISION_MEDIUM, /* 0x02 */
    gcSHADER_PRECISION_LOW, /* 0x03 */
}
gcSHADER_PRECISION;

/* gcSHADER_SHADERMODE enumeration. */
typedef enum _gcSHADER_SHADERMODE
{
    gcSHADER_SHADER_DEFAULT, /* 0x00 */
    gcSHADER_SHADER_SMOOTH, /* 0x01 */
    gcSHADER_SHADER_FLAT, /* 0x02 */
}
gcSHADER_SHADERMODE;

/* Kernel function property flags. */
typedef enum _gcePROPERTY_FLAGS
{
    gcvPROPERTY_REQD_WORK_GRP_SIZE    = 0x01
}
gceKERNEL_FUNCTION_PROPERTY_FLAGS;

/* HALTI interface block layout qualifiers */
typedef enum _gceINTERFACE_BLOCK_LAYOUT_ID
{
    gcvINTERFACE_BLOCK_NONE    = 0,
    gcvINTERFACE_BLOCK_SHARED    = 0x01,
    gcvINTERFACE_BLOCK_STD140    = 0x02,
    gcvINTERFACE_BLOCK_PACKED    = 0x04,
}
gceINTERFACE_BLOCK_LAYOUT_ID;

/* Uniform flags. */
typedef enum _gceUNIFORM_FLAGS
{
    gcvUNIFORM_KERNEL_ARG                  = 0x01,
    gcvUNIFORM_KERNEL_ARG_LOCAL            = 0x02,
    gcvUNIFORM_KERNEL_ARG_SAMPLER          = 0x04,
    gcvUNIFORM_LOCAL_ADDRESS_SPACE         = 0x08,
    gcvUNIFORM_PRIVATE_ADDRESS_SPACE       = 0x10,
    gcvUNIFORM_CONSTANT_ADDRESS_SPACE      = 0x20,
    gcvUNIFORM_GLOBAL_SIZE                 = 0x40,
    gcvUNIFORM_LOCAL_SIZE                  = 0x80,
    gcvUNIFORM_NUM_GROUPS                  = 0x100,
    gcvUNIFORM_GLOBAL_OFFSET               = 0x200,
    gcvUNIFORM_WORK_DIM                    = 0x400,
    gcvUNIFORM_KERNEL_ARG_CONSTANT         = 0x800,
    gcvUNIFORM_KERNEL_ARG_LOCAL_MEM_SIZE   = 0x1000,
    gcvUNIFORM_KERNEL_ARG_PRIVATE          = 0x2000,
    gcvUNIFORM_LOADTIME_CONSTANT           = 0x4000,
    gcvUNIFORM_IS_ARRAY                    = 0x8000,
    gcvUNIFORM_TRANSFORM_FEEDBACK_BUFFER   = 0x10000,
    gcvUNIFORM_TRANSFORM_FEEDBACK_STATE    = 0x20000,
    gcvUNIFORM_COMPILETIME_INITIALIZED     = 0x40000,
    gcvUNIFORM_OPT_CONSTANT_TEXLD_COORD    = 0x80000,
    gcvUNIFORM_IS_INACTIVE                 = 0x100000,
    gcvUNIFORM_USED_IN_SHADER              = 0x200000,
    gcvUNIFORM_USED_IN_LTC                 = 0x400000, /* it may be not used in
                                                          shader, but is used in LTC */
    gcvUNIFORM_KERNEL_ARG_PATCH            = 0x800000,
    gcvUNIFORM_INDIRECTLY_ADDRESSED        = 0x1000000,
    gcvUNIFORM_USE_LOAD_INSTRUCTION        = 0x2000000,
    gcvUNIFORM_STD140_SHARED               = 0x4000000,
    gcvUNIFORM_SAMPLER_CALCULATE_TEX_SIZE  = 0x8000000,
    gcvUNIFORM_PRINTF_ADDRESS              = 0x10000000,
    gcvUNIFORM_WORKITEM_PRINTF_BUFFER_SIZE = 0x20000000,
    gcvUNIFORM_KIND_GENERAL_PATCH          = 0x40000000,
    gcvUNIFORM_HAS_LOAD_BEFORE             = 0x80000000
}
gceUNIFORM_FLAGS;

#define UniformHasFlag(u, flag)         (((u)->flags & (flag)) != 0 )
#define SetUniformFlag(u, flag)         ((u)->flags |= (flag))
#define ResetUniformFlag(u, flag)       ((u)->flags &= ~(flag))

/* Whether the uniform was user-defined or built-in */
#define isUniformShaderDefined(u)           (((u)->name[0] != '#') ||                                       \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.near")) || \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.far")) ||  \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.diff"))    \
                                            )


#define isUniformArray(u)               (((u)->flags & gcvUNIFORM_IS_ARRAY) != 0)
#define isUniformInactive(u)            (((u)->flags & gcvUNIFORM_IS_INACTIVE) != 0)
#define isUniformLoadtimeConstant(u)    (((u)->flags & gcvUNIFORM_LOADTIME_CONSTANT) != 0)
#define isUniformOptConstantTexldCoord(u)    (((u)->flags & gcvUNIFORM_OPT_CONSTANT_TEXLD_COORD) != 0)

#define isUniformUsedInShader(u)        (((u)->flags & gcvUNIFORM_USED_IN_SHADER) != 0)
#define isUniformUsedInLTC(u)           (((u)->flags & gcvUNIFORM_USED_IN_LTC) != 0)
#define isUniformSTD140OrShared(u)      (((u)->flags & gcvUNIFORM_STD140_SHARED) != 0)
#define isUniformUsedInTextureSize(u)   (((u)->flags & gcvUNIFORM_SAMPLER_CALCULATE_TEX_SIZE) != 0)

#define isUniformTransfromFeedbackState(u)  (((u)->flags & gcvUNIFORM_TRANSFORM_FEEDBACK_STATE) != 0)
#define isUniformTransfromFeedbackBuffer(u) (((u)->flags & gcvUNIFORM_TRANSFORM_FEEDBACK_BUFFER) != 0)
#define isUniformWorkDim(u)                 (((u)->flags & gcvUNIFORM_WORK_DIM) != 0)
#define isUniformGlobalSize(u)              (((u)->flags & gcvUNIFORM_GLOBAL_SIZE) != 0)
#define isUniformLocalSize(u)               (((u)->flags & gcvUNIFORM_LOCAL_SIZE) != 0)
#define isUniformNumGroups(u)               (((u)->flags & gcvUNIFORM_NUM_GROUPS) != 0)
#define isUniformGlobalOffset(u)            (((u)->flags & gcvUNIFORM_GLOBAL_OFFSET) != 0)
#define isUniformLocalAddressSpace(u)       (((u)->flags & gcvUNIFORM_LOCAL_ADDRESS_SPACE) != 0)
#define isUniformPrivateAddressSpace(u)     (((u)->flags & gcvUNIFORM_PRIVATE_ADDRESS_SPACE) != 0)
#define isUniformConstantAddressSpace(u)    (((u)->flags & gcvUNIFORM_CONSTANT_ADDRESS_SPACE) != 0)
#define isUniformKernelArg(u)               (((u)->flags & gcvUNIFORM_KERNEL_ARG) != 0)
#define isUniformKernelArgConstant(u)       (((u)->flags & gcvUNIFORM_KERNEL_ARG_CONSTANT) != 0)
#define isUniformKernelArgSampler(u)        (((u)->flags & gcvUNIFORM_KERNEL_ARG_SAMPLER) != 0)
#define isUniformKernelArgLocal(u)          (((u)->flags & gcvUNIFORM_KERNEL_ARG_LOCAL) != 0)
#define isUniformKernelArgLocalMemSize(u)   (((u)->flags & gcvUNIFORM_KERNEL_ARG_LOCAL_MEM_SIZE) != 0)
#define isUniformKernelArgPrivate(u)        (((u)->flags & gcvUNIFORM_KERNEL_ARG_PRIVATE) != 0)
#define isUniformKernelArgPatch(u)          (((u)->flags & gcvUNIFORM_KERNEL_ARG_PATCH) != 0)
#define isUniformCompiletimeInitialized(u)  (((u)->flags & gcvUNIFORM_COMPILETIME_INITIALIZED) != 0)
#define isUniformIndirectlyAddressed(u)     (((u)->flags & gcvUNIFORM_INDIRECTLY_ADDRESSED) != 0)
#define isUniformUseLoadInstruction(u)      (((u)->flags & gcvUNIFORM_USE_LOAD_INSTRUCTION) != 0)
#define isUniformSamplerCalculateTexSize(u) (((u)->flags & gcvUNIFORM_SAMPLER_CALCULATE_TEX_SIZE) != 0)
#define isUniformNormal(u)                  ((u)->varCategory == gcSHADER_VAR_CATEGORY_NORMAL)
#define isUniformStruct(u)                  ((u)->varCategory == gcSHADER_VAR_CATEGORY_STRUCT)
#define isUniformBlockMember(u)             ((u)->varCategory == gcSHADER_VAR_CATEGORY_BLOCK_MEMBER)
#define isUniformBlockAddress(u)            ((u)->varCategory == gcSHADER_VAR_CATEGORY_BLOCK_ADDRESS)
#define isUniformLodMinMax(u)               ((u)->varCategory == gcSHADER_VAR_CATEGORY_LOD_MIN_MAX)
#define isUniformLevelBaseSize(u)           ((u)->varCategory == gcSHADER_VAR_CATEGORY_LEVEL_BASE_SIZE)

#define isUniformSampler(u)                 ((isUniformNormal(u)) &&\
                                             (gcmType_Kind(GetUniformType(u)) == gceTK_SAMPLER))
#define GetUniformType(un)                  ((un)->u.type)
#define GetUniformPhysical(u)               ((u)->physical)
#define GetUniformUBOIndex(u)               ((u)->blockIndex)
#define GetUniformLoadTempIndex(u)          ((u)->loadTempIndex)
#define SetUniformLoadTempIndex(u, v)       (GetUniformLoadTempIndex(u) = v)
#define GetUniformArraySize(u)              ((u)->arraySize)

#define SetUniformUsedInShader(u)     ((u)->flags |= gcvUNIFORM_USED_IN_SHADER)
#define ResetUniformUsedInShader(u)   ((u)->flags &= ~gcvUNIFORM_USED_IN_SHADER)
#define SetUniformUsedInLTC(u)        ((u)->flags |= gcvUNIFORM_USED_IN_LTC)
#define ResetUniformUsedInLTC(u)      ((u)->flags &= ~gcvUNIFORM_USED_IN_LTC)

#define gcdUNIFORM_KERNEL_ARG_MASK  (gcvUNIFORM_KERNEL_ARG         | \
                                     gcvUNIFORM_KERNEL_ARG_LOCAL   | \
                                     gcvUNIFORM_KERNEL_ARG_SAMPLER | \
                                     gcvUNIFORM_KERNEL_ARG_PRIVATE | \
                                     gcvUNIFORM_KERNEL_ARG_CONSTANT)

typedef enum _gceFEEDBACK_BUFFER_MODE
{
  gcvFEEDBACK_INTERLEAVED        = 0x00,
  gcvFEEDBACK_SEPARATE           = 0x01
} gceFEEDBACK_BUFFER_MODE;

/* Special register indices. */
typedef enum _gceBuiltinNameKind
{
    gcSL_NONBUILTINGNAME = 0,
    gcSL_POSITION        = -1,
    gcSL_POINT_SIZE      = -2,
    gcSL_COLOR           = -3,
    gcSL_FRONT_FACING    = -4,
    gcSL_POINT_COORD     = -5,
    gcSL_POSITION_W      = -6,
    gcSL_DEPTH           = -7,
    gcSL_FOG_COORD       = -8,
    gcSL_VERTEX_ID       = -9,
    gcSL_INSTANCE_ID     = -10,
    gcSL_FRONT_COLOR     = -11,
    gcSL_BACK_COLOR      = -12,
    gcSL_FRONT_SECONDARY_COLOR = -13,
    gcSL_BACK_SECONDARY_COLOR  = -14,
    gcSL_TEX_COORD       = -15,
    gcSL_SUBSAMPLE_DEPTH        = -16, /* internal subsample_depth register */
    gcSL_BUILTINNAME_COUNT      = -17
} gceBuiltinNameKind;

/* Special code generation indices. */
#define gcSL_CG_TEMP1                           112
#define gcSL_CG_TEMP1_X                         113
#define gcSL_CG_TEMP1_XY                        114
#define gcSL_CG_TEMP1_XYZ                       115
#define gcSL_CG_TEMP1_XYZW                      116
#define gcSL_CG_TEMP2                           117
#define gcSL_CG_TEMP2_X                         118
#define gcSL_CG_TEMP2_XY                        119
#define gcSL_CG_TEMP2_XYZ                       120
#define gcSL_CG_TEMP2_XYZW                      121
#define gcSL_CG_TEMP3                           122
#define gcSL_CG_TEMP3_X                         123
#define gcSL_CG_TEMP3_XY                        124
#define gcSL_CG_TEMP3_XYZ                       125
#define gcSL_CG_TEMP3_XYZW                      126
#define gcSL_CG_CONSTANT                        127
#define gcSL_CG_TEMP1_X_NO_SRC_SHIFT            109
#define gcSL_CG_TEMP2_X_NO_SRC_SHIFT            110
#define gcSL_CG_TEMP3_X_NO_SRC_SHIFT            111

/* now opcode is encoded with sat,src[0|1]'s neg and abs modifier */
#define gcdSL_OPCODE_Opcode                  0 : 8
#define gcdSL_OPCODE_Round                   8 : 3  /* rounding mode */
#define gcdSL_OPCODE_Sat                    11 : 1  /* target sat modifier */

#define gcmSL_OPCODE_GET(Value, Field) \
    gcmGETBITS(Value, gctUINT16, gcdSL_OPCODE_##Field)

#define gcmSL_OPCODE_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctUINT16, gcdSL_OPCODE_##Field, NewValue)

#define gcmSL_OPCODE_UPDATE(Value, Field, NewValue) \
        (Value) = gcmSL_OPCODE_SET(Value, Field, NewValue)

/* 4-bit enable bits. */
#define gcdSL_TARGET_Enable                     0 : 4
/* Indexed addressing mode of type gcSL_INDEXED. */
#define gcdSL_TARGET_Indexed                    4 : 3
/* precision on temp: 1 => high and 0 => medium */
#define gcdSL_TARGET_Precision                  7 : 1
/* 4-bit condition of type gcSL_CONDITION. */
#define gcdSL_TARGET_Condition                  8 : 4
/* Target format of type gcSL_FORMAT. */
#define gcdSL_TARGET_Format                    12 : 4

#define gcmSL_TARGET_GET(Value, Field) \
    gcmGETBITS(Value, gctUINT16, gcdSL_TARGET_##Field)

#define gcmSL_TARGET_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctUINT16, gcdSL_TARGET_##Field, NewValue)

#define SOURCE_is_32BIT   1

/* Register type of type gcSL_TYPE. */
#define gcdSL_SOURCE_Type                       0 : 3
/* Indexed register swizzle. */
#define gcdSL_SOURCE_Indexed                    3 : 3
/* Source format of type gcSL_FORMAT. */
#if SOURCE_is_32BIT
typedef gctUINT32 gctSOURCE_t;

#define gcdSL_SOURCE_Format                     6 : 4
/* Swizzle fields of type gcSL_SWIZZLE. */
#define gcdSL_SOURCE_Swizzle                   10 : 8
#define gcdSL_SOURCE_SwizzleX                  10 : 2
#define gcdSL_SOURCE_SwizzleY                  12 : 2
#define gcdSL_SOURCE_SwizzleZ                  14 : 2
#define gcdSL_SOURCE_SwizzleW                  16 : 2

/* source precision : 1 => high and 0 => medium */
#define gcdSL_SOURCE_Precision                 18 : 1

/* source modifier */
#define gcdSL_SOURCE_Neg                       19 : 1
#define gcdSL_SOURCE_Abs                       20 : 1

#else  /* source is 16 bit */
typedef gctUINT16 gctSOURCE_t;

#define gcdSL_SOURCE_Format                     6 : 2
/* Swizzle fields of type gcSL_SWIZZLE. */
#define gcdSL_SOURCE_Swizzle                    8 : 8
#define gcdSL_SOURCE_SwizzleX                   8 : 2
#define gcdSL_SOURCE_SwizzleY                  10 : 2
#define gcdSL_SOURCE_SwizzleZ                  12 : 2
#define gcdSL_SOURCE_SwizzleW                  14 : 2
#endif

#define gcmSL_SOURCE_GET(Value, Field) \
    gcmGETBITS(Value, gctSOURCE_t, gcdSL_SOURCE_##Field)

#define gcmSL_SOURCE_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctSOURCE_t, gcdSL_SOURCE_##Field, NewValue)

/* Index of register. */
#define gcdSL_INDEX_Index                      0 : 14
/* Constant value. */
#define gcdSL_INDEX_ConstValue                14 :  2

#define gcmSL_INDEX_GET(Value, Field) \
    gcmGETBITS(Value, gctUINT16, gcdSL_INDEX_##Field)

#define gcmSL_INDEX_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctUINT16, gcdSL_INDEX_##Field, NewValue)

#define gcmSL_JMP_TARGET(Value) (Value)->tempIndex
#define gcmSL_CALL_TARGET(Value) (Value)->tempIndex

#define gcmDummySamplerIdBit          0x2000   /* 14th bit is 1 (MSB for the field gcdSL_INDEX_Index) */

#define gcmMakeDummySamplerId(SID)    ((SID) | gcmDummySamplerIdBit)
#define gcmIsDummySamplerId(SID)      ((SID) >= gcmDummySamplerIdBit)
#define gcmGetOriginalSamplerId(SID)  ((SID) & ~gcmDummySamplerIdBit)

#define _MAX_VARYINGS                     16
#define _MAX_EXT_VARYINGS                 3
#define gcdSL_MIN_TEXTURE_LOD_BIAS       -16
#define gcdSL_MAX_TEXTURE_LOD_BIAS         6

/* Structure that defines a gcSL instruction. */
typedef struct _gcSL_INSTRUCTION
{
    /* Opcode of type gcSL_OPCODE. */
    gctUINT16                    opcode;

    /* Opcode condition and target write enable bits of type gcSL_TARGET. */
    gctUINT16                    temp;

    /* 16-bit temporary register index, or call/branch target inst. index. */
    gctUINT16                    tempIndex;

    /* Indexed register for destination. */
    gctUINT16                    tempIndexed;

    /* Type of source 0 operand of type gcSL_SOURCE. */
    gctSOURCE_t                  source0;

    /* 14-bit register index for source 0 operand of type gcSL_INDEX,
     * must accessed by gcmSL_INDEX_GET(source0Index, Index);
     * and 2-bit constant value to the base of the Index, must be
     * accessed by gcmSL_INDEX_GET(source0Index, ConstValue).
     */
    gctUINT16                    source0Index;

    /* Indexed register for source 0 operand. */
    gctUINT16                    source0Indexed;

    /* Type of source 1 operand of type gcSL_SOURCE. */
    gctSOURCE_t                  source1;

    /* 14-bit register index for source 1 operand of type gcSL_INDEX,
     * must accessed by gcmSL_INDEX_GET(source1Index, Index);
     * and 2-bit constant value to the base of the Index, must be
     * accessed by gcmSL_INDEX_GET(source1Index, ConstValue).
     */
    gctUINT16                    source1Index;

    /* Indexed register for source 1 operand. */
    gctUINT16                    source1Indexed;
}
* gcSL_INSTRUCTION;

/******************************************************************************\
|*********************************** SHADERS **********************************|
\******************************************************************************/
enum gceATTRIBUTE_Flag
{
    gcATTRIBUTE_ISTEXTURE  = 0x01,
    /* Flag to indicate the attribute is a varying packed with othe attribute
       and is no longer in use in the shader, but it cannot be removed from
       attribute array due to the shader maybe loaded from saved file and keep
       the index fro the attributes is needed */
    gcATTRIBUTE_PACKEDAWAY = 0x02,
    /* mark the attribute to always used to avoid be optimized away, it is
     * useful when doing re-compile, recompiled code cannot change attribute
     * mapping done by master shader
     */
    gcATTRIBUTE_ALWAYSUSED = 0x04,

    /* mark the attribute to not used based on new RA
     */
    gcATTRIBUTE_ISNOTUSED = 0x08


};

#define gcmATTRIBUTE_isTexture(att)   ((att)->flags & gcATTRIBUTE_ISTEXTURE)
#define gcmATTRIBUTE_packedAway(att)  ((att)->flags & gcATTRIBUTE_PACKEDAWAY)
#define gcmATTRIBUTE_alwaysUsed(att)  ((att)->flags & gcATTRIBUTE_ALWAYSUSED)
#define gcmATTRIBUTE_isNotUsed(att)   ((att)->flags & gcATTRIBUTE_ISNOTUSED)

#define gcmATTRIBUTE_SetIsTexture(att, v)   \
        ((att)->flags = ((att)->flags & ~gcATTRIBUTE_ISTEXTURE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISTEXTURE))
#define gcmATTRIBUTE_SetPackedAway(att,v )  \
        ((att)->flags = ((att)->flags & ~gcATTRIBUTE_PACKEDAWAY) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_PACKEDAWAY))

#define gcmATTRIBUTE_SetAlwaysUsed(att,v )  \
        ((att)->flags = ((att)->flags & ~gcATTRIBUTE_ALWAYSUSED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ALWAYSUSED))

#define gcmATTRIBUTE_SetNotUsed(att,v )  \
        ((att)->flags = ((att)->flags & ~gcATTRIBUTE_ISNOTUSED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISNOTUSED))

/* Forwarded declaration */
typedef struct _gcSHADER *              gcSHADER;
typedef struct _gcATTRIBUTE *            gcATTRIBUTE;
typedef struct _gcUNIFORM *             gcUNIFORM;
typedef struct _gcsUNIFORM_BLOCK *      gcsUNIFORM_BLOCK;
typedef struct _gcsSHADER_VAR_INFO      gcsSHADER_VAR_INFO;
typedef struct _gcOUTPUT *              gcOUTPUT;
typedef struct _gcsFUNCTION *            gcFUNCTION;
typedef struct _gcsKERNEL_FUNCTION *    gcKERNEL_FUNCTION;
typedef struct _gcVARIABLE *            gcVARIABLE;
typedef struct _gcSHADER_LIST *         gcSHADER_LIST;
typedef struct _gcBINARY_LIST *         gcBINARY_LIST;

struct _gcSHADER_LIST
{
    gcSHADER_LIST               next;
    gctINT                      index;
    gctINT                      data0;
    gctINT                      data1;
};

struct _gcBINARY_LIST
{
    gctINT                      index;
    gctINT                      data0;
    gctINT                      data1;
};

/* Structure the defines an attribute (input) for a shader. */
struct _gcATTRIBUTE
{
    /* The object. */
    gcsOBJECT                    object;

    /* Index of the attribute. */
    gctUINT16                    index;

    /* Type of the attribute. */
    gcSHADER_TYPE                type;

    /* Precision of the uniform. */
    gcSHADER_PRECISION           precision;

    /* Number of array elements for this attribute. */
    gctINT                       arraySize;

    /* Flag to indicate this attribute is used as a texture coordinate
       or packedAway. */
    gctINT8                      flags;

    /* when another texture coord packed with this attribute (2-2 packing) */
    gctBOOL                      isZWTexture;

#if gcdUSE_WCLIP_PATCH
    gctBOOL                      isPosition;
#endif

    /* Flag to indicate this attribute is enabeld or not. */
    gctBOOL                      enabled;

    /* Assigned input register index. */
    gctINT                       inputIndex;

    /* Flat input or smooth input. */
    gcSHADER_SHADERMODE             shaderMode;

    /* If other attributes packed to this attribute, we need to change the shade mode for each components. */
    gcSHADER_SHADERMODE componentShadeMode[4];

    /* Location index. */
    gctINT                        location;

    /* Flag whether attribute is invarinat or not. */
    gctBOOL                       isInvariant;

    /* Length of the attribute name. */
    gctINT                        nameLength;

    /* The attribute name. */
    char                          name[1];
};

/* Sampel structure, but inside a binary. */
typedef struct _gcBINARY_ATTRIBUTE
{
    /* Index of the attribute. */
    gctUINT16                    index;

    /* Type for this attribute of type gcATTRIBUTE_TYPE. */
    gctINT8                       type;

    /* Flag to indicate this attribute is used as a texture coordinate
       or packedAway. */
    gctINT8                       flags;

    /* Number of array elements for this attribute. */
    gctINT16                      arraySize;

    /* Length of the attribute name. */
    gctINT16                      nameLength;

    /* precision */
    gctINT16                      precision;

    /* Flag whether attribute is invarinat or not. */
    gctINT16                      isInvariant;

    /* The attribute name. */
    char                          name[1];
}
* gcBINARY_ATTRIBUTE;

/* Structure that defines inteface information associated with a variable (maybe a gcUNIFORM, gcVARIABLE) for a shader */
struct _gcsSHADER_VAR_INFO
{
    /* Variable category */
    gcSHADER_VAR_CATEGORY         varCategory;

    /* Data type for this most-inner variable. */
    gcSHADER_TYPE                 type;

    /* Only used for structure, block, or block member;
       When used for structure: it points to either first array element for
       arrayed struct or first struct element if struct is not arrayed;

       When used for block, point to first element in block */
    gctINT16                      firstChild;

    /* Only used for structure or blocks;
       When used for structure: it points to either next array element for
       arrayed struct or next struct element if struct is not arrayed;

       When used for block: it points to the next array element for block, if any */
    gctINT16                      nextSibling;

    /* Only used for structure or blocks;
       When used for structure: it points to either prev array element for
       arrayed struct or prev struct element if struct is not arayed;

       When used for block: it points to the previous array element for block, if any */
    gctINT16                      prevSibling;

    /* Only used for structure element, point to parent _gcUNIFORM */
    gctINT16                      parent;

    union
    {
        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                 numStructureElement;

        /* Number of element in block */
        gctUINT16                 numBlockElement;
    } u;

    /* Precision of the uniform. */
    gcSHADER_PRECISION            precision;

    /* Is variable a true array */
    gctBOOL                       isArray;

    /* Is local variable */
    gctBOOL                       isLocal;

    /* Is output variable */
    gctBOOL                       isOutput;

    /* Whether the variable is a pointer. */
    gctBOOL                       isPointer;

    /* Number of array elements for this uniform. */
    gctINT                        arraySize;

    /* Format of element of the variable. */
    gcSL_FORMAT                   format;

};

/* Structure that defines a uniform block for a shader. */
struct _gcsUNIFORM_BLOCK
{
    /* The object. */
    gcsOBJECT                       object;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind;

    gctBOOL                         _useLoadInst; /* indicate to use LOAD */
    gctBOOL                         _finished;

    gcsSHADER_VAR_INFO              info;

    /* Uniform block index */
    gctINT16                        blockIndex;

    /* Index of the uniform to keep the base address. */
    gctINT16                        index;

    /* Memory layout */
    gceINTERFACE_BLOCK_LAYOUT_ID    memoryLayout;

    /* block size in bytes */
    gctUINT32                       blockSize;

    /* number of uniforms in block */
    gctUINT32                       uniformCount;

    /* array of pointers to uniforms in block */
    gcUNIFORM                       *uniforms;

    /* If a uniform is used on both VS and PS,
    ** the index of this uniform on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Length of the uniform name. */
    gctINT                          nameLength;

    /* The uniform name. */
    char                            name[1];
};

/* Structure that defines a uniform block for a shader. */
typedef struct _gcBINARY_UNIFORM_BLOCK
{
    /* Memory layout */
    gctUINT16                       memoryLayout;

    /* block size in bytes */
    gctUINT16                       blockSize;

    /* Number of element in block */
    gctUINT16                       numBlockElement;

    /* points to first element in block */
    gctINT16                        firstChild;

    /* points to the next array element for block, if any */
    gctINT16                        nextSibling;

    /* points to the previous array element for block, if any */
    gctINT16                        prevSibling;

    /* Index of the uniform to keep the base address. */
    gctINT16                        index;

    /* Length of the uniform block name. */
    gctINT16                        nameLength;

    /* The uniform block name. */
    char                            name[1];
}
*gcBINARY_UNIFORM_BLOCK;

/* Structure that defines an uniform (constant register) for a shader. */
struct _gcUNIFORM
{
    /* The object. */
    gcsOBJECT                       object;

    /* Variable category */
    gcSHADER_VAR_CATEGORY           varCategory;

    /* Only used for structure or blocks;
       When used for structure, point to either first array element for
       arrayed struct or first struct element if struct is not arrayed

       When used for block, point to first element in block */
    gctINT16                        firstChild;

    /* Only used for structure or blocks;
       When used for structure, point to either next array element for
       arrayed struct or next struct element if struct is not arrayed;

       When used for block, point to next element in block */
    gctINT16                        nextSibling;

    /* Only used for structure or blocks;
       When used for structure, point to either prev array element for
       arrayed struct or prev struct element if struct is not arrayed;

       When used for block, point to previous element in block */
    gctINT16                        prevSibling;

    /* used for structure, point to parent _gcUNIFORM
       or for associated sampler of LOD_MIN_MAX and LEVEL_BASE_SIZE */
    gctINT16                        parent;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind;

    /* HALTI extras begin: */
    /* Uniform block index:
       Default block index = -1 */
    gctINT16                        blockIndex;

    /* stride on array or matrix */
    gctINT32                        arrayStride;
    gctINT16                        matrixStride;

    /* memory order of matrices */
    gctBOOL                         isRowMajor;

    /* offset from uniform block's base address */
    gctINT32                        offset;
    /* HALTI extras end: */

    union
    {
        /* Data type for this most-inner variable. */
        gcSHADER_TYPE               type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;

        /* Number of elements in block */
        gctUINT16                   numBlockElement;
    }
    u;

    /* Index of the uniform. */
    gctUINT16                       index;

    /* If this uniform is load from defaultUBO, this variable saved the temp index. */
    gctUINT16                       loadTempIndex;

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Index to image sampler for OpenCL */
    gctUINT16                       imageSamplerIndex;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    /* Flags. */
    gceUNIFORM_FLAGS                flags;

    /* Number of array elements for this uniform. */
    gctINT                          arraySize;

    /* Whether the uniform is part of model view projectoin matrix. */
    gctINT                          modelViewProjection;

    /* Format of element of the uniform shaderType. */
    gcSL_FORMAT                     format;

    /* Wheter the uniform is a pointer. */
    gctBOOL                         isPointer;

    /* Physically assigned values. */
    gctINT                          physical;
    gctUINT8                        swizzle;
    gctUINT32                       address;

    /* Compile-time constant value, */
    gcsValue                        initializer;

    /* If a uniform is used on both VS and PS,
    ** the index of this uniform on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Length of the uniform name. */
    gctINT                          nameLength;

    /* The uniform name. */
    char                            name[1];
};

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_UNIFORM
{
    union
    {
        /* Data type for this most-inner variable. */
        gctINT16                    type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;
    }
    u;

    /* Number of array elements for this uniform. */
    gctINT16                        arraySize;

    /* Length of the uniform name. */
    gctINT16                        nameLength;

    /* uniform flags */
    char                            flags[sizeof(gceUNIFORM_FLAGS)];

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcUNIFORM */
    gctINT16                        parent;

    /* precision */
    gctINT16                        precision;

    /* HALTI extras begin: */
    /* Uniform block index:
       Default block index = -1 */
    gctINT16                        blockIndex;

    /* stride on array or matrix */
    char                            arrayStride[sizeof(gctINT32)];

    gctINT16                        matrixStride;

    /* memory order of matrices */
    gctINT16                        isRowMajor;

    /* offset from uniform block's base address */
    char                            offset[sizeof(gctINT32)];

    /* compile-time constant value */
    char                            initializer[sizeof(gcsValue)];

    /* HALTI extras end: */
    /* The uniform name. */
    char                            name[1];
}
* gcBINARY_UNIFORM;

/* Same structure, but inside a binary with more variables. */
typedef struct _gcBINARY_UNIFORM_EX
{
    /* Uniform type of type gcUNIFORM_TYPE. */
    gctINT16                        type;

    /* Index of the uniform. */
    gctUINT16                       index;

    /* Number of array elements for this uniform. */
    gctINT16                        arraySize;

    /* Flags. */
    char                            flags[sizeof(gceUNIFORM_FLAGS)];

    /* Format of element of the uniform shaderType. */
    gctUINT16                       format;

    /* Wheter the uniform is a pointer. */
    gctINT16                        isPointer;

    /* Length of the uniform name. */
    gctINT16                        nameLength;

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Index to corresponding image sampler */
    gctUINT16                       imageSamplerIndex;

    /* compile-time constant value */
    char                            initializer[sizeof(gcsValue)];

    /* The uniform name. */
    char                            name[1];
}
* gcBINARY_UNIFORM_EX;


/* Structure that defines an output for a shader. */
struct _gcOUTPUT
{
    /* The object. */
    gcsOBJECT                       object;

    /* Type for this output. */
    gcSHADER_TYPE                   type;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    /* Number of array elements for this output. */
    gctINT                          arraySize;

    /* array Index for the output */
    gctINT                          arrayIndex;

    /* Temporary register index that holds the output value. */
    gctUINT16                       tempIndex;

    /* Converted to physical register. */
    gctBOOL                         convertedToPhysical;

    /* Flat output or smooth output. */
    gcSHADER_SHADERMODE             shaderMode;

    /* Location index. */
    gctINT                          location;
    gctINT                          output2RTIndex; /* user may specify location 1,3,
                                                     * real RT index are 1->0, 3->1 */

    /* Flag whether attribute is invarinat or not. */
    gctBOOL                         isInvariant;

    /* Length of the output name. */
    gctINT                          nameLength;

    /* The output name. */
    char                            name[1];
};

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_OUTPUT
{
    /* Type for this output. */
    gctINT8                         type;

    /* Number of array elements for this output. */
    gctINT8                         arraySize;

    /* Temporary register index that holds the output value. */
    gctUINT16                       tempIndex;

    /* Length of the output name. */
    gctINT16                        nameLength;

    /* precision */
    gctINT16                        precision;

    /* Flag whether attribute is invarinat or not. */
    gctINT16                        isInvariant;

    /* The output name. */
    char                            name[1];
}
* gcBINARY_OUTPUT;

/* Structure that defines a variable for a shader. */
struct _gcVARIABLE
{
    /* The object. */
    gcsOBJECT                       object;

    /* Variable category */
    gcSHADER_VAR_CATEGORY           varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcVARIABLE */
    gctINT16                        parent;

    union
    {
        /* Data type for this most-inner variable. */
        gcSHADER_TYPE               type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;
    }
    u;

    /* type qualifier */
    gctTYPE_QUALIFIER               qualifier;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    /* true if the variable is an local variable */
    gctBOOL                         isLocal;
    /* true if the variable is an output */
    gctBOOL                         isOutput;

    /* Number of array elements for this variable, at least 1 */
    gctINT                          arraySize;

    /* Start temporary register index that holds the variable value. */
    gctUINT16                       tempIndex;

    /* Length of the variable name. */
    gctINT                          nameLength;

    /* The variable name. */
    char                            name[1];
};

#define isVariableFunctionArg(v)        ((v)->varCategory >= gcSHADER_VAR_CATEGORY_FUNCTION_INPUT_ARGUMENT && (v)->varCategory <= gcSHADER_VAR_CATEGORY_FUNCTION_INOUT_ARGUMENT)
#define isVariableNormal(v)             (((v)->varCategory == gcSHADER_VAR_CATEGORY_NORMAL) || (isVariableFunctionArg(v)))

#define GET_BINARY_VARIABLE_PRECISION(flag)  (gcSHADER_PRECISION)((flag)&0x03)
#define GET_BINARY_VARIABLE_ISLOCAL(flag)    (gctBOOL)(((flag)&0x04) >> 2)
#define GET_BINARY_VARIABLE_ISOUTPUT(flag)   (gctBOOL)(((flag)&0x08) >> 3)
#define MAKE_BINARY_VARIABLE_FLAG(precision, isLocal, isOutput) \
          (((precision)&0x03) | ((isLocal != gcvFALSE) << 2) | ((isOutput != gcvFALSE) << 3))
/* Same structure, but inside a binary. */
typedef struct _gcBINARY_VARIABLE
{
    union
    {
        /* Data type for this most-inner variable. */
        gctINT8                     type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT8                    numStructureElement;
    }
    u;

    /* Number of array elements for this variable, at least 1,
           8 bit wide arraySize is not enough and does not match
           definition in struct _gcVARIABLE. This is a potential
           problem - KLC
        */
    gctINT8                         arraySize;

    /* Start temporary register index that holds the variable value. */
    gctUINT16                       tempIndex;

    /* Length of the variable name. */
    gctINT16                        nameLength;

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcVARIABLE */
    gctINT16                        parent;

    /* type qualifier is currently 16bit long.
       If it ever changes to more than 16bits, the alignment has to be adjusted
       when writing out to a shader binary
    */
    gctTYPE_QUALIFIER               qualifier;

    /* precision and isLocal*/
    gctINT16                        flags;

    /* The variable name. */
    char                            name[1];
}
* gcBINARY_VARIABLE;

typedef struct _gcsFUNCTION_ARGUMENT
{
    gctUINT16                       index;
    gctUINT8                        enable;
    gctUINT8                        qualifier;
}
gcsFUNCTION_ARGUMENT,
* gcsFUNCTION_ARGUMENT_PTR;

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_ARGUMENT
{
    gctUINT16                       index;
    gctUINT8                        enable;
    gctUINT8                        qualifier;
}
* gcBINARY_ARGUMENT;

typedef enum _gceFUNCTION_FLAG
{
  gcvFUNC_NOATTR        = 0x00,
  gcvFUNC_INTRINCICS    = 0x01, /* Function is openCL/OpenGL builtin function */
  gcvFUNC_ALWAYSINLINE  = 0x02, /* Always inline */
  gcvFUNC_NOINLINE      = 0x04, /* Neve inline */
  gcvFUNC_INLINEHINT    = 0x08, /* Inline is desirable */

  gcvFUNC_READNONE      = 0x10, /* Function does not access memory */
  gcvFUNC_READONLY      = 0x20, /* Function only reads from memory */
  gcvFUNC_STRUCTRET     = 0x40, /* Hidden pointer to structure to return */
  gcvFUNC_NORETURN      = 0x80, /* Function is not returning */

  gcvFUNC_INREG         = 0x100, /* Force argument to be passed in register */
  gcvFUNC_BYVAL         = 0x200, /* Pass structure by value */

} gceFUNCTION_FLAG;

typedef enum _gceINTRINSICS_KIND
{
    gceINTRIN_NONE         = 0, /* not an intrinsics */
    gceINTRIN_UNKNOWN      = 1, /* is an intrinsics, but unknown kind */

    /* common functions */
    gceINTRIN_clamp,
    gceINTRIN_min,
    gceINTRIN_max,
    gceINTRIN_sign,
    gceINTRIN_fmix,
    gceINTRIN_mix,
    /* Angle and Trigonometry Functions */
    gceINTRIN_radians,
    gceINTRIN_degrees,
    gceINTRIN_step,
    gceINTRIN_smoothstep,
    /* Geometric Functions */
    gceINTRIN_cross,
    gceINTRIN_fast_length,
    gceINTRIN_length,
    gceINTRIN_distance,
    gceINTRIN_dot,
    gceINTRIN_normalize,
    gceINTRIN_fast_normalize,
    gceINTRIN_faceforward,
    gceINTRIN_reflect,
    gceINTRIN_refract,
    /* Vector Relational Functions */
    gceINTRIN_isequal,
    gceINTRIN_isnotequal,
    gceINTRIN_isgreater,
    gceINTRIN_isgreaterequal,
    gceINTRIN_isless,
    gceINTRIN_islessequal,
    gceINTRIN_islessgreater,
    gceINTRIN_isordered,
    gceINTRIN_isunordered,
    gceINTRIN_isfinite,
    gceINTRIN_isnan,
    gceINTRIN_isinf,
    gceINTRIN_isnormal,
    gceINTRIN_signbit,
    gceINTRIN_lgamma,
    gceINTRIN_lgamma_r,
    gceINTRIN_shuffle,
    gceINTRIN_shuffle2,
    gceINTRIN_select,
    gceINTRIN_bitselect,
    gceINTRIN_any,
    gceINTRIN_all,
    /* Matrix Functions */
    gceINTRIN_matrixCompMult,
    /* Async copy and prefetch */
    gceINTRIN_async_work_group_copy,
    gceINTRIN_async_work_group_strided_copy,
    gceINTRIN_wait_group_events,
    gceINTRIN_prefetch,
    /* Atomic Functions */
    gceINTRIN_atomic_add,
    gceINTRIN_atomic_sub,
    gceINTRIN_atomic_inc,
    gceINTRIN_atomic_dec,
    gceINTRIN_atomic_xchg,
    gceINTRIN_atomic_cmpxchg,
    gceINTRIN_atomic_min,
    gceINTRIN_atomic_max,
    gceINTRIN_atomic_or,
    gceINTRIN_atomic_and,
    gceINTRIN_atomic_xor,

    /* work-item functions */
    gceINTRIN_get_global_id,
    gceINTRIN_get_local_id,
    gceINTRIN_get_group_id,
    gceINTRIN_get_work_dim,
    gceINTRIN_get_global_size,
    gceINTRIN_get_local_size,
    gceINTRIN_get_global_offset,
    gceINTRIN_get_num_groups,
    /* synchronization functions */
    gceINTRIN_barrier,
} gceINTRINSICS_KIND;

struct _gcsFUNCTION
{
    gcsOBJECT                       object;

    gctUINT32                       argumentArrayCount;
    gctUINT32                       argumentCount;
    gcsFUNCTION_ARGUMENT_PTR        arguments;

    gctUINT16                       label;
    gceFUNCTION_FLAG                flags;
    gceINTRINSICS_KIND              intrinsicsKind;

    /* Local variables. */
    gctUINT32                       localVariableCount;
    gcVARIABLE *                    localVariables;

    /* temp register start index and count */
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexCount;

    gctUINT                         codeStart;
    gctUINT                         codeCount;

    gctBOOL                         isRecursion;

    gctINT                          nameLength;
    char                            name[1];
};

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_FUNCTION
{
    gctINT16                        argumentCount;
    gctINT16                        localVariableCount;
    gctUINT16                       tempIndexStart;
    gctUINT16                       tempIndexCount;
    gctUINT16                       codeStart;
    gctUINT16                       codeCount;

    gctUINT16                       label;

    gctINT16                        nameLength;
    char                            name[1];
}
* gcBINARY_FUNCTION;

typedef struct _gcsIMAGE_SAMPLER
{
    /* Kernel function argument # associated with the image passed to the kernel function */
    gctUINT8                        imageNum;

    /* Sampler type either passed in as a kernel function argument which will be an argument #
            or
           defined as a constant variable inside the program which will be an unsigend integer value*/
    gctBOOL                         isConstantSamplerType;

    gctUINT32                       samplerType;
}
gcsIMAGE_SAMPLER,
* gcsIMAGE_SAMPLER_PTR;

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_IMAGE_SAMPLER
{
    /* index to uniform array associated with the sampler */
    gctUINT16                       uniformIndex;
    gctBOOL                         isConstantSamplerType;

    /* Kernel function argument # associated with the image passed to the kernel function */
    gctUINT8                        imageNum;

    /* Sampler type either passed in as a kernel function argument which will be an argument #
            or
           defined as a constant variable inside the program which will be an unsigend integer value*/
    gctUINT32                       samplerType;
}
* gcBINARY_IMAGE_SAMPLER;

typedef struct _gcsKERNEL_FUNCTION_PROPERTY
{
    gctINT                          propertyType;
    gctUINT32                       propertySize;
}
gcsKERNEL_FUNCTION_PROPERTY,
* gcsKERNEL_FUNCTION_PROPERTY_PTR;

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_KERNEL_FUNCTION_PROPERTY
{
    gctINT                          propertyType;
    gctUINT32                       propertySize;
}
* gcBINARY_KERNEL_FUNCTION_PROPERTY;

struct _gcsKERNEL_FUNCTION
{
    gcsOBJECT                       object;

    gcSHADER                        shader;
    gctUINT32                       argumentArrayCount;
    gctUINT32                       argumentCount;
    gcsFUNCTION_ARGUMENT_PTR        arguments;

    gctUINT16                       label;
    gceFUNCTION_FLAG                flags;

    /* Local address space size */
    gctUINT32                       localMemorySize;

    /* Uniforms Args */
    gctUINT32                       uniformArgumentArrayCount;
    gctUINT32                       uniformArgumentCount;
    gcUNIFORM *                     uniformArguments;
    gctINT                          samplerIndex;

    /* Image-Sampler associations */
    gctUINT32                       imageSamplerArrayCount;
    gctUINT32                       imageSamplerCount;
    gcsIMAGE_SAMPLER_PTR            imageSamplers;

    /* Local variables. */
    gctUINT32                       localVariableCount;
    gcVARIABLE *                    localVariables;

    /* temp register start index and count */
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexCount;

    /* Kernel function properties */
    gctUINT32                       propertyArrayCount;
    gctUINT32                       propertyCount;
    gcsKERNEL_FUNCTION_PROPERTY_PTR properties;
    gctUINT32                       propertyValueArrayCount;
    gctUINT32                       propertyValueCount;
    gctINT_PTR                      propertyValues;

    gctUINT                         codeStart;
    gctUINT                         codeCount;
    gctUINT                         codeEnd;

    gctBOOL                         isMain;

    gctINT                          nameLength;
    char                            name[1];
};

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_KERNEL_FUNCTION
{
    gctINT16                        argumentCount;
    gctUINT16                       label;
    gctUINT32                       localMemorySize;
    gctINT16                        uniformArgumentCount;
    gctINT16                        samplerIndex;
    gctINT16                        imageSamplerCount;
    gctINT16                        localVariableCount;
    gctUINT16                       tempIndexStart;
    gctUINT16                       tempIndexCount;
    gctINT16                        propertyCount;
    gctINT16                        propertyValueCount;

    gctUINT16                       codeStart;
    gctUINT16                       codeCount;
    gctUINT16                       codeEnd;

    gctUINT16                       isMain;

    gctINT16                        nameLength;
    char                            name[1];
}
* gcBINARY_KERNEL_FUNCTION;

/* Index into current instruction. */
typedef enum _gcSHADER_INSTRUCTION_INDEX
{
    gcSHADER_OPCODE,
    gcSHADER_SOURCE0,
    gcSHADER_SOURCE1,
}
gcSHADER_INSTRUCTION_INDEX;

typedef struct _gcSHADER_LINK * gcSHADER_LINK;

/* Structure defining a linked references for a label. */
struct _gcSHADER_LINK
{
    gcSHADER_LINK                   next;
    gctUINT                         referenced;
};

typedef struct _gcSHADER_LABEL * gcSHADER_LABEL;

/* Structure defining a label. */
struct _gcSHADER_LABEL
{
    gcSHADER_LABEL                  next;
    gctUINT                         label;
    gctUINT                         defined;
    gcSHADER_LINK                   referenced;
};

typedef struct _gcsVarTempRegInfo
{
    gcVARIABLE         variable;
    gctUINT32          streamoutSize;  /* size to write on feedback buffer */
    gctINT             tempRegCount;   /* number of temp register assigned
                                          to this variable */
    gcSHADER_TYPE *    tempRegTypes;   /* the type for each temp reg */
}
gcsVarTempRegInfo;

typedef struct _gcsTRANSFORM_FEEDBACK
{
    gctUINT32                       varyingCount;
    /* pointer to varyings to be streamed out */
    gcVARIABLE *                    varyings;
    gceFEEDBACK_BUFFER_MODE         bufferMode;
    /* driver set to 1 if transform feedback is active and not
       paused, 0 if inactive or paused */
    gcUNIFORM                       stateUniform;
    /* the temp register info for each varying */
    gcsVarTempRegInfo *             varRegInfos;
    union {
        /* array of uniform for separate transform feedback buffers */
        gcUNIFORM *                 separateBufUniforms;
        /* transfom feedback buffer for interleaved mode */
        gcUNIFORM                   interleavedBufUniform;
    } feedbackBuffer;
    gctINT                          shaderTempCount;
    /* total size to write to interleaved buffer for one vertex */
    gctUINT32                       totalSize;
}
gcsTRANSFORM_FEEDBACK;

typedef enum _gcSHADER_FLAGS
{
    gcSHADER_FLAG_OLDHEADER = 0x01, /* the old header word 5 is gcdSL_IR_VERSION,
                                          which always be 0x01 */
    gcSHADER_FLAG_OPENVG    = 0x02, /* the shader is an OpenVG shader */
    gcSHADER_FLAG_HWREG_ALLOCATED = 0x04, /* the shader is HW Register allocated
                                              * no need to geneated MOVA implicitly */
} gcSHADER_FLAGS;

typedef struct _gcLibraryList gcLibraryList;

struct _gcLibraryList
{
    gcSHADER        lib;
    gcLibraryList * next;
};

/* The structure that defines the gcSHADER object to the outside world. */
struct _gcSHADER
{
    /* The base object. */
    gcsOBJECT                   object;
    gceAPI                      clientApiVersion;  /* client API version. */
    gctUINT                     _id;                /* unique id used for triage */
    gctUINT                     _stringId;        /* unique id generated by shader source. */
    gctUINT                     _constVectorId;     /* unique constant uniform vector id */
    gctUINT                     _dummyUniformCount;
    gctUINT                     _tempRegCount;      /* the temp register count of the shader */

    /* Flag whether enable default UBO.*/
    gctBOOL                     enableDefaultUBO;
    gctINT                      _defaultUniformBlockIndex; /* Default uniform block index */

    /* Index of the uniform block that hold the const uniforms generated by compiler.*/
    gctINT                      constUniformBlockIndex;

    /* The size of the uniform block that hold the const uniforms. */
    gctINT                      constUBOSize;

    /* A memory buffer to store constant uniform. */
    gctUINT32 *                 constUBOData;

    /* Frontend compiler version */
    gctUINT32                   compilerVersion[2];

    /* Type of shader. */
    gcSHADER_KIND               type;

    /* Flags */
    gctUINT32                   flags;

    /* Maximum of kernel function arguments, used to calculation the starting uniform index */
    gctUINT32                   maxKernelFunctionArgs;

    /* Global uniform count, used for cl kernel patching. */
    gctUINT32                   globalUniformCount;

    /* Constant memory address space size for openCL */
    gctUINT32                   constantMemorySize;
    gctCHAR    *                constantMemoryBuffer;

    /* Private memory address space size for openCL */
    gctUINT32                   privateMemorySize;

    /* Local memory address space size for openCL, inherited from chosen kernel function */
    gctUINT32                   localMemorySize;

    /* Attributes. */
    gctUINT32                   attributeArraySize;
    gctUINT32                   attributeCount;
    gcATTRIBUTE *               attributes;

    /* Uniforms. */
    gctUINT32                   uniformArraySize;
    gctUINT32                   uniformCount;
    gctUINT32                   uniformVectorCount;  /* the vector count of all uniforms */
    gcUNIFORM *                 uniforms;
    gctINT                      samplerIndex;

    /* HALTI extras: Uniform block. */
    gctUINT32                   uniformBlockArraySize;
    gctUINT32                   uniformBlockCount;
    gcsUNIFORM_BLOCK *          uniformBlocks;

    /* HALTI extras: input/output buffer locations. */
    gctUINT32                   locationArraySize;
    gctUINT32                   locationCount;
    gctINT *                    locations;

    /* Outputs. */
    gctUINT32                   outputArraySize;    /* the size of 'outputs' be allocated */
    gctUINT32                   outputCount;        /* the output current be added, each
                                                       item in an array count as one output */
    gcOUTPUT *                  outputs;            /* pointer to the output array */

    /* Global variables. */
    gctUINT32                   variableArraySize;
    gctUINT32                   variableCount;
    gcVARIABLE *                variables;

    /* Functions. */
    gctUINT32                   functionArraySize;
    gctUINT32                   functionCount;
    gcFUNCTION *                functions;
    gcFUNCTION                  currentFunction;

    /* Kernel Functions. */
    gctUINT32                   kernelFunctionArraySize;
    gctUINT32                   kernelFunctionCount;
    gcKERNEL_FUNCTION *         kernelFunctions;
    gcKERNEL_FUNCTION           currentKernelFunction;

    /* Code. */
    gctUINT32                   codeCount;
    gctUINT                     lastInstruction;
    gcSHADER_INSTRUCTION_INDEX  instrIndex;
    gcSHADER_LABEL              labels;
    gcSL_INSTRUCTION            code;

    gctINT *                    loadUsers;

    /* load-time optimization uniforms */
    gctINT                      ltcUniformCount;      /* load-time constant uniform count */
    gctUINT                     ltcUniformBegin;      /* the begin offset of ltc in uniforms */
    gctINT *                    ltcCodeUniformIndex;  /* an array to map code index to uniform index,
                                                         element which has 0 value means no uniform for the code*/
    gctUINT                     ltcInstructionCount;  /* the total instruction count of the LTC expressions */
    gcSL_INSTRUCTION            ltcExpressions;       /* the expression array for ltc uniforms, which is a list of instructions */

#if gcdUSE_WCLIP_PATCH
    gctBOOL                     vsPositionZDependsOnW;    /* for wClip */
    gcSHADER_LIST               wClipTempIndexList;
    gcSHADER_LIST               wClipUniformIndexList;
#endif

    /* Optimization option. */
    gctUINT                     optimizationOption;

    /* Transform feedback varyings */
    gcsTRANSFORM_FEEDBACK       transformFeedback;

    /* Source code string */
    gctUINT                     sourceLength;            /* including terminating '\0' */
    gctSTRING                   source;

    /*  mapping from library temp index to linked to shader temp index */
    gctUINT32                   mappingTableExntries;
    gctINT *                    mappingTable;
    gctUINT                     linkedToShaderId;

    /* a list of library linked to this shader */
    gcLibraryList *             libraryList;
#if gcdSHADER_SRC_BY_MACHINECODE
    /* It is used to do high level GLSL shader detection, and give a BE a replacement index hint to replace
       automatically compiled machine code with manually written extremely optimized machine code. So it is
       a dynamic info based on driver client code, and supposedly it is not related to gcSL structure. So DO
       NOT CONSIDER IT IN LOAD/SAVE ROUTINES. Putting this info in gcSL structure because we don't want to
       break existed prototype of gcLinkShaders. If later somebody find another better place to pass this info
       into gcLinkShaders, feel free to change it */
    gctUINT32                   replaceIndex;
#endif

    gctBOOL                     dual16Mode;
    gctBOOL                     forceDisableDual16;

    gctUINT32                   RARegWaterMark;
    gctUINT32                   RATempReg;

    /* Disable EarlyZ for this program. */
    gctBOOL                     disableEarlyZ;
};

END_EXTERN_C()

#endif /* __gc_vsc_old_gcsl_h_ */


