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
#ifdef EGL_API_DRI
#include "gc_hal_user_linux.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <X11/Xlibint.h>
#include "xf86dristr.h"
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/Xdamage.h>
#include "xf86dri.h"
#include "drm_sarea.h"
#include "gc_hal_eglplatform.h"

#define _GC_OBJ_ZONE    gcvZONE_OS
#define SAREA_MAX                       0x2000


typedef struct __DRIcontextRec  __DRIcontextPriv;
typedef struct __DRIdrawableRec __DRIdrawablePriv;
typedef struct __DRIDisplayRec  __DRIDisplay;

void _VivGetLock(__DRIdrawablePriv * drawable);
void _UpdateDrawableInfo(__DRIdrawablePriv * drawable);
void _UpdateDrawableInfoDrawableInfo(__DRIdrawablePriv * drawable);

static pthread_mutex_t drmMutex = PTHREAD_MUTEX_INITIALIZER;

#define LINUX_LOCK_FRAMEBUFFER( context )                       \
    do {                                                        \
        gctUINT8 __ret = 0;                                     \
        pthread_mutex_lock(&drmMutex);                          \
        if (!context->initialized) {                            \
            _VivGetLock( context->drawablePriv);                \
            context->initialized = 1;                           \
        } else {                                                \
            DRM_CAS( context->hwLock, context->hHWContext,      \
               (DRM_LOCK_HELD | context->hHWContext), __ret );  \
            if ( __ret)                                         \
               _VivGetLock( context->drawablePriv );            \
        }                                                       \
    } while (0)

#define LINUX_UNLOCK_FRAMEBUFFER( context )                     \
    do {                                                        \
        DRM_UNLOCK( context->fd,                                \
          context->hwLock,                                      \
          context->hHWContext );                                \
        pthread_mutex_unlock(&drmMutex);                        \
    } while (0)


#define DRI_VALIDATE_DRAWABLE_INFO_ONCE(pDrawPriv)              \
    do {                                                        \
        if (*(pDrawPriv->pStamp) != pDrawPriv->lastStamp) {     \
            _UpdateDrawableInfo(pDrawPriv);                     \
        }                                                       \
    } while (0)


#define DRI_VALIDATE_EXTRADRAWABLE_INFO_ONCE(pDrawPriv)              \
    do {                                                        \
        if ((*(pDrawPriv->pStamp) != pDrawPriv->lastStamp)) {     \
            _UpdateDrawableInfoDrawableInfo(pDrawPriv);             \
        }                                                       \
    } while (0)


#define DRI_VALIDATE_DRAWABLE_INFO(pdp)                                 \
 do {                                                                   \
    while ((*(pdp->pStamp) != pdp->lastStamp)) {                          \
        DRM_UNLOCK(pdp->fd, &pdp->display->pSAREA->lock,                \
               pdp->contextPriv->hHWContext);                           \
                                                                        \
        DRM_SPINLOCK(&pdp->display->pSAREA->drawable_lock, pdp->drawLockID);\
        DRI_VALIDATE_EXTRADRAWABLE_INFO_ONCE(pdp);                           \
        DRM_SPINUNLOCK(&pdp->display->pSAREA->drawable_lock, pdp->drawLockID);   \
                                                                        \
        DRM_LIGHT_LOCK(pdp->fd, &pdp->display->pSAREA->lock,            \
              pdp->contextPriv->hHWContext);                            \
              break;                            \
    }                                                                   \
} while (0)


#define DRI_FULLSCREENCOVERED(pdp)                                 \
 do {                                                                   \
        DRM_UNLOCK(pdp->fd, &pdp->display->pSAREA->lock,                \
               pdp->contextPriv->hHWContext);                           \
                                                                        \
        DRM_SPINLOCK(&pdp->display->pSAREA->drawable_lock, pdp->drawLockID);\
        _FullScreenCovered(pdp);                           \
        DRM_SPINUNLOCK(&pdp->display->pSAREA->drawable_lock, pdp->drawLockID);   \
                                                                        \
        DRM_LIGHT_LOCK(pdp->fd, &pdp->display->pSAREA->lock,            \
              pdp->contextPriv->hHWContext);                            \
} while (0)



struct __DRIcontextRec {
    gctPOINTER eglContext;
    /*
    ** Kernel context handle used to access the device lock.
    */
    gctINT fd;
    __DRIid contextID;

    /*
    ** Kernel context handle used to access the device lock.
    */
    drm_context_t hHWContext;
    drm_hw_lock_t *hwLock;
    gctUINT lockCnt;
    gctBOOL initialized;

    /*
    ** Pointer to drawable currently bound to this context.
    */
    __DRIdrawablePriv *drawablePriv;
    __DRIcontextPriv            *next;
};

#ifdef DRI_PIXMAPRENDER_ASYNC

#define _FRAME_BUSY     1
#define _FRAME_FREE     0
#define _NOT_SET        0
#define _TO_INITFRAME   1
#define _TO_UPDATEFRAME 2
#define NUM_ASYNCFRAME  4

typedef struct _asyncFrame {
    __DRIdrawablePriv *dridrawable;
    HALNativeWindowType Drawable;
    Pixmap backPixmap;
    GC xgc;
    gcoSURF pixWrapSurf;
    gceSURF_TYPE surftype;
    gceSURF_FORMAT surfformat;
    gctUINT32 backNode;
    int w;
    int h;
    gctSIGNAL signal;
    int busy;
} asyncFrame;
#endif

struct __DRIdrawableRec {
    /**
     * X's drawable ID associated with this private drawable.
     */
    HALNativeWindowType drawable;
    /**
     * Kernel drawable handle
     */
    drm_drawable_t hHWDrawable;

    gctINT fd;

    /**
     * Reference count for number of context's currently bound to this
     * drawable.
     *
     * Once it reaches zero, the drawable can be destroyed.
     *
     * \note This behavior will change with GLX 1.3.
     */
    gctINT refcount;

    /**
     * Index of this drawable information in the SAREA.
     */
    gctUINT index;

    /**
     * Pointer to the "drawable has changed ID" stamp in the SAREA.
     */
    gctUINT *pStamp;

    /**
     * Last value of the stamp.
     *
     * If this differs from the value stored at __DRIdrawablePrivate::pStamp,
     * then the drawable information has been modified by the X server, and the
     * drawable information (below) should be retrieved from the X server.
     */
    gctUINT lastStamp;

    gctINT x;
    gctINT y;
    gctINT w;
    gctINT h;
    gctINT numClipRects;
    drm_clip_rect_t *pClipRects;

    gctINT drawLockID;

    gctINT backX;
    gctINT backY;
    gctINT backClipRectType;
    gctINT numBackClipRects;
    gctINT wWidth;
    gctINT wHeight;
    gctINT xWOrigin;
    gctINT yWOrigin;
    drm_clip_rect_t *pBackClipRects;

    /* Back buffer information */
    gctUINT nodeName;
    gctUINT backNode;
    gctUINT backBufferPhysAddr;
    gctUINT * backBufferLogicalAddr;

    __DRIcontextPriv *contextPriv;

    __DRIDisplay * display;
    gctINT screen;
    gctBOOL drawableResize;
    gctBOOL fullScreenMode;
    __DRIdrawablePriv * next;
    int fullscreenCovered;

#ifdef DRI_PIXMAPRENDER
    Pixmap backPixmap;
    GC xgc;
    gcoSURF pixWrapSurf;
    gceSURF_TYPE surftype;
    gceSURF_FORMAT surfformat;
    gctUINT32 backPixmapNode;
#else

#ifdef DRI_PIXMAPRENDER_ASYNC
    gceSURF_TYPE surftype;
    gceSURF_FORMAT surfformat;
    gctPOINTER frameMutex;
    gctHANDLE workerThread;
    asyncFrame ascframe[NUM_ASYNCFRAME];
    int curIndex;
    int busyNum;
    gctSIGNAL busySIG;
    gctSIGNAL stopSIG;
    gctSIGNAL exitSIG;
#endif

#endif
};


/* Structure that defines a display. */
struct __DRIDisplayRec
{
    gctINT                         drmFd;
    Display                        *dpy;
    drm_sarea_t                    *pSAREA;
    gctSIZE_T                      physicalAddr;
    gctPOINTER                     linearAddr;
    gctINT                         fbSize;
    gctINT                         stride;
    gctINT                         width;
    gctINT                         height;
    gceSURF_FORMAT                 format;
    gctINT                         screen;
    gctINT                         bpp;

    gcoSURF                        renderTarget;

    __DRIcontextPriv * activeContext;
    __DRIdrawablePriv * activeDrawable;

    __DRIcontextPriv * contextStack;
    __DRIdrawablePriv * drawableStack;
};

/* Structure that defines a window. */
struct _DRIWindow
{
    __DRIDisplay*      display;
    gctUINT            offset;
    gctINT             x, y;
    gctINT             width;
    gctINT             height;
    /* Color format. */
    gceSURF_FORMAT     format;
};

static halKeyMap keys[] =
{
    /* 00 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 01 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 02 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 03 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 04 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 05 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 06 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 07 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 08 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 09 */ { HAL_ESCAPE,          HAL_UNKNOWN },
    /* 0A */ { HAL_1,               HAL_UNKNOWN },
    /* 0B */ { HAL_2,               HAL_UNKNOWN },
    /* 0C */ { HAL_3,               HAL_UNKNOWN },
    /* 0D */ { HAL_4,               HAL_UNKNOWN },
    /* 0E */ { HAL_5,               HAL_UNKNOWN },
    /* 0F */ { HAL_6,               HAL_UNKNOWN },
    /* 10 */ { HAL_7,               HAL_UNKNOWN },
    /* 11 */ { HAL_8,               HAL_UNKNOWN },
    /* 12 */ { HAL_9,               HAL_UNKNOWN },
    /* 13 */ { HAL_0,               HAL_UNKNOWN },
    /* 14 */ { HAL_HYPHEN,          HAL_UNKNOWN },
    /* 15 */ { HAL_EQUAL,           HAL_UNKNOWN },
    /* 16 */ { HAL_BACKSPACE,       HAL_UNKNOWN },
    /* 17 */ { HAL_TAB,             HAL_UNKNOWN },
    /* 18 */ { HAL_Q,               HAL_UNKNOWN },
    /* 19 */ { HAL_W,               HAL_UNKNOWN },
    /* 1A */ { HAL_E,               HAL_UNKNOWN },
    /* 1B */ { HAL_R,               HAL_UNKNOWN },
    /* 1C */ { HAL_T,               HAL_UNKNOWN },
    /* 1D */ { HAL_Y,               HAL_UNKNOWN },
    /* 1E */ { HAL_U,               HAL_UNKNOWN },
    /* 1F */ { HAL_I,               HAL_UNKNOWN },
    /* 20 */ { HAL_O,               HAL_UNKNOWN },
    /* 21 */ { HAL_P,               HAL_UNKNOWN },
    /* 22 */ { HAL_LBRACKET,        HAL_UNKNOWN },
    /* 23 */ { HAL_RBRACKET,        HAL_UNKNOWN },
    /* 24 */ { HAL_ENTER,           HAL_UNKNOWN },
    /* 25 */ { HAL_LCTRL,           HAL_UNKNOWN },
    /* 26 */ { HAL_A,               HAL_UNKNOWN },
    /* 27 */ { HAL_S,               HAL_UNKNOWN },
    /* 28 */ { HAL_D,               HAL_UNKNOWN },
    /* 29 */ { HAL_F,               HAL_UNKNOWN },
    /* 2A */ { HAL_G,               HAL_UNKNOWN },
    /* 2B */ { HAL_H,               HAL_UNKNOWN },
    /* 2C */ { HAL_J,               HAL_UNKNOWN },
    /* 2D */ { HAL_K,               HAL_UNKNOWN },
    /* 2E */ { HAL_L,               HAL_UNKNOWN },
    /* 2F */ { HAL_SEMICOLON,       HAL_UNKNOWN },
    /* 30 */ { HAL_SINGLEQUOTE,     HAL_UNKNOWN },
    /* 31 */ { HAL_BACKQUOTE,       HAL_UNKNOWN },
    /* 32 */ { HAL_LSHIFT,          HAL_UNKNOWN },
    /* 33 */ { HAL_BACKSLASH,       HAL_UNKNOWN },
    /* 34 */ { HAL_Z,               HAL_UNKNOWN },
    /* 35 */ { HAL_X,               HAL_UNKNOWN },
    /* 36 */ { HAL_C,               HAL_UNKNOWN },
    /* 37 */ { HAL_V,               HAL_UNKNOWN },
    /* 38 */ { HAL_B,               HAL_UNKNOWN },
    /* 39 */ { HAL_N,               HAL_UNKNOWN },
    /* 3A */ { HAL_M,               HAL_UNKNOWN },
    /* 3B */ { HAL_COMMA,           HAL_UNKNOWN },
    /* 3C */ { HAL_PERIOD,          HAL_UNKNOWN },
    /* 3D */ { HAL_SLASH,           HAL_UNKNOWN },
    /* 3E */ { HAL_RSHIFT,          HAL_UNKNOWN },
    /* 3F */ { HAL_PAD_ASTERISK,    HAL_UNKNOWN },
    /* 40 */ { HAL_LALT,            HAL_UNKNOWN },
    /* 41 */ { HAL_SPACE,           HAL_UNKNOWN },
    /* 42 */ { HAL_CAPSLOCK,        HAL_UNKNOWN },
    /* 43 */ { HAL_F1,              HAL_UNKNOWN },
    /* 44 */ { HAL_F2,              HAL_UNKNOWN },
    /* 45 */ { HAL_F3,              HAL_UNKNOWN },
    /* 46 */ { HAL_F4,              HAL_UNKNOWN },
    /* 47 */ { HAL_F5,              HAL_UNKNOWN },
    /* 48 */ { HAL_F6,              HAL_UNKNOWN },
    /* 49 */ { HAL_F7,              HAL_UNKNOWN },
    /* 4A */ { HAL_F8,              HAL_UNKNOWN },
    /* 4B */ { HAL_F9,              HAL_UNKNOWN },
    /* 4C */ { HAL_F10,             HAL_UNKNOWN },
    /* 4D */ { HAL_NUMLOCK,         HAL_UNKNOWN },
    /* 4E */ { HAL_SCROLLLOCK,      HAL_UNKNOWN },
    /* 4F */ { HAL_PAD_7,           HAL_UNKNOWN },
    /* 50 */ { HAL_PAD_8,           HAL_UNKNOWN },
    /* 51 */ { HAL_PAD_9,           HAL_UNKNOWN },
    /* 52 */ { HAL_PAD_HYPHEN,      HAL_UNKNOWN },
    /* 53 */ { HAL_PAD_4,           HAL_UNKNOWN },
    /* 54 */ { HAL_PAD_5,           HAL_UNKNOWN },
    /* 55 */ { HAL_PAD_6,           HAL_UNKNOWN },
    /* 56 */ { HAL_PAD_PLUS,        HAL_UNKNOWN },
    /* 57 */ { HAL_PAD_1,           HAL_UNKNOWN },
    /* 58 */ { HAL_PAD_2,           HAL_UNKNOWN },
    /* 59 */ { HAL_PAD_3,           HAL_UNKNOWN },
    /* 5A */ { HAL_PAD_0,           HAL_UNKNOWN },
    /* 5B */ { HAL_PAD_PERIOD,      HAL_UNKNOWN },
    /* 5C */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 5D */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 5E */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 5F */ { HAL_F11,             HAL_UNKNOWN },
    /* 60 */ { HAL_F12,             HAL_UNKNOWN },
    /* 61 */ { HAL_HOME,            HAL_UNKNOWN },
    /* 62 */ { HAL_UP,              HAL_UNKNOWN },
    /* 63 */ { HAL_PGUP,            HAL_UNKNOWN },
    /* 64 */ { HAL_LEFT,            HAL_UNKNOWN },
    /* 65 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 66 */ { HAL_RIGHT,           HAL_UNKNOWN },
    /* 67 */ { HAL_END,             HAL_UNKNOWN },
    /* 68 */ { HAL_DOWN,            HAL_UNKNOWN },
    /* 69 */ { HAL_PGDN,            HAL_UNKNOWN },
    /* 6A */ { HAL_INSERT,          HAL_UNKNOWN },
    /* 6B */ { HAL_DELETE,          HAL_UNKNOWN },
    /* 6C */ { HAL_PAD_ENTER,       HAL_UNKNOWN },
    /* 6D */ { HAL_RCTRL,           HAL_UNKNOWN },
    /* 6E */ { HAL_BREAK,           HAL_UNKNOWN },
    /* 6F */ { HAL_PRNTSCRN,        HAL_UNKNOWN },
    /* 70 */ { HAL_PAD_SLASH,       HAL_UNKNOWN },
    /* 71 */ { HAL_RALT,            HAL_UNKNOWN },
    /* 72 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 73 */ { HAL_LWINDOW,         HAL_UNKNOWN },
    /* 74 */ { HAL_RWINDOW,         HAL_UNKNOWN },
    /* 75 */ { HAL_MENU,            HAL_UNKNOWN },
    /* 76 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 77 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 78 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 79 */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7A */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7B */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7C */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7D */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7E */ { HAL_UNKNOWN,         HAL_UNKNOWN },
    /* 7F */ { HAL_UNKNOWN,         HAL_UNKNOWN },
};

