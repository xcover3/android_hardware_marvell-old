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


#include "gc_hal_user_precomp.h"

#ifdef gcdENABLE_3D

#if gcdNULL_DRIVER < 2

#define _GC_OBJ_ZONE            gcvZONE_BUFOBJ
#define BUFFER_OBJECT_ALIGNMENT 8 /* max alignmet for all buffer object types */
#define BUFFER_INDEX_ALIGNMENT  16 /* max alignmet for index buffer object types */
#define BUFFER_OBJECT_SURFTYPE  gcvSURF_VERTEX /* TODO add surf type for buffer object */
#define DYNAMIC_BUFFER_MAX_COUNT    0x1000 /* max dynamic buffer size.*/

/******************************************************************************\
********************************** Structures **********************************
\******************************************************************************/

typedef struct _gcoBUFOBJ_INDEX
{
    gctUINT32       minIndex;
    gctUINT32       maxIndex;
    gctUINT32        count;
    gctUINT32        offset;

} gcoBUFOBJ_INDEX, * gcoBUFOBJ_INDEX_PTR;

/* gcoBUFOBJ object. */
struct _gcoBUFOBJ
{
    /* Object. */
    gcsOBJECT                   object;

    /* Number of bytes allocated for memory. */
    gctSIZE_T                   bytes;

    /* BufObj buffer node. */
    gcsSURF_NODE                memory;

    /* Type of the buffer */
    gceBUFOBJ_TYPE              type;
    gceSURF_TYPE                surfType;

    /* usage */
    gceBUFOBJ_USAGE             usage;

    /* Index management */
    gcoBUFOBJ_INDEX             indexNode;

    /* Start and end of the updated dynamic range */
    gctSIZE_T                   dynamicStart;
    gctSIZE_T                   dynamicEnd;

    /* gcdDUMP descriptor of the buffer */
    gctSTRING                   dumpDescriptor;
    gceDUMP_TAG                 dumpTag;

#if gcdSYNC
    gceFENCE_STATUS             fenceStatus;
    gcsSYNC_CONTEXT_PTR         fenceCtx;
#endif
};

/******************************************************************************\
******************************* gcoBUFOBJ API Code ******************************
\******************************************************************************/

gceSTATUS
gcoBUFOBJ_GetFence(
    IN gcoBUFOBJ bufObj
)
{
#if gcdSYNC
    if (bufObj)
    {
        gctBOOL fenceEnable;
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);
        if(fenceEnable)
        {
            gcoHARDWARE_GetFence(gcvNULL, &bufObj->fenceCtx);
            bufObj->fenceStatus = gcvFENCE_ENABLE;
        }
        else
        {
            bufObj->fenceStatus = gcvFENCE_GET;
        }
    }
