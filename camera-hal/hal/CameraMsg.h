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

#ifndef ANDROID_HARDWARE_CAMERA_MSG_H
#define ANDROID_HARDWARE_CAMERA_MSG_H

#include <utils/threads.h>
#include <utils/StrongPointer.h>
#include <utils/List.h>

#include <hardware/camera.h>
#include "cam_log_mrvl.h"

namespace android {

typedef void (*tMsgDone_Cb)(void* privatedata);


    class MsgData: public RefBase
    {
        public:
            friend class CameraMsg;
            /*
            class MsgDoneListener{
                public:
                    friend class MsgData;
                    virtual ~MsgDoneListener(){}
            };
            */
            class MsgPreviewFrameListener{
                public:
                    friend class MsgData;
                    virtual void PreviewFrameFromClient_notify(const void* key)=0;
                    virtual ~MsgPreviewFrameListener(){}
            };

        public:
        enum POST_STYLE{
            POST_IMMEDIATELY,
            POST_FRONT,
            POST_BACK
        };

        MsgData()
        {
            fMsgDone_Cb = NULL;
            mPrivate = NULL;
            mFrameListener = NULL;
            mKey = NULL;

            msgType = 0;
            ext1 = 0;
            ext2 = 0;

            timestamp = 0;
            data = NULL;
            index = 0;
            metadata = NULL;

            //put msg to back of msg queue by default.
            flag = POST_BACK;
        }
        virtual ~MsgData(){}

        int32_t     msgType;
        int32_t     ext1;
        int32_t     ext2;
        void*       user;

        nsecs_t     timestamp;
        void*       data;
        unsigned int index;
        void*       metadata;
        uint32_t    flag;

        void setListener(tMsgDone_Cb cb, void* private_data){
            fMsgDone_Cb = cb;
            mPrivate    = private_data;
        };

        void setFrameListener(MsgPreviewFrameListener* listener, void* priv){
            mFrameListener = listener;
            mKey  = priv;
        };

        void msgDataRelease();

    private:
        tMsgDone_Cb fMsgDone_Cb;
        void* mPrivate;
        MsgPreviewFrameListener* mFrameListener;
        void* mKey;
    };


    class CameraMsg: public RefBase
    {
        public:
            CameraMsg();
            virtual ~CameraMsg();

            status_t postMsg(sp<MsgData> msg);
            void removeMsg(int32_t msgType);

            void enableMsgType(int32_t msgType);
            void disableMsgType(int32_t msgType);
            int  msgTypeEnabled(int32_t msgType);
            void destoryMsgThread();
            /*
            void NotifyCb(int32_t msg_type,
                    int32_t ext1,
                    int32_t ext2,
                    void *user);

            void DataCb(int32_t msg_type,
                    const camera_memory_t *data, unsigned int index,
                    camera_frame_metadata_t *metadata, void *user);

            void DataCbTimestamp(int64_t timestamp,
                    int32_t msg_type,
                    const camera_memory_t *data, unsigned int index,
                    void *user);
            */

            void setCallbacks(camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    void* user);

        private:

            static void* _ThreadWrapper(void* me);
            int _msgThread();

            status_t _postMsg(sp<MsgData> msg);
            void _showMsgInfo(sp<MsgData> msg);

            int32_t     mMsgEnabled;
            Mutex       mMsgLock;
            Condition   mMsgCond;
            bool        mState;

            pthread_t       mThread;
            List<sp<MsgData> >  mMsgReceived;

            camera_notify_callback     mNotifyCb_master;
            camera_data_callback       mDataCb_master;
            camera_data_timestamp_callback mDataCbTimestamp_master;
            void                *mCallbackCookie_master;
    };

}; // namespace android

#endif
