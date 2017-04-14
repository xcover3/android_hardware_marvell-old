# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../../include

LOCAL_CFLAGS += -D_IPP_LINUX -D_VMETA_VER=18

LOCAL_SRC_FILES:= \
	../main/src/main.c \
	src/appvmetadec.c \

LOCAL_SHARED_LIBRARIES := libcodecvmetadec libcodecmpeg4dec libvmetahal libmiscgen libvmeta

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= appvmetadec

include $(BUILD_EXECUTABLE)
