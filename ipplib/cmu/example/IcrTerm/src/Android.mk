LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += vendor/marvell/generic/ipplib/cmu/include

LOCAL_SRC_FILES := \
	IcrTerm.c \
	utility.c \
	viewmodes.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
        libcutils \
	libicrctrlsvr

LOCAL_MODULE := icrterm
include $(BUILD_EXECUTABLE)

