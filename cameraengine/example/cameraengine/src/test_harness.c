/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_harness.h"
#include "test_harness_encoder.h"
#include "display.h"

#include "ippdefs.h"
#include "ippIP.h"
#include "DxOFmtConvert.h"


#if defined( CAMERAENGINE_INTERNAL_PROCESSING_ENABLED )

// This section is for debug
#if defined( PLATFORM_BOARD_BROWNSTONE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {SCREEN_WIDTH, SCREEN_HEIGHT};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_G50 )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {SCREEN_WIDTH, SCREEN_HEIGHT};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_ABILENE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {SCREEN_WIDTH, SCREEN_HEIGHT};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_ORCHID )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {SCREEN_WIDTH, SCREEN_HEIGHT};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_MK2 )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {SCREEN_WIDTH, SCREEN_HEIGHT};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_YELLOWSTONE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_TTCDKB ) || defined( PLATFORM_BOARD_TDDKB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_YCbCr420P;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_YCbCr420P;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {320, 240};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_MG1EVB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {320, 240};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_MG1SAARB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_BGR565;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_90L; // CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {320, 240};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_NEVOEVB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_NEVO )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {1280, 720};
CAM_Size        stDefaultVideoSize     = {1280, 720};
CAM_Size        stDefaultStillSize     = {3264, 2448};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#else
#error no platform is defined!
#endif

#else

#if defined( PLATFORM_BOARD_BROWNSTONE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {1280, 720};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_G50 )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {1280, 720};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_ABILENE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_ORCHID )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_MK2 )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined ( PLATFORM_BOARD_YELLOWSTONE )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {1920, 1080};
CAM_Size        stDefaultStillSize     = {3264, 2448}; // {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_TTCDKB ) || defined( PLATFORM_BOARD_TDDKB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_YCbCr420P;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_YCbCr420P;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_MG1EVB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_MG1SAARB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_NEVOEVB )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {640, 480};
CAM_Size        stDefaultVideoSize     = {640, 480};
CAM_Size        stDefaultStillSize     = {2592, 1944};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#elif defined( PLATFORM_BOARD_NEVO )
CAM_FlipRotate  eSensorRotate          = CAM_FLIPROTATE_NORMAL;
CAM_ImageFormat eDefaultVideoFormat    = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultPreviewFormat  = CAM_IMGFMT_CbYCrY;
CAM_ImageFormat eDefaultStillFormat    = CAM_IMGFMT_JPEG;
CAM_FlipRotate  eDefaultPreviewRotate  = CAM_FLIPROTATE_NORMAL;
CAM_Size        stDefaultPreviewSize   = {1280, 720};
CAM_Size        stDefaultVideoSize     = {1280, 720};
CAM_Size        stDefaultStillSize     = {3264, 2448};
CAM_Int32s      iPreviewFpsQ16         = (30 << 16);
CAM_Int32s      iVideoFpsQ16           = (30 << 16);
#else
#error no platform is defined!
#endif

#endif

CAM_ImageBuffer         *pImgBuf;
CAM_ImageBuffer         *pPreviewBuf;
CAM_Int32s              *pPreviewBufStatus;
CAM_Int32s              iPrevBufNum;
CAM_ImageBuffer         *pVideoBuf;
CAM_ImageBuffer         *pStillBuf;
CAM_ImageBufferReq      stPreviewBufReq;
CAM_ImageBufferReq      stVideoBufReq;
CAM_ImageBufferReq      stStillBufReq;

extern CAM_Bool         bDumpFrame;

CAM_JpegParam stJpegParam =
{
	1,         // 422
	80,        // quality factor
	CAM_FALSE, // enable exif
	CAM_FALSE, // embed thumbnail
	0, 0,
};

static const char* awbPreset[]={"DxOISP_AWB_PRESET_HORIZON",
                                "DxOISP_AWB_PRESET_TUNGSTEN",
                                "DxOISP_AWB_PRESET_TL84",
                                "DxOISP_AWB_PRESET_COOLWHITE",
                                "DxOISP_AWB_PRESET_D50",
                                "DxOISP_AWB_PRESET_DAYLIGHT",
                                "DxOISP_AWB_PRESET_CLOUDY",
                                "DxOISP_AWB_PRESET_D65",
                                "DxOISP_AWB_PRESET_SHADE"};

CAM_Int32s iExpectedPicNum = 0;
CAM_Int32s iPreviewBufNum  = APP_PREVIEW_BUFCNT;
CAM_Bool   bSaveStill       = CAM_TRUE;

int  iCamcorderInstanceCnt = 0;
void *hCamcorderHandle     = NULL;

FILE *fpPreviewDump  = NULL;
FILE *fpSnapshotDump = NULL;

DisplayCfg stDispCfg;


CAM_Error RestartCameraEngine( CAM_DeviceHandle hHandle )
{
	CAM_Error        error = CAM_ERROR_NONE;
	CAM_CaptureState eOldState;

	TRACE( "Recovering from the CI FIFO overflow fatal error...\n" );

#if 0
	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eOldState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	switch ( eOldState )
	{
	case CAM_CAPTURESTATE_PREVIEW:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_IDLE, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		break;

	case CAM_CAPTURESTATE_VIDEO:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_IDLE, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_VIDEO, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		break;

	case CAM_CAPTURESTATE_STILL:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_IDLE, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_STILL, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		break;

	default:
		break;
	}

#else
	error = CAM_SendCommand( hHandle, CAM_CMD_RESET_CAMERA, (CAM_Param)CAM_RESET_FAST, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

#endif

	TRACE( "Done!\n" );

	return error;
}

CAM_Error SafeDequeue( CAM_DeviceHandle hHandle, int *pPort, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error error = CAM_ERROR_NONE;

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_DEQUEUE_BUF, (CAM_Param)pPort, (CAM_Param)ppImgBuf );
	while ( CAM_ERROR_FATALERROR == error )
	{
		CAM_Error error1 = RestartCameraEngine( hHandle );
		ASSERT_CAM_ERROR( error1 );

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_DEQUEUE_BUF, (CAM_Param)pPort, (CAM_Param)ppImgBuf );
	}

	return error;
}

CAM_Error InitEngine( CAM_DeviceHandle *phHandle )
{
	CAM_Error            error = CAM_ERROR_NONE;
	int                  i, iSensorCount;
	CAM_DeviceProp       stCameraProp[CAM_MAX_SUPPORT_CAMERA];
	CAM_DeviceHandle     hHandle = NULL;
	CAM_CameraCapability stCameraCap;

	// get Camera Engine handle
	error = CAM_GetHandle( &hHandle );
	ASSERT_CAM_ERROR( error );

	*phHandle = hHandle;

	// enumerate all available sensors
	error = CAM_SendCommand( hHandle, CAM_CMD_ENUM_SENSORS, (CAM_Param)&iSensorCount, (CAM_Param)stCameraProp );
	ASSERT_CAM_ERROR( error );

	TRACE( "\n All sensors in this platform:\n" );
	for ( i = 0; i < iSensorCount; i++ )
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)i, (CAM_Param)&stCameraCap );
		ASSERT_CAM_ERROR( error );

		TRACE( "\t%d - %s\n", i, stCameraProp[i].sName );
		// TODO: you can check camera engine's capability here
	}
	TRACE( "\n" );

	return error;
}

CAM_Error SetSensor( CAM_DeviceHandle hHandle, CAM_Int32s iSensorID )
{
	CAM_Error            error       = CAM_ERROR_NONE;
	CAM_CameraCapability stCameraCap;
	CAM_PortCapability   *pPortCap   = NULL;

	// select sensor id
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_SENSOR_ID, (CAM_Param)iSensorID, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)iSensorID, (CAM_Param)&stCameraCap );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_SENSOR_ORIENTATION, (CAM_Param)eSensorRotate, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_PREVIEW_RESIZEFAVOR, (CAM_Param)CAM_TRUE, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_FRAMERATE, (CAM_Param)iPreviewFpsQ16, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&stDefaultPreviewSize );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)eDefaultPreviewFormat );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_VIDEO,(CAM_Param)&stDefaultVideoSize );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_VIDEO,(CAM_Param)eDefaultVideoFormat );
	ASSERT_CAM_ERROR( error );

	pPortCap = &stCameraCap.stPortCapability[CAM_PORT_STILL];
	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_STILL,(CAM_Param)&pPortCap->stMax );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_STILL, (CAM_Param)eDefaultStillFormat );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_FOCUSMODE, (CAM_Param)CAM_FOCUS_INFINITY, (CAM_Param)NULL );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_JPEGPARAM, (CAM_Param)&stJpegParam, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_BANDFILTER, (CAM_Param)CAM_BANDFILTER_60HZ, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	return error;
}


CAM_Error DeinitEngine( CAM_DeviceHandle hHandle )
{
	CAM_Error error = CAM_ERROR_NONE;

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_ANY, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_FreeHandle( &hHandle );
	ASSERT_CAM_ERROR( error );

	return error;
}

