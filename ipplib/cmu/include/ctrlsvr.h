/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *----------------------------------------------------------------------------*/

#ifndef _ICR_CTRLSVR_H_
#define _ICR_CTRLSVR_H_


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*************************
** Macros
**************************/


/* General */

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif



/* Range and normal values of the adjustments */

#define CONTRAST_MIN		-127
#define CONTRAST_MAX		128
#define CONTRAST_NORMAL		0			// default value for the standard mode

#define BRIGHTNESS_MIN		-127
#define BRIGHTNESS_MAX		128
#define BRIGHTNESS_NORMAL	0			// default value for the standard mode

#define SATURATION_MIN		-127
#define SATURATION_MAX		128
#define SATURATION_NORMAL	0			// default value for the standard mode

#define HUE_MIN				0
#define HUE_MAX				360
#define HUE_NORMAL			0			// default value for the standard mode

#define TONE_MIN			4000
#define TONE_MAX			10000
#define TONE_NORMAL			6500		// default value for the standard mode

#define BE_LEVEL_MIN		0
#define BE_LEVEL_MAX		14
#define BE_LEVEL_NORMAL		0			// default value for the standard mode

#define DEFAULT_ACE_MODE	ICR_ACE_OFF	// default value for the standard mode

#define DEFAULT_FLESH_TONE_ENABLE	1	// default value for the standard mode
#define DEFAULT_GAMUT_COMP_ENABLE	1	// default value for the standard mode



/* View mode */
#define ICR_VIEWMODE_NULL		-1
#define ICR_VIEWMODE_CUSTOM		0
#define ICR_VIEWMODE_STANDARD	1
#define ICR_VIEWMODE_MAX		16

/* Route */
#define ICR_ROUTE_MAX		4


/*************************
** Enumerate
**************************/

/* ICR error code */
typedef enum tagIcrError {
	ICR_ERR_OK,
	ICR_ERR_NOMEMORY,
	ICR_ERR_INVALID_PARAM,
	ICR_ERR_INVALID_STATE,
	ICR_ERR_DEVICEOP_FAILED,	// open / close, reg read/write, transform, etc
	ICR_ERR_UNEXPECTED,
} IcrError;

/*
 * Set the signal route for ICR
 */
typedef enum tagIcrRoute
{
    ICR_ROUTE_OFF,
    ICR_ROUTE_TV,
    ICR_ROUTE_LCD1,
    ICR_ROUTE_LCD2,
} IcrRoute;


/* ICR Channels
* main - the whole input picture frame
* pip - NOT SUPPORTED ON PXA168. sub-rect of the input picture frame.
*/
typedef enum tagIcrChannel {
	ICR_MAIN_CHANNEL,
	ICR_PIP_CHANNEL,
} IcrChannel;

/* Input source's content type */
// in the most case, sRGB will be used, and PAL/SECAM rgb is quite close to sRGB
//
// If full range (0~255) YUV is converted to RGB using none full range (16~235) YUV-RGB formula,
// there'll be color deviation. Such deviation can partially be corrected by ICR, but the head/foot
// range will loss. The typical case is JPEG YUV converted to RGB by GC which actually supports
// video YUV-RGB conversion instead JPEG full range YUV. On PXA168, we suggest user to decode JPEG
// to RGB directly to avoid the problem.
//
// NTSC image will appear to be "warm" and compressed gamut if display as sRGB directly.
/*
typedef enum tagIcrContentType {
	ICR_CONTENTTYPE_STANDARD = 0,	// sRGB or RGB converted from PAL/SECAM YUV (16~235) by GCxxx
	ICR_CONTENTTYPE_FULLRANGE = 1,	// RGB converted from JPEG YUV or other full range (0~255) YUV by GCxxx
	ICR_CONTENTTYPE_NTSC = 2,		// different RGB Gamut and white point
} IcrContentType;
*/

/* Input source's content type */
typedef enum tagIcrFormat {
	ICR_FORMAT_RGB888,
	ICR_FORMAT_XRGB8888,
	ICR_FORMAT_BGR888,
	ICR_FORMAT_XBGR8888,
	ICR_FORMAT_YUV444,
} IcrFormat;

/*
 * These modes are for setting the
 * adaptive contrast enhancement (ACE) in different levels. (Low to High)
 */
typedef enum tagIcrAceMode
{
    ICR_ACE_OFF = 0,
    ICR_ACE_LOW,
    ICR_ACE_MEDIUM,
    ICR_ACE_HIGH,
} IcrAceMode;


/*************************
** Structures
**************************/

/* Input/Output picture definition */
typedef struct tagIcrBitmapInfo {
	int iWidth;
	int iHeight;
	IcrFormat iFormat;
} IcrBitmapInfo;

