#!/usr/bin/env bash

ROOT_DIR=`pwd`
BUILD_DIR=$ROOT_DIR/build_mos
OUT_DIR=$ROOT_DIR/mos_outs
DEPS_DIR=$ROOT_DIR/deps/msys
rm -rf $OUT_DIR
mkdir -p $OUT_DIR

# ****************************************************************** #
OS=darwin       #android
ARCH=x86_64     #aarch64, arm
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
./configure \
    --prefix=$OUT_DIR \
    --arch=$ARCH \
    --target-os=$OS \
    --cc=$CC \
    --enable-runtime-cpudetect \
    --enable-cross-compile \
    --enable-static \
    --disable-shared \
    --enable-pic \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --enable-small \
--enable-asm \
--disable-doc \
--disable-debug \
--disable-programs \
--disable-ffmpeg \
--disable-ffplay \
--disable-postproc \
--disable-vulkan \
--disable-libxcb \
--disable-xlib \
--disable-hwaccels \
--disable-audiotoolbox \
--disable-filter=yadif_videotoolbox \
--enable-openssl \
--enable-libx264 --enable-encoder=libx264 \
--enable-libx265 --enable-encoder=libx265 \
--enable-libsrt \
--pkg-config-flags="--static" \
--install_name_dir='@rpath' \
--sysroot=$(xcrun --sdk macosx --show-sdk-path) \
--extra-cflags="-iwithsysroot /usr/include/libxml2 -arch x86_64 -mmacosx-version-min=10.13 -I$X264_HEADS -I$X265_HEADS -I$OPENSSL_HEADS -I$X264_HEADS -I$SRT_HEADS" \
--extra-ldflags="-arch x86_64 -mmacosx-version-min=10.13 -Wl,-dead_strip -Wl,-rpath,@loader_path -Wl,-rpath,@executable_path/../Frameworks -Wl,-rpath,@loader_path/Libraries -Wl,-rpath,@loader_path/../lib -L$X264_LIBS -L$X265_LIBS -L$OPENSSL_LIBS -L$SRT_LIBS" \
--extra-libs="-lsrt -lssl -lcrypto -lx264"

echo compile end


