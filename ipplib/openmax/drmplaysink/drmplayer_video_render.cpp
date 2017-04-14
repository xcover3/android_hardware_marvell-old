#define LOG_TAG "DrmVideoRender"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/List.h>
#include <utils/Vector.h>
#include <utils/RefBase.h>
#include "AwesomePlayer.h"
#include "OMX_IVCommon.h"
#include "IppOmxDrmPlayerExt.h"
#include "OMX_Core.h"

#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IMemory.h>
#include <ui/android_native_buffer.h>
#include <ui/GraphicBufferMapper.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <gralloc_priv.h>
#include <utils/Log.h>
#include <utils/Errors.h>
#include <cutils/properties.h>

//#define PERF
//#define DUMP

#ifdef PERF
// used to record render frame interval
struct timespec t_start_base;
struct timespec t_stop_base;
long render_interval = 0;
long time_tot_base = 0;

int32_t oldDisplayWidth = 0;
int32_t oldDisplayHeight = 0;
int32_t frameCount = 0;

#define GET_TIME_INTERVAL_USEC(A, B) (((((B).tv_sec)-((A).tv_sec))*1000000) + ((((B).tv_nsec)-((A).tv_nsec))/1000))
#define GET_TIME_INTERVAL_NSEC(A, B) (((((B).tv_sec)-((A).tv_sec))*1000000000LL) + ((((B).tv_nsec)-((A).tv_nsec))))
#endif

#ifdef DUMP
#define DUMPCNT 30
int dumpCnt = 0;
int dumpInx = 0;
int seekInx = 0;    //used record the sequence of seek operations
static FILE* fp_dump = NULL;
#endif

using namespace android;

enum GraphicBufferStatus {
    OWNED_BY_RENDER,
    OWNED_BY_NATIVE_WINDOW,
    WAIT_FOR_PROVIDE,       // Graphics buffer have been dequeued, wait for provide to OMX
};

typedef struct AllocatedBufferInfo {
    sp<GraphicBuffer> mGraphicBuffer;
    GraphicBufferStatus mStatus;
}AllocatedBufferInfo;

struct DrmPlayerNativeWindowRenderer : public AwesomeRenderer {
    DrmPlayerNativeWindowRenderer(int32_t rotationDegrees){
        applyRotation(rotationDegrees);
        mDecodedWidth = 0;
        mDecodedHeight = 0;
        mDisplayWidth = 0;
        mDisplayHeight = 0;
        mColorFormat = OMX_COLOR_FormatCbYCrY;
        LOGD("----- 2011.11.17 DrmPlayerNativeWindowRenderer Ver 1.1 -----");
    }

    virtual void render(MediaBuffer *buffer) {
    }

    void render(android_native_buffer_t* buffer){
        if(buffer == NULL){
            LOGE("render failed: bad parameter!");
        }

        android_native_buffer_t* buf = buffer;
        status_t err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf);

#ifdef PERF
        clock_gettime(CLOCK_REALTIME, &t_stop_base);
        render_interval = ((GET_TIME_INTERVAL_USEC(t_start_base, t_stop_base)));
        if(frameCount > 0){
            if((oldDisplayWidth==mDisplayWidth)&&(oldDisplayHeight==mDisplayHeight)){
                LOGD("----- w = %d, h = %d, render interval = %ld us -----", mDisplayWidth, mDisplayHeight, render_interval);
            }else{
                LOGD("----- Resolution changed! w = %d, h = %d, render interval = %ld us -----", mDisplayWidth, mDisplayHeight, render_interval);
            }
            time_tot_base += render_interval;
        }
        oldDisplayWidth = mDisplayWidth;
        oldDisplayHeight = mDisplayHeight;
        t_start_base = t_stop_base;
        frameCount++;
#endif

        if (err != 0) {
            LOGE("queueBuffer failed with error %s (%d)", strerror(-err), -err);
            return;
        }

