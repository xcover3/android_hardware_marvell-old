LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_blit.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_blit
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_rotate.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_rotate
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_scale.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_scale
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_scale_quality.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_scale_quality
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_sub_rect.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_sub_rect
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_filter_blit.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_filter_blit
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_yuv2rgb.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_yuv2rgb
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_blend_global.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_blend_global
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_blend_pixel.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_blend_pixel
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_bmm.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_bmm
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_multimap.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_multimap
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_compose.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_compose
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_cliprect.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_cliprect
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_flip_rotate.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_flip_rotate
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_display.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_display
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_tile_blend.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_tile_blend
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_tile_fill.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_tile_fill
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_tile.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_tile
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_multi-tile.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_multi-tile
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_super-tile.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_super-tile
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include/

LOCAL_SRC_FILES := \
    sample_multi-super-tile.c

LOCAL_SHARED_LIBRARIES := \
    libgcu \
    libcutils \

LOCAL_MODULE_PATH := $(LOCAL_PATH)
LOCAL_MODULE := sample_multi-super-tile
LOCAL_MODULE_TAGS := samples
include $(BUILD_EXECUTABLE)

