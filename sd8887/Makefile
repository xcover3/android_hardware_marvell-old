KDR ?= /home/ben/android-src/kernel/olpc-kernel
#KDR ?= /home/nie/git/pxa168-linux-2.6.29

all: bt wlan install

wlan:
	+cd wlan_src; \
	$(MAKE) -j4 CROSS_COMPILE=arm-eabi- KERNELDIR=${KDR}

bt:
	+cd mbtc_src; \
	$(MAKE) -j4 CROSS_COMPILE=arm-eabi- KERNELDIR=${KDR} default

install:
	adb push wlan_src/mlan.ko /system/lib/modules/mlan.ko
	adb push wlan_src/sd8xxx.ko /system/lib/modules/sd8887.ko
	adb push mbtc_src/mbt8xxx.ko /system/lib/modules/mbt8xxx.ko
	adb push FwImage/sd8887_uapsta_w1.bin /system/etc/firmware/mrvl/sd8887_uapsta_w1.bin
	adb push FwImage/sd8887_uapsta.bin /system/etc/firmware/mrvl/sd8887_uapsta.bin

clean: wlan_clean bt_clean

wlan_clean:
	+$(MAKE) -C wlan_src clean
bt_clean:
	+$(MAKE) -C bt_src clean
