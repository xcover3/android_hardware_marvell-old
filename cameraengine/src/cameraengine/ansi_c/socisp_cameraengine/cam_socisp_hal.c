/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stddef.h>

#include "CameraEngine.h"
#include "CameraOSAL.h"
#include "cam_log.h"
#include "cam_utility.h"

#include "cam_socisp_eng.h"
#include "cam_socisp_hal.h"

#include "DxOISP.h"
#include "DxOISP_Tools.h"

#include "cam_socisp_dmactrl.h"
#include "cam_socisp_sensorhal.h"

// FIXME
#define ThreadSafe_DxOISP_CommandGroupOpen()  DxOISP_CommandGroupOpen()

#define ThreadSafe_DxOISP_CommandGroupClose() DxOISP_CommandGroupClose()

#define ThreadSafe_DxOISP_StatusGroupOpen()   DxOISP_StatusGroupOpen()

#define ThreadSafe_DxOISP_StatusGroupClose()  DxOISP_StatusGroupClose()

typedef struct
{
	CAM_Size           stOutputSize;
	CAM_Size           stCropSize;
	CAM_Int8u          ucUpScaleTolerance;
} _CAM_ResToCropUTMap;

#define SOCISP_MAX_VIDEO_SIZE 7
typedef struct
{
	CAM_Size            stSensorSize;
	_CAM_ResToCropUTMap astResToCropUT[SOCISP_MAX_VIDEO_SIZE];
} _CAM_ImageStrategyMap;

static const CAM_ImageFormat gIspDispFormatCap[] =
{
	// YUV low->high
	CAM_IMGFMT_CbYCrY,
	CAM_IMGFMT_YCbYCr,
	CAM_IMGFMT_YCrYCb,
	CAM_IMGFMT_CrYCbY,
	// RGB low->high
	CAM_IMGFMT_RGB888,
	CAM_IMGFMT_BGR888,
	CAM_IMGFMT_RGB565,
	CAM_IMGFMT_BGR565,
};
#define SOCISP_MAX_DISP_FORMAT  _ARRAY_SIZE( gIspDispFormatCap )


static const CAM_ImageFormat gIspVidFormatCap[] =
{
	// YUV low->high
	CAM_IMGFMT_CbYCrY,
	CAM_IMGFMT_CrYCbY,
	CAM_IMGFMT_YCbYCr,
	CAM_IMGFMT_YCrYCb,
};
#define SOCISP_MAX_VIDEO_FORMAT   _ARRAY_SIZE( gIspVidFormatCap )

// TODO: check with DxO how to use "Preview" table in image strategy excel sheet
static const _CAM_ImageStrategyMap   gIspVidSizeCap[] =
{
	{
	{ 3264, 2448 },
	{{ {320, 240}, {2968, 2226}, 32 },
	{ {640, 480}, {2968, 2226}, 32 },
	{ {800, 480}, {2968, 1680}, 32 },
	{ {720, 576}, {2782, 2226}, 32 },
	{ {1024,768}, {2968, 2226}, 32 },
	{ {1280,720}, {2968, 1670}, 32 },
	{ {1920,1080},{1920, 1080}, 32 }}
	},

	{
	{ 2592, 1944 },
	{{ {320, 240}, {2356, 1768}, 32 },
	{ {640, 480}, {2356, 1768}, 32 },
	{ {800, 480}, {2356, 1334}, 32 },
	{ {720, 576}, {2210, 1768}, 32 },
	{ {1024,768}, {2356, 1768}, 32 },
	{ {1280,720}, {2356, 1326}, 32 },
	{ {1920,1080},{1920, 1080}, 32 }}
	},

	{
	{ 2048, 1536 },
	{{ {320, 240}, {1862, 1396}, 32 },
	{ {640, 480}, {1862, 1396}, 32 },
	{ {800, 480}, {1862, 1054}, 32 },
	{ {720, 576}, {1746, 1396}, 32 },
	{ {1024,768}, {1862, 1396}, 32 },
	{ {1280,720}, {1862, 1048}, 32 },
	{ {1920,1080},{1862, 1048}, 32 }}
	},

	{
	{ 1600, 1200 },
	{{ {320, 240}, {1454, 1090}, 32 },
	{ {640, 480}, {1454, 1090}, 32 },
	{ {800, 480}, {1454, 824 }, 32 },
	{ {720, 576}, {1364, 1090}, 32 },
	{ {1024,768}, {1454, 1090}, 32 },
	{ {1280,720}, {1454, 818 }, 32 },
	{ {1920,1080},{1920, 1080}, 32 }}          // FIXME: Need double check with DxO why this cell is blank
	},

	{
	{ 1920, 1080 },
	{{ {320, 240}, {1310, 982}, 32 },
	{ {640, 480}, {1310, 982}, 32 },
	{ {800, 480}, {1734, 982}, 32 },
	{ {720, 576}, {1228, 982}, 32 },
	{ {1024,768}, {1310, 982}, 32 },
	{ {1280,720}, {1746, 982}, 32 },
	{ {1920,1080},{1746, 982}, 32 }}
	},
};

#define SOCISP_MAX_SENSOR_COUNT    _ARRAY_SIZE( gIspVidSizeCap )


static const CAM_ImageFormat gIspStillFormatCap[] =
{
	// YUV low->high
	CAM_IMGFMT_CbYCrY,
	CAM_IMGFMT_CrYCbY,
	CAM_IMGFMT_YCbYCr,
	CAM_IMGFMT_YCrYCb,
	CAM_IMGFMT_BGGR10,
};
#define SOCISP_MAX_STILL_FORMAT   _ARRAY_SIZE( gIspStillFormatCap )


static const CAM_FlipRotate gIspFlipRotateCap[] =
{
	CAM_FLIPROTATE_NORMAL,
	CAM_FLIPROTATE_180,
	CAM_FLIPROTATE_HFLIP,
	CAM_FLIPROTATE_VFLIP,
};
#define SOCISP_MAX_FLIPROTATE  _ARRAY_SIZE( gIspFlipRotateCap )


static const CAM_ShotMode  gIspShotModeCap[] =
{
	CAM_SHOTMODE_AUTO,
};
#define SOCISP_MAX_SHOTMODE  _ARRAY_SIZE( gIspShotModeCap )

static const CAM_FocusMode gIspFocusModeCap[] =
{
	CAM_FOCUS_INFINITY,
};
#define SOCISP_MAX_FOCUSMODE  _ARRAY_SIZE( gIspFocusModeCap )

// global map table between Camera Engine and ISP
typedef struct
{
	CAM_ImageFormat eCEFormat;
	CAM_Int8u       eIspFormat;
	CAM_Int8u       eIspOrder;
}_CAM_DxOIspFmtMap;

static const _CAM_DxOIspFmtMap gImgFmtMap[] = { { CAM_IMGFMT_YCbYCr, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_YUYV,  },
                                                { CAM_IMGFMT_CbYCrY, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                // XXX: This is a hack to let ISP know: "Even for raw data capture,
                                                // I also need to set sensor parameters as normal UYVY case"
                                                { CAM_IMGFMT_RGGB8,  DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_BGGR8,  DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_GRBG8,  DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_GBRG8,  DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_RGGB10, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_BGGR10, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_GRBG10, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_GBRG10, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                                { CAM_IMGFMT_YCrYCb, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_YVYU,  },
                                                { CAM_IMGFMT_CrYCbY, DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_VYUY,  },
                                                { CAM_IMGFMT_RGGB8,  DxOISP_FORMAT_RAW,    DxOISP_SENSOR_PHASE_RGGB, },
                                                { CAM_IMGFMT_GRBG8,  DxOISP_FORMAT_RAW,    DxOISP_SENSOR_PHASE_GRBG, },
                                                { CAM_IMGFMT_BGGR8,  DxOISP_FORMAT_RAW,    DxOISP_SENSOR_PHASE_BGGR, },
                                                { CAM_IMGFMT_GBRG8,  DxOISP_FORMAT_RAW,    DxOISP_SENSOR_PHASE_GBRG, },
                                                { CAM_IMGFMT_JPEG,   DxOISP_FORMAT_YUV422, DxOISP_YUV422ORDER_UYVY,  },
                                               };
#define SOCISP_LEN_FMTMAP  _ARRAY_SIZE( gImgFmtMap )


#define SYSTEM_FREQUENCY            (400u)
#define SYSTEM_MAX_BANDWIDTH        (5000)
#define OV_INTERFRAME_INTERVAL_0    10
#define OV_INTERFRAME_INTERVAL_1    15
#define OV_UPSCALING_TOLER          2

#define DxOISP_MIN_DIGZOOM_Q16              (1 << 16)
#define DxOISP_DIGZOOM_STEP_Q16             (1 << 16)
#define DxOISP_MIN_FPS_Q16                  (1 << 16)
#define DxOISP_MAX_FPS_Q16                  (30<< 16)
#define DxOISP_SMOOTH_FRAMETOFRAME_LIMIT    (1000)
#define DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT (0xFFFF)


typedef struct
{
	CAM_Int8u          iFBufCnt;
	CAM_Int8u          iBandCnt;
	CAM_Int32s         aiFBufSizeTab[NB_FRAME_BUFFER];
} _CAM_IspFrameBufferReq;

typedef struct
{
	// isp firmware handle
	ts_DxOIspCmd      stDxOIspCmd;

	CAM_Thread        stIspMonitorThread;
	CAM_Thread        stIspPostProcessThread;
	void              *hEventIPCStatusUpdated;

	CAM_Int32s        iDigZoomQ16;

	// isp dma handle
	void              *hIspDmaHandle;

	// queued buffer counter
	CAM_Int32s        iDispBufCnt;
	CAM_Int32s        iCodecBufCnt;
	CAM_Int32s        iSkippedStillFrames;

	// Minimal buffer count requested by isp dma
	CAM_Int32s        iDispMinBufCnt;
	CAM_Int32s        iCodecMinBufCnt;

	// save isp state
	te_DxOISPMode     eIspSavedState;

	// isp dma and isp firmware status
	CAM_Bool          bIsCodecOn;
	CAM_Bool          bIsDisplayOn;
	CAM_Bool          bCodecShouldEnable;
	CAM_Bool          bDisplayShouldEnable;
	CAM_Bool          bIsIspOK;

	CAM_ImageFormat   eStillFormat;
	/*
	*@ internal buffer management, there are mainly 3 kinds of buffers of internal buffers:
	*  1. for Camera Engine use, e.g. display ring buffer, which aims to negative-shutter lag's display;
	*  2. for ISP use, e.g, frame buffer for still image processing, buffers for temporal denoising or video stablizer -- frame buffer
	*  3. for Camera Engine & isp use, e.g. raw-data buffer to JPEG encoder since ISP has no codec
	*/
	// frame buffer management
	_CAM_IspFrameBufferReq stFBufReq;
	_CAM_IspDmaBufNode     astFBuf[NB_FRAME_BUFFER];
	_CAM_ResToCropUTMap    *pastResToCropMap;
} _CAM_DxOIspState;

// internal function declaration
static void      _isp_default_config( _CAM_DxOIspState *pIspState, CAM_Int32s iSensorID );
static CAM_Error _isp_setup_codec_property( _CAM_DxOIspState *pIspState );
static CAM_Error _isp_set_internal_state( _CAM_DxOIspState *pIspState, te_DxOISPMode eDxOISPMode );
static CAM_Error _isp_set_display_port_state( _CAM_DxOIspState *pIspState, CAM_Bool bDisableDisplay );
CAM_Error        _isp_update_view( ts_DxOIspCmd *pDxOCmd, CAM_Int32s iCropWidth, CAM_Int32s iCropHeight,
                                   CAM_Int32u iDigZoomQ16, CAM_Int8u ucUpScaleTolerance, CAM_Bool bSmooth );

static void      _isp_compute_framebuffer_size( _CAM_IspFrameBufferReq *pIspFBuf, ts_DxOIspSyncCmd *pstIspSyncCmd );
static CAM_Bool  _isp_need_realloc_framebuffer( _CAM_DxOIspState *pIspState, _CAM_IspFrameBufferReq *pIspFBuf );
static CAM_Error _isp_alloc_framebuffer( _CAM_DxOIspState *pIspState );
static void      _isp_free_framebuffer( _CAM_DxOIspState *pIspState );

static void _isp_dump_status( void );
static void _isp_wait_for_mode( _CAM_DxOIspState *pIspState, CAM_Int8u ucTargetMode );

static void _isp_calc_pixperbayer( te_DxOISPFormat eIspFormat, CAM_Int32s *pPixH, CAM_Int32s *pPixV );
static void _isp_calc_upscaletolerance( CAM_Int8u *pucUpscalingTolerance,
                                        CAM_Int32u usSensorWidth,
                                        CAM_Int32s usSensorHeight,
                                        CAM_Int32u usOutWidth,
                                        CAM_Int32s usOutHeight );
static void _isp_set_min_interframe_time( CAM_Bool bStateTranstion, ts_DxOIspCmd *pDxOCmd );
static _CAM_ResToCropUTMap* _isp_get_imagestrategy_map( CAM_Int16u usSensorWidth, CAM_Int16u usSensorHeight,
                                                        _CAM_ImageStrategyMap *pastImageStrategyMap, CAM_Int32u uiCount );

CAM_Tick             iMaxBfEventDelay = 0, iMaxAftEventDelay = 0;

// Define the criteria of recover success. We think recover is success if IPC
// interrupt triggered in time for at least DxOISP_RECOVER_SUCCESS_INTERVAL times.
#define DxOISP_RECOVER_SUCCESS_INTERVAL       ( 10 )
// Maxim recover count, if recover fail number exceed this number, we'll exit application
#define DxOISP_MAX_RECOVER_COUNT              ( 5 )

static CAM_Int32s IspMonitorThread( void *pParam )
{
	_CAM_DxOIspState     *pIspState    = (_CAM_DxOIspState*)pParam;
	CAM_Error            error         = CAM_ERROR_NONE;
	CAM_Int32s           ret           = 0;
	CAM_Int32u           uiIspStatus;
	CAM_ThreadAttribute  stThreadAttr;
	CAM_Tick iInterruptTick, iBfEventTick, iAftEventTick;
	CAM_Int32u           uiRecoverFailedCount = 0, uiPrevHangCounterID = 0, uiCounter = 0;

	ret = CAM_ThreadGetAttribute( &pIspState->stIspMonitorThread, &stThreadAttr );
	ASSERT( ret == 0 );

	for ( ; ; )
	{
		CAM_Tick t;

		if ( pIspState->stIspMonitorThread.bExitThread == CAM_TRUE )
		{
			if ( pIspState->stIspMonitorThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pIspState->stIspMonitorThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}

			return 0;
		}

		// update thread attribute
		stThreadAttr.iPolicy   = CAM_THREAD_POLICY_NORMAL;
		stThreadAttr.iPriority = CAM_THREAD_PRIORITY_HIGH;
		ret = CAM_ThreadSetAttribute( &pIspState->stIspMonitorThread, &stThreadAttr );
		ASSERT( ret == 0 );

		error = ispdma_poll_ipc( pIspState->hIspDmaHandle, 500, &iInterruptTick );

		if ( error == CAM_ERROR_TIMEOUT )
		{
#if defined ( BUILD_OPTION_DEBUG_DUMP_ISP_EVENT_LATENCY )
			TRACE( CAM_WARN, "Warning: wait for isp event time out [iMaxBfEventDelay:%lldus,iMaxAftEventDelay:%lldus]( %s, %s, %d )\n",
			       iMaxBfEventDelay, iMaxAftEventDelay, __FILE__, __FUNCTION__, __LINE__ );
#endif
			_isp_dump_status();
			// We can not treat each time of IPC interrupt timeout as IPC can
			// NOT recover. Actually, ISP FW will automatically reset IPC when
			// pixel stuck is detected, if success, it can continue work and trigger
			// IPC interrupt in time at least DxOISP_RECOVER_SUCCESS_INTERVAL times.
			// If failed, we'll record the failed case and exit application
			// when CONSECUTIVE failed cases exceed DxOISP_MAX_RECOVER_COUNT
			if ( uiPrevHangCounterID + DxOISP_RECOVER_SUCCESS_INTERVAL > uiCounter )
			{
				uiRecoverFailedCount++;
				pIspState->bIsIspOK = uiRecoverFailedCount < DxOISP_MAX_RECOVER_COUNT;
				if ( !pIspState->bIsIspOK )
				{
					TRACE( CAM_ERROR, "Error: Detect ISP hang @ %d frame ( %s, %s, %d )\n",
					       uiCounter, __FILE__, __FUNCTION__, __LINE__ );
				}
			}
			else
			{
				uiRecoverFailedCount = 0;
			}
			uiPrevHangCounterID = uiCounter;
		}

#if defined ( BUILD_OPTION_DEBUG_DUMP_ISP_EVENT_LATENCY )
		ispdma_get_tick( pIspState->hIspDmaHandle, &iBfEventTick );
#endif
		// NOTE: execute DxOISP_Event() no matter if timeout happens
		DxOISP_Event();
		uiCounter++;

#if defined ( BUILD_OPTION_DEBUG_DUMP_ISP_EVENT_LATENCY )
		ispdma_get_tick( pIspState->hIspDmaHandle, &iAftEventTick );

		if ( iBfEventTick - iInterruptTick > iMaxBfEventDelay )
		{
			iMaxBfEventDelay = iBfEventTick - iInterruptTick;
			CELOG( "MaxBfEventDelay:%lldus\n", iMaxBfEventDelay );
		}

		if ( iAftEventTick - iInterruptTick > iMaxAftEventDelay )
		{
			iMaxAftEventDelay = iAftEventTick - iInterruptTick;
			CELOG( "iBfEventDelay:%lldus, DxO_Event Execute:%lldus, iMaxAftEventDelay:%lldus\n",
			        iBfEventTick - iInterruptTick, iAftEventTick - iBfEventTick, iMaxAftEventDelay );
		}
#endif

#if defined( BUILD_OPTION_STRATEGY_ENABLE_ISP_AUTO_RESET )
		DxOISP_StatusGroupOpen();
		DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.uiErrorCode ), sizeof( uint32_t ), &uiIspStatus );
		DxOISP_StatusGroupClose();

		if ( uiIspStatus == DxOISP_PIXEL_STUCK )
		{
			iMaxBfEventDelay  = 0;
			iMaxAftEventDelay = 0;
			TRACE( CAM_WARN, "Warning: We detected ISP hang, an automatically reset here( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			error = isp_reset_hardware( pIspState, CAM_RESET_COMPLETE );
			if ( error != CAM_ERROR_NONE )
			{
				TRACE( CAM_ERROR, "Error: isp automatically reset failed[%d]. ( %s, %d )\n", error, __FILE__, __LINE__ );
			}
			continue;
		}
#endif

		if ( pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
	}

	return 0;
}


static CAM_Int32s IspPostProcessThread( void *pParam )
{
	_CAM_DxOIspState   *pIspState = (_CAM_DxOIspState*)pParam;
	volatile CAM_Tick  t, t1;

#if defined ( BUILD_OPTION_DEBUG_DUMP_ISP_SHOOTING_PARAMETERS )
	CAM_Int16u usFrameRate1,   usFrameRate;
	CAM_Int16u usIspGain1,     usIspGain;
	CAM_Int32u uiExpTime1,     uiExpTime;
        CAM_Int16u usAnalogGain1,  usAnalogGain;
	CAM_Int16u usDigitalGain1, usDigitalGain;
//	uint8_t  ucAWBPreset1,   ucAWBPreset;
	CAM_Int16u usWBRedScale1,  usWBRedScale;
	CAM_Int16u usWBBlueScale1, usWBBlueScale;
	CAM_Int32s iRepeatCnt = 0;
#endif

	for ( ; ; )
	{
		int ret;
		CAM_Bool bIsTimeOut = 0;

		if ( pIspState->stIspPostProcessThread.bExitThread == CAM_TRUE )
		{
			if ( pIspState->stIspPostProcessThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pIspState->stIspPostProcessThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}

			return 0;
		}

		if ( pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup, 1000, &bIsTimeOut );
			ASSERT( ret == 0 );
		}

		if ( bIsTimeOut == 0 )
		{
			t = -CAM_TimeGetTickCount();
			DxOISP_PostEvent();

			t += CAM_TimeGetTickCount();

			ret = CAM_EventSet( pIspState->hEventIPCStatusUpdated );
			ASSERT( ret == 0 );

			// CELOG( "isp post event time: %.2f ms\n", t / 1000.0 );

#if defined ( BUILD_OPTION_DEBUG_DUMP_ISP_SHOOTING_PARAMETERS )
			usFrameRate1   = usFrameRate;
			usIspGain1     = usIspGain;
			uiExpTime1     = uiExpTime;
			usAnalogGain1  = usAnalogGain;
			usDigitalGain1 = usDigitalGain;
//			ucAWBPreset1   = ucAWBPreset;
			usWBRedScale1  = usWBRedScale;
			usWBBlueScale1 = usWBBlueScale;

			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stSystem.usFrameRate), 2, (uint8_t *)&usFrameRate);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usIspDigitalGain), 2, (uint8_t *)&usIspGain);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.uiExposureTime), 4, (uint8_t *)&uiExpTime);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usAnalogGain[0]), 2, (uint8_t *)&usAnalogGain);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usDigitalGain[0]), 2, (uint8_t *)&usDigitalGain);
