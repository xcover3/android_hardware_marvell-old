/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#include "CameraEngine.h"
#include "test_harness.h"


#define BURST_COUNT    2

extern CAM_Int32s iSensorID;
extern CAM_Int32s iExpectedPicNum;

CAM_Bool   bAFLocked = CAM_FALSE;
CAM_Bool   bCareThis = CAM_TRUE;
CAM_Bool   bTestShutterLag;
CAM_Tick   t0 = 0, t1 = 0, t_start_af = 0, t_lock_n_press = 0, t_shutter_close[BURST_COUNT] = {0}, t_snapshot[BURST_COUNT] = {0}, t_jpeg[BURST_COUNT] = {0}, t_save[BURST_COUNT] = {0};
CAM_Tick   t_shot1 = 0, t_shot2 = 0;
CAM_Int32s burst = 0;
void       *hEventShutter;

static void eventhandler( CAM_EventId eEventId, void *param, void *pUserData )
{
	int ret = 0;

	switch ( eEventId )
	{
	case CAM_EVENT_FRAME_DROPPED:
		break;

	case CAM_EVENT_FRAME_READY:
		if ( bTestShutterLag )
		{
			if ( (CAM_Int32s)param == CAM_PORT_PREVIEW )
			{
				t_snapshot[burst] = CAM_TimeGetTickCount();
			}
			else if ( (CAM_Int32s)param == CAM_PORT_STILL )
			{
				t_jpeg[burst] = CAM_TimeGetTickCount();
				burst++;
			}
		}
		break;

	case CAM_EVENT_FOCUS_AUTO_STOP:
		bAFLocked = CAM_TRUE;
		if ( bCareThis )
		{
			t_lock_n_press = CAM_TimeGetTickCount();
			bCareThis = CAM_FALSE;
		}
		TRACE( "AF %s!\n", ( (CAM_Int32s)param ? "success" : "fail" ) );
		break;

	case CAM_EVENT_STILL_SHUTTERCLOSED:
		if ( bTestShutterLag )
		{
			t_shutter_close[burst] = CAM_TimeGetTickCount();
		}
		if ( burst == 0 )
		{
			ret = CAM_EventSet( hEventShutter );
			if ( ret != 0 )
			{
				TRACE( "Event Set failed!\n" );
				return;
			}
		}
		break;

	case CAM_EVENT_STILL_ALLPROCESSED:
		break;

	default:
		break;
	}

	return;
}

