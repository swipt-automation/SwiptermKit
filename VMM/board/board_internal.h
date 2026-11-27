#ifndef VMM_BOARD_BOARD_INTERNAL_H
#define VMM_BOARD_BOARD_INTERNAL_H

#include "machine.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GICV2_LIST_REGS = 4,
    GICV2_NUM_SPIS = 96,
};

typedef struct gicv2_vif {
    uint32_t hcr;
    uint32_t vmcr;
    uint32_t apr;
    uint32_t lr[GICV2_LIST_REGS];
} gicv2_vif_t;

typedef struct gicv2_cpu_state {
    uint32_t ctlr;
    uint32_t pmr;
    uint32_t bpr;
    uint32_t enabled_local;
    uint32_t pending_local;
    uint32_t level_local;
    uint32_t active_local;
    uint32_t icfgr_ppi;
    uint32_t apr;
    uint8_t sgi_pending[16];
    uint8_t priority_local[32];
    gicv2_vif_t vif;
} gicv2_cpu_state_t;

typedef struct gicv2_state {
    bool dist_enabled;
    uint32_t spi_enabled[3];
    uint32_t spi_pending[3];
    uint32_t spi_level[3];
    uint32_t spi_active[3];
    uint8_t spi_priority[GICV2_NUM_SPIS];
    uint8_t spi_targets[GICV2_NUM_SPIS];
    uint32_t spi_icfgr[6];
    gicv2_cpu_state_t cpu[VMM_MAX_VCPUS];
} gicv2_state_t;

typedef struct pl011_state {
    uint32_t ilpr;
    uint32_t ibrd;
    uint32_t fbrd;
    uint32_t lcr_h;
    uint32_t cr;
    uint32_t ifls;
    uint32_t imsc;
    uint32_t ris;
    uint32_t dmacr;
} pl011_state_t;

typedef struct pl031_state {
    int64_t tick_offset;
    uint32_t mr;
    uint32_t lr;
    uint32_t cr;
    uint32_t imsc;
} pl031_state_t;

typedef struct gicv2 gicv2_t;
typedef struct pl011 pl011_t;
typedef struct pl031 pl031_t;

typedef struct vmm_timer_state {
    pthread_t thread;
    bool thread_started;
    bool initialized;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint64_t deadlines[VMM_MAX_VCPUS];
    bool stop;
} vmm_timer_state_t;

typedef enum vmm_preboot_phase {
    VMM_PREBOOT_ARMED,
    VMM_PREBOOT_REACHED,
    VMM_PREBOOT_RELEASED,
    VMM_PREBOOT_COMPLETE,
} vmm_preboot_phase_t;

typedef struct vmm_machine_priv {
    vmm_machine_t pub;

    gicv2_t *gic;
    pl011_t *uart;
    pl031_t *rtc;

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

    vmm_timer_state_t timer;
} vmm_machine_priv_t;

static inline vmm_machine_priv_t *vmm_priv(vmm_machine_t *vm) {
    return (vmm_machine_priv_t *)vm;
}

gicv2_t *gicv2_create(vmm_machine_t *vm);
void gicv2_destroy(gicv2_t *g);
void gicv2_set_timer_ppis(vmm_machine_t *vm, uint32_t cpu_id, uint32_t lines);
uint64_t gicv2_dist_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint32_t size);
void gicv2_dist_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val);
uint64_t gicv2_cpu_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset);
void gicv2_cpu_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val);
uint64_t gicv2_hyp_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset);
void gicv2_hyp_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val);
uint64_t gicv2_vcpu_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset);
void gicv2_vcpu_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val);
void gicv2_save_state(gicv2_t *g, gicv2_state_t *out);
void gicv2_load_state(gicv2_t *g, const gicv2_state_t *in);

pl011_t *pl011_create(vmm_machine_t *vm);
void pl011_destroy(pl011_t *u);
uint64_t pl011_read(pl011_t *u, uint32_t offset);
void pl011_write(pl011_t *u, uint32_t offset, uint64_t val);
void pl011_save_state(pl011_t *u, pl011_state_t *out);
void pl011_load_state(pl011_t *u, const pl011_state_t *in);

pl031_t *pl031_create(void);
void pl031_destroy(pl031_t *r);
uint64_t pl031_read(pl031_t *r, uint32_t offset);
void pl031_write(pl031_t *r, uint32_t offset, uint64_t val);
void pl031_save_state(pl031_t *r, pl031_state_t *out);
void pl031_load_state(pl031_t *r, const pl031_state_t *in);

int fdt_build(const vmm_machine_t *vm, const vmm_config_t *config, uint64_t initrd_start,
              void *buf, size_t buf_size);

int vmm_timer_init(vmm_machine_priv_t *priv);
void vmm_timer_destroy(vmm_machine_priv_t *priv);
void vmm_timer_disarm_all(vmm_machine_priv_t *priv);
void vmm_timer_sync_all(vmm_machine_priv_t *priv);
void vmm_timer_set_cntvct(vmm_machine_t *vm, uint64_t cntvct);
void vmm_timer_sync(vcpu_t *cpu);
void vmm_timer_sample_realtime(const vmm_machine_t *vm, uint64_t *realtime_ns, uint64_t *cntvct);

static inline void vmm_host_sev(void) {
    __asm__ __volatile__("dsb ish\n\tsev" ::: "memory");
}

void vmm_vcpu_wake(vmm_machine_t *vm, uint32_t cpu_id);

#endif
