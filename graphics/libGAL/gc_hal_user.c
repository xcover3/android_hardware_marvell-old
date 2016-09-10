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


/**
**  @file
**  gcoHAL object for user HAL layers.
**
*/

#include "gc_hal_user_precomp.h"

/* Zone used for header/footer. */
#define _GC_OBJ_ZONE    gcvZONE_HAL

/*******************************************************************************
***** Version Signature *******************************************************/

#define _gcmTXT2STR(t) #t
#define gcmTXT2STR(t) _gcmTXT2STR(t)
const char * _GAL_VERSION = "\n\0$VERSION$"
                            gcmTXT2STR(gcvVERSION_MAJOR) "."
                            gcmTXT2STR(gcvVERSION_MINOR) "."
                            gcmTXT2STR(gcvVERSION_PATCH) ":"
                            gcmTXT2STR(gcvVERSION_BUILD) "$\n";

/******************************************************************************\
******************************** gcoHAL API Code ********************************
\******************************************************************************/

#if gcdENABLE_2D
static gceSTATUS
_WForFilterBlit(
    IN gcoHAL Hal
    )
{
    gceSTATUS status;

    gcoSURF    srcSurf = gcvNULL;
    gcsRECT    srcRect;

    gcoSURF    dstSurf = gcvNULL;
    gcsRECT    dstRect;

    do
    {
        gcmERR_BREAK(gcoSURF_Construct(
            gcvNULL,
            256,
            256,
            1,
            gcvSURF_BITMAP,
            gcvSURF_A8R8G8B8,
            gcvPOOL_DEFAULT,
            &srcSurf
            ));

        gcmERR_BREAK(gcoSURF_Construct(
            gcvNULL,
            256,
            256,
            1,
            gcvSURF_BITMAP,
            gcvSURF_A8R8G8B8,
            gcvPOOL_DEFAULT,
            &dstSurf
            ));

        srcRect.left   = 0;
        srcRect.top    = 0;
        srcRect.right  = 64;
        srcRect.bottom = 16;

        dstRect.left   = 0;
        dstRect.top    = 0;
        dstRect.right  = 128;
        dstRect.bottom = 32;

        gcmERR_BREAK(gcoSURF_FilterBlit(
            srcSurf,
            dstSurf,
            &srcRect,
            &dstRect,
            gcvNULL
            ));

        gcmERR_BREAK(gcoSURF_Destroy(srcSurf));
        srcSurf = gcvNULL;

        gcmERR_BREAK(gcoSURF_Destroy(dstSurf));
        dstSurf = gcvNULL;
    }
    while(gcvFALSE);

    if (gcmIS_ERROR(status)) {
        gcmTRACE_ZONE(gcvLEVEL_ERROR, gcvZONE_HAL,
            "Failed to workarond for GC300.");

        if (srcSurf)
        {
            gcmVERIFY_OK(gcoSURF_Destroy(srcSurf));
        }

        if (dstSurf)
        {
            gcmVERIFY_OK(gcoSURF_Destroy(dstSurf));
        }
    }

    return status;
}
#endif

/*******************************************************************************
**
**  _GetUserDebugOption
**
**  Get user option from env variable VIV_OPTION.
**
**  INPUT:
**
**      gctHAL Hal
**          Pointer to HAL.
**
**  OUTPUT:
**
**      Nothing.
*/
gcsUSER_DEBUG_OPTION gcUserDebugOption =
{
    gcvDEBUG_MSG_NONE       /* gceDEBUG_MSG        debugMsg */
};

static gceSTATUS
_GetUserDebugOption(
    IN gcoHAL Hal
    )
{
    static gctINT envChecked = 0;

    if (!envChecked)
    {
        char* p = gcvNULL;
        gctSTRING pos = gcvNULL;

        gcoOS_GetEnv(gcvNULL, "VIV_DEBUG", &p);
        if (p)
        {
            gcoOS_StrStr(p, "-MSG_LEVEL", &pos);
            if (pos)
            {
                pos += sizeof("-MSG_LEVEL") - 1;
                while (pos[0] == ':')
                {
                    ++pos;
                    if (gcvSTATUS_OK == gcoOS_StrNCmp(pos, "ERROR", sizeof("ERROR") - 1))
                    {
                        /* Output error messages. */
                        gcUserDebugOption.debugMsg = gcvDEBUG_MSG_ERROR;
                        pos += sizeof("ERROR") - 1;
                    }
                    else if (gcvSTATUS_OK == gcoOS_StrNCmp(pos, "WARNING", sizeof("WARNING") - 1))
                    {
                        /* Output error messages. */
                        gcUserDebugOption.debugMsg = gcvDEBUG_MSG_WARNING;
                        pos += sizeof("WARNING") - 1;
                    }
                }
            }
        }

        envChecked = 1;
    }

    Hal->userDebugOption = &gcUserDebugOption;

    return gcvSTATUS_OK;
}

gctUINT32 gcFrameInfos[gcvFRAMEINFO_COUNT] = {0};


gceSTATUS
gcoHAL_FrameInfoOps(
    IN gcoHAL Hal,
    IN gceFRAMEINFO FrameInfo,
    IN gceFRAMEINFO_OP Op,
    IN OUT gctUINT * Val
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Hal=0x%x FrameInfo=%d Op=%d Val=0x%x", Hal, FrameInfo, Op, Val);
    gcmVERIFY_ARGUMENT(FrameInfo < gcvFRAMEINFO_COUNT);
    gcmVERIFY_ARGUMENT(Op < gcvFRAMEINFO_OP_COUNT);

    switch(Op)
    {
    case gcvFRAMEINFO_OP_ZERO:
        gcFrameInfos[FrameInfo] = 0;
        break;

    case gcvFRAMEINFO_OP_INC:
        gcFrameInfos[FrameInfo]++;
        break;

    case gcvFRAMEINFO_OP_DEC:
        if (gcFrameInfos[FrameInfo] == 0)
        {
            gcmPRINT("GAL: FramInfo(%d) underflowed", FrameInfo);
        }
        gcFrameInfos[FrameInfo]--;
        break;


    case gcvFRAMEINFO_OP_GET:
        gcmVERIFY_ARGUMENT(Val != gcvNULL);
        *Val = gcFrameInfos[FrameInfo];
        break;

    default:
        gcmPRINT("GAL: invalid FrameInfo operation(%d)", Op);
    }
    gcmFOOTER_NO();
    return status;
}

gctBOOL gcOptions[gcvOPTION_COUNT];

