# Copyright (C) 2016 Android For Marvell Project <ctx.xda@gmail.com>
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

LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so
include $(CLEAR_VARS)

BOARD_ENABLE_WFD_OPTIMIZATION ?= true 
BOARD_ENABLE_OVERLAY ?= false

LOCAL_SRC_FILES := \
    hwcomposer.cpp \
    HWCDisplayEventMonitor.cpp

LOCAL_SRC_FILES += \
    HWBaselayComposer.cpp

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_SRC_FILES += \
    HWOverlayComposer.cpp \
    OverlayDisplayEngine/IDisplayEngine.cpp \
    OverlayDisplayEngine/IOverlay.cpp \
    OverlayDisplayEngine/V4L2Overlay.cpp \
    HWCFenceManager.cpp
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_SRC_FILES += \
    HWVirtualComposer.cpp \
    GcuEngine.cpp
endif

LOCAL_C_INCLUDES := \
    hardware/libhardware/include \
    hardware/marvell/pxa1088/marvell-gralloc \
    hardware/marvell/pxa1088/graphics/user/include 

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_C_INCLUDES += \
    hardware/marvell/pxa1088/hwcomposer/OverlayDisplayEngine 
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_C_INCLUDES += \
    frameworks/native/services 
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := \
        libui \
        liblog \
        libutils \
        libcutils \
        libEGL \
        libgcu \
        libhardware \
        libhardware_legacy

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_SHARED_LIBRARIES += libbinder \
                          libsync
endif

LOCAL_MODULE := hwcomposer.mrvl

LOCAL_CFLAGS:= -DLOG_TAG=\"HWComposerMarvell\"

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_CFLAGS += -DENABLE_OVERLAY
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_CFLAGS += -DENABLE_WFD_OPTIMIZATION
endif

LOCAL_C_INCLUDES += hardware/marvell/pxa1088/hwcomposerGC
LOCAL_SHARED_LIBRARIES += libHWComposerGC
LOCAL_CFLAGS += -DENABLE_HWC_GC_PATH
LOCAL_CFLAGS += -DINTEGRATED_WITH_MARVELL

LOCAL_CFLAGS += -g

ifeq ($(ENABLE_OVERLAY_ONLY_HDMI_CONNECTED), true)
LOCAL_CFLAGS += -DENABLE_OVERLAY_ONLY_HDMI_CONNECTED
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

#LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES += \
    HWCFenceTest.cpp \
    HWCFenceManager.cpp \


LOCAL_C_INCLUDES := \
    hardware/libhardware/include \
    hardware/marvell/pxa1088/marvell-gralloc \
    hardware/marvell/pxa1088/graphics/user/include 


LOCAL_CFLAGS += -g

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libbinder libsync libui
LOCAL_PRELINK_MODULE := false
# LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bin
LOCAL_LDLIBS += -lpthread -lrt
LOCAL_MODULE := HWCFenceTest
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_MODULE_TAGS := optional

# include $(BUILD_EXECUTABLE)
