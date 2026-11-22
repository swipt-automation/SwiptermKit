#ifndef VMM_CPU_CPU_H
#define VMM_CPU_CPU_H

#include "../common/vmm_types.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ir_insn ir_insn_t;

enum {
    VCPU_ICACHE_PENDING = 16,
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
    uint64_t afsr0_el1;
    uint64_t afsr1_el1;
    uint64_t amair_el1;
    uint64_t actlr_el1;
    uint64_t osdlr_el1;
    uint64_t oslsr_el1;
    uint64_t uao;
    uint64_t dbgbvr[16];
    uint64_t dbgbcr[16];
    uint64_t dbgwvr[16];
    uint64_t dbgwcr[16];
} vcpu_sysreg_state_t;

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
    int32_t backedge_budget;
    uint32_t fpcr;
    uint32_t fpsr;
    vcpu_stop_reason_t stop_reason;
    uint32_t cpu_id;

    _Atomic uint32_t irq_pending;
    _Atomic bool stop_requested;
    _Atomic bool halted_wfi;
    _Atomic bool event_reg;

    uint64_t tlb_asid_salt;
    uint64_t tlb_epoch_local;
    uint64_t icache_epoch_local;
    bool icache_shootdown_pending;
    bool icache_shootdown_all;
    uint8_t icache_pending_count;
    vmm_machine_t *vm;
    void *icache;
    const ir_insn_t *fast_next;
    uint64_t exec_epoch;
    uint64_t ras_epoch;

    alignas(16) __uint128_t v[32];

    uint64_t excl_pa;
    uint64_t ic_ivau_last_va_page;
    alignas(16) __uint128_t excl_val;

    uint64_t sctlr_el1;
    uint64_t cpacr_el1;
    uint64_t tcr_el1;
    uint64_t ttbr0_el1;
    uint64_t ttbr1_el1;
    uint64_t mair_el1;
    uint64_t vbar_el1;
    uint64_t esr_el1;
    uint64_t far_el1;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t tpidr_el1;
    uint64_t contextidr_el1;
    uint64_t cntkctl_el1;
    uint64_t cntv_ctl_el0;
    uint64_t cntv_cval_el0;
    uint64_t cntp_ctl_el0;
    uint64_t cntp_cval_el0;
    uint64_t mdscr_el1;
    uint64_t par_el1;
    uint64_t mpidr_el1;
    vcpu_sysreg_state_t sysregs;

    alignas(64) softtlb_entry_t tlb[SOFTTLB_BANK_COUNT][SOFTTLB_ENTRIES];
    uintptr_t excl_hva;
    _Atomic uintptr_t wfe_watch_hva;
    uint64_t icache_pending_pa[VCPU_ICACHE_PENDING];
};

static_assert(alignof(vcpu_t) >= 16);
static_assert(sizeof(((vcpu_t *)0)->v) == 32 * 16);

void vcpu_init(vcpu_t *cpu, vmm_machine_t *vm, uint32_t cpu_id);
void vcpu_destroy(vcpu_t *cpu);
void vcpu_reset(vcpu_t *cpu, uint64_t entry_pc, uint64_t x0);
vcpu_stop_reason_t vcpu_run(vcpu_t *cpu);
void vcpu_raise_exception(vcpu_t *cpu, uint32_t esr, uint64_t far, uint64_t elr);
void vcpu_eret(vcpu_t *cpu);
void vcpu_flush_icache_all(vcpu_t *cpu);
bool vcpu_flush_icache_va(vcpu_t *cpu, uint64_t va);
void vcpu_flush_icache_pa(vcpu_t *cpu, uint64_t pa_page);

#endif
