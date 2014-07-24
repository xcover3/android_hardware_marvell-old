LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
	ion_helper_lib.c

LOCAL_MODULE := libionhelper

LOCAL_SHARED_LIBRARIES := libutils liblog

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
