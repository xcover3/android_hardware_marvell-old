/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include "CameraOSAL.h"

#include "cam_utility.h"
#include "cam_log.h"
#include "cam_gen.h"
#include "cam_ppu.h"

#include "cam_extisp_eng.h"

/* Function declarations */
// entry functions
static CAM_Error _open( void **ppDeviceData );
static CAM_Error _close( void **ppDeviceData );
static CAM_Error _command( void *pDeviceData, CAM_Command cmd, CAM_Param param1, CAM_Param param2 );

// cmd implementaion
static CAM_Error _set_sensor_id( _CAM_ExtIspState *pCameraState, CAM_Int32s iSensorID );
static CAM_Error _get_camera_capability( CAM_Int32s iSensorID, CAM_CameraCapability *pCameraCap );
static CAM_Error _get_port_bufreq( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBufferReq *pBufReq );
static CAM_Error _enqueue_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBuffer *pImgBuf );
static CAM_Error _dequeue_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _flush_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _set_camera_state( _CAM_ExtIspState *pCameraState, CAM_CaptureState eToState );
static CAM_Error _set_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Int32s iDigitalZoomQ16 );
static CAM_Error _set_still_submode( _CAM_ExtIspState *pCameraState, CAM_StillSubMode eStillSubMode, CAM_StillSubModeParam *pStillSubModeParam );
static CAM_Error _set_port_size( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_Size *pSize );
static CAM_Error _set_port_format( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageFormat eFormat );
static CAM_Error _set_port_rotation( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_FlipRotate eRotation );
static CAM_Error _set_jpeg_param( _CAM_ExtIspState *pCameraState, CAM_JpegParam *pJpegParam ); 
static CAM_Error _get_shotmode_capability( CAM_Int32s iSensorId, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap );
static CAM_Error _reset_camera( _CAM_ExtIspState *pCameraState, CAM_Int32s iResetType );
static CAM_Error _start_face_detection( _CAM_ExtIspState *pCameraState );
static CAM_Error _cancel_face_detection( _CAM_ExtIspState *pCameraState );
static CAM_Error _reset_zsl_preview( _CAM_ExtIspState *pCameraState, CAM_Size *pSize );

// state transtion function
static CAM_Error _standby2idle( _CAM_ExtIspState *pCameraState );
static CAM_Error _idle2null( _CAM_ExtIspState *pCameraState );
static CAM_Error _idle2standby( _CAM_ExtIspState *pCameraState );
static CAM_Error _idle2preview( _CAM_ExtIspState *pCameraState );
static CAM_Error _preview2idle( _CAM_ExtIspState *pCameraState );
static CAM_Error _preview2video( _CAM_ExtIspState *pCameraState );
static CAM_Error _preview2still( _CAM_ExtIspState *pCameraState );
static CAM_Error _video2preview( _CAM_ExtIspState *pCameraState );
static CAM_Error _still2preview( _CAM_ExtIspState *pCameraState );

// internal utility function
static CAM_Error _is_need_ppu( _CAM_PortState *pPortState, _CAM_SmartSensorConfig *pSensorConfig, _CAM_PostProcessParam *pPpuShotParam,
                               CAM_Bool *pbNeedPostProcessing, CAM_Bool *pbNeedPrivateBuffer );
static CAM_Error _request_ppu_srcimg( _CAM_ExtIspState *pCameraState, CAM_ImageBufferReq *pBufReq, CAM_ImageBuffer *pUsrImgBuf, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _release_ppu_srcimg( _CAM_ExtIspState *pCameraState, CAM_ImageBuffer *pImgBuf );
static CAM_Error _flush_all_ppu_srcimg( _CAM_ExtIspState *pCameraState );

static CAM_Error _dequeue_filled( _CAM_ExtIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _run_one_frame( _CAM_ExtIspState *pCameraState );
static CAM_Error _process_previewstate_buffer( _CAM_ExtIspState *pCameraState );
static CAM_Error _process_videostate_buffer( _CAM_ExtIspState *pCameraState );
static CAM_Error _process_stillstate_buffer( _CAM_ExtIspState *pCameraState );

static CAM_Bool  _is_valid_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Bool  _is_configurable_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _get_required_sensor_settings( _CAM_ExtIspState *pCameraState, CAM_CaptureState eCaptureState, _CAM_PostProcessParam *pPpuShotParam,
                                                _CAM_SmartSensorConfig *pSensorConfig );
static CAM_Error _calc_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Size *pSize, CAM_Int32s iDigitalZoomQ16,
                                     CAM_Int32s *pSensorDigitalZoomQ16, CAM_Int32s *pPpuDigitalZoomQ16 );
static CAM_Error _convert_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Size *pSize, CAM_CaptureState eToState );
static CAM_Error _set_sensor_digital_zoom( void *hSensorHandle, CAM_Int32s iSensorDigitalZoomQ16 );
static CAM_Error _check_image_buffer( CAM_ImageBuffer *pImgBuf, _CAM_PortState *pPortState );
static CAM_Error _negotiate_image_format( CAM_ImageFormat eDstFmt, CAM_ImageFormat *pSrcFmt, CAM_ImageFormat *pFmtCap, CAM_Int32s iFmtCapCnt );
static CAM_Error _set_zero_shutter_lag( _CAM_ExtIspState *pCameraState, CAM_StillSubMode eStillSubMode, CAM_Int32s iZslDepth );

// lock protection
static CAM_Error _lock_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _unlock_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId );


/* MACRO definition */ 
#define SEND_EVENT( pState, eEventId, param )\
	do\
	{\
		if ( pState->fnEventHandler != NULL )\
		{\
			pState->fnEventHandler( eEventId, (void*)param, pState->pUserData );\
		}\
		else\
		{\
			TRACE( CAM_WARN, "Warning: can not notify events due to Camera Engine event handler is not set! \n" );\
		}\
	} while ( 0 )

#define SET_SENSOR_SHOT_PARAM( pCameraState, TYPE, PARA_NAME, param, error )\
do\
{\
	_CAM_ShotParam stShotParam = stShotParamStatus;\
	stShotParam.PARA_NAME = (TYPE)param;\
	error = SmartSensor_SetShotParam( pCameraState->hSensorHandle, &stShotParam );\
	if ( error == CAM_ERROR_NONE )\
	{\
		pCameraState->stShotParam.PARA_NAME = (TYPE)param;\
	}\
} while ( 0 )

#define SET_SENSOR_SHOT_PARAM2( pCameraState, TYPE1, PARA_NAME1, param1, TYPE2, PARA_NAME2, param2, error )\
do\
{\
	_CAM_ShotParam stShotParam = stShotParamStatus;\
	stShotParam.PARA_NAME1 = (TYPE1)param1;\
	stShotParam.PARA_NAME2 = (TYPE2)param2;\
	error = SmartSensor_SetShotParam( pCameraState->hSensorHandle, &stShotParam );\
	if ( error == CAM_ERROR_NONE )\
	{\
		pCameraState->stShotParam.PARA_NAME1 = (TYPE1)param1;\
		pCameraState->stShotParam.PARA_NAME2 = (TYPE2)param2;\
	}\
} while ( 0 )


/* Global entities */
// ext-isp entry
const _CAM_DriverEntry entry_extisp =
{
	"camera_extisp",
	_open,
	_close,
	_command,
};

// #define YUYV2UYVY(a)  ((a & 0x0F) << 8) || ((a & 0xF0) >> 8) || ((a & 0x0F00) << 8) || ((a & 0xF000) >> 8)
#define YUYV2UYVY(a)  (a)

inline void swap(unsigned int* a, unsigned int* b) {
    unsigned int tmp = YUYV2UYVY(*a);
    *a = YUYV2UYVY(*b);
    *b = tmp;
}

/* Function definitions */
#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
static CAM_Int32s ProcessBufferThread( void *pParam )
{
	CAM_Int32s       ret           = 0;
	CAM_Error        error         = CAM_ERROR_NONE;
	_CAM_ExtIspState *pCameraState = (_CAM_ExtIspState*)pParam;


	for ( ; ; )
	{
		error = CAM_ERROR_NONE;

		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}

		if ( pCameraState->stProcessBufferThread.bExitThread )
		{
			if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}
			break;
		}

		error = _run_one_frame( pCameraState );
		switch ( error )
		{
		case CAM_ERROR_NONE:
		case CAM_ERROR_STATELIMIT:
			// set event because there's potential more work to do
			if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}
			break;

		case CAM_ERROR_NOTENOUGHBUFFERS:
			/* This error happens when camera-engine client doesn't en-queue enough number of buffers
			 * when it happens, "ProcessBufferThread" should sleep till camera-engine client send new commands
			 */
			break;

		case CAM_ERROR_PORTNOTACTIVE:
			/* This error happens when camera-engine is in Null/ Idle state, or in still state while all the required
			 * images have been generated. When it happens, "ProcessBufferThread" should sleep till camera-engine client
			 * send new commands.
			 */
			break;

		case CAM_ERROR_FATALERROR:
			/* This error happens when camera device met a serious issue and can not automatically recover
			 * the connection with sensor. When it happens, "ProcessBufferThread" should sleep till camera-engine
			 * client reset the camera device by change Camera Engine state to Idle.
			 */
			SEND_EVENT( pCameraState, CAM_EVENT_FATALERROR, 0 );
			break;

		default:
			TRACE( CAM_ERROR, "Error: ProcessBufferThread encountered error code[%d]( %s, %s, %d )\n",
			       error, __FILE__, __FUNCTION__, __LINE__ );
			break;
		}
		// prevent ProcessBufferThread locks the camera-engine port all the time.
		// XXX: make assumption that the maximum can no faster than 120fps
		CAM_Sleep( 8333 );
	}

	return 0;
}
#endif

static CAM_Int32s CheckFocusThread( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s      ret               = 0;
	CAM_Error       error             = CAM_ERROR_NONE;
	CAM_Bool        bFocusAutoStopped = CAM_FALSE;
	CAM_FocusState  eFocusState;

	ASSERT( pCameraState != NULL );

	for ( ; ; )
	{
		if ( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}

		if ( pCameraState->stFocusThread.bExitThread )
		{
			// be destroyed
			if ( pCameraState->stFocusThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pCameraState->stFocusThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}
			break;
		}

		bFocusAutoStopped = CAM_FALSE;
		eFocusState       = CAM_FOCUSSTATE_LIMIT;
		error = SmartSensor_CheckFocusState( pCameraState->hSensorHandle, &bFocusAutoStopped, &eFocusState );
		ASSERT_CAM_ERROR( error );

		ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );

		if ( !pCameraState->bFocusSleep && bFocusAutoStopped )
		{
			CAM_EventId eEventId    = ( eFocusState == CAM_FOCUSSTATE_OUTOFFOCUS ) ? CAM_EVENT_OUTOF_FOCUS : CAM_EVENT_IN_FOCUS;
			CAM_Bool    bFocusState = ( eEventId == CAM_EVENT_OUTOF_FOCUS ) ? CAM_FALSE : CAM_TRUE;

			SEND_EVENT( pCameraState, eEventId, -1 );
			SEND_EVENT( pCameraState, CAM_EVENT_FOCUS_AUTO_STOP, bFocusState );
			CELOG( "focus time: %.2f ms, result[%d]\n", ( CAM_TimeGetTickCount() - pCameraState->tFocusBegin ) / 1000.0, bFocusState );

			pCameraState->bFocusSleep = CAM_TRUE;
		}

		if ( !pCameraState->bFocusSleep )
		{
			if ( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventSet( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}
		}

		ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
		ASSERT( ret == 0 );

		CAM_Sleep( 8333 );
	}

	return 0;
}

static CAM_Error _open( void **ppDeviceData )
{
	CAM_Error        error           = CAM_ERROR_NONE;
	_CAM_ExtIspState *pCameraState   = NULL;
	CAM_Int32s       i               = 0;
	CAM_Int32s       ret             = 0;

	if ( NULL == ppDeviceData )
	{
		TRACE( CAM_ERROR, "Error: the location where we can store camera-engine handle should not be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	// allocate camera engine internal state structure
	pCameraState = (_CAM_ExtIspState*)malloc( sizeof( _CAM_ExtIspState ) );
	memset( pCameraState, 0, sizeof( _CAM_ExtIspState ) );

	// enum supported sensors
	error = SmartSensor_EnumSensors( &pCameraState->iSensorCount, pCameraState->stSensorProp );
	if ( error != CAM_ERROR_NONE )
	{
		free( pCameraState );
		pCameraState  = NULL;
		*ppDeviceData = NULL;

		TRACE( CAM_ERROR, "Error: camera-engine enum sensor failed( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}

	// initialize buffer queues of each port
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		ret = _InitQueue( &pCameraState->stPortState[i].qEmpty, CAM_MAX_PORT_BUF_CNT );
		ASSERT( ret == 0 );
		ret = _InitQueue( &pCameraState->stPortState[i].qFilled, CAM_MAX_PORT_BUF_CNT );
		ASSERT( ret == 0 );

		ret = CAM_MutexCreate( &pCameraState->stPortState[i].hMutex ); 
		ASSERT( ret == 0 );
	}

	// init ppu management
	error = ppu_init( &pCameraState->hPpuHandle );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}
	pCameraState->iPpuSrcImgAllocateCnt = 0;

	// no sensor is chosen
	pCameraState->iSensorID = -1;

	// check focus thread is not open
	pCameraState->stFocusThread.iThreadID = -1;

	// create process buffer thread if needed
#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
	pCameraState->stProcessBufferThread.stThreadAttribute.iDetachState  = PTHREAD_CREATE_DETACHED;
	pCameraState->stProcessBufferThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pCameraState->stProcessBufferThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );

	CAM_ThreadCreate( &pCameraState->stProcessBufferThread, ProcessBufferThread, (void*)pCameraState );
#endif

#if defined( BUILD_OPTION_STRATEGY_ENABLE_AUTOFOCUS )
	pCameraState->stFocusThread.stThreadAttribute.iDetachState  = PTHREAD_CREATE_DETACHED;
	pCameraState->stFocusThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pCameraState->stFocusThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pCameraState->stFocusThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pCameraState->stFocusThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );

	ret = CAM_ThreadCreate( &pCameraState->stFocusThread, CheckFocusThread, (void*)pCameraState );
	ASSERT( ret == 0 );
#endif
	ret = CAM_MutexCreate( &pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	pCameraState->bFocusSleep = CAM_TRUE;

	// create camera-engine control path mutex
	ret = CAM_MutexCreate( &pCameraState->hCEMutex );
	ASSERT( ret == 0 );

	// camera now in NULL state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_NULL;

	*ppDeviceData = pCameraState;

	return error;
}

static CAM_Error _close( void **ppDeviceData )
{
	CAM_Error        error         = CAM_ERROR_NONE;
	_CAM_ExtIspState *pCameraState = NULL;
	CAM_Int32s       i             = 0;
	CAM_Int32s       ret           = 0;

	if ( NULL == ppDeviceData || NULL == *ppDeviceData )
	{
		TRACE( CAM_ERROR, "Error: camera_extisp handle shouldn't be NULL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	pCameraState = (_CAM_ExtIspState*)(*ppDeviceData);

#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
	ret = CAM_ThreadDestroy( &pCameraState->stProcessBufferThread );
	ASSERT( ret == 0 );

	if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventDestroy( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );
	}

	if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventDestroy( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );
	}
#endif

#if defined( BUILD_OPTION_STRATEGY_ENABLE_AUTOFOCUS )
	ret = CAM_ThreadDestroy( &pCameraState->stFocusThread );
	ASSERT( ret == 0 );

	if ( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup )
	{
		ret = CAM_EventDestroy( &pCameraState->stFocusThread.stThreadAttribute.hEventWakeup );
		ASSERT( ret == 0 );
	}

	if ( pCameraState->stFocusThread.stThreadAttribute.hEventExitAck )
	{
		ret = CAM_EventDestroy( &pCameraState->stFocusThread.stThreadAttribute.hEventExitAck );
		ASSERT( ret == 0 );
	}
#endif
	ret = CAM_MutexDestroy( &pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	pCameraState->bFocusSleep = CAM_TRUE;

	if ( pCameraState->bIsFaceDetectionOpen )
	{
		error = SmartSensor_CancelFaceDetection( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );

		pCameraState->bIsFaceDetectionOpen = CAM_FALSE;
	}

	ret = CAM_MutexDestroy( &pCameraState->hCEMutex );
	ASSERT( ret == 0 );

	error = ppu_deinit( &pCameraState->hPpuHandle );
	ASSERT_CAM_ERROR( error );

	if ( pCameraState->hSensorHandle != NULL )
	{
		error = SmartSensor_Deinit( &pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );
	}

	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		ret = _DeinitQueue( &pCameraState->stPortState[i].qEmpty );
		ASSERT( ret == 0 );
		ret = _DeinitQueue( &pCameraState->stPortState[i].qFilled );
		ASSERT( ret == 0 );

		ret = CAM_MutexDestroy( &pCameraState->stPortState[i].hMutex );
		ASSERT( ret == 0 );
	}

	free( *ppDeviceData );
	*ppDeviceData = NULL;

	return CAM_ERROR_NONE;
}


static CAM_Error _command( void *pDeviceData,
                           CAM_Command cmd,
                           CAM_Param param1,
                           CAM_Param param2 )
{
	CAM_Int32s             ret               = 0;
	CAM_Error              error             = CAM_ERROR_NONE;
	_CAM_ExtIspState       *pCameraState     = (_CAM_ExtIspState*)pDeviceData;
	CAM_ShotModeCapability *pShotModeCap     = &pCameraState->stShotModeCap;
	_CAM_ShotParam         stShotParamStatus;

	memset( &stShotParamStatus, 0, sizeof( _CAM_ShotParam ) );

	if ( cmd != CAM_CMD_ENUM_SENSORS && cmd != CAM_CMD_QUERY_CAMERA_CAP )
	{
		_CHECK_BAD_POINTER( pCameraState );
		if ( pCameraState->hSensorHandle != NULL )
		{
			error = SmartSensor_GetShotParam( pCameraState->hSensorHandle, &stShotParamStatus );
			ASSERT_CAM_ERROR( error );
		}

		ret = CAM_MutexLock( pCameraState->hCEMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );
	}

	switch ( cmd )
	{
	////////////////////////////////////////////////////////////////////////////////
	// initial settings
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_ENUM_SENSORS:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_BAD_POINTER( param2 );

		error = SmartSensor_EnumSensors( (CAM_Int32s*)param1, param2 );
		break;

	case CAM_CMD_QUERY_CAMERA_CAP:
		_CHECK_BAD_POINTER( param2 );

		error = _get_camera_capability( (CAM_Int32s)param1, (CAM_CameraCapability*)param2 );
		break;

	case CAM_CMD_SET_SENSOR_ID:
		_lock_port( pCameraState, CAM_PORT_ANY );
		error = _set_sensor_id( pCameraState, (CAM_Int32s)param1 );
		_unlock_port( pCameraState, CAM_PORT_ANY );
		break;

	case CAM_CMD_GET_SENSOR_ID:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->iSensorID;
		break;

	case CAM_CMD_SET_EVENTHANDLER:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_NULL | CAM_CAPTURESTATE_IDLE );

		/* NOTE: no need to check the pointer validity. If param1 == NULL, it means that
		 *       user want to remove the previously registered event handler
		 */
		pCameraState->fnEventHandler = (CAM_EventHandler)param1;
		pCameraState->pUserData      = (void*)param2;

		if ( pCameraState->hSensorHandle != NULL )
		{
			error = SmartSensor_RegisterEventHandler( pCameraState->hSensorHandle, pCameraState->fnEventHandler, pCameraState->pUserData );
			ASSERT_CAM_ERROR( error );
		}
		break;

	case CAM_CMD_SET_SENSOR_ORIENTATION: 
		/* XXX: for some smart sensor and driver restriction, sensor orientation can only be changed while stream-off
		 *      but for usage-model perspective, we all know that it's unreasonable
		 */
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE );
		_CHECK_SUPPORT_ENUM( (CAM_FlipRotate)param1, pCameraState->stCameraCap.eSupportedSensorRotate, pCameraState->stCameraCap.iSupportedSensorRotateCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_FlipRotate, eSensorRotation, param1, error );
		break;

	case CAM_CMD_GET_SENSOR_ORIENTATION:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_FlipRotate*)param1 = pCameraState->stShotParam.eSensorRotation;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// state machine control commands
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_SET_STATE:
		CAM_PROFILE( (CAM_CaptureState)param1, CAM_CAPTURESTATE_STILL, CAM_PERF_OVERALL_BEGIN, "CAM_PERF: shutter release time begin" );
		_lock_port( pCameraState, CAM_PORT_ANY );
		CAM_PROFILE( (CAM_CaptureState)param1, CAM_CAPTURESTATE_STILL, CAM_PERF_INTERVAL, "CAM_PERF: lock port" );
		error = _set_camera_state( pCameraState, (CAM_CaptureState)param1 );
		CAM_PROFILE( (CAM_CaptureState)param1, CAM_CAPTURESTATE_STILL, CAM_PERF_INTERVAL, "CAM_PERF: preview --> still state" );
		_unlock_port( pCameraState, CAM_PORT_ANY );
		CAM_PROFILE( (CAM_CaptureState)param1, CAM_CAPTURESTATE_STILL, CAM_PERF_INTERVAL, "CAM_PERF: unlock port" );

#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
		// wake-up process buffer thread since port may become active
		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
