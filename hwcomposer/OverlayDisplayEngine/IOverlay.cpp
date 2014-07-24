/*
 * (C) Copyright 2010 Marvell Int32_Ternational Ltd.
 * All Rights Reserved
 *
 * MARVELL CONFIDENTIAL
 * Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Marvell Int32_Ternational Ltd or its
 * suppliers or licensors. Title to the Material remains with Marvell Int32_Ternational Ltd
 * or its suppliers and licensors. The Material contains trade secrets and
 * proprietary and confidential information of Marvell or its suppliers and
 * licensors. The Material is protected by worldwide copyright and trade secret
 * laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Marvell's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other int32_tellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such int32_tellectual property rights must be
 * express and approved by Marvell in writing.
 *
 */

#include "IDisplayEngine.h"
#include "FramebufferOverlay.h"
#include "V4L2Overlay.h"
#include "FakeOverlay.h"

namespace android{

enum OVERLAYDEVICETYPE{
    OVERLAY_DEVICE_UNKNOWN = 0,
    OVERLAY_DEVICE_FB      = 1,
    OVERLAY_DEVICE_V4L2    = 2,
    OVERLAY_DEVICE_FAKE    = 3,
};

#if 0
IDisplayEngine* CreateOverlayEngine(uint32_t idx, DMSInternalConfig const * dms_config)
{
    static char const * const *fb_overlay_device_template = dms_config->OVLY.fb_dev_name;
    static char const * const *v4l2_overlay_device_template = dms_config->OVLY.v4l2_dev_name;

    //  static uint32_t idx = 0;
    static OVERLAYDEVICETYPE tOvlyType = OVERLAY_DEVICE_UNKNOWN;

    if(OVERLAY_DEVICE_UNKNOWN == tOvlyType){
        int32_t fd = open(fb_overlay_device_template[idx], O_RDWR, 0);
        if(fd > 0){
            tOvlyType = OVERLAY_DEVICE_FB;
            close(fd);
        }else{
            tOvlyType = OVERLAY_DEVICE_V4L2;
        }
    }

    if(NULL == v4l2_overlay_device_template[idx]){
        tOvlyType = OVERLAY_DEVICE_FAKE;
    }

    IDisplayEngine* pOverlay = NULL;
    if(OVERLAY_DEVICE_V4L2 == tOvlyType){
        LOGD("create v4l2 overlay! ---------- idx == %d ----------", idx);
        pOverlay = new V4L2OverlayRef(v4l2_overlay_device_template[idx]);
    }else if (OVERLAY_DEVICE_FB == tOvlyType){
        LOGD("create fb overlay! ---------- idx == %d ----------", idx);
        pOverlay = new FBOverlayRef(fb_overlay_device_template[idx]);
    }else if (OVERLAY_DEVICE_FAKE == tOvlyType){
        LOGD("create fake overlay! ---------- idx == %d ----------", idx);
        pOverlay = new FakeOverlayRef(NULL);
    }

    return pOverlay;
}
#endif

}
