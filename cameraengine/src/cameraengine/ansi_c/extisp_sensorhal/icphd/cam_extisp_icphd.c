/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cam_log.h"
#include "cam_utility.h"


#include "cam_extisp_sensorhal.h"
#include "cam_extisp_v4l2base.h"
#include "cam_extisp_icphd.h"

/* NOTE: if you want to enable static resolution table to bypass sensor dynamically detect to save camera-off to viwerfinder-on latency,
 *       you can fill the following four tables according to your sensor's capability. And open macro
 *       BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT in CameraConfig.mk
 */

/* ICPHD video resolution table */
CAM_Size _ICPHDVideoResTable[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT] = {};

/* ICPHD still resolution table */
CAM_Size _ICPHDStillResTable[CAM_MAX_SUPPORT_IMAGE_SIZE_CNT] = {};

/* ICPHD video format table */
CAM_ImageFormat _ICPHDVideoFormatTable[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT] = {};

CAM_ImageFormat _ICPHDStillFormatTable[CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT] = {};

CAM_FlipRotate _ICPHDRotationTable[] =
{
	CAM_FLIPROTATE_NORMAL,
};


CAM_ShotMode _ICPHDShotModeTable[] =
{
	CAM_SHOTMODE_AUTO,
};

_CAM_RegEntry _ICPHDExpMeter_Mean[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDExpMeter[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_EXPOSUREMETERMODE_MEAN, _ICPHDExpMeter_Mean),
};

_CAM_RegEntry _ICPHDIsoMode_Auto[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDIsoMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_ISO_AUTO, _ICPHDIsoMode_Auto),
};

