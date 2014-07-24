/*
 * (C) Copyright 2010 Marvell Int32_Ternational Ltd.
 * All Rights Reserved
 *
 * MARVELL CONFIDENTIAL
 * Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Marvell Int32_Ternational Ltd or its
 * suppliers or licensors. Title to the Material remains with Marvell Int32_Ternational Ltd
 * or its suppliers and licensors. The Material contains trade secrets and
 * proprietary and confidential information of Marvell or its suppliers and
 * licensors. The Material is protected by worldwide copyright and trade secret
 * laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Marvell's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other int32_tellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such int32_tellectual property rights must be
 * express and approved by Marvell in writing.
 *
 */

#ifndef __FB_OVERLAY_H__
#define __FB_OVERLAY_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cutils/log.h>
#include <utils/Singleton.h>
#include <utils/Errors.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/CallStack.h>
#include <utils/Log.h>
#include <cutils/properties.h>

#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>

#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include "IDisplayEngine.h"
#include "video/mmp_ioctl.h"

namespace android{

#define FBOVLYLOG(...)                                    \
    do{                                                   \
        char value[PROPERTY_VALUE_MAX];                   \
        property_get("persist.dms.fbovly.log", value, "0");   \
        bool bLog = (atoi(value) == 1);                   \
        if(bLog){                                         \
            LOGD(__VA_ARGS__);                            \
        }                                                 \
    }while(0)                                             \

#define FBOVLYWRAPPERLOG(...)                                       \
    do{                                                             \
        char buffer[1024];                                          \
        sprintf(buffer, __VA_ARGS__);                               \
        FBOVLYLOG("%s: %s", this->m_strDevName.string(), buffer);   \
    }while(0)                                                       \


class FBOverlayRef : public IDisplayEngine
{
public:
    FBOverlayRef(const char* pDev) : m_fd(-1)
                                   , m_strDevName(pDev)
                                   , m_nFrameCount(0)
                                   , m_releaseFd(-1)
    {
        memset(&m_mmpSurf, 0, sizeof(m_mmpSurf));
    }

    ~FBOverlayRef(){close();}

public:

    status_t getCaps(DisplayEngineCaps& caps) {return NO_ERROR;}
    status_t getMode(DisplayEngineMode& mode) {return NO_ERROR;}
    status_t getLayout(DisplayEngineLayout& layout) {return NO_ERROR;}
    status_t setMode(const DisplayEngineMode& mode) {return NO_ERROR;}
    status_t setLayout(const DisplayEngineLayout& layout) {return NO_ERROR;}

    status_t open()
    {
        const char* ov_device = m_strDevName.string();
        m_fd = ::open(ov_device, O_RDWR | O_NONBLOCK);
        if( m_fd < 0 ) {
            LOGE("Open overlay device[%s] fail", ov_device);
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.yPitch = %d.", yPitch);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.uPitch = %d.", uPitch);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.vPitch = %d.", vPitch);

        m_mmpSurf.win.pitch[0] = yPitch;
        m_mmpSurf.win.pitch[1] = uPitch;
        m_mmpSurf.win.pitch[2] = vPitch;
        return NO_ERROR;
    }

    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        uint32_t nColorFmt = ResolveColorFormat(srcFormat);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoMode = %d.", nColorFmt);
        m_mmpSurf.win.pix_fmt = nColorFmt;
        return NO_ERROR;
    }

    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.srcWidth = %d.", r-l);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.srcHeight = %d.", b-t);

        m_mmpSurf.win.xsrc = r-l;
        m_mmpSurf.win.ysrc = b-t;
        return NO_ERROR;
    }

    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        struct mmp_colorkey_alpha colorkey;
        colorkey.mode = ck_type;
        colorkey.alphapath = alpha_type;
        colorkey.config = alpha_value;
        colorkey.y_coloralpha = ck_r;
        colorkey.u_coloralpha = ck_g;
        colorkey.v_coloralpha = ck_b;

        if( ioctl(m_fd, FB_IOCTL_SET_COLORKEYnALPHA, &colorkey) ) {
            LOGE("OVERLAY_SetConfig: Set color key failed");
            return -EIO;
        }
        return NO_ERROR;
    }

    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomXSize = %d.", width);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomYSize = %d.", height);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortOffset.xOffset = %d.", xOffset);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortOffset.yOffset = %d.", yOffset);

        m_mmpSurf.win.xdst = width;
        m_mmpSurf.win.ydst = height;
        m_mmpSurf.win.xpos = xOffset;
        m_mmpSurf.win.ypos = yOffset;
        return NO_ERROR;
    }

    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        return NO_ERROR;
    }

    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t length, uint32_t addrType)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.frameID     = %d.", m_nFrameCount);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.startAddr   = %p.", yAddr);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.length      = %d.", length);
        m_mmpSurf.addr.phys[0] = (uint32_t)yAddr;
        m_mmpSurf.addr.phys[1] = (uint32_t)uAddr;
        m_mmpSurf.addr.phys[2] = (uint32_t)vAddr;

        if( ioctl(m_fd, FB_IOCTL_FLIP_USR_BUF, &m_mmpSurf) ) {
            LOGE("ioctl flip_vid failed");
            m_releaseFd = -1;
            return -EIO;
        }

        m_nFrameCount++;
        m_releaseFd = m_mmpSurf.fence_fd;
        return NO_ERROR;
    }

    status_t configVdmaStatus(DISPLAY_VDMA& vdma)
    {
        return NO_ERROR;
    }

    status_t waitVSync(DISPLAY_SYNC_PATH path)
    {
        return NO_ERROR;
    }

    status_t setStreamOn(bool bOn)
    {
        int status = bOn ? 1 : 0;
        FBOVLYWRAPPERLOG("Stream %s.", bOn ? "on" : "off");
        if( ioctl(m_fd, FB_IOCTL_ENABLE_DMA, &status) ) {
            LOGE("ioctl OVERLAY %s FB_IOCTL_SWITCH_VID_OVLY failed", m_strDevName.string());
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t set3dVideoOn(bool bOn, uint32_t mode){return NO_ERROR;}

    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nImgNum)
    {
        return NO_ERROR;
    }

    status_t close()
    {
        setStreamOn(false);
        m_nFrameCount = 0;

        if(m_fd > 0){
            ::close(m_fd);
        }

        return NO_ERROR;
    }

    int32_t getFd() const {return m_fd;}

    int32_t getReleaseFd() const {return m_releaseFd;}

    const char* getName() const{return m_strDevName.string();}

private:

    int32_t m_fd;

    ///< dev name
    String8 m_strDevName;

    ///< frame count
    uint32_t m_nFrameCount;

    ///< mmp_surf in new LCD driver.
    struct mmp_surface m_mmpSurf;

    ///< release fd
    int32_t m_releaseFd;
};

}
#endif

