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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>
#include <utils/RefBase.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include <utils/Mutex.h>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include "gralloc_priv.h"
#include "HWVirtualComposer.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "HWVirtualComposer"
//#define LOG_NDEBUG 0


using namespace android;

HWVirtualComposer::HWVirtualComposer() : m_bRunning(false)
                                       , m_bClearBuffer(false)
                                       , m_pPrimaryFbLayer(NULL)
                                       , m_pGcuEngine(NULL)
                                       , m_pDefaultDisplayInfo(NULL)
                                       , m_previousDisplayMode(DISPLAY_CONTENT_UNKNOWN)
{
    m_pGcuEngine = new GcuEngine;
    m_pGcuEngine->LoadHintPic(0, "/etc/hint.bmp");
}

HWVirtualComposer::~HWVirtualComposer()
{
    if(NULL != m_pGcuEngine){
        delete m_pGcuEngine;
        m_pGcuEngine = NULL;
    }

    m_displays.clear();
}

bool HWVirtualComposer::readyToRun(size_t numDisplays, hwc_display_contents_1_t** displays)
{
    // check primary display device.
    hwc_display_contents_1_t* pPrimaryDisplayContents = displays[0];
    if(NULL == pPrimaryDisplayContents || 0 == pPrimaryDisplayContents->numHwLayers){
        ALOGV("WARNING: NULL hwc_display_contents_1_t* in primary display device!");
        return false;
    }

    //if no GEOMETRY_CHANGED, keep the previous status and skip the rest tests.
    if(!(pPrimaryDisplayContents->flags & HWC_GEOMETRY_CHANGED))
        return m_bRunning;

    // check display devices (if virtual device exists.)
    if(numDisplays <= HWC_DISPLAY_EXTERNAL){
        return false;
    }

    // check properties.
    char value[PROPERTY_VALUE_MAX];
    property_get("hwc.virtual.gcu.enable", value, "1");
    bool bEnable = (atoi(value) == 1);
    if(!bEnable){
        return false;
    }

    m_pPrimaryFbLayer = NULL;
    // check primary display contents.
    hwc_layer_1_t* pFbLayer = &(pPrimaryDisplayContents->hwLayers[pPrimaryDisplayContents->numHwLayers - 1]);
    if(NULL == pFbLayer
       || HWC_FRAMEBUFFER_TARGET != pFbLayer->compositionType){
        ALOGV("WARNING! primary display content is not valid for virtual composition!");
        return false;
    }

    // check virtual display device.
    hwc_display_contents_1_t* pVirtualDisplayContents = displays[HWC_NUM_DISPLAY_TYPES];
    if((NULL == pVirtualDisplayContents)
       || (pVirtualDisplayContents->numHwLayers <= 0)){
        ALOGV("WARNING! not a valid virtual display target!");
        return false;
    }

    // check virtual display contents.
    // if not mirror mode, or just few layers with rotation, this all-in-one blit may not helps.
    // TODO: we only check 1 virtual device, and may face multi-virtual devices case in future.
    hwc_layer_1_t* pVirtualFbLayer = &(pVirtualDisplayContents->hwLayers[pVirtualDisplayContents->numHwLayers - 1]);
    if((NULL == pVirtualFbLayer)
       || (DISPLAY_CONTENT_MIRROR != pVirtualFbLayer->reserved[1])
       || ((0 != pFbLayer->transform)
           && (6 > pPrimaryDisplayContents->numHwLayers))){
        ALOGV("WARNING! NOT a valid case: %d layers in %d blit mode with %d rotation.", pPrimaryDisplayContents->numHwLayers - 1, pVirtualFbLayer->reserved[1], pVirtualFbLayer->transform);
        m_previousDisplayMode = pVirtualFbLayer->reserved[1];
        return false;
    }
    if( m_previousDisplayMode == DISPLAY_CONTENT_UNKNOWN ) {
        m_previousDisplayMode = pVirtualFbLayer->reserved[1];
    } else if( m_previousDisplayMode != DISPLAY_CONTENT_MIRROR ) {
        m_previousDisplayMode = pVirtualFbLayer->reserved[1];
        m_bClearBuffer = true;
    }
    m_pPrimaryFbLayer = pFbLayer;
    return true;
}

