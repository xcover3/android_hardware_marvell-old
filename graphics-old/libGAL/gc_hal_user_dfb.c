/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


#if gcdENABLE_3D || gcdENABLE_VG
#ifdef EGL_API_DFB
#include <directfb.h>
#include "gc_hal_user_linux.h"
#include "gc_hal_eglplatform.h"

#define _GC_OBJ_ZONE    gcvZONE_OS

#ifndef ABS
#   define ABS(x) (((x) < 0) ? -(x) : (x))
#endif

#define DFB_EGL_NUM_BACKBUFFERS      3
#define DFB_DUMMY                    (31415926)

typedef struct _DFBEGLBuffer
{
    gctINT32 width;
    gctINT32 height;
    gceSURF_FORMAT format;
    gcoSURF surface;
    gctBOOL locked;
    gctPOINTER resolveBits;
} DFBEGLBuffer;

typedef struct _DFBEGlWindowInfo
{
    gctINT32 dx;
    gctINT32 dy;
    gctUINT width;
    gctUINT height;
    gceSURF_FORMAT format;
    gctUINT bpp;
} DFBEGlWindowInfo;

/* Structure that defines a pixmap. */
struct _DFBDisplay
{
    IDirectFB*             pDirectFB;
    IDirectFBDisplayLayer* pLayer;
    IDirectFBEventBuffer*  pEventBuffer;
    gctINT                 winWidth;
    gctINT                 winHeight;
    pthread_cond_t         cond;
    pthread_mutex_t        condMutex;
};

/* Structure that defines a pixmap. */
struct _DFBPixmap
{
    /* Pointer to memory bits. */
    gctPOINTER       original;
    gctPOINTER       bits;

    /* Bits per pixel. */
    gctINT         bpp;

    /* Size. */
    gctINT         width, height;
    gctINT         alignedWidth, alignedHeight;
    gctINT         stride;
};

struct _DFBWindow
{
    IDirectFBWindow*    pWindow;
    IDirectFBSurface*   pDFBSurf;
    DFBEGLBuffer        backbuffers[DFB_EGL_NUM_BACKBUFFERS];
    DFBEGlWindowInfo    info;
    gctINT              current;
};

static gctBOOL
GetDFBSurfFormat(IDirectFBSurface* pDFBSurface, gceSURF_FORMAT *Format, gctUINT32 *Bpp)
{
    gceSURF_FORMAT format;
    DFBSurfacePixelFormat dfb_format;
    gctUINT32 bpp;

    if(pDFBSurface == gcvNULL)
        return gcvFALSE;

    if (pDFBSurface->GetPixelFormat(pDFBSurface, &dfb_format) != DFB_OK)
    {
        return gcvFALSE;
    }

    switch (dfb_format)
    {
    /* A1R5G5B5 */
    case DSPF_ARGB1555:
        format = gcvSURF_A1R5G5B5;
        bpp = 16;
        break;
    /* A4R4G4B4 */
    case DSPF_ARGB4444:
        format = gcvSURF_A4R4G4B4;
        bpp = 16;
        break;
#if (DIRECTFB_MAJOR_VERSION >= 1) && (DIRECTFB_MINOR_VERSION >= 4)
    /* R4G4B4A4 */
    case DSPF_RGBA4444:
        format = gcvSURF_R4G4B4A4;
        bpp = 16;
        break;
#endif
    /* A8R8G8B8 */
    case DSPF_ARGB:
        format = gcvSURF_A8R8G8B8;
        bpp = 32;
        break;
    /* R5G6B5 */
    case DSPF_RGB16:
        format = gcvSURF_R5G6B5;
        bpp = 16;
        break;
    /* X1R5G5B5 */
    case DSPF_RGB555:
        format = gcvSURF_X1R5G5B5;
        bpp = 16;
        break;
    /* X4R4G4B4 */
    case DSPF_RGB444:
        format = gcvSURF_X4R4G4B4;
        bpp = 16;
        break;
    /* X8R8G8B8 */
    case DSPF_RGB32:
        format = gcvSURF_X8R8G8B8;
        bpp = 24;
        break;
    /* INDEX8 */
    case DSPF_LUT8:
        format = gcvSURF_INDEX8;
        bpp = 8;
        break;
    /* YV12 */
    case DSPF_YV12:
        format = gcvSURF_YV12;
        bpp = 12;
        break;
    /* I420 */
    case DSPF_I420:
        format = gcvSURF_I420;
        bpp = 12;
        break;
    /* NV12 */
    case DSPF_NV12:
        format = gcvSURF_NV12;
        bpp = 12;
        break;
    /* NV21 */
    case DSPF_NV21:
        format = gcvSURF_NV21;
        bpp = 12;
        break;
    /* NV16 */
    case DSPF_NV16:
        format = gcvSURF_NV16;
        bpp = 16;
        break;
    /* YUY2 */
    case DSPF_YUY2:
        format = gcvSURF_YUY2;
        bpp = 16;
        break;
    /* UYVY */
    case DSPF_UYVY:
        format = gcvSURF_UYVY;
        bpp = 16;
        break;
    case DSPF_A8:
        format = gcvSURF_A8;
        bpp = 8;
        break;
    /* UNKNOWN */
    default:
        format = gcvSURF_UNKNOWN;
        return gcvFALSE;
    }

    if (Format)
    {
        *Format = format;
    }

    if (Bpp)
    {
        *Bpp = bpp;
    }

    return gcvTRUE;
}

