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

#include <linux/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <utils/Timers.h>
#include <codecDef.h>
#include <codecJP.h>
#include <misc.h>
#include <hardware_legacy/power.h>
#include "ippIP.h"
#include "CameraSetting.h"

#include "ANativeWindowDisplay.h"
#include "CameraCtrl.h"
#include "Engine.h"
#include "FakeCam.h"
#include "cam_log_mrvl.h"
#include "CameraHardware.h"

#include "ippExif.h"
#include "exif_helper.h"
#include "JpegCompressor.h"

#define LOG_TAG "CameraHardware"

#define LIKELY(exp)   __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

/*
 * property
 */
#define PROP_FAKE	"persist.service.camera.fake"	//0,1
#define PROP_CNT	"persist.service.camera.cnt"	//0,>0,<0
#define PROP_HW		"service.camera.hw"	//overlay,base
#define PROP_AF		"service.camera.af"	//0,>0
#define PROP_PMEM	"service.camera.pmem"	//NULL,/dev/pmem,/dev/pmem_adsp
#define PROP_DUMP	"service.camera.dump"	//0 for nodump, >0 specify num of frames to dump, for each preview/still cycle
#define PROP_DUMPFLY	"service.camera.dumpfly"   //runtime dump, >0 trigger dump, 0 for nodump
#define PROP_DISPLAY    "service.camera.display"    //>0 display by default, 0 nodisplay for debug
#define PROP_BUFSRC     "service.camera.bufsrc"     //anw,phy

/*
 * variables for performance optimization
 * show preview/camcorder fps
 */
#define CAMERA_WAKELOCK "CameraHardware"
#define RETURN_IF_ERROR(x) do { \
    if (NO_ERROR != (x)) return -1; \
} while (0)

// #define YUYV2UYVY(a)  ((a & 0x0F) << 8) || ((a & 0xF0) >> 8) || ((a & 0x0F00) << 8) || ((a & 0xF000) >> 8)
#define YUYV2UYVY(a)  (a)

inline void swap(unsigned int* a, unsigned int* b) {
    unsigned int tmp = YUYV2UYVY(*a);
    *a = YUYV2UYVY(*b);
    *b = tmp;
}

namespace android {


#ifdef __FAKE_CAM_AVAILABLE__
static int mEnableFake;
#endif

CameraHardware::CameraHardware(int cameraID):
    mCameraID(cameraID),
    mRecordingEnabled(false),
    mANativeWindow(NULL),
    //mUseOverlay(false),
    kPreviewBufCnt(6),
    kStillBufCnt(2),
    kVideoBufCnt(6),
    mCamPtr(NULL),
    mState(CAM_STATE_IDLE),
    mMinDriverBufCnt(0),
    mDisplayEnabled(1),
#ifdef __DUMP_IMAGE__
    mDumpNum(0),
#endif
#ifdef __SHOW_FPS__
    mPreviewFrameCnt(0),
    mVideoFrameCnt(0),
    mStillFrameCnt(0),
#endif
    iSkipFrame(0)
{
    FUNC_TAG;
    if (acquire_wake_lock(PARTIAL_WAKE_LOCK, CAMERA_WAKELOCK) > 0) {
        TRACE_D("%s:acquire_wake_lock success",__FUNCTION__);
    }
    else{
        TRACE_E("acquire_wake_lock fail!");
    }

    /*
     * initialize the default parameters for the first time.
     */

    property_get(PROP_PMEM, PMEM_DEVICE, "/dev/ion");
    TRACE_D("%s:"PROP_PMEM"=%s",__FUNCTION__,PMEM_DEVICE);

    /*
     * get property for overlay/base selectionPIXEL_FORMAT_YUV422I),
            CAM_IMGFMT_YCbYCr},

     * setprop service.camera.hw x
     * x=base or overlay
     */
    char value[PROPERTY_VALUE_MAX];
    property_get(PROP_HW, value, "base");
    if (0 == strcmp(value, "overlay"))
        mUseOverlay = true;
    else
        mUseOverlay = false;
    TRACE_D("%s:"PROP_HW"=%s",__FUNCTION__,value);

#ifdef __FAKE_CAM_AVAILABLE__
    if(mEnableFake != 0){
        mCamPtr = new FakeCam(cameraID);
    }
    else
#endif
    {
        mCamPtr = new Engine(cameraID);
    }

#ifdef __RUNTIME_DISABLE_DISPLAY__
    property_get(PROP_DISPLAY, value, "1");
    TRACE_D("%s:"PROP_DISPLAY"=%s",__FUNCTION__, value);
    mDisplayEnabled = atoi(value);
#endif

    property_get(PROP_BUFSRC, value, "anw");
    if (0 == strcmp(value, "anw"))
        mUseANWBuf = true;
    else
        mUseANWBuf = false;
    TRACE_D("%s:"PROP_BUFSRC"=%s",__FUNCTION__,value);

    mMsg = new CameraMsg();
    if(mMsg == NULL)
        TRACE_E("Fail to create cameramsg");

#ifdef __DUMP_IMAGE__
    property_get(PROP_DUMP, value, "0");
    mDumpNum = atoi(value);
    TRACE_D("%s:"PROP_DUMP"=%d",__FUNCTION__, mDumpNum);
#endif

    /*
     * get preview/still/video buf cnt from camerasetting,
     * all buf cnt should be more than 2
     * use the default one if not specified.
     */
    int previewbufcnt,stillbufcnt,videobufcnt;
    mCamPtr->getBufCnt(&previewbufcnt,&stillbufcnt,&videobufcnt);
    if(previewbufcnt >= 2)
        kPreviewBufCnt = previewbufcnt;
    if(stillbufcnt >= 2)
        kStillBufCnt = stillbufcnt;
    if(videobufcnt >= 2)
        kVideoBufCnt = videobufcnt;

    //align all parameters during init
    CameraParameters params = mCamPtr->getParameters();
    mCamPtr->setParameters(params);
}

/*
 * stop CE
 * flush all CE buffers
 * free CE file handler
 */
CameraHardware::~CameraHardware()
{
    FUNC_TAG_E;
    release();

    //destroy msg thread
    mMsg->destoryMsgThread();
    mMsg.clear();

    //destroy display component
    if(mDisplay.get() != NULL){
        mDisplay->stop();
        mDisplay.clear();
    }

    //destroy camera device
    if (mCamPtr != NULL){
        mCamPtr->stopCapture();
        mCamPtr->unregisterStillBuffers();
        mCamPtr->stopPreview();
        mCamPtr->unregisterPreviewBuffers();
        mCamPtr.clear();
    }
    //destroy still buffers.
    mStillImageBuf.clear();
    //destroy preview buffers.
    mPreviewImageBuf.clear();

    TRACE_D("%s:release_wake_lock",__FUNCTION__);
    release_wake_lock(CAMERA_WAKELOCK);

    FUNC_TAG_X;
}

void CameraHardware::setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory, void *user)
{
    FUNC_TAG;
    Mutex::Autolock lock(mLock);

    if(mMsg == NULL){
        TRACE_E("Invalid mMsg");
        return;
    }

    mMsg->setCallbacks(notify_cb, data_cb, data_cb_timestamp, user);

    //TODO:sensor instance may only sendout notification msg, no datacallback should be
    //called, so here only set notify_callback;
    //set NULL if no eventnotification needed by sensor instance
    mCamPtr->setCallbacks(mMsg, get_memory, user);
    BufferHolder::setAllocator(get_memory);
}

void CameraHardware::enableMsgType(int32_t msgType)
{
    TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
    if ( msgType & (CAMERA_MSG_PREVIEW_FRAME | CAMERA_MSG_VIDEO_FRAME) ) {
        int cnt = mPreviewImageBuf.size();
        if ( cnt > 0 && cnt < kPreviewBufCnt) {
            //camera-hal is now previewing with half the buffers, allocate more
            TRACE_D("%s:append preview image buffers count=%d",__FUNCTION__,kPreviewBufCnt - cnt);
            Mutex::Autolock lock(mBuffersLock);
            createInternalPmemPreviewBuffers(kPreviewBufCnt, false);
            BufferHolder::showState(mPreviewImageBuf);
            BufferHolder::showInfo(mPreviewImageBuf);
        }
    }
    Mutex::Autolock lock(mLock);
    mMsg->enableMsgType(msgType);
}

