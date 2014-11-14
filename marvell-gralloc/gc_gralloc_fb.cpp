/****************************************************************************
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*****************************************************************************/

#include <sys/mman.h>

#include <dlfcn.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <linux/fb.h>

#include "gc_gralloc_priv.h"
#include "gc_gralloc_gr.h"

#if MRVL_SUPPORT_DISPLAY_MODEL
#include "dms_if_client.h"
#endif

/*
     FRAMEBUFFER_PIXEL_FORMAT

         Set framebuffer pixel format to use for android.
         If it is set to '0', current pixel format is detected and set.

         Available format values are:
             HAL_PIXEL_FORMAT_RGBA_8888 : 1
             HAL_PIXEL_FORMAT_RGBX_8888 : 2
             HAL_PIXEL_FORMAT_RGB_888   : 3
             HAL_PIXEL_FORMAT_RGB_565   : 4
             HAL_PIXEL_FORMAT_BGRA_8888 : 5
 */
#ifndef FRAMEBUFFER_PIXEL_FORMAT
#define FRAMEBUFFER_PIXEL_FORMAT  5
#endif

/*
     NUM_BUFFERS

         Numbers of buffers of framebuffer device for page flipping.

         Normally it can be '2' for double buffering, or '3' for tripple
         buffering.
         This value should equal to (yres_virtual / yres).
 */
#define NUM_BUFFERS               3


/*
     NUM_PAGES_MMAP

         Numbers of pages to mmap framebuffer to userspace.

         Normally the total buffer size should be rounded up to page boundary
         and mapped to userspace. On some platform, maping the rounded size
         will fail. Set non-zero value to specify the page count to map.
 */
#define NUM_PAGES_MMAP            0


/* Framebuffer pixel format table. */
static const struct
{
    int      format;
    uint32_t bits_per_pixel;
    uint32_t red_offset;
    uint32_t red_length;
    uint32_t green_offset;
    uint32_t green_length;
    uint32_t blue_offset;
    uint32_t blue_length;
    uint32_t transp_offset;
    uint32_t transp_length;
}
formatTable[] =
{
    {HAL_PIXEL_FORMAT_RGBA_8888, 32,  0, 8,  8, 8,  16, 8, 24, 8},
    {HAL_PIXEL_FORMAT_RGBX_8888, 32,  0, 8,  8, 8,  16, 8,  0, 0},
    {HAL_PIXEL_FORMAT_RGB_888,   24,  0, 8,  8, 8,  16, 8,  0, 0},
    {HAL_PIXEL_FORMAT_RGB_565,   16, 11, 5,  5, 6,   0, 5,  0, 0},
    {HAL_PIXEL_FORMAT_BGRA_8888, 32, 16, 8,  8, 8,   0, 8, 24, 8}
};

/* Framebuffer pixel format. */
static int format = FRAMEBUFFER_PIXEL_FORMAT;

#if MRVL_SUPPORT_DISPLAY_MODEL
android::sp<android::IDisplayModel> displayModel = NULL;
#endif

struct fb_context_t
{
    framebuffer_device_t device;
};

/*******************************************************************************
**
**  fb_setSwapInterval
**
**  Set framebuffer swap interval.
**
**  INPUT:
**
**      framebuffer_device_t * Dev
**          Specified framebuffer device.
**
**      int Interval
**          Specified Interval value.
**
**  OUTPUT:
**
**      Nothing.
*/
static int
fb_setSwapInterval(
    struct framebuffer_device_t * Dev,
    int Interval
    )
{
    if ((Interval < Dev->minSwapInterval)
    ||  (Interval > Dev->maxSwapInterval)
    )
    {
        return -EINVAL;
    }

    /* FIXME: implement fb_setSwapInterval. */
    return 0;
}