        for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
            it != mGraphicBuffersAllocated.end(); ++it){
            if(((*it).mGraphicBuffer)->handle == buf->handle){
                (*it).mStatus = OWNED_BY_NATIVE_WINDOW;
                break;
            }
        }
    }

    void render(
        const void *data, size_t size, void *platformPrivate){
    }

    status_t init();
    status_t allocateOutputBuffersFromNativeWindow(int &nBfferCount);
    status_t getGraphicBufferInfo(int nIndex, char ** ppVirAddress, char ** ppPhyAddress, void **pBufferHandle);
    status_t destroyNativeWindow();
    status_t reconfigNativeWindow();
    status_t returnAllGraphicsBuffers(int *pNum);
    status_t getNewGraphicsBuffer(void** pBufferHandle);
    status_t getProvideGraphicsBuffer(void** pBufferHandle);
    status_t configSurface(DRM_CONFIG_SET_SURFACE *pSurfaceSet);

//protected:
    ~DrmPlayerNativeWindowRenderer() {
    }
    int32_t mDecodedWidth, mDecodedHeight;
    int32_t mDisplayWidth, mDisplayHeight;
    OMX_COLOR_FORMATTYPE mColorFormat;
private:
    sp<SurfaceComposerClient> mSurfCC;
    sp<SurfaceControl> mSurfCtrl;
    sp<ANativeWindow> mNativeWindow;
    List<AllocatedBufferInfo> mGraphicBuffersAllocated;

    void applyRotation(int32_t rotationDegrees) {
        uint32_t transform;
        switch (rotationDegrees) {
            case 0: transform = 0; break;
            case 90: transform = HAL_TRANSFORM_ROT_90; break;
            case 180: transform = HAL_TRANSFORM_ROT_180; break;
            case 270: transform = HAL_TRANSFORM_ROT_270; break;
            default: transform = 0; break;
        }

        if (transform) {
            CHECK_EQ(0, native_window_set_buffers_transform(
                        mNativeWindow.get(), transform));
        }
    }

    DrmPlayerNativeWindowRenderer(const DrmPlayerNativeWindowRenderer &);
    DrmPlayerNativeWindowRenderer &operator=(
            const DrmPlayerNativeWindowRenderer &);
};

status_t DrmPlayerNativeWindowRenderer::init(){
    status_t state;

    mSurfCC = new SurfaceComposerClient;
    if(mSurfCC->initCheck() != NO_ERROR){
        LOGE("#%d - failed to creat surface composer client", __LINE__);
        return -1;
    }

    mSurfCtrl = mSurfCC->createSurface(
                                        getpid(),
                                        0,
                                        mSurfCC->getDisplayWidth(0),//default width
                                        mSurfCC->getDisplayHeight(0),//default height
                                        PIXEL_FORMAT_RGB_565, // ???
                                        ISurfaceComposer::eFXSurfaceNormal);
    if( mSurfCtrl.get() == NULL ) {
        LOGE("#%d - failed to mSurfCC->createSurface(...)", __LINE__);
        return -1;
    }

    state = mSurfCC->openTransaction();
    if (state != NO_ERROR) {
        LOGE("#%d - failed to mSurfCC->openTransaction(...)", __LINE__);
        return -1;
    }

    state = mSurfCtrl->setPosition(0,0);
    if (state != NO_ERROR) {
        LOGE("#%d - failed to mSurfCtrl->setPosition(...)", __LINE__);
        return -1;
    }

    //set native layer Z-order to 21015 to suitable with Netflix apk requairement.
    state = mSurfCtrl->setLayer( 21015 );
    if (state != NO_ERROR) {
        LOGE("#%d - failed to mSurfCtrl->setLayer(...)", __LINE__);
        return -1;
    }

    //do not show native surface until surface is reconfiged.
    /*state =  mSurfCtrl->show();
    if (state != NO_ERROR) {
        LOGE("#%d - failed to mSurfCtrl->show(...)", __LINE__);
        return -1;
    }*/

    state = mSurfCC->closeTransaction();
    if (state != NO_ERROR) {
        LOGE("#%d - failed to mSurfCC->closeTransaction(...)", __LINE__);
        return -1;
    }

    sp<Surface> surface = mSurfCtrl->getSurface();
    if (surface == NULL) {
        LOGE("#%d - failed to mSurfCtrl->getSurface(...)", __LINE__);
        return -1;
    }

    mNativeWindow = surface;
    return OK;
}

