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

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>

#include <linux/ion.h>
#include <linux/pxa_ion.h>
#include <sys/ioctl.h>

#include <utils/Log.h>
#include <string.h>
#include <stdlib.h>

#include "cam_log.h"
#include "cam_log_mrvl.h"
#include "CameraSetting.h"
#include "ippIP.h"
#include "CameraCtrl.h"
#include "Engine.h"

#define LOG_TAG "CameraMrvl"

#define PROP_TUNING     "service.camera.tuning"         //0,1
#define PROP_W_PREVIEW	"service.camera.width_preview"	//0,>0
#define PROP_H_PREVIEW	"service.camera.height_preview"	//0,>0
#define PROP_W_STILL	"service.camera.width_still"	//0,>0
#define PROP_H_STILL	"service.camera.height_still"	//0,>0
#define PROP_ZSL_MASK	"service.camera.zsl_mask"       //0,>0

namespace android {

    int Engine::getNumberOfCameras(){
        CAM_Error error = CAM_ERROR_NONE;
        CAM_DeviceHandle handle;
        CAM_DeviceProp  stCameraProp[4];

        // get the camera engine handle
        error = CAM_GetHandle(&handle);
        //ASSERT_CAM_ERROR(error);
        if (CAM_ERROR_NONE != error)
            return 0;

        // list all supported sensors
        int iSensorCount;
        error = CAM_SendCommand(handle,
                CAM_CMD_ENUM_SENSORS,
                (CAM_Param)&iSensorCount,
                (CAM_Param)stCameraProp);
        //ASSERT_CAM_ERROR(error);
        if (CAM_ERROR_NONE != error)
            return 0;

        // list all supported sensors
        TRACE_D("All supported sensors:");
        for (int i=0 ; i<iSensorCount ; i++){
            //init sensor table with existing sensorid, name, facing & orientation on platform
            int orient;
            switch(stCameraProp[i].eInstallOrientation)
            {
                case  CAM_FLIPROTATE_NORMAL:
                    orient = 0;
                    break;
                case  CAM_FLIPROTATE_90L:
                    orient = 270;
                    break;
                case  CAM_FLIPROTATE_90R:
                    orient = 90;
                    break;
                case  CAM_FLIPROTATE_180:
                    orient = 180;
                    break;
                default:
                    orient = 0;
                    break;
            }
            CameraSetting::initCameraTable(stCameraProp[i].sName, i, stCameraProp[i].iFacing, orient);
            TRACE_D("\tid=%d,name=%s, face=%d, orient=%d", i, stCameraProp[i].sName, stCameraProp[i].iFacing, orient);
        }

        error = CAM_FreeHandle(&handle);
        //ASSERT_CAM_ERROR(error);
        if (CAM_ERROR_NONE != error)
            return 0;

        return CameraSetting::getNumOfCameras();
    }

    void Engine::getCameraInfo(int cameraId, struct camera_info* cameraInfo)
    {
        CameraSetting camerasetting(cameraId);
        const camera_info* infoptr = camerasetting.getCameraInfo();
        if( NULL == infoptr ){
            TRACE_E("Error occur when get CameraInfo");
        }
        memcpy(cameraInfo, infoptr, sizeof(camera_info));
    }

    Engine::Engine(int cameraID):
        mSetting(cameraID),//if cameraID not specified, using 0 by default
        iExpectedPicNum(1)
        //mState(CAM_STATE_IDLE)
    {
        char value[PROPERTY_VALUE_MAX];
        ceInit(&mCEHandle,&mCamera_caps);
        //initialized default shot capability
        mCamera_shotcaps = mCamera_caps.stSupportedShotParams;
        /*
         * comment this command to use the default setting in camerasetting
         * comment it for debug only
         */
        mSetting.setBasicCaps(mCamera_caps);
        /*
         * choose camera tuning
         * setprop service.camera.tuning x
         * if x is set to 0, no camera tuning will be used
         */
        property_get(PROP_TUNING, value, "1");
        mTuning = atoi(value);
        TRACE_D("%s:"PROP_TUNING"=%d",__FUNCTION__, mTuning);
        if( mTuning != 0 ){
            /*
             * update cameraparameter SUPPORTED KEY
             * all supported tuning parameters from engine are stored in stSupportedShotParams
             */
            mSetting.setSupportedSceneModeCap(mCamera_shotcaps);
            /*
             * query current used tuning parameters from engine,
             * update cameraparameter KEY
             */
            ceGetCurrentSceneModeCap(mCEHandle,mSetting);
        }

        mFaceData.number_of_faces=0;
        mFaceData.faces=NULL;
        int max_face_num = mSetting.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
        if(max_face_num > 0)
            mFaces=( camera_face_t* )malloc(sizeof(camera_face_t)*max_face_num);
        else
            mFaces=NULL;
        /*
         * override preview size/format setting, for debug only
         * setprop service.camera.width_preview x
         * setprop service.camera.height_preview y
         * if x or y is set to 0, we'll use the parameters from class CameraSetting.
         */
        int width=0,height=0;
        property_get(PROP_W_PREVIEW, value, "");
        width = atoi(value);
        TRACE_D("%s:"PROP_W_PREVIEW"=%d",__FUNCTION__, width);
        property_get(PROP_H_PREVIEW, value, "");
        height = atoi(value);
        TRACE_D("%s:"PROP_H_PREVIEW"=%d",__FUNCTION__, height);

        if( width > 0 && height > 0 ){
            mSetting.setPreviewSize(width, height);
            const char* v=mSetting.get(CameraParameters::KEY_PREVIEW_SIZE);
            mSetting.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, v);
        }

        /*
         * override preview size/format setting, for debug only
         * setprop service.camera.width_still x
         * setprop service.camera.height_still y
         * if x or y is set to 0, we'll use the parameters from class CameraSetting.
         */
        width=0,height=0;
        property_get(PROP_W_STILL, value, "0");
        width = atoi(value);
        TRACE_D("%s:"PROP_W_STILL"=%d",__FUNCTION__, width);
        property_get(PROP_H_STILL, value, "0");
        height = atoi(value);
        TRACE_D("%s:"PROP_H_STILL"=%d",__FUNCTION__, height);

