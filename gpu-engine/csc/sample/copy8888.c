#include <string.h>
#include "gpu_csc.h"

void memcpy_MRVL(char * pDst, char * pSrc, int size);

GCUbool Copy8888To8888(GCUContext  pContext, GCUSurface pSurfaceARGB, GCUSurface pDstSurface)
{
    GCU_SURFACE_LOCK_DATA   lockSrcData;
    GCU_ALLOC_INFO          allocSrcInfos;
    GCUbool                 bLockSrcOk = GCU_FALSE;
    GCU_SURFACE_LOCK_DATA   lockDstData;
    GCU_ALLOC_INFO          allocDstInfos;
    GCUbool                 bLockDstOk = GCU_FALSE;

    if(pContext)
    {
        memset(&lockSrcData, 0, sizeof(lockSrcData));
        lockSrcData.pSurface = pSurfaceARGB;
        lockSrcData.flag.bits.preserve = 1;
        lockSrcData.arraySize = 1;
        lockSrcData.pAllocInfos = &allocSrcInfos;
        bLockSrcOk = gcuLockSurface(pContext, &lockSrcData);

        memset(&lockDstData, 0, sizeof(lockDstData));
        lockDstData.pSurface = pDstSurface;
        lockDstData.flag.bits.preserve = 1;
        lockDstData.arraySize = 1;
        lockDstData.pAllocInfos = &allocDstInfos;
        bLockDstOk = gcuLockSurface(pContext, &lockDstData);

        if(bLockSrcOk && bLockDstOk)
        {
            char * pDst;
            char * pSrc;
            GCUint dstStep;
            pDst     = (char *) allocDstInfos.virtualAddr;
            pSrc     = (char *) allocSrcInfos.virtualAddr;
            dstStep  = allocDstInfos.stride;
            GCUuint i;
            for(i=0; i<allocSrcInfos.height; i++)
            {
                memcpy_MRVL(pDst, pSrc, allocSrcInfos.width*4);
                pDst += allocDstInfos.stride;
                pSrc += allocSrcInfos.stride;
            }
        }
        gcuUnlockSurface(pContext, pSurfaceARGB);
        gcuUnlockSurface(pContext, pDstSurface);
    }
    return GCU_TRUE;
}