int test_camera_speed( CAM_DeviceHandle pData )
{
	CAM_Error            error;
	CAM_Int32s           port;
	CAM_Int32s           i;
	CAM_Int32s           burst_cnt = BURST_COUNT;
	CAM_PortCapability   *pPortCap = NULL;
	CAM_ImageBuffer      *pImgBuf  = NULL;
	CAM_DeviceHandle     hHandle;
	CAM_Size             stPreviewSize;
	CAM_Bool             bHaveSnap     = CAM_TRUE;
	CAM_Bool             bSupportAF    = CAM_FALSE;
	CAM_Bool             bSupportBurst = CAM_FALSE;
	CAM_CameraCapability stCameraCap;

	CAM_Tick camera_off_to_vf_on, preview_to_video, video_to_preview;
	CAM_Tick af_latency, shutter_total_lag, shutter_release_lag, shot_to_snapshot, shot_to_jpeg, shot_to_shot, shot_to_save, continuous_shot_interval;
	CAM_Tick con_second_shot_lag, con_second_snap_lag, con_second_encoder_lag;


	/* camera off to view-finder on latency */
	t0 = CAM_TimeGetTickCount();

	// enum sensors and get camera capability
#if 0
	error = InitEngine( &hHandle );
	ASSERT_CAM_ERROR( error );
#else
	hHandle = pData;
#endif
	// set current sensor ID
	error = SetSensor( hHandle, iSensorID );
	ASSERT_CAM_ERROR( error );

	// query camera capability
	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)iSensorID, (CAM_Param)&stCameraCap );
	ASSERT_CAM_ERROR( error );

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	// start preview and diaplay one frame
	StartPreview( hHandle );
	RunOneFrame( hHandle );

	t1 = CAM_TimeGetTickCount();
	camera_off_to_vf_on = t1 - t0;


	/* preview to video latency */
	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_GET_SIZE, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&stPreviewSize );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_VIDEO,(CAM_Param)&stPreviewSize );
	ASSERT_CAM_ERROR( error );

	t0 = CAM_TimeGetTickCount();
	StartRecording( hHandle );
	RunOneFrame( hHandle );
	t1 = CAM_TimeGetTickCount();

	preview_to_video = t1 - t0;


	/* video to preview */
	t0 = CAM_TimeGetTickCount();

	StopRecording( hHandle );
	RunOneFrame( hHandle );
	
	t1 = CAM_TimeGetTickCount();

	video_to_preview = t1 - t0;
	
	/* burst capture shutter lag */
	CAM_EventCreate( &hEventShutter );
	CAM_EventReset( hEventShutter );
	bAFLocked = CAM_FALSE;

	// AF Latency
	// is Camera Engine support AF
	for ( i = 0; i < stCameraCap.stSupportedShotParams.iSupportedFocusModeCnt; i++ )
	{
		if ( stCameraCap.stSupportedShotParams.eSupportedFocusMode[i] == CAM_FOCUS_AUTO_ONESHOT )
		{
			bSupportAF = CAM_TRUE;
		}
	}

	t_start_af = CAM_TimeGetTickCount();
	if ( !bSupportAF )
	{
		t_lock_n_press = CAM_TimeGetTickCount();
	}
	else
	{
		error = CAM_SendCommand( hHandle, CAM_CMD_SET_FOCUSMODE, CAM_FOCUS_AUTO_ONESHOT, (CAM_Param)NULL );
		ASSERT_CAM_ERROR( error );

		AutoFocus( hHandle );
		while ( !bAFLocked )
		{
			RunOneFrame( hHandle );
		}
	}
	af_latency = t_lock_n_press - t_start_af;

	for ( i = 0; i < stCameraCap.stSupportedShotParams.iSupportedStillSubModeCnt; i++ )
	{
		if ( stCameraCap.stSupportedShotParams.eSupportedStillSubMode[i] == CAM_STILLSUBMODE_BURST )
		{
			bSupportBurst = CAM_TRUE;
			break;
		}
	}

	if ( bSupportBurst )
	{
		CAM_StillSubModeParam stStillParam = { BURST_COUNT, 0 };
		burst_cnt = BURST_COUNT;

		error = CAM_SendCommand( hHandle, CAM_CMD_SET_STILL_SUBMODE, (CAM_Param)CAM_STILLSUBMODE_BURST, &stStillParam );
		ASSERT_CAM_ERROR( error );
	}
	else
	{
		burst_cnt = 1;
	}

	// shutter release Lag
	bTestShutterLag = CAM_TRUE;

	burst = 0;
	t_lock_n_press = CAM_TimeGetTickCount();
	TakePicture( hHandle );

	CAM_EventWait( hEventShutter, CAM_INFINITE_WAIT, NULL );
	
	for ( i = 0; i < burst_cnt; i++ )
	{
		port = CAM_PORT_PREVIEW;
		error = SafeDequeue( hHandle, &port, &pImgBuf );
		if ( error != CAM_ERROR_NONE )
		{
			bHaveSnap = CAM_FALSE;
			TRACE( "!!! no snapshot\n" );
		}
		else
		{
			DeliverSnapshotBuffer( hHandle, pImgBuf );
		}

		port = CAM_PORT_STILL;
		error = SafeDequeue( hHandle, &port, &pImgBuf );
		ASSERT_CAM_ERROR( error );
		DeliverStillBuffer( hHandle, pImgBuf );
		t_save[i] = CAM_TimeGetTickCount();
	}

	if ( bHaveSnap )
	{
		// from "snap" to snapshot
		shot_to_snapshot = t_snapshot[0] - t_lock_n_press;

		// second shutter lag
		con_second_snap_lag = t_snapshot[1] - t_shutter_close[1];
		// second JPEG encoder lag
		con_second_encoder_lag = t_jpeg[1] - t_snapshot[1];
	}
	else
	{
		// from "snap" to snapshot
		shot_to_snapshot = -1000;

		// second shutter lag
		con_second_snap_lag = -1000;
		// second JPEG encoder lag
		con_second_encoder_lag = -1000;
	}
	// from "snap" to shutter close
	shutter_release_lag = t_shutter_close[0] - t_lock_n_press;
	// from start AF to shutter close
	shutter_total_lag = shutter_release_lag + af_latency;
	// from "snap" to JPEG
	shot_to_jpeg = t_jpeg[0] - t_lock_n_press;
	// from "snap" to JPEG be saved
	shot_to_save = t_save[0] - t_lock_n_press;

	if ( burst_cnt > 1 )
	{
		// from first JPEG to second shutter close
		con_second_shot_lag = t_shutter_close[1] - t_jpeg[0];
		// second burst shutter lag
		continuous_shot_interval = t_jpeg[1] - t_jpeg[0];
	}
	else
	{
		// from first JPEG to second shutter close
		con_second_shot_lag = -1000;
		// second burst shutter lag
		continuous_shot_interval = -1000;
	}

	/* shot to shot lag */
	bTestShutterLag = CAM_FALSE;
	burst = 0;
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STILL_SUBMODE, CAM_STILLSUBMODE_SIMPLE, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );

	TakePicture( hHandle );
	CAM_EventWait( hEventShutter, CAM_INFINITE_WAIT, NULL );
	t_shot1 = CAM_TimeGetTickCount();

	port = CAM_PORT_STILL;
	error = SafeDequeue( hHandle, &port, &pImgBuf );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_STILL, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	iExpectedPicNum = 0;

	error = FreeImages( &pStillBuf, stStillBufReq.iMinBufCount );
	ASSERT_CAM_ERROR( error );

	burst = 0;
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	TakePicture( hHandle );
	CAM_EventWait( hEventShutter, CAM_INFINITE_WAIT, NULL );
	t_shot2 = CAM_TimeGetTickCount();
	shot_to_shot = t_shot2 - t_shot1;

	error = CAM_SendCommand( hHandle, CAM_CMD_SET_STATE, (CAM_Param)CAM_CAPTURESTATE_PREVIEW, UNUSED_PARAM );
	ASSERT_CAM_ERROR( error );
	StopPreview( hHandle );
	CAM_EventDestroy( &hEventShutter );