        if( width > 0 && height > 0 ){
            mSetting.setPictureSize(width, height);
            const char* v=mSetting.get(CameraParameters::KEY_PICTURE_SIZE);
            mSetting.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, v);
        }

        /*
         * choose ZSL path
         * setprop service.camera.zsl_mask x
         * if x is set to 0, zsl will be disabled; otherwise, zsl will be enabled.
         */
        property_get(PROP_ZSL_MASK, value, "0");
        mStillSubMode[0] = CAM_STILLSUBMODE_ZSL;
        mStillSubMode[1] = CAM_STILLSUBMODE_SIMPLE;
        if (0 == atoi(value)) mStillSubMode[0] = CAM_STILLSUBMODE_SIMPLE;
        TRACE_D("%s:"PROP_ZSL_MASK"=%d",__FUNCTION__, (int)mStillSubMode[0]);

        setParameters(mSetting);
    }

    Engine::~Engine(){
        CAM_Error error = CAM_ERROR_NONE;
        error = CAM_FreeHandle(&mCEHandle);
        ASSERT_CAM_ERROR(error);

        mPreviewBufQueue.clear();

        mStillBufQueue.clear();
        if(mFaces){
        free(mFaces);
        mFaces=NULL;
        }
    }

    //keep event handler function concise, or else it may affect preview performance.
    void Engine::NotifyEvents(CAM_EventId eEventId,void* param,void *pUserData)
    {
        Engine* self=static_cast<Engine*>(pUserData);
        TRACE_V("%s:eEventId=%d,param=%d",__FUNCTION__,eEventId, (int)param);
        if(self->mMsg != NULL){
            switch ( eEventId )
            {
                case CAM_EVENT_FRAME_DROPPED:
                    break;
                case CAM_EVENT_FRAME_READY:
                    TRACE_V("%s:CAM_EVENT_FRAME_READY",__FUNCTION__);
                    break;
                case CAM_EVENT_STILL_SHUTTERCLOSED:
                    TRACE_D("%s:CAM_EVENT_STILL_SHUTTERCLOSED",__FUNCTION__);
                    if (self->mMsg->msgTypeEnabled(CAMERA_MSG_SHUTTER)){
#if 0
                        int w,h;
                        CameraParameters param=self->getParameters();
                        param.getPreviewSize(&w, &h);
                        image_rect_type rawsize;
                        rawsize.width=w;rawsize.height=h;
                        {
                            sp<MsgData> msg = new MsgData;
                            MsgData* ptr    = msg.get();
                            ptr->msgType    = CAMERA_MSG_SHUTTER;
                            ptr->ext1       = (int32_t)(&rawsize);
                            ptr->ext2       = 0;
                            self->mMsg->postMsg(msg);
                        }
#endif
                        {
                            sp<MsgData> msg = new MsgData;
                            MsgData* ptr    = msg.get();
                            ptr->msgType    = CAMERA_MSG_SHUTTER;
                            ptr->ext1       = 0;
                            ptr->ext2       = 0;
                            self->mMsg->postMsg(msg);
                        }
                    }
                    break;
                case CAM_EVENT_IN_FOCUS:
                    break;
                case CAM_EVENT_OUTOF_FOCUS:
                    break;
                case CAM_EVENT_FOCUS_AUTO_STOP:
                    TRACE_D("%s:CAM_EVENT_FOCUS_AUTO_STOP:%d",__FUNCTION__,(int)param);
                    {
                        sp<MsgData> msg = new MsgData;
                        MsgData* ptr    = msg.get();
                        ptr->msgType    = CAMERA_MSG_FOCUS;
                        ptr->ext1       = (((int)param>0)?true:false);
                        ptr->ext2       = 0;
                        self->mMsg->postMsg(msg);
                    }
                    break;
                case CAM_EVENT_STILL_ALLPROCESSED:
                    break;
                case CAM_EVENT_FATALERROR:
                    break;
                case CAM_EVENT_FACE_UPDATE:
                    TRACE_V("%s:CAM_EVENT_FACE_UPDATE",__FUNCTION__);
                    if (self->mMsg->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA)){
                        int max_face_num = self->mSetting.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
                        if ( NULL == self->mFaces || max_face_num <=0 ) {
                            return;
                        }
                        CAM_MultiROI* pFaceROI;
                        pFaceROI=(CAM_MultiROI*)param;
                        //initialize the face structure
                        for(int i = 0; i < max_face_num; i++){
                           self->mFaces[i].rect[0] = 0;
                           self->mFaces[i].rect[1] = 0;
                           self->mFaces[i].rect[2] = 0;
                           self->mFaces[i].rect[3] = 0;
                           self->mFaces[i].score   = 0;
                           self->mFaces[i].id      = 0;
                           self->mFaces[i].left_eye[0]     = CameraArea::INVALID_AREA;
                           self->mFaces[i].left_eye[1]     = CameraArea::INVALID_AREA;
                           self->mFaces[i].right_eye[0]    = CameraArea::INVALID_AREA;
                           self->mFaces[i].right_eye[1]    = CameraArea::INVALID_AREA;
                           self->mFaces[i].mouth[0]        = CameraArea::INVALID_AREA;
                           self->mFaces[i].mouth[1]        = CameraArea::INVALID_AREA;
                        }
                        static bool hasFaceinfo = true;
                        for(int i=0;i<pFaceROI->iROICnt;i++){
                            hasFaceinfo = true;
                            TRACE_D("%s:FaceDetection: facenum=%d, left=%d, top=%d, width=%d, height=%d",
                                    __FUNCTION__, pFaceROI->iROICnt,
                                    pFaceROI->stWeiRect[i].stRect.iLeft,
                                    pFaceROI->stWeiRect[i].stRect.iTop,
                                    pFaceROI->stWeiRect[i].stRect.iWidth,
                                    pFaceROI->stWeiRect[i].stRect.iHeight
                                   );
                            status_t ret = NO_ERROR;
                            ret = CameraArea::checkArea(
                                    pFaceROI->stWeiRect[i].stRect.iTop,
                                    pFaceROI->stWeiRect[i].stRect.iLeft,
                                    pFaceROI->stWeiRect[i].stRect.iTop + pFaceROI->stWeiRect[i].stRect.iHeight,
                                    pFaceROI->stWeiRect[i].stRect.iLeft + pFaceROI->stWeiRect[i].stRect.iWidth,
                                    1);
                            if(NO_ERROR != ret){
                                TRACE_E("detected wrong faceinfo");
                                return;
                            }
                            self->mFaces[i].rect[0] = pFaceROI->stWeiRect[i].stRect.iLeft;
                            self->mFaces[i].rect[1] = pFaceROI->stWeiRect[i].stRect.iTop;
                            self->mFaces[i].rect[2] = self->mFaces[i].rect[0] +
                                pFaceROI->stWeiRect[i].stRect.iWidth;
                            self->mFaces[i].rect[3] = self->mFaces[i].rect[1] +
                                pFaceROI->stWeiRect[i].stRect.iHeight;
                        }
                        self->mFaceData.number_of_faces=pFaceROI->iROICnt;
                        self->mFaceData.faces = self->mFaces;

                        if(true == hasFaceinfo)
                        {
                            camera_memory_t* face_detected = self->mGetCameraMemory(-1, 1, 1, NULL);
                            {
                                sp<MsgData> msg = new MsgData;
                                MsgData* ptr    = msg.get();
                                ptr->msgType    = CAMERA_MSG_PREVIEW_METADATA;
                                ptr->data       = face_detected;
                                ptr->index      = 0;
                                ptr->metadata   = &self->mFaceData;
                                ptr->setListener(msgDone_Cb,face_detected);
                                self->mMsg->postMsg(msg);
                            }                            

                            TRACE_V("%s:Detected facenum = %d",__FUNCTION__,
                                    self->mFaceData.number_of_faces);
                        }
                        if(pFaceROI->iROICnt == 0)
                            hasFaceinfo = false;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void Engine::setCallbacks(sp<CameraMsg>& cameramsg,
            camera_request_memory get_camera_memory,void* user){
        mGetCameraMemory = get_camera_memory;
        mMsg = cameramsg;
        mCallbackCookie = user;
    }

    void Engine::setCallbacks(camera_notify_callback notify_cb,camera_data_callback data_cb,
            camera_request_memory get_camera_memory,void* user){
    }

    /*static*/
    void Engine::msgDone_Cb(void* user)
    {
        camera_memory_t* mem = reinterpret_cast<camera_memory_t*>(user);
        if ( NULL != mem ) 
        {
            mem->release(mem);
        }
        return;
    }

    //check previous state:
    //still: then it wanna switch back to preview, we only flush still port buffer,
    //enqueue preview buffers to preview port.
    //idle: then it's first time preview, we need to alloc preview buffers, switch ce
    //state from idle to preview.
    status_t Engine::startPreview(){
        status_t ret=NO_ERROR;

        if((CAM_STILLSUBMODE_ZSL == mStillSubMode[0]) &&
                strcmp(mSetting.get(CameraSetting::MRVL_KEY_ZSL_SUPPORTED),"true")==0){
            TRACE_D("%s:ZSL mode is enabled",__FUNCTION__);
            ret = ceSetStillSubMode(mCEHandle, CAM_STILLSUBMODE_ZSL);
            if(ret != NO_ERROR){
                TRACE_E("Fail to start ZSL mode");
            }
        }
        else{
            TRACE_D("%s:ZSL mode is disabled",__FUNCTION__);
        }

        return ceStartPreview(mCEHandle);
    }

    status_t Engine::stopPreview(){
        return ceStopPreview(mCEHandle);
    }
    status_t Engine::startFaceDetection(){
        return ceStartFaceDetection(mCEHandle);
    }
    status_t Engine::stopFaceDetection(){
        return ceStopFaceDetection(mCEHandle);
    }
    status_t Engine::startCapture(){
        return ceStartCapture(mCEHandle);//switch to capture mode
    }

    status_t Engine::stopCapture(){
        return ceStopCapture(mCEHandle);
    }

    status_t Engine::startVideo(){
        return ceStartVideo(mCEHandle);
    }

    status_t Engine::stopVideo(){
        return ceStopVideo(mCEHandle);
    }

    status_t Engine::setParameters(CameraParameters param){
        status_t ret=NO_ERROR;
        //store the parameters set from service
        CameraSetting tmpSetting = mSetting;
        ret |= tmpSetting.setParameters(param);
        if(NO_ERROR != ret){
            TRACE_E("Fail in camerasetting.setparameters");
            return ret;
        }

        //set tuning parameters as specified
        if( mTuning != 0 ){
            /*
             * update engine param according to parameters,
             * including flash/whitebalance/coloreffect/focus/antibanding etc.
             */
            ret |= ceUpdateSceneModeSetting(mCEHandle,tmpSetting);

            /*
             * read back setting from engine and update CameraSetting,
             * flash/whitebalance/focus mode may changed with different scene mode.
             * refer to Camera.java
             */
            ret |= ceGetCurrentSceneModeCap(mCEHandle,tmpSetting);

            /*
             * FIXME:cameraengine may NOT support focus mode while scene mode is enabled,
             * but camera app may assume autofocus be supported if scene mode key is set,
             * which may cause camera app quit when doautofocus/takepicture.
             * refer to Camera.java
             */
            if ( 0 != tmpSetting.get(CameraParameters::KEY_SCENE_MODE) &&
                    0 == tmpSetting.get(CameraParameters::KEY_FOCUS_MODE) ){
                tmpSetting.set(CameraParameters::KEY_FOCUS_MODE,
                        CameraParameters::FOCUS_MODE_AUTO);
            }
        }
        //restore back
        if(NO_ERROR == ret)
            mSetting = tmpSetting;
        else
            TRACE_E("Fail to setParameters, mSetting is not updated!");

        return ret;
    }

    const CameraSetting& Engine::getParameters(){
        //CameraParameters params = mSetting.getParameters();
        //return params;
        return mSetting;
    }

#if 0
    status_t Engine::registerPreviewBuffers(sp<BufferHolder> imagebuf){
        //
        mPreviewImageBuf = imagebuf;
        int bufcnt=imagebuf->getBufCnt();
        for (int i=0;i<bufcnt;i++){
            CAM_ImageBuffer* pImgBuf = mPreviewImageBuf->getEngBuf(i);
            if(NULL == pImgBuf){
                TRACE_E("Fail to getEngBuf");
                return UNKNOWN_ERROR;
            }

            /*
               CAM_Error error = CAM_ERROR_NONE;
               error = CAM_SendCommand(mCEHandle,
               CAM_CMD_PORT_ENQUEUE_BUF,
               (CAM_Param)CAM_PORT_PREVIEW,
               (CAM_Param)pImgBuf);
               if(CAM_ERROR_NONE != error)
               return UNKNOWN_ERROR;
               */
        }

        return NO_ERROR;
    }
#endif

    status_t Engine::unregisterPreviewBuffers(){
        //get all buffers back
        CAM_Error error = CAM_ERROR_NONE;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_FLUSH_BUF,
                (CAM_Param)CAM_PORT_PREVIEW,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        //if(CAM_ERROR_NONE != error)
        //    return UNKNOWN_ERROR;
        /*
         * TODO:do mem clear if any memory heap/buffer sp is hold
         * in this class previously, to forcely free all buffers and
         * prepare for next mem alloc.
         */
        mPreviewBufQueue.clear();
        return NO_ERROR;
    }

    //XXX:check buf req/cnt here
    //TODO: we need only the buf index as input after register buffer.
    status_t Engine::enqPreviewBuffer(sp<CamBuf> buf)
    {
        sp<CamBuf> pbuf=NULL;
        ssize_t index = mPreviewBufQueue.indexOfKey(const_cast<void*>(buf->getKey()));
        if(index == NAME_NOT_FOUND){
            mPreviewBufQueue.add(buf->getKey(), buf);
            pbuf = buf;
        }
        else{
            pbuf = mPreviewBufQueue.valueAt(index);
        }

        const EngBuf& engbuf = pbuf->getEngBuf();
        CAM_ImageBuffer* pImgBuf = const_cast<CAM_ImageBuffer*>(engbuf.getCAM_ImageBufferPtr());

        CAM_Error error = CAM_ERROR_NONE;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_ENQUEUE_BUF,
                (CAM_Param)CAM_PORT_PREVIEW,
                (CAM_Param)pImgBuf);
        ASSERT_CAM_ERROR(error);
        return NO_ERROR;
    }

    //status_t Engine::deqPreviewBuffer(int* index){
    status_t Engine::deqPreviewBuffer(void** key)
    {
        CAM_Error error = CAM_ERROR_NONE;
        error = ceGetPreviewFrame(mCEHandle,key);
        if(CAM_ERROR_NONE != error){
            return UNKNOWN_ERROR;
        }
        mPreviewBufQueue.removeItem(*key);
        return NO_ERROR;
    }

    //--------------
#if 0
    status_t Engine::registerStillBuffers(sp<BufferHolder> imagebuf){
        //TODO:

        //
        mStillImageBuf = imagebuf;
        int bufcnt=imagebuf->getBufCnt();
        for (int i=0;i<bufcnt;i++){
            CAM_ImageBuffer* pImgBuf = mStillImageBuf->getEngBuf(i);
            if(NULL == pImgBuf){
                TRACE_E("Fail to getEngBuf");
                return UNKNOWN_ERROR;
            }

            /*
               CAM_Error error = CAM_ERROR_NONE;
               error = CAM_SendCommand(mCEHandle,
               CAM_CMD_PORT_ENQUEUE_BUF,
               (CAM_Param)CAM_PORT_STILL,
               (CAM_Param)pImgBuf);
               if(CAM_ERROR_NONE != error)
               return UNKNOWN_ERROR;
               */
        }
        return NO_ERROR;
    }
#endif
    status_t Engine::unregisterStillBuffers(){
        CAM_Error error = CAM_ERROR_NONE;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_FLUSH_BUF,
                (CAM_Param)CAM_PORT_STILL,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        //if(CAM_ERROR_NONE != error)
        //    return UNKNOWN_ERROR;
        /*
         * TODO:do mem clear if any memory heap/buffer sp is hold
         * in this class previously, to forcely free all buffers and
         * prepare for next mem alloc.
         */
        mStillBufQueue.clear();
        return NO_ERROR;
    }

    //XXX:check buf req/cnt here
    //TODO: we need only the buf index as input after register buffer.
    status_t Engine::enqStillBuffer(sp<CamBuf> buf)
    {
        sp<CamBuf> pbuf=NULL;
        ssize_t index = mStillBufQueue.indexOfKey(const_cast<void*>(buf->getKey()));
        if(index == NAME_NOT_FOUND){
            mStillBufQueue.add(buf->getKey(), buf);
            pbuf = buf;
        }
        else{
            pbuf = mStillBufQueue.valueAt(index);
        }

        const EngBuf& engbuf = pbuf->getEngBuf();
        CAM_ImageBuffer* pImgBuf = const_cast<CAM_ImageBuffer*>(engbuf.getCAM_ImageBufferPtr());

        CAM_Error error = CAM_ERROR_NONE;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_ENQUEUE_BUF,
                (CAM_Param)CAM_PORT_STILL,
                (CAM_Param)pImgBuf);
        ASSERT_CAM_ERROR(error);
        return NO_ERROR;
    }

    //status_t Engine::deqStillBuffer(int* index){
    status_t Engine::deqStillBuffer(void** key)
    {
        CAM_Error error = CAM_ERROR_NONE;
        error = ceGetStillFrame(mCEHandle,key);
        if(CAM_ERROR_NONE != error){
            TRACE_E("ceGetStillFrame fail");
            return UNKNOWN_ERROR;
        }
        else
            mStillBufQueue.removeItem(*key);
        return NO_ERROR;
    }

    status_t Engine::autoFocus()
    {
        //focus mode
        int cesetting                           = -1;
        CAM_ShotModeCapability& camera_shotcaps  = mCamera_shotcaps;

        const char* v=mSetting.get(CameraParameters::KEY_FOCUS_MODE);
        if( 0 == v){
            return UNKNOWN_ERROR;
        }

        //CTS will call autofocus under infinity mode, this is not approved by camera engine.
        //so we use fake message to satisfy CTS test.
        if(0 == strcmp(v,CameraParameters::FOCUS_MODE_INFINITY)){
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

        if(0 != strcmp(v,CameraParameters::FOCUS_MODE_AUTO) &&
                0 != strcmp(v,CameraParameters::FOCUS_MODE_MACRO) &&
                0 != strcmp(v,CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)){
            TRACE_E("app should not call autoFocus in mode %s",v);
            return UNKNOWN_ERROR;
        }
        const char* focusmode = v;
        /*
         * determine the focus is touch-focus or not,
         * if KEY_MAX_NUM_FOCUS_AREAS is set >0, support Touch focus
         * FOCUS_AREAS: (left,top,right,bottom,weight)
         * Coordinates of the rectangle range from -1000 to 1000.
         * (-1000, -1000) is the upper left point. (1000, 1000) is the lower right point.
         * The width and height of focus areas cannot be 0 or negative.
         * send a touch focus spot to engin,since driver support spot focus, not ROI focus,
         */
        int max_focus_area = mSetting.getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS);
        Vector< sp<CameraArea> > tmpAreas;
        TRACE_V("%s:max_focus_area:%d",__FUNCTION__,max_focus_area);
        CAM_Error error = CAM_ERROR_NONE;
        bool hasFocusArea = false;

        CAM_MultiROI multiROI;
        if(max_focus_area > 0)
        {
            status_t ret = NO_ERROR;
            v = mSetting.get(CameraParameters::KEY_FOCUS_AREAS);
            if(0 != v){
                ret = CameraArea::parseAreas(v, strlen(v)+1, tmpAreas);
                if( NO_ERROR != ret || tmpAreas.size() > max_focus_area){
                    TRACE_E("Fail to parseAreas, ret=%d, size=%d", ret, tmpAreas.size());
                    hasFocusArea = false;
                    return BAD_VALUE;
                }
                if( false == tmpAreas[0]->isZeroArea()){
                    hasFocusArea = true;
                    TRACE_D("%s:Setting Focus area %s",__FUNCTION__,v);
                    tmpAreas[0]->transform(multiROI);

                    //currently, firmware support only spot focus,
                    //transform the area to a spot, which is the center of touch area
                    multiROI.stWeiRect[0].stRect.iLeft    += multiROI.stWeiRect[0].stRect.iWidth/2;
                    multiROI.stWeiRect[0].stRect.iTop     += multiROI.stWeiRect[0].stRect.iHeight/2;
                    multiROI.stWeiRect[0].stRect.iWidth   = 1;
                    multiROI.stWeiRect[0].stRect.iHeight  = 1;
                }
            }
        }
        //in AutoFocus mode,check whether there is focus area
        //if yes, set FocusZoneMode as CAM_FOCUSZONEMODE_MANUAL
        //otherwise, set FocusZoneMode as CAM_FOCUSZONEMODE_CENTER
        if(0 == strcmp(focusmode, CameraParameters::FOCUS_MODE_AUTO))
        {
            if(hasFocusArea){
                error = CAM_SendCommand( mCEHandle,
                        CAM_CMD_SET_FOCUSZONE,
                        (CAM_Param)CAM_FOCUSZONEMODE_MANUAL,
                        (CAM_Param)&multiROI );
                if (CAM_ERROR_NONE != error){
                    TRACE_E("set focus zone mode:manual failed!");
                    return UNKNOWN_ERROR;
                }
                focusmode = "TouchFocus";
            }
            else{
                error = CAM_SendCommand( mCEHandle,
                        CAM_CMD_SET_FOCUSZONE,
                        (CAM_Param)CAM_FOCUSZONEMODE_CENTER,
                        UNUSED_PARAM );
                if (CAM_ERROR_NONE != error){
                    TRACE_E("set focus zone mode:center failed!");
                    return UNKNOWN_ERROR;
                }
            }
        }
        error = CAM_SendCommand(mCEHandle,
                    CAM_CMD_START_FOCUS,
                    UNUSED_PARAM,
                    UNUSED_PARAM);
        //ASSERT_CAM_ERROR(error);
        if (CAM_ERROR_NONE != error){
            TRACE_E("start focus failed!");
            return UNKNOWN_ERROR;
        }

        TRACE_D("%s:FocusMode: %s", __FUNCTION__, focusmode);

        return NO_ERROR;
    }

    status_t Engine::cancelAutoFocus()
    {
        CAM_Error error = CAM_ERROR_NONE;
        CAM_CaptureState state;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        if(CAM_CAPTURESTATE_PREVIEW != state && CAM_CAPTURESTATE_VIDEO != state){
            TRACE_D("%s:cancel focus return directly in state %d",__FUNCTION__, state);
            return NO_ERROR;
        }

        const char* v=mSetting.get(CameraParameters::KEY_FOCUS_MODE);
        if( 0 == v){
            return NO_ERROR;
        }

        if(0 == strcmp(v,CameraParameters::FOCUS_MODE_INFINITY)){
            return NO_ERROR;
        }

        if(0 != strcmp(v,CameraParameters::FOCUS_MODE_AUTO) &&
                0 != strcmp(v,CameraParameters::FOCUS_MODE_MACRO) &&
                0 != strcmp(v,CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)){
            TRACE_E("app should not call cancelAutoFocus in mode %s",v);
        }

        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_CANCEL_FOCUS,
                UNUSED_PARAM,
                UNUSED_PARAM);
        return NO_ERROR;
    }

    status_t Engine::getBufCnt(int* previewbufcnt,int* stillbufcnt,int* videobufcnt)const
    {
        return mSetting.getBufCnt(previewbufcnt, stillbufcnt, videobufcnt);
    }


    status_t Engine::initPreviewParameters(){
        status_t error=NO_ERROR;

        //preview size/format should not be changed during preview state
        CAM_CaptureState state;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        if(CAM_CAPTURESTATE_PREVIEW == state){
            TRACE_D("%s:preview size/format should not be changed during preview stage",__FUNCTION__);
            return NO_ERROR;
        }

        /* Set preview parameters */
        CAM_Size szPreviewSize;
        mSetting.getPreviewSize(&szPreviewSize.iWidth,
                &szPreviewSize.iHeight);
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_SET_SIZE,
                (CAM_Param)CAM_PORT_PREVIEW,
                (CAM_Param)&szPreviewSize);
        ASSERT_CAM_ERROR(error);

        //android framework doc requires that preview/picture format
        //is in supported parameters.
        const char* v = mSetting.getPreviewFormat();
        int ePreviewFormat = mSetting.map_andr2ce(CameraSetting::map_imgfmt, v);
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_SET_FORMAT,
                (CAM_Param)CAM_PORT_PREVIEW,
                (CAM_Param)ePreviewFormat);
        ASSERT_CAM_ERROR(error);
        return error;
    }

    //CAM_ImageBufferReq Engine::getPreviewBufReq()
    EngBufReq Engine::getPreviewBufReq()
    {
        CAM_Error error = CAM_ERROR_NONE;

        status_t ret = initPreviewParameters();
        if(NO_ERROR != ret){
            return EngBufReq();
        }

        CAM_ImageBufferReq  engbufreq;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_GET_BUFREQ,
                (CAM_Param)CAM_PORT_PREVIEW,
                (CAM_Param)&engbufreq);
        ASSERT_CAM_ERROR(error);

        return EngBufReq(engbufreq);
    }

    status_t Engine::initStillParameters(){
        CAM_Error error;

        //still size/format should not be changed during still state
        CAM_CaptureState state;
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        if(CAM_CAPTURESTATE_STILL == state){
            TRACE_D("%s:still size/format should not be changed during still stage",__FUNCTION__);
            return NO_ERROR;
        }

        CAM_Size szPictureSize;
        mSetting.getPictureSize(&szPictureSize.iWidth,
                &szPictureSize.iHeight);

        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_SET_SIZE,
                (CAM_Param)CAM_PORT_STILL,
                (CAM_Param)&szPictureSize);
        ASSERT_CAM_ERROR(error);

        int jpeg_quality = mSetting.getInt(CameraParameters::KEY_JPEG_QUALITY);
        if(jpeg_quality > mCamera_caps.stSupportedJPEGParams.iMaxQualityFactor){
             TRACE_E("invalid quality factor=%d. supported range [%d,%d]",
                     jpeg_quality,mCamera_caps.stSupportedJPEGParams.iMaxQualityFactor,
                     mCamera_caps.stSupportedJPEGParams.iMinQualityFactor);
             jpeg_quality = mCamera_caps.stSupportedJPEGParams.iMaxQualityFactor;
         }
        else if(jpeg_quality < mCamera_caps.stSupportedJPEGParams.iMinQualityFactor){
            TRACE_E("invalid quality factor=%d. supported range [%d,%d]",
                    jpeg_quality,mCamera_caps.stSupportedJPEGParams.iMaxQualityFactor,
                    mCamera_caps.stSupportedJPEGParams.iMinQualityFactor);
            jpeg_quality = mCamera_caps.stSupportedJPEGParams.iMinQualityFactor;
        }

        //default jpeg param
        CAM_JpegParam jpegparam = {
            0,	// 420
            jpeg_quality, // quality factor
            CAM_FALSE, // enable exif
            CAM_FALSE, // embed thumbnail
            0, 0,
        };

        const char* v = mSetting.getPictureFormat();
        int ePictureFormat = mSetting.map_andr2ce(CameraSetting::map_imgfmt, v);
        if (CAM_IMGFMT_JPEG == ePictureFormat){
            error = CAM_SendCommand(mCEHandle,
                    CAM_CMD_SET_JPEGPARAM,
                    (CAM_Param)&jpegparam,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_SET_FORMAT,
                (CAM_Param)CAM_PORT_STILL,
                (CAM_Param)ePictureFormat);
        ASSERT_CAM_ERROR(error);
        return error;
    }

    //CAM_ImageBufferReq Engine::getStillBufReq()
    EngBufReq Engine::getStillBufReq()
    {
        initStillParameters();
        CAM_ImageBufferReq  engbufreq;
        CAM_Error error = CAM_SendCommand(mCEHandle,
                CAM_CMD_PORT_GET_BUFREQ,
                (CAM_Param)CAM_PORT_STILL,
                (CAM_Param)&engbufreq);
        ASSERT_CAM_ERROR(error);

        return EngBufReq(engbufreq);
    }
    //----------------------------------------------------------------------------------------
    CAM_Error Engine::ceInit(CAM_DeviceHandle *pHandle,CAM_CameraCapability *pcamera_caps)
    {
        CAM_Error error = CAM_ERROR_NONE;
        int ret;
        CAM_DeviceHandle handle;
        CAM_PortCapability *pCap;

        // get the camera engine handle
        error = CAM_GetHandle(&handle);
        *pHandle = handle;
        ASSERT_CAM_ERROR(error);

        // select the proper sensor id
        int sensorid = mSetting.getSensorID();
        const char* sensorname = mSetting.getSensorName();
        error = CAM_SendCommand(handle,
                CAM_CMD_SET_SENSOR_ID,
                (CAM_Param)sensorid,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        TRACE_D("Current sensor index:");
        TRACE_D("\t%d - %s", sensorid, sensorname);

        error = CAM_SendCommand(handle,
                CAM_CMD_QUERY_CAMERA_CAP,
                (CAM_Param)sensorid,
                (CAM_Param)pcamera_caps);
        ASSERT_CAM_ERROR(error);
#if 1
        error = CAM_SendCommand(handle,
                CAM_CMD_SET_EVENTHANDLER,
                (CAM_Param)NotifyEvents,
                (CAM_Param)this);
        ASSERT_CAM_ERROR(error);
#endif

        error = CAM_SendCommand(handle,
                CAM_CMD_SET_SENSOR_ORIENTATION,
                (CAM_Param)CAM_FLIPROTATE_NORMAL,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        return error;
    }

    status_t Engine::ceUpdateSceneModeSetting(const CAM_DeviceHandle &handle,
            CameraSetting& setting)
    {
        const char* v;
        int scenemode;
        CAM_FocusMode FocusMode;

        status_t ret = NO_ERROR;
        CAM_Error error = CAM_ERROR_NONE;
        CAM_ShotModeCapability camera_shotcaps;

        /*
         * update scene mode as required if engine support
         */
        //current scene mode
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_SHOTMODE,
                (CAM_Param)&scenemode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        //target scene mode
        v = setting.get(CameraParameters::KEY_SCENE_MODE);
        if( 0 != v){
            int scenemode_target = setting.map_andr2ce(CameraSetting::map_scenemode, v);

            //update scene mode if the target scenemode is different from current
            if (scenemode_target != scenemode &&
                    scenemode_target != -1){
                //update ce scene mode
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_SHOTMODE,
                        (CAM_Param)scenemode_target,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);
                scenemode = scenemode_target;
            }
        }
        TRACE_D("%s:scene mode=%s", __FUNCTION__, v);
        //query tuning parameters supported in current scene mode
        if( 0 != v){
            error = CAM_SendCommand(handle,
                    CAM_CMD_QUERY_SHOTMODE_CAP,
                    (CAM_Param)scenemode,
                    (CAM_Param)&camera_shotcaps);
            ASSERT_CAM_ERROR(error);
            //update shot capability under new shot mode.
            mCamera_shotcaps = camera_shotcaps;
        }
        else{
            TRACE_E("No valid scene mode, return.");
            ret = BAD_VALUE;
            return ret;
        }

        /*
         * update other parameters according to current used scene mode
         */
        int cesetting;
        int ce_idx;

        /*
         * update focus mode as required if engine support
         */
        //current focus mode
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_FOCUSMODE,
                (CAM_Param)&FocusMode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        //target focus mode
        v = setting.get(CameraParameters::KEY_FOCUS_MODE);
        if( 0 != v){
            int cesetting = setting.map_andr2ce(CameraSetting::map_focus, v);
            //update focus mode if the target focus mode is different from current
            ce_idx = -1;
            for(int i=0; i<mCamera_shotcaps.iSupportedFocusModeCnt; i++){
                ce_idx = mCamera_shotcaps.eSupportedFocusMode[i];
                if(ce_idx == cesetting){
                    TRACE_D("%s:focus mode=%s", __FUNCTION__, v);
                    break;
                }
            }
            if (cesetting != ce_idx || -1 == cesetting){
                TRACE_E("focus mode not supported,%s",v);
                return BAD_VALUE;
            }

            if(cesetting != FocusMode){
                //update ce focus mode
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_FOCUSMODE,
                        (CAM_Param)cesetting,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);
            }
        }

        //focus area
        int max_focus_area;
        Vector< sp<CameraArea> > tmpAreas;

        v = setting.get(CameraParameters::KEY_FOCUS_AREAS);
        max_focus_area = setting.getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS);

        if(max_focus_area > 0 && 0 != v){
            ret = CameraArea::parseAreas(v,strlen(v)+1,tmpAreas);
            if(NO_ERROR == ret && max_focus_area >= tmpAreas.size()){
                TRACE_D("%s:Focus area %s",__FUNCTION__,v);
            }
            else{
                TRACE_E("%s: parse area error,focus area is %s",__FUNCTION__,v)
                ret = BAD_VALUE;
                return ret;
            }
        }

        /*
         * auto white balance and AWB-lock setting
         * 1. if KEY_AUTO_WHITEBALANCE_LOCK is not set or set to false,
         *      awb value will always be upated.
         * 2. otherwise, check whether KEY_WHITE_BALANCE is changed.
         *    2.1. If yes, update awb value and reset KEY_AUTO_WHITEBALANCE_LOCK to false.
         *    2.2. if no, set awb-lock as true.
         */
        const char* v_awblock = NULL;
        const char* v_awb = NULL;
        const char* awb_pre = NULL;
        v_awblock = setting.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);
        v_awb = setting.get(CameraParameters::KEY_WHITE_BALANCE);
        awb_pre = setting.get(CameraSetting::MRVL_PREVIOUS_KEY_WHITE_BALANCE);

        //final awb string set to engine
        const char* awb_target = v_awb;

        if( v_awblock != 0 && awb_pre != 0 && v_awb != 0 &&
                strcmp(CameraParameters::TRUE, v_awblock) == 0 &&
                strcmp(v_awb, awb_pre) == 0){
            awb_target = v_awblock;
        }
        setting.set(CameraSetting::MRVL_PREVIOUS_KEY_WHITE_BALANCE,v_awb);

        cesetting = setting.map_andr2ce(CameraSetting::map_whitebalance, awb_target);
        if (-1 != cesetting){
            for (int i=0; i < camera_shotcaps.iSupportedWBModeCnt; i++){
                ce_idx = camera_shotcaps.eSupportedWBMode[i];
                if(ce_idx == cesetting){
                    error = CAM_SendCommand(handle,
                            CAM_CMD_SET_WHITEBALANCEMODE,
                            (CAM_Param)cesetting,
                            UNUSED_PARAM);
                    ASSERT_CAM_ERROR(error);
                    TRACE_D("%s:white balance=%s", __FUNCTION__, v_awb);
                    break;
                }
            }
        }

        //effect setting
        v = setting.get(CameraParameters::KEY_EFFECT);
        if( 0 != v){
            cesetting = setting.map_andr2ce(CameraSetting::map_coloreffect, v);
            if (-1 != cesetting){
                for (int i=0; i < camera_shotcaps.iSupportedColorEffectCnt; i++){
                    ce_idx = camera_shotcaps.eSupportedColorEffect[i];
                    if(ce_idx == cesetting){
                        error = CAM_SendCommand(handle,
                                CAM_CMD_SET_COLOREFFECT,
                                (CAM_Param)cesetting,
                                UNUSED_PARAM);
                        ASSERT_CAM_ERROR(error);
                        TRACE_D("%s:color effect=%s", __FUNCTION__, v);
                    }
                }
            }
        }

        //bandfilter setting
        v = setting.get(CameraParameters::KEY_ANTIBANDING);
        if( 0 != v){
            cesetting = setting.map_andr2ce(CameraSetting::map_bandfilter, v);
            if (-1 != cesetting){
                for (int i=0; i < camera_shotcaps.iSupportedBdFltModeCnt; i++){
                    ce_idx = camera_shotcaps.eSupportedBdFltMode[i];
                    if(ce_idx == cesetting){
                        error = CAM_SendCommand(handle,
                                CAM_CMD_SET_BANDFILTER,
                                (CAM_Param)cesetting,
                                UNUSED_PARAM);
                        ASSERT_CAM_ERROR(error);
                        TRACE_D("%s:antibanding=%s", __FUNCTION__, v);
                    }
                }
            }
        }

        //target flash mode
        v = setting.get(CameraParameters::KEY_FLASH_MODE);
        if( 0 != v){
            cesetting = setting.map_andr2ce(CameraSetting::map_flash, v);
            if (-1 != cesetting){
                for (int i=0; i < camera_shotcaps.iSupportedFlashModeCnt; i++){
                    ce_idx = camera_shotcaps.eSupportedFlashMode[i];
                    if(ce_idx == cesetting){
                        error = CAM_SendCommand(handle,
                                CAM_CMD_SET_FLASHMODE,
                                (CAM_Param)cesetting,
                                UNUSED_PARAM);
                        ASSERT_CAM_ERROR(error);
                        TRACE_D("%s:flash mode=%s", __FUNCTION__, v);
                        break;
                    }
                }
            }
            if (cesetting != ce_idx || -1 == cesetting){
                TRACE_E("flash mode not supported,%s",v);
                //check weather flashmode be supported or not in platform
                //if flashmode not be supported,return BAD_VALUE will cause 3rd part apk can not work.
                if(camera_shotcaps.iSupportedFlashModeCnt > 0)
                    return  BAD_VALUE;
            }
        }

        //EVC
        if ( 0 != camera_shotcaps.iEVCompStepQ16){
            v = setting.get(CameraParameters::KEY_EXPOSURE_COMPENSATION);
            if( 0 != v){
                int exposureQ16 = atoi(v) * camera_shotcaps.iEVCompStepQ16;
                // verify the range of the settings
                if ((exposureQ16 <= camera_shotcaps.iMaxEVCompQ16) &&
                        (exposureQ16 >= camera_shotcaps.iMinEVCompQ16) ){
                    error = CAM_SendCommand(handle,
                            CAM_CMD_SET_EVCOMPENSATION,
                            (CAM_Param)exposureQ16,
                            UNUSED_PARAM);
                    ASSERT_CAM_ERROR(error);
                    TRACE_D("%s:exposure compensation=%s", __FUNCTION__, v);
                }
            }
        }

        //auto exposure mode lock setting
        //if KEY_AUTO_EXPOSURE_LOCK be set as true, AE lock
        //else set the default AE mode: AUTO mode
        //During Lock, the EV Compensation value can be set
        v = setting.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
        if(0 != v && strcmp(CameraParameters::TRUE, v) == 0){
            cesetting = setting.map_andr2ce(CameraSetting::map_exposure,v);
        }
        else
            cesetting = CAM_EXPOSUREMODE_AUTO; //in default, auto mode be set

        if( 0 != v ){
            if(-1!=cesetting){   //AE Lock as true
                for(int i=0;i<camera_shotcaps.iSupportedExpModeCnt; i++){
                    ce_idx = camera_shotcaps.eSupportedExpMode[i];
                    if(ce_idx == cesetting){
                        error = CAM_SendCommand(handle,
                                CAM_CMD_SET_EXPOSUREMODE,
                                (CAM_Param)cesetting,
                                UNUSED_PARAM);
                        ASSERT_CAM_ERROR(error);
                        TRACE_D("%s:auto exposure lock=%s", __FUNCTION__, v);
                    }
                }
            }
        }

        //------------------------------------------------------------------------------
        //update Exposure Meter Area
        //when exposuremode is manual, the metering mode not allowed to be changed
        int exposuremode = -1;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_EXPOSUREMODE,
               (CAM_Param)&exposuremode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if(exposuremode != CAM_EXPOSUREMODE_MANUAL)
        {
            //check meter area
            tmpAreas.clear();
            CAM_MultiROI multiROI;
            v = setting.get(CameraParameters::KEY_METERING_AREAS);
            int max_meter_area = setting.getInt(CameraParameters::KEY_MAX_NUM_METERING_AREAS);

            if(max_meter_area > 0 && 0 != v){
                ret = CameraArea::parseAreas(v,strlen(v)+1,tmpAreas);
                if(NO_ERROR == ret && max_meter_area >= tmpAreas.size()){
                    TRACE_D("%s:Meter area %s",__FUNCTION__,v);
                }
                else{
                    TRACE_E("%s: parse area error, meter area is %s",__FUNCTION__,v);
                    ret = BAD_VALUE;
                    return ret;
                }

                //update exposure meter mode through CE API
                if(false == tmpAreas[0]->isZeroArea()) {
                    //set manaul mode when metering area is not zero area
                    tmpAreas[0]->transform(multiROI);
                    ce_idx = -1;
                    for(int i=0;i<camera_shotcaps.iSupportedExpMeterCnt; i++){
                        ce_idx = camera_shotcaps.eSupportedExpMeter[i];
                        if(camera_shotcaps.eSupportedExpMeter[i] == CAM_EXPOSUREMETERMODE_MANUAL){
                            error = CAM_SendCommand(handle,
                                    CAM_CMD_SET_EXPOSUREMETERMODE,
                                    (CAM_Param)CAM_EXPOSUREMETERMODE_MANUAL,
                                    &multiROI);
                            ASSERT_CAM_ERROR(error);
                            break;
                        }
                    }
                    if(ce_idx != CAM_EXPOSUREMETERMODE_MANUAL){
                        TRACE_E("Not Supported Metering mode:manual mode in current scene mode");
                    }
                }
                else{
                    //set auto metering mode when metering area is zero area
                    ce_idx = -1;
                    for(int i=0;i<camera_shotcaps.iSupportedExpMeterCnt; i++){
                        ce_idx = camera_shotcaps.eSupportedExpMeter[i];
                        if(camera_shotcaps.eSupportedExpMeter[i] == CAM_EXPOSUREMETERMODE_AUTO){
                            error = CAM_SendCommand(handle,
                                    CAM_CMD_SET_EXPOSUREMETERMODE,
                                    (CAM_Param)CAM_EXPOSUREMETERMODE_AUTO,
                                    UNUSED_PARAM);
                            ASSERT_CAM_ERROR(error);
                            break;
                        }
                    }
                }
            }
        }

        //digital zoom
        const int minzoom = camera_shotcaps.iMinDigZoomQ16;
        const int maxzoom = camera_shotcaps.iMaxDigZoomQ16;
        const int cezoomstep = camera_shotcaps.iDigZoomStepQ16;
        int cedigzoom_pre = -1;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_DIGZOOM,
                (CAM_Param)&cedigzoom_pre,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        if((0x1<<16) != maxzoom){
            v = setting.get(CameraParameters::KEY_ZOOM);
            int cedigzoom = minzoom + atoi(v) * cezoomstep;
            if (cedigzoom < minzoom){
                TRACE_E("Invalid zoom value:%d. set to min:%d",cedigzoom,minzoom);
                cedigzoom = minzoom;
                ret = BAD_VALUE;
            }
            else if (cedigzoom > maxzoom){
                TRACE_E("Invalid zoom value:%d. set to max:%d",cedigzoom,maxzoom);
                cedigzoom = maxzoom;
                ret = BAD_VALUE;
            }
            if(cedigzoom_pre != cedigzoom){
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_DIGZOOM,
                        (CAM_Param)cedigzoom,
                        UNUSED_PARAM );
                ASSERT_CAM_ERROR(error);
                TRACE_D("%s:digital zoom=%s", __FUNCTION__, v);
            }
        }

        // check fps range
        {
            int min_fps, max_fps;
            setting.getPreviewFpsRange(&min_fps, &max_fps);
            if((min_fps<0) || (max_fps<0) || (min_fps > max_fps)){
                TRACE_E("invalid fps range: min_fps=%d, max_fps=%d",
                        min_fps,max_fps);
                ret = BAD_VALUE;
            }
        }
