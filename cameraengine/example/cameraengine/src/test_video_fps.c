/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#include "CameraEngine.h"
#include "test_harness.h"


#define TOTAL_FRAME_COUNT  100


CAM_Size stPreviewSize[] =
{
	// {320, 240},  // QVGA
	// {480, 320},  // HVGA
	// {640, 480},  // VGA
	// {720, 480},  // D1
	// {800, 480},  // WVGA
	// {1024, 768}, // XGA
	{1280, 720},
	{1920, 1080},
};

extern int iSensorID;
extern int iPreviewBufNum;

static void eventhandler( CAM_EventId eEventId, void *param, void *pUserData )
{
	CAM_Error        error    = CAM_ERROR_NONE;
	CAM_DeviceHandle hHandle  = (CAM_DeviceHandle)pUserData;
	CAM_ImageBuffer  *pImgBuf = NULL;
	CAM_CaptureState eState;

	switch ( eEventId )
	{
	case CAM_EVENT_FRAME_DROPPED:
		break;

	case CAM_EVENT_FRAME_READY:
		break;

	case CAM_EVENT_IN_FOCUS:
		TRACE( "Event: In Focus!\n" );
		break;

	case CAM_EVENT_FOCUS_AUTO_STOP:
		break;

	case CAM_EVENT_STILL_SHUTTERCLOSED:
		break;

	case CAM_EVENT_STILL_ALLPROCESSED:
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

int test_video_fps( CAM_DeviceHandle hHandle )
{
	CAM_Error          error  = CAM_ERROR_NONE;
	CAM_Error          error1 = CAM_ERROR_NONE;
	CAM_Size           *pSizeArray;
	int                i, size_index, j, port;
	int                iSizeCnt;
	int                iFrameCnt;
	CAM_PortCapability *pPortCap = NULL;
	CAM_ImageBuffer    *pImgBuf  = NULL;
	volatile CAM_Tick  t = 0, t1 = 0;
	float              fDigZoom   = 1.0f;   
	float              fFrameRate = -1.0f;
	CAM_Int32s         iDigZoomQ16, iFpsQ16;

	CAM_CameraCapability stCameraCap;

	double fps_p_p[10] = {0};
	double fps_v_p[10][10];
	double fps_v_v[10] = {0};

	TRACE( "Please input the buffer number you want to enqueue into preview port( >=3 && <=10 ):\n" );
	scanf( "%d", &iPreviewBufNum );
	if ( iPreviewBufNum < 3 )
	{
		TRACE( "error preview number\n" );
		return -1;
	}

	// register event handler to Camera Engine
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_EVENTHANDLER, (CAM_Param)eventhandler, (CAM_Param)hHandle );
	ASSERT_CAM_ERROR( error );

	error = CAM_SendCommand( hHandle, CAM_CMD_QUERY_CAMERA_CAP, (CAM_Param)iSensorID, (CAM_Param)&stCameraCap );
	ASSERT_CAM_ERROR( error );
	
	pPortCap = &stCameraCap.stPortCapability[CAM_PORT_VIDEO];

	if ( pPortCap->bArbitrarySize )
	{
		pSizeArray = stPreviewSize;
		iSizeCnt   = sizeof( stPreviewSize ) / sizeof( stPreviewSize[0] );
	}
	else
	{
		pSizeArray = pPortCap->stSupportedSize;
		iSizeCnt   = pPortCap->iSupportedSizeCnt;
	}

	TRACE( "Please input the frame rate you want to set:\n" );
	scanf( "%f", &fFrameRate );
	iFpsQ16 = (CAM_Int32s)( fFrameRate * 65536.0f ) ;
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_FRAMERATE, (CAM_Param)iFpsQ16, NULL );
	ASSERT_CAM_ERROR( error );

	TRACE( "Please input the digital zoom you want to set( >= 1 ):\n" );
	scanf( "%f", &fDigZoom );
	iDigZoomQ16 = (CAM_Int32s)( fDigZoom * 65536 + 0.5f );
	error = CAM_SendCommand( hHandle, CAM_CMD_SET_DIGZOOM, (CAM_Param)iDigZoomQ16, NULL );
	ASSERT_CAM_ERROR( error );

	// Preview state preview port fps
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		if ( pPortCap->stSupportedSize[size_index].iWidth == 2048 )
		{
			fps_p_p[size_index] = -1.0;
			continue;
		}
		// try preview size
		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pSizeArray[size_index] );
		if ( error == CAM_ERROR_NOTSUPPORTEDARG )
		{
			fps_p_p[size_index] = -1.0;
			TRACE( "Target preview size (%d x %d) is not supported!\n", pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight );
			continue;
		}
		ASSERT_CAM_ERROR( error );

		StartPreview( hHandle );  // enter the preview state

		for ( i = 0; i < 10; i++ )
		{
			port = CAM_PORT_PREVIEW;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
				ASSERT_CAM_ERROR( error1 );
			}
		}

		t = -CAM_TimeGetTickCount();

		iFrameCnt = 0;
		while ( iFrameCnt < TOTAL_FRAME_COUNT )
		{
#if 1
			port = CAM_PORT_PREVIEW;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
				ASSERT_CAM_ERROR( error1 );

				iFrameCnt++;
			}
