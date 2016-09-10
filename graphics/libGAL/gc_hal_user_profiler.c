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


/*******************************************************************************
**    Profiler for Vivante HAL.
*/
#include "gc_hal_user_precomp.h"

#define _GC_OBJ_ZONE            gcvZONE_HAL

#if gcdENABLE_3D
#if VIVANTE_PROFILER

#ifdef ANDROID
#if gcdNEW_PROFILER_FILE
#define DEFAULT_PROFILE_FILE_NAME   "/sdcard/vprofiler.vpd"
#else
#define DEFAULT_PROFILE_FILE_NAME   "/sdcard/vprofiler.xml"
#endif
#else
#if gcdNEW_PROFILER_FILE
#define DEFAULT_PROFILE_FILE_NAME   "vprofiler.vpd"
#else
#define DEFAULT_PROFILE_FILE_NAME   "vprofiler.xml"
#endif
#endif

#if gcdNEW_PROFILER_FILE
#if VIVANTE_PROFILER_CONTEXT
#define _HAL Hal
#else
#define _HAL gcPLS.hal
#endif

#if VIVANTE_PROFILER_CONTEXT
typedef struct _glhal_map
{
    gctUINT hardwareContext;
    gcoHAL  profilerHal;
    struct _glhal_map * next;
}glhal_map;
glhal_map* halprofilermap = gcvNULL;
static gcoHAL glhal_map_current()
{
    gceSTATUS status;
    gcoHARDWARE hardware = gcvNULL;
    gctUINT32 context;
    glhal_map* p = halprofilermap;
    if(p == gcvNULL) return gcvNULL;
    gcmGETHARDWARE(hardware);
    if (hardware)
    {
        gcmVERIFY_OK(gcoHARDWARE_GetContext(hardware, &context));
    }
    else
    {
        return gcvNULL;
    }
    while(p != gcvNULL)
    {
        if(p->hardwareContext == context) return p->profilerHal;
        p=p->next;
    }
OnError:
    return gcvNULL;
}
static void glhal_map_create(gcoHAL Hal)
{
    gceSTATUS status;
    gcoHARDWARE hardware = gcvNULL;
    gctUINT32 context;
    glhal_map* p = halprofilermap;
    gctPOINTER    pointer;

    gcmGETHARDWARE(hardware);
    if (hardware)
    {
        gcmVERIFY_OK(gcoHARDWARE_GetContext(hardware, &context));
    }
    else
    {
        gcmASSERT(-1);
        return;
    }

    gcmONERROR( gcoOS_Allocate(gcvNULL,
        gcmSIZEOF(struct _glhal_map),
        &pointer));
    p = (glhal_map*) pointer;
    p->hardwareContext = context;
    p->profilerHal = _HAL;
    p->next = gcvNULL;

    p = halprofilermap;

    if(p == gcvNULL) halprofilermap = (glhal_map*) pointer;
    else
    {
        while(p->next != gcvNULL)
        {
            p=p->next;
        }
        p->next = pointer;

    }
    return;
OnError:
    gcmASSERT(-1);
    return;
}
static void glhal_map_delete(gcoHAL Hal)
{
    glhal_map* p = halprofilermap;

    if(p->profilerHal == _HAL)
    {
        halprofilermap = halprofilermap->next;
    }
    else
    {
        glhal_map* pre = p;
        p = p->next;
        while(p != gcvNULL)
        {
            if(p->profilerHal == _HAL)
            {
                pre->next = p->next;
                break;
            }
            pre = p;
            p=p->next;
        }
        if(p == gcvNULL)
        {
            /*Must match*/
            gcmASSERT(-1);
            return;
        }
    }
    gcoOS_Free(gcvNULL, p);

}
#else
static void glhal_map_create(gcoHAL Hal){}
static void glhal_map_delete(gcoHAL Hal){}
#endif


/* Write a data value. */
#define gcmWRITE_VALUE(IntData) \
    do \
{ \
    gceSTATUS status; \
    gctINT32 value = IntData; \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, gcmSIZEOF(value), &value)); \
} \
    while (gcvFALSE)

#define gcmWRITE_CONST(Const) \
    do \
{ \
    gceSTATUS status; \
    gctINT32 data = Const; \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, gcmSIZEOF(data), &data)); \
} \
    while (gcvFALSE)

#define gcmWRITE_COUNTER(Counter, Value) \
    gcmWRITE_CONST(Counter); \
    gcmWRITE_VALUE(Value)

/* Write a string value (char*). */
#define gcmWRITE_STRING(String) \
    do \
{ \
    gceSTATUS status; \
    gctSIZE_T length; \
    length = gcoOS_StrLen((gctSTRING)String, gcvNULL); \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, gcmSIZEOF(length), &length)); \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, length, String)); \
} \
    while (gcvFALSE)

#define gcmWRITE_BUFFER(Size, Buffer) \
    do \
{ \
    gceSTATUS status; \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, Size, Buffer)); \
} \
    while (gcvFALSE)

#else

#define gcmWRITE_STRING(String) \
    do \
    { \
    gceSTATUS status; \
    gctSIZE_T length; \
    length = gcoOS_StrLen((gctSTRING) String, gcvNULL); \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, length, String)); \
    } \
    while (gcvFALSE)

#define gcmWRITE_BUFFER(Buffer, ByteCount) \
    do \
    { \
    gceSTATUS status; \
    gcmERR_BREAK(gcoPROFILER_Write(_HAL, ByteCount, String)); \
    } \
    while (gcvFALSE)

#define gcmPRINT_XML_COUNTER(Counter) \
    do \
    { \
    char buffer[256]; \
    gctUINT offset = 0; \
    gceSTATUS status; \
    gcmERR_BREAK(gcoOS_PrintStrSafe(buffer, gcmSIZEOF(buffer), \
    &offset, \
    "<%s value=\"%d\"/>\n", \
# Counter, \
    _HAL->profiler.Counter)); \
    gcmWRITE_STRING(buffer); \
    } \
    while (gcvFALSE)

