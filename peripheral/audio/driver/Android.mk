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

ifneq ($(strip $(BOARD_AUDIO_COMPONENT_APU)),)

LOCAL_PATH:= $(call my-dir)

ifneq ($(BOARD_USES_GENERIC_AUDIO),true)

# prebuild the Marvell proprietory audio codec driver
include $(CLEAR_VARS)

LOCAL_MODULE := libacm
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PREBUILT_MODULE_FILE := $(TOP)/vendor/bsp/marvell/device/abox_edge/hal/audio/libacm.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := $(TARGET_ARCH)
include $(BUILD_PREBUILT)

# build audio.primary.mrvl.so
include $(CLEAR_VARS)

LOCAL_MODULE_TARGET_ARCH := arm

LOCAL_SRC_FILES:= \
    audio_hw_mrvl.c \
    audio_path.c \
    audio_config.c \
    audio_effect_mrvl.c

LOCAL_C_INCLUDES += \
    external/icu/icu4c/source/common \
    external/tinyalsa/include/ \
    $(LOCAL_PATH)/include/acm \
    system/media/audio/include \
    system/media/audio_utils/include/audio_utils \
    system/media/audio_effects/include/audio_effects \
    frameworks/av/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libtinyalsa \
    libhardware \
    libaudioutils \
    libeffects \
    libexpat \
    libacm

LOCAL_MODULE:= audio.primary.mrvl
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wno-unused-parameter

ifeq ($(strip $(BOARD_ENABLE_ADVANCED_AUDIO)),true)
   LOCAL_CFLAGS += -DWITH_ADVANCED_AUDIO
endif

ifeq ($(strip $(BOARD_WITH_STEREO_SPKR)),true)
   LOCAL_CFLAGS += -DWITH_STEREO_SPKR
endif

ifeq ($(strip $(BOARD_WITH_HEADSET_OUTPUT_ONLY)),true)
   LOCAL_CFLAGS += -DROUTE_SPEAKER_TO_HEADSET
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION) -D_POSIX_SOURCE

ifeq ($(strip $(BOARD_AUDIO_COMPONENT_APU)), MAP-LITE)
   LOCAL_CFLAGS += -DWITH_MAP_LITE
endif

include $(BUILD_SHARED_LIBRARY)

endif

endif
