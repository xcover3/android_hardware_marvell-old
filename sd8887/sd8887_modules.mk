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
PRODUCT_COPY_FILES += hardware/marvell/pxa1088/sd8887/FwImage/sd8887_uapsta.bin:system/etc/firmware/mrvl/sd8887_uapsta.bin