#define VIV_EXT
#if defined(VIV_EXT)

#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

#define X_VIVEXTQueryVersion            0
#define X_VIVEXTPixmapPhysaddr          1
#define X_VIVEXTDrawableFlush           2
#define X_VIVEXTDrawableInfo            3
#define X_VIVEXTFULLScreenInfo          4


#define VIVEXTNumberEvents              0

#define VIVEXTClientNotLocal            0
#define VIVEXTOperationNotSupported     1
#define VIVEXTNumberErrors        (VIVEXTOperationNotSupported + 1)



#define VIVEXTNAME "vivext"


#define VIVEXT_MAJOR_VERSION    1
#define VIVEXT_MINOR_VERSION    0
#define VIVEXT_PATCH_VERSION    0

typedef struct _VIVEXTQueryVersion {
    CARD8    reqType;
    CARD8    vivEXTReqType;
    CARD16    length B16;
} xVIVEXTQueryVersionReq;
#define sz_xVIVEXTQueryVersionReq    4


typedef struct {
    BYTE      type;/* X_Reply */
    BYTE      pad1;
    CARD16    sequenceNumber B16;
    CARD32    length B32;
    CARD16    majorVersion B16;        /* major version of vivEXT protocol */
    CARD16    minorVersion B16;        /* minor version of vivEXT protocol */
    CARD32    patchVersion B32;        /* patch version of vivEXT protocol */
    CARD32    pad3 B32;
    CARD32    pad4 B32;
    CARD32    pad5 B32;
    CARD32    pad6 B32;
} xVIVEXTQueryVersionReply;
#define sz_xVIVEXTQueryVersionReply    32


typedef struct _VIVEXTDrawableFlush {
    CARD8    reqType;        /* always vivEXTReqCode */
    CARD8    vivEXTReqType;        /* always X_vivEXTDrawableFlush */
    CARD16    length B16;
    CARD32    screen B32;
    CARD32    drawable B32;
} xVIVEXTDrawableFlushReq;
#define sz_xVIVEXTDrawableFlushReq    12

typedef struct _VIVEXTDrawableInfo {
    CARD8    reqType;
    CARD8    vivEXTReqType;
    CARD16    length B16;
    CARD32    screen B32;
    CARD32    drawable B32;
} xVIVEXTDrawableInfoReq;
#define sz_xVIVEXTDrawableInfoReq    12

typedef struct {
    BYTE    type;/* X_Reply */
    BYTE    pad1;
    CARD16    sequenceNumber B16;
    CARD32    length B32;
    INT16    drawableX B16;
    INT16    drawableY B16;
    INT16    drawableWidth B16;
    INT16    drawableHeight B16;
    CARD32    numClipRects B32;
    INT16       relX B16;
    INT16       relY B16;
    CARD32      alignedWidth B32;
    CARD32      alignedHeight B32;
    CARD32      stride B32;
    CARD32      nodeName B32;
    CARD32      phyAddress B32;
} xVIVEXTDrawableInfoReply;

#define sz_xVIVEXTDrawableInfoReply    44


typedef struct _VIVEXTFULLScreenInfo {
    CARD8    reqType;
    CARD8    vivEXTReqType;
    CARD16    length B16;
    CARD32    screen B32;
    CARD32    drawable B32;
} xVIVEXTFULLScreenInfoReq;
#define sz_xVIVEXTFULLScreenInfoReq    12

typedef struct {
    BYTE    type;            /* X_Reply */
    BYTE    pad1;
    CARD16    sequenceNumber B16;
    CARD32    length B32;
    CARD32    fullscreenCovered B32;    /* if fullscreen is covered by windows, set to 1 otherwise 0 */
    CARD32    pad3 B32;
    CARD32    pad4 B32;
    CARD32    pad5 B32;
    CARD32    pad6 B32;
    CARD32    pad7 B32;        /* bytes 29-32 */
} xVIVEXTFULLScreenInfoReply;
#define    sz_xVIVEXTFULLScreenInfoReply 32


static XExtensionInfo _VIVEXT_info_data;
static XExtensionInfo *VIVEXT_info = &_VIVEXT_info_data;
static char *VIVEXT_extension_name = VIVEXTNAME;

#define VIVEXTCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, VIVEXT_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *                           private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *extCodes);
static /* const */ XExtensionHooks VIVEXT_extension_hooks = {
    NULL,                                /* create_gc */
    NULL,                                /* copy_gc */
    NULL,                                /* flush_gc */
    NULL,                                /* free_gc */
    NULL,                                /* create_font */
    NULL,                                /* free_font */
    close_display,                        /* close_display */
    NULL,                                /* wire_to_event */
    NULL,                                /* event_to_wire */
    NULL,                                /* error */
    NULL,                                /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, VIVEXT_info,
                                   VIVEXT_extension_name,
                                   &VIVEXT_extension_hooks,
                                   0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, VIVEXT_info)

static Bool VIVEXTDrawableFlush(Display *dpy, unsigned int screen, unsigned int drawable)
{
    XExtDisplayInfo *info = find_display (dpy);
    xVIVEXTDrawableFlushReq *req;

    VIVEXTCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(VIVEXTDrawableFlush, req);
    req->reqType = info->codes->major_opcode;
    req->vivEXTReqType = X_VIVEXTDrawableFlush;
    req->screen = screen;
    req->drawable = drawable;

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}
static Bool VIVEXTDrawableInfo(Display* dpy, int screen, Drawable drawable,
    int* X, int* Y, int* W, int* H,
    int* numClipRects, drm_clip_rect_t ** pClipRects,
    int* relX,
    int* relY,
    unsigned int * alignedWidth,
    unsigned int * alignedHeight,
    unsigned int * stride,
    unsigned int * nodeName,
    unsigned int * phyAddress
)
{

    XExtDisplayInfo *info = find_display (dpy);
    xVIVEXTDrawableInfoReply rep;
    xVIVEXTDrawableInfoReq *req;
    int extranums = 0;

    VIVEXTCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(VIVEXTDrawableInfo, req);
    req->reqType = info->codes->major_opcode;
    req->vivEXTReqType = X_VIVEXTDrawableInfo;
    req->screen = screen;
    req->drawable = drawable;

    extranums = ( sizeof(xVIVEXTDrawableInfoReply) - 32 ) / 4;

    if (!_XReply(dpy, (xReply *)&rep, extranums , xFalse))
    {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    *X = (int)rep.drawableX;
    *Y = (int)rep.drawableY;
    *W = (int)rep.drawableWidth;
    *H = (int)rep.drawableHeight;
    *numClipRects = rep.numClipRects;
    *alignedWidth = (unsigned int)rep.alignedWidth;
    *alignedHeight = (unsigned int)rep.alignedHeight;
    *stride = (unsigned int)rep.stride;
    *nodeName = (unsigned int)rep.nodeName;
    *phyAddress = (unsigned int)rep.phyAddress;

    *relX = rep.relX;
    *relY = rep.relY;

    if (*numClipRects) {
       int len = sizeof(drm_clip_rect_t) * (*numClipRects);

       *pClipRects = (drm_clip_rect_t *)Xcalloc(len, 1);
       if (*pClipRects)
          _XRead(dpy, (char*)*pClipRects, len);
    } else {
        *pClipRects = NULL;
    }

    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}

static Bool VIVEXTFULLScreenInfo(Display* dpy, int screen, Drawable drawable)
{
    XExtDisplayInfo *info = find_display (dpy);
    xVIVEXTFULLScreenInfoReply rep;
    xVIVEXTFULLScreenInfoReq *req;
    int ret = 0;

    VIVEXTCheckExtension (dpy, info, False);


    LockDisplay(dpy);
    GetReq(VIVEXTFULLScreenInfo, req);
    req->reqType = info->codes->major_opcode;
    req->vivEXTReqType = X_VIVEXTFULLScreenInfo;
    req->screen = screen;
    req->drawable = drawable;

    if (!_XReply(dpy, (xReply *)&rep, 0 , xFalse))
    {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    ret = rep.fullscreenCovered;

    UnlockDisplay(dpy);
    SyncHandle();

    return (Bool)ret;
}


#endif


#if defined(LINUX)
static int _terminate = 0;

static void
vdkSignalHandler(
    int SigNum
    )
{
    signal(SIGINT,  vdkSignalHandler);
    signal(SIGQUIT, vdkSignalHandler);

    _terminate = 1;
}
#endif



static gceSTATUS _CreateOnScreenSurfaceWrapper(
    __DRIdrawablePriv * drawable,
      gceSURF_FORMAT Format
)
{
    __DRIDisplay * display;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("drawable=0x%x", drawable);

    display = drawable->display;

    if ((display->width == drawable->w) && (display->height == drawable->h)) {
        drawable->fullScreenMode = gcvTRUE;
    }

    if ((display->physicalAddr) && (drawable->fullScreenMode)) {
        do {
                gcmERR_BREAK(gcoSURF_ConstructWrapper(gcvNULL,
                    &display->renderTarget));

                gcmERR_BREAK(gcoSURF_SetBuffer(display->renderTarget,
                    gcvSURF_BITMAP,
                    Format,
                    display->width * display->bpp,
                    display->linearAddr,
                    display->physicalAddr
                    ));

                gcmERR_BREAK(gcoSURF_SetWindow(display->renderTarget,
                    0,
                    0,
                    display->width,
                    display->height));
        } while(gcvFALSE);
    }
    gcmFOOTER_NO();
    return status;
}

static gceSTATUS _DestroyOnScreenSurfaceWrapper(
    __DRIDisplay * Display
)
{
    if (Display->renderTarget) {
        gcoSURF_Destroy(Display->renderTarget);
        Display->renderTarget = gcvNULL;
    }

    return gcvSTATUS_OK;
}

static gceSTATUS _LockVideoNode(
        IN gcoHAL Hal,
        IN gctUINT32 Node,
        OUT gctUINT32 *Address,
        OUT gctPOINTER *Memory)
{
    gceSTATUS status;
    gcsHAL_INTERFACE iface;

    gcmASSERT(Address != gcvNULL);
    gcmASSERT(Memory != gcvNULL);
    gcmASSERT(Node != 0);

    memset(&iface, 0, sizeof(gcsHAL_INTERFACE));

    iface.command = gcvHAL_LOCK_VIDEO_MEMORY;
    iface.u.LockVideoMemory.node = Node;
    iface.u.LockVideoMemory.cacheable = gcvFALSE;
    /* Call kernel API. */
    gcmONERROR(gcoHAL_Call(Hal, &iface));

    /* Get allocated node in video memory. */
    *Address = iface.u.LockVideoMemory.address;
    *Memory = gcmUINT64_TO_PTR(iface.u.LockVideoMemory.memory);
OnError:
    return status;
}

static gceSTATUS _UnlockVideoNode(
        IN gcoHAL Hal,
        IN gctUINT32 Node)
{
    gcsHAL_INTERFACE iface;
    gceSTATUS status;

    gcmASSERT(Node != 0);

    memset(&iface, 0, sizeof(gcsHAL_INTERFACE));
    iface.command = gcvHAL_UNLOCK_VIDEO_MEMORY;
    iface.u.UnlockVideoMemory.node = Node;
    iface.u.UnlockVideoMemory.type = gcvSURF_BITMAP;
    iface.u.UnlockVideoMemory.asynchroneous = gcvTRUE;

/*
    gcmONERROR(gcoHAL_Commit(Hal, gcvTRUE));
*/

    /* Call kernel API. */
    gcmONERROR(gcoOS_DeviceControl(
               gcvNULL,
               IOCTL_GCHAL_INTERFACE,
               &iface, gcmSIZEOF(iface),
               &iface, gcmSIZEOF(iface)));
    gcmONERROR(iface.status);

    gcmONERROR(iface.status);

    if (iface.u.UnlockVideoMemory.asynchroneous){
        iface.u.UnlockVideoMemory.asynchroneous = gcvFALSE;
        gcmONERROR(gcoHAL_ScheduleEvent(Hal,&iface));
    }

OnError:
    return status;
}

static gceSTATUS _FreeVideoNode(
        IN gcoHAL Hal,
        IN gctUINT32 Node) {
    gcsHAL_INTERFACE iface;

    gcmASSERT(Node != 0);

    iface.command = gcvHAL_RELEASE_VIDEO_MEMORY;
    iface.u.ReleaseVideoMemory.node = Node;

    /* Call kernel API. */
    return gcoHAL_Call(Hal, &iface);
}

#if defined(DRI_PIXMAPRENDER) || defined(DRI_PIXMAPRENDER_ASYNC)
static void _createPixmapInfo(
        __DRIdrawablePriv *dridrawable,
        HALNativeWindowType Drawable,
        Pixmap *backPixmap,
        GC *xgc,
        gcoSURF *pixWrapSurf,
        gctUINT32 *backNode,
        gceSURF_TYPE surftype,
        gceSURF_FORMAT surfformat
        )
{
    int x,y,w,h;
    int xx, yy;
    unsigned int ww, hh, bb;
    int xw,yw;
    int numRects;
    drm_clip_rect_t *pClipRects;
    unsigned int alignedW;
    unsigned int alignedH;
    unsigned int stride;
    unsigned int nodeName;
    unsigned int physAddr;
    unsigned int depth = 24;
    Window    root;
    gctPOINTER  destLogicalAddr[3] = {0, 0, 0};
    gctUINT   destPhys[3] = {0,0,0};
    gceSTATUS status = gcvSTATUS_OK;

    __DRIDisplay * display;
    __DRIdrawablePriv *pdp=dridrawable;


    display = dridrawable->display;
    XGetGeometry(display->dpy, Drawable, &root, &xx, &yy, &ww, &hh, &bb, &depth);

    *backPixmap = XCreatePixmap(display->dpy, Drawable, dridrawable->w, dridrawable->h, depth);
    *xgc = XCreateGC(display->dpy, Drawable, 0, NULL);
    XFlush(display->dpy);

    VIVEXTDrawableInfo(display->dpy, pdp->screen, *backPixmap,
                    &x, &y, &w, &h,
                    &numRects, &pClipRects,
                    &xw,
                    &yw,
                    (unsigned int *)&alignedW,
                    (unsigned int *)&alignedH,
                    &stride,
                    &nodeName,
                    (unsigned int *)&physAddr);

    if ( nodeName )
        gcoHAL_ImportVideoMemory((gctUINT32)nodeName, (gctUINT32 *)backNode);
    else
        *pixWrapSurf = gcvNULL;

    gcmONERROR(_LockVideoNode(0, *backNode, &destPhys[0], &destLogicalAddr[0]));

    do {
        /* Construct a wrapper around the pixmap.  */
        gcmERR_BREAK(gcoSURF_ConstructWrapper(
        gcvNULL,
        pixWrapSurf
        ));

        /* Set the underlying frame buffer surface. */
        gcmERR_BREAK(gcoSURF_SetBuffer(
        *pixWrapSurf,
        surftype,
        surfformat,
        stride,
        destLogicalAddr[0],
        (gctUINT32)physAddr
        ));

        /* Set the window. */
        gcmERR_BREAK(gcoSURF_SetWindow(
        *pixWrapSurf,
        0,
        0,
        dridrawable->w,
        dridrawable->h
        ));
        return ;
    } while(0);


    gcmONERROR(_UnlockVideoNode(0, *backNode));
    gcmONERROR(_FreeVideoNode(0, *backNode));
    *backNode = 0;

OnError:

    if ( *backNode )
     _FreeVideoNode(0, *backNode);


    if ( *pixWrapSurf )
    gcoSURF_Destroy(*pixWrapSurf);

    if ( (*backPixmap) )
        XFreePixmap(display->dpy, *backPixmap);
    if ( (*xgc) )
        XFreeGC(display->dpy, *xgc);

    *backPixmap = (Pixmap)0;
    *xgc =(GC)0;
    *pixWrapSurf = (gcoSURF)0;
    fprintf(stderr, "Warning::Backpixmap can't be created for the current Drawable\n");
}

static void _destroyPixmapInfo(
    __DRIDisplay * display,
    Pixmap backPixmap,
    GC xgc,
    gcoSURF pixWrapSurf,
    gctUINT32 backNode)
{

    if ( backNode )
    _UnlockVideoNode(0, backNode);

    if ( backNode )
    _FreeVideoNode(0, backNode);

    if ( (gctUINT32)backPixmap )
        XFreePixmap(display->dpy, backPixmap);


    if ( (gctUINT32)xgc )
        XFreeGC(display->dpy, xgc);

    if ( pixWrapSurf )
        gcoSURF_Destroy(pixWrapSurf);

}
#endif

#ifdef DRI_PIXMAPRENDER_ASYNC
static void renderThread(void* refData)
{

    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable = (__DRIdrawablePriv *)refData;

    int i = 0;


    if ( drawable )
        context = drawable->contextPriv;
    else
        return ;

    while( 1 && drawable->frameMutex ) {


        if (gcmIS_ERROR(gcoOS_WaitSignal(gcvNULL, drawable->busySIG, gcvINFINITE)))
        {
            break;
        }

        if (gcmIS_SUCCESS(gcoOS_WaitSignal(gcvNULL, drawable->stopSIG, 0)))
        {
            gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex,0);
            gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
            break;
        }

        gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);

        if ( drawable->busyNum > 0 )
        {
            for ( i = 0; i < NUM_ASYNCFRAME; i++)
            {
                if ( drawable->ascframe[i].busy )
                {
                    gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);

                    if (gcmIS_ERROR(gcoOS_WaitSignal(gcvNULL, drawable->ascframe[i].signal, gcvINFINITE)))
                    {
                        gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);
                        continue;
                    }


                    LINUX_UNLOCK_FRAMEBUFFER(context);

                    gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);
                    drawable->ascframe[i].busy = _FRAME_FREE;
                    drawable->busyNum--;
                    if (drawable->busyNum<0) drawable->busyNum = 0;
                }
            }

        }
        gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);

    }

    /*gcoHAL_Commit(gcvNULL, gcvTRUE);*/

    if ( drawable->exitSIG)
    gcoOS_Signal(gcvNULL, drawable->exitSIG, gcvTRUE);

}