//			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.eSelectedPreset), 1, (uint8_t *)&ucAWBPreset);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.usWbRedScale), 2, (uint8_t *)&usWBRedScale);
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.usWbBlueScale), 2, (uint8_t *)&usWBBlueScale);
			DxOISP_StatusGroupClose();

			if ( usFrameRate != usFrameRate1 || usIspGain != usIspGain || (uiExpTime >> 6) != (uiExpTime1 >> 6) ||
			     usAnalogGain != usAnalogGain1 || usDigitalGain != usDigitalGain1 ||
			     usWBRedScale != usWBRedScale1 || usWBBlueScale != usWBBlueScale1 )
			{
				printf( "pRep=%d,FPS=%.2f,IG=%.3f,ET=%.3f,AG=%.3f,DG=%.3f,AWBRS=%.3f,AWBBS=%.3f\n",
				        iRepeatCnt, usFrameRate / 16.0, usIspGain / 256.0, uiExpTime / 65536.0,
				        usAnalogGain / 256.0, usDigitalGain / 256.0,
				        usWBRedScale / 256.0, usWBBlueScale / 256.0 );

				iRepeatCnt = 1;
			}
			else
			{
				iRepeatCnt++;
			}
#endif

		}
		else
		{
			CELOG( "wait post-event timed out\n" );
			continue;
		}
	}

	return 0;
}

CAM_Error isp_init( void **phIspHandle )
{
	CAM_Int32s       ret;
	CAM_Error        error;
	_CAM_DxOIspState *pIspState = NULL;

	pIspState = (_CAM_DxOIspState*)malloc( sizeof( _CAM_DxOIspState ) );
	if ( pIspState == NULL )
	{
		TRACE( CAM_ERROR, "Error: there is no enough memory to initialize soc-isp( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pIspState, 0, sizeof( _CAM_DxOIspState ) );

	// init isp dma
	error = ispdma_init( &pIspState->hIspDmaHandle );
	ASSERT_CAM_ERROR( error );

	// init isp
	DxOISP_Init();

	ret = CAM_EventCreate( &pIspState->hEventIPCStatusUpdated );
	ASSERT( ret == 0 );

	pIspState->stIspPostProcessThread.stThreadAttribute.iDetachState  = CAM_THREAD_CREATE_DETACHED;
	pIspState->stIspPostProcessThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pIspState->stIspPostProcessThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pIspState->stIspPostProcessThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );
	ret = CAM_ThreadCreate( &pIspState->stIspPostProcessThread, IspPostProcessThread, (void*)pIspState );
	ASSERT( ret == 0 );

	pIspState->stIspMonitorThread.stThreadAttribute.iDetachState  = CAM_THREAD_CREATE_DETACHED;
	pIspState->stIspMonitorThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pIspState->stIspMonitorThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pIspState->stIspMonitorThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pIspState->stIspMonitorThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );
	ret = CAM_ThreadCreate( &pIspState->stIspMonitorThread, IspMonitorThread, (void*)pIspState );
	ASSERT( ret == 0 );

	pIspState->bIsIspOK = CAM_TRUE;

	*phIspHandle = (void*)pIspState;

	return CAM_ERROR_NONE;
}

CAM_Error isp_deinit( void **phIspHandle )
{
	CAM_Int32s       ret;
	CAM_Error        error;
	_CAM_DxOIspState *pIspState = ( _CAM_DxOIspState* )*phIspHandle;
	CAM_Int8u        ucModeSet  = DxOISP_MODE_STANDBY;

	ret = CAM_ThreadDestroy( &pIspState->stIspMonitorThread );
	ASSERT( ret == 0 );

	if ( pIspState->stIspMonitorThread.stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventDestroy( &pIspState->stIspMonitorThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );
	}

	if ( pIspState->stIspMonitorThread.stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventDestroy( &pIspState->stIspMonitorThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );
	}

	ret = CAM_ThreadDestroy( &pIspState->stIspPostProcessThread );
	ASSERT( ret == 0 );
	if ( pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventDestroy( &pIspState->stIspPostProcessThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );
	}

	if ( pIspState->stIspPostProcessThread.stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventDestroy( &pIspState->stIspPostProcessThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );
	}

	ret = CAM_EventDestroy( &pIspState->hEventIPCStatusUpdated );
	ASSERT( ret == 0 );

	// XXX: using stand-by state to de-init ISP firmware, need DXO supply formal de-init support
	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), 1, &ucModeSet );
	ThreadSafe_DxOISP_CommandGroupClose();

	// deinit isp dma
	error = ispdma_deinit( &pIspState->hIspDmaHandle );
	ASSERT_CAM_ERROR( error );

	free( *phIspHandle );
	*phIspHandle = NULL;

	return CAM_ERROR_NONE;
}

CAM_Error isp_reset_hardware( void *hIspHandle, CAM_Int32s iResetType )
{
	// FIXME: we need a lock to protect the reset procedure?

	_CAM_DxOIspState *pIspState = ( _CAM_DxOIspState* )hIspHandle;
	CAM_Tick         t;
	ts_DxOIspCmd     *pDxOCmd = &pIspState->stDxOIspCmd;
	CAM_Int8u        ucModeState, ucSavedMode;
	CAM_Error        error;
	CAM_Int32s       cnt = 0;

	t = -CAM_TimeGetTickCount();

	// save current state
	ucSavedMode = pDxOCmd->stSync.stControl.eMode;

	if ( iResetType == CAM_RESET_COMPLETE )
	{
		// goto IDLE mode
		pDxOCmd->stSync.stControl.eMode = DxOISP_MODE_IDLE;

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), sizeof( uint8_t ), &pDxOCmd->stSync.stControl.eMode );
		ThreadSafe_DxOISP_CommandGroupClose();

		do
		{
			// Do not sleep in the first loop
			if ( cnt != 0 )
			{
				CAM_Sleep( 10000 );
			}

			DxOISP_Event();
			DxOISP_PostEvent();

			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.eMode ), 1, &ucModeState );
			DxOISP_StatusGroupClose();

			cnt++;
		} while ( ucModeState != pDxOCmd->stSync.stControl.eMode && cnt < 10 );
		ASSERT( ucModeState == pDxOCmd->stSync.stControl.eMode );
	}

	// reset ispdma
	error = ispdma_reset( pIspState->hIspDmaHandle );

	if ( iResetType == CAM_RESET_COMPLETE )
	{
		// goto previous state
		pDxOCmd->stSync.stControl.eMode = ucSavedMode;

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), sizeof( uint8_t ), &pDxOCmd->stSync.stControl.eMode );
		ThreadSafe_DxOISP_CommandGroupClose();

		// we don't wait state transition done here, cause we are in IPCMonitorThread now.
	}

	t += CAM_TimeGetTickCount();
//	TRACE( CAM_WARN, "reset time: %.2fms\n", t / 1000.0 );

	return error;
}

CAM_Error isp_enum_sensor( CAM_Int32s *piSensorCount, CAM_DeviceProp astCameraProp[CAM_MAX_SUPPORT_CAMERA] )
{
	CAM_Error error = CAM_ERROR_NONE;

	error = ispdma_enum_sensor( piSensorCount, astCameraProp );

	return error;
}

CAM_Error isp_select_sensor( void *hIspHandle, CAM_Int32s iSensorID )
{
	_CHECK_BAD_POINTER( hIspHandle );

	CAM_Error        error;
	CAM_Int32s       i;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;

	// select isp dma lane
	error = ispdma_select_sensor( pIspState->hIspDmaHandle, iSensorID );
	ASSERT_CAM_ERROR( error );

	_isp_default_config( pIspState, iSensorID );
	pIspState->iDigZoomQ16 = ( 1 << 16 );

	for ( i = 0; i < pIspState->stFBufReq.iFBufCnt; i++ )
	{
		free( pIspState->astFBuf[i].pUserData );
		memset( &pIspState->astFBuf[i], 0, sizeof( _CAM_IspDmaBufNode ) );
	}
	pIspState->stFBufReq.iFBufCnt = 0;

	return CAM_ERROR_NONE;
}

