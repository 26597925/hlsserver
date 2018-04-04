#!/bin/sh
export NDK=/home/wenford/vedio/android-ndk-r9b
export PREBUILT=$NDK/toolchains/arm-linux-androideabi-4.8/prebuilt
export PLATFORM=$NDK/platforms/android-15/arch-arm
export PREFIX=./out

build_one(){
	./configure --target-os=linux --prefix=$PREFIX \
							--enable-cross-compile \
							--enable-runtime-cpudetect \
							--disable-asm \
							--arch=arm \
							--cc=$PREBUILT/linux-x86_64/bin/arm-linux-androideabi-gcc \
							--cross-prefix=$PREBUILT/linux-x86_64/bin/arm-linux-androideabi- \
							--enable-stripping \
							--nm=$PREBUILT/linux-x86_64/bin/arm-linux-androideabi-nm \
							--sysroot=$PLATFORM \
							--enable-nonfree \
							--enable-version3 \
							--disable-everything \
							--enable-gpl \
							--disable-doc \
							--enable-bsfs \
							--enable-avfilter  \
							--enable-swscale  \
							--enable-swresample \
							--enable-avresample \
							--enable-demuxers \
							--enable-muxers \
							--enable-protocols \
							--enable-parsers \
							--enable-neon \
							--disable-ffplay \
							--disable-ffserver \
							--enable-ffmpeg \
							--enable-ffprobe \
							--enable-libx264 \
							--enable-filter=aresample \
							--enable-encoder=libx264 \
							--enable-encoder=libfaac \
							--enable-encoder=h264 \
							--enable-decoder=h264 \
							--enable-encoder=aac \
							--enable-decoder=aac \
							--enable-encoder=wmav2 \
							--enable-decoder=wmav2 \
							--enable-decoder=wmv1 \
							--enable-encoder=wmv3 \
							--enable-decoder=wmv3 \
							--enable-encoder=mpeg2video \
							--enable-decoder=mpeg2video \
							--enable-encoder=mp2 \
							--enable-decoder=mp2 \
							--enable-hwaccels \
							--enable-zlib \
							--disable-devices \
							--disable-avdevice \
							--enable-static	\
							--extra-cflags="-I/home/wenford/vedio/hls/x264-snapshot-20141218-2245 -fPIC -DANDROID -D__thumb__ -mthumb -Wfatal-errors -Wno-deprecated -mfloat-abi=softfp -mfpu=vfpv3-d16 -marm -march=armv7-a -pie -fPIE" \
							--extra-ldflags="-L/home/wenford/vedio/hls/x264-snapshot-20141218-2245 -pie -fPIE"

make -j8 install
}

build_one
