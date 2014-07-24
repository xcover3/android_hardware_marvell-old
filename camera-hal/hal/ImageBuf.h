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

#ifndef ANDROID_HARDWARE_CAMERA_IMAGEBUF_H
#define ANDROID_HARDWARE_CAMERA_IMAGEBUF_H

#define _ALIGN(x, iAlign) ( (((x)) + (iAlign) - 1) & (~((iAlign) - 1)) )

//for cameraparameters
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>

#include <linux/android_pmem.h>
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#include <sys/ioctl.h>

#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <linux/ioctl.h>
//#include <CameraHardwareInterface.h>

//Notes: we only refer to CAM_ImageBuffer & CAM_ImageBufferReq from CameraEngine.h,
//no other structure should be envolved.
#include <CameraEngine.h>

#include <cam_log_mrvl.h>

#define LOG_TAG "CameraBufferHolder"

typedef enum {
    PREVIEW_BUF,
    STILL_BUF,
    VIDEO_BUF,
    INTERNAL_BUF
} BUF_USAGE;

/*{{{*/

/*
 * image buffer header
 * typedef struct
 * {
 *     CAM_ImageFormat         eFormat;
 *     CAM_Int32s              iWidth;
 *     CAM_Int32s              iHeight;
 *     CAM_Int32s              iStep[3];       // Given by external before enqueue
 *     CAM_Int32s              iAllocLen[3];   // Given by external before enqueue
 *     CAM_Int32s              iFilledLen[3];
 *     CAM_Int32s              iOffset[3];     // Given by external before enqueue
 *     CAM_Int8u               *pBuffer[3];    // Given by external before enqueue, virtual address
 *     CAM_Int32u              iPhyAddr[3];    // Given by external before enqueue, physical address of pBuffer[]
 *     CAM_Tick                tTick;
 *     CAM_Int32u              iFlag;          // Given by external before enqueue
 *     CAM_Int32s              iUserIndex;     // Given by external before enqueue
 *     void                    *pUserData;     // Given by external before enqueue
 *     CAM_Int32s              iPrivateIndex;
 *     void                    *pPrivateData;
 *
 *     CAM_Bool                bEnableShotInfo;
 *     CAM_ShotInfo            stShotInfo;
 * } CAM_ImageBuffer;
 *
 * // buffer requirement
 * typedef struct
 * {
 *     CAM_ImageFormat         eFormat;
 *     CAM_Int32s              iWidth;         // image width in pixels
 *     CAM_Int32s              iHeight;        // image height in pixels
 *     CAM_Int32s              iMinStep[3];    // image buffer's row stride in bytes
 *     CAM_Int32s              iMinBufLen[3];
 *     CAM_Int32s              iMinBufCount;
 *     CAM_Int32s              iAlignment[3];  // alignment of each buffer start address
 *     CAM_Int32s              iRowAlign[3];   // alignment of each row
 *
 *     CAM_Int32u              iFlagOptimal;
 *     CAM_Int32u              iFlagAcceptable;
 * } CAM_ImageBufferReq;
*/

/*
 * CAM_ImageBufferReq
 * ^
 * |private
 * |
 * EngBufReq
 *
 *
 * CAM_ImageBuffer
 * ^
 * |private
 * |
 * EngBuf
 *
 *
 * BufInfo           BufState
 * ^                 ^
 * |                 |
 * |------------------
 * |
 * |public
 * |
 * BufDesc
 * ^
 * |
 * |public
 * |
 * CamBuf---------------- EngBufReq, EngBuf
 *           private
 *
 *
 * Vector<CamBuf>
 * ^
 * |public
 * |
 * BufferHolder ---------------- Allocator
 *
 */

/*}}}*/


namespace android {

    class EngBufReq;
    class EngBuf;
    class CamBuf;
    class BufferHolder;
    class CameraHeapMemory;

//EngBufReq
/*{{{*/
    //describe engine buffer requirement
    class EngBufReq: private CAM_ImageBufferReq
    {
        public:
            EngBufReq(const CAM_ImageBufferReq& engbufreq):
                CAM_ImageBufferReq(engbufreq){}

