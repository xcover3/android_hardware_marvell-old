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
**  gcoBUFFER object for user HAL layers.
**
*/

#include "gc_hal_user_precomp.h"
#include "gc_hal_user_buffer.h"

#if (gcdENABLE_3D || gcdENABLE_2D)

#define _GC_OBJ_ZONE            gcvZONE_BUFFER

#define gcmMAX_TEMPCMD_BUFFER_SIZE  0x20000
/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/

struct _gcoBUFFER
{
    /* Object. */
    gcsOBJECT                   object;

    /* Pointer to the required objects. */
    gcoHAL                      hal;

    /* Pointer to the hardware objects. */
    gcoHARDWARE                 hardware;

    /* Flag to indicate thread default or not */
    gctBOOL                     threadDefault;

    /* Handle of gckCONTEXT object. */
    gctUINT32                   context;

    /* Requested command buffer size. */
    gctSIZE_T                   bytes;

    /* Number of command buffers. */
    gctSIZE_T                   count;
    gctSIZE_T                   maxCount;

    gcsOQ *                     oq;

    /* List of command buffers. */
    gcoCMDBUF                   commandBufferList;
    gcoCMDBUF                   commandBufferHead;
    gcoCMDBUF                   commandBufferTail;

    /* Reserved bytes. */
    struct _gcsCOMMAND_INFO
    {
        gctUINT32   alignment;
        gctUINT32   reservedHead;
        gctUINT32   reservedTail;
        gceCMDBUF_SOURCE source;
    }                           info;
    gctUINT32                   totalReserved;

    /* Cache of vacant patch lists. */
    struct _gcsPATCH_LIST       *vacantPatch;

    struct _gcsTEMPCMDBUF tempCMDBUF;

};

/******************************************************************************\
******************************* Private Functions ******************************
\******************************************************************************/

static gceSTATUS
gcoCMDBUF_Free(
    IN gcoHARDWARE Hardware,
    IN gcsCOMMAND_INFO_PTR Info,
    IN gcoCMDBUF CommandBuffer
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsHAL_INTERFACE iface;

    gcmHEADER_ARG("CommandBuffer=0x%x", CommandBuffer);

    if (gcmUINT64_TO_PTR(CommandBuffer) != gcvNULL)
    {
        if (gcmUINT64_TO_PTR(CommandBuffer->logical) != gcvNULL)
        {
            /* Has the command buffer ever been committed? */
            if (CommandBuffer->commitCount == 0)
            {
                switch (Info->source)
                {
                case gcvCMDBUF_VIRTUAL:
                    iface.command = gcvHAL_FREE_VIRTUAL_COMMAND_BUFFER;
                    iface.u.FreeVirtualCommandBuffer.bytes    = CommandBuffer->bytes;
                    iface.u.FreeVirtualCommandBuffer.physical = CommandBuffer->physical;
                    iface.u.FreeVirtualCommandBuffer.logical  = CommandBuffer->logical;
                    gcmONERROR(gcoHAL_Call(gcvNULL, &iface));
                    break;
                case gcvCMDBUF_CONTIGUOUS:
                    gcmONERROR(gcoOS_FreeContiguous(
                        gcvNULL,
                        gcmINT2PTR(CommandBuffer->physical),
                        gcmUINT64_TO_PTR(CommandBuffer->logical),
                        (gctSIZE_T) CommandBuffer->bytes
                        ));
                    break;
                case gcvCMDBUF_RESERVED:
                    gcmONERROR(gcoHAL_UnlockVideoMemory(CommandBuffer->physical, gcvSURF_BITMAP));
                    gcmONERROR(gcoHAL_ReleaseVideoMemory(CommandBuffer->physical));
                    break;
                default:
                    break;
                }
            }
            else
            {
                switch (Info->source)
                {
                case gcvCMDBUF_VIRTUAL:
                    iface.command = gcvHAL_FREE_VIRTUAL_COMMAND_BUFFER;
                    iface.u.FreeVirtualCommandBuffer.bytes    = CommandBuffer->bytes;
                    iface.u.FreeVirtualCommandBuffer.physical = CommandBuffer->physical;
                    iface.u.FreeVirtualCommandBuffer.logical  = CommandBuffer->logical;

                    /* Send event. */
                    gcmONERROR(gcoHARDWARE_CallEvent(Hardware, &iface));

                    break;
                case gcvCMDBUF_CONTIGUOUS:
                    iface.command = gcvHAL_FREE_CONTIGUOUS_MEMORY;
                    iface.u.FreeContiguousMemory.bytes    = CommandBuffer->bytes;
                    iface.u.FreeContiguousMemory.physical = CommandBuffer->physical;
                    iface.u.FreeContiguousMemory.logical  = CommandBuffer->logical;

                    /* Send event. */
                    gcmONERROR(gcoHARDWARE_CallEvent(Hardware, &iface));

                    break;
                case gcvCMDBUF_RESERVED:
                    gcmONERROR(gcoHAL_UnlockVideoMemory(CommandBuffer->physical, gcvSURF_BITMAP));
                    gcmONERROR(gcoHAL_ReleaseVideoMemory(CommandBuffer->physical));
                    break;
                default:
                    break;
                }
            }

            /* Reset the buffer pointer. */
            CommandBuffer->logical = 0;
        }

#if gcdSECURE_USER
        if (gcmUINT64_TO_PTR(CommandBuffer->hintArray) != gcvNULL)
        {
            gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, gcmUINT64_TO_PTR(CommandBuffer->hintArray)));
            CommandBuffer->hintArray =
            CommandBuffer->hintArrayTail = 0;
        }