/*******************************************************************************
**
**  fb_post
**
**  Post back buffer to display.
**
**  INPUT:
**
**      framebuffer_device_t * Dev
**          Specified framebuffer device.
**
**      int Buffer
**          Specified back buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
static int
fb_post(
    struct framebuffer_device_t * Dev,
    buffer_handle_t Buffer
    )
{
    if (!Buffer)
    {
        return -EINVAL;
    }
    if (gc_private_handle_t::validate(Buffer) < 0)
    {
        return -EINVAL;
    }

    gc_private_handle_t const * hnd =
        reinterpret_cast<gc_private_handle_t const *>(Buffer);

    private_module_t * m =
        reinterpret_cast<private_module_t *>(Dev->common.module);

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
    {
        const size_t offset = hnd->base - m->framebuffer->base;

        m->info.activate = FB_ACTIVATE_VBL;
        m->info.yoffset  = offset / m->finfo.line_length;
        {
            // m->info.yoffset += hnd->fbpost_offset;
        }

#ifndef FRONT_BUFFER
#if MRVL_SUPPORT_DISPLAY_MODEL
        if (displayModel != NULL) {
            for (int i = 0; i < displayModel->CRTC_GetCurrentCrtcCount(); i++)
            {
                if (displayModel->CRTC_CheckFlipMode(i))
                    continue;

                displayModel->CRTC_FlipTo(i, -1, m->info.yoffset);
            }
        } else {
#else
        if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1)
        {
            ALOGE("FBIOPAN_DISPLAY failed");

            m->base.unlock(&m->base, Buffer);

            return -errno;
        }
#endif
#if MRVL_SUPPORT_DISPLAY_MODEL
        }
#endif
#endif
        m->currentBuffer = Buffer;
    }
    else
    {
        ALOGE("Cannot post this buffer");
    }

    return 0;
}

/*******************************************************************************
**
**  fb_compositionComplete
**
**  Sync with the hardware.
**
**  INPUT:
**
**      framebuffer_device_t * Dev
**          Specified framebuffer device.
**
**  OUTPUT:
**
**      Nothing.
*/


