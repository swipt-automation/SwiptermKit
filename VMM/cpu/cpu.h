#ifndef VMM_CPU_CPU_H
#define VMM_CPU_CPU_H

#include "../common/vmm_types.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ir_insn ir_insn_t;

enum {
    VCPU_ICACHE_PENDING = 64,
};

typedef void (*ir_handler_fn)(vcpu_t *cpu, const ir_insn_t *ir,
                              softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);

struct ir_insn {
    ir_handler_fn handler;
    union {
        uint64_t target_or_imm;
        int64_t simm;
        const ir_insn_t *target_ir;
    };
    uint8_t rd;
    uint8_t rn;
    uint8_t rm;
    uint8_t ra;
    uint16_t op;
    uint16_t flags;
    uint32_t fast_offset;
    uint8_t fast_rd;
    uint8_t fast_rn;
    uint8_t fast_rm;
    uint8_t fast_ra;
};

static_assert(sizeof(ir_insn_t) == 32);

typedef enum vcpu_stop_reason : uint32_t {
    VCPU_STOP_NONE = 0,
    VCPU_STOP_HALT = 1,
    VCPU_STOP_SUSPEND = 2,
    VCPU_STOP_REBOOT = 3,
    VCPU_STOP_PANIC = 4,
} vcpu_stop_reason_t;

typedef struct vcpu_sysreg_state {
    uint64_t csselr_el1;
    uint64_t actlr_el1;
    uint64_t mdccint_el1;
    uint64_t osdlr_el1;
    uint64_t oslsr_el1;
    uint64_t uao;
    uint64_t dbgbvr[16];
    uint64_t dbgbcr[16];
    uint64_t dbgwvr[16];
    uint64_t dbgwcr[16];
} vcpu_sysreg_state_t;

typedef struct vcpu_mmu_ctx vcpu_mmu_ctx_t;

enum : uint8_t {
    VCPU_WORLD_GUEST = 0,
    VCPU_WORLD_HOST = 1,
    VCPU_WORLD_HOST_NVHE = 2,
};

enum {
    VCPU_CTX_MAIN = 0,
    VCPU_CTX_L2_SLOTS = 2,
    VCPU_CTX_COUNT = 1 + VCPU_CTX_L2_SLOTS,
};

enum : uint32_t {
    VCPU_IRQ_PHYS = 1U << 0,
    VCPU_IRQ_VIRT = 1U << 1,
    VCPU_IRQ_VI = 1U << 2,
};

enum {
    VCPU_TIMER_PHYS = 0,
    VCPU_TIMER_VIRT = 1,
    VCPU_TIMER_HYP_PHYS = 2,
    VCPU_TIMER_HYP_VIRT = 3,
    VCPU_TIMER_COUNT = 4,
};

typedef struct vcpu_timer {
    uint64_t ctl;
    uint64_t cval;
} vcpu_timer_t;

#define VCPU_BANKED_REGS(X)                                                    \
    X(sctlr_el1) X(cpacr_el1) X(tcr_el1) X(ttbr0_el1) X(ttbr1_el1)             \
    X(mair_el1) X(vbar_el1) X(esr_el1) X(far_el1) X(elr_el1) X(spsr_el1)       \
    X(contextidr_el1) X(cntkctl_el1) X(amair_el1) X(afsr0_el1) X(afsr1_el1)

#define VCPU_BANK_FIELD(name) uint64_t name;

typedef struct vcpu_bank {
    VCPU_BANKED_REGS(VCPU_BANK_FIELD)
} vcpu_bank_t;

typedef struct vcpu_el2_state {
    vcpu_bank_t shadow;
    uint64_t shadow_sp;
    uint64_t hcr;
    uint64_t mdcr;
    uint64_t hstr;
    uint64_t hacr;
    uint64_t actlr;
    uint64_t vttbr;
    uint64_t vtcr;
    uint64_t vpidr;
    uint64_t vmpidr;
    uint64_t hpfar;
    uint64_t cntvoff;
} vcpu_el2_state_t;