CAM_Error isp_query_capability( void *hIspHandle, CAM_Int32s iSensorID, CAM_CameraCapability *pCameraCap )
{
	CAM_Int32s         i, cnt;
	CAM_PortCapability *pPortCap      = NULL;
	void               *hIspDmaHandle = NULL;
	CAM_Error          error;

	CAM_Int16u         usMaxDispHeight  = 0, usMaxDispWidth  = 0;
	CAM_Int16u         usMaxVidHeight   = 0, usMaxVidWidth   = 0;
	CAM_Int16u         usMaxStillHeight = 0, usMaxStillWidth = 0;
	CAM_Int16u         usBayerWidth     = 0, usBayerHeight   = 0;
	_CAM_ResToCropUTMap *pstResToCropUT = NULL;
	_CHECK_BAD_POINTER( pCameraCap );

	if ( hIspHandle == NULL )
	{
		CAM_Int8u    ucSensorID  = (CAM_Int8u)iSensorID;

		// init isp dma
		error = ispdma_init( &hIspDmaHandle );
		ASSERT_CAM_ERROR( error );

		// select isp dma lane
		error = ispdma_select_sensor( hIspDmaHandle, iSensorID );
		ASSERT_CAM_ERROR( error );

		DxOISP_Init();

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.ucSourceSelection), 1, &ucSensorID );
		ThreadSafe_DxOISP_CommandGroupClose();
	}

	// preview port capability
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usScreenFrameSizeX ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxDispWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usScreenFrameSizeY ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxDispHeight );
	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_PREVIEW];

	pPortCap->bArbitrarySize  = CAM_TRUE;
	pPortCap->stMin.iWidth    = 176;
	pPortCap->stMin.iHeight   = 144;
	pPortCap->stMax.iHeight   = usMaxDispHeight;
	pPortCap->stMax.iWidth    = usMaxDispWidth;

	pPortCap->iSupportedFormatCnt = SOCISP_MAX_DISP_FORMAT;
	for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
	{
		pPortCap->eSupportedFormat[i] = gIspDispFormatCap[i];
	}

	// still port capability
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usStillFrameSizeX ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxStillWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usStillFrameSizeY ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxStillHeight );

	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerWidth ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerHeight ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerHeight );
	pPortCap = &(pCameraCap->stPortCapability[CAM_PORT_STILL]);
	pPortCap->bArbitrarySize = CAM_TRUE;
	pPortCap->stMin.iWidth   = 176;
	pPortCap->stMin.iHeight  = 144;
	pPortCap->stMax.iHeight  = _MIN( usMaxStillHeight, usBayerHeight * 2 );
	pPortCap->stMax.iWidth   = _MIN( usMaxStillWidth, usBayerWidth * 2 );
	CELOG( "usMaxStillHeight: %d, usBayerHeight * 2: %d, usMaxStillWidth: %d, usBayerWidth * 2: %d\n",
	       usMaxStillHeight, usBayerHeight * 2, usMaxStillWidth, usBayerWidth * 2 );

	pPortCap->iSupportedFormatCnt = SOCISP_MAX_STILL_FORMAT;
	for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
	{
		pPortCap->eSupportedFormat[i] = gIspStillFormatCap[i];
	}

	// video port capability
	/* FIXME:
	 * These commands will get dxo-isp's maximum supported size when process on-the-fly.
	 * But if sensor's resolution is less than this value, it will lead to error.
	 */
	pstResToCropUT = _isp_get_imagestrategy_map( usBayerWidth * 2, usBayerHeight * 2, (_CAM_ImageStrategyMap*)gIspVidSizeCap, SOCISP_MAX_SENSOR_COUNT );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usVideoFrameSizeX ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxVidWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stIspCapabilities.stHardware.usVideoFrameSizeY ),
	                  sizeof( CAM_Int16u ),
	                  &usMaxVidHeight );
	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_VIDEO];
	pPortCap->bArbitrarySize = CAM_FALSE;
	if ( NULL != pstResToCropUT )
	{
		pPortCap->iSupportedSizeCnt = SOCISP_MAX_VIDEO_SIZE;
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			pPortCap->stSupportedSize[i] = pstResToCropUT[i].stOutputSize;
		}

		pPortCap->iSupportedFormatCnt = SOCISP_MAX_VIDEO_FORMAT;
		for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
		{
			pPortCap->eSupportedFormat[i] = gIspVidFormatCap[i];
		}
	}
	else
	{
		pPortCap->iSupportedSizeCnt = 0;
		pPortCap->iSupportedFormatCnt = 0;
	}

	// ISP rotation
	pCameraCap->iSupportedSensorRotateCnt = SOCISP_MAX_FLIPROTATE;
	for ( i = 0; i < pCameraCap->iSupportedSensorRotateCnt; i++ )
	{
		pCameraCap->eSupportedSensorRotate[i] = gIspFlipRotateCap[i];
	}

	// shot mode capability
	pCameraCap->iSupportedShotModeCnt = SOCISP_MAX_SHOTMODE;
	for ( i = 0; i < pCameraCap->iSupportedShotModeCnt; i++ )
	{
		pCameraCap->eSupportedShotMode[i] = gIspShotModeCap[i];
	}

	// TODO: shot parameters capability
	pCameraCap->stSupportedShotParams.iSupportedFocusModeCnt = 1;
	pCameraCap->stSupportedShotParams.eSupportedFocusMode[0] = CAM_FOCUS_INFINITY;

	pCameraCap->stSupportedShotParams.iMinDigZoomQ16  = DxOISP_MIN_DIGZOOM_Q16;
	pCameraCap->stSupportedShotParams.iMaxDigZoomQ16  = BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16;
	pCameraCap->stSupportedShotParams.iDigZoomStepQ16 = DxOISP_DIGZOOM_STEP_Q16;

	pCameraCap->stSupportedShotParams.iMinFpsQ16      = DxOISP_MIN_FPS_Q16;
	pCameraCap->stSupportedShotParams.iMaxFpsQ16      = DxOISP_MAX_FPS_Q16;

	pCameraCap->stSupportedShotParams.iSupportedStillSubModeCnt = 3;
	pCameraCap->stSupportedShotParams.eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
	pCameraCap->stSupportedShotParams.eSupportedStillSubMode[1] = CAM_STILLSUBMODE_BURST;
	pCameraCap->stSupportedShotParams.eSupportedStillSubMode[2] = CAM_STILLSUBMODE_ZSL;

	pCameraCap->stSupportedShotParams.iMaxBurstCnt = DxOISP_NB_MAX_CAPTURE;

	pCameraCap->stSupportedShotParams.iMaxZslDepth = DxOISP_NB_MAX_CAPTURE - 1;

	pCameraCap->stSupportedShotParams.iSupportedVideoSubModeCnt = 4;
	pCameraCap->stSupportedShotParams.eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;
	pCameraCap->stSupportedShotParams.eSupportedVideoSubMode[1] = CAM_VIDEOSUBMODE_STABILIZER;
	pCameraCap->stSupportedShotParams.eSupportedVideoSubMode[2] = CAM_VIDEOSUBMODE_TNR;
	pCameraCap->stSupportedShotParams.eSupportedVideoSubMode[3] = CAM_VIDEOSUBMODE_STABILIZEDTNR;

	pCameraCap->stSupportedShotParams.iSupportedBdFltModeCnt = 4;
	pCameraCap->stSupportedShotParams.eSupportedBdFltMode[0] = CAM_BANDFILTER_AUTO;
	pCameraCap->stSupportedShotParams.eSupportedBdFltMode[1] = CAM_BANDFILTER_OFF;
	pCameraCap->stSupportedShotParams.eSupportedBdFltMode[2] = CAM_BANDFILTER_50HZ;
	pCameraCap->stSupportedShotParams.eSupportedBdFltMode[3] = CAM_BANDFILTER_60HZ;

	pCameraCap->stSupportedShotParams.iSupportedExpModeCnt = 4;
	pCameraCap->stSupportedShotParams.eSupportedExpMode[0] = CAM_EXPOSUREMODE_AUTO;
	pCameraCap->stSupportedShotParams.eSupportedExpMode[1] = CAM_EXPOSUREMODE_APERTUREPRIOR;
	pCameraCap->stSupportedShotParams.eSupportedExpMode[2] = CAM_EXPOSUREMODE_SHUTTERPRIOR;
	pCameraCap->stSupportedShotParams.eSupportedExpMode[3] = CAM_EXPOSUREMODE_MANUAL;

	if ( hIspHandle == NULL )
	{
		CAM_Int8u ucModeSet = DxOISP_MODE_STANDBY;

		// XXX: using stand-by state to de-init ISP firmware, need DXO supply formal de-init support
		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), 1, &ucModeSet );
		ThreadSafe_DxOISP_CommandGroupClose();

		// deinit isp dma
		error = ispdma_deinit( &hIspDmaHandle );
		ASSERT_CAM_ERROR( error );
	}

	return CAM_ERROR_NONE;
}

CAM_Error isp_query_shotmodecap( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pstShotModeCap )
{
	// FIXME
	memset( pstShotModeCap, 0, sizeof( CAM_ShotModeCapability ) );

	pstShotModeCap->iSupportedFocusModeCnt = 1;
	pstShotModeCap->eSupportedFocusMode[0] = CAM_FOCUS_INFINITY;

	pstShotModeCap->iMinDigZoomQ16  = DxOISP_MIN_DIGZOOM_Q16;
	pstShotModeCap->iMaxDigZoomQ16  = BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16;
	pstShotModeCap->iDigZoomStepQ16 = DxOISP_DIGZOOM_STEP_Q16;

	pstShotModeCap->iMinFpsQ16      = DxOISP_MIN_FPS_Q16;
	pstShotModeCap->iMaxFpsQ16      = DxOISP_MAX_FPS_Q16;

	pstShotModeCap->iSupportedStillSubModeCnt = 3;
	pstShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
	pstShotModeCap->eSupportedStillSubMode[1] = CAM_STILLSUBMODE_BURST;
	pstShotModeCap->eSupportedStillSubMode[2] = CAM_STILLSUBMODE_ZSL;

	pstShotModeCap->iMaxBurstCnt = DxOISP_NB_MAX_CAPTURE;

	pstShotModeCap->iMaxZslDepth = DxOISP_NB_MAX_CAPTURE - 1;

	pstShotModeCap->iSupportedVideoSubModeCnt = 4;
	pstShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;
	pstShotModeCap->eSupportedVideoSubMode[1] = CAM_VIDEOSUBMODE_STABILIZER;
	pstShotModeCap->eSupportedVideoSubMode[2] = CAM_VIDEOSUBMODE_TNR;
	pstShotModeCap->eSupportedVideoSubMode[3] = CAM_VIDEOSUBMODE_STABILIZEDTNR;

	pstShotModeCap->iSupportedBdFltModeCnt = 4;
	pstShotModeCap->eSupportedBdFltMode[0] = CAM_BANDFILTER_AUTO;
	pstShotModeCap->eSupportedBdFltMode[1] = CAM_BANDFILTER_OFF;
	pstShotModeCap->eSupportedBdFltMode[2] = CAM_BANDFILTER_50HZ;
	pstShotModeCap->eSupportedBdFltMode[3] = CAM_BANDFILTER_60HZ;

	pstShotModeCap->iSupportedExpModeCnt = 4;
	pstShotModeCap->eSupportedExpMode[0] = CAM_EXPOSUREMODE_AUTO;
	pstShotModeCap->eSupportedExpMode[1] = CAM_EXPOSUREMODE_APERTUREPRIOR;
	pstShotModeCap->eSupportedExpMode[2] = CAM_EXPOSUREMODE_SHUTTERPRIOR;
	pstShotModeCap->eSupportedExpMode[3] = CAM_EXPOSUREMODE_MANUAL;

	return CAM_ERROR_NONE;
}

CAM_Error isp_config_output( void *hIspHandle, _CAM_ImageInfo *pPortConfig, _CAM_IspPort ePortID, CAM_CaptureState eToState )
{
	_CHECK_BAD_POINTER( hIspHandle );

	_CAM_DxOIspState  *pIspState  = (_CAM_DxOIspState*)hIspHandle;
	ts_DxOIspCmd      *pDxOCmd    = &pIspState->stDxOIspCmd;
	CAM_Int32u        i           = 0;
	CAM_Error         error;
	_CAM_IspDmaFormat stDmaFormat;

	if ( ISP_PORT_DISPLAY == ePortID )
	{
		// configure isp
		pDxOCmd->stSync.stControl.stOutputDisplay.usSizeX = (CAM_Int16u)pPortConfig->iWidth;
		pDxOCmd->stSync.stControl.stOutputDisplay.usSizeY = (CAM_Int16u)pPortConfig->iHeight;
		for ( i = 0; i < SOCISP_LEN_FMTMAP; i++ )
		{
			if ( gImgFmtMap[i].eCEFormat == pPortConfig->eFormat )
			{
				pDxOCmd->stSync.stControl.stOutputDisplay.eFormat = gImgFmtMap[i].eIspFormat;
				pDxOCmd->stSync.stControl.stOutputDisplay.eOrder  = gImgFmtMap[i].eIspOrder;
				break;
			}
		}
		ASSERT( i < SOCISP_LEN_FMTMAP );

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.stOutputDisplay ),sizeof( pDxOCmd->stSync.stControl.stOutputDisplay ), &pDxOCmd->stSync.stControl.stOutputDisplay );
		ThreadSafe_DxOISP_CommandGroupClose();

		stDmaFormat.eFormat        = pPortConfig->eFormat;
		stDmaFormat.iHeight        = pPortConfig->iHeight;
		stDmaFormat.iWidth         = pPortConfig->iWidth;

		error = ispdma_set_output_format( pIspState->hIspDmaHandle, &stDmaFormat, ePortID );
		ASSERT_CAM_ERROR( error );
	}
	else if ( ISP_PORT_CODEC == ePortID )
	{
		ts_DxOIspSyncCmd stTmpSync;

		// configure isp
		pDxOCmd->stSync.stControl.stOutputImage.usSizeX = pPortConfig->iWidth;
		pDxOCmd->stSync.stControl.stOutputImage.usSizeY = pPortConfig->iHeight;
		for ( i = 0; i < SOCISP_LEN_FMTMAP; i++ )
		{
			if ( gImgFmtMap[i].eCEFormat == pPortConfig->eFormat )
			{
				pDxOCmd->stSync.stControl.stOutputImage.eFormat = gImgFmtMap[i].eIspFormat;
				pDxOCmd->stSync.stControl.stOutputImage.eOrder  = gImgFmtMap[i].eIspOrder;
				break;
			}
		}
		ASSERT( i < SOCISP_LEN_FMTMAP );

		if ( CAM_CAPTURESTATE_VIDEO == eToState )
		{
			for ( i = 0; i < SOCISP_MAX_VIDEO_SIZE; i++ )
			{
				if ( pPortConfig->iWidth == pIspState->pastResToCropMap[i].stOutputSize.iWidth &&
				     pPortConfig->iHeight== pIspState->pastResToCropMap[i].stOutputSize.iHeight )
				{
					break;
				}
			}
			ASSERT( i < SOCISP_MAX_VIDEO_SIZE );

			_isp_update_view( pDxOCmd, pIspState->pastResToCropMap[i].stCropSize.iWidth,
			                  pIspState->pastResToCropMap[i].stCropSize.iHeight,
			                  pIspState->iDigZoomQ16,
			                  pIspState->pastResToCropMap[i].ucUpScaleTolerance, CAM_FALSE );
		}
		else
		{
			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.stOutputImage ), sizeof( pDxOCmd->stSync.stControl.stOutputImage ), &pDxOCmd->stSync.stControl.stOutputImage );
			ThreadSafe_DxOISP_CommandGroupClose();
		}

		// if goto VIDEO/STILL mode, update dma output format
		if ( CAM_CAPTURESTATE_VIDEO == eToState || CAM_CAPTURESTATE_STILL == eToState )
		{
			if ( eToState == CAM_CAPTURESTATE_STILL )
			{
				pIspState->eStillFormat    = pPortConfig->eFormat;
			}
			stDmaFormat.eFormat        = pPortConfig->eFormat;
			stDmaFormat.iHeight        = pPortConfig->iHeight;
			stDmaFormat.iWidth         = pPortConfig->iWidth;

			error = ispdma_set_output_format( pIspState->hIspDmaHandle, &stDmaFormat, ePortID );
			ASSERT_CAM_ERROR( error );
		}
	}
	else
	{
		ASSERT( 0 );
	}

	return CAM_ERROR_NONE;
}