status_t DrmPlayerNativeWindowRenderer::allocateOutputBuffersFromNativeWindow(int &nBufferCount){
    int halFmt = 0;
    status_t err;

    if(OMX_COLOR_FormatYUV420Planar == mColorFormat){
        halFmt = HAL_PIXEL_FORMAT_YCbCr_420_P;	// YUV420
    }else if (OMX_COLOR_FormatCbYCrY == mColorFormat){
        halFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
    }

    err = native_window_set_buffers_geometry(
            mNativeWindow.get(),
            mDisplayWidth,
            mDisplayHeight,
            halFmt);

    if (err != 0) {
        LOGE("native_window_set_buffers_geometry failed: %s (%d)",
                strerror(-err), -err);
        return err;
    }

    // Set up the native window.
    OMX_U32 usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_PRIVATE_3;

    usage |= GRALLOC_USAGE_PROTECTED;

    // Make sure to check whether either Stagefright or the video decoder
    // requested protected buffers.
    if (usage & GRALLOC_USAGE_PROTECTED) {
        // Verify that the ANativeWindow sends images directly to
        // SurfaceFlinger.
        int queuesToNativeWindow = 0;
        err = mNativeWindow->query(
                mNativeWindow.get(), NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER,
                &queuesToNativeWindow);
        if (err != 0) {
            LOGE("error authenticating native window: %d", err);
            return err;
        }
        if (queuesToNativeWindow != 1) {
            LOGE("native window could not be authenticated");
            return PERMISSION_DENIED;
        }
    }

    LOGV("native_window_set_usage usage=0x%x", usage);
    err = native_window_set_usage(
            mNativeWindow.get(), usage | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
    if (err != 0) {
        LOGE("native_window_set_usage failed: %s (%d)", strerror(-err), -err);
        return err;
    }

    int minUndequeuedBufs = 0;
    err = mNativeWindow->query(mNativeWindow.get(),
            NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
    if (err != 0) {
        LOGE("NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)",
                strerror(-err), -err);
        return err;
    }

    // XXX: Is this the right logic to use?  It's not clear to me what the OMX
    // buffer counts refer to - how do they account for the renderer holding on
    // to buffers?
    int newBufferCount = nBufferCount + minUndequeuedBufs;

    err = native_window_set_buffer_count(
            mNativeWindow.get(), newBufferCount);
    if (err != 0) {
        LOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err), -err);
        return err;
    }

    nBufferCount = newBufferCount;
    LOGI("allocating %d buffers from a native window on output port", newBufferCount);

    // Dequeue buffers and send them to OMX
    for (int i = 0; i < newBufferCount; i++) {
        android_native_buffer_t* buf;
        err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &buf);
        if (err != 0) {
            LOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
            break;
        }

        sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));

        AllocatedBufferInfo info;
        info.mGraphicBuffer = graphicBuffer;
        info.mStatus = WAIT_FOR_PROVIDE;

        // mGraphicBuffersAllocated record the entries of the buffers have been allocated..
        mGraphicBuffersAllocated.push_back(info);
    }

    OMX_U32 cancelStart;
    OMX_U32 cancelEnd;

    if (err != 0) {
        // If an error occurred while dequeuing we need to cancel any buffers
        // that were dequeued.
        cancelStart = 0;
        cancelEnd = newBufferCount;
    } else {
        // Return the last two/three buffers to the native window.
        cancelStart = newBufferCount - minUndequeuedBufs;
        cancelEnd = newBufferCount;
    }

    unsigned int i=0;
    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
         it != mGraphicBuffersAllocated.end(); ++it,i++){
         if(i>=cancelStart && i < cancelEnd){
            int err = mNativeWindow->cancelBuffer(mNativeWindow.get(), (*it).mGraphicBuffer.get());
            if (err != 0) {
                LOGE("cancelBuffer failed w/ error 0x%08x", err);
                return err;
            }
            (*it).mStatus = OWNED_BY_NATIVE_WINDOW;
         }
    }

    return OK;
}

