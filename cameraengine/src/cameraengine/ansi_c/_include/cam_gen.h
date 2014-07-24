/*******************************************************************************
//(C) Copyright [2009 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#ifndef _CAM_GEN_H_
#define _CAM_GEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_utility.h"

typedef CAM_Error (*_CAM_OpenFunc)( void **ppDeviceData );

typedef CAM_Error (*_CAM_CloseFunc)( void **ppDeviceData );

typedef CAM_Error (*_CAM_CommandFunc)( void *pDeviceData, CAM_Command cmd, CAM_Param param1, CAM_Param param2 );

typedef struct
{
	const char          *pName;
	_CAM_OpenFunc       pOpen;
	_CAM_CloseFunc      pClose;
	_CAM_CommandFunc    pCommand;
} _CAM_DriverEntry;

typedef struct
{
	CAM_Int32s       iTotalCnt;
	CAM_Int32s       iExtIspCnt;
	CAM_DeviceProp   stDeviceProp[CAM_MAX_SUPPORT_CAMERA];

	// event handler
	CAM_EventHandler fnEventHandler;
	void             *pUserData;

	CAM_Int32s       iSensorID;
	_CAM_DriverEntry *pEntry;
	void             *pDeviceData;
} _CAM_DeviceState;

// supported entries
extern const _CAM_DriverEntry entry_socisp;
extern const _CAM_DriverEntry entry_extisp;

typedef struct
{
	CAM_ShotMode           eShotMode;

	CAM_FlipRotate         eSensorRotation;
	CAM_Int32s             iFpsQ16;

	CAM_ExposureMode       eExpMode;
	CAM_Int32s             iEvCompQ16;
	CAM_Int32s             iShutterSpeedQ16;
	CAM_Int32s             iFNumQ16;
	CAM_ISOMode            eIsoMode;
	CAM_ExposureMeterMode  eExpMeterMode;
	CAM_MultiROI           stExpMeterROI;
	CAM_BandFilterMode     eBandFilterMode;

	CAM_WhiteBalanceMode   eWBMode;
	CAM_ColorEffect        eColorEffect;

	CAM_FlashMode          eFlashMode;

	CAM_FocusMode          eFocusMode;
	CAM_FocusZoneMode      eFocusZoneMode;
	CAM_MultiROI           stFocusROI;

	CAM_StillSubMode       eStillSubMode;
	CAM_StillSubModeParam  stStillSubModeParam;

	CAM_VideoSubMode       eVideoSubMode;

	CAM_Int32s             iDigZoomQ16;
	CAM_Int32s             iOptZoomQ16;

	CAM_Int32s             iSaturation;
	CAM_Int32s             iBrightness;
	CAM_Int32s             iContrast;
	CAM_Int32s             iSharpness;
} _CAM_ShotParam;

typedef struct
{
	CAM_Int32s       iWidth;  // width before rotate
	CAM_Int32s       iHeight; // height before rotate
	CAM_ImageFormat  eFormat;
	CAM_FlipRotate   eRotation;
	CAM_JpegParam    *pJpegParam;
	CAM_Bool         bIsStreaming;
} _CAM_ImageInfo;

//  @_CAM_PortState: indicate the port status like: output image attribute, buffer queue state and buffer requirement
typedef struct
{
	CAM_Int32s         iWidth;  // this is the width before port rotation
	CAM_Int32s         iHeight; // this is the height before port rotation
	CAM_ImageFormat    eFormat;
	CAM_FlipRotate     eRotation;

	_CAM_Queue         qEmpty;
	_CAM_Queue         qFilled;

	// buffer requirement of port
	CAM_ImageBufferReq stBufReq;

	// port mutex
	void               *hMutex;
} _CAM_PortState;


#ifdef __cplusplus
}
#endif

#endif  // _CAM_GEN_H_
