
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

#include <utils/threads.h>
#include <utils/StrongPointer.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include "CameraMsg.h"

#define LOG_TAG "CameraMsg"

namespace android {

    void MsgData::msgDataRelease()
    {
        if(fMsgDone_Cb)
        {
            fMsgDone_Cb(mPrivate);
            TRACE_V("%s: fMsgDone_Cb done",__FUNCTION__);
        }
        else
        {
            TRACE_V("%s: fMsgDone_Cb not set",__FUNCTION__);
        }

        if (mFrameListener)
        {
            mFrameListener->PreviewFrameFromClient_notify(mKey);
            TRACE_V("%s: Preview frame dataCallback is done.",__FUNCTION__);
        }
        else
        {
            TRACE_V("%s: mFrameListener is not set. and preview frame dataCallback is done.",__FUNCTION__);
        }
    }

    CameraMsg::CameraMsg()
    {
        FUNC_TAG;
        mState = true;
        mMsgEnabled = 0;
        mMsgReceived.clear();
        mNotifyCb_master = NULL;
        mDataCb_master = NULL;
        mDataCbTimestamp_master = NULL;
        mCallbackCookie_master = NULL;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&mThread, &attr, _ThreadWrapper, this);
        pthread_attr_destroy(&attr);

        TRACE_D("%s:create msg thread done",__FUNCTION__);
    }

    CameraMsg::~CameraMsg(){
        FUNC_TAG;
    }

    void CameraMsg::destoryMsgThread()
    {
        Mutex::Autolock autoLock(mMsgLock);
        mState = false;
        mMsgCond.signal();
    }

//debug only
#if 0
#define CHECK_MSG(msg)	do{ \
    if(mMsgEnabled == CAMERA_MSG_ALL_MSGS) \
    {TRACE_V("\tEnabled-msg:CAMERA_MSG_ALL_MSGS");} \
    else if(mMsgEnabled & msg) \
    {TRACE_D("\tEnabled-msg:" #msg );} \
}while(0)
#define CHECK_MSG_ALL() do{ \
    CHECK_MSG(CAMERA_MSG_ERROR); \
    CHECK_MSG(CAMERA_MSG_SHUTTER); \
    CHECK_MSG(CAMERA_MSG_FOCUS); \
    CHECK_MSG(CAMERA_MSG_ZOOM); \
    CHECK_MSG(CAMERA_MSG_PREVIEW_FRAME); \
    CHECK_MSG(CAMERA_MSG_VIDEO_FRAME); \
    CHECK_MSG(CAMERA_MSG_POSTVIEW_FRAME); \
    CHECK_MSG(CAMERA_MSG_RAW_IMAGE); \
    CHECK_MSG(CAMERA_MSG_RAW_IMAGE_NOTIFY); \
    CHECK_MSG(CAMERA_MSG_COMPRESSED_IMAGE); \
}while(0)
#else
#define CHECK_MSG_ALL() do{}while(0)
#endif

    void CameraMsg::enableMsgType(int32_t msgType)
    {
        TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
        Mutex::Autolock lock(mMsgLock);
        mMsgEnabled |= msgType;
        CHECK_MSG_ALL();
    }

    void CameraMsg::disableMsgType(int32_t msgType)
    {
        TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
        Mutex::Autolock lock(mMsgLock);
        mMsgEnabled &= ~msgType;
        CHECK_MSG_ALL();
    }

    int CameraMsg::msgTypeEnabled(int32_t msgType)
    {
        TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
        Mutex::Autolock lock(mMsgLock);
        return (mMsgEnabled & msgType);
    }
