#
# libveu
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    gpu_veu.cpp \
    gpu_veu_effect_and_transition.cpp \
    gpu_veu_internal.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libgcu \

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    frameworks/av/libvideoeditor/vss/common/inc \
    frameworks/av/libvideoeditor/vss/mcs/inc \
    frameworks/av/libvideoeditor/vss/inc \
    frameworks/av/libvideoeditor/osal/inc \

LOCAL_MODULE:= libveu
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