status_t DrmPlayerNativeWindowRenderer::getGraphicBufferInfo(int nIndex, char ** ppVirAddress, char ** ppPhyAddress, void **pBufferHandle){
    int i = 0;
    status_t state;

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
         it != mGraphicBuffersAllocated.end(); ++it,i++){
         if(i==nIndex){
            sp<GraphicBuffer> graphicBuffer = (*it).mGraphicBuffer;
            void* vaddr;

            android_native_buffer_t* bufHandle = graphicBuffer->getNativeBuffer();
            // return the buffer handle of the graphicBuffer
            *pBufferHandle = bufHandle;

            private_handle_t *priHandle = private_handle_t::dynamicCast(bufHandle->handle);
            if(priHandle == NULL){
                LOGE("getGraphicBufferInfo dynamicCast() failed.");
            }

            unsigned long paddr = priHandle->physAddr;
            unsigned long usage = GRALLOC_USAGE_SW_READ_OFTEN|GRALLOC_USAGE_SW_WRITE_OFTEN;
            state = graphicBuffer->lock(usage, &vaddr);
            if(state){
                LOGE("getGraphicBufferInfo() : lock graphic buffer failed.");
                return state;
            }

            (*ppPhyAddress) = (char*)paddr;
            (*ppVirAddress) = (char*)vaddr;
            return 0;
         }
    }

    LOGE("getGraphicBufferInfo failed.");
    return -1;
}

status_t DrmPlayerNativeWindowRenderer::destroyNativeWindow(){
    int err;

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
        it != mGraphicBuffersAllocated.end(); ++it){

        sp<GraphicBuffer> graphicBuffer = (*it).mGraphicBuffer;
        err = graphicBuffer->unlock();
        if(err){
            LOGE("destroyNativeWindow() : unlock graphic buffer failed.");
            return err;
        }

        if(OWNED_BY_RENDER == (*it).mStatus){
            err = mNativeWindow->cancelBuffer(mNativeWindow.get(), graphicBuffer.get());
            if (err != 0) {
                LOGE("cancelBuffer failed w/ error 0x%08x", err);
                return err;
            }
            (*it).mStatus = OWNED_BY_NATIVE_WINDOW;
        }
    }

    mGraphicBuffersAllocated.clear();
    mSurfCC->dispose();
    mSurfCC.clear();
    mSurfCtrl.clear();
    mNativeWindow.clear();

    return OK;
}

status_t DrmPlayerNativeWindowRenderer::reconfigNativeWindow(){
    int err;

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
        it != mGraphicBuffersAllocated.end(); ++it){

        sp<GraphicBuffer> graphicBuffer = (*it).mGraphicBuffer;
        err = graphicBuffer->unlock();
        if(err){
            LOGE("reconfigNativeWindow() : unlock graphic buffer failed.");
            return err;
        }

        if(OWNED_BY_RENDER == (*it).mStatus){
            err = mNativeWindow->cancelBuffer(mNativeWindow.get(), graphicBuffer.get());
            if (err != 0) {
                LOGE("cancelBuffer failed w/ error 0x%08x", err);
                return err;
            }
            (*it).mStatus = OWNED_BY_NATIVE_WINDOW;
        }
    }

    mGraphicBuffersAllocated.clear();

    return OK;
}

