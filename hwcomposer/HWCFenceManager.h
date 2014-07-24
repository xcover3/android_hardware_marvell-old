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

#ifndef __HWC_FENCE_MANAGER_H__
#define __HWC_FENCE_MANAGER_H__

#include <ui/Fence.h>
#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <cutils/properties.h>
#include <hardware/hwcomposer.h>

namespace android{

/*
 * Fence Timer Thread
 * Later, we may poll this thread on VSYNC,
 * so that we can update fence status in time.
 */
class HWCFenceTimerThread : public Thread
{
    friend class HWCFenceManager;
private:
    HWCFenceTimerThread();

    ~HWCFenceTimerThread();

    bool threadLoop();

    int64_t getCurrentStamp() const{
        return m_nCurrentStamp;
    }

    int32_t createFence(int64_t nFenceId);

    void signalFence(int64_t nFenceId);

    void reset();

private:
    int32_t m_nSyncTimeLineFd;

    int64_t m_nCurrentStamp;

    Mutex m_mutexLock;
};

class HWCFenceManager : public RefBase
{
public:
    HWCFenceManager();

    ~HWCFenceManager();

public:
    int32_t createFence(int64_t nFenceId);

    void signalFence(int64_t nFenceId);

    int64_t getCurrentStamp() const{
        return m_pEventThread->getCurrentStamp();
    }

    void reset();

    void dump(String8& result, char* buffer, int size);

private:
    ///< status
    bool m_bRunning;

    ///< the thread counting the current fence time status.
    sp<HWCFenceTimerThread> m_pEventThread;

};


}// end of namespace android

#endif
