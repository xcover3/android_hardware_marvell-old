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

#if gcdENABLE_VG

#include "gc_hal_user_vg.h"
#include <malloc.h>
#if GC355_PROFILER
    gctUINT64 startTimeusec = 0;
    gctUINT64 endTimeusec = 0;
    gctUINT64 deltaValue = 0;
#endif

#define _GC_OBJ_ZONE    gcvZONE_VG

#if gcdENABLE_SHARE_TESSELLATION
static gcsTESSELATION_PTR   g_tsBuffer;
static gctPOINTER           g_tsMutex;
#endif

/******************************************************************************\
*********************** Support Functions and Definitions **********************
\******************************************************************************/
#if GC355_PROFILER

gctINT vgHALProfilerDLinklist_GetLength(gcsPROFILERFUNCNODE *L)
{
    gcsPROFILERFUNCNODE *p;
    gctINT length = 0;

    p = L->next;
    while((p != gcvNULL) && (p != L))
    {
        length ++;
        p = p->next;
    }

    return length;
}

gcsPROFILERFUNCNODE * vgHALProfilerDLinklist_GetElem(gcsPROFILERFUNCNODE *L, gctINT i)
{
    int length, j;
    gcsPROFILERFUNCNODE *p;

    p = L;
    length = vgHALProfilerDLinklist_GetLength(L);
    if (i < 0 || i > length)
    {
        return gcvNULL;
    }

    for (j = 1; j <= i; j++)
    {
        p = p->next;
    }

    return p;
}

gctBOOL vgHALProfilerDLinklist_Insert(gcsPROFILERFUNCNODE *L, gctINT i, gcsPROFILERFUNCData_PTR elem)
{
    int length;
    gcsPROFILERFUNCNODE *p, *s;

    length = vgHALProfilerDLinklist_GetLength(L);
    if (i < 1 || i > length + 1)
    {
        return gcvFALSE;
    }

    p = vgHALProfilerDLinklist_GetElem(L, i-1);
    if (p == gcvNULL)
    {
        return gcvFALSE;
    }

    s = (gcsPROFILERFUNCNODE *)malloc(sizeof(gcsPROFILERFUNCNODE));
    if (s == gcvNULL)
    {
        gcoOS_Print("Create GC355 profiler function node failed!");
        return gcvFALSE;
    }

    s->data = elem;

    if ( i == (length + 1))
    {
        s->next = gcvNULL;
        s->pre = p;
        p->next = s;
    }
    else
    {
        s->next = p->next;
        p->next->pre = s;
        s->pre = p;
        p->next = s;
    }

    return gcvTRUE;
}

gctBOOL vgHALProfilerDLinklist_SearchAndFill(gcsPROFILERFUNCNODE *L, gctSTRING funcName, gctUINT64 elapsedTime, gctUINT64 cpuTime)
{
    gcsPROFILERFUNCNODE *p;
    gctINT match = 0;


    p = vgHALProfilerDLinklist_GetElem(L, vgHALProfilerDLinklist_GetLength(L));

    while(p != gcvNULL && p != L)
    {
        match = strcmp(p->data->funcName,funcName);
        if (match != 0)
        {
            p = p->pre;
        }
        else
        {
            break;
        }
    }

    if (p == L)
    {
        return gcvFALSE;
    }

    p->data->elapsedTime = elapsedTime;
    p->data->cpuTime = cpuTime;

    return gcvTRUE;
}

void _HalWriteProfileElapsedTimeInfo(gcoVG Vg)
{
    gctUINT64 endTime = 0;
    gctUINT64 deltaValue = 0;
    gctUINT offset = 0;
    gctSIZE_T length;
    gctCHAR savedValue[256] = {'\0'};

    gcoOS_GetTime(&endTime);
    deltaValue = (gctINT32)(endTime - Vg->appStartTime);
    gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "[%-10llu us]", deltaValue);

    length = gcoOS_StrLen((gctSTRING)savedValue, gcvNULL);
    gcoOS_Write(gcvNULL, Vg->apiTimeFile, length, savedValue);
}

void
_WriteHALAPITimeInfo(
    gcoVG Vg,
    gctSTRING functionName,
    gctUINT64 cpuTime
    )
 {
     gctUINT offset = 0;
     gctSIZE_T length;
     gctCHAR savedValue[256] = {'\0'};
     gctCHAR s[256] = "";
     gctCHAR Tag = '-';
     gctINT32 gpuTime = 0;

     _HalWriteProfileElapsedTimeInfo(Vg);

     if(Vg->pathName)
     {
         gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "[%-10s]",Vg->pathName);
     }
     else
     {
         gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "[%c]",Tag);
     }

     if (Vg->TreeDepth)
     {
         gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "[%d]    ",Vg->TreeDepth - Vg->varTreeDepth);
         if ((Vg->TreeDepth - Vg->varTreeDepth) > 1)
         {
             gctUINT i = 0,j = 0;
             memset(s,' ',(Vg->TreeDepth - Vg->varTreeDepth - 1) * 4);
             for (i = 0; i < (Vg->TreeDepth - Vg->varTreeDepth - 1) * 4; i += 4)
             {
                 s[i] = '|';
                 j = i;
             }
             s[j+1] = '-';
             gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "%s",s);
         }
     }

     gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "%s : %llu us , ",functionName, cpuTime);
     gcoOS_PrintStrSafe(savedValue, gcmSIZEOF(savedValue), &offset, "%llu us, %llu us\n",gpuTime, cpuTime + gpuTime);
     length = gcoOS_StrLen((gctSTRING)savedValue, gcvNULL);
     gcoOS_Write(gcvNULL,Vg->apiTimeFile, length, savedValue);
}

void
gcoVG_ProfilerEnableDisable(
    IN gcoVG Vg,
    IN gctUINT64 appStartTime,
    IN gctBOOL enableGetAPITimes,
    IN gctFILE apiTimeFile
    )
{
    Vg->enableGetAPITimes = enableGetAPITimes;
    Vg->apiTimeFile = apiTimeFile;
    if (enableGetAPITimes == 1)
    {
        Vg->appStartTime = appStartTime;
    }
}

void
gcoVG_ProfilerTreeDepth(
    IN gcoVG Vg,
    IN gctUINT TreeDepth
    )
{
    Vg->TreeDepth = TreeDepth;
}

void
gcoVG_ProfilerTag(
    IN gcoVG Vg,
    IN gctSTRING Tag
    )
{
    Vg->pathName = Tag;
}

void
gcoVG_ProfilerSetStates(
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT treeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth
    )
{
    if (Vg != gcvNULL)
    {
        Vg->funcDList = DList;
        Vg->saveLayerTreeDepth = saveLayerTreeDepth;
        Vg->varTreeDepth = varTreeDepth;
    }
}
#endif

static gceSTATUS
_FreeTessellationBuffer(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsTESSELATION_PTR TessellationBuffer
    )
{
    gceSTATUS status;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(_FreeTessellationBuffer);
#endif
    do
    {
        /* Delete the old buffer if any. */
        if (TessellationBuffer->node == 0)
        {
            gcmASSERT(TessellationBuffer->allocatedSize == 0);
            status = gcvSTATUS_OK;
            break;
        }
#if GC355_PROFILER
        /* Schedule to delete the old buffer. */
        gcmERR_BREAK(gcoHAL_ScheduleVideoMemory(
            Vg->hal, Vg, Vg->funcDList,
            Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth,
            TessellationBuffer->node
            ));
#else
        gcmERR_BREAK(gcoHAL_ScheduleVideoMemory(
            Vg->hal, TessellationBuffer->node
            ));
#endif
        /* Reset the buffer. */
        TessellationBuffer->node = 0;
        TessellationBuffer->allocatedSize = 0;
    }
    while (gcvFALSE);
#if GC355_PROFILER
	vghalLEAVESUBAPI(_FreeTessellationBuffer);
#endif
    /* Return status. */
    return status;
}

#if gcdENABLE_SHARE_TESSELLATION

static gceSTATUS
_TessellationBufferAddRef(
        IN gcoVG Vg,
#if GC355_PROFILER
        IN gcsPROFILERFUNCNODE *DList,
        IN gctUINT TreeDepth,
    	IN gctUINT saveLayerTreeDepth,
    	IN gctUINT varTreeDepth,
#endif
        IN gcsTESSELATION_PTR  TessellationBuffer)
{
    gcmASSERT(TessellationBuffer->reference >= 0);
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(_TessellationBufferAddRef);
#endif
    TessellationBuffer->reference++;
#if GC355_PROFILER
	vghalLEAVESUBAPI(_TessellationBufferAddRef);
#endif
    return gcvSTATUS_OK;
}

static gceSTATUS
_TessellationBufferRelease(
        IN gcoVG Vg,
#if GC355_PROFILER
        IN gcsPROFILERFUNCNODE *DList,
        IN gctUINT TreeDepth,
    	IN gctUINT saveLayerTreeDepth,
    	IN gctUINT varTreeDepth,
#endif
        IN gcsTESSELATION_PTR  TessellationBuffer)
{
    gceSTATUS status = gcvSTATUS_OK;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(_TessellationBufferRelease);
#endif
    TessellationBuffer->reference--;

    gcmASSERT(TessellationBuffer->reference >= 0);

    do
    {
        if (TessellationBuffer->reference == 0)
        {
#if GC355_PROFILER
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, TessellationBuffer));
#else
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, TessellationBuffer));
#endif
            gcmERR_BREAK(gcmOS_SAFE_FREE(Vg->os, TessellationBuffer));

            if (TessellationBuffer == g_tsBuffer) g_tsBuffer = gcvNULL;
        }

    }while(gcvFALSE);

#if GC355_PROFILER
 	vghalLEAVESUBAPI(_TessellationBufferRelease);
#endif
    return status;
}

static gceSTATUS
_CreateTSMutex(IN gcoVG Vg)
{
    if (g_tsMutex == gcvNULL)
    {
        return gcoOS_CreateMutex(Vg->os, &g_tsMutex);
    }

    return gcvSTATUS_OK;
}

