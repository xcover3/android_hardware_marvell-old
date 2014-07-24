/*****************************************************************************
*(C) Copyright [2010 - 2011] Marvell International Ltd.
* All Rights Reserved
******************************************************************************/
// #define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>

#include "cam_log.h"
#include "cam_utility.h"

#include "cam_gen.h"
#include "cam_ppu.h"

#include "cam_socdef.h"
/* #include "JpegCompressor.h" */

#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB )
#if ( PLATFORM_GRAPHICS_VALUE < PLATFORM_GRAPHICS_GC200 ) || ( PLATFORM_GRAPHICS_VALUE > PLATFORM_GRAPHICS_GC2000 )
#error No suitable GCU. Pls undefine the following macro: BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB
#endif

#include "gcu.h"
#define GCU_SUBSAMPLE_THRESHOLD  1.5
#endif

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
#include "ire_lib.h"
#endif

#include "ippdefs.h"
#include "ippIP.h"
#include "misc.h"
#include "codecDef.h"
#include "codecJP.h"

#include "codecVC.h"
#include "ippLV.h"

#define IPP_JPEG_SRCFMT    CAM_IMGFMT_YCbCr420P
// #define VMETA_JPEG_SRCFMT  CAM_IMGFMT_CbYCrY
#define VMETA_JPEG_SRCFMT  CAM_IMGFMT_YCbYCr

static void _ppu_retrieve_format( CAM_ImageFormat eFmt, CAM_ImageFormat *pFmtCap, CAM_Int32s iFmtCapCnt, CAM_Int32s *pIndex );

#define PPU_DUMP_FRAME( sFileName, pImgBuf ) do {\
                                                 FILE *fp = NULL;\
                                                 fp = fopen( sFileName, "wb" );\
                                                 fwrite( pImgBuf->pBuffer[0], pImgBuf->iFilledLen[0], 1, fp );\
                                                 fflush( fp );\
                                                 fclose( fp );\
                                                 fp = NULL;\
                                                } while ( 0 )

typedef struct
{
	void    *hGcuContext;
	void    *hIreHandle;
}_CAM_PpuState;


