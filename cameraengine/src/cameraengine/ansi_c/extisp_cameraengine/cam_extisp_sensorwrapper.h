/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#ifndef _CAM_EXTISP_SENSORWRAPPER_H_
#define _CAM_EXTISP_SENSORWRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_extisp_sensorhal.h"

CAM_Error SmartSensor_EnumSensors( CAM_Int32s *pSensorCount, CAM_DeviceProp stCameraProp[CAM_MAX_SUPPORT_CAMERA] );
CAM_Error SmartSensor_GetCapability( CAM_Int32s iSensorID, _CAM_SmartSensorCapability *pCapability );
CAM_Error SmartSensor_GetShotModeCapability( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap );
CAM_Error SmartSensor_GetDigitalZoomCapability( CAM_Int32s iSensorID, CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s *pSensorDigZoomQ16 );

CAM_Error SmartSensor_Init( void **phDevice, CAM_Int32s iSensorID );
CAM_Error SmartSensor_Deinit( void **phDevice );
CAM_Error SmartSensor_RegisterEventHandler( void *hDevice, CAM_EventHandler fnEventHandler, void *pUserData );
CAM_Error SmartSensor_Config( void *hDevice, _CAM_SmartSensorConfig *pSensorConfig );
CAM_Error SmartSensor_GetBufReq( void *hDevice, _CAM_SmartSensorConfig *pSensorConfig, CAM_ImageBufferReq *pBufReq );

CAM_Error SmartSensor_Enqueue( void *hDevice, CAM_ImageBuffer *pImgBuf );
CAM_Error SmartSensor_Dequeue( void *hDevice, CAM_ImageBuffer **ppImgBuf );
CAM_Error SmartSensor_Flush( void *hDevice );

CAM_Error SmartSensor_SetShotParam( void *hDevice, _CAM_ShotParam *pSettings );
CAM_Error SmartSensor_GetShotParam( void *hDevice, _CAM_ShotParam *pSettings );
CAM_Error SmartSensor_StartFocus( void *hDevice );
CAM_Error SmartSensor_CancelFocus( void *hDevice );
CAM_Error SmartSensor_CheckFocusState( void *hDevice, CAM_Bool *pFocusAutoStopped, CAM_FocusState *pFocusState );
CAM_Error SmartSensor_StartFaceDetection( void *hDevice );
CAM_Error SmartSensor_CancelFaceDetection( void *hDevice );

#ifdef __cplusplus
}
#endif

#endif  // _CAM_EXTISP_SENSORWRAPPER_H_