            //CAM_ImageFormat		getFormat() const;
            const char*	        getFormat() const;
            void                getSize(int* w, int* h) const;
            void                getAlignment(int* align0, int* align1, int* align2) const;
            void                getMinStep(int* step0, int* step1, int* step2) const;
            int                 getMinBufCount() const;
            void                getMinBufLen(int* buflen0, int* buflen1, int* buflen2) const;

            //utility to query buffer requirement for sepcified format & size.
            static EngBufReq    getEngBufReq(const char* imageformat, int width, int height, int bufcnt=0);

            const CAM_ImageBufferReq* getCAM_ImageBufferReqPtr() const
            {
                return static_cast<const CAM_ImageBufferReq*>(this);
            }

            void                show() const;

        //private:
        public:
            EngBufReq(){
                //memclr the base class obj directly, which is a raw structure
                memset((void*)(CAM_ImageBufferReq*)this, 0, sizeof(CAM_ImageBufferReq));
            }
    };
/*}}}*/

// EngBuf
/*{{{*/
    //describe engine buffer info
    class EngBuf: private CAM_ImageBuffer
    {
        public:
            EngBuf(){
                //memclr the base class obj directly, which is a raw structure
                memset((void*)(CAM_ImageBuffer*)this, 0, sizeof(CAM_ImageBuffer));
            }

            void        getSize(int* w, int* h) const;
            void        getStep(int* step0, int* step1, int* step2) const;
            void        getFilledLen(int* len0, int* len1, int* len2) const;
            void        getAllocLen(int* len0, int* len1, int* len2) const;
            void        getVAddr(unsigned char** vaddr0, unsigned char** vaddr1, unsigned char** vaddr2) const;
            void        getSetp(int* step0, int* step1, int* step2)const;
            const char*	getFormat() const;//TODO

            void        getPAddr(unsigned long* paddr0, unsigned long* paddr1, unsigned long* paddr2) const;
            int         getMinBufCount() const;

            //status_t	clearBuf();
            //void	    setFormat(CAM_ImageFormat format);
            void	    setFormat(const char* format);//TODO
            void	    setSize(int w, int h);
            void	    setPAddr(unsigned long paddr0, unsigned long paddr1, unsigned long paddr2);
            void	    setVAddr(unsigned char* const vaddr0,
                    unsigned char* const vaddr1, unsigned char* const vaddr2);
            void	    setFilledLen(int len0, int len1, int len2);
            void	    setStep(int step0, int step1, int step2);
            void	    setAllocLen(int len0,int len1,int len2);

            void	    setOffset(int offset0,int offset1,int offset2);
            void	    setUserIndex(int index);
            void	    setUserData(void* userdata);
            void	    setPrivateIndex(int index);
            void	    setPrivateData(void* privatedata);
            nsecs_t     getTimeStamp() const;
            void	    setEnShotInfo(bool enshot);
            void	    setFlag(unsigned int flag);

            const CAM_ImageBuffer* getCAM_ImageBufferPtr() const
            {
                return static_cast<const CAM_ImageBuffer*>(this);
                //return dynamic_cast<CAM_ImageBuffer*>(this);
            }

            static const char* mapFormatEng2Str(CAM_ImageFormat eFormat);
            static CAM_ImageFormat mapFormatStr2Eng(const char* foramt);

            void        show() const;
    };
/*}}}*/

// BufState
/*{{{*/
    //descrive buffer status
    class BufState
    {
        public:
            virtual ~BufState(){}
            BufState(): mDisplayRef(0), mProcessRef(0), mEngineRef(0), mVideoRef(0), mClientRef(0)
            {
#ifdef __RECORD_BUF_REF_TIME__
                mDisplayRef_Time = 0;
                mEngineRef_Time = 0;
                mVideoRef_Time = 0;
                mProcessRef_Time = 0;
                mClientRef_Time = 0;
#endif
            }
            void show() const
            {
                if(isFree()){
                    TRACE_D("\
                            -->free");
                }
                else{
#ifdef __RECORD_BUF_REF_TIME__
                    TRACE_D("\
                            -->displayRef=%d (%lldms), engineRef=%d (%lldms), videoRef=%d (%lldms), processRef=%d (%lldms)",
                            mDisplayRef,mDisplayRef_Time / 1000000L,
                            mEngineRef,mEngineRef_Time / 1000000L,
                            mVideoRef,mVideoRef_Time / 1000000L,
                            mProcessRef,mProcessRef_Time / 1000000L,
                            mClientRef, mClientRef_Time / 1000000L);
#else
                    TRACE_D("\
                            -->displayRef=%d, engineRef=%d, videoRef=%d, processRef=%d, clientRef=%d",
                            mDisplayRef,mEngineRef,mVideoRef,mProcessRef, mClientRef);
#endif
                }
            }

