LOCAL_PATH := $(call my-dir)
MY_LOCAL_PATH := $(LOCAL_PATH)

define all-c-files-under
$(patsubst ./%,%, \
	$(shell cd $(LOCAL_PATH) ; \
		find $(1) -name "*.c" -and -not -name ".*") \
  )
 endef

include $(CLEAR_VARS)
LOCAL_MODULE    := libhls
LOCAL_SRC_FILES :=  $(call all-c-files-under, src)

LOCAL_C_INCLUDES := $(MY_LOCAL_PATH)/lib/libcurl/include

LOCAL_STATIC_LIBRARIES := libcurl

LOCAL_SHARED_LIBRARIES += libssl libcrypto libz
LOCAL_SHARED_LIBRARIES += liblog libcutils

LOCAL_LDLIBS := -ldl

#include $(BUILD_EXECUTABLE)

include $(BUILD_SHARED_LIBRARY)
include $(MY_LOCAL_PATH)/lib/libcurl/Android.mk


