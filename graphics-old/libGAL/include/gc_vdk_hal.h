/****************************************************************************
*
*    Copyright 2012 - 2015 Vivante Corporation, Sunnyvale, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/


#ifndef __gc_vdk_hal_h_
#define __gc_vdk_hal_h_

#include <gc_vdk.h>

/*******************************************************************************
** Data type *******************************************************************
*/

typedef void * SURFACE;
typedef int SURF_FORMAT;
typedef void * OS;
#define IS_SUCCESS(func) (func == 0)
#ifdef __cplusplus
#define galNULL              0
#else
#define galNULL              ((void *) 0)
#endif


/*******************************************************************************
** API part.********************************************************************
*/
typedef int (* _GAL_GetDisplayByIndex) (int DisplayIndex,   EGLNativeDisplayType * ,void * );
typedef int (* _GAL_GetDisplayInfo) (EGLNativeDisplayType ,   int * ,   int * ,   unsigned long * ,   int * ,   int * );
typedef int (* _GAL_GetDisplayVirtual) (EGLNativeDisplayType ,   int * ,   int * );
typedef int (* _GAL_GetDisplayInfoEx) (EGLNativeDisplayType ,EGLNativeWindowType ,unsigned int ,   vdkDISPLAY_INFO * );
typedef int (* _GAL_GetDisplayBackbuffer) (EGLNativeDisplayType ,EGLNativeWindowType ,   void *  *  ,   SURFACE     *  ,   unsigned int * ,   int * ,   int * );
typedef int (* _GAL_SetDisplayVirtual) (EGLNativeDisplayType ,EGLNativeWindowType ,unsigned int ,int ,int );
typedef int (* _GAL_DestroyDisplay) (EGLNativeDisplayType );

/*******************************************************************************
** Windows. ********************************************************************
*/

typedef int (* _GAL_CreateWindow) (EGLNativeDisplayType ,int ,int ,int ,int ,   EGLNativeWindowType * );
typedef int (* _GAL_DestroyWindow) (EGLNativeDisplayType ,EGLNativeWindowType );
typedef int (* _GAL_DrawImage) (EGLNativeDisplayType ,EGLNativeWindowType ,int ,int ,int ,int ,int ,int ,int ,void * );
typedef int (* _GAL_GetWindowInfoEx) (EGLNativeDisplayType ,EGLNativeWindowType ,   int * ,   int * ,   int * ,   int * ,   int * ,   unsigned int * ,   SURF_FORMAT * );

/*******************************************************************************
** Pixmaps. ********************************************************************
*/

typedef int (* _GAL_CreatePixmap) (EGLNativeDisplayType ,int ,int ,int ,   EGLNativePixmapType * );
typedef int (* _GAL_GetPixmapInfo) (EGLNativeDisplayType ,EGLNativePixmapType ,   int * ,   int * ,   int * ,   int * ,   void * * );
typedef int (* _GAL_DrawPixmap) (EGLNativeDisplayType ,EGLNativePixmapType ,int ,int ,int ,int ,int ,int ,int ,void * );
typedef int (* _GAL_DestroyPixmap) (EGLNativeDisplayType ,EGLNativePixmapType );

/*******************************************************8/9/2012 11:40:45 AM************************
** OS relative. ****************************************************************
*/

typedef int (* _GAL_LoadEGLLibrary) (void * * );
typedef int (* _GAL_FreeEGLLibrary) (void * );
typedef int (* _GAL_ShowWindow) (EGLNativeDisplayType ,EGLNativeWindowType );
typedef int (* _GAL_HideWindow) (EGLNativeDisplayType ,EGLNativeWindowType );
typedef int (* _GAL_SetWindowTitle) (EGLNativeDisplayType ,EGLNativeWindowType ,const char * );
typedef int (* _GAL_CapturePointer) (EGLNativeDisplayType ,EGLNativeWindowType );
typedef int (* _GAL_GetEvent) (EGLNativeDisplayType ,EGLNativeWindowType ,   struct _halEvent * );
typedef int (* _GAL_CreateClientBuffer) (int ,int ,int ,int ,   void * * );
typedef int (* _GAL_GetClientBufferInfo) (void * ,   int * ,   int * ,   int * ,   void * * );
typedef int (* _GAL_DestroyClientBuffer) (void * );
typedef int (* _GAL_GetProcAddress) (OS ,void * ,const char * ,   void * * );

/*----- Time -----------------------------------------------------------------*/
/* Get the number of milliseconds since the system started. */

typedef unsigned int (* _GAL_GetTicks) (  void);


typedef struct _GAL_API {
_GAL_GetDisplayByIndex    GAL_GetDisplayByIndex;
_GAL_GetDisplayInfo    GAL_GetDisplayInfo;
_GAL_GetDisplayVirtual    GAL_GetDisplayVirtual;
_GAL_GetDisplayInfoEx    GAL_GetDisplayInfoEx;
_GAL_GetDisplayBackbuffer    GAL_GetDisplayBackbuffer;
_GAL_SetDisplayVirtual    GAL_SetDisplayVirtual;
_GAL_DestroyDisplay    GAL_DestroyDisplay;

/*******************************************************************************
** Windows. ********************************************************************
*/

_GAL_CreateWindow    GAL_CreateWindow;
_GAL_DestroyWindow    GAL_DestroyWindow;
_GAL_DrawImage    GAL_DrawImage;
_GAL_GetWindowInfoEx    GAL_GetWindowInfoEx;

/*******************************************************************************
** Pixmaps. ********************************************************************
*/

_GAL_CreatePixmap    GAL_CreatePixmap;
_GAL_GetPixmapInfo    GAL_GetPixmapInfo;
_GAL_DrawPixmap    GAL_DrawPixmap;
_GAL_DestroyPixmap    GAL_DestroyPixmap;

/*******************************************************************************
** OS relative. ****************************************************************
*/

_GAL_LoadEGLLibrary    GAL_LoadEGLLibrary;
_GAL_FreeEGLLibrary    GAL_FreeEGLLibrary;
_GAL_ShowWindow    GAL_ShowWindow;
_GAL_HideWindow    GAL_HideWindow;
_GAL_SetWindowTitle    GAL_SetWindowTitle;
_GAL_CapturePointer    GAL_CapturePointer;
_GAL_GetEvent    GAL_GetEvent;
_GAL_CreateClientBuffer    GAL_CreateClientBuffer;
_GAL_GetClientBufferInfo    GAL_GetClientBufferInfo;
_GAL_DestroyClientBuffer    GAL_DestroyClientBuffer;
_GAL_GetProcAddress    GAL_GetProcAddress;

/*----- Time -----------------------------------------------------------------*/
/* Get the number of milliseconds since the system started. */

_GAL_GetTicks    GAL_GetTicks;

} GAL_API;

#endif

