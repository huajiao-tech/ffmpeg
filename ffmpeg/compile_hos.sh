#!/bin/bash
make clean
set -e

ROOT_DIR=`pwd`
DEPS_DIR=$ROOT_DIR/deps/hsys

BUILD_DIR=build_hos
mkdir -p $BUILD_DIR

OPENSSL=$DEPS_DIR/openssl
LIBX264=$DEPS_DIR/libx264

export OHOS_SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony
export AS=${OHOS_SDK}/native/llvm/bin/llvm-as
export CC="${OHOS_SDK}/native/llvm/bin/aarch64-unknown-linux-ohos-clang"
export CXX="${OHOS_SDK}/native/llvm/bin/aarch64-unknown-linux-ohos-clang++"
export LD=${OHOS_SDK}/native/llvm/bin/ld.lld
export STRIP=${OHOS_SDK}/native/llvm/bin/llvm-strip
export RANLIB=${OHOS_SDK}/native/llvm/bin/llvm-ranlib
export OBJDUMP=${OHOS_SDK}/native/llvm/bin/llvm-objdump
export OBJCOPY=${OHOS_SDK}/native/llvm/bin/llvm-objcopy
export NM=${OHOS_SDK}/native/llvm/bin/llvm-nm
export AR=${OHOS_SDK}/native/llvm/bin/llvm-ar
export CFLAGS="-DOHOS_NDK -fPIC -D__MUSL__=1"
export CXXFLAGS="-DOHOS_NDK -fPIC -D__MUSL__=1"
export PREFIX="$ROOT_DIR/$BUILD_DIR"

echo build_ohos
# PKG_CONFIG_PATH="${LIBX264}/arm64-v8a/lib/pkgconfig" \

function build_ohos {
    ./configure \
    --arch=aarch64 \
    --cc=${CC} \
    --host-cc=${CC} \
    --host-ld=${CC} \
    --strip=${STRIP} \
    --prefix=$PREFIX \
    --target-os=linux \
    --enable-cross-compile \
    --extra-cflags="$CFLAGS" \
    --extra-ldflags="$LDFLAGS" \
    --enable-pic \
    --enable-static \
    --disable-shared \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-doc \
    --enable-avfilter \
    --enable-small \
    --disable-armv5te \
    --disable-armv6 \
    --disable-armv6t2 \
    --enable-vfp \
    --enable-neon \
    --disable-encoders \
    --disable-filters \
    --disable-vulkan \
    --enable-asm \
    --disable-debug \
    --enable-avdevice \
    --disable-encoders \
    --enable-encoder=mjpeg \
    --enable-encoder=png \
    --enable-encoder=aac \
    --disable-hwaccels \
    --disable-muxers \
    --enable-muxer=mov \
    --enable-muxer=mp4 \
    --enable-muxer=flv \
    --disable-decoders \
    --enable-decoder=mpeg4 \
    --enable-decoder=h264 \
    --enable-decoder=hevc \
    --enable-decoder=flv \
    --enable-decoder=aac \
    --enable-decoder=ac3 \
    --enable-decoder=mp3 \
    --enable-decoder=opus \
    --disable-demuxers \
    --enable-demuxer=hls \
    --enable-demuxer=mpegts \
    --enable-demuxer=mov \
    --enable-demuxer=flv \
    --enable-demuxer=live_flv \
    --enable-demuxer=rtsp \
    --enable-demuxer=rtmp \
    --enable-demuxer=mp3 \
    --enable-demuxer=matroska \
    --enable-demuxer=gif \
    --enable-demuxer=aac \
    --enable-protocol=rtmp \
    --disable-protocol=unix \
    --enable-openssl \
    --enable-version3 \
    --enable-libx264 \
    --enable-encoder=libx264 \
    --enable-encoder=libx264rgb \
    --sysroot="${OHOS_SDK}/native/sysroot" \
    --enable-gpl \
    --disable-x86asm \
    --extra-cflags="-I$OPENSSL/include -I$LIBX264/include -fPIC -fPIE -pie" \
    --extra-ldflags="-L$OPENSSL/arm64-v8a -L$LIBX264/arm64-v8a" \
    --extra-libs="-lssl -lcrypto -lx264"

    make clean
    make -j8
    make install
}

echo "build arm64-v8a -- begin"

build_ohos

echo "build arm64-v8a -- end"
