GPU_ENGINE_TOP_PATH := $(call my-dir)

CEU_PATH := $(GPU_ENGINE_TOP_PATH)/ceu

CSC_PATH := $(GPU_ENGINE_TOP_PATH)/csc/source

TEX_PATH := $(GPU_ENGINE_TOP_PATH)/tex/source

VEU_PATH := $(GPU_ENGINE_TOP_PATH)/veu/source

LOCAL_PATH := $(call my-dir)

include $(CEU_PATH)/Android.mk
include $(CSC_PATH)/Android.mk
include $(TEX_PATH)/Android.mk
#include $(VEU_PATH)/Android.mk

