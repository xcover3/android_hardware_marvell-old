#
## libcameraengine
#
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../CameraConfig.mk

# build ext-isp based camera-engine
ifeq (${ENABLE_EXT_ISP}, y)
LOCAL_SRC_FILES += \
    extisp_cameraengine/cam_extisp_eng.c \
    extisp_cameraengine/cam_extisp_sensorwrapper.c

LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/extisp_cameraengine
endif

#build soc-isp based camera engine
ifeq (${ENABLE_DXO_ISP}, y)
LOCAL_SRC_FILES += \
    socisp_cameraengine/cam_socisp_eng.c \
    socisp_cameraengine/cam_socisp_hal.c \
    socisp_cameraengine/DxO/DxOISP_Tools.c \
    socisp_cameraengine/DxO/DxOISP_Debug.c


ifeq (${ENABLE_MC_DRV}, y)
LOCAL_SRC_FILES += \
    socisp_cameraengine/ispdma/linux-mc/cam_socisp_media.c\
    socisp_cameraengine/ispdma/linux-mc/cam_socisp_dmactrl_mc.c

LOCAL_CFLAGS +=\
    -I $(LOCAL_PATH)/socisp_cameraengine/ispdma/linux-mc

else
LOCAL_SRC_FILES += \
    socisp_cameraengine/ispdma/linux/cam_socisp_dmactrl.c

LOCAL_CFLAGS +=\
    -I $(LOCAL_PATH)/socisp_cameraengine/ispdma/linux

endif


LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/socisp_cameraengine \
    -I $(LOCAL_PATH)/socisp_sensorhal \
    -I $(LOCAL_PATH)/socisp_cameraengine/DxO

LOCAL_WHOLE_STATIC_LIBRARIES += libdxoisp
endif

# generic files
LOCAL_SRC_FILES += \
    general/cam_ppu.c \
    general/cam_gen.c \
    general/cam_utility.c \
    general/cam_trace.c \
    general/cam_osal.c \

LOCAL_CFLAGS += \
    -I $(LOCAL_PATH)/_include \
    -I $(LOCAL_PATH)/../../../include \
    -D LINUX \
    -D ANDROID \
    -D CAM_LOG_ERROR

# external dependencies
LOCAL_C_INCLUDES := vendor/marvell/generic/phycontmem-lib/phycontmem \
                    vendor/marvell/generic/graphics/include \
                    vendor/marvell/generic/ipplib/include

# put static libraies used here
LOCAL_STATIC_LIBRARIES += libcamerasensorhal \
                          libippcam \

# put shared objects used here
LOCAL_SHARED_LIBRARIES += libphycontmem \
                          libcodecjpegdec \
                          libcodecjpegenc \
                          libmiscgen \
                          libGAL \
                          libgcu \
                          libutils \
                          libcutils

ifeq (${ENABLE_VMETA}, y)
LOCAL_SHARED_LIBRARIES += libcodecvmetaenc \
                          libvmeta
endif

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libcameraengine

#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)
