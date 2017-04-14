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

#include <ui/Region.h>
#include <utils/Log.h>
#include <system/graphics.h>
#include <cutils/properties.h>
#include "HWOverlayComposer.h"
#include "FramebufferEngine.h"
#include "gralloc_priv.h"

#define DEBUG 0

using namespace android;

HWOverlayComposer::HWOverlayComposer() : m_nOverlayChannel(HWC_DISPLAY_PRIMARY+1)
                                       , m_bRunning(false)
                                       , m_bDeferredClose(true)
                                       , m_pDefaultDisplayInfo(NULL)
                                       , m_pPrimaryFbLayer(NULL)
                                       , m_pGcuEngine(NULL)
                                       , m_bDebugClear(false)
{
    for(uint32_t i = 0; i < m_nOverlayChannel; ++i){
        m_vDisplayData.add(new DisplayData(i));
    }

    m_pBaseDisplayEngine = new FBBaseLayer("/dev/graphics/fb0");
    if(m_pBaseDisplayEngine == NULL || (m_pBaseDisplayEngine->open() < 0)){
        LOGE("ERROR: Open Base Layer Failed.");
    }

    m_pGcuEngine = new GcuEngine;
}

HWOverlayComposer::~HWOverlayComposer()
{
    for(uint32_t i = 0; i < m_nOverlayChannel; ++i){
        m_vDisplayData.editItemAt(i).clear();
    }

    if(m_pBaseDisplayEngine != NULL){
        m_pBaseDisplayEngine->close();
    }

    if(NULL != m_pGcuEngine){
        delete m_pGcuEngine;
    }
}

bool HWOverlayComposer::isOverlayCandidate(hwc_layer_1_t* layer)
{
    // a qualified overlay layer should be a normal visiable layer with
    // a hardware supported YUV/RGB format, a reasonable on-screen size.
    // Also, its memory should be physically continuously, and it is a
    // layer without rotation.
    // TODO: we may add more restrict conditions here.
    if( layer->flags & HWC_SKIP_LAYER )
        return false;

    if( layer->flags & HWC_OVERLAY_SKIP_LAYER )
        return false;

    if( !isPhyConts(layer) )
        return false;

    if( layer->blending != HWC_BLENDING_NONE )
        return false;

    if( layer->transform != 0 )
        return false;

    if( !isYuv(getPixelFormat(layer)))
        return false;

    int32_t screenWidth = m_pDefaultDisplayInfo->xres;
    int32_t screenHeight = m_pDefaultDisplayInfo->yres;
    hwc_rect_t& displayFrame = layer->displayFrame;
    int32_t videoWidth = displayFrame.right - displayFrame.left;
    int32_t videoHeight = displayFrame.bottom - displayFrame.top;

    if((displayFrame.left < 0) || (displayFrame.top < 0)
       || (videoWidth < 0) || (videoWidth > screenWidth)
       || (videoHeight < 0) || (videoHeight > screenHeight)){
        return false;
    }

    float ratioWidth = (float)videoWidth / screenWidth;
    float ratioHeight = (float)videoHeight / screenHeight;
    if((ratioWidth > 2.0f) || (ratioWidth < 0.5f)
       || (ratioHeight > 2.0f) || (ratioHeight < 0.5f)){
        return false;
    }

    return true;
}

bool HWOverlayComposer::readyToRun(size_t numDisplays, hwc_display_contents_1_t** displays)
{
    // check primary display device.
    hwc_display_contents_1_t* pPrimaryDisplayContents = displays[0];
    if(NULL == pPrimaryDisplayContents || 0 == pPrimaryDisplayContents->numHwLayers){
        ALOGE("ERROR: NULL hwc_display_contents_1_t* in primary display device!");
        return false;
    }

    //if no GEOMETRY_CHANGED, keep the previous status and skip the rest tests.
    if(!(pPrimaryDisplayContents->flags & HWC_GEOMETRY_CHANGED)){
        return m_bRunning;
    }

    //if virtual device exists, we skip overlay.
    if(0 >= numDisplays
       || ((HWC_DISPLAY_EXTERNAL + 1 < numDisplays) && (NULL != displays[numDisplays-1]))){
        return false;
    }

    hwc_layer_1_t* pFbLayer = &(pPrimaryDisplayContents->hwLayers[pPrimaryDisplayContents->numHwLayers - 1]);
    if(NULL == pFbLayer
       || HWC_FRAMEBUFFER_TARGET != pFbLayer->compositionType){
        LOGE("ERROR! primary display content is not valid for virtual composition!");
        return false;
    }

    // check properties.
    char value[PROPERTY_VALUE_MAX];
    property_get("hwc.overlay.enable", value, "0");
    bool bEnable = (atoi(value) == 1);
    if(!bEnable){
        return false;
    }

    return true;
}