#if 1
        //fps
        {
            const int framerate = setting.getPreviewFrameRate();
            const int min_fps = camera_shotcaps.iMinFpsQ16;
            const int max_fps = camera_shotcaps.iMaxFpsQ16;
            TRACE_V("%s: minfps=%d,maxfps=%d",__FUNCTION__,min_fps,max_fps);
            const int iFPSQ16 = framerate << 16;
            if(iFPSQ16 <= max_fps && iFPSQ16 >= min_fps && iFPSQ16 > 0){
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_FRAMERATE,
                        (CAM_Param)iFPSQ16,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);
                TRACE_D("%s:framerate:%d",__FUNCTION__,framerate);
            }
        }
#endif
        //For VT, brightness
        v = setting.get("light_key");
        const int minbright = camera_shotcaps.iMinBrightness;
        const int maxbright = camera_shotcaps.iMaxBrightness;
        TRACE_V("%s: minbright=%d,maxbright=%d",__FUNCTION__,minbright,maxbright);
        if( 0 != v){
            int brightval = atoi(v);
            if(brightval < minbright){
                TRACE_E("Invalid bright value:%d. set to min:%d", brightval, minbright);
                brightval = minbright;
                //ret = BAD_VALUE;
            }
            else if (brightval> maxbright){
                TRACE_E("Invalid bright value:%d. set to max:%d",brightval,maxbright);
                brightval = maxbright;
                //ret = BAD_VALUE;
            }
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_BRIGHTNESS,
                    (CAM_Param)brightval,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
            TRACE_D("%s:brightness=%d", __FUNCTION__, brightval);
        }

        //For VT, contrast
        v = setting.get("contrast_key");
        const int mincontrast = camera_shotcaps.iMinContrast;
        const int maxcontrast = camera_shotcaps.iMaxContrast;
        TRACE_V("%s: mincontrast=%d,maxcontrast=%d",__FUNCTION__,mincontrast,maxcontrast);
        if( 0 != v){
            int contrastval = atoi(v);
            if(contrastval < mincontrast){
                TRACE_E("Invalid contrast value:%d. set to min:%d", contrastval, mincontrast);
                contrastval = mincontrast;
                //ret = BAD_VALUE;
            }
            else if (contrastval > maxcontrast){
                TRACE_E("Invalid contrast value:%d. set to max:%d",contrastval,maxcontrast);
                contrastval = maxcontrast;
                //ret = BAD_VALUE;
            }
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_CONTRAST,
                    (CAM_Param)contrastval,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
            TRACE_D("%s:contrast=%d", __FUNCTION__, contrastval);
        }

        ret |= initPreviewParameters();
        ret |= initStillParameters();

        return ret;
    }

    status_t Engine::ceStartPreview(const CAM_DeviceHandle &handle)
    {
        CAM_Error error = CAM_ERROR_NONE;
        CAM_CaptureState state;

        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if (state == CAM_CAPTURESTATE_PREVIEW){
            return NO_ERROR;
        }
        else if (state == CAM_CAPTURESTATE_STILL ||
                state == CAM_CAPTURESTATE_VIDEO ||
                state == CAM_CAPTURESTATE_IDLE
                ){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            //ASSERT_CAM_ERROR(error);
            if(CAM_ERROR_NONE != error){
                return UNKNOWN_ERROR;
            }
            else{
                return NO_ERROR;
            }
        }
        else{
            TRACE_E("Unknown error happened,wrong state");
            return UNKNOWN_ERROR;
        }
    }

    /*
     * ONLY switch to capture state for ce,
     * assume that ce was already in preview state
     */
    status_t Engine::ceStartCapture(const CAM_DeviceHandle &handle)
    {

        CAM_Error error = CAM_ERROR_NONE;
#if 0
        // Enqueue all still image buffers
        for (int i=0; i<kStillBufCnt; i++){
            CAM_ImageBuffer* pImgBuf_still = mStillImageBuf->getEngBuf(i);
            error = CAM_SendCommand(handle,
                    CAM_CMD_PORT_ENQUEUE_BUF,
                    (CAM_Param)CAM_PORT_STILL,
                    (CAM_Param)pImgBuf_still);
            ASSERT_CAM_ERROR(error);
        }

        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STILL_BURST,
                (CAM_Param)&iExpectedPicNum,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
#endif

        CAM_CaptureState state;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if (state == CAM_CAPTURESTATE_STILL){
            return NO_ERROR;
        }
        else if (state == CAM_CAPTURESTATE_PREVIEW){
            // switch to still capture state
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_STILL,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        else if (state == CAM_CAPTURESTATE_VIDEO ||
                state == CAM_CAPTURESTATE_IDLE){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_STILL,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        else{
            TRACE_E("Unknown error happened,wrong state");
            return UNKNOWN_ERROR;
        }

        return NO_ERROR;
    }

    status_t Engine::ceStopCapture(const CAM_DeviceHandle &handle)
    {
        CAM_Error error = CAM_ERROR_NONE;
        CAM_CaptureState state;

        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if (state == CAM_CAPTURESTATE_STILL){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        else{
            TRACE_D("%s:current state is %d",__FUNCTION__,error);
        }
        return NO_ERROR;
    }

    status_t Engine::ceStartVideo(const CAM_DeviceHandle &handle)
    {
        status_t ret = ceSetStillSubMode(mCEHandle, mStillSubMode[1]);
        return ret;
    }

    status_t Engine::ceStopVideo(const CAM_DeviceHandle &handle)
    {
        status_t ret = ceSetStillSubMode(mCEHandle, mStillSubMode[0]); // keep default as capture preview mode
        return NO_ERROR;
    }

    /*
     * 1.set ce state from preview to idle
     * 2.unuse buf on preview port
     */
    status_t Engine::ceStopPreview(const CAM_DeviceHandle &handle)
    {

        CAM_Error error = CAM_ERROR_NONE;
        CAM_CaptureState state;

        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if (state == CAM_CAPTURESTATE_STILL){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_IDLE,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        else if (state == CAM_CAPTURESTATE_VIDEO){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_IDLE,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
        else if (state == CAM_CAPTURESTATE_PREVIEW){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_IDLE,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
        }
#if 0
        error = CAM_SendCommand(handle,
                CAM_CMD_PORT_FLUSH_BUF,
                (CAM_Param)CAM_PORT_ANY,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
#endif
        return NO_ERROR;
    }
    status_t Engine::ceStartFaceDetection(const CAM_DeviceHandle &handle){
        CAM_Error error = CAM_ERROR_NONE;
        TRACE_D("%s:start Face Detection; ",__FUNCTION__);

        int max_face_num = mSetting.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
        if (max_face_num <=0)
            return BAD_VALUE;

        error = CAM_SendCommand(handle,
                CAM_CMD_START_FACEDETECTION,
                UNUSED_PARAM,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        return NO_ERROR;
    }
    status_t Engine::ceStopFaceDetection(const CAM_DeviceHandle &handle){
        CAM_Error error = CAM_ERROR_NONE;
        TRACE_D("%s:stop Face Detection; ",__FUNCTION__);

        int max_face_num = mSetting.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
        if (max_face_num <=0)
            return BAD_VALUE;

        error = CAM_SendCommand(handle,
                CAM_CMD_CANCEL_FACEDETECTION,
                UNUSED_PARAM,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);
        return NO_ERROR;
    }

    CAM_Error Engine::ceGetPreviewFrame(const CAM_DeviceHandle &handle,void** key)
    {
        TRACE_V("-%s", __FUNCTION__);

        CAM_Error error = CAM_ERROR_NONE;
        CAM_ImageBuffer *pImgBuf;

        int i;
        IppStatus ippret;

        int port = CAM_PORT_PREVIEW;
        error = CAM_SendCommand(handle,
                CAM_CMD_PORT_DEQUEUE_BUF,
                (CAM_Param)&port,
                (CAM_Param)&pImgBuf);

        //filter the v4l2 polling failure error
        if (CAM_ERROR_FATALERROR == error) {
            TRACE_D("%s dequeue return with error %d, restore cameraengine", __FUNCTION__, error);
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_IDLE,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            //only recover engine to preview status if preview thread is not request to exit
            //if (CAM_STATE_PREVIEW == mState){
            error = CAM_SendCommand(handle,
                    CAM_CMD_SET_STATE,
                    (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);
            //}
            return CAM_ERROR_FATALERROR;
        }
        //check any other errors
        if (CAM_ERROR_NONE != error) {
            TRACE_D("%s x with error %d", __FUNCTION__, error);
            return error;
        }

        if(key)
            *key = (void*)pImgBuf->iUserIndex;

        return error;
    }

    CAM_Error Engine::ceGetStillFrame(const CAM_DeviceHandle &handle, void** key)
    {
        int port;
        CAM_Error error = CAM_ERROR_NONE;
        CAM_ImageBuffer *pImgBuf;
        do{
            port = CAM_PORT_STILL;
            error = CAM_SendCommand(handle,
                    CAM_CMD_PORT_DEQUEUE_BUF,
                    (CAM_Param)&port,
                    (CAM_Param)&pImgBuf);

            if (CAM_ERROR_NONE == error){
                break;
            }
            else if (CAM_ERROR_FATALERROR == error) {
#if 0
                TRACE_D("%s dequeue return with error %d, restore cameraengine", __FUNCTION__, error);
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_STATE,
                        (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);

                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_STATE,
                        (CAM_Param)CAM_CAPTURESTATE_IDLE,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);

                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_STATE,
                        (CAM_Param)CAM_CAPTURESTATE_PREVIEW,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);

                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_STATE,
                        (CAM_Param)CAM_CAPTURESTATE_STILL,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);
#else
                error = CAM_SendCommand(handle,
                        CAM_CMD_RESET_CAMERA,
                        (CAM_Param)CAM_RESET_COMPLETE,
                        UNUSED_PARAM);
                ASSERT_CAM_ERROR(error);
#endif
            }
            else{
                ASSERT_CAM_ERROR(error);
            }
        }while(1);

        if(key)
            *key = (void*)pImgBuf->iUserIndex;

        return error;
    }

    CAM_Error Engine::ceGetCurrentSceneModeCap(const CAM_DeviceHandle &handle,
            CameraSetting& setting)
    {
        CAM_Error error = CAM_ERROR_NONE;
        String8 v = String8("");

        // scene mode
        int scenemode;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_SHOTMODE,
                (CAM_Param)&scenemode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_scenemode,scenemode);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_SCENE_MODE,v.string());
            TRACE_D("%s:CE used scene mode:%s",__FUNCTION__,v.string());
        }

        // white balance
        CAM_WhiteBalanceMode WhiteBalance;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_WHITEBALANCEMODE,
                (CAM_Param)&WhiteBalance,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_whitebalance,WhiteBalance);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_WHITE_BALANCE,v.string());
            TRACE_D("%s:CE used whitebalance:%s",__FUNCTION__,v.string());
        }

        // color effect
        CAM_ColorEffect ColorEffect;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_COLOREFFECT,
                (CAM_Param)&ColorEffect,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_coloreffect,ColorEffect);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_EFFECT,v.string());
            TRACE_D("%s:CE used color effect:%s",__FUNCTION__,v.string());
        }

        // bandfilter
        CAM_BandFilterMode BandFilter;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_BANDFILTER,
                (CAM_Param)&BandFilter,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_bandfilter,BandFilter);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_ANTIBANDING,v.string());
            TRACE_D("%s:CE used antibanding:%s",__FUNCTION__,v.string());
        }

        // flash mode
        CAM_FlashMode FlashMode;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_FLASHMODE,
                (CAM_Param)&FlashMode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_flash,FlashMode);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_FLASH_MODE,v.string());
            TRACE_D("%s:CE used flash mode:%s",__FUNCTION__,v.string());
        }

        // focus mode
        CAM_FocusMode FocusMode;
        error = CAM_SendCommand(handle,
                CAM_CMD_GET_FOCUSMODE,
                (CAM_Param)&FocusMode,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        v = String8("");
        v = setting.map_ce2andr(CameraSetting::map_focus,FocusMode);
        if( v != String8("") ){
            setting.set(CameraParameters::KEY_FOCUS_MODE,v.string());
            TRACE_D("%s:CE used focus mode:%s",__FUNCTION__,v.string());
        }

        // EVC
        const int evcstep=mCamera_shotcaps.iEVCompStepQ16;
        if ( 0 != evcstep){
            int ev = 0;
            error = CAM_SendCommand(handle,
                    CAM_CMD_GET_EVCOMPENSATION,
                    (CAM_Param)&ev,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            v = String8("");
            char tmp[32] = {'\0'};
            int exposure = ev / evcstep;
            sprintf(tmp, "%d", exposure);
            v = String8(tmp);
            setting.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, v.string());
            TRACE_D("%s:CE used exposure compensation:%s",__FUNCTION__,v.string());
        }


        if( mSetting.get(CameraParameters::KEY_ZOOM_SUPPORTED) != 0 &&
                strcmp(mSetting.get(CameraParameters::KEY_ZOOM_SUPPORTED), "true") == 0){
            //digital zoom
            const int minzoom = mCamera_shotcaps.iMinDigZoomQ16;
            const int maxzoom = mCamera_shotcaps.iMaxDigZoomQ16;

            TRACE_V("%s: minzoom=%d,maxzoom=%d",__FUNCTION__,minzoom,maxzoom);
            const int cezoomstep = mCamera_shotcaps.iDigZoomStepQ16;
            if((0x1<<16) != maxzoom){
                int cedigzoom;
                char tmp[32] = {'\0'};
                error = CAM_SendCommand(handle,
                        CAM_CMD_GET_DIGZOOM,
                        (CAM_Param)&cedigzoom,
                        UNUSED_PARAM );
                ASSERT_CAM_ERROR(error);

                int andrzoomval = (cedigzoom-minzoom)/(cezoomstep);//1.0x for andr min zoom
                sprintf(tmp, "%d", andrzoomval);
                mSetting.set(CameraParameters::KEY_ZOOM, tmp);
                TRACE_D("%s:CE used digital zoom:%s",__FUNCTION__,tmp);
            }
        }
        else{
            TRACE_D("%s: zoom is not supported",__FUNCTION__);
        }

#if 1
        //fps
        {
            const int min_fps = mCamera_shotcaps.iMinFpsQ16 >> 16;
            const int max_fps = mCamera_shotcaps.iMaxFpsQ16 >> 16;
            int iFPSQ16;
            error = CAM_SendCommand(handle,
                    CAM_CMD_GET_FRAMERATE,
                    (CAM_Param)&iFPSQ16,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            int framerate = iFPSQ16 >> 16;
            TRACE_V("%s: minfps=%d,maxfps=%d,CE framerate=%d",__FUNCTION__,
                    min_fps,max_fps,framerate);
        }
#endif

        //For VT, brightness
        {
            int brightval;
            error = CAM_SendCommand(handle,
                    CAM_CMD_GET_BRIGHTNESS,
                    (CAM_Param)&brightval,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            char tmp[32] = {'\0'};
            sprintf(tmp, "%d", brightval);
            setting.set("light_key",tmp);
            TRACE_D("%s:CE used brightness=%d", __FUNCTION__, brightval);
        }

        //For VT, contrast
        {
            int contrastval;
            error = CAM_SendCommand(handle,
                    CAM_CMD_GET_CONTRAST,
                    (CAM_Param)&contrastval,
                    UNUSED_PARAM);
            ASSERT_CAM_ERROR(error);

            char tmp[32] = {'\0'};
            sprintf(tmp, "%d", contrastval);
            setting.set("contrast_key",tmp);
            TRACE_D("%s:CE used contrast=%d", __FUNCTION__, contrastval);
        }

        return error;
    }

    status_t Engine::ceSetStillSubMode(const CAM_DeviceHandle &handle, CAM_StillSubMode mode)
    {
        CAM_Error error = CAM_ERROR_NONE;
        CAM_CaptureState state;

        error = CAM_SendCommand(handle,
                CAM_CMD_GET_STATE,
                (CAM_Param)&state,
                UNUSED_PARAM);
        ASSERT_CAM_ERROR(error);

        if (state == CAM_CAPTURESTATE_PREVIEW ||state == CAM_CAPTURESTATE_IDLE){
            TRACE_D("%s:start to set still sub Mode",__FUNCTION__);
            CAM_StillSubMode isubmode_state;
            CAM_StillSubModeParam iStillModeParam;
            error = CAM_SendCommand(handle,
                    CAM_CMD_GET_STILL_SUBMODE,
                    (CAM_Param)&isubmode_state,
                    (CAM_Param)&iStillModeParam);
            ASSERT_CAM_ERROR(error);

            if(isubmode_state != mode)
            {
                switch(mode)
                {
                    case CAM_STILLSUBMODE_ZSL:
                        iStillModeParam.iBurstCnt = 1;
                        iStillModeParam.iZslDepth = 2; //depth can be optimized.
                        break;
                    case CAM_STILLSUBMODE_SIMPLE:
                        iStillModeParam.iBurstCnt = 1;
                        iStillModeParam.iZslDepth = 0;
                        break;
                }
                error = CAM_SendCommand(handle,
                        CAM_CMD_SET_STILL_SUBMODE,
                        (CAM_Param)mode,
                        (CAM_Param)&iStillModeParam);
                ASSERT_CAM_ERROR(error);

                TRACE_D("%s: set still sub mode sucess;", __FUNCTION__);
            }
        }
        else{
            TRACE_E("%s:the camera state is not preview or Idle,%d",__FUNCTION__,state);
        }
        return NO_ERROR;
    }

}; // namespace android