static gceSTATUS
_DestroyTSMutex(IN gcoVG Vg)
{
    if (g_tsMutex != gcvNULL)
    {
        return gcoOS_DeleteMutex(Vg->os, g_tsMutex);
    }

    return gcvSTATUS_OK;
}

#endif

static gceSTATUS
_GetTessellationBuffer(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
#if gcdMOVG
    IN gctINT Width,
    IN gctINT Height,
    OUT gcsTESSELATION_PTR * TessellationBuffer,
    OUT gctINT *AlignedWidth,
    OUT gctINT *AlignedHeight
#else
    OUT gcsTESSELATION_PTR * TessellationBuffer
#endif
    )
{
    gceSTATUS status;

#if gcdGC355_MEM_PRINT
#ifdef LINUX
    gcoOS_RecordAllocation();
#endif
#endif

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(_GetTessellationBuffer);
#endif
    do
    {
        gctUINT allocationSize;
        gctUINT bufferSize, alignedBufferSize;
        gctUINT L1Size, alignedL1Size;
        gctUINT L2Size, alignedL2Size;
        gctUINT bufferStride, shift;
        gcsTESSELATION_PTR buffer;
#if gcdMOVG
        gctINT  aw, ah;
        /* Validate requested size. */
        Width = gcmMIN(Vg->targetWidth, gcmMAX(Width, 0));
        Height = gcmMIN(Vg->targetHeight, gcmMAX(Height, 0));
#endif

        if (Vg->vg20)
        {
            /* The tessellation buffer is 128x16 tiled or 512x16 tiles without
               MSAA. */
#if gcdMOVG
            aw = gcmALIGN(Width, 128);
            bufferStride = (Vg->renderQuality == gcvVG_NONANTIALIASED)
                ? ( aw * 2)
                : ( aw * 8);

            ah = gcmALIGN(Height, 16);
            bufferSize = bufferStride * ah;
#else
            bufferStride = ((Vg->targetWidth + 127) & ~127) * 8;

            bufferSize = bufferStride * ((Vg->targetHeight + 15) & ~15);
#endif

            /* Determine buffer shift value. */
            shift = 0;
        }
        else
        {
            /* For MSAA, we use 8x1 64-bit pixels per cache line;
               otherwise we use 32x1 16-bit pixels per cache line. */
#if gcdMOVG
            aw = (Vg->renderQuality == gcvVG_NONANTIALIASED) ?
                 gcmALIGN(Width, 32) :
                 gcmALIGN(Width, 8);
            bufferStride = (Vg->renderQuality == gcvVG_NONANTIALIASED)
                ? (aw * 2)
                : (aw * 8);

            ah = Height;
            bufferSize = bufferStride * Height;

            /* Determine buffer shift value. */
            shift = (Width > 2048)
                ? 3
                : (Width > 1024)
                    ? 2
                    : (Width > 512)
                        ? 1
                        : 0;
#else
            bufferStride = (Vg->renderQuality == gcvVG_NONANTIALIASED)
                ? (((Vg->targetWidth + 31) & ~31) * 2)
                : (((Vg->targetWidth +  7) &  ~7) * 8);

            bufferSize = bufferStride * Vg->targetHeight;

            /* Determine buffer shift value. */
            shift = (Vg->targetWidth > 2048)
                ? 3
                : (Vg->targetWidth > 1024)
                    ? 2
                    : (Vg->targetWidth > 512)
                        ? 1
                        : 0;
#endif
        }

        /* L1 status.  Each bit represents 64 bytes from Buffer.
           So we need BufferSize / 64 bits, grouped in 64-bit cache lines. */
        L1Size = ((bufferSize / 64 + 63) & ~63) / 8;

        /* L2 status.  Each bit represents 8 bytes from L1 status.
           So we need L1Size / 8 bits, grouped in 64-bit cache lines. */
        L2Size  = ((L1Size / 8 + 63) & ~63) / 8;

        /* Align the buffer sizes. */
        alignedBufferSize = gcmALIGN(bufferSize, 64);
        alignedL1Size     = gcmALIGN(L1Size,     64);
        alignedL2Size     = gcmALIGN(L2Size,     64);

        /* Determin the allocation size. */
        allocationSize = alignedBufferSize + alignedL1Size + alignedL2Size;

#if gcdENABLE_SHARE_TESSELLATION

        status = _CreateTSMutex(Vg);
        if (gcmIS_ERROR(status))
        {
            break;
        }

        status = gcoOS_AcquireMutex(Vg->os, g_tsMutex, gcvINFINITE);
        if (gcmIS_ERROR(status))
        {
            break;
        }

        if ((g_tsBuffer == gcvNULL) || (g_tsBuffer->allocatedSize < allocationSize))
        {
            gcmERR_GOTO(gcoOS_Allocate(Vg->os,  gcmSIZEOF(gcsTESSELATION), (gctPOINTER *) &buffer));

            /* Allocate a new buffer. */
            gcmERR_GOTO(gcoVGHARDWARE_AllocateLinearVideoMemory(
                Vg->hw,
                allocationSize, 64,
                gcvPOOL_DEFAULT,
                gcvALLOC_FLAG_CONTIGUOUS,
                &buffer->node,
                &buffer->tsBufferPhysical,
                (gctPOINTER *) &buffer->tsBufferLogical
                ));

            /* Set the new size. */
            buffer->allocatedSize = allocationSize;
            /* Determine buffer pointers. */
            buffer->L1BufferLogical  = buffer->tsBufferLogical  + alignedBufferSize;
            buffer->L1BufferPhysical = buffer->tsBufferPhysical + alignedBufferSize;
            buffer->L2BufferLogical  = buffer->L1BufferLogical  + alignedL1Size;
            buffer->L2BufferPhysical = buffer->L1BufferPhysical + alignedL1Size;

            /* Set buffer parameters. */
            buffer->stride    = bufferStride;
            buffer->shift     = shift;
            buffer->clearSize = alignedL2Size;
            buffer->reference = 0;

            g_tsBuffer = buffer;
        }

        if (Vg->tsBuffer != g_tsBuffer)
        {
            if (Vg->tsBuffer != gcvNULL)
            {
#if GC355_PROFILER
                gcmERR_GOTO(_TessellationBufferRelease(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Vg->tsBuffer));
#else
                gcmERR_GOTO(_TessellationBufferRelease(Vg, Vg->tsBuffer));
#endif
            }

            Vg->tsBuffer = g_tsBuffer;
#if GC355_PROFILER
            gcmERR_GOTO(_TessellationBufferAddRef(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Vg->tsBuffer));
#else
            gcmERR_GOTO(_TessellationBufferAddRef(Vg, Vg->tsBuffer));
#endif
        }


        Vg->tsBuffer2.node              = Vg->tsBuffer->node;
        Vg->tsBuffer2.tsBufferLogical   = Vg->tsBuffer->tsBufferLogical;
        Vg->tsBuffer2.tsBufferPhysical  = Vg->tsBuffer->tsBufferPhysical;
        Vg->tsBuffer2.allocatedSize     = Vg->tsBuffer->allocatedSize;

        Vg->tsBuffer2.L1BufferLogical   = Vg->tsBuffer->tsBufferLogical  + alignedBufferSize;
        Vg->tsBuffer2.L1BufferPhysical  = Vg->tsBuffer->tsBufferPhysical + alignedBufferSize;
        Vg->tsBuffer2.L2BufferLogical   = Vg->tsBuffer->L1BufferLogical  + alignedL1Size;
        Vg->tsBuffer2.L2BufferPhysical  = Vg->tsBuffer->L1BufferPhysical + alignedL1Size;
        Vg->tsBuffer2.stride            = bufferStride;
        Vg->tsBuffer2.shift             = shift;
        Vg->tsBuffer2.clearSize         = alignedL2Size;

        /* Get the buffer pointer. */
        buffer = &Vg->tsBuffer2;
#else
        /* Advance to the next buffer. */
        if (Vg->tsBufferIndex == gcmCOUNTOF(Vg->tsBuffer) - 1)
        {
            Vg->tsBufferIndex = 0;
        }
        else
        {
            Vg->tsBufferIndex += 1;
        }

        /* Get the buffer pointer. */
        buffer = &Vg->tsBuffer[Vg->tsBufferIndex];

        /* Is the buffer big enough? */
        if (buffer->allocatedSize < allocationSize)
        {
#if GC355_PROFILER
            /* No, reallocate the buffer. */
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, buffer));
#else
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, buffer));
#endif
            /* Allocate a new buffer. */
            gcmERR_BREAK(gcoVGHARDWARE_AllocateLinearVideoMemory(
                Vg->hw,
                allocationSize, 64,
                gcvPOOL_DEFAULT,
                gcvALLOC_FLAG_CONTIGUOUS,
                &buffer->node,
                &buffer->tsBufferPhysical,
                (gctPOINTER *) &buffer->tsBufferLogical
                ));

#if gcdGC355_MEM_PRINT
#ifdef LINUX
            gcoOS_AddRecordAllocation(-(gctINT32)buffer->allocatedSize);
            gcoOS_AddRecordAllocation((gctINT32)allocationSize);
#endif
#endif

            /* Set the new size. */
            buffer->allocatedSize = allocationSize;
        }
        else
        {
            /* Success. */
            status = gcvSTATUS_OK;
        }

        /* Determine buffer pointers. */
        buffer->L1BufferLogical  = buffer->tsBufferLogical  + alignedBufferSize;
        buffer->L1BufferPhysical = buffer->tsBufferPhysical + alignedBufferSize;
        buffer->L2BufferLogical  = buffer->L1BufferLogical  + alignedL1Size;
        buffer->L2BufferPhysical = buffer->L1BufferPhysical + alignedL1Size;

        /* Set buffer parameters. */
        buffer->stride    = bufferStride;
        buffer->shift     = shift;
        buffer->clearSize = alignedL2Size;
#endif

#if gcdMOVG
        buffer->width = aw;
        buffer->height = ah;

        if (AlignedWidth)
        {
            *AlignedWidth = aw;
        }
        if (AlignedHeight)
        {
            *AlignedHeight = ah;
        }
