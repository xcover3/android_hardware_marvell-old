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

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += hardware/marvell/pxa1088/original-kernel-headers

ifeq ($(shell if [ $(PLATFORM_SDK_VERSION) -ge 9 ]; then echo big9; fi),big9)
LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
    frameworks/native/include/media/openmax\
    frameworks/native/include/media/hardware\
    frameworks/av/include\
    hardware/marvell/pxa1088/libstagefrighthw/renderer\
    hardware/marvell/pxa1088/ipplib/openmax/include\
    frameworks/base/include/binder\
    frameworks/base/include/utils

LOCAL_CFLAGS += -DUSE_ION

else
# Set up the OpenCore variables.
include external/opencore/Config.mk
LOCAL_C_INCLUDES := $(PV_INCLUDES)
LOCAL_CFLAGS := $(PV_CFLAGS_MINUS_VISIBILITY)

LOCAL_CFLAGS += -DUSE_ION

LOCAL_C_INCLUDES += $(JNI_H_INCLUDE) \
    $(TOP)/external/opencore/extern_libs_v2/khronos/openmax/include
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq ($(BOARD_ENABLE_VMETADEC_POWEROPT_BYDEFAULT), true)
LOCAL_CFLAGS += -DENABLE_VMETADEC_POWEROPT
endif

ifeq ($(BOARD_ENABLE_VMETADEC_ADVANAVSYNC_1080P_BYDEFAULT), true)
LOCAL_CFLAGS += -DENABLE_VMETADEC_ADVANAVSYNC_1080P
endif

ifeq ($(BOARD_ENABLE_FAST_OVERLAY), true)
LOCAL_CFLAGS += -DFAST_OVERLAY
endif

ifeq ($(ENABLE_MARVELL_DRMPLAY),true)
LOCAL_CFLAGS += -D_MARVELL_DRM_PLAYER
endif

LOCAL_SRC_FILES += \
        stagefright_mrvl_omx_plugin.cpp \

LOCAL_SHARED_LIBRARIES :=       \
        libbinder               \
        libutils                \
        libcutils               \
        libstagefright_foundation


ifeq ($(shell if [ $(PLATFORM_SDK_VERSION) -ge 9 ]; then echo big9; fi),big9)
LOCAL_SHARED_LIBRARIES += \
        libMrvlOmx
else
LOCAL_SHARED_LIBRARIES += \
        libMrvlOmxWrapper
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_MODULE:= libstagefrighthw

include $(BUILD_SHARED_LIBRARY)