#undef CHECK_MSG

    void CameraMsg::setCallbacks(
            camera_notify_callback notify_cb,
            camera_data_callback data_cb,
            camera_data_timestamp_callback data_cb_timestamp,
            void *user)
    {
        FUNC_TAG;
        Mutex::Autolock lock(mMsgLock);

        mNotifyCb_master = notify_cb;
        mDataCb_master = data_cb;
        mDataCbTimestamp_master = data_cb_timestamp;
        mCallbackCookie_master = user;
    }

    void* CameraMsg::_ThreadWrapper(void* me)
    {
        sp<CameraMsg> self = static_cast<CameraMsg*>(me);
        status_t err = self->_msgThread();
        return (void*) err;
    }

    void CameraMsg::removeMsg(int32_t msgType)
    {
        FUNC_TAG;
        Mutex::Autolock lock(mMsgLock);
        if(mMsgReceived.empty()){
            return;
        }
        List<sp<MsgData> >::iterator itor = mMsgReceived.begin();
        for(; itor != mMsgReceived.end();){
            sp<MsgData> msg = *itor;
            if(msg.get()->msgType == msgType){
                TRACE_D("%s: remove msg type 0x%x from msgqueue", __FUNCTION__, msgType);
                itor = mMsgReceived.erase(itor);
            }
            else{
                itor++;
            }
        }
    }

    status_t CameraMsg::postMsg(sp<MsgData> msg)
    {
        _showMsgInfo(msg);

#ifdef __DEBUG_WITHOUT_MSG_THREAD__
        return _postMsg(msg);
#endif

        int32_t msgType = msg.get()->msgType;
        if( !msgTypeEnabled( msgType )){
            TRACE_V("%s: Drop disabled msg: 0x%x", __FUNCTION__, msgType);
            Mutex::Autolock lock(mMsgLock);
            if (mState)
            {
                msg->msgDataRelease();
            }
            return UNKNOWN_ERROR;
        }


        int flag = msg.get()->flag;
        if(flag == MsgData::POST_IMMEDIATELY){
            return _postMsg(msg);
        }

        Mutex::Autolock lock(mMsgLock);
        size_t length = mMsgReceived.size();
        if(length>10){
            TRACE_E("Too many pending msg...");
        }
        if(flag == MsgData::POST_FRONT){
            mMsgReceived.push_front(msg);
        }
        else{
            mMsgReceived.push_back(msg);
        }
        mMsgCond.signal();
        return NO_ERROR;
    }

    int CameraMsg::_msgThread()
    {
        while(1)
        {
            mMsgLock.lock();
            while (mState && (mMsgReceived.empty())) {
                mMsgCond.waitRelative(mMsgLock, 20000000LL);
            }
            if (mState != true) {
                TRACE_D("%s: exit",__FUNCTION__);
                mMsgReceived.clear();
                mMsgLock.unlock();
                return NO_ERROR;
            }

            sp<MsgData> msg = *mMsgReceived.begin();
            mMsgReceived.erase(mMsgReceived.begin());
            mMsgLock.unlock();

            //deliver msg to upper layer
            _showMsgInfo(msg);

            _postMsg(msg);
        }
        return NO_ERROR;
    }

    //call this with mMsgLock unlocked!
    status_t CameraMsg::_postMsg(sp<MsgData> msg)
    {
        TRACE_V("%s: post Msg out:",__FUNCTION__);
        _showMsgInfo(msg);

        const MsgData* ptr=msg.get();
        int32_t msgType = ptr->msgType;
        if( !msgTypeEnabled( msgType ))
        {
            TRACE_V("%s: Drop disabled msg: 0x%x", __FUNCTION__, msgType);
            Mutex::Autolock lock(mMsgLock);
            if (mState)
            {
                msg->msgDataRelease();
            }
            return NO_ERROR;
        }

        //extract the data parcel
        int32_t     ext1;
        int32_t     ext2;
        void*       user;

        nsecs_t     timestamp;
        camera_memory_t* data;
        sp<IMemory> imem;
        unsigned int index;
        camera_frame_metadata_t* metadata;
        uint32_t    flag;

        switch(msgType){
            case CAMERA_MSG_PREVIEW_FRAME:
                metadata = static_cast<camera_frame_metadata_t *>(ptr->metadata);
                index = ptr->index;
                user = mCallbackCookie_master;
                if(mDataCb_master)
                {
                    if (index == -1)
                    {
                        imem = reinterpret_cast<IMemory*>(ptr->data);
                        data = reinterpret_cast<camera_memory_t*>(&imem);
                    }
                    else
                    {
                        data = static_cast<camera_memory_t*>(ptr->data);
                    }
                    mDataCb_master(msgType, data, index, metadata, user);
                    TRACE_V("DataCB Done");
                }
                break;
            case CAMERA_MSG_POSTVIEW_FRAME:
            case CAMERA_MSG_RAW_IMAGE:
            case CAMERA_MSG_COMPRESSED_IMAGE:
            case CAMERA_MSG_PREVIEW_METADATA:
                data = static_cast<camera_memory_t*>(ptr->data);
                metadata = static_cast<camera_frame_metadata_t *>(ptr->metadata);
                index = ptr->index;
                user = mCallbackCookie_master;
                if(mDataCb_master){
                    mDataCb_master(msgType, data, index, metadata, user);
                    TRACE_V("DataCB Done");
                }
                break;
            case CAMERA_MSG_VIDEO_FRAME:
                data = static_cast<camera_memory_t*>(ptr->data);
                index = ptr->index;
                timestamp = ptr->timestamp;
                user = mCallbackCookie_master;
                if(mDataCbTimestamp_master){
                    mDataCbTimestamp_master(timestamp, msgType, data, index, user);
                    TRACE_V("DataCbTimestamp Done");
                }
                break;
            default:
                ext1 = ptr->ext1;
                ext2 = ptr->ext2;
                user = mCallbackCookie_master;
                if(mNotifyCb_master){
                    mNotifyCb_master(msgType, ext1, ext2, user);
                    TRACE_V("NotifyCb Done");
                }
                break;
        }
        TRACE_V("msg done");

        Mutex::Autolock lock(mMsgLock);
        if (mState)
        {
            msg->msgDataRelease();
        }
        return NO_ERROR;
    }

    void CameraMsg::_showMsgInfo(sp<MsgData> msg)
    {
#ifndef __DEBUG_MSG__
#else
        const MsgData* ptr=msg.get();

        //extract the data parcel
        tMsgDone_Cb cb;
        void* priv;

        int32_t     msgType;
        int32_t     ext1;
        int32_t     ext2;
        void*       user;

        nsecs_t     timestamp;
        void* data;
        unsigned int index;
        void* metadata;
        uint32_t    flag;

        //
        cb          = ptr->fMsgDone_Cb;
        priv        = ptr->mPrivate;

        msgType     = ptr->msgType;
        ext1        = ptr->ext1;
        ext2        = ptr->ext2;
        user        = ptr->user;

        timestamp   = ptr->timestamp;
        data        = ptr->data;
        metadata    = ptr->metadata;
        index       = ptr->index;
        flag        = ptr->flag;

        TRACE_D("    -->msgType     = 0x%x", msgType);
        TRACE_D("    -->fMsgDone_Cb = 0x%x", cb);
        TRACE_D("    -->mPrivate    = 0x%x", priv);

        TRACE_D("    -->ext1        = 0x%x", ext1);
        TRACE_D("    -->ext2        = 0x%x", ext2);
        TRACE_D("    -->user        = 0x%x", user);

        TRACE_D("    -->timestamp   = %lld", timestamp);
        TRACE_D("    -->data        = 0x%x", data);
        TRACE_D("    -->metadata    = 0x%x", metadata);
        TRACE_D("    -->index       = 0x%x", index);
        TRACE_D("    -->flag        = 0x%x", flag);


#endif
        return;
    }


}; // namespace android


