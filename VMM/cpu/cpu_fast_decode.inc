#include "cpu_fast_layout.h"

#define CHECK_OFFSET(type, field, offset) static_assert(offsetof(type, field) == offset)
CHECK_OFFSET(ir_insn_t, target_or_imm, IR_IMM);
CHECK_OFFSET(ir_insn_t, op, IR_OP);
CHECK_OFFSET(ir_insn_t, flags, IR_FLAGS);
CHECK_OFFSET(ir_insn_t, fast_offset, IR_OFFSET);
CHECK_OFFSET(ir_insn_t, fast_rd, IR_RD);
CHECK_OFFSET(ir_insn_t, fast_rn, IR_RN);
CHECK_OFFSET(ir_insn_t, fast_rm, IR_RM);
CHECK_OFFSET(ir_insn_t, fast_ra, IR_RA);
CHECK_OFFSET(vcpu_t, x[32], REG_SINK * sizeof(uint64_t));
CHECK_OFFSET(vcpu_t, fast_sp, CPU_FAST_SP);
CHECK_OFFSET(vcpu_t, sp_el, CPU_SP);
CHECK_OFFSET(vcpu_t, pc, CPU_PC);
CHECK_OFFSET(vcpu_t, nzcv, CPU_NZCV);
CHECK_OFFSET(vcpu_t, daif, CPU_DAIF);
CHECK_OFFSET(vcpu_t, el, CPU_EL);
CHECK_OFFSET(vcpu_t, spsel, CPU_SPSEL);
CHECK_OFFSET(vcpu_t, fp_host_active, CPU_FP_ACTIVE);
CHECK_OFFSET(vcpu_t, excl_size, CPU_EXCL_SIZE);
CHECK_OFFSET(vcpu_t, excl_valid, CPU_EXCL_VALID);
CHECK_OFFSET(vcpu_t, fpcr, CPU_FPCR);
CHECK_OFFSET(vcpu_t, fpsr, CPU_FPSR);
CHECK_OFFSET(vcpu_t, icache_shootdown_pending, CPU_ICACHE_SHOOTDOWN);
CHECK_OFFSET(vcpu_t, excl_val, CPU_EXCL_VAL);
CHECK_OFFSET(vcpu_t, cntkctl_el1, CPU_CNTKCTL);
CHECK_OFFSET(vcpu_t, excl_hva, (CPU_EXCL_HVA_PAGE << 12) + CPU_EXCL_HVA);
static_assert(CPU_FPCR_MASK == 0x07C80000U);
static_assert(CPU_FPSR_MASK == 0x0800009FU);
CHECK_OFFSET(vcpu_t, backedge_budget, CPU_BUDGET);
CHECK_OFFSET(vcpu_t, irq_pending, CPU_IRQ);
CHECK_OFFSET(vcpu_t, stop_requested, CPU_STOP);
CHECK_OFFSET(vcpu_t, tlb_asid_salt, CPU_SALT);
CHECK_OFFSET(vcpu_t, tlb_epoch_local, CPU_TLB_EPOCH);
CHECK_OFFSET(vcpu_t, icache_epoch_local, CPU_ICACHE_EPOCH);
CHECK_OFFSET(vcpu_t, vm, CPU_VM);
CHECK_OFFSET(vcpu_t, icache, CPU_ICACHE);
CHECK_OFFSET(vcpu_t, exec_epoch, CPU_EXEC_EPOCH);
CHECK_OFFSET(vcpu_t, ras_epoch, CPU_RAS_EPOCH);
CHECK_OFFSET(vcpu_t, v, CPU_V);
CHECK_OFFSET(vcpu_t, ic_ivau_last_va_page, CPU_IC_IVAU_LAST);
CHECK_OFFSET(vcpu_t, sctlr_el1, CPU_SCTLR);
CHECK_OFFSET(vcpu_t, tpidr_el0, CPU_TPIDR_EL0);
CHECK_OFFSET(vcpu_t, tpidrro_el0, CPU_TPIDRRO_EL0);
CHECK_OFFSET(vcpu_t, tpidr_el1, CPU_TPIDR_EL1);
CHECK_OFFSET(vcpu_t, tlb, CPU_TLB);
CHECK_OFFSET(vmm_machine_t, tlb_epoch, VM_TLB_EPOCH);
CHECK_OFFSET(vmm_machine_t, icache_epoch, VM_ICACHE_EPOCH);
CHECK_OFFSET(vmm_machine_t, wfe_waiters, VM_WFE_WAITERS);
CHECK_OFFSET(ir_page_t, pa_page, PAGE_PA);
CHECK_OFFSET(ir_page_t, va_page, PAGE_VA);
CHECK_OFFSET(ir_page_t, decoded_mask, PAGE_DECODED_MASK);
CHECK_OFFSET(ir_page_t, gen, PAGE_GEN);
CHECK_OFFSET(ir_page_t, insns, PAGE_INSNS);
CHECK_OFFSET(cpu_icache_t, cur_page, IC_CUR_PAGE);
CHECK_OFFSET(cpu_icache_t, cur_exec_epoch, IC_CUR_EXEC_EPOCH);
CHECK_OFFSET(cpu_icache_t, ras_sp, IC_RAS_SP);
#ifdef CPU_STATS
CHECK_OFFSET(cpu_icache_t, stat_dispatches, IC_STAT_DISPATCHES);
#endif
CHECK_OFFSET(cpu_icache_t, ras, IC_RAS);
CHECK_OFFSET(cpu_icache_t, exec_cache, IC_EXEC_CACHE);
CHECK_OFFSET(exec_cache_entry_t, tag, 0);
CHECK_OFFSET(exec_cache_entry_t, pg, 8);
CHECK_OFFSET(exec_cache_entry_t, exec_epoch, 16);
CHECK_OFFSET(exec_cache_entry_t, pg_gen, 24);
CHECK_OFFSET(ras_entry_t, pc, 0);
CHECK_OFFSET(ras_entry_t, ir, 8);
CHECK_OFFSET(ras_entry_t, pg, 16);
CHECK_OFFSET(ras_entry_t, ras_epoch, 24);
CHECK_OFFSET(ras_entry_t, pg_gen, 28);
static_assert(sizeof(((vcpu_t *)0)->tlb[0]) == (1U << TLB_BANK_SHIFT));
static_assert(sizeof(((cpu_icache_t *)0)->exec_cache[0]) == (1U << EXEC_BANK_SHIFT));
static_assert(EXEC_CACHE_ENTRIES == (1U << (EXEC_BANK_SHIFT - 5)));
static_assert(SOFTTLB_EL0_R == 0 && SOFTTLB_EL0_W == 1 && SOFTTLB_EL1_R == 2 && SOFTTLB_EL1_W == 3 &&
              SOFTTLB_BANK_COUNT == 4);
static_assert(VMM_PAGE_SHIFT == 12 && MMU_SALT_USER_TAG == 0xffff000000000ffcULL);
static_assert(MMU_SALT_MMU_OFF == 1ULL << SALT_MMU_OFF && MMU_SALT_ALIGN == 1ULL << SALT_ALIGN &&
              MMU_SALT_FP_TRAP == 1ULL << SALT_FP_TRAP && MMU_SALT_ALIGN > EXEC_CACHE_MASK);
static_assert(TILE_SHIFT == 5 && RAS_SIZE == 16);
static_assert(IR_IMM_HI == IR_IMM + 4 && CPU_V % 16 == 0 && V_SLOT + 32 <= 256);
#undef CHECK_OFFSET