#endif
		break;

	case CAM_CMD_GET_STATE:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY );

		*(CAM_CaptureState*)param1 = pCameraState->eCaptureState;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// buffer management
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_PORT_GET_BUFREQ:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _get_port_bufreq( pCameraState, (CAM_Int32s)param1, (CAM_ImageBufferReq*)param2 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	case CAM_CMD_PORT_ENQUEUE_BUF:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _enqueue_buffer( pCameraState, (CAM_Int32s)param1, (CAM_ImageBuffer*)param2 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );

#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
		// wake-up process buffer thread since some buffer maybe can dequeue
		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
#endif
		break;

	case CAM_CMD_PORT_DEQUEUE_BUF:
		{
			CAM_Int32s iPortId;
			_CHECK_BAD_POINTER( param1 );
			iPortId = *(CAM_Int32s*)param1;
			_CHECK_BAD_ANY_PORT_ID( iPortId );

			_lock_port( pCameraState, iPortId );
			error = _dequeue_buffer( pCameraState, (CAM_Int32s*)param1, (CAM_ImageBuffer**)param2 );
			_unlock_port( pCameraState, iPortId );
		}
		break;

	case CAM_CMD_PORT_FLUSH_BUF:
		_CHECK_BAD_ANY_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _flush_buffer( pCameraState, (CAM_Int32s)param1 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	////////////////////////////////////////////////////////////////////////////////
	//  port configuration
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_PORT_SET_SIZE:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _set_port_size( pCameraState, (CAM_Int32s)param1, (CAM_Size*)param2 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	case CAM_CMD_PORT_GET_SIZE:
		_CHECK_BAD_POINTER( param2 );
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_NULL );

		((CAM_Size*)param2)->iWidth  = pCameraState->stPortState[(CAM_Int32s)param1].iWidth;
		((CAM_Size*)param2)->iHeight = pCameraState->stPortState[(CAM_Int32s)param1].iHeight;
		break;

	case CAM_CMD_PORT_SET_FORMAT:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _set_port_format( pCameraState, (CAM_Int32s)param1, (CAM_ImageFormat)param2 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	case CAM_CMD_PORT_GET_FORMAT:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );
		_CHECK_BAD_POINTER( param2 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_ImageFormat*)param2 = pCameraState->stPortState[(CAM_Int32s)param1].eFormat;
		break;

	case CAM_CMD_PORT_SET_ROTATION:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _set_port_rotation( pCameraState, (CAM_Int32s)param1, (CAM_FlipRotate)param2 );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	case CAM_CMD_PORT_GET_ROTATION:
		_CHECK_BAD_SINGLE_PORT_ID( (CAM_Int32s)param1 );
		_CHECK_BAD_POINTER( param2 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_FlipRotate*)param2 = pCameraState->stPortState[(CAM_Int32s)param1].eRotation;
		break;

	case CAM_CMD_SET_JPEGPARAM:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

		_lock_port( pCameraState, CAM_PORT_STILL );
		error = _set_jpeg_param( pCameraState, (CAM_JpegParam*)param1 );
		_unlock_port( pCameraState, CAM_PORT_STILL );
		break;

	case CAM_CMD_GET_JPEGPARAM:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_JpegParam*)param1 = pCameraState->stJpegParam;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// frame rate
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_SET_FRAMERATE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState,  CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iFpsQ16, param1, error );
		break;

	case CAM_CMD_GET_FRAMERATE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iFpsQ16;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// preview aspect-ratio
	////////////////////////////////////////////////////////////////////////////////

	case CAM_CMD_SET_PREVIEW_RESIZEFAVOR:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW );

		pCameraState->bPreviewFavorAspectRatio = (CAM_Bool)param1;
		break;

	case CAM_CMD_GET_PREVIEW_RESIZEFAVOR:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_Bool*)param1 = pCameraState->bPreviewFavorAspectRatio;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// shooting parameters
	////////////////////////////////////////////////////////////////////////////////

	// shot mode
	case CAM_CMD_QUERY_SHOTMODE_CAP:
		_CHECK_BAD_POINTER( param2 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_SUPPORT_ENUM( (CAM_ShotMode)param1, pCameraState->stCameraCap.eSupportedShotMode, pCameraState->stCameraCap.iSupportedShotModeCnt );

		error = _get_shotmode_capability ( pCameraState->iSensorID, (CAM_ShotMode)param1, (CAM_ShotModeCapability*)param2 );
		break;

	case CAM_CMD_SET_SHOTMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ShotMode)param1, pCameraState->stCameraCap.eSupportedShotMode, pCameraState->stCameraCap.iSupportedShotModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_ShotMode, eShotMode, param1, error );

		if ( error == CAM_ERROR_NONE )
		{
			// update shot mode capability
			error = _get_shotmode_capability( pCameraState->iSensorID, (CAM_ShotMode)param1, &pCameraState->stShotModeCap );
			ASSERT_CAM_ERROR( error );

			error = SmartSensor_GetShotParam( pCameraState->hSensorHandle, &stShotParamStatus );
			ASSERT_CAM_ERROR( error );

			// update current shot parameters
			memcpy( &pCameraState->stShotParam, &stShotParamStatus, sizeof(_CAM_ShotParam) );
		}
		break;

	case CAM_CMD_GET_SHOTMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ShotMode*)param1 = pCameraState->stShotParam.eShotMode;
		break;

	// video sub-mode
	case CAM_CMD_SET_VIDEO_SUBMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_SUPPORT_ENUM( (CAM_VideoSubMode)param1, pShotModeCap->eSupportedVideoSubMode, pShotModeCap->iSupportedVideoSubModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_VideoSubMode, eVideoSubMode, param1, error );
		break;

	case CAM_CMD_GET_VIDEO_SUBMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_VideoSubMode*)param1 = pCameraState->stShotParam.eVideoSubMode;
		break;

	// still sub-mode
	case CAM_CMD_SET_STILL_SUBMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW );
		_CHECK_SUPPORT_ENUM( (CAM_StillSubMode)param1, pShotModeCap->eSupportedStillSubMode, pShotModeCap->iSupportedStillSubModeCnt );
		if ( (CAM_StillSubMode)param1 != CAM_STILLSUBMODE_SIMPLE )
		{
			_CHECK_BAD_POINTER( param2 );
		}

		_lock_port( pCameraState, CAM_PORT_PREVIEW );
		error = _set_still_submode( pCameraState, (CAM_StillSubMode)param1, (CAM_StillSubModeParam*)param2 );
		_unlock_port( pCameraState, CAM_PORT_PREVIEW );
		break;

	case CAM_CMD_GET_STILL_SUBMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_StillSubMode*)param1 = pCameraState->stShotParam.eStillSubMode;

		if ( param2 != NULL )
		{
			*((CAM_StillSubModeParam*)param2) = pCameraState->stShotParam.stStillSubModeParam;
		}
		break;

	case CAM_CMD_SET_EXPOSUREMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ExposureMode)param1, pShotModeCap->eSupportedExpMode, pShotModeCap->iSupportedExpModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_ExposureMode, eExpMode, param1, error );
		break;

	case CAM_CMD_GET_EXPOSUREMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ExposureMode*)param1 = pCameraState->stShotParam.eExpMode;
		break;

	// exposure metering mode
	case CAM_CMD_SET_EXPOSUREMETERMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ExposureMeterMode)param1, pShotModeCap->eSupportedExpMeter, pShotModeCap->iSupportedExpMeterCnt );

		if ( (CAM_ExposureMeterMode)param1 == CAM_EXPOSUREMETERMODE_MANUAL )
		{
			CAM_MultiROI *pExpMeterROI = (CAM_MultiROI*)param2;
			_CHECK_BAD_POINTER( pExpMeterROI );
			_CHECK_SUPPORT_RANGE( pExpMeterROI->iROICnt, 1, pShotModeCap->iMaxExpMeterROINum );
			_CHECK_BAD_ROI_COORDINATE( pExpMeterROI );

			SET_SENSOR_SHOT_PARAM2( pCameraState, CAM_ExposureMeterMode, eExpMeterMode, param1, CAM_MultiROI, stExpMeterROI, *pExpMeterROI, error );
		}
		else
		{
			SET_SENSOR_SHOT_PARAM( pCameraState, CAM_ExposureMeterMode, eExpMeterMode, param1, error );
		}
		break;

	case CAM_CMD_GET_EXPOSUREMETERMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ExposureMeterMode*)param1 = pCameraState->stShotParam.eExpMeterMode;

		if ( pCameraState->stShotParam.eExpMeterMode == CAM_EXPOSUREMETERMODE_MANUAL && param2 != NULL )
		{
			*(CAM_MultiROI*)param2 = pCameraState->stShotParam.stExpMeterROI;
		}
		break;

	case CAM_CMD_SET_EVCOMPENSATION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinEVCompQ16, pShotModeCap->iMaxEVCompQ16 );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iEvCompQ16, param1, error );
		break;

	case CAM_CMD_GET_EVCOMPENSATION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iEvCompQ16;
		break;

	case CAM_CMD_SET_ISO:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ISOMode)param1, pShotModeCap->eSupportedIsoMode, pShotModeCap->iSupportedIsoModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_ISOMode, eIsoMode, param1, error );
		break;

	case CAM_CMD_GET_ISO:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ISOMode*)param1 = pCameraState->stShotParam.eIsoMode;
		break;

	case CAM_CMD_SET_SHUTTERSPEED:
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinShutSpdQ16,pShotModeCap->iMaxShutSpdQ16 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_STILL & ~CAM_CAPTURESTATE_NULL );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iShutterSpeedQ16, param1, error );
		break;

	case CAM_CMD_GET_SHUTTERSPEED:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iShutterSpeedQ16;
		break;

	case CAM_CMD_SET_BANDFILTER:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_BandFilterMode)param1, pShotModeCap->eSupportedBdFltMode, pShotModeCap->iSupportedBdFltModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_BandFilterMode, eBandFilterMode, param1, error );
		break;

	case CAM_CMD_GET_BANDFILTER:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_BandFilterMode*)param1 = pCameraState->stShotParam.eBandFilterMode;
		break;

	case CAM_CMD_SET_FLASHMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_FlashMode)param1, pShotModeCap->eSupportedFlashMode, pShotModeCap->iSupportedFlashModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_FlashMode, eFlashMode, param1, error );
		break;

	case CAM_CMD_GET_FLASHMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );
		*(CAM_FlashMode*)param1 = pCameraState->stShotParam.eFlashMode;
		break;

	// white balance
	case CAM_CMD_SET_WHITEBALANCEMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_WhiteBalanceMode)param1, pShotModeCap->eSupportedWBMode, pShotModeCap->iSupportedWBModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_WhiteBalanceMode, eWBMode, param1, error );
		break;

	case CAM_CMD_GET_WHITEBALANCEMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_WhiteBalanceMode*)param1 = pCameraState->stShotParam.eWBMode;
		break;

	// focus
	case CAM_CMD_SET_FOCUSMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_FocusMode)param1, pShotModeCap->eSupportedFocusMode, pShotModeCap->iSupportedFocusModeCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_FocusMode, eFocusMode, param1, error );
		break;

	case CAM_CMD_GET_FOCUSMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FocusMode*)param1 = pCameraState->stShotParam.eFocusMode;
		break;

	case CAM_CMD_SET_FOCUSZONE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_FocusZoneMode)param1, pShotModeCap->eSupportedFocusZoneMode, pShotModeCap->iSupportedFocusZoneModeCnt );

		if ( pCameraState->stShotParam.eFocusMode != CAM_FOCUS_AUTO_ONESHOT &&
		     pCameraState->stShotParam.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_PICTURE &&
		     pCameraState->stShotParam.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_VIDEO )
		{
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}

		if ( (CAM_FocusZoneMode)param1 == CAM_FOCUSZONEMODE_MANUAL )
		{
			CAM_MultiROI *pFocusROI = (CAM_MultiROI*)param2;
			_CHECK_BAD_POINTER( pFocusROI );
			_CHECK_SUPPORT_RANGE( pFocusROI->iROICnt, 1, pShotModeCap->iMaxFocusROINum );
			_CHECK_BAD_ROI_COORDINATE( pFocusROI );

			SET_SENSOR_SHOT_PARAM2( pCameraState, CAM_FocusZoneMode, eFocusZoneMode, param1, CAM_MultiROI, stFocusROI, *pFocusROI, error );
		}
		else
		{
			SET_SENSOR_SHOT_PARAM( pCameraState, CAM_FocusZoneMode, eFocusZoneMode, param1, error );
		}
		break;

	case CAM_CMD_GET_FOCUSZONE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FocusZoneMode*)param1 = pCameraState->stShotParam.eFocusZoneMode;
		if ( pCameraState->stShotParam.eFocusZoneMode == CAM_FOCUSZONEMODE_MANUAL && param2 != NULL )
		{
			*(CAM_MultiROI*)param2 = pCameraState->stShotParam.stFocusROI;
		}
		break;

	case CAM_CMD_START_FOCUS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

		if ( stShotParamStatus.eFocusMode != CAM_FOCUS_AUTO_ONESHOT &&
		     stShotParamStatus.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_PICTURE &&
		     stShotParamStatus.eFocusMode != CAM_FOCUS_MACRO )
		{
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
		else
		{
			ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );

			if ( !pCameraState->bFocusSleep )
			{
				TRACE( CAM_ERROR, "Error: the last auto-focus is not finished yet( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				error = CAM_ERROR_NOTSUPPORTEDCMD;
			}
			else
			{

				// XXX: according OV guild, CE implement macro focus as manual focus, but framework require same behavior between macro and auto focus,
				//      which will use start focus to trigger and need call back to indicate focus state, so fake operation is performed for macro mode.
				if ( stShotParamStatus.eFocusMode != CAM_FOCUS_MACRO )
				{
					error = SmartSensor_StartFocus( pCameraState->hSensorHandle );
				}

				if ( error == CAM_ERROR_NONE )
				{
					pCameraState->tFocusBegin = CAM_TimeGetTickCount();
					// trigger checkfocus thread
					pCameraState->bFocusSleep = CAM_FALSE;
					if ( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup )
					{
						ret = CAM_EventSet( pCameraState->stFocusThread.stThreadAttribute.hEventWakeup );
						ASSERT( ret == 0 );
					}
				}
			}

			ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
			ASSERT( ret == 0 );
		}
		break;

	case CAM_CMD_CANCEL_FOCUS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO | CAM_CAPTURESTATE_IDLE );

		if ( stShotParamStatus.eFocusMode != CAM_FOCUS_AUTO_ONESHOT &&
		     stShotParamStatus.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_PICTURE &&
		     stShotParamStatus.eFocusMode != CAM_FOCUS_MACRO )
		{
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
		else if ( stShotParamStatus.eFocusMode != CAM_FOCUS_MACRO  )
		{
			ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );

			if ( !pCameraState->bFocusSleep )
			{
				TRACE( CAM_WARN, "Warnning: Focussing is not finished under [%d]mode, it always recommands to cancel focus after focus finished( %s, %s, %d )!\n", stShotParamStatus.eFocusMode, __FILE__, __FUNCTION__, __LINE__ );

				pCameraState->bFocusSleep = CAM_TRUE;
			}

			ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
			ASSERT( ret == 0 );

			error = SmartSensor_CancelFocus( pCameraState->hSensorHandle );
		}
		break;

	// zoom
	case CAM_CMD_SET_DIGZOOM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pCameraState->stShotModeCap.iMinDigZoomQ16, pCameraState->stShotModeCap.iMaxDigZoomQ16 );

		_lock_port( pCameraState, CAM_PORT_ANY );
		error = _set_digital_zoom( pCameraState, (CAM_Int32s)param1 );
		_unlock_port( pCameraState, CAM_PORT_ANY );
		break;

	case CAM_CMD_GET_DIGZOOM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iDigZoomQ16;
		break;

	// digital special effect
	case CAM_CMD_SET_COLOREFFECT:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ColorEffect)param1, pShotModeCap->eSupportedColorEffect, pShotModeCap->iSupportedColorEffectCnt );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_ColorEffect, eColorEffect, param1, error );
		break;

	case CAM_CMD_GET_COLOREFFECT:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ColorEffect*)param1 = pCameraState->stShotParam.eColorEffect;
		break;

	// visual effect
	case CAM_CMD_SET_BRIGHTNESS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinBrightness, pShotModeCap->iMaxBrightness );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iBrightness, param1, error );
		break;

	case CAM_CMD_GET_BRIGHTNESS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iBrightness;
		break;

	case CAM_CMD_SET_CONTRAST:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinContrast, pShotModeCap->iMaxContrast );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iContrast, param1, error );
		break;

	case CAM_CMD_GET_CONTRAST:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iContrast;
		break;

	case CAM_CMD_SET_SATURATION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinSaturation, pShotModeCap->iMaxSaturation );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iSaturation, param1, error );
		break;

	case CAM_CMD_GET_SATURATION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iSaturation;
		break;

	case CAM_CMD_SET_SHARPNESS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinSharpness, pShotModeCap->iMaxSharpness );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iSharpness, param1, error );
		break;

	case CAM_CMD_GET_SHARPNESS:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iSharpness;
		break;

	case CAM_CMD_SET_OPTZOOM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinOptZoomQ16, pShotModeCap->iMaxOptZoomQ16 );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iOptZoomQ16, param1, error );
		break;

	case CAM_CMD_GET_OPTZOOM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32u*)param1 = pCameraState->stShotParam.iOptZoomQ16;
		break;

	case CAM_CMD_SET_FNUM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pShotModeCap->iMinFNumQ16, pShotModeCap->iMaxFNumQ16 );

		SET_SENSOR_SHOT_PARAM( pCameraState, CAM_Int32s, iFNumQ16, param1, error );
		break;

	case CAM_CMD_GET_FNUM:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		*(CAM_Int32s*)param1 = pCameraState->stShotParam.iFNumQ16;
		break;

	case CAM_CMD_RESET_CAMERA:
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, CAM_RESET_FAST, CAM_RESET_COMPLETE );

		error = _reset_camera( pCameraState, (CAM_Int32s)param1 );

#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
		// wake-up process buffer thread since port may become active
		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
#endif
		break;

	case CAM_CMD_START_FACEDETECTION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

		error = _start_face_detection( pCameraState );
		break;

	case CAM_CMD_CANCEL_FACEDETECTION:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

		error = _cancel_face_detection( pCameraState );
		break;

	default:
		TRACE( CAM_ERROR, "Error: Unrecognized command id[%d]( %s, %s, %d )\n", cmd, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
		break;
	}

	if ( cmd != CAM_CMD_ENUM_SENSORS && cmd != CAM_CMD_QUERY_CAMERA_CAP )
	{
		ret = CAM_MutexUnlock( pCameraState->hCEMutex );
		ASSERT( ret == 0 );
	}

	return error;
}

static CAM_Error _set_sensor_id( _CAM_ExtIspState *pCameraState, CAM_Int32s iSensorID )
{
	CAM_Error      error = CAM_ERROR_NONE;
	CAM_Int32s     i = 0, ret = -1;

	_CHECK_SUPPORT_RANGE( iSensorID, 0, pCameraState->iSensorCount - 1 );
	// sensor ID can only be changed when all ports are configurable
	if ( !_is_configurable_port( pCameraState, CAM_PORT_PREVIEW ) ||\
	     !_is_configurable_port( pCameraState, CAM_PORT_VIDEO ) ||\
	     !_is_configurable_port( pCameraState, CAM_PORT_STILL ) )
	{
		TRACE( CAM_ERROR, "Error: port is not configurable, pls check if there is buffer pushed into any port( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

#if 0

	// only in NULL state that sensor id can switch
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_NULL );
	ASSERT( pCameraState->iSensorID == -1 );

#else

	// only in NULL/IDLE state that sensor id can switch
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_NULL | CAM_CAPTURESTATE_IDLE );

	// if change sensor ID, first deinit obsolete sensor node
	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
	{
		ASSERT( pCameraState->iSensorID != -1 );

		if ( pCameraState->iSensorID == iSensorID )
		{
			return CAM_ERROR_NONE;
		}
		else
		{
			error = SmartSensor_Deinit( &pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );

			// here we will flush all buffers assigned to the old sensor in all ports, so client should aware that, he can
			// handle those buffers after switch sensor ID. A good practice is that client flush all ports themselves.
			for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
			{
				ret = _FlushQueue( &pCameraState->stPortState[i].qEmpty );
				ASSERT( ret == 0 );
				ret = _FlushQueue( &pCameraState->stPortState[i].qFilled );
				ASSERT( ret == 0 );
			}

		}
	}
#endif

	// init the new sensor handle
	error = SmartSensor_Init( &(pCameraState->hSensorHandle), iSensorID );
	ASSERT_CAM_ERROR( error );

	// register event handler
	error = SmartSensor_RegisterEventHandler( pCameraState->hSensorHandle, pCameraState->fnEventHandler, pCameraState->pUserData );
	ASSERT_CAM_ERROR( error );

	pCameraState->iSensorID = iSensorID;

	// get camera capability under current sensor 
	error = _get_camera_capability( iSensorID, &pCameraState->stCameraCap );
	ASSERT_CAM_ERROR( error );

	// set default port attribute
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		pCameraState->stPortState[i].iWidth  = pCameraState->stCameraCap.stPortCapability[i].stMin.iWidth;
		pCameraState->stPortState[i].iHeight = pCameraState->stCameraCap.stPortCapability[i].stMin.iHeight;
		pCameraState->stPortState[i].eFormat = pCameraState->stCameraCap.stPortCapability[i].eSupportedFormat[0];
	}

	// set default still attributes
	pCameraState->iStillRestCount      = 0;

	// set default preview attributes
	pCameraState->bPreviewFavorAspectRatio = CAM_FALSE;

	// set default JPEG params
	pCameraState->stJpegParam.iSampling      = 1; // 0 - 420, 1 - 422, 2 - 444
	pCameraState->stJpegParam.iQualityFactor = 80;
	pCameraState->stJpegParam.bEnableExif    = CAM_FALSE;
	pCameraState->stJpegParam.bEnableThumb   = CAM_FALSE;
	pCameraState->stJpegParam.iThumbWidth    = 0;
	pCameraState->stJpegParam.iThumbHeight   = 0;

	// by default, we will set shot mode as AUTO
	error = SmartSensor_GetShotParam( pCameraState->hSensorHandle, &pCameraState->stShotParam );
	ASSERT_CAM_ERROR( error );

	pCameraState->stShotParam.eShotMode = CAM_SHOTMODE_AUTO;
	error = SmartSensor_SetShotParam( pCameraState->hSensorHandle, &pCameraState->stShotParam );
	ASSERT_CAM_ERROR( error );

	error = _get_shotmode_capability( pCameraState->iSensorID, pCameraState->stShotParam.eShotMode, &(pCameraState->stShotModeCap) );
	ASSERT_CAM_ERROR( error );

	// by default, focus not open
	pCameraState->bIsFocusOpen         = CAM_FALSE;

	// by default, face detecion is not open
	pCameraState->bIsFaceDetectionOpen = CAM_FALSE;

	// initialize ppu param
	memset( &pCameraState->stPpuShotParam, 0, sizeof( _CAM_PostProcessParam ) );
	pCameraState->stPpuShotParam.iPpuDigitalZoomQ16 = ( 1 << 16 );

	pCameraState->eCaptureState = CAM_CAPTURESTATE_IDLE;

	// init port buffer requirement
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		error = _get_port_bufreq( pCameraState, i, &pCameraState->stPortState[i].stBufReq );
		ASSERT_CAM_ERROR( error );
	}

	return error;
}

