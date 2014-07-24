/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#ifndef _CAM_SOCISP_DMACTRL_H_
#define _CAM_SOCISP_DMACTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "DxOISP_Tools.h"
#include "cam_socisp_hal.h"

// event id
typedef enum
{
	ISPDMA_EVENT_DISP_EOF  = 0x01,
	ISPDMA_EVENT_CODEC_EOF = 0x02,
	ISPDMA_EVENT_BAND_EOF  = 0x04,
	ISPDMA_EVENT_DISP_DROP = 0x08,
	ISPDMA_EVENT_CODEC_DROP= 0x10,
	ISPDMA_EVENT_ERROR     = 0x20,
	ISPDMA_EVENT_ALL       = 0x3f,

	ISPDMA_EVENT_LIMIT     = 0x7fffffff,
}_CAM_IspDmaEvent;

// buffer node
typedef struct
{
	CAM_Int8u      *pVirtAddr;
	CAM_Int32u     uiPhyAddr;
	CAM_Int32s     iBufLen;
	CAM_Int8u      *pUserData;
}_CAM_IspDmaBufNode;

typedef struct 
{
	CAM_Int32s         iWidth;
	CAM_Int32s         iHeight;
	CAM_ImageFormat    eFormat;
}_CAM_IspDmaFormat;

typedef struct
{
	CAM_Int32s         iBandCnt;
	CAM_Int32s         iBandSize;

	CAM_Int32s         iBufHandleMode;
}_CAM_IspDmaProperty;

/* APIs */

// start/stop isp-dma
CAM_Error ispdma_init( void **phIspDmaHandle );
CAM_Error ispdma_deinit( void **phIspDmaHandle );

CAM_Error ispdma_reset( void *hIspDmaHandle );
// isp-dma configuration
CAM_Error ispdma_enum_sensor( CAM_Int32s *piSensorCount, CAM_DeviceProp astCameraProp[CAM_MAX_SUPPORT_CAMERA] );
CAM_Error ispdma_select_sensor( void *hIspDmaHandle, CAM_Int32s iSensorID );

CAM_Error ispdma_set_output_format( void *hIspDmaHandle, _CAM_IspDmaFormat *pstDmaFormat, _CAM_IspPort ePortID );
CAM_Error ispdma_set_codec_property( void *hIspDmaHandle, _CAM_IspDmaProperty *pstDmaProperty );
CAM_Error ispdma_set_framebuffer( void *hIspDmaHandle, _CAM_IspDmaBufNode astFBuf[NB_FRAME_BUFFER], CAM_Int32s iBandCnt );

CAM_Error ispdma_sensor_streamon( void *hIspDmaHandle, CAM_Bool bOn );
CAM_Error ispdma_isp_streamon( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bOn );
CAM_Error ispdma_isp_set_mode( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_Bool bIsStreaming );

// buffer management
CAM_Error ispdma_enqueue_buffer( void *hIspDmaHandle, CAM_ImageBuffer *pstImgBuf, _CAM_IspPort ePortID );
CAM_Error ispdma_dequeue_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID, CAM_ImageBuffer **ppImgBuf, CAM_Bool *pbIsError );
CAM_Error ispdma_flush_buffer( void *hIspDmaHandle, _CAM_IspPort ePortID );
CAM_Error ispdma_get_bufreq( void *hIspDmaHandle, _CAM_ImageInfo *pstConfig, CAM_ImageBufferReq *pstBufReq );
CAM_Error ispdma_get_min_bufcnt( void *hIspDmaHandle, CAM_Bool bIsStreaming, CAM_Int32s *pMinBufCnt );

// event management
CAM_Error ispdma_poll_buffer( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Int32s iRequest, CAM_Int32s *piResult );  // FIXME: what is request data structure

inline CAM_Error ispdma_poll_ipc( void *hIspDmaHandle, CAM_Int32s iTimeout, CAM_Tick *ptTick );

CAM_Error ispdma_get_tick( void *hIspDmaHandle, CAM_Tick *ptTick );

// pattern generator( for debug )
CAM_Error ispdma_config_test_pattern( void *hIspDmaHandle, CAM_Int8u ucMode, CAM_Int16u byr[4] );

#ifdef __cplusplus
}
#endif

#endif //  _CAM_SOCISP_DMACTRL_H_
