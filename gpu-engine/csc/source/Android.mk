BASE_PATH  := $(call my-dir)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := gpu_csc.cpp ARGB8888ToYUV_data.c

LOCAL_SHARED_LIBRARIES := libgcu libcutils

LOCAL_SRC_FILES += ARGB8888ToYUV_NEON.s
LOCAL_SRC_FILES += ARGB8888ToYUV_C.c

LOCAL_LDFLAGS += -Wl,--no-warn-shared-textrel

LOCAL_C_INCLUDES += \
    hardware/marvell/pxa1088/graphics/user/include \
    hardware/marvell/pxa1088/graphics/include \
    hardware/marvell/pxa1088/graphics \
    hardware/marvell/pxa1088/graphics/user \
    hardware/marvell/pxa1088/gpu-engine/csc/include

LOCAL_MODULE := libgpucsc
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)


