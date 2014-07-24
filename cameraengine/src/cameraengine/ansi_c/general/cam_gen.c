/******************************************************************************
//(C) Copyright [2009 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/


#include <stdlib.h>
#include <string.h>

#include "CameraEngine.h"

#include "cam_log.h"
#include "cam_trace.h"
#include "cam_gen.h"


CAM_Error CAM_GetHandle( CAM_DeviceHandle *phHandle )
{
	CAM_Error         error   = CAM_ERROR_NONE;
	_CAM_DeviceState  *pState = NULL;
	CAM_Int32s        iExtIspCnt, iSocIspCnt;

	CELOG( "Welcome to use Marvell Camera Engine version %d.%d, deliverd by APSE-IPP Shanghai\n", CAM_ENGINE_VERSION_MAJOR, CAM_ENGINE_VERSION_MINOR );

	_CHECK_BAD_POINTER( phHandle );

	pState = (_CAM_DeviceState*)malloc( sizeof( _CAM_DeviceState ) );
	if ( pState == NULL )
	{
		TRACE( CAM_ERROR, "Error: out of memory to allocate for Camera Engine handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		*phHandle = NULL;
		return CAM_ERROR_OUTOFMEMORY;
	}

	memset( pState, 0, sizeof( _CAM_DeviceState ) );

#if defined( EXT )
	// enum ext-isp sensors firstly
	error = entry_extisp.pCommand( NULL, CAM_CMD_ENUM_SENSORS, &iExtIspCnt, pState->stDeviceProp );
	if ( error != CAM_ERROR_NONE )
	{
		free( pState );
		*phHandle = NULL;
		return error;
	}
#else
	iExtIspCnt = 0;
#endif

#if defined( DXO )
	// enum soc-isp sensors
	error = entry_socisp.pCommand( NULL, CAM_CMD_ENUM_SENSORS, &iSocIspCnt, &( pState->stDeviceProp[iExtIspCnt] ) );
	if ( error != CAM_ERROR_NONE )
	{
		free( pState );
		*phHandle = NULL;
		return error;
	}
#else
	iSocIspCnt = 0;
#endif

	if ( iExtIspCnt + iSocIspCnt <= 0 )
	{
		TRACE( CAM_ERROR, "Error: no sensor detected in Camera Engine( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		free( pState );
		*phHandle = NULL;
		return CAM_ERROR_NONE;
	}

	pState->iExtIspCnt = iExtIspCnt;
	pState->iTotalCnt  = iExtIspCnt + iSocIspCnt; 
	pState->iSensorID  = -1;

	*phHandle = (CAM_DeviceHandle)pState;

#if defined ( BUILD_OPTION_DEBUG_DUMP_CE_CALLING )
	// init trace dump
	{
		CAM_Int32s ret = 0;
		ret = _cam_inittrace();
		if ( ret != 0 )
		{
			TRACE( CAM_WARN, "Warning: calling sequence trace initial failed\n" );
		}
	}
#endif

	return error;
}

CAM_Error CAM_FreeHandle( CAM_DeviceHandle *phHandle )
{
	CAM_Error         error   = CAM_ERROR_NONE;
	_CAM_DeviceState  *pState = NULL;

	if ( phHandle == NULL || *phHandle == NULL ) 
	{
		TRACE( CAM_ERROR, "Error: Camera Engine handle shouldn't be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADPOINTER;
	}

	pState = (_CAM_DeviceState*)*phHandle;

	if ( pState->pEntry != NULL )
	{
		error = pState->pEntry->pClose( &pState->pDeviceData );
		ASSERT_CAM_ERROR( error );
	}

	pState->pEntry = NULL;
	free( pState );
	*phHandle = NULL;

#if defined ( BUILD_OPTION_DEBUG_DUMP_CE_CALLING )
	_cam_closetrace();
#endif

	return CAM_ERROR_NONE;
}

CAM_Error CAM_SendCommand( CAM_DeviceHandle hHandle, CAM_Command cmd, CAM_Param param1, CAM_Param param2 )
{
	CAM_Error         error    = CAM_ERROR_NONE;
	_CAM_DeviceState  *pState  = (_CAM_DeviceState*)hHandle;

#if defined ( BUILD_OPTION_DEBUG_DUMP_CE_CALLING )
	CAM_Tick tTick;
	_cam_devtrace( (void*)(pState->pDeviceData) );
#endif

	if ( pState == NULL )
	{
		TRACE( CAM_ERROR, "Error: Camera Engine handle shouldn't be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADPOINTER;
	}

#if defined ( BUILD_OPTION_DEBUG_DUMP_CE_CALLING )
	{
		CAM_Int32s iSensorID = pState->iSensorID;
		_cam_cmdtrace( iSensorID, cmd, (void*)param1, (void*)param2, &tTick );
	}
#endif

	switch ( cmd )
	{
	case CAM_CMD_ENUM_SENSORS:
		{
			CAM_Int32s     i;
			CAM_DeviceProp *pDetectedSensors = NULL;

			_CHECK_BAD_POINTER( param1 );
			_CHECK_BAD_POINTER( param2 );

			pDetectedSensors = (CAM_DeviceProp*)param2;
			*((CAM_Int32s*)param1) = pState->iTotalCnt;

			for ( i = 0; i < pState->iTotalCnt; i++ )
			{
				memcpy( &pDetectedSensors[i], &( pState->stDeviceProp[i] ), sizeof( CAM_DeviceProp ) );
			}

			error = CAM_ERROR_NONE;
		}
		break;

	case CAM_CMD_QUERY_CAMERA_CAP:
		{
			CAM_Int32s       iInternalSensorID = -1;
			CAM_Int32s       iSensorID         = (CAM_Int32s)param1;
			const _CAM_DriverEntry  *pEntry    = NULL;

			_CHECK_SUPPORT_RANGE( iSensorID, 0, pState->iTotalCnt - 1 );

#if defined( EXT )
			if ( iSensorID < pState->iExtIspCnt )
			{
				pEntry = &entry_extisp;
				iInternalSensorID = iSensorID;
			}
#endif

#if defined( DXO )
			if ( iSensorID >= pState->iExtIspCnt )
			{
				pEntry = &entry_socisp;
				iInternalSensorID = iSensorID - pState->iExtIspCnt;
			}
#endif

			error = pEntry->pCommand( pState->pDeviceData, cmd, (CAM_Param)iInternalSensorID, (CAM_Param)param2 );
		}
		break;

	case CAM_CMD_SET_SENSOR_ID:
		{
			CAM_Int32s       iInternalSensorID = -1;
			CAM_Int32s       iSensorID         = (CAM_Int32s)param1;
			const _CAM_DriverEntry  *pEntry    = NULL;

			_CHECK_SUPPORT_RANGE( iSensorID, 0, pState->iTotalCnt - 1 );

#if defined( EXT )
			if ( iSensorID < pState->iExtIspCnt )
			{
				pEntry = &entry_extisp;
				iInternalSensorID = iSensorID;
			}
#endif

#if defined( DXO )
			if ( iSensorID >= pState->iExtIspCnt )
			{
				pEntry = &entry_socisp;
				iInternalSensorID = iSensorID - pState->iExtIspCnt;
			}
#endif

			if ( pEntry != pState->pEntry )
			{
				if ( pState->pEntry != NULL )
				{
					error = pState->pEntry->pClose( &pState->pDeviceData );
					ASSERT_CAM_ERROR( error );
				}

				pState->pEntry = (_CAM_DriverEntry *)pEntry;

				error = pState->pEntry->pOpen( &pState->pDeviceData );
				ASSERT_CAM_ERROR( error );

				// error = pState->pEntry->pCommand( pState->pDeviceData, CAM_CMD_SET_EVENTHANDLER, pState->fnEventHandler, pState->pUserData );
				// ASSERT_CAM_ERROR( error );
			}

			error = pState->pEntry->pCommand( pState->pDeviceData, cmd, (CAM_Param)iInternalSensorID, (CAM_Param)param2 );
			ASSERT_CAM_ERROR( error );

			error = pState->pEntry->pCommand( pState->pDeviceData, CAM_CMD_SET_EVENTHANDLER, pState->fnEventHandler, pState->pUserData );
			ASSERT_CAM_ERROR( error );

			pState->iSensorID = iSensorID;

			error = CAM_ERROR_NONE;
		}
		break;

	case CAM_CMD_GET_SENSOR_ID:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pState->iSensorID;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_EVENTHANDLER:
		// NOTE: need to check the pointer validity. If param1 == NULL, it means that
		// user want to remove the previously registered event handler
		pState->fnEventHandler = (CAM_EventHandler)param1;
		pState->pUserData      = (void*)param2;

		if ( pState->pEntry != NULL )
		{
			error = pState->pEntry->pCommand( pState->pDeviceData, cmd, param1, param2 );
		}
		break;

	case CAM_CMD_GET_STATE:
		if ( pState->pEntry == NULL )
		{
			*(CAM_CaptureState*)param1 = CAM_CAPTURESTATE_NULL;
		}
		else
		{
			error = pState->pEntry->pCommand( pState->pDeviceData, cmd, param1, param2 );
		}
		break;

	default:
		if ( pState->pEntry == NULL )
		{
			TRACE( CAM_ERROR, "Error: pls set sensor ID firstly( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}
		else
		{
			error = pState->pEntry->pCommand( pState->pDeviceData, cmd, param1, param2 );
		}
		break;
	}

#if defined ( BUILD_OPTION_DEBUG_DUMP_CE_CALLING )
	_cam_rettrace( cmd, (void*)param1, (void*)param2, error, tTick );
#endif

	return error;
}
