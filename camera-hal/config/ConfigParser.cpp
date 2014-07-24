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

#include <expat.h>
#include "ConfigParser.h"
#define LOG_TAG "CameraConfig"

namespace android {

Mutex CameraProfiles::sLock;
bool CameraProfiles::sIsInitialized = false;
CameraProfiles *CameraProfiles::sInstance = NULL;
#define PRINT_PARSER_LOG
#ifdef PRINT_PARSER_LOG
    #define PARSER_LOGD(...) do { ALOGD(__VA_ARGS__); } while(0);
#else
    #define PARSER_LOGD(...) do {} while(0);
#endif
#define PARSER_LOGE(...) do { ALOGE(__VA_ARGS__); } while(0);

CameraProfiles*
CameraProfiles::getInstance()
{
    ALOGV("getInstance");

    Mutex::Autolock lock(sLock);
    if (!sIsInitialized)
    {
        const char *defaultXmlFile = "/etc/camera_profiles.xml";
        FILE *fp = fopen(defaultXmlFile, "r");
        if (fp == NULL)
        {
            PARSER_LOGE("could not find media config xml file");
            sInstance = NULL;
        }
        else
        {
            fclose(fp);
            sInstance = createInstanceFromXmlFile(defaultXmlFile);
            PARSER_LOGD("createInstanceFromXmlFile sucess!.");
            sIsInitialized = true;
        }
    }
    return sInstance;
}

CameraProfiles*
CameraProfiles::createInstanceFromXmlFile(const char *xml)
{
    FILE *fp = NULL;
    CHECK((fp = fopen(xml, "r")));

    XML_Parser parser = ::XML_ParserCreate(NULL);
    CHECK(parser != NULL);

    CameraProfiles *profiles = new CameraProfiles();
    profiles->mBufferReq = NULL;
    ::XML_SetUserData(parser, profiles);
    ::XML_SetElementHandler(parser, startElementHandler, NULL);

    const int BUFF_SIZE = 512;
    for (;;) {
        void *buff = ::XML_GetBuffer(parser, BUFF_SIZE);
        if (buff == NULL) {
            PARSER_LOGE("failed to in call to XML_GetBuffer()");
            delete profiles;
            profiles = NULL;
            goto exit;
        }

        int bytes_read = ::fread(buff, 1, BUFF_SIZE, fp);
        if (bytes_read < 0) {
            PARSER_LOGE("failed in call to read");
            delete profiles;
            profiles = NULL;
            goto exit;
        }

        CHECK(::XML_ParseBuffer(parser, bytes_read, bytes_read == 0));
        // done parsing the xml file;
        if (bytes_read == 0) break;
    }

exit:
    ::XML_ParserFree(parser);
    ::fclose(fp);
    return profiles;
}

void CameraProfiles::startElementHandler(void *userData, const char *name, const char **atts)
{
    CameraProfiles *profiles = (CameraProfiles *) userData;

    if (strcmp("BufferReq", name) == 0)
    {
        createBufferReq(atts, profiles);
    }
    else if (strcmp("CamConfig", name) == 0)
    {
        createCamConfig(atts, profiles);
    }
    else if (strcmp("BoardConfig", name) == 0)
    {
        createBoardConfig(atts, profiles);
    }
    else if (strcmp("PreviewConfig", name) == 0)
    {
        parsePreviewConfig(atts, profiles);
    }
    else if (strcmp("PictureConfig", name) == 0)
    {
        parsePictureConfig(atts, profiles);
    }
    else if (strcmp("VideoRecordingConfig", name) == 0)
    {
        parseVideoConfig(atts, profiles);
    }
    else if (strcmp("PreviewFormat", name) == 0)
    {
        parsePreviewFormats(atts, profiles);
    }
    else if (strcmp("PreviewSize", name) == 0)
    {
        parsePreviewSizes(atts, profiles);
    }
    else if (strcmp("PictureFormat", name) == 0)
    {
        parsePictureFormats(atts, profiles);
    }
    else if (strcmp("PictureSize", name) == 0)
    {
        parsePictureSizes(atts, profiles);
    }
    else if (strcmp("VideoFormat", name) == 0)
    {
        parseVideoFormats(atts, profiles);
    }
    else if (strcmp("VideoSize", name) == 0)
    {
       parseVideoSizes(atts, profiles);
    }
}

int CameraProfiles::getCameraId(const char** atts)
{
    if (!atts[0]) return 0;
    CHECK(!strcmp("camraID", atts[0]));
    return atoi(atts[1]);
}

CameraProfiles::BufferRequirement*
        CameraProfiles::createBufferReq(const char **atts, CameraProfiles *profiles) {
    CHECK(!strcmp("horstride", atts[0]) &&
          !strcmp("verstride",   atts[2]));
    CameraProfiles::BufferRequirement* pBufferReq=
         new CameraProfiles::BufferRequirement(
             atoi(atts[1]), atoi(atts[3]));
    profiles->mBufferReq = pBufferReq;
    return pBufferReq;

}

CameraProfiles::CamConfig*
CameraProfiles::createCamConfig(const char **atts, CameraProfiles *profiles)
{
    CamConfig* pCamConfig = NULL;
    profiles->mCurrentCameraId = getCameraId(atts);
    pCamConfig = new CamConfig(profiles->mCurrentCameraId);
    profiles->mCamConfigs.add(pCamConfig);
    return pCamConfig;
}

//Borad Config section
CameraProfiles::BoardConfig*
CameraProfiles::createBoardConfig(const char **atts, CameraProfiles *profiles)
{
    CHECK(!strcmp("previewbufcnt", atts[0]) &&
          !strcmp("stillbufcnt",   atts[2]) &&
          !strcmp("videobufcnt",   atts[4]) &&
          !strcmp("sensorname",    atts[6]) &&
          !strcmp("isptype",       atts[8]) &&
          !strcmp("productname",   atts[10]));

    CameraProfiles::BoardConfig* boardconfig=
         new CameraProfiles::BoardConfig(profiles->mCurrentCameraId,
             atoi(atts[1]), atoi(atts[3]), atoi(atts[5]), atts[7], atoi(atts[9]), atts[11]);

    size_t nCamConfigs;
    CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
    profiles->mCamConfigs[nCamConfigs - 1]->mBoardConfig = boardconfig;

    PARSER_LOGD("--------Board config-----------");
    PARSER_LOGD("preview buffer count = %d", boardconfig->mPreviewBufCnt);
    PARSER_LOGD("still buffer count = %d", boardconfig->mStillBufCnt);
    PARSER_LOGD("video buffer count = %d", boardconfig->mVideoBufCnt);
    PARSER_LOGD("sensor name  = %s", boardconfig->mSensorName.string());
    PARSER_LOGD("product name = %s", boardconfig->mProductName.string());
    PARSER_LOGD("mIspType = %d", boardconfig->mIspType);
    PARSER_LOGD("mCameraID = %d", boardconfig->mCameraID);

    return boardconfig;
}

CameraProfiles::BoardConfig* CameraProfiles::getBoardConfig(int cameraid)
{
    size_t nCamConfigs;
    CHECK((nCamConfigs = mCamConfigs.size()) >= 1);

    for (unsigned int i = 0; i < nCamConfigs; i++)
    {
        if (mCamConfigs.itemAt(i)->mCameraID == cameraid)
        {
            return mCamConfigs.itemAt(i)->mBoardConfig;
        }
    }

    return NULL;
}

CameraProfiles::BufferRequirement* CameraProfiles::getBufferReq()
{
    return mBufferReq;
}

int CameraProfiles::getBufferStrideReq(int &strideX, int &strideY)
{
    CameraProfiles::BufferRequirement* req = getBufferReq();
    if(req == NULL) {
            return -1;
    }
    strideX = req->mStrideX;
    strideY = req->mStrideY;
    return 0;
}

int CameraProfiles::getBoardConfigByName(int cameraid, const char* name)
{
    CameraProfiles::BoardConfig* boardconfig = getBoardConfig(cameraid);

    if (boardconfig == NULL) return -1;

    if (!strcmp("previewbufcnt", name))
        return boardconfig->mPreviewBufCnt;
    if (!strcmp("stillbufcnt", name))
        return boardconfig->mStillBufCnt;
    if (!strcmp("videobufcnt", name))
        return boardconfig->mVideoBufCnt;
    if (!strcmp("isptype", name))
        return boardconfig->mIspType;

    PARSER_LOGE("The given board config name %s is not found", name);
    return -1;
}

const char* CameraProfiles::getSensorName(int cameraid)
{
    CameraProfiles::BoardConfig* boardconfig = getBoardConfig(cameraid);

    if (boardconfig == NULL) return NULL;
    return boardconfig->mSensorName.string();
}

const char* CameraProfiles::getProductName(int cameraid)
{
    CameraProfiles::BoardConfig* boardconfig = getBoardConfig(cameraid);

    if (boardconfig == NULL) return NULL;
    return boardconfig->mProductName.string();
}

//Preview Config section

void CameraProfiles::parsePreviewConfig(const char **atts, CameraProfiles *profiles)
{
    PARSER_LOGD("--------Preview config-----------");
}

CameraProfiles::PreviewConfig* CameraProfiles::getPreviewConfig(int cameraid)
{
    size_t nCamConfigs;
    CHECK((nCamConfigs = mCamConfigs.size()) >= 1);

    for (unsigned int i = 0; i < nCamConfigs; i++)
    {
        if (mCamConfigs.itemAt(i)->mCameraID == cameraid)
        {
            return mCamConfigs.itemAt(i)->mPreviewConfig;
        }
    }
    return NULL;
}

void CameraProfiles::parsePreviewFormats(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPreviewConfig == NULL )
    {
        CameraProfiles::PreviewConfig* previewconfig=
             new CameraProfiles::PreviewConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mPreviewConfig = previewconfig;
    }

