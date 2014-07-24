CE_LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# camera-engine lib
include $(CE_LOCAL_PATH)/src/cameraengine/ansi_c/Mdroid_cameraengine.mk
include $(CE_LOCAL_PATH)/src/cameraengine/ansi_c/Mdroid_sensorhal.mk

# camera sample application in /system/bin/MrvlCameraDemo

include $(CE_LOCAL_PATH)/example/cameraengine/Mdroid.mk

# sensor vendor deliverables
include $(CE_LOCAL_PATH)/src/cameraengine/ansi_c/extisp_sensorhal/ov5642/Mdroid.mk

# DxO deliverables
include $(CE_LOCAL_PATH)/src/cameraengine/ansi_c/socisp_cameraengine/DxO/isp-firmware/Mdroid.mk

# tool library of camera usage model, including: 
# 1. libippexif to generate exif image from JPEG image
include $(CE_LOCAL_PATH)/tool/lib/Mdroid.mk

# tool library sample application, including:
# 1. MrvlExifGenerator in /system/bin
#include $(CE_LOCAL_PATH)/tool/example/Mdroid.mk
