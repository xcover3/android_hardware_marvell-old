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
#ifndef __HWC_GC_INTERFACE_H__
#define __HWC_GC_INTERFACE_H__
#include <stdlib.h>
#include <hardware/hwcomposer.h>

#ifdef INTEGRATED_WITH_MARVELL
    #define HWC_RENAME(x) x##_gc
    #define hwc_set             HWC_RENAME(hwc_set)
    #define hwc_prepare         HWC_RENAME(hwc_prepare)
    #define hwc_eventControl    HWC_RENAME(hwc_eventControl)
    #define hwc_blank           HWC_RENAME(hwc_blank)
    #define hwc_device_open     HWC_RENAME(hwc_device_open)
    #define hwc_device_close    HWC_RENAME(hwc_device_close)
#endif

#ifdef __cplusplus
extern "C" {
#endif

int
hwc_prepare(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t** displays
    );

int
hwc_set(
    hwc_composer_device_1_t * dev,
    size_t numDisplays,
    hwc_display_contents_1_t** displays
    );

int
hwc_eventControl(
    hwc_composer_device_1_t * dev,
    int disp,
    int event,
    int enabled
    );

int
hwc_blank(
    struct hwc_composer_device_1 * dev,
    int disp,
    int blank
    );

int
hwc_query(
    hwc_composer_device_1_t * dev,
    int what,
    int* value
    );

int
hwc_getDisplayConfigs(
    struct hwc_composer_device_1* dev,
    int disp,
    uint32_t* configs,
    size_t* numConfigs
    );

int
hwc_getDisplayAttributes(
    struct hwc_composer_device_1* dev,
    int disp,
    uint32_t config,
    const uint32_t* attributes,
    int32_t* values
    );

int
hwc_device_close(
    struct hw_device_t * dev
    );

int
hwc_device_open(
    const struct hw_module_t * module,
    const char * name,
    struct hw_device_t ** device
    );
#ifdef __cplusplus
}
#endif
#endif //__HWC_GC_INTERFACE_H__