void CameraHardware::disableMsgType(int32_t msgType)
{
    TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
    Mutex::Autolock lock(mLock);
    mMsg->disableMsgType(msgType);
}

int CameraHardware::msgTypeEnabled(int32_t msgType)
{
    TRACE_V("-%s:0x%x",__FUNCTION__,msgType);
    Mutex::Autolock lock(mLock);
    return mMsg->msgTypeEnabled(msgType);
}

/*static*/
void CameraHardware::msgDone_Cb(void* user)
{
    camera_memory_t* mem = reinterpret_cast<camera_memory_t*>(user);
    if ( NULL != mem ) {
        mem->release(mem);
    }
    return;
}
int CameraHardware::setPreviewWindow(struct preview_stream_ops *window)
{
    FUNC_TAG;
    mLock.lock();
    TRACE_D("%s:mANativeWindow=%p, window=%p",__FUNCTION__,mANativeWindow,window);
    int ret=NO_ERROR;
    mANativeWindow = window;

    if (mANativeWindow != NULL) {
        if (mDisplay.get() == NULL) {
            mDisplay = new ANativeWindowDisplay(this);
        }
        if (mDisplay.get() != NULL) {
            ret = mDisplay->setPreviewWindow(window);
            if (NO_ERROR != RegisterPreviewBuffers()) {
                TRACE_E("Fail to RegisterPreviewBuffers.");
                return -1;
            }
            mDisplay->start();
        }
    } else {
        if (mDisplay.get() != NULL) {
            mDisplay->setPreviewWindow(window);
            mDisplay->stop();
            mDisplay.clear();
        }
        mPreviewImageBuf.clear();
     }

    mLock.unlock();
    return ret;
}

int CameraHardware::previewThread()
{
    int release_cnt = 0;//pmem buffers to be released
    int free_cnt = 0;//free pmem buffers ready to be released
    nsecs_t newFrameTime, frameInterval;

    mBuffersLock.lock();

    //loop all buf and enqueue free buf to engine
    const int cnt=mPreviewImageBuf.size();
    if ( cnt > kPreviewBufCnt ) {
        release_cnt = cnt - kPreviewBufCnt;
    }

    for (int i = 0; i<cnt; i++) {
        sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(i);
        if(buf->isFree()){
            if (LIKELY(i >= release_cnt)) {//either not in swaping progess or now looping newly allcated anw buffers
                status_t stat=mCamPtr->enqPreviewBuffer(buf);
                if(NO_ERROR == stat){
                    buf->toEngine();
                }
                else {
                    TRACE_E("enqPreviewBuffer fail!");
                }
            }
            else {
                free_cnt++;
                if (free_cnt >= release_cnt) {
                    //all previously allocated pmem buffers are free, now release them and use only anw buffers for previewing
                    mPreviewImageBuf.removeItemsAt(0,release_cnt);
                    mBuffersLock.unlock();
                    BufferHolder::showState(mPreviewImageBuf);
                    BufferHolder::showInfo(mPreviewImageBuf);
                    return NO_ERROR;
                }

            }
        }
    }
    int displayref, engineref, videoref, processref, clientref;
    int engine_refcnt=0;
    for (int i = 0; i < cnt; i++) {
        const sp<CamBuf>& buf = mPreviewImageBuf.itemAt(i);
        buf->getRef(&displayref, &engineref, &videoref, &processref, &clientref);
        engine_refcnt += engineref;
    }
    TRACE_V("%s:engine buf cnt=%d",__FUNCTION__,engine_refcnt);
    mBuffersLock.unlock();//unlock before return

    if(engine_refcnt <= mMinDriverBufCnt){//we must leave some buf in v4l2 for user-pointer
        TRACE_D("%s:driver required bufcnt:%d:current bufcnt:%d; waiting for buf...",
                __FUNCTION__,mMinDriverBufCnt,engine_refcnt);

        mBuffersLock.lock();
        BufferHolder::showState(mPreviewImageBuf);
        mBuffersCond.waitRelative(mBuffersLock, 20000000LL);//20ms timeout
        mBuffersLock.unlock();//unlock before return
        return NO_ERROR;
    }

    /*
     * at least one buf could be dequeue from engine
     */
    void* key =(void*) -1;
    status_t error = NO_ERROR;

    {
        static int PREVIEW_RETRY_CNT = 0;
        Mutex::Autolock lock(mLock);
        error = mCamPtr->deqPreviewBuffer(&key);
        if (UNLIKELY(NO_ERROR != error)) {

            TRACE_E("Fail to dequeue buf from CE... sth. must be wrong... return for next preview...");
            if(PREVIEW_RETRY_CNT++ > MAX_PREVIEW_RETRY_CNT){
                TRACE_E("Reach max deqpreviewbuffer fail, exit");
                PREVIEW_RETRY_CNT = 0;
                return -1;
            }
            else
                return NO_ERROR;
        }
        PREVIEW_RETRY_CNT = 0;
    }

    int index;
    sp<CamBuf> buf;
    {
        Mutex::Autolock lock(mBuffersLock);
        const int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf,key);

        if(index<0){
            TRACE_E("dequeued preview buffer key invalid: %p, return for next preview...",key);
            return NO_ERROR;
        }
        buf = mPreviewImageBuf.editItemAt(index);
        buf->fromEngine();
    }

#ifdef __PREVIE_SKIP_FRAME_CNT__
    if(iSkipFrame++ < __PREVIE_SKIP_FRAME_CNT__){
        TRACE_D("Skip frame: %d, key=%x",iSkipFrame,key);
        return NO_ERROR;
    }
    else{
        //avoid overflow
        iSkipFrame = __PREVIE_SKIP_FRAME_CNT__;
    }
#endif

    //flush bufferable/cachable buffers if necessary
    BufferHolder::flush(buf);

#ifdef __SHOW_FPS__
    mPreviewFrameCnt++;
#endif

#ifdef __DUMP_IMAGE__
    {
        if (UNLIKELY(mDumpNum > 0)) {
            TRACE_D("%s:"PROP_DUMP"=%d",__FUNCTION__, mDumpNum);
            if(mPreviewFrameCnt <= mDumpNum){
                BufferHolder::_dump_frame_buffer("preview", mPreviewFrameCnt, *(buf.get()));
            }

            char value[PROPERTY_VALUE_MAX];
            property_get(PROP_DUMPFLY, value, "0");
            int DumpFlyEn = atoi(value);
            TRACE_V("%s:"PROP_DUMPFLY"=%d",__FUNCTION__, DumpFlyEn);
            if(DumpFlyEn > 0){
                TRACE_D("%s:Runtime dump enabled",__FUNCTION__);
                BufferHolder::_dump_frame_buffer("preview_fly", mPreviewFrameCnt, *(buf.get()));
            }
        }
    }
