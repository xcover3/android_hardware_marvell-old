ifeq ($(WIFI_DRIVER_MODULE_NAME), sd8xxx)

	LOCAL_PATH := $(call my-dir)

	include $(call all-subdir-makefiles,$(LOCAL_PATH))
endif