# Copyright (C) 2016 The Android Open Source Project
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

include $(CLEAR_VARS)

LOCAL_MODULE := keystore.iap140
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PREBUILT_MODULE_FILE := $(TOP)/vendor/bsp/marvell/device/abox_edge/hal/keystore/keystore.iap140.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := $(TARGET_ARCH)
LOCAL_MODULE_RELATIVE_PATH := hw
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_MODULE := libtee_client
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PREBUILT_MODULE_FILE := $(TOP)/vendor/bsp/marvell/device/abox_edge/hal/keystore/libtee_client.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := $(TARGET_ARCH)
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)

LOCAL_MODULE := teec_sstd_ca
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_PREBUILT_MODULE_FILE := $(TOP)/vendor/bsp/marvell/device/abox_edge/hal/keystore/teec_sstd_ca
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := $(TARGET_ARCH)
include $(BUILD_PREBUILT)