#endif
    }

    /* Success. */
    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
gcoBUFFER_GetCMDBUF(
    IN gcoBUFFER Buffer
    )
{
    gceSTATUS status;
    gcoCMDBUF commandBuffer = gcvNULL;
    gcePIPE_SELECT entryPipe;

    gcmHEADER_ARG("Buffer=0x%x", Buffer);

    /* Determine the next command buffer. */
    if (Buffer->commandBufferTail == gcvNULL)
    {
        /* Select 3D pipe for the first buffer. */
        entryPipe = gcvPIPE_3D;

        /* Get the head of the list. */
        Buffer->commandBufferHead =
        Buffer->commandBufferTail =
        commandBuffer = Buffer->commandBufferList;
    }
    else
    {
        /* Get current entry pipe. */
        entryPipe = Buffer->commandBufferTail->entryPipe;

        /* Get the next command buffer. */
        Buffer->commandBufferTail =
        commandBuffer = Buffer->commandBufferTail->next;
    }

    /* Test if command buffer is available. */
    status = gcoOS_WaitSignal(gcvNULL, commandBuffer->signal, 0);

    /* Not available? */
    if (status == gcvSTATUS_TIMEOUT)
    {
        /* Construct new command buffer. */
        if ((Buffer->maxCount == 0) || (Buffer->count < Buffer->maxCount))
        {
            gcoCMDBUF temp = gcvNULL;
            gcmONERROR(gcoCMDBUF_Construct(gcvNULL, Buffer->hardware,
                                           Buffer->bytes,
                                           (gcsCOMMAND_INFO_PTR) &Buffer->info,
                                           &temp));

            /* Insert into the list. */
            temp->prev = commandBuffer->prev;
            temp->next = commandBuffer;
            commandBuffer->prev->next = temp;
            commandBuffer->prev = temp;
            Buffer->commandBufferTail = temp;

            Buffer->count += 1;
            gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_BUFFER,
                          "Using %lu command buffers.",
                          Buffer->count);

            commandBuffer = temp;
        }

        /* Wait for buffer to become available. */
        gcmONERROR(gcoOS_WaitSignal(gcvNULL, commandBuffer->signal, gcvINFINITE));
    }
    else
    {
        gcmONERROR(status);
    }

    /* Set the entry pipe. */
    commandBuffer->entryPipe = entryPipe;

    /* Reset command buffer. */
    commandBuffer->startOffset = 0;
    commandBuffer->offset      = Buffer->info.reservedHead;
    commandBuffer->free        = commandBuffer->bytes - Buffer->totalReserved;

    /* Succees. */
    gcmFOOTER_ARG("commandBuffer=0x%x", commandBuffer);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

