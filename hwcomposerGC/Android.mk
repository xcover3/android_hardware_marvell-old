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

# By default, ENABLE_HWC_GC_PATH should be false, as for most platforms it should be disabled.
# To Enable HWC GC path, should define ENABLE_HWC_GC_PATH as true in BoardConfig.mk
ENABLE_HWC_GC_PATH   ?= false

ifeq ($(ENABLE_HWC_GC_PATH), true)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libHWComposerGC.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES :=

LOCAL_C_INCLUDES := \
    hardware/libhardware/include \

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

LOCAL_SHARED_LIBRARIES := \
        libhardware \
        liblog \
        libutils \
        libcutils \
        libEGL \
        libbinder \
        libGAL \
        libsync \

LOCAL_MODULE := libHWComposerGC

LOCAL_WHOLE_STATIC_LIBRARIES := libHWComposerGC
LOCAL_CFLAGS := -DENABLE_HWC_GC_PATH -DINTEGRATED_WITH_MARVELL -g
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