#endif

    //To make frame rate more smoothly;
    newFrameTime = systemTime();
    frameInterval = newFrameTime - mLastFrameTime;
    mLastFrameTime = newFrameTime;

    if (frameInterval < 14000000)
    {
        TRACE_D("too short preview frame interval %lld ms", frameInterval / 1000000L);
        usleep((14000000 - frameInterval) / 1000);
    }

    //display
    if (LIKELY(mDisplayEnabled && mANativeWindow)){
        //by default, we always set return status to fail so that buf could be recycle correctly
        //even display was disabled.
        status_t ret = -1;

        //increase the ref cnt beforehand
        mBuffersLock.lock();
        buf->toDisplay();
        mBuffersLock.unlock();

        sp<DisplayData> frame;
        if (buf->getType() == CamBuf::BUF_TYPE_PMEM) {
            //Prepare info for display
            const char* format = buf->getFormat();
            int w,h;
            buf->getSize(&w,&h);
            unsigned char* data = buf->getVAddr();
            size_t size = buf->getLen();

            //construct display frame
            frame = new DisplayData(
                (mUseANWBuf) ? DisplayData::DISPLAY_TYPE_SWAPPING : DisplayData::DISPLAY_TYPE_COPY,
                data, w, h, size, format);

            frame->setFrameListener(this,key);
        }
        else{
            void* data = key;
            frame = new DisplayData(
                    DisplayData::DISPLAY_TYPE_ANW,
                    data,
                    0,
                    0,
                    0,
                    ""
                    );
            frame->setFrameListener(0,0);//don't need the callback notification for ANW buffer
        }

        if (mDisplay.get() != NULL) {
            ret = mDisplay->msgDisplay(frame);
        }

        //decrease the ref cnt if fail
        if(NO_ERROR != ret){
            ALOGD("post buffer to display thread failed");
            //ref of the buffer will be decreased in ~DisplayData;
        }
    }

    //preview
    status_t ret = deliverData(CAMERA_MSG_PREVIEW_FRAME, buf, mPreviewImageBuf);

    //recording
    {
        bool brecordingenabled;
        {
            //Notes: DONT hold mLock when do callback to service side
            Mutex::Autolock lock(mLock);
            brecordingenabled = mRecordingEnabled;
        }
        if (brecordingenabled){
            TRACE_V("%s:mRecordingEnabled = true",__FUNCTION__);
#ifdef __SHOW_FPS__
            mVideoFrameCnt++;
#endif

            status_t ret = -1;

            //increase the ref cnt beforehand
            mBuffersLock.lock();
            buf->toVideo();
            mBuffersLock.unlock();

            /* // flip it in vertical */
            // int i, j;
            // const EngBuf& engbuf = buf->getEngBuf();
            // CAM_ImageBuffer* pImgBuf_still = const_cast<CAM_ImageBuffer*>(engbuf.getCAM_ImageBufferPtr());
            // unsigned char* ptr = (unsigned char*) pImgBuf_still->pBuffer[0];
            // for (i = 0; i < pImgBuf_still->iHeight; i++) {
                // for (j = 0; j < pImgBuf_still->iWidth; j+=4) {
                    // unsigned int* swap1 = (unsigned int*)(&ptr[j]);
                    // unsigned int* swap2 = (unsigned int*)(&ptr[pImgBuf_still->iWidth * 2 - j - 4]);
                    // swap(swap1, swap2);
                // }
                // ptr += pImgBuf_still->iWidth * 2;
            // }

            ret = deliverData(CAMERA_MSG_VIDEO_FRAME, buf, mPreviewImageBuf);
            if(ret != NO_ERROR){
                mBuffersLock.lock();
                buf->fromVideo();
                mBuffersLock.unlock();
            }
        }
    }

    usleep(1000);

    //FUNC_TAG_X;
    return NO_ERROR;
}

status_t CameraHardware::createInternalPmemPreviewBuffers(int bufCnt, bool needStopPreview) {
    if (needStopPreview) {
        mCamPtr->stopPreview();
        mCamPtr->unregisterPreviewBuffers();
        mPreviewImageBuf.clear();
    }

    int startCnt = mPreviewImageBuf.size();
    const EngBufReq bufreq = mCamPtr->getPreviewBufReq();
    mMinDriverBufCnt = bufreq.getMinBufCount();
    //allow internal buffers to be released separately
    for (int i = startCnt; i < bufCnt; i++) {
        mPreviewImageBuf.appendVector(
                BufferHolder::allocBuffers(bufreq, 1, PMEM_DEVICE) );
        mPreviewImageBuf.editItemAt(i)->setKey((void*)(i));
        mPreviewImageBuf.editItemAt(i)->editEngBuf().setUserIndex(i);
    }

    if (mPreviewImageBuf.size() < bufCnt) {
        TRACE_E("fail to allocate Pmem buffer");
        return NO_MEMORY;
    }

    return NO_ERROR;
}

status_t CameraHardware::createInternalPmemPreviewBuffers(bool needStopPreview) {
    return createInternalPmemPreviewBuffers(kPreviewBufCnt, needStopPreview);
}

status_t CameraHardware::createANWPreviewBuffers(Vector<sp<BufDesc> >& ANWQueue, int bufCnt, bool needStopPreview) {
    if (needStopPreview) {
        mCamPtr->stopPreview();
        mCamPtr->unregisterPreviewBuffers();
        mPreviewImageBuf.clear();
    }

    if(mDisplay.get() != NULL){
        mDisplay->stop();
        mDisplay.clear();
    }
    mDisplay = new ANativeWindowDisplay(this);
    mDisplay->setPreviewWindow(mANativeWindow);

    const EngBufReq bufreq = mCamPtr->getPreviewBufReq();
    mMinDriverBufCnt = bufreq.getMinBufCount();

    int w, h;
    const char* format = NULL;
    CameraParameters params = mCamPtr->getParameters();
    params.getPreviewSize(&w, &h);
    format = params.getPreviewFormat();

    status_t stat = mDisplay->allocateBuffers(w, h, format, ANWQueue, bufCnt);
    if(stat != NO_ERROR || ANWQueue.size() == 0){
        TRACE_E("fail to allocate ANW buffer");
        return NO_MEMORY;
    }

    return NO_ERROR;
}

status_t CameraHardware::createANWPreviewBuffers(Vector<sp<BufDesc> >& ANWQueue, bool needStopPreview) {
    return createANWPreviewBuffers(ANWQueue, kPreviewBufCnt, needStopPreview);
}

void CameraHardware::flushPreviewBuffersInEngine() {
    mCamPtr->unregisterPreviewBuffers();
    const int cnt = mPreviewImageBuf.size();
    for (int i = 0; i<cnt; i++) {
        sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(i);
        int displayref,engineref,videoref,processref,clientref;
        buf->getRef(&displayref,&engineref,&videoref,&processref,&clientref);
        if(engineref > 0)
            buf->fromEngine();
    }
}

bool CameraHardware::isPreviewParamChanged() {
    int w, h, previous_w = -1, previous_h = -1;
    const char* format = "";
    const char* previous_format = "";

    CameraParameters params = mCamPtr->getParameters();
    params.getPreviewSize(&w, &h);
    format = params.getPreviewFormat();

    if ((int)(mPreviewImageBuf.size()) > 0) {
        const sp<CamBuf> frame = mPreviewImageBuf.itemAt(0);
        previous_format = frame->getFormat();
        frame->getSize(&previous_w, &previous_h);
    }

    if (previous_w == w &&
        previous_h == h &&
        0 == strcmp(format, previous_format)) {
        return false;
    }

    return true;
}

CamBuf::BUF_TYPE CameraHardware::getPreviewBufferType() {
    CamBuf::BUF_TYPE buf_type = CamBuf::BUF_TYPE_NONE;

    if((int)(mPreviewImageBuf.size()) > 0)
    {
        const sp<CamBuf> frame = mPreviewImageBuf.itemAt(0);
        buf_type = frame->getType();
    }

    return buf_type;
}

