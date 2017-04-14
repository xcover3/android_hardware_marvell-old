/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <math.h>

#include "ctrlsvr.h"

#include "utility.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define BMP_BM			1, 1
#define BMP_FSIZE		2, 2
#define BMP_RESERVE1	4, 2
#define BMP_DATAOFFSET	6, 2
#define BMP_INFOSIZE	8, 2
#define BMP_WIDTH		10, 2
#define BMP_HEIGHT		12, 2
#define BMP_BITPLANE	14, 1

#define BMP_BPP			15, 1
#define BMP_COMPRESS	16, 2
#define BMP_DATASIZE	18, 2
#define BMP_HRES		20, 2
#define BMP_VRES		22, 2
#define BMP_INDEXNUM	24, 2
#define BMP_SIGINDEX	26, 2

unsigned int GET_ITEM(BYTE *data, int offset, int byte)
{
	if (byte == 1)
		return (unsigned short)((data[(offset-1)*2+1]<<8)|data[(offset-1)*2]);

	return (unsigned int)((data[(offset-1)*2+3]<<24)|(data[(offset-1)*2+2]<<16)|(data[(offset-1)*2+1]<<8)|data[(offset-1)*2]);
}

void SET_ITEM(BYTE *data, int offset, int byte, unsigned int val)
{
	if (byte == 1)
	{
		data[(offset-1)*2+0] = (val) & 0xff;
		data[(offset-1)*2+1] = ((val) >> 8) & 0xff;
	}
	else if (byte == 2)
	{
		data[(offset-1)*2+0] = (val) & 0xff;
		data[(offset-1)*2+1] = ((val) >> 8) & 0xff;
		data[(offset-1)*2+2] = ((val) >> 16) & 0xff;
		data[(offset-1)*2+3] = ((val) >> 24) & 0xff;
	}
}

typedef BYTE BitmapHeader[54];

#if defined(WM) || defined (WINCE)
int IcrGetBitmapInfo(const wchar_t *filename, int *pWidth, int *pHeight, int *pBpp, int *bIndexed)
#else
int IcrGetBitmapInfo(const char *filename, int *pWidth, int *pHeight, int *pBpp, int *bIndexed)
#endif
{
	FILE *f;
	BitmapHeader bmpHeader;
	unsigned int t;

#if defined(WM) || defined (WINCE)
	f = _wfopen(filename, L"rb");
#else
	f = fopen(filename, "rb");
#endif
	ICR_CHECK(f != NULL, "Failed to open the image file to read!\n");

	fread(bmpHeader, sizeof(bmpHeader), 1, f);
	fclose(f);

	*pWidth = (int)GET_ITEM(bmpHeader, BMP_WIDTH);
	*pHeight = (int)GET_ITEM(bmpHeader, BMP_HEIGHT);
	*pBpp = (int)GET_ITEM(bmpHeader, BMP_BPP);
	t = GET_ITEM(bmpHeader, BMP_INDEXNUM);
	*bIndexed = t == 0 ? 0 : 1;

	return 0;
}

#if defined(WM) || defined (WINCE)
int IcrLoadBitmap(const wchar_t *filename, BYTE *pRGB, int *pWidth, int *pHeight)
#else
int IcrLoadBitmap(const char *filename, BYTE *pRGB, int *pWidth, int *pHeight)
#endif
{
	FILE *f;
	BitmapHeader bmpHeader;
	unsigned int t;
	int i, padding;
	BYTE tmp[4];

	// read the image data
#if defined(WM) || defined (WINCE)
	f = _wfopen(filename, L"rb");
#else
	f = fopen(filename, "rb");
#endif
	ICR_CHECK(f != NULL, "Failed to open the image file to read!\n");
	fread(bmpHeader, sizeof(bmpHeader), 1, f);

	t = GET_ITEM(bmpHeader, BMP_BM);
	ICR_CHECK(t == 0x4d42, "The input file is not a bitmap file!\n");

	t = GET_ITEM(bmpHeader, BMP_BPP);
	ICR_CHECK(t == 24, "The input bitmap file is not of 24bpp!\n");

	t = GET_ITEM(bmpHeader, BMP_INDEXNUM);
	ICR_CHECK(t == 0, "The input bitmap file is not of non-indexed bitmap!\n");

	*pWidth = GET_ITEM(bmpHeader, BMP_WIDTH);
	*pHeight = GET_ITEM(bmpHeader, BMP_HEIGHT);

	padding = (4 - (*pWidth * 3) % 4) % 4;
	for (i=0 ; i< *pHeight ; i++)
	{
		ICR_CHECK(*pWidth * 3 == fread(pRGB + *pWidth * 3 * i, 1, *pWidth * 3, f), "Unsupported or corrupted image file to load!\n");
		ICR_CHECK(padding == fread(tmp, 1, padding, f), "Unsupported or corrupted image file to load!\n");
	}

	// data from *.bmp file is of BGR format, and we want to demostrate RGB instead BGR processing capability
	// so, here we need to do a BGR --> RGB conversion
	for (i=*pWidth * *pHeight*3-3 ; i>=0 ; i-=3)
	{
		BYTE tmp = pRGB[i];
		pRGB[i] = pRGB[i+2];
		pRGB[i+2] = tmp;
	}

	fclose(f);

	return 0;
}

