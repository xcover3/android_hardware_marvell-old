LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)	\
	$(LOCAL_PATH)/../../../include/

LOCAL_CFLAGS += -mabi=aapcs-linux

LOCAL_SRC_FILES:= \
	misc.c \
	arm_c_linux/common.c \
	arm_c_linux/perf.c  \
	arm_c_linux/render.c  \
	arm_c_linux/thread.c

LOCAL_SRC_FILES += ipp_android_log.c

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE := libmiscgen
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