typedef enum cpu_fast_op {
#define FAST_OP(op) FAST_##op,
#define FAST_V2(op) FAST_OP(op##_8b) FAST_OP(op##_16b)
#define FAST_V4(op) FAST_V2(op) FAST_OP(op##_4h) FAST_OP(op##_8h)
#define FAST_V5(op) FAST_V4(op) FAST_OP(op##_4s)
#define FAST_V6(op) FAST_V4(op) FAST_OP(op##_2s) FAST_OP(op##_4s)
#define FAST_V7(op) FAST_V6(op) FAST_OP(op##_2d)
#define FAST_V7D(op) FAST_V7(op) FAST_OP(op##_d)
#define FAST_BHSD(op) FAST_OP(op##_b) FAST_OP(op##_h) FAST_OP(op##_s) FAST_OP(op##_d)
#define FAST_MEM3(op) FAST_OP(op##_off) FAST_OP(op##_pre) FAST_OP(op##_post)
#define FAST_MEM6(op) FAST_MEM3(op) FAST_OP(op##_uxtw) FAST_OP(op##_xreg) FAST_OP(op##_sxtw)
#define FAST_LDST1(op) FAST_OP(op##_8b_off) FAST_OP(op##_8b_post) FAST_OP(op##_8b_postr) \
                       FAST_OP(op##_16b_off) FAST_OP(op##_16b_post) FAST_OP(op##_16b_postr)
#define FAST_LANE(op) FAST_OP(op##_b_off) FAST_OP(op##_b_post) FAST_OP(op##_b_postr) \
                      FAST_OP(op##_h_off) FAST_OP(op##_h_post) FAST_OP(op##_h_postr) \
                      FAST_OP(op##_s_off) FAST_OP(op##_s_post) FAST_OP(op##_s_postr) \
                      FAST_OP(op##_d_off) FAST_OP(op##_d_post) FAST_OP(op##_d_postr)
#include "cpu_fast_ops.inc"
#undef FAST_OP
#undef FAST_V2
#undef FAST_V4
#undef FAST_V5
#undef FAST_V6
#undef FAST_V7
#undef FAST_V7D
#undef FAST_BHSD
#undef FAST_MEM3
#undef FAST_MEM6
#undef FAST_LDST1
#undef FAST_LANE
    FAST_COUNT,
} cpu_fast_op_t;

extern const uint32_t cpu_fast_offsets[FAST_COUNT];
extern void cpu_fast_run(vcpu_t *cpu);

typedef enum cpu_pt_kind : uint8_t {
    CPU_PT_NONE,
    CPU_PT_V,
    CPU_PT_XR,
    CPU_PT_XW,
    CPU_PT_XB,
} cpu_pt_kind_t;

typedef struct cpu_pt_leaf {
    uint32_t var_mask;
    uint32_t base;
    uint32_t features;
    struct {
        uint8_t start;
        uint8_t width;
        cpu_pt_kind_t kind;
    } slots[4];
} cpu_pt_leaf_t;

#include "generated/a64_passthrough.inc"

extern const uint32_t cpu_pt_offsets[CPU_PT_STUB_COUNT];

static uint32_t cpu_pt_missing_features(void) {
    static _Atomic uint32_t cached;
    uint32_t missing = atomic_load_explicit(&cached, memory_order_relaxed);
    if (missing == 0) {
        missing = 1U << 31;
        for (size_t i = 0; i < sizeof(cpu_pt_feature_names) / sizeof(cpu_pt_feature_names[0]); ++i) {
            char name[64] = "hw.optional.arm.";
            int present = 0;
            size_t size = sizeof(present);
            strlcat(name, cpu_pt_feature_names[i], sizeof(name));
            if (sysctlbyname(name, &present, &size, nullptr, 0) != 0 || !present) {
                missing |= 1U << i;
            }
        }
        atomic_store_explicit(&cached, missing, memory_order_relaxed);
    }
    return missing;
}

static void cpu_fast_decode_passthrough(ir_insn_t *ir) {
    const uint32_t word = (uint32_t)ir->target_or_imm;
    const int32_t index = cpu_pt_find(word);
    if (index < 0) {
        return;
    }
    const cpu_pt_leaf_t *leaf = &cpu_pt_leaves[index];
    if (leaf->features & cpu_pt_missing_features()) {
        return;
    }
    uint32_t stub = leaf->base;
    for (uint32_t mask = leaf->var_mask, bit = 1; mask != 0; mask &= mask - 1U, bit <<= 1) {
        if (word & mask & -mask) {
            stub += bit;
        }
    }
    const uint32_t offset = cpu_pt_offsets[stub];
    if (offset == 0U) {
        return;
    }
    uint8_t *slots = &ir->fast_rd;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t reg = (word >> leaf->slots[i].start) & ((1U << leaf->slots[i].width) - 1U);
        if (i == 2 && (offset & CPU_PT_WIDE_RM)) {
            reg |= ((word >> 20) & 1U) << 4;
        }
        switch (leaf->slots[i].kind) {
        case CPU_PT_NONE: break;
        case CPU_PT_V: slots[i] = (uint8_t)(V_SLOT + reg); break;
        case CPU_PT_XR: slots[i] = (uint8_t)reg; break;
        case CPU_PT_XW: slots[i] = (uint8_t)(reg == REG_ZR ? REG_SINK : reg); break;
        case CPU_PT_XB: slots[i] = (uint8_t)(reg == REG_ZR ? REG_SP : reg); break;
        }
    }
    ir->fast_offset = offset & ~(uint32_t)CPU_PT_WIDE_RM;
}

#define WIDTH(op) {FAST_##op##32, FAST_##op##64}
#define SHIFTS(op, width) {FAST_##op##_r##width, FAST_##op##_lsl##width, \
                           FAST_##op##_lsr##width, FAST_##op##_asr##width}
#define LOG_SHIFTS(op, width) {FAST_##op##_r##width, FAST_##op##_lsl##width, \
                               FAST_##op##_lsr##width, FAST_##op##_asr##width, FAST_##op##_ror##width}

static const cpu_fast_op_t addshift_ops[2][4][4] = {
    {SHIFTS(add, 32), SHIFTS(adds, 32), SHIFTS(sub, 32), SHIFTS(subs, 32)},
    {SHIFTS(add, 64), SHIFTS(adds, 64), SHIFTS(sub, 64), SHIFTS(subs, 64)},
};
static const cpu_fast_op_t logshift_ops[2][8][5] = {
    {LOG_SHIFTS(and, 32), LOG_SHIFTS(bic, 32), LOG_SHIFTS(orr, 32), LOG_SHIFTS(orn, 32),
     LOG_SHIFTS(eor, 32), LOG_SHIFTS(eon, 32), LOG_SHIFTS(ands, 32), LOG_SHIFTS(bics, 32)},
    {LOG_SHIFTS(and, 64), LOG_SHIFTS(bic, 64), LOG_SHIFTS(orr, 64), LOG_SHIFTS(orn, 64),
     LOG_SHIFTS(eor, 64), LOG_SHIFTS(eon, 64), LOG_SHIFTS(ands, 64), LOG_SHIFTS(bics, 64)},
};
static const cpu_fast_op_t carry_ops[][2] = {
    WIDTH(adc_r), WIDTH(adcs_r), WIDTH(sbc_r), WIDTH(sbcs_r),
};
static const cpu_fast_op_t dp2_ops[][2] = {
    WIDTH(udiv_r), WIDTH(sdiv_r), WIDTH(lsl_r), WIDTH(lsr_r), WIDTH(asr_r), WIDTH(ror_r),
};
static const cpu_fast_op_t logimm_ops[][2] = {
    WIDTH(and_i), WIDTH(orr_i), WIDTH(eor_i), WIDTH(ands_i),
};
static const cpu_fast_op_t bitfield_ops[2][2][2] = {
    {WIDTH(ubfx), WIDTH(ubfiz)}, {WIDTH(sbfx), WIDTH(sbfiz)},
};
static const cpu_fast_op_t cb_ops[2][2] = {
    WIDTH(cbz), WIDTH(cbnz),
};

#undef WIDTH
#undef SHIFTS
#undef LOG_SHIFTS

typedef struct cpu_fast_family {
    cpu_fast_op_t base;
    uint8_t count;
} cpu_fast_family_t;

static cpu_fast_op_t cpu_fast_arrangement(cpu_fast_family_t family, uint32_t size, uint32_t q) {
    uint32_t arr = (size << 1) | q;
    if (family.count == 5U) {
        if (arr == 5U) return family.base + 4U;
        return arr < 4U ? family.base + arr : FAST_COUNT;
    }
    if (family.count >= 7U) {
        if (arr == 7U) return family.base + 6U;
        return arr < 6U ? family.base + arr : FAST_COUNT;
    }
    return arr < family.count ? family.base + arr : FAST_COUNT;
}

static const cpu_fast_family_t simd_same_ops[2][32] = {
    {
        [6] = {FAST_vcmgt_8b, 7}, [7] = {FAST_vcmge_8b, 7}, [8] = {FAST_vsshl_8b, 7},
        [12] = {FAST_vsmax_8b, 6}, [13] = {FAST_vsmin_8b, 6}, [16] = {FAST_vadd_8b, 7},
        [17] = {FAST_vcmtst_8b, 7}, [19] = {FAST_vmul_8b, 6}, [20] = {FAST_vsmaxp_8b, 6},
        [21] = {FAST_vsminp_8b, 6}, [23] = {FAST_vaddp_8b, 7},
    },
    {
        [6] = {FAST_vcmhi_8b, 7}, [7] = {FAST_vcmhs_8b, 7}, [8] = {FAST_vushl_8b, 7},
        [12] = {FAST_vumax_8b, 6}, [13] = {FAST_vumin_8b, 6}, [16] = {FAST_vsub_8b, 7},
        [17] = {FAST_vcmeq_8b, 7}, [20] = {FAST_vumaxp_8b, 6}, [21] = {FAST_vuminp_8b, 6},
    },
};
static const cpu_fast_family_t simd_misc_ops[2][32] = {
    {
        [0] = {FAST_vrev64_8b, 6}, [1] = {FAST_vrev16_8b, 2}, [4] = {FAST_vcls_8b, 6},
        [5] = {FAST_vcnt_8b, 2}, [8] = {FAST_vcmgt0_8b, 7}, [9] = {FAST_vcmeq0_8b, 7},
        [10] = {FAST_vcmlt0_8b, 7}, [11] = {FAST_vabs_8b, 7}, [18] = {FAST_vxtn_8b, 6},
    },
    {
        [0] = {FAST_vrev32_8b, 4}, [4] = {FAST_vclz_8b, 6}, [5] = {FAST_vnot_8b, 2},
        [8] = {FAST_vcmge0_8b, 7}, [9] = {FAST_vcmle0_8b, 7}, [11] = {FAST_vneg_8b, 7},
    },
};
static const cpu_fast_family_t simd_across_ops[2][32] = {
    {[3] = {FAST_vsaddlv_8b, 5}, [10] = {FAST_vsmaxv_8b, 5}, [26] = {FAST_vsminv_8b, 5}, [27] = {FAST_vaddv_8b, 5}},
    {[3] = {FAST_vuaddlv_8b, 5}, [10] = {FAST_vumaxv_8b, 5}, [26] = {FAST_vuminv_8b, 5}},
};

typedef enum cpu_fast_kind : uint8_t {
    FAST_KIND_FIXED,
    FAST_KIND_WIDTH,
    FAST_KIND_ADDSUB_IMM,
    FAST_KIND_FLAGS_IMM,
    FAST_KIND_FUSED_IMM_BCOND,
    FAST_KIND_FUSED_REG_BCOND,
    FAST_KIND_ADDSUB_SHIFT,
    FAST_KIND_LOG_SHIFT,
    FAST_KIND_ADDSUB_EXT,
    FAST_KIND_CARRY,
    FAST_KIND_LOG_IMM,
    FAST_KIND_DP_1SRC,
    FAST_KIND_DP_2SRC,
    FAST_KIND_DP_3SRC,
    FAST_KIND_CONDSEL,
    FAST_KIND_CONDCMP,
    FAST_KIND_ADRP_ADD,
    FAST_KIND_ADRP_LOAD,
    FAST_KIND_SBFX,
    FAST_KIND_BITFIELD,
    FAST_KIND_B_COND,
    FAST_KIND_CBZ,
    FAST_KIND_TBZ,
    FAST_KIND_BRANCH_REG,
    FAST_KIND_LOAD,
    FAST_KIND_STORE,
    FAST_KIND_LDST_SINGLE,
    FAST_KIND_LDST_PAIR,
    FAST_KIND_LDR_LIT,
    FAST_KIND_UNPRIV,
    FAST_KIND_ORDERED,
    FAST_KIND_MRS,
    FAST_KIND_MSR,
    FAST_KIND_SYS,
    FAST_KIND_PSTATE,
    FAST_KIND_EXCLUSIVE,
    FAST_KIND_CRC32,
    FAST_KIND_MOPS,
    FAST_KIND_VLDST_MULTI,
    FAST_KIND_VLDST_SINGLE,
    FAST_KIND_FP_SCALAR,
    FAST_KIND_SIMD,
    FAST_KIND_PASSTHROUGH,
} cpu_fast_kind_t;

static_assert(FAST_movk64 == FAST_movk32 + 1 && FAST_extr64 == FAST_extr32 + 1);

typedef struct cpu_fast_entry {
    ir_handler_fn handler;
    cpu_fast_kind_t kind;
    cpu_fast_op_t op;
} cpu_fast_entry_t;

static const cpu_fast_entry_t cpu_fast_entries[] = {
    {op_nop, FAST_KIND_FIXED, FAST_nop},
    {op_mov_imm, FAST_KIND_FIXED, FAST_mov_imm},
    {op_mov_reg64, FAST_KIND_FIXED, FAST_mov_reg},
    {op_mov_reg32, FAST_KIND_FIXED, FAST_mov_reg32},
    {op_add_reg64, FAST_KIND_FIXED, FAST_add_r64},
    {op_add_reg32, FAST_KIND_FIXED, FAST_add_r32},
    {op_sub_reg64, FAST_KIND_FIXED, FAST_sub_r64},
    {op_sub_reg32, FAST_KIND_FIXED, FAST_sub_r32},
    {op_subs_reg64, FAST_KIND_FIXED, FAST_subs_r64},
    {op_subs_reg32, FAST_KIND_FIXED, FAST_subs_r32},
    {op_and_reg64, FAST_KIND_FIXED, FAST_and_r64},
    {op_and_reg32, FAST_KIND_FIXED, FAST_and_r32},
    {op_orr_reg64, FAST_KIND_FIXED, FAST_orr_r64},
    {op_orr_reg32, FAST_KIND_FIXED, FAST_orr_r32},
    {op_eor_reg64, FAST_KIND_FIXED, FAST_eor_r64},
    {op_eor_reg32, FAST_KIND_FIXED, FAST_eor_r32},
    {op_ands_reg64, FAST_KIND_FIXED, FAST_ands_r64},
    {op_ands_reg32, FAST_KIND_FIXED, FAST_ands_r32},
    {op_lsl_imm64, FAST_KIND_FIXED, FAST_lsl_i64},
    {op_lsl_imm32, FAST_KIND_FIXED, FAST_lsl_i32},
    {op_lsr_imm64, FAST_KIND_FIXED, FAST_lsr_i64},
    {op_lsr_imm32, FAST_KIND_FIXED, FAST_lsr_i32},
    {op_asr_imm64, FAST_KIND_FIXED, FAST_asr_i64},
    {op_asr_imm32, FAST_KIND_FIXED, FAST_asr_i32},
    {op_ubfx64, FAST_KIND_FIXED, FAST_ubfx_m64},
    {op_ubfx32, FAST_KIND_FIXED, FAST_ubfx_m32},
    {op_b, FAST_KIND_FIXED, FAST_b},
    {op_bl, FAST_KIND_FIXED, FAST_bl},
    {op_ret, FAST_KIND_FIXED, FAST_ret},
    {cpu_op_barrier, FAST_KIND_FIXED, FAST_barrier},
    {cpu_op_clrex, FAST_KIND_FIXED, FAST_clrex},
    {op_movk, FAST_KIND_WIDTH, FAST_movk32},
    {op_extr, FAST_KIND_WIDTH, FAST_extr32},
    {op_add_imm64, FAST_KIND_ADDSUB_IMM, FAST_add_i64},
    {op_add_imm32, FAST_KIND_ADDSUB_IMM, FAST_add_i32},
    {op_sub_imm64, FAST_KIND_ADDSUB_IMM, FAST_sub_i64},
    {op_sub_imm32, FAST_KIND_ADDSUB_IMM, FAST_sub_i32},
    {op_adds_imm64, FAST_KIND_FLAGS_IMM, FAST_adds_i64},
    {op_adds_imm32, FAST_KIND_FLAGS_IMM, FAST_adds_i32},
    {op_subs_imm64, FAST_KIND_FLAGS_IMM, FAST_subs_i64},
    {op_subs_imm32, FAST_KIND_FLAGS_IMM, FAST_subs_i32},
    {op_fused_subs_imm_bcond, FAST_KIND_FUSED_IMM_BCOND, FAST_COUNT},
    {op_fused_subs_reg_bcond, FAST_KIND_FUSED_REG_BCOND, FAST_COUNT},
    {op_fused_subs_reg0_bcond, FAST_KIND_FUSED_REG_BCOND, FAST_COUNT},
    {op_addsub_shift, FAST_KIND_ADDSUB_SHIFT, FAST_COUNT},
    {op_log_shift, FAST_KIND_LOG_SHIFT, FAST_COUNT},
    {op_addsub_ext, FAST_KIND_ADDSUB_EXT, FAST_COUNT},
    {op_addsub_carry, FAST_KIND_CARRY, FAST_COUNT},
    {op_log_imm, FAST_KIND_LOG_IMM, FAST_COUNT},
    {op_dp_1src, FAST_KIND_DP_1SRC, FAST_COUNT},
    {op_dp_2src, FAST_KIND_DP_2SRC, FAST_COUNT},
    {op_dp_3src, FAST_KIND_DP_3SRC, FAST_COUNT},
    {op_condsel, FAST_KIND_CONDSEL, FAST_COUNT},
    {op_condcmp, FAST_KIND_CONDCMP, FAST_COUNT},
    {op_fused_adrp_add, FAST_KIND_ADRP_ADD, FAST_adrp_add},
    {op_fused_adrp_ldr64, FAST_KIND_ADRP_LOAD, FAST_adrp_ldr64},
    {op_fused_adrp_ldr32, FAST_KIND_ADRP_LOAD, FAST_adrp_ldr32},
    {op_sbfx64, FAST_KIND_SBFX, FAST_sbfx64},
    {op_sbfx32, FAST_KIND_SBFX, FAST_sbfx32},
    {op_bitfield, FAST_KIND_BITFIELD, FAST_COUNT},
    {op_b_cond, FAST_KIND_B_COND, FAST_b_eq},
    {op_cbz, FAST_KIND_CBZ, FAST_COUNT},
    {op_tbz, FAST_KIND_TBZ, FAST_COUNT},
    {op_branch_reg, FAST_KIND_BRANCH_REG, FAST_COUNT},
    {op_ldr_u64, FAST_KIND_LOAD, FAST_ldr64_off},
    {op_fused_ldr64_add64, FAST_KIND_LOAD, FAST_ldr64_off},
    {op_ldr_u32, FAST_KIND_LOAD, FAST_ldr32_off},
    {op_fused_ldr32_add32, FAST_KIND_LOAD, FAST_ldr32_off},
    {op_ldrsw_u, FAST_KIND_LOAD, FAST_ldrsw64_off},
    {op_ldrh_u, FAST_KIND_LOAD, FAST_ldr16_off},
    {op_ldrb_u, FAST_KIND_LOAD, FAST_ldr8_off},
    {op_ldp_64, FAST_KIND_LOAD, FAST_ldp64_off},
    {op_str_u64, FAST_KIND_STORE, FAST_str64_off},
    {op_str_u32, FAST_KIND_STORE, FAST_str32_off},
    {op_strh_u, FAST_KIND_STORE, FAST_str16_off},
    {op_strb_u, FAST_KIND_STORE, FAST_str8_off},
    {op_stp_64, FAST_KIND_STORE, FAST_stp64_off},
    {op_ldst_single, FAST_KIND_LDST_SINGLE, FAST_COUNT},
    {op_ldst_pair, FAST_KIND_LDST_PAIR, FAST_COUNT},
    {op_ldr_lit, FAST_KIND_LDR_LIT, FAST_COUNT},
    {cpu_op_ldst_unpriv, FAST_KIND_UNPRIV, FAST_COUNT},
    {cpu_op_ldst_ord, FAST_KIND_ORDERED, FAST_COUNT},
    {cpu_op_sysreg_mrs, FAST_KIND_MRS, FAST_COUNT},
    {cpu_op_sysreg_msr, FAST_KIND_MSR, FAST_COUNT},
    {cpu_op_sys_insn, FAST_KIND_SYS, FAST_COUNT},
    {cpu_op_pstate_msr, FAST_KIND_PSTATE, FAST_COUNT},
    {cpu_op_ldst_exclr, FAST_KIND_EXCLUSIVE, FAST_COUNT},
    {cpu_op_crc32, FAST_KIND_CRC32, FAST_COUNT},
    {cpu_op_mops_cpy, FAST_KIND_MOPS, FAST_mops_tail},
    {cpu_op_mops_set, FAST_KIND_MOPS, FAST_mops_tail},
    {cpu_op_simd_ldst_multi, FAST_KIND_VLDST_MULTI, FAST_COUNT},
    {cpu_op_simd_ldst_single, FAST_KIND_VLDST_SINGLE, FAST_COUNT},
    {cpu_op_fp_scalar, FAST_KIND_FP_SCALAR, FAST_COUNT},
    {cpu_op_simd_dp, FAST_KIND_SIMD, FAST_COUNT},
    {cpu_op_crypto, FAST_KIND_SIMD, FAST_COUNT},
    {cpu_op_lse_memop, FAST_KIND_PASSTHROUGH, FAST_COUNT},
    {cpu_op_lse_cas, FAST_KIND_PASSTHROUGH, FAST_COUNT},
    {cpu_op_lse_casp, FAST_KIND_PASSTHROUGH, FAST_COUNT},
};

enum { CPU_FAST_SLOTS = 256 };

static const cpu_fast_entry_t *cpu_fast_slots[CPU_FAST_SLOTS];

static uint32_t cpu_fast_slot(ir_handler_fn handler) {
    return (uint32_t)(((uintptr_t)handler * 0x9E3779B97F4A7C15ULL) >> 56);
}

static void cpu_fast_fill_slots(void) {
    for (size_t i = 0; i < sizeof(cpu_fast_entries) / sizeof(cpu_fast_entries[0]); ++i) {
        uint32_t slot = cpu_fast_slot(cpu_fast_entries[i].handler);
        while (cpu_fast_slots[slot]) {
            slot = (slot + 1U) & (CPU_FAST_SLOTS - 1U);
        }
        cpu_fast_slots[slot] = &cpu_fast_entries[i];
    }
}

static const cpu_fast_entry_t *cpu_fast_lookup(ir_handler_fn handler) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, cpu_fast_fill_slots);
    for (uint32_t slot = cpu_fast_slot(handler);; slot = (slot + 1U) & (CPU_FAST_SLOTS - 1U)) {
        const cpu_fast_entry_t *entry = cpu_fast_slots[slot];
        if (!entry || entry->handler == handler) {
            return entry;
        }
    }
}

static cpu_fast_op_t cpu_fast_decode_simd(ir_insn_t *ir, cpu_fast_kind_t kind) {
    const uint32_t word = (uint32_t)ir->target_or_imm;
    const uint32_t q = (word >> 30) & 1U;
    const uint32_t u = (word >> 29) & 1U;
    const uint32_t size = (word >> 22) & 3U;
    const uint32_t rd = word & 31U;
    const uint32_t rn = (word >> 5) & 31U;
    const uint32_t rm = (word >> 16) & 31U;
    ir->fast_rd = (uint8_t)(V_SLOT + rd);
    ir->fast_rn = (uint8_t)(V_SLOT + rn);
    ir->fast_rm = (uint8_t)(V_SLOT + rm);
    ir->fast_ra = 0;

    switch (kind) {
    case FAST_KIND_VLDST_MULTI: {
        static const uint8_t regs[16] = {[2] = 4, [6] = 3, [7] = 1, [10] = 2};
        uint32_t count = regs[(word >> 12) & 15U];
        if (count == 0U || rd + count > 32U) return FAST_COUNT;
        bool post = ((word >> 23) & 1U) != 0;
        bool load = ((word >> 22) & 1U) != 0;
        ir->fast_rn = (uint8_t)(rn == REG_ZR ? REG_SP : rn);
        ir->fast_rm = (uint8_t)rm;
        uint32_t mode = !post ? 0U : rm == REG_ZR ? 1U : 2U;
        return (load ? FAST_vld1_1_8b_off : FAST_vst1_1_8b_off) + ((count - 1U) * 2U + q) * 3U + mode;
    }
    case FAST_KIND_VLDST_SINGLE: {
        uint32_t opcode = (word >> 13) & 7U;
        uint32_t s = (word >> 12) & 1U;
        uint32_t lane_size = (word >> 10) & 3U;
        uint32_t esize_log = opcode >> 1;
        uint32_t index;
        if (((word >> 21) & 1U) != 0U || (opcode & 1U) != 0U) return FAST_COUNT;
        if (esize_log == 0U) {
            index = (q << 3) | (s << 2) | lane_size;
        } else if (esize_log == 1U && (lane_size & 1U) == 0U) {
            index = (q << 2) | (s << 1) | (lane_size >> 1);
        } else if (esize_log == 2U && lane_size == 0U) {
            index = (q << 1) | s;
        } else if (esize_log == 2U && lane_size == 1U && s == 0U) {
            esize_log = 3U;
            index = q;
        } else {
            return FAST_COUNT;
        }
        bool post = ((word >> 23) & 1U) != 0;
        bool load = ((word >> 22) & 1U) != 0;
        ir->fast_rn = (uint8_t)(rn == REG_ZR ? REG_SP : rn);
        ir->fast_rm = (uint8_t)rm;
        ir->fast_ra = (uint8_t)(index << esize_log);
        uint32_t mode = !post ? 0U : rm == REG_ZR ? 1U : 2U;
        return (load ? FAST_vld1l_b_off : FAST_vst1l_b_off) + esize_log * 3U + mode;
    }
    case FAST_KIND_FP_SCALAR: {
        uint32_t ftype = (word >> 22) & 3U;
        uint32_t rmode = (word >> 19) & 3U;
        uint32_t opcode = (word >> 16) & 7U;
        if (ir->op != SUBOP_FP_2INT || u != 0U || (opcode & 6U) != 6U) return FAST_COUNT;
        uint32_t form;
        if ((word >> 31) == 0U && ftype == 0U && rmode == 0U) form = 0U;
        else if ((word >> 31) != 0U && ftype == 1U && rmode == 0U) form = 1U;
        else if ((word >> 31) != 0U && ftype == 2U && rmode == 1U) form = 2U;
        else return FAST_COUNT;
        uint32_t to_vector = opcode & 1U;
        if (to_vector) {
            ir->fast_rn = (uint8_t)rn;
        } else {
            ir->fast_rd = (uint8_t)(rd == REG_ZR ? REG_SINK : rd);
        }
        return FAST_vfmov_ws + form * 2U + to_vector;
    }
    default:
        break;
    }

    switch (ir->op) {
    case SUBOP_SIMD_SAME: {
        uint32_t opc = (word >> 11) & 31U;
        if (opc == 3U) return FAST_vand_8b + ((((u << 2) | size) << 1) | q);
        return cpu_fast_arrangement(simd_same_ops[u][opc], size, q);
    }
    case SUBOP_SISD_SAME:
        if (size != 3U || ((word >> 11) & 31U) != 16U) return FAST_COUNT;
        return u ? FAST_vsub_d : FAST_vadd_d;
    case SUBOP_SIMD_MISC: {
        uint32_t opc = (word >> 12) & 31U;
        if (u && opc == 5U && size == 1U) return FAST_vrbit_8b + q;
        return cpu_fast_arrangement(simd_misc_ops[u][opc], size, q);
    }
    case SUBOP_SIMD_SHF:
    case SUBOP_SISD_SHF: {
        uint32_t immh = (word >> 19) & 15U;
        uint32_t imm7 = (word >> 16) & 127U;
        if (immh == 0U) return FAST_COUNT;
        uint32_t esize_log = 31U - (uint32_t)__builtin_clz(immh);
        uint32_t esize = 8U << esize_log;
        uint8_t right = (uint8_t)(imm7 - 2U * esize);
        uint8_t left = (uint8_t)(imm7 - esize);
        cpu_fast_family_t family;
        switch ((u << 5) | ((word >> 11) & 31U)) {
        case 0x00U: family = (cpu_fast_family_t){FAST_vshls_8b, 8}; ir->fast_ra = right; break;
        case 0x20U: family = (cpu_fast_family_t){FAST_vshlu_8b, 8}; ir->fast_ra = right; break;
        case 0x02U: family = (cpu_fast_family_t){FAST_vsras_8b, 8}; ir->fast_ra = right; break;
        case 0x22U: family = (cpu_fast_family_t){FAST_vsrau_8b, 8}; ir->fast_ra = right; break;
        case 0x28U: family = (cpu_fast_family_t){FAST_vsxi_8b, 8}; ir->fast_ra = right; break;
        case 0x0AU: family = (cpu_fast_family_t){FAST_vshlu_8b, 8}; ir->fast_ra = left; break;
        case 0x2AU: family = (cpu_fast_family_t){FAST_vsxi_8b, 8}; ir->fast_ra = left; break;
        case 0x10U: family = (cpu_fast_family_t){FAST_vshrn_8b, 6}; ir->fast_ra = right; break;
        case 0x14U: family = (cpu_fast_family_t){FAST_vsshll_8b, 6}; ir->fast_ra = left; break;
        case 0x34U: family = (cpu_fast_family_t){FAST_vushll_8b, 6}; ir->fast_ra = left; break;
        default: return FAST_COUNT;
        }
        if (ir->op == SUBOP_SISD_SHF) {
            return family.count == 8U && esize_log == 3U ? family.base + 7U : FAST_COUNT;
        }
        return cpu_fast_arrangement(family, esize_log, q);
    }
    case SUBOP_SIMD_EXT: {
        uint32_t imm4 = (word >> 11) & 15U;
        if (size != 0U || (!q && imm4 >= 8U)) return FAST_COUNT;
        ir->fast_ra = (uint8_t)imm4;
        return FAST_vext_8b + q;
    }
    case SUBOP_SIMD_PERM: {
        static const cpu_fast_op_t perm[8] = {
            FAST_COUNT, FAST_vuzp1_8b, FAST_vtrn1_8b, FAST_vzip1_8b,
            FAST_COUNT, FAST_vuzp2_8b, FAST_vtrn2_8b, FAST_vzip2_8b,
        };
        cpu_fast_op_t base = perm[(word >> 12) & 7U];
        if (base == FAST_COUNT) return FAST_COUNT;
        return cpu_fast_arrangement((cpu_fast_family_t){base, 7}, size, q);
    }
    case SUBOP_SIMD_TBL: {
        uint32_t len = (word >> 13) & 3U;
        if (size != 0U || rn + len > 31U) return FAST_COUNT;
        return FAST_vtbl1_8b + ((((((word >> 12) & 1U) << 2) | len) << 1) | q);
    }
    case SUBOP_SIMD_INS: {
        uint32_t imm5 = (word >> 16) & 31U;
        uint32_t imm4 = (word >> 11) & 15U;
        if ((imm5 & 15U) == 0U) return FAST_COUNT;
        uint32_t esz = (uint32_t)__builtin_ctz(imm5);
        ir->fast_ra = (uint8_t)((imm5 >> (esz + 1U)) << esz);
        if (u) {
            if (!q) return FAST_COUNT;
            ir->fast_rm = (uint8_t)((imm4 >> esz) << esz);
            return FAST_vinse_b + esz;
        }
        switch (imm4) {
        case 0U:
            return cpu_fast_arrangement((cpu_fast_family_t){FAST_vdupe_8b, 7}, esz, q);
        case 1U:
            ir->fast_rn = (uint8_t)rn;
            return cpu_fast_arrangement((cpu_fast_family_t){FAST_vdupg_8b, 7}, esz, q);
        case 3U:
            if (!q) return FAST_COUNT;
            ir->fast_rn = (uint8_t)rn;
            return FAST_vinsg_b + esz;
        case 5U: {
            static const cpu_fast_op_t smov[3][2] = {
                {FAST_vsmov_bw, FAST_vsmov_bx}, {FAST_vsmov_hw, FAST_vsmov_hx}, {FAST_COUNT, FAST_vsmov_sx},
            };
            if (esz > 2U) return FAST_COUNT;
            ir->fast_rd = (uint8_t)(rd == REG_ZR ? REG_SINK : rd);
            return smov[esz][q];
        }
        case 7U:
            if (q != (esz == 3U ? 1U : 0U)) return FAST_COUNT;
            ir->fast_rd = (uint8_t)(rd == REG_ZR ? REG_SINK : rd);
            return FAST_vumov_b + esz;
        default:
            return FAST_COUNT;
        }
    }
    case SUBOP_SIMD_ONE: {
        uint32_t imm5 = (word >> 16) & 31U;
        if (u || ((word >> 11) & 15U) != 0U || (imm5 & 15U) == 0U) return FAST_COUNT;
        uint32_t esz = (uint32_t)__builtin_ctz(imm5);
        ir->fast_ra = (uint8_t)((imm5 >> (esz + 1U)) << esz);
        return FAST_vdups_b + esz;
    }
    case SUBOP_SIMD_IMM: {
        uint32_t cmode = (word >> 12) & 15U;
        uint32_t imm8 = (((word >> 16) & 7U) << 5) | ((word >> 5) & 31U);
        uint32_t c3 = cmode >> 1;
        uint32_t value;
        if (((word >> 11) & 1U) != 0U) return FAST_COUNT;
        if (c3 <= 3U) value = imm8 << (8U * c3);
        else if (c3 <= 5U) value = (imm8 << (8U * (c3 - 4U))) * 0x00010001U;
        else if (c3 == 6U) value = (cmode & 1U) ? ((imm8 << 16) | 0xFFFFU) : ((imm8 << 8) | 0xFFU);
        else if (cmode == 14U && !u) value = imm8 * 0x01010101U;
        else if (cmode == 14U && (imm8 == 0U || imm8 == 0xFFU)) value = imm8 ? UINT32_MAX : 0U;
        else return FAST_COUNT;
        cpu_fast_op_t base = FAST_vmovi_8b;
        if (c3 <= 5U && (cmode & 1U) != 0U) {
            base = u ? FAST_vandi_8b : FAST_vorri_8b;
            if (u) value = ~value;
        } else if (u && c3 <= 6U) {
            value = ~value;
        }
        ir->target_or_imm = (uint32_t)ir->target_or_imm | ((uint64_t)value << 32);
        return base + q;
    }
    case SUBOP_SIMD_ALL:
        return cpu_fast_arrangement(simd_across_ops[u][(word >> 12) & 31U], size, q);
    case SUBOP_CRYPTO_AES: {
        uint32_t opc = (word >> 12) & 31U;
        return opc >= 4U && opc <= 7U ? FAST_aese + (opc - 4U) : FAST_COUNT;
    }
    case SUBOP_CRYPTO_SHA2: {
        uint32_t opc = (word >> 12) & 31U;
        return opc <= 2U ? FAST_sha1h + opc : FAST_COUNT;
    }
    case SUBOP_CRYPTO_SHA3: {
        uint32_t opc = (word >> 12) & 7U;
        return opc <= 6U ? FAST_sha1c + opc : FAST_COUNT;
    }
    case SUBOP_CRYPTO_IMM6:
        ir->fast_ra = (uint8_t)((word >> 10) & 63U);
        return FAST_xar;
    case SUBOP_CRYPTO_4: {
        uint32_t op0 = (word >> 21) & 3U;
        ir->fast_ra = (uint8_t)(V_SLOT + ((word >> 10) & 31U));
        return op0 <= 1U ? FAST_eor3 + op0 : FAST_COUNT;
    }
    case SUBOP_CRYPTO_SHA512_2:
        return ((word >> 10) & 3U) == 0U ? FAST_sha512su0 : FAST_COUNT;
    case SUBOP_CRYPTO_SHA512_3:
        return ((word >> 14) & 1U) == 0U ? FAST_sha512h + ((word >> 10) & 3U) : FAST_COUNT;
    default:
        return FAST_COUNT;
    }
}

static void cpu_fast_decode(ir_insn_t *ir) {
    const uint16_t flags = ir->flags;
    const bool wide = (flags & IR_FLAG_WIDE) != 0;
    const bool inv = (flags & IR_FLAG_INV) != 0;
    const bool sub = (flags & IR_FLAG_SUB) != 0;
    const bool setflags = (flags & IR_FLAG_SETFLAGS) != 0;

    ir->fast_offset = 0;
    ir->fast_rd = ir->rd == REG_ZR ? REG_SINK : ir->rd;
    ir->fast_rn = ir->rn;
    ir->fast_rm = ir->rm;
    ir->fast_ra = ir->ra;

    const cpu_fast_entry_t *entry = cpu_fast_lookup(ir->handler);
    if (!entry) {
        return;
    }
    cpu_fast_op_t op = entry->op;
    switch (entry->kind) {
    case FAST_KIND_FIXED:
        break;
    case FAST_KIND_WIDTH:
        op += wide;
        break;
    case FAST_KIND_ADDSUB_IMM:
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        if (ir->rd == REG_ZR) ir->fast_rd = REG_SP;
        break;
    case FAST_KIND_FLAGS_IMM:
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        break;
    case FAST_KIND_FUSED_IMM_BCOND:
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        op = (wide ? FAST_f_imm64_eq : FAST_f_imm32_eq) + ir[1].op;
        break;
    case FAST_KIND_FUSED_REG_BCOND:
        op = (wide ? FAST_f_reg64_eq : FAST_f_reg32_eq) + ir[1].op;
        break;
    case FAST_KIND_ADDSUB_SHIFT:
    case FAST_KIND_LOG_SHIFT: {
        ir->fast_ra = ir->op & 63;
        uint32_t shift = ir->fast_ra ? 1U + ((ir->op >> 8) & 3U) : 0U;
        op = entry->kind == FAST_KIND_ADDSUB_SHIFT ? addshift_ops[wide][2U * sub + setflags][shift]
                                                   : logshift_ops[wide][2U * (ir->op >> 10) + inv][shift];
        break;
    }
    case FAST_KIND_ADDSUB_EXT: {
        static const cpu_fast_op_t extended[][4] = {
            {FAST_add_ext32, FAST_adds_ext32, FAST_sub_ext32, FAST_subs_ext32},
            {FAST_add_ext64, FAST_adds_ext64, FAST_sub_ext64, FAST_subs_ext64},
        };
        unsigned bytes_log = (ir->op >> 8) & 3U;
        ir->fast_ra = (uint8_t)(64U - (8U << bytes_log));
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        if (ir->rd == REG_ZR && !setflags) ir->fast_rd = REG_SP;
        op = extended[wide][2U * sub + setflags];
        break;
    }
    case FAST_KIND_CARRY:
        op = carry_ops[2U * sub + setflags][wide];
        break;
    case FAST_KIND_LOG_IMM:
        op = logimm_ops[ir->op][wide];
        if (ir->rd == REG_ZR && ir->op != 3) ir->fast_rd = REG_SP;
        break;
    case FAST_KIND_DP_1SRC: {
        static const cpu_fast_op_t unary[2][8] = {
            {FAST_rbit32, FAST_rev16w, FAST_rev32w, FAST_rev64,
             FAST_clz32, FAST_cls32, FAST_cls32, FAST_cls32},
            {FAST_rbit64, FAST_rev16x, FAST_rev32x, FAST_rev64,
             FAST_clz64, FAST_cls64, FAST_cls64, FAST_cls64},
        };
        op = unary[wide][ir->op];
        break;
    }
    case FAST_KIND_DP_2SRC:
        op = dp2_ops[ir->op][wide];
        break;
    case FAST_KIND_DP_3SRC: {
        static const cpu_fast_op_t multiply[] = {
            FAST_madd32, FAST_msub32, FAST_madd64, FAST_msub64, FAST_smaddl,
            FAST_smsubl, FAST_umaddl, FAST_umsubl, FAST_smulh, FAST_umulh,
        };
        op = multiply[ir->op];
        break;
    }
    case FAST_KIND_CONDSEL: {
        static const cpu_fast_op_t select[][4] = {
            {FAST_csel32_eq, FAST_csinc32_eq, FAST_csinv32_eq, FAST_csneg32_eq},
            {FAST_csel64_eq, FAST_csinc64_eq, FAST_csinv64_eq, FAST_csneg64_eq},
        };
        op = select[wide][ir->op >> 4] + (ir->op & 15U);
        break;
    }
    case FAST_KIND_CONDCMP:
        ir->fast_ra = (uint8_t)(ir->op >> 4);
        op = FAST_ccmp_eq + (ir->op & 15U);
        break;
    case FAST_KIND_ADRP_ADD:
        ir->fast_rm = ir[1].rd == REG_ZR ? REG_SP : ir[1].rd;
        break;
    case FAST_KIND_ADRP_LOAD:
        ir->fast_rm = ir[1].rd == REG_ZR ? REG_SINK : ir[1].rd;
        break;
    case FAST_KIND_SBFX:
        ir->fast_rm = (uint8_t)(ir->op >> 8);
        ir->fast_ra = (uint8_t)(ir->op & 0xFFU);
        break;
    case FAST_KIND_BITFIELD: {
        uint32_t bits = wide ? 64U : 32U;
        uint32_t high = ir->op & 63U, rotate = (ir->op >> 6) & 63U;
        bool wrap = high < rotate;
        ir->fast_rm = (uint8_t)(bits - 1U - high);
        ir->fast_ra = (uint8_t)(wrap ? bits - rotate : bits - 1U - high + rotate);
        if ((ir->op >> 12) == 1) {
            op = wrap ? (wide ? FAST_bfi64 : FAST_bfi32) : (wide ? FAST_bfxil64 : FAST_bfxil32);
        } else {
            op = bitfield_ops[(ir->op >> 12) == 0][wrap][wide];
        }
        break;
    }
    case FAST_KIND_B_COND:
        op += ir->op;
        break;
    case FAST_KIND_CBZ:
        ir->fast_rd = ir->rd;
        op = cb_ops[inv][wide];
        break;
    case FAST_KIND_TBZ:
        ir->fast_rd = ir->rd;
        op = inv ? FAST_tbnz : FAST_tbz;
        break;
    case FAST_KIND_BRANCH_REG:
        op = ir->op == 1U ? FAST_blr : FAST_br;
        break;
    case FAST_KIND_LOAD:
    case FAST_KIND_STORE:
    case FAST_KIND_LDST_SINGLE:
    case FAST_KIND_LDST_PAIR: {
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        unsigned mode = (flags & IR_FLAG_PRE) ? 1U : (flags & IR_FLAG_POST) ? 2U : 0U;
        bool load = entry->kind != FAST_KIND_STORE;
        if (entry->kind == FAST_KIND_LDST_SINGLE && (ir->op & 64)) {
            unsigned ext = (ir->op >> 8) & 7U;
            mode = ext == 2 ? 3U : ext == 6 ? 5U : 4U;
            ir->fast_ra = (uint8_t)(ir->op >> 11);
        }
        if (flags & IR_FLAG_SIMD) {
            static const cpu_fast_op_t single[2][5] = {
                {FAST_vstr8_off, FAST_vstr16_off, FAST_vstr32_off, FAST_vstr64_off, FAST_vstr128_off},
                {FAST_vldr8_off, FAST_vldr16_off, FAST_vldr32_off, FAST_vldr64_off, FAST_vldr128_off},
            };
            static const cpu_fast_op_t pair[2][3] = {
                {FAST_vstp32_off, FAST_vstp64_off, FAST_vstp128_off},
                {FAST_vldp32_off, FAST_vldp64_off, FAST_vldp128_off},
            };
            unsigned size = ir->op & 7U;
            load = (ir->op & 8U) != 0;
            if (entry->kind == FAST_KIND_LDST_PAIR && size >= 2U && size <= 4U) {
                op = pair[load][size - 2U];
                ir->fast_rm = (uint8_t)(V_SLOT + ir->rm);
            } else if (entry->kind == FAST_KIND_LDST_SINGLE && size <= 4U) {
                op = single[load][size];
            } else {
                return;
            }
            ir->fast_rd = (uint8_t)(V_SLOT + ir->rd);
            ir->fast_offset = cpu_fast_offsets[op + mode];
            return;
        }
        if (entry->kind == FAST_KIND_LDST_SINGLE) {
            static const cpu_fast_op_t single[][4] = {
                {FAST_str8_off, FAST_str16_off, FAST_str32_off, FAST_str64_off},
                {FAST_ldr8_off, FAST_ldr16_off, FAST_ldr32_off, FAST_ldr64_off},
                {FAST_ldrsb64_off, FAST_ldrsh64_off, FAST_ldrsw64_off, FAST_COUNT},
                {FAST_ldrsb32_off, FAST_ldrsh32_off, FAST_COUNT, FAST_COUNT},
            };
            unsigned size = ir->op & 7U;
            load = (ir->op & 8U) != 0;
            unsigned sign = (ir->op >> 4) & 3U;
            if (size >= 4 || sign > 2) return;
            op = single[sign ? sign + 1 : load][size];
            if (op == FAST_COUNT) return;
        } else if (entry->kind == FAST_KIND_LDST_PAIR) {
            unsigned size = ir->op & 7U;
            load = (ir->op & 8U) != 0;
            if (size != 2 && size != 3) return;
            if (size == 2 && (ir->op & 16U)) {
                if (!load) return;
                op = FAST_ldpsw64_off;
            } else if (size == 3) {
                op = load ? FAST_ldp64_off : FAST_stp64_off;
            } else {
                op = load ? FAST_ldp32_off : FAST_stp32_off;
            }
            if (load && ir->rm == REG_ZR) ir->fast_rm = REG_SINK;
        }
        if (!load) ir->fast_rd = ir->rd;
        op += mode;
        break;
    }
    case FAST_KIND_LDR_LIT:
        if (flags & IR_FLAG_SIMD) {
            if (ir->op > 2U) return;
            ir->fast_rd = (uint8_t)(V_SLOT + ir->rd);
            op = FAST_vldr32_lit + ir->op;
        } else {
            op = FAST_ldr32_lit + ir->op;
        }
        break;
    case FAST_KIND_UNPRIV: {
        static const cpu_fast_op_t unpriv[4][4] = {
            {FAST_sttr8, FAST_sttr16, FAST_sttr32, FAST_sttr64},
            {FAST_ldtr8, FAST_ldtr16, FAST_ldtr32, FAST_ldtr64},
            {FAST_ldtrsb64, FAST_ldtrsh64, FAST_ldtrsw64, FAST_COUNT},
            {FAST_ldtrsb32, FAST_ldtrsh32, FAST_COUNT, FAST_COUNT},
        };
        uint32_t opc = (uint32_t)ir->op >> 2;
        op = unpriv[opc][ir->op & 3U];
        if (op == FAST_COUNT) return;
        if (ir->rn == REG_ZR) ir->fast_rn = REG_SP;
        if (opc == 0U) ir->fast_rd = ir->rd;
        break;
    }
    case FAST_KIND_ORDERED: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        const uint32_t rt = word & 31U;
        const uint32_t rn = (word >> 5) & 31U;
        const bool load = ((word >> 22) & 1U) != 0;
        ir->fast_rd = (uint8_t)(load && rt == REG_ZR ? REG_SINK : rt);
        ir->fast_rn = (uint8_t)(rn == REG_ZR ? REG_SP : rn);
        op = (load ? FAST_ldar8 : FAST_stlr8) + (word >> 30);
        break;
    }
    case FAST_KIND_MSR: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        size_t field = 0;
        ir->fast_rd = (uint8_t)(word & 31U);
        switch ((uint16_t)(word >> 5)) {
        case A64_SYSREG_TPIDR_EL0: op = FAST_msr_tpidr_el0; break;
        case A64_SYSREG_DAIF: op = FAST_msr_daif; break;
        case A64_SYSREG_NZCV: op = FAST_msr_nzcv; break;
        case A64_SYSREG_FPCR: op = FAST_msr_fpcr; break;
        case A64_SYSREG_FPSR: op = FAST_msr_fpsr; break;
        case A64_SYSREG_SP_EL0: op = FAST_msr_sp_el0; break;
        case A64_SYSREG_ELR_EL1: op = FAST_msr_el1; field = offsetof(vcpu_t, elr_el1); break;
        case A64_SYSREG_SPSR_EL1: op = FAST_msr_el1; field = offsetof(vcpu_t, spsr_el1); break;
        case A64_SYSREG_ESR_EL1: op = FAST_msr_el1; field = offsetof(vcpu_t, esr_el1); break;
        case A64_SYSREG_FAR_EL1: op = FAST_msr_el1; field = offsetof(vcpu_t, far_el1); break;
        case A64_SYSREG_TPIDR_EL1: op = FAST_msr_el1; field = offsetof(vcpu_t, tpidr_el1); break;
        case A64_SYSREG_TPIDRRO_EL0: op = FAST_msr_el1; field = offsetof(vcpu_t, tpidrro_el0); break;
        default: return;
        }
        ir->fast_ra = (uint8_t)(field / 8U);
        break;
    }
    case FAST_KIND_MRS: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        const uint32_t rt = word & 31U;
        size_t field = 0;
        switch ((uint16_t)(word >> 5)) {
        case A64_SYSREG_TPIDR_EL0: op = FAST_mrs_tpidr_el0; break;
        case A64_SYSREG_TPIDRRO_EL0: op = FAST_mrs_tpidrro_el0; break;
        case A64_SYSREG_TPIDR_EL1: op = FAST_mrs_tpidr_el1; break;
        case A64_SYSREG_SP_EL0: op = FAST_mrs_sp_el0; break;
        case A64_SYSREG_DAIF: op = FAST_mrs_daif; break;
        case A64_SYSREG_NZCV: op = FAST_mrs_nzcv; break;
        case A64_SYSREG_FPCR: op = FAST_mrs_fpcr; break;
        case A64_SYSREG_FPSR: op = FAST_mrs_fpsr; break;
        case A64_SYSREG_DCZID_EL0: op = FAST_mrs_dczid; break;
        case A64_SYSREG_CNTVCT_EL0: op = FAST_mrs_cntvct; break;
        case A64_SYSREG_ELR_EL1: op = FAST_mrs_el1; field = offsetof(vcpu_t, elr_el1); break;
        case A64_SYSREG_SPSR_EL1: op = FAST_mrs_el1; field = offsetof(vcpu_t, spsr_el1); break;
        case A64_SYSREG_ESR_EL1: op = FAST_mrs_el1; field = offsetof(vcpu_t, esr_el1); break;
        case A64_SYSREG_FAR_EL1: op = FAST_mrs_el1; field = offsetof(vcpu_t, far_el1); break;
        case A64_SYSREG_VBAR_EL1: op = FAST_mrs_el1; field = offsetof(vcpu_t, vbar_el1); break;
        default: return;
        }
        ir->fast_rd = (uint8_t)(rt == REG_ZR ? REG_SINK : rt);
        ir->fast_ra = (uint8_t)(field / 8U);
        break;
    }
    case FAST_KIND_SYS: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        const uint32_t rt = word & 31U;
        switch (A64_SYSOP_KEY((word >> 16) & 7U, (word >> 12) & 15U, (word >> 8) & 15U, (word >> 5) & 7U)) {
        case A64_SYSOP_DC_ZVA:
            ir->fast_rd = (uint8_t)rt;
            op = FAST_dc_zva;
            break;
        case A64_SYSOP_IC_IVAU:
            if (rt == REG_ZR) return;
            ir->fast_rd = (uint8_t)rt;
            op = FAST_ic_ivau;
            break;
        case A64_SYSOP_DC_CVAC:
        case A64_SYSOP_DC_CVAU:
        case A64_SYSOP_DC_CIVAC:
        case A64_SYSOP_DC_CVAP:
            op = FAST_dc_uet;
            break;
        case A64_SYSOP_DC_IVAC:
        case A64_SYSOP_DC_ISW:
        case A64_SYSOP_DC_CSW:
        case A64_SYSOP_DC_CISW:
            op = FAST_dc_el1;
            break;
        default:
            return;
        }
        break;
    }
    case FAST_KIND_PSTATE: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        const uint32_t op2 = (word >> 5) & 7U;
        if (((word >> 16) & 7U) != 3U || (op2 != 6U && op2 != 7U)) return;
        ir->fast_ra = (uint8_t)((word >> 8) & 15U);
        op = op2 == 6U ? FAST_msr_daifset : FAST_msr_daifclr;
        break;
    }
    case FAST_KIND_EXCLUSIVE: {
        const uint32_t word = (uint32_t)ir->target_or_imm;
        const uint32_t rt = word & 31U;
        const uint32_t rn = (word >> 5) & 31U;
        const uint32_t rs = (word >> 16) & 31U;
        const uint32_t form = ((word >> 15) & 1U) * 4U + (word >> 30);
        if (cpu_pt_missing_features() & CPU_PT_FEAT_LSE) return;
        ir->fast_rn = (uint8_t)(rn == REG_ZR ? REG_SP : rn);
        if ((word >> 22) & 1U) {
            ir->fast_rd = (uint8_t)(rt == REG_ZR ? REG_SINK : rt);
            op = FAST_ldxr8 + form;
        } else {
            ir->fast_rd = (uint8_t)rt;
            ir->fast_rm = (uint8_t)(rs == REG_ZR ? REG_SINK : rs);
            op = FAST_stxr8 + form;
        }
        break;
    }
    case FAST_KIND_CRC32:
        op = FAST_crc32b + (ir->op - SUBOP_CRC32B);
        break;
    case FAST_KIND_MOPS:
        if ((ir->op & 3U) == 0U) return;
        break;
    case FAST_KIND_VLDST_MULTI:
    case FAST_KIND_VLDST_SINGLE:
    case FAST_KIND_FP_SCALAR:
    case FAST_KIND_SIMD:
        op = cpu_fast_decode_simd(ir, entry->kind);
        if (op == FAST_COUNT) {
            cpu_fast_decode_passthrough(ir);
            return;
        }
        break;
    case FAST_KIND_PASSTHROUGH:
        cpu_fast_decode_passthrough(ir);
        return;
    }
    ir->fast_offset = cpu_fast_offsets[op];
}
