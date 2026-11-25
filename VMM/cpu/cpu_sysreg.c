#include "cpu_internal.h"
#include "cpu_fast_layout.h"
#include "generated/a64_sysreg_gen.inc"

#include <sys/sysctl.h>

enum : uint64_t {
    ID_MIDR_VAL = 0x000F0010ULL,
    ID_AA64PFR0_VAL = (1ULL << 0) | (1ULL << 4) | (1ULL << 16) | (1ULL << 20) | (1ULL << 56) | (1ULL << 60),
    ID_AA64PFR1_VAL = 0x0000000000000000ULL,
    ID_AA64DFR0_VAL = 0x0000000000101006ULL,
    ID_AA64ISAR0_VAL = (2ULL << 4) | (1ULL << 8) | (1ULL << 16) | (2ULL << 20) | (1ULL << 28) |
                       (1ULL << 44) | (1ULL << 48) | (2ULL << 52) | (2ULL << 56),
    ID_AA64ISAR0_SHA256 = 1ULL << 12,
    ID_AA64ISAR0_SHA512 = 2ULL << 12,
    ID_AA64ISAR0_SHA3 = 1ULL << 32,
    ID_AA64ISAR1_VAL = (2ULL << 0) | (1ULL << 12) | (1ULL << 16) | (2ULL << 20) | (1ULL << 36),
    ID_AA64ISAR1_FRINTTS = 1ULL << 32,
    ID_AA64ISAR1_BF16 = 1ULL << 44,
    ID_AA64ISAR1_I8MM = 1ULL << 52,
    ID_AA64ISAR2_VAL = (1ULL << 16) | (1ULL << 20),
    ID_AA64ISAR2_CSSC = 1ULL << 52,
    ID_AA64MMFR0_VAL = 0x000000000F100022ULL | (1ULL << 60),
    ID_AA64MMFR1_VAL = (2ULL << 0) | (2ULL << 20) | (1ULL << 60),
    ID_AA64PFR0_EL2 = 1ULL << 8,
    ID_AA64MMFR0_TGRAN_2 = (2ULL << 32) | (1ULL << 36) | (2ULL << 40),
    ID_AA64MMFR1_VH = 1ULL << 8,
    ID_AA64MMFR2_VAL = 1ULL << 60,
    ID_AA64MMFR2_FWB = 1ULL << 40,
    ID_AA64MMFR2_AT = 1ULL << 32,
    CTR_EL0_VAL = 0x9444C004ULL,
    CLIDR_EL1_VAL = 0x0A200023ULL,
    CCSIDR_L1D_VAL = (255ULL << 13) | (7ULL << 3) | 2ULL,
    CCSIDR_L1I_VAL = (511ULL << 13) | (5ULL << 3) | 2ULL,
    CCSIDR_L2_VAL = (16383ULL << 13) | (11ULL << 3) | 2ULL,
};

static uint64_t cpu_ccsidr_encode(uint64_t size, uint64_t base_assoc, uint64_t fallback) {
    if (size < 64ULL * base_assoc) {
        return fallback;
    }
    uint64_t assoc = (size % 3ULL == 0) ? (base_assoc / 4ULL * 3ULL) : base_assoc;
    while (size / (64ULL * assoc) > 32768ULL && assoc <= 512ULL) {
        assoc <<= 1;
    }
    uint64_t sets = size / (64ULL * assoc);
    if (sets == 0 || sets > 32768ULL) {
        return fallback;
    }
    return ((sets - 1ULL) << 13) | ((assoc - 1ULL) << 3) | 2ULL;
}

static uint64_t cpu_ccsidr_val(uint64_t csselr) {
    static _Atomic uint64_t cached[3];
    if (csselr >= 3ULL) {
        return 0;
    }
    uint64_t val = atomic_load_explicit(&cached[csselr], memory_order_relaxed);
    if (val == 0) {
        static const char *const names[3] = {
            "hw.perflevel0.l1dcachesize",
            "hw.perflevel0.l1icachesize",
            "hw.perflevel0.l2cachesize",
        };
        static const uint64_t base_assocs[3] = {8ULL, 8ULL, 16ULL};
        static const uint64_t fallbacks[3] = {CCSIDR_L1D_VAL, CCSIDR_L1I_VAL, CCSIDR_L2_VAL};
        uint64_t size = 0;
        size_t len = sizeof(size);
        if (sysctlbyname(names[csselr], &size, &len, nullptr, 0) != 0) {
            size = 0;
        }
        val = cpu_ccsidr_encode(size, base_assocs[csselr], fallbacks[csselr]);
        atomic_store_explicit(&cached[csselr], val, memory_order_relaxed);
    }
    return val;
}

uint32_t cpu_host_features(void) {
    static const char *const names[] = {
#define CPU_HOST_FEATURE_NAME(name) "hw.optional.arm.FEAT_" #name,
        CPU_HOST_FEATURES(CPU_HOST_FEATURE_NAME)
#undef CPU_HOST_FEATURE_NAME
    };
    static _Atomic uint32_t cached;
    uint32_t features = atomic_load_explicit(&cached, memory_order_relaxed);
    if (features == 0) {
        features = 1U << 31;
        for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            int present = 0;
            size_t size = sizeof(present);
            if (sysctlbyname(names[i], &present, &size, nullptr, 0) == 0 && present) {
                features |= 1U << i;
            }
        }
        atomic_store_explicit(&cached, features, memory_order_relaxed);
    }
    return features;
}

bool vcpu_check_interrupts(vcpu_t *cpu) {
    bool icache_flushed = mmu_poll_shootdown(cpu);
    if (icache_flushed || cpu_jit_stale(cpu)) {
        return true;
    }
    if (atomic_load_explicit(&cpu->stop_requested, memory_order_relaxed)) {
        return true;
    }
    return vcpu_irq_deliverable(cpu) != 0;
}

uint32_t cpu_guest_traps(const vcpu_t *cpu) {
    static const struct {
        uint64_t hcr;
        uint32_t trap;
    } hcr_traps[] = {
        {HCR_EL2_TVM, CPU_TRAP_TVM},     {HCR_EL2_TRVM, CPU_TRAP_TRVM}, {HCR_EL2_TACR, CPU_TRAP_TACR},
        {HCR_EL2_TID1, CPU_TRAP_TID1},   {HCR_EL2_TID2, CPU_TRAP_TID2}, {HCR_EL2_TID3, CPU_TRAP_TID3},
        {HCR_EL2_TSW, CPU_TRAP_TSW},     {HCR_EL2_TPCP, CPU_TRAP_TPCP}, {HCR_EL2_TPU, CPU_TRAP_TPU},
        {HCR_EL2_TIDCP, CPU_TRAP_TIDCP}, {HCR_EL2_TDZ, CPU_TRAP_TDZ},   {HCR_EL2_TWI, CPU_TRAP_TWI},
        {HCR_EL2_TWE, CPU_TRAP_TWE},     {HCR_EL2_TTLB, CPU_TRAP_TTLB},
    };
    if (!cpu->has_el2) {
        return 0;
    }
    uint64_t hcr = cpu->el2.hcr;
    uint32_t traps = 0;
    for (uint32_t i = 0; i < sizeof(hcr_traps) / sizeof(hcr_traps[0]); i++) {
        if ((hcr & hcr_traps[i].hcr) != 0) {
            traps |= hcr_traps[i].trap;
        }
    }
    uint64_t mdcr = cpu->el2.mdcr;
    if ((mdcr & (MDCR_EL2_TDA | MDCR_EL2_TDE)) != 0) {
        traps |= CPU_TRAP_TDA;
    }
    if ((mdcr & (MDCR_EL2_TDOSA | MDCR_EL2_TDE)) != 0) {
        traps |= CPU_TRAP_TDOSA;
    }
    if ((mdcr & (MDCR_EL2_TPM | MDCR_EL2_TPMCR)) != 0) {
        traps |= CPU_TRAP_TPM;
    }
    uint64_t cnthctl = cpu->el2.shadow.cntkctl_el1;
    uint32_t el1_shift = (hcr & HCR_EL2_E2H) != 0 ? 10U : 0U;
    if ((cnthctl & (1ULL << el1_shift)) == 0) {
        traps |= CPU_TRAP_CNTPCT;
    }
    if ((cnthctl & (2ULL << el1_shift)) == 0) {
        traps |= CPU_TRAP_CNTP;
    }
    if ((cnthctl & (1ULL << 13)) != 0) {
        traps |= CPU_TRAP_CNTV;
    }
    if ((cnthctl & (1ULL << 14)) != 0) {
        traps |= CPU_TRAP_CNTVCT;
    }
    return traps;
}