            void resetState(){
                mDisplayRef = mEngineRef = mVideoRef = mProcessRef = mClientRef = 0;
            }
            bool isFree() const{
                return (mDisplayRef == 0) && (mEngineRef == 0) && (mVideoRef == 0) && (mProcessRef == 0) && (mClientRef == 0);
            }
            void getRef(int* displayref, int* engineref, int* videoref, int* processref, int * clientref)const
            {
                *displayref = mDisplayRef;
                *engineref = mEngineRef;
                *videoref = mVideoRef;
                *processref = mProcessRef;
                *clientref = mClientRef;
            }

            void fromEngine(){
                if(mEngineRef == 0){
                    TRACE_E("Multi fromEngine");
                }
                mEngineRef = 0;
                //clrTime(&mEngineRef_Time);
            }
            void fromDisplay(){
                if(mDisplayRef == 0){
                    TRACE_E("Multi fromDisplay");
                }
                mDisplayRef = 0;
                //clrTime(&mDisplayRef_Time);
            }
            void fromClient(){
                if(mClientRef == 0){
                    TRACE_E("Multi fromDisplay");
                }
                mClientRef = 0;
                //clrTime(&mClientRef_Time);
            }
            void fromVideo(){
                if(mVideoRef == 0){
                    TRACE_E("Multi fromVideo");
                }
                mVideoRef = 0;
                //clrTime(&mVideoRef_Time);
            }
            void fromProcess(){
                if(mProcessRef == 0){
                    TRACE_E("Multi fromProcess");
                }
                mProcessRef = 0;
                //clrTime(&mProcessRef_Time);
            }

            void toEngine(){
                if(mEngineRef > 0){
                    TRACE_E("Multi toEngine");
                }
                mEngineRef = 1;
#ifdef __RECORD_BUF_REF_TIME__
                setTime(&mEngineRef_Time);
#endif
            }
            void toProcess(){
                if(mProcessRef > 0){
                    TRACE_E("Multi toProcess");
                }
                mProcessRef = 1;
#ifdef __RECORD_BUF_REF_TIME__
                setTime(&mProcessRef_Time);
#endif
            }
            void toDisplay(){
                if(mDisplayRef > 0){
                    TRACE_E("Multi toDisplay");
                }
                mDisplayRef = 1;
#ifdef __RECORD_BUF_REF_TIME__
                setTime(&mDisplayRef_Time);
#endif
            }

            void toClient(){
                if(mClientRef > 0){
                    TRACE_E("Multi toClient");
                }
                mClientRef = 1;
#ifdef __RECORD_BUF_REF_TIME__
                setTime(&mClientRef_Time);
#endif
            }

            void toVideo(){
                if(mVideoRef > 0){
                    TRACE_E("Multi toVideo");
                }
                mVideoRef = 1;
#ifdef __RECORD_BUF_REF_TIME__
                setTime(&mVideoRef_Time);
#endif
            }

        private:
            int mDisplayRef;
            int mProcessRef;
            int mEngineRef;
            int mVideoRef;
            int mClientRef;
#ifdef __RECORD_BUF_REF_TIME__
            nsecs_t mDisplayRef_Time;
            nsecs_t mEngineRef_Time;
            nsecs_t mVideoRef_Time;
            nsecs_t mProcessRef_Time;
            msecs_t mClientRef_Time;

