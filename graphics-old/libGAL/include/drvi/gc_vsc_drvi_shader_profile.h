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


/***********************************************************************************
   All following definitions are designed for DX/openGL(ES)/openCL drivers to manage
   to do shaders' HW-level linkage programming, shader flush programming and shader
   level recompiler.
************************************************************************************/
#ifndef __gc_vsc_drvi_shader_profile_h_
#define __gc_vsc_drvi_shader_profile_h_

BEGIN_EXTERN_C()

/* Some macroes for version type */
#define ENCODE_SHADER_VER_TYPE(client, type, major, minor)      ((((client) & 0xFF) << 24) | \
                                                                 (((type)   & 0xFF) << 16) | \
                                                                 (((major)  & 0xFF) << 08) | \
                                                                 (((minor)  & 0xFF) << 00))
#define DECODE_SHADER_CLIENT(vt)         (((vt) >> 24) & 0xFF)
#define DECODE_SHADER_TYPE(vt)           (((vt) >> 16) & 0xFF)
#define DECODE_SHADER_MAJOR_VER(vt)      (((vt) >> 08) & 0xFF)
#define DECODE_SHADER_MINOR_VER(vt)      (((vt) >> 00) & 0xFF)

/* Capability definition of executable profile */

#define MAX_SHADER_IO_NUM                      32
#define MAX_PS_OUTPUT_NUM                      8

/* 0~15 for client normal constant buffers, such as DX cb/icb or GLES uniform block
   16 for normal uniforms of OGL(ES), AKA, default uniform block
   17 for LTC constants
   18 for immediate values
   19~31 for other internal usages of constant buffers for patches and other recompiler requests */
#define MAX_SHADER_CONSTANT_ARRAY_NUM          32
#define GLCL_NORMAL_UNIFORM_CONSTANT_ARRAY     16
#define LTC_CONSTANT_ARRAY                     GLCL_NORMAL_UNIFORM_CONSTANT_ARRAY + 1
#define IMM_CONSTANT_ARRAY                     LTC_CONSTANT_ARRAY + 1
#define START_CONSTANT_ARRAY_FOR_PATCH         IMM_CONSTANT_ARRAY + 1

#define MAX_SHADER_SAMPLER_NUM                 16
#define MAX_SHADER_RESOURCE_NUM                128
#define MAX_SHADER_STREAM_OUT_BUFFER_NUM       4
#define MAX_SHADER_UAV_NUM                     8

/* Channel definition */

#define CHANNEL_NUM                            4
#define CHANNEL_X                              0  /* R */
#define CHANNEL_Y                              1  /* G */
#define CHANNEL_Z                              2  /* B */
#define CHANNEL_W                              3  /* A */

#define NOT_ASSIGNED                           0xFFFFFFFF

typedef enum SHADER_TYPE
{
    SHADER_TYPE_UNKNOWN             = 0,
    SHADER_TYPE_VERTEX              = 1,
    SHADER_TYPE_PIXEL               = 2,
    SHADER_TYPE_GEOMETRY            = 3,
    SHADER_TYPE_HULL                = 4,
    SHADER_TYPE_DOMAIN              = 5,

    /* GPGPU shader, such as openCL and DX's compute */
    SHADER_TYPE_GENERAL             = 6,

    SHADER_TYPE_GL_SUPPORTED_COUNT  = SHADER_TYPE_PIXEL,
}
SHADER_TYPE;

typedef enum SHADER_CLIENT
{
    SHADER_CLIENT_UNKNOWN           = 0,
    SHADER_CLIENT_DX                = 1,
    SHADER_CLIENT_GL                = 2,
    SHADER_CLIENT_GLES              = 3,
    SHADER_CLIENT_CL                = 4,
}
SHADER_CLIENT;

