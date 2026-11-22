#include "cpu_internal.h"
#include "cpu_fast_layout.h"
#include "generated/a64_sysreg_gen.inc"

enum : uint64_t {
    ID_MIDR_VAL = 0x410FD050ULL,
    ID_AA64PFR0_VAL = 0x0000000000000011ULL,
    ID_AA64PFR1_VAL = 0x0000000000000000ULL,
    ID_AA64DFR0_VAL = 0x0000000000101006ULL,
    ID_AA64ISAR0_VAL = (2ULL << 4) | (1ULL << 8) | (1ULL << 12) |
                       (1ULL << 16) | (2ULL << 20) | (1ULL << 28) |
                       (2ULL << 56),
    ID_AA64ISAR1_VAL = (1ULL << 12) | (1ULL << 20),
    ID_AA64ISAR2_VAL = (1ULL << 16),
    ID_AA64MMFR0_VAL = 0x000000000F100022ULL,
    ID_AA64MMFR1_VAL = (1ULL << 16) | (1ULL << 20),
    ID_AA64MMFR2_VAL = 0x0000000000000000ULL,
    CTR_EL0_VAL = 0x9444C004ULL,
    CLIDR_EL1_VAL = 0x0A200023ULL,
    CCSIDR_EL1_VAL = 0x700FE01AULL,
};

bool vcpu_check_interrupts(vcpu_t *cpu) {
    bool icache_flushed = mmu_poll_shootdown(cpu);
    if (icache_flushed) {
        return true;
    }
    if (atomic_load_explicit(&cpu->stop_requested, memory_order_relaxed)) {
        return true;
    }
    if ((cpu->daif & (1U << 7)) == 0 &&
        atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) != 0) {
        return true;
    }
    return false;
}

