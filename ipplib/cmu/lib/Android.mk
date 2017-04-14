LOCAL_PATH:=$(call my-dir)

MY_IPP_STATIC_LIBRARIES := \
	libicrdriver_android \
	libicrctrlsvr

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libicrdriver_android.a libicrctrlsvr.a
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

define translate-a-to-so
  $(eval include $(CLEAR_VARS)) \
  $(eval LOCAL_WHOLE_STATIC_LIBRARIES := libicrdriver_android libicrctrlsvr) \
  $(eval LOCAL_MODULE := libicrctrlsvr) \
  $(eval LOCAL_MODULE_TAGS := optional) \
  $(eval LOCAL_PRELINK_MODULE := false) \
  $(eval include $(BUILD_SHARED_LIBRARY))
endef

$(call translate-a-to-so, $(MY_IPP_STATIC_LIBRARIES))