#if defined(WM) || defined (WINCE)
int IcrStoreBitmap(const wchar_t *filename, BYTE *pRGB, int iWidth, int iHeight)
#else
int IcrStoreBitmap(const char *filename, BYTE *pRGB, int iWidth, int iHeight)
#endif
{
	BitmapHeader bmpHeader;
	FILE *f;
	int i, padding;
	BYTE tmp[4] = {0, 0, 0, 0};

#if defined(WM) || defined (WINCE)
	f = _wfopen(filename, L"wb");
#else
	f = fopen(filename, "wb");
#endif
	ICR_CHECK(f != NULL, "Failed to open the image file to write!\n");

	SET_ITEM(bmpHeader, BMP_BM, 0x4d42);
	SET_ITEM(bmpHeader, BMP_FSIZE, ((iWidth*3 + 3) & ~0x3) *iHeight+54);
	SET_ITEM(bmpHeader, BMP_RESERVE1, 0);
	SET_ITEM(bmpHeader, BMP_DATAOFFSET, 54);
	SET_ITEM(bmpHeader, BMP_INFOSIZE, 40);
	SET_ITEM(bmpHeader, BMP_WIDTH, iWidth);
	SET_ITEM(bmpHeader, BMP_HEIGHT, iHeight);
	SET_ITEM(bmpHeader, BMP_BITPLANE, 0x01);

	SET_ITEM(bmpHeader, BMP_BPP, 24);
	SET_ITEM(bmpHeader, BMP_COMPRESS, 0);
	SET_ITEM(bmpHeader, BMP_DATASIZE, 0);
	SET_ITEM(bmpHeader, BMP_HRES, 3780);
	SET_ITEM(bmpHeader, BMP_VRES, 3780);
	SET_ITEM(bmpHeader, BMP_INDEXNUM, 0);
	SET_ITEM(bmpHeader, BMP_SIGINDEX, 0);

	fwrite(bmpHeader, 1, sizeof(bmpHeader), f);

	// RGB --> BGR
	for (i=iWidth*iHeight*3-3 ; i>=0 ; i-=3)
	{
		BYTE tmp = pRGB[i];
		pRGB[i] = pRGB[i+2];
		pRGB[i+2] = tmp;
	}

	padding = (4 - (iWidth * 3) % 4) % 4;
	for (i=0 ; i<iHeight ; i++)
	{
		fwrite(pRGB + iWidth * 3 * i, 1, iWidth*3, f);
		fwrite(tmp, 1, padding, f);
	}

	// BGR --> RGB
	for (i=iWidth * iHeight * 3-3 ; i>=0 ; i-=3)
	{
		BYTE tmp = pRGB[i];
		pRGB[i] = pRGB[i+2];
		pRGB[i+2] = tmp;
	}

	fclose(f);

	return 0;
}

int IcrDisplayBitmap(BYTE *pRGB, int iWidth, int iHeight)
{
	LOG("FIXME: Display image is currently not implemented!\n");

	return -1;
}


#if defined(WM) || defined(WINCE)

#include <ddraw.h>
#include "windows.h"

