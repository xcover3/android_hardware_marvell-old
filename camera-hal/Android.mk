LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

USE_CAMERA_STUB:=false

ifneq ($(strip $(USE_CAMERA_STUB)),true)

# put your source files here

LOCAL_SRC_FILES:= \
                hal/ImageBuf.cpp \
                display/ANativeWindowDisplay.cpp\
                setting/CameraSetting.cpp \
                camctrl/exif_helper.cpp \
                camctrl/FakeCam.cpp \
                camctrl/Engine.cpp \
                config/ConfigParser.cpp\
                hal/CameraHardware.cpp \
                CameraModule.cpp \
                hal/CameraMsg.cpp \
                hal/JpegCompressor.cpp \
                ../../../../frameworks/base/core/jni/android/graphics/YuvToJpegEncoder.cpp

# put the libraries you depend on here.
LOCAL_SHARED_LIBRARIES:= \
        libbinder            \
        libui                \
        libutils             \
        libcutils            \
        liblog               \
        libmedia             \
        libcamera_client     \
        libmiscgen           \
        libcodecjpegdec      \
        libcodecjpegenc      \
        libphycontmem        \
        libandroid_runtime \
        libjpeg \
        libskia \
        libexpat

LOCAL_CFLAGS += -I hardware/libhardware/include  \
                                -I vendor/marvell/generic/ipplib/include \
                                -I vendor/marvell/generic/camera-hal/hal \
                                -I vendor/marvell/generic/camera-hal/camctrl \
                                -I vendor/marvell/generic/camera-hal/display \
                                -I vendor/marvell/generic/camera-hal/setting \
                                -I vendor/marvell/generic/camera-hal/config \
                                -I vendor/marvell/generic/cameraengine/include \
                                -I vendor/marvell/generic/cameraengine/src/cameraengine/ansi_c/_include \
                                -I vendor/marvell/generic/bmm-lib/lib \
                                -I vendor/marvell/generic/cameraengine/tool/include \
                                -I vendor/marvell/generic/marvell-gralloc \
                                -I vendor/marvell/generic/graphics/user/user/hal/inc/ \
                                -I frameworks/base/core/jni/android/graphics \
                                -I external/expat/lib \
                                -I external/skia/include/core \
                                -I external/skia/include/images \
                                -I external/jpeg \

# hacking solution for memory sharing for ANW/camera_memory_t
#LOCAL_CFLAGS += -I frameworks/base/services/camera/libcameraservice

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

# for cameraengine log
LOCAL_CFLAGS += -D ANDROID_CAMERAENGINE \
#                                -D CAM_LOG_VERBOSE

#LOCAL_PRELINK_MODULE:=false

LOCAL_SHARED_LIBRARIES += libcameraengine \

LOCAL_STATIC_LIBRARIES += \
        libcodecjpegdec \
        libippcam \
        libippexif \
        FaceTrack \
        libpower

# put your module name here
LOCAL_MODULE:= camera.xo4

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

include $(BUILD_SHARED_LIBRARY)

endif