#else
			// include display in fps test
			error = RunOneFrame( hHandle );
			ASSERT_CAM_ERROR(error);
#endif

		}

		t += CAM_TimeGetTickCount();

		fps_p_p[size_index] = iFrameCnt * 1000000.0 / t;

		TRACE( "Resolution: %d * %d, fps: %f \n", pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight, fps_p_p[size_index] );

		StopPreview( hHandle );
	}

	TRACE( "Preview state preview port fps( buffer number: %d ):\n", iPreviewBufNum );
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		TRACE( "\n\tPreview %d x %d:\t%.2f\n",pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight, fps_p_p[size_index] );
	}


	// Video state preview port fps
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{

		for ( j = 0; j < 1; j++ )
		{

			if ( pSizeArray[size_index].iWidth == 2048 || pSizeArray[j].iWidth == 2048 )
			{
				fps_v_p[size_index][j] = -1.0;
				continue;
			}

			// try preview size
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pSizeArray[j] );
			if ( error == CAM_ERROR_NOTSUPPORTEDARG )
			{
				fps_v_p[size_index][j] = -1.0;
				continue;
			}
			ASSERT_CAM_ERROR( error );

			// try video size
			error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&pSizeArray[size_index] );
			if ( error == CAM_ERROR_NOTSUPPORTEDARG )
			{
				fps_v_p[size_index][j] = -1.0;
				TRACE( "Target video size (%d x %d) is not supported!\n", pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight );
				continue;
			}
			ASSERT_CAM_ERROR( error );

			StartPreview( hHandle );  // enter the preview state

			StartRecording( hHandle );

			for ( i = 0; i < 10; i++ )
			{
				port = CAM_PORT_PREVIEW;
				error = SafeDequeue( hHandle, &port, &pImgBuf );
				if ( error == CAM_ERROR_NONE )
				{
					error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
					ASSERT_CAM_ERROR( error1 );
				}

				port = CAM_PORT_VIDEO;
				error = SafeDequeue( hHandle, &port, &pImgBuf );
				if ( error == CAM_ERROR_NONE )
				{
					error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
					ASSERT_CAM_ERROR( error1 );
				}
			}

			t = -CAM_TimeGetTickCount();

			iFrameCnt = 0;
			while ( iFrameCnt < TOTAL_FRAME_COUNT )
			{
				port = CAM_PORT_PREVIEW;
				error = SafeDequeue( hHandle, &port, &pImgBuf );
				if ( error == CAM_ERROR_NONE )
				{
					error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
					ASSERT_CAM_ERROR( error1 );
					iFrameCnt++;
				}

				port = CAM_PORT_VIDEO;
				error = SafeDequeue( hHandle, &port, &pImgBuf );
				if ( error == CAM_ERROR_NONE )
				{
					error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
					ASSERT_CAM_ERROR( error1 );
				}
			}

			t += CAM_TimeGetTickCount();

			fps_v_p[size_index][j] = iFrameCnt * 1000000.0 / t;

			TRACE( "Target video size (%d x %d) fps: %.2f!\n", pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight, fps_v_p[size_index][j] );

			StopRecording( hHandle );

			StopPreview( hHandle );
		}
	}

	// Video state video port fps
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		if ( pSizeArray[size_index].iWidth == 2048 )
		{
			fps_v_v[size_index] = -1.0;
			continue;
		}

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_PREVIEW, (CAM_Param)&pSizeArray[0] );

		// start preview
		StartPreview( hHandle );  // enter the preview state

		error = CAM_SendCommand( hHandle, CAM_CMD_PORT_SET_SIZE, (CAM_Param)CAM_PORT_VIDEO, (CAM_Param)&pSizeArray[size_index] );
		ASSERT_CAM_ERROR( error );

		StartRecording( hHandle );

		// error = CAM_SendCommand( hHandle, CAM_CMD_PORT_FLUSH_BUF, (CAM_Param)CAM_PORT_PREVIEW, UNUSED_PARAM );
		// ASSERT_CAM_ERROR( error );

		for ( i = 0; i < 10; i++ )
		{
			port = CAM_PORT_PREVIEW;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
				ASSERT_CAM_ERROR( error1 );
			}

			port = CAM_PORT_VIDEO;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
				ASSERT_CAM_ERROR( error1 );
			}
		}

		t = -CAM_TimeGetTickCount();

		iFrameCnt = 0;
		while ( iFrameCnt < TOTAL_FRAME_COUNT )
		{
			port = CAM_PORT_PREVIEW;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = CAM_SendCommand( hHandle, CAM_CMD_PORT_ENQUEUE_BUF, (CAM_Param)port, (CAM_Param)pImgBuf );
				ASSERT_CAM_ERROR( error1 );
			}

			port = CAM_PORT_VIDEO;
			error = SafeDequeue( hHandle, &port, &pImgBuf );
			if ( error == CAM_ERROR_NONE )
			{
				error1 = DeliverVideoBuffer( hHandle, pImgBuf );

				iFrameCnt++;
			}

			// t3 = CAM_TimeGetTickCount();
			// TRACE( "video dequeue time:%.2f ms, enqueue time:%.2f ms\n", (t2 - t1) / 1000.0, (t3 - t2) / 1000.0 );
		}

		t += CAM_TimeGetTickCount();

		fps_v_v[size_index] = iFrameCnt * 1000000.0 / t;

		StopRecording( hHandle );

		StopPreview( hHandle );
	}

	TRACE( "Preview state preview port fps( buffer number: %d ):\n", iPreviewBufNum );
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		TRACE( "\n\tPreview %d x %d:\t%.2f\n", pSizeArray[size_index].iWidth, pSizeArray[size_index].iHeight, fps_p_p[size_index] );
	}

	TRACE( "Video state preview port fps:\n" );
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		// j = size_index;
		for ( j = 0; j < iSizeCnt; j++ )
		{
			if ( fps_v_p[size_index][j] != -1 )
			{
				TRACE( "\n\tVideo (%d x %d), Preview (%d x %d): \t%.2f\n",
				       pSizeArray[size_index].iWidth,
				       pSizeArray[size_index].iHeight,
				       pSizeArray[j].iWidth,
				       pSizeArray[j].iHeight,
				       fps_v_p[size_index][j] );
			}
		}
	}

	TRACE( "Video state video port fps:\n" );
	for ( size_index = 0; size_index < iSizeCnt; size_index++ )
	{
		TRACE( "\n\tVideo: %d x %d\t%.2f\n",
		         pSizeArray[size_index].iWidth,
		         pSizeArray[size_index].iHeight,
		         fps_v_v[size_index] );
	}

	return 0;
}