status_t DrmPlayerNativeWindowRenderer::returnAllGraphicsBuffers(int *pNum){
    int err;
    int Count = 0;

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
        it != mGraphicBuffersAllocated.end(); ++it){
        if(OWNED_BY_RENDER == (*it).mStatus){
            (*it).mStatus = WAIT_FOR_PROVIDE;
            Count++;
        }
    }
    *pNum = Count;
    LOGD("Total %d buffers wait for provide.", Count);
    return OK;
}

status_t DrmPlayerNativeWindowRenderer::getNewGraphicsBuffer(void** pBufferHandle){
    int err;
    android_native_buffer_t* newBuf = NULL;

    err = mNativeWindow->dequeueBuffer(mNativeWindow.get(), &newBuf);
    if (err != 0) {
        LOGE("dequeueBuffer failed w/ error 0x%08x", err);
        return err;
    }

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
         it != mGraphicBuffersAllocated.end(); ++it){
        if(((*it).mGraphicBuffer)->handle == newBuf->handle){
            (*it).mStatus = OWNED_BY_RENDER;
            break;
        }
    }

    err = mNativeWindow->lockBuffer(mNativeWindow.get(), newBuf);

    if (err != 0) {
        LOGE("lockBuffer failed w/ error 0x%08x", err);
        return err;
    }

    (*pBufferHandle) = newBuf;
    return OK;
}

status_t DrmPlayerNativeWindowRenderer::getProvideGraphicsBuffer(void** pBufferHandle){
    int err;
    android_native_buffer_t* provideBuf = NULL;

    for (List<AllocatedBufferInfo>::iterator it = mGraphicBuffersAllocated.begin();
         it != mGraphicBuffersAllocated.end(); ++it){
        if(WAIT_FOR_PROVIDE == (*it).mStatus){
            (*it).mStatus = OWNED_BY_RENDER;
            provideBuf = (*it).mGraphicBuffer->getNativeBuffer();
            break;
        }
    }

    if(provideBuf == NULL){
        LOGE("getProvideGraphicsBuffer failed, no GraphicsBuffer is owned by render.");
        return -1;
    }

    err = mNativeWindow->lockBuffer(mNativeWindow.get(), provideBuf);
    if (err != 0) {
        LOGE("lockBuffer failed w/ error 0x%08x", err);
        return err;
    }

    (*pBufferHandle) = provideBuf;
    return OK;
}


status_t DrmPlayerNativeWindowRenderer::configSurface(DRM_CONFIG_SET_SURFACE *pSurfaceSet){
    int err = NO_ERROR;

    err = mSurfCC->openTransaction();
    if (err != NO_ERROR) {
        LOGE("openTransaction failed w/ error 0x%08x", err);
        return err;
    }

    err = mSurfCtrl->setPosition(pSurfaceSet->displayX, pSurfaceSet->displayY);
    if (err != NO_ERROR) {
        LOGE("setPosition failed w/ error 0x%08x", err);
        return err;
    }

    err = mSurfCtrl->setSize(pSurfaceSet->displayWidth, pSurfaceSet->displayHeight);
    if (err != NO_ERROR) {
        LOGE("setSize failed w/ error 0x%08x", err);
        return err;
    }

    err = mSurfCtrl->show();
    if (err != NO_ERROR) {
        LOGE("show surface failed w/ error 0x%08x", err);
        return err;
    }

    err = mSurfCC->closeTransaction();
    if (err != NO_ERROR) {
        LOGE("closeTransaction failed w/ error 0x%08x", err);
        return err;
    }

    return NO_ERROR;
}

extern "C"  void *drmplayer_videorender_init(void *pRenderHandle){
    DrmPlayerNativeWindowRenderer *handle;

#ifdef PERF
    time_tot_base = 0;
    frameCount = 0;
#endif
    handle = new DrmPlayerNativeWindowRenderer(0);
    if (handle == NULL){
        LOGE("drmplayer_videorender_init() : failed at create render");
        return NULL;
    }

    if(handle->init()){
        LOGE("drmplayer_videorender_init() : failed at init render");
        return NULL;
    }

    return (void *)handle;
}