CAM_Error isp_set_state( void *hIspHandle, CAM_CaptureState eState )
{
	_CHECK_BAD_POINTER( hIspHandle );

	_CAM_DxOIspState    *pIspState  = (_CAM_DxOIspState*)hIspHandle;
	ts_DxOIspCmd        *pDxOCmd    = &pIspState->stDxOIspCmd;
	te_DxOISPMode       eDxOIspMode;
	CAM_Bool            bIsDispStreaming, bIsCodecStreaming;
	CAM_Error           error       = CAM_ERROR_NONE;

	switch ( eState )
	{
	case CAM_CAPTURESTATE_IDLE:
		eDxOIspMode                     = DxOISP_MODE_IDLE;
		pIspState->bDisplayShouldEnable = CAM_FALSE;
		pIspState->bCodecShouldEnable   = CAM_FALSE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_TRUE;
		break;

	case CAM_CAPTURESTATE_PREVIEW:
		if ( pDxOCmd->stSync.stControl.eSubModeCapture == DxOISP_CAPTURE_ZERO_SHUTTER_LAG )
		{
			eDxOIspMode = DxOISP_MODE_PRECAPTURE;
		}
		else
		{
			eDxOIspMode = DxOISP_MODE_PREVIEW;
		}

		pIspState->bDisplayShouldEnable  = CAM_TRUE;
		pIspState->bCodecShouldEnable    = CAM_FALSE;
		bIsDispStreaming                 = CAM_TRUE;
		bIsCodecStreaming                = CAM_TRUE;
		break;

	case CAM_CAPTURESTATE_VIDEO:
		eDxOIspMode                      = DxOISP_MODE_VIDEOREC;
		pIspState->bDisplayShouldEnable  = CAM_TRUE;
		pIspState->bCodecShouldEnable    = CAM_TRUE;
		bIsDispStreaming                 = CAM_TRUE;
		bIsCodecStreaming                = CAM_TRUE;
		break;

	case CAM_CAPTURESTATE_STILL:
		eDxOIspMode                      = DxOISP_MODE_CAPTURE_A;
		pIspState->bDisplayShouldEnable  = _is_bayer_format( pIspState->eStillFormat ) ? CAM_FALSE : CAM_TRUE;
		pIspState->bCodecShouldEnable    = CAM_TRUE;
		bIsDispStreaming                 = CAM_TRUE;
		bIsCodecStreaming                = _is_bayer_format( pIspState->eStillFormat ) ? CAM_TRUE : CAM_FALSE;
		break;

	default:
		TRACE( CAM_ERROR, "Error: invalid state transition to[%d]( %s, %s, %d )\n", eState, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_STATELIMIT;
	}

	error = ispdma_get_min_bufcnt( pIspState->hIspDmaHandle, bIsDispStreaming, &pIspState->iDispMinBufCnt );
	ASSERT_CAM_ERROR( error );

	error = ispdma_get_min_bufcnt( pIspState->hIspDmaHandle, bIsCodecStreaming, &pIspState->iCodecMinBufCnt );
	ASSERT_CAM_ERROR( error );

	pIspState->eIspSavedState = eDxOIspMode;

	// if we are already in that mode, skip this state transition.
	if ( eDxOIspMode == pDxOCmd->stSync.stControl.eMode )
	{
		return error;
	}

	if ( pIspState->bCodecShouldEnable == CAM_FALSE || pIspState->iCodecBufCnt >= pIspState->iCodecMinBufCnt )
	{
		error = _isp_set_internal_state( pIspState, eDxOIspMode );
	}

	return error;
}

CAM_Error isp_enqueue_buffer( void *hIspHandle, CAM_ImageBuffer *pstImgBuf, _CAM_IspPort ePortID, CAM_Bool bKeepDispStatus )
{
	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;
	ts_DxOIspCmd     *pDxOCmd   = &pIspState->stDxOIspCmd;

	_CHECK_BAD_POINTER( hIspHandle );

	error = ispdma_enqueue_buffer( pIspState->hIspDmaHandle, pstImgBuf, ePortID );

	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	if ( ePortID == ISP_PORT_DISPLAY )
	{
		pIspState->iDispBufCnt++;

		// if we have enough buffer, stream on the ISP and DMA
		if ( pIspState->iDispBufCnt          >= pIspState->iDispMinBufCnt &&
		     pIspState->bIsDisplayOn         == CAM_FALSE                 &&
		     pIspState->bDisplayShouldEnable == CAM_TRUE                  &&
		     bKeepDispStatus == CAM_FALSE )
		{
			// STREAM ON DISPLAY port
			_isp_set_display_port_state( pIspState, CAM_FALSE );
		}
	}
	else if ( ePortID == ISP_PORT_CODEC )
	{
		pIspState->iCodecBufCnt++;
		// if we have enough buffer, stream on the ISP and DMA
		if ( pIspState->iCodecBufCnt       >= pIspState->iCodecMinBufCnt &&
		     pIspState->bIsCodecOn         == CAM_FALSE                  &&
		     pIspState->bCodecShouldEnable == CAM_TRUE )
		{
			ASSERT( pIspState->eIspSavedState != pDxOCmd->stSync.stControl.eMode );

			error = _isp_set_internal_state( pIspState, pIspState->eIspSavedState );
		}
	}

	return error;
}

CAM_Error isp_dequeue_buffer( void *hIspHandle, _CAM_IspPort ePortID, CAM_ImageBuffer **ppImgBuf, CAM_Bool *pbIsError )
{
	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;

	_CHECK_BAD_POINTER( hIspHandle );

	error = ispdma_dequeue_buffer( pIspState->hIspDmaHandle, ePortID, ppImgBuf, pbIsError );

	if ( error == CAM_ERROR_NONE )
	{
		if ( ePortID == ISP_PORT_CODEC )
		{
			CAM_Int8u ucModeState;

			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.eMode ), 1, &ucModeState );
			DxOISP_StatusGroupClose();

			if ( _is_bayer_format( pIspState->eStillFormat ) &&
			    ( ucModeState == DxOISP_MODE_CAPTURE_A || ucModeState == DxOISP_MODE_CAPTURE_B ) )
			{
				ts_SENSOR_Attr   stSensorAttr = { 0 };

				DxOISP_SensorGetAttr( pIspState->stDxOIspCmd.stSync.stControl.ucSourceSelection, &stSensorAttr );
				if ( pIspState->iSkippedStillFrames < stSensorAttr.uiStillSkipFrameNum )
				{
					// frame should be skipped
					error = ispdma_enqueue_buffer( pIspState->hIspDmaHandle, *ppImgBuf, ePortID );
					ASSERT( error == CAM_ERROR_NONE );

					*ppImgBuf  = NULL;
					*pbIsError = CAM_TRUE;
					pIspState->iSkippedStillFrames++;

					return error;
				}
			}

			// log the shotting condition if necessary
			if ( (*ppImgBuf)->bEnableShotInfo )
			{
				// get statics from FW
				DxOISP_StatusGroupOpen();
				DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usAnalogGain),
				                  sizeof(uint16_t)*4, (uint8_t*)((*ppImgBuf)->stRawShotInfo.usAGain) );
				DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usDigitalGain),
				                  sizeof(uint16_t)*4, (uint8_t*)((*ppImgBuf)->stRawShotInfo.usDGain) );
				DxOISP_StatusGet(DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.usIspDigitalGain),
				                  sizeof(uint16_t), (uint8_t*)&((*ppImgBuf)->stRawShotInfo.usIspGain) );
				DxOISP_StatusGet(DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.eSelectedPreset),
				                  sizeof(uint8_t), (uint8_t*)&((*ppImgBuf)->stRawShotInfo.ucAWBPreset) );
				DxOISP_StatusGet(DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.usWbRedScale),
				                  sizeof(uint16_t), (uint8_t*)&((*ppImgBuf)->stRawShotInfo.usWbRedScale) );
				DxOISP_StatusGet(DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAwb.usWbBlueScale),
				                  sizeof(uint16_t), (uint8_t*)&((*ppImgBuf)->stRawShotInfo.usWbBlueScale) );
				DxOISP_StatusGet(DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAe.uiExposureTime),
				                  sizeof(uint32_t), (uint8_t*)&((*ppImgBuf)->stShotInfo.iExposureTimeQ16) );
				DxOISP_StatusGroupClose();
			}

			pIspState->iCodecBufCnt--;
		}
		else if ( ePortID == ISP_PORT_DISPLAY )
		{
			pIspState->iDispBufCnt--;
		}

		ASSERT( pIspState->iCodecBufCnt >= 0 &&  pIspState->iDispBufCnt >= 0 );
	}

	return error;
}

CAM_Error isp_poll_buffer( void *hIspHandle, CAM_Int32s iTimeout, CAM_Int32s iRequest, CAM_Int32s *piResult )
{
	_CHECK_BAD_POINTER( hIspHandle );

	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;

	if ( !pIspState->bIsIspOK )
	{
		*piResult = 0;
		TRACE( CAM_ERROR, "Error: poll buffer failed by ISP hang, please reopen camera engine to recover ( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_FATALERROR;
	}

	error = ispdma_poll_buffer( pIspState->hIspDmaHandle, iTimeout, iRequest, piResult );

	return error;
}

CAM_Error isp_flush_buffer( void *hIspHandle, _CAM_IspPort ePortID )
{
	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;
	ts_DxOIspCmd     *pDxOCmd   = &pIspState->stDxOIspCmd;
	CAM_Int32s       iRet, iLoopCnt;
	CAM_Int8u        ucDisplayStatus;
	CAM_Bool         bStreamOffCodec = CAM_FALSE;
	CAM_Bool         bStreamOffDisp  = CAM_FALSE;

	_CHECK_BAD_POINTER( hIspHandle );

	if ( ( ePortID & ISP_PORT_DISPLAY ) != 0 && pIspState->bIsDisplayOn == CAM_TRUE )
	{
		bStreamOffDisp         = CAM_TRUE;
		pIspState->iDispBufCnt = 0;
	}
	if ( ( ePortID & ISP_PORT_CODEC ) != 0 && pIspState->bIsCodecOn == CAM_TRUE )
	{
		bStreamOffCodec         = CAM_TRUE;
		pIspState->iCodecBufCnt = 0;
	}

	// TODO: flush in STILL mode?
	if ( pDxOCmd->stSync.stControl.eMode == DxOISP_MODE_CAPTURE_A ||
	     pDxOCmd->stSync.stControl.eMode == DxOISP_MODE_CAPTURE_B )
	{
		bStreamOffDisp  = CAM_FALSE;
		bStreamOffCodec = CAM_FALSE;
	}

	if ( bStreamOffCodec == CAM_TRUE )
	{
		pDxOCmd->stSync.stControl.eMode = DxOISP_MODE_PREVIEW;
		_isp_set_internal_state( pIspState, pDxOCmd->stSync.stControl.eMode );
	}
	else if ( bStreamOffDisp == CAM_TRUE )
	{
		_isp_set_display_port_state( pIspState, CAM_TRUE );
	}

	error = ispdma_flush_buffer( pIspState->hIspDmaHandle, ePortID );

	return error;
}

CAM_Error isp_get_bufreq( void *hIspHandle, _CAM_ImageInfo *pstPortConfig, CAM_ImageBufferReq *pstBufReq )
{
	_CHECK_BAD_POINTER( hIspHandle );

	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;

	error = ispdma_get_bufreq( pIspState->hIspDmaHandle, pstPortConfig, pstBufReq );

	return error;
}

CAM_Error isp_get_shotparams( void *hIspHandle, CAM_Command eCmd, CAM_Param param1, CAM_Param param2 )
{
	_CHECK_BAD_POINTER( hIspHandle );

	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;

	switch ( eCmd )
	{
	case CAM_CMD_GET_FRAMERATE:
		{
			CAM_Int16u usFrameRate;
			CAM_Int8u  eMode;

			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.eMode ), sizeof( CAM_Int8u ), &eMode );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.usFrameRate ), sizeof( CAM_Int16u ), &usFrameRate );
			DxOISP_StatusGroupClose();

			if ( eMode == DxOISP_MODE_STANDBY || eMode == DxOISP_MODE_IDLE )
			{
				usFrameRate = pIspState->stDxOIspCmd.stSync.stControl.usFrameRate;
			}

			// The returned value must be Q16
			*(CAM_Int32s *)param1 = ((CAM_Int32s)usFrameRate) << ( 16 - DxOISP_FPS_FRAC_PART );
		}
		break;
	// FIXME: add more command support
	default:
		TRACE( CAM_ERROR, "Error: soc-isp don't support command[%d] ( %s, %s, %d )\n", eCmd, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
		break;
	}

	return CAM_ERROR_NONE;
}

