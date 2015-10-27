# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(WIFI_DRIVER_HAL_PERIPHERAL), sd8xxx)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        wifi_driver_hal_sd8xxx.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH) \
        device/generic/brillo/wifi_driver_hal/include \
        hardware/bsp/marvell/peripheral/libwireless

LOCAL_SHARED_LIBRARIES := \
        libc    \
        libcutils \
        libutils \
        libMarvellWireless

LOCAL_STATIC_LIBRARIES +=

LOCAL_MODULE:= $(WIFI_DRIVER_HAL_MODULE)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_PRELINK_MODULE := false

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

# We need this so that the installed files could be picked up
# based on the local module's name
ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
    $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS)

include $(BUILD_SHARED_LIBRARY)

endif