static gceSTATUS
_FillInOptions(
    IN gcoHAL Hal
    )
{
    gctSTRING fboScheme = gcvNULL;

    gcoOS_ZeroMemory(gcOptions, sizeof(gcOptions[0]) * gcvOPTION_COUNT);
    gcOptions[gcvOPTION_PREFER_ZCONVERT_BYPASS] = gcvTRUE;


    gcOptions[gcvOPTION_FBO_PREFER_MEM] = gcvFALSE;
    if (gcmIS_SUCCESS(gcoOS_GetEnv(gcvNULL, "VIV_FBO_PREFER_MEM", &fboScheme)) && fboScheme)
    {
        if (gcmIS_SUCCESS(gcoOS_StrCmp(fboScheme, "1")))
        {
            gcOptions[gcvOPTION_FBO_PREFER_MEM] = gcvTRUE;
        }
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHAL_GetOption
**
**  Get option boolean result.
**
**  INPUT:
**      gcoHAL Hal
**          Hal oboject
**
**      gceOPTION Option
**          Option to query about
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_GetOption(
     IN gcoHAL Hal,
     IN gceOPTION Option
     )
{
    gceSTATUS status;
    gcmHEADER_ARG("Hal=0x%x Option=%d", Hal, Option);
    gcmASSERT(Option < gcvOPTION_COUNT);

    status = (gcOptions[Option])? gcvSTATUS_TRUE : gcvSTATUS_FALSE;

    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcGetUserDebugOption
**
**  Get user debug option.
**
**  INPUT:
**
**      Nothing.
**
**  OUTPUT:
**
**      Nothing.
*/
gcsUSER_DEBUG_OPTION *
gcGetUserDebugOption(
    void
    )
{
    return &gcUserDebugOption;
}

/*******************************************************************************
**
**  gcoHAL_ConstructEx
**
**  Construct a new gcoHAL object.
**
**  INPUT:
**
**      gctPOINTER Context
**          Pointer to a context that can be used by the platform specific
**          functions.
**
**      gcoOS Os
**          Pointer to an gcoOS object.
**
**  OUTPUT:
**
**      gcoHAL * Hal
**          Pointer to a variable that will hold the gcoHAL object pointer.
*/
gceSTATUS
gcoHAL_ConstructEx(
    IN gctPOINTER Context,
    IN gcoOS Os,
    OUT gcoHAL * Hal
    )
{
    gctBOOL created = gcvFALSE;
    gcoHAL hal = gcPLS.hal;
    gceSTATUS status;
    gctPOINTER pointer = gcvNULL;
    gcsHAL_INTERFACE iface;
    gctINT32 i;

    gcmHEADER_ARG("Context=0x%x OS=0x%x", Context, Os);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Hal != gcvNULL);

    if (hal == gcvNULL)
    {
        /* Create the gcoHAL object. */
        gcmONERROR(gcoOS_Allocate(gcvNULL,
                                  gcmSIZEOF(struct _gcoHAL),
                                  &pointer));
        hal     = pointer;
        created = gcvTRUE;

        gcoOS_ZeroMemory(hal, gcmSIZEOF(struct _gcoHAL));

        /* Initialize the object. */
        hal->object.type = gcvOBJ_HAL;

        /* Zero the gco2D, gco3D, and gcoDUMP objects. */
        hal->dump      = gcvNULL;

        /* Query the kernel version number. */
        iface.command = gcvHAL_VERSION;
        gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                       IOCTL_GCHAL_INTERFACE,
                                       &iface, gcmSIZEOF(iface),
                                       &iface, gcmSIZEOF(iface)));

        /* Test if versions match. */
        if ((iface.u.Version.major != gcvVERSION_MAJOR)
        ||  (iface.u.Version.minor != gcvVERSION_MINOR)
        ||  (iface.u.Version.patch != gcvVERSION_PATCH)
        ||  (iface.u.Version.build != gcvVERSION_BUILD)
        )
        {
            gcmPRINT("HAL user version %d.%d.%d build %u %s %s",
                     gcvVERSION_MAJOR, gcvVERSION_MINOR,
                     gcvVERSION_PATCH, gcvVERSION_BUILD);
            gcmPRINT("HAL kernel version %d.%d.%d build %u",
                     iface.u.Version.major, iface.u.Version.minor,
                     iface.u.Version.patch, iface.u.Version.build);

            gcmONERROR(gcvSTATUS_VERSION_MISMATCH);
        }

#if gcmIS_DEBUG(gcdDEBUG_TRACE)
    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HAL,
                  "HAL user version %d.%d.%d build %u %s %s",
                  gcvVERSION_MAJOR, gcvVERSION_MINOR,
                  gcvVERSION_PATCH, gcvVERSION_BUILD);
    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_HAL,
                  "HAL kernel version %d.%d.%d build %u",
                  iface.u.Version.major, iface.u.Version.minor,
                  iface.u.Version.patch, iface.u.Version.build);
#endif

        /* Query chip info */
        iface.command = gcvHAL_CHIP_INFO;
        gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                       IOCTL_GCHAL_INTERFACE,
                                       &iface, gcmSIZEOF(iface),
                                       &iface, gcmSIZEOF(iface)));

        hal->chipCount = iface.u.ChipInfo.count;

        for (i = 0; i < hal->chipCount; i++)
        {
            hal->chipTypes[i] = iface.u.ChipInfo.types[i];

            switch (hal->chipTypes[i])
            {
#if gcdMULTI_GPU_AFFINITY
            case gcvHARDWARE_3D:
            case gcvHARDWARE_OCL:
                hal->is3DAvailable = gcvTRUE;
                break;
#else
            case gcvHARDWARE_3D:
                hal->is3DAvailable = gcvTRUE;
                break;
#endif
            case gcvHARDWARE_2D:
                hal->separated2D = gcvTRUE;
                break;

            case gcvHARDWARE_3D2D:
                hal->is3DAvailable = gcvTRUE;
                break;

            default:
                break;
            }
        }

#if VIVANTE_PROFILER
        hal->profiler.enable = 0;
#endif

        hal->isGpuBenchSmoothTriangle = gcvFALSE;
    }

    /* Get user option. */
    _GetUserDebugOption(hal);

    _FillInOptions(hal);

    /* Return pointer to the gcoHAL object. */
    *Hal = hal;

    /* Success. */
    gcmFOOTER_ARG("*Hal=0x%x", *Hal);
    return gcvSTATUS_OK;

OnError:
    /* Roll back. */
    if ((hal != gcvNULL) && created)
    {
        gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, hal));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_DestroyEx
