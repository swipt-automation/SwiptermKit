#ifndef VMM_BOARD_MACHINE_H
#define VMM_BOARD_MACHINE_H

#include "../cpu/cpu.h"
#include "../virtio/virtio_mmio.h"

#include <os/lock.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct vmm_config {
    size_t ram_size;
    uint32_t cpu_count;
    const void *kernel_image;
    size_t kernel_size;
    const void *initramfs;
    size_t initramfs_size;
    const char *root_disk_path;
    const char *snapshot_path;
    uint8_t snapshot_key[32];
    const char *cmdline;
    bool gpu;
    swipterm_network *network;
    void *callback_ctx;
    void (*on_halt)(void *ctx);
} vmm_config_t;

enum {
    VMM_TLB_RING = 64,
};

typedef struct vmm_shm_window {
    uint64_t base;
    uint64_t size;
    _Atomic uintptr_t *pages;
} vmm_shm_window_t;

struct vmm_machine {
    uint8_t *ram;
    size_t ram_size;

    uint32_t cpu_count;
    vcpu_t *cpus[VMM_MAX_VCPUS];

    alignas(128) _Atomic uint64_t tlb_epoch;
    alignas(128) _Atomic uint32_t wfe_waiters;
    alignas(128) os_unfair_lock tlb_lock;
    alignas(128) _Atomic bool running;
    _Atomic bool suspended;
    _Atomic uint64_t timer_base;

    virtio_dev_t *virtio_devs[VMM_VIRTIO_MMIO_SLOTS];
    vmm_shm_window_t gpu_shm;

    void *callback_ctx;
    void (*on_halt)(void *ctx);

    vmm_tlbi_t tlb_ring[VMM_TLB_RING];
    struct cpu_pool *cpu_pool;
};

vmm_machine_t *vmm_create(const vmm_config_t *config, int *err_out);
int vmm_start(vmm_machine_t *vm);
void vmm_suspend(vmm_machine_t *vm);
void vmm_resume(vmm_machine_t *vm);
void vmm_destroy(vmm_machine_t *vm);
int vmm_finish_preboot(vmm_machine_t *vm, const char *snapshot_path);
bool vmm_preboot_trap(vcpu_t *cpu);

uint64_t vmm_mmio_read(vcpu_t *cpu, uint64_t gpa, uint32_t size);
void vmm_mmio_write(vcpu_t *cpu, uint64_t gpa, uint32_t size, uint64_t val);

#define VMM_TIMER_FREQ 24000000ULL

uint64_t vmm_timer_ticks(void);
uint64_t vmm_timer_cntvct(const vmm_machine_t *vm);
uint64_t vmm_sample_cpu_time_ns(vmm_machine_t *vm);
bool vmm_timer_native(void);
void vmm_timer_write(vcpu_t *cpu, uint64_t *reg, uint64_t value);
void vmm_vcpu_wfi(vcpu_t *cpu);

void gicv2_set_irq(vmm_machine_t *vm, uint32_t intid, bool level);

int vmm_attach_virtio_fs(vmm_machine_t *vm, const char *tag, const char *host_root,
                         bool read_only, uint32_t *slot_out);
int vmm_attach_virtio_blk(vmm_machine_t *vm, int fd, uint32_t *slot_out);
void vmm_detach_virtio(vmm_machine_t *vm, uint32_t slot);

#endif
