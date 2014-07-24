LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include/ \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include

LOCAL_SRC_FILES:= \
    sample_image_effect.cpp \
    sample_utils.cpp

LOCAL_SHARED_LIBRARIES := \
    libceu \
    libgcu

LOCAL_MODULE:= sample_image_effect

LOCAL_MODULE_TAGS := samples

LOCAL_CFLAGS := -DGL_GLEXT_PROTOTYPES

include $(BUILD_EXECUTABLE)

#############################################################
# RGB to UYVY transform
#############################################################
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include/ \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include

LOCAL_SRC_FILES:= \
    sample_rgb2uyvy.cpp \
    sample_utils.cpp

LOCAL_SHARED_LIBRARIES := \
    libceu \
    libgcu

LOCAL_MODULE:= sample_rgb2uyvy

LOCAL_MODULE_TAGS := samples

LOCAL_CFLAGS := -DGL_GLEXT_PROTOTYPES

include $(BUILD_EXECUTABLE)
