/******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
******************************************************************************/

#ifndef _CAM_SOCISP_HAL_H_
#define _CAM_SOCISP_HAL_H_

#ifdef __cplusplus
extern "C"{
#endif

#include "CameraEngine.h"
#include "cam_socisp_eng.h"
#include "cam_gen.h"

typedef enum
{
	ISP_PORT_DISPLAY = 1,
	ISP_PORT_CODEC   = 2,
	ISP_PORT_BOTH    = ISP_PORT_DISPLAY | ISP_PORT_CODEC,

	ISP_PORT_NUM     = ISP_PORT_BOTH,

	ISP_PORT_LIMIT   = 0x7fffffff,
}_CAM_IspPort;


CAM_Error isp_init( void **phIspHandle );
CAM_Error isp_deinit( void **phIspHandle );

CAM_Error isp_reset_hardware( void *hIspHandle, CAM_Int32s iResetType );

CAM_Error isp_enum_sensor( CAM_Int32s *piSensorCount, CAM_DeviceProp astCameraProp[CAM_MAX_SUPPORT_CAMERA] );
CAM_Error isp_select_sensor( void *hIspHandle, CAM_Int32s iSensorID );
CAM_Error isp_query_capability( void *hIspHandle, CAM_Int32s iSensorID, CAM_CameraCapability *pstCameraCap );

CAM_Error isp_set_state( void *hIspHandle, CAM_CaptureState eState );
CAM_Error isp_config_output( void *hIspHandle, _CAM_ImageInfo *pstPortConfig, _CAM_IspPort ePortID, CAM_CaptureState eToState );

CAM_Error isp_poll_buffer( void *hIspHandle, CAM_Int32s iTimeout, CAM_Int32s iRequest, CAM_Int32s *piResult );
CAM_Error isp_enqueue_buffer( void *hIspHandle, CAM_ImageBuffer *pstImgBuf, _CAM_IspPort ePortID, CAM_Bool bKeepDispStatus );
CAM_Error isp_dequeue_buffer( void *hIspHandle, _CAM_IspPort ePortID, CAM_ImageBuffer **ppImgBuf, CAM_Bool *pbIsError );
CAM_Error isp_flush_buffer( void *hIspHandle, _CAM_IspPort ePortID );
CAM_Error isp_get_bufreq( void *hIspHandle, _CAM_ImageInfo *pstPortConfig, CAM_ImageBufferReq *pstBufReq );

CAM_Error isp_set_shotparams( void *hIspHandle, CAM_Command eCmd, CAM_Param param1, CAM_Param param2 );
CAM_Error isp_get_shotparams( void *hIspHandle, CAM_Command eCmd, CAM_Param param1, CAM_Param param2 );
CAM_Error isp_query_shotmodecap( CAM_Int32s iSensorID, CAM_ShotMode eShotMode, CAM_ShotModeCapability *pstShotModeCap );

#ifdef __cplusplus
}
#endif

#endif // _CAM_SOCISP_HAL_H_
