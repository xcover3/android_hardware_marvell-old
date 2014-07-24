/******************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cam_utility.h"
#include "cam_log.h"
#include "CameraOSAL.h"

#include "cam_gen.h"
#include "cam_ppu.h"

#include "cam_socisp_eng.h"
#include "cam_socisp_hal.h"

/* Native Functions Declaration */

// Camera Engine entry functions
static CAM_Error _open( void **ppDeviceData );
static CAM_Error _close( void **ppDeviceData );
static CAM_Error _command( void *pDeviceData, CAM_Command cmd, CAM_Param param1, CAM_Param param2 );

static CAM_Error _set_camera_state( _CAM_SocIspState *pCameraState, CAM_CaptureState eToState );
static CAM_Error _idle2preview( _CAM_SocIspState *pCameraState );
static CAM_Error _preview2idle( _CAM_SocIspState *pCameraState );
static CAM_Error _preview2video( _CAM_SocIspState *pCameraState );
static CAM_Error _preview2still( _CAM_SocIspState *pCameraState );
static CAM_Error _video2preview( _CAM_SocIspState *pCameraState );
static CAM_Error _still2preview( _CAM_SocIspState *pCameraState );

static CAM_Error _get_camera_capability( _CAM_SocIspState *pCameraState, CAM_Int32s iSensorID, CAM_CameraCapability *pCameraCap );
static CAM_Error _get_shotmode_capability( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap );

static CAM_Error _set_sensor_id( _CAM_SocIspState *pCameraState, CAM_Int32s iSensorID );
static CAM_Error _set_jpeg_param( _CAM_SocIspState *pCameraState, CAM_JpegParam *pJpegParam );

static CAM_Error _set_port_size( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_Size *pSize );
static CAM_Error _set_port_format( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageFormat eFormat );
static CAM_Error _get_port_bufreq( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBufferReq *pBufReq );
static CAM_Error _get_required_isp_settings( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, _CAM_ImageInfo *pstIspConfig );

static CAM_Error _enqueue_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBuffer *pImgBuf );
static CAM_Error _dequeue_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _flush_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _dequeue_filled( _CAM_SocIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _process_port_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _adjust_port_bufqueue( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _check_image_buffer( CAM_ImageBuffer *pBuf, CAM_ImageBufferReq *pBufReq );

static CAM_Bool  _is_valid_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Bool  _is_configurable_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );

static CAM_Error _is_need_ppu( _CAM_PortState *pPortState, CAM_Bool *pbNeedPostProcessing, CAM_Bool *pbNeedPrivateBuffer );
static CAM_Error _request_ppu_srcimg( _CAM_SocIspState *pCameraState, CAM_ImageBufferReq *pBufReq, CAM_ImageBuffer *pUsrImgBuf, CAM_ImageBuffer **ppImgBuf );
static CAM_Error _release_ppu_srcimg( _CAM_SocIspState *pCameraState, CAM_ImageBuffer *pImgBuf );
static CAM_Error _flush_all_ppu_srcimg( _CAM_SocIspState *pCameraState );

static CAM_Error _lock_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );
static CAM_Error _unlock_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId );

/* Macros */
#define SEND_EVENT( pState, eEventID, param )\
	do\
	{\
		if ( pState->fnEventHandler != NULL )\
		{\
			pState->fnEventHandler( eEventID, (void*)param, pState->pUserData );\
		}\
		else\
		{\
			TRACE( CAM_WARN, "Warning: can not notify events due to Camera Engine event handler is not set!\n" );\
		}\
	} while ( 0 )

#define SET_SHOT_PARAM( pCameraState, TYPE, PARA_NAME, param1, param2, error )\
do\
{\
	error = isp_set_shotparams( pCameraState->hIspHandle, cmd, param1, param2 );\
	if ( error == CAM_ERROR_NONE )\
	{\
		pCameraState->stShotParamSetting.PARA_NAME = (TYPE)param1;\
	}\
} while ( 0 )

#define GET_SHOT_PARAM( pCameraState, cmd, param1, param2, error )\
do\
{\
	error = isp_get_shotparams( pCameraState->hIspHandle, cmd, param1, param2 );\
	if ( error != CAM_ERROR_NONE )\
	{\
		TRACE( CAM_ERROR, "Error: failed to get shot parameters[%d] ( %s, %s, %d )\n", error, __FILE__, __FUNCTION__, __LINE__ );\
	}\
} while ( 0 )


/* global variables */
// soc-isp entry
const _CAM_DriverEntry entry_socisp =
{
	"camera_socisp",
	_open,
	_close,
	_command,
};


static CAM_Int32s ProcessBufferThread( void *pParam )
{
	CAM_Int32s       ret           = 0;
	CAM_Error        error         = CAM_ERROR_NONE;
	_CAM_SocIspState *pCameraState = (_CAM_SocIspState*)pParam;
	CAM_Int32s       iPortId;
	_CAM_IspPort     ePollPort, eResult;
	CAM_Int32u       usTimeOut;

	for ( ; ; )
	{
		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventWait( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}

		if ( pCameraState->stProcessBufferThread.bExitThread == CAM_TRUE )
		{
			if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck )
			{
				ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck );
				ASSERT( ret == 0 );
			}

			return 0;
		}

		ePollPort = 0;
		if ( (pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW ||
		     pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO ||
		     pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL) &&
		    (pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount >= pCameraState->astPortState[CAM_PORT_PREVIEW].stBufReq.iMinBufCount) )
		{
			ePollPort |= ISP_PORT_DISPLAY;
		}
		if ( (pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO &&
		    (pCameraState->astPortState[CAM_PORT_VIDEO].qEmpty.iQueueCount >= pCameraState->astPortState[CAM_PORT_VIDEO].stBufReq.iMinBufCount)) ||
		    (pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL &&
		    (pCameraState->astPortState[CAM_PORT_STILL].qEmpty.iQueueCount >= pCameraState->astPortState[CAM_PORT_STILL].stBufReq.iMinBufCount)) )
		{
			ePollPort |= ISP_PORT_CODEC;
		}

		if ( ePollPort == 0 )
		{
			// both ports are not available,this case will happen if not enough buffer for
			// ports, so, we need wait for enqueue buffer
			continue;
		}

		usTimeOut = ((2000 << 16) / pCameraState->stShotParamSetting.iFpsQ16);

		// we set the timeout as 2*1000/fps to tolerate some jitter in driver
		error = isp_poll_buffer( pCameraState->hIspHandle, usTimeOut, ePollPort, (CAM_Int32s *)&eResult );
		if ( error == CAM_ERROR_TIMEOUT || (eResult == 0 && error == CAM_ERROR_NONE) )
		{
			if ( error == CAM_ERROR_TIMEOUT )
			{
				TRACE( CAM_WARN, "isp-dma port[%d] poll buffer event time out[%dms]( %s, %s, %d )\n", ePollPort, usTimeOut, __FILE__, __FUNCTION__, __LINE__ );
			}

			if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}
			continue;
		}

		ret = CAM_MutexLock( pCameraState->hCEMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );

		if ( eResult & ISP_PORT_DISPLAY )
		{
			iPortId = CAM_PORT_PREVIEW;

			_lock_port( pCameraState, iPortId );
			error = _process_port_buffer( pCameraState, iPortId );
			_unlock_port( pCameraState, iPortId );
		}

		if ( eResult & ISP_PORT_CODEC )
		{
			if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO || pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL )
			{
				iPortId = ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO ) ? CAM_PORT_VIDEO : CAM_PORT_STILL;

				_lock_port( pCameraState, iPortId );
				error = _process_port_buffer( pCameraState, iPortId );
				_unlock_port( pCameraState, iPortId );
			}
		}

		ret = CAM_MutexUnlock( pCameraState->hCEMutex );
		ASSERT( ret == 0 );

		switch ( error )
		{
		case CAM_ERROR_NONE:
			ret = CAM_EventSet( pCameraState->hEventBufUpdate );
			ASSERT( ret == 0 );

			// set event because there's potential more work to do
			if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
			{
				ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
				ASSERT( ret == 0 );
			}
			break;

		case CAM_ERROR_NOTENOUGHBUFFERS:
			/* This error happens when Camera Engine Client doesn't en-queue enough number of buffers.
			 * When it happens, "Process Buffer Thread" should sleep till Camera Engine Client send new commands.
			 */
			break;

		case CAM_ERROR_PORTNOTACTIVE:
			/* This error happens when Camera Engine is in NULL / IDLE state, or in STILL state while all the required
			 * images have been generated. When it happens, "ProcessBufferThread" should sleep till Camera Engine Client send new commands.
			 */
			break;

		case CAM_ERROR_FATALERROR:
			/* This error happens when camera device met a serious issue and can not automatically recover
			 * the connection with sensor.When it happens, "Process Buffer Thread" should sleep till Camera Engine Client reset the camera device
			 * by change Camera Engine state to IDLE.
			 */
			pCameraState->bIsIspOK = CAM_FALSE;
			break;

		case CAM_ERROR_BUFFERUNAVAILABLE:
			break;

		default:
			ASSERT_CAM_ERROR( error );
			break;
		}
	}

	return 0;
}


