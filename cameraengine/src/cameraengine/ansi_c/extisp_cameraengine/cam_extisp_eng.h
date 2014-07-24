/*****************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/

#ifndef __CAM_ENGINE_EXTISP_H_
#define __CAM_ENGINE_EXTISP_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "CameraEngine.h"

#include "cam_extisp_sensorwrapper.h"
#include "cam_socdef.h"

#include "cam_gen.h"
#include "cam_utility.h"

typedef struct
{
	// camera-engine handle lock
	void                    *hCEMutex;

	// available sensors
	CAM_Int32s              iSensorCount;
	CAM_DeviceProp          stSensorProp[CAM_MAX_SUPPORT_CAMERA];

	// process buffer work thread management
#if !defined( BUILD_OPTION_DEBUG_DISABLE_PROCESS_THREAD )
	CAM_Thread              stProcessBufferThread;
#endif
	// check-focus thread
	CAM_Thread              stFocusThread;
	void                    *hFocusMutex;
	CAM_Bool                bFocusSleep;         // flag to notify focus thread to be sleep
	CAM_Tick                tFocusBegin;

	// event handler user registered to camera-engine
	CAM_EventHandler        fnEventHandler;
	void                    *pUserData;

	// post-processing unit handler
	void                    *hPpuHandle;
	CAM_Bool                bNeedPostProcessing;
	CAM_Bool                bUsePrivateBuffer;

	// buffer management for embeded post-processing unit
	CAM_ImageBuffer         stPpuSrcImg[CAM_MAX_PORT_BUF_CNT];    // the array index is used to fill the CAM_ImageBuffer::iUserIndex
	CAM_Bool                bPpuSrcImgUsed[CAM_MAX_PORT_BUF_CNT]; // whether the position on corresponding index is using 
	CAM_Int32s              iPpuSrcImgAllocateCnt;                // indicate how many image buffers in stPpuSrcImage are allocated by camera-enigne itself

	// current camera capability
	CAM_CameraCapability    stCameraCap;
	CAM_ShotModeCapability  stShotModeCap;

	// current sensor status
	CAM_Int32s              iSensorID;
	void                    *hSensorHandle;
	CAM_ImageBufferReq      stCurrentSensorBufReq;  // updated whenever the sensor is configured

	// camera state
	CAM_CaptureState        eCaptureState;
	_CAM_PortState          stPortState[3];

	// preview port settings
	CAM_Bool                bPreviewFavorAspectRatio;

	// still state settings
	CAM_JpegParam           stJpegParam;
	CAM_Bool                bIsFocusOpen;
	CAM_Int32s              iStillRestCount;    // still image capture counter

	// shot parameters
	_CAM_ShotParam          stShotParam;
	_CAM_PostProcessParam   stPpuShotParam;

	// buffer ring
	_CAM_Queue              qPpuBufferRing;

	// advanced feature
	CAM_Bool                bIsFaceDetectionOpen;
} _CAM_ExtIspState;

#ifdef __cplusplus
}
#endif

#endif // __CAM_ENGINE_EXTISP_H_
