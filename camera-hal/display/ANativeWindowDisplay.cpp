
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <utils/Log.h>

#include "ImageBuf.h"
#include "gc_gralloc_priv.h"

#define LOG_TAG "CameraDisplay"
#include "ANativeWindowDisplay.h"
#include "CameraSetting.h"

namespace android {

#define LIKELY(exp)   __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

#define CAMHAL_GRALLOC_USAGE \
    (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_PRIVATE_3)

ANativeWindowDisplay::ANativeWindowDisplay(ANWHandleListener* listener)
{
    FUNC_TAG;

    mInitialized = false;
    mStart = false;
    mPause = false;
    mANativeWindow = NULL;
    mANWHandleListener = listener;
    mNativeBufCnt = 0;
    {
        _clearAllQueues();
    }
}

ANativeWindowDisplay::~ANativeWindowDisplay()
{
    FUNC_TAG;
    {
        Mutex::Autolock autoLock2(mLock);
        mStart = false;
        Mutex::Autolock autoLock1(mReceiveLock);
        mFrameAvailableCondition.signal();
    }
    stop();
    mWindowFrameInDisplay.clear();
    mFramesReceived.clear();

    if(mANativeWindow != NULL){
        TRACE_D("%s:clean mANativeWindow",__FUNCTION__);
        _clearCurrentWindow();
        mANativeWindow = NULL;
        _clearAllQueues();
    }
    mInitialized = false;
}

//for debug purpose only.
//hold mLock before calling inside!
void ANativeWindowDisplay::showQueueInfo()
{
    FUNC_TAG;

    bool mlock_locked_inside = false;
    if (mLock.tryLock() == NO_ERROR) {
        TRACE_D("mLock was not locked before calling inside");
        mlock_locked_inside = true;
    }

    bool mreceivelock_locked_inside = false;
    if (mReceiveLock.tryLock() == NO_ERROR){
        TRACE_D("mReceiveLock was not locked before calling inside");
        mreceivelock_locked_inside = true;
    }


    int cnt=0;

    TRACE_D("---------QInfo------------------");
    cnt = mFrameHandle.size();
    TRACE_D("->mFrameHandle: %d", cnt);
    for(int i=0; i<cnt; i++) {
        TRACE_D("-->index=%d, key=%p, value=%p",i,mFrameHandle.keyAt(i),mFrameHandle.valueAt(i));
    }

    cnt = mFrameQueued.size();
    TRACE_D("->mFrameQueued: %d", cnt);
    for(int i=0; i<cnt; i++) {
        TRACE_D("-->index=%d, key=%p, value=%p",i,mFrameQueued.keyAt(i),mFrameQueued.valueAt(i));
    }

    cnt = mFrameDequeued.size();
    TRACE_D("->mFrameDequeued: %d", cnt);
    for(int i=0; i<cnt; i++) {
        TRACE_D("-->index=%d, key=%p, value=%p",i,mFrameDequeued.keyAt(i),mFrameDequeued.valueAt(i));
    }

    cnt = mFramesReceived.size();
    TRACE_D("->mFramesReceived: %d", cnt);
    for(int i=0; i<cnt; i++) {
        TRACE_D("-->index=%d, value=%p",i,mFramesReceived.itemAt(i).get());
    }

    cnt = mWindowFrameInDisplay.size();
    TRACE_D("->mWindowFrameInDisplay: %d", cnt);
    for(int i=0; i<cnt; i++) {
        //NOTES: avoid POD error
        TRACE_D("-->index=%d, key=%p, value=%p",i,mWindowFrameInDisplay.keyAt(i),
                mWindowFrameInDisplay.valueAt(i).get());
    }

    if(mreceivelock_locked_inside){
        mReceiveLock.unlock();
    }

    if(mlock_locked_inside){
        mLock.unlock();
    }
}

int ANativeWindowDisplay::setPreviewWindow(struct preview_stream_ops *window)
{
    TRACE_D("%s:mANativeWindow=%p, window=%p",__FUNCTION__,mANativeWindow,window);

    Mutex::Autolock autoLock2(mLock);

    if(mStart == true && mANativeWindow != window){
        TRACE_E("ANW could not be changed when display aleady started");
        return -1;
    }

    showQueueInfo();
    if(mANativeWindow != NULL){
        if(window == NULL || mANativeWindow != window){
#if 0
            //FIXME:here don't need to clear current window, which has already been disconnect
            //in cameraservice beforehand.
            TRACE_D("%s:clean previous mANativeWindow",__FUNCTION__);
            _clearCurrentWindow();
#endif
            _clearAllQueues();
            mInitialized = false;
        }
    }
    mANativeWindow = window;
    return 0;
}

//hold mLock before calling inside!
void ANativeWindowDisplay::_clearAllQueues(){
    FUNC_TAG;
    showQueueInfo();

    mFrameHandle.clear();
    mFrameQueued.clear();
    mFrameDequeued.clear();
    mWindowFrameInDisplay.clear();
    //XXX: clean all previously received frames.
    mFramesReceived.clear();
}

//hold mLock before calling inside!
void ANativeWindowDisplay::_clearCurrentWindow(){
    FUNC_TAG;
    showQueueInfo();

    if(mANativeWindow != NULL){
        size_t cnt = mFrameDequeued.size();
        for(int i=0;i<(int)cnt;i++){
            buffer_handle_t* handle = mFrameDequeued.keyAt(i);
            _CancelFrame(handle);
        }
        mFrameDequeued.clear();
    }
}

//hold mLock before calling inside!
status_t ANativeWindowDisplay::allocateBuffers(int width, int height, const char* format,
        Vector<sp<BufDesc> >& outputqueue, int anw_bufcnt)
{
    FUNC_TAG;
    {
        Mutex::Autolock autoLock2(mLock);
        if(mStart == true){
            TRACE_E("allocateBuffers should not happen when display aleady started");
            return -1;
        }
    }

    mFrameWidth = width;
    mFrameHeight = height;
    mFrameFormat = String8(format);
    mNativeBufCnt = anw_bufcnt;

    if(mNativeBufCnt <= 0){
        TRACE_E("fatal error, invalid mNativeBufCnt=%d", mNativeBufCnt);
        return -1;
    }
    else{
        TRACE_D("%s: mNativeBufCnt=%d",__FUNCTION__, mNativeBufCnt);
    }

    GraphicBufferMapper& mapper(GraphicBufferMapper::get());
    const Rect rect(0,0,mFrameWidth, mFrameHeight);

    status_t err;
    int i = -1;
    int cancel_start,cancel_end;

    if ( NULL == mANativeWindow ) {
        TRACE_E("No mANativeWindow available, return without allocating buffer");
        return -1;
    }

    // Set gralloc usage bits for window.
    err = mANativeWindow->set_usage(mANativeWindow, CAMHAL_GRALLOC_USAGE);
    if (err != 0) {
        TRACE_E("set_usage failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        return -1;
    }

    TRACE_D("set_buffer_count = %d", mNativeBufCnt);
    ///Set the number of buffers needed for camera preview
    err = mANativeWindow->set_buffer_count(mANativeWindow, mNativeBufCnt);
    if (err != 0) {
        TRACE_E("set_buffer_count failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        return -1;
    }

    if(mFrameHandle.isEmpty() == false){
        TRACE_E("Previous mFrameHandle not cleaned.");
        mFrameHandle.clear();
    }
    mFrameHandle.setCapacity(mNativeBufCnt);
    mFrameDequeued.setCapacity(mNativeBufCnt);
    mFrameQueued.setCapacity(mNativeBufCnt);

    int surface_format=0;
/*    if(strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420P_I420)==0)//I420
        surface_format = HAL_PIXEL_FORMAT_YCbCr_420_P;
    else*/ if(strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420P)==0)//YV12
        surface_format = HAL_PIXEL_FORMAT_YV12;
    else if(strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420SP)==0)//NV21
        surface_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    else if(strcmp(format, CameraParameters::PIXEL_FORMAT_YUV422I)==0)//UYVY
        surface_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
    else if(strcmp(format, CameraParameters::PIXEL_FORMAT_RGBA8888)==0)
        surface_format = HAL_PIXEL_FORMAT_RGBA_8888;
    else if(strcmp(format, CameraParameters::PIXEL_FORMAT_RGB565)==0)
        surface_format = HAL_PIXEL_FORMAT_RGB_565;
    else
        TRACE_E("unsupported format =%s", format);

    TRACE_D("%s:native window use format 0x%x",__FUNCTION__, surface_format);

    int strideX, strideY;
    CameraSetting::getBufferStrideReq(strideX,strideY);
    int alignWidth = _ALIGN(width, strideX);
    int alignHeight = _ALIGN(height, strideY);
    err = mANativeWindow->set_buffers_geometry(
            mANativeWindow,
            alignWidth,
            alignHeight,
            surface_format);

    if (err != 0) {
        TRACE_E("set_buffers_geometry failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        return -1;
    }

    mUndequeuedCnt = 0;
    err = mANativeWindow->get_min_undequeued_buffer_count(mANativeWindow, &mUndequeuedCnt);

    if (err != 0) {
        TRACE_E("get_min_undequeued_buffer_count failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        return -1;
    }
    TRACE_D("%s:get_min_undequeued_buffer_count = %d",__FUNCTION__, mUndequeuedCnt);

    for ( i=0; i < mNativeBufCnt; i++ ) {
        buffer_handle_t* phandle;
        int stride;  // dummy variable to get stride
        err = mANativeWindow->dequeue_buffer(mANativeWindow, &phandle, &stride);

        if (err != 0) {
            TRACE_E("dequeue_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                TRACE_E("Preview surface abandoned!");
                mANativeWindow = NULL;
            }
            cancel_start = 0;
            cancel_end = i;
            goto fail;
        }

        TRACE_D("%s: stride = %d, handle: %p fd = %d",__FUNCTION__,stride, *phandle, ((private_handle_t*)(*phandle))->fd);

        //init key, set valut to invalid 0
        mFrameHandle.add(phandle, (void*)0);
    }

    // lock the initial queueable buffers
    for( i = 0;  i < mNativeBufCnt-mUndequeuedCnt; i++ ) {
        buffer_handle_t* phandle = mFrameHandle.keyAt(i);
        err = mANativeWindow->lock_buffer(mANativeWindow, phandle);
        if (err != NO_ERROR) {
            TRACE_E("lock_buffer failed: %s (%d)", strerror(-err), -err);
            mANativeWindow = NULL;
            cancel_start = 0;
            cancel_end = mNativeBufCnt;
            goto fail;
        }

        void* img = NULL;

        err = mapper.lock(*phandle, CAMHAL_GRALLOC_USAGE, rect, &img);
        if (err != NO_ERROR) {
            TRACE_E("GraphicBufferMapper lock failed: %s  (%d)", strerror(-err), -err);
            cancel_start = 0;
            cancel_end = mNativeBufCnt;
            goto fail;
        }
        TRACE_D("%s: dequeued bufferhandle=%p, img addr=%p", __FUNCTION__,phandle, img);

        mapper.unlock((buffer_handle_t) *phandle);

        mFrameHandle.add(phandle, img);
        mFrameDequeued.add(phandle, img);
    }

    // return the rest of the buffers back to ANativeWindow
    for(i = (mNativeBufCnt-1); i >= mNativeBufCnt - mUndequeuedCnt; i--)
    {
        buffer_handle_t* phandle = mFrameHandle.keyAt(i);
        err = mANativeWindow->cancel_buffer(mANativeWindow, phandle);
        if (err != 0) {
            TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                TRACE_E("Preview surface abandoned!");
                mANativeWindow = NULL;
            }
            cancel_start = 0;
            cancel_end = i+1;
            goto fail;
        }

        void *img = NULL;
        mapper.lock((buffer_handle_t) *phandle, CAMHAL_GRALLOC_USAGE, rect, &img);
        mapper.unlock((buffer_handle_t) *phandle);
        TRACE_D("%s:  queued bufferhandle=%p, img addr=%p", __FUNCTION__,phandle, img);

        mFrameHandle.add(phandle, img);
        mFrameQueued.add(phandle, img);
    }

    //
#if 0
    {
        int cnt = mFrameDequeued.size();
        for(int i=0; i<cnt; i++)
        {
            buffer_handle_t* phandle = mFrameDequeued.keyAt(i);
            void* img = mFrameDequeued.valueAt(phandle);
            _CancelFrame(phandle);
            mFrameQueued.add(phandle,img)
        }
        mFrameDequeued.clear();

        cnt = mFrameHandle.size();
        for(int i=0; i<cnt; i++)
        {
            sp<DisplayData> frame = new DisplayData(
                    DisplayData::DISPLAY_TYPE_ANW,
                    0,
                    mFrameWidth,
                    mFrameHeight,
                    0,
                    format
                    );
            frame->setListener(mANWHandleListener,phandle);
            mWindowFrameInDisplay.add(phandle, frame);
        }
    }
#else
    //by default, let all buffers hold by ANW
    {
        int cnt = mFrameDequeued.size();
        for(int i=0; i<cnt; i++)
        {
            buffer_handle_t* phandle = mFrameDequeued.keyAt(i);
            void* img = mFrameDequeued.valueAt(i);
            _CancelFrame(phandle);
            mFrameQueued.add(phandle,img);
        }
        mFrameDequeued.clear();
    }

    {
        int cnt = mFrameHandle.size();
        for(int i=0; i<cnt; i++) {
            buffer_handle_t* handle = mFrameHandle.keyAt(i);
            unsigned char* vaddr = (unsigned char*)mFrameHandle.valueAt(i);

            struct ion_fd_data req;
            struct ion_custom_data data;
            struct ion_pxa_region region;
            unsigned long paddr = 0;
            int fd, ret;

            /* fd_pmem stores buffer fd */
            int fd_pmem = ((private_handle_t*)(*handle))->master;
            /* fd stores control fd on /dev/ion */
            fd = ((private_handle_t*)(*handle))->fd;

            memset(&req, 0, sizeof(struct ion_fd_data));
            req.fd = fd_pmem;
            ret = ioctl(fd, ION_IOC_IMPORT, &req);
            if (ret < 0) {
                TRACE_E("Failed to get imagebuf req using ION buffer");
                paddr = 0;
            } else {
                memset(&region, 0, sizeof(struct ion_pxa_region));
                region.handle = req.handle;
                data.cmd = ION_PXA_PHYS;
                data.arg = (unsigned long)&region;
                ret = ioctl(fd, ION_IOC_CUSTOM, &data);
                if (ret < 0) {
                    TRACE_E("Failed to get imagebuf physical address using ION buffer");
                    paddr = 0;
                }
                else {
                    paddr = region.addr;
                }
            }

            sp<BufDesc> desc = new BufDesc();
            desc->setType(BufDesc::BUF_TYPE_NATIVEWINDOW);
            desc->setKey(handle);
            desc->setOwner(NULL);//XXX: let ImageHolder take care of it
            desc->setFormat(mFrameFormat.string());
            desc->setSize(mFrameWidth, mFrameHeight);
            desc->setLen(-1);//XXX: let ImageHolder take care of it
            desc->setPAddr(paddr);
            desc->setVAddr(vaddr);
            desc->setPrivate((void*)fd_pmem);
            //desc->setIMemory(buffer);
            desc->toDisplay();
            outputqueue.add(desc);
            //outputqueue.push_back(desc);
        }
    }

#endif

    mInitialized = true;
    return NO_ERROR;

fail:
    // need to cancel buffers if any were dequeued
    for (int i=cancel_start; i<cancel_end; i++) {
        buffer_handle_t* phandle = mFrameQueued.keyAt(i);
        int err = mANativeWindow->cancel_buffer(mANativeWindow, phandle);
        if (err != 0) {
            TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
        }
        mANativeWindow = NULL;
    }
    TRACE_E("Error occurred, performing cleanup");

    return -1;
}

//DONOT hold mLock before calling inside!
status_t ANativeWindowDisplay::start()
{
    Mutex::Autolock autoLock2(mLock);

    if(mStart && mPause){
        mPause = false;
        TRACE_D("%s:wakeup displaythread from pause",__FUNCTION__);
        mFrameAvailableCondition.signal();

#ifdef __SHOW_FPS__
        mDisplayFrameCnt = 0;
        mDisplayTimer.start();
#endif
        return NO_ERROR;
    }
    if(mStart && !mPause){
        TRACE_D("%s: displaythread already started",__FUNCTION__);
        return NO_ERROR;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&mThread, &attr, _ThreadWrapper, this);
    pthread_attr_destroy(&attr);

    mStart = true;
    mPause = false;

    TRACE_D("%s:start displaythread",__FUNCTION__);
    mFrameAvailableCondition.signal();

#ifdef __SHOW_FPS__
    mDisplayFrameCnt = 0;
    mDisplayTimer.start();
#endif

    return NO_ERROR;
}

//DONOT hold mLock before calling inside!
status_t ANativeWindowDisplay::pause()
{
    FUNC_TAG;
    Mutex::Autolock autoLock2(mLock);
    if(mPause == true){
        TRACE_D("%s: displaythread already paused",__FUNCTION__);
        return NO_ERROR;
    }
    mPause = true;
    Mutex::Autolock autoLock1(mReceiveLock);
    mFrameAvailableCondition.signal();


#ifdef __SHOW_FPS__
    mDisplayTimer.stop();
    mDisplayTimer.showDur("Display Duration:");
    mDisplayTimer.showFPS("Display fps:",mDisplayFrameCnt);
    mDisplayFrameCnt = 0;
#endif

    return NO_ERROR;
}

//it may called multi times
//DONOT hold mLock before calling inside!
status_t ANativeWindowDisplay::stop()
{
    FUNC_TAG;
    {
        Mutex::Autolock autoLock2(mLock);
        showQueueInfo();
        if(mStart == false){
            TRACE_D("%s: display thread already stopped",__FUNCTION__);
            return NO_ERROR;
        }
        mStart = false;
        mPause = false;
        Mutex::Autolock autoLock1(mReceiveLock);
        mFrameAvailableCondition.signal();
    }

#ifdef __SHOW_FPS__
    mDisplayTimer.stop();
    mDisplayTimer.showDur("Display Duration:");
    mDisplayTimer.showFPS("Display fps:",mDisplayFrameCnt);
    mDisplayFrameCnt = 0;
#endif

    void *dummy;
    pthread_join(mThread, &dummy);
    return NO_ERROR;
}

//static
void* ANativeWindowDisplay::_ThreadWrapper(void* me)
{
    ANativeWindowDisplay* self = static_cast<ANativeWindowDisplay*>(me);
    status_t err = self->_displayThread();
    return (void*) err;
}

void ANativeWindowDisplay::notify_ANWHandleListener(buffer_handle_t* handle)
{
    if(mANWHandleListener){
        TRACE_V("%s: notify %p", __FUNCTION__, handle);
        mANWHandleListener->ANWHandle_notify(handle);
    }
    else{
        TRACE_V("%s:mANWHandleListener not set",__FUNCTION__);
    }
}

//hold mLock before calling inside!
int ANativeWindowDisplay::_processIdle()
{
    //FUNC_TAG_E;
    //showQueueInfo();
    while((int)mFrameQueued.size() > mUndequeuedCnt+1){
        buffer_handle_t* handle=NULL;
        status_t err = _DequeueFrame(&handle);
        if(err != NO_ERROR){
            TRACE_E("_DequeueFrame fail");
            return -1;
        }

        void* ptr = mFrameHandle.valueFor(handle);
        mFrameDequeued.add(handle,ptr);
        mFrameQueued.removeItem(handle);

        //signal ANW handle free
        notify_ANWHandleListener(handle);

        //signal frame display done
        if(mWindowFrameInDisplay.indexOfKey(handle) != NAME_NOT_FOUND){
            mWindowFrameInDisplay.removeItem(handle);
        }
    }
    //showQueueInfo();
    //FUNC_TAG_X;
    return 0;
}

int ANativeWindowDisplay::_displayThread()
{

    while(1)
    {
        //FUNC_TAG;showQueueInfo();

        mReceiveLock.lock();
        //priority of mStart is higher than mPause
        while ((mStart && (mFramesReceived.empty() || mPause))){
            if(mPause){
                TRACE_V("%s: pause",__FUNCTION__);
            }
            else{
                TRACE_V("%s: waiting for incoming frames",__FUNCTION__);
            }
            //TODO:need to call _processIdle() here?
            _processIdle();
            mFrameAvailableCondition.waitRelative(mReceiveLock, 1000000000LL);
        }
        if (mStart != true){
            TRACE_D("%s: exit",__FUNCTION__);
            mReceiveLock.unlock();
            return NO_ERROR;
        }

        sp<DisplayData> frame = *mFramesReceived.begin();
        mFramesReceived.erase(mFramesReceived.begin());
        //DONOT hold mlock when display
        mReceiveLock.unlock();

        //TODO:display one frame;
        //_display should take care of the frame-ownership internally
        //here we DONOT care about it.
        Mutex::Autolock autoLock(mLock);
        if(UNLIKELY(mANativeWindow == NULL)){
            TRACE_D("%s:no mANativeWindow available, exit",__FUNCTION__);
            return -1;
        }
        _display(frame);

#ifdef __SHOW_FPS__
        mDisplayFrameCnt++;
#endif
        TRACE_V("%s:post one frame done, %p",__FUNCTION__, frame.get());

        _processIdle();
    }

}

//return error means display component will not take over the ownership,
//the caller should take care of it
status_t ANativeWindowDisplay::msgDisplay(sp<DisplayData>& frame){
    if(frame.get() == NULL){
        TRACE_E("Invalid frame for display, return");
        return -1;
    }

    if( frame->mType != DisplayData::DISPLAY_TYPE_COPY &&
        frame->mType != frame->DisplayData::DISPLAY_TYPE_SWAPPING &&
        frame->mType != frame->DisplayData::DISPLAY_TYPE_ANW)
    {
        TRACE_E("Invalid buffer type %d", frame->mType);
        return -1;
    }
    TRACE_V("%s:frame = %p, type = %d",__FUNCTION__, frame.get(), frame->mType);

    {
        Mutex::Autolock autoLock(mLock);
        if(mStart == false || mPause == true)
        {
            TRACE_D("%s:display thread stopped or paused, mStart=%d, mPause=%d",
                    __FUNCTION__, mStart==false? 0:1, mPause==true? 1:0);
#if 1   //ignore frames directly
            return -1;
#else   //push to queue and delay return frame
            ;
#endif
        }
    }

    {
        Mutex::Autolock autoLock(mReceiveLock);
        mFramesReceived.push_back(frame);
        mFrameAvailableCondition.signal();
    }
    return NO_ERROR;
}


status_t ANativeWindowDisplay::_prepare_for_display(sp<DisplayData>& frame)
{
    if(mANativeWindow == NULL || mInitialized != true){
        TRACE_D("%s:no mANativeWindow available, return without display",__FUNCTION__);
        return -1;
    }
    if(frame.get() == NULL){
        TRACE_D("%s:Invalid frame NULL",__FUNCTION__);
        return -1;
    }
    return NO_ERROR;
}

//MAIN display branch
//no lock protect
//hold mLock before calling inside!
//for any failure, the input frame ownership will be rls directly!
status_t ANativeWindowDisplay::_display(sp<DisplayData>& frame)
{
    TRACE_V("%s e",__FUNCTION__);

    status_t ret = NO_ERROR;

    //check whether current native window support display such frame image
    //the input frame will be rls directly if any failure
    ret = _prepare_for_display(frame);
    if(ret != NO_ERROR){
        TRACE_D("%s:prepare_for_display failed, return without display",__FUNCTION__);
        return ret;
    }

    //parse all input buffer type
    //for all display branches, if any error returned, the frame COULD (but not SHOULD) be rls directly!
    switch(frame->mType)
    {
        case DisplayData::DISPLAY_TYPE_COPY:
            //for external mem, copy and post, and the external frame resource COULD be
            //returned here directly no matter success or fail.
            ret = _display(frame->mHandle, frame->mLen);

//whether delay return external buffer
//#define __DELAY_RETURN_EXTERNAL_BUFFER__
#ifdef __DELAY_RETURN_EXTERNAL_BUFFER__
            if(ret != NO_ERROR){
                TRACE_E("display frame failed. %p", (void*)frame.get());
            }
            else{
                //delay return buffers, could be used to simulate the internal nativewindow buffer case
                //FIXME:assume the new display handler always put at end of queue
                mWindowFrameInDisplay.add(
                        mFrameQueued.valueAt(mFrameQueued.size()-1),frame );
            }
#endif
            break;
        case DisplayData::DISPLAY_TYPE_SWAPPING:
            //now in buffer swapping progress, mFrameQueued.size() >= 4 > mUndequeuedCnt >= 3
            //dequeue the last buffer reserved for mix buffer previewing from ANativeWindow
            //then copy (pmem buf -> reserved anw buf) and post
           {
                void* ptr=NULL;
                buffer_handle_t* handle=NULL;
                status_t err = _DequeueFrame(&handle);
                if(err != NO_ERROR){
                    TRACE_E("_DequeueFrame fail");
                    return -1;
                }

                if(mWindowFrameInDisplay.indexOfKey(handle) != NAME_NOT_FOUND){
                    mWindowFrameInDisplay.removeItem(handle);
                }

                ptr = mFrameHandle.valueFor(handle);
                memcpy(ptr, frame->mHandle, frame->mLen);

                if(_QueueFrame(handle) != NO_ERROR){
                    TRACE_E("_QueueFrame fail, discard swapping frame, move current frame to mFrameDequeued");
                    mFrameQueued.removeItem(handle);
                    mFrameDequeued.add(handle,ptr);
                }
           }
            break;
        case DisplayData::DISPLAY_TYPE_ANW:
            //for ANW bufferhandler, the buffer COULD NOT be rls if success;
            //if failed, we COULD rls it directly.
            ret = _display(static_cast<buffer_handle_t*>(const_cast<void*>(frame->mHandle)));
            if(ret != NO_ERROR){
                TRACE_E("display frame failed. %p", (void*)frame.get());
            }
            else{
                //need to hold until this frame's display completed,
                //the free will only happen in ANW Dequeue process
                mWindowFrameInDisplay.add(
                        static_cast<buffer_handle_t*>(const_cast<void*>(frame->mHandle)),frame );
            }

            break;
        default:
            ret = -1;
            break;
    }

    TRACE_V("%s x",__FUNCTION__);
    return ret;
}

//one display branch
//ANW will take ownership of the frame if return success,
//or else, caller should take care of it
//hold mLock before calling inside!
status_t ANativeWindowDisplay::_display(buffer_handle_t* handle)
{
    //check whether it's in recorded list
    ssize_t index = mFrameHandle.indexOfKey(handle);
    if(index == NAME_NOT_FOUND){
        TRACE_E("One unrecognized bufferhandle queued for display, %p", handle);
        showQueueInfo();
        return -1;
    }
    else{
        TRACE_V("%s:existed bufferhandle found, %p",__FUNCTION__, handle);
    }

    status_t err;
    //such function call should be taken as success always, even if any error was returned.
    err=_QueueFrame(handle);

    mFrameQueued.add(handle,mFrameDequeued.valueFor(handle));
    mFrameDequeued.removeItem(handle);

    if(err != NO_ERROR){
        TRACE_E("frame %p could not display correctly", handle);
    }

    return NO_ERROR;
}

//one display branch, display external raw buffer
//no lock protect
//hold mLock before calling inside!
status_t ANativeWindowDisplay::_display(const void* buffer, int len)
{
    int cnt = mFrameDequeued.size();
    if(cnt > 0){
        TRACE_V("%s:Using handle from mFrameDequeued",__FUNCTION__);
    }
    else{
        TRACE_V("%s:Dequeue new handle from ANativeWindow",__FUNCTION__);
#if 0
        buffer_handle_t* handle;
        status_t err;
        err = _DequeueFrame(&handle);
        if(err != NO_ERROR){
            TRACE_E("_DequeueFrame fail");
            return -1;
        }

        void* ptr = mFrameHandle.valueFor(handle);
        mFrameDequeued.add(handle,ptr);
        mFrameQueued.removeItem(handle);
#else
        _processIdle();
#endif

    }
    {
        buffer_handle_t* handle = static_cast<buffer_handle_t*>(mFrameDequeued.keyAt(0));
        void* dst = mFrameDequeued.valueAt(0);
        const void* src = buffer;
        memcpy(dst, src, len);

        status_t err;
        err = _QueueFrame(handle);

        mFrameQueued.add(handle,dst);
        mFrameDequeued.removeItemsAt(0);

        if(err != NO_ERROR){
            TRACE_E("_QueueFrame fail");
        }
    }

    return NO_ERROR;

}

//if queue buffer failed, simply cancel buffer.
//return success if one ANW buffer dequeued well, otherwise no new ANW available.
//mFrameDequeued, mFrameQueued, mFrameHandle should not be touched inside.
//no lock protect
//hold mLock before calling inside!
status_t ANativeWindowDisplay::_DequeueFrame(buffer_handle_t** phandle_ret)
{
    TRACE_V("%s",__FUNCTION__);

    status_t err;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    if ( NULL == mANativeWindow || mInitialized != true) {
        return -1;
    }

    buffer_handle_t* phandle;
    int stride;  // dummy variable to get stride
    err = mANativeWindow->dequeue_buffer(mANativeWindow, &phandle, &stride);
    if (err != 0) {
        TRACE_E("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        return -1;
    }

    err = mANativeWindow->lock_buffer(mANativeWindow, phandle);
    if (err != 0) {
        TRACE_E("lockbuffer failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
        else{
            err = mANativeWindow->cancel_buffer(mANativeWindow, phandle);
            if ( ENODEV == err ) {
                TRACE_E("Fatal error: cancel_buffer failed: %s (%d), stop display", strerror(-err), -err);
                mANativeWindow = NULL;
            }
        }
        return -1;
    }

    //check whether it's in recorded list
    ssize_t index = mFrameHandle.indexOfKey(phandle);
    if(index == NAME_NOT_FOUND){
        TRACE_E("One new unrecognized bufferhandle dequeued, %p", phandle);
        showQueueInfo();
        err = mANativeWindow->cancel_buffer(mANativeWindow, phandle);
        if (err != 0) {
            TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                TRACE_E("Preview surface abandoned!");
                mANativeWindow = NULL;
            }
        }
        return -1;
    }
    else{
        TRACE_V("%s:existed bufferhandle found, %p",__FUNCTION__, phandle);
    }

    // lock buffer before sending to FrameProvider for filling
    const Rect rect(0,0,mFrameWidth,mFrameHeight);
    void *img;

    int retry_count = 0;
    while(retry_count++ < MAX_RETRY_COUNT){
        err = mapper.lock((buffer_handle_t) *phandle, CAMHAL_GRALLOC_USAGE, rect, &img);
        if (err != 0){
            TRACE_E("GraphicBufferMapper lock failed: %s (%d), retry=%d",
                    strerror(-err), -err, retry_count);
            usleep(15000);
            continue;
        }
        break;
    }
    if(err != 0){
        TRACE_E("GraphicBufferMapper lock failed: %s (%d), exit", strerror(-err), -err);
        //TODO:whether we need do unlock before cancel_buffer
        mapper.unlock((buffer_handle_t)*phandle);
        err = mANativeWindow->cancel_buffer(mANativeWindow, phandle);
        if (err != 0) {
            TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                TRACE_E("Preview surface abandoned!");
                mANativeWindow = NULL;
            }
        }
        return -1;
    }

    *phandle_ret = phandle;

    TRACE_V("%s:dequeue bufferhandle = %p, vaddr = %p",__FUNCTION__, phandle, img);

    return err;
}


//if queue buffer failed, simply cancel buffer. so it could be taken as success always.
//the return value only stand for "enqueue_buffer" or "cancel_buffer"
//mFrameDequeued, mFrameQueued, mFrameHandle should not be touched inside.
//no lock protect
//hold mLock before calling inside!
status_t ANativeWindowDisplay::_QueueFrame(buffer_handle_t* handle)
{
    TRACE_V("%s: bufferhandle=%p",__FUNCTION__,handle);

    status_t err = NO_ERROR;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    {//handle crop, which may also be used to handle the display zoom:
        int32_t left    = 0;
        int32_t top     = 0;
        int32_t right   = mFrameWidth;
        int32_t bottom  = mFrameHeight;

        err = mANativeWindow->set_crop(mANativeWindow, left, top, right, bottom);
        if(err != NO_ERROR){
            TRACE_E("set_crop failed: %s (%d)", strerror(-err), -err);
        }
    }

    mapper.unlock((buffer_handle_t)*handle);
    err = mANativeWindow->enqueue_buffer(mANativeWindow, handle);
    if (err != 0) {
        TRACE_E("enqueue_buffer failed: %s (%d); cancel_buffer instead",
                strerror(-err), -err);
        err = mANativeWindow->cancel_buffer(mANativeWindow, handle);
        if (err != 0) {
            TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                TRACE_E("Preview surface abandoned!");
                mANativeWindow = NULL;
            }
        }
    }

    return err;
}

//mFrameDequeued, mFrameQueued, mFrameHandle should not be touched inside.
//no lock protect
//hold mLock before calling inside!
status_t ANativeWindowDisplay::_CancelFrame(buffer_handle_t* handle)
{
    TRACE_V("%s: bufferhandle=%p",__FUNCTION__,handle);

    status_t err = NO_ERROR;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    mapper.unlock((buffer_handle_t)*handle);
    err = mANativeWindow->cancel_buffer(mANativeWindow, handle);
    if (err != 0) {
        TRACE_E("cancel_buffer failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            TRACE_E("Preview surface abandoned!");
            mANativeWindow = NULL;
        }
    }

    return err;
}

};
