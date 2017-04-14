/*
* (C) Copyright 2010 Marvell International Ltd.
* All Rights Reserved
*
* MARVELL CONFIDENTIAL
* Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Marvell International Ltd or its
* suppliers or licensors. Title to the Material remains with Marvell International Ltd
* or its suppliers and licensors. The Material contains trade secrets and
* proprietary and confidential information of Marvell or its suppliers and
* licensors. The Material is protected by worldwide copyright and trade secret
* laws and treaty provisions. No part of the Material may be used, copied,
* reproduced, modified, published, uploaded, posted, transmitted, distributed,
* or disclosed in any way without Marvell's prior express written permission.
*
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or delivery
* of the Materials, either expressly, by implication, inducement, estoppel or
* otherwise. Any license under such intellectual property rights must be
* express and approved by Marvell in writing.
*
*/


#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include <utils/Mutex.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <surfaceflinger/Transform.h>
#include "GcuEngine.h"

using namespace android;

GcuEngine::GcuEngine() : mGCUContextPtr(NULL)
                       , mFlushAtEnd(false)
                       , mAndPatternPtr(NULL)
                       , mOrPatternPtr(NULL)
{
    if(!Init()) {
        LOGE("GcuEngine initialization failed!");
    }
}

GcuEngine::~GcuEngine()
{
    if (NULL != mAndPatternPtr){
        gcuDestroySurface(mGCUContextPtr, mAndPatternPtr);
    }

    if (NULL != mOrPatternPtr){
        gcuDestroySurface(mGCUContextPtr, mOrPatternPtr);
    }

    for(uint32_t i = 0; i < MAX_HINT_PICS; ++i){
        if(NULL != mHintSurface[i]){
            gcuDestroySurface(mGCUContextPtr, mHintSurface[i]);
        }
    }

    if (mGCUContextPtr != NULL) {
        gcuDestroyContext(mGCUContextPtr);
    }

    gcuTerminate();
}

bool GcuEngine::Init()
{
    GCU_INIT_DATA initData;
    GCU_CONTEXT_DATA contextData;
    // Init GCU library
    memset(&initData, 0, sizeof(initData));

    initData.debug = 0; // set it 1 to open GCU log out
    gcuInitialize(&initData);

    // Create Context
    memset(&contextData, 0, sizeof(contextData));
    mGCUContextPtr = gcuCreateContext(&contextData);

    if(mGCUContextPtr == NULL)
    {
         GCUenum result = gcuGetError();
         LOGE("GCU create context failed, error code = %d", result);
         return false;
    }

    gcuSet(mGCUContextPtr, GCU_QUALITY, GCU_QUALITY_HIGH);

    preparePatternSurfaces();

    for(uint32_t i = 0; i < MAX_HINT_PICS; ++i){
        mHintSurface[i] = NULL;
    }

    mFlushAtEnd = false;
    return true;
}

void GcuEngine::preparePatternSurfaces()
{
    GCUVirtualAddr virtAddr;
    GCUPhysicalAddr physAddr;

    uint32_t width = 16;
    uint32_t height = 4;
    mAndPatternPtr = _gcuCreateBuffer(mGCUContextPtr,
                                      width,
                                      height,
                                      GCU_FORMAT_ABGR8888,
                                      &virtAddr,
                                      &physAddr);

    mOrPatternPtr = _gcuCreateBuffer(mGCUContextPtr,
                                     width,
                                     height,
                                     GCU_FORMAT_ABGR8888,
                                     &virtAddr,
                                     &physAddr);

    GCU_FILL_DATA fillData;
    GCU_RECT dstRect;
    dstRect.left   = 0;
    dstRect.right  = width;
    dstRect.top    = 0;
    dstRect.bottom = height;
    memset(&fillData, 0, sizeof(fillData));

    fillData.bSolidColor = GCU_TRUE;
    fillData.color = 0x00FFFFFF;
    fillData.pRect = &dstRect;
    fillData.pSurface = mAndPatternPtr;

    gcuFill(mGCUContextPtr, &fillData);

    fillData.bSolidColor = GCU_TRUE;
    fillData.color = 0xFF000000;
    fillData.pRect = &dstRect;
    fillData.pSurface = mOrPatternPtr;
    gcuFill(mGCUContextPtr, &fillData);

    gcuFinish(mGCUContextPtr);

#if 0
    //Prepare pattern surface.
    uint32_t* pBuffer = (uint32_t*)virtAddr;
    for(uint32_t i = 0; i < width*height; ++i){
        pBuffer[i] = 0x00FFFFFF;
    }
#endif
}