static CAM_Error _get_camera_capability( CAM_Int32s iSensorID, CAM_CameraCapability *pCameraCap )
{
	CAM_Int32s                 i, j;
	CAM_Error                  error = CAM_ERROR_NONE;
	_CAM_SmartSensorCapability stSensorCap;
	CAM_PortCapability         *pPortCap        = NULL;
	CAM_Int32s                 iPpuFmtCnt       = 0;
	CAM_Bool                   bIsArbitrarySize = CAM_FALSE; 
	CAM_ImageFormat            stPpuFmtCap[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT] = {0};

	_CHECK_BAD_POINTER( pCameraCap );

	memset( pCameraCap, 0, sizeof( CAM_CameraCapability ) );

	error = SmartSensor_GetCapability( iSensorID, &stSensorCap );
	ASSERT_CAM_ERROR( error );

	// supported shot mode
	for ( i = 0; i < stSensorCap.iSupportedShotModeCnt; i++ )
	{
		pCameraCap->eSupportedShotMode[i] = stSensorCap.eSupportedShotMode[i];
	}
	pCameraCap->iSupportedShotModeCnt = stSensorCap.iSupportedShotModeCnt;

	// all supported shot parameters
	pCameraCap->stSupportedShotParams = stSensorCap.stSupportedShotParams;
	// add on ppu digital zoom capability
	if ( stSensorCap.stSupportedShotParams.iMaxDigZoomQ16 == stSensorCap.stSupportedShotParams.iMinDigZoomQ16 )
	{
		pCameraCap->stSupportedShotParams.iDigZoomStepQ16 = (CAM_Int32s)(0.2 * 65536 + 0.5);
	}
	if ( stSensorCap.stSupportedShotParams.iMaxDigZoomQ16 < BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16 )
	{
		pCameraCap->stSupportedShotParams.bSupportSmoothDigZoom = CAM_FALSE;
	}
	pCameraCap->stSupportedShotParams.iMinDigZoomQ16 = ( 1 << 16 );
	pCameraCap->stSupportedShotParams.iMaxDigZoomQ16 = BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16;

	// add on ppu ZSL capability
	if ( BUILD_OPTION_STRATEGY_MAX_ZSL_DEPTH > 0 )
	{
		for ( i = 0; i < stSensorCap.stSupportedShotParams.iSupportedStillSubModeCnt; i++ )
		{
			if ( stSensorCap.stSupportedShotParams.eSupportedStillSubMode[i] == CAM_STILLSUBMODE_ZSL )
			{
				break;
			}
		}
		if ( i < stSensorCap.stSupportedShotParams.iSupportedStillSubModeCnt )
		{
			TRACE( CAM_WARN, "Warning: sensor supports ZSL by itself, will directly use sensor's capability( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		}
		else
		{
			TRACE( CAM_WARN, "Warning: sensor doesn't support ZSL, will open cameraengine's S/W ZSL as required( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

			// update zsl capability
			pCameraCap->stSupportedShotParams.eSupportedStillSubMode[i] = CAM_STILLSUBMODE_ZSL;
			pCameraCap->stSupportedShotParams.iSupportedStillSubModeCnt++;
			pCameraCap->stSupportedShotParams.iMaxZslDepth = BUILD_OPTION_STRATEGY_MAX_ZSL_DEPTH;

			for ( j = 0; j < stSensorCap.stSupportedShotParams.iSupportedStillSubModeCnt; j++ )
			{
				if ( stSensorCap.stSupportedShotParams.eSupportedStillSubMode[j] == CAM_STILLSUBMODE_BURST )
				{
					break;
				}
			}
			if ( j < stSensorCap.stSupportedShotParams.iSupportedStillSubModeCnt )
			{
				// update burst + zsl capability
				pCameraCap->stSupportedShotParams.eSupportedStillSubMode[ i + 1 ] = CAM_STILLSUBMODE_BURSTZSL;
				pCameraCap->stSupportedShotParams.iSupportedStillSubModeCnt++;
			}
		}
	}

	/* preview port capability */
	// size
	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_PREVIEW];

	bIsArbitrarySize = stSensorCap.bArbitraryVideoSize;
	if ( bIsArbitrarySize )
	{
		// support arbitrary size by sensor
		pPortCap->stMin = stSensorCap.stMinVideoSize;
		pPortCap->stMax = stSensorCap.stMaxVideoSize;

		pPortCap->bArbitrarySize = bIsArbitrarySize;
	}
	else
	{
		CAM_Size stMinSize = { 0, 0 }, stMaxSize = { 0, 0 };
		
		error = ppu_query_resizer_cap( &bIsArbitrarySize, &stMinSize, &stMaxSize );
		ASSERT_CAM_ERROR( error );

		pPortCap->bArbitrarySize = bIsArbitrarySize;

		if ( pPortCap->bArbitrarySize )
		{
			// if ppu support arbitrary size
			pPortCap->stMin = stMinSize;  
			pPortCap->stMax = stMaxSize;
		}
		else
		{
			// if ppu do not support arbitrary size
			pPortCap->stMin = stSensorCap.stMinVideoSize;
			pPortCap->stMax = stSensorCap.stMaxVideoSize;
		}

		// natively supported sizes by sensor, suppose has better performance
		pPortCap->iSupportedSizeCnt = stSensorCap.iSupportedVideoSizeCnt;
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			pPortCap->stSupportedSize[i] = stSensorCap.stSupportedVideoSize[i];
		}
	}

	// format
	error = ppu_query_csc_cap( stSensorCap.eSupportedVideoFormat, stSensorCap.iSupportedVideoFormatCnt, stPpuFmtCap, &iPpuFmtCnt );
	ASSERT_CAM_ERROR( error );

	for ( i = 0; i < stSensorCap.iSupportedVideoFormatCnt; i++ )
	{
		pPortCap->eSupportedFormat[i] = stSensorCap.eSupportedVideoFormat[i];
        TRACE( CAM_WARN, "eSupportedVideoFormat: %d \n", stSensorCap.eSupportedVideoFormat[i]);
	}
	for ( i = stSensorCap.iSupportedVideoFormatCnt; i < _MIN( ( iPpuFmtCnt + stSensorCap.iSupportedVideoFormatCnt ), CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT ); i++ )
	{
		pPortCap->eSupportedFormat[i] = stPpuFmtCap[i - stSensorCap.iSupportedVideoFormatCnt];
	}
	pPortCap->iSupportedFormatCnt = _MIN( ( iPpuFmtCnt + stSensorCap.iSupportedVideoFormatCnt ), CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT );

	// rotation
	error = ppu_query_rotator_cap( pPortCap->eSupportedRotate, &pPortCap->iSupportedRotateCnt );
	ASSERT_CAM_ERROR( error );

	/* video port capability */
	// FIXME: need to add ppu to video port
	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_VIDEO];

	// size
	pPortCap->bArbitrarySize  = stSensorCap.bArbitraryVideoSize;
	pPortCap->stMin.iWidth    = stSensorCap.stMinVideoSize.iWidth;
	pPortCap->stMin.iHeight   = stSensorCap.stMinVideoSize.iHeight;
	pPortCap->stMax.iWidth    = stSensorCap.stMaxVideoSize.iWidth;
	pPortCap->stMax.iHeight   = stSensorCap.stMaxVideoSize.iHeight;

	if ( !pPortCap->bArbitrarySize )
	{
		pPortCap->iSupportedSizeCnt = stSensorCap.iSupportedVideoSizeCnt;
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			pPortCap->stSupportedSize[i] = stSensorCap.stSupportedVideoSize[i];
		}
	}

	// format 
	pPortCap->iSupportedFormatCnt = stSensorCap.iSupportedVideoFormatCnt;
	for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ ) 
	{
		pPortCap->eSupportedFormat[i] = stSensorCap.eSupportedVideoFormat[i];
        TRACE( CAM_WARN, "eSupportedFormat %d \n", pPortCap->eSupportedFormat[i]);
	}

	// rotate
	pPortCap->iSupportedRotateCnt = 1;
	pPortCap->eSupportedRotate[0] = CAM_FLIPROTATE_NORMAL;


	/* still port capability */
	pPortCap = &pCameraCap->stPortCapability[CAM_PORT_STILL];

	// size
	pPortCap->bArbitrarySize  = stSensorCap.bArbitraryStillSize;
	pPortCap->stMin.iWidth    = stSensorCap.stMinStillSize.iWidth;
	pPortCap->stMin.iHeight   = stSensorCap.stMinStillSize.iHeight;
	pPortCap->stMax.iWidth    = stSensorCap.stMaxStillSize.iWidth;
	pPortCap->stMax.iHeight   = stSensorCap.stMaxStillSize.iHeight;

	if ( !pPortCap->bArbitrarySize )
	{
		pPortCap->iSupportedSizeCnt = stSensorCap.iSupportedStillSizeCnt;
		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ ) 
		{
			pPortCap->stSupportedSize[i] = stSensorCap.stSupportedStillSize[i];
		}
	}

	// format
	if ( stSensorCap.stSupportedJPEGParams.bSupportJpeg )
	{
		// sensor originally support JPEG
		pPortCap->iSupportedFormatCnt = stSensorCap.iSupportedStillFormatCnt;
		for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ ) 
		{
			pPortCap->eSupportedFormat[i] = stSensorCap.eSupportedStillFormat[i];
            TRACE( CAM_WARN, "eSupportedStillFormat: %d \n", pPortCap->eSupportedFormat[i]);
		}

		pCameraCap->stSupportedJPEGParams = stSensorCap.stSupportedJPEGParams;
	}
	else
	{
		CAM_JpegCapability stJpegEncCap;

		error = ppu_query_jpegenc_cap( stSensorCap.eSupportedStillFormat, stSensorCap.iSupportedStillFormatCnt, &stJpegEncCap );
		ASSERT_CAM_ERROR( error );

		if ( stJpegEncCap.bSupportJpeg )
		{
			pPortCap->iSupportedFormatCnt = 1;
			pPortCap->eSupportedFormat[0] = CAM_IMGFMT_JPEG;
			TRACE( CAM_INFO, "Info: sensor[%d] don't support JPEG format, will enable Camera Engine's embeded JPEG compressor...\n", iSensorID );
		}
		else
		{

			pPortCap->iSupportedFormatCnt = stSensorCap.iSupportedStillFormatCnt;
			for ( i = 0; i < pPortCap->iSupportedFormatCnt; i++ )
			{
				pPortCap->eSupportedFormat[i] = stSensorCap.eSupportedStillFormat[i];
			}

		}

		pCameraCap->stSupportedJPEGParams = stJpegEncCap;
	}

	// rotation
	error = ppu_query_rotator_cap( pPortCap->eSupportedRotate, &pPortCap->iSupportedRotateCnt );
	ASSERT_CAM_ERROR( error );

	// sensor rotation capability, generally speaking, sensor only support flip/mirror
	pCameraCap->iSupportedSensorRotateCnt = stSensorCap.iSupportedRotateCnt;
	for ( i = 0; i < pCameraCap->iSupportedSensorRotateCnt; i++ )
	{
		pCameraCap->eSupportedSensorRotate[i] = stSensorCap.eSupportedRotate[i];
	}

	// face detection
	pCameraCap->iMaxFaceNum = stSensorCap.iMaxFaceNum;
	return CAM_ERROR_NONE;
}


static CAM_Error _get_shotmode_capability( CAM_Int32s iSensorId, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Error  error = CAM_ERROR_NONE;
	CAM_Int32s i, j;

	error = SmartSensor_GetShotModeCapability( iSensorId, eShotMode, pShotModeCap );
	ASSERT_CAM_ERROR( error );

	// XXX
	if ( pShotModeCap->iMinDigZoomQ16 == pShotModeCap->iMaxDigZoomQ16 && pShotModeCap->iMaxDigZoomQ16 == (1 << 16) )
	{
		pShotModeCap->iDigZoomStepQ16 = (CAM_Int32s)(0.2 * 65536 + 0.5);
	}
	if ( pShotModeCap->iMaxDigZoomQ16 < BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16 )
	{
		pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;
	}
	pShotModeCap->iMinDigZoomQ16 = ( 1 << 16 );
	pShotModeCap->iMaxDigZoomQ16 = BUILD_OPTION_STRATEGY_MAX_DIGITALZOOM_Q16;

	// add on ppu ZSL capability
	if ( BUILD_OPTION_STRATEGY_MAX_ZSL_DEPTH > 0 )
	{
		for ( i = 0; i < pShotModeCap->iSupportedStillSubModeCnt; i++ )
		{
			if ( pShotModeCap->eSupportedStillSubMode[i] == CAM_STILLSUBMODE_ZSL )
			{
				break;
			}
		}
		if ( i < pShotModeCap->iSupportedStillSubModeCnt )
		{
			TRACE( CAM_WARN, "Warning: sensor supports ZSL by itself, will directly use sensor's capability( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		}
		else
		{
			TRACE( CAM_WARN, "Warning: sensor doesn't support ZSL, will open cameraengine's S/W ZSL as required( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

			// update zsl capability
			pShotModeCap->eSupportedStillSubMode[i] = CAM_STILLSUBMODE_ZSL;
			pShotModeCap->iSupportedStillSubModeCnt++;
			pShotModeCap->iMaxZslDepth = BUILD_OPTION_STRATEGY_MAX_ZSL_DEPTH;

			for ( j = 0; j < pShotModeCap->iSupportedStillSubModeCnt; j++ )
			{
				if ( pShotModeCap->eSupportedStillSubMode[j] == CAM_STILLSUBMODE_BURST )
				{
					break;
				}
			}
			if ( j < pShotModeCap->iSupportedStillSubModeCnt )
			{
				// update burst + zsl capability
				pShotModeCap->eSupportedStillSubMode[ i + 1 ] = CAM_STILLSUBMODE_BURSTZSL;
				pShotModeCap->iSupportedStillSubModeCnt++;
			}
		}
	}

	return error;
}


// get buffer requirement of given port
static CAM_Error _get_port_bufreq( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBufferReq *pBufReq )
{
	CAM_Error              error          = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;

	// bad argument check
	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pBufReq );
	
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	if ( !_is_valid_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: for sensor[%d], port[%d] is invalid( %s, %s, %d )\n",
		       pCameraState->iSensorID, iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTVALID;
	}

	// the acceptable and optimal port buffer property depend on if HW or SW is used to generate the data
	if ( iPortId == CAM_PORT_PREVIEW )
	{
		error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_PREVIEW, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );
	}
	else if ( iPortId == CAM_PORT_VIDEO )
	{
		error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_VIDEO, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );
	}
	else if ( iPortId == CAM_PORT_STILL )
	{
		error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_STILL, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );
	}

	// don't use private buffer, get buffer requirement from sensor hal directly
	if ( stSensorConfig.iWidth == pCameraState->stPortState[iPortId].iWidth &&  stSensorConfig.iHeight == pCameraState->stPortState[iPortId].iHeight &&
	     stSensorConfig.eFormat == pCameraState->stPortState[iPortId].eFormat && pCameraState->stPortState[iPortId].eRotation == CAM_FLIPROTATE_NORMAL )
	{
		error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, pBufReq );
		ASSERT_CAM_ERROR( error );
	}
	else  // get buffer requirement from ppu
	{
		_CAM_ImageInfo stImgInfo;

		stImgInfo.iWidth       = pCameraState->stPortState[iPortId].iWidth;
		stImgInfo.iHeight      = pCameraState->stPortState[iPortId].iHeight;
		stImgInfo.eFormat      = pCameraState->stPortState[iPortId].eFormat;
		stImgInfo.eRotation    = pCameraState->stPortState[iPortId].eRotation;
		stImgInfo.bIsStreaming = CAM_TRUE;
		if ( stImgInfo.eFormat == CAM_IMGFMT_JPEG )
		{
			stImgInfo.pJpegParam = &pCameraState->stJpegParam;
		}
		else
		{
			stImgInfo.pJpegParam = NULL;
		}

		error = ppu_get_bufreq( pCameraState->hPpuHandle, &stImgInfo, pBufReq );
		ASSERT_CAM_ERROR( error );
	}

	return error;
}


// enqueue buffer to given port
static CAM_Error _enqueue_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBuffer *pImgBuf )
{
	CAM_Int32s         ret         = 0;
	CAM_Error          error       = CAM_ERROR_NONE;
	_CAM_PortState     *pPortState = NULL;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pImgBuf );

	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	if ( !_is_valid_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is invalid( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTVALID;
	}

	pPortState = &( pCameraState->stPortState[iPortId] );

	error = _check_image_buffer( pImgBuf, pPortState );
	if ( error != CAM_ERROR_NONE )
	{
		if ( error == CAM_WARN_DUPLICATEBUFFER )
		{
			TRACE( CAM_WARN, "Warning: same buffer was enqueued to port[%d], this enqueue will be taken as invalid to camera-engine( %s, %s, %d )\n",
			       iPortId, __FILE__, __FUNCTION__, __LINE__ );
		}
		return error;
	}

	/* enqueue buffer to the corresponding port
	 * whether to enqueue buffers to sensor depends on the port id and the current state
	 * sensor enqueue is triggered by preview port when the current state is preview
	 * sensor enqueue is triggered by video port when the current state is video
	 * sensor enqueue is triggered by still port when the current state is still
	 */
	ret = _EnQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: enqueue buffer to port[%d] failed( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFRESOURCE;
	}

	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && iPortId == CAM_PORT_PREVIEW )
	{
		if ( pCameraState->bNeedPostProcessing )
		{
			if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
			{
				// ZSL mode keeps fixed private buffer set after it's enabled and has its own buffer management,
				// so no need to request ppu buffers or enqueue buffer to sensor here.
				return CAM_ERROR_NONE;
			}

			CAM_ImageBuffer *pTmpImg = NULL;
			error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pImgBuf, &pTmpImg );
			if ( error != CAM_ERROR_NONE )
			{
				TRACE( CAM_ERROR, "Error: request private buffer failed( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

				ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
				ASSERT( ret == 0 );
			}
			else
			{
				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pTmpImg );

				if ( error != CAM_ERROR_NONE )
				{
					TRACE( CAM_ERROR, "Error: sensor hal enqueue failed( %s, %s, %d )\n",
					       __FILE__, __FUNCTION__, __LINE__ );

					ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
					ASSERT( ret == 0 );

					error = _release_ppu_srcimg( pCameraState, pTmpImg );
					ASSERT_CAM_ERROR( error );
				}
			}
		}
		else
		{
			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pImgBuf );

			if ( error != CAM_ERROR_NONE )
			{
				ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
				ASSERT( ret == 0 );
			}
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO && iPortId == CAM_PORT_VIDEO )
	{
		error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pImgBuf );
		ASSERT_CAM_ERROR( error );
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && iPortId == CAM_PORT_STILL )
	{
		// in still state, enqueue to driver only when we need more images
		if ( pCameraState->iStillRestCount > 0 )
		{
			if ( pCameraState->bNeedPostProcessing )
			{
				if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
				{
					// ZSL mode keeps fixed private buffer set after it's enabled and has its own buffer management,
					// so no need to request ppu buffers or enqueue buffer to sensor here.
					return CAM_ERROR_NONE;
				}

				CAM_ImageBuffer *pTmpImg = NULL;
				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pImgBuf, &pTmpImg );
				if ( error != CAM_ERROR_NONE )
				{
					TRACE( CAM_ERROR, "Error: request private buffer failed( %s, %s, %d )\n",
					       __FILE__, __FUNCTION__, __LINE__ );

					ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
					ASSERT( ret == 0 );
				}
				else
				{
					error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pTmpImg );

					if ( error != CAM_ERROR_NONE )
					{
						CAM_Error err;
						TRACE( CAM_ERROR, "Error: sensor hal enqueue failed( %s, %s, %d )\n",
						       __FILE__, __FUNCTION__, __LINE__ );

						ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
						ASSERT( ret == 0 );

						err = _release_ppu_srcimg( pCameraState, pTmpImg );
						ASSERT_CAM_ERROR( err );
					}
				}
			}
			else
			{
				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pImgBuf );
				if ( error != CAM_ERROR_NONE )
				{
					TRACE( CAM_ERROR, "Error: sensor hal enqueue failed( %s, %s, %d )\n",
					       __FILE__, __FUNCTION__, __LINE__ );

					ret = _RemoveQueue( &pCameraState->stPortState[iPortId].qEmpty, (void*)pImgBuf );
					ASSERT( ret == 0 );
				}
			}
		}
	}

	return error;
}

