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

#ifndef ANDROID_HARDWARE_ENGINE_H
#define ANDROID_HARDWARE_ENGINE_H

namespace android {

    class Engine:public CameraCtrl{
        public:
            //Engine(const CameraSetting& setting);
            Engine(int cameraID=0);
            virtual ~Engine();
            int	    mTuning;

            static void msgDone_Cb(void* user);
            virtual void setCallbacks(sp<CameraMsg>& cameramsg,
                    camera_request_memory get_camera_memory,void* user);

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

            //preview
            status_t enqPreviewBuffer(sp<CamBuf> buf);
            status_t deqPreviewBuffer(void** key);
            status_t unregisterPreviewBuffers();

            //still
            status_t enqStillBuffer(sp<CamBuf> buf);
            status_t deqStillBuffer(void** key);
            status_t unregisterStillBuffers();

            EngBufReq getPreviewBufReq();
            EngBufReq getStillBufReq();

            status_t initPreviewParameters();
            status_t initStillParameters();

            status_t autoFocus();
            status_t cancelAutoFocus();

            status_t getBufCnt(int* previewbufcnt,int* stillbufcnt,int* videobufcnt) const;

            static int getNumberOfCameras();
            static void getCameraInfo(int cameraId, struct camera_info* cameraInfo);

            virtual void setCallbacks(camera_notify_callback notify_cb=NULL,
                    camera_data_callback data_cb=NULL,
                    camera_request_memory get_camera_memory=NULL,
                    void* user=NULL);
            virtual void enableMsgType(int32_t msgType){};
            virtual void disableMsgType(int32_t msgType){};

            static void NotifyEvents(CAM_EventId eEventId,void* param,void *pUserData);
            //malloc in Engine::constructor
            camera_frame_metadata_t mFaceData;
            camera_face_t* mFaces;
        private:
            CameraSetting mSetting;

            camera_notify_callback mNotifyCb;
            camera_data_callback       mDataCb;
            camera_request_memory mGetCameraMemory;
            void                *mCallbackCookie;

            CAM_DeviceHandle	    mCEHandle;

            CAM_CameraCapability    mCamera_caps;
            CAM_ShotModeCapability  mCamera_shotcaps;
            CAM_StillSubMode            mStillSubMode[2];   //hold capture preview and recording preview's sub mode

            KeyedVector<void*, sp<CamBuf> > mPreviewBufQueue;

            KeyedVector<void*, sp<CamBuf> > mStillBufQueue;

            KeyedVector<void*, sp<CamBuf> > mVideoBufQueue;

#if 1
            int iExpectedPicNum;
#endif
            CAM_Error ceInit(CAM_DeviceHandle *pHandle,CAM_CameraCapability *pcamera_caps);
            status_t ceStartPreview(const CAM_DeviceHandle &handle);
            status_t ceUpdateSceneModeSetting(const CAM_DeviceHandle &handle,
                    CameraSetting& setting);
            status_t ceStopPreview(const CAM_DeviceHandle &handle);

            status_t ceStartCapture(const CAM_DeviceHandle &handle);
            status_t ceStopCapture(const CAM_DeviceHandle &handle);

            status_t ceStartFaceDetection(const CAM_DeviceHandle &handle);
            status_t ceStopFaceDetection(const CAM_DeviceHandle &handle);

            status_t ceStartVideo(const CAM_DeviceHandle &handle);
            status_t ceStopVideo(const CAM_DeviceHandle &handle);

            CAM_Error ceGetPreviewFrame(const CAM_DeviceHandle &handle,void** key);
            CAM_Error ceGetStillFrame(const CAM_DeviceHandle &handle,void** key);
            CAM_Error ceGetCurrentSceneModeCap(const CAM_DeviceHandle &handle,
                    CameraSetting& setting);
            status_t ceSetStillSubMode(const CAM_DeviceHandle &handle, CAM_StillSubMode mode);

    };

}; // namespace android
#endif