// API
// create a post-processing handler for use
CAM_Error ppu_init( void **phHandle )
{
	*phHandle = NULL;

	_CAM_PpuState *pPpuState = (_CAM_PpuState*)malloc( sizeof( _CAM_PpuState ) );
	if ( pPpuState == NULL )
	{
		TRACE( CAM_ERROR, "Error: out of memeory to allocate PPU handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pPpuState, 0, sizeof( _CAM_PpuState ) );

#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB )
	{
		GCU_INIT_DATA    stInitData;
		GCU_CONTEXT_DATA stContextData;

		memset( &stInitData, 0, sizeof( stInitData ) );
		// stInitData.debug = GCU_TRUE;
		gcuInitialize( &stInitData );

		memset( &stContextData, 0, sizeof( stContextData ) );
		pPpuState->hGcuContext = gcuCreateContext( &stContextData );
		TRACE( CAM_INFO, "Info: ppu handle: %p\n", pPpuState->hGcuContext );

		ASSERT( pPpuState->hGcuContext != NULL );
	}
#else
	pPpuState->hGcuContext = NULL;
#endif

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
	pPpuState->hIreHandle = (void*)malloc( sizeof( struct ire_info ) );
	if ( pPpuState->hIreHandle == NULL )
	{
		TRACE( CAM_ERROR, "Error: out of memeory to allocate IRE handle( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}
	memset( pPpuState->hIreHandle, 0, sizeof( struct ire_info ) );

	ire_set_use_srcbuf( pPpuState->hIreHandle, 1 );
	pPpuState->hIreHandle->ctx = 0;

	if ( ire_open( pPpuState->hIreHandle, NULL ) < 0 )
	{
		TRACE( CAM_ERROR, "Error: open ire failed( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PPUFAILED;
	}
#else
	pPpuState->hIreHandle = NULL;
#endif

	*phHandle = pPpuState;

	return CAM_ERROR_NONE;
}

CAM_Error ppu_deinit( void **phHandle )
{
	_CAM_PpuState *pPpuState = ( _CAM_PpuState* )phHandle;
#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB )
	if ( pPpuState->hGcuContext != NULL )
	{
		gcuDestroyContext( pPpuState->hGcuContext );
		gcuTerminate();

		pPpuState->hGcuContext = NULL;
	}
#endif

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
	ire_close( pPpuState->hIreHandle );
	free( pPpuState->hIreHandle );
	pPpuState->hIreHandle = NULL;
#endif

	free( *phHandle );
	*phHandle = NULL;

	return CAM_ERROR_NONE;
}

CAM_Error ppu_query_jpegenc_cap( CAM_ImageFormat *pBaseFmtCap, CAM_Int32s iBaseFmtCapCnt, CAM_JpegCapability *pJpegEncCap )
{
	CAM_Int32s iIndex = -1;
	memset( pJpegEncCap, 0, sizeof( CAM_JpegCapability ) );

#if defined( BUILD_OPTION_STRATEGY_ENABLE_VMETAJPEGENCODER )
	_ppu_retrieve_format( VMETA_JPEG_SRCFMT, pBaseFmtCap, iBaseFmtCapCnt, &iIndex );
#endif

#if defined( BUILD_OPTION_STRATEGY_ENABLE_IPPJPEGENCODER )
	if ( iIndex < 0 )
	{
		_ppu_retrieve_format( IPP_JPEG_SRCFMT, pBaseFmtCap, iBaseFmtCapCnt, &iIndex );
	}
#endif

	if ( iIndex >= 0 )
	{
		pJpegEncCap->bSupportJpeg      = CAM_TRUE;
		pJpegEncCap->iMinQualityFactor = 5;
		pJpegEncCap->iMaxQualityFactor = 95;
		pJpegEncCap->bSupportExif      = CAM_FALSE;
		pJpegEncCap->bSupportThumb     = CAM_FALSE;
	}
	else
	{
		pJpegEncCap->bSupportJpeg      = CAM_FALSE;
	}

	return CAM_ERROR_NONE;
}

CAM_Error ppu_query_csc_cap( CAM_ImageFormat *pBaseFmtCap, CAM_Int32s iBaseFmtCapCnt, CAM_ImageFormat *pAddOnFmtCap, CAM_Int32s *pAddOnFmtCapCnt )
{
	CAM_Int32s cnt = 0;
	CAM_Int32s i   = 0;

#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB )
	// GCU can do UYVY --> BGR565 / BGRA8888 conversion
	// add BGR565 and BGRA8888 support if it is not natively supported
	{
		CAM_Bool bSupportCbYCrY = CAM_FALSE, bSupportBGR565 = CAM_FALSE, bSupportBGRA8888 = CAM_FALSE;

		for ( i = 0; i < iBaseFmtCapCnt; i++ )
		{
			if ( pBaseFmtCap[i] == CAM_IMGFMT_CbYCrY )
			{
				bSupportCbYCrY = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_BGR565 )
			{
				bSupportBGR565 = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_BGRA8888 )
			{
				bSupportBGRA8888 = CAM_TRUE;
			}
		}

		if ( bSupportCbYCrY && !bSupportBGR565 )
		{
			pAddOnFmtCap[cnt++] = CAM_IMGFMT_BGR565;
		}
		if ( bSupportCbYCrY && !bSupportBGRA8888 )
		{
			pAddOnFmtCap[cnt++] = CAM_IMGFMT_BGRA8888;
		}
        pAddOnFmtCap[cnt++] = CAM_IMGFMT_RGB565;
	}
#endif

#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_YCC420P_YCC420SP )
	{
		CAM_Bool bSupportYCbCr420P  = CAM_FALSE, bSupportYCrCb420P  = CAM_FALSE;
		CAM_Bool bSupportYCbCr420SP = CAM_FALSE, bSupportYCrCb420SP = CAM_FALSE;
        CAM_Bool bSupportYCbYCr = CAM_FALSE;

		for ( i = 0; i < iBaseFmtCapCnt; i++ )
		{
			if ( pBaseFmtCap[i] == CAM_IMGFMT_YCbCr420P )
			{
				bSupportYCbCr420P = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_YCrCb420P )
			{
				bSupportYCrCb420P = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_YCbCr420SP )
			{
				bSupportYCbCr420SP = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_YCrCb420SP )
			{
				bSupportYCrCb420SP = CAM_TRUE;
			}
			else if ( pBaseFmtCap[i] == CAM_IMGFMT_YCbYCr )
			{
				bSupportYCbYCr = CAM_TRUE;
			}
		}
		if ( bSupportYCbYCr )
		{
			pAddOnFmtCap[cnt++] = CAM_IMGFMT_YCbYCr;
		}
		if ( ( bSupportYCbCr420P || bSupportYCrCb420P ) && !bSupportYCbCr420SP )
		{
			pAddOnFmtCap[cnt++] = CAM_IMGFMT_YCbCr420SP;
		}
		if ( ( bSupportYCbCr420P || bSupportYCrCb420P ) && !bSupportYCrCb420SP )
		{
			pAddOnFmtCap[cnt++] = CAM_IMGFMT_YCrCb420SP;
		}
        pAddOnFmtCap[cnt++] = CAM_IMGFMT_YCrCb420SP;
	}
#endif

	*pAddOnFmtCapCnt = cnt;

	return CAM_ERROR_NONE;
}

CAM_Error ppu_query_rotator_cap( CAM_FlipRotate *pRotCap, CAM_Int32s *pRotCnt )
{
#if defined ( BUILD_OPTION_STRATEGY_ENABLE_ROTATOR )
	pRotCap[0] = CAM_FLIPROTATE_NORMAL;
	pRotCap[1] = CAM_FLIPROTATE_90L;
	pRotCap[2] = CAM_FLIPROTATE_90R;
	pRotCap[3] = CAM_FLIPROTATE_180;
	*pRotCnt   = 4;
#else
	pRotCap[0] = CAM_FLIPROTATE_NORMAL;
	*pRotCnt   = 1;
#endif

	return CAM_ERROR_NONE;
}

CAM_Error ppu_query_resizer_cap( CAM_Bool *pbIsArbitrary, CAM_Size *pMin, CAM_Size *pMax )
{
#if defined( BUILD_OPTION_STRATEGY_ENABLE_RESIZER )
	*pbIsArbitrary = CAM_TRUE;
	pMin->iWidth   = 64;
	pMin->iHeight  = 64;
	pMax->iWidth   = 1920;
	pMax->iHeight  = 1920;
#else
	*pbIsArbitrary = CAM_FALSE;
#endif
	return CAM_ERROR_NONE;
}

CAM_Error ppu_negotiate_format( CAM_ImageFormat eDstFmt, CAM_ImageFormat *pSrcFmt, CAM_ImageFormat *pFmtCap, CAM_Int32s iFmtCapCnt )
{
	CAM_Int32s      iIndex = -1;
	CAM_Error       error  = CAM_ERROR_NONE;
	CAM_ImageFormat eSrcFmt;

    ALOGE("ppu src: %d, dst %d\n", eDstFmt, pSrcFmt);
	switch ( eDstFmt )
	{
	case CAM_IMGFMT_JPEG:
#if defined ( BUILD_OPTION_STRATEGY_ENABLE_IPPJPEGENCODER )
		eSrcFmt = IPP_JPEG_SRCFMT;
#endif
#if defined ( BUILD_OPTION_STRATEGY_ENABLE_VMETAJPEGENCODER )
		eSrcFmt = VMETA_JPEG_SRCFMT;
#endif
		break;
    case CAM_IMGFMT_RGBA8888:   
	case CAM_IMGFMT_RGB565:
        eSrcFmt = CAM_IMGFMT_YCbYCr;
        break;

	case CAM_IMGFMT_BGR565:
	case CAM_IMGFMT_BGRA8888:
		eSrcFmt = CAM_IMGFMT_CbYCrY;
		break;

	/* case CAM_IMGFMT_BGRA8888: */
		/* eSrcFmt = CAM_IMGFMT_CbYCrY; */
		/* break; */
	/* CAM_IMGFMT_BGRA8888   = 2010, */
	/* CAM_IMGFMT_RGBA8888   = 2011, */

	case CAM_IMGFMT_YCbYCr:
		eSrcFmt = CAM_IMGFMT_BGRA8888;
		break;

	case CAM_IMGFMT_CbYCrY:
		eSrcFmt = CAM_IMGFMT_YCbCr420P;
		break;

	case CAM_IMGFMT_YCbCr420SP:
	case CAM_IMGFMT_YCrCb420SP:
        eSrcFmt = CAM_IMGFMT_YCbYCr;
		/* eSrcFmt = CAM_IMGFMT_YCbCr420P; */
		break;

	default:
		ASSERT( 0 );
		break;
	}

    int j;
    for (j = 0; j < iFmtCapCnt; j++ )
	{
        ALOGE("fmtcap: %d\n", pFmtCap[j]);
	}

	_ppu_retrieve_format( eSrcFmt, pFmtCap, iFmtCapCnt, &iIndex );
	if ( iIndex < 0 )
	{
		TRACE( CAM_ERROR, "Error: ppu cannot handle generate image to format[%d]( %s, %s, %d )\n", eDstFmt, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
	else
	{
		*pSrcFmt = eSrcFmt;
		error = CAM_ERROR_NONE;
	}

	return error;
}

CAM_Error ppu_get_bufreq( void *hHandle, _CAM_ImageInfo *pImgInfo, CAM_ImageBufferReq *pBufReq )
{
	void   *pUserData = NULL;

	// BAC check
	_CHECK_BAD_POINTER( pImgInfo );
	_CHECK_BAD_POINTER( pBufReq );

	pBufReq->eFormat = pImgInfo->eFormat;
	if ( pImgInfo->eRotation == CAM_FLIPROTATE_90L || pImgInfo->eRotation == CAM_FLIPROTATE_90R ||
	     pImgInfo->eRotation == CAM_FLIPROTATE_DFLIP || pImgInfo->eRotation == CAM_FLIPROTATE_ADFLIP )
	{
		pBufReq->iWidth  = pImgInfo->iHeight;
		pBufReq->iHeight = pImgInfo->iWidth;
	}
	else
	{
		pBufReq->iWidth  = pImgInfo->iWidth;
		pBufReq->iHeight = pImgInfo->iHeight;
	}

	// ipp lib, gcu requirement
	pBufReq->iAlignment[0] = 32;
	if ( pImgInfo->eFormat == CAM_IMGFMT_YCbCr444P || pImgInfo->eFormat == CAM_IMGFMT_YCbCr422P || pImgInfo->eFormat == CAM_IMGFMT_YCbCr420P || pImgInfo->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		pBufReq->iAlignment[1] = 32;
		pBufReq->iAlignment[2] = 32;
	}
	else if ( pImgInfo->eFormat == CAM_IMGFMT_YCbCr420SP || pImgInfo->eFormat == CAM_IMGFMT_YCrCb420SP )
	{
		pBufReq->iAlignment[1] = 32;
		pBufReq->iAlignment[2] = 0;
	}
	else
	{
		pBufReq->iAlignment[1] = 0;
		pBufReq->iAlignment[2] = 0;
	}

	pBufReq->iRowAlign[0] = 1;
	if ( pImgInfo->eFormat == CAM_IMGFMT_YCbCr444P || pImgInfo->eFormat == CAM_IMGFMT_YCbCr422P || pImgInfo->eFormat == CAM_IMGFMT_YCbCr420P || pImgInfo->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		pBufReq->iRowAlign[0] = 8;
		pBufReq->iRowAlign[1] = 8;
		pBufReq->iRowAlign[2] = 8;
	}
	else if ( pImgInfo->eFormat == CAM_IMGFMT_YCbCr420SP || pImgInfo->eFormat == CAM_IMGFMT_YCrCb420SP )
	{
		pBufReq->iRowAlign[0] = 8;
		pBufReq->iRowAlign[1] = 8;
		pBufReq->iRowAlign[2] = 0;
	}
	else
	{
		pBufReq->iRowAlign[1] = 0;
		pBufReq->iRowAlign[2] = 0;
	}

	pBufReq->iFlagOptimal = BUF_FLAG_PHYSICALCONTIGUOUS |
	                        BUF_FLAG_L1CACHEABLE | BUF_FLAG_L2CACHEABLE | BUF_FLAG_BUFFERABLE |
	                        BUF_FLAG_YUVBACKTOBACK | BUF_FLAG_FORBIDPADDING;

	pBufReq->iFlagAcceptable = pBufReq->iFlagOptimal | BUF_FLAG_L1NONCACHEABLE | BUF_FLAG_L2NONCACHEABLE | BUF_FLAG_UNBUFFERABLE;

	pBufReq->iMinBufCount = ( pImgInfo->bIsStreaming == CAM_TRUE ) ? 2 : 1;
	_calcstep( pBufReq->eFormat, pBufReq->iWidth, pBufReq->iRowAlign, pBufReq->iMinStep );

	pUserData = NULL;
	// print JPEG buffer size infomation
	if ( pBufReq->eFormat == CAM_IMGFMT_JPEG )
	{
		pUserData = pImgInfo->pJpegParam;
		ASSERT( pUserData != NULL );

		TRACE( CAM_INFO, "Info: JPEG width: %d, height: %d, quality: %d\n", pBufReq->iWidth, pBufReq->iHeight,
		       pImgInfo->pJpegParam->iQualityFactor );
	}

	_calcbuflen( pBufReq->eFormat, pBufReq->iWidth, pBufReq->iHeight, pBufReq->iMinStep, pBufReq->iMinBufLen, pUserData );

	/*
	CELOG( "Info: format = %d, width = %d, height = %d, align = %d, step = %d, %d, %d, size = %d, %d, %d\n",
	       pBufReq->eFormat, pBufReq->iWidth, pBufReq->iHeight, pBufReq->iRowAlign,
	       pBufReq->iMinStep[0], pBufReq->iMinStep[1], pBufReq->iMinStep[2],
	       pBufReq->iMinBufLen[0], pBufReq->iMinBufLen[1], pBufReq->iMinBufLen[2] );
	*/

	return CAM_ERROR_NONE;
}


/* all Camera Engine's post-processing in included in this function, currently features include:
 *  1. image rotate
 *  2. image resize
 * *3. color space conversion( recommend use Marvell's CCIC to do it in camera driver if possible )
 *  4. JPEG encoder + rotate
*/
static CAM_Error _gcu_yuv2rgb_rszrot_roi( void *hGcuContext, CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
static CAM_Error _ire_yuv420p_rot( void *hIreHandle, CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_FlipRotate eRotate );
#endif

static CAM_Error _ipp_yuv_csc_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_Bool bIsInPlace );
static CAM_Error _ipp_jpegdec_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_tojpeg_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_JpegParam *pJpegParam );

static CAM_Error _ipp_ycc420p_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_yuv422p_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_uyvy_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_ycc420p2sp_rszrot_roi_swap( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_Bool bIsInPlace );
static CAM_Error _ipp_yuv420p2uyvy_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_yuyv2yuv420sp_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate );
static CAM_Error _ipp_jpeg_rotation( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_FlipRotate eRotate );
static CAM_Error _ipp_yuv2jpeg( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_JpegParam *pJpegParam );
static CAM_Error _vmeta_yuv2jpeg( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_JpegParam *pJpegParam );
static CAM_Error _ipp_jpeg_roidec( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI );

static IppStatus ippiYCbYCrToYUV420SP( Ipp8u *pSrc[3], int *pSrcStep, IppiSize stSrcSize, Ipp8u *pDst[3], int *pDstStep, IppiSize stDstSize );
static IppStatus ippiYCbCr420ToCbYCrY_8u_P3C2R_C( Ipp8u *pSrc[3], int *pSrcStep, IppiSize stSrcSize, Ipp8u *pDst[3], int *pDstStep, IppiSize stDstSize );
static IppStatus ippiYCbCr422RszRot_8u_C2R_C( const Ipp8u *pSrc, int srcStep, 
                                              IppiSize srcSize, Ipp8u *pDst, int dstStep, IppiSize dstSize, 
                                              IppCameraInterpolation interpolation, IppCameraRotation rotation,
                                              int rcpRatiox, int rcpRatioy );
// temporal workaround for nevo release ICS3, will be removed after neon optimized I420 resize rotate is done.
static IppStatus ippiYCbCr420RszRot_8u_P3R_C( const Ipp8u *pSrc[3], int srcStep[3], IppiSize srcSize, Ipp8u *pDst[3],
                                            int dstStep[3], IppiSize dstSize, IppCameraInterpolation interpolation,
                                            IppCameraRotation rotation, int rcpRatiox, int rcpRatioy );
static IppStatus ippiYCbCr422RszRot_8u_P3R_C( const Ipp8u *pSrc[3], int srcStep[3],
                                              IppiSize srcSize, Ipp8u *pDst[3], int dstStep[3], IppiSize dstSize,
                                              IppCameraInterpolation interpolation, IppCameraRotation rotation,
                                              int rcpRatiox, int rcpRatioy );
static void      _ppu_copy_image( Ipp8u *pSrc, int iSrcStep, Ipp8u *pDst, int iDstStep, int iBytesPerLine, int iLines );

CAM_Error ppu_image_process( void *hHandle, CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, _CAM_PostProcessParam *pPostProcessParam )
{
	_CAM_PpuState  *pPpuState    = (_CAM_PpuState*)hHandle;
	CAM_Error      error         = CAM_ERROR_NONE;
	CAM_FlipRotate eRotate;
	CAM_Rect       stCrop, stROI;

	eRotate = pPostProcessParam->eRotation;

	if ( pPostProcessParam->bFavorAspectRatio )
	{
		CAM_Int32s iWidthBeforeRotate, iHeightBeforeRotate;

		if ( eRotate == CAM_FLIPROTATE_90L || eRotate == CAM_FLIPROTATE_90R ||
		     eRotate == CAM_FLIPROTATE_DFLIP || eRotate == CAM_FLIPROTATE_ADFLIP )
		{
			iWidthBeforeRotate  = pDstImg->iHeight;
			iHeightBeforeRotate = pDstImg->iWidth;
		}
		else
		{
			iWidthBeforeRotate  = pDstImg->iWidth;
			iHeightBeforeRotate = pDstImg->iHeight;
		}

		_calccrop( pSrcImg->iWidth, pSrcImg->iHeight, iWidthBeforeRotate, iHeightBeforeRotate, &stCrop );
	}
	else
	{
		stCrop.iTop    = 0;
		stCrop.iLeft   = 0;
		stCrop.iHeight = pSrcImg->iHeight;
		stCrop.iWidth  = pSrcImg->iWidth;
	}

	if ( stCrop.iWidth & 0x000f )
	{
		TRACE( CAM_ERROR, "Error: ppu cannot handle image whose width[%d] is not 16 pixel align( %s, %s, %d )\n",\
		       stCrop.iWidth, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	// calc zoom ROI based on the crop
	if ( stCrop.iWidth & 0x0010 )
	{
		// odd 16 pixel align
		CAM_Int32s iWidthFloor, iWidthCeil;
		iWidthFloor = _FLOOR_TO( ( (CAM_Int64s)stCrop.iWidth << 16 ) / pPostProcessParam->iPpuDigitalZoomQ16, 16 );
		iWidthCeil  = _ALIGN_TO( ( (CAM_Int64s)stCrop.iWidth << 16 ) / pPostProcessParam->iPpuDigitalZoomQ16, 16 );

		if ( iWidthFloor & 0x0010 )
		{
			stROI.iWidth = iWidthFloor;
		}
		else if ( iWidthCeil & 0x0010 )
		{
			stROI.iWidth = iWidthCeil;
		}
		else
		{
			stROI.iWidth = _MAX( iWidthFloor - 16, 0 );
		}
	}
	else
	{
		// 32 pixel align
		stROI.iWidth  = _ROUND_TO( ( (CAM_Int64s)stCrop.iWidth << 16 ) / pPostProcessParam->iPpuDigitalZoomQ16, 32 );
	}
	stROI.iWidth  = _MIN( stROI.iWidth, stCrop.iWidth );

	stROI.iHeight = ( (CAM_Int64s)stCrop.iHeight << 16 ) / pPostProcessParam->iPpuDigitalZoomQ16;
	stROI.iHeight = _MIN( stROI.iHeight, stCrop.iHeight );

	stROI.iTop    = stCrop.iTop + ( stCrop.iHeight - stROI.iHeight ) / 2;
	stROI.iLeft   = _ROUND_TO( stCrop.iLeft + ( stCrop.iWidth - stROI.iWidth ) / 2, 16 );

    /* ALOGE("ppu trace line:stROI.iWidth %d stROI.iHeight %d stROI.iTop: %d stROI.iLeft: %d\n", stROI.iWidth, stROI.iHeight,  */
            /* stROI.iTop, stROI.iLeft); */
	// gcu yuv->rgb resize/rotate/zoom
	if ( CAM_IMGFMT_CbYCrY == pSrcImg->eFormat && 
	     ( CAM_IMGFMT_BGR565 == pDstImg->eFormat || CAM_IMGFMT_BGRA8888 == pDstImg->eFormat ) )
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		ASSERT( pPostProcessParam->bIsInPlace == CAM_FALSE );
		error = _gcu_yuv2rgb_rszrot_roi( pPpuState->hGcuContext, pSrcImg, pDstImg, &stROI, eRotate );
	}

    /* else if ( CAM_IMGFMT_YCbYCr == pSrcImg->eFormat ) */
	/* { */
        /* pDstImg->eFormat = CAM_IMGFMT_RGBA8888; */
        /* [> ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); <] */
		/* ASSERT( pPostProcessParam->bIsInPlace == CAM_FALSE ); */
		/* error = _gcu_yuv2rgb_rszrot_roi( pPpuState->hGcuContext, pSrcImg, pDstImg, &stROI, eRotate ); */
	/* } */

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
	// ire rotate
	else if ( ( CAM_IMGFMT_YCbCr420P == pSrcImg->eFormat || CAM_IMGFMT_YCrCb420P == pSrcImg->eFormat ) &&
	          pSrcImg->eFormat == pDstImg->eFormat &&
	          pROI->iWidth == pDstImg->iHeight && pROI->iHeight == pDstImg->iWidth &&
	          ( eRotate == CAM_FLIPROTATE_90L || eRotate == CAM_FLIPROTATE_90R ) &&
	          pROI->iWidth == pSrcImg->iStep[0] )
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		error = _ire_yuv420p_rot( pPpuState->hIreHandle, pSrcImg, pDstImg, eRotate );
	}
#endif

	// ipp yuv data resize/rotate/zoom
	else if ( CAM_IMGFMT_JPEG != pSrcImg->eFormat && CAM_IMGFMT_JPEG != pDstImg->eFormat )
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		error = _ipp_yuv_csc_rszrot_roi( pSrcImg, pDstImg, &stROI, eRotate, pPostProcessParam->bIsInPlace );
	}
	// JPEG snapshot generator
	else if ( CAM_IMGFMT_JPEG == pSrcImg->eFormat && CAM_IMGFMT_JPEG != pDstImg->eFormat )
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		ASSERT( pPostProcessParam->bIsInPlace == CAM_FALSE );
		error = _ipp_jpegdec_rszrot_roi( pSrcImg, pDstImg, &stROI, eRotate );
	}
	// YUV/JPEG to JPEG
	else if ( pDstImg->eFormat == CAM_IMGFMT_JPEG )
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		ASSERT( pPostProcessParam->bIsInPlace == CAM_FALSE );
		ASSERT( eRotate == CAM_FLIPROTATE_NORMAL );
		error = _ipp_tojpeg_rszrot_roi( pSrcImg, pDstImg, &stROI, eRotate, pPostProcessParam->pJpegParam );
	}
	else
	{
    /* ALOGE("ppu trace line: %s %d\n", __FILE__, __LINE__); */
		TRACE( CAM_ERROR, "Error: unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
		       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	return error;
}

static CAM_Error _gcu_yuv2rgb_rszrot_roi( void *hGcuContext, CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error  error = CAM_ERROR_NONE;

#if defined( BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB )
	_CHECK_BAD_POINTER( hGcuContext );

	if ( !((int)pSrcImg->pBuffer[0] & 31) && !(pSrcImg->iStep[0] & 31) &&
	     !((int)pDstImg->pBuffer[0] & 31) && !(pDstImg->iStep[0] & 31) )
	{
		GCU_BLT_DATA stBltData;
		GCU_RECT     stSrcRect, stDstRect;

        /* ALOGE("ppu pSrcImg->iStep[0] %d pSrcImg->iWidth %d pSrcImg->iHeight %d", pSrcImg->iStep[0], pSrcImg->iWidth, */
                /* pSrcImg->iHeight); */
        /* ALOGE("ppu pDstImg->iStep[0] %d pDstImg->iWidth %d pDstImg->iHeight %d", pDstImg->iStep[0], pDstImg->iWidth, */
                /* pDstImg->iHeight); */

		memset( &stBltData, 0, sizeof( GCU_BLT_DATA ) );
			stBltData.pSrcSurface = _gcuCreatePreAllocBuffer( hGcuContext, pSrcImg->iWidth, pSrcImg->iHeight, GCU_FORMAT_YUY2,
			                                                  GCU_TRUE, pSrcImg->pBuffer[0], pSrcImg->iPhyAddr[0] == 0 ? GCU_FALSE : GCU_TRUE,
			                                                  pSrcImg->iPhyAddr[0] );
        stSrcRect.left     = pROI->iLeft;
        stSrcRect.top      = pROI->iTop;
        stSrcRect.right    = pROI->iWidth;
        stSrcRect.bottom   = pROI->iHeight;
		stBltData.pSrcRect = &stSrcRect;

        stBltData.pDstSurface = _gcuCreatePreAllocBuffer( hGcuContext, pDstImg->iStep[0] >> 2, pDstImg->iHeight, GCU_FORMAT_ABGR8888,
                                                          GCU_TRUE, pDstImg->pBuffer[0], pDstImg->iPhyAddr[0] == 0 ? GCU_FALSE : GCU_TRUE,
                                                          pDstImg->iPhyAddr[0] );

		stDstRect.left     = 0;
		stDstRect.top      = 0;
		stDstRect.right    = pDstImg->iWidth;
		stDstRect.bottom   = pDstImg->iHeight;
		stBltData.pDstRect = &stDstRect;

		switch ( eRotate )
		{
		case CAM_FLIPROTATE_NORMAL:
			stBltData.rotation = GCU_ROTATION_0;
			break;

		case CAM_FLIPROTATE_90L:
			stBltData.rotation = GCU_ROTATION_90;
			break;

		case CAM_FLIPROTATE_90R:
			stBltData.rotation = GCU_ROTATION_270;
			break;

		case CAM_FLIPROTATE_180:
			stBltData.rotation = GCU_ROTATION_180;
			break;

		default:
			TRACE( CAM_ERROR, "Error: unsupported rotate[%d]( %s, %s, %d )\n", eRotate, __FILE__, __FUNCTION__, __LINE__ );
			ASSERT( 0 );
			break;
		}
		/**********************************************************************************************************
		 * normal quality is two steps solution ( 1. horizontal blit + vertical filter blit; 2. rotate )
		 * high quality is three steps solution ( 1. horizontal filter blit; 2. vertical filter blit; 3. rotate )
		 * We suggest always use the normal quality for performance reason
		***********************************************************************************************************/
		// gcuSet( hGcuContext, GCU_QUALITY, GCU_QUALITY_HIGH );

		gcuBlit( hGcuContext, &stBltData );
		gcuFinish( hGcuContext );

		pDstImg->iFilledLen[0] = pDstImg->iHeight * pDstImg->iStep[0];
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: GCU resize/rotate requires 32 bytes align in starting address/row size( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		
		pDstImg->iFilledLen[0] = 0;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;

		error = CAM_ERROR_NOTSUPPORTEDARG;
	}
#else
	TRACE( CAM_ERROR, "Error: Pls open macro BUILD_OPTION_STRATEGY_ENABLE_CSC_CBYCRY_RGB to enable converter to BGR565/BGRA8888( %s, %s, %d )\n",
	       __FILE__, __FUNCTION__, __LINE__ );
	error = CAM_ERROR_NOTSUPPORTEDARG;
#endif

	return error;
}

static CAM_Error _ipp_yuv_csc_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_Bool bIsInPlace )
{
	CAM_Error error = CAM_ERROR_NONE;

	// from ycc420 planar
	if ( CAM_IMGFMT_YCbCr420P == pSrcImg->eFormat || CAM_IMGFMT_YCrCb420P == pSrcImg->eFormat )
	{
		if ( pSrcImg->eFormat == pDstImg->eFormat )
		{
			ASSERT( bIsInPlace == CAM_FALSE );
			error = _ipp_ycc420p_rszrot_roi( pSrcImg, pDstImg, pROI, eRotate );
		}
		else if ( CAM_IMGFMT_YCbCr420SP == pDstImg->eFormat || CAM_IMGFMT_YCrCb420SP == pDstImg->eFormat )
		{
			error = _ipp_ycc420p2sp_rszrot_roi_swap( pSrcImg, pDstImg, pROI, eRotate, bIsInPlace );
		}
		else if ( CAM_IMGFMT_YCbCr420P == pSrcImg->eFormat && CAM_IMGFMT_CbYCrY == pDstImg->eFormat )
		{
			ASSERT( bIsInPlace == CAM_FALSE );
			error = _ipp_yuv420p2uyvy_rszrot_roi( pSrcImg, pDstImg, pROI, eRotate );
		}
		else
		{
			TRACE( CAM_ERROR, "Error: IPP primitives unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
			       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
	}
	// from yuv422 planar
	else if ( CAM_IMGFMT_YCbCr422P == pSrcImg->eFormat )
	{
		if ( CAM_IMGFMT_YCbCr422P == pDstImg->eFormat )
		{
			ASSERT( bIsInPlace == CAM_FALSE );
			error = _ipp_yuv422p_rszrot_roi( pSrcImg, pDstImg, pROI, eRotate );
		}
		else
		{
			TRACE( CAM_ERROR, "Error: IPP primitives unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
			       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
	}
	// from yuyv
	else if ( CAM_IMGFMT_YCbYCr == pSrcImg->eFormat )
	{
		if ( CAM_IMGFMT_YCbYCr == pDstImg->eFormat )
		{
			ASSERT( bIsInPlace == CAM_FALSE );
			error = _ipp_uyvy_rszrot_roi( pSrcImg, pDstImg, pROI, eRotate );
		}

        else if ( CAM_IMGFMT_YCrCb420SP == pDstImg->eFormat )
        {
			error = _ipp_yuyv2yuv420sp_roi( pSrcImg, pDstImg, pROI, eRotate );
        }
		else
		{
			TRACE( CAM_ERROR, "Error: IPP primitives unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
			       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
	}	// from uyvy
	else if ( CAM_IMGFMT_CbYCrY == pSrcImg->eFormat )
	{
		if ( CAM_IMGFMT_CbYCrY == pDstImg->eFormat )
		{
			ASSERT( bIsInPlace == CAM_FALSE );
			error = _ipp_uyvy_rszrot_roi( pSrcImg, pDstImg, pROI, eRotate );
		}
		else
		{
			TRACE( CAM_ERROR, "Error: IPP primitives unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
			       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
			error = CAM_ERROR_NOTSUPPORTEDCMD;
		}
	}
	else
	{
		TRACE( CAM_ERROR, "Error: IPP primitives unsupported format conversion[%d --> %d]( %s, %s, %d )\n", \
		       pSrcImg->eFormat, pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDCMD;
	}

	return error;
}

static CAM_Error _ipp_jpegdec_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error error = CAM_ERROR_NONE;

	ASSERT( pSrcImg->pBuffer[0] != NULL );

#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software jpeg snapshot generating on non-cacheable source buffer results in low performance!\n" );
	}
#endif

	if ( pDstImg->eFormat != CAM_IMGFMT_CbYCrY && pDstImg->eFormat != CAM_IMGFMT_YCbCr420SP &&
	     pDstImg->eFormat != CAM_IMGFMT_YCrCb420SP && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		error = _ipp_jpeg_roidec( pSrcImg, pDstImg, pROI );
	}
	else
	{
		CAM_ImageBuffer     stTmpImg;
		CAM_Rect            stROIAfterZoom;
		CAM_Int8u           *pHeap          = NULL;

		memset( &stTmpImg, 0, sizeof( CAM_ImageBuffer ) );

		pHeap = ( CAM_Int8u* )malloc( pDstImg->iHeight * pDstImg->iWidth * 2 + 128 );
		if ( pHeap == NULL )
		{
			TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}

		stTmpImg.iWidth  = pDstImg->iWidth;
		stTmpImg.iHeight = pDstImg->iHeight;

#if 0
		CELOG( "Info:\
		        the pROI->iWidth:%d, pROI->iHeight:%d, pDstImg->iWidth:%d, pDstImg->iHeight:%d, \n\
		        stTmpImg.iWidth:%d, stTmpImg.iHeight:%d \n",
		        pROI->iWidth, pROI->iHeight, pDstImg->iWidth, pDstImg->iHeight,
		        stTmpImg.iWidth, stTmpImg.iHeight );
#endif

		stTmpImg.eFormat  = CAM_IMGFMT_YCbCr420P;
		stTmpImg.iStep[0] = _ALIGN_TO( stTmpImg.iWidth, 8 );
		stTmpImg.iStep[1] = _ALIGN_TO( stTmpImg.iWidth >> 1, 8 );
		stTmpImg.iStep[2] = _ALIGN_TO( stTmpImg.iWidth >> 1, 8 );

		stTmpImg.pBuffer[0] = (CAM_Int8u*)_ALIGN_TO( pHeap, 8 );
		stTmpImg.pBuffer[1] = stTmpImg.pBuffer[0] + stTmpImg.iStep[0] * stTmpImg.iHeight;
		stTmpImg.pBuffer[2] = stTmpImg.pBuffer[1] + stTmpImg.iStep[1] * ( stTmpImg.iHeight >> 1 );

		stTmpImg.iFilledLen[0] = stTmpImg.iStep[0] * stTmpImg.iHeight;
		stTmpImg.iFilledLen[1] = stTmpImg.iStep[1] * ( stTmpImg.iHeight >> 1 );
		stTmpImg.iFilledLen[2] = stTmpImg.iStep[2] * ( stTmpImg.iHeight >> 1 );

		// 1. first decode to a YCbCr420P image with the same size to dst image
		error = _ipp_jpeg_roidec( pSrcImg, &stTmpImg, pROI );
		if ( error != CAM_ERROR_NONE )
		{
			free( pHeap );
			pHeap = NULL;

			return error;
		}

		stROIAfterZoom.iLeft   = 0;
		stROIAfterZoom.iTop    = 0;
		stROIAfterZoom.iHeight = stTmpImg.iHeight;
		stROIAfterZoom.iWidth  = stTmpImg.iWidth;

		// 2. second to do rotation and color conversion
		error = _ipp_yuv_csc_rszrot_roi( &stTmpImg, pDstImg, &stROIAfterZoom, eRotate, CAM_FALSE );
		free( pHeap );
		pHeap = NULL;
	}

	return error;
}

static CAM_Error _ipp_tojpeg_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_JpegParam *pJpegParam )
{
	CAM_Error       error = CAM_ERROR_NONE;
	CAM_ImageBuffer stUYVYImgAfterZoom, stImgBeforeZoom, stImgAfterZoom;
	CAM_Int8u       *pHeapUYVYAfterZoom = NULL, *pHeapBeforeZoom = NULL, *pHeapAfterZoom = NULL;
	CAM_Rect        stZoomROI;

	memset( &stImgBeforeZoom, 0, sizeof( CAM_ImageBuffer ) );
	memset( &stImgAfterZoom, 0, sizeof( CAM_ImageBuffer ) );

	// source format: UYVY case
    /* if ( pSrcImg->eFormat == CAM_IMGFMT_CbYCrY ) */
    if ( pSrcImg->eFormat == CAM_IMGFMT_YCbYCr )
	{
		// resize/rotate
		if ( pROI->iHeight == pDstImg->iHeight && pROI->iWidth == pDstImg->iWidth &&
		     pROI->iTop == 0 && pROI->iLeft == 0 && eRotate == CAM_FLIPROTATE_NORMAL )
		{
			stUYVYImgAfterZoom = *pSrcImg;
		}
		else
		{
			stUYVYImgAfterZoom.iHeight = pDstImg->iHeight;
			stUYVYImgAfterZoom.iWidth  = pDstImg->iWidth;

			stUYVYImgAfterZoom.eFormat  = pSrcImg->eFormat;
			stUYVYImgAfterZoom.iStep[0] = _ALIGN_TO( stUYVYImgAfterZoom.iWidth << 1, 8 );

			pHeapUYVYAfterZoom = ( CAM_Int8u* )osalbmm_malloc( stUYVYImgAfterZoom.iHeight * stUYVYImgAfterZoom.iStep[0], OSALBMM_ATTR_DEFAULT ); // cacheable & bufferable
			if ( pHeapUYVYAfterZoom == NULL )
			{
				TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )!\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_OUTOFMEMORY;
			}

			stUYVYImgAfterZoom.pBuffer[0]    = (CAM_Int8u*)_ALIGN_TO( pHeapUYVYAfterZoom, 8 );
			stUYVYImgAfterZoom.iPhyAddr[0]   = (CAM_Int32u)osalbmm_get_paddr( stUYVYImgAfterZoom.pBuffer[0] );
			stUYVYImgAfterZoom.iAllocLen[0]  = stUYVYImgAfterZoom.iHeight * stUYVYImgAfterZoom.iStep[0];
			stUYVYImgAfterZoom.iFilledLen[0] = stUYVYImgAfterZoom.iHeight * stUYVYImgAfterZoom.iStep[0];
			stUYVYImgAfterZoom.iOffset[0]    = 0;

			// invalidate cache line
			osalbmm_flush_cache_range( pHeapUYVYAfterZoom, stUYVYImgAfterZoom.iHeight * stUYVYImgAfterZoom.iStep[0], OSALBMM_DMA_FROM_DEVICE );

			error = _ipp_yuv_csc_rszrot_roi( pSrcImg, &stUYVYImgAfterZoom, pROI, eRotate, CAM_FALSE );
			ASSERT( error == CAM_ERROR_NONE );

			// flush cache line to pmem
			osalbmm_flush_cache_range( pHeapUYVYAfterZoom, stUYVYImgAfterZoom.iHeight * stUYVYImgAfterZoom.iStep[0], OSALBMM_DMA_TO_DEVICE );
		}

        /* YUYVJpegCompressor compressor; */
        /* CAM_ImageBuffer* pImgBuf_still = &stUYVYImgAfterZoom; */
        /* status_t ret = compressor.compressRawImage(pImgBuf_still->pBuffer[0], pImgBuf_still->iWidth, pImgBuf_still->iHeight, 90); */
        /* if (ret == NO_ERROR) { */
            /* compressor.getCompressedImage(pDstImg->pBuffer[0]); */
            /* pDstImg->setLen(compressor.getCompressedSize()); */
        /* } else { */
            /* ALOGE("%s(): compression failure", __func__); */
            /* return -ENODATA; */
        /* } */

		// vmeta JPEG encoder
        error = _vmeta_yuv2jpeg( &stUYVYImgAfterZoom, pDstImg, pJpegParam );

		if ( pHeapUYVYAfterZoom != NULL )
		{
			free( pHeapUYVYAfterZoom );
			pHeapUYVYAfterZoom = NULL;
		}

		return error;
	}

	// source format: JPEG & other case
	if ( pSrcImg->eFormat == CAM_IMGFMT_JPEG )
	{
		// JPEG rotation case
		if ( pROI->iHeight == pDstImg->iHeight && pROI->iWidth == pDstImg->iWidth &&
		     pROI->iTop == 0 && pROI->iLeft == 0 )
		{
			if ( eRotate != CAM_FLIPROTATE_NORMAL )
			{
				error = _ipp_jpeg_rotation( pSrcImg, pDstImg, eRotate );
				return error;
			}
			else
			{
				// directly copy image data
				TRACE( CAM_WARN, "Warning: redundant memory copy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				memmove( pDstImg->pBuffer[0], pSrcImg->pBuffer[0], pSrcImg->iFilledLen[0] );
				pDstImg->iFilledLen[0] = pSrcImg->iFilledLen[0];
				return error;
			}
		}
		// need use ROI decoder to get raw picture 
		else
		{
			stImgBeforeZoom.iHeight = pROI->iHeight;
			stImgBeforeZoom.iWidth  = pROI->iWidth;

			stImgBeforeZoom.eFormat  = CAM_IMGFMT_YCbCr422P;
			stImgBeforeZoom.iStep[0] = _ALIGN_TO( stImgBeforeZoom.iWidth, 8 );
			stImgBeforeZoom.iStep[1] = _ALIGN_TO( stImgBeforeZoom.iWidth >> 1, 8 );
			stImgBeforeZoom.iStep[2] = _ALIGN_TO( stImgBeforeZoom.iWidth >> 1, 8 );

			pHeapBeforeZoom = ( CAM_Int8u* )malloc( pROI->iHeight * ( stImgBeforeZoom.iStep[0] + stImgBeforeZoom.iStep[1] + stImgBeforeZoom.iStep[2] ) + 128 );
			if ( pHeapBeforeZoom == NULL )
			{
				TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )!\n",
				       __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_OUTOFMEMORY;
			}

			stImgBeforeZoom.pBuffer[0] = (CAM_Int8u*)_ALIGN_TO( pHeapBeforeZoom, 8 );
			stImgBeforeZoom.pBuffer[1] = stImgBeforeZoom.pBuffer[0] + stImgBeforeZoom.iStep[0] * stImgBeforeZoom.iHeight;
			stImgBeforeZoom.pBuffer[2] = stImgBeforeZoom.pBuffer[1] + stImgBeforeZoom.iStep[1] * stImgBeforeZoom.iHeight;

			error = _ipp_jpeg_roidec( pSrcImg, &stImgBeforeZoom, pROI );
			ASSERT( error == CAM_ERROR_NONE );

			stZoomROI.iTop    = 0;
			stZoomROI.iLeft   = 0;
			stZoomROI.iHeight = pROI->iHeight;
			stZoomROI.iWidth  = pROI->iWidth;
		}
	}
	else
	{
		stImgBeforeZoom = *pSrcImg;
		stZoomROI       = *pROI;
	}

	// resize/rotate
	if ( stZoomROI.iHeight == pDstImg->iHeight && stZoomROI.iWidth == pDstImg->iWidth && 
	     stZoomROI.iTop == 0 && stZoomROI.iLeft == 0 && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		stImgAfterZoom = stImgBeforeZoom;
	}
	else
	{
		stImgAfterZoom.iHeight = pDstImg->iHeight;
		stImgAfterZoom.iWidth  = pDstImg->iWidth;

		stImgAfterZoom.eFormat  = stImgBeforeZoom.eFormat;
		stImgAfterZoom.iStep[0] = _ALIGN_TO( stImgAfterZoom.iWidth, 8 );
		stImgAfterZoom.iStep[1] = _ALIGN_TO( stImgAfterZoom.iWidth >> 1, 8 );
		stImgAfterZoom.iStep[2] = _ALIGN_TO( stImgAfterZoom.iWidth >> 1, 8 );

		pHeapAfterZoom = ( CAM_Int8u* )malloc( pDstImg->iHeight * ( stImgAfterZoom.iStep[0] + stImgAfterZoom.iStep[1] + stImgAfterZoom.iStep[2] ) + 128 );
		if ( pHeapAfterZoom == NULL )
		{	
			TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )!\n", __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_OUTOFMEMORY;
		}

		stImgAfterZoom.pBuffer[0] = (CAM_Int8u*)_ALIGN_TO( pHeapAfterZoom, 8 );
		stImgAfterZoom.pBuffer[1] = stImgAfterZoom.pBuffer[0] + stImgAfterZoom.iStep[0] * stImgAfterZoom.iHeight;
		stImgAfterZoom.pBuffer[2] = stImgAfterZoom.pBuffer[1] + stImgAfterZoom.iStep[1] * stImgAfterZoom.iHeight;

		error = _ipp_yuv_csc_rszrot_roi( &stImgBeforeZoom, &stImgAfterZoom, &stZoomROI, eRotate, CAM_FALSE );
		ASSERT( error == CAM_ERROR_NONE );
	}

	// ipp JPEG encoder
	error = _ipp_yuv2jpeg( &stImgAfterZoom, pDstImg, pJpegParam );

	if ( pHeapBeforeZoom != NULL )
	{
		free( pHeapBeforeZoom );
		pHeapBeforeZoom = NULL;
	}
	if ( pHeapAfterZoom != NULL )
	{
		free( pHeapAfterZoom );
		pHeapAfterZoom = NULL;
	}

	return error;
}

#if ( PLATFORM_IRE_VALUE != PLATFORM_IRE_NONE )
static CAM_Error _ire_yuv420p_rot( void *hIreHandle, CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_FlipRotate eRotate )
{
	struct ire_info  *pIreState    = (struct ire_info*)hIreHandle;
	IRE_LIB_ROTATION ire_rotation = ( eRotate == CAM_FLIPROTATE_90R ) ? IRE_LIB_ROT_90 : IRE_LIB_ROT_270;
	struct v_frame   vsrc, vdst;
	CAM_Int32s       ret;

	ASSERT( pIreState != NULL );

	pPpuState->stIreState.rotate_degree = ire_rotation;

	vsrc.width  = pSrcImg->iWidth;
	vsrc.height = pSrcImg->iHeight;

	vsrc.start[0]  = pSrcImg->pBuffer[0];
	vsrc.length[0] = pSrcImg->iAllocLen[0] + pSrcImg->iAllocLen[1] + pSrcImg->iAllocLen[2];

	vdst.width  = pDstImg->iWidth;
	vdst.height = pDstImg->iHeight;

	vdst.start[0]  = pDstImg->pBuffer[0];
	vdst.length[0] = pDstImg->iAllocLen[0] + pDstImg->iAllocLen[1] + pDstImg->iAllocLen[2];

	ret = ire_process( pIreState, &vsrc, &vdst );
	ASSERT( ret == 0 );

	return CAM_ERROR_NONE;
}
#endif

static CAM_Error _ipp_ycc420p_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error error       = CAM_ERROR_NONE;
	CAM_Int8u *pSrcBuf[3] = { NULL, NULL, NULL };

#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software resize/memcpy on non-cacheable source buffer results in low performance\n" );
	}
#endif

	pSrcBuf[0] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft );
	pSrcBuf[1] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 ) );
	pSrcBuf[2] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 ) );

	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		TRACE( CAM_WARN, "Warning: redundant memory copy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

		// just crop here
		_ppu_copy_image( pSrcImg->pBuffer[0], pSrcImg->iStep[0], pDstImg->pBuffer[0], pDstImg->iStep[0], pROI->iWidth, pROI->iHeight );
		_ppu_copy_image( pSrcImg->pBuffer[1], pSrcImg->iStep[1], pDstImg->pBuffer[1], pDstImg->iStep[1], pROI->iWidth >> 1, pROI->iHeight >> 1 );
		_ppu_copy_image( pSrcImg->pBuffer[2], pSrcImg->iStep[2], pDstImg->pBuffer[2], pDstImg->iStep[2], pROI->iWidth >> 1, pROI->iHeight >> 1 );

		error = CAM_ERROR_NONE;
	}
	else if ( !( pSrcImg->iStep[0] & 7 ) && !( pSrcImg->iStep[1] & 7 ) && !( pSrcImg->iStep[2] & 7 ) &&
	          !( pDstImg->iStep[0] & 3 ) && !( pDstImg->iStep[1] & 1 ) && !( pDstImg->iStep[2] & 1 )
	        )
	{
		IppStatus         eIppRet;
		IppiSize          stSrcSize, stDstSize;
		IppCameraRotation eIppRotate;
		CAM_Int32s        rcpRatioxQ16, rcpRatioyQ16;

		IppCameraInterpolation eInterpolation = ippCameraInterpBilinear;

		switch ( eRotate )
		{
		case CAM_FLIPROTATE_NORMAL:
			eIppRotate       = ippCameraRotateDisable;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight; 
			break;

		case CAM_FLIPROTATE_90L:
			eIppRotate       = ippCameraRotate90L;
			stDstSize.width  = pDstImg->iHeight;
			stDstSize.height = pDstImg->iWidth; 
			break;

		case CAM_FLIPROTATE_90R:
			eIppRotate       = ippCameraRotate90R;
			stDstSize.width  = pDstImg->iHeight;
			stDstSize.height = pDstImg->iWidth;
			break;

		case CAM_FLIPROTATE_180:
			eIppRotate       = ippCameraRotate180;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight; 
			break;

		case CAM_FLIPROTATE_HFLIP:
			eIppRotate       = ippCameraFlipHorizontal; 
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight; 
			break;

		case CAM_FLIPROTATE_VFLIP:
			eIppRotate       = ippCameraFlipVertical; 
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight; 
			break;

		default:
			TRACE( CAM_ERROR, "Error: unsupported rotate[%d]( %s, %s, %d )\n", eRotate, __FILE__, __FUNCTION__, __LINE__ );
			ASSERT( 0 );
			break;
		}

		stSrcSize.width  = pROI->iWidth;
		stSrcSize.height = pROI->iHeight;

		rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
		rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );


		/*
		TRACE( CAM_INFO, "SrcStep = {%d, %d, %d}, srcSize = (%d x %d),\nDstStep = {%d, %d, %d}, dstSize = (%d x %d),\nrotation = %d, \
		       scale = (%d, %d)\n",
		       pSrcImg->iStep[0], pSrcImg->iStep[1], pSrcImg->iStep[2],
		       stSrcSize.width, stSrcSize.height,
		       pDstImg->iStep[0], pDstImg->iStep[1], pDstImg->iStep[2],
		       stDstSize.width, stDstSize.height,
		       eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
		*/

// temporal workaround for nevo release ICS3, will be removed after neon optimized I420 resize rotate is done.
#if defined( PLATFORM_BOARD_NEVO )
		eIppRet = ippiYCbCr420RszRot_8u_P3R_C( (const Ipp8u**)pSrcBuf, pSrcImg->iStep, stSrcSize,
		                                        pDstImg->pBuffer, pDstImg->iStep, stDstSize,
		                                        eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
#else
		eIppRet = ippiYCbCr420RszRot_8u_P3R( (const Ipp8u**)pSrcBuf, pSrcImg->iStep, stSrcSize,
		                                      pDstImg->pBuffer, pDstImg->iStep, stDstSize,
		                                      eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
#endif

		ASSERT( eIppRet == ippStsNoErr );

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: Marvell IPP yuv420planar primitive requires buffer address/step 8 bytes align( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( error == CAM_ERROR_NONE )
	{
		pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1] = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 );
		pDstImg->iFilledLen[2] = pDstImg->iStep[2] * ( pDstImg->iHeight >> 1 );
	}
	else
	{
		pDstImg->iFilledLen[0] = 0;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;
	}

	return error;
}

static CAM_Error _ipp_yuv422p_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error error       = CAM_ERROR_NONE;
	CAM_Int8u *pSrcBuf[3] = { NULL, NULL, NULL };

	pSrcBuf[0] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft );
	pSrcBuf[1] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[1] + pROI->iTop * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 ) );
	pSrcBuf[2] = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[2] + pROI->iTop * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 ) );