#define gcmPRINT_XML(Format, Value) \
    do \
    { \
    char buffer[256]; \
    gctUINT offset = 0; \
    gceSTATUS status; \
    gcmERR_BREAK(gcoOS_PrintStrSafe(buffer, gcmSIZEOF(buffer), \
    &offset, \
    Format, \
    Value)); \
    gcmWRITE_STRING(buffer); \
    } \
    while (gcvFALSE)
#endif

gceSTATUS
gcoPROFILER_Initialize(
    IN gcoHAL Hal,
    IN gctBOOL Enable
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    char profilerName[256] = {'\0'};
    char inputFileName[256] = {'\0'};
    char* fileName = gcvNULL;
    char* filter = gcvNULL;
    char* env = gcvNULL;
    gcoHARDWARE hardware = gcvNULL;
    gcsHAL_INTERFACE iface;
#ifdef ANDROID
    gctBOOL matchResult = gcvFALSE;
#endif
    gcmHEADER();
#if VIVANTE_PROFILER_PM
    if (Enable)
    {
        gcoHAL_ConfigPowerManagement(gcvFALSE);
    }
    else
    {
        gcoHAL_ConfigPowerManagement(gcvTRUE);
        /* disable profiler in kernel. */
        iface.command = gcvHAL_SET_PROFILE_SETTING;
        iface.u.SetProfileSetting.enable = gcvFALSE;

        /* Call the kernel. */
        status = gcoOS_DeviceControl(gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface));

        gcmFOOTER();
        return status;
    }
#endif
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    glhal_map_create(_HAL);
    gcmGETHARDWARE(hardware);

#ifdef ANDROID
    gcoOS_GetEnv(gcvNULL, "VP_PROCESS_NAME", &env);
    if ((env != gcvNULL) && (env[0] !=0)) matchResult = (gcoOS_DetectProcessByName(env) ? gcvTRUE : gcvFALSE);
    if(matchResult != gcvTRUE) {
        return gcvSTATUS_MISMATCH;
    }
    env= gcvNULL;
#endif

    gcoOS_ZeroMemory(&_HAL->profiler, gcmSIZEOF(_HAL->profiler));

    gcoOS_GetEnv(gcvNULL,
        "VP_COUNTER_FILTER",
        &filter);

    /* Enable/Disable specific counters. */
    if ((filter != gcvNULL))
    {
        gctUINT bitsLen = (gctUINT) gcoOS_StrLen(filter, gcvNULL);
        if (bitsLen > 2)
        {
            _HAL->profiler.enableHal = (filter[2] == '1');
        }
        else
        {
            _HAL->profiler.enableHal = gcvTRUE;
        }

        if (bitsLen > 3)
        {
            _HAL->profiler.enableHW = (filter[3] == '1');
        }
        else
        {
            _HAL->profiler.enableHW = gcvTRUE;
        }

        if (bitsLen > 8)
        {
            _HAL->profiler.enableSH = (filter[8] == '1');
        }
        else
        {
            _HAL->profiler.enableSH = gcvTRUE;
        }
    }
    else
    {
        _HAL->profiler.enableHal = gcvTRUE;
        _HAL->profiler.enableHW = gcvTRUE;
        _HAL->profiler.enableSH = gcvTRUE;
    }

    gcoOS_GetEnv(gcvNULL,
        "VP_OUTPUT",
        &fileName);
    if(fileName) gcoOS_StrCatSafe(inputFileName,256,fileName);

    _HAL->profiler.useSocket = gcvFALSE;
    _HAL->profiler.disableOutputCounter = gcvFALSE;
    if (fileName && *fileName != '\0' && *fileName != ' ')
    {
    }
    else
    {
        fileName = DEFAULT_PROFILE_FILE_NAME;
        if(fileName) gcoOS_StrCatSafe(inputFileName,256,fileName);
    }
#if VIVANTE_PROFILER_CONTEXT
    {
        /*generate file name for each context*/
        gctHANDLE pid = gcoOS_GetCurrentProcessID();
        static gctUINT8 num = 1;
        gctUINT offset = 0;
        char* pos = gcvNULL;

        gcoOS_StrStr (inputFileName, ".vpd",&pos);
        if(pos) pos[0] = '\0';
        gcoOS_PrintStrSafe(profilerName, gcmSIZEOF(profilerName), &offset, "%s_%d_%d.vpd",inputFileName,(gctUINTPTR_T)pid, num);

        num++;
    }
#endif
    if (! _HAL->profiler.useSocket)
    {
        status = gcoOS_Open(gcvNULL,
            profilerName,
#if gcdNEW_PROFILER_FILE
            gcvFILE_CREATE,
#else
            gcvFILE_CREATETEXT,
#endif
            &_HAL->profiler.file);
    }

    if (gcmIS_ERROR(status))
    {
        _HAL->profiler.enable = 0;
        status = gcvSTATUS_GENERIC_IO;

        gcmFOOTER();
        return status;
    }

    /* enable profiler in kernel. */
    iface.command = gcvHAL_SET_PROFILE_SETTING;
    iface.u.SetProfileSetting.enable = gcvTRUE;

    /* Call the kernel. */
    status = gcoOS_DeviceControl(gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        &iface, gcmSIZEOF(iface),
        &iface, gcmSIZEOF(iface));

    if (gcmIS_ERROR(status))
    {
        _HAL->profiler.enable = 0;

        gcmFOOTER();
        return status;
    }

#if VIVANTE_PROFILER_NEW
    if (hardware)
    {
        int bufId = 0;
        gcoHARDWARE_EnableCounters(hardware);
        for (; bufId < NumOfDrawBuf;bufId++)
        {
            gcoBUFOBJ_Construct(_HAL, gcvBUFOBJ_TYPE_GENERIC_BUFFER, &_HAL->profiler.newCounterBuf[bufId]);
            gcoBUFOBJ_Upload(_HAL->profiler.newCounterBuf[bufId], gcvNULL, 0, 64*sizeof(gctUINT32), gcvBUFOBJ_USAGE_STATIC_DRAW);
        }
        _HAL->profiler.curBufId = 0;
    }