bool HWOverlayComposer::traverse(uint32_t nType, hwc_display_contents_1_t* layers)
{
    Mutex::Autolock lock(mLock);
    SortedOverlayVector vSortedOverlay;
    Region overlayRegion;
    Region normalRegion;
    sp<DisplayData>& pDisplayData = m_vDisplayData.editItemAt(nType);
    DrawingOverlayVector& vCurrentOverlay = pDisplayData->m_vCurrentOverlay;
    Rect& currentOverlayRect = pDisplayData->m_currentOverlayRect;
    uint32_t nOverlayDevices = pDisplayData->m_nOverlayDevices;

    // find potential overlay layers.
    for( size_t i = 0; i < layers->numHwLayers; ++i ) {
        hwc_layer_1_t *tmp = &(layers->hwLayers[i]);
        if((NULL == tmp)
           || (HWC_FRAMEBUFFER_TARGET == tmp->compositionType)){
            continue;
        }

        if(isOverlayCandidate(tmp)){
            vSortedOverlay.add(tmp);
        }else{
            //normalRegion.add(rect);
            hwc_rect_t& displayFrame = tmp->displayFrame;
            Rect rect(displayFrame.left, displayFrame.top, displayFrame.right, displayFrame.bottom);
            normalRegion.orSelf(rect);
        }
    }

    if(!vSortedOverlay.isEmpty()){
        if(vSortedOverlay.size() > 1){
            //TODO: check overlap between different overlay layer.
        }

        // figure out the overlay region and the on-screen region.
        // then, check if we can go through the PARTIAL_DISPLAY mode.
        for(size_t i = 0; i < vSortedOverlay.size(); ++i){
            hwc_layer_1_t* layer = vSortedOverlay[i];
            hwc_rect_t& displayFrame = layer->displayFrame;
            Rect rect(displayFrame.left, displayFrame.top, displayFrame.right, displayFrame.bottom);
            if(CC_LIKELY(i < nOverlayDevices)){
                vCurrentOverlay.add(layer);
                overlayRegion.orSelf(rect);
            }else{
                normalRegion.orSelf(rect);
            }
        }

        // if no overlap between on-screen layer & overlay layer,
        // or partially overlapped, we can use overlay.
        Region regionOverlap(normalRegion & overlayRegion);
        if(regionOverlap.isEmpty()){
            currentOverlayRect = overlayRegion.getBounds();
            return true;
        }
    }

    // no overlay found, clear members.
    currentOverlayRect.clear();
    vCurrentOverlay.clear();
    return false;
}

void HWOverlayComposer::allocateOverlay(uint32_t nType)
{
    sp<DisplayData>& pDisplayData = m_vDisplayData.editItemAt(nType);
    DrawingOverlayVector& vCurrentOverlay = pDisplayData->m_vCurrentOverlay;
    sp<OverlayDevice>& pOverlayDevice = pDisplayData->m_pOverlayDevice;

    // currently we have only 1 overlay devices, so, we assume this currentOverlay.size() == 1.
    hwc_layer_1_t*& layer = vCurrentOverlay.editItemAt(0);
    if(pOverlayDevice != NULL){
        if(!pOverlayDevice->isOpen()){
            pOverlayDevice->open();
        }

        if(pOverlayDevice->readyDrawOverlay()){
            // the first several frames might be covered over by base layer
            // so, we let the first several frames go to both overlay and base layer.
            layer->compositionType = HWC_OVERLAY;
        }
    }else{
        ALOGE("ERROR: Overlay device is not initialized, please check!");
    }

}

void HWOverlayComposer::prepare(size_t numDisplays, hwc_display_contents_1_t** displays)
{
    static bool prevStatus = false;
    if(readyToRun(numDisplays, displays)){
        // check LCD & HDMI device which may enable overlay path.
        bool bRunning = false;
        for(uint32_t nType = 0; nType < m_nOverlayChannel; ++nType){
            hwc_display_contents_1_t* layers = displays[nType];
            if(NULL != layers && traverse(nType, layers)){
                allocateOverlay(nType);
                bRunning |= true;
            }else{
                bRunning |= false;
            }

            // at least 1 path qualified.
            m_bRunning = bRunning;
        }
    }else{
        m_bRunning = false;
    }

#if 0
    if(prevStatus != m_bRunning){
        prevStatus = m_bRunning;
        // if the first overlay frame comes
        // or the last overlay frame goes away,
        // we should force a vsync happen, to
        // avoid a un-displayed frame drop.
        m_pBaseDisplayEngine->waitVSync(DISPLAY_SYNC_SELF);
    }
#endif
}

