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

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include "cam_log_mrvl.h"
#include "CameraSetting.h"

#include "ImageBuf.h"
#define LOG_TAG "CameraBufferHolder"

namespace android {

//==========================================================================================

#define ARRAY_SET(dest,src) \
    do{\
        dest[0]=src##0; \
        dest[1]=src##1; \
        dest[2]=src##2; \
    }while(0);

#define ARRAY_GET(dest,src) \
    do{\
        *(dest##0)=src[0]; \
        *(dest##1)=src[1]; \
        *(dest##2)=src[2]; \
    }while(0);

//EngBufReq
/*{{{*/

    int EngBufReq::getMinBufCount()const
    {
        return iMinBufCount;
    }

    void EngBufReq::getSize(int* w, int* h)const
    {
        *w = iWidth;
        *h = iHeight;
    }

    void EngBufReq::getAlignment(int* align0, int* align1, int* align2)const
    {
        ARRAY_GET(align,iAlignment);
    }

    void EngBufReq::getMinBufLen(int* buflen0, int* buflen1, int* buflen2)const
    {
        ARRAY_GET(buflen,iMinBufLen);
    }

    //CAM_ImageFormat EngBufReq::getFormat()const
    const char* EngBufReq::getFormat()const
    {
        const char* fmt = EngBuf::mapFormatEng2Str(eFormat);
        return fmt;
    }

    void	EngBufReq::getMinStep(int* step0, int* step1, int* step2)const
    {
        ARRAY_GET(step,iMinStep);
    }

    //stati
    EngBufReq EngBufReq::getEngBufReq(const char* imageformat, int width, int height, int bufcnt)
    {
        //check image format & init image basic info
        const char* fmt_str = imageformat;
        EngBufReq bufreq;
        /*
        if(strcmp(fmt_str,CameraParameters::PIXEL_FORMAT_YUV420P_I420)==0){
            bufreq.eFormat = CAM_IMGFMT_YCbCr420P;
            bufreq.iMinStep[0] =	width;
            bufreq.iMinStep[1] =	width / 2;
            bufreq.iMinStep[2] =	width / 2;
            bufreq.iMinBufLen[0] =	width * height;
            bufreq.iMinBufLen[1] =	bufreq.iMinBufLen[0]/4;
            bufreq.iMinBufLen[2] =	bufreq.iMinBufLen[0]/4;
        }
        else*/ if(strcmp(fmt_str,CameraParameters::PIXEL_FORMAT_YUV420P)==0){
            bufreq.eFormat = CAM_IMGFMT_YCrCb420P;
            bufreq.iMinStep[0] =	width;
            bufreq.iMinStep[1] =	width / 2;
            bufreq.iMinStep[2] =	width / 2;
            bufreq.iMinBufLen[0] =	width * height;
            bufreq.iMinBufLen[1] =	bufreq.iMinBufLen[0]/4;
            bufreq.iMinBufLen[2] =	bufreq.iMinBufLen[0]/4;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV420SP)==0){
            bufreq.eFormat = CAM_IMGFMT_YCrCb420SP;
            bufreq.iMinStep[0] =	width;
            bufreq.iMinStep[1] =	width;
            bufreq.iMinStep[2] =	0;
            bufreq.iMinBufLen[0] =	width * height;
            bufreq.iMinBufLen[1] =	bufreq.iMinBufLen[0]/2;
            bufreq.iMinBufLen[2] =	0;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV422I)==0){
            bufreq.eFormat = CAM_IMGFMT_CbYCrY;
            bufreq.iMinStep[0] =	width * 2;
            bufreq.iMinStep[1] =	0;
            bufreq.iMinStep[2] =	0;
            bufreq.iMinBufLen[0] =	width * height * 2;
            bufreq.iMinBufLen[1] =	0;
            bufreq.iMinBufLen[2] =	0;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV422P)==0){
            bufreq.eFormat = CAM_IMGFMT_YCbCr422P;
            bufreq.iMinStep[0] =	width;
            bufreq.iMinStep[1] =	width/2;
            bufreq.iMinStep[2] =	width/2;
            bufreq.iMinBufLen[0] =	width * height;
            bufreq.iMinBufLen[1] =	bufreq.iMinBufLen[0]/2;
            bufreq.iMinBufLen[2] =	bufreq.iMinBufLen[0]/2;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_RGB565)==0){
            bufreq.eFormat = CAM_IMGFMT_RGB565;
            bufreq.iMinStep[0] =	width * 2;
            bufreq.iMinStep[1] =	0;
            bufreq.iMinStep[2] =	0;
            bufreq.iMinBufLen[0] =	width * height * 2;
            bufreq.iMinBufLen[1] =	0;
            bufreq.iMinBufLen[2] =	0;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_JPEG)==0){
            bufreq.eFormat = CAM_IMGFMT_JPEG;
            bufreq.iMinStep[0] =	0;
            bufreq.iMinStep[1] =	0;
            bufreq.iMinStep[2] =	0;
            bufreq.iMinBufLen[0] =	width * height / 2;//TODO:experience value
            bufreq.iMinBufLen[1] =	0;
            bufreq.iMinBufLen[2] =	0;
        }
        else{
            TRACE_E("Invalid imageformat: %s", fmt_str);
        }

        //init other parameters
        {
            bufreq.iWidth =	width;
            bufreq.iHeight = height;
            bufreq.iMinBufCount = bufcnt;//TODO
        }
        return bufreq;
    }

    void EngBufReq::show()const
    {
        TRACE_D("EngBufReq: \n \
                -->eFormat=%d, \n \
                -->iWidth=%d, \n \
                -->iHeight=%d, \n \
                -->iMinStep=%d,%d,%d, \n \
                -->iMinBufLen=%d,%d,%d, \n \
                -->iMinBufCount=%d, \n \
                -->iAlignment=%d,%d,%d, \n \
                -->iRowAlign=%d,%d,%d, \n \
                -->iFlagOptimal=%d, \n \
                -->iFlagAcceptable=%d",
                eFormat,
                iWidth,
                iHeight,
                iMinStep[0], iMinStep[1], iMinStep[2],
                iMinBufLen[0], iMinBufLen[1], iMinBufLen[2],
                iMinBufCount,
                iAlignment[0], iAlignment[1], iAlignment[2],
                iRowAlign[0], iRowAlign[1], iRowAlign[2],
                iFlagOptimal, iFlagAcceptable
                );
        return;
    }

