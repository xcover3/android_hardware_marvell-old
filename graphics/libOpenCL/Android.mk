LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libOpenCL.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

ifeq ($(USE_OPENCL),true)
include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libOpenCL

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL

LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE:= libOpenCL
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
endif