CAM_Error isp_set_shotparams( void *hIspHandle, CAM_Command eCmd, CAM_Param param1, CAM_Param param2 )
{
	_CHECK_BAD_POINTER( hIspHandle );

	CAM_Error        error      = CAM_ERROR_NONE;
	_CAM_DxOIspState *pIspState = (_CAM_DxOIspState*)hIspHandle;
	ts_DxOIspCmd     *pDxOCmd   = &pIspState->stDxOIspCmd;

	switch ( eCmd )
	{
#if 0
	case CAM_CMD_SET_SENSOR_ORIENTATION:
		switch ( (CAM_FlipRotate)param1 )
		{
			case CAM_FLIPROTATE_NORMAL:
				pDxOCmd->stSync.stControl.eImageOrientation = DxOISP_FLIP_OFF_MIRROR_OFF;
				break;

			case CAM_FLIPROTATE_HFLIP:
				pDxOCmd->stSync.stControl.eImageOrientation = DxOISP_FLIP_OFF_MIRROR_ON;
				break;

			case CAM_FLIPROTATE_VFLIP:
				pDxOCmd->stSync.stControl.eImageOrientation = DxOISP_FLIP_ON_MIRROR_OFF;
				break;

			case CAM_FLIPROTATE_180:
				pDxOCmd->stSync.stControl.eImageOrientation = DxOISP_FLIP_ON_MIRROR_ON;
				break;

			default:
				TRACE( CAM_ERROR, "Error: dxo-isp don't support output rotation[%d]( %s, %s, %d )\n", (CAM_Int32u)param1, __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
				break;
		}

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eImageOrientation ),
		                   sizeof( pDxOCmd->stSync.stControl.eImageOrientation ),
		                   &pDxOCmd->stSync.stControl.eImageOrientation );
		ThreadSafe_DxOISP_CommandGroupClose();
		break;
#endif
	case CAM_CMD_SET_OPTZOOM:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_DIGZOOM:
		{
			CAM_Int32s iDigZoomQ16Set = (CAM_Int32s)param1;
			CAM_Int16u usDispXStart, usDispYStart, usDispXEnd, usDispYEnd;
			CAM_Int16u usOrigCropWidth, usOrigCropHeight;
			CAM_Int16u usSizeX, usSizeY;
			CAM_Int8u  ucUpScale;

			// get crop status
			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.stCrop.usXAddrStart),
			                  sizeof( CAM_Int16u ),
			                  &usDispXStart );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.stCrop.usYAddrStart),
			                  sizeof( CAM_Int16u ),
			                  &usDispYStart );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.stCrop.usXAddrEnd),
			                  sizeof( CAM_Int16u ),
			                  &usDispXEnd );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.stCrop.usYAddrEnd),
			                  sizeof( CAM_Int16u ),
			                  &usDispYEnd );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.usSizeX),
			                  sizeof( CAM_Int16u ),
			                  &usSizeX );
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stOutputDisplay.usSizeY),
			                  sizeof( CAM_Int16u ),
			                  &usSizeY );
			DxOISP_StatusGroupClose();

			usOrigCropWidth = (( usDispXEnd - usDispXStart + 1) * pIspState->iDigZoomQ16) >> 16;
			usOrigCropHeight= (( usDispYEnd - usDispYStart + 1) * pIspState->iDigZoomQ16) >> 16;

			_isp_calc_upscaletolerance( &ucUpScale, usOrigCropWidth, usOrigCropHeight, usSizeX, usSizeY );

			_isp_update_view( pDxOCmd, usOrigCropWidth, usOrigCropHeight, iDigZoomQ16Set, ucUpScale, CAM_TRUE );

			pIspState->iDigZoomQ16 = iDigZoomQ16Set;
		}
		break;

	case CAM_CMD_SET_EXPOSUREMODE:
		{
			CAM_ExposureMode eExpMode = (CAM_ExposureMode)param1;

			switch ( eExpMode )
			{
			case CAM_EXPOSUREMODE_AUTO:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_AUTO;
				break;

			case CAM_EXPOSUREMODE_APERTUREPRIOR:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_APERTURE_PRIORITY;
				break;

			case CAM_EXPOSUREMODE_SHUTTERPRIOR:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_SHUTTER_SPEED_PRIORITY;
				break;

			case CAM_EXPOSUREMODE_PROGRAM:
				// FIXME: how to mapping?
				break;

			case CAM_EXPOSUREMODE_MANUAL:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				break;

			default:
				TRACE( CAM_ERROR, "Error: invalid auto exposure mode[%d] ( %s, %s, %d )\n", eExpMode, __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
				break;
			}

			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stCameraControl.stAe.eMode ),
			                   sizeof( pDxOCmd->stSync.stCameraControl.stAe.eMode ),
			                   &pDxOCmd->stSync.stCameraControl.stAe.eMode );
			ThreadSafe_DxOISP_CommandGroupClose();
		}
		break;

	case CAM_CMD_SET_BANDFILTER:
		{
			CAM_BandFilterMode eBandFilter = (CAM_BandFilterMode)param1;
			switch ( eBandFilter )
			{
			case CAM_BANDFILTER_AUTO:
				pDxOCmd->stSync.stCameraControl.stAfk.eMode = DxOISP_AFK_MODE_AUTO;
				break;

			case CAM_BANDFILTER_OFF:
				pDxOCmd->stSync.stCameraControl.stAfk.eMode = DxOISP_AFK_MODE_MANUAL;
				pDxOCmd->stSync.stCameraControl.stAfk.stForced.eFrequency = DxOISP_AFK_FREQ_0HZ;
				break;

			case CAM_BANDFILTER_50HZ:
				pDxOCmd->stSync.stCameraControl.stAfk.eMode = DxOISP_AFK_MODE_MANUAL;
				pDxOCmd->stSync.stCameraControl.stAfk.stForced.eFrequency = DxOISP_AFK_FREQ_50HZ;
				break;

			case CAM_BANDFILTER_60HZ:
				pDxOCmd->stSync.stCameraControl.stAfk.eMode = DxOISP_AFK_MODE_MANUAL;
				pDxOCmd->stSync.stCameraControl.stAfk.stForced.eFrequency = DxOISP_AFK_FREQ_60HZ;
				break;

			default:
				TRACE( CAM_ERROR, "Error: invalid banding filter[%d] ( %s, %s, %d )\n", eBandFilter, __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
				break;
			}

			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stCameraControl.stAfk.eMode ),
			                   sizeof( pDxOCmd->stSync.stCameraControl.stAfk.eMode ),
			                   &pDxOCmd->stSync.stCameraControl.stAfk.eMode );
			DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stCameraControl.stAfk.stForced.eFrequency ),
			                   sizeof( pDxOCmd->stSync.stCameraControl.stAfk.stForced.eFrequency ),
			                   &pDxOCmd->stSync.stCameraControl.stAfk.stForced.eFrequency );
			ThreadSafe_DxOISP_CommandGroupClose();
		}
		break;

	case CAM_CMD_SET_SHOTMODE:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTSUPPORTEDARG ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_FOCUSMODE:
		{
			CAM_FocusMode eFocusMode = (CAM_FocusMode)param1;
			switch ( eFocusMode )
			{
			case CAM_FOCUS_MANUAL:
				pDxOCmd->stSync.stCameraControl.stAf.eMode = DxOISP_AF_MODE_MANUAL;

				// DxOISP_StatusGroupOpen();
				// DxOISP_StatusGet( DxOISP_STATUS_OFFSET(stSync.stImageInfo.stAf.usPosition), sizeof(uint16_t), (uint16_t*)&pDxOCmd->stSync.stCameraControl.stAf.stForced.usPosition );
				// DxOISP_StatusGroupClose();

				pDxOCmd->stSync.stCameraControl.stAf.stForced.usPosition = 530;
				CELOG( "AF position: %d dioptry\n", pDxOCmd->stSync.stCameraControl.stAf.stForced.usPosition );

				DxOISP_CommandGroupOpen();
				DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAf),sizeof(pDxOCmd->stSync.stCameraControl.stAf), &pDxOCmd->stSync.stCameraControl.stAf );
				DxOISP_CommandGroupClose();
				break;

			case CAM_FOCUS_AUTO_ONESHOT:
				pDxOCmd->stSync.stCameraControl.stAf.eMode = DxOISP_AF_MODE_AUTO;

				DxOISP_CommandGroupOpen();
				DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAf),sizeof(pDxOCmd->stSync.stCameraControl.stAf), &pDxOCmd->stSync.stCameraControl.stAf );
				DxOISP_CommandGroupClose();
				break;

			default:
				break;
			}
		}
		break;

	case CAM_CMD_SET_VIDEO_SUBMODE:
		{
			CAM_VideoSubMode eVideoSubMode  = ( CAM_VideoSubMode )param1;
			CAM_Int8u        ucModeState     = pDxOCmd->stSync.stControl.eMode;
			CAM_Int32s       i;

			switch ( eVideoSubMode )
			{
				case CAM_VIDEOSUBMODE_SIMPLE:
					pDxOCmd->stSync.stControl.eSubModeVideoRec    = DxOISP_VR_SIMPLE;
					break;

				case CAM_VIDEOSUBMODE_STABILIZER:
					pDxOCmd->stSync.stControl.eSubModeVideoRec    = DxOISP_VR_STABILIZED;
					break;
				case CAM_VIDEOSUBMODE_TNR:
					pDxOCmd->stSync.stControl.eSubModeVideoRec    = DxOISP_VR_TEMPORAL_NOISE;
					break;
				case CAM_VIDEOSUBMODE_STABILIZEDTNR:
					pDxOCmd->stSync.stControl.eSubModeVideoRec    = DxOISP_VR_STABILIZED_TEMPORAL_NOISE;
					break;
				default:
					TRACE( CAM_ERROR, "Error: invalid video sub mode[%d] ( %s, %s, %d )\n", eVideoSubMode, __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_NOTSUPPORTEDARG;
					break;
			}

			DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.eSubModeVideoRec), sizeof(pDxOCmd->stSync.stControl.eSubModeVideoRec), &pDxOCmd->stSync.stControl.eSubModeVideoRec );
			DxOISP_CommandGroupClose();

			// If we are not in VIDEO mode, we just set VIDEO_SUBMODE to isp and it should auto take effect in coming up VIDEO mode.
			if ( ucModeState == DxOISP_MODE_VIDEOREC )
			{
				_isp_set_internal_state( pIspState, DxOISP_MODE_VIDEOREC );
			}
		}
		break;
	case CAM_CMD_SET_STILL_SUBMODE:
		{
			CAM_StillSubMode      eStillSubMode       = ( CAM_StillSubMode )param1;
			CAM_StillSubModeParam stStillSubModeParam = *( CAM_StillSubModeParam * )param2;
			CAM_Int8u             ucMode              = pDxOCmd->stSync.stControl.eMode;
			CAM_Int32s            i;

			switch ( eStillSubMode )
			{
				case CAM_STILLSUBMODE_SIMPLE:
					if ( stStillSubModeParam.iBurstCnt != 1 || stStillSubModeParam.iZslDepth != 0 )
					{
						TRACE( CAM_ERROR, "Error: invalid argument[%d, %d] for CAM_STILLSUBMODE_SIMPLE. ( %s, %s, %d )\n",
						       stStillSubModeParam.iBurstCnt, stStillSubModeParam.iZslDepth, __FILE__, __FUNCTION__, __LINE__ );
						return CAM_ERROR_NOTSUPPORTEDARG;
					}
					pDxOCmd->stSync.stControl.eSubModeCapture  = DxOISP_CAPTURE_BURST;
					pDxOCmd->stSync.stControl.ucNbCaptureFrame = 1;
					break;
				case CAM_STILLSUBMODE_BURST:
					if ( stStillSubModeParam.iBurstCnt < 1 || stStillSubModeParam.iBurstCnt > DxOISP_NB_MAX_CAPTURE || stStillSubModeParam.iZslDepth != 0 )
					{
						TRACE( CAM_ERROR, "Error: invalid argument[%d, %d] for CAM_STILLSUBMODE_BURST. ( %s, %s, %d )\n",
						       stStillSubModeParam.iBurstCnt, stStillSubModeParam.iZslDepth, __FILE__, __FUNCTION__, __LINE__ );
						return CAM_ERROR_NOTSUPPORTEDARG;
					}
					pDxOCmd->stSync.stControl.eSubModeCapture  = DxOISP_CAPTURE_BURST;
					pDxOCmd->stSync.stControl.ucNbCaptureFrame = stStillSubModeParam.iBurstCnt;
					break;
				case CAM_STILLSUBMODE_ZSL:
					if ( stStillSubModeParam.iBurstCnt != 1 || stStillSubModeParam.iZslDepth < 1 || stStillSubModeParam.iZslDepth > DxOISP_NB_MAX_CAPTURE )
					{
						TRACE( CAM_ERROR, "Error: invalid argument[%d, %d] for CAM_STILLSUBMODE_ZSL. ( %s, %s, %d )\n",
						       stStillSubModeParam.iBurstCnt, stStillSubModeParam.iZslDepth, __FILE__, __FUNCTION__, __LINE__ );
						return CAM_ERROR_NOTSUPPORTEDARG;
					}
					pDxOCmd->stSync.stControl.eSubModeCapture  = DxOISP_CAPTURE_ZERO_SHUTTER_LAG;
					pDxOCmd->stSync.stControl.ucNbCaptureFrame = stStillSubModeParam.iZslDepth + 1;
					break;
				case CAM_STILLSUBMODE_HDR:
				default:
					TRACE( CAM_ERROR, "Error: invalid still sub mode[%d] ( %s, %s, %d )\n", eStillSubMode, __FILE__, __FUNCTION__, __LINE__ );
					return CAM_ERROR_NOTSUPPORTEDARG;
					break;
			}

			DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.eSubModeCapture), sizeof(uint8_t), &pDxOCmd->stSync.stControl.eSubModeCapture );
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.ucNbCaptureFrame), sizeof(uint8_t), &pDxOCmd->stSync.stControl.ucNbCaptureFrame );
			DxOISP_CommandGroupClose();

			if ( ucMode == DxOISP_MODE_PREVIEW || ucMode == DxOISP_MODE_PRECAPTURE )
			{
				if ( eStillSubMode == CAM_STILLSUBMODE_ZSL )
				{
					// enable ZSL feature
					_isp_set_internal_state( pIspState, DxOISP_MODE_PRECAPTURE );
				}
				else if ( ucMode == DxOISP_MODE_PRECAPTURE )
				{
					// disable ZSL feature
					_isp_set_internal_state( pIspState, DxOISP_MODE_PREVIEW );
				}
			}
		}
		break;

	case CAM_CMD_SET_FRAMERATE:
		{
			CAM_Int16u  usNewFrameRate = (CAM_Int16u)(((CAM_Int32s)param1) >> (16 - DxOISP_FPS_FRAC_PART));

			// we need update MinInterFrameTime according to new frame rate
			pDxOCmd->stAsync.stSystem.usMinInterframeTime = pDxOCmd->stAsync.stSystem.usMinInterframeTime * usNewFrameRate / pDxOCmd->stSync.stControl.usFrameRate;

			// input parameter is Q16, so, we need right shift by 16 and left shift by DxOISP_FPS_FRAC_PART
			pDxOCmd->stSync.stControl.usFrameRate = usNewFrameRate;
			DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.usFrameRate), sizeof(pDxOCmd->stSync.stControl.usFrameRate), &pDxOCmd->stSync.stControl.usFrameRate );
			DxOISP_CommandGroupClose();

			_isp_set_min_interframe_time( CAM_FALSE, pDxOCmd );
		}
		break;