/*}}}*/



//EngBuf
/*{{{*/
    void    EngBuf::setVAddr(unsigned char* const vaddr0, unsigned char* const vaddr1, unsigned char* const vaddr2)
    {
        ARRAY_SET(pBuffer,vaddr);
    }

    void    EngBuf::setPAddr(unsigned long paddr0, unsigned long paddr1, unsigned long paddr2)
    {
        ARRAY_SET(iPhyAddr,paddr);
    }

    void    EngBuf::getAllocLen(int* len0, int* len1, int* len2) const
    {
        ARRAY_GET(len,iAllocLen);
    }

    void    EngBuf::setFilledLen(int len0, int len1, int len2)
    {
        ARRAY_SET(iFilledLen,len);
    }

    void    EngBuf::getPAddr(unsigned long* paddr0, unsigned long* paddr1, unsigned long* paddr2)const
    {
        ARRAY_GET(paddr,iPhyAddr);
    }

    void    EngBuf::getVAddr(unsigned char** vaddr0, unsigned char** vaddr1, unsigned char** vaddr2)const
    {
        ARRAY_GET(vaddr,pBuffer);
    }

    void    EngBuf::getSize(int* w, int* h)const
    {
        *w = iWidth;
        *h = iHeight;
    }

    void    EngBuf::getSetp(int* step0, int* step1, int* step2)const
    {
        ARRAY_GET(step,iStep);
    }

    void    EngBuf::getFilledLen(int* len0, int* len1, int* len2)const
    {
        ARRAY_GET(len,iFilledLen);
    }

    void	EngBuf::getStep(int* step0, int* step1, int* step2) const
    {
        ARRAY_GET(step,iStep);
    }

    void	EngBuf::setFormat(const char* format)//TODO
    {
        eFormat = mapFormatStr2Eng(format);
    }
    /*
    void	EngBuf::setFormat(CAM_ImageFormat format)
    {
        eFormat = format;
    }
    */

    void	EngBuf::setSize(int w, int h)
    {
        iWidth = w;
        iHeight = h;
    }

    void	EngBuf::setStep(int step0, int step1, int step2)
    {
        ARRAY_SET(iStep,step);
    }

    void	EngBuf::setAllocLen(int len0,int len1,int len2)
    {
        ARRAY_SET(iAllocLen,len);
    }

    void	EngBuf::setOffset(int offset0,int offset1,int offset2)
    {
        ARRAY_SET(iOffset,offset);
    }

    void	EngBuf::setUserIndex(int index)
    {
        iUserIndex = index;
    }
    void	EngBuf::setUserData(void* userdata)
    {
        pUserData = userdata;
    }
    void	EngBuf::setPrivateIndex(int index)
    {
        iPrivateIndex = index;
    }
    void	EngBuf::setPrivateData(void* privatedata)
    {
        pPrivateData = privatedata;
    }
    nsecs_t     EngBuf::getTimeStamp() const
    {
        return getCAM_ImageBufferPtr()->tTick;

    }
    void	EngBuf::setEnShotInfo(bool enshot)
    {
        bEnableShotInfo = enshot;
    }
    void	EngBuf::setFlag(unsigned int flag)
    {
        iFlag=flag;
    }
#if 1
    //static
    const char* EngBuf::mapFormatEng2Str(CAM_ImageFormat eFormat)
    {
#if 1
        String8 ret = CameraSetting::map_ce2andr(CameraSetting::map_imgfmt, eFormat);
        return ret.string();
#else
        const char* fmt="";
        switch (eFormat){
            case CAM_IMGFMT_RGB565:
                fmt = (CameraParameters::PIXEL_FORMAT_RGB565);
                break;
            case CAM_IMGFMT_YCbCr420P:
                fmt = (CameraParameters::PIXEL_FORMAT_YUV420P_I420);
                break;
            case CAM_IMGFMT_YCrCb420P:
                fmt = (CameraParameters::PIXEL_FORMAT_YUV420P);
                break;
            case CAM_IMGFMT_YCrCb420SP:
                fmt = (CameraParameters::PIXEL_FORMAT_YUV420SP); //NV21
                break;
            case CAM_IMGFMT_CbYCrY:
                fmt = (CameraParameters::PIXEL_FORMAT_YUV422I_UYVY);
                break;
            case CAM_IMGFMT_YCbCr422P:
                fmt = (CameraParameters::PIXEL_FORMAT_YUV422P);
                break;
            case CAM_IMGFMT_JPEG:
                fmt = (CameraParameters::PIXEL_FORMAT_JPEG);
                break;
            default:
                TRACE_E("Invalid format, %d", eFormat);
        }
        return fmt;
#endif
    }
    //static
    CAM_ImageFormat EngBuf::mapFormatStr2Eng(const char* format)
    {
#if 1
        int cesetting = CameraSetting::map_andr2ce(CameraSetting::CameraSetting::map_imgfmt,format);
        return (CAM_ImageFormat)(cesetting);
#else
        const char* fmt_str = format;
        CAM_ImageFormat eFormat = (CAM_ImageFormat)0xFFFF;
        if(strcmp(fmt_str,CameraParameters::PIXEL_FORMAT_YUV420P_I420)==0){
            eFormat = CAM_IMGFMT_YCbCr420P;
        }
        else if(strcmp(fmt_str,CameraParameters::PIXEL_FORMAT_YUV420P)==0){
            eFormat = CAM_IMGFMT_YCrCb420P;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV420SP)==0){
            eFormat = CAM_IMGFMT_YCrCb420SP;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV422I_UYVY)==0){
            eFormat = CAM_IMGFMT_CbYCrY;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_YUV422P)==0){
            eFormat = CAM_IMGFMT_YCbCr422P;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_RGB565)==0){
            eFormat = CAM_IMGFMT_RGB565;
        }
        else if(strcmp(fmt_str, CameraParameters::PIXEL_FORMAT_JPEG)==0){
            eFormat = CAM_IMGFMT_JPEG;
        }
        return eFormat;