            void setTime(nsecs_t* ref_time){
                *ref_time = systemTime();
            }
            void clrTime(nsecs_t* ref_time){
                *ref_time = 0;
            }
#endif
    };
/*}}}*/


// BufInfo
/*{{{*/
    //describe android buffer info
    class BufInfo
    {
        public:
            BufInfo()
                : mBufType(BUF_TYPE_NONE), mWidth(0), mHeight(0), mLen(0),
                  mFormat(""), mKey(NULL), mOwner(NULL), mVAddr(NULL),
                  mPAddr(NULL), mPrivate(NULL), mBuffer(NULL)
            {
            }
            virtual ~BufInfo(){
                mBuffer.clear();
                mOwner.clear();
            }
            enum BUF_TYPE {
                BUF_TYPE_NONE           = 0x0001,
                BUF_TYPE_ASHMEM         = 0x0002, //directly allocated from ASHMEM, no phyaddr available
                BUF_TYPE_PMEM           = 0x0004, //directly allocated from PMEM
                BUF_TYPE_NATIVEWINDOW   = 0x0008, //native window buffer
                BUF_TYPE_RAWMEM         = 0x0010, //common memory from malloc
                BUF_TYPE_CAMERAMEMT     = 0x0020, //buffer alloca from camera momory allocator,
                                                    //with image desc info available
                BUF_TYPE_CAMERAMEMT_RAW = 0x0040, //buffer alloca from camera momory allocator,
                                                    //only contain buf len info,
                                                    //jpeg/exif may use such kind buffer.
            };

            void setType(BUF_TYPE type){mBufType = type;}
            void setKey(void* key){mKey = key;}
            void setOwner(sp<BufferHolder> owner){ mOwner = owner;}
            void setFormat(const char* fmt){mFormat = String8(fmt);}
            void setSize(int w, int h){mWidth=w;mHeight=h;}
            void setLen(int len){mLen=len;}
            void setVAddr(unsigned char* vaddr){mVAddr=vaddr;}
            void setPAddr(unsigned long paddr){mPAddr=paddr;}
            void setPrivate(void* user){mPrivate=user;}

            //NOTES: for most buffer types except BUF_TYPE_NATIVEWINDOW, key stands for
            //the buffer allocation index in buffer holder
            void* getKey()const {return mKey;}
            sp<BufferHolder> getOwner()const{return mOwner;}
            BUF_TYPE getType()const {return mBufType;}
            const char* getFormat() const {return mFormat.string();}
            void getSize(int* w, int* h)const {*w = mWidth; *h = mHeight;}
            int getLen()const {return mLen;}
            unsigned char* getVAddr()const {return mVAddr;}
            unsigned long getPAddr()const {return mPAddr;}
            void* getPrivate(){return mPrivate;}

            /*
            //only for BUF_TYPE_PMEM
            sp<IMemoryHeap> getBufHeap();
            */

            //only for MemoryHeapBase obj,
            //invalid for camera_memory_t
            void setIMemory(sp<IMemory> buf){ mBuffer = buf;}
            sp<IMemory>	getIMemory()const {return mBuffer;}

            void show() const
            {
                TRACE_D("BufInfo: \n \
                        -->mBufType=%d, \n \
                        -->mWidth=%d, \n \
                        -->mHeight=%d, \n \
                        -->mLen=%d, \n \
                        -->mFormat=%s \n \
                        -->mKey=%p \n \
                        -->mOwner=%p \n \
                        -->mPrivate=%p \n \
                        -->mVAddr=%p \n \
                        -->mPAddr=%lx \n \
                        -->mBuffer=%p",
                        mBufType,
                        mWidth,
                        mHeight,
                        mLen,
                        mFormat.string(),
                        mKey,
                        mOwner.get(),
                        mPrivate,
                        mVAddr,
                        mPAddr,
                        mBuffer.get()
                        );
                return;
            }

        private:

            BUF_TYPE mBufType;
            int mWidth;
            int mHeight;
            int mLen;
            //const char* mFormat;
            String8 mFormat;

            //for BUF_TYPE_NATIVEWINDOW: use buffer_handle_t*
            //for BUF_TYPE_CAMERAMEMT: use int index or void* vaddr
            //for BUF_TYPE_PMEM: use unsigned long paddr or int index
            //for BUF_TYPE_RAWBUFFER: use void* vaddr or int index
            //const void* mKey;
            void* mKey;
            sp<BufferHolder> mOwner;

            unsigned char* mVAddr;
            unsigned long mPAddr;

            void* mPrivate;//RESERVED