static CAM_Error _dequeue_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error  error   = CAM_ERROR_NONE;
	CAM_Int32s iPortId = *pPortId;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pPortId );
	_CHECK_BAD_POINTER( ppImgBuf );

	_CHECK_BAD_ANY_PORT_ID( *pPortId );

	// allow dequeue at IDLE state to get the images generated in previous dequeue
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	*ppImgBuf = NULL;

	error = _dequeue_filled( pCameraState, &iPortId, ppImgBuf );

	// if buffer is not ready, but port is active, so we need to run one frame from driver directly
	if ( error == CAM_ERROR_NONE && *ppImgBuf == NULL )
	{
		error = _run_one_frame( pCameraState );
		if ( error != CAM_ERROR_NONE )
		{
			return error;
		}

		iPortId = *pPortId ;
		error = _dequeue_filled( pCameraState, &iPortId, ppImgBuf );
		if ( error == CAM_ERROR_NONE && *ppImgBuf == NULL )
		{
			return CAM_ERROR_TIMEOUT;
		}
	}

	*pPortId = iPortId;

	return error;
}

static CAM_Error _flush_buffer( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Error  error = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_ANY_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	if ( iPortId == CAM_PORT_ANY )
	{
		CAM_Int32s i = 0;
		if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_IDLE && pCameraState->eCaptureState != CAM_CAPTURESTATE_NULL )
		{
			error = SmartSensor_Flush( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );

			if ( pCameraState->bNeedPostProcessing )
			{
				error = _flush_all_ppu_srcimg( pCameraState );
				ASSERT_CAM_ERROR( error );
			}
		}
		for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
		{
			_FlushQueue( &pCameraState->stPortState[i].qEmpty );
			_FlushQueue( &pCameraState->stPortState[i].qFilled );
		}
	}
	else
	{
		CAM_Bool bIsFlushSensor = ( ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && iPortId == CAM_PORT_PREVIEW ) &&
		                            !( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL ) ) ||
		                          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO   && iPortId == CAM_PORT_VIDEO ) ||
		                          ( ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL   && iPortId == CAM_PORT_STILL ) &&
		                            !( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL ) );

		if ( bIsFlushSensor )
		{
			error = SmartSensor_Flush( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );

			if ( pCameraState->bNeedPostProcessing )
			{
				error = _flush_all_ppu_srcimg( pCameraState );
				ASSERT_CAM_ERROR( error );
			}
		}

		_FlushQueue( &pCameraState->stPortState[iPortId].qEmpty );
		_FlushQueue( &pCameraState->stPortState[iPortId].qFilled );
	}

	return error;
}