#endif
    }
#endif

    void    EngBuf::show()const
    {
        TRACE_D("EngBuf: \n \
                -->eFormat=%d, \n \
                -->iWidth=%d, \n \
                -->iHeight=%d, \n \
                -->iStep=%d,%d,%d, \n \
                -->iAllocLen=%d,%d,%d, \n \
                -->iFilledLen=%d,%d,%d, \n \
                -->iOffset=%d,%d,%d, \n \
                -->pBuffer=%p,%p,%p, \n \
                -->iPhyAddr=%x,%x,%x, \n \
                -->iFlag=%x, \n \
                -->iUserIndex=%d, \n \
                -->pUserData=%p, \n \
                -->iPrivateIndex=%d, \n \
                -->pPrivateData=%p, \n \
                -->bEnableShotInfo=%d",
                eFormat,
                iWidth,iHeight,
                iStep[0],iStep[1],iStep[2],
                iAllocLen[0],iAllocLen[1],iAllocLen[2],
                iFilledLen[0],iFilledLen[1],iFilledLen[2],
                iOffset[0],iOffset[1],iOffset[2],
                pBuffer[0],pBuffer[1],pBuffer[2],
                iPhyAddr[0],iPhyAddr[1],iPhyAddr[2],
                iFlag,iUserIndex,pUserData,iPrivateIndex,
                pPrivateData,bEnableShotInfo);
        return;
    }

/*}}}*/

#undef ARRAY_SET
#undef ARRAY_GET


