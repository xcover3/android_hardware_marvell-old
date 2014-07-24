/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
* (C) Copyright 2010 Marvell International Ltd.
* All Rights Reserved
*
* MARVELL CONFIDENTIAL
* Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
* The source code contained or described herein and all documents related to
* the source code ("Material") are owned by Marvell International Ltd or its
* suppliers or licensors. Title to the Material remains with Marvell International Ltd
* or its suppliers and licensors. The Material contains trade secrets and
* proprietary and confidential information of Marvell or its suppliers and
* licensors. The Material is protected by worldwide copyright and trade secret
* laws and treaty provisions. No part of the Material may be used, copied,
* reproduced, modified, published, uploaded, posted, transmitted, distributed,
* or disclosed in any way without Marvell's prior express written permission.
*
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or delivery
* of the Materials, either expressly, by implication, inducement, estoppel or
* otherwise. Any license under such intellectual property rights must be
* express and approved by Marvell in writing.
*
*/

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>
#include <cutils/properties.h>
#include <utils/String8.h>
#include <utils/Mutex.h>

#include <EGL/egl.h>

#ifdef ENABLE_OVERLAY
#include "HWOverlayComposer.h"
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
#include "GcuEngine.h"
#include "HWVirtualComposer.h"
#endif
#include "HWCFenceManager.h"

#ifdef ENABLE_HWC_GC_PATH
#include "HWBaselayComposer.h"
#endif

#include "HWCDisplayEventMonitor.h"
#include "gralloc_priv.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

using namespace android;
/*****************************************************************************/

#define HWC_1_1 1
#if HWC_1_1
#define HWC_VERSION HWC_DEVICE_API_VERSION_1_2
#else
#define HWC_VERSION HWC_DEVICE_API_VERSION_1_0
#endif

struct hwc_context_t {
    hwc_composer_device_1_t device;
    /* our private state goes below here */
#ifdef ENABLE_OVERLAY
    HWOverlayComposer *overlayComposer;
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
    HWVirtualComposer *virtualComposer;
#endif
#ifdef ENABLE_HWC_GC_PATH
    HWBaselayComposer *baseComposer;
#endif

    HWCFenceManager *pFenceManager;

    hwc_procs_t *procs;
    bool skip;

    // total display device must keep same as MAX_DISPLAYS define in surfaceflinger/DisplayHardware/HWComposer.h
    bool disp_actived[HWC_NUM_DISPLAY_TYPES + 3];
    framebuffer_device_t *fbdev[HWC_NUM_DISPLAY_TYPES + 3];

    sp<HWCDisplayEventMonitor> monitor;
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: (uint16_t)HWC_DEVICE_API_VERSION_1_0,
        hal_api_version: (uint16_t)HARDWARE_HAL_API_VERSION,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Marvell Hardware Composer HAL",
        author: "Marvel PIE",
        methods: &hwc_module_methods,
        dso : NULL,
        reserved : {0,0,0,0,0,0,0,0,0,0,
                  0,0,0,0,0,0,0,0,0,0,
                  0,0,0,0,0},
    }
};

/*****************************************************************************/
static void dump_layer(hwc_layer_1_t const* l) {
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays) {
    ATRACE_CALL();
    if (displays) {
        struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
        uint32_t numRestDisplays = numDisplays;
#ifdef ENABLE_OVERLAY
        if( !ctx->skip && ctx->overlayComposer ) {
            ctx->overlayComposer->prepare(numDisplays, displays);
        }
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
        if( !ctx->skip && ctx->virtualComposer ) {
            numRestDisplays = ctx->virtualComposer->prepare(numDisplays, displays);
        }
#endif
#ifdef ENABLE_HWC_GC_PATH
        if(ctx->baseComposer)
            ctx->baseComposer->prepare(&ctx->device, numRestDisplays, displays);
#endif
    }
    return 0;
}