**
**  Destroy an gcoHAL object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object that needs to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
**
*******************************************************************************/
gceSTATUS
gcoHAL_DestroyEx(
    IN gcoHAL Hal
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Hal=0x%x", Hal);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hal, gcvOBJ_HAL);

    /* Destroy the gcoDUMP object if any. */
    if (Hal->dump != gcvNULL)
    {
        gcmONERROR(gcoDUMP_Destroy(Hal->dump));
        Hal->dump = gcvNULL;
    }

    /* Free the gcoHAL object. */
    gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Hal));

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
**  gcoHAL_Construct
**
**  Construct a new gcoHAL object. Empty function only for compatibility.
**
**  INPUT:
**
**      gctPOINTER Context
**          Pointer to a context that can be used by the platform specific
**          functions.
**
**      gcoOS Os
**          Pointer to an gcoOS object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_Construct(
    IN gctPOINTER Context,
    IN gcoOS Os,
    OUT gcoHAL * Hal
    )
{
    /* Return gcoHAL object for compatibility to prevent any failure in applications. */
    *Hal = gcPLS.hal;

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHAL_Destroy
**
**  Destroy an gcoHAL object. Empty function only for compatibility.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object that needs to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
**
*******************************************************************************/
gceSTATUS
gcoHAL_Destroy(
    IN gcoHAL Hal
    )
{
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoHAL_Call
**
**  Call the kernel HAL layer.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gcsHAL_INTERFACE * Interface
**          Pointer to an gcsHAL_INTERFACE structure that defines the command to
**          be executed by the kernel HAL layer.
**
**  OUTPUT:
**
**      gcsHAL_INTERFACE * Interface
**          Pointer to an gcsHAL_INTERFACE structure that will be filled in by
**          the kernel HAL layer.
*/
gceSTATUS
gcoHAL_Call(
    IN gcoHAL Hal,
    IN OUT gcsHAL_INTERFACE * Interface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Interface=0x%x", Interface);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Interface != gcvNULL);

    /* Call kernel service. */
    status = gcoOS_DeviceControl(
        gcvNULL,
        IOCTL_GCHAL_INTERFACE,
        Interface, gcmSIZEOF(gcsHAL_INTERFACE),
        Interface, gcmSIZEOF(gcsHAL_INTERFACE)
        );

    if (gcmIS_SUCCESS(status))
    {
        status = Interface->status;
    }

    if (status == gcvSTATUS_OUT_OF_MEMORY)
    {
#if gcdENABLE_VG
        gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
        gcmGETCURRENTHARDWARE(currentType);
        if (currentType == gcvHARDWARE_VG)
        {
            /* Commit any command queue to memory. */
            gcmONERROR(gcoVGHARDWARE_Commit(gcvNULL, gcvTRUE));
        }
        else
#endif
        {
            /* Commit any command queue to memory. */
            gcmONERROR(gcoHARDWARE_Commit(gcvNULL));

            /* Stall the hardware. */
            gcmONERROR(gcoHARDWARE_Stall(gcvNULL));
        }

        /* Retry kernel call again. */
        status = gcoOS_DeviceControl(
            gcvNULL,
            IOCTL_GCHAL_INTERFACE,
            Interface, gcmSIZEOF(gcsHAL_INTERFACE),
            Interface, gcmSIZEOF(gcsHAL_INTERFACE)
            );

        if (gcmIS_SUCCESS(status))
        {
            status = Interface->status;
        }
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_MapMemory
**
**  Map a range of video memory into the process space.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctPHYS_ADDR Physical
**          Physical address of video memory to map.
**
**      gctSIZE_T NumberOfBytes
**          Number of bytes to map.
**
**  OUTPUT:
**
**      gctPOINTER * Logical
**          Pointer to a variable that will hold the logical address of the
**          mapped video memory.
*/
gceSTATUS
gcoHAL_MapMemory(
    IN gcoHAL Hal,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T NumberOfBytes,
    OUT gctPOINTER * Logical
    )
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmHEADER_ARG("Physical=0x%x NumberOfBytes=%lu",
                  Physical, NumberOfBytes);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Call kernel API to map the memory. */
    iface.command              = gcvHAL_MAP_MEMORY;
    iface.u.MapMemory.physical = gcmPTR2INT32(Physical);
    iface.u.MapMemory.bytes    = NumberOfBytes;
    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    /* Return logical address. */
    *Logical = gcmUINT64_TO_PTR(iface.u.MapMemory.logical);

    /* Success. */
    gcmFOOTER_ARG("*Logical=0x%x", *Logical);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_UnmapMemory
**
**  Unmap a range of video memory from the process space.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctPHYS_ADDR Physical
**          Physical address of video memory to unmap.
**
**      gctSIZE_T NumberOfBytes
**          Number of bytes to unmap.
**
**      gctPOINTER Logical
**          Logical address of the video memory to unmap.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_UnmapMemory(
    IN gcoHAL Hal,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T NumberOfBytes,
    IN gctPOINTER Logical
    )
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmHEADER_ARG("Physical=0x%x NumberOfBytes=%lu Logical=0x%x",
                  Physical, NumberOfBytes, Logical);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Call kernel API to unmap the memory. */
    iface.command                = gcvHAL_UNMAP_MEMORY;
    iface.u.UnmapMemory.physical = gcmPTR2INT32(Physical);
    iface.u.UnmapMemory.bytes    = NumberOfBytes;
    iface.u.UnmapMemory.logical  = gcmPTR_TO_UINT64(Logical);
    status = gcoHAL_Call(gcvNULL, &iface);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ScheduleUnmapMemory
**
**  Schedule an unmap of a buffer mapped through its physical address.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctPHYS_ADDR Physical
**          Physical address of video memory to unmap.
**
**      gctSIZE_T NumberOfBytes
**          Number of bytes to unmap.
**
**      gctPOINTER Logical
**          Logical address of the video memory to unmap.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_ScheduleUnmapMemory(
    IN gcoHAL Hal,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T NumberOfBytes,
    IN gctPOINTER Logical
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Physical=0x%x NumberOfBytes=%lu Logical=0x%x",
                  Physical, NumberOfBytes, Logical);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(NumberOfBytes > 0);
    gcmVERIFY_ARGUMENT(Logical != gcvNULL);

    /* Schedule an event to unmap the user memory. */
    iface.command                = gcvHAL_UNMAP_MEMORY;
    iface.u.UnmapMemory.bytes    = NumberOfBytes;
    iface.u.UnmapMemory.physical = gcmPTR2INT32(Physical);
    iface.u.UnmapMemory.logical  = gcmPTR_TO_UINT64(Logical);
    status = gcoHAL_ScheduleEvent(gcvNULL, &iface);

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_MapUserMemory
**
**  Map a contiguous memory to GPU address space.
**
**  INPUT:
**
**      gctPOINTER Logical
**          Logical address of this memory.
**
**      gctUINT32 Physical
**          Physical address of this memory.
**
**      gctSIZE_T Size
**          Size in bytes of the memory to map.
**
**  OUTPUT:
**
**      gctPOINTER Info
**          Information record returned by gcoHAL_MapUserMemory.
**
**      gctUINT32_PTR GPUAddress
**          The GPU address returned by gcoHAL_MapUserMemory.
*/
gceSTATUS
gcoHAL_MapUserMemory(
    IN gctPOINTER Logical,
    IN gctUINT32 Physical,
    IN gctSIZE_T Size,
    OUT gctPOINTER * Info,
    OUT gctUINT32_PTR GPUAddress
    )
{
    gceSTATUS status;
    gctUINT32 baseAddress;
    gctUINT32 gpuPhysical = Physical;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Logical=0x%08x Physical=0x%08x Size=0x%08x",
                   Logical, Physical, Size);

    gcmVERIFY_ARGUMENT(Logical != gcvNULL || Physical != gcvINVALID_ADDRESS);
    gcmVERIFY_ARGUMENT(Info != gcvNULL);
    gcmVERIFY_ARGUMENT(GPUAddress != gcvNULL);
    gcmVERIFY_ARGUMENT(Size != 0);

    gcmONERROR(gcoOS_GetBaseAddress(gcvNULL, &baseAddress));

    /* Only valid physical address can be converted to GPU's view */
    if (Physical != gcvINVALID_ADDRESS)
    {
        /* Convert physical to GPU's view. */
        Physical -= baseAddress;

        gcoOS_CPUPhysicalToGPUPhysical(Physical, &gpuPhysical);
    }

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    /* 2D VG can access the whole 4G memory */
    if (currentHW == gcvHARDWARE_VG)
    {
        /* CPU physical address can be used as GPU address. */
        *GPUAddress = Physical;
        *Info = gcvNULL;

        /* Add the memory info. */
        gcmDUMP_ADD_MEMORY_INFO(*GPUAddress, Logical, Physical + baseAddress, Size);

        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }
#endif

#if !gcdPROCESS_ADDRESS_SPACE
    /* Does memory in the range [baseAddress, baseAddress + 2G - 1]? */
    if (!(gpuPhysical & 0x80000000)
    && !((gpuPhysical + Size - 1) & 0x80000000)
    )
    {
        /* CPU physical address can be used as GPU address. */
        *GPUAddress = gpuPhysical;
        *Info = gcvNULL;
    }
    else
#endif
    {
        /* Physical pointer has to be mapped before use. */
        gcmONERROR(gcoOS_MapUserMemoryEx(
                   gcvNULL,
                   Logical,
                   Physical,
                   Size,
                   Info,
                   GPUAddress
                   ));
    }

    /* Add the memory info. */
    gcmDUMP_ADD_MEMORY_INFO(*GPUAddress, Logical, Physical + baseAddress, Size);

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_UnmapUserMemory
**
**  Unmap a contiguous memory from GPU address space.
**
**  INPUT:
**
**      gctPOINTER Logical
**          Pointer to memory to unmap.
**
**      gctSIZE_T Size
**          Size in bytes of the memory to unmap.
**
**      gctPOINTER Info
**          Information record returned by gcoHAL_MapUserMemory.
**
**      gctUINT32_PTR GPUAddress
**          The address returned by gcoHAL_MapUserMemory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_UnmapUserMemory(
    IN gctPOINTER Logical,
    IN gctSIZE_T Size,
    IN gctPOINTER Info,
    IN gctUINT32 GPUAddress
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Info=0x%x Size=%lu GPUAddress=0x%08x Logical=0x%x",
                  Info, Size, GPUAddress, Logical);

    if (Info != gcvNULL)
    {
        /* Delete the memory info. */
        gcmDUMP_DEL_MEMORY_INFO(GPUAddress);

        gcmONERROR(gcoHAL_ScheduleUnmapUserMemory(
                   gcvNULL,
                   Info,
                   Size,
                   GPUAddress,
                   Logical
                   ));
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ScheduleUnmapUserMemory
**
**  Schedule an unmap of a user buffer using event mechanism.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctPOINTER Info
**          Information record returned by gcoOS_MapUserMemory.
**
**      gctSIZE_T Size
**          Size in bytes of the memory to unlock.
**
**      gctUINT32_PTR Address
**          The address returned by gcoOS_MapUserMemory.
**
**      gctPOINTER Memory
**          Pointer to memory to unlock.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_ScheduleUnmapUserMemory(
    IN gcoHAL Hal,
    IN gctPOINTER Info,
    IN gctSIZE_T Size,
    IN gctUINT32 Address,
    IN gctPOINTER Memory
    )
{
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif

    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Info=0x%x Size=%lu Address=0x%08x Memory=0x%x",
                  Info, Size, Address, Memory);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Size > 0);
    gcmVERIFY_ARGUMENT(Memory != gcvNULL);

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);

    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcoVGHARDWARE_ScheduleUnmapUserMemory(
                   gcvNULL,
                   Info,
                   Size,
                   Address,
                   Memory
                   );
    }
    else
#endif
    {
        /* Schedule an event to unmap the user memory. */
        iface.command = gcvHAL_UNMAP_USER_MEMORY;
        iface.u.UnmapUserMemory.info    = gcmPTR2INT32(Info);
        iface.u.UnmapUserMemory.size    = Size;
        iface.u.UnmapUserMemory.address = Address;
        iface.u.UnmapUserMemory.memory  = gcmPTR_TO_UINT64(Memory);
        status = gcoHAL_ScheduleEvent(gcvNULL, &iface);
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_SendFence
**
**  Insert fence command in command buffer
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      Nothing.
*/
#if gcdENABLE_3D
gceSTATUS
gcoHAL_SendFence(
    IN gcoHAL Hal
    )
{
    gceSTATUS status = gcvSTATUS_OK;
#if gcdSYNC
    gctBOOL fenceEnable;
#endif
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcmHEADER();

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }
    else
#endif
    {
#if gcdSYNC
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);

        if (fenceEnable)
        {
            gcmVERIFY_OK(gcoHARDWARE_SendFence(gcvNULL));
        }
#endif
    }

    /* Success. */
    gcmFOOTER_NO();
    return status;
}
#endif /* gcdENABLE_3D */

/*******************************************************************************
**
**  gcoHAL_Commit
**
**  Commit the current command buffer to hardware and optionally wait until the
**  hardware is finished.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctBOOL Stall
**          gcvTRUE if the thread needs to wait until the hardware has finished
**          executing the committed command buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_Commit(
    IN gcoHAL Hal,
    IN gctBOOL Stall
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
#if (gcdENABLE_3D && !gcdENABLE_VG)
#if gcdSYNC
    gctBOOL fenceEnable;
#endif
#endif

    gcmHEADER_ARG("Stall=%d", Stall);

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        /* Commit the command queue. */
        gcmONERROR(gcoVGHARDWARE_Commit(gcvNULL, Stall));

    }
    else