typedef enum SHADER_IO_USAGE
{
    /* 0 - 13 must be equal to D3DDECLUSAGE!!!!!!! SO DONT CHANGE THEIR VALUES!!!! */

    /* Common usages, for gfx clients only */
    SHADER_IO_USAGE_POSITION                 = 0,
    SHADER_IO_USAGE_TESSFACTOR               = 8,
    SHADER_IO_USAGE_ISFRONTFACE              = 18,

    /* For gfx clients only, but excluding DX1x */
    SHADER_IO_USAGE_BLENDWEIGHT              = 1,
    SHADER_IO_USAGE_BLENDINDICES             = 2,
    SHADER_IO_USAGE_NORMAL                   = 3,
    SHADER_IO_USAGE_POINTSIZE                = 4,
    SHADER_IO_USAGE_TEXCOORD                 = 5,
    SHADER_IO_USAGE_TANGENT                  = 6,
    SHADER_IO_USAGE_BINORMAL                 = 7,
    SHADER_IO_USAGE_TRANSFORMEDPOS           = 9,
    SHADER_IO_USAGE_COLOR                    = 10,
    SHADER_IO_USAGE_FOG                      = 11,
    SHADER_IO_USAGE_DEPTH                    = 12,
    SHADER_IO_USAGE_SAMPLE                   = 13,

    /* For gfx clients only, but excluding DX9 - SGV */
    SHADER_IO_USAGE_VERTEXID                 = 14,
    SHADER_IO_USAGE_PRIMITIVEID              = 15,
    SHADER_IO_USAGE_INSTANCEID               = 16,
    SHADER_IO_USAGE_INPUTCOVERAGE            = 17,
    SHADER_IO_USAGE_SAMPLEINDEX              = 19,
    SHADER_IO_USAGE_OUTPUTCONTROLPOINTID     = 20,
    SHADER_IO_USAGE_FORKINSTANCEID           = 21,
    SHADER_IO_USAGE_JOININSTANCEID           = 22,
    SHADER_IO_USAGE_DOMAIN                   = 23,
    SHADER_IO_USAGE_THREADID                 = 24,
    SHADER_IO_USAGE_THREADGROUPID            = 25,
    SHADER_IO_USAGE_THREADIDINGROUP          = 26,
    SHADER_IO_USAGE_THREADIDINGROUPFLATTENED = 27,

    /* For gfx clients only, but excluding DX9 - SIV */
    SHADER_IO_USAGE_CLIPDISTANCE             = 28,
    SHADER_IO_USAGE_CULLDISTANCE             = 29,
    SHADER_IO_USAGE_RENDERTARGETARRAYINDEX   = 30,
    SHADER_IO_USAGE_VIEWPORTARRAYINDEX       = 31,
    SHADER_IO_USAGE_DEPTHGREATEREQUAL        = 32,
    SHADER_IO_USAGE_DEPTHLESSEQUAL           = 33,
    SHADER_IO_USAGE_INSIDETESSFACTOR         = 34,

    /* A special usage which means IO is used by general purpose */
    SHADER_IO_USAGE_GENERAL                  = 35,

    /* Add NEW usages here */

    /* Must be at last!!!!!!! */
    SHADER_IO_USAGE_TOTAL_COUNT,
}
SHADER_IO_USAGE;

typedef enum SHADER_CONSTANT_USAGE
{
    /* DX9 only */
    SHADER_CONSTANT_USAGE_FLOAT              = 0,
    SHADER_CONSTANT_USAGE_INTEGER            = 1,
    SHADER_CONSTANT_USAGE_BOOLEAN            = 2,

    /* For other clients */
    SHADER_CONSTANT_USAGE_MIXED              = 3,

    /* Must be at last!!!!!!! */
    SHADER_CONSTANT_USAGE_TOTAL_COUNT        = 4,
}
SHADER_CONSTANT_USAGE;

typedef enum SHADER_RESOURCE_DIMENSION
{
    SHADER_RESOURCE_DIMENSION_UNKNOW         = 0,
    SHADER_RESOURCE_DIMENSION_BUFFER         = 1,
    SHADER_RESOURCE_DIMENSION_1D             = 2,
    SHADER_RESOURCE_DIMENSION_1DARRAY        = 3,
    SHADER_RESOURCE_DIMENSION_2D             = 4,
    SHADER_RESOURCE_DIMENSION_2DARRAY        = 5,
    SHADER_RESOURCE_DIMENSION_2DMS           = 6,
    SHADER_RESOURCE_DIMENSION_2DMSARRAY      = 7,
    SHADER_RESOURCE_DIMENSION_3D             = 8,
    SHADER_RESOURCE_DIMENSION_CUBE           = 9,
    SHADER_RESOURCE_DIMENSION_CUBEARRAY      = 10,

    /* Must be at last!!!!!!! */
    SHADER_RESOURCE_DIMENSION_TOTAL_COUNT,
}
SHADER_RESOURCE_DIMENSION;

