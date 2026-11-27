#ifndef VMM_COMMON_VMM_TYPES_H
#define VMM_COMMON_VMM_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    VMM_MAX_VCPUS = 8,
    VMM_PAGE_SHIFT = 12,
    VMM_PAGE_SIZE = 4096,
    VMM_PAGE_MASK = 4095,
    SOFTTLB_ENTRIES = 4096,
    SOFTTLB_MASK = SOFTTLB_ENTRIES - 1,
};

#define SOFTTLB_TAG_INVALID 1ULL
#define SOFTTLB_TAG_GLOBAL 2ULL

#define VMM_GICD_BASE 0x08000000ULL
#define VMM_GICD_SIZE 0x00010000ULL
#define VMM_GICC_BASE 0x08010000ULL
#define VMM_GICC_SIZE 0x00002000ULL
#define VMM_GICH_BASE 0x08030000ULL
#define VMM_GICH_SIZE 0x00010000ULL
#define VMM_GICV_BASE 0x08040000ULL
#define VMM_GICV_SIZE 0x00010000ULL

#define VMM_UART0_BASE 0x09000000ULL
#define VMM_UART0_SIZE 0x00001000ULL

#define VMM_RTC_BASE 0x09010000ULL
#define VMM_RTC_SIZE 0x00001000ULL

#define VMM_VIRTIO_MMIO_BASE 0x0a000000ULL
#define VMM_VIRTIO_MMIO_STRIDE 0x00000200ULL
#define VMM_VIRTIO_MMIO_SLOTS 16U
#define VMM_VIRTIO_MMIO_SIZE (VMM_VIRTIO_MMIO_STRIDE * VMM_VIRTIO_MMIO_SLOTS)

#define VMM_GPU_SHM_BASE 0x8000000000ULL
#define VMM_GPU_SHM_SIZE 0x200000000ULL

#define VMM_RAM_BASE 0x40000000ULL
#define VMM_RAM_ALIGN 0x00004000ULL
#define VMM_DTB_ADDR 0x40000000ULL
#define VMM_DTB_MAX_SIZE 0x00200000ULL
#define VMM_KERNEL_ADDR 0x40200000ULL

enum {
    VMM_VIRTIO_SLOT_BLK = 0,
    VMM_VIRTIO_SLOT_VSOCK = 1,
    VMM_VIRTIO_SLOT_NET = 2,
    VMM_VIRTIO_SLOT_BALLOON = 3,
    VMM_VIRTIO_SLOT_RNG = 4,
    VMM_VIRTIO_SLOT_FS_BASE = 5,
    VMM_VIRTIO_SLOT_FS_COUNT = 4,
    VMM_VIRTIO_SLOT_DISK_BASE = 9,
    VMM_VIRTIO_SLOT_DISK_COUNT = 4,
    VMM_VIRTIO_SLOT_GPU = 13,
};

enum {
    VMM_IRQ_PPI_BASE = 16,
    VMM_IRQ_GIC_MAINT = 25,
    VMM_IRQ_TIMER_HYP_PHYS = 26,
    VMM_IRQ_TIMER_VIRT = 27,
    VMM_IRQ_TIMER_HYP_VIRT = 28,
    VMM_IRQ_TIMER_SEC_PHYS = 29,
    VMM_IRQ_TIMER_PHYS = 30,
    VMM_IRQ_SPI_BASE = 32,
    VMM_IRQ_UART0 = 33,
    VMM_IRQ_RTC = 34,
    VMM_IRQ_VIRTIO_BASE = 48,
};

typedef enum softtlb_bank : uint8_t {
    SOFTTLB_EL0_R = 0,
    SOFTTLB_EL0_W = 1,
    SOFTTLB_EL1_R = 2,
    SOFTTLB_EL1_W = 3,
    SOFTTLB_BANK_COUNT = 4,
} softtlb_bank_t;

typedef struct softtlb_entry {
    uint64_t tag;
    uintptr_t addend;
} softtlb_entry_t;

static_assert(sizeof(softtlb_entry_t) == 16);

typedef enum vmm_tlbi_target : uint8_t {
    VMM_TLBI_HOST = 0,
    VMM_TLBI_GUEST = 1,
    VMM_TLBI_GUEST_ALL = 2,
} vmm_tlbi_target_t;

typedef struct vmm_tlbi {
    uint64_t va;
    uint64_t pages;
    uint16_t asid;
    bool all_asids;
    vmm_tlbi_target_t target;
    uint16_t vmid;
} vmm_tlbi_t;

typedef struct vmm_machine vmm_machine_t;
typedef struct vcpu vcpu_t;

#endif