extern "C" int  drmplayer_videorender_allocateGraphicsBuffer(void *pRenderHandle, int nCount, int *pAllocated, OMX_COLOR_FORMATTYPE eColorFormat, size_t decodedWidth, size_t decodedHeight, size_t displayWidth, size_t displayHeight){
    if(pRenderHandle == NULL){
        LOGE("drmplayer_videorender_allocateGraphicsBuffer() : Bad parameter!");
        return -1;
    }

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    pDrmPlayerRenderHandle->mDecodedWidth = decodedWidth;
    pDrmPlayerRenderHandle->mDecodedHeight = decodedHeight;
    pDrmPlayerRenderHandle->mDisplayWidth = displayWidth;
    pDrmPlayerRenderHandle->mDisplayHeight = displayHeight;
    pDrmPlayerRenderHandle->mColorFormat   = eColorFormat;

    int nBufferCount = nCount;
    if(OK == pDrmPlayerRenderHandle->allocateOutputBuffersFromNativeWindow(nBufferCount)){
        *pAllocated = nBufferCount;
    }else{
        LOGE("drmplayer_videorender_allocateGraphicsBuffer() : allocateOutputBuffersFromNativeWindow failed!");
        return -1;
    }

    return 0;
}

extern "C" int drmplayer_videorender_getGraphicsBufferInfo(void *pRenderHandle, int nIndex, char ** ppVirAddress, char ** ppPhyAddress, void **pBufferHandle){
    if(pRenderHandle == NULL){
        LOGE("drmplayer_videorender_getGraphicsBuffer() : Bad parameter!");
        return -1;
    }

    char * pBufVirAddress=NULL;
    char * pBufPhyAddress=NULL;
    void * pBufHandle=NULL;

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;

    if(pDrmPlayerRenderHandle){
        int err = pDrmPlayerRenderHandle->getGraphicBufferInfo(nIndex, &pBufVirAddress, &pBufPhyAddress, &pBufHandle);

        if(OK == err){
            (*ppVirAddress) = pBufVirAddress;
            (*ppPhyAddress) = pBufPhyAddress;
            (*pBufferHandle) = pBufHandle;
            return 0;
        }
        LOGE("drmplayer_videorender_getGraphicsBufferInfo() : getGraphicBufferInfo failed!");
        return -1;
    }

    return 0;
}

extern "C" int drmplayer_videorender_getNewGraphicsBuffer(void *pRenderHandle, int bufferCount, void * pVideoDecoderOutBufferQ, void **pNewBufferHeader){
    if(pRenderHandle == NULL || pVideoDecoderOutBufferQ == NULL){
        LOGE("drmplayer_videorender_getNewGraphicsBuffer() : Bad parameter!");
        return -1;
    }

    int i;
    void * pBufHandle=NULL;

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    OMX_BUFFERHEADERTYPE** pBufferQ = (OMX_BUFFERHEADERTYPE**)pVideoDecoderOutBufferQ;

    if(pDrmPlayerRenderHandle){
        int err = pDrmPlayerRenderHandle->getNewGraphicsBuffer(&pBufHandle);

        if(err){
            LOGE("drmplayer_videorender_getNewGraphicsBuffer() : getNewGraphicBuffer() failed!");
            return -1;
        }

        for (i=0; i<bufferCount; i++){
            if(pBufferQ[i]==NULL){
                LOGE(" ACCESS NULL POINTER i=%d ", i);
            }
            if(((android_native_buffer_t*)(pBufferQ[i]->pAppPrivate))->handle == ((android_native_buffer_t*)pBufHandle)->handle){
                (*pNewBufferHeader) = pBufferQ[i];
                break;
            }
        }

        if(i == bufferCount){
            LOGE("drmplayer_videorender_getNewGraphicsBuffer() : find new buffer header failed!");
            return -1;
        }
    }

    return 0;
}