typedef enum SHADER_SAMPLER_MODE
{
    SHADER_SAMPLER_MODE_DEFAULT              = 0,
    SHADER_SAMPLER_MODE_COMPARISON           = 1,
    SHADER_SAMPLER_MODE_MONO                 = 2,
}
SHADER_SAMPLER_MODE;

typedef enum SHADER_RESOURCE_RETURN_TYPE
{
    SHADER_RESOURCE_RETURN_TYPE_UNORM        = 0,
    SHADER_RESOURCE_RETURN_TYPE_SNORM        = 1,
    SHADER_RESOURCE_RETURN_TYPE_SINT         = 2,
    SHADER_RESOURCE_RETURN_TYPE_UINT         = 3,
    SHADER_RESOURCE_RETURN_TYPE_FLOAT        = 4,
}
SHADER_RESOURCE_RETURN_TYPE;

typedef enum SHADER_RESOURCE_ACCESS_MODE
{
    SHADER_RESOURCE_ACCESS_MODE_TYPE         = 0,
    SHADER_RESOURCE_ACCESS_MODE_RAW          = 1,
    SHADER_RESOURCE_ACCESS_MODE_STRUCTURED   = 2,
}
SHADER_RESOURCE_ACCESS_MODE;

typedef enum SHADER_UAV_DIMENSION
{
    SHADER_UAV_DIMENSION_UNKNOWN             = 0,
    SHADER_UAV_DIMENSION_BUFFER              = 1,
    SHADER_UAV_DIMENSION_1D                  = 2,
    SHADER_UAV_DIMENSION_1DARRAY             = 3,
    SHADER_UAV_DIMENSION_2D                  = 4,
    SHADER_UAV_DIMENSION_2DARRAY             = 5,
    SHADER_UAV_DIMENSION_3D                  = 6,

    /* Must be at last!!!!!!! */
    SHADER_UAV_DIMENSION_TOTAL_COUNT,
}
SHADER_UAV_DIMENSION;

typedef enum SHADER_UAV_TYPE
{
    SHADER_UAV_TYPE_UNORM                    = 0,
    SHADER_UAV_TYPE_SNORM                    = 1,
    SHADER_UAV_TYPE_SINT                     = 2,
    SHADER_UAV_TYPE_UINT                     = 3,
    SHADER_UAV_TYPE_FLOAT                    = 4,
}
SHADER_UAV_TYPE;

typedef enum SHADER_UAV_ACCESS_MODE
{
    SHADER_UAV_ACCESS_MODE_TYPE              = 0,
    SHADER_UAV_ACCESS_MODE_RAW               = 1,
    SHADER_UAV_ACCESS_MODE_STRUCTURED        = 2,
}
SHADER_UAV_ACCESS_MODE;

typedef enum SHADER_HW_ACCESS_MODE
{
    SHADER_HW_ACCESS_MODE_REGISTER           = 0,
    SHADER_HW_ACCESS_MODE_MEMORY             = 1
}
SHADER_HW_ACCESS_MODE;

typedef enum SHADER_PATCH_CONSTANT_MODE
{
    SHADER_PATCH_CONSTANT_MODE_CTC           = 0,
    SHADER_PATCH_CONSTANT_MODE_VAL_2_INST    = 1,
    SHADER_PATCH_CONSTANT_MODE_VAL_2_MEMORREG= 2,
}
SHADER_PATCH_CONSTANT_MODE;

/* Define Shader Static Patch constant flag */
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG                0x00000001
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG_LOCAL          0x00000002
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG_SAMPLER        0x00000004
#define SSP_CONSTANT_FLAG_CL_LOCAL_ADDRESS_SPACE       0x00000008
#define SSP_CONSTANT_FLAG_CL_PRIVATE_ADDRESS_SPACE     0x00000010
#define SSP_CONSTANT_FLAG_CL_CONSTANT_ADDRESS_SPACE    0x00000020
#define SSP_CONSTANT_FLAG_CL_GLOBAL_SIZE               0x00000040
#define SSP_CONSTANT_FLAG_CL_LOCAL_SIZE                0x00000080
#define SSP_CONSTANT_FLAG_CL_NUM_GROUPS                0x00000100
#define SSP_CONSTANT_FLAG_CL_GLOBAL_OFFSET             0x00000200
#define SSP_CONSTANT_FLAG_CL_WORK_DIM                  0x00000400
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG_CONSTANT       0x00000800
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG_LOCAL_MEM_SIZE 0x00001000
#define SSP_CONSTANT_FLAG_CL_KERNEL_ARG_PRIVATE        0x00002000
#define SSP_CONSTANT_FLAG_HALTI_FX_BUFFER              0x00010000
#define SSP_CONSTANT_FLAG_HALTI_FX_STATE               0x00020000

