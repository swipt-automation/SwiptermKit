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
    bool preboot_checkpoint;
    bool snapshot_required;
    uint8_t snapshot_key[32];
    const char *cmdline;
    swipterm_network *network;
    void *callback_ctx;
    void (*console_output)(void *ctx, const void *data, size_t len);
    void (*on_halt)(void *ctx);
    void (*on_panic)(void *ctx, const char *message);
} vmm_config_t;

enum {
    VMM_TLB_RING = 64,
    VMM_ICACHE_RING = 64,
};

typedef struct vmm_icache_shootdown {
    uint32_t count;
    uint64_t pa_pages[VCPU_ICACHE_PENDING];
} vmm_icache_shootdown_t;

struct vmm_machine {
    uint8_t *ram;
    size_t ram_size;

    uint32_t cpu_count;
    vcpu_t *cpus[VMM_MAX_VCPUS];

    _Atomic uint64_t tlb_epoch;
    _Atomic uint64_t icache_epoch;
    _Atomic uint32_t wfe_waiters;
    _Atomic bool running;
    _Atomic bool suspended;

    virtio_dev_t *virtio_devs[VMM_VIRTIO_MMIO_SLOTS];
    void *gic;
    void *uart;
    void *rtc;

    void *callback_ctx;
    void (*console_output)(void *ctx, const void *data, size_t len);
    void (*on_halt)(void *ctx);
    void (*on_panic)(void *ctx, const char *message);

    os_unfair_lock tlb_lock;
    vmm_tlbi_t tlb_ring[VMM_TLB_RING];
    os_unfair_lock icache_lock;
    vmm_icache_shootdown_t icache_ring[VMM_ICACHE_RING];
    _Atomic uint64_t *icache_ram_bitmap;
    size_t icache_ram_words;
};

vmm_machine_t *vmm_create(const vmm_config_t *config, int *err_out);
int vmm_start(vmm_machine_t *vm);
int vmm_suspend(vmm_machine_t *vm);
int vmm_resume(vmm_machine_t *vm);
void vmm_stop(vmm_machine_t *vm);
void vmm_destroy(vmm_machine_t *vm);
int vmm_save_snapshot(vmm_machine_t *vm, const char *path);
bool vmm_is_warm_boot(vmm_machine_t *vm);
int vmm_finish_preboot(vmm_machine_t *vm, const char *snapshot_path, int *save_error);
bool vmm_preboot_trap(vcpu_t *cpu);

uint64_t vmm_mmio_read(vmm_machine_t *vm, vcpu_t *cpu, uint64_t gpa, uint32_t size);
void vmm_mmio_write(vmm_machine_t *vm, vcpu_t *cpu, uint64_t gpa, uint32_t size, uint64_t val);

uint64_t vmm_timer_cntfrq(void);
uint64_t vmm_timer_cntvct(const vmm_machine_t *vm);
void vmm_timer_write(vcpu_t *cpu, uint64_t *reg, uint64_t value);
void vmm_vcpu_wfi(vcpu_t *cpu);

void gicv2_set_irq(vmm_machine_t *vm, uint32_t intid, bool level);

int fdt_build(const vmm_machine_t *vm, const vmm_config_t *config,
              void *buf, size_t buf_size, size_t *out_size);

int vmm_attach_virtio_fs(vmm_machine_t *vm, const char *tag, const char *host_root,
                         bool read_only, uint32_t *slot_out);
void vmm_detach_virtio_fs(vmm_machine_t *vm, uint32_t slot);

#endif
