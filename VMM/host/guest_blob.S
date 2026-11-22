    .section __TEXT,__const
    .p2align 14
swipterm_guest_kernel_bytes:
#ifdef SWIPTERM_GUEST_KERNEL_IMAGE
    .incbin SWIPTERM_GUEST_KERNEL_IMAGE
#endif
swipterm_guest_kernel_end:

    .p2align 14
swipterm_guest_initramfs_bytes:
#ifdef SWIPTERM_GUEST_INITRAMFS
    .incbin SWIPTERM_GUEST_INITRAMFS
#endif
swipterm_guest_initramfs_end:

    .section __DATA,__const
    .globl _swipterm_guest_kernel_data
    .p2align 3
_swipterm_guest_kernel_data:
    .quad swipterm_guest_kernel_bytes

    .globl _swipterm_guest_kernel_size
    .p2align 3
_swipterm_guest_kernel_size:
    .quad swipterm_guest_kernel_end - swipterm_guest_kernel_bytes

    .globl _swipterm_guest_initramfs_data
    .p2align 3
_swipterm_guest_initramfs_data:
    .quad swipterm_guest_initramfs_bytes

    .globl _swipterm_guest_initramfs_size
    .p2align 3
_swipterm_guest_initramfs_size:
    .quad swipterm_guest_initramfs_end - swipterm_guest_initramfs_bytes