static halKeyMap keys[] =
{   /*  directFB identifiers for basic mapping    */
    /* 00 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 01 */ { HAL_A,               HAL_UNKNOWN     },
    /* 02 */ { HAL_B,               HAL_UNKNOWN     },
    /* 03 */ { HAL_C,               HAL_UNKNOWN     },
    /* 04 */ { HAL_D,               HAL_UNKNOWN     },
    /* 05 */ { HAL_E,               HAL_UNKNOWN     },
    /* 06 */ { HAL_F,               HAL_UNKNOWN     },
    /* 07 */ { HAL_G,               HAL_UNKNOWN     },
    /* 08 */ { HAL_H,               HAL_UNKNOWN     },
    /* 09 */ { HAL_I,               HAL_UNKNOWN     },
    /* 0A */ { HAL_J,               HAL_UNKNOWN     },
    /* 0B */ { HAL_K,               HAL_UNKNOWN     },
    /* 0C */ { HAL_L,               HAL_UNKNOWN     },
    /* 0D */ { HAL_M,               HAL_UNKNOWN     },
    /* 0E */ { HAL_N,               HAL_UNKNOWN     },
    /* 0F */ { HAL_O,               HAL_UNKNOWN     },
    /* 10 */ { HAL_P,               HAL_UNKNOWN     },
    /* 11 */ { HAL_Q,               HAL_UNKNOWN     },
    /* 12 */ { HAL_R,               HAL_UNKNOWN     },
    /* 13 */ { HAL_S,               HAL_UNKNOWN     },
    /* 14 */ { HAL_T,               HAL_UNKNOWN     },
    /* 15 */ { HAL_U,               HAL_UNKNOWN     },
    /* 16 */ { HAL_V,               HAL_UNKNOWN     },
    /* 17 */ { HAL_W,               HAL_UNKNOWN     },
    /* 18 */ { HAL_X,               HAL_UNKNOWN     },
    /* 19 */ { HAL_Y,               HAL_UNKNOWN     },
    /* 1A */ { HAL_Z,               HAL_UNKNOWN     },
    /* 1B */ { HAL_0,               HAL_UNKNOWN     },
    /* 1C */ { HAL_1,               HAL_UNKNOWN     },
    /* 1D */ { HAL_2,               HAL_UNKNOWN     },
    /* 1E */ { HAL_3,               HAL_UNKNOWN     },
    /* 1F */ { HAL_4,               HAL_UNKNOWN     },
    /* 20 */ { HAL_5,               HAL_UNKNOWN     },
    /* 21 */ { HAL_6,               HAL_UNKNOWN     },
    /* 22 */ { HAL_7,               HAL_UNKNOWN     },
    /* 23 */ { HAL_8,               HAL_UNKNOWN     },
    /* 24 */ { HAL_9,               HAL_UNKNOWN     },
    /* 25 */ { HAL_F1,              HAL_UNKNOWN     },
    /* 26 */ { HAL_F2,              HAL_UNKNOWN     },
    /* 27 */ { HAL_F3,              HAL_UNKNOWN     },
    /* 28 */ { HAL_F4,              HAL_UNKNOWN     },
    /* 29 */ { HAL_F5,              HAL_UNKNOWN     },
    /* 2A */ { HAL_F6,              HAL_UNKNOWN     },
    /* 2B */ { HAL_F7,              HAL_UNKNOWN     },
    /* 2C */ { HAL_F8,              HAL_UNKNOWN     },
    /* 2D */ { HAL_F9,              HAL_UNKNOWN     },
    /* 2E */ { HAL_F10,             HAL_UNKNOWN     },
    /* 2F */ { HAL_F11,             HAL_UNKNOWN     },
    /* 30 */ { HAL_F12,             HAL_UNKNOWN     },
    /* 31 */ { HAL_LSHIFT,          HAL_UNKNOWN     },
    /* 32 */ { HAL_RSHIFT,          HAL_UNKNOWN     },
    /* 33 */ { HAL_LCTRL,           HAL_UNKNOWN     },
    /* 34 */ { HAL_RCTRL,           HAL_UNKNOWN     },
    /* 35 */ { HAL_LALT,            HAL_UNKNOWN     },
    /* 36 */ { HAL_RALT,            HAL_UNKNOWN     },
    /* 37 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 38 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 39 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 3A */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 3B */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 3C */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 3D */ { HAL_CAPSLOCK,        HAL_UNKNOWN     },
    /* 3E */ { HAL_NUMLOCK,         HAL_UNKNOWN     },
    /* 3F */ { HAL_SCROLLLOCK,      HAL_UNKNOWN     },
    /* 40 */ { HAL_ESCAPE,          HAL_UNKNOWN     },
    /* 41 */ { HAL_LEFT,            HAL_UNKNOWN     },
    /* 42 */ { HAL_RIGHT,           HAL_UNKNOWN     },
    /* 43 */ { HAL_UP,              HAL_UNKNOWN     },
    /* 44 */ { HAL_DOWN,            HAL_UNKNOWN     },
    /* 45 */ { HAL_TAB,             HAL_UNKNOWN     },
    /* 46 */ { HAL_ENTER,           HAL_UNKNOWN     },
    /* 47 */ { HAL_SPACE,           HAL_UNKNOWN     },
    /* 48 */ { HAL_BACKSPACE,       HAL_UNKNOWN     },
    /* 49 */ { HAL_INSERT,          HAL_UNKNOWN     },
    /* 4A */ { HAL_DELETE,          HAL_UNKNOWN     },
    /* 4B */ { HAL_HOME,            HAL_UNKNOWN     },
    /* 4C */ { HAL_END,             HAL_UNKNOWN     },
    /* 4D */ { HAL_PGUP,            HAL_UNKNOWN     },
    /* 4E */ { HAL_PGDN,            HAL_UNKNOWN     },
    /* 4F */ { HAL_PRNTSCRN,        HAL_UNKNOWN     },
    /* 50 */ { HAL_BREAK,           HAL_UNKNOWN     },
    /* 51 */ { HAL_SINGLEQUOTE,     HAL_UNKNOWN     },
    /* 52 */ { HAL_HYPHEN,          HAL_UNKNOWN     },
    /* 53 */ { HAL_EQUAL,           HAL_UNKNOWN     },
    /* 54 */ { HAL_LBRACKET,        HAL_UNKNOWN     },
    /* 55 */ { HAL_RBRACKET,        HAL_UNKNOWN     },
    /* 56 */ { HAL_BACKSLASH,       HAL_UNKNOWN     },
    /* 57 */ { HAL_SEMICOLON,       HAL_UNKNOWN     },
    /* 58 */ { HAL_BACKQUOTE,       HAL_UNKNOWN     },
    /* 59 */ { HAL_COMMA,           HAL_UNKNOWN     },
    /* 5A */ { HAL_PERIOD,          HAL_UNKNOWN     },
    /* 5B */ { HAL_SLASH,           HAL_UNKNOWN     },
    /* 5C */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 5D */ { HAL_PAD_SLASH,       HAL_UNKNOWN     },
    /* 5E */ { HAL_PAD_ASTERISK,    HAL_UNKNOWN     },
    /* 5F */ { HAL_PAD_HYPHEN,      HAL_UNKNOWN     },
    /* 60 */ { HAL_PAD_PLUS,        HAL_UNKNOWN     },
    /* 61 */ { HAL_PAD_ENTER,       HAL_UNKNOWN     },
    /* 62 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 63 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 64 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 65 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 66 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 67 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 68 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 69 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6A */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6B */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6C */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6D */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6E */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 6F */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 70 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 71 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 72 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 73 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 74 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 75 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 76 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 77 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 78 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 79 */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7A */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7B */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7C */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7D */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7E */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
    /* 7F */ { HAL_UNKNOWN,         HAL_UNKNOWN     },
};


