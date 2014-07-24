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


CAM_Bool   bCanReturnToPreview;  // used to decide if we can change back to preview state
extern int iSensorID;


static void eventhandler( CAM_EventId eEventId, void *param, void *pUserData )
{
	switch ( eEventId )
	{
	case CAM_EVENT_FRAME_DROPPED:
		// TRACE( "Warning: port %d frame dropped, please enqueue buffer in time!( %s, %s, %d )\n", (CAM_Int32s)param, __FILE__, __FUNCTION__, __LINE__ );
		break;

	case CAM_EVENT_FRAME_READY:
		// Ingore this notification, since in this application we uses blocking call method to get the buffer
		break;

	case CAM_EVENT_IN_FOCUS:
		TRACE( "Event: In Focus!\n" );
		break;

	case CAM_EVENT_FOCUS_AUTO_STOP:
		if ( (CAM_Int32s)param == 1 )
		{
			TRACE( "Event: Focus Success!\n" );
		}
		else
		{
			TRACE( "Event: Focus Fail!\n" );
		}
		break;

	case CAM_EVENT_STILL_SHUTTERCLOSED:
		break;

	case CAM_EVENT_STILL_ALLPROCESSED:
		bCanReturnToPreview = CAM_TRUE;
		break;

	case CAM_EVENT_FACE_UPDATE:
		{
			CAM_MultiROI   *pFaceROI = (CAM_MultiROI*)param;
			int            i;

			TRACE( "Event: face detection update!\n" );
			TRACE( "face number: %d\n", pFaceROI->iROICnt );
			for ( i = 0; i < pFaceROI->iROICnt; i++ )
			{
				TRACE( "[%d]th facing cooidinate is: weight = %d, width = %d, height = %d, left = %d, top = %d\n",
				       i, pFaceROI->stWeiRect[i].iWeight, pFaceROI->stWeiRect[i].stRect.iWidth, pFaceROI->stWeiRect[i].stRect.iHeight, pFaceROI->stWeiRect[i].stRect.iLeft, pFaceROI->stWeiRect[i].stRect.iTop );
			}
		}
		break;

	default:
		TRACE( "Warning: not handled event[%d]!\n", eEventId );
		break;
	}
}


int _test_dv_dsc_actively_dequeue( CAM_DeviceHandle hHandle )
{
	CAM_CameraCapability stCameraCap;
	CAM_Error            error;

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)iSensorID, (CAM_Param)&stCameraCap );
	ASSERT_CAM_ERROR( error );

	error = GetParametersAndDisplay( hHandle, &stCameraCap );
	ASSERT_CAM_ERROR( error );

	error = StartPreview( hHandle );  // enter the preview state
	ASSERT_CAM_ERROR( error );
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
				error = AutoFocus( hHandle );
				if ( error != CAM_ERROR_NONE )
				{
					TRACE( "do nothing for prepare capture\n" );
				}
				break;

			case CMD_STARTFD:
				error = CAM_SendCommand( hHandle, CAM_CMD_START_FACEDETECTION, UNUSED_PARAM, UNUSED_PARAM );
				if ( error == CAM_ERROR_NOTSUPPORTEDARG )
				{
					TRACE( "do not support face detection\n" );
				}
				else
				{
					ASSERT_CAM_ERROR( error );
				}
				break;

			case CMD_CANCELFD:
				error = CAM_SendCommand( hHandle, CAM_CMD_CANCEL_FACEDETECTION, UNUSED_PARAM, UNUSED_PARAM );
				if ( error == CAM_ERROR_NOTSUPPORTEDARG )
				{
					TRACE( "do not support face detection\n" );
				}
				else
				{
					ASSERT_CAM_ERROR( error );
				}
				break;

			case CMD_STILLCAPTURE:
				bCanReturnToPreview = CAM_FALSE;
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
				{
					error = SetParameters( hHandle, cmd, param );
					ASSERT_CAM_ERROR( error );
				}
				break;
			}

			error = GetParametersAndDisplay( hHandle, &stCameraCap );
			ASSERT_CAM_ERROR( error );
		}

		error = RunOneFrame( hHandle );
		if ( error == CAM_ERROR_NOTENOUGHBUFFERS || error == CAM_ERROR_TIMEOUT ||
		     error == CAM_ERROR_PORTNOTACTIVE    || error == CAM_ERROR_BUFFERUNAVAILABLE
			)
		{
			;
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}

		if ( bCanReturnToPreview )
		{
			/* state change */
			error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
			ASSERT_CAM_ERROR( error );

			/* simulate Marvell Android camera-hal behavior */
			// error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_IDLE, UNUSED_PARAM );
			// ASSERT_CAM_ERROR( error );
			// error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
			// ASSERT_CAM_ERROR( error );
			bCanReturnToPreview = CAM_FALSE;
		}
	}

	error = StopPreview( hHandle );
	ASSERT_CAM_ERROR( error );

	return 0;
}

int test_dv_dsc_actively_dequeue( CAM_DeviceHandle hHandle )
{
	int val;

	/* change STDIN settings to allow non-blocking read */
	if ( ( val = fcntl( STDIN_FILENO, F_GETFL, 0 ) ) < 0 )
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
	val = _test_dv_dsc_actively_dequeue( hHandle );

	/* restore STDIN settings */
	if ( fcntl( STDIN_FILENO, F_SETFL, val ) < 0 )
	{
		TRACE( "set_fl failed to set FL\n" );
		return -1;
	}

	return val;
}