status_t CameraHardware::RegisterPreviewBuffers()
{
    Mutex::Autolock lock(mBuffersLock);
    if (mANativeWindow == NULL) {
        TRACE_D("%s:mANativeWindow is invalid",__FUNCTION__);

        if (!isPreviewParamChanged() && CAM_STATE_STILL == mState) {
            //still->preview, and preview param not changed.
            TRACE_D("%s:PMEM path: preview size/format not changed, switch to preview directly, using previous buffers!",__FUNCTION__);
            flushPreviewBuffersInEngine();
        } else {
            //idle->preview, or preview param changed.
            TRACE_D("%s:PMEM path: preview size/format changed, switch to idle then preview, re-alloc buffers!",__FUNCTION__);
            if ( msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME | CAMERA_MSG_VIDEO_FRAME) ) {
                RETURN_IF_ERROR(createInternalPmemPreviewBuffers());
            }
            else {
                RETURN_IF_ERROR(createInternalPmemPreviewBuffers( (kPreviewBufCnt+1)/2) );
            }
        }
    } else {
        TRACE_D("%s:mANativeWindow is valid",__FUNCTION__);

        if (!mUseANWBuf) {
            if (!isPreviewParamChanged()) {
                if (CAM_STATE_STILL == mState) {
                    //debug path: still->preview, and preview param not changed.
                    TRACE_D("%s:debug path: preview size/format not changed, switch to preview directly, using previous buffers!",__FUNCTION__);
                    flushPreviewBuffersInEngine();
                } else {
                    //debug path: idle->preview, and preview param not changed.
                    Vector<sp<BufDesc> > ANWQueue;
                    RETURN_IF_ERROR(createANWPreviewBuffers(ANWQueue, DEF_ANW_BUFCNT, false));
                }
            } else {
                //debug path: preview param changed.
                TRACE_D("%s:debug path: preview size/format changed, switch to idle then preview, re-alloc buffers!",__FUNCTION__);
                Vector<sp<BufDesc> > ANWQueue;
                RETURN_IF_ERROR(createInternalPmemPreviewBuffers());
                RETURN_IF_ERROR(createANWPreviewBuffers(ANWQueue, DEF_ANW_BUFCNT, false));
            }
        } else {
            if (!isPreviewParamChanged()) {
                if (CAM_STATE_STILL == mState) {
                    //still->preview, and preview param not changed.
                    TRACE_D("%s:ANW path: preview size/format not changed, switch to preview directly, using previous buffers!",__FUNCTION__);
                    flushPreviewBuffersInEngine();
                }
                else if (getPreviewBufferType() == CamBuf::BUF_TYPE_PMEM &&
                    (int)mPreviewImageBuf.size() <= kPreviewBufCnt) {
                    //When valid surface is set when pmem preview is in progress WITHOUT display thread,
                    //allocate anw buffers and append them to the end of mPreviewImageBuf,
                    //then start display thread and buffer swapping progress.
                    TRACE_D("%s:ANW path: buffer type changed, append ANW buffers to swap out Pmem buffers!",__FUNCTION__);
                    Vector<sp<BufDesc> > ANWQueue;
                    RETURN_IF_ERROR(createANWPreviewBuffers(ANWQueue, false));
                    const EngBufReq bufreq = mCamPtr->getPreviewBufReq();
                    mPreviewImageBuf.appendVector(BufferHolder::registerANWBuf(bufreq, ANWQueue));
                } else {
                    //idle->preview, preview param not changed
                    TRACE_D("%s:ANW path: preview size/format not changed, switch to idle then preview, re-alloc buffers!",__FUNCTION__);
                    Vector<sp<BufDesc> > ANWQueue;
                    RETURN_IF_ERROR(createANWPreviewBuffers(ANWQueue));
                    const EngBufReq bufreq = mCamPtr->getPreviewBufReq();
                    mPreviewImageBuf = BufferHolder::registerANWBuf(bufreq, ANWQueue);
                }
            } else {
                //preview param changed.
                TRACE_D("%s:ANW path: preview size/format changed, switch to idle then preview, re-alloc buffers!",__FUNCTION__);
                Vector<sp<BufDesc> > ANWQueue;
                RETURN_IF_ERROR(createANWPreviewBuffers(ANWQueue));
                const EngBufReq bufreq = mCamPtr->getPreviewBufReq();
                mPreviewImageBuf = BufferHolder::registerANWBuf(bufreq, ANWQueue);
            }
        }
    }

    BufferHolder::showState(mPreviewImageBuf);
    BufferHolder::showInfo(mPreviewImageBuf);

    return NO_ERROR;
}

status_t CameraHardware::startPreview()
{
    FUNC_TAG_E;

    mLock.lock();

    if (mPreviewThread != 0) {
        TRACE_E("previous mPreviewThread not clear");
        mLock.unlock();
        return INVALID_OPERATION;
    }
    mLock.unlock();

    //disableMsgType(CAMERA_MSG_SHUTTER|CAME RA_MSG_COMPRESSED_IMAGE|CAMERA_MSG_RAW_IMAGE|CAMERA_MSG_RAW_IMAGE_NOTIFY);
    //cancelPicture();

    if ((mANativeWindow == NULL) || (mPreviewImageBuf.size() == 0)){
        if (NO_ERROR != RegisterPreviewBuffers()){
            TRACE_E("Fail to RegisterPreviewBuffers.");
            return -1;
        }
    } else {
        TRACE_D("%s:preview buffer has been registered",__FUNCTION__);
    }

    if (mDisplay.get() != NULL) {
        mDisplay->start();
    }

    if (NO_ERROR != mCamPtr->startPreview()){
        TRACE_E("Fail to startPreview.");
        return -1;
    }

    mState = CAM_STATE_PREVIEW;
    iSkipFrame = 0;

#ifdef DUMP_IMAGE
    preview_cycle++;
#endif

#ifdef __SHOW_FPS__
    mPreviewFrameCnt = 0;
    mPreviewTimer.start();
#endif

    mLastFrameTime = systemTime();
    mPreviewThread = new PreviewThread(this);

    FUNC_TAG_X;
    return NO_ERROR;
}

/*
 * this function maybe called multi-times by cameraservice.
 * this function maybe called without calling startpreview beforehand.
 */
void CameraHardware::stopPreview()
{
    FUNC_TAG_E;

    __stopPreviewThread();

    mCamPtr->stopPreview();
    mCamPtr->unregisterPreviewBuffers();

    //we have to force release all ANW buffers as cameraservice may disconnect from
    //previous surface immediately after stoppreview.
    if(mDisplay.get() != NULL){
        mDisplay->stop();
        mDisplay.clear();
    }

    mBuffersLock.lock();
    BufferHolder::showState(mPreviewImageBuf);
    int cnt = mPreviewImageBuf.size();
    for(int i=0; i<cnt; i++) {
        sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(i);
        buf->resetState();
        if(buf->getType() != CamBuf::BUF_TYPE_PMEM){
            buf->toDisplay();
        }
    }
    mPreviewImageBuf.clear();
    mBuffersLock.unlock();

    mState = CAM_STATE_IDLE;

    FUNC_TAG_X;
}

//donot stop sensor preview, only cancel the preview thread
void CameraHardware::__stopPreviewThread()
{
    FUNC_TAG_E;

#ifdef __SHOW_FPS__
    mPreviewTimer.stop();
    mPreviewTimer.showDur("Preview Duration:");
    mPreviewTimer.showFPS("preview fps:",mPreviewFrameCnt);
    mPreviewFrameCnt = 0;
#endif

    //check previewthread, we simply assume stoppreview would be called at any moment
    sp<PreviewThread> previewThread;
    {//scope for the lock
        Mutex::Autolock lock(mLock);
        previewThread = mPreviewThread;
    }

    //don't hold the lock while waiting for the thread to quit
    if (previewThread != NULL) {
        TRACE_D("%s(pid=%d): request preview thread to exit",
                __FUNCTION__, gettid());
        previewThread->requestExitAndWait();
    }
    else{
        TRACE_D("%s:preview thread already exit",__FUNCTION__);
        return;
    }

    {
        Mutex::Autolock lock(mLock);
        mPreviewThread.clear();
    }
    FUNC_TAG_X;
}

int CameraHardware::previewEnabled()
{
    return (mPreviewThread != 0 );
}

int CameraHardware::recordingEnabled(){
    return mRecordingEnabled;
}

int CameraHardware::storeMetaDataInBuffers(int enable){
    return -1;
    /*
    bStoreMetaData = enable;
    return NO_ERROR;
    */
}

status_t CameraHardware::startRecording()
{
    FUNC_TAG;
    mBuffersLock.lock();
    BufferHolder::showState(mPreviewImageBuf);
    mBuffersLock.unlock();
    Mutex::Autolock lock(mLock);

    mCamPtr->startVideo();
    mRecordingEnabled = true;

#ifdef __SHOW_FPS__
    mVideoFrameCnt = 0;
    mVideoTimer.start();
#endif

    return NO_ERROR;
}

void CameraHardware::stopRecording()
{
    FUNC_TAG;
    Mutex::Autolock lock(mLock);
    mRecordingEnabled = false;

#ifdef __SHOW_FPS__
    mVideoTimer.stop();
    mVideoTimer.showDur("Video Duration:");
    mVideoTimer.showFPS("Video fps:",mVideoFrameCnt);
    mVideoFrameCnt = 0;
#endif

    mBuffersLock.lock();
    BufferHolder::showState(mPreviewImageBuf);

    //XXX:recycle all recording buffers for previous recording session.
    //NOTES: actually there is a racecondition in cameraservice videoframecallback path,
    //which is hiden by videoframe rls strategy in camerasource.
    int displayref, engineref, videoref, processref, clientref;
    const int cnt=mPreviewImageBuf.size();
    for (int i = 0; i < cnt; i++) {
        const sp<CamBuf>& buf = mPreviewImageBuf.itemAt(i);
        buf->getRef(&displayref, &engineref, &videoref, &processref, &clientref);
        if(0 < videoref){
            buf->fromVideo();
        }
    }

    mBuffersLock.unlock();

    mCamPtr->stopVideo();

}

