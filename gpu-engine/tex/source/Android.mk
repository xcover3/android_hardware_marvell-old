#Create Shared lib
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libgputex.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include

ifeq ($(ARCH_ARM_HAVE_NEON),true)
    LOCAL_CFLAGS += -D_MRVL_NEON_OPT
endif

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_MODULE:=libgputex
LOCAL_WHOLE_STATIC_LIBRARIES := libgputex

LOCAL_MODULE_TAGS:=optional
TARGET_PRELINK_MODULES:=false
include $(BUILD_SHARED_LIBRARY)