            sp<IMemory> mBuffer;
    };
/*}}}*/

// Bufesc
/*{{{*/
    //for common buffer descriptor
    class BufDesc: public BufInfo, public BufState, public RefBase
    {
        public:
            BufDesc()
            {
                mCamMemT = NULL;
            }
            virtual ~BufDesc()
            {
                if (mCamMemT) {
                    if (getType() == BUF_TYPE_NATIVEWINDOW) {
                        //Every piece of ANW buffer has a MemoryHeapBase_Ext behind it.
                        //Here we're calling __put_memory() from ImageBuf.h to release
                        //the MemoryHeapBase_Ext object.
                        mCamMemT->release(mCamMemT);
                        mCamMemT = NULL;
                    }
                    if (getType() == BUF_TYPE_CAMERAMEMT ||
                        getType() == BUF_TYPE_CAMERAMEMT_RAW) {
                        //Buffers allocated from BufferHolder::allocBuffersWithAllocator()
                        //or BufferHolder::allocRawBuffersWithAllocator()
                        //We should call release() after posting every piece of this kind of buffers,
                        //(by this time, CameraHardwareInterface has already taken over the ownership)
                        //since incStrong() is called in __get_memory().
                        //Here we're calling __put_memory() from CameraHardwareInterface.h to release them
                        //FIXME: automatic release would cause problems when we
                        //allocate several buffers within one request.
                        mCamMemT->release(mCamMemT);
                        mCamMemT = NULL;
                    }
                    if (getType() == BUF_TYPE_PMEM ||
                        getType() == BUF_TYPE_ASHMEM) {
                        //Buffers allocated from BufferHolder::allocBuffers()
                        //We do not need to release this kind of buffers after using them,
                        //because there is a local CameraHeapMemory object referenced by all the
                        //CamBuf objects created within one allocation request.
                    }
                }
            }

            status_t set_cammem_t(camera_memory_t* mem)
            {
                if(NULL == mem){
                    TRACE_E("Invalid camera_memory_t pointer NULL");
                    return -1;
                }
                mCamMemT = mem;
                return NO_ERROR;
            }

            camera_memory_t* get_cammem_t()
            {
                return mCamMemT;
            }

            void setCameraHeapMemoryRef(sp<CameraHeapMemory>& ref)
            {
                mCameraHeapMemoryRef = ref;
            }

        private:
            camera_memory_t* mCamMemT;
            sp<CameraHeapMemory> mCameraHeapMemoryRef;
    };
/*}}}*/

//CamBuf
/*{{{*/
    //for android & engine buffer description
    //1>android buffer interface for communication
    //2>user should not aware the internal implementation of EngBuf, EngBufReq, which could only be
    //accessed by returned internal obj reference.
    //3>put mem operations here
    class CamBuf: public BufDesc
    {
        public:
            CamBuf(){
                mCamMemT_ANW_Wrap = NULL;
            }
            virtual ~CamBuf(){
                if(mCamMemT_ANW_Wrap && getType() == BUF_TYPE_NATIVEWINDOW){
                    mCamMemT_ANW_Wrap->release(mCamMemT_ANW_Wrap);
                    mCamMemT_ANW_Wrap = NULL;
                }
            }
            /*
            status_t getPhyAddr(int index, unsigned long* paddr0,
                    unsigned long* paddr1, unsigned long* paddr2);

            status_t getVirAddr(int index, unsigned char** vaddr0,
                    unsigned char** vaddr1, unsigned char** vaddr2);
                    */

            const EngBuf& getEngBuf()const
            {
                return mEngBuf;
            }
            EngBuf& editEngBuf(){
                return mEngBuf;
            }
            const EngBufReq& getEngBufReq()const
            {
                return mEngBufReq;
            }
            EngBufReq& editEngBufReq(){
                return mEngBufReq;
            }
            status_t set_cammem_ANW_wrap(camera_memory_t* mem){
                if(getType() != BUF_TYPE_NATIVEWINDOW){
                    TRACE_E("Fail to wrap none ANW buffer to camera_memory_t");
                    return -1;
                }
                if(NULL == mem){
                    TRACE_E("Invalid camera_memory_t pointer NULL");
                    return -1;
                }
                mCamMemT_ANW_Wrap = mem;
                return NO_ERROR;
            }

            camera_memory_t* get_cammem_ANW_wrap(){
                if(getType() != BUF_TYPE_NATIVEWINDOW){
                    TRACE_E("Fail to get camera_memory_t from none ANW buffer");
                    return NULL;
                }
                return mCamMemT_ANW_Wrap;
            }

        private:
            //engine buffer desciptor
            EngBuf mEngBuf;
            EngBufReq mEngBufReq;

