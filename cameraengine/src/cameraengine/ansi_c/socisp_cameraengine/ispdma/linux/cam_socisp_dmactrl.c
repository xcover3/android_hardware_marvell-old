/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>

#include "cam_log.h"
#include "cam_utility.h"

#include "CameraEngine.h"
#include "cam_socisp_dmactrl.h"

#include "mmp_dxoisp.h"
#include "DxOISP_ahblAccess.h"

// input lane
typedef enum
{
	ISPDMA_INSRC_CSI_4LN,
	ISPDMA_INSRC_CSI_1LN,
	ISPDMA_INSRC_PSC_0,
	ISPDMA_INERC_PSC_1,
	ISPDMA_INSRC_AHBSLAVE,
	ISPDMA_INSRC_DMA,

	ISPDMA_INSRC_NUM,

	ISPDMA_INSRC_LIMIT = 0x7fffffff,
} _CAM_IspDmaInLane;

typedef struct
{
	int                     iSensorID;
	struct ispdma_in_config cfg_in;
}_CAM_IspDmaSensorInput;

typedef struct
{
	int                            fd;
	CAM_Int8u                      *ipc_addr;
	struct ispdma_in_config        cfg_in;
	struct ispdma_display_config   cfg_disp;
	struct ispdma_codec_config     cfg_codec;
	struct ispdma_fb_config        cfg_fb;
	struct ispdma_streaming_config cfg_onoff;
}_CAM_IspDmaState;

static _CAM_IspDmaSensorInput gSensorLaneMap[] = {
                                                   { 0, { ISP_INPUT_PATH_CSI0, 10, 0, 0, .par.csi = { ISPDMA_CSI_LITTLEENDIAN, 0 } } },
                                                   { 1, { ISP_INPUT_PATH_CSI0, 10, 0, 0, .par.csi = { ISPDMA_CSI_LITTLEENDIAN, 0 } } },
                                                 };
#define SENSOR_NUM _ARRAY_SIZE( gSensorLaneMap )

#define IPC_ADDR_SIZE (0x40000)

static CAM_Int8u* ipc_addr = 0;

CAM_Int8u DxOISP_HasInputFifoOverflow = 0; // XXX: what this flag for?