bool GcuEngine::Blit(PBlitDataDesc blitDesc)
{
    blitDesc->dump();

    bool result = false;
#if HARDWARE_ENGINE_SWITCH
    gceHARDWARE_TYPE hardware_type;
    gcoHAL_GetHardwareType(gcvNULL, &hardware_type);
    gcoHAL_SetHardwareType(gcvNULL, gcvHARDWARE_2D);
#endif
    switch (blitDesc->mBlitType)
    {
        case GPU_BLIT_SRC:
        {
            result = SrcBlit(blitDesc);
            break;
        }
        case GPU_BLIT_STRETCH:
        {
            result = FilterBlit(blitDesc);
            break;
        }
        case GPU_BLIT_FILL:
        {
            result = FillBlit(blitDesc);
            break;
        }
        case GPU_BLIT_FILTER:
        {
            result = FilterBlit(blitDesc);
            break;
        }
        case GPU_BLIT_ROP:
        {
            result = RopBlit(blitDesc);
            break;
        }
        default:
        {
            LOGE("Invalid blit type for GPU acceleration");
            return false;
        }
    }

    if (mFlushAtEnd) {
        gcuFlush(mGCUContextPtr);
    }

#if HARDWARE_ENGINE_SWITCH
    gcoHAL_SetHardwareType(gcvNULL, hardware_type);
#endif

    return result;
}

bool GcuEngine::RopBlit(PBlitDataDesc blitDesc)
{
    GCU_ROP_DATA bltData;
    GCU_RECT srcRect, dstRect, patRect;
    //getRects(blitDesc, srcRect, dstRect);
    GCUSurface pSrcSurface = NULL, pDstSurface = NULL;
    getSurfaces(blitDesc, pSrcSurface, pDstSurface);

    srcRect.left   = 0;
    srcRect.right  = 1;
    srcRect.top    = 0;
    srcRect.bottom = 1;

    dstRect.left   = 0;
    dstRect.right  = 1024;
    dstRect.top    = 0;
    dstRect.bottom = 600;

    patRect.left   = 0;
    patRect.right  = 1;
    patRect.top    = 0;
    patRect.bottom = 1;

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = (blitDesc->mRop == 0x88) ? mAndPatternPtr : mOrPatternPtr;
    bltData.pDstSurface = pDstSurface;
    bltData.pSrcRect = &srcRect;
    bltData.pDstRect = &dstRect;
    bltData.rotation = getGCURotation(blitDesc->mRotationDegree);
    bltData.pClipRect = &dstRect;
    bltData.rop = blitDesc->mRop;
    bltData.pPattern = mAndPatternPtr;
    bltData.pPatRect = &patRect;

    gcuSet(mGCUContextPtr, GCU_QUALITY, GCU_QUALITY_NORMAL);
    gcuRop(mGCUContextPtr, &bltData);
    gcuFinish(mGCUContextPtr);

    gcuSet(mGCUContextPtr, GCU_QUALITY, GCU_QUALITY_HIGH);
    gcuDestroySurface(mGCUContextPtr, pSrcSurface);
    gcuDestroySurface(mGCUContextPtr, pDstSurface);

    return true;
}

