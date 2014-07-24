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

#ifndef __FAKE_OVERLAY_H__
#define __FAKE_OVERLAY_H__

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
#include "IDisplayEngine.h"

namespace android{

class FakeOverlayRef : public IDisplayEngine
{
public:
    FakeOverlayRef(const char* pDev) : m_fd(1)
        , m_strDevName(pDev)
    {
        char buf[16];
        sprintf(buf, "FakeOverlay-%d", m_nCount++);
        m_strDevName = buf;
    }

    ~FakeOverlayRef()
    {
        close();
        m_nCount--;
    }

public:
    status_t open()
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t getCaps(DisplayEngineCaps& caps)
    {
        return NO_ERROR;
    }

    status_t getMode(DisplayEngineMode& mode)
    {
        return NO_ERROR;
    }

    status_t getLayout(DisplayEngineLayout& layout)
    {
        return NO_ERROR;
    }

    status_t setMode(const DisplayEngineMode& mode)
    {
        return NO_ERROR;
    }

    status_t setLayout(const DisplayEngineLayout& layout)
    {
        return NO_ERROR;
    }

    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t length, uint32_t addrType)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t configVdmaStatus(DISPLAY_VDMA& vdma)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t waitVSync(DISPLAY_SYNC_PATH path)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setStreamOn(bool bOn)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t set3dVideoOn(bool bOn, uint32_t mode)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nImgNum)
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t close()
    {
        LOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    int32_t getFd() const {return m_fd;}

    int32_t getReleaseFd() const {return m_fd;}

    const char* getName() const{return m_strDevName.string();}

private:
    ///< fake fd.
    int32_t m_fd;

    ///< dev name.
    String8 m_strDevName;

    ///< ref count for name difference.
    static uint32_t m_nCount;
};

uint32_t FakeOverlayRef::m_nCount = 0;

}
#endif