    String8 temp(atts[1]);
    pCamConfig->mPreviewConfig->mPreviewFormats.add(temp);
    int i = pCamConfig->mPreviewConfig->mPreviewFormats.size();
    PARSER_LOGD("Support preivew format: %s", pCamConfig->mPreviewConfig->mPreviewFormats.itemAt(i-1).string());

    return ;
}

Vector<String8>* CameraProfiles::getPreviewFormats(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPreviewConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mPreviewConfig->mPreviewFormats);
}

void CameraProfiles::parsePreviewSizes(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPreviewConfig == NULL)
    {
        CameraProfiles::PreviewConfig* previewconfig=
             new CameraProfiles::PreviewConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mPreviewConfig = previewconfig;
    }

    ImageSize temp(atoi(atts[1]), atoi(atts[3]));
    pCamConfig->mPreviewConfig->mPreviewSizes.add(temp);
    PARSER_LOGD("Support preivew size: %s X %s ", atts[1], atts[3]);

    return ;
}

Vector<ImageSize>* CameraProfiles::getPreviewSizes(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPreviewConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mPreviewConfig->mPreviewSizes);
}

//Picture Config section
void CameraProfiles::parsePictureConfig(const char **atts, CameraProfiles *profiles)
{
    PARSER_LOGD("--------Picture config-----------");
}

