/****************************************************************************
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*****************************************************************************/



#ifndef __gc_gralloc_priv_h_
#define __gc_gralloc_priv_h_

#include "gralloc_priv.h"
#include "gc_hal.h"
#include "gc_hal_driver.h"

#ifdef __cplusplus
struct gc_private_handle_t : public private_handle_t
{
#else
struct gc_private_handle_t
{
    struct private_handle_t common;
#endif
    /* GC surface format. */
    int     surfFormat;

    int     surface;
    int     vidNode;
    int     pool;
    int     adjustedSize;

    int     resolveSurface;
    int     resolveVidNode;
    int     resolvePool;
    int     resolveAdjustedSize;

    int     hwDoneSignal;

    int     lockUsage;
    int     allocUsage;
    int     clientPID;

    int     dirtyX;
    int     dirtyY;
    int     dirtyWidth;
    int     dirtyHeight;

    int     bpr;
    int     fbpost_offset;
    int     needUnregister;

#ifdef __cplusplus

    static const int sNumInts = GC_PRIVATE_HANDLE_INT_COUNT;
    static const int sNumFds  = GC_PRIVATE_HANDLE_FD_COUNT;
    static const int sMagic   = 0x3141592;

    gc_private_handle_t(int fd, int size, int flags) :
        private_handle_t(fd, size, flags),

        surfFormat(0),

        surface(0),
        vidNode(0),
        pool(0),
        adjustedSize(0),

        resolveSurface(0),
        resolveVidNode(0),
        resolvePool(0),
        resolveAdjustedSize(0),

        hwDoneSignal(0),

        lockUsage(0),
        allocUsage(0),
        clientPID(0),

        dirtyX(0),
        dirtyY(0),
        dirtyWidth(0),
        dirtyHeight(0),
        bpr(0),
        fbpost_offset(0),
        needUnregister(0)
    {
        magic   = sMagic;
        version = sizeof(native_handle);
        numInts = sNumInts;
        numFds  = sNumFds;
    }

    ~gc_private_handle_t()
    {
        magic = 0;
    }

    static int
    validate(
        const native_handle * Handle
        )
    {
        const gc_private_handle_t* hnd = (const gc_private_handle_t *) Handle;
        if (!Handle)
        {
            ALOGW("Invalid gralloc handle: NULL");

            return -EINVAL;
        }
        if ((Handle->version != sizeof(native_handle))
        ||  (Handle->numInts != sNumInts)
        ||  (Handle->numFds != sNumFds)
        ||  (hnd->magic != sMagic)
        )
        {
            ALOGE("Invalid buffer handle (at %p), "
                 "version=%d, "
                 "sizeof (native_handle)=%d, "
                 "numInts=%d, "
                 "sNumInts=%d, "
                 "numFds=%d, "
                 "sNumFds=%d, "
                 "hnd->magic=%d, "
                 "sMagic=%d",
                 Handle,
                 Handle->version,
                 sizeof (native_handle),
                 Handle->numInts,
                 sNumInts,
                 Handle->numFds,
                 sNumFds,
                 hnd->magic,
                 sMagic);

            return -EINVAL;
        }

        return 0;
    }
#endif
};

#endif /* __gc_gralloc_priv_h_ */