void HWOverlayComposer::set(size_t numDisplays, hwc_display_contents_1_t** displays){
    Mutex::Autolock lock(mLock);

    hwc_display_contents_1_t* pPrimaryDisplayContents = displays[0];
    if(NULL == pPrimaryDisplayContents || 0 == pPrimaryDisplayContents->numHwLayers){
        LOGE("ERROR: NULL hwc_display_contents_1_t* in primary display device!");
        m_pPrimaryFbLayer = NULL;
    }else{
        m_pPrimaryFbLayer = &(pPrimaryDisplayContents->hwLayers[pPrimaryDisplayContents->numHwLayers - 1]);
    }

    // set overlay in each path.
    for(uint32_t nType = 0; nType < m_nOverlayChannel; ++nType){
        sp<DisplayData>& pDisplayData = m_vDisplayData.editItemAt(nType);
        DrawingOverlayVector& vCurrentOverlay = pDisplayData->m_vCurrentOverlay;
        DrawingOverlayVector& vDrawingOverlay = pDisplayData->m_vDrawingOverlay;
        Rect& currentOverlayRect = pDisplayData->m_currentOverlayRect;
        Rect& drawingOverlayRect = pDisplayData->m_drawingOverlayRect;
        sp<OverlayDevice>& pOverlayDevice = pDisplayData->m_pOverlayDevice;

        if(vCurrentOverlay.isEmpty()){
            if(pOverlayDevice->isOpen()){
                if(!m_bDeferredClose){
                    pOverlayDevice->close();
                }
            }
        }else{
            hwc_layer_1_t*& layer = vCurrentOverlay.editItemAt(0);
            pOverlayDevice->setOverlayAlphaMode(DISP_OVLY_GLOBAL_ALPHA, 0xFF,
                                                DISP_OVLY_COLORKEY_DISABLE, 0x0);
            pOverlayDevice->commit(layer);
        }

        // set partial display region if overlay status changes.
        if (drawingOverlayRect != currentOverlayRect){
            setOverlayRegion(currentOverlayRect);
        }
    }
}

void HWOverlayComposer::colorFillLayer(hwc_layer_1_t* pLayer, const Rect& rect, uint32_t nColor)
{
    if(pLayer == NULL)
        return;

    BlitDataDescription blitDesc;
    uint32_t width = pLayer->sourceCrop.right - pLayer->sourceCrop.left;
    uint32_t height = pLayer->sourceCrop.bottom - pLayer->sourceCrop.top;
    buffer_handle_t srcBufferHandle = pLayer->handle;
    if(NULL == srcBufferHandle){
        return;
    }

    private_handle_t* pSrcPrivHandle = private_handle_t::dynamicCast(srcBufferHandle);
    buffer_handle_t dstBufferHandle = srcBufferHandle;
    private_handle_t* pDstPrivHandle = private_handle_t::dynamicCast(dstBufferHandle);
    DISP_RECT dstRect;

    dstRect.l = rect.left;
    dstRect.r = rect.right;
    dstRect.t = rect.top;
    dstRect.b = rect.bottom;

    ConstructBlitDataDescription(blitDesc, GPU_BLIT_FILL, true, DISPLAY_SURFACE_ROTATION_0,
                                 width, height,
                                 HAL_PIXEL_FORMAT_RGB_565, &dstRect,
                                 pSrcPrivHandle->physAddr, 0, 0,
                                 width*4, 0, 0,
                                 width, height, HAL_PIXEL_FORMAT_RGB_565,
                                 &dstRect, &dstRect, 1,
                                 pDstPrivHandle->physAddr, width*4,
                                 nColor, 0, 0, NULL, true, 0);

    if(!m_pGcuEngine->Blit(&blitDesc)){
        LOGE("ERROR: GCU 2D Fill Blit Error!");
    }
}

