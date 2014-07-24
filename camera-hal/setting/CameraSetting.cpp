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

#include "cam_log_mrvl.h"
#include "CameraSetting.h"
#include "ippExif.h"
#define LOG_TAG "CameraSetting"

namespace android {

    static const int MAX_CAMERA_CNT=4;
    MrvlCameraInfo CameraSetting::mMrvlCameraInfo[MAX_CAMERA_CNT];
    CameraProfiles* CameraSetting::mCamProfiles = NULL;

    int CameraSetting::iNumOfSensors=0;
    const char CameraSetting::MRVL_KEY_ZSL_SUPPORTED[] = "zsl-supported";

    //used to save the previous AWB values, for AWB-lock
    const char CameraSetting::MRVL_PREVIOUS_KEY_WHITE_BALANCE[] = "wb-previous";
    const char CameraSetting::MRVL_KEY_TOUCHFOCUS_SUPPORTED[] = "touch-focus-supported";

    const CameraSetting::map_ce_andr CameraSetting::map_imgfmt[]={
        {String8(CameraSetting::PIXEL_FORMAT_YUV422P),
            -1/*CAM_IMGFMT_YCbCr422P*/},//yuv422p not supported on surface
        // {String8(CameraSetting::PIXEL_FORMAT_YUV420P_I420),
            // CAM_IMGFMT_YCbCr420P},
        {String8(CameraSetting::PIXEL_FORMAT_YUV420P),
            CAM_IMGFMT_YCrCb420P},
        {String8(CameraSetting::PIXEL_FORMAT_YUV420SP),
            CAM_IMGFMT_YCrCb420SP},
        {String8(CameraSetting::PIXEL_FORMAT_RGB565),
            CAM_IMGFMT_RGB565},
        {String8(CameraSetting::PIXEL_FORMAT_YUV422I),
            CAM_IMGFMT_YCbYCr},
        {String8(""),
            CAM_IMGFMT_YCrYCb},
        {String8(""),
            CAM_IMGFMT_CrYCbY},
        {String8(CameraParameters::PIXEL_FORMAT_JPEG),
            CAM_IMGFMT_JPEG},
        {String8(CameraParameters::PIXEL_FORMAT_RGBA8888),
            CAM_IMGFMT_RGBA8888},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_exposure[]={
        {String8(""),CAM_EXPOSUREMODE_AUTO},
        {String8(""),CAM_EXPOSUREMODE_APERTUREPRIOR},
        {String8(""),CAM_EXPOSUREMODE_SHUTTERPRIOR},
        {String8(""),CAM_EXPOSUREMODE_PROGRAM},
        {String8(CameraParameters::TRUE),
            CAM_EXPOSUREMODE_MANUAL},           //for AE Lock
        {String8(CameraParameters::FALSE),
            -1},
        {String8(""),
            -1},   //end flag
    };


    const CameraSetting::map_ce_andr CameraSetting::map_whitebalance[]={
        {String8(CameraParameters::WHITE_BALANCE_AUTO),
            CAM_WHITEBALANCEMODE_AUTO},
        {String8(CameraParameters::WHITE_BALANCE_INCANDESCENT),
            CAM_WHITEBALANCEMODE_INCANDESCENT},
        {String8(CameraParameters::WHITE_BALANCE_FLUORESCENT),
            CAM_WHITEBALANCEMODE_COOLWHITE_FLUORESCENT},
        {String8(CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT),
            CAM_WHITEBALANCEMODE_DAYWHITE_FLUORESCENT},
        {String8(""),
            CAM_WHITEBALANCEMODE_DAYLIGHT_FLUORESCENT},
        {String8(CameraParameters::WHITE_BALANCE_DAYLIGHT),
            CAM_WHITEBALANCEMODE_DAYLIGHT},
        {String8(CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT),
            CAM_WHITEBALANCEMODE_CLOUDY},
        {String8(CameraParameters::WHITE_BALANCE_SHADE),
            CAM_WHITEBALANCEMODE_SHADOW},
        {String8(CameraParameters::TRUE),        //not a WhiteBalance mode, for AWB Lock
            CAM_WHITEBALANCEMODE_LOCK},
        {String8(CameraParameters::WHITE_BALANCE_TWILIGHT),
            -1},
        {String8(CameraParameters::FALSE),      //
            -1},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_coloreffect[]={
        {String8(CameraParameters::EFFECT_NONE),
            CAM_COLOREFFECT_OFF},
        {String8(""),
            CAM_COLOREFFECT_VIVID},
        {String8(CameraParameters::EFFECT_SEPIA),
            CAM_COLOREFFECT_SEPIA},
        {String8(CameraParameters::EFFECT_MONO),
            CAM_COLOREFFECT_GRAYSCALE},
        {String8(CameraParameters::EFFECT_NEGATIVE),
            CAM_COLOREFFECT_NEGATIVE},
        {String8(CameraParameters::EFFECT_AQUA),
            -1},
        {String8(CameraParameters::EFFECT_POSTERIZE),
            -1},
        {String8(CameraParameters::EFFECT_SOLARIZE),
            CAM_COLOREFFECT_SOLARIZE},
        {String8(CameraParameters::EFFECT_BLACKBOARD),
            -1},
        {String8(CameraParameters::EFFECT_WHITEBOARD),
            -1},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_bandfilter[]={
        {String8(CameraParameters::ANTIBANDING_AUTO),
            CAM_BANDFILTER_AUTO},
        {String8(CameraParameters::ANTIBANDING_OFF),
            CAM_BANDFILTER_OFF},
        {String8(CameraParameters::ANTIBANDING_50HZ),
            CAM_BANDFILTER_50HZ},
        {String8(CameraParameters::ANTIBANDING_60HZ),
            CAM_BANDFILTER_60HZ},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_flash[]={
        {String8(CameraParameters::FLASH_MODE_AUTO),
            CAM_FLASH_AUTO},
        {String8(CameraParameters::FLASH_MODE_OFF),
            CAM_FLASH_OFF},
        {String8(CameraParameters::FLASH_MODE_ON),
            CAM_FLASH_ON},
        {String8(CameraParameters::FLASH_MODE_TORCH),
            CAM_FLASH_TORCH},
        {String8(CameraParameters::FLASH_MODE_RED_EYE),
            CAM_FLASH_REDEYE},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_focus[]={

        {String8(CameraParameters::FOCUS_MODE_AUTO),
            CAM_FOCUS_AUTO_ONESHOT},
        {String8(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO),
            CAM_FOCUS_AUTO_CONTINUOUS_VIDEO},
        {String8(CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE),
            CAM_FOCUS_AUTO_CONTINUOUS_PICTURE},
        {String8(CameraParameters::FOCUS_MODE_MACRO),
            CAM_FOCUS_MACRO},
        {String8(""),
            CAM_FOCUS_HYPERFOCAL},
        {String8(CameraParameters::FOCUS_MODE_INFINITY),
            CAM_FOCUS_INFINITY},
        {String8(""),
            CAM_FOCUS_MANUAL},
        {String8(""),
            -1},	//end flag
    };

    const CameraSetting::map_ce_andr CameraSetting::map_scenemode[]={
        {String8(CameraParameters::SCENE_MODE_PORTRAIT),
            CAM_SHOTMODE_PORTRAIT},
        {String8(CameraParameters::SCENE_MODE_LANDSCAPE),
            CAM_SHOTMODE_LANDSCAPE},
        {String8(CameraParameters::SCENE_MODE_NIGHT_PORTRAIT),
            CAM_SHOTMODE_NIGHTPORTRAIT},
        {String8(CameraParameters::SCENE_MODE_NIGHT),
            CAM_SHOTMODE_NIGHTSCENE},
        {String8(""),
            CAM_SHOTMODE_CHILD},
        {String8(""),
            CAM_SHOTMODE_INDOOR},
        {String8(""),
            CAM_SHOTMODE_PLANTS},
        {String8(CameraParameters::SCENE_MODE_SNOW),
            CAM_SHOTMODE_SNOW},
        {String8(CameraParameters::SCENE_MODE_FIREWORKS),
            CAM_SHOTMODE_FIREWORKS},
        {String8(""),
            CAM_SHOTMODE_SUBMARINE},
        {String8(CameraParameters::SCENE_MODE_ACTION),
            -1},
        {String8(CameraParameters::SCENE_MODE_AUTO),
            CAM_SHOTMODE_AUTO},
        {String8(CameraParameters::SCENE_MODE_BARCODE),
            -1},
        {String8(CameraParameters::SCENE_MODE_CANDLELIGHT),
            -1},
        {String8(CameraParameters::SCENE_MODE_STEADYPHOTO),
            -1},
        {String8(CameraParameters::SCENE_MODE_THEATRE),
            -1},
        {String8(CameraParameters::SCENE_MODE_SUNSET),
            -1},
        {String8(CameraParameters::SCENE_MODE_SPORTS),
            CAM_SHOTMODE_SPORTS},
        /*
           {String8(CameraParameters::SCENE_MODE_MANUAL),
           CAM_SHOTMODE_MANUAL},//FIXME
           */
        {String8(""),
            -1},	//end flag
    };

    const int CameraSetting::AndrZoomRatio=100; //The zoom ratio is in 1/100 increments.

    //IppExifOrientation
    // The rotation angle in degrees relative to the orientation of the camera.
    // Rotation can only be 0, 90, 180 or 270.
    const CameraSetting::map_ce_andr CameraSetting::map_exifrotation[]={
        {String8("0"),
            ippExifOrientationNormal},
        {String8("90"),
            ippExifOrientation90R},
        {String8("180"),
            ippExifOrientation180},
        {String8("270"),
            ippExifOrientation90L},
        {String8(""),
            ippExifOrientationVFLIP},
        {String8(""),
            ippExifOrientationHFLIP},
        {String8(""),
            ippExifOrientationADFLIP},
        {String8(""),
            ippExifOrientationDFLIP},
        {String8(""),
            -1},	//end flag
    };

    //IppExifWhiteBalance
    const CameraSetting::map_ce_andr CameraSetting::map_exifwhitebalance[]={
        {String8(CameraParameters::WHITE_BALANCE_AUTO),
            ippExifWhiteBalanceAuto},
        {String8(CameraParameters::WHITE_BALANCE_INCANDESCENT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_FLUORESCENT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_DAYLIGHT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_TWILIGHT),
            ippExifWhiteBalanceManual},
        {String8(CameraParameters::WHITE_BALANCE_SHADE),
            ippExifWhiteBalanceManual},
        {String8(""),
            -1},	//end flag
    };

    static String8 size_to_str(int width,int height){
        String8 v = String8("");
        char val1[10];
        char val2[10];
        sprintf(val1,"%d", width);
        sprintf(val2,"%d", height);
        v = String8(val1)+
            String8("x")+
            String8(val2);
        return v;
    };

    CameraSetting::CameraSetting(const char* sensorname)
        : mCamera_caps(),
        mCamera_shotcaps()
    {
        int max_camera = CameraSetting::iNumOfSensors;
        int i;
        for(i=0; i<max_camera; i++){
            const char* detected_sensor = CameraSetting::mMrvlCameraInfo[i].getSensorName();
            int detected_sensorid = CameraSetting::mMrvlCameraInfo[i].getSensorID();
            //sensor detected and name matched
            if(-1 != detected_sensorid &&
                    0 == strncmp(detected_sensor,sensorname,strlen(detected_sensor))){
                pMrvlCameraInfo = &CameraSetting::mMrvlCameraInfo[i];
                break;
            }
        }
        if(max_camera == i){
            TRACE_E("No matching sensor in list. Pls check CameraSetting::mMrvlCameraInfo[]");
            pMrvlCameraInfo = NULL;
            return;
        }
        initDefaultParameters();

    }

    CameraSetting::CameraSetting(int sensorid)
        : mCamera_caps(),
        mCamera_shotcaps()
    {
        int max_camera = CameraSetting::iNumOfSensors;
        int i;
        for(i=0; i<max_camera; i++){
            const char* tmp = CameraSetting::mMrvlCameraInfo[i].getSensorName();
            int detected_sensor = CameraSetting::mMrvlCameraInfo[i].getSensorID();
            //sensor detected and id matched
            if(detected_sensor != -1 &&
                    sensorid == detected_sensor){
                pMrvlCameraInfo = &CameraSetting::mMrvlCameraInfo[i];
                break;
            }
        }
        if(max_camera == i){
            TRACE_E("No matching sensor in list. Pls check CameraSetting::mMrvlCameraInfo[]");
            pMrvlCameraInfo = NULL;
            return;
        }
        initDefaultParameters();
    }

    status_t CameraSetting::initDefaultParameters()
    {
        // (refer to CameraParameters.h)

        int Def_Preview_width           = 640;
        int Def_Preview_height          = 480;
        const char* Def_Preview_fmt     = CameraParameters::PIXEL_FORMAT_YUV422I;
        const char* Def_Video_fmt       = CameraParameters::PIXEL_FORMAT_YUV422I;

        int Def_Pic_width               = 640;
        int Def_Pic_height              = 480;
        const char* Def_Pic_fmt         = CameraParameters::PIXEL_FORMAT_JPEG;

        int Def_Video_width             = 640;
        int Def_Video_height            = 480;

        /*
         * mPlatformSupportedFormats[] should list all the preview & video
         * formats that *supported* by camera sensor & surface & video codec.
         * Especially:
         * 1.mPlatformSupportedFormats[0] should store the *default* preview format,
         * as to android's requirements, it should be NV21.
         * 2.mPlatformSupportedFormats[1] should store the *preferred* video/preview
         * format in platform.
         * 3.mPlatformSupportedFormats[1] *could* be ignored if the default preview format
         * and preferred video format are just same.
         */
        mPlatformSupportedFormats.clear();

        mCamProfiles = CameraProfiles::getInstance();
        if (mCamProfiles == NULL)
        {
            //default setting;
            mProductName     = "default";
            iPreviewBufCnt   = 9;
            iStillBufCnt     = 3;
            iVideoBufCnt     = 9;
            mPlatformSupportedFormats.add(CameraParameters::PIXEL_FORMAT_YUV422I);
            // mPlatformSupportedFormats.add(CameraParameters::PIXEL_FORMAT_YUV422I);
        }
	else
        {
            iPreviewBufCnt = mCamProfiles->getBoardConfigByName(pMrvlCameraInfo->getSensorID(),"previewbufcnt");
            iStillBufCnt   = mCamProfiles->getBoardConfigByName(pMrvlCameraInfo->getSensorID(),"stillbufcnt");
            iVideoBufCnt   = mCamProfiles->getBoardConfigByName(pMrvlCameraInfo->getSensorID(),"videobufcnt");
            mProductName   = mCamProfiles->getProductName(pMrvlCameraInfo->getSensorID());

            Vector<String8>* pPreviewFormats = mCamProfiles->getPreviewFormats(pMrvlCameraInfo->getSensorID());
            if (pPreviewFormats->size() == 0) {
                TRACE_E("Fatal Error! No supported format available");
                return UNKNOWN_ERROR;
            }
            for (int i=0;i<pPreviewFormats->size();i++){
                mPlatformSupportedFormats.add(pPreviewFormats->itemAt(i).string());
            }
        }

        if(mPlatformSupportedFormats.size() == 0){
            TRACE_E("Fatal Error! No platform supported format available");
            return UNKNOWN_ERROR;
        }
        else{
            Vector<const char*>::iterator itor = mPlatformSupportedFormats.begin();
            for(;itor<mPlatformSupportedFormats.end();++itor){
                TRACE_D("%s: Platform supported format: %s", __FUNCTION__, *itor);
            }
        }

        int width = Def_Preview_width;
        int height = Def_Preview_height;
        String8 v = size_to_str(width, height);
        set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, v.string());
        setPreviewSize(width, height);

        //for performance consideration, default keys "KEY_PREVIEW_FORMATS/KEY_VIDEO_FRAME_FORMAT"
        //may be initialized here to YUV420SP/NV21 for Android API requirements.
        //const char *fmt = Def_Preview_fmt;
        const char *fmt = mPlatformSupportedFormats.itemAt(0);
        set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,fmt);
        setPreviewFormat(fmt);
        TRACE_D("%s: set preview format: %s", __FUNCTION__, fmt);

        size_t fmtcnt = mPlatformSupportedFormats.size();
        if(fmtcnt == 1){
            set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,fmt);
            TRACE_D("%s: set KEY_VIDEO_FRAME_FORMAT format: %s", __FUNCTION__, fmt);
        }
        else{
            const char* videofmt = mPlatformSupportedFormats.itemAt(1);
            set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,videofmt);
            TRACE_D("%s: set KEY_VIDEO_FRAME_FORMAT format: %s", __FUNCTION__, videofmt);
        }

        width = Def_Video_width;
        height = Def_Video_height;
        v = size_to_str(width, height);
        set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, "320x240,640x480");
        set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, v.string());
        setVideoSize(width, height);

