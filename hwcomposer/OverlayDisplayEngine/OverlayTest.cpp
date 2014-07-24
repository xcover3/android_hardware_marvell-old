/*
 * (C) Copyright 2010 Marvell Int32_Ternational Ltd.
 * All Rights Reserved
 *
 * MARVELL CONFIDENTIAL
 * Copyright 2008 ~ 2010 Marvell International Ltd All Rights Reserved.
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Marvell Int32_Ternational Ltd or its
 * suppliers or licensors. Title to the Material remains with Marvell Int32_Ternational Ltd
 * or its suppliers and licensors. The Material contains trade secrets and
 * proprietary and confidential information of Marvell or its suppliers and
 * licensors. The Material is protected by worldwide copyright and trade secret
 * laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Marvell's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other int32_tellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such int32_tellectual property rights must be
 * express and approved by Marvell in writing.
 *
 */
//#define LOG_NDEBUG 0
#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <cutils/ashmem.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

#include "OverlayDevice.h"
#include "IDisplayEngine.h"
#include "V4L2Overlay.h"
#include "phycontmem.h"

#include "fb_ioctl.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DisplayEngineTest"
#endif

#define HANDLE_ERROR(status)                    \
    do{                                         \
        if(NO_ERROR != status) {                \
            LOGE("Function:(%s), Line:(%d), status error!", __FUNCTION__, __LINE__);    \
            goto ERROR;                         \
        }                                       \
    }while(0)


#define ALIGN(m, align)  (((m + align - 1) & ~(align - 1)))
#define ALIGN_4K(m)    ALIGN((m), 0x1000)

using namespace android;

#define TEST_BUFFER_NUMBER 3

int _loadYUV(const char* filename, void* pData, unsigned int size)
{
    FILE*  fp = NULL;
    fp = fopen(filename, "rb");
    if(fp) {
        fread(pData, 1, size, fp);
        fclose(fp);
        return 0;
    } else {
        printf("fail to load YUV data from %s\n",filename);
        return -1;
    }
}

static void _get_time( struct timespec *t ) {
    clock_gettime(CLOCK_REALTIME, t);
}
static double _time_ms( struct timespec *t1, struct timespec *t2 ) {
    double ret = 0;
    if( t2->tv_nsec < t1->tv_nsec ) {
        t2->tv_sec--;
        ret = ( t2->tv_nsec + 1000000000LL ) - t1->tv_nsec;
    }else {
        ret = t2->tv_nsec - t1->tv_nsec;
    }

    ret += ( t2->tv_sec - t1->tv_sec ) * 1000000000LL;

    return ret/1000000;
}

void usage()
{
    printf("ov_test file width height format[0:I420 1:UYVY]\n");
}

void sig_handler( int sig)
{
    if(sig == SIGINT){
        property_set("test.end", "1");
    }
}

static double totalFrames = 0;
struct timespec t1;
struct timespec t2;

const char* DEFAULT_FILENAME =  "/data/output.yuv";
const uint32_t dstWidth = 320;
const uint32_t dstHeight = 240;

#define TIME_INTERVAL 60