_CAM_RegEntry _ICPHDBdFltMode_Auto[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _ICPHDBdFltMode_Off[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _ICPHDBdFltMode_50Hz[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _ICPHDBdFltMode_60Hz[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDBdFltMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_BANDFILTER_50HZ, _ICPHDBdFltMode_50Hz),
};

_CAM_RegEntry _ICPHDFlashMode_Off[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _ICPHDFlashMode_On[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_RegEntry _ICPHDFlashMode_Auto[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDFlashMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_FLASH_OFF, _ICPHDFlashMode_Off),
};

_CAM_RegEntry _ICPHDWBMode_Auto[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDWBMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_WHITEBALANCEMODE_AUTO, _ICPHDWBMode_Auto),
};

_CAM_RegEntry _ICPHDFocusMode_Infinity[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDFocusMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_FOCUS_INFINITY,  _ICPHDFocusMode_Infinity),
};

_CAM_RegEntry _ICPHDColorEffectMode_Off[] =
{
	{0, 0, 0, 0, 0},
};

_CAM_ParameterEntry _ICPHDColorEffectMode[] =
{
	PARAM_ENTRY_USE_REGTABLE(CAM_COLOREFFECT_OFF, _ICPHDColorEffectMode_Off),
};

static CAM_Error _ICPHDSaveAeAwb( void* );
static CAM_Error _ICPHDRestoreAeAwb( void* );
static CAM_Error _ICPHDStartFlash( void* );
static CAM_Error _ICPHDStopFlash( void* );
static CAM_Error _ICPHDSetJpegParam( ICPHDState*, CAM_JpegParam* );
static CAM_Error _ICPHDFillFrameShotInfo( ICPHDState*, CAM_ImageBuffer* );


// shot mode capability
static void _ICPHDAutoModeCap( CAM_ShotModeCapability* );
// static void _ICPHDManualModeCap( CAM_ShotModeCapability* );
// static void _ICPHDNightModeCap( CAM_ShotModeCapability* );
/* shot mode cap function table */
ICPHD_ShotModeCap _ICPHDShotModeCap[CAM_SHOTMODE_NUM] = { _ICPHDAutoModeCap  , // CAM_SHOTMODE_AUTO = 0,
                                                        NULL              , // CAM_SHOTMODE_MANUAL,
                                                        NULL              , // CAM_SHOTMODE_PORTRAIT,
                                                        NULL              , // CAM_SHOTMODE_LANDSCAPE,
                                                        NULL              , // CAM_SHOTMODE_NIGHTPORTRAIT,
                                                        NULL              , // CAM_SHOTMODE_NIGHTSCENE,
                                                        NULL              , // CAM_SHOTMODE_CHILD,
                                                        NULL              , // CAM_SHOTMODE_INDOOR,
                                                        NULL              , // CAM_SHOTMODE_PLANTS,
                                                        NULL              , // CAM_SHOTMODE_SNOW,
                                                        NULL              , // CAM_SHOTMODE_BEACH,
                                                        NULL              , // CAM_SHOTMODE_FIREWORKS,
                                                        NULL              , // CAM_SHOTMODE_SUBMARINE,
                                                      };


_CAM_SmartSensorFunc func_icphd;

CAM_Error ICPHD_SelfDetect( _CAM_SmartSensorAttri *pSensorInfo )
{
	CAM_Error error = CAM_ERROR_NONE;

	/* NOTE:  If you open macro BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT in CameraConfig.mk
	 *        to bypass sensor dynamically detect to save camera-off to viwerfinder-on latency, you should initilize
	 *        _ICPHDVideoResTable/_ICPHDStillResTable/_ICPHDVideoFormatTable/_ICPHDStillFormatTable manually.
	 */

#if !defined( BUILD_OPTION_STRATEGY_DISABLE_DYNAMIC_SENSOR_DETECT )
	error = V4L2_SelfDetect( pSensorInfo, "icp-hd", &func_icphd,
	                         _ICPHDVideoResTable, _ICPHDStillResTable,
	                         _ICPHDVideoFormatTable, _ICPHDStillFormatTable );
	pSensorInfo->stSensorProp.eInstallOrientation = CAM_FLIPROTATE_NORMAL;
	pSensorInfo->stSensorProp.iFacing             = CAM_SENSOR_FACING_BACK;
	pSensorInfo->stSensorProp.iOutputProp         = CAM_OUTPUT_SINGLESTREAM;
#else
	{
		_V4L2SensorEntry *pSensorEntry = (_V4L2SensorEntry*)( pSensorInfo->cReserved );
		strcpy( pSensorInfo->stSensorProp.sName, "icp-hd" );
		pSensorInfo->stSensorProp.eInstallOrientation = CAM_FLIPROTATE_NORMAL;
		pSensorInfo->stSensorProp.iFacing             = CAM_SENSOR_FACING_BACK;
		pSensorInfo->stSensorProp.iOutputProp         = CAM_OUTPUT_SINGLESTREAM;

		pSensorInfo->pFunc                            = &func_icphd;

		// FIXME: the following is just an example in Marvell platform, you should change it according to your driver implementation
		strcpy( pSensorEntry->sDeviceName, "/dev/video0" );
		pSensorEntry->iV4L2SensorID = 1;
	}
#endif

	return error;
}


CAM_Error ICPHD_GetCapability( _CAM_SmartSensorCapability *pCapability )
{
	CAM_Int32s i = 0;

	// preview/video supporting
	// format
	pCapability->iSupportedVideoFormatCnt = 0;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT; i++ )
	{
		if ( _ICPHDVideoFormatTable[i] == 0 )
		{
			break;
		}
		pCapability->eSupportedVideoFormat[pCapability->iSupportedVideoFormatCnt] = _ICPHDVideoFormatTable[i];
		pCapability->iSupportedVideoFormatCnt++;
	}

	pCapability->bArbitraryVideoSize    = CAM_FALSE;
	pCapability->iSupportedVideoSizeCnt = 0;
	pCapability->stMinVideoSize.iWidth  = _ICPHDVideoResTable[0].iWidth;
	pCapability->stMinVideoSize.iHeight = _ICPHDVideoResTable[0].iHeight;
	pCapability->stMaxVideoSize.iWidth  = _ICPHDVideoResTable[0].iWidth;
	pCapability->stMaxVideoSize.iHeight = _ICPHDVideoResTable[0].iHeight;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_SIZE_CNT; i++ )
	{
		if ( _ICPHDVideoResTable[i].iWidth == 0 || _ICPHDVideoResTable[i].iHeight == 0)
		{
			break;
		}
		pCapability->stSupportedVideoSize[pCapability->iSupportedVideoSizeCnt] = _ICPHDVideoResTable[i];
		pCapability->iSupportedVideoSizeCnt++;

		if ( ( pCapability->stMinVideoSize.iWidth > _ICPHDVideoResTable[i].iWidth ) ||
		     ( ( pCapability->stMinVideoSize.iWidth == _ICPHDVideoResTable[i].iWidth ) && ( pCapability->stMinVideoSize.iHeight > _ICPHDVideoResTable[i].iHeight ) ) )
		{
			pCapability->stMinVideoSize.iWidth = _ICPHDVideoResTable[i].iWidth;
			pCapability->stMinVideoSize.iHeight = _ICPHDVideoResTable[i].iHeight;
		}
		if ( ( pCapability->stMaxVideoSize.iWidth < _ICPHDVideoResTable[i].iWidth) ||
		     ( ( pCapability->stMaxVideoSize.iWidth == _ICPHDVideoResTable[i].iWidth ) && ( pCapability->stMaxVideoSize.iHeight < _ICPHDVideoResTable[i].iHeight ) ) )
		{
			pCapability->stMaxVideoSize.iWidth = _ICPHDVideoResTable[i].iWidth;
			pCapability->stMaxVideoSize.iHeight = _ICPHDVideoResTable[i].iHeight;
		}
	}

	// still capture supporting
	// format
	pCapability->iSupportedStillFormatCnt           = 0;
	pCapability->stSupportedJPEGParams.bSupportJpeg = CAM_FALSE;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_FORMAT_CNT; i++ )
	{
		if ( _ICPHDStillFormatTable[i] == CAM_IMGFMT_JPEG )
		{
			// JPEG encoder functionalities
			pCapability->stSupportedJPEGParams.bSupportJpeg      = CAM_TRUE;
			pCapability->stSupportedJPEGParams.bSupportExif      = CAM_FALSE;
			pCapability->stSupportedJPEGParams.bSupportThumb     = CAM_FALSE;
			pCapability->stSupportedJPEGParams.iMinQualityFactor = 80;
			pCapability->stSupportedJPEGParams.iMaxQualityFactor = 80;
		}
		if ( _ICPHDStillFormatTable[i] == 0 )
		{
			break;
		}
		pCapability->eSupportedStillFormat[pCapability->iSupportedStillFormatCnt] = _ICPHDStillFormatTable[i];
		pCapability->iSupportedStillFormatCnt++;
	}
	// resolution
	pCapability->bArbitraryStillSize    = CAM_FALSE;
	pCapability->iSupportedStillSizeCnt = 0;
	pCapability->stMinStillSize.iWidth  = _ICPHDStillResTable[0].iWidth;
	pCapability->stMinStillSize.iHeight = _ICPHDStillResTable[0].iHeight;
	pCapability->stMaxStillSize.iWidth  = _ICPHDStillResTable[0].iWidth;
	pCapability->stMaxStillSize.iHeight = _ICPHDStillResTable[0].iHeight;
	for ( i = 0; i < CAM_MAX_SUPPORT_IMAGE_SIZE_CNT; i++ )
	{
		if ( _ICPHDStillResTable[i].iWidth == 0 || _ICPHDStillResTable[i] .iHeight == 0 )
		{
			break;
		}

		pCapability->stSupportedStillSize[pCapability->iSupportedStillSizeCnt] = _ICPHDStillResTable[i];
		pCapability->iSupportedStillSizeCnt++;

		if ( ( pCapability->stMinStillSize.iWidth > _ICPHDStillResTable[i].iWidth ) ||
		     ( ( pCapability->stMinStillSize.iWidth == _ICPHDStillResTable[i].iWidth ) && ( pCapability->stMinStillSize.iHeight > _ICPHDStillResTable[i].iHeight ) ) )
		{
			pCapability->stMinStillSize.iWidth  = _ICPHDStillResTable[i].iWidth;
			pCapability->stMinStillSize.iHeight = _ICPHDStillResTable[i].iHeight;
		}
		if ( ( pCapability->stMaxStillSize.iWidth < _ICPHDStillResTable[i].iWidth ) ||
		     ( ( pCapability->stMaxStillSize.iWidth == _ICPHDStillResTable[i].iWidth ) && ( pCapability->stMaxStillSize.iHeight < _ICPHDStillResTable[i].iHeight ) ) )
		{
			pCapability->stMaxStillSize.iWidth  = _ICPHDStillResTable[i].iWidth;
			pCapability->stMaxStillSize.iHeight = _ICPHDStillResTable[i].iHeight;
		}
	}

	// rotate
	pCapability->iSupportedRotateCnt = _ARRAY_SIZE( _ICPHDRotationTable );
	for ( i = 0; i < pCapability->iSupportedRotateCnt; i++ )
	{
		pCapability->eSupportedRotate[i] = _ICPHDRotationTable[i];
	}

	pCapability->iSupportedShotModeCnt = _ARRAY_SIZE( _ICPHDShotModeTable );
	for ( i = 0; i < pCapability->iSupportedShotModeCnt; i++ )
	{
		pCapability->eSupportedShotMode[i] = _ICPHDShotModeTable[i];
	}

	// FIXME: all supported shot parameters
	_ICPHDAutoModeCap( &pCapability->stSupportedShotParams );

	return CAM_ERROR_NONE;

}