#if 0
	DeinitEngine( hHandle );
#endif

	TRACE("=================================camera speed report==============================\n");
	TRACE( "Camera-off to View-finder On:    %.2f ms\n", (double)camera_off_to_vf_on / 1000.0 );
	TRACE( "Preview To Video:                %.2f ms\n", (double)preview_to_video / 1000.0 );
	TRACE( "Video To Preview:                %.2f ms\n", (double)video_to_preview / 1000.0 );
	TRACE( "AF Latency:                      %.2f ms\n", (double)af_latency / 1000.0 );
	TRACE( "Shutter Total Lag:               %.2f ms\n", (double)shutter_total_lag / 1000.0 );
	TRACE( "Shutter Release Lag:             %.2f ms\n", (double)shutter_release_lag / 1000.0 );
	TRACE( "Shot To Snapshot:                %.2f ms\n", (double)shot_to_snapshot / 1000.0 );
	TRACE( "Shot To JPEG:                    %.2f ms\n", (double)shot_to_jpeg / 1000.0 );
	TRACE( "Shot To Save:                    %.2f ms\n", (double)shot_to_save / 1000.0 );
	TRACE( "Shot To Shot:                    %.2f ms\n", (double)shot_to_shot / 1000.0 );

	TRACE( "Continuous Shot second lag:      %.2f ms\n", (double)con_second_shot_lag / 1000.0 );
	TRACE( "Continuous Shot second snap lag: %.2f ms\n", (double)con_second_snap_lag / 1000.0 );
	TRACE( "Continuous Shot second jpeg lag: %.2f ms\n", (double)con_second_encoder_lag / 1000.0 );
	TRACE( "Continuous Shot interval:        %.2f ms\n", (double)continuous_shot_interval / 1000.0 );
	TRACE("=================================EOR===========================================\n");


	return 0;
}
