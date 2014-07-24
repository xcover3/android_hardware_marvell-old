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

#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <utils/threads.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <utils/StrongPointer.h>

#include <linux/ion.h>
#include <linux/pxa_ion.h>
#include <sys/ioctl.h>

#include <cutils/properties.h>
#include <hardware/camera.h>
#include "CameraCtrl.h"
#include "ANativeWindowDisplay.h"
#include "CameraMsg.h"

namespace android {

    #define MAX_CAMERAS_SUPPORTED (5)
    #define MAX_PREVIEW_RETRY_CNT (20)

    class CameraHardware;

    extern int HAL_getNumberOfCameras();
    extern void HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo);

#if 0
//froyo/gingerbread version
    extern CameraHardware* HAL_openCameraHardware(int cameraId);
    extern void HAL_closeCameraHardware(int cameraId);

    //class CameraPreviewInterface;
    extern CameraHardware* getInstance(CameraHardware* camera);
#endif


    class CameraHardware: virtual public RefBase, virtual public DisplayData::DisplayDataListener,
        virtual public ANativeWindowDisplay::ANWHandleListener, virtual public MsgData::MsgPreviewFrameListener
    {
        public:

            virtual void displaydata_notify(const void* user);
            virtual void ANWHandle_notify(buffer_handle_t* handle);
            virtual void PreviewFrameFromClient_notify(const void* key);

            CameraHardware(int cameraID=0);
            virtual ~CameraHardware();

            virtual void        releasePreviewBuffer(const void* key);

#if 0
//froyo/gingerbread version
            static CameraHardware* createInstance();
            static CameraHardware* createInstance(int cameraId);
            static void destroyInstance(int cameraId);
#endif
            /*******************************************************************
             * implementation of camera_device_ops functions. BEGIN
             *******************************************************************/
/*{{{*/
            /** Set the ANativeWindow to which preview frames are sent */
            int setPreviewWindow(struct preview_stream_ops *window);

            /** Set the notification and data callbacks */
            void setCallbacks( camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    camera_request_memory get_memory,
                    void *user);

            /**
             * The following three functions all take a msg_type, which is a bitmask of
             * the messages defined in include/ui/Camera.h
             */

            /**
             * Enable a message, or set of messages.
             */
            void enableMsgType(int32_t msg_type);

            /**
             * Disable a message, or a set of messages.
             *
             * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
             * HAL should not rely on its client to call releaseRecordingFrame() to
             * release video recording frames sent out by the cameral HAL before and
             * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
             * clients must not modify/access any video recording frame after calling
             * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
             */
            void disableMsgType(int32_t msg_type);

            /**
             * Query whether a message, or a set of messages, is enabled.  Note that
             * this is operates as an AND, if any of the messages queried are off, this
             * will return false.
             */
            int msgTypeEnabled(int32_t msg_type);

            /**
             * Start preview mode.
             */
            int startPreview();

            /**
             * Stop a previously started preview.
             */
            void stopPreview();

            /**
             * Returns true if preview is enabled.
             */
            int previewEnabled();

            /**
             * Request the camera HAL to store meta data or real YUV data in the video
             * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
             * it is not called, the default camera HAL behavior is to store real YUV
             * data in the video buffers.
             *
             * This method should be called before startRecording() in order to be
             * effective.
             *
             * If meta data is stored in the video buffers, it is up to the receiver of
             * the video buffers to interpret the contents and to find the actual frame
             * data with the help of the meta data in the buffer. How this is done is
             * outside of the scope of this method.
             *
             * Some camera HALs may not support storing meta data in the video buffers,
             * but all camera HALs should support storing real YUV data in the video
             * buffers. If the camera HAL does not support storing the meta data in the
             * video buffers when it is requested to do do, INVALID_OPERATION must be
             * returned. It is very useful for the camera HAL to pass meta data rather
             * than the actual frame data directly to the video encoder, since the
             * amount of the uncompressed frame data can be very large if video size is
             * large.
             *
             * @param enable if true to instruct the camera HAL to store
             *        meta data in the video buffers; false to instruct
             *        the camera HAL to store real YUV data in the video
             *        buffers.
             *
             * @return OK on success.
             */
            int storeMetaDataInBuffers(int enable);

            /**
             * Start record mode. When a record image is available, a
             * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
             * frame. Every record frame must be released by a camera HAL client via
             * releaseRecordingFrame() before the client calls
             * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
             * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
             * responsibility to manage the life-cycle of the video recording frames,
             * and the client must not modify/access any video recording frames.
             */
            int startRecording();

            /**
             * Stop a previously started recording.
             */
            void stopRecording();

            /**
             * Returns true if recording is enabled.
             */
            int recordingEnabled();

            /**
             * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
             *
             * It is camera HAL client's responsibility to release video recording
             * frames sent out by the camera HAL before the camera HAL receives a call
             * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
             * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
             * responsibility to manage the life-cycle of the video recording frames.
             */
            void releaseRecordingFrame(const void *opaque);

            /**
             * Start auto focus, the notification callback routine is called with
             * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
             * called again if another auto focus is needed.
             */
            int autoFocus();

            /**
             * Cancels auto-focus function. If the auto-focus is still in progress,
             * this function will cancel it. Whether the auto-focus is in progress or
             * not, this function will return the focus position to the default.  If
             * the camera does not support auto-focus, this is a no-op.
             */
            int cancelAutoFocus();

            /**
             * Take a picture.
             */
            int takePicture();

            /**
             * Cancel a picture that was started with takePicture. Calling this method
             * when no picture is being taken is a no-op.
             */
            int cancelPicture();

            /**
             * Set the camera parameters. This returns BAD_VALUE if any parameter is
             * invalid or not supported.
             */
            int setParameters(const char *parameters);
            int setParameters(const CameraParameters& params);

            /** Retrieve the camera parameters.  The buffer returned by the camera HAL
              must be returned back to it with put_parameters, if put_parameters
              is not NULL.
              */
            char* getParameters();

            /** The camera HAL uses its own memory to pass us the parameters when we
              call get_parameters.  Use this function to return the memory back to
              the camera HAL, if put_parameters is not NULL.  If put_parameters
              is NULL, then you have to use free() to release the memory.
              */
            void putParameters(char *);

            /**
             * Send command to camera driver.
             */
            int sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

            /**
             * Release the hardware resources owned by this object.  Note that this is
             * *not* done in the destructor.
             */
            void release();

            /**
             * Dump state of the camera hardware
             */
            int dump(int fd);
/*}}}*/
            /*******************************************************************
             * implementation of camera_device_ops functions. END
             *******************************************************************/

        private:

            /***************************************************************
             * thread handler. BEGIN
             ***************************************************************/
/*{{{*/
            class PictureThread : public Thread {
                CameraHardware* mHardware;
                public:
                PictureThread(CameraHardware* hw):
                    Thread(false), mHardware(hw) { }
                virtual void onFirstRef() {
                    run("CameraPictureThread", PRIORITY_URGENT_DISPLAY);
                }
                virtual bool threadLoop() {
                    mHardware->pictureThread();
                    //once run.
                    return false;
                }
            };

            class PreviewThread : public Thread {
                CameraHardware* mHardware;
                public:
                PreviewThread(CameraHardware* hw)
                    : Thread(false), mHardware(hw) { }
                virtual void onFirstRef() {
                    run("CameraPreviewThread", PRIORITY_NORMAL);
                }
                virtual bool threadLoop() {
                    mHardware->previewThread();
                    // loop until we need to quit
                    return true;
                }
            };

            class ProcessThread : public Thread {
                CameraHardware* mHardware;
                public:
                ProcessThread(CameraHardware* hw)
                    : Thread(false), mHardware(hw) { }
                virtual void onFirstRef() {
                    run("ProcessThread", PRIORITY_URGENT_DISPLAY);
                }
                virtual bool threadLoop() {
                    mHardware->processThread();
                    return false;
                }
            };

            class ExifThread : public Thread {
                CameraHardware* mHardware;
                public:
                ExifThread(CameraHardware* hw)
                    : Thread(false), mHardware(hw) { }
                virtual void onFirstRef() {
                    run("StillExifThread", PRIORITY_NORMAL);
                }
                virtual bool threadLoop() {
                    mHardware->exifThread();
                    // once run
                    return true;
                }

            };


            int previewThread();

            int pictureThread();

            int processThread();

            int exifThread();
/*}}}*/
            /***************************************************************
             * thread handler. END
             ***************************************************************/

#if 0
//froyo/gingerbread version
            CameraHardware(int cameraID=0);
            virtual ~CameraHardware();

            static CameraHardware* singleton;
            static CameraHardware* multiInstance[MAX_CAMERAS_SUPPORTED];
#endif

            virtual status_t    RegisterPreviewBuffers();
            void		__stopPreviewThread();
            status_t __stopExifThread();

            sp<ANativeWindowDisplay> mDisplay;
            preview_stream_ops_t*  mANativeWindow;

            const int mCameraID;
            mutable Mutex       mLock;
            mutable Mutex       mExifQueLock;
            int kPreviewBufCnt;
            int kStillBufCnt;
            int kVideoBufCnt;
            int iExpectedPicNum;
            int iSkipFrame;

#ifdef __DUMP_IMAGE__
            int mDumpNum;
#endif
            int mMinDriverBufCnt;
/*
            camera_notify_callback     mNotifyCb;
            camera_data_callback       mDataCb;
            camera_data_timestamp_callback mDataCbTimestamp;
            void                *mCallbackCookie;
*/
            camera_request_memory mGetCameraMemory;

            static void msgDone_Cb(void* user);
            bool                mRecordingEnabled;
            char PMEM_DEVICE[PROPERTY_VALUE_MAX];
            bool                mUseOverlay;
            bool                mUseANWBuf;
            int                 mDisplayEnabled;

            bool                bStoreMetaData;

            sp<CameraCtrl> mCamPtr;
            sp<IMemoryHeap> mPreviewHeap;

            Vector<sp<CamBuf> >    mPreviewImageBuf;
            Vector<sp<CamBuf> >    mStillImageBuf;
            Vector<sp<CamBuf> >    mExifImageBuf;

            //KeyedVector<void*, CamBuf> mPreviewQueue;
            //KeyedVector<void*, CamBuf> mStillQueue;

            sp<PreviewThread>   mPreviewThread;
            sp<PictureThread>   mPictureThread;
            sp<ExifThread>      mExifThread;

            //perf
#ifdef __SHOW_FPS__
            long mPreviewFrameCnt;
            long mVideoFrameCnt;
            long mStillFrameCnt;
            CamTimer mPreviewTimer;
            CamTimer mVideoTimer;
            CamTimer mStillTimer;
#endif

            /*
             * When video encoding can't catch up with camera input, streaming
             * buffer queue will finally drain. Then, preview thread begins to
             * wait for empty buffer
             * After an empty buffer is returned through releaseRecordingFrame(),
             * preview thread will be signaled
             */
            Mutex               mBuffersLock;
            Condition           mBuffersCond;

            enum CAMERA_STATE{
                CAM_STATE_IDLE,
                CAM_STATE_PREVIEW,
                CAM_STATE_STILL,
                CAM_STATE_VIDEO,
            };
            CAMERA_STATE mState;

            bool mIsPreviewBufReady;

            KeyedVector<void *, BufInfo> mFrameBuffers;//void* key,
            KeyedVector<void *, void*> mBuffersDequeued;//void* key,
            KeyedVector<void *, void*> mBuffersQueued;//void* key,

            sp<CameraMsg> mMsg;

            status_t getExifImage(const sp<CamBuf>& jpeg, const CameraSetting& setting, sp<CamBuf>& output);

            status_t deliverData(int32_t msgType, sp<CamBuf> buf, Vector<sp<CamBuf> > imagebuf);

            /*
             * Helper utilities
             */
            status_t createInternalPmemPreviewBuffers(bool needStopPreview = true);
            status_t createInternalPmemPreviewBuffers(int bufCnt, bool needStopPreview = true);
            status_t createANWPreviewBuffers(Vector<sp<BufDesc> >& ANWQueue, bool needStopPreview = true);
            status_t createANWPreviewBuffers(Vector<sp<BufDesc> >& ANWQueue, int bufCnt, bool needStopPreview = true);
            void     flushPreviewBuffersInEngine();
            bool     isPreviewParamChanged();
            CamBuf::BUF_TYPE getPreviewBufferType();

            nsecs_t mLastFrameTime, mInterval;
    };


}; // namespace android

#endif