static void asyncRenderKill(__DRIdrawablePriv * drawable)
{
    if ( drawable == gcvNULL || drawable->busySIG == gcvNULL || drawable->stopSIG == gcvNULL || drawable->exitSIG == gcvNULL || drawable->frameMutex == gcvNULL )
    return ;

    XSync(drawable->display->dpy, False);

    while(1) {
        gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);
        if (drawable->busyNum == 0)
        {
            drawable->curIndex = -1;
            gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
            break;
        }
        gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
    }

    gcoOS_Signal(gcvNULL, drawable->busySIG, gcvTRUE);
    gcoOS_Signal(gcvNULL, drawable->stopSIG, gcvTRUE);

    while(gcmIS_ERROR(gcoOS_WaitSignal(gcvNULL, drawable->exitSIG, 0)))
    {
        gcoOS_Signal(gcvNULL, drawable->busySIG, gcvTRUE);
        gcoOS_Signal(gcvNULL, drawable->stopSIG, gcvTRUE);
    }

    gcoOS_DestroySignal(gcvNULL, drawable->stopSIG);
    gcoOS_DestroySignal(gcvNULL, drawable->exitSIG);
    gcoOS_DestroySignal(gcvNULL, drawable->busySIG);

    drawable->stopSIG = gcvNULL;
    drawable->busySIG = gcvNULL;
    drawable->exitSIG = gcvNULL;
}

static void asyncRenderDestroy(__DRIdrawablePriv * drawable)
{

    __DRIDisplay * display;
    int i = 0;

    if ( drawable == gcvNULL ) return;

    display = drawable->display;

    /*gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);*/
    for ( i = 0; i < NUM_ASYNCFRAME; i++)
    {
        if ( drawable->ascframe[i].backPixmap && drawable->ascframe[i].pixWrapSurf )
            _destroyPixmapInfo(display, drawable->ascframe[i].backPixmap, drawable->ascframe[i].xgc, drawable->ascframe[i].pixWrapSurf, drawable->ascframe[i].backNode);

        if ( drawable->ascframe[i].signal )
            gcoOS_DestroySignal(gcvNULL, drawable->ascframe[i].signal);

        drawable->ascframe[i].signal = gcvNULL;
        drawable->ascframe[i].w = 0;
        drawable->ascframe[i].h = 0;

        drawable->ascframe[i].backPixmap = 0;
        drawable->ascframe[i].pixWrapSurf = gcvNULL;
    }
    /*gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);*/

}

static void asyncRenderSurf(__DRIdrawablePriv * drawable, int index, gcoSURF *renderSurf)
{

    if ( index < 0 || index >= NUM_ASYNCFRAME || drawable->frameMutex == gcvNULL )
        *renderSurf = gcvNULL;
    else {

        if (drawable->stopSIG == gcvNULL)
        *renderSurf = gcvNULL;
        else
        *renderSurf = drawable->ascframe[index].pixWrapSurf;
    }
}

static int pickFrameIndex(__DRIdrawablePriv * drawable, int *index)
{
    int crtframe = _NOT_SET;
    int i = 0;

    gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);


    if ( drawable->w != drawable->ascframe[0].w || drawable->h !=drawable->ascframe[0].h)
    {
        gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
        /* stop thread */
        /* destroy old frames */
        /* set stat to recreate frames */
        asyncRenderKill(drawable);
        asyncRenderDestroy(drawable);
        crtframe = _TO_UPDATEFRAME;

    } else {

        for(i = ( drawable->curIndex + 1 )%NUM_ASYNCFRAME ; i < NUM_ASYNCFRAME; i++)
        {
            if ( drawable->ascframe[i].busy == _FRAME_FREE ) {
                *index = i;
                drawable->curIndex = i;
                break;
            }
        }

       if ( i >= NUM_ASYNCFRAME )
       {
            /* release mutex to give the renderthread chance to reset busynum */
            gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
            while(1) {
                gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);
                    if (drawable->busyNum == 0)
                    {
                        *index = 0;
                        drawable->curIndex = 0;
                        break;
                    }
                gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);
                }
       }

        gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);

    }
    return crtframe;
}

static void asyncRenderBegin(__DRIdrawablePriv * drawable, HALNativeWindowType Drawable, int *index)
{
    gceSTATUS status = gcvSTATUS_OK;
    int crtframe = _NOT_SET;
    int i = 0;

    if ( drawable->frameMutex == gcvNULL ){
        gcoOS_CreateMutex(
            gcvNULL,
            &drawable->frameMutex
            );

        if ( drawable->frameMutex == gcvNULL ){
            *index = -1;
            return ;
        }
        crtframe = _TO_INITFRAME;
    }

    if ( crtframe == _NOT_SET) {

        crtframe = pickFrameIndex(drawable, index);

    }

    if ( crtframe == _TO_INITFRAME || crtframe == _TO_UPDATEFRAME ) {
        drawable->busyNum = 0;
        gcoOS_CreateSignal(gcvNULL, gcvFALSE, &drawable->busySIG);
        gcoOS_CreateSignal(gcvNULL, gcvFALSE, &drawable->stopSIG);
        gcoOS_CreateSignal(gcvNULL, gcvFALSE, &drawable->exitSIG);

        gcoOS_CreateThread(gcvNULL, (gcTHREAD_ROUTINE)renderThread, drawable, &drawable->workerThread);


        for ( i = 0; i < NUM_ASYNCFRAME; i++)
        {
            drawable->ascframe[i].w = drawable->w;
            drawable->ascframe[i].h = drawable->h;
            gcmERR_BREAK(gcoOS_CreateSignal(
            gcvNULL,
            gcvFALSE,
            &drawable->ascframe[i].signal
            ));

            drawable->ascframe[i].dridrawable = drawable;
            drawable->ascframe[i].surftype = drawable->surftype;
            drawable->ascframe[i].surfformat = drawable->surfformat;
            drawable->ascframe[i].pixWrapSurf = gcvNULL;
            drawable->ascframe[i].Drawable = Drawable;
            drawable->ascframe[i].backNode = 0;
            _createPixmapInfo(drawable, Drawable, &(drawable->ascframe[i].backPixmap), &(drawable->ascframe[i].xgc), &(drawable->ascframe[i].pixWrapSurf), &drawable->ascframe[i].backNode, drawable->surftype, drawable->surfformat);
            if ( drawable->ascframe[i].pixWrapSurf == gcvNULL)
                break;
        }

       if ( i < NUM_ASYNCFRAME )
        *index = -1;
       else {
        *index = 0;
        drawable->curIndex = 0;
       }
    }

    if ( drawable->frameMutex )
    gcoOS_AcquireMutex(gcvNULL, drawable->frameMutex, gcvINFINITE);
}


static void asyncRenderEnd(__DRIdrawablePriv * drawable, HALNativeWindowType Drawable, int index)
{
    gcsHAL_INTERFACE iface;
    __DRIDisplay * display;

    if ( drawable->frameMutex == gcvNULL )
        return ;

    if ( drawable == gcvNULL || index < -1 )
        goto ENDUNLOCK ;

    if (drawable->ascframe[index].signal == gcvNULL
        || drawable->ascframe[index].backPixmap == 0
        || drawable->stopSIG == gcvNULL
        || drawable->exitSIG == gcvNULL
        || drawable->busySIG == gcvNULL )
        goto ENDUNLOCK;

    display = drawable->display;

    drawable->ascframe[index].busy = _FRAME_BUSY;
    drawable->ascframe[index].Drawable = Drawable;
    drawable->busyNum ++;

    iface.command            = gcvHAL_SIGNAL;
    iface.u.Signal.signal    = gcmPTR_TO_UINT64(drawable->ascframe[index].signal);
    iface.u.Signal.auxSignal = 0;
    iface.u.Signal.process = gcmPTR_TO_UINT64(gcoOS_GetCurrentProcessID());
    iface.u.Signal.fromWhere = gcvKERNEL_PIXEL;

    /* Schedule the event. */
    gcoHAL_ScheduleEvent(gcvNULL, &iface);

    if ( drawable->busySIG)
    gcoOS_Signal(gcvNULL,drawable->busySIG, gcvTRUE);

    XCopyArea(display->dpy, drawable->ascframe[index].backPixmap, drawable->ascframe[index].Drawable, drawable->ascframe[index].xgc, 0, 0, drawable->ascframe[index].w, drawable->ascframe[index].h, 0, 0);

    XFlush(display->dpy);

ENDUNLOCK:
    gcoOS_ReleaseMutex(gcvNULL, drawable->frameMutex);

    gcoHAL_Commit(gcvNULL, gcvFALSE);
}

#endif


#ifdef DRI_PIXMAPRENDER
static void _CreateBackPixmapForDrawable(__DRIdrawablePriv * drawable)
{
    _createPixmapInfo(drawable, drawable->drawable, &drawable->backPixmap, &drawable->xgc, &drawable->pixWrapSurf, &drawable->backPixmapNode, drawable->surftype, drawable->surfformat);
}

static void _DestroyBackPixmapForDrawable(__DRIdrawablePriv * drawable)
{
    __DRIDisplay * display;
    display = drawable->display;

    if ( drawable->pixWrapSurf )
        _destroyPixmapInfo(display, drawable->backPixmap, drawable->xgc, drawable->pixWrapSurf, drawable->backPixmapNode);

    drawable->backPixmapNode = 0;
    drawable->backPixmap = (Pixmap)0;
    drawable->pixWrapSurf = gcvNULL;
    drawable->xgc = (GC) 0;

}
#endif