#define	MAX_ICR_SURFACE			32
static LPDIRECTDRAW				g_pDD = NULL;
static LPDIRECTDRAWSURFACE		g_pSurface[MAX_ICR_SURFACE];
static void*					g_pBuffer[MAX_ICR_SURFACE];

// WM & WINCE version of buffer allocation
// allocated by ddraw only because we want a physical contiguous buffer and ddraw can provide such buffer
void * IcrBufAlloc(
	IcrSession* pSession,
	int iWidth, int iHeight, int iBytePerPixel,
	unsigned int iFlag)
{
	int i;

	HRESULT hRet;
	DDSURFACEDESC ddsd;
	DDSURFACEDESC ddSurfaceDesc;

	if (g_pDD == NULL)
	{
		hRet = DirectDrawCreate(NULL, &g_pDD, NULL);
		if (hRet != DD_OK)
		{
			LOG("\nDirectDrawCreate FAILED\n");
			return NULL;
		}

		// Get exclusive mode
		hRet = g_pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
		if (hRet != DD_OK)
		{
			LOG("\nSetCooperativeLevel FAILED\n");
			return NULL;
		}

		// init the surface array
		for (i=0 ; i<MAX_ICR_SURFACE ; i++)
			g_pSurface[i] = NULL;
	}

	// Find an empty surface holder
	for (i=0 ; i<MAX_ICR_SURFACE ; i++)
	{
		if (g_pSurface[i] == NULL) break;
	}
	if (i >= MAX_ICR_SURFACE)
	{
		LOG("\nToo many surfaces has been allocated!\n");
		return NULL;
	}

	// Create the primary surface with 1 back buffer
	memset(&ddsd, 0, sizeof(ddsd));

	// Create a offscreen bitmap.
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
	ddsd.dwWidth = iWidth;
	ddsd.dwHeight = iHeight;
	ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
	ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = iBytePerPixel * 8;
	ddsd.ddpfPixelFormat.dwRBitMask = 0xFF0000;
	ddsd.ddpfPixelFormat.dwGBitMask = 0x00FF00;
	ddsd.ddpfPixelFormat.dwBBitMask = 0x0000FF;

	// Create a offscreen bitmap.
	hRet = g_pDD->CreateSurface(&ddsd, &g_pSurface[i], NULL);
	if (hRet != DD_OK)
	{
		LOG("\nCreate surface fail %d\n", hRet);
		return NULL;
	}

	ddSurfaceDesc.dwSize = sizeof(DDSURFACEDESC);
	if((hRet = g_pSurface[i]->Lock(NULL, &ddSurfaceDesc, DDLOCK_READONLY|DDLOCK_WAITNOTBUSY, NULL)) != DD_OK)
	{
		LOG("\nLock surface fail 0x%x\n", hRet);
		return NULL;
	}

	g_pBuffer[i] = (BYTE*)ddSurfaceDesc.lpSurface;

	return g_pBuffer[i];
}

void IcrBufFree(
				IcrSession* pSession,
				void * addr)
{
	int i;

	if (g_pDD == NULL || addr == NULL) return;

	for (i=0 ; i<MAX_ICR_SURFACE ; i++)
	{
		if (g_pBuffer[i] == addr)
			break;
	}

	if (i >= MAX_ICR_SURFACE) return;

	g_pSurface[i]->Unlock(NULL);
	g_pSurface[i]->Release();
	g_pSurface[i] = NULL;
	g_pBuffer[i] = NULL;

	for (i=0 ; i<MAX_ICR_SURFACE ; i++)
	{
		if (g_pSurface[i] != NULL)
			break;
	}

	if (i >= MAX_ICR_SURFACE)
	{
		g_pDD->Release();
		g_pDD = NULL;
	}
}

#elif defined (WIN32)

void * IcrBufAlloc(
	IcrSession* pSession,
	int iWidth, int iHeight, int iBytePerPixel,
	unsigned int iFlag)
{
    return malloc(iWidth * iHeight * iBytePerPixel);
}

void IcrBufFree(
	IcrSession* pSession,
	void * addr)
{
	free(addr);
}

#else // if defined (LINUX)

// Linux version of buffer allocation now is provided by IcrCtrlSvr
// since it currently requires the knowledge of low level Icr driver handle
// Eventually, this will be replaced by platform generic physical contingous buffer allocation function (such as bmm)

#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
