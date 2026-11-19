#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build/rootfs"
OUTPUT="$ROOT/Frameworks/CSwiptermAlpineRootfs.xcframework"
DEPLOYMENT_TARGET=16.0
{ read -r URL; read -r SHA256; } < "$ROOT/rootfs.ref"
ARCHIVE="$BUILD/$(basename "$URL")"

verify() {
    [[ -f "$ARCHIVE" ]] && [[ "$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)" == "$SHA256" ]]
}

mkdir -p "$BUILD"
if ! verify; then
    curl -fsSL --retry 3 -o "$ARCHIVE.download" "$URL"
    mv "$ARCHIVE.download" "$ARCHIVE"
    if ! verify; then
        echo "error: $URL does not match the SHA-256 in rootfs.ref" >&2
        exit 1
    fi
fi

build_platform() {
    local sdk="$1" target="$2"
    mkdir -p "$BUILD/$sdk"
    xcrun --sdk "$sdk" clang -target "$target" -isysroot "$(xcrun --sdk "$sdk" --show-sdk-path)" \
        -DROOTFS_ARCHIVE="\"$ARCHIVE\"" -c "$ROOT/Rootfs/rootfs.S" -o "$BUILD/$sdk/rootfs.o"
    rm -f "$BUILD/$sdk/libCSwiptermAlpineRootfs.a"
    libtool -static -o "$BUILD/$sdk/libCSwiptermAlpineRootfs.a" "$BUILD/$sdk/rootfs.o"
}

build_platform iphoneos "arm64-apple-ios$DEPLOYMENT_TARGET"
build_platform iphonesimulator "arm64-apple-ios$DEPLOYMENT_TARGET-simulator"

rm -rf "$OUTPUT"
xcodebuild -create-xcframework \
    -library "$BUILD/iphoneos/libCSwiptermAlpineRootfs.a" -headers "$ROOT/Rootfs/include" \
    -library "$BUILD/iphonesimulator/libCSwiptermAlpineRootfs.a" -headers "$ROOT/Rootfs/include" \
    -output "$OUTPUT" > /dev/null
