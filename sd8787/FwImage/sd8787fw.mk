#
# copy sd8787_uapsta.bin to /etc/firmware/mrvl
#

include $(CLEAR_VARS)

PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/sd8787_uapsta.bin:system/etc/firmware/mrvl/sd8787_uapsta.bin
PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/sd8777_uapsta.bin:system/etc/firmware/mrvl/sd8777_uapsta.bin

PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/libertas/sd8686_v9.bin:system/etc/firmware/libertas/sd8686_v9.bin
PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/libertas/sd8686_v9_helper.bin:system/etc/firmware/libertas/sd8686_v9_helper.bin