static CAM_Error _open( void **ppDeviceData )
{
	CAM_Error          error         = CAM_ERROR_NONE;
	_CAM_SocIspState   *pCameraState = NULL;
	CAM_Int32s         i = 0, ret = 0;

	// BAC check
	if ( ppDeviceData == NULL )
	{
		TRACE( CAM_ERROR, "Error: pointer to camera_socisp handle shouldn't be NULL( %s, %d )\n", __FILE__, __LINE__ );
		return CAM_ERROR_BADPOINTER;
	}

	// allocate soc-isp state internal structure
	pCameraState = (_CAM_SocIspState*)malloc( sizeof( _CAM_SocIspState ) );
	if ( pCameraState == NULL )
	{
		TRACE( CAM_ERROR, "Error: no enough memory for camera_socisp handle( %s, %d )\n", __FILE__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pCameraState, 0, sizeof( _CAM_SocIspState ) );

	// initialize buffer queues of each port
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		ret = _InitQueue( &pCameraState->astPortState[i].qEmpty, CAM_MAX_PORT_BUF_CNT );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: short of memory to create data queue for port[%d]( %s, %s, %d )", i, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}
		ret = _InitQueue( &pCameraState->astPortState[i].qFilled, CAM_MAX_PORT_BUF_CNT );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: short of memory to create data queue for port[%d]( %s, %s, %d )", i, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}

		ret = CAM_MutexCreate( &pCameraState->astPortState[i].hMutex );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: out of resource to create mutex for port[%d]( %s, %s, %d )", i, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}

		ret = CAM_EventCreate( &pCameraState->ahPortBufUpdate[i] );
		if ( ret != 0 )
		{
			TRACE( CAM_ERROR, "Error: out of resource to create event for port[%d]( %s, %s, %d )", i, __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}
		ret = CAM_EventReset( pCameraState->ahPortBufUpdate[i] );
		ASSERT( ret == 0 );
	}

	pCameraState->iSensorID = -1;

	// init isp
	error = isp_init( &pCameraState->hIspHandle );
	ASSERT_CAM_ERROR( error );

	// init ppu
	error = ppu_init( &pCameraState->hPpuHandle );
	ASSERT_CAM_ERROR( error );

	pCameraState->iPpuSrcImgAllocateCnt = 0;

	// enum sensor
	error = isp_enum_sensor( &pCameraState->iSensorCount, pCameraState->astSensorProp );
	ASSERT_CAM_ERROR( error );

	// create process buffer thread
	ret = CAM_EventCreate( &pCameraState->hEventBufUpdate );
	ASSERT( ret == 0 );
	ret = CAM_EventReset( pCameraState->hEventBufUpdate );
	ASSERT( ret == 0 );

	pCameraState->stProcessBufferThread.stThreadAttribute.iDetachState  = CAM_THREAD_CREATE_DETACHED;
	pCameraState->stProcessBufferThread.stThreadAttribute.iPolicy       = CAM_THREAD_POLICY_NORMAL;
	pCameraState->stProcessBufferThread.stThreadAttribute.iPriority     = CAM_THREAD_PRIORITY_NORMAL;

	ret = CAM_EventCreate( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
	ASSERT( ret == 0 );

	ret = CAM_EventCreate( &pCameraState->stProcessBufferThread.stThreadAttribute.hEventExitAck );
	ASSERT( ret == 0 );
	ret = CAM_ThreadCreate( &pCameraState->stProcessBufferThread, ProcessBufferThread, (void*)pCameraState );
	ASSERT( ret == 0 );

	pCameraState->eCaptureState = CAM_CAPTURESTATE_NULL;
	pCameraState->bIsIspOK      = CAM_TRUE;

	*ppDeviceData = pCameraState;

	// create entity mutex
	ret = CAM_MutexCreate( &pCameraState->hCEMutex );
	ASSERT( ret == 0 );

	return error;
}


static CAM_Error _close( void **ppDeviceData )
{
	_CAM_SocIspState *pCameraState = NULL;
	CAM_Error        error         = CAM_ERROR_NONE;
	void             *hCEMutex     = NULL;
	CAM_Int32s       i = 0, ret = 0;

	if ( NULL == ppDeviceData || NULL == *ppDeviceData )
	{
		TRACE( CAM_ERROR, "Error: camera_socisp handle shouldn't be NULL ( %s, %d )\n", __FILE__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	pCameraState = (_CAM_SocIspState*)(*ppDeviceData);

	// destroy process buffer thread
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

	ret = CAM_EventDestroy( &pCameraState->hEventBufUpdate );
	ASSERT( ret == 0 );

	hCEMutex = pCameraState->hCEMutex;

	ret = CAM_MutexLock( hCEMutex, CAM_INFINITE_WAIT, NULL );
	ASSERT( ret == 0 );

	// deinit isp
	error = isp_deinit( &pCameraState->hIspHandle );
	ASSERT_CAM_ERROR( error );

	// deinit ppu
	error = ppu_deinit( &pCameraState->hPpuHandle );
	ASSERT_CAM_ERROR( error );

	// deinit data queue on each port
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		_DeinitQueue( &pCameraState->astPortState[i].qEmpty );

		_DeinitQueue( &pCameraState->astPortState[i].qFilled );

		CAM_MutexDestroy( &pCameraState->astPortState[i].hMutex );

		ret = CAM_EventDestroy( &pCameraState->ahPortBufUpdate[i] );
		ASSERT( ret == 0 );
	}

	free( *ppDeviceData );
	*ppDeviceData = NULL;

	ret = CAM_MutexUnlock( hCEMutex );
	ASSERT( ret == 0 );

	ret = CAM_MutexDestroy( &hCEMutex );
	ASSERT( ret == 0 );

	return error;
}

static CAM_Error _command( void *pDeviceData,
                           CAM_Command cmd,
                           CAM_Param param1, CAM_Param param2 )
{
	CAM_Error              error         = CAM_ERROR_NONE;
	CAM_Int32s             iPortId       = -2;
	CAM_Int32s             ret           = 0;
	_CAM_SocIspState       *pCameraState = (_CAM_SocIspState*)pDeviceData;
	CAM_ShotModeCapability *pShotModeCap = &pCameraState->stShotModeCap;

	if ( cmd != CAM_CMD_ENUM_SENSORS && cmd != CAM_CMD_QUERY_CAMERA_CAP && cmd != CAM_CMD_PORT_DEQUEUE_BUF )
	{
		_CHECK_BAD_POINTER( pCameraState );

		ret = CAM_MutexLock( pCameraState->hCEMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );
	}

	switch ( cmd )
	{
	////////////////////////////////////////////////////////////////////////////////
	// initialization
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_ENUM_SENSORS:
		_CHECK_BAD_POINTER( param1 );
		_CHECK_BAD_POINTER( param2 );

		error = isp_enum_sensor( (CAM_Int32s*)param1, (CAM_DeviceProp*)param2 );
		break;

	case CAM_CMD_QUERY_CAMERA_CAP:
		_CHECK_BAD_POINTER( param2 );

		error = _get_camera_capability( pCameraState, (CAM_Int32s)param1, (CAM_CameraCapability*)param2 );
		break;

	case CAM_CMD_SET_EVENTHANDLER:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE );

		/* NOTE: no need to check the pointer validity. If param1 == NULL, it means that
		 * user want to remove the previously registered event handler
		 */
		pCameraState->fnEventHandler = (CAM_EventHandler)param1;
		pCameraState->pUserData      = (void*)param2;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_SENSOR_ID:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_NULL );

		_lock_port( pCameraState, CAM_PORT_ANY );
		error = _set_sensor_id( pCameraState, (CAM_Int32s)param1 );
		_unlock_port( pCameraState, CAM_PORT_ANY );
		break;

	case CAM_CMD_GET_SENSOR_ID:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY );

		*(CAM_Int32s*)param1 = (CAM_Int32s)pCameraState->iSensorID ;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_SENSOR_ORIENTATION:
		SET_SHOT_PARAM( pCameraState, CAM_FlipRotate, eSensorRotation, param1, param2, error );
		break;

	case CAM_CMD_GET_SENSOR_ORIENTATION:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FlipRotate*)param1 = pCameraState->stShotParamSetting.eSensorRotation;
		error = CAM_ERROR_NONE;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// state machine
	////////////////////////////////////////////////////////////////////////////////

	case CAM_CMD_SET_STATE:
		// wake-up process buffer thread since port may become active
		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventReset( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}

		_lock_port( pCameraState, CAM_PORT_ANY );
		error = _set_camera_state( pCameraState, (CAM_CaptureState)param1 );
		_unlock_port( pCameraState, CAM_PORT_ANY );

		if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_IDLE && pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
		break;

	case CAM_CMD_GET_STATE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_CaptureState*)param1 = pCameraState->eCaptureState;
		error = CAM_ERROR_NONE;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// buffer management
	////////////////////////////////////////////////////////////////////////////////

	case CAM_CMD_PORT_GET_BUFREQ:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		*(CAM_ImageBufferReq*)param2 = pCameraState->astPortState[iPortId].stBufReq;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_PORT_ENQUEUE_BUF:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		_lock_port( pCameraState, iPortId );
		error = _enqueue_buffer( pCameraState, iPortId, (CAM_ImageBuffer*)param2 );
		_unlock_port( pCameraState, iPortId );

		if ( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup )
		{
			ret = CAM_EventSet( pCameraState->stProcessBufferThread.stThreadAttribute.hEventWakeup );
			ASSERT( ret == 0 );
		}
		break;

	case CAM_CMD_PORT_DEQUEUE_BUF:
		error = _dequeue_buffer( pCameraState, (CAM_Int32s*)param1, (CAM_ImageBuffer**)param2 );
		break;

	case CAM_CMD_PORT_FLUSH_BUF:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_ANY_PORT_ID( iPortId );

		_lock_port( pCameraState, (CAM_Int32s)param1 );
		error = _flush_buffer( pCameraState, iPortId );
		_unlock_port( pCameraState, (CAM_Int32s)param1 );
		break;

	////////////////////////////////////////////////////////////////////////////////
	// data port control
	////////////////////////////////////////////////////////////////////////////////

	case CAM_CMD_PORT_SET_SIZE:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		_lock_port( pCameraState, iPortId );
		error = _set_port_size( pCameraState, iPortId, (CAM_Size*)param2 );
		_unlock_port( pCameraState, iPortId );
		break;

	case CAM_CMD_PORT_GET_SIZE:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		((CAM_Size*)param2)->iWidth  = pCameraState->astPortState[iPortId].iWidth;
		((CAM_Size*)param2)->iHeight = pCameraState->astPortState[iPortId].iHeight;

		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_PORT_SET_FORMAT:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );

		_lock_port( pCameraState, iPortId );
		error = _set_port_format( pCameraState, iPortId, (CAM_ImageFormat)param2 );
		_unlock_port( pCameraState, iPortId );
		break;

	case CAM_CMD_PORT_GET_FORMAT:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		*(CAM_ImageFormat*)param2 = pCameraState->astPortState[iPortId].eFormat;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_PORT_SET_ROTATION:
		iPortId = (CAM_Int32s)param1;

		// TODO
		error = CAM_ERROR_NOTIMPLEMENTED;
		break;

	case CAM_CMD_PORT_GET_ROTATION:
		iPortId = (CAM_Int32s)param1;
		_CHECK_BAD_SINGLE_PORT_ID( iPortId );
		_CHECK_BAD_POINTER( param2 );

		*(CAM_FlipRotate*)param2 = pCameraState->astPortState[iPortId].eRotation;
		error = CAM_ERROR_NONE;
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
		error = CAM_ERROR_NONE;
		break;

	////////////////////////////////////////////////////////////////////////////////
	// frame rate
	////////////////////////////////////////////////////////////////////////////////
	case CAM_CMD_SET_FRAMERATE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState,  CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );

		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iFpsQ16, param1, param2, error );
		break;

	case CAM_CMD_GET_FRAMERATE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		GET_SHOT_PARAM( pCameraState, cmd, param1, param2, error );
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
	case CAM_CMD_QUERY_SHOTMODE_CAP:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY );
		_CHECK_SUPPORT_ENUM( (CAM_ShotMode)param1, pCameraState->stCameraCap.eSupportedShotMode, pCameraState->stCameraCap.iSupportedShotModeCnt );
		_CHECK_BAD_POINTER( param2 );

		error = _get_shotmode_capability( pCameraState->iSensorID, (CAM_ShotMode)param1, (CAM_ShotModeCapability*)param2 );
		break;

	case CAM_CMD_SET_SHOTMODE:
		SET_SHOT_PARAM( pCameraState, CAM_ShotMode, eShotMode, param1, param2, error );
		break;

	case CAM_CMD_GET_SHOTMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ShotMode*)param1 = pCameraState->stShotParamSetting.eShotMode;
		error = CAM_ERROR_NONE;
		break;

	// video sub-mode
	case CAM_CMD_SET_VIDEO_SUBMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_SUPPORT_ENUM( (CAM_VideoSubMode)param1, pShotModeCap->eSupportedVideoSubMode, pShotModeCap->iSupportedVideoSubModeCnt );

		SET_SHOT_PARAM( pCameraState, CAM_VideoSubMode, eVideoSubMode, param1, param2, error );
		break;

	case CAM_CMD_GET_VIDEO_SUBMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_VideoSubMode*)param1 = pCameraState->stShotParamSetting.eVideoSubMode;
		break;

	// still sub-mode
	case CAM_CMD_SET_STILL_SUBMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW );
		_CHECK_SUPPORT_ENUM( (CAM_StillSubMode)param1, pShotModeCap->eSupportedStillSubMode, pShotModeCap->iSupportedStillSubModeCnt );

		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW &&
		     (CAM_StillSubMode)param1 == CAM_STILLSUBMODE_ZSL )
		{
			// configure codec port for ZSL, such as image width and height.
			_CAM_ImageInfo  stIspConfig;

			memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
			_lock_port( pCameraState, CAM_PORT_STILL );
			error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
			ASSERT_CAM_ERROR( error );

			error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_PREVIEW );
			ASSERT_CAM_ERROR( error );
			_unlock_port( pCameraState, CAM_PORT_STILL );
		}

		SET_SHOT_PARAM( pCameraState, CAM_StillSubMode, eStillSubMode, param1, param2, error );

		if ( error == CAM_ERROR_NONE )
		{
			pCameraState->stShotParamSetting.stStillSubModeParam = *(CAM_StillSubModeParam*)param2;
		}
		break;

	case CAM_CMD_GET_STILL_SUBMODE:
		_CHECK_BAD_POINTER( param1 );
		*(CAM_StillSubMode*)param1 = pCameraState->stShotParamSetting.eStillSubMode;

		if ( param2 != NULL )
		{
			*(CAM_StillSubModeParam*)param2 = pCameraState->stShotParamSetting.stStillSubModeParam;
		}
		break;

	case CAM_CMD_SET_OPTZOOM:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iOptZoomQ16, param1, param2, error );
		break;

	case CAM_CMD_GET_OPTZOOM:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iOptZoomQ16;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_DIGZOOM:
		_CHECK_SUPPORT_RANGE( (CAM_Int32s)param1, pCameraState->stShotModeCap.iMinDigZoomQ16, pCameraState->stShotModeCap.iMaxDigZoomQ16 );

		if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
		{
			pCameraState->stShotParamSetting.iDigZoomQ16 = (CAM_Int32s)param1;
		}
		else
		{
			SET_SHOT_PARAM( pCameraState, CAM_Int32s, iDigZoomQ16, param1, param2, error );
		}
		break;

	case CAM_CMD_GET_DIGZOOM:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iDigZoomQ16;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_BANDFILTER:
		SET_SHOT_PARAM( pCameraState, CAM_BandFilterMode, eBandFilterMode, param1, param2, error );
		break;

	case CAM_CMD_GET_BANDFILTER:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_BandFilterMode*)param1 = pCameraState->stShotParamSetting.eBandFilterMode;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_GET_EXPOSUREMODE:
		_CHECK_BAD_POINTER( param1 );
		*(CAM_ExposureMode*)param1 = pCameraState->stShotParamSetting.eExpMode;
		break;

	case CAM_CMD_SET_EXPOSUREMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_ExposureMode)param1, pShotModeCap->eSupportedExpMode, pShotModeCap->iSupportedExpModeCnt );

		SET_SHOT_PARAM( pCameraState, CAM_ExposureMode, eExpMode, param1, param2, error );
		break;

	case CAM_CMD_SET_EXPOSUREMETERMODE:
		SET_SHOT_PARAM( pCameraState, CAM_ExposureMeterMode, eExpMeterMode, param1, param2, error );
		break;

	case CAM_CMD_GET_EXPOSUREMETERMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ExposureMeterMode*)param1 = pCameraState->stShotParamSetting.eExpMeterMode;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_ISO:
		SET_SHOT_PARAM( pCameraState, CAM_ISOMode, eIsoMode, param1, param2, error );
		break;

	case CAM_CMD_GET_ISO:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ISOMode*)param1 = pCameraState->stShotParamSetting.eIsoMode;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_EVCOMPENSATION:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iEvCompQ16, param1, param2, error );
		break;

	case CAM_CMD_GET_EVCOMPENSATION:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iEvCompQ16;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_SHUTTERSPEED:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iShutterSpeedQ16, param1, param2, error );
		break;

	case CAM_CMD_GET_SHUTTERSPEED:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iShutterSpeedQ16;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_FNUM:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iFNumQ16, param1, param2, error );
		break;

	case CAM_CMD_GET_FNUM:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iFNumQ16;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_WHITEBALANCEMODE:
		SET_SHOT_PARAM( pCameraState, CAM_WhiteBalanceMode, eWBMode, param1, param2, error );
		break;

	case CAM_CMD_GET_WHITEBALANCEMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_WhiteBalanceMode*)param1 = pCameraState->stShotParamSetting.eWBMode;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_COLOREFFECT:
		SET_SHOT_PARAM( pCameraState, CAM_ColorEffect, eColorEffect, param1, param2, error );
		break;

	case CAM_CMD_GET_COLOREFFECT:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_ColorEffect*)param1 = pCameraState->stShotParamSetting.eColorEffect;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_FOCUSMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_FocusMode)param1,pShotModeCap->eSupportedFocusMode, pShotModeCap->iSupportedFocusModeCnt );

		SET_SHOT_PARAM( pCameraState, CAM_FocusMode, eFocusMode, param1, param2, error );
		break;

	case CAM_CMD_GET_FOCUSMODE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FocusMode*)param1 = pCameraState->stShotParamSetting.eFocusMode;
		break;

	case CAM_CMD_SET_FOCUSZONE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_IDLE | CAM_CAPTURESTATE_PREVIEW | CAM_CAPTURESTATE_VIDEO );
		_CHECK_SUPPORT_ENUM( (CAM_FocusZoneMode)param1, pShotModeCap->eSupportedFocusZoneMode, pShotModeCap->iSupportedFocusZoneModeCnt );

		if ( pCameraState->stShotParamSetting.eFocusMode != CAM_FOCUS_AUTO_ONESHOT &&
		     pCameraState->stShotParamSetting.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_PICTURE &&
		     pCameraState->stShotParamSetting.eFocusMode != CAM_FOCUS_AUTO_CONTINUOUS_VIDEO )
		{
			return CAM_ERROR_NOTSUPPORTEDCMD;
		}

		SET_SHOT_PARAM( pCameraState, CAM_FocusZoneMode, eFocusZoneMode, param1, param2, error );
		break;

	case CAM_CMD_GET_FOCUSZONE:
		_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FocusZoneMode*)param1 = pCameraState->stShotParamSetting.eFocusZoneMode;
		if ( pCameraState->stShotParamSetting.eFocusZoneMode == CAM_FOCUSZONEMODE_MANUAL && param2 != NULL )
		{
			*(CAM_MultiROI*)param2 = pCameraState->stShotParamSetting.stFocusROI;
		}
		break;

	case CAM_CMD_START_FOCUS:
		SET_SHOT_PARAM( pCameraState, CAM_FocusMode, eFocusMode, param1, param2, error );
		break;

	case CAM_CMD_CANCEL_FOCUS:
		// TODO
		break;

	case CAM_CMD_SET_FLASHMODE:
		SET_SHOT_PARAM( pCameraState, CAM_FlashMode, eFlashMode, param1, param2, error );
		break;

	case CAM_CMD_GET_FLASHMODE:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_FlashMode*)param1 = pCameraState->stShotParamSetting.eFlashMode;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_SATURATION:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iSaturation, param1, param2, error );
		break;

	case CAM_CMD_GET_SATURATION:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iSaturation;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_CONTRAST:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iContrast, param1, param2, error );
		break;

	case CAM_CMD_GET_CONTRAST:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iContrast;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_BRIGHTNESS:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iBrightness, param1, param2, error );
		break;

	case CAM_CMD_GET_BRIGHTNESS:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iBrightness;
		error = CAM_ERROR_NONE;
		break;

	case CAM_CMD_SET_SHARPNESS:
		SET_SHOT_PARAM( pCameraState, CAM_Int32s, iSharpness, param1, param2, error );
		break;

	case CAM_CMD_GET_SHARPNESS:
		_CHECK_BAD_POINTER( param1 );

		*(CAM_Int32s*)param1 = pCameraState->stShotParamSetting.iSharpness;
		error = CAM_ERROR_NONE;
		break;

	default:
		TRACE( CAM_ERROR, "Error: unrecognized command ID[%d]( %s, %d )\n", cmd, __FILE__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
		break;
	}

	if ( cmd != CAM_CMD_ENUM_SENSORS && cmd != CAM_CMD_QUERY_CAMERA_CAP && cmd != CAM_CMD_PORT_DEQUEUE_BUF )
	{
		ret = CAM_MutexUnlock( pCameraState->hCEMutex );
		ASSERT( ret == 0 );
	}

	return error;
}

