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
#include <linux/fb.h>

//#include "dms_private.h"
#include "IDisplayEngine.h"
#include "FramebufferEngine.h"

namespace android{

status_t DisplayEngineLayout::from(const struct fb_var_screeninfo& varInfo, const struct fb_fix_screeninfo& fixInfo)
{
    //TODO
    m_nBase = fixInfo.smem_start;
    m_nSize = fixInfo.smem_len;

    m_nWidth = varInfo.xres;
    m_nHeight = varInfo.yres;
    m_nVirtualWidth = varInfo.xres_virtual;
    m_nVirtualHeight = varInfo.yres_virtual;
    m_nOffsetX = varInfo.xoffset;
    m_nOffsetY = varInfo.yoffset;

    return NO_ERROR;
}

status_t DisplayEngineMode::from(const struct fb_var_screeninfo& varInfo)
{
    m_nActivate = varInfo.activate;
    m_nPixClock = varInfo.pixclock;
    m_nLeftMargin = varInfo.left_margin;
    m_nRightMargin = varInfo.right_margin;
    m_nUpperMargin = varInfo.upper_margin;
    m_nLowerMargin = varInfo.lower_margin;
    m_nHsyncLen = varInfo.hsync_len;
    m_nVsyncLen = varInfo.vsync_len;
    m_nSync = varInfo.sync;
    m_nVMode = varInfo.vmode;
    m_nBpp = varInfo.bits_per_pixel;

    return NO_ERROR;
}

uint32_t ResolveColorFormat(uint32_t dmsFormat)
{
    switch (dmsFormat){
        case HAL_PIXEL_FORMAT_RGB_565: //DISP_FOURCC_RGB565:
            return FB_VMODE_RGB565;
        case HAL_PIXEL_FORMAT_RGB_888: //DISP_FOURCC_RGB888:
            return FB_VMODE_RGB888PACK;
        case HAL_PIXEL_FORMAT_RGBA_8888: //DISP_FOURCC_ABGR8888:
            /*TRICKY
             * Accroding to lcd driver, this BGRA888 format is managed as
             * Bytes Sequence: 0   1   2   3
             * Color:         RR  GG  BB  AA
             * So, this format should mapping to the RGBA8888.
             */
            return FB_VMODE_BGRA888;
        case HAL_PIXEL_FORMAT_BGRA_8888: //DISP_FOURCC_ARGB8888:
            /*TRICKY
             * Accroding to lcd driver, this BGRA888 format is managed as
             * Bytes Sequence:  0   1   2   3
             * Color:          BB  GG  RR  AA
             * So, this format should mapping to the BGRA8888.
             */
            return FB_VMODE_RGBA888;
        case HAL_PIXEL_FORMAT_YV12: //DISP_FOURCC_YV12:
            return FB_VMODE_YUV420PLANAR_SWAPUV;
        case HAL_PIXEL_FORMAT_YCbCr_420_P: //DISP_FOURCC_I420:
            return FB_VMODE_YUV420PLANAR;
        case HAL_PIXEL_FORMAT_CbYCrY_422_I: //DISP_FOURCC_UYVY:
            return FB_VMODE_YUV422PACKED;
        case HAL_PIXEL_FORMAT_YCbCr_422_I: //DISP_FOURCC_YUY2:
            return FB_VMODE_YUV422PACKED_SWAPYUorV;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP: //DISP_FOURCC_NV21:
        default:
            LOGE("UNKNOWN FORMAT %d !!!!", dmsFormat);
            return  FB_VMODE_RGB565;
    }
}

void  ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& displayLayout, const DisplayEngineMode& displayMode)
{
    //TODO, need a judgement of the valid value, or you may update a wrong value.
    varInfo.xres = displayLayout.m_nWidth;
    varInfo.yres = displayLayout.m_nHeight;
    varInfo.xres_virtual = displayLayout.m_nVirtualWidth;
    varInfo.yres_virtual = displayLayout.m_nVirtualHeight;
    varInfo.xoffset = displayLayout.m_nOffsetX;
    varInfo.yoffset = displayLayout.m_nOffsetY;
    varInfo.bits_per_pixel = displayMode.m_nBpp;
    varInfo.activate = displayMode.m_nActivate;
    varInfo.pixclock = displayMode.m_nPixClock;
    varInfo.left_margin = displayMode.m_nLeftMargin;
    varInfo.right_margin = displayMode.m_nRightMargin;
    varInfo.upper_margin = displayMode.m_nUpperMargin;
    varInfo.lower_margin = displayMode.m_nLowerMargin;
    varInfo.hsync_len = displayMode.m_nHsyncLen;
    varInfo.vsync_len = displayMode.m_nVsyncLen;

    switch (displayMode.m_nBpp) {
    case 16:
        varInfo.red.offset      = 11;
        varInfo.red.length      = 5;
        varInfo.green.offset    = 5;
        varInfo.green.length    = 6;
        varInfo.blue.offset     = 0;
        varInfo.blue.length     = 5;
        varInfo.transp.offset   = 0;
        varInfo.transp.length   = 0;
        break;
    case 24:
        varInfo.red.offset      = 16;
        varInfo.red.length      = 8;
        varInfo.green.offset    = 8;
        varInfo.green.length    = 8;
        varInfo.blue.offset     = 0;
        varInfo.blue.length     = 8;
        varInfo.transp.offset   = 0;
        varInfo.transp.length   = 0;
        break;
    case 32:
        varInfo.red.offset      = 16;
        varInfo.red.length      = 8;
        varInfo.green.offset    = 8;
        varInfo.green.length    = 8;
        varInfo.blue.offset     = 0;
        varInfo.blue.length     = 8;
        varInfo.transp.offset   = 24;
        varInfo.transp.length   = 8;
        break;
    default:
        LOGE("Unsupported BPP mode in DisplayEngineCaps.");
        break;
    }
}

IDisplayEngine* CreateBaseLayerDisplayEngine(const char* pszName, DISPLAYENGINETYPE type)
{
    FBBaseLayer* pFBBaseLayer = new FBBaseLayer(pszName);
    return pFBBaseLayer;
}

IDisplayEngine* CreateDisplayEngine(const char* pszName, DISPLAYENGINETYPE type)
{
    switch (type){
        case DISP_ENGINE_BASELAYER:
        {
            return CreateBaseLayerDisplayEngine(pszName, type);
        }
        case DISP_ENGINE_OVERLAY:
        {
            //TODO
            return NULL;
        }
        default:
            return NULL;
    }

    return NULL;
}



}
