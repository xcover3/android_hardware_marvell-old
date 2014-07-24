LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../source \
	vendor/marvell/generic/graphics/user/include \
	vendor/marvell/generic/graphics/include \
	vendor/marvell/generic/graphics \
	vendor/marvell/generic/graphics/user

LOCAL_SRC_FILES := \
    copy8888.c \
    memcpy_MRVL.s \
    sample_ARGBToYUV.c

LOCAL_SHARED_LIBRARIES := \
	libgpucsc	\
	libgcu  \
	libcutils	\

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_ARGBToYUV
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../source \
	vendor/marvell/generic/graphics/user/include \
	vendor/marvell/generic/graphics/include \
	vendor/marvell/generic/graphics \
	vendor/marvell/generic/graphics/user

LOCAL_SRC_FILES := \
    copy8888.c \
    memcpy_MRVL.s \
	sample_ARGBToYUV2.c

LOCAL_SHARED_LIBRARIES := \
	libgpucsc	\
	libgcu  \
	libcutils	\

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_ARGBToYUV2
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../source \
	vendor/marvell/generic/graphics/user/include \
	vendor/marvell/generic/graphics/include \
	vendor/marvell/generic/graphics \
	vendor/marvell/generic/graphics/user

LOCAL_SRC_FILES := \
	sample_ARGBToYUV3.c

LOCAL_SHARED_LIBRARIES := \
	libgpucsc	\
	libgcu  \
	libcutils	\

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_ARGBToYUV3
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../source \
	vendor/marvell/generic/graphics/user/include \
	vendor/marvell/generic/graphics/include \
	vendor/marvell/generic/graphics \
	vendor/marvell/generic/graphics/user

LOCAL_SRC_FILES := \
	sample_ARGBToYUV4.c

LOCAL_SHARED_LIBRARIES := \
	libgpucsc	\
	libgcu  \
	libcutils	\

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_ARGBToYUV4
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

