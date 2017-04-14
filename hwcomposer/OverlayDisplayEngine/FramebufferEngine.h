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

#ifndef __FB_BASELAYER_ENGINE_H__
#define __FB_BASELAYER_ENGINE_H__

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
#include <cutils/properties.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/CallStack.h>
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>

#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include "IDisplayEngine.h"
#include "fb_ioctl.h"

namespace android{

#define FBBASELOG(...)                                    \
    do{                                                   \
        char value[PROPERTY_VALUE_MAX];                   \
        property_get("persist.dms.fb.log", value, "0");   \
        bool bLog = (atoi(value) == 1);                   \
        if(bLog){                                         \
            LOGD(__VA_ARGS__);                            \
        }                                                 \
    }while(0)                                             \

#define FBBASEWRAPPERLOG(...)                                       \
    do{                                                             \
        char buffer[1024];                                          \
        sprintf(buffer, __VA_ARGS__);                               \
        FBBASELOG("%s: %s", this->m_strDevName.string(), buffer);   \
    }while(0)                                                       \


class FBBaseLayer : public IDisplayEngine
{
public:
    FBBaseLayer(const char* pDev)
        : m_fd(-1)
        , m_strDevName(pDev)
        , m_displayMode()
        , m_displayLayout()
    {
        memset(&m_overlaySurf, 0, sizeof(m_overlaySurf));
    }

    ~FBBaseLayer(){close();}

public:
    status_t open()
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        const char* ov_device = m_strDevName.string();
        m_fd = ::open(ov_device, O_RDWR);
        if( m_fd < 0 ) {
            LOGE("Open overlay device[%s] fail", ov_device);
            return -EIO;
        }

        int32_t ret = ioctl(m_fd, FB_IOCTL_WAIT_VSYNC_ON, NULL);
        if (ret < 0){
            LOGE("enable base layer vsync failed.");
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t getCaps(DisplayEngineCaps& caps) {return NO_ERROR;}

    status_t getLayout(DisplayEngineLayout& caps)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        if(isOpen()){
            struct ::fb_var_screeninfo varInfo;
            int32_t ret = ioctl(m_fd, FBIOGET_VSCREENINFO, &varInfo);
            if (ret < 0)
            {
                LOGE("ioctl(m_fd, FBIOGET_VSCREENINFO, &varInfo) failed.");
                return ret;
            }

            struct ::fb_fix_screeninfo fb_finfo;
            ret = ioctl(m_fd, FBIOGET_FSCREENINFO, &fb_finfo);
            if (ret < 0){
                LOGE("ioctl(pCrtc->io_fd, FBIOGET_FSCREENINFO, &fb_finfo) failed.");
                return ret;
            }

            caps.from(varInfo, fb_finfo);
            m_displayLayout.from(varInfo, fb_finfo);
            return NO_ERROR;
        }

        return -EIO;
    }

    status_t setLayout(const DisplayEngineLayout& caps)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        if(isOpen()){
            struct ::fb_var_screeninfo varInfo;

            //caps.to(varInfo);
            ResolveScreenInfo(varInfo, caps, m_displayMode);
            int32_t ret = ioctl(m_fd, FBIOPUT_VSCREENINFO, &varInfo);
            if (ret < 0){
                LOGE("ioctl(pCrtc->io_fd, FBIOGET_FSCREENINFO, &fb_finfo) failed.");
                return ret;
            }

            m_displayLayout = caps;
            return NO_ERROR;
        }

        return -EIO;
    }

    status_t getMode(DisplayEngineMode& caps)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        if(isOpen()){
            struct ::fb_var_screeninfo varInfo;
            int32_t ret = ioctl(m_fd, FBIOGET_VSCREENINFO, &varInfo);
            if (ret < 0)
            {
                LOGE("ioctl(m_fd, FBIOGET_VSCREENINFO, &varInfo) failed.");
                return ret;
            }

            m_displayMode.from(varInfo);
            caps.from(varInfo);
            //caps = displaymode;
            return NO_ERROR;
        }

