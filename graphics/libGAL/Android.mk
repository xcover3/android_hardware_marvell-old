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
include $(LOCAL_PATH)/Android.mk.def

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	gc_hal_user_hardware_blt.c \
	gc_hal_user_hardware_clear.c \
	gc_hal_user_hardware_filter_blt_de.c \
	gc_hal_user_hardware_filter_blt_vr.c \
	gc_hal_user_hardware.c \
	gc_hal_user_hardware_pattern.c \
	gc_hal_user_hardware_pipe.c \
	gc_hal_user_hardware_primitive.c \
	gc_hal_user_hardware_query.c \
	gc_hal_user_hardware_source.c \
	gc_hal_user_hardware_target.c \
	gc_hal_user_brush.c \
	gc_hal_user_brush_cache.c \
	gc_hal_user_buffer.c \
	gc_hal_user.c \
	gc_hal_user_dump.c \
	gc_hal_user_heap.c \
	gc_hal_user_paint.c \
	gc_hal_user_path.c \
	gc_hal_user_profiler.c \
	gc_hal_user_query.c \
	gc_hal_user_queue.c \
	gc_hal_user_raster.c \
	gc_hal_user_rect.c \
	gc_hal_user_surface.c \
	gc_hal_user_vg.c \
	gc_hal_user_debug.c \
	gc_hal_user_os.c \
	gc_hal_user_math.c

ifneq ($(VIVANTE_NO_3D),1)
LOCAL_SRC_FILES += \
	gc_hal_user_hardware_engine.c \
	gc_hal_user_hardware_frag_proc.c \
	gc_hal_user_hardware_texture.c \
	gc_hal_user_hardware_stream.c \
	gc_hal_user_hardware_composition.c \
	gc_hal_user_engine.c \
	gc_hal_user_index.c \
	gc_hal_user_md5.c \
	gc_hal_user_mem.c \
	gc_hal_user_texture.c \
	gc_hal_user_vertex.c \
	gc_hal_user_vertex_array.c

LOCAL_SRC_FILES += \
	gc_hal_user_hardware_code_gen.c \
	gc_hal_user_hardware_linker.c \
	gc_hal_user_hardware_shader.c \
	gc_hal_user_compiler.c \
	gc_hal_user_linker.c \
	gc_hal_optimizer.c \
	gc_hal_optimizer_dump.c \
	gc_hal_optimizer_util.c \
	gc_hal_user_loadtime_optimizer.c
endif

LOCAL_CFLAGS := \
	$(CFLAGS)

ifeq ($(VIVANTE_NO_3D),1)
LOCAL_CFLAGS += \
	-DVIVANTE_NO_3D
endif

LOCAL_C_INCLUDES := \
	hardware/marvell/pxa1088/graphics/libGAL/include \

ifeq ($(USE_LINUX_MODE_FOR_ANDROID), 1)
	LOCAL_C_INCLUDES += \
		hardware/marvell/pxa1088/graphics/include/sdk/inc

	LOCAL_SRC_FILES += \
		gc_hal_user_fbdev.c
endif

LOCAL_LDFLAGS := \
	-Wl,-z,defs \
	-Wl,--version-script=$(LOCAL_PATH)/libGAL.map

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libdl

LOCAL_GENERATED_SOURCES := \

LOCAL_MODULE         := libGAL
LOCAL_MODULE_TAGS    := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