CAM_Error ICPHD_GetShotModeCapability( CAM_ShotMode eShotMode, CAM_ShotModeCapability *pShotModeCap )
{
	CAM_Int32u i = 0;
	// CAM_Error error = CAM_ERROR_NONE;

	// BAC check
	for ( i = 0; i < _ARRAY_SIZE(_ICPHDShotModeTable); i++ )
	{
		if ( _ICPHDShotModeTable[i] == eShotMode )
		{
			break;
		}
	}

	if ( i >= _ARRAY_SIZE(_ICPHDShotModeTable) || pShotModeCap == NULL )
	{
		return CAM_ERROR_BADARGUMENT;
	}

	(void)(_ICPHDShotModeCap[eShotMode])( pShotModeCap );

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_Init( void **ppDevice, void *pSensorEntry )
{
	CAM_Error            error      = CAM_ERROR_NONE;
	_V4L2SensorAttribute _ICPHDAttri;
	_V4L2SensorEntry     *pSensor   = (_V4L2SensorEntry*)pSensorEntry;
	ICPHDState            *pState    = (ICPHDState*)malloc( sizeof(ICPHDState) );

	memset( &_ICPHDAttri, 0, sizeof(_V4L2SensorAttribute) );

	*ppDevice = pState;
	if ( *ppDevice == NULL )
	{
		return CAM_ERROR_OUTOFMEMORY;
	}

	_ICPHDAttri.stV4L2SensorEntry.iV4L2SensorID = pSensor->iV4L2SensorID;
	strcpy( _ICPHDAttri.stV4L2SensorEntry.sDeviceName, pSensor->sDeviceName );

#ifdef BUILD_OPTION_DEBUG_DISABLE_SAVE_RESTORE_3A
	_ICPHDAttri.iUnstableFrameCnt    = 20;
	_ICPHDAttri.fnSaveAeAwb          = NULL;
	_ICPHDAttri.fnRestoreAeAwb       = NULL;
	_ICPHDAttri.pSaveRestoreUserData = NULL;
	_ICPHDAttri.fnStartFlash         = NULL;
	_ICPHDAttri.fnStopFlash          = NULL;
#else
	_ICPHDAttri.iUnstableFrameCnt    = 2;
	_ICPHDAttri.fnSaveAeAwb          = _ICPHDSaveAeAwb;
	_ICPHDAttri.fnRestoreAeAwb       = _ICPHDRestoreAeAwb;
	_ICPHDAttri.pSaveRestoreUserData = (void*)pState;
	_ICPHDAttri.fnStartFlash         = _ICPHDStartFlash;
	_ICPHDAttri.fnStopFlash          = _ICPHDStopFlash;
#endif

	error = V4L2_Init( &pState->stV4L2, &_ICPHDAttri );
	if ( error != CAM_ERROR_NONE )
	{
		free( pState );
		pState    = NULL;
		*ppDevice = NULL;
		return error;
	}

	/* here we can set default shot params */
	pState->stV4L2.stShotParamSetting.eShotMode                     = CAM_SHOTMODE_AUTO;
	pState->stV4L2.stShotParamSetting.eSensorRotation               = CAM_FLIPROTATE_NORMAL;
	pState->stV4L2.stShotParamSetting.eExpMode                      = CAM_EXPOSUREMODE_AUTO;
	pState->stV4L2.stShotParamSetting.eExpMeterMode                 = CAM_EXPOSUREMETERMODE_AUTO;
	pState->stV4L2.stShotParamSetting.eIsoMode                      = CAM_ISO_AUTO;
	pState->stV4L2.stShotParamSetting.eBandFilterMode               = CAM_BANDFILTER_50HZ;
#if defined( BUILD_OPTION_STRATEGY_ENABLE_FLASH )
	pState->stV4L2.stShotParamSetting.eFlashMode                    = CAM_FLASH_OFF;
#else
	pState->stV4L2.stShotParamSetting.eFlashMode                    = -1;
#endif
	pState->stV4L2.stShotParamSetting.eWBMode                       = CAM_WHITEBALANCEMODE_AUTO;
	pState->stV4L2.stShotParamSetting.eFocusMode                    = CAM_FOCUS_INFINITY;
	pState->stV4L2.stShotParamSetting.eFocusZoneMode                = CAM_FOCUSZONEMODE_CENTER;
	pState->stV4L2.stShotParamSetting.eColorEffect                  = CAM_COLOREFFECT_OFF;
	pState->stV4L2.stShotParamSetting.eVideoSubMode                 = CAM_VIDEOSUBMODE_SIMPLE;
	pState->stV4L2.stShotParamSetting.eStillSubMode                 = CAM_STILLSUBMODE_SIMPLE;
	pState->stV4L2.stShotParamSetting.iEvCompQ16                    = 0;
	pState->stV4L2.stShotParamSetting.iShutterSpeedQ16              = -1;
	pState->stV4L2.stShotParamSetting.iFNumQ16                      = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pState->stV4L2.stShotParamSetting.iDigZoomQ16                   = 1 << 16;
	pState->stV4L2.stShotParamSetting.iOptZoomQ16                   = 1 << 16;
	pState->stV4L2.stShotParamSetting.iSaturation                   = 64;
	pState->stV4L2.stShotParamSetting.iBrightness                   = 0;
	pState->stV4L2.stShotParamSetting.iContrast                     = 0;
	pState->stV4L2.stShotParamSetting.iSharpness                    = 0;
	pState->stV4L2.stShotParamSetting.iFpsQ16                       = 30 << 16;

	pState->stV4L2.stShotParamSetting.stStillSubModeParam.iBurstCnt = 1;
	pState->stV4L2.stShotParamSetting.stStillSubModeParam.iZslDepth = 0;

	memset( &pState->stV4L2.stShotParamSetting.stExpMeterROI, 0, sizeof(CAM_MultiROI) );
	memset( &pState->stV4L2.stShotParamSetting.stFocusROI, 0, sizeof(CAM_MultiROI) );

	return error;
}

CAM_Error ICPHD_Deinit( void **ppDevice )
{
	CAM_Error error   = CAM_ERROR_NONE;
	ICPHDState *pState = (ICPHDState*)(*ppDevice);

	error = V4L2_Deinit( &pState->stV4L2 );

	if ( error == CAM_ERROR_NONE )
	{
		free( pState );
		pState    = NULL;
		*ppDevice = NULL;
	}

	return error;
}

CAM_Error ICPHD_RegisterEventHandler( void *pDevice, CAM_EventHandler fnEventHandler, void *pUserData )
{
	ICPHDState *pState = (ICPHDState*)pDevice;
	CAM_Error error   = CAM_ERROR_NONE;

	error = V4L2_RegisterEventHandler( &pState->stV4L2, fnEventHandler, pUserData );
	return error;
}

CAM_Error ICPHD_Config( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig )
{
	ICPHDState *pState = (ICPHDState*)pDevice;
	CAM_Error error   = CAM_ERROR_NONE;

	error = V4L2_Config( &pState->stV4L2, pSensorConfig );
	if ( error != CAM_ERROR_NONE )
	{
		return error;
	}

	if ( pSensorConfig->eState != CAM_CAPTURESTATE_IDLE )
	{
		if ( pSensorConfig->eState == CAM_IMGFMT_JPEG )
		{
			error = _ICPHDSetJpegParam( pDevice, &(pSensorConfig->stJpegParam) );
			if ( error != CAM_ERROR_NONE )
			{
				return error;
			}
		}
	}

	pState->stV4L2.stConfig = *pSensorConfig;

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_GetBufReq( void *pDevice, _CAM_SmartSensorConfig *pSensorConfig, CAM_ImageBufferReq *pBufReq )
{
	ICPHDState *pState = (ICPHDState*)pDevice;

	return V4L2_GetBufReq( &pState->stV4L2, pSensorConfig, pBufReq );
}

CAM_Error ICPHD_Enqueue( void *pDevice, CAM_ImageBuffer *pImgBuf )
{
	ICPHDState *pState = (ICPHDState*)pDevice;

	return V4L2_Enqueue( &pState->stV4L2, pImgBuf );
}

CAM_Error ICPHD_Dequeue( void *pDevice, CAM_ImageBuffer **ppImgBuf )
{
	ICPHDState *pState = (ICPHDState*)pDevice;
	CAM_Error error = CAM_ERROR_NONE;

	error = V4L2_Dequeue( &pState->stV4L2, ppImgBuf );

	if ( ( error == CAM_ERROR_NONE ) && ( (*ppImgBuf)->bEnableShotInfo ) )
	{
		error = _ICPHDFillFrameShotInfo( pState, *ppImgBuf );
	}

	return error;
}

CAM_Error ICPHD_Flush( void *pDevice )
{
	ICPHDState *pState = (ICPHDState*)pDevice;

	return V4L2_Flush( &pState->stV4L2 );
}

CAM_Error ICPHD_SetShotParam( void *pDevice, _CAM_ShotParam *pShotParam )
{
	// TODO: here you can add your code to set shot params you supported, just like examples in cam_extisp_ov5642.c or you can give your own style

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_GetShotParam( void *pDevice, _CAM_ShotParam *pShotParam )
{
	ICPHDState *pState = (ICPHDState*)pDevice;

	*pShotParam = pState->stV4L2.stShotParamSetting;

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_StartFocus( void *pDevice )
{
	// TODO: add your start focus code here,an refrence is cam_extisp_ov5642.c

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_CancelFocus( void *pDevice )
{
	// TODO: add your cancel focus code here,an refrence is cam_extisp_ov5642.c
	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_CheckFocusState( void *pDevice, CAM_Bool *pFocusAutoStopped, CAM_FocusState *pFocusState )
{
	// TODO: add your check focus status code here,an refrence is cam_extisp_ov5642.c
	*pFocusAutoStopped = CAM_TRUE;
	*pFocusState       = CAM_FOCUSSTATE_INFOCUS;

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_GetDigitalZoomCapability( CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s *pSensorDigZoomQ16 )
{
	// TODO: add your get zoom capability code here,an refrence is cam_extisp_ov5642.c
	*pSensorDigZoomQ16 = ( 1 << 16 );

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_StartFaceDetection( void *pDevice )
{
	// TODO: add your start face detection code here,an refrence is cam_extisp_ov5642.c

	return CAM_ERROR_NONE;
}

CAM_Error ICPHD_CancelFaceDetection( void *pDevice )
{
	// TODO: add your cancel face detection code here,an refrence is cam_extisp_ov5642.c
	return CAM_ERROR_NONE;
}



_CAM_SmartSensorFunc func_icphd =
{
	ICPHD_GetCapability,
	ICPHD_GetShotModeCapability,

	ICPHD_SelfDetect,
	ICPHD_Init,
	ICPHD_Deinit,
	ICPHD_RegisterEventHandler,
	ICPHD_Config,
	ICPHD_GetBufReq,
	ICPHD_Enqueue,
	ICPHD_Dequeue,
	ICPHD_Flush,
	ICPHD_SetShotParam,
	ICPHD_GetShotParam,

	ICPHD_StartFocus,
	ICPHD_CancelFocus,
	ICPHD_CheckFocusState,
	ICPHD_GetDigitalZoomCapability,
	ICPHD_StartFaceDetection,
	ICPHD_CancelFaceDetection,
};

static CAM_Error _ICPHDSetJpegParam( ICPHDState *pState, CAM_JpegParam *pJpegParam )
{
	if ( pJpegParam->bEnableExif )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( pJpegParam->bEnableThumb )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( pJpegParam->iQualityFactor != 80 )
	{
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	return CAM_ERROR_NONE;
}

// get shot info
static CAM_Error _ICPHDFillFrameShotInfo( ICPHDState *pState, CAM_ImageBuffer *pImgBuf )
{
	// TODO: will contact sensor/module vendor to get the real value of these parameters

	CAM_Error    error      = CAM_ERROR_NONE;
	CAM_ShotInfo *pShotInfo = &(pImgBuf->stShotInfo);

	pShotInfo->iExposureTimeQ16    = (1 << 16) / 30;
	pShotInfo->iFNumQ16            = (CAM_Int32s)( 2.8 * 65536 + 0.5 );
	pShotInfo->eExposureMode       = pState->stV4L2.stShotParamSetting.eExpMode;
	pShotInfo->eExpMeterMode       = CAM_EXPOSUREMETERMODE_MEAN;
	pShotInfo->iISOSpeed           = 100;
	pShotInfo->iSubjectDistQ16     = 0;
	pShotInfo->bIsFlashOn          = ( pState->stV4L2.stConfig.eState == CAM_CAPTURESTATE_STILL ) ? pState->stV4L2.bIsCaptureFlashOn : pState->stV4L2.bIsFlashOn;
	pShotInfo->iFocalLenQ16        = (CAM_Int32s)( 3.79 * 65536 + 0.5 );
	pShotInfo->iFocalLen35mm       = 0;

	return error;
}


/*-------------------------------------------------------------------------------------------------------------------------------------
 * ICPHD shotmode capability
 * TODO: if you enable new shot mode, pls add a correspoding modcap function here, and add it to ICPHD_shotModeCap _ICPHDShotModeCap array
 *------------------------------------------------------------------------------------------------------------------------------------*/
static void _ICPHDAutoModeCap( CAM_ShotModeCapability *pShotModeCap )
{
	// exposure mode
	pShotModeCap->iSupportedExpModeCnt  = 1;
	pShotModeCap->eSupportedExpMode[0]  = CAM_EXPOSUREMODE_AUTO;

	// exposure metering mode
	pShotModeCap->iSupportedExpMeterCnt = 1;
	pShotModeCap->eSupportedExpMeter[0] = CAM_EXPOSUREMETERMODE_AUTO;

	// EV compensation
	pShotModeCap->iEVCompStepQ16 = 0;
	pShotModeCap->iMinEVCompQ16  = 0;
	pShotModeCap->iMaxEVCompQ16  = 0;

	// ISO mode
	pShotModeCap->iSupportedIsoModeCnt = 1;
	pShotModeCap->eSupportedIsoMode[0] = CAM_ISO_AUTO;

	// shutter speed
	pShotModeCap->iMinShutSpdQ16 = -1;
	pShotModeCap->iMaxShutSpdQ16 = -1;

	// F-number
	pShotModeCap->iMinFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);
	pShotModeCap->iMaxFNumQ16 = (CAM_Int32s)(2.8 * 65536 + 0.5);

	// band filter
	pShotModeCap->iSupportedBdFltModeCnt = 1;
	pShotModeCap->eSupportedBdFltMode[0] = CAM_BANDFILTER_50HZ;

	// flash mode
	pShotModeCap->iSupportedFlashModeCnt = 1;
	pShotModeCap->eSupportedFlashMode[0] = CAM_FLASH_OFF;

	// white balance mode
	pShotModeCap->iSupportedWBModeCnt = 1;
	pShotModeCap->eSupportedWBMode[0] = CAM_WHITEBALANCEMODE_AUTO;

	// focus mode
	pShotModeCap->iSupportedFocusModeCnt = 1;
	pShotModeCap->eSupportedFocusMode[0] = CAM_FOCUS_INFINITY;

	// focus zone mode
	pShotModeCap->iSupportedFocusZoneModeCnt = 1;
	pShotModeCap->eSupportedFocusZoneMode[0] = CAM_FOCUSZONEMODE_CENTER;

	// color effect
	pShotModeCap->iSupportedColorEffectCnt = 1;
	pShotModeCap->eSupportedColorEffect[0] = CAM_COLOREFFECT_OFF;

	// video sub-mode
	pShotModeCap->iSupportedVideoSubModeCnt = 1;
	pShotModeCap->eSupportedVideoSubMode[0] = CAM_VIDEOSUBMODE_SIMPLE;

	// still sub-mode
	pShotModeCap->iSupportedStillSubModeCnt = 1;
	pShotModeCap->eSupportedStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;

	// optical zoom mode
	pShotModeCap->iMinOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxOptZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);

	// digital zoom mode
	pShotModeCap->iMinDigZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->iMaxDigZoomQ16 = (CAM_Int32s)(1.0 * 65536 + 0.5);
	pShotModeCap->bSupportSmoothDigZoom = CAM_FALSE;

	// frame rate
	pShotModeCap->iMinFpsQ16     = 1 << 16;
	pShotModeCap->iMaxFpsQ16     = 40 << 16;

	// brightness
	pShotModeCap->iMinBrightness = 0;
	pShotModeCap->iMaxBrightness = 0;

	// contrast
	pShotModeCap->iMinContrast = 0;
	pShotModeCap->iMaxContrast = 0;

	// saturation
	pShotModeCap->iMinSaturation = 0;
	pShotModeCap->iMaxSaturation = 0;

	// sharpness
	pShotModeCap->iMinSharpness = 0;
	pShotModeCap->iMaxSharpness = 0;

	pShotModeCap->iMaxBurstCnt = 1;
	pShotModeCap->iMaxZslDepth = 0; // set 0 because sensor not support
	pShotModeCap->iMaxExpMeterROINum = 0;
	pShotModeCap->iMaxFocusROINum    = 0;

	return;
}


static CAM_Error _ICPHDSaveAeAwb( void *pUserData )
{
	// TODO: add your sensor specific save 3A function here
	return CAM_ERROR_NONE;
}

static CAM_Error _ICPHDRestoreAeAwb( void *pUserData )
{
	// TODO: add your sensor specific restore 3A function here
	return CAM_ERROR_NONE;
}

static CAM_Error _ICPHDStartFlash( void *pData )
{
	// TODO: add your sensor specific start flash function here
	return CAM_ERROR_NONE;
}

static CAM_Error _ICPHDStopFlash( void *pData )
{
	// TODO: add your sensor specific stop flash function here
	return CAM_ERROR_NONE;
}