CAM_Error StartPreview( CAM_DeviceHandle hHandle )
{
	CAM_Error       error = CAM_ERROR_NONE;
	int             i, ret;
	CAM_Size        stPreviewSize;
	CAM_ImageFormat eFormat;
	CAM_FlipRotate  eRotate;

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&stPreviewSize );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_FORMAT, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&eFormat );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_ROTATION, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&eRotate );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_BUFREQ, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&stPreviewBufReq );
	ASSERT_CAM_ERROR( error );

	ret = displaydemo_open( stPreviewBufReq.iWidth, stPreviewBufReq.iHeight, stPreviewBufReq.eFormat, &stDispCfg );
	ASSERT( ret == 0 );

	/* buffer negotiation on PREVIEW port */
	if ( stPreviewBufReq.iMinBufCount < iPreviewBufNum )
	{
		stPreviewBufReq.iMinBufCount = iPreviewBufNum;
	}
	error = AllocateImages( &stPreviewBufReq, &pPreviewBuf );
	ASSERT_CAM_ERROR( error );

	pPreviewBufStatus = malloc( sizeof( CAM_Int32s ) * stPreviewBufReq.iMinBufCount );
	ASSERT( pPreviewBufStatus != NULL );

	memset( pPreviewBufStatus, 0, sizeof( CAM_Int32s ) * stPreviewBufReq.iMinBufCount );
	iPrevBufNum = stPreviewBufReq.iMinBufCount;

	/* enqueue buffers on PREVIEW port */
	for ( i = 0; i < stPreviewBufReq.iMinBufCount; i++ )
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pPreviewBuf[i] );
		ASSERT_CAM_ERROR(error);
	}

	/* start preview */
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	return error;
}

CAM_Error StopPreview( CAM_DeviceHandle hHandle )
{
	CAM_Error        error = CAM_ERROR_NONE;
	CAM_CaptureState eState;

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( eState == CAM_CAPTURESTATE_STILL )
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
	}
	else if ( eState == CAM_CAPTURESTATE_VIDEO )
	{
		error = StopRecording( hHandle );
		ASSERT_CAM_ERROR( error );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_IDLE, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	displaydemo_close( &stDispCfg );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_ANY, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( pPreviewBuf != NULL )
	{
		FreeImages( &pPreviewBuf, stPreviewBufReq.iMinBufCount );
	}

	if ( pVideoBuf != NULL )
	{
		FreeImages( &pVideoBuf, stVideoBufReq.iMinBufCount );
	}

	if ( pStillBuf != NULL )
	{
		FreeImages( &pStillBuf, stStillBufReq.iMinBufCount );
	}

	return error;
}


CAM_Error AutoFocus( CAM_DeviceHandle hHandle )
{
	CAM_Error        error = CAM_ERROR_NONE;
	CAM_CaptureState eState;

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( eState != CAM_CAPTURESTATE_PREVIEW )
	{
		TRACE( "Warning: only accept auto focus command at preview state!\n" );
		error = CAM_ERROR_STATELIMIT;
	}
	else
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_START_FOCUS, UNUSED_PARAM, UNUSED_PARAM );
		// ASSERT_CAM_ERROR( error );
	}

	return error;
}


CAM_Error CancelAutoFocus( CAM_DeviceHandle hHandle )
{
	CAM_Error        error;
	CAM_CaptureState eState;

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_CANCEL_FOCUS, UNUSED_PARAM, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	return error;
}


CAM_Error TakePicture( CAM_DeviceHandle hHandle )
{
	CAM_CaptureState      eState;
	CAM_Error             error;
	CAM_StillSubMode      eStillSubMode;
	CAM_StillSubModeParam stStillSubModeParam;
	int                   i, ret;

	if ( iExpectedPicNum > 0 )
	{
		TRACE( "Previous image(s) are not yet dequeued!\n" );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( eState != CAM_CAPTURESTATE_PREVIEW )
	{
		TRACE( "Bad state transition!\n" );
		return CAM_ERROR_BADSTATETRANSITION;
	}

	// Allocate and enqueue still image buffers
	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_BUFREQ, (CAM_Param)CAM_PORT_STILL, (CAM_Param)&stStillBufReq );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STILL_SUBMODE, (CAM_Param)&eStillSubMode, (CAM_Param)&stStillSubModeParam );
	ASSERT_CAM_ERROR( error );

	iExpectedPicNum = stStillSubModeParam.iBurstCnt;
	TRACE( "iExpectedPicNum: %d\n", iExpectedPicNum );

	if ( pStillBuf == NULL )
	{
		// Buffer negotiation on STILL port
		if ( stStillBufReq.iMinBufCount < APP_STILL_BUFCNT )
		{
			stStillBufReq.iMinBufCount = APP_STILL_BUFCNT;
		}

		if ( stStillBufReq.iMinBufCount < iExpectedPicNum )
		{
			stStillBufReq.iMinBufCount = iExpectedPicNum;
		}

		error = AllocateImages( &stStillBufReq, &pStillBuf );
		ASSERT_CAM_ERROR( error );

		// Enqueue all still image buffers
		for ( i = 0; i < stStillBufReq.iMinBufCount; i++ )
		{
			pStillBuf[i].bEnableShotInfo = CAM_TRUE;
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_STILL, (CAM_Param)&pStillBuf[i] );
			ASSERT_CAM_ERROR( error );
		}
	}

	// start still state
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_STILL, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "Start to capture %d image(s) ( %s, %s, %d )\n", iExpectedPicNum, __FILE__, __FUNCTION__, __LINE__ );

	return error;
}

CAM_Error StartRecording( CAM_DeviceHandle hHandle )
{
	CAM_CaptureState eState;
	CAM_Error        error;
	char             sFileName[256] = {0};
	int              i;
	char             ext[][8] = {"yuv", "mpeg4", "h263", "h264"};

#ifdef _ENABLE_VIDEO_ENCODER_
	int iCamcorderType = 3;  // h264
#else
	int iCamcorderType = 0;  // raw data
#endif

	if ( iCamcorderType >= 0 )
	{
#if defined( ANDROID )
		sprintf( sFileName, "/data/Video%d%d%d.%s", (iCamcorderInstanceCnt / 100) % 10, (iCamcorderInstanceCnt / 10) % 10, iCamcorderInstanceCnt % 10, ext[iCamcorderType] );
#else
		sprintf( sFileName, "./Video%d%d%d.%s", (iCamcorderInstanceCnt / 100) % 10, (iCamcorderInstanceCnt / 10) % 10, iCamcorderInstanceCnt % 10, ext[iCamcorderType] );
#endif
		TRACE( "Video recording --> %s\n", sFileName );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( eState != CAM_CAPTURESTATE_PREVIEW )
	{
		TRACE( "Bad state transition!\n" );
		return CAM_ERROR_BADSTATETRANSITION;
	}

	iCamcorderInstanceCnt++;

	// allocate and enqueue video image buffers
	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_BUFREQ, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&stVideoBufReq );
	ASSERT_CAM_ERROR( error );

	// negotiate buffer count on video port
	if ( stVideoBufReq.iMinBufCount < APP_VIDEO_BUFCNT )
	{
		stVideoBufReq.iMinBufCount = APP_VIDEO_BUFCNT;
	}

	error = AllocateImages( &stVideoBufReq, &pVideoBuf );
	ASSERT_CAM_ERROR( error );

	// enqueue all video image buffers
	for ( i = 0; i < stVideoBufReq.iMinBufCount; i++ )
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&pVideoBuf[i] );
		ASSERT_CAM_ERROR( error );
	}

	// initialize the video encoder
	hCamcorderHandle = VideoEncoder_Init( hHandle, pVideoBuf, stVideoBufReq.iMinBufCount, iCamcorderType, sFileName );
	if ( hCamcorderHandle == NULL )
	{
		TRACE( "Failed to initialize the video encoder!\n" );
		ASSERT( hCamcorderHandle != NULL );
	}

	// start recording
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_VIDEO, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	return error;
}


CAM_Error StopRecording( CAM_DeviceHandle hHandle )
{
	CAM_CaptureState eState;
	CAM_Error        error;

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	if ( eState != CAM_CAPTURESTATE_VIDEO )
	{
		TRACE( "Bad state transition!\n" );
		return CAM_ERROR_BADSTATETRANSITION;
	}

	VideoEncoder_SendEOS( hCamcorderHandle );
	do
	{
		RunOneFrame( hHandle );
	} while ( !VideoEncoder_EOSReceived( hCamcorderHandle ) );

	TRACE( "Closing video file......\n" );

	VideoEncoder_Deinit( hCamcorderHandle );
	hCamcorderHandle = NULL;

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_VIDEO, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	TRACE( "Done\n" );

	FreeImages( &pVideoBuf, stVideoBufReq.iMinBufCount );

	return error;
}


