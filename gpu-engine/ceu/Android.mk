LOCAL_PATH := $(call my-dir)
GPU_CEU_LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#The gpu-ceu lib: libceu
include $(GPU_CEU_LOCAL_PATH)/source/Android.mk

#The resource apk
include $(GPU_CEU_LOCAL_PATH)/res/GPUEngine-CEU-Res/Android.mk

#The sample application
include $(GPU_CEU_LOCAL_PATH)/sample/Android.mk