void CameraHardware::releasePreviewBuffer(const void* key)
{
    Mutex::Autolock buflock(mBuffersLock);
    int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf, key);
    if (index>=0) {
        TRACE_V("%s: buffer key #%p, index #%d returned", __FUNCTION__, key, index);
        mPreviewImageBuf.editItemAt(index)->fromDisplay();
        mBuffersCond.signal();
    }
    else {
        TRACE_E("%s: invalid key #%p, index=%d", __FUNCTION__, key, index);
    }
}

void CameraHardware::displaydata_notify(const void* key)
{
    releasePreviewBuffer(key);
}

void CameraHardware::PreviewFrameFromClient_notify(const void* key)
{
    Mutex::Autolock buflock(mBuffersLock);
    int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf, key);
    if (index>=0) {
        TRACE_V("%s: buffer key #%p, index #%d returned", __FUNCTION__, key, index);
        mPreviewImageBuf.editItemAt(index)->fromClient();
        mBuffersCond.signal();
    }
    else {
        TRACE_E("%s: invalid key #%p, index=%d", __FUNCTION__, key, index);
    }
}

void CameraHardware::ANWHandle_notify(buffer_handle_t* handle)
{
    if(mUseANWBuf){
        releasePreviewBuffer(handle);
    }
}

void CameraHardware::releaseRecordingFrame(const void *opaque)
{
    const void* mem=opaque;
    Mutex::Autolock buflock(mBuffersLock);
    int index = BufferHolder::getQueueIndexByVAddr(mPreviewImageBuf, mem);
    if (index>=0) {
        TRACE_V("%s: buffer key #%p, index #%d returned", __FUNCTION__, mem, index);
        mPreviewImageBuf.editItemAt(index)->fromVideo();
        mBuffersCond.signal();
    }
    else {
        TRACE_E("%s: invalid key #%p, index=%d", __FUNCTION__, mem, index);
    }
}

status_t CameraHardware::autoFocus()
{
    FUNC_TAG;
    int iEnableAutoFocus                    = 0;
    char value[PROPERTY_VALUE_MAX]          = {0};
    status_t stat                           = NO_ERROR;

    //remove previous autofocus notification
    if(mMsg != NULL)
        mMsg->removeMsg(CAMERA_MSG_FOCUS);

    property_get(PROP_AF, value, "1");
    TRACE_D("%s:"PROP_AF"=%s",__FUNCTION__, value);
    iEnableAutoFocus = atoi(value);

    if(iEnableAutoFocus == 0)
    {
        {
            sp<MsgData> msg = new MsgData;
            MsgData* ptr    = msg.get();
            ptr->msgType    = CAMERA_MSG_FOCUS;
            ptr->ext1       = true;
            ptr->ext2       = 0;
            mMsg->postMsg(msg);
        }
        return NO_ERROR;
    }

    {
        Mutex::Autolock lock(mLock);
        stat = mCamPtr->autoFocus();
    }

    if(NO_ERROR != stat)
    {
        TRACE_E("focus failed.");
        {
            sp<MsgData> msg = new MsgData;
            MsgData* ptr    = msg.get();
            ptr->msgType    = CAMERA_MSG_FOCUS;
            ptr->ext1       = false;
            ptr->ext2       = 0;
            mMsg->postMsg(msg);
        }
    }

    return NO_ERROR;
}

status_t CameraHardware::cancelAutoFocus()
{
    FUNC_TAG;
    int iEnableAutoFocus                    = 0;
    char value[PROPERTY_VALUE_MAX]          = {0};

    property_get(PROP_AF, value, "1");
    TRACE_D("%s:"PROP_AF"=%s",__FUNCTION__, value);
    iEnableAutoFocus = atoi(value);
    if(iEnableAutoFocus == 0){
        return NO_ERROR;
    }

    status_t ret;
    {
        Mutex::Autolock lock(mLock);
        ret = mCamPtr->cancelAutoFocus();
    }

    if(mMsg != NULL)
        mMsg->removeMsg(CAMERA_MSG_FOCUS);

    return ret;
}

/*
 * when enter picturethread,
 * CE should be at STILL state,
 * all free preview buf should have been enqueue to preview port for snapshot
 * when exit,
 * CE should be at preview state,
 * preview buf status are recorder by bufstate[]
 */
int CameraHardware::pictureThread()
{
    FUNC_TAG_E;
#ifdef __SHOW_FPS__
    mStillTimer.start();
#endif

    void* bufkey =(void*) -1;
    int buflen=-1;
    status_t ret_code = NO_ERROR;

    camera_memory_t* exif_cammemt=NULL;
    int rls_exif_cammemt=0;
    int index = -1;

    sp<CamBuf> exif_buf;
    status_t exif_ret=NO_ERROR;
    int jpeg_bufidx = -1;
    sp<CamBuf> jpeg;
    CameraSetting setting;

    //register still buffer, we may do continuous shot
    {
        Mutex::Autolock lock(mBuffersLock);
        const int cnt=mStillImageBuf.size();
        for (int i = 0; i<cnt; i++) {
            sp<CamBuf>& buf = mStillImageBuf.editItemAt(i);
            if(buf->isFree()){
                status_t stat=mCamPtr->enqStillBuffer(buf);
                if(NO_ERROR == stat)
                    buf->toEngine();
                else
                    TRACE_E("enqStillBuffer fail!");
            }
        }

        status_t stat = mCamPtr->deqStillBuffer(&bufkey);
        if(NO_ERROR != stat){
            ret_code = -1;
            goto quit;
        }
        jpeg_bufidx = BufferHolder::getQueueIndexByKey(mStillImageBuf, bufkey);
        if(jpeg_bufidx < 0){
            ret_code = -1;
            goto quit;
        }
        mStillImageBuf.editItemAt(jpeg_bufidx)->fromEngine();
        jpeg = mStillImageBuf.itemAt(jpeg_bufidx);
    }

#ifdef __DUMP_IMAGE__
    {
        if (UNLIKELY(mDumpNum > 0)) {
            TRACE_D("%s:"PROP_DUMP"=%d", __FUNCTION__, mDumpNum);
            if(mDumpNum){
                BufferHolder::_dump_frame_buffer("jpeg", mStillFrameCnt, *(jpeg.get()));
            }
        }
    }
#endif

#if 1
    {
        //FIXME: here simply post a temp buffer, as in ZSL mode, the raw data could not be gotten from Engine
        //by the same API calling.
        mLock.lock();
        if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE))
        {
            int len = 4;
            camera_memory_t* memtmp = mGetCameraMemory(-1, len, 1, NULL);
            int index = 0;
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = CAMERA_MSG_RAW_IMAGE;
                ptr->data       = memtmp;
                ptr->index      = index;
                ptr->setListener(msgDone_Cb, memtmp);
                mMsg->postMsg(msg);
            }
        }
        else if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY))
        {
            sp<MsgData> msg = new MsgData;
            MsgData* ptr    = msg.get();
            ptr->msgType    = CAMERA_MSG_RAW_IMAGE_NOTIFY;
            ptr->ext1       = 0;
            ptr->ext2       = 0;
            mMsg->postMsg(msg);
        }

    }