bool GcuEngine::FilterBlit(PBlitDataDesc blitDesc)
{
    GCU_BLT_DATA bltData;
    GCU_RECT srcRect, dstRect;
    getRects(blitDesc, srcRect, dstRect);
    GCUSurface pSrcSurface = NULL, pDstSurface = NULL;
    getSurfaces(blitDesc, pSrcSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pDstSurface;
    bltData.pSrcRect = &srcRect;
    bltData.pDstRect = &dstRect;
    bltData.rotation = getGCURotation(blitDesc->mRotationDegree);

    gcuBlit(mGCUContextPtr, &bltData);
    gcuFinish(mGCUContextPtr);

    gcuDestroySurface(mGCUContextPtr, pSrcSurface);
    gcuDestroySurface(mGCUContextPtr, pDstSurface);

    return true;
}

bool GcuEngine::SrcBlit(PBlitDataDesc blitDesc)
{
    GCU_BLT_DATA bltData;
    GCU_RECT srcRect, dstRect;
    getRects(blitDesc, srcRect, dstRect);

    GCUSurface pSrcSurface = NULL, pDstSurface = NULL;
    getSurfaces(blitDesc, pSrcSurface, pDstSurface);

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pDstSurface;

    bltData.pSrcRect = &srcRect;
    bltData.pDstRect = &dstRect;
    bltData.rotation = getGCURotation(blitDesc->mRotationDegree);

    gcuBlit(mGCUContextPtr, &bltData);
    gcuFinish(mGCUContextPtr);

    gcuDestroySurface(mGCUContextPtr, pSrcSurface);
    gcuDestroySurface(mGCUContextPtr, pDstSurface);

    return true;
}

bool GcuEngine::FillBlit(PBlitDataDesc blitDesc)
{
    GCU_FILL_DATA fillData;

    GCU_RECT dstRect;
    dstRect.left   = blitDesc->mDstRect->l;
    dstRect.right  = blitDesc->mDstRect->r;
    dstRect.top    = blitDesc->mDstRect->t;
    dstRect.bottom = blitDesc->mDstRect->b;


    GCUSurface pDstSurface = _gcuCreatePreAllocBuffer(mGCUContextPtr,
                                                      blitDesc->mDstWidth,
                                                      blitDesc->mDstHeight,
                                                      getGCUFormat(blitDesc->mDstFormat),
                                                      GCU_TRUE,
                                                      (void*)0x1000, //fakeVirtualAddr,
                                                      GCU_TRUE,
                                                      blitDesc->mDstAddr);
    memset(&fillData, 0, sizeof(fillData));
    if (blitDesc->mIsSolidFill) {
        fillData.bSolidColor = GCU_TRUE;
        fillData.color = blitDesc->mFillColor;
        fillData.pRect = &dstRect;
        fillData.pSurface = pDstSurface;
    } else {
        fillData.bSolidColor = GCU_FALSE;
        //fillData.pPattern = blitDesc->mPatternPtr;
        fillData.color = blitDesc->mFillColor;
        fillData.pRect = &dstRect;
        fillData.pSurface = pDstSurface;
    }

    gcuFill(mGCUContextPtr, &fillData);
    gcuFinish(mGCUContextPtr);
    gcuDestroySurface(mGCUContextPtr, pDstSurface);

    return true;
}

void GcuEngine::getRects(PBlitDataDesc blitDesc, GCU_RECT &srcRect, GCU_RECT &dstRect)
{
    srcRect.left   = blitDesc->mSrcRect->l;
    srcRect.right  = blitDesc->mSrcRect->r;
    srcRect.top    = blitDesc->mSrcRect->t;
    srcRect.bottom = blitDesc->mSrcRect->b;

    dstRect.left   = blitDesc->mDstRect->l;
    dstRect.right  = blitDesc->mDstRect->r;
    dstRect.top    = blitDesc->mDstRect->t;
    dstRect.bottom = blitDesc->mDstRect->b;

}

bool GcuEngine::getSurfaces(PBlitDataDesc blitDesc, GCUSurface &pSrcSurface, GCUSurface &pDstSurface)
{
    GCUuint srcWidth  = blitDesc->mSrcWidth;
    GCUuint srcHeight = blitDesc->mSrcHeight;
    GCUuint dstWidth  = blitDesc->mDstWidth;
    GCUuint dstHeight = blitDesc->mDstHeight;

    void *fakeVirtualAddr = (void *)0x1000;

    pSrcSurface = _gcuCreatePreAllocBuffer(mGCUContextPtr,
                                           srcWidth,
                                           srcHeight,
                                           getGCUFormat(blitDesc->mSrcFormat),
                                           GCU_TRUE,
                                           fakeVirtualAddr,
                                           GCU_TRUE,
                                           blitDesc->mSrcAddr);

    pDstSurface = _gcuCreatePreAllocBuffer(mGCUContextPtr,
                                           dstWidth,
                                           dstHeight,
                                           getGCUFormat(blitDesc->mDstFormat),
                                           GCU_TRUE,
                                           fakeVirtualAddr,
                                           GCU_TRUE,
                                           blitDesc->mDstAddr);
    return true;
}

GCU_ROTATION GcuEngine::getGCURotation(uint32_t rotationDegree)
{
    GCU_ROTATION res = GCU_ROTATION_0;
    if(rotationDegree & Transform::ROT_INVALID)
        return res;

    switch(rotationDegree)
    {
        case DISPLAY_SURFACE_ROTATION_0:
             res = GCU_ROTATION_0;
             break;
        case DISPLAY_SURFACE_ROTATION_90:
             res = GCU_ROTATION_90;
             break;
        case DISPLAY_SURFACE_ROTATION_180:
             res = GCU_ROTATION_180;
             break;
        case DISPLAY_SURFACE_ROTATION_270:
             res = GCU_ROTATION_270;
             break;
        default:
             LOGE("Invalid rotation degree. %d lines, %s", __LINE__, __FUNCTION__);
    }

    // Temporarily we do not concern the FLIP case.
    return res;
}


GCU_FORMAT GcuEngine::getGCUFormat(uint32_t format)
{
    switch (format) {
        case HAL_PIXEL_FORMAT_RGB_565: //DISP_FOURCC_RGB565:
            return GCU_FORMAT_RGB565;
/*
        case DISP_FOURCC_RGBX8888:
            return GCU_FORMAT_XRGB8888;
        case DISP_FOURCC_BGRA8888:
            return GCU_FORMAT_BGRA8888;
        case DISP_FOURCC_RGBA8888:
            return GCU_FORMAT_RGBA8888;
*/
        case HAL_PIXEL_FORMAT_RGBA_8888: //DISP_FOURCC_ABGR8888:
            return GCU_FORMAT_ABGR8888;
        case HAL_PIXEL_FORMAT_BGRA_8888: // DISP_FOURCC_ARGB8888:
            return GCU_FORMAT_ARGB8888;
        case HAL_PIXEL_FORMAT_YCbCr_420_P: //DISP_FOURCC_I420:
            return GCU_FORMAT_I420;
        case HAL_PIXEL_FORMAT_CbYCrY_422_I: //DISP_FOURCC_UYVY:
            return GCU_FORMAT_UYVY;
        case HAL_PIXEL_FORMAT_YCbCr_422_I: //DISP_FOURCC_YUY2:
            return GCU_FORMAT_YUY2;
        case HAL_PIXEL_FORMAT_YV12: //DISP_FOURCC_YV12:
            return GCU_FORMAT_YV12;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            return GCU_FORMAT_NV21;
        default:
            LOGE("New type switch needed! %d, %s", __LINE__, __FUNCTION__);
    }
    return GCU_FORMAT_UNKNOW;
}

bool GcuEngine::LoadHintPic(uint32_t id, const char* fileName)
{
    if(NULL == fileName || id >= MAX_HINT_PICS){
        LOGE("ERROR: Load %s to mHintSurface[%d] failed!", fileName, id);
        return false;
    }

    GCUSurface pSuface = _gcuLoadRGBSurfaceFromFile(mGCUContextPtr, fileName);
    if(NULL != pSuface){
        mHintSurface[id] = pSuface;
        return true;
    }

    LOGE("ERROR: _gcuLoadRGBSurfaceFromFile(%s) Failed!", fileName);
    return false;
}

bool GcuEngine::BlitHintPic(uint32_t id, PBlitDataDesc blitDesc)
{
    if(id >= MAX_HINT_PICS || NULL == mHintSurface[id]){
        LOGE("ERROR: HINT id(%d), mHintSurface[%d] = %p.", id, id, mHintSurface[id]);
        return false;
    }

    GCU_BLT_DATA bltData;
    GCU_RECT srcRect, dstRect;
    getRects(blitDesc, srcRect, dstRect);

    GCUSurface pSrcSurface = NULL, pDstSurface = NULL;
    getSurfaces(blitDesc, pSrcSurface, pDstSurface);

    pSrcSurface = mHintSurface[id];
    GCU_SURFACE_DATA surfaceData;
    gcuQuerySurfaceInfo(mGCUContextPtr, pSrcSurface, &surfaceData);

    srcRect.left   = 0;
    srcRect.right  = surfaceData.width;
    srcRect.top    = 0;
    srcRect.bottom = surfaceData.height;

    dstRect.left   = (dstRect.right - surfaceData.width) / 2;
    dstRect.right  = (dstRect.right + surfaceData.width) / 2;
    dstRect.top    = (dstRect.bottom - surfaceData.height) / 2;
    dstRect.bottom = (dstRect.bottom + surfaceData.height) / 2;

    memset(&bltData, 0, sizeof(bltData));
    bltData.pSrcSurface = pSrcSurface;
    bltData.pDstSurface = pDstSurface;

    bltData.pSrcRect = &srcRect;
    bltData.pDstRect = &dstRect;
    bltData.rotation = getGCURotation(blitDesc->mRotationDegree);

    gcuBlit(mGCUContextPtr, &bltData);
    gcuFinish(mGCUContextPtr);

    gcuDestroySurface(mGCUContextPtr, pDstSurface);

    return true;
}
