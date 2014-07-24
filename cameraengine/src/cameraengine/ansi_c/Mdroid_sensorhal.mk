#
## libcamerasensorhal
#
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../CameraConfig.mk

# ext-isp based sensor hal
ifeq (${ENABLE_EXT_ISP}, y)
LOCAL_SRC_FILES += \
    extisp_sensorhal/cam_extisp_sensormanifest.c \
    extisp_sensorhal/icphd/cam_extisp_icphd.c \
    extisp_sensorhal/ov5642/cam_extisp_ov5642.c \
    extisp_sensorhal/basicsensor/cam_extisp_basicsensor.c \
    extisp_sensorhal/fakesensor/cam_extisp_fakesensor.c \
    extisp_sensorhal/cam_extisp_v4l2base.c

LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/extisp_sensorhal

LOCAL_WHOLE_STATIC_LIBRARIES += FaceTrack
endif

# soc-isp based sensor hal
ifeq (${ENABLE_DXO_ISP}, y)
LOCAL_SRC_FILES += \
    socisp_sensorhal/cam_socisp_sensorhal.c \
    socisp_sensorhal/ov8820/sensorOVT8820.c

LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/socisp_sensorhal \
    -I $(LOCAL_PATH)/socisp_sensorhal/ov8820 \
    -I $(LOCAL_PATH)/socisp_cameraengine/DxO

ifeq ($(ENABLE_MC_DRV), y)
LOCAL_CFLAGS +=\
    -I $(LOCAL_PATH)/socisp_cameraengine/ispdma/linux-mc

endif
endif

# generic includes
LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/../../../include \
    -I $(LOCAL_PATH)/_include \
    -D LINUX \
    -D ANDROID \
    -D CAM_LOG_ERROR

# external dependencies
LOCAL_C_INCLUDES := vendor/marvell/generic/phycontmem-lib/phycontmem \
                    vendor/marvell/generic/ipplib/include

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libcamerasensorhal

include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)