#endif

        /* Set the result pointer. */
        (* TessellationBuffer) = buffer;


#if gcdENABLE_SHARE_TESSELLATION
ErrorHandler:
     gcmVERIFY_OK(gcoOS_ReleaseMutex(Vg->os, g_tsMutex));
#endif

#if gcdGC355_MEM_PRINT
#ifdef LINUX
    Vg->tsCurMemSize += gcoOS_EndRecordAllocation();
    if (Vg->tsMaxMemSize < Vg->tsCurMemSize)
    {
        Vg->tsMaxMemSize = Vg->tsCurMemSize;
    }
#endif
#endif
    }
    while(gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(_GetTessellationBuffer);
#endif

    /* Return status. */
    return status;
}


/******************************************************************************\
********************************* gcoHAL Object ********************************
\******************************************************************************/

/*******************************************************************************
**
**  gcoHAL_QueryPathStorage
**
**  Query path data storage attributes.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**  OUTPUT:
**
**      gcsPATH_BUFFER_INFO_PTR Information
**          Pointer to the information structure to receive buffer attributes.
*/
gceSTATUS
gcoHAL_QueryPathStorage(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    OUT gcsPATH_BUFFER_INFO_PTR Information
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_QueryPathStorage);
#endif
    /* Copy the command buffer information. */
    status = gcoVGHARDWARE_QueryPathStorage(
        gcvNULL, Information
        );
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_QueryPathStorage);
#endif
    /* Return status. */
    return status;
}

gceSTATUS
gcoHAL_AssociateCompletion(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsPATH_DATA_PTR PathData
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_AssociateCompletion);
#endif
    /* Relay the call. */
    status = gcoVGHARDWARE_AssociateCompletion(
        gcvNULL, &PathData->data
        );
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_AssociateCompletion);
#endif
    /* Return status. */
    return status;
}

gceSTATUS
gcoHAL_DeassociateCompletion(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsPATH_DATA_PTR PathData
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_DeassociateCompletion);
#endif
    /* Relay the call. */
    status = gcoVGHARDWARE_DeassociateCompletion(
        gcvNULL, &PathData->data
        );
#if GC355_PROFILER
		vghalLEAVESUBAPI(gcoHAL_DeassociateCompletion);
#endif
    /* Return status. */
    return status;
}

gceSTATUS
gcoHAL_CheckCompletion(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsPATH_DATA_PTR PathData
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_CheckCompletion);
#endif
    /* Relay the call. */
    status = gcoVGHARDWARE_CheckCompletion(
        gcvNULL, &PathData->data
        );
#if GC355_PROFILER
		vghalLEAVESUBAPI(gcoHAL_CheckCompletion);
#endif
    /* Return status. */
    return status;
}

gceSTATUS
gcoHAL_WaitCompletion(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsPATH_DATA_PTR PathData
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_WaitCompletion);
#endif
    /* Relay the call. */
    status = gcoVGHARDWARE_WaitCompletion(
        gcvNULL, &PathData->data
        );
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_WaitCompletion);
#endif
    /* Return status. */
    return status;
}


/*******************************************************************************
**
**  gcoHAL_Flush
**
**  Flush the pixel cache.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_Flush(
    IN gcoHAL Hal
#if GC355_PROFILER
    ,
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth
#endif
    )
{
	gceSTATUS status;
    /* Flush the current pipe. */
    status = gcoVGHARDWARE_FlushPipe(gcvNULL);
    return status;
}

/*******************************************************************************
**
**  gcoHAL_AllocateLinearVideoMemory
**
**  Allocate and lock linear video memory. If both Address and Memory parameters
**  are given as gcvNULL, the memory will not be locked, only allocated.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**      gctUINT Size
**          The size of the area to allocate.
**
**      gctUINT Alignment
**          The minimum alignment for the allocated area.
**
**      gcePOOL Pool
**          The desired pool for the allocated area.
**
**  OUTPUT:
**
**      gctUINT32 * Node
**          Pointer to the variable to receive the node of the allocated area.
**
**      gctUINT32 * Address
**          Pointer to the variable to receive the hardware address of the
**          allocated area.  Can be gcvNULL.
**
**      gctPOINTER * Memory
**          Pointer to the variable to receive the logical pointer to the
**          allocated area.  Can be gcvNULL.
*/
gceSTATUS
gcoHAL_AllocateLinearVideoMemory(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctUINT Size,
    IN gctUINT Alignment,
    IN gcePOOL Pool,
    OUT gctUINT32 * Node,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    )
{
    gceSTATUS status;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_AllocateLinearVideoMemory);
#endif
    /* Call down to gcoHARDWARE. */
    status = gcoVGHARDWARE_AllocateLinearVideoMemory(
        gcvNULL, Size, Alignment, Pool, gcvALLOC_FLAG_NONE, Node, Address, Memory
        );
#if GC355_PROFILER
   	vghalLEAVESUBAPI(gcoHAL_AllocateLinearVideoMemory);
#endif
    /* Return the status. */
    return status;
}
/*******************************************************************************
**
**  gcoHAL_FreeVideoMemory
**
**  Free linear video memory allocated with gcoHAL_AllocateLinearVideoMemory.
**  Assumed that the memory is locked.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**      gctUINT32 Node
**          Node of the allocated memory to free.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_FreeVideoMemory(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctUINT32 Node
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_FreeVideoMemory);
#endif
    /* Call down to gcoHARDWARE. */
    status = gcoVGHARDWARE_FreeVideoMemory(gcvNULL, Node);
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_FreeVideoMemory);
#endif
    /* Return the status. */
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ScheduleVideoMemory
**
**  Schedule to free linear video memory allocated with
**  gcoHAL_AllocateLinearVideoMemory. Assumed that the memory is locked.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**      gctUINT32 Node
**          Node of the allocated memory to free.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoHAL_ScheduleVideoMemory(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctUINT32 Node
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_ScheduleVideoMemory);
#endif
    /* Call down to gcoHARDWARE. */
    status = gcoVGHARDWARE_ScheduleVideoMemory(gcvNULL, Node, gcvTRUE);
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_ScheduleVideoMemory);
#endif
    /* Return the status. */
    return status;
}

/*******************************************************************************
**
**  gcoHAL_SplitAddress
**
**  Split a hardware specific memory address into a pool and offset.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**      gctUINT32 Address
**          Address in hardware specific format.
**
**  OUTPUT:
**
**      gcePOOL * Pool
**          Pointer to a variable that will hold the pool type for the address.
**
**      gctUINT32 * Offset
**          Pointer to a variable that will hold the offset for the address.
*/
gceSTATUS
gcoHAL_SplitAddress(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctUINT32 Address,
    OUT gcePOOL * Pool,
    OUT gctUINT32 * Offset
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_SplitAddress);
#endif
    /* Call hardware library. */
    status = gcoVGHARDWARE_SplitAddress(gcvNULL, Address, Pool, Offset);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoHAL_SplitAddress);
#endif
    return status;
}

/*******************************************************************************
**
**  gcoHAL_CombineAddress
**
**  Combine pool and offset into a harwdare address.
**
**  INPUT:
**
**      gcoHALE Hardware
**          Pointer to the gcoHAL object.
**
**      gcePOOL Pool
**          Pool type for the address.
**
**      gctUINT32 Offset
**          Offset for the address.
**
**  OUTPUT:
**
**      gctUINT32 * Address
**          Pointer to the combined address.
*/
gceSTATUS
gcoHAL_CombineAddress(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcePOOL Pool,
    IN gctUINT32 Offset,
    OUT gctUINT32 * Address
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_CombineAddress);
#endif
    /* Call hardware library. */
    status = gcoVGHARDWARE_CombineAddress(gcvNULL, Pool, Offset, Address);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoHAL_CombineAddress);
#endif
    return status;
}

/*******************************************************************************
**
**  gcoHAL_QueryCommandBuffer
**
**  Query command buffer attributes.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**  OUTPUT:
**
**      gcsCOMMAND_BUFFER_INFO_PTR Information
**          Pointer to the information structure to receive buffer attributes.
*/
gceSTATUS
gcoHAL_QueryCommandBuffer(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    OUT gcsCOMMAND_BUFFER_INFO_PTR Information
    )
{
    gceSTATUS status;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_QueryCommandBuffer);
#endif
    /* Copy the command buffer information. */
    status = gcoVGHARDWARE_QueryCommandBuffer(
        gcvNULL, Information
        );
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_QueryCommandBuffer);
#endif
    /* Return status. */
    return status;
}

/*******************************************************************************
**
**  gcoHAL_GetAlignedSurfaceSize
**
**  Align the specified size accordingly to the hardware requirements.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to an gcoHAL object.
**
**      gceSURF_TYPE Type
**          Type of surface to create.
**
**      gctUINT32_PTR Width
**          Pointer to the width to be aligned.  If 'Width' is gcvNULL, no width
**          will be aligned.
**
**      gctUINT32_PTR Height
**          Pointer to the height to be aligned.  If 'Height' is gcvNULL, no height
**          will be aligned.
**
**  OUTPUT:
**
**      gctUINT32_PTR Width
**          Pointer to a variable that will receive the aligned width.
**
**      gctUINT32_PTR Height
**          Pointer to a variable that will receive the aligned height.
*/
gceSTATUS
gcoHAL_GetAlignedSurfaceSize(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceSURF_TYPE Type,
    IN OUT gctUINT32_PTR Width,
    IN OUT gctUINT32_PTR Height
    )
{
    gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_GetAlignedSurfaceSize);
#endif
    /* Align the sizes. */
    status = gcoVGHARDWARE_AlignToTile(
        gcvNULL, Type, Width, Height
        );
#if GC355_PROFILER
	vghalLEAVESUBAPI(gcoHAL_GetAlignedSurfaceSize);
#endif
    /* Return the status. */
    return status;
}