static gceSTATUS
gcsPATCH_Free(
    IN gcoBUFFER Buffer,
    IN struct _gcsPATCH_LIST *PatchList
    )
{
    gcmHEADER_ARG("Buffer=0x%x, PatchList=0x%x", Buffer, PatchList);

    /* Add to the cache. */
    PatchList->next = Buffer->vacantPatch;
    Buffer->vacantPatch = PatchList;

    /* Succees. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/******************************************************************************\
***************************** gcoCMDBUF Object Code *****************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoCMDBUF_Construct
**
**  Construct a new gcoCMDBUF object.
**
**  INPUT:
**
**      gcoOS Os
**          Pointer to a gcoOS object.
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctSIZE_T Bytes
**          Number of bytes for the buffer.
**
**      gcsCOMMAND_BUFFER_PTR Info
**          Alignment and head/tail information.
**
**  OUTPUT:
**
**      gcoCMDBUF * CommandBuffer
**          Pointer to a variable that will hold the the gcoCMDBUF object
**          pointer.
*/
gceSTATUS
gcoCMDBUF_Construct(
    IN gcoOS Os,
    IN gcoHARDWARE Hardware,
    IN gctSIZE_T Bytes,
    IN gcsCOMMAND_INFO_PTR Info,
    OUT gcoCMDBUF * CommandBuffer
    )
{
    gceSTATUS status;
    gcoCMDBUF commandBuffer = gcvNULL;
    gctSIZE_T objectSize    = 0;
    gctPOINTER pointer      = gcvNULL;
    gctPHYS_ADDR physical   = gcvNULL;
    gctSIZE_T tmpSize       = 0;
    gctUINT32 node          = 0;

    gcmHEADER_ARG("Bytes=%lu Info=0x%x", Bytes, Info);

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(Bytes > 0);
    gcmDEBUG_VERIFY_ARGUMENT(CommandBuffer != gcvNULL);

    /* Set the size of the object. */
    objectSize = gcmSIZEOF(struct _gcoCMDBUF);

    /* Allocate the gcoCMDBUF object. */
    /* Currently in most OS we are able to access the user-side data from
       the kernel by simple memory mapping, therefore here we allocate the
       object from the cached user memory. */
    gcmONERROR(gcoOS_AllocateSharedMemory(gcvNULL, objectSize, &pointer));
    commandBuffer = pointer;

    /* Reset the command buffer object. */
    gcoOS_ZeroMemory(commandBuffer, objectSize);

    /* Initialize the gcoCMDBUF object. */
    commandBuffer->object.type = gcvOBJ_COMMANDBUFFER;

    /* Create the signal. */
    gcmONERROR(gcoOS_CreateSignal(gcvNULL, gcvFALSE, &commandBuffer->signal));

    gcmTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_SIGNAL,
                  "%s(%d): buffer signal created 0x%08X",
                  __FUNCTION__, __LINE__, commandBuffer->signal);

    /* Mark the buffer as available. */
    gcmONERROR(gcoOS_Signal(gcvNULL, commandBuffer->signal, gcvTRUE));

    /* Try to allocate the command buffer space. */
    while (gcvTRUE)
    {
        gcsHAL_INTERFACE iface;

        /* Set the desired size. */
        gcmSAFECASTSIZET(commandBuffer->bytes, Bytes);

        /* Allocate the buffer for the command. */
        switch (Info->source)
        {
        case gcvCMDBUF_VIRTUAL:
            iface.command = gcvHAL_ALLOCATE_VIRTUAL_COMMAND_BUFFER;
            iface.u.AllocateVirtualCommandBuffer.bytes = commandBuffer->bytes;

            /* Call kernel service. */
            gcmONERROR(gcoOS_DeviceControl(gcvNULL, IOCTL_GCHAL_INTERFACE,
                                           &iface, gcmSIZEOF(iface),
                                           &iface, gcmSIZEOF(iface)));
            status = iface.status;
            if (status == gcvSTATUS_OUT_OF_MEMORY)
                goto retry;
            gcmONERROR(status);

            commandBuffer->bytes = (gctUINT32)iface.u.AllocateVirtualCommandBuffer.bytes;
            commandBuffer->physical = iface.u.AllocateVirtualCommandBuffer.physical;
            commandBuffer->logical = iface.u.AllocateVirtualCommandBuffer.logical;
            break;

        case gcvCMDBUF_CONTIGUOUS:
            tmpSize = (gctSIZE_T) commandBuffer->bytes;
            status = gcoOS_AllocateContiguous(gcvNULL, gcvTRUE,
                                              &tmpSize,
                                              &physical,
                                              &pointer);
            if (status == gcvSTATUS_OUT_OF_MEMORY)
                goto retry;
            gcmONERROR(status);

            gcmSAFECASTSIZET(commandBuffer->bytes, tmpSize);
            commandBuffer->physical = gcmPTR2INT32(physical);
            commandBuffer->logical = gcmPTR_TO_UINT64(pointer);
            break;

        case gcvCMDBUF_RESERVED:
            tmpSize = (gctSIZE_T) commandBuffer->bytes;
            status = gcoHAL_AllocateVideoMemory(
                        Info->alignment,
                        gcvSURF_BITMAP,
                        0,
                        gcvPOOL_SYSTEM,
                        &tmpSize,
                        &node);
            if (status == gcvSTATUS_OUT_OF_MEMORY)
                goto retry;
            gcmONERROR(status);

            gcmONERROR(gcoHAL_LockVideoMemory(
                node, gcvFALSE, (gctUINT32 *)&physical, &pointer));
            gcmSAFECASTSIZET(commandBuffer->bytes, tmpSize);
            commandBuffer->physical = node;
            commandBuffer->logical = gcmPTR_TO_UINT64(pointer);

            break;

        default:
            break;
        }

        /* Initialize command buffer. */
        commandBuffer->free = commandBuffer->bytes;

#if gcdSECURE_USER
        /* Determine the size of the state array. */
        commandBuffer->hintArraySize = Bytes;

        /* Allocate the state array. */
        status = gcoOS_AllocateSharedMemory(gcvNULL,
                                (gctSIZE_T) commandBuffer->hintArraySize,
                                &pointer);
        if (status == gcvSTATUS_OUT_OF_MEMORY)
            goto retry;
        gcmONERROR(status);
        commandBuffer->hintArray = gcmPTR_TO_UINT64(pointer);

        /* Initialize the state array tail pointer. */
        commandBuffer->hintArrayTail = commandBuffer->hintArray;
#endif

        /* The command buffer is successfully allocated. */
        break;

retry:
        if (Bytes <= (4 << 10))
            goto OnError;

        /* Free the command buffer objects. */
        gcmONERROR(gcoCMDBUF_Free(Hardware, Info, commandBuffer));

        /* Try lower size. */
        Bytes >>= 1;
    }

    /* Return pointer to the gcoCMDBUF object. */
    *CommandBuffer = commandBuffer;

    /* Success. */
    gcmFOOTER_ARG("*CommandBuffer=0x%x", *CommandBuffer);
    return gcvSTATUS_OK;

OnError:
    /* Roll back. */
    gcmVERIFY_OK(gcoCMDBUF_Destroy(Hardware, Info, commandBuffer));

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoCMDBUF_Destroy
**
**  Destroy a gcoCMDBUF object.
**
**  INPUT:
**
**      gcoCMDBUF CommandBuffer
**          Pointer to an gcoCMDBUF object.
**
**  OUTPUT:
**
**      None.
*/
gceSTATUS
gcoCMDBUF_Destroy(
    IN gcoHARDWARE Hardware,
    IN gcsCOMMAND_INFO_PTR Info,
    IN gcoCMDBUF CommandBuffer
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("CommandBuffer=0x%x", CommandBuffer);

    /* Verify the object. */
    gcmVERIFY_OBJECT(CommandBuffer, gcvOBJ_COMMANDBUFFER);

    /* Destroy command buffer allocations. */
    gcmONERROR(gcoCMDBUF_Free(Hardware, Info, CommandBuffer));

    /* Destroy signal. */
    if (CommandBuffer->signal != gcvNULL)
    {
        gcmONERROR(gcoOS_DestroySignal(gcvNULL, CommandBuffer->signal));
        CommandBuffer->signal = gcvNULL;
    }

    /* Free the gcoCMDBUF object. */
    gcmONERROR(gcmOS_SAFE_FREE_SHARED_MEMORY(gcvNULL, CommandBuffer));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/******************************************************************************\
******************************* gcoBUFFER API Code ******************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoBUFFER_Construct
**
**  Construct a new gcoBUFFER object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to a gcoHAL object.
**
**      gcoHARDWARE Hardware
**          Pointer to a gcoHARDWARE object.
**
**      gctUINT32 Context
**          Handle of a gckCONTEXT object.
**
**      gctSIZE_T MaxSize
**          Maximum size of buffer.
**
**      gctBOOL ThreadDefault
**          If gcvTRUE, the buffer is for thread default's hardware object
**
**  OUTPUT:
**
**      gcoBUFFER * Buffer
**          Pointer to a variable that will hold the the gcoBUFFER object
**          pointer.
*/
gceSTATUS
gcoBUFFER_Construct(
    IN gcoHAL Hal,
    IN gcoHARDWARE Hardware,
    IN gctUINT32 Context,
    IN gctSIZE_T MaxSize,
    IN gctBOOL ThreadDefault,
    OUT gcoBUFFER * Buffer
    )
{
    gceSTATUS status;
    gcoBUFFER buffer = gcvNULL;
    gctPOINTER pointer = gcvNULL;
    gcoCMDBUF commandBuffer;
    gctUINT i = 0;
    gceHARDWARE_TYPE currentType;

    gcmHEADER_ARG("Hal=0x%x Hardware=0x%x Context=0x%x MaxSize=%lu",
                  Hal, Hardware, Context, MaxSize);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Hal, gcvOBJ_HAL);
    gcmVERIFY_OBJECT(Hardware, gcvOBJ_HARDWARE);
    gcmDEBUG_VERIFY_ARGUMENT(Buffer != gcvNULL);


    /***************************************************************************
    ** Allocate and reset the gcoBUFFER object.
    */

    gcmONERROR(gcoOS_Allocate(gcvNULL, gcmSIZEOF(struct _gcoBUFFER), &pointer));
    buffer = (gcoBUFFER) pointer;

    /* Initialize the gcoBUFFER object. */
    buffer->object.type = gcvOBJ_BUFFER;
    buffer->hal         = Hal;
    buffer->context     = Context;
    buffer->threadDefault = ThreadDefault;

    /* Zero the command buffers. */
    buffer->commandBufferList = gcvNULL;
    buffer->commandBufferHead = gcvNULL;
    buffer->commandBufferTail = gcvNULL;

    /* Set the requested size. */
    buffer->bytes = MaxSize;

    /* Set the default maximum number of command buffers. */
    buffer->maxCount = gcdMAX_CMD_BUFFERS;

    buffer->oq = gcvNULL;

    /* Reset vacant patch item list. */
    buffer->vacantPatch = gcvNULL;

     gcmONERROR(gcoHAL_GetHardwareType(gcvNULL, &currentType));
     if (currentType == gcvHARDWARE_2D)
     {
         buffer->hardware = Hardware;
     }
     else
     {
         buffer->hardware = gcvNULL;
     }

    /**************************************************************************
    ** Allocate temp command buffer.
    */
    gcmONERROR(gcoOS_Allocate(gcvNULL, gcmMAX_TEMPCMD_BUFFER_SIZE, &buffer->tempCMDBUF.buffer));
    buffer->tempCMDBUF.currentByteSize = 0;
    buffer->tempCMDBUF.inUse = gcvFALSE;

    /***************************************************************************
    ** Query alignment.
    */

    gcmONERROR(gcoHARDWARE_QueryCommandBuffer(
        buffer->hardware,
        &buffer->info.alignment,
        &buffer->info.reservedHead,
        &buffer->info.reservedTail,
        &buffer->info.source
        ));

    buffer->totalReserved
        = buffer->info.reservedHead
        + buffer->info.reservedTail
        + buffer->info.alignment;


    /***************************************************************************
    ** Initialize the command buffers.
    */

    /* Construct a command buffer. */
    for (i = 0; i < gcdCMD_BUFFERS; ++i)
    {
        gcmONERROR(gcoCMDBUF_Construct(gcvNULL, buffer->hardware,
                                       MaxSize,
                                       (gcsCOMMAND_INFO_PTR) &buffer->info,
                                       &commandBuffer));

        if (buffer->commandBufferList == gcvNULL)
        {
            buffer->commandBufferList = commandBuffer;
            commandBuffer->prev =
            commandBuffer->next = commandBuffer;
        }
        else
        {
            /* Add to the tail. */
            commandBuffer->prev = buffer->commandBufferList->prev;
            commandBuffer->next = buffer->commandBufferList;
            buffer->commandBufferList->prev->next = commandBuffer;
            buffer->commandBufferList->prev = commandBuffer;
        }
    }

    /* Number of buffers initialized. */
    buffer->count = gcdCMD_BUFFERS;

    /* Get the current command buffer. */
    gcmONERROR(gcoBUFFER_GetCMDBUF(buffer));

    /* Return pointer to the gcoBUFFER object. */
    *Buffer = buffer;


    /* Success. */
    gcmFOOTER_ARG("*Buffer=0x%x", *Buffer);
    return gcvSTATUS_OK;

OnError:
    /* Roll back. */
    gcmVERIFY_OK(gcoBUFFER_Destroy(buffer));

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFFER_AttachOQ
**
**  Attach an OQ object to gcoBUFFER object.
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to an gcoBUFFER object to set state.
**
**      gctPOINTER oq
**          Pointer to OQ object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoBUFFER_AttachOQ(
    IN gcoBUFFER Buffer,
    IN gctPOINTER OQ
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Buffer=0x%x OQ=%d", Buffer, OQ);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);

    if (OQ)
    {
        gcmASSERT(!Buffer->oq);
        Buffer->oq = OQ;
        Buffer->oq->oqStatus = gcvOQ_Enable;
        Buffer->oq->oqIndex = 0;
    }
    else
    {
        Buffer->oq->oqStatus = gcvOQ_Disable;
        Buffer->oq = gcvNULL;
    }

    /* Return the status. */
    gcmFOOTER_NO();
    return status;
}


/*******************************************************************************
**
**  gcoBUFFER_Destroy
**
**  Destroy an gcoBUFFER object.
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to an gcoBUFFER object to delete.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoBUFFER_Destroy(
    IN gcoBUFFER Buffer
    )
{
    gceSTATUS status;
    gcoCMDBUF commandBufferList;
    gcoCMDBUF commandBuffer;
    struct _gcsPATCH_LIST *patchList;

    gcmHEADER_ARG("Buffer=0x%x", Buffer);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);

    /* Destroy all command buffers. */
    while (Buffer->commandBufferList != gcvNULL)
    {
        /* Get the head of the list. */
        commandBufferList = commandBuffer = Buffer->commandBufferList;

        /* Remove the head buffer from the list. */
        if (commandBufferList->next == commandBufferList)
        {
            Buffer->commandBufferList = gcvNULL;
        }
        else
        {
            Buffer->commandBufferList = commandBufferList->next;
            commandBufferList->prev->next = commandBufferList->next;
            commandBufferList->next->prev = commandBufferList->prev;
        }

        /* Free all patch list items. */
        while (commandBuffer->patchHead != gcvNULL)
        {
            /* Extract the head of the list. */
            patchList = commandBuffer->patchHead;
            commandBuffer->patchHead = patchList->next;

            /* Free the patch list item. */
            gcsPATCH_Free(Buffer, patchList);
        }

        /* Reset the tail patch. */
        commandBuffer->patchTail = gcvNULL;

        /* Destroy command buffer. */
        gcmONERROR(gcoCMDBUF_Destroy(Buffer->hardware, &Buffer->info, commandBuffer));
    }

    /* Destroy the vacant patch list. */
    while (Buffer->vacantPatch != NULL)
    {
        patchList = Buffer->vacantPatch;
        Buffer->vacantPatch = patchList->next;
        gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, patchList));
    }

    gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Buffer->tempCMDBUF.buffer));

    /* Free the object memory. */
    gcmONERROR(gcmOS_SAFE_FREE(gcvNULL, Buffer));

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFFER_Write
**
**  Copy a number of bytes into the buffer.
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to an gcoBUFFER object.
**
**      gctCONST_POINTER Data
**          Pointer to a buffer that contains the data to be copied.
**
**      IN gctSIZE_T Bytes
**          Number of bytes to copy.
**
**      IN gctBOOL Aligned
**          gcvTRUE if the data needs to be aligned to 64-bit.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoBUFFER_Write(
    IN gcoBUFFER Buffer,
    IN gctCONST_POINTER Data,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned
    )
{
    gceSTATUS status;
    gcoCMDBUF reserve;

    gcmHEADER_ARG("Buffer=0x%x Data=0x%x Bytes=%lu Aligned=%d",
                  Buffer, Data, Bytes, Aligned);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);
    gcmDEBUG_VERIFY_ARGUMENT(Data != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(Bytes > 0);

    /* Reserve data in the buffer. */
    gcmONERROR(gcoBUFFER_Reserve(Buffer, Bytes, Aligned, gcvCOMMAND_3D, &reserve));

    /* Write data into the buffer. */
    gcoOS_MemCopy(gcmUINT64_TO_PTR(reserve->lastReserve), Data, Bytes);

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFFER_Reserve
**
**  Reserve a number of bytes in the buffer.
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to an gcoBUFFER object.
**
**      gctSIZE_T Bytes
**          Number of bytes to reserve.
**
**      gctBOOL Aligned
**          gcvTRUE if the data needs to be aligned to 64-bit.
**
**  OUTPUT:
**
**      gctUINT32_PTR ** AddressHints
**          Pointer to a variable that receives the current position in the
**          state hint array.  gcvNULL is allowed.
**
**      gctPOINTER * Memory
**          Pointer to a variable that will hold the address of location in the
**          buffer that has been reserved.
*/
gceSTATUS
gcoBUFFER_Reserve(
    IN gcoBUFFER Buffer,
    IN gctSIZE_T Bytes,
    IN gctBOOL Aligned,
    IN gctUINT32 Usage,
    OUT gcoCMDBUF * Reserve
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcoCMDBUF commandBuffer;
    gctUINT32 alignBytes, bytes, offset, reserveBytes, oldBytes;
    gcsHAL_INTERFACE iface;
    gctBOOL notInSamePage = gcvFALSE;

    gcmHEADER_ARG("Buffer=0x%x Bytes=%lu Aligned=%d Reserve=0x%x",
                  Buffer, Bytes, Aligned, Reserve);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);
    gcmDEBUG_VERIFY_ARGUMENT(Reserve != gcvNULL);

    gcmSAFECASTSIZET(reserveBytes, Bytes);

    /* Get the current command buffer. */
    commandBuffer = Buffer->commandBufferTail;

    if (Buffer->threadDefault)
    {
        gcmTRACE(gcvLEVEL_INFO, "Thread Default command buffer is accumulating commands");
    }


    if (Buffer->tempCMDBUF.inUse)
    {
        gcmFATAL("!!!temp command buffer is in using, should not send command into real command buffer");
    }

    /* Compute the number of aligned bytes. */
    alignBytes = Aligned
               ? ( gcmALIGN(commandBuffer->offset, Buffer->info.alignment)
                 - commandBuffer->offset
                 )
               : 0;

    /* Compute the number of required bytes. */

    oldBytes = bytes = reserveBytes + alignBytes;

#if gcdENABLE_3D
    /* Always assume there is Begin/EndOQ command could need when reserve command buffer.
    ** The size is to make sure there are enough room.
    */
    bytes += gcmALIGN(4 * gcmSIZEOF(gctUINT32), 8);
#endif

    if (gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PIPE_2D)
     && gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PIPE_3D)
     && !gcoHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_MMU)
     && (Usage == gcvCOMMAND_2D))
    {
        offset = commandBuffer->offset + alignBytes;

        if (((offset + bytes + Buffer->info.reservedTail - 1) & ~0xFFF) != (offset & ~0xFFF))
        {
            notInSamePage = gcvTRUE;
        }
    }

    if (bytes > commandBuffer->free || notInSamePage)
    {
        /* Sent event to signal when command buffer completes. */
        iface.command            = gcvHAL_SIGNAL;
        iface.u.Signal.signal    = gcmPTR_TO_UINT64(commandBuffer->signal);
        iface.u.Signal.auxSignal = 0;
        iface.u.Signal.process   = gcmPTR_TO_UINT64(gcoOS_GetCurrentProcessID());
        iface.u.Signal.fromWhere = gcvKERNEL_COMMAND;

        /* Send event. */
        gcmONERROR(gcoHARDWARE_CallEvent(Buffer->hardware, &iface));

        /* Commit current command buffer. */
        if (commandBuffer->startOffset != commandBuffer->bytes)
        {
            gcmONERROR(gcoHARDWARE_Commit(Buffer->hardware));
        }

        /* Grab a new command buffer. */
        gcmONERROR(gcoBUFFER_GetCMDBUF(Buffer));

        /* Skip full buffers. */
        commandBuffer = Buffer->commandBufferHead;
        while (commandBuffer->startOffset == commandBuffer->bytes)
        {
            Buffer->commandBufferHead = commandBuffer = commandBuffer->next;
        }

        /* Get new buffer. */
        commandBuffer = Buffer->commandBufferTail;

        if (Bytes > commandBuffer->free)
        {
            /* This just won't fit! */
            gcmFATAL("FATAL: Command of %lu bytes is too big!", Bytes);
            gcmONERROR(gcvSTATUS_OUT_OF_MEMORY);
        }
        /* Calculate total bytes again. */
        alignBytes = 0;
        bytes      = reserveBytes;
    }
    else
    {
         bytes = oldBytes;
    }

    gcmASSERT(commandBuffer != gcvNULL);
    gcmASSERT(bytes <= commandBuffer->free);

    /* Determine the data offset. */
    offset = commandBuffer->offset + alignBytes;

    /* Update the last reserved location. */
    commandBuffer->lastReserve = gcmPTR_TO_UINT64((gctUINT8_PTR) gcmUINT64_TO_PTR(commandBuffer->logical) + offset);
    commandBuffer->lastOffset  = offset;

    /* Adjust command buffer size. */
    commandBuffer->offset += bytes;
    commandBuffer->free   -= bytes;