CameraProfiles::PictureConfig* CameraProfiles::getPictureConfig(int cameraid)
{
    size_t nCamConfigs;
    CHECK((nCamConfigs = mCamConfigs.size()) >= 1);

    for (unsigned int i = 0; i < nCamConfigs; i++)
    {
        if (mCamConfigs.itemAt(i)->mCameraID == cameraid)
        {
            return mCamConfigs.itemAt(i)->mPictureConfig;
        }
    }
    return NULL;
}

void CameraProfiles::parsePictureFormats(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPictureConfig == NULL)
    {
        CameraProfiles::PictureConfig* picconfig=
             new CameraProfiles::PictureConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mPictureConfig = picconfig;
    }
    String8 temp(atts[1]);
    pCamConfig->mPictureConfig->mPictureFormats.add(temp);
    PARSER_LOGD("Support pciture format: %s", atts[1]);

    return ;
}

void CameraProfiles::parsePictureSizes(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPictureConfig == NULL)
    {
        CameraProfiles::PictureConfig* picconfig=
             new CameraProfiles::PictureConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mPictureConfig = picconfig;
    }
    ImageSize temp(atoi(atts[1]), atoi(atts[3]));
    pCamConfig->mPictureConfig->mPictureSizes.add(temp);
    PARSER_LOGD("Support pciture size: %s", atts[1]);

    return ;
}

