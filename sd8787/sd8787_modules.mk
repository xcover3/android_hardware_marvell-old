# driver
PRODUCT_PACKAGES += \
	mlanconfig \
	mlanutl \
	mlan2040coex \
	uaputl.exe \
	mlanevent.exe \

# Shared
PRODUCT_PACKAGES += \
	rfkill \
	libMarvellWireless \
	MarvellWirelessDaemon \

# Wifi
PRODUCT_PACKAGES += \
	iperf \
	wireless_tool \
	iwconfig \
	iwlist \
	iwpriv \
	iwspy \
	iwgetid \
	iwevent \
	ifrename \
	macadd \
	WapiCertMgmt \
	simal \
	WiFiDirectDemo \

# Bluetooth
PRODUCT_PACKAGES += \
	sdptool \
	hciconfig \
	hcitool \
	l2ping \
	hciattach \
	rfcomm \
	avinfo \

#
# copy sd8xxx_uapsta.bin to /etc/firmware/mrvl
#
PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/sd8787_uapsta.bin:system/etc/firmware/mrvl/sd8787_uapsta.bin

PRODUCT_COPY_FILES += vendor/marvell/generic/sd8787/FwImage/sd8777_uapsta.bin:system/etc/firmware/mrvl/sd8777_uapsta.bin
