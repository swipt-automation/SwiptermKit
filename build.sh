#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
FRAMEWORKS="$ROOT/Frameworks"

MACOS_TARGET="arm64-apple-macos14.0"
IOS_TARGET="arm64-apple-ios17.0"
IOS_SIM_TARGET="arm64-apple-ios17.0-simulator"

SLICES=(
    "macosx:$MACOS_TARGET"
    "iphoneos:$IOS_TARGET"
    "iphonesimulator:$IOS_SIM_TARGET"
)

build_vmm_slice() {
    local sdk="$1" target="$2"
    local dir="$BUILD/vmm/$sdk"
    local sysroot clang
    sysroot="$(xcrun --sdk "$sdk" --show-sdk-path)"
    clang="$(xcrun --sdk "$sdk" --find clang)"

    rm -rf "$dir/objects"
    mkdir -p "$dir/objects"

    local cflags=(
        -target "$target" -isysroot "$sysroot"
        -O3 -g -fno-common
        -I"$ROOT/VMM"
        -I"$ROOT/VMM/include/CSwiptermKit"
        -I"$ROOT/Guest/swiptermd"
    )
    local stats_define="-UCPU_STATS"
    if [[ "${SWIPTERM_CPU_STATS:-0}" == "1" ]]; then
        stats_define="-DCPU_STATS"
    fi
    cflags+=("$stats_define")
    if [[ -d "$ROOT/VMM/deps/libslirp/src" ]]; then
        cflags+=(-I"$ROOT/VMM/deps/libslirp/src")
    fi
    if [[ -d "$ROOT/VMM/deps/libslirp/glib_compat" ]]; then
        cflags+=(-I"$ROOT/VMM/deps/libslirp/glib_compat")
    fi

    local objects=()
    local idx=0
    while IFS= read -r -d '' source; do
        local rel="${source#"$ROOT/VMM/"}"
        local obj="$dir/objects/${rel//\//_}.o"
        local std_flag="-std=c23"
        local warn_flags=(-Wall -Wextra -Werror)
        if [[ "$rel" == deps/* ]]; then
            std_flag="-std=gnu11"
            warn_flags=(-w)
        fi
        "$clang" "$std_flag" "${cflags[@]}" "${warn_flags[@]}" -c "$source" -o "$obj"
        objects+=("$obj")
        idx=$((idx + 1))
    done < <(find "$ROOT/VMM" -type f -name '*.c' -print0 | sort -z)

    local fast_asm_obj="$dir/objects/cpu_cpu_fast_arm64.S.o"
    "$clang" -target "$target" -isysroot "$sysroot" -I"$ROOT/VMM" "$stats_define" -c "$ROOT/VMM/cpu/cpu_fast_arm64.S" -o "$fast_asm_obj"
    objects+=("$fast_asm_obj")

    local embed_kernel="${SWIPTERM_EMBED_KERNEL:-1}"
    local asm_flags=(
        -target "$target" -isysroot "$sysroot"
        -DSWIPTERM_GUEST_INITRAMFS="\"$BUILD/guest/initramfs.cpio\""
    )
    if [[ "$embed_kernel" == "1" ]]; then
        if [[ ! -s "$BUILD/guest/Image" ]]; then
            echo "error: missing guest kernel image: $BUILD/guest/Image" >&2
            exit 1
        fi
        asm_flags+=(-DSWIPTERM_GUEST_KERNEL_IMAGE="\"$BUILD/guest/Image\"")
    fi
    if [[ ! -s "$BUILD/guest/initramfs.cpio" ]]; then
        echo "error: missing guest initramfs: $BUILD/guest/initramfs.cpio" >&2
        exit 1
    fi
    local blob_obj="$dir/objects/host_guest_blob.o"
    "$clang" "${asm_flags[@]}" -c "$ROOT/VMM/host/guest_blob.S" -o "$blob_obj"
    objects+=("$blob_obj")

    rm -f "$dir/libCSwiptermKit.a"
    libtool -static -no_warning_for_no_symbols -o "$dir/libCSwiptermKit.a" "${objects[@]}"
}

hash_files() {
    shasum -a 256 "$@" | shasum -a 256 | awk '{print $1}'
}

prepare_guest_artifacts() {
    mkdir -p "$BUILD/guest"

    if [[ -n "${SWIPTERM_PREBUILT_KERNEL:-}" && -f "$SWIPTERM_PREBUILT_KERNEL" ]]; then
        local pre_hash pre_stamp="$BUILD/guest/Image.prebuilt.sha256"
        pre_hash="$(hash_files "$SWIPTERM_PREBUILT_KERNEL")"
        if [[ ! -s "$BUILD/guest/Image" ]] || [[ "$(cat "$pre_stamp" 2>/dev/null || true)" != "$pre_hash" ]]; then
            cp "$SWIPTERM_PREBUILT_KERNEL" "$BUILD/guest/Image"
            printf '%s\n' "$pre_hash" > "$pre_stamp"
        fi
    else
        local kernel_hash kernel_stamp="$BUILD/guest/Image.sha256"
        local kernel_inputs=("$ROOT"/Guest/kernel/*.config "$ROOT/Guest/build_guest.sh")
        local patch
        for patch in "$ROOT"/Guest/patches/*.patch; do
            [[ -f "$patch" ]] && kernel_inputs+=("$patch")
        done
        kernel_hash="$(hash_files "${kernel_inputs[@]}")"
        if [[ ! -s "$BUILD/guest/Image" ]] || [[ "$(cat "$kernel_stamp" 2>/dev/null || true)" != "$kernel_hash" ]]; then
            OUT_DIR="$BUILD/guest" "$ROOT/Guest/build_guest.sh" kernel
            printf '%s\n' "$kernel_hash" > "$kernel_stamp"
        fi
    fi

    if [[ -n "${SWIPTERM_PREBUILT_INITRAMFS:-}" && -f "$SWIPTERM_PREBUILT_INITRAMFS" ]]; then
        local pre_hash pre_stamp="$BUILD/guest/initramfs.cpio.prebuilt.sha256"
        pre_hash="$(hash_files "$SWIPTERM_PREBUILT_INITRAMFS")"
        if [[ ! -s "$BUILD/guest/initramfs.cpio" ]] || [[ "$(cat "$pre_stamp" 2>/dev/null || true)" != "$pre_hash" ]]; then
            cp "$SWIPTERM_PREBUILT_INITRAMFS" "$BUILD/guest/initramfs.cpio"
            printf '%s\n' "$pre_hash" > "$pre_stamp"
        fi
    else
        local initramfs_hash initramfs_stamp="$BUILD/guest/initramfs.cpio.sha256"
        initramfs_hash="$(hash_files "$ROOT"/Guest/swiptermd/*.[ch] "$ROOT/Guest/swiptermd/Makefile" "$ROOT/Guest/build_guest.sh")"
        if [[ ! -s "$BUILD/guest/initramfs.cpio" ]] || [[ "$(cat "$initramfs_stamp" 2>/dev/null || true)" != "$initramfs_hash" ]]; then
            OUT_DIR="$BUILD/guest" "$ROOT/Guest/build_guest.sh" initramfs
            printf '%s\n' "$initramfs_hash" > "$initramfs_stamp"
        fi
    fi
}

build_kernel() {
    prepare_guest_artifacts
    mkdir -p "$BUILD/vmm" "$FRAMEWORKS"
    local xc_args=()
    for spec in "${SLICES[@]}"; do
        local sdk="${spec%%:*}" target="${spec#*:}"
        build_vmm_slice "$sdk" "$target"
        xc_args+=(-library "$BUILD/vmm/$sdk/libCSwiptermKit.a" -headers "$ROOT/VMM/include")
    done

    local output="$FRAMEWORKS/CSwiptermKit.xcframework"
    rm -rf "$output"
    xcodebuild -create-xcframework "${xc_args[@]}" -output "$output" > /dev/null
}

ensure_erofs_tool() {
    local tool="$BUILD/host-tools/tar2erofs"
    local stamp="$BUILD/host-tools/tar2erofs.sha256"
    local hash
    hash="$(hash_files \
        "$ROOT/VMM/host/erofs_archive.c" \
        "$ROOT/VMM/host/ext4_upper.c" \
        "$ROOT/VMM/host/host_internal.h" \
        "$ROOT/VMM/include/CSwiptermKit/swipterm.h" \
        "$ROOT/Guest/swiptermd/protocol.h" \
        "$ROOT/Guest/swiptermd/wire_format.h")"
    if [[ -x "$tool" ]] && [[ "$(cat "$stamp" 2>/dev/null || true)" == "$hash" ]]; then
        return
    fi
    mkdir -p "$BUILD/host-tools"
    cat > "$BUILD/host-tools/tar2erofs.c" <<'EOF'
#include "swipterm.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <archive> <root_dir>\n", argv[0]);
        return 2;
    }
    char *err = NULL;
    if (!swipterm_image_import_file(argv[1], argv[2], NULL, NULL, &err)) {
        fprintf(stderr, "error: %s\n", err ? err : "import failed");
        free(err);
        return 1;
    }
    return 0;
}
EOF
    local sysroot clang
    sysroot="$(xcrun --sdk macosx --show-sdk-path)"
    clang="$(xcrun --sdk macosx --find clang)"
    "$clang" -target "$MACOS_TARGET" -isysroot "$sysroot" -std=c23 -O3 \
        -I"$ROOT/VMM" -I"$ROOT/VMM/include/CSwiptermKit" -I"$ROOT/Guest/swiptermd" \
        "$BUILD/host-tools/tar2erofs.c" \
        "$ROOT/VMM/host/erofs_archive.c" \
        "$ROOT/VMM/host/ext4_upper.c" \
        -larchive -llzma -o "$tool"
    printf '%s\n' "$hash" > "$stamp"
}

prepare_rootfs_erofs() {
    local name="$1" erofs_path="$2" tarball_glob="$3" prebuilt_path="${4:-}"
    local stamp="$erofs_path.sha256"
    if [[ -n "$prebuilt_path" && -f "$prebuilt_path" ]]; then
        local hash
        hash="$(hash_files "$prebuilt_path")"
        if [[ ! -s "$erofs_path" ]] || [[ "$(cat "$stamp" 2>/dev/null || true)" != "$hash" ]]; then
            mkdir -p "$(dirname "$erofs_path")"
            cp "$prebuilt_path" "$erofs_path"
            printf '%s\n' "$hash" > "$stamp"
        fi
        return
    fi
    mkdir -p "$(dirname "$erofs_path")"
    local tarball=""
    for candidate in $tarball_glob; do
        if [[ -f "$candidate" ]]; then
            tarball="$candidate"
            break
        fi
    done
    if [[ -z "$tarball" && ! -s "$erofs_path" ]]; then
        if [[ "$name" == "alpine" ]]; then
            local alpine_ver="${ALPINE_VERSION:-3.24.4}"
            local alpine_sha256="${ALPINE_SHA256:-}"
            if [[ "$alpine_ver" == "3.24.4" && -z "$alpine_sha256" ]]; then
                alpine_sha256="4b14c18f7e90a063b61462ea1462a5732c154bf6adad594974fe6487cd98a8de"
            fi
            local alpine_branch="v${alpine_ver%.*}"
            tarball="$BUILD/rootfs/alpine-minirootfs-${alpine_ver}-aarch64.tar.gz"
            curl -fsSL --retry 3 "https://dl-cdn.alpinelinux.org/alpine/${alpine_branch}/releases/aarch64/alpine-minirootfs-${alpine_ver}-aarch64.tar.gz" \
                -o "$tarball.tmp"
            if [[ -n "$alpine_sha256" && "$(shasum -a 256 "$tarball.tmp" | awk '{print $1}')" != "$alpine_sha256" ]]; then
                rm -f "$tarball.tmp"
                echo "error: Alpine minirootfs checksum mismatch for $alpine_ver" >&2
                exit 1
            fi
            mv "$tarball.tmp" "$tarball"
        elif [[ "$name" == "debian" && -x "$ROOT/Rootfs/debian/build_debian.sh" ]]; then
            tarball="$BUILD/rootfs/debian-trixie-arm64.tar"
            "$ROOT/Rootfs/debian/build_debian.sh" "$tarball"
        fi
    fi
    if [[ -n "$tarball" ]]; then
        ensure_erofs_tool
        local hash
        hash="$(hash_files "$tarball" "$BUILD/host-tools/tar2erofs")"
        if [[ -s "$erofs_path" ]] && [[ "$(cat "$stamp" 2>/dev/null || true)" == "$hash" ]]; then
            return
        fi
        local stage="$BUILD/rootfs/.stage-$name"
        rm -rf "$stage"
        "$BUILD/host-tools/tar2erofs" "$tarball" "$stage"
        mv "$stage/lower.erofs" "$erofs_path"
        rm -rf "$stage"
        printf '%s\n' "$hash" > "$stamp"
    fi
}

build_rootfs_framework() {
    local name="$1" asm_src="$2" headers_dir="$3" erofs_path="$4" output="$5"
    if [[ ! -s "$erofs_path" ]]; then
        echo "error: missing or empty rootfs image: $erofs_path" >&2
        exit 1
    fi
    local xc_args=()
    for spec in "${SLICES[@]}"; do
        local sdk="${spec%%:*}" target="${spec#*:}"
        local dir="$BUILD/rootfs/$name/$sdk"
        local sysroot clang
        sysroot="$(xcrun --sdk "$sdk" --show-sdk-path)"
        clang="$(xcrun --sdk "$sdk" --find clang)"
        mkdir -p "$dir"
        local asm_flags=(-target "$target" -isysroot "$sysroot" -DROOTFS_ARCHIVE="\"$erofs_path\"")
        "$clang" "${asm_flags[@]}" -c "$asm_src" -o "$dir/rootfs.o"
        rm -f "$dir/lib${name}.a"
        libtool -static -no_warning_for_no_symbols -o "$dir/lib${name}.a" "$dir/rootfs.o"
        xc_args+=(-library "$dir/lib${name}.a" -headers "$headers_dir")
    done

    rm -rf "$output"
    xcodebuild -create-xcframework "${xc_args[@]}" -output "$output" > /dev/null
}

build_rootfs() {
    mkdir -p "$BUILD/rootfs" "$FRAMEWORKS"
    prepare_rootfs_erofs "alpine" "$BUILD/rootfs/alpine.erofs" "$BUILD/rootfs/alpine-minirootfs-*.tar.gz" "${SWIPTERM_PREBUILT_ALPINE_EROFS:-}"
    prepare_rootfs_erofs "debian" "$BUILD/rootfs/debian.erofs" "$BUILD/rootfs/debian-*.tar*" "${SWIPTERM_PREBUILT_DEBIAN_EROFS:-}"

    build_rootfs_framework \
        "CSwiptermAlpineRootfs" \
        "$ROOT/Rootfs/rootfs.S" \
        "$ROOT/Rootfs/include" \
        "$BUILD/rootfs/alpine.erofs" \
        "$FRAMEWORKS/CSwiptermAlpineRootfs.xcframework"

    build_rootfs_framework \
        "CSwiptermDebianRootfs" \
        "$ROOT/Rootfs/debian_rootfs.S" \
        "$ROOT/Rootfs/debian/include" \
        "$BUILD/rootfs/debian.erofs" \
        "$FRAMEWORKS/CSwiptermDebianRootfs.xcframework"
}

case "${1:-all}" in
    all)
        build_kernel
        build_rootfs
        ;;
    kernel)
        build_kernel
        ;;
    rootfs)
        build_rootfs
        ;;
    *)
        echo "usage: $0 [all|kernel|rootfs]" >&2
        exit 64
        ;;
esac
