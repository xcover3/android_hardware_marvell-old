/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/
// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================

#include "DxOFmtConvert.h"
#include "CameraEngine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define   DBG_LOG(...)            do{ printf( __VA_ARGS__ ); fflush( NULL ); }while( 0 )

static const CAM_Int32u S_uiIndex420_Y[NB_YUV420ORDER][4] = {{0,1,0,1},   //YUV420ORDER_YYU
                                                             {0,2,0,2},   //YUV420ORDER_YUY
                                                             {1,2,1,2}};  //YUV420ORDER_UYY

//The third entry is the line position of V
static const CAM_Int32u S_uiIndex420_UV[NB_YUV420ORDER][3] = {{2,2,1},   //YUV420ORDER_YYU
                                                              {1,1,1},   //YUV420ORDER_YUY
                                                              {0,0,1}};  //YUV420ORDER_UYY

static const CAM_Int32u S_uiIndex422_Y[NB_YUV422ORDER][4] = {{0,2,0,2},  //YUV422ORDER_YUYV
                                                             {0,1,0,1},  //YUV422ORDER_YYUV
                                                             {1,2,1,2},  //YUV422ORDER_UYYV
                                                             {2,3,2,3},  //YUV422ORDER_UVYY
                                                             {1,3,1,3},  //YUV422ORDER_UYVY
                                                             {0,3,0,3},  //YUV422ORDER_YUVY
                                                             {0,2,0,2},  //YUV422ORDER_YVYU
                                                             {0,1,0,1},  //YUV422ORDER_YYVU
                                                             {1,2,1,2},  //YUV422ORDER_VYYU
                                                             {2,3,2,3},  //YUV422ORDER_VUYY
                                                             {1,3,1,3},  //YUV422ORDER_VYUY
                                                             {0,3,0,3}}; //YUV422ORDER_YVUY

static const CAM_Int32u S_uiIndex422_UV[NB_YUV422ORDER][4] = {{1,1,3,3},  //YUV422ORDER_YUYV
                                                              {2,2,3,3},  //YUV422ORDER_YYUV
                                                              {0,0,3,3},  //YUV422ORDER_UYYV
                                                              {0,0,1,1},  //YUV422ORDER_UVYY
                                                              {0,0,2,2},  //YUV422ORDER_UYVY
                                                              {1,1,2,2},  //YUV422ORDER_YUVY
                                                              {3,3,1,1},  //YUV422ORDER_YVYU
                                                              {3,3,2,2},  //YUV422ORDER_YYVU
                                                              {3,3,0,0},  //YUV422ORDER_VYYU
                                                              {1,1,0,0},  //YUV422ORDER_VUYY
                                                              {2,2,0,0},  //YUV422ORDER_VYUY
                                                              {2,2,1,1}}; //YUV422ORDER_YVUY

static const CAM_Int8u S_ucIndexRGB[NB_RGBORDER][3] = {{0,1,2},  //RGBORDER_RGB
                                                       {0,2,1},  //RGBORDER_RBG
                                                       {1,0,2},  //RGBORDER_GRB
                                                       {2,0,1},  //RGBORDER_GBR
                                                       {2,1,0},  //RGBORDER_BGR
                                                       {1,2,0}}; //RGBORDER_BRG

typedef struct _DXRImageHeader
{
	CAM_Int8s  ucFourCC[4];         // 'DXR '
	CAM_Int8s  ucType[16];          // hint for interpreting the raw data
	CAM_Int32s uiWidth;             // image width (per channel)
	CAM_Int32s uiHeight;            // image height (per channel)
	CAM_Int32s uiPrecision;         // precision, i.e. number of bits per sample
	CAM_Int32s uiIsSigned;          // 0 for unsigned, 1 for signed samples
	CAM_Int32s uiChannels;          // number of channels
	CAM_Int32s uiExtensions;        // number of extensions
	CAM_Int8s  ucPadding[20];       // reserved for future use
} CAM_DXRImageHeader;