void HWOverlayComposer::colorFillFrameBuffer(uint32_t nColor)
{
    if(m_pPrimaryFbLayer == NULL)
        return;

    BlitDataDescription blitDesc;
    uint32_t width = m_pDefaultDisplayInfo->xres_virtual;
    uint32_t nBufferCnt = m_pDefaultDisplayInfo->yres_virtual / m_pDefaultDisplayInfo->yres + 1;
    uint32_t height = m_pDefaultDisplayInfo->yres_virtual / nBufferCnt;
    buffer_handle_t srcBufferHandle = m_pPrimaryFbLayer->handle;
    if(NULL == srcBufferHandle){
        return;
    }

    private_handle_t* pSrcPrivHandle = private_handle_t::dynamicCast(srcBufferHandle);
    buffer_handle_t dstBufferHandle = srcBufferHandle;
    private_handle_t* pDstPrivHandle = private_handle_t::dynamicCast(dstBufferHandle);
    DISP_RECT srcRect;
    DISP_RECT dstRect;

    srcRect.l = 0;
    srcRect.r = width;
    srcRect.t = 0;
    srcRect.b = height;

    ConstructBlitDataDescription(blitDesc, GPU_BLIT_FILL, true, DISPLAY_SURFACE_ROTATION_0,
                                 width, height,
                                 pSrcPrivHandle->format, &srcRect,
                                 pSrcPrivHandle->physAddr, 0, 0,
                                 width*4, 0, 0,
                                 width, height, pDstPrivHandle->format,
                                 &srcRect, &srcRect, 1,
                                 pDstPrivHandle->physAddr, width*4,
                                 nColor, 0, 0, NULL, true, 0);

    if(!m_pGcuEngine->Blit(&blitDesc)){
        LOGE("ERROR: GCU 2D Fill Blit Error!");
    }
}

void HWOverlayComposer::finishCompose()
{
    Mutex::Autolock lock(mLock);
    for(uint32_t nType = 0; nType < m_nOverlayChannel; ++nType){
        sp<DisplayData>& pDisplayData = m_vDisplayData.editItemAt(nType);
        DrawingOverlayVector& vCurrentOverlay = pDisplayData->m_vCurrentOverlay;
        DrawingOverlayVector& vDrawingOverlay = pDisplayData->m_vDrawingOverlay;
        Rect& currentOverlayRect = pDisplayData->m_currentOverlayRect;
        Rect& drawingOverlayRect = pDisplayData->m_drawingOverlayRect;
        sp<OverlayDevice>& pOverlayDevice = pDisplayData->m_pOverlayDevice;

        pOverlayDevice->onCommitFinished();
        if(vCurrentOverlay.isEmpty()){
            if(m_bDeferredClose){
                if(pOverlayDevice->isOpen()){
                    pOverlayDevice->close();
                }
            }
        }

        vDrawingOverlay = vCurrentOverlay;
        drawingOverlayRect = currentOverlayRect;
        // overlay blit finishs, clear members.
        currentOverlayRect.clear();
        vCurrentOverlay.clear();
    }
}

void HWOverlayComposer::transparentizeFrameBuffer(uint32_t nAlpha)
{
    static uint32_t savedAlpha = 0xFFFFFFFF;
    if(m_pPrimaryFbLayer == NULL)
        return;

    BlitDataDescription blitDesc;
    uint32_t width = m_pDefaultDisplayInfo->xres_virtual;
    uint32_t nBufferCnt = m_pDefaultDisplayInfo->yres_virtual / m_pDefaultDisplayInfo->yres + 1;
    uint32_t height = m_pDefaultDisplayInfo->yres_virtual / nBufferCnt;

    buffer_handle_t srcBufferHandle = m_pPrimaryFbLayer->handle;
    if(NULL == srcBufferHandle){
        return;
    }

    private_handle_t* pSrcPrivHandle = private_handle_t::dynamicCast(srcBufferHandle);
    buffer_handle_t dstBufferHandle = srcBufferHandle;
    private_handle_t* pDstPrivHandle = private_handle_t::dynamicCast(dstBufferHandle);
    uint32_t nRop = (nAlpha == 0) ? 0x88 : 0xEE;

    savedAlpha = nAlpha;
    ConstructBlitDataDescription(blitDesc, GPU_BLIT_ROP, true, DISPLAY_SURFACE_ROTATION_0,
                                 width, height,
                                 pSrcPrivHandle->format, NULL,
                                 pSrcPrivHandle->physAddr, 0, 0,
                                 width*4, 0, 0,
                                 width, height, pDstPrivHandle->format,
                                 NULL, NULL, 1,
                                 pDstPrivHandle->physAddr, width*4,
                                 0xFF000000, 0, 0, NULL, true, nRop);

    if(!m_pGcuEngine->Blit(&blitDesc)){
        LOGE("ERROR: GCU 2D Fill Blit Error!");
    }
}

void HWOverlayComposer::setOverlayRegion(const Rect& rect)
{
    if (m_pBaseDisplayEngine->setPartialDisplayRegion(rect.left, rect.right, rect.top, rect.bottom, 0) < 0){
        ALOGE("ERROR: Fail to set partial display!");
    }
}

