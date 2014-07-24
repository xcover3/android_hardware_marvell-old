/*******************************************************************************
// (C) Copyright [2010 - 2011] Marvell International Ltd.
// All Rights Reserved
*******************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "cam_utility.h"
#include "cam_log.h"
#include "cam_extisp_sensorwrapper.h"

typedef struct 
{
	CAM_Int32s       iSensorID;
	void             *pDevice;
} _CAM_SmartSensorState;

extern const _CAM_SmartSensorEntry gSmartSensorTable[];
extern const CAM_Int32s            gSmartSensorCnt;

// sensors in current platform
static _CAM_SmartSensorAttri gCurrentSensorTable[4];
static CAM_Int32s            gCurrentSensorCnt  = 0;


CAM_Error SmartSensor_EnumSensors( CAM_Int32s *pSensorCount, CAM_DeviceProp stCameraProp[CAM_MAX_SUPPORT_CAMERA] )
{
	CAM_Int32s            i = 0;
	CAM_Error             error = CAM_ERROR_NONE;
	_CAM_SmartSensorAttri stDetectedSensor;

	// bad argument check
	if ( pSensorCount == NULL || stCameraProp == NULL )
	{
		return CAM_ERROR_BADPOINTER;
	}

	if ( gCurrentSensorCnt == 0 )
	{
		for ( i = 0; i < gSmartSensorCnt; i++ )
		{
			memset( &stDetectedSensor, 0, sizeof( _CAM_SmartSensorAttri ) );

			error = gSmartSensorTable[i].pFunc->SelfDetect( &stDetectedSensor );
			if ( CAM_ERROR_NONE != error )
			{
				*pSensorCount = 0;
				TRACE( CAM_ERROR, "Error: cannot complete sensor self-detect, pls confirm your sensor/sensor driver/system are all ready( %s, %s, %d )\n", 
				       __FILE__, __FUNCTION__, __LINE__ );
				return error;
			}
			
			if ( stDetectedSensor.pFunc != NULL )
			{
				memcpy( &stCameraProp[gCurrentSensorCnt], &stDetectedSensor.stSensorProp, sizeof( CAM_DeviceProp ) );

				memcpy( &gCurrentSensorTable[gCurrentSensorCnt], &stDetectedSensor, sizeof( _CAM_SmartSensorAttri ) );

				gCurrentSensorCnt++;

				if ( gCurrentSensorCnt >= CAM_MAX_SUPPORT_CAMERA )
				{
					TRACE( CAM_ERROR, "Error: Camera Engine cannot support more than %d sensors in one platform, "
					       "so the potential more camera sensors will be ignored ( %s, %s, %d )\n",
					       CAM_MAX_SUPPORT_CAMERA, __FILE__, __FUNCTION__, __LINE__ );
					break;
				}
			}
		}

		*pSensorCount = gCurrentSensorCnt;
	}
	else
	{
		for ( i = 0; i < gCurrentSensorCnt; i++ )
		{
			memcpy( &stCameraProp[i], &gCurrentSensorTable[i].stSensorProp, sizeof( CAM_DeviceProp ) );
		}
		*pSensorCount = gCurrentSensorCnt;
	}

	return CAM_ERROR_NONE;
}

CAM_Error SmartSensor_GetCapability( CAM_Int32s iSensorID, _CAM_SmartSensorCapability *pCapability )
{
	CAM_Error error = CAM_ERROR_NONE;

	if ( iSensorID < 0 || iSensorID >= gCurrentSensorCnt )
	{
		TRACE( CAM_ERROR, "Error: sensor ID[ %d ] out of range( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	memset( pCapability, 0, sizeof( _CAM_SmartSensorCapability ) );

	error = gCurrentSensorTable[iSensorID].pFunc->GetCapability( pCapability );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: get capability failed for sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	// This is the basic requirement to the sensor HAL implementations
	// If sensor HAL does not conform to this assumption, upper level may not work correctly
	ASSERT( pCapability->iSupportedVideoSizeCnt > 0 );
	ASSERT( pCapability->iSupportedVideoFormatCnt > 0 );
	ASSERT( pCapability->iSupportedStillSizeCnt > 0 );
	ASSERT( pCapability->iSupportedStillFormatCnt > 0 );

	return error;
}

CAM_Error SmartSensor_GetShotModeCapability( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Error error = CAM_ERROR_NONE;

	if ( iSensorID < 0 || iSensorID >= gCurrentSensorCnt )
	{
		TRACE( CAM_ERROR, "Error: sensor ID[ %d ] out of range( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[iSensorID].pFunc->GetShotModeCapability( eShotMode, pShotModeCap );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: get shot mode capability failed for sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_GetDigitalZoomCapability( CAM_Int32s iSensorID, CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s *pSensorDigZoomQ16 )
{
	CAM_Error error = CAM_ERROR_NONE;

	if ( iSensorID < 0 || iSensorID >= gCurrentSensorCnt )
	{
		TRACE( CAM_ERROR, "Error: sensor ID[ %d ] out of range( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[iSensorID].pFunc->GetDigitalZoomCapability( iWidth, iHeight, pSensorDigZoomQ16 );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: get digital zoom capability failed for sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_Init( void **phDevice, int iSensorID )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)malloc( sizeof( _CAM_SmartSensorState ) );
	if ( pState == NULL )
	{
		TRACE( CAM_ERROR, "Error: No memeory for sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}

	error = gCurrentSensorTable[iSensorID].pFunc->Init( &pState->pDevice, (void*)(gCurrentSensorTable[iSensorID].cReserved) );
	if ( CAM_ERROR_NONE == error )
	{
		pState->iSensorID = iSensorID;
		*phDevice         = pState;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: initialization failed for sensor[%d]( %s, %s, %d )\n", iSensorID, __FILE__, __FUNCTION__, __LINE__ );
		free( pState );
		*phDevice = NULL;
	}

	return error;
}

CAM_Error SmartSensor_Deinit( void **phDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = NULL;

	if ( phDevice == NULL || *phDevice == NULL || ((_CAM_SmartSensorState*)(*phDevice))->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	pState = (_CAM_SmartSensorState*)(*phDevice);
	error = gCurrentSensorTable[pState->iSensorID].pFunc->Deinit( &pState->pDevice );
	ASSERT( CAM_ERROR_NONE == error );

	free( pState );
	pState    = NULL;
	*phDevice = NULL;

	return error;
}

CAM_Error SmartSensor_RegisterEventHandler( void *hDevice, CAM_EventHandler fnEventHandler, void *pUserData )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->RegisterEventHandler( pState->pDevice, fnEventHandler, pUserData );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: register event handler failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_Config( void *hDevice, _CAM_SmartSensorConfig *pSensorConfig )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->Config( pState->pDevice, pSensorConfig );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: config failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_GetBufReq( void *hDevice, _CAM_SmartSensorConfig *pSensorConfig, CAM_ImageBufferReq *pBufReq )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->GetBufReq(pState->pDevice, pSensorConfig, pBufReq);
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: get buffer requirement failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_Enqueue( void *hDevice, CAM_ImageBuffer *pImgBuf )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->Enqueue( pState->pDevice, pImgBuf );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: enqueue buffer failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_Dequeue( void *hDevice, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->Dequeue( pState->pDevice, ppImgBuf );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: dequeue buffer failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_Flush( void *hDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->Flush( pState->pDevice );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: flush buffer failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_SetShotParam( void *hDevice, _CAM_ShotParam *pSettings )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->SetShotParam( pState->pDevice, pSettings );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: set shot parameters failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_GetShotParam( void *hDevice, _CAM_ShotParam *pSettings )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	memset( pSettings, 0, sizeof(_CAM_ShotParam) );

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->GetShotParam( pState->pDevice, pSettings );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: get shot parameters failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_StartFocus( void *hDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->StartFocus( pState->pDevice );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: start focus failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_CancelFocus( void *hDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->CancelFocus( pState->pDevice );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: cancel focus failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_CheckFocusState( void *hDevice, CAM_Bool *pFocusAutoStopped, CAM_FocusState *pFocusState )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->CheckFocusState( pState->pDevice, pFocusAutoStopped, pFocusState );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: check focus state failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_StartFaceDetection( void *hDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->StartFaceDetection( pState->pDevice );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: start face detection failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}

CAM_Error SmartSensor_CancelFaceDetection( void *hDevice )
{
	CAM_Error             error   = CAM_ERROR_NONE;
	_CAM_SmartSensorState *pState = (_CAM_SmartSensorState*)hDevice;

	if ( pState == NULL || pState->pDevice == NULL )
	{
		TRACE( CAM_ERROR, "Error: invalid sensor handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADSENSORID;
	}

	error = gCurrentSensorTable[pState->iSensorID].pFunc->CancelFaceDetection( pState->pDevice );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: cancel face detection failed for sensor[%d]( %s, %s, %d )\n", pState->iSensorID, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}