/*
	case CAM_CMD_SET_EXPOSUREMETERMODE:
		DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerWidth ),
		                  sizeof( CAM_Int16u ),
		                  &usSensorWidth );
		DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerHeight ),
		                  sizeof( CAM_Int16u ),
		                  &usSensorHeight );
		DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.eImageType ),
		                  sizeof( CAM_Int8u ),
		                  &sensorImgType );

		_calc_pixpersample( sensorImgType, &pixPerBayer_H, &pixPerBayer_V );


		switch ( (CAM_ExposureMeterMode)param1 )
		{
		case CAM_EXPOSUREMETERMODE_MEAN:
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrStart = 0;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrEnd   = usSensorWidth * pixPerBayer_H | 1;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrStart = 0;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrEnd   = usSensorHeight * pixPerBayer_V | 1;
			pDxOCmd->stSync.stCameraControl.stROI.ucWeights[0] = 1;
			for ( i = 1; i < 6; i++ )
			{
				pDxOCmd->stSync.stCameraControl.stROI.ucWeights[i] = 0;
			}

			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stROI),
			                   sizeof(pDxOCmd->stSync.stCameraControl.stROI),
			                   &pDxOCmd->stSync.stCameraControl.stROI );
			ThreadSafe_DxOISP_CommandGroupClose();

			break;

		case CAM_EXPOSUREMETERMODE_CENTRALWEIGHTED:
			// weight 1
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrStart = 0;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrEnd   = usSensorWidth * pixPerBayer_H | 1;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrStart = 0;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrEnd   = usSensorHeight * pixPerBayer_V | 1;
			pDxOCmd->stSync.stCameraControl.stROI.ucWeights[0] = 1;
			// weight 2
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[1].usXAddrStart = (usSensorWidth * pixPerBayer_H >> 2) & (~1);
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[1].usXAddrEnd   = (usSensorWidth * pixPerBayer_H * 3 >> 2) | 1;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[1].usYAddrStart = (usSensorHeight * pixPerBayer_V >> 2) & (~1);
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[1].usYAddrEnd   = ((usSensorHeight * pixPerBayer_V) * 3 >> 2) | 1;
			pDxOCmd->stSync.stCameraControl.stROI.ucWeights[1] = 2;
			// weight 3
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[2].usXAddrStart = (usSensorWidth * pixPerBayer_H * 3 >> 3) & (~1);
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[2].usXAddrEnd   = (usSensorWidth * pixPerBayer_H * 5 >> 3) | 1;
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[2].usYAddrStart = (usSensorHeight * pixPerBayer_V * 3 >> 3) & (~1);
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[2].usYAddrEnd   = (usSensorHeight * pixPerBayer_V * 5 >> 3) | 1;
			pDxOCmd->stSync.stCameraControl.stROI.ucWeights[2] = 3;

			for ( i = 3; i < 6; i++ )
			{
				pDxOCmd->stSync.stCameraControl.stROI.ucWeights[i] = 0;
			}

			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stROI),
			                   sizeof(pDxOCmd->stSync.stCameraControl.stROI),
			                   &pDxOCmd->stSync.stCameraControl.stROI );
			ThreadSafe_DxOISP_CommandGroupClose();

			break;

		case CAM_EXPOSUREMETERMODE_SPOT:
			_calc_pixpersample( sensorImgType, &pixPerBayer_H, &pixPerBayer_V );

			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrStart = ( usSensorWidth * pixPerBayer_H * 21845 >> 16 );
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrEnd   = ( pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usXAddrStart << 1 );
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrStart = ( usSensorHeight * pixPerBayer_V * 21845 >> 16 );
			pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrEnd   = ( pDxOCmd->stSync.stCameraControl.stROI.stFaces[0].usYAddrStart << 1 );
			pDxOCmd->stSync.stCameraControl.stROI.ucWeights[0] = 1;
			for ( i = 1; i < 6; i++ )
			{
				pDxOCmd->stSync.stCameraControl.stROI.ucWeights[i] = 0;
			}

			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stROI),
			                   sizeof(pDxOCmd->stSync.stCameraControl.stROI),
			                   &pDxOCmd->stSync.stCameraControl.stROI );
			ThreadSafe_DxOISP_CommandGroupClose();

			break;

		case CAM_EXPOSUREMETERMODE_MATRIX:
			break;

		default:
			TRACE( CAM_ERROR, "CAM_ERROR_NOTSUPPORTEDARG ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDARG;
			break;
		}
		break;

	case CAM_CMD_SET_ISO:
		{
			CAM_ISOMode eISOMode = (CAM_ISOMode)param1;
			CAM_Int32s  i        = 0;
			switch ( eISOMode )
			{
			case CAM_ISO_AUTO:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_AUTO;
				break;

			case CAM_ISO_50:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x80;
				}
				break;

			case CAM_ISO_100:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x100;
				}
				break;

			case CAM_ISO_200:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x200;
				}
				break;

			case CAM_ISO_400:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x400;
				}
				break;

			case CAM_ISO_800:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x800;
				}
				break;

			case CAM_ISO_1600:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x1000;
				}
				break;

			case CAM_ISO_3200:
				pDxOCmd->stSync.stCameraControl.stAe.eMode = DxOISP_AE_MODE_MANUAL;
				for ( i = 0; i < 4; i++ )
				{
					pDxOCmd->stSync.stCameraControl.stAe.stForced.usAnalogGain[i] = 0x2000;
				}
				break;

			default:
				TRACE( CAM_ERROR, "Error: soc-isp don't support ISO mode[%d] ( %s, %s, %d )\n", eISOMode, __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		break;

	case CAM_CMD_SET_EVCOMPENSATION:
		if ( pDxOCmd->stSync.stCameraControl.stAe.eMode == DxOISP_AE_MODE_MANUAL )
		{
			TRACE( CAM_ERROR, "CAM_ERROR_NOTSUPPORTEDCMD ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}

		pDxOCmd->stSync.stCameraControl.stAe.stControl.cEvBias = 0;
		pDxOCmd->stSync.stCameraControl.stAe.stControl.cEvBias |= ((((CAM_Int32s)param1) & 0xf800)  >> 11);
		pDxOCmd->stSync.stCameraControl.stAe.stControl.cEvBias |= ((((CAM_Int32s)param1) & 0x30000) >> 11);

		ThreadSafe_DxOISP_CommandGroupOpen();
		DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAe.stControl.cEvBias),
		                   sizeof(pDxOCmd->stSync.stCameraControl.stAe.stControl.cEvBias),
		                   &pDxOCmd->stSync.stCameraControl.stAe.stControl.cEvBias );
		ThreadSafe_DxOISP_CommandGroupClose();
		break;

	case CAM_CMD_SET_SHUTTERSPEED:
		if ( pDxOCmd->stSync.stCameraControl.stAe.eMode == DxOISP_AE_MODE_MANUAL ||
		     pDxOCmd->stSync.stCameraControl.stAe.eMode == DxOISP_AE_MODE_SHUTTER_SPEED_PRIORITY )
		{
			pDxOCmd->stSync.stCameraControl.stAe.stForced.uiExposureTime = (CAM_Int32u)param1;
			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAe.stForced.uiExposureTime),
			                   sizeof(pDxOCmd->stSync.stCameraControl.stAe.stForced.uiExposureTime),
			                   &pDxOCmd->stSync.stCameraControl.stAe.stForced.uiExposureTime );
			ThreadSafe_DxOISP_CommandGroupClose();
		}
		else
		{
			TRACE( CAM_ERROR, "CAM_ERROR_NOTSUPPORTEDCMD ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}
		break;

	case CAM_CMD_SET_FNUM:
		if ( pDxOCmd->stSync.stCameraControl.stAe.eMode == DxOISP_AE_MODE_MANUAL ||
		     pDxOCmd->stSync.stCameraControl.stAe.eMode == DxOISP_AE_MODE_APERTURE_PRIORITY )
		{
			pDxOCmd->stSync.stCameraControl.stAe.stForced.usAperture = (((CAM_Int32u)param1 & 0xffff00) >> 8);
			ThreadSafe_DxOISP_CommandGroupOpen();
			DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stCameraControl.stAe.stForced.usAperture ),
			                   sizeof( pDxOCmd->stSync.stCameraControl.stAe.stForced.usAperture ),
			                   &pDxOCmd->stSync.stCameraControl.stAe.stForced.usAperture );
			ThreadSafe_DxOISP_CommandGroupClose();
		}
		else
		{
			TRACE( CAM_ERROR, "CAM_ERROR_NOTSUPPORTEDCMD ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}
		break;

	case CAM_CMD_SET_WHITEBALANCEMODE:
		switch ( (CAM_WhiteBalanceMode)param1 )
		{
		case CAM_WHITEBALANCEMODE_AUTO:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_AUTO;
			break;

		case CAM_WHITEBALANCEMODE_DAYLIGHT:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_DAYLIGHT;
			break;

		case CAM_WHITEBALANCEMODE_CLOUDY:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_CLOUDY;
			break;

		case CAM_WHITEBALANCEMODE_INCANDESCENT:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_TUNGSTEN;
			break;

		case CAM_WHITEBALANCEMODE_SHADOW:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_SHADE;
			break;


		case CAM_WHITEBALANCEMODE_FLUORESCENT1:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_TL84;
			break;

		case CAM_WHITEBALANCEMODE_FLUORESCENT2:
		case CAM_WHITEBALANCEMODE_FLUORESCENT3:
			pDxOCmd->stSync.stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_PRESET;
			pDxOCmd->stSync.stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_COOLWHITE;
			break;
		}
		break;

	case CAM_CMD_SET_SATURATION:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_CONTRAST:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_SHARPNESS:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_COLOREFFECT:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_START_FOCUS:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_CANCEL_FOCUS:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_SET_FLASHMODE:
		// FIXME
		TRACE( CAM_ERROR, "CAM_ERROR_NOTIMPLEMENTED ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;
*/
	default:
		TRACE( CAM_ERROR, "Error: soc-isp don't support command[%d] ( %s, %s, %d )\n", eCmd, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
		break;
	}
	return CAM_ERROR_NONE;
}

static _CAM_ResToCropUTMap* _isp_get_imagestrategy_map( CAM_Int16u usSensorWidth, CAM_Int16u usSensorHeight,
                                                        _CAM_ImageStrategyMap *pastImageStrategyMap, CAM_Int32u uiCount )
{
	CAM_Int32u i;
	_CAM_ResToCropUTMap *pstResToCropUT = NULL;

	for ( i = 0; i < uiCount; i++ )
	{
		if ( usSensorWidth == pastImageStrategyMap[i].stSensorSize.iWidth &&
		     usSensorHeight == pastImageStrategyMap[i].stSensorSize.iHeight )
		{
			pstResToCropUT = &pastImageStrategyMap[i].astResToCropUT[0];
			break;
		}
	}

	if ( i == uiCount )
	{
		TRACE( CAM_ERROR, "Error: Sensor size[%dx%d] is not supported in image strategy table ( %s, %s, %d )\n",
		       usSensorWidth, usSensorHeight, __FILE__, __FUNCTION__, __LINE__ );
	}

	return pstResToCropUT;
}

static void  _isp_default_config( _CAM_DxOIspState *pIspState, CAM_Int32s iSensorID )
{
	// typical values are as following:
	// camera state        : idle
	// sensor id           : 0
	// pattern generator   : 0
	// display/codec height: 640
	// display/codec width : 480

	ts_DxOIspCmd *pDxOCmd = &pIspState->stDxOIspCmd;

	CAM_Int32s    i, j;
	CAM_Int8u     ucMode;
	ts_DxOISPCrop stDefaultCrop = { 0, 0, 0xffff, 0xffff };

	// asynchronous command section
	ts_DxOIspAsyncCmd *pAsyncCmd = &pDxOCmd->stAsync;
	// synchronous command section
	ts_DxOIspSyncCmd  *pSyncCmd  = &pDxOCmd->stSync;

	// default display and codec putput attributes
	CAM_Int8u eSensorDefaultRot       = DxOISP_FLIP_ON_MIRROR_ON;

	CAM_Int8u  eDefaultPreviewFmt     = DxOISP_FORMAT_YUV422;
	CAM_Int16u usDefaultPreviewWidth  = 1920;
	CAM_Int16u usDefaultPreviewHeight = 1080;

	CAM_Int8u  eDefaultCodecFmt       = DxOISP_FORMAT_YUV422;
	CAM_Int16u usDefaultCodecWidth    = 1920;
	CAM_Int16u usDefaultCodecHeight   = 1080;

	CAM_Int16u usBayerWidth, usBayerHeight;

	// system parameters
	pAsyncCmd->stSystem.uiSystemClock       = SYSTEM_FREQUENCY          << DxOISP_SYSTEM_CLOCK_FRAC_PART;
	pAsyncCmd->stSystem.usMinInterframeTime = OV_INTERFRAME_INTERVAL_1  << DxOISP_MIN_INTERFRAME_TIME_FRAC_PART;
	pAsyncCmd->stSystem.uiMaxBandwidth      = SYSTEM_MAX_BANDWIDTH      << DxOISP_MAX_BANDWIDTH_FRAC_PART; // 1000 << DxOISP_MAX_BANDWIDTH_FRAC_PART;

	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stAsync.stSystem.uiSystemClock ), sizeof( pAsyncCmd->stSystem.uiSystemClock ), (CAM_Int8u*)&pAsyncCmd->stSystem.uiSystemClock );
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stAsync.stSystem.usMinInterframeTime ), sizeof( pAsyncCmd->stSystem.usMinInterframeTime ), (CAM_Int8u*)&pAsyncCmd->stSystem.usMinInterframeTime );
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stAsync.stSystem.uiMaxBandwidth ), sizeof( uint32_t ), (CAM_Int8u*)&pAsyncCmd->stSystem.uiMaxBandwidth );

	// general commands
	pSyncCmd->stControl.ucSourceSelection          = (CAM_Int8u)iSensorID;
	pSyncCmd->stControl.eMode                      = DxOISP_MODE_IDLE;

	// Set ISP to idle so that we can get sensor capability
	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.ucSourceSelection), sizeof( pDxOCmd->stSync.stControl.ucSourceSelection ), &pDxOCmd->stSync.stControl.ucSourceSelection ) ;
	DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stControl.eMode), sizeof( pDxOCmd->stSync.stControl.eMode ), &pDxOCmd->stSync.stControl.eMode ) ;
	ThreadSafe_DxOISP_CommandGroupClose();

	_isp_wait_for_mode( pIspState, pDxOCmd->stSync.stControl.eMode );

	pSyncCmd->stControl.stTestPatternAttribute.usX = 0;
	pSyncCmd->stControl.stTestPatternAttribute.usY = 0;
	pSyncCmd->stControl.isTestPatternEnabled       = 0;
	pSyncCmd->stControl.ucNbCaptureFrame           = 1;
	pSyncCmd->stControl.eSubModeCapture            = DxOISP_CAPTURE_BURST;
	pSyncCmd->stControl.eSubModeVideoRec           = DxOISP_VR_SIMPLE;
	pSyncCmd->stControl.eImageOrientation          = eSensorDefaultRot;
	// FIXME: Here we take assumption on full FOV case, sensor output width can be only 2 cases:
	//        1. 3264 non-binning(skip) case
	//        2. 3264/2 in binning(skip) case
	pSyncCmd->stControl.ucUpscalingTolerance       = _MAX( _DIV_CEIL( 1920 << DxOISP_UPSCALING_TOLERANCE_FRAC_PART, 3264 / 2 ),
	                                                       _DIV_CEIL( 1920 << DxOISP_UPSCALING_TOLERANCE_FRAC_PART, 3264 ) );
	pSyncCmd->stControl.isDisplayDisabled          = 0;
	pSyncCmd->stControl.isRawOutputEnabled         = 0;
	pSyncCmd->stControl.usFrameRate                = ( (DxOISP_MAX_FPS_Q16>>16) << DxOISP_FPS_FRAC_PART );
	pSyncCmd->stControl.usZoomFocal                = ( 1 << DxOISP_ZOOMFOCAL_FRAC_PART );
	pSyncCmd->stControl.usFrameToFrameLimit        =  DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT;

	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerWidth ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerHeight ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerHeight );

	usBayerWidth <<= 1;
	usBayerHeight<<= 1;

	pIspState->pastResToCropMap = _isp_get_imagestrategy_map( usBayerWidth, usBayerHeight,
	                              (_CAM_ImageStrategyMap*)gIspVidSizeCap, SOCISP_MAX_SENSOR_COUNT );

	// display output
	pSyncCmd->stControl.stOutputDisplay.eFormat             = eDefaultPreviewFmt;
	pSyncCmd->stControl.stOutputDisplay.eOrder              = DxOISP_YUV422ORDER_UYVY;
	pSyncCmd->stControl.stOutputDisplay.eEncoding           = DxOISP_ENCODING_YUV_601FU;
	pSyncCmd->stControl.stOutputDisplay.usSizeX             = usDefaultPreviewWidth;
	pSyncCmd->stControl.stOutputDisplay.usSizeY             = usDefaultPreviewHeight;
	pSyncCmd->stControl.stOutputDisplay.stCrop.usXAddrStart = 0; // 672; // 664;
	pSyncCmd->stControl.stOutputDisplay.stCrop.usYAddrStart = 0; // 684; // 680;
	pSyncCmd->stControl.stOutputDisplay.stCrop.usXAddrEnd   = usBayerWidth - 1; // 2591; // 2583;
	pSyncCmd->stControl.stOutputDisplay.stCrop.usYAddrEnd   = usBayerHeight- 1; // 1763; // 1759;

	// codec output
	pSyncCmd->stControl.stOutputImage.eFormat               = eDefaultCodecFmt;
	pSyncCmd->stControl.stOutputImage.eOrder                = DxOISP_YUV422ORDER_UYVY;
	pSyncCmd->stControl.stOutputImage.eEncoding             = DxOISP_ENCODING_YUV_601FU;
	pSyncCmd->stControl.stOutputImage.usSizeX               = usDefaultCodecWidth;
	pSyncCmd->stControl.stOutputImage.usSizeY               = usDefaultCodecHeight;
	pSyncCmd->stControl.stOutputImage.stCrop.usXAddrStart   = 0; // 672; // 664;
	pSyncCmd->stControl.stOutputImage.stCrop.usYAddrStart   = 0; // 684; // 680;
	pSyncCmd->stControl.stOutputImage.stCrop.usXAddrEnd     = usBayerWidth - 1; // 2591; // 2583;
	pSyncCmd->stControl.stOutputImage.stCrop.usYAddrEnd     = usBayerHeight- 1; // 1763; // 1759;

	// exposure
	pSyncCmd->stCameraControl.stAe.eMode                         = DxOISP_AE_MODE_AUTO;
	pSyncCmd->stCameraControl.stAe.stForced.uiExposureTime       = 30 << DxOISP_EXPOSURETIME_FRAC_PART;
	pSyncCmd->stCameraControl.stAe.stControl.uiMaxExposureTime   = 35 << DxOISP_EXPOSURETIME_FRAC_PART;
	for ( i = 0; i < 4; i++ )
	{
		pSyncCmd->stCameraControl.stAe.stForced.usAnalogGain[i]  = 1 << DxOISP_GAIN_FRAC_PART;
		pSyncCmd->stCameraControl.stAe.stForced.usDigitalGain[i] = 1 << DxOISP_GAIN_FRAC_PART;
	}
	pSyncCmd->stCameraControl.stAe.stForced.usIspGain            = 1 << DxOISP_GAIN_FRAC_PART;
	pSyncCmd->stCameraControl.stAe.stControl.cEvBias             = 0 << DxOISP_EVBIAS_FRAC_PART;
	pSyncCmd->stCameraControl.stAe.stForced.usAperture           = 2007;

	// anti-flickering related
	pSyncCmd->stCameraControl.stAfk.eMode                        = DxOISP_AFK_MODE_MANUAL;
	pSyncCmd->stCameraControl.stAfk.stForced.eFrequency          = DxOISP_AFK_FREQ_50HZ;

	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync), sizeof( pDxOCmd->stSync.stControl ), &pDxOCmd->stSync.stControl ) ;
	DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAe.eMode), sizeof( uint8_t ), &pDxOCmd->stSync.stCameraControl.stAe.eMode );
	DxOISP_CommandSet( DxOISP_CMD_OFFSET(stSync.stCameraControl.stAe.stControl.uiMaxExposureTime), sizeof( uint32_t ), &pDxOCmd->stSync.stCameraControl.stAe.stControl.uiMaxExposureTime );
	ThreadSafe_DxOISP_CommandGroupClose();


