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

#ifndef ANDROID_HARDWARE_CAMERA_CONFIG_H
#define ANDROID_HARDWARE_CAMERA_CONFIG_H

#include <stdlib.h>
#include <utils/Log.h>
#include <utils/Vector.h>
#include <cutils/properties.h>

#include <utils/threads.h>
#include <hardware/camera.h>

#include <camera/CameraParameters.h>

#define LITERAL_TO_STRING_INTERNA(x) #x
#define LITERAL_TO_STRING(x) LITERAL_TO_STRING_INTERNA(x)
#define CHECK(x)           \
    LOG_ALWAYS_FATAL_IF(   \
              !(x),        \
              __FILE__ ":" LITERAL_TO_STRING(__LINE__) " " #x)

namespace android {

struct ImageSize
{
    ImageSize() : mWidth(0), mHeight(0){}

    ImageSize(int width, int height)
        : mWidth(width), mHeight(height){}

    ImageSize(const ImageSize& copy) {
        mWidth = copy.mWidth;
        mHeight = copy.mHeight;
    }

    ~ImageSize(){}

    int mWidth;
    int mHeight;
};

class CameraProfiles
{
public:

    static CameraProfiles* getInstance();
    int getBoardConfigByName(int cameraid, const char* name);
    int getBufferStrideReq(int &strideX, int &strideY);
    const char* getSensorName(int cameraid);
    const char* getProductName(int cameraid);

    Vector<ImageSize>* getPreviewSizes(int camid);
    Vector<String8>* getPreviewFormats(int camid);
    Vector<ImageSize>* getPictureSizes(int camid);
    Vector<String8>* getPictureFormats(int camid);
    Vector<ImageSize>* getVideoSizes(int camid);
    Vector<String8>* getVideoFormats(int camid);

private:
    struct BufferRequirement {
            BufferRequirement(int x, int y)
            : mStrideX(x),
              mStrideY(y)
            {}
            ~BufferRequirement( ) {}
        int mStrideX;
        int mStrideY;
    };

    struct BoardConfig
    {
        BoardConfig(int CameraID, int PreviewBufCnt, int StillBufCnt,
                    int VideoBufCnt, const char* SensorName, int IspType,
                    const char* ProductName)
            : mPreviewBufCnt(PreviewBufCnt),
              mStillBufCnt(StillBufCnt),
              mVideoBufCnt(VideoBufCnt),
              mSensorName(SensorName),
              mProductName(ProductName),
              mIspType(IspType),
              mCameraID(CameraID){}

        ~BoardConfig(){}

        int mPreviewBufCnt;
        int mStillBufCnt;
        int mVideoBufCnt;
        String8 mSensorName;
        String8 mProductName;
        int mIspType;
        int mCameraID;
    };

    struct PreviewConfig
    {
        PreviewConfig(int CameraID)
            : mCameraID(CameraID){}

        ~PreviewConfig(){}

        Vector<String8>     mPreviewFormats;
        Vector<ImageSize>   mPreviewSizes;
        int                 mCameraID;
    };

    struct PictureConfig
    {
        PictureConfig(int CameraID)
            : mCameraID(CameraID){}

        ~PictureConfig(){}

        Vector<String8>      mPictureFormats;
        Vector<ImageSize>    mPictureSizes;
        int                  mCameraID;
    };

    struct VideoConfig
    {
        VideoConfig(int CameraID)
            : mCameraID(CameraID){}

        ~VideoConfig(){}

        Vector<String8>       mVideoFormats;
        Vector<ImageSize>    mVideoSizes;
        int                  mCameraID;
    };

    struct CamConfig
    {
        CamConfig(int CameraID)
            : mCameraID(CameraID)
        {
            mBoardConfig = NULL;
            mPreviewConfig = NULL;
            mPictureConfig = NULL;
            mVideoConfig = NULL;
        }

        ~CamConfig()
        {
            delete mBoardConfig;
            delete mPreviewConfig;
            delete mPictureConfig;
            delete mVideoConfig;
        }

        int mCameraID;
        BoardConfig* mBoardConfig;
        PreviewConfig* mPreviewConfig;
        PictureConfig* mPictureConfig;
        VideoConfig* mVideoConfig;
    };

    CameraProfiles(){}
    CameraProfiles(int cameraId){}
    ~CameraProfiles()
    {
        for (unsigned int i=0; i<mCamConfigs.size();i++)
            delete mCamConfigs.itemAt(i);
    }

    static void startElementHandler(void *userData, const char *name, const char **atts);
    static CameraProfiles* createInstanceFromXmlFile(const char *xml);

    static int getCameraId(const char **atts);
    static BufferRequirement* createBufferReq(const char **atts, CameraProfiles *profiles);
    static CamConfig* createCamConfig(const char **atts, CameraProfiles *profiles);
    static BoardConfig* createBoardConfig(const char **atts, CameraProfiles *profiles);
    static void parsePreviewConfig(const char **atts, CameraProfiles *profiles);
    static void parsePictureConfig(const char **atts, CameraProfiles *profiles);
    static void parseVideoConfig(const char **atts, CameraProfiles *profiles);

    BoardConfig* getBoardConfig(int cameraid);
    PreviewConfig* getPreviewConfig(int cameraid);
    PictureConfig* getPictureConfig(int cameraid);
    VideoConfig* getVideoConfig(int cameraid);
    BufferRequirement* getBufferReq();

    static void parsePreviewSizes(const char **atts, CameraProfiles *profiles);
    static void parsePreviewFormats(const char **atts, CameraProfiles *profiles);
    static void parsePictureSizes(const char **atts, CameraProfiles *profiles);
    static void parsePictureFormats(const char **atts, CameraProfiles *profiles);
    static void parseVideoSizes(const char **atts, CameraProfiles *profiles);
    static void parseVideoFormats(const char **atts, CameraProfiles *profiles);

    static bool sIsInitialized;
    static CameraProfiles *sInstance;
    static Mutex sLock;

    Vector<int> mCameraIds;
    int         mCurrentCameraId;
    Vector<CamConfig*> mCamConfigs;
    BufferRequirement* mBufferReq;
};

}; // namespace android
#endif
