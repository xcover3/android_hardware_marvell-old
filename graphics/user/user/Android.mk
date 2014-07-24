LOCAL_PATH:= $(call my-dir)
TAG := MRVL

BUILD_OPTION_USE_GPUTEX ?= 1

#
# copy egl.cfg to /system/lib/egl
#
ifeq ($(strip $(MRVL_GCU2D_V3)),true)
GCU_LIB := libgcu3
else
GCU_LIB := libgcu
endif

include $(CLEAR_VARS)

file := $(TARGET_OUT)/lib/egl/egl.cfg
$(file) : $(LOCAL_PATH)/egl.cfg | $(ACP)
	$(transform-prebuilt-to-target)
ALL_PREBUILT += $(file)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := \
	libEGL.a \
	libeglapi.a \
	libeglos.a \
	libGAL.a \
	libGLES_CM.a \
	libhalarchuser.a \
	libhalosuser.a \
	libGLESv2x.a \
	libGLESv2SC.a \
	libglslCompiler.a \
	libglslPreprocessor.a \
	$(GCU_LIB).a \
	libOpenCL.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

#
# Build the GAL library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
	
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libGAL \
	libhalarchuser \
	libhalosuser

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl liblog

ifeq ($(strip $(BUILD_OPTION_USE_GPUTEX)),1)
LOCAL_SHARED_LIBRARIES += libgputex
endif

LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE:= libGAL
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


#
# Build the EGL library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
	
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libGLES_CM

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL liblog libEGL_$(TAG)
LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/egl
LOCAL_MODULE:= libGLESv1_CM_$(TAG)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#
# Build the OpenGL ES 1.x library
#


include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
	
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libEGL \
	libeglapi \
	libeglos

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL liblog libhardware
LOCAL_LDLIBS := -ldl -llog -lhardware
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/egl
LOCAL_MODULE:= libEGL_$(TAG)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#
# Build the OpenGL ES 2.x library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
	
LOCAL_WHOLE_STATIC_LIBRARIES := \
	libGLESv2x \
	libGLESv2SC \
	libglslCompiler \
	libglslPreprocessor

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL libEGL_$(TAG)
LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/egl
LOCAL_MODULE:= libGLESv2_$(TAG)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES :=

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libGLESv2SC \
        libglslCompiler \
        libglslPreprocessor

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libEGL_$(TAG) libGAL
LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/
LOCAL_MODULE:= libGLESv2SC
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#
# Build the gcu library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
	
LOCAL_WHOLE_STATIC_LIBRARIES := \
	$(GCU_LIB)

LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL
LOCAL_LDLIBS := -ldl -llog
#LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/
LOCAL_MODULE:= libgcu
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#
# Build the OpenCL library
#
ifeq ($(USE_OPENCL),1)
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libOpenCL
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libcutils libutils libdl libGAL
LOCAL_LDLIBS := -ldl -llog
LOCAL_MODULE:= libOpenCL
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
endif

# !!! hack to resolve the dependency of libGLESv2_MRVL.so on libEGL_MRVL.so
$(TARGET_OUT_SHARED_LIBRARIES)/libEGL_$(TAG).so:$(TARGET_OUT_SHARED_LIBRARIES)/egl/libEGL_$(TAG).so




