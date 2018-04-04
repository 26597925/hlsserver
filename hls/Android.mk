LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

define all-c-files-under
$(patsubst ./%,%, \
	$(shell cd $(LOCAL_PATH) ; \
		find $(1) -name "*.c" -and -not -name ".*") \
  )
 endef

LOCAL_MODULE := libhls

FFMPEG =$(LOCAL_PATH)/lib/ffmpeg
X264 = $(LOCAL_PATH)/lib/x264

LOCAL_C_INCLUDES := $(FFMPEG)

cmd-strip = $(TOOLCHAIN_PREFIX)strip --strip-debug -x $1 -g

#LOCAL_SRC_FILES := ffmpeg.c ffmpeg_opt.c cmdutils.c ffmpeg_filter.c mongoose.c util.c hls.c key_list.c hls_server.c hls_proxy.c libhls.c
LOCAL_SRC_FILES :=  $(call all-c-files-under, src)

LOCAL_STATIC_LIBRARIES += libz \
	libcutils \
	libc \
	liblog

#LOCAL_CFLAGS := -DANDROID_NDK \
#	-DDISABLE_IMPORTGL \
#	-fvisibility=hidden

LOCAL_CFLAGS := -std=c99 -O2 -W -Wall -pthread -pipe $(COPT)

LOCAL_LDFLAGS += $(FFMPEG)/libavformat/libavformat.a
LOCAL_LDFLAGS += $(FFMPEG)/libavcodec/libavcodec.a
LOCAL_LDFLAGS += $(FFMPEG)/libavresample/libavresample.a
LOCAL_LDFLAGS += $(FFMPEG)/libpostproc/libpostproc.a
LOCAL_LDFLAGS += $(FFMPEG)/libavfilter/libavfilter.a
LOCAL_LDFLAGS += $(FFMPEG)/libswscale/libswscale.a
LOCAL_LDFLAGS += $(FFMPEG)/libavutil/libavutil.a
LOCAL_LDFLAGS += $(FFMPEG)/libswresample/libswresample.a
LOCAL_LDFLAGS += $(X264)/libx264.a
LOCAL_LDFLAGS += $(X264)/libfaac.a
LOCAL_LDFLAGS += -ldl -lz


LOCAL_SHARED_LIBRARIES += liblog libdl

include $(BUILD_SHARED_LIBRARY)