/* Define Shader Static Patch common flag */
#define SSP_COMMON_FLAG_STREAMOUT_BY_STORE             0x00000001
#define SSP_COMMON_FLAG_LOCAL_MEMORY                   0x00000002 /* For register spillage and CL private mem */
#define SSP_COMMON_FLAG_SHARED_MEMORY                  0x00000004 /* For CL local memory or DirectCompute shared mem */

/* IO mapping table definitions, for v# and o#

   They are used to HW-level linkage programming. For DX9, such linking will alway
   use usage and usage index, but for others clients, it will use ioIndex (ioIndex
   of peers must be same) for most IOs, and usage and usage index for special IOs,
   such as frontface and pointsize, etc
*/

typedef struct SHADER_IO_CHANNEL_MAPPING
{
    struct
    {
        /* This channel is active within shader */
        gctUINT                                 bActiveWithinShader  : 1;

        /* This IO channel is declared by 'DCL' instruction, DX only */
        gctUINT                                 bDeclared            : 1;

        /* PS input only, compared with 'constant' from heading vertex */
        gctUINT                                 bNeedInterpolate     : 1;

        /* PS input only, interpolate is from centroid, not pixel center */
        gctUINT                                 bCentroidInterpolate : 1;

        /* PS input only, for MSAA's subpixel rendering */
        gctUINT                                 bSampleFrequency     : 1;

        /* HW reg channel this IO channel is colored to. The basic rule of HW channel is total HW
           channel mask of hwRegNo must be 0001(x), 0011(xy), 0111(xyz), or 1111(xyzw) even though
           some sub-channels of them are inactive, which is the requirement of HW design since our
           HW can not support per-channel or channel shift register programming. Besides above basic
           rule, there are different requirements for different shader stages.
           1. VS input and PS output:

              Since such IO is connected to memory of fixed function unit, we can not arbitarily
              allocate HW channel, it must conform to data element layout/format (ioChannelMask
              indicates full/partial of such layout) in memory of FFU unless driver changes data
              element of FFU with new layout/format, but such changing is not necessary. For example,
              if ioChannelMask is 0110(yz) or 0101(xz), HW reg channel mask must be 0111(xyz), i.e,
              must start from x and hole retains, meanwhile, x is mapped to x, y is mapped to y, and
              so on. But for masked out channels in ioChannelMask, hwRegChannel may be garbage.

              If the IO is packed with others, as driver must merge multiple FFU data elements with
              different layout/format to new data element of FFU with new layout/format, IO channel
              can be mapped to HW channel arbitarily depending on where this channel will be merged.
              For example, yz and XZ may be merged to XyZz or XyzZ. Driver needs use ioChannelMask
              and hwRegChannel to analyze and generate such merged memory data element of FFU.

           2. I/O among shaders:
              Whatever packed or not, as the element layout is fully handled by shader internally, IO
              channel can be theoretically mapped to HW channel arbitarily as long as HW register
              footprint are diminished. But current compiler implementation still uses same solution
              as 1 for unpacked case.

              When doing HW linkage, for packed case, in addition to do general check, such as usage
              and usage index, or ioIndex, we also need check whether same channel of same IO is packed
              into same HW channel.
        */
        gctUINT                                 hwRegChannel         : 2;

        /* Reserved bits */
        gctUINT                                 reserved             : 25;
    } s;

    /* Usage of this channel, it will be initialized to SHADER_IO_USAGE_GENERAL at first */
    SHADER_IO_USAGE                             ioUsage;

    /* For DX9, it may be greater than 0, but for otherwise, it is always 0 */
    gctUINT                                     usageIndex;
}
SHADER_IO_CHANNEL_MAPPING;

