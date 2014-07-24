/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CameraEngine.h"
#include "test_harness.h"

CAM_Tick   tAFBegin = 0, tAFClose = 0;
CAM_Bool   bDumpFrame = CAM_FALSE;
extern CAM_Int32s iSensorID;

static void eventhandler( CAM_EventId eEventId, void *param, void *pUserData )
{
	CAM_Error        error    = CAM_ERROR_NONE;
	CAM_DeviceHandle hHandle  = (CAM_DeviceHandle)pUserData;
	CAM_ImageBuffer  *pImgBuf = NULL;
	CAM_CaptureState eState;

	switch ( eEventId )
	{
	case CAM_EVENT_FRAME_DROPPED:
		TRACE( "Warning: port[%d] frame dropped, please enqueue buffer in time!\n", (CAM_Int32s)param );
		break;

	case CAM_EVENT_FRAME_READY:
		{
			// NOTE: if heavy workload (DeliverXXXXBuffer functions) is executed in the context of the function,
			//       it may block other events such as "CAM_EVENT_FRAME_DROPPED", hence the actual number of frame
			//       droppes may be large than the reported numbers.
			CAM_Int32s iPortId = (CAM_Int32s)param;
			error = SafeDequeue( hHandle, &iPortId, &pImgBuf );
			ASSERT_CAM_ERROR( error );
			
			switch ( iPortId )
			{
			case CAM_PORT_PREVIEW:
				error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
				ASSERT_CAM_ERROR( error );

				if ( eState == CAM_CAPTURESTATE_STILL )
				{
					error = DeliverSnapshotBuffer( hHandle, pImgBuf );
					ASSERT_CAM_ERROR( error );
					CAM_Sleep( 1000000 ); // stall for one second
				}
				else
				{
					error = DeliverPreviewBuffer( hHandle, pImgBuf );
					ASSERT_CAM_ERROR( error );
					if ( bDumpFrame )
					{
						error = TakePicture( hHandle );
						ASSERT_CAM_ERROR( error );
						bDumpFrame = CAM_FALSE;
					}
				}
			break;

			case CAM_PORT_VIDEO:
				error = DeliverVideoBuffer( hHandle, pImgBuf );
				ASSERT_CAM_ERROR(error);
				break;

			case CAM_PORT_STILL:
				error = DeliverStillBuffer( hHandle, pImgBuf );
				ASSERT_CAM_ERROR( error );
				break;
			}
		}
		break;

	case CAM_EVENT_IN_FOCUS:
		TRACE( "Event: In Focus!\n" );
		break;

	case CAM_EVENT_FOCUS_AUTO_STOP:
		if ( (CAM_Int32s)param == 1 )
		{
			TRACE( "Event: Focus Success!\n" );
			tAFClose = CAM_TimeGetTickCount();
			TRACE( "AF convergence time cost: %.2f ms\n", ( tAFClose - tAFBegin ) / 1000.0 );
		}
		else
		{
			tAFClose = tAFBegin - 1;
			TRACE( "Event: Focus Fail!\n" );
		}
		bDumpFrame = CAM_TRUE;
		break;

	case CAM_EVENT_STILL_SHUTTERCLOSED:
		break;

	case CAM_EVENT_STILL_ALLPROCESSED:
		/* state change */
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );
		break;

	case CAM_EVENT_FATALERROR:
		error = RestartCameraEngine( hHandle );
		ASSERT_CAM_ERROR( error );
		break;

	default:
		TRACE( "Warning: not handled event!\n" );
		break;
	}
}

int _test_dv_dsc_on_event_dequeue( CAM_DeviceHandle hHandle )
{
	CAM_CameraCapability stCameraCap;
	CAM_Error            error = CAM_ERROR_NONE;

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)iSensorID, (CAM_Param)&stCameraCap );
	ASSERT_CAM_ERROR( error );

	error = GetParametersAndDisplay( hHandle, &stCameraCap );
	ASSERT_CAM_ERROR( error );

	StartPreview( hHandle );  // enter the preview state
	TRACE( "Start Preview\n" );
	for ( ; ; )
	{
		CAM_AppCmd       cmd;
		CameraParam      param;
		CAM_CaptureState eState;

		error = DequeueUserCmd( hHandle, &cmd, &param );
		ASSERT_CAM_ERROR( error );

		if ( cmd == CMD_EXIT )
		{
			break;
		}
		else if ( cmd != CMD_NULL )
		{
			switch ( cmd )
			{
			case CMD_SELECTSENSOR:
				StopPreview( hHandle );
				ASSERT_CAM_ERROR( error );
				
				SetSensor( hHandle, param.int32 );
				ASSERT_CAM_ERROR( error );

				error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)param.int32, (CAM_Param)&stCameraCap );
				ASSERT_CAM_ERROR( error );

				error = StartPreview( hHandle );
				ASSERT_CAM_ERROR( error );
				break;

			case CMD_PREPARECAPTURE:
				tAFBegin = CAM_TimeGetTickCount();
				error = AutoFocus( hHandle );
				if ( error != CAM_ERROR_NONE )
				{
					TRACE( "do nothing for prepare capture\n" );
				}
				break;

			case CMD_STILLCAPTURE:
				error = TakePicture( hHandle );
				ASSERT_CAM_ERROR( error );
				break;

			case CMD_CANCELCAPTURE:
				error = CancelAutoFocus( hHandle );
				ASSERT_CAM_ERROR( error );
				break;

			case CMD_VIDEOONOFF:
				error = CAM_SendCommand( hHandle, CAM_CMD_GET_STATE, (CAM_Param)&eState, UNUSED_PARAM );
				ASSERT_CAM_ERROR( error );
				if ( eState == CAM_CAPTURESTATE_PREVIEW )
				{
					error = StartRecording( hHandle );
					ASSERT_CAM_ERROR( error );
				}
				else if ( eState == CAM_CAPTURESTATE_VIDEO )
				{
					error = StopRecording( hHandle );
					ASSERT_CAM_ERROR( error );
				}
				break;

			default:
				error = SetParameters( hHandle, cmd, param );
				ASSERT_CAM_ERROR( error );
				break;
			}

			error = GetParametersAndDisplay( hHandle, &stCameraCap );
			ASSERT_CAM_ERROR( error );
		}
	}

	error = StopPreview( hHandle );
	ASSERT_CAM_ERROR( error );

	return 0;
}

int test_dv_dsc_on_event_dequeue( CAM_DeviceHandle hHandle )
{
	int val;

	/* change STDIN settings to allow non-blocking read */
	if ( ( val=fcntl( STDIN_FILENO, F_GETFL, 0 ) ) < 0 )
	{
		TRACE( "get_fl failed to get FL\n" );
		return -1;
	}
	if ( fcntl( STDIN_FILENO, F_SETFL, val | O_NONBLOCK ) < 0 )
	{
		TRACE( "set_fl failed to set FL\n" );
		return -1;
	}

	/* start the camera process */
	val = _test_dv_dsc_on_event_dequeue( hHandle );

	/* restore STDIN settings */
	if ( fcntl( STDIN_FILENO, F_SETFL, val ) < 0 )
	{
		TRACE( "set_fl failed to set FL\n" );
		return -1;
	}

	return val;
}
