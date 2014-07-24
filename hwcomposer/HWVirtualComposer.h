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

#ifndef __HW_VIRTUAL_COMPOSER_H__
#define __HW_VIRTUAL_COMPOSER_H__

#include <linux/fb.h>
#include <utils/RefBase.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include "GcuEngine.h"

namespace android{

/**
 * Refer to WindowManagerService.java.
 *    private static final int DISPLAY_CONTENT_UNKNOWN = 0;
 *    private static final int DISPLAY_CONTENT_MIRROR = 1;
 *    private static final int DISPLAY_CONTENT_UNIQUE = 2;
 */
enum DISPLAY_CONTENT_MODE{
    DISPLAY_CONTENT_UNKNOWN = 0,
    DISPLAY_CONTENT_MIRROR = 1,
    DISPLAY_CONTENT_UNIQUE = 2,
};


class HWVirtualComposer
{
public:
    HWVirtualComposer();

    ~HWVirtualComposer();

public:
    /*prepare
     *
     */
    uint32_t prepare(size_t numDisplays, hwc_display_contents_1_t** displays);

    /*set
     *
     */
    void set(size_t numDisplays, hwc_display_contents_1_t** displays);

    /*dump
     *
     */
    void dump(String8& result, char* buffer, int size, bool dumpManager = true){}

    /*set FB info.
     */
    void setSourceDisplayInfo(const fb_var_screeninfo* info);

    /*get running status.
     */
    bool isRunning(){
        return m_bRunning;
    }

private:
    class HwcDisplayData : public RefBase
    {
    public:
        HwcDisplayData(const hwc_layer_1_t* layer) : m_pLayer(layer)
                                                   , m_nDisplayMode(DISPLAY_CONTENT_UNKNOWN)
                                                   , m_hFbHandle(NULL)
                                                   , m_nTransform(-1)
        {
            if(NULL != m_pLayer) {
                // save important info. Because m_player is just a saved pointer value.
                // its contents may changed outside, we need save all relative current info now.
                m_nDisplayMode = (DISPLAY_CONTENT_MODE)m_pLayer->reserved[1];
                m_nTransform = m_pLayer->transform;
                m_hFbHandle = m_pLayer->handle;
            }
        }

        ~HwcDisplayData()
        {
            m_pLayer = NULL;
            m_vBuffer.clear();
        }

    public:
        bool isBufferCleared(ANativeWindowBuffer* buffer){
            return m_vBuffer.indexOf(buffer) != NAME_NOT_FOUND;
        }

        void addClearedBuffer(ANativeWindowBuffer* buffer){
            m_vBuffer.add(buffer);
        }

        bool isEqual(const hwc_layer_1_t* layer) const
        {
            if(NULL != layer && NULL != m_pLayer)
                return m_pLayer == layer;
            else
                return false;
        }

        void from(const hwc_layer_1_t* layer, bool bResetClearStatus)
        {
            if (bResetClearStatus
                || (m_hFbHandle != layer->handle)
                || (m_nTransform != layer->transform) ){
                ///< params change, reset all saved cleared buffers.
                m_vBuffer.clear();
            }

            m_pLayer = layer;
            m_nTransform = m_pLayer->transform;
            m_hFbHandle = m_pLayer->handle;
        }

    public:
        ///< current pointer to fb dest.
        const hwc_layer_1_t* m_pLayer;

        ///< current display mode of fb dest.
        DISPLAY_CONTENT_MODE m_nDisplayMode;

        ///< current fbTargetHandle of fb dest.
        buffer_handle_t m_hFbHandle;

        ///< current transform of fb dest.
        uint32_t m_nTransform;

        ///< cleared buffer list of fb dest.
        SortedVector<ANativeWindowBuffer*> m_vBuffer;
    };

private:

    bool readyToRun(size_t numDisplays, hwc_display_contents_1_t** displays);

    bool blit(hwc_layer_1_t* src, sp<HwcDisplayData>& displayData);

    DISPLAY_SURFACE_ROTATION resolveDisplayOrientation(uint32_t orientation);

    void dumpOneFrame(void* frame, uint32_t w, uint32_t h, uint32_t format);

private:
    bool m_bRunning;
    bool m_bClearBuffer;

    hwc_layer_1_t* m_pPrimaryFbLayer;

    GcuEngine*     m_pGcuEngine;

    const fb_var_screeninfo* m_pDefaultDisplayInfo;

    DefaultKeyedVector<uint32_t, sp<HwcDisplayData> > m_displays;

    int m_previousDisplayMode;
};

}

#endif

