/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/

#ifndef _CAM_UTILITY_H_
#define _CAM_UTILITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "CameraOSAL.h"
#include "CameraEngine.h"

/*
+* MACRO to calc time interval
+*/
#define CAM_PERF_INTERVAL       0
#define CAM_PERF_OVERALL_BEGIN  1
#define CAM_PERF_OVERALL_END    2
#define _CAM_TIME_CALC( tInterval, tOverall, eCameraState, eStateMask, CAM_PERF_TYPE, ... )\
	do\
	{\
		if ( ( eCameraState ) & ( eStateMask ) )\
		{\
			if ( CAM_PERF_TYPE == CAM_PERF_INTERVAL )\
			{\
				CELOG( "CAM_PERF: ************************\n" );\
				CELOG( __VA_ARGS__ );\
				CELOG( "CAM_PERF: interval cost %.2f ms\n", ( CAM_TimeGetTickCount() - tInterval ) / 1000.0 );\
				CELOG( "CAM_PERF: position: ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );\
				tInterval    = CAM_TimeGetTickCount();\
				CELOG( "CAM_PERF: ************************\n" );\
			}\
			else if ( CAM_PERF_TYPE == CAM_PERF_OVERALL_BEGIN )\
			{\
				CELOG( "CAM_PERF: =========================\n" );\
				tOverall  = CAM_TimeGetTickCount();\
				tInterval = tOverall;\
				CELOG( __VA_ARGS__ );\
				CELOG( "CAM_PERF: position: ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );\
				CELOG( "CAM_PERF: =========================\n" );\
			}\
			else if ( CAM_PERF_TYPE == CAM_PERF_OVERALL_END )\
			{\
				CELOG( "CAM_PERF: =========================\n" );\
				CELOG( __VA_ARGS__ );\
				tOverall = CAM_TimeGetTickCount() - tOverall;\
				CELOG( "CAM_PERF: overall cost %.2f ms\n", tOverall / 1000.0 );\
				CELOG( "CAM_PERF: position: ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );\
				CELOG( "CAM_PERF: =========================\n" );\
			}\
			else\
			{\
				CELOG( "CAM_PERF: Unkown test time Type[%d] ( %s, %s, %d )\n", CAM_PERF_TYPE, __FILE__, __FUNCTION__, __LINE__ );\
			}\
		}\
	} while ( 0 )

#if defined( BUILD_OPTION_DEBUG_PROFILE_PERFORMANCE )
static CAM_Tick tInterval = 0, tOverall = 0;
#define CAM_PROFILE( eCameraState, eStateMask, CAM_PERF_TYPE, ... ) _CAM_TIME_CALC( tInterval, tOverall, eCameraState, eStateMask, CAM_PERF_TYPE, __VA_ARGS__ )
#else
#define CAM_PROFILE( eCameraState, eStateMask, CAM_PERF_TYPE, ... )
#endif

/*
 * some basic calculation MACROs
 */
#define _FLOOR_TO(x,iAlign) ( ((CAM_Int32s)(x)) & (~((iAlign) - 1)) )

#define _ALIGN_TO(x,iAlign) ( (((CAM_Int32s)(x)) + (iAlign) - 1) & (~((iAlign) - 1)) )

#define _ROUND_TO(x,iAlign) ( (((CAM_Int32s)(x)) + (iAlign) / 2) & (~((iAlign) - 1)) )

#define _ARRAY_SIZE(array)  ( sizeof(array) / sizeof((array)[0]) )

#define _MAX(a,b)           ( (a) > (b) ? (a) : (b) )

#define _MIN(a,b)           ( (a) > (b) ? (b) : (a) )

#define _ABSDIFF(a,b)       ( (a) > (b) ? ((a) - (b)) : ((b) - (a)) )

#define _DIV_ROUND(a, b)    (((a) + ((b) >> 1)) / (b))

#define _DIV_CEIL(a, b)     (((a) + (b) - 1) / (b))

/*
 * build optimization related MACROs
 */
#define LIKELY(x)       __builtin_expect((x),1)
#define UNLIKELY(x)     __builtin_expect((x),0)

/*
 *@ data queue
 */
typedef struct _queue
{
	void       **pData;
	CAM_Int32s iDataNum;
	CAM_Int32s iDeQueuePos;
	CAM_Int32s iQueueCount;
} _CAM_Queue;

CAM_Int32s _InitQueue( _CAM_Queue *pQueue, CAM_Int32s iDataNum );
CAM_Int32s _DeinitQueue( _CAM_Queue *pQueue );
CAM_Int32s _EnQueue( _CAM_Queue *pQueue, void *pData );
CAM_Int32s _DeQueue( _CAM_Queue *pQueue, void **ppData );
CAM_Int32s _FlushQueue( _CAM_Queue *pQueue );
CAM_Int32s _GetQueue( _CAM_Queue *pQueue, CAM_Int32s iIndex, void **ppData );
CAM_Int32s _RemoveQueue( _CAM_Queue *pQueue, void *pData );
CAM_Int32s _RetrieveQueue( _CAM_Queue *pQueue, void *pData, int *pIndex );

/*
 * basic ulitity functions
 */
CAM_Int32s  _calcjpegbuflen( CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_JpegParam *pJpegParam );
CAM_Int32s  _calczslbufnum( CAM_Int32s iMinSensorBufReq, CAM_Int32s iZslDepth );
CAM_Int32s  _calcbyteperpix( CAM_ImageFormat eFormat );
CAM_Bool    _is_format_rawandplanar( CAM_ImageFormat eFormat );
CAM_Int32s  _is_roi_update( CAM_MultiROI *pOldMultiROI, CAM_MultiROI *pNewMultiROI, CAM_Bool *pIsUpdate );
void        _calcimglen( CAM_ImageBuffer *pImgBuf );
void        _calcstep( CAM_ImageFormat eFormat, CAM_Int32s iWidth, CAM_Int32s iAlign[3], CAM_Int32s iStep[3] );
void        _calcbuflen( CAM_ImageFormat eFormat, CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s iStep[3], CAM_Int32s iLength[3], void *pUserData );
void        _calccrop( CAM_Int32s iWidth, CAM_Int32s iHeight, CAM_Int32s iRatioW, CAM_Int32s iRatioH, CAM_Rect *pRect );
void        _calcpixpersample( CAM_ImageFormat eFormat, CAM_Int32s *pPixH, CAM_Int32s *pPixV );
CAM_Bool    _is_bayer_format( CAM_ImageFormat eFormat );

#ifdef __cplusplus
}
#endif

#endif  // _CAM_UTILITY_H_