typedef struct SHADER_IO_REG_MAPPING
{
    SHADER_IO_CHANNEL_MAPPING                   ioChannelMapping[CHANNEL_NUM];

    /* It is the same as index of ioRegMapping which owns it */
    gctUINT                                     ioIndex;

    /* Which channels are active, corresponding to bActiveWithinShader */
    gctUINT                                     ioChannelMask;

    /* First valid channel based on io channel mask */
    gctUINT                                     firstValidIoChannel;

    /* Indicate which IOs are packed with this IO. All IOs that are packed together share
       same hwRegNo, otherwise, each IO must be colored to different hwRegNo */
    gctUINT                                     packedIoIndexMask;

    /* HW reg number this IO reg is colored to */
    gctUINT                                     hwRegNo;
}
SHADER_IO_REG_MAPPING;

typedef struct USAGE_2_IO
{
    /* Which Io index is used by this usage on usageindex 0 */
    gctUINT                                     mainIoIndex;

    /* Which channels are used by this usage on usageindex 0 */
    gctUINT                                     mainIoChannelMask;

    /* First valid channel based on channel mask on usageindex 0 */
    gctUINT                                     mainFirstValidIoChannel;

    /* Masked due to different usageIndex */
    gctUINT                                     ioIndexMask;
}
USAGE_2_IO;