bool cpu_sysreg_trapped(uint32_t traps, uint32_t word) {
    if (traps == 0) {
        return false;
    }
    uint32_t op0 = (word >> 19) & 3U;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crn = (word >> 12) & 15U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t need;
    switch ((uint16_t)(word >> 5)) {
    case A64_SYSREG_SCTLR_EL1:
    case A64_SYSREG_TTBR0_EL1:
    case A64_SYSREG_TTBR1_EL1:
    case A64_SYSREG_TCR_EL1:
    case A64_SYSREG_AFSR0_EL1:
    case A64_SYSREG_AFSR1_EL1:
    case A64_SYSREG_ESR_EL1:
    case A64_SYSREG_FAR_EL1:
    case A64_SYSREG_MAIR_EL1:
    case A64_SYSREG_AMAIR_EL1:
    case A64_SYSREG_CONTEXTIDR_EL1:
        need = ((word >> 21) & 1U) != 0 ? CPU_TRAP_TRVM : CPU_TRAP_TVM;
        break;
    case A64_SYSREG_ACTLR_EL1: need = CPU_TRAP_TACR; break;
    case A64_SYSREG_REVIDR_EL1:
    case A64_SYSREG_AIDR_EL1: need = CPU_TRAP_TID1; break;
    case A64_SYSREG_CTR_EL0:
    case A64_SYSREG_CCSIDR_EL1:
    case A64_SYSREG_CLIDR_EL1:
    case A64_SYSREG_CSSELR_EL1: need = CPU_TRAP_TID2; break;
    case A64_SYSREG_CNTPCT_EL0:
    case A64_SYSREG_CNTPCTSS_EL0: need = CPU_TRAP_CNTPCT; break;
    case A64_SYSREG_CNTP_TVAL_EL0:
    case A64_SYSREG_CNTP_CTL_EL0:
    case A64_SYSREG_CNTP_CVAL_EL0: need = CPU_TRAP_CNTP; break;
    case A64_SYSREG_CNTV_TVAL_EL0:
    case A64_SYSREG_CNTV_CTL_EL0:
    case A64_SYSREG_CNTV_CVAL_EL0: need = CPU_TRAP_CNTV; break;
    case A64_SYSREG_CNTVCT_EL0:
    case A64_SYSREG_CNTVCTSS_EL0: need = CPU_TRAP_CNTVCT; break;
    default:
        if (op0 == 2U) {
            need = op1 == 0U && crn == 1U ? CPU_TRAP_TDOSA : CPU_TRAP_TDA;
        } else if (op0 != 3U) {
            return false;
        } else if (op1 == 0U && crn == 0U && crm != 0U) {
            need = CPU_TRAP_TID3;
        } else if ((crn == 9U && crm >= 12U) || (op1 == 3U && crn == 14U && crm >= 8U)) {
            need = CPU_TRAP_TPM;
        } else if (crn == 11U || crn == 15U) {
            need = CPU_TRAP_TIDCP;
        } else {
            return false;
        }
        break;
    }
    return (traps & need) != 0;
}

bool cpu_sysop_trapped(uint32_t traps, uint32_t word) {
    if (traps == 0) {
        return false;
    }
    uint32_t need;
    switch ((uint16_t)((word >> 5) & 0x3FFFU)) {
    case A64_SYSOP_DC_ISW:
    case A64_SYSOP_DC_CSW:
    case A64_SYSOP_DC_CISW: need = CPU_TRAP_TSW; break;
    case A64_SYSOP_DC_IVAC:
    case A64_SYSOP_DC_CVAC:
    case A64_SYSOP_DC_CIVAC:
    case A64_SYSOP_DC_CVAP:
    case A64_SYSOP_DC_CVADP: need = CPU_TRAP_TPCP; break;
    case A64_SYSOP_DC_CVAU:
    case A64_SYSOP_IC_IVAU:
    case A64_SYSOP_IC_IALLU:
    case A64_SYSOP_IC_IALLUIS: need = CPU_TRAP_TPU; break;
    case A64_SYSOP_DC_ZVA: need = CPU_TRAP_TDZ; break;
    default:
        if (((word >> 16) & 7U) != 0U || ((word >> 12) & 15U) != 8U) {
            return false;
        }
        need = CPU_TRAP_TTLB;
        break;
    }
    return (traps & need) != 0;
}

static bool guest_trapped(const vcpu_t *cpu, uint32_t word, bool sysop) {
    if (cpu->world != VCPU_WORLD_GUEST ||
        !(sysop ? cpu_sysop_trapped(cpu->traps, word) : cpu_sysreg_trapped(cpu->traps, word))) {
        return false;
    }
    if (cpu->el != 0) {
        return true;
    }
    uint64_t sctlr = cpu->sctlr_el1;
    uint64_t kctl = cpu->cntkctl_el1;
    if (sysop) {
        switch ((uint16_t)((word >> 5) & 0x3FFFU)) {
        case A64_SYSOP_DC_ZVA: return (sctlr & (1ULL << 14)) != 0;
        case A64_SYSOP_DC_CVAC:
        case A64_SYSOP_DC_CVAU:
        case A64_SYSOP_DC_CIVAC:
        case A64_SYSOP_DC_CVAP:
        case A64_SYSOP_DC_CVADP:
        case A64_SYSOP_IC_IVAU: return (sctlr & (1ULL << 26)) != 0;
        default: return false;
        }
    }
    switch ((uint16_t)(word >> 5)) {
    case A64_SYSREG_CTR_EL0: return (sctlr & (1ULL << 15)) != 0;
    case A64_SYSREG_CNTPCT_EL0:
    case A64_SYSREG_CNTPCTSS_EL0: return (kctl & 1ULL) != 0;
    case A64_SYSREG_CNTVCT_EL0:
    case A64_SYSREG_CNTVCTSS_EL0: return (kctl & 2ULL) != 0;
    case A64_SYSREG_CNTP_TVAL_EL0:
    case A64_SYSREG_CNTP_CTL_EL0:
    case A64_SYSREG_CNTP_CVAL_EL0: return (kctl & (1ULL << 9)) != 0;
    case A64_SYSREG_CNTV_TVAL_EL0:
    case A64_SYSREG_CNTV_CTL_EL0:
    case A64_SYSREG_CNTV_CVAL_EL0: return (kctl & (1ULL << 8)) != 0;
    default: return false;
    }
}

static bool raise_guest_trap(vcpu_t *cpu, const ir_insn_t *ir, uint32_t word, bool sysop) {
    if (!guest_trapped(cpu, word, sysop)) {
        return false;
    }
    cpu->pc = cpu_ir_pc(cpu, ir);
    vcpu_raise_exception_el2(cpu, vcpu_sys_trap_esr(word), 0, cpu->pc);
    return true;
}