struct vcpu {
    uint64_t x[33];
    uint64_t fast_sp;
    uint64_t sp_el[2];
    uint64_t pc;
    uint32_t nzcv;
    uint32_t daif;
    uint8_t el;
    uint8_t spsel;
    uint8_t pan;
    uint8_t excl_size;
    bool excl_valid;
    bool fp_host_active;
    uint8_t world;
    bool has_el2;
    int32_t backedge_budget;
    uint32_t fpcr;
    uint32_t fpsr;
    vcpu_stop_reason_t stop_reason;

    _Atomic uint32_t irq_pending;
    _Atomic bool stop_requested;
    _Atomic bool halted_wfi;
    _Atomic bool event_reg;
    uint32_t cpu_id;
    uint8_t irq_mask;
    uint8_t irq_force;

    uint64_t tlb_asid_salt;
    uint64_t tlb_epoch_local;
    uint64_t icache_epoch_local;
    bool icache_shootdown_pending;
    bool icache_shootdown_all;
    uint8_t icache_pending_count;
    uint16_t tlb_user_gen;
    vmm_machine_t *vm;
    void *icache;
    const ir_insn_t *fast_next;
    uint64_t exec_epoch;
    uint64_t ras_epoch;

    alignas(16) __uint128_t v[32];

    uint64_t excl_pa;
    uint64_t ic_ivau_last_va_page;
    alignas(16) __uint128_t excl_val;

    union {
        vcpu_bank_t bank;
        struct {
            VCPU_BANKED_REGS(VCPU_BANK_FIELD)
        };
    };
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t tpidr_el1;
    uint64_t tpidr_el2;
    uint64_t cntpct_base;
    uint64_t cntvct_base;
    uint64_t mdscr_el1;
    uint64_t par_el1;
    uint64_t mpidr_el1;
    vcpu_timer_t timers[VCPU_TIMER_COUNT];
    vcpu_sysreg_state_t sysregs;

    softtlb_entry_t (*tlb)[SOFTTLB_ENTRIES];
    void *exec_cache;
    vcpu_mmu_ctx_t *mmu_ctx[VCPU_CTX_COUNT];
    vcpu_el2_state_t el2;
    uintptr_t excl_hva;
    _Atomic uintptr_t wfe_watch_hva;
    uint64_t icache_pending_pa[VCPU_ICACHE_PENDING];
    uint32_t wfe_poll_ticks;
    uint32_t wfi_poll_ticks;
    uint32_t traps;
    uint32_t guest_traps;
    uint16_t l2_vmid[VCPU_CTX_L2_SLOTS];
    uint8_t ctx_slot;
    uint8_t ctx_stale;
    uint8_t l2_victim;
    bool main_guest;
};

static_assert(alignof(vcpu_t) >= 16);
static_assert(sizeof(((vcpu_t *)0)->v) == 32 * 16);

void vcpu_init(vcpu_t *cpu, vmm_machine_t *vm, uint32_t cpu_id);
void vcpu_destroy(vcpu_t *cpu);
void vcpu_enable_el2(vcpu_t *cpu);
void vcpu_reset(vcpu_t *cpu, uint64_t entry_pc, uint64_t x0);
void vcpu_set_world(vcpu_t *cpu, uint8_t world);
void vcpu_sync_el2(vcpu_t *cpu);
void vcpu_update_irq_gate(vcpu_t *cpu);
void vcpu_update_counter_bases(vcpu_t *cpu);
vcpu_stop_reason_t vcpu_run(vcpu_t *cpu);
void vcpu_raise_exception(vcpu_t *cpu, uint32_t esr, uint64_t far, uint64_t elr);
void vcpu_eret(vcpu_t *cpu);
void vcpu_flush_icache_all(vcpu_t *cpu);
bool vcpu_flush_icache_va(vcpu_t *cpu, uint64_t va);
void vcpu_flush_icache_pa(vcpu_t *cpu, uint64_t pa_page);

#endif