uint32_t HWVirtualComposer::prepare(size_t numDisplays, hwc_display_contents_1_t** displays)
{
    ATRACE_CALL();

    ///< check all conditions before we do virtual composition.
    if(readyToRun(numDisplays, displays)){
        for (uint32_t i = HWC_NUM_DISPLAY_TYPES; i < numDisplays; ++i) {
            hwc_display_contents_1_t* list = displays[i];
            //bool bNeedCompose = (NULL != list) && (list->flags & HWC_GEOMETRY_CHANGED) && (list->numHwLayers > 0);
            bool bNeedCompose = (NULL != list) && (list->numHwLayers > 0);
            if(bNeedCompose){
                hwc_layer_1_t* destFbLayer = &(list->hwLayers[list->numHwLayers - 1]);
                if(NULL != destFbLayer){
                    // update display info
                    if(NAME_NOT_FOUND == m_displays.indexOfKey(i)){
                        ALOGD("New Dest FB layer[%d](%p) found!", i, destFbLayer);
                        sp<HwcDisplayData> data = new HwcDisplayData(destFbLayer);
                        m_displays.add(i, data);
                    }else if (!(m_displays.valueFor(i)->isEqual(destFbLayer))){
                        // STOP -> START, we should update the clear status.
                        m_displays.editValueFor(i)->from(destFbLayer, !m_bRunning || m_bClearBuffer);
                        m_bClearBuffer = false;
                    }

                    for(uint32_t j = 0; j < list->numHwLayers; ++j){
                        ///< clear all flags.
                        list->hwLayers[j].compositionType = HWC_2D_TARGET;
                        list->hwLayers[j].flags &= ~HWC_SKIP_LAYER;
                    }
                }
            }
        }

        m_bRunning = true;
        return HWC_DISPLAY_EXTERNAL + 1; //leave virtual display device along.
    }else{
        // START -> STOP, trigger HWC_GEOMETRY_CHANGED to notify other composer.
        if(m_bRunning){
            for (uint32_t i = HWC_DISPLAY_EXTERNAL + 1; i < numDisplays; ++i) {
                hwc_display_contents_1_t* list = displays[i];
                if(NULL != list)
                    list->flags |= HWC_GEOMETRY_CHANGED;
            }
        }
        m_bRunning = false;
        return numDisplays; // give all displays out.
    }
}

void HWVirtualComposer::set(size_t numDisplays, hwc_display_contents_1_t** displays)
{
    ATRACE_CALL();
    if( !m_bRunning
        || (numDisplays <= HWC_DISPLAY_EXTERNAL)
        || NULL == displays){
        return;
    }

    hwc_display_contents_1_t* pPrimaryDisplayContents = displays[0];
    if(NULL == pPrimaryDisplayContents || 0 == pPrimaryDisplayContents->numHwLayers){
        ALOGV("WARNING: NULL hwc_display_contents_1_t* in primary display device!");
        return;
    }
    m_pPrimaryFbLayer = &(pPrimaryDisplayContents->hwLayers[pPrimaryDisplayContents->numHwLayers - 1]);

    // Loop for all virtual displays.
    for (uint32_t i = HWC_NUM_DISPLAY_TYPES; i < numDisplays; ++i) {
        hwc_display_contents_1_t* list = displays[i];

        ///< do blit if a valid list with geometry changes in its existing layers.
        bool bNeedCompose = (NULL != list) && (list->numHwLayers > 0);
        if(bNeedCompose){
            hwc_layer_1_t* destFbLayer = &(list->hwLayers[list->numHwLayers - 1]);
            sp<HwcDisplayData>& displayData = m_displays.editValueFor(i);
            if((NULL == destFbLayer) ||
               (HWC_2D_TARGET != destFbLayer->compositionType) ||
               (displayData->m_pLayer != destFbLayer) ||
               (displayData->m_nDisplayMode != DISPLAY_CONTENT_MIRROR)){
                ALOGE("ERROR: Not a valid FRAMEBUFFER_TARGET for Virtual Composer to blit!");
                continue;
            }

            if(!blit(m_pPrimaryFbLayer, displayData)){
                ALOGE("ERROR! blit from primary FB to virtual FB failed!");
            }
        }
    }
}

void HWVirtualComposer::setSourceDisplayInfo(const fb_var_screeninfo* info){
    m_pDefaultDisplayInfo = info;
}