static CAM_Error _set_port_size( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_Size *pSize )
{
	CAM_PortCapability *pPortCap = NULL;
	CAM_Error          error     = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pSize );

	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	if ( !_is_configurable_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is not configurable( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

	pPortCap = &pCameraState->stCameraCap.stPortCapability[iPortId];

	// check if this size is acceptable
	if ( pPortCap->bArbitrarySize )
	{
		if ( !( pSize->iWidth >= pPortCap->stMin.iWidth && pSize->iHeight >= pPortCap->stMin.iHeight &&
		        pSize->iWidth <= pPortCap->stMax.iWidth && pSize->iHeight <= pPortCap->stMax.iHeight )
		   )
		{
			TRACE( CAM_ERROR, "Error: image size(%d x %d) for port[%d] is not supported(%d x %d) - (%d x %d)( %s, %s, %d )\n",
			       pSize->iWidth, pSize->iHeight, iPortId,
			       pPortCap->stMin.iWidth, pPortCap->stMin.iHeight,
			       pPortCap->stMax.iWidth, pPortCap->stMax.iHeight,
			       __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDARG;
		}
	}
	else
	{
		CAM_Int32s i = 0;

		for ( i = 0; i < pPortCap->iSupportedSizeCnt; i++ )
		{
			if ( pSize->iWidth  == pPortCap->stSupportedSize[i].iWidth &&
			     pSize->iHeight == pPortCap->stSupportedSize[i].iHeight )
			{
				break;
			}
		}
		if ( i < pPortCap->iSupportedSizeCnt )
		{
			// not arbitrary size, video size should follow preview size 
			if ( iPortId == CAM_PORT_VIDEO && 
			     !( pCameraState->stCameraCap.stPortCapability[CAM_PORT_PREVIEW].bArbitrarySize ) &&
			     ( pSize->iWidth  != pCameraState->stPortState[CAM_PORT_PREVIEW].iWidth ||
			       pSize->iHeight != pCameraState->stPortState[CAM_PORT_PREVIEW].iHeight ) )
			{
				TRACE( CAM_ERROR, "Error: image size(%d x %d) for video port must be aligned with preview port size(%d x %d), "
				       "engine made this assumption to keep better performance( %s, %s, %d )\n",
				       pSize->iWidth, pSize->iHeight,
				       pCameraState->stPortState[CAM_PORT_PREVIEW].iWidth,
				       pCameraState->stPortState[CAM_PORT_PREVIEW].iHeight,
				       __FILE__, __FUNCTION__, __LINE__ );
				error = CAM_ERROR_NOTSUPPORTEDARG;

			}
		}
		else
		{
			TRACE( CAM_ERROR, "Error: image size(%d x %d) for port[%d] is not supported( %d options )( %s, %s, %d )\n",
			       pSize->iWidth, pSize->iHeight, iPortId, pPortCap->iSupportedSizeCnt, __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDARG;
		}
	}

	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	// if S/W ZSL is on, and the still port size is changed, need to reconfigure sensor to output frames in new size
	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && iPortId == CAM_PORT_STILL &&
	   ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL ) &&
	   ( pCameraState->stPortState[iPortId].iWidth != pSize->iWidth || pCameraState->stPortState[iPortId].iHeight != pSize->iHeight ) )
	{
		_lock_port( pCameraState, CAM_PORT_PREVIEW );
		error = _reset_zsl_preview( pCameraState, pSize );
		ASSERT_CAM_ERROR( error );
		_unlock_port( pCameraState, CAM_PORT_PREVIEW );
	}

	pCameraState->stPortState[iPortId].iWidth  = pSize->iWidth;
	pCameraState->stPortState[iPortId].iHeight = pSize->iHeight;

	error = _get_port_bufreq( pCameraState, iPortId, &pCameraState->stPortState[iPortId].stBufReq );
	ASSERT_CAM_ERROR( error );

	return error;
}

static CAM_Error _set_port_format( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageFormat eFormat )
{
	CAM_PortCapability *pPortCap = NULL;
	CAM_Error          error     = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	// check if supported
	pPortCap = &pCameraState->stCameraCap.stPortCapability[iPortId];
	_CHECK_SUPPORT_ENUM( eFormat, pPortCap->eSupportedFormat, pPortCap->iSupportedFormatCnt );

	if ( !_is_configurable_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is not configurable( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

    /* if (iPortId == CAM_PORT_STILL) */
        pCameraState->stPortState[iPortId].eFormat = eFormat;
    /* else */
        /* pCameraState->stPortState[iPortId].eFormat = CAM_IMGFMT_RGBA8888; */

	// update the port buffer requirement
	error = _get_port_bufreq( pCameraState, iPortId, &pCameraState->stPortState[iPortId].stBufReq );
	ASSERT_CAM_ERROR( error );

	return error;
}

static CAM_Error _set_port_rotation( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId, CAM_FlipRotate eRotation )
{
	CAM_PortCapability *pPortCap = NULL;
	CAM_Error          error     = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

	// check if supported
	pPortCap = &pCameraState->stCameraCap.stPortCapability[iPortId];
	_CHECK_SUPPORT_ENUM( eRotation, pPortCap->eSupportedRotate, pPortCap->iSupportedRotateCnt );

	if ( !_is_configurable_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is not configurable( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

	pCameraState->stPortState[iPortId].eRotation = eRotation;

	// update the port buffer requirement
	error = _get_port_bufreq( pCameraState, iPortId, &pCameraState->stPortState[iPortId].stBufReq );
	ASSERT_CAM_ERROR( error );

	return error;
}

static CAM_Error _set_jpeg_param( _CAM_ExtIspState *pCameraState, CAM_JpegParam *pJpegParam )
{
	CAM_Error error = CAM_ERROR_NONE;

	// bad argument check
	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pJpegParam );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

	if ( !_is_configurable_port( pCameraState, CAM_PORT_STILL ) )
	{
		TRACE( CAM_ERROR, "Error: still port is not configurable( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

	if ( pJpegParam->bEnableExif && pCameraState->stCameraCap.stSupportedJPEGParams.bSupportExif == CAM_FALSE )
	{
		TRACE( CAM_ERROR, "Error: EXIF is not supported by current sensor( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
	else if ( pJpegParam->bEnableThumb && pCameraState->stCameraCap.stSupportedJPEGParams.bSupportThumb == CAM_FALSE )
	{
		TRACE( CAM_ERROR, "Error: JPEG thumbnail is not supported by current sensor( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
	else if ( pJpegParam->iQualityFactor < pCameraState->stCameraCap.stSupportedJPEGParams.iMinQualityFactor||
	          pJpegParam->iQualityFactor > pCameraState->stCameraCap.stSupportedJPEGParams.iMaxQualityFactor )
	{
		TRACE( CAM_ERROR, "Error: JPEG quality[%d] must be in the range[%d, %d]( %s, %s, %d )\n", pJpegParam->iQualityFactor,
		       pCameraState->stCameraCap.stSupportedJPEGParams.iMinQualityFactor,
		       pCameraState->stCameraCap.stSupportedJPEGParams.iMaxQualityFactor,
		       __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
	else
	{
		pCameraState->stJpegParam = *pJpegParam;

		// update the port buffer requirement
		error = _get_port_bufreq( pCameraState, (CAM_Int32s)CAM_PORT_STILL, &pCameraState->stPortState[CAM_PORT_STILL].stBufReq );
		ASSERT_CAM_ERROR( error );
	}

	return error;
}

static CAM_Error _set_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Int32s iDigitalZoomQ16 )
{
	CAM_Error              error   = CAM_ERROR_NONE;
	CAM_Int32s             iPortId = -1;
	CAM_Int32s             i       = 0;
	CAM_Int32s             iSensorDigitalZoomQ16, iPpuDigitalZoomQ16;
	CAM_Bool               bNeedPostProcessing, bUsePrivateBuffer;
	CAM_Bool               bNeedReplaceSensorBuf;
	_CAM_SmartSensorConfig stSensorConfig;
	CAM_Size               stSize = { 0, 0 };
	_CAM_PostProcessParam  stPpuParamSettings = pCameraState->stPpuShotParam;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );

	switch ( pCameraState->eCaptureState )
	{
	case CAM_CAPTURESTATE_IDLE:
		pCameraState->stShotParam.iDigZoomQ16 = iDigitalZoomQ16;
		break;

	case CAM_CAPTURESTATE_PREVIEW:
	case CAM_CAPTURESTATE_VIDEO:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
		{
			iPortId = CAM_PORT_PREVIEW;
		}
		else
		{
			iPortId = CAM_PORT_VIDEO;
		}

		error = _get_required_sensor_settings( pCameraState, pCameraState->eCaptureState, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		stSize.iWidth  = stSensorConfig.iWidth;
		stSize.iHeight = stSensorConfig.iHeight;

		error = _calc_digital_zoom( pCameraState, &stSize, iDigitalZoomQ16, &iSensorDigitalZoomQ16, &iPpuDigitalZoomQ16 );
		ASSERT_CAM_ERROR( error );

		stPpuParamSettings.iPpuDigitalZoomQ16 = iPpuDigitalZoomQ16;
		error = _is_need_ppu( &(pCameraState->stPortState[iPortId]), &stSensorConfig, &stPpuParamSettings,
		                      &bNeedPostProcessing, &bUsePrivateBuffer );
		ASSERT_CAM_ERROR( error );

		bNeedReplaceSensorBuf = ( ( pCameraState->bNeedPostProcessing == CAM_FALSE && bNeedPostProcessing == CAM_TRUE ) ||
		                          ( pCameraState->bNeedPostProcessing == CAM_TRUE && bNeedPostProcessing == CAM_FALSE ) ||
		                          ( pCameraState->bNeedPostProcessing == CAM_TRUE && bNeedPostProcessing == CAM_TRUE && pCameraState->bUsePrivateBuffer != bUsePrivateBuffer )
		                        );

		if ( bNeedReplaceSensorBuf )
		{
			error = SmartSensor_Flush( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );

			if ( pCameraState->bNeedPostProcessing )
			{
				error = _flush_all_ppu_srcimg( pCameraState );
				ASSERT_CAM_ERROR( error );
			}

			// update bNeedPostProcessing, bUsePrivateBuffer
			pCameraState->bNeedPostProcessing = bNeedPostProcessing;
			pCameraState->bUsePrivateBuffer   = bUsePrivateBuffer;

			for ( i = 0; i < pCameraState->stPortState[iPortId].qEmpty.iQueueCount; i++ )
			{
				CAM_Int32s      ret         = 0;
				CAM_ImageBuffer *pUsrBuf    = NULL;
				CAM_ImageBuffer *pSensorBuf = NULL;

				ret = _GetQueue( &pCameraState->stPortState[iPortId].qEmpty, i, (void**)&pUsrBuf );
				ASSERT( ret == 0 );

				if ( pCameraState->bNeedPostProcessing )
				{
					TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
					error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
					ASSERT_CAM_ERROR( error );
				}
				else
				{
					pSensorBuf = pUsrBuf;
					TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in preview state!\n" );
				}

				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
		}

		error = _set_sensor_digital_zoom( pCameraState->hSensorHandle, iSensorDigitalZoomQ16 );
		ASSERT_CAM_ERROR( error );

		pCameraState->stShotParam.iDigZoomQ16           = iDigitalZoomQ16;
		pCameraState->stPpuShotParam.iPpuDigitalZoomQ16 = iPpuDigitalZoomQ16;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	return error;
}

static CAM_Error _convert_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Size *pSize, CAM_CaptureState eToState )
{
	CAM_Error  error  = CAM_ERROR_NONE;
	CAM_Int32s iSensorDigitalZoomQ16, iPpuDigitalZoomQ16;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pSize );

	error = _calc_digital_zoom( pCameraState, pSize, pCameraState->stShotParam.iDigZoomQ16, &iSensorDigitalZoomQ16, &iPpuDigitalZoomQ16 );
	ASSERT_CAM_ERROR( error );

#if 0
	if ( ( eToState == CAM_CAPTURESTATE_PREVIEW || eToState == CAM_CAPTURESTATE_VIDEO ) &&
	      iPpuDigitalZoomQ16 > ( 1 << 16 ) )
	{
		TRACE( CAM_ERROR, "Error: Currently we do not support PPU digital zoom in preview/video state for performance consideration!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NOTSUPPORTEDARG;
	}
#endif

	error = _set_sensor_digital_zoom( pCameraState->hSensorHandle, iSensorDigitalZoomQ16 );
	ASSERT_CAM_ERROR( error );

	pCameraState->stPpuShotParam.iPpuDigitalZoomQ16 = iPpuDigitalZoomQ16;

	return error;
}

static CAM_Error _reset_camera( _CAM_ExtIspState *pCameraState, CAM_Int32s iResetType )
{
	CAM_Int32s             i = 0;
	CAM_Size               stSize = { 0, 0 };
	CAM_Error              error  = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;
	CAM_Int32s             iPortId;

	// current port
	switch ( pCameraState->eCaptureState )
	{
	case CAM_CAPTURESTATE_IDLE:
	case CAM_CAPTURESTATE_STANDBY:
	case CAM_CAPTURESTATE_NULL:
		TRACE( CAM_ERROR, "Error: cannot reset sensor in state[%d]( %s, %s, %d )\n", pCameraState->eCaptureState, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_STATELIMIT;
		break;

	case CAM_CAPTURESTATE_PREVIEW:
		iPortId = CAM_PORT_PREVIEW;
		break;

	case CAM_CAPTURESTATE_STILL:
		iPortId = CAM_PORT_STILL;
		break;

	case CAM_CAPTURESTATE_VIDEO:
		iPortId = CAM_PORT_VIDEO;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	_lock_port( pCameraState, CAM_PORT_ANY );

	// get sensor settings
	error = _get_required_sensor_settings( pCameraState, pCameraState->eCaptureState, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// send config to sensor
	stSensorConfig.iResetType = iResetType;
	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// convert digital zoom
	stSize.iWidth  = stSensorConfig.iWidth;
	stSize.iHeight = stSensorConfig.iHeight;
	error = _convert_digital_zoom( pCameraState, &stSize, pCameraState->eCaptureState );
	ASSERT_CAM_ERROR( error );

	// tag all ppu src buffers as unusing
	if ( pCameraState->bNeedPostProcessing )
	{
		for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
		{
			pCameraState->bPpuSrcImgUsed[i] = CAM_FALSE;
		}
	}

	for ( i = 0; i < pCameraState->stPortState[iPortId].qEmpty.iQueueCount; i++ )
	{
		CAM_Int32s      ret         = 0;
		CAM_ImageBuffer *pUsrBuf    = NULL;
		CAM_ImageBuffer *pSensorBuf = NULL;

		ret = _GetQueue( &pCameraState->stPortState[iPortId].qEmpty, i, (void**)&pUsrBuf );
		ASSERT( ret == 0 );

		if ( pCameraState->bNeedPostProcessing )
		{
			error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
			ASSERT_CAM_ERROR( error );
		}
		else
		{
			pSensorBuf = pUsrBuf;
		}

		error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
		ASSERT_CAM_ERROR( error );
	}

	_unlock_port( pCameraState, CAM_PORT_ANY );

	return CAM_ERROR_NONE;
}

static CAM_Error _start_face_detection( _CAM_ExtIspState *pCameraState )
{
	CAM_Error error  = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );

	if ( pCameraState->stCameraCap.iMaxFaceNum <= 0 )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( pCameraState->bIsFaceDetectionOpen == CAM_FALSE )
	{
		error = SmartSensor_StartFaceDetection( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );

		pCameraState->bIsFaceDetectionOpen = CAM_TRUE;
	}
	else
	{
		TRACE( CAM_INFO, "Info: face detection is already on( %s, %d )\n", __FILE__, __LINE__ );
		error = CAM_ERROR_NONE;
	}

	return error;
}

static CAM_Error _cancel_face_detection( _CAM_ExtIspState *pCameraState )
{
	CAM_Error error  = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );

	if ( pCameraState->stCameraCap.iMaxFaceNum <= 0 )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( pCameraState->bIsFaceDetectionOpen == CAM_TRUE )
	{
		error = SmartSensor_CancelFaceDetection( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );

		pCameraState->bIsFaceDetectionOpen = CAM_FALSE;
	}
	else
	{
		TRACE( CAM_INFO, "Info: face detection is already off( %s, %d )\n", __FILE__, __LINE__ );
		error = CAM_ERROR_NONE;
	}

	return error;
}

static CAM_Error _set_camera_state( _CAM_ExtIspState *pCameraState, CAM_CaptureState eToState )
{
	CAM_Error error = CAM_ERROR_NONE;

	// BAC check
	_CHECK_BAD_POINTER( pCameraState );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY );

	if ( eToState == pCameraState->eCaptureState )
	{
		CELOG( "Camera Engine already in state[%d], will do nothing\n", eToState );
		return CAM_ERROR_NONE;
	}

	switch ( eToState )
	{
	case CAM_CAPTURESTATE_NULL:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
		{
			CELOG( "Idle --> Null ...\n" );
			error = _idle2null( pCameraState );
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	case CAM_CAPTURESTATE_STANDBY:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
		{
			error = CAM_ERROR_NOTIMPLEMENTED;
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	case CAM_CAPTURESTATE_IDLE:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
		{
			CELOG( "Preview --> Idle ...\n" );
			error = _preview2idle( pCameraState );
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	case CAM_CAPTURESTATE_PREVIEW:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
		{
			CELOG( "Idle --> Preview ...\n" );
			error = _idle2preview( pCameraState );
		}
		else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL )
		{
			CELOG( "Still --> Preview ...\n" );
			error = _still2preview( pCameraState );
		}
		else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO )
		{
			CELOG( "Video --> Preview ...\n" );
			error = _video2preview( pCameraState );
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	case CAM_CAPTURESTATE_STILL:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
		{
			CELOG( "Preview --> Still ...\n" );
			error = _preview2still( pCameraState );
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	case CAM_CAPTURESTATE_VIDEO:
		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
		{
			CELOG( "Preview --> Video ...\n" );
			error = _preview2video( pCameraState );
		}
		else
		{
			error = CAM_ERROR_BADSTATETRANSITION;
		}
		break;

	default:
		TRACE( CAM_ERROR, "Error: State Transition Failed( %d -> %d): Invalid Dst State( %s, %s, %d )\n", pCameraState->eCaptureState, eToState, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_BADARGUMENT;
		break;
	}

	if ( error == CAM_ERROR_NONE )
	{
		CELOG( "State Transition Done\n" );
	}
	else if ( error == CAM_ERROR_BADSTATETRANSITION )
	{
		TRACE( CAM_ERROR, "Error: State Transition Failed( %d -> %d ): Unsupported State Transition( %s, %s, %d )\n",
		       pCameraState->eCaptureState, eToState, __FILE__, __FUNCTION__, __LINE__ );
	}
	else
	{
		TRACE( CAM_ERROR, "Error: State Transition Failed( %d -> %d ): error code[%d]( %s, %s, %d )\n", pCameraState->eCaptureState, eToState, error, __FILE__, __FUNCTION__, __LINE__ );
	}

	return error;
}



static CAM_Bool _is_valid_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId )
{
	// TODO: check the port consistency, here

	return CAM_TRUE;
}


static CAM_Bool _is_configurable_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId )
{
	// configurable port must be valid and must has no buffer

	CAM_Bool bCfg = CAM_TRUE;

	bCfg = ( pCameraState->stPortState[iPortId].qEmpty.iQueueCount + pCameraState->stPortState[iPortId].qFilled.iQueueCount == 0 );
	
	return bCfg;
}


/******************************************************************************************************
//
// Name:         _dequeue_filled
// Description:  dequeue an existing filled image according to the given port id
// Arguments:    pCameraState[IN] : camera state
//               pPortId [IN/OUT] : port id
//               ppImgBuf  [OUT]  : pointer to the dequeued buffer
// Return:       None
// Notes:        Three possible results:
//               1. The given port is not active and has no filled buffer, return NULL with "port not active" error code;
//               2. The given port has buffer, return the buffer with no error code, regardless if the port is active or not;
//               3. The given port is active and has no filled buffer, return NULL with no error.
// Version Log:  version      Date          Author     Description
 *******************************************************************************************************/
static CAM_Error _dequeue_filled( _CAM_ExtIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error  error = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pPortId );
	_CHECK_BAD_POINTER( ppImgBuf );
	_CHECK_BAD_ANY_PORT_ID( *pPortId );

	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_IDLE & ~CAM_CAPTURESTATE_NULL );

	*ppImgBuf = NULL;

	if ( *pPortId == CAM_PORT_PREVIEW || *pPortId == CAM_PORT_VIDEO || *pPortId == CAM_PORT_STILL )
	{
		if ( 0 == _DeQueue( &pCameraState->stPortState[*pPortId].qFilled, (void**)ppImgBuf ) )
		{
			// got a buffer, and return no error;
			error = CAM_ERROR_NONE;
		}
		else if ( ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && *pPortId != CAM_PORT_PREVIEW ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO && *pPortId == CAM_PORT_STILL ) ||
#if defined( BUILD_OPTION_STRATEGY_ENABLE_SNAPSHOT )
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId == CAM_PORT_VIDEO ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId != CAM_PORT_VIDEO && pCameraState->iStillRestCount <= 0 )
#else
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId != CAM_PORT_STILL ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId == CAM_PORT_STILL && pCameraState->iStillRestCount <= 0 )
#endif
		        )
		{
			// got no buffer, and return port not active error code
			TRACE( CAM_ERROR, "Error: port[%d] is not active in state[%d]( %s, %s, %d )\n", *pPortId, pCameraState->eCaptureState,
			       __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_PORTNOTACTIVE;
		}
		else
		{
			// got no buffer, and return no error
			error = CAM_ERROR_NONE;
		}
	}
	else if ( *pPortId == CAM_PORT_ANY )
	{
		if ( 0 == _DeQueue( &pCameraState->stPortState[CAM_PORT_STILL].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_STILL;
			error    = CAM_ERROR_NONE;
		}
		else if ( 0 == _DeQueue( &pCameraState->stPortState[CAM_PORT_VIDEO].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_VIDEO;
			error    = CAM_ERROR_NONE;
		}
		else if ( 0 == _DeQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_PREVIEW;
			error    = CAM_ERROR_NONE;
		}
		else
		{
			*ppImgBuf = NULL;
			error     = CAM_ERROR_NONE;
		}
	}

	return error;
}

static CAM_Error _run_one_frame( _CAM_ExtIspState *pCameraState )
{
	CAM_Error error = CAM_ERROR_NONE;

	ASSERT( pCameraState != NULL );

	switch ( pCameraState->eCaptureState )
	{
	case CAM_CAPTURESTATE_NULL:
	case CAM_CAPTURESTATE_STANDBY:
	case CAM_CAPTURESTATE_IDLE:
		error = CAM_ERROR_PORTNOTACTIVE;
		break;

	case CAM_CAPTURESTATE_PREVIEW:
		error = _process_previewstate_buffer( pCameraState );
		break;

	case CAM_CAPTURESTATE_VIDEO:
		error = _process_videostate_buffer( pCameraState );
		break;

	case CAM_CAPTURESTATE_STILL:
		error = _process_stillstate_buffer( pCameraState );
		break;

	default:
		ASSERT( 0 );
		break;
	}

	return error;
}

static CAM_Error _process_previewstate_buffer( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s      ret         = 0;
	CAM_Error       error       = CAM_ERROR_NONE;
	CAM_ImageBuffer *pSensorImg = NULL;
	CAM_ImageBuffer *pPortImg   = NULL;

	_CAM_PortState  *pPreviewPort = &pCameraState->stPortState[CAM_PORT_PREVIEW];

	_lock_port( pCameraState, CAM_PORT_PREVIEW );

	if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_PREVIEW )
	{
		// TRACE( CAM_ERROR, "Error: current state[%d] is not preview state( %s, %s, %d )\n", pCameraState->eCaptureState, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_STATELIMIT;
		goto exit;
	}

	if ( pPreviewPort->qEmpty.iQueueCount + pPreviewPort->qFilled.iQueueCount <= pPreviewPort->stBufReq.iMinBufCount )
	{
		TRACE( CAM_WARN, "Warning: there is no enough buffers for preview port, ProcessBufferThread enter waiting stage "
		       "unless new buffer is enqueued. Pls check: 1. whether enough buffers are allocated; "
		       "2. which component of the pipeline is blocking the buffer flow\n" );
		error = CAM_ERROR_NOTENOUGHBUFFERS;
		goto exit;
	}

	// overwrite filled image when there's no enough number of empty buffers
	// we must have enough empty buffers on preview port to start the processing
	while ( pPreviewPort->qEmpty.iQueueCount <= pPreviewPort->stBufReq.iMinBufCount )
	{
		CAM_ImageBuffer *pTmpImg = NULL;

		// if there's no empty buffer, we try to overwrite a filled buffer
		ret = _DeQueue( &pPreviewPort->qFilled, (void**)&pTmpImg );
		ASSERT( ret == 0 );

		TRACE( CAM_WARN, "Warning: overwrite one filled buffer in preview port, pls check why enqueue speed is slow( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		// queue as empty buffer
		error = _enqueue_buffer( pCameraState, CAM_PORT_PREVIEW, pTmpImg );
		ASSERT_CAM_ERROR( error );

		// inform the frame loss
		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_PREVIEW );
	}

	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL &&
	     pCameraState->qPpuBufferRing.iQueueCount == pCameraState->qPpuBufferRing.iDataNum )
	{
		// enqueue outdated ring buffer back to sensor right after dequeue one from sensor
		CAM_ImageBuffer *pTmpBuf = NULL;

		ret = _DeQueue( &pCameraState->qPpuBufferRing, (void**)&pTmpBuf );
		ASSERT( ret == 0 );

		error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pTmpBuf );
		ASSERT_CAM_ERROR( error );
	}

	// get image buffer from sensor
	error = SmartSensor_Dequeue( pCameraState->hSensorHandle, &pSensorImg );
	if ( error != CAM_ERROR_NONE )
	{
		goto exit;
	}

	if ( pCameraState->bNeedPostProcessing )
	{
		_CAM_PostProcessParam stPostProcessParam;

		stPostProcessParam.bFavorAspectRatio  = CAM_FALSE;
		stPostProcessParam.iPpuDigitalZoomQ16 = pCameraState->stPpuShotParam.iPpuDigitalZoomQ16;
		stPostProcessParam.eRotation          = pPreviewPort->eRotation;
		stPostProcessParam.pJpegParam         = NULL;
		stPostProcessParam.pUsrData           = NULL;
		// whether in-place or not
		stPostProcessParam.bIsInPlace         = ( ( pCameraState->bUsePrivateBuffer == CAM_FALSE ) ? CAM_TRUE : CAM_FALSE );

		// get the preview buffer
		if ( stPostProcessParam.bIsInPlace )
		{
			ret = _RemoveQueue( &pPreviewPort->qEmpty, pSensorImg->pUserData );
			ASSERT( ret == 0 );

			pPortImg = (CAM_ImageBuffer*)( pSensorImg->pUserData );
		}
		else
		{
			ret = _DeQueue( &pPreviewPort->qEmpty, (void**)&pPortImg );
			ASSERT( ret == 0 && pPortImg != NULL );
		}

#if defined( CAM_PERF )
		{
			CAM_Tick t = -CAM_TimeGetTickCount();
#endif
			// image resizing & rotate to generate the preview buffer
			error = ppu_image_process( pCameraState->hPpuHandle, pSensorImg, pPortImg, &stPostProcessParam );
			ASSERT_CAM_ERROR( error );

#if defined( CAM_PERF )
			t += CAM_TimeGetTickCount();
			CELOG( "Perf: format conversion[%d -> %d], resize[(%d * %d) -> (%d * %d)], digzoom[%.2f], rotation[%d] cost %.2f ms\n",
			       pSensorImg->eFormat,  pPortImg->eFormat,
			       pSensorImg->iWidth,   pSensorImg->iHeight,
			       pPreviewPort->iWidth, pPreviewPort->iHeight,
			       pCameraState->stPpuShotParam.iPpuDigitalZoomQ16 / 65536.0,
			       pPreviewPort->eRotation,
			       t / 1000.0 );
		}
#endif
		if ( pPortImg->bEnableShotInfo )
		{
			pPortImg->stShotInfo = pSensorImg->stShotInfo;
		}

		pPortImg->tTick = pSensorImg->tTick;

		if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
		{
			// enqueue sensor buffer to ring
			ret = _EnQueue( &pCameraState->qPpuBufferRing, pSensorImg );
			ASSERT( ret == 0 );
		}
		else
		{
			// release the ppu src image( _request_ppu_srcimg called by _enqueue_buffer )
			error = _release_ppu_srcimg( pCameraState, pSensorImg );
			ASSERT_CAM_ERROR( error );
		}

		ret = _EnQueue( &pPreviewPort->qFilled, pPortImg );
		ASSERT( ret == 0 );
	}
	else
	{
		// remove from the empty queue
		ret = _RemoveQueue( &pPreviewPort->qEmpty, pSensorImg );
		ASSERT( ret == 0 );

		// put to the filled queue
		ret = _EnQueue( &pPreviewPort->qFilled, pSensorImg );
		ASSERT( ret == 0 );
	}

	SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, CAM_PORT_PREVIEW );

exit:
	_unlock_port( pCameraState, CAM_PORT_PREVIEW );
	return error;
}


static CAM_Error _process_videostate_buffer( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s      ret         = 0;
	CAM_Error       error       = CAM_ERROR_NONE;
	CAM_ImageBuffer *pSensorImg = NULL;
	CAM_ImageBuffer *pPortImg   = NULL;

	_CAM_PortState  *pPreviewPort = &pCameraState->stPortState[CAM_PORT_PREVIEW];
	_CAM_PortState  *pVideoPort   = &pCameraState->stPortState[CAM_PORT_VIDEO];

	_lock_port( pCameraState, CAM_PORT_VIDEO );

	if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_VIDEO )
	{
		// TRACE( CAM_ERROR, "Error: current state[%d] is not video state( %s, %s, %d )\n", pCameraState->eCaptureState, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_STATELIMIT;
		goto exit;
	}

	if ( pVideoPort->qEmpty.iQueueCount + pVideoPort->qFilled.iQueueCount <= pVideoPort->stBufReq.iMinBufCount )
	{
		//TRACE( CAM_WARN, "Warning: there is no enough buffers for preview port, ProcessBufferThread enter waiting stage "
		//       "unless new buffer is enqueued. Pls check: 1. whether enough buffers are allocated; "
		//       "2. which component of the pipeline is blocking the buffer flow\n" );
		error = CAM_ERROR_NOTENOUGHBUFFERS;
		goto exit;
	}

	// we must have enough empty buffers on video port to start the processing
	while ( pVideoPort->qEmpty.iQueueCount <= pVideoPort->stBufReq.iMinBufCount )
	{
		CAM_ImageBuffer *pTmpImg = NULL;

		// if there's no empty buffer, we try to overwrite a filled buffer
		ret = _DeQueue( &pVideoPort->qFilled, (void**)&pTmpImg );
		ASSERT( ret == 0 )

		TRACE( CAM_WARN, "Warning: overwrite one filled buffer in video port, pls check why enqueue speed is slow( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

		error = _enqueue_buffer( pCameraState, CAM_PORT_VIDEO, pTmpImg );
		ASSERT_CAM_ERROR( error );

		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_VIDEO );
	}

	// get sensor buffer
	error = SmartSensor_Dequeue( pCameraState->hSensorHandle, &pSensorImg );
	if ( error != CAM_ERROR_NONE ) 
	{
		goto exit;
	}

	_lock_port( pCameraState, CAM_PORT_PREVIEW );

	// try to process the preview image
	if ( _DeQueue( &pPreviewPort->qEmpty, (void**)&pPortImg ) == -1 )
	{
		// anyway, one frame will drop form preview port
		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_PREVIEW );
		if ( _DeQueue( &pPreviewPort->qFilled, (void**)&pPortImg ) == -1 )
		{
			pPortImg = NULL;
		}
	}

	if ( pPortImg != NULL )
	{
#if defined( CAM_PERF )
		CAM_Tick t = -CAM_TimeGetTickCount();
#endif
		_CAM_PostProcessParam stPostProcessParam;

		stPostProcessParam.bFavorAspectRatio  = pCameraState->bPreviewFavorAspectRatio;
		stPostProcessParam.iPpuDigitalZoomQ16 = pCameraState->stPpuShotParam.iPpuDigitalZoomQ16;
		stPostProcessParam.eRotation          = pPreviewPort->eRotation;
		stPostProcessParam.pJpegParam         = NULL;
		stPostProcessParam.pUsrData           = NULL;
		stPostProcessParam.bIsInPlace         = CAM_FALSE;

		// image resizing & rotate to generate the preview buffer
		error = ppu_image_process( pCameraState->hPpuHandle, pSensorImg, pPortImg, &stPostProcessParam );
		ASSERT_CAM_ERROR( error );

#if defined( CAM_PERF )
		t += CAM_TimeGetTickCount();
		CELOG( "Perf: format conversion[%d -> %d], resize[(%d * %d) -> (%d * %d)], rotation[%d] cost %.2f ms\n",
		       pSensorImg->eFormat,  pPortImg->eFormat,
		       pSensorImg->iWidth,   pSensorImg->iHeight,
		       pPreviewPort->iWidth, pPreviewPort->iHeight,
		       pPreviewPort->eRotation,
		       t / 1000.0 );
#endif
		if ( pPortImg->bEnableShotInfo )
		{
			pPortImg->stShotInfo = pSensorImg->stShotInfo;
		}

		pPortImg->tTick = pSensorImg->tTick;

		ret = _EnQueue( &pPreviewPort->qFilled, (void*)pPortImg );
		ASSERT( ret == 0 );

		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, CAM_PORT_PREVIEW );
	}

	_unlock_port( pCameraState, CAM_PORT_PREVIEW );

	// remove from the input queue and put to the output queue
	ret = _RemoveQueue( &pVideoPort->qEmpty, (void*)pSensorImg );
	ASSERT( ret == 0 );

	ret = _EnQueue( &pVideoPort->qFilled, (void*)pSensorImg );
	ASSERT( ret == 0 );

	SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, CAM_PORT_VIDEO );

exit:
	_unlock_port( pCameraState, CAM_PORT_VIDEO );
	return error;
}

static CAM_Error _process_stillstate_buffer( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s      ret         = 0;
	CAM_Error       error       = CAM_ERROR_NONE;
	CAM_ImageBuffer *pSensorImg = NULL;
	CAM_ImageBuffer *pPortImg   = NULL;

	_CAM_PortState  *pPreviewPort = &pCameraState->stPortState[CAM_PORT_PREVIEW];
	_CAM_PortState  *pStillPort   = &pCameraState->stPortState[CAM_PORT_STILL];

	_lock_port( pCameraState, CAM_PORT_STILL );

	if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_STILL )
	{
		// TRACE( CAM_ERROR, "Error: current state[%d] is not still state( %s, %s, %d )\n", pCameraState->eCaptureState, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_STATELIMIT;
		goto exit;
	}

	if ( pCameraState->iStillRestCount <= 0 )
	{
		// if ZSL enabled, will keep updating ring buffer even no buffer needs to be dequeued
		if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
		{
			error = SmartSensor_Dequeue( pCameraState->hSensorHandle, &pSensorImg );
			if ( error != CAM_ERROR_NONE )
			{
				goto exit;
			}

			if ( pCameraState->qPpuBufferRing.iQueueCount == pCameraState->qPpuBufferRing.iDataNum )
			{
				CAM_ImageBuffer *pTmpBuf = NULL;

				ret = _DeQueue( &pCameraState->qPpuBufferRing, (void**)&pTmpBuf );
				ASSERT( ret == 0 );

				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pTmpBuf );
				ASSERT_CAM_ERROR( error );
			}

			ret = _EnQueue( &pCameraState->qPpuBufferRing, pSensorImg );
			ASSERT( ret == 0 );
		}
		else
		{
			error = CAM_ERROR_PORTNOTACTIVE;
		}

		goto exit;
	}

	// we don't overwrite filled still image data if there's no enough empty buffer

	CAM_PROFILE( pCameraState->eCaptureState, CAM_CAPTURESTATE_STILL, CAM_PERF_INTERVAL, "CAM_PERF: start to dequeue" );
	// will try to dequeue from buffer ring firstly if ZSL is on
	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
	{
		if ( pCameraState->qPpuBufferRing.iQueueCount > 0 )
		{
			// get buffer from ring
			ret = _DeQueue( &pCameraState->qPpuBufferRing, (void**)&pSensorImg );
			ASSERT( ret == 0 );
		}
		else
		{
			TRACE( CAM_WARN, "Warning: ZSL ring out of data, will directly dequeue from sensor( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

			error = SmartSensor_Dequeue( pCameraState->hSensorHandle, &pSensorImg );
			if ( error != CAM_ERROR_NONE )
			{
				goto exit;
			}
		}
	}
	else
	{
		// get buffer from sensor
		error = SmartSensor_Dequeue( pCameraState->hSensorHandle, &pSensorImg );
		if ( error != CAM_ERROR_NONE )
		{
			goto exit;
		}
	}

	SEND_EVENT( pCameraState, CAM_EVENT_STILL_SHUTTERCLOSED, 0 );
	CAM_PROFILE( pCameraState->eCaptureState, CAM_CAPTURESTATE_STILL, CAM_PERF_INTERVAL, "CAM_PERF: dequeue buffer time" );
	CAM_PROFILE( pCameraState->eCaptureState, CAM_CAPTURESTATE_STILL, CAM_PERF_OVERALL_END, "CAM_PERF: shutter release time end" );

	// try to process the snapshot image if feature opened
#if defined( BUILD_OPTION_STRATEGY_ENABLE_SNAPSHOT )
	_lock_port( pCameraState, CAM_PORT_PREVIEW );
	if ( 0 == _DeQueue( &pPreviewPort->qEmpty, (void**)&pPortImg ) )
	{
#if defined( CAM_PERF )
		CAM_Tick t = -CAM_TimeGetTickCount();
#endif

		_CAM_PostProcessParam stPostProcessParam;

		stPostProcessParam.bFavorAspectRatio  = pCameraState->bPreviewFavorAspectRatio;
		stPostProcessParam.iPpuDigitalZoomQ16 = pCameraState->stPpuShotParam.iPpuDigitalZoomQ16;
		stPostProcessParam.eRotation          = CAM_FLIPROTATE_NORMAL;
		stPostProcessParam.pJpegParam         = NULL;
		stPostProcessParam.pUsrData           = NULL;
		stPostProcessParam.bIsInPlace         = CAM_FALSE;

		// image resizing & rotate to generate the preview buffer
		error = ppu_image_process( pCameraState->hPpuHandle, pSensorImg, pPortImg, &stPostProcessParam );
		if ( error == CAM_ERROR_NONE )
		{
#if defined( CAM_PERF )
			t += CAM_TimeGetTickCount();
			CELOG( "Perf: format conversion[%d -> %d], resize[(%d * %d) -> (%d * %d)], rotation[%d] cost %.2f ms\n",
			       pSensorImg->eFormat, pPortImg->eFormat,
			       pSensorImg->iWidth, pSensorImg->iHeight,
			       pPreviewPort->iWidth, pPreviewPort->iHeight,
			       pPreviewPort->eRotation,
			       t / 1000.0 );
#endif
			if ( pPortImg->bEnableShotInfo )
			{
				pPortImg->stShotInfo = pSensorImg->stShotInfo;
			}

			pPortImg->tTick = pSensorImg->tTick;

			ret = _EnQueue( &pPreviewPort->qFilled, (void*)pPortImg );
			ASSERT( ret == 0 );

			// notify that the snapshot is ready on the port
			SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, CAM_PORT_PREVIEW );
		}
		else
		{
			ret = _EnQueue( &pPreviewPort->qEmpty, (void*)pPortImg );
			ASSERT( ret == 0 );

			// notify the snapshot frame loss
			SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_PREVIEW );
		}
	}
	else
	{
		// notify the snapshot frame loss
		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_PREVIEW );
	}
	_unlock_port( pCameraState, CAM_PORT_PREVIEW );
#else
	// notify the snapshot frame loss
	SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, CAM_PORT_PREVIEW );