typedef struct _CFAImageHeader
{
	CAM_Int8s  cfaID[4];            // must be equal to 'CFA '
	CAM_Int32s version;             // currently is 1
	CAM_Int32s uCFABlockWidth;      // # of CFA blocks [2x2pixels] horizontally
	CAM_Int32s uCFABlockHeight;     // # of CFA blocks [2x2pixels] vertically
	CAM_Int8u  phase;               // 0:GBRG, 1:BGGR, 2:RGGB, 3:GRBG
	CAM_Int8u  precision;           // number of bits per sample (up to 15)
	CAM_Int8u  padding[110];        // padding for 128 bytes header
} CAM_CFAImageHeader;

typedef struct _DXRImage
{
	CAM_DXRImageHeader stDXRHeader;
	CAM_Int32u         uiChannelSize[8];
	void*              pChannel[8];
} CAM_DXRImage;

typedef struct _CFAImage
{
	CAM_CFAImageHeader stCFAHeader;
	CAM_Int16u         *pusImage;
} CAM_CFAImage;

typedef struct _PYConfig
{
	char      sModuleID[128];
	float     fAperture;
	int       iPrecision;
	int       iBayer;
} CAM_PYConfig;

static CAM_Error UnPackRawImage( CAM_Int16u *pDstImg, CAM_Int8u *pSrcImg, CAM_Int32s uiPixNum, CAM_Int32u usPrecision )
{
	CAM_Int32s i, index, uiBitShift;
	CAM_Int16u d0, d4;

	for ( index = 0; index < uiPixNum; index += 4 )
	{
		for ( i = 0; i < 4; i++ )
		{
			d0 = ((CAM_Int16u) pSrcImg[i]) << 2;
			d4 = ((CAM_Int16u) pSrcImg[4]) >> (i * 2);
			d4 &= 0x3;
			pDstImg[i] = d0 | d4;
		}
		pSrcImg += 5;
		pDstImg += 4;
	}

	if ( usPrecision > 10 )
	{
		uiBitShift = usPrecision - 10;

		for ( i = 0; i < uiPixNum; i++ )
		{
			pDstImg[i] = pDstImg[i] >> uiBitShift;
		}
	}

	return CAM_ERROR_NONE;
}


static CAM_Error SetCFAImageHeader( CAM_CFAImageHeader *pstCFAHeader, int iWidth, int iHeight, int iPrecision, int iBayer )
{
	int i;

	pstCFAHeader->cfaID[0]        = 'C';
	pstCFAHeader->cfaID[1]        = 'F';
	pstCFAHeader->cfaID[2]        = 'A';
	pstCFAHeader->cfaID[3]        = ' ';

	pstCFAHeader->version         = 1;
	pstCFAHeader->uCFABlockWidth  = iWidth;
	pstCFAHeader->uCFABlockHeight = iHeight;
	if ( (iBayer < 0) || (iBayer > 3) )
	{
		DBG_LOG( "ERROR: unsupported bayer type!\n" );
		return CAM_ERROR_BADARGUMENT;
	}
	else
	{
		pstCFAHeader->phase = iBayer;
	}

	pstCFAHeader->precision = iPrecision;
	for( i = 0;i < 110; i++ )
	{
		pstCFAHeader->padding[i] = 0;
	}

	return CAM_ERROR_NONE;
}

