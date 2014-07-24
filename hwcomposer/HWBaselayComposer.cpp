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
#include <errno.h>
#include "HWBaselayComposer.h"
#ifdef ENABLE_HWC_GC_PATH
    #include "HWCGCInterface.h"
#endif
#include <cutils/properties.h>
#include <cutils/log.h>

using namespace android;

//#define HWC_COMPILE_CHECK(expn) typedef char __HWC_ASSERT__[(expn)?1:-1]

// Here we need to check hwc_layer_t and hwc_layer_list size as libHWComposerGC.so is released in binary.
// If you meet compile error here, ask graphics team for a new libHWComposerGC.so. and update value here.
//HWC_COMPILE_CHECK(sizeof(hwc_layer_1_t) == 100);
//HWC_COMPILE_CHECK(sizeof(hwc_display_contents_1_t) == 20);

HWBaselayComposer::HWBaselayComposer()
:mHwc(0)
{

}

HWBaselayComposer::~HWBaselayComposer()
{

}

int HWBaselayComposer::open(const char * name, struct hw_device_t** device)
{
#ifdef ENABLE_HWC_GC_PATH
    char value[PROPERTY_VALUE_MAX];
    property_get( "persist.hwc.gc.disable", value, "0" );
    if( memcmp(value, "1", 1) == 0 )
    {
        return -EINVAL;
    }
    else
    {
        return hwc_device_open_gc(NULL, name, (struct hw_device_t**)&mHwc);
    }
#else
    return -EINVAL;
#endif
}

int HWBaselayComposer::close(struct hw_device_t* dev)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_device_close_gc(&mHwc->common);
#else
    return 0;
#endif
}

int HWBaselayComposer::prepare(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_prepare_gc(mHwc, numDisplays, displays);
#else
    return 0;
#endif
}

int HWBaselayComposer::set(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_set_gc(mHwc, numDisplays, displays);
#else
    return 0;
#endif
}

int HWBaselayComposer::blank(hwc_composer_device_1_t *dev, int disp, int blk)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_blank_gc(mHwc, disp, blk);
#else
    return 0;
#endif
}