typedef struct _M_Pixmap
{
    Display *dpy;
    Drawable pixmap;
    gctINT mapped;
    gctPOINTER destLogicalAddr;
    gctPOINTER phyAddr;
    gctINT stride;
    gctPOINTER next;
}MPIXMAP,*PMPIXMAP;

static PMPIXMAP _vpixmaphead = gcvNULL;
static MPIXMAP _cachepixmap = {gcvNULL, (Drawable)0, 0, gcvNULL, gcvNULL, 0, gcvNULL};

static gctBOOL _pixmapMapped(Drawable pixmap, gctPOINTER *destLogicalAddr, gctPOINTER *phyAddr, Display **dpy, gctINT *stride)
{
    PMPIXMAP pixmapnode = gcvNULL;
    if ( _vpixmaphead == gcvNULL )
        return gcvFALSE;

    if ( _cachepixmap.pixmap == pixmap)
    {
        *destLogicalAddr = _cachepixmap.destLogicalAddr;
        *phyAddr = _cachepixmap.phyAddr;
        *dpy = _cachepixmap.dpy;
        *stride = _cachepixmap.stride;
        return gcvTRUE;
    }

    pixmapnode = _vpixmaphead;
    while( pixmapnode )
    {
        if ( pixmapnode->pixmap == pixmap )
        {
            if ( pixmapnode->mapped)
            {
                *destLogicalAddr = pixmapnode->destLogicalAddr;
                *phyAddr = pixmapnode->phyAddr;
                *dpy = pixmapnode->dpy;
                *stride = pixmapnode->stride;
                return gcvTRUE;
            }
            else
                return gcvFALSE;
        }
        pixmapnode = (PMPIXMAP)pixmapnode->next;
    }

    return gcvFALSE;

}

static gctBOOL _setPixmapMapped(Drawable pixmap, gctPOINTER destLogicalAddr, gctPOINTER phyAddr, Display *dpy, gctINT stride)
{
    PMPIXMAP pixmapnode = gcvNULL;
    PMPIXMAP prepixmapnode = gcvNULL;

    _cachepixmap.destLogicalAddr = destLogicalAddr;
    _cachepixmap.phyAddr = phyAddr;
    _cachepixmap.dpy = dpy;
    _cachepixmap.pixmap = pixmap;
    _cachepixmap.mapped = 1;
    _cachepixmap.stride = stride;

    if ( _vpixmaphead == gcvNULL )
    {
        _vpixmaphead = (PMPIXMAP)malloc(sizeof(MPIXMAP));
        _vpixmaphead->mapped = 1;
        _vpixmaphead->pixmap = pixmap;
        _vpixmaphead->dpy= dpy;
        _vpixmaphead->destLogicalAddr = destLogicalAddr;
        _vpixmaphead->phyAddr = phyAddr;
        _vpixmaphead->stride = stride;
        _vpixmaphead->next = gcvNULL;
        return gcvTRUE;
    }

    pixmapnode = _vpixmaphead;
    while( pixmapnode )
    {
        prepixmapnode = pixmapnode;
        if ( pixmapnode->pixmap == pixmap )
        {
            pixmapnode->mapped = 1;
            pixmapnode->destLogicalAddr = destLogicalAddr;
            pixmapnode->phyAddr = phyAddr;
            pixmapnode->stride = stride;
            return gcvTRUE;
        }
        pixmapnode = (PMPIXMAP)pixmapnode->next;
    }
    pixmapnode = (PMPIXMAP)malloc(sizeof(MPIXMAP));
    pixmapnode->mapped = 1;
    pixmapnode->pixmap = pixmap;
    pixmapnode->dpy= dpy;
    pixmapnode->destLogicalAddr = destLogicalAddr;
    pixmapnode->phyAddr = phyAddr;
    pixmapnode->stride = stride;
    pixmapnode->next = gcvNULL;
    prepixmapnode->next = pixmapnode;
    return gcvTRUE;
}

static gctBOOL _unSetPixmapMapped(Drawable pixmap)
{
    PMPIXMAP pixmapnode = gcvNULL;
    PMPIXMAP prepixmapnode = gcvNULL;

    _cachepixmap.pixmap = (Drawable)0;
    _cachepixmap.mapped = 0;

    if ( _vpixmaphead == gcvNULL )
        return gcvFALSE;

    pixmapnode = _vpixmaphead;
    while( pixmapnode )
    {
        if ( pixmapnode->pixmap == pixmap )
        {
            pixmapnode->mapped = 0;
            if ( pixmapnode == _vpixmaphead)
            _vpixmaphead = pixmapnode->next;
            else
            prepixmapnode->next = pixmapnode->next;
            free(pixmapnode);
            return gcvTRUE;
        }
        prepixmapnode = pixmapnode;
        pixmapnode = (PMPIXMAP)pixmapnode->next;
    }

    return gcvTRUE;
}

gceSTATUS _getPixmapDrawableInfo(Display *dpy, Drawable pixmap, gctUINT* videoNode, gctINT* s)
{
    int discarded;
    unsigned int uDiscarded;
    unsigned int nodeName;
    int numClipRects;
    drm_clip_rect_t* pClipRects;
    int stride = 0;
    int x;
    int y;
    int w;
    int h;
    int alignedw;
    int alignedh;


    if ( !VIVEXTDrawableInfo(dpy, DefaultScreen(dpy), pixmap,
                    &x, &y, &w, &h,
                    &numClipRects, &pClipRects,
                    &discarded,
                    &discarded,
                    (unsigned int *)&alignedw,
                    (unsigned int *)&alignedh,
                    (unsigned int *)&stride,
                    (unsigned int *)&nodeName,
                    (unsigned int *)&uDiscarded) )
    {
        *videoNode = 0;
        *s = 0;
        return gcvSTATUS_INVALID_ARGUMENT;
    }



    /* Extract the data needed for pixmaps from the variables set in dri.c */
    if ( nodeName )
    gcoHAL_ImportVideoMemory(nodeName, videoNode);
    else
    *videoNode = 0;

    *s = stride;
    return gcvSTATUS_OK;
}

void _UpdateDrawableInfo(__DRIdrawablePriv * drawable)
{
    __DRIDisplay * display;

    display = drawable->display;

    DRM_SPINUNLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);

    if(!XF86DRIGetDrawableInfo(display->dpy, drawable->screen, drawable->drawable,
                    &drawable->index, &drawable->lastStamp,
                    &drawable->x, &drawable->y, &drawable->w, &drawable->h,
                    &drawable->numClipRects, &drawable->pClipRects,
                    &drawable->backX,
                    &drawable->backY,
                    &drawable->numBackClipRects,
                    &drawable->pBackClipRects)) {
        /* Error -- eg the window may have been destroyed.  Keep going
         * with no cliprects.
         */
        drawable->pStamp = &drawable->lastStamp; /* prevent endless loop */
        drawable->numClipRects = 0;
        drawable->pClipRects = NULL;
        drawable->numBackClipRects = 0;
        drawable->pBackClipRects = NULL;
    }

    DRM_SPINLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
}

void _UpdateDrawableInfoDrawableInfo(__DRIdrawablePriv * drawable)
{

    __DRIdrawablePriv *pdp=drawable;
    __DRIDisplay * display;

    int x;
    int y;
    int w;
    int h;
    int numClipRects;
    drm_clip_rect_t *pClipRects;
    unsigned int stride;
    unsigned int nodeName = 0;

    display = drawable->display;

    DRM_SPINUNLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);

    /* Assign __DRIdrawablePrivate first */
    if (!XF86DRIGetDrawableInfo(display->dpy, pdp->screen, pdp->drawable,
                    &pdp->index, &pdp->lastStamp,
                    &x, &y, &w, &h,
                    &numClipRects, &pClipRects,
                    &pdp->backX,
                    &pdp->backY,
                    &pdp->numBackClipRects,
                    &pdp->pBackClipRects
                                    )) {
        /* Error -- eg the window may have been destroyed.  Keep going
         * with no cliprects.
         */
        pdp->pStamp = &pdp->lastStamp; /* prevent endless loop */
        pdp->numClipRects = 0;
        pdp->pClipRects = NULL;
        pdp->numBackClipRects = 0;
        pdp->pBackClipRects = NULL;
        goto ENDFLAG;
    }
    else
       pdp->pStamp = &(display->pSAREA->drawableTable[pdp->index].stamp);


    VIVEXTDrawableInfo(display->dpy, pdp->screen, pdp->drawable,
                    &pdp->x, &pdp->y, &pdp->w, &pdp->h,
                    &pdp->numClipRects, &pdp->pClipRects,
                    &pdp->xWOrigin,
                    &pdp->yWOrigin,
                    (unsigned int *)&pdp->wWidth,
                    (unsigned int *)&pdp->wHeight,
                    &stride,
                    &nodeName,
                    (unsigned int *)&pdp->backBufferPhysAddr);



    if ( nodeName )
    {

        if ( pdp->backNode )
            _FreeVideoNode(0, pdp->backNode);

        pdp->backNode = 0;
        pdp->nodeName = nodeName;

        gcoHAL_ImportVideoMemory((gctUINT32)nodeName, (gctUINT32 *)&pdp->backNode);
    }
    else
    pdp->backNode = 0;

ENDFLAG:
    DRM_SPINLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
}

void _FullScreenCovered(__DRIdrawablePriv * drawable)
{
    Bool ret;
    __DRIDisplay * display;

    display = drawable->display;
    drawable->fullscreenCovered = 0;
    DRM_SPINUNLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
    ret = VIVEXTFULLScreenInfo(display->dpy, drawable->screen, drawable->drawable);
    drawable->fullscreenCovered = (int)ret;
    DRM_SPINLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
}

/* Lock the hardware and validate drawable state.
 * LOCK_FRAMEBUFFER( gc ) and UNLOCK_FRAMEBUFFER( gc ) should be called whenever
 * OpenGL driver needs to access framebuffer hardware.
 */
void _VivGetLock(__DRIdrawablePriv * drawable)
{
    __DRIDisplay * display;
    gctINT width;
    gctINT height;

    display = drawable->display;
    width = drawable->w;
    height  = drawable->h;

    if (drawable) {
        drmGetLock( drawable->fd, drawable->contextPriv->hHWContext, 0 );
    }

    if (drawable)
    {
        DRI_VALIDATE_DRAWABLE_INFO( drawable );
        DRI_FULLSCREENCOVERED(drawable);

        if ((drawable->w != width) || (drawable->h != height))
        {

#ifdef DRI_PIXMAPRENDER
            _DestroyBackPixmapForDrawable(drawable);
            _CreateBackPixmapForDrawable(drawable);
#endif

            drawable->drawableResize = gcvTRUE;
            if ((display->width == width) && (display->height == height)) {
                drawable->fullScreenMode = gcvTRUE;
            } else {
                drawable->fullScreenMode = gcvFALSE;
            }
        }
    }

}

static void
_GetDisplayInfo(
    __DRIDisplay* display,
    int scrn)
{
    int   fd = -1;
    unsigned long hSAREA;
    unsigned long hFB;
    char *BusID;
/*
    const char * err_msg;
    const char * err_extra;
*/
    void * pSAREA = NULL;
    Display * dpy = display->dpy;
    int   status;
    void * priv;
    int priv_size;
    int junk;

    if (XF86DRIOpenConnection(dpy, scrn, &hSAREA, &BusID))
    {

        fd = drmOpen(NULL,BusID);

        XFree(BusID); /* No longer needed */
/*
        err_msg = "open DRM";
        err_extra = strerror( -fd );
*/
        if (fd >= 0)
        {

            drm_magic_t magic;
            display->drmFd = fd;
/*
            err_msg = "drmGetMagic";
            err_extra = NULL;
*/
            if (!drmGetMagic(fd, &magic))
            {
                /*err_msg = "XF86DRIAuthConnection";*/
                if (XF86DRIAuthConnection(dpy, scrn, magic))
                {
                    /*err_msg = "XF86DRIGetDeviceInfo";*/
                    if (XF86DRIGetDeviceInfo(dpy, scrn,
                         &hFB,
                         &junk,
                         &display->fbSize,
                         &display->stride,
                         &priv_size,
                         &priv))
                    {
                        /* Set on screen frame buffer physical address */
                        display->physicalAddr = hFB;
                        display->width = DisplayWidth(dpy, scrn);
                        display->height = DisplayHeight(dpy, scrn);
                        display->bpp = display->stride / display->width;
                        status = drmMap(fd, hFB, display->fbSize, &display->linearAddr);
                        if (status == 0)
                        {
                         /*
                            * Map the SAREA region.  Further mmap regions may be setup in
                            * each DRI driver's "createScreen" function.
                         */
                            status = drmMap(fd, hSAREA, SAREA_MAX, &pSAREA);
                            display->pSAREA = pSAREA;
                        }
                    }
                }
            }
        }
    }
}

__DRIcontextPriv * _FindContext(__DRIDisplay* display,
    IN gctPOINTER Context)
{
    gctBOOL found = gcvFALSE;
    __DRIcontextPriv * cur;

    cur = display->contextStack;

    while ((cur != NULL) && (!found)) {
        if (cur->eglContext == Context) {
            found = gcvTRUE;
        } else {
            cur = cur->next;
        }
    }
    return found ? cur : NULL;
}

__DRIdrawablePriv * _FindDrawable(__DRIDisplay* display,
    IN HALNativeWindowType Drawable)
{
    gctBOOL found = gcvFALSE;
    __DRIdrawablePriv * cur;

    cur = display->drawableStack;

    while ((cur != NULL) && (!found)) {
        if (cur->drawable == Drawable) {
            found = gcvTRUE;
        } else {
            cur = cur->next;
        }
    }
    return found ? cur: NULL;
}

/*******************************************************************************
** Display.
*/

void
_GetColorBitsInfoFromMask(
    gctSIZE_T Mask,
    gctUINT *Length,
    gctUINT *Offset
)
{
    gctINT start, end;
    size_t i;

    if (Length == NULL && Offset == NULL)
    {
        return;
    }

    if (Mask == 0)
    {
        start = 0;
        end   = 0;
    }
    else
    {
        start = end = -1;

        for (i = 0; i < (sizeof(Mask) * 8); i++)
        {
            if (start == -1)
            {
                if ((Mask & (1 << i)) > 0)
                {
                    start = i;
                }
            }
            else if ((Mask & (1 << i)) == 0)
            {
                end = i;
                break;
            }
        }

        if (end == -1)
        {
            end = i;
        }
    }

    if (Length != NULL)
    {
        *Length = end - start;
    }

    if (Offset != NULL)
    {
        *Offset = start;
    }
}

