/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#ifndef _CAM_EXTISP_SENSORHAL_H_
#define _CAM_EXTISP_SENSORHAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "CameraEngine.h"
#include "cam_socdef.h"
#include "cam_gen.h"

typedef struct 
{
	CAM_Bool                       bArbitraryVideoSize;
	CAM_Size                       stMinVideoSize;
	CAM_Size                       stMaxVideoSize;

	CAM_Int32s                     iSupportedVideoSizeCnt;
	CAM_Size                       stSupportedVideoSize[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT];

	CAM_Int32s                     iSupportedVideoFormatCnt;
	CAM_ImageFormat                eSupportedVideoFormat[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT];

	CAM_Bool                       bArbitraryStillSize;
	CAM_Size                       stMinStillSize;
	CAM_Size                       stMaxStillSize;

	CAM_Int32s                     iSupportedStillSizeCnt;
	CAM_Size                       stSupportedStillSize[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT];

	CAM_Int32s                     iSupportedStillFormatCnt;
	CAM_ImageFormat                eSupportedStillFormat[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT];

	CAM_Int32s                     iSupportedRotateCnt;
	CAM_FlipRotate                 eSupportedRotate[CAM_FLIPROTATE_NUM];

	CAM_Int32s                     iSupportedShotModeCnt;
	CAM_ShotMode                   eSupportedShotMode[CAM_SHOTMODE_NUM];

	// all supported shot paramters
	CAM_ShotModeCapability         stSupportedShotParams;

	CAM_JpegCapability             stSupportedJPEGParams;

	CAM_Int32s                     iMaxFaceNum;   // max supported facing number of face detection
} _CAM_SmartSensorCapability;


typedef struct
{
	CAM_CaptureState               eState;

	CAM_Int32s                     iWidth;
	CAM_Int32s                     iHeight;
	CAM_ImageFormat                eFormat;
	CAM_JpegParam                  stJpegParam;

	// reset type
	CAM_Int32s                     iResetType;

	// output
	CAM_Bool                       bFlushed;
} _CAM_SmartSensorConfig;

typedef struct _cam_smartsensor_attri* _CAM_SmartSensorAttri_T;

typedef struct
{
	CAM_Error (*GetCapability)( _CAM_SmartSensorCapability *pCapability );
	CAM_Error (*GetShotModeCapability)( CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap );
	
	CAM_Error (*SelfDetect)( _CAM_SmartSensorAttri_T pSensorData );
	CAM_Error (*Init)( void **ppDevice, void *pSensorEntry );
	CAM_Error (*Deinit)( void **pDevice );
	CAM_Error (*RegisterEventHandler)( void *pDevice, CAM_EventHandler fnEventHandler, void *pUserData );
	CAM_Error (*Config)( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig );
	CAM_Error (*GetBufReq)( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig, CAM_ImageBufferReq *pBufReq );
	CAM_Error (*Enqueue)( void *pDevice, CAM_ImageBuffer *pImgBuf );
	CAM_Error (*Dequeue)( void *pDevice, CAM_ImageBuffer **ppImgBuf );
	CAM_Error (*Flush)( void *pDevice );
	CAM_Error (*SetShotParam)( void *pDevice, _CAM_ShotParam *pSettings );
	CAM_Error (*GetShotParam)( void *pDevice, _CAM_ShotParam *pSettings );
	CAM_Error (*StartFocus)( void *pDevice );
	CAM_Error (*CancelFocus)( void *pDevice );
	CAM_Error (*CheckFocusState)( void *pDevice, CAM_Bool *pFocusAutoStopped, CAM_FocusState *pFocusState );
	CAM_Error (*GetDigitalZoomCapability)( CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s *pSensorDigZoomQ16 );
	CAM_Error (*StartFaceDetection)( void *pDevice );
	CAM_Error (*CancelFaceDetection)( void *pDevice );
} _CAM_SmartSensorFunc;

typedef struct
{
	CAM_Int8s             sSensorName[32];
	_CAM_SmartSensorFunc  *pFunc;
} _CAM_SmartSensorEntry;

typedef struct _cam_smartsensor_attri
{
	CAM_DeviceProp       stSensorProp;
	_CAM_SmartSensorFunc *pFunc;
	CAM_Int8s            cReserved[50]; // a field reverved for sensor entry in differnt platform
} _CAM_SmartSensorAttri;

#ifdef __cplusplus
}
#endif

#endif  // _CAM_EXTISP_SENSORHAL_H_