#endif
    /* // flip it in vertical */
    /* int i, j; */
    /* unsigned char* ptr = (unsigned char*) pSensorImg->pBuffer[0]; */
    /* for (i = 0; i < pSensorImg->iHeight; i++) { */
        /* for (j = 0; j < pSensorImg->iWidth; j+=4) { */
            /* unsigned int* swap1 = (unsigned int*)(&ptr[j]); */
            /* unsigned int* swap2 = (unsigned int*)(&ptr[pSensorImg->iWidth * 2 - j - 4]); */
            /* swap(swap1, swap2); */
        /* } */
        /* ptr += pSensorImg->iWidth * 2; */
    /* } */

	// try to compress the image if JPEG compressor opened
	error = CAM_ERROR_NONE;
	if ( pCameraState->bNeedPostProcessing )
	{
		_CAM_PostProcessParam stPostProcessParam;

		stPostProcessParam.bFavorAspectRatio  = pCameraState->bPreviewFavorAspectRatio;
		stPostProcessParam.iPpuDigitalZoomQ16 = pCameraState->stPpuShotParam.iPpuDigitalZoomQ16;
		stPostProcessParam.eRotation          = pStillPort->eRotation;
		stPostProcessParam.pJpegParam         = &pCameraState->stJpegParam;
		stPostProcessParam.pUsrData           = NULL;
		stPostProcessParam.bIsInPlace         = ( ( pCameraState->bUsePrivateBuffer == CAM_FALSE ) ? CAM_TRUE : CAM_FALSE );

		// get the still port buffer
		if ( stPostProcessParam.bIsInPlace )
		{
			ret = _RemoveQueue( &pStillPort->qEmpty, pSensorImg->pUserData );
			ASSERT( ret == 0 );

			pPortImg = (CAM_ImageBuffer*)( pSensorImg->pUserData );
		}
		else
		{
			ret = _DeQueue( &pStillPort->qEmpty, (void**)&pPortImg );
			ASSERT( ret == 0 && pPortImg != NULL );
		}

		// JPEG encoding
		{
#if defined( CAM_PERF )
			CAM_Tick t = -CAM_TimeGetTickCount();
#endif
			error = ppu_image_process( pCameraState->hPpuHandle, pSensorImg, pPortImg, &stPostProcessParam );
			ASSERT_CAM_ERROR( error );

#if defined( CAM_PERF )
			t += CAM_TimeGetTickCount();
			CELOG( "Perf: format conversion[%d -> %d], resize[(%d * %d) -> (%d * %d)], digzoom[%.2f], rotation[%d] cost %.2f ms\n",
			       pSensorImg->eFormat, pPortImg->eFormat,
			       pSensorImg->iWidth, pSensorImg->iHeight,
			       pStillPort->iWidth, pStillPort->iHeight,
			       pCameraState->stPpuShotParam.iPpuDigitalZoomQ16 / 65536.0,
			       pStillPort->eRotation,
			       t / 1000.0 );
#endif
		}

		if ( pPortImg->bEnableShotInfo )
		{
			pPortImg->stShotInfo = pSensorImg->stShotInfo;
		}

		pPortImg->tTick = pSensorImg->tTick;

		if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
		{
			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorImg );
			ASSERT_CAM_ERROR( error );
		}
		else
		{
			error = _release_ppu_srcimg( pCameraState, pSensorImg );
			ASSERT_CAM_ERROR( error );
		}

		ret = _EnQueue( &pStillPort->qFilled, pPortImg );
		ASSERT( ret == 0 );
	}
	else
	{
		// remove from the empty queue
		ret = _RemoveQueue( &pStillPort->qEmpty, pSensorImg );
		ASSERT( ret == 0 );

		// put to the filled queue
		ret = _EnQueue( &pStillPort->qFilled, pSensorImg );
		ASSERT( ret == 0 );
	}

	pCameraState->iStillRestCount--;
	SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, CAM_PORT_STILL );

	// notify that all the images are processed
	if ( pCameraState->iStillRestCount <= 0 )
	{
		SEND_EVENT( pCameraState, CAM_EVENT_STILL_ALLPROCESSED, 0 );
	}

exit:
	_unlock_port( pCameraState, CAM_PORT_STILL );
	return error;
}

static CAM_Error _negotiate_image_format( CAM_ImageFormat eDstFmt, CAM_ImageFormat *pSrcFmt, CAM_ImageFormat *pFmtCap, CAM_Int32s iFmtCapCnt )
{
	CAM_Int32s i     = 0;
	CAM_Error  error = CAM_ERROR_NONE;

	for ( i = 0; i < iFmtCapCnt; i++ )
	{
		if ( pFmtCap[i] == eDstFmt )
		{
			break;
		}
	}

	if ( i < iFmtCapCnt )
	{
		*pSrcFmt = pFmtCap[i];
		return CAM_ERROR_NONE;
	}

	error = ppu_negotiate_format( eDstFmt, pSrcFmt, pFmtCap, iFmtCapCnt );
	return error;
}

/* negotiate image size with sensor( i.e. find the closest sensor resolution )
 * ideal size is exactly the same size as the target,
 * if can't find such a size, find out the max size that is no larger than the target size
 */
static CAM_Error _negotiate_image_size( CAM_Size *pDstSize, CAM_Size *pSrcSize, CAM_Size *pSizeCap, CAM_Int32s iSizeCapCnt )
{
	CAM_Int32s i;
	CAM_Int32s iNearestMatch = -1, iExactMatch = -1, iBestMatch = -1;
	CAM_Int32s iNearestPixNum, iDstPixNum;

	iDstPixNum     = pDstSize->iWidth * pDstSize->iHeight;
	iNearestPixNum = -1;

	for ( i = 0; i < iSizeCapCnt; i++ )
	{
		CAM_Int32s iPixNum = pSizeCap[i].iWidth * pSizeCap[i].iHeight;
		
		if ( ( iNearestPixNum == -1 ) || ( ( _ABSDIFF( iPixNum, iDstPixNum ) < _ABSDIFF( iNearestPixNum, iDstPixNum ) ) &&
		     ( pDstSize->iWidth / pDstSize->iHeight == pSizeCap[i].iWidth / pSizeCap[i].iHeight ) ) )
		{
			iNearestMatch  = i;
			iNearestPixNum = iPixNum;
		}
		if ( pSizeCap[i].iWidth == pDstSize->iWidth && pSizeCap[i].iHeight == pDstSize->iHeight )
		{
			iExactMatch = i;
		}
	}

	if ( iExactMatch >= 0 )
	{
		iBestMatch = iExactMatch;
	}
	else
	{
		iBestMatch = iNearestMatch;
	}

	pSrcSize->iWidth  = pSizeCap[iBestMatch].iWidth;
	pSrcSize->iHeight = pSizeCap[iBestMatch].iHeight;

	return CAM_ERROR_NONE;
}