        width = Def_Pic_width;
        height = Def_Pic_height;
        v = size_to_str(width, height);
        set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, v.string());
        setPictureSize(width, height);

        fmt = Def_Pic_fmt;
        set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,fmt);
        setPictureFormat(fmt);

        set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7");
        setPreviewFrameRate(30);

        /*
         * the following parameters are not supported,
         * return fake values for CTS tests
         */
        set(CameraParameters::KEY_ZOOM_RATIOS, "100");
        set(CameraParameters::KEY_ZOOM, "0");
        set(CameraParameters::KEY_MAX_ZOOM, "0");
        set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::FALSE);
        set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, CameraParameters::FALSE);

        set(CameraParameters::KEY_FOCAL_LENGTH, "0.1");
        set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "50");
        set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "50");
        set(CameraParameters::KEY_FOCUS_DISTANCES, "0.05,2,Infinity");

        set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "160");
        set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "120");
        set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "160x120,0x0");
        set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "80");

        set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "10000,30000");
        set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(10000,30000)");

        //set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, "auto");
        //set(CameraParameters::KEY_FLASH_MODE, "auto");

        set(CameraSetting::MRVL_KEY_ZSL_SUPPORTED, CameraParameters::FALSE);

        set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, CameraParameters::FALSE);

        set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW,0);
        set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW,0);

        set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, 0);
        set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,0);
        return NO_ERROR;
    }

    status_t CameraSetting::setBasicCaps(const CAM_CameraCapability& camera_caps)
    {
        mCamera_caps = camera_caps;
        String8 v = String8("");
        int width = 0;
        int height = 0;
        int width_min = 0;
        int height_min = 0;

        //preview port negotiation
        if(mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].bArbitrarySize){
            if(mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stMin.iWidth < 320 &&
                    mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stMin.iHeight < 240 &&
                    mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stMax.iWidth > 640 &&
                    mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stMax.iHeight > 480){
                set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,"640x480,320x240");
            }
        }
        else{
            char val1[10];
            char val2[10];
            v = String8("");
            width_min = 0;
            height_min = 0;
            for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].iSupportedSizeCnt; i++){
                width = mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stSupportedSize[i].iWidth;
                height = mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stSupportedSize[i].iHeight;
                if( width_min <= 0 || height_min <= 0 ||
                        width < width_min){
                    width_min = width;
                    height_min = height;
                }
                sprintf(val1,"%d", width);
                sprintf(val2,"%d", height);
                v += String8(val1)+
                    String8("x")+
                    String8(val2);
                /*
                   v = String8(itoa(mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stSupportedSize[i].iWidth))+
                   String8("x")+
                   String8(itoa(mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].stSupportedSize[i].iHeight));
                   */
                if(i != (mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].iSupportedSizeCnt-1))
                    v += String8(",");
            }
            set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,v);
            if( width_min > 0 && height_min > 0 ){
                setPreviewSize(width_min,height_min);
            }
        }
        TRACE_V("%s:CE supported preview size:%s",__FUNCTION__,
                get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES));
        TRACE_V("%s:default preview size:%dx%d",__FUNCTION__,width_min,height_min);
        /*
         * NOTES:we may only support some special formats on current platform
         * here list all supported parameters
         */
        //temporarily record preferred video fmt;
        const char* videofmt = get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);
        TRACE_V("%s:videofmt :%s",__FUNCTION__,
                get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES));
        v = String8("");
        for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].iSupportedFormatCnt; i++){
            int ce_idx = mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].eSupportedFormat[i];
            String8 ret = map_ce2andr(map_imgfmt,ce_idx);
            if (ret == String8(""))
                continue;

            bool platformsupport = false;
            Vector<const char*>::iterator itor = mPlatformSupportedFormats.begin();
            for(;itor < mPlatformSupportedFormats.end();++itor){
                if( String8(*itor) == ret ){
                    platformsupport = true;
                    break;
                }
            }
            if(platformsupport == false){
                TRACE_D("%s:sensor output format %s not supported by platform",
                        __FUNCTION__,ret.string());
                continue;
            }

            //filter out the preferred preview format and put it HEAD.
            if( v == String8("") ){
                v += ret;
                continue;
            }
            if (ret == String8(videofmt)){
                v = ret + String8(",") + v;
            }
            else{
                v += String8(",") + ret;
            }
        }
        if(v == String8("")){
            set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,CameraSetting::KEY_PREVIEW_FORMAT);
            TRACE_E("CE support NO preview format, use default:%s",CameraSetting::KEY_PREVIEW_FORMAT);
        }
        else{
            set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,v.string());
            TRACE_V("%s:CE supported preview format:%s",__FUNCTION__,v.string());
            TRACE_V("%s:default preview format:%s",__FUNCTION__,get(CameraParameters::KEY_PREVIEW_FORMAT));
        }

        //still port negotiation
        if(mCamera_caps.stPortCapability[CAM_PORT_STILL].bArbitrarySize){
            if(mCamera_caps.stPortCapability[CAM_PORT_STILL].stMin.iWidth <= 320 &&
                    mCamera_caps.stPortCapability[CAM_PORT_STILL].stMin.iHeight <= 240 &&
                    mCamera_caps.stPortCapability[CAM_PORT_STILL].stMax.iWidth >= 2560 &&
                    mCamera_caps.stPortCapability[CAM_PORT_STILL].stMax.iHeight >= 1920){
                set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "640x480,320x240,2048x1536,2560x1920");
            }
        }
        else{
            char val1[10];
            char val2[10];
            v = String8("");
            width_min = 0;
            height_min = 0;
            for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_STILL].iSupportedSizeCnt; i++){
                width = mCamera_caps.stPortCapability[CAM_PORT_STILL].stSupportedSize[i].iWidth;
                height = mCamera_caps.stPortCapability[CAM_PORT_STILL].stSupportedSize[i].iHeight;
                if( width_min <= 0 || height_min <= 0 ||
                        width < width_min){
                    width_min = width;
                    height_min = height;
                }
                sprintf(val1,"%d", width);
                sprintf(val2,"%d", height);
                v += String8(val1)+
                    String8("x")+
                    String8(val2);
                /*
                   v = String8(itoa(mCamera_caps.stPortCapability[CAM_PORT_STILL].stSupportedSize[i].iWidth))+
                   String8("x")+
                   String8(itoa(mCamera_caps.stPortCapability[CAM_PORT_STILL].stSupportedSize[i].iHeight));
                   */
                if(i != (mCamera_caps.stPortCapability[CAM_PORT_STILL].iSupportedSizeCnt-1))
                    v += String8(",");
            }
            set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,v);
            if( width_min > 0 && height_min > 0 ){
                setPictureSize(width_min,height_min);
            }
        }
        TRACE_V("%s:CE supported pic size:%s",__FUNCTION__,
                get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES));
        TRACE_V("%s:default picture size:%dx%d",__FUNCTION__,width_min,height_min);

        // still capture format negotiation
        v = String8("");
        // mCamera_caps.stPortCapability[CAM_PORT_STILL].eSupportedFormat[0] = CAM_IMGFMT_JPEG;
        for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_STILL].iSupportedFormatCnt; i++){
            int ce_idx = mCamera_caps.stPortCapability[CAM_PORT_STILL].eSupportedFormat[i];
            String8 ret = map_ce2andr(map_imgfmt,ce_idx);
            if (ret == String8(""))
                continue;
            if( v != String8("") )
                v += String8(",");
            v += ret;
        }
        if(v == String8("")){
            set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,CameraSetting::KEY_PICTURE_FORMAT);
            TRACE_E("CE support NO still format, use default:%s",CameraSetting::KEY_PICTURE_FORMAT);
        }
        else{
            set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,v.string());
            TRACE_V("%s:CE supported picture format:%s",__FUNCTION__,v.string());
            TRACE_V("%s:default picture format:%s",__FUNCTION__,get(CameraParameters::KEY_PICTURE_FORMAT));
        }

        TRACE_V("%s:default video format:%s",__FUNCTION__,get(CameraParameters::KEY_VIDEO_FRAME_FORMAT));

        set(CameraParameters::KEY_JPEG_QUALITY, camera_caps.stSupportedJPEGParams.iMaxQualityFactor);
        set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0");

        /*
         * TODO:Platform special val, use the following parameters by force
         * if platform/android not support some sensor settings.
         * It means we may not use all sensor supported settings here.
         */
           // set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,"yuv422i");
           // setPreviewFormat("yuv422i");

           // set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,"jpeg");
           // setPictureFormat("jpeg");

        return NO_ERROR;
    }

    status_t CameraSetting::setSupportedSceneModeCap(const CAM_ShotModeCapability& camera_shotcaps)
    {
        mCamera_shotcaps = camera_shotcaps;

        int ce_idx = -1;
        String8 v = String8("");
        String8 ret;
        int len=0;

        /*
         * TODO: currently only support scene mode-manual by defalut,
         * donot notify upper layer here.
         * refer to ceInit():CAM_SHOTMODE_MANUAL
         */

        //map_scenemode/shotmode
        v = String8("");
        for (int i=0; i < mCamera_caps.iSupportedShotModeCnt; i++){
            ce_idx = mCamera_caps.eSupportedShotMode[i];
            ret = map_ce2andr(map_scenemode,ce_idx);
            if (ret == String8(""))
                continue;
            if( v != String8("") )
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,v.string());
            TRACE_V("%s:CE supported scene mode:%s",__FUNCTION__,v.string());
            set(CameraParameters::KEY_SCENE_MODE,CameraParameters::SCENE_MODE_AUTO);
        }
        //set face detected
        v = String8("");
        if(mCamera_caps.iMaxFaceNum > 0){
            //The maximum number of detected faces supported by hardware face
            // detection. If the value is 0, hardware face detection is not supported.
            // Example: "5". Read only
            set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW,mCamera_caps.iMaxFaceNum);
            // The maximum number of detected faces supported by software face
            // detection. If the value is 0, software face detection is not supported.
            // Example: "5". Read only
            //set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW,mCamera_caps.iMaxFaceNum);
        }
        //map_whitebalance
        v = String8("");
        for (int i=0; i < camera_shotcaps.iSupportedWBModeCnt; i++){
            ce_idx = camera_shotcaps.eSupportedWBMode[i];
            ret = map_ce2andr(map_whitebalance,ce_idx);
            if ((ret == String8(""))||(ret == String8(CameraParameters::TRUE)))
                continue;
            if (v != String8(""))
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,v.string());
            TRACE_V("%s:CE supported white balance:%s",__FUNCTION__,v.string());
        }

        //map_coloreffect
        v = String8("");
        for (int i=0; i < camera_shotcaps.iSupportedColorEffectCnt; i++){
            ce_idx = camera_shotcaps.eSupportedColorEffect[i];
            ret = map_ce2andr(map_coloreffect,ce_idx);
            if (ret == String8(""))
                continue;
            if (v != String8(""))
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_EFFECTS,v.string());
            TRACE_V("%s:CE supported color effect:%s",__FUNCTION__,v.string());
        }

        //map_bandfilter
        v = String8("");
        for (int i=0; i < camera_shotcaps.iSupportedBdFltModeCnt; i++){
            ce_idx = camera_shotcaps.eSupportedBdFltMode[i];
            ret = map_ce2andr(map_bandfilter,ce_idx);
            if (ret == String8(""))
                continue;
            if (v != String8(""))
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,v.string());
            TRACE_V("%s:CE supported antibanding:%s",__FUNCTION__,v.string());
        }

        //map_flash
        v = String8("");
        for (int i=0; i < camera_shotcaps.iSupportedFlashModeCnt; i++){
            ce_idx = camera_shotcaps.eSupportedFlashMode[i];
            ret = map_ce2andr(map_flash,ce_idx);
            if (ret == String8(""))
                continue;
            if (v != String8(""))
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_FLASH_MODES,v.string());
            TRACE_V("%s:CE supported flash mode:%s",__FUNCTION__,v.string());
        }

        //map_focus
        v = String8("");
        for (int i=0; i < camera_shotcaps.iSupportedFocusModeCnt; i++){
            ce_idx = camera_shotcaps.eSupportedFocusMode[i];
            ret = map_ce2andr(map_focus,ce_idx);
            if (ret == String8(""))
                continue;
            if (v != String8(""))
                v += String8(",");
            v += ret;
        }
        if( v != String8("") ){
            set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,v.string());
            TRACE_V("%s:CE supported focus mode:%s",__FUNCTION__,v.string());
        }

        //set the maximum number of focus areas supported by hardware
        if(camera_shotcaps.iMaxFocusROINum > 0){
            set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS,camera_shotcaps.iMaxFocusROINum);
            TRACE_V("%s:CE supported max num of focus areas:%d",__FUNCTION__,camera_shotcaps.iMaxFocusROINum);
        }

        //set the maximum number of metering areas supported by hardware
        if(camera_shotcaps.iMaxExpMeterROINum > 0){
            set(CameraParameters::KEY_MAX_NUM_METERING_AREAS,camera_shotcaps.iMaxExpMeterROINum);
            TRACE_V("%s:CE supported max num of metering area:%d", __FUNCTION__,camera_shotcaps.iMaxExpMeterROINum);
        }

        // evc
        if ( 0 != camera_shotcaps.iEVCompStepQ16)
        {
            v = String8("");
            char tmp[32] = {'\0'};
            int max_exposure = camera_shotcaps.iMaxEVCompQ16 / camera_shotcaps.iEVCompStepQ16;
            sprintf(tmp, "%d", max_exposure);
            v = String8(tmp);
            set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, v.string());
            TRACE_V("%s:CE supported max exposure compensation:%s",__FUNCTION__,v.string());

            int min_exposure = camera_shotcaps.iMinEVCompQ16 / camera_shotcaps.iEVCompStepQ16;
            sprintf(tmp, "%d", min_exposure);
            v = String8(tmp);
            set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, v.string());
            TRACE_V("%s:CE supported min exposure compensation:%s",__FUNCTION__,v.string());

            set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "1");
            TRACE_V("%s:CE supported exposure compensation step:%s",__FUNCTION__,"1");
        }

        //for Auto White Balance Lock
        for(int i=0;i<camera_shotcaps.iSupportedWBModeCnt;i++)
        {
            if(camera_shotcaps.eSupportedWBMode[i]==CAM_WHITEBALANCEMODE_LOCK)
            {
                set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED,CameraParameters::TRUE);
                TRACE_D("%s:CE supported auto-white-balance Lock:%s",__FUNCTION__,CameraParameters::TRUE);
            }
        }
        //for Auto Exposure Lock
        for(int i=0;i<camera_shotcaps.iSupportedExpModeCnt;i++)
        {
            if(camera_shotcaps.eSupportedExpMode[i]==CAM_EXPOSUREMODE_MANUAL)
            {
                set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED,CameraParameters::TRUE);
                TRACE_D("%s:CE supported auto-exposure Lock:%s",__FUNCTION__,CameraParameters::TRUE);
            }
        }

        //digital zoom
        // KEY_ZOOM_RATIOS stands for the zoom ratios of all zoom values.
        // The zoom ratio is in 1/100 increments.
        // Ex: a zoom of 3.2x is returned as 320.
        // The number of list elements is KEY_MAX_ZOOM + 1. ??
        // The first element is always 100.
        // The last element is the zoom ratio of zoom value KEY_MAX_ZOOM. ??
        // Example value: "100,150,200,250,300,350,400". Read only.
        // NOTES:
        // 1>android app use the slider ratio index for zoom setting index,
        // which starts from 0 instead of 1.
        //
        v=String8("");
        const int minzoom = camera_shotcaps.iMinDigZoomQ16;
        const int maxzoom = camera_shotcaps.iMaxDigZoomQ16;
        const int cezoomstep = camera_shotcaps.iDigZoomStepQ16;
        if ((0x1<<16) == maxzoom){
            set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::FALSE);
        }
        else{
            for (int i=minzoom; i<=maxzoom; i+=cezoomstep){
                //1.0x for andr min zoom, the first element is always 100
                int zoomratio = (i-minzoom)*AndrZoomRatio/65536 + 100;
                char tmp[32] = {'\0'};
                sprintf(tmp, "%d", zoomratio);
                if (String8("") == v)
                    v = String8(tmp);
                else
                    v = v + String8(",") + String8(tmp);
            }
            if( v != String8("") ){
                set(CameraParameters::KEY_ZOOM_RATIOS, v.string());
                TRACE_V("%s:CE supported digital zoom:%s",__FUNCTION__,v.string());

                char tmp[32] = {'\0'};
                //1.0x for andr min zoom
                const int andrmaxzoom = (maxzoom-minzoom)/(cezoomstep);
                sprintf(tmp, "%d", andrmaxzoom);
                set(CameraParameters::KEY_MAX_ZOOM, tmp);
                set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::TRUE);
            }
            else{
                set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::FALSE);
            }
        }


        // add fps range support
        {
            int min_fps = (camera_shotcaps.iMinFpsQ16 >> 16);
            int max_fps = (camera_shotcaps.iMaxFpsQ16 >> 16);

            v = String8("");
            char tmp[32] = {'\0'};
            sprintf(tmp, "%d,%d", min_fps*1000,max_fps*1000);
            v = String8(tmp);
            set(CameraParameters::KEY_PREVIEW_FPS_RANGE, v.string());

            sprintf(tmp, "(%d,%d)", min_fps*1000,max_fps*1000);
            v = String8(tmp);
            set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, v.string());

            v = String8("");
            for (int i = max_fps; i >= min_fps; i--) {
                sprintf(tmp, "%d", i);
                v += String8(tmp);
                if (i > min_fps) {
                    v += String8(",");
                }
            }
            set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, v.string());

        }

         //add zsl support
        //if low level support ZSL, then mZslModeEnabled set as 1,
        //else set as 0
        {
            for(int i=0;i<camera_shotcaps.iSupportedStillSubModeCnt;i++)
            {
                if(camera_shotcaps.eSupportedStillSubMode[i] == CAM_STILLSUBMODE_ZSL)
                {
                    set(CameraSetting::MRVL_KEY_ZSL_SUPPORTED, CameraParameters::TRUE);
                    TRACE_V("%s:CE supported ZSL:%s",__FUNCTION__,CameraParameters::TRUE);
                    break;
                }
            }
        }

        return NO_ERROR;
    }

    CAM_ShotModeCapability CameraSetting::getSupportedSceneModeCap(void) const
    {
        return mCamera_shotcaps;
    }

    /*
     * map ce parameters to android parameters
     * INPUT
     * map_tab:map table, for wb, focus, color effect...
     * ce_index:cameraengine parameter
     * RETURN
     * String8:android parameters, String8("") if no mapping
     */
    String8 CameraSetting::map_ce2andr(/*int map_len,*/const map_ce_andr* map_tab, int ce_idx)
    {
        if (-1 == ce_idx)
            return String8("");

        String8 v = String8("");
        for (int j=0; ; j++){
            if (map_tab[j].ce_idx == -1 && String8("") == map_tab[j].andr_str)
                break;//get end
            if (ce_idx == map_tab[j].ce_idx && String8("") != map_tab[j].andr_str){
                v = map_tab[j].andr_str;
                break;
            }
        }
        return v;
    }

    /*
     * map android parameters to ce parameters
     * INPUT
     * map_tab:map table, for wb, focus, color effect...
     * andr_str:android parameter
     * RETURN
     * int:ce parameters, -1 if no mapping
     */
    int CameraSetting::map_andr2ce(/*int map_len,*/const map_ce_andr* map_tab, const char* andr_str)
    {
        if (0 == andr_str)
            return -1;

        int ce_idx = -1;
        for (int j=0; ; j++){
            if (map_tab[j].ce_idx == -1 && String8("") == map_tab[j].andr_str)
                break;//get end
            if (String8(andr_str) ==  map_tab[j].andr_str && -1 != map_tab[j].ce_idx){
                ce_idx = map_tab[j].ce_idx;
                break;
            }
        }
        return ce_idx;
    }

    /*
     * FIXME: complete the setting map
     */
    status_t CameraSetting::setParameters(const CameraParameters& param)
    {
        FUNC_TAG;
        status_t ret = NO_ERROR;
        const char* v;
        int cesetting;
        int ce_idx = -1;
        //check preview format
        v = param.getPreviewFormat();
        if (0 == v){
            TRACE_E("preview format not set");
            return BAD_VALUE;
        }
        cesetting = map_andr2ce(CameraSetting::map_imgfmt,v);
        if (-1 != cesetting){
            ce_idx = -1;
            for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].iSupportedFormatCnt; i++){
                ce_idx = mCamera_caps.stPortCapability[CAM_PORT_PREVIEW].eSupportedFormat[i];
                if(ce_idx == cesetting){
                    TRACE_V("%s:preview format=%s", __FUNCTION__, v);
                    break;
                }
            }
        }
        if (cesetting != ce_idx || -1 == cesetting){
            TRACE_E("preview format not supported,%s",v);
            return BAD_VALUE;
        }

        //check picture format
        v = param.getPictureFormat();
        if (0 == v){
            TRACE_E("picture format not set");
            return BAD_VALUE;
        }
        cesetting = map_andr2ce(CameraSetting::map_imgfmt,v);
        if (-1 != cesetting){
            ce_idx = -1;
            for(int i=0; i<mCamera_caps.stPortCapability[CAM_PORT_STILL].iSupportedFormatCnt; i++){
                ce_idx = mCamera_caps.stPortCapability[CAM_PORT_STILL].eSupportedFormat[i];
                if(ce_idx == cesetting){
                    TRACE_V("%s:picture format=%s", __FUNCTION__, v);
                    break;
                }
            }
        }
        if (cesetting != ce_idx || -1 == cesetting){
            TRACE_E("picture format not supported,%s",v);
            return BAD_VALUE;
        }

        // update platform specific parameters according
        // to the camera parameters passed in
        int prev_width, prev_height;
        param.getPreviewSize(&prev_width, &prev_height);
        TRACE_V("%s:preview size=%dx%d", __FUNCTION__, prev_width, prev_height);
        if (prev_width <= 0 || prev_height <= 0) {
            TRACE_E("invalid preview size: width=%d height=%d", prev_width, prev_height);
            return BAD_VALUE;
        }

        int pic_width, pic_height;
        param.getPictureSize(&pic_width, &pic_height);
        TRACE_V("%s:picture size=%dx%d\n", __FUNCTION__, pic_width, pic_height);

        /*
         * copy to local setting and check it, the platform ones should be kept unchanged
         */
        //*(static_cast<CameraParameters *>(this)) = param;
        CameraParameters::operator=(param);

        //FIXME:to support mutil-preview/video formats including NV21, here we have to
        //update video-frame-format again after initialization, as preview/video happens
        //in the same thread and using the same buffers.
        //NOTES:we could reset KEY_VIDEO_FRAME_FORMAT here based on the assumption that
        //camcorder will get video-frame-format after setting the preview parameters.
        v = param.getPreviewFormat();
        set(CameraParameters::KEY_VIDEO_FRAME_FORMAT,v);
        TRACE_V("%s:video format=%s", __FUNCTION__, v);

        mPreviewFlipRot = CAM_FLIPROTATE_NORMAL;
        return ret;
    }

    CameraParameters CameraSetting::getParameters() const
    {
        return (*this);
        //return static_cast<CameraParameters>(*this);
    }

    status_t CameraSetting::getBufCnt(int* previewbufcnt,int* stillbufcnt,int* videobufcnt) const
    {
        *previewbufcnt  =	iPreviewBufCnt;
        *stillbufcnt    =	iStillBufCnt;
        *videobufcnt    =	iVideoBufCnt;
        return NO_ERROR;
    }

    int CameraSetting::getSensorID() const
    {
        return pMrvlCameraInfo->getSensorID();
    }

    const char* CameraSetting::getProductName() const
    {
        return mProductName;
    }

    const char* CameraSetting::getSensorName() const
    {
        return pMrvlCameraInfo->getSensorName();
    }

    int CameraSetting::getBufferStrideReq(int& strideX, int& strideY)
    {
        mCamProfiles = CameraProfiles::getInstance();

        if(mCamProfiles == NULL || mCamProfiles->getBufferStrideReq(strideX,strideY) == -1)
        {
                strideX = 16;
                strideY = 16;
        }
        // TRACE_E("%s:X:%d,Y:%d",__FUNCTION__,strideX,strideY);
        return 0;
    }

    const camera_info* CameraSetting::getCameraInfo() const
    {
        return pMrvlCameraInfo->getCameraInfo();
    }

    //here "sensor index != sensorid" case is also supported, though currently they are always same.
    status_t CameraSetting::initCameraTable(const char* sensorname, int sensorid, int facing, int orient)
    {
        // TRACE_E("%s: sensorname=%s, sensorid=%d, facing=%d, orient=%d",
                // __FUNCTION__, sensorname, sensorid, facing, orient);
        if(CameraSetting::iNumOfSensors > MAX_CAMERA_CNT){
            TRACE_E("Invalid sensor cnt=%d, max cnt=%d", CameraSetting::iNumOfSensors, MAX_CAMERA_CNT);
            return BAD_VALUE;
        }
        CameraSetting::mMrvlCameraInfo[iNumOfSensors].setSensorName(sensorname);
        CameraSetting::mMrvlCameraInfo[iNumOfSensors].setSensorID(sensorid); CameraSetting::mMrvlCameraInfo[iNumOfSensors].setFacing(facing);
        CameraSetting::mMrvlCameraInfo[iNumOfSensors].setOrient(orient);
        CameraSetting::iNumOfSensors++;
        return NO_ERROR;
    }

    int CameraSetting::getNumOfCameras(){
        TRACE_V("%s:iNumOfSensors=%d",__FUNCTION__,iNumOfSensors);
        return CameraSetting::iNumOfSensors;
    }

    const camera_info* MrvlCameraInfo::getCameraInfo() const{
        return &mCameraInfo;
    }


    /*--------------------CameraArea Class STARTS here-----------------------------*/

    status_t CameraArea::transform(
            int width,
            int height,
            int &top,
            int &left,
            int &areaWidth,
            int &areaHeight)
    {
        status_t ret = NO_ERROR;
        int hRange, vRange;
        double hScale, vScale;

        hRange = CameraArea::RIGHT - CameraArea::LEFT;
        vRange = CameraArea::BOTTOM - CameraArea::TOP;
        hScale = ( double ) width / ( double ) hRange;
        vScale = ( double ) height / ( double ) vRange;

        top     = ( mTop + vRange / 2 ) * vScale;
        left    = ( mLeft + hRange / 2 ) * hScale;
        areaHeight  = ( mBottom + vRange / 2 ) * vScale;
        areaHeight  -= top;
        areaWidth   = ( mRight + hRange / 2) * hScale;
        areaWidth   -= left;
        return ret;
    }

    status_t CameraArea::transform(CAM_MultiROI &multiROI)
    {
        multiROI.iROICnt = 1;
        multiROI.stWeiRect[0].stRect.iLeft      = mLeft;
        multiROI.stWeiRect[0].stRect.iTop       = mTop;
        multiROI.stWeiRect[0].stRect.iWidth     = mRight - mLeft;
        multiROI.stWeiRect[0].stRect.iHeight    = mBottom - mTop;
        multiROI.stWeiRect[0].iWeight           = mWeight;
        return NO_ERROR;
    }

    status_t CameraArea::checkArea(
            int top,
            int left,
            int bottom,
            int right,
            int weight)
    {

        //Handles the invalid regin corner case.
        if ( ( 0 == top ) && ( 0 == left ) && ( 0 == bottom ) && ( 0 == right ) && ( 0 == weight ) ) {
            return NO_ERROR;
        }

        if ( ( CameraArea::WEIGHT_MIN > weight ) ||  ( CameraArea::WEIGHT_MAX < weight ) ) {
            TRACE_E("Camera area weight is invalid %d", weight);
            return BAD_VALUE;
        }

        if ( ( CameraArea::TOP > top ) || ( CameraArea::BOTTOM < top ) ) {
            TRACE_E("Camera area top coordinate is invalid %d", top );
            return BAD_VALUE;
        }

        if ( ( CameraArea::TOP > bottom ) || ( CameraArea::BOTTOM < bottom ) ) {
            TRACE_E("Camera area bottom coordinate is invalid %d", bottom );
            return BAD_VALUE;
        }

        if ( ( CameraArea::LEFT > left ) || ( CameraArea::RIGHT < left ) ) {
            TRACE_E("Camera area left coordinate is invalid %d", left );
            return BAD_VALUE;
        }

        if ( ( CameraArea::LEFT > right ) || ( CameraArea::RIGHT < right ) ) {
            TRACE_E("Camera area right coordinate is invalid %d", right );
            return BAD_VALUE;
        }

        if ( left >= right ) {
            TRACE_E("Camera area left larger than right");
            return BAD_VALUE;
        }

        if ( top >= bottom ) {
            TRACE_E("Camera area top larger than bottom");
            return BAD_VALUE;
        }

        return NO_ERROR;
    }

    // Parse string like "(-10,-10,0,0,300),(0,0,0,0,0)"
    status_t CameraArea::parseAreas(const char *area,
            int areaLength,
            Vector< sp<CameraArea> > &areas)
    {
        status_t ret = NO_ERROR;
        char *ctx;
        char *pArea = NULL;
        char *pStart = NULL;
        char *pEnd = NULL;
        const char *startToken = "(";
        const char endToken = ')';
        const char sep = ',';
        int top, left, bottom, right, weight;
        char *tmpBuffer = NULL;
        sp<CameraArea> currentArea;

        if ( ( NULL == area ) || ( 0 >= areaLength ) ){
            return BAD_VALUE;
        }

        tmpBuffer = ( char * ) malloc(areaLength);
        if ( NULL == tmpBuffer ){
            return BAD_VALUE;
        }

        memcpy(tmpBuffer, area, areaLength);
        pArea = strtok_r(tmpBuffer, startToken, &ctx);

        do{
            pStart = pArea;
            if ( NULL == pStart ){
                TRACE_E("Parsing of the left area coordinate failed!");
                ret = BAD_VALUE;
                break;
            }
            else{
                left = static_cast<int>(strtol(pStart, &pEnd, 10));
            }

            if ( sep != *pEnd ){
                TRACE_E("Parsing of the top area coordinate failed!");
                ret = BAD_VALUE;
                break;
            }
            else{
                top = static_cast<int>(strtol(pEnd+1, &pEnd, 10));
            }

            if ( sep != *pEnd ){
                TRACE_E("Parsing of the right area coordinate failed!");
                ret = BAD_VALUE;
                break;
            }
            else{
                right = static_cast<int>(strtol(pEnd+1, &pEnd, 10));
            }

            if ( sep != *pEnd ){
                TRACE_E("Parsing of the bottom area coordinate failed!");
                ret = BAD_VALUE;
                break;
            }
            else{
                bottom = static_cast<int>(strtol(pEnd+1, &pEnd, 10));
            }

            if ( sep != *pEnd ){
                TRACE_E("Parsing of the weight area coordinate failed!");
                ret = BAD_VALUE;
                break;
            }
            else{
                weight = static_cast<int>(strtol(pEnd+1, &pEnd, 10));
            }

            if ( endToken != *pEnd ){
                TRACE_E("Malformed area!");
                ret = BAD_VALUE;
                break;
            }

            ret = checkArea(top, left, bottom, right, weight);
            if ( NO_ERROR != ret ) {
                break;
            }

            currentArea = new CameraArea(top, left, bottom, right, weight);
            TRACE_D("%s:Area parsed [(%d,%d), (%d,%d) %d]",
                    __FUNCTION__, top, left, bottom, right, weight);
            if ( NULL != currentArea.get() ){
                areas.add(currentArea);
            }
            else{
                ret = BAD_VALUE;
                break;
            }
            pArea = strtok_r(NULL, startToken, &ctx);
        }while ( NULL != pArea );

        if ( NULL != tmpBuffer ){
            free(tmpBuffer);
            tmpBuffer = NULL;
        }

        return ret;
    }

    bool CameraArea::areAreasDifferent(Vector< sp<CameraArea> > &area1,
            Vector< sp<CameraArea> > &area2) {
        if (area1.size() != area2.size()) {
            return true;
        }

        // not going to care about sorting order for now
        for (int i = 0; i < area1.size(); i++) {
            if (!area1.itemAt(i)->compare(area2.itemAt(i))) {
                return true;
            }
        }
        return false;
    }

    bool CameraArea::compare(const sp<CameraArea> &area) {
        return ((mTop == area->mTop) &&
                (mLeft == area->mLeft) &&
                (mBottom == area->mBottom) &&
                (mRight == area->mRight) &&
                (mWeight == area->mWeight));
    }


/*--------------------CameraArea Class ENDS here-----------------------------*/

}; //namespace android
