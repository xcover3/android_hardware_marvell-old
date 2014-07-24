#
## libdxofmtconvert
#
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# generic files
LOCAL_SRC_FILES += \
    DxOFmtConvert/DxOFmtConvert.c

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libdxofmtconvert

include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)
