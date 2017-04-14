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


/*********************************************************************************
   Following definitions are ONLY used for openGL(ES) to communicate with client
   query/binding request. NOTE that ONLY active parts are collected into each high
   level table.
**********************************************************************************/

#ifndef __gc_vsc_drvi_program_profile_h_
#define __gc_vsc_drvi_program_profile_h_

BEGIN_EXTERN_C()

typedef enum _SHADER_DATA_TYPE
{
    SHADER_DATA_TYPE_UNKONW    = 0,
}
SHADER_DATA_TYPE;

typedef enum _SHADER_DATA_PRECISION
{
    SHADER_DATA_PRECISION_DEFAULT  = 0,
    SHADER_DATA_PRECISION_HIGH     = 1,
    SHADER_DATA_PRECISION_MEDIUM   = 2,
    SHADER_DATA_PRECISION_LOW      = 3,
}
SHADER_DATA_PRECISION;

typedef enum _FX_BUFFERING_MODE
{
    FX_BUFFERING_MODE_INTERLEAVED  = 0,
    FX_BUFFERING_MODE_SEPARATED    = 1,
}
FX_BUFFERING_MODE;

typedef enum MATRIX_MAJOR
{
    MATRIX_MAJOR_ROW                    = 0,
    MATRIX_MAJOR_COLUMN                 = 1,
}
MATRIX_MAJOR;

typedef enum SL_UNIFORM_USAGE
{
    /* All user defined uniforms will get this type */
    SL_UNIFORM_USAGE_USER_DEFINED       = 0,

    /* Built-in uniforms */
    SL_UNIFORM_USAGE_DEPTHRANGE_NEAR    = 1,
    SL_UNIFORM_USAGE_DEPTHRANGE_FAR     = 2,
    SL_UNIFORM_USAGE_DEPTHRANGE_DIFF    = 3,
    SL_UNIFORM_USAGE_HEIGHT             = 4,
}
SL_UNIFORM_USAGE;

typedef enum SL_FRAGOUT_USAGE
{
    SL_FRAGOUT_USAGE_USER_DEFINED       = 0,
    SL_FRAGOUT_USAGE_FRAGCOLOR          = 1,
    SL_FRAGOUT_USAGE_FRAGDATA           = 2,
}
SL_FRAGOUT_USAGE;

typedef enum SL_FX_STREAMING_MODE
{
    SL_FX_STREAMING_MODE_FFU            = 0,
    SL_FX_STREAMING_MODE_SHADER         = 1,
}
SL_FX_STREAMING_MODE;

/* Attribute table definition

   1. glGetActiveAttrib works on boundary of SL_ATTRIBUTE_TABLE_ENTRY which currently does not support
      array. It even must work when glLinkProgram failed.
   2. glGetAttribLocation and other location related functions work on boundary of SHADER_IO_REG_MAPPING,
      i.e, it must be on vec4 boundary.
*/

typedef struct SL_ATTRIBUTE_TABLE_ENTRY
{
    SHADER_DATA_TYPE                            type;
    gctCONST_STRING                             name;

    /* Decl'ed array size by GLSL, this time, it must be 1 */
    gctSIZE_T                                   arraySize;

    /* Index based on countOfEntries in SL_ATTRIBUTE_TABLE. It is corresponding to active attribute index */
    gctUINT                                     attribEntryIndex;

    /* Points to HW IO mapping that VS SEP maintains. Compiler MUST assure ioRegMappings are squeezed
       together for all active vec4 that activeVec4Mask are masked. So driver just needs go through
       activeVec4Mask to program partial of type to HW when programing vertexelement/vs input related
       registers */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* There are 2 types of pre-assigned location, from high priority to low priority
       1. Shader 'layout(location = ?)' qualifier
       2. By glBindAttribLocation API call

       If user didn't assign it, we should allocate it then. Each ioRegMapping has a
       corresponding location. */
    gctUINT*                                    pLocation;

    /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4 */
    gctUINT                                     vec4BasedSize;

    /* To vec4BasedSize vec4 that expanded from type, which are really used by VS */
    gctUINT                                     activeVec4Mask;
}
SL_ATTRIBUTE_TABLE_ENTRY;

