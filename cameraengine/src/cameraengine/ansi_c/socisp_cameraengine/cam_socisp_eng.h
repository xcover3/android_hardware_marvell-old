/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#ifndef __CAM_ENGINE_SOCISP_H_
#define __CAM_ENGINE_SOCISP_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "CameraEngine.h"
#include "cam_utility.h"
#include "cam_socisp_dmactrl.h"
#include "cam_gen.h"

// soc-isp Camera Engine internal state structure
typedef struct
{
	/*
	// 3 use cases:     viewfinder, camcorder, still capture;
	// 5 camera stages: standby, idle, preview, still, video;
	// 3 ports:         preview, still, video;
	*/

	// entry lock
	void                    *hCEMutex;

	// available sensors
	CAM_Int32s              iSensorCount;
	CAM_DeviceProp          astSensorProp[CAM_MAX_SUPPORT_CAMERA];

	// event handler
	CAM_EventHandler        fnEventHandler;
	void                    *pUserData;

	// camera capability( mainly depend on ISP )
	CAM_CameraCapability    stCameraCap;
	CAM_ShotModeCapability  stShotModeCap;

	// current sensor
	CAM_Int32s              iSensorID;
	// shot parameter setting
	_CAM_ShotParam          stShotParamSetting;

	// capture state
	CAM_CaptureState        eCaptureState;

	// port state
	_CAM_PortState          astPortState[3];
	void*                   ahPortBufUpdate[3];

	// preview port settings
	CAM_Bool                bPreviewFavorAspectRatio;

	// JPEG info
	CAM_JpegParam           stJpegParam;

	// thread management
	CAM_Thread              stProcessBufferThread;
	void                    *hEventBufUpdate;

	// ISP handle
	void                    *hIspHandle;

	// post-processing unit
	void                    *hPpuHandle;
	CAM_Bool                bNeedPostProcessing;
	CAM_Bool                bUsePrivateBuffer;
	// buffer management for embeded post-processing unit
	CAM_ImageBuffer         astPpuSrcImg[CAM_MAX_PORT_BUF_CNT];    // the array index is used to fill the CAM_ImageBuffer::iUserIndex
	CAM_Bool                abPpuSrcImgUsed[CAM_MAX_PORT_BUF_CNT]; // whether the position on corresponding index is using
	CAM_Int32s              iPpuSrcImgAllocateCnt;                 // indicate how many image buffers in stPpuSrcImage are allocated by camera-enigne itself

	CAM_Int32s              iStillRestCount;                       // still image capture counter

	// FIXME: Temporary add it for error resilience, will remove it after upgrade to camera adaptor
	CAM_Bool                bIsIspOK;
} _CAM_SocIspState;


#ifdef __cplusplus
}
#endif

#endif // __CAM_ENGINE_SOCISP_H_
