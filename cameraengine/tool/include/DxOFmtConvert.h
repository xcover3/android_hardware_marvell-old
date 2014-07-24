/*******************************************************************************
//(C) Copyright [2010 - 2011] Marvell International Ltd.
//All Rights Reserved
*******************************************************************************/
// ============================================================================
// DxO Labs proprietary and confidential information
// Copyright (C) DxO Labs 1999-2011 - (All rights reserved)
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

#include "CameraEngine.h"

typedef enum {
	ENCODING_YUV_601FS = 0,
	ENCODING_YUV_601FU,
	ENCODING_YUV_601U,
	ENCODING_YUV_601S,
	NB_ENCODING
} DxOEncoding;

typedef enum {
	FORMAT_YUV422 = 0,
	FORMAT_YUV420,
	FORMAT_RGB565,
	FORMAT_RGB888,
	FORMAT_RAW,
	NB_FORMAT
} DxOFormat;

typedef enum {
	RGBORDER_RGB,
	RGBORDER_RBG,
	RGBORDER_GRB,
	RGBORDER_GBR,
	RGBORDER_BGR,
	RGBORDER_BRG,
	NB_RGBORDER
} DxORgbOrder;

typedef enum {
	YUV422ORDER_YUYV,
	YUV422ORDER_YYUV,
	YUV422ORDER_UYYV,
	YUV422ORDER_UVYY,
	YUV422ORDER_UYVY,
	YUV422ORDER_YUVY,
	YUV422ORDER_YVYU,
	YUV422ORDER_YYVU,
	YUV422ORDER_VYYU,
	YUV422ORDER_VUYY,
	YUV422ORDER_VYUY,
	YUV422ORDER_YVUY,
	NB_YUV422ORDER
} DxOYuv422Order;

typedef enum {
	YUV420ORDER_YYU,
	YUV420ORDER_YUY,
	YUV420ORDER_UYY,
	NB_YUV420ORDER
} DxOYuv420Order;

typedef enum {
	E_COLOR_SPACE_VIDEO,
	E_COLOR_SPACE_STILL
} DxOColorSpace;

CAM_Error GenerateDxOFiles( CAM_ImageBuffer *pImgBuf, DxOFormat eFormat, CAM_Int8u eOrder,
                            DxOEncoding eEncoding, DxOColorSpace ucColorSpace, char *sFileNamePrefix );

#ifdef __cplusplus
}
#endif