/*******************************************************************************
**
**  gcoHAL_ReserveTask
**
**  Allocate a task to be perfomed when signaled from the specified block.
**
**  INPUT:
**
**      gcoHAL Hal
**          Pointer to the gcoHAL object.
**
**      gceBLOCK Block
**          Specifies the hardware block that should generate the event to
**          perform the task.
**
**      gctUINT TaskCount
**          The number of tasks to add.
**
**      gctSIZE_T Bytes
**          The size of the task in bytes.
**
**  OUTPUT:
**
**      gctPOINTER * Memory
**          Pointer to the reserved task to be filled in by the caller.
*/
gceSTATUS
gcoHAL_ReserveTask(
    IN gcoHAL Hal,
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceBLOCK Block,
    IN gctUINT TaskCount,
    IN gctUINT32 Bytes,
    OUT gctPOINTER * Memory
    )
{
	gceSTATUS status;
#if GC355_PROFILER
	gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoHAL_ReserveTask);
#endif
    /* Allocate an event. */
    status = gcoVGHARDWARE_ReserveTask(
        gcvNULL, Block, TaskCount, Bytes, Memory
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoHAL_ReserveTask);
#endif
    /* Return the status. */
    return status;
}

/******************************************************************************\
********************************** gcoVG Object ********************************
\******************************************************************************/

gctBOOL
gcoVG_IsMaskSupported(
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceSURF_FORMAT Format
    )
{
	gctBOOL result;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_IsMaskSupported);
#endif
    result = gcoVGHARDWARE_IsMaskSupported(Format);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_IsMaskSupported);
#endif
    /* Return the status. */
    return result;
}

gctBOOL
gcoVG_IsTargetSupported(
#if GC355_PROFILER
	IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceSURF_FORMAT Format
    )
{
	gctBOOL result;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_IsTargetSupported);
#endif
    result = gcoVGHARDWARE_IsTargetSupported(Format);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_IsTargetSupported);
#endif
    /* Return the status. */
    return result;
}

gctBOOL
gcoVG_IsImageSupported(
#if GC355_PROFILER
    IN gcoVG Vg,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceSURF_FORMAT Format
    )
{
	gctBOOL result;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_IsImageSupported);
#endif
    result = gcoVGHARDWARE_IsImageSupported(Format);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_IsImageSupported);
#endif
    /* Return the status. */
    return result;
}

gctUINT8 gcoVG_PackColorComponent(
#if GC355_PROFILER
	gcoVG Vg,
    gcsPROFILERFUNCNODE *DList,
    gctUINT TreeDepth,
    gctUINT saveLayerTreeDepth,
    gctUINT varTreeDepth,
#endif
    gctFLOAT Value
    )
{
	gctUINT8 result;
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_PackColorComponent);
#endif
    result = gcoVGHARDWARE_PackColorComponent(Value);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_PackColorComponent);
#endif
    /* Return the status. */
    return result;
}


/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   The gcoVG object constructor.
**
**  gcoVG_Construct constructs a new gcoVG object if the running hardware
**  has an OpenVG pipe.
**
**  @param[in]  Hal     Pointer to an gcoHAL object.
**  @param[out] Vg      Pointer to a variable receiving the gcoVG object
**                      pointer.
*/
gceSTATUS
gcoVG_Construct(
    IN gcoHAL Hal,
    OUT gcoVG * Vg
    )
{
    gceSTATUS status, last;
    gcoVG vg = gcvNULL;

    gcmHEADER_ARG("Hal=0x%x Vg=0x%x ",
                  Hal, Vg);
    gcmVERIFY_ARGUMENT(Vg != gcvNULL);

    do
    {
#if !gcdENABLE_SHARE_TESSELLATION
        gctUINT i;
#endif

        /* Make sure the hardware supports VG. */
        if (!gcoVGHARDWARE_IsFeatureAvailable(gcvNULL, gcvFEATURE_PIPE_VG))
        {
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* Allocate the gcoVG structure. */
        gcmERR_BREAK(gcoOS_Allocate(
            gcvNULL, gcmSIZEOF(struct _gcoVG), (gctPOINTER *) &vg
            ));

        /* Reset everything to 0. */
        gcoOS_ZeroMemory(vg, sizeof(struct _gcoVG));

        /* Initialize the object. */
        vg->object.type = gcvOBJ_VG;

        /* Copy object pointers. */
        vg->hal = Hal;
        vg->os  = gcvNULL;
        vg->hw  = gcvNULL;

        /* Query VG version support. */
        vg->vg20 = gcoVGHARDWARE_IsFeatureAvailable(
            vg->hw, gcvFEATURE_VG20
            );

        vg->vg21 = gcoVGHARDWARE_IsFeatureAvailable(
            vg->hw, gcvFEATURE_VG21
            );

        /* Set default gcoVG states. */
        vg->renderQuality = gcvVG_NONANTIALIASED;

        /* Query path data command buffer attributes. */
        gcmERR_BREAK(gcoVGHARDWARE_QueryPathStorage(vg->hw, &vg->pathInfo));

        /* Query generic command buffer attributes. */
        gcmERR_BREAK(gcoVGHARDWARE_QueryCommandBuffer(vg->hw, &vg->bufferInfo));

#if gcdENABLE_SHARE_TESSELLATION
        vg->tsBuffer = gcvNULL;
#else
        /* Reset the tesselation buffers. */
        vg->tsBufferIndex = gcmCOUNTOF(vg->tsBuffer) - 1;

        for (i = 0; i < gcmCOUNTOF(vg->tsBuffer); i += 1)
        {
            vg->tsBuffer[i].node = 0;
            vg->tsBuffer[i].allocatedSize = 0;
        }
#endif

#if gcdGC355_MEM_PRINT
#ifdef LINUX
        vg->tsCurMemSize = 0;
        vg->tsMaxMemSize = 0;
#endif
#endif

        /* Return gcoVG object pointer. */
        *Vg = vg;

        /* Success. */
        gcmFOOTER_ARG("*Vg=0x%x", *Vg);
        return gcvSTATUS_OK;
    }
    while (gcvFALSE);

    if (vg != gcvNULL)
    {
        /* Free the gcoVG structure. */
        gcmCHECK_STATUS(gcmOS_SAFE_FREE(gcvNULL, vg));
    }

    /* Return the error. */
    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   The gcoVG object destructor.
**
**  gcoVG_Destroy destroys a previously constructed gcoVG object.
**
**  @param[in]  Vg      Pointer to the gcoVG object that needs to be destroyed.
*/
gceSTATUS
gcoVG_Destroy(
    IN gcoVG Vg
#if GC355_PROFILER
    ,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth
#endif
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x",
                  Vg);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

    do
    {
#if gcdENABLE_SHARE_TESSELLATION
        if (Vg->tsBuffer != gcvNULL)
        {
            gcmERR_BREAK(gcoOS_AcquireMutex(Vg->os, g_tsMutex, gcvINFINITE));
#if GC355_PROFILER
            status = _TessellationBufferRelease(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Vg->tsBuffer);
#else
            status = _TessellationBufferRelease(Vg, Vg->tsBuffer);
#endif
            gcmVERIFY_OK(gcoOS_ReleaseMutex(Vg->os, g_tsMutex));

            gcmERR_BREAK(status);
        }
#else
        gctUINT i;

        /* Destroy tesselation buffers. */
        for (i = 0; i < gcmCOUNTOF(Vg->tsBuffer); i += 1)
        {
#if GC355_PROFILER
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, &Vg->tsBuffer[i]));
#else
            gcmERR_BREAK(_FreeTessellationBuffer(Vg, &Vg->tsBuffer[i]));
#endif
        }
#endif
        /* Destroy scissor surface. */
        if (Vg->scissor != gcvNULL)
        {
            gcmERR_BREAK(gcoSURF_Destroy(Vg->scissor));
            Vg->scissor = gcvNULL;
        }

        /* Mark the object as unknown. */
        Vg->object.type = gcvOBJ_UNKNOWN;

#if gcdGC355_MEM_PRINT
#ifdef LINUX
        gcmPRINT("04) Tessellation buffer: %d \n", Vg->tsMaxMemSize);
#endif
#endif

        /* Free the gcoVG structure. */
        gcmERR_BREAK(gcmOS_SAFE_FREE(Vg->os, Vg));
    }
    while (gcvFALSE);

    gcmFOOTER_NO();
    /* Return status. */
    return gcvSTATUS_OK;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the current render target.
**
**  Specify the current render target for the gcoVG object to render into.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Target  Pointer to a gcoSURF object that defines the target.
*/
gceSTATUS
gcoVG_SetTarget(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Target
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Target=0x%x ",
                  Vg, Target);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetTarget);
#endif

    do
    {
        /* Is there a target currently set? */
        if (Target == gcvNULL)
        {
            /* Call hardware layer to set the target. */
            gcmERR_BREAK(gcoVGHARDWARE_SetVgTarget(
                Vg->hw, gcvNULL
                ));

            /* Reset the target. */
            if (Vg->target != gcvNULL)
            {
                gcoSURF_Destroy(Vg->target);
            }
            Vg->target       = gcvNULL;
            Vg->targetWidth  = 0;
            Vg->targetHeight = 0;
        }
        else
        {
            /* Call hardware layer to set the target. */
            gcmERR_BREAK(gcoVGHARDWARE_SetVgTarget(
                Vg->hw, &Target->info
                ));

            /* Set target. */
            if (Vg->target != gcvNULL)
            {
                gcoSURF_Destroy(Vg->target);
            }
            gcoSURF_ReferenceSurface(Target);
            Vg->target       = Target;
            Vg->targetWidth  = Target->info.rect.right;
            Vg->targetHeight = Target->info.rect.bottom;
        }
    }
    while (gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetTarget);
#endif

    gcmFOOTER();
    /* Return the error. */
    return status;
}

gceSTATUS
gcoVG_UnsetTarget(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Surface
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Vg=0x%x Surface=0x%x ",
                  Vg, Surface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_UnSetTarget);
#endif

    /* Only unset surface if it is the current render target. */
    if (Vg->target == Surface)
    {
#if GC355_PROFILER
        status = gcoVG_SetTarget(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, gcvNULL);
#else
        status = gcoVG_SetTarget(Vg, gcvNULL);
#endif
    }

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_UnSetTarget);
#endif
    gcmFOOTER();
    /* Return the status. */
    return status;
}

