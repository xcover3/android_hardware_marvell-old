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
#include <string.h>
#include <stdio.h>

int main( int argc, char **argv )
{
	int iWidth = 3264, iHeight = 2448;
	CAM_Int32s   uiPixNum;
	CAM_DXRImage stDXR;
	CAM_CFAImage stCFA;
	char         sRawFileName[256] = {0};
	char         *sFileName = "test_image";
	CAM_PYConfig stPYConfig;
	float        fAGain = 2.3f, fDGain = 2.3f, fExpTime = 2.3f;
	CAM_Int8u    *pImgBuf = NULL;

	if ( argc != 6 )
	{
		printf( "Usage:%s: width height AGain DGain ExpTime\n", argv[0] );
		return -1;
	}

	iWdith = atoi( argv[1] );
	iHeight= atoi( argv[2] );
	fAGain = atof( argv[3] );
	fDGain = atof( argv[4] );
	fExpTime=atof( argv[5] );
	uiPixNum = iWdith * iHeight;

	memset( &stDXR, 0, sizeof(CAM_DXRImage) );
	memset( &stCFA, 0, sizeof(CAM_CFAImage) );

	pImgBuf = (CAM_Int8u *)malloc( iWidth*2*iHeight*sizeof(CAM_Int8u) );
	if ( pImgBuf == NULL )
	{
		printf( "Failed to malloc buffer! ( %s, %s, %d )\n", __FILE__, __FUNCTION__, __LINE__ );
		return -1;
	}

	strcpy( stPYConfig.sModuleID, "OV8820_Mxx" );
	stPYConfig.fAperture  = 2.4f;
	stPYConfig.iPrecision = 10;
	stPYConfig.iBayer     = 1;

	strcpy( sRawFileName, sFileName );
	strcat( sRawFileName, "py" );
	GeneratePYFile( sRawFileName, &stPYConfig, iWidth, iHeight,
	                fAGain, fDGain, fExpTime );

	error = AllocAndSetCFAImage( &stCFA, pImgBuf, iWidth/2, iHeight/2, stPYConfig.iPrecision, stPYConfig.iBayer );
	ASSERT_CAM_ERROR( error );

	error = AllocAndSetDXRImage( &stDXR, (CAM_Int8u*)stCFA.pusImage, iWidth/2, iHeight/2
	                           , FORMAT_RAW, 0, ENCODING_YUV_601FS, E_COLOR_SPACE_STILL );
	ASSERT_CAM_ERROR( error );

	strcpy( sRawFileName, sFileName );
	strcat( sRawFileName, "raw" );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		TRACE( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		exit( -1 );
	}
	fwrite( stCFA.pusImage, sizeof( CAM_Int16u ), uiPixNum, fp );
	fclose( fp );

	strcpy( sRawFileName, sFileName );
	strcat( sRawFileName, "cfa" );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		TRACE( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		exit( -1 );
	}
	fwrite( &stCFA.stCFAHeader, sizeof( CAM_CFAImageHeader ), 1, fp );
	fwrite( stCFA.pusImage, sizeof( CAM_Int16u ), uiPixNum, fp );
	fclose( fp );

	strcpy( sRawFileName, sFileName );
	strcat( sRawFileName, "dxr" );
	fp = fopen( sRawFileName, "wb" );
	if ( NULL == fp )
	{
		TRACE( "Failed to open file:%s ( %s, %s, %d )\n", sRawFileName, __FILE__, __FUNCTION__, __LINE__ );
		exit( -1 );
	}
	fwrite( &stDXR.stDXRHeader, sizeof( CAM_DXRImageHeader ), 1, fp );
	for ( i = 0; i < stDXR.stDXRHeader.uiChannels; i++ )
	{
		fwrite( stDXR.pChannel[i], sizeof( CAM_Int8u ), stDXR.uiChannelSize[i], fp );
	}
	fclose( fp );

	FreeDXRImage( &stDXR );
	FreeCFAImage( &stCFA );

	return 0;
}