#endif
    {
#if (gcdENABLE_3D && !gcdENABLE_VG)
#if gcdSYNC
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);
        if (fenceEnable)
        {
            gcoHARDWARE_SendFence(gcvNULL);
        }
#endif
#endif
        /* Commit the command buffer to hardware. */
        gcmONERROR(gcoHARDWARE_Commit(gcvNULL));

        if (Stall)
        {
            /* Stall the hardware. */
            gcmONERROR(gcoHARDWARE_Stall(gcvNULL));
        }
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_2D
/*******************************************************************************
**
**  gcoHAL_Get2DEngine
**
**  Get the pointer to the gco2D object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gco2D * Engine
**          Pointer to a variable receiving the gco2D object pointer.
*/
gceSTATUS
gcoHAL_Get2DEngine(
    IN gcoHAL Hal,
    OUT gco2D * Engine
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

    gcsTLS_PTR tls;

    gcmHEADER();

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Engine != gcvNULL);

    gcmONERROR(gcoOS_GetTLS(&tls));

    if (tls->engine2D == gcvNULL)
    {
        gceCHIPMODEL chipModel;
        gctUINT32 chipRevision;

        /* Construct the gco2D object. */
        gcmONERROR(gco2D_Construct(Hal, &tls->engine2D));

        gcmONERROR(
            gcoHARDWARE_QueryChipIdentity(gcvNULL,
                                          &chipModel,
                                          &chipRevision,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL,
                                          gcvNULL));

        if (gcoHARDWARE_Is2DAvailable(gcvNULL) && (chipRevision <= 0x00004400))
        {
            _WForFilterBlit(gcvNULL);
        }
    }

    /* Return pointer to the gco2D object. */
    *Engine = tls->engine2D;

    /* Success. */
    gcmFOOTER_ARG("*Engine=0x%x", *Engine);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_2D */

#if gcdENABLE_VG
/*******************************************************************************
**
**  gcoHAL_GetVGEngine
**
**  Get the pointer to the gcoVG object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gcoVG * Engine
**          Pointer to a variable receiving the gcoVG object pointer.
*/
gceSTATUS
gcoHAL_GetVGEngine(
    IN gcoHAL Hal,
    OUT gcoVG * Engine
    )
{
#if gcdENABLE_VG
    gceSTATUS status;
    gcsTLS_PTR __tls__;

    gcmHEADER();

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Engine != gcvNULL);

    gcmONERROR(gcoOS_GetTLS(&__tls__));

    if (__tls__->engineVG == gcvNULL)
    {
        /* Construct the gcoVG object. */
        gcmONERROR(gcoVG_Construct(gcPLS.hal, &__tls__->engineVG));
    }

    /* Return pointer to the gcoVG object. */
    *Engine = __tls__->engineVG;

    /* Success. */
    gcmFOOTER_ARG("*Engine=0x%x", *Engine);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
#else
    return gcvSTATUS_NOT_SUPPORTED;
#endif
}
#endif

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHAL_Get3DEngine
**
**  Get the pointer to the gco3D object. Don't use it!!!
**  Keep it here for external library (libgcu.so)
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gco3D * Engine
**          Pointer to a variable receiving the gco3D object pointer.
*/
gceSTATUS
gcoHAL_Get3DEngine(
    IN gcoHAL Hal,
    OUT gco3D * Engine
    )
{
    gceSTATUS status;
    gcsTLS_PTR tls;

    gcmHEADER();

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Engine != gcvNULL);

    gcmONERROR(gcoOS_GetTLS(&tls));

    /* Return NULL to the gco3D object. */
    *Engine = gcvNULL;

    /* Success. */
    gcmFOOTER_ARG("*Engine=0x%x", *Engine);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;

}

