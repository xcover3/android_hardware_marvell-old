/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
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

#ifndef ANDROID_HARDWARE_ANATIVEWINDOWDISPLAY_H
#define ANDROID_HARDWARE_ANATIVEWINDOWDISPLAY_H

#include <hardware/camera.h>
#include "ImageBuf.h"
#include <ui/GraphicBufferMapper.h>

namespace android {

/*
 * ANativeWindowDisplay useage:
 * new ANativeWindowDisplay
 * setANWHandleListener()//
 * start
 * msgDisplay()
 * ...
 * pause
 * start
 * msgDisplay()
 * ...
 * stop
 */

/*{{{*/
class DisplayData: public RefBase {

    public:
        friend class ANativeWindowDisplay;

        //frame rls listener interface
        class DisplayDataListener {
            public:
                friend class DisplayData;
                virtual ~DisplayDataListener(){}
                virtual void displaydata_notify(const void* userpri)=0;
        };

    public:
        enum DISPLAY_TYPE {
            DISPLAY_TYPE_COPY       = 0x0001,
            DISPLAY_TYPE_ANW        = 0x0002,
            DISPLAY_TYPE_SWAPPING   = 0x0003,
        };

        DisplayData(DISPLAY_TYPE display_type, const void* handle, int width, int height, int len, const char* format):
            mType(display_type),
            mHandle(handle),
            mWidth(width),
            mHeight(height),
            mLen(len),
            mFormat(format),
            mListener(NULL),
            mUserPri(NULL){}

        //notification implementation
        virtual ~DisplayData(){
            if(mListener){
                mListener->displaydata_notify(mUserPri);
                TRACE_V("%s:mListener notify done",__FUNCTION__);
            }
            else{
                TRACE_V("%s:mListener not set",__FUNCTION__);
            }
        }

        void setFrameListener(/*const*/ DisplayDataListener* listener, const void* userpri) {
            mListener = listener;
            mUserPri = userpri;
        }

    private:
        DisplayData();

        DISPLAY_TYPE mType;
        int mWidth;
        int mHeight;
        int mLen;
        String8 mFormat;

        //for DISPLAY_TYPE_COPY: contain void* vaddr
        //for DISPLAY_TYPE_ANW: contain buffer_handle_t*
        const void* mHandle;

        const void* mUserPri;
        /*const*/ DisplayDataListener* mListener;

        //typedef void (*_listener_callback)(const void* user);
        //_listener_callback mNotify;
};
/*}}}*/

//Display handler class - This class basically handles the buffer posting to display
/*{{{*/
#define DEF_ANW_BUFCNT (5)
#define MAX_RETRY_COUNT (5)
class ANativeWindowDisplay: public RefBase
{

public:

    //ANW handle listener interface
    class ANWHandleListener {
        public:
            friend class ANativeWindowDisplay;
            virtual ~ANWHandleListener(){}
            virtual void ANWHandle_notify(buffer_handle_t* handle)=0;
    };

    ANativeWindowDisplay(ANWHandleListener* listener);

    virtual ~ANativeWindowDisplay();

    status_t allocateBuffers(int width, int height, const char* format,
            Vector<sp<BufDesc> >& outputqueue, int anw_bufcnt = DEF_ANW_BUFCNT);

    int setPreviewWindow(struct preview_stream_ops *window);

    status_t start();
    status_t pause();
    status_t stop();
    status_t msgDisplay(sp<DisplayData>& frame);

    //int setANWHandleListener(DisplayDataListener* listener){mANWHanleListener = listener;};

private:

    static void* _ThreadWrapper(void* me);
    int _displayThread();

    //virtual status_t _display(sp<BufferHolder> imagebuf, int index);
    status_t _display(sp<DisplayData>& frame);
    status_t _display(const void* buffer, int len);
    status_t _display(buffer_handle_t* handle);
    status_t _prepare_for_display(sp<DisplayData>& frame);


    status_t _QueueFrame(buffer_handle_t* handle);
    status_t _DequeueFrame(buffer_handle_t** phandle);
    status_t _CancelFrame(buffer_handle_t* handle);

    int _processIdle();

    int mANW_BUFCNT;
    bool mInitialized;

    Mutex mLock;
    Mutex mReceiveLock;
    bool mStart;
    bool mPause;

    int mFrameWidth;
    int mFrameHeight;
    String8  mFrameFormat;

    preview_stream_ops_t*  mANativeWindow;

    int mNativeBufCnt;
    int mUndequeuedCnt;

    /*
     *
     *mFrameDequeued:
     *    dequeue_buffer
     *    lock_buffer
     *    mapper.lock
     *
     *mFrameQueued:
     *    mapper.unlock
     *    enqueue_buffer/cancel_buffer
     *
     *mFrameHandle:
     *    mFrameDequeued + mFrameQueued
     *
    */

    KeyedVector<buffer_handle_t *, void*> mFrameHandle;//buffer_handle_t*, vaddr*

    KeyedVector<buffer_handle_t *, void*> mFrameQueued;//buffer_handle_t*, vaddr*
    KeyedVector<buffer_handle_t *, void*> mFrameDequeued;//buffer_handle_t*, vaddr*

    void _clearCurrentWindow();
    void _clearAllQueues();
    void showQueueInfo();

    pthread_t       mThread;
    Condition       mFrameAvailableCondition;

#if defined __SHOW_FPS__ || defined __DUMP_IMAGE__
    long mDisplayFrameCnt;
#endif
#ifdef __SHOW_FPS__
    CamTimer mDisplayTimer;
#endif

private:
    Vector<sp<DisplayData> >  mFramesReceived;

    //used to hold the input displayframe,
    //in other words, delay the release/notify operation.
    KeyedVector<buffer_handle_t *, sp<DisplayData> > mWindowFrameInDisplay;
#if 1
    //
    ANWHandleListener* mANWHandleListener;
    void notify_ANWHandleListener(buffer_handle_t* handle);
#endif


};
/*}}}*/
};

#endif