#endif
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_WaitFence(
    IN gcoBUFOBJ bufObj
)
{
#if gcdSYNC
    if (bufObj)
    {
        gctBOOL fenceEnable;
        gcoHARDWARE_GetFenceEnabled(gcvNULL, &fenceEnable);
        if(fenceEnable)
        {
            gcoHARDWARE_WaitFence(gcvNULL, bufObj->fenceCtx);
        }
        else
        {
            if(bufObj->fenceStatus == gcvFENCE_GET)
            {
                bufObj->fenceStatus = gcvFENCE_ENABLE;
                gcoHARDWARE_SetFenceEnabled(gcvNULL, gcvTRUE);
                gcoHAL_Commit(gcvNULL, gcvTRUE);
            }
        }

    }
#endif
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_IsFenceEnabled(
    IN gcoBUFOBJ bufObj
    )
{
    gceSTATUS status = gcvSTATUS_NOT_SUPPORTED;
#if gcdSYNC
    if (bufObj)
    {
        return (bufObj->fenceStatus != gcvFENCE_DISABLE);
    }
#endif
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_Construct
**
**  Construct a new gcoBUFOBJ object.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**  OUTPUT:
**
**      gcoBUFOBJ * BufObj
**          Pointer to a variable that will receive the gcoBUFOBJ object pointer.
*/
gceSTATUS
gcoBUFOBJ_Construct(
    IN gcoHAL Hal,
    IN gceBUFOBJ_TYPE Type,
    OUT gcoBUFOBJ * BufObj
    )
{
    gcoOS os;
    gceSTATUS status;
    gcoBUFOBJ bufobj;
    gctPOINTER pointer = gcvNULL;

    gcmHEADER();

    /* Verify the arguments. */
    gcmDEBUG_VERIFY_ARGUMENT(BufObj != gcvNULL);

    /* Extract the gcoOS object pointer. */
    os = gcvNULL;

    /* Allocate the gcoBUFOBJ object. */
    gcmONERROR(gcoOS_Allocate(os,
                              gcmSIZEOF(struct _gcoBUFOBJ),
                              &pointer));

    bufobj = pointer;

    /* Initialize the gcoBUFOBJ object. */
    bufobj->object.type = gcvOBJ_BUFOBJ;

    /* No attributes and memory assigned yet. */
    bufobj->bytes                       = 0;
    bufobj->memory.pool                 = gcvPOOL_UNKNOWN;
    bufobj->memory.valid                = gcvFALSE;
    bufobj->memory.lockCount            = 0;
    bufobj->memory.lockedInKernel       = gcvFALSE;

    /* Generic Attributes */
    bufobj->type                        = Type;
    switch (Type)
    {
    case gcvBUFOBJ_TYPE_ARRAY_BUFFER:
        bufobj->surfType = gcvSURF_VERTEX;
        bufobj->dumpDescriptor = "stream";
        bufobj->dumpTag = gcvTAG_STREAM;
        break;
    case gcvBUFOBJ_TYPE_ELEMENT_ARRAY_BUFFER:
        bufobj->surfType = gcvSURF_INDEX;
        bufobj->dumpDescriptor = "index";
        bufobj->dumpTag = gcvTAG_INDEX;
        break;
    case gcvBUFOBJ_TYPE_GENERIC_BUFFER:
        /* We need to create a new surf type for other buffers */
        bufobj->surfType = BUFFER_OBJECT_SURFTYPE;
        bufobj->dumpDescriptor = "bufobj";
        bufobj->dumpTag = gcvTAG_BUFOBJ;
        break;
    default:
        gcmASSERT(0);
        bufobj->surfType = BUFFER_OBJECT_SURFTYPE;
        bufobj->dumpDescriptor = "bufobj";
        bufobj->dumpTag = gcvTAG_BUFOBJ;
        break;
    }

    bufobj->usage                       = gcvBUFOBJ_USAGE_STATIC_DRAW;
    bufobj->dynamicStart                = ~0U;
    bufobj->dynamicEnd                  = 0;

#if gcdSYNC
    bufobj->fenceStatus                 = gcvFENCE_DISABLE;
    bufobj->fenceCtx                    = gcvNULL;
#endif

    /* Attributes for index management */
    bufobj->indexNode.minIndex          = ~0U;
    bufobj->indexNode.maxIndex          = 0;
    bufobj->indexNode.count                = 0;
    bufobj->indexNode.offset            = 0;

    /* Return pointer to the gcoBUFOBJ object. */
    *BufObj = bufobj;

    gcmPROFILE_GC(GLBUFOBJ_OBJECT, 1);

    /* Success. */
    gcmFOOTER_ARG("*BufObj=0x%x", *BufObj);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_Destroy
**
**  Destroy a gcoBUFOBJ object.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object that needs to be destroyed.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoBUFOBJ_Destroy(
    IN gcoBUFOBJ BufObj
    )
{
    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    gcmPROFILE_GC(GLBUFOBJ_OBJECT, -1);

#if gcdSYNC
    {
        gcsSYNC_CONTEXT_PTR ptr = BufObj->fenceCtx;

        while(ptr)
        {
            BufObj->fenceCtx = ptr->next;

            gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, ptr));

            ptr = BufObj->fenceCtx;
        }
    }
#endif

    /* Free the bufobj buffer. */
    gcmVERIFY_OK(gcoBUFOBJ_Free(BufObj));

    /* Mark the gcoBUFOBJ object as unknown. */
    BufObj->object.type = gcvOBJ_UNKNOWN;

    /* Free the gcoBUFOBJ object. */
    gcmVERIFY_OK(gcmOS_SAFE_FREE(gcvNULL, BufObj));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoBUFOBJ_Lock
**
**  Lock bufobj in memory.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Physical address of the bufobj buffer.
**
**      gctPOINTER * Memory
**          Logical address of the bufobj buffer.
*/
gceSTATUS
gcoBUFOBJ_Lock(
    IN gcoBUFOBJ BufObj,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Do we have a buffer allocated? */
    if (BufObj->memory.pool == gcvPOOL_UNKNOWN)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Lock the bufobj buffer. */
    gcmONERROR(gcoHARDWARE_Lock(&BufObj->memory,
                                Address,
                                Memory));

    /* Success. */
    gcmFOOTER_ARG("*Address=0x%08x *Memory=0x%x",
                  gcmOPT_VALUE(Address), gcmOPT_POINTER(Memory));
    return status;

OnError:
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_FastLock
**
**  Lock bufobj in memory.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Physical address of the bufobj buffer.
**
**      gctPOINTER * Memory
**          Logical address of the bufobj buffer.
*/
gceSTATUS
gcoBUFOBJ_FastLock(
    IN gcoBUFOBJ BufObj,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{

    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    if (Memory != gcvNULL)
    {
        /* Return logical address. */
        *Memory = BufObj->memory.logical;
    }

    if (Address != gcvNULL)
    {
        /* Return physical address. */
        *Address = BufObj->memory.physical;
    }

    /* Success. */
    gcmFOOTER_ARG("*Address=0x%08x *Memory=0x%x",
                  gcmOPT_VALUE(Address), gcmOPT_POINTER(Memory));

    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoBUFOBJ_Unlock
**
**  Unlock bufobj that was previously locked with gcoBUFOBJ_Lock.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoBUFOBJ_Unlock(
    IN gcoBUFOBJ BufObj
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Do we have a buffer allocated? */
    if (BufObj->memory.pool == gcvPOOL_UNKNOWN)
    {
        gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
    }

    /* Unlock the bufobj buffer. */
    gcmONERROR(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));

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
**  gcoBUFOBJ_Free
**
**  Free existing bufobj buffer.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoBUFOBJ_Free(
    IN gcoBUFOBJ BufObj
    )
{
    gceSTATUS status;
    gctUINT bytes;

    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    gcmSAFECASTSIZET(bytes, BufObj->bytes);
    gcmPROFILE_GC(GLBUFOBJ_OBJECT_BYTES, -1 * bytes);

    /* Do we have a buffer allocated? */
    if (BufObj->memory.pool != gcvPOOL_UNKNOWN)
    {
        if (gcPLS.hal->dump != gcvNULL)
        {
            /* Dump deletion. */
            gcmVERIFY_OK(gcoDUMP_Delete(gcPLS.hal->dump,
                                        BufObj->memory.physical));
        }

        /* Unlock the bufobj buffer. */
        gcmONERROR(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));

        /* Create an event to free the video memory. */
        gcmONERROR(gcoHARDWARE_ScheduleVideoMemory(&BufObj->memory));

        /* Reset the pointer. */
        BufObj->bytes        = 0;
        BufObj->memory.pool  = gcvPOOL_UNKNOWN;
        BufObj->memory.valid = gcvFALSE;
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
**  gcoBUFOBJ_Upload
**
**  Upload bufobj data into an bufobj buffer.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to a gcoBUFOBJ object.
**
**      gctCONST_POINTER Buffer
**          Pointer to bufobj data to upload.
**
**      gctUINT32 Offset
**          Offset in the buffer
**
**      gctSIZE_T Bytes
**          Number of bytes to upload.
**
**      IN gceBUFOBJ_USAGE Usage usage
**          Specifies whether the stream is dynamic or static.
**          Dynamic streams will be allocated in system memory while
**          static streams will be allocated in the default memory
**          pool.
**
**        STREAM
**        The data store contents will be modified once and used at most a few times.
**
**        STATIC
**        The data store contents will be modified once and used many times.
**
**        DYNAMIC
**        The data store contents will be modified repeatedly and used many times.
**
**        The nature of access may be one of these:
**
**        DRAW
**        The data store contents are modified by the application, and used as
**        the source for GL drawing and image specification commands.
**
**        READ
**        The data store contents are modified by reading data from the GL,
**        and used to return that data when queried by the application.
**
**        COPY
**        The data store contents are modified by reading data from the GL,
**        and used as the source for GL drawing and image specification commands.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoBUFOBJ_Upload (
    IN gcoBUFOBJ BufObj,
    IN gctCONST_POINTER Buffer,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Bytes,
    IN gceBUFOBJ_USAGE Usage
    )
{
    gceSTATUS status;
    gctUINT alignment, offset, bytes;
    gcePOOL memoryPool;
    gctBOOL dynamic;
    gctBOOL allocate;
    gctBOOL duplicate;
    gctSIZE_T allocationSize;
    gctBOOL needBiggerSpace;
    gcsSURF_NODE memory;
    gctBOOL bDisableFenceAndDynamicStream;

    gcmHEADER_ARG("BufObj=0x%x Buffer=0x%x Offset=%u Bytes=%lu, Dynamic=%d",
                   BufObj, Buffer, Offset, Bytes, Usage);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);
    gcmDEBUG_VERIFY_ARGUMENT(Bytes > 0);

    /* Read useage.*/
    bDisableFenceAndDynamicStream = (Usage & gcvBUFOBJ_USAGE_DISABLE_FENCE_DYNAMIC_STREAM) ? gcvTRUE : gcvFALSE;
    Usage &= ~gcvBUFOBJ_USAGE_DISABLE_FENCE_DYNAMIC_STREAM;

    /* Dynamic for all cases other than static draw */
    dynamic = (Usage != gcvBUFOBJ_USAGE_STATIC_DRAW);

    /* Set default values */
    allocate  = gcvFALSE;
    duplicate = gcvFALSE;
    allocationSize = 0;

    /* Has it been initialized? */
    if (BufObj->memory.pool == gcvPOOL_UNKNOWN)
    {
        allocate = gcvTRUE;
        duplicate  = gcvFALSE;
        allocationSize = Offset + Bytes;
    }
    else
    {
        /* Is it resized? */
        needBiggerSpace = (BufObj->bytes < (Bytes + Offset));

        /* If resized */
        if (needBiggerSpace)
        {
            allocate = gcvTRUE;
            allocationSize = Offset + Bytes;
            /* Is incoming buffer a whole buffer? */
            if((Offset == 0) && Buffer != gcvNULL)
            {
                /* Yes, then we are going to replace it anyway. So why duplicate? */
                duplicate = gcvFALSE;
                /* Clear dynamic range. */
                BufObj->dynamicStart = ~0U;
                BufObj->dynamicEnd   = 0;
            }
            else
            {
                duplicate = gcvTRUE;
            }
        }
        else if (Offset == 0 && Bytes == BufObj->bytes)
        {
            allocate = gcvTRUE;
            allocationSize = Bytes;
            duplicate = gcvFALSE;

            /* Clear dynamic range. */
            BufObj->dynamicStart = ~0U;
            BufObj->dynamicEnd   = 0;
        }
        /* if dynamic */
        else if (dynamic)
        {
            /* If the data range overlaps for a dynamic stream,
            *  create a new buffer.
            */
            if ((Offset < BufObj->dynamicEnd)
                && ((Offset + Bytes) > BufObj->dynamicStart))
            {
                allocate  = gcvTRUE;
                allocationSize = BufObj->bytes;
                if((BufObj->bytes == Bytes) && (Offset == 0) && (Buffer != gcvNULL))
                {
                    /* We are going to override anyway. So why duplicate? */
                    duplicate = gcvFALSE;

                    /* Clear dynamic range. */
                    BufObj->dynamicStart = ~0U;
                    BufObj->dynamicEnd   = 0;
                }
                else
                {
                    duplicate = gcvTRUE;
                }
            }
        }

#if gcdSYNC
        if(!bDisableFenceAndDynamicStream)
        {
            if(BufObj->bytes > DYNAMIC_BUFFER_MAX_COUNT  &&
                duplicate                                &&
                !needBiggerSpace
                )
            {
                allocate = gcvFALSE;
                duplicate = gcvFALSE;
            }

            if (!allocate && !duplicate && !bDisableFenceAndDynamicStream)
            {
                gcoBUFOBJ_WaitFence(BufObj);
            }
        }
#endif
    }

    if(bDisableFenceAndDynamicStream)
    {
        duplicate = gcvFALSE;
    }

    /* Set new usage */
    BufObj->usage = Usage;

    /* If we need to allocate new memory */
    if (allocate)
    {
        switch (BufObj->type)
        {
        case gcvBUFOBJ_TYPE_ARRAY_BUFFER:
            /* Query the stream alignment. */
            gcmONERROR(
                gcoHARDWARE_QueryStreamCaps(gcvNULL,
                                            gcvNULL,
                                            gcvNULL,
                                            gcvNULL,
                                            &alignment));
            /* Get memory pool type */
            memoryPool = dynamic ? gcvPOOL_UNIFIED : gcvPOOL_DEFAULT;
            break;

        case gcvBUFOBJ_TYPE_ELEMENT_ARRAY_BUFFER:
            /* Get alignment */
            alignment = BUFFER_INDEX_ALIGNMENT;
            /* Get memory pool type */
            memoryPool = gcvPOOL_DEFAULT;
            break;

        case gcvBUFOBJ_TYPE_GENERIC_BUFFER:
            /* Get alignment */
            alignment = BUFFER_OBJECT_ALIGNMENT;
            /* Get memory pool type */
            memoryPool = gcvPOOL_DEFAULT;
            break;

        default:
            /* Get alignment */
            alignment = BUFFER_OBJECT_ALIGNMENT;
            /* Get memory pool type */
            memoryPool = gcvPOOL_DEFAULT;
            break;
        }

        gcmONERROR(gcsSURF_NODE_Construct(
            &memory,
            allocationSize,
            alignment,
            BufObj->surfType,
            gcvALLOC_FLAG_NONE,
            memoryPool
            ));

        /* Lock the bufobj buffer. */
        gcmONERROR(gcoHARDWARE_Lock(&memory,
                                    gcvNULL,
                                    gcvNULL));

        /* If we need to duplicate old data */
        if (duplicate)
        {
            /* Replicate the contents of old buffer in new buffer */
            gcmONERROR(gcoHARDWARE_CopyData(&memory,
                                            0,
                                            BufObj->memory.logical,
                                            BufObj->bytes));
        }

        /* Free any allocated video memory. */
        gcmONERROR(gcoBUFOBJ_Free(BufObj));

        /* Reset BufObj */
        BufObj->bytes = allocationSize;
        BufObj->memory = memory;
    }

    if (Buffer != gcvNULL)
    {
        gcmSAFECASTSIZET(offset, Offset);
        /* Upload data into the stream and clean CPU cache */
        gcmONERROR(gcoHARDWARE_CopyData(&BufObj->memory,
                                        offset,
                                        Buffer,
                                        Bytes));

        /* Flush GPU cache */
        gcmONERROR(gcoBUFOBJ_GPUCacheOperation(BufObj));

        /* Update written range for dynamic buffers. */
        if (dynamic)
        {
            if (Offset < BufObj->dynamicStart)
            {
                BufObj->dynamicStart = Offset;
            }

            if ((Offset + Bytes) > BufObj->dynamicEnd)
            {
                BufObj->dynamicEnd = Offset + Bytes;
            }
        }

        /* Dump the buffer. */
        gcmDUMP_BUFFER(gcvNULL,
                       BufObj->dumpDescriptor,
                       BufObj->memory.physical,
                       BufObj->memory.logical,
                       Offset,
                       Bytes);

        if (gcPLS.hal->dump != gcvNULL)
        {
            /* Dump the bufobj buffer. */
            gcmVERIFY_OK(gcoDUMP_DumpData(gcPLS.hal->dump,
                                          BufObj->dumpTag,
                                          BufObj->memory.physical,
                                          Bytes,
                                          Buffer));
        }
    }

    gcmSAFECASTSIZET(bytes, Bytes);
    gcmPROFILE_GC(GLBUFOBJ_OBJECT_BYTES, bytes);

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
**  gcoBUFOBJ_GetSize
**
**  Returns the size of the buffer object
**
**  INPUT:
**
**      ggcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      gctSIZE_T
**          Size of the bufobj
**
*/
gceSTATUS
gcoBUFOBJ_GetSize(
    IN gcoBUFOBJ BufObj,
    OUT gctSIZE_T_PTR Size
    )
{
    gcmHEADER_ARG("BufObj=0x%x Size=0x%x", BufObj, Size);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Return size */
    if (Size != gcvNULL)
    {
        *Size = BufObj->bytes;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoBUFOBJ_GetNode
**
**  Returns the memory node of the buffer object
**
**  INPUT:
**
**      ggcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**  OUTPUT:
**
**      gcsSURF_NODE_PTR Node
**          Memory node of the bufobj
**
*/
gceSTATUS
gcoBUFOBJ_GetNode(
    IN gcoBUFOBJ BufObj,
    OUT gcsSURF_NODE_PTR * Node
    )
{
    gcmHEADER_ARG("BufObj=0x%x Node=0x%x", BufObj, Node);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Return size */
    if (Node != gcvNULL)
    {
        *Node = &BufObj->memory;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**
**  gcoBUFOBJ_IndexBind
**
**  Bind the index object to the hardware.
**
**  INPUT:
**
**      gcoBUFOBJ Index
**          Pointer to an gcoBUFOBJ object.
**
**      gceINDEX_TYPE IndexType
**          Index type.
**
**      gctUINT32 Offset
**          Offset of stream.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoBUFOBJ_IndexBind (
    IN gcoBUFOBJ Index,
    IN gceINDEX_TYPE Type,
    IN gctUINT32 Offset,
    IN gctSIZE_T Count
    )
{
    gceSTATUS status;
    gctUINT32 address;
    gctBOOL indexLocked;

    gcmHEADER_ARG("Index=0x%x Type=%d Offset=%u", Index, Type, Offset);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Index, gcvOBJ_BUFOBJ);

    /* Is locked? */
    indexLocked = gcvFALSE;

    /* Lock the bufobj buffer. */
    gcmONERROR(gcoHARDWARE_Lock(&Index->memory, &address, gcvNULL));
    indexLocked = gcvTRUE;

    /* Add offset */
    address += Offset;

    /* Program index */
    gcmONERROR(gcoHARDWARE_BindIndex(gcvNULL, address, 0, Type, (Count * 3)));
    /* Unlock the bufobj buffer. */
    if (indexLocked)
    {
        /* Unlock the bufobj buffer. */
        gcmONERROR(gcoHARDWARE_Unlock(&Index->memory, Index->surfType));
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
OnError:

    if (indexLocked)
    {
        /* Unlock the bufobj buffer. */
        gcmVERIFY_OK(gcoHARDWARE_Unlock(&Index->memory, Index->surfType));
    }

    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_IndexGetRange
**
**  Determine the index range in the current index buffer.
**
**  INPUT:
**
**      gcoBUFOBJ Index
**          Pointer to an gcoBUFOBJ object that needs to be destroyed.
**
**      gceINDEX_TYPE Type
**          Index type.
**
**      gctUINT32 Offset
**          Offset into the buffer.
**
**      gctUINT32 Count
**          Number of indices to analyze.
**
**  OUTPUT:
**
**      gctUINT32 * MinimumIndex
**          Pointer to a variable receiving the minimum index value in
**          the index buffer.
**
**      gctUINT32 * MaximumIndex
**          Pointer to a variable receiving the maximum index value in
**          the index buffer.
*/
gceSTATUS
gcoBUFOBJ_IndexGetRange(
    IN gcoBUFOBJ Index,
    IN gceINDEX_TYPE Type,
    IN gctUINT32 Offset,
    IN gctUINT32 Count,
    OUT gctUINT32 * MinimumIndex,
    OUT gctUINT32 * MaximumIndex
    )
{
    gceSTATUS status;
    gctUINT32 minIndex, maxIndex;
    gctUINT8_PTR data;
    gctBOOL primRestart;
    gctSIZE_T i;
    gcoBUFOBJ_INDEX * indexNode;
    gctBOOL indexLocked;

    gcmHEADER_ARG("Index=0x%x Type=%d Offset=%u Count=%u MinimumIndex=0x%x MaximumIndex=0x%x",
                  Index, Type, Count, MinimumIndex, MaximumIndex);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Index, gcvOBJ_BUFOBJ);
    gcmDEBUG_VERIFY_ARGUMENT(MinimumIndex != gcvNULL);
    gcmDEBUG_VERIFY_ARGUMENT(MaximumIndex != gcvNULL);

    /* Shortcut to the main index structure. */
    indexNode = &Index->indexNode;
    indexLocked = gcvFALSE;

    /* Not yet determined or determined for a different count? */
    if ((indexNode->maxIndex == 0) || (indexNode->minIndex == ~0U) || (indexNode->count != Count) || (indexNode->offset != Offset))
    {
        /* Lock the bufobj buffer. */
        gcmONERROR(gcoHARDWARE_Lock(&Index->memory, gcvNULL, (gctPOINTER)&data));

        /* Index is locked */
        indexLocked = gcvTRUE;

        /* Reset the extremes. */
        minIndex = ~0U;
        maxIndex =  0;

        /* get primitive restart */
        primRestart = (gcoHARDWARE_IsPrimitiveRestart(gcvNULL) == gcvTRUE);

        /* Determine the range. */
        switch (Type)
        {
        case gcvINDEX_8:
            {
                gctUINT8_PTR indexBuffer = (gctUINT8_PTR) (data + Offset);
                for (i = 0; i < Count; i++)
                {
                    gctUINT32 curIndex = *indexBuffer++;
                    if (primRestart && curIndex == 0xFF)
                        continue;
                    if (curIndex < minIndex) minIndex = curIndex;
                    if (curIndex > maxIndex) maxIndex = curIndex;
                }
            }
            break;
        case gcvINDEX_16:
            {
                gctUINT16_PTR indexBuffer = (gctUINT16_PTR) (data + Offset);
                for (i = 0; i < Count; i++)
                {
                    gctUINT32 curIndex = *indexBuffer++;
                    if (primRestart && curIndex == 0xFFFF)
                        continue;
                    if (curIndex < minIndex) minIndex = curIndex;
                    if (curIndex > maxIndex) maxIndex = curIndex;
                }
            }
            break;
        case gcvINDEX_32:
            {
                gctUINT32_PTR indexBuffer = (gctUINT32_PTR) (data + Offset);
                for (i = 0; i < Count; i++)
                {
                    gctUINT32 curIndex = *indexBuffer++;
                    if (primRestart && curIndex == 0xFFFFFFFF)
                        continue;
                    if (curIndex < minIndex) minIndex = curIndex;
                    if (curIndex > maxIndex) maxIndex = curIndex;
                }
            }
            break;
        default:
            gcmONERROR(gcvSTATUS_INVALID_ARGUMENT);
        }

        indexNode->minIndex = minIndex;
        indexNode->maxIndex = maxIndex;
        indexNode->count = Count;
        indexNode->offset = Offset;

        /* Unlock the bufobj buffer. */
        gcmONERROR(gcoHARDWARE_Unlock(&Index->memory, Index->surfType));
    }

    /* Set results. */
    *MinimumIndex = indexNode->minIndex;
    *MaximumIndex = indexNode->maxIndex;

    /* Success. */
    gcmFOOTER_ARG("*MinimumIndex=%u *MaximumIndex=%u", * MinimumIndex, * MaximumIndex);
    return gcvSTATUS_OK;

OnError:
    if (indexLocked)
    {
        /* Unlock the bufobj buffer. */
        gcmVERIFY_OK(gcoHARDWARE_Unlock(&Index->memory, Index->surfType));
    }
    /* Return the status. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_SetDirty
**
**  Sets a buffer object as dirty
**
**  INPUT:
**
**      ggcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object.
**
**
*/
gceSTATUS
gcoBUFOBJ_SetDirty(
    IN gcoBUFOBJ BufObj
    )
{
    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Reset min max */
    if (BufObj->type == gcvBUFOBJ_TYPE_ELEMENT_ARRAY_BUFFER)
    {
        BufObj->indexNode.maxIndex = 0;
        BufObj->indexNode.minIndex = ~0U;
        BufObj->indexNode.count = 0;
        BufObj->indexNode.offset = 0;
    }

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;
}

/* Creates a new buffer if needed */
gceSTATUS
gcoBUFOBJ_AlignIndexBufferWhenNeeded(
    IN gcoBUFOBJ BufObj,
    IN gctSIZE_T Offset,
    OUT gcoBUFOBJ * AlignedBufObj
    )
{
    gceSTATUS   status;
    gctUINT32   address, offset;
    gctPOINTER  memory;
    gctBOOL     indexLocked;
    gcoBUFOBJ   newBuffer = gcvNULL;

    gcmHEADER_ARG("BufObj=0x%x AlignedBufObj=0x%x", BufObj, AlignedBufObj);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Set locked to false */
    indexLocked = gcvFALSE;

    gcmONERROR(gcoHARDWARE_Lock(&BufObj->memory, &address, &memory));
    indexLocked = gcvTRUE;

    gcmSAFECASTSIZET(offset, Offset);
    /* Add offset to address */
    address += offset;

    /* If address is not 16 byte aligned */
    if (address % 16)
    {
        /* Construct the new buffer */
        gcmONERROR(gcoBUFOBJ_Construct(gcvNULL, gcvBUFOBJ_TYPE_ELEMENT_ARRAY_BUFFER, &newBuffer));

        /* Calculate logical address */
        memory = (gctUINT8_PTR) memory + Offset;

        /* Upload the data with 0 offset */
        gcmONERROR(gcoBUFOBJ_Upload(newBuffer, memory, 0, (BufObj->bytes - Offset), BufObj->usage));
    }

    /* Set output */
    *AlignedBufObj = newBuffer;

    /* Unlock the bufobj buffer. */
    gcmONERROR(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));

    /* Success. */
    gcmFOOTER_NO();
    return gcvSTATUS_OK;

OnError:

    if (indexLocked)
    {
        /* Unlock the bufobj buffer. */
        gcmVERIFY_OK(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));
    }

    if (newBuffer)
    {
        /* Destroy new buffer if created */
        gcmVERIFY_OK(gcoBUFOBJ_Destroy(newBuffer));
    }

    /* Set output */
    *AlignedBufObj = gcvNULL;

    gcmFOOTER_NO();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_CPUCacheOperation_Range
**
**  Handles CPU side cache operations on a specified range in a buffer
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object
**
**      gctUINT32 Offset
**          Offset into the buffer to indicate the start of the range
**
**      gctSIZE_T Length,
**          Length of the range that starts from offset
**
**      gceCACHEOPERATION Operation
**          Cache operation that will be performed
**
**  OUTPUT:
**
**      NONE
*/
gceSTATUS
gcoBUFOBJ_CPUCacheOperation_Range(
    IN gcoBUFOBJ BufObj,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Length,
    IN gceCACHEOPERATION Operation
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("BufObj=0x%x, Offset=%u Length=%u Operation=%d", BufObj, Offset, Length, Operation);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Call cache operation */
    gcoSURF_NODE_CPUCacheOperation(&BufObj->memory, BufObj->surfType, Offset, Length, Operation);

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_CPUCacheOperation
**
**  Handles CPU side cache operations on a specified buffer
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object
**
**      gceCACHEOPERATION Operation
**          Cache operation that will be performed
**
**  OUTPUT:
**
**      NONE
*/
gceSTATUS
gcoBUFOBJ_CPUCacheOperation(
    IN gcoBUFOBJ BufObj,
    IN gceCACHEOPERATION Operation
    )
{
    gceSTATUS status;
    gctPOINTER memory;
    gctBOOL locked = gcvFALSE;

    gcmHEADER_ARG("BufObj=0x%x, Operation=%d", BufObj, Operation);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    /* Lock the node. */
    gcmONERROR(gcoHARDWARE_Lock(&BufObj->memory, gcvNULL, &memory));
    locked = gcvTRUE;

    gcmONERROR(gcoSURF_NODE_Cache(&BufObj->memory, memory, BufObj->bytes, Operation));

    /* Unlock the node. */
    gcmONERROR(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));
    locked = gcvFALSE;

    gcmFOOTER();
    return gcvSTATUS_OK;

OnError:
    if (locked)
    {
        gcmVERIFY_OK(gcoHARDWARE_Unlock(&BufObj->memory, BufObj->surfType));
    }

    gcmFOOTER();
    return status;
}

/*******************************************************************************
**
**  gcoBUFOBJ_GPUCacheOperation
**
**  Handles GPU cache operations.
**
**  INPUT:
**
**      gcoBUFOBJ BufObj
**          Pointer to an gcoBUFOBJ object
**
**  OUTPUT:
**
**      NONE
*/
gceSTATUS
gcoBUFOBJ_GPUCacheOperation(
    gcoBUFOBJ BufObj
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("BufObj=0x%x", BufObj);

    gcmVERIFY_OBJECT(BufObj, gcvOBJ_BUFOBJ);

    switch(BufObj->type)
    {
    case gcvBUFOBJ_TYPE_ARRAY_BUFFER:
        status = gcoHARDWARE_FlushVertex(gcvNULL);
        break;
    case gcvBUFOBJ_TYPE_ELEMENT_ARRAY_BUFFER:
    case gcvBUFOBJ_TYPE_GENERIC_BUFFER:
    default:
        break;
    }

    gcmFOOTER();
    return status;
}

void
gcoBUFOBJ_Dump(
    IN gcoBUFOBJ BufObj
    )
{
    gceSTATUS status;
    gctPOINTER logical;
    gctUINT32 physical;

    status = gcoHARDWARE_Lock(&BufObj->memory, &physical, &logical);

    if (gcmIS_SUCCESS(status))
    {
        gcmDUMP_BUFFER(gcvNULL,
                       BufObj->dumpDescriptor,
                       physical,
                       logical,
                       0,
                       BufObj->bytes);

        gcoHARDWARE_Unlock(&BufObj->memory, gcvSURF_VERTEX);
    }
}

#else /* gcdNULL_DRIVER < 2 */


/*******************************************************************************
** NULL DRIVER STUBS
*/

gceSTATUS gcoBUFOBJ_Construct(
    IN gcoHAL Hal,
    OUT gcoBUFOBJ * BufObj
    )
{
    *BufObj = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_Destroy(
    IN gcoBUFOBJ BufObj
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_Lock(
    IN gcoBUFOBJ BufObj,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{
    *Address = 0;
    *Memory = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_FastLock(
    IN gcoBUFOBJ BufObj,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{
    *Address = 0;
    *Memory = gcvNULL;
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_Unlock(
    IN gcoBUFOBJ BufObj
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_Free(
    IN gcoBUFOBJ BufObj
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS gcoBUFOBJ_Upload(
    IN gcoBUFOBJ BufObj,
    IN gctCONST_POINTER Buffer,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Bytes,
    IN gceBUFOBJ_USAGE Usage
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_IndexBind (
    IN gcoBUFOBJ Index,
    IN gceINDEX_TYPE Type,
    IN gctUINT32 Offset
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_IndexGetRange(
    IN gcoBUFOBJ Index,
    IN gceINDEX_TYPE Type,
    IN gctUINT32 Count,
    OUT gctUINT32 * MinimumIndex,
    OUT gctUINT32 * MaximumIndex
    )
{
    * MinimumIndex = ~0U;
    * MaximumIndex = 0;
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_CPUCacheOperation(
    IN gcoBUFOBJ BufObj,
    IN gceCACHEOPERATION Operation
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_CPUCacheOperation_Range(
    IN gcoBUFOBJ BufObj,
    IN gctSIZE_T Offset,
    IN gctSIZE_T Length,
    IN gceCACHEOPERATION Operation
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_GPUCacheOperation(
    gcoBUFOBJ BufObj
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_SetDirty(
    IN gcoBUFOBJ BufObj
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoBUFOBJ_AlignIndexBufferWhenNeeded(
    IN gcoBUFOBJ BufObj,
    IN gctSIZE_T Offset,
    OUT gcoBUFOBJ * AlignedBufObj
    )
{
    return gcvSTATUS_OK;
}

void
gcoBUFOBJ_Dump(
    IN gcoBUFOBJ BufObj
    )
{
    return;
}

#endif /* gcdNULL_DRIVER < 2 */
#endif /* gcdENABLE_3D */