#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software resize/memcpy on non-cacheable source buffer results in low performance!\n" );
	}
#endif
	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		TRACE( CAM_WARN, "Warning: redundant memory copy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

		// just crop here
		_ppu_copy_image( pSrcImg->pBuffer[0], pSrcImg->iStep[0], pDstImg->pBuffer[0], pDstImg->iStep[0], pROI->iWidth, pROI->iHeight );
		_ppu_copy_image( pSrcImg->pBuffer[1], pSrcImg->iStep[1], pDstImg->pBuffer[1], pDstImg->iStep[1], pROI->iWidth >> 1, pROI->iHeight );
		_ppu_copy_image( pSrcImg->pBuffer[2], pSrcImg->iStep[2], pDstImg->pBuffer[2], pDstImg->iStep[2], pROI->iWidth >> 1, pROI->iHeight );

		error = CAM_ERROR_NONE;
	}
	else if ( !( pSrcImg->iStep[0] & 7 ) && !( pSrcImg->iStep[1] & 7 ) && !( pSrcImg->iStep[2] & 7 ) &&
	          !( pDstImg->iStep[0] & 7 ) && !( pDstImg->iStep[1] & 7 ) && !( pDstImg->iStep[2] & 7 ) )
	{
		IppStatus              eIppRet;
		IppCameraRotation      eIppRotate;
		IppiSize               stSrcSize, stDstSize;
		CAM_Int32s             rcpRatioxQ16, rcpRatioyQ16;
		IppCameraInterpolation eInterpolation = ippCameraInterpBilinear;

		switch ( eRotate )
		{
		case CAM_FLIPROTATE_NORMAL:
			eIppRotate       = ippCameraRotateDisable;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight;
			break;

		case CAM_FLIPROTATE_90L:
			eIppRotate       = ippCameraRotate90L;
			stDstSize.width  = pDstImg->iHeight;
			stDstSize.height = pDstImg->iWidth;
			break;

		case CAM_FLIPROTATE_90R:
			eIppRotate       = ippCameraRotate90R;
			stDstSize.width  = pDstImg->iHeight;
			stDstSize.height = pDstImg->iWidth;
			break;

		case CAM_FLIPROTATE_180:
			eIppRotate       = ippCameraRotate180;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight;
			break;

		case CAM_FLIPROTATE_HFLIP:
			eIppRotate       = ippCameraFlipHorizontal;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight;
			break;

		case CAM_FLIPROTATE_VFLIP:
			eIppRotate       = ippCameraFlipVertical;
			stDstSize.width  = pDstImg->iWidth;
			stDstSize.height = pDstImg->iHeight;
			break;

		default:
			TRACE( CAM_ERROR, "Error: unsupported rotate[%d]( %s, %s, %d )\n", eRotate, __FILE__, __FUNCTION__, __LINE__ );
			ASSERT( 0 );
			break;
		}

		stSrcSize.width  = pROI->iWidth;
		stSrcSize.height = pROI->iHeight;

		rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
		rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );

// temporal workaround for nevo release ICS3, will be removed after neon optimized I420 resize rotate is done.
#if defined( PLATFORM_BOARD_NEVO )
		eIppRet = ippiYCbCr422RszRot_8u_P3R_C( (const Ipp8u**)pSrcBuf, pSrcImg->iStep, stSrcSize,
		                                        pDstImg->pBuffer, pDstImg->iStep, stDstSize,
		                                        eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
#else
		eIppRet = ippiYCbCr422RszRot_8u_P3R( (const Ipp8u**)pSrcBuf, pSrcImg->iStep, stSrcSize,
		                                      pDstImg->pBuffer, pDstImg->iStep, stDstSize,
		                                      eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
#endif
		ASSERT( eIppRet == ippStsNoErr );

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: Marvell IPP yuv422planar primitive requires address/step 8 bytes align, only 90L/90R are supported and no resize( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	if ( error == CAM_ERROR_NONE )
	{
		pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1] = pDstImg->iStep[1] * pDstImg->iHeight;
		pDstImg->iFilledLen[2] = pDstImg->iStep[1] * pDstImg->iHeight;
	}
	else
	{
		pDstImg->iFilledLen[0] = 0;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;
	}

	return error;
}

static CAM_Error _ipp_uyvy_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error  error          = CAM_ERROR_NONE;
	CAM_Int8u  *pSrcBuf       = NULL;

	pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
	pDstImg->iFilledLen[1] = 0;
	pDstImg->iFilledLen[2] = 0;
#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software resize/memcpy on non-cacheable source buffer will result in low performance!\n" );
	}
#endif
	pSrcBuf = (CAM_Int8u*)( (CAM_Int32s)pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft );

	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		TRACE( CAM_WARN, "Warning: redundant memory copy( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );

		// just crop here
		_ppu_copy_image( pSrcImg->pBuffer[0], pSrcImg->iStep[0], pDstImg->pBuffer[0], pDstImg->iStep[0], pROI->iWidth << 2, pROI->iHeight );

		error = CAM_ERROR_NONE;
	}
	else if ( ( pSrcImg->iStep[0] & 7 ) == 0 && ( pDstImg->iStep[0] & 7 ) == 0 &&
	          ( eRotate == CAM_FLIPROTATE_90L || eRotate == CAM_FLIPROTATE_90R ) &&
	          ( pROI->iWidth == pDstImg->iHeight && pROI->iHeight == pDstImg->iWidth )
	         )
	{
		IppStatus         eIppRet;
		IppCameraRotation eIppRotation = ( eRotate == CAM_FLIPROTATE_90L ) ? ippCameraRotate90L : ippCameraRotate90R;
		IppiSize          stROISize    = { pROI->iWidth, pROI->iHeight };

		eIppRet = ippiYCbCr422Rotate_8u_C2R( pSrcBuf, pSrcImg->iStep[0],
		                                     pDstImg->pBuffer[0], pDstImg->iStep[0],
		                                     stROISize, eIppRotation );
		ASSERT( eIppRet == ippStsNoErr );

		error = CAM_ERROR_NONE;
	}
	else if ( ( pSrcImg->iStep[0] & 7 ) == 0 && ( pDstImg->iStep[0] & 7 ) == 0 &&
	          eRotate == CAM_FLIPROTATE_NORMAL )
	{
		IppStatus              eIppRet;
		IppiSize               stSrcSize, stDstSize;
		CAM_Int32s             rcpRatioxQ16, rcpRatioyQ16;
		IppCameraInterpolation eInterpolation = ippCameraInterpBilinear;
		
		stSrcSize.width  = pROI->iWidth;
		stSrcSize.height = pROI->iHeight;
		
		stDstSize.width  = pDstImg->iWidth;
		stDstSize.height = pDstImg->iHeight; 

		rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
		rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );

		eIppRet = ippiYCbCr422RszRot_8u_C2R_C ( (const Ipp8u*) pSrcBuf, pSrcImg->iStep[0], 
		                                        stSrcSize, pDstImg->pBuffer[0], pDstImg->iStep[0], stDstSize,
		                                        eInterpolation, ippCameraRotateDisable,
		                                        rcpRatioxQ16, rcpRatioyQ16 );

		ASSERT( eIppRet == ippStsNoErr );

		error = CAM_ERROR_NONE;
	}
	else
	{
		TRACE( CAM_ERROR, "Error: Marvell IPP uyvy primitive requires address/step 8 bytes align, only 90L/90R or resize are supported( %s, %s, %d )\n",
		       __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_NOTSUPPORTEDARG;
	}

	return error;
}

static CAM_Error _ipp_ycc420p2sp_rszrot_roi_swap( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate, CAM_Bool bIsInPlace )
{
	CAM_Error  error   = CAM_ERROR_NONE;
	IppStatus  eIppRet;
	CAM_Int8u  *pSrcBuf[3], *pDstBuf[3];
	CAM_Int32s pSrcStep[3], pDstStep[3];
	IppiSize   stROISize;
	CAM_Int8u  *pSrcHeap = NULL, *pDstHeap = NULL;
#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software resize/memcpy on non-cacheable source buffer results in low performance!\n" );
	}
#endif
	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		pSrcBuf[0]  = pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft;
		pSrcStep[0] = pSrcImg->iStep[0];

		if ( ( pSrcImg->eFormat == CAM_IMGFMT_YCbCr420P && pDstImg->eFormat == CAM_IMGFMT_YCbCr420SP ) ||
		     ( pSrcImg->eFormat == CAM_IMGFMT_YCrCb420P && pDstImg->eFormat == CAM_IMGFMT_YCrCb420SP ) )
		{
			pSrcBuf[1]  = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );
			pSrcBuf[2]  = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );

			pSrcStep[1] = pSrcImg->iStep[1];
			pSrcStep[2] = pSrcImg->iStep[2];
		}
		else
		{
			// swap u/v plane
			pSrcBuf[1]  = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );
			pSrcBuf[2]  = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );

			pSrcStep[1] = pSrcImg->iStep[2];
			pSrcStep[2] = pSrcImg->iStep[1];
		}

		if ( bIsInPlace == CAM_FALSE )
		{
			pDstBuf[0]  = pDstImg->pBuffer[0];
			pDstBuf[1]  = pDstImg->pBuffer[1];
			pDstStep[0] = pDstImg->iStep[0];
			pDstStep[1] = pDstImg->iStep[1];
		}
		else
		{
			pDstHeap = ( CAM_Int8u* )malloc( pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 ) + 8 );
			if ( pDstHeap == NULL )
			{
				TRACE( CAM_ERROR, "Error: no enough memory to do 420P->420SP conversion( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_OUTOFMEMORY;
			}
			pDstBuf[0]  = pDstImg->pBuffer[0];
			pDstBuf[1]  = (CAM_Int8u*)_ALIGN_TO( pDstHeap, 8 );
			pDstStep[0] = pDstImg->iStep[0];
			pDstStep[1] = pDstImg->iStep[1];
		}
	}
	else
	{
		IppiSize               stSrcSize, stDstSize;
		IppCameraRotation      eIppRotate;
		CAM_Int32s             rcpRatioxQ16, rcpRatioyQ16;
		CAM_Int8u              *pBufBeforeZoom[3], *pBufAfterZoom[3];
		CAM_Int32s             pStepAfterZoom[3];
		IppCameraInterpolation eInterpolation = ippCameraInterpBilinear;

		if ( !( pSrcImg->iStep[0] & 7 ) && !( pSrcImg->iStep[1] & 7 ) && !( pSrcImg->iStep[2] & 7 ) )
		{
			pBufBeforeZoom[0] = pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft;
			pBufBeforeZoom[1] = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );
			pBufBeforeZoom[2] = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );

			pSrcHeap = ( CAM_Int8u* )malloc( pDstImg->iWidth * pDstImg->iHeight * 3 / 2 + 24 );
			if ( pSrcHeap == NULL )
			{
				TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_OUTOFMEMORY;
			}

			pStepAfterZoom[0] = _ALIGN_TO( pDstImg->iWidth, 8 );
			pStepAfterZoom[1] = _ALIGN_TO( pDstImg->iWidth >> 1, 8 );
			pStepAfterZoom[2] = _ALIGN_TO( pDstImg->iWidth >> 1, 8 );

			pBufAfterZoom[0] = (CAM_Int8u*)_ALIGN_TO( pSrcHeap, 8 );
			pBufAfterZoom[1] = pBufAfterZoom[0] + pStepAfterZoom[0] * pDstImg->iHeight;
			pBufAfterZoom[2] = pBufAfterZoom[1] + pStepAfterZoom[1] * ( pDstImg->iHeight >> 1 );

			switch ( eRotate )
			{
			case CAM_FLIPROTATE_NORMAL:
				eIppRotate       = ippCameraRotateDisable;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_90L:
				eIppRotate       = ippCameraRotate90L;
				stDstSize.width  = pDstImg->iHeight;
				stDstSize.height = pDstImg->iWidth;
				break;

			case CAM_FLIPROTATE_90R:
				eIppRotate       = ippCameraRotate90R;
				stDstSize.width  = pDstImg->iHeight;
				stDstSize.height = pDstImg->iWidth;
				break;

			case CAM_FLIPROTATE_180:
				eIppRotate       = ippCameraRotate180;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_HFLIP:
				eIppRotate       = ippCameraFlipHorizontal;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_VFLIP:
				eIppRotate       = ippCameraFlipVertical;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			default:
				TRACE( CAM_ERROR, "Error: unsupported rotate[%d]( %s, %s, %d )\n", eRotate, __FILE__, __FUNCTION__, __LINE__ );
				ASSERT( 0 );
				break;
			}

			stSrcSize.width  = pROI->iWidth;
			stSrcSize.height = pROI->iHeight;

			rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
			rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );

#if 0
			TRACE( CAM_INFO, "SrcStep = {%d, %d, %d}, srcSize = (%d x %d),\n DstStep = {%d, %d, %d}, dstSize = (%d x %d),\nrotation = %d, scale = (%d, %d)\n",
			       pSrcImg->iStep[0], pSrcImg->iStep[1], pSrcImg->iStep[2],
			       stSrcSize.width, stSrcSize.height,
			       pDstImg->iStep[0], pDstImg->iStep[1], pDstImg->iStep[2],
			       stDstSize.width, stDstSize.height,
			       eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
#endif

			eIppRet = ippiYCbCr420RszRot_8u_P3R( (const Ipp8u**)pBufBeforeZoom, pSrcImg->iStep, stSrcSize,
			                                      pBufAfterZoom, pStepAfterZoom, stDstSize,
			                                      eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );

			ASSERT( eIppRet == ippStsNoErr );

			pSrcBuf[0] = pBufAfterZoom[0];
			pSrcStep[0] = pStepAfterZoom[0];

			if ( ( pSrcImg->eFormat == CAM_IMGFMT_YCbCr420P && pDstImg->eFormat == CAM_IMGFMT_YCbCr420SP ) ||
			     ( pSrcImg->eFormat == CAM_IMGFMT_YCrCb420P && pDstImg->eFormat == CAM_IMGFMT_YCrCb420SP ) )
			{
				pSrcBuf[1]  = pBufAfterZoom[1];
				pSrcBuf[2]  = pBufAfterZoom[2];
				pSrcStep[1] = pStepAfterZoom[1];
				pSrcStep[2] = pStepAfterZoom[2];
			}
			else
			{
				// swap u/v plane
				pSrcBuf[1]  = pBufAfterZoom[2];
				pSrcBuf[2]  = pBufAfterZoom[1];
				pSrcStep[1] = pStepAfterZoom[2];
				pSrcStep[2] = pStepAfterZoom[1];
			}

			pDstBuf[0]  = pDstImg->pBuffer[0];
			pDstBuf[1]  = pDstImg->pBuffer[1];
			pDstStep[0] = pDstImg->iStep[0];
			pDstStep[1] = pDstImg->iStep[1];
		}
		else
		{
			TRACE( CAM_ERROR, "Error: Marvell IPP yuv420 planar primitive requires address/step 8 bytes align( %s, %s, %d )\n",
			       __FILE__, __FUNCTION__, __LINE__ );
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
	}

	stROISize.height = pDstImg->iHeight;
	stROISize.width  = pDstImg->iWidth;

	eIppRet = ippiYCbCr420PToYCbCr420SP_8u_P3P2R( (const Ipp8u**)pSrcBuf, pSrcStep, pDstBuf, pDstStep, stROISize );
	if ( pSrcHeap != NULL )
	{
		free( pSrcHeap );
		pSrcHeap = NULL;
	}
	if ( pDstHeap != NULL )
	{
		memmove( pDstImg->pBuffer[1], pDstBuf[1], pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 ) );
		free( pDstHeap );
		pDstHeap = NULL;
	}
	ASSERT( eIppRet == ippStsNoErr );

	pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
	pDstImg->iFilledLen[1] = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 );
	pDstImg->iFilledLen[2] = 0; 

	return CAM_ERROR_NONE;
}

static CAM_Error _ipp_yuyv2yuv420sp_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error  error   = CAM_ERROR_NONE;
	IppStatus  eIppRet = ippStsNoErr;
	CAM_Int8u  *pBufAfterZoom[3];
	CAM_Int32s pStepAfterZoom[3];
	IppiSize   stROISize;

	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		pBufAfterZoom[0] = pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft ;
		pBufAfterZoom[1] = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );
		pBufAfterZoom[2] = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );

		pStepAfterZoom[0] = pSrcImg->iStep[0];
		pStepAfterZoom[1] = pSrcImg->iStep[1];
		pStepAfterZoom[2] = pSrcImg->iStep[2];
	}
	else
    {
        TRACE( CAM_ERROR, "Error: unsupported resize\n");
    }

	stROISize.height = pDstImg->iHeight;
	stROISize.width  = pDstImg->iWidth;

    eIppRet = ippiYCbYCrToYUV420SP( (Ipp8u**)pBufAfterZoom, pStepAfterZoom, stROISize, pDstImg->pBuffer, pDstImg->iStep, stROISize );

	ASSERT( eIppRet == ippStsNoErr );

	pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
	pDstImg->iFilledLen[1] = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 );
	pDstImg->iFilledLen[2] = 0; 

	return CAM_ERROR_NONE;
}

static CAM_Error _ipp_yuv420p2uyvy_rszrot_roi( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI, CAM_FlipRotate eRotate )
{
	CAM_Error  error   = CAM_ERROR_NONE;
	IppStatus  eIppRet = ippStsNoErr;
	CAM_Int8u  *pBufAfterZoom[3];
	CAM_Int32s pStepAfterZoom[3];
	IppiSize   stROISize;
	CAM_Int8u  *pHeap = NULL;
#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software resize/memcpy on non-cacheable source buffer results in low performance!\n" );
	}