typedef enum sysreg_bank_kind : uint8_t {
    BANK_NONE,
    BANK_UNDEF,
    BANK_ACTIVE,
    BANK_SHADOW,
} sysreg_bank_kind_t;

static sysreg_bank_kind_t sysreg_bank(const vcpu_t *cpu, uint16_t key, size_t *off) {
    uint32_t view;
    switch (key) {
#define BANK_KEYS(field, el1, el2, el12)                                                    \
    case A64_SYSREG_##el1: *off = offsetof(vcpu_bank_t, field); view = 0; break;            \
    case A64_SYSREG_##el2: *off = offsetof(vcpu_bank_t, field); view = 1; break;            \
    case A64_SYSREG_##el12: *off = offsetof(vcpu_bank_t, field); view = 2; break;
    BANK_KEYS(sctlr_el1, SCTLR_EL1, SCTLR_EL2, SCTLR_EL12)
    BANK_KEYS(cpacr_el1, CPACR_EL1, CPTR_EL2, CPACR_EL12)
    BANK_KEYS(tcr_el1, TCR_EL1, TCR_EL2, TCR_EL12)
    BANK_KEYS(ttbr0_el1, TTBR0_EL1, TTBR0_EL2, TTBR0_EL12)
    BANK_KEYS(ttbr1_el1, TTBR1_EL1, TTBR1_EL2, TTBR1_EL12)
    BANK_KEYS(mair_el1, MAIR_EL1, MAIR_EL2, MAIR_EL12)
    BANK_KEYS(vbar_el1, VBAR_EL1, VBAR_EL2, VBAR_EL12)
    BANK_KEYS(esr_el1, ESR_EL1, ESR_EL2, ESR_EL12)
    BANK_KEYS(far_el1, FAR_EL1, FAR_EL2, FAR_EL12)
    BANK_KEYS(elr_el1, ELR_EL1, ELR_EL2, ELR_EL12)
    BANK_KEYS(spsr_el1, SPSR_EL1, SPSR_EL2, SPSR_EL12)
    BANK_KEYS(contextidr_el1, CONTEXTIDR_EL1, CONTEXTIDR_EL2, CONTEXTIDR_EL12)
    BANK_KEYS(cntkctl_el1, CNTKCTL_EL1, CNTHCTL_EL2, CNTKCTL_EL12)
    BANK_KEYS(amair_el1, AMAIR_EL1, AMAIR_EL2, AMAIR_EL12)
    BANK_KEYS(afsr0_el1, AFSR0_EL1, AFSR0_EL2, AFSR0_EL12)
    BANK_KEYS(afsr1_el1, AFSR1_EL1, AFSR1_EL2, AFSR1_EL12)
#undef BANK_KEYS
    default:
        return BANK_NONE;
    }
    static const sysreg_bank_kind_t views[3][3] = {
        [VCPU_WORLD_GUEST] = {BANK_ACTIVE, BANK_UNDEF, BANK_UNDEF},
        [VCPU_WORLD_HOST] = {BANK_ACTIVE, BANK_ACTIVE, BANK_SHADOW},
        [VCPU_WORLD_HOST_NVHE] = {BANK_SHADOW, BANK_ACTIVE, BANK_UNDEF},
    };
    return views[cpu->world][view];
}

static uint64_t *sysreg_bank_reg(vcpu_t *cpu, sysreg_bank_kind_t kind, size_t off) {
    vcpu_bank_t *bank = kind == BANK_ACTIVE ? &cpu->bank : &cpu->el2.shadow;
    return (uint64_t *)((uint8_t *)bank + off);
}

static bool sysreg_bank_write(vcpu_t *cpu, sysreg_bank_kind_t kind, size_t off, uint64_t val) {
    uint64_t *reg = sysreg_bank_reg(cpu, kind, off);
    uint64_t old = *reg;
    if (off == offsetof(vcpu_bank_t, cntkctl_el1) && (kind == BANK_ACTIVE) == (cpu->world != VCPU_WORLD_GUEST)) {
        val &= CNTHCTL_EL2_MASK;
    }
    *reg = val;
    if (kind == BANK_SHADOW) {
        if (off == offsetof(vcpu_bank_t, sctlr_el1) && ((old ^ val) & 1ULL)) {
            mmu_tlbi_all(cpu);
        } else if (off == offsetof(vcpu_bank_t, tcr_el1) && old != val) {
            cpu->exec_epoch++;
            cpu->ras_epoch++;
        }
        return false;
    }
    switch (off) {
    case offsetof(vcpu_bank_t, sctlr_el1):
        if ((old ^ val) & 1ULL) {
            mmu_tlbi_all(cpu);
            mmu_update_asid_salt(cpu);
            return true;
        }
        if ((old ^ val) & (1ULL << 19)) {
            cpu->exec_epoch++;
            cpu->ras_epoch++;
        }
        mmu_update_asid_salt(cpu);
        return false;
    case offsetof(vcpu_bank_t, tcr_el1):
        if (old != val) {
            cpu->exec_epoch++;
            cpu->ras_epoch++;
        }
        mmu_update_asid_salt(cpu);
        return false;
    case offsetof(vcpu_bank_t, cpacr_el1):
    case offsetof(vcpu_bank_t, ttbr0_el1):
    case offsetof(vcpu_bank_t, ttbr1_el1):
        mmu_update_asid_salt(cpu);
        return false;
    default:
        return false;
    }
}

static vcpu_timer_t *sysreg_timer(vcpu_t *cpu, uint32_t op1, uint32_t crn, uint32_t crm, uint32_t op2,
                                  uint32_t *index, bool *undef) {
    *undef = false;
    if (crn != 14U || (crm != 2U && crm != 3U) || op2 > 2U || (op1 != 3U && !cpu->has_el2)) {
        return NULL;
    }
    uint32_t virt = crm == 3U;
    switch (op1) {
    case 3U:
        *index = virt + (cpu->world == VCPU_WORLD_HOST ? VCPU_TIMER_HYP_PHYS : VCPU_TIMER_PHYS);
        break;
    case 4U:
        *undef = cpu->world == VCPU_WORLD_GUEST;
        *index = virt + VCPU_TIMER_HYP_PHYS;
        break;
    case 5U:
        *undef = cpu->world != VCPU_WORLD_HOST;
        *index = virt + VCPU_TIMER_PHYS;
        break;
    default:
        return NULL;
    }
    return &cpu->timers[*index];
}

static uint64_t timer_count(const vcpu_t *cpu, uint32_t index) {
    return vmm_timer_cntvct(cpu->vm) - (index == VCPU_TIMER_VIRT ? cpu->el2.cntvoff : 0);
}

static uint64_t timer_ctl_read(const vcpu_t *cpu, uint32_t index) {
    const vcpu_timer_t *t = &cpu->timers[index];
    bool istatus = ((t->ctl & 1ULL) != 0) && (timer_count(cpu, index) >= t->cval);
    return (t->ctl & 3ULL) | (istatus ? 4ULL : 0ULL);
}

static void sysreg_undef(vcpu_t *cpu, const ir_insn_t *ir, uint32_t word) {
    cpu->pc = cpu_ir_pc(cpu, ir);
    if (cpu->has_el2) {
        vcpu_raise_undef(cpu, cpu->pc);
    } else {
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
    }
}

