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

#ifndef __HW_OVERLAY_DEVICE_H__
#define __HW_OVERLAY_DEVICE_H__

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <ui/Rect.h>
#include <utils/RefBase.h>
#include <utils/Log.h>
#include <utils/SortedVector.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include "HWCFenceManager.h"
#include "gralloc_priv.h"
#include "OverlayDisplayEngine/IDisplayEngine.h"
#include "OverlayDisplayEngine/FramebufferOverlay.h"

namespace android{

#define ALIGN(m, align)  (((m + align - 1) & ~(align - 1)))
#define ALIGN_4K(m)                         ALIGN((m), 0x1000)

#define DISP_OVLY_PER_PIXEL_ALPHA           0x1
#define DISP_OVLY_GLOBAL_ALPHA              0x2
#define DISP_OVLY_COLORKEY_RGB              0x3
#define DISP_OVLY_COLORKEY_DISABLE          0x0

#define DMA_DELAY_FRAME_NUM                 2

class OverlayDevice : public RefBase
{
public:
    OverlayDevice(uint32_t nType) : m_bOpen(false)
                                  , m_nFrameCount(0)
                                  , m_pShadowAddr(NULL)
    {
        const char* DEVICE_NAME[] = {"/dev/graphics/fb1",
                                     "/dev/graphics/fb2",
                                     "\0"};

        if(nType > 1){
            LOGE("ERROR! No such devices in channel %d.", nType);
        }

        m_pOverlayEngine = new FBOverlayRef(DEVICE_NAME[nType]);
        if(NO_ERROR != m_pOverlayEngine->open()){
            LOGE("ERROR! Open overlay device failed!");
        }

        //memset(&m_shadowHwcLayer, 0, sizeof(m_shadowHwcLayer));
    }

    ~OverlayDevice()
    {
        m_bOpen = false;
        m_pOverlayEngine.clear();
    }

public:

    bool isOpen()
    {
        return m_bOpen;
    }

    bool open()
    {
        if(!m_bOpen){
            Mutex::Autolock lock(m_mutexLock);
            m_bOpen = true;

            //if status changes to on, we should reset the fence manager.
            m_pShadowAddr = NULL;
            m_nFrameCount = 0;
        }
        return true;
    }

    bool close()
    {
        if(m_bOpen){
            Mutex::Autolock lock(m_mutexLock);

            m_bOpen = false;
            m_pOverlayEngine->setStreamOn(false);

            m_pShadowAddr = NULL;
            //if status changes to off, we should do fast forward to release all fence waiting outside.
            m_nFrameCount = 0;
        }
        return true;
    }

    status_t setOverlayAlphaMode(uint32_t alphaMode, uint32_t alphaValue, uint32_t colorKeyMode, uint32_t colorKeyValue)
    {
        static uint32_t savedAlphaMode = 0xFFFFFFFF;
        static uint32_t savedAlphaValue = 0xFFFFFFFF;
        static uint32_t savedColorKeyMode = 0xFFFFFFFF;
        static uint32_t savedColorKeyValue = 0xFFFFFFFF;

        if(savedAlphaMode != alphaMode || savedAlphaValue != alphaValue ||
           savedColorKeyMode != colorKeyMode || savedColorKeyValue != colorKeyValue){
            savedAlphaMode = alphaMode;
            savedAlphaValue = alphaValue;
            savedColorKeyMode = colorKeyMode;
            savedColorKeyValue = colorKeyValue;

            uint32_t r = (colorKeyValue >> 16) & 0xFF;
            uint32_t g = (colorKeyValue >> 8 ) & 0xFF;
            uint32_t b = (colorKeyValue) & 0xFF;

            LOGD("-------------------- change overlay alpha blending mode --------------------");
            //status = m_pOverlayEngine->setColorKey(DISP_OVLY_GLOBAL_ALPHA, 0xFF, DISP_OVLY_COLORKEY_RGB, 0, 0, 0);
            return m_pOverlayEngine->setColorKey(alphaMode, alphaValue, colorKeyMode, r, g, b);
        }

        return NO_ERROR;
    }

