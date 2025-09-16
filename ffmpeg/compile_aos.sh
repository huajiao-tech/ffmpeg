#!/bin/bash
set -e

PROJ_DIR=$(pwd)
echo $PROJ_DIR

BUILD_DIR=build_aos
mkdir -p $BUILD_DIR
DEPS_DIR=$PROJ_DIR/deps/asys

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

# 根据你当前的操作系统，使用对应的 NDK_PLATFORM 变量
NDK_PLATFORM=darwin-x86_64
# NDK_PLATFORM=linux-x86_64
# NDK_PLATFORM=windows-x86_64

# NDK_VERSION=21.4.7075529
NDK_VERSION=25.1.8937393
API=21

PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
export PKG_CONFIG_LIBDIR
# ****************************************************************** #
export ANDROID_NDK_HOME=$HOME/Library/Android/sdk/ndk/$NDK_VERSION
export TOOLCHAIN=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$NDK_PLATFORM/bin
export SYSROOT=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$NDK_PLATFORM/sysroot
export CROSS_PREFIX=$TOOLCHAIN/llvm-
#
export PATH=$TOOLCHAIN:$PATH


function build_android {
    ./configure \
    --prefix=$PREFIX \
    --cross-prefix=$CROSS_PREFIX \
    --target-os=android \
    --arch=$ARCH \
    --cpu=$CPU \
    --cc=$CC \
    --cxx=$CXX \
    --nm=${CROSS_PREFIX}nm \
    --strip=${CROSS_PREFIX}strip \
    --enable-cross-compile \
    --sysroot=$SYSROOT \
    --extra-cflags="$CFLAGS" \
    --extra-ldflags="$LDFLAGS" \
    --extra-libs="$EXTRALIBS" \
    --extra-ldexeflags=-pie \
    --enable-runtime-cpudetect \
  --enable-static \
  --disable-shared \
  --disable-ffmpeg \
  --disable-ffplay \
  --disable-ffprobe \
  --disable-avfilter \
  --disable-filters \
  --enable-postproc \
  --enable-small \
  --disable-armv5te \
  --disable-armv6 \
  --disable-armv6t2 \
  --enable-vfp \
  --enable-neon \
  --enable-pic \
  --enable-gpl \
  --enable-version3 \
  --enable-asm \
  --disable-x86asm \
  --disable-debug \
  --enable-avdevice \
  --disable-hwaccels \
  --disable-encoders \
    --enable-encoder=flv \
    --enable-encoder=aac \
    --enable-encoder=mjpeg \
    --enable-encoder=png \
    --enable-libx264 \
	  --enable-encoder=libx264 \
  --disable-muxers \
    --enable-muxer=mov \
    --enable-muxer=mp4 \
  --disable-decoders \
    --enable-decoder=mpeg4 \
    --enable-decoder=h264 \
    --enable-decoder=hevc \
    --enable-decoder=flv \
    --enable-decoder=aac \
    --enable-decoder=ac3 \
    --enable-decoder=mp3 \
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
  --enable-openssl \
  --pkg-config-flags="--static"
 
    make clean
    make -j8
    make install
}

# arm64-v8a
echo "build arm64-v8a -- begin"

ARCH=aarch64
CPU=armv8-a
PLATFORM=aarch64
PREFIX=$PROJ_DIR/$BUILD_DIR/arm64-v8a
export CC=$TOOLCHAIN/$ARCH-linux-android$API-clang
export CXX=$TOOLCHAIN/$ARCH-linux-android$API-clang++
CFLAGS="-I$OPENSSL_HEADS -I$X264_HEADS"
LDFLAGS="-L$OPENSSL_LIBS -L$X264_LIBS"
EXTRALIBS="-lssl -lcrypto -lx264"
build_android

echo "build arm64-v8a -- end"

#armeabi-v7a
# echo "build armeabi-v7a -- begin" 

# ARCH=arm
# CPU=armv7-a
# PLATFORM=armv7a
# PREFIX=$PROJ_DIR/$BUILD_DIR/armeabi-v7a
# export CC=$TOOLCHAIN/$ARCH-linux-android$API-clang
# export CXX=$TOOLCHAIN/$ARCH-linux-android$API-clang++
# CFLAGS="-mfloat-abi=softfp -march=$CPU"
# LDFLAGS="-Wl,--fix-cortex-a8"

# echo "build arm64-v8a -- end"