/*
	// region of interest
	for ( i = 0; i < DxOISP_NB_FACES; i++ )
	{
		pSyncCmd->stCameraControl.stROI.stFaces[i] = defaultCrop;
	}
	for ( i = 0; i < DxOISP_NB_FACES; i++ )
	{
		pSyncCmd->stCameraControl.stROI.ucWeights[i] = 0;
	}
*/


/*
	// auto white-balance related
	pSyncCmd->stCameraControl.stAwb.eMode = DxOISP_AWB_MODE_AUTO;
	pSyncCmd->stCameraControl.stAwb.stForced.ePreset = DxOISP_AWB_PRESET_DAYLIGHT;
	pSyncCmd->stCameraControl.stAwb.stForced.usWbRedScale  = (1 << DxOISP_WBSCALE_FRAC_PART);
	pSyncCmd->stCameraControl.stAwb.stForced.usWbBlueScale = (1 << DxOISP_WBSCALE_FRAC_PART);

	// flash related
	pSyncCmd->stCameraControl.stFlash.eMode = DxOISP_FLASH_MODE_AUTO;
	pSyncCmd->stCameraControl.stFlash.stForced.usPower = (0 << DxOISP_FLASH_POWER_FRAC_PART);

	// digital autofocus related
	pSyncCmd->stCameraControl.stDaf.eMode = DxOISP_DAF_MODE_AUTO;
	pSyncCmd->stCameraControl.stDaf.stForced.eFocus = DxOISP_DAF_FOCUS_PORTRAIT;

	// mechanical autofocus related
	pSyncCmd->stCameraControl.stAf.eMode = DxOISP_AF_MODE_AUTO;
	pSyncCmd->stCameraControl.stAf.stForced.usPosition = (0 << DxOISP_AF_POSITION_FRAC_PART);
*/

	// asynchronous command section

/*
	// tuning parameters
	for ( i = 0; i < DxOISP_NB_COARSE_GAIN; i++ )
	{
		for ( j = 0; j < DxOISP_NB_COARSE_ILLUMINANT; j++ )
		{
			pAsyncCmd->stLensCorrection.ucShading[i][j] = 0x80;
			pAsyncCmd->stColor.cSaturationAdjustment[i][j] = 0;
			pAsyncCmd->stExposure.ucBlackPoint[i][j] = 0x80;
			pAsyncCmd->stExposure.ucWhitePoint[i][j] = 0x80;
			pAsyncCmd->stExposure.ucGamma[i][j] = 0x80;
		}
	}
	for ( i = 0; i < DxOISP_NB_DISTANCE; i++ )
	{
		for ( j = 0; j < DxOISP_NB_COARSE_ILLUMINANT; j++ )
		{
			pAsyncCmd->stLensCorrection.ucColorFringing[i][j] = 0xff;
			pAsyncCmd->stLensCorrection.ucSharpness[i][j] = 0x80;
		}
	}
	for ( i = 0; i < DxOISP_NB_EXPOSURE_TIME; i++ )
	{
		pAsyncCmd->stSensorDefectCorrection.ucBright[i] = 0x80;
		pAsyncCmd->stSensorDefectCorrection.ucDark[i] = 0x80;
	}
	for ( i = 0; i < DxOISP_NB_FINE_GAIN; i++ )
	{
		pAsyncCmd->stSensorDefectCorrection.ucLuminanceNoise[i] = 0x80;
		pAsyncCmd->stSensorDefectCorrection.ucChrominanceNoise[i] = 0x80;
		pAsyncCmd->stNoiseDetails.ucSegmentation[i] = 0x80;
		pAsyncCmd->stNoiseDetails.ucGrainSize[i] = 0x80;
	}
	for ( i = 0; i < DxOISP_NB_FINE_ILLUMINANT; i++ )
	{
		pAsyncCmd->stColor.cWbBiasRed[i] = 0;
		pAsyncCmd->stColor.cWbBiasBlue[i] = 0;
	}
	pAsyncCmd->stExposure.cToneCurveBrightness = 0;
	pAsyncCmd->stExposure.cToneCurveContrast = 0;

	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( 0, sizeof( *pSyncCmd ), pSyncCmd );
	ThreadSafe_DxOISP_CommandGroupClose();

	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stAsync ), sizeof(*pAsyncCmd), pAsyncCmd );
*/

	return;
}

static void _isp_calc_upscaletolerance( CAM_Int8u *pucUpscalingTolerance,
                                        CAM_Int32u usSensorWidth,
                                        CAM_Int32s usSensorHeight,
                                        CAM_Int32u usOutWidth,
                                        CAM_Int32s usOutHeight )
{
	CAM_Int8u  ucUpscalingTolerance;

#if 0
	CAM_Int32s iDecim = _DIV_CEIL( usSensorWidth/2, 1920 );

	ucUpscalingTolerance = _DIV_CEIL( 1920 << DxOISP_UPSCALING_TOLERANCE_FRAC_PART, (usSensorWidth / (2*iDecim)) );
#else
	// Hard code to maximum UpscalingTolerance, because it's impossible to get the optimal one
	// without in-depth knowlege about DxO's implementation
	ucUpscalingTolerance = OV_UPSCALING_TOLER << DxOISP_UPSCALING_TOLERANCE_FRAC_PART;
#endif

	ucUpscalingTolerance = _MAX( ucUpscalingTolerance, 1 << DxOISP_UPSCALING_TOLERANCE_FRAC_PART );
	ucUpscalingTolerance = _MIN( ucUpscalingTolerance, 2 << DxOISP_UPSCALING_TOLERANCE_FRAC_PART );
	*pucUpscalingTolerance = ucUpscalingTolerance;

	return;
}

static void _isp_calc_pixperbayer( te_DxOISPFormat eIspFormat, CAM_Int32s *pPixH, CAM_Int32s *pPixV )
{
	switch ( eIspFormat )
	{
	case DxOISP_FORMAT_RAW:
	case DxOISP_FORMAT_YUV420:
		*pPixH = 2;
		*pPixV = 2;
		break;

	case DxOISP_FORMAT_YUV422:
		*pPixH = 2;
		*pPixV = 1;
		break;

	default:
		TRACE( CAM_ERROR, "not supported bayer format[%d]( %s, %s, %d )\n", eIspFormat, __FILE__, __FUNCTION__, __LINE__ );
		*pPixH = -1;
		*pPixV = -1;
		ASSERT( 0 );
		break;
	}

	return;
}

static CAM_Error _isp_setup_codec_property( _CAM_DxOIspState *pIspState )
{
	_CHECK_BAD_POINTER( pIspState );

	ts_DxOIspCmd        *pDxOCmd    = &pIspState->stDxOIspCmd;
	CAM_Int32u          i           = 0;
	CAM_Error           error       = CAM_ERROR_NONE;
	_CAM_IspDmaProperty stDmaProperty;

	// configure codec dma property
	stDmaProperty.iBandCnt       = pIspState->stFBufReq.iBandCnt;
	stDmaProperty.iBandSize      = pDxOCmd->stSync.stControl.usBandSize;
	stDmaProperty.iBufHandleMode = ( pDxOCmd->stSync.stControl.eMode == DxOISP_MODE_CAPTURE_A ||
	                                 pDxOCmd->stSync.stControl.eMode == DxOISP_MODE_CAPTURE_B ) ? 1 : 0;

	error = ispdma_set_codec_property( pIspState->hIspDmaHandle, &stDmaProperty );

	return error;
}

static void StreamStateToChar( CAM_Int32s v, CAM_Int8s *msg )
{
	if ( v & 0x1 )
	{
		strcpy( msg, "InFrame" );
	}
	else
	{
		strcpy( msg, "Idle" );
	}

	if ( v & 0x2 )
	{
		strcat( msg, "BlockedProd" );  // = idle
	}

	if ( v & 0x4 )
	{
		strcat( msg, "BlockedCons" );  // recieving pixels, but blocked by downstream blocks
	}

	if( v & 0x8 )
	{
		strcat( msg, "LastProd" );
	}

	if( v & 0x10 )
	{
		strcat( msg, "LastCons" );
	}

	if( v & 0x20 )
	{
		strcat( msg, "StickyCons" );
	}

	return;
}

static void _isp_dump_status()
{
	ts_DxOISPInfo   stInfo;
	CAM_Int32u      state, i;
	CAM_Int8s       msg[256];

	// sensor -> ISP status
	stInfo.id = 0;
	stInfo.ptr = (int*)(*( infoInstTab ));
	stInfo.port = 0;
	getInfoStreamState( &stInfo, &state );
	StreamStateToChar( state, msg );
	TRACE( CAM_WARN, "pixInfo0 port: %d: state=0x%02x ( %s )\n", 0, state, msg );

	// ISP -> diplay/codec DMA status
	memset( msg, 0, sizeof( msg ) );
	stInfo.id = 1;
	stInfo.ptr = (int*)(*( infoInstTab + 1 ));
	for ( i = 0; i < 2; i++ )
	{
		stInfo.port = i;
		getInfoStreamState( &stInfo, &state );
		StreamStateToChar( state, msg );
		TRACE( CAM_WARN, "pixInfo port: %d, state 0x%02x( %s )\n", i , state, msg );
		memset( msg, 0, sizeof( msg ) );
	}

	return;
}

static void _isp_wait_for_mode( _CAM_DxOIspState *pIspState, CAM_Int8u ucTargetMode )
{
	CAM_Int32s       cnt = 0;
	CAM_Int8u        ucModeState;
	ts_DxOIspCmd     *pDxOCmd = &pIspState->stDxOIspCmd;

	do
	{
#if 1
		CAM_Int32s ret  = 0;
		ret = CAM_EventWait( pIspState->hEventIPCStatusUpdated, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );
#else
		CAM_Sleep( 1000000 );
#endif

		DxOISP_StatusGroupOpen();
		DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.eMode ), 1, &ucModeState );
		DxOISP_StatusGroupClose();

		cnt++;
	} while ( ( ucModeState != ucTargetMode ) && cnt < 10 );
	ASSERT( ucModeState == ucTargetMode );
}

static void _isp_set_min_interframe_time( CAM_Bool bIsStateTranstion, ts_DxOIspCmd *pstDxOCmd )
{
	CAM_Int16u usFrameRate = pstDxOCmd->stSync.stControl.usFrameRate;
	CAM_Int8u eMode = pstDxOCmd->stSync.stControl.eMode;
	CAM_Int8u eVidSubMode = pstDxOCmd->stSync.stControl.eSubModeVideoRec;
	CAM_Int8u eCapSubMode = pstDxOCmd->stSync.stControl.eSubModeCapture;
	CAM_Int16u usMinInterFrameTime = 0;

	// Dynamic change of MinInterFrameTime can improve robustness of whole
	// system, this value should be different for each senario because
	// DxO microcode have different complexibility. Data below are coming
	// from experimental on 400MHz single core only abilene board, maybe
	// need more tunning for other boards
	if ( bIsStateTranstion )
	{
		usMinInterFrameTime = 20;
	}
	else
	{
		switch ( eMode )
		{
		case DxOISP_MODE_IDLE:
			usMinInterFrameTime = 5;
			break;
		case DxOISP_MODE_PREVIEW:
			usMinInterFrameTime = 10;
			break;
		case DxOISP_MODE_VIDEOREC:
			{
				switch ( eVidSubMode )
				{
				case CAM_VIDEOSUBMODE_SIMPLE:
					usMinInterFrameTime = 10;
					break;
				case CAM_VIDEOSUBMODE_STABILIZER:
					usMinInterFrameTime = 12;
					break;
				case CAM_VIDEOSUBMODE_TNR:
					usMinInterFrameTime = 12;
					break;
				case CAM_VIDEOSUBMODE_STABILIZEDTNR:
					usMinInterFrameTime = 15;
					break;
				default:
					usMinInterFrameTime = 10;
					TRACE( CAM_WARN, "Warning: bad video sub mode[%d]( %s, %s, %d )\n", eVidSubMode, __FILE__, __FUNCTION__, __LINE__ );
					break;
				}
			}
			break;
		case DxOISP_MODE_CAPTURE_A:
		case DxOISP_MODE_CAPTURE_B:
			{
				switch ( eCapSubMode )
				{
				case DxOISP_CAPTURE_SIMPLE:
					usMinInterFrameTime = 15;
					break;
				case DxOISP_CAPTURE_BURST:
					usMinInterFrameTime = 15;
					break;
				case DxOISP_CAPTURE_ZERO_SHUTTER_LAG:
					usMinInterFrameTime = 15;
					break;
				case DxOISP_CAPTURE_HDR:
					usMinInterFrameTime = 15;
					break;
				default:
					usMinInterFrameTime = 15;
					TRACE( CAM_WARN, "Warning: bad capture sub mode[%d]( %s, %s, %d )\n", eCapSubMode, __FILE__, __FUNCTION__, __LINE__ );
					break;
				}
			}
			break;
		default:
			usMinInterFrameTime = 12;
			TRACE( CAM_WARN, "Warning: bad state mode[%d]( %s, %s, %d )\n", eMode, __FILE__, __FUNCTION__, __LINE__ );
			break;
		}
	}

	// As all the data above are get from 30fps, so, we need adjust
	// that data according to frame rate of ourselves
	usMinInterFrameTime = (( (CAM_Int32u)usMinInterFrameTime << DxOISP_MIN_INTERFRAME_TIME_FRAC_PART ) *
	                      pstDxOCmd->stSync.stControl.usFrameRate / 30 ) >> DxOISP_FPS_FRAC_PART;

	pstDxOCmd->stAsync.stSystem.usMinInterframeTime = usMinInterFrameTime;
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stAsync.stSystem.usMinInterframeTime ), sizeof( uint16_t ), &usMinInterFrameTime );

	return ;
}

static void _isp_compute_framebuffer_size( _CAM_IspFrameBufferReq *pIspFBuf, ts_DxOIspSyncCmd *pstIspSyncCmd )
{
	CAM_Int16u       usBayerWidth   = 0, usBayerHeight = 0;
	CAM_Int8u        ucPixelWidth   = 0;
	CAM_Int32s       iPixPerBayer_H = 0, iPixPerBayer_V = 0;
	te_DxOISPFormat  eSensorImgFmt  = 0;

	// query frame buffer request from ISP firmware
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerWidth ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerWidth );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.usBayerHeight ),
	                  sizeof( CAM_Int16u ),
	                  &usBayerHeight );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.eImageType ),
	                  sizeof( CAM_Int8u ),
	                  &eSensorImgFmt );
	DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stAsync.stSensorCapabilities.stImageFeature.ucPixelWidth ),
	                  sizeof( CAM_Int8u ),
	                  &ucPixelWidth );

	_isp_calc_pixperbayer( eSensorImgFmt, &iPixPerBayer_H, &iPixPerBayer_V );

	pstIspSyncCmd->stControl.usBandSize       = DxOISP_ComputeBandSize( pstIspSyncCmd );
	DxOISP_ComputeFrameBufferSize( pstIspSyncCmd,
	                               usBayerWidth  * iPixPerBayer_H,
	                               usBayerHeight * iPixPerBayer_V,
	                               ucPixelWidth,
	                               (uint8_t *)&pIspFBuf->iFBufCnt,
	                               (uint8_t *)&pIspFBuf->iBandCnt,
	                               (uint32_t*)pIspFBuf->aiFBufSizeTab );
}

static CAM_Bool _isp_need_realloc_framebuffer( _CAM_DxOIspState *pIspState, _CAM_IspFrameBufferReq *pIspFBuf )
{
	CAM_Int32s i;

	if ( pIspFBuf->iFBufCnt == pIspState->stFBufReq.iFBufCnt )
	{
		for ( i = 0; i < pIspFBuf->iFBufCnt; i++ )
		{
			if ( pIspFBuf->aiFBufSizeTab[i] != pIspState->stFBufReq.aiFBufSizeTab[i] )
			{
				return CAM_TRUE;
			}
		}
		return CAM_FALSE;
	}
	return CAM_TRUE;
}

