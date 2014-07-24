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
#include <linux/videodev2.h>
#include <cutils/ashmem.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <fb_ioctl.h>


using namespace android;

#define PROP_VIDEO_PATH "service.video.path"
#define PROP_OVERLAY_ROTATE "service.overlay.rotate"
#define BASELAY_DEVICE "/dev/graphics/fb1"

#define ALIGN16(m)  (((m + 15)/16) * 16)
#define ALIGN4(m)  (((m + 3)/4) * 4)

/* pmem dev node */
#define MAX_FRAME_BUFFERS   7

v4l2_buf_type nBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT; // = (multi_planes ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);

static int v4l2_overlay_ioctl(int fd, int req, void *arg, const char* msg)
{
    int ret = ioctl(fd, req, arg);
    if(ret < 0 && req != (int)VIDIOC_DQBUF)
        LOGE("V4L2 ioctl %s failed, return value(%d).", msg, ret);

    return ret;
}

static int configure_pixfmt(struct v4l2_format *pixelFormat, int32_t fmt,
                            uint32_t w, uint32_t h, v4l2_buf_type type, bool bMultiPlane)
{
    uint32_t nColorFmt = V4L2_PIX_FMT_RGB565;
    switch (fmt) {
        case HAL_PIXEL_FORMAT_RGB_565: //DISP_FOURCC_RGB565://FB_VMODE_RGB565:
            nColorFmt = V4L2_PIX_FMT_RGB565X;
            break;
        case HAL_PIXEL_FORMAT_CbYCrY_422_I: //DISP_FOURCC_UYVY://FB_VMODE_YUV422PACKED:
            nColorFmt = V4L2_PIX_FMT_UYVY;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_P: //DISP_FOURCC_I420://FB_VMODE_YUV420PLANAR:
            nColorFmt = V4L2_PIX_FMT_YUV420;
            break;
        case HAL_PIXEL_FORMAT_YV12: //DISP_FOURCC_YV12://FB_VMODE_YUV420PLANAR_SWAPUV:
            nColorFmt = V4L2_PIX_FMT_YVU420;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_I: //DISP_FOURCC_YUY2://FB_VMODE_YUV420PLANAR_SWAPUV:
            nColorFmt = V4L2_PIX_FMT_YUYV;
            break;
        case HAL_PIXEL_FORMAT_RGBA_8888: //DISP_FOURCC_ABGR8888:
            nColorFmt = V4L2_PIX_FMT_RGB32;
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888: //DISP_FOURCC_ARGB8888:
            nColorFmt = V4L2_PIX_FMT_BGR32;
            break;
        default:
            LOGE("Pixformat will be error");
            return -1;
    }

	pixelFormat->type = type;
	if (bMultiPlane) {
        pixelFormat->fmt.pix_mp.width       = w;
        pixelFormat->fmt.pix_mp.height      = h;
        pixelFormat->fmt.pix_mp.pixelformat = nColorFmt;
        pixelFormat->fmt.pix_mp.num_planes  = 2;
	} else {
        pixelFormat->fmt.pix.width       = w;
        pixelFormat->fmt.pix.height      = h;
        pixelFormat->fmt.pix.pixelformat = nColorFmt;
		//pixelFormat->fmt.pix.field       = V4L2_FIELD_INTERLACED;
	}

    return 0;
}

static int v4l2_overlay_set_colorkey(int fd, int enable, int colorkey)
{
    struct v4l2_framebuffer fbuf;
    struct v4l2_format fmt;

    //LOGD("%s:: %s the color key", __FUNCTION__, enable ? "enable" : "disable");

    memset(&fbuf, 0, sizeof(fbuf));
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FBUF, &fbuf, "get transparency enables")){
        return -1;
    }

    if (enable)
        fbuf.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
    else
        fbuf.flags &= ~V4L2_FBUF_FLAG_CHROMAKEY;

    if(v4l2_overlay_ioctl(fd, VIDIOC_S_FBUF, &fbuf, "enable chrome key")){
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &fmt, "setcolor key")){
        return -1;
    }

    fmt.fmt.win.chromakey = enable ? (colorkey & 0xFFFFFFFF) : 0;
    if(v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &fmt, "set color key")){
        return -1;
    }

    return 0;
}