extern "C" int drmplayer_videorender_getProvideGraphicsBuffer(void *pRenderHandle, int bufferCount, void * pVideoDecoderOutBufferQ, void **pNewBufferHeader){
    if(pRenderHandle == NULL || pVideoDecoderOutBufferQ == NULL){
        LOGE("drmplayer_videorender_getProvideGraphicsBuffer() : Bad parameter!");
        return -1;
    }

    int i;
    void * pBufHandle=NULL;

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    OMX_BUFFERHEADERTYPE** pBufferQ = (OMX_BUFFERHEADERTYPE**)pVideoDecoderOutBufferQ;

    if(pDrmPlayerRenderHandle){
        int err = pDrmPlayerRenderHandle->getProvideGraphicsBuffer(&pBufHandle);

        if(err){
            LOGE("drmplayer_videorender_getProvideGraphicsBuffer() : getProvideGraphicsBuffer() failed!");
            return -1;
        }

        for (i=0; i<bufferCount; i++){
            if(pBufferQ[i]==NULL){
                LOGE(" ACCESS NULL POINTER i=%d ", i);
            }
            if(((android_native_buffer_t*)(pBufferQ[i]->pAppPrivate))->handle == ((android_native_buffer_t*)pBufHandle)->handle){
                (*pNewBufferHeader) = pBufferQ[i];
                break;
            }
        }

        if(i == bufferCount){
            LOGE("drmplayer_videorender_getNewGraphicsBuffer() : find new buffer header failed!");
            return -1;
        }
    }

    return 0;
}



extern "C" void drmplayer_videorender_getAndroidVersion(char *Version){
    if(Version){
        strcpy(Version, "Android3.2");
    }
}

extern "C" void drmplayer_videorender(void *pRenderHandle, const void *data, size_t size, void *platformPrivate){
    if((pRenderHandle == NULL) || (platformPrivate == NULL)){
        LOGE("drmplayer_videorender() : Bad parameter!");
    }

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    android_native_buffer_t *pBufferHandle = (android_native_buffer_t*)(((OMX_BUFFERHEADERTYPE*)platformPrivate)->pAppPrivate);

#ifdef DUMP
    if(dumpCnt > 0){
        char picName[100];
        sprintf(picName, "/data/dump/Seek%d_Pic%d_%dx%d", seekInx, DUMPCNT-dumpCnt, pDrmPlayerRenderHandle->mDisplayWidth, pDrmPlayerRenderHandle->mDisplayHeight);

        OMX_BUFFERHEADERTYPE* pBufHeader = (OMX_BUFFERHEADERTYPE*)platformPrivate;
        fp_dump= fopen(picName, "wb");
        if(fp_dump){
            fwrite(pBufHeader->pBuffer+pBufHeader->nOffset, 1, pBufHeader->nFilledLen, fp_dump);
            fclose(fp_dump);
            fp_dump = NULL;
        }

        dumpInx++;
        dumpCnt--;
    }
#endif

    if (pDrmPlayerRenderHandle){
        pDrmPlayerRenderHandle->render(pBufferHandle);
    }
}

extern "C" int drmplayer_videorender_configSurface(void *pRenderHandle, void *pSurfaceSet){
    if((pRenderHandle==NULL) || (pSurfaceSet==NULL)){
        LOGE("drmplayer_videorender_configSurface() : Bad parameter!");
        return -1;
    }

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    DRM_CONFIG_SET_SURFACE* newSurfaceConfig = (DRM_CONFIG_SET_SURFACE*)pSurfaceSet;

    int status = pDrmPlayerRenderHandle->configSurface(newSurfaceConfig);
    if(status != NO_ERROR){
        LOGE("drmplayer_videorender_configSurface() : config surface failed.");
        return -1;
    }

    LOGD("native surface has been set to: width = %d, height = %d", (int)(newSurfaceConfig->displayWidth), (int)(newSurfaceConfig->displayHeight));
    return OK;
}

