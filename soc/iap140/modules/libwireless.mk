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

PRODUCT_COPY_FILES += \
    $(TOP)/vendor/bsp/marvell/device/abox_edge/hal/wifi/daemons/wireless_daemon:/system/bin/wireless_daemon \
    $(TOP)/hardware/bsp/marvell/peripheral/libwireless/mwirelessd.rc:/system/etc/init/mwirelessd.rc

PRODUCT_PACKAGES += \
    libMarvellWireless

BOARD_SEPOLICY_DIRS += hardware/bsp/marvell/peripheral/libwireless/sepolicy
