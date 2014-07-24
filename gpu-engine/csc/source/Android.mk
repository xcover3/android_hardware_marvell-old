BASE_PATH  := $(call my-dir)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := gpu_csc.cpp ARGB8888ToYUV_data.c

LOCAL_SHARED_LIBRARIES := libgcu libcutils

ifeq ($(ARCH_ARM_HAVE_NEON),true)
    LOCAL_SRC_FILES += ARGB8888ToYUV_NEON.s
else
    LOCAL_SRC_FILES += ARGB8888ToYUV_C.c
endif

LOCAL_LDFLAGS += -Wl,--no-warn-shared-textrel

LOCAL_C_INCLUDES += \
    vendor/marvell/generic/graphics/user/include \
    vendor/marvell/generic/graphics/include \
    vendor/marvell/generic/graphics \
    vendor/marvell/generic/graphics/user \
    vendor/marvell/generic/gpu-engine/csc/include

LOCAL_MODULE := libgpucsc
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)