extern "C" void drmplayer_videorender_deinit(void *pRenderHandle){
    if(pRenderHandle == NULL){
        LOGE("drmplayer_videorender_deinit() : Bad parameter!");
        return;
    }

#ifdef PERF
    LOGD("----- Total Time = %ldus, Total Frame Count = %d, Avg Render Interval = %lfus, Fps = %lf -----", time_tot_base, frameCount-1, (double)(time_tot_base)/(frameCount-1), (double)(frameCount-1)*1000000/(time_tot_base));
    time_tot_base = 0;
    frameCount = 0;
#endif

#ifdef DUMP
    int dumpCnt = 0;
    int dumpInx = 0;
    int seekInx = 0;    //used record the sequence of seek operations
#endif

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;

    if (pDrmPlayerRenderHandle){
        if(OK != pDrmPlayerRenderHandle->destroyNativeWindow()){
            LOGE("drmplayer_videorender_deinit() : destroyNativeWindow failed!");
            return;
        }

    delete pDrmPlayerRenderHandle;
    }
}

extern "C" void drmplayer_videorender_getDisplayInfo(void *pRenderHandle,OMX_COLOR_FORMATTYPE *pColorFormat, size_t *pDisplayWidth, size_t *pDisplayHeight, size_t *pDecodedWidth, size_t *pDecodedHeight){
    if(pRenderHandle == NULL){
        LOGE("drmplayer_videorender_getDisplayInfo() : Bad parameter!");
        return;
    }
    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    (*pDisplayWidth) = pDrmPlayerRenderHandle->mDisplayWidth;
    (*pDisplayHeight) = pDrmPlayerRenderHandle->mDisplayHeight;
    (*pDecodedWidth) = pDrmPlayerRenderHandle->mDecodedWidth;
    (*pDecodedHeight) = pDrmPlayerRenderHandle->mDecodedHeight;
    (*pColorFormat) = pDrmPlayerRenderHandle->mColorFormat;
}

extern "C" void *drmplayer_videorender_reconfig(void *pRenderHandle, OMX_COLOR_FORMATTYPE mColorFormat, size_t displayWidth, size_t displayHeight, size_t decodedWidth, size_t decodedHeight, bool bdestroyov){
    if(pRenderHandle == NULL){
        LOGE("drmplayer_videorender_reconfig() : Bad parameter!");
        return NULL;
    }

    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;
    //pDrmPlayerRenderHandle->mDecodedWidth = decodedWidth;
    //pDrmPlayerRenderHandle->mDecodedHeight = decodedHeight;
    //pDrmPlayerRenderHandle->mDisplayWidth = displayWidth;
    //pDrmPlayerRenderHandle->mDisplayHeight = displayHeight;
    //pDrmPlayerRenderHandle->mColorFormat   = mColorFormat;

    if(OK != pDrmPlayerRenderHandle->reconfigNativeWindow()){
        LOGE("drmplayer_videorender_reconfig() : reconfigNativeWindow() failed!");
        return NULL;
    }
    return NULL;
}

extern "C" int drmplayer_videorender_returnAllGraphicsBuffers(void *pRenderHandle, int *pNum){
    if((pRenderHandle==NULL)||(pNum==NULL)){
        LOGE("drmplayer_videorender_reconfig() : Bad parameter!");
        return -1;
    }

#ifdef DUMP
    dumpCnt = DUMPCNT;
    seekInx ++;
#endif
    DrmPlayerNativeWindowRenderer *pDrmPlayerRenderHandle = ( DrmPlayerNativeWindowRenderer *)pRenderHandle;

    int Count;
    if(OK != pDrmPlayerRenderHandle->returnAllGraphicsBuffers(&Count)){
        LOGE("drmplayer_videorender_reconfig() : reconfigNativeWindow() failed!");
        return -1;
    }
    (*pNum) = Count;
	LOGD("Total %d buffers wait for provide.", (*pNum));
    return OK;
}


