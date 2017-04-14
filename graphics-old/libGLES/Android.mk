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
LOCAL_PREBUILT_LIBS := \
	libGLES_CM.a \
	libGLESv2x.a \
	libGLESv2SC.a \
	libglslCompiler.a \
	libglslPreprocessor.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)


include $(CLEAR_VARS)
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libGLES_CM

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL liblog libEGL_MRVL

LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/egl
LOCAL_MODULE:= libGLESv1_CM_MRVL
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libGLESv2x \
	libGLESv2SC \
	libglslCompiler \
	libglslPreprocessor

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL libEGL_MRVL

LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/egl
LOCAL_MODULE:= libGLESv2_MRVL
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_WHOLE_STATIC_LIBRARIES := \
        libGLESv2SC \
        libglslCompiler \
        libglslPreprocessor

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libEGL_MRVL libGAL

LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/
LOCAL_MODULE:= libGLESv2SC
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