#else
    {//handle raw image
        void* snapshot_bufkey=(void*) -1;
        status_t stat = mCamPtr->deqPreviewBuffer(&snapshot_bufkey);
        mLock.lock();
        if(NO_ERROR != stat){
            TRACE_D("Fail to get snapshot, Deliver false data");
            sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(0);
            //if fail to get snapshot, diliver the false buffer to application
            //otherwise, CTS will fail at “Raw picture callback not received”

            if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE))
            {
                status_t ret = deliverData(CAMERA_MSG_RAW_IMAGE, buf, mPreviewImageBuf);
            }
            else if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY)){
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = CAMERA_MSG_RAW_IMAGE_NOTIFY;
                ptr->ext1       = 0;
                ptr->ext2       = 0;
                mMsg->postMsg(msg);
            }
        }
        else{
            int index;
            sp<CamBuf> buf;
            {
                Mutex::Autolock lock(mBuffersLock);
                const int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf,snapshot_bufkey);

                if(index<0){
                    TRACE_E("dequeued snapshot key invalid: %p",snapshot_bufkey);
                }
                else{
                    buf = mPreviewImageBuf.editItemAt(index);
                    buf->fromEngine();

                    //flush bufferable/cachable buffers if necessary
                    BufferHolder::flush(buf);

                    if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE))
                    {
                        status_t ret = deliverData(CAMERA_MSG_RAW_IMAGE, buf, mPreviewImageBuf);
                    }
                    else if(mMsg->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY)){
                        sp<MsgData> msg = new MsgData;
                        MsgData* ptr    = msg.get();
                        ptr->msgType    = CAMERA_MSG_RAW_IMAGE_NOTIFY;
                        ptr->ext1       = 0;
                        ptr->ext2       = 0;
                        mMsg->postMsg(msg);
                    }
                }
            }
        }
    }
#endif

    //still buf generated, try exif based on it
    //flush all preview buf here
    {
        mCamPtr->unregisterPreviewBuffers();
        setting = mCamPtr->getParameters();

        if (NO_ERROR != mCamPtr->stopCapture()){
            TRACE_E("Fail to stopCapture.");
            mLock.unlock();
            return -1;
        }
        //free still buf here, to avoid out-of-memory issue
        mCamPtr->unregisterStillBuffers();
        mLock.unlock();
    }

    exif_ret = getExifImage(jpeg, setting, exif_buf);

    if(exif_ret == NO_ERROR && exif_buf != NULL){
        TRACE_D("%s:exif will be delivered.",__FUNCTION__);
        Mutex::Autolock exiflock(mExifQueLock);
        mExifImageBuf.push_back(exif_buf);
    }
    else{
       TRACE_E("exif generate failed,jpeg will be deliverd");
        void* vaddr = jpeg->getVAddr();
        int len = jpeg->getLen();
        EngBufReq bufreq = mCamPtr->getStillBufReq();
        Vector<sp<CamBuf> > queue = BufferHolder::allocBuffers(
                bufreq,
                1,
                PMEM_DEVICE);
        exif_buf = queue.itemAt(0);
        void* vaddr_des = exif_buf->getVAddr();
        memcpy(vaddr_des, vaddr, len);
        // TODO use software jpg encoder
        // YUYVJpegCompressor compressor;
        // const EngBuf& engbuf = jpeg->getEngBuf();
        // CAM_ImageBuffer* pImgBuf_still = const_cast<CAM_ImageBuffer*>(engbuf.getCAM_ImageBufferPtr());
        // status_t ret = compressor.compressRawImage(vaddr, pImgBuf_still->iWidth, pImgBuf_still->iHeight, 90);
        // if (ret == NO_ERROR) {
            // compressor.getCompressedImage(vaddr_des);
            // exif_buf->setLen(compressor.getCompressedSize());
        // } else {
            // ALOGE("%s(): compression failure", __func__);
            // return -ENODATA;
        // }
        Mutex::Autolock exiflock(mExifQueLock);
        mExifImageBuf.push_back(exif_buf);
    }
    mStillImageBuf.clear();

    if (mExifThread == NULL)
    {
        mExifThread = new ExifThread(this);
        if(mExifThread == NULL)
        {
            TRACE_E("fail to create exifthread");
            return UNKNOWN_ERROR;
        }
    }

quit:

    if(ret_code != NO_ERROR){
        TRACE_E("deqStillBuffer fail");
        TRACE_E("NULL jpeg will be delivered");
    }

#ifdef __SHOW_FPS__
    mStillTimer.stop();
    mStillTimer.showDur("Still Duration:");
    mStillFrameCnt = 0;
#endif

    FUNC_TAG_X;
    return ret_code;
}


int CameraHardware::exifThread()
{
    sp<CamBuf> exif_buf;
    camera_memory_t* exif_cammemt=NULL;
    int index = -1;
    int rls_exif_cammemt = 0;

    mExifQueLock.lock();
    const int cnt=mExifImageBuf.size();
    //for(int i=0; i<cnt; i++){
    if (cnt > 0){
        TRACE_D("%s:exif will be delivered.",__FUNCTION__);
        exif_buf = mExifImageBuf.editItemAt(0);
        mExifImageBuf.removeAt(0);
        mExifQueLock.unlock();

        if(exif_buf->getType() == CamBuf::BUF_TYPE_CAMERAMEMT ||
            exif_buf->getType() == CamBuf::BUF_TYPE_CAMERAMEMT_RAW){
            TRACE_D("%s: BUF_TYPE_CAMERAMEMT used, post directly", __FUNCTION__);
            sp<BufferHolder> exif_imgbuf = exif_buf->getOwner();
            exif_cammemt = exif_imgbuf->getCamMemT();
            unsigned char* vaddr = exif_buf->getVAddr();
            index = (int)(vaddr - (unsigned char*)(exif_cammemt->data))/exif_buf->getLen();
            assert(index>=0);
            rls_exif_cammemt = 0;
        }
        else{
            TRACE_D("%s: non BUF_TYPE_CAMERAMEMT used, copy and post",__FUNCTION__);
            void* vaddr = exif_buf->getVAddr();
            int len = exif_buf->getLen();
            // sp<BufferHolder> exif_imgbuf = exif_buf->getOwner();
            // exif_cammemt = exif_imgbuf->getCamMemT();
            exif_cammemt = mGetCameraMemory(-1, len, 1, NULL);
            TRACE_E("%s: exif_cammemt %d",__FUNCTION__, exif_cammemt);
            index = 0;
            memcpy(exif_cammemt->data, vaddr, len);
            rls_exif_cammemt = 1;
        }

#ifdef __DUMP_IMAGE__
        {
            if (UNLIKELY(mDumpNum > 0)) {
                TRACE_D("%s:"PROP_DUMP"=%d", __FUNCTION__, mDumpNum);
                if(mDumpNum){
                    BufferHolder::_dump_frame_buffer("exif", mStillFrameCnt, *(exif_buf.get()));
                }
            }
        }
        mStillFrameCnt++;
#endif

        //for exif callback, we'll post it immediately for performance
        {
            sp<MsgData> msg = new MsgData;
            MsgData* ptr    = msg.get();
            ptr->msgType    = CAMERA_MSG_COMPRESSED_IMAGE;
            ptr->index      = index;
            ptr->flag       = MsgData::POST_IMMEDIATELY;
            ptr->data       = exif_cammemt;
            mMsg->postMsg(msg);
        }
        if(rls_exif_cammemt){
            exif_cammemt->release(exif_cammemt);
        }
        //explicitly rls exif mem.
        exif_buf.clear();
    }
    else
    {
        mExifQueLock.unlock();
    }

    usleep(1000);
    return NO_ERROR;
}

status_t CameraHardware::__stopExifThread()
{
    //don't hold the lock while waiting for the thread to quit
    sp<ExifThread> exifThread;
    {
        //scope for the lock
        Mutex::Autolock lock(mLock);
        exifThread = mExifThread;
    }

    if (exifThread != NULL)
    {
        TRACE_D("%s(pid=%d): request exif thread to exit",
                __FUNCTION__, gettid());
        exifThread->requestExitAndWait();
    }

    {
        Mutex::Autolock lock(mLock);
        mExifThread.clear();
    }
    return NO_ERROR;
}

