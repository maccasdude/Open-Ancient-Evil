#!/bin/bash
# Make the release packages of Open AncientEvil in dist/:
#
#   OpenAncientEvil-<ver>-src.tar.gz          source (also used by the PKGBUILD)
#   OpenAncientEvil-<ver>-linux-x64.tar.gz    Linux binary + source
#   OpenAncientEvil-<ver>-arch.tar.gz         PKGBUILD + source + prebuilt binary
#   OpenAncientEvil-<ver>-windows-x64.zip     ancientevil.exe (needs mingw-w64)
#
#   tools/package.sh [--no-windows] [--no-linux]
#
# The version comes from the VERSION file (or $AE_VERSION).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VER=${AE_VERSION:-$(cat "$ROOT/VERSION")}
DIST=$ROOT/dist
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
DO_WIN=1
DO_LINUX=1
for a in "$@"; do
    case $a in
    --no-windows) DO_WIN=0 ;;
    --no-linux) DO_LINUX=0 ;;
    esac
done
mkdir -p "$DIST"
SRCFILES=(src packaging re tools Makefile README.md CHANGELOG.md LICENSE VERSION)

# --- source tarball (top folder ancientevil-<ver>, as the PKGBUILD expects) ---
echo "== source"
S=$TMP/ancientevil-$VER
mkdir -p "$S"
(cd "$ROOT" && cp -r "${SRCFILES[@]}" "$S/")
rm -rf "$S/build" "$S/build-win"
SRC_TGZ=$DIST/OpenAncientEvil-$VER-src.tar.gz
tar czf "$SRC_TGZ" -C "$TMP" "ancientevil-$VER"

if [ $DO_LINUX = 1 ]; then
    # --- Linux (with FFmpeg when its development files are installed) ---
    echo "== linux"
    L=$TMP/linux
    cp -r "$S" "$L"
    make -C "$L" -j"$(nproc)" >/dev/null
    strip "$L/ancientevil"
    N=OpenAncientEvil-$VER-linux-x64
    mkdir -p "$TMP/$N"
    (cd "$L" && cp -r ancientevil "${SRCFILES[@]}" "$TMP/$N/")
    rm -rf "$TMP/$N/build"
    tar czf "$DIST/$N.tar.gz" -C "$TMP" "$N"

    # --- Arch / CachyOS ---
    echo "== arch"
    P=$TMP/pre
    cp -r "$S" "$P"
    make -C "$P" -j"$(nproc)" NO_FFMPEG=1 >/dev/null   # (prebuilt: needs only sdl2)
    strip "$P/ancientevil"
    N=OpenAncientEvil-$VER-arch
    A=$TMP/$N
    mkdir -p "$A/arch" "$A/prebuilt"
    cp "$SRC_TGZ" "$A/arch/ancientevil-$VER.tar.gz"
    SUM=$(sha256sum "$A/arch/ancientevil-$VER.tar.gz" | cut -d' ' -f1)
    sed "s/^pkgver=.*/pkgver=$VER/; s/^sha256sums=.*/sha256sums=('$SUM')/" \
        "$ROOT/packaging/arch/PKGBUILD" > "$A/arch/PKGBUILD"
    cp "$ROOT/packaging/arch/ancientevil.install" "$A/arch/"
    cp "$ROOT/packaging/arch/README-ARCH.txt" "$A/"
    cp "$P/ancientevil" "$A/prebuilt/"
    tar czf "$DIST/$N.tar.gz" -C "$TMP" "$N"
fi

if [ $DO_WIN = 1 ]; then
    if command -v x86_64-w64-mingw32-g++ >/dev/null || command -v x86_64-w64-mingw32-g++-posix >/dev/null; then
        echo "== windows"
        "$ROOT/tools/build-windows.sh" >/dev/null
        N=OpenAncientEvil-$VER-windows-x64
        rm -rf "$TMP/$N"
        cp -r "$ROOT/build-win/OpenAncientEvil" "$TMP/$N"
        cp "$ROOT/CHANGELOG.md" "$TMP/$N/"
        (cd "$TMP" && rm -f "$DIST/$N.zip" && zip -qr "$DIST/$N.zip" "$N")
    else
        echo "== windows: skipped (no mingw-w64)"
    fi
fi
ls -la "$DIST"
