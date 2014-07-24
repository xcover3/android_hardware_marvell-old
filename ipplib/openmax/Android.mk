LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := \
	lib_il_basecore_wmmx2lnx.a \
	lib_il_ippomxmem_wmmx2lnx.a \
	lib_il_aacdec_wmmx2lnx.a \
	lib_il_aacenc_wmmx2lnx.a \
	lib_il_mp3dec_wmmx2lnx.a \
	lib_il_wmadec_wmmx2lnx.a \
	lib_il_vmetadec_wmmx2lnx.a \
	lib_il_vmetaenc_wmmx2lnx.a \
	lib_il_h264dec_wmmx2lnx.a \
	lib_il_h264enc_wmmx2lnx.a \
	lib_il_h263dec_wmmx2lnx.a \
	lib_il_h263enc_wmmx2lnx.a \
	lib_il_mpeg4aspdec_wmmx2lnx.a \
	lib_il_mpeg4enc_wmmx2lnx.a \
	lib_il_wmvdec_wmmx2lnx.a \
	lib_il_amrnbdec_wmmx2lnx.a \
	lib_il_amrnbenc_wmmx2lnx.a \
	lib_il_amrwbdec_wmmx2lnx.a \
	lib_il_amrwbenc_wmmx2lnx.a

LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

#netflix drm.play
ifeq ($(ENABLE_MARVELL_DRMPLAY),true)
include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := lib_il_drmplayer_wmmx2lnx.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)
endif

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_WHOLE_STATIC_LIBRARIES := \
        lib_il_basecore_wmmx2lnx \
        lib_il_ippomxmem_wmmx2lnx \
        lib_il_aacdec_wmmx2lnx \
        lib_il_aacenc_wmmx2lnx \
	lib_il_mp3dec_wmmx2lnx \
	lib_il_wmadec_wmmx2lnx \
        lib_il_vmetadec_wmmx2lnx \
        lib_il_vmetaenc_wmmx2lnx \
	lib_il_h264dec_wmmx2lnx \
	lib_il_h264enc_wmmx2lnx \
	lib_il_h263dec_wmmx2lnx \
	lib_il_h263enc_wmmx2lnx \
	lib_il_mpeg4aspdec_wmmx2lnx \
	lib_il_mpeg4enc_wmmx2lnx \
	lib_il_wmvdec_wmmx2lnx \
	lib_il_amrnbdec_wmmx2lnx \
	lib_il_amrnbenc_wmmx2lnx \
	lib_il_amrwbdec_wmmx2lnx \
	lib_il_amrwbenc_wmmx2lnx \


LOCAL_SHARED_LIBRARIES := \
        libcodecaacdec \
        libcodecaacenc \
	libcodecmp3dec \
	libcodecwmadec \
	libmiscgen \
	libdl \
        libcodech263dec \
        libcodech263enc \
        libvmetahal \
        libvmeta \
	libcodecamrnbdec \
	libcodecamrnbenc \
	libcodecamrwbdec \
	libcodecamrwbenc \

LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel

LOCAL_CFLAGS += \
        -D_MARVELL_AUDIO_AACDECODER \
        -D_MARVELL_AUDIO_AACENCODER \
        -D_MARVELL_AUDIO_MP3DECODER \
        -D_MARVELL_AUDIO_WMADECODER \
        -D_MARVELL_VIDEO_VMETADECODER \
        -D_MARVELL_VIDEO_VMETAENCODER \
        -D_MARVELL_VIDEO_H264DECODER \
        -D_MARVELL_VIDEO_H264ENCODER \
        -D_MARVELL_VIDEO_H263DECODER \
        -D_MARVELL_VIDEO_H263ENCODER \
        -D_MARVELL_VIDEO_MPEG4ASPDECODER \
        -D_MARVELL_VIDEO_MPEG4ENCODER \
        -D_MARVELL_VIDEO_WMVDECODER \
	-D_MARVELL_AUDIO_AMRNBDECODER \
	-D_MARVELL_AUDIO_AMRNBENCODER \
	-D_MARVELL_AUDIO_AMRWBDECODER \
	-D_MARVELL_AUDIO_AMRWBENCODER \

ifeq ($(ENABLE_MARVELL_DRMPLAY),true)
LOCAL_WHOLE_STATIC_LIBRARIES += lib_il_drmplayer_wmmx2lnx
LOCAL_SHARED_LIBRARIES += libdrmplaysink libmrvldrm
LOCAL_CFLAGS += -D_MARVELL_VIDEO_DRMPLAYER

endif

#LOCAL_SHARED_LIBRARIES += \
	libbmm \

LOCAL_SRC_FILES := \
	IppOmxComponentRegistry.c

LOCAL_C_INCLUDES := \
    frameworks/native/include/media/openmax

LOCAL_COPY_HEADERS := \
	include/OMX_IppDef.h

LOCAL_COPY_HEADERS_TO := \
	libipp

LOCAL_MODULE := libMrvlOmx
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