            //record camera_memory_t wrapped from ANW BUF_TYPE_NATIVEWINDOW buf
            camera_memory_t* mCamMemT_ANW_Wrap;
    };
/*}}}*/

//BufferHolder
/*{{{*/
    //act as a buffer allocator, used to generate the buffer queue
    //class BufferHolder: public Vector<CamBuf>
    class BufferHolder: public RefBase
    {
        public:
            BufferHolder();
            virtual ~BufferHolder();

            static void setAllocator(camera_request_memory get_memory)
            {
                mGetMemory = get_memory;
            }

            static Vector<sp<CamBuf> > allocBuffers(const EngBufReq& bufreq, int bufcnt, const char* BufNode="/dev/pmem");
            static Vector<sp<CamBuf> > allocBuffers(const char* imageformat, int width, int height, int bufcnt,
                    const char* BufNode="/dev/pmem");

            //status_t allocBuffersWithAllocator(){return NO_ERROR;}//XXX
            static Vector<sp<CamBuf> > allocBuffersWithAllocator(const EngBufReq& bufreq, int bufcnt,
                    camera_request_memory _get_memory_allocator=NULL, const char* BufNode = "");

            static Vector<sp<CamBuf> > allocRawBuffersWithAllocator(int bufsize, int bufcnt,
                    camera_request_memory _get_memory_allocator=NULL, const char* BufNode = "");

            static Vector<sp<CamBuf> > registerANWBuf(const EngBufReq& bufreq, Vector<sp<BufDesc> >& queue);

            /*
            static Vector<sp<CamBuf> > registerCamBuf(const CamBuf& buf);
            static Vector<sp<CamBuf> > registerCamBuf(const BufDesc& info);
            */

            static int getQueueIndexByKey(const Vector<sp<CamBuf> >& queue,const void* key);
            static int getQueueIndexBySP(const Vector<sp<CamBuf> >& queue,const sp<IMemory>& mem);
            static int getQueueIndexByVAddr(const Vector<sp<CamBuf> >& queue,const void* vaddr);

            camera_memory_t* getCamMemT() const{return mCamMemT;}

            //override the baseclass show() function
            static void showState(const Vector<sp<CamBuf> >& queue)
            {
                TRACE_D("*****buf state*****");
                int cnt = queue.size();
                for(int i=0; i<cnt; i++) {
                    TRACE_D("index=%d",i);
                    queue.itemAt(i).get()->BufState::show();
                }
            }
            static void showInfo(const Vector<sp<CamBuf> >& queue)
            {
                TRACE_D("*****buf info*****");
                int cnt = queue.size();
                for(int i=0; i<cnt; i++) {
                    TRACE_D("index=%d",i);
                    queue.itemAt(i).get()->BufInfo::show();
                }
            }

            static void showEngBuf(const Vector<sp<CamBuf> >& queue)
            {
                TRACE_D("*****engbuf info*****");
                int cnt = queue.size();
                for(int i=0; i<cnt; i++) {
                    TRACE_D("index=%d",i);
                    queue.itemAt(i)->getEngBuf().show();
                }
            }

            void showEngBufReq(const Vector<sp<CamBuf> >& queue) const
            {
                TRACE_D("*****engbufreq info*****");
                int cnt = queue.size();
                for(int i=0; i<cnt; i++) {
                    TRACE_D("index=%d",i);
                    queue.itemAt(i)->getEngBufReq().show();
                }
            }

