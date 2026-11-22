#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${OUT_DIR:-$SCRIPT_DIR/build}"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

if [[ "$(uname -s)" != "Linux" ]]; then
    exec orb -m "${ARCH_VM:-arch}" env OUT_DIR="$OUT_DIR" ${LINUX_WORK:+LINUX_WORK="$LINUX_WORK"} \
        bash "$SCRIPT_DIR/build_guest.sh" "$@"
fi

LINUX_VERSION="6.12"
LINUX_SHA256="b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb"
LINUX_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$LINUX_VERSION.tar.xz"
E2FSPROGS_VERSION="1.47.4"
E2FSPROGS_SHA256="fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b"
E2FSPROGS_URL="https://cdn.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v$E2FSPROGS_VERSION/e2fsprogs-$E2FSPROGS_VERSION.tar.xz"
E2FSPROGS_CONFIGURE=(
    --disable-nls --disable-fuse2fs --disable-debugfs --disable-imager --disable-resizer
    --disable-defrag --disable-uuidd --disable-e2initrd-helper --disable-tdb --disable-backtrace
    --disable-mmp --disable-bmap-stats --disable-rpath --disable-elf-shlibs
    --enable-libuuid --enable-libblkid --without-libarchive
    --without-udev-rules-dir --without-crond-dir --without-systemd-unit-dir
    "CFLAGS=-Os -ffunction-sections -fdata-sections"
    "LDFLAGS_STATIC=-static -Wl,--gc-sections"
)

LINUX_WORK="${LINUX_WORK:-${XDG_CACHE_HOME:-$HOME/.cache}/swipterm-guest}"
JOBS="$(nproc)"
mkdir -p "$LINUX_WORK"

fetch() {
    local url="$1" sha256="$2"
    local file
    file="$OUT_DIR/$(basename "$url")"
    if [[ ! -f "$file" ]] || ! echo "$sha256  $file" | sha256sum -c --status; then
        echo "[build_guest] Downloading $url" >&2
        curl -fsSL --retry 3 "$url" -o "$file.tmp"
        if ! echo "$sha256  $file.tmp" | sha256sum -c --status; then
            rm -f "$file.tmp"
            echo "error: checksum mismatch for $url" >&2
            exit 1
        fi
        mv "$file.tmp" "$file"
    fi
    printf '%s\n' "$file"
}

extract() {
    local tarball="$1" dest="$2" stamp="$3"
    shift 3
    if [[ "$(cat "$dest/.swipterm-stamp" 2>/dev/null || true)" == "$stamp" ]]; then
        return
    fi
    echo "[build_guest] Extracting $(basename "$tarball")"
    rm -rf "$dest"
    mkdir -p "$dest"
    tar -xf "$tarball" -C "$dest" --strip-components=1
    local patch
    for patch in "$@"; do
        echo "  -> $(basename "$patch")"
        patch -d "$dest" -p1 < "$patch"
    done
    printf '%s\n' "$stamp" > "$dest/.swipterm-stamp"
}

build_e2fsprogs() {
    local out="$OUT_DIR/e2fsprogs"
    local stamp
    stamp="$(printf '%s\n' "$E2FSPROGS_SHA256" "${E2FSPROGS_CONFIGURE[@]}" | sha256sum | cut -d' ' -f1)"
    if [[ "$(cat "$out/.swipterm-stamp" 2>/dev/null || true)" == "$stamp" ]]; then
        return
    fi
    local tarball src="$LINUX_WORK/e2fsprogs-$E2FSPROGS_VERSION"
    tarball="$(fetch "$E2FSPROGS_URL" "$E2FSPROGS_SHA256")"
    extract "$tarball" "$src" "$E2FSPROGS_SHA256"
    echo "[build_guest] Building static e2fsprogs $E2FSPROGS_VERSION"
    rm -rf "$src/build"
    mkdir -p "$src/build"
    (
        cd "$src/build"
        ../configure -q "${E2FSPROGS_CONFIGURE[@]}"
        make -s -j"$JOBS" libs
        make -s -C misc mke2fs.static tune2fs.static
        make -s -C e2fsck e2fsck.static
    )
    rm -rf "$out"
    mkdir -p "$out"
    install -m 0755 -s "$src/build/misc/mke2fs.static" "$out/mke2fs"
    install -m 0755 -s "$src/build/misc/tune2fs.static" "$out/tune2fs"
    install -m 0755 -s "$src/build/e2fsck/e2fsck.static" "$out/e2fsck"
    printf '%s\n' "$stamp" > "$out/.swipterm-stamp"
}

