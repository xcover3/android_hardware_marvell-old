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

#ifndef __V4L2_OVERLAY_H__
#define __V4L2_OVERLAY_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/properties.h>
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

#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>

#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include "videodev2.h"
//#include "dms_private.h"
#include "IDisplayEngine.h"

namespace android
{

#define MAX_FRAME_BUFFERS 7


#define V4L2LOG(...)                                    \
    do{                                                 \
        char value[PROPERTY_VALUE_MAX];                 \
        property_get("v4l2.wrapper.log", value, "0");   \
        bool bLog = (atoi(value) == 1);                 \
        if(bLog){                                       \
            LOGD(__VA_ARGS__);                          \
        }                                               \
    }while(0)                                           \

#define V4L2WRAPPERLOG(...)                                         \
    do{                                                             \
        char buffer[1024];                                          \
        sprintf(buffer, __VA_ARGS__);                               \
        V4L2LOG("%s: %s", this->m_strDevName.string(), buffer);     \
    }while(0)                                                       \



class VideoSourceInfo // : public __IMAGE_CONVERT_INFO
{
public:
    VideoSourceInfo(): m_nWidth(0)
                     , m_nHeight(0)
                     , m_nFormat(0)
                     , m_nPitchY(0)
                     , m_nPitchU(0)
                     , m_nPitchV(0)
                     , m_nCropL(0)
                     , m_nCropT(0)
                     , m_nCropB(0)
                     , m_nCropR(0)
                     , m_bMultiPlanes(false)
    {}

    ~VideoSourceInfo(){}


    VideoSourceInfo& operator = (const VideoSourceInfo& rhs)
    {
        m_nWidth = rhs.m_nWidth;
        m_nHeight = rhs.m_nHeight;
        m_nFormat = rhs.m_nFormat;
        m_nPitchY = rhs.m_nPitchY;
        m_nPitchU = rhs.m_nPitchU;
        m_nPitchV = rhs.m_nPitchV;
        m_nCropL = rhs.m_nCropL;
        m_nCropT = rhs.m_nCropT;
        m_nCropB = rhs.m_nCropB;
        m_nCropR = rhs.m_nCropR;
        m_bMultiPlanes = rhs.m_bMultiPlanes;
        return *this;
    }

    void clear()
    {
        m_nWidth = 0;
        m_nHeight = 0;
        m_nFormat = 0;
        m_nPitchY = 0;
        m_nPitchU = 0;
        m_nPitchV = 0;
        m_bMultiPlanes = false;
    }


public:
    void setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        m_nPitchY = yPitch;
        m_nPitchU = uPitch;
        m_nPitchV = vPitch;
    }

    void setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        m_nWidth = srcWidth;
        m_nHeight = srcHeight;
        m_nFormat = srcFormat;
    }

    void setMultiPlaneFlag(bool on){ m_bMultiPlanes = on; }

    bool operator == (const VideoSourceInfo& rhs) const
    {
        return (m_nWidth == rhs.m_nWidth) &&
            (m_nHeight == rhs.m_nHeight) &&
            (m_nFormat == rhs.m_nFormat) &&
            (m_nPitchY == rhs.m_nPitchY) &&
            (m_nPitchU == rhs.m_nPitchU) &&
            (m_nPitchV == rhs.m_nPitchV) &&
            (m_nCropL == rhs.m_nCropL) &&
            (m_nCropT == rhs.m_nCropT) &&
            (m_nCropB == rhs.m_nCropB) &&
            (m_nCropR == rhs.m_nCropR) &&
            (m_bMultiPlanes == rhs.m_bMultiPlanes);
    }

    bool operator != (const VideoSourceInfo& rhs) const
    {
        return !(rhs == *this);
    }

private:
    friend class V4L2OverlayRef;

#if 0
    uint32_t getBpp(uint32_t fmt)
    {
        switch (fmt) {
            case DISP_FOURCC_RGB565: //FB_VMODE_RGB565:
            case DISP_FOURCC_UYVY: //FB_VMODE_YUV422PACKED:
            case DISP_FOURCC_YUY2:
                return 16;
                break;
            case DISP_FOURCC_ABGR8888:
            case DISP_FOURCC_ARGB8888:
                return 32;
                break;
            case DISP_FOURCC_I420: //FB_VMODE_YUV420PLANAR:
            case DISP_FOURCC_YV12: //FB_VMODE_YUV420PLANAR_SWAPUV:
                return 12;
                break;
            default:
                LOGE("Pixformat bpp might error, please check the input color format. Now use default 16.");
                return 16;
        }
    }
#endif

private:

    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nFormat;
    uint32_t m_nPitchY;
    uint32_t m_nPitchU;
    uint32_t m_nPitchV;
    uint32_t m_nCropL;
    uint32_t m_nCropT;
    uint32_t m_nCropB;
    uint32_t m_nCropR;
    uint32_t m_nBpp;
    bool m_bMultiPlanes;
};