bool HWOverlayComposer::isYuv(uint32_t format) {
    return ((format >= HAL_PIXEL_FORMAT_YCbCr_422_SP)
             && (format <= HAL_PIXEL_FORMAT_YCbCr_420_SP_MRVL))
            || (HAL_PIXEL_FORMAT_YV12 == format);
}

bool HWOverlayComposer::isScale(hwc_layer_1_t * layer) {
    int dw = layer->displayFrame.right - layer->displayFrame.left;
    int dh = layer->displayFrame.bottom - layer->displayFrame.top;
    private_handle_t *ph = private_handle_t::dynamicCast( layer->handle );
    assert( ph != NULL );
    return dw != ph->width || dh != ph->height;
}

uint32_t HWOverlayComposer::getPixelFormat(const hwc_layer_1_t * layer) {
    private_handle_t *ph = private_handle_t::dynamicCast( layer->handle );
    assert( ph != NULL );
    return ph->format;
}

bool HWOverlayComposer::isPhyConts(hwc_layer_1_t * layer) {
    if(NULL == layer->handle){
        return false;
    }

    private_handle_t *ph = private_handle_t::dynamicCast( layer->handle );
    assert( ph != NULL );
    return (ph->flags & private_handle_t::PRIV_FLAGS_USES_PMEM);
}

// check if rgb layer qualified to go overLay
bool HWOverlayComposer::isQualifiedRGBLayer(hwc_layer_1_t *layer)
{
    return (!isScale(layer) && // no scaling
           (layer->transform == 0x00000000) && // only rotation 0 supported
           (layer->visibleRegionScreen.numRects == 1));// only one visible region
}

void HWOverlayComposer::dump(String8& result, char* buffer, int size, bool dumpManager)
{
    Mutex::Autolock lock(mLock);

    result.append("--------------- HWC Overlay Composer Info ---------------\n");
    for(uint32_t nType = 0; nType < m_nOverlayChannel; ++nType){
        sp<DisplayData>& pDisplayData = m_vDisplayData.editItemAt(nType);
        DrawingOverlayVector& vCurrentOverlay = pDisplayData->m_vCurrentOverlay;
        DrawingOverlayVector& vDrawingOverlay = pDisplayData->m_vDrawingOverlay;
        Rect& currentOverlayRect = pDisplayData->m_currentOverlayRect;
        Rect& drawingOverlayRect = pDisplayData->m_drawingOverlayRect;
        sp<OverlayDevice>& pOverlayDevice = pDisplayData->m_pOverlayDevice;

        sprintf(buffer, "%s Overlay Compositor Info\n", (HWC_DISPLAY_PRIMARY == nType) ? "LCD" : "HDMI");
        result.append(buffer);

        sprintf(buffer, "    [Current Overlay Count] : [%d].\n", vCurrentOverlay.size());
        result.append(buffer);

        for(uint32_t i = 0; i < vCurrentOverlay.size(); ++i){
            sprintf(buffer, "        [%d] Overlay Layer: [%p]\n", i, vCurrentOverlay[i]);
            result.append(buffer);
            sprintf(buffer, "        [%d] Overlay Handle: [%p]\n", i, vCurrentOverlay[i]->handle);
            result.append(buffer);
            hwc_rect_t& displayFrame = vCurrentOverlay[i]->displayFrame;
            sprintf(buffer, "        [%d] Overlay Rect : [%d %d %d %d]\n",
                    i, displayFrame.left, displayFrame.top, displayFrame.right, displayFrame.bottom);
            result.append(buffer);
        }

        sprintf(buffer, "    [Drawing Overlay Count] : [%d]\n", vDrawingOverlay.size());
        result.append(buffer);
        for(uint32_t i = 0; i < vDrawingOverlay.size(); ++i){
            sprintf(buffer, "        [%d] Overlay Layer: [%p]\n", i, vDrawingOverlay[i]);
            result.append(buffer);
            sprintf(buffer, "        [%d] Overlay Handle: [%p]\n", i, vCurrentOverlay[i]->handle);
            result.append(buffer);
            hwc_rect_t& displayFrame = vDrawingOverlay[i]->displayFrame;
            sprintf(buffer, "        [%d] Overlay Rect : [%d %d %d %d]\n",
                    i, displayFrame.left, displayFrame.top, displayFrame.right, displayFrame.bottom);
            result.append(buffer);
        }

        pOverlayDevice->dump(result, buffer, size);
    }
    return;
}
