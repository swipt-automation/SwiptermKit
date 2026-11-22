    .section __TEXT,__const
    .p2align 14
swipterm_debian_rootfs_bytes:
#ifdef ROOTFS_ARCHIVE
    .incbin ROOTFS_ARCHIVE
#endif
swipterm_debian_rootfs_end:

    .section __DATA,__const
    .globl _swipterm_debian_rootfs_data
    .p2align 3
_swipterm_debian_rootfs_data:
    .quad swipterm_debian_rootfs_bytes

    .globl _swipterm_debian_rootfs_size
    .p2align 3
_swipterm_debian_rootfs_size:
    .quad swipterm_debian_rootfs_end - swipterm_debian_rootfs_bytes