/* structure for mode definition */
typedef struct _IcrViewModeParam {
	int iCon;
	int iBri;
	int iSat;
	int iHue;
	int iTone;

	int iBeLevel;
	IcrAceMode eAceMode;

	int bFleshToneEnabled;
	int bGamutCompressEnabled;

	int iLocalAxis[14];
	int iLocalHue[14];
	int iLocalSat[14];

	int iOutCsc[3][4];
} IcrViewModeParam;


/*************************
** Color control server handle
**************************/

/* Handle of the color control server */
typedef void IcrSession;

/* Token */
typedef long long IcrToken;

/*************************
** Functions
**************************/

extern IcrError IcrOpen(
	OUT	IcrSession **ppIcrSession);

extern IcrError IcrClose(
	IN	IcrSession* Session);

extern IcrError IcrRouteSet(
	IN	IcrSession* Session,
	IN	IcrRoute eRoute);

extern IcrError IcrRouteGet(
	IN	IcrSession* Session,
	OUT	IcrRoute *pRoute);

extern IcrError IcrMainGet(
	IN	IcrSession* pSession,
	OUT int *width,
	OUT int *height);

extern IcrError IcrPipSet(
	IN	IcrSession* pSession,
	IN	int left,
	IN	int top,
	IN	int right,
	IN	int bottom);

extern IcrError IcrPipGet(
	IN	IcrSession* pSession,
	OUT int *left,
	OUT int *top,
	OUT int *right,
	OUT int *bottom);

extern IcrError IcrLetterBoxSet(
	IN	IcrSession* pSession,
	IN	int left,
	IN	int top,
	IN	int right,
	IN	int bottom);

extern IcrError IcrLetterBoxGet(
	IN	IcrSession* pSession,
	OUT int *left,
	OUT int *top,
	OUT int *right,
	OUT int *bottom);

extern IcrError IcrViewModeRegister(
	IN	IcrSession* pSession,
	IN	int iModeIndex,
	IN	const IcrViewModeParam *pViewMode);

extern IcrError IcrFormatGet(
	IN	IcrSession* pSession,
	IN  IcrFormat *pInFormat,
	IN  IcrFormat *pOutFormat);

extern IcrError IcrFormatSet(
	IN	IcrSession* pSession,
	IN  IcrFormat eInFormat,
	IN  IcrFormat eOutFormat);

extern IcrError IcrViewModeGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int *pModeIndex);

extern IcrError IcrViewModeSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iModeIndex);

extern IcrError IcrFleshToneEnable(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int bEnabled);

extern IcrError IcrFleshToneGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pEnabled);

extern IcrError IcrGamutCompressEnable(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int bEnabled);

extern IcrError IcrGamutCompressGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pEnabled);

extern IcrError IcrBrightnessGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pBrightness);

extern IcrError IcrBrightnessSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iBrightness);

extern IcrError IcrContrastGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pContrast);

extern IcrError IcrContrastSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iContrast);

extern IcrError IcrBeLevelGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pBeLevel);

extern IcrError IcrBeLevelSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iBeLevel);

extern IcrError IcrAceModeGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	IcrAceMode* pAceMode);

extern IcrError IcrAceModeSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	IcrAceMode eAceMode);

extern IcrError IcrHueGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pHue);

extern IcrError IcrHueSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iHue);

extern IcrError IcrSaturationGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pSaturation);

extern IcrError IcrSaturationSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iSaturation);

extern IcrError IcrToneGet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	OUT	int* pTone);

extern IcrError IcrToneSet(
	IN	IcrSession* pSession,
	IN	IcrChannel eChannel,
	IN	int iTone);

#if defined(PLATFORM_PXA168) || defined(WIN32)

extern IcrError IcrTransform(
	IN		IcrSession* pSession,
	IN		void* pSrcData,				// source bitmap data
	IN		IcrBitmapInfo* pSrcInfo,	// source bitmap data
	INOUT	void* pDstData,				// dest bitmap data
	IN		IcrBitmapInfo* pDstInfo,
	IN		int iDstOffsetX,
	IN		int iDstOffsetY,
	OUT		IcrToken *pToken);

extern IcrError IcrWaitTransform(
	IN		IcrSession* pSession,
	IN		IcrToken iToken);

#if defined(PLATFORM_PXA168) && defined(__linux__)

extern void * IcrBufAlloc(
	IcrSession* pSession,
	unsigned int iSize,
	unsigned int iFlag);

extern void IcrBufFree(
	IcrSession* pSession,
	void * addr);

#endif // defined(PLATFORM_PXA168) && defined(__linux__)

#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _ICR_CTRLSVR_H_