    void commit(hwc_layer_1_t* layer)
    {
        if(!m_bOpen){
            return;
        }

#if 0
        updateFenceStatus();
#endif

        private_handle_t *ph = private_handle_t::dynamicCast( layer->handle );

        if(m_pShadowAddr == (void*)ph->physAddr){
            return;
        }

        uint32_t srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
        uint32_t srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
        uint32_t srcStrideX = ph->width;
        uint32_t srcStrideY = ph->height;
        uint32_t dstWidth = layer->displayFrame.right - layer->displayFrame.left;
        uint32_t dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;
        uint32_t format = ph->format;

        uint32_t nAddrY = 0;
        uint32_t nAddrU = 0;
        uint32_t nAddrV = 0;
        resolveImageAddr(format, ph->physAddr, srcStrideX, srcStrideY,
                         &nAddrY, &nAddrU, &nAddrV);

        uint32_t srcUStrideX = 0;
        uint32_t srcVStrideX = 0;
        resolveImageStride(format, srcStrideX, &srcUStrideX, &srcVStrideX);

        uint32_t nImgSize = resolveImageSize(format, srcWidth, srcHeight);
        uint32_t length = ALIGN_4K(nImgSize);

        status_t status = NO_ERROR;
        status = m_pOverlayEngine->setSrcPitch(srcStrideX, srcUStrideX, srcVStrideX);
        status = m_pOverlayEngine->setSrcCrop(0, 0, srcWidth, srcHeight);
        status = m_pOverlayEngine->setSrcResolution(srcWidth, srcHeight, format);
        status = m_pOverlayEngine->setDstPosition(dstWidth, dstHeight, layer->displayFrame.left, layer->displayFrame.top);
        status = m_pOverlayEngine->drawImage((void*)nAddrY, (void*)nAddrU, (void*)nAddrV, length, 1);

        if(NO_ERROR != status){
            LOGE("ERROR! Error happens in commit image to overlay device, status = %d.", status);
        }

        if(++m_nFrameCount == 1){
            status = m_pOverlayEngine->setStreamOn(true);
        }

        int32_t nFenceFd = m_pOverlayEngine->getReleaseFd();

        Mutex::Autolock lock(m_mutexLock);
        layer->releaseFenceFd = nFenceFd;

        //m_shadowHwcLayer.handle = layer->handle;
        m_pShadowAddr = (void*)ph->physAddr;
        return;
    }

    void onCommitFinished()
    {
        updateFenceStatus();
    }

    void updateFenceStatus()
    {
    }

    uint32_t resolveImageSize(uint32_t nFormat, uint32_t nWidth, uint32_t nHeight)
    {
        switch (nFormat)
        {
            case HAL_PIXEL_FORMAT_YCbCr_420_P:
            {
                return (uint32_t)(nWidth * nHeight * 1.5);
            }
            case HAL_PIXEL_FORMAT_YV12:
            {
                return (uint32_t)(nWidth * nHeight * 1.5);
            }
            default:
                LOGE("ERROR! Can not resolve image size for format %d.", nFormat);
                return (uint32_t)(nWidth * nHeight * 4);
        }
    }

    void resolveImageAddr(uint32_t nFormat, uint32_t pPhysAddr, uint32_t srcStrideX, uint32_t srcStrideY,
                          uint32_t* pAddrY, uint32_t* pAddrU, uint32_t* pAddrV)
    {
        switch (nFormat)
        {
            case HAL_PIXEL_FORMAT_YCbCr_420_P:
            {
                *pAddrY = pPhysAddr;
                *pAddrU = *pAddrY + (srcStrideX * srcStrideY);
                *pAddrV = *pAddrU + ((srcStrideX * srcStrideY) >> 2);
                return;
            }
            case HAL_PIXEL_FORMAT_YV12:
            {
                *pAddrY = pPhysAddr;
                *pAddrU = *pAddrY + (srcStrideX * srcStrideY);
                *pAddrV = *pAddrU + ((srcStrideX * srcStrideY) >> 2);
                return;
            }
            default:
                LOGE("ERROR! Can not resolve image addr because of a unsupported format %d.", nFormat);
        }
    }

    void resolveImageStride(uint32_t nFormat, uint32_t srcStrideX, uint32_t* pUStrideX, uint32_t* pVStrideX)
    {
        switch (nFormat)
        {
            case HAL_PIXEL_FORMAT_YCbCr_420_P:
            {
                *pUStrideX = srcStrideX >> 1;
                *pVStrideX = srcStrideX >> 1;
                return;
            }
            case HAL_PIXEL_FORMAT_YV12:
            {
                *pUStrideX = srcStrideX >> 1;
                *pVStrideX = srcStrideX >> 1;
                return;
            }
            default:
                LOGE("ERROR! Can not resolve image strid because of a unsupported format %d.", nFormat);
        }
    }

    void dump(String8& result, char* buffer, int size)
    {
        Mutex::Autolock lock(m_mutexLock);

        sprintf(buffer, "Overlay Device Info\n");
        result.append(buffer);
    }

    bool readyDrawOverlay(){
        return m_nFrameCount > DMA_DELAY_FRAME_NUM;
    }

private:

    ///< talk to device driver.
    sp<IDisplayEngine> m_pOverlayEngine;

    ///< status.
    bool m_bOpen;

    uint32_t m_nFrameCount;

    //hwc_layer_1_t m_shadowHwcLayer;
    void* m_pShadowAddr;

    Mutex m_mutexLock;
};

sp<OverlayDevice> createOverlay(uint32_t nDisplayType);

}


#endif
