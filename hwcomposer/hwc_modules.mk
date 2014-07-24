# displayservice modules

PRODUCT_PACKAGES += \
    hwcomposer.xo4

ifeq ($(ENABLE_HWC_GC_PATH), true)
PRODUCT_PACKAGES += \
    libHWComposerGC
endif


