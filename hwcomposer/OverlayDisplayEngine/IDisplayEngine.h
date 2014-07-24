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

#ifndef __INTERFACE_DISPLAYENGINE_H__
#define __INTERFACE_DISPLAYENGINE_H__

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
#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>
#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>
#include "DisplaySurface.h"

struct fb_var_screeninfo;
struct fb_fix_screeninfo;

#define MAX_BUFFER_NUM 32

namespace android
{

enum DISPLAYENGINETYPE{
    DISP_ENGINE_BASELAYER = 0,
    DISP_ENGINE_OVERLAY   = 1,
};

class DisplayEngineLayout;

class DisplayEngineMode
{
public:
    DisplayEngineMode()
        : m_nBpp(0)
        , m_nActivate(0)
        , m_nPixClock(0)
        , m_nLeftMargin(0)
        , m_nRightMargin(0)
        , m_nUpperMargin(0)
        , m_nLowerMargin(0)
        , m_nHsyncLen(0)
        , m_nVsyncLen(0)
        , m_nSync(0)
        , m_nVMode(0)
    {}

    ~DisplayEngineMode(){}

    status_t from(const fb_var_screeninfo& varInfo);

    ///< friends
    friend class DisplayModel;
    friend void ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& m_displayLayout, const DisplayEngineMode& caps);

private:
    uint32_t m_nBpp;
    uint32_t m_nActivate;
    uint32_t m_nPixClock;
    uint32_t m_nLeftMargin;
    uint32_t m_nRightMargin;
    uint32_t m_nUpperMargin;
    uint32_t m_nLowerMargin;
    uint32_t m_nHsyncLen;
    uint32_t m_nVsyncLen;
    uint32_t m_nSync;
    uint32_t m_nVMode;
};

class DisplayEngineCaps
{
    //TODO
};

class DisplayEngineLayout
{
public:
    DisplayEngineLayout()
        : m_nBase(0)
        , m_nSize(0)
        , m_nWidth(0)
        , m_nHeight(0)
        , m_nVirtualWidth(0)
        , m_nVirtualHeight(0)
        , m_nOffsetX(0)
        , m_nOffsetY(0)
        , m_nActivate(0)
    {}

    ~DisplayEngineLayout(){}

    status_t from(const fb_var_screeninfo& varInfo, const fb_fix_screeninfo& fixInfo);

    ///< friends
    friend class DisplayModel;
    friend void ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& m_displayLayout, const DisplayEngineMode& caps);

private:
    uint32_t m_nBase;
    uint32_t m_nSize;
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nVirtualWidth;
    uint32_t m_nVirtualHeight;
    uint32_t m_nOffsetX;
    uint32_t m_nOffsetY;
    uint32_t m_nActivate;
};

class IDisplayEngine : public RefBase
{
public:
    IDisplayEngine(){};
    virtual ~IDisplayEngine(){};

public:
    ///< operation start.
    virtual status_t open() = 0;
    virtual status_t close() = 0;

    virtual status_t getCaps(DisplayEngineCaps& caps) = 0;

    ///< set/get mode capabilities.
    virtual status_t getMode(DisplayEngineMode& mode) = 0;
    virtual status_t getLayout(DisplayEngineLayout& layout) = 0;

    virtual status_t setMode(const DisplayEngineMode& mode) = 0;
    virtual status_t setLayout(const DisplayEngineLayout& layout) = 0;

    ///< draw related.
    virtual status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch) = 0;
    virtual status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat) = 0;
    virtual status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b) = 0;
    virtual status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b) = 0;
    virtual status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset) = 0;
    virtual status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color) = 0;
    virtual status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t lenght, uint32_t addrType) = 0;
    virtual status_t getConsumedImages(uint32_t vAddr[], uint32_t& nNumber) = 0;
    virtual status_t configVdmaStatus(DISPLAY_VDMA& vdma) = 0;
    virtual status_t waitVSync(DISPLAY_SYNC_PATH path) = 0;

    ///< dma related.
    virtual status_t setStreamOn(bool bOn) = 0;

    ///< special feature related.
    virtual status_t set3dVideoOn(bool bOn, uint32_t mode) = 0;

    virtual int32_t getFd() const = 0;
    virtual int32_t getReleaseFd() const = 0;
    virtual const char* getName() const = 0;
};

// factory-liked creator.
IDisplayEngine* CreateDisplayEngine(const char* pszName, DISPLAYENGINETYPE type);
IDisplayEngine* CreateBaseLayerEngine(const char* pszName, DISPLAYENGINETYPE type);
//IDisplayEngine* CreateOverlayEngine(uint32_t idx, DMSInternalConfig const * dms_config);

// common functions;
uint32_t ResolveColorFormat(uint32_t dmsFormat);
void  ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& displayLayout, const DisplayEngineMode& displayMode);

}

#endif