static CAM_Error _set_sensor_id( _CAM_SocIspState *pCameraState, CAM_Int32s iSensorID )
{
	CAM_Error  error = CAM_ERROR_NONE;
	CAM_Int32s i;

	// port
	_CAM_PortState *pPreviewPort = &pCameraState->astPortState[CAM_PORT_PREVIEW];
	_CAM_PortState *pStillPort   = &pCameraState->astPortState[CAM_PORT_STILL];
	_CAM_PortState *pVideoPort   = &pCameraState->astPortState[CAM_PORT_VIDEO];

	if ( iSensorID < 0 || iSensorID >= pCameraState->iSensorCount )
	{
		TRACE( CAM_ERROR, "Error: set sensor ID[%d] out of range[ 0, %d )( %s, %d )\n", iSensorID, pCameraState->iSensorCount, __FILE__, __LINE__ );
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	// set sensor ID to isp
	error = isp_select_sensor( pCameraState->hIspHandle, iSensorID );
	ASSERT_CAM_ERROR( error );

	GET_SHOT_PARAM( pCameraState, CAM_CMD_GET_FRAMERATE, &pCameraState->stShotParamSetting.iFpsQ16, NULL, error );

	// query camera capabilities
	error = _get_camera_capability( pCameraState, iSensorID, &pCameraState->stCameraCap );
	ASSERT_CAM_ERROR( error );

	// configure default output stream
	// previw port
	pPreviewPort->iWidth    = 320;
	pPreviewPort->iHeight   = 240;
	pPreviewPort->eFormat   = pCameraState->stCameraCap.stPortCapability[CAM_PORT_PREVIEW].eSupportedFormat[0];
	pPreviewPort->eRotation = CAM_FLIPROTATE_NORMAL;

	// video port
	pVideoPort->iWidth     = 320;
	pVideoPort->iHeight    = 240;
	pVideoPort->eFormat    = pCameraState->stCameraCap.stPortCapability[CAM_PORT_VIDEO].eSupportedFormat[0];
	pVideoPort->eRotation  = CAM_FLIPROTATE_NORMAL;

	// still port
	pStillPort->iWidth            = pCameraState->stCameraCap.stPortCapability[CAM_PORT_STILL].stMax.iWidth;
	pStillPort->iHeight           = pCameraState->stCameraCap.stPortCapability[CAM_PORT_STILL].stMax.iHeight;
	pStillPort->eFormat           = pCameraState->stCameraCap.stPortCapability[CAM_PORT_STILL].eSupportedFormat[0];
	pStillPort->eRotation         = CAM_FLIPROTATE_NORMAL;
	pCameraState->iStillRestCount = 0;

	// set default JPEG params
	pCameraState->stJpegParam.iSampling      = 1; // 0 - 420, 1 - 422, 2 - 444
	pCameraState->stJpegParam.iQualityFactor = 80;
	pCameraState->stJpegParam.bEnableExif    = CAM_FALSE;
	pCameraState->stJpegParam.bEnableThumb   = CAM_FALSE;
	pCameraState->stJpegParam.iThumbWidth    = 0;
	pCameraState->stJpegParam.iThumbHeight   = 0;

	// init buffer requirement of each port
	for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
	{
		error = _get_port_bufreq( pCameraState, i, &pCameraState->astPortState[i].stBufReq );
		ASSERT_CAM_ERROR( error );
	}

	// FIXME: default shot parameters
	pCameraState->stShotParamSetting.eShotMode                     = CAM_SHOTMODE_AUTO;
	pCameraState->stShotParamSetting.stStillSubModeParam.iBurstCnt = 1;
	pCameraState->stShotParamSetting.stStillSubModeParam.iZslDepth = 0;
	pCameraState->stShotParamSetting.iDigZoomQ16                   = (1 << 16);
	pCameraState->stShotParamSetting.eFocusMode                    = CAM_FOCUS_INFINITY;

	error = _get_shotmode_capability( iSensorID, pCameraState->stShotParamSetting.eShotMode, &pCameraState->stShotModeCap );
	ASSERT_CAM_ERROR( error );

	// now in IDLE state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_IDLE;

	return error;
}

static CAM_Error _set_port_size( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_Size *pSize )
{
	CAM_Error error = CAM_ERROR_NONE;
	CAM_Bool  bSizeSupported;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );
	_CHECK_BAD_POINTER( pSize );

	if ( !_is_configurable_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is not configurable( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

	if ( pCameraState->stCameraCap.stPortCapability[iPortId].bArbitrarySize )
	{
		bSizeSupported = pSize->iWidth <= pCameraState->stCameraCap.stPortCapability[iPortId].stMax.iWidth &&
		                 pSize->iHeight <= pCameraState->stCameraCap.stPortCapability[iPortId].stMax.iHeight;
	}
	else
	{
		int i;
		for ( i = 0; i < pCameraState->stCameraCap.stPortCapability[iPortId].iSupportedSizeCnt; i++ )
		{
			if ( pSize->iWidth == pCameraState->stCameraCap.stPortCapability[iPortId].stSupportedSize[i].iWidth &&
			     pSize->iHeight== pCameraState->stCameraCap.stPortCapability[iPortId].stSupportedSize[i].iHeight )
			{
				break;
			}
		}

		bSizeSupported = i < pCameraState->stCameraCap.stPortCapability[iPortId].iSupportedSizeCnt;
	}

	if ( bSizeSupported )
	{
		pCameraState->astPortState[iPortId].iWidth  = pSize->iWidth;
		pCameraState->astPortState[iPortId].iHeight = pSize->iHeight;

		// TODO: update port buffer requirement
		error = _get_port_bufreq( pCameraState, iPortId, &pCameraState->astPortState[iPortId].stBufReq );
		ASSERT_CAM_ERROR( error );

		// update ZSL mode
		if ( iPortId == CAM_PORT_STILL &&
		     pCameraState->stShotParamSetting.eStillSubMode == CAM_STILLSUBMODE_ZSL &&
		     pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
		{
			CAM_StillSubModeParam stStillSubModeParam;
			_CAM_ImageInfo        stIspConfig;

			memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
			// configure codec port for ZSL, such as image width and height.
			error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
			ASSERT_CAM_ERROR( error );

			error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_PREVIEW );
			ASSERT_CAM_ERROR( error );

			stStillSubModeParam = pCameraState->stShotParamSetting.stStillSubModeParam;

			error = isp_set_shotparams( pCameraState->hIspHandle, CAM_CMD_SET_STILL_SUBMODE,
			                            (CAM_Param)pCameraState->stShotParamSetting.eStillSubMode,
			                            (CAM_Param)&stStillSubModeParam );
			ASSERT( error == CAM_ERROR_NONE );
		}

	}
	else
	{
		TRACE( CAM_ERROR, "Error: size[%d * %d] configured to port[%d] is out of range[%d * %d]( %s, %s, %d )\n",
		       pSize->iWidth, pSize->iHeight, iPortId,
		       pCameraState->stCameraCap.stPortCapability[iPortId].stMax.iWidth, pCameraState->stCameraCap.stPortCapability[iPortId].stMax.iHeight,
		       __FILE__, __FUNCTION__, __LINE__ );

		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	return error;
}

static CAM_Error _set_port_format( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageFormat eFormat )
{
	CAM_PortCapability *pPortCap = NULL;
	CAM_Error          error     = CAM_ERROR_NONE;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	// check if required format supported
	pPortCap = &pCameraState->stCameraCap.stPortCapability[iPortId];
	_CHECK_SUPPORT_ENUM( eFormat, pPortCap->eSupportedFormat, pPortCap->iSupportedFormatCnt );

	if ( !_is_configurable_port( pCameraState, iPortId ) )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is not configurable( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PORTNOTCONFIGUREABLE;
	}

	pCameraState->astPortState[iPortId].eFormat = eFormat;

	// update the port buffer requirement
	error = _get_port_bufreq( pCameraState, iPortId, &pCameraState->astPortState[iPortId].stBufReq );
	ASSERT_CAM_ERROR( error );

	return error;
}

static CAM_Error _set_jpeg_param( _CAM_SocIspState *pCameraState, CAM_JpegParam *pJpegParam )
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
		TRACE( CAM_ERROR, "Error: JPEG quality[%d] must between %d ~ %d\n", pJpegParam->iQualityFactor,
		       pCameraState->stCameraCap.stSupportedJPEGParams.iMinQualityFactor,
		       pCameraState->stCameraCap.stSupportedJPEGParams.iMaxQualityFactor );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
	else
	{
		pCameraState->stJpegParam = *pJpegParam;

		// update the port buffer requirement
		error = _get_port_bufreq( pCameraState, (CAM_Int32s)CAM_PORT_STILL, &pCameraState->astPortState[CAM_PORT_STILL].stBufReq );
		ASSERT_CAM_ERROR( error );
	}

	return error;
}

static CAM_Error _get_camera_capability( _CAM_SocIspState *pCameraState, CAM_Int32s iSensorID, CAM_CameraCapability *pCameraCap )
{
	CAM_Error error       = CAM_ERROR_NONE;
	void      *hIspHandle = NULL;

	_CHECK_BAD_POINTER( pCameraCap );

	memset( pCameraCap, 0, sizeof( CAM_CameraCapability ) );

	// query isp capability
	if ( pCameraState == NULL )
	{
		hIspHandle = NULL;
	}
	else
	{
		hIspHandle = pCameraState->hIspHandle;
	}

	// query isp capability first
	error = isp_query_capability( hIspHandle, iSensorID, pCameraCap );
	ASSERT_CAM_ERROR( error );

	// query ppu capability, currently just JPEG encoder
	{
		CAM_JpegCapability stJpegCap;
		error = ppu_query_jpegenc_cap( pCameraCap->stPortCapability[CAM_PORT_STILL].eSupportedFormat, pCameraCap->stPortCapability[CAM_PORT_STILL].iSupportedFormatCnt, &stJpegCap );
		ASSERT_CAM_ERROR( error );

		if ( stJpegCap.bSupportJpeg )
		{
			pCameraCap->stPortCapability[CAM_PORT_STILL].eSupportedFormat[pCameraCap->stPortCapability[CAM_PORT_STILL].iSupportedFormatCnt] = CAM_IMGFMT_JPEG;
			pCameraCap->stPortCapability[CAM_PORT_STILL].iSupportedFormatCnt++;
			pCameraCap->stSupportedJPEGParams = stJpegCap;
		}
	}

	return error;
}

static CAM_Error _get_shotmode_capability( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Error error = CAM_ERROR_NONE;

	// query shotmode capability from isp
	error = isp_query_shotmodecap( iSensorID, eShotMode, pShotModeCap );
	ASSERT_CAM_ERROR( error );

	return error;
}

static CAM_Bool  _is_configurable_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	if ( pCameraState->astPortState[iPortId].qEmpty.iQueueCount  == 0 &&
	     pCameraState->astPortState[iPortId].qFilled.iQueueCount == 0 )
	{
		return CAM_TRUE;
	}
	else
	{
		return CAM_FALSE;
	}
}
static CAM_Bool _is_valid_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	// TODO: check the port consistency here

	return CAM_TRUE;
}