static int hwc_set(struct hwc_composer_device_1 *dev,
                size_t numDisplays, hwc_display_contents_1_t** displays)
{
    ATRACE_CALL();
    int status = 0;
    uint32_t numRestDisplays = numDisplays;
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
#ifdef ENABLE_WFD_OPTIMIZATION
    if(!ctx->skip && ctx->virtualComposer && ctx->virtualComposer->isRunning()){
        numRestDisplays = HWC_NUM_DISPLAY_TYPES;
    }
#endif
#ifdef ENABLE_HWC_GC_PATH
    if(ctx->baseComposer)
    {
        status = ctx->baseComposer->set(&ctx->device, numRestDisplays, displays);
    }
    else
    {
#endif

#if !HWC_1_1
        /* Only swap buffer for HWC1.0 3d path, HWC1.1 3d Path swapbuffers in doComposite */
        EGLBoolean sucess = eglSwapBuffers((EGLDisplay)displays[0]->dpy, (EGLSurface)displays[0]->sur);
        status = sucess ? status : HWC_EGL_ERROR;
#endif
#ifdef ENABLE_HWC_GC_PATH
    }
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
    if( !ctx->skip && ctx->virtualComposer && ctx->virtualComposer->isRunning()) {
        ctx->virtualComposer->set(numDisplays, displays);
    }
#endif
#ifdef ENABLE_OVERLAY
    if(!ctx->skip && ctx->overlayComposer) {
        ctx->overlayComposer->set(numDisplays, displays);
    }
#endif
#if HWC_1_1
    {
        /* Post the frame buffer for HWC1.1, HWC1.0 will post buffer in fbPost */
        for(size_t i = 0; i < numDisplays; i++)
        {
            if(ctx->fbdev[i] != NULL && ctx->disp_actived[i])
            {
                bool isMultiOverlay = true;
                for(size_t j = 0; j < displays[i]->numHwLayers - 1; j++)
                {
                    if(displays[i]->hwLayers[j].compositionType != HWC_OVERLAY)
                    {
                        isMultiOverlay = false;
                        break;
                    }
                }
                if(!isMultiOverlay)
                {
                    ctx->fbdev[i]->post(ctx->fbdev[i], displays[i]->hwLayers[displays[i]->numHwLayers - 1].handle);
                }
            }
        }
    }
#endif
#ifdef ENABLE_OVERLAY
    if(!ctx->skip && ctx->overlayComposer) {
        ctx->overlayComposer->finishCompose();
    }
#endif
    return status;
}