static uint64_t timer_ctl_read(const vcpu_t *cpu, uint64_t ctl, uint64_t cval) {
    bool istatus = ((ctl & 1ULL) != 0) && (vmm_timer_cntvct(cpu->vm) >= cval);
    return (ctl & 3ULL) | (istatus ? 4ULL : 0ULL);
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

    uint64_t val = 0;
    switch (key) {
    case A64_SYSREG_MIDR_EL1: val = ID_MIDR_VAL; break;
    case A64_SYSREG_MPIDR_EL1: val = cpu->mpidr_el1; break;
    case A64_SYSREG_REVIDR_EL1: val = 0; break;
    case A64_SYSREG_AIDR_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64PFR0_EL1: val = ID_AA64PFR0_VAL; break;
    case A64_SYSREG_ID_AA64PFR1_EL1: val = ID_AA64PFR1_VAL; break;
    case A64_SYSREG_ID_AA64DFR0_EL1: val = ID_AA64DFR0_VAL; break;
    case A64_SYSREG_ID_AA64DFR1_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64AFR0_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64AFR1_EL1: val = 0; break;
    case A64_SYSREG_ID_AA64ISAR0_EL1: val = ID_AA64ISAR0_VAL; break;
    case A64_SYSREG_ID_AA64ISAR1_EL1: val = ID_AA64ISAR1_VAL; break;
    case A64_SYSREG_ID_AA64ISAR2_EL1: val = ID_AA64ISAR2_VAL; break;
    case A64_SYSREG_ID_AA64MMFR0_EL1: val = ID_AA64MMFR0_VAL; break;
    case A64_SYSREG_ID_AA64MMFR1_EL1: val = ID_AA64MMFR1_VAL; break;
    case A64_SYSREG_ID_AA64MMFR2_EL1: val = ID_AA64MMFR2_VAL; break;

    case A64_SYSREG_SCTLR_EL1: val = cpu->sctlr_el1; break;
    case A64_SYSREG_ACTLR_EL1: val = cpu->sysregs.actlr_el1; break;
    case A64_SYSREG_CPACR_EL1: val = cpu->cpacr_el1; break;
    case A64_SYSREG_TTBR0_EL1: val = cpu->ttbr0_el1; break;
    case A64_SYSREG_TTBR1_EL1: val = cpu->ttbr1_el1; break;
    case A64_SYSREG_TCR_EL1: val = cpu->tcr_el1; break;
    case A64_SYSREG_AFSR0_EL1: val = cpu->sysregs.afsr0_el1; break;
    case A64_SYSREG_AFSR1_EL1: val = cpu->sysregs.afsr1_el1; break;
    case A64_SYSREG_ESR_EL1: val = cpu->esr_el1; break;
    case A64_SYSREG_FAR_EL1: val = cpu->far_el1; break;
    case A64_SYSREG_PAR_EL1: val = cpu->par_el1; break;
    case A64_SYSREG_MAIR_EL1: val = cpu->mair_el1; break;
    case A64_SYSREG_AMAIR_EL1: val = cpu->sysregs.amair_el1; break;
    case A64_SYSREG_VBAR_EL1: val = cpu->vbar_el1; break;
    case A64_SYSREG_CONTEXTIDR_EL1: val = cpu->contextidr_el1; break;
    case A64_SYSREG_TPIDR_EL0: val = cpu->tpidr_el0; break;
    case A64_SYSREG_TPIDRRO_EL0: val = cpu->tpidrro_el0; break;
    case A64_SYSREG_TPIDR_EL1: val = cpu->tpidr_el1; break;
    case A64_SYSREG_CNTKCTL_EL1: val = cpu->cntkctl_el1; break;

    case A64_SYSREG_CNTFRQ_EL0:
        if (cpu->el == 0 && !(cpu->cntkctl_el1 & 3ULL)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_cntfrq();
        break;
    case A64_SYSREG_CNTPCT_EL0:
        if (!timer_el0_allowed(cpu, 0)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_cntvct(cpu->vm);
        break;
    case A64_SYSREG_CNTVCT_EL0:
        if (!timer_el0_allowed(cpu, 1)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = vmm_timer_cntvct(cpu->vm);
        break;
    case A64_SYSREG_CNTP_TVAL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = (uint64_t)(uint32_t)(int32_t)(cpu->cntp_cval_el0 - vmm_timer_cntvct(cpu->vm));
        break;
    case A64_SYSREG_CNTP_CTL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = timer_ctl_read(cpu, cpu->cntp_ctl_el0, cpu->cntp_cval_el0);
        break;
    case A64_SYSREG_CNTP_CVAL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = cpu->cntp_cval_el0;
        break;
    case A64_SYSREG_CNTV_TVAL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = (uint64_t)(uint32_t)(int32_t)(cpu->cntv_cval_el0 - vmm_timer_cntvct(cpu->vm));
        break;
    case A64_SYSREG_CNTV_CTL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = timer_ctl_read(cpu, cpu->cntv_ctl_el0, cpu->cntv_cval_el0);
        break;
    case A64_SYSREG_CNTV_CVAL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        val = cpu->cntv_cval_el0;
        break;

    case A64_SYSREG_SPSR_EL1: val = cpu->spsr_el1; break;
    case A64_SYSREG_ELR_EL1: val = cpu->elr_el1; break;
    case A64_SYSREG_SP_EL0: val = cpu->sp_el[0]; break;
    case A64_SYSREG_SPSEL: val = cpu->spsel & 1U; break;
    case A64_SYSREG_CURRENTEL: val = (uint64_t)cpu->el << 2; break;
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
        val = (cpu->el == 0 && !(cpu->sctlr_el1 & (1ULL << 14)))
                  ? (0x10ULL | (uint64_t)VMM_DCZID_BS)
                  : (uint64_t)VMM_DCZID_BS;
        break;
    case A64_SYSREG_CLIDR_EL1: val = CLIDR_EL1_VAL; break;
    case A64_SYSREG_CCSIDR_EL1: val = CCSIDR_EL1_VAL; break;
    case A64_SYSREG_CSSELR_EL1: val = cpu->sysregs.csselr_el1; break;

    case A64_SYSREG_MDSCR_EL1: val = cpu->mdscr_el1; break;
    case A64_SYSREG_MDCCSR_EL0: val = 0; break;
    case A64_SYSREG_OSDLR_EL1: val = cpu->sysregs.osdlr_el1; break;
    case A64_SYSREG_OSLSR_EL1: val = cpu->sysregs.oslsr_el1 | (1ULL << 3); break;
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
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
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
            cpu->fpcr = (uint32_t)val & CPU_FPCR_ALL_MASK;
            if (cpu->fp_host_active) {
                cpu_hw_write_fpcr(cpu->fpcr & CPU_FPCR_HW_MASK);
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

    bool exit_loop = false;
    switch (key) {
    case A64_SYSREG_SCTLR_EL1: {
        uint64_t old = cpu->sctlr_el1;
        cpu->sctlr_el1 = val;
        if ((old ^ val) & 1ULL) {
            mmu_tlbi_all(cpu, false);
            exit_loop = true;
        } else {
            if ((old ^ val) & (1ULL << 19)) {
                cpu->exec_epoch++;
                cpu->ras_epoch++;
            }
            mmu_update_asid_salt(cpu);
        }
        break;
    }
    case A64_SYSREG_ACTLR_EL1: cpu->sysregs.actlr_el1 = val; break;
    case A64_SYSREG_CPACR_EL1:
        cpu->cpacr_el1 = val;
        mmu_update_asid_salt(cpu);
        break;
    case A64_SYSREG_TTBR0_EL1:
        cpu->ttbr0_el1 = val;
        mmu_update_asid_salt(cpu);
        break;
    case A64_SYSREG_TTBR1_EL1:
        cpu->ttbr1_el1 = val;
        mmu_update_asid_salt(cpu);
        break;
    case A64_SYSREG_TCR_EL1:
        if (cpu->tcr_el1 != val) {
            cpu->tcr_el1 = val;
            cpu->exec_epoch++;
            cpu->ras_epoch++;
        }
        mmu_update_asid_salt(cpu);
        break;
    case A64_SYSREG_AFSR0_EL1: cpu->sysregs.afsr0_el1 = val; break;
    case A64_SYSREG_AFSR1_EL1: cpu->sysregs.afsr1_el1 = val; break;
    case A64_SYSREG_ESR_EL1: cpu->esr_el1 = val; break;
    case A64_SYSREG_FAR_EL1: cpu->far_el1 = val; break;
    case A64_SYSREG_PAR_EL1: cpu->par_el1 = val; break;
    case A64_SYSREG_MAIR_EL1: cpu->mair_el1 = val; break;
    case A64_SYSREG_AMAIR_EL1: cpu->sysregs.amair_el1 = val; break;
    case A64_SYSREG_VBAR_EL1: cpu->vbar_el1 = val; break;
    case A64_SYSREG_CONTEXTIDR_EL1: cpu->contextidr_el1 = val; break;
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
    case A64_SYSREG_CNTKCTL_EL1: cpu->cntkctl_el1 = val; break;

    case A64_SYSREG_CNTP_TVAL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntp_cval_el0,
                        vmm_timer_cntvct(cpu->vm) + (uint64_t)(int64_t)(int32_t)val);
        break;
    case A64_SYSREG_CNTP_CTL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntp_ctl_el0, val & 3ULL);
        break;
    case A64_SYSREG_CNTP_CVAL_EL0:
        if (!timer_el0_allowed(cpu, 9)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntp_cval_el0, val);
        break;
    case A64_SYSREG_CNTV_TVAL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntv_cval_el0,
                        vmm_timer_cntvct(cpu->vm) + (uint64_t)(int64_t)(int32_t)val);
        break;
    case A64_SYSREG_CNTV_CTL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntv_ctl_el0, val & 3ULL);
        break;
    case A64_SYSREG_CNTV_CVAL_EL0:
        if (!timer_el0_allowed(cpu, 8)) {
            cpu->pc = cpu_ir_pc(cpu, ir);
            vcpu_raise_sys_trap(cpu, word, cpu->pc);
            return;
        }
        vmm_timer_write(cpu, &cpu->cntv_cval_el0, val);
        break;

    case A64_SYSREG_SPSR_EL1: cpu->spsr_el1 = val; break;
    case A64_SYSREG_ELR_EL1: cpu->elr_el1 = val; break;
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
        if ((cpu->daif & (1U << 7)) == 0 &&
            atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) != 0) {
            exit_loop = true;
        }
        break;
    case A64_SYSREG_NZCV: cpu->nzcv = (uint32_t)(val & 0xF0000000U); break;
    case A64_SYSREG_CSSELR_EL1: cpu->sysregs.csselr_el1 = val & 0xFULL; break;
    case A64_SYSREG_MDSCR_EL1: cpu->mdscr_el1 = val; break;
    case A64_SYSREG_OSDLR_EL1: cpu->sysregs.osdlr_el1 = val & 1ULL; break;
    case A64_SYSREG_OSLAR_EL1: cpu->sysregs.oslsr_el1 = (val & 1ULL) << 1; break;
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
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_sys_trap(cpu, word, cpu->pc);
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
        if ((cpu->daif & (1U << 7)) == 0 &&
            atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) != 0) {
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

static void exec_at(vcpu_t *cpu, uint64_t va, mmu_access_t acc, uint8_t target_el) {
    mmu_translation_t tr = mmu_translate(cpu, va, acc, target_el);
    if (tr.ok) {
        cpu->par_el1 = (tr.pa & 0x0000FFFFFFFFF000ULL) | (0xFFULL << 56) | (3ULL << 7);
    } else {
        cpu->par_el1 = 1ULL | ((uint64_t)(tr.fault_fsc & 0x3FU) << 1);
    }
}

static void exec_tlbi_range(vcpu_t *cpu, uint64_t xt, bool all_asids, bool inner_shareable) {
    uint32_t tg = (uint32_t)((xt >> 46) & 3U);
    uint8_t tg_shift = tg == 2U ? 14U : (tg == 3U ? 16U : 12U);
    uint32_t scale = (uint32_t)((xt >> 44) & 3U);
    uint32_t num = (uint32_t)((xt >> 39) & 0x1FU);
    uint64_t num_pages = (uint64_t)(num + 1U) << (5U * scale + 1U);
    uint64_t start_va = (uint64_t)((int64_t)(xt << 27) >> (27U - tg_shift));
    uint16_t asid = (uint16_t)(xt >> 48);
    mmu_tlbi_range(cpu, start_va, num_pages, tg_shift, asid, all_asids, inner_shareable);
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
                vcpu_raise_data_abort(cpu, xt, tr.fault_fsc, true, elr);
                return;
            }
            if (is_dev) {
                vcpu_raise_data_abort(cpu, xt, 0x21U, true, elr);
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
    case A64_SYSOP_AT_S1E1RP:
        exec_at(cpu, xt, MMU_ACCESS_READ, 1);
        break;
    case A64_SYSOP_AT_S1E1W:
    case A64_SYSOP_AT_S1E1WP:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 1);
        break;
    case A64_SYSOP_AT_S1E0R:
        exec_at(cpu, xt, MMU_ACCESS_READ, 0);
        break;
    case A64_SYSOP_AT_S1E0W:
        exec_at(cpu, xt, MMU_ACCESS_WRITE, 0);
        break;

    case A64_SYSOP_TLBI_VMALLE1:
        mmu_tlbi_all(cpu, false);
        break;
    case A64_SYSOP_TLBI_VMALLE1IS:
    case A64_SYSOP_TLBI_VMALLE1OS:
        mmu_tlbi_all(cpu, true);
        break;
    case A64_SYSOP_TLBI_ASIDE1:
        mmu_tlbi_asid(cpu, (uint16_t)(xt >> 48), false);
        break;
    case A64_SYSOP_TLBI_ASIDE1IS:
    case A64_SYSOP_TLBI_ASIDE1OS:
        mmu_tlbi_asid(cpu, (uint16_t)(xt >> 48), true);
        break;
    case A64_SYSOP_TLBI_VAE1:
    case A64_SYSOP_TLBI_VALE1: {
        uint64_t va = (uint64_t)((int64_t)(xt << 20) >> 8);
        mmu_tlbi_va(cpu, va, (uint16_t)(xt >> 48), false, false);
        break;
    }
    case A64_SYSOP_TLBI_VAE1IS:
    case A64_SYSOP_TLBI_VAE1OS:
    case A64_SYSOP_TLBI_VALE1IS:
    case A64_SYSOP_TLBI_VALE1OS: {
        uint64_t va = (uint64_t)((int64_t)(xt << 20) >> 8);
        mmu_tlbi_va(cpu, va, (uint16_t)(xt >> 48), false, true);
        break;
    }
    case A64_SYSOP_TLBI_VAAE1:
    case A64_SYSOP_TLBI_VAALE1: {
        uint64_t va = (uint64_t)((int64_t)(xt << 20) >> 8);
        mmu_tlbi_va(cpu, va, 0, true, false);
        break;
    }
    case A64_SYSOP_TLBI_VAAE1IS:
    case A64_SYSOP_TLBI_VAAE1OS:
    case A64_SYSOP_TLBI_VAALE1IS:
    case A64_SYSOP_TLBI_VAALE1OS: {
        uint64_t va = (uint64_t)((int64_t)(xt << 20) >> 8);
        mmu_tlbi_va(cpu, va, 0, true, true);
        break;
    }
    case A64_SYSOP_TLBI_RVAE1:
    case A64_SYSOP_TLBI_RVALE1:
        exec_tlbi_range(cpu, xt, false, false);
        break;
    case A64_SYSOP_TLBI_RVAE1IS:
    case A64_SYSOP_TLBI_RVAE1OS:
    case A64_SYSOP_TLBI_RVALE1IS:
    case A64_SYSOP_TLBI_RVALE1OS:
        exec_tlbi_range(cpu, xt, false, true);
        break;
    case A64_SYSOP_TLBI_RVAAE1:
    case A64_SYSOP_TLBI_RVAALE1:
        exec_tlbi_range(cpu, xt, true, false);
        break;
    case A64_SYSOP_TLBI_RVAAE1IS:
    case A64_SYSOP_TLBI_RVAAE1OS:
    case A64_SYSOP_TLBI_RVAALE1IS:
    case A64_SYSOP_TLBI_RVAALE1OS:
        exec_tlbi_range(cpu, xt, true, true);
        break;

    default:
        break;
    }

    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static vcpu_t *find_vcpu_by_mpidr(vmm_machine_t *vm, uint64_t mpidr) {
    if (!vm) {
        return nullptr;
    }
    uint64_t aff = mpidr & 0x00FFFFFFULL;
    for (uint32_t i = 0; i < vm->cpu_count && i < VMM_MAX_VCPUS; ++i) {
        vcpu_t *c = vm->cpus[i];
        if (c && (c->mpidr_el1 & 0x00FFFFFFULL) == aff) {
            return c;
        }
    }
    return nullptr;
}

void vcpu_handle_hvc(vcpu_t *cpu) {
    uint32_t func_id = (uint32_t)cpu->x[0];
    if (mmu_handle_pv_hvc(cpu, func_id)) {
        return;
    }
    if (vmm_handle_psci(cpu)) {
        return;
    }

    switch (func_id) {
    case SMCCC_VERSION:
        cpu->x[0] = 0x00010001ULL;
        return;
    case SMCCC_ARCH_FEATURES:
        cpu->x[0] = ((uint32_t)cpu->x[1] == SMCCC_VERSION) ? 0ULL : (uint64_t)PSCI_RET_NOT_SUPPORTED;
        return;
    case PSCI_FN_VERSION:
        cpu->x[0] = PSCI_VERSION_1_1;
        return;
    case PSCI_FN_FEATURES:
        switch ((uint32_t)cpu->x[1]) {
        case SMCCC_VERSION:
        case PSCI_FN_VERSION:
        case PSCI_FN_CPU_SUSPEND:
        case PSCI_FN64_CPU_SUSPEND:
        case PSCI_FN_CPU_OFF:
        case PSCI_FN_CPU_ON:
        case PSCI_FN64_CPU_ON:
        case PSCI_FN_AFFINITY_INFO:
        case PSCI_FN64_AFFINITY_INFO:
        case PSCI_FN_MIGRATE_INFO_TYPE:
        case PSCI_FN_SYSTEM_OFF:
        case PSCI_FN_SYSTEM_RESET:
        case PSCI_FN_SYSTEM_RESET2:
        case PSCI_FN64_SYSTEM_RESET2:
        case PSCI_FN_FEATURES:
            cpu->x[0] = 0;
            return;
        default:
            cpu->x[0] = (uint64_t)PSCI_RET_NOT_SUPPORTED;
            return;
        }
    case PSCI_FN_MIGRATE_INFO_TYPE:
        cpu->x[0] = 2;
        return;
    case PSCI_FN_CPU_SUSPEND:
    case PSCI_FN64_CPU_SUSPEND:
        cpu->x[0] = PSCI_RET_SUCCESS;
        return;
    case PSCI_FN_CPU_OFF:
        cpu->stop_reason = VCPU_STOP_HALT;
        atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
        cpu->x[0] = PSCI_RET_SUCCESS;
        return;
    case PSCI_FN_CPU_ON:
    case PSCI_FN64_CPU_ON: {
        vcpu_t *target = find_vcpu_by_mpidr(cpu->vm, cpu->x[1]);
        if (!target) {
            cpu->x[0] = (uint64_t)PSCI_RET_INVALID_PARAMS;
            return;
        }
        if (target == cpu || !atomic_load_explicit(&target->halted_wfi, memory_order_acquire)) {
            cpu->x[0] = (uint64_t)PSCI_RET_ALREADY_ON;
            return;
        }
        vcpu_reset(target, cpu->x[2], cpu->x[3]);
        atomic_store_explicit(&target->halted_wfi, false, memory_order_release);
        cpu->x[0] = PSCI_RET_SUCCESS;
        return;
    }
    case PSCI_FN_AFFINITY_INFO:
    case PSCI_FN64_AFFINITY_INFO: {
        vcpu_t *target = find_vcpu_by_mpidr(cpu->vm, cpu->x[1]);
        if (!target) {
            cpu->x[0] = (uint64_t)PSCI_RET_INVALID_PARAMS;
            return;
        }
        bool off = atomic_load_explicit(&target->halted_wfi, memory_order_acquire) && target->pc == 0;
        cpu->x[0] = off ? 1ULL : 0ULL;
        return;
    }
    case PSCI_FN_SYSTEM_OFF:
        cpu->stop_reason = VCPU_STOP_HALT;
        if (cpu->vm) {
            atomic_store_explicit(&cpu->vm->running, false, memory_order_release);
            for (uint32_t i = 0; i < cpu->vm->cpu_count && i < VMM_MAX_VCPUS; ++i) {
                if (cpu->vm->cpus[i]) {
                    cpu->vm->cpus[i]->stop_reason = VCPU_STOP_HALT;
                    atomic_store_explicit(&cpu->vm->cpus[i]->stop_requested, true, memory_order_release);
                }
            }
        } else {
            atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
        }
        cpu->x[0] = PSCI_RET_SUCCESS;
        return;
    case PSCI_FN_SYSTEM_RESET:
    case PSCI_FN_SYSTEM_RESET2:
    case PSCI_FN64_SYSTEM_RESET2:
        cpu->stop_reason = VCPU_STOP_REBOOT;
        if (cpu->vm) {
            atomic_store_explicit(&cpu->vm->running, false, memory_order_release);
            for (uint32_t i = 0; i < cpu->vm->cpu_count && i < VMM_MAX_VCPUS; ++i) {
                if (cpu->vm->cpus[i]) {
                    cpu->vm->cpus[i]->stop_reason = VCPU_STOP_REBOOT;
                    atomic_store_explicit(&cpu->vm->cpus[i]->stop_requested, true, memory_order_release);
                }
            }
        } else {
            atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
        }
        cpu->x[0] = PSCI_RET_SUCCESS;
        return;
    default:
        cpu->x[0] = (uint64_t)PSCI_RET_NOT_SUPPORTED;
        return;
    }
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
        if (ir->target_or_imm == 0x5357) {
            cpu_fp_leave_guest(cpu);
            if (vmm_preboot_trap(cpu)) {
                return;
            }
        }
        vcpu_raise_undef(cpu, cur_pc);
        return;
    }
    cpu->pc = cur_pc + 4ULL;
    cpu_fp_leave_guest(cpu);
    vcpu_handle_hvc(cpu);
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
    cpu->pc = cpu_ir_pc(cpu, ir) + 4ULL;
    cpu_fp_leave_guest(cpu);
    vmm_vcpu_wfi(cpu);
}

void cpu_op_wfe(vcpu_t *cpu, const ir_insn_t *ir,
                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (atomic_exchange_explicit(&cpu->event_reg, false, memory_order_acq_rel)) {
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
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
