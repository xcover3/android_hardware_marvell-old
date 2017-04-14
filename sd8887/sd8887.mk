SD8887_DRVSRC:=$(TOPDIR)hardware/marvell/pxa1088/sd8887
sd8887_wifi:
	$(log) "making sd8887 wifi driver..."
	$(hide)cd $(SD8887_DRVSRC)/wlan_src && \
	make -j$(MAKE_JOBS) default
	#$(hide)mkdir -p $(KERNEL_OUTDIR)/modules/
	$(hide)cp $(SD8887_DRVSRC)/wlan_src/sd8xxx.ko $(KBUILD_OUTPUT)/modules/sd8887.ko
	$(hide)cp $(SD8887_DRVSRC)/wlan_src/mlan.ko $(KBUILD_OUTPUT)/modules/mlan.ko
	$(log) "sd8887 wifi driver done."

clean_sd8887_wifi:
	$(hide)cd $(SD8887_DRVSRC)/wlan_src &&\
	make clean
	rm -f $(KBUILD_OUTPUT)/modules/sd8887.ko
	$(log) "sd8887 wifi driver cleaned."
	rm -f $(KBUILD_OUTPUT)/modules/mlan.ko
	$(log) "mlan driver cleaned."

$(eval $(call add-module,sd8887_wifi) )

sd8887_mbtchar:
	$(log) "making sd8887 mbtchar BT driver..."
	$(hide)cd $(SD8887_DRVSRC)/mbtc_src && \
	$(MAKE) default
	$(hide)cp $(SD8887_DRVSRC)/mbtc_src/mbt8xxx.ko $(KBUILD_OUTPUT)/modules/mbt8xxx.ko
	$(log) "sd8887 mbtchar bt driver done."

clean_sd8887_mbtchar:
	$(hide)cd $(SD8887_DRVSRC)/mbtc_src &&\
	$(MAKE) clean
	$(hide)rm -f $(KBUILD_OUTPUT)/modules/mbt8xxx.ko
	$(log) "sd8887 mbtchar driver cleaned."

$(eval $(call add-module,sd8887_mbtchar) )
