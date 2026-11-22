ARCH_VM ?= arch
TARGETS := all initramfs kernel install clean

.PHONY: $(TARGETS) digest check

RESOURCES := $(abspath $(CURDIR)/../Sources/SwiptermKit/Resources)
KERNEL_FRAGMENTS := $(sort $(wildcard $(CURDIR)/kernel/*.config))
ARTIFACTS := Image.gz initramfs.cpio.gz
INPUTS := Makefile $(patsubst $(CURDIR)/%,%,$(KERNEL_FRAGMENTS)) $(sort $(wildcard swiptermd/*.[ch]))
LOCK = { printf '%s  inputs\n' "$$(sha256sum $(INPUTS) | sha256sum | cut -d' ' -f1)"; \
	cd $(RESOURCES) && sha256sum $(ARTIFACTS); }

ifneq ($(shell uname -s),Linux)

$(TARGETS):
	orb -m $(ARCH_VM) make -C $(CURDIR) $@

else

SHELL := bash
.SHELLFLAGS := -eo pipefail -c
.DELETE_ON_ERROR:

LINUX_VERSION := 6.12
LINUX_SHA256 := b1a2562be56e42afb3f8489d4c2a7ac472ac23098f1ef1c1e40da601f54625eb
LINUX_URL := https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(LINUX_VERSION).tar.xz
E2FSPROGS_VERSION := 1.47.4
E2FSPROGS_SHA256 := fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b
E2FSPROGS_URL := https://cdn.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v$(E2FSPROGS_VERSION)/e2fsprogs-$(E2FSPROGS_VERSION).tar.xz
E2FSPROGS_CONFIGURE := \
	--disable-nls --disable-fuse2fs --disable-debugfs --disable-imager --disable-resizer \
	--disable-defrag --disable-uuidd --disable-e2initrd-helper --disable-tdb --disable-backtrace \
	--disable-mmp --disable-bmap-stats --disable-rpath --disable-elf-shlibs \
	--enable-libuuid --enable-libblkid --without-libarchive \
	--without-udev-rules-dir --without-crond-dir --without-systemd-unit-dir \
	"CFLAGS=-Os -ffunction-sections -fdata-sections" \
	"LDFLAGS_STATIC=-static -Wl,--gc-sections"

BUILD := $(CURDIR)/build
LINUX_WORK ?= $(or $(XDG_CACHE_HOME),$(HOME)/.cache)/swipterm-guest
JOBS ?= $(shell nproc)
SOURCE_DATE_EPOCH ?= 0

LINUX_TARBALL := $(LINUX_WORK)/linux-$(LINUX_VERSION).tar.xz
LINUX_SRC := $(LINUX_WORK)/linux-$(LINUX_VERSION)
KBUILD := $(LINUX_WORK)/kbuild-$(LINUX_VERSION)
E2FSPROGS_TARBALL := $(LINUX_WORK)/e2fsprogs-$(E2FSPROGS_VERSION).tar.xz
E2FSPROGS_SRC := $(LINUX_WORK)/e2fsprogs-$(E2FSPROGS_VERSION)

KMAKE = $(MAKE) -C $(LINUX_SRC) O=$(KBUILD) ARCH=arm64 \
	KBUILD_BUILD_USER=swipterm KBUILD_BUILD_HOST=swipterm KBUILD_BUILD_VERSION=1 \
	KBUILD_BUILD_TIMESTAMP="$$(LC_ALL=C date -u -d @$(SOURCE_DATE_EPOCH))"

ifeq ($(origin CC),default)
CC := $(shell command -v musl-gcc >/dev/null 2>&1 && echo musl-gcc || echo gcc)
endif
HOSTCC ?= cc
CFLAGS ?= -static -Os -std=gnu11 -Wall -Wextra -Werror -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections
LDFLAGS ?= -static -Wl,--gc-sections

DAEMON_HDRS := $(wildcard swiptermd/*.h)
DAEMON_SRCS := $(filter-out swiptermd/cmd_stub.c swiptermd/modprobe.c,$(sort $(wildcard swiptermd/*.c)))

define VERIFY_CONFIG
FNR == NR {
	if (match($$0, /^CONFIG_[A-Za-z0-9_]+=/))
		resolved[substr($$0, 1, RLENGTH - 1)] = $$0
	else if (/^# CONFIG_[A-Za-z0-9_]+ is not set$$/)
		resolved[$$2] = $$0
	next
}
/^CONFIG_/ || /^# CONFIG_[A-Za-z0-9_]+ is not set$$/ {
	symbol = /^#/ ? $$2 : substr($$0, 1, index($$0, "=") - 1)
	have = (symbol in resolved) ? resolved[symbol] : "absent"
	if (have != $$0 && !(have == "absent" && /^#/)) {
		printf "error: requested %s, resolved %s\n", $$0, have | "cat >&2"
		failed = 1
	}
}
END { exit failed }
endef
export VERIFY_CONFIG

all: kernel initramfs
kernel: $(BUILD)/Image.gz
initramfs: $(BUILD)/initramfs.cpio.gz

install: all
	install -D -m 0644 -t $(RESOURCES) $(addprefix $(BUILD)/,$(ARTIFACTS))
	$(LOCK) > artifacts.lock

clean:
	rm -rf $(BUILD)

define fetch
	mkdir -p $(@D)
	curl -fsSL --retry 3 -o $@.tmp $(1)
	echo "$(2)  $@.tmp" | sha256sum -c --quiet
	mv $@.tmp $@
endef

define extract
	echo "$(1)  $|" | sha256sum -c --quiet
	rm -rf $(@D)
	mkdir -p $(@D)
	tar -xf $| -C $(@D) --strip-components=1
	echo $(1) > $@
endef

$(LINUX_TARBALL):
	$(call fetch,$(LINUX_URL),$(LINUX_SHA256))

$(E2FSPROGS_TARBALL):
	$(call fetch,$(E2FSPROGS_URL),$(E2FSPROGS_SHA256))

$(LINUX_SRC)/.swipterm-stamp: | $(LINUX_TARBALL)
	$(call extract,$(LINUX_SHA256))

$(E2FSPROGS_SRC)/.swipterm-stamp: | $(E2FSPROGS_TARBALL)
	$(call extract,$(E2FSPROGS_SHA256))

$(BUILD)/e2fsprogs/e2fsck $(BUILD)/e2fsprogs/tune2fs &: $(E2FSPROGS_SRC)/.swipterm-stamp Makefile
	rm -rf $(E2FSPROGS_SRC)/build
	mkdir -p $(E2FSPROGS_SRC)/build
	cd $(E2FSPROGS_SRC)/build && ../configure -q $(E2FSPROGS_CONFIGURE)
	$(MAKE) -C $(E2FSPROGS_SRC)/build -s -j$(JOBS) libs
	$(MAKE) -C $(E2FSPROGS_SRC)/build/misc -s tune2fs.static
	$(MAKE) -C $(E2FSPROGS_SRC)/build/e2fsck -s e2fsck.static
	install -D -m 0755 -s $(E2FSPROGS_SRC)/build/misc/tune2fs.static $(BUILD)/e2fsprogs/tune2fs
	install -D -m 0755 -s $(E2FSPROGS_SRC)/build/e2fsck/e2fsck.static $(BUILD)/e2fsprogs/e2fsck

$(BUILD)/swiptermd: $(DAEMON_SRCS) $(DAEMON_HDRS) Makefile
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) -o $@ $(LDFLAGS)
	strip -s $@

$(BUILD)/cmd_stub: swiptermd/cmd_stub.c Makefile
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	strip -s $@

$(BUILD)/modprobe: swiptermd/modprobe.c $(DAEMON_HDRS) Makefile
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	strip -s $@

$(BUILD)/gen_init_cpio: $(LINUX_SRC)/.swipterm-stamp
	mkdir -p $(@D)
	$(HOSTCC) -O2 $(LINUX_SRC)/usr/gen_init_cpio.c -o $@

$(BUILD)/initramfs.list: Makefile
	mkdir -p $(@D)
	printf '%s\n' \
		'dir /dev 0755 0 0' \
		'dir /mnt 0755 0 0' \
		'dir /mnt/root 0755 0 0' \
		'dir /proc 0555 0 0' \
		'dir /run 0755 0 0' \
		'dir /sbin 0755 0 0' \
		'dir /sys 0555 0 0' \
		'file /init $(BUILD)/swiptermd 0755 0 0' \
		'file /cmd_stub $(BUILD)/cmd_stub 0755 0 0' \
		'file /sbin/modprobe $(BUILD)/modprobe 0755 0 0' \
		'file /sbin/e2fsck $(BUILD)/e2fsprogs/e2fsck 0755 0 0' \
		'file /sbin/tune2fs $(BUILD)/e2fsprogs/tune2fs 0755 0 0' > $@

$(BUILD)/initramfs.cpio.gz: $(BUILD)/initramfs.list $(BUILD)/gen_init_cpio $(BUILD)/swiptermd $(BUILD)/cmd_stub \
		$(BUILD)/modprobe $(BUILD)/e2fsprogs/e2fsck $(BUILD)/e2fsprogs/tune2fs
	$(BUILD)/gen_init_cpio -t $(SOURCE_DATE_EPOCH) $< | gzip -9n > $@

$(BUILD)/kernel.config: $(KERNEL_FRAGMENTS) $(LINUX_SRC)/.swipterm-stamp Makefile
	mkdir -p $(KBUILD) $(@D)
	$(KMAKE) -s virtconfig > /dev/null
	$(LINUX_SRC)/scripts/kconfig/merge_config.sh -Q -m -O $(KBUILD) $(KBUILD)/.config $(KERNEL_FRAGMENTS) > /dev/null
	$(KMAKE) -s olddefconfig
	$(KMAKE) -s mod2noconfig
	awk "$$VERIFY_CONFIG" $(KBUILD)/.config $(KERNEL_FRAGMENTS)
	cp $(KBUILD)/.config $@

$(BUILD)/Image.gz: $(BUILD)/kernel.config
	$(KMAKE) -j$(JOBS) Image
	gzip -9n < $(KBUILD)/arch/arm64/boot/Image > $@

endif

digest:
	$(LOCK) > artifacts.lock

check:
	$(LOCK) | diff -u artifacts.lock - || { echo "error: Guest/artifacts.lock does not match; run make -C Guest install" >&2; exit 1; }