CAM_Error RunOneFrame( CAM_DeviceHandle hHandle )
{
	CAM_Error        error    = CAM_ERROR_NONE;
	CAM_CaptureState eState;
	int              iPortId;
	CAM_ImageBuffer  *pImgBuf = NULL;

	static CAM_CaptureState eOldState = CAM_CAPTURESTATE_IDLE;

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	if ( eOldState != eState )
	{
		switch ( eState )
		{
		case CAM_CAPTURESTATE_STANDBY:
			TRACE( "Standby State\n" );
			break;
		case CAM_CAPTURESTATE_IDLE:
			TRACE( "Idle State\n" );
			break;
		case CAM_CAPTURESTATE_PREVIEW:
			TRACE( "Preview State\n" );
			break;
		case CAM_CAPTURESTATE_VIDEO:
			TRACE( "Video State\n" );
			break;
		case CAM_CAPTURESTATE_STILL:
			TRACE( "Still State\n" );
			break;
		default:
			TRACE( "Invalid State\n" );
			break;
		}
		eOldState = eState;
	}

	iPortId = CAM_PORT_ANY;
	error = SafeDequeue( hHandle, &iPortId, &pImgBuf );

	// TRACE( "port:%d\n", iPortId );
	if ( error == CAM_ERROR_NONE )
	{
		switch ( iPortId )
		{
		case CAM_PORT_PREVIEW:
			if ( eState == CAM_CAPTURESTATE_STILL )
			{
				TRACE( "got snapshot( %s, %d )\n", __FILE__, __LINE__ );
				error = DeliverSnapshotBuffer( hHandle, pImgBuf );  // snapshot images
				ASSERT_CAM_ERROR( error );
			}
			else
			{
				error = DeliverPreviewBuffer( hHandle, pImgBuf );
				ASSERT_CAM_ERROR( error );
			}
			break;

		case CAM_PORT_STILL:
			error = DeliverStillBuffer( hHandle, pImgBuf );
			ASSERT_CAM_ERROR( error );
			break;

		case CAM_PORT_VIDEO:
			error = DeliverVideoBuffer( hHandle, pImgBuf );
			ASSERT_CAM_ERROR( error );
			break;

		default:
			ASSERT( CAM_FALSE );
			break;
		}
	}

	return error;
}


