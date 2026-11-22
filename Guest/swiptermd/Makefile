ifeq ($(origin CC), default)
CC := $(shell command -v musl-gcc >/dev/null 2>&1 && echo musl-gcc || echo gcc)
endif
CFLAGS ?= -static -Os -std=gnu11 -Wall -Wextra -Werror -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections
LDFLAGS ?= -static -Wl,--gc-sections

DAEMON_SRCS = main.c storage.c network.c container.c notify.c login.c ns_init.c readiness.c proc_pty.c fs_ops.c host_cmd_cuse.c sock_mirror.c
DAEMON_HDRS = protocol.h wire_format.h swiptermd.h
E2FSPROGS_TOOLS = mke2fs e2fsck tune2fs

.PHONY: all clean

all: swiptermd cmd_stub modprobe initramfs.cpio

swiptermd: $(DAEMON_SRCS) $(DAEMON_HDRS)
	$(CC) $(CFLAGS) $(DAEMON_SRCS) -o $@ $(LDFLAGS)
	strip -s $@ 2>/dev/null || true

cmd_stub: cmd_stub.c
	$(CC) $(CFLAGS) cmd_stub.c -o $@ $(LDFLAGS)
	strip -s $@ 2>/dev/null || true

modprobe: modprobe.c $(DAEMON_HDRS)
	$(CC) $(CFLAGS) modprobe.c -o $@ $(LDFLAGS)
	strip -s $@ 2>/dev/null || true

initramfs.cpio: swiptermd cmd_stub modprobe $(addprefix $(E2FSPROGS_BIN)/,$(E2FSPROGS_TOOLS))
	@rm -rf .initramfs_stage
	@mkdir -p .initramfs_stage/proc .initramfs_stage/sys .initramfs_stage/dev \
		.initramfs_stage/run .initramfs_stage/sbin .initramfs_stage/mnt/lower \
		.initramfs_stage/mnt/upper .initramfs_stage/mnt/merged
	@cp swiptermd .initramfs_stage/init
	@cp cmd_stub .initramfs_stage/cmd_stub
	@cp modprobe .initramfs_stage/sbin/modprobe
	@cp $(addprefix $(E2FSPROGS_BIN)/,$(E2FSPROGS_TOOLS)) .initramfs_stage/sbin/
	@chmod 0755 .initramfs_stage/init .initramfs_stage/cmd_stub .initramfs_stage/sbin/*
	@(cd .initramfs_stage && find . | LC_ALL=C sort | cpio --quiet -o -H newc -R 0:0) > $@
	@rm -rf .initramfs_stage

clean:
	rm -rf swiptermd cmd_stub modprobe initramfs.cpio .initramfs_stage