static CAM_Error _get_port_bufreq( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBufferReq *pBufReq )
{
	_CAM_PortState *pPortState = NULL;
	CAM_Error      error       = CAM_ERROR_NONE;
	_CAM_ImageInfo stImgInfo;


	// BAC check
	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pBufReq );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	pPortState = &pCameraState->astPortState[iPortId];

	stImgInfo.iWidth       = pPortState->iWidth;
	stImgInfo.iHeight      = pPortState->iHeight;
	stImgInfo.eFormat      = pPortState->eFormat;
	stImgInfo.eRotation    = pPortState->eRotation;
	stImgInfo.pJpegParam   = &pCameraState->stJpegParam;
	stImgInfo.bIsStreaming = ( iPortId == CAM_PORT_STILL ) ? CAM_FALSE : CAM_TRUE;

	if ( pPortState->eFormat != CAM_IMGFMT_JPEG )
	{
		// get buffer requirement from isp
		error = isp_get_bufreq( pCameraState->hIspHandle, &stImgInfo, pBufReq );
	}
	else
	{
		// get buffer requirement from ppu
		error = ppu_get_bufreq( pCameraState->hPpuHandle, &stImgInfo, pBufReq );
	}

	return error;
}

static CAM_Error _enqueue_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, CAM_ImageBuffer *pImgBuf )
{
	CAM_Int32s ret;
	CAM_Error  error = CAM_ERROR_NONE;

	_CAM_PortState     *pPortState  = &pCameraState->astPortState[iPortId];
	CAM_ImageBufferReq *pPortBufReq = &pPortState->stBufReq;

	// BAC check
	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pImgBuf );
	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	// check whether enqueued buffer compatible to port requirement
	error = _check_image_buffer( pImgBuf, pPortBufReq );
	ASSERT_CAM_ERROR( error );

	// enqueue this buffer to port, and if the port is active now, enqueue this buffer to ISP DMA
	ret = _EnQueue( &pPortState->qEmpty, (void*)pImgBuf );
	if ( ret != 0 )
	{
		TRACE( CAM_ERROR, "Error: port[%d] is too full to accept a new buffer ( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFRESOURCE;
	}

	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && iPortId == CAM_PORT_PREVIEW )
	{
		error = isp_enqueue_buffer( pCameraState->hIspHandle, pImgBuf, ISP_PORT_DISPLAY, CAM_FALSE );

		if ( error != CAM_ERROR_NONE )
		{
			ret = _RemoveQueue( &pPortState->qEmpty, (void*)pImgBuf );
			ASSERT( ret == 0 );
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO && ( iPortId == CAM_PORT_VIDEO || iPortId == CAM_PORT_PREVIEW ) )
	{
		_CAM_IspPort eIspPort = ( iPortId == CAM_PORT_VIDEO ) ? ISP_PORT_CODEC : ISP_PORT_DISPLAY;

		error = isp_enqueue_buffer( pCameraState->hIspHandle, pImgBuf, eIspPort, CAM_FALSE );

		if ( error != CAM_ERROR_NONE )
		{
			ret = _RemoveQueue( &pPortState->qEmpty, (void*)pImgBuf );
			ASSERT( ret == 0 );
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && ( iPortId == CAM_PORT_STILL || iPortId == CAM_PORT_PREVIEW ) )
	{
		_CAM_IspPort eIspPort = ( iPortId == CAM_PORT_STILL ) ? ISP_PORT_CODEC : ISP_PORT_DISPLAY;

		// enqueue to isp only when we need more images
		if ( pCameraState->bNeedPostProcessing && iPortId == CAM_PORT_STILL )
		{
			CAM_ImageBuffer    *pIspImg = NULL;
			CAM_ImageBufferReq stBufReq;
			_CAM_ImageInfo     stIspConfig;

			error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
			ASSERT_CAM_ERROR( error );

			error = isp_get_bufreq( pCameraState->hIspHandle, &stIspConfig, &stBufReq );
			ASSERT_CAM_ERROR( error );

			error = _request_ppu_srcimg( pCameraState, &stBufReq, pImgBuf, &pIspImg );
			if ( error != CAM_ERROR_NONE )
			{
				TRACE( CAM_ERROR, "Error: no memory available to afford ppu-src buffer ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

				ret = _RemoveQueue( &pCameraState->astPortState[iPortId].qEmpty, (void*)pImgBuf );
				ASSERT( ret == 0 );
			}
			else
			{
				error = isp_enqueue_buffer( pCameraState->hIspHandle, pIspImg, eIspPort, CAM_FALSE );
				if ( error != CAM_ERROR_NONE )
				{
					TRACE( CAM_ERROR, "Error: isp is too full to accept a new buffer ( %s, %s, %d )\n",
					       __FILE__, __FUNCTION__, __LINE__ );

					ret = _RemoveQueue( &pCameraState->astPortState[iPortId].qEmpty, (void*)pImgBuf );
					ASSERT( ret == 0 );

					error = _release_ppu_srcimg( pCameraState, pIspImg );
					ASSERT_CAM_ERROR( error );
				}
			}
		}
		else
		{
			error = isp_enqueue_buffer( pCameraState->hIspHandle, pImgBuf, eIspPort, CAM_FALSE );
			if ( error != CAM_ERROR_NONE )
			{
				TRACE( CAM_ERROR, "Error: isp is too full to accept a new buffer ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

				ret = _RemoveQueue( &pPortState->qEmpty, (void*)pImgBuf );
				ASSERT( ret == 0 );
			}
		}
	}

	return error;
}

static CAM_Error _dequeue_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error  error     = CAM_ERROR_NONE;
	CAM_Int32s ret       = 0;
	CAM_Int32s iLockPort;
	CAM_Int32s i         = 0;
	CAM_Int32s iTimeOut  = 0;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_BAD_POINTER( pPortId );
	_CHECK_BAD_POINTER( ppImgBuf );

	_CHECK_BAD_ANY_PORT_ID( *pPortId );

	iLockPort = *pPortId;

	_lock_port( pCameraState, iLockPort );
	error = _dequeue_filled( pCameraState, pPortId, ppImgBuf );
	_unlock_port( pCameraState, iLockPort );

	if ( error == CAM_ERROR_BUFFERUNAVAILABLE )
	{
		while ( i < 2 )
		{
			if ( !pCameraState->bIsIspOK )
			{
				TRACE( CAM_ERROR, "Error: dequeue buffer failed by ISP hang, please reopen camera engine to recover ( %s, %s, %d )\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_FATALERROR;
			}

			if ( iLockPort != CAM_PORT_ANY )
			{
				ret = CAM_EventWait( pCameraState->ahPortBufUpdate[iLockPort], 1000, &iTimeOut );
				ASSERT( ret == 0 );
			}
			else
			{
				ret = CAM_EventWait( pCameraState->hEventBufUpdate, 1000, &iTimeOut );
				ASSERT( ret == 0 );
			}

			if ( iTimeOut )
			{
				i++;
				continue;
			}

			_lock_port( pCameraState, iLockPort );
			error = _dequeue_filled( pCameraState, pPortId, ppImgBuf );
			_unlock_port( pCameraState, iLockPort );
			if ( error == CAM_ERROR_BUFFERUNAVAILABLE )
			{
				i++;
				continue;
			}
			else
			{
				break;
			}
		}

		if ( i >= 2 )
		{
			error = CAM_ERROR_TIMEOUT;
		}
	}

	return error;
}

static CAM_Error _flush_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Error    error    = CAM_ERROR_NONE;
	CAM_Int32s   i        = 0;
	_CAM_IspPort eIspPort;

	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_STATE_LIMIT( pCameraState->eCaptureState, CAM_CAPTURESTATE_ANY & ~CAM_CAPTURESTATE_STANDBY & ~CAM_CAPTURESTATE_NULL );
	_CHECK_BAD_ANY_PORT_ID( iPortId );

	if ( iPortId == CAM_PORT_ANY )
	{
		if ( pCameraState->eCaptureState != CAM_CAPTURESTATE_IDLE && pCameraState->eCaptureState != CAM_CAPTURESTATE_NULL )
		{
			error = isp_flush_buffer( pCameraState->hIspHandle, ISP_PORT_BOTH );
			ASSERT_CAM_ERROR( error );
		}

		for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
		{
			_FlushQueue( &pCameraState->astPortState[i].qEmpty );
			_FlushQueue( &pCameraState->astPortState[i].qFilled );
		}

		error = CAM_ERROR_NONE;
	}
	else if ( ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && iPortId == CAM_PORT_PREVIEW ) ||
	          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO && ( iPortId == CAM_PORT_VIDEO || iPortId == CAM_PORT_PREVIEW ) ) ||
	          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && ( iPortId == CAM_PORT_STILL || iPortId == CAM_PORT_PREVIEW ) ) )
	{
		if ( iPortId == CAM_PORT_PREVIEW )
		{
			eIspPort = ISP_PORT_DISPLAY;
		}
		else
		{
			eIspPort = ISP_PORT_CODEC;
		}

		error = isp_flush_buffer( pCameraState->hIspHandle, eIspPort );
		ASSERT_CAM_ERROR( error );

		_FlushQueue( &pCameraState->astPortState[iPortId].qEmpty );
		_FlushQueue( &pCameraState->astPortState[iPortId].qFilled );

		error = CAM_ERROR_NONE;
	}
	else
	{
		_FlushQueue( &pCameraState->astPortState[iPortId].qEmpty );
		_FlushQueue( &pCameraState->astPortState[iPortId].qFilled );

		error = CAM_ERROR_NONE;
	}

	// reset all ppu buffer
	// TODO: we assume only do post processing on VIDEO and STILL port by now.
	if ( pCameraState->bNeedPostProcessing && ( iPortId != CAM_PORT_PREVIEW ) )
	{
		for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
		{
			pCameraState->abPpuSrcImgUsed[i] = CAM_FALSE;
		}
	}

	return error;
}