//BufferHolder
/*{{{*/
    camera_request_memory BufferHolder::mGetMemory=NULL;
    BufferHolder::BufferHolder(){
        mBufHeap.clear();
        mFD_PMEM = -1;
        mDevFd = -1;
        mPhysBase = 0;
        mHandle = NULL;
        mBufNode = String8("");
        mAlignSize = 0;
        mBufLength = 0;
        mBufLength_align = 0;
        mCamMemT = NULL;
    }
    BufferHolder::~BufferHolder(){
            //in ION scenario, mFD_PMEM is the buffer fd.
            //we shouldn't close it here then before ~MemoryHeapBase() which
            //would use the buffer fd to free ION region.
            //or we can just do it here and let ~MemoryHeapBase() do nothing.
        if(mFD_PMEM >= 0){//ANW buf won't come here
            mBufHeap->dispose();
        }
        mBufHeap.clear();
        if(mCamMemT != NULL){
            mCamMemT->release(mCamMemT);
            mCamMemT = NULL;
        }
        if(mFD_PMEM >= 0){
            close(mFD_PMEM);
            mFD_PMEM = -1;
            close(mDevFd);
            mDevFd = -1;
            mPhysBase = 0;
            mHandle = NULL;
        }
    }

    Vector<sp<CamBuf> > BufferHolder::allocBuffers(const EngBufReq& bufreq, int bufcnt,
            const char* BufNode)
    {

        sp<BufferHolder> self_sp = new BufferHolder;
        BufferHolder* self = self_sp.get();
        Vector<sp<CamBuf> > OutputQueue;
        OutputQueue.setCapacity(bufcnt);

        bool reCalBufLen = false;
        int actualWidth, actualHeight;
        int bufferStrideReqX, bufferStrideReqY;
        int width,height;
        bufreq.getSize(&width,&height);
        if(0>=height || 0>=width){
            TRACE_E("Invalid size: %dx%d", width, height);
            return OutputQueue;
        }

        // CameraSetting::getBufferStrideReq(bufferStrideReqX,bufferStrideReqY);
        // if(usage != INTERNAL_BUF && usage != STILL_BUF) {
            // if(width % bufferStrideReqX != 0 || height % bufferStrideReqY != 0) {
                reCalBufLen = true;
                actualWidth = _ALIGN(width, 16);
                actualHeight = _ALIGN(height, 32);
                TRACE_D("Eng Buffer request is not match with BufferStrideReq, actual buf wxh:%dx%d",actualWidth,actualHeight);
            // }
        // }
        // else
            // reCalBufLen = false;

        int align[3];
        bufreq.getAlignment(align+0,align+1,align+2);
        int engalign=align[0]; //TODO: only consider first plan start addr align
        int androidalign = (int)getpagesize();
        int alignsize = androidalign>engalign? androidalign:engalign;//start addr align & buf length align

        //for android required IMemory interface, so no hole is allowed between plannar data,
        //so here we simply assume continuous buffer
        CameraProfiles* mCamProfiles = CameraProfiles::getInstance();
        int len[3];
        int tmpMin[3];
        if(reCalBufLen == true) {
                bufreq.getMinBufLen(tmpMin+0,tmpMin+1,tmpMin+2);
                len[0] = tmpMin[0]/width*actualWidth/height*actualHeight;
                len[1] = tmpMin[1]/width*actualWidth/height*actualHeight;
                len[2] = tmpMin[2]/width*actualWidth/height*actualHeight;
        }
        else
            bufreq.getMinBufLen(len+0,len+1,len+2);

        int buflen = len[0] + len[1] + len[2];
        //consider buf length align & buf start addr align
        int buflen_align = ((buflen + alignsize-1) & ~(alignsize-1)) + (alignsize);

        self->mBufLength = buflen;
        self->mBufLength_align = buflen_align;

        self->mBufNode = String8(BufNode);
        self->mFD_PMEM = -1;
        self->mDevFd = -1;
        int fd_pmem = -1;
        struct ion_fd_data req_fd;
        struct pmem_region region;
        int ret;

        sp<MemoryHeapBase> base;
        sp<CameraHeapMemory> chm;
        if (0 != strcmp(BufNode, ""))
        {
            struct ion_fd_data req;
            struct ion_custom_data data;
            struct ion_pxa_region ion_region;
            TRACE_D("%s:alloc imagebuf from %s",__FUNCTION__,BufNode);

            int fd = open(BufNode, O_RDWR);
            if (fd >= 0) {
                chm = new CameraHeapMemory(fd, buflen_align, bufcnt);
                base = chm -> mHeap;
            }
            else {
                ALOGE("%s:error opening %s: %s", __FUNCTION__, BufNode, strerror(errno));
                goto dumpion;
            }
            if (base->heapID() < 0) {
                TRACE_E("Failed to allocate imagebuf from %s", BufNode);
                goto dumpion;
            }

            /* import buffer fd to get handle */
            memset(&req, 0, sizeof(struct ion_fd_data));
            req.fd = base->heapID(); /* buffer fd */
            ret = ioctl(fd, ION_IOC_IMPORT, &req);
            if (ret < 0) {
                ALOGE("failed to import buffer fd:%d, return error:%d", req.fd, ret);
                close(fd);
                goto dumpion;
            }
            /* fetch physical address */
            memset(&ion_region, 0, sizeof(struct ion_pxa_region));
            memset(&data, 0, sizeof(struct ion_custom_data));
            ion_region.handle = req.handle;
            data.cmd = ION_PXA_PHYS;
            data.arg = (unsigned long)&ion_region;
            ret = ioctl(fd, ION_IOC_CUSTOM, &data);
            if (ret < 0) {
                ALOGE("failed to get physical address from ION, return error:%d", ret);
                close(fd);
                goto dumpion;
            }

            region.offset = ion_region.addr; /* get physical address */
            self->mPhysBase = ion_region.addr;
            self->mHandle = req.handle;
            self->mDevFd = fd;   // control fd on device
            self->mFD_PMEM = base->heapID(); /* buffer fd */
            self->mBufHeap = base;
            fd_pmem = fd;  // use buffer fd instead
        }
        else{
            fd_pmem = -1;

            sp<MemoryHeapBase> base = new MemoryHeapBase( self->mBufLength_align * bufcnt);
            self->mBufHeap = base;
        }
        for (int i = 0; i < bufcnt; i++) {
            ssize_t offset = 0;
            size_t size = 0;
            sp<IMemory> buffer;
            //important: reallocate offset-aligned buffers to replace the both-offset-and-length-aligned
            //ones(with incorrect buf length) automatically allocated in CameraHeapMemory::commonInitialization()
            chm->mBuffers[i] = new MemoryBase(self->mBufHeap, i * self->mBufLength_align, self->mBufLength);
            buffer = chm->mBuffers[i];
            buffer->getMemory(&offset, &size);

            unsigned long paddr[3];
            if(fd_pmem >= 0){
                paddr[0] = (unsigned long)(region.offset + i * self->mBufLength_align);
                paddr[1] = paddr[0] + len[0];
                paddr[2] = paddr[1] + len[1];
            }
            else{
                //no phyaddr available for ashmem
                paddr[0] = paddr[1] = paddr[2] = 0;
            }

            //vaddr
            unsigned char* vaddr[3];
            vaddr[0] = (unsigned char*)((unsigned char*)self->mBufHeap->base() + offset);
            vaddr[1] = (unsigned char*)((unsigned char*)(vaddr[0]) + len[0]);
            vaddr[2] = (unsigned char*)((unsigned char*)(vaddr[1]) + len[1]);

            //width, height
            int width,height;
            bufreq.getSize(&width, &height);

            int step[3];
            bufreq.getMinStep(step+0, step+1, step+2);//TODO:check step

            const char* fmt = bufreq.getFormat();

            //-----------------------------------------------------------------------
            //construct CamBuf
            sp<CamBuf> cambuf_sp = new CamBuf;
            CamBuf* cambuf = cambuf_sp.get();

            //init BufDesc part
            BufDesc* bufdesc = cambuf;

            if(fd_pmem >= 0){
                bufdesc->setType(BufDesc::BUF_TYPE_PMEM);
            }
            else{
                bufdesc->setType(BufDesc::BUF_TYPE_ASHMEM);
            }
            bufdesc->setKey((void*)i);//use index to mark such buffer node
            bufdesc->setOwner(self_sp);
            bufdesc->setFormat(fmt);
            bufdesc->setSize(width, height);
            bufdesc->setLen(len[0]+len[1]+len[2]);
            bufdesc->setPAddr(paddr[0]);
            bufdesc->setVAddr(vaddr[0]);
            bufdesc->setIMemory(buffer);
            bufdesc->setPrivate((void*)0);

            //init EngBuf part
            EngBuf* engbuf = &(cambuf->editEngBuf());

            engbuf->setFormat(fmt);
            engbuf->setSize(width, height);
            engbuf->setPAddr(paddr[0],paddr[1],paddr[2]);
            engbuf->setVAddr(vaddr[0],vaddr[1],vaddr[2]);

            engbuf->setAllocLen(len[0],len[1],len[2]);
            engbuf->setFilledLen(len[0],len[1],len[2]);

            engbuf->setStep(step[0],step[1],step[2]);

            engbuf->setOffset(0,0,0);
            engbuf->setUserIndex((int)(bufdesc->getKey()));
            engbuf->setUserData(NULL);
            engbuf->setPrivateIndex(0);
            engbuf->setPrivateData(NULL);
            engbuf->setEnShotInfo(CAM_TRUE);

            if(fd_pmem >= 0){
                engbuf->setFlag(
                        BUF_FLAG_PHYSICALCONTIGUOUS |
                        BUF_FLAG_L1NONCACHEABLE |
                        BUF_FLAG_L2NONCACHEABLE |
                        BUF_FLAG_UNBUFFERABLE |
                        BUF_FLAG_YUVBACKTOBACK |
                        BUF_FLAG_FORBIDPADDING);
            }
            else{
                engbuf->setFlag(
                        BUF_FLAG_NONPHYSICALCONTIGUOUS |
                        BUF_FLAG_YUVBACKTOBACK |
                        BUF_FLAG_FORBIDPADDING);
            }

            //init EngBufReq part
            EngBufReq* req = &(cambuf->editEngBufReq());

            *req = bufreq;
            //important: keep one reference of CameraHeapBase in each CamBuf
            bufdesc->setCameraHeapMemoryRef(chm);
            bufdesc->set_cammem_t(&(chm->handle));
            OutputQueue.push_back(cambuf_sp);
        }
        return OutputQueue;
dumpion:
        //allocate buffers from ION failed, output "/sys/kernel/debug/ion/carveout_heap" to logcat
        ALOGE("=======================ION memory usage information:==================");
        FILE* fp = fopen("/sys/kernel/debug/ion/carveout_heap", "r");
        if(fp == NULL) {
            return OutputQueue;
        }

        int bufsize = 65536;
        char* buf = (char*)malloc(bufsize);

        int read = fread(buf, 1, bufsize, fp);
        fclose(fp);

        char* line = buf;
        for (int i = 0; i < read; i++) {
            if (buf[i] == '\n') {
                buf[i] = '\0';
                ALOGE("%s", line);
                line = buf+i+1;
            }
            else if (i == read-1) {
                ALOGE("%s", line);
            }
        }
        free(buf);
        ALOGE("======================================================================");
        return OutputQueue;

    }

    Vector<sp<CamBuf> >  BufferHolder::allocBuffers(const char* imageformat, int width, int height, int bufcnt,
            const char* BufNode)
    {
        EngBufReq bufreq = EngBufReq::getEngBufReq(imageformat, width, height, bufcnt);
        return allocBuffers(bufreq, bufcnt, BufNode);
    }

    Vector<sp<CamBuf> >  BufferHolder::allocRawBuffersWithAllocator(int bufsize, int bufcnt,
            camera_request_memory _get_memory_allocator, const char* BufNode)
    {
        sp<BufferHolder> self_sp = new BufferHolder;
        BufferHolder* self = self_sp.get();
        Vector<sp<CamBuf> > OutputQueue;
        OutputQueue.setCapacity(bufcnt);

        if(strcmp(BufNode, "") == 0){
            TRACE_D("%s: allocator using ashmem", __FUNCTION__);
            self->mFD_PMEM = -1;
        }
        else{
            TRACE_D("%s: allocator using mem node %s", __FUNCTION__, BufNode);
            int open_flags = O_RDWR;
            //open_flags |= O_SYNC;//XXX
            self->mFD_PMEM = open(BufNode, open_flags);
            if (self->mFD_PMEM < 0) {
                TRACE_E("error opening %s: %s", BufNode, strerror(errno));
                return OutputQueue;
            }
        }

        self->mBufLength = bufsize;

        camera_request_memory allocator = NULL;
        if(_get_memory_allocator == NULL){
            if(mGetMemory == NULL){
                TRACE_E("No allocator available");
                return OutputQueue;
            }
            allocator = mGetMemory;
        }
        else{
            allocator = _get_memory_allocator;
        }
        camera_memory_t* mem = allocator(self->mFD_PMEM, self->mBufLength, bufcnt, NULL);

        self->mCamMemT = mem;

        for(int i=0; i<bufcnt; i++)
        {
            unsigned long paddr[3];
            paddr[0] = paddr[1] = paddr[2] = 0;

            //vaddr
            unsigned char* vaddr[3];
            vaddr[0] = (unsigned char*)((unsigned char*)mem->data + i * self->mBufLength);
            vaddr[1] = vaddr[2] = 0;

            //-----------------------------------------------------------------------
            //construct CamBuf
            sp<CamBuf> cambuf_sp = new CamBuf;
            CamBuf* cambuf = cambuf_sp.get();

            //init BufDesc part
            BufDesc* bufdesc = cambuf;

            bufdesc->setType(BufDesc::BUF_TYPE_CAMERAMEMT_RAW);
            bufdesc->setKey((void*)i);//use index to mark such buffer node
            bufdesc->setOwner(self_sp);
            bufdesc->setFormat("");
            bufdesc->setSize(0, 0);
            bufdesc->setLen(bufsize);
            bufdesc->setPAddr(paddr[0]);
            bufdesc->setVAddr(vaddr[0]);
            bufdesc->setIMemory(0);
            bufdesc->setPrivate((void*)0);

            //init EngBufReq part
            //EngBufReq* req = &(cambuf->editEngBufReq());
            //*req = bufreq;

            OutputQueue.push_back(cambuf_sp);
        }
        return OutputQueue;
    }


    Vector<sp<CamBuf> >  BufferHolder::allocBuffersWithAllocator(const EngBufReq& bufreq, int bufcnt,
            camera_request_memory _get_memory_allocator, const char* BufNode)
    {
        sp<BufferHolder> self_sp = new BufferHolder;
        BufferHolder* self = self_sp.get();
        Vector<sp<CamBuf> > OutputQueue;
        OutputQueue.setCapacity(bufcnt);

        int align[3];
        bufreq.getAlignment(align+0,align+1,align+2);
        int engalign=align[0]; //TODO: only consider first plan start addr align
        int androidalign = (int)getpagesize();
        int alignsize = androidalign>engalign? androidalign:engalign;//start addr align & buf length align

        //for android required IMemory interface, so no hole is allowed between plannar data,
        //so here we simply assume continuous buffer
        int minbuflen[3];
        bufreq.getMinBufLen(minbuflen+0,minbuflen+1,minbuflen+2);
        int buflen = minbuflen[0] + minbuflen[1] + minbuflen[2];
        //consider buf length align & buf start addr align
        int buflen_align = ((buflen + alignsize-1) & ~(alignsize-1)) + (alignsize);

        self->mBufLength = buflen;
        //self->mBufLength_align = buflen_align;
        self->mBufLength_align = buflen;

        if(strcmp(BufNode, "") == 0){
            TRACE_D("%s: allocator using ashmem", __FUNCTION__);
            self->mFD_PMEM = -1;
        }
        else{
            TRACE_D("%s: allocator using mem node %s", __FUNCTION__, BufNode);
            int open_flags = O_RDWR;
            //open_flags |= O_SYNC;//XXX
            self->mFD_PMEM = open(BufNode, open_flags);
            if (self->mFD_PMEM < 0) {
                TRACE_E("error opening %s: %s", BufNode, strerror(errno));
                return OutputQueue;
            }
        }
        camera_request_memory allocator = NULL;
        if(_get_memory_allocator == NULL){
            if(mGetMemory == NULL){
                TRACE_E("No allocator available");
                return OutputQueue;
            }
            allocator = mGetMemory;
        }
        else{
            allocator = _get_memory_allocator;
        }

        //FIXME: align hole?
        camera_memory_t* mem = allocator(self->mFD_PMEM, self->mBufLength, bufcnt, NULL);
        //XXX: make a local recorder as we need to rls the buffer when destroy
        self->mCamMemT = mem;

        struct pmem_region region;
        if(self->mFD_PMEM >= 0){
            if (ioctl(self->mFD_PMEM, PMEM_GET_PHYS, &region)) {
                TRACE_E("Failed to get imagebuf physical address");
                return OutputQueue;
            }
        }

        for(int i=0; i<bufcnt; i++)
        {
            int len[3];
            bufreq.getMinBufLen(len+0,len+1,len+2);

            //width, height
            int width,height;
            bufreq.getSize(&width, &height);

            int step[3];
            bufreq.getMinStep(step+0, step+1, step+2);//TODO:check step

            const char* fmt = bufreq.getFormat();

            unsigned long paddr[3];

            if(self->mFD_PMEM >= 0){
                paddr[0] = (unsigned long)(region.offset + i * self->mBufLength);
                paddr[1] = paddr[0] + len[0];
                paddr[2] = paddr[1] + len[1];
            }
            else{
                paddr[0] = paddr[1] = paddr[2] = 0;
            }

            //vaddr
            unsigned char* vaddr[3];
            vaddr[0] = (unsigned char*)((unsigned char*)mem->data + i * self->mBufLength);
            vaddr[1] = (unsigned char*)((unsigned char*)(vaddr[0]) + len[0]);
            vaddr[2] = (unsigned char*)((unsigned char*)(vaddr[1]) + len[1]);


            //-----------------------------------------------------------------------
            //construct CamBuf
            sp<CamBuf> cambuf_sp = new CamBuf;
            CamBuf* cambuf = cambuf_sp.get();

            //init BufDesc part
            BufDesc* bufdesc = cambuf;

            bufdesc->setType(BufDesc::BUF_TYPE_CAMERAMEMT);
            bufdesc->setKey((void*)i);//use index to mark such buffer node
            bufdesc->setOwner(self_sp);
            bufdesc->setFormat(fmt);
            bufdesc->setSize(width, height);
            bufdesc->setLen(len[0]+len[1]+len[2]);
            bufdesc->setPAddr(paddr[0]);
            bufdesc->setVAddr(vaddr[0]);
            bufdesc->setIMemory(0);
            bufdesc->setPrivate((void*)0);

            //init EngBuf part
            EngBuf* engbuf = &(cambuf->editEngBuf());

            engbuf->setFormat(fmt);
            engbuf->setSize(width, height);
            engbuf->setPAddr(paddr[0],paddr[1],paddr[2]);
            engbuf->setVAddr(vaddr[0],vaddr[1],vaddr[2]);

            engbuf->setAllocLen(len[0],len[1],len[2]);
            engbuf->setFilledLen(len[0],len[1],len[2]);

            engbuf->setStep(step[0],step[1],step[2]);

            engbuf->setOffset(0,0,0);
            engbuf->setUserIndex((int)(bufdesc->getKey()));
            engbuf->setUserData(NULL);
            engbuf->setPrivateIndex(0);
            engbuf->setPrivateData(NULL);
            engbuf->setEnShotInfo(CAM_TRUE);

            if(self->mFD_PMEM >= 0){
                engbuf->setFlag(
                        BUF_FLAG_PHYSICALCONTIGUOUS |
                        BUF_FLAG_L1NONCACHEABLE |
                        BUF_FLAG_L2NONCACHEABLE |
                        BUF_FLAG_UNBUFFERABLE |
                        BUF_FLAG_YUVBACKTOBACK |
                        BUF_FLAG_FORBIDPADDING);
            }
            else{
                engbuf->setFlag(
                        BUF_FLAG_NONPHYSICALCONTIGUOUS |
                        BUF_FLAG_YUVBACKTOBACK |
                        BUF_FLAG_FORBIDPADDING);
            }

            //init EngBufReq part
            EngBufReq* req = &(cambuf->editEngBufReq());

            *req = bufreq;

            OutputQueue.push_back(cambuf_sp);
        }
        return OutputQueue;
    }


    Vector<sp<CamBuf> > BufferHolder::registerANWBuf(const EngBufReq& bufreq, Vector<sp<BufDesc> >& queue)
    {
        int bufcnt = queue.size();
        sp<BufDesc> desc = queue.itemAt(0);
        const char* imageformat = desc->getFormat();
        int width,height;
        int bufferStrideReqX, bufferStrideReqY;
        bool reCalBufLen = false;
        int actualWidth, actualHeight;
        desc->getSize(&width,&height);

        //VPU encoder requires 16 pixel aligment on width and height. so the buffer is allocated on 16 pixel alignment,
        //and EngBufReq should also be 16 pixel alignment.
        CameraSetting::getBufferStrideReq(bufferStrideReqX,bufferStrideReqY);

        if(width % bufferStrideReqX != 0 || height % bufferStrideReqY != 0) {
            reCalBufLen = true;
            actualWidth = ((width-1)/bufferStrideReqX+1)*bufferStrideReqX;
            actualHeight = ((height-1)/bufferStrideReqY+1)*bufferStrideReqY;
            TRACE_D("Eng Buffer request is not match with BufferStrideReq, actual buf wxh:%dx%d",actualWidth,actualHeight);
        }

        sp<BufferHolder> self_sp = new BufferHolder;
        BufferHolder* self = self_sp.get();
        Vector<sp<CamBuf> > OutputQueue;
        OutputQueue.setCapacity(bufcnt);

        for (int i = 0; i < bufcnt; i++) {
            sp<BufDesc> desc = queue.itemAt(i);

            ssize_t offset = 0;
            size_t size = 0;

            int len[3];
            int tmpMin[3];
            if(reCalBufLen == true) {
                bufreq.getMinBufLen(tmpMin+0,tmpMin+1,tmpMin+2);
                len[0] = tmpMin[0]/width*actualWidth/height*actualHeight;
                len[1] = tmpMin[1]/width*actualWidth/height*actualHeight;
                len[2] = tmpMin[2]/width*actualWidth/height*actualHeight;
            }
            else
                bufreq.getMinBufLen(len+0,len+1,len+2);

            int step[3];
            bufreq.getMinStep(step+0, step+1, step+2);//TODO:check step

            //-----------------------------------------------------------------------
            //construct CamBuf
            sp<CamBuf> cambuf_sp = new CamBuf;
            CamBuf* cambuf = cambuf_sp.get();

            //vaddr
            unsigned char* vaddr[3];
            vaddr[0] = (unsigned char*)(desc->getVAddr());
            vaddr[1] = (unsigned char*)((unsigned char*)(vaddr[0]) + len[0]);
            vaddr[2] = (unsigned char*)((unsigned char*)(vaddr[1]) + len[1]);

            unsigned long paddr[3];
            paddr[0] = (unsigned long)(desc->getPAddr());
            paddr[1] = paddr[0] + len[0];
            paddr[2] = paddr[1] + len[1];

            //init BufDesc part
            BufDesc* bufdesc = cambuf;

            bufdesc->setType(desc->getType());
            bufdesc->setKey(desc->getKey());
            bufdesc->setOwner(self_sp);
            bufdesc->setFormat(imageformat);
            bufdesc->setSize(width, height);
            bufdesc->setLen(len[0]+len[1]+len[2]);
            bufdesc->setPAddr(paddr[0]);
            bufdesc->setVAddr(vaddr[0]);
            //bufdesc->setIMemory(buffer);
            bufdesc->setPrivate(desc->getPrivate());

            BufState* state = cambuf;
            *state = *(desc.get());

            //init EngBuf part
            EngBuf* engbuf = &(cambuf->editEngBuf());

            engbuf->setFormat(imageformat);
            engbuf->setSize(width, height);
            engbuf->setPAddr(paddr[0],paddr[1],paddr[2]);
            engbuf->setVAddr(vaddr[0],vaddr[1],vaddr[2]);

            engbuf->setAllocLen(len[0],len[1],len[2]);
            engbuf->setFilledLen(len[0],len[1],len[2]);

            engbuf->setStep(step[0],step[1],step[2]);

            engbuf->setOffset(0,0,0);
            engbuf->setUserIndex(int(desc->getKey()));//XXX
            engbuf->setUserData(NULL);
            engbuf->setPrivateIndex(0);
            engbuf->setPrivateData(NULL);
            engbuf->setEnShotInfo(CAM_TRUE);

            engbuf->setFlag(
                    BUF_FLAG_PHYSICALCONTIGUOUS |
                    BUF_FLAG_L1NONCACHEABLE |
                    BUF_FLAG_L2NONCACHEABLE |
                    BUF_FLAG_UNBUFFERABLE |
                    BUF_FLAG_YUVBACKTOBACK |
                    BUF_FLAG_FORBIDPADDING);

            //init EngBufReq part
            EngBufReq* req = &(cambuf->editEngBufReq());

            *req = bufreq;

            //To Wrap the ANW buffer to camera_memory_t.
            //only for BUF_TYPE_NATIVEWINDOW.
            camera_memory_t* mem = get_camera_memory_t_from_ANW(cambuf);
            cambuf->set_cammem_ANW_wrap(mem);

            OutputQueue.push_back(cambuf_sp);
        }
        return OutputQueue;
    }

    /*
    status_t BufferHolder::registerExternalBuf(const char* fmt, int width, int height,
            unsigned char* vaddr, unsigned long paddr, const void* key)
    {
        EngBufReq bufreq = EngBufreq::getEngBufReq(imageformat, width, height, bufcnt);

        push_back();
    }
    */
    /*
    Vector<sp<CamBuf> >  BufferHolder::registerCamBuf(const CamBuf& buf)
    {
        Vector<sp<CamBuf> > OutputQueue;
        OutputQueue.push_back(buf);
    }
    */
    //for common android usage, only BufDesc is necessary to provide,
    //EngBug & EngBufReq should be processed internally
    /*
    Vector<sp<CamBuf> >  BufferHolder::registerCamBuf(const BufDesc& info)
    {
        int width,height;
        info.getSize(&width,&height);
        const char* format = info.getFormat();
        EngBufReq bufreq = EngBufReq::getEngBufReq(format, width, height, 0);

        //-----------------------------------------------------------------------

        //-----------------------------------------------------------------------
        //prepare all necessary buffer informations
        int len[3];
        bufreq.getMinBufLen(len+0,len+1,len+2);

        unsigned long paddr[3];
        paddr[0] = (unsigned long) (info.getPAddr());
        paddr[1] = (unsigned long) (paddr[0] + len[0]);
        paddr[2] = (unsigned long) (paddr[1] + len[1]);

        unsigned char* vaddr[3];
        vaddr[0] = (unsigned char*)(info.getVAddr());
        vaddr[1] = (unsigned char*)((vaddr[0]) + len[0]);
        vaddr[2] = (unsigned char*)((vaddr[1]) + len[1]);

        int step[3];
        bufreq.getMinStep(step+0, step+1, step+2);//TODO:check step

        const char* fmt = bufreq.getFormat();

        //-----------------------------------------------------------------------
        //construct CamBuf
        CamBuf cambuf;

        //init BufDesc part
        BufDesc& bufdesc = cambuf;
        bufdesc = info;

        //init EngBuf part
        //TODO
        EngBuf& engbuf = cambuf.editEngBuf();

        engbuf.setFormat(format);
        engbuf.setSize(width, height);
        engbuf.setPAddr(paddr[0],paddr[1],paddr[2]);
        engbuf.setVAddr(vaddr[0],vaddr[1],vaddr[2]);

        engbuf.setAllocLen(len[0],len[1],len[2]);
        engbuf.setFilledLen(len[0],len[1],len[2]);

        engbuf.setOffset(0,0,0);
        engbuf.setUserIndex((int)info.getKey());//XXX
        engbuf.setUserData(NULL);
        engbuf.setPrivateIndex(0);
        engbuf.setPrivateData(NULL);
        engbuf.setEnShotInfo(CAM_TRUE);

        engbuf.setFlag(
                BUF_FLAG_PHYSICALCONTIGUOUS |
                BUF_FLAG_L1NONCACHEABLE |
                BUF_FLAG_L2NONCACHEABLE |
                BUF_FLAG_UNBUFFERABLE |
                BUF_FLAG_YUVBACKTOBACK |
                BUF_FLAG_FORBIDPADDING);
        //engbuf.show();

        //init EngBufReq part
        EngBufReq& req = cambuf.editEngBufReq();

        req = bufreq;

        push_back(cambuf);
        return NO_ERROR;
    }
*/

    int BufferHolder::getQueueIndexByKey(const Vector<sp<CamBuf> >& queue, const void* key)
    {
        int cnt=queue.size();
        for(int i=0; i<cnt; i++) {
            if(queue.itemAt(i)->getKey() == key)
                return i;
        }
        TRACE_E("Fail to find key=%p",key);
        return -1;
    }
    //XXX
    int	BufferHolder::getQueueIndexBySP(const Vector<sp<CamBuf> >& queue, const sp<IMemory>& mem)
    {
        return -1;
    }
    int	BufferHolder::getQueueIndexByVAddr(const Vector<sp<CamBuf> >& queue, const void* vaddr)
    {
        int cnt=queue.size();
        for(int i=0; i<cnt; i++) {
            if(queue.itemAt(i)->getVAddr() == vaddr)
                return i;
        }
        TRACE_E("Fail to find vaddr=%p",vaddr);
        return -1;
    }

/*}}}*/

}; //namespace android