static int v4l2_overlay_set_global_alpha(int fd, int enable, int alpha)
{
    struct v4l2_framebuffer fbuf;
    struct v4l2_format fmt;

    //LOGD("%s:: %s the global alpha", __FUNCTION__, enable ? "enable" : "disable");

    memset(&fbuf, 0, sizeof(fbuf));
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FBUF, &fbuf, "get transparency enables")){
        return -1;
    }

    if (enable)
        fbuf.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
    else
        fbuf.flags &= ~V4L2_FBUF_FLAG_GLOBAL_ALPHA;

    if(v4l2_overlay_ioctl(fd, VIDIOC_S_FBUF, &fbuf, "enable global alpha")){
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &fmt, "get global alpha")){
        return -1;
    }

    fmt.fmt.win.global_alpha = enable ? (alpha & 0xFF) : 0;
    if(v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &fmt, "set global alpha")){
        return -1;
    }

    return 0;
}

static int v4l2_overlay_set_format(int fd, uint32_t w, uint32_t h, uint32_t fmt, bool bMultiPlane = false)
{
    struct v4l2_format format;
    int ret;

    memset(&format, 0, sizeof(format));
    format.type = nBufType;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get format");
    if (ret)
       return ret;
    V4L2LOG("%s:: format.fmt.pix.width=%d format.fmt.pix.height=%d format.fmt.pix.pixelformat=%x\n", __FUNCTION__, format.fmt.pix.width, format.fmt.pix.height,format.fmt.pix.pixelformat);

    configure_pixfmt(&format, fmt, w, h, nBufType, bMultiPlane);
    ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format, "set output format");
    if (ret)
       return ret;

    format.type = nBufType;
    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get output format");
    if (ret)
       return ret;
    V4L2LOG("%s:: format.fmt.pix.width=%d format.fmt.pix.height=%d format.fmt.pix.pixelformat=%x\n", __FUNCTION__, format.fmt.pix.width, format.fmt.pix.height,format.fmt.pix.pixelformat);

   return 0;
}

static int v4l2_overlay_get_position(int fd, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    struct v4l2_format format;

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get overlay position format")){
        return -1;
    }
    *x = format.fmt.win.w.left;
    *y = format.fmt.win.w.top;
    *w = format.fmt.win.w.width;
    *h = format.fmt.win.w.height;

    return 0;
}

static int v4l2_overlay_set_position(int fd, int32_t x, int32_t y, int32_t w, int32_t h)
{
    struct v4l2_format format;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get format")){
        LOGE("Get overlay window failed.");
        return -1;
    }

    V4L2LOG("get Window position: left = %d, top = %d, width = %d, height = %d", format.fmt.win.w.left,
                   format.fmt.win.w.top,
                   format.fmt.win.w.width,
                   format.fmt.win.w.height);

    format.fmt.win.w.left = x;
    format.fmt.win.w.top  = y;
    format.fmt.win.w.width = w;
    format.fmt.win.w.height = h;
    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format, "set overlay window")){
        LOGE("Set overlay window failed.");
        return -1;
    }

    V4L2LOG("set Window position: left = %d, top = %d, width = %d, height = %d", x, y, w, h);
    return 0;
}
static int v4l2_overlay_check_caps(int fd)
{
    struct v4l2_capability cap;
    if (v4l2_overlay_ioctl(fd, VIDIOC_QUERYCAP, &cap, "query capabilities")){
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)){
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)){
        LOGE("overlay driver does not support V4L2_CAP_VIDEO_OUTPUT");
        return -1;
    }

    return 0;
}

static int v4l2_overlay_set_crop(int fd, int32_t x, int32_t y, int32_t w, int32_t h)
{
    struct v4l2_crop crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_G_CROP, &crop, "get overlay crop")){
        LOGE("Get overlay window failed.");
        return -1;
    }

    V4L2LOG("get crop position: left = %d, top = %d, width = %d, height = %d",
                   crop.c.left,
                   crop.c.top,
                   crop.c.width,
                   crop.c.height);

    crop.c.left = x;
    crop.c.top  = y;
    crop.c.width = w;
    crop.c.height = h;
    crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    if(v4l2_overlay_ioctl(fd, VIDIOC_S_CROP, &crop, "set overlay crop")){
        LOGE("Set overlay window failed.");
        return -1;
    }

    V4L2LOG("set crop position: left = %d, top = %d, width = %d, height = %d", x, y, w, h);
    return 0;
}