static uint64_t *sysreg_el2(vcpu_t *cpu, uint16_t key) {
    switch (key) {
    case A64_SYSREG_HCR_EL2: return &cpu->el2.hcr;
    case A64_SYSREG_MDCR_EL2: return &cpu->el2.mdcr;
    case A64_SYSREG_HSTR_EL2: return &cpu->el2.hstr;
    case A64_SYSREG_HACR_EL2: return &cpu->el2.hacr;
    case A64_SYSREG_ACTLR_EL2: return &cpu->el2.actlr;
    case A64_SYSREG_VTTBR_EL2: return &cpu->el2.vttbr;
    case A64_SYSREG_VTCR_EL2: return &cpu->el2.vtcr;
    case A64_SYSREG_VPIDR_EL2: return &cpu->el2.vpidr;
    case A64_SYSREG_VMPIDR_EL2: return &cpu->el2.vmpidr;
    case A64_SYSREG_HPFAR_EL2: return &cpu->el2.hpfar;
    case A64_SYSREG_CNTVOFF_EL2: return &cpu->el2.cntvoff;
    case A64_SYSREG_TPIDR_EL2: return &cpu->tpidr_el2;
    case A64_SYSREG_SP_EL1: return &cpu->el2.shadow_sp;
    default: return NULL;
    }
}

static bool timer_el0_allowed(const vcpu_t *cpu, uint32_t bit) {
    return cpu->el >= 1 || ((cpu->cntkctl_el1 & (1ULL << bit)) != 0);
}

