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

#ifndef ANDROID_HARDWARE_CAMERA_FAKECAM_H
#define ANDROID_HARDWARE_CAMERA_FAKECAM_H

namespace android {

    class FakeCam: public CameraCtrl{
        public:
            FakeCam(int cameraID=0);
            virtual ~FakeCam();
            virtual void setCallbacks(sp<CameraMsg>& cameramsg,
                    camera_request_memory get_camera_memory,void* user);
            void setCallbacks(camera_notify_callback notify_cb=NULL,
                    camera_data_callback data_cb=NULL,
                    camera_request_memory get_camera_memory=NULL,
                    void* user=NULL);
            void enableMsgType(int32_t msgType){};
            void disableMsgType(int32_t msgType){};

            status_t startPreview();
            status_t stopPreview();

            status_t startCapture();
            status_t stopCapture();

            status_t startFaceDetection();
            status_t stopFaceDetection();

            status_t startVideo();
            status_t stopVideo();

            status_t setParameters(CameraParameters param);
            const CameraSetting& getParameters();

            //status_t registerPreviewBuffers(sp<BufferHolder> imagebuf);
            status_t unregisterPreviewBuffers();

            status_t enqPreviewBuffer(sp<CamBuf> buf);
            status_t deqPreviewBuffer(void** key);

            //status_t registerStillBuffers(sp<BufferHolder> imagebuf);
            status_t unregisterStillBuffers();

            status_t enqStillBuffer(sp<CamBuf> buf);
            status_t deqStillBuffer(void** key);
            sp<IMemory> getStillFrame();

            EngBufReq getPreviewBufReq();
            EngBufReq getStillBufReq();

            void* getUserData(void* ptr = NULL){return NULL;};
            sp<IMemory> getExifImage(const void* key);

            status_t autoFocus();
            status_t cancelAutoFocus();

            status_t getBufCnt(int* previewbufcnt,int* stillbufcnt,int* videobufcnt) const;
#if 0
            static int getNumberOfCameras(){
            }
#endif

            static int getNumberOfCameras();
            static void getCameraInfo(int cameraId, struct camera_info* cameraInfo);

            static const int kRed = 0xf800;
            static const int kGreen = 0x07c0;
            static const int kBlue = 0x003e;

        private:
            int mCameraId;
            CameraSetting mSetting;
            int kPreviewBufCnt;
            int kStillBufCnt;
            int kVideoBufCnt;

            camera_notify_callback     mNotifyCb;
            camera_data_callback       mDataCb;
            camera_request_memory mGetCameraMemory;
            void                *mCallbackCookie;

            List<sp<CamBuf> >  mPreviewBufQueued;
            List<sp<CamBuf> >  mPreviewBufDequeued;

            List<sp<CamBuf> >  mStillBufQueued;
            List<sp<CamBuf> >  mStillBufDequeued;

            List<sp<CamBuf> >  mVideoBufQueued;
            List<sp<CamBuf> >  mVideoBufDequeued;

            void fillpreviewbuffer(sp<CamBuf> imagebuf);
            void fillstillbuffer(sp<CamBuf> imagebuf);
            //for preview fps control
            int mPreviewTimeStamp;//us
            int mPreviewStdDelay;//us
            int mPreviewRealDelay;
            int mPreviewFrameRate;
    };

}; // namespace android
#endif