class V4L2OverlayRef : public IDisplayEngine
{
public:
    enum ADDRTYPE{
        V4L2_VIRTUAL_ADDRESS = 0,
        V4L2_PHYSICAL_ADDRESS = 1,
    };

public:
    V4L2OverlayRef(const char* pDev) : m_fd(-1)
                                     , m_bFirstFrame(true)
                                     , m_bStreamOn(false)
                                     , m_iFrameCount(0)
                                     , m_nCapability(MAX_FRAME_BUFFERS)
                                     , m_iBufferIndex(0)
                                     , m_nBufferNum(MAX_FRAME_BUFFERS)
                                     , m_strDevName(pDev)
                                     , m_iFlag(FLAG_CLOSE)
                                     , m_nDstWidth(0)
                                     , m_nDstHeight(0)
                                     , m_nXoffset(0)
                                     , m_nYoffset(0)
                                     , m_nGlobalAlpha(0)
                                     , m_nColorKey(0)
                                     , m_userSetting()
                                     , m_wrapperSetting()
                                     , m_nMode3d(0)
        {
            for(uint32_t i = 0; i < m_nBufferNum; ++i){
                m_vBufferAddr.push(NULL);
            }

            m_vDeferFreeAddr.clear();

            memset(m_plane, 0, sizeof(m_plane));
        }

    ~V4L2OverlayRef(){close();}


public:
    status_t open();

    status_t getCaps(DisplayEngineCaps& caps) {return NO_ERROR;}
    status_t getMode(DisplayEngineMode& mode) {return NO_ERROR;}
    status_t getLayout(DisplayEngineLayout& layout) {return NO_ERROR;}
    status_t setMode(const DisplayEngineMode& mode) {return NO_ERROR;}
    status_t setLayout(const DisplayEngineLayout& layout) {return NO_ERROR;}
    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch);
    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat);
    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b);
    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b);
    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset);
    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color);
    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t lenght, uint32_t addrType = V4L2_PHYSICAL_ADDRESS);
    status_t configVdmaStatus(DISPLAY_VDMA& vdma){return NO_ERROR;}
    status_t waitVSync(DISPLAY_SYNC_PATH path){return NO_ERROR;}
    status_t setStreamOn(bool bOn);
    status_t set3dVideoOn(bool bOn, uint32_t mode);
    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nNumber);
    status_t close();
    int32_t getFd() const {return m_fd;}
    int32_t getReleaseFd() const {return m_fd;}
    const char* getName() const{return m_strDevName.string();}

private:
    bool isEnabled(){return m_fd > 0;}

    int32_t getBufferIndex(){return m_iBufferIndex;}

    bool canDraw();

    status_t requestBuffer();

    status_t setCapability(uint32_t capability = MAX_FRAME_BUFFERS)
    {
        uint32_t caps = capability;
        if(capability > MAX_FRAME_BUFFERS){
            V4L2WRAPPERLOG("capability set to MAX_FRAME_BUFFERS(%d).", MAX_FRAME_BUFFERS);
            caps == MAX_FRAME_BUFFERS;
        }

        m_nCapability = caps;
        if(NO_ERROR != requestBuffer()){
            LOGE("Can not alloc buffer properly.");
            return -EIO;
        }

        return NO_ERROR;
    }

    bool needSetDstPositionToDrv(uint32_t width, uint32_t height,
                                 uint32_t xOffset, uint32_t yOffset){
        return ((m_nDstWidth != width) || (m_nDstHeight != height) ||
                (m_nXoffset != xOffset) || (m_nYoffset != yOffset));
    }

    status_t setSrcPitchAndResolustion(VideoSourceInfo& videoInfo);

    status_t getConsumedImage(void*& vAddr);

    bool isDuplicatedDraw(void* yAddr, void* vAddr, void* uAddr, int length){
        V4L2WRAPPERLOG("%s m_vBufferAddr[%d] = %p.", __func__, m_iBufferIndex, m_vBufferAddr[m_iBufferIndex]);
        return m_vBufferAddr[m_iBufferIndex] == yAddr;
    }

private:
    enum
    {
        FLAG_OPEN           = 0x00000001,
        FLAG_SRC_RESOLUTION = 0x00000002,
        FLAG_SRC_CROP       = 0x00000004,
        FLAG_DST_POSITION   = 0x00000008,
        FLAG_SET_PITCH      = 0x00000020,
        FLAG_SET_STREAM_ON  = 0x00000040,
        FLAG_REQUEST_BUFFER = 0x00000080,
        FLAG_SET_COLOR_KEY  = 0x00000100,
        FLAG_ON_DRAW        = 0x00000100,
        FLAG_CLOSE          = 0x00000000,
    };

private:
    ///< fd
    int32_t  m_fd;

    bool m_bFirstFrame;
    bool m_bStreamOn;
    uint32_t  m_iFrameCount;


    ///< the number of buffer drv holds(request to drv)
    uint32_t  m_nCapability;

    ///< local buffer index management
    uint32_t  m_iBufferIndex;
    uint32_t  m_nBufferNum;
    Vector<void*> m_vBufferAddr;
    Vector<void*> m_vDeferFreeAddr;

    ///< dev name
    String8 m_strDevName;

    ///< status flag
    uint32_t m_iFlag;

    ///< status members
    uint32_t m_nDstWidth;
    uint32_t m_nDstHeight;
    uint32_t m_nXoffset;
    uint32_t m_nYoffset;
    uint32_t m_nGlobalAlpha;
    uint32_t m_nColorKey;

    VideoSourceInfo m_userSetting;
    VideoSourceInfo m_wrapperSetting;

    ///< Planes for 3D video
    v4l2_plane m_plane[2];
    uint32_t m_nMode3d;
};

}// end of namespace

#endif
