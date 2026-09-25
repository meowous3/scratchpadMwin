#!/usr/bin/env bash
# The ffmpeg melo ships: one static 1.6 MB binary for the silence scan
# (sidecar/src/silence.ts). Reads YouTube and local-import formats from a file
# or plain HTTP and writes raw float samples. No TLS: an https source is read
# through the sidecar's local relay.
#
# LGPL-2.1-or-later: no --enable-gpl, no nonfree. The pinned tarball and this
# configure line are the corresponding source; release-sources.sh ships both.
#
#   scripts/build-ffmpeg.sh        -> vendor/ffmpeg/bin/ffmpeg
set -euo pipefail

# MSYS2 on Windows: the same configure line with the MSVC toolchain, and an
# .exe that MSVC already built without symbols. configure refuses an MSYS host
# unless the target OS is named.
case "$(uname -s)" in MINGW*|MSYS*) WIN=1 ;; *) WIN=0 ;; esac
EXE=""; [ "$WIN" = 1 ] && EXE=".exe"
TOOLCHAIN=(); [ "$WIN" = 1 ] && TOOLCHAIN=(--target-os=win64 --arch=x86_64 --toolchain=msvc)

VER="7.1.1"
# ffmpeg.org's tarball; its release-key signature was checked when pinned
SHA256="733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/vendor/ffmpeg"
if [ -x "$OUT/bin/ffmpeg$EXE" ]; then
    echo "ffmpeg already built at vendor/ffmpeg — delete it to rebuild."
    exit 0
fi

WORK="$ROOT/vendor/ffmpeg-src"
mkdir -p "$WORK"
cd "$WORK"
if [ ! -f "ffmpeg-$VER.tar.xz" ]; then
    curl -fsSLo "ffmpeg-$VER.tar.xz.part" "https://ffmpeg.org/releases/ffmpeg-$VER.tar.xz"
    mv "ffmpeg-$VER.tar.xz.part" "ffmpeg-$VER.tar.xz"
fi
echo "$SHA256  ffmpeg-$VER.tar.xz" | sha256sum -c --quiet \
    || { echo "FATAL: ffmpeg-$VER.tar.xz does not match the pinned SHA-256" >&2; exit 1; }
rm -rf "ffmpeg-$VER"
tar -xJf "ffmpeg-$VER.tar.xz"
cd "ffmpeg-$VER"

./configure --prefix="$OUT" \
    --disable-everything --disable-autodetect --disable-doc --disable-debug \
    --disable-ffplay --disable-ffprobe --disable-avdevice --disable-swscale \
    --disable-postproc --disable-x86asm \
    --enable-small --enable-static --disable-shared \
    --enable-protocol=file,pipe,http,tcp \
    --enable-demuxer=matroska,mov,ogg,mp3,flac,wav,aac \
    --enable-decoder=opus,vorbis,aac,mp3,mp3float,flac,pcm_s16le,pcm_s24le,pcm_f32le \
    --enable-parser=opus,vorbis,aac,mpegaudio,flac \
    --enable-encoder=pcm_f32le --enable-muxer=pcm_f32le \
    --enable-filter=aresample,aformat,anull,atrim --enable-swresample \
    ${TOOLCHAIN[@]+"${TOOLCHAIN[@]}"}
make -j"$(nproc)"
mkdir -p "$OUT/bin"
if [ "$WIN" = 1 ]; then
    cp ffmpeg.exe "$OUT/bin/ffmpeg.exe"
else
    strip -o "$OUT/bin/ffmpeg" ffmpeg
fi
cp COPYING.LGPLv2.1 "$OUT/"   # the licence text ships with the binary
echo "built vendor/ffmpeg/bin/ffmpeg$EXE ($(du -h "$OUT/bin/ffmpeg$EXE" | cut -f1))"