static CAM_Error _get_required_sensor_settings( _CAM_ExtIspState *pCameraState, CAM_CaptureState eCaptureState, _CAM_PostProcessParam *pPpuShotParam,
                                                _CAM_SmartSensorConfig *pSensorConfig )
{
	CAM_Error                  error               = CAM_ERROR_NONE;
	CAM_Int32s                 iCurrentPort        = -1;
	CAM_Size                   stNegSize, stDstSize;
	CAM_ImageFormat            eNegFormat, eDstFormat;
	_CAM_SmartSensorCapability stSensorCap;
	CAM_Int32s                 iSupportedFormatCnt, iSupportedSizeCnt;
	CAM_ImageFormat            *pSupportedFormat   = NULL;
	CAM_Size                   *pSupportedSize     = NULL;
	CAM_StillSubMode           eStillSubMode       = pPpuShotParam->eStillSubMode;

	memset( pSensorConfig, 0, sizeof( _CAM_SmartSensorConfig ) );

	switch ( eCaptureState )
	{
	case CAM_CAPTURESTATE_IDLE:
	case CAM_CAPTURESTATE_STANDBY:
		pSensorConfig->eState = CAM_CAPTURESTATE_IDLE;
		return CAM_ERROR_NONE;

	case CAM_CAPTURESTATE_PREVIEW:
		if ( !( eStillSubMode & CAM_STILLSUBMODE_ZSL ) )
		{
			iCurrentPort          = CAM_PORT_PREVIEW;
			stDstSize.iWidth      = pCameraState->stPortState[iCurrentPort].iWidth;
			stDstSize.iHeight     = pCameraState->stPortState[iCurrentPort].iHeight;
			eDstFormat            = pCameraState->stPortState[iCurrentPort].eFormat;
			pSensorConfig->eState = eCaptureState;
		}
		else
		{
			// ZSL case sensor settings, change here to switch zsl data path
			iCurrentPort          = CAM_PORT_PREVIEW;
			stDstSize.iWidth      = pCameraState->stPortState[CAM_PORT_STILL].iWidth;
			stDstSize.iHeight     = pCameraState->stPortState[CAM_PORT_STILL].iHeight;
			eDstFormat            = pCameraState->stPortState[iCurrentPort].eFormat;
			pSensorConfig->eState = eCaptureState;
			//pSensorConfig->stJpegParam = pCameraState->stJpegParam;
		}
		break;

	case CAM_CAPTURESTATE_STILL:
		iCurrentPort               = CAM_PORT_STILL;
		stDstSize.iWidth           = pCameraState->stPortState[iCurrentPort].iWidth;
		stDstSize.iHeight          = pCameraState->stPortState[iCurrentPort].iHeight;
		eDstFormat                 = pCameraState->stPortState[iCurrentPort].eFormat;
		pSensorConfig->eState      = eCaptureState;
		pSensorConfig->stJpegParam = pCameraState->stJpegParam;
		break;

	case CAM_CAPTURESTATE_VIDEO:
		iCurrentPort          = CAM_PORT_VIDEO;
		stDstSize.iWidth      = pCameraState->stPortState[iCurrentPort].iWidth;
		stDstSize.iHeight     = pCameraState->stPortState[iCurrentPort].iHeight;
		eDstFormat            = pCameraState->stPortState[iCurrentPort].eFormat;
		pSensorConfig->eState = eCaptureState;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	error = SmartSensor_GetCapability( pCameraState->iSensorID, &stSensorCap );
	ASSERT_CAM_ERROR( error );

	switch ( iCurrentPort )
	{
	case CAM_PORT_PREVIEW:
	case CAM_PORT_VIDEO:
		pSupportedSize      = stSensorCap.stSupportedVideoSize;
		pSupportedFormat    = stSensorCap.eSupportedVideoFormat;
		iSupportedSizeCnt   = stSensorCap.iSupportedVideoSizeCnt;
		iSupportedFormatCnt = stSensorCap.iSupportedVideoFormatCnt;
		break;

	case CAM_PORT_STILL:
		pSupportedSize      = stSensorCap.stSupportedStillSize;
		pSupportedFormat    = stSensorCap.eSupportedStillFormat;
		iSupportedSizeCnt   = stSensorCap.iSupportedStillSizeCnt;
		iSupportedFormatCnt = stSensorCap.iSupportedStillFormatCnt;
		break;

	default:
		ASSERT( 0 );
		break;
	}

	// negotiate image size
	error = _negotiate_image_size( &stDstSize, &stNegSize, pSupportedSize, iSupportedSizeCnt );
	ASSERT_CAM_ERROR( error );

	pSensorConfig->iWidth  = stNegSize.iWidth;
	pSensorConfig->iHeight = stNegSize.iHeight;

	TRACE( CAM_INFO, "Info: Port[%d] Required size: %d x %d\n", iCurrentPort, stDstSize.iWidth, stDstSize.iHeight );
	TRACE( CAM_INFO, "Info: Port[%d] Selected size: %d x %d\n", iCurrentPort, stNegSize.iWidth, stNegSize.iHeight );

	// negotiate image format
	error = _negotiate_image_format( eDstFormat, &eNegFormat, pSupportedFormat, iSupportedFormatCnt );
	if ( error != CAM_ERROR_NONE )
	{
		TRACE( CAM_ERROR, "Error: unsupported format[%d] for port[%d]( %s, %s, %d )\n", eDstFormat, iCurrentPort, __FILE__, __FUNCTION__, __LINE__ );
		return error;
	}
	else
	{
		pSensorConfig->eFormat = eNegFormat;
		TRACE( CAM_INFO, "Info: port[%d] required format[%d], selected format[%d]\n", iCurrentPort, eDstFormat, eNegFormat );
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _calc_digital_zoom( _CAM_ExtIspState *pCameraState, CAM_Size *pSize, CAM_Int32s iDigitalZoomQ16, CAM_Int32s *pSensorDigitalZoomQ16, CAM_Int32s *pPpuDigitalZoomQ16 )
{
	CAM_Error  error                = CAM_ERROR_NONE;
	CAM_Int32s iSensorDigZoomCapQ16 = 0;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pSize );
	_CHECK_BAD_POINTER( pSensorDigitalZoomQ16 );
	_CHECK_BAD_POINTER( pPpuDigitalZoomQ16 );

	error = SmartSensor_GetDigitalZoomCapability( pCameraState->iSensorID, pSize->iWidth, pSize->iHeight, &iSensorDigZoomCapQ16 );
	ASSERT_CAM_ERROR( error );

	// TODO: here we can do some optimization to avoid very small PPU resize
	if ( iDigitalZoomQ16 > iSensorDigZoomCapQ16 )
	{
		*pPpuDigitalZoomQ16    = ( ( (CAM_Int64u)iDigitalZoomQ16 ) << 16 ) / iSensorDigZoomCapQ16;
		*pSensorDigitalZoomQ16 = iSensorDigZoomCapQ16;
	}
	else
	{
		*pPpuDigitalZoomQ16    = 1 << 16;
		*pSensorDigitalZoomQ16 = iDigitalZoomQ16;
	}

	return error;
}

static CAM_Error _set_sensor_digital_zoom( void *hSensorHandle, CAM_Int32s iSensorDigitalZoomQ16 )
{
	CAM_Error      error             = CAM_ERROR_NONE;
	_CAM_ShotParam stShotParamStatus;

	_CHECK_BAD_POINTER( hSensorHandle );

	error = SmartSensor_GetShotParam( hSensorHandle, &stShotParamStatus );
	ASSERT_CAM_ERROR( error );

	stShotParamStatus.iDigZoomQ16 = iSensorDigitalZoomQ16;
	error = SmartSensor_SetShotParam( hSensorHandle, &stShotParamStatus );
	ASSERT_CAM_ERROR( error );

	return error;
}



/*----------------------------------------------------------------------------------------------
*  Camera Engine's State Machine
*               null
*                |
*               idle -- [ standby ]
*                |
*     still -- preview -- video
*---------------------------------------------------------------------------------------------*/
/****************************************************************************************************
* State Transition: standby->idle
* Notes:
****************************************************************************************************/
static CAM_Error _standby2idle( _CAM_ExtIspState *pCameraState )
{
	return CAM_ERROR_NOTIMPLEMENTED;
}

/****************************************************************************************************
* State Transition: idle->standby
* Notes:
****************************************************************************************************/
static CAM_Error _idle2standby( _CAM_ExtIspState *pCameraState )
{
	return CAM_ERROR_NOTIMPLEMENTED;
}

/****************************************************************************************************
* State Transition: idle->null
* Notes:
****************************************************************************************************/
static CAM_Error _idle2null( _CAM_ExtIspState *pCameraState )
{
	CAM_Error  error = CAM_ERROR_NONE;
	CAM_Int32s i     = 0;

	ASSERT( pCameraState->iSensorID != -1 );

	error = SmartSensor_Deinit( &pCameraState->hSensorHandle );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	// NOTE: here we will flush all buffers assigned to the old sensor in all ports, so APP should aware that, he can
	// handle those buffers after switch sensor ID. A good practice is that APP flush all ports themselves.
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		_FlushQueue( &pCameraState->stPortState[i].qEmpty );
		_FlushQueue( &pCameraState->stPortState[i].qFilled );
	}

	pCameraState->iSensorID = -1;

	pCameraState->eCaptureState = CAM_CAPTURESTATE_NULL;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: idle->preview
* Notes: If Client's requirement is not met by sensor driver, Camera Engine will require a
*       nearest size from sensor( _get_required_sensor_settings ), and do resize/rotae/color space
*       convertion in Camera Engine to meet this requirement. In this case, Camera Engine will allocate
*       private buffers to sensor.
****************************************************************************************************/
static CAM_Error _idle2preview( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s             i = 0, ret = 0;
	CAM_Size               stSize = { 0, 0 };
	CAM_Error              error = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;

	// check preview port
	if ( !_is_valid_port( pCameraState, CAM_PORT_PREVIEW ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	// get sensor settings( resolution/format etc. )
	error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_PREVIEW, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// send config to sensor
	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// convert digital zoom
	stSize.iWidth  = stSensorConfig.iWidth;
	stSize.iHeight = stSensorConfig.iHeight;
	error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// get smart sensor buffer requirement
	error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
	ASSERT_CAM_ERROR( error );

	error = _is_need_ppu( &(pCameraState->stPortState[CAM_PORT_PREVIEW]), &stSensorConfig, &(pCameraState->stPpuShotParam),
	                      &(pCameraState->bNeedPostProcessing), &(pCameraState->bUsePrivateBuffer) );
	ASSERT_CAM_ERROR( error );

	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
	{
		CAM_Int32s iZslBufNum;
		iZslBufNum = _calczslbufnum( pCameraState->stCurrentSensorBufReq.iMinBufCount, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );

		for ( i = 0; i < iZslBufNum; i++ )
		{
			CAM_ImageBuffer *pTmpBuf = NULL;
			CAM_ImageBuffer *pZslBuf = NULL;

			TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
			error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pTmpBuf, &pZslBuf );
			ASSERT_CAM_ERROR( error );

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pZslBuf );
			ASSERT_CAM_ERROR( error );
		}

		// init buffer ring
		TRACE( CAM_INFO, "Info: Init buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ret = _InitQueue( &pCameraState->qPpuBufferRing, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );
		ASSERT( ret == 0 );
	}
	else
	{
		for ( i = 0; i < pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
		{
			CAM_ImageBuffer *pUsrBuf    = NULL;
			CAM_ImageBuffer *pSensorBuf = NULL;

			ret = _GetQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pUsrBuf );
			ASSERT( ret == 0 );

			if ( pCameraState->bNeedPostProcessing )
			{
				TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
			else
			{
				pSensorBuf = pUsrBuf;
				TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in preview state!\n" );
			}

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
			ASSERT_CAM_ERROR( error );
		}
	}

	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: preview->idle
* Notes: 
****************************************************************************************************/
static CAM_Error _preview2idle( _CAM_ExtIspState *pCameraState )
{
	CAM_Error              error = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;
	CAM_Int32s             ret = 0;

	memset( &stSensorConfig, 0, sizeof( _CAM_SmartSensorConfig ) );

	ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );
	// sleep check focus thread if it wake up
	if ( !pCameraState->bFocusSleep )
	{
		pCameraState->bFocusSleep = CAM_TRUE;

		if ( pCameraState->stShotParam.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
		{
			error = SmartSensor_CancelFocus( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}
	}

	ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_IDLE, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// sensor state switch to idle, buffers will be returned to Camera Engine 
	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// make sure the buffers enqueued to sensor has been reclaimed
	if ( !stSensorConfig.bFlushed )
	{
		error = SmartSensor_Flush( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );
	}

	// if ZSL enabled, deinit ring
	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
	{
		TRACE( CAM_INFO, "Info: Deinit buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ret = _DeinitQueue( &pCameraState->qPpuBufferRing );
		ASSERT( ret == 0 );
	}

	// flush ppu src images
	if ( pCameraState->bNeedPostProcessing )
	{
		error = _flush_all_ppu_srcimg( pCameraState );
		ASSERT_CAM_ERROR( error );
	}

	// XXX: Pls be informed that here buffers in filled queue did not be moved to empty queue, this may lead a mixup of 
	//      two time-adjacent preview frames. If you think this is important, just add this operation.      

	pCameraState->eCaptureState = CAM_CAPTURESTATE_IDLE;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: preview->video
* Notes: video port don't support resize/rotate/color space conversion.
****************************************************************************************************/
static CAM_Error _preview2video( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s             i      = 0;
	CAM_Size               stSize = { 0, 0 };
	CAM_Error              error  = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;
	CAM_Int32s             ret    = 0;

	if ( !_is_valid_port( pCameraState, CAM_PORT_VIDEO ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	memset( &stSensorConfig, 0, sizeof( _CAM_SmartSensorConfig ) );

	ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );
	// sleep check focus thread if it wake up
	if ( !pCameraState->bFocusSleep )
	{
		pCameraState->bFocusSleep = CAM_TRUE;

		if ( pCameraState->stShotParam.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
		{
			error = SmartSensor_CancelFocus( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}
	}

	ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_VIDEO, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// convert digital zoom
	stSize.iWidth  = stSensorConfig.iWidth;
	stSize.iHeight = stSensorConfig.iHeight;
	error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_VIDEO );
	ASSERT_CAM_ERROR( error );

	// make sure the buffers enqueued to sensor has been reclaimed
	if ( !stSensorConfig.bFlushed )
	{
		error = SmartSensor_Flush( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );
	}

	// if ZSL enabled, deinit ring
	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
	{
		TRACE( CAM_INFO, "Info: Deinit buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ret = _DeinitQueue( &pCameraState->qPpuBufferRing );
		ASSERT( ret == 0 );
	}

	// flush ppu src buffers
	if ( pCameraState->bNeedPostProcessing )
	{
		error = _flush_all_ppu_srcimg( pCameraState );
		ASSERT_CAM_ERROR( error );
	}

	error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
	ASSERT_CAM_ERROR( error );

	// XXX: ppu is never used in video state
	pCameraState->bNeedPostProcessing = CAM_FALSE;
	pCameraState->bUsePrivateBuffer   = CAM_FALSE;

	// enqueue video port buffers
	for ( i = 0; i < pCameraState->stPortState[CAM_PORT_VIDEO].qEmpty.iQueueCount; i++ )
	{
		CAM_ImageBuffer *pTmpImg = NULL;

		(void)_GetQueue( &pCameraState->stPortState[CAM_PORT_VIDEO].qEmpty, i, (void**)&pTmpImg );
		ASSERT( pTmpImg != NULL );

		error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pTmpImg );
		ASSERT_CAM_ERROR( error );
	}

	pCameraState->eCaptureState = CAM_CAPTURESTATE_VIDEO;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: preview->still
* Notes:
****************************************************************************************************/
static CAM_Error _preview2still( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s             i        = 0;
	CAM_Int32s             ret      = 0;
	CAM_Size               stSize   = { 0, 0 };
	CAM_ImageBuffer        *pTmpBuf = NULL;
	CAM_Error              error    = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;

	// preview & still port must be valid
	if ( !_is_valid_port( pCameraState, CAM_PORT_PREVIEW ) || !_is_valid_port( pCameraState, CAM_PORT_STILL ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );

	// sleep check focus thread if it wake up
	if ( !pCameraState->bFocusSleep )
	{
		pCameraState->bFocusSleep = CAM_TRUE;
		TRACE( CAM_WARN, "Warnning: Focussing is not finished under [%d]mode before capture, which may lead captured image out of focus, please check camera calling sequence whether has issue, it always strongly recommends to capture after focus call back arrived( %s, %s, %d )!\n", pCameraState->stShotParam.eFocusMode, __FILE__, __FUNCTION__, __LINE__ );

		if ( pCameraState->stShotParam.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
		{
			error = SmartSensor_CancelFocus( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}
	}

	ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	// if ZSL enabled, no need to reconfig sensor
	if ( !( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL ) )
	{
		error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_STILL, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		// convert digital zoom
		stSize.iWidth  = stSensorConfig.iWidth;
		stSize.iHeight = stSensorConfig.iHeight;
		error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_STILL );
		ASSERT_CAM_ERROR( error );

		// make sure the buffers enqueued to sensor has been reclaimed
		if ( !stSensorConfig.bFlushed )
		{
			error = SmartSensor_Flush( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}

		// flush all ppu src images
		if ( pCameraState->bNeedPostProcessing )
		{
			error = _flush_all_ppu_srcimg( pCameraState );
			ASSERT_CAM_ERROR( error );
		}

		error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
		ASSERT_CAM_ERROR( error );

		error = _is_need_ppu( &(pCameraState->stPortState[CAM_PORT_STILL]), &stSensorConfig, &(pCameraState->stPpuShotParam),
		                      &(pCameraState->bNeedPostProcessing), &(pCameraState->bUsePrivateBuffer) );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < pCameraState->stPortState[CAM_PORT_STILL].qEmpty.iQueueCount; i++ )
		{
			CAM_ImageBuffer *pUsrBuf    = NULL;
			CAM_ImageBuffer *pSensorBuf = NULL;

			ret = _GetQueue( &pCameraState->stPortState[CAM_PORT_STILL].qEmpty, i, (void**)&pUsrBuf );
			ASSERT( ret == 0 );

			if ( pCameraState->bNeedPostProcessing )
			{
				TRACE( CAM_INFO, "Info: Post Processing is enabled in still state!\n" );

				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
			else
			{
				pSensorBuf = pUsrBuf;
				TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in still state!\n" );
			}
			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
			ASSERT_CAM_ERROR( error );
		}
	}

	// clear the preview buffers so that it won't be mixed up with the snapshot images
	while ( _DeQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qFilled, (void**)&pTmpBuf ) == 0 )
	{
		ret = _EnQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty, pTmpBuf );
		ASSERT( ret == 0 );
	}

	// In still image capture state, we only capture the given number of images (iStillBurstCount)
	// while one frame is captured and processed, event CAM_EVENT_FRAME_READY is sent;
	// while all capture is done, we'll sent out an event CAM_EVENT_STILL_SHUTTERCLOSED;
	// while all images been processed, CAM_EVENT_STILL_ALLPROCESSED will be sent.
	pCameraState->iStillRestCount += pCameraState->stShotParam.stStillSubModeParam.iBurstCnt;

	pCameraState->eCaptureState   = CAM_CAPTURESTATE_STILL;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: video->preview
* Notes: 
****************************************************************************************************/
static CAM_Error _video2preview( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s             i      = 0;
	CAM_Int32s             ret    = 0;
	CAM_Size               stSize = { 0, 0 };
	CAM_Error              error  = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;

	error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_PREVIEW, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// convert digital zoom
	stSize.iWidth  = stSensorConfig.iWidth;
	stSize.iHeight = stSensorConfig.iHeight;
	error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// make sure the buffers enqueued to sensor has been reclaimed
	if ( !stSensorConfig.bFlushed )
	{
		error = SmartSensor_Flush( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );
	}

	error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
	ASSERT_CAM_ERROR( error );

	error = _is_need_ppu( &(pCameraState->stPortState[CAM_PORT_PREVIEW]), &stSensorConfig, &(pCameraState->stPpuShotParam),
	                      &(pCameraState->bNeedPostProcessing), &(pCameraState->bUsePrivateBuffer) );
	ASSERT_CAM_ERROR( error );

	if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
	{
		CAM_Int32s iZslBufNum;
		iZslBufNum = _calczslbufnum( pCameraState->stCurrentSensorBufReq.iMinBufCount, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );

		for ( i = 0; i < iZslBufNum; i++ )
		{
			CAM_ImageBuffer *pTmpBuf = NULL;
			CAM_ImageBuffer *pZslBuf = NULL;

			TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
			error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pTmpBuf, &pZslBuf );
			ASSERT_CAM_ERROR( error );

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pZslBuf );
			ASSERT_CAM_ERROR( error );
		}

		// init buffer ring
		TRACE( CAM_INFO, "Info: Init buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ret = _InitQueue( &pCameraState->qPpuBufferRing, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );
		ASSERT( ret == 0 );
	}
	else
	{
		for ( i = 0; i < pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
		{
			CAM_ImageBuffer *pUsrBuf    = NULL;
			CAM_ImageBuffer *pSensorBuf = NULL;

			ret = _GetQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pUsrBuf );
			ASSERT( ret == 0 );

			if ( pCameraState->bNeedPostProcessing )
			{
				TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
			else
			{
				pSensorBuf = pUsrBuf;
				TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in preview state!\n" );
			}

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
			ASSERT_CAM_ERROR( error );
		}
	}

	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return CAM_ERROR_NONE;
}

/****************************************************************************************************
* State Transition: still->preview
* Notes: 
****************************************************************************************************/
static CAM_Error _still2preview( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s             i      = 0;
	CAM_Int32s             ret    = 0;
	CAM_Size               stSize = { 0, 0 };
	CAM_Error              error  = CAM_ERROR_NONE;
	_CAM_SmartSensorConfig stSensorConfig;

	// preview port must be valid
	if ( !_is_valid_port( pCameraState, CAM_PORT_PREVIEW ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	// if ZSL enabled, no need to set preview config back to sensor, as sensor configuration did not change when state transition: preview2still
	if ( !( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL ) )
	{
		error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_PREVIEW, &pCameraState->stPpuShotParam, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		// convert digital zoom
		stSize.iWidth  = stSensorConfig.iWidth;
		stSize.iHeight = stSensorConfig.iHeight;
		error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_PREVIEW );
		ASSERT_CAM_ERROR( error );

		// make sure the buffers enqueued to sensor has been reclaimed
		if ( !stSensorConfig.bFlushed )
		{
			error = SmartSensor_Flush( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}

		// flush all ppu src images
		if ( pCameraState->bNeedPostProcessing )
		{
			error = _flush_all_ppu_srcimg( pCameraState );
			ASSERT_CAM_ERROR( error );
		}

		error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
		ASSERT_CAM_ERROR( error );

		error = _is_need_ppu( &(pCameraState->stPortState[CAM_PORT_PREVIEW]), &stSensorConfig, &(pCameraState->stPpuShotParam),
		                      &(pCameraState->bNeedPostProcessing), &(pCameraState->bUsePrivateBuffer) );
		ASSERT_CAM_ERROR( error );

		for ( i = 0; i < pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
		{
			CAM_ImageBuffer *pUsrBuf    = NULL;
			CAM_ImageBuffer *pSensorBuf = NULL;

			ret = _GetQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pUsrBuf );
			ASSERT( ret == 0 );

			if ( pCameraState->bNeedPostProcessing )
			{
				TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
			else
			{
				pSensorBuf = pUsrBuf;
				TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in preview state!\n" );
			}

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
			ASSERT_CAM_ERROR( error );
		}
	}

	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return CAM_ERROR_NONE;
}

/* port protection */
static CAM_Error _lock_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Int32s ret = 0;

	_CHECK_BAD_ANY_PORT_ID( iPortId );

	if ( iPortId == CAM_PORT_ANY )
	{
		CAM_Int32s i = 0;

		for ( i = CAM_PORT_STILL; i >= CAM_PORT_PREVIEW; i-- )
		{
			ret = CAM_MutexLock( pCameraState->stPortState[i].hMutex, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}
	}
	else
	{
		ret = CAM_MutexLock( pCameraState->stPortState[iPortId].hMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _unlock_port( _CAM_ExtIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Int32s ret = 0;

	_CHECK_BAD_ANY_PORT_ID( iPortId );

	if ( iPortId == CAM_PORT_ANY )
	{
		CAM_Int32s i = 0;

		for ( i = CAM_PORT_PREVIEW ; i <= CAM_PORT_STILL; i++ )
		{
			ret = CAM_MutexUnlock( pCameraState->stPortState[i].hMutex );
			ASSERT( ret == 0 );
		}
	}
	else
	{
		ret = CAM_MutexUnlock( pCameraState->stPortState[iPortId].hMutex );
		ASSERT( ret == 0 );
	}

	return CAM_ERROR_NONE;
}


/* input buffer check */
static CAM_Error _check_image_buffer( CAM_ImageBuffer *pImgBuf, _CAM_PortState *pPortState )
{
	CAM_Error          error    = CAM_ERROR_NONE;
	CAM_Int32s         i        = 0;
	CAM_Int32s         iIndex   = -1;
	CAM_Int32s         ret      = 0;
	CAM_ImageBufferReq *pBufReq = NULL;

	_CHECK_BAD_POINTER( pImgBuf );
	_CHECK_BAD_POINTER( pPortState );

	// first check duplicate enqueue
	ret = _RetrieveQueue( &(pPortState->qEmpty), pImgBuf, &iIndex );
	ASSERT( ret == 0 );
	if ( iIndex != -1 )
	{
		return CAM_WARN_DUPLICATEBUFFER;
	}

	ret = _RetrieveQueue( &(pPortState->qFilled), pImgBuf, &iIndex );
	ASSERT( ret == 0 );
	if ( iIndex != -1 )
	{
		return CAM_WARN_DUPLICATEBUFFER;
	}

	pBufReq = &(pPortState->stBufReq);

	if ( ( (pImgBuf->iFlag & BUF_FLAG_PHYSICALCONTIGUOUS) && (pImgBuf->iFlag & BUF_FLAG_NONPHYSICALCONTIGUOUS) ) ||
	     ( !(pImgBuf->iFlag & BUF_FLAG_PHYSICALCONTIGUOUS) && !(pImgBuf->iFlag & BUF_FLAG_NONPHYSICALCONTIGUOUS) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_PHYSICALCONTIGUOUS and BUF_FLAG_NONPHYSICALCONTIGUOUS must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pImgBuf->iFlag & BUF_FLAG_L1CACHEABLE) && (pImgBuf->iFlag & BUF_FLAG_L1NONCACHEABLE) ) ||
	     ( !(pImgBuf->iFlag & BUF_FLAG_L1CACHEABLE) && !(pImgBuf->iFlag & BUF_FLAG_L1NONCACHEABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_L1CACHEABLE and BUF_FLAG_L1NONCACHEABLE must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pImgBuf->iFlag & BUF_FLAG_L2CACHEABLE) &&  (pImgBuf->iFlag & BUF_FLAG_L2NONCACHEABLE) ) ||
	     ( !(pImgBuf->iFlag & BUF_FLAG_L2CACHEABLE) && !(pImgBuf->iFlag & BUF_FLAG_L2NONCACHEABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_L2CACHEABLE and BUF_FLAG_L2NONCACHEABLE must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pImgBuf->iFlag & BUF_FLAG_BUFFERABLE) &&  (pImgBuf->iFlag & BUF_FLAG_UNBUFFERABLE) ) ||
	     ( !(pImgBuf->iFlag & BUF_FLAG_BUFFERABLE) && !(pImgBuf->iFlag & BUF_FLAG_UNBUFFERABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_BUFFERABLE and BUF_FLAG_UNBUFFERABLE must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( !(
	       ( (pImgBuf->iFlag & BUF_FLAG_YUVPLANER) && !(pImgBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) && !(pImgBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) ) ||
	       (!(pImgBuf->iFlag & BUF_FLAG_YUVPLANER) &&  (pImgBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) && !(pImgBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) ) ||
	       (!(pImgBuf->iFlag & BUF_FLAG_YUVPLANER) && !(pImgBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) &&  (pImgBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) )
	      ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_YUVPLANER and BUF_FLAG_YUVBACKTOBACK and BUF_FLAG_YVUBACKTOBACK must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pImgBuf->iFlag & BUF_FLAG_ALLOWPADDING) &&  (pImgBuf->iFlag & BUF_FLAG_FORBIDPADDING) ) ||
	     ( !(pImgBuf->iFlag & BUF_FLAG_ALLOWPADDING) && !(pImgBuf->iFlag & BUF_FLAG_FORBIDPADDING) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_ALLOWPADDING and BUF_FLAG_FORBIDPADDING must be set exclusively )!( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	if ( pImgBuf->iFlag & ~pBufReq->iFlagAcceptable )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( iFlag (= 0x%x) is not acceptable (iFlagAcceptable = 0x%x) )!( %s, %s, %d )\n",
		       pImgBuf->iFlag, pBufReq->iFlagAcceptable, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADBUFFER;
	}

	for ( i = 0; i < 3; i++ )
	{
		if ( pBufReq->iMinBufLen[i] <= 0 )
		{
			continue;
		}

		// check iStep[i]
		if ( pBufReq->iFlagAcceptable & BUF_FLAG_FORBIDPADDING )
		{
			if ( pImgBuf->iStep[i] != pBufReq->iMinStep[i] )
			{
				TRACE( CAM_ERROR, "Error: BUF_FLAG_FORBIDPADDING flag is set, hence CAM_ImageBuffer::iStep[%d] (%d) should be equal to "
				       "CAM_ImageBufferReq::iMinStep[%d] (%d)!( %s, %s, %d )\n",
				       i, pImgBuf->iStep[i], i, pBufReq->iMinStep[i], __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_BADBUFFER;
			}
		}
		else
		{
			if ( pImgBuf->iStep[i] < pBufReq->iMinStep[i] )
			{
				TRACE( CAM_ERROR, "Error: BUF_FLAG_FORBIDPADDING flag is clear, hence CAM_ImageBuffer::iStep[%d] (%d) should be no less"
				       "than CAM_ImageBufferReq::iMinStep[%d] (%d)!( %s, %s, %d )\n",
				       i, pImgBuf->iStep[i], i, pBufReq->iMinStep[i], __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_BADBUFFER;
			}
		}

		// check row align
		if ( pImgBuf->iStep[i] & (pBufReq->iRowAlign[i] - 1) )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::iStep[%d] (%d) should align to CAM_ImageBufferReq::iRowAlign[%d] (%d)!( %s, %s, %d )\n",
			       i, pImgBuf->iStep[i], i, pBufReq->iRowAlign[i], __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_BADBUFFER;
		}

		// check allocate len
		if ( pImgBuf->iAllocLen[i] < pBufReq->iMinBufLen[i] + pImgBuf->iOffset[i] )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::iAllocLen[%d] (%d) should be no less than"
			       "CAM_ImageBufferReq::iMinBufLen[%d] (%d) + CAM_ImageBuffer::iOffset[%d] (%d)!( %s, %s, %d )\n",
			       i, pImgBuf->iAllocLen[i], i, pBufReq->iMinBufLen[i], i, pImgBuf->iOffset[i], __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_BADBUFFER;
		}

		// check pointer
		if ( pBufReq->iMinBufLen[i] > 0 && pImgBuf->pBuffer[i] == NULL )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::pBuffer[%d] (%p) should point to valid address!( %s, %s, %d )\n",
			       i, pImgBuf->pBuffer[i], __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_BADBUFFER;
		}

		if ( (CAM_Int32s)(pImgBuf->pBuffer[i] + pImgBuf->iOffset[i]) & (pBufReq->iAlignment[i] - 1) )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::pBuffer[%d] (%p) + CAM_ImageBuffer::iOffset[%d] (%d) should align to"
			       "CAM_ImageBufferReq::iAlignment[%d] (%d) byte!( %s, %s, %d )\n",
			       i, pImgBuf->pBuffer[i], i, pImgBuf->iOffset[i], i, pBufReq->iAlignment[i], __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_BADBUFFER;
		}
	}

	if ( !(pBufReq->iFlagAcceptable & BUF_FLAG_YUVPLANER) )
	{
		// all three buffers must be back-to-back
		if ( pBufReq->iFlagAcceptable & BUF_FLAG_YUVBACKTOBACK )
		{
			// back to back layout in YUV order
			if ( pBufReq->iMinBufLen[1] > 0 &&
			     pImgBuf->pBuffer[0] + pImgBuf->iOffset[0] + pBufReq->iMinBufLen[0] != pImgBuf->pBuffer[1] + pImgBuf->iOffset[1] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!( %s, %s, %d )\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}

			if ( pBufReq->iMinBufLen[2] > 0 &&
			     pImgBuf->pBuffer[1] + pImgBuf->iOffset[1] + pBufReq->iMinBufLen[1] != pImgBuf->pBuffer[2] + pImgBuf->iOffset[2] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!( %s, %s, %d )\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		else if ( pBufReq->iFlagAcceptable & BUF_FLAG_YVUBACKTOBACK )
		{
			// back to back layout in YVU order
			if ( pBufReq->iMinBufLen[2] > 0 &&
			     pImgBuf->pBuffer[0] + pImgBuf->iOffset[0] + pBufReq->iMinBufLen[0] != pImgBuf->pBuffer[2] + pImgBuf->iOffset[2] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!( %s, %s, %d )\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}

			if ( pBufReq->iMinBufLen[1] > 0 &&
			     pImgBuf->pBuffer[2] + pImgBuf->iOffset[2] + pBufReq->iMinBufLen[2] != pImgBuf->pBuffer[1] + pImgBuf->iOffset[1] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!( %s, %s, %d )\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
	}

	return error;
}

/* check whether ppu in needed in current usage case */
static CAM_Error _is_need_ppu( _CAM_PortState *pPortState, _CAM_SmartSensorConfig *pSensorConfig, _CAM_PostProcessParam *pPpuShotParam,
                               CAM_Bool *pbNeedPostProcessing, CAM_Bool *pbNeedPrivateBuffer )
{
	*pbNeedPostProcessing = ( pSensorConfig->iWidth  != pPortState->iWidth  || pSensorConfig->iHeight != pPortState->iHeight   ||
	                          pSensorConfig->eFormat != pPortState->eFormat || pPortState->eRotation  != CAM_FLIPROTATE_NORMAL ||
	                          pPpuShotParam->iPpuDigitalZoomQ16 > ( 1 << 16 ) || ( pPpuShotParam->eStillSubMode & CAM_STILLSUBMODE_ZSL ) );

    ALOGE("ppu pSensorConfig->eFormat: %d pPortState->eFormat %d", pSensorConfig->eFormat, pPortState->eFormat);
	if ( *pbNeedPostProcessing == CAM_TRUE )
	{
		if ( ( pSensorConfig->eFormat == CAM_IMGFMT_YCbCr420P || pSensorConfig->eFormat == CAM_IMGFMT_YCrCb420P ) &&
		     ( pPortState->eFormat == CAM_IMGFMT_YCbCr420SP || pPortState->eFormat == CAM_IMGFMT_YCrCb420SP ) &&
		     !( pPpuShotParam->eStillSubMode & CAM_STILLSUBMODE_ZSL ) )
		{
			*pbNeedPrivateBuffer = CAM_FALSE;
		}
		else
		{
			*pbNeedPrivateBuffer = CAM_TRUE;
		}
	}
	else
	{
		*pbNeedPrivateBuffer = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}


/* ppu buffer management */
static CAM_Error _request_ppu_srcimg( _CAM_ExtIspState *pCameraState, CAM_ImageBufferReq *pBufReq, CAM_ImageBuffer *pUsrImgBuf, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Int32s      i         = 0;
	CAM_Int32s      iTotalLen = 0;
	CAM_ImageBuffer *pTmpImg  = NULL;

	ASSERT( pCameraState->bNeedPostProcessing == CAM_TRUE );

	if ( !pCameraState->bUsePrivateBuffer )
	{
		// just reuse the user image buffer
		for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
		{
			if ( !pCameraState->bPpuSrcImgUsed[i] && pCameraState->stPpuSrcImg[i].pUserData == pUsrImgBuf )
			{
				break;
			}
		}

		if ( i < CAM_MAX_PORT_BUF_CNT )
		{
			pCameraState->bPpuSrcImgUsed[i] = CAM_TRUE;
			*ppImgBuf                       = &pCameraState->stPpuSrcImg[i];

			return CAM_ERROR_NONE;
		}

		for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
		{
			if ( !pCameraState->bPpuSrcImgUsed[i] && pCameraState->stPpuSrcImg[i].pUserData == NULL )
			{
				break;
			}
		}
		// we need to check if we have enough structures to hold the buffer
		if ( i >= CAM_MAX_PORT_BUF_CNT )
		{
			TRACE( CAM_ERROR, "Error: too many buffers to be handled by camera-engine( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}

		pTmpImg = &pCameraState->stPpuSrcImg[i];

		pTmpImg->eFormat       = pBufReq->eFormat;
		pTmpImg->iOffset[0]    = 0;
		pTmpImg->iOffset[1]    = 0;
		pTmpImg->iOffset[2]    = 0;
		pTmpImg->iStep[0]      = pBufReq->iMinStep[0];
		pTmpImg->iStep[1]      = pBufReq->iMinStep[1];
		pTmpImg->iStep[2]      = pBufReq->iMinStep[2];
		pTmpImg->iWidth        = pBufReq->iWidth;
		pTmpImg->iHeight       = pBufReq->iHeight;
		pTmpImg->iAllocLen[0]  = pBufReq->iMinBufLen[0];
		pTmpImg->iAllocLen[1]  = pBufReq->iMinBufLen[1];
		pTmpImg->iAllocLen[2]  = pBufReq->iMinBufLen[2];
		pTmpImg->iFilledLen[0] = 0;
		pTmpImg->iFilledLen[1] = 0;
		pTmpImg->iFilledLen[2] = 0;
		pTmpImg->iFlag         = pUsrImgBuf->iFlag;
		pTmpImg->iUserIndex    = i;
		pTmpImg->pBuffer[0]    = pUsrImgBuf->pBuffer[0];
		pTmpImg->pBuffer[1]    = (CAM_Int8u*)_ALIGN_TO( pTmpImg->pBuffer[0] + pTmpImg->iAllocLen[0], pBufReq->iAlignment[1] );
		pTmpImg->pBuffer[2]    = (CAM_Int8u*)_ALIGN_TO( pTmpImg->pBuffer[1] + pTmpImg->iAllocLen[1], pBufReq->iAlignment[2] );
		pTmpImg->iPhyAddr[0]   = 0;
		pTmpImg->iPhyAddr[1]   = 0;
		pTmpImg->iPhyAddr[2]   = 0;
		pTmpImg->pUserData     = (void*)pUsrImgBuf;

		pTmpImg->bEnableShotInfo = pUsrImgBuf->bEnableShotInfo;

		pCameraState->bPpuSrcImgUsed[i] = CAM_TRUE;

		*ppImgBuf = pTmpImg;
	}
	else
	{
		// search all allocated privated images and try to find one that is not in use
		for ( i = 0; i < pCameraState->iPpuSrcImgAllocateCnt; i++ )
		{
			if ( !pCameraState->bPpuSrcImgUsed[i] )
			{
				break;
			}
		}

		if ( i < pCameraState->iPpuSrcImgAllocateCnt )
		{	
			// found! then use it
			*ppImgBuf                       = &pCameraState->stPpuSrcImg[i];
			pCameraState->bPpuSrcImgUsed[i] = CAM_TRUE;

			return CAM_ERROR_NONE;
		}

		// not found, so we need to allocate a new one. and before that, we need to check if we have enough structures to hold the buffer
		if ( i >= CAM_MAX_PORT_BUF_CNT )
		{
			TRACE( CAM_ERROR, "Error: too many buffers to be handled by camera-engine( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}

		// start to allocate a new priviate image
		pTmpImg = &pCameraState->stPpuSrcImg[i];

		pTmpImg->eFormat       = pBufReq->eFormat;
		pTmpImg->iOffset[0]    = 0;
		pTmpImg->iOffset[1]    = 0;
		pTmpImg->iOffset[2]    = 0;
		pTmpImg->iStep[0]      = pBufReq->iMinStep[0];
		pTmpImg->iStep[1]      = pBufReq->iMinStep[1];
		pTmpImg->iStep[2]      = pBufReq->iMinStep[2];
		pTmpImg->iWidth        = pBufReq->iWidth;
		pTmpImg->iHeight       = pBufReq->iHeight;
		pTmpImg->iAllocLen[0]  = pBufReq->iMinBufLen[0];
		pTmpImg->iAllocLen[1]  = pBufReq->iMinBufLen[1];
		pTmpImg->iAllocLen[2]  = pBufReq->iMinBufLen[2];
		pTmpImg->iFilledLen[0] = 0;
		pTmpImg->iFilledLen[1] = 0;
		pTmpImg->iFilledLen[2] = 0;
		pTmpImg->iUserIndex    = i;
		iTotalLen              = pTmpImg->iAllocLen[0] + pBufReq->iAlignment[0] +
		                         pTmpImg->iAllocLen[1] + pBufReq->iAlignment[1] +
		                         pTmpImg->iAllocLen[2] + pBufReq->iAlignment[2];
		pTmpImg->pUserData     = osalbmm_malloc( iTotalLen, OSALBMM_ATTR_DEFAULT ); // cachable & bufferable
		if ( pTmpImg->pUserData == NULL )
		{
			TRACE( CAM_ERROR, "Error: failed to allocate %d bytes memory( %s, %s, %d )\n", iTotalLen, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}

		pTmpImg->pBuffer[0] = (CAM_Int8u*)_ALIGN_TO( pTmpImg->pUserData, pBufReq->iAlignment[0] );
		pTmpImg->pBuffer[1] = (CAM_Int8u*)_ALIGN_TO( pTmpImg->pBuffer[0] + pTmpImg->iAllocLen[0], pBufReq->iAlignment[1] );
		pTmpImg->pBuffer[2] = (CAM_Int8u*)_ALIGN_TO( pTmpImg->pBuffer[1] + pTmpImg->iAllocLen[1], pBufReq->iAlignment[2] );

		pTmpImg->iPhyAddr[0] = (CAM_Int32u)osalbmm_get_paddr( pTmpImg->pBuffer[0] );
		pTmpImg->iPhyAddr[1] = (CAM_Int32u)osalbmm_get_paddr( pTmpImg->pBuffer[1] );
		pTmpImg->iPhyAddr[2] = (CAM_Int32u)osalbmm_get_paddr( pTmpImg->pBuffer[2] );

		pTmpImg->iFlag = BUF_FLAG_PHYSICALCONTIGUOUS |
		                 BUF_FLAG_L1CACHEABLE | BUF_FLAG_L2CACHEABLE | BUF_FLAG_BUFFERABLE |
		                 BUF_FLAG_YUVBACKTOBACK | BUF_FLAG_FORBIDPADDING;

		pTmpImg->bEnableShotInfo = ( pUsrImgBuf ? pUsrImgBuf->bEnableShotInfo : CAM_TRUE );

		// invalidate bmm buffer's cache line
		osalbmm_flush_cache_range( pTmpImg->pUserData, iTotalLen, OSALBMM_DMA_FROM_DEVICE );

		pCameraState->iPpuSrcImgAllocateCnt++;
		pCameraState->bPpuSrcImgUsed[i] = CAM_TRUE;

		*ppImgBuf = pTmpImg;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _release_ppu_srcimg( _CAM_ExtIspState *pCameraState, CAM_ImageBuffer *pImgBuf )
{
	CAM_Int32s iBufIndex = pImgBuf->iUserIndex;

	ASSERT( pCameraState->bNeedPostProcessing == CAM_TRUE );

	ASSERT( pCameraState->bPpuSrcImgUsed[iBufIndex] == CAM_TRUE );
	ASSERT( &pCameraState->stPpuSrcImg[iBufIndex] == pImgBuf );

	if ( !pCameraState->bUsePrivateBuffer )
	{
		ASSERT( iBufIndex >= 0 && iBufIndex < CAM_MAX_PORT_BUF_CNT );
	}
	else
	{
		ASSERT( iBufIndex >= 0 && iBufIndex < pCameraState->iPpuSrcImgAllocateCnt );
	}

	pCameraState->bPpuSrcImgUsed[iBufIndex] = CAM_FALSE;

	return CAM_ERROR_NONE;
}

static CAM_Error _flush_all_ppu_srcimg( _CAM_ExtIspState *pCameraState )
{
	CAM_Int32s i = 0;

	if ( pCameraState->bUsePrivateBuffer && ( pCameraState->iPpuSrcImgAllocateCnt > 0 ) )
	{
		for ( i = pCameraState->iPpuSrcImgAllocateCnt - 1; i >= 0; i-- )
		{
			osalbmm_free( pCameraState->stPpuSrcImg[i].pUserData );
		}
	}

	pCameraState->iPpuSrcImgAllocateCnt = 0;

	for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
	{
		memset( &pCameraState->stPpuSrcImg[i], 0, sizeof( CAM_ImageBuffer ) );
		pCameraState->bPpuSrcImgUsed[i] = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _set_still_submode( _CAM_ExtIspState *pCameraState, CAM_StillSubMode eStillSubMode, CAM_StillSubModeParam *pStillSubModeParam )
{
	CAM_Error              error = CAM_ERROR_NONE;
	CAM_Int32s             i     = 0;
	CAM_Bool               bNeedPostProcessing, bUsePrivateBuffer;
	CAM_ShotModeCapability stSensorShotModeCap;
	_CAM_ShotParam         stSensorShotParamSettings;
	CAM_StillSubModeParam  stStillParam = { 1, 0 };

	_CHECK_BAD_POINTER( pCameraState );

	if ( pStillSubModeParam != NULL )
	{
		stStillParam = *pStillSubModeParam;
	}

	// get sensor capability
	error = SmartSensor_GetShotModeCapability( pCameraState->iSensorID, pCameraState->stShotParam.eShotMode, &stSensorShotModeCap );
	ASSERT_CAM_ERROR( error );

	switch ( eStillSubMode )
	{
	case CAM_STILLSUBMODE_SIMPLE:
		stStillParam.iBurstCnt = 1;
		stStillParam.iZslDepth = 0;
		break;

	case CAM_STILLSUBMODE_BURST:
		_CHECK_SUPPORT_RANGE( stStillParam.iBurstCnt, 2, pCameraState->stShotModeCap.iMaxBurstCnt );
		_CHECK_SUPPORT_RANGE( stStillParam.iZslDepth, 0, 0 );
		break;

	case CAM_STILLSUBMODE_ZSL:
		_CHECK_SUPPORT_RANGE( stStillParam.iBurstCnt, 1, 1 );
		_CHECK_SUPPORT_RANGE( stStillParam.iZslDepth, 1, pCameraState->stShotModeCap.iMaxZslDepth );
		break;

	case CAM_STILLSUBMODE_BURSTZSL:
		_CHECK_SUPPORT_RANGE( stStillParam.iBurstCnt, 2, pCameraState->stShotModeCap.iMaxBurstCnt );
		_CHECK_SUPPORT_RANGE( stStillParam.iZslDepth, 1, pCameraState->stShotModeCap.iMaxZslDepth );
		break;

	default:
		ASSERT( 0 );
		break;
	}

	// get current shot param
	error = SmartSensor_GetShotParam( pCameraState->hSensorHandle, &stSensorShotParamSettings );
	ASSERT_CAM_ERROR( error );

	// burst mode
	if ( pCameraState->stShotParam.stStillSubModeParam.iBurstCnt != stStillParam.iBurstCnt )
	{
		stSensorShotParamSettings.stStillSubModeParam.iBurstCnt = stStillParam.iBurstCnt;
	}

	// zsl mode
	if ( pCameraState->stShotParam.stStillSubModeParam.iZslDepth != stStillParam.iZslDepth )
	{
		// we won't do hybrid zsl, it's either all done by sensor or camera engine
		if ( stSensorShotModeCap.iMaxZslDepth > 0 && stSensorShotModeCap.iMaxZslDepth >= stStillParam.iZslDepth )
		{
			TRACE( CAM_WARN, "Warning: sensor supports ZSL by itself, will directly use sensor's capability( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			stSensorShotParamSettings.eStillSubMode                 = eStillSubMode;
			stSensorShotParamSettings.stStillSubModeParam.iZslDepth = stStillParam.iZslDepth;
		}
		else
		{
			TRACE( CAM_WARN, "Warning: sensor doesn't support ZSL, will use camera-engine's S/W ZSL( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

			// zsl is done by camera-engine
			error = _set_zero_shutter_lag( pCameraState, eStillSubMode & CAM_STILLSUBMODE_ZSL, stStillParam.iZslDepth );
			ASSERT_CAM_ERROR( error );

			stSensorShotParamSettings.eStillSubMode                 = stSensorShotParamSettings.eStillSubMode & ( ~CAM_STILLSUBMODE_ZSL );
			stSensorShotParamSettings.stStillSubModeParam.iZslDepth = 0;
		}
	}

	error = SmartSensor_SetShotParam( pCameraState->hSensorHandle, &stSensorShotParamSettings );

	if ( error == CAM_ERROR_NONE )
	{
		// update still submode status
		pCameraState->stShotParam.eStillSubMode       = eStillSubMode;
		pCameraState->stShotParam.stStillSubModeParam = stStillParam;
	}

	return error;
}

static CAM_Error _set_zero_shutter_lag( _CAM_ExtIspState *pCameraState, CAM_StillSubMode eStillSubMode, CAM_Int32s iZslDepth )
{
	CAM_Error              error = CAM_ERROR_NONE;
	CAM_Int32s             i     = 0;
	CAM_Int32s             ret   = 0;
	CAM_Size               stSize;
	CAM_Bool               bNeedPostProcessing, bUsePrivateBuffer;
	_CAM_SmartSensorConfig stSensorConfig;

	switch ( pCameraState->eCaptureState )
	{
	case CAM_CAPTURESTATE_IDLE:
		// update submode and zsl depth, wait for _idle2preview to do enabling
		pCameraState->stPpuShotParam.eStillSubMode = eStillSubMode;
		break;

	case CAM_CAPTURESTATE_PREVIEW:
	{
		_CAM_PostProcessParam stPpuParamSettings = pCameraState->stPpuShotParam;

		stPpuParamSettings.eStillSubMode = eStillSubMode;

		error = _get_required_sensor_settings( pCameraState, pCameraState->eCaptureState, &stPpuParamSettings, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		// update bNeedPostProcessing & bUsePrivateBuffer
		error = _is_need_ppu( &(pCameraState->stPortState[CAM_PORT_PREVIEW]), &stSensorConfig, &stPpuParamSettings,
		                      &bNeedPostProcessing, &bUsePrivateBuffer );
		ASSERT_CAM_ERROR( error );

		error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
		ASSERT_CAM_ERROR( error );

		// convert digital zoom
		stSize.iWidth  = stSensorConfig.iWidth;
		stSize.iHeight = stSensorConfig.iHeight;
		error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_STILL );
		ASSERT_CAM_ERROR( error );

		// update sensor buffer requirement
		error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
		ASSERT_CAM_ERROR( error );

		if ( eStillSubMode & CAM_STILLSUBMODE_ZSL )
		{
			// if zsl mode not change, but need change zsl depth, must first deinit ring
			if ( pCameraState->stPpuShotParam.eStillSubMode & CAM_STILLSUBMODE_ZSL )
			{
				ret = _DeinitQueue( &pCameraState->qPpuBufferRing );
				ASSERT( ret == 0 );
			}

			TRACE( CAM_INFO, "Info: Init buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			ret = _InitQueue( &pCameraState->qPpuBufferRing, iZslDepth );
			ASSERT( ret == 0 );
		}
		else
		{
			TRACE( CAM_INFO, "Info: Deinit buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			ret = _DeinitQueue( &pCameraState->qPpuBufferRing );
			ASSERT( ret == 0 );
		}

		// no matter turn on/off ZSL, need flush all ppu source images
		error = _flush_all_ppu_srcimg( pCameraState );
		ASSERT_CAM_ERROR( error );

		pCameraState->bNeedPostProcessing = bNeedPostProcessing;
		pCameraState->bUsePrivateBuffer = bUsePrivateBuffer;

		if ( eStillSubMode & CAM_STILLSUBMODE_ZSL )
		{
			// if enable ZSL, request all private buffers needed
			CAM_Int32s iZslBufNum;
			iZslBufNum = _calczslbufnum( pCameraState->stCurrentSensorBufReq.iMinBufCount, iZslDepth );

			for ( i = 0; i < iZslBufNum; i++ )
			{
				CAM_ImageBuffer *pTmpBuf = NULL;
				CAM_ImageBuffer *pZslBuf = NULL;

				TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
				error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pTmpBuf, &pZslBuf );
				ASSERT_CAM_ERROR( error );

				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pZslBuf );
				ASSERT_CAM_ERROR( error );
			}
		}
		else
		{
			// disable ZSL case
			for ( i = 0; i < pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
			{
				CAM_ImageBuffer *pUsrBuf    = NULL;
				CAM_ImageBuffer *pSensorBuf = NULL;

				ret = _GetQueue( &pCameraState->stPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pUsrBuf );
				ASSERT( ret == 0 );

				if ( pCameraState->bNeedPostProcessing )
				{
					TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
					error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pUsrBuf, &pSensorBuf );
					ASSERT_CAM_ERROR( error );
				}
				else
				{
					TRACE( CAM_INFO, "Info: Post Processing is NOT enabled in preview state!\n" );
					pSensorBuf = pUsrBuf;
				}

				error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pSensorBuf );
				ASSERT_CAM_ERROR( error );
			}
		}

		// update ppu still sub-mode
		pCameraState->stPpuShotParam.eStillSubMode = pCameraState->stPpuShotParam.eStillSubMode & ( ~CAM_STILLSUBMODE_ZSL );
		pCameraState->stPpuShotParam.eStillSubMode = pCameraState->stPpuShotParam.eStillSubMode | eStillSubMode;
	}
		break;

	default:
		ASSERT( 0 );
		break;
	}

	return error;
}

static CAM_Error _reset_zsl_preview( _CAM_ExtIspState *pCameraState, CAM_Size *pSize )
{
	CAM_Int32s                 i = 0, ret = 0;
	CAM_Error                  error = CAM_ERROR_NONE;
	CAM_Size                   stNegSize, stSize;
	_CAM_SmartSensorConfig     stSensorConfig;
	_CAM_SmartSensorCapability stSensorCap;

	ret = CAM_MutexLock( pCameraState->hFocusMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );
	// sleep check focus thread if it wake up
	if ( !pCameraState->bFocusSleep )
	{
		pCameraState->bFocusSleep = CAM_TRUE;

		if ( pCameraState->stShotParam.eFocusMode == CAM_FOCUS_AUTO_ONESHOT )
		{
			error = SmartSensor_CancelFocus( pCameraState->hSensorHandle );
			ASSERT_CAM_ERROR( error );
		}
	}

	ret = CAM_MutexUnlock( pCameraState->hFocusMutex );
	ASSERT( ret == 0 );

	error = _get_required_sensor_settings( pCameraState, CAM_CAPTURESTATE_IDLE, &pCameraState->stPpuShotParam, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// sensor state switch to idle
	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// make sure the buffers enqueued to sensor has been reclaimed
	if ( !stSensorConfig.bFlushed )
	{
		error = SmartSensor_Flush( pCameraState->hSensorHandle );
		ASSERT_CAM_ERROR( error );
	}

	// deinit ring
	TRACE( CAM_INFO, "Info: Deinit buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	ret = _DeinitQueue( &pCameraState->qPpuBufferRing );
	ASSERT( ret == 0 );

	// flush ppu src images
	error = _flush_all_ppu_srcimg( pCameraState );
	ASSERT_CAM_ERROR( error );

	/* get required sensor settings for zsl preview */
	// get sensor cap
	error = SmartSensor_GetCapability( pCameraState->iSensorID, &stSensorCap );
	ASSERT_CAM_ERROR( error );

	// negotiate image size
	error = _negotiate_image_size( pSize, &stNegSize, stSensorCap.stSupportedVideoSize, stSensorCap.iSupportedVideoSizeCnt );
	ASSERT_CAM_ERROR( error );

	TRACE( CAM_INFO, "Info: Port[%d] Required size: %d x %d\n", CAM_PORT_PREVIEW, pSize->iWidth, pSize->iHeight );
	TRACE( CAM_INFO, "Info: Port[%d] Selected size: %d x %d\n", CAM_PORT_PREVIEW, stNegSize.iWidth, stNegSize.iHeight );

	stSensorConfig.iWidth  = stNegSize.iWidth;
	stSensorConfig.iHeight = stNegSize.iHeight;
	stSensorConfig.eFormat = pCameraState->stPortState[CAM_PORT_PREVIEW].eFormat;
	stSensorConfig.eState  = CAM_CAPTURESTATE_PREVIEW;

	// send config to sensor
	error = SmartSensor_Config( pCameraState->hSensorHandle, &stSensorConfig );
	ASSERT_CAM_ERROR( error );

	// convert digital zoom
	stSize.iWidth  = stSensorConfig.iWidth;
	stSize.iHeight = stSensorConfig.iHeight;
	error = _convert_digital_zoom( pCameraState, &stSize, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// get smart sensor buffer requirement
	error = SmartSensor_GetBufReq( pCameraState->hSensorHandle, &stSensorConfig, &pCameraState->stCurrentSensorBufReq );
	ASSERT_CAM_ERROR( error );

	// request new private buffers, enqueue to sensor and init the buffer ring
	{
		CAM_Int32s iZslBufNum;
		iZslBufNum = _calczslbufnum( pCameraState->stCurrentSensorBufReq.iMinBufCount, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );

		for ( i = 0; i < iZslBufNum; i++ )
		{
			CAM_ImageBuffer *pTmpBuf = NULL;
			CAM_ImageBuffer *pZslBuf = NULL;

			TRACE( CAM_INFO, "Info: Post Processing is enabled in preview state!\n" );
			error = _request_ppu_srcimg( pCameraState, &pCameraState->stCurrentSensorBufReq, pTmpBuf, &pZslBuf );
			ASSERT_CAM_ERROR( error );

			error = SmartSensor_Enqueue( pCameraState->hSensorHandle, pZslBuf );
			ASSERT_CAM_ERROR( error );
		}

		// init buffer ring
		TRACE( CAM_INFO, "Info: Init buffer ring( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ret = _InitQueue( &pCameraState->qPpuBufferRing, pCameraState->stShotParam.stStillSubModeParam.iZslDepth );
		ASSERT( ret == 0 );
	}

	return CAM_ERROR_NONE;
}
