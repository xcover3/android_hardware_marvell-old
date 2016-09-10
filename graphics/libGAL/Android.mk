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

# halosuser
LOCAL_SRC_FILES += \
	gc_hal_user_dri.c \
	gc_hal_user_x.c \
	gc_hal_user_debug.c \
	gc_hal_user_dfb.c \
	gc_hal_user_fbdev.c \
	gc_hal_user_math.c \
	gc_hal_user_os.c \
	gc_hal_user_wayland.c 

# GAL
LOCAL_SRC_FILES += \
	gc_hal_user.c \
	gc_hal_user_brush.c \
	gc_hal_user_brush_cache.c \
	gc_hal_user_dump.c \
	gc_hal_user_format.c \
	gc_hal_user_hardware_blt.c \
	gc_hal_user_hardware_clear.c \
	gc_hal_user_hardware_filter_blt_blocksize.c \
	gc_hal_user_hardware_filter_blt_de.c \
	gc_hal_user_hardware_filter_blt_vr.c \
	gc_hal_user_hardware_pattern.c \
	gc_hal_user_hardware_pipe.c \
	gc_hal_user_hardware.c \
	gc_hal_user_hardware_primitive.c \
	gc_hal_user_hardware_query.c \
	gc_hal_user_hardware_shader.c \
	gc_hal_user_hardware_source.c \
	gc_hal_user_hardware_target.c \
	gc_hal_user_heap.c \
	gc_hal_user_platform.c \
	gc_hal_user_profiler.c \
	gc_hal_user_query.c \
	gc_hal_user_queue.c \
	gc_hal_user_raster.c \
	gc_hal_user_rect.c \
	gc_hal_user_shader.c \
	gc_hal_user_statistics.c \
	gc_hal_user_surface.c \

# halarchuser_vg
ifdef ($(VIVANTE_VG),true)
LOCAL_SRC_FILES += \
    gc_hal_user_hardware_context_vg.c \
    gc_hal_user_hardware_vg.c \
    gc_hal_user_hardware_vg_software.c \
    gc_hal_user_buffer_vg.c \
	gc_hal_user_vg.c
endif

# 2D
ifdef ($(VIVANTE_2D),true)
LOCAL_SRC_FILES += \
	gc_hal_user_buffer.c
endif

# 3D
ifdef ($(VIVANTE_3D),true)
LOCAL_SRC_FILES += \
	gc_hal_user_hardware_engine.c \
	gc_hal_user_hardware_frag_proc.c \
	gc_hal_user_hardware_texture.c \
	gc_hal_user_hardware_stream.c \
	gc_hal_user_hardware_composition.c \
	gc_hal_user_buffer.c \
	gc_hal_user_bufobj.c \
	gc_hal_user_surface.c \
	gc_hal_user_engine.c \
	gc_hal_user_index.c \
	gc_hal_user_md5.c \
	gc_hal_user_mem.c \
	gc_hal_user_texture.c \
	gc_hal_user_vertex.c \
	gc_hal_user_vertex_array.c \
	gc_hal_user_hardware_shader.c
endif

LOCAL_CFLAGS := \
	$(CFLAGS)

ifdef ($(VIVANTE_VG),true)
LOCAL_CFLAGS += \
	-DgcdENABLE_VG
endif

ifdef ($(VIVANTE_3D),true)
LOCAL_CFLAGS += \
	-DgcdENABLE_3D
endif

LOCAL_C_INCLUDES := \
	hardware/marvell/pxa1088/graphics/libGAL/include \

LOCAL_LDFLAGS := \
	-Wl,-z,defs \
	-Wl,--version-script=$(LOCAL_PATH)/libGAL.map

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libdl

LOCAL_MODULE         := libGAL
LOCAL_MODULE_TAGS    := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

