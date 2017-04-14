# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../../include

LOCAL_CFLAGS += -D_IPP_LINUX -D_VMETA_VER=18

LOCAL_SRC_FILES:= \
	../main/src/main.c \
	src/appvmetaenc.c \

LOCAL_SHARED_LIBRARIES := libcodecvmetaenc libvmetahal libmiscgen libvmeta

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= appvmetaenc

include $(BUILD_EXECUTABLE)