            static status_t flush(sp<CamBuf>& buf)
            {
                /*
                   DMA_BIDIRECTIONAL;
                   DMA_TO_DEVICE;
                   DMA_FROM_DEVICE;
                   */
                if(buf.get() == NULL){
                    TRACE_E("buffer node is NULL");
                    return -1;
                }
                if(buf->getType() != BufInfo::BUF_TYPE_PMEM){
                    TRACE_V("%s: Buf type %x don't need flush, return", __FUNCTION__, buf->getType());
                    return NO_ERROR;
                }

                const sp<BufferHolder>& imgbuf = buf->getOwner();
                if(imgbuf == NULL){
                    TRACE_E("Inavlid owner");
                    return -1;
                }

                if (imgbuf.get()->mBufNode == "/dev/ion" && imgbuf.get()->mFD_PMEM >= 0) {
                    const char* fmt = buf->getFormat();
                    if(strcmp(fmt,CameraParameters::PIXEL_FORMAT_YUV422I) == 0 ||
                            // strcmp(fmt,CameraParameters::PIXEL_FORMAT_YUV420P_I420) == 0 ||
                            strcmp(fmt,CameraParameters::PIXEL_FORMAT_YUV420P) == 0 ||
                            strcmp(fmt,CameraParameters::PIXEL_FORMAT_YUV422P) == 0){
                        TRACE_V("%s:The image format don't need flush, return without flush",__FUNCTION__);
                    }
                    else{
                        ssize_t offset = 0;
                        size_t size = 0;
                        sp<IMemory> mem = buf->getIMemory();
                        mem->getMemory(&offset, &size);

                        struct ion_pxa_cache_region region;
                        struct ion_custom_data data;
                        int ret;
                        memset(&region, 0, sizeof(struct ion_pxa_cache_region));
                        memset(&data, 0, sizeof(struct ion_custom_data));
                        region.handle = imgbuf.get()->mHandle;
                        region.offset = offset;
                        region.len = size;
                        region.dir = PXA_DMA_BIDIRECTIONAL;
                        data.cmd = ION_PXA_SYNC;
                        data.arg = (unsigned long)&region;
                        ret = ioctl(imgbuf.get()->mDevFd, ION_IOC_CUSTOM, &data);
                        if (ret < 0) {
                            TRACE_E("failed to flush cache, ret:%d", ret);
                            return INVALID_OPERATION;
                        }
                    }
                }
                return NO_ERROR;
            }
        private:

            size_t mAlignSize;//buffer start addr & buf length align
            int mBufLength;//actually used buf len
            int mBufLength_align;//allocated buf len

            sp<MemoryHeapBase>  mBufHeap;
            int mFD_PMEM;
            String8 mBufNode;

            camera_memory_t* mCamMemT;

            static camera_request_memory mGetMemory;

            /* added for ION support */
            int mDevFd;    //device fd
            uint32_t mPhysBase;  //base of physical address
            struct ion_handle *mHandle;

        public:
#ifdef __DUMP_IMAGE__
            static int _dump_frame_buffer(const char* dump_name, int counter, const CamBuf& imagebuf)
            {
                const BufInfo& info = imagebuf;
                if(info.getVAddr() == NULL){
                    TRACE_E("Invalid info.");
                    return 0;
                }

                const char* fmt = "";
                fmt = info.getFormat();

                int w=0,h=0;
                info.getSize(&w,&h);

                unsigned char* bufptr = const_cast<unsigned char*>(info.getVAddr());
                TRACE_D("%s:vaddr = %p",__FUNCTION__, bufptr);

                int len = info.getLen();
                TRACE_D("%s:len: %d", __FUNCTION__,len);

                char fname[64];
                sprintf(fname,"/data/%s_%dx%d_%d.%s",dump_name, w, h, counter, fmt);
                TRACE_D("%s:dump buf to:%s",__FUNCTION__,fname);

                FILE *fp_dump = fopen(fname, "wb");
                //FILE *fp_dump = fopen(fname, "ab");
                if (fp_dump) {
                    fwrite(bufptr, 1, len, fp_dump);
                    fclose(fp_dump);
                }
                else{
                    TRACE_E("Fail to open %s", fname);
                }
                return 0;
            }
#endif
    };
/*}}}*/

    //FIXME: CameraHeapMemory is private member class inside CameraHardwareInterface,
    //to access it, we have to make one extra copy here. Looks pretty ugly though it works.
/*{{{*/
    static void __put_memory(camera_memory_t *data);
    static camera_memory_t* __get_memory(int fd, size_t buf_size, uint_t num_bufs,
                                         void *user __attribute__((unused)));

    class CameraHeapMemory : public RefBase {
    public:
        //add default constructor which will be used when contruct below CameraHeapMemory_Ext
        CameraHeapMemory(){};

        CameraHeapMemory(int fd, size_t buf_size, uint_t num_buffers = 1) :
                         mBufSize(buf_size),
                         mNumBufs(num_buffers)
        {
            mHeap = new MemoryHeapBase(fd, buf_size * num_buffers);
            commonInitialization();
        }

