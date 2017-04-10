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
**  gcoSURF object for user HAL layers.
**
*/

#include "gc_hal_user_precomp.h"

#define _GC_OBJ_ZONE    gcvZONE_SURFACE

/******************************************************************************\
**************************** gcoSURF API Support Code **************************
\******************************************************************************/

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoSURF_LockTileStatus
**
**  Locked tile status buffer of surface
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_LockTileStatus(
    IN gcoSURF Surface
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Lock the tile status buffer. */
        gcmONERROR(
            gcoHARDWARE_Lock(&Surface->info.tileStatusNode,
                             gcvNULL,
                             gcvNULL));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Locked tile status 0x%x: physical=0x%08X logical=0x%x "
                      "lockedCount=%d",
                      &Surface->info.tileStatusNode,
                      Surface->info.tileStatusNode.physical,
                      Surface->info.tileStatusNode.logical,
                      Surface->info.tileStatusNode.lockCount);

        /* Only 1 address. */
        Surface->info.tileStatusNode.count = 1;

        /* Check if this is the first lock. */
        if (Surface->info.tileStatusNode.firstLock)
        {
            /* Fill the tile status memory with the filler. */
            gcoOS_MemFill(Surface->info.tileStatusNode.logical,
                          (gctUINT8) Surface->info.tileStatusFiller,
                          Surface->info.tileStatusNode.size);

            /* Flush the node from cache. */
            gcmONERROR(
                gcoSURF_NODE_Cache(&Surface->info.tileStatusNode,
                                 Surface->info.tileStatusNode.logical,
                                 Surface->info.tileStatusNode.size,
                                 gcvCACHE_CLEAN));

            /* Dump the memory write. */
            gcmDUMP_BUFFER(gcvNULL,
                           "memory",
                           Surface->info.tileStatusNode.physical,
                           Surface->info.tileStatusNode.logical,
                           0,
                           Surface->info.tileStatusNode.size);

#if gcdDUMP
            if (Surface->info.tileStatusFiller == 0x0)
            {
                gcmDUMP_BUFFER(gcvNULL,
                               "memory",
                               Surface->info.node.physical,
                               Surface->info.node.logical,
                               0,
                               Surface->info.node.size);
            }
#endif

            /* No longer first lock. */
            Surface->info.tileStatusNode.firstLock = gcvFALSE;
        }
    }

    /* Lock the hierarchical Z tile status buffer. */
    if (Surface->info.hzTileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Lock the tile status buffer. */
        gcmONERROR(
            gcoHARDWARE_Lock(&Surface->info.hzTileStatusNode,
                             gcvNULL,
                             gcvNULL));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Locked HZ tile status 0x%x: physical=0x%08X logical=0x%x "
                      "lockedCount=%d",
                      &Surface->info.hzTileStatusNode,
                      Surface->info.hzTileStatusNode.physical,
                      Surface->info.hzTileStatusNode.logical,
                      Surface->info.hzTileStatusNode.lockCount);

        /* Only 1 address. */
        Surface->info.hzTileStatusNode.count = 1;

        /* Check if this is the first lock. */
        if (Surface->info.hzTileStatusNode.firstLock)
        {
            /* Fill the tile status memory with the filler. */
            gcoOS_MemFill(Surface->info.hzTileStatusNode.logical,
                          (gctUINT8) Surface->info.hzTileStatusFiller,
                          Surface->info.hzTileStatusNode.size);

            /* Flush the node from cache. */
            gcmONERROR(
                gcoSURF_NODE_Cache(&Surface->info.hzTileStatusNode,
                                 Surface->info.hzTileStatusNode.logical,
                                 Surface->info.hzTileStatusNode.size,
                                 gcvCACHE_CLEAN));

            /* Dump the memory write. */
            gcmDUMP_BUFFER(gcvNULL,
                           "memory",
                           Surface->info.hzTileStatusNode.physical,
                           Surface->info.hzTileStatusNode.logical,
                           0,
                           Surface->info.hzTileStatusNode.size);

            /* No longer first lock. */
            Surface->info.hzTileStatusNode.firstLock = gcvFALSE;
        }
    }
OnError:
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_LockHzBuffer
**
**  Locked HZ buffer buffer of surface
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_LockHzBuffer(
    IN gcoSURF Surface
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Surface->info.hzNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmONERROR(
            gcoHARDWARE_Lock(&Surface->info.hzNode,
                             gcvNULL,
                             gcvNULL));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Locked HZ surface 0x%x: physical=0x%08X logical=0x%x "
                      "lockCount=%d",
                      &Surface->info.hzNode,
                      Surface->info.hzNode.physical,
                      Surface->info.hzNode.logical,
                      Surface->info.hzNode.lockCount);

        /* Only 1 address. */
        Surface->info.hzNode.count = 1;
    }
OnError:
    gcmFOOTER_NO();
    return status;

}

/*******************************************************************************
**
**  gcoSURF_Construct
**
**  Allocate tile status buffer for surface
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_AllocateTileStatus(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;
    gctSIZE_T bytes;
    gctUINT alignment;
    gctBOOL tileStatusInVirtual;

#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /* No tile status buffer allocated. */
    Surface->info.tileStatusNode.pool             = gcvPOOL_UNKNOWN;
    Surface->info.hzTileStatusNode.pool           = gcvPOOL_UNKNOWN;

    /* Set tile status disabled at the beginging to be consistent with POOL value */
    Surface->info.tileStatusDisabled = gcvTRUE;
    Surface->info.dirty = gcvTRUE;

    tileStatusInVirtual = gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_MC20);

    /*   Verify if the type requires a tile status buffer:
    ** - do not turn on fast clear if the surface is virtual;
    ** - for user pools we don't have the address of the surface yet,
    **   delay tile status determination until we map the surface.
    */
    if ((Surface->info.node.pool == gcvPOOL_USER) ||
        ((Surface->info.node.pool == gcvPOOL_VIRTUAL) && !tileStatusInVirtual))
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if ((Surface->info.type != gcvSURF_RENDER_TARGET) &&
        (Surface->info.type != gcvSURF_DEPTH))
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if (Surface->info.hints & gcvSURF_NO_TILE_STATUS)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if ((Surface->info.formatInfo.fakedFormat &&
        !Surface->info.paddingFormat
        ) ||
        ((Surface->info.bitsPerPixel > 32) &&
         (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_64BPP_HW_CLEAR_SUPPORT) == gcvFALSE)
        ) ||
        (Surface->info.bitsPerPixel < 16)
       )
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /* Can't support multi-slice surface*/
    if (Surface->depth > 1)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }


#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
    gcmASSERT(currentType != gcvHARDWARE_VG);
#endif

    /* Set default fill color. */
    switch (Surface->info.format)
    {
    case gcvSURF_D16:
        Surface->info.clearValue[0] =
        Surface->info.fcValue       = 0xFFFFFFFF;
        gcmONERROR(gcoHARDWARE_HzClearValueControl(Surface->info.format,
                                                   Surface->info.fcValue,
                                                   &Surface->info.fcValueHz,
                                                   gcvNULL));
        break;

    case gcvSURF_D24X8:
    case gcvSURF_D24S8:
        Surface->info.clearValue[0] =
        Surface->info.fcValue       = 0xFFFFFF00;
        gcmONERROR(gcoHARDWARE_HzClearValueControl(Surface->info.format,
                                                   Surface->info.fcValue,
                                                   &Surface->info.fcValueHz,
                                                   gcvNULL));
        break;

    case gcvSURF_S8:
        Surface->info.clearValue[0] =
        Surface->info.fcValue       = 0x00000000;
        break;

    case gcvSURF_R8_1_X8R8G8B8:
    case gcvSURF_G8R8_1_X8R8G8B8:
        Surface->info.clearValue[0]      =
        Surface->info.clearValueUpper[0] =
        Surface->info.fcValue            =
        Surface->info.fcValueUpper       = 0xFF000000;
        break;

    default:
        Surface->info.clearValue[0]      =
        Surface->info.clearValueUpper[0] =
        Surface->info.fcValue            =
        Surface->info.fcValueUpper       = 0x00000000;
        break;
    }

    /* Query the linear size for the tile status buffer. */
    status = gcoHARDWARE_QueryTileStatus(gcvNULL,
                                         Surface->info.alignedWidth,
                                         Surface->info.alignedHeight,
                                         Surface->info.size,
                                         &bytes,
                                         &alignment,
                                         &Surface->info.tileStatusFiller);

    /* Tile status supported? */
    if ((status == gcvSTATUS_NOT_SUPPORTED) || (0 == bytes))
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }
    else if (gcmIS_ERROR(status))
    {
        gcmFOOTER_NO();
        return status;
    }

    if (Surface->info.TSDirty)
    {
        Surface->info.tileStatusFiller = 0x0;
        Surface->info.dirty = gcvFALSE;
        Surface->info.TSDirty = gcvFALSE;
    }

    /* Copy filler. */
    Surface->info.hzTileStatusFiller = Surface->info.tileStatusFiller;

    if (!(Surface->info.hints & gcvSURF_NO_VIDMEM))
    {
        /* Allocate the tile status buffer. */
        status = gcsSURF_NODE_Construct(
            &Surface->info.tileStatusNode,
            bytes,
            alignment,
            gcvSURF_TILE_STATUS,
            gcvALLOC_FLAG_NONE,
            gcvPOOL_DEFAULT
            );

        if (gcmIS_ERROR(status))
        {
            /* Commit any command buffer and wait for idle hardware. */
            status = gcoHAL_Commit(gcvNULL, gcvTRUE);

            if (gcmIS_SUCCESS(status))
            {
                /* Try allocating again. */
                status = gcsSURF_NODE_Construct(
                    &Surface->info.tileStatusNode,
                    bytes,
                    alignment,
                    gcvSURF_TILE_STATUS,
                    gcvALLOC_FLAG_NONE,
                    gcvPOOL_DEFAULT
                    );
            }
        }
    }

    if (gcmIS_SUCCESS(status))
    {
        /* When allocate successfully, set tile status is enabled for this surface by default.
        ** Logically, we should disable tile status buffer initially.
        ** But for MSAA, we always enable FC, otherwise it will hang up on hw.
        ** So for non-cleared we also need enable FC by default.
        */
        Surface->info.tileStatusDisabled = gcvFALSE;

        /* Only set garbagePadded=0 if by default cleared tile status. */
        if (Surface->info.paddingFormat)
        {
            Surface->info.garbagePadded = gcvFALSE;
        }

        if (!(Surface->info.hints & gcvSURF_NO_COMPRESSION))
        {
            /*
            ** Get surface compression setting.
            */
            gcoHARDWARE_QueryCompression(gcvNULL,
                                         &Surface->info,
                                         &Surface->info.compressed,
                                         &Surface->info.compressFormat);

            gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                          "Allocated tile status 0x%x: pool=%d size=%u",
                          &Surface->info.tileStatusNode,
                          Surface->info.tileStatusNode.pool,
                          Surface->info.tileStatusNode.size);
        }

        /* Allocate tile status for hierarchical Z buffer. */
        if (Surface->info.hzNode.pool != gcvPOOL_UNKNOWN)
        {
            /* Query the linear size for the tile status buffer. */
            status = gcoHARDWARE_QueryTileStatus(gcvNULL,
                                                 0,
                                                 0,
                                                 Surface->info.hzNode.size,
                                                 &bytes,
                                                 &alignment,
                                                 gcvNULL);

            /* Tile status supported? */
            if (status == gcvSTATUS_NOT_SUPPORTED)
            {
                return gcvSTATUS_OK;
            }

            if (!(Surface->info.hints & gcvSURF_NO_VIDMEM))
            {
                status = gcsSURF_NODE_Construct(
                             &Surface->info.hzTileStatusNode,
                             bytes,
                             alignment,
                             gcvSURF_TILE_STATUS,
                             gcvALLOC_FLAG_NONE,
                             gcvPOOL_DEFAULT
                             );
            }

            if (gcmIS_SUCCESS(status))
            {
                gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                              "Allocated HZ tile status 0x%x: pool=%d size=%u",
                              &Surface->info.hzTileStatusNode,
                              Surface->info.hzTileStatusNode.pool,
                              Surface->info.hzTileStatusNode.size);
            }
        }
    }

OnError:
    gcmFOOTER_NO();
    /* Return the status. */
    return status;
}


/*******************************************************************************
**
**  gcoSURF_Construct
**
**  Allocate HZ buffer for surface
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_AllocateHzBuffer(
    IN gcoSURF Surface
    )
{
    gcePOOL  pool;
    gceSTATUS status;
    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Surface->info.hzNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    pool = Surface->info.node.pool;

    /* No Hierarchical Z buffer allocated. */
    Surface->info.hzNode.pool = gcvPOOL_UNKNOWN;

    Surface->info.hzDisabled = gcvTRUE;

    /* Can't support multi-slice surface*/
    if (Surface->depth > 1)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /* Check if this is a depth buffer and the GPU supports hierarchical Z. */
    if ((Surface->info.type == gcvSURF_DEPTH) &&
        (Surface->info.format != gcvSURF_S8) &&
        (pool != gcvPOOL_USER) &&
        ((Surface->info.hints & gcvSURF_NO_VIDMEM) == 0) &&
        (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_HZ) == gcvSTATUS_TRUE))
    {
        gctSIZE_T bytes;
        gctUINT32 sizeAlignment = 32 * 32 * 4
                                * ((Surface->info.tiling & gcvTILING_SPLIT_BUFFER) ? 2 : 1);

        gctSIZE_T unalignedBytes = (Surface->info.size + 63)/64 * 4;

        /* Compute the hierarchical Z buffer size.  Allocate enough for
        ** 16-bit min/max values. */
        if (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_NEW_RA) == gcvSTATUS_TRUE)
        {
            bytes = gcmALIGN(unalignedBytes / 2, sizeAlignment);
        }
        else
        {
            bytes = gcmALIGN(unalignedBytes, sizeAlignment);
        }

        /* Allocate the hierarchical Z buffer. */
        status = gcsSURF_NODE_Construct(
                    &Surface->info.hzNode,
                    bytes,
                    64,
                    gcvSURF_HIERARCHICAL_DEPTH,
                    gcvALLOC_FLAG_NONE,
                    pool
                    );

        if (gcmIS_SUCCESS(status))
        {
            gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                          "Allocated HZ surface 0x%x: pool=%d size=%u",
                          &Surface->info.hzNode,
                          Surface->info.hzNode.pool,
                          Surface->info.hzNode.size);
        }
    }

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

}
#endif

static gceSTATUS
_Lock(
#if gcdENABLE_VG
    IN gcoSURF Surface,
    IN gceHARDWARE_TYPE CurrentType
#else
    IN gcoSURF Surface
#endif
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    if (CurrentType == gcvHARDWARE_VG)
    {
        gcmONERROR(
            gcoVGHARDWARE_Lock(gcvNULL,
                         &Surface->info.node,
                         gcvNULL,
                         gcvNULL));
    }
    else
#endif
    {
        /* Lock the video memory. */
        gcmONERROR(
            gcoHARDWARE_Lock(
                         &Surface->info.node,
                         gcvNULL,
                         gcvNULL));
    }

    Surface->info.node.physicalBottom = Surface->info.node.physical + Surface->info.bottomBufferOffset;
    Surface->info.node.logicalBottom  = Surface->info.node.logical  + Surface->info.bottomBufferOffset;

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                  "Locked surface 0x%x: physical=0x%08X logical=0x%x lockCount=%d",
                  &Surface->info.node,
                  Surface->info.node.physical,
                  Surface->info.node.logical,
                  Surface->info.node.lockCount);

#if gcdENABLE_3D
    /* Lock the hierarchical Z node. */
    gcmONERROR(gcoSURF_LockHzBuffer(Surface));

    /* Lock the tile status buffer and hierarchical Z tile status buffer. */
    gcmONERROR(gcoSURF_LockTileStatus(Surface));

#endif /* gcdENABLE_3D */
    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    return status;
}

static gceSTATUS
_Unlock(
#if gcdENABLE_VG
    IN gcoSURF Surface,
    IN gceHARDWARE_TYPE CurrentType
#else
    IN gcoSURF Surface
#endif
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    if (CurrentType == gcvHARDWARE_VG)
    {
        /* Unlock the surface. */
        gcmONERROR(
            gcoVGHARDWARE_Unlock(gcvNULL,
                               &Surface->info.node,
                               Surface->info.type));
    }
    else
#endif
    {
        /* Unlock the surface. */
        gcmONERROR(
            gcoHARDWARE_Unlock(
                               &Surface->info.node,
                               Surface->info.type));
    }

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                  "Unlocked surface 0x%x: lockCount=%d",
                  &Surface->info.node,
                  Surface->info.node.lockCount);

#if gcdENABLE_3D
    /* Unlock the hierarchical Z buffer. */
    if (Surface->info.hzNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmONERROR(
            gcoHARDWARE_Unlock(&Surface->info.hzNode,
                               gcvSURF_HIERARCHICAL_DEPTH));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Unlocked HZ surface 0x%x: lockCount=%d",
                      &Surface->info.hzNode,
                      Surface->info.hzNode.lockCount);
    }

    /* Unlock the tile status buffer. */
    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmONERROR(
            gcoHARDWARE_Unlock(&Surface->info.tileStatusNode,
                               gcvSURF_TILE_STATUS));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Unlocked tile status 0x%x: lockCount=%d",
                      &Surface->info.hzNode,
                      Surface->info.hzNode.lockCount);
    }

    /* Unlock the hierarchical tile status buffer. */
    if (Surface->info.hzTileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        gcmONERROR(
            gcoHARDWARE_Unlock(&Surface->info.hzTileStatusNode,
                               gcvSURF_TILE_STATUS));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Unlocked HZ tile status 0x%x: lockCount=%d",
                      &Surface->info.hzNode,
                      Surface->info.hzNode.lockCount);
    }
#endif /* gcdENABLE_3D */

    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Return the error. */
    return status;
}

static gceSTATUS
_FreeSurface(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
#endif

#if gcdSYNC
    {
        gcsSYNC_CONTEXT_PTR ptr = Surface->info.fenceCtx;

        while(ptr)
        {
           Surface->info.fenceCtx = ptr->next;
           gcmONERROR(gcoOS_Free(gcvNULL,ptr));
           ptr = Surface->info.fenceCtx;
        }
    }
#endif

    /* We only manage valid and non-user pools. */
    if ((Surface->info.node.pool != gcvPOOL_UNKNOWN)
    &&  (Surface->info.node.pool != gcvPOOL_USER)
    )
    {
#if gcdENABLE_VG
        /* Unlock the video memory. */
        gcmONERROR(_Unlock(Surface, currentType));

        if (currentType == gcvHARDWARE_VG)
        {
            /* Free the video memory. */
            gcmONERROR(
                gcoVGHARDWARE_ScheduleVideoMemory(gcvNULL, Surface->info.node.u.normal.node, gcvFALSE));
        }
        else
        {
            /* Free the video memory. */
            gcmONERROR(
                gcoHARDWARE_ScheduleVideoMemory(&Surface->info.node));
        }
#else
        /* Unlock the video memory. */
        gcmONERROR(_Unlock(Surface));

        /* Free the video memory. */
        gcmONERROR(
            gcoHARDWARE_ScheduleVideoMemory(&Surface->info.node));
#endif
        /* Mark the memory as freed. */
        Surface->info.node.pool = gcvPOOL_UNKNOWN;

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Freed surface 0x%x",
                      &Surface->info.node);
    }

#if gcdENABLE_3D
    if (Surface->info.hzNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Free the hierarchical Z video memory. */
        gcmONERROR(
            gcoHARDWARE_ScheduleVideoMemory(&Surface->info.hzNode));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Freed HZ surface 0x%x",
                      &Surface->info.hzNode);

        /* Mark the memory as freed. */
        Surface->info.hzNode.pool = gcvPOOL_UNKNOWN;
    }

    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Free the tile status memory. */
        gcmONERROR(
            gcoHARDWARE_ScheduleVideoMemory(&Surface->info.tileStatusNode));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Freed tile status 0x%x",
                      &Surface->info.tileStatusNode);

        /* Mark the tile status as freed. */
        Surface->info.tileStatusNode.pool = gcvPOOL_UNKNOWN;
    }

    if (Surface->info.hzTileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Free the hierarchical Z tile status memory. */
        gcmONERROR(
            gcoHARDWARE_ScheduleVideoMemory(&Surface->info.hzTileStatusNode));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Freed HZ tile status 0x%x",
                      &Surface->info.hzTileStatusNode);

        /* Mark the tile status as freed. */
        Surface->info.hzTileStatusNode.pool = gcvPOOL_UNKNOWN;
    }
#endif /* gcdENABLE_3D */

    if (Surface->info.shBuf != gcvNULL)
    {
        /* Destroy shared buffer. */
        gcmVERIFY_OK(
            gcoHAL_DestroyShBuffer(Surface->info.shBuf));

        /* Mark it as freed. */
        Surface->info.shBuf = gcvNULL;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
#if gcdENABLE_BUFFER_ALIGNMENT

#if gcdENABLE_BANK_ALIGNMENT

#if !gcdBANK_BIT_START
#error gcdBANK_BIT_START not defined.
#endif

#if !gcdBANK_BIT_END
#error gcdBANK_BIT_END not defined.
#endif
#endif

/*******************************************************************************
**  _GetBankOffsetBytes
**
**  Return the bytes needed to offset sub-buffers to different banks.
**
**  ARGUMENTS:
**
**      gceSURF_TYPE Type
**          Type of buffer.
**
**      gctUINT32 TopBufferSize
**          Size of the top buffer, need\ed to compute offset of the second buffer.
**
**  OUTPUT:
**
**      gctUINT32_PTR Bytes
**          Pointer to a variable that will receive the byte offset needed.
**
*/
gceSTATUS
_GetBankOffsetBytes(
    IN gcoSURF Surface,
    IN gceSURF_TYPE Type,
    IN gctUINT32 TopBufferSize,
    OUT gctUINT32_PTR Bytes
    )

{
    gctUINT32 baseOffset = 0;
    gctUINT32 offset     = 0;

#if gcdENABLE_BANK_ALIGNMENT
    gctUINT32 bank;
    /* To retrieve the bank. */
    static const gctUINT32 bankMask = (0xFFFFFFFF << gcdBANK_BIT_START)
                                    ^ (0xFFFFFFFF << (gcdBANK_BIT_END + 1));
#endif

    gcmHEADER_ARG("Type=%d TopBufferSize=%x Bytes=0x%x", Type, TopBufferSize, Bytes);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Bytes != gcvNULL);

    switch(Type)
    {
    case gcvSURF_RENDER_TARGET:
        /* Put second buffer atleast 16KB away. */
        baseOffset = offset = (1 << 14);

#if gcdENABLE_BANK_ALIGNMENT
        TopBufferSize += (1 << 14);
        bank = (TopBufferSize & bankMask) >> (gcdBANK_BIT_START);

        /* Put second buffer (c1 or z1) 5 banks away. */
        if (bank <= 5)
        {
            offset += (5 - bank) << (gcdBANK_BIT_START);
        }
        else
        {
            offset += (8 + 5 - bank) << (gcdBANK_BIT_START);
        }
#if gcdBANK_CHANNEL_BIT
        /* Minimum 256 byte alignment needed for fast_msaa or small msaa. */
        if ((gcdBANK_CHANNEL_BIT > 7) ||
            ((gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_FAST_MSAA) != gcvSTATUS_TRUE) &&
             (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_SMALL_MSAA) != gcvSTATUS_TRUE)))

        {
            /* Add a channel offset at the channel bit. */
            offset += (1 << gcdBANK_CHANNEL_BIT);
        }

#endif
#endif
        break;

    case gcvSURF_DEPTH:
        /* Put second buffer atleast 16KB away. */
        baseOffset = offset = (1 << 14);

#if gcdENABLE_BANK_ALIGNMENT
        TopBufferSize += (1 << 14);
        bank = (TopBufferSize & bankMask) >> (gcdBANK_BIT_START);

        /* Put second buffer (c1 or z1) 5 banks away. */
        if (bank <= 5)
        {
            offset += (5 - bank) << (gcdBANK_BIT_START);
        }
        else
        {
            offset += (8 + 5 - bank) << (gcdBANK_BIT_START);
        }

#if gcdBANK_CHANNEL_BIT
        /* Subtract the channel bit, as it's added by kernel side. */
        if (offset >= (1 << gcdBANK_CHANNEL_BIT))
        {
            /* Minimum 256 byte alignment needed for fast_msaa or small msaa. */
            if ((gcdBANK_CHANNEL_BIT > 7) ||
                ((gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_FAST_MSAA) != gcvSTATUS_TRUE) &&
                 (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_SMALL_MSAA) != gcvSTATUS_TRUE)))
            {
                offset -= (1 << gcdBANK_CHANNEL_BIT);
            }
        }
#endif
#endif

        break;

    default:
        /* No alignment needed. */
        baseOffset = offset = 0;
    }

    *Bytes = offset;

    /* Avoid compiler warnings. */
    baseOffset = baseOffset;

    /* Only disable bottom-buffer-offset on android system. */
#if gcdPARTIAL_FAST_CLEAR && defined(ANDROID)
    if (!(Surface->info.samples.x > 1 || Surface->info.samples.y > 1))
    {
        /* In NOAA mode, disable extra bottom-buffer-offset if want
         * partial fast clear. 'baseOffset' is 16KB aligned, it can still
         * support. */
        *Bytes = baseOffset;
    }
#endif

    gcmFOOTER_ARG("*Bytes=0x%x", *Bytes);
    return gcvSTATUS_OK;
}

#endif
#endif

static void
_ComputeSurfacePlacement(
    gcoSURF Surface
    )
{
    gctUINT32 blockSize;
    gcsSURF_FORMAT_INFO_PTR formatInfo = &Surface->info.formatInfo;
    blockSize = formatInfo->blockSize / formatInfo->layers;

    switch (Surface->info.format)
    {
    case gcvSURF_YV12:
        /*  WxH Y plane followed by (W/2)x(H/2) V and U planes. */
        Surface->info.stride
            = Surface->info.alignedWidth;

        Surface->info.uStride =
        Surface->info.vStride
#if defined(ANDROID)
            /*
             * Per google's requirement, we need u/v plane align to 16,
             * and there is no gap between YV plane
             */
            = (Surface->info.alignedWidth / 2 + 0xf) & ~0xf;
#else
            = (Surface->info.alignedWidth / 2);
#endif

        Surface->info.vOffset
            = Surface->info.stride * Surface->info.alignedHeight;

        Surface->info.uOffset
            = Surface->info.vOffset
            + Surface->info.vStride * Surface->info.alignedHeight / 2;

        Surface->info.sliceSize
            = Surface->info.uOffset
            + Surface->info.uStride * Surface->info.alignedHeight / 2;
        break;

    case gcvSURF_I420:
        /*  WxH Y plane followed by (W/2)x(H/2) U and V planes. */
        Surface->info.stride
            = Surface->info.alignedWidth;

        Surface->info.uStride =
        Surface->info.vStride
#if defined(ANDROID)
            /*
             * Per google's requirement, we need u/v plane align to 16,
             * and there is no gap between YV plane
             */
            = (Surface->info.alignedWidth / 2 + 0xf) & ~0xf;
#else
            = (Surface->info.alignedWidth / 2);
#endif

        Surface->info.uOffset
            = Surface->info.stride * Surface->info.alignedHeight;

        Surface->info.vOffset
            = Surface->info.uOffset
            + Surface->info.uStride * Surface->info.alignedHeight / 2;

        Surface->info.sliceSize
            = Surface->info.vOffset
            + Surface->info.vStride * Surface->info.alignedHeight / 2;
        break;

    case gcvSURF_NV12:
    case gcvSURF_NV21:
        /*  WxH Y plane followed by (W)x(H/2) interleaved U/V plane. */
        Surface->info.stride  =
        Surface->info.uStride =
        Surface->info.vStride
            = Surface->info.alignedWidth;

        Surface->info.uOffset =
        Surface->info.vOffset
            = Surface->info.stride * Surface->info.alignedHeight;

        Surface->info.sliceSize
            = Surface->info.uOffset
            + Surface->info.uStride * Surface->info.alignedHeight / 2;
        break;

    case gcvSURF_NV16:
    case gcvSURF_NV61:
        /*  WxH Y plane followed by WxH interleaved U/V(V/U) plane. */
        Surface->info.stride  =
        Surface->info.uStride =
        Surface->info.vStride
            = Surface->info.alignedWidth;

        Surface->info.uOffset =
        Surface->info.vOffset
            = Surface->info.stride * Surface->info.alignedHeight;

        Surface->info.sliceSize
            = Surface->info.uOffset
            + Surface->info.uStride * Surface->info.alignedHeight;
        break;

    case gcvSURF_YUY2:
    case gcvSURF_UYVY:
    case gcvSURF_YVYU:
    case gcvSURF_VYUY:
        /*  WxH interleaved Y/U/Y/V plane. */
        Surface->info.stride  =
        Surface->info.uStride =
        Surface->info.vStride
            = Surface->info.alignedWidth * 2;

        Surface->info.uOffset = Surface->info.vOffset = 0;

        Surface->info.sliceSize
            = Surface->info.stride * Surface->info.alignedHeight;
        break;

    default:
        Surface->info.stride
            = (Surface->info.alignedWidth / formatInfo->blockWidth)
            * blockSize / 8;

        Surface->info.uStride = Surface->info.vStride = 0;

        Surface->info.uOffset = Surface->info.vOffset = 0;

        Surface->info.sliceSize
            = (Surface->info.alignedWidth  / formatInfo->blockWidth)
            * (Surface->info.alignedHeight / formatInfo->blockHeight)
            * blockSize / 8;
        break;
    }
}

static gceSTATUS
_AllocateSurface(
    IN gcoSURF Surface,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN gcePOOL Pool
    )
{
    gceSTATUS status;
    gceSURF_FORMAT format;
    gcsSURF_FORMAT_INFO_PTR formatInfo;
    /* Extra pages needed to offset sub-buffers to different banks. */
    gctUINT32 bankOffsetBytes = 0;
    gctUINT32 layers;
#if gcdENABLE_3D
    gctUINT32 blockSize;
#endif

#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;

    gcmGETCURRENTHARDWARE(currentType);
#endif

    format = (gceSURF_FORMAT) (Format & ~gcvSURF_FORMAT_OCL);
    gcmONERROR(gcoSURF_QueryFormat(format, &formatInfo));
    Surface->info.formatInfo = *formatInfo;
    layers = formatInfo->layers;
#if gcdENABLE_3D
    blockSize = formatInfo->blockSize / layers;
#endif

#if gcdENABLE_VG
    if (currentType == gcvHARDWARE_VG)
    {
        /* Compute bits per pixel. */
        gcmONERROR(
            gcoVGHARDWARE_ConvertFormat(gcvNULL,
                                        format,
                                        (gctUINT32_PTR)&Surface->info.bitsPerPixel,
                                        gcvNULL));
    }
    else
#endif
    {
        Surface->info.bitsPerPixel = formatInfo->bitsPerPixel;
    }

    /* Set dimensions of surface. */
    Surface->info.rect.left   = 0;
    Surface->info.rect.top    = 0;
    Surface->info.rect.right  = Width;
    Surface->info.rect.bottom = Height;

    /* Set the number of planes. */
    Surface->depth = Depth;

    /* Initialize rotation. */
    Surface->info.rotation    = gcvSURF_0_DEGREE;
#if gcdENABLE_3D
    Surface->info.orientation = gcvORIENTATION_TOP_BOTTOM;
    Surface->resolvable       = gcvTRUE;
#endif

    /* Obtain canonical type of surface. */
    Surface->info.type   = (gceSURF_TYPE) ((gctUINT32) Type & 0xFF);
    /* Get 'hints' of this surface. */
    Surface->info.hints  = (gceSURF_TYPE) ((gctUINT32) Type & ~0xFF);
    /* Append texture surface flag */
    Surface->info.hints |= (Surface->info.type == gcvSURF_TEXTURE) ? gcvSURF_CREATE_AS_TEXTURE : 0;
    /* Set format of surface. */
    Surface->info.format = format;
    Surface->info.tiling = ((Type & gcvSURF_TEXTURE) == gcvSURF_TEXTURE) ? ((Type & gcvSURF_LINEAR) ? gcvLINEAR : gcvTILED): gcvLINEAR;

    /* Set aligned surface size. */
    Surface->info.alignedWidth  = Width;
    Surface->info.alignedHeight = Height;
    Surface->info.is16Bit       = (formatInfo->bitsPerPixel == 16);

#if gcdENABLE_3D
    /* Init superTiled info. */
    Surface->info.superTiled = gcvFALSE;
#endif

    /* User pool? */
    if (Pool == gcvPOOL_USER)
    {
        /* Init the node as the user allocated. */
        Surface->info.node.pool                    = gcvPOOL_USER;
        Surface->info.node.u.wrapped.logicalMapped = gcvFALSE;
        Surface->info.node.u.wrapped.mappingInfo   = gcvNULL;

        /* Align the dimensions by the block size. */
        Surface->info.alignedWidth  = gcmALIGN_NP2(Surface->info.alignedWidth,
                                                   formatInfo->blockWidth);
        Surface->info.alignedHeight = gcmALIGN_NP2(Surface->info.alignedHeight,
                                                   formatInfo->blockHeight);

        /* Always single layer for user surface */
        gcmASSERT(layers == 1);

        /* Compute the surface placement parameters. */
        _ComputeSurfacePlacement(Surface);

        Surface->info.layerSize = Surface->info.sliceSize * Surface->depth;

        Surface->info.size = Surface->info.layerSize * layers;

        if (Surface->info.type == gcvSURF_TEXTURE)
        {
            Surface->info.tiling = gcvTILED;
        }
    }

    /* No --> allocate video memory. */
    else
    {
#if gcdENABLE_VG
        if (currentType == gcvHARDWARE_VG)
        {
            gcmONERROR(
                gcoVGHARDWARE_AlignToTile(gcvNULL,
                                          Surface->info.type,
                                          &Surface->info.alignedWidth,
                                          &Surface->info.alignedHeight));
        }
        else
#endif
        {
            /* Align width and height to tiles. */
#if gcdENABLE_3D
            if ((Surface->info.samples.x > 1 || Surface->info.samples.y > 1)
            &&  (Surface->info.vaa == gcvFALSE)
            )
            {
                Width  = gcoMATH_DivideUInt(Width,  Surface->info.samples.x);
                Height = gcoMATH_DivideUInt(Height, Surface->info.samples.y);

                gcmONERROR(
                    gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                      Surface->info.type,
                                                      Format,
                                                      &Width,
                                                      &Height,
                                                      Depth,
                                                      &Surface->info.tiling,
                                                      &Surface->info.superTiled));

                Surface->info.alignedWidth  = Width  * Surface->info.samples.x;
                Surface->info.alignedHeight = Height * Surface->info.samples.y;
            }
            else
            {
                gcmONERROR(
                    gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                      Type,
                                                      Format,
                                                      &Surface->info.alignedWidth,
                                                      &Surface->info.alignedHeight,
                                                      Depth,
                                                      &Surface->info.tiling,
                                                      &Surface->info.superTiled));

#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
                if ((Type == gcvSURF_FLIP_BITMAP) &&
                    (Height != Surface->info.alignedHeight))
                {
                    Surface->info.flipBitmapOffset = (Surface->info.alignedHeight - Height)
                                                   *  Surface->info.alignedWidth
                                                   *  Surface->info.bitsPerPixel / 8;
                }
#   endif
            }
#else
            gcmONERROR(
                gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                  Surface->info.type,
                                                  Format,
                                                  &Surface->info.alignedWidth,
                                                  &Surface->info.alignedHeight,
                                                  Depth,
                                                  &Surface->info.tiling,
                                                  gcvNULL));
#endif /* gcdENABLE_3D */
        }

        /*
        ** We cannot use multi tiled/supertiled to create 3D surface, this surface cannot be recognized
        ** by texture unit now.
        */
        if ((Depth > 1) &&
            (Surface->info.tiling & gcvTILING_SPLIT_BUFFER))
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
        /* Determine bank offset bytes. */
#if gcdENABLE_3D
        /* If HW need to be programmed as multi pipe with 2 addresses,
         * bottom addresses need to calculated now, no matter the surface itself
         * was split or not.
         */
        if (gcoHARDWARE_IsMultiPipes(gcvNULL) == gcvTRUE)
        {
            gctUINT halfHeight = gcmALIGN(Surface->info.alignedHeight / 2,
                                          Surface->info.superTiled ? 64 : 4);

            gctUINT32 topBufferSize = ((Surface->info.alignedWidth/ formatInfo->blockWidth)
                                    *  (halfHeight/ formatInfo->blockHeight)
                                    *  blockSize) / 8;

#if gcdENABLE_BUFFER_ALIGNMENT
            if (Surface->info.tiling & gcvTILING_SPLIT_BUFFER)
            {
                gcmONERROR(
                    _GetBankOffsetBytes(Surface,
                                        Surface->info.type,
                                        topBufferSize,
                                        &bankOffsetBytes));
            }
#endif
            Surface->info.bottomBufferOffset = topBufferSize + bankOffsetBytes;
        }
        else
#endif
        {
            Surface->info.bottomBufferOffset = 0;
        }

        /* Compute the surface placement parameters. */
        _ComputeSurfacePlacement(Surface);

        /* Append bank offset bytes. */
        Surface->info.sliceSize += bankOffsetBytes;

        Surface->info.layerSize = Surface->info.sliceSize * Surface->depth;

        Surface->info.size = Surface->info.layerSize * layers;
    }

    if (!(Type & gcvSURF_NO_VIDMEM) && (Pool != gcvPOOL_USER))
    {
        gctUINT bytes = Surface->info.size;
        gctUINT alignment;
        gctUINT32 allocFlags = gcvALLOC_FLAG_NONE;

#if gcdENABLE_3D
#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
        /*
         * May need more bytes for flip bitmap.
         * Offset is '0' for non-flip bitmap.
         */
        bytes += Surface->info.flipBitmapOffset;
#endif

        if ((gcoHARDWARE_QuerySurfaceRenderable(gcvNULL, &Surface->info) == gcvSTATUS_OK) &&
            (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_TILE_FILLER))             &&
            Depth == 1 )
        {
            /* 256 tile alignment for fast clear fill feature. */
            bytes = gcmALIGN(bytes, 256 * 64);
        }

        if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_NEW_RA) ||
            gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_COLOR_COMPRESSION))
        {
            /* - Depth surfaces need 256 byte alignment.
             * - Color compression need 256 byte alignment(same in all versions?)
             *   Doing this generically.
             */
            alignment = 256;
        }
        else if (Surface->info.samples.x > 1 || Surface->info.samples.y > 1)
        {
            /* 256 byte alignment for MSAA surface */
            alignment = 256;
        }
        else
#endif
        {
            /* alignment should be 16(pixels) * byte per pixels for tiled surface*/
            alignment = (formatInfo->bitsPerPixel >= 64) ? (4*4*formatInfo->bitsPerPixel/8) : 64;
        }

#if gcdENABLE_2D
        if ((Surface->info.type == gcvSURF_BITMAP) && gcoHARDWARE_Is2DAvailable(gcvNULL))
        {
            gcoHARDWARE_Query2DSurfaceAllocationInfo(gcvNULL, &Surface->info, &bytes, &alignment);
        }
#endif

        if (Surface->info.flags & gcvSURF_FLAG_CONTENT_PROTECTED)
        {
            allocFlags |= gcvALLOC_FLAG_SECURITY;
        }

        if (Surface->info.flags & gcvSURF_FLAG_CONTIGUOUS)
        {
            allocFlags |= gcvALLOC_FLAG_CONTIGUOUS;
        }

        if (Format & gcvSURF_FORMAT_OCL)
        {
            bytes = gcmALIGN(bytes + 64,  64);
        }

        gcmONERROR(gcsSURF_NODE_Construct(
            &Surface->info.node,
            bytes,
            alignment,
            Surface->info.type,
            allocFlags,
            Pool
            ));

        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "Allocated surface 0x%x: pool=%d size=%dx%dx%d bytes=%u",
                      &Surface->info.node,
                      Surface->info.node.pool,
                      Surface->info.alignedWidth,
                      Surface->info.alignedHeight,
                      Surface->depth,
                      Surface->info.size);
    }

#if gcdENABLE_3D
    /* No Hierarchical Z buffer allocated. */
    gcmONERROR(gcoSURF_AllocateHzBuffer(Surface));

    /* Allocate tile status buffer after HZ buffer
    ** b/c we need allocat HZ tile status if HZ exist.
    */
    gcmONERROR(gcoSURF_AllocateTileStatus(Surface));

    Surface->info.hasStencilComponent = (format == gcvSURF_D24S8             ||
                                         format == gcvSURF_S8D32F            ||
                                         format == gcvSURF_S8D32F_2_A8R8G8B8 ||
                                         format == gcvSURF_S8D32F_1_G32R32F  ||
                                         format == gcvSURF_D24S8_1_A8R8G8B8  ||
                                         format == gcvSURF_S8);

    Surface->info.canDropStencilPlane = gcvTRUE;
#endif

    if (Pool != gcvPOOL_USER)
    {
        if (!(Type & gcvSURF_NO_VIDMEM))
        {
#if gcdENABLE_VG
            /* Lock the surface. */
            gcmONERROR(_Lock(Surface, currentType));
#else
            /* Lock the surface. */
            gcmONERROR(_Lock(Surface));
#endif
        }
    }

    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Free the memory allocated to the surface. */
    _FreeSurface(Surface);

    /* Return the status. */
    return status;
}

static gceSTATUS
_UnmapUserBuffer(
    IN gcoSURF Surface,
    IN gctBOOL ForceUnmap
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Surface=0x%x ForceUnmap=%d", Surface, ForceUnmap);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Cannot be negative. */
        gcmASSERT(Surface->info.node.lockCount >= 0);

        /* Nothing is mapped? */
        if (Surface->info.node.lockCount == 0)
        {
            /* Nothing to do. */
            status = gcvSTATUS_OK;
            break;
        }

        /* Make sure the reference counter is proper. */
        if (Surface->info.node.lockCount > 1)
        {
            /* Forced unmap? */
            if (ForceUnmap)
            {
                /* Invalid reference count. */
                gcmASSERT(gcvFALSE);
            }
            else
            {
                /* Decrement. */
                Surface->info.node.lockCount -= 1;

                /* Done for now. */
                status = gcvSTATUS_OK;
                break;
            }
        }

        /* Unmap the logical memory. */
        if (Surface->info.node.u.wrapped.logicalMapped)
        {
            gcmERR_BREAK(gcoHAL_UnmapMemory(
                gcvNULL,
                gcmINT2PTR(Surface->info.node.physical),
                Surface->info.size,
                Surface->info.node.logical
                ));

            Surface->info.node.physical = ~0U;
            Surface->info.node.u.wrapped.logicalMapped = gcvFALSE;
        }

        /* Unmap the physical memory. */
        if (Surface->info.node.u.wrapped.mappingInfo != gcvNULL)
        {
            gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;

            /* Save the current hardware type */
            gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL, &currentType));

            if (Surface->info.node.u.wrapped.mappingHardwareType != currentType)
            {
                /* Change to the mapping hardware type */
                gcmVERIFY_OK(gcoHAL_SetHardwareType(gcvNULL,
                                                    Surface->info.node.u.wrapped.mappingHardwareType));
            }

            gcmERR_BREAK(gcoHAL_ScheduleUnmapUserMemory(
                gcvNULL,
                Surface->info.node.u.wrapped.mappingInfo,
                Surface->info.size,
                Surface->info.node.physical,
                Surface->info.node.logical
                ));

            Surface->info.node.logical = gcvNULL;
            Surface->info.node.u.wrapped.mappingInfo = gcvNULL;

            if (Surface->info.node.u.wrapped.mappingHardwareType != currentType)
            {
                /* Restore the current hardware type */
                gcmVERIFY_OK(gcoHAL_SetHardwareType(gcvNULL, currentType));
            }
        }

#if gcdSECURE_USER
        gcmERR_BREAK(gcoHAL_ScheduleUnmapUserMemory(
            gcvNULL,
            gcvNULL,
            Surface->info.size,
            0,
            Surface->info.node.logical));
#endif

        /* Reset the surface. */
        Surface->info.node.lockCount = 0;
        Surface->info.node.valid = gcvFALSE;
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/******************************************************************************\
******************************** gcoSURF API Code *******************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoSURF_Construct
**
**  Create a new gcoSURF object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gctUINT Width
**          Width of surface to create in pixels.
**
**      gctUINT Height
**          Height of surface to create in lines.
**
**      gctUINT Depth
**          Depth of surface to create in planes.
**
**      gceSURF_TYPE Type
**          Type of surface to create.
**
**      gceSURF_FORMAT Format
**          Format of surface to create.
**
**      gcePOOL Pool
**          Pool to allocate surface from.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to the variable that will hold the gcoSURF object pointer.
*/
gceSTATUS
gcoSURF_Construct(
    IN gcoHAL Hal,
    IN gctUINT Width,
    IN gctUINT Height,
    IN gctUINT Depth,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN gcePOOL Pool,
    OUT gcoSURF * Surface
    )
{
    gcoSURF surface = gcvNULL;
    gceSTATUS status;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER_ARG("Width=%u Height=%u Depth=%u Type=%d Format=%d Pool=%d",
                  Width, Height, Depth, Type, Format, Pool);

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    /* Allocate the gcoSURF object. */
    gcmONERROR(
        gcoOS_Allocate(gcvNULL,
                       gcmSIZEOF(struct _gcoSURF),
                       &pointer));

    gcoOS_ZeroMemory(pointer, gcmSIZEOF(struct _gcoSURF));

    surface = pointer;

    /* Initialize the gcoSURF object.*/
    surface->object.type    = gcvOBJ_SURF;
    surface->info.offset    = 0;

    surface->info.dither2D      = gcvFALSE;
    surface->info.deferDither3D = gcvFALSE;
    surface->info.paddingFormat = (Format == gcvSURF_R8_1_X8R8G8B8 || Format == gcvSURF_G8R8_1_X8R8G8B8)
                                ? gcvTRUE : gcvFALSE;
    surface->info.garbagePadded = gcvTRUE;

#if gcdENABLE_3D
    /* 1 sample per pixel, no VAA. */

#if defined(ANDROID)
    surface->info.flags = 0;
#endif
    surface->info.samples.x = 1;
    surface->info.samples.y = 1;
    surface->info.vaa       = gcvFALSE;

    surface->info.colorType = gcvSURF_COLOR_UNKNOWN;

    surface->info.flags = gcvSURF_FLAG_NONE;
    if (Type & gcvSURF_PROTECTED_CONTENT)
    {
        surface->info.flags |= gcvSURF_FLAG_CONTENT_PROTECTED;
    }

    if (Type & gcvSURF_CONTIGUOUS)
    {
        surface->info.flags |= gcvSURF_FLAG_CONTIGUOUS;
        Type &= ~gcvSURF_CONTIGUOUS;
    }

    /* set color space */
    if ((Format == gcvSURF_A8_SBGR8) ||
        (Format == gcvSURF_SBGR8) ||
        (Format == gcvSURF_X8_SBGR8))
    {
        surface->info.colorSpace = gcvSURF_COLOR_SPACE_NONLINEAR;
    }
    else
    {
        surface->info.colorSpace = gcvSURF_COLOR_SPACE_LINEAR;
    }

    surface->info.hzNode.pool = gcvPOOL_UNKNOWN;
    surface->info.tileStatusNode.pool = gcvPOOL_UNKNOWN;
    surface->info.hzTileStatusNode.pool = gcvPOOL_UNKNOWN;
#endif /* gcdENABLE_3D */

    if (Type & gcvSURF_CACHEABLE)
    {
        gcmASSERT(Pool != gcvPOOL_USER);
        surface->info.node.u.normal.cacheable = gcvTRUE;
        Type &= ~gcvSURF_CACHEABLE;
    }
    else if (Pool != gcvPOOL_USER)
    {
        surface->info.node.u.normal.cacheable = gcvFALSE;
    }

#if gcdENABLE_3D
    if (Type & gcvSURF_TILE_STATUS_DIRTY)
    {
        surface->info.TSDirty = gcvTRUE;
        Type &= ~gcvSURF_TILE_STATUS_DIRTY;
    }
#endif

    if (Depth < 1)
    {
        /* One plane. */
        Depth = 1;
    }

    /* Allocate surface. */
    gcmONERROR(
        _AllocateSurface(surface,
                         Width, Height, Depth,
                         Type,
                         Format,
                         Pool));

    surface->referenceCount = 1;

#if gcdSYNC
    surface->info.fenceStatus = gcvFENCE_DISABLE;
    surface->info.fenceCtx    = gcvNULL;
    surface->info.sharedLock  = gcvNULL;
#endif

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                  "Created gcoSURF 0x%x",
                  surface);

    /* Return pointer to the gcoSURF object. */
    *Surface = surface;

    /* Success. */
    gcmFOOTER_ARG("*Surface=0x%x", *Surface);
    return gcvSTATUS_OK;

OnError:
    /* Free the allocated memory. */
    if (surface != gcvNULL)
    {
        gcmOS_SAFE_FREE(gcvNULL, surface);
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Destroy
**
**  Destroy an gcoSURF object.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object to be destroyed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Destroy(
    IN gcoSURF Surface
    )
{
#if gcdENABLE_3D
    gcsTLS_PTR tls;
#endif

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Decrement surface reference count. */
    if (--Surface->referenceCount != 0)
    {
        /* Still references to this surface. */
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

#if gcdENABLE_3D

    if (gcmIS_ERROR(gcoOS_GetTLS(&tls)))
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }

#if gcdENABLE_VG
    /* Unset VG target. */
    if (tls->engineVG != gcvNULL)
    {
#if GC355_PROFILER
        gcmVERIFY_OK(
            gcoVG_UnsetTarget(tls->engineVG,
            gcvNULL,0,0,0,
            Surface));
#else
        gcmVERIFY_OK(
            gcoVG_UnsetTarget(tls->engineVG,
            Surface));
#endif
    }
#endif

    /* Special case for 3D surfaces. */
    if (tls->engine3D != gcvNULL)
    {
        /* If this is a render target, unset it. */
        if ((Surface->info.type == gcvSURF_RENDER_TARGET)
        ||  (Surface->info.type == gcvSURF_TEXTURE)
        )
        {
            gctUINT32 rtIdx;

            for (rtIdx = 0; rtIdx < 4; ++rtIdx)
            {
                gcmVERIFY_OK(gco3D_UnsetTargetEx(tls->engine3D, rtIdx, Surface));
            }
        }

        /* If this is a depth buffer, unset it. */
        else if (Surface->info.type == gcvSURF_DEPTH)
        {
            gcmVERIFY_OK(
                gco3D_UnsetDepth(tls->engine3D, Surface));
        }
    }
#endif

    if (gcPLS.hal->dump != gcvNULL)
    {
        /* Dump the deletion. */
        gcmVERIFY_OK(
            gcoDUMP_Delete(gcPLS.hal->dump, Surface->info.node.physical));
    }

    /* User-allocated surface? */
    if (Surface->info.node.pool == gcvPOOL_USER)
    {
        gcmVERIFY_OK(
            _UnmapUserBuffer(Surface, gcvTRUE));
    }

#if gcdGC355_MEM_PRINT
#ifdef LINUX
    gcoOS_AddRecordAllocation(-(gctINT32)Surface->info.node.size);
#endif
#endif

    /* Free the video memory. */
    gcmVERIFY_OK(_FreeSurface(Surface));

    /* Mark gcoSURF object as unknown. */
    Surface->object.type = gcvOBJ_UNKNOWN;

    /* Free the gcoSURF object. */
    gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, Surface));

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                  "Destroyed gcoSURF 0x%x",
                  Surface);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_QueryVidMemNode
**
**  Query the video memory node attributes of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      gctUINT32 * Node
**          Pointer to a variable receiving the video memory node.
**
**      gcePOOL * Pool
**          Pointer to a variable receiving the pool the video memory node originated from.
**
**      gctUINT_PTR Bytes
**          Pointer to a variable receiving the video memory node size in bytes.
**
*/
gceSTATUS
gcoSURF_QueryVidMemNode(
    IN gcoSURF Surface,
    OUT gctUINT32 * Node,
    OUT gcePOOL * Pool,
    OUT gctSIZE_T_PTR Bytes
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(Node != gcvNULL);
    gcmVERIFY_ARGUMENT(Pool != gcvNULL);
    gcmVERIFY_ARGUMENT(Bytes != gcvNULL);

    /* Return the video memory attributes. */
    *Node = Surface->info.node.u.normal.node;
    *Pool = Surface->info.node.pool;
    *Bytes = Surface->info.node.size;

    /* Success. */
    gcmFOOTER_ARG("*Node=0x%x *Pool=%d *Bytes=%d", *Node, *Pool, *Bytes);
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gcoSURF_WrapSurface
**
**  Wrap gcoSURF_Object with known logica address (CPU) and physical address(GPU)
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object to be destroyed.
**
**      gctUINT Alignment
**          Alignment of each pixel row in bytes.
**
**      gctPOINTER Logical
**          Logical pointer to the user allocated surface or gcvNULL if no
**          logical pointer has been provided.
**
**      gctUINT32 Physical
**          Physical pointer(GPU address) to the user allocated surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_WrapSurface(
    IN gcoSURF Surface,
    IN gctUINT Alignment,
    IN gctPOINTER Logical,
    IN gctUINT32 Physical
    )
{
    gceSTATUS status = gcvSTATUS_OK;

#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x Alignment=%u Logical=0x%x Physical=%08x",
              Surface, Alignment, Logical, Physical);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Has to be user-allocated surface. */
        if (Surface->info.node.pool != gcvPOOL_USER)
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* Already mapped? */
        if (Surface->info.node.lockCount > 0)
        {
            if ((Logical != gcvNULL) &&
                (Logical != Surface->info.node.logical))
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            if ((Physical != gcvINVALID_ADDRESS) &&
                (Physical != Surface->info.node.physical))
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            /* Success. */
            break;
        }
#if gcdENABLE_VG
        gcmGETCURRENTHARDWARE(currentType);
#endif
        /* Set new alignment. */
        if (Alignment != 0)
        {
            /* Compute the unaligned stride. */
            gctUINT32 stride;
            gctUINT32 bytesPerPixel;
            gcsSURF_FORMAT_INFO_PTR formatInfo = &Surface->info.formatInfo;

            switch (Surface->info.format)
            {
            case gcvSURF_YV12:
            case gcvSURF_I420:
            case gcvSURF_NV12:
            case gcvSURF_NV21:
            case gcvSURF_NV16:
            case gcvSURF_NV61:
                bytesPerPixel = 1;
                stride = Surface->info.alignedWidth;
                break;

            default:
                bytesPerPixel = Surface->info.bitsPerPixel / 8;
                stride = Surface->info.alignedWidth * Surface->info.bitsPerPixel / 8;
                break;
            }

            /* Align the stride (Alignment can be not a power of number). */
            if (gcoMATH_ModuloUInt(stride, Alignment) != 0)
            {
                stride = gcoMATH_DivideUInt(stride, Alignment)  * Alignment
                       + Alignment;

                Surface->info.alignedWidth = stride / bytesPerPixel;
            }

            /* Compute the new surface placement parameters. */
            _ComputeSurfacePlacement(Surface);

            if (stride != Surface->info.stride)
            {
                /*
                 * Still not equal, which means user stride is not pixel aligned, ie,
                 * stride != alignedWidth(user) * bytesPerPixel
                 */
                Surface->info.stride = stride;

                /* Re-calculate slice size. */
                Surface->info.sliceSize = stride * (Surface->info.alignedHeight / formatInfo->blockHeight);
            }

            Surface->info.layerSize = Surface->info.sliceSize * Surface->depth;

            /* We won't map multi-layer surface. */
            gcmASSERT(formatInfo->layers == 1);

            /* Compute the new size. */
            Surface->info.size = Surface->info.layerSize * formatInfo->layers;
        }

        /* Validate the surface. */
        Surface->info.node.valid = gcvTRUE;

        /* Set the lock count. */
        Surface->info.node.lockCount++;

        /* Set the node parameters. */
        Surface->info.node.u.wrapped.logicalMapped = gcvFALSE;
        Surface->info.node.u.wrapped.mappingInfo   = gcvNULL;

        Surface->info.node.logical                 = Logical;
        Surface->info.node.physical                = Physical;
        Surface->info.node.count                   = 1;
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoSURF_MapUserSurface
**
**  Store the logical and physical pointers to the user-allocated surface in
**  the gcoSURF object and map the pointers as necessary.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object to be destroyed.
**
**      gctUINT Alignment
**          Alignment of each pixel row in bytes.
**
**      gctPOINTER Logical
**          Logical pointer to the user allocated surface or gcvNULL if no
**          logical pointer has been provided.
**
**      gctUINT32 Physical
**          Physical pointer to the user allocated surface or gcvINVALID_ADDRESS if no
**          physical pointer has been provided.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_MapUserSurface(
    IN gcoSURF Surface,
    IN gctUINT Alignment,
    IN gctPOINTER Logical,
    IN gctUINT32 Physical
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gctBOOL logicalMapped = gcvFALSE;
    gctPOINTER mappingInfo = gcvNULL;

    gctPOINTER logical = gcvNULL;
    gctUINT32 physical = 0;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x Alignment=%u Logical=0x%x Physical=%08x",
              Surface, Alignment, Logical, Physical);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Has to be user-allocated surface. */
        if (Surface->info.node.pool != gcvPOOL_USER)
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* Already mapped? */
        if (Surface->info.node.lockCount > 0)
        {
            if ((Logical != gcvNULL) &&
                (Logical != Surface->info.node.logical))
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            if ((Physical != gcvINVALID_ADDRESS) &&
                (Physical != Surface->info.node.physical))
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            /* Success. */
            break;
        }
#if gcdENABLE_VG
        gcmGETCURRENTHARDWARE(currentType);
#endif
        /* Set new alignment. */
        if (Alignment != 0)
        {
            /* Compute the unaligned stride. */
            gctUINT32 stride;
            gctUINT32 bytesPerPixel;
            gcsSURF_FORMAT_INFO_PTR formatInfo = &Surface->info.formatInfo;

            switch (Surface->info.format)
            {
            case gcvSURF_YV12:
            case gcvSURF_I420:
            case gcvSURF_NV12:
            case gcvSURF_NV21:
            case gcvSURF_NV16:
            case gcvSURF_NV61:
                bytesPerPixel = 1;
                stride = Surface->info.alignedWidth;
                break;

            default:
                bytesPerPixel = Surface->info.bitsPerPixel / 8;
                stride = Surface->info.alignedWidth * Surface->info.bitsPerPixel / 8;
                break;
            }

            /* Align the stride (Alignment can be not a power of number). */
            if (gcoMATH_ModuloUInt(stride, Alignment) != 0)
            {
                stride = gcoMATH_DivideUInt(stride, Alignment)  * Alignment
                       + Alignment;

                Surface->info.alignedWidth = stride / bytesPerPixel;
            }

            /* Compute the new surface placement parameters. */
            _ComputeSurfacePlacement(Surface);

            if (stride != Surface->info.stride)
            {
                /*
                 * Still not equal, which means user stride is not pixel aligned, ie,
                 * stride != alignedWidth(user) * bytesPerPixel
                 */
                Surface->info.stride = stride;

                /* Re-calculate slice size. */
                Surface->info.sliceSize = stride * (Surface->info.alignedHeight / formatInfo->blockHeight);
            }

            Surface->info.layerSize = Surface->info.sliceSize * Surface->depth;

            /* We won't map multi-layer surface. */
            gcmASSERT(formatInfo->layers == 1);

            /* Compute the new size. */
            Surface->info.size = Surface->info.layerSize * formatInfo->layers;
        }

        /* Map logical pointer if not specified. */
        if (Logical == gcvNULL)
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }
        else
        {
            /* Set the logical pointer. */
            logical = Logical;
        }

        /* Map physical pointer to GPU address. */
        gcmERR_BREAK(gcoHAL_MapUserMemory(
                Logical,
                Physical,
                Surface->info.size,
                &mappingInfo,
                &physical
                ));

        /* Validate the surface. */
        Surface->info.node.valid = gcvTRUE;

        /* Set the lock count. */
        Surface->info.node.lockCount++;

        /* Set the node parameters. */
        Surface->info.node.u.wrapped.logicalMapped = logicalMapped;
        Surface->info.node.u.wrapped.mappingInfo   = mappingInfo;
        gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL,
                                            &Surface->info.node.u.wrapped.mappingHardwareType));
        Surface->info.node.logical                 = logical;
        Surface->info.node.physical                = physical;
        Surface->info.node.count                   = 1;
    }
    while (gcvFALSE);

    /* Roll back. */
    if (gcmIS_ERROR(status))
    {
        if (logicalMapped)
        {
            gcmVERIFY_OK(gcoHAL_UnmapMemory(
                gcvNULL,
                gcmINT2PTR(physical),
                Surface->info.size,
                logical
                ));
        }

        if (mappingInfo != gcvNULL)
        {
            gcmVERIFY_OK(gcoOS_UnmapUserMemory(
                gcvNULL,
                logical,
                Surface->info.size,
                mappingInfo,
                physical
                ));
        }
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_IsValid
**
**  Verify whether the surface is a valid gcoSURF object.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to a gcoSURF object.
**
**  RETURNS:
**
**      The return value of the function is set to gcvSTATUS_TRUE if the
**      surface is valid.
*/
gceSTATUS
gcoSURF_IsValid(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Set return value. */
    status = ((Surface != gcvNULL)
        && (Surface->object.type != gcvOBJ_SURF))
        ? gcvSTATUS_FALSE
        : gcvSTATUS_TRUE;

    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoSURF_IsTileStatusSupported
**
**  Verify whether the tile status is supported by the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to a gcoSURF object.
**
**  RETURNS:
**
**      The return value of the function is set to gcvSTATUS_TRUE if the
**      tile status is supported.
*/
gceSTATUS
gcoSURF_IsTileStatusSupported(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Set return value. */
    status = (Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN)
        ? gcvSTATUS_NOT_SUPPORTED
        : gcvSTATUS_TRUE;

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_IsTileStatusEnabled
**
**  Verify whether the tile status is enabled on this surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to a gcoSURF object.
**
**  RETURNS:
**
**      The return value of the function is set to gcvSTATUS_TRUE if the
**      tile status is enabled.
*/
gceSTATUS
gcoSURF_IsTileStatusEnabled(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Check whether the surface has enabled tile status. */
    if ((Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN) &&
        (Surface->info.tileStatusDisabled == gcvFALSE))
    {
        status = gcvSTATUS_TRUE;
    }
    else
    {
        status = gcvSTATUS_FALSE;
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_IsCompressed
**
**  Verify whether the surface is compressed.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to a gcoSURF object.
**
**  RETURNS:
**
**      The return value of the function is set to gcvSTATUS_TRUE if the
**      tile status is supported.
*/
gceSTATUS
gcoSURF_IsCompressed(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Check whether the surface is compressed. */
    if ((Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN) &&
        (Surface->info.tileStatusDisabled == gcvFALSE) &&
        Surface->info.compressed)
    {
        status = gcvSTATUS_TRUE;
    }
    else
    {
        status = gcvSTATUS_FALSE;
    }

    /* Return status. */
    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_EnableTileStatusEx
**
**  Enable tile status for the specified surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gctUINT RtIndex
**          Which RT slot will be bound for this surface
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_EnableTileStatusEx(
    IN gcoSURF Surface,
    IN gctUINT RtIndex
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x RtIndex=%d", Surface, RtIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Enable tile status. */
        gcmERR_BREAK(
            gcoHARDWARE_EnableTileStatus(gcvNULL,
                                         &Surface->info,
                                         (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN) ?
                                         Surface->info.tileStatusNode.physical : 0,
                                         &Surface->info.hzTileStatusNode,
                                         RtIndex));

        /* Success. */
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_EnableTileStatus
**
**  Enable tile status for the specified surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_EnableTileStatus(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    status = gcoSURF_EnableTileStatusEx(Surface, 0);

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoSURF_DisableTileStatus
**
**  Disable tile status for the specified surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gctBOOL Decompress
**          Set if the render target needs to decompressed by issuing a resolve
**          onto itself.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_DisableTileStatus(
    IN gcoSURF Surface,
    IN gctBOOL Decompress
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
        {
            /* Disable tile status. */
            gcmERR_BREAK(
                gcoHARDWARE_DisableTileStatus(gcvNULL, &Surface->info,
                                              Decompress));
        }

        /* Success. */
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }
    while (gcvFALSE);

    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoSURF_FlushTileStatus
**
**  Flush tile status for the specified surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gctBOOL Decompress
**          Set if the render target needs to decompressed by issuing a resolve
**          onto itself.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_FlushTileStatus(
    IN gcoSURF Surface,
    IN gctBOOL Decompress
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        /* Disable tile status. */
        gcmONERROR(
            gcoHARDWARE_FlushTileStatus(gcvNULL, &Surface->info,
                                        Decompress));
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#endif /* gcdENABLE_3D */

/*******************************************************************************
**
**  gcoSURF_GetSize
**
**  Get the size of an gcoSURF object.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT * Width
**          Pointer to variable that will receive the width of the gcoSURF
**          object.  If 'Width' is gcvNULL, no width information shall be returned.
**
**      gctUINT * Height
**          Pointer to variable that will receive the height of the gcoSURF
**          object.  If 'Height' is gcvNULL, no height information shall be returned.
**
**      gctUINT * Depth
**          Pointer to variable that will receive the depth of the gcoSURF
**          object.  If 'Depth' is gcvNULL, no depth information shall be returned.
*/
gceSTATUS
gcoSURF_GetSize(
    IN gcoSURF Surface,
    OUT gctUINT * Width,
    OUT gctUINT * Height,
    OUT gctUINT * Depth
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Width != gcvNULL)
    {
        /* Return the width. */
        *Width =
#if gcdENABLE_3D
            gcoMATH_DivideUInt(Surface->info.rect.right,
                                    Surface->info.samples.x);
#else
            Surface->info.rect.right;
#endif
    }

    if (Height != gcvNULL)
    {
        /* Return the height. */
        *Height =
#if gcdENABLE_3D
            gcoMATH_DivideUInt(Surface->info.rect.bottom,
                                     Surface->info.samples.y);
#else
            Surface->info.rect.bottom;
#endif
    }

    if (Depth != gcvNULL)
    {
        /* Return the depth. */
        *Depth = Surface->depth;
    }

    /* Success. */
    gcmFOOTER_ARG("*Width=%u *Height=%u *Depth=%u",
                  (Width  == gcvNULL) ? 0 : *Width,
                  (Height == gcvNULL) ? 0 : *Height,
                  (Depth  == gcvNULL) ? 0 : *Depth);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_GetAlignedSize
**
**  Get the aligned size of an gcoSURF object.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT * Width
**          Pointer to variable that receives the aligned width of the gcoSURF
**          object.  If 'Width' is gcvNULL, no width information shall be returned.
**
**      gctUINT * Height
**          Pointer to variable that receives the aligned height of the gcoSURF
**          object.  If 'Height' is gcvNULL, no height information shall be
**          returned.
**
**      gctINT * Stride
**          Pointer to variable that receives the stride of the gcoSURF object.
**          If 'Stride' is gcvNULL, no stride information shall be returned.
*/
gceSTATUS
gcoSURF_GetAlignedSize(
    IN gcoSURF Surface,
    OUT gctUINT * Width,
    OUT gctUINT * Height,
    OUT gctINT * Stride
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Width != gcvNULL)
    {
        /* Return the aligned width. */
        *Width = Surface->info.alignedWidth;
    }

    if (Height != gcvNULL)
    {
        /* Return the aligned height. */
        *Height = Surface->info.alignedHeight;
    }

    if (Stride != gcvNULL)
    {
        /* Return the stride. */
        *Stride = Surface->info.stride;
    }

    /* Success. */
    gcmFOOTER_ARG("*Width=%u *Height=%u *Stride=%d",
                  (Width  == gcvNULL) ? 0 : *Width,
                  (Height == gcvNULL) ? 0 : *Height,
                  (Stride == gcvNULL) ? 0 : *Stride);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_GetAlignment
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gceSURF_TYPE Type
**          Type of surface.
**
**      gceSURF_FORMAT Format
**          Format of surface.
**
**  OUTPUT:
**
**      gctUINT * addressAlignment
**          Pointer to the variable of address alignment.
**      gctUINT * xAlignmenet
**          Pointer to the variable of x Alignment.
**      gctUINT * yAlignment
**          Pointer to the variable of y Alignment.
*/
gceSTATUS
gcoSURF_GetAlignment(
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    OUT gctUINT * AddressAlignment,
    OUT gctUINT * XAlignment,
    OUT gctUINT * YAlignment
    )
{
    gceSTATUS status;
    gctUINT32 bitsPerPixel;
    gctUINT xAlign = (gcvSURF_TEXTURE == Type) ? 4 : 16;
    gctUINT yAlign = 4;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Type=%d Format=%d", Type, Format);

    /* Compute alignment factors. */
    if (XAlignment != gcvNULL)
    {
        *XAlignment = xAlign;
    }

    if (YAlignment != gcvNULL)
    {
        *YAlignment = yAlign;
    }
#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
    if (currentType == gcvHARDWARE_VG)
    {
        /* Compute bits per pixel. */
        gcmONERROR(gcoVGHARDWARE_ConvertFormat(gcvNULL,
                                             Format,
                                             &bitsPerPixel,
                                             gcvNULL));
    }
    else
#endif
    {
        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(Format,
                                             &bitsPerPixel,
                                             gcvNULL));
    }

    if (AddressAlignment != gcvNULL)
    {
        *AddressAlignment = xAlign * yAlign * bitsPerPixel / 8;
    }

    gcmFOOTER_ARG("*XAlignment=0x%x  *YAlignment=0x%x *AddressAlignment=0x%x",
        gcmOPT_VALUE(XAlignment), gcmOPT_VALUE(YAlignment), gcmOPT_VALUE(AddressAlignment));
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoSURF_AlignResolveRect (need to modify)
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gceSURF_TYPE Type
**          Type of surface.
**
**      gceSURF_FORMAT Format
**          Format of surface.
**
**  OUTPUT:
**
**      gctUINT * addressAlignment
**          Pointer to the variable of address alignment.
**      gctUINT * xAlignmenet
**          Pointer to the variable of x Alignment.
**      gctUINT * yAlignment
**          Pointer to the variable of y Alignment.
*/
gceSTATUS
gcoSURF_AlignResolveRect(
    IN gcoSURF Surf,
    IN gcsPOINT_PTR RectOrigin,
    IN gcsPOINT_PTR RectSize,
    OUT gcsPOINT_PTR AlignedOrigin,
    OUT gcsPOINT_PTR AlignedSize
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surf=0x%x RectOrigin=0x%x RectSize=0x%x",
        Surf, RectOrigin, RectSize);

    status = gcoHARDWARE_AlignResolveRect(&Surf->info, RectOrigin, RectSize,
                                         AlignedOrigin, AlignedSize);

    /* Success. */
    gcmFOOTER();

    return status;
}
#endif

/*******************************************************************************
**
**  gcoSURF_GetFormat
**
**  Get surface type and format.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gceSURF_TYPE * Type
**          Pointer to variable that receives the type of the gcoSURF object.
**          If 'Type' is gcvNULL, no type information shall be returned.
**
**      gceSURF_FORMAT * Format
**          Pointer to variable that receives the format of the gcoSURF object.
**          If 'Format' is gcvNULL, no format information shall be returned.
*/
gceSTATUS
gcoSURF_GetFormat(
    IN gcoSURF Surface,
    OUT gceSURF_TYPE * Type,
    OUT gceSURF_FORMAT * Format
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Type != gcvNULL)
    {
        /* Return the surface type. */
        *Type = Surface->info.type;
    }

    if (Format != gcvNULL)
    {
        /* Return the surface format. */
        *Format = Surface->info.format;
    }

    /* Success. */
    gcmFOOTER_ARG("*Type=%d *Format=%d",
                  (Type   == gcvNULL) ? 0 : *Type,
                  (Format == gcvNULL) ? 0 : *Format);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_GetFormatInfo
**
**  Get surface format information.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gcsSURF_FORMAT_INFO_PTR * formatInfo
**          Pointer to variable that receives the format informationof the gcoSURF object.
**          If 'formatInfo' is gcvNULL, no format information shall be returned.
*/
gceSTATUS
gcoSURF_GetFormatInfo(
    IN gcoSURF Surface,
    OUT gcsSURF_FORMAT_INFO_PTR * formatInfo
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (formatInfo != gcvNULL)
    {
        /* Return the surface format. */
        *formatInfo = &Surface->info.formatInfo;
    }

    /* Success. */
    gcmFOOTER_ARG("*Format=0x%x",
                  (formatInfo == gcvNULL) ? 0 : *formatInfo);
    return gcvSTATUS_OK;
}



/*******************************************************************************
**
**  gcoSURF_GetPackedFormat
**
**  Get surface packed format for multiple-layer surface
**  gcvSURF_A32B32G32R32UI_2 ->gcvSURF_A32B32G32R32UI
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**
**      gceSURF_FORMAT * Format
**          Pointer to variable that receives the format of the gcoSURF object.
**          If 'Format' is gcvNULL, no format information shall be returned.
*/
gceSTATUS
gcoSURF_GetPackedFormat(
    IN gcoSURF Surface,
    OUT gceSURF_FORMAT * Format
    )
{
    gceSURF_FORMAT format;
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    switch (Surface->info.format)
    {
    case gcvSURF_X16R16G16B16_2_A8R8G8B8:
        format = gcvSURF_X16R16G16B16;
        break;

    case gcvSURF_A16R16G16B16_2_A8R8G8B8:
        format = gcvSURF_A16R16G16B16;
        break;

    case gcvSURF_A32R32G32B32_2_G32R32F:
    case gcvSURF_A32R32G32B32_4_A8R8G8B8:
        format = gcvSURF_A32R32G32B32;
        break;

    case gcvSURF_S8D32F_1_G32R32F:
    case gcvSURF_S8D32F_2_A8R8G8B8:
        format = gcvSURF_S8D32F;
        break;

    case gcvSURF_X16B16G16R16F_2_A8R8G8B8:
        format = gcvSURF_X16B16G16R16F;
        break;

    case gcvSURF_A16B16G16R16F_2_A8R8G8B8:
        format = gcvSURF_A16B16G16R16F;
        break;

    case gcvSURF_G32R32F_2_A8R8G8B8:
        format = gcvSURF_G32R32F;
        break;

    case gcvSURF_X32B32G32R32F_2_G32R32F:
    case gcvSURF_X32B32G32R32F_4_A8R8G8B8:
        format = gcvSURF_X32B32G32R32F;
        break;

    case gcvSURF_A32B32G32R32F_2_G32R32F:
    case gcvSURF_A32B32G32R32F_4_A8R8G8B8:
        format = gcvSURF_A32B32G32R32F;
        break;

    case gcvSURF_R16F_1_A4R4G4B4:
        format = gcvSURF_R16F;
        break;

    case gcvSURF_G16R16F_1_A8R8G8B8:
        format = gcvSURF_G16R16F;
        break;

    case gcvSURF_B16G16R16F_2_A8R8G8B8:
        format = gcvSURF_B16G16R16F;
        break;

    case gcvSURF_R32F_1_A8R8G8B8:
        format = gcvSURF_R32F;
        break;

    case gcvSURF_B32G32R32F_3_A8R8G8B8:
        format = gcvSURF_B32G32R32F;
        break;

    case gcvSURF_B10G11R11F_1_A8R8G8B8:
        format = gcvSURF_B10G11R11F;
        break;

    case gcvSURF_G32R32I_2_A8R8G8B8:
        format = gcvSURF_G32R32I;
        break;

    case gcvSURF_G32R32UI_2_A8R8G8B8:
        format = gcvSURF_G32R32UI;
        break;

    case gcvSURF_X16B16G16R16I_2_A8R8G8B8:
        format = gcvSURF_X16B16G16R16I;
        break;

    case gcvSURF_A16B16G16R16I_2_A8R8G8B8:
        format = gcvSURF_A16B16G16R16I;
        break;

    case gcvSURF_X16B16G16R16UI_2_A8R8G8B8:
        format = gcvSURF_X16B16G16R16UI;
        break;

    case gcvSURF_A16B16G16R16UI_2_A8R8G8B8:
        format = gcvSURF_A16B16G16R16UI;
        break;

    case gcvSURF_X32B32G32R32I_2_G32R32I:
    case gcvSURF_X32B32G32R32I_3_A8R8G8B8:
        format = gcvSURF_X32B32G32R32I;
        break;

    case gcvSURF_A32B32G32R32I_2_G32R32I:
    case gcvSURF_A32B32G32R32I_4_A8R8G8B8:
        format = gcvSURF_A32B32G32R32I;
        break;

    case gcvSURF_X32B32G32R32UI_2_G32R32UI:
    case gcvSURF_X32B32G32R32UI_3_A8R8G8B8:
        format = gcvSURF_X32B32G32R32UI;
        break;

    case gcvSURF_A32B32G32R32UI_2_G32R32UI:
    case gcvSURF_A32B32G32R32UI_4_A8R8G8B8:
        format = gcvSURF_A32B32G32R32UI;
        break;

    case gcvSURF_A2B10G10R10UI_1_A8R8G8B8:
        format = gcvSURF_A2B10G10R10UI;
        break;

    case gcvSURF_A8B8G8R8I_1_A8R8G8B8:
        format = gcvSURF_A8B8G8R8I;
        break;

    case gcvSURF_A8B8G8R8UI_1_A8R8G8B8:
        format = gcvSURF_A8B8G8R8UI;
        break;

    case gcvSURF_R8I_1_A4R4G4B4:
        format = gcvSURF_R8I;
        break;

    case gcvSURF_R8UI_1_A4R4G4B4:
        format = gcvSURF_R8UI;
        break;

    case gcvSURF_R16I_1_A4R4G4B4:
        format = gcvSURF_R16I;
        break;

    case gcvSURF_R16UI_1_A4R4G4B4:
        format = gcvSURF_R16UI;
        break;

    case gcvSURF_R32I_1_A8R8G8B8:
        format = gcvSURF_R32I;
        break;

    case gcvSURF_R32UI_1_A8R8G8B8:
        format = gcvSURF_R32UI;
        break;

    case gcvSURF_X8R8I_1_A4R4G4B4:
        format = gcvSURF_X8R8I;
        break;

    case gcvSURF_X8R8UI_1_A4R4G4B4:
        format = gcvSURF_X8R8UI;
        break;

    case gcvSURF_G8R8I_1_A4R4G4B4:
        format = gcvSURF_G8R8I;
        break;

    case gcvSURF_G8R8UI_1_A4R4G4B4:
        format = gcvSURF_G8R8UI;
        break;

    case gcvSURF_X16R16I_1_A4R4G4B4:
        format = gcvSURF_X16R16I;
        break;

    case gcvSURF_X16R16UI_1_A4R4G4B4:
        format = gcvSURF_X16R16UI;
        break;

    case gcvSURF_G16R16I_1_A8R8G8B8:
        format = gcvSURF_G16R16I;
        break;

    case gcvSURF_G16R16UI_1_A8R8G8B8:
        format = gcvSURF_G16R16UI;
        break;
    case gcvSURF_X32R32I_1_A8R8G8B8:
        format = gcvSURF_X32R32I;
        break;

    case gcvSURF_X32R32UI_1_A8R8G8B8:
        format = gcvSURF_X32R32UI;
        break;

    case gcvSURF_X8G8R8I_1_A4R4G4B4:
        format = gcvSURF_X8G8R8I;
        break;

    case gcvSURF_X8G8R8UI_1_A4R4G4B4:
        format = gcvSURF_X8G8R8UI;
        break;

    case gcvSURF_B8G8R8I_1_A8R8G8B8:
        format = gcvSURF_B8G8R8I;
        break;

    case gcvSURF_B8G8R8UI_1_A8R8G8B8:
        format = gcvSURF_B8G8R8UI;
        break;

    case gcvSURF_B16G16R16I_2_A8R8G8B8:
        format = gcvSURF_B16G16R16I;
        break;

    case gcvSURF_B16G16R16UI_2_A8R8G8B8:
        format = gcvSURF_B16G16R16UI;
        break;

    case gcvSURF_B32G32R32I_3_A8R8G8B8:
        format = gcvSURF_B32G32R32I;
        break;

    case gcvSURF_B32G32R32UI_3_A8R8G8B8:
        format = gcvSURF_B32G32R32UI;
        break;

    default:
        format = Surface->info.format;
        break;
    }

    if (Format != gcvNULL)
    {
        /* Return the surface format. */
        *Format = format;
    }

    /* Success. */
    gcmFOOTER_ARG("*Format=%d",
                  (Format == gcvNULL) ? 0 : *Format);
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gcoSURF_GetTiling
**
**  Get surface tiling.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gceTILING * Tiling
**          Pointer to variable that receives the tiling of the gcoSURF object.
*/
gceSTATUS
gcoSURF_GetTiling(
    IN gcoSURF Surface,
    OUT gceTILING * Tiling
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (Tiling != gcvNULL)
    {
        /* Return the surface tiling. */
        *Tiling = Surface->info.tiling;
    }

    /* Success. */
    gcmFOOTER_ARG("*Tiling=%d",
                  (Tiling == gcvNULL) ? 0 : *Tiling);
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gcoSURF_GetBottomBufferOffset
**
**  Get bottom buffer offset for split tiled surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT_PTR BottomBufferOffset
**          Pointer to variable that receives the offset value.
*/
gceSTATUS
gcoSURF_GetBottomBufferOffset(
    IN gcoSURF Surface,
    OUT gctUINT_PTR BottomBufferOffset
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (BottomBufferOffset != gcvNULL)
    {
        /* Return the surface tiling. */
        *BottomBufferOffset = Surface->info.bottomBufferOffset;
    }

    /* Success. */
    gcmFOOTER_ARG("*BottomBufferOffset=%d",
                  (BottomBufferOffset == gcvNULL) ? 0 : *BottomBufferOffset);
    return gcvSTATUS_OK;
}


#if gcdENABLE_3D
#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
/*******************************************************************************
**
**  gcoSURF_GetFlipBitmapOffset
**
**  Get real pixel address offset for flip bitmap.
**  Flip bitmap means the bitmap is RS target of a tile surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT_PTR FlipBitmapOffset
**          Pointer to variable that receives the offset value.
*/
gceSTATUS
gcoSURF_GetFlipBitmapOffset(
    IN gcoSURF Surface,
    OUT gctUINT_PTR FlipBitmapOffset
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (FlipBitmapOffset != gcvNULL)
    {
        /* Return the surface tiling. */
        *FlipBitmapOffset = Surface->info.flipBitmapOffset;
    }

    /* Success. */
    gcmFOOTER_ARG("*FlipBitmapOffset=%d",
                  (FlipBitmapOffset == gcvNULL) ? 0 : *FlipBitmapOffset);
    return gcvSTATUS_OK;
}
#endif

gceSTATUS
gcoSURF_SetSharedLock(
    IN gcoSURF Surface,
    IN gctPOINTER sharedLock
    )
{
    if(Surface)
    {
        Surface->info.sharedLock = sharedLock;
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gcoSURF_GetFence(
    IN gcoSURF surf
    )
{
#if gcdSYNC
    if (surf)
    {
        gctBOOL fenceEnable;
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);
        if(fenceEnable)
        {
            gcmLOCK_SHARE_OBJ((&surf->info));
            gcoHARDWARE_GetFence(gcvNULL, &surf->info.fenceCtx);
            gcmUNLOCK_SHARE_OBJ((&surf->info));
        }
        else
        {
            surf->info.fenceStatus = gcvFENCE_GET;
        }
    }
#endif
    return gcvSTATUS_OK;
}

gceSTATUS
gcoSURF_WaitFence(
    IN gcoSURF surf
    )
{
#if gcdSYNC
    if (surf)
    {
        gctBOOL fenceEnable;
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);
        if(fenceEnable)
        {
            gcmLOCK_SHARE_OBJ((&surf->info));
            gcoHARDWARE_WaitFence(gcvNULL, surf->info.fenceCtx);
            gcmUNLOCK_SHARE_OBJ((&surf->info));
        }
        else
        {
            if(surf->info.fenceStatus == gcvFENCE_GET)
            {
                surf->info.fenceStatus = gcvFENCE_ENABLE;
                gcoHARDWARE_SetFenceEnabled(gcvNULL, gcvTRUE);
                gcoHAL_Commit(gcvNULL, gcvTRUE);
            }
        }
    }
#endif
    return gcvSTATUS_OK;
}
#endif

/*******************************************************************************
**
**  gcoSURF_Lock
**
**  Lock the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Physical address array of the surface:
**          For YV12, Address[0] is for Y channel,
**                    Address[1] is for U channel and
**                    Address[2] is for V channel;
**          For I420, Address[0] is for Y channel,
**                    Address[1] is for U channel and
**                    Address[2] is for V channel;
**          For NV12, Address[0] is for Y channel and
**                    Address[1] is for UV channel;
**          For all other formats, only Address[0] is used to return the
**          physical address.
**
**      gctPOINTER * Memory
**          Logical address array of the surface:
**          For YV12, Memory[0] is for Y channel,
**                    Memory[1] is for V channel and
**                    Memory[2] is for U channel;
**          For I420, Memory[0] is for Y channel,
**                    Memory[1] is for U channel and
**                    Memory[2] is for V channel;
**          For NV12, Memory[0] is for Y channel and
**                    Memory[1] is for UV channel;
**          For all other formats, only Memory[0] is used to return the logical
**          address.
*/
gceSTATUS
gcoSURF_Lock(
    IN gcoSURF Surface,
    OPTIONAL OUT gctUINT32 * Address,
    OPTIONAL OUT gctPOINTER * Memory
    )
{
    gceSTATUS status;
    gctUINT8_PTR logical2 = gcvNULL, logical3 = gcvNULL;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
    /* Lock the surface. */
    gcmONERROR(_Lock(Surface, currentType));
#else
    /* Lock the surface. */
    gcmONERROR(_Lock(Surface));
#endif
    /* Determine surface addresses. */
    switch (Surface->info.format)
    {
    case gcvSURF_YV12:
    case gcvSURF_I420:
        Surface->info.node.count = 3;

        logical2                     = Surface->info.node.logical
                                     + Surface->info.uOffset;

        Surface->info.node.physical2 = Surface->info.node.physical
                                     + Surface->info.uOffset;

        logical3                     = Surface->info.node.logical
                                     + Surface->info.vOffset;

        Surface->info.node.physical3 = Surface->info.node.physical
                                     + Surface->info.vOffset;
        break;

    case gcvSURF_NV12:
    case gcvSURF_NV21:
    case gcvSURF_NV16:
    case gcvSURF_NV61:
        Surface->info.node.count = 2;

        logical2                     = Surface->info.node.logical
                                     + Surface->info.uOffset;

        Surface->info.node.physical2 = Surface->info.node.physical
                                     + Surface->info.uOffset;
        break;

    default:
        Surface->info.node.count = 1;
    }

    /* Set result. */
    if (Address != gcvNULL)
    {
        switch (Surface->info.node.count)
        {
        case 3:
            Address[2] = Surface->info.node.physical3;

            /* FALLTHROUGH */
        case 2:
            Address[1] = Surface->info.node.physical2;

            /* FALLTHROUGH */
        case 1:
            Address[0] = Surface->info.node.physical;

            /* FALLTHROUGH */
        default:
            break;
        }
    }

    if (Memory != gcvNULL)
    {
        switch (Surface->info.node.count)
        {
        case 3:
            Memory[2] = logical3;

            /* FALLTHROUGH */
        case 2:
            Memory[1] = logical2;

            /* FALLTHROUGH */
        case 1:
            Memory[0] = Surface->info.node.logical;

            /* FALLTHROUGH */
        default:
            break;
        }
    }

    /* Success. */
    gcmFOOTER_ARG("*Address=%08X *Memory=0x%x",
                  (Address == gcvNULL) ? 0 : *Address,
                  (Memory  == gcvNULL) ? gcvNULL : *Memory);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Unlock
**
**  Unlock the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gctPOINTER Memory
**          Pointer to mapped memory.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Unlock(
    IN gcoSURF Surface,
    IN gctPOINTER Memory
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif
    gcmHEADER_ARG("Surface=0x%x Memory=0x%x", Surface, Memory);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
    /* Unlock the surface. */
    gcmONERROR(_Unlock(Surface, currentType));
#else
    /* Unlock the surface. */
    gcmONERROR(_Unlock(Surface));
#endif
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
**  gcoSURF_Fill
**
**  Fill surface with specified value.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gcsPOINT_PTR Origin
**          Pointer to the origin of the area to be filled.
**          Assumed to (0, 0) if gcvNULL is given.
**
**      gcsSIZE_PTR Size
**          Pointer to the size of the area to be filled.
**          Assumed to the size of the surface if gcvNULL is given.
**
**      gctUINT32 Value
**          Value to be set.
**
**      gctUINT32 Mask
**          Value mask.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Fill(
    IN gcoSURF Surface,
    IN gcsPOINT_PTR Origin,
    IN gcsSIZE_PTR Size,
    IN gctUINT32 Value,
    IN gctUINT32 Mask
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

/*******************************************************************************
**
**  gcoSURF_Blend
**
**  Alpha blend two surfaces together.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to the source gcoSURF object.
**
**      gcoSURF DestSurface
**          Pointer to the destination gcoSURF object.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin within the source.
**          If gcvNULL is specified, (0, 0) origin is assumed.
**
**      gcsPOINT_PTR DestOrigin
**          The origin within the destination.
**          If gcvNULL is specified, (0, 0) origin is assumed.
**
**      gcsSIZE_PTR Size
**          The size of the area to be blended.
**
**      gceSURF_BLEND_MODE Mode
**          One of the blending modes.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Blend(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsSIZE_PTR Size,
    IN gceSURF_BLEND_MODE Mode
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

#if gcdENABLE_3D
/******************************************************************************\
********************************* Support Code *********************************
\******************************************************************************/

/*******************************************************************************
**
**  _ConvertValue
**
**  Convert a value to a 32-bit color or depth value.
**
**  INPUT:
**
**      gceVALUE_TYPE ValueType
**          Type of value.
**
**      gcuVALUE Value
**          Value data.
**
**      gctUINT Bits
**          Number of bits used in output.
**
**  OUTPUT:
**
**      Nothing.
**
**  RETURNS:
**
**      gctUINT32
**          Converted or casted value.
*/
extern gctFLOAT _LinearToNonLinearConv(gctFLOAT lFloat);

static gctUINT32
_ConvertValue(
    IN gceVALUE_TYPE ValueType,
    IN gcuVALUE Value,
    IN gctUINT Bits
    )
{
    /* Setup maximum value. */
    gctUINT uMaxValue = (Bits == 32) ? ~0 : ((1 << Bits) - 1);
    gctUINT32 tmpRet = 0;
    gcmASSERT(Bits <= 32);

    /*
    ** Data conversion clear path:
    ** Here we need handle clamp here coz client driver just pass clear value down.
    ** Now we plan to support INT/UINT RT, floating depth or floating RT later, we need set gcvVALUE_FLAG_NEED_CLAMP
    ** base on surface format.
    */
    switch (ValueType & ~gcvVALUE_FLAG_MASK)
    {
    case gcvVALUE_UINT:
        return ((Value.uintValue > uMaxValue) ? uMaxValue : Value.uintValue);

    case gcvVALUE_INT:
        {
            gctUINT32 mask = (Bits == 32) ? ~0 : ((1 << Bits) - 1);
            gctINT iMinValue = (Bits == 32)? (1 << (Bits-1))   :((~( 1 << (Bits -1))) + 1);
            gctINT iMaxValue = (Bits == 32)? (~(1 << (Bits-1))): ((1 << (Bits - 1)) - 1);
            return gcmCLAMP(Value.intValue, iMinValue, iMaxValue) & mask;
        }

    case gcvVALUE_FIXED:
        {
            gctFIXED_POINT tmpFixedValue = Value.fixedValue;
            if (ValueType & gcvVALUE_FLAG_UNSIGNED_DENORM)
            {
                tmpFixedValue = gcmFIXEDCLAMP_0_TO_1(tmpFixedValue);
                /* Convert fixed point (0.0 - 1.0) into color value. */
                return (gctUINT32) (((gctUINT64)uMaxValue * tmpFixedValue) >> 16);
            }
            else if (ValueType & gcvVALUE_FLAG_SIGNED_DENORM)
            {
                gcmASSERT(0);
                return 0;
            }
            else
            {
                gcmASSERT(0);
                return 0;
            }
        }
        break;

    case gcvVALUE_FLOAT:
        {
            gctFLOAT tmpFloat = Value.floatValue;
            gctFLOAT sFloat = tmpFloat;

            if (ValueType & gcvVALUE_FLAG_GAMMAR)
            {
                gcmASSERT ((ValueType & gcvVALUE_FLAG_FLOAT_TO_FLOAT16) == 0);

                sFloat = _LinearToNonLinearConv(tmpFloat);
            }

            if (ValueType & gcvVALUE_FLAG_FLOAT_TO_FLOAT16)
            {
                gcmASSERT ((ValueType & gcvVALUE_FLAG_GAMMAR) == 0);
                tmpRet = (gctUINT32)gcoMATH_FloatToFloat16(*(gctUINT32*)&tmpFloat);
                return tmpRet;
            }
            else if (ValueType & gcvVALUE_FLAG_UNSIGNED_DENORM)
            {
                sFloat = gcmFLOATCLAMP_0_TO_1(sFloat);
                /* Convert floating point (0.0 - 1.0) into color value. */
                tmpRet = gcoMATH_Float2UInt(gcoMATH_Multiply(gcoMATH_UInt2Float(uMaxValue), sFloat));
                return tmpRet > uMaxValue ? uMaxValue : tmpRet;
            }
            else if (ValueType & gcvVALUE_FLAG_SIGNED_DENORM)
            {
                gcmASSERT(0);
                return 0;
            }
            else
            {
                tmpRet = *(gctUINT32*)&sFloat;
                return tmpRet > uMaxValue ? uMaxValue : tmpRet;
            }
        }
        break;

    default:
        return 0;
        break;
    }
}
gctUINT32
_ByteMaskToBitMask(
    gctUINT32 ClearMask
    )
{
    gctUINT32 clearMask = 0;

    /* Byte mask to bit mask. */
    if (ClearMask & 0x1)
    {
        clearMask |= 0xFF;
    }

    if (ClearMask & 0x2)
    {
        clearMask |= (0xFF << 8);
    }

    if (ClearMask & 0x4)
    {
        clearMask |= (0xFF << 16);
    }

    if (ClearMask & 0x8)
    {
        clearMask |= (0xFF << 24);
    }

    return clearMask;
}


static gceSTATUS
_ComputeClear(
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctUINT32 LayerIndex
    )
{
    gceSTATUS status;
    gctUINT32 maskRed, maskGreen, maskBlue, maskAlpha;
    gcsSURF_FORMAT_INFO_PTR info = &Surface->formatInfo;

    gceVALUE_TYPE clearValueType;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x LayerIndex=0x%d", Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(ClearArgs != gcvNULL);

    /* Test for clearing render target. */
    if (ClearArgs->flags & gcvCLEAR_COLOR)
    {
        Surface->clearMask[LayerIndex] = ((ClearArgs->colorMask & 0x1) << 2) | /* Red   */
                                          (ClearArgs->colorMask & 0x2)       | /* Green */
                                         ((ClearArgs->colorMask & 0x4) >> 2) | /* Blue  */
                                          (ClearArgs->colorMask & 0x8);        /* Alpha */
        maskRed   = (ClearArgs->colorMask & 0x1) ? 0xFFFFFFFF: 0;
        maskGreen = (ClearArgs->colorMask & 0x2) ? 0xFFFFFFFF: 0;
        maskBlue  = (ClearArgs->colorMask & 0x4) ? 0xFFFFFFFF: 0;
        maskAlpha = (ClearArgs->colorMask & 0x8) ? 0xFFFFFFFF: 0;

        Surface->clearBitMask[LayerIndex] =
           ((maskRed & ((1 << info->u.rgba.red.width) - 1)) << info->u.rgba.red.start)       |
           ((maskGreen & ((1 << info->u.rgba.green.width) - 1)) << info->u.rgba.green.start) |
           ((maskBlue & ((1 << info->u.rgba.blue.width) - 1)) << info->u.rgba.blue.start)    |
           ((maskAlpha & ((1 << info->u.rgba.alpha.width) - 1)) << info->u.rgba.alpha.start) ;

        Surface->clearBitMask[LayerIndex] |= (0xFFFFFFFF << info->bitsPerPixel);
        /* Dispatch on render target format. */
        switch (Surface->format)
        {
        case gcvSURF_X4R4G4B4: /* 12-bit RGB color without alpha channel. */
        case gcvSURF_A4R4G4B4: /* 12-bit RGB color with alpha channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red into 4-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 4) << 8)
                /* Convert green into 4-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.g, 4) << 4)
                /* Convert blue into 4-bit. */
                | _ConvertValue(clearValueType,
                                ClearArgs->color.b, 4)
                /* Convert alpha into 4-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.a, 4) << 12);

            /* Expand 16-bit color into 32-bit color. */
            Surface->clearValue[0] |= Surface->clearValue[0] << 16;
            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

        case gcvSURF_X1R5G5B5: /* 15-bit RGB color without alpha channel. */
        case gcvSURF_A1R5G5B5: /* 15-bit RGB color with alpha channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red into 5-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 5) << 10)
                /* Convert green into 5-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.g, 5) << 5)
                /* Convert blue into 5-bit. */
                | _ConvertValue(clearValueType,
                                ClearArgs->color.b, 5)
                /* Convert alpha into 1-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.a, 1) << 15);

            /* Expand 16-bit color into 32-bit color. */
            Surface->clearValue[0] |= Surface->clearValue[0] << 16;
            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

        case gcvSURF_R5G6B5: /* 16-bit RGB color without alpha channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red into 5-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 5) << 11)
                /* Convert green into 6-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.g, 6) << 5)
                /* Convert blue into 5-bit. */
                | _ConvertValue(clearValueType,
                                ClearArgs->color.b, 5);

            /* Expand 16-bit color into 32-bit color. */
            Surface->clearValue[0] |= Surface->clearValue[0] << 16;
            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

        case gcvSURF_YUY2:
            {
                gctUINT8 r, g, b;
                gctUINT8 y, u, v;
                gcmASSERT(LayerIndex == 0);
                /* Query YUY2 render target support. */
                if (gcoHAL_IsFeatureAvailable(gcvNULL,
                                              gcvFEATURE_YUY2_RENDER_TARGET)
                    != gcvSTATUS_TRUE)
                {
                    /* No, reject. */
                    gcmFOOTER_NO();
                    return gcvSTATUS_INVALID_ARGUMENT;
                }
                clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

                /* Get 8-bit RGB values. */
                r = (gctUINT8) _ConvertValue(clearValueType,
                                  ClearArgs->color.r, 8);

                g = (gctUINT8) _ConvertValue(clearValueType,
                                  ClearArgs->color.g, 8);

                b = (gctUINT8) _ConvertValue(clearValueType,
                                  ClearArgs->color.b, 8);

                /* Convert to YUV. */
                gcoHARDWARE_RGB2YUV(r, g, b, &y, &u, &v);

                /* Set the clear value. */
                Surface->clearValueUpper[0] =
                Surface->clearValue[0] =  ((gctUINT32) y)
                                     | (((gctUINT32) u) <<  8)
                                     | (((gctUINT32) y) << 16)
                                     | (((gctUINT32) v) << 24);

            }
            break;

        case gcvSURF_X8R8G8B8: /* 24-bit RGB without alpha channel. */
        case gcvSURF_A8R8G8B8: /* 24-bit RGB with alpha channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red to 8-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 8) << 16)
                /* Convert green to 8-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.g, 8) << 8)
                /* Convert blue to 8-bit. */
                |  _ConvertValue(clearValueType,
                                ClearArgs->color.b, 8)
                    /* Convert alpha to 8-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.a, 8) << 24);

            /* Test for VAA. */
            if (ClearArgs->flags & gcvCLEAR_HAS_VAA)
            {
                if (Surface->format == gcvSURF_X8R8G8B8)
                {
                    /* Convert to C4R4G4B4C4R4G4B4. */
                    Surface->clearValue[0]
                        = ((Surface->clearValue[0] & 0x00000F)      )
                        | ((Surface->clearValue[0] & 0x0000F0) << 12)
                        | ((Surface->clearValue[0] & 0x000F00) >>  4)
                        | ((Surface->clearValue[0] & 0x00F000) <<  8)
                        | ((Surface->clearValue[0] & 0x0F0000) >>  8)
                        | ((Surface->clearValue[0] & 0xF00000) <<  4);
                }
            }

            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;
        case gcvSURF_R8_1_X8R8G8B8: /* 32-bit R8 without bga channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red to 8-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 8) << 16)
                /* Convert green to 8-bit. */
                | 0xFF000000;

            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

        case gcvSURF_G8R8_1_X8R8G8B8: /* 32-bit R8 without bga channel. */
            gcmASSERT(LayerIndex == 0);
            clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);

            Surface->clearValue[0]
                /* Convert red to 8-bit. */
                = (_ConvertValue(clearValueType,
                                 ClearArgs->color.r, 8) << 16)
                /* Convert green to 8-bit. */
                | (_ConvertValue(clearValueType,
                                 ClearArgs->color.g, 8) << 8)
                /* Convert green to 8-bit. */
                | 0xFF000000;

            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

         case gcvSURF_G8R8:
             /* 16-bit RG color without alpha channel. */
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);
             Surface->clearValue[0]
                 /* Convert red to 8-bit. */
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 8))
                 /* Convert green to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 8) << 8);

             /* Expand 16-bit color into 32-bit color. */
             Surface->clearValue[0] |= Surface->clearValue[0] << 16;

             Surface->clearValueUpper[0] = Surface->clearValue[0];

             break;

         case gcvSURF_A8_SBGR8: /* 24-bit RGB with alpha channel. */
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType    |
                               gcvVALUE_FLAG_UNSIGNED_DENORM |
                               gcvVALUE_FLAG_GAMMAR);
             Surface->clearValue[0]
                 /* Convert red to 8-bit. */
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 8))
                 /* Convert green to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 8) << 8)
                 /* Convert blue to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                 ClearArgs->color.b, 8) << 16);

             clearValueType &= ~gcvVALUE_FLAG_GAMMAR;
             Surface->clearValue[0] |= (_ConvertValue(clearValueType,
                                                    ClearArgs->color.a, 8) << 24);

             Surface->clearValueUpper[0] = Surface->clearValue[0];

             break;

         case gcvSURF_X2R10G10B10:
         case gcvSURF_A2R10G10B10:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_UNSIGNED_DENORM);
             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 /* Convert red to 10-bit. */
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 10) << 20)
                 /* Convert green to 10-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 10) << 10)
                 /* Convert blue to 10-bit. */
                 | _ConvertValue(clearValueType,
                                 ClearArgs->color.b, 10)
                 /* Convert alpha to 2-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 2) << 30);

             break;

         case gcvSURF_X2B10G10R10:
         case gcvSURF_A2B10G10R10:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType| gcvVALUE_FLAG_UNSIGNED_DENORM);
             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 /* Convert red to 10-bit. */
                 = _ConvertValue(clearValueType,
                                 ClearArgs->color.r, 10)
                 /* Convert green to 10-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 10) << 10)
                 /* Convert blue to 10-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.b, 10) << 20)
                 /* Convert alpha to 2-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 2) << 30);

             break;

          case gcvSURF_A8B12G12R12_2_A8R8G8B8:
             clearValueType = (ClearArgs->color.valueType| gcvVALUE_FLAG_UNSIGNED_DENORM);
             switch (LayerIndex)
             {
             case 0:
                 Surface->clearValueUpper[0] =
                 Surface->clearValue[0] =
                    /* Convert red upper 4 bits to 8-bit*/
                    ( ((_ConvertValue(clearValueType,
                                     ClearArgs->color.r, 12) & 0xf00) >> 4) << 16)
                    /* Convert green upper 4 bits to 8-bit*/
                    | (((_ConvertValue(clearValueType,
                                     ClearArgs->color.g, 12) & 0xf00) >> 4) << 8)
                    /* Convert blue upper 4 bits to 8-bit*/
                    | ((_ConvertValue(clearValueType,
                                    ClearArgs->color.b, 12) & 0xf00) >> 4)
                        /* Convert alpha to 8-bit. */
                    | (_ConvertValue(clearValueType,
                                     ClearArgs->color.a, 8) << 24);

                 break;

             case 1:
                 Surface->clearValueUpper[1] =
                 Surface->clearValue[1] =
                    /* Convert red lower 8 bits to 8-bit. */
                    ( (_ConvertValue(clearValueType,
                                     ClearArgs->color.r, 12) & 0xff) << 16)
                    /* Convert green lower 8 bits to 8-bit. */
                    | ((_ConvertValue(clearValueType,
                                     ClearArgs->color.g, 12) & 0xff) << 8)
                    /* Convert blue lower 8 bits to 8-bit. */
                    | (_ConvertValue(clearValueType,
                                    ClearArgs->color.b, 12) & 0xff)
                        /* Convert alpha to 8-bit. */
                    | (_ConvertValue(clearValueType,
                                     ClearArgs->color.a, 8) << 24);
                 break;

             default:
                 gcmASSERT(0);
                 break;
             }
             break;

         case gcvSURF_R5G5B5A1:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType| gcvVALUE_FLAG_UNSIGNED_DENORM);
             Surface->clearValue[0]
                 /* Convert red into 5-bit. */
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 5) << 11)
                 /* Convert green into 5-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 5) << 6)
                 /* Convert blue into 5-bit. */
                 | (_ConvertValue(clearValueType,
                                 ClearArgs->color.b, 5) << 1)
                 /* Convert alpha into 1-bit. */
                 | _ConvertValue(clearValueType,
                                  ClearArgs->color.a, 1);

             /* Expand 16-bit color into 32-bit color. */
             Surface->clearValue[0] |= Surface->clearValue[0] << 16;
             Surface->clearValueUpper[0] = Surface->clearValue[0];

             break;

         case gcvSURF_A2B10G10R10UI:
         case gcvSURF_A2B10G10R10UI_1_A8R8G8B8:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 /* Convert red to 10-bit. */
                 = _ConvertValue(clearValueType,
                                 ClearArgs->color.r, 10)
                 /* Convert green to 10-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 10) << 10)
                 /* Convert blue to 10-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.b, 10) << 20)
                 /* Convert alpha to 2-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 2) << 30);

             break;

         case gcvSURF_A8B8G8R8I:
         case gcvSURF_A8B8G8R8UI:
         case gcvSURF_A8B8G8R8I_1_A8R8G8B8:
         case gcvSURF_A8B8G8R8UI_1_A8R8G8B8:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 /* Convert red to 8-bit. */
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 8))
                 /* Convert green to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 8) << 8)
                 /* Convert blue to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                 ClearArgs->color.b, 8) << 16)
                 /* Convert alpha to 8-bit. */
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 8) << 24);

             break;


         case gcvSURF_R16F:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_FLOAT_TO_FLOAT16);

             Surface->clearValue[0] = _ConvertValue(clearValueType,
                                                  ClearArgs->color.r, 16);
             Surface->clearValue[0] |= Surface->clearValue[0] << 16;

             Surface->clearValueUpper[0] = Surface->clearValue[0];
             break;

         case gcvSURF_R16I:
         case gcvSURF_R16UI:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValue[0] = _ConvertValue(clearValueType,
                                                  ClearArgs->color.r, 16);
             Surface->clearValue[0] |= Surface->clearValue[0] << 16;

             Surface->clearValueUpper[0] = Surface->clearValue[0];
             break;

         case gcvSURF_G16R16F:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_FLOAT_TO_FLOAT16);

             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 16) << 16));

             break;

         case gcvSURF_G16R16I:
         case gcvSURF_G16R16UI:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 16) << 16));

             break;

         case gcvSURF_A16B16G16R16F:
         case gcvSURF_X16B16G16R16F:
             gcmASSERT(LayerIndex == 0);
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_FLOAT_TO_FLOAT16);

             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 16) << 16));
             Surface->clearValueUpper[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.b, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 16) << 16));
             break;

         case gcvSURF_A16B16G16R16F_2_A8R8G8B8:
             clearValueType = (ClearArgs->color.valueType | gcvVALUE_FLAG_FLOAT_TO_FLOAT16);

             switch (LayerIndex)
             {
             case 0:
                 Surface->clearValueUpper[0] =
                 Surface->clearValue[0]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.r, 16)
                     | (_ConvertValue(clearValueType,
                                      ClearArgs->color.g, 16) << 16));

                 break;
             case 1:
                 Surface->clearValueUpper[1] =
                 Surface->clearValue[1]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.b, 16)
                     | (_ConvertValue(clearValueType,
                                      ClearArgs->color.a, 16) << 16));

                 break;
             default:
                 gcmASSERT(0);
                 break;
             }
             break;

         case gcvSURF_A16B16G16R16I:
         case gcvSURF_X16B16G16R16I:
         case gcvSURF_A16B16G16R16UI:
         case gcvSURF_X16B16G16R16UI:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 16) << 16));
             Surface->clearValueUpper[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.b, 16)
                 | (_ConvertValue(clearValueType,
                                  ClearArgs->color.a, 16) << 16));
             break;

         case gcvSURF_A16B16G16R16I_2_A8R8G8B8:
         case gcvSURF_A16B16G16R16UI_2_A8R8G8B8:
             clearValueType = ClearArgs->color.valueType;

             switch (LayerIndex)
             {
             case 0:
                 Surface->clearValueUpper[0] =
                 Surface->clearValue[0]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.r, 16)
                     | (_ConvertValue(clearValueType,
                                      ClearArgs->color.g, 16) << 16));
                 break;

             case 1:
                 Surface->clearValueUpper[1] =
                 Surface->clearValue[1]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.b, 16)
                     | (_ConvertValue(clearValueType,
                                      ClearArgs->color.a, 16) << 16));
                 break;

             default:
                 gcmASSERT(0);
                 break;
             }
             break;


         case gcvSURF_R32F:
         case gcvSURF_R32I:
         case gcvSURF_R32UI:
         case gcvSURF_R32UI_1_A8R8G8B8:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValueUpper[0] =
             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 32));
             break;

         case gcvSURF_G32R32F:
         case gcvSURF_G32R32I:
         case gcvSURF_G32R32UI:
             gcmASSERT(LayerIndex == 0);
             clearValueType = ClearArgs->color.valueType;

             Surface->clearValue[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.r, 32));
             Surface->clearValueUpper[0]
                 = (_ConvertValue(clearValueType,
                                  ClearArgs->color.g, 32));
             break;

         case gcvSURF_A32B32G32R32F_2_G32R32F:
         case gcvSURF_X32B32G32R32F_2_G32R32F:
         case gcvSURF_A32B32G32R32I_2_G32R32I:
         case gcvSURF_X32B32G32R32I_2_G32R32I:
         case gcvSURF_A32B32G32R32UI_2_G32R32UI:
         case gcvSURF_X32B32G32R32UI_2_G32R32UI:
             clearValueType = ClearArgs->color.valueType;

             switch (LayerIndex)
             {
             case 0:
                 Surface->clearValue[0]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.r, 32));
                 Surface->clearValueUpper[0]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.g, 32));

                 break;
             case 1:
                 Surface->clearValue[1]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.b, 32));
                 Surface->clearValueUpper[1]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.a, 32));

                 break;
             default:
                 gcmASSERT(0);
                 break;
             }
             break;


         case gcvSURF_A32B32G32R32I_4_A8R8G8B8:
         case gcvSURF_A32B32G32R32UI_4_A8R8G8B8:
             clearValueType = ClearArgs->color.valueType;

             switch (LayerIndex)
             {
             case 0:
                 Surface->clearValueUpper[0] =
                 Surface->clearValue[0]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.r, 32));
                 break;
             case 1:
                 Surface->clearValueUpper[1] =
                 Surface->clearValue[1]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.g, 32));

                 break;
             case 2:
                 Surface->clearValueUpper[2] =
                 Surface->clearValue[2]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.b, 32));
                 break;

             case 3:
                 Surface->clearValueUpper[3] =
                 Surface->clearValue[3]
                     = (_ConvertValue(clearValueType,
                                      ClearArgs->color.a, 32));

                 break;
             default:
                 gcmASSERT(0);
                 break;
             }

             break;



         default:
            gcmTRACE(
                gcvLEVEL_ERROR,
                "%s(%d): Unknown format=%d",
                __FUNCTION__, __LINE__, Surface->format
                );

            /* Invalid surface format. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
         }
    }

    /* Test for clearing depth or stencil buffer. */
    if (ClearArgs->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
    {
        gctBOOL  clearDepth   = (ClearArgs->flags & gcvCLEAR_DEPTH) && ClearArgs->depthMask;
        gctBOOL  clearStencil = (ClearArgs->flags & gcvCLEAR_STENCIL) && ClearArgs->stencilMask;

        gcmASSERT(LayerIndex == 0);
        switch (Surface->format)
        {
        case gcvSURF_D16: /* 16-bit depth without stencil. */
            /* Write to all bytes for depth, no bytes for stencil. */
            Surface->clearMask[0]   = clearDepth ? 0xF : 0x0;
            break;

        case gcvSURF_D24S8: /* 24-bit depth with 8-bit stencil. */
            /* Write to upper 3 bytes for depth, lower byte for stencil. */
            Surface->clearMask[0] = clearDepth   ? 0xE : 0x0;
            Surface->clearMask[0] |= clearStencil ? 0x1 : 0x0;
            break;

        case gcvSURF_D24X8: /* 24-bit depth with no stencil. */
            /* Write all bytes for depth. */
            Surface->clearMask[0] = clearDepth ? 0xF : 0x0;
            break;

        case gcvSURF_S8:
            Surface->clearMask[0] = clearStencil ? 0x1: 0x0;
            break;

        default:
            /* Invalid depth buffer format. */
            break;
        }
        Surface->clearBitMask[0] = _ByteMaskToBitMask(Surface->clearMask[0]);

            /* Dispatch on depth format. */
        switch (Surface->format)
        {
        case gcvSURF_D16: /* 16-bit depth without stencil. */
            /* Convert depth value to 16-bit. */
            clearValueType = (gcvVALUE_FLOAT | gcvVALUE_FLAG_UNSIGNED_DENORM);
            Surface->clearValue[0] = _ConvertValue(clearValueType,
                                                   ClearArgs->depth,
                                                   16);

            /* Expand 16-bit depth value into 32-bit. */
            Surface->clearValue[0] |= (Surface->clearValue[0] << 16);
            Surface->clearValueUpper[0] = Surface->clearValue[0];

            break;

        case gcvSURF_D24S8: /* 24-bit depth with 8-bit stencil. */
            /* Convert depth value to 24-bit. */
            clearValueType = (gcvVALUE_FLOAT | gcvVALUE_FLAG_UNSIGNED_DENORM);
            Surface->clearValue[0] = _ConvertValue(clearValueType,
                                                   ClearArgs->depth,
                                                   24) << 8;

            /* Combine with masked stencil value. */
            Surface->clearValue[0] |= (ClearArgs->stencil & 0xFF);
            Surface->clearValueUpper[0] = Surface->clearValue[0];
            break;

        case gcvSURF_D24X8: /* 24-bit depth with no stencil. */
            /* Convert depth value to 24-bit. */
            clearValueType = (gcvVALUE_FLOAT | gcvVALUE_FLAG_UNSIGNED_DENORM);
            Surface->clearValue[0] = _ConvertValue(clearValueType,
                                                   ClearArgs->depth,
                                                   24) << 8;
            Surface->clearValueUpper[0] = Surface->clearValue[0];
            break;

        case gcvSURF_S8:
            Surface->clearValue[0] = (ClearArgs->stencil & 0xFF);
            Surface->clearValue[0] |= (Surface->clearValue[0] << 8);
            Surface->clearValue[0] |= (Surface->clearValue[0] << 16);
            Surface->clearValueUpper[0] = Surface->clearValue[0];
            break;

        default:
            /* Invalid depth buffer format. */
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        /* Compute HZ clear value. */
        gcmONERROR(gcoHARDWARE_HzClearValueControl(Surface->format,
                                                   Surface->clearValue[0],
                                                   &Surface->clearValueHz,
                                                   gcvNULL));

    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the error. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_ClearRectEx(
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctUINT32 LayerIndex
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsRECT_PTR rect;
    gcmHEADER_ARG("Surface=0x%08x ClearArgs=0x%x, LayerIndex=%d",
                  Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Surface != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(ClearArgs != gcvNULL);

    rect = ClearArgs->clearRect;

    /* Clamp the coordinates to surface dimensions. */
    rect->left   = gcmMAX(rect->left,   0);
    rect->top    = gcmMAX(rect->top,    0);
    rect->right  = gcmMIN(rect->right,  (gctINT32) Surface->alignedWidth);
    rect->bottom = gcmMIN(rect->bottom, (gctINT32) Surface->alignedHeight);

    /* Test for a valid rectangle. */
    if ((rect->left < rect->right) && (rect->top < rect->bottom))
    {
        /* Compute clear values. */
        gcmONERROR(_ComputeClear(Surface, ClearArgs, LayerIndex));

        /* Test for clearing render target. */
        if ((ClearArgs->flags & gcvCLEAR_COLOR)
          && (Surface->clearMask[LayerIndex] != 0x0))
        {
            status = gcvSTATUS_NOT_SUPPORTED;

            /* Try hardware clear if requested or default. */
            if (!(ClearArgs->flags & gcvCLEAR_WITH_CPU_ONLY))
            {
                status = gcoHARDWARE_Clear(gcvNULL,
                                           Surface,
                                           rect->left, rect->top, rect->right, rect->bottom,
                                           Surface->clearValue[LayerIndex],
                                           Surface->clearValueUpper[LayerIndex],
                                           Surface->clearMask[LayerIndex]);
            }

            /* Try software clear if requested or default. */
            if (!(ClearArgs->flags & gcvCLEAR_WITH_GPU_ONLY)
                && gcmIS_ERROR(status))
            {
                status = gcoHARDWARE_ClearSoftware(gcvNULL,
                                                   Surface,
                                                   rect->left, rect->top, rect->right, rect->bottom,
                                                   Surface->clearValue[LayerIndex],
                                                   Surface->clearValueUpper[LayerIndex],
                                                   Surface->clearMask[LayerIndex],
                                                   0xFF);
            }
        }

        if (status == gcvSTATUS_NOT_ALIGNED)
        {
            goto OnError;
        }

        /* Break now if status is error. */
        gcmONERROR(status);

        /* Test for clearing depth or stencil buffer. */
        if (ClearArgs->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
        {
            gcmASSERT(LayerIndex == 0);
            if (Surface->clearMask[0] != 0)
            {
                status = gcvSTATUS_NOT_SUPPORTED;

                /* Try hardware clear if requested or default. */
                if (!(ClearArgs->flags & gcvCLEAR_WITH_CPU_ONLY))
                {
                    if (!(ClearArgs->flags & gcvCLEAR_STENCIL) || (ClearArgs->stencilMask == 0xff))
                    {
                        status = gcoHARDWARE_Clear(gcvNULL,
                                                   Surface,
                                                   rect->left, rect->top,
                                                   rect->right, rect->bottom,
                                                   Surface->clearValue[0],
                                                   Surface->clearValueUpper[0],
                                                   Surface->clearMask[0]);
                    }
                }

                /* Try software clear if requested or default. */
                if (!(ClearArgs->flags & gcvCLEAR_WITH_GPU_ONLY)
                    && gcmIS_ERROR(status))
                {
                    status = gcoHARDWARE_ClearSoftware(gcvNULL,
                                                       Surface,
                                                       rect->left, rect->top, rect->right, rect->bottom,
                                                       Surface->clearValue[0],
                                                       Surface->clearValueUpper[0],
                                                       Surface->clearMask[0],
                                                       ClearArgs->stencilMask);
                }
            }
        }

        if (ClearArgs->flags & gcvCLEAR_HZ)
        {
            gctUINT width = 0, height = 0;
            gcsSURF_INFO hzSurf = {0};
            gcsSURF_FORMAT_INFO_PTR formatInfo = gcvNULL;

            /* Compute the hw specific clear window. */
            gcmONERROR(
                gcoHARDWARE_ComputeClearWindow(gcvNULL,
                                               Surface->stride,
                                               &width,
                                               &height));

            hzSurf.alignedWidth  = width;
            hzSurf.alignedHeight = height;
            hzSurf.rect.left     = 0;
            hzSurf.rect.top      = 0;
            hzSurf.rect.right    = width;
            hzSurf.rect.bottom   = height;
            hzSurf.format        = gcvSURF_A8R8G8B8;
            hzSurf.node          = Surface->hzNode;
            hzSurf.tiling        = Surface->tiling;
            hzSurf.stride        = width * 4;
            hzSurf.bottomBufferOffset = Surface->bottomBufferOffset;

            gcoSURF_QueryFormat(gcvSURF_A8R8G8B8, &formatInfo);

            hzSurf.formatInfo = *formatInfo;

            /* Send clear command to hardware. */
            gcmONERROR(
                gcoHARDWARE_Clear(gcvNULL,
                                  &hzSurf,
                                  0, 0, width, height,
                                  Surface->clearValueHz,
                                  Surface->clearValueHz,
                                  0xF));
        }
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


static gceSTATUS
_ClearRect(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctUINT32 LayerIndex
    )
{
    gceSTATUS status;
    gcsRECT_PTR rect;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x, LayerIndex=%d",
                   Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmDEBUG_VERIFY_ARGUMENT(ClearArgs != gcvNULL);

    rect = ClearArgs->clearRect;

    /* Adjust the rect according to the msaa sample. */
    rect->left   *= Surface->info.samples.x;
    rect->right  *= Surface->info.samples.x;
    rect->top    *= Surface->info.samples.y;
    rect->bottom *= Surface->info.samples.y;

    do
    {
        gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                      "gcoSURF_Clear: Clearing surface 0x%x @ 0x%08X",
                      Surface,
                      Surface->info.node.physical);

        if (!(ClearArgs->flags & gcvCLEAR_WITH_CPU_ONLY)
            && ((Surface->info.samples.x > 1)
                || (Surface->info.samples.y > 1))
            && ((gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_FAST_MSAA) == gcvTRUE) ||
                (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_SMALL_MSAA) == gcvTRUE)))
        {
            status = gcvSTATUS_NOT_ALIGNED;
            break;
        }

        if (ClearArgs->flags & gcvCLEAR_WITH_GPU_ONLY)
        {
            gctUINT originX, originY;
            gctUINT sizeX,   sizeY;
            gcsRECT_PTR clearRect;

            /* Get shortcurt. */
            clearRect = ClearArgs->clearRect;

            /* Query resolve clear alignment for this surface. */
            gcmVERIFY_OK(
                gcoHARDWARE_GetSurfaceResolveAlignment(gcvNULL,
                                                       &Surface->info,
                                                       &originX,
                                                       &originY,
                                                       &sizeX,
                                                       &sizeY));

            if ((clearRect->left & (originX - 1)) ||
                (clearRect->top  & (originY - 1)) ||
                (   (rect->right  < Surface->info.rect.right) &&
                    ((clearRect->right  - clearRect->left) & (sizeX - 1))) ||
                (   (rect->bottom < Surface->info.rect.bottom) &&
                    ((clearRect->bottom - clearRect->top ) & (sizeY - 1))))
            {
                /*
                 * Quickly reject if not resolve aligned.
                 * Avoid decompress and disable tile status below.
                 */
                status = gcvSTATUS_NOT_ALIGNED;
                break;
            }
        }

        /* Flush the tile status and decompress the buffers. */
        gcmERR_BREAK(gcoSURF_DisableTileStatus(Surface, gcvTRUE));

        status =_ClearRectEx(&Surface->info, ClearArgs, LayerIndex);

        if (status == gcvSTATUS_NOT_ALIGNED)
        {
            break;
        }

        gcmERR_BREAK(status);
    }
    while (gcvFALSE);

    if ((ClearArgs->flags & gcvCLEAR_DEPTH)
        && (Surface->info.hzNode.size > 0))
    {
        gcsSURF_INFO hzSurf = {0};
        gcsSURF_CLEAR_ARGS clearArgs;
        gcsRECT hzRect;

        gcoOS_MemCopy(&clearArgs, ClearArgs, sizeof(gcsSURF_CLEAR_ARGS));

        hzRect.left =
        hzRect.top  = 0;
        hzRect.right =
        hzRect.bottom = gcvHZCLEAR_RECT;

        hzSurf.hzNode = Surface->info.hzNode;
        hzSurf.format = gcvSURF_UNKNOWN;
        hzSurf.rect.left   =
        hzSurf.rect.right  = 0;

        hzSurf.rect.bottom   =
        hzSurf.rect.top      =
        hzSurf.alignedHeight =
        hzSurf.alignedWidth  = gcvHZCLEAR_RECT;
        gcmSAFECASTSIZET(hzSurf.stride, Surface->info.hzNode.size);

        /* If full clear, clear HZ to clear value*/
        if (rect->left == 0 && rect->right >= Surface->info.rect.right &&
            rect->top == 0 && rect->bottom >= Surface->info.rect.bottom)
        {
            hzSurf.clearValueHz  = Surface->info.clearValueHz;
        }
        else
        {
            hzSurf.clearValueHz  = 0xffffffff;
        }

        if (Surface->info.tiling & gcvTILING_SPLIT_BUFFER)
        {
            hzSurf.tiling    = gcvMULTI_TILED;
            hzSurf.bottomBufferOffset = hzSurf.stride / 2;
        }
        else
        {
            hzSurf.tiling    = gcvTILED;
        }

        clearArgs.clearRect = &hzRect;
        clearArgs.flags = gcvCLEAR_HZ;

        _ClearRectEx(&hzSurf, &clearArgs, 0);

        Surface->info.hzDisabled = gcvFALSE;
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}


static gceSTATUS
_DoClearTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gctUINT32 TileStatusAddress,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctINT32 LayerIndex
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x TileStatusAddress=0x%08x ClearArgs=0x%x, LayerIndex=%d",
                  Surface, TileStatusAddress, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Surface != 0);
    gcmDEBUG_VERIFY_ARGUMENT(TileStatusAddress != 0);

    /* Compute clear values. */
    gcmONERROR(
        _ComputeClear(Surface, ClearArgs, LayerIndex));

    /* Always flush (and invalidate) the tile status cache. */
    gcmONERROR(
        gcoHARDWARE_FlushTileStatus(gcvNULL,
                                    Surface,
                                    gcvFALSE));

    gcmASSERT(LayerIndex == 0);

    /* Test for clearing render target. */
    if (ClearArgs->flags & gcvCLEAR_COLOR)
    {
        if (Surface->clearMask[0] != 0)
        {
            /* Send the clear command to the hardware. */
            status =
                gcoHARDWARE_ClearTileStatus(gcvNULL,
                                            Surface,
                                            TileStatusAddress,
                                            0,
                                            gcvSURF_RENDER_TARGET,
                                            Surface->clearValue[0],
                                            Surface->clearValueUpper[0],
                                            Surface->clearMask[0]);

            if (status == gcvSTATUS_NOT_SUPPORTED)
            {
                goto OnError;
            }

            gcmONERROR(status);
        }
        else
        {
            gcmFOOTER_ARG("%s", "gcvSTATUS_SKIP");
            return gcvSTATUS_SKIP;
        }
    }

    /* Test for clearing depth or stencil buffer. */
    if (ClearArgs->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
    {
        if (Surface->clearMask[0] != 0)
        {
            status = gcvSTATUS_NOT_SUPPORTED;

            if (!(ClearArgs->flags & gcvCLEAR_STENCIL) || (ClearArgs->stencilMask == 0xff))
            {
                /* Send clear command to hardware. */
                status = gcoHARDWARE_ClearTileStatus(gcvNULL,
                                                     Surface,
                                                     TileStatusAddress,
                                                     0,
                                                     gcvSURF_DEPTH,
                                                     Surface->clearValue[0],
                                                     Surface->clearValueUpper[0],
                                                     Surface->clearMask[0]);
            }

            if (status == gcvSTATUS_NOT_SUPPORTED)
            {
                goto OnError;
            }
            gcmONERROR(status);

            /* Send semaphore from RASTER to PIXEL. */
            gcmONERROR(
                gcoHARDWARE_Semaphore(gcvNULL,
                                      gcvWHERE_RASTER,
                                      gcvWHERE_PIXEL,
                                      gcvHOW_SEMAPHORE,
                                      gcvNULL));
        }
        else
        {
            gcmFOOTER_ARG("%s", "gcvSTATUS_SKIP");
            return gcvSTATUS_SKIP;
        }
    }

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
**  _ClearHzTileStatus
**
**  Perform a hierarchical Z tile status clear with the current depth values.
**  Note that this function does not recompute the clear colors, since it must
**  be called after gco3D_ClearTileStatus has cleared the depth tile status.
**
**  INPUT:
**
**      gco3D Engine
**          Pointer to an gco3D object.
**
**      gcsSURF_INFO_PTR Surface
**          Pointer of the depth surface to clear.
**
**      gcsSURF_NODE_PTR TileStatusAddress
**          Pointer to the hierarhical Z tile status node toclear.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS
_ClearHzTileStatus(
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_NODE_PTR TileStatus
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x TileStatus=0x%x",
                   Surface, TileStatus);

    /* Send clear command to hardware. */
    gcmONERROR(
        gcoHARDWARE_ClearTileStatus(gcvNULL,
                                    Surface,
                                    TileStatus->physical,
                                    TileStatus->size,
                                    gcvSURF_HIERARCHICAL_DEPTH,
                                    Surface->clearValueHz,
                                    Surface->clearValueHz,
                                    0xF));

    /* Send semaphore from RASTER to PIXEL. */
    gcmONERROR(
        gcoHARDWARE_Semaphore(gcvNULL,
                              gcvWHERE_RASTER,
                              gcvWHERE_PIXEL,
                              gcvHOW_SEMAPHORE,
                              gcvNULL));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/* Attempt to clear using tile status. */
static gceSTATUS
_ClearTileStatus(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctINT32 LayerIndex
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x LayerIndex=%d", Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmDEBUG_VERIFY_ARGUMENT(ClearArgs != gcvNULL);

    if (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    {
        gctBOOL saved = Surface->info.tileStatusDisabled;

        do
        {
            gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SURFACE,
                          "_ClearTileStatus: Clearing tile status 0x%x @ 0x%08X for"
                          "surface 0x%x",
                          Surface->info.tileStatusNode,
                          Surface->info.tileStatusNode.physical,
                          Surface);
            /* Turn on the tile status on in the beginning,
            ** So the later invalidate ts cache will not be skipped. */
            Surface->info.tileStatusDisabled = gcvFALSE;

            /* Clear the tile status. */
            status = _DoClearTileStatus(&Surface->info,
                                        Surface->info.tileStatusNode.physical,
                                        ClearArgs,
                                        LayerIndex);

            if (status == gcvSTATUS_SKIP)
            {
                /* Should restore the tile status when skip the clear. */
                Surface->info.tileStatusDisabled = saved;

                /* Nothing needed clearing, and no error has occurred. */
                status = gcvSTATUS_OK;
                break;
            }

            if (status == gcvSTATUS_NOT_SUPPORTED)
            {
                break;
            }

            gcmERR_BREAK(status);

            if ((ClearArgs->flags & gcvCLEAR_DEPTH) &&
                (Surface->info.hzTileStatusNode.pool != gcvPOOL_UNKNOWN))
            {
                /* Clear the hierarchical Z tile status. */
                status = _ClearHzTileStatus(&Surface->info,
                                            &Surface->info.hzTileStatusNode);

                if (status == gcvSTATUS_NOT_SUPPORTED)
                {
                    break;
                }

                gcmERR_BREAK(status);

                Surface->info.hzDisabled = gcvFALSE;
            }

            Surface->info.dirty = gcvTRUE;

            /* Reset the tile status. */
            gcmERR_BREAK(
                gcoSURF_EnableTileStatus(Surface));
        }
        while (gcvFALSE);

        /* Restore if failed. */
        if (gcmIS_ERROR(status))
        {
            Surface->info.tileStatusDisabled = saved;
        }
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

#if gcdPARTIAL_FAST_CLEAR
static gceSTATUS
_ClearTileStatusWindowAligned(
    IN gcsSURF_INFO_PTR Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctINT32 LayerIndex,
    OUT gcsRECT_PTR AlignedRect
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x LayerIndex=%d",
                  Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(ClearArgs != 0);

    /* No MRT support when tile status enabled for now. */
    gcmASSERT(LayerIndex == 0);

    /* Compute clear values. */
    gcmONERROR(_ComputeClear(Surface, ClearArgs, LayerIndex));

    /* Check clear masks. */
    if (Surface->clearMask[0] == 0)
    {
        /* Nothing to clear. */
        gcmFOOTER_ARG("%s", "gcvSTATUS_SKIP");
        return gcvSTATUS_SKIP;
    }

    /* Check clearValue changes. */
    if ((Surface->tileStatusDisabled == gcvFALSE)
    &&  (   (Surface->fcValue != Surface->clearValue[0])
        ||  (Surface->fcValueUpper != Surface->clearValueUpper[0]))
    )
    {
        /*
         * Tile status is on but clear value changed.
         * Fail back to 3D draw clear or disable tile status and continue
         * partial fast clear.
         */
#if gcdHAL_3D_DRAWBLIT
        status = gcvSTATUS_NOT_SUPPORTED;
        gcmFOOTER();
        return status;
#else
        gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, Surface, gcvTRUE));
#endif
    }

    /* Flush the tile status cache. */
    gcmONERROR(gcoHARDWARE_FlushTileStatus(gcvNULL, Surface, gcvFALSE));

    /* Test for clearing render target. */
    if (ClearArgs->flags & gcvCLEAR_COLOR)
    {
        /* Send the clear command to the hardware. */
        status =
            gcoHARDWARE_ClearTileStatusWindowAligned(gcvNULL,
                                                     Surface,
                                                     gcvSURF_RENDER_TARGET,
                                                     Surface->clearValue[0],
                                                     Surface->clearValueUpper[0],
                                                     Surface->clearMask[0],
                                                     ClearArgs->clearRect,
                                                     AlignedRect);

        if (status == gcvSTATUS_NOT_SUPPORTED)
        {
            goto OnError;
        }

        gcmONERROR(status);
    }

    /* Test for clearing depth or stencil buffer. */
    if (ClearArgs->flags & (gcvCLEAR_DEPTH | gcvCLEAR_STENCIL))
    {
        if (Surface->hzNode.pool != gcvPOOL_UNKNOWN)
        {
            /* Can not support clear depth when HZ buffer exists. */
            status = gcvSTATUS_NOT_SUPPORTED;
            goto OnError;
        }

        /* Send the clear command to the hardware. */
        status =
            gcoHARDWARE_ClearTileStatusWindowAligned(gcvNULL,
                                                     Surface,
                                                     gcvSURF_DEPTH,
                                                     Surface->clearValue[0],
                                                     Surface->clearValueUpper[0],
                                                     Surface->clearMask[0],
                                                     ClearArgs->clearRect,
                                                     AlignedRect);

        if (status == gcvSTATUS_NOT_SUPPORTED)
        {
            goto OnError;
        }

        gcmONERROR(status);

        /* Send semaphore from RASTER to PIXEL. */
        gcmONERROR(
            gcoHARDWARE_Semaphore(gcvNULL,
                                  gcvWHERE_RASTER,
                                  gcvWHERE_PIXEL,
                                  gcvHOW_SEMAPHORE_STALL,
                                  gcvNULL));
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
_PartialFastClear(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctINT32 LayerIndex
    )
{
    gceSTATUS status;
    gcsRECT alignedRect;
    gctBOOL saved;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x LayerIndex=%d",
                  Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ClearArgs != gcvNULL);

    if ((Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN) ||
        (Surface->info.samples.x > 1) ||
        (Surface->info.samples.y > 1))
    {
        /* No tile status buffer or AA. */
        status = gcvSTATUS_NOT_SUPPORTED;
        goto OnError;
    }

    /* Backup previous tile status is state. */
    saved = Surface->info.tileStatusDisabled;

    /* Do the partial fast clear. */
    status = _ClearTileStatusWindowAligned(&Surface->info,
                                           ClearArgs,
                                           LayerIndex,
                                           &alignedRect);

    if (gcmIS_SUCCESS(status))
    {
#if gcdHAL_3D_DRAWBLIT
        gcsSURF_BLITDRAW_ARGS blitDrawArgs;
        gcsRECT_PTR clearRect;
        gcsRECT rect[4];
        gctINT count;
        gctINT i;
#endif

        /* Tile status is enabled if success, turn it on. */
        Surface->info.tileStatusDisabled = gcvFALSE;

        Surface->info.dirty = gcvTRUE;

        /* Reset the tile status. */
        gcmONERROR(gcoSURF_EnableTileStatus(Surface));

        if (saved)
        {
            /*
             * Invalidate tile status cache.
             * A case is that tile status is decompressed and disabled but still
             * cached. Tile status memory is changed in above clear, we need
             * invalidate tile status cache here.
             * Decompressing by resolve onto self will only read tile status,
             * hardware will drop tile status cache without write back in that
             * case. So here only 'invalidate' is done in 'flush'.
             */
            gcmONERROR(gcoSURF_FlushTileStatus(Surface, gcvFALSE));
        }

#if gcdHAL_3D_DRAWBLIT
        /* Now Use 3D drawblit to clear areas left. */
        gcoOS_ZeroMemory(&blitDrawArgs, gcmSIZEOF(blitDrawArgs));

        blitDrawArgs.version = gcvHAL_ARG_VERSION_V1;
        blitDrawArgs.uArgs.v1.type = gcvBLITDRAW_CLEAR;

        gcoOS_MemCopy(&blitDrawArgs.uArgs.v1.u.clear.clearArgs,
                      ClearArgs,
                      gcmSIZEOF(gcsSURF_CLEAR_ARGS));

        if (ClearArgs->flags & gcvCLEAR_COLOR)
        {
            blitDrawArgs.uArgs.v1.u.clear.rtSurface = Surface;
        }
        else
        {
            blitDrawArgs.uArgs.v1.u.clear.dsSurface = Surface;
        }

        /* Get not cleared area count. */
        clearRect = ClearArgs->clearRect;
        count = 0;

        if (alignedRect.left > clearRect->left)
        {
            rect[count].left   = clearRect->left;
            rect[count].top    = alignedRect.top;
            rect[count].right  = alignedRect.left;
            rect[count].bottom = alignedRect.bottom;
            count++;
        }

        if (alignedRect.top > clearRect->top)
        {
            rect[count].left   = clearRect->left;
            rect[count].top    = clearRect->top;
            rect[count].right  = clearRect->right;
            rect[count].bottom = alignedRect.top;
            count++;
        }

        if (alignedRect.right < clearRect->right)
        {
            rect[count].left   = alignedRect.right;
            rect[count].top    = alignedRect.top;
            rect[count].right  = clearRect->right;
            rect[count].bottom = alignedRect.bottom;
            count++;
        }

        if (alignedRect.bottom < clearRect->bottom)
        {
            rect[count].left   = clearRect->left;
            rect[count].top    = alignedRect.bottom;
            rect[count].right  = clearRect->right;
            rect[count].bottom = clearRect->bottom;
            count++;
        }

        for (i = 0; i < count; i++)
        {
            /* Call blit draw to clear. */
            blitDrawArgs.uArgs.v1.u.clear.clearArgs.clearRect = &rect[i];
            gcmERR_BREAK(gcoHARDWARE_BlitDraw(&blitDrawArgs));
        }
#endif
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

static gceSTATUS
_Clear(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctINT32 LayerIndex,
    OUT gctBOOL * BlitDraw
    )
{
    gceSTATUS status;
    gcsRECT_PTR rect;
    gctBOOL fullSize = gcvFALSE;
    gctPOINTER memory[3] = {gcvNULL};

    gcmHEADER_ARG("Surface=0x%x ClearArg=0x%x LayerIndex=%d",
              Surface, ClearArgs, LayerIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ClearArgs != gcvNULL);

    rect = ClearArgs->clearRect;

    /* Lock the surface. */
    gcmONERROR(gcoSURF_Lock(Surface, gcvNULL, memory));

    if (ClearArgs->flags & gcvCLEAR_WITH_CPU_ONLY)
    {
        /* Software clear only. */
        status = _ClearRect(Surface, ClearArgs, LayerIndex);

        /* Unlock the surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(Surface, gcvNULL));

        /* Return the status. */
        gcmFOOTER();
        return status;
    }

    if ((ClearArgs->flags & gcvCLEAR_STENCIL)
    &&  Surface->info.hasStencilComponent
    )
    {
        Surface->info.canDropStencilPlane = gcvFALSE;
    }

    do
    {
        /* Hardware clear path. */
        status = gcvSTATUS_NOT_SUPPORTED;

        /* Test for entire surface clear. */
        if ((rect->left == 0)  &&  (rect->top == 0)
        &&  (rect->right  >= (Surface->info.rect.right  / Surface->info.samples.x))
        &&  (rect->bottom >= (Surface->info.rect.bottom / Surface->info.samples.x))
        )
        {
            fullSize = gcvTRUE;

            /*
             * 1. Best way: Fast clear.
             * Full fast clear when it is an entire surface clear.
             */
            status = _ClearTileStatus(Surface, ClearArgs, LayerIndex);
        }
#if gcdPARTIAL_FAST_CLEAR
        else
        {
            /*
             * 2. Partial fast clear + 3D draw clear.
             * Clear tile status window and than draw clear for not aligned parts.
             */
            status = _PartialFastClear(Surface, ClearArgs, LayerIndex);
        }
#endif

        if (gcmIS_SUCCESS(status))
        {
            /* Done. */
            break;
        }

#if gcdHAL_3D_DRAWBLIT
        {
            /*
             * 3. Try resolve clear if tile status is disabled.
             * resolve clear is better than below draw clear when no tile status.
             */
            gcsRECT tempRect;

            /* Save rect. Because the func will modify the parameter.*/
            gcoOS_MemCopy(&tempRect,
                          ClearArgs->clearRect,
                          gcmSIZEOF(gcsRECT));

            status = _ClearRect(Surface, ClearArgs, LayerIndex);

            /* Reset rect. Because the func will modify the parameter.*/
            if (gcmIS_ERROR(status))
            {
                gcoOS_MemCopy(ClearArgs->clearRect,
                              &tempRect,
                              gcmSIZEOF(gcsRECT));
            }

            if (gcmIS_SUCCESS(status))
            {
                /* Done. */
                break;
            }
        }

        if (gcoSURF_IsRenderable(Surface) == gcvSTATUS_OK)
        {
            /*
             * 4. Try 3D draw clear.
             * Resolve clear will need to decompress and disable tile status
             * before the actual clear. So 3D draw clear is bettern if tile
             * status is currently enabled.
             *
             * Another thing is that when draw clear succeeds, all layers of the
             * surface are cleared at the same time.
             */
            gcsSURF_BLITDRAW_ARGS blitDrawArgs;

            gcoOS_ZeroMemory(&blitDrawArgs, gcmSIZEOF(blitDrawArgs));
            blitDrawArgs.version = gcvHAL_ARG_VERSION_V1;
            blitDrawArgs.uArgs.v1.type = gcvBLITDRAW_CLEAR;

            gcoOS_MemCopy(&blitDrawArgs.uArgs.v1.u.clear.clearArgs,
                          ClearArgs,
                          gcmSIZEOF(gcsSURF_CLEAR_ARGS));

            if (ClearArgs->flags & gcvCLEAR_COLOR)
            {
                blitDrawArgs.uArgs.v1.u.clear.rtSurface = Surface;
            }
            else
            {
                blitDrawArgs.uArgs.v1.u.clear.dsSurface = Surface;
            }

            status = gcoHARDWARE_BlitDraw(&blitDrawArgs);

            if (gcmIS_SUCCESS(status))
            {
                /*
                 * Report the flag to caller, which means all layers of the
                 * surface are cleared.
                 */
                *BlitDraw = gcvTRUE;

                /* Done. */
                break;
            }
        }

#else
        /* Try resolve clear. */
        status = _ClearRect(Surface, ClearArgs, LayerIndex);

        if (gcmIS_SUCCESS(status))
        {
            /* Done. */
            break;
        }
#endif

#if !gcdHAL_3D_DRAWBLIT
        if (!(ClearArgs->flags & gcvCLEAR_WITH_GPU_ONLY))
#endif
        {
            /*
             * 6. Last, use software clear.
             * If no GPU-ONLY requested, try software clear.
             */
            gceCLEAR savedFlags = ClearArgs->flags;
            ClearArgs->flags &= ~gcvCLEAR_WITH_GPU_ONLY;
            ClearArgs->flags |= gcvCLEAR_WITH_CPU_ONLY;

            status = _ClearRect(Surface, ClearArgs, LayerIndex);

            /* Restore flags. */
            ClearArgs->flags = savedFlags;
        }
    }
    while (gcvFALSE);

    if (gcmIS_SUCCESS(status) &&  fullSize)
    {
        /* Set garbagePadded=0 if full clear */
        if (Surface->info.paddingFormat)
        {
            Surface->info.garbagePadded = gcvFALSE;
        }

        if (gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_PE_DITHER_FIX) == gcvFALSE)
        {
            gctUINT8 clearMask = Surface->info.clearMask[LayerIndex];

            /* Full mask clear can reset the deferDither3D flag */
            if ((clearMask == 0xF)

                ||  (   (clearMask == 0x7)
                &&  (   (Surface->info.format == gcvSURF_X8R8G8B8)
                ||  (Surface->info.format == gcvSURF_R5G6B5)))

                ||  (   (clearMask == 0xE)
                &&  Surface->info.hasStencilComponent
                &&  Surface->info.canDropStencilPlane)
                )
            {
                Surface->info.deferDither3D = gcvFALSE;
            }
        }
    }

    /* Unlock the surface. */
    gcmVERIFY_OK(gcoSURF_Unlock(Surface, gcvNULL));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


gceSTATUS
gcoSURF_Clear(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR  ClearArgs)
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_CLEAR_ARGS clearArgs;
    gctUINT originalOffset;
    gctUINT layerIndex;
    gcsRECT clearRect;
    gctINT  width;
    gctINT  height;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x", Surface, ClearArgs);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ClearArgs != 0);

    gcoOS_MemCopy(&clearArgs, ClearArgs, sizeof(gcsSURF_CLEAR_ARGS));

    if (Surface->info.vaa)
    {
        /* Add VAA to flags. */
        clearArgs.flags |= gcvCLEAR_HAS_VAA;
    }

    width  = Surface->info.rect.right  / Surface->info.samples.x;
    height = Surface->info.rect.bottom / Surface->info.samples.y;

    if (ClearArgs->clearRect == gcvNULL)
    {
        /* Full screen. */
        clearRect.left = 0;
        clearRect.top  = 0;
        clearRect.right  = width;
        clearRect.bottom = height;
    }
    else
    {
        /* Intersect with surface size. */
        clearRect.left   = gcmMAX(0, ClearArgs->clearRect->left);
        clearRect.top    = gcmMAX(0, ClearArgs->clearRect->top);
        clearRect.right  = gcmMIN(width,  ClearArgs->clearRect->right);
        clearRect.bottom = gcmMIN(height, ClearArgs->clearRect->bottom);
    }

    clearArgs.clearRect = &clearRect;

    originalOffset = Surface->info.offset;

    for (layerIndex = 0; layerIndex < Surface->info.formatInfo.layers; layerIndex++)
    {
        gctBOOL blitDraw = gcvFALSE;
        gctSIZE_T offset = clearArgs.offset + Surface->info.layerSize * layerIndex;

        gcmONERROR(gcoSURF_SetOffset(Surface, offset));

        status = _Clear(Surface, &clearArgs, layerIndex, &blitDraw);

        if (status == gcvSTATUS_NOT_ALIGNED)
        {
            goto OnError;
        }

        gcmONERROR(status);

        if (blitDraw)
        {
            break;
        }
    }

    gcmONERROR(gcoSURF_SetOffset(Surface, originalOffset));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  depr_gcoSURF_Resolve
**
**  Resolve the surface to the frame buffer.  Resolve means that the frame is
**  finished and should be displayed into the frame buffer, either by copying
**  the data or by flipping to the surface, depending on the hardware's
**  capabilities.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into, or gcvNULL if the resolve surface is
**          a physical address.
**
**      gctUINT32 DestAddress
**          Physical address of the destination surface.
**
**      gctPOINTER DestBits
**          Logical address of the destination surface.
**
**      gctINT DestStride
**          Stride of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gceSURF_TYPE DestType
**          Type of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gceSURF_FORMAT DestFormat
**          Format of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gctUINT DestWidth
**          Width of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gctUINT DestHeight
**          Height of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
depr_gcoSURF_Resolve(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gctUINT32 DestAddress,
    IN gctPOINTER DestBits,
    IN gctINT DestStride,
    IN gceSURF_TYPE DestType,
    IN gceSURF_FORMAT DestFormat,
    IN gctUINT DestWidth,
    IN gctUINT DestHeight
    )
{
    gcsPOINT rectOrigin;
    gcsPOINT rectSize;
    gceSTATUS status;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x DestAddress=%08x DestBits=0x%x "
              "DestStride=%d DestType=%d DestFormat=%d DestWidth=%u "
              "DestHeight=%u",
              SrcSurface, DestSurface, DestAddress, DestBits, DestStride,
              DestType, DestFormat, DestWidth, DestHeight);

    /* Validate the source surface. */
    gcmVERIFY_OBJECT(SrcSurface, gcvOBJ_SURF);

    /* Fill in coordinates. */
    rectOrigin.x = 0;
    rectOrigin.y = 0;

    if (DestSurface == gcvNULL)
    {
        rectSize.x = DestWidth;
        rectSize.y = DestHeight;
    }
    else
    {
        rectSize.x = DestSurface->info.alignedWidth;
        rectSize.y = DestSurface->info.alignedHeight;
    }

    /* Call generic function. */
    status = depr_gcoSURF_ResolveRect(
        SrcSurface, DestSurface,
        DestAddress, DestBits, DestStride, DestType, DestFormat,
        DestWidth, DestHeight,
        &rectOrigin, &rectOrigin, &rectSize
        );
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  depr_gcoSURF_ResolveRect
**
**  Resolve a rectangular area of a surface to another surface.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into, or gcvNULL if the resolve surface is
**          a physical address.
**
**      gctUINT32 DestAddress
**          Physical address of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gctPOINTER DestBits
**          Logical address of the destination surface.
**
**      gctINT DestStride
**          Stride of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gceSURF_TYPE DestType
**          Type of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gceSURF_FORMAT DestFormat
**          Format of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gctUINT DestWidth
**          Width of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gctUINT DestHeight
**          Height of the destination surface.
**          If 'DestSurface' is not gcvNULL, this parameter is ignored.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin of the source area to be resolved.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be resolved.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be resolved.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
depr_gcoSURF_ResolveRect(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gctUINT32 DestAddress,
    IN gctPOINTER DestBits,
    IN gctINT DestStride,
    IN gceSURF_TYPE DestType,
    IN gceSURF_FORMAT DestFormat,
    IN gctUINT DestWidth,
    IN gctUINT DestHeight,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gctPOINTER destination[3] = {gcvNULL};
    gctPOINTER mapInfo = gcvNULL;
    gcsSURF_INFO_PTR destInfo;
    struct _gcsSURF_INFO _destInfo;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x DestAddress=%08x DestBits=0x%x "
              "DestStride=%d DestType=%d DestFormat=%d DestWidth=%u "
              "DestHeight=%u SrcOrigin=0x%x DestOrigin=0x%x RectSize=0x%x",
              SrcSurface, DestSurface, DestAddress, DestBits, DestStride,
              DestType, DestFormat, DestWidth, SrcOrigin, DestOrigin,
              RectSize);

    /* Validate the source surface. */
    gcmVERIFY_OBJECT(SrcSurface, gcvOBJ_SURF);

    do
    {
        gcsPOINT rectSize;

        /* Destination surface specified? */
        if (DestSurface != gcvNULL)
        {
            /* Set the pointer to the structure. */
            destInfo = &DestSurface->info;

            /* Lock the surface. */
            if (DestBits == gcvNULL)
            {
                gcmERR_BREAK(
                    gcoSURF_Lock(DestSurface,
                                 gcvNULL,
                                 destination));
            }
        }

        /* Create a surface wrapper. */
        else
        {
            /* Fill the surface info structure. */
            _destInfo.type          = DestType;
            _destInfo.format        = DestFormat;
            _destInfo.rect.left     = 0;
            _destInfo.rect.top      = 0;
            _destInfo.rect.right    = DestWidth;
            _destInfo.rect.bottom   = DestHeight;
            _destInfo.alignedWidth  = DestWidth;
            _destInfo.alignedHeight = DestHeight;
            _destInfo.rotation      = gcvSURF_0_DEGREE;
            _destInfo.orientation   = gcvORIENTATION_TOP_BOTTOM;
            _destInfo.stride        = DestStride;
            _destInfo.size          = DestStride * DestHeight;
            _destInfo.node.valid    = gcvTRUE;
            _destInfo.node.pool     = gcvPOOL_UNKNOWN;
            _destInfo.node.physical = DestAddress;
            _destInfo.node.logical  = DestBits;
            _destInfo.samples.x     = 1;
            _destInfo.samples.y     = 1;

            /* Set the pointer to the structure. */
            destInfo = &_destInfo;

            gcmERR_BREAK(
                gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                  DestType,
                                                  DestFormat,
                                                  &_destInfo.alignedWidth,
                                                  &_destInfo.alignedHeight,
                                                  1,
                                                  &_destInfo.tiling,
                                                  &_destInfo.superTiled));

            /* Map the user memory. */
            if (DestBits != gcvNULL)
            {
                gcmERR_BREAK(
                    gcoOS_MapUserMemory(gcvNULL,
                                        DestBits,
                                        destInfo->size,
                                        &mapInfo,
                                        &destInfo->node.physical));
            }
        }

        /* Determine the resolve size. */
        if ((DestOrigin->x == 0)
        &&  (DestOrigin->y == 0)
        &&  (RectSize->x == destInfo->rect.right)
        &&  (RectSize->y == destInfo->rect.bottom)
        )
        {
            /* Full destination resolve, a special case. */
            rectSize.x = destInfo->alignedWidth;
            rectSize.y = destInfo->alignedHeight;
        }
        else
        {
            rectSize.x = RectSize->x;
            rectSize.y = RectSize->y;
        }

        /* Perform resolve. */
        gcmERR_BREAK(
            gcoHARDWARE_ResolveRect(gcvNULL,
                                    &SrcSurface->info,
                                    destInfo,
                                    SrcOrigin,
                                    DestOrigin,
                                    &rectSize,
                                    gcvFALSE));
    }
    while (gcvFALSE);

    /* Unlock the surface. */
    if (destination[0] != gcvNULL)
    {
        /* Unlock the resolve buffer. */
        gcmVERIFY_OK(
            gcoSURF_Unlock(DestSurface,
                           destination[0]));
    }

    /* Unmap the surface. */
    if (mapInfo != gcvNULL)
    {
        /* Unmap the user memory. */
        gcmVERIFY_OK(
            gcoHAL_ScheduleUnmapUserMemory(gcvNULL,
                                           mapInfo,
                                           destInfo->size,
                                           destInfo->node.physical,
                                           DestBits));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

#define gcdCOLOR_SPACE_CONVERSION_NONE         0
#define gcdCOLOR_SPACE_CONVERSION_TO_LINEAR    1
#define gcdCOLOR_SPACE_CONVERSION_TO_NONLINEAR 2

gceSTATUS
gcoSURF_MixSurfacesCPU(
    IN gcoSURF TargetSurface,
    IN gcoSURF SourceSurface,
    IN gctFLOAT mixFactor
    )
{
    gceSTATUS status;
    gcoSURF srcSurf, dstSurf;
    gctPOINTER srcAddr[3] = {gcvNULL};
    gctPOINTER dstAddr[3] = {gcvNULL};
    _PFNreadPixel pfReadPixel = gcvNULL;
    _PFNwritePixel pfWritePixel = gcvNULL;
    _PFNcalcPixelAddr pfSrcCalcAddr = gcvNULL;
    _PFNcalcPixelAddr pfDstCalcAddr = gcvNULL;
    gctINT i, j;

    gcsSURF_FORMAT_INFO *srcFmtInfo, *dstFmtInfo;

    srcSurf = SourceSurface;
    dstSurf = TargetSurface;

    /* Target and Source surfaces need to have same dimensions and format. */
    if ((dstSurf->info.rect.bottom != srcSurf->info.rect.bottom)
     || (dstSurf->info.rect.right != srcSurf->info.rect.right)
     || (dstSurf->info.format != srcSurf->info.format)
     || (dstSurf->info.type != srcSurf->info.type)
     || (dstSurf->info.tiling != srcSurf->info.tiling))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* MSAA case not supported. */
    if (dstSurf->info.samples.x > 1 || dstSurf->info.samples.y > 1 ||
        srcSurf->info.samples.x > 1 || srcSurf->info.samples.y > 1)
    {
        return gcvSTATUS_NOT_SUPPORTED;
    }

    srcFmtInfo = &SourceSurface->info.formatInfo;
    dstFmtInfo = &dstSurf->info.formatInfo;

    /*
    ** Integer format upload/blit, the data type must be totally matched.
    */
    if (((srcFmtInfo->fmtDataType == gcvFORMAT_DATATYPE_UNSIGNED_INTEGER) ||
         (srcFmtInfo->fmtDataType == gcvFORMAT_DATATYPE_SIGNED_INTEGER)) &&
         (srcFmtInfo->fmtDataType != dstFmtInfo->fmtDataType))
    {
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* TODO: those function pointers can be recorded in gcoSURF */
    pfReadPixel  = gcoSURF_GetReadPixelFunc(srcSurf);
    pfWritePixel = gcoSURF_GetWritePixelFunc(dstSurf);
    pfSrcCalcAddr = gcoHARDWARE_GetProcCalcPixelAddr(gcvNULL, srcSurf);
    pfDstCalcAddr = gcoHARDWARE_GetProcCalcPixelAddr(gcvNULL, dstSurf);

    /* set color space conversion flag */
    gcmASSERT(srcSurf->info.colorSpace == dstSurf->info.colorSpace);

    if (!pfReadPixel || !pfWritePixel || !pfSrcCalcAddr || !pfDstCalcAddr)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Flush the GPU cache */
    gcmONERROR(gcoHARDWARE_FlushTileStatus(gcvNULL, &srcSurf->info, gcvTRUE));
    gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, &dstSurf->info, gcvTRUE));

    /* Synchronize with the GPU. */
    /* TODO: if both of the surfaces previously were not write by GPU,
    ** or already did the sync, no need to do it again.
    */
    gcmONERROR(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));
    gcmONERROR(gcoHARDWARE_Commit(gcvNULL));
    gcmONERROR(gcoHARDWARE_Stall(gcvNULL));

    /* Lock the surfaces. */
    gcmONERROR(gcoSURF_Lock(srcSurf, gcvNULL, srcAddr));
    gcmONERROR(gcoSURF_Lock(dstSurf, gcvNULL, dstAddr));

    /* Src surface might be written by GPU previously, CPU need to invalidate
    ** its cache before reading.
    ** Dst surface alo need invalidate CPU cache to guarantee CPU cache is coherent
    ** with memory, so it's correct to flush out after writing.
    */
    gcmONERROR(gcoSURF_NODE_Cache(&srcSurf->info.node,
                                  srcAddr[0],
                                  srcSurf->info.size,
                                  gcvCACHE_INVALIDATE));
    gcmONERROR(gcoSURF_NODE_Cache(&dstSurf->info.node,
                                  dstAddr[0],
                                  dstSurf->info.size,
                                  gcvCACHE_INVALIDATE));

    for (j = srcSurf->info.rect.top; j < srcSurf->info.rect.bottom; ++j)
    {
        for (i = srcSurf->info.rect.left; i < srcSurf->info.rect.right; ++i)
        {
            gcsPIXEL internalSrc, internalDst;
            gctPOINTER srcAddr_l[4];
            gctPOINTER dstAddr_l[4];

            pfSrcCalcAddr(srcSurf, (gctSIZE_T)i, (gctSIZE_T)j, (gctSIZE_T)0, srcAddr_l);
            pfDstCalcAddr(dstSurf, (gctSIZE_T)i, (gctSIZE_T)j, (gctSIZE_T)0, dstAddr_l);

            pfReadPixel(srcAddr_l, &internalSrc);
            pfReadPixel(dstAddr_l, &internalDst);

            if (srcSurf->info.colorSpace == gcvSURF_COLOR_SPACE_NONLINEAR)
            {
                gcoSURF_PixelToLinear(&internalSrc);
                gcoSURF_PixelToLinear(&internalDst);
            }

            /* Mix the pixels. */
            internalDst.pf.r = internalDst.pf.r * (1 - mixFactor) + internalSrc.pf.r * mixFactor;
            internalDst.pf.g = internalDst.pf.g * (1 - mixFactor) + internalSrc.pf.g * mixFactor;
            internalDst.pf.b = internalDst.pf.b * (1 - mixFactor) + internalSrc.pf.b * mixFactor;
            internalDst.pf.a = internalDst.pf.a * (1 - mixFactor) + internalSrc.pf.a * mixFactor;
            internalDst.pf.d = internalDst.pf.d * (1 - mixFactor) + internalSrc.pf.d * mixFactor;
            internalDst.pf.s = internalDst.pf.s * (1 - mixFactor) + internalSrc.pf.s * mixFactor;

            if (srcSurf->info.colorSpace == gcvSURF_COLOR_SPACE_NONLINEAR)
            {
                gcoSURF_PixelToNonLinear(&internalDst);
            }

            pfWritePixel(&internalDst, dstAddr_l, 0);
        }
    }

    /* Dst surface was written by CPU and might be accessed by GPU later */
    gcmONERROR(gcoSURF_NODE_Cache(&dstSurf->info.node,
                                  dstAddr[0],
                                  dstSurf->info.size,
                                  gcvCACHE_CLEAN));

#if gcdDUMP
    /* verify the source */
    gcmDUMP_BUFFER(gcvNULL,
                   "verify",
                   srcSurf->info.node.physical,
                   srcSurf->info.node.logical,
                   0,
                   srcSurf->info.size);
    /* upload the destination */
    gcmDUMP_BUFFER(gcvNULL,
                   "memory",
                   dstSurf->info.node.physical,
                   dstSurf->info.node.logical,
                   0,
                   dstSurf->info.size);
#endif

OnError:
    /* Unlock the surfaces. */
    if (srcAddr[0])
    {
        gcoSURF_Unlock(srcSurf, srcAddr[0]);
    }
    if (dstAddr[0])
    {
        gcoSURF_Unlock(dstSurf, dstAddr[0]);
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_Resample
**
**  Determine the ratio between the two non-multisampled surfaces and resample
**  based on the ratio.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface.
**
**  OUTPUT:
**
**      Nothing.
*/

#define RESAMPLE_FLAG 0xBAAD1234

gceSTATUS
gcoSURF_Resample(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface
    )
{
    gceSTATUS status;
    gcsSAMPLES srcSamples = { 0, 0 };
    gcsSAMPLES destSamples = { 0, 0 };
    gcsPOINT rectOrigin = { 0, 0 };
    gcsPOINT dstOrigin = { RESAMPLE_FLAG, RESAMPLE_FLAG };
    gcsPOINT rectSize;
    gctUINT   srcOffset, desOffset;
    gctUINT32 srcPhysical[3] = {~0U}, destPhysical[3] = {~0U};
    gctPOINTER srcLogical[3] = {gcvNULL}, destLogical[3] = {gcvNULL}, tempDestLogical[3] = {gcvNULL};
    gctUINT i;
    gcoSURF tempDestSurface = gcvNULL;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x", SrcSurface, DestSurface);

    /* Validate the surfaces. */
    gcmVERIFY_OBJECT(SrcSurface, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(DestSurface, gcvOBJ_SURF);

    srcOffset  = SrcSurface->info.offset;
    desOffset = DestSurface->info.offset;
    SrcSurface->info.offset = 0;
    DestSurface->info.offset = 0;

    /* Both surfaces have to be non-multisampled. */
    if ((SrcSurface->info.samples.x  != 1)
    ||  (SrcSurface->info.samples.y  != 1)
    ||  (DestSurface->info.samples.x != 1)
    ||  (DestSurface->info.samples.y != 1)
    )
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }


#if gcdDUMP
    {
        gctSTRING env;
        gctINT simFrame = 0;
        gctUINT frameCount;
        gcoOS_GetEnv(gcvNULL, "SIM_Frame", &env);
        gcoHAL_FrameInfoOps(gcvNULL,
                            gcvFRAMEINFO_FRAME_NUM,
                            gcvFRAMEINFO_OP_GET,
                            &frameCount);
        gcoOS_StrToInt(env, &simFrame);
        if ((gctINT)frameCount < simFrame)
        {
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }
#endif
    /* Determine the samples and fill in coordinates. */
    if (SrcSurface->info.rect.right == DestSurface->info.rect.right)
    {
        srcSamples.x  = 1;
        destSamples.x = 1;
        rectSize.x    = SrcSurface->info.rect.right;
    }

    else if (SrcSurface->info.rect.right / 2 == (DestSurface->info.rect.right))
    {
        srcSamples.x  = 2;
        destSamples.x = 1;
        rectSize.x    = DestSurface->info.rect.right;
    }

    else if (SrcSurface->info.rect.right == DestSurface->info.rect.right / 2)
    {
        srcSamples.x  = 1;
        destSamples.x = 2;
        rectSize.x    = SrcSurface->info.rect.right;
    }

    else
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if (SrcSurface->info.rect.bottom == DestSurface->info.rect.bottom)
    {
        srcSamples.y  = 1;
        destSamples.y = 1;
        rectSize.y    = SrcSurface->info.rect.bottom;
    }

    else if (SrcSurface->info.rect.bottom / 2 == (DestSurface->info.rect.bottom))
    {
        srcSamples.y  = 2;
        destSamples.y = 1;
        rectSize.y    = DestSurface->info.rect.bottom;
    }

    else if (SrcSurface->info.rect.bottom == DestSurface->info.rect.bottom / 2)
    {
        srcSamples.y  = 1;
        destSamples.y = 2;
        rectSize.y    = SrcSurface->info.rect.bottom;
    }

    else
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Overwrite surface samples. */
    SrcSurface->info.samples  = srcSamples;
    DestSurface->info.samples = destSamples;

    /* 3D texture. */
    if (SrcSurface->depth != DestSurface->depth)
    {
        /* Need to be reducing depth, with resample. */
        if (SrcSurface->depth < DestSurface->depth)
        {
            gcmONERROR(gcvSTATUS_INVALID_REQUEST);
        }

        /* Lock source surface if larger than 1 depth. */
        gcmONERROR(gcoSURF_Lock(SrcSurface, srcPhysical, srcLogical));

        /* Lock destination surface if larger than 1 depth. */
        gcmONERROR(gcoSURF_Lock(DestSurface, destPhysical, destLogical));

        gcoSURF_Construct(gcvNULL,
                          DestSurface->info.rect.right,
                          DestSurface->info.rect.bottom,
                          1,
                          DestSurface->info.type,
                          DestSurface->info.format,
                          gcvPOOL_DEFAULT,
                          &tempDestSurface);

        /* Lock tempDest once. */
        gcmONERROR(gcoSURF_Lock(tempDestSurface, gcvNULL, tempDestLogical));

        /* Downsample 2 slices, and mix them together.
           Ignore the last Src slice, if the size is odd. */
        for (i = 0; i < DestSurface->depth; ++i)
        {
            dstOrigin.x = dstOrigin.y = RESAMPLE_FLAG;
            /* Call generic function. */
            gcmONERROR(gcoSURF_ResolveRect(SrcSurface,
                                           DestSurface,
                                           &rectOrigin,
                                           &dstOrigin,
                                           &rectSize));

            /* Move to next source depth. */
            SrcSurface->info.node.physical  += SrcSurface->info.sliceSize;
            SrcSurface->info.node.logical   += SrcSurface->info.sliceSize;

            dstOrigin.x = dstOrigin.y = RESAMPLE_FLAG;

            gcmONERROR(gcoSURF_ResolveRect(SrcSurface,
                                           tempDestSurface,
                                           &rectOrigin,
                                           &dstOrigin,
                                           &rectSize));

            gcmONERROR(gcoSURF_MixSurfacesCPU(DestSurface,
                                              tempDestSurface,
                                              0.5f));

            /* Move to next depth. */
            SrcSurface->info.node.physical  += SrcSurface->info.sliceSize;
            SrcSurface->info.node.logical   += SrcSurface->info.sliceSize;
            DestSurface->info.node.physical += DestSurface->info.sliceSize;
            DestSurface->info.node.logical  += DestSurface->info.sliceSize;
        }

        /* Restore addresses. */
        SrcSurface->info.node.physical  = srcPhysical[0];
        SrcSurface->info.node.logical   = srcLogical[0];
        DestSurface->info.node.physical = destPhysical[0];
        DestSurface->info.node.logical  = destLogical[0];

        /* Unlock surfaces. */
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface,  srcLogical[0]));
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destLogical[0]));
        gcmVERIFY_OK(gcoSURF_Unlock(tempDestSurface, tempDestLogical[0]));
        gcmVERIFY_OK(gcoSURF_Destroy(tempDestSurface));
        tempDestSurface = gcvNULL;
    }
    /* CUBE/2DArray. */
    else if (SrcSurface->depth > 1)
    {
        /* Lock source surface if larger than 1 depth. */
        gcmONERROR(gcoSURF_Lock(SrcSurface, srcPhysical, srcLogical));

        /* Lock destination surface if larger than 1 depth. */
        gcmONERROR(gcoSURF_Lock(DestSurface, destPhysical, destLogical));

        for (i = 0; i < SrcSurface->depth; ++i)
        {
            dstOrigin.x = dstOrigin.y = RESAMPLE_FLAG;
            /* Call generic function. */
            gcmONERROR(gcoSURF_ResolveRect(SrcSurface,
                                           DestSurface,
                                           &rectOrigin,
                                           &dstOrigin,
                                           &rectSize));

            /* Move to next depth. */
            SrcSurface->info.node.physical  += SrcSurface->info.sliceSize;
            SrcSurface->info.node.logical   += SrcSurface->info.sliceSize;
            DestSurface->info.node.physical += DestSurface->info.sliceSize;
            DestSurface->info.node.logical  += DestSurface->info.sliceSize;
        }

        /* Restore addresses. */
        SrcSurface->info.node.physical  = srcPhysical[0];
        SrcSurface->info.node.logical   = srcLogical[0];
        DestSurface->info.node.physical = destPhysical[0];
        DestSurface->info.node.logical  = destLogical[0];

        /* Unlock surfaces. */
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface,  srcLogical[0]));
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destLogical[0]));
    }
    else
    {
        /* Call generic function. */
        gcmONERROR(gcoSURF_ResolveRect(SrcSurface,
                                       DestSurface,
                                       &rectOrigin,
                                       &dstOrigin,
                                       &rectSize));
    }

    /* Restore samples. */
    SrcSurface->info.samples.x  = 1;
    SrcSurface->info.samples.y  = 1;
    DestSurface->info.samples.x = 1;
    DestSurface->info.samples.y = 1;
    /*Restore offset */
    SrcSurface->info.offset = srcOffset;
    DestSurface->info.offset = desOffset;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:

    /* Restore samples. */
    SrcSurface->info.samples.x  = 1;
    SrcSurface->info.samples.y  = 1;
    DestSurface->info.samples.x = 1;
    DestSurface->info.samples.y = 1;
    SrcSurface->info.offset = srcOffset;
    DestSurface->info.offset = desOffset;


    if (srcLogical[0] != gcvNULL)
    {
        /* Restore addresses. */
        SrcSurface->info.node.physical = srcPhysical[0];
        SrcSurface->info.node.logical  = srcLogical[0];

        /* Unlock surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface, srcLogical[0]));
    }

    if (destLogical[0] != gcvNULL)
    {
        /* Restore addresses. */
        DestSurface->info.node.physical = destPhysical[0];
        DestSurface->info.node.logical  = destLogical[0];

        /* Unlock surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destLogical[0]));
    }

    if (tempDestSurface != gcvNULL)
    {
        if (tempDestLogical[0] != gcvNULL)
        {
            /* Unlock surface. */
            gcmVERIFY_OK(gcoSURF_Unlock(tempDestSurface, tempDestLogical[0]));
        }

        gcmVERIFY_OK(gcoSURF_Destroy(tempDestSurface));
    }


    /* Fallback to CPU resampling */
    if (gcmIS_ERROR(status))
    {
        gcsSURF_BLIT_ARGS  blitArgs;
        gcePATCH_ID patchID = gcvPATCH_INVALID;

        gcoHAL_GetPatchID(gcvNULL, &patchID);

        if (patchID != gcvPATCH_GFXBENCH && patchID != gcvPATCH_FRUITNINJA)
        {
            gcoOS_ZeroMemory(&blitArgs, sizeof(blitArgs));
            blitArgs.srcX               = 0;
            blitArgs.srcY               = 0;
            blitArgs.srcZ               = 0;
            blitArgs.srcWidth           = SrcSurface->info.rect.right;
            blitArgs.srcHeight          = SrcSurface->info.rect.bottom;
            blitArgs.srcDepth           = SrcSurface->depth;
            blitArgs.srcSurface         = SrcSurface;
            blitArgs.dstX               = 0;
            blitArgs.dstY               = 0;
            blitArgs.dstZ               = 0;
            blitArgs.dstWidth           = DestSurface->info.rect.right;
            blitArgs.dstHeight          = DestSurface->info.rect.bottom;
            blitArgs.dstDepth           = DestSurface->depth;
            blitArgs.dstSurface         = DestSurface;
            blitArgs.xReverse           = gcvFALSE;
            blitArgs.yReverse           = gcvFALSE;
            blitArgs.scissorTest        = gcvFALSE;
            status = gcoSURF_BlitCPU(&blitArgs);
        }
        else
        {
            /* Skip SW GENERATION for low levels */
        }
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoSURF_GetResolveAlignment
**
**  Query the resolve alignment of the hardware.
**
**  INPUT:
**
**      gcsSURF_INFO_PTR Surface,
**          Pointer to an surface info.
**
**  OUTPUT:
**
**      gctUINT *originX,
**          X direction origin alignment
**
**      gctUINT *originY
**          Y direction origin alignemnt
**
**      gctUINT *sizeX,
**          X direction size alignment
**
**      gctUINT *sizeY
**          Y direction size alignemnt
**
*/
gceSTATUS
gcoSURF_GetResolveAlignment(
    IN gcoSURF Surface,
    OUT gctUINT *originX,
    OUT gctUINT *originY,
    OUT gctUINT *sizeX,
    OUT gctUINT *sizeY
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Query the tile sizes through gcoHARDWARE. */
    status = gcoHARDWARE_GetSurfaceResolveAlignment(gcvNULL, &Surface->info, originX, originY, sizeX, sizeY);

    gcmFOOTER_ARG("status=%d *originX=%d *originY=%d *sizeX=%d *sizeY=%d",
                  status, gcmOPT_VALUE(originX), gcmOPT_VALUE(originY),
                  gcmOPT_VALUE(sizeX), gcmOPT_VALUE(sizeY));
    return status;
}


gceSTATUS gcoSURF_TranslateRotationRect(
    gcsSIZE_PTR rtSize,
    gceSURF_ROTATION rotation,
    gcsRECT * rect
    )
{
    gctFLOAT tx,ty,tz,tw;
    gctFLOAT tmp;

    tx  = (gctFLOAT)rect->left;
    ty  = (gctFLOAT)rect->top;

    tz  = (gctFLOAT)rect->right;
    tw  = (gctFLOAT)rect->bottom;

    /* 1, translate to rt center */
    tx = tx - (gctFLOAT)rtSize->width / 2.0f;
    ty = ty - (gctFLOAT)rtSize->height / 2.0f;

    tz = tz - (gctFLOAT)rtSize->width / 2.0f;
    tw = tw - (gctFLOAT)rtSize->height / 2.0f;

    /* cos? -sin?    90D  x = -y  180D x = -x 270D x = y
       sin? cos?          y = x        y = -y      y = -x */

    switch (rotation)
    {
        case gcvSURF_90_DEGREE:
            /* 2, rotate */
            tmp = tx;
            tx  = -ty;
            ty  = tmp;

            tmp = tz;
            tz  = -tw;
            tw  = tmp;
            /* 3, translate back  */
            tx = tx + (gctFLOAT)rtSize->height / 2.0f;
            ty = ty + (gctFLOAT)rtSize->width / 2.0f;

            tz = tz + (gctFLOAT)rtSize->height / 2.0f;
            tw = tw + (gctFLOAT)rtSize->width / 2.0f;

            /* Form the new (left,top) (right,bottom) */
            rect->left   = (gctINT32)tz;
            rect->top    = (gctINT32)ty;
            rect->right  = (gctINT32)tx;
            rect->bottom = (gctINT32)tw;
            break;

        case gcvSURF_270_DEGREE:
            /* 2, rotate */
            tmp = tx;
            tx  = ty;
            ty  = -tmp;

            tmp = tz;
            tz  = tw;
            tw  = -tmp;
            /* 3, translate back  */
            tx = tx + (gctFLOAT)rtSize->height / 2.0f;
            ty = ty + (gctFLOAT)rtSize->width / 2.0f;

            tz = tz + (gctFLOAT)rtSize->height / 2.0f;
            tw = tw + (gctFLOAT)rtSize->width / 2.0f;

            /* Form the new (left,top) (right,bottom) */
            rect->left   = (gctINT32)tx;
            rect->top    = (gctINT32)tw;
            rect->right  = (gctINT32)tz;
            rect->bottom = (gctINT32)ty;
            break;

         default :
            break;
    }

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_Resolve
**
**  Resolve the surface to the frame buffer.  Resolve means that the frame is
**  finished and should be displayed into the frame buffer, either by copying
**  the data or by flipping to the surface, depending on the hardware's
**  capabilities.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into.
**
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Resolve(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface
    )
{
    gceSTATUS status;

    gcsSURF_RESOLVE_ARGS args;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x", SrcSurface, DestSurface);

    args.version = gcvHAL_ARG_VERSION_V1;

    args.uArgs.v1.yInverted = gcvFALSE;

    status = gcoSURF_ResolveEx(SrcSurface,
                               DestSurface,
                               &args);

    gcmFOOTER();

    return status;

}


/*******************************************************************************
**
**  gcoSURF_ResolveEx
**
**  Resolve the surface to the frame buffer.  Resolve means that the frame is
**  finished and should be displayed into the frame buffer, either by copying
**  the data or by flipping to the surface, depending on the hardware's
**  capabilities.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into.
**
**      gcsSURF_RESOLVE_ARGS *args
**          Pointer to extra resolve argument (multi-version)
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_ResolveEx(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsSURF_RESOLVE_ARGS *args
    )
{
    gcsPOINT rectOrigin;
    gcsPOINT rectSize;
    gceSTATUS status;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x args=0x%x", SrcSurface, DestSurface, args);

    /* Validate the surfaces. */
    gcmVERIFY_OBJECT(SrcSurface, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(DestSurface, gcvOBJ_SURF);

    /* Fill in coordinates. */
    rectOrigin.x = 0;
    rectOrigin.y = 0;
    rectSize.x   = DestSurface->info.alignedWidth;
    rectSize.y   = DestSurface->info.alignedHeight;

    /* Call generic function. */
    status = gcoSURF_ResolveRectEx(
        SrcSurface, DestSurface,
        &rectOrigin, &rectOrigin, &rectSize, args
        );

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_ResolveRect
**
**  Resolve a rectangular area of a surface to another surface.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin of the source area to be resolved.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be resolved.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be resolved.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_ResolveRect(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gcsSURF_RESOLVE_ARGS args;
    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x SrcOrigin=0x%x "
                  "DestOrigin=0x%x RectSize=0x%x",
                  SrcSurface, DestSurface, SrcOrigin, DestOrigin, RectSize);

    args.version = gcvHAL_ARG_VERSION_V1;

    args.uArgs.v1.yInverted = gcvFALSE;

    status = gcoSURF_ResolveRectEx(SrcSurface,
                                   DestSurface,
                                   SrcOrigin,
                                   DestOrigin,
                                   RectSize,
                                   &args);


    gcmFOOTER();

    return status;

}


/*******************************************************************************
**
**  gcoSURF_ResolveRectEx
**
**  Resolve a rectangular area of a surface to another surface.
**  Support explicilitly yInverted resolve request in argument.
**  Note: RS only support tile<->linear yInverted resolve, other
**        case will be fallbacked to SW.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to a gcoSURF object that represents the source surface
**          to be resolved.
**
**      gcoSURF DestSurface
**          Pointer to a gcoSURF object that represents the destination surface
**          to resolve into.
**
**      gcsPOINT_PTR SrcOrigin
**          The origin of the source area to be resolved.
**
**      gcsPOINT_PTR DestOrigin
**          The origin of the destination area to be resolved.
**
**      gcsPOINT_PTR RectSize
**          The size of the rectangular area to be resolved.
**
**      gcsSURF_RESOLVE_ARGS *args
**          Point to extra resolve argument (multiple-version)
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_ResolveRectEx(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize,
    IN gcsSURF_RESOLVE_ARGS *args
    )
{
    gceSTATUS status;
    gctPOINTER source[3];
    gctPOINTER target[3];

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x SrcOrigin=0x%x "
                  "DestOrigin=0x%x RectSize=0x%x args=0x%x",
                  SrcSurface, DestSurface, SrcOrigin, DestOrigin, RectSize, args);

    /* Validate the surfaces. */
    gcmVERIFY_OBJECT(SrcSurface, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(DestSurface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(SrcOrigin != gcvNULL);
    gcmVERIFY_ARGUMENT(DestOrigin != gcvNULL);
    gcmVERIFY_ARGUMENT(RectSize != gcvNULL);
    gcmVERIFY_ARGUMENT(args != gcvNULL);

    do
    {
        gcsPOINT rectSize;
        gctUINT32 physical[3] = {0};
        gctINT srcMaxWidth, srcMaxHeight;
        gctINT dstMaxWidth, dstMaxHeight;
        gctBOOL resampleBlt = gcvFALSE;
        gctBOOL yInverted = gcvFALSE;

        if ((DestOrigin->x == (gctINT) RESAMPLE_FLAG) &&
            (DestOrigin->y == (gctINT) RESAMPLE_FLAG))
        {
            resampleBlt = gcvTRUE;
            DestOrigin->x = SrcOrigin->x;
            DestOrigin->y = SrcOrigin->y;
        }

        yInverted = args->uArgs.v1.yInverted;

        /* Reset lock pointers. */
        source[0] = gcvNULL;
        target[0] = gcvNULL;

        /* Lock the surfaces. */
        gcmERR_BREAK(gcoSURF_Lock(SrcSurface, gcvNULL, source));
        gcmERR_BREAK(gcoSURF_Lock(DestSurface, physical, target));

        DestSurface->info.canDropStencilPlane = SrcSurface->info.canDropStencilPlane;

        gcmERR_BREAK(
            gcoHARDWARE_FlushTileStatus(gcvNULL,
                                        &SrcSurface->info,
                                        gcvFALSE));

        if (SrcSurface->info.type == gcvSURF_BITMAP)
        {
            /* Flush the CPU cache. Source would've been rendered by the CPU. */
            gcmERR_BREAK(gcoSURF_NODE_Cache(
                &SrcSurface->info.node,
                source[0],
                SrcSurface->info.size,
                gcvCACHE_CLEAN));
        }

        if (DestSurface->info.type == gcvSURF_BITMAP)
        {
            gcmERR_BREAK(gcoSURF_NODE_Cache(
                &DestSurface->info.node,
                target[0],
                DestSurface->info.size,
                gcvCACHE_FLUSH));
        }

        /* Make sure we don't go beyond the source surface. */
        srcMaxWidth  = SrcSurface->info.alignedWidth  - SrcOrigin->x;
        srcMaxHeight = SrcSurface->info.alignedHeight - SrcOrigin->y;

        /* Make sure we don't go beyond the target surface. */
        dstMaxWidth  = DestSurface->info.alignedWidth  - DestOrigin->x;
        dstMaxHeight = DestSurface->info.alignedHeight - DestOrigin->y;

        if (resampleBlt)
        {
            /* Determine the resolve size. */
            if ((DestOrigin->x == 0) &&
                (RectSize->x == DestSurface->info.rect.right) &&
                (DestSurface->info.alignedWidth <= SrcSurface->info.alignedWidth / SrcSurface->info.samples.x))
            {
                /* take destination aligned size only if source is MSAA-required aligned accordingly. */
                rectSize.x = DestSurface->info.alignedWidth;
            }
            else
            {
                rectSize.x = gcmMIN(dstMaxWidth, (gctINT)(SrcSurface->info.alignedWidth / SrcSurface->info.samples.x));
            }

            /* Determine the resolve size. */
            if ((DestOrigin->y == 0) &&
                (RectSize->y == DestSurface->info.rect.bottom)&&
                (DestSurface->info.alignedHeight <= SrcSurface->info.alignedHeight / SrcSurface->info.samples.y))
            {
                /* take destination aligned size only if source is MSAA-required aligned accordingly. */
                rectSize.y = DestSurface->info.alignedHeight;
            }
            else
            {
                rectSize.y = gcmMIN(dstMaxHeight, (gctINT)(SrcSurface->info.alignedHeight / SrcSurface->info.samples.y));
            }

            gcmASSERT(rectSize.x <= srcMaxWidth);
            gcmASSERT(rectSize.y <= srcMaxHeight);
            gcmASSERT(rectSize.x <= dstMaxWidth);
            gcmASSERT(rectSize.y <= dstMaxHeight);
        }
        else
        {
            /* Determine the resolve size. */
            if ((DestOrigin->x == 0) &&
                (RectSize->x >= DestSurface->info.rect.right))
            {
                /* Full width resolve, a special case. */
                rectSize.x = DestSurface->info.alignedWidth;
            }
            else
            {
                rectSize.x = RectSize->x;
            }

            if ((DestOrigin->y == 0) &&
                (RectSize->y >= DestSurface->info.rect.bottom))
            {
                /* Full height resolve, a special case. */
                rectSize.y = DestSurface->info.alignedHeight;
            }
            else
            {
                rectSize.y = RectSize->y;
            }

            rectSize.x = gcmMIN(srcMaxWidth,  rectSize.x);
            rectSize.y = gcmMIN(srcMaxHeight, rectSize.y);

            rectSize.x = gcmMIN(dstMaxWidth,  rectSize.x);
            rectSize.y = gcmMIN(dstMaxHeight, rectSize.y);

        }

        if (DestSurface->info.hzNode.valid)
        {
            /* Disable any HZ attached to destination. */
            DestSurface->info.hzDisabled = gcvTRUE;
        }



        /*
        ** 1, gcoHARDWARE_ResolveRect can't handle multi-layer source/dest.
        ** 2, Fake format, except padding channel ones.
        ** 3, gcoHARDWARE_ResolveRect can't handle non unsigned normalized source/dest
        ** For those cases, just fall back to generic copy pixels path.
        */
        if (((SrcSurface->info.formatInfo.layers > 1)                                            ||
             (DestSurface->info.formatInfo.layers > 1)                                           ||
             (SrcSurface->info.formatInfo.fakedFormat &&
             /* Faked format, but not paddingFormat with default value padded, go SW */
              !(SrcSurface->info.paddingFormat && !SrcSurface->info.garbagePadded)
             )                                                                                   ||
             (DestSurface->info.formatInfo.fakedFormat && !DestSurface->info.paddingFormat)      ||
             (SrcSurface->info.formatInfo.fmtDataType != gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED) ||
             (DestSurface->info.formatInfo.fmtDataType != gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED)
            ) &&
            ((SrcSurface->info.format != gcvSURF_S8) || (DestSurface->info.format != gcvSURF_S8))
           )
        {

            gcmERR_BREAK(gcoSURF_CopyPixels(SrcSurface, DestSurface,
                               SrcOrigin->x, SrcOrigin->y,
                               DestOrigin->x, DestOrigin->y,
                               rectSize.x,(yInverted ? -rectSize.y : rectSize.y)));
            break;
        }

        /* Special case a resolve from the depth buffer with tile status. */
        if ((SrcSurface->info.type == gcvSURF_DEPTH)
        &&  (SrcSurface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
        )
        {
            /* Resolve a depth buffer. */
            gcmERR_BREAK(
                gcoHARDWARE_ResolveDepth(gcvNULL,
                                         &SrcSurface->info,
                                         &DestSurface->info,
                                         SrcOrigin,
                                         DestOrigin,
                                         &rectSize,
                                         yInverted));
        }
        else
        {
#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
            /*
             * If flip bitmap is used as RS source, we need to calculate the
             * real pixel address before hw operation.
             */
            gctUINT32 physical;
            gctUINT8_PTR logical;

            /* Save original addresses. */
            physical = SrcSurface->info.node.physical;
            logical  = SrcSurface->info.node.logical;

            /* Calculate flip bitmap pixel addresses. */
            SrcSurface->info.node.physical += SrcSurface->info.flipBitmapOffset;
            SrcSurface->info.node.logical  += SrcSurface->info.flipBitmapOffset;
#endif

            /* Perform resolve. */
            gcmERR_BREAK(
                gcoHARDWARE_ResolveRect(gcvNULL,
                                        &SrcSurface->info,
                                        &DestSurface->info,
                                        SrcOrigin,
                                        DestOrigin,
                                        &rectSize,
                                        yInverted));

#if defined(ANDROID) && gcdANDROID_UNALIGNED_LINEAR_COMPOSITION_ADJUST
            /* Restore addresses. */
            SrcSurface->info.node.physical = physical;
            SrcSurface->info.node.logical  = logical;
#endif
        }

        /* If dst surface was fully overwritten, reset the deferDither3D flag. */
        if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PE_DITHER_FIX) == gcvFALSE &&
            DestOrigin->x == 0 && RectSize->x >= DestSurface->info.rect.right  / DestSurface->info.samples.x &&
            DestOrigin->y == 0 && RectSize->y >= DestSurface->info.rect.bottom / DestSurface->info.samples.y)
        {
            DestSurface->info.deferDither3D = gcvFALSE;
        }
    }
    while (gcvFALSE);

    /* Unlock the surfaces. */
    if (target[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, target[0]));
    }

    if (source[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface, source[0]));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}



static gceSTATUS
_3DBlitClearRect(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs,
    IN gctUINT32 LayerIndex
    )
{
    gceSTATUS status;
    gcsPOINT origin, rectSize;
    gcs3DBLIT_INFO clearInfo = {0};
    gcs3DBLIT_INFO hzClearInfo = {0};
    gctBOOL fastClear = gcvFALSE;
    gctBOOL supportFastClear = gcvTRUE, hzSupportFastClear = gcvTRUE;
    gcsRECT_PTR rect = ClearArgs->clearRect;
    gcsSAMPLES originSamples;
    gctBOOL clearHZ = ((ClearArgs->flags & gcvCLEAR_DEPTH) && Surface->info.hzNode.pool != gcvPOOL_UNKNOWN);

    origin.x = ClearArgs->clearRect->left;
    origin.y = ClearArgs->clearRect->top;
    rectSize.x = ClearArgs->clearRect->right - ClearArgs->clearRect->left;
    rectSize.y = ClearArgs->clearRect->bottom - ClearArgs->clearRect->top;

    if ((ClearArgs->flags & gcvCLEAR_STENCIL) && Surface->info.hasStencilComponent)
    {
        Surface->info.canDropStencilPlane = gcvFALSE;
    }

    /* Test for entire surface clear. */
    if ((rect->left == 0)  &&  (rect->top == 0)
    &&  (rect->right >= (Surface->info.rect.right / Surface->info.samples.x))
    &&  (rect->bottom >= (Surface->info.rect.bottom / Surface->info.samples.x))
    )
    {
        fastClear = gcvTRUE;
    }

    /* Compute clear values. */
    gcmONERROR(_ComputeClear(&Surface->info, ClearArgs, LayerIndex));

    /* Prepare clearInfo. */
    clearInfo.clearValue[0] = Surface->info.clearValue[LayerIndex];
    clearInfo.clearValue[1] = Surface->info.clearValueUpper[LayerIndex];
    clearInfo.clearMask = Surface->info.clearBitMask[LayerIndex];
    clearInfo.destAddress = Surface->info.node.physical + Surface->info.offset;
    clearInfo.destTileStatusAddress = Surface->info.tileStatusNode.physical;
    clearInfo.origin = &origin;
    clearInfo.rect = &rectSize;
    if (clearInfo.clearMask == 0)
    {
        return gcvSTATUS_OK;
    }

    if (clearHZ)
    {
        /* Prepare hzClearInfo. */
        hzClearInfo.clearValue[0] = Surface->info.clearValueHz;
        hzClearInfo.clearValue[1] = Surface->info.clearValueHz;
        hzClearInfo.clearMask = _ByteMaskToBitMask(0xF);
        hzClearInfo.destAddress = Surface->info.hzNode.physical;
        hzClearInfo.destTileStatusAddress = Surface->info.hzTileStatusNode.physical;
        hzClearInfo.origin = &origin;
        hzClearInfo.rect = &rectSize;
    }

        /* Check limitation of 3DBLIT to determine to use FC, HW Clear or SW clear. */
        gcmONERROR(
            gcoHARDWARE_3DBlitClearQuery(gcvNULL, &Surface->info, &clearInfo, &supportFastClear));

        fastClear &= supportFastClear;

        if (clearHZ)
        {
            gcmONERROR(
                gcoHARDWARE_3DBlitClearQuery(gcvNULL, &Surface->info, &clearInfo, &hzSupportFastClear));

            fastClear &= hzSupportFastClear;
        }

    gcoHARDWARE_SelectPipe(gcvNULL, gcvPIPE_3D, gcvNULL);

    originSamples = Surface->info.samples;
    Surface->info.samples.x = 1;
    Surface->info.samples.y = 1;

    /* Flush the tile status cache. */
    gcmONERROR(
        gcoHARDWARE_FlushTileStatus(gcvNULL, &Surface->info, gcvFALSE));

    Surface->info.samples = originSamples;

    gcmONERROR(
        gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));

    if (!fastClear)
    {
         /* Flush the tile status and decompress the buffers. */
        gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, &Surface->info, gcvTRUE));

        clearInfo.destTileStatusAddress = 0;

        if (clearHZ)
        {
            hzClearInfo.destTileStatusAddress = 0;
        }
    }

    /* Clear. */
    gcmONERROR(
        gcoHARDWARE_3DBlitClear(gcvNULL, &Surface->info, &clearInfo, &origin, &rectSize));

    if (clearHZ)
    {
        /* Clear HZ. */
        gcmONERROR(
            gcoHARDWARE_3DBlitClear(gcvNULL, &Surface->info, &hzClearInfo, &origin, &rectSize));
    }

    if (fastClear)
    {
        /* Record FC value. */
        Surface->info.fcValue = clearInfo.clearValue[0];
        Surface->info.fcValueUpper = clearInfo.clearValue[1];

        if (clearHZ)
        {
            /* Record HZ FC value. */
            Surface->info.fcValueHz = hzClearInfo.clearValue[0];
        }

        /* Turn the tile status on again. */
        Surface->info.tileStatusDisabled = gcvFALSE;

        /* Reset the tile status. */
        gcmONERROR(gcoSURF_EnableTileStatus(Surface));
    }

    return gcvSTATUS_OK;

OnError:
    return status;
}

gceSTATUS
gcoSURF_3DBlitClearRect(
    IN gcoSURF Surface,
    IN gcsSURF_CLEAR_ARGS_PTR ClearArgs
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsSURF_CLEAR_ARGS clearArgs;
    gctUINT originalOffset;
    gctUINT LayerIndex;
    gcsRECT fullRect;

    gcmHEADER_ARG("Surface=0x%x ClearArgs=0x%x", Surface, ClearArgs);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ClearArgs != gcvNULL);


    gcoOS_MemCopy(&clearArgs, ClearArgs, sizeof(gcsSURF_CLEAR_ARGS));

    if (gcvNULL == clearArgs.clearRect)
    {
        fullRect.left = 0;
        fullRect.top  = 0;
        fullRect.right = Surface->info.alignedWidth;
        fullRect.bottom = Surface->info.alignedHeight;
        clearArgs.clearRect = &fullRect;
    }

    originalOffset = Surface->info.offset;
    for (LayerIndex = 0; LayerIndex < Surface->info.formatInfo.layers; LayerIndex++)
    {
        gcmONERROR(gcoSURF_SetOffset(Surface, clearArgs.offset + Surface->info.layerSize * LayerIndex));

        gcmONERROR(_3DBlitClearRect(Surface, &clearArgs, LayerIndex));
    }
    gcmONERROR(gcoSURF_SetOffset(Surface, originalOffset));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_3DBlitBltRect(
    IN gcoSURF SrcSurf,
    IN gcoSURF DestSurf,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("SrcSurf=0x%x DestSurf=0x%x", SrcSurf, DestSurf);

    gcmONERROR(gcoHARDWARE_FlushTileStatus(gcvNULL, &SrcSurf->info, gcvFALSE));

    if (!DestSurf->info.tileStatusDisabled)
    {
        gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, &DestSurf->info, gcvTRUE));
    }

    gcmONERROR(gcoHARDWARE_3DBlitBlt(gcvNULL, &SrcSurf->info, &DestSurf->info, SrcOrigin, DestOrigin, RectSize));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_3DBlitCopy(
    IN gctUINT32 SrcAddress,
    IN gctUINT32 DestAddress,
    IN gctUINT32 Bytes
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("SrcAddress=0x%x DestAddress=0x%x Bytes=0x%x",
                  SrcAddress, DestAddress, Bytes);

    gcmONERROR(gcoHARDWARE_3DBlitCopy(gcvNULL, SrcAddress, DestAddress, Bytes));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}


#endif /* gcdENABLE_3D */

#if gcdENABLE_2D
/*******************************************************************************
**
**  gcoSURF_SetClipping
**
**  Set clipping rectangle to the size of the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetClipping(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        gco2D engine;
        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));
        gcmERR_BREAK(gco2D_SetClipping(engine, &Surface->info.rect));
    }
    while (gcvFALSE);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Clear2D
**
**  Clear one or more rectangular areas.
**
**  INPUT:
**
**      gcoSURF DestSurface
**          Pointer to the destination surface.
**
**      gctUINT32 RectCount
**          The number of rectangles to draw. The array of rectangles
**          pointed to by Rect parameter must have at least RectCount items.
**          Note, that for masked source blits only one destination rectangle
**          is supported.
**
**      gcsRECT_PTR DestRect
**          Pointer to a list of destination rectangles.
**
**      gctUINT32 LoColor
**          Low 32-bit clear color values.
**
**      gctUINT32 HiColor
**          high 32-bit clear color values.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Clear2D(
    IN gcoSURF DestSurface,
    IN gctUINT32 RectCount,
    IN gcsRECT_PTR DestRect,
    IN gctUINT32 LoColor,
    IN gctUINT32 HiColor
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;
    gctPOINTER destMemory[3] = {gcvNULL};
    gco2D engine;

    gcmHEADER_ARG("DestSurface=0x%x RectCount=%u DestRect=0x%x LoColor=%u HiColor=%u",
              DestSurface, RectCount, DestRect, LoColor, HiColor);

    do
    {
        /* Validate the object. */
        gcmBADOBJECT_BREAK(DestSurface, gcvOBJ_SURF);

        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        /* Use surface rect if not specified. */
        if (DestRect == gcvNULL)
        {
            if (RectCount != 1)
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            DestRect = &DestSurface->info.rect;
        }

        /* Lock the destination. */
        gcmERR_BREAK(gcoSURF_Lock(
            DestSurface,
            gcvNULL,
            destMemory
            ));

        /* Program the destination. */
        gcmERR_BREAK(gco2D_SetTargetEx(
            engine,
            DestSurface->info.node.physical,
            DestSurface->info.stride,
            DestSurface->info.rotation,
            DestSurface->info.alignedWidth,
            DestSurface->info.alignedHeight
            ));

        gcmERR_BREAK(gco2D_SetTransparencyAdvanced(
            engine,
            gcv2D_OPAQUE,
            gcv2D_OPAQUE,
            gcv2D_OPAQUE
            ));

        /* Form a CLEAR command. */
        gcmERR_BREAK(gco2D_Clear(
            engine,
            RectCount,
            DestRect,
            LoColor,
            gcvFALSE,
            0xCC,
            0xCC
            ));
    }
    while (gcvFALSE);

    /* Unlock the destination. */
    if (destMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destMemory[0]));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Line
**
**  Draw one or more Bresenham lines.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to an gcoSURF object.
**
**      gctUINT32 LineCount
**          The number of lines to draw. The array of line positions pointed
**          to by Position parameter must have at least LineCount items.
**
**      gcsRECT_PTR Position
**          Points to an array of positions in (x0, y0)-(x1, y1) format.
**
**      gcoBRUSH Brush
**          Brush to use for drawing.
**
**      gctUINT8 FgRop
**          Foreground ROP to use with opaque pixels.
**
**      gctUINT8 BgRop
**          Background ROP to use with transparent pixels.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Line(
    IN gcoSURF DestSurface,
    IN gctUINT32 LineCount,
    IN gcsRECT_PTR Position,
    IN gcoBRUSH Brush,
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop
    )
{
    gceSTATUS status;
    gctPOINTER destMemory[3] = {gcvNULL};
    gco2D engine;

    gcmHEADER_ARG("DestSurface=0x%x LineCount=%u Position=0x%x Brush=0x%x FgRop=%02x "
              "BgRop=%02x",
              DestSurface, LineCount, Position, Brush, FgRop, BgRop);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(DestSurface, gcvOBJ_SURF);

    do
    {
        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        /* Lock the destination. */
        gcmERR_BREAK(gcoSURF_Lock(
            DestSurface,
            gcvNULL,
            destMemory
            ));

        /* Program the destination. */
        gcmERR_BREAK(gco2D_SetTargetEx(
            engine,
            DestSurface->info.node.physical,
            DestSurface->info.stride,
            DestSurface->info.rotation,
            DestSurface->info.alignedWidth,
            DestSurface->info.alignedHeight
            ));

        gcmERR_BREAK(gco2D_SetTransparencyAdvanced(
            engine,
            gcv2D_OPAQUE,
            gcv2D_OPAQUE,
            gcv2D_OPAQUE
            ));

        /* Draw a LINE command. */
        gcmERR_BREAK(gco2D_Line(
            engine,
            LineCount,
            Position,
            Brush,
            FgRop,
            BgRop,
            DestSurface->info.format
            ));
    }
    while (gcvFALSE);

    /* Unlock the destination. */
    if (destMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destMemory[0]));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Blit
**
**  Generic rectangular blit.
**
**  INPUT:
**
**      OPTIONAL gcoSURF SrcSurface
**          Pointer to the source surface.
**
**      gcoSURF DestSurface
**          Pointer to the destination surface.
**
**      gctUINT32 RectCount
**          The number of rectangles to draw. The array of rectangles
**          pointed to by Rect parameter must have at least RectCount items.
**          Note, that for masked source blits only one destination rectangle
**          is supported.
**
**      OPTIONAL gcsRECT_PTR SrcRect
**          If RectCount is 1, SrcRect represents an absolute rectangle within
**          the source surface.
**          If RectCount is greater then 1, (right,bottom) members of SrcRect
**          are ignored and (left,top) members are used as the offset from
**          the origin of each destination rectangle in DestRect list to
**          determine the corresponding source rectangle. In this case the width
**          and the height of the source are assumed the same as of the
**          corresponding destination rectangle.
**
**      gcsRECT_PTR DestRect
**          Pointer to a list of destination rectangles.
**
**      OPTIONAL gcoBRUSH Brush
**          Brush to use for drawing.
**
**      gctUINT8 FgRop
**          Foreground ROP to use with opaque pixels.
**
**      gctUINT8 BgRop
**          Background ROP to use with transparent pixels.
**
**      OPTIONAL gceSURF_TRANSPARENCY Transparency
**          gcvSURF_OPAQUE - each pixel of the bitmap overwrites the destination.
**          gcvSURF_SOURCE_MATCH - source pixels compared against register value
**              to determine the transparency. In simple terms, the transparency
**              comes down to selecting the ROP code to use. Opaque pixels use
**              foreground ROP and transparent ones use background ROP.
**          gcvSURF_SOURCE_MASK - monochrome source mask defines transparency.
**          gcvSURF_PATTERN_MASK - pattern mask defines transparency.
**
**      OPTIONAL gctUINT32 TransparencyColor
**          This value is used in gcvSURF_SOURCE_MATCH transparency mode.
**          The value is compared against each pixel to determine transparency
**          of the pixel. If the values found equal, the pixel is transparent;
**          otherwise it is opaque.
**
**      OPTIONAL gctPOINTER Mask
**          A pointer to monochrome mask for masked source blits.
**
**      OPTIONAL gceSURF_MONOPACK MaskPack
**          Determines how many horizontal pixels are there per each 32-bit
**          chunk of monochrome mask. For example, if set to gcvSURF_PACKED8,
**          each 32-bit chunk is 8-pixel wide, which also means that it defines
**          4 vertical lines of pixel mask.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Blit(
    IN OPTIONAL gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gctUINT32 RectCount,
    IN OPTIONAL gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN OPTIONAL gcoBRUSH Brush,
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN OPTIONAL gceSURF_TRANSPARENCY Transparency,
    IN OPTIONAL gctUINT32 TransparencyColor,
    IN OPTIONAL gctPOINTER Mask,
    IN OPTIONAL gceSURF_MONOPACK MaskPack
    )
{
    gceSTATUS status;

    /*gcoHARDWARE hardware;*/
    gco2D engine;

    gce2D_TRANSPARENCY srcTransparency;
    gce2D_TRANSPARENCY dstTransparency;
    gce2D_TRANSPARENCY patTransparency;

    gctBOOL useBrush;
    gctBOOL useSource;

    gctBOOL stretchBlt = gcvFALSE;
    gctBOOL relativeSource = gcvFALSE;

    gctPOINTER srcMemory[3]  = {gcvNULL};
    gctPOINTER destMemory[3] = {gcvNULL};

    gctBOOL useSoftEngine = gcvFALSE;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x RectCount=%u SrcRect=0x%x "
              "DestRect=0x%x Brush=0x%x FgRop=%02x BgRop=%02x Transparency=%d "
              "TransparencyColor=%08x Mask=0x%x MaskPack=%d",
              SrcSurface, DestSurface, RectCount, SrcRect, DestRect, Brush,
              FgRop, BgRop, Transparency, TransparencyColor, Mask, MaskPack);

    do
    {
        gctUINT32 destFormat, destFormatSwizzle, destIsYUV;

        /* Validate the object. */
        gcmBADOBJECT_BREAK(DestSurface, gcvOBJ_SURF);

        if (Mask != gcvNULL && gcoHAL_IsFeatureAvailable(gcvNULL, gcvFEATURE_ANDROID_ONLY) == gcvTRUE)
        {
            gcmFOOTER_ARG("status=%d", gcvSTATUS_NOT_SUPPORTED);
            return gcvSTATUS_NOT_SUPPORTED;
        }

        /* Is 2D Hardware available? */
        if (!gcoHARDWARE_Is2DAvailable(gcvNULL))
        {
            /* No, use software renderer. */
            gcmERR_BREAK(gcoHARDWARE_UseSoftware2D(gcvNULL, gcvTRUE));
            useSoftEngine = gcvTRUE;
        }

        /* Is the destination format supported? */
        if (gcmIS_ERROR(gcoHARDWARE_TranslateDestinationFormat(
                gcvNULL, DestSurface->info.format, gcvTRUE,
                &destFormat, &destFormatSwizzle, &destIsYUV)))
        {
            /* No, use software renderer. */
            gcmERR_BREAK(gcoHARDWARE_UseSoftware2D(gcvNULL, gcvTRUE));
            useSoftEngine = gcvTRUE;
        }

        /* Translate the specified transparency mode. */
        gcmERR_BREAK(gcoHARDWARE_TranslateSurfTransparency(
            Transparency,
            &srcTransparency,
            &dstTransparency,
            &patTransparency
            ));

        /* Determine the resource usage. */
        gcoHARDWARE_Get2DResourceUsage(
            FgRop, BgRop,
            srcTransparency,
            &useSource, &useBrush, gcvNULL
            );

        /* Use surface rect if not specified. */
        if (DestRect == gcvNULL)
        {
            if (RectCount != 1)
            {
                status = gcvSTATUS_INVALID_ARGUMENT;
                break;
            }

            DestRect = &DestSurface->info.rect;
        }

        /* Get 2D engine. */
        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        /* Setup the brush if needed. */
        if (useBrush)
        {
            /* Flush the brush. */
            gcmERR_BREAK(gco2D_FlushBrush(
                engine,
                Brush,
                DestSurface->info.format
                ));
        }

        /* Setup the source if needed. */
        if (useSource)
        {
            /* Validate the object. */
            gcmBADOBJECT_BREAK(SrcSurface, gcvOBJ_SURF);

            /* Use surface rect if not specified. */
            if (SrcRect == gcvNULL)
            {
                SrcRect = &SrcSurface->info.rect;
            }

            /* Lock the source. */
            gcmERR_BREAK(gcoSURF_Lock(
                SrcSurface,
                gcvNULL,
                srcMemory
                ));

            /* Determine the relative flag. */
            relativeSource = (RectCount > 1) ? gcvTRUE : gcvFALSE;

            /* Program the source. */
            if (Mask == gcvNULL)
            {
                gctBOOL equal;

                /* Check whether this should be a stretch/shrink blit. */
                if ( (gcsRECT_IsOfEqualSize(SrcRect, DestRect, &equal) ==
                          gcvSTATUS_OK) &&
                     !equal )
                {
                    /* Calculate the stretch factors. */
                    gcmERR_BREAK(gco2D_SetStretchRectFactors(
                        engine,
                        SrcRect, DestRect
                        ));

                    /* Mark as stretch blit. */
                    stretchBlt = gcvTRUE;
                }

                gcmERR_BREAK(gco2D_SetColorSourceEx(
                    engine,
                    useSoftEngine ?
                        (gctUINT32)(gctUINTPTR_T)SrcSurface->info.node.logical
                        : SrcSurface->info.node.physical,
                    SrcSurface->info.stride,
                    SrcSurface->info.format,
                    SrcSurface->info.rotation,
                    SrcSurface->info.alignedWidth,
                    SrcSurface->info.alignedHeight,
                    relativeSource,
                    Transparency,
                    TransparencyColor
                    ));

                gcmERR_BREAK(gco2D_SetSource(
                    engine,
                    SrcRect
                    ));
            }
        }

        /* Lock the destination. */
        gcmERR_BREAK(gcoSURF_Lock(
            DestSurface,
            gcvNULL,
            destMemory
            ));

        gcmERR_BREAK(gco2D_SetTargetEx(
            engine,
            useSoftEngine ?
                (gctUINT32)(gctUINTPTR_T)DestSurface->info.node.logical
                : DestSurface->info.node.physical,
            DestSurface->info.stride,
            DestSurface->info.rotation,
            DestSurface->info.alignedWidth,
            DestSurface->info.alignedHeight
            ));

        /* Masked sources need to be handled differently. */
        if (useSource && (Mask != gcvNULL))
        {
            gctUINT32 streamPackHeightMask;
            gcsSURF_FORMAT_INFO_PTR srcFormat[2];
            gctUINT32 srcAlignedLeft, srcAlignedTop;
            gctINT32 tileWidth, tileHeight;
            gctUINT32 tileHeightMask;
            gctUINT32 maxHeight;
            gctUINT32 srcBaseAddress;
            gcsRECT srcSubRect;
            gcsRECT destSubRect;
            gcsRECT maskRect;
            gcsPOINT maskSize;
            gctUINT32 lines2render;
            gctUINT32 streamWidth;
            gceSURF_MONOPACK streamPack;

            /* Compute the destination size. */
            gctUINT32 destWidth  = DestRect->right  - DestRect->left;
            gctUINT32 destHeight = DestRect->bottom - DestRect->top;

            /* Query tile size. */
            gcmASSERT(SrcSurface->info.type == gcvSURF_BITMAP);
            gcoHARDWARE_QueryTileSize(
                &tileWidth, &tileHeight,
                gcvNULL, gcvNULL,
                gcvNULL
                );

            tileHeightMask = tileHeight - 1;

            /* Determine left source coordinate. */
            srcSubRect.left = SrcRect->left & 7;

            /* Assume 8-pixel packed stream. */
            streamWidth = gcmALIGN(srcSubRect.left + destWidth, 8);

            /* Do we fit? */
            if (streamWidth == 8)
            {
                streamPack = gcvSURF_PACKED8;
                streamPackHeightMask = ~3U;
            }

            /* Nope, don't fit. */
            else
            {
                /* Assume 16-pixel packed stream. */
                streamWidth = gcmALIGN(srcSubRect.left + destWidth, 16);

                /* Do we fit now? */
                if (streamWidth == 16)
                {
                    streamPack = gcvSURF_PACKED16;
                    streamPackHeightMask = ~1U;
                }

                /* Still don't. */
                else
                {
                    /* Assume unpacked stream. */
                    streamWidth = gcmALIGN(srcSubRect.left + destWidth, 32);
                    streamPack = gcvSURF_UNPACKED;
                    streamPackHeightMask = ~0U;
                }
            }

            /* Determine the maximum stream height. */
            maxHeight  = gcoMATH_DivideUInt(gco2D_GetMaximumDataCount() << 5,
                                            streamWidth);
            maxHeight &= streamPackHeightMask;

            /* Determine the sub source rectangle. */
            srcSubRect.top    = SrcRect->top & tileHeightMask;
            srcSubRect.right  = srcSubRect.left + destWidth;
            srcSubRect.bottom = srcSubRect.top;

            /* Init destination subrectangle. */
            destSubRect.left   = DestRect->left;
            destSubRect.top    = DestRect->top;
            destSubRect.right  = DestRect->right;
            destSubRect.bottom = destSubRect.top;

            /* Determine the number of lines to render. */
            lines2render = srcSubRect.top + destHeight;

            /* Determine the aligned source coordinates. */
            srcAlignedLeft = SrcRect->left - srcSubRect.left;
            srcAlignedTop  = SrcRect->top  - srcSubRect.top;
            gcmASSERT((srcAlignedLeft % tileWidth) == 0);

            /* Get format characteristics. */
            gcmERR_BREAK(gcoSURF_QueryFormat(SrcSurface->info.format, srcFormat));

            /* Determine the initial source address. */
            srcBaseAddress
                = (useSoftEngine ?
                        (gctUINT32)(gctUINTPTR_T)SrcSurface->info.node.logical
                        : SrcSurface->info.node.physical)
                +   srcAlignedTop  * SrcSurface->info.stride
                + ((srcAlignedLeft * srcFormat[0]->bitsPerPixel) >> 3);

            /* Set initial mask coordinates. */
            maskRect.left   = srcAlignedLeft;
            maskRect.top    = srcAlignedTop;
            maskRect.right  = maskRect.left + streamWidth;
            maskRect.bottom = maskRect.top;

            /* Set mask size. */
            maskSize.x = SrcSurface->info.rect.right;
            maskSize.y = SrcSurface->info.rect.bottom;

            do
            {
                /* Determine the area to render in this pass. */
                srcSubRect.top = srcSubRect.bottom & tileHeightMask;
                srcSubRect.bottom = srcSubRect.top + lines2render;
                if (srcSubRect.bottom > (gctINT32) maxHeight)
                    srcSubRect.bottom = maxHeight & ~tileHeightMask;

                destSubRect.top = destSubRect.bottom;
                destSubRect.bottom
                    = destSubRect.top
                    + (srcSubRect.bottom - srcSubRect.top);

                maskRect.top = maskRect.bottom;
                maskRect.bottom = maskRect.top + srcSubRect.bottom;

                /* Set source rectangle size. */
                gcmERR_BREAK(gco2D_SetSource(
                    engine,
                    &srcSubRect
                    ));

                /* Configure masked source. */
                gcmERR_BREAK(gco2D_SetMaskedSource(
                    engine,
                    srcBaseAddress,
                    SrcSurface->info.stride,
                    SrcSurface->info.format,
                    relativeSource,
                    streamPack
                    ));

                /* Do the blit. */
                gcmERR_BREAK(gco2D_MonoBlit(
                    engine,
                    Mask,
                    &maskSize,
                    &maskRect,
                    MaskPack,
                    streamPack,
                    &destSubRect,
                    FgRop,
                    BgRop,
                    DestSurface->info.format
                    ));

                /* Update the source address. */
                srcBaseAddress += srcSubRect.bottom * SrcSurface->info.stride;

                /* Update the line counter. */
                lines2render -= srcSubRect.bottom;
            }
            while (lines2render);
        }
        else if (stretchBlt)
        {
            gcmERR_BREAK(gco2D_StretchBlit(
                engine,
                RectCount,
                DestRect,
                FgRop,
                BgRop,
                DestSurface->info.format
                ));
        }
        else
        {
            gcmERR_BREAK(gco2D_Blit(
                engine,
                RectCount,
                DestRect,
                FgRop,
                BgRop,
                DestSurface->info.format
                ));
        }
    }
    while (gcvFALSE);

    /* Unlock the source. */
    if (srcMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface, srcMemory[0]));
    }

    /* Unlock the destination. */
    if (destMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destMemory[0]));
    }

    /*gcmGETHARDWARE(hardware);*/

    if (useSoftEngine)
    {
        /* Disable software renderer. */
        gcmVERIFY_OK(gcoHARDWARE_UseSoftware2D(gcvNULL, gcvFALSE));
    }

/*OnError:*/
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_MonoBlit
**
**  Monochrome blit.
**
**  INPUT:
**
**      gcoSURF DestSurface
**          Pointer to the destination surface.
**
**      gctPOINTER Source
**          A pointer to the monochrome bitmap.
**
**      gceSURF_MONOPACK SourcePack
**          Determines how many horizontal pixels are there per each 32-bit
**          chunk of monochrome bitmap. For example, if set to gcvSURF_PACKED8,
**          each 32-bit chunk is 8-pixel wide, which also means that it defines
**          4 vertical lines of pixels.
**
**      gcsPOINT_PTR SourceSize
**          Size of the source monochrome bitmap in pixels.
**
**      gcsPOINT_PTR SourceOrigin
**          Top left coordinate of the source within the bitmap.
**
**      gcsRECT_PTR DestRect
**          Pointer to a list of destination rectangles.
**
**      OPTIONAL gcoBRUSH Brush
**          Brush to use for drawing.
**
**      gctUINT8 FgRop
**          Foreground ROP to use with opaque pixels.
**
**      gctUINT8 BgRop
**          Background ROP to use with transparent pixels.
**
**      gctBOOL ColorConvert
**          The values of FgColor and BgColor parameters are stored directly in
**          internal color registers and are used either directly as the source
**          color or converted to the format of destination before actually
**          used. The later happens if ColorConvert is not zero.
**
**      gctUINT8 MonoTransparency
**          This value is used in gcvSURF_SOURCE_MATCH transparency mode.
**          The value can be either 0 or 1 and is compared against each mono
**          pixel to determine transparency of the pixel. If the values found
**          equal, the pixel is transparent; otherwise it is opaque.
**
**      gceSURF_TRANSPARENCY Transparency
**          gcvSURF_OPAQUE - each pixel of the bitmap overwrites the destination.
**          gcvSURF_SOURCE_MATCH - source pixels compared against register value
**              to determine the transparency. In simple terms, the transparency
**              comes down to selecting the ROP code to use. Opaque pixels use
**              foreground ROP and transparent ones use background ROP.
**          gcvSURF_SOURCE_MASK - monochrome source mask defines transparency.
**          gcvSURF_PATTERN_MASK - pattern mask defines transparency.
**
**      gctUINT32 FgColor
**          The values are used to represent foreground color
**          of the source. If the values are in destination format, set
**          ColorConvert to 0. Otherwise, provide the values in ARGB8 format
**          and set ColorConvert to 1 to instruct the hardware to convert the
**          values to the destination format before they are actually used.
**
**      gctUINT32 BgColor
**          The values are used to represent background color
**          of the source. If the values are in destination format, set
**          ColorConvert to 0. Otherwise, provide the values in ARGB8 format
**          and set ColorConvert to 1 to instruct the hardware to convert the
**          values to the destination format before they are actually used.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_MonoBlit(
    IN gcoSURF DestSurface,
    IN gctPOINTER Source,
    IN gceSURF_MONOPACK SourcePack,
    IN gcsPOINT_PTR SourceSize,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsRECT_PTR DestRect,
    IN OPTIONAL gcoBRUSH Brush,
    IN gctUINT8 FgRop,
    IN gctUINT8 BgRop,
    IN gctBOOL ColorConvert,
    IN gctUINT8 MonoTransparency,
    IN gceSURF_TRANSPARENCY Transparency,
    IN gctUINT32 FgColor,
    IN gctUINT32 BgColor
    )
{
    gceSTATUS status;

    gco2D engine;

    gce2D_TRANSPARENCY srcTransparency;
    gce2D_TRANSPARENCY dstTransparency;
    gce2D_TRANSPARENCY patTransparency;

    gctBOOL useBrush;
    gctBOOL useSource;

    gctUINT32 destWidth;
    gctUINT32 destHeight;

    gctUINT32 maxHeight;
    gctUINT32 streamPackHeightMask;
    gcsPOINT sourceOrigin;
    gcsRECT srcSubRect;
    gcsRECT destSubRect;
    gcsRECT streamRect;
    gctUINT32 lines2render;
    gctUINT32 streamWidth;
    gceSURF_MONOPACK streamPack;

    gctPOINTER destMemory[3] = {gcvNULL};

    gctBOOL useSotfEngine = gcvFALSE;

    gcmHEADER_ARG("DestSurface=0x%x Source=0x%x SourceSize=0x%x SourceOrigin=0x%x "
              "DestRect=0x%x Brush=0x%x FgRop=%02x BgRop=%02x ColorConvert=%d "
              "MonoTransparency=%u Transparency=%d FgColor=%08x BgColor=%08x",
              DestSurface, Source, SourceSize, SourceOrigin, DestRect, Brush,
              FgRop, BgRop, ColorConvert, MonoTransparency, Transparency,
              FgColor, BgColor);

    do
    {
        gctUINT32 destFormat, destFormatSwizzle, destIsYUV;

        /* Validate the object. */
        gcmBADOBJECT_BREAK(DestSurface, gcvOBJ_SURF);

        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        /* Is the destination format supported? */
        if (gcmIS_ERROR(gcoHARDWARE_TranslateDestinationFormat(
                gcvNULL, DestSurface->info.format, gcvTRUE,
                &destFormat, &destFormatSwizzle, &destIsYUV)))
        {
            /* No, use software renderer. */
            gcmERR_BREAK(gcoHARDWARE_UseSoftware2D(gcvNULL, gcvTRUE));
            useSotfEngine = gcvTRUE;
        }

        /* Translate the specified transparency mode. */
        gcmERR_BREAK(gcoHARDWARE_TranslateSurfTransparency(
            Transparency,
            &srcTransparency,
            &dstTransparency,
            &patTransparency
            ));

        /* Determine the resource usage. */
        gcoHARDWARE_Get2DResourceUsage(
            FgRop, BgRop,
            srcTransparency,
            &useSource, &useBrush, gcvNULL
            );

        /* Source must be used. */
        if (!useSource)
        {
            status = gcvSTATUS_INVALID_ARGUMENT;
            break;
        }

        /* Use surface rect if not specified. */
        if (DestRect == gcvNULL)
        {
            DestRect = &DestSurface->info.rect;
        }

        /* Default to 0 origin. */
        if (SourceOrigin == gcvNULL)
        {
            SourceOrigin = &sourceOrigin;
            SourceOrigin->x = 0;
            SourceOrigin->y = 0;
        }

        /* Lock the destination. */
        gcmERR_BREAK(gcoSURF_Lock(
            DestSurface,
            gcvNULL,
            destMemory
            ));

        gcmERR_BREAK(gco2D_SetTargetEx(
            engine,
            useSotfEngine ?
                (gctUINT32)(gctUINTPTR_T)DestSurface->info.node.logical
                : DestSurface->info.node.physical,
            DestSurface->info.stride,
            DestSurface->info.rotation,
            DestSurface->info.alignedWidth,
            DestSurface->info.alignedHeight
            ));

        /* Setup the brush if needed. */
        if (useBrush)
        {
            /* Flush the brush. */
            gcmERR_BREAK(gco2D_FlushBrush(
                engine,
                Brush,
                DestSurface->info.format
                ));
        }

        /* Compute the destination size. */
        destWidth  = DestRect->right  - DestRect->left;
        destHeight = DestRect->bottom - DestRect->top;

        /* Determine the number of lines to render. */
        lines2render = destHeight;

        /* Determine left source coordinate. */
        srcSubRect.left = SourceOrigin->x & 7;

        /* Assume 8-pixel packed stream. */
        streamWidth = gcmALIGN(srcSubRect.left + destWidth, 8);

        /* Do we fit? */
        if (streamWidth == 8)
        {
            streamPack = gcvSURF_PACKED8;
            streamPackHeightMask = ~3U;
        }

        /* Nope, don't fit. */
        else
        {
            /* Assume 16-pixel packed stream. */
            streamWidth = gcmALIGN(srcSubRect.left + destWidth, 16);

            /* Do we fit now? */
            if (streamWidth == 16)
            {
                streamPack = gcvSURF_PACKED16;
                streamPackHeightMask = ~1U;
            }

            /* Still don't. */
            else
            {
                /* Assume unpacked stream. */
                streamWidth = gcmALIGN(srcSubRect.left + destWidth, 32);
                streamPack = gcvSURF_UNPACKED;
                streamPackHeightMask = ~0U;
            }
        }

        /* Set the rectangle value. */
        srcSubRect.top = srcSubRect.right = srcSubRect.bottom = 0;

        /* Set source rectangle size. */
        gcmERR_BREAK(gco2D_SetSource(
            engine,
            &srcSubRect
            ));

        /* Program the source. */
        gcmERR_BREAK(gco2D_SetMonochromeSource(
            engine,
            ColorConvert,
            MonoTransparency,
            streamPack,
            gcvFALSE,
            Transparency,
            FgColor,
            BgColor
            ));

        /* Determine the maxumum stream height. */
        maxHeight  = gcoMATH_DivideUInt(gco2D_GetMaximumDataCount() << 5,
                                        streamWidth);
        maxHeight &= streamPackHeightMask;

        /* Set the stream rectangle. */
        streamRect.left   = SourceOrigin->x - srcSubRect.left;
        streamRect.top    = SourceOrigin->y;
        streamRect.right  = streamRect.left + streamWidth;
        streamRect.bottom = streamRect.top;

        /* Init destination subrectangle. */
        destSubRect.left   = DestRect->left;
        destSubRect.top    = DestRect->top;
        destSubRect.right  = DestRect->right;
        destSubRect.bottom = destSubRect.top;

        do
        {
            /* Determine the area to render in this pass. */
            gctUINT32 currLines = (lines2render > maxHeight)
                ? maxHeight
                : lines2render;

            streamRect.top     = streamRect.bottom;
            streamRect.bottom += currLines;

            destSubRect.top     = destSubRect.bottom;
            destSubRect.bottom += currLines;

            /* Do the blit. */
            gcmERR_BREAK(gco2D_MonoBlit(
                engine,
                Source,
                SourceSize,
                &streamRect,
                SourcePack,
                streamPack,
                &destSubRect,
                FgRop, BgRop,
                DestSurface->info.format
                ));

            /* Update the line counter. */
            lines2render -= currLines;
        }
        while (lines2render);
    }
    while (gcvFALSE);

    /* Unlock the destination. */
    if (destMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destMemory[0]));
    }

    if (useSotfEngine)
    {
        /* Disable software renderer. */
        gcmVERIFY_OK(gcoHARDWARE_UseSoftware2D(gcvNULL, gcvFALSE));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_FilterBlit
**
**  Filter blit.
**
**  INPUT:
**
**      gcoSURF SrcSurface
**          Pointer to the source surface.
**
**      gcoSURF DestSurface
**          Pointer to the destination surface.
**
**      gcsRECT_PTR SrcRect
**          Coordinates of the entire source image.
**
**      gcsRECT_PTR DestRect
**          Coordinates of the entire destination image.
**
**      gcsRECT_PTR DestSubRect
**          Coordinates of a sub area within the destination to render.
**          If DestSubRect is gcvNULL, the complete image will be rendered
**          using coordinates set by DestRect.
**          If DestSubRect is not gcvNULL and DestSubRect and DestRect are
**          no equal, DestSubRect is assumed to be within DestRect and
**          will be used to reneder the sub area only.
**
**  OUTPUT:
**
**      Nothing.
*/

gceSTATUS
gcoSURF_FilterBlit(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsRECT_PTR SrcRect,
    IN gcsRECT_PTR DestRect,
    IN gcsRECT_PTR DestSubRect
    )
{
    gceSTATUS status;
    gcsRECT destSubRect, srcRect, destRect;
    gctBOOL ditherBy3D = gcvFALSE;
    gctBOOL ditherNotSupported = gcvFALSE;
    gctBOOL rotateWK =  gcvFALSE;
    gctBOOL enable2DDither =  gcvFALSE;

    gctPOINTER srcMemory[3] = {gcvNULL, };
    gctPOINTER destMemory[3] = {gcvNULL, };

    gcsSURF_INFO_PTR tempSurf = gcvNULL;
    gcsSURF_INFO_PTR temp2Surf = gcvNULL;

    gco2D engine = gcvNULL;

    gceSURF_ROTATION srcRotBackup = (gceSURF_ROTATION)-1, dstRotBackup = (gceSURF_ROTATION)-1;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x SrcRect=0x%x DestRect=0x%x "
              "DestSubRect=0x%x",
              SrcSurface, DestSurface, SrcRect, DestRect, DestSubRect);

    if (SrcSurface == gcvNULL || DestSurface == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    do
    {
        gcsSURF_FORMAT_INFO_PTR srcFormat[2];
        gcsSURF_FORMAT_INFO_PTR destFormat[2];

        /* Verify the surfaces. */
        gcmBADOBJECT_BREAK(SrcSurface, gcvOBJ_SURF);
        gcmBADOBJECT_BREAK(DestSurface, gcvOBJ_SURF);

        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        /* Use surface rect if not specified. */
        if (SrcRect == gcvNULL)
        {
            SrcRect = &SrcSurface->info.rect;
        }

        /* Use surface rect if not specified. */
        if (DestRect == gcvNULL)
        {
            DestRect = &DestSurface->info.rect;
        }

        /* Make sure the destination sub rectangle is set. */
        if (DestSubRect == gcvNULL)
        {
            destSubRect.left   = 0;
            destSubRect.top    = 0;
            destSubRect.right  = DestRect->right  - DestRect->left;
            destSubRect.bottom = DestRect->bottom - DestRect->top;

            DestSubRect = &destSubRect;
        }

        gcmERR_BREAK(gcoSURF_QueryFormat(SrcSurface->info.format, srcFormat));
        gcmERR_BREAK(gcoSURF_QueryFormat(DestSurface->info.format, destFormat));

        if ((SrcSurface->info.dither2D || DestSurface->info.dither2D)
            && ((srcFormat[0]->bitsPerPixel > destFormat[0]->bitsPerPixel)
            || (srcFormat[0]->fmtClass == gcvFORMAT_CLASS_YUV)))
        {
            if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_2D_DITHER))
            {
                gcmERR_BREAK(gco2D_EnableDither(
                    engine,
                    gcvTRUE));

                enable2DDither = gcvTRUE;
            }
            else if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PIPE_3D))
            {
                ditherBy3D = gcvTRUE;
            }
            else
            {
                /* Hardware does not support dither. */
                ditherNotSupported = gcvTRUE;
            }
        }

        if ((SrcSurface->info.rotation != gcvSURF_0_DEGREE || DestSurface->info.rotation != gcvSURF_0_DEGREE)
            && !gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_2D_FILTERBLIT_FULLROTATION))
        {
            rotateWK = gcvTRUE;
        }
        else if (ditherBy3D && (((DestSubRect->right - DestSubRect->left) & 15)
            || ((DestSubRect->bottom - DestSubRect->top) & 3)))
        {
            rotateWK = gcvTRUE;
        }

        /* Lock the destination. */
        gcmERR_BREAK(gcoSURF_Lock(
            DestSurface,
            gcvNULL,
            destMemory
            ));

        /* Lock the source. */
        gcmERR_BREAK(gcoSURF_Lock(
            SrcSurface,
            gcvNULL,
            srcMemory
            ));

        if (ditherBy3D || rotateWK)
        {
            gcsRECT tempRect;

            srcRotBackup = SrcSurface->info.rotation;
            dstRotBackup = DestSurface->info.rotation;

            srcRect = *SrcRect;
            destRect = *DestRect;
            destSubRect = *DestSubRect;

            if (rotateWK)
            {
                if (SrcSurface->info.rotation != gcvSURF_0_DEGREE)
                {
                    SrcSurface->info.rotation = gcvSURF_0_DEGREE;
                    gcmERR_BREAK(gcsRECT_RelativeRotation(srcRotBackup, &DestSurface->info.rotation));

                    gcmERR_BREAK(gcsRECT_Rotate(
                        &srcRect,
                        srcRotBackup,
                        gcvSURF_0_DEGREE,
                        SrcSurface->info.alignedWidth,
                        SrcSurface->info.alignedHeight));

                    destSubRect.left += destRect.left;
                    destSubRect.right += destRect.left;
                    destSubRect.top += destRect.top;
                    destSubRect.bottom += destRect.top;

                    gcmERR_BREAK(gcsRECT_Rotate(
                        &destSubRect,
                        dstRotBackup,
                        DestSurface->info.rotation,
                        DestSurface->info.alignedWidth,
                        DestSurface->info.alignedHeight));

                    gcmERR_BREAK(gcsRECT_Rotate(
                        &destRect,
                        dstRotBackup,
                        DestSurface->info.rotation,
                        DestSurface->info.alignedWidth,
                        DestSurface->info.alignedHeight));

                    destSubRect.left -= destRect.left;
                    destSubRect.right -= destRect.left;
                    destSubRect.top -= destRect.top;
                    destSubRect.bottom -= destRect.top;
                }

                tempRect.left = 0;
                tempRect.top = 0;
                tempRect.right = destRect.right - destRect.left;
                tempRect.bottom = destRect.bottom - destRect.top;

                gcmERR_BREAK(gcoHARDWARE_Get2DTempSurface(
                    gcvNULL,
                    tempRect.right,
                    tempRect.bottom,
                    ditherBy3D ? gcvSURF_A8R8G8B8 : DestSurface->info.format,
                    DestSurface->info.flags,
                    &tempSurf));

                tempSurf->rotation = gcvSURF_0_DEGREE;
            }
#if gcdENABLE_3D
            else
            {
                /* Only dither. */
                gctBOOL swap = (DestSurface->info.rotation == gcvSURF_90_DEGREE)
                    || (DestSurface->info.rotation == gcvSURF_270_DEGREE);

                tempRect.left = 0;
                tempRect.top = 0;
                tempRect.right = destRect.right - destRect.left;
                tempRect.bottom = destRect.bottom - destRect.top;

                gcmERR_BREAK(gcoHARDWARE_Get2DTempSurface(
                    gcvNULL,
                    swap ? tempRect.bottom : tempRect.right,
                    swap ? tempRect.right : tempRect.bottom,
                    gcvSURF_A8R8G8B8,
                    DestSurface->info.flags,
                    &tempSurf
                    ));

                tempSurf->rotation = DestSurface->info.rotation;
            }
#endif /* gcdENABLE_3D */

            gcmERR_BREAK(gco2D_FilterBlitEx(
                engine,
                SrcSurface->info.node.physical,
                SrcSurface->info.stride,
                SrcSurface->info.node.physical2,
                SrcSurface->info.uStride,
                SrcSurface->info.node.physical3,
                SrcSurface->info.vStride,
                SrcSurface->info.format,
                SrcSurface->info.rotation,
                SrcSurface->info.alignedWidth,
                SrcSurface->info.alignedHeight,
                &srcRect,
                tempSurf->node.physical,
                tempSurf->stride,
                tempSurf->format,
                tempSurf->rotation,
                tempSurf->alignedWidth,
                tempSurf->alignedHeight,
                &tempRect,
                &destSubRect
                ));

            tempRect = destSubRect;
#if gcdENABLE_3D
            if (ditherBy3D)
            {
                gcsPOINT srcOrigin, destOrigin, rectSize;
                gcsSURF_INFO * DitherDest;
                gctBOOL savedDeferDither3D;

                if (rotateWK)
                {
                    srcOrigin.x = tempRect.left;
                    srcOrigin.y = tempRect.top;

                    rectSize.x = tempRect.right - tempRect.left;
                    rectSize.y = tempRect.bottom - tempRect.top;

                    destOrigin.x = 0;
                    destOrigin.y = 0;

                    tempRect.left = 0;
                    tempRect.top = 0;
                    tempRect.right = rectSize.x;
                    tempRect.bottom = rectSize.y;

                    gcmERR_BREAK(gcoHARDWARE_Get2DTempSurface(
                        gcvNULL,
                        tempRect.right,
                        tempRect.bottom,
                        DestSurface->info.format,
                        DestSurface->info.flags,
                        &temp2Surf
                        ));

                    temp2Surf->rotation = gcvSURF_0_DEGREE;

                    DitherDest = temp2Surf;
                }
                else
                {
                    destSubRect.left += destRect.left;
                    destSubRect.right += destRect.left;
                    destSubRect.top += destRect.top;
                    destSubRect.bottom += destRect.top;

                    if (DestSurface->info.rotation != gcvSURF_0_DEGREE)
                    {
                        gcmERR_BREAK(gcsRECT_Rotate(
                            &destSubRect,
                            DestSurface->info.rotation,
                            gcvSURF_0_DEGREE,
                            DestSurface->info.alignedWidth,
                            DestSurface->info.alignedHeight));

                        gcmERR_BREAK(gcsRECT_Rotate(
                            &tempRect,
                            DestSurface->info.rotation,
                            gcvSURF_0_DEGREE,
                            tempSurf->alignedWidth,
                            tempSurf->alignedHeight));

                        DestSurface->info.rotation = gcvSURF_0_DEGREE;
                        tempSurf->rotation = gcvSURF_0_DEGREE;
                    }

                    srcOrigin.x = tempRect.left;
                    srcOrigin.y = tempRect.top;

                    destOrigin.x = destSubRect.left;
                    destOrigin.y = destSubRect.top;

                    rectSize.x = destSubRect.right - destSubRect.left;
                    rectSize.y = destSubRect.bottom - destSubRect.top;

                    DitherDest = &DestSurface->info;
                }

                gcmERR_BREAK(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));

                /* Mark resolve to enable dither */
                savedDeferDither3D = tempSurf->deferDither3D;
                tempSurf->deferDither3D = gcvTRUE;

                rectSize.x = gcmALIGN(rectSize.x, 16);
                rectSize.y = gcmALIGN(rectSize.y, 4);

                gcmERR_BREAK(gcoHARDWARE_ResolveRect(
                        gcvNULL,
                        tempSurf,
                        DitherDest,
                        &srcOrigin, &destOrigin, &rectSize, gcvFALSE));

                gcmERR_BREAK(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));

                tempSurf->deferDither3D = savedDeferDither3D;
            }
#endif /* gcdENABLE_3D */
            if (rotateWK)
            {
                /* bitblit rorate. */
                gcsSURF_INFO * srcSurf = ditherBy3D ? temp2Surf : tempSurf;
                gceSURF_ROTATION tSrcRot = (gceSURF_ROTATION)-1, tDestRot = (gceSURF_ROTATION)-1;
                gctBOOL bMirror = gcvFALSE;

                destSubRect.left += destRect.left;
                destSubRect.right += destRect.left;
                destSubRect.top += destRect.top;
                destSubRect.bottom += destRect.top;

                if (!gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_2D_BITBLIT_FULLROTATION))
                {
                    tDestRot = DestSurface->info.rotation;

                    gcmERR_BREAK(gcsRECT_RelativeRotation(srcSurf->rotation, &tDestRot));
                    switch (tDestRot)
                    {
                    case gcvSURF_0_DEGREE:
                        tSrcRot = tDestRot = gcvSURF_0_DEGREE;

                        break;

                    case gcvSURF_90_DEGREE:
                        tSrcRot = gcvSURF_0_DEGREE;
                        tDestRot = gcvSURF_90_DEGREE;

                        break;

                    case gcvSURF_180_DEGREE:
                        tSrcRot = tDestRot = gcvSURF_0_DEGREE;
                        bMirror = gcvTRUE;

                        break;

                    case gcvSURF_270_DEGREE:
                        tSrcRot = gcvSURF_90_DEGREE;
                        tDestRot = gcvSURF_0_DEGREE;

                        break;

                    default:
                        status = gcvSTATUS_NOT_SUPPORTED;

                        break;
                    }

                    gcmERR_BREAK(status);

                    gcmERR_BREAK(gcsRECT_Rotate(
                        &tempRect,
                        srcSurf->rotation,
                        tSrcRot,
                        srcSurf->alignedWidth,
                        srcSurf->alignedHeight));

                    gcmERR_BREAK(gcsRECT_Rotate(
                        &destSubRect,
                        DestSurface->info.rotation,
                        tDestRot,
                        DestSurface->info.alignedWidth,
                        DestSurface->info.alignedHeight));

                    srcSurf->rotation = tSrcRot;
                    DestSurface->info.rotation = tDestRot;

                    if (bMirror)
                    {
                        gcmERR_BREAK(gco2D_SetBitBlitMirror(
                            engine,
                            gcvTRUE,
                            gcvTRUE));
                    }
                }

                gcmERR_BREAK(gco2D_SetClipping(
                    engine,
                    &destSubRect));

                gcmERR_BREAK(gco2D_SetColorSourceEx(
                    engine,
                    srcSurf->node.physical,
                    srcSurf->stride,
                    srcSurf->format,
                    srcSurf->rotation,
                    srcSurf->alignedWidth,
                    srcSurf->alignedHeight,
                    gcvFALSE,
                    gcvSURF_OPAQUE,
                    0
                    ));

                gcmERR_BREAK(gco2D_SetSource(
                    engine,
                    &tempRect
                    ));

                gcmERR_BREAK(gco2D_SetTargetEx(
                    engine,
                    DestSurface->info.node.physical,
                    DestSurface->info.stride,
                    DestSurface->info.rotation,
                    DestSurface->info.alignedWidth,
                    DestSurface->info.alignedHeight
                    ));

                gcmERR_BREAK(gco2D_Blit(
                    engine,
                    1,
                    &destSubRect,
                    0xCC,
                    0xCC,
                    DestSurface->info.format
                    ));

                if (bMirror)
                {
                    gcmERR_BREAK(gco2D_SetBitBlitMirror(
                        engine,
                        gcvFALSE,
                        gcvFALSE));
                }
            }
        }
        else
        {
            /* Call gco2D object to complete the blit. */
            gcmERR_BREAK(gco2D_FilterBlitEx(
                engine,
                SrcSurface->info.node.physical,
                SrcSurface->info.stride,
                SrcSurface->info.node.physical2,
                SrcSurface->info.uStride,
                SrcSurface->info.node.physical3,
                SrcSurface->info.vStride,
                SrcSurface->info.format,
                SrcSurface->info.rotation,
                SrcSurface->info.alignedWidth,
                SrcSurface->info.alignedHeight,
                SrcRect,
                DestSurface->info.node.physical,
                DestSurface->info.stride,
                DestSurface->info.format,
                DestSurface->info.rotation,
                DestSurface->info.alignedWidth,
                DestSurface->info.alignedHeight,
                DestRect,
                DestSubRect
                ));
        }
    }
    while (gcvFALSE);

    if (enable2DDither)
    {
        gcmVERIFY_OK(gco2D_EnableDither(
            engine,
            gcvFALSE));
    }

    if (srcRotBackup != (gceSURF_ROTATION)-1)
    {
        SrcSurface->info.rotation = srcRotBackup;
    }

    if (dstRotBackup != (gceSURF_ROTATION)-1)
    {
        DestSurface->info.rotation = dstRotBackup;
    }

    /* Unlock the source. */
    if (srcMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(SrcSurface, srcMemory[0]));
    }

    /* Unlock the destination. */
    if (destMemory[0] != gcvNULL)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(DestSurface, destMemory[0]));
    }

    /* Free temp buffer. */
    if (tempSurf != gcvNULL)
    {
        gcmVERIFY_OK(gcoHARDWARE_Put2DTempSurface(gcvNULL, tempSurf));
    }

    if (temp2Surf != gcvNULL)
    {
        gcmVERIFY_OK(gcoHARDWARE_Put2DTempSurface(gcvNULL, temp2Surf));
    }

    if (ditherNotSupported)
    {
        status = gcvSTATUS_NOT_SUPPORT_DITHER;
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_EnableAlphaBlend
**
**  Enable alpha blending engine in the hardware and disengage the ROP engine.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gctUINT8 SrcGlobalAlphaValue
**          Global alpha value for the color components.
**
**      gctUINT8 DstGlobalAlphaValue
**          Global alpha value for the color components.
**
**      gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode
**          Per-pixel alpha component mode.
**
**      gceSURF_PIXEL_ALPHA_MODE DstAlphaMode
**          Per-pixel alpha component mode.
**
**      gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode
**          Global/per-pixel alpha values selection.
**
**      gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode
**          Global/per-pixel alpha values selection.
**
**      gceSURF_BLEND_FACTOR_MODE SrcFactorMode
**          Final blending factor mode.
**
**      gceSURF_BLEND_FACTOR_MODE DstFactorMode
**          Final blending factor mode.
**
**      gceSURF_PIXEL_COLOR_MODE SrcColorMode
**          Per-pixel color component mode.
**
**      gceSURF_PIXEL_COLOR_MODE DstColorMode
**          Per-pixel color component mode.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_EnableAlphaBlend(
    IN gcoSURF Surface,
    IN gctUINT8 SrcGlobalAlphaValue,
    IN gctUINT8 DstGlobalAlphaValue,
    IN gceSURF_PIXEL_ALPHA_MODE SrcAlphaMode,
    IN gceSURF_PIXEL_ALPHA_MODE DstAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE SrcGlobalAlphaMode,
    IN gceSURF_GLOBAL_ALPHA_MODE DstGlobalAlphaMode,
    IN gceSURF_BLEND_FACTOR_MODE SrcFactorMode,
    IN gceSURF_BLEND_FACTOR_MODE DstFactorMode,
    IN gceSURF_PIXEL_COLOR_MODE SrcColorMode,
    IN gceSURF_PIXEL_COLOR_MODE DstColorMode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x SrcGlobalAlphaValue=%u DstGlobalAlphaValue=%u "
              "SrcAlphaMode=%d DstAlphaMode=%d SrcGlobalAlphaMode=%d "
              "DstGlobalAlphaMode=%d SrcFactorMode=%d DstFactorMode=%d "
              "SrcColorMode=%d DstColorMode=%d",
              Surface, SrcGlobalAlphaValue, DstGlobalAlphaValue, SrcAlphaMode,
              DstAlphaMode, SrcGlobalAlphaMode, DstGlobalAlphaMode,
              SrcFactorMode, DstFactorMode, SrcColorMode, DstColorMode);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do {
        gco2D engine;

        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));

        gcmERR_BREAK(gco2D_EnableAlphaBlend(
                engine,
                (gctUINT32)SrcGlobalAlphaValue << 24,
                (gctUINT32)DstGlobalAlphaValue << 24,
                SrcAlphaMode,
                DstAlphaMode,
                SrcGlobalAlphaMode,
                DstGlobalAlphaMode,
                SrcFactorMode,
                DstFactorMode,
                SrcColorMode,
                DstColorMode
                ));

    } while (gcvFALSE);

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_DisableAlphaBlend
**
**  Disable alpha blending engine in the hardware and engage the ROP engine.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_DisableAlphaBlend(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        gco2D engine;
        gcmERR_BREAK(gcoHAL_Get2DEngine(gcvNULL, &engine));
        gcmERR_BREAK(gco2D_DisableAlphaBlend(engine));
    }
    while (gcvFALSE);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Set2DSource
**
**  Set source surface for 2D engine.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_ROTATION Rotation
**          Source rotation parameter.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Set2DSource(
    gcoSURF Surface,
    gceSURF_ROTATION Rotation
    )
{
    gceSTATUS status;
    gco2D engine;
    gctUINT addressNum;
    gctUINT physical[3];
    gctUINT stride[3];
    gctUINT width;
    gctUINT height;
    gce2D_TILE_STATUS_CONFIG tileStatusConfig;

    gcmHEADER_ARG("Source=0x%x Rotation=%d", Surface, Rotation);
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    /* Get 2D engine. */
    gcmONERROR(gcoHAL_Get2DEngine(gcvNULL, &engine));

    /* Get shortcut. */
    addressNum = Surface->info.node.count;

    /* Calculate surface size. */
#if gcdENABLE_3D
    width  = Surface->info.rect.right  / Surface->info.samples.x;
    height = Surface->info.rect.bottom / Surface->info.samples.y;
#else
    width  = Surface->info.rect.right;
    height = Surface->info.rect.bottom;
#endif

    /* Get physical addresses and strides. */
    switch (addressNum)
    {
    case 3:
        physical[2] = Surface->info.node.physical3;
        stride  [2] = Surface->info.vStride;
    case 2:
        physical[1] = Surface->info.node.physical2;
        stride  [1] = Surface->info.uStride;
    case 1:
        physical[0] = Surface->info.node.physical;
        stride  [0] = Surface->info.stride;
        break;
    }

    if (Surface->info.tiling & gcvTILING_SPLIT_BUFFER)
    {
        /* Split buffer needs two addresses. */
        gcmASSERT(addressNum == 1);

        physical[1] = Surface->info.node.physicalBottom;
        stride  [1] = Surface->info.stride;
        addressNum  = 2;
    }

    /* Extract master surface arguments and call 2D api. */
    gcmONERROR(
        gco2D_SetGenericSource(engine,
                               physical,
                               addressNum,
                               stride,
                               addressNum,
                               Surface->info.tiling,
                               Surface->info.format,
                               Rotation,
                               width,
                               height));

#if gcdENABLE_3D
    /* Extract tile status arguments. */
    if ((Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN) ||
        (Surface->info.tileStatusDisabled) ||
        (Surface->info.dirty == gcvFALSE))
    {
        /* No tile status or tile status disabled. */
        tileStatusConfig = gcv2D_TSC_DISABLE;
    }
    else
    {
        /* Tile status enabled. */
        tileStatusConfig = Surface->info.compressed ? gcv2D_TSC_COMPRESSED
                         : gcv2D_TSC_ENABLE;
    }

    if (Surface->info.samples.x > 1 || Surface->info.samples.y > 1)
    {
        /* Down-sample. */
        tileStatusConfig |= gcv2D_TSC_DOWN_SAMPLER;
    }

    /* Set tile status states. */
    gcmONERROR(
        gco2D_SetSourceTileStatus(engine,
                                  tileStatusConfig,
                                  Surface->info.format,
                                  Surface->info.fcValue,
                                  Surface->info.tileStatusNode.physical));

#else
    /* No 3D. */
    tileStatusConfig = gcv2D_TSC_DISABLE;

    /* Set tile status states. */
    gcmONERROR(
        gco2D_SetSourceTileStatus(engine,
                                  tileStatusConfig,
                                  gcvSURF_UNKNOWN,
                                  0,
                                  ~0U));
#endif

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Set2DTarget
**
**  Set target surface for 2D engine.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_ROTATION Rotation
**          Destination rotation parameter.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Set2DTarget(
    gcoSURF Surface,
    gceSURF_ROTATION Rotation
    )
{
    gceSTATUS status;
    gco2D engine;
    gctUINT addressNum;
    gctUINT physical[3];
    gctUINT stride[3];
    gctUINT width;
    gctUINT height;
    gce2D_TILE_STATUS_CONFIG tileStatusConfig;

    gcmHEADER_ARG("Source=0x%x Rotation=%d", Surface, Rotation);
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    /* Get 2D engine. */
    gcmONERROR(gcoHAL_Get2DEngine(gcvNULL, &engine));

    /* Get shortcut. */
    addressNum = Surface->info.node.count;

    /* Calculate surface size. */
#if gcdENABLE_3D
    width  = Surface->info.rect.right  / Surface->info.samples.x;
    height = Surface->info.rect.bottom / Surface->info.samples.y;
#else
    width  = Surface->info.rect.right;
    height = Surface->info.rect.bottom;
#endif

    /* Get physical addresses and strides. */
    switch (addressNum)
    {
    case 3:
        physical[2] = Surface->info.node.physical3;
        stride  [2] = Surface->info.vStride;
    case 2:
        physical[1] = Surface->info.node.physical2;
        stride  [1] = Surface->info.uStride;
    case 1:
        physical[0] = Surface->info.node.physical;
        stride  [0] = Surface->info.stride;
        break;
    }

    if (Surface->info.tiling & gcvTILING_SPLIT_BUFFER)
    {
        /* Split buffer needs two addresses. */
        gcmASSERT(addressNum == 1);

        physical[1] = Surface->info.node.physicalBottom;
        stride  [1] = Surface->info.stride;
        addressNum  = 2;
    }

    /* Extract master surface arguments and call 2D api. */
    gcmONERROR(
        gco2D_SetGenericTarget(engine,
                               physical,
                               addressNum,
                               stride,
                               addressNum,
                               Surface->info.tiling,
                               Surface->info.format,
                               Rotation,
                               width,
                               height));

#if gcdENABLE_3D
    /* Extract tile status arguments. */
    if ((Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN) ||
        (Surface->info.tileStatusDisabled) ||
        (Surface->info.dirty == gcvFALSE))
    {
        /* No tile status or tile status disabled. */
        tileStatusConfig = gcv2D_TSC_DISABLE;
    }
    else
    {
        /* Tile status enabled. */
        tileStatusConfig = Surface->info.compressed ? gcv2D_TSC_COMPRESSED
                         : gcv2D_TSC_ENABLE;
    }

    /* Set tile status states. */
    gcmONERROR(
        gco2D_SetTargetTileStatus(engine,
                                  tileStatusConfig,
                                  Surface->info.format,
                                  Surface->info.fcValue,
                                  Surface->info.tileStatusNode.physical));

#else
    /* No 3D. */
    tileStatusConfig = gcv2D_TSC_DISABLE;

    /* Set tile status states. */
    gcmONERROR(
        gco2D_SetTargetTileStatus(engine,
                                  tileStatusConfig,
                                  gcvSURF_UNKNOWN,
                                  0,
                                  ~0U));
#endif

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}
#endif  /* gcdENABLE_2D */

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoSURF_CopyPixels
**
**  Copy a rectangular area from one surface to another with format conversion.
**
**  INPUT:
**
**      gcoSURF Source
**          Pointer to the source surface.
**
**      gcoSURF Target
**          Pointer to the target surface.
**
**      gctINT SourceX, SourceY
**          Source surface origin.
**
**      gctINT TargetX, TargetY
**          Target surface origin.
**
**      gctINT Width, Height
**          The size of the area. If Height is negative, the area will
**          be copied with a vertical flip.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_CopyPixels(
    IN gcoSURF Source,
    IN gcoSURF Target,
    IN gctINT SourceX,
    IN gctINT SourceY,
    IN gctINT TargetX,
    IN gctINT TargetY,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status, last;
    gctPOINTER srcMemory[3] = {gcvNULL};
    gctPOINTER trgMemory[3] = {gcvNULL};

    gcmHEADER_ARG("Source=0x%x Target=0x%x SourceX=%d SourceY=%d TargetX=%d TargetY=%d "
              "Width=%d Height=%d",
              Source, Target, SourceX, SourceY, TargetX, TargetY, Width,
              Height);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Source, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(Target, gcvOBJ_SURF);

    do
    {
        gctINT realHeight = gcmABS(Height);
        /* Lock the surfaces. */
        gcmERR_BREAK(
            gcoSURF_Lock(Source, gcvNULL, srcMemory));
        gcmERR_BREAK(
            gcoSURF_Lock(Target, gcvNULL, trgMemory));

        if (Source->info.type == gcvSURF_BITMAP)
        {
            /* Flush the CPU cache. Source would've been rendered by the CPU. */
            gcmERR_BREAK(gcoSURF_NODE_Cache(
                &Source->info.node,
                srcMemory[0],
                Source->info.size,
                gcvCACHE_CLEAN));
        }

        if (Target->info.type == gcvSURF_BITMAP)
        {
            gcmERR_BREAK(gcoSURF_NODE_Cache(
                &Target->info.node,
                trgMemory[0],
                Target->info.size,
                gcvCACHE_FLUSH));
        }

        /* Flush the surfaces. */
        gcmERR_BREAK(gcoSURF_Flush(Source));
        gcmERR_BREAK(gcoSURF_Flush(Target));

#if gcdENABLE_3D
        /* Disable the tile status and decompress the buffers. */

        /* Don't disable tilestatus when fastMSAA is present. Otherwise it will hang.
           Also this is unnecessary. */
        if ((Source->info.samples.x == 1) || (Source->info.samples.y == 1))
        {
            gcmERR_BREAK(
                gcoSURF_DisableTileStatus(Source, gcvTRUE));
        }

        /* Disable the tile status for the destination. */
        gcmERR_BREAK(
            gcoSURF_DisableTileStatus(Target, gcvTRUE));

#endif /* gcdENABLE_3D */

        /*
        ** Only unsigned normalized data type and no space conversion needed go hardware copy pixels path.
        ** as this path can't handle other cases.
        */
        if ((Source->info.formatInfo.fmtDataType == gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED) &&
            (Target->info.formatInfo.fmtDataType == gcvFORMAT_DATATYPE_UNSIGNED_NORMALIZED) &&
            (!Source->info.formatInfo.fakedFormat || (Source->info.paddingFormat && !Source->info.garbagePadded)) &&
            (!Target->info.formatInfo.fakedFormat || Target->info.paddingFormat) &&
            (Source->info.colorSpace == Target->info.colorSpace))
        {
            /* Read the pixel. */
            gcmERR_BREAK(
                gcoHARDWARE_CopyPixels(gcvNULL,
                &Source->info,
                &Target->info,
                SourceX, SourceY,
                TargetX, TargetY,
                Width, Height));
        }
        else
        {
            gcsSURF_BLIT_ARGS arg;

            gcoOS_ZeroMemory(&arg, sizeof(arg));

            /* The offset must be multiple of sliceSize */
            gcmASSERT(Source->info.offset % Source->info.sliceSize == 0);
            gcmASSERT(Target->info.offset % Target->info.sliceSize == 0);

            arg.srcSurface = Source;
            arg.srcX       = SourceX;
            arg.srcY       = SourceY;
            arg.srcZ       = Source->info.offset / Source->info.sliceSize;
            arg.dstSurface = Target;
            arg.dstX       = TargetX;
            arg.dstY       = TargetY;
            arg.dstZ       = Target->info.offset / Target->info.sliceSize;
            arg.srcWidth   = arg.dstWidth  = Width;
            arg.srcHeight  = arg.dstHeight = realHeight;
            arg.srcDepth   = arg.dstDepth  = 1;
            arg.yReverse   = (Height < 0);
            gcmERR_BREAK(gcoSURF_BlitCPU(&arg));
        }

        if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PE_DITHER_FIX) == gcvFALSE &&
            TargetX == 0 && Width  >= Target->info.rect.right  / Target->info.samples.x &&
            TargetY == 0 && realHeight >= Target->info.rect.bottom / Target->info.samples.y)
        {
            Target->info.deferDither3D = gcvFALSE;
        }
    }
    while (gcvFALSE);

    /* Unlock the surfaces. */
    if (srcMemory[0] != gcvNULL)
    {
        gcmCHECK_STATUS(
            gcoSURF_Unlock(Source, srcMemory[0]));
    }

    if (trgMemory[0] != gcvNULL)
    {
        gcmCHECK_STATUS(
            gcoSURF_Unlock(Target, trgMemory[0]));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_ReadPixel
**
**  gcoSURF_ReadPixel reads and returns the current value of the pixel from
**  the specified surface. The pixel value is returned in the specified format.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gctPOINTER Memory
**          Pointer to the actual surface bits returned by gcoSURF_Lock.
**
**      gctINT X, Y
**          Coordinates of the pixel.
**
**      gceSURF_FORMAT Format
**          Format of the pixel value to be returned.
**
**  OUTPUT:
**
**      gctPOINTER PixelValue
**          Pointer to the placeholder for the result.
*/
gceSTATUS
gcoSURF_ReadPixel(
    IN gcoSURF Surface,
    IN gctPOINTER Memory,
    IN gctINT X,
    IN gctINT Y,
    IN gceSURF_FORMAT Format,
    OUT gctPOINTER PixelValue
    )
{
    gceSTATUS status, last;
    gctPOINTER srcMemory[3] = {gcvNULL};
    gcsSURF_INFO target;

    gcmHEADER_ARG("Surface=0x%x Memory=0x%x X=%d Y=%d Format=%d",
                  Surface, Memory, X, Y, Format);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Flush the surface. */
        gcmERR_BREAK(
            gcoSURF_Flush(Surface));

#if gcdENABLE_3D
        /* Flush the tile status and decompress the buffers. */
        gcmERR_BREAK(
            gcoSURF_DisableTileStatus(Surface, gcvTRUE));
#endif /* gcdENABLE_3D */

        /* Lock the source surface. */
        gcmERR_BREAK(
            gcoSURF_Lock(Surface, gcvNULL, srcMemory));

        /* Fill in the target structure. */
        target.type          = gcvSURF_BITMAP;
        target.format        = Format;

        target.rect.left     = 0;
        target.rect.top      = 0;
        target.rect.right    = 1;
        target.rect.bottom   = 1;

        target.alignedWidth  = 1;
        target.alignedHeight = 1;

        target.rotation      = gcvSURF_0_DEGREE;

        target.stride        = 0;
        target.size          = 0;

        target.node.valid    = gcvTRUE;
        target.node.logical  = (gctUINT8_PTR) PixelValue;

        target.samples.x     = 1;
        target.samples.y     = 1;

        /* Read the pixel. */
        gcmERR_BREAK(
            gcoHARDWARE_CopyPixels(gcvNULL,
                                   &Surface->info,
                                   &target,
                                   X, Y,
                                   0, 0,
                                   1, 1));
    }
    while (gcvFALSE);

    /* Unlock the source surface. */
    if (srcMemory[0] != gcvNULL)
    {
        gcmCHECK_STATUS(
            gcoSURF_Unlock(Surface, srcMemory[0]));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_WritePixel
**
**  gcoSURF_WritePixel writes a color value to a pixel of the specified surface.
**  The pixel value is specified in the specified format.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gctPOINTER Memory
**          Pointer to the actual surface bits returned by gcoSURF_Lock.
**
**      gctINT X, Y
**          Coordinates of the pixel.
**
**      gceSURF_FORMAT Format
**          Format of the pixel value to be returned.
**
**      gctPOINTER PixelValue
**          Pointer to the pixel value.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_WritePixel(
    IN gcoSURF Surface,
    IN gctPOINTER Memory,
    IN gctINT X,
    IN gctINT Y,
    IN gceSURF_FORMAT Format,
    IN gctPOINTER PixelValue
    )
{
    gceSTATUS status, last;
    gctPOINTER trgMemory[3] = {gcvNULL};
    gcsSURF_INFO source;

    gcmHEADER_ARG("Surface=0x%x Memory=0x%x X=%d Y=%d Format=%d PixelValue=0x%x",
              Surface, Memory, X, Y, Format, PixelValue);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    do
    {
        /* Flush the surface. */
        gcmERR_BREAK(
            gcoSURF_Flush(Surface));

#if gcdENABLE_3D
        /* Disable the tile status and decompress the buffers. */
        gcmERR_BREAK(
            gcoSURF_DisableTileStatus(Surface, gcvTRUE));
#endif

        /* Lock the source surface. */
        gcmERR_BREAK(
            gcoSURF_Lock(Surface, gcvNULL, trgMemory));

        /* Fill in the source structure. */
        source.type          = gcvSURF_BITMAP;
        source.format        = Format;

        source.rect.left     = 0;
        source.rect.top      = 0;
        source.rect.right    = 1;
        source.rect.bottom   = 1;

        source.alignedWidth  = 1;
        source.alignedHeight = 1;

        source.rotation      = gcvSURF_0_DEGREE;

        source.stride        = 0;
        source.size          = 0;

        source.node.valid    = gcvTRUE;
        source.node.logical  = (gctUINT8_PTR) PixelValue;

        source.samples.x     = 1;
        source.samples.y     = 1;

        /* Read the pixel. */
        gcmERR_BREAK(
            gcoHARDWARE_CopyPixels(gcvNULL,
                                   &source,
                                   &Surface->info,
                                   0, 0,
                                   X, Y,
                                   1, 1));
    }
    while (gcvFALSE);

    /* Unlock the source surface. */
    if (trgMemory[0] != gcvNULL)
    {
        gcmCHECK_STATUS(gcoSURF_Unlock(Surface, trgMemory[0]));
    }

    /* Return status. */
    gcmFOOTER();
    return status;
}
#endif /* gcdENABLE_3D */

gceSTATUS
gcoSURF_NODE_Cache(
    IN gcsSURF_NODE_PTR Node,
    IN gctPOINTER Logical,
    IN gctSIZE_T Bytes,
    IN gceCACHEOPERATION Operation
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Node=0x%x, Operation=%d, Bytes=%u", Node, Operation, Bytes);

    if (Node->pool == gcvPOOL_USER)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

#if !gcdPAGED_MEMORY_CACHEABLE
    if (Node->u.normal.cacheable == gcvFALSE)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }
#endif

    switch (Operation)
    {
        case gcvCACHE_CLEAN:
            gcmONERROR(gcoOS_CacheClean(gcvNULL, Node->u.normal.node, Logical, Bytes));
            break;

        case gcvCACHE_INVALIDATE:
            gcmONERROR(gcoOS_CacheInvalidate(gcvNULL, Node->u.normal.node, Logical, Bytes));
            break;

        case gcvCACHE_FLUSH:
            gcmONERROR(gcoOS_CacheFlush(gcvNULL, Node->u.normal.node, Logical, Bytes));
            break;

        default:
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
            break;
    }

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_NODE_CPUCacheOperation
**
**  Perform the specified CPU cache operation on the surface node.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR Node
**          Pointer to the surface node.
**      gctPOINTER Logical
**          logical address to flush.
**      gctSIZE_T Bytes
**          bytes to flush.
**      gceSURF_CPU_CACHE_OP_TYPE Operation
**          Cache operation to be performed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_NODE_CPUCacheOperation(
    IN gcsSURF_NODE_PTR Node,
    IN gceSURF_TYPE Type,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Length,
    IN gceCACHEOPERATION Operation
    )
{

    gceSTATUS status;
    gctPOINTER memory;
    gctBOOL locked = gcvFALSE;

    gcmHEADER_ARG("Node=0x%x, Type=%u, Offset=%u, Length=%u, Operation=%d", Node, Type, Offset, Length, Operation);

    /* Lock the node. */
    gcmONERROR(gcoHARDWARE_Lock(Node, gcvNULL, &memory));
    locked = gcvTRUE;

    gcmONERROR(gcoSURF_NODE_Cache(Node,
                                  (gctUINT8_PTR)memory + Offset,
                                  Length,
                                  Operation));


    /* Unlock the node. */
    gcmONERROR(gcoHARDWARE_Unlock(Node, Type));
    locked = gcvFALSE;

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    if (locked)
    {
        gcmVERIFY_OK(gcoHARDWARE_Unlock(Node, Type));
    }

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_LockNode(
                 IN gcsSURF_NODE_PTR Node,
                 OUT gctUINT32 * Address,
                 OUT gctPOINTER * Memory
                 )
{
    gceSTATUS status;

    gcmHEADER_ARG("Node=0x%x, Address=0x%x, Memory=0x%x", Node, Address, Memory);

    gcmONERROR(gcoHARDWARE_Lock(Node, Address, Memory));

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_UnLockNode(
                   IN gcsSURF_NODE_PTR Node,
                   IN gceSURF_TYPE Type
                   )
{
    gceSTATUS status;

    gcmHEADER_ARG("Node=0x%x, Type=%u", Node, Type);

    gcmONERROR(gcoHARDWARE_Unlock(Node, Type));

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_CPUCacheOperation
**
**  Perform the specified CPU cache operation on the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_CPU_CACHE_OP_TYPE Operation
**          Cache operation to be performed.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_CPUCacheOperation(
    IN gcoSURF Surface,
    IN gceCACHEOPERATION Operation
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER source[3] = {0};
    gctBOOL locked = gcvFALSE;

    gcmHEADER_ARG("Surface=0x%x, Operation=%d", Surface, Operation);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Lock the surfaces. */
    gcmONERROR(gcoSURF_Lock(Surface, gcvNULL, source));
    locked = gcvTRUE;

    gcmONERROR(gcoSURF_NODE_Cache(&Surface->info.node,
                                  source[0],
                                  Surface->info.node.size,
                                  Operation));

    /* Unlock the surfaces. */
    gcmONERROR(gcoSURF_Unlock(Surface, source[0]));
    locked = gcvFALSE;

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    if (locked)
    {
        gcmVERIFY_OK(gcoSURF_Unlock(Surface, source[0]));
    }

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_Flush
**
**  Flush the caches to make sure the surface has all pixels.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_Flush(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Flush the current pipe. */
    status = gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL);
    gcmFOOTER();
    return status;
}

#if gcdENABLE_3D
/*******************************************************************************
**
**  gcoSURF_FillFromTile
**
**  Fill the surface from the information in the tile status buffer.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_FillFromTile(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_TILE_FILLER)
    &&  (Surface->info.type == gcvSURF_RENDER_TARGET)
    &&  (Surface->info.samples.x == 1) && (Surface->info.samples.y == 1)
    &&  (Surface->info.compressed == gcvFALSE)
    &&  (Surface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    &&  (Surface->info.tileStatusDisabled == gcvFALSE)
    &&  ((Surface->info.node.size & 0x3fff) == 0))
    {
        /*
         * Call underlying tile status disable to do FC fill:
         * 1. Flush pipe / tile status cache
         * 2. Decompress (Fill) tile status
         * 3. Set surface tileStatusDisabled to true.
         */
        gcmONERROR(
            gcoHARDWARE_DisableTileStatus(gcvNULL,
                                          &Surface->info,
                                          gcvTRUE));
    }
    else
    if ((Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN)
    ||  (Surface->info.tileStatusDisabled == gcvTRUE))
    {
        /* Flush pipe cache. */
        gcmONERROR(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));

        /*
         * No need to fill if tile status disabled.
         * Return OK here to tell the caller that FC fill is done, because
         * the caller(drivers) may be unable to know it.
         */
        status = gcvSTATUS_OK;
    }
    else
    {
        /* Set return value. */
        status = gcvSTATUS_NOT_SUPPORTED;
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#if gcdENABLE_3D || gcdENABLE_VG
/*******************************************************************************
**
**  gcoSURF_SetSamples
**
**  Set the number of samples per pixel.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gctUINT Samples
**          Number of samples per pixel.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetSamples(
    IN gcoSURF Surface,
    IN gctUINT Samples
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctINT width = 0, height = 0;
#if gcdENABLE_3D
    gcePATCH_ID patchId = gcvPATCH_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x Samples=%u", Surface, Samples);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if gcdENABLE_3D
    gcoHARDWARE_GetPatchID(gcvNULL, &patchId);
    if (patchId == gcvPATCH_ANTUTU || patchId == gcvPATCH_ANTUTU4X)
    {
        Samples = 0;
    }
#endif

    /* Make sure this is not user-allocated surface. */
    if (Surface->info.node.pool == gcvPOOL_USER)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    switch (Samples)
    {
    case 16: /* VAA sampling. */
        if (gcoHAL_IsFeatureAvailable(gcvNULL,
                                      gcvFEATURE_VAA) == gcvSTATUS_TRUE)
        {
            gctBOOL valid;

            if (Surface->info.type == gcvSURF_RENDER_TARGET)
            {
                /* VAA is only supported on RGB888 surfaces. */
                switch (Surface->info.format)
                {
                case gcvSURF_X8R8G8B8:
                case gcvSURF_A8R8G8B8:
                case gcvSURF_R8_1_X8R8G8B8:
                case gcvSURF_G8R8_1_X8R8G8B8:
                    /* Render targets need 2 samples per pixel. */
                    width = (Surface->info.samples.x != 2)
                          ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.right,
                                                        Surface->info.samples.x) * 2
                          : Surface->info.rect.right;

                    Surface->info.samples.x = 2;

                    valid = gcvTRUE;
                    break;

                default:
                    valid = gcvFALSE;
                    break;
                }
            }
            else
            {
                /* Any other target needs 1 sample per pixel. */
                width = (Surface->info.samples.x != 1)
                      ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.right,
                                                    Surface->info.samples.x)
                      : Surface->info.rect.right;

                Surface->info.samples.x = 1;
                valid = gcvTRUE;
            }

            if (valid)
            {
                /* Normal height. */
                height = (Surface->info.samples.y != 1)
                       ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.bottom,
                                                     Surface->info.samples.y)
                       : Surface->info.rect.bottom;

                Surface->info.samples.y = 1;
                Surface->info.vaa       = (Surface->info.samples.x == 2);
                break;
            }
        }
        /* Fall through to 4x MSAA. */

        /* fall through */
    case 4: /* 2x2 sampling. */
        width  = (Surface->info.samples.x != 2)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.right,
                                             Surface->info.samples.x) * 2
               : Surface->info.rect.right;
        height = (Surface->info.samples.y != 2)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.bottom,
                                             Surface->info.samples.y) * 2
               : Surface->info.rect.bottom;

        Surface->info.samples.x = 2;
        Surface->info.samples.y = 2;
        Surface->info.vaa       = gcvFALSE;
        break;

    case 2: /* 2x1 sampling. */
        width  = (Surface->info.samples.x != 2)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.right,
                                             Surface->info.samples.x) * 2
               : Surface->info.rect.right;
        height = (Surface->info.samples.y != 1)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.bottom,
                                             Surface->info.samples.y)
               : Surface->info.rect.bottom;

        Surface->info.samples.x = 2;
        Surface->info.samples.y = 1;
        Surface->info.vaa       = gcvFALSE;
        break;

    case 0:
    case 1: /* 1x1 sampling. */
        width  = (Surface->info.samples.x != 1)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.right,
                                             Surface->info.samples.x)
               : Surface->info.rect.right;
        height = (Surface->info.samples.y != 1)
               ? (gctINT) gcoMATH_DivideUInt(Surface->info.rect.bottom,
                                             Surface->info.samples.y)
               : Surface->info.rect.bottom;

        Surface->info.samples.x = 1;
        Surface->info.samples.y = 1;
        Surface->info.vaa       = gcvFALSE;
        break;

    default:
        /* Invalid multi-sample request. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    if ((Surface->info.rect.right  != width)
    ||  (Surface->info.rect.bottom != height)
    )
    {
        gceSURF_TYPE type = Surface->info.type;

        /* TODO: Shall we add hints bits? */
        type = (gceSURF_FORMAT) (type | Surface->info.hints);

#if gcdENABLE_3D
        if ((Surface->info.tileStatusNode.pool == gcvPOOL_UNKNOWN) &&
            !(Surface->info.hints & gcvSURF_NO_VIDMEM))
        {
            type |= gcvSURF_NO_TILE_STATUS;
        }
#endif

        /* Destroy existing surface memory. */
        gcmONERROR(_FreeSurface(Surface));

        /* Allocate new surface. */
        gcmONERROR(
            _AllocateSurface(Surface,
                             width, height, Surface->depth,
                             type,
                             Surface->info.format,
                             gcvPOOL_DEFAULT));
    }

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_GetSamples
**
**  Get the number of samples per pixel.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      gctUINT_PTR Samples
**          Pointer to variable receiving the number of samples per pixel.
**
*/
gceSTATUS
gcoSURF_GetSamples(
    IN gcoSURF Surface,
    OUT gctUINT_PTR Samples
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(Samples != gcvNULL);

    /* Return samples. */
    *Samples = Surface->info.samples.x * Surface->info.samples.y;

    /* Test for VAA. */
    if (Surface->info.vaa)
    {
        gcmASSERT(*Samples == 2);

        /* Setup to 16 samples for VAA. */
        *Samples = 16;
    }

    /* Success. */
    gcmFOOTER_ARG("*Samples=%u", *Samples);
    return gcvSTATUS_OK;
}
#endif

/*******************************************************************************
**
**  gcoSURF_SetResolvability
**
**  Set the resolvability of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gctBOOL Resolvable
**          gcvTRUE if the surface is resolvable or gcvFALSE if not.  This is
**          required for alignment purposes.
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_SetResolvability(
    IN gcoSURF Surface,
    IN gctBOOL Resolvable
    )
{
#if gcdENABLE_3D
    gcmHEADER_ARG("Surface=0x%x Resolvable=%d", Surface, Resolvable);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Set the resolvability. */
    Surface->resolvable = Resolvable;

    /* Success. */
    gcmFOOTER_NO();
#endif
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_SetOrientation
**
**  Set the orientation of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceORIENTATION Orientation
**          The requested surface orientation.  Orientation can be one of the
**          following values:
**
**              gcvORIENTATION_TOP_BOTTOM - Surface is from top to bottom.
**              gcvORIENTATION_BOTTOM_TOP - Surface is from bottom to top.
**
**  OUTPUT:
**
*/
gceSTATUS
gcoSURF_SetOrientation(
    IN gcoSURF Surface,
    IN gceORIENTATION Orientation
    )
{
    gcmHEADER_ARG("Surface=0x%x Orientation=%d", Surface, Orientation);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if !gcdREMOVE_SURF_ORIENTATION
    /* Set the orientation. */
    Surface->info.orientation = Orientation;

#endif
    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_QueryOrientation
**
**  Query the orientation of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      gceORIENTATION * Orientation
**          Pointer to a variable receiving the surface orientation.
**
*/
gceSTATUS
gcoSURF_QueryOrientation(
    IN gcoSURF Surface,
    OUT gceORIENTATION * Orientation
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(Orientation != gcvNULL);

    /* Return the orientation. */
    *Orientation = Surface->info.orientation;

    /* Success. */
    gcmFOOTER_ARG("*Orientation=%d", *Orientation);
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_QueryFlags
**
**  Query status of the flag.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to surface object.
**      gceSURF_FLAG Flag
**          Flag which is queried
**
**  OUTPUT:
**      None
**
*/
gceSTATUS
gcoSURF_QueryFlags(
    IN gcoSURF Surface,
    IN gceSURF_FLAG Flag
    )
{
    gceSTATUS status = gcvSTATUS_TRUE;
    gcmHEADER_ARG("Surface=0x%x Flag=0x%x", Surface, Flag);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if(Surface->info.flags & Flag)
    {
        status = gcvSTATUS_TRUE;
    }
    else
    {
        status = gcvSTATUS_FALSE;
    }
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_QueryFormat
**
**  Return pixel format parameters.
**
**  INPUT:
**
**      gceSURF_FORMAT Format
**          API format.
**
**  OUTPUT:
**
**      gcsSURF_FORMAT_INFO_PTR * Info
**          Pointer to a variable that will hold the format description entry.
**          If the format in question is interleaved, two pointers will be
**          returned stored in an array fashion.
**
*/
gceSTATUS
gcoSURF_QueryFormat(
    IN gceSURF_FORMAT Format,
    OUT gcsSURF_FORMAT_INFO_PTR * Info
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Format=%d", Format);

    status = gcoHARDWARE_QueryFormat(Format, Info);

    gcmFOOTER();
    return status;
}



/*******************************************************************************
**
**  gcoSURF_SetColorType
**
**  Set the color type of the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_COLOR_TYPE colorType
**          color type of the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetColorType(
    IN gcoSURF Surface,
    IN gceSURF_COLOR_TYPE ColorType
    )
{
    gcmHEADER_ARG("Surface=0x%x ColorType=%d", Surface, ColorType);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Set the color type. */
    Surface->info.colorType = ColorType;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_GetColorType
**
**  Get the color type of the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      gceSURF_COLOR_TYPE *colorType
**          pointer to the variable receiving color type of the surface.
**
*/
gceSTATUS
gcoSURF_GetColorType(
    IN gcoSURF Surface,
    OUT gceSURF_COLOR_TYPE *ColorType
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ColorType != gcvNULL);

    /* Return the color type. */
    *ColorType = Surface->info.colorType;

    /* Success. */
    gcmFOOTER_ARG("*ColorType=%d", *ColorType);
    return gcvSTATUS_OK;
}




/*******************************************************************************
**
**  gcoSURF_SetColorSpace
**
**  Set the color type of the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_COLOR_SPACE ColorSpace
**          color space of the surface.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetColorSpace(
    IN gcoSURF Surface,
    IN gceSURF_COLOR_SPACE ColorSpace
    )
{
    gcmHEADER_ARG("Surface=0x%x ColorSpace=%d", Surface, ColorSpace);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Set the color space. */
    Surface->info.colorSpace = ColorSpace;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_GetColorSpace
**
**  Get the color space of the surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**  OUTPUT:
**
**      gceSURF_COLOR_SPACE *ColorSpace
**          pointer to the variable receiving color space of the surface.
**
*/
gceSTATUS
gcoSURF_GetColorSpace(
    IN gcoSURF Surface,
    OUT gceSURF_COLOR_SPACE *ColorSpace
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_ARGUMENT(ColorSpace != gcvNULL);

    /* Return the color type. */
    *ColorSpace = Surface->info.colorSpace;

    /* Success. */
    gcmFOOTER_ARG("*ColorSpace=%d", *ColorSpace);
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gcoSURF_SetRotation
**
**  Set the surface ration angle.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_ROTATION Rotation
**          Rotation angle.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetRotation(
    IN gcoSURF Surface,
    IN gceSURF_ROTATION Rotation
    )
{
    gcmHEADER_ARG("Surface=0x%x Rotation=%d", Surface, Rotation);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Support only 2D surfaces. */
    if (Surface->info.type != gcvSURF_BITMAP)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Set new rotation. */
    Surface->info.rotation = Rotation;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_SetDither
**
**  Set the surface dither flag.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the surface.
**
**      gceSURF_ROTATION Dither
**          dither enable or not.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetDither(
    IN gcoSURF Surface,
    IN gctBOOL Dither
    )
{
    gcmHEADER_ARG("Surface=0x%x Dither=%d", Surface, Dither);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Support only 2D surfaces. */
    if (Surface->info.type != gcvSURF_BITMAP)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* Set new rotation. */
    Surface->info.dither2D = Dither;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

#if gcdENABLE_3D || gcdENABLE_VG
/*******************************************************************************
**
**  gcoSURF_Copy
**
**  Copy one tiled surface to another tiled surfaces.  This is used for handling
**  unaligned surfaces.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to the gcoSURF object that describes the surface to copy
**          into.
**
**      gcoSURF Source
**          Pointer to the gcoSURF object that describes the source surface to
**          copy from.
**
**  OUTPUT:
**
**      Nothing.
**
*/
gceSTATUS
gcoSURF_Copy(
    IN gcoSURF Surface,
    IN gcoSURF Source
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctUINT8_PTR source = gcvNULL, target = gcvNULL;

#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif

    gcmHEADER_ARG("Surface=0x%x Source=0x%x", Surface, Source);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(Source, gcvOBJ_SURF);


    if ((Surface->info.tiling != Source->info.tiling) ||
        ((Surface->info.tiling != gcvTILED) &&
         (Surface->info.tiling != gcvSUPERTILED)
        )
       )
    {
        /* Both surfaces need to the same tiled.
        ** only tile and supertile are supported.
        */
        gcmFOOTER();
        return gcvSTATUS_INVALID_REQUEST;
    }

    do
    {
        gctUINT y;
        gctUINT sourceOffset, targetOffset;
        gctINT height = 0;
        gctPOINTER pointer[3] = { gcvNULL };

#if gcdENABLE_VG
        gcmGETCURRENTHARDWARE(currentType);
        if (currentType == gcvHARDWARE_VG)
        {
            /* Flush the pipe. */
            gcmERR_BREAK(gcoVGHARDWARE_FlushPipe(gcvNULL));

            /* Commit and stall the pipe. */
            gcmERR_BREAK(gcoHAL_Commit(gcvNULL, gcvTRUE));

            /* Get the tile height. */
            gcmERR_BREAK(gcoVGHARDWARE_QueryTileSize(
                                                   gcvNULL, gcvNULL,
                                                   gcvNULL, &height,
                                                   gcvNULL));
        }
        else
#endif
        {
            /* Flush the pipe. */
            gcmERR_BREAK(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));

            /* Commit and stall the pipe. */
            gcmERR_BREAK(gcoHAL_Commit(gcvNULL, gcvTRUE));

            switch (Surface->info.tiling)
            {
            case gcvTILED:
                /* Get the tile height. */
                gcmERR_BREAK(gcoHARDWARE_QueryTileSize(gcvNULL, gcvNULL,
                                                       gcvNULL, &height,
                                                       gcvNULL));
                break;
            case gcvSUPERTILED:
                height = 64;
                break;
            default:
                gcmASSERT(0);
                height = 4;
                break;
            }
        }
        /* Lock the surfaces. */
        gcmERR_BREAK(gcoSURF_Lock(Source, gcvNULL, pointer));

        source = pointer[0];

        gcmERR_BREAK(gcoSURF_Lock(Surface, gcvNULL, pointer));

        target = pointer[0];

        /* Reset initial offsets. */
        sourceOffset = 0;
        targetOffset = 0;

        /* Loop target surface, one row of tiles at a time. */
        for (y = 0; y < Surface->info.alignedHeight; y += height)
        {
            /* Copy one row of tiles. */
            gcoOS_MemCopy(target + targetOffset,
                          source + sourceOffset,
                          Surface->info.stride * height);

            /* Move to next row of tiles. */
            sourceOffset += Source->info.stride  * height;
            targetOffset += Surface->info.stride * height;
        }
    }
    while (gcvFALSE);

    if (source != gcvNULL)
    {
        /* Unlock source surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(Source, source));
    }

    if (target != gcvNULL)
    {
        /* Unlock target surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(Surface, target));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif

#if gcdENABLE_3D
gceSTATUS
gcoSURF_IsHWResolveable(
    IN gcoSURF SrcSurface,
    IN gcoSURF DestSurface,
    IN gcsPOINT_PTR SrcOrigin,
    IN gcsPOINT_PTR DestOrigin,
    IN gcsPOINT_PTR RectSize
    )
{
    gceSTATUS status;
    gcsPOINT  rectSize;
    gctINT maxWidth;
    gctINT maxHeight;

    gcmHEADER_ARG("SrcSurface=0x%x DestSurface=0x%x SrcOrigin=0x%x "
                  "DestOrigin=0x%x RectSize=0x%x",
                  SrcSurface, DestSurface, SrcOrigin, DestOrigin, RectSize);

    if ((DestOrigin->x == 0) &&
        (DestOrigin->y == 0) &&
        (RectSize->x == DestSurface->info.rect.right) &&
        (RectSize->y == DestSurface->info.rect.bottom))
    {
        /* Full destination resolve, a special case. */
        rectSize.x = DestSurface->info.alignedWidth;
        rectSize.y = DestSurface->info.alignedHeight;
    }
    else
    {
        rectSize.x = RectSize->x;
        rectSize.y = RectSize->y;
    }

    /* Make sure we don't go beyond the source surface. */
    maxWidth  = SrcSurface->info.alignedWidth  - SrcOrigin->x;
    maxHeight = SrcSurface->info.alignedHeight - SrcOrigin->y;

    rectSize.x = gcmMIN(maxWidth,  rectSize.x);
    rectSize.y = gcmMIN(maxHeight, rectSize.y);

    /* Make sure we don't go beyond the target surface. */
    maxWidth  = DestSurface->info.alignedWidth  - DestOrigin->x;
    maxHeight = DestSurface->info.alignedHeight - DestOrigin->y;

    rectSize.x = gcmMIN(maxWidth,  rectSize.x);
    rectSize.y = gcmMIN(maxHeight, rectSize.y);

    if ((SrcSurface->info.type == gcvSURF_DEPTH)
    &&  (SrcSurface->info.tileStatusNode.pool != gcvPOOL_UNKNOWN)
    )
    {
        status = gcvSTATUS_FALSE;
    }
    else
    {
         status = gcoHARDWARE_IsHWResolveable(&SrcSurface->info,
                                              &DestSurface->info,
                                              SrcOrigin,
                                              DestOrigin,
                                              &rectSize);
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}
#endif /* gcdENABLE_3D */

/*******************************************************************************
**
**  gcoSURF_ConstructWrapper
**
**  Create a new gcoSURF wrapper object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gcoSURF * Surface
**          Pointer to the variable that will hold the gcoSURF object pointer.
*/
gceSTATUS
gcoSURF_ConstructWrapper(
    IN gcoHAL Hal,
    OUT gcoSURF * Surface
    )
{
    gcoSURF surface;
    gceSTATUS status;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER();

    /* Verify the arguments. */
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    do
    {
        /* Allocate the gcoSURF object. */
        gcmONERROR(gcoOS_Allocate(
            gcvNULL,
            gcmSIZEOF(struct _gcoSURF),
            &pointer
            ));

        surface = pointer;

        /* Reset the object. */
        gcoOS_ZeroMemory(surface, gcmSIZEOF(struct _gcoSURF));

        /* Initialize the gcoSURF object.*/
        surface->object.type = gcvOBJ_SURF;

#if gcdENABLE_3D
        /* 1 sample per pixel. */
        surface->info.samples.x = 1;
        surface->info.samples.y = 1;
#endif

        /* One plane. */
        surface->depth = 1;

        /* Initialize the node. */
        surface->info.node.pool      = gcvPOOL_USER;
        surface->info.node.physical  = ~0U;
        surface->info.node.physical2 = ~0U;
        surface->info.node.physical3 = ~0U;
        surface->info.node.count     = 1;
        surface->referenceCount = 1;

        surface->info.flags = gcvSURF_FLAG_NONE;

        /* Return pointer to the gcoSURF object. */
        *Surface = surface;

        /* Success. */
        gcmFOOTER_ARG("*Surface=0x%x", *Surface);
        return gcvSTATUS_OK;
    }
    while (gcvFALSE);

OnError:

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_SetFlags
**
**  Set status of the flag.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to surface object.
**      gceSURF_FLAG Flag
**          Surface Flag
**      gctBOOL Value
**          New value for this flag.
**
**  OUTPUT:
**      None
**
*/
gceSTATUS
gcoSURF_SetFlags(
    IN gcoSURF Surface,
    IN gceSURF_FLAG Flag,
    IN gctBOOL Value
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Surface=0x%x Flag=0x%x Value=0x%x", Surface, Flag, Value);

    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    if(Value)
    {
        Surface->info.flags |= Flag;
    }
    else
    {
        Surface->info.flags &= ~Flag;
    }

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_SetBuffer
**
**  Set the underlying buffer for the surface wrapper.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**      gceSURF_TYPE Type
**          Type of surface to create.
**
**      gceSURF_FORMAT Format
**          Format of surface to create.
**
**      gctINT Stride
**          Surface stride. Is set to ~0 the stride will be autocomputed.
**
**      gctPOINTER Logical
**          Logical pointer to the user allocated surface or gcvNULL if no
**          logical pointer has been provided.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetBuffer(
    IN gcoSURF Surface,
    IN gceSURF_TYPE Type,
    IN gceSURF_FORMAT Format,
    IN gctUINT Stride,
    IN gctPOINTER Logical,
    IN gctUINT32 Physical
    )
{
    gceSTATUS status;
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentType = gcvHARDWARE_INVALID;
#endif
    gcsSURF_FORMAT_INFO_PTR surfInfo;

    gcmHEADER_ARG("Surface=0x%x Type=%d Format=%d Stride=%u Logical=0x%x "
                  "Physical=%08x",
                  Surface, Type, Format, Stride, Logical, Physical);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Has to be user-allocated surface. */
    if (Surface->info.node.pool != gcvPOOL_USER)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Unmap the current buffer if any. */
    gcmONERROR(_UnmapUserBuffer(Surface, gcvTRUE));

    /* Determine the stride parameters. */
    Surface->autoStride = (Stride == ~0U);

    /* Set surface parameters. */
    Surface->info.type   = Type;
    Surface->info.format = Format;
    Surface->info.stride = Stride;

    /* Set node pointers. */
    Surface->logical  = (gctUINT8_PTR) Logical;
    Surface->physical = Physical;

#if gcdENABLE_VG
    gcmGETCURRENTHARDWARE(currentType);
    if (currentType == gcvHARDWARE_VG)
    {
        /* Compute bits per pixel. */
        gcmONERROR(gcoVGHARDWARE_ConvertFormat(gcvNULL,
                                             Format,
                                             (gctUINT32_PTR)&Surface->info.bitsPerPixel,
                                             gcvNULL));
    }
    else
#endif
    {
        /* Compute bits per pixel. */
        gcmONERROR(gcoHARDWARE_ConvertFormat(Format,
                                             (gctUINT32_PTR)&Surface->info.bitsPerPixel,
                                             gcvNULL));
    }

    /* Initialize Surface->info.formatInfo */
    gcmONERROR(gcoSURF_QueryFormat(Format, &surfInfo));
    Surface->info.formatInfo = *surfInfo;

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
**  gcoSURF_SetVideoBuffer
**
**  Set the underlying video buffer for the surface wrapper.
**  The video plane addresses should be specified individually.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**      gceSURF_TYPE Type
**          Type of surface to create.
**
**      gceSURF_FORMAT Format
**          Format of surface to create.
**
**      gctINT Stride
**          Surface stride. Is set to ~0 the stride will be autocomputed.
**
**      gctPOINTER LogicalPlane1
**          Logical pointer to the first plane of the user allocated surface
**          or gcvNULL if no logical pointer has been provided.
**
**      gctUINT32 PhysicalPlane1
**          Physical pointer to the user allocated surface or ~0 if no
**          physical pointer has been provided.
**
**  OUTPUT:
**
**      Nothing.
*/

/*******************************************************************************
**
**  gcoSURF_SetWindow
**
**  Set the size of the surface in pixels and map the underlying buffer set by
**  gcoSURF_SetBuffer if necessary.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**      gctINT X, Y
**          The origin of the surface.
**
**      gctINT Width, Height
**          Size of the surface in pixels.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetWindow(
    IN gcoSURF Surface,
    IN gctUINT X,
    IN gctUINT Y,
    IN gctUINT Width,
    IN gctUINT Height
    )
{
    gceSTATUS status;
    gctUINT32 offsetX;
    gctUINT32 offsetY;
    gctUINT   userStride;
    gctUINT   halStride;
    gctINT    bytesPerPixel;
#if gcdSECURE_USER
    gcsHAL_INTERFACE iface;
#endif
#if gcdENABLE_VG
    gceHARDWARE_TYPE currentHW = gcvHARDWARE_INVALID;
#endif
    gcsSURF_FORMAT_INFO_PTR formatInfo;

    gcmHEADER_ARG("Surface=0x%x X=%u Y=%u Width=%u Height=%u",
                  Surface, X, Y, Width, Height);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Unmap the current buffer if any. */
    gcmONERROR(
        _UnmapUserBuffer(Surface, gcvTRUE));

    /* Make sure at least one of the surface pointers is set. */
    if ((Surface->logical == gcvNULL) && (Surface->physical == ~0U))
    {
        gcmONERROR(gcvSTATUS_INVALID_ADDRESS);
    }

    /* Set initial aligned width and height. */
    Surface->info.alignedWidth  = Width;
    Surface->info.alignedHeight = Height;

    /* Stride is the same as the width? */
    if (Surface->autoStride)
    {
        /* Compute the stride. */
        Surface->info.stride = Width * Surface->info.bitsPerPixel / 8;
    }
    else
    {
#if gcdENABLE_VG
        gcmGETCURRENTHARDWARE(currentHW);
        if (currentHW == gcvHARDWARE_VG)
        {
            gcmONERROR(
                gcoVGHARDWARE_AlignToTile(gcvNULL,
                                          Surface->info.type,
                                          &Surface->info.alignedWidth,
                                          &Surface->info.alignedHeight));
        }
        else
#endif
        {
            /* Align the surface size. */
#if gcdENABLE_3D
            gcmONERROR(
                gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                  Surface->info.type,
                                                  Surface->info.format,
                                                  &Surface->info.alignedWidth,
                                                  &Surface->info.alignedHeight,
                                                  Surface->depth,
                                                  &Surface->info.tiling,
                                                  &Surface->info.superTiled));
#else
            gcmONERROR(
                gcoHARDWARE_AlignToTileCompatible(gcvNULL,
                                                  Surface->info.type,
                                                  Surface->info.format,
                                                  &Surface->info.alignedWidth,
                                                  &Surface->info.alignedHeight,
                                                  Surface->depth,
                                                  &Surface->info.tiling,
                                                  gcvNULL));
#endif /* gcdENABLE_3D */
        }
    }

#if gcdENABLE_VG
    if (currentHW != gcvHARDWARE_VG)
    {
#endif

    /* Get shortcut. */
    formatInfo = &Surface->info.formatInfo;

    /* bytes per pixel of first plane. */
    switch (Surface->info.format)
    {
    case gcvSURF_YV12:
    case gcvSURF_I420:
    case gcvSURF_NV12:
    case gcvSURF_NV21:
    case gcvSURF_NV16:
    case gcvSURF_NV61:
        bytesPerPixel = 1;
        halStride = Surface->info.alignedWidth;
        break;

    default:
        bytesPerPixel = Surface->info.bitsPerPixel / 8;
        halStride = (Surface->info.alignedWidth / formatInfo->blockWidth)
                  * (formatInfo->blockSize / formatInfo->layers) / 8;
        break;
    }

    /* Backup user stride. */
    userStride = Surface->info.stride;

    if (userStride != halStride)
    {
        if ((Surface->info.type != gcvSURF_BITMAP)
        ||  (Surface->info.stride < Width * bytesPerPixel)
        ||  (Surface->info.stride & (4 * bytesPerPixel - 1))
        )
        {
            /*
             * 1. For Vivante internal surfaces types, user buffer placement
             * must be the same as what defined in HAL, otherwise the user
             * buffer is not compatible with HAL.
             * 2. For bitmap surface, user buffer stride may be larger than
             * least stride defined in HAL, and should be aligned.
             */
            gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
        }

        /* Calculte alignedWidth from user stride. */
        Surface->info.alignedWidth = userStride / bytesPerPixel;
    }

    /* Compute the surface placement parameters. */
    _ComputeSurfacePlacement(Surface);

    if (userStride != Surface->info.stride)
    {
        /*
         * Still not equal, which means user stride is not pixel aligned, ie,
         * userStride != alignedWidth(user) * bytesPerPixel
         */
        Surface->info.stride = userStride;

        /* Re-calculate slice size. */
        Surface->info.sliceSize = userStride * (Surface->info.alignedHeight / formatInfo->blockHeight);
    }

    /* Restore alignedWidth. */
    Surface->info.alignedWidth = halStride / bytesPerPixel;

#if gcdENABLE_VG
    }
#endif

    /* Set the rectangle. */
    Surface->info.rect.left   = 0;
    Surface->info.rect.top    = 0;
    Surface->info.rect.right  = Width;
    Surface->info.rect.bottom = Height;

    offsetX = X * Surface->info.bitsPerPixel / 8;
    offsetY = Y * Surface->info.stride;

    /* Compute the surface sliceSize and size. */
    Surface->info.sliceSize = (Surface->info.alignedWidth  / Surface->info.formatInfo.blockWidth)
                            * (Surface->info.alignedHeight / Surface->info.formatInfo.blockHeight)
                            * Surface->info.formatInfo.blockSize / 8;

    Surface->info.layerSize = Surface->info.sliceSize
                            * Surface->depth;

    /* Always single layer for user surface */
    gcmASSERT(Surface->info.formatInfo.layers == 1);

    Surface->info.size      = Surface->info.layerSize
                            * Surface->info.formatInfo.layers;

    /* Need to map logical pointer? */
    if (Surface->logical == gcvNULL)
    {
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }
    else
    {
        Surface->info.node.logical = (gctUINT8_PTR) Surface->logical + offsetY;
    }

    /* Need to map physical pointer? */
#if gcdPROCESS_ADDRESS_SPACE
    if (1)
#else
    if (Surface->physical == ~0U)
#endif
    {
        /* Map the physical pointer. */
        gcmONERROR(
            gcoOS_MapUserMemory(gcvNULL,
                                (gctUINT8_PTR) Surface->logical + offsetY,
                                Surface->info.size,
                                &Surface->info.node.u.wrapped.mappingInfo,
                                &Surface->info.node.physical));

        gcmVERIFY_OK(gcoHAL_GetHardwareType(gcvNULL,
                                            &Surface->info.node.u.wrapped.mappingHardwareType));
    }
    else
    {
        Surface->info.node.physical = Surface->physical + offsetY;
    }

    Surface->info.node.logical  += offsetX;
    Surface->info.node.physical += offsetX;

#if gcdSECURE_USER
    iface.command = gcvHAL_MAP_USER_MEMORY;
    iface.u.MapUserMemory.memory  = gcmPTR_TO_UINT64(Surface->info.node.logical);
    iface.u.MapUserMemory.address = Surface->info.node.physical;
    iface.u.MapUserMemory.size    = Surface->info.size;
    gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                   IOCTL_GCHAL_INTERFACE,
                                   &iface, gcmSIZEOF(iface),
                                   &iface, gcmSIZEOF(iface)));

    Surface->info.node.physical = (gctUINT32) Surface->info.node.logical;
#endif

    /* Validate the node. */
    Surface->info.node.lockCount = 1;
    Surface->info.node.valid     = gcvTRUE;

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
**  gcoSURF_SetAlignment
**
**  Set the alignment width/height of the surface and calculate stride/size.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**      gctINT Width, Height
**          Size of the surface in pixels.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_SetAlignment(
    IN gcoSURF Surface,
    IN gctUINT Width,
    IN gctUINT Height
    )
{
    gcmHEADER_ARG("Surface=0x%x Width=%u Height=%u", Surface, Width, Height);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    Surface->info.alignedWidth = Width;
    Surface->info.alignedHeight = Height;

    /* Compute the surface stride. */
    Surface->info.stride = Surface->info.alignedWidth
                           * Surface->info.bitsPerPixel / 8;

    /* Compute the surface size. */
    Surface->info.size
        = Surface->info.stride
        * Surface->info.alignedHeight;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoSURF_ReferenceSurface
**
**  Increase reference count of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_ReferenceSurface(
    IN gcoSURF Surface
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    Surface->referenceCount++;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}


/*******************************************************************************
**
**  gcoSURF_QueryReferenceCount
**
**  Query reference count of a surface
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      gctINT32 ReferenceCount
**          Reference count of a surface
*/
gceSTATUS
gcoSURF_QueryReferenceCount(
    IN gcoSURF Surface,
    OUT gctINT32 * ReferenceCount
    )
{
    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    *ReferenceCount = Surface->referenceCount;

    gcmFOOTER_ARG("*ReferenceCount=%d", *ReferenceCount);
    return gcvSTATUS_OK;
}

gceSTATUS
gcoSURF_SetOffset(
    IN gcoSURF Surface,
    IN gctSIZE_T Offset
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmSAFECASTSIZET(Surface->info.offset, Offset);

    return status;
}

#if gcdENABLE_3D
gceSTATUS
gcoSURF_IsRenderable(
    IN gcoSURF Surface
    )
{
    gceSTATUS   status;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Check whether the surface is renderable. */
    status = gcoHARDWARE_QuerySurfaceRenderable(gcvNULL, &Surface->info);

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_IsFormatRenderableAsRT(
    IN gcoSURF Surface
    )
{
    gceSTATUS               status;
    gceSURF_FORMAT          format;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

    /* Check whether the surface format is renderable when
       Tex bind to fbo. */
    format = Surface->info.format;

    if(format >= 700)
        status = gcvSTATUS_FALSE;
    else
        status = gcvSTATUS_TRUE;

    /* Return status. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoSURF_Swap(
    IN gcoSURF Surface1,
    IN gcoSURF Surface2
    )
{
    struct _gcoSURF temp;

    gcmHEADER_ARG("Surface1=%p Surface2=%p", Surface1, Surface2);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Surface1, gcvOBJ_SURF);
    gcmVERIFY_OBJECT(Surface2, gcvOBJ_SURF);

    /* Swap the surfaces. */
    temp      = *Surface1;
    *Surface1 = *Surface2;
    *Surface2 = temp;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

#define gcdCOLOR_SPACE_CONVERSION_NONE         0
#define gcdCOLOR_SPACE_CONVERSION_TO_LINEAR    1
#define gcdCOLOR_SPACE_CONVERSION_TO_NONLINEAR 2

static
gceSTATUS
_AveragePixels(
    gcsPIXEL *pixels,
    gctINT pixelCount,
    gceFORMAT_DATATYPE inputFormat,
    gcsPIXEL *outPixel
    )
{
    gctINT i;
    gcsPIXEL mergePixel;

    gcoOS_ZeroMemory(&mergePixel, sizeof(gcsPIXEL));

    switch(inputFormat)
    {
    case gcvFORMAT_DATATYPE_UNSIGNED_INTEGER:
        for (i = 0; i < pixelCount; i++)
        {
            mergePixel.pui.r += pixels[i].pui.r;
            mergePixel.pui.g += pixels[i].pui.g;
            mergePixel.pui.b += pixels[i].pui.b;
            mergePixel.pui.a += pixels[i].pui.a;
            mergePixel.pui.d += pixels[i].pui.d;
            mergePixel.pui.s += pixels[i].pui.s;
        }

        mergePixel.pui.r /= pixelCount;
        mergePixel.pui.g /= pixelCount;
        mergePixel.pui.b /= pixelCount;
        mergePixel.pui.a /= pixelCount;
        mergePixel.pui.d /= pixelCount;
        mergePixel.pui.s /= pixelCount;
        break;

    case gcvFORMAT_DATATYPE_SIGNED_INTEGER:
        for (i = 0; i < pixelCount; i++)
        {
            mergePixel.pi.r += pixels[i].pi.r;
            mergePixel.pi.g += pixels[i].pi.g;
            mergePixel.pi.b += pixels[i].pi.b;
            mergePixel.pi.a += pixels[i].pi.a;
            mergePixel.pi.d += pixels[i].pi.d;
            mergePixel.pi.s += pixels[i].pi.s;
        }

        mergePixel.pi.r /= pixelCount;
        mergePixel.pi.g /= pixelCount;
        mergePixel.pi.b /= pixelCount;
        mergePixel.pi.a /= pixelCount;
        mergePixel.pi.d /= pixelCount;
        mergePixel.pi.s /= pixelCount;

        break;
    default:
        for (i = 0; i < pixelCount; i++)
        {
            mergePixel.pf.r += pixels[i].pf.r;
            mergePixel.pf.g += pixels[i].pf.g;
            mergePixel.pf.b += pixels[i].pf.b;
            mergePixel.pf.a += pixels[i].pf.a;
            mergePixel.pf.d += pixels[i].pf.d;
            mergePixel.pf.s += pixels[i].pf.s;
        }

        mergePixel.pf.r /= pixelCount;
        mergePixel.pf.g /= pixelCount;
        mergePixel.pf.b /= pixelCount;
        mergePixel.pf.a /= pixelCount;
        mergePixel.pf.d /= pixelCount;
        mergePixel.pf.s /= pixelCount;
        break;
    }

    *outPixel = mergePixel;

    return gcvSTATUS_OK;
}

gceSTATUS
gcoSURF_BlitCPU(
    IN gcsSURF_BLIT_ARGS* args
    )
{
    gceSTATUS status;
    gcoSURF srcSurf, dstSurf;
    gctPOINTER srcAddr[3] = {gcvNULL};
    gctPOINTER dstAddr[3] = {gcvNULL};
    _PFNreadPixel pfReadPixel = gcvNULL;
    _PFNwritePixel pfWritePixel = gcvNULL;
    _PFNcalcPixelAddr pfSrcCalcAddr = gcvNULL;
    _PFNcalcPixelAddr pfDstCalcAddr = gcvNULL;
    gctFLOAT xScale, yScale, zScale;
    gctINT iDst, jDst, kDst;
    gctINT colorSpaceConvert = gcdCOLOR_SPACE_CONVERSION_NONE;
    gcsSURF_BLIT_ARGS blitArgs;
    gctINT scissorHeight = 0;
    gctINT scissorWidth = 0;

    gcsSURF_FORMAT_INFO *srcFmtInfo, *dstFmtInfo;

    gcoOS_MemCopy(&blitArgs, args, sizeof(gcsSURF_BLIT_ARGS));

    srcSurf = args->srcSurface;
    dstSurf = args->dstSurface;

    /* MSAA surface should multiple samples.*/
    blitArgs.srcWidth  *= srcSurf->info.samples.x;
    blitArgs.srcHeight *= srcSurf->info.samples.y;
    blitArgs.srcX      *= srcSurf->info.samples.x;
    blitArgs.srcY      *= srcSurf->info.samples.y;

    blitArgs.dstWidth  *= dstSurf->info.samples.x;
    blitArgs.dstHeight *= dstSurf->info.samples.y;
    blitArgs.dstX      *= dstSurf->info.samples.x;
    blitArgs.dstY      *= dstSurf->info.samples.y;

    if(blitArgs.scissorTest)
    {
        scissorHeight = blitArgs.scissor.bottom - blitArgs.scissor.top;
        scissorWidth  = blitArgs.scissor.right - blitArgs.scissor.left;
        blitArgs.scissor.top    *= dstSurf->info.samples.y;
        blitArgs.scissor.left   *= dstSurf->info.samples.x;
        blitArgs.scissor.bottom  = scissorHeight * dstSurf->info.samples.y + blitArgs.scissor.top;
        blitArgs.scissor.right   = scissorWidth * dstSurf->info.samples.x + blitArgs.scissor.left;
    }

    if (!args || !args->srcSurface || !args->dstSurface)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if (args->srcSurface == args->dstSurface)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* If either the src or dst rect are negative */
    if (0 > blitArgs.srcWidth || 0 > blitArgs.srcHeight || 0 > blitArgs.srcDepth ||
        0 > blitArgs.dstWidth || 0 > blitArgs.dstHeight || 0 > blitArgs.dstDepth)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* If either the src or dst rect are zero, skip the blit */
    if (0 == blitArgs.srcWidth || 0 == blitArgs.srcHeight || 0 == blitArgs.srcDepth ||
        0 == blitArgs.dstWidth || 0 == blitArgs.dstHeight || 0 == blitArgs.dstDepth)
    {
        return gcvSTATUS_OK;
    }

    srcFmtInfo = &srcSurf->info.formatInfo;
    dstFmtInfo = &dstSurf->info.formatInfo;

    /* If either the src or dst rect has no intersection with the surface, skip the blit. */
    if (blitArgs.srcX + blitArgs.srcWidth  <= 0 || blitArgs.srcX >= (gctINT)(srcSurf->info.alignedWidth)  ||
        blitArgs.srcY + blitArgs.srcHeight <= 0 || blitArgs.srcY >= (gctINT)(srcSurf->info.alignedHeight) ||
        blitArgs.srcZ + blitArgs.srcDepth  <= 0 || blitArgs.srcZ >= (gctINT)(srcSurf->depth)              ||
        blitArgs.dstX + blitArgs.dstWidth  <= 0 || blitArgs.dstX >= (gctINT)(dstSurf->info.alignedWidth)  ||
        blitArgs.dstY + blitArgs.dstHeight <= 0 || blitArgs.dstY >= (gctINT)(dstSurf->info.alignedHeight) ||
        blitArgs.dstZ + blitArgs.dstDepth  <= 0 || blitArgs.dstZ >= (gctINT)(dstSurf->depth))
    {
        return gcvSTATUS_OK;
    }

    /* Propagate canDropStencil flag to the destination surface */
    dstSurf->info.canDropStencilPlane = srcSurf->info.canDropStencilPlane;

    if (dstSurf->info.hzNode.valid)
    {
        /* Disable any HZ attached to destination. */
        dstSurf->info.hzDisabled = gcvTRUE;
    }

    /*
    ** For integer format upload/blit, the data type must be totally matched.
    ** And we should not do conversion to float per spec, or precision will be lost.
    */
    if (((srcFmtInfo->fmtDataType == gcvFORMAT_DATATYPE_UNSIGNED_INTEGER) ||
         (srcFmtInfo->fmtDataType == gcvFORMAT_DATATYPE_SIGNED_INTEGER)) &&
         (srcFmtInfo->fmtDataType != dstFmtInfo->fmtDataType))
    {
        return gcvSTATUS_NOT_SUPPORTED;
    }

    /* TODO: those function pointers can be recorded in gcoSURF */
    pfReadPixel  = gcoSURF_GetReadPixelFunc(srcSurf);
    pfWritePixel = gcoSURF_GetWritePixelFunc(dstSurf);
    pfSrcCalcAddr = gcoHARDWARE_GetProcCalcPixelAddr(gcvNULL, srcSurf);
    pfDstCalcAddr = gcoHARDWARE_GetProcCalcPixelAddr(gcvNULL, dstSurf);

    /* set color space conversion flag */
    if (srcSurf->info.colorSpace != dstSurf->info.colorSpace)
    {
        gcmASSERT(dstSurf->info.colorSpace != gcvSURF_COLOR_SPACE_UNKNOWN);

        if (srcSurf->info.colorSpace == gcvSURF_COLOR_SPACE_LINEAR)
        {
            colorSpaceConvert = gcdCOLOR_SPACE_CONVERSION_TO_NONLINEAR;
        }
        else if (srcSurf->info.colorSpace == gcvSURF_COLOR_SPACE_NONLINEAR)
        {
            colorSpaceConvert = gcdCOLOR_SPACE_CONVERSION_TO_LINEAR;
        }
        else
        {
            /* color space should NOT be gcvSURF_COLOR_SPACE_UNKNOWN */
            gcmASSERT(0);
        }
    }

    if (!pfReadPixel || !pfWritePixel || !pfSrcCalcAddr || !pfDstCalcAddr)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Flush the GPU cache */
    gcmONERROR(gcoHARDWARE_FlushTileStatus(gcvNULL, &srcSurf->info, gcvTRUE));
    gcmONERROR(gcoHARDWARE_DisableTileStatus(gcvNULL, &dstSurf->info, gcvTRUE));

    /* Synchronize with the GPU. */
    /* TODO: if both of the surfaces previously were not write by GPU,
    ** or already did the sync, no need to do it again.
    */
    gcmONERROR(gcoHARDWARE_FlushPipe(gcvNULL, gcvNULL));
    gcmONERROR(gcoHARDWARE_Commit(gcvNULL));
    gcmONERROR(gcoHARDWARE_Stall(gcvNULL));

    /* Lock the surfaces. */
    gcmONERROR(gcoSURF_Lock(srcSurf, gcvNULL, srcAddr));
    gcmONERROR(gcoSURF_Lock(dstSurf, gcvNULL, dstAddr));

    /* Src surface might be written by GPU previously, CPU need to invalidate
    ** its cache before reading.
    ** Dst surface alo need invalidate CPU cache to guarantee CPU cache is coherent
    ** with memory, so it's correct to flush out after writing.
    */
    gcmONERROR(gcoSURF_NODE_Cache(&srcSurf->info.node,
                                  srcAddr[0],
                                  srcSurf->info.size,
                                  gcvCACHE_INVALIDATE));
    gcmONERROR(gcoSURF_NODE_Cache(&dstSurf->info.node,
                                  dstAddr[0],
                                  dstSurf->info.size,
                                  gcvCACHE_INVALIDATE));

    xScale = blitArgs.srcWidth  / (gctFLOAT)blitArgs.dstWidth;
    yScale = blitArgs.srcHeight / (gctFLOAT)blitArgs.dstHeight;
    zScale = blitArgs.srcDepth  / (gctFLOAT)blitArgs.dstDepth;

    for (kDst = blitArgs.dstZ; kDst < blitArgs.dstZ + blitArgs.dstDepth; ++kDst)
    {
        gctINT kSrc;
        gctINT paceI, paceJ, paceK;
        gctINT paceImax, paceJmax, paceKmax;

        if (kDst < 0 || kDst >= (gctINT)dstSurf->depth)
        {
            continue;
        }

        kSrc = blitArgs.srcZ + (gctINT)((kDst - blitArgs.dstZ) * zScale);

        if (kSrc < 0 || kSrc >= (gctINT)srcSurf->depth)
        {
            continue;
        }

        for (jDst = blitArgs.dstY; jDst < blitArgs.dstY + blitArgs.dstHeight; ++jDst)
        {
            gctINT jSrc;

            if (jDst < 0 || jDst >= (gctINT)dstSurf->info.alignedHeight)
            {
                continue;
            }

            /* scissor test */
            if (blitArgs.scissorTest &&
                (jDst < blitArgs.scissor.top || jDst >= blitArgs.scissor.bottom))
            {
                continue;
            }

            jSrc = (gctINT)((jDst - blitArgs.dstY) * yScale);
            if (jSrc > blitArgs.srcHeight - 1)
            {
                jSrc = blitArgs.srcHeight - 1;
            }
            if (blitArgs.yReverse)
            {
                jSrc = blitArgs.srcHeight - 1 - jSrc;
            }
            jSrc += blitArgs.srcY;

            if (jSrc < 0 || jSrc >= (gctINT)srcSurf->info.alignedHeight)
            {
                continue;
            }

            for (iDst = blitArgs.dstX; iDst < blitArgs.dstX + blitArgs.dstWidth; ++iDst)
            {
                gcsPIXEL internal;
                gcsPIXEL samplePixels[32];
                gctUINT sampleCount       = 0;
                gctPOINTER srcAddr_l[4]   = {gcvNULL};
                gctPOINTER dstAddr_l[4]   = {gcvNULL};
                gctINT     iSrc;

                if (iDst < 0 || iDst >= (gctINT)dstSurf->info.alignedWidth)
                {
                    continue;
                }

                /* scissor test */
                if (blitArgs.scissorTest &&
                    (iDst < blitArgs.scissor.left || iDst >= blitArgs.scissor.right))
                {
                    continue;
                }

                iSrc = (gctINT)((iDst - blitArgs.dstX) * xScale);
                if (iSrc > blitArgs.srcWidth - 1)
                {
                    iSrc = blitArgs.srcWidth - 1;
                }
                if (blitArgs.xReverse)
                {
                    iSrc = blitArgs.srcWidth - 1 - iSrc;
                }
                iSrc += blitArgs.srcX;

                if (iSrc < 0 || iSrc >= (gctINT)srcSurf->info.alignedWidth)
                {
                    continue;
                }

                paceImax = (gctINT)(xScale + 0.5f) > 1 ? (gctINT)(xScale + 0.5f) : 1;
                paceJmax = (gctINT)(yScale + 0.5f) > 1 ? (gctINT)(yScale + 0.5f) : 1;
                paceKmax = (gctINT)(zScale + 0.5f) > 1 ? (gctINT)(zScale + 0.5f) : 1;

                for (paceK = 0; paceK < paceKmax; paceK++)
                {
                    for(paceJ = 0; paceJ < paceJmax; paceJ++)
                    {
                        for(paceI = 0; paceI < paceImax; paceI++)

                        {
                            gctINT sampleI = iSrc + paceI * (blitArgs.xReverse ? -1 : 1);
                            gctINT sampleJ = jSrc + paceJ * (blitArgs.yReverse ? -1 : 1);
                            gctINT sampleK = kSrc + paceK;

                            sampleI = gcmCLAMP(sampleI, 0, (gctINT)(srcSurf->info.alignedWidth - 1));
                            sampleJ = gcmCLAMP(sampleJ, 0, (gctINT)(srcSurf->info.alignedHeight - 1));
                            sampleK = gcmCLAMP(sampleK, 0, (gctINT)(srcSurf->depth - 1));

                            pfSrcCalcAddr(srcSurf, (gctSIZE_T)sampleI, (gctSIZE_T)sampleJ, (gctSIZE_T)sampleK, srcAddr_l);

                            pfReadPixel(srcAddr_l, &samplePixels[sampleCount]);

                            if (colorSpaceConvert == gcdCOLOR_SPACE_CONVERSION_TO_LINEAR)
                            {
                                gcoSURF_PixelToLinear(&samplePixels[sampleCount]);
                            }
                            else if (colorSpaceConvert == gcdCOLOR_SPACE_CONVERSION_TO_NONLINEAR)
                            {
                                gcoSURF_PixelToNonLinear(&samplePixels[sampleCount]);

                            }
                            sampleCount++;
                        }
                    }
                }

                if (sampleCount > 1)
                {
                    _AveragePixels(samplePixels, sampleCount, srcFmtInfo->fmtDataType, &internal);
                }
                else
                {
                    internal = samplePixels[0];
                }

                pfDstCalcAddr(dstSurf, (gctSIZE_T)iDst, (gctSIZE_T)jDst, (gctSIZE_T)kDst, dstAddr_l);

                pfWritePixel(&internal, dstAddr_l, args->flags);

                /* TODO: If they are of same type, we can skip some conversion */
            }
        }
    }

    /* Dst surface was written by CPU and might be accessed by GPU later */
    gcmONERROR(gcoSURF_NODE_Cache(&dstSurf->info.node,
                                  dstAddr[0],
                                  dstSurf->info.size,
                                  gcvCACHE_CLEAN));

    if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PE_DITHER_FIX) == gcvFALSE &&
        args->flags == 0 && /* Full mask overwritten */
        args->dstX == 0 && args->dstWidth  >= dstSurf->info.rect.right  / dstSurf->info.samples.x &&
        args->dstY == 0 && args->dstHeight >= dstSurf->info.rect.bottom / dstSurf->info.samples.y)
    {
        dstSurf->info.deferDither3D = gcvFALSE;
    }

    if (dstSurf->info.paddingFormat &&
        args->dstX == 0 && args->dstWidth  >= dstSurf->info.rect.right  / dstSurf->info.samples.x &&
        args->dstY == 0 && args->dstHeight >= dstSurf->info.rect.bottom / dstSurf->info.samples.y)
    {
        dstSurf->info.garbagePadded = gcvFALSE;
    }

#if gcdDUMP
    /* verify the source */
    gcmDUMP_BUFFER(gcvNULL,
                   "verify",
                   srcSurf->info.node.physical,
                   srcSurf->info.node.logical,
                   0,
                   srcSurf->info.size);
    /* upload the destination */
    gcmDUMP_BUFFER(gcvNULL,
                   "memory",
                   dstSurf->info.node.physical,
                   dstSurf->info.node.logical,
                   0,
                   dstSurf->info.size);
#endif

OnError:
    /* Unlock the surfaces. */
    if (srcAddr[0])
    {
        gcoSURF_Unlock(srcSurf, srcAddr[0]);
    }
    if (dstAddr[0])
    {
        gcoSURF_Unlock(dstSurf, dstAddr[0]);
    }

    return gcvSTATUS_OK;
}

gceSTATUS
gcoSURF_BlitDraw(
    IN gcsSURF_BLITDRAW_ARGS *args
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("args=0x%x", args);

    status = gcoHARDWARE_BlitDraw(args);

    gcmFOOTER_NO();
    return status;

}

gceSTATUS
gcoSURF_Preserve(
    IN gcoSURF Source,
    IN gcoSURF Dest,
    IN gcsRECT_PTR MaskRect
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsRECT rects[4];
    gctINT  count = 0;
    gctINT  width, height;
    gctUINT resolveAlignmentX = 0;
    gctUINT resolveAlignmentY = 0;
    gctUINT tileAlignmentX    = 0;
    gctUINT tileAlignmentY    = 0;

    gcmHEADER_ARG("Source=0x%x Dest=0x%x MaskRect=0x%x",
                  Source, Dest,
                  MaskRect);

    gcmASSERT(!(Dest->info.flags & gcvSURF_FLAG_CONTENT_UPDATED));

    /* Get surface size. */
    width  = Dest->info.rect.right  / Dest->info.samples.x;
    height = Dest->info.rect.bottom / Dest->info.samples.y;

    if ((MaskRect != gcvNULL) &&
        (MaskRect->left   <= 0) &&
        (MaskRect->top    <= 0) &&
        (MaskRect->right  >= (gctINT) width) &&
        (MaskRect->bottom >= (gctINT) height))
    {
        gcmFOOTER_NO();
        /* Full screen clear. No copy. */
        return gcvSTATUS_OK;
    }

    /* Query surface resolve alignment parameters. */
    gcmONERROR(
        gcoHARDWARE_GetSurfaceResolveAlignment(gcvNULL,
                                               &Dest->info,
                                               &tileAlignmentX,
                                               &tileAlignmentY,
                                               &resolveAlignmentX,
                                               &resolveAlignmentY));

    if ((MaskRect == gcvNULL) ||
        (MaskRect->left == MaskRect->right) ||
        (MaskRect->top  == MaskRect->bottom))
    {
        /* Zarro clear rect, need copy full surface. */
        rects[0].left   = 0;
        rects[0].top    = 0;
        rects[0].right  = gcmALIGN(width,  resolveAlignmentX);
        rects[0].bottom = gcmALIGN(height, resolveAlignmentY);
        count = 1;
    }
    else
    {
        gcsRECT maskRect;

        if (Dest->info.flags & gcvSURF_FLAG_CONTENT_YINVERTED)
        {
            /* Y inverted content. */
            maskRect.left   = MaskRect->left;
            maskRect.top    = height - MaskRect->bottom;
            maskRect.right  = MaskRect->right;
            maskRect.bottom = height - MaskRect->top;
        }
        else
        {
            maskRect = *MaskRect;
        }

        /* Avoid right,bottom coordinate exceeding surface boundary. */
        if (tileAlignmentX < resolveAlignmentX)
        {
            tileAlignmentX = resolveAlignmentX;
        }

        if (tileAlignmentY < resolveAlignmentY)
        {
            tileAlignmentY = resolveAlignmentY;
        }

        /*
         *  +------------------------+
         *  |                        |
         *  |           r1           |
         *  |                        |
         *  |......+----------+......|
         *  |      |          |      |
         *  |  r0  |  mask    |  r2  |
         *  |      |   rect   |      |
         *  |......+----------+......|
         *  |                        |
         *  |           r3           |
         *  |                        |
         *  +------------------------+
         */

        /* Get real size of clear  */
        if (maskRect.left > 0)
        {
            rects[count].left   = 0;
            rects[count].top    = gcmALIGN_BASE(maskRect.top, tileAlignmentY);
            rects[count].right  = gcmALIGN(maskRect.left, resolveAlignmentX);
            rects[count].bottom = rects[count].top + gcmALIGN(maskRect.bottom - rects[count].top, resolveAlignmentY);
            count++;
        }

        if (maskRect.top > 0)
        {
            rects[count].left   = 0;
            rects[count].top    = 0;
            rects[count].right  = gcmALIGN(width, resolveAlignmentX);
            rects[count].bottom = gcmALIGN(maskRect.top, resolveAlignmentY);
            count++;
        }

        if (maskRect.right < width)
        {
            rects[count].left   = gcmALIGN_BASE(maskRect.right, tileAlignmentX);
            rects[count].top    = gcmALIGN_BASE(maskRect.top,   tileAlignmentY);
            rects[count].right  = rects[count].left + gcmALIGN(width - rects[count].left, resolveAlignmentX);
            rects[count].bottom = rects[count].top  + gcmALIGN(maskRect.bottom - rects[count].top, resolveAlignmentY);
            count++;
        }

        if (maskRect.bottom < height)
        {
            rects[count].left   = 0;
            rects[count].top    = gcmALIGN_BASE(maskRect.bottom, tileAlignmentY);
            rects[count].right  = gcmALIGN(width, resolveAlignmentX);
            rects[count].bottom = rects[count].top + gcmALIGN(height - rects[count].top, resolveAlignmentY);
            count++;
        }
    }

    /* Preserve calculated rects. */
    gcmONERROR(
        gcoHARDWARE_PreserveRects(gcvNULL,
                                  &Source->info,
                                  &Dest->info,
                                  rects,
                                  count));

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}
#endif

gceSTATUS
gcoSURF_ResetSurWH(
    IN gcoSURF Surface,
    IN gctUINT oriw,
    IN gctUINT orih,
    IN gctUINT alignw,
    IN gctUINT alignh,
    IN gceSURF_FORMAT fmt
)
{
    gceSTATUS status;
    Surface->info.rect.right  = oriw;
    Surface->info.rect.bottom = orih;
    Surface->info.alignedWidth = alignw;
    Surface->info.alignedHeight = alignh;

    gcmONERROR(gcoHARDWARE_ConvertFormat(
                          fmt,
                          (gctUINT32_PTR)&Surface->info.bitsPerPixel,
                          gcvNULL));

    /* Compute surface placement parameters. */
    _ComputeSurfacePlacement(Surface);

    Surface->info.layerSize = Surface->info.sliceSize * Surface->depth;

    gcmASSERT(Surface->info.formatInfo.layers == 1);

    Surface->info.size = Surface->info.layerSize * Surface->info.formatInfo.layers;

    /* Success. */
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    return status;

}

/*******************************************************************************
**
**  gcoSURF_UpdateTimeStamp
**
**  Increase timestamp of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_UpdateTimeStamp(
    IN gcoSURF Surface
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Surface=0x%X", Surface);
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    /* Increase timestamp value. */
    Surface->info.timeStamp++;

    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_UpdateTimeStamp
**
**  Query timestamp of a surface.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      gctUINT64 * TimeStamp
**          Pointer to hold the timestamp. Can not be null.
*/
gceSTATUS
gcoSURF_QueryTimeStamp(
    IN gcoSURF Surface,
    OUT gctUINT64 * TimeStamp
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Surface=0x%x", Surface);
    gcmVERIFY_ARGUMENT(Surface != gcvNULL);
    gcmVERIFY_ARGUMENT(TimeStamp != gcvNULL);

    /* Increase timestamp value. */
    *TimeStamp = Surface->info.timeStamp;

    gcmFOOTER_ARG("*TimeStamp=%lld", *TimeStamp);
    return status;
}

/*******************************************************************************
**
**  gcoSURF_AllocShBuffer
**
**  Allocate shared buffer (gctSHBUF) for this surface, so that its shared
**  states can be accessed across processes.
**
**  Shared buffer is freed when surface is destroyed.
**
**  Usage:
**  1. Process (A) constructed a surface (a) which is to be used by other
**     processes such as process (B).
**
**  2. Process (A) need alloc ShBuf (s) by gcoSURF_AllocShBuffer for surface
**     (a) if (a) need shared its states to other processes.
**
**  3. Process (B) need get surface node and other information of surface (a)
**     includes the ShBuf handle by some IPC method (such as android
**     Binder mechanism). So process (B) wrapps it as surface (b).
**
**  4. Process (B) need call gcoSURF_BindShBuffer on surface (b) with ShBuf (s)
**     to connect.
**
**  5. Processes can then call gcoSURF_PushSharedInfo/gcoSURF_PopSharedInfo to
**     shared states.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      gctSHBUF * ShBuf
**          Pointer to hold shared buffer handle.
*/
gceSTATUS
gcoSURF_AllocShBuffer(
    IN gcoSURF Surface,
    OUT gctSHBUF * ShBuf
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x ShBuf=%d",
                  Surface, (gctUINT32) (gctUINTPTR_T) ShBuf);

    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    if (Surface->info.shBuf == gcvNULL)
    {
        /* Create ShBuf. */
        gcmONERROR(
            gcoHAL_CreateShBuffer(sizeof (gcsSURF_SHARED_INFO),
                                  &Surface->info.shBuf));
    }

    /* Returns shared buffer handle. */
    *ShBuf = Surface->info.shBuf;

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_BindShBuffer
**
**  Bind surface to a shared buffer. The share buffer should be allocated by
**  gcoSURF_AllocShBuffer.
**
**  See gcoSURF_AllocShBuffer.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      gctSHBUF ShBuf
**          Pointer shared buffer handle to connect to.
*/
gceSTATUS
gcoSURF_BindShBuffer(
    IN gcoSURF Surface,
    OUT gctSHBUF ShBuf
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Surface=0x%x ShBuf=%d",
                  Surface, (gctUINT32) (gctUINTPTR_T) ShBuf);

    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    if (Surface->info.shBuf == gcvNULL)
    {
        /* Map/reference ShBuf. */
        gcmONERROR(gcoHAL_MapShBuffer(ShBuf));
        Surface->info.shBuf = ShBuf;
    }
    else
    {
        /* Already has a ShBuf. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_PushSharedInfo
**
**  Push surface shared states to shared buffer. Shared buffer should be
**  initialized before this function either by gcoSURF_AllocShBuffer or
**  gcoSURF_BindShBuffer. gcvSTATUS_NOT_SUPPORTED if not ever bound.
**
**  See gcoSURF_AllocShBuffer.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_PushSharedInfo(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;
    gcsSURF_SHARED_INFO info;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    if (Surface->info.shBuf == gcvNULL)
    {
        /* No shared buffer bound. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Gather states. */
    info.magic              = gcvSURF_SHARED_INFO_MAGIC;
    info.timeStamp          = Surface->info.timeStamp;

#if gcdENABLE_3D
    info.tileStatusDisabled = Surface->info.tileStatusDisabled;
    info.dirty              = Surface->info.dirty;
    info.fcValue            = Surface->info.fcValue;
    info.fcValueUpper       = Surface->info.fcValueUpper;
    info.compressed         = Surface->info.compressed;
#endif

    /* Put structure to shared buffer object. */
    gcmONERROR(
        gcoHAL_WriteShBuffer(Surface->info.shBuf,
                             &info,
                             sizeof (gcsSURF_SHARED_INFO)));

    /* Success. */
    gcmFOOTER_NO();
    return status;

OnError:
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoSURF_PopSharedInfo
**
**  Pop surface shared states from shared buffer. Shared buffer must be
**  initialized before this function either by gcoSURF_AllocShBuffer or
**  gcoSURF_BindShBuffer. gcvSTATUS_NOT_SUPPORTED if not ever bound.
**
**  Before sync shared states to this surface, timestamp is checked. If
**  timestamp is not newer than current, sync is discard and gcvSTATUS_SKIP
**  is returned.
**
**  See gcoSURF_AllocShBuffer.
**
**  INPUT:
**
**      gcoSURF Surface
**          Pointer to gcoSURF object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoSURF_PopSharedInfo(
    IN gcoSURF Surface
    )
{
    gceSTATUS status;
    gcsSURF_SHARED_INFO info;
    gctUINT32 size = sizeof (gcsSURF_SHARED_INFO);
    gctUINT32 bytesRead = 0;

    gcmHEADER_ARG("Surface=0x%x", Surface);

    gcmVERIFY_ARGUMENT(Surface != gcvNULL);

    if (Surface->info.shBuf == gcvNULL)
    {
        /* No shared buffer bound. */
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    /* Get structure from shared buffer object. */
    gcmONERROR(
        gcoHAL_ReadShBuffer(Surface->info.shBuf,
                            &info,
                            size,
                            &bytesRead));

    if (status == gcvSTATUS_SKIP)
    {
        /* No data in shared buffer. */
        goto OnError;
    }

    /* Check magic. */
    if ((info.magic != gcvSURF_SHARED_INFO_MAGIC) ||
        (bytesRead  != sizeof (gcsSURF_SHARED_INFO)))
    {
        /* Magic mismatch. */
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Check time stamp. */
    if (info.timeStamp <= Surface->info.timeStamp)
    {
        status = gcvSTATUS_SKIP;
        goto OnError;
    }

    /* Update surface states. */
    Surface->info.timeStamp          = info.timeStamp;

#if gcdENABLE_3D
    Surface->info.tileStatusDisabled = info.tileStatusDisabled;
    Surface->info.dirty              = info.dirty;
    Surface->info.fcValue            = info.fcValue;
    Surface->info.fcValueUpper       = info.fcValueUpper;
    Surface->info.compressed         = info.compressed;
#endif

    /* Success. */
    gcmFOOTER_NO();
    return status;

OnError:
    gcmFOOTER();
    return status;
}

gceSTATUS
gcsSURF_NODE_Construct(
    IN gcsSURF_NODE_PTR Node,
    IN gctSIZE_T Bytes,
    IN gctUINT Alignment,
    IN gceSURF_TYPE Type,
    IN gctUINT32 Flag,
    IN gcePOOL Pool
    )
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;
    struct _gcsHAL_ALLOCATE_LINEAR_VIDEO_MEMORY * alvm
        = (struct _gcsHAL_ALLOCATE_LINEAR_VIDEO_MEMORY *) &iface.u;

    gcmHEADER_ARG("Node=%p, Bytes=%llu, Alignement=%d, Type=%d, Flag=%d, Pool=%d",
                  Node, Bytes, Alignment, Type, Flag, Pool);

#ifdef LINUX
#ifndef ANDROID
#if gcdENABLE_3D
    gcePATCH_ID patchID = gcvPATCH_INVALID;
    gcoHAL_GetPatchID(gcvNULL, &patchID);

    if ( patchID == gcvPATCH_CHROME )
    {
        Flag |= gcvALLOC_FLAG_MEMLIMIT;
    }
#endif
#endif
#endif

    iface.command   = gcvHAL_ALLOCATE_LINEAR_VIDEO_MEMORY;

    gcmSAFECASTSIZET(alvm->bytes, Bytes);

    alvm->alignment = Alignment;
    alvm->type      = Type;
    alvm->pool      = Pool;
    alvm->flag      = Flag;

    gcoOS_ZeroMemory(Node, gcmSIZEOF(gcsSURF_NODE));

    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));

    Node->u.normal.node = alvm->node;
    Node->pool          = alvm->pool;
    Node->size          = alvm->bytes;

    Node->firstLock     = gcvTRUE;

    Node->physical      = ~0U;
    Node->physical2     = ~0U;
    Node->physical3     = ~0U;

#if gcdGC355_MEM_PRINT
#ifdef LINUX
    gcoOS_AddRecordAllocation((gctINT32)Node->size);
#endif
#endif

    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    gcmFOOTER();
    return status;
}

