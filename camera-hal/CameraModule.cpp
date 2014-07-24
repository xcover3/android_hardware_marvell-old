/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
*
* This file maps the Camera Hardware Interface to Camera Instance Implementation.
*
*/

#define LOG_TAG "CameraModule"

#include <utils/threads.h>
#include "CameraHardware.h"
#include "cam_log_mrvl.h"

static android::CameraHardware* gCameraHals[MAX_CAMERAS_SUPPORTED];

static android::Mutex gCameraHalDeviceLock;
static int gNum_Cameras = 0;

static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device);
static int camera_device_close(hw_device_t* device);
static int camera_get_number_of_cameras(void);
static int camera_get_camera_info(int camera_id, struct camera_info *info);

static struct hw_module_methods_t camera_module_methods = {
        open: camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: {
         tag: HARDWARE_MODULE_TAG,
         version_major: 1,
         version_minor: 0,
         id: CAMERA_HARDWARE_MODULE_ID,
         name: "MRVL CameraHal Module",
         author: "MRVL",
         methods: &camera_module_methods,
         dso: NULL, /* remove compilation warnings */
         reserved: {0}, /* remove compilation warnings */
    },
    get_number_of_cameras: camera_get_number_of_cameras,
    get_camera_info: camera_get_camera_info,
};

typedef struct mrvl_camera_device {
    camera_device_t base;
    /* specific "private" data can go here (base.priv) */
    int cameraid;
} mrvl_camera_device_t;


/*******************************************************************
 * implementation of camera_device_ops functions
 *******************************************************************/

int camera_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->setPreviewWindow(window);

    return rv;
}

void camera_set_callbacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
}

void camera_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->enableMsgType(msg_type);
}

void camera_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->disableMsgType(msg_type);
}

int camera_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return 0;

    mrvl_dev = (mrvl_camera_device_t*) device;

    return gCameraHals[mrvl_dev->cameraid]->msgTypeEnabled(msg_type);
}

int camera_start_preview(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->startPreview();

    return rv;
}

void camera_stop_preview(struct camera_device * device)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->stopPreview();
}

int camera_preview_enabled(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->previewEnabled();
    return rv;
}

int camera_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    //  TODO: meta data buffer not current supported
    rv = gCameraHals[mrvl_dev->cameraid]->storeMetaDataInBuffers(enable);
    return rv;
    //return enable ? android::INVALID_OPERATION: android::OK;
}

int camera_start_recording(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->startRecording();
    return rv;
}

void camera_stop_recording(struct camera_device * device)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->stopRecording();
}

int camera_recording_enabled(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->recordingEnabled();
    return rv;
}

void camera_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->releaseRecordingFrame(opaque);
}

int camera_auto_focus(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->autoFocus();
    return rv;
}

int camera_cancel_auto_focus(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->cancelAutoFocus();
    return rv;
}

int camera_take_picture(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->takePicture();
    return rv;
}

int camera_cancel_picture(struct camera_device * device)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->cancelPicture();
    return rv;
}

int camera_set_parameters(struct camera_device * device, const char *params)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->setParameters(params);
    return rv;
}

char* camera_get_parameters(struct camera_device * device)
{
    char* param = NULL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return NULL;

    mrvl_dev = (mrvl_camera_device_t*) device;

    param = gCameraHals[mrvl_dev->cameraid]->getParameters();

    return param;
}

static void camera_put_parameters(struct camera_device *device, char *parms)
{
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->putParameters(parms);
}

int camera_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    ALOGV("%s", __FUNCTION__);

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->sendCommand(cmd, arg1, arg2);
    return rv;
}

void camera_release(struct camera_device * device)
{
    FUNC_TAG;
    mrvl_camera_device_t* mrvl_dev = NULL;

    if(!device)
        return;

    mrvl_dev = (mrvl_camera_device_t*) device;

    gCameraHals[mrvl_dev->cameraid]->release();
}

int camera_dump(struct camera_device * device, int fd)
{
    int rv = -EINVAL;
    mrvl_camera_device_t* mrvl_dev = NULL;

    if(!device)
        return rv;

    mrvl_dev = (mrvl_camera_device_t*) device;

    rv = gCameraHals[mrvl_dev->cameraid]->dump(fd);
    return rv;
}

