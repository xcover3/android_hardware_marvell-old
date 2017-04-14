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
//#define LOG_NDEBUG 0
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <sys/poll.h>
#include <hardware_legacy/uevent.h>
#include "HWCDisplayEventMonitor.h"

#define VSYNC_CTRL_PATH "/sys/class/graphics/fb0/device/vsync"
#define VSYNC_TIMESTAMP_PATH "/sys/class/graphics/fb0/device/vsync_ts"

using namespace android;

HWCDisplayEventMonitor::HWCDisplayEventMonitor(hwc_procs_t * procs)
    : mProcs( procs ), mVsyncOn(false) {
    mVsyncFd = open(VSYNC_CTRL_PATH, O_WRONLY);
    if( mVsyncFd < 0 ) {
        ALOGE("Open vsync control file %s failed : %s", VSYNC_CTRL_PATH, strerror(errno));
    }
}

HWCDisplayEventMonitor::~HWCDisplayEventMonitor() {
    if( mVsyncFd > 0 )
        close(mVsyncFd);
}

void HWCDisplayEventMonitor::eventControl(int event, int enabled) {
    const char *value = enabled ? "u1" : "u0";
    int fd = -1;
    ALOGV("setevent %d to %d", event, enabled);
    switch( event ) {
        case HWC_EVENT_VSYNC:
            fd = mVsyncFd;
            if( !(mVsyncOn^(enabled == 1)) ) return;
            mVsyncOn = (enabled == 1);
            break;
        default:
            break;
    }
    if( fd < 0 ) {
        ALOGE("HWC eventControl not supported for event : %d", event);
    } else {
        _writeToFile(fd, value, 2);
    }
}

void HWCDisplayEventMonitor::onFirstRef() {
    run("Display event monitor", PRIORITY_URGENT_DISPLAY);
}

bool HWCDisplayEventMonitor::threadLoop() {

    uint64_t timestamp = 0;
    int fd;
    const int max_count = 64;
    char buffer[max_count];
    struct pollfd ufds;
    int len, res;
    memset(buffer, 0, max_count);

    fd = open(VSYNC_TIMESTAMP_PATH, O_RDONLY);
    if(fd < 0) {
        ALOGE("open vsync event file failed");
        return false;
    }

    ufds.fd = fd;
    ufds.events = 0;

    do {
        res = poll(&ufds, 1, -1);
        if(res <= 0){
            ALOGV("poll return error %d", res);
            continue;
        }

        lseek(fd, 0, SEEK_SET);
        len = read(fd, buffer, max_count);

        if(len > 0) {
            timestamp = strtoull(buffer, NULL, 16);

            if (mVsyncOn && mProcs && mProcs->vsync ) {
                ALOGV("fire vsync event w/ timestamp = %lld", timestamp);
                mProcs->vsync(mProcs, 0, timestamp);
            }
        }
    } while (1);

    close(fd);
    return false;
}

void HWCDisplayEventMonitor::_writeToFile(int fd, const char * value, int size) {
    int ret = write(fd, value, size);
    if( ret < 0 ) {
        ALOGE("failed to write value[%s] to fd %d : %s",value, fd, strerror(errno));
    }
}

