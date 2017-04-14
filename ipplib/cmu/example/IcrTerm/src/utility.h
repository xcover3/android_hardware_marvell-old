/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *-----------------------------------------------------------------------------
 */

#ifndef _TEST_UTILITY_H_
#define _TEST_UTILITY_H_


#include "ctrlsvr.h"




#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#define LOG(...)\
	printf(__VA_ARGS__);

#define ICR_CHECK(cond, ...)\
	if ( !(cond) )\
	{\
		LOG("Test suite error at file '%s' line %d\n", __FILE__, __LINE__);\
		LOG("Test suite error content: '"#cond"'\n");\
		LOG(__VA_ARGS__);\
		exit(-1);\
	}

#define ICR_ASSERT(cond, ...)\
	if ( !(cond) )\
	{\
		LOG("Test failed at file '%s' line %d\n", __FILE__, __LINE__);\
		LOG("Test content: '"#cond"'\n");\
		LOG(__VA_ARGS__);\
	}

typedef unsigned char BYTE;


#if defined(WM) || defined(WINCE)

extern int IcrGetBitmapInfo(const wchar_t *filename, int *pWidth, int *pHeight, int *pBpp, int *bIndexed);
extern int IcrLoadBitmap(const wchar_t *filename, BYTE *pRGB, int *pWidth, int *pHeight);
extern int IcrStoreBitmap(const wchar_t *filename, BYTE *pRGB, int iWidth, int iHeight);
extern int IcrDisplayBitmap(BYTE *pRGB, int iWidth, int iHeight);

#else

extern int IcrGetBitmapInfo(const char *filename, int *pWidth, int *pHeight, int *pBpp, int *bIndexed);
extern int IcrLoadBitmap(const char *filename, BYTE *pRGB, int *pWidth, int *pHeight);
extern int IcrStoreBitmap(const char *filename, BYTE *pRGB, int iWidth, int iHeight);
extern int IcrDisplayBitmap(BYTE *pRGB, int iWidth, int iHeight);

#endif


#if defined(WIN32) || defined(WM) || defined(WINCE)

extern void * IcrBufAlloc(
	IcrSession* pSession,
	int iWidth, int iHeight, int iBytePerPixel,
	unsigned int iFlag);
extern void IcrBufFree(
	IcrSession* pSession,
	void * addr);

#endif





#ifdef __cplusplus
}
#endif /* __cplusplus */




#endif // _TEST_UTILITY_H_

