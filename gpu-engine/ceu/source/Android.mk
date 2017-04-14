#
# libceu
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
    gpu_ceu.cpp \
    gpu_ceu_utils.cpp  \
    gpu_effect_internal.cpp \
    gpu_ceu_effect_null.cpp  \
    gpu_ceu_effect_oldmovie.cpp \
    gpu_ceu_effect_toonshading.cpp \
    gpu_ceu_effect_hatching.cpp \
    gpu_ceu_effect_pencilsketch.cpp \
    gpu_ceu_effect_glow.cpp \
    gpu_ceu_effect_twist.cpp \
    gpu_ceu_effect_frame.cpp \
    gpu_ceu_effect_sunshine.cpp \
    gpu_ceu_rgb2uyvy.cpp \
    gpu_ceu_rgb2yuyv.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libskia \
    libEGL \
    libGLESv2 \
    libandroidfw

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include \
    external/skia/include/core \
    external/skia/include/images

LOCAL_MODULE:= libceu

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DGL_GLEXT_PROTOTYPES

include $(BUILD_SHARED_LIBRARY)