/*----------------------------------------------------------------------------------------------
*  Camera Engine's State Machine
*               null
*                |
*               idle -- (standby)
*                |
*     still -- preview -- video
*---------------------------------------------------------------------------------------------*/
static CAM_Error _set_camera_state( _CAM_SocIspState *pCameraState, CAM_CaptureState eToState )
{
	CAM_Error   error = CAM_ERROR_NONE;
	CAM_Int32s  ret;

	// BAC check
	_CHECK_BAD_POINTER( pCameraState );
	_CHECK_STATE_LIMIT( eToState, CAM_CAPTURESTATE_ANY );

	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE )
	{
		switch ( eToState )
		{
		case CAM_CAPTURESTATE_PREVIEW:
			CELOG( "Idle --> Preview ...\n" );
			error = _idle2preview( pCameraState );
			break;

		default:
			error = CAM_ERROR_BADSTATETRANSITION;
			break;
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW )
	{
		switch ( eToState )
		{
		case CAM_CAPTURESTATE_IDLE:
			CELOG( "Preview --> Idle ...\n" );
			error = _preview2idle( pCameraState );

			ret = CAM_EventSet( pCameraState->ahPortBufUpdate[CAM_PORT_PREVIEW] );
			ASSERT( ret == 0 );

			ret = CAM_EventSet( pCameraState->hEventBufUpdate );
			ASSERT( ret == 0 );
			break;

		case CAM_CAPTURESTATE_VIDEO:
			CELOG( "Preview --> Video ...\n" );
			error = _preview2video( pCameraState );
			break;

		case CAM_CAPTURESTATE_STILL:
			CELOG( "Preview --> Still ...\n" );
			error = _preview2still( pCameraState );
			break;

		default:
			error = CAM_ERROR_BADSTATETRANSITION;
			break;
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO )
	{
		switch ( eToState )
		{
		case CAM_CAPTURESTATE_PREVIEW:
			CELOG( "Video --> Preview ...\n" );
			error = _video2preview( pCameraState );

			ret = CAM_EventSet( pCameraState->ahPortBufUpdate[CAM_PORT_VIDEO] );
			ASSERT( ret == 0 );
			break;

		default:
			error = CAM_ERROR_BADSTATETRANSITION;
			break;
		}
	}
	else if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL )
	{
		switch ( eToState )
		{
		case CAM_CAPTURESTATE_PREVIEW:
			CELOG( "Still --> Preview ...\n" );
			error = _still2preview( pCameraState );

			ret = CAM_EventSet( pCameraState->ahPortBufUpdate[CAM_PORT_STILL] );
			ASSERT( ret == 0 );
			break;

		default:
			error = CAM_ERROR_BADSTATETRANSITION;
			break;
		}
	}

	if ( error == CAM_ERROR_NONE )
	{
		CELOG( "State Transition Done\n" );
	}
	else if ( error == CAM_ERROR_BADSTATETRANSITION )
	{
		TRACE( CAM_ERROR, "Error: Unsupported State Transition( %d -> %d )( %s, %s, %d )\n", pCameraState->eCaptureState, eToState, __FILE__, __FUNCTION__, __LINE__ );
	}
	else
	{
		TRACE( CAM_ERROR, "Error: State Transition Failed, error code[%d]\n", error );
	}

	return error;
}

/****************************************** third level functions, which will be called by the second level functions ************************************************/

/* @_dequeue_filled: dequeue a dilled image from the filled queue of indicated port
 *  Three possible results:
 *   1. The given port is not active and has no filled buffer, return CAM_ERROR_PORTNOTACTIVE, and buffer be NULL;
 *   2. The given port has buffer, return the buffer with CAM_ERROR_NONE, regardless if the port is active or not;
 *   3. The given port is active and has no filled buffer, return NULL and CAM_ERROR_NONE.
 */
static CAM_Error _dequeue_filled( _CAM_SocIspState *pCameraState, CAM_Int32s *pPortId, CAM_ImageBuffer **ppImgBuf )
{
	CAM_Error      error         = CAM_ERROR_NONE;
	_CAM_PortState *pstPortState = NULL;
	CAM_Bool       bBufNotEnough = CAM_FALSE;

	_CHECK_BAD_POINTER( pPortId );
	_CHECK_BAD_POINTER( ppImgBuf );
	_CHECK_BAD_POINTER( pCameraState );

	_CHECK_BAD_ANY_PORT_ID( *pPortId );

	*ppImgBuf = NULL;

	if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_IDLE ||
	     pCameraState->eCaptureState == CAM_CAPTURESTATE_STANDBY ||
	     pCameraState->eCaptureState == CAM_CAPTURESTATE_NULL )
	{
		return CAM_ERROR_PORTNOTACTIVE;
	}

	if ( *pPortId == CAM_PORT_PREVIEW || *pPortId == CAM_PORT_VIDEO || *pPortId == CAM_PORT_STILL )
	{
		if ( 0 == _DeQueue( &pCameraState->astPortState[*pPortId].qFilled, (void**)ppImgBuf ) )
		{
			// got a buffer, and return no error
			error = CAM_ERROR_NONE;
		}
		else if ( ( pCameraState->eCaptureState == CAM_CAPTURESTATE_PREVIEW && *pPortId != CAM_PORT_PREVIEW ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO && *pPortId != CAM_PORT_VIDEO && *pPortId != CAM_PORT_PREVIEW ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId != CAM_PORT_STILL && *pPortId != CAM_PORT_PREVIEW ) ||
		          ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL && *pPortId != CAM_PORT_VIDEO && pCameraState->iStillRestCount <= 0 )
		        )
		{
			// got no buffer, and return port not active error code
			TRACE( CAM_ERROR, "Error: port[%d] is not active in state[%d]( %s, %s, %d )\n", *pPortId, pCameraState->eCaptureState,
			       __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_PORTNOTACTIVE;
		}
		else
		{
			pstPortState = &pCameraState->astPortState[*pPortId];
			bBufNotEnough = pstPortState->qFilled.iQueueCount + pstPortState->qEmpty.iQueueCount < pstPortState->stBufReq.iMinBufCount;

			error = bBufNotEnough ? CAM_ERROR_NOTENOUGHBUFFERS : CAM_ERROR_BUFFERUNAVAILABLE;
		}
	}
	else if ( *pPortId == CAM_PORT_ANY )
	{
		if ( 0 == _DeQueue( &pCameraState->astPortState[CAM_PORT_STILL].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_STILL;
			error    = CAM_ERROR_NONE;
		}
		else if ( 0 == _DeQueue( &pCameraState->astPortState[CAM_PORT_VIDEO].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_VIDEO;
			error    = CAM_ERROR_NONE;
		}
		else if ( 0 == _DeQueue( &pCameraState->astPortState[CAM_PORT_PREVIEW].qFilled, (void**)ppImgBuf ) )
		{
			*pPortId = CAM_PORT_PREVIEW;
			error    = CAM_ERROR_NONE;
		}
		else
		{
			CAM_Int32s i;

			// preview port is always available
			pstPortState  = &pCameraState->astPortState[CAM_PORT_PREVIEW];
			bBufNotEnough = pstPortState->qFilled.iQueueCount + pstPortState->qEmpty.iQueueCount < pstPortState->stBufReq.iMinBufCount;

			if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_VIDEO )
			{
				pstPortState = &pCameraState->astPortState[CAM_PORT_VIDEO];
				bBufNotEnough &= pstPortState->qFilled.iQueueCount + pstPortState->qEmpty.iQueueCount < pstPortState->stBufReq.iMinBufCount;
			}

			if ( pCameraState->eCaptureState == CAM_CAPTURESTATE_STILL )
			{
				pstPortState = &pCameraState->astPortState[CAM_PORT_STILL];
				bBufNotEnough &= pstPortState->qFilled.iQueueCount + pstPortState->qEmpty.iQueueCount < pstPortState->stBufReq.iMinBufCount;
			}

			*ppImgBuf = NULL;

			error = bBufNotEnough ? CAM_ERROR_NOTENOUGHBUFFERS : CAM_ERROR_BUFFERUNAVAILABLE;
		}
	}

	return error;
}

