/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/mman.h>

#include "cam_log.h"
#include "cam_utility.h"

#include "CameraEngine.h"
#include "cam_socisp_dmactrl.h"

#include "mvisp.h"
#include "DxOISP_ahblAccess.h"
#include "cam_socisp_media.h"
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <sys/poll.h>
#include <linux/videodev2.h>

typedef struct
{
	CAM_DeviceProp  stSensorProp;
	CAM_Int32s      iMaxWidth;
	CAM_Int32s      iMaxHeight;
	CAM_Int32s      iSensorID;
} _CAM_SensorDesc;

typedef struct
{
	struct v4l2_dxoipc_config_codec     stCodecProp;
	struct v4l2_format                  stCodecFmt;
} _CAM_IspDmaCodecPort;

typedef struct
{
	struct v4l2_buffer                  stV4L2Buf; // V4L2 buffer
	CAM_ImageBuffer                     *pImgBuf;  // pointer to CAM_ImageBuffer structure
	CAM_Bool                            iFlag;     // 1: buffer is in driver, 0: buffer is not in driver
} _CAM_MCBufDesc;


typedef struct
{
	CAM_Int8u                           *pucIPCAddr;

	_CAM_MediaDevice                    *pstMedia;
	_CAM_MediaEntity                    *pstSensor;
	_CAM_MediaEntity                    *pstCCIC;
	_CAM_MediaEntity                    *pstCCICRaw;
	_CAM_MediaEntity                    *pstIsp;
	_CAM_MediaEntity                    *pstIspIn;
	_CAM_MediaEntity                    *pstIspDisp;
	_CAM_MediaEntity                    *pstIspCodec;

	struct v4l2_format                  stCfgDispFmt;
	_CAM_IspDmaCodecPort                stCfgCodec;
	struct v4l2_format                  stCfgCCICRaw;
	struct v4l2_dxoipc_set_fb           stCfgFB;
	struct v4l2_dxoipc_streaming_config stCfgOnOff;
	CAM_Bool                            bIsCCICRawEnabled;

	_CAM_MCBufDesc                      astBufPool[2][CAM_MAX_PORT_BUF_CNT];
	CAM_Int32s                          iMCBufDescCount[2]; // number of CAM_ImageBuffer recognized in wrapper
	CAM_Int32s                          iPortBufNum[2];     // number of buffers in driver
	// XXX: We decide to hide raw data port with codec port, but use port format
	// to distinguish them. If the format is directly supported by sensor, we'll
	// use raw data path, otherwise, use ISP
	// This is a temporary solution, after HW is ready and triple port output use
	// case is necessary, we'll upgrade our design.
	CAM_ImageFormat                     aePortFormat[2];
} _CAM_MC_IspDmaState;


static _CAM_SensorDesc gSensorSupported[] =
{
	{ {"ov8820", CAM_FLIPROTATE_NORMAL, CAM_SENSOR_FACING_BACK, CAM_OUTPUT_DUALSTREAM }, 3264u, 2448u, 0 },
};

#define MAX_SUPPORT_SENSOR_NUM  _ARRAY_SIZE( gSensorSupported )

#define pucIPCAddr_SIZE (0x40000)

static CAM_Int8u      *pucIPCAddr  = 0;
CAM_Int32s            iIspCameraFd = -1;
CAM_Int32s            iIspCCICFd   = -1;


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////          INTERNAL APIs           //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

static CAM_Error _ce2driver_format( CAM_ImageFormat eFormat, CAM_Int32u *pDstFmt )
{
	CAM_Int32u iFmt = 0;

	switch ( eFormat )
	{
	case CAM_IMGFMT_CbYCrY:
		iFmt = V4L2_PIX_FMT_UYVY;
		break;

	case CAM_IMGFMT_RGGB8:
		iFmt = V4L2_PIX_FMT_SRGGB8;
		break;

	case CAM_IMGFMT_BGGR8:
		iFmt = V4L2_PIX_FMT_SBGGR8;
		break;

	case CAM_IMGFMT_GRBG8:
		iFmt = V4L2_PIX_FMT_SGRBG8;
		break;

	case CAM_IMGFMT_GBRG8:
		iFmt = V4L2_PIX_FMT_SGBRG8;
		break;

	case CAM_IMGFMT_RGGB10:
		iFmt = V4L2_PIX_FMT_SRGGB10;
		break;

	case CAM_IMGFMT_BGGR10:
		iFmt = V4L2_PIX_FMT_SBGGR10;
		break;

	case CAM_IMGFMT_GRBG10:
		iFmt = V4L2_PIX_FMT_SGRBG10;
		break;

	case CAM_IMGFMT_GBRG10:
		iFmt = V4L2_PIX_FMT_SGBRG10;
		break;

	default:
		*pDstFmt = -1;
		return CAM_ERROR_BADARGUMENT;
		break;
	}

	*pDstFmt = iFmt;

	return CAM_ERROR_NONE;
}

