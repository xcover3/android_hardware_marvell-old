/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/
#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CameraEngine.h"

// mmp2
#if defined( PLATFORM_BOARD_BROWNSTONE )
	#include <plat/fb_ioctl.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/fb2"
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/graphics/fb2"
	#define SCREEN_WIDTH            1280
	#define SCREEN_HEIGHT           720
	#define DISPLAY_FB
#elif defined( PLATFORM_BOARD_G50 )
	#include <plat/fb_ioctl.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/fb2"
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/graphics/fb2"
	#define SCREEN_WIDTH            1024
	#define SCREEN_HEIGHT           600
	#define DISPLAY_FB
// mmp3
#elif defined( PLATFORM_BOARD_ABILENE )
	#include <linux/videodev2.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/video1" // tablet LCD
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/video2" // HDMI
	#define SCREEN_WIDTH            1280
	#define SCREEN_HEIGHT           720
	#define DISPLAY_V4L2
#elif defined( PLATFORM_BOARD_ORCHID )
	#include <linux/videodev2.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/video1" // tablet LCD
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/video2" // HDMI
	#define SCREEN_WIDTH            1280
	#define SCREEN_HEIGHT           720
	#define DISPLAY_V4L2
#elif defined( PLATFORM_BOARD_MK2 )
        #include <linux/videodev2.h>
        #define DEFAULT_DISPLAY_DEVICE  "/dev/video1" // tablet LCD
        #define DEFAULT_DISPLAY_DEVICE2 "/dev/video2" // HDMI
        #define SCREEN_WIDTH            1280
        #define SCREEN_HEIGHT           720
        #define DISPLAY_V4L2
#elif defined( PLATFORM_BOARD_YELLOWSTONE )
	#include <linux/videodev2.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/video1" // tablet LCD
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/video2" // HDMI
	#define SCREEN_WIDTH            1280
	#define SCREEN_HEIGHT           720
	#define DISPLAY_V4L2
// pxa920/pxa910
#elif defined( PLATFORM_BOARD_TTCDKB ) || defined( PLATFORM_BOARD_TDDKB )
	#include <plat/fb_ioctl.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/fb1"
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/graphics/fb1"
	#define SCREEN_WIDTH            320
	#define SCREEN_HEIGHT           480
	#define DISPLAY_FB
// pxa955/pxa978
#elif defined( PLATFORM_BOARD_MG1EVB ) || defined( PLATFORM_BOARD_NEVOEVB )
	#include <plat/fb_ioctl.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/fb1"
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/graphics/fb1"
	#define SCREEN_WIDTH            480
	#define SCREEN_HEIGHT           640
	#define DISPLAY_FB
#elif defined( PLATFORM_BOARD_MG1SAARB ) || defined( PLATFORM_BOARD_NEVO )
	#include <plat/fb_ioctl.h>
	#define DEFAULT_DISPLAY_DEVICE  "/dev/fb1"
	#define DEFAULT_DISPLAY_DEVICE2 "/dev/graphics/fb1"
	#define SCREEN_WIDTH            480
	#define SCREEN_HEIGHT           640
	#define DISPLAY_FB
#else
	#error no platform is defined!
#endif


typedef struct
{
	int             fd_lcd;
	int             on;
	int             bpp;
	int             format;
	int             step;
	int             screen_width;
	int             screen_height;
	int             frame_width;
	int             frame_height;
	int             screen_pos_x;
	int             screen_pos_y;
	int             yoff;
	int             ylen;
	int             cboff;
	int             cblen;
	int             croff;
	int             crlen;
	int             fix_smem_len;
	unsigned char   *map;
	unsigned char   *overlay[3];
} DisplayCfg;

int displaydemo_open( int iFrameWidth, int iFrameHeight, CAM_ImageFormat eFrameFormat, DisplayCfg *pDispCfg );
int displaydemo_close( DisplayCfg *pDispCfg );
int displaydemo_display( CAM_DeviceHandle hHandle, DisplayCfg *pDispCfg, CAM_ImageBuffer *pImgBuf );

#ifdef __cplusplus
}
#endif

#endif // _DISPLAY_H_