CAM_Error GetParametersAndDisplay( CAM_DeviceHandle hHandle, CAM_CameraCapability *pCameraCap )
{
	CAM_Error              error;
	int                    i;
	CAM_PortCapability     *pPortCap;
	CameraParam            stCameraParam;
	CAM_ShotModeCapability stShotModeCap;
	CAM_DeviceProp         stCameraProp[CAM_MAX_SUPPORT_CAMERA];
	CAM_Int32s             iSensorCount;
	CAM_StillSubMode       eStillSubMode;
	CAM_StillSubModeParam  stStillParam;

	TRACE( "<q>:\texit this application\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_SENSOR_ID, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<sensor: %d>:\tset sensor id\n", stCameraParam.int32 );

	// enumerate all available sensors
	error = CAM_SendCommand( hHandle, CAM_CMD_ENUM_SENSORS, (CAM_Param)&iSensorCount, (CAM_Param)stCameraProp );
	ASSERT_CAM_ERROR( error );

	for ( i = 0; i < iSensorCount; i++ )
	{
		if ( stCameraParam.int32 == i )
		{
			TRACE( "\t[%d: %s]", i, stCameraProp[i].sName );
		}
		else
		{
			TRACE( "\t%d: %s", i, stCameraProp[i].sName );
		}
	}
	TRACE( "\n" );


	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&stCameraParam.size );
	ASSERT_CAM_ERROR( error );
	TRACE( "<ps: %d x %d>:\tset preview size\n", stCameraParam.size.iWidth, stCameraParam.size.iHeight );

	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_PREVIEW];
	if ( pPortCap->bArbitrarySize )
	{
		TRACE( "\t%d x %d - %d x %d\n",
		        pPortCap->stMin.iWidth, pPortCap->stMin.iHeight,
		        pPortCap->stMax.iWidth, pPortCap->stMax.iHeight );
	}
	else
	{
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			if ( stCameraParam.size.iWidth == pPortCap->stSupportedSize[i].iWidth &&
				 stCameraParam.size.iHeight == pPortCap->stSupportedSize[i].iHeight )
			{
				TRACE( "\t[%d x %d]", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
			else
			{
				TRACE( "\t%d x %d", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
		}
		TRACE( "\n" );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_FORMAT, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&stCameraParam.int32 );
	ASSERT_CAM_ERROR( error );
	TRACE( "<pf: %d>:\tset preview format\n", stCameraParam.int32 );

	for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
	{
		if ( (CAM_ImageFormat)stCameraParam.int32 == pPortCap->eSupportedFormat[i] )
		{
			TRACE( "\t[%d - ", pPortCap->eSupportedFormat[i] );
		}
		else
		{
			TRACE( "\t%d - ", pPortCap->eSupportedFormat[i] );
		}

		switch ( pPortCap->eSupportedFormat[i] )
		{
		case CAM_IMGFMT_RGB565:
			TRACE( "RGB565" );
			break;
		case CAM_IMGFMT_BGR565:
			TRACE( "BGR565" );
			break;
		case CAM_IMGFMT_YCbCr444P:
			TRACE( "YUV444P" );
			break;
		case CAM_IMGFMT_YCbCr444I:
			TRACE( "YUV444I" );
			break;
		case CAM_IMGFMT_YCbCr422P:
			TRACE( "YUV422P" );
			break;
		case CAM_IMGFMT_YCbYCr:
			TRACE( "YUYV" );
			break;
		case CAM_IMGFMT_CbYCrY:
			TRACE( "UYVY" );
			break;
		case CAM_IMGFMT_YCrYCb:
			TRACE( "YVYU" );
			break;
		case CAM_IMGFMT_CrYCbY:
			TRACE( "VYUY" );
			break;
		case CAM_IMGFMT_YCrCb420P:
			TRACE( "YVU420P" );
			break;
		case CAM_IMGFMT_YCbCr420P:
			TRACE( "YUV420P" );
			break;
		case CAM_IMGFMT_YCbCr420SP:
			TRACE( "YUV420SP" );
			break;
		case CAM_IMGFMT_YCrCb420SP:
			TRACE( "YVU420SP" );
			break;
		case CAM_IMGFMT_RGB888:
			TRACE( "RGB888" );
			break;
		case CAM_IMGFMT_BGR888:
			TRACE( "BGR888" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ImageFormat)stCameraParam.int32 == pPortCap->eSupportedFormat[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_SIZE, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&stCameraParam.size );
	ASSERT_CAM_ERROR( error );
	TRACE( "<vs: %d x %d>:\tset video size\n", stCameraParam.size.iWidth, stCameraParam.size.iHeight );

	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_VIDEO];
	if ( pPortCap->bArbitrarySize )
	{
		TRACE( "\t%d x %d - %d x %d\n", pPortCap->stMin.iWidth, pPortCap->stMin.iHeight, pPortCap->stMax.iWidth, pPortCap->stMax.iHeight );
	}
	else
	{
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			if ( stCameraParam.size.iWidth == pPortCap->stSupportedSize[i].iWidth && stCameraParam.size.iHeight == pPortCap->stSupportedSize[i].iHeight )
			{
				TRACE( "\t[%d x %d]", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
			else
			{
				TRACE( "\t%d x %d", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
		}
		TRACE( "\n" );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_SIZE, (CAM_Param)CAM_PORT_STILL, (CAM_Param)&stCameraParam.size );
	ASSERT_CAM_ERROR( error );
	TRACE( "<ss: %d x %d>:\tset still size\n", stCameraParam.size.iWidth, stCameraParam.size.iHeight );

	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_STILL];
	if ( pPortCap->bArbitrarySize )
	{
		TRACE( "\t%d x %d - %d x %d\n",
		       pPortCap->stMin.iWidth, pPortCap->stMin.iHeight,
		       pPortCap->stMax.iWidth, pPortCap->stMax.iHeight );
	}
	else
	{
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			if ( stCameraParam.size.iWidth == pPortCap->stSupportedSize[i].iWidth && stCameraParam.size.iHeight == pPortCap->stSupportedSize[i].iHeight )
			{
				TRACE( "\t[%d x %d]", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
			else
			{
				TRACE( "\t%d x %d", pPortCap->stSupportedSize[i].iWidth, pPortCap->stSupportedSize[i].iHeight );
			}
		}
		TRACE( "\n" );
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_FORMAT, (CAM_Param)CAM_PORT_STILL, (CAM_Param)&stCameraParam.int32 );
	ASSERT_CAM_ERROR( error );
	TRACE( "<ssf: %d>:\tset still format\n", stCameraParam.int32 );

	for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
	{
		if ( (CAM_ImageFormat)stCameraParam.int32 == pPortCap->eSupportedFormat[i] )
		{
			TRACE( "\t[%d - ", pPortCap->eSupportedFormat[i] );
		}
		else
		{
			TRACE( "\t%d - ", pPortCap->eSupportedFormat[i] );
		}

		switch ( pPortCap->eSupportedFormat[i] )
		{
		case CAM_IMGFMT_BGGR10:
			TRACE( "BGGR10" );
			break;
		case CAM_IMGFMT_JPEG:
			TRACE( "JPEG" );
			break;
		case CAM_IMGFMT_BGR565:
			TRACE( "BGR565" );
			break;
		case CAM_IMGFMT_YCbCr444P:
			TRACE( "YUV444P" );
			break;
		case CAM_IMGFMT_YCbCr444I:
			TRACE( "YUV444I" );
			break;
		case CAM_IMGFMT_YCbCr422P:
			TRACE( "YUV422P" );
			break;
		case CAM_IMGFMT_YCbYCr:
			TRACE( "YUYV" );
			break;
		case CAM_IMGFMT_CbYCrY:
			TRACE( "UYVY" );
			break;
		case CAM_IMGFMT_YCrYCb:
			TRACE( "YVYU" );
			break;
		case CAM_IMGFMT_CrYCbY:
			TRACE( "VYUY" );
			break;
		case CAM_IMGFMT_YCrCb420P:
			TRACE( "YVU420P" );
			break;
		case CAM_IMGFMT_YCbCr420P:
			TRACE( "YUV420P" );
			break;
		case CAM_IMGFMT_YCbCr420SP:
			TRACE( "YUV420SP" );
			break;
		case CAM_IMGFMT_YCrCb420SP:
			TRACE( "YVU420SP" );
			break;
		case CAM_IMGFMT_RGB888:
			TRACE( "RGB888" );
			break;
		case CAM_IMGFMT_BGR888:
			TRACE( "BGR888" );
			break;
		case CAM_IMGFMT_RGB565:
			TRACE( "RGB565" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ImageFormat)stCameraParam.int32 == pPortCap->eSupportedFormat[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	TRACE(
	       "<r>:\tswitch on/off video recording\n"
	       "<hp>:\thalf-press to prepare taking picture\n"
	       "<rp>:\trelease-press to cancel capture\n"
	       "<p>:\tfull-press to take picture\n"
	       "<sf>:\tstart face detecton\n"
	       "<cf>:\tcancel face detecton\n" );


	error = CAM_SendCommand( hHandle, CAM_CMD_GET_SHOTMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<pm: %d>:\tset photo mode\n", stCameraParam.int32 );
	for ( i = 0; i < pCameraCap->iSupportedShotModeCnt; i++ )
	{
		if ( (CAM_ShotMode)stCameraParam.int32 == pCameraCap->eSupportedShotMode[i] )
		{
			TRACE( "\t[%d - ", pCameraCap->eSupportedShotMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", pCameraCap->eSupportedShotMode[i] );
		}

		switch ( pCameraCap->eSupportedShotMode[i] )
		{
		case CAM_SHOTMODE_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_SHOTMODE_MANUAL:
			TRACE( "Manual" );
			break;
		case CAM_SHOTMODE_PORTRAIT:
			TRACE( "Portrait" );
			break;
		case CAM_SHOTMODE_LANDSCAPE:
			TRACE( "Landscape" );
			break;
		case CAM_SHOTMODE_NIGHTPORTRAIT:
			TRACE( "Night portrait" );
			break;
		case CAM_SHOTMODE_NIGHTSCENE:
			TRACE( "Night" );
			break;
		case CAM_SHOTMODE_CHILD:
			TRACE( "Child" );
			break;
		case CAM_SHOTMODE_INDOOR:
			TRACE( "Indoor" );
			break;
		case CAM_SHOTMODE_PLANTS:
			TRACE( "Plant" );
			break;
		case CAM_SHOTMODE_SNOW:
			TRACE( "Snow" );
			break;
		case CAM_SHOTMODE_BEACH:
			TRACE( "Beach" );
			break;
		case CAM_SHOTMODE_FIREWORKS:
			TRACE( "Fire work" );
			break;
		case CAM_SHOTMODE_SUBMARINE:
			TRACE( "Submarine" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ShotMode)stCameraParam.int32 == pCameraCap->eSupportedShotMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_SHOTMODE_CAP, (CAM_Param)stCameraParam.int32, (CAM_Param)&stShotModeCap );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_EXPOSUREMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<em: %d>:\tset exposure mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedExpModeCnt; i++ )
	{
		if ( (CAM_ExposureMode)stCameraParam.int32 == stShotModeCap.eSupportedExpMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedExpMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedExpMode[i] );
		}

		switch ( stShotModeCap.eSupportedExpMode[i] )
		{
		case CAM_EXPOSUREMODE_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_EXPOSUREMODE_APERTUREPRIOR:
			TRACE( "Aperture Prior" );
			break;
		case CAM_EXPOSUREMODE_SHUTTERPRIOR:
			TRACE( "Shutter Prior" );
			break;
		case CAM_EXPOSUREMODE_PROGRAM:
			TRACE( "Program" );
			break;
		case CAM_EXPOSUREMODE_MANUAL:
			TRACE( "Manual" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ExposureMode)stCameraParam.int32 == stShotModeCap.eSupportedExpMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_EXPOSUREMETERMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<emm: %d>:\tset exposure metering mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedExpMeterCnt; i++ )
	{
		if ( (CAM_ExposureMeterMode)stCameraParam.int32 == stShotModeCap.eSupportedExpMeter[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedExpMeter[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedExpMeter[i] );
		}

		switch ( stShotModeCap.eSupportedExpMeter[i] )
		{
		case CAM_EXPOSUREMETERMODE_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_EXPOSUREMETERMODE_MEAN:
			TRACE( "Mean" );
			break;
		case CAM_EXPOSUREMETERMODE_CENTRALWEIGHTED:
			TRACE( "Central Weighted" );
			break;
		case CAM_EXPOSUREMETERMODE_SPOT:
			TRACE( "Spot" );
			break;
		case CAM_EXPOSUREMETERMODE_MATRIX:
			TRACE( "Matrix (Evaluative)" );
			break;
		case CAM_EXPOSUREMETERMODE_MANUAL:
			TRACE( "Manual" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ExposureMeterMode)stCameraParam.int32 == stShotModeCap.eSupportedExpMeter[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_EVCOMPENSATION, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<ev: %d>:\tset EV (%d ev ~ %d ev, step: %d)\n",\
	       stCameraParam.int32,\
	       stShotModeCap.iMinEVCompQ16, stShotModeCap.iMaxEVCompQ16, stShotModeCap.iEVCompStepQ16 );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_ISO, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<iso: %d>:\tset ISO mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedIsoModeCnt; i++ )
	{
		if ( (CAM_ISOMode)stCameraParam.int32 == stShotModeCap.eSupportedIsoMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedIsoMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedIsoMode[i] );
		}

		switch ( stShotModeCap.eSupportedIsoMode[i] )
		{
		case CAM_ISO_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_ISO_50:
			TRACE( "50" );
			break;
		case CAM_ISO_100:
			TRACE( "100" );
			break;
		case CAM_ISO_200:
			TRACE( "200" );
			break;
		case CAM_ISO_400:
			TRACE( "400" );
			break;
		case CAM_ISO_800:
			TRACE( "800" );
			break;
		case CAM_ISO_1600:
			TRACE( "1600" );
			break;
		case CAM_ISO_3200:
			TRACE( "3200" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ISOMode)stCameraParam.int32 == stShotModeCap.eSupportedIsoMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_SHUTTERSPEED, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<st: %.3f>:\tset shutter time (%.3f sec - %.3f sec)\n", stCameraParam.int32 / 65536.0f,
	       stShotModeCap.iMinShutSpdQ16 / 65536.0f, stShotModeCap.iMaxShutSpdQ16 / 65536.0f );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_FNUM, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<fn: %.1f>:\tset f-number (%.1f - %.1f)\n",stCameraParam.int32 / 65536.0f,
	       stShotModeCap.iMinFNumQ16 / 65536.0f, stShotModeCap.iMaxFNumQ16 / 65536.0f );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_BANDFILTER, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<bf: %d>:\tset band filter\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedBdFltModeCnt; i++ )
	{
		if ( (CAM_BandFilterMode)stCameraParam.int32 == stShotModeCap.eSupportedBdFltMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedBdFltMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedBdFltMode[i] );
		}
		switch ( stShotModeCap.eSupportedBdFltMode[i] )
		{
		case CAM_BANDFILTER_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_BANDFILTER_OFF:
			TRACE( "Off" );
			break;
		case CAM_BANDFILTER_50HZ:
			TRACE( "50 Hz" );
			break;
		case CAM_BANDFILTER_60HZ:
			TRACE( "60 Hz" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_BandFilterMode)stCameraParam.int32 == stShotModeCap.eSupportedBdFltMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_SENSOR_ORIENTATION, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<so: %d>:\tset orientation\n", stCameraParam.int32 );
	for ( i = 0; i < pCameraCap->iSupportedSensorRotateCnt; i++ )
	{
		if ( (CAM_FlipRotate)stCameraParam.int32 == pCameraCap->eSupportedSensorRotate[i] )
		{
			TRACE( "\t[%d - ", pCameraCap->eSupportedSensorRotate[i] );
		}
		else
		{
			TRACE( "\t%d - ", pCameraCap->eSupportedSensorRotate[i] );
		}
		switch ( pCameraCap->eSupportedSensorRotate[i] )
		{
		case CAM_FLIPROTATE_NORMAL:
			TRACE( "Normal" );
			break;
		case CAM_FLIPROTATE_90L:
			TRACE( "90L" );
			break;
		case CAM_FLIPROTATE_90R:
			TRACE( "90R" );
			break;
		case CAM_FLIPROTATE_180:
			TRACE( "180" );
			break;
		case CAM_FLIPROTATE_HFLIP:
			TRACE( "HFLIP" );
			break;
		case CAM_FLIPROTATE_VFLIP:
			TRACE( "VFLIP" );
			break;
		case CAM_FLIPROTATE_DFLIP:
			TRACE( "DFLIP" );
			break;
		case CAM_FLIPROTATE_ADFLIP:
			TRACE( "ADFLIP" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_FlipRotate)stCameraParam.int32 == pCameraCap->eSupportedSensorRotate[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );


	error = CAM_SendCommand( hHandle, CAM_CMD_GET_FLASHMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<sm: %d>:\tset flash mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedFlashModeCnt; i++ )
	{
		if ( (CAM_FlashMode)stCameraParam.int32 == stShotModeCap.eSupportedFlashMode[i] )
		{
			TRACE( "\t[ %d - ", stShotModeCap.eSupportedFlashMode[i] );
		}
		else
		{
			TRACE( "\t  %d - ", stShotModeCap.eSupportedFlashMode[i] );
		}

		switch ( stShotModeCap.eSupportedFlashMode[i] )
		{
		case CAM_FLASH_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_FLASH_OFF:
			TRACE( "Off" );
			break;
		case CAM_FLASH_ON:
			TRACE( "On" );
			break;
		case CAM_FLASH_TORCH:
			TRACE( "Torch" );
			break;
		case CAM_FLASH_REDEYE:
			TRACE( "Red Eye" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_FlashMode)stCameraParam.int32 == stShotModeCap.eSupportedFlashMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_WHITEBALANCEMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<wb: %d>:\tset white balance mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedWBModeCnt; i++ )
	{
		if ( (CAM_WhiteBalanceMode)stCameraParam.int32 == stShotModeCap.eSupportedWBMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedWBMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedWBMode[i] );
		}

		switch ( stShotModeCap.eSupportedWBMode[i] )
		{
		case CAM_WHITEBALANCEMODE_AUTO:
			TRACE( "Auto" );
			break;
		case CAM_WHITEBALANCEMODE_INCANDESCENT:
			TRACE( "Incandescent" );
			break;
		case CAM_WHITEBALANCEMODE_DAYLIGHT_FLUORESCENT:
			TRACE( "Daylight Fluorescent" );
			break;
		case CAM_WHITEBALANCEMODE_DAYWHITE_FLUORESCENT:
			TRACE( "Daywhite Fluorescent" );
			break;
		case CAM_WHITEBALANCEMODE_COOLWHITE_FLUORESCENT:
			TRACE( "Coolwhite Fluorescent" );
			break;
		case CAM_WHITEBALANCEMODE_DAYLIGHT:
			TRACE( "Daylight" );
			break;
		case CAM_WHITEBALANCEMODE_CLOUDY:
			TRACE( "Cloudy" );
			break;
		case CAM_WHITEBALANCEMODE_SHADOW:
			TRACE( "Shadow" );
			break;
		case CAM_WHITEBALANCEMODE_LOCK:
			TRACE( "Lock" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_WhiteBalanceMode)stCameraParam.int32 == stShotModeCap.eSupportedWBMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_FOCUSMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<fm: %d>:\tset focus mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedFocusModeCnt; i++ )
	{
		if ( (CAM_FocusMode)stCameraParam.int32 == stShotModeCap.eSupportedFocusMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedFocusMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedFocusMode[i] );
		}

		switch ( stShotModeCap.eSupportedFocusMode[i] )
		{
		case CAM_FOCUS_AUTO_ONESHOT:
			TRACE( "Auto OneShot Center" );
			break;
		case CAM_FOCUS_AUTO_CONTINUOUS_VIDEO:
			TRACE( "Auto Continuous Video" );
			break;
		case CAM_FOCUS_AUTO_CONTINUOUS_PICTURE:
			TRACE( "Auto Continuous Picture" );
			break;
		case CAM_FOCUS_SUPERMACRO:
			TRACE( "Supper Macro" );
			break;
		case CAM_FOCUS_MACRO:
			TRACE( "Macro" );
			break;
		case CAM_FOCUS_HYPERFOCAL:
			TRACE( "Hyper Focal" );
			break;
		case CAM_FOCUS_INFINITY:
			TRACE( "Infinity" );
			break;
		case CAM_FOCUS_MANUAL:
			TRACE( "Manual" );
			break;
		default:
			TRACE( "invalid focus mode[%d]\n", stShotModeCap.eSupportedFocusMode[i] );
			ASSERT( 0 );
			break;
		}

		if ( (CAM_FocusMode)stCameraParam.int32 == stShotModeCap.eSupportedFocusMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_DIGZOOM, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<z: %.2f>:\tset digital zoom (%.2f - %.2f)\n", stCameraParam.int32 / 65536.0f, stShotModeCap.iMinDigZoomQ16 / 65536.0f, stShotModeCap.iMaxDigZoomQ16 / 65536.0f );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_FRAMERATE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<fr: %.2f>:\tset frame rate (%.2f - %.2f)\n", stCameraParam.int32 / 65536.0f, stShotModeCap.iMinFpsQ16 / 65536.0f, stShotModeCap.iMaxFpsQ16 / 65536.0f  );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_COLOREFFECT, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<ce: %d>:\tset color effect\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedColorEffectCnt; i++ )
	{
		if ( (CAM_ColorEffect)stCameraParam.int32 == stShotModeCap.eSupportedColorEffect[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedColorEffect[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedColorEffect[i] );
		}

		switch ( stShotModeCap.eSupportedColorEffect[i] )
		{
		case CAM_COLOREFFECT_OFF:
			TRACE( "Off" );
			break;
		case CAM_COLOREFFECT_VIVID:
			TRACE( "Vivid" );
			break;
		case CAM_COLOREFFECT_SEPIA:
			TRACE( "Sepia" );
			break;
		case CAM_COLOREFFECT_GRAYSCALE:
			TRACE( "Gray-scale" );
			break;
		case CAM_COLOREFFECT_NEGATIVE:
			TRACE( "Negative" );
			break;
		case CAM_COLOREFFECT_SOLARIZE:
			TRACE( "Solarize" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_ColorEffect)stCameraParam.int32 == stShotModeCap.eSupportedColorEffect[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_VIDEO_SUBMODE, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<vidmode: %d>:\tset video sub-mode\n", stCameraParam.int32 );
	for ( i = 0; i < stShotModeCap.iSupportedVideoSubModeCnt; i++ )
	{
		if ( (CAM_VideoSubMode)stCameraParam.int32 == stShotModeCap.eSupportedVideoSubMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedVideoSubMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedVideoSubMode[i] );
		}

		switch ( stShotModeCap.eSupportedVideoSubMode[i] )
		{
		case CAM_VIDEOSUBMODE_SIMPLE:
			TRACE( "Simple" );
			break;
		case CAM_VIDEOSUBMODE_STABILIZER:
			TRACE( "Video Stabilizer" );
			break;
		case CAM_VIDEOSUBMODE_TNR:
			TRACE( "Temporal-Noise Reduction" );
			break;
		case CAM_VIDEOSUBMODE_STABILIZEDTNR:
			TRACE( "Video Stabilizer + Temporal-Noise Reduction" );
			break;
		default:
			ASSERT( 0 );
			break;
		}

		if ( (CAM_VideoSubMode)stCameraParam.int32 == stShotModeCap.eSupportedVideoSubMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_STILL_SUBMODE, (CAM_Param)&eStillSubMode, (CAM_Param)&stStillParam );
	ASSERT_CAM_ERROR( error );

	stCameraParam.param_3int.int1 = eStillSubMode;
	stCameraParam.param_3int.int2 = stStillParam.iBurstCnt;
	stCameraParam.param_3int.int3 = stStillParam.iZslDepth;

	TRACE( "<stillmode: %d, %d, %d>:\tset still sub-mode\n", stCameraParam.param_3int.int1,
	                                                         stCameraParam.param_3int.int2,
	                                                         stCameraParam.param_3int.int3 );
	for ( i = 0; i < stShotModeCap.iSupportedStillSubModeCnt; i++ )
	{
		if ( (CAM_StillSubMode)stCameraParam.param_3int.int1 == stShotModeCap.eSupportedStillSubMode[i] )
		{
			TRACE( "\t[%d - ", stShotModeCap.eSupportedStillSubMode[i] );
		}
		else
		{
			TRACE( "\t%d - ", stShotModeCap.eSupportedStillSubMode[i] );
		}

		switch ( stShotModeCap.eSupportedStillSubMode[i] )
		{
		case CAM_STILLSUBMODE_SIMPLE:
			TRACE( "Simple (param:1,0)" );
			break;
		case CAM_STILLSUBMODE_BURST:
			TRACE( "Burst Capture (param:2~%d,0)", stShotModeCap.iMaxBurstCnt );
			break;
		case CAM_STILLSUBMODE_ZSL:
			TRACE( "Zero Shutter Lag (param:1,1~%d)", stShotModeCap.iMaxZslDepth );
			break;
		case CAM_STILLSUBMODE_BURSTZSL:
			TRACE( "Burst + ZSL (param:2~%d,1~%d)", stShotModeCap.iMaxBurstCnt, stShotModeCap.iMaxZslDepth );
			break;
		case CAM_STILLSUBMODE_HDR:
			TRACE( "High Dynamic Range" );
			break;
		default:
			ASSERT( 0 );
		}

		if ( (CAM_StillSubMode)stCameraParam.param_3int.int1 == stShotModeCap.eSupportedStillSubMode[i] )
		{
			TRACE( "]" );
		}
	}
	TRACE( "\n" );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_BRIGHTNESS, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<b: %d>:\tset brightness (%d ~ %d)\n", stCameraParam.int32, stShotModeCap.iMinBrightness, stShotModeCap.iMaxBrightness );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_CONTRAST, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<c: %d>:\tset contrast (%d ~ %d)\n", stCameraParam.int32, stShotModeCap.iMinContrast, stShotModeCap.iMaxContrast );

	error = CAM_SendCommand( hHandle, CAM_CMD_GET_SATURATION, (CAM_Param)&stCameraParam.int32, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TRACE( "<s: %d>:\tset saturation (%d ~ %d)\n", stCameraParam.int32, stShotModeCap.iMinSaturation, stShotModeCap.iMaxSaturation );

	return error;
}

CAM_Error DequeueUserCmd( CAM_DeviceHandle hHandle, CAM_AppCmd *pCmd, CameraParam *pParam )
{
	char                   buf[256] = {0};
	int                    len = 0;
	float                  temp;
	CAM_Error              error = CAM_ERROR_NONE;

	len = read( STDIN_FILENO, buf, 255 );
	if ( len > 0 )
	{
		buf[len-1] = '\0';
		// TRACE( "cmd: %s\n", buf );
	}
	else
	{
		buf[0] = '\0'; // in case buf is empty
	}

	if ( strcmp(buf, "q") == 0 )
	{
		*pCmd = CMD_EXIT;
	}
	else if ( sscanf(buf, "sensor: %d", &pParam->int32) == 1)
	{
		*pCmd = CMD_SELECTSENSOR;
	}
	else if ( sscanf(buf, "ss: %d x %d", &pParam->size.iWidth, &pParam->size.iHeight) == 2 )
	{
		*pCmd = CMD_STILLSIZE;
	}
	else if ( sscanf(buf, "vs: %d x %d", &pParam->size.iWidth, &pParam->size.iHeight) == 2 )
	{
		*pCmd = CMD_VIDEOSIZE;
	}
	else if ( sscanf(buf, "ps: %d x %d", &pParam->size.iWidth, &pParam->size.iHeight) == 2 )
	{
		*pCmd = CMD_PREVIEWSIZE;
	}
	else if ( sscanf(buf, "pf: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_PREVIEWFORMAT;
	}
	else if ( sscanf(buf, "ssf: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_STILLFORMAT;
	}
	else if ( strcmp(buf, "hp") == 0 )
	{
		*pCmd = CMD_PREPARECAPTURE;
	}
	else if ( strcmp(buf, "p") == 0 )
	{
		*pCmd = CMD_STILLCAPTURE;
	}
	else if ( strcmp(buf, "rp") == 0 )
	{
		*pCmd = CMD_CANCELCAPTURE;
	}
	else if ( strcmp(buf, "r") == 0 )
	{
		*pCmd = CMD_VIDEOONOFF;
	}
	else if ( strcmp(buf, "sf") == 0 )
	{
		*pCmd = CMD_STARTFD;
	}
	else if ( strcmp(buf, "cf") == 0 )
	{
		*pCmd = CMD_CANCELFD;
	}
	else if ( sscanf(buf, "pm: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_SHOTMODE;
	}
	else if ( sscanf(buf, "em: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_EXPMODE;
	}
	else if ( sscanf(buf, "emm: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_EXPMETER;
	}
	else if ( sscanf(buf, "ev: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_EVCOMP;
	}
	else if ( sscanf(buf, "iso: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_ISO;
	}
	else if ( sscanf(buf, "st: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_SHUTTERTIME;
	}
	else if ( sscanf(buf, "fn: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_FNUM;
	}
	else if ( sscanf(buf, "bf: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_BANDFILTER;
	}
	else if ( sscanf(buf, "sm: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_FLASHMODE;
	}
	else if ( sscanf(buf, "wb: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_WB;
	}
	else if ( sscanf(buf, "fm: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_FOCUSMODE;
	}
	else if ( sscanf(buf, "z: %f", &temp) == 1 )
	{
		pParam->int32 = (int)( temp * 65536.0f + 0.5f );
		*pCmd = CMD_DIGZOOM;
	}
	else if ( sscanf(buf, "fr: %f", &temp) == 1 )
	{
		pParam->int32 = (int)( temp * 65536.0f );
		*pCmd = CMD_FRAMERATE;
	}
	else if ( sscanf(buf, "ce: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_COLOREFFECT;
	}
	else if ( sscanf(buf, "b: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_BRIGHTNESS;
	}
	else if ( sscanf(buf, "c: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_CONTRAST;
	}
	else if ( sscanf(buf, "s: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_SATURATION;
	}
	else if ( sscanf(buf, "so: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_SENSORFLIP;
	}
	else if ( sscanf(buf, "vidmode: %d", &pParam->int32) == 1 )
	{
		*pCmd = CMD_VIDSUBMODE;
	}
	else if ( sscanf(buf, "stillmode: %d, %d, %d", &pParam->param_3int.int1, &pParam->param_3int.int2, &pParam->param_3int.int3) == 3 )
	{
		*pCmd = CMD_STILLSUBMODE;
	}
	else
	{
		*pCmd = CMD_NULL;
	}

	return CAM_ERROR_NONE;
}

CAM_Error SetParameters( CAM_DeviceHandle hHandle, CAM_AppCmd cmd, CameraParam stCameraParam )
{
	CAM_Error        error = CAM_ERROR_NONE;
	CAM_CaptureState eState;

	switch ( cmd )
	{
	case CMD_DIGZOOM:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_DIGZOOM, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "digital zoom changed to %.2f\n", stCameraParam.int32 / 65536.0f );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_FRAMERATE:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_FRAMERATE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "frame rate changed to %.2f\n", stCameraParam.int32 / 65536.0f );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_STILLSIZE:
		error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		if ( eState == CAM_CAPTURESTATE_STILL )
		{
			TRACE( "Pls switch to preview state for still port size setting\n" );
		}
		else
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_STILL, (CAM_Param)&(stCameraParam.size) );
			ASSERT_CAM_ERROR( error );
		}
		break;


	case CMD_VIDEOSIZE:
		error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		if ( eState == CAM_CAPTURESTATE_VIDEO )
		{
			TRACE( "Pls switch to preview state for video port size setting\n" );
		}
		else
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&(stCameraParam.size) );
			ASSERT_CAM_ERROR( error );
		}
		break;


	case CMD_PREVIEWSIZE:
		error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		if ( eState == CAM_CAPTURESTATE_PREVIEW )
		{
			StopPreview( hHandle );
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&(stCameraParam.size) );
			ASSERT_CAM_ERROR( error );
			StartPreview( hHandle );
		}
		else if ( eState == CAM_CAPTURESTATE_VIDEO )
		{
			TRACE( "Pls switch to preview state for preview port size setting\n" );
		}
		else
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&(stCameraParam.size) );
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_PREVIEWFORMAT:
		error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		if ( eState == CAM_CAPTURESTATE_PREVIEW )
		{
			StopPreview( hHandle );
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)stCameraParam.int32 );
			ASSERT_CAM_ERROR( error );
			StartPreview( hHandle );
		}
		else if ( eState == CAM_CAPTURESTATE_VIDEO )
		{
			TRACE( "Pls switch to preview state for preview port format setting\n" );
		}
		else
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&(stCameraParam.int32) );
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_STILLFORMAT:
		error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		if ( eState != CAM_CAPTURESTATE_PREVIEW )
		{
			TRACE( "Pls switch to preview state for still port format setting\n" );
		}
		else
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_FORMAT, (CAM_Param)CAM_PORT_STILL, (CAM_Param)stCameraParam.int32 );
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_SHOTMODE:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_SHOTMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Photo mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_VIDSUBMODE:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_VIDEO_SUBMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Video sub-mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_STILLSUBMODE:
	{
		CAM_StillSubMode      eStillSubMode = stCameraParam.param_3int.int1;
		CAM_StillSubModeParam stStillParam;

		stStillParam.iBurstCnt = stCameraParam.param_3int.int2;
		stStillParam.iZslDepth = stCameraParam.param_3int.int3;

		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STILL_SUBMODE, (CAM_Param)eStillSubMode, (CAM_Param)&stStillParam );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Still sub-mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
	}
		break;

	case CMD_SENSORFLIP:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_SENSOR_ORIENTATION, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Sensor rotation is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_FLASHMODE:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_FLASHMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Flash mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_EVCOMP:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVCOMPENSATION, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_SHOTMODELIMIT == error )
		{
			TRACE( "NOT allowed in current photo mode!\n" );
		}
		else if ( CAM_ERROR_NONE == error )
		{
			TRACE( "EV is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_COLOREFFECT:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_COLOREFFECT, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Color effect is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_ISO:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_ISO, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "ISO is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_SHUTTERTIME:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_SHUTTERSPEED, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Shutter speed is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_FNUM:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_FNUM, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "F-Number is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_WB:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_WHITEBALANCEMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "White balance mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_SATURATION:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_SATURATION, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Saturation is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_BRIGHTNESS:
		error = CAM_SendCommand( hHandle,CAM_CMD_SET_BRIGHTNESS , (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Brightness is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_CONTRAST:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_CONTRAST, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Contrast is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_EXPMODE:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_EXPOSUREMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Exposure mode is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_EXPMETER:
		{
			if ( (CAM_ExposureMeterMode)stCameraParam.int32 == CAM_EXPOSUREMETERMODE_MANUAL )
			{
				CAM_MultiROI  stExpMeterROI;

				memset( &stExpMeterROI, 0, sizeof( CAM_MultiROI ) );

				stExpMeterROI.iROICnt       = 1;
				stExpMeterROI.stWeiRect[0].stRect.iLeft   = 500;
				stExpMeterROI.stWeiRect[0].stRect.iTop    = 500;
				stExpMeterROI.stWeiRect[0].stRect.iWidth  = 500;
				stExpMeterROI.stWeiRect[0].stRect.iHeight = 500;

				error = CAM_SendCommand( hHandle, CAM_CMD_SET_EXPOSUREMETERMODE, (CAM_Param)stCameraParam.int32, (CAM_Param)(&stExpMeterROI) );
			}
			else
			{
				error = CAM_SendCommand( hHandle, CAM_CMD_SET_EXPOSUREMETERMODE, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
			}

			if ( CAM_ERROR_NONE == error )
			{
				TRACE( "Exposure meter mode is changed\n" );
			}
			else
			{
				ASSERT_CAM_ERROR( error );
			}
		}
		break;

	case CMD_BANDFILTER:
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_BANDFILTER, (CAM_Param)stCameraParam.int32, UNUSED_PARAM );
		if ( CAM_ERROR_NONE == error )
		{
			TRACE( "Band filter is changed\n" );
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CMD_FOCUSMODE:
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_SET_FOCUSMODE, (CAM_Param)(stCameraParam.int32), (CAM_Param)NULL );
			if ( CAM_ERROR_NONE == error )
			{
				TRACE( "Focus mode is changed\n" );
			}
			else
			{
				ASSERT_CAM_ERROR( error );
			}
		}
		break;

	default:
		TRACE( "Invalid command\n" );
		break;
	}

	return error;
}


CAM_Error DeliverPreviewBuffer( CAM_DeviceHandle hHandle, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error  error = CAM_ERROR_NONE;
	int        i, ret;
	static int iPreviewFrameCnt = 0;

#ifdef PREVIEW_DUMP
	if ( fpPreviewDump == NULL )
	{
#if defined( ANDROID )
		fpPreviewDump = fopen( "/data/preview.yuv", "wb" );
#else
		fpPreviewDump = fopen( "preview.yuv", "wb" );
#endif
	}

	if ( iPreviewFrameCnt < PREVIEW_DUMP )
	{
		for ( i = 0; i < 3; i++ )
		{
			if ( pImgBuf->iFilledLen[i] > 0 )
			{
				fwrite( pImgBuf->pBuffer[i], 1, pImgBuf->iFilledLen[i], fpPreviewDump );
			}
		}

	}

	if ( iPreviewFrameCnt == PREVIEW_DUMP )
	{
		fclose( fpPreviewDump );
	}
#endif

	// dump specified frame
	if ( bDumpFrame )
	{
#if defined( ANDROID )
		FILE *fp = fopen( "/data/preview_dump.yuv", "wb" );
#else
		FILE *fp = fopen( "preview_dump.yuv", "wb" );
#endif
		for ( i = 0; i < 3; i++ )
		{
			if ( pImgBuf->iFilledLen[i] > 0 )
			{
				fwrite( pImgBuf->pBuffer[i], 1, pImgBuf->iFilledLen[i], fp );
			}
		}
		fclose( fp );
	}

	// display to overlay
	ret = displaydemo_display( hHandle, &stDispCfg, pImgBuf );
	if ( ret != 0 )
	{
		displaydemo_close( &stDispCfg );
	}
	ASSERT( ret == 0 );

	if ( iPreviewFrameCnt % PRINT_INTERVAL == 0 )
	{
		TRACE( "preview %d\n", iPreviewFrameCnt );
	}
	iPreviewFrameCnt++;

	return error;
}

CAM_Error DeliverSnapshotBuffer( CAM_DeviceHandle hHandle, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error  error = CAM_ERROR_NONE;
	int        i;
	static int iSnapshotFrameCnt = 0;

	if ( iSnapshotFrameCnt < SNAPSHOT_DUMP )
	{
		if ( fpSnapshotDump == NULL )
		{
#if defined( ANDROID )
			fpSnapshotDump = fopen( "/data/snapshot.yuv", "wb" );
#else
			fpSnapshotDump = fopen( "snapshot.yuv", "wb" );
#endif
		}

		for ( i = 0; i< 3; i++ )
		{
			if ( pImgBuf->iFilledLen[i] > 0 )
			{
				fwrite( pImgBuf->pBuffer[i], 1, pImgBuf->iFilledLen[i], fpSnapshotDump );
			}
		}

		if ( iSnapshotFrameCnt == SNAPSHOT_DUMP - 1 )
		{
			fclose( fpSnapshotDump );
		}
	}

	TRACE( "snapshot %d\n", iSnapshotFrameCnt );

	displaydemo_display( hHandle, &stDispCfg, pImgBuf );

	iSnapshotFrameCnt++;

	return error;
}


CAM_Error ExtractThumbnailAndDisplay( CAM_ImageBuffer *pImgBuf )
{
	TRACE( "TODO: ExtractThumbnailAndDisplay() is NOT implemented yet!\n" );

	return CAM_ERROR_NONE;
}

CAM_Error PrintShotInformation( CAM_ImageBuffer *pImgBuf )
{
	TRACE( " ===========================Shoting information start======================\n");
	TRACE( "ISP Gain    :%15.9f\n",pImgBuf->stRawShotInfo.usIspGain/256.f );
	TRACE( "ExposureTime:%15.9f\n",pImgBuf->stShotInfo.iExposureTimeQ16/65536.f );
	TRACE( "Analog Gain :%15.9f\n",pImgBuf->stRawShotInfo.usAGain[0]/256.f );
	TRACE( "Digital Gain:%15.9f\n",pImgBuf->stRawShotInfo.usDGain[0]/256.f );
	TRACE( "AWB Preset  :%s (%u)\n",awbPreset[pImgBuf->stRawShotInfo.ucAWBPreset], pImgBuf->stRawShotInfo.ucAWBPreset );
	TRACE( "WbRedScale  :%15.9f\n",pImgBuf->stRawShotInfo.usWbRedScale/256.f );
	TRACE( "WBBlueScale :%15.9f\n",pImgBuf->stRawShotInfo.usWbBlueScale/256.f );
	TRACE( " ===========================Shoting information End======================\n");

	return CAM_ERROR_NONE;
}

CAM_Bool _is_bayer_format( CAM_ImageFormat eFormat )
{
	CAM_Bool bIsBayerFormat = CAM_FALSE;

	bIsBayerFormat = ( eFormat == CAM_IMGFMT_RGGB8 ) || ( eFormat == CAM_IMGFMT_BGGR8 ) ||
	                 ( eFormat == CAM_IMGFMT_GRBG8 ) || ( eFormat == CAM_IMGFMT_GBRG8 ) ||
	                 ( eFormat == CAM_IMGFMT_RGGB10 ) || ( eFormat == CAM_IMGFMT_BGGR10 ) ||
	                 ( eFormat == CAM_IMGFMT_GRBG10 ) || ( eFormat == CAM_IMGFMT_GBRG10 );

	return bIsBayerFormat;
}


CAM_Error DeliverStillBuffer( CAM_DeviceHandle hHandle, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error  error = CAM_ERROR_NONE;
	int        i;
	static int cnt = 0;
	CAM_ImageFormat eFormat;

	if ( bSaveStill )
	{
		char     sFileName[256] = {0};
		FILE     *fp            = NULL;
		char     sSuffix[2][4] = { {"jpg"}, {""} };


		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_FORMAT, (CAM_Param)CAM_PORT_STILL, &eFormat );
		ASSERT_CAM_ERROR( error );
/*
		CAM_Tick t;
		// allocate zoomed buffer
		CAM_ImageBufferReq stBufReq = stStillBufReq;
		CAM_ImageBuffer *pTmpBuf = NULL;
		stBufReq.iMinBufCount = 1;
		error = AllocateImages( &stBufReq, &pTmpBuf );
		ASSERT_CAM_ERROR( error );

		// call zoom
		t = -CAM_TimeGetTickCount();
		ipp_jpeg_zoom( pImgBuf, pTmpBuf, 300, 90 );
		t += CAM_TimeGetTickCount();
		TRACE( "ipp_jpeg_zoom cost %.2f ms\n", t / 1000.0 );
*/
#if defined( ANDROID )
		sprintf( sFileName, "/data/IMG%d%d%d.%s", (cnt / 100) % 10, (cnt / 10) % 10, cnt % 10, sSuffix[_is_bayer_format(eFormat) ? 1 : 0] );
#else
		sprintf( sFileName, "IMG%d%d%d.%s", (cnt / 100) % 10, (cnt / 10) % 10, cnt % 10, sSuffix[_is_bayer_format(eFormat) ? 1 : 0] );
#endif
		PrintShotInformation( pImgBuf );

		if ( _is_bayer_format(eFormat) )
		{
			TRACE( "Store image location: %s\n", sFileName );
			GenerateDxOFiles( pImgBuf, FORMAT_RAW, 0, ENCODING_YUV_601FS, E_COLOR_SPACE_STILL, sFileName );
		}
		else
		{
			TRACE( "Store image location: %s\n", sFileName );
			fp = fopen( sFileName, "wb" );
			if ( fp == NULL )
			{
				TRACE( "Can not open file: %s\n", sFileName );
				return CAM_ERROR_OUTOFRESOURCE;
			}

			for ( i = 0; i< 3; i++ )
			{
				if ( pImgBuf->iFilledLen[i] > 0 )
				{
					fwrite( pImgBuf->pBuffer[i], 1, pImgBuf->iFilledLen[i], fp );
				}
			}

			fflush( fp );
			fclose( fp );
		}
	}

	iExpectedPicNum--;
	if ( iExpectedPicNum == 0 )
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_STILL, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );

		error = FreeImages( &pStillBuf, stStillBufReq.iMinBufCount );
		ASSERT_CAM_ERROR( error );
	}
	else
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)CAM_PORT_STILL, (CAM_Param)pImgBuf );
		ASSERT_CAM_ERROR( error );
	}
	cnt++;

	return error;
}

CAM_Error DeliverVideoBuffer( CAM_DeviceHandle hHandle, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error  error          = CAM_ERROR_NONE;
	static int iVideoFrameCnt = 0;

	if ( iVideoFrameCnt % PRINT_INTERVAL == 0 )
	{
		TRACE( "video %d\n", iVideoFrameCnt );
	}

	VideoEncoder_DeliverInput( hCamcorderHandle, pImgBuf );
	iVideoFrameCnt++;

	return error;
}

CAM_Error AllocateImages( CAM_ImageBufferReq *pBufReq, CAM_ImageBuffer **ppImgBuf )
{
	int             i, iTotalLen;
	CAM_ImageBuffer *pImgBuf;
	int             iExtraLen = 0;

	pImgBuf     = (CAM_ImageBuffer*)malloc( sizeof( CAM_ImageBuffer ) * pBufReq->iMinBufCount );
	(*ppImgBuf) = pImgBuf;

	for ( i = 0; i < pBufReq->iMinBufCount; i++ )
	{
		pImgBuf[i].eFormat       = pBufReq->eFormat;
		pImgBuf[i].iOffset[0]    = 0;
		pImgBuf[i].iOffset[1]    = 0;
		pImgBuf[i].iOffset[2]    = 0;
		pImgBuf[i].iStep[0]      = pBufReq->iMinStep[0];
		pImgBuf[i].iStep[1]      = pBufReq->iMinStep[1];
		pImgBuf[i].iStep[2]      = pBufReq->iMinStep[2];

		if ( pImgBuf[i].eFormat == CAM_IMGFMT_CbYCrY )
		{
			iExtraLen = 4096 + 1920 * 8 * 2;
		}

		pImgBuf[i].iAllocLen[0]  = pBufReq->iMinBufLen[0] + pImgBuf[i].iOffset[0] + iExtraLen;
		pImgBuf[i].iAllocLen[1]  = pBufReq->iMinBufLen[1] + pImgBuf[i].iOffset[1];
		pImgBuf[i].iAllocLen[2]  = pBufReq->iMinBufLen[2] + pImgBuf[i].iOffset[2];
		pImgBuf[i].iFilledLen[0] = 0;
		pImgBuf[i].iFilledLen[1] = 0;
		pImgBuf[i].iFilledLen[2] = 0;
		pImgBuf[i].iWidth        = pBufReq->iWidth;
		pImgBuf[i].iHeight       = pBufReq->iHeight;
		pImgBuf[i].iUserIndex    = i;
		iTotalLen                = pImgBuf[i].iAllocLen[0] + pBufReq->iAlignment[0] +
		                           pImgBuf[i].iAllocLen[1] + pBufReq->iAlignment[1] +
		                           pImgBuf[i].iAllocLen[2] + pBufReq->iAlignment[2];
		pImgBuf[i].pUserData     = osalbmm_malloc( iTotalLen, OSALBMM_ATTR_NONCACHED );
		pImgBuf[i].iPrivateIndex = 0;
		pImgBuf[i].pPrivateData  = NULL;
		pImgBuf[i].tTick         = 0;
		pImgBuf[i].pBuffer[0]    = (CAM_Int8u*)ALIGN_TO( (CAM_Int32s)(pImgBuf[i].pUserData), pBufReq->iAlignment[0] );
		pImgBuf[i].pBuffer[1]    = (CAM_Int8u*)ALIGN_TO( (CAM_Int32s)(pImgBuf[i].pBuffer[0] + pImgBuf[i].iAllocLen[0]), pBufReq->iAlignment[1] );
		pImgBuf[i].pBuffer[2]    = (CAM_Int8u*)ALIGN_TO( (CAM_Int32s)(pImgBuf[i].pBuffer[1] + pImgBuf[i].iAllocLen[1]), pBufReq->iAlignment[2] );
		pImgBuf[i].iPhyAddr[0]   = osalbmm_get_paddr( pImgBuf[i].pBuffer[0] );
		pImgBuf[i].iPhyAddr[1]   = osalbmm_get_paddr( pImgBuf[i].pBuffer[1] );
		pImgBuf[i].iPhyAddr[2]   = osalbmm_get_paddr( pImgBuf[i].pBuffer[2] );
		pImgBuf[i].iFlag         = BUF_FLAG_PHYSICALCONTIGUOUS |
		                           BUF_FLAG_L1NONCACHEABLE | BUF_FLAG_L2NONCACHEABLE | BUF_FLAG_UNBUFFERABLE |
		                           BUF_FLAG_YUVBACKTOBACK | BUF_FLAG_FORBIDPADDING;

		if ( pImgBuf[i].pUserData == NULL )
		{
			TRACE( "failed to %d osalbmm_malloc( %d, ... )\n", i, iTotalLen );
			for ( i = i - 1; i >= 0; i-- )
			{
				osalbmm_free( pImgBuf[i].pUserData );
			}
			free( pImgBuf );
			(*ppImgBuf) = NULL;
			return CAM_ERROR_OUTOFMEMORY;
		}

		// invalidate osalbmm buffer's cache line
		osalbmm_flush_cache_range( pImgBuf[i].pUserData, iTotalLen, OSALBMM_DMA_FROM_DEVICE );


		// memset( pImgBuf[i].pBuffer[0], 0, pImgBuf[i].iAllocLen[0] );
		// memset( pImgBuf[i].pBuffer[1], 0, pImgBuf[i].iAllocLen[1] );
		// memset( pImgBuf[i].pBuffer[2], 0, pImgBuf[i].iAllocLen[2] );
	}

	return CAM_ERROR_NONE;
}

CAM_Error FreeImages( CAM_ImageBuffer **ppImgBuf, int iCount )
{
	int             i;
	CAM_ImageBuffer *pImgBuf;

	pImgBuf = (*ppImgBuf);

	for ( i = 0; i < iCount; i++ )
	{
		osalbmm_free( pImgBuf[i].pUserData );
	}

	free( pImgBuf );

	*ppImgBuf = NULL;

	return CAM_ERROR_NONE;
}

// Required by IPP lib
void* _CALLBACK ippiMalloc( int size )
{
	return malloc( size );
}

void  _CALLBACK ippiFree( void *pSrcBuf )
{
	free( pSrcBuf );
	return;
}