#endif
	if ( pROI->iWidth == pDstImg->iWidth && pROI->iHeight == pDstImg->iHeight && eRotate == CAM_FLIPROTATE_NORMAL )
	{
		pBufAfterZoom[0] = pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft ;
		pBufAfterZoom[1] = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );
		pBufAfterZoom[2] = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );

		pStepAfterZoom[0] = pSrcImg->iStep[0];
		pStepAfterZoom[1] = pSrcImg->iStep[1];
		pStepAfterZoom[2] = pSrcImg->iStep[2];
	}
	else
	{
		IppiSize          stSrcSize, stDstSize;
		IppCameraRotation eIppRotate;
		CAM_Int32s        rcpRatioxQ16, rcpRatioyQ16;
		CAM_Int8u         *pBufBeforeZoom[3];
		
		IppCameraInterpolation eInterpolation = ippCameraInterpBilinear;

		// NOTE: IPP primitives require step 8 byte align, if not, the result will be UNEXPECTED!!!
		if ( !( pSrcImg->iStep[0] & 7 ) && !( pSrcImg->iStep[1] & 7 ) && !( pSrcImg->iStep[2] & 7 ) )
		{
			pBufBeforeZoom[0] = pSrcImg->pBuffer[0] + pROI->iTop * pSrcImg->iStep[0] + pROI->iLeft;
			pBufBeforeZoom[1] = pSrcImg->pBuffer[1] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[1] + ( pROI->iLeft >> 1 );
			pBufBeforeZoom[2] = pSrcImg->pBuffer[2] + ( pROI->iTop >> 1 ) * pSrcImg->iStep[2] + ( pROI->iLeft >> 1 );

			pHeap = ( CAM_Int8u* )malloc( pDstImg->iWidth * pDstImg->iHeight * 3 / 2 + 8 );
			if ( pHeap == NULL )
			{
				TRACE( CAM_ERROR, "Error: no enough memory afford ipp image post processing( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
				return CAM_ERROR_OUTOFMEMORY;
			}

			pStepAfterZoom[0] = _ALIGN_TO( pDstImg->iWidth, 8 );
			pStepAfterZoom[1] = _ALIGN_TO( pDstImg->iWidth >> 1, 8 );
			pStepAfterZoom[2] = _ALIGN_TO( pDstImg->iWidth >> 1, 8 );

			pBufAfterZoom[0] = (CAM_Int8u*)_ALIGN_TO( pHeap, 8 );
			pBufAfterZoom[1] = pBufAfterZoom[0] + pStepAfterZoom[0] * pDstImg->iHeight;
			pBufAfterZoom[2] = pBufAfterZoom[1] + pStepAfterZoom[1] * ( pDstImg->iHeight >> 1 );

			switch ( eRotate )
			{
			case CAM_FLIPROTATE_NORMAL:
				eIppRotate       = ippCameraRotateDisable;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_90L:
				eIppRotate       = ippCameraRotate90L;
				stDstSize.width  = pDstImg->iHeight;
				stDstSize.height = pDstImg->iWidth;
				break;

			case CAM_FLIPROTATE_90R:
				eIppRotate       = ippCameraRotate90R;
				stDstSize.width  = pDstImg->iHeight;
				stDstSize.height = pDstImg->iWidth;
				break;

			case CAM_FLIPROTATE_180:
				eIppRotate       = ippCameraRotate180;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_HFLIP:
				eIppRotate       = ippCameraFlipHorizontal;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			case CAM_FLIPROTATE_VFLIP:
				eIppRotate       = ippCameraFlipVertical;
				stDstSize.width  = pDstImg->iWidth;
				stDstSize.height = pDstImg->iHeight;
				break;

			default:
				TRACE( CAM_ERROR, "Error: unsupported rotate[%d]( %s, %s, %d )\n", eRotate, __FILE__, __FUNCTION__, __LINE__ );
				ASSERT( 0 );
				break;
			}

			stSrcSize.width  = pROI->iWidth;
			stSrcSize.height = pROI->iHeight;

			rcpRatioxQ16 = ( ( ( stSrcSize.width & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.width & ~1 ) - 1 );
			rcpRatioyQ16 = ( ( ( stSrcSize.height & ~1 ) - 1 ) << 16 ) / ( ( stDstSize.height & ~1 ) - 1 );


			/*
			TRACE( CAM_INFO, "SrcStep = {%d, %d, %d}, srcSize = (%d x %d),\nDstStep = {%d, %d, %d}, dstSize = (%d x %d),\nrotation = %d, scale = (%d, %d)\n",
			       pSrcImg->iStep[0], pSrcImg->iStep[1], pSrcImg->iStep[2],
			       stSrcSize.width, stSrcSize.height,
			       pDstImg->iStep[0], pDstImg->iStep[1], pDstImg->iStep[2],
			       stDstSize.width, stDstSize.height,
			       eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );
			*/

			eIppRet = ippiYCbCr420RszRot_8u_P3R( (const Ipp8u**)pBufBeforeZoom, pSrcImg->iStep, stSrcSize,
			                                      pBufAfterZoom, pStepAfterZoom, stDstSize,
			                                      eInterpolation, eIppRotate, rcpRatioxQ16, rcpRatioyQ16 );

			ASSERT( eIppRet == ippStsNoErr );
		}
		else
		{
			TRACE( CAM_ERROR, "Error: Marvell IPP yuv422 planar primitive requires address/step 8 bytes align( %s, %s, %d )\n",
			       __FILE__, __FUNCTION__, __LINE__  );
			return CAM_ERROR_NOTSUPPORTEDARG;
		}
	}

	stROISize.height = pDstImg->iHeight;
	stROISize.width  = pDstImg->iWidth;

	eIppRet = ippiYCbCr420ToCbYCrY_8u_P3C2R_C( (Ipp8u**)pBufAfterZoom, pStepAfterZoom, stROISize, pDstImg->pBuffer, pDstImg->iStep, stROISize );

	if ( pHeap != NULL )
	{
		free( pHeap );
		pHeap = NULL;
	}
	ASSERT( eIppRet == ippStsNoErr );

	pDstImg->iFilledLen[0] = pDstImg->iStep[0] * pDstImg->iHeight;
	pDstImg->iFilledLen[1] = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1 );
	pDstImg->iFilledLen[2] = 0; 

	return CAM_ERROR_NONE;
}


static CAM_Error _ipp_jpeg_rotation( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_FlipRotate eRotate )
{
#if 0
	void                     *pDecoderState  = NULL;
	MiscGeneralCallbackTable *pCallBackTable = NULL;
	IppJPEGDecoderParam      stDecoderPar;
	IppBitstream             stSrcBitStream;
	IppBitstream             stDstBitStream;
	IppPicture               stDstPicture;
	IppCodecStatus           eRetCode;
	CAM_Int8u                cImgEnd[2]      = { 0 };

	memset( &stDecoderPar, 0, sizeof( IppJPEGDecoderParam ) );
	memset( &stDstPicture, 0, sizeof( IppPicture ) );

	pDstImg->iFilledLen[0] = 0;
	pDstImg->iFilledLen[1] = 0;
	pDstImg->iFilledLen[2] = 0;

	stDecoderPar.nModeFlag = IPP_JPEG_ROTATE;

	switch ( eRotate )
	{
	case CAM_FLIPROTATE_90L:
		stDecoderPar.UnionParamJPEG.rotParam.RotMode = IPP_JP_90L;
		break;

	case CAM_FLIPROTATE_90R:
		stDecoderPar.UnionParamJPEG.rotParam.RotMode = IPP_JP_90R;
		break;

	case CAM_FLIPROTATE_180:
		stDecoderPar.UnionParamJPEG.rotParam.RotMode = IPP_JP_180;
		break;

	default:
		TRACE( CAM_ERROR, "Error: do not support JPEG rotation other than 90/180/270( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		ASSERT( 0 );
		break;
	}

	stDecoderPar.UnionParamJPEG.rotParam.pStreamHandler = NULL;

	// init callback table
	if ( miscInitGeneralCallbackTable( &pCallBackTable ) != 0 )
	{
		TRACE( CAM_ERROR, "Error: init JPEG rotation failed( %s, %s, %d )!\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PPUFAILED; 
	}

	stSrcBitStream.pBsBuffer      = pSrcImg->pBuffer[0];
	stSrcBitStream.bsByteLen      = pSrcImg->iFilledLen[0];
	stSrcBitStream.pBsCurByte     = pSrcImg->pBuffer[0];
	stSrcBitStream.bsCurBitOffset = 0;

	/* XXX: why I do this? Oh, it's for robustness consideration, if a JPEG file
	 *      is damaged, maybe we need not frustrated, some decoders also can decode it
	 *      although imcomplete, but "half a bread is better than none", you know.IPP JPEG
	 *      decoder can decode a damaged JPEG at its best, but the presumption is that
	 *      you should give it a 0xFFD9 to remind it, the file is finished.
	 */
	cImgEnd[0] = pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2];
	cImgEnd[1] = pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1];
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1] = 0xD9;
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2] = 0xFF;

	eRetCode = DecoderInitAlloc_JPEG( &stSrcBitStream, &stDstPicture, pCallBackTable, &pDecoderState );

	if ( IPP_STATUS_NOERR != eRetCode )
	{
#if defined( ANDROID )
		char sFileName[256] = "/data/badjpeg_ppu1.dat";
#else
		char sFileName[256] = "badjpeg_ppu1.dat";
#endif
		PPU_DUMP_FRAME( sFileName, pSrcImg );

		TRACE( CAM_ERROR, "Error: init IPP JPEG decoder failed, the input JPEG data have been dumped to[%s]( %s, %s, %d )!\n", sFileName, __FILE__, __FUNCTION__, __LINE__ );
		miscFreeGeneralCallbackTable( &pCallBackTable );

		return CAM_ERROR_PPUFAILED;
	}

	stDstBitStream.pBsBuffer      = stDstBitStream.pBsCurByte = pDstImg->pBuffer[0];
	stDstBitStream.bsByteLen      = pDstImg->iAllocLen[0];
	stDstBitStream.bsCurBitOffset = 0;

	// call the core JPEG decoder function
	eRetCode = Decode_JPEG( ( void* )&stSrcBitStream, &stDstBitStream, NULL, &stDecoderPar, pDecoderState );
	if ( IPP_STATUS_JPEG_EOF != eRetCode && IPP_STATUS_INPUT_ERR != eRetCode )
	{
		TRACE( CAM_ERROR, "Error: IPP JPEG rotation failed, error code[%d]( %s, %s, %d )!\n", eRetCode, __FILE__, __FUNCTION__, __LINE__ );

		DecoderFree_JPEG( &pDecoderState );
		miscFreeGeneralCallbackTable( &pCallBackTable );
		pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1] = cImgEnd[1];
		pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2] = cImgEnd[0];

		return CAM_ERROR_PPUFAILED;
	}
	if ( IPP_STATUS_INPUT_ERR == eRetCode )
	{
		TRACE( CAM_WARN, "Warning: Input JPEG file is damaged, Camera Enigne/IPP lib will do our best to handle it, \
		       but also pls check the data source( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	}

	DecoderFree_JPEG( &pDecoderState );
	miscFreeGeneralCallbackTable( &pCallBackTable );
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1] = cImgEnd[1];
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2] = cImgEnd[0];

	pDstImg->iFilledLen[0] = ( int )( stDstBitStream.pBsCurByte - stDstBitStream.pBsBuffer );
	pDstImg->iFilledLen[1] = 0;
	pDstImg->iFilledLen[2] = 0;

	return CAM_ERROR_NONE;
#else
	TRACE( CAM_ERROR, "cannot support JPEG rotate in this platform( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	return CAM_ERROR_NOTSUPPORTEDARG;
#endif
}

static CAM_Error _ipp_jpeg_roidec( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_Rect *pROI )
{
	CAM_Error                error           = CAM_ERROR_NONE;
	MiscGeneralCallbackTable *pCallBackTable = NULL;
	void                     *pDecoderState  = NULL;
	IppJPEGDecoderParam      stDecoderPar;
	IppPicture               stDstPicture;
	IppBitstream             stSrcBitStream;
	IppCodecStatus           eRetCode;
	CAM_Int8u                cImgEnd[2]      = { 0 };
	CAM_Bool                 bNeedSwapChroma = CAM_FALSE;

	memset( &stDecoderPar, 0, sizeof( IppJPEGDecoderParam ) );
	memset( &stDstPicture, 0, sizeof( IppPicture ) );

	ASSERT( pSrcImg->pBuffer[0] != NULL );
#if 0
	if ( pSrcImg->iFlag & BUF_FLAG_L1NONCACHEABLE )
	{
		TRACE( CAM_WARN, "Warning: perform software jpeg snapshot generating on non-cacheable source buffer results in low performance!\n" );
	}
#endif
	// init callback table
	if ( miscInitGeneralCallbackTable( &pCallBackTable ) != 0 )
	{
		TRACE( CAM_ERROR, "Error: init IPP JPEG decoder failed( %s, %s, %d )!\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PPUFAILED; 
	}

	// no need to read image from file
	pCallBackTable->fFileRead     = NULL;
	stSrcBitStream.pBsCurByte     = stSrcBitStream.pBsBuffer = pSrcImg->pBuffer[0];
	stSrcBitStream.bsByteLen      = pSrcImg->iFilledLen[0];
	stSrcBitStream.bsCurBitOffset = 0;

	/* XXX: why I do this? Oh, it's for robustness consideration, if a JPEG file
	 *      is damaged, maybe we need not frustrated, some decoders also can decode it
	 *      although imcomplete, but "half a bread is better than none", you know.IPP JPEG
	 *      decoder can decode a damaged JPEG at its best, but the presumption is that
	 *      you should give it a 0xFFD9 to remind it, the file is finished.
	 */
	cImgEnd[0] = pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2];
	cImgEnd[1] = pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1];
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1] = 0xD9;
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2] = 0xFF;

#if 0
	CELOG( "stSrcBitStream.pBsCurByte: %p\n stSrcBitStream.pBsBuffer: %p\n\
	        stSrcBitStream.bsByteLen: %d\n, stSrcBitStream.bsCurBitOffset :%d\n",
	        stSrcBitStream.pBsCurByte, stSrcBitStream.pBsBuffer, stSrcBitStream.bsByteLen,
	        stSrcBitStream.bsCurBitOffset );
#endif

	eRetCode = DecoderInitAlloc_JPEG( &stSrcBitStream, &stDstPicture, pCallBackTable, &pDecoderState, (void*)NULL );
	if ( IPP_STATUS_NOERR != eRetCode )
	{
#if defined( ANDROID )
		char sFileName[256] = "/data/badjpeg_ppu2.dat";
#else
		char sFileName[256] = "badjpeg_ppu2.dat";
#endif
		PPU_DUMP_FRAME( sFileName, pSrcImg );

		TRACE( CAM_ERROR, "Error: init JPEG decoder failed, the input JPEG data have been dumped to[%s]( %s, %s, %d )!\n", sFileName, __FILE__, __FUNCTION__, __LINE__ );
		miscFreeGeneralCallbackTable( &pCallBackTable );
		return CAM_ERROR_PPUFAILED;
	}

	switch ( pDstImg->eFormat )
	{
	case CAM_IMGFMT_YCbCr420P:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1);
		pDstImg->iFilledLen[2]     = pDstImg->iStep[2] * ( pDstImg->iHeight >> 1);
		stDstPicture.picPlaneNum   = 3;
		stDecoderPar.nDesiredColor = JPEG_YUV411;
		bNeedSwapChroma            = CAM_FALSE;
		break;

	case CAM_IMGFMT_YCrCb420P:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = pDstImg->iStep[1] * ( pDstImg->iHeight >> 1);
		pDstImg->iFilledLen[2]     = pDstImg->iStep[2] * ( pDstImg->iHeight >> 1);
		stDstPicture.picPlaneNum   = 3;
		stDecoderPar.nDesiredColor = JPEG_YUV411;
		bNeedSwapChroma            = CAM_TRUE;
		break;

	case CAM_IMGFMT_YCbCr422P:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = pDstImg->iStep[1] * pDstImg->iHeight;
		pDstImg->iFilledLen[2]     = pDstImg->iStep[2] * pDstImg->iHeight;
		stDstPicture.picPlaneNum   = 3;
		stDecoderPar.nDesiredColor = JPEG_YUV422;
		bNeedSwapChroma            = CAM_FALSE;
		break;

	case CAM_IMGFMT_YCbCr444P:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = pDstImg->iStep[1] * pDstImg->iHeight;
		pDstImg->iFilledLen[2]     = pDstImg->iStep[2] * pDstImg->iHeight;
		stDstPicture.picPlaneNum   = 3;
		stDecoderPar.nDesiredColor = JPEG_YUV444;
		bNeedSwapChroma            = CAM_FALSE;
		break;

	case CAM_IMGFMT_RGB565:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = 0;
		pDstImg->iFilledLen[2]     = 0;
		stDstPicture.picPlaneNum   = 1;
		stDecoderPar.nDesiredColor = JPEG_RGB565;
		bNeedSwapChroma            = CAM_FALSE;
		break;

	case CAM_IMGFMT_BGR565:
		pDstImg->iFilledLen[0]     = pDstImg->iStep[0] * pDstImg->iHeight;
		pDstImg->iFilledLen[1]     = 0;
		pDstImg->iFilledLen[2]     = 0;
		stDstPicture.picPlaneNum   = 1;
		stDecoderPar.nDesiredColor = JPEG_BGR565;
		bNeedSwapChroma            = CAM_FALSE;
		break;

	default:
		TRACE( CAM_ERROR, "Error: the given format[%d] is not supported by JPEG ROI decoder!( %s, %s, %d )\n",
		       pDstImg->eFormat, __FILE__, __FUNCTION__, __LINE__ );
		ASSERT( 0 );
		break;
	}

	stDstPicture.picWidth  =  pDstImg->iWidth;
	stDstPicture.picHeight =  pDstImg->iHeight;

	stDstPicture.picPlaneStep[0] = pDstImg->iStep[0];
	stDstPicture.ppPicPlane[0]   = pDstImg->pBuffer[0];

	if ( bNeedSwapChroma )
	{
		stDstPicture.picPlaneStep[1] = pDstImg->iStep[2];
		stDstPicture.picPlaneStep[2] = pDstImg->iStep[1];
		stDstPicture.ppPicPlane[1]   = pDstImg->pBuffer[2];
		stDstPicture.ppPicPlane[2]   = pDstImg->pBuffer[1];
	}
	else
	{
		stDstPicture.picPlaneStep[1] = pDstImg->iStep[1];
		stDstPicture.picPlaneStep[2] = pDstImg->iStep[2];
		stDstPicture.ppPicPlane[1]   = pDstImg->pBuffer[1];
		stDstPicture.ppPicPlane[2]   = pDstImg->pBuffer[2];
        }


	stDecoderPar.srcROI.x        = pROI->iLeft;
	stDecoderPar.srcROI.y        = pROI->iTop;
	stDecoderPar.srcROI.width    = pROI->iWidth;
	stDecoderPar.srcROI.height   = pROI->iHeight;

	stDecoderPar.nScale = 0; // decoder will self calc the Scale factor

#if 0
	CELOG( "Info:\
	        stDstPicture.picWidth=%d,\n\
	        stDstPicture.picHeight=%d,\n\
	        stDstPicture.picPlaneStep[0]=%d,\n\
	        stDstPicture.picPlaneStep[1]=%d,\n\
	        stDstPicture.picPlaneStep[2]=%d,\n\
	        stDstPicture.ppPicPlane[0]=%p,\n\
	        stDstPicture.ppPicPlane[1]=%p,\n\
	        stDstPicture.ppPicPlane[2]=%p,\n\
	        stDecoderPar.nDesiredColor=%d,\n\
	        stDecoderPar.srcROI.width=%d,\n\
	        stDecoderPar.srcROI.height=%d,\n\
	        stDecoderPar.srcROI.x=%d,\n\
	        stDecoderPar.srcROI.y=%d,\n\
	        stDecoderPar.nScale=%d\n",
	        stDstPicture.picWidth,
	        stDstPicture.picHeight,
	        stDstPicture.picPlaneStep[0],
	        stDstPicture.picPlaneStep[1],
	        stDstPicture.picPlaneStep[2],
	        stDstPicture.ppPicPlane[0],
	        stDstPicture.ppPicPlane[1],
	        stDstPicture.ppPicPlane[2],
	        stDecoderPar.nDesiredColor,
	        stDecoderPar.srcROI.width,
	        stDecoderPar.srcROI.height,
	        stDecoderPar.srcROI.x,
	        stDecoderPar.srcROI.y,
	        stDecoderPar.nScale );