#if gcdENABLE_3D
   /* Resume OQ if it's there at the first reserve in the command buffer */
   if (Buffer->oq && Buffer->oq->oqStatus == gcvOQ_Paused)
   {
      gctUINT64 oqCommand;

      oqCommand = commandBuffer->lastReserve;

      /* Don't disable atomic, as we may hit the last 8 byte of cmd buffer,
         we need make sure there is room for endOQ */
      gcoHARDWARE_ResumeOQ(gcvNULL, Buffer->oq, oqCommand);
      commandBuffer->free   -= 8;
      commandBuffer->offset += 8;
      commandBuffer->lastReserve += 8;
      commandBuffer->lastOffset  += 8;

   }
#endif

    if (Usage & gcvCOMMAND_3D)
    {
        commandBuffer->using3D = gcvTRUE;
    }

    if (Usage & gcvCOMMAND_2D)
    {
        commandBuffer->using2D = gcvTRUE;
    }

    /* Set the result. */
    * Reserve = commandBuffer;

    /* Success. */
    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}


/*******************************************************************************
**
**  gcoBUFFER_Commit
**
**  Commit the command buffer to the hardware.
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to a gcoBUFFER object.
**
**      gcePIPE_SELECT CurrentPipe
**          Current graphics pipe.
**
**      gcsSTATE_DELTA_PTR StateDelta
**          Pointer to the state delta.
**
**      gcoQUEUE Queue
**          Pointer to a gcoQUEUE object.
**
**  OUTPUT:
**
**      Nothing.
*/
#if gcdDUMP
gceSTATUS
gcoBUFFER_Commit(
    IN gcoBUFFER Buffer,
    IN gcePIPE_SELECT CurrentPipe,
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gcoQUEUE Queue,
    OUT gctPOINTER *DumpLogical,
    OUT gctUINT32 *DumpBytes
    )
