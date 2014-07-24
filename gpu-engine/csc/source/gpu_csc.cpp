/***********************************************************************************
 *
 *    Copyright (c) 2012 - 2013 by Marvell International Ltd. and its affiliates.
 *    All rights reserved.
 *
 *    This software file (the "File") is owned and distributed by Marvell
 *    International Ltd. and/or its affiliates ("Marvell") under Marvell Commercial
 *    License.
 *
 *    If you received this File from Marvell and you have entered into a commercial
 *    license agreement (a "Commercial License") with Marvell, the File is licensed
 *    to you under the terms of the applicable Commercial License.
 *
 ***********************************************************************************/

#include <string.h>
#include "gpu_csc.h"

GCUbool _gcuConvertARGB2YUV(GCUContext pContext, GCUSurface pARGBSurface, GCUSurface pYUVSurface)
{
    GCU_SURFACE_DATA        srcSurfaceData;
    GCU_SURFACE_DATA        dstSurfaceData;
    GCU_SURFACE_LOCK_DATA   lockSrcData;
    GCU_ALLOC_INFO          allocSrcInfos;
    GCUbool                 bLockSrcOk = GCU_FALSE;
    GCU_SURFACE_LOCK_DATA   lockDstData;
    GCU_ALLOC_INFO          allocDstInfos[3];
    GCUbool                 bLockDstOk = GCU_FALSE;
    GCU_FORMAT              yuvFormat;

    if(pContext)
    {
        memset(&srcSurfaceData, 0, sizeof(srcSurfaceData));
        bLockSrcOk = gcuQuerySurfaceInfo(pContext, pARGBSurface, &srcSurfaceData);

        memset(&dstSurfaceData, 0, sizeof(dstSurfaceData));
        bLockDstOk = gcuQuerySurfaceInfo(pContext, pYUVSurface, &dstSurfaceData);

        if(bLockSrcOk && bLockDstOk)
        {
            yuvFormat = dstSurfaceData.format;
            if(((srcSurfaceData.format != GCU_FORMAT_ARGB8888 &&
                srcSurfaceData.format != GCU_FORMAT_XRGB8888 ) ||
                (yuvFormat != GCU_FORMAT_UYVY && yuvFormat != GCU_FORMAT_I420 &&
                yuvFormat != GCU_FORMAT_NV21 && yuvFormat != GCU_FORMAT_YV12 &&
                yuvFormat != GCU_FORMAT_NV12)) && ((yuvFormat != GCU_FORMAT_NV12) ||
                (srcSurfaceData.format != GCU_FORMAT_RGB565)))
            {
                return GCU_FALSE;
            }
        }
        else
        {
            return GCU_FALSE;
        }

        memset(&lockSrcData, 0, sizeof(lockSrcData));
        lockSrcData.pSurface = pARGBSurface;
        lockSrcData.flag.bits.preserve = 1;
        lockSrcData.arraySize = srcSurfaceData.arraySize;
        lockSrcData.pAllocInfos = &allocSrcInfos;
        bLockSrcOk = gcuLockSurface(pContext, &lockSrcData);

        memset(&lockDstData, 0, sizeof(lockDstData));
        lockDstData.pSurface = pYUVSurface;
        lockDstData.flag.bits.preserve = 1;
        lockDstData.arraySize = dstSurfaceData.arraySize;
        lockDstData.pAllocInfos = allocDstInfos;
        bLockDstOk = gcuLockSurface(pContext, &lockDstData);

        if(bLockSrcOk && bLockDstOk)
        {
            switch(yuvFormat)
            {
                unsigned char* pDst[3];
                GCUint dstStep[3];
                case GCU_FORMAT_UYVY:
                {
                    pDst[0]     = (unsigned char*)allocDstInfos[0].virtualAddr;
                    dstStep[0]  = allocDstInfos[0].stride;
                    gpu_csc_ARGBToUYVY((unsigned char*)allocSrcInfos.virtualAddr, allocSrcInfos.stride, pDst[0], dstStep[0], allocSrcInfos.width, allocSrcInfos.height);
                    break;
                }

                case GCU_FORMAT_NV21:
                {
                    pDst[0]     = (unsigned char*)allocDstInfos[0].virtualAddr;
                    pDst[1]     = (unsigned char*)allocDstInfos[1].virtualAddr;
                    dstStep[0]  = allocDstInfos[0].stride;
                    dstStep[1]  = allocDstInfos[1].stride;
                    gpu_csc_ARGBToNV21((unsigned char*)allocSrcInfos.virtualAddr, allocSrcInfos.stride, pDst, dstStep, allocSrcInfos.width, allocSrcInfos.height);
                    break;
                }

                case GCU_FORMAT_I420:
                case GCU_FORMAT_YV12:
                {
                    pDst[0]     = (unsigned char*)allocDstInfos[0].virtualAddr;
                    pDst[1]     = (unsigned char*)allocDstInfos[1].virtualAddr;
                    pDst[2]     = (unsigned char*)allocDstInfos[2].virtualAddr;
                    dstStep[0]  = allocDstInfos[0].stride;
                    dstStep[1]  = allocDstInfos[1].stride;
                    dstStep[2]  = allocDstInfos[2].stride;
                    gpu_csc_ARGBToI420((unsigned char*)allocSrcInfos.virtualAddr, allocSrcInfos.stride, pDst, dstStep, allocSrcInfos.width, allocSrcInfos.height);
                    break;
                }

                case GCU_FORMAT_NV12:
                {
                    if(srcSurfaceData.format == GCU_FORMAT_RGB565)
                    {
                        pDst[0]     = (unsigned char*) allocDstInfos[0].virtualAddr;
                        pDst[1]     = (unsigned char*) allocDstInfos[1].virtualAddr;
                        dstStep[0]  = allocDstInfos[0].stride;
                        dstStep[1]  = allocDstInfos[1].stride;
                        gpu_csc_RGBToNV12((unsigned char*)allocSrcInfos.virtualAddr, allocSrcInfos.stride, pDst, dstStep, allocSrcInfos.width, allocSrcInfos.height);
                    }
                    else
                    {
                        pDst[0]     = (unsigned char*)allocDstInfos[0].virtualAddr;
                        pDst[1]     = (unsigned char*)allocDstInfos[1].virtualAddr;
                        dstStep[0]  = allocDstInfos[0].stride;
                        dstStep[1]  = allocDstInfos[1].stride;
                        gpu_csc_ARGBToNV12((unsigned char*)allocSrcInfos.virtualAddr, allocSrcInfos.stride, pDst, dstStep, allocSrcInfos.width, allocSrcInfos.height);
                    }
                    break;
                }

                default:
                    return GCU_FALSE;
            }

            gcuUnlockSurface(pContext, pARGBSurface);
            gcuUnlockSurface(pContext, pYUVSurface);

            if(GCU_NO_ERROR != gcuGetError())
            {
                return GCU_FALSE;
            }
        }
        else
        {
            if(bLockSrcOk)
            {
                gcuUnlockSurface(pContext, pARGBSurface);
            }

            if(bLockDstOk)
            {
                gcuUnlockSurface(pContext, pYUVSurface);
            }

            return GCU_FALSE;
        }

        return GCU_TRUE;
    }

    return GCU_FALSE;
}