status_t CameraHardware::takePicture()
{
    FUNC_TAG_E;
    cancelPicture();
    mLock.lock();
    if (mPictureThread != 0) {
        TRACE_E("previous mPictureThread not clear");
        mLock.unlock();
        return INVALID_OPERATION;
    }


    if(mRecordingEnabled == true) {
        TRACE_D("snapshot during recording begin");
        mLock.unlock();
        return NO_ERROR;
    }

    mLock.unlock();

    if(CAM_STATE_IDLE == mState){
#if 1
        //If takepicture is invoked at idle status
        if (NO_ERROR != RegisterPreviewBuffers()){
            TRACE_E("Fail to RegisterPreviewBuffers.");
            return -1;
        }
        mBuffersLock.lock();
        const int cnt=mPreviewImageBuf.size();
        for (int i = 0; i<cnt; i++) {
            sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(i);
            if(buf->isFree()){
                status_t stat=mCamPtr->enqPreviewBuffer(buf);
                if(NO_ERROR == stat)
                    buf->toEngine();
                else
                    TRACE_E("enqPreviewBuffer fail!");
            }
        }
        mBuffersLock.unlock();
#endif
        if (NO_ERROR != mCamPtr->startPreview()){
            TRACE_E("Fail to startPreview.");
            return -1;
        }
    }
    else if(CAM_STATE_PREVIEW == mState){
        /*
         * If takepicture is invoked at preview status.
         * DONOT stop ce preview, switch to still caputre mode directly,
         * BUT we do need to stop the camera-hal preview thread.
         * all preview bufs are flushed out.
         * Check the preview thread status on next StartPreview() calling
         */
        __stopPreviewThread();//XXX

        //enqueue preview buffer for snapshot
        mBuffersLock.lock();
        //const int cnt=mPreviewImageBuf.size();
        const int cnt = 1; //cnt is burstcapture value
        for (int i = 0; i< cnt; i++) {
            sp<CamBuf>& buf = mPreviewImageBuf.editItemAt(i);
            if(buf->isFree()){
                status_t stat=mCamPtr->enqPreviewBuffer(buf);
                if(NO_ERROR == stat)
                    buf->toEngine();
                else
                    TRACE_E("enqPreviewBuffer fail!");
            }
        }
        mBuffersLock.unlock();

    }
    else{
        TRACE_E("Invalid state, mState=%d",mState);
        return UNKNOWN_ERROR;
    }

    //avoid out-of-memory
    mStillImageBuf.clear();

    EngBufReq bufreq = mCamPtr->getStillBufReq();

    mStillImageBuf = BufferHolder::allocBuffers(
            bufreq,
            kStillBufCnt,
            PMEM_DEVICE
            );

    BufferHolder::showState(mStillImageBuf);
    BufferHolder::showInfo(mStillImageBuf);

    //register still buffer
    mBuffersLock.lock();
    const int cnt=mStillImageBuf.size();
    for (int i = 0; i<cnt; i++) {
        sp<CamBuf> buf = mStillImageBuf.editItemAt(i);
        if(buf->isFree()){
            status_t stat=mCamPtr->enqStillBuffer(buf);
            if(NO_ERROR == stat)
                buf->toEngine();
            else
                TRACE_E("enqStillBuffer fail!");
        }
    }
    mBuffersLock.unlock();
    if (NO_ERROR != mCamPtr->startCapture()){
        TRACE_E("Fail to startCapture.");
        return -1;
    }

    /*
     * switch sensor from preview to capture mode,
     * prepare for capture thread
     */

    mPictureThread = new PictureThread(this);
    if(mPictureThread == NULL){
        TRACE_E("%s:fail to create picturethread",__FUNCTION__);
        return -1;
    }
    mState = CAM_STATE_STILL;
    return NO_ERROR;
}

status_t CameraHardware::cancelPicture()
{
    FUNC_TAG_E;
    sp<PictureThread> pictureThread;
    {//scope for the lock
        Mutex::Autolock lock(mLock);
        pictureThread = mPictureThread;
    }

    //don't hold the lock while waiting for the thread to quit
    if (pictureThread != NULL) {
        TRACE_D("%s(pid=%d): request picture thread to exit",
                __FUNCTION__, gettid());
        pictureThread->requestExitAndWait();
    }

    {
        Mutex::Autolock lock(mLock);
        mPictureThread.clear();
    }

    FUNC_TAG_X;
    return NO_ERROR;
}

int CameraHardware::setParameters(const char* parameters)
{
    CameraParameters params;
    String8 str_params(parameters);
    params.unflatten(str_params);

    return setParameters(params);
}

//NOTES: setparameters may not be called during some camera usage.
int CameraHardware::setParameters(const CameraParameters& params)
{
    FUNC_TAG;
    Mutex::Autolock lock(mLock);
    //TRACE_D("%s:(%s)",__FUNCTION__,params.flatten().string());
    params.dump();
    return mCamPtr->setParameters(params);
}

char* CameraHardware::getParameters()
{
    FUNC_TAG;
    String8 params_str8;
    char* params_string;

    Mutex::Autolock lock(mLock);
    CameraParameters params = mCamPtr->getParameters();
    params_str8 = params.flatten();

    // camera service frees this string...
    params_string = (char*) malloc(sizeof(char) * (params_str8.length()+1));
    strcpy(params_string, params_str8.string());

    //TRACE_D("%s:(%s)",__FUNCTION__,params_string);
    params.dump();
    return params_string;
}

void CameraHardware::putParameters(char *parms)
{
    free(parms);
}

int CameraHardware::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    FUNC_TAG;
    status_t ret = NO_ERROR;

    if(!previewEnabled())
    {
        TRACE_E("%s:preview is not enabled!", __FUNCTION__);
        ret = BAD_VALUE;
    }

    if(NO_ERROR == ret){
        CameraParameters params = mCamPtr->getParameters();
        switch(cmd){
            case CAMERA_CMD_START_FACE_DETECTION:
                TRACE_D("%s:Start Face Detection",__FUNCTION__);
                ret = mCamPtr->startFaceDetection();
                break;
            case CAMERA_CMD_STOP_FACE_DETECTION:
                TRACE_D("%s:Stop Face Detection",__FUNCTION__);
                ret = mCamPtr->stopFaceDetection();
                //after stopFaceDetection, remove all FD msg in msgQueue
                if(mMsg != NULL)
                    mMsg->removeMsg(CAMERA_MSG_PREVIEW_METADATA);
                break;
            default:
                ret = BAD_VALUE;
                break;
        }
    }
    return ret;
}

//XXX
int CameraHardware::dump(int fd)
{
    FUNC_TAG;
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    AutoMutex lock(&mLock);
    write(fd, result.string(), result.size());
    return NO_ERROR;
}


void CameraHardware::release()
{
    FUNC_TAG_E;

    cancelAutoFocus();
    cancelPicture();
    stopPreview();
    __stopExifThread();
    FUNC_TAG_X;
}

status_t CameraHardware::getExifImage(const sp<CamBuf>& jpeg, const CameraSetting& setting,
        sp<CamBuf>& output)
{
    status_t ret = NO_ERROR;

    const EngBuf& engbuf = jpeg->getEngBuf();
    CAM_ImageBuffer* pImgBuf_still = const_cast<CAM_ImageBuffer*>(engbuf.getCAM_ImageBufferPtr());

#ifdef __EXIF_GENERATOR__
    ret = ExifGenerator(pImgBuf_still, setting, output);
#else//__EXIF_GENERATOR__
    output.clear();
    ret = -1;
#endif//__EXIF_GENERATOR__
    return ret;
}


status_t CameraHardware::deliverData(int32_t msgType, sp<CamBuf> buf, Vector<sp<CamBuf> > imagebuf){
    TRACE_V("%s: msgType=%x, buf key=%p", __FUNCTION__, msgType, buf->getKey());

    status_t ret = NO_ERROR;
    if(msgType == CAMERA_MSG_VIDEO_FRAME){
        //const nsecs_t timestamp = systemTime();
        const nsecs_t timestamp = buf->getEngBuf().getTimeStamp();
        if(buf->getType() == CamBuf::BUF_TYPE_CAMERAMEMT){
            TRACE_V("%s:callbacktimestamp camera_memory_t* buffer",__FUNCTION__);
            const sp<BufferHolder>& imgbuf = buf->getOwner();
            camera_memory_t* mem = imgbuf->getCamMemT();
            int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf, buf->getKey());
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->timestamp  = timestamp;
                ptr->index      = index;
                ptr->data       = mem;
                mMsg->postMsg(msg);
            }
            ret = NO_ERROR;
        }
        else if(buf->getType() == CamBuf::BUF_TYPE_PMEM){
            TRACE_V("%s:callbacktimestamp sp<IMemory>* buffer",__FUNCTION__);
            sp<IMemory> mem = buf->getIMemory();
            int index = -1; //XXX:hacking path
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->timestamp  = timestamp;
                ptr->index      = index;
                ptr->data       = reinterpret_cast<camera_memory_t*>(&mem);
                mMsg->postMsg(msg);
            }
            ret = NO_ERROR;
        }
        else if(buf->getType() == CamBuf::BUF_TYPE_NATIVEWINDOW){
#if 1
            camera_memory_t* mem = buf->get_cammem_ANW_wrap();
            int index = 0;
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->timestamp  = timestamp;
                ptr->index      = index;
                ptr->data       = mem;
                mMsg->postMsg(msg);
            }
            ret = NO_ERROR;