static CAM_Error AllocAndSetCFAImage( CAM_CFAImage *pstCFA, CAM_Int8u *pSrcImage, int iWidth, int iHeight, int iPrecision, int iBayer )
{
	CAM_Error error;

	SetCFAImageHeader( &pstCFA->stCFAHeader, iWidth, iHeight, iPrecision, iBayer );

	pstCFA->pusImage = (CAM_Int16u *)malloc( iWidth*iHeight*4*sizeof(CAM_Int16u));
	if ( NULL == pstCFA->pusImage )
	{
		DBG_LOG( "Failed to malloc CFA image buffer ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_OUTOFMEMORY;
	}

	error = UnPackRawImage( pstCFA->pusImage, pSrcImage, iWidth*iHeight*4, iPrecision );

	return error;
}

void FreeCFAImage( CAM_CFAImage *pstCFA )
{
	if ( pstCFA->pusImage )
	{
		free( pstCFA->pusImage );
	}

	return ;
}

static CAM_Error AllocAndSetDXRImage( CAM_DXRImage *pstDXR, CAM_Int8u *pucPix, int iWidth, int iHeight,
                               DxOFormat eFormat, CAM_Int8u eOrder, DxOEncoding eEncoding, DxOColorSpace ucColorSpace )
{
	int i, j, k;
	CAM_Int8u        ucOffset;
	char           ucTypeyuv420_Video[16] = {"YUV420601"};
	char           ucTypeyuv420_Still[16] = {"YUV420JPG"};
	char           ucTypeyuv422_Video[16] = {"YUV422601"};
	char           ucTypeyuv422_Still[16] = {"YUV422JPG"};

	CAM_Int32u uiIndex422_Y [4];
	CAM_Int32u uiIndex422_UV[4];
	CAM_Int32u uiIndex420_Y [4];
	CAM_Int32u uiIndex420_UV[3];
	CAM_Int32u uiChannelSize[8];

	strcpy( pstDXR->stDXRHeader.ucFourCC, "DXR " );
	memset( pstDXR->uiChannelSize, 0, sizeof(pstDXR->uiChannelSize) );
	memset( pstDXR->stDXRHeader.ucType, 0, sizeof(pstDXR->stDXRHeader.ucType) );
	memset( pstDXR->stDXRHeader.ucPadding, 0, sizeof(pstDXR->stDXRHeader.ucPadding) );

	switch ( eFormat )
	{
	case FORMAT_YUV422:
		{
			memcpy( uiIndex422_Y , S_uiIndex422_Y[eOrder] , sizeof(S_uiIndex422_Y[0]) );
			memcpy( uiIndex422_UV, S_uiIndex422_UV[eOrder], sizeof(S_uiIndex422_UV[0]) );
			uiIndex422_Y[2]  += 4*iWidth;
			uiIndex422_Y[3]  += 4*iWidth;
			uiIndex422_UV[1] += 4*iWidth;
			uiIndex422_UV[3] += 4*iWidth;
			if( ucColorSpace == E_COLOR_SPACE_VIDEO )
			{
				memcpy( pstDXR->stDXRHeader.ucType, ucTypeyuv422_Video, 9 );
			}
			else if( ucColorSpace == E_COLOR_SPACE_STILL )
			{
				memcpy( pstDXR->stDXRHeader.ucType, ucTypeyuv422_Still, 9 );
			}
			pstDXR->stDXRHeader.uiChannels   = 8;
			pstDXR->stDXRHeader.uiPrecision  = 8;
			pstDXR->stDXRHeader.uiWidth      = iWidth;
			pstDXR->stDXRHeader.uiHeight     = iHeight;

			for ( i = 0; i < pstDXR->stDXRHeader.uiChannels; i++ )
			{
				pstDXR->uiChannelSize[i] = iWidth * iHeight;
			}
		}
		break;

	case FORMAT_YUV420:
		{
			memcpy( uiIndex420_Y, S_uiIndex420_Y[eOrder], sizeof(S_uiIndex420_Y[0]) );
			memcpy( uiIndex420_UV, S_uiIndex420_UV[eOrder], sizeof(S_uiIndex420_UV[0]) );
			uiIndex420_Y [2] += 3*iWidth;
			uiIndex420_Y [3] += 3*iWidth;
			uiIndex420_UV[uiIndex420_UV[2]] += 3*iWidth;
			if( ucColorSpace == E_COLOR_SPACE_VIDEO )
			{
				memcpy( pstDXR->stDXRHeader.ucType, ucTypeyuv420_Video, 9 );
			}
			else if( ucColorSpace == E_COLOR_SPACE_STILL )
			{
				memcpy( pstDXR->stDXRHeader.ucType, ucTypeyuv420_Still, 9 );
			}
			pstDXR->stDXRHeader.uiChannels   = 6;
			pstDXR->stDXRHeader.uiPrecision  = 8;
			pstDXR->stDXRHeader.uiWidth      = iWidth;
			pstDXR->stDXRHeader.uiHeight     = iHeight;

			for ( i = 0; i < pstDXR->stDXRHeader.uiChannels; i++ )
			{
				pstDXR->uiChannelSize[i] = iWidth * iHeight;
			}
		}
		break;

	case FORMAT_RAW:
		{
			memcpy( pstDXR->stDXRHeader.ucType, "Bayer3", 6 );
			pstDXR->stDXRHeader.uiChannels   = 4;
			pstDXR->stDXRHeader.uiPrecision  = 10;
			pstDXR->stDXRHeader.uiWidth      = iWidth;
			pstDXR->stDXRHeader.uiHeight     = iHeight;

			for ( i = 0; i < pstDXR->stDXRHeader.uiChannels; i++ )
			{
				pstDXR->uiChannelSize[i] = iWidth * iHeight * sizeof(CAM_Int16u);
			}
		}
		break;

	case FORMAT_RGB888:
	case FORMAT_RGB565:
		{
			memcpy( pstDXR->stDXRHeader.ucType, "RGB", 3 );
			pstDXR->stDXRHeader.uiChannels   = 3;
			pstDXR->stDXRHeader.uiPrecision  = 8;
			pstDXR->stDXRHeader.uiWidth      = iWidth * 2;
			pstDXR->stDXRHeader.uiHeight     = iHeight* 2;

			for ( i = 0; i < pstDXR->stDXRHeader.uiChannels; i++ )
			{
				pstDXR->uiChannelSize[i] = iWidth * iHeight * 4;
			}
		}
		break;

	default:
		DBG_LOG( "Unknown format[%d] ( %s, %s, %d )\n", eFormat, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	pstDXR->stDXRHeader.uiIsSigned   = 0;
	pstDXR->stDXRHeader.uiExtensions = 0;

	switch( eEncoding )
	{
		case ENCODING_YUV_601FU:
		case ENCODING_YUV_601U:
			ucOffset = 0;
			break;

		case ENCODING_YUV_601FS:
		case ENCODING_YUV_601S:
			ucOffset = 128;
			break;

		default:
			ucOffset = 0;
			break;
	}

	for ( i = 0; i < pstDXR->stDXRHeader.uiChannels; i++ )
	{
		pstDXR->pChannel[i] = (void*)malloc( pstDXR->uiChannelSize[i] );
		if ( NULL == pstDXR->pChannel[i] )
		{
			DBG_LOG( "Failed to malloc buffer for DXR channel[%d] ( %s, %s, %d )\n", i, __FILE__, __FUNCTION__, __LINE__ );
			for ( j = 0; j < i; j++ )
			{
				free( pstDXR->pChannel[j] );
				pstDXR->pChannel[j] = NULL;
			}
			return CAM_ERROR_OUTOFMEMORY;
		}
	}

	switch( eFormat )
	{
	case FORMAT_YUV420:
		{
			/* Y Plane */
			for( k = 0; k < 4; k++ )
			{
				const CAM_Int8u* pTmp  = pucPix + uiIndex420_Y[k];
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k];
				CAM_Int16u usBayerW  = 3;
				int        iPixIndex = 0;

				for( j = 0; j < iHeight; j++ )
				{
					for( i = 0; i < iWidth ; i++ )
					{
						pDst[iPixIndex++] = *pTmp;
						pTmp += usBayerW;
					}
					pTmp += iWidth*usBayerW;
				}
			}

			/* U/V Plane */
			for( k = 0; k < 2; k++ )
			{
				const CAM_Int8u* pTmp = pucPix + uiIndex420_UV[k];
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k+4];
				int   iPixIndex = 0;

				for( j = 0; j < iHeight; j++ )
				{
					for( i = 0; i < iWidth; i++ )
					{
						CAM_Int8u ucPixVal = (CAM_Int8u)(*pTmp + ucOffset);
						pDst[iPixIndex++] = ucPixVal;
						pTmp += 3;
					}
					pTmp += 3 * iWidth;
				}
			}
		}
		break;

	case FORMAT_YUV422:
		{
			/* Y Plane */
			for( k = 0; k < 4; k++ )
			{
				const CAM_Int8u* pTmp = pucPix + uiIndex422_Y[k];
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k];
				CAM_Int16u usBayerW   = 4;
				int        iPixIndex = 0;

				for( j = 0; j < iHeight; j++ )
				{
					for( i = 0; i < iWidth; i++ )
					{
						pDst[iPixIndex++] = *pTmp;
						pTmp += usBayerW;
					}
					pTmp += iWidth*usBayerW;
				}
			}

			/* U/V Plane */
			for( k = 0; k < 4; k++ )
			{
				const CAM_Int8u* pTmp = pucPix + uiIndex422_UV[k];
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k+4];
				int            iPixIndex = 0;

				for( j = 0; j < iHeight; j++ )
				{
					for( i = 0; i < iWidth; i++ )
					{
						CAM_Int8u ucPixVal = (CAM_Int8u)(*pTmp + ucOffset);
						pDst[iPixIndex++] = ucPixVal;
						pTmp += 4;
					}
					pTmp += iWidth*4;
				}
			}
		}
		break;

	case FORMAT_RAW:
		{
			for( k = 0; k < 4; k++ )
			{
				const CAM_Int16u* pTmp;
				CAM_Int32u Index[4] = {0, 1, 2*iWidth, 2*iWidth+1};
				CAM_Int16u *pDst = (CAM_Int16u*)pstDXR->pChannel[k];
				int iBayerIdx = 0;

				pTmp = (CAM_Int16u *)( pucPix + Index[k]*sizeof(CAM_Int16u) );
				for( j = 0; j < iHeight; j++ )
				{
					for( i = 0; i < iWidth; i++ )
					{
						pDst[iBayerIdx++] = *pTmp;
						pTmp += 2;
					}
					pTmp += iWidth*2;
				}
			}
		}
		break;

	case FORMAT_RGB888:
		{
			for( k = 0;k < 3; k++ )
			{
				const CAM_Int8u* pTmp;
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k];
				int   iPixIndex = 0;

				pTmp = pucPix + S_ucIndexRGB[eOrder][k];
				for( j = 0; j < iHeight*2; j++ )
				{
					for( i = 0; i < iWidth*2; i++ )
					{
						pDst[iPixIndex++] = *pTmp;
						pTmp += 3;
					}
				}
			}
		}
		break;

	case FORMAT_RGB565:
		{
			for( k = 0; k < 3; k++ )
			{
				const CAM_Int16u* pusTmp = (CAM_Int16u *)pucPix;
				CAM_Int8u *pDst = (CAM_Int8u*)pstDXR->pChannel[k];
				int   iPixIndex = 0;

				for( j = 0; j < iHeight*2; j++ )
				{
					for( i = 0; i< iWidth*2; i++ )
					{
						CAM_Int8u ucPix = 0;

						if( k == S_ucIndexRGB[eOrder][0] )
						{
							ucPix = (CAM_Int8u)((*pusTmp)&0x001F);
						}
						else if( k == S_ucIndexRGB[eOrder][1] )
						{
							ucPix = (CAM_Int8u)(((*pusTmp)&0x07E0)>>5);
						}
						else if( k == S_ucIndexRGB[eOrder][2] )
						{
							ucPix = (CAM_Int8u)(((*pusTmp)&0xF800)>>11);
						}

						pDst[iPixIndex++] = ucPix;
						pusTmp++;
					}
				}
			}
		}
		break;

	default:
		DBG_LOG( "unknown format[%d] ( %s, %s, %d )\n", eFormat, __FILE__, __FUNCTION__, __LINE__ );
		return CAM_ERROR_BADARGUMENT;
	}

	return CAM_ERROR_NONE;
}

