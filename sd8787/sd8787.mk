SD8787_DRVSRC:=$(TOPDIR)vendor/marvell/generic/sd8787
sd8787_wifi:
	$(log) "making sd8787 wifi driver..."
	$(hide)cd $(SD8787_DRVSRC)/wlan_src && \
	make -j$(MAKE_JOBS) default
	#$(hide)mkdir -p $(KERNEL_OUTDIR)/modules/
	$(hide)cp $(SD8787_DRVSRC)/wlan_src/sd8xxx.ko $(KBUILD_OUTPUT)/modules/sd8787.ko
	$(hide)cp $(SD8787_DRVSRC)/wlan_src/mlan.ko $(KBUILD_OUTPUT)/modules/mlan.ko
	$(log) "sd8787 wifi driver done."

clean_sd8787_wifi:
	$(hide)cd $(SD8787_DRVSRC)/wlan_src &&\
	make clean
	rm -f $(KBUILD_OUTPUT)/modules/sd8787.ko
	$(log) "sd8787 wifi driver cleaned."
	rm -f $(KBUILD_OUTPUT)/modules/mlan.ko
	$(log) "mlan driver cleaned."

$(eval $(call add-module,sd8787_wifi) )

sd8787_mbtchar:
	$(log) "making sd8787 mbtchar BT driver..."
	$(hide)cd $(SD8787_DRVSRC)/mbtc_src && \
	$(MAKE) default
	$(hide)cp $(SD8787_DRVSRC)/mbtc_src/mbt8xxx.ko $(KBUILD_OUTPUT)/modules/mbt8xxx.ko
	$(log) "sd8787 mbtchar bt driver done."

clean_sd8787_mbtchar:
	$(hide)cd $(SD8787_DRVSRC)/mbtc_src &&\
	$(MAKE) clean
	$(hide)rm -f $(KBUILD_OUTPUT)/modules/mbt8xxx.ko
	$(log) "sd8787 mbtchar driver cleaned."

$(eval $(call add-module,sd8787_mbtchar) )