typedef struct SL_ATTRIBUTE_TABLE
{
    /* Entries for active attributes of VS */
    SL_ATTRIBUTE_TABLE_ENTRY*                   pAttribEntries;
    gctUINT                                     countOfEntries;

    /* For query with ACTIVE_ATTRIBUTE_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
SL_ATTRIBUTE_TABLE;

/* Uniform (default uniform-block) table definition

   1. glGetActiveUniform works on boundary of SL_UNIFORM_TABLE_ENTRY which does not support struct,
      and the least supported data layout is array. It even must work when glLinkProgram failed.
   2. glGetUniformLocation and other location related functions work on boundary of element of
      scalar/vec/matrix/sampler array, i.e, single scalar/vec/matrix/sampler boundary.
*/

typedef struct SL_UNIFORM_HW_SUB_MAPPING
{
    /* We have 2 choices here

       1. Dynamic-indexed sub array or imm-indexed sub array?
          For Dynamic-indexed sub array, HW register/memory must be successive, while for imm-indexed
          sub array, each element can be allocated into anywhere HW can hold it, by which we can reduce
          HW uniform storage footprint. So do we need split imm-indexed sub array into more pieces??

       2. What's more, do we need much smaller granularity other than element of array, such as vec or
          channel??

       Currently, imm-indexed sub array is regard as dynamic-indexed sub array and granularity of element
       of array are used, by which, overhead of driver is small, but HW uniform storage footprint is big.
    */

    /* Active range of sub array of whole array uniform sized by 'activeArraySize'. */
    gctUINT                                     startIdx;
    gctUINT                                     rangeOfSubArray;

    /* Which channels are really used by compiler. For example, vec3 can be 0x07, 0x06 and 0x05,etc and when
       it is 0x05, although CHANNEL_Y is disabled, compiler will still allocate continuous 3 HW channels for it.
       That means compiler will assure minimum usage of HW channels while hole is permitted, unless above 2nd
       choice is channel-based granularity (see commments of validChannelMask of SHADER_CONSTANT_SUB_ARRAY_MAPPING).
       To driver uniform update, it then can use validChannelMask to match validChannelMask of SHADER_CONSTANT_SUB_ARRAY_MAPPING
       (further validHWChannelMask) to determine where data goes. For sampler, it must be 0x01 */
    gctUINT                                     validChannelMask;

    /* Points to HW constant/sampler mapping that SEPs maintains. Compiler assure this active sub range is
       mapped to successive HW register/memory. */
    union
    {
        /* Directly increase HW regNo or memory address of pSubCBMapping with (4-tuples * (1~4) based on type of
           element) to access next element of array if 'rangeOfSubArray' is GT 1 */
        SHADER_CONSTANT_SUB_ARRAY_MAPPING*      pSubCBMapping;

        /* Use (pSamplerMapping ++) to next sampler mapping access if 'rangeOfSubArray' is GT 1 */
        SHADER_SAMPLER_SLOT_MAPPING*            pSamplerMapping;
    } hwMapping;
}
SL_UNIFORM_HW_SUB_MAPPING;

typedef struct SL_UNIFORM_HW_MAPPING
{
    /* From spec, when mapping to mem, it looks individual uniform of named uniform block can not be separated
       into several active sections due to there is no API let user set part of uniform data, so as long as any
       part of such uniform is used, compiler must deem all data of uniform is used. In such case, pHWSubMappings
       maps whole of uniform to HW resource, and countOfHWSubMappings must be 1 */
    SL_UNIFORM_HW_SUB_MAPPING*                  pHWSubMappings;
    gctUINT                                     countOfHWSubMappings;
}
SL_UNIFORM_HW_MAPPING;

typedef struct SL_UNIFORM_COMMON_ENTRY
{
    SHADER_DATA_TYPE                            type;
    gctCONST_STRING                             name;
    SHADER_DATA_PRECISION                       precision;

    /* Decl'ed array size by GLSL */
    gctSIZE_T                                   arraySize;

    /* Maximun used element index + 1, considering in whole program scope */
    gctSIZE_T                                   activeArraySize;

    /* It is corresponding to active uniform index. NOTE that this index is numbered in global
       scope, including default uniform block and all named uniform blocks */
    gctUINT                                     uniformEntryIndex;

    /* Different shader stage may have different HW mappings. */
    SL_UNIFORM_HW_MAPPING                       hwMappings[SHADER_TYPE_GL_SUPPORTED_COUNT];
}
SL_UNIFORM_COMMON_ENTRY;

typedef struct SL_UNIFORM_TABLE_ENTRY
{
    SL_UNIFORM_COMMON_ENTRY                     common;

    /* Specially for built-in ones */
    SL_UNIFORM_USAGE                            usage;

    /* First location index of this uniform, the last location for this uniform is
       locationBase + (activeArraySize of SL_UNIFORM_COMMON_ENTRY) - 1 */
    gctUINT                                     locationBase;
}
SL_UNIFORM_TABLE_ENTRY;

typedef struct SL_UNIFORM_TABLE
{
    /* Entries for active uniforms in default uniform block of whole program. They are
       arranged into 3 chunks in order (sampler + builtin + user-defined) */
    SL_UNIFORM_TABLE_ENTRY*                     pUniformEntries;
    gctUINT                                     countOfEntries;
    gctUINT                                     firstBuiltinUniformEntryIndex;
    gctUINT                                     firstUserUniformEntryIndex;

    /* For query with ACTIVE_UNIFORM_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
SL_UNIFORM_TABLE;

/* Uniform-block (named uniform-block, pure constants, can't be samplers) table definition

   1. glGetActiveUniformBlockiv and other uniform block index related functions work on boundary of
      SL_UNIFORMBLOCK_TABLE_ENTRY (uniform group).
   2. glGetActiveUniform works on boundary of SL_UNIFORMBLOCK_UNIFORM_ENTRY which does not support
      struct, and the least supported data layout is array. It even must work when glLinkProgram failed.
   3. Uniforms of uniform-block have no location concept.
*/

typedef struct SL_UNIFORMBLOCK_UNIFORM_ENTRY
{
    SL_UNIFORM_COMMON_ENTRY                     common;

    /* Layout info for individual uniform of uniform block */
    gctSIZE_T                                   offset;
    gctSIZE_T                                   arrayStride;
    gctSIZE_T                                   matrixStride;
    MATRIX_MAJOR                                matrixMajor;
}
SL_UNIFORMBLOCK_UNIFORM_ENTRY;

typedef struct SL_UNIFORMBLOCK_TABLE_ENTRY
{
    SL_UNIFORMBLOCK_UNIFORM_ENTRY*              pUniformEntries;
    gctUINT                                     countOfEntries;

    /* Each includes copy of uniformEntryIndex of 'common' of pUniformEntries. Whole list is for
       query with UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES */
    gctUINT*                                    pUniformIndices;

    /* Index based on countOfEntries in SL_UNIFORMBLOCK_TABLE. It is corresponding to active uniform
       block index */
    gctUINT                                     ubEntryIndex;

    /* How many byte size does storage store this uniform block in minimum? */
    gctUINT                                     dataSizeInByte;

    /* If true, there must be something in hwMappings in SL_UNIFORM_COMMON_ENTRY */
    gctBOOL                                     bRefedByShader[SHADER_TYPE_GL_SUPPORTED_COUNT];
}
SL_UNIFORMBLOCK_TABLE_ENTRY;

typedef struct SL_UNIFORMBLOCK_TABLE
{
    /* Entries for active uniform blocks of whole program */
    SL_UNIFORMBLOCK_TABLE_ENTRY*                pUniformBlockEntries;
    gctUINT                                     countOfEntries;

    /* For query with ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH */
    gctUINT                                     maxLengthOfName;
}
SL_UNIFORMBLOCK_TABLE;

/* Fragment-output table definition

   1. No API works on boundary of SL_FRAGOUT_TABLE_ENTRY.
   2. glGetFragDataLocation works on boundary of SHADER_IO_REG_MAPPING, i.e, it must be on vec4 boundary.
*/

typedef struct SL_FRAGOUT_TABLE_ENTRY
{
    SHADER_DATA_TYPE                            type;
    gctCONST_STRING                             name;

    /* Specially for built-in ones */
    SL_FRAGOUT_USAGE                            usage;

    /* Index based on countOfEntries in SL_FRAGOUT_TABLE. */
    gctUINT                                     fragOutEntryIndex;

    /* Points to HW IO mapping that PS SEP maintains. Compiler MUST assure ioRegMappings are squeezed
       together for all active vec4 that activeVec4Mask are masked. So driver just needs go through
       activeVec4Mask to program partial of type to HW when programing PE/ps output related registers */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* Shader can have 'layout(location = ?)' qualifier, if so, just use it. Otherwise, we should allocate
       it then. Each ioRegMapping has a corresponding location. */
    gctUINT*                                    pLocation;

    /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4. Since
       frag output variable can only be scalar or vector, so it actually is the array size GLSL decl'ed */
    gctUINT                                     vec4BasedSize;

    /* To vec4BasedSize vec4 that expanded from type, which are really used by PS */
    gctUINT                                     activeVec4Mask;
}
SL_FRAGOUT_TABLE_ENTRY;

typedef struct SL_FRAGOUT_TABLE
{
    /* Entries for active fragment color of fragment shader */
    SL_FRAGOUT_TABLE_ENTRY*                     pFragOutEntries;
    gctUINT                                     countOfEntries;
}
SL_FRAGOUT_TABLE;

/* Transform feedback table definition

   glTransformFeedbackVaryings and glGetTransformFeedbackVarying work on boundary of SL_TRANSFORM_FEEDBACK_OUT_TABLE_ENTRY.
*/

typedef struct SL_TRANSFORM_FEEDBACK_OUT_TABLE_ENTRY
{
    SHADER_DATA_TYPE                            type;
    gctCONST_STRING                             name;

    /* Decl'ed array size by GLSL */
    gctSIZE_T                                   arraySize;

    /* Index based on countOfEntries in SL_TRANSFORM_FEEDBACK_OUT_TABLE. */
    gctUINT                                     fxOutEntryIndex;

    /* Data streamming-2-memory mode, by fixed function unit or shader? */
    SL_FX_STREAMING_MODE                        fxStreamingMode;

    union
    {
        /* For writing to XFBO with fixed-function */
        struct
        {
            /* Points to HW IO mapping that VS SEP maintains.

               From spec, if VS has no write to output variable, the value streamed out to buffer is undefined.
               So, we have 2 choices depending on compiler design.

               1. Compiler MUST assure ioRegMappings are squeezed together for all active vec4 that activeVec4Mask
                  are masked. So driver needs go through activeVec4Mask to select correct HW reg for active ones and
                  any of HW reg for unactive ones when programing SO related registers.

               2. Compiler allocate HW regs for vec4 by regarding them all active. In this case, activeVec4Mask still
                  points out which are really written by VS

               Choice 2 is simple, but choice 1 can get better perf */
            SHADER_IO_REG_MAPPING*              pIoRegMapping;

            /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4 */
            gctUINT                             vec4BasedSize;

            /* To vec4BasedSize vec4 that expanded from type, which are really written by VS */
            gctUINT                             activeVec4Mask;
        } s;

        /* For the case that shader stores data into XFBO */
        SHADER_UAV_SLOT_MAPPING*                pSoBufferMapping;
    } u;
}
SL_TRANSFORM_FEEDBACK_OUT_TABLE_ENTRY;

typedef struct SL_TRANSFORM_FEEDBACK_OUT_TABLE
{
    /* Entries for active transform feedback varyings. pFxOutEntries must be in-orderly corresponding to
       param 'varyings' of glTransformFeedbackVaryings, and countOfEntries is also corresponding to param
       'count' of that API */
    SL_TRANSFORM_FEEDBACK_OUT_TABLE_ENTRY*      pFxOutEntries;
    gctUINT                                     countOfEntries;

    /* To control how to serialize to buffer */
    FX_BUFFERING_MODE                           mode;

    /* For query with TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
SL_TRANSFORM_FEEDBACK_OUT_TABLE;

/* Executable program profile definition. It is generated only when glLinkProgram or glProgramBinary
   calling. Each client program only has one profile like this. */
typedef struct PROGRAM_EXECUTABLE_PROFILE
{
    /* The shaders that this program contains. Add new shaders here, for example, geometry shader */
    SHADER_EXECUTABLE_PROFILE                   ExecutableVS;
    SHADER_EXECUTABLE_PROFILE                   ExecutablePS;

    /* High level mapping tables from high level variables to # */
    SL_ATTRIBUTE_TABLE                          attribTable;
    SL_UNIFORM_TABLE                            uniformTable;
    SL_UNIFORMBLOCK_TABLE                       uniformBlockTable;
    SL_FRAGOUT_TABLE                            fragOutTable;
    SL_TRANSFORM_FEEDBACK_OUT_TABLE             fxOutTable;
}
PROGRAM_EXECUTABLE_PROFILE;

END_EXTERN_C();

#endif /* __gc_vsc_drvi_program_profile_h_ */


