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

graphics_dirs := \
	libEGL \
	libGAL \
	libGLES \
	libgcu

ifeq ($(USE_OPENCL),true)
graphics_dirs += libOpenCL
endif

include $(call all-named-subdir-makefiles,$(graphics_dirs))

# !!! hack to resolve the dependency of libGLESv2_MRVL.so on libEGL_MRVL.so
$(TARGET_OUT_SHARED_LIBRARIES)/libEGL_MRVL.so:$(TARGET_OUT_SHARED_LIBRARIES)/egl/libEGL_MRVL.so