gceSTATUS
gcoVG_SetUserToSurface(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT UserToSurface[9]
    )
{
    gcmHEADER_ARG("Vg=0x%x UserToSurface=0x%x ",
                  Vg, UserToSurface);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
    gcmVERIFY_ARGUMENT(UserToSurface != gcvNULL);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetUserToSurface);
#endif

    /* Copy first row. */
    Vg->userToSurface[0] = UserToSurface[0];
    Vg->userToSurface[1] = UserToSurface[3];
    Vg->userToSurface[2] = UserToSurface[6];

    /* Copy second row. */
    Vg->userToSurface[3] = UserToSurface[1];
    Vg->userToSurface[4] = UserToSurface[4];
    Vg->userToSurface[5] = UserToSurface[7];

    /* Copy third row. */
    Vg->userToSurface[6] = UserToSurface[2];
    Vg->userToSurface[7] = UserToSurface[5];
    Vg->userToSurface[8] = UserToSurface[8];

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetUserToSurface);
#endif
    gcmFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gcoVG_SetSurfaceToImage(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT SurfaceToImage[9]
    )
{
    gcmHEADER_ARG("Vg=0x%x UserToSurface=0x%x ",
                  Vg, SurfaceToImage);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
    gcmVERIFY_ARGUMENT(SurfaceToImage != gcvNULL);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetSurfaceToImage);
#endif
    /* Copy first row. */
    Vg->surfaceToImage[0] = SurfaceToImage[0];
    Vg->surfaceToImage[1] = SurfaceToImage[3];
    Vg->surfaceToImage[2] = SurfaceToImage[6];

    /* Copy second row. */
    Vg->surfaceToImage[3] = SurfaceToImage[1];
    Vg->surfaceToImage[4] = SurfaceToImage[4];
    Vg->surfaceToImage[5] = SurfaceToImage[7];

    /* Copy third row. */
    Vg->surfaceToImage[6] = SurfaceToImage[2];
    Vg->surfaceToImage[7] = SurfaceToImage[5];
    Vg->surfaceToImage[8] = SurfaceToImage[8];

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetSurfaceToImage);
#endif
    gcmFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Enable or disable masking.
**
**  Enable or disable the masking.  When masking is enabled, make sure to set
**  a masking surface.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Enable  gcvTRUE to enable masking or gcvFALSE to disable
**                      masking.  Masking is by default disabled.
*/
gceSTATUS
gcoVG_EnableMask(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Enable=0x%x ",
                  Vg, Enable);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_EnableMask);
#endif
    /* Call gcoVGHARDWARE to enable masking. */
    status = gcoVGHARDWARE_EnableMask(Vg->hw, Enable);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_EnableMask);
#endif
    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the current mask buffer.
**
**  Specify the current mask buffer to be used by the gcoVG object.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Mask    Pointer to a gcoSURF object that defines the mask.
*/
gceSTATUS
gcoVG_SetMask(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Mask
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Mask=0x%x ",
                  Vg, Mask);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetMask);
#endif

    do
    {
        if (Vg->mask == Mask)
        {
            /* The mask did not change, do nothing. */
            status = gcvSTATUS_OK;
            break;
        }

        /* Is there a mask currently set? */
        if (Vg->mask != gcvNULL)
        {
            /* Unlock the current mask. */
            gcmERR_BREAK(gcoSURF_Unlock(
                Vg->mask, gcvNULL
                ));

            /* Reset the mask. */
            Vg->mask = gcvNULL;
        }

        /* Mask reset? */
        if (Mask == gcvNULL)
        {
            /* Success. */
            status = gcvSTATUS_OK;
            break;
        }

        /* Make sure we have a good surface. */
        gcmVERIFY_OBJECT(Mask, gcvOBJ_SURF);
#if GC355_PROFILER
        /* Test if surface format is supported. */
        if (!gcoVG_IsMaskSupported(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Mask->info.format))
#else
        if (!gcoVG_IsMaskSupported(Mask->info.format))
#endif
        {
            /* Format not supported. */
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }

        /* Set mask. */
        Vg->mask = Mask;

        /* Lock the surface. */
        gcmERR_BREAK(gcoSURF_Lock(
            Mask, gcvNULL, gcvNULL
            ));

        /* Program hardware. */
        gcmERR_BREAK(gcoVGHARDWARE_SetVgMask(
            Vg->hw, &Mask->info
            ));

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetMask);
#endif
    gcmFOOTER();
    /* Return the error. */
    return status;
}

gceSTATUS
gcoVG_UnsetMask(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Surface
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("Vg=0x%x Surface=0x%x ",
                  Vg, Surface);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
    gcmVERIFY_OBJECT(Surface, gcvOBJ_SURF);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_UnsetMask);
#endif
    /* Only unset surface if it is the current mask. */
    if (Vg->mask == Surface)
    {
#if GC355_PROFILER
        status = gcoVG_SetMask(Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, gcvNULL);
#else
        status = gcoVG_SetMask(Vg, gcvNULL);
#endif
    }

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_UnsetMask);
#endif

    gcmFOOTER();
    /* Return the status. */
    return status;
}

gceSTATUS
gcoVG_FlushMask(
    IN gcoVG Vg
#if GC355_PROFILER
    ,
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth
#endif
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x",
                  Vg);
    /* Verify the argument. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_FlushMask);
#endif
    /* Flush mask. */
    status = gcoVGHARDWARE_FlushVgMask(Vg->hw);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_FlushMask);
#endif
    gcmFOOTER();
    /* Return the status. */
    return status;
}


/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Enable or disable scissoring.
**
**  Enable or disable scissoring.  When scissoring is enabled, make sure to set
**  the scissor regions.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Enable  gcvTRUE to enable scissoring or gcvFALSE to disable
**                      scissoring.  Scissoring is by default disabled.
*/
gceSTATUS
gcoVG_EnableScissor(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Enable=0x%x ",
                  Vg, Enable);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_EnableScissor);
#endif
    /* Call gcoVGHARDWARE to enable scissoring. */
    status = gcoVGHARDWARE_EnableScissor(Vg->hw, Enable);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_EnableScissor);
#endif

    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the scissor rectangles.
**
**  Set the scissor rectangles.  If there are no rectangles specified, the
**  scissoring region is empty an nothing will be rendered when scissoring is
**  enabled.
**
**  @param[in]  Vg              Pointer to the gcoVG object.
**  @param[in]  RectangleCount  Number of rectangles to be used as the
**                              scissoring region.
**  @Param[in]  Rectangles      Pointer to the rectangles to be used as the
**                              scissoring region.
*/
gceSTATUS
gcoVG_SetScissor(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctSIZE_T RectangleCount,
    IN gcsVG_RECT_PTR Rectangles
    )
{
    gceSTATUS status, last;
    gctUINT8_PTR bits = gcvNULL;

    gcmHEADER_ARG("Vg=0x%x RectangleCount=0x%x  Rectangles=0x%x",
                  Vg, RectangleCount, Rectangles);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

    if (RectangleCount > 0)
    {
        gcmVERIFY_ARGUMENT(Rectangles != gcvNULL);
    }
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetScissor);
#endif

    do
    {
        /* Free any current scissor surface. */
        if (Vg->scissor != gcvNULL)
        {
            gcmERR_GOTO(gcoSURF_Destroy(Vg->scissor));
            Vg->scissor = gcvNULL;
        }

        /* Construct a new scissor surface. */
        gcmERR_GOTO(gcoSURF_Construct(
            Vg->hal,
            Vg->targetWidth,
            Vg->targetHeight,
            1,
            gcvSURF_SCISSOR,
            gcvSURF_A1,
            gcvPOOL_DEFAULT,
            &Vg->scissor
            ));

        /* Lock scissor surface. */
        gcmERR_GOTO(gcoSURF_Lock(
            Vg->scissor,
            &Vg->scissorAddress,
            &Vg->scissorBits
            ));

        bits = (gctUINT8_PTR) Vg->scissorBits;

        /* Zero the entire surface. */
        gcoOS_ZeroMemory(
            bits,
            Vg->scissor->info.stride * Vg->targetHeight
            );

        /* Loop while there are rectangles to process. */
        while (RectangleCount > 0)
        {
            gctINT left, right, top, bottom;

            /* Intersect rectangle with the surface. */
            left   = gcmMAX(Rectangles->x, 0);
            right  = gcmMIN(Rectangles->x + Rectangles->width,  Vg->targetWidth);
            top    = gcmMAX(Rectangles->y, 0);
            bottom = gcmMIN(Rectangles->y + Rectangles->height, Vg->targetHeight);

            /* Test for valid scissor region. */
            if ((left < right) && (top < bottom))
            {
                /* Compute left and right byte. */
                gctINT leftByte  = left        >> 3;
                gctINT rightByte = (right - 1) >> 3;

                /* Compute left and right mask. */
                gctUINT8 leftMask  = 0xFF << (left & 7);
                gctUINT8 rightMask = 0xFF >> (((right - 1) & 7) ^ 7);

                /* Start at top line of region. */
                gctUINT32 offset = top * Vg->scissor->info.stride;

                /* See if left and right bytes overlap. */
                if (leftByte == rightByte)
                {
                    while (top++ < bottom)
                    {
                        /* Allow left and right pixels. */
                        bits[offset + leftByte] |= leftMask & rightMask;

                        /* Next line. */
                        offset += Vg->scissor->info.stride;
                    }
                }

                else
                {
                    while (top++ < bottom)
                    {
                        /* Allow left pixels. */
                        bits[offset + leftByte] |= leftMask;

                        /* See if we have any middle bytes to fill. */
                        if (leftByte + 1 < rightByte)
                        {
                            /* Allow all middle pixels. */
                            gcoOS_MemFill(
                                &bits[offset + leftByte + 1],
                                0xFF,
                                rightByte - leftByte - 1
                                );
                        }

                        /* Allow right pixels. */
                        bits[offset + rightByte] |= rightMask;

                        /* Next line. */
                        offset += Vg->scissor->info.stride;
                    }
                }
            }

            /* Next rectangle. */
            ++Rectangles;
            --RectangleCount;
        }

        /* Program the scissors to the hardware. */
        gcmERR_GOTO(gcoVGHARDWARE_SetScissor(
            Vg->hw,
            Vg->scissorAddress,
            Vg->scissor->info.stride
            ));

        /* Unlock the scissor surface. */
        gcmVERIFY_OK(gcoSURF_Unlock(
            Vg->scissor, bits
            ));

        /* Success. */
        status = gcvSTATUS_OK;

ErrorHandler:
        if (bits != gcvNULL)
        {
            /* Unlock the scissor surface. */
            gcmCHECK_STATUS(gcoSURF_Unlock(
                Vg->scissor, bits
                ));
        }
    }
    while (gcvFALSE);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetScissor);
