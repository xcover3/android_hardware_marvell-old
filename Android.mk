LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_BOARD_PLATFORM), mrvl)
ifeq ($(TARGET_SOC), pxa1088)

pxa1088_dirs := \
	MarvellWirelessDaemon \
	gpu-engine \
	graphics \
	hwcomposer \
	hwcomposerGC \
	ipplib \
	libMarvellWireless \
	libstagefrighthw \
	marvell-gralloc \
	phycontmem-lib \
	sd8787 \
	vmeta-lib

include $(call all-named-subdir-makefiles,$(pxa1088_dirs))

endif
endif

ifneq ($(BOARD_HAVE_BLUETOOTH_MRVL),)
	include $(call all-named-subdir-makefiles,libbt-vendor)
endif