/*******************************************************************************
**
**  gcoHAL_SetFscaleValue
**
**  Set the fscale value used when GPU is gcvPOWER_ON.
**
**  INPUT:
**
**      gctUINT FscalueValue
**          Fscale value to be set.
**
**  OUTPUT:
**
**          Nothing.
*/
gceSTATUS
gcoHAL_SetFscaleValue(
    IN gctUINT FscaleValue
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("FscaleValue=0x%X", FscaleValue);

    iface.command = gcvHAL_SET_FSCALE_VALUE;
    iface.u.SetFscaleValue.value = FscaleValue;

    status = gcoHAL_Call(gcvNULL, &iface);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_GetFscaleValue
**
**  Get current, minimal and maximal fscale value .
**
**  INPUT:
**
**          Nothing.
**
**  OUTPUT:
**
**          FscaleValue
**              Current fscale value.
**
**          MinFscaleValue
**              Minimal fscale value can be used. Pass gcvNULL if not needed.
**
**          MaxFscaleValue
**              Maximal fscale value can be used. Pass gcvNULL if not needed.
*/
gceSTATUS
gcoHAL_GetFscaleValue(
    OUT gctUINT * FscaleValue,
    OUT gctUINT * MinFscaleValue,
    OUT gctUINT * MaxFscaleValue
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("FscaleValue=0x%X MinFscaleValue=0x%X MaxFscaleValue=0x%X",
                   FscaleValue, MinFscaleValue, MaxFscaleValue);

    gcmVERIFY_ARGUMENT(FscaleValue != gcvNULL);

    iface.command = gcvHAL_GET_FSCALE_VALUE;

    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    *FscaleValue =iface.u.GetFscaleValue.value;

    if (MinFscaleValue)
    {
        *MinFscaleValue =iface.u.GetFscaleValue.minValue;
    }

    if (MaxFscaleValue)
    {
        *MaxFscaleValue =iface.u.GetFscaleValue.maxValue;
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
OnError:
    gcmFOOTER();
    return status;
}

/* Set PLS enable NP2Texture */
gceSTATUS
gcoHAL_SetBltNP2Texture(
    gctBOOL enable
    )
{
#if gcdUSE_NPOT_PATCH
    gcPLS.bNeedSupportNP2Texture = enable;
#endif

    return gcvSTATUS_OK;
}
#endif /* gcdENABLE_3D */

/*******************************************************************************
**
**  gcoHAL_GetDump
**
**  Get the pointer to the gcoDUMP object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gcoDUMP * Dump
**          Pointer to a variable receiving the gcoDUMP object pointer.
*/
gceSTATUS
gcoHAL_GetDump(
    IN gcoHAL Hal,
    OUT gcoDUMP * Dump
    )
{
    gceSTATUS status;

    gcmHEADER();

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Dump != gcvNULL);

    if (gcPLS.hal->dump == gcvNULL)
    {
        /* Construct the gcoDUMP object. */
        gcmONERROR(gcoDUMP_Construct(gcvNULL, gcvNULL, &gcPLS.hal->dump));
    }

    /* Return pointer to the gcoDUMP object. */
    *Dump = gcPLS.hal->dump;

    /* Success. */
    gcmFOOTER_ARG("*Dump=0x%x", *Dump);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoHAL_GetPatchID
**
**  Get the patch ID according to current process name.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gcePATCH_ID * PatchID
**          Pointer to a variable receiving the gcePATCH_ID.
**
*******************************************************************************/
gceSTATUS
gcoHAL_GetPatchID(
    IN  gcoHAL Hal,
    OUT gcePATCH_ID * PatchID
    )
{
#if gcdDISABLE_CORES_2D3D
    return gcvSTATUS_INVALID_ARGUMENT;
#else
    gceSTATUS status = gcvSTATUS_OK;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcmHEADER();
#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }
    else
#endif
    {
        status = gcoHARDWARE_GetPatchID(gcvNULL, PatchID);
    }
    gcmFOOTER();
    return status;
#endif
}

/*******************************************************************************
**
**  gcoHAL_SetPatchID
**
**  Set patch ID
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  INPUT:
**
**      gcePATCH_ID PatchID
**          Patch value to protend.
**
*******************************************************************************/
gceSTATUS
gcoHAL_SetPatchID(
    IN  gcoHAL Hal,
    IN  gcePATCH_ID PatchID
    )
{
    gceSTATUS status = gcvSTATUS_OK;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcmHEADER();
#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }
    else
#endif
    {
        status = gcoHARDWARE_SetPatchID(gcvNULL, PatchID);
    }
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHAL_SetGlobalPatchID
**
**  Set global patch ID, mostly time used by vPlayer to pretend to be any app.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  INPUT:
**
**      gcePATCH_ID PatchID
**          Patch value to protend.
**
*******************************************************************************/
gceSTATUS
gcoHAL_SetGlobalPatchID(
    IN  gcoHAL Hal,
    IN  gcePATCH_ID PatchID
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER();
    gcPLS.patchID = PatchID;
    gcmPRINT("!!!Set PatchID=%d", PatchID);
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoHAL_GetSpecialHintData(
    IN  gcoHAL Hal,
    OUT gctINT * Hint
    )
{
    gceSTATUS status = gcvSTATUS_OK;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcmHEADER();
#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }
    else
#endif
    {
        status = gcoHARDWARE_GetSpecialHintData(gcvNULL, Hint);
    }
    gcmFOOTER();
    return status;
}
#endif /* gcdENABLE_3D */

/* Schedule an event. */
gceSTATUS
gcoHAL_ScheduleEvent(
    IN gcoHAL Hal,
    IN OUT gcsHAL_INTERFACE * Interface
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcmHEADER_ARG("Interface=0x%x", Interface);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Interface != gcvNULL);
#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentHW);
    if (currentHW == gcvHARDWARE_VG)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }
    else
#endif
    {
        /* Send event to hardware layer. */
        status = gcoHARDWARE_CallEvent(gcvNULL, Interface);
    }
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_SetPowerManagementState
**
**  Set GPU to a specified power state.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gceCHIPPOWERSTATE State
**          Power State.
**
*/
gceSTATUS
gcoHAL_SetPowerManagementState(
    IN gcoHAL Hal,
    IN gceCHIPPOWERSTATE State
    )
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmHEADER_ARG("State=%d", State);

    /* Call kernel API to set power management state. */
    iface.command                    = gcvHAL_SET_POWER_MANAGEMENT_STATE;
    iface.u.SetPowerManagement.state = State;
    status = gcoHAL_Call(gcvNULL, &iface);

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_Compact(
    IN gcoHAL Hal
    )
{
    gceSTATUS status;

    gcmHEADER();

    status = gcoOS_Compact(gcvNULL);

    gcmFOOTER();
    return status;
}

#if VIVANTE_PROFILER
gceSTATUS
gcoHAL_ProfileStart(
    IN gcoHAL Hal
    )
{
    gceSTATUS status;

    gcmHEADER();

    status = gcoOS_ProfileStart(gcvNULL);

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_ProfileEnd(
    IN gcoHAL Hal,
    IN gctCONST_STRING Title
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Title=%s", Title);

    status = gcoOS_ProfileEnd(gcvNULL, Title);

    gcmFOOTER();
    return status;
}
#else
gceSTATUS
gcoHAL_ProfileStart(
    IN gcoHAL Hal
    )
{
    gcmHEADER();
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}

gceSTATUS
gcoHAL_ProfileEnd(
    IN gcoHAL Hal,
    IN gctCONST_STRING Title
    )
{
    gcmHEADER_ARG("Title=%s", Title);
    gcmFOOTER_NO();
    return gcvSTATUS_INVALID_REQUEST;
}
#endif

gceSTATUS
gcoHAL_DestroySurface(
    IN gcoHAL Hal,
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%0x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Destroy the surface using this gcoHAL. */
    gcmONERROR(gcoSURF_Destroy(Surface));

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
**  gcoHAL_SetTimer
**
**  Set Timer to start/stop.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to a gcoHAL object.
**
**      gctUINT32 Timer
**          Which Timer to select for being started or stopped.
**
**      gctBOOL Start
**          Start (gcvTRUE) or Stop (gcvFALSE) request.
**
*/
gceSTATUS
gcoHAL_SetTimer(
    IN gcoHAL Hal,
    IN gctUINT32 Timer,
    IN gctBOOL Start
    )
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmHEADER_ARG("Timer=0x%0x Start=0x%0x", Timer, Start);

    /* Call kernel API to map the memory. */
    iface.command             = gcvHAL_TIMESTAMP;
    iface.u.TimeStamp.timer   = Timer;
    iface.u.TimeStamp.request = Start;
    gcmONERROR(gcoHAL_ScheduleEvent(gcvNULL, &iface));

    /* Commit exisiting command buffer, to execute any scheduled timer events. */
    gcmONERROR(gcoHAL_Commit(gcvNULL, gcvFALSE));

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
**  gcoHAL_GetTimerTime
**
**  Get Time delta from a Timer in microseconds.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to a gcoHAL object.
**
**      gctUINT32 Timer
**          Which Timer to select.
**
**      gctINT32_PTR TimeDelta
**          Pointer to a variable, which will store the result.
**
*/
gceSTATUS
gcoHAL_GetTimerTime(
    IN gcoHAL Hal,
    IN gctUINT32 Timer,
    OUT gctINT32_PTR TimeDelta
    )
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmHEADER_ARG("Timer=0x%0x", Timer);

    if (TimeDelta == gcvNULL)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Commit exisiting command buffer, to execute any scheduled timer events. */
    gcmONERROR(gcoHAL_Commit(gcvNULL, gcvTRUE));

    /* Call kernel API to map the memory. */
    iface.command             = gcvHAL_TIMESTAMP;
    iface.u.TimeStamp.timer   = Timer;
    iface.u.TimeStamp.request = 2;
    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    /* Return time delta value. */
    *TimeDelta = iface.u.TimeStamp.timeDelta;

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
**  gcoHAL_SetTimeOut
**
**  set timeout value.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to a gcoHAL object.
**
**      gctUINT32 timeOut
**          Which timeOut value to set.
**
*/
gceSTATUS
gcoHAL_SetTimeOut(
    IN gcoHAL Hal,
    IN gctUINT32 timeOut
    )
{
#if gcdGPU_TIMEOUT
    gcsHAL_INTERFACE iface;
    gceSTATUS status;
#endif
    gcmHEADER_ARG("Hal=0x%x timeOut=%d", Hal, timeOut);

#if gcdGPU_TIMEOUT
    iface.command = gcvHAL_SET_TIMEOUT;
    iface.u.SetTimeOut.timeOut = timeOut;
    status = gcoOS_DeviceControl(gcvNULL,
                                 IOCTL_GCHAL_INTERFACE,
                                 &iface, gcmSIZEOF(iface),
                                 &iface, gcmSIZEOF(iface));
    if (gcmIS_ERROR(status))
    {
        gcmFOOTER();
        return status;
    }
#endif
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gcoHAL_AddFrameDB(
    void
    )
{
#if gcdFRAME_DB
    gceSTATUS status;
    gcsHAL_INTERFACE ioctl;

    gcmHEADER();

    /* Test if we have outgrown our frame count. */
    if (gcPLS.hal->frameDBIndex >= gcdFRAME_DB)
    {
        gcmONERROR(gcvSTATUS_TOO_COMPLEX);
    }

    /* Get frame information. */
    ioctl.command                  = gcvHAL_GET_FRAME_INFO;
    ioctl.u.GetFrameInfo.frameInfo =
        gcmPTR_TO_UINT64(&gcPLS.hal->frameDB[gcPLS.hal->frameDBIndex]);
    gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                   IOCTL_GCHAL_INTERFACE,
                                   &ioctl, gcmSIZEOF(ioctl),
                                   &ioctl, gcmSIZEOF(ioctl)));

    /* Increment frame index. */
    gcPLS.hal->frameDBIndex += 1;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
#else
    return gcvSTATUS_NOT_SUPPORTED;
#endif
}

#if gcdFRAME_DB
static gceSTATUS
gcoHAL_PrintFrameDB(
    gctFILE File,
    gctCONST_STRING Format,
    ...
    )
{
    gctCHAR buffer[256];
    gctUINT offset = 0;
    gceSTATUS status;
    gctARGUMENTS arguments;

    gcmHEADER_ARG("File=0x%x Format=0x%x", File, Format);

    /* Format the string. */
    gcmARGUMENTS_START(arguments, Format);
    gcmONERROR(gcoOS_PrintStrVSafe(buffer, gcmSIZEOF(buffer), &offset, Format, arguments));
    gcmARGUMENTS_END(arguments);

    if (File != gcvNULL)
    {
        /* Print to file. */
        gcmONERROR(gcoOS_Write(gcvNULL, File, offset, buffer));
    }
    else
    {
        /* Print to debugger. */
        gcmPRINT("%s", buffer);
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#define gcmGET_COUNTER(record, field, usePrev) \
    ( record->field \
    - (usePrev ? record[-1].field : 0) \
    )

gceSTATUS
gcoHAL_DumpFrameDB(
    gctCONST_STRING Filename OPTIONAL
    )
{
#if gcdFRAME_DB
    gctINT i, j;
    gctFILE file = gcvNULL;
    gcsHAL_FRAME_INFO * record;
    gceSTATUS status;

    gcmHEADER_ARG("Filename=%s", gcmOPT_STRING(Filename));

    if (Filename != gcvNULL)
    {
        /* Open the filename. Silently ignore any errors. */
        gcoOS_Open(gcvNULL, Filename, gcvFILE_CREATETEXT, &file);
    }

    /* Loop through all records. */
    for (i = 0, record = gcPLS.hal->frameDB;
         i < gcPLS.hal->frameDBIndex;
         ++i, ++record)
    {
        const gctBOOL usePrev = (i > 0) && !gcdFRAME_DB_RESET;

        /* Print frame info. */
        gcmONERROR(gcoHAL_PrintFrameDB(
            file,
            "Frame %d: tick=%llu delta=%llu\n",
            i + 1,
            record->ticks,
            (i == 0) ? 0 : record->ticks - record[-1].ticks));

        for (j = 0; (j < 8) && (record->mcCycles[j] > 0); ++j)
        {
            /* Print HW cycles. */
            gcmONERROR(gcoHAL_PrintFrameDB(
                file,
                "  HW[%d]: cycles=%u idle=%u mcCycles=%u\n",
                j,
                record->cycles[j],
                record->idleCycles[j],
                gcmGET_COUNTER(record, mcCycles[j], (i > 0))));
        }

        for (j = 0; (j < 8) && (record->mcCycles[j] > 0); ++j)
        {
            /* Print HW bandwidth. */
            gcmONERROR(gcoHAL_PrintFrameDB(
                file,
                "  BW[%d]: readBytes=%u/%llu writeBytes=%u/%llu\n",
                j,
                record->readRequests[j],
                (gctUINT64) record->readBytes8[j] << 3,
                record->writeRequests[j],
                (gctUINT64) record->writeBytes8[j] << 3));
        }

        /* Print shader HW counters. */
        gcmONERROR(gcoHAL_PrintFrameDB(
            file,
            "  SH: cycles=%u vs=%u/%u ps=%u/%u\n",
            gcmGET_COUNTER(record, shaderCycles, usePrev),
            gcmGET_COUNTER(record, vsInstructionCount, usePrev),
            gcmGET_COUNTER(record, vsTextureCount, usePrev),
            gcmGET_COUNTER(record, psInstructionCount, usePrev),
            gcmGET_COUNTER(record, psTextureCount, usePrev)));

        /* Print texture HW counters. */
        gcmONERROR(gcoHAL_PrintFrameDB(
            file,
            "  TX: bi=%u tri=%u bytes=%llu hit=%u miss=%u\n",
            gcmGET_COUNTER(record, bilinearRequests, usePrev),
            gcmGET_COUNTER(record, trilinearRequests, usePrev),
            (gctUINT64) gcmGET_COUNTER(record, txBytes8, usePrev) << 3,
            gcmGET_COUNTER(record, txHitCount, usePrev),
            gcmGET_COUNTER(record, txMissCount, usePrev)));

        /* Print primitive assembly HW counters. */
        gcmONERROR(gcoHAL_PrintFrameDB(
            file,
            "  PA: vertex=%u primitives=%u/-%u/@%u/#%u/%u\n",
            gcmGET_COUNTER(record, vertexCount, usePrev),
            gcmGET_COUNTER(record, primitiveCount, usePrev),
            gcmGET_COUNTER(record, rejectedPrimitives, usePrev),
            gcmGET_COUNTER(record, culledPrimitives, usePrev),
            gcmGET_COUNTER(record, clippedPrimitives, usePrev),
            gcmGET_COUNTER(record, outPrimitives, usePrev)));

        /* Print rasterizer HW counters. */
        gcmONERROR(gcoHAL_PrintFrameDB(
            file,
            "  RA: primitives=%u quads=-%u/%u/%u pixels=%u\n",
            gcmGET_COUNTER(record, inPrimitives, usePrev),
            gcmGET_COUNTER(record, culledQuadCount, usePrev),
            gcmGET_COUNTER(record, totalQuadCount, usePrev),
            gcmGET_COUNTER(record, quadCount, usePrev),
            gcmGET_COUNTER(record, totalPixelCount, usePrev)));

        for (j = 0; (j < 8) && (record->mcCycles[j] > 0); ++j)
        {
            /* Print per-pipe pixel count. */
            gcmONERROR(gcoHAL_PrintFrameDB(
                file,
                "  PE[%d]: color=-%u/+%u depth=-%u/+%u\n",
                j,
                gcmGET_COUNTER(record, colorKilled[j], usePrev),
                gcmGET_COUNTER(record, colorDrawn[j], usePrev),
                gcmGET_COUNTER(record, depthKilled[j], usePrev),
                gcmGET_COUNTER(record, depthDrawn[j], usePrev)));
        }
    }

    /* Reset frame index. */
    gcPLS.hal->frameDBIndex = 0;

    if (file != gcvNULL)
    {
        /* Close the file. */
        gcmONERROR(gcoOS_Close(gcvNULL, file));
        file = gcvNULL;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (file != gcvNULL)
    {
        /* Close the file. */
        gcmONERROR(gcoOS_Close(gcvNULL, file));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
#else
    return gcvSTATUS_NOT_SUPPORTED;
#endif
}

/*******************************************************************************
 *  Generic record allocation using preallocated containers.
 ******************************************************************************/

gceSTATUS
gcsCONTAINER_Construct(
    IN gcsCONTAINER_PTR Container,
    gctUINT RecordsPerContainer,
    gctUINT RecordSize
    )
{
    gcmHEADER_ARG("Container=0x%x RecordsPerContainer=%d RecordSize=%d",
        Container, RecordsPerContainer, RecordSize);

    gcmDEBUG_VERIFY_ARGUMENT(Container           != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(RecordsPerContainer != 0);
    gcmDEBUG_VERIFY_ARGUMENT(RecordSize          != 0);

    RecordSize += gcmSIZEOF(gcsCONTAINER_RECORD);

    Container->containerSize
        = gcmSIZEOF(gcsCONTAINER_LINK)
        + RecordSize * RecordsPerContainer;

    Container->recordSize  = RecordSize;
    Container->recordCount = RecordsPerContainer;

    Container->containers = gcvNULL;

    Container->freeList.next  = &Container->freeList;
    Container->freeList.prev  = &Container->freeList;
    Container->allocList.next = &Container->allocList;
    Container->allocList.prev = &Container->allocList;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gcsCONTAINER_Destroy(
    IN gcsCONTAINER_PTR Container
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsCONTAINER_LINK_PTR container;

    gcmHEADER_ARG("Container=0x%x", Container);
    gcmDEBUG_VERIFY_ARGUMENT(Container != gcvNULL);

    while (Container->containers != gcvNULL)
    {
        /* Get the current container. */
        container = Container->containers;

        /* Remove it from the list. */
        Container->containers = container->next;

        /* Free the current container. */
        gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, container));
    }

    Container->containerSize = 0;
    Container->recordSize    = 0;
    Container->recordCount   = 0;

    Container->freeList.next  = &Container->freeList;
    Container->freeList.prev  = &Container->freeList;
    Container->allocList.next = &Container->allocList;
    Container->allocList.prev = &Container->allocList;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcsCONTAINER_AllocateRecord(
    IN gcsCONTAINER_PTR Container,
    OUT gctPOINTER * Record
    )
{
    gceSTATUS status;
    gcsCONTAINER_RECORD_PTR currRecord;
    gcsCONTAINER_LINK_PTR container;
    gctUINT i;

    /* Are there free nodes available? */
    if (Container->freeList.next == &Container->freeList)
    {
        /* No, allocate an array of nodes. */
        gcmONERROR(gcoOS_Allocate(
            gcvNULL, Container->containerSize, (gctPOINTER *) &container
            ));

        /* Add to the list of containers. */
        container->next = Container->containers;
        Container->containers = container;

        /* Determine the pointer to the first layer item. */
        currRecord = (gcsCONTAINER_RECORD_PTR) (container + 1);

        /* Initialize free list. */
        for (i = 0; i < Container->recordCount; i += 1)
        {
            currRecord->prev = Container->freeList.prev;
            currRecord->next = &Container->freeList;
            Container->freeList.prev->next = currRecord;
            Container->freeList.prev       = currRecord;

            currRecord = (gcsCONTAINER_RECORD_PTR)
                ((gctUINT8_PTR) currRecord + Container->recordSize);
        }
    }

    /* Get the first free record. */
    currRecord = Container->freeList.next;

    /* Remove the record from free list. */
    currRecord->prev->next = currRecord->next;
    currRecord->next->prev = currRecord->prev;

    /* Add to allocated list. */
    currRecord->prev = &Container->allocList;
    currRecord->next = Container->allocList.next;
    Container->allocList.next->prev = currRecord;
    Container->allocList.next       = currRecord;

    /* Set return pointer. */
    * Record = currRecord + 1;

    /* Success. */
    return gcvSTATUS_OK;

    /* Return error. */
OnError:
    return status;
}

gceSTATUS
gcsCONTAINER_FreeRecord(
    IN gcsCONTAINER_PTR Container,
    IN gctPOINTER Record
    )
{
    gcsCONTAINER_RECORD_PTR currRecord;

    /* Get the record pointer */
    currRecord = (gcsCONTAINER_RECORD_PTR) Record - 1;

    /* Remove from allocated list. */
    currRecord->prev->next = currRecord->next;
    currRecord->next->prev = currRecord->prev;

    /* Add to free list. */
    currRecord->prev = &Container->freeList;
    currRecord->next = Container->freeList.next;
    Container->freeList.next->prev = currRecord;
    Container->freeList.next       = currRecord;

    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gcsCONTAINER_FreeAll(
    IN gcsCONTAINER_PTR Container
    )
{
    if (Container->allocList.next != &Container->allocList)
    {
        /* Move the entire allocated list to free list. */
        Container->allocList.prev->next = Container->freeList.next;
        Container->freeList.next->prev = Container->allocList.prev;
        Container->allocList.next->prev = &Container->freeList;
        Container->freeList.next = Container->allocList.next;

        /* Reset allocated list. */
        Container->allocList.prev = &Container->allocList;
        Container->allocList.next = &Container->allocList;
    }

    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gcoHAL_NameVideoMemory(
    IN gctUINT32 Handle,
    OUT gctUINT32 * Name
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    iface.command = gcvHAL_NAME_VIDEO_MEMORY;

    iface.u.NameVideoMemory.handle = Handle;

    status = gcoHAL_Call(gcvNULL, &iface);

    *Name = iface.u.NameVideoMemory.name;

    return status;
}

gceSTATUS
gcoHAL_ImportVideoMemory(
    IN gctUINT32 Name,
    OUT gctUINT32 * Handle
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    iface.command = gcvHAL_IMPORT_VIDEO_MEMORY;

    iface.u.ImportVideoMemory.name = Name;

    status = gcoHAL_Call(gcvNULL, &iface);

    *Handle = iface.u.ImportVideoMemory.handle;

    return status;
}

gceSTATUS
gcoHAL_GetVideoMemoryFd(
    IN gctUINT32 Handle,
    OUT gctINT * Fd
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Handle=%d", Handle);

    gcmVERIFY_ARGUMENT(Fd != gcvNULL);

    iface.command = gcvHAL_GET_VIDEO_MEMORY_FD;

    iface.u.GetVideoMemoryFd.handle = Handle;

    status = gcoHAL_Call(gcvNULL, &iface);

    *Fd = iface.u.GetVideoMemoryFd.fd;

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_GetHardware(
    IN gcoHAL Hal,
    OUT gcoHARDWARE* Hw
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoHARDWARE Hardware = gcvNULL;

    gcmHEADER_ARG("Hal=0x%x Hw=0x%x", Hal, Hw);

    *Hw = gcvNULL;

    gcmGETHARDWARE(Hardware);

OnError:
    *Hw = Hardware;

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_GetProductName(
    IN gcoHAL Hal,
    OUT gctSTRING *ProductName
    )
{
    gceSTATUS status= gcvSTATUS_OK;
    gcoHARDWARE hardware = gcvNULL;

    gcmHEADER_ARG("Hal=0x%x ProductName=0x%x", Hal, ProductName);

    gcmGETHARDWARE(hardware);

    status = gcoHARDWARE_GetProductName(hardware, ProductName);

    gcmFOOTER_ARG("return productName=%s", gcmOPT_VALUE(ProductName));

OnError:

    return status;
}


/*******************************************************************************
**
**  gcoHAL_CreateShBuffer
**
**  Create shared buffer.
**  The shared buffer can be used across processes. Other process needs call
**  gcoHAL_MapShBuffer before use it.
**
**  INPUT:
**
**      gctUINT32 Size
**          Specify the shared buffer size.
**
**  OUTPUT:
**
**      gctSHBUF * ShBuf
**          Pointer to hold return shared buffer handle.
*/
gceSTATUS
gcoHAL_CreateShBuffer(
    IN gctUINT32 Size,
    OUT gctSHBUF * ShBuf
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Size=%u", Size);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(ShBuf != gcvNULL);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command = gcvHAL_SHBUF;
    iface.u.ShBuf.command = gcvSHBUF_CREATE;
    iface.u.ShBuf.bytes   = Size;

    /* Call kernel driver. */
    gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                   IOCTL_GCHAL_INTERFACE,
                                   &iface, gcmSIZEOF(iface),
                                   &iface, gcmSIZEOF(iface)));

    /* Copy out shared buffer handle. */
    *ShBuf = (gctSHBUF)(gctUINTPTR_T) iface.u.ShBuf.id;

    /* Success. */
    gcmFOOTER_ARG("*ShBuf=%u", (gctUINT32)(gctUINTPTR_T) *ShBuf);
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoHAL_DestroyShBuffer
**
**  Destroy(dereference) specified shared buffer.
**  When a shared buffer is created by gcoHAL_CreateShBuffer, or mapped by
**  gcoHAL_MapShBuffer, it must be destroyed to free the resource.
**
**  INPUT:
**
**      gctSHBUF ShBuf
**          Specify the shared buffer to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_DestroyShBuffer(
    IN gctSHBUF ShBuf
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("ShBuf=%u", (gctUINT32)(gctUINTPTR_T) ShBuf);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command = gcvHAL_SHBUF;
    iface.u.ShBuf.command = gcvSHBUF_DESTROY;
    iface.u.ShBuf.id      = (gctUINT64)(gctUINTPTR_T) ShBuf;

    /* Call kernel driver. */
    status = gcoOS_DeviceControl(gcvNULL,
                                 IOCTL_GCHAL_INTERFACE,
                                 &iface, gcmSIZEOF(iface),
                                 &iface, gcmSIZEOF(iface));

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_MapShBuffer
**
**  Map shared buffer into this process so that it can be used in this process.
**  Call gcoHAL_DestroyShBuffer to destroy the map.
**
**  INPUT:
**
**      gctSHBUF ShBuf
**          Specify the shared buffer to be mapped.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_MapShBuffer(
    IN gctSHBUF ShBuf
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("ShBuf=%u", (gctUINT32)(gctUINTPTR_T) ShBuf);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(ShBuf != gcvNULL);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command = gcvHAL_SHBUF;
    iface.u.ShBuf.command = gcvSHBUF_MAP;
    iface.u.ShBuf.id      = (gctUINT64)(gctUINTPTR_T) ShBuf;

    /* Call kernel driver. */
    status = gcoOS_DeviceControl(gcvNULL,
                                 IOCTL_GCHAL_INTERFACE,
                                 &iface, gcmSIZEOF(iface),
                                 &iface, gcmSIZEOF(iface));

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_WriteShBuffer
**
**  Write data into shared buffer.
**
**  INPUT:
**
**      gctSHBUF ShBuf
**          Specify the shared buffer to be written to.
**
**      gctPOINTER Data
**          Pointer to hold the source data.
**
**      gctUINT32 ByteCount
**          Specify number of bytes to write. If this is larger than
**          shared buffer size, gcvSTATUS_INVALID_ARGUMENT is returned.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_WriteShBuffer(
    IN gctSHBUF ShBuf,
    IN gctCONST_POINTER Data,
    IN gctUINT32 ByteCount
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("ShBuf=%u Data=0x%08X ByteCount=%u",
                  (gctUINT32)(gctUINTPTR_T) ShBuf, Data, ByteCount);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command = gcvHAL_SHBUF;
    iface.u.ShBuf.command = gcvSHBUF_WRITE;
    iface.u.ShBuf.id      = (gctUINT64)(gctUINTPTR_T) ShBuf;
    iface.u.ShBuf.data    = (gctUINT64)(gctUINTPTR_T) Data;
    iface.u.ShBuf.bytes   = ByteCount;

    /* Call kernel driver. */
    status = gcoOS_DeviceControl(gcvNULL,
                                 IOCTL_GCHAL_INTERFACE,
                                 &iface, gcmSIZEOF(iface),
                                 &iface, gcmSIZEOF(iface));

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ReadShBuffer
**
**  Read data from shared buffer.
**
**  INPUT:
**
**      gctSHBUF ShBuf
**          Specify the shared buffer to be read from.
**
**      gctPOINTER Data
**          Pointer to save output data.
**
**      gctUINT32 ByteCount
**          Specify number of bytes to read.
**          If this is larger than shared buffer size, only avaiable bytes are
**          copied. If smaller, copy requested size.
**
**  OUTPUT:
**
**      gctUINT32 * BytesRead
**          Pointer to hold how many bytes actually read from shared buffer.
*/
gceSTATUS
gcoHAL_ReadShBuffer(
    IN gctSHBUF ShBuf,
    IN gctPOINTER Data,
    IN gctUINT32 ByteCount,
    OUT gctUINT32 * BytesRead
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("ShBuf=%u Data=0x%08X ByteCount=%u",
                  (gctUINT32)(gctUINTPTR_T) ShBuf, Data, ByteCount);

    /* Initialize the gcsHAL_INTERFACE structure. */
    iface.command = gcvHAL_SHBUF;
    iface.u.ShBuf.command = gcvSHBUF_READ;
    iface.u.ShBuf.id      = (gctUINT64)(gctUINTPTR_T) ShBuf;
    iface.u.ShBuf.data    = (gctUINT64)(gctUINTPTR_T) Data;
    iface.u.ShBuf.bytes   = ByteCount;

    /* Call kernel driver. */
    gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                   IOCTL_GCHAL_INTERFACE,
                                   &iface, gcmSIZEOF(iface),
                                   &iface, gcmSIZEOF(iface)));

    /* Return copied size. */
    *BytesRead = iface.u.ShBuf.bytes;

    /* Success. */
    gcmFOOTER_ARG("*BytesRead=%u", *BytesRead);
    return status;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ConfigPowerManagement
**
**  Enable/disable power management. After power management is disabled, there is no auto IDLE/SUSPEND/OFF.
**
**  Input:
**
**      Enable.
**          Enable or disable power management.
**
*/
gceSTATUS
gcoHAL_ConfigPowerManagement(
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;
    gcmHEADER_ARG("%d", Enable);

    iface.command = gcvHAL_CONFIG_POWER_MANAGEMENT;
    iface.u.ConfigPowerManagement.enable = Enable;

    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_AllocateVideoMemory(
    IN gctUINT Alignment,
    IN gceSURF_TYPE Type,
    IN gctUINT32 Flag,
    IN gcePOOL Pool,
    IN OUT gctSIZE_T * Bytes,
    OUT gctUINT32_PTR Node
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;
    struct _gcsHAL_ALLOCATE_LINEAR_VIDEO_MEMORY * alvm
        = (struct _gcsHAL_ALLOCATE_LINEAR_VIDEO_MEMORY *) &iface.u;

    gcmHEADER_ARG("Node=%p, Bytes=%llu, Alignement=%d, Type=%d, Flag=%d, Pool=%d",
                  Node, Bytes, Alignment, Type, Flag, Pool);

    iface.command   = gcvHAL_ALLOCATE_LINEAR_VIDEO_MEMORY;

    gcmSAFECASTSIZET(alvm->bytes, *Bytes);

    alvm->alignment = Alignment;
    alvm->type      = Type;
    alvm->pool      = Pool;
    alvm->flag      = Flag;

    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    *Node  = alvm->node;
    *Bytes = alvm->bytes;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_LockVideoMemory(
    IN gctUINT32 Node,
    IN gctBOOL Cacheable,
    OUT gctUINT32 * Physical,
    OUT gctPOINTER * Logical
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Node=%d, Cacheable=%d", Node, Cacheable);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Node != 0);

    /* Fill in the kernel call structure. */
    iface.command = gcvHAL_LOCK_VIDEO_MEMORY;
    iface.u.LockVideoMemory.node = Node;
    iface.u.LockVideoMemory.cacheable = Cacheable;

    /* Call the kernel. */
    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    if (Physical)
    {
        /* Return physical address. */
        *Physical = iface.u.LockVideoMemory.address;
    }

    if (Logical)
    {
        /* Return logical address. */
        *Logical = gcmUINT64_TO_PTR(iface.u.LockVideoMemory.memory);
    }

    /* Success. */
    gcmFOOTER_ARG("*Physical=0x%x *Logical=0x%x", *Physical, *Logical);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_UnlockVideoMemory(
    IN gctUINT32 Node,
    IN gceSURF_TYPE Type
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_INTERFACE iface;
    gcmHEADER_ARG("Node=0x%x", Node);

    iface.command = gcvHAL_UNLOCK_VIDEO_MEMORY;
    iface.u.UnlockVideoMemory.node = Node;
    iface.u.UnlockVideoMemory.type = Type;

    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    /* Schedule an event for the unlock. */
    gcmONERROR(gcoHARDWARE_CallEvent(gcvNULL, &iface));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoHAL_ReleaseVideoMemory(
    IN gctUINT32 Node
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("Node=0x%x", Node);

    /* Release the allocated video memory synchronously. */
    iface.command = gcvHAL_RELEASE_VIDEO_MEMORY;
    iface.u.ReleaseVideoMemory.node = Node;

    /* Call kernel HAL. */
    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

