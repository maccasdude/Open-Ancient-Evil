#!/bin/bash
# Cross-compile Open AncientEvil for 64-bit Windows with mingw-w64 (on Linux).
#
#   tools/build-windows.sh [--no-ffmpeg]
#
# Needs: x86_64-w64-mingw32-g++-posix (Ubuntu: apt install mingw-w64), curl,
# tar, xz, make. SDL2 (the official mingw development package) and a small
# FFmpeg (movies and CD music: only the decoders the game needs, static,
# LGPL) are downloaded and built into build-win/deps the first time.
# The result is build-win/OpenAncientEvil/ with a self-contained ancientevil.exe.
set -euo pipefail

SDL_VER=2.30.9
FFMPEG_VER=6.1.1
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build-win
DEPS=$OUT/deps
HOST=x86_64-w64-mingw32
CXX=${CXX:-$HOST-g++-posix}
command -v "$CXX" >/dev/null || CXX=$HOST-g++
USE_FFMPEG=1
[ "${1:-}" = "--no-ffmpeg" ] && USE_FFMPEG=0
mkdir -p "$DEPS"

# --- SDL2 -------------------------------------------------------------------
SDL=$DEPS/SDL2-$SDL_VER/$HOST
if [ ! -d "$SDL" ]; then
    echo "== SDL2 $SDL_VER"
    curl -fsSL -o "$DEPS/sdl.tgz" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz"
    tar xzf "$DEPS/sdl.tgz" -C "$DEPS"
fi

# --- FFmpeg (minimal, static) -------------------------------------------------
FF=$DEPS/ffmpeg
if [ $USE_FFMPEG = 1 ] && [ ! -f "$FF/lib/libavcodec.a" ]; then
    echo "== FFmpeg $FFMPEG_VER (minimal)"
    SRC=$DEPS/ffmpeg-$FFMPEG_VER
    if [ ! -d "$SRC" ]; then
        if [ -n "${FFMPEG_TARBALL:-}" ]; then cp "$FFMPEG_TARBALL" "$DEPS/ff.tar.xz"
        else curl -fsSL -o "$DEPS/ff.tar.xz" "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VER.tar.xz"; fi
        tar xJf "$DEPS/ff.tar.xz" -C "$DEPS"
    fi
    (cd "$SRC" && ./configure --prefix="$FF" --enable-cross-compile --arch=x86_64 --target-os=mingw32 \
        --cross-prefix=$HOST- --disable-everything --disable-programs --disable-doc --disable-network \
        --disable-autodetect --disable-debug --disable-x86asm --enable-static --disable-shared \
        --disable-avdevice --disable-avfilter --disable-postproc \
        --enable-avformat --enable-avcodec --enable-swscale --enable-swresample --enable-protocol=file \
        --enable-demuxer=avi,wav,ogg,mp3,flac,matroska,mov,aiff \
        --enable-decoder=indeo5,indeo3,indeo4,cinepak,msvideo1,pcm_s16le,pcm_u8,pcm_s16be,adpcm_ms,adpcm_ima_wav,vorbis,opus,mp3,mp3float,flac,aac \
        --enable-parser=vorbis,mpegaudio,flac,opus,aac >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null)
fi

# --- the game -----------------------------------------------------------------
echo "== ancientevil.exe"
CXXFLAGS="-std=c++17 -O2 -fno-strict-aliasing -fwrapv -Wno-unused-result -I$SDL/include -I$SDL/include/SDL2 -Dmain=SDL_main"
LIBS="-L$SDL/lib -lmingw32 -lSDL2main -lSDL2"
# (SDL2 linked in statically: its system libraries)
SYSLIBS="-ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lbcrypt"
if [ $USE_FFMPEG = 1 ]; then
    CXXFLAGS="$CXXFLAGS -DHAVE_LIBAV -I$FF/include"
    LIBS="$LIBS -L$FF/lib -lavformat -lavcodec -lswscale -lswresample -lavutil"
fi
OBJ=$OUT/obj
mkdir -p "$OBJ"
objs=()
for f in "$ROOT"/src/engine/*.cpp "$ROOT"/src/platform/*.cpp "$ROOT"/src/game/*.cpp; do
    o=$OBJ/$(basename "$(dirname "$f")")_$(basename "${f%.cpp}").o
    objs+=("$o")
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find "$ROOT/src" -name '*.h' -newer "$o" -print -quit)" ]; then
        echo "   $(basename "$f")" && $CXX $CXXFLAGS -c "$f" -o "$o" &
        while [ "$(jobs -r | wc -l)" -ge "$(nproc)" ]; do sleep 0.1; done
    fi
done
wait
DIST=$OUT/OpenAncientEvil
mkdir -p "$DIST"
if [ -f "$ROOT/packaging/windows/ancientevil.rc" ]; then
    $HOST-windres "$ROOT/packaging/windows/ancientevil.rc" -O coff -o "$OBJ/res.o"
    objs+=("$OBJ/res.o")
fi
$CXX -o "$DIST/ancientevil.exe" "${objs[@]}" -mwindows -static -static-libgcc -static-libstdc++ $LIBS $SYSLIBS -lm
$HOST-strip "$DIST/ancientevil.exe"
cp "$ROOT/README.md" "$DIST/README.md"
[ -f "$ROOT/LICENSE" ] && cp "$ROOT/LICENSE" "$DIST/LICENSE.txt"
cp "$ROOT/packaging/windows/README-WINDOWS.txt" "$ROOT/packaging/windows/THIRD-PARTY.txt" "$DIST/"
echo "== done: $DIST"