int main(int argc, char** argv)
{
    signal(SIGINT, sig_handler);

    int format = HAL_PIXEL_FORMAT_YCbCr_420_P;
    uint32_t srcWidth = 640;
    uint32_t srcHeight = 480;

    const char* pszFileName = DEFAULT_FILENAME;
    if(argc < 5){
        usage();
        exit(1);
    }
    else{
        uint32_t i = 1;
        pszFileName = argv[i++];
        srcWidth = atoi(argv[i++]);
        srcHeight = atoi(argv[i++]);
        format = HAL_PIXEL_FORMAT_YCbCr_420_P;//format = (atoi(argv[i++]) == 0)??
    }


    FILE* fp = fopen(pszFileName, "rb");
    if(NULL == fp){
        LOGE("Can not open file %s", pszFileName);
        return 0;
    }

    unsigned char * input_buf[TEST_BUFFER_NUMBER] = {NULL};
    unsigned char * input_phy_buf[TEST_BUFFER_NUMBER] = {NULL};
    uint16_t solidColor[TEST_BUFFER_NUMBER] = {0xF800, 0x07E0, 0x001F};

    float factor = (format == HAL_PIXEL_FORMAT_YCbCr_420_P) ? 1.5f : 2.0f;
    uint32_t imageSize = ((uint32_t)(srcWidth * srcHeight * factor));
    uint32_t length = ALIGN_4K(imageSize);

    Vector<uint32_t> vFreeBuffer;
    for(uint32_t i = 0; i < TEST_BUFFER_NUMBER; ++i){
        input_buf[i] = (unsigned char *)phy_cont_malloc(length, PHY_CONT_MEM_ATTR_NONCACHED);
        V4L2LOG("Allocated input_buffer[%d] = %p.", i, input_buf[i]);
        if(NULL == input_buf[i]){
            LOGE("Can not alloc enough memory.");
            return -1;
        }

        fread(input_buf[i], 1, imageSize, fp);
        //memset(input_buf[i], 0, imageSize);
        input_phy_buf[i] = (unsigned char*)phy_cont_getpa(input_buf[i]);
        V4L2LOG("Corresponding input_buffer[%d] = %p.", i, input_phy_buf[i]);
        vFreeBuffer.add((uint32_t)input_phy_buf[i]);
    }

    Vector<uint32_t> vFlipedBuf;
    Vector<uint32_t> vReleaseFd;

    status_t status = NO_ERROR;
    const sp<IDisplayEngine> pDisplayEngine = new FBOverlayRef("/dev/graphics/fb1");
    void* vImage = NULL;

    uint32_t nTestFrames = 100;
    char value[PROPERTY_VALUE_MAX];
    property_get("v4l2.test.frame", value, "10000");
    nTestFrames = atoi(value);

    int retry = 10;
    uint32_t FreeBuf[MAX_BUFFER_NUM * 3];
    uint32_t nFreeBuf = MAX_BUFFER_NUM * 3;
    memset(FreeBuf, 0, sizeof(uint32_t) * MAX_BUFFER_NUM * 3);

    int fdBaseLayer = open("/dev/graphics/fb0", O_RDWR);

    bool bSetDMA = false;

    ///< Enter display engine operation.
    property_set("persist.dms.v4l2.unlock", "0");
    V4L2LOG("run in %d.", __LINE__);
    // test overlay v4l2 ioctl sequence
    status = pDisplayEngine->open();
    V4L2LOG("run in %d.", __LINE__);
    HANDLE_ERROR(status);

    for(uint32_t i = 0; i < nTestFrames; ++i){
        V4L2LOG("run in %d.", __LINE__);
        status = pDisplayEngine->setSrcPitch(srcWidth, srcWidth/2, srcWidth/2);
        HANDLE_ERROR(status);

        V4L2LOG("run in %d.", __LINE__);
        status = pDisplayEngine->setSrcCrop(0, 0, srcWidth, srcHeight);
        HANDLE_ERROR(status);

        status = pDisplayEngine->setSrcResolution(srcWidth, srcHeight, format);
        V4L2LOG("run in %d.", __LINE__);
        HANDLE_ERROR(status);

        V4L2LOG("run in %d.", __LINE__);
        //status = pDisplayEngine->setColorKey(DISP_OVLY_PER_PIXEL_ALPHA, 0x0, DISP_OVLY_COLORKEY_DISABLE, 0, 0, 0);
        status = pDisplayEngine->setColorKey(DISP_OVLY_GLOBAL_ALPHA, 0xFF, DISP_OVLY_COLORKEY_DISABLE, 0, 0, 0);
        V4L2LOG("run in %d.", __LINE__);
        HANDLE_ERROR(status);

        V4L2LOG("run in %d.", __LINE__);
        status = pDisplayEngine->setDstPosition(dstWidth, dstHeight, 0, 0);
        V4L2LOG("run in %d.", __LINE__);
        HANDLE_ERROR(status);


        V4L2LOG("run in %d.", __LINE__);
        _get_time( &t1 );

        vImage = NULL;
        if(!vFreeBuffer.isEmpty()){
            vImage = (void*)vFreeBuffer.top();
            vFreeBuffer.pop();
        }else{
            while(vFreeBuffer.isEmpty()){
                sp<Fence> fence = new Fence(vReleaseFd[0]);
                if(fence->wait(2000) != OK){
                    LOGD(">>>>>>>>>>>>>>>>>>>> wait prev buf(%p) time out <<<<<<<<<<<<<<<<<<<<", (void*)vFlipedBuf[0]);
                }else{
                    LOGD(">>>>>>>>>>>>>>>>>>>> wait prev buf(%p) ok!!!!!!! <<<<<<<<<<<<<<<<<<<<", (void*)vFlipedBuf[0]);
                    vFreeBuffer.add(vFlipedBuf[0]);
                    vFlipedBuf.removeItemsAt(0);
                    vReleaseFd.removeItemsAt(0);
                }
            }

            vImage = (void*)vFreeBuffer.top();
            vFreeBuffer.pop();
          }

#if 0
        bool bRetry = true;
        retry = 10;
        while(bRetry && retry)
        {
            retry--;
            LOGD("retry get free list %d times", 10-retry);
            pDisplayEngine->getConsumedImages(FreeBuf, nFreeBuf);
            if(0 < nFreeBuf){
                for(uint32_t k = 0; k < nFreeBuf; k+=3){
                    if(NULL != (void*)FreeBuf[k]){
                        if(vFreeBuffer.indexOf((uint32_t)FreeBuf[k]) == NAME_NOT_FOUND){
                            LOGD("get free buffer(%p)", (void*)FreeBuf[k]);
                            vFreeBuffer.add((uint32_t)FreeBuf[k]);
                            bRetry = false;
                        }else{
                            LOGD("Found a duplicated address %p.", (void*)FreeBuf[k]);
                        }
                    }
                }
            }
        }
#endif

        for(uint32_t index = 0; index < vFreeBuffer.size(); ++index){
            LOGD("vFreeBuffer[%d] = %p.", index, (void*)vFreeBuffer[index]);
        }

        V4L2LOG("run in %d.", __LINE__);
        printf("loop NO.%d \n", i);
        V4L2LOG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> loop NO.%d \n", i);
        if((status = pDisplayEngine->drawImage(vImage, (void*)((uint32_t)vImage + 640 * 480), (void*)((uint32_t)vImage + 640 * 480 * 5 / 4), length, 1)) != NO_ERROR){
            LOGE("Error while draw overlay.");
        }

#if 0
        sp<Fence> fence1 = new Fence(pDisplayEngine->getReleaseFd());
        if(fence1->wait(2000) != OK){
            LOGD(">>>>>>>>>>>>>>>>>>>> wait current buf (%p) time out <<<<<<<<<<<<<<<<<<<<", (void*)vImage);
        }
#endif

        vFlipedBuf.add((uint32_t)vImage);
        vReleaseFd.add(pDisplayEngine->getReleaseFd());

        for(uint32_t j = 0; j < vFlipedBuf.size(); ++j){
            LOGD(">>>>>>>>>>>>>>>>>>>> Current vFlipedBuf[%d] = %p", j, (void*)vFlipedBuf[j]);
        }

        if((!(i % TIME_INTERVAL)) && !((i / TIME_INTERVAL) % 2))
        {
            status = pDisplayEngine->setStreamOn(true);
            HANDLE_ERROR(status);
            bSetDMA = true;

            struct mvdisp_partdisp partialDisplay;
            partialDisplay.id = 0;
            partialDisplay.horpix_start = 0;
            partialDisplay.horpix_end = 1024;
            partialDisplay.vertline_start = 0;
            partialDisplay.vertline_end = 600;
            partialDisplay.color = 0;

//            if (ioctl(fdBaseLayer, FB_IOCTL_GRA_PARTDISP, &partialDisplay) == -1) {
//                ALOGE("ERROR: Fail to set partial display!");
//            }
        }

        usleep(33334); // 30fps

        V4L2LOG("run in %d.", __LINE__);
        totalFrames += 1.0;
        if( i%51 == 0 ) {
            _get_time( &t2 );

            double elapsedTime = _time_ms( &t1, &t2 );
            printf("t2 - t1 = %f\n", elapsedTime );
            double fps = totalFrames / (elapsedTime / 1000);
            printf("FPS = %f\n", fps);
        }

        V4L2LOG("run in %d.", __LINE__);

        if((!(i % TIME_INTERVAL)) && ((i / TIME_INTERVAL) % 2)){
            //status = pDisplayEngine->setStreamOn(false);
            bSetDMA = false;

            struct mvdisp_partdisp partialDisplay;
            memset(&partialDisplay, 0, sizeof(mvdisp_partdisp));
//            if (ioctl(fdBaseLayer, FB_IOCTL_GRA_PARTDISP, &partialDisplay) == -1) {
//                ALOGE("ERROR: Fail to set partial display!");
//            }
        }

        char value[256];

        property_get("test.end", value, "0");
        if(atoi(value) == 1)
            break;
    }

    property_set("test.end", "0");

    struct mvdisp_partdisp partialDisplay;
    memset(&partialDisplay, 0, sizeof(mvdisp_partdisp));

//    if (ioctl(fdBaseLayer, FB_IOCTL_GRA_PARTDISP, &partialDisplay) == -1) {
//        ALOGE("ERROR: Fail to set partial display!");
//    }


    V4L2LOG("run in %d.", __LINE__);
    status = pDisplayEngine->close();
    V4L2LOG("run in %d.", __LINE__);
    HANDLE_ERROR(status);

    V4L2LOG("run in %d.", __LINE__);
    phy_cont_free(input_buf);

    fclose(fp);

    property_set("persist.dms.v4l2.unlock", "1");
    V4L2LOG("run in %d.", __LINE__);
    return 0;

ERROR:
    V4L2LOG("run in %d.", __LINE__);
    for(uint32_t i = 0; i < TEST_BUFFER_NUMBER; ++i){
        phy_cont_free(input_buf[i]);
    }

    V4L2LOG("run in %d.", __LINE__);
    property_set("persist.dms.v4l2.unlock", "1");
    return -1;
}
