#!/usr/bin/env bash

ROOT_DIR=`pwd`
BUILD_DIR=$ROOT_DIR/build_ios
OUT_DIR=$ROOT_DIR/outs_ios
DEPS_DIR=$ROOT_DIR/deps/isys
rm -rf $OUT_DIR
mkdir -p $OUT_DIR

# export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:/Users/zhiyongleng/works/FFmpeg/deps_ios/srt/build_ios/lib/pkgconfig"
PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
export PKG_CONFIG_LIBDIR
# ****************************************************************** #
OS=darwin       #android
ARCH=arm64      #x86_64     android: aarch64, arm
CPU=generic     #armv8-a, armv7-a
CC=clang

X264_HEADS=$DEPS_DIR/x264/include
X264_LIBS=$DEPS_DIR/x264/lib
X265_HEADS=$DEPS_DIR/x265/include
X265_LIBS=$DEPS_DIR/x265/lib
OPENSSL_HEADS=$DEPS_DIR/openssl/include
OPENSSL_LIBS=$DEPS_DIR/openssl/lib
SRT_HEADS=$DEPS_DIR/libsrt/include
SRT_LIBS=$DEPS_DIR/libsrt/lib
RIST_HEADS=$DEPS_DIR/librist/include
RIST_LIBS=$DEPS_DIR/librist/lib

echo begin configure dir:$ROOT_DIR OPENSSL_HEADS:$OPENSSL_HEADS
# ****************************************************************** #
# PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/lib/pkgconfig;/usr/local/opt/openssl/lib/pkgconfig" \
./configure \
    --prefix=$OUT_DIR \
    --arch=$ARCH \
    --target-os=$OS \
    --cc=$CC \
    --enable-cross-compile \
    --enable-runtime-cpudetect \
    --enable-static \
    --disable-shared \
    --enable-pic \
    --enable-gpl \
    --enable-version3 \
    --enable-small \
--enable-asm \
--disable-doc \
--disable-debug \
--disable-programs \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-avdevice \
--disable-postproc \
--disable-vulkan \
--disable-hwaccels \
--disable-cuda-llvm \
--disable-cuvid \
--disable-encoders \
    --enable-encoder=flv \
    --enable-encoder=aac \
    --enable-encoder=mjpeg \
    --enable-encoder=png \
	--enable-libx264 \
	--enable-encoder=libx264 \
    --enable-libx265 \
	--enable-encoder=libx265 \
--disable-decoders \
    --enable-decoder=mpeg4 \
    --enable-decoder=h264 \
    --enable-decoder=hevc \
    --enable-decoder=flv \
    --enable-decoder=aac \
    --enable-decoder=ac3 \
    --enable-decoder=mp3 \
    --enable-decoder=mjpeg \
    --enable-decoder=png \
--disable-muxers \
    --enable-muxer=mov \
    --enable-muxer=mp4 \
    --enable-muxer=mpegts \
    --enable-muxer=hls \
    --enable-muxer=flv \
--disable-demuxers \
    --enable-demuxer=hls \
    --enable-demuxer=mpegts \
    --enable-demuxer=mov \
    --enable-demuxer=flv \
    --enable-demuxer=live_flv \
    --enable-demuxer=rtsp \
    --enable-demuxer=mp3 \
    --enable-demuxer=matroska \
    --enable-demuxer=gif \
    --enable-demuxer=aac \
--enable-hwaccel=h264_videotoolbox \
--enable-hwaccel=hevc_videotoolbox \
--enable-openssl \
--pkg-config-flags="--static" \
--install_name_dir=@rpath \
--sysroot=$(xcrun --sdk iphoneos --show-sdk-path) \
--extra-cflags="-iwithsysroot /usr/include/libxml2 -arch $ARCH -miphoneos-version-min=9.0 -I$OPENSSL_HEADS -I$X264_HEADS -I$X265_HEADS" \
--extra-ldflags="-arch $ARCH -miphoneos-version-min=9.0 -Wl,-dead_strip -L$OPENSSL_LIBS -L$X264_LIBS -L$X265_LIBS" \
--extra-libs="-lssl -lcrypto -lx264 -lx265"

echo compile end