void cpu_op_sysreg_mrs(vcpu_t *cpu, const ir_insn_t *ir,
                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t op0 = (word >> 19) & 3U;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crn = (word >> 12) & 15U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t op2 = (word >> 5) & 7U;
    uint32_t rt = word & 31U;
    uint16_t key = A64_SYSREG_KEY(op0, op1, crn, crm, op2);

    if (key == A64_SYSREG_FPCR || key == A64_SYSREG_FPSR) {
        if (!cpu_fp_enabled(cpu)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_fp_trap(cpu, cpu->pc);
            return;
        }
        if (key == A64_SYSREG_FPCR) {
            cpu_set_x(cpu, rt, cpu->fpcr);
        } else {
            if (cpu->fp_host_active) {
                cpu->fpsr = (cpu->fpsr | cpu_hw_read_fpsr()) & CPU_FPSR_MASK;
            }
            cpu_set_x(cpu, rt, cpu->fpsr);
        }
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }

    if (cpu->el == 0 && op1 != 3U) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
        return;
    }
    if (raise_guest_trap(cpu, ir, word, false)) {
        return;
    }

    uint64_t val = 0;
    size_t off;
    sysreg_bank_kind_t bank = sysreg_bank(cpu, key, &off);
    if (bank != BANK_NONE) {
        if (bank == BANK_UNDEF) {
            sysreg_undef(cpu, ir, word);
            return;
        }
        cpu_set_x(cpu, rt, *sysreg_bank_reg(cpu, bank, off));
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
    uint32_t timer;
    bool undef;
    vcpu_timer_t *t = sysreg_timer(cpu, op1, crn, crm, op2, &timer, &undef);
    if (t) {
        if (undef) {
            sysreg_undef(cpu, ir, word);
            return;
        }
        if (!timer_el0_allowed(cpu, crm == 3U ? 8U : 9U)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        if (op2 == 0U) {
            val = (uint64_t)(uint32_t)(int32_t)(t->cval - timer_count(cpu, timer));
        } else if (op2 == 1U) {
            val = timer_ctl_read(cpu, timer);
        } else {
            val = t->cval;
        }
        cpu_set_x(cpu, rt, val);
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
    if (op1 >= 4U && cpu->has_el2) {
        uint64_t *reg = cpu->world != VCPU_WORLD_GUEST ? sysreg_el2(cpu, key) : NULL;
        if (!reg) {
            sysreg_undef(cpu, ir, word);
            return;
        }
        cpu_set_x(cpu, rt, *reg);
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }

    switch (key) {
    case A64_SYSREG_MIDR_EL1:
        val = cpu->world == VCPU_WORLD_GUEST && cpu->has_el2 ? cpu->el2.vpidr : ID_MIDR_VAL;
        break;
    case A64_SYSREG_MPIDR_EL1:
        val = cpu->world == VCPU_WORLD_GUEST && cpu->has_el2 ? cpu->el2.vmpidr : cpu->mpidr_el1;
        break;
    case A64_SYSREG_REVIDR_EL1: val = 0; break;
    case A64_SYSREG_AIDR_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64PFR0_EL1: val = ID_AA64PFR0_VAL | (cpu->has_el2 ? ID_AA64PFR0_EL2 : 0); break;
    case A64_SYSREG_ID_AA64PFR1_EL1: val = ID_AA64PFR1_VAL; break;
    case A64_SYSREG_ID_AA64DFR0_EL1: val = ID_AA64DFR0_VAL; break;
    case A64_SYSREG_ID_AA64DFR1_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64AFR0_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64AFR1_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64ISAR0_EL1: {
        uint32_t host = cpu_host_features();
        val = ID_AA64ISAR0_VAL | ((host & CPU_HOST_FEAT_SHA512) ? ID_AA64ISAR0_SHA512 : ID_AA64ISAR0_SHA256) |
              ((host & CPU_HOST_FEAT_SHA3) ? ID_AA64ISAR0_SHA3 : 0);
        break;
    }
    case A64_SYSREG_ID_AA64ISAR1_EL1: {
        uint32_t host = cpu_host_features();
        val = ID_AA64ISAR1_VAL | ((host & CPU_HOST_FEAT_FRINTTS) ? ID_AA64ISAR1_FRINTTS : 0) |
              ((host & CPU_HOST_FEAT_BF16) ? ID_AA64ISAR1_BF16 : 0) |
              ((host & CPU_HOST_FEAT_I8MM) ? ID_AA64ISAR1_I8MM : 0);
        break;
    }
    case A64_SYSREG_ID_AA64ISAR2_EL1:
        val = ID_AA64ISAR2_VAL | ((cpu_host_features() & CPU_HOST_FEAT_CSSC) ? ID_AA64ISAR2_CSSC : 0);
        break;
    case A64_SYSREG_ID_AA64MMFR0_EL1: val = ID_AA64MMFR0_VAL | (cpu->has_el2 ? ID_AA64MMFR0_TGRAN_2 : 0); break;
    case A64_SYSREG_ID_AA64MMFR1_EL1: val = ID_AA64MMFR1_VAL | (cpu->has_el2 ? ID_AA64MMFR1_VH : 0); break;
    case A64_SYSREG_ID_AA64MMFR2_EL1:
        val = ID_AA64MMFR2_VAL | ((cpu_host_features() & CPU_HOST_FEAT_LSE2) ? ID_AA64MMFR2_AT : 0) |
              (cpu->has_el2 ? ID_AA64MMFR2_FWB : 0);
        break;

    case A64_SYSREG_ACTLR_EL1: val = cpu->sysregs.actlr_el1; break;
    case A64_SYSREG_PAR_EL1: val = cpu->par_el1; break;
    case A64_SYSREG_TPIDR_EL0: val = cpu->tpidr_el0; break;
    case A64_SYSREG_TPIDRRO_EL0: val = cpu->tpidrro_el0; break;
    case A64_SYSREG_TPIDR_EL1: val = cpu->tpidr_el1; break;

    case A64_SYSREG_CNTFRQ_EL0:
        if (cpu->el == 0 && !(cpu->cntkctl_el1 & 3ULL)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_cntfrq();
        break;
    case A64_SYSREG_CNTPCT_EL0:
    case A64_SYSREG_CNTPCTSS_EL0:
        if (!timer_el0_allowed(cpu, 0)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_cntvct(cpu->vm);
        break;
    case A64_SYSREG_CNTVCT_EL0:
    case A64_SYSREG_CNTVCTSS_EL0:
        if (!timer_el0_allowed(cpu, 1)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_ticks() - cpu->cntvct_base;
        break;

    case A64_SYSREG_SP_EL0: val = cpu->sp_el[0]; break;
    case A64_SYSREG_SPSEL: val = cpu->spsel & 1U; break;
    case A64_SYSREG_CURRENTEL: val = (uint64_t)cpu_current_el(cpu) << 2; break;
    case A64_SYSREG_PAN: val = (uint64_t)(cpu->pan & 1U) << 22; break;
    case A64_SYSREG_UAO: val = cpu->sysregs.uao; break;
    case A64_SYSREG_DAIF:
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 9))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = cpu->daif & 0x3C0U;
        break;
    case A64_SYSREG_NZCV: val = cpu->nzcv & 0xF0000000U; break;

    case A64_SYSREG_CTR_EL0:
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 15))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = CTR_EL0_VAL;
        break;
    case A64_SYSREG_DCZID_EL0:
        val = ((cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 14))) || (cpu->traps & CPU_TRAP_TDZ) != 0)
                  ? (0x10ULL | (uint64_t)VMM_DCZID_BS)
                  : (uint64_t)VMM_DCZID_BS;
        break;
    case A64_SYSREG_CLIDR_EL1: val = CLIDR_EL1_VAL; break;
    case A64_SYSREG_CCSIDR_EL1: val = cpu_ccsidr_val(cpu->sysregs.csselr_el1); break;
    case A64_SYSREG_CSSELR_EL1: val = cpu->sysregs.csselr_el1; break;
    case A64_SYSREG_ISR_EL1: {
        uint32_t irq = atomic_load_explicit(&cpu->irq_pending, memory_order_acquire);
        if (cpu->world == VCPU_WORLD_GUEST && cpu->has_el2) {
            uint64_t hcr = cpu->el2.hcr;
            bool i_pend = (hcr & HCR_EL2_IMO) ? (irq & (VCPU_IRQ_VIRT | VCPU_IRQ_VI)) != 0
                                              : (irq & VCPU_IRQ_PHYS) != 0;
            bool f_pend = (hcr & HCR_EL2_FMO) && (hcr & HCR_EL2_VF);
            bool a_pend = (hcr & HCR_EL2_AMO) && (hcr & HCR_EL2_VSE);
            val = (f_pend ? (1ULL << 6) : 0ULL) | (i_pend ? (1ULL << 7) : 0ULL) |
                  (a_pend ? (1ULL << 8) : 0ULL);
        } else {
            val = (irq & VCPU_IRQ_PHYS) ? (1ULL << 7) : 0ULL;
        }
        break;
    }

    case A64_SYSREG_MDSCR_EL1: val = cpu->mdscr_el1; break;
    case A64_SYSREG_MDCCINT_EL1: val = cpu->sysregs.mdccint_el1; break;
    case A64_SYSREG_MDCCSR_EL0:
    case A64_SYSREG_DBGDTR_EL0:
    case A64_SYSREG_DBGDTRRX_EL0:
        if (cpu->el == 0 && (cpu->mdscr_el1 & (1ULL << 12)) != 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = 0;
        break;
    case A64_SYSREG_OSDLR_EL1: val = cpu->sysregs.osdlr_el1; break;
    case A64_SYSREG_OSLSR_EL1: val = cpu->sysregs.oslsr_el1 | (1ULL << 3); break;
    case A64_SYSREG_OSECCR_EL1:
    case A64_SYSREG_OSDTRRX_EL1:
    case A64_SYSREG_OSDTRTX_EL1:
    case A64_SYSREG_DBGPRCR_EL1:
    case A64_SYSREG_DBGCLAIMSET_EL1:
    case A64_SYSREG_DBGCLAIMCLR_EL1:
    case A64_SYSREG_DBGAUTHSTATUS_EL1:
        val = 0;
        break;
    case A64_SYSREG_PMCR_EL0:
    case A64_SYSREG_PMCNTENSET_EL0:
    case A64_SYSREG_PMCNTENCLR_EL0:
    case A64_SYSREG_PMOVSCLR_EL0:
    case A64_SYSREG_PMSELR_EL0:
    case A64_SYSREG_PMCEID0_EL0:
    case A64_SYSREG_PMCEID1_EL0:
    case A64_SYSREG_PMCCNTR_EL0:
    case A64_SYSREG_PMXEVTYPER_EL0:
    case A64_SYSREG_PMXEVCNTR_EL0:
    case A64_SYSREG_PMUSERENR_EL0:
    case A64_SYSREG_PMINTENSET_EL1:
    case A64_SYSREG_PMINTENCLR_EL1:
    case A64_SYSREG_PMOVSSET_EL0:
        val = 0;
        break;

    default:
        if (op0 == 3U && op1 == 0U && crn == 0U && crm <= 7U) {
            val = 0;
            break;
        }
        if (op0 == 2U && op1 == 0U && crn == 0U && (op2 == 4U || op2 == 5U || op2 == 6U || op2 == 7U)) {
            if (op2 == 4U) val = cpu->sysregs.dbgbvr[crm];
            else if (op2 == 5U) val = cpu->sysregs.dbgbcr[crm];
            else if (op2 == 6U) val = cpu->sysregs.dbgwvr[crm];
            else val = cpu->sysregs.dbgwcr[crm];
            break;
        }
        sysreg_undef(cpu, ir, word);
        return;
    }

    cpu_set_x(cpu, rt, val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_sysreg_msr(vcpu_t *cpu, const ir_insn_t *ir,
                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t op0 = (word >> 19) & 3U;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crn = (word >> 12) & 15U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t op2 = (word >> 5) & 7U;
    uint32_t rt = word & 31U;
    uint16_t key = A64_SYSREG_KEY(op0, op1, crn, crm, op2);
    uint64_t val = cpu_get_x(cpu, rt);

    if (key == A64_SYSREG_FPCR || key == A64_SYSREG_FPSR) {
        if (!cpu_fp_enabled(cpu)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_fp_trap(cpu, cpu->pc);
            return;
        }
        if (key == A64_SYSREG_FPCR) {
            cpu->fpcr = (uint32_t)val & CPU_FPCR_MASK;
            if (cpu->fp_host_active) {
                cpu_hw_write_fpcr(cpu->fpcr);
            }
        } else {
            cpu->fpsr = (uint32_t)val & CPU_FPSR_MASK;
            if (cpu->fp_host_active) {
                cpu_hw_write_fpsr(cpu->fpsr);
            }
        }
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }

    if (cpu->el == 0 && op1 != 3U) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
        return;
    }
    if (raise_guest_trap(cpu, ir, word, false)) {
        return;
    }

    bool exit_loop = false;
    size_t off;
    sysreg_bank_kind_t bank = sysreg_bank(cpu, key, &off);
    uint32_t timer;
    bool undef;
    vcpu_timer_t *t = sysreg_timer(cpu, op1, crn, crm, op2, &timer, &undef);
    if (bank == BANK_UNDEF || (t && undef)) {
        sysreg_undef(cpu, ir, word);
        return;
    }
    if (bank != BANK_NONE) {
        exit_loop = sysreg_bank_write(cpu, bank, off, val);
    } else if (t) {
        if (!timer_el0_allowed(cpu, crm == 3U ? 8U : 9U)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        if (op2 == 0U) {
            vmm_timer_write(cpu, &t->cval, timer_count(cpu, timer) + (uint64_t)(int64_t)(int32_t)val);
        } else if (op2 == 1U) {
            vmm_timer_write(cpu, &t->ctl, val & 3ULL);
        } else {
            vmm_timer_write(cpu, &t->cval, val);
        }
    } else if (op1 >= 4U && cpu->has_el2) {
        uint64_t *reg = cpu->world != VCPU_WORLD_GUEST ? sysreg_el2(cpu, key) : NULL;
        if (!reg) {
            sysreg_undef(cpu, ir, word);
            return;
        }
        if (key == A64_SYSREG_HCR_EL2) {
            cpu->el2.hcr = val | HCR_EL2_RW;
            vcpu_set_world(cpu, (val & HCR_EL2_E2H) != 0 ? VCPU_WORLD_HOST : VCPU_WORLD_HOST_NVHE);
            vcpu_update_irq_gate(cpu);
            exit_loop = true;
        } else if (key == A64_SYSREG_CNTVOFF_EL2) {
            vmm_timer_write(cpu, reg, val);
            vcpu_update_counter_bases(cpu);
        } else {
            *reg = val;
        }
    } else switch (key) {
    case A64_SYSREG_ACTLR_EL1: cpu->sysregs.actlr_el1 = val; break;
    case A64_SYSREG_PAR_EL1: cpu->par_el1 = val; break;
    case A64_SYSREG_TPIDR_EL0: cpu->tpidr_el0 = val; break;
    case A64_SYSREG_TPIDRRO_EL0:
        if (cpu->el == 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->tpidrro_el0 = val;
        break;
    case A64_SYSREG_TPIDR_EL1: cpu->tpidr_el1 = val; break;
    case A64_SYSREG_SP_EL0: cpu->sp_el[0] = val; break;
    case A64_SYSREG_SPSEL: cpu->spsel = (uint8_t)(val & 1U); break;
    case A64_SYSREG_PAN:
        cpu->pan = (uint8_t)((val >> 22) & 1U);
        mmu_update_asid_salt(cpu);
        break;
    case A64_SYSREG_UAO: cpu->sysregs.uao = val & (1ULL << 23); break;
    case A64_SYSREG_DAIF:
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 9))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->daif = (uint32_t)(val & 0x3C0U);
        if (vcpu_irq_deliverable(cpu) != 0) {
            exit_loop = true;
        }
        break;
    case A64_SYSREG_NZCV: cpu->nzcv = (uint32_t)(val & 0xF0000000U); break;
    case A64_SYSREG_CSSELR_EL1: cpu->sysregs.csselr_el1 = val & 0xFULL; break;
    case A64_SYSREG_MDSCR_EL1: cpu->mdscr_el1 = val; break;
    case A64_SYSREG_MDCCINT_EL1: cpu->sysregs.mdccint_el1 = val & (3ULL << 29); break;
    case A64_SYSREG_DBGDTR_EL0:
    case A64_SYSREG_DBGDTRTX_EL0:
        if (cpu->el == 0 && (cpu->mdscr_el1 & (1ULL << 12)) != 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        break;
    case A64_SYSREG_OSDLR_EL1: cpu->sysregs.osdlr_el1 = val & 1ULL; break;
    case A64_SYSREG_OSLAR_EL1: cpu->sysregs.oslsr_el1 = (val & 1ULL) << 1; break;
    case A64_SYSREG_OSECCR_EL1:
    case A64_SYSREG_OSDTRRX_EL1:
    case A64_SYSREG_OSDTRTX_EL1:
    case A64_SYSREG_DBGPRCR_EL1:
    case A64_SYSREG_DBGCLAIMSET_EL1:
    case A64_SYSREG_DBGCLAIMCLR_EL1:
        break;
    case A64_SYSREG_PMCR_EL0:
    case A64_SYSREG_PMCNTENSET_EL0:
    case A64_SYSREG_PMCNTENCLR_EL0:
    case A64_SYSREG_PMOVSCLR_EL0:
    case A64_SYSREG_PMSELR_EL0:
    case A64_SYSREG_PMCCNTR_EL0:
    case A64_SYSREG_PMXEVTYPER_EL0:
    case A64_SYSREG_PMXEVCNTR_EL0:
    case A64_SYSREG_PMUSERENR_EL0:
    case A64_SYSREG_PMINTENSET_EL1:
    case A64_SYSREG_PMINTENCLR_EL1:
    case A64_SYSREG_PMOVSSET_EL0:
        break;

    default:
        if (op0 == 2U && op1 == 0U && crn == 0U && (op2 == 4U || op2 == 5U || op2 == 6U || op2 == 7U)) {
            if (op2 == 4U) cpu->sysregs.dbgbvr[crm] = val;
            else if (op2 == 5U) cpu->sysregs.dbgbcr[crm] = val;
            else if (op2 == 6U) cpu->sysregs.dbgwvr[crm] = val;
            else cpu->sysregs.dbgwcr[crm] = val;
            break;
        }
        sysreg_undef(cpu, ir, word);
        return;
    }

    if (exit_loop) {
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_pstate_msr(vcpu_t *cpu, const ir_insn_t *ir,
                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t op2 = (word >> 5) & 7U;

    if (op1 == 0U && op2 == 5U) {
        if (cpu->el == 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->spsel = (uint8_t)(crm & 1U);
    } else if (op1 == 3U && op2 == 6U) {
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 9))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->daif |= (crm & 0xFU) << 6;
    } else if (op1 == 3U && op2 == 7U) {
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 9))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->daif &= ~((crm & 0xFU) << 6);
        if (vcpu_irq_deliverable(cpu) != 0) {
            cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
            return;
        }
    } else if (op1 == 0U && op2 == 4U) {
        if (cpu->el == 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->pan = (uint8_t)(crm & 1U);
        mmu_update_asid_salt(cpu);
    } else if (op1 == 0U && op2 == 3U) {
        if (cpu->el == 0) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        cpu->sysregs.uao = (uint64_t)(crm & 1U) << 23;
    } else {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_undef(cpu, cpu->pc);
        return;
    }

    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static void swap_bank(vcpu_t *cpu) {
    vcpu_bank_t bank = cpu->bank;
    cpu->bank = cpu->el2.shadow;
    cpu->el2.shadow = bank;
}

static void exec_at(vcpu_t *cpu, uint64_t va, mmu_access_t acc, uint8_t target_el, bool pan, bool el1_regime,
                    bool s12) {
    bool swap = el1_regime && cpu->world != VCPU_WORLD_GUEST;
    uint8_t saved_world = cpu->world;
    uint32_t stages = 0;
    if (el1_regime && (cpu->el2.hcr & HCR_EL2_VM) != 0) {
        stages = s12 ? MMU_STAGE2 : MMU_STAGE2_PTW;
    }
    if (swap) {
        cpu->world = VCPU_WORLD_GUEST;
        swap_bank(cpu);
    }
    uint8_t saved_pan = cpu->pan;
    cpu->pan &= pan;
    mmu_translation_t tr = mmu_translate(cpu, va, acc, target_el, stages);
    cpu->pan = saved_pan;
    if (swap) {
        swap_bank(cpu);
        cpu->world = saved_world;
    }
    if (tr.ok) {
        cpu->par_el1 = (tr.pa & 0x0000FFFFFFFFF000ULL) | (0xFFULL << 56) | (3ULL << 7);
    } else {
        cpu->par_el1 = 1ULL | ((uint64_t)(tr.fault_fsc & 0x3FU) << 1) | ((uint64_t)tr.s1ptw << 8) |
                       ((uint64_t)tr.s2_fault << 9);
    }
}

static vmm_tlbi_t tlbi_page(uint64_t xt, bool all_asids) {
    return (vmm_tlbi_t){
        .va = (uint64_t)((int64_t)(xt << 20) >> 8),
        .pages = 1,
        .asid = (uint16_t)(xt >> 48),
        .all_asids = all_asids,
    };
}

static vmm_tlbi_t tlbi_range(uint64_t xt, bool all_asids) {
    uint32_t tg = (uint32_t)((xt >> 46) & 3U);
    uint32_t tg_shift = tg == 2U ? 14U : (tg == 3U ? 16U : 12U);
    uint32_t scale = (uint32_t)((xt >> 44) & 3U);
    uint32_t num = (uint32_t)((xt >> 39) & 0x1FU);
    return (vmm_tlbi_t){
        .va = (uint64_t)((int64_t)(xt << 27) >> (27U - tg_shift)),
        .pages = (uint64_t)(num + 1U) << (5U * scale + 1U + tg_shift - VMM_PAGE_SHIFT),
        .asid = (uint16_t)(xt >> 48),
        .all_asids = all_asids,
    };
}

static void exec_tlbi(vcpu_t *cpu, uint16_t key, uint64_t xt) {
    vmm_tlbi_target_t el1 = cpu->world == VCPU_WORLD_GUEST || (cpu->el2.hcr & HCR_EL2_TGE) == 0
                                ? VMM_TLBI_GUEST
                                : VMM_TLBI_HOST;
    bool nvhe = (cpu->el2.hcr & HCR_EL2_E2H) == 0;
    bool is = true;
    vmm_tlbi_t op;
    switch (key) {
    case A64_SYSOP_TLBI_VMALLE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_VMALLE1IS:
    case A64_SYSOP_TLBI_VMALLE1OS:
        op = (vmm_tlbi_t){.all_asids = true, .target = el1};
        break;
    case A64_SYSOP_TLBI_ALLE2:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_ALLE2IS:
    case A64_SYSOP_TLBI_ALLE2OS:
        op = (vmm_tlbi_t){.all_asids = true, .target = VMM_TLBI_HOST};
        break;
    case A64_SYSOP_TLBI_ALLE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_ALLE1IS:
    case A64_SYSOP_TLBI_ALLE1OS:
        op = (vmm_tlbi_t){.all_asids = true, .target = VMM_TLBI_GUEST_ALL};
        break;
    case A64_SYSOP_TLBI_VMALLS12E1:
    case A64_SYSOP_TLBI_IPAS2E1:
    case A64_SYSOP_TLBI_IPAS2LE1:
    case A64_SYSOP_TLBI_RIPAS2E1:
    case A64_SYSOP_TLBI_RIPAS2LE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_VMALLS12E1IS:
    case A64_SYSOP_TLBI_VMALLS12E1OS:
    case A64_SYSOP_TLBI_IPAS2E1IS:
    case A64_SYSOP_TLBI_IPAS2E1OS:
    case A64_SYSOP_TLBI_IPAS2LE1IS:
    case A64_SYSOP_TLBI_IPAS2LE1OS:
    case A64_SYSOP_TLBI_RIPAS2E1IS:
    case A64_SYSOP_TLBI_RIPAS2E1OS:
    case A64_SYSOP_TLBI_RIPAS2LE1IS:
    case A64_SYSOP_TLBI_RIPAS2LE1OS:
        op = (vmm_tlbi_t){.all_asids = true, .target = VMM_TLBI_GUEST};
        break;
    case A64_SYSOP_TLBI_VAE2:
    case A64_SYSOP_TLBI_VALE2:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_VAE2IS:
    case A64_SYSOP_TLBI_VAE2OS:
    case A64_SYSOP_TLBI_VALE2IS:
    case A64_SYSOP_TLBI_VALE2OS:
        op = tlbi_page(xt, nvhe);
        op.target = VMM_TLBI_HOST;
        break;
    case A64_SYSOP_TLBI_RVAE2:
    case A64_SYSOP_TLBI_RVALE2:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_RVAE2IS:
    case A64_SYSOP_TLBI_RVAE2OS:
    case A64_SYSOP_TLBI_RVALE2IS:
    case A64_SYSOP_TLBI_RVALE2OS:
        op = tlbi_range(xt, nvhe);
        op.target = VMM_TLBI_HOST;
        break;
    case A64_SYSOP_TLBI_ASIDE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_ASIDE1IS:
    case A64_SYSOP_TLBI_ASIDE1OS:
        op = (vmm_tlbi_t){.asid = (uint16_t)(xt >> 48), .target = el1};
        break;
    case A64_SYSOP_TLBI_VAE1:
    case A64_SYSOP_TLBI_VALE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_VAE1IS:
    case A64_SYSOP_TLBI_VAE1OS:
    case A64_SYSOP_TLBI_VALE1IS:
    case A64_SYSOP_TLBI_VALE1OS:
        op = tlbi_page(xt, false);
        op.target = el1;
        break;
    case A64_SYSOP_TLBI_VAAE1:
    case A64_SYSOP_TLBI_VAALE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_VAAE1IS:
    case A64_SYSOP_TLBI_VAAE1OS:
    case A64_SYSOP_TLBI_VAALE1IS:
    case A64_SYSOP_TLBI_VAALE1OS:
        op = tlbi_page(xt, true);
        op.target = el1;
        break;
    case A64_SYSOP_TLBI_RVAE1:
    case A64_SYSOP_TLBI_RVALE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_RVAE1IS:
    case A64_SYSOP_TLBI_RVAE1OS:
    case A64_SYSOP_TLBI_RVALE1IS:
    case A64_SYSOP_TLBI_RVALE1OS:
        op = tlbi_range(xt, false);
        op.target = el1;
        break;
    case A64_SYSOP_TLBI_RVAAE1:
    case A64_SYSOP_TLBI_RVAALE1:
        is = false;
        [[fallthrough]];
    case A64_SYSOP_TLBI_RVAAE1IS:
    case A64_SYSOP_TLBI_RVAAE1OS:
    case A64_SYSOP_TLBI_RVAALE1IS:
    case A64_SYSOP_TLBI_RVAALE1OS:
        op = tlbi_range(xt, true);
        op.target = el1;
        break;
    default:
        return;
    }
    op.vmid = cpu_vmid(cpu);
    mmu_tlbi(cpu, op, is || (cpu->world == VCPU_WORLD_GUEST && (cpu->el2.hcr & HCR_EL2_FB) != 0));
}

void cpu_op_sys_insn(vcpu_t *cpu, const ir_insn_t *ir,
                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crn = (word >> 12) & 15U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t op2 = (word >> 5) & 7U;
    uint32_t rt = word & 31U;
    uint16_t key = A64_SYSOP_KEY(op1, crn, crm, op2);
    uint64_t xt = cpu_get_x(cpu, rt);

    if (cpu->el == 0 && op1 != 3U) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
        return;
    }
    if (op1 >= 4U && cpu->world == VCPU_WORLD_GUEST && cpu->has_el2) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_undef(cpu, cpu->pc);
        return;
    }
    if (raise_guest_trap(cpu, ir, word, true)) {
        return;
    }

    bool el1_regime = cpu->world == VCPU_WORLD_HOST_NVHE || (cpu->el2.hcr & HCR_EL2_TGE) == 0;
    switch (key) {
    case A64_SYSOP_DC_ZVA: {
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 14))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        const uint64_t zva_size = 4ULL << VMM_DCZID_BS;
        uint64_t base = xt & ~(zva_size - 1ULL);
        uintptr_t hva;
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, base, &hva), 1)) {
            memset((void *)hva, 0, (size_t)zva_size);
        } else {
            uint64_t elr = cpu_ir_pc(cpu, ir);
            mmu_translation_t tr;
            bool is_dev = false;
            if (!mmu_translate_and_fill(cpu, base, MMU_ACCESS_WRITE, cpu->el, &tr, &is_dev)) {
                vcpu_raise_data_abort(cpu, xt, &tr, true, elr);
                return;
            }
            if (is_dev) {
                vcpu_raise_data_abort(cpu, xt, NULL, true, elr);
                return;
            }
            if (tr.is_mmio) {
                cpu_fp_leave_guest(cpu);
                for (uint64_t off = 0; off < zva_size; off += 8ULL) {
                    vmm_mmio_write(cpu->vm, cpu, tr.pa + off, 8, 0);
                }
            } else {
                memset((void *)tr.hva, 0, (size_t)zva_size);
            }
        }
        break;
    }
    case A64_SYSOP_DC_CVAC:
    case A64_SYSOP_DC_CVAU:
    case A64_SYSOP_DC_CIVAC:
    case A64_SYSOP_DC_CVAP:
    case A64_SYSOP_DC_CVADP:
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 26))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        break;
    case A64_SYSOP_DC_IVAC:
    case A64_SYSOP_DC_ISW:
    case A64_SYSOP_DC_CSW:
    case A64_SYSOP_DC_CISW:
        break;

    case A64_SYSOP_IC_IALLUIS:
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        vcpu_flush_icache_all(cpu);
        cpu->icache_shootdown_all = true;
        mmu_icache_broadcast(cpu);
        return;
    case A64_SYSOP_IC_IALLU:
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        cpu->ic_ivau_last_va_page = UINT64_MAX;
        vcpu_flush_icache_all(cpu);
        return;
    case A64_SYSOP_IC_IVAU: {
        if (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 26))) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        uint64_t va_page = xt & ~(uint64_t)VMM_PAGE_MASK;
        uint64_t cur_va = ((cpu_icache_t *)cpu->icache)->cur_page->va_page;
        uint64_t next_pc = cpu_ir_pc(cpu, ir) + 4ULL;
        bool queued = vcpu_flush_icache_va(cpu, xt);
        if (va_page == cur_va) {
            cpu->pc = next_pc;
            return;
        }
        if (queued) {
            cpu->ic_ivau_last_va_page = va_page;
        }
        break;
    }

    case A64_SYSOP_AT_S1E1R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 1, false, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E1RP:
        exec_at(cpu, xt, MMU_ACCESS_READ, 1, true, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E1W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 1, false, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E1WP:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 1, true, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E0R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 0, false, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E0W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 0, false, el1_regime, false);
        break;
    case A64_SYSOP_AT_S1E2R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 1, false, false, false);
        break;
    case A64_SYSOP_AT_S1E2W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 1, false, false, false);
        break;
    case A64_SYSOP_AT_S12E1R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 1, false, true, true);
        break;
    case A64_SYSOP_AT_S12E1W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 1, false, true, true);
        break;
    case A64_SYSOP_AT_S12E0R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 0, false, true, true);
        break;
    case A64_SYSOP_AT_S12E0W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 0, false, true, true);
        break;

    default:
        exec_tlbi(cpu, key, xt);
        break;
    }

    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_svc(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    uint32_t imm16 = (uint32_t)ir->target_or_imm & 0xFFFFU;
    uint64_t next_pc = cpu_ir_pc(cpu, ir) + 4ULL;
    vcpu_raise_exception(cpu, (0x15U << 26) | (1U << 25) | imm16, 0, next_pc);
}