        return -EIO;
    }

    status_t setMode(const DisplayEngineMode& caps)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        if(isOpen()){
            struct ::fb_var_screeninfo varInfo;
            ResolveScreenInfo(varInfo,  m_displayLayout, caps);
            int32_t ret = ioctl(m_fd, FBIOPUT_VSCREENINFO, &varInfo);
            if (ret < 0){
                LOGE("ioctl(pCrtc->io_fd, FBIOGET_FSCREENINFO, &fb_finfo) failed.");
                return ret;
            }

            m_displayMode = caps;
            return NO_ERROR;
        }

        return -EIO;
    }

    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        m_overlaySurf.viewPortInfo.yPitch = yPitch;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.yPitch = %d", m_overlaySurf.viewPortInfo.yPitch);
        m_overlaySurf.viewPortInfo.uPitch = uPitch;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.uPitch = %d", m_overlaySurf.viewPortInfo.uPitch);
        m_overlaySurf.viewPortInfo.vPitch = vPitch;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.vPitch = %d", m_overlaySurf.viewPortInfo.vPitch);
        return NO_ERROR;
    }

    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        m_overlaySurf.videoMode = ResolveColorFormat(srcFormat);
        //m_overlaySurf.videoMode = FB_VMODE_RGB565; // ResolveColorFormat(srcFormat);
        FBBASEWRAPPERLOG("m_overlaySurf.videoMode = %d", m_overlaySurf.videoMode);
        return NO_ERROR;
    }

    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        m_overlaySurf.viewPortInfo.srcWidth = r - l;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.srcWidth = %d", m_overlaySurf.viewPortInfo.srcWidth);
        m_overlaySurf.viewPortInfo.srcHeight = b - t;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.srcHeight = %d", m_overlaySurf.viewPortInfo.srcHeight);
        return NO_ERROR;
    }

    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        struct _sColorKeyNAlpha colorkey;
        colorkey.config = alpha_value;
        colorkey.alphapath = alpha_type;
        colorkey.mode = ck_type;
        colorkey.Y_ColorAlpha = ck_r;
        colorkey.U_ColorAlpha = ck_g;
        colorkey.V_ColorAlpha = ck_b;

        if( ioctl(m_fd, FB_IOCTL_SET_COLORKEYnALPHA, &colorkey) ) {
            LOGE("OVERLAY_SetConfig: Set color key failed");
            return -EIO;
        }
        return NO_ERROR;
    }

    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        m_overlaySurf.viewPortInfo.zoomXSize = width;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomXSize = %d", m_overlaySurf.viewPortInfo.zoomXSize);
        m_overlaySurf.viewPortInfo.zoomYSize = height;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomYSize = %d", m_overlaySurf.viewPortInfo.zoomYSize);
        m_overlaySurf.viewPortOffset.xOffset = xOffset;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortOffset.xOffset = %d", m_overlaySurf.viewPortOffset.xOffset);
        m_overlaySurf.viewPortOffset.yOffset = yOffset;
        FBBASEWRAPPERLOG("m_overlaySurf.viewPortOffset.yOffset = %d", m_overlaySurf.viewPortOffset.yOffset);
        return NO_ERROR;
    }

    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        struct mvdisp_partdisp partialDisplay;
        partialDisplay.id = 0;
        partialDisplay.horpix_start = l;
        partialDisplay.horpix_end = r;
        partialDisplay.vertline_start = t;
        partialDisplay.vertline_end = b;
        partialDisplay.color = color;

        if (ioctl(m_fd, FB_IOCTL_GRA_PARTDISP, &partialDisplay) < 0) {
            ALOGE("ERROR: Fail to set partial display!");
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t length, uint32_t addrType)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);

        m_overlaySurf.videoBufferAddr.frameID     = 0;
        FBBASEWRAPPERLOG("m_overlaySurf.videoBufferAddr.frameID     = %d", m_overlaySurf.videoBufferAddr.frameID    );
        m_overlaySurf.videoBufferAddr.startAddr[0] = (unsigned char*)yAddr;
        FBBASEWRAPPERLOG("m_overlaySurf.videoBufferAddr.startAddr[0] = %p", m_overlaySurf.videoBufferAddr.startAddr[0]);
        m_overlaySurf.videoBufferAddr.startAddr[1]= (unsigned char *)uAddr;
        m_overlaySurf.videoBufferAddr.startAddr[2]= (unsigned char *)vAddr;

        m_overlaySurf.videoBufferAddr.inputData   = 0;
        FBBASEWRAPPERLOG("m_overlaySurf.videoBufferAddr.inputData   = %p", m_overlaySurf.videoBufferAddr.inputData  );
        m_overlaySurf.videoBufferAddr.length      = length;
        FBBASEWRAPPERLOG("m_overlaySurf.videoBufferAddr.length      = %d", m_overlaySurf.videoBufferAddr.length     );

        struct timeval old_time, cur_time;
        gettimeofday(&old_time, NULL);

        if( ioctl(m_fd, FB_IOCTL_FLIP_VID_BUFFER, &m_overlaySurf) ) {
            LOGE("ioctl flip_vid failed");
            return -EIO;
        }

        gettimeofday(&cur_time, NULL);
        int us_time = (1000000 * (cur_time.tv_sec - old_time.tv_sec ) + cur_time.tv_usec - old_time.tv_usec);

        if (us_time >= 2000)
        {
            LOGD("flip video buffer too long, time duation %d us", us_time);
        }

        return NO_ERROR;
    }


    status_t configVdmaStatus(DISPLAY_VDMA& vdma)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        struct mvdisp_vdma  disp_vdma;
        disp_vdma.path = vdma.path;
        disp_vdma.layer = vdma.layer;
        disp_vdma.enable = vdma.enable;

        if( ioctl(m_fd, FB_IOCTL_VDMA_SET, &disp_vdma) ) {
            LOGE("ioctl BASE %s FB_IOCTL_VDMA_EN failed", m_strDevName.string());
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t waitVSync(DISPLAY_SYNC_PATH path)
    {
        FBBASEWRAPPERLOG("%s in. path = %d.", __FUNCTION__, path);
        uint32_t sync = 0;
#if 0
        switch (path)
        {
            case DISPLAY_SYNC_SELF:
                sync = SYNC_SELF;
                break;
            case DISPLAY_SYNC_PANEL:
                sync = SYNC_PANEL;
                break;
            case DISPLAY_SYNC_TV:
                sync = SYNC_TV;
                break;
            case DISPLAY_SYNC_PANEL_TV:
                sync = SYNC_PANEL_TV;
                break;
            default:
                LOGE("Unknown sync path selected!");
                return -EIO;
        }
#else
        sync = path;
#endif

        struct timeval old_time, cur_time;
        gettimeofday(&old_time, NULL);

        if( ioctl(m_fd, FB_IOCTL_WAIT_VSYNC, path) ) {
            LOGE("ioctl BASE %s FB_IOCTL_VDMA_EN failed", m_strDevName.string());
            return -EIO;
        }

        gettimeofday(&cur_time, NULL);
        int us_time = (1000000 * (cur_time.tv_sec - old_time.tv_sec ) + cur_time.tv_usec - old_time.tv_usec);

        if (us_time >= 25000)
        {
            LOGD("Wait vsync path %d too long, time duation %d us", path, us_time);
        }

        return NO_ERROR;
    }

    status_t setStreamOn(bool bOn)
    {
        FBBASEWRAPPERLOG("%s in. set stream to %s.", __FUNCTION__, bOn ? "on" : "off");
        int status = bOn ? 1 : 0;

        if( ioctl(m_fd, FB_IOCTL_SWITCH_GRA_OVLY, &status) ) {
            LOGE("ioctl OVERLAY %s FB_IOCTL_SWITCH_VID_OVLY failed", m_strDevName.string());
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t set3dVideoOn(bool bOn, uint32_t mode){return NO_ERROR;}

    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nImgNum)
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        if( ioctl(m_fd, FB_IOCTL_GET_FREELIST, vAddr) ) {
            LOGE("ioctl FB_IOCTL_GET_FREELSIT failed");
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t close()
    {
        FBBASEWRAPPERLOG("%s in.", __FUNCTION__);
        setStreamOn(false);

        if(m_fd > 0){
            ::close(m_fd);
        }

        return NO_ERROR;
    }

    int32_t getFd() const {return m_fd;}

    int32_t getReleaseFd() const {return m_fd;}

    const char* getName() const{return m_strDevName.string();}

private:
    bool isOpen(){
        return m_fd > 0;
    }

    ///< dev fd.
    int32_t m_fd;

    ///< dev name.
    String8 m_strDevName;

    ///< fb overlay surface.
    struct _sOvlySurface  m_overlaySurf;

    ///< current setted mode & layout.
    DisplayEngineMode     m_displayMode;
    DisplayEngineLayout   m_displayLayout;

};

}
#endif

