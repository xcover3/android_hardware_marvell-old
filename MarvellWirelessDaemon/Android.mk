LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \

LOCAL_SRC_FILES := \
        marvell_wireless_daemon.c

#ifeq ($(shell if [ $(PLATFORM_SDK_VERSION) -ge 9 ]; then echo big9; fi),big9)
#LOCAL_C_INCLUDES := \
#        external/bluetooth/bluez/lib
#else
#LOCAL_C_INCLUDES := \
#        external/bluetooth/bluez/include
#endif

LOCAL_SHARED_LIBRARIES := \
    libc\
    libcutils \
    libutils \
    libnetutils
#    libbluetooth

LOCAL_MODULE:=MarvellWirelessDaemon
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Werror
ifdef SD8787_NEED_CALIBRATE
LOCAL_CFLAGS += -DSD8787_NEED_CALIBRATE
endif

ifeq ($(SD8787_CAL_ON_BOARD),true)
LOCAL_CFLAGS += -DSD8787_CAL_ON_BOARD
# Make symlinks from /system/etc/firmware/mrvl to /data/misc/wireless
CONFIGS := \
	wifi_init_cfg.conf \
	bt_init_cfg.conf
SYMLINKS := $(addprefix $(TARGET_OUT)/etc/firmware/mrvl/,$(CONFIGS))
$(SYMLINKS): CAL_BINARY := /data/misc/wireless
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@ -> $(CAL_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(CAL_BINARY)/$(notdir $@) $@

ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

# We need this so that the installed files could be picked up based on the
# local module name
ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
    $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS)
endif
include $(BUILD_EXECUTABLE)