static CAM_Error _isp_alloc_framebuffer( _CAM_DxOIspState *pIspState )
{
	CAM_Int32s i, j;

	for ( i = 0; i < NB_FRAME_BUFFER; i++ )
	{
		if ( i < pIspState->stFBufReq.iFBufCnt )
		{
			pIspState->astFBuf[i].pUserData = osalbmm_malloc( pIspState->stFBufReq.aiFBufSizeTab[i], OSALBMM_ATTR_DEFAULT );
			pIspState->astFBuf[i].pVirtAddr = (CAM_Int8u*)pIspState->astFBuf[i].pUserData;
			pIspState->astFBuf[i].uiPhyAddr = osalbmm_get_paddr( pIspState->astFBuf[i].pVirtAddr );
			pIspState->astFBuf[i].iBufLen   = pIspState->stFBufReq.aiFBufSizeTab[i];

			if ( pIspState->astFBuf[i].pVirtAddr == 0 || pIspState->astFBuf[i].uiPhyAddr == 0 )
			{
				TRACE( CAM_ERROR, "Error: There is no enough memory to alloc frame buffer\n" );
				for ( j = i - 1; j >= 0; j-- )
				{
					osalbmm_free( pIspState->astFBuf[j].pUserData );
					memset ( &pIspState->astFBuf[j], 0, sizeof( _CAM_IspDmaBufNode ) );
				}
				return CAM_ERROR_OUTOFMEMORY;
			}
		}
		else
		{
			memset( &pIspState->astFBuf[i], 0, sizeof( _CAM_IspDmaBufNode ) );
		}
	}

	return CAM_ERROR_NONE;
}

static void _isp_free_framebuffer( _CAM_DxOIspState *pIspState )
{
	CAM_Int32s i;

	for ( i = 0; i < NB_FRAME_BUFFER; i++ )
	{
		if ( pIspState->astFBuf[i].pUserData != NULL )
		{
			osalbmm_free( pIspState->astFBuf[i].pUserData );
		}
		memset( &pIspState->astFBuf[i], 0, sizeof( _CAM_IspDmaBufNode ) );
	}
}

static CAM_Error _isp_set_internal_state( _CAM_DxOIspState *pIspState, te_DxOISPMode eDxOISPMode )
{
	_CAM_IspFrameBufferReq stTempFBufReq;
	ts_DxOIspCmd           *pDxOCmd    = &pIspState->stDxOIspCmd;
	te_DxOISPMode          eFromState  = pDxOCmd->stSync.stControl.eMode;
	CAM_Int32s             i           = 0;
	CAM_Bool               bSensorOn, bIspDisplayOn, bIspCodecOn, bIsDispStreaming, bIsCodecStreaming;
	CAM_Bool               bCheckDispStatus, bNeedReallocFBuf, bPostFreeFBuf = CAM_FALSE;
	CAM_Error              error       = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pIspState );

	switch ( eDxOISPMode )
	{
	case DxOISP_MODE_STANDBY:
	case DxOISP_MODE_IDLE:
		bSensorOn                       = CAM_FALSE;
		bIspCodecOn                     = CAM_FALSE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_TRUE;
		break;

	case DxOISP_MODE_PREVIEW:
		bSensorOn                       = CAM_TRUE;
		bIspCodecOn                     = CAM_FALSE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_TRUE;
		break;

	case DxOISP_MODE_PRECAPTURE:
		bSensorOn                       = CAM_TRUE;
		bIspCodecOn                     = CAM_FALSE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_TRUE;
		break;

	case DxOISP_MODE_CAPTURE_A:
	case DxOISP_MODE_CAPTURE_B:
		bSensorOn                       = CAM_TRUE;
		bIspCodecOn                     = CAM_TRUE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_FALSE;
		pIspState->iSkippedStillFrames  = 0;
		break;

	case DxOISP_MODE_VIDEOREC:
		bSensorOn                       = CAM_TRUE;
		bIspCodecOn                     = CAM_TRUE;
		bIsDispStreaming                = CAM_TRUE;
		bIsCodecStreaming               = CAM_TRUE;
		break;

	default:
		TRACE( CAM_ERROR, "Error: invalid internal state transition to[%d]( %s, %s, %d )\n", eDxOISPMode, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_STATELIMIT;
	}

	pDxOCmd->stSync.stControl.isDisplayDisabled = 1;
	bIspDisplayOn                               = CAM_FALSE;
	if ( pIspState->bDisplayShouldEnable == CAM_TRUE && pIspState->iDispBufCnt >= pIspState->iDispMinBufCnt )
	{
		pDxOCmd->stSync.stControl.isDisplayDisabled = 0;
		bIspDisplayOn                               = CAM_TRUE;
	}

	pDxOCmd->stSync.stControl.eMode = eDxOISPMode;
	// Set this value to DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT because:
	// 1. Progressive digital zoom is not necessary for state transtion case
	// 2. According to DxO, this value MUST BE set to DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT otherwise, it'll assert fail
	pDxOCmd->stSync.stControl.usFrameToFrameLimit = DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT;

	_isp_compute_framebuffer_size( &stTempFBufReq, &pDxOCmd->stSync );

	bNeedReallocFBuf = _isp_need_realloc_framebuffer( pIspState, &stTempFBufReq );
	pIspState->stFBufReq = stTempFBufReq;

	if ( bNeedReallocFBuf == CAM_TRUE )
	{
		if ( eDxOISPMode == DxOISP_MODE_PREVIEW || eDxOISPMode == DxOISP_MODE_IDLE )
		{
			bPostFreeFBuf = CAM_TRUE;
		}
		else
		{
			if ( eFromState != DxOISP_MODE_PREVIEW && eFromState != DxOISP_MODE_IDLE )
			{
				CAM_Int8u ucMode = DxOISP_MODE_PREVIEW;
				_isp_set_min_interframe_time( CAM_TRUE, pDxOCmd );
				ThreadSafe_DxOISP_CommandGroupOpen();
				DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), sizeof(uint8_t), &ucMode );
				ThreadSafe_DxOISP_CommandGroupClose();

				_isp_wait_for_mode ( pIspState, DxOISP_MODE_PREVIEW );
				_isp_set_min_interframe_time( CAM_FALSE, pDxOCmd );
			}

			if ( pIspState->stFBufReq.iFBufCnt > 0 )
			{
				_isp_free_framebuffer( pIspState );

				error = ispdma_set_framebuffer( pIspState->hIspDmaHandle, pIspState->astFBuf, 0 );
				ASSERT_CAM_ERROR( error );
			}

			if ( pIspState->stFBufReq.iFBufCnt > 0 )
			{
				error = _isp_alloc_framebuffer( pIspState );
				ASSERT_CAM_ERROR( error );

				error = ispdma_set_framebuffer( pIspState->hIspDmaHandle, pIspState->astFBuf, pIspState->stFBufReq.iFBufCnt );
				ASSERT_CAM_ERROR( error );
			}
		}
	}

	if ( bIspCodecOn == CAM_TRUE && pIspState->bIsCodecOn == CAM_FALSE )
	{
		error = _isp_setup_codec_property( pIspState );
		ASSERT_CAM_ERROR( error );
	}

	error = ispdma_isp_set_mode( pIspState->hIspDmaHandle, ISP_PORT_DISPLAY, bIsDispStreaming );
	ASSERT_CAM_ERROR( error );

	error = ispdma_isp_set_mode( pIspState->hIspDmaHandle, ISP_PORT_CODEC, bIsCodecStreaming );
	ASSERT_CAM_ERROR( error );

	// stream on DMAs if necessary, DMA should be started before DxO FW state transition
	if ( bSensorOn == CAM_TRUE )
	{
		error = ispdma_sensor_streamon( pIspState->hIspDmaHandle, bSensorOn );
		ASSERT_CAM_ERROR( error );
	}

	if ( bIspDisplayOn == CAM_TRUE )
	{
		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_DISPLAY, bIspDisplayOn );
		ASSERT_CAM_ERROR( error );
		pIspState->bIsDisplayOn = CAM_TRUE;
	}

	if ( bIspCodecOn == CAM_TRUE )
	{
		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_CODEC, bIspCodecOn );
		ASSERT_CAM_ERROR( error );
		pIspState->bIsCodecOn = CAM_TRUE;
	}

	// Enlarge the MinInterFrame time when state transition, because there will be
	// heavy load for DxOISP_Event() in this time
	_isp_set_min_interframe_time( CAM_TRUE, pDxOCmd );

	iMaxBfEventDelay  = 0;
	iMaxAftEventDelay = 0;

	// configure isp
	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.eMode ), sizeof(uint8_t), &pDxOCmd->stSync.stControl.eMode );
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.isDisplayDisabled ), sizeof(uint8_t), &pDxOCmd->stSync.stControl.isDisplayDisabled );
	// usBandSize has been computed in isp_config_output function.
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.usBandSize ), sizeof(uint16_t), &pDxOCmd->stSync.stControl.usBandSize );
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.usFrameToFrameLimit ), sizeof(uint16_t), &pDxOCmd->stSync.stControl.usFrameToFrameLimit );
	ThreadSafe_DxOISP_CommandGroupClose();

	_isp_wait_for_mode( pIspState, pDxOCmd->stSync.stControl.eMode );

	_isp_set_min_interframe_time( CAM_FALSE, pDxOCmd );

	iMaxBfEventDelay  = 0;
	iMaxAftEventDelay = 0;

	if ( bPostFreeFBuf == CAM_TRUE )
	{
		_isp_free_framebuffer( pIspState );
		error = ispdma_set_framebuffer( pIspState->hIspDmaHandle, pIspState->astFBuf, 0 );
		ASSERT_CAM_ERROR( error );
	}

	// stream off DMAs
	if ( bSensorOn == CAM_FALSE )
	{
		error = ispdma_sensor_streamon( pIspState->hIspDmaHandle, bSensorOn );
		ASSERT_CAM_ERROR( error );
	}

	if ( bIspDisplayOn == CAM_FALSE )
	{
		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_DISPLAY, bIspDisplayOn );
		ASSERT_CAM_ERROR( error );
		pIspState->bIsDisplayOn = CAM_FALSE;
	}

	if ( bIspCodecOn == CAM_FALSE )
	{
		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_CODEC, bIspCodecOn );
		ASSERT_CAM_ERROR( error );
		pIspState->bIsCodecOn = CAM_FALSE;
	}

	return error;
}

static CAM_Error _isp_set_display_port_state( _CAM_DxOIspState *pIspState, CAM_Bool bDisableDisplay )
{
	CAM_Error    error    = CAM_ERROR_NONE;
	ts_DxOIspCmd *pDxOCmd = &pIspState->stDxOIspCmd;
	CAM_Int8u    ucDisplayStatus;
	CAM_Int32s   iLoopCnt = 0;

	pDxOCmd->stSync.stControl.isDisplayDisabled = ( bDisableDisplay == CAM_TRUE ) ? 1 : 0;

	if ( bDisableDisplay == CAM_FALSE )
	{
		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_DISPLAY, CAM_TRUE );
		ASSERT_CAM_ERROR( error );
	}

	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl.isDisplayDisabled ), 1, &pDxOCmd->stSync.stControl.isDisplayDisabled );
	ThreadSafe_DxOISP_CommandGroupClose();

	// Make sure isp firmware has disabled display port before we stream off isp dma display port.
	if ( bDisableDisplay == CAM_TRUE )
	{
		do
		{
			CAM_Int32s ret  = 0;
			ret = CAM_EventWait( pIspState->hEventIPCStatusUpdated, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );

			DxOISP_StatusGroupOpen();
			DxOISP_StatusGet( DxOISP_STATUS_OFFSET( stSync.stSystem.isDisplayDisabled ), 1, &ucDisplayStatus );
			DxOISP_StatusGroupClose();

			iLoopCnt++;
		} while( ucDisplayStatus != pDxOCmd->stSync.stControl.isDisplayDisabled && iLoopCnt < 10 );

		ASSERT( ucDisplayStatus == pDxOCmd->stSync.stControl.isDisplayDisabled );

		error = ispdma_isp_streamon( pIspState->hIspDmaHandle, ISP_PORT_DISPLAY, CAM_FALSE );
		ASSERT_CAM_ERROR( error );
	}

	return error;
}

CAM_Error _isp_update_view( ts_DxOIspCmd *pDxOCmd, CAM_Int32s iCropWidth, CAM_Int32s iCropHeight,
                            CAM_Int32u iDigZoomQ16, CAM_Int8u ucUpScaleTolerance, CAM_Bool bSmooth )
{
	CAM_Int16u usDispXStart, usDispXEnd, usDispYStart, usDispYEnd;
	CAM_Int16u usDispXCenter, usDispYCenter;
	CAM_Int16u usDispNewWidth, usDispNewHeight;

	CELOG( "before: Xstart: %d, Xend: %d, Ystart: %d, Yend: %d, usFrameToFrameLimit:%d, ucUpscalingTolerance:%d\n",
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrStart,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrEnd,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrStart,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrEnd,
	       pDxOCmd->stSync.stControl.usFrameToFrameLimit,
	       pDxOCmd->stSync.stControl.ucUpscalingTolerance );

	// get current cropping size
	usDispXStart = pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrStart;
	usDispYStart = pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrStart;
	usDispXEnd   = pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrEnd;
	usDispYEnd   = pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrEnd;

	usDispXCenter = (usDispXStart + usDispXEnd) / 2;
	usDispYCenter = (usDispYStart + usDispYEnd) / 2;

	usDispNewWidth  = (CAM_Int16u)( (iCropWidth << 16 ) / ( 2*iDigZoomQ16) );
	usDispNewHeight = (CAM_Int16u)( (iCropHeight << 16) / ( 2*iDigZoomQ16) );

	usDispNewWidth  = _MAX( usDispNewWidth,  4 );
	usDispNewHeight = _MAX( usDispNewHeight, 4 );

	pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrStart =\
	pDxOCmd->stSync.stControl.stOutputImage.stCrop.usXAddrStart   = _ALIGN_TO( usDispXCenter - usDispNewWidth, 2 );

	pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrEnd   =\
	pDxOCmd->stSync.stControl.stOutputImage.stCrop.usXAddrEnd     = ( usDispXCenter + usDispNewWidth ) | 0x1;

	pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrStart =\
	pDxOCmd->stSync.stControl.stOutputImage.stCrop.usYAddrStart   = _ALIGN_TO( usDispYCenter - usDispNewHeight, 2 );

	pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrEnd   =\
	pDxOCmd->stSync.stControl.stOutputImage.stCrop.usYAddrEnd     = ( usDispYCenter + usDispNewHeight ) | 0x1;

	pDxOCmd->stSync.stControl.ucUpscalingTolerance = ucUpScaleTolerance;
	pDxOCmd->stSync.stControl.usFrameToFrameLimit  = bSmooth ? DxOISP_SMOOTH_FRAMETOFRAME_LIMIT : DxOISP_NONSMOOTH_FRAMETOFRAME_LIMIT;

	ThreadSafe_DxOISP_CommandGroupOpen();
	DxOISP_CommandSet( DxOISP_CMD_OFFSET( stSync.stControl ), sizeof( pDxOCmd->stSync.stControl ), &pDxOCmd->stSync.stControl );
	ThreadSafe_DxOISP_CommandGroupClose();

	CELOG( "after: Xstart: %d, Xend: %d, Ystart: %d, Yend: %d, Zoom: %d, usFrameToFrameLimit:%d, ucUpscalingTolerance:%d\n",
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrStart,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usXAddrEnd,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrStart,
	       pDxOCmd->stSync.stControl.stOutputDisplay.stCrop.usYAddrEnd,
	       iDigZoomQ16 >> 16,
	       pDxOCmd->stSync.stControl.usFrameToFrameLimit,
	       pDxOCmd->stSync.stControl.ucUpscalingTolerance );

	return CAM_ERROR_NONE;
}