#else

            camera_memory_t* videobuf = mGetCameraMemory(-1, 4, 1, NULL);
            if( (NULL == videobuf) || ( NULL == videobuf->data)) {
                TRACE_E("Fail to getcameramemory");
                ret = -1;
            }
            else{
                videobuf->data = buf->getVAddr();
                int index = 0;
                {
                    sp<MsgData> msg = new MsgData;
                    MsgData* ptr    = msg.get();
                    ptr->msgType    = msgType;
                    ptr->timestamp  = timestamp;
                    ptr->index      = index;
                    ptr->data       = videobuf;
                    ptr->setListener(msgDone_Cb, videobuf);
                    mMsg->postMsg(msg);
                }

                videobuf->release(videobuf);
                ret = NO_ERROR;
            }
#endif
        }
        else{
            TRACE_E("Invalid buffer type, skip CAMERA_MSG_VIDEO_FRAME callback");
            ret = -1;
        }
    }
    else if(msgType == CAMERA_MSG_PREVIEW_FRAME){
        if(buf->getType() == CamBuf::BUF_TYPE_CAMERAMEMT){
            TRACE_V("%s:callback camera_memory_t* buffer",__FUNCTION__);
            const sp<BufferHolder>& imgbuf = buf->getOwner();
            camera_memory_t* mem = const_cast<camera_memory_t*>(imgbuf->getCamMemT());
            int index = BufferHolder::getQueueIndexByKey(mPreviewImageBuf,buf->getKey());
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->index      = index;
                ptr->data       = mem;
                ptr->setFrameListener(this, buf->getKey());
                mBuffersLock.lock();
                buf->toClient();
                mBuffersLock.unlock();
                mMsg->postMsg(msg);
            }

            //NOTES: DONT need to release wrapped ANW-memory_t memory!
            //BufferHolder will take care of it.
        }
        else if(buf->getType() == CamBuf::BUF_TYPE_PMEM){
            TRACE_V("%s:callback sp<IMemory>* buffer",__FUNCTION__);
            sp<IMemory> mem = buf->getIMemory();
            int index = -1; //XXX:hacking path
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->index      = index;
                ptr->data       = reinterpret_cast<camera_memory_t*>(mem.get());
                ptr->setFrameListener(this, buf->getKey());
                mBuffersLock.lock();
                buf->toClient();
                mBuffersLock.unlock();
                mMsg->postMsg(msg);
            }
        }
        else if(buf->getType() == CamBuf::BUF_TYPE_NATIVEWINDOW){
            camera_memory_t* mem = buf->get_cammem_ANW_wrap();
            int index = 0;
            {
                sp<MsgData> msg = new MsgData;
                MsgData* ptr    = msg.get();
                ptr->msgType    = msgType;
                ptr->index      = index;
                ptr->data       = mem;
                ptr->setFrameListener(this, buf->getKey());
                mBuffersLock.lock();
                buf->toClient();
                mBuffersLock.unlock();
                mMsg->postMsg(msg);
            }

            //NOTES: DONT need to release wrapped ANW-memory_t memory!
        }
        else{
            TRACE_E("Invalid buffer type, skip type %x callback",msgType);
            ret = -1;
        }
    }
    return ret;
}

/*
 * -----------------------------------------------------------
 */

extern int HAL_getNumberOfCameras()
{
    FUNC_TAG;
    char value[PROPERTY_VALUE_MAX];
#ifdef __FAKE_CAM_AVAILABLE__
    /*
     * if set to nonzero, then enable fake camera, for debug purpose only.
     */
    //DEBUG property: persist.service.camera.fake,
    //if set to 0, do not use fake camera
    //else, use fake camera
    //default: 0;
    property_get(PROP_FAKE, value, "0");
    mEnableFake = atoi(value);
    TRACE_D("%s:"PROP_FAKE"=%d",__FUNCTION__, mEnableFake);

    if(mEnableFake != 0){
        TRACE_D("%s:Enable fake camera!",__FUNCTION__);
        int fakecnt = FakeCam::getNumberOfCameras();
        return fakecnt;
    }
#endif

    //DEBUG property: persist.service.camera.cnt,
    //if set to "0", do not probe sensor, return 0;
    //if set to positive, probe sensor, return specified value;
    //if set to negative, probe sensor, return probed sensor count;
    //default: -1;
    property_get(PROP_CNT, value, "-1");
    TRACE_D("%s:"PROP_CNT"=%s",__FUNCTION__,value);
    const int dbg_sensorcnt=atoi(value);

    if (dbg_sensorcnt == 0){
        TRACE_D("%s:use fake cnt=%d",__FUNCTION__,dbg_sensorcnt);
        return dbg_sensorcnt;
    }

    // get the camera engine handle
    int sensorcnt = Engine::getNumberOfCameras();

    if (dbg_sensorcnt > 0){
        TRACE_D("%s:actually probed sensor cnt=%d; but use fake sensor cnt=%d",
                __FUNCTION__,sensorcnt,dbg_sensorcnt);
        return dbg_sensorcnt;
    }

    TRACE_D("%s:use actually probed sensor cnt=%d",__FUNCTION__,sensorcnt);
    return sensorcnt;
}

extern void HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo)
{
    FUNC_TAG;
#ifdef __FAKE_CAM_AVAILABLE__
    if(mEnableFake != 0){
        FakeCam::getCameraInfo(cameraId, cameraInfo);
    }
    else
#endif
    {
        Engine::getCameraInfo(cameraId, cameraInfo);
    }
}

/*{{{*/


#if 0
//froyo version

CameraHardware* CameraHardware::singleton;

/*static*/
CameraHardware* CameraHardware::createInstance()
{
    FUNC_TAG;
    if (singleton != 0) {
        CameraHardware* hardware = singleton;
        return hardware;
    }
    CameraHardware* hardware(new CameraHardware());
    singleton = hardware;
    return hardware;
}

extern "C" CameraHardware* openCameraHardware()
{
    FUNC_TAG;
    return CameraHardware::createInstance();
}
#endif

#if 0
//gingerbread version

CameraHardware* CameraHardware::multiInstance[MAX_CAMERAS_SUPPORTED];

/*static*/
CameraHardware* CameraHardware::createInstance(int cameraId)
{
    FUNC_TAG;
    TRACE_D("%s:cameraId=%d",__FUNCTION__,cameraId);
    if( MAX_CAMERAS_SUPPORTED<=cameraId )
    {
        TRACE_E("only suppport %d cameras, required %d",
                MAX_CAMERAS_SUPPORTED,cameraId);
        return NULL;
    }
    if (multiInstance[cameraId] != 0) {
        CameraHardware* hardware = multiInstance[cameraId];
        return hardware;
    }
    CameraHardware* hardware = new CameraHardware(cameraId);
    multiInstance[cameraId] = hardware;
    return hardware;
}


/*static*/
void CameraHardware::destroyInstance(int cameraId)
{
    TRACE_D("%s:cameraId=%d",__FUNCTION__,cameraId);
    if(multiInstance[cameraId]){
        multiInstance[cameraId]->release();
        delete multiInstance[cameraId];
    }
    else{
        TRACE_E("%s:camerainstace %d already destroyed",__FUNCTION__, cameraId);
    }
    multiInstance[cameraId] = NULL;
}

extern CameraHardware* HAL_openCameraHardware(int cameraId)
{
    TRACE_D("%s:cameraId=%d",__FUNCTION__,cameraId);
    return CameraHardware::createInstance(cameraId);
}

extern void HAL_closeCameraHardware(int cameraId)
{
    TRACE_D("%s:cameraId=%d",__FUNCTION__,cameraId);
    CameraHardware::destroyInstance(cameraId);
}
#endif
/*}}}*/

}; // namespace android