/*******************************************************************************
** Display.
*/

gceSTATUS
gcoOS_GetDisplay(
    OUT HALNativeDisplayType * Display,
    IN gctPOINTER Context
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    struct _DFBDisplay* display = gcvNULL;
    DFBDisplayLayerConfig config;
    gcmHEADER();
    *Display = (HALNativeDisplayType)gcvNULL;
    do
    {
        if(DirectFBInit(gcvNULL, gcvNULL) != DFB_OK)
        {
            gcmFATAL("%s: failed to init directfb.", __FUNCTION__);
            status = gcvSTATUS_OUT_OF_RESOURCES;
            gcmFOOTER();
            return status;
        }
        display = (struct _DFBDisplay*) malloc(sizeof (struct _DFBDisplay));
        memset(display, 0, sizeof(struct _DFBDisplay));

        if (DirectFBCreate(&(display->pDirectFB)) != DFB_OK)
        {
            gcmFATAL("%s: failed to create directfb.", __FUNCTION__);
            status = gcvSTATUS_OUT_OF_RESOURCES;
            gcmFOOTER();
            return status;
        }
        if(display->pDirectFB->CreateInputEventBuffer(display->pDirectFB, DICAPS_KEYS, DFB_FALSE, &(display->pEventBuffer)) != DFB_OK)
        {
            gcmFATAL("%s: failed to create directfb input event.", __FUNCTION__);
            status = gcvSTATUS_OUT_OF_RESOURCES;
            break;
        }
        if(display->pDirectFB->GetDisplayLayer( display->pDirectFB, DLID_PRIMARY, &(display->pLayer) ) != DFB_OK)
            break;
        display->pLayer->GetConfiguration(display->pLayer, &config);
        display->winWidth = config.width;
        display->winHeight = config.height;
        pthread_cond_init(&(display->cond), gcvNULL);
        pthread_mutex_init(&(display->condMutex), gcvNULL);
        *Display = (HALNativeDisplayType)display;
        gcmFOOTER();
        return status;
    }
    while(0);
    if(display != gcvNULL)
    {
        if(display->pDirectFB != gcvNULL)
        {
            if(display->pEventBuffer != gcvNULL)
            {
                display->pEventBuffer->Release(display->pEventBuffer);
            }
            if(display->pLayer != gcvNULL)
            {
                display->pLayer->Release(display->pLayer);
            }
            display->pDirectFB->Release(display->pDirectFB);
        }
        free(display);
        display = gcvNULL;
    }
    status = gcvSTATUS_OUT_OF_RESOURCES;
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_GetDisplayByIndex(
    IN gctINT DisplayIndex,
    OUT HALNativeDisplayType * Display,
    IN gctPOINTER Context
    )
{
    return gcoOS_GetDisplay(Display, Context);
}

gceSTATUS
gcoOS_GetDisplayInfo(
    IN HALNativeDisplayType Display,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctSIZE_T * Physical,
    OUT gctINT * Stride,
    OUT gctINT * BitsPerPixel
    )
{
    if(Display == gcvNULL)
        return gcvSTATUS_NOT_SUPPORTED;
    if(Width != gcvNULL)
        *Width = Display->winWidth;
    if(Height != gcvNULL)
        *Height = Display->winHeight;
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_GetDisplayInfoEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctUINT DisplayInfoSize,
    OUT halDISPLAY_INFO * DisplayInfo
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    struct _DFBWindow* window = Window;

    DisplayInfo->width = window->info.width;
    DisplayInfo->height = window->info.height;
    DisplayInfo->stride = DFB_DUMMY;
    DisplayInfo->bitsPerPixel = window->info.bpp;
    DisplayInfo->logical = (gctPOINTER) DFB_DUMMY;
    DisplayInfo->physical = DFB_DUMMY;
    DisplayInfo->multiBuffer = DFB_EGL_NUM_BACKBUFFERS;
    DisplayInfo->backBufferY = 0;
    DisplayInfo->flip = gcvTRUE;
    DisplayInfo->wrapFB = gcvFALSE;

    return status;
}

gceSTATUS
gcoOS_GetDisplayVirtual(
    IN HALNativeDisplayType Display,
    OUT gctINT * Width,
    OUT gctINT * Height
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_GetDisplayBackbuffer(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    OUT gctPOINTER  *  context,
    OUT gcoSURF     *  surface,
    OUT gctUINT * Offset,
    OUT gctINT * X,
    OUT gctINT * Y
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    struct _DFBWindow  *window = Window;

    /*Wait the back buffer to be free*/
    pthread_mutex_lock(&(Display->condMutex));
    if (window->backbuffers[window->current].locked)
    {
        pthread_cond_wait(&(Display->cond), &(Display->condMutex));
    }

    window->backbuffers[window->current].locked = gcvTRUE;
    pthread_mutex_unlock(&(Display->condMutex));

    *context = &window->backbuffers[window->current];
    *surface = window->backbuffers[window->current].surface;
    *Offset  = 0;
    *X       = 0;
    *Y       = 0;
    window->current = (window->current + 1)%DFB_EGL_NUM_BACKBUFFERS;

    return status;
}

gceSTATUS
gcoOS_SetDisplayVirtual(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctUINT Offset,
    IN gctINT X,
    IN gctINT Y
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_SetDisplayVirtualEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctPOINTER Context,
    IN gcoSURF Surface,
    IN gctUINT Offset,
    IN gctINT X,
    IN gctINT Y
    )
{
    gctPOINTER logical;
    gctINT pitch, BitsPerPixel;
    IDirectFBSurface *pDFBSurf;
    DFBEGLBuffer *buffer = (DFBEGLBuffer*)Context;

    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    BitsPerPixel = Window->info.bpp;
    pDFBSurf = Window->pDFBSurf;
    if (pDFBSurf == gcvNULL)
    {
        if(Window->pWindow->GetSurface( Window->pWindow, &pDFBSurf ) != DFB_OK)
            return gcvSTATUS_INVALID_ARGUMENT;
        Window->pDFBSurf = pDFBSurf;
    }

    if (pDFBSurf->Lock(pDFBSurf,
        DSLF_WRITE,
        &logical,
        &pitch) != DFB_OK)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }
    memcpy(logical, buffer->resolveBits, ((buffer->width * BitsPerPixel / 8 + 3) & ~3) * gcmABS(buffer->height));
    pDFBSurf->Unlock(pDFBSurf);
    pDFBSurf->Flip( pDFBSurf, NULL, DSFLIP_WAITFORSYNC );
    Display->pDirectFB->WaitIdle( Display->pDirectFB );

    pthread_mutex_lock(&(Display->condMutex));
    buffer->locked = gcvFALSE;
    pthread_cond_broadcast(&(Display->cond));
    pthread_mutex_unlock(&(Display->condMutex));

    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_SetSwapInterval(
    IN HALNativeDisplayType Display,
    IN gctINT Interval
)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_SetSwapIntervalEx(
    IN HALNativeDisplayType Display,
    IN gctINT Interval,
    IN gctPOINTER localDisplay)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_GetSwapInterval(
    IN HALNativeDisplayType Display,
    IN gctINT_PTR Min,
    IN gctINT_PTR Max
)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_DestroyDisplay(
    IN HALNativeDisplayType Display
    )
{
    if(Display)
    {
        if(Display->pDirectFB)
        {
            if(Display->pEventBuffer != gcvNULL)
            {
                Display->pEventBuffer->Release(Display->pEventBuffer);
            }
            if(Display->pLayer != gcvNULL)
            {
                Display->pLayer->Release(Display->pLayer);
            }
            Display->pDirectFB->Release(Display->pDirectFB);
        }
        pthread_mutex_destroy(&(Display->condMutex));
        pthread_cond_destroy(&(Display->cond));
        free(Display);
    }
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_DisplayBufferRegions(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctINT NumRects,
    IN gctINT_PTR Rects
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

/*******************************************************************************
** Context
*/
gceSTATUS
gcoOS_CreateContext(IN gctPOINTER Display, IN gctPOINTER Context)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_DestroyContext(IN gctPOINTER Display, IN gctPOINTER Context)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_MakeCurrent(IN gctPOINTER Display,
    IN HALNativeWindowType DrawDrawable,
    IN HALNativeWindowType ReadDrawable,
    IN gctPOINTER Context,
    IN gcoSURF ResolveTarget)
{
    return gcvSTATUS_NOT_SUPPORTED;
}


/*******************************************************************************
** Drawable
*/
gceSTATUS
gcoOS_CreateDrawable(IN gctPOINTER Display, IN HALNativeWindowType Drawable)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_DestroyDrawable(IN gctPOINTER Display, IN HALNativeWindowType Drawable)
{
    return gcvSTATUS_NOT_SUPPORTED;
}

/*******************************************************************************
** Windows
*/
void
gcoOS_DestoryBackBuffers(
    IN HALNativeWindowType Window
    )
{
    gctUINT i;

    if (Window != gcvNULL)
    {
        for (i = 0; i < DFB_EGL_NUM_BACKBUFFERS; i++)
        {
            if (Window->backbuffers[i].surface != gcvNULL)
            {
                gcoSURF_Unlock(
                    Window->backbuffers[i].surface,
                    gcvNULL
                    );

                gcoSURF_Destroy(
                    Window->backbuffers[i].surface
                    );
            }
        }
    }
}

gceSTATUS
gcoOS_CreateBackBuffers(
    IN HALNativeDisplayType Display,
    IN gctINT X,
    IN gctINT Y,
    IN gctINT Width,
    IN gctINT Height,
    IN HALNativeWindowType window
    )
{
    gctUINT i;
    gceSTATUS status = gcvSTATUS_OK;
    do
    {
        gceSURF_FORMAT resolveFormat = gcvSURF_UNKNOWN;
        gctUINT32 BitsPerPixel = 0;
        IDirectFBWindow  *pWindow = window->pWindow;
        IDirectFBSurface *pDFBSurf = window->pDFBSurf;

        window->info.dx     = 0;
        window->info.dy     = 0;
        window->info.width  = Width;
        window->info.height = Height;

        if (pDFBSurf == gcvNULL)
        {
            if(pWindow->GetSurface( pWindow, &pDFBSurf ) != DFB_OK)
                break;
            window->pDFBSurf = pDFBSurf;
        }

        if (!GetDFBSurfFormat(pDFBSurf, &resolveFormat, &BitsPerPixel))
        {
            break;
        }

        window->info.bpp = BitsPerPixel;
        window->info.format = resolveFormat;

        for (i = 0; i < DFB_EGL_NUM_BACKBUFFERS; i++)
        {
            gctPOINTER memoryResolve[3] = {gcvNULL};
            gcmONERROR(
                gcoSURF_Construct(
                    gcvNULL,
                    Width,
                    Height,
                    1,
                    gcvSURF_BITMAP,
                    resolveFormat,
                    gcvPOOL_DEFAULT,
                    &window->backbuffers[i].surface
                    ));

            gcmONERROR(
                gcoSURF_Lock(
                    window->backbuffers[i].surface,
                    gcvNULL,
                    memoryResolve
                    ));

            window->backbuffers[i].resolveBits = memoryResolve[0];
            window->backbuffers[i].width = window->info.width;
            window->backbuffers[i].height = window->info.height;
            window->backbuffers[i].format = resolveFormat;
            window->backbuffers[i].locked = gcvFALSE;
        }
    }
    while (gcvFALSE);

    return status;

OnError:
    gcoOS_DestoryBackBuffers(window);
    status = gcvSTATUS_OUT_OF_RESOURCES;
    return status;
}

gceSTATUS
gcoOS_CreateWindow(
    IN HALNativeDisplayType Display,
    IN gctINT X,
    IN gctINT Y,
    IN gctINT Width,
    IN gctINT Height,
    OUT HALNativeWindowType * Window
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    struct _DFBWindow  *DFBWindow = gcvNULL;
    IDirectFBWindow    *window = gcvNULL;

    gcmHEADER_ARG("Display=0x%x X=%d Y=%d Width=%d Height=%d", Display, X, Y, Width, Height);

    do
    {
        DFBWindowDescription  desc;
        if(Display == gcvNULL)
            break;
        if(Display->pLayer == gcvNULL)
            break;
        Display->pLayer->SetCooperativeLevel( Display->pLayer, DLSCL_ADMINISTRATIVE );

        DFBWindow = (struct _DFBWindow *) malloc(sizeof(struct _DFBWindow));
        if (DFBWindow == gcvNULL)
            break;
        memset(DFBWindow, 0, sizeof(struct _DFBWindow));

        desc.flags = ( DWDESC_POSX | DWDESC_POSY);
        if(X < 0)
            X = 0;
        if(Y < 0)
            Y = 0;
        desc.posx   = X;
        desc.posy   = Y;
        /* if no width or height setting, dfb system will use the default layer config for the window creation*/
        if((Width <= 0) || (Height <= 0))
        {
            Width = Display->winWidth;
            Height = Display->winHeight;
        }
        else
        {
            Display->winWidth = Width;
            Display->winHeight = Height;
        }
        desc.flags = desc.flags | DWDESC_WIDTH | DWDESC_HEIGHT;
        desc.width  = Width;
        desc.height = Height;
        if(Display->pLayer->CreateWindow( Display->pLayer, &desc, &window) != DFB_OK)
            break;
        DFBWindow->pWindow = window;

        window->SetOpacity(window, 0xFF);
        window->RaiseToTop(window);
        window->AttachEventBuffer(window, Display->pEventBuffer);
        Display->pLayer->EnableCursor(Display->pLayer, 0);

        status = gcoOS_CreateBackBuffers(Display, X, Y, Width, Height, DFBWindow);
        if(status != gcvSTATUS_OK)
            break;
        *Window = DFBWindow;
        gcmFOOTER_ARG("*Window=0x%x", *Window);
        return status;
    }
    while (0);

    if(window)
        window->Release(window);

    status = gcvSTATUS_OUT_OF_RESOURCES;
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_GetWindowInfo(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    OUT gctINT * X,
    OUT gctINT * Y,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctINT * BitsPerPixel,
    OUT gctUINT * Offset
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctINT width, height;
    gceSURF_FORMAT format;

    gcmHEADER_ARG("Display=0x%x Window=0x%x", Display, Window);

    if (Window == 0 || Window->pWindow == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    do
    {
        IDirectFBWindow  *pWindow = Window->pWindow;
        IDirectFBSurface *pDFBSurf = Window->pDFBSurf;

        if (pDFBSurf == gcvNULL)
        {
            if(pWindow->GetSurface( pWindow, &pDFBSurf ) != DFB_OK)
                break;
            Window->pDFBSurf = pDFBSurf;
        }

        if (!GetDFBSurfFormat(pDFBSurf, &format, (gctUINT32*)BitsPerPixel))
        {
            break;
        }

        if(X != gcvNULL)
            * X = 0;
        if(Y != gcvNULL)
            * Y = 0;
        pDFBSurf->GetSize(pDFBSurf, &width, &height);

        if(Width != gcvNULL)
            * Width = width;
        if(Height != gcvNULL)
            * Height = height;

        if (Offset != gcvNULL)
            *Offset = 0;

        gcmFOOTER_NO();
        return status;
    }
    while(gcvFALSE);

    status = gcvSTATUS_INVALID_ARGUMENT;
    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoOS_DestroyWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{
    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == NULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    if (Window->pDFBSurf)
        Window->pDFBSurf->Release(Window->pDFBSurf);

    Window->pWindow->Release(Window->pWindow);

    gcoOS_DestoryBackBuffers(Window);

    free(Window);
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_SwapBuffers(
    IN gctPOINTER Display,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_DrawImage(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gctINT Width,
    IN gctINT Height,
    IN gctINT BitsPerPixel,
    IN gctPOINTER Bits
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_GetImage(
    IN HALNativeWindowType Window,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    OUT gctINT * BitsPerPixel,
    OUT gctPOINTER * Bits
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

/*******************************************************************************
** Pixmaps. ********************************************************************
*/

gceSTATUS
gcoOS_CreatePixmap(
    IN HALNativeDisplayType Display,
    IN gctINT Width,
    IN gctINT Height,
    IN gctINT BitsPerPixel,
    OUT HALNativePixmapType * Pixmap
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctINT alignedWidth, alignedHeight;
    struct _DFBPixmap* pixmap;
    gcmHEADER_ARG("Display=0x%x Width=%d Height=%d BitsPerPixel=%d", Display, Width, Height, BitsPerPixel);

    if ((Width <= 0) || (Height <= 0) || (BitsPerPixel <= 0))
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    do
    {
        alignedWidth   = (Width + 0x0F) & (~0x0F);
        alignedHeight  = (Height + 0x3) & (~0x03);
        pixmap = (struct _DFBPixmap*) malloc(sizeof (struct _DFBPixmap));

        if (pixmap == gcvNULL)
        {
            break;
        }

        pixmap->original = malloc(alignedWidth * alignedHeight * (BitsPerPixel + 7) / 8 + 64);
        if (pixmap->original == gcvNULL)
        {
            free(pixmap);
            pixmap = gcvNULL;

            break;
        }
        pixmap->bits = (gctPOINTER)(((gctUINT)(gctCHAR*)pixmap->original + 0x3F) & (~0x3F));

        pixmap->width = Width;
        pixmap->height = Height;
        pixmap->alignedWidth    = alignedWidth;
        pixmap->alignedHeight   = alignedHeight;
        pixmap->bpp     = (BitsPerPixel + 0x07) & (~0x07);
        pixmap->stride  = Width * (pixmap->bpp) / 8;
        *Pixmap = (HALNativePixmapType)pixmap;
        gcmFOOTER_ARG("*Pixmap=0x%x", *Pixmap);
        return status;
    }
    while (0);

    status = gcvSTATUS_OUT_OF_RESOURCES;
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_GetPixmapInfo(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctINT * BitsPerPixel,
    OUT gctINT * Stride,
    OUT gctPOINTER * Bits
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    struct _DFBPixmap* pixmap;
    gcmHEADER_ARG("Display=0x%x Pixmap=0x%x", Display, Pixmap);
    pixmap = (struct _DFBPixmap*)Pixmap;
    if (pixmap == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    if (Width != NULL)
    {
        *Width = pixmap->width;
    }

    if (Height != NULL)
    {
        *Height = pixmap->height;
    }

    if (BitsPerPixel != NULL)
    {
        *BitsPerPixel = pixmap->bpp;
    }

    if (Stride != NULL)
    {
        *Stride = pixmap->stride;
    }

    if (Bits != NULL)
    {
        *Bits = pixmap->bits;
    }

    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoOS_DestroyPixmap(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap
    )
{
    struct _DFBPixmap* pixmap;
    pixmap = (struct _DFBPixmap*)Pixmap;
    if (pixmap != gcvNULL)
    {
        if (pixmap->original != NULL)
        {
            free(pixmap->original);
        }
        free(pixmap);
        pixmap = gcvNULL;
        Pixmap = gcvNULL;
    }
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_DrawPixmap(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gctINT Width,
    IN gctINT Height,
    IN gctINT BitsPerPixel,
    IN gctPOINTER Bits
    )
{

    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_LoadEGLLibrary(
    OUT gctHANDLE * Handle
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    *Handle = gcvNULL;
#if !gcdSTATIC_LINK
#if defined(ANDROID)
    status = gcoOS_LoadLibrary(gcvNULL, "libEGL_VIVANTE.so", Handle);
#else
    status = gcoOS_LoadLibrary(gcvNULL, "libEGL.so", Handle);
#endif
#endif

    return status;
}

gceSTATUS
gcoOS_FreeEGLLibrary(
    IN gctHANDLE Handle
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    if (Handle != gcvNULL)
        status = gcoOS_FreeLibrary(gcvNULL, Handle);
    return status;
}

gceSTATUS
gcoOS_ShowWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{

    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    Window->pWindow->RaiseToTop(Window->pWindow);

    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_HideWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{

    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    Window->pWindow->LowerToBottom(Window->pWindow);

    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_SetWindowTitle(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctCONST_STRING Title
    )
{

    if((Display == gcvNULL) || (Window == 0))
        return gcvSTATUS_INVALID_ARGUMENT;

    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_CapturePointer(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_GetEvent(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    OUT halEvent * Event
    )
{
    /* event buffer. */
    DFBWindowEvent evt;
    /* Translated scancode. */
    halKeys scancode;

    /* Test for valid Window and Event pointers. */
    if ((Display == gcvNULL) ||(Window == gcvNULL) || (Event == gcvNULL))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if(Display->pEventBuffer == gcvNULL)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if(Display->pEventBuffer->GetEvent( Display->pEventBuffer, DFB_EVENT(&evt) ) == DFB_OK)
    {
        switch (evt.type)
        {
        case DWET_KEYDOWN:
        case DWET_KEYUP:
            /* Keyboard event. */
            Event->type = HAL_KEYBOARD;

            /* Translate the scancode. */
            scancode = (evt.key_id & 0x80)
                ? keys[evt.key_id & 0x7F].extended
                : keys[evt.key_id & 0x7F].normal;
            /* Set scancode. */
            Event->data.keyboard.scancode = scancode;

            /* Set ASCII key. */
            Event->data.keyboard.key = (  (scancode < HAL_SPACE)
                || (scancode >= HAL_F1)
                )
                ? 0
                : (char) scancode;

            /* Set up or down flag. */
            Event->data.keyboard.pressed = (evt.type == DWET_KEYDOWN);
            /* Valid event. */
            return gcvSTATUS_OK;
        case DWET_BUTTONDOWN:
        case DWET_BUTTONUP:
            /* Button event. */
            Event->type = HAL_BUTTON;

            /* Set button states. */
            Event->data.button.left   = (evt.button == DIBI_LEFT) ? 1 : 0;
            Event->data.button.middle = (evt.button & DIBI_MIDDLE) ? 1 : 0;
            Event->data.button.right  = (evt.button & DIBI_RIGHT) ? 1 : 0;
            Event->data.button.x      = evt.cx;
            Event->data.button.y      = evt.cy;
            return gcvSTATUS_OK;
        case DWET_MOTION:
            /* Pointer event.*/
            Event->type = HAL_POINTER;

            /* Set pointer location. */
            Event->data.pointer.x = evt.cx;
            Event->data.pointer.y = evt.cy;
            /* Valid event. */
            return gcvSTATUS_OK;
        case DWET_CLOSE:
        case DWET_DESTROYED:
            /* Application should close. */
            Event->type = HAL_CLOSE;
            /* Valid event. */
            return gcvSTATUS_OK;
        default:
            return gcvSTATUS_NOT_FOUND;
        }
    }
    return gcvSTATUS_NOT_FOUND;
}

gceSTATUS
gcoOS_CreateClientBuffer(
    IN gctINT Width,
    IN gctINT Height,
    IN gctINT Format,
    IN gctINT Type,
    OUT gctPOINTER * ClientBuffer
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_GetClientBufferInfo(
    IN gctPOINTER ClientBuffer,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctINT * Stride,
    OUT gctPOINTER * Bits
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_DestroyClientBuffer(
    IN gctPOINTER ClientBuffer
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

gceSTATUS
gcoOS_InitLocalDisplayInfo(
    IN HALNativeDisplayType Display,
    IN OUT gctPOINTER * localDisplay
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_DeinitLocalDisplayInfo(
    IN HALNativeDisplayType Display,
    IN OUT gctPOINTER * localDisplay
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_GetDisplayInfoEx2(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctPOINTER  localDisplay,
    IN gctUINT DisplayInfoSize,
    OUT halDISPLAY_INFO * DisplayInfo
    )
{
    return gcoOS_GetDisplayInfoEx(Display,Window,DisplayInfoSize,DisplayInfo);
}

gceSTATUS
gcoOS_GetDisplayBackbufferEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctPOINTER  localDisplay,
    OUT gctPOINTER  *  context,
    OUT gcoSURF     *  surface,
    OUT gctUINT * Offset,
    OUT gctINT * X,
    OUT gctINT * Y
    )
{
    return gcoOS_GetDisplayBackbuffer(Display, Window, context, surface, Offset, X, Y);
}

gceSTATUS
gcoOS_IsValidDisplay(
    IN HALNativeDisplayType Display
    )
{
    if(Display != gcvNULL)
        return gcvSTATUS_OK;
    return gcvSTATUS_INVALID_ARGUMENT;
}

gctBOOL
gcoOS_SynchronousFlip(
    IN HALNativeDisplayType Display
    )
{
    return gcvFALSE;
}

gceSTATUS
gcoOS_GetNativeVisualId(
    IN HALNativeDisplayType Display,
    OUT gctINT* nativeVisualId
    )
{
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_GetWindowInfoEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    OUT gctINT * X,
    OUT gctINT * Y,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctINT * BitsPerPixel,
    OUT gctUINT * Offset,
    OUT gceSURF_FORMAT * Format
    )
{
    gctINT width, height;
    gceSURF_FORMAT format;
    IDirectFBSurface *pDFBSurf;

    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    do
    {
        pDFBSurf = Window->pDFBSurf;
        if (pDFBSurf == gcvNULL)
        {
            if(Window->pWindow->GetSurface( Window->pWindow, &pDFBSurf ) != DFB_OK)
                break;
            Window->pDFBSurf = pDFBSurf;
        }

        if (!GetDFBSurfFormat(pDFBSurf, &format, (gctUINT32*)BitsPerPixel))
        {
            break;
        }

        if(Format != gcvNULL)
            *Format = format;

        if(X != gcvNULL)
            * X = 0;
        if(Y != gcvNULL)
            * Y = 0;
        pDFBSurf->GetSize(pDFBSurf, &width, &height);

        if(Width != gcvNULL)
            * Width = width;
        if(Height != gcvNULL)
            * Height = height;
        return gcvSTATUS_OK;
    }
    while(gcvFALSE);
    return gcvSTATUS_INVALID_ARGUMENT;
}

gceSTATUS
gcoOS_DrawImageEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctINT Left,
    IN gctINT Top,
    IN gctINT Right,
    IN gctINT Bottom,
    IN gctINT Width,
    IN gctINT Height,
    IN gctINT BitsPerPixel,
    IN gctPOINTER Bits,
    IN gceSURF_FORMAT Format
    )
{
    gctPOINTER logical;
    gctINT pitch;
    IDirectFBSurface *pDFBSurf;

    if((Display == gcvNULL) || (Window == 0) || Window->pWindow == gcvNULL)
        return gcvSTATUS_INVALID_ARGUMENT;

    pDFBSurf = Window->pDFBSurf;
    if (pDFBSurf == gcvNULL)
    {
        if(Window->pWindow->GetSurface( Window->pWindow, &pDFBSurf ) != DFB_OK)
            return gcvSTATUS_INVALID_ARGUMENT;
        Window->pDFBSurf = pDFBSurf;
    }

    if (pDFBSurf->Lock(pDFBSurf,
        DSLF_WRITE,
        &logical,
        &pitch) != DFB_OK)
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }
    memcpy(logical, Bits, ((Width * BitsPerPixel / 8 + 3) & ~3) * gcmABS(Height));
    pDFBSurf->Unlock(pDFBSurf);
    pDFBSurf->Flip( pDFBSurf, NULL, DSFLIP_WAITFORSYNC );
    Display->pDirectFB->WaitIdle( Display->pDirectFB );

    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_GetPixmapInfoEx(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap,
    OUT gctINT * Width,
    OUT gctINT * Height,
    OUT gctINT * BitsPerPixel,
    OUT gctINT * Stride,
    OUT gctPOINTER * Bits,
    OUT gceSURF_FORMAT * Format
    )
{
    if (gcmIS_ERROR(gcoOS_GetPixmapInfo(
                        Display,
                        Pixmap,
                      (gctINT_PTR) Width, (gctINT_PTR) Height,
                      (gctINT_PTR) BitsPerPixel,
                      Stride,
                      Bits)))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Return format for pixmap depth. */
    switch (*BitsPerPixel)
    {
    case 16:
        *Format = gcvSURF_R5G6B5;
        break;

    case 32:
        *Format = gcvSURF_X8R8G8B8;
        break;

    default:
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Success. */
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_CopyPixmapBits(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap,
    IN gctUINT DstWidth,
    IN gctUINT DstHeight,
    IN gctINT DstStride,
    IN gceSURF_FORMAT DstFormat,
    OUT gctPOINTER DstBits
    )
{
    return gcvSTATUS_NOT_SUPPORTED;
}

#endif
#endif