static CAM_Error _process_port_buffer( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Int32s      ret       = 0, i;
	CAM_Error       error     = CAM_ERROR_NONE;
	_CAM_IspPort    eIspPort  = ISP_PORT_BOTH;
	CAM_ImageBuffer *pIspImg  = NULL, *pPortImg = NULL;
	_CAM_PortState  *pPortState;
	CAM_Bool        bIsError;

	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	pPortState = &pCameraState->astPortState[iPortId];

	/* overwrite filled image when there's no enough number of empty buffers
	 * we must have enough empty buffers on preview port to start the processing
	 */
	error = _adjust_port_bufqueue( pCameraState, iPortId );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	if ( iPortId == CAM_PORT_PREVIEW )
	{
		eIspPort = ISP_PORT_DISPLAY;
	}
	else
	{
		eIspPort = ISP_PORT_CODEC;
	}

	// get buffer from isp
	bIsError = CAM_FALSE;
	error = isp_dequeue_buffer( pCameraState->hIspHandle, eIspPort, &pIspImg, &bIsError );
	if ( error != CAM_ERROR_NONE || pIspImg == NULL )
	{
		return error;
	}

	if ( iPortId == CAM_PORT_STILL )
	{
		SEND_EVENT( pCameraState, CAM_EVENT_STILL_SHUTTERCLOSED, 0 );

		if ( pCameraState->bNeedPostProcessing )
		{
			_CAM_PostProcessParam stPostProcessParam;

			stPostProcessParam.bFavorAspectRatio  = pCameraState->bPreviewFavorAspectRatio;
			stPostProcessParam.iPpuDigitalZoomQ16 = 1 << 16;
			stPostProcessParam.eRotation          = pPortState->eRotation;
			stPostProcessParam.pJpegParam         = &pCameraState->stJpegParam;
			stPostProcessParam.pUsrData           = NULL;
			stPostProcessParam.bIsInPlace         = ( ( pCameraState->bUsePrivateBuffer == CAM_FALSE ) ? CAM_TRUE : CAM_FALSE );

			if ( stPostProcessParam.bIsInPlace )
			{
				ret = _RemoveQueue( &pPortState->qEmpty, pIspImg->pUserData );
				ASSERT( ret == 0 );

				pPortImg = (CAM_ImageBuffer*)( pIspImg->pUserData );
			}
			else
			{
				ret = _DeQueue( &pPortState->qEmpty, (void**)&pPortImg );
				ASSERT( ret == 0 && pPortImg != NULL );
			}

#if defined( CAM_PERF )
			{
				CAM_Tick t = -CAM_TimeGetTickCount();
#endif
				error = ppu_image_process( pCameraState->hPpuHandle, pIspImg, pPortImg, &stPostProcessParam );
				ASSERT_CAM_ERROR( error );

#if defined( CAM_PERF )
				t += CAM_TimeGetTickCount();
				CELOG( "Perf: format conversion[%d -> %d], resize[(%d * %d) -> (%d * %d)], rotation[%d] cost %.2f ms\n",
				        pSensorImg->eFormat, pPortImg->eFormat,
				        pSensorImg->iWidth, pSensorImg->iHeight,
				        pStillPort->iWidth, pStillPort->iHeight,
				        pStillPort->eRotation,
				        t / 1000.0 );
			}
#endif
			if ( pPortImg->bEnableShotInfo )
			{
				pPortImg->stShotInfo    = pIspImg->stShotInfo;
				pPortImg->stRawShotInfo = pIspImg->stRawShotInfo;
			}

			pPortImg->tTick = pIspImg->tTick;

			error = _release_ppu_srcimg( pCameraState, pIspImg );
			ASSERT_CAM_ERROR( error );
		}
		else
		{
			// remove from the empty queue
			ret = _RemoveQueue( &pPortState->qEmpty, pIspImg );
			ASSERT( ret == 0 );

			pPortImg = pIspImg;
		}

		ret = _EnQueue( &pPortState->qFilled, pPortImg );
		ASSERT( ret == 0 );

		SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, iPortId );

		pCameraState->iStillRestCount--;
		if ( pCameraState->iStillRestCount <= 0 )
		{
			// notify that all the images are processed
			SEND_EVENT( pCameraState, CAM_EVENT_STILL_ALLPROCESSED, 0 );
		}
	}
	else
	{
		if ( bIsError )
		{
			error = isp_enqueue_buffer( pCameraState->hIspHandle, pIspImg, eIspPort, CAM_FALSE );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}
			SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, iPortId );

			return error;
		}
		else
		{
			// remove from the empty queue
			ret = _RemoveQueue( &pPortState->qEmpty, pIspImg );
			ASSERT( ret == 0 );

			// put to the filled queue
			ret = _EnQueue( &pPortState->qFilled, pIspImg );
			ASSERT( ret == 0 );

			SEND_EVENT( pCameraState, CAM_EVENT_FRAME_READY, iPortId );
		}
	}

	ret = CAM_EventSet( pCameraState->ahPortBufUpdate[iPortId] );
	ASSERT( ret == 0 );

	return error;
}

static CAM_Error _adjust_port_bufqueue( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_ImageBuffer *pTmpImg = NULL;
	CAM_Error       error    = CAM_ERROR_NONE;

	_CHECK_BAD_SINGLE_PORT_ID( iPortId );

	// CELOG( "There are %d empty  buffers in port %d\n", pCameraState->astPortState[iPortId].qEmpty.iQueueCount, iPortId );
	// CELOG( "There are %d filled buffers in port %d\n", pCameraState->astPortState[iPortId].qFilled.iQueueCount, iPortId );

	while ( pCameraState->astPortState[iPortId].qEmpty.iQueueCount <= pCameraState->astPortState[iPortId].stBufReq.iMinBufCount )
	{
		pTmpImg = NULL;
		TRACE( CAM_ERROR, "Error: there is no enough buffer for port[%d] ( %s, %s, %d )\n", iPortId, __FILE__, __FUNCTION__, __LINE__ );

		/* if there's no enough empty buffer, we'll try to overwrite a filled buffer */
		error = _dequeue_filled( pCameraState, &iPortId, &pTmpImg );
		switch ( error )
		{
		case CAM_ERROR_NONE:
			/* inform about the frame loss */
			SEND_EVENT( pCameraState, CAM_EVENT_FRAME_DROPPED, iPortId );

			/* queue back the filled image */
			error = _enqueue_buffer( pCameraState, iPortId, pTmpImg );
			ASSERT_CAM_ERROR( error );
			break;

		case CAM_ERROR_BUFFERUNAVAILABLE:
			return CAM_ERROR_NOTENOUGHBUFFERS;
			break;

		default:
			return error;
			break;
		}
	}

	return error;
}

/* Camera Engine state machine */
static CAM_Error _idle2preview( _CAM_SocIspState *pCameraState )
{
	CAM_Int32s       i            = 0;
	CAM_Error        error        = CAM_ERROR_NONE;
	_CAM_ImageInfo   stIspConfig;
	CAM_Int32s       ret          = 0;

	// current port
	_CAM_PortState *pPreviewPort = &pCameraState->astPortState[CAM_PORT_PREVIEW];

	// check whether preview port is ready
	if ( !_is_valid_port( pCameraState, CAM_PORT_PREVIEW ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
	// configure isp display port
	error = _get_required_isp_settings( pCameraState, CAM_PORT_PREVIEW, &stIspConfig );
	ASSERT_CAM_ERROR( error );

	error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_DISPLAY, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	if ( pCameraState->stShotParamSetting.eStillSubMode == CAM_STILLSUBMODE_ZSL )
	{
		memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
		// configure isp codec port
		error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
		ASSERT_CAM_ERROR( error );

		error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_PREVIEW );
		ASSERT_CAM_ERROR( error );
	}

	// push preview buffers to isp
	for ( i = 0; i < pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
	{
		CAM_ImageBuffer *pIspImg = NULL;

		ret = _GetQueue( &pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pIspImg );
		ASSERT( ret == 0 );

		error = isp_enqueue_buffer( pCameraState->hIspHandle, pIspImg, ISP_PORT_DISPLAY, CAM_FALSE );
		ASSERT_CAM_ERROR( error );
	}

	// set isp to preview state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// update digital zoom
	error = isp_set_shotparams( pCameraState->hIspHandle, CAM_CMD_SET_DIGZOOM,
	                            (CAM_Param)pCameraState->stShotParamSetting.iDigZoomQ16, NULL );
	ASSERT_CAM_ERROR( error );

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return error;
}

static CAM_Error _preview2idle( _CAM_SocIspState *pCameraState )
{
	CAM_Int32s       ret       = 0;
	CAM_Error        error     = CAM_ERROR_NONE;

	// port
	_CAM_PortState *pPreviewPort = &pCameraState->astPortState[CAM_PORT_PREVIEW];

	// set isp to IDLE state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_IDLE );
	ASSERT_CAM_ERROR( error );

	// flush preview buffers enqueued to isp
	error = isp_flush_buffer( pCameraState->hIspHandle, ISP_PORT_DISPLAY );
	ASSERT_CAM_ERROR( error );

	// queue-back filled preview image to empty queue
	while ( 1 )
	{
		CAM_ImageBuffer *pTmpImg = NULL;

		ret = _DeQueue( &pPreviewPort->qFilled, (void **)&pTmpImg );
		if ( ret != 0 )
		{
			break;
		}

		ret = _EnQueue( &pPreviewPort->qEmpty, (void *)pTmpImg );
		ASSERT( ret == 0 );
	}

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_IDLE;

	return error;
}

static CAM_Error _preview2video( _CAM_SocIspState *pCameraState )
{
	CAM_Int32s     i            = 0;
	CAM_Error      error        = CAM_ERROR_NONE;
	_CAM_ImageInfo stIspConfig;

	// current port: preview port and video port
	_CAM_PortState *pVideoPort = &pCameraState->astPortState[CAM_PORT_VIDEO];

	// check whether preview&video port is ready
	if ( !_is_valid_port( pCameraState, CAM_PORT_PREVIEW ) || !_is_valid_port( pCameraState, CAM_PORT_VIDEO ) )
	{
		return CAM_ERROR_PORTNOTVALID;
	}

	memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
	// configure isp codec port
	error = _get_required_isp_settings( pCameraState, CAM_PORT_VIDEO, &stIspConfig );
	ASSERT_CAM_ERROR( error );

	error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_VIDEO );
	ASSERT_CAM_ERROR( error );

	// push video buffers to isp
	for ( i = 0; i < pCameraState->astPortState[CAM_PORT_VIDEO].qEmpty.iQueueCount; i++ )
	{
		CAM_ImageBuffer  *pTmpImg = NULL;
		CAM_Int32s       ret       = 0;

		ret = _GetQueue( &pCameraState->astPortState[CAM_PORT_VIDEO].qEmpty, i, (void**)&pTmpImg );
		ASSERT( ret == 0 );

		error = isp_enqueue_buffer( pCameraState->hIspHandle, pTmpImg, ISP_PORT_CODEC, CAM_FALSE );
		ASSERT_CAM_ERROR( error );
	}

	// set isp to video state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_VIDEO );
	ASSERT_CAM_ERROR( error );

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_VIDEO;

	return error;
}

static CAM_Error _video2preview( _CAM_SocIspState *pCameraState )
{
	CAM_Error error = CAM_ERROR_NONE;
	_CAM_ImageInfo stIspConfig;

	if ( pCameraState->stShotParamSetting.eStillSubMode == CAM_STILLSUBMODE_ZSL )
	{
		memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );
		// configure isp codec port
		error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
		ASSERT_CAM_ERROR( error );

		error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_PREVIEW );
		ASSERT_CAM_ERROR( error );
	}

	// video port
	_CAM_PortState *pVideoPort = &pCameraState->astPortState[CAM_PORT_VIDEO];

	// set isp to preview state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// flush video buffers enqueued to isp
	error = isp_flush_buffer( pCameraState->hIspHandle, ISP_PORT_CODEC );
	ASSERT_CAM_ERROR( error );

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return error;
}