/*******************************************************************************
**
**  mapFrameBufferLocked
**
**  Open framebuffer device and initialize.
**
**  INPUT:
**
**      private_module_t * Module
**          Specified gralloc module.
**
**  OUTPUT:
**
**      Nothing.
*/
int
mapFrameBufferLocked(
    struct private_module_t * Module
    )
{
    /* already initialized... */
    if (Module->framebuffer)
    {
        return 0;
    }

    char const * const device_template[] =
    {
        "/dev/graphics/fb%u",
        "/dev/fb%u",
        0
    };

    int     fd = -1;
    uint32_t i = 0;
    char name[64];

    /* Open framebuffer device. */
    while ((fd == -1) && device_template[i])
    {
        snprintf(name, 64, device_template[i], 0);
        fd = open(name, O_RDWR, 0);
        i++;
    }

    if (fd < 0)
    {
        ALOGE("Can not find a valid frambuffer device.");

        return -errno;
    }

    /* Get fix screen info. */
    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        ALOGE("ioctl(FBIOGET_FSCREENINFO) failed.");
        return -errno;
    }

    /* Get variable screen info. */
    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
    {
        ALOGE("ioctl(FBIOGET_FSCREENINFO) failed.");
        return -errno;
    }

    /* Set pixel format. */
    info.reserved[0]    = 0;
    info.reserved[1]    = 0;
    info.reserved[2]    = 0;
    info.xoffset        = 0;
    info.yoffset        = 0;
    info.activate       = FB_ACTIVATE_NOW;

    char prop_value[PROPERTY_VALUE_MAX];
    char default_value[PROPERTY_VALUE_MAX] = { '\0' };
    int max_format = sizeof(formatTable)/sizeof(formatTable[0]) + 1;
    sprintf(default_value, "%d",  max_format);
    property_get("persist.sys.display.format", prop_value, default_value);
    if (strcmp( prop_value, default_value)) {
        ALOGI("Default framebuffer pixel format=%s", prop_value);
        format = prop_value[0] - '0';
    }

    if (format > 0)
    {
        /* Find specified pixel format info in table. */
        for (i = 0;
             i < sizeof (formatTable) / sizeof (formatTable[0]);
             i++)
        {
             if (formatTable[i].format == format)
             {
                 /* Set pixel format detail. */
                 info.bits_per_pixel = formatTable[i].bits_per_pixel;
                 info.red.offset     = formatTable[i].red_offset;
                 info.red.length     = formatTable[i].red_length;
                 info.green.offset   = formatTable[i].green_offset;
                 info.green.length   = formatTable[i].green_length;
                 info.blue.offset    = formatTable[i].blue_offset;
                 info.blue.length    = formatTable[i].blue_length;
                 info.transp.offset  = formatTable[i].transp_offset;
                 info.transp.length  = formatTable[i].transp_length;

                 break;
             }
        }

        if (i == sizeof (formatTable) / sizeof (formatTable[0]))
        {
            /* Can not find format info in table. */
            ALOGW("Unkown format specified: %d", format);

            /* Set to unknown format. */
            format = 0;
        }
    }

    /* Try default format if specified format not found. */
    if (format == 0)
    {
         /* Find default pixel format id. */
         for (i = 0;
              i < sizeof (formatTable) / sizeof (formatTable[0]);
              i++)
         {
             if ((info.bits_per_pixel == formatTable[i].bits_per_pixel)
             &&  (info.red.offset     == formatTable[i].red_offset)
             &&  (info.red.length     == formatTable[i].red_length)
             &&  (info.green.offset   == formatTable[i].green_offset)
             &&  (info.green.length   == formatTable[i].green_length)
             &&  (info.blue.offset    == formatTable[i].blue_offset)
             &&  (info.blue.length    == formatTable[i].blue_length)
          /* &&  (info.transp.offset  == formatTable[i].transp_offset) */
             &&  (info.transp.length  == formatTable[i].transp_length)
             )
             {
                 format = formatTable[i].format;
                 break;
             }
         }

         if (format == 0)
         {
            /* Can not find format info in table. */
             ALOGE("Unknown default pixel format");

             return -EFAULT;
         }

         ALOGI("Use default framebuffer pixel format=%d", format);
    }

    /* Request NUM_BUFFERS screens (at least 2 for page flipping) */
    //info.yres_virtual = info.yres * NUM_BUFFERS;
    info.yres_virtual = gcmALIGN(info.yres, 4)* NUM_BUFFERS;
    /* Align xres to 16 multiple and set to xres_virtual as frame buffer actual stride */
    info.xres_virtual = gcmALIGN(info.xres, 16);

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1)
    {
        info.yres_virtual = info.yres;

        ALOGE("FBIOPUT_VSCREENINFO failed, page flipping not supported");
    }

#ifndef FRONT_BUFFER
#if MRVL_SUPPORT_DISPLAY_MODEL
    if (displayModel != NULL) {
        int width, height;
        displayModel->CRTC_GetVirtualScreenResolution(&width, &height);
        info.yres_virtual = height * NUM_BUFFERS;
    } else {
        info.yres_virtual = info.yres * NUM_BUFFERS;
    }
#else
    if (info.yres_virtual < info.yres * 2)
    {
        /* we need at least 2 for page-flipping. */
        info.yres_virtual = info.yres;

        ALOGW("page flipping not supported "
             "(yres_virtual=%d, requested=%d)",
             info.yres_virtual,
             info.yres * 2);
    }