gceSTATUS
gcoOS_GetDisplay(
    OUT HALNativeDisplayType * Display,
    IN gctPOINTER Context
    )
{
    XImage *image;
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER();
    do
    {

        *Display = XOpenDisplay(gcvNULL);
        if (*Display == gcvNULL)
        {
            status = gcvSTATUS_OUT_OF_RESOURCES;
            break;
        }

        image = XGetImage(*Display,
                          DefaultRootWindow(*Display),
                          0, 0, 1, 1, AllPlanes, ZPixmap);

        if (image == gcvNULL)
        {
            /* Error. */
            status = gcvSTATUS_NOT_SUPPORTED;
            break;
        }
        XDestroyImage(image);

        gcmFOOTER_ARG("*Display=0x%x", *Display);
        return status;
    }
    while (0);

    if (*Display != gcvNULL)
    {
        XCloseDisplay(*Display);
    }

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
    Screen * screen;
    XImage *image;
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x", Display);
    if (Display == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    screen = XScreenOfDisplay(Display, DefaultScreen(Display));

    if (Width != gcvNULL)
    {
        *Width = XWidthOfScreen(screen);
    }

    if (Height != gcvNULL)
    {
        *Height = XHeightOfScreen(screen);
    }

    if (Physical != gcvNULL)
    {
        *Physical = ~0;
    }

    if (Stride != gcvNULL)
    {
        *Stride = 0;
    }

    if (BitsPerPixel != gcvNULL)
    {
        image = XGetImage(Display,
            DefaultRootWindow(Display),
            0, 0, 1, 1, AllPlanes, ZPixmap);

        if (image != gcvNULL)
        {
            *BitsPerPixel = image->bits_per_pixel;
            XDestroyImage(image);
        }

    }

    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoOS_GetDisplayInfoEx(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctUINT DisplayInfoSize,
    OUT halDISPLAY_INFO * DisplayInfo
    )
{
    Screen * screen;
    XImage *image;

    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x Window=0x%x DisplayInfoSize=%u", Display, Window, DisplayInfoSize);
    /* Valid display? and structure size? */
    if ((Display == gcvNULL) || (DisplayInfoSize != sizeof(halDISPLAY_INFO)))
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    image = XGetImage(Display,
        DefaultRootWindow(Display),
        0, 0, 1, 1, AllPlanes, ZPixmap);

    if (image == gcvNULL)
    {
        /* Error. */
        status = gcvSTATUS_NOT_SUPPORTED;
        gcmFOOTER();
        return status;
    }

    _GetColorBitsInfoFromMask(image->red_mask, &DisplayInfo->redLength, &DisplayInfo->redOffset);
    _GetColorBitsInfoFromMask(image->green_mask, &DisplayInfo->greenLength, &DisplayInfo->greenOffset);
    _GetColorBitsInfoFromMask(image->blue_mask, &DisplayInfo->blueLength, &DisplayInfo->blueOffset);

    XDestroyImage(image);

    screen = XScreenOfDisplay(Display, DefaultScreen(Display));

    /* Return the size of the display. */
    DisplayInfo->width  = XWidthOfScreen(screen);
    DisplayInfo->height = XHeightOfScreen(screen);

    /* The stride of the display is not known in the X environment. */
    DisplayInfo->stride = ~0;

    DisplayInfo->bitsPerPixel = image->bits_per_pixel;

    /* Get the color info. */
    DisplayInfo->alphaLength = image->bits_per_pixel - image->depth;
    DisplayInfo->alphaOffset = image->depth;

    /* The logical address of the display is not known in the X
    ** environment. */
    DisplayInfo->logical = NULL;

    /* The physical address of the display is not known in the X
    ** environment. */
    DisplayInfo->physical = ~0;

    /* No flip. */
    DisplayInfo->flip = 0;

    DisplayInfo->wrapFB = gcvTRUE;

    /* Success. */
    gcmFOOTER_ARG("*DisplayInfo=0x%x", *DisplayInfo);
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
    return gcvSTATUS_NOT_SUPPORTED;
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
    return gcoOS_SetDisplayVirtual(Display, Window, Offset, X, Y);
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
    if (Display != gcvNULL)
    {
        XCloseDisplay(Display);
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
gcoOS_CreateContext(IN gctPOINTER localDisplay, IN gctPOINTER Context)
{
    __DRIcontextPriv *context;
    __DRIDisplay* display;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("localDisplay=0x%x, Context=0x%x\n", localDisplay, Context);

    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    display = (__DRIDisplay*)localDisplay;

    context = (__DRIcontextPriv *)malloc(sizeof(__DRIcontextPriv));
    if (!context) {
        status = gcvSTATUS_OUT_OF_RESOURCES;
        return status;
    }

    memset(context, 0, sizeof(__DRIcontextPriv));

    if (!XF86DRICreateContextWithConfig(display->dpy, display->screen, 0/*modes->fbconfigID*/,
                                        &context->contextID, &context->hHWContext)) {
        status = gcvSTATUS_OUT_OF_RESOURCES;
        free(context);
        return status;
    }

    context->eglContext = Context;
    context->hwLock = (drm_hw_lock_t *)&display->pSAREA->lock;
    context->next = display->contextStack;
    context->fd = display->drmFd;
    display->contextStack = context;

#ifdef DRI_PIXMAPRENDER
    if ( context->drawablePriv )
    {
            _DestroyBackPixmapForDrawable(context->drawablePriv);
            _CreateBackPixmapForDrawable(context->drawablePriv);
    }
#endif
    /* Success. */
    gcmFOOTER_ARG("*context=0x%x", context);
    return status;
}

gceSTATUS
gcoOS_DestroyContext(IN gctPOINTER localDisplay, IN gctPOINTER Context)
{
    __DRIDisplay* display;
    gceSTATUS status = gcvSTATUS_OK;
    __DRIcontextPriv            *cur;
    __DRIcontextPriv            *prev;
    gctBOOL        found = gcvFALSE;


    gcmHEADER_ARG("localDisplay=0x%x, Context=0x%x\n", localDisplay, Context);

    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    display = (__DRIDisplay*)localDisplay;
    prev = cur = display->contextStack;
    while ((cur != NULL) && (!found)) {
        if (cur->eglContext == Context) {
            found = gcvTRUE;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    if (found) {

#ifdef DRI_PIXMAPRENDER
    if ( cur->drawablePriv )
    {
            _DestroyBackPixmapForDrawable(cur->drawablePriv);
            _CreateBackPixmapForDrawable(cur->drawablePriv);
    }
#endif

#if defined(DRI_PIXMAPRENDER_ASYNC)
    /*asyncRenderKill(cur->drawablePriv);*/
    asyncRenderDestroy(cur->drawablePriv);
#endif

        XF86DRIDestroyContext(display->dpy, display->screen, cur->contextID);
        if (display->contextStack == cur) {
            display->contextStack = cur->next;
        } else {
            prev->next = cur->next;
        }
        free(cur);
    }
    _DestroyOnScreenSurfaceWrapper(display);
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_MakeCurrent(IN gctPOINTER localDisplay,
    IN HALNativeWindowType DrawDrawable,
    IN HALNativeWindowType ReadDrawable,
    IN gctPOINTER Context,
    IN gcoSURF ResolveTarget)
{
   __DRIDisplay* display;
    gceSTATUS status = gcvSTATUS_OK;
    __DRIdrawablePriv *drawable;
    gceSURF_FORMAT format;

    gcmHEADER_ARG("localDisplay=0x%x, Drawable = 0x%x, Readable = 0x%x Context=0x%x\n", localDisplay, DrawDrawable, ReadDrawable, Context);

    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }


    display = (__DRIDisplay*)localDisplay;

    display->activeContext = _FindContext(display, Context);
    display->activeDrawable = _FindDrawable(display, DrawDrawable);
    if (display->activeContext && display->activeDrawable) {
         display->activeContext->drawablePriv = display->activeDrawable;
         display->activeDrawable->contextPriv = display->activeContext;
         drawable = display->activeDrawable;
    } else {
        status = gcvSTATUS_OUT_OF_RESOURCES;
        return status;
    }

    if (!drawable->pStamp) {
       DRM_SPINLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
       _UpdateDrawableInfoDrawableInfo(drawable);
       DRM_SPINUNLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
       drawable->pStamp = &(display->pSAREA->drawableTable[drawable->index].stamp);
    } else {
        if (*drawable->pStamp != drawable->lastStamp) {
            DRM_SPINLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
            _UpdateDrawableInfoDrawableInfo(drawable);
            DRM_SPINUNLOCK(&display->pSAREA->drawable_lock, drawable->drawLockID);
        }
    }

    gcoSURF_GetFormat(ResolveTarget, gcvNULL,&format );

#ifdef DRI_PIXMAPRENDER
    if ( drawable )
    {
            _DestroyBackPixmapForDrawable(drawable);
            gcoSURF_GetFormat(ResolveTarget, &drawable->surftype, &drawable->surfformat);
            _CreateBackPixmapForDrawable(drawable);
    }
#endif

    _CreateOnScreenSurfaceWrapper(drawable, format);

    gcmFOOTER();
    return status;
}


/*******************************************************************************
** Drawable
*/
gceSTATUS
gcoOS_CreateDrawable(IN gctPOINTER localDisplay, IN HALNativeWindowType Drawable)
{
    __DRIdrawablePriv *drawable;

    __DRIDisplay* display;
    gceSTATUS status = gcvSTATUS_OK;

    gcmHEADER_ARG("localDisplay=0x%x, Drawable=0x%x\n", localDisplay, Drawable);

    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    display = (__DRIDisplay*)localDisplay;

    drawable = (__DRIdrawablePriv *)malloc(sizeof(__DRIdrawablePriv));
    if (!drawable) {
        status = gcvSTATUS_OUT_OF_RESOURCES;
        return status;
    }

    memset(drawable, 0, sizeof(__DRIdrawablePriv));
    if (!XF86DRICreateDrawable(display->dpy, display->screen, Drawable, &drawable->hHWDrawable)) {
        free(drawable);
        status = gcvSTATUS_OUT_OF_RESOURCES;
        return status;
    }


    drawable->drawable = Drawable;
    drawable->refcount = 0;
    drawable->pStamp = NULL;
    drawable->lastStamp = 0;
    drawable->index = 0;
    drawable->x = 0;
    drawable->y = 0;
    drawable->w = 0;
    drawable->h = 0;
    drawable->numClipRects = 0;
    drawable->numBackClipRects = 0;
    drawable->pClipRects = NULL;
    drawable->pBackClipRects = NULL;
    drawable->display = display;
    drawable->drawableResize = gcvFALSE;
    drawable->screen = display->screen;
    drawable->drawLockID = 1;
    drawable->fd = display->drmFd;
    drawable->backNode = 0;
    drawable->nodeName = 0;

    drawable->next = display->drawableStack;
    display->drawableStack = drawable;
#ifdef DRI_PIXMAPRENDER
    drawable->backPixmap = (Pixmap)0;
    drawable->xgc = (GC)0;
    drawable->pixWrapSurf = gcvNULL;
#endif

#ifdef DRI_PIXMAPRENDER_ASYNC
    drawable->frameMutex = gcvNULL;
    drawable->workerThread = gcvNULL;
    drawable->busyNum = 0;
    drawable->curIndex = -1;
    drawable->busySIG = gcvNULL;
    drawable->stopSIG = gcvNULL;
    drawable->exitSIG = gcvNULL;
#endif
    /* Success. */
    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_DestroyDrawable(IN gctPOINTER localDisplay, IN HALNativeWindowType Drawable)
{
    __DRIdrawablePriv *cur;
    __DRIdrawablePriv *prev;
    __DRIDisplay* display;
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL found = gcvFALSE;

    gcmHEADER_ARG("localDisplay=0x%x, Drawable=0x%x\n", localDisplay, Drawable);

    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    display = (__DRIDisplay*)localDisplay;
    prev = cur = display->drawableStack;
    while ((cur != NULL) && (!found)) {
        if (cur->drawable == Drawable) {
            found = gcvTRUE;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    if (found) {

        if ( cur->backNode && cur->nodeName )
            _FreeVideoNode(0, cur->backNode);

        cur->backNode = 0;
        cur->nodeName = 0;

#ifdef DRI_PIXMAPRENDER
        _DestroyBackPixmapForDrawable(cur);
#endif
#ifdef DRI_PIXMAPRENDER_ASYNC
        /*asyncRenderKill(cur);*/
        asyncRenderDestroy(cur);
#endif
        XF86DRIDestroyDrawable(display->dpy, display->screen, Drawable);
        if (display->drawableStack == cur) {
            display->drawableStack = cur->next;
        } else {
            prev->next = cur->next;
        }
        free(cur);
    }

    /* Success. */
    gcmFOOTER();
    return status;
}

/*******************************************************************************
** Windows
*/

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
    XSetWindowAttributes attr;
    Screen * screen;
    gctINT width, height;
    gceSTATUS status = gcvSTATUS_OK;
    gctINT    ignoreDisplaySize = 0;
    gctCHAR * p;

    gcmHEADER_ARG("Display=0x%x X=%d Y=%d Width=%d Height=%d", Display, X, Y, Width, Height);
    /* Test if we have a valid display data structure pointer. */
    if (Display == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }
    do
    {
        screen = XScreenOfDisplay(Display, DefaultScreen(Display));
        width  = XWidthOfScreen(screen);
        height = XHeightOfScreen(screen);

        attr.background_pixmap = 0;
        attr.border_pixel      = 0;
        attr.event_mask        = ButtonPressMask
                               | ButtonReleaseMask
                               | KeyPressMask
                               | KeyReleaseMask
                               | PointerMotionMask;

        p = getenv("DRI_IGNORE_DISPLAY_SIZE");
        if (p != gcvNULL)
        {
            ignoreDisplaySize = atoi(p);
        }

        /* Test for zero width. */
        if (Width == 0)
        {
            /* Use display width instead. */
            Width = width;
        }

        /* Test for zero height. */
        if (Height == 0)
        {
            /* Use display height instead. */
            Height = height;
        }

        /* Test for auto-center X coordinate. */
        if (X == -1)
        {
            /* Center the window horizontally. */
            X = (width - Width) / 2;
        }

        /* Test for auto-center Y coordinate. */
        if (Y == -1)
        {
            /* Center the window vertically. */
            Y = (height - Height) / 2;
        }

        /* Clamp coordinates to display. */
        if (X < 0) X = 0;
        if (Y < 0) Y = 0;
        if (!ignoreDisplaySize)
        {
            if (X + Width  > width)  Width  = width  - X;
            if (Y + Height > height) Height = height - Y;
        }

        *Window = XCreateWindow(Display,
                               RootWindow(Display,
                                          DefaultScreen(Display)),
                               X, Y,
                               Width, Height,
                               0,
                               DefaultDepth(Display, DefaultScreen(Display)),
                               InputOutput,
                               DefaultVisual(Display, DefaultScreen(Display)),
                               CWEventMask,
                               &attr);

        if (!*Window)
        {
            break;
        }

        XMoveWindow(Display, *Window, X, Y);

        gcmFOOTER_ARG("*Window=0x%x", *Window);
        return status;
    }
    while (0);

    if (*Window)
    {
        XUnmapWindow(Display, *Window);
        XDestroyWindow(Display, *Window);
    }

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
    XWindowAttributes attr;
    XImage *image;
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x Window=0x%x", Display, Window);
    if (Window == 0)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    XGetWindowAttributes(Display, Window, &attr);

    if (X != NULL)
    {
        *X = attr.x;
    }

    if (Y != NULL)
    {
        *Y = attr.y;
    }

    if (Width != NULL)
    {
        *Width = attr.width;
    }

    if (Height != NULL)
    {
        *Height = attr.height;
    }

    if (BitsPerPixel != NULL)
    {
        image = XGetImage(Display,
            DefaultRootWindow(Display),
            0, 0, 1, 1, AllPlanes, ZPixmap);

        if (image != NULL)
        {
            *BitsPerPixel = image->bits_per_pixel;
            XDestroyImage(image);
        }
    }

    if (Offset != NULL)
    {
        *Offset = 0;
    }

    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoOS_DestroyWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{
    if((Display == gcvNULL) || (Window == 0))
        return gcvSTATUS_INVALID_ARGUMENT;
    XUnmapWindow(Display, Window);
    XDestroyWindow(Display, Window);
    return gcvSTATUS_OK;
}


#ifdef DRI_PIXMAPRENDER
static gceSTATUS
gcoOS_SwapBuffersXwinEx(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gcsPOINT srcOrigin, size;
    gceSTATUS status = gcvSTATUS_OK;


    gcmHEADER_ARG("localDisplay=0x%x", localDisplay);
    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    driDisplay = (__DRIDisplay*)localDisplay;
    drawable = _FindDrawable(driDisplay, Drawable);

    if ( drawable )
    {
        context = drawable->contextPriv;
        gcoSURF_GetFormat(ResolveTarget, &drawable->surftype, &drawable->surfformat);
        LINUX_LOCK_FRAMEBUFFER(context);
    } else {
        goto ENDXWINEX;
    }


    ResolveTarget = drawable->pixWrapSurf;


    if (drawable) {
        srcOrigin.x = 0;
        srcOrigin.y = 0;
        size.x = *Width;
        size.y = *Height;
        status = gcoSURF_ResolveRect(RenderTarget,
            ResolveTarget,
            &srcOrigin,
            &srcOrigin,
            &size);
        if (gcmIS_ERROR(status))
        {
            goto ENDXWINEX;
        }
        gcoHAL_Commit(gcvNULL, gcvFALSE);
    }

    *Width = drawable->w;
    *Height = drawable->h;

#if defined(VIV_EXT)
        if ( drawable->backPixmap )
        VIVEXTDrawableFlush(driDisplay->dpy, (unsigned int)(driDisplay->screen),(unsigned int)drawable->backPixmap);
#endif
    XCopyArea(driDisplay->dpy, drawable->backPixmap, Drawable, drawable->xgc, 0, 0, *Width, *Height, 0, 0);
    XFlush(driDisplay->dpy);

ENDXWINEX:
    if ( drawable )
    {
        context = drawable->contextPriv;
        LINUX_UNLOCK_FRAMEBUFFER(context);
    }
    gcmFOOTER();
    return status;

}
#endif

#ifdef DRI_PIXMAPRENDER_ASYNC
static gceSTATUS
gcoOS_SwapBuffersXwinAsync(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gceSTATUS status = gcvSTATUS_OK;
    gcsPOINT srcOrigin, size;

    int index = -1;

    gcmHEADER_ARG("localDisplay=0x%x", localDisplay);
    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    driDisplay = (__DRIDisplay*)localDisplay;
    drawable = _FindDrawable(driDisplay, Drawable);

    if ( drawable )
    {
        context = drawable->contextPriv;
        gcoSURF_GetFormat(ResolveTarget, &drawable->surftype, &drawable->surfformat);
        /* To check Win by DRI */
        /*LINUX_LOCK_FRAMEBUFFER(context);*/
        /*LINUX_UNLOCK_FRAMEBUFFER(context);*/
    } else {
        goto ENDXWINEX;
    }


    asyncRenderBegin(drawable, Drawable, &index);

    asyncRenderSurf(drawable, index, &ResolveTarget);

    if (drawable) {
        srcOrigin.x = 0;
        srcOrigin.y = 0;
        size.x = *Width;
        size.y = *Height;
        status = gcoSURF_ResolveRect(RenderTarget,
            ResolveTarget,
            &srcOrigin,
            &srcOrigin,
            &size);
        if (gcmIS_ERROR(status))
        {
            goto ENDXWINEX;
        }
        gcoHAL_Commit(gcvNULL, gcvFALSE);
    }

    *Width = drawable->w;
    *Height = drawable->h;

    LINUX_LOCK_FRAMEBUFFER(context);

    asyncRenderEnd(drawable, Drawable, index);


ENDXWINEX:
    gcmFOOTER();
    return status;

}
#endif

#if !defined(DRI_PIXMAPRENDER) && !defined(DRI_PIXMAPRENDER_ASYNC)

static gceSTATUS
gcoOS_SwapBuffersXwin(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    gctUINT8_PTR  destStart;
    gctUINT8_PTR  srcStart;
    gctUINT i,j;
    gctUINT alignedWidth, alignedHeight;
    gctUINT alignedDestWidth;
    drm_clip_rect_t *pClipRects;
    gctUINT xoffset, yoffset;
    gctUINT height, width;
    __DRIDisplay* driDisplay;
    gcsPOINT srcOrigin, size;
    gceSTATUS status = gcvSTATUS_OK;

    Window root_w;
    Window child_w;
    int root_x;
    int root_y;
    int win_x;
    int win_y;
    unsigned int mask_return;
    static int root_x_s = -1;
    static int root_y_s = -1;
    static int cursorindex = 6;

    gcmHEADER_ARG("localDisplay=0x%x", localDisplay);
    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    driDisplay = (__DRIDisplay*)localDisplay;
    drawable = _FindDrawable(driDisplay, Drawable);

    if ( drawable )
    {
        context = drawable->contextPriv;
        LINUX_LOCK_FRAMEBUFFER(context);
    }


    if (drawable && drawable->fullScreenMode) {
        if ( drawable->fullscreenCovered == 0)
        {
                ResolveTarget = driDisplay->renderTarget;
        }
    }


    if (drawable) {
        srcOrigin.x = 0;
        srcOrigin.y = 0;
        size.x = *Width;
        size.y = *Height;
        status = gcoSURF_ResolveRect(RenderTarget,
            ResolveTarget,
            &srcOrigin,
            &srcOrigin,
            &size);
        if (gcmIS_ERROR(status))
        {
            goto ENDXWIN;
        }
        gcoHAL_Commit(gcvNULL, gcvFALSE);
    }

    if (drawable && drawable->fullScreenMode) {
        /* Full screen apps, image is directly resolved to on screen buffer */
        LINUX_UNLOCK_FRAMEBUFFER(context);
        XQueryPointer(driDisplay->dpy, DefaultRootWindow(driDisplay->dpy), &root_w, &child_w, &root_x, &root_y, &win_x, &win_y, &mask_return);
        if ( root_x != root_x_s || root_y != root_y_s )
        {
           root_x_s = root_x;
           root_y_s = root_y;
           cursorindex = 6;
        } else {
           cursorindex--;
        }

        if ( cursorindex > 0) {
           XFixesHideCursor(driDisplay->dpy,Drawable);
           XFlush(driDisplay->dpy);
           gcoHAL_Commit(gcvNULL, gcvFALSE);
           XFixesShowCursor(driDisplay->dpy,Drawable);
        }
        gcmFOOTER();
        return status;
    }

    if (drawable) {
        alignedDestWidth = drawable->wWidth;

        gcoSURF_GetAlignedSize(
                        ResolveTarget,
                        &alignedWidth,
                        &alignedHeight,
                        gcvNULL
        );

        if (!drawable->drawableResize) {

            srcStart = ResolveBits;
            pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
            if ( (drawable->backNode == 0) && (drawable->numClipRects > 0) )
            {
                pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
                for (i = 0; i < drawable->numClipRects; i++)
                {
                            destStart = driDisplay->linearAddr;
                            srcStart = ResolveBits;
                            xoffset = pClipRects->x1 - drawable->xWOrigin;
                            yoffset = pClipRects->y1 - drawable->yWOrigin;
                            width = pClipRects->x2 - pClipRects->x1;
                            height = pClipRects->y2 - pClipRects->y1;
                            srcStart += (alignedWidth*yoffset + xoffset ) * driDisplay->bpp;
                            xoffset = pClipRects->x1;
                            yoffset = pClipRects->y1;
                            destStart += (alignedDestWidth * yoffset + xoffset) * driDisplay->bpp;

                            for (j = 0; j < height; j++) {
                            memcpy((destStart + j * alignedDestWidth * driDisplay->bpp), (srcStart + j * alignedWidth * driDisplay->bpp), width * driDisplay->bpp);
                            }

                            pClipRects++;
                }
            }
        } else {
            drawable->drawableResize = gcvFALSE;
        }
        *Width = drawable->w;
        *Height = drawable->h;
    }

ENDXWIN:

    if ( drawable )
    {
        context = drawable->contextPriv;
        LINUX_UNLOCK_FRAMEBUFFER(context);
    }

    gcmFOOTER();
    return status;
}

#endif

#if !defined(DRI_PIXMAPRENDER_ASYNC)
static gcoSURF _GetWrapSurface(gcoSURF ResolveTarget, gctPOINTER phyaddr, gctPOINTER lineaddr, gctUINT width, gctUINT height, gctUINT stride)
{
    gceSURF_TYPE surftype;
    gceSURF_FORMAT surfformat;
    static gcoSURF tarsurf = gcvNULL;

    gceSTATUS status = gcvSTATUS_OK;

    if ( tarsurf )
        gcoSURF_Destroy(tarsurf);

    gcoSURF_GetFormat(ResolveTarget, &surftype, &surfformat);

    do {
        /* Construct a wrapper around the pixmap.  */
        gcmERR_BREAK(gcoSURF_ConstructWrapper(
        gcvNULL,
        &tarsurf
        ));

        /* Set the underlying frame buffer surface. */
        gcmERR_BREAK(gcoSURF_SetBuffer(
        tarsurf,
        surftype,
        surfformat,
        stride,
        lineaddr,
        (gctUINT32)phyaddr
        ));

        /* Set the window. */
        gcmERR_BREAK(gcoSURF_SetWindow(
        tarsurf,
        0,
        0,
        width,
        height
        ));

        return tarsurf;
    } while(0);

    return gcvNULL;
}

static gceSTATUS _SwapBuffersWithDirectMode(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gctPOINTER  destLogicalAddr[3] = {0, 0, 0};
    gctUINT8_PTR  destStart;
    gctUINT8_PTR  destPhyStart;
    gctUINT   destPhys[3] = {0,0,0};
    gctUINT alignedDestWidth;
    gctUINT height, width;
    gceSTATUS status = gcvSTATUS_OK;
    drm_clip_rect_t *pClipRects;
    gctUINT xoffset, yoffset;
    XserverRegion region;
    XRectangle damagedRect[1];
    gcoSURF wrapsurf = gcvNULL;


    driDisplay = (__DRIDisplay*)localDisplay;

    drawable = _FindDrawable( driDisplay, Drawable );
    context = drawable->contextPriv;

    if ( drawable )
    {
        if ( drawable->fullScreenMode)
        {
            if ( drawable->fullscreenCovered == 0 )
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        /* resolve function needs aligned position and size */
        /* so only handle one */
        if ( drawable->numClipRects < 1)
            return gcvSTATUS_INVALID_ARGUMENT;
        if ( drawable->numClipRects > 1)
            return gcvSTATUS_INVALID_ARGUMENT;

         pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
         xoffset = pClipRects->x1 - drawable->xWOrigin;
         yoffset = pClipRects->y1 - drawable->yWOrigin;

        if (xoffset != 0 || yoffset != 0)
            return gcvSTATUS_INVALID_ARGUMENT;

        width = pClipRects->x2 - pClipRects->x1;
        height = pClipRects->y2 - pClipRects->y1;

        if ( *Width != width || *Height != height )
            return gcvSTATUS_INVALID_ARGUMENT;

    }

    if (drawable) {
        status = gcvSTATUS_INVALID_ARGUMENT;
        context = drawable->contextPriv;
        LINUX_LOCK_FRAMEBUFFER(context);
        if (drawable->backNode && !drawable->drawableResize)
        {

            _LockVideoNode(0, drawable->backNode, &destPhys[0], &destLogicalAddr[0]);
            drawable->backBufferPhysAddr = destPhys[0];
            drawable->backBufferLogicalAddr = destLogicalAddr[0];

            if ( drawable->numClipRects) {

                alignedDestWidth = drawable->wWidth;
                destStart = (gctUINT8_PTR)drawable->backBufferLogicalAddr;
                destPhyStart = (gctUINT8_PTR)drawable->backBufferPhysAddr;

                pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
                width = pClipRects->x2 - pClipRects->x1;
                height = pClipRects->y2 - pClipRects->y1;
                xoffset = pClipRects->x1;
                yoffset = pClipRects->y1;
                destStart += (alignedDestWidth * yoffset + xoffset) * driDisplay->bpp;
                destPhyStart += (alignedDestWidth * yoffset + xoffset) * driDisplay->bpp;

                wrapsurf = _GetWrapSurface(ResolveTarget, (gctPOINTER)destPhyStart, (gctPOINTER)destStart, width, height, alignedDestWidth*driDisplay->bpp);

                if ( wrapsurf )
                {
                    status = gcoSURF_Resolve(RenderTarget,wrapsurf);
                    gcoHAL_Commit(gcvNULL, gcvFALSE);
                }

            }
            _UnlockVideoNode(0, drawable->backNode);
            drawable->drawableResize = gcvFALSE;
        }

        LINUX_UNLOCK_FRAMEBUFFER(context);

        if (gcmIS_ERROR(status))
        {
                return status;
        }

#if defined(VIV_EXT)
        if ( wrapsurf )
        VIVEXTDrawableFlush(driDisplay->dpy, (unsigned int)(driDisplay->screen),(unsigned int)Drawable);
#endif

        /* Report damage to Xserver */
        damagedRect[0].x = 0;
        damagedRect[0].y = 0;
        damagedRect[0].width = drawable->w;
        damagedRect[0].height = drawable->h;
        region = XFixesCreateRegion (driDisplay->dpy, &damagedRect[0], 1);
        XDamageAdd (driDisplay->dpy, Drawable, region);
        XFixesDestroyRegion(driDisplay->dpy, region);
        XFlush(driDisplay->dpy);

        *Width = drawable->w;
        *Height = drawable->h;
    }

    return gcvSTATUS_OK;
}
#endif

#ifdef DRI_PIXMAPRENDER
static gceSTATUS
gcoOS_SwapBuffersUnityEx(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{

    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gctPOINTER  destLogicalAddr[3] = {0, 0, 0};
    gctUINT8_PTR  destStart;
    gctUINT8_PTR  srcStart;
    gctUINT   destPhys[3] = {0,0,0};
    gctUINT i, j;
    gctUINT alignedWidth, alignedHeight;
    gctUINT alignedDestWidth;
    gctUINT height, width;
    gcsPOINT srcOrigin, size;
    gceSTATUS status = gcvSTATUS_OK;
    drm_clip_rect_t *pClipRects;
    gctUINT xoffset, yoffset;
    XserverRegion region;
    XRectangle damagedRect[1];

    gcmHEADER_ARG("localDisplay=0x%x", localDisplay);
    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    driDisplay = (__DRIDisplay*)localDisplay;

    drawable = _FindDrawable( driDisplay, Drawable );
    context = drawable->contextPriv;

    if (_SwapBuffersWithDirectMode(localDisplay, Drawable, RenderTarget, ResolveTarget, ResolveBits, Width, Height) == gcvSTATUS_OK)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    if (drawable) {
        srcOrigin.x = 0;
        srcOrigin.y = 0;
        size.x = *Width;
        size.y = *Height;

        status = gcoSURF_ResolveRect(RenderTarget,
            ResolveTarget,
            &srcOrigin,
            &srcOrigin,
            &size);
        if (gcmIS_ERROR(status))
        {
            gcmFOOTER();
            return status;
        }
    }

    if (drawable) {
        gcoHAL_Commit(gcvNULL, gcvFALSE);
        context = drawable->contextPriv;
        LINUX_LOCK_FRAMEBUFFER(context);
        if (drawable->backNode && !drawable->drawableResize)
        {

            _LockVideoNode(0, drawable->backNode, &destPhys[0], &destLogicalAddr[0]);
            drawable->backBufferPhysAddr = destPhys[0];
            drawable->backBufferLogicalAddr = destLogicalAddr[0];

            gcoSURF_GetAlignedSize(
                    ResolveTarget,
                    &alignedWidth,
                    &alignedHeight,
                    gcvNULL
                    );

            alignedDestWidth = drawable->wWidth;

            srcStart = ResolveBits;
            destStart = destLogicalAddr[0];

            if (drawable->numClipRects > 0)
            {
                    pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
                    for (i = 0; i < drawable->numClipRects; i++)
                    {
                        destStart = destLogicalAddr[0];
                        srcStart = ResolveBits;
                        xoffset = pClipRects->x1 - drawable->xWOrigin;
                        yoffset = pClipRects->y1 - drawable->yWOrigin;
                        width = pClipRects->x2 - pClipRects->x1;
                        height = pClipRects->y2 - pClipRects->y1;
                        srcStart += (alignedWidth*yoffset + xoffset ) * driDisplay->bpp;
                        xoffset = pClipRects->x1;
                        yoffset = pClipRects->y1;
                        destStart += (alignedDestWidth * yoffset + xoffset) * driDisplay->bpp;

                        for (j = 0; j < height; j++) {
                        memcpy((destStart + j * alignedDestWidth * driDisplay->bpp), (srcStart + j * alignedWidth * driDisplay->bpp), width * driDisplay->bpp);
                        }

                        pClipRects++;
                    }
            }

            _UnlockVideoNode(0, drawable->backNode);
        }
        LINUX_UNLOCK_FRAMEBUFFER(context);
        drawable->drawableResize = gcvFALSE;

        damagedRect[0].x = 0;
        damagedRect[0].y = 0;
        damagedRect[0].width = drawable->w;
        damagedRect[0].height = drawable->h;
        region = XFixesCreateRegion (driDisplay->dpy, &damagedRect[0], 1);
        XDamageAdd (driDisplay->dpy, Drawable, region);
        XFixesDestroyRegion(driDisplay->dpy, region);
        XFlush(driDisplay->dpy);

        *Width = drawable->w;
        *Height = drawable->h;
    }

    gcmFOOTER();
    return gcvSTATUS_OK;
}
#endif


#if !defined(DRI_PIXMAPRENDER) && !defined(DRI_PIXMAPRENDER_ASYNC)
static gceSTATUS
gcoOS_SwapBuffersUnity(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{

    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gctPOINTER  destLogicalAddr[3] = {0, 0, 0};
    gctUINT8_PTR  destStart;
    gctUINT8_PTR  srcStart;
    gctUINT   destPhys[3] = {0,0,0};
    gctUINT i, j;
    gctUINT alignedWidth, alignedHeight;
    gctUINT alignedDestWidth;
    gctUINT height, width;
    gcsPOINT srcOrigin, size;
    gceSTATUS status = gcvSTATUS_OK;
    drm_clip_rect_t *pClipRects;
    gctUINT xoffset, yoffset;
    XserverRegion region;
    XRectangle damagedRect[1];

    Window root_w;
    Window child_w;
    int root_x;
    int root_y;
    int win_x;
    int win_y;
    unsigned int mask_return;
    static int root_x_s = -1;
    static int root_y_s = -1;
    static int cursorindex = 6;

    gcmHEADER_ARG("localDisplay=0x%x", localDisplay);
    if (localDisplay == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    driDisplay = (__DRIDisplay*)localDisplay;

    drawable = _FindDrawable( driDisplay, Drawable );
    context = drawable->contextPriv;

    if (_SwapBuffersWithDirectMode(localDisplay, Drawable, RenderTarget, ResolveTarget, ResolveBits, Width, Height) == gcvSTATUS_OK)
    {
        gcmFOOTER();
        return gcvSTATUS_OK;
    }

    if (drawable && drawable->fullScreenMode) {
        /* If drawable is full and covered by another win, we can't resolve it to screen directly */
        /* If covered, go normal path */
        if ( drawable->fullscreenCovered == 0)
            ResolveTarget = driDisplay->renderTarget;
    }

    if (drawable) {
        srcOrigin.x = 0;
        srcOrigin.y = 0;
        size.x = *Width;
        size.y = *Height;

        status = gcoSURF_ResolveRect(RenderTarget,
            ResolveTarget,
            &srcOrigin,
            &srcOrigin,
            &size);
        if (gcmIS_ERROR(status))
        {
            gcmFOOTER();
            return status;
        }
    }

    if (drawable && drawable->fullScreenMode && (drawable->fullscreenCovered == 0 ) ) {
       /* Full screen apps, image is directly resolved to on screen buffer
        * Force the xserver to update the area where the cursor is
       * Fix the garbage image issue when moving win with compiz
        */
       XQueryPointer(driDisplay->dpy, DefaultRootWindow(driDisplay->dpy), &root_w, &child_w, &root_x, &root_y, &win_x, &win_y, &mask_return);
       if ( root_x != root_x_s || root_y != root_y_s )
       {
           root_x_s = root_x;
           root_y_s = root_y;
           cursorindex = 6;
       } else {
           cursorindex--;
       }

       if ( cursorindex > 0) {
           XFixesHideCursor(driDisplay->dpy,Drawable);
           XFlush(driDisplay->dpy);
           gcoHAL_Commit(gcvNULL, gcvFALSE);
           XFixesShowCursor(driDisplay->dpy,Drawable);
       }
        gcmFOOTER();
        return gcvSTATUS_OK;
    }


    if (drawable) {
        gcoHAL_Commit(gcvNULL, gcvFALSE);
        context = drawable->contextPriv;
        LINUX_LOCK_FRAMEBUFFER(context);
        if (drawable->backNode && !drawable->drawableResize)
        {

            _LockVideoNode(0, drawable->backNode, &destPhys[0], &destLogicalAddr[0]);
            drawable->backBufferPhysAddr = destPhys[0];
            drawable->backBufferLogicalAddr = destLogicalAddr[0];

            gcoSURF_GetAlignedSize(
                    ResolveTarget,
                    &alignedWidth,
                    &alignedHeight,
                    gcvNULL
                    );

            alignedDestWidth = drawable->wWidth;

            srcStart = ResolveBits;
            destStart = destLogicalAddr[0];

            if (drawable->numClipRects > 0)
            {
                    pClipRects = (drm_clip_rect_t *)drawable->pClipRects;
                    for (i = 0; i < drawable->numClipRects; i++)
                    {
                        destStart = destLogicalAddr[0];
                        srcStart = ResolveBits;
                        xoffset = pClipRects->x1 - drawable->xWOrigin;
                        yoffset = pClipRects->y1 - drawable->yWOrigin;
                        width = pClipRects->x2 - pClipRects->x1;
                        height = pClipRects->y2 - pClipRects->y1;
                        srcStart += (alignedWidth*yoffset + xoffset ) * driDisplay->bpp;
                        xoffset = pClipRects->x1;
                        yoffset = pClipRects->y1;
                        destStart += (alignedDestWidth * yoffset + xoffset) * driDisplay->bpp;

                        for (j = 0; j < height; j++) {
                        memcpy((destStart + j * alignedDestWidth * driDisplay->bpp), (srcStart + j * alignedWidth * driDisplay->bpp), width * driDisplay->bpp);
                        }

                        pClipRects++;
                    }
            }

            _UnlockVideoNode(0, drawable->backNode);
        }
        LINUX_UNLOCK_FRAMEBUFFER(context);
        drawable->drawableResize = gcvFALSE;
        if (!drawable->fullScreenMode) {
            /* Report damage to Xserver */
            damagedRect[0].x = 0;
            damagedRect[0].y = 0;
            damagedRect[0].width = drawable->w;
            damagedRect[0].height = drawable->h;
            region = XFixesCreateRegion (driDisplay->dpy, &damagedRect[0], 1);
            XDamageAdd (driDisplay->dpy, Drawable, region);
            XFixesDestroyRegion(driDisplay->dpy, region);
            XFlush(driDisplay->dpy);
        }
        *Width = drawable->w;
        *Height = drawable->h;
    }

    gcmFOOTER();
    return gcvSTATUS_OK;
}

#endif

static gctBOOL _legacyPath(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable)
{
    __DRIcontextPriv * context;
    __DRIdrawablePriv * drawable;
    __DRIDisplay* driDisplay;
    gctBOOL ret = gcvFALSE;
    driDisplay = (__DRIDisplay*)localDisplay;

    drawable = _FindDrawable( driDisplay, Drawable );
    if ( drawable ) {
        context = drawable->contextPriv;
        if ( context )
        {
            LINUX_LOCK_FRAMEBUFFER(context);
            ret= (drawable->backNode == 0)? gcvTRUE : gcvFALSE;
            LINUX_UNLOCK_FRAMEBUFFER(context);
        }
    }
    return ret;
}


gceSTATUS
gcoOS_SwapBuffers(
    IN gctPOINTER localDisplay,
    IN HALNativeWindowType Drawable,
    IN gcoSURF RenderTarget,
    IN gcoSURF ResolveTarget,
    IN gctPOINTER ResolveBits,
    OUT gctUINT *Width,
    OUT gctUINT *Height
    )
{
#ifdef DRI_PIXMAPRENDER_ASYNC
#define gcoOS_SwapBuffersUnityAsync gcoOS_SwapBuffersXwinAsync
#endif
    /* Front buffer swap for NON-UNITY desktops */
    if ( _legacyPath(localDisplay, Drawable) )
    {
#ifdef DRI_PIXMAPRENDER
        return gcoOS_SwapBuffersXwinEx(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);
#else

#ifdef DRI_PIXMAPRENDER_ASYNC

        return gcoOS_SwapBuffersXwinAsync(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);

#else
        return gcoOS_SwapBuffersXwin(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);
#endif

#endif
    }
    else
    {

#ifdef DRI_PIXMAPRENDER
        return gcoOS_SwapBuffersUnityEx(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);
#else

#ifdef DRI_PIXMAPRENDER_ASYNC

        return gcoOS_SwapBuffersUnityAsync(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);

#else
        return gcoOS_SwapBuffersUnity(
                                localDisplay,
                                Drawable,
                                RenderTarget,
                                ResolveTarget,
                                ResolveBits,
                                Width,
                                Height);
#endif

#endif
    }
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
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x Window=0x%x Left=%d Top=%d Right=%d Bottom=%d Width=%d Height=%d BitsPerPixel=%d Bits=0x%x",
                  Display, Window, Left, Top, Right, Bottom, Width, Height, BitsPerPixel, Bits);

#if defined(VIV_EXT)
    if ( Left == 0
        && Top == 0
        && Right == 0
        && Bottom == 0
        && Width == 0
        && Height == 0
        && BitsPerPixel == 0
        && Bits == gcvNULL )
   {
        VIVEXTDrawableFlush(Display, (unsigned int)0,(unsigned int)Window);
   }
#endif

    gcmFOOTER();
    return status;
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
    /* Get default root window. */
    XImage *image;
    Window rootWindow;
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x Width=%d Height=%d BitsPerPixel=%d", Display, Width, Height, BitsPerPixel);
    /* Test if we have a valid display data structure pointer. */
    if (Display == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }
    if ((Width <= 0) || (Height <= 0))
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }
    rootWindow = DefaultRootWindow(Display);

    /* Check BitsPerPixel, only support 16bit and 32bit now. */
    switch (BitsPerPixel)
    {
    case 0:
        image = XGetImage(Display,
            rootWindow,
            0, 0, 1, 1, AllPlanes, ZPixmap);

        if (image == gcvNULL)
        {
            /* Error. */
            status = gcvSTATUS_INVALID_ARGUMENT;
            gcmFOOTER();
            return status;
        }
        BitsPerPixel = image->bits_per_pixel;
        break;

    case 16:
    case 32:
        break;

    default:
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    do
    {
        /* Create native pixmap, ie X11 Pixmap. */
        *Pixmap = XCreatePixmap(Display,
            rootWindow,
            Width,
            Height,
            BitsPerPixel);

        if (*Pixmap == 0)
        {
            break;
        }

        /* Flush command buffer. */
        XFlush(Display);

        gcmFOOTER_ARG("*Pixmap=0x%x", *Pixmap);
        return status;
    }
    while (0);

    /* Roll back. */
    if (*Pixmap != 0)
    {
        XFreePixmap(Display, *Pixmap);
        XFlush(Display);
    }

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
    Window rootWindow = 0;
    gctINT x = 0, y = 0;
    gctUINT width = 0, height = 0, borderWidth = 0, bitsPerPixel = 0;
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER  destLogicalAddr = gcvNULL;
    gctPOINTER  phyAddr = gcvNULL;
    HALNativeDisplayType tmpDisplay;
    gctPOINTER tDestLogicalAddr[3] = {0, 0, 0};
    gctUINT destPhys[3] = {0,0,0};
    gctUINT videoNode = 0;
    gctBOOL mapped = gcvFALSE;
    int tStride = 0;
    gcmHEADER_ARG("Display=0x%x Pixmap=0x%x", Display, Pixmap);

    if (Pixmap == 0)
    {
        /* Pixmap is not a valid pixmap data structure pointer. */
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    if ( Width == NULL && Height == NULL
          && BitsPerPixel == NULL && Stride == NULL
          && Bits == NULL )
    {
        /*UnLock lokced videonode */
        if (_pixmapMapped(Pixmap, &destLogicalAddr, &phyAddr, &tmpDisplay, (gctINT *)&tStride) == gcvFALSE)
        {
            gcmFOOTER();
            status = gcvSTATUS_OK;
            return status;
        }
        if (Display == (HALNativeDisplayType)gcvNULL)
        {
            Display = tmpDisplay;
        }
        status = _getPixmapDrawableInfo(Display, Pixmap, &videoNode, &tStride);
        if ((status == gcvSTATUS_OK) && videoNode)
        {
            _UnlockVideoNode(0, videoNode);
            _unSetPixmapMapped(Pixmap);
        }
        gcmFOOTER();
        return status;
    }

    /* Query pixmap parameters. */
    if (XGetGeometry(Display,
        Pixmap,
        &rootWindow,
        &x, &y, &width, &height,
        &borderWidth,
        &bitsPerPixel) == False)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        gcmFOOTER();
        return status;
    }

    /* Set back values. */
    if (Width != NULL)
    {
        *Width = width;
    }

    if (Height != NULL)
    {
        *Height = height;
    }

    mapped = _pixmapMapped(Pixmap, &tDestLogicalAddr[0], &phyAddr, &tmpDisplay, (gctINT *)&tStride);

    destLogicalAddr = tDestLogicalAddr[0];
    /* 1st pass: Get Logical & Physical address and stride through DRI */
    if ((Bits != NULL) && (Stride != NULL))
    {
        if ( mapped == gcvFALSE )
        {
                status = _getPixmapDrawableInfo(Display, Pixmap, &videoNode, Stride);
                if ((status == gcvSTATUS_OK) && videoNode)
                {
                    if (_LockVideoNode(0, videoNode, &destPhys[0], &tDestLogicalAddr[0]) == gcvSTATUS_MEMORY_LOCKED)
                    {
                        _UnlockVideoNode(0, videoNode);
                        _LockVideoNode(0, videoNode, &destPhys[0], &tDestLogicalAddr[0]);
                    }
                    _setPixmapMapped(Pixmap, tDestLogicalAddr[0], (gctPOINTER)destPhys[0], Display, *Stride);
                    phyAddr = (gctPOINTER)destPhys[0];
                    tStride = *Stride;
                }
        }
        *Bits = phyAddr;
        destLogicalAddr = tDestLogicalAddr[0];
    }

    /* 2nd pass: Get the locally stored Logical address */
    if ((BitsPerPixel != NULL) && (Bits != NULL))
    {
        *BitsPerPixel = bitsPerPixel;
        *Bits = destLogicalAddr;
    }

    if ( Stride != NULL )
        *Stride = tStride;

    gcmFOOTER_NO();
    return status;
}

gceSTATUS
gcoOS_DestroyPixmap(
    IN HALNativeDisplayType Display,
    IN HALNativePixmapType Pixmap
    )
{
    if (Pixmap != 0)
    {
        XFreePixmap(Display, Pixmap);
        XFlush(Display);
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
    Screen* xScreen;
    Visual* xVisual;
    gctINT     status;
    XImage* xImage = gcvNULL;
    GC gc = NULL;

    Drawable pixmap = Pixmap;
    gceSTATUS eStatus = gcvSTATUS_OK;
    gcmHEADER_ARG("Display=0x%x Pixmap=0x%x Left=%d Top=%d Right=%d Bottom=%d Width=%d Height=%d BitsPerPixel=%d Bits=0x%x",
        Display, Pixmap, Left, Top, Right, Bottom, Width, Height, BitsPerPixel, Bits);

    do
    {
        /* Create graphics context. */
        gc = XCreateGC(Display, pixmap, 0, gcvNULL);

        /* Fetch defaults. */
        xScreen = DefaultScreenOfDisplay(Display);
        xVisual = DefaultVisualOfScreen(xScreen);

        /* Create image from the bits. */
        xImage = XCreateImage(Display,
            xVisual,
            BitsPerPixel,
            ZPixmap,
            0,
            (char*) Bits,
            Width,
            Height,
            8,
            Width * BitsPerPixel / 8);

        if (xImage == gcvNULL)
        {
            /* Error. */
            eStatus = gcvSTATUS_OUT_OF_RESOURCES;
            break;
        }

        /* Draw the image. */
        status = XPutImage(Display,
            Pixmap,
            gc,
            xImage,
            Left, Top,           /* Source origin. */
            Left, Top,           /* Destination origin. */
            Right - Left, Bottom - Top);

        if (status != Success)
        {
            /* Error. */
            eStatus = gcvSTATUS_INVALID_ARGUMENT;
            break;
        }

        /* Flush buffers. */
        status = XFlush(Display);

        if (status != Success)
        {
            /* Error. */
            eStatus = gcvSTATUS_INVALID_ARGUMENT;
            break;
        }
    }
    while (0);

    /* Destroy the image. */
    if (xImage != gcvNULL)
    {
        xImage->data = gcvNULL;
        XDestroyImage(xImage);
    }

    /* Free graphics context. */
    XFreeGC(Display, gc);

    /* Return result. */
    gcmFOOTER();
    return eStatus;
}

gceSTATUS
gcoOS_LoadEGLLibrary(
    OUT gctHANDLE * Handle
    )
{
    XInitThreads();
#if defined(LINUX)
        signal(SIGINT,  vdkSignalHandler);
        signal(SIGQUIT, vdkSignalHandler);
#endif

#if defined(__APPLE__)
    return gcoOS_LoadLibrary(gcvNULL, "libEGL.dylib", Handle);
#else
    return gcoOS_LoadLibrary(gcvNULL, "libEGL.so", Handle);
#endif
}

gceSTATUS
gcoOS_FreeEGLLibrary(
    IN gctHANDLE Handle
    )
{
    return gcoOS_FreeLibrary(gcvNULL, Handle);
}

gceSTATUS
gcoOS_ShowWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{
    if((Display == gcvNULL) || (Window == 0))
        return gcvSTATUS_INVALID_ARGUMENT;
    XMapRaised(Display, Window);
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_HideWindow(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window
    )
{
    if((Display == gcvNULL) || (Window == 0))
        return gcvSTATUS_INVALID_ARGUMENT;
    XUnmapWindow(Display, Window);
    return gcvSTATUS_OK;
}

gceSTATUS
gcoOS_SetWindowTitle(
    IN HALNativeDisplayType Display,
    IN HALNativeWindowType Window,
    IN gctCONST_STRING Title
    )
{
    XTextProperty tp;

    if((Display == gcvNULL) || (Window == 0))
        return gcvSTATUS_INVALID_ARGUMENT;

    XStringListToTextProperty((char**) &Title, 1, &tp);

    XSetWMProperties(Display,
        Window,
        &tp,
        &tp,
        gcvNULL,
        0,
        gcvNULL,
        gcvNULL,
        gcvNULL);
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
    XEvent event;
    halKeys scancode;

    if ((Display == gcvNULL) || (Window == 0) || (Event == gcvNULL))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    while (XPending(Display))
    {
        XNextEvent(Display, &event);

        switch (event.type)
        {
        case MotionNotify:
            Event->type = HAL_POINTER;
            Event->data.pointer.x = event.xmotion.x;
            Event->data.pointer.y = event.xmotion.y;
            return gcvSTATUS_OK;

        case ButtonRelease:
            Event->type = HAL_BUTTON;
            Event->data.button.left   = event.xbutton.state & Button1Mask;
            Event->data.button.middle = event.xbutton.state & Button2Mask;
            Event->data.button.right  = event.xbutton.state & Button3Mask;
            Event->data.button.x      = event.xbutton.x;
            Event->data.button.y      = event.xbutton.y;
            return gcvSTATUS_OK;

        case KeyPress:
        case KeyRelease:
            scancode = keys[event.xkey.keycode].normal;

            if (scancode == HAL_UNKNOWN)
            {
                break;
            }

            Event->type                   = HAL_KEYBOARD;
            Event->data.keyboard.pressed  = (event.type == KeyPress);
            Event->data.keyboard.scancode = scancode;
            Event->data.keyboard.key      = (  (scancode < HAL_SPACE)
                || (scancode >= HAL_F1)
                )
                ? 0
                : (char) scancode;
            return gcvSTATUS_OK;
        }
    }

        if (_terminate)
        {
            _terminate = 0;
            Event->type = HAL_CLOSE;
            return gcvSTATUS_OK;
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
    gctINT          screen;
    gceSTATUS status = gcvSTATUS_OK;
    gcmHEADER();
    __DRIDisplay* display;

    do
    {
        display = (__DRIDisplay*) malloc(sizeof(__DRIDisplay));

        if (display == gcvNULL)
        {
            status = gcvSTATUS_OUT_OF_RESOURCES;
            break;
        }
        memset(display, 0, sizeof(__DRIDisplay));

        display->dpy = Display;
        display->contextStack = NULL;
        display->drawableStack = NULL;

        screen = DefaultScreen(Display);

        _GetDisplayInfo(display, screen);

        *localDisplay = (gctPOINTER)display;
        gcmFOOTER_ARG("*localDisplay=0x%x", *localDisplay);
        return status;
    }
    while (0);

    gcmFOOTER();
    return status;
}

gceSTATUS
gcoOS_DeinitLocalDisplayInfo(
    IN HALNativeDisplayType Display,
    IN OUT gctPOINTER * localDisplay
    )
{
    __DRIDisplay* display;

    display = (__DRIDisplay*)*localDisplay;
    if (display != gcvNULL)
    {
        drmUnmap((drmAddress)display->pSAREA, SAREA_MAX);
        drmUnmap((drmAddress)display->linearAddr, display->fbSize);
        drmClose(display->drmFd);
        free(display);
        *localDisplay = gcvNULL;
    }

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
    gceSTATUS status = gcoOS_GetDisplayInfoEx(Display, Window, DisplayInfoSize, DisplayInfo);
    if(gcmIS_SUCCESS(status))
    {
        if ((DisplayInfo->logical == gcvNULL) || (DisplayInfo->physical == ~0U))
        {
            /* No offset. */
            status = gcvSTATUS_NOT_SUPPORTED;
        }
        else
            status = gcvSTATUS_OK;
    }
    return status;
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
    *nativeVisualId = (gctINT)
        DefaultVisual(Display,DefaultScreen(Display))->visualid;
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
    halDISPLAY_INFO info;

    if (gcmIS_ERROR(gcoOS_GetWindowInfo(
                          Display,
                          Window,
                          X,
                          Y,
                          (gctINT_PTR) Width,
                          (gctINT_PTR) Height,
                          (gctINT_PTR) BitsPerPixel,
                          gcvNULL)))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if (gcmIS_ERROR(gcoOS_GetDisplayInfoEx(
        Display,
        Window,
        sizeof(info),
        &info)))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Get the color format. */
    switch (info.greenLength)
    {
    case 4:
        if (info.blueOffset == 0)
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X4R4G4B4 : gcvSURF_A4R4G4B4;
        }
        else
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X4B4G4R4 : gcvSURF_A4B4G4R4;
        }
        break;

    case 5:
        if (info.blueOffset == 0)
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X1R5G5B5 : gcvSURF_A1R5G5B5;
        }
        else
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X1B5G5R5 : gcvSURF_A1B5G5R5;
        }
        break;

    case 6:
        *Format = gcvSURF_R5G6B5;
        break;

    case 8:
        if (info.blueOffset == 0)
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X8R8G8B8 : gcvSURF_A8R8G8B8;
        }
        else
        {
            *Format = (info.alphaLength == 0) ? gcvSURF_X8B8G8R8 : gcvSURF_A8B8G8R8;
        }
        break;

    default:
        /* Unsupported colot depth. */
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Success. */
    return gcvSTATUS_OK;
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
    return gcoOS_DrawImage(Display,
                           Window,
                           Left,
                           Top,
                           Right,
                           Bottom,
                           Width,
                           Height,
                           BitsPerPixel,
                           Bits);
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

    gctPOINTER logicalAddress = 0;
    gctINT XStride;
    gctPOINTER XBits;

/* The first calling gcoOS_GetPixmapInfo makes pixmap cache-pool work */
    if (gcmIS_ERROR(gcoOS_GetPixmapInfo(
                        Display,
                        Pixmap,
                        gcvNULL,
                        gcvNULL,
                        gcvNULL,
                        &XStride,
                        &XBits)))
    {
        return gcvFALSE;
    }

    if (gcmIS_ERROR(gcoOS_GetPixmapInfo(
                        Display,
                        Pixmap,
                        (gctINT_PTR) Width, (gctINT_PTR) Height,
                        (gctINT_PTR) BitsPerPixel,
                        gcvNULL,
                        &logicalAddress)))
    {
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    /* Return format for pixmap depth. */
    switch (*BitsPerPixel)
    {
    case 16:
        *Format = gcvSURF_R5G6B5;
        *BitsPerPixel = 16;
        break;

    case 24:
        *Format = gcvSURF_X8R8G8B8;
        *BitsPerPixel = 24;
        break;
    case 32:
        *Format = gcvSURF_A8R8G8B8;
        *BitsPerPixel = 32;
        break;

    default:
        return gcvSTATUS_INVALID_ARGUMENT;
    }

    if ( Bits != gcvNULL )
        *Bits = logicalAddress;

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
    gctINT status = gcvSTATUS_OK;
    Window rootWindow;
    XImage *img = 0;
    int x = 0, y = 0;
    unsigned int  w = 0, h = 0, bitsPerPixel = 0, borderWidth = 0;

    switch (DstFormat)
    {
    case gcvSURF_R5G6B5:
        break;

    case gcvSURF_X8R8G8B8:
    case gcvSURF_A8R8G8B8:
        break;

    default:
       printf("gcoOS_GetPixmapInfo error format\n");
        return gcvSTATUS_INVALID_ARGUMENT;
    }

   if (XGetGeometry(Display,
        Pixmap,
        &rootWindow,
        &x, &y, &w, &h,
        &borderWidth,
        &bitsPerPixel) == False)
    {
        printf("gcoOS_GetPixmapInfo error\n");
        status = gcvSTATUS_INVALID_ARGUMENT;
        return status;
    }
    else
    {
        img = XGetImage(Display, Pixmap, x, y, w, h, AllPlanes, ZPixmap);
    }

    if (img && DstBits)
    {
         gctUINT8* src = (gctUINT8*)img->data + (img->xoffset * img->bits_per_pixel >> 3);
         gctUINT8* dst = (gctUINT8*)DstBits;
         gctINT SrcStride = img->bytes_per_line;
         gctINT len = gcmMIN(DstStride, SrcStride);
         gctINT height = gcmMIN(h, DstHeight);

         if (SrcStride == DstStride)
         {
             gcoOS_MemCopy(dst, src, len * height);
         }
         else
         {
             gctUINT n;
             for (n = 0; n < height; n++)
             {
                 gcoOS_MemCopy(dst, src, len);
                 src += SrcStride;
                 dst += DstStride;
             }
         }
    }

    if (img) XDestroyImage(img);

    return status;
}

#endif
#endif