int camera_device_close(hw_device_t* device)
{
    FUNC_TAG;

    int ret = 0;
    mrvl_camera_device_t* mrvl_dev = NULL;

    android::Mutex::Autolock lock(gCameraHalDeviceLock);

    if (!device) {
        ret = -EINVAL;
        goto done;
    }

    mrvl_dev = (mrvl_camera_device_t*) device;

    if (mrvl_dev) {
        int cameraid = mrvl_dev->cameraid;
        android::CameraHardware* camera = gCameraHals[cameraid];

        if(camera){
            delete camera;
            camera = NULL;
        }
        else{
            TRACE_E("gCameraHals[cameraid] == NULL, camera %d already free!", cameraid);
        }
        gCameraHals[cameraid] = NULL;

        if (mrvl_dev->base.ops) {
            free(mrvl_dev->base.ops);
        }
        free(mrvl_dev);
    }
done:
    return ret;
}

/*******************************************************************
 * implementation of camera_module functions
 *******************************************************************/

/* open device handle to one of the cameras
 *
 * assume camera service will keep singleton of each camera
 * so this function will always only be called once per camera instance
 */

int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    FUNC_TAG;

    int rv = 0;
    int cameraid;
    mrvl_camera_device_t* camera_device = NULL;
    camera_device_ops_t* camera_ops = NULL;
    android::CameraHardware* camera = NULL;

    android::Mutex::Autolock lock(gCameraHalDeviceLock);


    if (name != NULL) {
        cameraid = atoi(name);

        if(cameraid > gNum_Cameras || cameraid < 0)
        {
            ALOGE("camera service provided cameraid out of bounds, "
                    "cameraid = %d, num supported = %d",
                    cameraid, gNum_Cameras);
            rv = -EINVAL;
            goto fail;
        }

        camera_device = (mrvl_camera_device_t*)malloc(sizeof(*camera_device));
        if(!camera_device)
        {
            ALOGE("camera_device allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        camera_ops = (camera_device_ops_t*)malloc(sizeof(*camera_ops));
        if(!camera_ops)
        {
            ALOGE("camera_ops allocation fail");
            rv = -ENOMEM;
            goto fail;
        }

        memset(camera_device, 0, sizeof(*camera_device));
        memset(camera_ops, 0, sizeof(*camera_ops));

        camera_device->base.common.tag = HARDWARE_DEVICE_TAG;
        camera_device->base.common.version = 0;
        camera_device->base.common.module = (hw_module_t *)(module);
        camera_device->base.common.close = camera_device_close;
        camera_device->base.ops = camera_ops;

        camera_ops->set_preview_window = camera_set_preview_window;
        camera_ops->set_callbacks = camera_set_callbacks;
        camera_ops->enable_msg_type = camera_enable_msg_type;
        camera_ops->disable_msg_type = camera_disable_msg_type;
        camera_ops->msg_type_enabled = camera_msg_type_enabled;
        camera_ops->start_preview = camera_start_preview;
        camera_ops->stop_preview = camera_stop_preview;
        camera_ops->preview_enabled = camera_preview_enabled;
        camera_ops->store_meta_data_in_buffers = camera_store_meta_data_in_buffers;
        camera_ops->start_recording = camera_start_recording;
        camera_ops->stop_recording = camera_stop_recording;
        camera_ops->recording_enabled = camera_recording_enabled;
        camera_ops->release_recording_frame = camera_release_recording_frame;
        camera_ops->auto_focus = camera_auto_focus;
        camera_ops->cancel_auto_focus = camera_cancel_auto_focus;
        camera_ops->take_picture = camera_take_picture;
        camera_ops->cancel_picture = camera_cancel_picture;
        camera_ops->set_parameters = camera_set_parameters;
        camera_ops->get_parameters = camera_get_parameters;
        camera_ops->put_parameters = camera_put_parameters;
        camera_ops->send_command = camera_send_command;
        camera_ops->release = camera_release;
        camera_ops->dump = camera_dump;

        *device = &camera_device->base.common;

        // -------- specific stuff --------

        if(gCameraHals[cameraid] != NULL){
            TRACE_E("gCameraHals[cameraid] != NULL, previous camera %d not free!", cameraid);
            delete gCameraHals[cameraid];
        }
        android::CameraHardware* camera = new android::CameraHardware(cameraid);
        gCameraHals[cameraid] = camera;
        camera_device->cameraid = cameraid;
    }

    return rv;

fail:
    if(camera_device) {
        free(camera_device);
        camera_device = NULL;
    }
    if(camera_ops) {
        free(camera_ops);
        camera_ops = NULL;
    }
    if(camera) {
        camera->release();
        camera = NULL;
    }
    *device = NULL;
    return rv;
}

int camera_get_number_of_cameras(void)
{
    FUNC_TAG;

    int num_cameras = android::HAL_getNumberOfCameras();
    gNum_Cameras = num_cameras;
    return num_cameras;
}

int camera_get_camera_info(int camera_id, struct camera_info *info)
{
    FUNC_TAG;

    android::HAL_getCameraInfo(camera_id, info);
    return 0;
}