#endif
#endif

    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
    {
        ALOGE("ioctl(FBIOGET_VSCREENINFO) failed.");

        return -errno;
    }

    uint64_t  refreshQuotient =
    (
            uint64_t( info.upper_margin + info.lower_margin + info.yres + info.vsync_len)
            * ( info.left_margin  + info.right_margin + info.xres + info.hsync_len)
            * info.pixclock
    );

    /* Beware, info.pixclock might be 0 under emulation, so avoid a
     * division-by-0 here (SIGFPE on ARM) */
    int refreshRate = refreshQuotient > 0 ? (int)(1000000000000000LLU / refreshQuotient) : 0;

    if (refreshRate == 0) {
        /* bleagh, bad info from the driver */
        refreshRate = 60*1000;  /* 60 Hz */
    }

    if (int(info.width) <= 0 || int(info.height) <= 0)
    {
        /* the driver doesn't return that information default to 160 dpi. */
        info.width  = 153; // ((info.xres * 25.4f) / 160.0f + 0.5f);
        info.height = 115; // ((info.yres * 25.4f) / 160.0f + 0.5f);
    }

    float xdpi = (info.xres * 25.4f) / info.width;
    float ydpi = (info.yres * 25.4f) / info.height;
    float fps  = refreshRate / 1000.0f;

    ALOGI("using (fd=%d)\n"
         "id           = %s\n"
         "xres         = %d px\n"
         "yres         = %d px\n"
         "xres_virtual = %d px\n"
         "yres_virtual = %d px\n"
         "bpp          = %d\n"
         "r            = %2u:%u\n"
         "g            = %2u:%u\n"
         "b            = %2u:%u\n",
         fd,
         finfo.id,
         info.xres,
         info.yres,
         info.xres_virtual,
         info.yres_virtual,
         info.bits_per_pixel,
         info.red.offset, info.red.length,
         info.green.offset, info.green.length,
         info.blue.offset, info.blue.length);

    ALOGI("width        = %d mm (%f dpi)\n"
         "height       = %d mm (%f dpi)\n"
         "refresh rate = %.2f Hz\n"
         "Framebuffer phys addr = %p\n",
         info.width,  xdpi,
         info.height, ydpi,
         fps,
         (void *) finfo.smem_start);


    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        return -errno;
    }

    if (finfo.smem_len <= 0)
    {
        return -errno;
    }

    Module->info  = info;
    Module->finfo = finfo;
    Module->xdpi  = xdpi;
    Module->ydpi  = ydpi;
    Module->fps   = fps;

    /* map the framebuffer */
    size_t fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);

    /* Check pages for mapping. */
    if (NUM_PAGES_MMAP != 0)
    {
        fbSize = NUM_PAGES_MMAP * PAGE_SIZE;
    }

    Module->framebuffer = new gc_private_handle_t(dup(fd), fbSize, 0);
    Module->framebuffer->master = open("/dev/null", O_RDONLY);

#ifndef FRONT_BUFFER
#if MRVL_SUPPORT_DISPLAY_MODEL
    if (displayModel != NULL)
        Module->numBuffers = 2;
    else
        Module->numBuffers = info.yres_virtual / info.yres;
#else
    Module->numBuffers = info.yres_virtual / info.yres;
#endif
#else
    Module->numBuffers = 1;
#endif
    Module->bufferMask = 0;

    void * vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (vaddr == MAP_FAILED)
    {
        ALOGE("Error mapping the framebuffer (%s)", strerror(errno));

        return -errno;
    }

    Module->framebuffer->base = intptr_t(vaddr);
    Module->framebuffer->physAddr = intptr_t(finfo.smem_start);
    //memset(vaddr, 0, fbSize);

    return 0;
}

/*******************************************************************************
**
**  mapFrameBuffer
**
**  Open framebuffer device and initialize (call mapFrameBufferLocked).
**
**  INPUT:
**
**      private_module_t * Module
**          Specified gralloc module.
**
**  OUTPUT:
**
**      Nothing.
*/
static int
mapFrameBuffer(
    struct private_module_t* Module
    )
{
    pthread_mutex_lock(&Module->lock);

    int err = mapFrameBufferLocked(Module);

    pthread_mutex_unlock(&Module->lock);

    return err;
}

/*******************************************************************************
**
**  fb_close
**
**  Close framebuffer device.
**
**  INPUT:
**
**      struct hw_device_t * Dev
**          Specified framebuffer device.
**
**  OUTPUT:
**
**      Nothing.
*/
static int
fb_close(
    struct hw_device_t * Dev
    )
{
    fb_context_t * ctx = (fb_context_t*) Dev;

    if (ctx != NULL)
    {
        free(ctx);
    }

    return 0;
}