build_initramfs() {
    build_e2fsprogs
    echo "[build_guest] Building swiptermd + cmd_stub + modprobe + initramfs.cpio"
    make -C "$SCRIPT_DIR/swiptermd" clean all E2FSPROGS_BIN="$OUT_DIR/e2fsprogs"
    local artifact
    for artifact in initramfs.cpio swiptermd cmd_stub modprobe; do
        cp "$SCRIPT_DIR/swiptermd/$artifact" "$OUT_DIR/$artifact"
    done
    echo "[build_guest] Built $OUT_DIR/initramfs.cpio ($(stat -c%s "$OUT_DIR/initramfs.cpio") bytes)"
}

verify_kernel_config() {
    local config="$1"
    shift
    local failed=0 line symbol
    while IFS= read -r line; do
        case "$line" in
        "# CONFIG_"*" is not set")
            symbol="${line#\# }"
            symbol="${symbol%% *}"
            if grep -q "^$symbol=" "$config"; then
                echo "error: requested '$line', resolved '$(grep "^$symbol=" "$config")'" >&2
                failed=1
            fi
            ;;
        CONFIG_*)
            if ! grep -qxF "$line" "$config"; then
                symbol="${line%%=*}"
                echo "error: requested '$line', resolved '$(grep -E "^(# )?$symbol[= ]" "$config" || echo absent)'" >&2
                failed=1
            fi
            ;;
        esac
    done < <(cat "$@")
    return "$failed"
}

build_kernel() {
    local tarball src="$LINUX_WORK/linux-$LINUX_VERSION" kbuild="$LINUX_WORK/kbuild-$LINUX_VERSION"
    local patches=()
    local patch
    for patch in "$SCRIPT_DIR"/patches/*.patch; do
        [[ -f "$patch" ]] && patches+=("$patch")
    done
    local stamp="$LINUX_SHA256"
    if ((${#patches[@]} > 0)); then
        stamp="$(cat <(echo "$LINUX_SHA256") "${patches[@]}" | sha256sum | cut -d' ' -f1)"
    fi
    tarball="$(fetch "$LINUX_URL" "$LINUX_SHA256")"
    extract "$tarball" "$src" "$stamp" "${patches[@]}"

    local fragments=("$SCRIPT_DIR"/kernel/*.config)
    local kmake=(make -C "$src" O="$kbuild" ARCH=arm64
        KBUILD_BUILD_USER=swipterm KBUILD_BUILD_HOST=swipterm KBUILD_BUILD_VERSION=1)
    echo "[build_guest] Configuring Linux $LINUX_VERSION: virtconfig + ${fragments[*]##*/}"
    mkdir -p "$kbuild"
    "${kmake[@]}" -s virtconfig > /dev/null
    "$src/scripts/kconfig/merge_config.sh" -Q -m -O "$kbuild" "$kbuild/.config" "${fragments[@]}" > /dev/null
    "${kmake[@]}" -s olddefconfig
    "${kmake[@]}" -s mod2noconfig
    if ! verify_kernel_config "$kbuild/.config" "${fragments[@]}"; then
        echo "error: kernel config fragments did not resolve as requested" >&2
        exit 1
    fi

    echo "[build_guest] Building Linux $LINUX_VERSION Image"
    "${kmake[@]}" -j"$JOBS" Image
    cp "$kbuild/arch/arm64/boot/Image" "$OUT_DIR/Image"
    cp "$kbuild/.config" "$OUT_DIR/kernel.config"
    echo "[build_guest] Built $OUT_DIR/Image ($(stat -c%s "$OUT_DIR/Image") bytes)"
}

case "${1:-all}" in
all)
    build_initramfs
    build_kernel
    ;;
initramfs)
    build_initramfs
    ;;
kernel)
    build_kernel
    ;;
*)
    echo "usage: $0 [all|initramfs|kernel]" >&2
    exit 64
    ;;
esac