#endif
    gcmFOOTER();
    /* Return the error. */
    return status;
}

gceSTATUS
gcoVG_SetTileFillColor(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT Red,
    IN gctFLOAT Green,
    IN gctFLOAT Blue,
    IN gctFLOAT Alpha
    )
{
    gcmHEADER_ARG("Vg=0x%x Red=0x%x  Green=0x%x Blue=0x%x  Alpha=0x%x",
                  Vg, Red, Green, Blue, Alpha);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetTileFillColor);
#endif
    /* Set tile fill color. */
    Vg->tileFillColor = gcoVGHARDWARE_PackColor32(Red, Green, Blue, Alpha);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetTileFillColor);
#endif
    gcmFOOTER_NO();
    /* Success. */
    return gcvSTATUS_OK;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Enable or disable color transformation.
**
**  Enable or disable color transformation.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Enable  gcvTRUE to enable color transformation or gcvFALSE to
**                      disable color transformation.  Color transformation is
**                      by default disabled.
*/
gceSTATUS
gcoVG_EnableColorTransform(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Enable=0x%x",
                  Vg, Enable);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_EnableColorTransform);
#endif
    /* Call gcoVGHARDWARE to enable color transformation. */
    status = gcoVGHARDWARE_EnableColorTransform(Vg->hw, Enable);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_EnableColorTransform);
#endif

    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the color transformation values.
**
**  Set the color transformation values.
**
**  @param[in]  Vg              Pointer to the gcoVG object.
**  @param[in]  ColorTransform  Pointer to an array of four scale and four bias
**                              values for each color channel, in the order:
**                              red, green, blue, alpha.
*/
gceSTATUS
gcoVG_SetColorTransform(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT ColorTransform[8]
    )
{
    gctINT i;
    gctFLOAT_PTR scale, offset;
    gctFLOAT clampedScale[4], clampedOffset[4];
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x ColorTransform=0x%x",
                  Vg, ColorTransform);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
    gcmVERIFY_ARGUMENT(ColorTransform != gcvNULL);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetColorTransform);
#endif
    /* Set initial pointers. */
    scale  = &ColorTransform[0];
    offset = &ColorTransform[4];

    /* Clamp scale and offset values. */
    for (i = 0; i < 4; i += 1)
    {
        clampedScale[i]  = gcmCLAMP(scale[i], -127.0f, 127.0f);
        clampedOffset[i] = gcmCLAMP(offset[i],  -1.0f,   1.0f);
    }

    /* Send scale and offset to hardware. */
    status = gcoVGHARDWARE_SetColorTransform(Vg->hw, clampedScale, clampedOffset);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetColorTransform);
#endif
    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the paint to be used by the next rendering.
**
**  Program the hardware with the specified color.
**
**  @param[in]  Vg          Pointer to the gcoVG object.
**  @param[in]  Red         The value of the red channel.
**  @param[in]  Green       The value of the green channel.
**  @param[in]  Blue        The value of the blue channel.
**  @param[in]  Alpha       The value of the alpha channel.
*/
gceSTATUS
gcoVG_SetSolidPaint(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctUINT8 Red,
    IN gctUINT8 Green,
    IN gctUINT8 Blue,
    IN gctUINT8 Alpha
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Red=0x%x Green=0x%x Blue=0x%x Alpha=0x%x",
                  Vg, Red, Green, Blue, Alpha);

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetSolidPaint);
#endif
    /* Set solid paint. */
    status = gcoVGHARDWARE_SetPaintSolid(
        Vg->hw, Red, Green, Blue, Alpha
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetSolidPaint);
#endif
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoVG_SetLinearPaint(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT Constant,
    IN gctFLOAT StepX,
    IN gctFLOAT StepY
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Constant=0x%x StepX=0x%x StepY=0x%x",
                  Vg, Constant, StepX, StepY);
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetLinearPaint);
#endif
    /* Set solid paint. */
    status =  gcoVGHARDWARE_SetPaintLinear(
        Vg->hw, Constant, StepX, StepY
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetLinearPaint);
#endif
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoVG_SetRadialPaint(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT LinConstant,
    IN gctFLOAT LinStepX,
    IN gctFLOAT LinStepY,
    IN gctFLOAT RadConstant,
    IN gctFLOAT RadStepX,
    IN gctFLOAT RadStepY,
    IN gctFLOAT RadStepXX,
    IN gctFLOAT RadStepYY,
    IN gctFLOAT RadStepXY
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x LinConstant=0x%x LinStepX=0x%x LinStepY=0x%x"
                "RadConstant=0x%x RadStepX=0x%x RadStepY=0x%x RadStepXX=0x%x RadStepXY=0x%x",
                  Vg, LinConstant, LinStepX, LinStepY, RadConstant, RadStepX, RadStepY, RadStepXX, RadStepXY
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetRadialPaint);
#endif
    /* Set solid paint. */
    status = gcoVGHARDWARE_SetPaintRadial(
        Vg->hw,
        LinConstant,
        LinStepX,
        LinStepY,
        RadConstant,
        RadStepX,
        RadStepY,
        RadStepXX,
        RadStepYY,
        RadStepXY
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetRadialPaint);
#endif
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoVG_SetPatternPaint(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctFLOAT UConstant,
    IN gctFLOAT UStepX,
    IN gctFLOAT UStepY,
    IN gctFLOAT VConstant,
    IN gctFLOAT VStepX,
    IN gctFLOAT VStepY,
    IN gctBOOL Linear
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x UConstant=0x%x UStepX=0x%x UStepY=0x%x"
                "VConstant=0x%x VStepX=0x%x VStepY=0x%x Linear=0x%x",
                  Vg, UConstant, UStepX, UStepY, VConstant, VStepX, VStepY, Linear
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetPatternPaint);
#endif
    /* Set solid paint. */
    status = gcoVGHARDWARE_SetPaintPattern(
        Vg->hw,
        UConstant, UStepX, UStepY,
        VConstant, VStepX, VStepY,
        Linear
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetPatternPaint);
#endif
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoVG_SetColorRamp(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF ColorRamp,
    IN gceTILE_MODE ColorRampSpreadMode
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x ColorRamp=0x%x ColorRampSpreadMode=0x%x",
                  Vg, ColorRamp, ColorRampSpreadMode
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetColorRamp);
#endif
    /* Set solid paint. */
    status = gcoVGHARDWARE_SetPaintImage(
        Vg->hw,
        &ColorRamp->info,
        ColorRampSpreadMode,
        gcvFILTER_LINEAR,
        Vg->tileFillColor
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetColorRamp);
#endif
    gcmFOOTER();
    return status;

}

gceSTATUS
gcoVG_SetPattern(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctINT32 width,
    IN gctINT32 height,
    IN gcoSURF Pattern,
    IN gceTILE_MODE TileMode,
    IN gceIMAGE_FILTER Filter
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Pattern=0x%x TileMode=0x%x Filter=0x%x",
                  Vg, Pattern, TileMode, Filter
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetPattern);
#endif
    /* Set solid paint. */
    Pattern->info.rect.right  = width;
    Pattern->info.rect.bottom = height;
    status = gcoVGHARDWARE_SetPaintImage(
        Vg->hw,
        &Pattern->info,
        TileMode,
        Filter,
        Vg->tileFillColor
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetPattern);
#endif
    gcmFOOTER();
    return status;
}


/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the blend mode.
**
**  Set the blend mode to be used by the next rendering.
**
**  @param[in]  Vg          Pointer to the gcoVG object.
**  @param[in]  Mode        Blend mode.
*/
gceSTATUS
gcoVG_SetBlendMode(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceVG_BLEND Mode
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Mode=0x%x",
                  Vg, Mode
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetBlendMode);
#endif
    /* Set blend mode in hardware. */
    status = gcoVGHARDWARE_SetVgBlendMode(Vg->hw, Mode);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetBlendMode);
#endif
    gcmFOOTER();
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Clear the current render target.
**
**  The current render target will be cleared using the specified color.  The
**  color is specified in pre-multiplied sRGB format.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Origin  Pointer to the origin of the rectangle to be cleared.
**  @param[in]  Size    Pointer to the size of the rectangle to be cleared.
*/
gceSTATUS
gcoVG_Clear(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctINT X,
    IN gctINT Y,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x X=0x%x Width=0x%x Height=0x%x",
                  Vg, X, Width,  Height
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_Clear);
#endif
    /* Clear rectangle. */
    status = gcoVGHARDWARE_VgClear(Vg->hw, X, Y, Width, Height);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_Clear);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

#if gcdMOVG
static gctBOOL
    _Transform(
    IN gctFLOAT Value[2],
    IN gctFLOAT Matrix[9],
    OUT gctFLOAT result[2])
{
    if (Matrix == NULL) {
        result[0] = Value[0];
        result[1] = Value[1];

        return gcvTRUE;
    }

    result[0] = Value[0] * Matrix[0 * 3 + 0] + Value[1] * Matrix[0 * 3 + 1] + Matrix[0 * 3 + 2];
    result[1] = Value[0] * Matrix[1 * 3 + 0] + Value[1] * Matrix[1 * 3 + 1] + Matrix[1 * 3 + 2];

    return gcvTRUE;
}
#endif

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Draw a path with the current settings.
**
**  Draw the specified path into the current render target.
**  All current states will take effect, including blending, paint, scissoring,
**  masking, and color transformation.  The path will be offset by the speficied
**  value to lower the retesselation of paths that only "move" on the screen.
**
**  @param[in]  Vg                  Pointer to the gcoVG object.
**  @param[in]  PathData            Pointer to the path data buffer.
**  @param[in]  Scale               Path scale.
**  @param[in]  Bias                Path bias.
**  @param[in]  SoftwareTesselation Software vs. hardware tesselation.
*/