/* map format between camera-engine and isp-dma driver */
int _ce2driver_format( CAM_ImageFormat eFormat )
{
	int format = -1;

	switch ( eFormat )
	{
	case CAM_IMGFMT_CbYCrY:
		format = ISP_FORMAT_UYVY;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	return format;
}

CAM_Error ispdma_get_tick( void *hIspDmaHandle, CAM_Tick *ptTick )
{
	return CAM_ERROR_NOTIMPLEMENTED;
}


inline CAM_Error ispdma_poll_ipc( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Tick *ptTick )
{
	_CAM_IspDmaState      *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;
	CAM_Int32s            ret;
	struct ispdma_ipcwait wait = {0, 0};

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( ptTick );

	wait.timeout = iTimeout;

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_EVENT_WAIT_IPC, &wait );
	if ( ret < 0 )
	{
		*ptTick = 0;
		TRACE( CAM_WARN, "Warning: IPC event polling timeout %d ms( %s, %s, %d )!\n", wait.timeout, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_TIMEOUT;
	}

	*ptTick = wait.tick;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_poll_buffer( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Int32s iRequest, CAM_Int32s *piResult )
{
	_CAM_IspDmaState       *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;
	CAM_Int32s             ret;
	struct ispdma_pollinfo poll          = {0, 0, 0};

	_CHECK_BAD_POINTER( hIspDmaHandle );

	poll.timeout = iTimeout;
	poll.request = iRequest;

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_BUFFER_POLL, &poll );

	*piResult = poll.result;

	if ( ret < 0 )
	{
		TRACE( CAM_WARN, "Warning: buffer polling timeout %d ms( %s, %s, %d )!\n", poll.timeout, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_TIMEOUT;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_init( void **phIspDmaHandle )
{
	_CHECK_BAD_POINTER( phIspDmaHandle );

	ASSERT( *phIspDmaHandle == NULL );

	_CAM_IspDmaState *pIspDmaState = NULL;

	pIspDmaState = (_CAM_IspDmaState*)malloc( sizeof( _CAM_IspDmaState ) );
	if ( pIspDmaState == NULL )
	{
		TRACE( CAM_ERROR, "Error: no enough memory to create soc-isp-dma devices( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pIspDmaState, 0, sizeof( _CAM_IspDmaState ) );


	pIspDmaState->fd = open( "/dev/mmp-dxoisp", O_RDWR );
	if ( pIspDmaState->fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to open soc-isp-dma ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	pIspDmaState->ipc_addr = (CAM_Int8u*)mmap( 0, IPC_ADDR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pIspDmaState->fd, 0 );
	if ( pIspDmaState->ipc_addr == NULL )
	{
		TRACE( CAM_ERROR, "Error: can't map ipc registers( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	ipc_addr = pIspDmaState->ipc_addr;

	// ASSERT( ioctl( pIspDmaState->fd, ISP_IOCTL_DEBUG_ENABLE, 3 ) == 0 );

	*phIspDmaHandle = pIspDmaState;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_deinit( void **phIspDmaHandle )
{
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)(*phIspDmaHandle);
	CAM_Int32s       ret           = 0;

	ASSERT( phIspDmaHandle != NULL && *phIspDmaHandle != NULL );

	if ( pIspDmaState->fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: soc-isp-dma is not opened yet ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	if ( pIspDmaState->ipc_addr != NULL )
	{
		ret = munmap( pIspDmaState->ipc_addr, IPC_ADDR_SIZE );
		ASSERT( ret == 0 );
		pIspDmaState->ipc_addr = NULL;
		ipc_addr               = 0;
	}

	close( pIspDmaState->fd );

	free( *phIspDmaHandle );
	*phIspDmaHandle = NULL;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_reset( void *hIspDmaHandle )
{
	return CAM_ERROR_NOTIMPLEMENTED;
}

CAM_Error ispdma_enum_sensor( CAM_Int32s *piSensorCount, CAM_DeviceProp astCameraProp[CAM_MAX_SUPPORT_CAMERA] )
{
	*piSensorCount                       = 1;
	astCameraProp[0].eInstallOrientation = CAM_FLIPROTATE_NORMAL;
	astCameraProp[0].iFacing             = CAM_SENSOR_FACING_BACK;
	astCameraProp[0].iOutputProp         = CAM_OUTPUT_DUALSTREAM;
	strcpy( astCameraProp[0].sName, "OV8820" );

	return CAM_ERROR_NONE;
}


// isp-dma configuration
CAM_Error ispdma_select_sensor( void *hIspDmaHandle, CAM_Int32s iSensorID )
{
	CAM_Int32u       i             = 0;
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	// TODO: need map sensor ID to input lane, and keep it as a table
	for ( i = 0; i < SENSOR_NUM; i++ )
	{
		if ( iSensorID == gSensorLaneMap[i].iSensorID )
		{
			pIspDmaState->cfg_in = gSensorLaneMap[i].cfg_in;
			break;
		}
	}
	ASSERT( i < SENSOR_NUM );

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_INPUT, &pIspDmaState->cfg_in );
	ASSERT( ret == 0 );

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_output_format( void *hIspDmaHandle, _CAM_IspDmaFormat *pDmaFormat, _CAM_IspPort ePortID )
{
	CAM_Int32s       i             = 0;
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	if ( ePortID == ISP_PORT_DISPLAY )
	{
		pIspDmaState->cfg_disp.format = _ce2driver_format( pDmaFormat->eFormat );
		pIspDmaState->cfg_disp.w      = pDmaFormat->iWidth;
		pIspDmaState->cfg_disp.h      = pDmaFormat->iHeight;
		pIspDmaState->cfg_disp.burst  = 256;

		TRACE( CAM_INFO, "set up display dma, width: %d, height: %d, format: %d\n", pIspDmaState->cfg_disp.w, pIspDmaState->cfg_disp.h, pIspDmaState->cfg_disp.format );

		ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_DISPLAY, &pIspDmaState->cfg_disp );
	}
	else if ( ePortID == ISP_PORT_CODEC )
	{
		pIspDmaState->cfg_codec.format  = _ce2driver_format( pDmaFormat->eFormat );
		pIspDmaState->cfg_codec.w       = pDmaFormat->iWidth;
		pIspDmaState->cfg_codec.h       = pDmaFormat->iHeight;
		pIspDmaState->cfg_codec.burst   = 256;

		TRACE( CAM_INFO, "set up codec dma, width: %d, height: %d, format: %d\n",
		       pIspDmaState->cfg_codec.w, pIspDmaState->cfg_codec.h, pDmaFormat->eFormat );

		ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_CODEC, &pIspDmaState->cfg_codec );
	}

	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: setup ispdma driver output format failed, error code[%s]( %s, %s, %d )\n", strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_codec_property( void *hIspDmaHandle, _CAM_IspDmaProperty *pDmaProperty )
{
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	pIspDmaState->cfg_codec.reorder = pDmaProperty->iBandCnt > 1 ? 1 : 0;
	pIspDmaState->cfg_codec.vbSize  = pDmaProperty->iBandSize;
	pIspDmaState->cfg_codec.vbNum   = pDmaProperty->iBandCnt;
	pIspDmaState->cfg_codec.mode    = pDmaProperty->iBufHandleMode;
	pIspDmaState->cfg_codec.burst   = 256;

	TRACE( CAM_INFO, "set up codec dma property, burst: %d codec dma band number: %d, band size: %d\n",
	       pIspDmaState->cfg_codec.burst,
	       pIspDmaState->cfg_codec.vbNum, pIspDmaState->cfg_codec.vbSize );

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_CODEC, &pIspDmaState->cfg_codec );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: setup ispdma driver codec property failed, error code[%s]( %s, %s, %d )\n", strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_framebuffer( void *hIspDmaHandle, _CAM_IspDmaBufNode astFBuf[NB_FRAME_BUFFER], CAM_Int32s iBandCnt )
{
	CAM_Int32s       i             = 0;
	CAM_Int32s       ret           = 0;
	CAM_Bool         bStreamOn     = ( iBandCnt > 0 ? CAM_TRUE : CAM_FALSE );
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	if ( bStreamOn )
	{
		pIspDmaState->cfg_fb.burst_read  = 64;
		pIspDmaState->cfg_fb.burst_write = 256;
		pIspDmaState->cfg_fb.fbcnt       = iBandCnt;
		for ( i = 0; i < iBandCnt; i++ )
		{
			pIspDmaState->cfg_fb.virAddr[i] = astFBuf[i].pVirtAddr;
			pIspDmaState->cfg_fb.phyAddr[i] = astFBuf[i].uiPhyAddr;
			pIspDmaState->cfg_fb.size[i]    = astFBuf[i].iBufLen;

			TRACE( CAM_INFO, "fb dma NO.: %d, size: %d\n", i, pIspDmaState->cfg_fb.size[i] );
		}
		ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_FB, &pIspDmaState->cfg_fb );
		if ( ret < 0 )
		{
			TRACE( CAM_ERROR, "Error: setup frame buffer failed, error code[%s]( %s, %s, %d )\n", strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}

	pIspDmaState->cfg_onoff.enable_fbrx = pIspDmaState->cfg_onoff.enable_fbtx = bStreamOn;
	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_STREAMING, &pIspDmaState->cfg_onoff );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: stream %s frame buffer failed, error code[%s]( %s, %s, %d )\n", bStreamOn ? "on" : "off", strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_sensor_streamon( void *hIspDmaHandle, CAM_Bool bOn )
{
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	if ( pIspDmaState->cfg_onoff.enable_in != bOn )
	{
		pIspDmaState->cfg_onoff.enable_in = bOn;

		ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_STREAMING, &pIspDmaState->cfg_onoff );
		if ( ret < 0 )
		{
			TRACE( CAM_ERROR, "Error: stream %s sensor failed, error code[%s]( %s, %s, %d )\n", bOn ? "on" : "off", strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_isp_streamon( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bOn )
{
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	ASSERT( ePortID == ISP_PORT_DISPLAY || ePortID == ISP_PORT_CODEC || ePortID == ISP_PORT_BOTH );

	switch ( ePortID )
	{
	case ISP_PORT_DISPLAY:
		pIspDmaState->cfg_onoff.enable_disp  = bOn;
		break;

	case ISP_PORT_CODEC:
		pIspDmaState->cfg_onoff.enable_codec = bOn;
		break;

	case ISP_PORT_BOTH:
		pIspDmaState->cfg_onoff.enable_disp  = bOn;
		pIspDmaState->cfg_onoff.enable_codec = bOn;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_ISPDMA_SETUP_STREAMING, &pIspDmaState->cfg_onoff );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: stream %s port[%d] failed.[%s]( %s, %s, %d )\n", bOn ? "on" : "off", ePortID, strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_isp_set_mode( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bIsStreaming )
{
    /* Do nothing,this interface is for mc only */

    return CAM_ERROR_NONE;
}

// buffer management
CAM_Error ispdma_enqueue_buffer( void *hIspDmaHandle, CAM_ImageBuffer *pstImgBuf, _CAM_IspPort ePortID )
{
	struct ispdma_buffer buf;
	CAM_Int32s           ret = 0;
	_CAM_IspDmaState     *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	ASSERT( ePortID == ISP_PORT_DISPLAY || ePortID == ISP_PORT_CODEC );

	memset( &buf, 0, sizeof( struct ispdma_buffer ) );

	if ( ePortID == ISP_PORT_DISPLAY )
	{
		buf.port = ISP_OUTPUT_PORT_DISPLAY;
	}
	else
	{
		buf.port = ISP_OUTPUT_PORT_CODEC;
	}

	buf.user_index = pstImgBuf->iUserIndex;
	buf.user_data  = (void*)pstImgBuf;
	buf.virAddr[0] = pstImgBuf->pBuffer[0];
	buf.phyAddr[0] = pstImgBuf->iPhyAddr[0];
	buf.pitch[0]   = pstImgBuf->iStep[0];
	buf.format     = _ce2driver_format( pstImgBuf->eFormat );
	buf.w          = pstImgBuf->iWidth;
	buf.h          = pstImgBuf->iHeight;
	buf.tick       = 0;

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_BUFFER_ENQUEUE, &buf );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: Isp En-queue buffer to port[%d] failed, error code[%s]( %s, %s, %d )\n", ePortID, strerror(errno), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}


CAM_Error ispdma_dequeue_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_ImageBuffer **ppImgBuf, CAM_Bool *pbIsError )
{
	struct ispdma_buffer buf;
	CAM_Int32s           ret           = 0;
	_CAM_IspDmaState     *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;
	CAM_Error            error         = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	ASSERT( ePortID == ISP_PORT_DISPLAY || ePortID == ISP_PORT_CODEC );

	memset( &buf, 0, sizeof( struct ispdma_buffer ) );

	if ( ePortID == ISP_PORT_DISPLAY )
	{
		buf.port = ISP_OUTPUT_PORT_DISPLAY;
	}
	else
	{
		buf.port = ISP_OUTPUT_PORT_CODEC;
	}

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_BUFFER_BLOCK_DEQUEUE, &buf );

	if ( ret == 0 )
	{
		*ppImgBuf   = buf.user_data;
		*pbIsError  = buf.errcnt > 0 ? CAM_TRUE : CAM_FALSE;

		_calcimglen( *ppImgBuf );

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: dma dequeue buffer failed, error code[%d] ( %s, %s, %d )\n", ret, __FILE__, __FUNCTION__, __LINE__ );
		error         = CAM_ERROR_BUFFERUNAVAILABLE;
		*pbIsError    = CAM_TRUE;
	}

	return error;
}

CAM_Error ispdma_flush_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID )
{
	int              port          = -1;
	CAM_Int32s       ret           = 0;
	_CAM_IspDmaState *pIspDmaState = (_CAM_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	ASSERT( ePortID == ISP_PORT_DISPLAY || ePortID == ISP_PORT_CODEC || ePortID == ISP_PORT_BOTH );

	if ( ePortID == ISP_PORT_DISPLAY )
	{
		port = ISP_OUTPUT_PORT_DISPLAY;
	}
	else if ( ePortID == ISP_PORT_CODEC )
	{
		port = ISP_OUTPUT_PORT_CODEC;
	}
	else
	{
		port = ISP_OUTPUT_PORT_BOTH;
	}

	ret = ioctl( pIspDmaState->fd, ISP_IOCTL_BUFFER_FLUSH, port );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: flush isp-dma failed, error code[%s]( %s, %d )\n", strerror(errno), __FILE__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_get_bufreq( void *hIspDmaHandle, _CAM_ImageInfo *pstConfig, CAM_ImageBufferReq *pstBufReq )
{
	// BAC check
	_CHECK_BAD_POINTER( pstConfig );
	_CHECK_BAD_POINTER( pstBufReq );

	pstBufReq->eFormat = pstConfig->eFormat;

	pstBufReq->iWidth  = pstConfig->iWidth;
	pstBufReq->iHeight = pstConfig->iHeight;

	// 8 byte alignment is required by DMA
	pstBufReq->iAlignment[0] = 8;
	if ( pstConfig->eFormat == CAM_IMGFMT_YCbCr444P || pstConfig->eFormat == CAM_IMGFMT_YCbCr422P ||
	     pstConfig->eFormat == CAM_IMGFMT_YCbCr420P || pstConfig->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		// 8 byte alignment is required by DMA
		pstBufReq->iAlignment[1] = 8;
		pstBufReq->iAlignment[2] = 8;
	}
	else if ( pstConfig->eFormat == CAM_IMGFMT_YCbCr420SP || pstConfig->eFormat == CAM_IMGFMT_YCrCb420SP )
	{
		// 8 byte alignment is required by DMA
		pstBufReq->iAlignment[1] = 8;
		pstBufReq->iAlignment[2] = 0;
	}
	else
	{
		pstBufReq->iAlignment[1] = 0;
		pstBufReq->iAlignment[2] = 0;
	}

	pstBufReq->iRowAlign[0] = 1;
	if ( pstConfig->eFormat == CAM_IMGFMT_YCbCr444P || pstConfig->eFormat == CAM_IMGFMT_YCbCr422P ||
	     pstConfig->eFormat == CAM_IMGFMT_YCbCr420P || pstConfig->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		pstBufReq->iRowAlign[0] = 8;
		pstBufReq->iRowAlign[1] = 8;
		pstBufReq->iRowAlign[2] = 8;
	}
	else if ( pstConfig->eFormat == CAM_IMGFMT_YCbCr420SP || pstConfig->eFormat == CAM_IMGFMT_YCrCb420SP )
	{
		pstBufReq->iRowAlign[0] = 8;
		pstBufReq->iRowAlign[1] = 8;
		pstBufReq->iRowAlign[2] = 0;
	}
	else
	{
		pstBufReq->iRowAlign[1] = 0;
		pstBufReq->iRowAlign[2] = 0;
	}

	pstBufReq->iFlagOptimal = BUF_FLAG_PHYSICALCONTIGUOUS |
	                          BUF_FLAG_L1CACHEABLE | BUF_FLAG_L2CACHEABLE | BUF_FLAG_BUFFERABLE |
	                          BUF_FLAG_YUVBACKTOBACK | BUF_FLAG_FORBIDPADDING;

	pstBufReq->iFlagAcceptable = pstBufReq->iFlagOptimal | BUF_FLAG_L1NONCACHEABLE | BUF_FLAG_L2NONCACHEABLE | BUF_FLAG_UNBUFFERABLE;

	pstBufReq->iMinBufCount = pstConfig->bIsStreaming == CAM_TRUE ? 3 : 1;
	_calcstep( pstBufReq->eFormat,pstBufReq->iWidth, pstBufReq->iRowAlign, pstBufReq->iMinStep );


	_calcbuflen( pstBufReq->eFormat, pstBufReq->iWidth, pstBufReq->iHeight, pstBufReq->iMinStep, pstBufReq->iMinBufLen, NULL );

	/*
	CELOG( "Info: format = %d, width = %d, height = %d, align = %d, step = %d, %d, %d, size = %d, %d, %d\n",
	       pstBufReq->eFormat, pstBufReq->iWidth, pstBufReq->iHeight, pstBufReq->iRowAlign,
	       pstBufReq->iMinStep[0], pstBufReq->iMinStep[1], pstBufReq->iMinStep[2],
	       pstBufReq->iMinBufLen[0], pstBufReq->iMinBufLen[1], pstBufReq->iMinBufLen[2] );
	*/

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_get_min_bufcnt( void *hIspDmaHandle, CAM_Bool bIsStreaming, CAM_Int32s *pMinBufCnt )
{
	_CHECK_BAD_POINTER( pMinBufCnt );

	*pMinBufCnt = ( bIsStreaming == CAM_TRUE ) ? 3 : 1;

	return CAM_ERROR_NONE;
}

/* isp register access */
inline void DxOISP_setRegister( const uint32_t uiOffset, uint32_t uiValue )
{
#if 0
	struct dxo_regs dxoreg = {0};

	assert( uiOffset % sizeof(uint32_t) == 0 );

	memset( &dxoreg, 0, sizeof(dxoreg) );
	dxoreg.offset = uiOffset;
	dxoreg.value  = uiValue;

	ioctl( ispdma_fd, ISP_IOCTL_IPCREG_SET, &dxoreg );
#else
	ASSERT( ipc_addr != 0 );

	*(uint32_t*)(ipc_addr + uiOffset) = uiValue;

#endif
}

inline uint32_t DxOISP_getRegister( const uint32_t uiOffset )
{
	uint32_t uiResult = 0;
#if 0
	struct dxo_regs dxoreg = {0};

	assert( uiOffset % sizeof(uint32_t) == 0 );

	memset( &dxoreg, 0, sizeof( dxoreg ) );
	dxoreg.offset = uiOffset;
	ioctl( ispdma_fd, ISP_IOCTL_IPCREG_GET, &dxoreg );
	uiResult = dxoreg.value;
#else
	ASSERT( ipc_addr != 0 );

	uiResult = *(uint32_t*)(ipc_addr + uiOffset);
#endif

	return uiResult;
}

inline void DxOISP_setMultiRegister( const uint32_t uiOffset, const uint32_t* puiSrc, uint32_t uiNbElem )
{
	uint32_t i = 0;
#if 0
	struct dxo_regs dxoreg = {0};

	assert( uiOffset % sizeof( uint32_t ) == 0 );

	memset( &dxoreg, 0, sizeof( dxoreg ) );
	dxoreg.offset = uiOffset;
	dxoreg.src    = puiSrc;
	dxoreg.cnt    = uiNbElem;

	ioctl( ispdma_fd, ISP_IOCTL_IPCREG_SET_MULTI, &dxoreg );
#else
	ASSERT( ipc_addr != 0 );

	for ( i = 0; i < uiNbElem; i++ )
	{
		*(uint32_t*)(ipc_addr + uiOffset) = puiSrc[i];
	}
#endif

	return;
}

inline void DxOISP_getMultiRegister( const uint32_t uiOffset, uint32_t* puiDst, uint32_t uiNbElem )
{
	uint32_t i = 0;
#if 0
	struct dxo_regs dxoreg = {0};
	uint32_t        uiResult;

	assert( uiOffset % sizeof( uint32_t ) == 0 );

	memset( &dxoreg, 0, sizeof( dxoreg ) );
	dxoreg.offset = uiOffset;
	dxoreg.dst    = puiDst;
	dxoreg.cnt    = uiNbElem;

	ioctl( ispdma_fd, ISP_IOCTL_IPCREG_GET_MULTI, &dxoreg );
#else
	ASSERT( ipc_addr != 0 );

	for ( i = 0; i < uiNbElem; i++ )
	{
		puiDst[i] = *(uint32_t*)( ipc_addr + uiOffset );
	}
#endif

	return;
}