bool HWVirtualComposer::blit(hwc_layer_1_t* src, sp<HwcDisplayData>& displayData)
{
    const hwc_layer_1_t* dst = displayData->m_pLayer;
    ANativeWindow* pNativeWindow = (ANativeWindow*)dst->reserved[0];
    if(NULL == pNativeWindow){
        ALOGE("ERROR: Not a valid virtual framebuffer target!");
        return false;
    }

    ANativeWindowBuffer* pNativeBuffer = NULL;
    int32_t ret = pNativeWindow->dequeueBuffer_DEPRECATED(pNativeWindow, &pNativeBuffer);
    if(0 > ret || NULL == pNativeBuffer){
        ALOGE("ERROR: Can not get a valid buffer handle from virtual framebuffer target!");
        return false;
    }

    uint32_t width = m_pDefaultDisplayInfo->xres_virtual;
    uint32_t height = m_pDefaultDisplayInfo->yres_virtual >> 1; // now we assume there are only 2 buffers in LCD.
    uint32_t orientation = resolveDisplayOrientation(dst->transform);
    BlitDataDescription blitDesc;
    DISP_RECT srcRect;
    DISP_RECT dstRect;
    buffer_handle_t srcBufferHandle = src->handle;
    private_handle_t* pSrcPrivHandle = private_handle_t::dynamicCast(srcBufferHandle);
    buffer_handle_t dstBufferHandle = pNativeBuffer->handle;
    private_handle_t* pDstPrivHandle = private_handle_t::dynamicCast(dstBufferHandle);

    bool bIsSecureContents = false;
    static bool preSecureState = false;
    if(preSecureState != bIsSecureContents){
        displayData->m_vBuffer.clear();
    }

    if(NULL == pSrcPrivHandle || NULL == pDstPrivHandle){
        ALOGE("ERROR! pSrcPrivHandle = %p, pDstPrivHandle = %p", pSrcPrivHandle, pDstPrivHandle);
        goto ERROR_OUT;
    }

    srcRect.l              = src->sourceCrop.left;
    srcRect.t              = src->sourceCrop.top;
    srcRect.r              = src->sourceCrop.right;
    srcRect.b              = src->sourceCrop.bottom;

    if(!displayData->isBufferCleared(pNativeBuffer)){
        dstRect.l              = 0;
        dstRect.t              = 0;
        dstRect.r              = pDstPrivHandle->width;
        dstRect.b              = pDstPrivHandle->height;
        ConstructBlitDataDescription(blitDesc, GPU_BLIT_FILL, true, DISPLAY_SURFACE_ROTATION_0,
                                     width, height,
                                     pSrcPrivHandle->format, &srcRect,
                                     pSrcPrivHandle->physAddr, 0, 0,
                                     width*4, 0, 0,
                                     pDstPrivHandle->width, pDstPrivHandle->height, pDstPrivHandle->format,
                                     &dstRect, &dstRect, 1,
                                     pDstPrivHandle->physAddr, pDstPrivHandle->mem_xstride,
                                     0xFF000000, 0, 0, NULL, true, 0x0);

        if(!m_pGcuEngine->Blit(&blitDesc)){
            ALOGE("ERROR: GCU 2D Fill Blit Error!");
            goto ERROR_OUT;
        }

        displayData->addClearedBuffer(pNativeBuffer);
    }

    dstRect.l              = dst->displayFrame.left;
    dstRect.t              = dst->displayFrame.top;
    dstRect.r              = dst->displayFrame.right;
    dstRect.b              = dst->displayFrame.bottom;

    ConstructBlitDataDescription(blitDesc, GPU_BLIT_FILTER, true, orientation,
                                 width, height,
                                 pSrcPrivHandle->format, &srcRect,
                                 pSrcPrivHandle->physAddr, 0, 0,
                                 width*4, 0, 0,
                                 pDstPrivHandle->width, pDstPrivHandle->height, pDstPrivHandle->format,
                                 &dstRect, &dstRect, 1,
                                 pDstPrivHandle->physAddr, pDstPrivHandle->mem_xstride,
                                 0, 0, 0, NULL, true, 0x0);

    if(!m_pGcuEngine->Blit(&blitDesc)){
        ALOGE("ERROR: GCU 2D Filter Blit Error!");
        goto ERROR_OUT;
    }

    /*
    dumpOneFrame((void*)pDstPrivHandle->base, pDstPrivHandle->width,
                 pDstPrivHandle->height, pDstPrivHandle->format);
    */

ERROR_OUT:
    //queue back to WFD AVstreaming pipeline.
    ret = pNativeWindow->queueBuffer_DEPRECATED(pNativeWindow, pNativeBuffer);
    if(0 > ret){
        ALOGE("ERROR: Queue buffer failed!");
    }

    return (ret >= 0);
}

DISPLAY_SURFACE_ROTATION HWVirtualComposer::resolveDisplayOrientation(uint32_t orientation)
{
    switch (orientation){
        case DISPLAY_SURFACE_ROTATION_0:
            return DISPLAY_SURFACE_ROTATION_0;
        case DISPLAY_SURFACE_ROTATION_90:
            return DISPLAY_SURFACE_ROTATION_270;
        case DISPLAY_SURFACE_ROTATION_180:
            return DISPLAY_SURFACE_ROTATION_180;
        case DISPLAY_SURFACE_ROTATION_270:
            return DISPLAY_SURFACE_ROTATION_90;
        default:
            ALOGE("ERROR: Wrong rotation found, use default 0 degree for dest surface!");
            return DISPLAY_SURFACE_ROTATION_0;
    }
}

void HWVirtualComposer::dumpOneFrame(void* frame, uint32_t w, uint32_t h, uint32_t format)
{
    FILE* fp = fopen("/data/dump.rgb", "a+b");
    if(NULL != fp){
        fwrite(frame, 1, w*h*4, fp);
        fclose(fp);
    }
}