static CAM_Error _preview2still( _CAM_SocIspState *pCameraState )
{
	CAM_Int32s      i           = 0;
	_CAM_ImageInfo  stIspConfig;
	CAM_Error       error       = CAM_ERROR_NONE;
	CAM_ImageBuffer *pTmpImg    = NULL;
	CAM_Int32s      ret         = 0;

	// current port: preview port, still port
	_CAM_PortState *pPreviewPort = &pCameraState->astPortState[CAM_PORT_PREVIEW];
	_CAM_PortState *pStillPort   = &pCameraState->astPortState[CAM_PORT_STILL];

	memset( &stIspConfig, 0, sizeof(_CAM_ImageInfo) );

	error = _is_need_ppu( pStillPort, &pCameraState->bNeedPostProcessing, &pCameraState->bUsePrivateBuffer );
	ASSERT_CAM_ERROR( error );

	// configure isp codec port
	error = _get_required_isp_settings( pCameraState, CAM_PORT_STILL, &stIspConfig );
	ASSERT_CAM_ERROR( error );

	error = isp_config_output( pCameraState->hIspHandle, &stIspConfig, ISP_PORT_CODEC, CAM_CAPTURESTATE_STILL );
	ASSERT_CAM_ERROR( error );

	// push still buffers
	for ( i = 0; i < pCameraState->astPortState[CAM_PORT_STILL].qEmpty.iQueueCount; i++ )
	{
		CAM_ImageBuffer *pUsrBuf = NULL;
		CAM_ImageBuffer *pIspBuf = NULL;

		ret = _GetQueue( &pCameraState->astPortState[CAM_PORT_STILL].qEmpty, i, (void**)&pUsrBuf );
		ASSERT( ret == 0 );

		if ( pCameraState->bNeedPostProcessing )
		{
			TRACE( CAM_INFO, "Info: post processing unit is used in still state to enable JPEG compressor!\n" );

			CAM_ImageBufferReq stBufReq;
			error = isp_get_bufreq( pCameraState->hIspHandle, &stIspConfig, &stBufReq );
			ASSERT_CAM_ERROR( error );

			error = _request_ppu_srcimg( pCameraState, &stBufReq, pUsrBuf, &pIspBuf );
			ASSERT_CAM_ERROR( error );
		}
		else
		{
			pIspBuf = pUsrBuf;
		}

		error = isp_enqueue_buffer( pCameraState->hIspHandle, pIspBuf, ISP_PORT_CODEC, CAM_FALSE );
		ASSERT_CAM_ERROR( error );
	}

	// clear the preview buffers so that it won't be mixed up with the snapshot images
	error = isp_flush_buffer( pCameraState->hIspHandle, ISP_PORT_DISPLAY );
	ASSERT_CAM_ERROR( error );

	while ( 1 )
	{
		ret = _DeQueue( &pCameraState->astPortState[CAM_PORT_PREVIEW].qFilled, (void**)&pTmpImg );
		if ( ret != 0 )
		{
			break;
		}

		ret = _EnQueue( &pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty, pTmpImg );
		ASSERT( ret == 0 );
	}

	for ( i = 0; i < pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty.iQueueCount; i++ )
	{
		ret = _GetQueue( &pCameraState->astPortState[CAM_PORT_PREVIEW].qEmpty, i, (void**)&pTmpImg );
		ASSERT( ret == 0 );

		error = isp_enqueue_buffer( pCameraState->hIspHandle, pTmpImg, ISP_PORT_DISPLAY, CAM_TRUE );
		ASSERT_CAM_ERROR( error );
	}

	// set isp to still state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_STILL );
	ASSERT_CAM_ERROR( error );

	pCameraState->iStillRestCount += pCameraState->stShotParamSetting.stStillSubModeParam.iBurstCnt;

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_STILL;

	return error;
}

static CAM_Error _still2preview( _CAM_SocIspState *pCameraState )
{
	CAM_Error  error = CAM_ERROR_NONE;

	// preview port
	_CAM_PortState *pPreviewPort = &pCameraState->astPortState[CAM_PORT_PREVIEW];

	// set isp to preview state
	error = isp_set_state( pCameraState->hIspHandle, CAM_CAPTURESTATE_PREVIEW );
	ASSERT_CAM_ERROR( error );

	// flush still buffers enqueued to isp
	error = isp_flush_buffer( pCameraState->hIspHandle, ISP_PORT_CODEC );
	ASSERT_CAM_ERROR( error );

	// flush all ppu src images
	if ( pCameraState->bNeedPostProcessing )
	{
		error = _flush_all_ppu_srcimg( pCameraState );
		ASSERT_CAM_ERROR( error );
	}

	error = _is_need_ppu( pPreviewPort, &pCameraState->bNeedPostProcessing, &pCameraState->bUsePrivateBuffer );
	ASSERT_CAM_ERROR( error );

	// update camera state
	pCameraState->eCaptureState = CAM_CAPTURESTATE_PREVIEW;

	return error;
}

static CAM_Error _get_required_isp_settings( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId, _CAM_ImageInfo *pstIspConfig )
{
	CAM_Error            error       = CAM_ERROR_NONE;
	_CAM_PortState       *pPortState = &pCameraState->astPortState[iPortId];
	CAM_CameraCapability stCameraCap;
	CAM_Int32s           i;
	CAM_ImageFormat      eIspFormat;

	error = isp_query_capability( pCameraState->hIspHandle, pCameraState->iSensorID, &stCameraCap );
	ASSERT_CAM_ERROR( error );

	for ( i = 0; i < stCameraCap.stPortCapability[iPortId].iSupportedFormatCnt; i++ )
	{
		if ( pPortState->eFormat == stCameraCap.stPortCapability[iPortId].eSupportedFormat[i] )
		{
			break;
		}
	}

	if ( i >= stCameraCap.stPortCapability[iPortId].iSupportedFormatCnt )
	{
		// need ppu
		error = ppu_negotiate_format( pPortState->eFormat, &eIspFormat,
		                              stCameraCap.stPortCapability[iPortId].eSupportedFormat,
		                              stCameraCap.stPortCapability[iPortId].iSupportedFormatCnt );
		ASSERT_CAM_ERROR( error );
	}
	else
	{
		eIspFormat = pPortState->eFormat;
	}

	pstIspConfig->eFormat      = eIspFormat;
	pstIspConfig->iWidth       = pPortState->iWidth;
	pstIspConfig->iHeight      = pPortState->iHeight;
	pstIspConfig->eRotation    = pPortState->eRotation;
	pstIspConfig->pJpegParam   = &pCameraState->stJpegParam;
	pstIspConfig->bIsStreaming = ( iPortId == CAM_PORT_STILL ) ? CAM_FALSE : CAM_TRUE;

	return error;
}