#endif

    _HAL->profiler.isSyncMode = gcvTRUE;
    gcoOS_GetEnv(gcvNULL,
        "VP_SYNC_MODE",
        &env);

    if ((env != gcvNULL) && gcmIS_SUCCESS(gcoOS_StrCmp(env, "0")))
    {
        _HAL->profiler.isSyncMode = gcvFALSE;
    }

    _HAL->profiler.enable = 1;
    gcoOS_GetTime(&_HAL->profiler.frameStart);
    _HAL->profiler.frameStartTimeusec = _HAL->profiler.frameStart;
    _HAL->profiler.prevVSInstCount = 0;
    _HAL->profiler.prevVSBranchInstCount = 0;
    _HAL->profiler.prevVSTexInstCount = 0;
    _HAL->profiler.prevVSVertexCount = 0;
    _HAL->profiler.prevPSInstCount = 0;
    _HAL->profiler.prevPSBranchInstCount = 0;
    _HAL->profiler.prevPSTexInstCount = 0;
    _HAL->profiler.prevPSPixelCount = 0;

#if gcdNEW_PROFILER_FILE
    gcmWRITE_CONST(VPHEADER);
    gcmWRITE_BUFFER(4, "VP12");
#else
    gcmWRITE_STRING("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<VProfile>\n");
#endif

OnError:
    /* Success. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoPROFILER_Destroy(
    IN gcoHAL Hal
    )
{
    gcmHEADER();
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
#if gcdNEW_PROFILER_FILE
    gcmWRITE_CONST(VPG_END);
#else
    gcmWRITE_STRING("</VProfile>\n");
#endif

    gcoPROFILER_Flush(gcvNULL);
    if (_HAL->profiler.enable )
    {
        {
            /* Close the profiler file. */
            gcmVERIFY_OK(gcoOS_Close(gcvNULL, _HAL->profiler.file));
        }
    }
    glhal_map_delete(_HAL);
#if VIVANTE_PROFILER_NEW
    {
        gctUINT32 i = 0;
        for (i = 0;  i < NumOfDrawBuf; i++)
            gcoBUFOBJ_Destroy(_HAL->profiler.newCounterBuf[i]);
    }