gceSTATUS
gcoVG_DrawPath(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcsPATH_DATA_PTR PathData,
    IN gctFLOAT Scale,
    IN gctFLOAT Bias,
#if gcdMOVG
    IN gctUINT32 Width,
    IN gctUINT32 Height,
    IN gctFLOAT *Bounds,
#endif
    IN gctBOOL SoftwareTesselation
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x PathData=0x%x Scale=0x%x Bias=0x%x SoftwareTesselation=0x%x",
                  Vg, PathData, Scale,  Bias, SoftwareTesselation
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_DrawPath);
#endif

    do
    {
        gcsTESSELATION_PTR tessellationBuffer = gcvNULL;

#if gcdMOVG
        gctINT tsWidth, tsHeight;
        gctINT x, y;
        gctFLOAT minx, miny, maxx, maxy;  /* Bounding box of the path. */
        gctFLOAT tBounds[4][2];    /* LEFT, RIGHT, BOTTOM, TOP. */
        gctFLOAT points[4][2];      /* Four corners of the bounding. */

        /* Get the pointer to the current tesselation buffer. */
        if (!SoftwareTesselation)
        {
#if GC355_PROFILER
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Width, Height,
                &tessellationBuffer, &tsWidth, &tsHeight
                ));
#else
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Width, Height,
                &tessellationBuffer, &tsWidth, &tsHeight
                ));
#endif
        }
        else
        {
#if GC355_PROFILER
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Vg->targetWidth, Vg->targetHeight,
                &tessellationBuffer, &tsWidth, &tsHeight
                ));
#else
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Vg->targetWidth, Vg->targetHeight,
                &tessellationBuffer, &tsWidth, &tsHeight
                ));
#endif
            tsWidth = Vg->targetWidth;
            tsHeight = Vg->targetHeight;
        }
#else
#if GC355_PROFILER
        /* Get the pointer to the current tesselation buffer. */
        gcmERR_BREAK(_GetTessellationBuffer(
            Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, &tessellationBuffer
            ));
#else
        gcmERR_BREAK(_GetTessellationBuffer(
            Vg, &tessellationBuffer
            ));
#endif
#endif

        /* Set path type. In VG version before 2.0 it was not possible to change
           the type of the path data within a path, therefore the data within
           a path had to be of the same type; we set the type here. With VG 2.0
           and above the path objects carry data types within them. */
        gcmERR_BREAK(gcoVGHARDWARE_SetPathDataType(
            Vg->hw, PathData->dataType
            ));

#if gcdMOVG
        if (!SoftwareTesselation)
        {
            /* Transform the bounds. */
            points[0][0] = Bounds[0];
            points[0][1] = Bounds[2];
            points[1][0] = Bounds[1];
            points[1][1] = Bounds[2];
            points[2][0] = Bounds[1];
            points[2][1] = Bounds[3];
            points[3][0] = Bounds[0];
            points[3][1] = Bounds[3];
            _Transform(points[0], Vg->userToSurface, tBounds[0]);
            _Transform(points[1], Vg->userToSurface, tBounds[1]);
            _Transform(points[2], Vg->userToSurface, tBounds[2]);
            _Transform(points[3], Vg->userToSurface, tBounds[3]);
            minx = maxx = tBounds[0][0];
            miny = maxy = tBounds[0][1];
            minx = gcmMIN(minx, gcmMIN(tBounds[1][0], gcmMIN(tBounds[2][0], tBounds[3][0])));
            maxx = gcmMAX(maxx, gcmMAX(tBounds[1][0], gcmMAX(tBounds[2][0], tBounds[3][0])));
            miny = gcmMIN(miny, gcmMIN(tBounds[1][1], gcmMIN(tBounds[2][1], tBounds[3][1])));
            maxy = gcmMAX(maxy, gcmMAX(tBounds[1][1], gcmMAX(tBounds[2][1], tBounds[3][1])));

            /* Clip to render target area. */
            if (minx < 0.0f)
                minx = 0.0f;
            if (miny < 0.0f)
                miny = 0.0f;
            if (maxx > (gctFLOAT)Vg->targetWidth)
                maxx = (gctFLOAT)Vg->targetWidth;
            if (maxy > (gctFLOAT)Vg->targetHeight)
                maxy = (gctFLOAT)Vg->targetHeight;
        }
        else
        {
            minx = miny = 0.0f;
            maxx = (gctFLOAT)tsWidth;
            maxy = (gctFLOAT)tsHeight;
        }

        /* Looping tessellation in small tiles. */
        for (y = (gctINT)miny; y < (gctINT)maxy; y += tsHeight)
        {
            for (x = (gctINT)minx; x < (gctINT)maxx; x += tsWidth)
            {
                /* Program tesselation buffers. */
                gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
                    Vg->hw,
                    SoftwareTesselation,
                    x, y,
                    x, y,
                    (gctUINT16)tsWidth,
                    (gctUINT16)tsHeight,
                    Bias, Scale,
                    Vg->userToSurface,
                    tessellationBuffer
                    ));

                /* Draw the path. */
                gcmERR_BREAK(gcoVGHARDWARE_DrawPath(
                    Vg->hw,
                    SoftwareTesselation,
                    PathData,
                    tessellationBuffer,
                    gcvNULL
                    ));
            }
        }
#else
        /* Program tesselation buffers. */
        gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
            Vg->hw,
            SoftwareTesselation,
            (gctUINT16)Vg->targetWidth, (gctUINT16)Vg->targetHeight,
            Bias, Scale,
            Vg->userToSurface,
            tessellationBuffer
            ));

        /* Draw the path. */
        gcmERR_BREAK(gcoVGHARDWARE_DrawPath(
            Vg->hw,
            SoftwareTesselation,
            PathData,
            tessellationBuffer,
            gcvNULL
            ));
#endif

        /* Success. */
        status = gcvSTATUS_OK;
    }
    while (gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_DrawPath);
#endif

    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_DrawImage(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Source,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gcsSIZE_PTR SourceSize,
    IN gctINT SourceX,
    IN gctINT SourceY,
    IN gctINT TargetX,
    IN gctINT TargetY,
    IN gctINT Width,
    IN gctINT Height,
    IN gctBOOL Mask,
    IN gctBOOL isDrawImage
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x ",
                  Vg
                  );

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_DrawImage);
#endif

    do
    {
        gcsVG_RECT srcRect;
        gcsVG_RECT trgRect;

        srcRect.x      = SourceOrigin->x + SourceX;
        srcRect.y      = SourceOrigin->y + SourceY;
        srcRect.width  = Width;
        srcRect.height = Height;

        trgRect.x      = TargetOrigin->x + TargetX;
        trgRect.y      = TargetOrigin->y + TargetY;
        trgRect.width  = Width;
        trgRect.height = Height;

        /* Draw the image. */
        gcmERR_BREAK(gcoVGHARDWARE_DrawImage(
            Vg->hw,
            &Source->info,
            &srcRect,
            &trgRect,
            gcvFILTER_POINT,
            Mask,
            isDrawImage
            ));
    }
    while (gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_DrawImage);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_TesselateImage(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Image,
    IN gcsVG_RECT_PTR Rectangle,
    IN gceIMAGE_FILTER Filter,
    IN gctBOOL Mask,
#if gcdMOVG
    IN gctBOOL SoftwareTesselation,
    IN gceVG_BLEND BlendMode
#else
    IN gctBOOL SoftwareTesselation
#endif
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Image=0x%x Rectangle=0x%x Filter=0x%x Mask=0x%x, SoftwareTesselation=0x%x",
                  Vg, Image, Rectangle,  Filter, Mask, SoftwareTesselation
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_TesselateImage);
#endif
    do
    {
        static gctFLOAT userToSurface[6] =
        {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };

#if gcdMOVG
        gcsTESSELATION_PTR tessellationBuffer = gcvNULL;
        gctBOOL     withTS = gcvFALSE;

        if ((BlendMode != gcvVG_BLEND_SRC_OVER)
            || Vg->scissor != gcvNULL
            || SoftwareTesselation)
        {
            withTS = gcvTRUE;
        }

        if (withTS)
        {
            gctINT tsw, tsh;
#if GC355_PROFILER
            /* Get the pointer to the current tesselation buffer. */
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, Vg->targetWidth, Vg->targetHeight, &tessellationBuffer, &tsw, &tsh
                ));
#else
            gcmERR_BREAK(_GetTessellationBuffer(
                Vg, Vg->targetWidth, Vg->targetHeight, &tessellationBuffer, &tsw, &tsh
                ));
#endif
            /* Program tesselation buffers. */
            gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
                Vg->hw,
                SoftwareTesselation,
                0, 0,
                0, 0,
                (gctUINT16)Vg->targetWidth, (gctUINT16)Vg->targetHeight,
                0.0f, 1.0f,
                userToSurface,
                tessellationBuffer
                ));
        }
#else
        gcsTESSELATION_PTR tessellationBuffer = gcvNULL;
#if GC355_PROFILER
        /* Get the pointer to the current tesselation buffer. */
        gcmERR_BREAK(_GetTessellationBuffer(
            Vg, Vg->funcDList, Vg->TreeDepth, Vg->saveLayerTreeDepth, Vg->varTreeDepth, &tessellationBuffer
            ));
#else
        gcmERR_BREAK(_GetTessellationBuffer(
            Vg, &tessellationBuffer
            ));
#endif
        /* Program tesselation buffers. */
        gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
            Vg->hw,
            SoftwareTesselation,
            (gctUINT16)Vg->targetWidth, (gctUINT16)Vg->targetHeight,
            0.0f, 1.0f,
            userToSurface,
            tessellationBuffer
            ));
