#
# Copyright (C) 2016 Android For Marvell Project <ctx.xda@gmail.com>
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

LOCAL_PATH := $(call my-dir)

include $($(LOCAL_PATH)/gpu-engine/gpuengine_modules.mk)

# Graphics
PRODUCT_PACKAGES += \
	libGAL \
	libGLESv1_CM_MRVL \
	libEGL_MRVL \
	libGLESv2_MRVL \
	libGLESv2SC \
	libgcu

# HWC HAL
PRODUCT_PACKAGES += \
	hwcomposer.mrvl \
	libHWComposerGC

# Gralloc
PRODUCT_PACKAGES += \
	gralloc.mrvl

# Stagefright
PRODUCT_PACKAGES += \
	libstagefrighthw

# Wireless
PRODUCT_PACKAGES += \
    libMarvellWireless \
    MarvellWirelessDaemon
