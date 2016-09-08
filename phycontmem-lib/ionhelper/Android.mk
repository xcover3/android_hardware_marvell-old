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

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += hardware/marvell/pxa1088/original-kernel-headers

LOCAL_SRC_FILES :=  \
	ion_helper_lib.c

LOCAL_MODULE := libionhelper

LOCAL_SHARED_LIBRARIES := libutils liblog

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