static int hwc_blank(struct hwc_composer_device_1* dev, int disp, int blank)
{
    int status = 0;
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

    /* Check device. */
    if(ctx == NULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if(disp > HWC_NUM_DISPLAY_TYPES)
    {
        /* Invalid parameters. */
        ALOGE("%s(%d): Invalid parameters!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    ctx->disp_actived[disp] = !blank;

#ifdef ENABLE_HWC_GC_PATH
    if(ctx->baseComposer)
    {
        status = ctx->baseComposer->blank(&ctx->device, disp, blank);
    }
#endif

    return status;

}

static int hwc_getDisplayConfigs(struct hwc_composer_device_1* dev, int disp,
            uint32_t* configs, size_t* numConfigs)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

     /* Check device. */
    if (ctx == NULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if (configs == NULL || numConfigs == NULL || *numConfigs == 0)
    {
        /* Invalid parameters. */
        ALOGE("%s(%d): Invalid parameters!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    /*
     * TODO (VENDOR): Support external display.
     * Currenty only primary display is supported.
     */
    if (disp == HWC_DISPLAY_PRIMARY)
    {
        /*
         * TODO (VENDOR): Support multiple configs.
         * Current only one config is supported.
         */
        configs[0]  = 0;
        *numConfigs = 1;
        return 0;
    }
    else
    {
        /*
         * TODO (VENDOR): Support external display.
         * Currently external display is not supported.
         */
        ALOGE("%s(%d): TODO: disp %d not supported!",
             __FUNCTION__, __LINE__, disp);
        return -EINVAL;
    }

    return 0;
}

static int hwc_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp,
            uint32_t config, const uint32_t* attributes, int32_t* values)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

    /* Check device. */
    if (ctx == NULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    if (attributes == NULL || values == 0)
    {
        /* Invalid parameters. */
        ALOGE("%s(%d): Invalid parameters!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    /*
     * TODO (VENDOR): Support external display.
     * Currently only primary display is supported.
     * */
    if (disp != HWC_DISPLAY_PRIMARY)
    {
        ALOGE("%s(%d): TODO: disp %d not supported!",
             __FUNCTION__, __LINE__, disp);
        return -EINVAL;
    }

    /*
     * TODO (VENDOR): Support mutiple configs.
     * Currently only one config is supported.
     */
    if (config != 0)
    {
        ALOGE("%s(%d): TODO: config %d not supported!",
             __FUNCTION__, __LINE__, config);
        return 0;
    }

    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++)
    {
        switch (attributes[i])
        {
        case HWC_DISPLAY_VSYNC_PERIOD:
            values[i] = nsecs_t(1e9/ctx->fbdev[disp]->fps);
            break;

        case HWC_DISPLAY_WIDTH:
            values[i] = ctx->fbdev[disp]->width;
            break;

        case HWC_DISPLAY_HEIGHT:
            values[i] = ctx->fbdev[disp]->height;
            break;

        case HWC_DISPLAY_DPI_X:
            values[i] = ctx->fbdev[disp]->xdpi;
            break;

        case HWC_DISPLAY_DPI_Y:
            values[i] = ctx->fbdev[disp]->ydpi;
            break;

        case HWC_DISPLAY_FORMAT:
            values[i] = ctx->fbdev[disp]->format;
            break;

        default:
            return -EINVAL;
        }
    }

    return 0;
}

static void hwc_dump(hwc_composer_device_1_t *dev,
        char *buff,
        int buff_len) {
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    String8 result;
    char buffer[1024];
#ifdef ENABLE_OVERLAY
    if(ctx->overlayComposer){
        ctx->overlayComposer->dump(result, buffer, 1024);
        strncpy(buff, result.string(), buff_len - 1);
    }
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
    if(ctx->virtualComposer){
        ctx->virtualComposer->dump(result, buffer, 1024);
        strncpy(buff, result.string(), buff_len - 1);
    }
#endif
}

static int hwc_query(hwc_composer_device_1_t *dev,
        int what, int* value)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we don't support the background layer yet
        value[0] = 0;
        break;

    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = 1000000000.0 / ctx->fbdev[HWC_DISPLAY_PRIMARY]->fps;
        break;

    case HWC_DISPLAY_TYPES_SUPPORTED:
        /*
         * TODO (VENDOR): Support multiple displays.
         * Currently only primary display is supported.
         */
        *value = HWC_DISPLAY_PRIMARY_BIT;
        break;

    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static void hwc_registerProcs(hwc_composer_device_1_t* dev,
                                    hwc_procs_t const* procs)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;

    ctx->procs = (typeof(ctx->procs)) procs;
    if( !ctx->monitor.get() ) {
        ctx->monitor = new HWCDisplayEventMonitor(ctx->procs);
    }
}



static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx) {
#ifdef ENABLE_OVERLAY
        if(ctx->overlayComposer)
            delete ctx->overlayComposer;
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
        if(ctx->virtualComposer)
            delete ctx->virtualComposer;
#endif
#ifdef ENABLE_HWC_GC_PATH
        if(ctx->baseComposer)
        {
            ctx->baseComposer->close(dev);
            delete ctx->baseComposer;
        }
#endif

        for(int i = 0; i <= HWC_NUM_DISPLAY_TYPES; i++)
        {
            if(ctx->fbdev[i] != NULL)
                framebuffer_close(ctx->fbdev[i]);
        }

        free(ctx);
    }
    return 0;
}

static int hwc_event_control(hwc_composer_device_1_t* dev, int disp,
        int event, int enabled)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
    {
        if( ctx->monitor.get() ) {
            /* TO DO, need transfer the disp to correct fb device */
            ctx->monitor->eventControl(event, enabled);
        }
        return 0;
    }
    default:
        return -EINVAL;
    }
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;
    hw_module_t const *gralloc;

    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_VERSION;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
        dev->device.dump = hwc_dump;
        dev->device.blank = hwc_blank;
        dev->device.getDisplayConfigs = hwc_getDisplayConfigs;
        dev->device.getDisplayAttributes = hwc_getDisplayAttributes;
        dev->device.registerProcs = hwc_registerProcs;
        dev->device.query = hwc_query;
        dev->device.eventControl = hwc_event_control;

        *device = &dev->device.common;

        // create different composers.
#ifdef ENABLE_OVERLAY
        dev->overlayComposer = new HWOverlayComposer();
#endif
#ifdef ENABLE_WFD_OPTIMIZATION
        dev->virtualComposer = new HWVirtualComposer();
#endif
#ifdef ENABLE_HWC_GC_PATH
        dev->baseComposer = new HWBaselayComposer();
        int st = dev->baseComposer->open(name, NULL);
        if(st < 0)
        {
            ALOGW("Construct BaselayComposer failed, disable it.");
            delete dev->baseComposer;
            dev->baseComposer = NULL;
        }
        char value[PROPERTY_VALUE_MAX];
        property_get( "persist.hwc.skip", value, "0" );
        if(strcmp(value,"1") == 0 ) {
            dev->skip = true;
        } else {
            dev->skip = false;
        }
#endif

#if HWC_1_1
        if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &gralloc) == 0) {
            status = 0;
        } else {
            status = -EINVAL;
            ALOGE("load gralloc module failed");
        }

        /* defaul always open the buildin screen */
        int err = framebuffer_open(gralloc, &dev->fbdev[HWC_DISPLAY_PRIMARY]);

        if (err) {
            ALOGE("framebuffer_open failed (%s)", strerror(-err));
            return -err;
        }

        private_module_t * m = (private_module_t *) gralloc;
#ifdef ENABLE_WFD_OPTIMIZATION
        if(dev->virtualComposer)
            dev->virtualComposer->setSourceDisplayInfo(&m->info);
#endif
#ifdef ENABLE_OVERLAY
        if(dev->overlayComposer)
            dev->overlayComposer->setSourceDisplayInfo(&m->info);
#endif
#endif
    }
    return status;
}
