LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_SRC_FILES := \
    sample_color.c

LOCAL_SHARED_LIBRARIES := \
    libveu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_color
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_SRC_FILES := \
    sample_slidetransition.c

LOCAL_SHARED_LIBRARIES := \
    libveu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_slidetransition
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_SRC_FILES := \
    sample_zoom.c

LOCAL_SHARED_LIBRARIES := \
    libveu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_zoom
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_SRC_FILES := \
    sample_surface.c

LOCAL_SHARED_LIBRARIES := \
    libveu \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_surface
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_SRC_FILES := \
    sample_fadeblack.c

LOCAL_SHARED_LIBRARIES := \
    libveu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_fadeblack
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

