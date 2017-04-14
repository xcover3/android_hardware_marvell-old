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
//#define LOG_NDEBUG 0

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/android_pmem.h>
#include <cutils/ashmem.h>

#ifdef  LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "V4L2Overlay"

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

#include "fb_ioctl.h"
#include "OverlayDevice.h"
#include "V4L2Overlay.h"
#include "V4L2OverlayHelper.h"

namespace android
{

    status_t V4L2OverlayRef::open()
    {
        if(m_iFlag & FLAG_OPEN){
            V4L2WRAPPERLOG("Already open");
            return NO_ERROR;
        }

        const char* ov_device = m_strDevName.string();
        m_fd = ::open(ov_device, O_RDWR | O_NONBLOCK);
        if( m_fd <= 0 ) {
            LOGE("Open overlay device[%s] fail", ov_device);
            return -EIO;
        }
        m_iFlag |= FLAG_OPEN;

        return NO_ERROR;
    }


    status_t V4L2OverlayRef::setSrcPitch(uint32_t srcYPitch, uint32_t srcUPitch, uint32_t srcVPitch)
    {
        V4L2WRAPPERLOG("%s, srcYPitch(%d), srcUPitch(%d), srcVPitch(%d).", __FUNCTION__, srcYPitch, srcUPitch, srcVPitch);
        if (!isEnabled()) {
            return -EIO;
        }

        m_userSetting.setSrcPitch(srcYPitch, srcUPitch, srcVPitch);

        m_iFlag |= FLAG_SET_PITCH;

        if((m_iFlag & FLAG_SRC_RESOLUTION) && (m_iFlag & FLAG_SRC_CROP)){
            return setSrcPitchAndResolustion(m_userSetting);
        }

        return NO_ERROR;
    }

    status_t V4L2OverlayRef::setSrcResolution(int srcWidth, int srcHeight, int srcFormat)
    {
        V4L2WRAPPERLOG("%s srcWidth = %d, srcHeight = %d, srcFormat = 0x%x.", __func__, srcWidth, srcHeight, srcFormat);
        if (!isEnabled()) {
            return -EIO;
        }

        m_userSetting.setSrcResolution(srcWidth, srcHeight, srcFormat);

        m_iFlag |= FLAG_SRC_RESOLUTION;

        if((m_iFlag & FLAG_SRC_CROP) && (m_iFlag & FLAG_SET_PITCH)){
            return setSrcPitchAndResolustion(m_userSetting);
        }

        return NO_ERROR;
    }

    status_t V4L2OverlayRef::requestBuffer()
    {
        v4l2_buf_type nType = (m_wrapperSetting.m_bMultiPlanes ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
        if(!(m_iFlag & FLAG_REQUEST_BUFFER)){
            V4L2WRAPPERLOG("%s request %d buffers.", __FUNCTION__, m_nCapability);
            struct  v4l2_requestbuffers requestbuffers;
            memset(&requestbuffers,0,sizeof(requestbuffers));
            requestbuffers.type     = nType;
            requestbuffers.memory   = V4L2_MEMORY_USERPTR;
            requestbuffers.count    = m_nCapability;
            if (v4l2_overlay_ioctl(m_fd, VIDIOC_REQBUFS, &requestbuffers, "request buffer")) {
                LOGE("Error: can't require buffer");
                return -EIO;
            }

            m_iFlag |= (m_nCapability > 0) ? FLAG_REQUEST_BUFFER : 0; //if no cap set, Flag don't change.
        }

        return NO_ERROR;
    }

    status_t V4L2OverlayRef::setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        if (!isEnabled()) {
            return -EIO;
        }

        uint32_t nColorKey = (((ck_r & 0xFF) << 16) | ((ck_g & 0xFF) << 8) | (ck_b & 0xFF));
        if(!(m_iFlag & FLAG_SET_COLOR_KEY) ||
           ((m_nColorKey != (uint32_t)nColorKey) ||
            (m_nGlobalAlpha != (uint32_t)alpha_value))) {
            uint32_t nEnable = (alpha_type == DISP_OVLY_GLOBAL_ALPHA);
            if (v4l2_overlay_set_global_alpha(m_fd, nEnable, alpha_value)){
                LOGE("Set global alpha fail\n");
                return -EIO;
            }

            nEnable = (ck_type == DISP_OVLY_COLORKEY_RGB);
            if (v4l2_overlay_set_colorkey(m_fd, nEnable, nColorKey)){
                return -EIO;
            }

            m_nColorKey = nColorKey;
            m_nGlobalAlpha = alpha_value;
        }
        return NO_ERROR;
    }

