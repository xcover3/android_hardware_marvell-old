LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
	phycontmem.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../ionhelper

LOCAL_MODULE := libphycontmem

LOCAL_SHARED_LIBRARIES := libutils libionhelper

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

