LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)   \
        $(JNI_H_INCLUDE) \
        vendor/marvell/generic/libstagefrighthw/renderer \
        vendor/marvell/generic/ipplib/openmax/include \
        vendor/marvell/generic/marvell-gralloc \
        frameworks/base/include \
        frameworks/base/media/libstagefright/include \
        hardware/libhardware/include \
        system/core/include

ifeq ($(shell if [ $(PLATFORM_SDK_VERSION) -ge 9 ]; then echo big9; fi),big9)
LOCAL_C_INCLUDES += \
        frameworks/base/include/media/stagefright/openmax
else
LOCAL_C_INCLUDES += \
        external/opencore/extern_libs_v2/khronos/openmax/include
endif

LOCAL_CFLAGS += -mabi=aapcs-linux

LOCAL_SRC_FILES:= \
drmplayer_video_render.cpp\
drmplayer_audio_sink.cpp

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := libmedia libutils libsurfaceflinger_client libcutils libui libstagefright
LOCAL_MODULE := libdrmplaysink
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