#endif

	// call the core JPEG decoder function
	eRetCode = Decode_JPEG( &stDstPicture, &stDecoderPar, pDecoderState );
	if ( IPP_STATUS_NOERR != eRetCode )
	{
		TRACE( CAM_ERROR, "Error: JPEG_ROIDec Failed, error code[%d]( %s, %s, %d )!\n", eRetCode, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_PPUFAILED;
	}
	else
	{
		error = CAM_ERROR_NONE;
	}

	DecoderFree_JPEG( &pDecoderState );
	miscFreeGeneralCallbackTable( &pCallBackTable );
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 1] = cImgEnd[1];
	pSrcImg->pBuffer[0][stSrcBitStream.bsByteLen - 2] = cImgEnd[0];

	if ( error != CAM_ERROR_NONE )
	{
		pDstImg->iFilledLen[0] = 0;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;
	}

	return error;
}

static CAM_Error _ipp_yuv2jpeg( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_JpegParam *pJpegParam )
{
	IppPicture                  stSrcPic;
	IppBitstream                stDstStream;
	IppJPEGEncoderParam         stEncoderPar;
	IppCodecStatus              eRetCode;
	void                        *pEncoderState  = NULL;
	MiscGeneralCallbackTable    *pCallBackTable = NULL;

	CAM_Error                   error           = CAM_ERROR_NONE;

	if ( pDstImg->iWidth != pSrcImg->iWidth || pDstImg->iHeight != pSrcImg->iHeight )
	{
		TRACE( CAM_ERROR, "Error: JPEG encoder don't support input size and output size differ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_NOTSUPPORTEDARG;
	}

	stSrcPic.picWidth            = pSrcImg->iWidth;
	stSrcPic.picHeight           = pSrcImg->iHeight;
	stSrcPic.picFormat           = ( ( pSrcImg->eFormat == CAM_IMGFMT_YCbCr420P || pSrcImg->eFormat == CAM_IMGFMT_YCrCb420P ) ? JPEG_YUV411 : JPEG_YUV422 );
	stSrcPic.picChannelNum       = 3;
	stSrcPic.picPlaneNum         = 3;

	stSrcPic.picPlaneStep[0]     = pSrcImg->iStep[0];
	stSrcPic.ppPicPlane[0]       = pSrcImg->pBuffer[0];

	if ( pSrcImg->eFormat == CAM_IMGFMT_YCrCb420P )
	{
		stSrcPic.ppPicPlane[1]   = pSrcImg->pBuffer[2];
		stSrcPic.ppPicPlane[2]   = pSrcImg->pBuffer[1];
		stSrcPic.picPlaneStep[1] = pSrcImg->iStep[2];
		stSrcPic.picPlaneStep[2] = pSrcImg->iStep[1];
	}
	else
	{
		stSrcPic.ppPicPlane[1]   = pSrcImg->pBuffer[1];
		stSrcPic.ppPicPlane[2]   = pSrcImg->pBuffer[2];
		stSrcPic.picPlaneStep[1] = pSrcImg->iStep[1];
		stSrcPic.picPlaneStep[2] = pSrcImg->iStep[2];
	}

	stSrcPic.picROI.x            = 0;
	stSrcPic.picROI.y            = 0;
	stSrcPic.picROI.width        = pSrcImg->iWidth;
	stSrcPic.picROI.height       = pSrcImg->iHeight;

	stDstStream.pBsBuffer        = ( Ipp8u* )pDstImg->pBuffer[0];
	stDstStream.bsByteLen        = pDstImg->iAllocLen[0];
	stDstStream.pBsCurByte       = ( Ipp8u* )pDstImg->pBuffer[0];
	stDstStream.bsCurBitOffset   = 0;

	stEncoderPar.nQuality          = 90; // pJpegParam->iQualityFactor;
	stEncoderPar.nRestartInterval  = 0;
	stEncoderPar.nJPEGMode         = JPEG_BASELINE;
	stEncoderPar.nSubsampling      = ( stSrcPic.picFormat == JPEG_YUV411 ? JPEG_411 : JPEG_422 );
	stEncoderPar.nBufType          = JPEG_INTEGRATEBUF;
	stEncoderPar.pSrcPicHandler    = (void*)NULL;
	stEncoderPar.pStreamHandler    = (void*)NULL;

#if 0
	TRACE( CAM_INFO, "stEncoderPar.nBufType = %d\n stEncoderPar.nJPEGMode = %d\n stEncoderPar.nQuality = %d\n \
	       stEncoderPar.nRestartInterval = %d\n stEncoderPar.nSubsampling = %d\n \
	       stEncoderPar.pSrcPicHandler = %p\n stEncoderPar.pStreamHandler = %p\n \
	       stSrcPic.picWidth = %d\n stSrcPic.picHeight = %d\n stSrcPic.picFormat = %d\n \
	       stSrcPic.picChannelNum = %d\n, stSrcPic.picPlaneNum = %d\n stSrcPic.picPlaneStep[0] = %d\n \
	       stSrcPic.picPlaneStep[1] = %d\n stSrcPic.picPlaneStep[2] = %d\n stSrcPic.ppPicPlane[0] = %p\n \
	       stSrcPic.ppPicPlane[1] = %p\n stSrcPic.ppPicPlane[2] = %p\n stSrcPic.picROI.x = %d\n \
	       stSrcPic.picROI.y = %d\n stSrcPic.picROI.width = %d\n stSrcPic.picROI.height = %d\n \
	       stSrcPic.picStatus = %d\n stSrcPic.picTimeStamp = %d\n stSrcPic.picOrderCnt = %d\n ",\
	       stEncoderPar.nBufType, stEncoderPar.nJPEGMode, stEncoderPar.nQuality, \
	       stEncoderPar.nRestartInterval, stEncoderPar.nSubsampling, stEncoderPar.pSrcPicHandler, \
	       stEncoderPar.pStreamHandler, stSrcPic.picWidth, stSrcPic.picHeight, stSrcPic.picFormat, \
	       stSrcPic.picChannelNum, stSrcPic.picPlaneNum, stSrcPic.picPlaneStep[0], stSrcPic.picPlaneStep[1], \
	       stSrcPic.picPlaneStep[2], stSrcPic.ppPicPlane[0], stSrcPic.ppPicPlane[1], stSrcPic.ppPicPlane[2], \
	       stSrcPic.picROI.x, stSrcPic.picROI.y, stSrcPic.picROI.width, stSrcPic.picROI.height, \
	       stSrcPic.picStatus, (CAM_Int32s)stSrcPic.picTimeStamp, stSrcPic.picOrderCnt );
#endif

	if ( miscInitGeneralCallbackTable( &pCallBackTable ) != 0 )
	{
		return CAM_ERROR_OUTOFMEMORY;
	}

	// NOTE: set fStreamRealloc as NULL to prevent reallocate buffer in encoder
	pCallBackTable->fStreamRealloc = NULL;

	eRetCode = EncoderInitAlloc_JPEG( &stEncoderPar, &stSrcPic, &stDstStream, pCallBackTable, &pEncoderState );
	if ( IPP_STATUS_NOERR != eRetCode )
	{
		TRACE( CAM_ERROR, "Error: IPP JPEG encoder init failed, error code[%d]( %s, %s, %d )\n", eRetCode, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	eRetCode = Encode_JPEG( &stSrcPic, &stDstStream, pEncoderState );
	if ( IPP_STATUS_NOERR != eRetCode )
	{
		TRACE( CAM_ERROR, "Error: IPP JPEG encoder failed, error code[%d]( %s, %s, %d )\n", eRetCode, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_PPUFAILED;
	}

	// NOTE: workaround here for IPP JPEG encoder's behavior: it will rewind buffer without notification in case of output buffer is smaller than required
	if ( IPP_STATUS_NOERR == eRetCode && ( stDstStream.pBsBuffer[0] != 0xFF || stDstStream.pBsBuffer[1] != 0xD8 ) )
	{
		TRACE( CAM_ERROR, "Error: output stream buffer is overwritten, which means output buffer is smaller than what is needed, "
		       "pls allocate bigger JPEG buffer for this, you can modify calcjpegbuflen() functon in cam_utility.c to enlarge "
		       "camera-engine JPEG buffer requirement( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_PPUFAILED;
	}

	EncoderFree_JPEG( &pEncoderState );
	miscFreeGeneralCallbackTable( &pCallBackTable );
	if ( error == CAM_ERROR_NONE )
	{
		pDstImg->iFilledLen[0] = (CAM_Int32s)stDstStream.pBsCurByte - (CAM_Int32s)stDstStream.pBsBuffer;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;
	}
	else
	{
		pDstImg->iFilledLen[0] = 0;
		pDstImg->iFilledLen[1] = 0;
		pDstImg->iFilledLen[2] = 0;
	}

	return error;
}

static CAM_Error _vmeta_yuv2jpeg( CAM_ImageBuffer *pSrcImg, CAM_ImageBuffer *pDstImg, CAM_JpegParam *pJpegParam )
{
#if defined( BUILD_OPTION_STRATEGY_ENABLE_VMETAJPEGENCODER )
	IppVmetaEncParSet        stEncParSet;
	void                     *pEncoderState = NULL;
	IppCodecStatus           eRetCode;
	IppVmetaBitstream        stBitStream;
	IppVmetaBitstream        *pBitStream;
	IppVmetaPicture          stPicture;
	IppVmetaPicture          *pPicture;
	IppVmetaEncInfo          stEncInfo;
	MiscGeneralCallbackTable *pCallBackTable = NULL;
	int                      ret;

	/* setup vmeta encoder parameters */
	memset( &stEncParSet, 0, sizeof( IppVmetaEncParSet ) );
	stEncParSet.eInputYUVFmt   = IPP_YCbYCr422I;
	stEncParSet.eOutputStrmFmt = IPP_VIDEO_STRM_FMT_JPEG;
	stEncParSet.nWidth         = pSrcImg->iWidth;
	stEncParSet.nHeight        = pSrcImg->iHeight;
	stEncParSet.nFrameRateNum  = 30;
	stEncParSet.bMultiIns      = 1;
	stEncParSet.nQP            = 90;
	stEncParSet.bFirstUser     = 0;
	stEncParSet.nBFrameNum     = 0;

	/* config vmeta picture buffer */
	memset( &stPicture, 0, sizeof( IppVmetaPicture ) );
	stPicture.pBuf             = pSrcImg->pBuffer[0];
	stPicture.nPhyAddr         = pSrcImg->iPhyAddr[0];
	stPicture.nBufSize         = pSrcImg->iAllocLen[0];
	stPicture.nDataLen         = pSrcImg->iFilledLen[0];
	stPicture.nOffset          = pSrcImg->iOffset[0];
	stPicture.nBufProp         = 0;

	/* config vmeta stream buffer */
	memset( &stBitStream, 0, sizeof( IppVmetaBitstream ) );
	stBitStream.pBuf     = pDstImg->pBuffer[0];
	stBitStream.nPhyAddr = pDstImg->iPhyAddr[0];
	stBitStream.nBufSize = pDstImg->iAllocLen[0];
	stBitStream.nDataLen = 0;
	stBitStream.nOffset  = pDstImg->iOffset[0];

	if ( miscInitGeneralCallbackTable( &pCallBackTable ) != 0 )
	{
		return CAM_ERROR_OUTOFMEMORY;
	}

	eRetCode = EncoderInitAlloc_Vmeta( &stEncParSet, pCallBackTable, &pEncoderState );
	if ( eRetCode != IPP_STATUS_NOERR )
	{
		EncoderFree_Vmeta( &pEncoderState );
		miscFreeGeneralCallbackTable( &pCallBackTable );
		TRACE( CAM_ERROR, "Error: vmeta encoder init fail, error code[%d]( %s, %s, %d )\n", eRetCode, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PPUFAILED;
	}

	eRetCode = EncoderPushBuffer_Vmeta( IPP_VMETA_BUF_TYPE_STRM, (void*)&stBitStream, pEncoderState );
	ASSERT( eRetCode == IPP_STATUS_NOERR );
	eRetCode = EncoderPushBuffer_Vmeta( IPP_VMETA_BUF_TYPE_PIC, (void*)&stPicture, pEncoderState );
	if ( eRetCode != IPP_STATUS_NOERR )
	{
		EncoderFree_Vmeta( &pEncoderState );
		miscFreeGeneralCallbackTable( &pCallBackTable );
		TRACE( CAM_ERROR, "Error: vmeta push buffer fail( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_PPUFAILED;
	}
	while ( 1 )
	{
		eRetCode = EncodeFrame_Vmeta( &stEncInfo, pEncoderState );
		
		CELOG( "eRetCode: %d\n", eRetCode );

		if ( eRetCode == IPP_STATUS_END_OF_PICTURE || eRetCode == IPP_STATUS_END_OF_STREAM || eRetCode == IPP_STATUS_OUTPUT_DATA )
		{
			EncoderPopBuffer_Vmeta( IPP_VMETA_BUF_TYPE_PIC, (void **)&pPicture, pEncoderState );
			if ( pPicture == NULL )
			{
				continue;
			}
			EncoderPopBuffer_Vmeta( IPP_VMETA_BUF_TYPE_STRM, (void **)&pBitStream, pEncoderState );
			if ( pBitStream == NULL )
			{
				continue;
			}
			else
			{
				break;
			}
		}
		else if ( eRetCode == IPP_STATUS_NEED_INPUT )
		{
			eRetCode = EncodeSendCmd_Vmeta( IPPVC_END_OF_STREAM, NULL, NULL, pEncoderState );
			ASSERT( eRetCode == IPP_STATUS_NOERR );
		}
	}

	pDstImg->iFilledLen[0] = pBitStream->nDataLen;
	pDstImg->iFilledLen[1] = 0;
	pDstImg->iFilledLen[2] = 0;

	EncoderFree_Vmeta( &pEncoderState );
	miscFreeGeneralCallbackTable( &pCallBackTable );

	return CAM_ERROR_NONE;
#else
	TRACE( CAM_ERROR, "pls open BUILD_OPTION_STRATEGY_ENABLE_VMETAJPEGENCODER to open vmeta encoder( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
	return CAM_ERROR_NOTSUPPORTEDARG;
#endif
}

static IppStatus ippiYCbYCrToYUV420SP( Ipp8u *pSrc[3], int *pSrcStep, IppiSize stSrcSize, Ipp8u *pDst[3], int *pDstStep, IppiSize stDstSize )
{
	int i, j, U, V;
	Ipp8u *pSrcOdd = NULL, *pSrcEven = NULL, *pSrcU = NULL, *pSrcV = NULL, *pDstYEven = NULL, *pDstYOdd = NULL, *pDstU = NULL, *pDstV = NULL;
    pSrcOdd = pSrc[0];
    pSrcEven = pSrc[0] + pSrcStep[0];

	pDstYOdd = pDst[0];
	pDstYEven = pDst[0] + pDstStep[0];
	pDstU = pDst[1];

	for ( i = 0; i < stSrcSize.height; i += 2 )
	{
		for ( j = 0; j < stSrcSize.width; j += 2 )
		{
            *pDstYOdd++ = *pSrcOdd++;
            U = *pSrcOdd++;
            *pDstYOdd++ = *pSrcOdd++;
            V = *pSrcOdd++;
            *pDstU++ = V;
            *pDstU++ = U;

            *pDstYEven++ = *pSrcEven++;
            pSrcEven++;
            /* *pDstU++ = pSrcEven++; */
            *pDstYEven++ = *pSrcEven++;
            pSrcEven++;
            /* *pDstU++ = pSrcEven++; */
		}

		pDstYOdd += ( pDstStep[0] << 1 )  - stDstSize.width;
		pDstYEven += ( pDstStep[0] << 1 ) - stDstSize.width;

		/* pDstU += ( pDstStep[1] << 1 ) - stDstSize.width; */
		/* pDstV += pDstStep[1] - ( stSrcSize.width >> 1 ); */

		pSrcOdd  += ( pSrcStep[0] << 1 ) - ( stSrcSize.width << 1 );
		pSrcEven += ( pSrcStep[0] << 1 ) - ( stSrcSize.width << 1 );
    }

    pDstStep[0] = stSrcSize.width;
    pDstStep[1] = stSrcSize.width;
    pDstStep[2] = 0;
	return ippStsNoErr; 
}

static IppStatus ippiYCbCr420ToCbYCrY_8u_P3C2R_C( Ipp8u *pSrc[3], int *pSrcStep, IppiSize stSrcSize, Ipp8u *pDst[3], int *pDstStep, IppiSize stDstSize )
{
	int   i, j;
	Ipp8u *pSrcYOdd = NULL, *pSrcYEven = NULL, *pSrcU = NULL, *pSrcV = NULL, *pDstOdd = NULL, *pDstEven = NULL;

	ASSERT( ( stSrcSize.width == stDstSize.width ) && ( stSrcSize.height == stDstSize.height ) );

	pSrcYOdd  = pSrc[0];
	pSrcYEven = pSrc[0] + pSrcStep[0];
	pSrcU     = pSrc[1];
	pSrcV     = pSrc[2];
	pDstOdd   = pDst[0];
	pDstEven  = pDst[0] + pDstStep[0];
	for ( i = 0; i < stSrcSize.height; i += 2 )
	{
		for ( j = 0; j < stSrcSize.width; j += 2 )
		{
			*pDstOdd++ = *pSrcU;
			*pDstOdd++ = *pSrcYOdd++;
			*pDstOdd++ = *pSrcV;
			*pDstOdd++ = *pSrcYOdd++;

			*pDstEven++ = *pSrcU++;
			*pDstEven++ = *pSrcYEven++;
			*pDstEven++ = *pSrcV++;
			*pDstEven++ = *pSrcYEven++;
		}

		pSrcYOdd += ( pSrcStep[0] << 1 )  - stSrcSize.width;
		pSrcYEven += ( pSrcStep[0] << 1 ) - stSrcSize.width;

		pSrcU += pSrcStep[1] - ( stSrcSize.width >> 1 );
		pSrcV += pSrcStep[2] - ( stSrcSize.width >> 1 );

		pDstOdd  += ( pDstStep[0] << 1 ) - ( stDstSize.width << 1 );
		pDstEven += ( pDstStep[0] << 1 ) - ( stDstSize.width << 1 );
	}

	return ippStsNoErr;
}

/*********************************************************************************
; Name:             ippiYCbCr422RszRot_8u_C2R_C
; Description:      This function combine resizing, color space conversion and
;                   rotation into one function to reduce memory access
; Input Arguments:  pSrc            - A Pointer to source image plane
;                   srcStep         - A 1-element vector containing the 
;                                     distance, in bytes, between the start of
;                                     lines in each of the input image plane.
;                   srcSize         - size of Source image
;                   pDst            - A 1-element vector containing pointers to
;                                    the start of each of the destination plane
;                   dstStep         - A 1-element vector containing the distance,
;                                     in bytes, between the start of lines in each
;                                     of the output image plane.
;                   dstSize         - size of output image.
;                   interpolation   - type of interpolation
;                                     to perform for resampling of the input image.
;                                     The following are currently supported.
;                                     ippCameraInterpNearearest Nearest neighboring
;                                     ippCameraInterpBilinear   Bilinear interpolation.
;                   rotation        - Rotation control parameter; must be of the
;                                     following pre-defined values:
;                                     ippCameraRotate90L, ippCameraRotate90R,
;                                     ippCameraRotate180, ippCameraRotateDisable
;                                     ippCameraFlipHrz or ippiCameraFlipVt
; Output Arguments:pDst             - Pointer to the vector to be initialized.
; Notes:    prototype:
;           IppStatus ippiYCbCr422RszRot_8u_C2R_C (const Ipp8u *pSrc,
;                                                  int srcStep, IppiSize srcSize, Ipp8u *pDst,
;                                                  int dstStep, IppiSize dstSize, IppCameraInterpolation
;                                                  interpolation, IppCameraRotation rotation, int rcpRatiox,
;                                                  int rcpRatioy)
;**********************************************************************************/

static IppStatus ippiYCbCr422RszRot_8u_C2R_C ( const Ipp8u *pSrc, int srcStep,
                                               IppiSize srcSize, Ipp8u *pDst, int dstStep, IppiSize dstSize,
                                               IppCameraInterpolation interpolation, IppCameraRotation rotation,
                                               int rcpRatiox, int rcpRatioy )
{
	int x, y;
	Ipp32s ay, fx, fx1, ay1, fx2;
	int dstWidth, dstHeight;
	Ipp32s xLimit, yLimit;
	Ipp8u *pSrcY1, *pSrcY2, *pSrcCb1, *pSrcCr1, *pSrcCb2, *pSrcCr2;
	Ipp8u *pDstY, *pDstCb, *pDstCr;
	Ipp8u *pDstY1, *pDstY2, *pDstY3, *pDstY4;
	Ipp8u *pDstCb1, *pDstCb2, *pDstCr1, *pDstCr2;
	int stepX, stepY, stepX1, stepX2, stepY1, stepY2;
	Ipp32s yLT, yRT, yLB, yRB, yT, yB, yR;
	Ipp8u Y1, Y2, Y3, Y4, Cb1, Cb2, Cr1, Cr2;
	int col, dcol, drow;
	int xNum, yNum;

	ASSERT( pSrc && pDst );

	dstWidth  = dstSize.width;
	dstHeight = dstSize.height;

	xLimit = (srcSize.width-1) << 16;
	yLimit = (srcSize.height-1) << 16;
	xNum   = dstSize.width;
	yNum   = dstSize.height;

	ay      = 0;
	ay1     = rcpRatioy;
	pSrcY1  = (Ipp8u*)(pSrc + 1);
	pSrcY2  = (Ipp8u*)(pSrcY1 + (ay1 >> 16) * srcStep);
	pSrcCb1 = (Ipp8u*)pSrc;
	pSrcCb2 = (Ipp8u*)(pSrcCb1 + ((ay1 + 32768) >> 16) * srcStep);
	pSrcCr1 = (Ipp8u*)(pSrc + 2);
	pSrcCr2 = (Ipp8u*)(pSrcCr1 + ((ay1 + 32768) >> 16) * srcStep);
	pDstY   = (Ipp8u*)(pDst + 1);
	pDstCb  = (Ipp8u*)pDst;
	pDstCr  = (Ipp8u*)(pDst + 2);

	switch ( rotation )
	{
	case ippCameraRotateDisable:
		pDstY1 = pDstY;
		pDstY2 = pDstY + 2;
		pDstY3 = pDstY + dstStep;
		pDstY4 = pDstY3 + 2;
		stepX  = 4;
		stepX1 = 4;
		stepX2 = 4;
		stepY  = 2 * dstStep - dstSize.width * 2;
		stepY1 = 2 * dstStep - dstSize.width * 2;
		stepY2 = 2 * dstStep - dstSize.width * 2;
		pDstCb1 = pDstCb;
		pDstCb2 = pDstCb + dstStep;
		pDstCr1 = pDstCr;
		pDstCr2 = pDstCr + dstStep;
		break;

	default:
		return ippStsBadArgErr;
	};

	if ( interpolation == ippCameraInterpBilinear )
	{
		for ( y = 0; y < yNum; y += 2 )
		{
			fx = 0;
			fx1 = rcpRatiox;
			fx2 = 32768;
			for ( x = 0; x < xNum; x += 2 )
			{
				/* 1. Resize for Y */
				col = fx >> 16;
				col = col * 2;
				dcol = fx & 0xffff;
				drow = ay & 0xffff;
				yLT = pSrcY1[col];
				yRT = pSrcY1[col + 2];
				yLB = pSrcY1[col + srcStep];
				yRB = pSrcY1[col + srcStep + 2];

				/*
				if ( fx == xLimit )
				{
					yRT = pSrcY1[col];
					yRB = pSrcY1[col + srcStep[0]];
				}
				else
				{
					yRT = pSrcY1[col + 1];
					yRB = pSrcY1[col + srcStep[0] + 1];
				}
				*/

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y1 = (Ipp8u)yR;

				col = fx1 >> 16;
				col = col * 2;
				dcol = fx1 & 0xffff;
				drow = ay & 0xffff;
				yLT = pSrcY1[col];
				// yRT = pSrcY1[col + 1];
				yLB = pSrcY1[col + srcStep];
				// yRB = pSrcY1[col + srcStep[0] + 1];

				if ( fx1 >= xLimit )
				{
					yRT = pSrcY1[col];
					yRB = pSrcY1[col + srcStep];
				}
				else
				{
					yRT = pSrcY1[col + 2];
					yRB = pSrcY1[col + srcStep + 2];
				}

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y2 = (Ipp8u)yR;

				col = fx >> 16;
				col = col * 2;
				dcol = fx & 0xffff;
				drow = ay1 & 0xffff;
				yLT = pSrcY2[col];
				yRT = pSrcY2[col + 2];
				yLB = pSrcY2[col + srcStep];
				yRB = pSrcY2[col + srcStep + 2];

				/*
				if ( fx == xLimit )
				{
					yRT = pSrcY2[col];
					yRB = pSrcY2[col + srcStep[0]];
				}
				else
				{
					yRT = pSrcY2[col + 1];
					yRB = pSrcY2[col + srcStep[0] + 1];
				}
				*/
				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y3 = (Ipp8u)yR;

				col = fx1 >> 16;
				col = col * 2;
				dcol = fx1 & 0xffff;
				drow = ay1 & 0xffff;
				yLT = pSrcY2[col];
				// yRT = pSrcY2[col + 2];
				yLB = pSrcY2[col + srcStep];
				// yRB = pSrcY2[col + srcStep + 2];

				if ( fx1 >= xLimit )
				{
					yRT = pSrcY2[col];
					yRB = pSrcY2[col + srcStep];
				}
				else
				{
					yRT = pSrcY2[col + 2];
					yRB = pSrcY2[col + srcStep + 2];
				}

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y4 = (Ipp8u) yR;

				/* 2. Resize for Cb and Cr */
				col = fx2 >> 16;
				col = col * 4;
				Cb1 = pSrcCb1[col];
				Cb2 = pSrcCb2[col];
				Cr1 = pSrcCr1[col];
				Cr2 = pSrcCr2[col];

				fx += rcpRatiox << 1;
				fx1 += rcpRatiox << 1;
				/*
				if ( fx1 >= xLimit )
				{
					fx1 = xLimit - 1;
				}
				*/
				fx2 += rcpRatiox;
				/*
				if ( fx2 >= (xLimit >> 1) )
				{
					fx2 = (xLimit >> 1) - 1;
				}
				*/
				/* 2. Rotation storage */
				*pDstY1 = Y1;
				pDstY1 += stepX;
				*pDstY2 = Y2;
				pDstY2 += stepX;
				*pDstY3 = Y3;
				pDstY3 += stepX;
				*pDstY4 = Y4;
				pDstY4 += stepX;
				*pDstCb1 = Cb1;
				pDstCb1 += stepX1;
				*pDstCb2 = Cb2;
				pDstCb2 += stepX1;
				*pDstCr1 = Cr1;
				pDstCr1 += stepX2;
				*pDstCr2 = Cr2;
				pDstCr2 += stepX2;
			}
			pDstY1 += stepY;
			pDstY2 += stepY;
			pDstY3 += stepY;
			pDstY4 += stepY;
			pDstCb1 += stepY1;
			pDstCb2 += stepY1;
			pDstCr1 += stepY2;
			pDstCr2 += stepY2;

			ay += 2 * rcpRatioy;
			ay1 += 2 * rcpRatioy;
			if ( ay1 >= yLimit )
			{
				ay1 = yLimit - 1;
			}

			pSrcY1  = (Ipp8u*)(pSrc + 1 + (ay >> 16) * srcStep);
			pSrcY2  = (Ipp8u*)(pSrc + 1 + (ay1 >> 16) * srcStep);
			pSrcCb1 = (Ipp8u*)(pSrc + ((ay+32768) >> 16) * srcStep);
			pSrcCb2 = (Ipp8u*)(pSrc + ((ay1+32768) >> 16) * srcStep);
			pSrcCr1 = (Ipp8u*)(pSrc + 2 + ((ay+32768) >> 16) * srcStep);
			pSrcCr2 = (Ipp8u*)(pSrc + 2 + ((ay1+32768) >> 16) * srcStep);
		}
	}
	else
	{
		return ippStsBadArgErr;
	}

	return ippStsNoErr;
}

// temporal workaround for nevo release ICS3, will be removed after neon optimized I420 resize rotate is done.
#if defined( PLATFORM_BOARD_NEVO )
static IppStatus ippiYCbCr420RszRot_8u_P3R_C( const Ipp8u *pSrc[3], int srcStep[3], IppiSize srcSize, Ipp8u *pDst[3],
                                            int dstStep[3], IppiSize dstSize, IppCameraInterpolation interpolation,
                                            IppCameraRotation rotation, int rcpRatiox, int rcpRatioy )
{
	int     x, y;
	int	    stepX_Y, stepY_Y, stepX_Cb, stepX_Cr, stepY_Cb, stepY_Cr;
	int     col, dcol, drow;
	Ipp32s  ay, fx;
	Ipp8u 	*pSrcY, *pSrcCb, *pSrcCr;
	Ipp8u   *pDstY, *pDstCb, *pDstCr;
	Ipp32s  xLimit, yLimit;
	int     xNum, yNum;
	Ipp32s  yLT, yRT, yLB, yRB, yT, yB, yR;
	Ipp8u 	Y;
	int     tmp, tmp1;

	ASSERT( pSrc && pDst );

	xNum	 = dstSize.width;
	yNum	 = dstSize.height;
	xLimit	 = (srcSize.width-1)  << 16;
	yLimit   = (srcSize.height-1) << 16;

	switch (rotation) {
	case ippCameraRotateDisable:
		pDstY    = pDst[0];
		pDstCb   = pDst[1];
		pDstCr   = pDst[2];
		stepX_Y  = 1;
		stepY_Y  = dstStep[0] - dstSize.width;
		stepX_Cb = 1;
		stepY_Cb = dstStep[1] - (dstSize.width>>1);
		stepX_Cr = 1;
		stepY_Cr = dstStep[2] - (dstSize.width>>1);
		break;
	case ippCameraRotate90L:
		pDstY    = pDst[0] + dstStep[0] * (dstSize.width - 1);
		pDstCb   = pDst[1] + dstStep[1] * ((dstSize.width>>1) - 1);
		pDstCr   = pDst[2] + dstStep[2] * ((dstSize.width>>1) - 1);
		stepX_Y  = -dstStep[0];
		stepY_Y  = dstStep[0] * dstSize.width + 1;
		stepX_Cb = -dstStep[1];
		stepY_Cb = dstStep[1] * (dstSize.width>>1) + 1;
		stepX_Cr = -dstStep[2];
		stepY_Cr = dstStep[2] * (dstSize.width>>1) + 1;
		break;
	case ippCameraRotate90R:
		pDstY    = pDst[0] + (dstSize.height-1);
		pDstCb   = pDst[1] + ((dstSize.height>>1) - 1);
		pDstCr   = pDst[2] + ((dstSize.height>>1) - 1);
		stepX_Y  = dstStep[0];
		stepY_Y  = -dstStep[0] * dstSize.width - 1;
		stepX_Cb = dstStep[1];
		stepY_Cb = -dstStep[1] * (dstSize.width>>1) - 1;;
		stepX_Cr = dstStep[2];
		stepY_Cr =-dstStep[2] * (dstSize.width>>1) - 1;;
		break;
	case ippCameraRotate180:
		pDstY    = pDst[0] + (dstSize.height-1)*dstStep[0] + dstSize.width-1;
		pDstCb   = pDst[1] + ((dstSize.height>>1)-1)*dstStep[1] + (dstSize.width>>1)-1;
		pDstCr   = pDst[2] + ((dstSize.height>>1)-1)*dstStep[2] + (dstSize.width>>1)-1;
		stepX_Y  = -1;
		stepY_Y  = dstSize.width - dstStep[0];
		stepX_Cb = -1;
		stepY_Cb = (dstSize.width>>1) - dstStep[1];
		stepX_Cr = -1;
		stepY_Cr = (dstSize.width>>1) - dstStep[2];
		break;
	case ippCameraFlipHorizontal:
		pDstY    = pDst[0] + dstSize.width - 1;
		pDstCb   = pDst[1] + (dstSize.width>>1) - 1;
		pDstCr   = pDst[2] + (dstSize.width>>1) - 1;
		stepX_Y  = -1;
		stepY_Y  = dstStep[0] + dstSize.width;
		stepX_Cb = -1;
		stepY_Cb = dstStep[1] + (dstSize.width>>1);
		stepX_Cr = -1;
		stepY_Cr = dstStep[2] + (dstSize.width>>1);
		break;
	case ippCameraFlipVertical:
		pDstY    = pDst[0] + (dstSize.height - 1) * dstStep[0];
		pDstCb   = pDst[1] + ((dstSize.height>>1) - 1) * dstStep[1];
		pDstCr   = pDst[2] + ((dstSize.height>>1) - 1) * dstStep[2];
		stepX_Y  = 1;
		stepY_Y  = -dstStep[0] - dstSize.width;
		stepX_Cb = 1;
		stepY_Cb = -dstStep[1] - (dstSize.width>>1);
		stepX_Cr = 1;
		stepY_Cr = -dstStep[1] - (dstSize.width>>1);
		break;
	default:
		return ippStsBadArgErr;
	};

	if ((rotation == ippCameraRotateDisable) &&
		(((dstSize.width == (srcSize.width>>1)) && (dstSize.height == (srcSize.height>>1))) ||
		((dstSize.width == (srcSize.width>>2)) && (dstSize.height == (srcSize.height>>2))))){
		pSrcY = (Ipp8u*)pSrc[0];
		pSrcCb= (Ipp8u*)pSrc[1];
		pSrcCr= (Ipp8u*)pSrc[2];
		if (dstSize.width == (srcSize.width>>1)){
			for (y=0; y<yNum; y+=2){
				for (x =0; x<xNum; x+=2){
					if (ippCameraInterpNearest == interpolation){
						*pDstY     = pSrcY[(x<<1)+1+(y<<1)*srcStep[0]];
						*(pDstY+1) = pSrcY[((x+1)<<1)+1+(y<<1)*srcStep[0]];
						*(pDstY+dstStep[0]) = pSrcY[(x<<1)+1+((y+1)<<1)*srcStep[0]];
						*(pDstY+dstStep[0]+1) = pSrcY[((x+1)<<1)+1+((y+1)<<1)*srcStep[0]];
						*pDstCb    = pSrcCb[(x+1)+(y)*srcStep[1]];
						*pDstCr    = pSrcCr[(x+1)+(y)*srcStep[1]];
					}else{
						*pDstY     = (Ipp8u)((pSrcY[(x<<1)+(y<<1)*srcStep[0]]
							+ pSrcY[(x<<1)+1+(y<<1)*srcStep[0]]
							+ pSrcY[(x<<1)+((y<<1)+1)*srcStep[0]]
							+ pSrcY[(x<<1)+1+((y<<1)+1)*srcStep[0]] + 4)>>2);
						*(pDstY+1) = (Ipp8u)((pSrcY[((x+1)<<1)+(y<<1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+1+(y<<1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+((y<<1)+1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+1+((y<<1)+1)*srcStep[0]] + 4)>>2);
						*(pDstY+dstStep[0]) = (Ipp8u)((pSrcY[((x)<<1)+((y+1)<<1)*srcStep[0]]
							+ pSrcY[((x)<<1)+1+((y+1)<<1)*srcStep[0]]
							+ pSrcY[((x)<<1)+(((y+1)<<1)+1)*srcStep[0]]
							+ pSrcY[((x)<<1)+1+(((y+1)<<1)+1)*srcStep[0]] + 4)>>2);
						*(pDstY+dstStep[0]+1) = (Ipp8u)((pSrcY[((x+1)<<1)+((y+1)<<1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+1+((y+1)<<1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+(((y+1)<<1)+1)*srcStep[0]]
							+ pSrcY[((x+1)<<1)+1+(((y+1)<<1)+1)*srcStep[0]] + 4)>>2);
						*pDstCb    = (Ipp8u)((pSrcCb[x+y*srcStep[1]]
							+ pSrcCb[(x+1)+y*srcStep[1]]
							+ pSrcCb[x+(y+1)*srcStep[1]]
							+ pSrcCb[(x+1)+(y+1)*srcStep[1]] + 4)>>2);
						*pDstCr    = (Ipp8u)((pSrcCr[x+y*srcStep[2]]
							+ pSrcCr[(x+1)+y*srcStep[2]]
							+ pSrcCr[x+(y+1)*srcStep[2]]
							+ pSrcCr[(x+1)+(y+1)*srcStep[2]] + 4)>>2);
					}
					pDstY  += 2;
					pDstCb += 1;
					pDstCr += 1;
				}
				pDstY  += ((dstStep[0]<<1) - xNum);
				pDstCb += (dstStep[1] - (xNum>>1));
				pDstCr += (dstStep[2] - (xNum>>1));
			}
		}else{
			for (y=0; y<yNum; y+=2){
				for (x =0; x<xNum; x+=2){
					if (ippCameraInterpNearest == interpolation){
						*pDstY     = pSrcY[(x<<2)+1+((y<<2)+1)*srcStep[0]];
						*(pDstY+1) = pSrcY[((x+1)<<2)+1+((y<<2)+1)*srcStep[0]];
						*(pDstY+dstStep[0]) = pSrcY[(x<<2)+1+(((y+1)<<2)+1)*srcStep[0]];
						*(pDstY+dstStep[0]+1) = pSrcY[((x+1)<<2)+1+(((y+1)<<2)+1)*srcStep[0]];
						*pDstCb    = pSrcCb[((x<<1)+1)+((y<<1)+1)*srcStep[1]];
						*pDstCr    = pSrcCr[((x<<1)+1)+(y<<1)*srcStep[1]];
					}else{
						tmp        = ((y<<2)+1)*srcStep[0] + (x<<2) + 1;
						*pDstY     = (Ipp8u)((pSrcY[tmp] + pSrcY[tmp+1]
							+ pSrcY[tmp+srcStep[0]]
							+ pSrcY[tmp+srcStep[0]+1]+4)>>2);
						tmp1       = tmp + 4;
						*(pDstY+1) = (Ipp8u)((pSrcY[tmp1] + pSrcY[tmp1+1]
							+ pSrcY[tmp1+srcStep[0]]
							+ pSrcY[tmp1+srcStep[0]+1]+4)>>2);

						tmp       += (srcStep[0]<<2);
						*(pDstY+dstStep[0]) = (Ipp8u)((pSrcY[tmp] + pSrcY[tmp+1]
							+ pSrcY[tmp+srcStep[0]]
							+ pSrcY[tmp+srcStep[0]+1]+4)>>2);
						tmp1       = tmp + 4;
						*(pDstY+dstStep[0]+1) = (Ipp8u)((pSrcY[tmp1] + pSrcY[tmp1+1]
							+ pSrcY[tmp1+srcStep[0]]
							+ pSrcY[tmp1+srcStep[0]+1]+4)>>2);

						tmp        = ((y<<1)+1)*srcStep[1] + (x<<1) + 1;
						*pDstCb    = (Ipp8u)((pSrcCb[tmp] + pSrcCb[tmp+1]
							+ pSrcCb[tmp+srcStep[1]]
							+ pSrcCb[tmp+1+srcStep[1]] + 4)>>2);

						tmp        = ((y<<1)+1)*srcStep[2] + (x<<1) + 1;
						*pDstCr    = (Ipp8u)((pSrcCr[tmp] + pSrcCr[tmp+1]
							+ pSrcCr[tmp+srcStep[2]]
							+ pSrcCr[tmp+1+srcStep[2]] + 4)>>2);
					}
					pDstY  += 2;
					pDstCb += 1;
					pDstCr += 1;
				}
				pDstY  += ((dstStep[0]<<1) - xNum);
				pDstCb += (dstStep[1] - (xNum>>1));
				pDstCr += (dstStep[2] - (xNum>>1));
			}
		}
	}else{
		if ( ippCameraInterpNearest == interpolation){
			ay = 32768;
			for (y = 0; y < yNum; y++){
				fx = 32768;
				for (x = 0; x < xNum; x++) {
					pSrcY  = (Ipp8u*)(pSrc[0] + (ay>>16) * srcStep[0]);
					col    = fx >> 16;
					*pDstY = pSrcY[col];

					fx += rcpRatiox;
					pDstY += stepX_Y;
				}
				ay  += rcpRatioy;

				pDstY  += stepY_Y;
			}

		}else{
			ay =0;
			/* resizing Y plane */
			for (y = 0; y < yNum; y++){
				fx = 0;
				for (x = 0; x < xNum; x++) {
					pSrcY = (Ipp8u*)(pSrc[0]+(ay >> 16) * srcStep[0]);
					col = fx >> 16;
					dcol = fx & 0xffff;
					drow = ay & 0xffff;
					yLT = pSrcY[col];
					yLB = pSrcY[col + srcStep[0]];
					if (fx == xLimit){
						yRT = pSrcY[col];
						yRB = pSrcY[col + srcStep[0]];
					}else{
						yRT = pSrcY[col + 1];
						yRB = pSrcY[col + srcStep[0] + 1];
					}
					yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
					yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
					yR = ((drow * (yB - yT)) >> 16) + yT;
					Y  = (Ipp8u)yR;
					*pDstY = Y;

					fx += rcpRatiox;
					pDstY += stepX_Y;
				}
				ay  += rcpRatioy;

				if (ay>= yLimit) {
					ay = yLimit - 1;
				}
				pDstY  += stepY_Y;
			}

		}

		yNum = yNum >> 1;
		xNum = xNum >> 1;

		ay = 32768;
		/* resizing Cb/Cr plane */
		for (y = 0; y < yNum; y++){
			fx = 32768;
			for (x = 0; x < xNum; x++) {
				pSrcCb = (Ipp8u*)(pSrc[1] + (ay>>16)*srcStep[1]);
				pSrcCr = (Ipp8u*)(pSrc[2] + (ay>>16)*srcStep[2]);
				col    = fx >> 16;
				*pDstCr = pSrcCr[col];
				*pDstCb = pSrcCb[col];

				fx += rcpRatiox;

				pDstCb += stepX_Cb;
				pDstCr += stepX_Cr;
			}
			ay += rcpRatioy;

			pDstCr  += stepY_Cr;
			pDstCb  += stepY_Cb;
		}
	}
	return ippStsNoErr;
}

static IppStatus ippiYCbCr422RszRot_8u_P3R_C( const Ipp8u *pSrc[3], int srcStep[3],
                                              IppiSize srcSize, Ipp8u *pDst[3], int dstStep[3], IppiSize dstSize,
                                              IppCameraInterpolation interpolation, IppCameraRotation rotation,
                                              int rcpRatiox, int rcpRatioy )
{
	int x, y;
	Ipp32s ay, fx, fx1, ay1, fx2;
	int dstWidth, dstHeight;
	Ipp32s xLimit, yLimit;
	Ipp8u *pSrcY1, *pSrcY2, *pSrcCb1, *pSrcCr1, *pSrcCb2, *pSrcCr2;
	Ipp8u *pDstY, *pDstCb, *pDstCr;
	Ipp8u *pDstY1, *pDstY2, *pDstY3, *pDstY4;
	Ipp8u *pDstCb1, *pDstCb2, *pDstCr1, *pDstCr2;
	int stepX, stepY, stepX1, stepX2, stepY1, stepY2;
	Ipp32s yLT, yRT, yLB, yRB, yT, yB, yR;
	Ipp8u Y1, Y2, Y3, Y4, Cb1, Cb2, Cr1, Cr2;
	int col, dcol, drow;
	int xNum, yNum;

	ASSERT( pSrc && pDst );

	dstWidth = dstSize.width;
	dstHeight = dstSize.height;

	xLimit = (srcSize.width-1) << 16;
	yLimit = (srcSize.height-1) << 16;
	xNum = dstSize.width;
	yNum = dstSize.height;

	ay = 0;
	ay1 = rcpRatioy;
	pSrcY1 = (Ipp8u*)(pSrc[0]);
	pSrcY2 = (Ipp8u*)(pSrcY1 + (ay1 >> 16) * srcStep[0]);
	pSrcCb1 = (Ipp8u*)pSrc[1];
	pSrcCb2 = (Ipp8u*)(pSrcCb1 + ((ay1+32768) >> 16) * srcStep[1]);
	pSrcCr1 = (Ipp8u*)pSrc[2];
	pSrcCr2 = (Ipp8u*)(pSrcCr1 + ((ay1+32768) >> 16) * srcStep[2]);
	pDstY = (Ipp8u *)pDst[0];
	pDstCb = (Ipp8u *)pDst[1];
	pDstCr = (Ipp8u *)pDst[2];

	switch (rotation) {
	case ippCameraRotateDisable:
		pDstY1 = pDstY;
		pDstY2 = pDstY + 1;
		pDstY3 = pDstY + dstStep[0];
		pDstY4 = pDstY3 + 1;
		stepX = 2;
		stepX1 = 1;
		stepX2 = 1;
		stepY = 2 * dstStep[0] - dstSize.width;
		stepY1 = 2 * dstStep[1] - (dstSize.width >> 1);
		stepY2 = 2 * dstStep[2] - (dstSize.width >> 1);
		pDstCb1 = pDstCb;
		pDstCb2 = pDstCb + dstStep[1];
		pDstCr1 = pDstCr;
		pDstCr2 = pDstCr + dstStep[2];
		break;
	case ippCameraRotate90L:
		pDstY1 = pDstY + dstStep[0] * (dstSize.width - 1);
		pDstY2 = pDstY1 - dstStep[0];
		pDstY3 = pDstY1 + 1;
		pDstY4 = pDstY2 + 1;
		stepX = -2 * dstStep[0];
		stepX1 = -2 * dstStep[1];
		stepX2 = -2 * dstStep[2];
		stepY = dstStep[0] * dstSize.width + 2;
		stepY1 = dstStep[1] * dstSize.width + 1;
		stepY2 = dstStep[2] * dstSize.width + 1;
		pDstCb1 = pDstCb + (dstSize.width - 1) * dstStep[1];
		pDstCb2 = pDstCb1 - dstStep[1];
		pDstCr1 = pDstCr + (dstSize.width - 1) * dstStep[2];
		pDstCr2 = pDstCr1 - dstStep[2];
		break;
	case ippCameraRotate90R:
		pDstY1 = pDstY + dstSize.height - 1;
		pDstY2 = pDstY1 + dstStep[0];
		pDstY3 = pDstY1 - 1;
		pDstY4 = pDstY2 - 1;
		stepX = 2 * dstStep[0];
		stepX1 = 2 * dstStep[1];
		stepX2 = 2 * dstStep[2];
		stepY = -(dstStep[0] * dstSize.width + 2);
		stepY1 = -(dstStep[1] * dstSize.width + 1);
		stepY2 = -(dstStep[2] * dstSize.width + 1);
		pDstCb1 = pDstCb + (dstSize.height >> 1) - 1;
		pDstCb2 = pDstCb1 + dstStep[1];
		pDstCr1 = pDstCr + (dstSize.height >> 1) - 1;
		pDstCr2 = pDstCr1 + dstStep[2];
		break;
	case ippCameraRotate180:
		pDstY1 = pDstY + (dstSize.height - 1) * dstStep[0] + dstSize.width - 1;
		pDstY2 = pDstY1 - 1;
		pDstY3 = pDstY1 - dstStep[0];
		pDstY4 = pDstY3 - 1;
		stepX = -2;
		stepX1 = -1;
		stepX2 = -1;
		stepY = - ((dstStep[0] << 1) - dstSize.width);
		stepY1 = - ((dstStep[1] << 1) - (dstSize.width >> 1));
		stepY2 = - ((dstStep[2] << 1) - (dstSize.width >> 1));
		pDstCb1 = pDstCb + (dstSize.height - 1) * dstStep[1] + (dstSize.width >> 1) - 1;
		pDstCb2 = pDstCb1 - dstStep[1];
		pDstCr1 = pDstCr + (dstSize.height - 1) * dstStep[2] + (dstSize.width >> 1) - 1;
		pDstCr2 = pDstCr1 - dstStep[2];
		break;
	case ippCameraFlipHorizontal:
		pDstY1 = pDstY + dstSize.width - 1;
		pDstY2 = pDstY1 - 1;
		pDstY3 = pDstY1 + dstStep[0];
		pDstY4 = pDstY3 - 1;
		stepX = -2;
		stepX1 = -1;
		stepX2 = -1;
		stepY = 2 * dstStep[0] + dstSize.width;
		stepY1 = 2 * dstStep[1] + (dstSize.width >> 1);
		stepY2 = 2 * dstStep[2] + (dstSize.width >> 1);
		pDstCb1 = pDstCb + (dstSize.width >> 1) - 1;
		pDstCb2 = pDstCb1 + dstStep[1];
		pDstCr1 = pDstCr + (dstSize.width >> 1) - 1;
		pDstCr2 = pDstCr1 + dstStep[2];
		break;
	case ippCameraFlipVertical:
		pDstY1 = pDstY + dstStep[0] * (dstSize.height - 1);
		pDstY2 = pDstY1 + 1;
		pDstY3 = pDstY1 - dstStep[0];
		pDstY4 = pDstY3 + 1;
		stepX = 2;
		stepX1 = 1;
		stepX2 = 1;
		stepY = -(2 * dstStep[0] + dstSize.width);
		stepY1 = -(2 * dstStep[1] + (dstSize.width >> 1));
		stepY2 = -(2 * dstStep[2] + (dstSize.width >> 1));
		pDstCb1 = pDstCb + dstStep[1] * (dstSize.height - 1);
		pDstCb2 = pDstCb1 - dstStep[1];
		pDstCr1 = pDstCr + dstStep[2] * (dstSize.height - 1);
		pDstCr2 = pDstCr1 - dstStep[2];
		break;
	default:
		return ippStsBadArgErr;
	};

	{
		for(y = 0; y < yNum; y += 2) {
			fx = 0;
			fx1 = rcpRatiox;
			fx2 = 32768;
			for(x = 0; x < xNum; x+= 2) {
				/* 1. Resize for Y */
				col = fx >> 16;
				dcol = fx & 0xffff;
				drow = ay & 0xffff;
				yLT = pSrcY1[col];
				yRT = pSrcY1[col + 1];
				yLB = pSrcY1[col + srcStep[0]];
				yRB = pSrcY1[col + srcStep[0] + 1];

				/*if (fx == xLimit){
					yRT = pSrcY1[col];
					yRB = pSrcY1[col + srcStep[0]];
				}else{
					yRT = pSrcY1[col + 1];
					yRB = pSrcY1[col + srcStep[0] + 1];
				}*/

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y1 = (Ipp8u)yR;

				col = fx1 >> 16;
				dcol = fx1 & 0xffff;
				drow = ay & 0xffff;
				yLT = pSrcY1[col];
			//	yRT = pSrcY1[col + 1];
				yLB = pSrcY1[col + srcStep[0]];
			//	yRB = pSrcY1[col + srcStep[0] + 1];

				if (fx1 >= xLimit){
					yRT = pSrcY1[col];
					yRB = pSrcY1[col + srcStep[0]];
				}else{
					yRT = pSrcY1[col + 1];
					yRB = pSrcY1[col + srcStep[0] + 1];
				}/**/

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y2 = (Ipp8u)yR;

				col = fx >> 16;
				dcol = fx & 0xffff;
				drow = ay1 & 0xffff;
				yLT = pSrcY2[col];
				yRT = pSrcY2[col + 1];
				yLB = pSrcY2[col + srcStep[0]];
				yRB = pSrcY2[col + srcStep[0] + 1];

				/*				if (fx == xLimit){
					yRT = pSrcY2[col];
					yRB = pSrcY2[col + srcStep[0]];
				}else{
					yRT = pSrcY2[col + 1];
					yRB = pSrcY2[col + srcStep[0] + 1];
				}*/
				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y3 = (Ipp8u)yR;

				col = fx1 >> 16;
				dcol = fx1 & 0xffff;
				drow = ay1 & 0xffff;
				yLT = pSrcY2[col];
			//	yRT = pSrcY2[col + 1];
				yLB = pSrcY2[col + srcStep[0]];
			//	yRB = pSrcY2[col + srcStep[0] + 1];

				if (fx1 >= xLimit){
					yRT = pSrcY2[col];
					yRB = pSrcY2[col + srcStep[0]];
				}else{
					yRT = pSrcY2[col + 1];
					yRB = pSrcY2[col + srcStep[0] + 1];
				}/**/

				yT = ((dcol * (yRT - yLT)) >> 16) + yLT;
				yB = ((dcol * (yRB - yLB)) >> 16) + yLB;
				yR = ((drow * (yB - yT)) >> 16) + yT;
				Y4 = (Ipp8u) yR;

				/* 2. Resize for Cb and Cr */
				col = fx2 >> 16;
				Cb1 = pSrcCb1[col];
				Cb2 = pSrcCb2[col];
				Cr1 = pSrcCr1[col];
				Cr2 = pSrcCr2[col];

				fx += rcpRatiox << 1;
				fx1 += rcpRatiox << 1;
			/*	if(fx1 >= xLimit) {
					fx1 = xLimit - 1;
				}*/
				fx2 += rcpRatiox;
			/*	if(fx2 >= (xLimit >> 1)) {
					fx2 = (xLimit >> 1) - 1;
				}
			*/
				/* 2. Rotation storage */
				*pDstY1 = Y1;
				pDstY1 += stepX;
				*pDstY2 = Y2;
				pDstY2 += stepX;
				*pDstY3 = Y3;
				pDstY3 += stepX;
				*pDstY4 = Y4;
				pDstY4 += stepX;
				*pDstCb1 = Cb1;
				pDstCb1 += stepX1;
				*pDstCb2 = Cb2;
				pDstCb2 += stepX1;
				*pDstCr1 = Cr1;
				pDstCr1 += stepX2;
				*pDstCr2 = Cr2;
				pDstCr2 += stepX2;
			}
			pDstY1 += stepY;
			pDstY2 += stepY;
			pDstY3 += stepY;
			pDstY4 += stepY;
			pDstCb1 += stepY1;
			pDstCb2 += stepY1;
			pDstCr1 += stepY2;
			pDstCr2 += stepY2;

			ay += 2 * rcpRatioy;
			ay1 += 2 * rcpRatioy;
			if(ay1 >= yLimit) {
				ay1 = yLimit - 1;
			}

			pSrcY1 = (Ipp8u*)(pSrc[0] + (ay >> 16) * srcStep[0]);
			pSrcY2 = (Ipp8u*)(pSrc[0] + (ay1 >> 16) * srcStep[0]);
			pSrcCb1 = (Ipp8u*)(pSrc[1] + ((ay+32768) >> 16) * srcStep[1]);
			pSrcCb2 = (Ipp8u*)(pSrc[1] + ((ay1+32768) >> 16) * srcStep[1]);
			pSrcCr1 = (Ipp8u*)(pSrc[2] + ((ay+32768) >> 16) * srcStep[2]);
			pSrcCr2 = (Ipp8u*)(pSrc[2] + ((ay1+32768) >> 16) * srcStep[2]);
		}
	}

	return ippStsNoErr;
}
#endif


static void _ppu_retrieve_format( CAM_ImageFormat eFmt, CAM_ImageFormat *pFmtCap, CAM_Int32s iFmtCapCnt, CAM_Int32s *pIndex )
{
	int j;

	*pIndex = -1;

	for ( j = 0; j < iFmtCapCnt; j++ )
	{
		if ( pFmtCap[j] == eFmt )
		{
			*pIndex = j;
			break;
		}
	}

	return;
}

static void _ppu_copy_image( Ipp8u *pSrc, int iSrcStep, Ipp8u *pDst, int iDstStep, int iBytesPerLine, int iLines )
{
	int   i, j;
	Ipp8u *pSrcBuf = NULL, *pDstBuf = NULL;

	pSrcBuf = pSrc;
	pDstBuf = pDst;

	for ( i = 0; i < iLines; i++ )
	{
		memmove( pDstBuf, pSrcBuf, iBytesPerLine );
		pSrcBuf += iSrcStep;
		pDstBuf += iDstStep;
	}

	return;
}