void FreeDXRImage( CAM_DXRImage *pstDXRImage )
{
	int i;

	for ( i = 0; i < pstDXRImage->stDXRHeader.uiChannels; i++ )
	{
		if ( pstDXRImage->pChannel[i] )
		{
			free( pstDXRImage->pChannel[i] );
		}
	}

	return ;
}

static void GeneratePYFile( char* sFileName, CAM_PYConfig *pstPYConfig, int iWidth, int iHeight, float fAGain, float fDGain, float fExpTime )
{
	FILE *fp = NULL;
	// output raw PY file
	if ( ( fp = fopen( sFileName,"wb" ) ) == NULL )
	{
		DBG_LOG( "Cannot open output file: %s ( %s, %s, %d )\n",sFileName, __FILE__, __FUNCTION__, __LINE__ );
		return ;
	}

	fprintf( fp, "params = {'sensor' : {'afPosition': 0.000000000000000000,\n" );
	fprintf( fp, "                      'borders'   : {'x': 0.000000000000000000,\n" );
	fprintf( fp, "                                     'y': 0.000000000000000000\n" );
	fprintf( fp, "                                    },\n" );
	fprintf( fp, "                      'exposure'  : {'aperture': %.18f,\n",pstPYConfig->fAperture );
	fprintf( fp, "                                     'gain'    : {'analog' : {'b' : %.18f,\n", fAGain );
	fprintf( fp, "                                                              'gb': %.18f,\n", fAGain );
	fprintf( fp, "                                                              'gr': %.18f,\n", fAGain );
	fprintf( fp, "                                                              'r' : %.18f\n", fAGain );
	fprintf( fp, "                                                             },\n" );
	fprintf( fp, "                                                  'digital': {'b' : %.18f,\n", fDGain );
	fprintf( fp, "                                                              'gb': %.18f,\n", fDGain );
	fprintf( fp, "                                                              'gr': %.18f,\n", fDGain );
	fprintf( fp, "                                                              'r' : %.18f\n", fDGain );
	fprintf( fp, "                                                             },\n" );
	fprintf( fp, "                                                  'isp'    : 1.000000000000000000\n" );
	fprintf( fp, "                                                 },\n" );
	fprintf( fp, "                                     'time'    : %.18f\n", fExpTime );
	fprintf( fp, "                                    },\n" );
	fprintf( fp, "                      'flashPower': 0.000000000000000000,\n" );
	fprintf( fp, "                      'mode'      : 'E_CC_FRAME_MODE_STILL',\n" );
	fprintf( fp, "                      'moduleID'  : '%s',\n",pstPYConfig->sModuleID );
	fprintf( fp, "                      'pix'       : {'bitdepth': %d,\n", pstPYConfig->iPrecision );
	fprintf( fp, "                                     'hMirror' : 0,\n" );
	switch( pstPYConfig->iBayer ) // 0: Gb-B-R-Gr; 1:B-Gb-Gr-R; 2:R-Gr-Gb-B; 3:Gr-R-B-Gb
	{
	case 0:
		fprintf( fp, "                                     'order'   : 'E_BAYER_PHASE_GBBRGR',\n" );
		break;

	case 1:
		fprintf( fp, "                                     'order'   : 'E_BAYER_PHASE_BGBGRR',\n" );
		break;

	case 2:
		fprintf( fp, "                                     'order'   : 'E_BAYER_PHASE_RGRGBB',\n" );
		break;

	case 3:
		fprintf( fp, "                                     'order'   : 'E_BAYER_PHASE_GRRBGB',\n" );
		break;

	default:
		DBG_LOG( "ERROR: unsupported bayer type! ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return ;
	}
	fprintf( fp, "                                     'vFlip'   : 0,\n" );
	fprintf( fp, "                                     'xBin'    : 1,\n" );
	fprintf( fp, "                                     'xEnd'    : %d,\n", (iWidth/2 -1));
	fprintf( fp, "                                     'xMaxSize': %d,\n", iWidth/2);
	fprintf( fp, "                                     'xSkip'   : 1,\n" );
	fprintf( fp, "                                     'xStart'  : 0,\n" );
	fprintf( fp, "                                     'yBin'    : 1,\n" );
	fprintf( fp, "                                     'yEnd'    : %d,\n", (iHeight/2 -1));
	fprintf( fp, "                                     'yMaxSize': %d,\n", iHeight/2);
	fprintf( fp, "                                     'ySkip'   : 1,\n" );
	fprintf( fp, "                                     'yStart'  : 0\n" );
	fprintf( fp, "                                    },\n" );
	fprintf( fp, "                      'validTime' : 0.000000000000000000,\n" );
	fprintf( fp, "                      'wbScale'   : {'b': 1.000000000000000000,\n" );
	fprintf( fp, "                                     'r': 1.000000000000000000\n" );
	fprintf( fp, "                                    },\n" );
	fprintf( fp, "                      'zoomFocal' : 0.000000000000000000\n" );
	fprintf( fp, "                     }\n" );
	fprintf( fp, "         }\n" );

	fclose( fp );

}

static void LoadDxOConfig( CAM_PYConfig *pstPYConfig )
{
	FILE *fp;
	char sTmp[128];

	if ( ( fp = fopen( "DxO_config.txt","r" ) ) != NULL )
	{
		fscanf( fp, "%s %s\n", (char*)&sTmp, (char*)&pstPYConfig->sModuleID );
		fscanf( fp, "%s %f\n", (char*)&sTmp, &pstPYConfig->fAperture );
		fscanf( fp, "%s %d\n", (char*)&sTmp, &pstPYConfig->iPrecision );
		fscanf( fp, "%s %d\n", (char*)&sTmp, &pstPYConfig->iBayer );

		fclose( fp );
	}
	else
	{
		strcpy( pstPYConfig->sModuleID, "OV8820_Mxx" );
		pstPYConfig->fAperture  = 2.4f;
		pstPYConfig->iPrecision = 10;
		pstPYConfig->iBayer     = 1;
	}
}


CAM_Error GenerateDxOFiles( CAM_ImageBuffer *pImgBuf, DxOFormat eFormat, CAM_Int8u eOrder,
                            DxOEncoding eEncoding, DxOColorSpace ucColorSpace, char *sFileNamePrefix )
{
	CAM_Int32s   uiPixNum = pImgBuf->iWidth * pImgBuf->iHeight;
	CAM_DXRImage stDXR;
	CAM_CFAImage stCFA;
	char         sRawFileName[256] = {0};
	CAM_PYConfig stPYConfig;
	CAM_Error    error;
	FILE         *fp = NULL;
	CAM_Int32s   i;

	memset( &stDXR, 0, sizeof(CAM_DXRImage) );
	memset( &stCFA, 0, sizeof(CAM_CFAImage) );

	LoadDxOConfig( &stPYConfig );

	strcpy( sRawFileName, sFileNamePrefix );
	strcat( sRawFileName, "py" );
	GeneratePYFile( sRawFileName, &stPYConfig, pImgBuf->iWidth, pImgBuf->iHeight,
	                pImgBuf->stRawShotInfo.usAGain[0]/256.f, pImgBuf->stRawShotInfo.usDGain[0]/256.f, pImgBuf->stShotInfo.iExposureTimeQ16/65536.f );

	error = AllocAndSetCFAImage( &stCFA, pImgBuf->pBuffer[0], pImgBuf->iWidth/2, pImgBuf->iHeight/2, stPYConfig.iPrecision, stPYConfig.iBayer );
	if ( error != CAM_ERROR_NONE )
	{
		DBG_LOG( "Failed to generate CFA image ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

	error = AllocAndSetDXRImage( &stDXR, (CAM_Int8u*)stCFA.pusImage, pImgBuf->iWidth/2, pImgBuf->iHeight/2,
	                             eFormat, eOrder, eEncoding, ucColorSpace );
	if ( error != CAM_ERROR_NONE )
	{
		DBG_LOG( "Failed to generate CFA image ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		goto exit;
	}

#if 0
	// According to Ping-Sing, RAW file is not necessary
	strcpy( sRawFileName, sFileNamePrefix );
	strcat( sRawFileName, "raw" );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		DBG_LOG( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_FATALERROR;
		goto exit;
	}
	fwrite( stCFA.pusImage, sizeof( CAM_Int16u ), uiPixNum, fp );
	fclose( fp );
#endif

	strcpy( sRawFileName, sFileNamePrefix );
	strcat( sRawFileName, "cfa" );
	DBG_LOG( "Start store %s\n", sRawFileName );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		DBG_LOG( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_FATALERROR;
		goto exit;
	}
	fwrite( &stCFA.stCFAHeader, sizeof( CAM_CFAImageHeader ), 1, fp );
	fwrite( stCFA.pusImage, sizeof( CAM_Int16u ), uiPixNum, fp );
	fclose( fp );
	DBG_LOG( "Save %s success\n", sRawFileName );

	strcpy( sRawFileName, sFileNamePrefix );
	strcat( sRawFileName, "dxr" );
	DBG_LOG( "Start store %s\n", sRawFileName );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		DBG_LOG( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		error = CAM_ERROR_FATALERROR;
		goto exit;
	}
	fwrite( &stDXR.stDXRHeader, sizeof( CAM_DXRImageHeader ), 1, fp );
	for ( i = 0; i < stDXR.stDXRHeader.uiChannels; i++ )
	{
		fwrite( stDXR.pChannel[i], sizeof( CAM_Int8u ), stDXR.uiChannelSize[i], fp );
	}
	fclose( fp );
	DBG_LOG( "Save %s success\n", sRawFileName );

exit:
	FreeDXRImage( &stDXR );
	FreeCFAImage( &stCFA );

	return error;
}