Vector<ImageSize>* CameraProfiles::getPictureSizes(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPictureConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mPictureConfig->mPictureSizes);
}

Vector<String8>* CameraProfiles::getPictureFormats(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mPictureConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mPictureConfig->mPictureFormats);
}

//Video Config section
void CameraProfiles::parseVideoConfig(const char **atts, CameraProfiles *profiles)
{
    PARSER_LOGD("--------Video config-----------");
}

CameraProfiles::VideoConfig* CameraProfiles::getVideoConfig(int cameraid)
{
    size_t nCamConfigs;
    CHECK((nCamConfigs = mCamConfigs.size()) >= 1);

    for (unsigned int i = 0; i < nCamConfigs; i++)
    {
        if (mCamConfigs.itemAt(i)->mCameraID == cameraid)
        {
            return mCamConfigs.itemAt(i)->mVideoConfig;
        }
    }
    return NULL;
}

void CameraProfiles::parseVideoFormats(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mVideoConfig == NULL)
    {
        CameraProfiles::VideoConfig* videoconfig=
             new CameraProfiles::VideoConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mVideoConfig = videoconfig;
    }

    String8 temp(atts[1]);
    pCamConfig->mVideoConfig->mVideoFormats.add(temp);
    PARSER_LOGD("Support video format: %s", atts[1]);
    return ;
}

void CameraProfiles::parseVideoSizes(const char **atts, CameraProfiles *profiles)
{
    int camid= profiles->mCurrentCameraId;
    CamConfig* pCamConfig = profiles->mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mVideoConfig == NULL)
    {
        CameraProfiles::VideoConfig* videoconfig=
             new CameraProfiles::VideoConfig(profiles->mCurrentCameraId);
        size_t nCamConfigs;
        CHECK((nCamConfigs = profiles->mCamConfigs.size()) >= 1);
        profiles->mCamConfigs.itemAt(nCamConfigs - 1)->mVideoConfig = videoconfig;
    }

    ImageSize temp(atoi(atts[1]), atoi(atts[3]));
    pCamConfig->mVideoConfig->mVideoSizes.add(temp);
    PARSER_LOGD("Support video size: %s x %s", atts[1], atts[3]);
    return ;
}

Vector<ImageSize>* CameraProfiles::getVideoSizes(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mVideoConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mVideoConfig->mVideoSizes);
}

Vector<String8>* CameraProfiles::getVideoFormats(int camid)
{
    CamConfig* pCamConfig = mCamConfigs.itemAt(camid);

    if (pCamConfig == NULL)
        PARSER_LOGE("Can not find the right CamConfig!!!!");

    if (pCamConfig->mVideoConfig == NULL)
        PARSER_LOGE("preview config is NULL");

    return &(pCamConfig->mVideoConfig->mVideoFormats);
}

}; // namespace android