#endif

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/* Write data to profile. */
gceSTATUS
gcoPROFILER_Write(
    IN gcoHAL Hal,
    IN gctSIZE_T ByteCount,
    IN gctCONST_POINTER Data
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("ByteCount=%lu Data=0x%x", ByteCount, Data);
    if(!_HAL)
    {
        gcmFOOTER();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    /* Check if already destroyed. */
       if (_HAL->profiler.enable)
    {
        {
            status = gcoOS_Write(gcvNULL,
                _HAL->profiler.file,
                ByteCount, Data);
        }
    }

    gcmFOOTER();
    return status;
}

/* Flush data out. */
gceSTATUS
gcoPROFILER_Flush(
    IN gcoHAL Hal
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER();
    if(!_HAL)
    {
        gcmFOOTER();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (_HAL->profiler.enable)
    {
        {
            status = gcoOS_Flush(gcvNULL,
                _HAL->profiler.file);
        }
    }

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoPROFILER_Count(
    IN gcoHAL Hal,
    IN gctUINT32 Enum,
    IN gctINT Value
    )
{
#if PROFILE_HAL_COUNTERS
    gcmHEADER_ARG("Enum=%lu Value=%d", Enum, Value);
    #if VIVANTE_PROFILER_CONTEXT
    if(!_HAL) _HAL = glhal_map_current();
    #endif
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (_HAL->profiler.enable)
    {
        switch (Enum)
        {
        case GLINDEX_OBJECT:
            _HAL->profiler.indexBufferNewObjectsAlloc   += Value;
            _HAL->profiler.indexBufferTotalObjectsAlloc += Value;
            break;

        case GLINDEX_OBJECT_BYTES:
            _HAL->profiler.indexBufferNewBytesAlloc   += Value;
            _HAL->profiler.indexBufferTotalBytesAlloc += Value;
            break;

        case GLVERTEX_OBJECT:
            _HAL->profiler.vertexBufferNewObjectsAlloc   += Value;
            _HAL->profiler.vertexBufferTotalObjectsAlloc += Value;
            break;

        case GLVERTEX_OBJECT_BYTES:
            _HAL->profiler.vertexBufferNewBytesAlloc   += Value;
            _HAL->profiler.vertexBufferTotalBytesAlloc += Value;
            break;

        case GLTEXTURE_OBJECT:
            _HAL->profiler.textureBufferNewObjectsAlloc   += Value;
            _HAL->profiler.textureBufferTotalObjectsAlloc += Value;
            break;

        case GLTEXTURE_OBJECT_BYTES:
            _HAL->profiler.textureBufferNewBytesAlloc   += Value;
            _HAL->profiler.textureBufferTotalBytesAlloc += Value;
            break;

        default:
            break;
        }
    }
#endif

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

#if gcdENABLE_3D
#if !gcdNEW_PROFILER_FILE
gceSTATUS
gcoPROFILER_Shader(
                   IN gcoHAL Hal,
                   IN gcSHADER Shader
                   )
{
    /*#if PROFILE_SHADER_COUNTERS*/

    gcmHEADER_ARG("Shader=0x%x", Shader);
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (_HAL->profiler.enableSH)
    {
        gctUINT16 alu = 0, tex = 0, i;

        if (_HAL->profiler.enable)
        {
            /* Profile shader */
            for (i = 0; i < Shader->codeCount; i++ )
            {
                switch (gcmSL_OPCODE_GET(Shader->code[i].opcode, Opcode))
                {
                case gcSL_NOP:
                    break;

                case gcSL_TEXLD:
                    tex++;
                    break;

                default:
                    alu++;
                    break;
                }
            }

            gcmPRINT_XML("<InstructionCount value=\"%d\"/>\n", tex + alu);
            gcmPRINT_XML("<ALUInstructionCount value=\"%d\"/>\n", alu);
            gcmPRINT_XML("<TextureInstructionCount value=\"%d\"/>\n", tex);
            gcmPRINT_XML("<Attributes value=\"%lu\"/>\n", Shader->attributeCount);
            gcmPRINT_XML("<Uniforms value=\"%lu\"/>\n", Shader->uniformCount);
            gcmPRINT_XML("<Functions value=\"%lu\"/>\n", Shader->functionCount);
        }
        /*#endif*/
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}
#endif

gceSTATUS
gcoPROFILER_ShaderVS(
    IN gcoHAL Hal,
    IN gctPOINTER Vs
    )
{
    /*#if PROFILE_SHADER_COUNTERS*/
    gcmHEADER_ARG("Vs=0x%x", Vs);
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (_HAL->profiler.enable)
    {
        if (_HAL->profiler.enableSH)
        {
#if gcdNEW_PROFILER_FILE
            gctUINT16 alu = 0, tex = 0, i;
            gcSHADER Shader = (gcSHADER)Vs;

            /* Profile shader */
            for (i = 0; i < Shader->codeCount; i++ )
            {
                switch (gcmSL_OPCODE_GET(Shader->code[i].opcode, Opcode))
                {
                case gcSL_NOP:
                    break;

                case gcSL_TEXLD:
                    tex++;
                    break;

                default:
                    alu++;
                    break;
                }
            }

            gcmWRITE_CONST(VPG_PVS);

            gcmWRITE_COUNTER(VPC_PVSINSTRCOUNT, (tex + alu));
            gcmWRITE_COUNTER(VPC_PVSALUINSTRCOUNT, alu);
            gcmWRITE_COUNTER(VPC_PVSTEXINSTRCOUNT, tex);
            gcmWRITE_COUNTER(VPC_PVSATTRIBCOUNT, (Shader->attributeCount));
            gcmWRITE_COUNTER(VPC_PVSUNIFORMCOUNT, (Shader->uniformCount));
            gcmWRITE_COUNTER(VPC_PVSFUNCTIONCOUNT, (Shader->functionCount));
            if (Shader->source)
            {
                gcmWRITE_CONST(VPC_PVSSOURCE);
                gcmWRITE_STRING(Shader->source);
            }
            gcmWRITE_CONST(VPG_END);
#else
            gcmWRITE_STRING("<VS>\n");
            gcoPROFILER_Shader(gcvNULL, (gcSHADER) Vs);
            gcmWRITE_STRING("</VS>\n");
#endif
        }
    }
    /*#endif*/

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gcoPROFILER_ShaderFS(
    IN gcoHAL Hal,
    IN void* Fs
    )
{
    /*#if PROFILE_SHADER_COUNTERS*/
    gcmHEADER_ARG("Fs=0x%x", Fs);
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (_HAL->profiler.enable)
    {
        if (_HAL->profiler.enableSH)
        {
#if gcdNEW_PROFILER_FILE
            gctUINT16 alu = 0, tex = 0, i;
            gcSHADER Shader = (gcSHADER)Fs;

            /* Profile shader */
            for (i = 0; i < Shader->codeCount; i++ )
            {
                switch (gcmSL_OPCODE_GET(Shader->code[i].opcode, Opcode))
                {
                case gcSL_NOP:
                    break;

                case gcSL_TEXLD:
                    tex++;
                    break;

                default:
                    alu++;
                    break;
                }
            }

            gcmWRITE_CONST(VPG_PPS);
            gcmWRITE_COUNTER(VPC_PPSINSTRCOUNT, (tex + alu));
            gcmWRITE_COUNTER(VPC_PPSALUINSTRCOUNT, alu);
            gcmWRITE_COUNTER(VPC_PPSTEXINSTRCOUNT, tex);
            gcmWRITE_COUNTER(VPC_PPSATTRIBCOUNT, (Shader->attributeCount));
            gcmWRITE_COUNTER(VPC_PPSUNIFORMCOUNT, (Shader->uniformCount));
            gcmWRITE_COUNTER(VPC_PPSFUNCTIONCOUNT, (Shader->functionCount));
            if (Shader->source)
            {
                gcmWRITE_CONST(VPC_PPSSOURCE);
                gcmWRITE_STRING(Shader->source);
            }
            gcmWRITE_CONST(VPG_END);
#else
            gcmWRITE_STRING("<FS>\n");
            gcoPROFILER_Shader(gcvNULL, (gcSHADER) Fs);
            gcmWRITE_STRING("</FS>\n");
#endif
        }
    }
    /*#endif*/

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

#endif /* vivante_no_3d */
gceSTATUS
gcoPROFILER_EndFrame(
                     IN gcoHAL Hal
                     )
{
    /*#if    (PROFILE_HAL_COUNTERS || PROFILE_HW_COUNTERS)*/
    gcsHAL_INTERFACE iface;
    gceSTATUS status;
    /*#endif*/
#if VIVANTE_PROFILER_CONTEXT
    gcoHARDWARE hardware = gcvNULL;
    gctUINT32 context;
#endif

    gcmHEADER();
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }
    if (!_HAL->profiler.enable)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /*#if PROFILE_HAL_COUNTERS*/
    if (_HAL->profiler.enableHal)
    {
        if (!_HAL->profiler.disableOutputCounter)
        {
#if gcdNEW_PROFILER_FILE
            gcmWRITE_CONST(VPG_HAL);

            gcmWRITE_COUNTER(VPC_HALVERTBUFNEWBYTEALLOC, _HAL->profiler.vertexBufferNewBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALVERTBUFTOTALBYTEALLOC, _HAL->profiler.vertexBufferTotalBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALVERTBUFNEWOBJALLOC, _HAL->profiler.vertexBufferNewObjectsAlloc);
            gcmWRITE_COUNTER(VPC_HALVERTBUFTOTALOBJALLOC, _HAL->profiler.vertexBufferTotalObjectsAlloc);

            gcmWRITE_COUNTER(VPC_HALINDBUFNEWBYTEALLOC, _HAL->profiler.indexBufferNewBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALINDBUFTOTALBYTEALLOC, _HAL->profiler.indexBufferTotalBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALINDBUFNEWOBJALLOC, _HAL->profiler.indexBufferNewObjectsAlloc);
            gcmWRITE_COUNTER(VPC_HALINDBUFTOTALOBJALLOC, _HAL->profiler.indexBufferTotalObjectsAlloc);

            gcmWRITE_COUNTER(VPC_HALTEXBUFNEWBYTEALLOC, _HAL->profiler.textureBufferNewBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALTEXBUFTOTALBYTEALLOC, _HAL->profiler.textureBufferTotalBytesAlloc);
            gcmWRITE_COUNTER(VPC_HALTEXBUFNEWOBJALLOC, _HAL->profiler.textureBufferNewObjectsAlloc);
            gcmWRITE_COUNTER(VPC_HALTEXBUFTOTALOBJALLOC, _HAL->profiler.textureBufferTotalObjectsAlloc);

            gcmWRITE_CONST(VPG_END);

#else
            gcmWRITE_STRING("<HALCounters>\n");

            gcmPRINT_XML_COUNTER(vertexBufferNewBytesAlloc);
            gcmPRINT_XML_COUNTER(vertexBufferTotalBytesAlloc);
            gcmPRINT_XML_COUNTER(vertexBufferNewObjectsAlloc);
            gcmPRINT_XML_COUNTER(vertexBufferTotalObjectsAlloc);

            gcmPRINT_XML_COUNTER(indexBufferNewBytesAlloc);
            gcmPRINT_XML_COUNTER(indexBufferTotalBytesAlloc);
            gcmPRINT_XML_COUNTER(indexBufferNewObjectsAlloc);
            gcmPRINT_XML_COUNTER(indexBufferTotalObjectsAlloc);

            gcmPRINT_XML_COUNTER(textureBufferNewBytesAlloc);
            gcmPRINT_XML_COUNTER(textureBufferTotalBytesAlloc);
            gcmPRINT_XML_COUNTER(textureBufferNewObjectsAlloc);
            gcmPRINT_XML_COUNTER(textureBufferTotalObjectsAlloc);

            gcmWRITE_STRING("</HALCounters>\n");
#endif
        }

        /* Reset per-frame counters. */
        _HAL->profiler.vertexBufferNewBytesAlloc   = 0;
        _HAL->profiler.vertexBufferNewObjectsAlloc = 0;

        _HAL->profiler.indexBufferNewBytesAlloc   = 0;
        _HAL->profiler.indexBufferNewObjectsAlloc = 0;

        _HAL->profiler.textureBufferNewBytesAlloc   = 0;
        _HAL->profiler.textureBufferNewObjectsAlloc = 0;
    }
    /*#endif*/

    /*#if PROFILE_HW_COUNTERS*/
    /* gcvHAL_READ_ALL_PROFILE_REGISTERS. */
    if (_HAL->profiler.enableHW)
    {
#if VIVANTE_PROFILER_PERDRAW
        /* TODO */
            /* Set Register clear Flag. */
            iface.command = gcvHAL_READ_PROFILER_REGISTER_SETTING;
            iface.u.SetProfilerRegisterClear.bclear = gcvTRUE;

            /* Call the kernel. */
            status = gcoOS_DeviceControl(gcvNULL,
                                       IOCTL_GCHAL_INTERFACE,
                                       &iface, gcmSIZEOF(iface),
                                       &iface, gcmSIZEOF(iface));
            /* Verify result. */
            if (gcmIS_ERROR(status))
            {
                gcmFOOTER_NO();
                return gcvSTATUS_GENERIC_IO;
            }
#endif

        iface.command = gcvHAL_READ_ALL_PROFILE_REGISTERS;
#if VIVANTE_PROFILER_CONTEXT
        gcmGETHARDWARE(hardware);
        if (hardware)
        {
            gcmVERIFY_OK(gcoHARDWARE_GetContext(hardware, &context));
            if (context != 0)
                iface.u.RegisterProfileData.context = context;
        }
#endif
        /* Call the kernel. */
        status = gcoOS_DeviceControl(gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            &iface, gcmSIZEOF(iface),
            &iface, gcmSIZEOF(iface));

        /* Verify result. */
        if (gcmNO_ERROR(status) && !_HAL->profiler.disableOutputCounter)
        {
#define gcmCOUNTER(name)    iface.u.RegisterProfileData.counters.name

#if gcdNEW_PROFILER_FILE
            gcmWRITE_CONST(VPG_HW);
            gcmWRITE_CONST(VPG_GPU);
            gcmWRITE_COUNTER(VPC_GPUREAD64BYTE, gcmCOUNTER(gpuTotalRead64BytesPerFrame));
            gcmWRITE_COUNTER(VPC_GPUWRITE64BYTE, gcmCOUNTER(gpuTotalWrite64BytesPerFrame));
            gcmWRITE_COUNTER(VPC_GPUCYCLES, gcmCOUNTER(gpuCyclesCounter));
            gcmWRITE_COUNTER(VPC_GPUTOTALCYCLES, gcmCOUNTER(gpuTotalCyclesCounter));
            gcmWRITE_COUNTER(VPC_GPUIDLECYCLES, gcmCOUNTER(gpuIdleCyclesCounter));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_VS);
            gcmWRITE_COUNTER(VPC_VSINSTCOUNT, gcmCOUNTER(vs_inst_counter));
            gcmWRITE_COUNTER(VPC_VSBRANCHINSTCOUNT, gcmCOUNTER(vtx_branch_inst_counter));
            gcmWRITE_COUNTER(VPC_VSTEXLDINSTCOUNT, gcmCOUNTER(vtx_texld_inst_counter));
            gcmWRITE_COUNTER(VPC_VSRENDEREDVERTCOUNT, gcmCOUNTER(rendered_vertice_counter));

            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_PA);
            gcmWRITE_COUNTER(VPC_PAINVERTCOUNT, gcmCOUNTER(pa_input_vtx_counter));
            gcmWRITE_COUNTER(VPC_PAINPRIMCOUNT, gcmCOUNTER(pa_input_prim_counter));
            gcmWRITE_COUNTER(VPC_PAOUTPRIMCOUNT, gcmCOUNTER(pa_output_prim_counter));
            gcmWRITE_COUNTER(VPC_PADEPTHCLIPCOUNT, gcmCOUNTER(pa_depth_clipped_counter));
            gcmWRITE_COUNTER(VPC_PATRIVIALREJCOUNT, gcmCOUNTER(pa_trivial_rejected_counter));
            gcmWRITE_COUNTER(VPC_PACULLCOUNT, gcmCOUNTER(pa_culled_counter));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_SETUP);
            gcmWRITE_COUNTER(VPC_SETRIANGLECOUNT, gcmCOUNTER(se_culled_triangle_count));
            gcmWRITE_COUNTER(VPC_SELINECOUNT, gcmCOUNTER(se_culled_lines_count));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_RA);
            gcmWRITE_COUNTER(VPC_RAVALIDPIXCOUNT, gcmCOUNTER(ra_valid_pixel_count));
            gcmWRITE_COUNTER(VPC_RATOTALQUADCOUNT, gcmCOUNTER(ra_total_quad_count));
            gcmWRITE_COUNTER(VPC_RAVALIDQUADCOUNTEZ, gcmCOUNTER(ra_valid_quad_count_after_early_z));
            gcmWRITE_COUNTER(VPC_RATOTALPRIMCOUNT, gcmCOUNTER(ra_total_primitive_count));
            gcmWRITE_COUNTER(VPC_RAPIPECACHEMISSCOUNT, gcmCOUNTER(ra_pipe_cache_miss_counter));
            gcmWRITE_COUNTER(VPC_RAPREFCACHEMISSCOUNT, gcmCOUNTER(ra_prefetch_cache_miss_counter));
            gcmWRITE_COUNTER(VPC_RAEEZCULLCOUNT, gcmCOUNTER(ra_eez_culled_counter));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_TX);
            gcmWRITE_COUNTER(VPC_TXTOTBILINEARREQ, gcmCOUNTER(tx_total_bilinear_requests));
            gcmWRITE_COUNTER(VPC_TXTOTTRILINEARREQ, gcmCOUNTER(tx_total_trilinear_requests));
            gcmWRITE_COUNTER(VPC_TXTOTTEXREQ, gcmCOUNTER(tx_total_texture_requests));
            gcmWRITE_COUNTER(VPC_TXMEMREADCOUNT, gcmCOUNTER(tx_mem_read_count));
            gcmWRITE_COUNTER(VPC_TXMEMREADIN8BCOUNT, gcmCOUNTER(tx_mem_read_in_8B_count));
            gcmWRITE_COUNTER(VPC_TXCACHEMISSCOUNT, gcmCOUNTER(tx_cache_miss_count));
            gcmWRITE_COUNTER(VPC_TXCACHEHITTEXELCOUNT, gcmCOUNTER(tx_cache_hit_texel_count));
            gcmWRITE_COUNTER(VPC_TXCACHEMISSTEXELCOUNT, gcmCOUNTER(tx_cache_miss_texel_count));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_PS);

            gcmWRITE_COUNTER(VPC_PSINSTCOUNT, gcmCOUNTER(ps_inst_counter) );
            gcmWRITE_COUNTER(VPC_PSBRANCHINSTCOUNT, gcmCOUNTER(pxl_branch_inst_counter));
            gcmWRITE_COUNTER(VPC_PSTEXLDINSTCOUNT, gcmCOUNTER(pxl_texld_inst_counter));
            gcmWRITE_COUNTER(VPC_PSRENDEREDPIXCOUNT, gcmCOUNTER(rendered_pixel_counter));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_PE);
            gcmWRITE_COUNTER(VPC_PEKILLEDBYCOLOR, gcmCOUNTER(pe_pixel_count_killed_by_color_pipe));
            gcmWRITE_COUNTER(VPC_PEKILLEDBYDEPTH, gcmCOUNTER(pe_pixel_count_killed_by_depth_pipe));
            gcmWRITE_COUNTER(VPC_PEDRAWNBYCOLOR, gcmCOUNTER(pe_pixel_count_drawn_by_color_pipe));
            gcmWRITE_COUNTER(VPC_PEDRAWNBYDEPTH, gcmCOUNTER(pe_pixel_count_drawn_by_depth_pipe));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_MC);
            gcmWRITE_COUNTER(VPC_MCREADREQ8BPIPE, gcmCOUNTER(mc_total_read_req_8B_from_pipeline));
            gcmWRITE_COUNTER(VPC_MCREADREQ8BIP, gcmCOUNTER(mc_total_read_req_8B_from_IP));
            gcmWRITE_COUNTER(VPC_MCWRITEREQ8BPIPE, gcmCOUNTER(mc_total_write_req_8B_from_pipeline));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_AXI);
            gcmWRITE_COUNTER(VPC_AXIREADREQSTALLED, gcmCOUNTER(hi_axi_cycles_read_request_stalled));
            gcmWRITE_COUNTER(VPC_AXIWRITEREQSTALLED, gcmCOUNTER(hi_axi_cycles_write_request_stalled));
            gcmWRITE_COUNTER(VPC_AXIWRITEDATASTALLED, gcmCOUNTER(hi_axi_cycles_write_data_stalled));
            gcmWRITE_CONST(VPG_END);

            gcmWRITE_CONST(VPG_END);