/*******************************************************************************
**
**  fb_open
**
**  Open framebuffer device.
**
**  INPUT:
**
**      hw_module_t const * Module
**          Specified gralloc module.
**
**      const char * Name
**          Specified framebuffer device name.
**
**  OUTPUT:
**
**      hw_device_t ** Device
**          Framebuffer device handle.
*/
int
fb_device_open(
    hw_module_t const * Module,
    const char * Name,
    hw_device_t ** Device
    )
{
    int status = -EINVAL;

    if (!strcmp(Name, GRALLOC_HARDWARE_FB0))
    {
        alloc_device_t * gralloc_device;

        status = gralloc_open(Module, &gralloc_device);
        if (status < 0)
        {
            return status;
        }

        /* Initialize our state here */
        fb_context_t * dev = (fb_context_t *) malloc(sizeof (*dev));
        memset(dev, 0, sizeof (*dev));

        /* initialize the procs */
        dev->device.common.tag          = HARDWARE_DEVICE_TAG;
        dev->device.common.version      = 0;
        dev->device.common.module       = const_cast<hw_module_t*>(Module);
        dev->device.common.close        = fb_close;
        dev->device.setSwapInterval     = fb_setSwapInterval;
        dev->device.post                = fb_post;
        dev->device.setUpdateRect       = 0;
        /* This function will be called in surfaceflinger. Set it to
         * 'fb_compositionComplete' if you need a hardware sync. Normally for
         * copybit composition (deprecated). */
        dev->device.compositionComplete = NULL;

#if MRVL_SUPPORT_DISPLAY_MODEL
        android::sp<android::ProcessState> proc(android::ProcessState::self());
        android::sp<android::IServiceManager> sm = android::defaultServiceManager();
        android::sp<android::IBinder> binder = sm->getService(android::String16("DisplayModel"));
        if (binder != NULL) {
            ALOGI("get DisplayModule service proxy\n");
            displayModel = android::interface_cast<android::IDisplayModel>(binder);
            int sum = displayModel->CRTC_GetCurrentCrtcCount();

            for(int i = 0; i < sum; i++) {
                ALOGI("CRTC %d : name %s\n", i, displayModel->CRTC_GetNameByIndex(i));
                ALOGI("    OUTPUT name : %s\n", displayModel->CRTC_GetCurrentOutput(i));
                ALOGI("        MODE : %s \n", displayModel->OUTPUT_GetCurrentModeName(displayModel->CRTC_GetCurrentOutput(i)));
            }
        } else {
            ALOGE("fail to get DisplayModule service proxy\n");
        }

        if (displayModel != NULL) {
            displayModel->CRTC_GetCurrentFormat(0, &format);
        }
#endif

        private_module_t * m = (private_module_t *) Module;
        status = mapFrameBuffer(m);

        if (status >= 0)
        {
            int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);

#if MRVL_SUPPORT_DISPLAY_MODEL
            int32_t width = m->info.xres;
            int32_t height = m->info.yres;

            if (displayModel != NULL) {
                displayModel->CRTC_GetVirtualScreenResolution(&width, &height);
            }
#endif

            const_cast<uint32_t&>(dev->device.flags)      = 0;
            const_cast<uint32_t&>(dev->device.width)      = m->info.xres;
            const_cast<uint32_t&>(dev->device.height)     = m->info.yres;
            const_cast<int&>(dev->device.stride)          = stride;
            const_cast<int&>(dev->device.format)          = format;
            const_cast<float&>(dev->device.xdpi)          = m->xdpi;
            const_cast<float&>(dev->device.ydpi)          = m->ydpi;
            const_cast<float&>(dev->device.fps)           = m->fps;
            const_cast<int&>(dev->device.minSwapInterval) = 1;
            const_cast<int&>(dev->device.maxSwapInterval) = 1;
#if ANDROID_SDK_VERSION >= 17
            const_cast<int&>(dev->device.numFramebuffers) = NUM_BUFFERS;
#endif

            *Device = &dev->device.common;
        }
    }

    return status;
}