#else
gceSTATUS
gcoBUFFER_Commit(
    IN gcoBUFFER Buffer,
    IN gcePIPE_SELECT CurrentPipe,
    IN gcsSTATE_DELTA_PTR StateDelta,
    IN gcoQUEUE Queue
    )
#endif
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;
    gcoCMDBUF commandBuffer;
    gctBOOL tailBuffer;
    gctBOOL emptyBuffer;
    gctUINT32 newOffset;
    gctINT spaceLeft;
#if gcdMULTI_GPU
    gceCORE_3D_MASK chipEnable;
    gceMULTI_GPU_MODE mode;
#endif

    gcmHEADER_ARG("Buffer=0x%x Queue=0x%x",
                  Buffer, Queue);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);
    gcmVERIFY_OBJECT(Queue, gcvOBJ_QUEUE);

    /* Grab the head command buffer. */
    commandBuffer = Buffer->commandBufferHead;

    /* No current command buffer? */
    if (commandBuffer == gcvNULL)
    {
        gcmFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if (Buffer->threadDefault)
    {
        gcmTRACE(gcvLEVEL_INFO, "Thread Default command buffer is comitting commands");
    }

#if gcdENABLE_3D
    if (Buffer->oq && Buffer->oq->oqStatus == gcvOQ_Enable)
    {
        gctUINT alignedBytes;
        gcoCMDBUF tailCommandBuffer = Buffer->commandBufferTail;

        alignedBytes = gcmALIGN(tailCommandBuffer->offset, Buffer->info.alignment) - tailCommandBuffer->offset;

        if (tailCommandBuffer->free >= (8 + alignedBytes))
        {
            gctUINT64 oqCommand = gcmPTR_TO_UINT64((gctUINT8_PTR) gcmUINT64_TO_PTR(tailCommandBuffer->logical)
                                                    + tailCommandBuffer->offset + alignedBytes);
            /* As the command buffer always checked for room of end OQ, we always could add OQ command here */
            gcoHARDWARE_PauseOQ(gcvNULL, Buffer->oq, oqCommand);
            tailCommandBuffer->free   -= (8 + alignedBytes);
            tailCommandBuffer->offset += (8 + alignedBytes);
        }
        else
        {
            gcmPRINT("!!! OQ paused is skipped and it could be wrong");
        }
    }
#endif


    /* Loop through all buffers. */
    while (gcvTRUE)
    {
        /* Advance commit count. */
        commandBuffer->commitCount++;

        /* Determine whether this is the tail buffer. */
        tailBuffer = (commandBuffer == Buffer->commandBufferTail);

        /* Determine whether the buffer is empty. */
        emptyBuffer = (commandBuffer->offset - commandBuffer->startOffset <= Buffer->info.reservedHead);

        /* Send for execution. */
        if (emptyBuffer)
        {
            if (tailBuffer)
            {
                gcmONERROR(gcoQUEUE_Commit(Queue, gcvFALSE));
            }
        }
        else
        {
            /* Make sure the tail got aligned properly. */
            commandBuffer->offset = gcmALIGN(commandBuffer->offset, Buffer->info.alignment);

#if gcdDUMP
            *DumpLogical = (gctUINT8_PTR) gcmUINT64_TO_PTR(commandBuffer->logical)
                         + commandBuffer->startOffset
                         + Buffer->info.reservedHead;

            *DumpBytes = commandBuffer->offset
                       - commandBuffer->startOffset
                       - Buffer->info.reservedHead;
#endif

            if (gcPLS.hal->dump != gcvNULL)
            {
                /* Dump the command buffer. */
                gcmVERIFY_OK(
                    gcoDUMP_DumpData(gcPLS.hal->dump,
                                     gcvTAG_COMMAND,
                                     0,
                                     commandBuffer->offset
                                     - commandBuffer->startOffset
                                     - Buffer->info.reservedHead,
                                     (gctUINT8_PTR) gcmUINT64_TO_PTR(commandBuffer->logical)
                                     + commandBuffer->startOffset
                                     + Buffer->info.reservedHead));
            }

            /* The current pipe becomes the exit pipe for the current command buffer. */
            commandBuffer->exitPipe = CurrentPipe;

            /* Send command and context buffer to hardware. */
#if gcdENABLE_3D
            iface.u.Commit.context = (commandBuffer->using2D && !commandBuffer->using3D)
                                   ? 0 : Buffer->context;
#else
            iface.u.Commit.context = 0;
#endif

            iface.command = gcvHAL_COMMIT;
            iface.u.Commit.commandBuffer = gcmPTR_TO_UINT64(commandBuffer);
            iface.u.Commit.delta = gcmPTR_TO_UINT64(StateDelta);
            iface.u.Commit.queue = tailBuffer ? gcmPTR_TO_UINT64(Queue->head) : 0;
#if gcdMULTI_GPU
            gcmONERROR(gcoHARDWARE_GetChipEnable(gcvNULL, &chipEnable));
            gcmONERROR(gcoHARDWARE_GetMultiGPUMode(gcvNULL, &mode));

            iface.u.Commit.chipEnable = chipEnable;
            iface.u.Commit.gpuMode    = mode;
#endif

            /* Call kernel service. */
            gcmONERROR(gcoOS_DeviceControl(gcvNULL,
                                           IOCTL_GCHAL_INTERFACE,
                                           &iface, gcmSIZEOF(iface),
                                           &iface, gcmSIZEOF(iface)));
            gcmONERROR(iface.status);

            /* Advance the offset for next commit. */
            newOffset = commandBuffer->offset + Buffer->info.reservedTail;

            /* Compute the size of the space left in the buffer. */
            spaceLeft = (gctINT) (commandBuffer->bytes - newOffset);

            if (spaceLeft > (gctINT) Buffer->totalReserved)
            {
                /* Adjust buffer offset and size. */
                commandBuffer->startOffset = newOffset;
                commandBuffer->offset      = commandBuffer->startOffset + Buffer->info.reservedHead;
                commandBuffer->free        = commandBuffer->bytes - commandBuffer->offset
                                           - Buffer->info.alignment - Buffer->info.reservedTail;
            }
            else
            {
                /* Buffer is full. */
                commandBuffer->startOffset = commandBuffer->bytes;
                commandBuffer->offset      = commandBuffer->bytes;
                commandBuffer->free        = 0;
            }

            /* The exit pipe becomes the entry pipe for the next command buffer. */
            commandBuffer->entryPipe = commandBuffer->exitPipe;

#if gcdSECURE_USER
            /* Reset the state array tail. */
            commandBuffer->hintArrayTail = commandBuffer->hintArray;
#endif

            /* Reset usage flags. */
            commandBuffer->using2D         = gcvFALSE;
            commandBuffer->using3D         = gcvFALSE;
            commandBuffer->usingFilterBlit = gcvFALSE;
            commandBuffer->usingPalette    = gcvFALSE;
        }

        /* If this is the tail buffer, we are done. */
        if (tailBuffer)
        {
            /* Empty buffer must be the tail. */
            gcmONERROR(gcoQUEUE_Free(Queue));
            break;
        }

        /* Advance to the next buffer. */
        commandBuffer = Buffer->commandBufferHead = commandBuffer->next;
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
**  gcoBUFFER_StartTEMPCMDBUF
**
**  Star to use temp command buffer
**
**  INPUT:
**
**      gcoBUFFER Buffer
**          Pointer to a gcoBUFFER object
**
**
**
**  OUTPUT:
**
**      gcsTEMPCMDBUF *tempCMDBUF
**          Pointer to a variable that will hold the gcsTEMPCMDBUF
*/

gceSTATUS
gcoBUFFER_StartTEMPCMDBUF(
    IN gcoBUFFER Buffer,
    OUT gcsTEMPCMDBUF *tempCMDBUF
    )
{
    gcmHEADER_ARG("Buffer=0x%x tempCMDBUF=0x%x", Buffer, tempCMDBUF);

    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);
    gcmVERIFY_ARGUMENT(tempCMDBUF != gcvNULL);

    *tempCMDBUF = &Buffer->tempCMDBUF;

    gcmASSERT((*tempCMDBUF)->currentByteSize == 0);

    gcmASSERT((*tempCMDBUF)->inUse == gcvFALSE);

    (*tempCMDBUF)->inUse = gcvTRUE;

    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoBUFFER_EndTEMPCMDBUF
**
**  End using temp command buffer and send to real command buffer.
**
**  INPUT:
**      gcoBUFFER Buffer
**          Pointer to a gcoBUFFER object
**
**  OUTPUT:
**
**      Nothing
*/

gceSTATUS
gcoBUFFER_EndTEMPCMDBUF(
    IN gcoBUFFER Buffer
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gcsTEMPCMDBUF tempCMDBUF;
    gcmHEADER_ARG("Buffer=0x%x", Buffer);

    gcmVERIFY_OBJECT(Buffer, gcvOBJ_BUFFER);

    tempCMDBUF = &Buffer->tempCMDBUF;

    if (tempCMDBUF->currentByteSize >  gcmMAX_TEMPCMD_BUFFER_SIZE)
    {
        gcmPRINT(" Temp command buffer is overflowed!");
    }

    tempCMDBUF->inUse = gcvFALSE;


    if (tempCMDBUF->currentByteSize)
    {
        status = gcoBUFFER_Write(Buffer,
                                 tempCMDBUF->buffer,
                                 tempCMDBUF->currentByteSize,
                                 gcvTRUE);
        tempCMDBUF->currentByteSize = 0;
    }

    gcmFOOTER_NO();
    return status;
}
#endif  /* gcdENABLE_3D */
