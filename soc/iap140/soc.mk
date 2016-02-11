#
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

LOCAL_PATH := hardware/bsp/marvell/soc/iap140

TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_KERNEL_ARCH := $(TARGET_ARCH)

TARGET_NO_BOOTLOADER := false
TARGET_NO_KERNEL := false

BOARD_KERNEL_CMDLINE := androidboot.console=ttyS1 console=ttyS1,115200 panic_debug uart_dma crashkernel=4k@0x8140000 user_debug=31 earlyprintk=uart8250-32bit,0xd4017000 cma=20M ddr_mode=2 RDCA=08140400 cpmem=32M@0x06000000 androidboot.exist.cp=18 androidboot.hardware=iap140 androidboot.selinux=enforcing

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/init.iap140.rc:root/init.iap140.rc \
	system/core/rootdir/init.usb.rc:root/init.usb.rc \
	$(LOCAL_PATH)/init.iap140.usb.rc:root/init.iap140.usb.rc \
	system/core/rootdir/ueventd.rc:root/ueventd.rc \
	$(LOCAL_PATH)/ueventd.iap140.rc:root/ueventd.iap140.rc

BOARD_SEPOLICY_DIRS += \
	$(LOCAL_PATH)/sepolicy

# Set up the local kernel.
TARGET_KERNEL_SRC := hardware/bsp/kernel/marvell/pxa-3.14
TARGET_KERNEL_DEFCONFIG := abox_edge_defconfig

# Add boot control
DEVICE_PACKAGES += \
	bootctrl.mrvl

# Include the HAL modules.
-include hardware/bsp/marvell/soc/iap140/hal_modules.mk