    status_t V4L2OverlayRef::setDstPosition(int width, int height, int xOffset, int yOffset)
    {
        V4L2WRAPPERLOG("%s width = %d, height = %d, xOffset = %d, yOffset = %d.", __func__, width, height, xOffset, yOffset);

        if (!isEnabled()) {
            return -EIO;
        }

        if((!(m_iFlag & FLAG_DST_POSITION)) || needSetDstPositionToDrv(width,  height,  xOffset,  yOffset)){
            m_nDstWidth = width;
            m_nDstHeight = height;
            m_nXoffset = xOffset;
            m_nYoffset = yOffset;

            if (v4l2_overlay_set_position(m_fd, xOffset, yOffset, width, height)){
                LOGE("can not update window position and size.");
                return -EIO;
            }
        }

        m_iFlag |= FLAG_DST_POSITION;
        return NO_ERROR;
    }

    status_t V4L2OverlayRef::setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        return NO_ERROR;
    }

    status_t V4L2OverlayRef::drawImage(void* yAddr,void* uAddr, void* vAddr, int length, uint32_t addrType)
    {
        V4L2WRAPPERLOG("%s addr(%p) and length(%d)", __FUNCTION__, yAddr, length);

        if(NULL == yAddr || 0 >= length || (!isEnabled())){
            LOGE("Invalid addr(%p) & length(%d) or Overlay not enabled.", yAddr, length);
            return -EINVAL;
        }

        if(!canDraw()){
            LOGE("Can not draw to overlay, preserved buffer all occupied, call getConsumedImage() to free them.");
            return -EINVAL;
        }

        struct  v4l2_buffer newbuffer;
        memset (&newbuffer, 0, sizeof(newbuffer));
        newbuffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        newbuffer.memory = V4L2_MEMORY_USERPTR;
        newbuffer.index = getBufferIndex();
        newbuffer.m.userptr = (unsigned long)yAddr;
        newbuffer.length = (length + 0xFFF) & (~0xFFF);
        if(1 == addrType){
            newbuffer.flags |= 0x80000000; //V4L2_BUF_FLAG_PHYADDR;
        }

        if(m_userSetting.m_bMultiPlanes){
            newbuffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            newbuffer.m.planes = m_plane;
            newbuffer.length = 2;
            struct v4l2_plane *p = newbuffer.m.planes;
            p[0].m.userptr   = (unsigned long)yAddr;
            p[0].length      = (length + 0xFFF) & (~0xFFF);
            V4L2WRAPPERLOG("p[0].m.userptr = %p, length = %d", (void*)p[0].m.userptr, p[0].length);
            uint32_t nPlaneOffset = m_nMode3d ?  ((m_userSetting.m_nHeight * m_userSetting.m_nPitchY) >> 1) : (m_userSetting.m_nPitchY >> 1);
            p[1].m.userptr   = (unsigned long)yAddr + nPlaneOffset;
            p[1].length      = (length + 0xFFF) & (~0xFFF);
            V4L2WRAPPERLOG("p[1].m.userptr = %p, length = %d", (void*)p[1].m.userptr, p[1].length);

        }

        if (-1 == v4l2_overlay_ioctl(m_fd, VIDIOC_QBUF, &newbuffer, "Flip buffer")) {
            if(!m_bFirstFrame){
                LOGE("Can't Flip buffer\n");
                return -EIO;
            }
        }

        m_vBufferAddr.editItemAt(newbuffer.index) = yAddr;
        m_iFlag |= FLAG_ON_DRAW;
        if (NO_ERROR != setStreamOn(true)){
            LOGE("%s switch stream on failed.", __FUNCTION__);
            return -EIO;
        }

        m_bFirstFrame = false;
        m_iFrameCount++;

        //Clear flge after each drawing.
        m_iFlag &= ~(FLAG_SRC_RESOLUTION | FLAG_SRC_CROP | FLAG_DST_POSITION | FLAG_SET_COLOR_KEY | FLAG_SET_PITCH);
        return NO_ERROR;
    }

    status_t V4L2OverlayRef::setStreamOn(bool bOn)
    {
        if(!isEnabled()){
            return -EINVAL;
        }

        if(bOn && (!(m_iFlag & FLAG_ON_DRAW))){
            V4L2WRAPPERLOG("None drawing before, defer this turn on");
            return NO_ERROR;
        }else if(!bOn && !(m_iFlag & FLAG_SET_STREAM_ON)){
            V4L2WRAPPERLOG("Not opened before, needn't close");
            return NO_ERROR;
        }

        v4l2_buf_type nType = (m_wrapperSetting.m_bMultiPlanes ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
        if (m_bFirstFrame || m_bStreamOn != bOn){
            V4L2WRAPPERLOG("%s set overlay to %s, now overlay status is %x, m_bFirstFrame(%d), m_bStreamOn(%d).", __FUNCTION__, bOn?"on":"off", m_iFlag, m_bFirstFrame, m_bStreamOn);
            uint32_t streamOnOff = (bOn) ? VIDIOC_STREAMON : VIDIOC_STREAMOFF;
            uint32_t buftype = nType; //V4L2_BUF_TYPE_VIDEO_OUTPUT;
            char msg[16] = {0};
            sprintf(msg, "%s", (bOn) ? "STREAMON" : "STREAMOFF");
            if(v4l2_overlay_ioctl(m_fd, streamOnOff, &buftype, msg)) {
                LOGE("Error: video overlay fail while %s.\n", msg);
                return -EIO;
            } else {
                m_bStreamOn = bOn;
            }
        }

        if(bOn)
            m_iFlag |= FLAG_SET_STREAM_ON;
        else{
            for(uint32_t i = 0; i < m_vBufferAddr.size(); ++i){
                if( NULL != m_vBufferAddr[i]){
                    m_vDeferFreeAddr.push(m_vBufferAddr[i]);
                    m_vBufferAddr.editItemAt(i) = NULL;
                }
            }

            m_bFirstFrame = true;
            m_iFrameCount = 0;

            // This flag can not be cleared here since we may set stream off after set resolution or pitch.
            // So, only stream on/off involved bits were touched.
            m_iFlag &= ~(FLAG_SET_STREAM_ON | FLAG_ON_DRAW | FLAG_REQUEST_BUFFER);
            setCapability(0); // Notify drv to clear the buffer used before.

            // Clear drv settings. do this as the last step of stream off since
            // some notification (as above) to drv need this setting.
            m_wrapperSetting.clear();

            V4L2WRAPPERLOG("%s, After stream off, overlay status is %x, m_bFirstFrame(%d), m_bStreamOn(%d).", __FUNCTION__, m_iFlag, m_bFirstFrame, m_bStreamOn);
        }

        return NO_ERROR;
    }

    status_t V4L2OverlayRef::close()
    {
        if (m_bStreamOn){
            if(NO_ERROR != setStreamOn(false)){
                LOGE("Error: switch off video overlay fail\n" );
                return -EIO;
            }
        }

        if(m_fd > 0){
            ::close(m_fd);
            m_fd = 0;
        }

        m_bFirstFrame = true;
        m_iFrameCount = 0;
        m_bStreamOn = false;
        m_iFlag = 0;
        m_iBufferIndex = 0;
        return NO_ERROR;
    }

    bool V4L2OverlayRef::canDraw()
    {
        for(uint32_t i = 0; i < m_vBufferAddr.size(); ++i){
            V4L2WRAPPERLOG("m_vBufferAddr[%d] = 0x%p.", i, m_vBufferAddr[i]);
        }

        for(uint32_t i = 0; i < m_vBufferAddr.size(); ++i){
            if( NULL == m_vBufferAddr[i]){
                m_iBufferIndex = i;
                V4L2WRAPPERLOG("Can draw! index = %d.", m_iBufferIndex);
                return true;
            }
        }

        return false;
    }

    status_t V4L2OverlayRef::getConsumedImages(uint32_t freeList[], uint32_t& nNumber)
    {
        uint32_t i = 0;
        void* vAddr = NULL;
        while(NO_ERROR == getConsumedImage(vAddr)){
            V4L2WRAPPERLOG("freeList[%d] = %p", i, vAddr);
            freeList[i] = (uint32_t)vAddr;
            i+=3; // v4l2 has only y addr, so, skip uv addr.
            vAddr = NULL;
        }

        nNumber = i/3;
        return NO_ERROR;
    }

    status_t V4L2OverlayRef::getConsumedImage(void*& vAddr)
    {
        V4L2WRAPPERLOG("-------------------- enter %s--------------------", __FUNCTION__);
        if(!m_vDeferFreeAddr.isEmpty()){
            // if stream off, all occpied buffer should be drained out.
            vAddr = m_vDeferFreeAddr.top();
            m_vDeferFreeAddr.pop();
            V4L2WRAPPERLOG("Ln:%d. Get Deferred vAddr = %p.", __LINE__, vAddr);
            return (vAddr == NULL) ? (-EINVAL) : ((uint32_t)(NO_ERROR));
        }

        v4l2_buffer buf;
        if (m_iFrameCount > 1){
            V4L2WRAPPERLOG("-------------------- %s free %d--------------------", __FUNCTION__, __LINE__);
            v4l2_buf_type nType = (m_wrapperSetting.m_bMultiPlanes ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
            buf.type = nType;
            buf.memory = V4L2_MEMORY_USERPTR;
            if(m_wrapperSetting.m_bMultiPlanes){
                buf.m.planes = m_plane;
                buf.length	= 2;
            }

            if (v4l2_overlay_ioctl(m_fd, VIDIOC_DQBUF, &buf, "dqbuf")){
                V4L2WRAPPERLOG("No more buffer to be dequeue.\n");
                return -EINVAL;
            }

            vAddr = m_vBufferAddr[buf.index];
            m_vBufferAddr.editItemAt(buf.index) = NULL;
            V4L2WRAPPERLOG("Ln:%d. Get vAddr = %p. buf.index = %d.", __LINE__, vAddr, buf.index);
            return NO_ERROR;
        }

        return -EINVAL;
    }

    status_t V4L2OverlayRef::setSrcPitchAndResolustion(VideoSourceInfo& videoInfo)
    {
        V4L2WRAPPERLOG("%s, srcWidth(%d), srcHeight(%d), srcFormat(%d), srcYPitch(%d), srcUPitch(%d), srcVPitch(%d).", __FUNCTION__, videoInfo.m_nWidth, videoInfo.m_nHeight, videoInfo.m_nFormat, videoInfo.m_nPitchY, videoInfo.m_nPitchU, videoInfo.m_nPitchV);

        if(videoInfo != m_wrapperSetting)
        {
            if (NO_ERROR != setStreamOn(false)){
                LOGE("Can not stream off the ovly for set new param.");
                return -EIO;
            }

            m_wrapperSetting = videoInfo;
            uint32_t nCropWidth = videoInfo.m_nCropR - videoInfo.m_nCropL;
            uint32_t nCropHeight = videoInfo.m_nCropB - videoInfo.m_nCropT;

            if (v4l2_overlay_check_caps(m_fd)){
                LOGE("check caps failed.");
                return -EIO;
            }

            if (v4l2_overlay_set_format(m_fd, videoInfo.m_nWidth, videoInfo.m_nHeight, videoInfo.m_nFormat, videoInfo.m_bMultiPlanes)){
                LOGE("Set view port offset fail\n");
                return -EIO;
            }

            if(NO_ERROR != setCapability(MAX_FRAME_BUFFERS)){
                LOGE("Can not alloc buffer properly.");
                return -EIO;
            }

            if(videoInfo.m_bMultiPlanes){
                nCropWidth >>= m_nMode3d ? 0 : 1;
                nCropHeight >>= m_nMode3d ? 1 : 0;
            }

            if (v4l2_overlay_set_crop(m_fd, videoInfo.m_nCropL, videoInfo.m_nCropT, nCropWidth, nCropHeight)){
                LOGE("Set view port offset fail\n");
                return -EIO;
            }
        }

        return NO_ERROR;
    }


status_t V4L2OverlayRef::setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
{
    V4L2WRAPPERLOG("%s l(%d) t(%d) r(%d) b(%d).", __func__, l, t, r, b);
    if (!isEnabled()) {
        return -EIO;
    }

    m_userSetting.m_nCropL = l;
    m_userSetting.m_nCropR = r;
    m_userSetting.m_nCropT = t;
    m_userSetting.m_nCropB = b;

    m_iFlag |= FLAG_SRC_CROP;

    if((m_iFlag & FLAG_SRC_RESOLUTION) && (m_iFlag & FLAG_SET_PITCH)){
        return setSrcPitchAndResolustion(m_userSetting);
    }

    return NO_ERROR;
}

status_t V4L2OverlayRef::set3dVideoOn(bool bOn, uint32_t mode)
{
    m_userSetting.setMultiPlaneFlag(bOn);
    m_nMode3d = mode;
    nBufType = (bOn ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
    return NO_ERROR;
}

} // end of namespace android
