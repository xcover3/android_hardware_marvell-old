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

static void eventhandler( CAM_EventId eEventId, void *param, void *pUserData )
{
	switch ( eEventId )
	{
	case CAM_EVENT_FRAME_DROPPED:
		TRACE( "Warning: port %d frame dropped, please enqueue buffer in time!\n", (CAM_Int32s)param );
		break;

	case CAM_EVENT_FRAME_READY:
		// Ingore this notification, since in this application we uses blocking call method to get the buffer
		break;

	case CAM_EVENT_IN_FOCUS:
		TRACE( "Event: Focus Complete!\n" );
		break;

	case CAM_EVENT_STILL_SHUTTERCLOSED:
		break;

	case CAM_EVENT_STILL_ALLPROCESSED:
		break;

	default:
		TRACE( "Warning: not handled event!\n" );
		break;
	}
}

int test_dequeue_singleport( CAM_DeviceHandle hHandle )
{
	CAM_Error       error;
	int             i, port;
	CAM_ImageBuffer *pImgBuf;

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	StartPreview( hHandle );  // enter the preview state
	// StartRecording( hHandle );

	for ( i = 0; i < 20; i++ )
	{
		TRACE( "de-queue preview %d\n", i );
		port = CAM_PORT_PREVIEW;
		error = SafeDequeue( hHandle, &port, &pImgBuf );
		if ( error == CAM_ERROR_NONE )
		{
			TRACE( "en-queue preview %d\n", i );
			CAM_Error error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
			ASSERT_CAM_ERROR( error1 );
		}
		else if ( error == CAM_ERROR_NOTENOUGHBUFFERS || error == CAM_ERROR_TIMEOUT ||
		          error == CAM_ERROR_PORTNOTACTIVE    || error == CAM_ERROR_BUFFERUNAVAILABLE
		        )
		{
			;
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
	}

	StartRecording( hHandle );
	for ( i = 0; i < 20; i++ )
	{
		TRACE( "de-queue video %d\n", i );
		port = CAM_PORT_VIDEO;
		error = SafeDequeue( hHandle, &port, &pImgBuf );
		if ( error == CAM_ERROR_NONE )
		{
			TRACE( "en-queue video %d\n", i );
			CAM_Error error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
			ASSERT_CAM_ERROR( error1 );
		}
		else if ( error == CAM_ERROR_NOTENOUGHBUFFERS || error == CAM_ERROR_TIMEOUT ||
		          error == CAM_ERROR_PORTNOTACTIVE    || error == CAM_ERROR_BUFFERUNAVAILABLE
		        )
		{
			;
		}
		else
		{
			ASSERT_CAM_ERROR( error );
		}
	}

	StopRecording( hHandle );
	StopPreview( hHandle );

	TRACE( "============Report=================\n" );
	TRACE( "test:%s succeed\n", __FUNCTION__ );
	TRACE( "============EOR====================\n" );

	return 0;
}


int test_flush( CAM_DeviceHandle hHandle )
{
	CAM_Error error;
	int       i, j, port;

	// register event handler to CameraEngine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	StartPreview( hHandle );  // enter the preview state
	StartRecording( hHandle );

	for ( j = 0; j < 30; j++ )
	{
		port = CAM_PORT_PREVIEW;

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)port, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < stPreviewBufReq.iMinBufCount; i++ )
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)&pPreviewBuf[i] );
			ASSERT_CAM_ERROR( error );
		}

		port = CAM_PORT_VIDEO;

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)port, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < stVideoBufReq.iMinBufCount; i++ )
		{

			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)&pVideoBuf[i] );
			ASSERT_CAM_ERROR( error );
		}
	}

	StopRecording( hHandle );

	TakePicture( hHandle );

	for ( j = 0; j < 30; j++ )
	{
		port = CAM_PORT_PREVIEW;

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)port, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < stPreviewBufReq.iMinBufCount; i++ )
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)&pPreviewBuf[i] );
			ASSERT_CAM_ERROR( error );
		}

		port = CAM_PORT_STILL;

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)port, UNUSED_PARAM );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < stStillBufReq.iMinBufCount; i++ )
		{
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)&pStillBuf[i] );
			ASSERT_CAM_ERROR( error );
		}
	}

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	StopPreview( hHandle );

	TRACE( "============Report=================\n" );
	TRACE( "test:%s succeed\n", __FUNCTION__ );
	TRACE( "============EOR====================\n" );

	return 0;
}


int test_state_transition( CAM_DeviceHandle hHandle )
{
	CAM_Error error;

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	// idle -> preview
	StartPreview( hHandle );  // enter the preview state
	// preview->idle
	StopPreview( hHandle );

	// idle->preview
	StartPreview( hHandle );  // enter the preview state
	// preview->video
	StartRecording( hHandle );
	// video ->preview
	StopRecording( hHandle );

	// preview->video
	StartRecording( hHandle );
	// video ->preview
	StopRecording( hHandle );

	// preview->video
	StartRecording( hHandle );
	// video ->preview
	StopRecording( hHandle );

	StopPreview( hHandle );

	TRACE( "============Report=================\n" );
	TRACE( "test:%s succeed\n", __FUNCTION__ );
	TRACE( "============EOR====================\n" );

	return 0;
}
