#!/usr/bin/env bash
set -euo pipefail

OUT_TAR="${1:?usage: $0 <output.tar>}"
SUITE="${DEBIAN_SUITE:-trixie}"
MIRROR="${DEBIAN_MIRROR:-http://deb.debian.org/debian}"
ARCH_VM="${ARCH_VM:-arch}"

PACKAGES="apt,bash,bubblewrap,ca-certificates,coreutils,curl,dash,findutils,git,grep,gzip,iproute2,iputils-ping,libgcc-s1,libstdc++6,locales,openssh-client,procps,python3,sed,tar,tzdata,util-linux,xz-utils,zlib1g"

mkdir -p "$(dirname "$OUT_TAR")"
OUT_ABS="$(cd "$(dirname "$OUT_TAR")" && pwd)/$(basename "$OUT_TAR")"

run_linux() {
    if [[ "$(uname -s)" == "Linux" ]]; then
        bash -lc "$1"
    else
        orb -m "$ARCH_VM" bash -lc "$1"
    fi
}

run_linux "mmdebstrap --arch=arm64 --variant=minbase --include='$PACKAGES' '$SUITE' '$OUT_ABS' '$MIRROR'"