/* port protection */
static CAM_Error _lock_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Int32s ret = 0;

	_CHECK_BAD_ANY_PORT_ID( iPortId );

	if ( iPortId == CAM_PORT_ANY )
	{
		CAM_Int32s i = 0;

		for ( i = CAM_PORT_STILL; i >= CAM_PORT_PREVIEW; i-- )
		{
			ret = CAM_MutexLock( pCameraState->astPortState[i].hMutex, CAM_INFINITE_WAIT, NULL );
			ASSERT( ret == 0 );
		}
	}
	else
	{
		ret = CAM_MutexLock( pCameraState->astPortState[iPortId].hMutex, CAM_INFINITE_WAIT, NULL );
		ASSERT( ret == 0 );
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _unlock_port( _CAM_SocIspState *pCameraState, CAM_Int32s iPortId )
{
	CAM_Int32s ret = 0;

	_CHECK_BAD_ANY_PORT_ID( iPortId );

	if ( iPortId == CAM_PORT_ANY )
	{
		CAM_Int32s i   = 0;

		for ( i = CAM_PORT_PREVIEW; i <= CAM_PORT_STILL; i++ )
		{
			ret = CAM_MutexUnlock( pCameraState->astPortState[i].hMutex );
			ASSERT( ret == 0 );
		}
	}
	else
	{
		ret = CAM_MutexUnlock( pCameraState->astPortState[iPortId].hMutex );
		ASSERT( ret == 0 );
	}

	return CAM_ERROR_NONE;
}


static CAM_Error _check_image_buffer( CAM_ImageBuffer *pBuf, CAM_ImageBufferReq *pBufReq )
{
	CAM_Error  error = CAM_ERROR_NONE;
	CAM_Int32s i     = 0;

	_CHECK_BAD_POINTER( pBuf );
	_CHECK_BAD_POINTER( pBufReq );

	if ( ( (pBuf->iFlag & BUF_FLAG_PHYSICALCONTIGUOUS) && (pBuf->iFlag & BUF_FLAG_NONPHYSICALCONTIGUOUS) ) ||
	     ( !(pBuf->iFlag & BUF_FLAG_PHYSICALCONTIGUOUS) && !(pBuf->iFlag & BUF_FLAG_NONPHYSICALCONTIGUOUS) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_PHYSICALCONTIGUOUS and BUF_FLAG_NONPHYSICALCONTIGUOUS must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pBuf->iFlag & BUF_FLAG_L1CACHEABLE) && (pBuf->iFlag & BUF_FLAG_L1NONCACHEABLE) ) ||
	     ( !(pBuf->iFlag & BUF_FLAG_L1CACHEABLE) && !(pBuf->iFlag & BUF_FLAG_L1NONCACHEABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_L1CACHEABLE and BUF_FLAG_L1NONCACHEABLE must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pBuf->iFlag & BUF_FLAG_L2CACHEABLE) &&  (pBuf->iFlag & BUF_FLAG_L2NONCACHEABLE) ) ||
	     ( !(pBuf->iFlag & BUF_FLAG_L2CACHEABLE) && !(pBuf->iFlag & BUF_FLAG_L2NONCACHEABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_L2CACHEABLE and BUF_FLAG_L2NONCACHEABLE must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pBuf->iFlag & BUF_FLAG_BUFFERABLE) &&  (pBuf->iFlag & BUF_FLAG_UNBUFFERABLE) ) ||
	     ( !(pBuf->iFlag & BUF_FLAG_BUFFERABLE) && !(pBuf->iFlag & BUF_FLAG_UNBUFFERABLE) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_BUFFERABLE and BUF_FLAG_UNBUFFERABLE must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( !(
	       ( (pBuf->iFlag & BUF_FLAG_YUVPLANER) && !(pBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) && !(pBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) ) ||
	       (!(pBuf->iFlag & BUF_FLAG_YUVPLANER) &&  (pBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) && !(pBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) ) ||
	       (!(pBuf->iFlag & BUF_FLAG_YUVPLANER) && !(pBuf->iFlag & BUF_FLAG_YUVBACKTOBACK) &&  (pBuf->iFlag & BUF_FLAG_YVUBACKTOBACK) )
	      ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_YUVPLANER and BUF_FLAG_YUVBACKTOBACK and BUF_FLAG_YVUBACKTOBACK must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( ( (pBuf->iFlag & BUF_FLAG_ALLOWPADDING) &&  (pBuf->iFlag & BUF_FLAG_FORBIDPADDING) ) ||
	     ( !(pBuf->iFlag & BUF_FLAG_ALLOWPADDING) && !(pBuf->iFlag & BUF_FLAG_FORBIDPADDING) ) )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( BUF_FLAG_ALLOWPADDING and BUF_FLAG_FORBIDPADDING must be set exclusively )!\n" );
		return CAM_ERROR_BADBUFFER;
	}

	if ( pBuf->iFlag & ~pBufReq->iFlagAcceptable )
	{
		TRACE( CAM_ERROR, "Error: bad buffer( iFlag (= 0x%x) is not acceptable (iFlagAcceptable = 0x%x) )!\n", pBuf->iFlag, pBufReq->iFlagAcceptable );
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
			if ( pBuf->iStep[i] != pBufReq->iMinStep[i] )
			{
				TRACE( CAM_ERROR, "Error: BUF_FLAG_FORBIDPADDING flag is set, hence CAM_ImageBuffer::iStep[%d] (%d) should be equal to \
				       CAM_ImageBufferReq::iMinStep[%d] (%d)!\n", i, pBuf->iStep[i], i, pBufReq->iMinStep[i] );
				return CAM_ERROR_BADBUFFER;
			}
		}
		else
		{
			if ( pBuf->iStep[i] < pBufReq->iMinStep[i] )
			{
				TRACE( CAM_ERROR, "Error: BUF_FLAG_FORBIDPADDING flag is clear, hence CAM_ImageBuffer::iStep[%d] (%d) should be no less than \
				       CAM_ImageBufferReq::iMinStep[%d] (%d)!\n", i, pBuf->iStep[i], i, pBufReq->iMinStep[i] );
				return CAM_ERROR_BADBUFFER;
			}
		}

		// check row align
		if ( pBuf->iStep[i] & (pBufReq->iRowAlign[i] - 1) )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::iStep[%d] (%d) should align to CAM_ImageBufferReq::iRowAlign[%d] (%d)!\n",
			       i, pBuf->iStep[i], i, pBufReq->iRowAlign[i] );
			return CAM_ERROR_BADBUFFER;
		}

		// check allocate len
		if ( pBuf->iAllocLen[i] < pBufReq->iMinBufLen[i] + pBuf->iOffset[i] )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::iAllocLen[%d] (%d) should be no less than \
			       CAM_ImageBufferReq::iMinBufLen[%d] (%d) + CAM_ImageBuffer::iOffset[%d] (%d)!\n", \
			       i, pBuf->iAllocLen[i], i, pBufReq->iMinBufLen[i], i, pBuf->iOffset[i] );
			return CAM_ERROR_BADBUFFER;
		}

		// check pointer
		if ( pBufReq->iMinBufLen[i] > 0 && pBuf->pBuffer[i] == NULL )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::pBuffer[%d] (%p) should point to valid address!\n", i, pBuf->pBuffer[i] );
			return CAM_ERROR_BADBUFFER;
		}

		if ( (CAM_Int32s)(pBuf->pBuffer[i] + pBuf->iOffset[i]) & (pBufReq->iAlignment[i] - 1) )
		{
			TRACE( CAM_ERROR, "Error: CAM_ImageBuffer::pBuffer[%d] (%p) + CAM_ImageBuffer::iOffset[%d] (%d) should align to \
			       CAM_ImageBufferReq::iAlignment[%d] (%d) byte!\n", i, pBuf->pBuffer[i], i, pBuf->iOffset[i], i, pBufReq->iAlignment[i] );
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
			     pBuf->pBuffer[0] + pBuf->iOffset[0] + pBufReq->iMinBufLen[0] != pBuf->pBuffer[1] + pBuf->iOffset[1] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!\n" );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}

			if ( pBufReq->iMinBufLen[2] > 0 &&
			     pBuf->pBuffer[1] + pBuf->iOffset[1] + pBufReq->iMinBufLen[1] != pBuf->pBuffer[2] + pBuf->iOffset[2] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!\n" );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
		else if ( pBufReq->iFlagAcceptable & BUF_FLAG_YVUBACKTOBACK )
		{
			// back to back layout in YVU order
			if ( pBufReq->iMinBufLen[2] > 0 &&
			     pBuf->pBuffer[0] + pBuf->iOffset[0] + pBufReq->iMinBufLen[0] != pBuf->pBuffer[2] + pBuf->iOffset[2] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!\n" );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}

			if ( pBufReq->iMinBufLen[1] > 0 &&
			     pBuf->pBuffer[2] + pBuf->iOffset[2] + pBufReq->iMinBufLen[2] != pBuf->pBuffer[1] + pBuf->iOffset[1] )
			{
				TRACE( CAM_ERROR, "Error: The input buffer must in back-to-back layout!\n" );
				return CAM_ERROR_NOTSUPPORTEDARG;
			}
		}
	}

	return error;
}

/* check whether ppu in needed in current usage case */
static CAM_Error _is_need_ppu( _CAM_PortState *pPortState, CAM_Bool *pbNeedPostProcessing, CAM_Bool *pbNeedPrivateBuffer )
{
	*pbNeedPostProcessing = ( pPortState->eFormat == CAM_IMGFMT_JPEG );

	if ( *pbNeedPostProcessing == CAM_TRUE )
	{
		*pbNeedPrivateBuffer = CAM_TRUE;
	}
	else
	{
		*pbNeedPrivateBuffer = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}


/* ppu buffer management */
static CAM_Error _request_ppu_srcimg( _CAM_SocIspState *pCameraState, CAM_ImageBufferReq *pBufReq, CAM_ImageBuffer *pUsrImgBuf, CAM_ImageBuffer **ppImgBuf )
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
			if ( !pCameraState->abPpuSrcImgUsed[i] && pCameraState->astPpuSrcImg[i].pUserData == pUsrImgBuf )
			{
				break;
			}
		}

		if ( i < CAM_MAX_PORT_BUF_CNT )
		{
			pCameraState->abPpuSrcImgUsed[i] = CAM_TRUE;
			*ppImgBuf                        = &pCameraState->astPpuSrcImg[i];

			return CAM_ERROR_NONE;
		}

		for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
		{
			if ( !pCameraState->abPpuSrcImgUsed[i] && pCameraState->astPpuSrcImg[i].pUserData == NULL )
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

		pTmpImg                = &pCameraState->astPpuSrcImg[i];

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

		pCameraState->abPpuSrcImgUsed[i] = CAM_TRUE;

		*ppImgBuf = pTmpImg;
	}
	else
	{
		// search all allocated private images and try to find one that is not in use
		for ( i = 0; i < pCameraState->iPpuSrcImgAllocateCnt; i++ )
		{
			if ( !pCameraState->abPpuSrcImgUsed[i] )
			{
				break;
			}
		}

		if ( i < pCameraState->iPpuSrcImgAllocateCnt )
		{
			// found! then use it
			*ppImgBuf                        = &pCameraState->astPpuSrcImg[i];
			pCameraState->abPpuSrcImgUsed[i] = CAM_TRUE;

			return CAM_ERROR_NONE;
		}

		// not found, so we need to allocate a new one. and before that, we need to check if we have enough structures to hold the buffer
		if ( i >= CAM_MAX_PORT_BUF_CNT )
		{
			TRACE( CAM_ERROR, "Error: too many buffers to be handled by camera-engine( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFRESOURCE;
		}

		// start to allocate a new priviate image
		pTmpImg                = &pCameraState->astPpuSrcImg[i];

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

		pTmpImg->bEnableShotInfo = pUsrImgBuf->bEnableShotInfo;

		// invalidate bmm buffer's cache line
		osalbmm_flush_cache_range( pTmpImg->pUserData, iTotalLen, OSALBMM_DMA_FROM_DEVICE );

		pCameraState->iPpuSrcImgAllocateCnt++;
		pCameraState->abPpuSrcImgUsed[i] = CAM_TRUE;

		*ppImgBuf = pTmpImg;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error _release_ppu_srcimg( _CAM_SocIspState *pCameraState, CAM_ImageBuffer *pImgBuf )
{
	CAM_Int32s iBufIndex = pImgBuf->iUserIndex;

	ASSERT( pCameraState->bNeedPostProcessing == CAM_TRUE );

	ASSERT( pCameraState->abPpuSrcImgUsed[iBufIndex] == CAM_TRUE );
	ASSERT( &pCameraState->astPpuSrcImg[iBufIndex] == pImgBuf );

	if ( !pCameraState->bUsePrivateBuffer )
	{
		ASSERT( iBufIndex >= 0 && iBufIndex < CAM_MAX_PORT_BUF_CNT );
	}
	else
	{
		ASSERT( iBufIndex >= 0 && iBufIndex < pCameraState->iPpuSrcImgAllocateCnt );
	}

	pCameraState->abPpuSrcImgUsed[iBufIndex] = CAM_FALSE;

	return CAM_ERROR_NONE;
}

static CAM_Error _flush_all_ppu_srcimg( _CAM_SocIspState *pCameraState )
{
	CAM_Int32s i = 0;

	if ( pCameraState->bUsePrivateBuffer && ( pCameraState->iPpuSrcImgAllocateCnt > 0 ) )
	{
		for ( i = pCameraState->iPpuSrcImgAllocateCnt - 1; i >= 0; i-- )
		{
			osalbmm_free( pCameraState->astPpuSrcImg[i].pUserData );
		}
	}

	pCameraState->iPpuSrcImgAllocateCnt = 0;

	for ( i = 0; i < CAM_MAX_PORT_BUF_CNT; i++ )
	{
		memset( &pCameraState->astPpuSrcImg[i], 0, sizeof( CAM_ImageBuffer ) );
		pCameraState->abPpuSrcImgUsed[i] = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}
