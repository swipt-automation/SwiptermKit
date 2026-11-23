#ifndef VMM_BOARD_BOARD_INTERNAL_H
#define VMM_BOARD_BOARD_INTERNAL_H

#include "machine.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VMM_TIMER_FREQ 24000000ULL

typedef struct gicv2_snapshot {
    uint32_t dist_enabled;
    uint32_t spi_enabled[3];
    uint32_t spi_pending[3];
    uint32_t spi_level[3];
    uint32_t spi_active[3];
    uint8_t spi_priority[96];
    uint8_t spi_targets[96];
    uint32_t spi_icfgr[6];
    struct {
        uint32_t ctlr;
        uint32_t pmr;
        uint32_t bpr;
        uint32_t enabled_local;
        uint32_t pending_local;
        uint32_t level_local;
        uint32_t active_local;
        uint32_t icfgr_ppi;
        uint8_t sgi_pending[16];
        uint8_t priority_local[32];
    } cpu[VMM_MAX_VCPUS];
} gicv2_snapshot_t;

typedef struct pl011_snapshot {
    uint32_t ilpr;
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t lcr_h;
    uint32_t cr;
    uint32_t ifls;
    uint32_t imsc;
    uint32_t ris;
    uint32_t dmacr;
} pl011_snapshot_t;

typedef struct pl031_snapshot {
    int64_t tick_offset;
    uint32_t mr;
    uint32_t lr;
    uint32_t cr;
    uint32_t imsc;
    uint32_t ris;
} pl031_snapshot_t;

typedef struct vmm_timer_state {
    pthread_t thread;
    bool thread_started;
    bool initialized;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint64_t deadlines[VMM_MAX_VCPUS];
    bool stop;
    _Atomic uint64_t boot_ticks;
    _Atomic bool active;
} vmm_timer_state_t;

typedef enum vmm_preboot_phase {
    VMM_PREBOOT_DISABLED,
    VMM_PREBOOT_ARMED,
    VMM_PREBOOT_REACHED,
    VMM_PREBOOT_RELEASED,
    VMM_PREBOOT_COMPLETE,
} vmm_preboot_phase_t;

typedef struct vmm_machine_priv {
    vmm_machine_t pub;
    uint64_t priv_magic;

    pthread_t threads[VMM_MAX_VCPUS];
    bool thread_started[VMM_MAX_VCPUS];
    _Atomic bool cpu_online[VMM_MAX_VCPUS];

    pthread_mutex_t wfi_lock[VMM_MAX_VCPUS];
    pthread_cond_t wfi_cond[VMM_MAX_VCPUS];

    pthread_mutex_t wfe_lock[VMM_MAX_VCPUS];
    pthread_cond_t wfe_cond[VMM_MAX_VCPUS];

    pthread_mutex_t state_lock;
    pthread_cond_t suspend_cond;
    uint32_t suspended_count;
    bool halt_notified;
    bool restored_from_snapshot;
    _Atomic vmm_preboot_phase_t preboot_phase;
    uint8_t snapshot_key[32];

    uint64_t kernel_entry_pc;
    uint64_t dtb_addr;
    int blk_fd;

    vmm_timer_state_t timer;
} vmm_machine_priv_t;

#define VMM_MACHINE_PRIV_MAGIC 0x535749504d414348ULL

static inline vmm_machine_priv_t *vmm_priv(vmm_machine_t *vm) {
    if (!vm) {
        return NULL;
    }
    vmm_machine_priv_t *priv = (vmm_machine_priv_t *)vm;
    return priv->priv_magic == VMM_MACHINE_PRIV_MAGIC ? priv : NULL;
}

static inline const vmm_machine_priv_t *vmm_priv_const(const vmm_machine_t *vm) {
    if (!vm) {
        return NULL;
    }
    const vmm_machine_priv_t *priv = (const vmm_machine_priv_t *)vm;
    return priv->priv_magic == VMM_MACHINE_PRIV_MAGIC ? priv : NULL;
}

static inline uint64_t vmm_initrd_addr(size_t ram_size, size_t initramfs_size) {
    if (initramfs_size == 0 || initramfs_size >= ram_size) {
        return 0;
    }
    return (VMM_RAM_BASE + (uint64_t)ram_size - (uint64_t)initramfs_size) & ~(uint64_t)VMM_PAGE_MASK;
}

void *gicv2_create(vmm_machine_t *vm);
void gicv2_destroy(void *gic);
void gicv2_set_timer_ppis(vmm_machine_t *vm, uint32_t cpu_id, bool virt, bool phys);
uint64_t gicv2_dist_read(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size);
void gicv2_dist_write(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val);
uint64_t gicv2_cpu_read(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size);
void gicv2_cpu_write(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val);
void gicv2_save_state(const void *gic, gicv2_snapshot_t *out);
void gicv2_load_state(void *gic, const gicv2_snapshot_t *in);

void *pl011_create(vmm_machine_t *vm);
void pl011_destroy(void *uart);
uint64_t pl011_read(void *uart, uint32_t offset, uint32_t size);
void pl011_write(void *uart, uint32_t offset, uint32_t size, uint64_t val);
void pl011_push_rx(vmm_machine_t *vm, const uint8_t *data, size_t len);
void pl011_save_state(const void *uart, pl011_snapshot_t *out);
void pl011_load_state(void *uart, const pl011_snapshot_t *in);

void *pl031_create(vmm_machine_t *vm);
void pl031_destroy(void *rtc);
uint64_t pl031_read(void *rtc, uint32_t offset, uint32_t size);
void pl031_write(void *rtc, uint32_t offset, uint32_t size, uint64_t val);
void pl031_save_state(const void *rtc, pl031_snapshot_t *out);
void pl031_load_state(void *rtc, const pl031_snapshot_t *in);

int vmm_timer_init(vmm_machine_priv_t *priv);
void vmm_timer_destroy(vmm_machine_priv_t *priv);
void vmm_timer_disarm_all(vmm_machine_priv_t *priv);
void vmm_timer_sync_all(vmm_machine_priv_t *priv);
void vmm_timer_set_cntvct(vmm_machine_t *vm, uint64_t cntvct);
void vmm_timer_sync(vcpu_t *cpu);

void vmm_vcpu_wake(vmm_machine_t *vm, uint32_t cpu_id);
void vmm_vcpu_wfe(vcpu_t *cpu);
void vmm_vcpu_sev(vmm_machine_t *vm);
void vmm_vcpu_wfe_notify_write(vmm_machine_t *vm, uintptr_t hva);
bool vmm_handle_psci(vcpu_t *cpu);

#endif
