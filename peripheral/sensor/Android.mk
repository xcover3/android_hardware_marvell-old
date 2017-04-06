# Copyright (C) 2008 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

BOARD_HAVE_AVAGO=true
BOARD_HAVE_DYNA=true

COMMON_CFLAGS := -DLOG_TAG=\"Sensors\" -Wno-unused-parameter
COMMON_C_INCLUDES += $(LOCAL_PATH)/sensors

ifeq ($(BOARD_HAVE_AVAGO),true)
COMMON_CFLAGS += -DBOARD_HAVE_AVAGO
endif
ifeq ($(BOARD_HAVE_DYNA),true)
COMMON_CFLAGS += -DBOARD_HAVE_DYNA
endif

#-----------------------------------------------------------
# Build sensors.iap140
include $(CLEAR_VARS)
LOCAL_MODULE := sensors.iap140
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_CFLAGS := $(COMMON_CFLAGS)
LOCAL_C_INCLUDES := $(COMMON_C_INCLUDES)

LOCAL_SRC_FILES += SensorBase.cpp \
    InputEventReader.cpp \
    sensors/LightSensor.cpp \
    sensors/ProximitySensor.cpp \
    MrvlSensorBase.cpp \
    MrvlSensorWrapper.cpp \
    sensors_hal.cpp

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libcutils \
    libutils

include $(BUILD_SHARED_LIBRARY)