void cpu_op_hvc(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t cur_pc = cpu_ir_pc(cpu, ir);
    if (cpu->el == 0) {
        cpu->pc = cur_pc;
        if (ir->target_or_imm == 0x5357 && cpu->world == (cpu->has_el2 ? VCPU_WORLD_HOST : VCPU_WORLD_GUEST)) {
            cpu_fp_leave_guest(cpu);
            if (vmm_preboot_trap(cpu)) {
                return;
            }
        }
        vcpu_raise_undef(cpu, cur_pc);
        return;
    }
    if (cpu->has_el2) {
        if ((cpu->el2.hcr & HCR_EL2_HCD) != 0) {
            vcpu_raise_undef(cpu, cur_pc);
            return;
        }
        uint32_t imm16 = (uint32_t)ir->target_or_imm & 0xFFFFU;
        vcpu_raise_exception_el2(cpu, (0x16U << 26) | (1U << 25) | imm16, 0, cur_pc + 4ULL);
        return;
    }
    cpu->pc = cur_pc + 4ULL;
    cpu_fp_leave_guest(cpu);
    if (!vmm_handle_psci(cpu)) {
        cpu->x[0] = (uint64_t)PSCI_RET_NOT_SUPPORTED;
    }
    if (atomic_load_explicit(&cpu->stop_requested, memory_order_relaxed)) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_smc(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t cur_pc = cpu_ir_pc(cpu, ir);
    if (!cpu->has_el2 || cpu->el == 0) {
        vcpu_raise_undef(cpu, cur_pc);
        return;
    }
    if (cpu->world == VCPU_WORLD_GUEST && (cpu->el2.hcr & HCR_EL2_TSC) != 0) {
        uint32_t imm16 = (uint32_t)ir->target_or_imm & 0xFFFFU;
        vcpu_raise_exception_el2(cpu, (0x17U << 26) | (1U << 25) | imm16, 0, cur_pc);
        return;
    }
    cpu->pc = cur_pc + 4ULL;
    cpu_fp_leave_guest(cpu);
    if (!vmm_handle_psci(cpu)) {
        cpu->x[0] = (uint64_t)PSCI_RET_NOT_SUPPORTED;
    }
    if (atomic_load_explicit(&cpu->stop_requested, memory_order_relaxed)) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_brk(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    uint32_t imm16 = (uint32_t)ir->target_or_imm & 0xFFFFU;
    uint64_t cur_pc = cpu_ir_pc(cpu, ir);
    vcpu_raise_exception(cpu, (0x3CU << 26) | (1U << 25) | imm16, 0, cur_pc);
}

void cpu_op_hlt(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    cpu->pc = cpu_ir_pc(cpu, ir);
    cpu->stop_reason = VCPU_STOP_HALT;
    atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
}

void cpu_op_eret(vcpu_t *cpu, const ir_insn_t *ir,
                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    if (cpu->el == 0) {
        uint64_t cur_pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_undef(cpu, cur_pc);
        return;
    }
    vcpu_eret(cpu);
}

void cpu_op_wfi(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    if ((cpu->traps & CPU_TRAP_TWI) != 0) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_exception_el2(cpu, (0x01U << 26) | (1U << 25) | (1U << 24) | (0xEU << 20), 0, cpu->pc);
        return;
    }
    cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
    cpu_fp_leave_guest(cpu);
    vmm_vcpu_wfi(cpu);
}

void cpu_op_wfe(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (atomic_exchange_explicit(&cpu->event_reg, false, memory_order_acq_rel)) {
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
    if ((cpu->traps & CPU_TRAP_TWE) != 0) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_exception_el2(cpu, (0x01U << 26) | (1U << 25) | (1U << 24) | (0xEU << 20) | 1U, 0, cpu->pc);
        return;
    }
    if (vcpu_check_interrupts(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        return;
    }
    cpu_fp_leave_guest(cpu);
    vmm_vcpu_wfe(cpu);
    if (vcpu_check_interrupts(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_sev(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (cpu->vm) {
        vmm_vcpu_sev(cpu->vm);
    } else {
        atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_sevl(vcpu_t *cpu, const ir_insn_t *ir,
                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_barrier(vcpu_t *cpu, const ir_insn_t *ir,
                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    atomic_thread_fence(memory_order_seq_cst);
    if (__builtin_expect(cpu->icache_shootdown_pending, 0)) {
        mmu_icache_broadcast(cpu);
    }
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (__builtin_expect(ic->cur_exec_epoch != cpu->exec_epoch, 0)) {
        cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_clrex(vcpu_t *cpu, const ir_insn_t *ir,
                  softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu->excl_valid = false;
    cpu->excl_hva = 0;
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}
