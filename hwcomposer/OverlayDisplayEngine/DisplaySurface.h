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

#ifndef __DMS_DISPLAY_SURFACE_H__
#define __DMS_DISPLAY_SURFACE_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <binder/IMemory.h>

#ifdef __cplusplus
extern "C" {
namespace android{
#endif

/**
 *  struct DISPLAYSURFACE comes exactly from
 *
 *  typedef int FBVideoMode;
 *  struct _sOvlySurface {
 *	    FBVideoMode videoMode;
 *	    struct _sViewPortInfo viewPortInfo;
 *	    struct _sViewPortOffset viewPortOffset;
 *	    struct _sVideoBufferAddr videoBufferAddr;
 *  };
 *  in fb_ioctl.h.
 *  For the platform compatibility concern, we define our own structure, and
 *  trans to the real _sOvlySurface by IDisplayEngine.
 *
 */

#define DISP_WFD_GUI_MODE       1
#define DISP_WFD_VIDEO_MODE     2

#define MAX_DISPLAY_QUEUE_NUM   30

typedef int DisplayVideoMode; 

typedef struct DisplayColorKeyNAlpha {
	unsigned int mode;
	unsigned int alphapath;
	unsigned int config;
	unsigned int Y_ColorAlpha;
	unsigned int U_ColorAlpha;
	unsigned int V_ColorAlpha;
}DISPLAYCOLORKEYNALPHA, *PDISPLAYCOLORKEYNALPHA;

typedef struct DisplayViewPortInfo {
    unsigned short srcWidth;        /* video source size */
    unsigned short srcHeight;
    unsigned short zoomXSize;       /* size after zooming */
    unsigned short zoomYSize;
    unsigned short yPitch;
    unsigned short uPitch;
    unsigned short vPitch;
    unsigned int rotation;
    unsigned int yuv_format;
}DISPLAYVIEWPORTINFO, *PDISPLAYVIEWPORTINFO;

typedef struct DisplayViewPortOffset {
    unsigned short xOffset;         /* position on screen */
    unsigned short yOffset;
}DISPLAYVIEWPORTOFFSET, *PDISPLAYVIEWPORTOFFSET;

typedef struct DisplayVideoBufferAddr {
    unsigned char   frameID;        /* which frame wants */
    /* new buffer (PA). 3 addr for YUV planar */
    unsigned char *startAddr[3];
    unsigned char *inputData;       /* input buf address (VA) */
    unsigned int length;            /* input data's length */
}DISPLAYVIDEOBUFFERADDR, *PDISPLAYVIDEOBUFFERADDR;

typedef struct DisplaySurface{
    uint32_t videoMode;
    DisplayViewPortInfo viewPortInfo;
    DisplayViewPortOffset viewPortOffset;
    DisplayVideoBufferAddr videoBufferAddr;
}DISPLAYSURFACE, *PDISPLAYSURFACE;

typedef struct DisplayVdma {
       /* path id, 0->panel, 1->TV, 2->panel2 */
       uint32_t path;
       /* 0: grafhics, 1: video */
       uint32_t layer;
       uint32_t enable;
}DISPLAY_VDMA, *PDISPLAY_VDMA;

typedef enum Display_Sync_Path{
    DISPLAY_SYNC_SELF = 0x0,
    DISPLAY_SYNC_PANEL = 0x1,
    DISPLAY_SYNC_TV = 0x2,
    DISPLAY_SYNC_PANEL_TV = 0x3,
}DISPLAY_SYNC_PATH;


#ifdef __cplusplus
} // end of namespace
} // end of extern "C"
#endif

#endif