static CAM_Error _config_pipeline( _CAM_MC_IspDmaState *pIspDmaState, CAM_Int32s iWidth, CAM_Int32s iHeight )
{
	struct  v4l2_subdev_format  stSubDevFmt;
	CAM_Int32s                  ret = 0;
	// this is just a convention with driver. And it only means 10bit Bayer
	// raw. Bayer order or SRGB means nothing here.
	CAM_Int32u                  uiCode = V4L2_MBUS_FMT_SBGGR10_1X10, uiColorSpace = V4L2_COLORSPACE_SRGB;

	// Width/Height are fake values, they won't be affect functionality of whole pipeline
	// We set these value here just handshake all components in the pipeline

	memset( &stSubDevFmt, 0x00, sizeof( struct v4l2_subdev_format ) );
	stSubDevFmt.which                = V4L2_SUBDEV_FORMAT_ACTIVE;
	stSubDevFmt.pad                  = 0;
	stSubDevFmt.format.width         = iWidth;
	stSubDevFmt.format.height        = iHeight;
	stSubDevFmt.format.code          = uiCode;
	stSubDevFmt.format.field         = V4L2_FIELD_NONE;
	stSubDevFmt.format.colorspace    = uiColorSpace;
	ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_SUBDEV_S_FMT, &stSubDevFmt );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set isp format, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	memset( &stSubDevFmt, 0x00, sizeof( struct v4l2_subdev_format ) );
	stSubDevFmt.which                = V4L2_SUBDEV_FORMAT_ACTIVE;
	stSubDevFmt.pad                  = 1;
	stSubDevFmt.format.width         = iWidth;
	stSubDevFmt.format.height        = iHeight;
	stSubDevFmt.format.code          = uiCode;
	stSubDevFmt.format.field         = V4L2_FIELD_NONE;
	stSubDevFmt.format.colorspace    = uiColorSpace;
	ret = mc_ioctl( pIspDmaState->pstCCIC->fd, VIDIOC_SUBDEV_S_FMT, &stSubDevFmt );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set ccic pad[1] format, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	memset( &stSubDevFmt, 0x00, sizeof( struct v4l2_subdev_format ) );
	stSubDevFmt.which                = V4L2_SUBDEV_FORMAT_ACTIVE;
	stSubDevFmt.pad                  = 0;
	stSubDevFmt.format.width         = iWidth;
	stSubDevFmt.format.height        = iHeight;
	stSubDevFmt.format.code          = uiCode;
	stSubDevFmt.format.field         = V4L2_FIELD_NONE;
	stSubDevFmt.format.colorspace    = uiColorSpace;
	ret = mc_ioctl( pIspDmaState->pstCCIC->fd, VIDIOC_SUBDEV_S_FMT, &stSubDevFmt );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set ccic pad[0] format, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	memset( &stSubDevFmt, 0x00, sizeof( struct v4l2_subdev_format ) );
	stSubDevFmt.which                = V4L2_SUBDEV_FORMAT_ACTIVE;
	stSubDevFmt.pad                  = 0;
	stSubDevFmt.format.width         = iWidth;
	stSubDevFmt.format.height        = iHeight;
	stSubDevFmt.format.code          = uiCode;
	stSubDevFmt.format.field         = V4L2_FIELD_NONE;
	stSubDevFmt.format.colorspace    = uiColorSpace;
	ret = mc_ioctl( pIspDmaState->pstSensor->fd, VIDIOC_SUBDEV_S_FMT, &stSubDevFmt );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set sensor format, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _set_links_by_pad( _CAM_MC_IspDmaState *pIspDmaState,
                                    _CAM_MediaEntity    *pstEntity,
                                    _CAM_MediaPad       *pstSrcPad,
                                    _CAM_MediaPad       *pstDstPad )
{
	CAM_Int32u      i;
	_CAM_MediaLink  *pstLink = NULL;
	CAM_Int32s      ret = 0;


	for ( i = 0; i < pstEntity->uiNumLinks; i++ )
	{
		pstLink = &pstEntity->astLinks[i];

		if ( pstLink->pstSrcPad == pstSrcPad && pstLink->pstDstPad == pstDstPad )
		{
			break;
		}
	}

	if ( i == pstEntity->uiNumLinks )
	{
		TRACE( CAM_ERROR, "Error: failed to search the link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	ret = media_setup_link( pIspDmaState->pstMedia, pstLink->pstSrcPad, pstLink->pstDstPad, MEDIA_LNK_FL_ENABLED );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set media link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _free_links_by_pad( _CAM_MC_IspDmaState *pIspDmaState,
                                     _CAM_MediaEntity    *pstEntity,
                                     _CAM_MediaPad       *pstSrcPad,
                                     _CAM_MediaPad       *pstDstPad )
{
	CAM_Int32u             i;
	CAM_Int32s             ret = CAM_ERROR_NONE;
	_CAM_MediaLink         *pstLink = NULL;
	struct media_link_desc stULink;

	for ( i = 0; i < pstEntity->uiNumLinks; i++ )
	{
		pstLink = &pstEntity->astLinks[i];

		if ( pstLink->pstSrcPad == pstSrcPad && pstLink->pstDstPad == pstDstPad )
		{
			break;
		}
	}

	if ( i == pstEntity->uiNumLinks )
	{
		TRACE( CAM_ERROR, "Error: failed to search the link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	/* source pad */
	stULink.source.entity   = pstLink->pstSrcPad->pstEntity->stInfo.id;
	stULink.source.index    = pstLink->pstSrcPad->uiIndex;
	stULink.source.flags    = MEDIA_PAD_FL_SOURCE;

	/* sink pad */
	stULink.sink.entity     = pstLink->pstDstPad->pstEntity->stInfo.id;
	stULink.sink.index      = pstLink->pstDstPad->uiIndex;
	stULink.sink.flags      = MEDIA_PAD_FL_SINK;

	stULink.flags = 0;

	ret = mc_ioctl( pIspDmaState->pstMedia->fd, MEDIA_IOC_SETUP_LINK, &stULink );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: free link failed, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _set_mc_links( _CAM_MC_IspDmaState *pIspDmaState )
{
	_CAM_MediaPad   *pstSrcPad, *pstDstPad;
	CAM_Error       error = CAM_ERROR_NONE;

	pstSrcPad = &pIspDmaState->pstCCIC->astPads[1];
	pstDstPad = &pIspDmaState->pstIsp->astPads[0];  // isp: pad_0 input; pad_1: codec output; pad_2: display output
	error = _set_links_by_pad( pIspDmaState,pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to set link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstCCIC->astPads[2];
	pstDstPad = &pIspDmaState->pstCCICRaw->astPads[0];
	error = _set_links_by_pad( pIspDmaState,pIspDmaState->pstCCIC, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to set link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstIsp->astPads[2];
	pstDstPad = &pIspDmaState->pstIspDisp->astPads[0];
	error = _set_links_by_pad( pIspDmaState,pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to set link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstIsp->astPads[1];
	pstDstPad = &pIspDmaState->pstIspCodec->astPads[0];
	error = _set_links_by_pad( pIspDmaState,pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to set link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	return error;
}

static CAM_Error _free_mc_links( _CAM_MC_IspDmaState *pIspDmaState )
{
	_CAM_MediaPad   *pstSrcPad, *pstDstPad;
	CAM_Error       error = CAM_ERROR_NONE;

	pstSrcPad = &pIspDmaState->pstSensor->astPads[0];
	pstDstPad = &pIspDmaState->pstCCIC->astPads[0];
	error = _free_links_by_pad( pIspDmaState, pIspDmaState->pstCCIC, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to free link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstIsp->astPads[1];
	pstDstPad = &pIspDmaState->pstIspCodec->astPads[0];
	error = _free_links_by_pad( pIspDmaState, pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to free link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstIsp->astPads[2];
	pstDstPad = &pIspDmaState->pstIspDisp->astPads[0];
	error = _free_links_by_pad( pIspDmaState, pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to free link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	pstSrcPad = &pIspDmaState->pstCCIC->astPads[1];
	pstDstPad = &pIspDmaState->pstIsp->astPads[0];
	error = _free_links_by_pad( pIspDmaState, pIspDmaState->pstIsp, pstSrcPad, pstDstPad );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to free link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	return error;
}

static CAM_Error _close_mc_drv_res( _CAM_MC_IspDmaState *pIspDmaState )
{
	CAM_Int32s ret;

	if ( pIspDmaState == NULL )
	{
		return CAM_ERROR_NONE;
	}

	if ( pIspDmaState->pucIPCAddr != NULL )
	{
		ret = munmap( pIspDmaState->pucIPCAddr, pucIPCAddr_SIZE );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: memeory unmap failed, error code[%s]( %s, %s, %d )", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
		pIspDmaState->pucIPCAddr = NULL;
		pucIPCAddr               = 0;
	}

	if ( pIspDmaState->pstCCIC != NULL )
	{
		if ( pIspDmaState->pstCCIC->fd > 0 )
		{
			mc_close( pIspDmaState->pstCCIC->fd );
			pIspDmaState->pstCCIC->fd = -1;
		}
	}
	if ( pIspDmaState->pstSensor != NULL )
	{
		if ( pIspDmaState->pstSensor->fd > 0 )
		{
			mc_close( pIspDmaState->pstSensor->fd );
			pIspDmaState->pstSensor->fd = -1;
			iIspCameraFd                = -1;
			iIspCCICFd                  = -1;
		}
	}
	if ( pIspDmaState->pstCCICRaw != NULL )
	{
		if ( pIspDmaState->pstCCICRaw->fd > 0 )
		{
			mc_close( pIspDmaState->pstCCICRaw->fd );
			pIspDmaState->pstCCICRaw->fd = -1;
		}
	}
	if ( pIspDmaState->pstIsp != NULL )
	{
		if ( pIspDmaState->pstIsp->fd > 0 )
		{
			mc_close( pIspDmaState->pstIsp->fd );
			pIspDmaState->pstIsp->fd = -1;
		}
	}
	if ( pIspDmaState->pstIspIn != NULL )
	{
		if ( pIspDmaState->pstIspIn->fd > 0 )
		{
			mc_close( pIspDmaState->pstIspIn->fd );
			pIspDmaState->pstIspIn->fd = -1;
		}
	}
	if ( pIspDmaState->pstIspCodec != NULL )
	{
		if ( pIspDmaState->pstIspCodec->fd > 0 )
		{
			mc_close( pIspDmaState->pstIspCodec->fd );
			pIspDmaState->pstIspCodec->fd = -1;
		}
	}
	if ( pIspDmaState->pstIspDisp != NULL )
	{
		if ( pIspDmaState->pstIspDisp->fd > 0 )
		{
			mc_close( pIspDmaState->pstIspDisp->fd );
			pIspDmaState->pstIspDisp->fd = -1;
		}
	}
	if ( pIspDmaState->pstMedia )
	{
		media_close( pIspDmaState->pstMedia );
		pIspDmaState->pstMedia = NULL;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _open_mc_drv_res( _CAM_MC_IspDmaState *pIspDmaState )
{
	_CAM_MediaEntity    *pstEntity = NULL;
	CAM_Int32u          i;

	// Open media controller
	pIspDmaState->pstMedia = media_open( "/dev/media0" );
	if ( NULL == pIspDmaState->pstMedia )
	{
		TRACE( CAM_ERROR, "Error: failed to open media controller( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	// Open fd for each node
	pstEntity  = pIspDmaState->pstMedia->astEntities;
	for ( i = 0; i < pIspDmaState->pstMedia->uiEntityCount; i++ )
	{
		if ( strcmp( pstEntity->stInfo.name, "pxaccic" ) == 0 )
		{
			if ( pIspDmaState->pstCCIC == NULL )
			{
				pIspDmaState->pstCCIC = pstEntity;
				pIspDmaState->pstCCIC->fd = mc_open( pIspDmaState->pstCCIC->sDevName, O_RDWR );

				if ( pIspDmaState->pstCCIC->fd < 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to open ccic, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					_close_mc_drv_res( pIspDmaState );
					return CAM_ERROR_DRIVEROPFAILED;
				}
				iIspCCICFd = pIspDmaState->pstCCIC->fd;

				TRACE( CAM_INFO, "Info: open CCIC: %s success, fd[%d] ( %s, %s, %d )\n",
				       pIspDmaState->pstCCIC->sDevName, pIspDmaState->pstCCIC->fd, __FILE__, __FUNCTION__, __LINE__ );
			}
		}
		if ( strcmp( pstEntity->stInfo.name, "mvisp dma_ccic1 output" ) == 0 )
		{
			if ( pIspDmaState->pstCCICRaw == NULL )
			{
				pIspDmaState->pstCCICRaw = pstEntity;
				pIspDmaState->pstCCICRaw->fd = mc_open( pIspDmaState->pstCCICRaw->sDevName, O_RDWR );
				if ( pIspDmaState->pstCCICRaw->fd < 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to open ccic_video, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					_close_mc_drv_res( pIspDmaState );
					return CAM_ERROR_DRIVEROPFAILED;
				}

				TRACE( CAM_INFO, "Info: open CCIC RAW: %s success, fd[%d] ( %s, %s, %d )\n",
				       pIspDmaState->pstCCICRaw->sDevName, pIspDmaState->pstCCICRaw->fd, __FILE__, __FUNCTION__, __LINE__ );
			}
		}
		if ( strcmp( pstEntity->stInfo.name, "mvisp_ispdma" ) == 0 )
		{
			if ( pIspDmaState->pstIsp == NULL )
			{
				pIspDmaState->pstIsp     = pstEntity;
				pIspDmaState->pstIsp->fd = mc_open( pIspDmaState->pstIsp->sDevName, O_RDWR );
				if ( pIspDmaState->pstIsp->fd < 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to open isp, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					_close_mc_drv_res( pIspDmaState );
					return CAM_ERROR_DRIVEROPFAILED;
				}

				TRACE( CAM_INFO, "Info: open ISP: %s success, fd[%d] ( %s, %s, %d )\n",
				       pIspDmaState->pstIsp->sDevName, pIspDmaState->pstIsp->fd, __FILE__, __FUNCTION__, __LINE__ );
			}
		}
		if ( strcmp( pstEntity->stInfo.name, "mvisp dma_input input" ) == 0 )
		{
			if ( pIspDmaState->pstIspIn == NULL )
			{
				pIspDmaState->pstIspIn = pstEntity;
			}
		}
		if ( strcmp( pstEntity->stInfo.name, "mvisp dma_codec output" ) == 0 )
		{
			if ( pIspDmaState->pstIspCodec == NULL )
			{
				pIspDmaState->pstIspCodec     = pstEntity;
				pIspDmaState->pstIspCodec->fd = mc_open( pIspDmaState->pstIspCodec->sDevName, O_RDWR );
				if ( pIspDmaState->pstIspCodec->fd < 0 )
				{
					TRACE( CAM_ERROR, "Error:failed to open isp video codec, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					_close_mc_drv_res( pIspDmaState );
					return CAM_ERROR_DRIVEROPFAILED;
				}
			}

			TRACE( CAM_INFO, "Info: open ISP Codec: %s success, fd[%d] ( %s, %s, %d )\n",
				       pIspDmaState->pstIspCodec->sDevName, pIspDmaState->pstIspCodec->fd, __FILE__, __FUNCTION__, __LINE__ );
		}
		if ( strcmp( pstEntity->stInfo.name, "mvisp dma_display output" ) == 0 )
		{
			if ( pIspDmaState->pstIspDisp == NULL )
			{
				pIspDmaState->pstIspDisp = pstEntity;
				pIspDmaState->pstIspDisp->fd = mc_open( pIspDmaState->pstIspDisp->sDevName, O_RDWR );
				if ( pIspDmaState->pstIspDisp->fd < 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to open isp video disp, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					_close_mc_drv_res( pIspDmaState );
					return CAM_ERROR_DRIVEROPFAILED;
				}
			}

			TRACE( CAM_INFO, "Info: open ISP Disp: %s success, fd[%d] ( %s, %s, %d )\n",
				       pIspDmaState->pstIspDisp->sDevName, pIspDmaState->pstIspDisp->fd, __FILE__, __FUNCTION__, __LINE__ );
		}

		pstEntity++;
	}

	if ( pIspDmaState->pstIsp->fd >= 0 )
	{
		pIspDmaState->pucIPCAddr = (CAM_Int8u*)mmap( 0, pucIPCAddr_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pIspDmaState->pstIsp->fd, 0 );
		if ( pIspDmaState->pucIPCAddr == NULL )
		{
			TRACE( CAM_ERROR, "Error: can't map ipc registers, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			_close_mc_drv_res( pIspDmaState );
			return CAM_ERROR_DRIVEROPFAILED;
		}
		pucIPCAddr = (CAM_Int8u*)pIspDmaState->pucIPCAddr;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: isp fd is not open( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_FATALERROR;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _ispdma_mc_close( _CAM_MC_IspDmaState *pIspDmaState )
{
	CAM_Int32s  ret;
	CAM_Error   error;

	if ( pIspDmaState == NULL )
	{
		return CAM_ERROR_NONE;
	}

	if ( NULL == pIspDmaState->pstMedia )
	{
		return CAM_ERROR_NONE;
	}

	if ( pIspDmaState->pstMedia->fd < 0 )
	{
		TRACE( CAM_INFO, "Info: soc-isp-dma is not opened yet( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NONE;
	}

	error = _free_mc_links( pIspDmaState );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to free links( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	error = _close_mc_drv_res( pIspDmaState );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to close media controller driver( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	return CAM_ERROR_NONE;
}


static CAM_Error _ispdma_mc_open( _CAM_MC_IspDmaState *pIspDmaState )
{
	CAM_Error error;

	_CHECK_BAD_POINTER( pIspDmaState );

	error = _open_mc_drv_res( pIspDmaState );
	if ( CAM_ERROR_NONE != error )
	{
		_close_mc_drv_res( pIspDmaState );
		TRACE( CAM_ERROR, "Error: failed to open media controller( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	// Step up links
	error = _set_mc_links( pIspDmaState );
	if ( CAM_ERROR_NONE != error )
	{
		_ispdma_mc_close( pIspDmaState );
		TRACE( CAM_ERROR, "Error: failed to set media controller links( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	return CAM_ERROR_NONE;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////          EXTERNAL APIs           //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

CAM_Error ispdma_get_tick( void *hIspDmaHandle, CAM_Tick *ptTick )
{
	_CAM_MC_IspDmaState         *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	struct v4l2_ispdma_timeinfo stIspTick;
	CAM_Int32s                  ret;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( ptTick );

	stIspTick.sec   = 0;
	stIspTick.usec  = 0;
	stIspTick.delta = 0;
	ret = ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_ISPDMA_GETDELTA, &stIspTick );

	if ( ret < 0 )
	{
		*ptTick = 0;
		TRACE( CAM_ERROR, "Error: failed to get isp timer[%s]( %s, %s, %d )\n",
		       strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return  CAM_ERROR_DRIVEROPFAILED;
	}

	*ptTick = stIspTick.sec * 1000000 + stIspTick.usec;

	return CAM_ERROR_NONE;
}

inline CAM_Error ispdma_poll_ipc( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Tick *ptTick )
{
	_CAM_MC_IspDmaState         *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	CAM_Int32s                  ret;
	struct v4l2_dxoipc_ipcwait  wait;
	CAM_Tick                    tick;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( ptTick );

	wait.tickinfo.sec      = 0;
	wait.tickinfo.usec     = 0;
	wait.tickinfo.delta    = 0;
	wait.timeout           = iTimeout;
	ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_DXOIPC_WAIT_IPC, &wait );

	if ( ret < 0 )
	{
		*ptTick = 0;
		TRACE( CAM_WARN, "Warning: IPC event polling timeout[%d ms], driver buffer count[disp:%d, codec:%d]( %s, %s, %d )\n",
		       wait.timeout, pIspDmaState->iPortBufNum[0], pIspDmaState->iPortBufNum[1], __FILE__, __FUNCTION__, __LINE__ );
		return  CAM_ERROR_TIMEOUT;
	}

	*ptTick = wait.tickinfo.sec * 1000000 + wait.tickinfo.usec;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_poll_buffer( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Int32s iRequest, CAM_Int32s *piResult )
{
	_CAM_MC_IspDmaState         *pIspDmaState   = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	CAM_Int32s                  ret;
	struct pollfd               astPollBufFd[2];
	CAM_Bool                    abPortEnabled[2] = {CAM_FALSE, CAM_FALSE};
	CAM_Int32s                  iPortCnt        = 0;
	CAM_Int32s                  iResult         = 0;
	CAM_Int32s                  iDispPortIdx = -1, iCodecPortIdx = -1;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( piResult );

	// FIXME: we can not output display buffer when raw data dump on MMP3 due to HW limitation
	if ( (iRequest & ISP_PORT_DISPLAY) && (!pIspDmaState->bIsCCICRawEnabled) )
	{
		astPollBufFd[iPortCnt].fd        = pIspDmaState->pstIspDisp->fd;
		astPollBufFd[iPortCnt].events    = POLLIN;
		astPollBufFd[iPortCnt].revents   = 0;
		iDispPortIdx = iPortCnt;
		iPortCnt++;
		abPortEnabled[0] = pIspDmaState->stCfgOnOff.enable_disp;
	}
	if ( iRequest & ISP_PORT_CODEC )
	{
		if ( !_is_bayer_format( pIspDmaState->aePortFormat[1] ) )
		{
			astPollBufFd[iPortCnt].fd        = pIspDmaState->pstIspCodec->fd;
			abPortEnabled[1] = pIspDmaState->stCfgOnOff.enable_codec;
		}
		else
		{
			astPollBufFd[iPortCnt].fd        = pIspDmaState->pstCCICRaw->fd;
			abPortEnabled[1] = pIspDmaState->bIsCCICRawEnabled;
		}
		astPollBufFd[iPortCnt].events    = POLLIN;
		astPollBufFd[iPortCnt].revents   = 0;
		iCodecPortIdx = iPortCnt;
		iPortCnt++;
	}

	if ( iPortCnt == 0 )
	{
		*piResult = 0;
		// FIXME: Hack because in MMP3 A1, we can not output both display and CCIC raw port
		return (iRequest & ISP_PORT_DISPLAY) ? CAM_ERROR_NONE : CAM_ERROR_BADARGUMENT;
	}

	ret = mc_poll( astPollBufFd, iPortCnt, iTimeout );

	if ( ret == 0 )
	{
		*piResult = 0;
		TRACE( CAM_WARN, "Warning: buffer status[disp:%d, codec:%d], port status[disp:%d, codec:%d], buffer polling timeout[%d ms]( %s, %s, %d )\n",
		       pIspDmaState->iPortBufNum[0], pIspDmaState->iPortBufNum[1], abPortEnabled[0], abPortEnabled[1],
		       iTimeout, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_TIMEOUT;
	}
	else if ( ret < 0 )
	{
		*piResult = 0;
		TRACE( CAM_ERROR, "Error: poll buffer failed[%s], iTimeOut[%d] ( %s, %s, %d )\n", strerror( errno ), iTimeout, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	if ( (iRequest & ISP_PORT_DISPLAY) && (iDispPortIdx >= 0) && ((astPollBufFd[iDispPortIdx].revents & POLLIN) != 0) )
	{
		iResult |= ISP_PORT_DISPLAY;
	}
	if ( (iRequest & ISP_PORT_CODEC)   && (iCodecPortIdx >= 0) && ((astPollBufFd[iCodecPortIdx].revents & POLLIN) != 0) )
	{
		iResult |= ISP_PORT_CODEC;
	}

	if ( iResult == 0 )
	{
		CAM_Int32s i = 0;

		for ( i = 0; i < iPortCnt; i++ )
		{
			TRACE( CAM_WARN, "Warning: poll port[%d] buffer failed with revents[%d], please check if stream on fd[%d] before or buffer enqueued ( %s, %s, %d )\n",
			       iRequest, astPollBufFd[i].revents, astPollBufFd[i].fd, __FILE__, __FUNCTION__, __LINE__ );
		}
	}

	*piResult = iResult;

	return CAM_ERROR_NONE;
}


CAM_Error ispdma_init( void **phIspDmaHandle )
{
	_CAM_MC_IspDmaState         *pIspDmaState = NULL;
	enum v4l2_buf_type          eBufType;
	CAM_Int32s                  ret;
	CAM_Error                   error = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( phIspDmaHandle );


	// Alloc memory and initialize
	pIspDmaState = (_CAM_MC_IspDmaState*)malloc( sizeof( _CAM_MC_IspDmaState ) );
	if ( pIspDmaState == NULL )
	{
		TRACE( CAM_ERROR, "Error: no enough memory to create soc-isp-dma devices( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pIspDmaState, 0, sizeof( _CAM_MC_IspDmaState ) );

	error = _ispdma_mc_open( pIspDmaState );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to open media controller( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	*phIspDmaHandle = pIspDmaState;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_deinit( void **phIspDmaHandle )
{
	_CAM_MC_IspDmaState *pIspDmaState = (_CAM_MC_IspDmaState*)(*phIspDmaHandle);
	CAM_Int32s          ret           = 0;
	CAM_Error           error         = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( phIspDmaHandle );

	error = _ispdma_mc_close( pIspDmaState );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to close media controller( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	free( *phIspDmaHandle );
	*phIspDmaHandle = NULL;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_reset( void *hIspDmaHandle )
{
	CAM_Int32s               ret           = 0;
	_CAM_MC_IspDmaState      *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	struct v4l2_ispdma_reset stResetParam;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_ISPDMA_RESET, &stResetParam );

	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error:failed to reset ispdma, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

// isp-dma configuration
CAM_Error ispdma_enum_sensor( CAM_Int32s *piSensorCount, CAM_DeviceProp astCameraProp[CAM_MAX_SUPPORT_CAMERA] )
{
	CAM_Int32u          i, j;
	CAM_Int32s          ret;
	CAM_Int32s          fd = -1;
	_CAM_MediaEntity    *pEntity    = NULL;
	_CAM_MediaEntity    astEntities[MAX_ENTITY_COUNT];
	CAM_Int32u          uiEntityCount;
	CAM_Int32s          iNumSensor = 0;

	fd = mc_open( "/dev/media0", O_RDWR );
	if ( fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: can't open media device[/dev/media0]( %s, %s, %d )\n", __FILE__, __FUNCTION__,__LINE__ );
		*piSensorCount = 0;
		return CAM_ERROR_NONE;
	}

	ret = media_open_entities( fd, astEntities, MAX_ENTITY_COUNT, &uiEntityCount );
	if ( ret < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to enumerate entities for device[/dev/media0]( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		*piSensorCount = 0;
		return CAM_ERROR_DRIVEROPFAILED;
	}

	// search sensors
	pEntity = astEntities;
	for ( i = 0; i < uiEntityCount; i++ )
	{
		for ( j = 0; j < MAX_SUPPORT_SENSOR_NUM; j++ )
		{
			if ( strncmp( pEntity->stInfo.name, gSensorSupported[j].stSensorProp.sName, 6 ) == 0 )
			{
				astCameraProp[iNumSensor]       = gSensorSupported[j].stSensorProp;
				gSensorSupported[j].iSensorID   = iNumSensor++;
				TRACE( CAM_INFO, "Info: raw sensor[%s] found\n", gSensorSupported[j].stSensorProp.sName );
			}
		}
		pEntity++;
	}

	media_close_entities( astEntities, uiEntityCount );

	mc_close( fd );

	*piSensorCount  = iNumSensor;
	TRACE( CAM_INFO, "Info: %d raw sensor found\n", iNumSensor );

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_select_sensor( void *hIspDmaHandle, CAM_Int32s iSensorID )
{
	CAM_Int32u          i               = 0;
	CAM_Int32s          ret             = 0;
	CAM_Error           error           = CAM_ERROR_NONE;
	_CAM_MC_IspDmaState *pIspDmaState   = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	_CAM_SensorDesc     *pSensor        = NULL;
	_CAM_MediaEntity    *pEntity        = NULL;

	_CHECK_BAD_POINTER( hIspDmaHandle );

	for ( i = 0; i < MAX_SUPPORT_SENSOR_NUM; i++ )
	{
		if ( gSensorSupported[i].iSensorID == iSensorID )
		{
			pSensor = &gSensorSupported[i];
			break;
		}
	}
	if ( i == MAX_SUPPORT_SENSOR_NUM )
	{
		TRACE( CAM_ERROR, "Error: failed to search selected raw sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_FATALERROR;
	}

	// Find the specified sensor
	pEntity  = pIspDmaState->pstMedia->astEntities;
	for ( i = 0; i < pIspDmaState->pstMedia->uiEntityCount; i++ )
	{
		if ( strncmp( pEntity->stInfo.name, pSensor->stSensorProp.sName, 6 )  == 0 )
		{
			break;
		}
		pEntity++;
	}
	if ( i >= pIspDmaState->pstMedia->uiEntityCount )
	{
		TRACE( CAM_ERROR, "Error: failed to find specified sensor[%s]( %s, %s, %d )\n",
		       pSensor->stSensorProp.sName, __FILE__, __FUNCTION__, __LINE__ );
		_ispdma_mc_close( pIspDmaState );
		return CAM_ERROR_FATALERROR;
	}

	pIspDmaState->pstSensor        = pEntity;
	pIspDmaState->pstSensor->fd    = mc_open( pIspDmaState->pstSensor->sDevName, O_RDWR );
	if ( pIspDmaState->pstSensor->fd < 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to open sensor, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		_ispdma_mc_close( pIspDmaState );
		return CAM_ERROR_DRIVEROPFAILED;
	}
	iIspCameraFd = pIspDmaState->pstSensor->fd;

	error = _set_links_by_pad( pIspDmaState, pIspDmaState->pstCCIC, &pIspDmaState->pstSensor->astPads[0], &pIspDmaState->pstCCIC->astPads[0] );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error:failed to set link( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		_ispdma_mc_close( pIspDmaState );
		return error;
	}

	error = _config_pipeline( pIspDmaState, pSensor->iMaxWidth, pSensor->iMaxHeight );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: failed to set width * height[%d * %d]( %s, %s, %d )\n", pSensor->iMaxWidth, pSensor->iMaxHeight, __FILE__, __FUNCTION__, __LINE__ );
		_ispdma_mc_close( pIspDmaState );
		return error;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_output_format( void *hIspDmaHandle, _CAM_IspDmaFormat *pDmaFormat, _CAM_IspPort ePortID )
{
	CAM_Int32s                 i             = 0;
	CAM_Int32s                 ret           = 0;
	_CAM_MC_IspDmaState        *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	struct v4l2_format         *pstV4L2Fmt   = NULL;
	CAM_Int32s                 aiStep[3]     = {0}, aiAlign[3] = {8, 0, 0};
	CAM_Int32s                 fd            = -1;
	struct v4l2_requestbuffers stReqBufs;
	CAM_Error                  error;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( pDmaFormat );

	switch ( ePortID )
	{
	case ISP_PORT_DISPLAY:
		pstV4L2Fmt = &pIspDmaState->stCfgDispFmt;
		fd = pIspDmaState->pstIspDisp->fd;
		pIspDmaState->aePortFormat[0] = pDmaFormat->eFormat;
		break;
	case ISP_PORT_CODEC:
		if ( !_is_bayer_format( pDmaFormat->eFormat ) )
		{
			pstV4L2Fmt = &pIspDmaState->stCfgCodec.stCodecFmt;
			fd = pIspDmaState->pstIspCodec->fd;
		}
		else
		{
			pstV4L2Fmt = &pIspDmaState->stCfgCCICRaw;
			fd = pIspDmaState->pstCCICRaw->fd;
		}
		pIspDmaState->aePortFormat[1] = pDmaFormat->eFormat;
		break;
	default:
		TRACE( CAM_ERROR, "Error: unknown port id[%d]( %s, %s, %d )\n", ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	// input:  V4L2_BUF_TYPE_VIDEO_OUTPUT
	// output: V4L2_BUF_TYPE_VIDEO_CAPTURE
	pstV4L2Fmt->type                 = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	pstV4L2Fmt->fmt.pix.width        = pDmaFormat->iWidth;
	pstV4L2Fmt->fmt.pix.height       = pDmaFormat->iHeight;
	error = _ce2driver_format( pDmaFormat->eFormat, &pstV4L2Fmt->fmt.pix.pixelformat );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: format mapping failed( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}
	pstV4L2Fmt->fmt.pix.colorspace   =  V4L2_COLORSPACE_JPEG;
	_calcstep( pDmaFormat->eFormat, pDmaFormat->iWidth, aiAlign, aiStep );
	pstV4L2Fmt->fmt.pix.bytesperline = aiStep[0];
	pstV4L2Fmt->fmt.pix.sizeimage    = aiStep[0] * pDmaFormat->iHeight;

	TRACE( CAM_INFO, "set up [%d] port, width: %d, height: %d, format: %d\n",
	           ePortID, pstV4L2Fmt->fmt.pix.width, pstV4L2Fmt->fmt.pix.height, pstV4L2Fmt->fmt.pix.pixelformat );

	ret = mc_ioctl( fd, VIDIOC_S_FMT, pstV4L2Fmt );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to config [%d] port, error code[%s]( %s, %s, %d )\n", ePortID, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	memset( &stReqBufs, 0x00, sizeof( struct v4l2_requestbuffers ) );
	stReqBufs.count   = 0;
	stReqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stReqBufs.memory  = V4L2_MEMORY_USERPTR;
	ret = mc_ioctl( fd,  VIDIOC_REQBUFS, &stReqBufs );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to req buffers, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	stReqBufs.count   = CAM_MAX_PORT_BUF_CNT;
	stReqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stReqBufs.memory  = V4L2_MEMORY_USERPTR;
	ret = mc_ioctl( fd,  VIDIOC_REQBUFS, &stReqBufs );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to req buffers, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_codec_property( void *hIspDmaHandle, _CAM_IspDmaProperty *pDmaProperty )
{
	CAM_Int32s                 ret           = 0;
	_CAM_MC_IspDmaState        *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	_CHECK_BAD_POINTER( pDmaProperty );

	pIspDmaState->stCfgCodec.stCodecProp.dma_burst_size   = 256;
	pIspDmaState->stCfgCodec.stCodecProp.vbnum            = pDmaProperty->iBandCnt;
	pIspDmaState->stCfgCodec.stCodecProp.vbsize           = pDmaProperty->iBandSize * 2;

	TRACE( CAM_INFO, "set up codec dma property, burst: %d\n codec dma band number: %d, band size: %d\n",
	       pIspDmaState->stCfgCodec.stCodecProp.dma_burst_size,
	       pIspDmaState->stCfgCodec.stCodecProp.vbnum,
	       pIspDmaState->stCfgCodec.stCodecProp.vbsize );

	ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_DXOIPC_CONFIG_CODEC, &pIspDmaState->stCfgCodec.stCodecProp );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to config codec property, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_set_framebuffer( void *hIspDmaHandle, _CAM_IspDmaBufNode astFBuf[NB_FRAME_BUFFER], CAM_Int32s iBandCnt )
{
	_CHECK_BAD_POINTER( hIspDmaHandle );

	CAM_Int32s          i             = 0;
	CAM_Int32s          ret           = 0;
	CAM_Bool            bStreamOn     = ( iBandCnt ) > 0 ? CAM_TRUE : CAM_FALSE;
	_CAM_MC_IspDmaState *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;


	if ( bStreamOn )
	{
		pIspDmaState->stCfgFB.burst_read  = 64;
		pIspDmaState->stCfgFB.burst_write = 256;
		pIspDmaState->stCfgFB.fbcnt       = iBandCnt;
		for ( i = 0; i < iBandCnt; i++ )
		{
			pIspDmaState->stCfgFB.virAddr[i] = astFBuf[i].pVirtAddr;
			pIspDmaState->stCfgFB.phyAddr[i] = astFBuf[i].uiPhyAddr;
			pIspDmaState->stCfgFB.size[i]    = astFBuf[i].iBufLen;

			TRACE( CAM_INFO, "Info: fb dma NO.: %d, size: %d\n", i, pIspDmaState->stCfgFB.size[i] );
		}
		ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_DXOIPC_SET_FB, &pIspDmaState->stCfgFB );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to set frame buffer, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}
	}

	pIspDmaState->stCfgOnOff.enable_fbrx = pIspDmaState->stCfgOnOff.enable_fbtx = bStreamOn;
	ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_DXOIPC_SET_STREAM, &pIspDmaState->stCfgOnOff );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to set stream, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_DRIVEROPFAILED;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_sensor_streamon( void *hIspDmaHandle, CAM_Bool bOn )
{
	_CHECK_BAD_POINTER( hIspDmaHandle );

	/* Do nothing */

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_isp_streamon( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bOn )
{
	CAM_Int32s          ret           = 0;
	_CAM_MC_IspDmaState *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	CAM_Int32s          iCmd          = bOn ? VIDIOC_STREAMON : VIDIOC_STREAMOFF;
	enum v4l2_buf_type  eBufType      = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	if ( ePortID != ISP_PORT_DISPLAY && ePortID != ISP_PORT_CODEC && ePortID != ISP_PORT_BOTH )
	{
		TRACE( CAM_ERROR, "Error: can not support port id[%d]( %s, %s, %d )\n", ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	if ( ePortID & ISP_PORT_DISPLAY )
	{
		if ( bOn != pIspDmaState->stCfgOnOff.enable_disp )
		{
			ret = mc_ioctl( pIspDmaState->pstIspDisp->fd, iCmd, &eBufType );
			if ( ret != 0 )
			{
				TRACE( CAM_ERROR, "Error: failed to stream [%s] display port, error code[%s]( %s, %s, %d )\n",
				       bOn ? "On" : "Off", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_DRIVEROPFAILED;
			}

			pIspDmaState->stCfgOnOff.enable_disp = bOn;
		}
	}

	if ( ePortID & ISP_PORT_CODEC )
	{
		if ( !_is_bayer_format( pIspDmaState->aePortFormat[1] ) )
		{
			if ( bOn != pIspDmaState->stCfgOnOff.enable_codec )
			{
				ret = mc_ioctl( pIspDmaState->pstIspCodec->fd, iCmd, &eBufType );
				if ( ret != 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to stream [%s] codec port, error code[%s]( %s, %s, %d )\n",
					      bOn ? "On" : "Off", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_DRIVEROPFAILED;
				}

				pIspDmaState->stCfgOnOff.enable_codec = bOn;
			}
		}
		else
		{
			if ( bOn != pIspDmaState->bIsCCICRawEnabled )
			{

				ret = mc_ioctl( pIspDmaState->pstCCICRaw->fd, iCmd, &eBufType );
				if ( ret != 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to stream [%s] CCIC raw dump port, error code[%s]( %s, %s, %d )\n",
					       bOn ? "On" : "Off", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_DRIVEROPFAILED;
				}

				pIspDmaState->bIsCCICRawEnabled = bOn;
			}
		}
	}

	return CAM_ERROR_NONE;
}


CAM_Error ispdma_isp_set_mode( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bIsStreaming )
{
	CAM_Int32s                      ret           = 0;
	_CAM_MC_IspDmaState             *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	struct v4l2_ispdma_capture_mode astCapMode[2];
	CAM_Int32s                      iNum = 0, i;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	if ( ePortID != ISP_PORT_DISPLAY && ePortID != ISP_PORT_CODEC && ePortID != ISP_PORT_BOTH )
	{
		TRACE( CAM_ERROR, "Error: can not support port id[%d]( %s, %s, %d )\n", ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	if ( ePortID & ISP_PORT_DISPLAY )
	{
		astCapMode[iNum].port = ISPDMA_PORT_DISPLAY;
		astCapMode[iNum].mode = bIsStreaming ? ISPVIDEO_NORMAL_CAPTURE : ISPVIDEO_STILL_CAPTURE;
		iNum++;
	}

	if ( ePortID & ISP_PORT_CODEC )
	{
		astCapMode[iNum].port = ISPDMA_PORT_CODEC;
		astCapMode[iNum].mode = bIsStreaming ? ISPVIDEO_NORMAL_CAPTURE : ISPVIDEO_STILL_CAPTURE;
		iNum++;
	}

	for ( i = 0; i < iNum; i++ )
	{
		ret = mc_ioctl( pIspDmaState->pstIsp->fd, VIDIOC_PRIVATE_ISPDMA_CAPTURE_MODE, &astCapMode[i] );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: failed to set capture mode for port[%d], error code[%s]( %s, %s, %d )\n",
				astCapMode[i].port, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}

		TRACE( CAM_INFO, "Info: set_mode:port %d, mode: %d\n", astCapMode[i].port, astCapMode[i].mode );
	}

	return CAM_ERROR_NONE;
}


CAM_Error ispdma_enqueue_buffer( void *hIspDmaHandle, CAM_ImageBuffer *pstImgBuf, _CAM_IspPort ePortID )
{
	CAM_Int32s                  ret = 0;
	_CAM_MC_IspDmaState         *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	CAM_Int32s                  fd = -1;
	CAM_Int32s                  i;
	CAM_Int32s                  iPortIdx;
	CAM_Tick                    tick;
	_CAM_MCBufDesc              *pstMCBufDesc = NULL;

	_CHECK_BAD_POINTER( ((void *)(hIspDmaHandle && pstImgBuf)) );

	switch ( ePortID )
	{
	case ISP_PORT_DISPLAY:
		fd       = pIspDmaState->pstIspDisp->fd;
		iPortIdx = 0;
		break;
	case ISP_PORT_CODEC:
		if ( !_is_bayer_format( pIspDmaState->aePortFormat[1] ) )
		{
			fd = pIspDmaState->pstIspCodec->fd;
		}
		else
		{
			fd = pIspDmaState->pstCCICRaw->fd;
		}
		iPortIdx = 1;
		break;
	default:
		TRACE( CAM_ERROR, "Error: can not support port_id[%d] ( %s, %s, %d )", ePortID,
			__FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	// find a slot for enqueue
	for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
	{
		if ( pIspDmaState->astBufPool[iPortIdx][i].pImgBuf == pstImgBuf )
		{
			pstMCBufDesc = &pIspDmaState->astBufPool[iPortIdx][i];
			break;
		}
	}

	if ( i >= CAM_MAX_PORT_BUF_CNT )
	{
		if ( pIspDmaState->iMCBufDescCount[iPortIdx] < CAM_MAX_PORT_BUF_CNT )
		{
			// check back to back buffer layout
			if ( _is_format_rawandplanar( pstImgBuf->eFormat ) )
			{
				CAM_Int32s j = 1;
				while ( ( pstImgBuf->iAllocLen[j] > 0 ) && ( j < 3 ) )
				{
					if ( pstImgBuf->pBuffer[j] + pstImgBuf->iOffset[j] != pstImgBuf->pBuffer[j - 1] + pstImgBuf->iOffset[j - 1] + pstImgBuf->iAllocLen[j - 1] )
					{
						TRACE( CAM_ERROR, "Error: requires \"back to back\" buffer layout when the input is raw planar( %s, %s, %d )\n",
						       __FILE__, __FUNCTION__, __LINE__ );
						TRACE( CAM_ERROR, "pBuffer = {%p, %p, %p}, iOffset = {%d, %d, %d}, iAllocLen = {%d, %d, %d}\n",\
						       pstImgBuf->pBuffer[0], pstImgBuf->pBuffer[1], pstImgBuf->pBuffer[2],\
						       pstImgBuf->iOffset[0], pstImgBuf->iOffset[1], pstImgBuf->iOffset[2],\
						       pstImgBuf->iAllocLen[0], pstImgBuf->iAllocLen[1], pstImgBuf->iAllocLen[2] );
						return CAM_ERROR_BADBUFFER;
					}
					j++;
				}
			}

			pstMCBufDesc = &pIspDmaState->astBufPool[iPortIdx][pIspDmaState->iMCBufDescCount[iPortIdx]];

			// this is a new image buffer, we need allocate an entity to record its information
			memset( pstMCBufDesc, 0, sizeof( _CAM_MCBufDesc ) );
			pstMCBufDesc->pImgBuf                = pstImgBuf;
			pstMCBufDesc->stV4L2Buf.memory       = V4L2_MEMORY_USERPTR;
			pstMCBufDesc->stV4L2Buf.type         = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			pstMCBufDesc->stV4L2Buf.m.userptr    = (unsigned long)pstImgBuf->pBuffer[0] + pstImgBuf->iOffset[0];
			pstMCBufDesc->stV4L2Buf.length       = pstImgBuf->iAllocLen[0] + pstImgBuf->iAllocLen[1] + pstImgBuf->iAllocLen[2];
			pstMCBufDesc->stV4L2Buf.index        = pIspDmaState->iMCBufDescCount[iPortIdx];
			pIspDmaState->iMCBufDescCount[iPortIdx]++;
		}
		else
		{
			TRACE( CAM_ERROR, "Error: not enough entires for to MC buffer descriptor( %s, %s, %d )\n",
			       __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}
	}

	if ( pstMCBufDesc->iFlag )
	{
		TRACE( CAM_ERROR, "Error: enqueue buffer[%p] twice( %s, %s, %d )\n",
		       pstImgBuf, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NONE;
	}

	ret = mc_ioctl( fd, VIDIOC_QBUF, &pstMCBufDesc->stV4L2Buf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: failed to enqueue image buffer[%p] to fd[%d], error code[%s]( %s, %s, %d )\n",
		       pstImgBuf, fd, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );

		TRACE( CAM_ERROR, "pstMCBufDesc->stV4L2Buf.type      = %d\n", pstMCBufDesc->stV4L2Buf.type );
		TRACE( CAM_ERROR, "pstMCBufDesc->stV4L2Buf.memory    = %d\n", pstMCBufDesc->stV4L2Buf.memory );
		TRACE( CAM_ERROR, "pstMCBufDesc->stV4L2Buf.index     = %d\n", pstMCBufDesc->stV4L2Buf.index );
		TRACE( CAM_ERROR, "pstMCBufDesc->stV4L2Buf.m.userptr = %p\n", (void*)pstMCBufDesc->stV4L2Buf.m.userptr );
		TRACE( CAM_ERROR, "pstMCBufDesc->stV4L2Buf.length    = %d\n", pstMCBufDesc->stV4L2Buf.length );

		return CAM_ERROR_DRIVEROPFAILED;
	}

	pstMCBufDesc->iFlag = 1;
	pIspDmaState->iPortBufNum[iPortIdx]++;

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_dequeue_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_ImageBuffer **ppImgBuf, CAM_Bool *pbIsError )
{
	CAM_Int32s                  ret           = 0;
	_CAM_MC_IspDmaState         *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	CAM_Error                   error         = CAM_ERROR_NONE;
	CAM_Int32s                  fd            = -1;
	struct v4l2_buffer          stV4L2Buf;
	CAM_Int32s                  iPortIdx;
	CAM_Tick                    tick;
	CAM_Bool                    bIsPortActivated = CAM_TRUE;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	if ( ePortID != ISP_PORT_DISPLAY && ePortID != ISP_PORT_CODEC )
	{
		TRACE( CAM_ERROR, "Error: can not support port id[%d]( %s, %s, %d )\n",
		       ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	switch ( ePortID )
	{
	case ISP_PORT_DISPLAY:
		fd = pIspDmaState->pstIspDisp->fd;
		iPortIdx = 0;
		bIsPortActivated = pIspDmaState->stCfgOnOff.enable_disp;
		break;

	case ISP_PORT_CODEC:
		if ( !_is_bayer_format( pIspDmaState->aePortFormat[1] ) )
		{
			fd = pIspDmaState->pstIspCodec->fd;
			bIsPortActivated = pIspDmaState->stCfgOnOff.enable_codec;
		}
		else
		{
			fd = pIspDmaState->pstCCICRaw->fd;
			bIsPortActivated = pIspDmaState->bIsCCICRawEnabled;
		}
		iPortIdx = 1;
		break;

	default:
		TRACE( CAM_ERROR, "Error: can not support port id[%d] ( %s, %s, %d )", ePortID,
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	if ( bIsPortActivated != CAM_TRUE )
	{
		TRACE( CAM_WARN, "Warning: try to dequeue buffer from inactive port[%d] ( %s, %s, %d )\n",
		       ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTACTIVE;
	}

	memset( &stV4L2Buf, 0, sizeof( struct v4l2_buffer ) );
	stV4L2Buf.memory       = V4L2_MEMORY_USERPTR;
	stV4L2Buf.type         = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = mc_ioctl( fd, VIDIOC_DQBUF, &stV4L2Buf );
	if ( 0 == ret )
	{
		if ( stV4L2Buf.index >= (CAM_Int32u)pIspDmaState->iMCBufDescCount[iPortIdx] )
		{
			TRACE( CAM_ERROR, "Error: buffer index[%d] returned from driver is out of range[%d] in port[%d]( %s, %s, %d )\n",
			       stV4L2Buf.index, pIspDmaState->iMCBufDescCount[iPortIdx], ePortID, __FILE__, __FUNCTION__, __LINE__ );
			*pbIsError  = CAM_TRUE;
			return CAM_ERROR_BUFFERUNAVAILABLE;
		}

		*ppImgBuf = pIspDmaState->astBufPool[iPortIdx][stV4L2Buf.index].pImgBuf;

		// report error flag
		if ( stV4L2Buf.flags & V4L2_BUF_FLAG_ERROR )
		{
			*pbIsError = CAM_TRUE;
		}
		else
		{
			*pbIsError = CAM_FALSE;
		}

		_calcimglen( *ppImgBuf );

		pIspDmaState->astBufPool[iPortIdx][stV4L2Buf.index].iFlag = 0;
		pIspDmaState->iPortBufNum[iPortIdx]--;

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: dma dequeue buffer failed, fd[%d], error code[%s]( %s, %s, %d )\n",
		       fd, strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
		error       = CAM_ERROR_BUFFERUNAVAILABLE;
		*pbIsError  = CAM_TRUE;
		*ppImgBuf   = NULL;
	}

	return error;
}

CAM_Error ispdma_flush_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID )
{
	CAM_Int32s                 i;
	CAM_Int32s                 ret           = 0;
	_CAM_MC_IspDmaState        *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;
	struct v4l2_requestbuffers stReqBufs;
	CAM_Int32s                 fd[2], aiPortId[2];
	CAM_Int32s                 iFlushPortNum = 0;
	enum v4l2_buf_type         eBufType      = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	_CHECK_BAD_POINTER( hIspDmaHandle );
	if ( ePortID != ISP_PORT_DISPLAY && ePortID != ISP_PORT_CODEC && ePortID != ISP_PORT_BOTH )
	{
		TRACE( CAM_ERROR, "Error: not support flush port[%d]( %s, %s, %d )\n", ePortID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	iFlushPortNum = 0;
	if ( ePortID & ISP_PORT_DISPLAY )
	{
		aiPortId[iFlushPortNum] = ISP_PORT_DISPLAY;
		fd[iFlushPortNum]       = pIspDmaState->pstIspDisp->fd;

		if ( pIspDmaState->stCfgOnOff.enable_disp )
		{
			ret = mc_ioctl( pIspDmaState->pstIspDisp->fd, VIDIOC_STREAMOFF, &eBufType );
			if ( ret != 0 )
			{
				TRACE( CAM_ERROR, "Error: failed to stream off display port, error code[%s]( %s, %s, %d )\n", strerror( errno ),
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_DRIVEROPFAILED;
			}
			pIspDmaState->stCfgOnOff.enable_disp = CAM_FALSE;
		}
		pIspDmaState->iPortBufNum[ ISP_PORT_DISPLAY - 1 ] = 0;
		iFlushPortNum++;
		TRACE( CAM_INFO, "Info: display port flused( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	}

	if ( ePortID & ISP_PORT_CODEC )
	{
		if ( !_is_bayer_format( pIspDmaState->aePortFormat[1] ) )
		{
			fd[iFlushPortNum]       = pIspDmaState->pstIspCodec->fd;

			if ( pIspDmaState->stCfgOnOff.enable_codec )
			{
				ret = mc_ioctl( pIspDmaState->pstIspCodec->fd, VIDIOC_STREAMOFF, &eBufType );
				if ( ret != 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to stream off codec port, error code[%s]( %s, %s, %d )\n", strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_DRIVEROPFAILED;
				}
				pIspDmaState->stCfgOnOff.enable_codec = CAM_FALSE;
			}
			TRACE( CAM_INFO, "Info: codec port flused( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		}
		else
		{
			fd[iFlushPortNum]       = pIspDmaState->pstCCICRaw->fd;

			if ( pIspDmaState->bIsCCICRawEnabled )
			{
				ret = mc_ioctl( pIspDmaState->pstCCICRaw->fd, VIDIOC_STREAMOFF, &eBufType );
				if ( ret != 0 )
				{
					TRACE( CAM_ERROR, "Error: failed to stream off display port, error code[%s]( %s, %s, %d )\n",
					       strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_DRIVEROPFAILED;
				}
				pIspDmaState->bIsCCICRawEnabled = CAM_FALSE;
			}
			TRACE( CAM_INFO, "Info: ccic raw dump port flused( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		}

		aiPortId[iFlushPortNum] = ISP_PORT_CODEC;
		pIspDmaState->iPortBufNum[ ISP_PORT_CODEC - 1 ] = 0;
		iFlushPortNum++;
		TRACE( CAM_INFO, "Info: codec port flused( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	}

	for ( i = 0; i < iFlushPortNum; i++ )
	{
		memset( &stReqBufs, 0x00, sizeof( struct v4l2_requestbuffers ) );
		stReqBufs.count   = 0;
		stReqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stReqBufs.memory  = V4L2_MEMORY_USERPTR;
		ret = mc_ioctl( fd[i], VIDIOC_REQBUFS, &stReqBufs );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: request buffer 0 from isp-dma port[%d] failed, fd[%d], error code[%s]( %s, %s, %d )\n",
			       aiPortId[i], fd[i], strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}

		/* The behavior of flush buffer is flush all buffer in driver, but we
		 * still keep available number of slots in driver for future usage model
		 */
		stReqBufs.count   = CAM_MAX_PORT_BUF_CNT;
		stReqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stReqBufs.memory  = V4L2_MEMORY_USERPTR;
		ret = mc_ioctl( fd[i],  VIDIOC_REQBUFS, &stReqBufs );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: reqbuf buffer %d from isp-dma port[%d] failed, fd[%d], error code[%s]( %s, %s, %d )\n",
			       CAM_MAX_PORT_BUF_CNT, aiPortId[i], fd[i], strerror( errno ), __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_DRIVEROPFAILED;
		}

		memset( pIspDmaState->astBufPool[aiPortId[i] - 1], 0, sizeof( _CAM_MCBufDesc ) * CAM_MAX_PORT_BUF_CNT );
		pIspDmaState->iMCBufDescCount[aiPortId[i] - 1] = 0;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_get_bufreq( void *hIspDmaHandle, _CAM_ImageInfo *pstConfig, CAM_ImageBufferReq *pstBufReq )
{
	_CAM_MC_IspDmaState        *pIspDmaState = (_CAM_MC_IspDmaState*)hIspDmaHandle;

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

	pstBufReq->iMinBufCount = ( pstConfig->bIsStreaming == CAM_TRUE || _is_bayer_format(pIspDmaState->aePortFormat[1]) ) ? 2 : 1;
	_calcstep( pstBufReq->eFormat, pstBufReq->iWidth, pstBufReq->iRowAlign, pstBufReq->iMinStep );

	_calcbuflen( pstBufReq->eFormat, pstBufReq->iWidth, pstBufReq->iHeight, pstBufReq->iMinStep, pstBufReq->iMinBufLen, NULL );

	/*
	CELOG( "Info: format = %d, width = %d, height = %d, align = %d, %d, %d, step = %d, %d, %d, size = %d, %d, %d\n",
	        pstBufReq->eFormat, pstBufReq->iWidth, pstBufReq->iHeight,
	        pstBufReq->iRowAlign[0], pstBufReq->iRowAlign[1], pstBufReq->iRowAlign[2],
	        pstBufReq->iMinStep[0], pstBufReq->iMinStep[1], pstBufReq->iMinStep[2],
	        pstBufReq->iMinBufLen[0], pstBufReq->iMinBufLen[1], pstBufReq->iMinBufLen[2] );
	*/

	return CAM_ERROR_NONE;
}

CAM_Error ispdma_get_min_bufcnt( void *hIspDmaHandle, CAM_Bool bIsStreaming, CAM_Int32s *pMinBufCnt )
{
	_CHECK_BAD_POINTER( pMinBufCnt );

	*pMinBufCnt = ( bIsStreaming == CAM_TRUE ) ? 2 : 1;

	return CAM_ERROR_NONE;
}

/* isp register access */
inline void DxOISP_setRegister( const uint32_t uiOffset, uint32_t uiValue )
{
#if 0
	struct dxo_regs stDxOReg = {0};

	assert( uiOffset % sizeof(uint32_t) == 0 ) ;

	memset( &stDxOReg, 0, sizeof(stDxOReg) );
	stDxOReg.offset = uiOffset;
	stDxOReg.value  = uiValue;

	mc_ioctl( ispdma_fd, ISP_IOCTL_IPCREG_SET, &stDxOReg );
#else
	ASSERT( pucIPCAddr != 0 );

	*(uint32_t*)(pucIPCAddr + uiOffset) = uiValue;

#endif
}

inline uint32_t DxOISP_getRegister( const uint32_t uiOffset )
{
	uint32_t value;
#if 0
	struct dxo_regs stDxOReg = {0};
	uint32_t uiResult;

	assert( uiOffset % sizeof(uint32_t) == 0 ) ;

	memset( &stDxOReg, 0, sizeof( stDxOReg ) );
	stDxOReg.offset = uiOffset;
	mc_ioctl( ispdma_fd, ISP_IOCTL_IPCREG_GET, &stDxOReg );
#else
	ASSERT( pucIPCAddr != 0 );

	value = *(uint32_t*)(pucIPCAddr + uiOffset);
#endif

	return value;
}

inline void DxOISP_setMultiRegister( const uint32_t uiOffset, const uint32_t* puiSrc, uint32_t uiNbElem )
{
#if 0
	struct dxo_regs stDxOReg = {0};
	uint32_t        i;

	assert( uiOffset % sizeof( uint32_t ) == 0 ) ;

	memset( &stDxOReg, 0, sizeof( stDxOReg ) );
	stDxOReg.offset = uiOffset;
	stDxOReg.src    = puiSrc;
	stDxOReg.cnt    = uiNbElem;

	mc_ioctl( ispdma_fd, ISP_IOCTL_IPCREG_SET_MULTI, &stDxOReg );
#else
	CAM_Int32u i = 0;
	ASSERT( pucIPCAddr != 0 );

	for ( i = 0; i < uiNbElem; i++ )
	{
		*(uint32_t*)(pucIPCAddr + uiOffset) = puiSrc[i];
	}
#endif

	return;
}

inline void DxOISP_getMultiRegister( const uint32_t uiOffset, uint32_t* puiDst, uint32_t uiNbElem )
{
#if 0
	struct dxo_regs stDxOReg = {0};
	uint32_t        i;
	uint32_t        uiResult;

	assert( uiOffset % sizeof( uint32_t ) == 0 ) ;

	memset( &stDxOReg, 0, sizeof( stDxOReg ) );
	stDxOReg.offset = uiOffset;
	stDxOReg.dst    = puiDst;
	stDxOReg.cnt    = uiNbElem;

	mc_ioctl( ispdma_fd, ISP_IOCTL_IPCREG_GET_MULTI, &stDxOReg );
#else
	CAM_Int32u i = 0;
	ASSERT( pucIPCAddr != 0 );

	for ( i = 0; i < uiNbElem; i++ )
	{
		puiDst[i] = *(uint32_t*)( pucIPCAddr + uiOffset );
	}
#endif

	return;
}
