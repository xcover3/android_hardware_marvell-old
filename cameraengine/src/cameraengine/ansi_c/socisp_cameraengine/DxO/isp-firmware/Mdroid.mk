LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../../CameraConfig.mk

ifneq (${DXO_FIRMWARE_VERSION}, )

ifeq (${ENABLE_DXO_ISP}, y)
LOCAL_PREBUILT_LIBS := \
        $(DXO_FIRMWARE_VERSION)/libdxoisp.a

LOCAL_MODULE_TAGS := optional

include $(BUILD_MULTI_PREBUILT)
endif

endif


