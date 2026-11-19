#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
ISH="$BUILD/ish"
ISH_REF="$(tr -d '[:space:]' < "$ROOT/ish.ref")"
OUTPUT="$ROOT/Frameworks/CSwiptermKit.xcframework"
DEPLOYMENT_TARGET=16.0
MESON_OPTIONS=(
    --buildtype=debugoptimized
    -Db_ndebug=false
    -Dguest_arch=arm64
    -Dlog_handler=nslog
    -Dwarning_level=0
)

export PATH="/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:$PATH"

checkout_ish() {
    if [[ "$(git -C "$ISH" rev-parse HEAD 2>/dev/null)" != "$ISH_REF" ]]; then
        rm -rf "$ISH" "$BUILD/iphoneos" "$BUILD/iphonesimulator"
        git init -q "$ISH"
        git -C "$ISH" fetch -q --depth 1 https://github.com/OpenMinis/ish-arm64 "$ISH_REF"
        git -C "$ISH" checkout -q FETCH_HEAD
        git -C "$ISH" submodule update -q --init --depth 1 deps/libarchive
    fi

    local patches=("$ROOT"/Patches/*.patch)
    local stamp="$ISH/.git/swipterm-patches"
    local digest
    digest="$(cat "${patches[@]}" | shasum -a 256 | cut -d' ' -f1)"
    if [[ "$(cat "$stamp" 2>/dev/null)" != "$digest" ]]; then
        git -C "$ISH" reset -q --hard
        git -C "$ISH" clean -qfd
        git -C "$ISH" apply "${patches[@]}"
        echo "$digest" > "$stamp"
    fi
}

generate_overlay() {
    local bundle="$ISH/app/RootfsPatch.bundle"
    local manifest="$bundle/manifest.plist"
    local count=0
    {
        echo '#include "overlay.h"'
        echo "const int swipterm_overlay_version = $(/usr/libexec/PlistBuddy -c 'Print :version' "$manifest");"
        while source=$(/usr/libexec/PlistBuddy -c "Print :files:$count:src" "$manifest" 2>/dev/null); do
            echo "static const unsigned char file_$count[] = {"
            xxd -i < "$bundle/$source"
            echo "};"
            count=$((count + 1))
        done
        echo "const struct swipterm_overlay_file swipterm_overlay_files[] = {"
        for ((index = 0; index < count; index++)); do
            echo "    {\"$(/usr/libexec/PlistBuddy -c "Print :files:$index:dst" "$manifest")\", file_$index, sizeof(file_$index)},"
        done
        echo "};"
        echo "const size_t swipterm_overlay_file_count = $count;"
    } > "$BUILD/overlay.c"
}

build_platform() {
    local sdk="$1" target="$2"
    local dir="$BUILD/$sdk"
    local sysroot
    sysroot="$(xcrun --sdk "$sdk" --show-sdk-path)"
    local clang
    clang="$(xcrun --sdk "$sdk" --find clang)"
    mkdir -p "$dir"

    cat > "$dir/cross.ini" <<EOF
[binaries]
c = '$clang'
ar = '$(xcrun --sdk "$sdk" --find ar)'
strip = '$(xcrun --sdk "$sdk" --find strip)'

[host_machine]
system = 'darwin'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[built-in options]
c_args = ['-target', '$target', '-isysroot', '$sysroot']
c_link_args = ['-target', '$target', '-isysroot', '$sysroot']

[properties]
needs_exe_wrapper = true
EOF

    if [[ -f "$dir/meson/build.ninja" ]]; then
        meson configure "$dir/meson" "${MESON_OPTIONS[@]}"
    else
        CC_FOR_BUILD="env -u SDKROOT xcrun --sdk macosx clang" \
            meson setup "$dir/meson" "$ISH" --cross-file "$dir/cross.ini" "${MESON_OPTIONS[@]}"
    fi
    ninja -C "$dir/meson" --quiet libish.a libish_emu.a libfakefs.a

    local cflags=(
        -target "$target" -isysroot "$sysroot"
        -std=gnu11 -O2 -g -fno-common
        -DGUEST_ARM64=1
        -I"$ISH" -I"$dir/meson" -I"$ISH/deps/libarchive/libarchive" -I"$ROOT/Kernel" -I"$ROOT/Kernel/include/CSwiptermKit"
    )
    local objects=()
    rm -rf "$dir/objects"
    mkdir -p "$dir/objects"
    for source in "$ROOT"/Kernel/*.c; do
        objects+=("$dir/objects/$(basename "${source%.c}").o")
        "$clang" "${cflags[@]}" -Wall -Werror -Wno-unused-function -c "$source" -o "${objects[${#objects[@]} - 1]}"
    done
    for source in "$BUILD/overlay.c" "$ISH/tools/fakefs.c"; do
        objects+=("$dir/objects/$(basename "${source%.c}").o")
        "$clang" "${cflags[@]}" -c "$source" -o "${objects[${#objects[@]} - 1]}"
    done

    libtool -static -no_warning_for_no_symbols -o "$dir/libCSwiptermKit.a" \
        "$dir/meson/libish.a" "$dir/meson/libish_emu.a" "$dir/meson/libfakefs.a" "${objects[@]}"
}

build_kernel() {
    mkdir -p "$BUILD"
    checkout_ish
    generate_overlay
    build_platform iphoneos "arm64-apple-ios$DEPLOYMENT_TARGET"
    build_platform iphonesimulator "arm64-apple-ios$DEPLOYMENT_TARGET-simulator"

    rm -rf "$OUTPUT"
    xcodebuild -create-xcframework \
        -library "$BUILD/iphoneos/libCSwiptermKit.a" -headers "$ROOT/Kernel/include" \
        -library "$BUILD/iphonesimulator/libCSwiptermKit.a" -headers "$ROOT/Kernel/include" \
        -output "$OUTPUT" > /dev/null
}

case "${1:-all}" in
    all)
        build_kernel
        "$ROOT/rootfs.sh"
        ;;
    kernel)
        build_kernel
        ;;
    rootfs)
        "$ROOT/rootfs.sh"
        ;;
    *)
        echo "usage: $0 [all|kernel|rootfs]" >&2
        exit 64
        ;;
esac
