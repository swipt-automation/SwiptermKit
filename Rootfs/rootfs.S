    .section __TEXT,__const
    .p2align 4
swipterm_alpine_rootfs_bytes:
    .incbin ROOTFS_ARCHIVE
swipterm_alpine_rootfs_end:

    .section __DATA,__const
    .globl _swipterm_alpine_rootfs_data
    .p2align 3
_swipterm_alpine_rootfs_data:
    .quad swipterm_alpine_rootfs_bytes

    .globl _swipterm_alpine_rootfs_size
    .p2align 3
_swipterm_alpine_rootfs_size:
    .quad swipterm_alpine_rootfs_end - swipterm_alpine_rootfs_bytes
