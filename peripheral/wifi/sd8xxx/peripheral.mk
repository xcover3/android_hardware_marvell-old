
# Copyright 2015 The Android Open Source Project
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
#

WLAN_BSP_SRC = hardware/bsp/marvell/peripheral/wifi/sd8xxx
WLAN_LIB_DIR = $(TOP)/vendor/bsp/marvell/device/abox_edge

PRODUCT_COPY_FILES += \
    $(WLAN_LIB_DIR)/hal/wifi/sd8xxx/firmware/sd8777_uapsta.bin:system/etc/firmware/mrvl/sd8777_uapsta.bin \
    $(WLAN_LIB_DIR)/hal/wifi/sd8xxx/firmware/txpwrlimit_cfg.bin:system/etc/firmware/mrvl/txpwrlimit_cfg.bin \
    $(WLAN_LIB_DIR)/hal/wifi/sd8xxx/calibration/txpwrlimit_cfg.bin:system/etc/firmware/mrvl/txpwrlimit_cfg.bin \
    $(WLAN_LIB_DIR)/hal/wifi/sd8xxx/calibration/WlanCalData_ext.conf:system/etc/firmware/mrvl/WlanCalData_ext.conf

WIFI_DRIVER_HAL_MODULE := wifi_driver.$(soc_name)
WIFI_DRIVER_HAL_PERIPHERAL := sd8xxx

BOARD_SEPOLICY_DIRS += $(WLAN_BSP_SRC)/sepolicy
