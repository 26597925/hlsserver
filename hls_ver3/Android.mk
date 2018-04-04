LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

define all-c-files-under
$(patsubst ./%,%, \
	$(shell cd $(LOCAL_PATH) ; \
		find $(1) -name "*.c" -and -not -name ".*") \
  )
 endef

LOCAL_MODULE := libhls

cmd-strip = $(TOOLCHAIN_PREFIX)strip --strip-debug -x $1 -g

LOCAL_SRC_FILES :=  $(call all-c-files-under, src)

LOCAL_STATIC_LIBRARIES += libz \
	libcutils \
	libc \
	liblog

#LOCAL_CFLAGS := -DANDROID_NDK \
#	-DDISABLE_IMPORTGL \
#	-fvisibility=hidden

LOCAL_CFLAGS := -std=c99 -O2 -W -Wall -pthread -pipe

LOCAL_LDFLAGS += -ldl -lz -llog

LOCAL_SHARED_LIBRARIES += liblog libdl

include $(BUILD_SHARED_LIBRARY)