typedef struct SHADER_IO_MAPPING
{
    /* IO regs */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* Number of IO regs allocated for pIoRegMapping. Can not be greater than
       MAX_SHADER_IO_NUM. It must be (maxUsed# + 1). Hole is permitted except
       for OGL(ES) which must squeeze valid IOs together */
    gctUINT                                     countOfIoRegMapping;

    /* Indicate which IO regs are used */
    gctUINT                                     ioIndexMask;

    /* Which IO regs are used by each usage */
    USAGE_2_IO                                  usage2IO[SHADER_IO_USAGE_TOTAL_COUNT];
}
SHADER_IO_MAPPING;

/* Constant mapping table definitions, for c#/i#/b#/cb#/icb#

   They are used to update constant values
*/

typedef struct SHADER_CONSTANT_HW_LOCATION_MAPPING
{
    SHADER_HW_ACCESS_MODE                       hwAccessMode;

    union
    {
        /* Case to map to constant register */
        gctUINT                                 hwRegNo;

        /* Case to map to constant memory

           CAUTION:
           Currently, memory layout is designed to 4-tuples based as constant register,that means
           every element of scalar/vec2~4 array will be put into separated room of 4-tuples.With
           such layout design, we can support both DX1x cb#/icb# and GL named uniform block. But
           GL named uniform block has more loose layout requirement based on spec (for example,
           there could be no padding between any pair of GL type of data, but it is unfriendly for
           DX), so HW may design a different layout requirement for constant buffer, or directly
           use similar ld as GPGPU uses. If so, we should re-design followings and their users.
        */
        struct
        {
            union
            {
                /* For address case, it is relative address */
                gctUINT                         hwConstantArrayBase;

                /* For place-holder # case */
                gctUINT                         hwConstantArraySlot;
            } memBase;

            /* Must be at 4-tuples alignment */
            gctUINT                             offsetInConstantArray;
        } memAddr;
    } hwLoc;

    /* Which channels of this HW location are valid for this 4-tuples constant */
    gctUINT                                     validHWChannelMask;
}
SHADER_CONSTANT_HW_LOCATION_MAPPING;

typedef struct SHADER_CONSTANT_SUB_ARRAY_MAPPING
{
    /* Which constant array owns this sub-array */
    struct SHADER_CONSTANT_ARRAY_MAPPING*       pParentConstantArray;

    /* Start and size of sub array. 'startIdx' is the index within 'arrayRange' of parent */
    gctUINT                                     startIdx;
    gctUINT                                     subArrayRange;

    /* Only used by DX, for DX10+, it is the 2nd dimension of cb/icb, otherwise, it is 1st dimension */
    gctUINT                                     firstMSCSharpRegNo;

    /* Which channels are valid for this 4-tuples constant sub array. Note that mapping from validChannelMask
       to validHWChannelMask is not channel-based, so hole is supported. For example, 0011(xy) may be mapped
       to 0011(xy), 0110(yz) or 1100(zw), but 0101(xz) can only be mapped to 0111(xyz) or 1110(yzw) */
    gctUINT                                     validChannelMask;

    /* HW constant location for first element of this array.
       Note that all elements in each sub-array must have same channel mask designated by validChannelMask
       and validHWChannelMask respectively */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwFirstConstantLocation;
}
SHADER_CONSTANT_SUB_ARRAY_MAPPING;

typedef struct SHADER_CONSTANT_ARRAY_MAPPING
{
    /* It is the same as index of constantArrayMapping which owns it */
    gctUINT                                     constantArrayIndex;

    SHADER_CONSTANT_USAGE                       constantUsage;

    /* Array size, including all sub-arrays it holds */
    gctUINT                                     arrayRange;

    /* For 2 purposes, we need split constant buffer into several subs
       1. Not all constant registers are used in constant buffer or a big uniform for OGL, so each
          used part can be put into a sub
       2. OGL may have many uniforms, each may be put into different sub if they can be put together

       So there is a possibility that several sub constant arrays share same startIdx and subArrayRange
       with different validChannelMask, also there is a possibility that several sub constant arrays
       share same HW reg/mem with different validHWChannelMask */
    SHADER_CONSTANT_SUB_ARRAY_MAPPING*          pSubConstantArrays;
    gctUINT                                     countOfSubConstantArray;
}
SHADER_CONSTANT_ARRAY_MAPPING;

typedef struct SHADER_COMPILE_TIME_CONSTANT
{
    gctUINT                                     constantValue[CHANNEL_NUM];

    /* HW constant location that this CTC maps to. Just use validHWChannelMask to indicate which channels
       have an immedidate CTC value */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwConstantLocation;
}
SHADER_COMPILE_TIME_CONSTANT;

typedef struct SHADER_CONSTANT_MAPPING
{
    /* Constant buffer arrays */
    SHADER_CONSTANT_ARRAY_MAPPING*              pConstantArrayMapping;

    /* Number of constant buffer arrays allocated for pConstantArrayMapping. Can not be greater than
       MAX_SHADER_CONSTANT_ARRAY_NUM. It must be (maxUsed# + 1). Hole is permitted except for OGL(ES)
       which must squeeze valid constant buffer arrays together */
    gctUINT                                     countOfConstantArrayMapping;

    /* Indicate which arrays are used */
    gctUINT                                     arrayIndexMask;

    /* Only used for DX9, no meaning for other clients */
    gctUINT                                     usage2ArrayIndex[SHADER_CONSTANT_USAGE_TOTAL_COUNT];

    /* Compiling-time immediate values */
    SHADER_COMPILE_TIME_CONSTANT*               pCompileTimeConstant;
    gctUINT                                     countOfCompileTimeConstant;
}
SHADER_CONSTANT_MAPPING;

/* Sampler mapping table definitions for s#

   They are used to update sampler state
*/

typedef struct SHADER_SAMPLER_SLOT_MAPPING
{
    /* It is the same as index of sampler which owns it */
    gctUINT                                     samplerSlotIndex;

    /* It does not apply to DX1x, and it will be removed after HW supports separated t# */
    SHADER_RESOURCE_DIMENSION                   samplerDimension;

    /* Only for OGL(ES). Return type by sample/ld inst, and it will be removed after HW
       supports separated t# */
    SHADER_RESOURCE_RETURN_TYPE                 samplerReturnType;

    /* Sampler mode, DX1x only */
    SHADER_SAMPLER_MODE                         samplerMode;

    /* HW slot number */
    gctUINT                                     hwSamplerSlot;
}
SHADER_SAMPLER_SLOT_MAPPING;

typedef struct SHADER_SAMPLER_MAPPING
{
    SHADER_SAMPLER_SLOT_MAPPING*                pSampler;

    /* Number of samplers allocated for pSampler. Can not be greater than
       MAX_SHADER_SAMPLER_NUM. It must be (maxUsed# + 1). Hole is permitted
       except for OGL(ES) which must squeeze valid samplers together */
    gctUINT                                     countOfSamplers;

    /* Indicate which samplers are used */
    gctUINT                                     samplerSlotMask;

    /* It does not apply to DX1x, and it will be removed after HW supports separated t# */
    gctUINT                                     dim2SamplerSlotMask[SHADER_RESOURCE_DIMENSION_TOTAL_COUNT];
}
SHADER_SAMPLER_MAPPING;

/* Shader resource mapping table definitions for t#

   They are used to update shader resource state
*/

typedef struct SHADER_RESOURCE_SLOT_MAPPING
{
    /* It is the same as index of resource which owns it */
    gctUINT                                     resourceSlotIndex;

    SHADER_RESOURCE_ACCESS_MODE                 accessMode;

    union
    {
        /* For type of access mode */
        struct
        {
            /* It only applies to DX1x */
            SHADER_RESOURCE_DIMENSION           resourceDimension;

            /* Resource return type by sample/ld inst */
            SHADER_RESOURCE_RETURN_TYPE         resourceReturnType;
        } s;

        /* For structured of access mode */
        gctUINT                                 structureSize;
    } u;

    /* HW slot number */
    gctUINT                                     hwResourceSlot;
}
SHADER_RESOURCE_SLOT_MAPPING;

typedef struct SHADER_RESOURCE_MAPPING
{
    SHADER_RESOURCE_SLOT_MAPPING*               pResource;

    /* Number of resources allocated for pResource. Can not be greater than
       MAX_SHADER_RESOURCE_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfResources;

    /* Indicate which resources are used */
    gctUINT                                     resourceSlotMask[4];

    /* It only applies to DX1x for typed access mode */
    gctUINT                                     dim2ResourceSlotMask[SHADER_RESOURCE_DIMENSION_TOTAL_COUNT][4];
}
SHADER_RESOURCE_MAPPING;

/* Global memory mapping table definitions for u#

   They are used to update global memory state, such as UAV
*/

typedef struct SHADER_UAV_SLOT_MAPPING
{
    /* It is the same as index of UAV which owns it */
    gctUINT                                     uavSlotIndex;

    SHADER_UAV_ACCESS_MODE                      accessMode;

    union
    {
        /* For type of access mode */
        struct
        {
            /* It only applies to DX1x */
            SHADER_UAV_DIMENSION                uavDimension;

            /* UAV return type by ld/store inst */
            SHADER_UAV_TYPE                     uavType;
        } s;

        /* For structured of access mode */
        gctUINT                                 structureSize;
    } u;

    union
    {
        /* For address case, it is relative address */
        gctUINT                                 hwAddressBase;

        /* For place-holder # case */
        gctUINT                                 hwUavSlot;
    } hwLoc;
}
SHADER_UAV_SLOT_MAPPING;

typedef struct SHADER_UAV_MAPPING
{
    SHADER_UAV_SLOT_MAPPING*                    pUAV;

    /* Number of UAVs allocated for pUAV. Can not be greater than
       MAX_SHADER_UAV_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfUAVs;

    /* Indicate which UAVs are used */
    gctUINT                                     uavSlotMask;

    /* It only applies to DX1x for typed access mode */
    gctUINT                                     dim2UavSlotMask[SHADER_UAV_DIMENSION_TOTAL_COUNT];
}
SHADER_UAV_MAPPING;

/* Common defintion for shader patch.

   Not only for static compiling, but also for draw time recompiling
*/

typedef struct SHADER_PATCH_COMMON_ENTRY
{
    /* These are similiar with identical members of SHADER_PATCH_CONSTANT_ENTRY */
    gctUINT                                     patchFlag;
    gctUINT                                     patchFlagIndex;
    void*                                       pPrivateData;  /*IT MUST BE PUT AT LAST!!! */
}
SHADER_PATCH_COMMON_ENTRY;

typedef struct SHADER_PATCH_COMMON
{
    SHADER_PATCH_COMMON_ENTRY*                  pPatchCommonEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PATCH_COMMON;

/* Constants defintion for shader patch.

   Not only for static compiling, but also for draw time recompiling
*/

typedef struct SHADER_PATCH_CONSTANT_AS_INST_IMM
{
    gctUINT                                     patchedPC;
    gctUINT                                     srcNo;
}
SHADER_PATCH_CONSTANT_AS_INST_IMM;

typedef struct SHADER_PATCH_CONSTANT_ENTRY
{
    /* Each patch flag needs determine which constant value is put to which channel of which
       HW location by itself. */
    gctUINT                                     patchFlag;

    /* For some flags, there may have multiple different patch constants, so we need an index
       to differiate them. It is numbered from zero. */
    gctUINT                                     patchFlagIndex;

    /* To select method to set constant after patching */
    SHADER_PATCH_CONSTANT_MODE                  mode;

    union
    {
        SHADER_CONSTANT_SUB_ARRAY_MAPPING*      pSubCBMapping; /* SHADER_PATCH_CONSTANT_MODE_VAL_2_MEMORREG */
        SHADER_COMPILE_TIME_CONSTANT            ctcConstant;   /* SHADER_PATCH_CONSTANT_MODE_CTC */
        SHADER_PATCH_CONSTANT_AS_INST_IMM       instImm;       /* SHADER_PATCH_CONSTANT_MODE_VAL_2_INST */
    } u;

    /* For some flags, such as SHADER_RECOMPILE_CONSTANT_TEXCOORD, they will have their private
       data to tell driver how to fetch/generate constants. NOTE IT MUST BE PUT AT LAST!!! */
    void*                                       pPrivateData;
}
SHADER_PATCH_CONSTANT_ENTRY;

typedef struct SHADER_PATCH_CONSTANT
{
    SHADER_PATCH_CONSTANT_ENTRY*                pPatchConstantEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PATCH_CONSTANT;

/* Shader static patch stream out private structure definition for SSP_COMMON_FLAG_STREAMOUT_BY_STORE */
typedef struct SSP_STREAMOUT_BY_LSTORE_PRIV
{
    /* patchFlagIndex of SHADER_PATCH_COMMON_ENTRY indicates stream buffer index which can not be greater
       than MAX_SHADER_STREAM_OUT_BUFFER_NUM */
    SHADER_UAV_SLOT_MAPPING*                    pSOBuffer;
}
SSP_STREAMOUT_BY_LSTORE_PRIV;

struct SHADER_EXECUTABLE_INSTANCE;

/* Executable shader profile definition. Each BE compiling or glProgramBinary will generate one
   profile like this. */
typedef struct SHADER_EXECUTABLE_PROFILE
{
    /* Profile version */
    gctUINT                                     profileVersion;

    /* Target HW this executable can run on */
    gctUINT                                     chipModel;
    gctUINT                                     chipRevision;

    /* From MSB to LSB, 8-bits client + 8-bits type + 8-bits majorVersion + 8-bits minorVersion */
    gctUINT                                     shVersionType;

    /* Compiled machine code. Note that current sizeOfMachineCode must be 4-times of DWORD since
       HW inst is 128-bits wide. It may be changed later for future chip */
    gctUINT*                                    pMachineCode;
    gctUINT                                     sizeOfMachineCode;

    /* EndPC of main routine since HW will use it to terminate shader execution. The whole machine
       code is organized as main routine followed by all sub-routines */
    gctUINT                                     endPCOfMainRoutine;

    /* Temp register count the machine code uses */
    gctUINT                                     gprCount;

    /* Low level mapping tables from # to HW resource. Add new mapping tables here, for example,
       function table for DX11+ */
    SHADER_IO_MAPPING                           inputMapping;
    SHADER_IO_MAPPING                           outputMapping;
    SHADER_CONSTANT_MAPPING                     constantMapping;
    SHADER_SAMPLER_MAPPING                      samplerMapping;
    SHADER_RESOURCE_MAPPING                     resourceMapping;
    SHADER_UAV_MAPPING                          uavMapping;

    /* New added constants which need driver set values for from state side after statically compiling,
       such as transform feedback states, which will be as constants when SO is handled by ST. Note for
       static patch constant, staticPatchConstants.mode can not be SHADER_PATCH_CONSTANT_MODE_CTC. */
    SHADER_PATCH_CONSTANT                       staticPatchConstants;

    /* New added global memory that needs driver additionally allocate or set after statically compiling.
       All patches using extra global memory must use LD or ST instructions. For example, register spill
       needs driver allocate extra memory to put spilled data, or SO hanled by ST need set SO buffers to
       */
    SHADER_PATCH_COMMON                         staticPatchExtraMems;

    /* TODO: LTC-folding gcSL insts and constants here.
       1. Do we need convert constants used by LTC-folding gcSL insts to # based?? If so, UpdateUniform can
          save constant values in # while set data to HW locations, and then use them when do LTC calculation.
          But if static patch constant is not used in machine code, but for LTC, then we may need a new mode
          in SHADER_PATCH_CONSTANT_MODE to handle it, such as SHADER_PATCH_CONSTANT_MODE_LTC_ONLY.
       2. Can we make LTC calculation only based on dirty #?? Or
       3. Can we directly translate LTC-folding gcSL insts to CPU code??
    */

    /* Current SEI that this profile uses. This is the one used to program HW registers. Currently disable it
       due to we're using program-level recompiling */
    /*SHADER_EXECUTABLE_INSTANCE*                 pCurInstance;*/
}
SHADER_EXECUTABLE_PROFILE;

END_EXTERN_C();

#endif /* __gc_vsc_drvi_shader_profile_h_ */


