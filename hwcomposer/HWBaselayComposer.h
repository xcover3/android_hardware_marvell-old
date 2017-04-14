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
#ifndef __HW_BASELAY_COMPOSER_H__
#define __HW_BASELAY_COMPOSER_H__
#include <string.h>
#include <hardware/hwcomposer.h>

namespace android {
/*
 * HWBaselayComposer is the wrapper class to forward HWComposer calling
 * to Vivante HWComposer to process Layers except Overlays.
 * Vivante HWComposer will composer layers into Baselay now.
 */
class HWBaselayComposer {
public:
    HWBaselayComposer();
    ~HWBaselayComposer();
    int open(const char * name, struct hw_device_t** device);
    int close(struct hw_device_t* dev);
    int prepare(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays);
    int set(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays);
    int blank(hwc_composer_device_1_t *dev, int disp, int blk);
private:
    hwc_composer_device_1_t*  mHwc;
};
};
#endif //__HW_BASELAY_MANAGER_H__
