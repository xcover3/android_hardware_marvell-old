/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>

#include <getopt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>
#include <unistd.h>
#include <sys/time.h>

#include "test_harness.h"
#include "display.h"


#define MAX_LCD_BUF_NUM MAX_QUEUE_NUM

int                            iOvlyBufCnt = 0;
extern CAM_ImageBuffer         *pPreviewBuf;
extern int                     *pPreviewBufStatus;
extern int                     iPrevBufNum;


#if defined( DISABLE_DISPLAY )

int displaydemo_open( int iFrameWidth, int iFrameHeight, CAM_ImageFormat eFrameFormat, DisplayCfg *pDispCfg )
{
	return 0;
}

int displaydemo_close( DisplayCfg *pDispCfg )
{
	return 0;
}

int displaydemo_display( CAM_DeviceHandle hHandle, DisplayCfg *pDispCfg, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error error;

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)pImgBuf );
	ASSERT_CAM_ERROR( error );

	return 0;
}

#else

#if defined( DISPLAY_V4L2 )

CAM_Thread        stRetrieveBufferThread;
CAM_DeviceHandle  hCamDeviceHandle = NULL;
static CAM_Int32s _retrieve_buffer_thread( void *pParam );

int overlay_open( char *dev, DisplayCfg *pDispCfg )
{
	struct v4l2_capability     cap;
	struct v4l2_format         format;
	struct v4l2_framebuffer    fbuf;

	int    ret;

	if ( !pDispCfg || !dev )
	{
		return -1;
	}

	memset( &cap,    0, sizeof(struct v4l2_capability) );
	memset( &format, 0, sizeof(struct v4l2_format) );
	memset( &fbuf,   0, sizeof(struct v4l2_framebuffer));

	// open device
	pDispCfg->fd_lcd = open( dev, O_RDWR );
	if ( pDispCfg->fd_lcd < 0 )
	{
		TRACE( "open device %s failed( %s, %d )\n", dev, __FILE__, __LINE__ );
		return -2;
	}

	// query driver capability
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_QUERYCAP, &cap );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_QUERYCAP failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	if ( !( cap.capabilities & V4L2_CAP_STREAMING ) )
	{
		TRACE( "Error: overlay driver does not support V4L2_CAP_STREAMING\n" );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	if ( !( cap.capabilities & V4L2_CAP_VIDEO_OUTPUT ) )
	{
		TRACE( "Error: overlay driver does not support V4L2_CAP_VIDEO_OUTPUT\n" );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// set format
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	switch ( pDispCfg->format )
	{
	case CAM_IMGFMT_CbYCrY:
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		pDispCfg->bpp              = 16;
		pDispCfg->step             = pDispCfg->frame_width * 2;
		break;

	case CAM_IMGFMT_YCbCr420P:
	case CAM_IMGFMT_YCbCr420SP:
	case CAM_IMGFMT_YCrCb420SP: // hack for display velidation, overlay don't support sp display
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		pDispCfg->bpp              = 12;
		pDispCfg->step             = pDispCfg->frame_width;
		break;

	case CAM_IMGFMT_YCrCb420P:
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
		pDispCfg->bpp              = 12;
		pDispCfg->step             = pDispCfg->frame_width;
		break;

	case CAM_IMGFMT_BGR565:
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
		pDispCfg->bpp              = 16;
		pDispCfg->step             = pDispCfg->frame_width * 2;
		break;

	default:
		TRACE( "Error: not supported format( %s, %d )\n", __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// V4L2_BUF_TYPE_VIDEO_OUTPUT: set the video format and source size related
	format.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	format.fmt.pix.width       = pDispCfg->frame_width;
	format.fmt.pix.height      = pDispCfg->frame_height;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	memset( &format, 0, sizeof( format ) );
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// V4L2_BUF_TYPE_VIDEO_OVERLAY: the dst win related and color key, alpha etc.
	format.type                = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	format.fmt.win.w.width     = pDispCfg->screen_width;
	format.fmt.win.w.height    = pDispCfg->screen_height;
	format.fmt.win.w.left      = pDispCfg->screen_pos_x;
	format.fmt.win.w.top       = pDispCfg->screen_pos_y;
	// TRACE( "Info: fmt %d, width:%d, height:%d, w.width:%d, w.height:%d, w.left:%d, w.top:%d\n", pDispCfg->format, pDispCfg->frame_width, pDispCfg->frame_height, pDispCfg->screen_width,  pDispCfg->screen_height, pDispCfg->screen_pos_x, pDispCfg->screen_pos_y );
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FMT failed, return value[%d, %s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// enable global alpha
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FBUF, &fbuf );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FBUF failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	fbuf.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FBUF, &fbuf );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FBUF failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	memset( &format, 0, sizeof( format ) );
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__  );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	format.fmt.win.global_alpha = 0xFF;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// set colorkey
	memset( &fbuf, 0, sizeof(fbuf) );
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FBUF, &fbuf );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FBUF failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	fbuf.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FBUF, &fbuf );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FBUF failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	memset( &format, 0, sizeof(format) );
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_G_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_G_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__  );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}
	format.fmt.win.chromakey = 0x0;
	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_S_FMT, &format );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_S_FMT failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
		return -1;
	}

	// stream off display controller
	pDispCfg->on = 0;

	return 0;
}

