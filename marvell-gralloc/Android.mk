##############################################################################
#  
#
#
#  Copyright 2006, The Android Open Source Project
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.  
##############################################################################
#  
#  
##############################################################################


LOCAL_PATH	:= $(call my-dir)
include $(LOCAL_PATH)/Android.mk.def

# Setting LOCAL_PATH will mess up all-subdir-makefiles, so do it beforehand.
#SAVE_MAKEFILES := $(call all-subdir-makefiles)

#
# gralloc.<property>.so
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	gc_gralloc_alloc.cpp \
	gc_gralloc_fb.cpp \
	gc_gralloc_map.cpp \
	gralloc.cpp

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SHARED_LIBRARIES := liblog libcutils libGAL libutils
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)

# See hardware/libhardware/modules/README.android to see how this is named.

LOCAL_MODULE := gralloc.xo4

# With front buffer rendering, gralloc always provides the same buffer 
# when GRALLOC_USAGE_HW_FB. Obviously there is no synchronization with the display.
# Can be used to test non-VSYNC-locked rendering.
LOCAL_CFLAGS := \
	-DLOG_TAG=\"v_gralloc\" \
	-DDISABLE_FRONT_BUFFER \
	-DFRAMEBUFFER_PIXEL_FORMAT=$(FRAMEBUFFER_PIXEL_FORMAT)

LOCAL_C_INCLUDES += vendor/marvell/generic/displayservice/include \
                    vendor/marvell/generic/graphics/user/include \
                    vendor/marvell/generic/graphics/user/user/hal/inc \
                    vendor/marvell/generic/graphics/user/user/hal/user \
                    vendor/marvell/generic/graphics/user/user/hal/os/linux/user \
                    vendor/marvell/generic/graphics/user

LOCAL_SHARED_LIBRARIES += libgcu libutils libbinder

ifeq ($(strip $(BOARD_ENABLE_MULTI_DISPLAYS)),true)
    LOCAL_SHARED_LIBRARIES += libdisplaymodel
    LOCAL_C_INCLUDES += vendor/marvell/generic/displayservice
    LOCAL_CFLAGS += -DMRVL_SUPPORT_DISPLAY_MODEL=1
endif

LOCAL_CFLAGS += -DUSE_ION

LOCAL_CFLAGS += $(CFLAGS)

include $(BUILD_SHARED_LIBRARY)