#endif

        /* Draw the image. */
        status = gcoVGHARDWARE_TesselateImage(
            Vg->hw,
            SoftwareTesselation,
            &Image->info,
            Rectangle,
            Filter,
            Mask,
            Vg->userToSurface,
            Vg->surfaceToImage,
            tessellationBuffer
            );

#if gcdMOVG
        if (withTS)
        {
            if ((status != gcvSTATUS_OK) && !SoftwareTesselation)
            {
                /* Program tesselation buffers. */
                gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
                    Vg->hw,
                    gcvTRUE,
                    0, 0,
                    0, 0,
                    (gctUINT16)Vg->targetWidth, (gctUINT16)Vg->targetHeight,
                    0.0f, 1.0f,
                    userToSurface,
                    tessellationBuffer
                    ));

                /* Draw the image. */
                gcmERR_BREAK(gcoVGHARDWARE_TesselateImage(
                    Vg->hw,
                    gcvTRUE,
                    &Image->info,
                    Rectangle,
                    Filter,
                    Mask,
                    Vg->userToSurface,
                    Vg->surfaceToImage,
                    tessellationBuffer
                    ));
            }
        }
#else
        if ((status != gcvSTATUS_OK) && !SoftwareTesselation)
        {
            /* Program tesselation buffers. */
            gcmERR_BREAK(gcoVGHARDWARE_SetTessellation(
                Vg->hw,
                gcvTRUE,
                (gctUINT16)Vg->targetWidth, (gctUINT16)Vg->targetHeight,
                0.0f, 1.0f,
                userToSurface,
                tessellationBuffer
                ));

            /* Draw the image. */
            gcmERR_BREAK(gcoVGHARDWARE_TesselateImage(
                Vg->hw,
                gcvTRUE,
                &Image->info,
                Rectangle,
                Filter,
                Mask,
                Vg->userToSurface,
                Vg->surfaceToImage,
                tessellationBuffer
                ));
        }
#endif
    }
    while (gcvFALSE);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_TesselateImage);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_DrawSurfaceToImage(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Image,
    IN const gcsVG_RECT_PTR SrcRectangle,
    IN const gcsVG_RECT_PTR DstRectangle,
    IN const gctFLOAT Matrix[9],
    IN gceIMAGE_FILTER Filter,
    IN gctBOOL Mask,
    IN gctBOOL FirstTime
    )
{
    gceSTATUS status;

    gcmHEADER_ARG(
    	"Vg=%p Image=%p SrcRectangle=%p DstRectangle=%p Matrix=%p Filter=%d Mask=%d FirstTime=%d",
        Vg, Image, SrcRectangle, DstRectangle, Matrix, Filter, Mask, FirstTime
        );

    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_DrawSurfaceToImage);
#endif

	/* Draw the image. */
	status = gcoVGHARDWARE_DrawSurfaceToImage(
		Vg->hw,
		&Image->info,
		SrcRectangle,
		DstRectangle,
		Filter,
		Mask,
		Matrix,
		FirstTime
		);

#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_DrawSurfaceToImage);
#endif
    gcmFOOTER();

    /* Return the status. */
    return status;
}

gceSTATUS
gcoVG_Blit(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Source,
    IN gcoSURF Target,
    IN gcsVG_RECT_PTR SrcRect,
    IN gcsVG_RECT_PTR TrgRect,
    IN gceIMAGE_FILTER Filter,
    IN gceVG_BLEND Mode
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Source=0x%x Target=0x%x SrcRect=0x%x TrgRect=0x%x, Filter=0x%x Mode =0x%x",
                  Vg, Source, Target,  SrcRect, TrgRect, Filter, Mode
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_Blit);
#endif
    /* Send to the hardware. */
    status = gcoVGHARDWARE_VgBlit(
        Vg->hw,
        &Source->info,
        &Target->info,
        SrcRect,
        TrgRect,
        Filter,
        gcvVG_BLEND_SRC
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_Blit);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_ColorMatrix(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Source,
    IN gcoSURF Target,
    IN const gctFLOAT * Matrix,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Source=0x%x Target=0x%x Matrix=0x%x ColorChannels=0x%x, FilterLinear=0x%x FilterPremultiplied =0x%x"
                    "SourceOrigin=0x%x  TargetOrigin=0x%x Width=0x%x Height=0x%x",
                  Vg, Source, Target,  Matrix, ColorChannels, FilterLinear, FilterPremultiplied, SourceOrigin,
                    TargetOrigin, Width, Height
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_ColorMatrix);
#endif
    /* Execute color matrix. */
    status = gcoVGHARDWARE_ColorMatrix(
        Vg->hw,
        &Source->info,
        &Target->info,
        Matrix,
        ColorChannels,
        FilterLinear,
        FilterPremultiplied,
        SourceOrigin,
        TargetOrigin,
        Width, Height
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_ColorMatrix);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_SeparableConvolve(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Source,
    IN gcoSURF Target,
    IN gctINT KernelWidth,
    IN gctINT KernelHeight,
    IN gctINT ShiftX,
    IN gctINT ShiftY,
    IN const gctINT16 * KernelX,
    IN const gctINT16 * KernelY,
    IN gctFLOAT Scale,
    IN gctFLOAT Bias,
    IN gceTILE_MODE TilingMode,
    IN gctFLOAT_PTR FillColor,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gcsSIZE_PTR SourceSize,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Source=0x%x Target=0x%x",
                  Vg, Source, Target
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SeparableConvolve);
#endif
    /* Execute color matrix. */
    status = gcoVGHARDWARE_SeparableConvolve(
        Vg->hw,
        &Source->info, &Target->info,
        KernelWidth, KernelHeight,
        ShiftX, ShiftY,
        KernelX, KernelY,
        Scale, Bias,
        TilingMode, FillColor,
        ColorChannels,
        FilterLinear,
        FilterPremultiplied,
        SourceOrigin,
        TargetOrigin,
        SourceSize,
        Width, Height
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SeparableConvolve);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_GaussianBlur(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gcoSURF Source,
    IN gcoSURF Target,
    IN gctFLOAT StdDeviationX,
    IN gctFLOAT StdDeviationY,
    IN gceTILE_MODE TilingMode,
    IN gctFLOAT_PTR FillColor,
    IN gceCHANNEL ColorChannels,
    IN gctBOOL FilterLinear,
    IN gctBOOL FilterPremultiplied,
    IN gcsPOINT_PTR SourceOrigin,
    IN gcsPOINT_PTR TargetOrigin,
    IN gcsSIZE_PTR SourceSize,
    IN gctINT Width,
    IN gctINT Height
    )
{
    gceSTATUS status;


    gcmHEADER_ARG("Vg=0x%x Source=0x%x Target=0x%x",
                  Vg, Source, Target
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_GaussianBlur);
#endif
    /* Execute color matrix. */
    status = gcoVGHARDWARE_GaussianBlur(
        Vg->hw,
        &Source->info, &Target->info,
        StdDeviationX, StdDeviationY,
        TilingMode, FillColor,
        ColorChannels,
        FilterLinear,
        FilterPremultiplied,
        SourceOrigin,
        TargetOrigin,
        SourceSize,
        Width, Height
        );
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_GaussianBlur);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Set the image mode.
**
**  Set the image mode to be used by the next rendering.
**
**  @param[in]  Vg          Pointer to the gcoVG object.
**  @param[in]  Mode        Image mode.
*/
gceSTATUS
gcoVG_SetImageMode(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceVG_IMAGE Mode
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Mode=0x%x ",
                  Vg, Mode
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetImageMode);
#endif
    /* Set image mode in hardware. */
    status = gcoVGHARDWARE_SetVgImageMode(Vg->hw, Mode);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetImageMode);
#endif

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoVG_SetRenderingQuality(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceRENDER_QUALITY Quality
    )
{
    gceSTATUS status;

    gcmHEADER_ARG("Vg=0x%x Quality=0x%x ",
                  Vg, Quality
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg,DList,TreeDepth,saveLayerTreeDepth,varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetRenderingQuality);
#endif
    /* Set the quality. */
    Vg->renderQuality = Quality;

    /* Update the setting in hardware. */
    status = gcoVGHARDWARE_SetRenderingQuality(Vg->hw, Quality);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetRenderingQuality);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}

gceSTATUS
gcoVG_SetFillRule(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gceFILL_RULE FillRule
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x FillRule=0x%x ",
                  Vg, FillRule
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);
#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_SetFillRule);
#endif
    /* Update the setting in hardware. */
    status = gcoVGHARDWARE_SetFillRule(Vg->hw, FillRule);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_SetFillRule);
#endif
    gcmFOOTER();
    /* Return status. */
    return status;
}


/******************************************************************************/
/** @ingroup gcoVG
**
**  @brief   Enable or disable dither.
**
**  @param[in]  Vg      Pointer to the gcoVG object.
**  @param[in]  Enable  gcvTRUE to enable dither or gcvFALSE to disable
**                      dither.  dither is by default disabled.
*/
gceSTATUS
gcoVG_EnableDither(
    IN gcoVG Vg,
#if GC355_PROFILER
    IN gcsPROFILERFUNCNODE *DList,
    IN gctUINT TreeDepth,
    IN gctUINT saveLayerTreeDepth,
    IN gctUINT varTreeDepth,
#endif
    IN gctBOOL Enable
    )
{
    gceSTATUS status;
    gcmHEADER_ARG("Vg=0x%x Enable=0x%x ",
                  Vg, Enable
                  );
    /* Verify the arguments. */
    gcmVERIFY_OBJECT(Vg, gcvOBJ_VG);

#if GC355_PROFILER
    gcoVG_ProfilerSetStates(Vg, DList, TreeDepth, saveLayerTreeDepth, varTreeDepth);
    vghalENTERSUBAPI(gcoVG_EnableDither);
#endif
    /* Call gcoVGHARDWARE to enable scissoring. */
    status = gcoVGHARDWARE_EnableDither(Vg->hw, Enable);
#if GC355_PROFILER
    vghalLEAVESUBAPI(gcoVG_EnableDither);
#endif

    gcmFOOTER();
    return status;
}

#endif /* gcdENABLE_VG */
