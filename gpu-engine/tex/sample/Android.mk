#Create Shared lib
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    sample_astc_etc2.c

LOCAL_SHARED_LIBRARIES:= libgputex

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include

LOCAL_MODULE:=astc_etc2
LOCAL_MODULE_TAGS:=optional
TARGET_PRELINK_MODULES:=false
include $(BUILD_EXECUTABLE)