void overlay_close( DisplayCfg *pDispCfg )
{
	CAM_Int32s ret;

	if ( pDispCfg->fd_lcd >= 0 )
	{
		if ( pDispCfg->on == 1 )
		{
			pDispCfg->on = 0;
			// stream off display controller
			enum v4l2_buf_type type;
			type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			ret = ioctl( pDispCfg->fd_lcd, VIDIOC_STREAMOFF, &type );
			if ( ret != 0 )
			{
				TRACE( "Error: VIDIOC_STREAMOFF failed, return value[%d:%s]( %s, %d )\n", ret, strerror( errno ), __FILE__, __LINE__ );
			}

			// destroy the buffer retrieving thread
			if ( stRetrieveBufferThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventSet( stRetrieveBufferThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}
			ret = CAM_ThreadDestroy( &stRetrieveBufferThread );
			ASSERT( ret == 0 );

			if ( stRetrieveBufferThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventDestroy( &stRetrieveBufferThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}

			if ( stRetrieveBufferThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventDestroy( &stRetrieveBufferThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}
		}

		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;

		hCamDeviceHandle = NULL;
	}
	return;
}

int display_to_overlay( CAM_DeviceHandle hHandle, DisplayCfg *pDispCfg, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error                  error;
	struct v4l2_buffer         buffer;
	struct v4l2_requestbuffers reqbuf;
	int                        i, ret;

	if ( !pDispCfg )
	{
		return -1;
	}

	memset( &buffer, 0, sizeof(struct v4l2_buffer) );
	memset( &reqbuf, 0, sizeof(struct v4l2_requestbuffers) );
	buffer.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buffer.memory    = V4L2_MEMORY_USERPTR;
	buffer.index     = pImgBuf->iUserIndex;
	buffer.m.userptr = ( unsigned long )pImgBuf->pBuffer[0];

	// XXX: V4L2 display driver need input buffer size as page align, we think it's unreasonable and has
	//      notified BSP team, here is a workaround for this issue.
	buffer.length    = MIN( ALIGN_TO( pImgBuf->iFilledLen[0] + pImgBuf->iFilledLen[1] + pImgBuf->iFilledLen[2], 4096 ), pImgBuf->iAllocLen[0] + pImgBuf->iAllocLen[1] + pImgBuf->iAllocLen[2] );

	if ( pDispCfg->on == 0 )
	{
		// require buffers
		reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		reqbuf.count  = iPrevBufNum;
		reqbuf.memory = V4L2_MEMORY_USERPTR;
		ret = ioctl( pDispCfg->fd_lcd, VIDIOC_REQBUFS, &reqbuf );
		if ( ret < 0 )
		{
			TRACE( "Error: VIDIOC_REQBUFS failed, return value[%d:%s]( %s, %d )\n", ret, strerror(errno), __FILE__, __LINE__ );
			return -1;
		}
	}

	ret = ioctl( pDispCfg->fd_lcd, VIDIOC_QBUF, &buffer );
	if ( ret != 0 )
	{
		TRACE( "Error: VIDIOC_QBUF failed, return value[%d:%s]( %s, %d )\n", ret, strerror(errno),  __FILE__, __LINE__ );
		return -1;
	}

	if ( pDispCfg->on == 0 )
	{
		// stream on overlay
		pDispCfg->on = 1;
		enum v4l2_buf_type type;
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		ret = ioctl( pDispCfg->fd_lcd, VIDIOC_STREAMON, &type );
		if ( ret < 0 )
		{
			TRACE( "Error: VIDIOC_STREAMON failed, return value[%d, %s]( %s, %d )\n", ret, strerror(errno), __FILE__, __LINE__ );
			return -1;
		}

		// create a thread to retrieve buffer from overlay driver
		hCamDeviceHandle = hHandle;
		stRetrieveBufferThread.stThreadAttribute.iDetachState = CAM_THREAD_CREATE_DETACHED;
		stRetrieveBufferThread.stThreadAttribute.iPolicy      = CAM_THREAD_POLICY_NORMAL;
		stRetrieveBufferThread.stThreadAttribute.iPriority    = CAM_THREAD_PRIORITY_NORMAL;

		ret = CAM_EventCreate( &stRetrieveBufferThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );

		ret = CAM_EventCreate( &stRetrieveBufferThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );

		ret = CAM_ThreadCreate( &stRetrieveBufferThread, _retrieve_buffer_thread, ( void * )pDispCfg );
		ASSERT( ret == 0 );

		if ( stRetrieveBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( stRetrieveBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
	}

	for ( i = 0; i < iPrevBufNum; i++ )
	{
		if ( pPreviewBuf[i].iPhyAddr[0] == pImgBuf->iPhyAddr[0] )
		{
			pPreviewBufStatus[i] = 1; // 1 is in overlay
			iOvlyBufCnt++;
			break;
		}
	}
	// TRACE( "overlay buf cnt: %d\n", iOvlyBufCnt );

	return 0;
}

static CAM_Int32s _retrieve_buffer_thread( void *pParam )
{
	CAM_Int32s         ret       = 0;
	CAM_Error          error     = CAM_ERROR_NONE;
	CAM_Int32s         i;
	DisplayCfg         *pDispCfg = ( DisplayCfg * )pParam;
	struct v4l2_buffer buffer;

	while ( 1 )
	{
		if ( stRetrieveBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( stRetrieveBufferThread.stThreadAttribute.hEventWakeup, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}

		if ( pDispCfg->on != 0 )
		{
			memset( &buffer, 0, sizeof( buffer ) );
			buffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			buffer.memory = V4L2_MEMORY_USERPTR;

			ret = ioctl( pDispCfg->fd_lcd, VIDIOC_DQBUF, &buffer );
			if ( ret != 0 )
			{
				TRACE( "Error: VIDIOC_DQBUF failed[%s]( %s, %d )\n", strerror(errno), __FILE__, __LINE__ );
			}
			else
			{
				for ( i = 0; i < iPrevBufNum; i++ )
				{
					if ( buffer.m.userptr == ( unsigned long )pPreviewBuf[i].pBuffer[0] && pPreviewBufStatus[i] == 1 )
					{
						error = CAM_SendCommand( hCamDeviceHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pPreviewBuf[i] );
						// TRACE( "****************from overlay: iPrivateIndex: %d, iPhyAddr[0]: %x**************\n" ,pPreviewBuf[j].iPrivateIndex, pPreviewBuf[j].iPhyAddr[0] );
						ASSERT_CAM_ERROR( error );
						iOvlyBufCnt--;
						pPreviewBufStatus[i] = 0;
						break;
					}
				}

				if ( stRetrieveBufferThread.stThreadAttribute.hEventWakeup )
				{
					ret = CAM_EventSet( stRetrieveBufferThread.stThreadAttribute.hEventWakeup );
					ASSERT( ret == 0 );
				}
			}
		}

		if ( stRetrieveBufferThread.bExitThread == CAM_TRUE )
		{
			if ( stRetrieveBufferThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( stRetrieveBufferThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}
			return 0;
		}
	}

	return 0;
}

#else

int overlay_open( char *dev, DisplayCfg *pDispCfg )
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct _sViewPortInfo    gViewPortInfo;
	struct _sViewPortOffset  gViewPortOffset;
	struct _sColorKeyNAlpha  colorkey;
	int                      format;

	int                      ret;

	if ( !pDispCfg || !dev )
	{
		return -1;
	}

	switch ( pDispCfg->format )
	{
	case CAM_IMGFMT_CbYCrY:
		format         = FB_VMODE_YUV422PACKED;
		pDispCfg->bpp  = 16;
		pDispCfg->step = pDispCfg->frame_width * 2;
		break;

	case CAM_IMGFMT_YCbYCr:
		format         = FB_VMODE_YUV422PACKED_SWAPYUorV;
		pDispCfg->bpp  = 16;
		pDispCfg->step = pDispCfg->frame_width * 2;
		break;

	case CAM_IMGFMT_YCbCr422P:
		format         = FB_VMODE_YUV422PLANAR;
		pDispCfg->bpp  = 16;
		pDispCfg->step = pDispCfg->frame_width;
		break;

	case CAM_IMGFMT_YCbCr420P:
	case CAM_IMGFMT_YCbCr420SP:
	case CAM_IMGFMT_YCrCb420SP: // hack for display velidation, overlay don't support sp display
		format         = FB_VMODE_YUV420PLANAR;
		pDispCfg->bpp  = 12;
		pDispCfg->step = pDispCfg->frame_width;
		break;

	case CAM_IMGFMT_YCrCb420P:
		format         = FB_VMODE_YUV420PLANAR_SWAPUV;
		pDispCfg->bpp  = 12;
		pDispCfg->step = pDispCfg->frame_width;
		break;

	case CAM_IMGFMT_BGR565:
		format         = FB_VMODE_RGB565;
		pDispCfg->bpp  = 16;
		pDispCfg->step = pDispCfg->frame_width * 2;
		break;

	case CAM_IMGFMT_BGRA8888:
		format         = FB_VMODE_RGB888UNPACK;
		pDispCfg->bpp  = 32;
		pDispCfg->step = pDispCfg->frame_width * 4;
		break;

	default:
		TRACE( "Error: not supported format( %s, %d )\n", __FILE__, __LINE__ );
		return -1;
	}

	memset( &fix, 0, sizeof(struct fb_fix_screeninfo) );
	memset( &var, 0, sizeof(struct fb_var_screeninfo) );
	memset( &gViewPortInfo, 0, sizeof(struct _sViewPortInfo) );
	memset( &gViewPortOffset, 0, sizeof(struct _sViewPortOffset) );

	pDispCfg->fd_lcd = open( dev, O_RDWR );
	if ( pDispCfg->fd_lcd < 0 )
	{
		TRACE( "open device %s failed( %s, %d )\n", dev, __FILE__, __LINE__ );
		return -2;
	}

	colorkey.mode              = FB_DISABLE_COLORKEY_MODE;
	colorkey.alphapath         = FB_CONFIG_ALPHA;
	colorkey.config            = 0xff;

	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SET_COLORKEYnALPHA, &colorkey );
	if ( ret )
	{
		TRACE( "Error: FB_IOCTL_SET_COLORKEYnALPHA failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_PUT_GLOBAL_ALPHABLEND, 0xff );
	if ( ret )
	{
		TRACE( "Error: FB_IOCTL_PUT_GLOBAL_ALPHABLEND failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	gViewPortInfo.srcWidth  = pDispCfg->frame_width;
	gViewPortInfo.srcHeight = pDispCfg->frame_height;
	gViewPortInfo.zoomXSize = pDispCfg->screen_width;
	gViewPortInfo.zoomYSize = pDispCfg->screen_height;

	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SET_VIEWPORT_INFO, &gViewPortInfo );
	if ( ret )
	{
		TRACE( "Error: FB_IOCTL_SET_VIEWPORT_INFO failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	gViewPortOffset.xOffset = pDispCfg->screen_pos_x;
	gViewPortOffset.yOffset = pDispCfg->screen_pos_y;

	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SET_VID_OFFSET, &gViewPortOffset );
	if ( ret )
	{
		TRACE( "Error: set view port offset failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	// always set VideoMode after ViewPortInfo, or driver will overwrite it
	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SET_VIDEO_MODE, &format );
	if ( ret )
	{
		TRACE( "Error: set video mode failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	pDispCfg->on = 0;
	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SWITCH_VID_OVLY, &pDispCfg->on );
	if ( ret )
	{
		TRACE( "Error: switch off video overlay failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -1;
	}

	/* get updated "fix" screeninfo */
	ret = ioctl( pDispCfg->fd_lcd, FBIOGET_FSCREENINFO, &fix );
	if ( ret )
	{
		TRACE( "Error: FBIOGET_FSCREENINFO failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -4;
	}

	/* get updated "var" screeninfo */
	ret = ioctl( pDispCfg->fd_lcd, FBIOGET_VSCREENINFO, &var );
	if ( ret )
	{
		TRACE( "Error: FBIOGET_VSCREENINFO failed, return value[%d]( %s, %d )\n", ret, __FILE__, __LINE__ );
		close( pDispCfg->fd_lcd );
		return -5;
	}

	pDispCfg->yoff  = var.red.offset;
	pDispCfg->ylen  = var.red.length;
	pDispCfg->cboff = var.green.offset;
	pDispCfg->cblen = var.green.length;
	pDispCfg->croff = var.blue.offset;
	pDispCfg->crlen = var.blue.length;

	pDispCfg->fix_smem_len = fix.smem_len;

	return 0;
}

void overlay_close( DisplayCfg *pDispCfg )
{
	if ( pDispCfg->fd_lcd >= 0 )
	{
		int ret;

		pDispCfg->on = 0;
		ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SWITCH_VID_OVLY, &pDispCfg->on );
		if ( ret != 0 )
		{
			TRACE( "Error: switch off video overlay failed( %s, %d )", __FILE__, __LINE__ );
		}

		close( pDispCfg->fd_lcd );
		pDispCfg->fd_lcd = -1;
	}

	return;
}

int display_to_overlay( CAM_DeviceHandle hHandle, DisplayCfg *pDispCfg, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error            error;
	struct _sOvlySurface stOvlySurface;
	int                  i, j;
	int                  ret;
	unsigned char        *pFreeBufAddr[MAX_LCD_BUF_NUM][3];  /* to store buffer which could be released */

	memset( pFreeBufAddr, 0, 3 * MAX_LCD_BUF_NUM * sizeof( unsigned char* ) );

	if ( !pDispCfg )
	{
		return -1;
	}

	memset( &stOvlySurface, 0, sizeof( struct _sOvlySurface ) );

	if ( ioctl( pDispCfg->fd_lcd, FB_IOCTL_GET_VIDEO_MODE, &stOvlySurface.videoMode ) )
	{
		TRACE( "Error: FB_IOCTL_GET_VIDEO_MODE failed( %s, %d )\n", __FILE__, __LINE__ );
		return -2;
	}

	if ( ioctl( pDispCfg->fd_lcd, FB_IOCTL_GET_VIEWPORT_INFO, &stOvlySurface.viewPortInfo ) )
	{
		TRACE( "Error: FB_IOCTL_GET_VIEWPORT_INFO failed( %s, %d )\n", __FILE__, __LINE__ );
		return -2;
	}

	if ( ioctl( pDispCfg->fd_lcd, FB_IOCTL_GET_VID_OFFSET, &stOvlySurface.viewPortOffset ) )
	{
		TRACE( "Error: FB_IOCTL_GET_VID_OFFSET failed( %s, %d )\n", __FILE__, __LINE__ );
		return -2;
	}

	switch ( stOvlySurface.videoMode )
	{
	case FB_VMODE_YUV420PLANAR:
		stOvlySurface.videoBufferAddr.startAddr[0] = (unsigned char*)pImgBuf->iPhyAddr[0];
		stOvlySurface.videoBufferAddr.startAddr[1] = (unsigned char*)pImgBuf->iPhyAddr[1];
		stOvlySurface.videoBufferAddr.startAddr[2] = (unsigned char*)pImgBuf->iPhyAddr[2];
		break;

	case FB_VMODE_YUV422PACKED:
	case FB_VMODE_YUV422PACKED_SWAPYUorV:
	case FB_VMODE_RGB565:
		stOvlySurface.videoBufferAddr.startAddr[0] = (unsigned char*)pImgBuf->iPhyAddr[0];
		stOvlySurface.videoBufferAddr.startAddr[1] = NULL;
		stOvlySurface.videoBufferAddr.startAddr[2] = NULL;
		break;

	default:
		TRACE( "Error: not supported format( %s, %d )\n", __FILE__, __LINE__ );
		return -1;
		break;
	}
	stOvlySurface.viewPortInfo.yPitch = pImgBuf->iStep[0];
	stOvlySurface.viewPortInfo.uPitch = pImgBuf->iStep[1];
	stOvlySurface.viewPortInfo.vPitch = pImgBuf->iStep[2];

	/* initialize ovly surface parameter */
	stOvlySurface.viewPortInfo.srcWidth  = pDispCfg->frame_width;
	stOvlySurface.viewPortInfo.srcHeight = pDispCfg->frame_height;

	stOvlySurface.viewPortInfo.zoomXSize = MIN( pDispCfg->screen_width, pDispCfg->frame_width );
	stOvlySurface.viewPortInfo.zoomYSize = MIN( pDispCfg->screen_height, pDispCfg->frame_height );
	stOvlySurface.viewPortOffset.xOffset = pDispCfg->screen_pos_x ;
	stOvlySurface.viewPortOffset.yOffset = pDispCfg->screen_pos_y;


	stOvlySurface.videoBufferAddr.frameID    = 0;
	stOvlySurface.videoBufferAddr.inputData  = 0;  // set as NULL when buf is allocated in user space
	stOvlySurface.videoBufferAddr.length     = pImgBuf->iFilledLen[0] + pImgBuf->iFilledLen[1] + pImgBuf->iFilledLen[2];

	ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_FLIP_VID_BUFFER, &stOvlySurface );
	if ( ret )
	{
		TRACE( "Error: FB_IOCTL_FLIP_VID_BUFFER failed( %s, %d )\n", __FILE__, __LINE__ );
		return -2;
	}

	if ( pDispCfg->on == 0 )
	{
		pDispCfg->on = 1;
		ret = ioctl( pDispCfg->fd_lcd, FB_IOCTL_SWITCH_VID_OVLY, &pDispCfg->on );
		if ( ret )
		{
			TRACE( "Error: switch on video overlay failed( %s, %d )\n", __FILE__, __LINE__ );
			close( pDispCfg->fd_lcd );
			return -1;
		}
	}

	// TRACE( "----------------- from camera: iPrivateIndex: %d, iPhyAddr[0]: %x---------------\n", pImgBuf->iPrivateIndex, pImgBuf->iPhyAddr[0] );

	for ( i = 0; i < iPrevBufNum; i++ )
	{
		if ( pPreviewBuf[i].iPhyAddr[0] == pImgBuf->iPhyAddr[0] )
		{
			pPreviewBufStatus[i] = 1; // 1 is in overlay
			iOvlyBufCnt++;
			break;
		}
	}
	// TRACE( "overlay buf cnt: %d\n", iOvlyBufCnt );

	/* get buffer list which could be released by overlay */
	if ( ioctl( pDispCfg->fd_lcd, FB_IOCTL_GET_FREELIST, &pFreeBufAddr ) )
	{
		TRACE( "Error: FB_IOCTL_GET_FREELIST failed( %s, %d )\n", __FILE__, __LINE__ );
		return -1;
	}

	for ( i = 0; i < MAX_LCD_BUF_NUM; i++ )
	{
		if ( pFreeBufAddr[i][0] ) // we use only Y-planar addr as match index
		{
			for ( j = 0; j < MAX_LCD_BUF_NUM; j++ )
			{
				if ( pFreeBufAddr[i][0] == (unsigned char*)pPreviewBuf[j].iPhyAddr[0] && pPreviewBufStatus[j] == 1 )
				{
					error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pPreviewBuf[j] );
					// TRACE( "****************from overlay: iPrivateIndex: %d, iPhyAddr[0]: %x**************\n" ,pPreviewBuf[j].iPrivateIndex, pPreviewBuf[j].iPhyAddr[0] );
					ASSERT_CAM_ERROR( error );
					iOvlyBufCnt--;
					pPreviewBufStatus[j] = 0;
					break;
				}
			}
			if ( j >= MAX_LCD_BUF_NUM )
			{
				TRACE( "Error: cannot find match between overlay free_list and app buffer list( %s, %d )\n", __FILE__, __LINE__ );
				return -2;
			}
		}
	}

	return 0;
}

#endif

int displaydemo_open( int iFrameWidth, int iFrameHeight, CAM_ImageFormat eFrameFormat, DisplayCfg *pDispCfg )
{
	int ret;
	memset( pDispCfg, 0, sizeof( DisplayCfg ) );

	pDispCfg->screen_pos_x = MAX( 0, ( SCREEN_WIDTH - iFrameWidth ) / 2 );
	pDispCfg->screen_pos_y = MAX( 0, ( SCREEN_HEIGHT - iFrameHeight ) / 2 );

	pDispCfg->screen_width  = iFrameWidth;
	pDispCfg->screen_height = iFrameHeight;

	pDispCfg->frame_width  = iFrameWidth;
	pDispCfg->frame_height = iFrameHeight;

	pDispCfg->format       = eFrameFormat;

	ret = overlay_open( DEFAULT_DISPLAY_DEVICE, pDispCfg );
	if ( ret != 0 )
	{
		if ( overlay_open( DEFAULT_DISPLAY_DEVICE2, pDispCfg ) != 0 )
		{
			return -1;
		}
	}

	return ( pDispCfg->fd_lcd > 0 ) ? 0 : -1;
}

int displaydemo_close( DisplayCfg *pDispCfg )
{
	if ( !pDispCfg )
	{
		return -1;
	}

	if ( pDispCfg->fd_lcd > 0 )
	{
		overlay_close( pDispCfg );

		pDispCfg->fd_lcd = -1;

		return 0;
	}

	return -1;
}

int displaydemo_display( CAM_DeviceHandle hHandle, DisplayCfg *pDispCfg, CAM_ImageBuffer *pImgBuf )
{
	int ret;

	ret = display_to_overlay( hHandle, pDispCfg, pImgBuf );

	return ret;
}

#endif