        CameraHeapMemory(size_t buf_size, uint_t num_buffers = 1) :
                         mBufSize(buf_size),
                         mNumBufs(num_buffers)
        {
            mHeap = new MemoryHeapBase(buf_size * num_buffers);
            commonInitialization();
        }

        void commonInitialization()
        {
            handle.data = mHeap->base();
            handle.size = mBufSize * mNumBufs;
            handle.handle = this;

            mBuffers = new sp<MemoryBase>[mNumBufs];
            for (uint_t i = 0; i < mNumBufs; i++)
                mBuffers[i] = new MemoryBase(mHeap,
                                             i * mBufSize,
                                             mBufSize);

            handle.release = __put_memory;
        }

        virtual ~CameraHeapMemory()
        {
            delete [] mBuffers;
        }

        size_t mBufSize;
        uint_t mNumBufs;
        sp<MemoryHeapBase> mHeap;
        sp<MemoryBase> *mBuffers;

        camera_memory_t handle;
    };

    static camera_memory_t* __get_memory(int fd, size_t buf_size, uint_t num_bufs,
                                         void *user __attribute__((unused)))
    {
        CameraHeapMemory *mem;
        if (fd < 0)
            mem = new CameraHeapMemory(buf_size, num_bufs);
        else
            mem = new CameraHeapMemory(fd, buf_size, num_bufs);
        mem->incStrong(mem);
        return &mem->handle;
    }

    static void __put_memory(camera_memory_t *data)
    {
        if (!data)
            return;

        CameraHeapMemory *mem = static_cast<CameraHeapMemory *>(data->handle);
        mem->decStrong(mem);
    }
/*}}}*/

//MemoryHeapBase->cameraHeapMemory->camera_memory_t converter
/*{{{*/
    class MemoryHeapBase_Ext: public MemoryHeapBase
    {
        public:
            MemoryHeapBase_Ext(int fd, void *base, size_t size, uint32_t flags,
                    const char* device, size_t offset)
                //:MemoryHeapBase(-1, size, flags, offset)
                //MemoryHeapBase default ctor could meet requirements, skip calling explicitly
        {
	         MemoryHeapBase::init(fd, base, size, flags, device);
        }
    };

    class CameraHeapMemory_Ext: public CameraHeapMemory
    {
        public:
            /*
            using CameraHeapMemory::CameraHeapMemory(int fd, size_t buf_size, uint_t num_buffers = 1);
            using CameraHeapMemory::CameraHeapMemory(size_t buf_size, uint_t num_buffers = 1);
            */

            CameraHeapMemory_Ext(int fd, void* base, size_t size, uint_t num_buffers,
                    const char* device, uint32_t flags = MemoryHeapBase::DONT_MAP_LOCALLY,
                    uint32_t offset = 0)
        {
            mHeap = new MemoryHeapBase_Ext(fd, base, size, flags, device, offset);

            mBufSize = size;
            mNumBufs = num_buffers;
            handle.data = mHeap->base();
            //handle.data = base;
            handle.size = mBufSize * mNumBufs;
            handle.handle = static_cast<CameraHeapMemory*>(this);

            mBuffers = new sp<MemoryBase>[mNumBufs];
            for (uint_t i = 0; i < mNumBufs; i++)
                mBuffers[i] = new MemoryBase(mHeap,
                                             i * mBufSize,
                                             mBufSize);

            handle.release = __put_memory;
        }
    };

    /*
     * NOTES:as MemoryHeapBase will do dispose in destor function, the ANW buffer fd will be closed.
     * we must pay more attention to the release point of CameraHeapMemory_Ext obj created here.
     */
    static camera_memory_t* get_camera_memory_t_from_ANW(sp<CamBuf> buf)
    {
        TRACE_V("%s: key=%p", __FUNCTION__,buf->getKey());
        CameraHeapMemory *mem;
        int fd = (int)(buf->getPrivate());
        if (fd >= 0){
            mem = new CameraHeapMemory_Ext(dup(fd), buf->getVAddr(), buf->getLen(),
                    1, "/dev/ion", MemoryHeapBase::DONT_MAP_LOCALLY);
        }
        else{
            TRACE_E("Invalid buffer node %d", fd);
            return NULL;
        }

        mem->incStrong(mem);
        return &mem->handle;
    }

/*}}}*/

}; // namespace android

#endif