#else
            gcmWRITE_STRING("<HWCounters>\n");

            gcmPRINT_XML("<read_64Byte value=\"%u\"/>\n",
                gcmCOUNTER(gpuTotalRead64BytesPerFrame));
            gcmPRINT_XML("<write_64Byte value=\"%u\"/>\n",
                gcmCOUNTER(gpuTotalWrite64BytesPerFrame));
            gcmPRINT_XML("<gpu_cycles value=\"%u\"/>\n",
                gcmCOUNTER(gpuCyclesCounter));
            gcmPRINT_XML("<pe_pixel_count_killed_by_color_pipe value=\"%u\"/>\n",
                gcmCOUNTER(pe_pixel_count_killed_by_color_pipe));
            gcmPRINT_XML("<pe_pixel_count_killed_by_depth_pipe value=\"%u\"/>\n",
                gcmCOUNTER(pe_pixel_count_killed_by_depth_pipe));
            gcmPRINT_XML("<pe_pixel_count_drawn_by_color_pipe value=\"%u\"/>\n",
                gcmCOUNTER(pe_pixel_count_drawn_by_color_pipe));
            gcmPRINT_XML("<pe_pixel_count_drawn_by_depth_pipe value=\"%u\"/>\n",
                gcmCOUNTER(pe_pixel_count_drawn_by_depth_pipe));
            gcmPRINT_XML("<ps_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(ps_inst_counter));
            gcmPRINT_XML("<rendered_pixel_counter value=\"%u\"/>\n",
                gcmCOUNTER(rendered_pixel_counter));
            gcmPRINT_XML("<vs_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(vs_inst_counter));
            gcmPRINT_XML("<rendered_vertice_counter value=\"%u\"/>\n",
                gcmCOUNTER(rendered_vertice_counter));
            gcmPRINT_XML("<vtx_branch_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(vtx_branch_inst_counter));
            gcmPRINT_XML("<vtx_texld_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(vtx_texld_inst_counter));
            gcmPRINT_XML("<pxl_branch_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(pxl_branch_inst_counter));
            gcmPRINT_XML("<pxl_texld_inst_counter value=\"%u\"/>\n",
                gcmCOUNTER(pxl_texld_inst_counter));
            gcmPRINT_XML("<pa_input_vtx_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_input_vtx_counter));
            gcmPRINT_XML("<pa_input_prim_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_input_prim_counter));
            gcmPRINT_XML("<pa_output_prim_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_output_prim_counter));
            gcmPRINT_XML("<pa_depth_clipped_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_depth_clipped_counter));
            gcmPRINT_XML("<pa_trivial_rejected_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_trivial_rejected_counter));
            gcmPRINT_XML("<pa_culled_counter value=\"%u\"/>\n",
                gcmCOUNTER(pa_culled_counter));
            gcmPRINT_XML("<se_culled_triangle_count value=\"%u\"/>\n",
                gcmCOUNTER(se_culled_triangle_count));
            gcmPRINT_XML("<se_culled_lines_count value=\"%u\"/>\n",
                gcmCOUNTER(se_culled_lines_count));
            gcmPRINT_XML("<ra_valid_pixel_count value=\"%u\"/>\n",
                gcmCOUNTER(ra_valid_pixel_count));
            gcmPRINT_XML("<ra_total_quad_count value=\"%u\"/>\n",
                gcmCOUNTER(ra_total_quad_count));
            gcmPRINT_XML("<ra_valid_quad_count_after_early_z value=\"%u\"/>\n",
                gcmCOUNTER(ra_valid_quad_count_after_early_z));
            gcmPRINT_XML("<ra_total_primitive_count value=\"%u\"/>\n",
                gcmCOUNTER(ra_total_primitive_count));
            gcmPRINT_XML("<ra_pipe_cache_miss_counter value=\"%u\"/>\n",
                gcmCOUNTER(ra_pipe_cache_miss_counter));
            gcmPRINT_XML("<ra_prefetch_cache_miss_counter value=\"%u\"/>\n",
                gcmCOUNTER(ra_prefetch_cache_miss_counter));
            gcmPRINT_XML("<ra_eez_culled_counter value=\"%u\"/>\n",
                gcmCOUNTER(ra_eez_culled_counter));
            gcmPRINT_XML("<tx_total_bilinear_requests value=\"%u\"/>\n",
                gcmCOUNTER(tx_total_bilinear_requests));
            gcmPRINT_XML("<tx_total_trilinear_requests value=\"%u\"/>\n",
                gcmCOUNTER(tx_total_trilinear_requests));
            gcmPRINT_XML("<tx_total_discarded_texture_requests value=\"%u\"/>\n",
                gcmCOUNTER(tx_total_discarded_texture_requests));
            gcmPRINT_XML("<tx_total_texture_requests value=\"%u\"/>\n",
                gcmCOUNTER(tx_total_texture_requests));
            gcmPRINT_XML("<tx_mem_read_count value=\"%u\"/>\n",
                gcmCOUNTER(tx_mem_read_count));
            gcmPRINT_XML("<tx_mem_read_in_8B_count value=\"%u\"/>\n",
                gcmCOUNTER(tx_mem_read_in_8B_count));
            gcmPRINT_XML("<tx_cache_miss_count value=\"%u\"/>\n",
                gcmCOUNTER(tx_cache_miss_count));
            gcmPRINT_XML("<tx_cache_hit_texel_count value=\"%u\"/>\n",
                gcmCOUNTER(tx_cache_hit_texel_count));
            gcmPRINT_XML("<tx_cache_miss_texel_count value=\"%u\"/>\n",
                gcmCOUNTER(tx_cache_miss_texel_count));
            gcmPRINT_XML("<mc_total_read_req_8B_from_pipeline value=\"%u\"/>\n",
                gcmCOUNTER(mc_total_read_req_8B_from_pipeline));
            gcmPRINT_XML("<mc_total_read_req_8B_from_IP value=\"%u\"/>\n",
                gcmCOUNTER(mc_total_read_req_8B_from_IP));
            gcmPRINT_XML("<mc_total_write_req_8B_from_pipeline value=\"%u\"/>\n",
                gcmCOUNTER(mc_total_write_req_8B_from_pipeline));
            gcmPRINT_XML("<hi_axi_cycles_read_request_stalled value=\"%u\"/>\n",
                gcmCOUNTER(hi_axi_cycles_read_request_stalled));
            gcmPRINT_XML("<hi_axi_cycles_write_request_stalled value=\"%u\"/>\n",
                gcmCOUNTER(hi_axi_cycles_write_request_stalled));
            gcmPRINT_XML("<hi_axi_cycles_write_data_stalled value=\"%u\"/>\n",
                gcmCOUNTER(hi_axi_cycles_write_data_stalled));

            gcmWRITE_STRING("</HWCounters>\n");
#endif
        }
    }
    /*#endif*/

#if VIVANTE_PROFILER_NEW

    if (hardware)
    {
        static gctUINT32 frameNo = 0;
        gctUINT32   address;
        gctPOINTER  memory;
        gctUINT32 i, j;

        gcmPRINT("Frame %d:\n", frameNo);
        gcmPRINT("--------\n");

        for (j = 0; j < _HAL->profiler.curBufId; j++)
        {
            gcmPRINT("draw #%d\n", j);
            gcoBUFOBJ_Lock(_HAL->profiler.newCounterBuf[j], &address, &memory);
            /* module FE */
            for (i = 0; i < 3; i++)
            {
                gcmPRINT("FE counter #%d: %d\n", i, *((gctUINT32_PTR)memory + i));
            }
            /* module PA */
            for (i = 0; i < 7; i++)
            {
                gcmPRINT("PA counter #%d: %d\n", i, *((gctUINT32_PTR)memory + i + 3));
            }
            /* module FE */
            for (i = 0; i < 4; i++)
            {
                gcmPRINT("PE counter #%d: %d\n", i, *((gctUINT32_PTR)memory + i + 10));
            }
            gcmPRINT("--------\n");
        }
        frameNo++;
        _HAL->profiler.curBufId = 0;
    }
#endif

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

#if VIVANTE_PROFILER_CONTEXT
OnError:
    gcmFOOTER_NO();
    return status;
#endif
}

gceSTATUS
gcoPROFILER_EndDraw(
                    IN gcoHAL Hal,
                    IN gctBOOL FirstDraw
                    )
{
#if VIVANTE_PROFILER_NEW
    gceSTATUS status;
    gcoHARDWARE hardware = gcvNULL;
    gcmHEADER();
    if(!_HAL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* TODO */
    if (_HAL->profiler.curBufId >= NumOfDrawBuf)
        return gcvSTATUS_NOT_SUPPORTED;

    gcmGETHARDWARE(hardware);
    if (hardware)
    {
        gctUINT32   address;
        gctPOINTER  memory;
        gctUINT32 i;

        gcoBUFOBJ_Lock(_HAL->profiler.newCounterBuf[_HAL->profiler.curBufId], &address, &memory);
        /* module FE */
        for (i = 0; i < 3; i++)
        {
            gcoHARDWARE_ProbeCounter(hardware, address + i * sizeof(gctUINT32), 0, i);
        }
        /* module PA */
        for (i = 0; i < 7; i++)
        {
            gcoHARDWARE_ProbeCounter(hardware, address + (i + 3) * sizeof(gctUINT32), 2, i);
        }
        /* module FE */
        for (i = 0; i < 4; i++)
        {
            gcoHARDWARE_ProbeCounter(hardware, address + (i + 10) * sizeof(gctUINT32), 7, i);
        }
        _HAL->profiler.curBufId++;
    }

OnError:
    gcmFOOTER_NO();
#endif
    return gcvSTATUS_OK;
}

#else
/* Stubs for Profiler functions. */

/* Initialize the gcsProfiler. */
gceSTATUS
gcoPROFILER_Initialize(
                       IN gcoHAL Hal,
                       IN gctBOOL Enable
                       )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Destroy the gcProfiler. */
gceSTATUS
gcoPROFILER_Destroy(
                    IN gcoHAL Hal
                    )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Write data to profile. */
gceSTATUS
gcoPROFILER_Write(
                  IN gcoHAL Hal,
                  IN gctSIZE_T ByteCount,
                  IN gctCONST_POINTER Data
                  )
{
    gcmHEADER_ARG("ByteCount=%lu Data=0x%x", ByteCount, Data);

    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Flush data out. */
gceSTATUS
gcoPROFILER_Flush(
                  IN gcoHAL Hal
                  )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Call to signal end of frame. */
gceSTATUS
gcoPROFILER_EndFrame(
                     IN gcoHAL Hal
                     )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Call to signal end of draw. */
gceSTATUS
gcoPROFILER_EndDraw(
                    IN gcoHAL Hal,
                    IN gctBOOL FirstDraw
                    )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

gceSTATUS
gcoPROFILER_Count(
                  IN gcoHAL Hal,
                  IN gctUINT32 Enum,
                  IN gctINT Value
                  )
{
    gcmHEADER_ARG("Enum=%lu Value=%d", Enum, Value);

    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Profile input vertex shader. */
gceSTATUS
gcoPROFILER_ShaderVS(
                     IN gcoHAL Hal,
                     IN gctPOINTER Vs
                     )
{
    gcmHEADER_ARG("Vs=0x%x", Vs);
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

/* Profile input fragment shader. */
gceSTATUS
gcoPROFILER_ShaderFS(
                     IN gcoHAL Hal,
                     IN gctPOINTER Fs
                     )
{
    gcmHEADER_ARG("Fs=0x%x", Fs);
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

#endif /* VIVANTE_PROFILER */
#endif /* gcdENABLE_3D */
