#include "cpu_internal.h"

static inline void cpu_notify_wfe_write(vcpu_t *cpu, uintptr_t hva) {
    vmm_machine_t *vm = cpu->vm;
    if (__builtin_expect(atomic_load_explicit(&vm->wfe_waiters, memory_order_seq_cst) != 0, 0)) {
        vmm_vcpu_wfe_notify_write(vm, hva);
    }
}

static bool unpriv_read_bytes(vcpu_t *cpu, uint64_t va, uint8_t *dst, uint32_t size, uint64_t elr) {
    const softtlb_entry_t *bank = cpu->tlb[SOFTTLB_EL0_R];
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, size, &hva), 1)) {
        cpu_lane_set(dst, 0, size, cpu_lane_get_u((const uint8_t *)hva, 0, size));
        return true;
    }
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = va + done;
        uint32_t chunk = (uint32_t)(VMM_PAGE_SIZE - (cur & VMM_PAGE_MASK));
        if (chunk > size - done) {
            chunk = size - done;
        }
        mmu_translation_t tr;
        if (!mmu_translate_and_fill(cpu, cur, MMU_ACCESS_READ, 0, &tr, NULL)) {
            vcpu_raise_abort(cpu, cur, &tr, false, elr, false);
            return false;
        }
        if (tr.is_mmio) {
            cpu_fp_leave_guest(cpu);
            uint64_t v = vmm_mmio_read(cpu, tr.pa, chunk);
            memcpy(dst + done, &v, chunk);
        } else {
            memcpy(dst + done, (const void *)tr.hva, chunk);
        }
        done += chunk;
    }
    return true;
}

static bool unpriv_write_bytes(vcpu_t *cpu, uint64_t va, const uint8_t *src, uint32_t size, uint64_t elr) {
    const softtlb_entry_t *bank = cpu->tlb[SOFTTLB_EL0_W];
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, size, &hva), 1)) {
        cpu_lane_set((uint8_t *)hva, 0, size, cpu_lane_get_u(src, 0, size));
        return true;
    }
    uint32_t done = 0;
    while (done < size) {
        uint64_t cur = va + done;
        uint32_t chunk = (uint32_t)(VMM_PAGE_SIZE - (cur & VMM_PAGE_MASK));
        if (chunk > size - done) {
            chunk = size - done;
        }
        mmu_translation_t tr;
        if (!mmu_translate_and_fill(cpu, cur, MMU_ACCESS_WRITE, 0, &tr, NULL)) {
            vcpu_raise_abort(cpu, cur, &tr, true, elr, false);
            return false;
        }
        if (tr.is_mmio) {
            uint64_t v = 0;
            memcpy(&v, src + done, chunk);
            cpu_fp_leave_guest(cpu);
            vmm_mmio_write(cpu, tr.pa, chunk, v);
        } else {
            memcpy((void *)tr.hva, src + done, chunk);
        }
        done += chunk;
    }
    return true;
}

void cpu_op_ldst_unpriv(vcpu_t *cpu, const ir_insn_t *ir,
                        softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t opc = (uint32_t)ir->op >> 2;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, ir->rn, elr)) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + (uint64_t)ir->simm;
    uint32_t bytes = 1U << (ir->op & 3U);

    if (opc == 0U) {
        uint64_t val = cpu_get_x(cpu, ir->rd);
        if (!unpriv_write_bytes(cpu, va, (const uint8_t *)&val, bytes, elr)) {
            return;
        }
    } else {
        uint64_t val = 0;
        if (!unpriv_read_bytes(cpu, va, (uint8_t *)&val, bytes, elr)) {
            return;
        }
        if (opc == 2U) {
            val = (uint64_t)cpu_sext(val, bytes * 8U);
        } else if (opc == 3U) {
            val = (uint64_t)(uint32_t)cpu_sext(val, bytes * 8U);
        }
        cpu_set_x(cpu, ir->rd, val);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static bool resolve_atomic_addr(vcpu_t *cpu, uint64_t va, mmu_access_t acc, bool is_write_abort,
                                uint64_t elr, mmu_translation_t *tr_out) {
    if (!mmu_translate_and_fill(cpu, va, acc, cpu->el, tr_out, NULL)) {
        vcpu_raise_abort(cpu, va, tr_out, is_write_abort, elr, false);
        return false;
    }
    return true;
}

static bool ordered_access(vcpu_t *cpu, const ir_insn_t *ir, const softtlb_entry_t *tlb_r,
                           const softtlb_entry_t *tlb_w, uint32_t word, int64_t offset, uint32_t opc) {
    uint32_t size = (word >> 30) & 3U;
    bool is_load = opc != 0U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return false;
    }
    uint64_t va = cpu_get_xsp(cpu, rn) + (uint64_t)offset;
    uint32_t bytes = 1U << size;

    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, !is_load, elr, false);
        return false;
    }

    uintptr_t hva = 0;
    bool is_mmio = false;
    uint64_t pa = 0;
    const softtlb_entry_t *tlb = is_load ? tlb_r : tlb_w;
    if (!mmu_tlb_lookup(tlb, cpu->tlb_asid_salt, va, bytes, &hva)) {
        mmu_translation_t tr;
        if (!resolve_atomic_addr(cpu, va, is_load ? MMU_ACCESS_READ : MMU_ACCESS_WRITE, !is_load, elr, &tr)) {
            return false;
        }
        hva = tr.hva;
        pa = tr.pa;
        is_mmio = tr.is_mmio;
    }

    if (is_load) {
        uint64_t val = 0;
        if (is_mmio) {
            cpu_fp_leave_guest(cpu);
            val = vmm_mmio_read(cpu, pa, bytes);
            atomic_thread_fence(memory_order_seq_cst);
        } else if (size == 0U) {
            val = __atomic_load_n((const uint8_t *)hva, __ATOMIC_SEQ_CST);
        } else if (size == 1U) {
            val = __atomic_load_n((const uint16_t *)hva, __ATOMIC_SEQ_CST);
        } else if (size == 2U) {
            val = __atomic_load_n((const uint32_t *)hva, __ATOMIC_SEQ_CST);
        } else {
            val = __atomic_load_n((const uint64_t *)hva, __ATOMIC_SEQ_CST);
        }
        if (opc == 2U) {
            val = (uint64_t)cpu_sext(val, 8U << size);
        } else if (opc == 3U) {
            val = (uint32_t)cpu_sext(val, 8U << size);
        }
        cpu_set_x(cpu, rt, val);
    } else {
        uint64_t val = cpu_get_x(cpu, rt);
        if (is_mmio) {
            atomic_thread_fence(memory_order_seq_cst);
            cpu_fp_leave_guest(cpu);
            vmm_mmio_write(cpu, pa, bytes, val);
        } else if (size == 0U) {
            __atomic_store_n((uint8_t *)hva, (uint8_t)val, __ATOMIC_SEQ_CST);
        } else if (size == 1U) {
            __atomic_store_n((uint16_t *)hva, (uint16_t)val, __ATOMIC_SEQ_CST);
        } else if (size == 2U) {
            __atomic_store_n((uint32_t *)hva, (uint32_t)val, __ATOMIC_SEQ_CST);
        } else {
            __atomic_store_n((uint64_t *)hva, val, __ATOMIC_SEQ_CST);
        }
        if (!is_mmio) {
            cpu_notify_wfe_write(cpu, hva);
        }
    }
    return true;
}

void cpu_op_ldst_ord(vcpu_t *cpu, const ir_insn_t *ir,
                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    if (ordered_access(cpu, ir, tlb_r, tlb_w, word, 0, (word >> 22) & 1U)) {
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
}

void cpu_op_ldapstl_unscaled(vcpu_t *cpu, const ir_insn_t *ir,
                             softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    if (ordered_access(cpu, ir, tlb_r, tlb_w, word, cpu_sext((word >> 12) & 0x1FFU, 9), (word >> 22) & 3U)) {
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
}

void cpu_op_ldst_exclr(vcpu_t *cpu, const ir_insn_t *ir,
                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t size = (word >> 30) & 3U;
    bool is_load = ((word >> 22) & 1U) != 0;
    bool ordered = ((word >> 15) & 1U) != 0;
    uint32_t rs = (word >> 16) & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t va = cpu_get_xsp(cpu, rn);
    uint32_t bytes = 1U << size;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return;
    }
    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, !is_load, elr, false);
        return;
    }

    mmu_translation_t tr;
    if (!resolve_atomic_addr(cpu, va, is_load ? MMU_ACCESS_READ : MMU_ACCESS_WRITE, !is_load, elr, &tr)) {
        return;
    }

    if (is_load) {
        uint64_t val = 0;
        int mo = ordered ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED;
        if (tr.is_mmio) {
            cpu_fp_leave_guest(cpu);
            val = vmm_mmio_read(cpu, tr.pa, bytes);
        } else if (size == 0U) {
            val = __atomic_load_n((uint8_t *)tr.hva, mo);
        } else if (size == 1U) {
            val = __atomic_load_n((uint16_t *)tr.hva, mo);
        } else if (size == 2U) {
            val = __atomic_load_n((uint32_t *)tr.hva, mo);
        } else {
            val = __atomic_load_n((uint64_t *)tr.hva, mo);
        }
        cpu->excl_valid = true;
        cpu->excl_pa = tr.pa;
        cpu->excl_hva = tr.is_mmio ? 0 : tr.hva;
        cpu->excl_size = (uint8_t)bytes;
        cpu->excl_val = val;
        cpu_set_x(cpu, rt, val);
    } else {
        bool ok = false;
        if (cpu->excl_valid && cpu->excl_size == bytes &&
            (tr.is_mmio ? cpu->excl_hva == 0 && cpu->excl_pa == tr.pa : cpu->excl_hva == tr.hva)) {
            uint64_t nv = cpu_get_x(cpu, rt);
            int succ = ordered ? __ATOMIC_ACQ_REL : __ATOMIC_RELAXED;
            if (tr.is_mmio) {
                cpu_fp_leave_guest(cpu);
                vmm_mmio_write(cpu, tr.pa, bytes, nv);
                ok = true;
            } else if (size == 0U) {
                uint8_t exp = (uint8_t)cpu->excl_val;
                ok = __atomic_compare_exchange_n((uint8_t *)tr.hva, &exp, (uint8_t)nv, false, succ, __ATOMIC_RELAXED);
            } else if (size == 1U) {
                uint16_t exp = (uint16_t)cpu->excl_val;
                ok = __atomic_compare_exchange_n((uint16_t *)tr.hva, &exp, (uint16_t)nv, false, succ, __ATOMIC_RELAXED);
            } else if (size == 2U) {
                uint32_t exp = (uint32_t)cpu->excl_val;
                ok = __atomic_compare_exchange_n((uint32_t *)tr.hva, &exp, (uint32_t)nv, false, succ, __ATOMIC_RELAXED);
            } else {
                uint64_t exp = (uint64_t)cpu->excl_val;
                ok = __atomic_compare_exchange_n((uint64_t *)tr.hva, &exp, nv, false, succ, __ATOMIC_RELAXED);
            }
            if (ok && !tr.is_mmio) {
                cpu_notify_wfe_write(cpu, tr.hva);
            }
        }
        cpu->excl_valid = false;
        cpu->excl_hva = 0;
        cpu_set_x(cpu, rs, ok ? 0ULL : 1ULL);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_ldst_exclp(vcpu_t *cpu, const ir_insn_t *ir,
                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    bool is_64 = ((word >> 30) & 1U) != 0;
    bool is_load = ((word >> 22) & 1U) != 0;
    bool ordered = ((word >> 15) & 1U) != 0;
    uint32_t rs = (word >> 16) & 31U;
    uint32_t rt2 = (word >> 10) & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t va = cpu_get_xsp(cpu, rn);
    uint32_t bytes = is_64 ? 16U : 8U;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return;
    }
    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, !is_load, elr, false);
        return;
    }

    mmu_translation_t tr;
    if (!resolve_atomic_addr(cpu, va, is_load ? MMU_ACCESS_READ : MMU_ACCESS_WRITE, !is_load, elr, &tr)) {
        return;
    }

    if (is_load) {
        int mo = ordered ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED;
        __uint128_t val = 0;
        if (tr.is_mmio) {
            cpu_fp_leave_guest(cpu);
            val = vmm_mmio_read(cpu, tr.pa, 8U);
            if (is_64) {
                val |= (__uint128_t)vmm_mmio_read(cpu, tr.pa + 8U, 8U) << 64;
            }
        } else if (is_64) {
            val = __atomic_load_n((__uint128_t *)tr.hva, mo);
        } else {
            val = __atomic_load_n((uint64_t *)tr.hva, mo);
        }
        if (is_64) {
            cpu_set_x(cpu, rt, (uint64_t)val);
            cpu_set_x(cpu, rt2, (uint64_t)(val >> 64));
        } else {
            cpu_set_x(cpu, rt, (uint32_t)val);
            cpu_set_x(cpu, rt2, (uint32_t)(val >> 32));
        }
        cpu->excl_valid = true;
        cpu->excl_pa = tr.pa;
        cpu->excl_hva = tr.is_mmio ? 0 : tr.hva;
        cpu->excl_size = (uint8_t)bytes;
        cpu->excl_val = val;
    } else {
        bool ok = false;
        if (cpu->excl_valid && cpu->excl_pa == tr.pa && cpu->excl_size == bytes) {
            int succ = ordered ? __ATOMIC_ACQ_REL : __ATOMIC_RELAXED;
            __uint128_t nv = is_64 ? (__uint128_t)cpu_get_x(cpu, rt) | ((__uint128_t)cpu_get_x(cpu, rt2) << 64)
                                   : (uint64_t)(uint32_t)cpu_get_x(cpu, rt) |
                                         ((uint64_t)(uint32_t)cpu_get_x(cpu, rt2) << 32);
            if (tr.is_mmio) {
                cpu_fp_leave_guest(cpu);
                vmm_mmio_write(cpu, tr.pa, 8U, (uint64_t)nv);
                if (is_64) {
                    vmm_mmio_write(cpu, tr.pa + 8U, 8U, (uint64_t)(nv >> 64));
                }
                ok = true;
            } else if (is_64) {
                __uint128_t exp = cpu->excl_val;
                ok = __atomic_compare_exchange_n((__uint128_t *)tr.hva, &exp, nv, false, succ, __ATOMIC_RELAXED);
            } else {
                uint64_t exp = (uint64_t)cpu->excl_val;
                ok = __atomic_compare_exchange_n((uint64_t *)tr.hva, &exp, (uint64_t)nv, false, succ, __ATOMIC_RELAXED);
            }
            if (ok && !tr.is_mmio) {
                cpu_notify_wfe_write(cpu, tr.hva);
            }
        }
        cpu->excl_valid = false;
        cpu->excl_hva = 0;
        cpu_set_x(cpu, rs, ok ? 0ULL : 1ULL);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static int lse_mem_order(bool acquire, bool release) {
    if (acquire && release) return __ATOMIC_ACQ_REL;
    if (acquire) return __ATOMIC_ACQUIRE;
    if (release) return __ATOMIC_RELEASE;
    return __ATOMIC_RELAXED;
}

void cpu_op_lse_cas(vcpu_t *cpu, const ir_insn_t *ir,
                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t size = (word >> 30) & 3U;
    bool acq = ((word >> 22) & 1U) != 0;
    bool rel = ((word >> 15) & 1U) != 0;
    uint32_t rs = (word >> 16) & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t va = cpu_get_xsp(cpu, rn);
    uint32_t bytes = 1U << size;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return;
    }

    uintptr_t hva = 0;
    bool is_mmio = false;
    uint64_t pa = 0;
    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, true, elr, false);
        return;
    }
    if (!mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, bytes, &hva)) {
        mmu_translation_t tr;
        if (!resolve_atomic_addr(cpu, va, MMU_ACCESS_WRITE, true, elr, &tr)) {
            return;
        }
        hva = tr.hva;
        pa = tr.pa;
        is_mmio = tr.is_mmio;
    }

    int mo = lse_mem_order(acq, rel);
    int fmo = acq ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED;
    uint64_t cmp = cpu_get_x(cpu, rs);
    uint64_t nv = cpu_get_x(cpu, rt);
    uint64_t old = 0;

    if (is_mmio) {
        cpu_fp_leave_guest(cpu);
        old = vmm_mmio_read(cpu, pa, bytes);
        uint64_t mask = bytes == 8U ? UINT64_MAX : ((1ULL << (bytes * 8U)) - 1ULL);
        if ((old & mask) == (cmp & mask)) {
            vmm_mmio_write(cpu, pa, bytes, nv & mask);
        }
    } else if (size == 0U) {
        uint8_t exp = (uint8_t)cmp;
        __atomic_compare_exchange_n((uint8_t *)hva, &exp, (uint8_t)nv, false, mo, fmo);
        old = exp;
    } else if (size == 1U) {
        uint16_t exp = (uint16_t)cmp;
        __atomic_compare_exchange_n((uint16_t *)hva, &exp, (uint16_t)nv, false, mo, fmo);
        old = exp;
    } else if (size == 2U) {
        uint32_t exp = (uint32_t)cmp;
        __atomic_compare_exchange_n((uint32_t *)hva, &exp, (uint32_t)nv, false, mo, fmo);
        old = exp;
    } else {
        uint64_t exp = cmp;
        __atomic_compare_exchange_n((uint64_t *)hva, &exp, nv, false, mo, fmo);
        old = exp;
    }

    if (!is_mmio) {
        cpu_notify_wfe_write(cpu, hva);
    }
    cpu_set_x(cpu, rs, old);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_lse_casp(vcpu_t *cpu, const ir_insn_t *ir,
                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    bool is_64 = ((word >> 30) & 1U) != 0;
    bool acq = ((word >> 22) & 1U) != 0;
    bool rel = ((word >> 15) & 1U) != 0;
    uint32_t rs = (word >> 16) & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if ((rs & 1U) != 0 || (rt & 1U) != 0) {
        vcpu_raise_exception(cpu, 1U << 25, 0, elr);
        return;
    }
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, rn);
    uint32_t bytes = is_64 ? 16U : 8U;
    uintptr_t hva = 0;
    bool is_mmio = false;
    uint64_t pa = 0;
    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, true, elr, false);
        return;
    }
    if (!mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, bytes, &hva)) {
        mmu_translation_t tr;
        if (!resolve_atomic_addr(cpu, va, MMU_ACCESS_WRITE, true, elr, &tr)) {
            return;
        }
        hva = tr.hva;
        pa = tr.pa;
        is_mmio = tr.is_mmio;
    }
    int mo = lse_mem_order(acq, rel);
    int fmo = acq ? __ATOMIC_ACQUIRE : __ATOMIC_RELAXED;
    __uint128_t exp, nv;
    if (is_64) {
        exp = (__uint128_t)cpu_get_x(cpu, rs) | ((__uint128_t)cpu_get_x(cpu, rs + 1U) << 64);
        nv = (__uint128_t)cpu_get_x(cpu, rt) | ((__uint128_t)cpu_get_x(cpu, rt + 1U) << 64);
    } else {
        exp = (uint64_t)(uint32_t)cpu_get_x(cpu, rs) | ((uint64_t)(uint32_t)cpu_get_x(cpu, rs + 1U) << 32);
        nv = (uint64_t)(uint32_t)cpu_get_x(cpu, rt) | ((uint64_t)(uint32_t)cpu_get_x(cpu, rt + 1U) << 32);
    }
    if (is_mmio) {
        cpu_fp_leave_guest(cpu);
        __uint128_t old = vmm_mmio_read(cpu, pa, 8U);
        if (is_64) {
            old |= (__uint128_t)vmm_mmio_read(cpu, pa + 8U, 8U) << 64;
        }
        if (old == exp) {
            vmm_mmio_write(cpu, pa, 8U, (uint64_t)nv);
            if (is_64) {
                vmm_mmio_write(cpu, pa + 8U, 8U, (uint64_t)(nv >> 64));
            }
        }
        exp = old;
    } else if (is_64) {
        __atomic_compare_exchange_n((__uint128_t *)hva, &exp, nv, false, mo, fmo);
    } else {
        uint64_t exp64 = (uint64_t)exp;
        __atomic_compare_exchange_n((uint64_t *)hva, &exp64, (uint64_t)nv, false, mo, fmo);
        exp = exp64;
    }
    if (is_64) {
        cpu_set_x(cpu, rs, (uint64_t)exp);
        cpu_set_x(cpu, rs + 1U, (uint64_t)(exp >> 64));
    } else {
        cpu_set_x(cpu, rs, (uint32_t)exp);
        cpu_set_x(cpu, rs + 1U, (uint32_t)(exp >> 32));
    }
    if (!is_mmio) {
        cpu_notify_wfe_write(cpu, hva);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static uint64_t lse_alu(uint32_t opc, uint32_t bits, uint64_t old_val, uint64_t op_val) {
    switch (opc) {
    case 0U: return old_val + op_val;
    case 1U: return old_val & ~op_val;
    case 2U: return old_val ^ op_val;
    case 3U: return old_val | op_val;
    case 4U: {
        int64_t a = cpu_sext(old_val, bits);
        int64_t b = cpu_sext(op_val, bits);
        return (uint64_t)(a > b ? a : b);
    }
    case 5U: {
        int64_t a = cpu_sext(old_val, bits);
        int64_t b = cpu_sext(op_val, bits);
        return (uint64_t)(a < b ? a : b);
    }
    case 6U: {
        uint64_t m = bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL);
        return (old_val & m) > (op_val & m) ? old_val : op_val;
    }
    case 7U: {
        uint64_t m = bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL);
        return (old_val & m) < (op_val & m) ? old_val : op_val;
    }
    default: return op_val;
    }
}

#define LSE_MINMAX_ORDERED(mnemonic, size, reg, type)                                                     \
    do {                                                                                                  \
        type operand = (type)op_val, result;                                                              \
        if (acq && rel) {                                                                                 \
            __asm__ volatile(mnemonic "al" size " %" reg "1, %" reg "0, [%2]"                               \
                             : "=r"(result) : "r"(operand), "r"(hva) : "memory");                          \
        } else if (acq) {                                                                                 \
            __asm__ volatile(mnemonic "a" size " %" reg "1, %" reg "0, [%2]"                                \
                             : "=r"(result) : "r"(operand), "r"(hva) : "memory");                          \
        } else if (rel) {                                                                                 \
            __asm__ volatile(mnemonic "l" size " %" reg "1, %" reg "0, [%2]"                                \
                             : "=r"(result) : "r"(operand), "r"(hva) : "memory");                          \
        } else {                                                                                          \
            __asm__ volatile(mnemonic size " %" reg "1, %" reg "0, [%2]"                                    \
                             : "=r"(result) : "r"(operand), "r"(hva) : "memory");                          \
        }                                                                                                 \
        old = result;                                                                                     \
    } while (0)

#define LSE_MINMAX(mnemonic)                                                                              \
    do {                                                                                                  \
        if (size == 0U) LSE_MINMAX_ORDERED(mnemonic, "b", "w", uint8_t);                                   \
        else if (size == 1U) LSE_MINMAX_ORDERED(mnemonic, "h", "w", uint16_t);                             \
        else if (size == 2U) LSE_MINMAX_ORDERED(mnemonic, "", "w", uint32_t);                              \
        else LSE_MINMAX_ORDERED(mnemonic, "", "x", uint64_t);                                              \
    } while (0)

__attribute__((target("lse"), noinline))
static uint64_t lse_minmax_hw(uint32_t opc, uint32_t size, bool acq, bool rel,
                              uintptr_t hva, uint64_t op_val) {
    uint64_t old = 0;
    switch (opc) {
    case 4U: LSE_MINMAX("ldsmax"); break;
    case 5U: LSE_MINMAX("ldsmin"); break;
    case 6U: LSE_MINMAX("ldumax"); break;
    default: LSE_MINMAX("ldumin"); break;
    }
    return old;
}

void cpu_op_lse_memop(vcpu_t *cpu, const ir_insn_t *ir,
                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t size = (word >> 30) & 3U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;
    uint64_t va = cpu_get_xsp(cpu, rn);
    uint32_t bytes = 1U << size;
    uint64_t elr = cpu_ir_pc(cpu, ir);
    if (!cpu_check_sp_align(cpu, rn, elr)) {
        return;
    }

    if ((word & 0x00FFFC00U) == 0x00BFC000U) {
        if ((va & (bytes - 1U)) != 0) {
            vcpu_raise_abort(cpu, va, NULL, false, elr, false);
            return;
        }
        uintptr_t hva = 0;
        bool is_mmio = false;
        uint64_t pa = 0;
        if (!mmu_tlb_lookup(tlb_r, cpu->tlb_asid_salt, va, bytes, &hva)) {
            mmu_translation_t tr;
            if (!resolve_atomic_addr(cpu, va, MMU_ACCESS_READ, false, elr, &tr)) {
                return;
            }
            hva = tr.hva;
            pa = tr.pa;
            is_mmio = tr.is_mmio;
        }
        uint64_t val = 0;
        if (is_mmio) {
            cpu_fp_leave_guest(cpu);
            val = vmm_mmio_read(cpu, pa, bytes);
            atomic_thread_fence(memory_order_acquire);
        } else if (size == 0U) {
            val = __atomic_load_n((const uint8_t *)hva, __ATOMIC_ACQUIRE);
        } else if (size == 1U) {
            val = __atomic_load_n((const uint16_t *)hva, __ATOMIC_ACQUIRE);
        } else if (size == 2U) {
            val = __atomic_load_n((const uint32_t *)hva, __ATOMIC_ACQUIRE);
        } else {
            val = __atomic_load_n((const uint64_t *)hva, __ATOMIC_ACQUIRE);
        }
        cpu_set_x(cpu, rt, val);
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }

    bool acq = ((word >> 23) & 1U) != 0 && rt != 31U;
    bool rel = ((word >> 22) & 1U) != 0;
    uint32_t rs = (word >> 16) & 31U;
    uint32_t opc = (word >> 12) & 7U;
    bool is_swp = ((word >> 15) & 1U) != 0;

    uintptr_t hva = 0;
    bool is_mmio = false;
    uint64_t pa = 0;
    if ((va & (bytes - 1U)) != 0) {
        vcpu_raise_abort(cpu, va, NULL, true, elr, false);
        return;
    }
    if (!mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, bytes, &hva)) {
        mmu_translation_t tr;
        if (!resolve_atomic_addr(cpu, va, MMU_ACCESS_WRITE, true, elr, &tr)) {
            return;
        }
        hva = tr.hva;
        pa = tr.pa;
        is_mmio = tr.is_mmio;
    }

    int mo = lse_mem_order(acq, rel);
    uint64_t op_val = cpu_get_x(cpu, rs);
    uint64_t old = 0;

    if (is_mmio) {
        cpu_fp_leave_guest(cpu);
        old = vmm_mmio_read(cpu, pa, bytes);
        uint64_t nv = is_swp ? op_val : lse_alu(opc, bytes * 8U, old, op_val);
        vmm_mmio_write(cpu, pa, bytes, nv);
    } else if (is_swp) {
        if (size == 0U) old = __atomic_exchange_n((uint8_t *)hva, (uint8_t)op_val, mo);
        else if (size == 1U) old = __atomic_exchange_n((uint16_t *)hva, (uint16_t)op_val, mo);
        else if (size == 2U) old = __atomic_exchange_n((uint32_t *)hva, (uint32_t)op_val, mo);
        else old = __atomic_exchange_n((uint64_t *)hva, op_val, mo);
    } else if (opc <= 3U) {
        switch (opc) {
        case 0U:
            if (size == 0U) old = __atomic_fetch_add((uint8_t *)hva, (uint8_t)op_val, mo);
            else if (size == 1U) old = __atomic_fetch_add((uint16_t *)hva, (uint16_t)op_val, mo);
            else if (size == 2U) old = __atomic_fetch_add((uint32_t *)hva, (uint32_t)op_val, mo);
            else old = __atomic_fetch_add((uint64_t *)hva, op_val, mo);
            break;
        case 1U:
            if (size == 0U) old = __atomic_fetch_and((uint8_t *)hva, (uint8_t)~op_val, mo);
            else if (size == 1U) old = __atomic_fetch_and((uint16_t *)hva, (uint16_t)~op_val, mo);
            else if (size == 2U) old = __atomic_fetch_and((uint32_t *)hva, (uint32_t)~op_val, mo);
            else old = __atomic_fetch_and((uint64_t *)hva, ~op_val, mo);
            break;
        case 2U:
            if (size == 0U) old = __atomic_fetch_xor((uint8_t *)hva, (uint8_t)op_val, mo);
            else if (size == 1U) old = __atomic_fetch_xor((uint16_t *)hva, (uint16_t)op_val, mo);
            else if (size == 2U) old = __atomic_fetch_xor((uint32_t *)hva, (uint32_t)op_val, mo);
            else old = __atomic_fetch_xor((uint64_t *)hva, op_val, mo);
            break;
        default:
            if (size == 0U) old = __atomic_fetch_or((uint8_t *)hva, (uint8_t)op_val, mo);
            else if (size == 1U) old = __atomic_fetch_or((uint16_t *)hva, (uint16_t)op_val, mo);
            else if (size == 2U) old = __atomic_fetch_or((uint32_t *)hva, (uint32_t)op_val, mo);
            else old = __atomic_fetch_or((uint64_t *)hva, op_val, mo);
            break;
        }
    } else if (cpu_host_features() & CPU_HOST_FEAT_LSE) {
        old = lse_minmax_hw(opc, size, acq, rel, hva, op_val);
    } else {
        if (size == 0U) {
            uint8_t cur = __atomic_load_n((uint8_t *)hva, __ATOMIC_RELAXED);
            while (!__atomic_compare_exchange_n((uint8_t *)hva, &cur, (uint8_t)lse_alu(opc, 8U, cur, op_val),
                                                false, mo, __ATOMIC_RELAXED)) {}
            old = cur;
        } else if (size == 1U) {
            uint16_t cur = __atomic_load_n((uint16_t *)hva, __ATOMIC_RELAXED);
            while (!__atomic_compare_exchange_n((uint16_t *)hva, &cur, (uint16_t)lse_alu(opc, 16U, cur, op_val),
                                                false, mo, __ATOMIC_RELAXED)) {}
            old = cur;
        } else if (size == 2U) {
            uint32_t cur = __atomic_load_n((uint32_t *)hva, __ATOMIC_RELAXED);
            while (!__atomic_compare_exchange_n((uint32_t *)hva, &cur, (uint32_t)lse_alu(opc, 32U, cur, op_val),
                                                false, mo, __ATOMIC_RELAXED)) {}
            old = cur;
        } else {
            uint64_t cur = __atomic_load_n((uint64_t *)hva, __ATOMIC_RELAXED);
            while (!__atomic_compare_exchange_n((uint64_t *)hva, &cur, lse_alu(opc, 64U, cur, op_val),
                                                false, mo, __ATOMIC_RELAXED)) {}
            old = cur;
        }
    }

    if (!is_mmio) {
        cpu_notify_wfe_write(cpu, hva);
    }
    cpu_set_x(cpu, rt, old);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static inline bool mops_is_enabled(const vcpu_t *cpu) {
    return cpu->el >= 1 || ((cpu->sctlr_el1 & (1ULL << 33)) != 0);
}

static void mops_raise_wrong_option(vcpu_t *cpu, bool is_set, uint32_t op2, uint32_t stage,
                                    uint32_t rd, uint32_t rs, uint32_t rn, uint64_t elr) {
    uint32_t iss = (is_set ? (1U << 24) : 0U) |
                   ((op2 & 0xFU) << 19) |
                   ((stage == 2U) ? (1U << 18) : 0U) |
                   (1U << 17) |
                   (1U << 16) |
                   ((rd & 31U) << 10) |
                   ((rs & 31U) << 5) |
                   (rn & 31U);
    vcpu_raise_exception(cpu, (0x27U << 26) | (1U << 25) | iss, 0, elr);
}

static inline bool mops_resolve_page(vcpu_t *cpu, const softtlb_entry_t *bank, uint64_t va,
                                     mmu_access_t acc, uint8_t access_el,
                                     uintptr_t *hva_out, mmu_translation_t *tr_out) {
    if (__builtin_expect(mmu_tlb_lookup_aligned(bank, cpu->tlb_asid_salt, va, hva_out), 1)) {
        tr_out->is_mmio = false;
        return true;
    }
    if (!mmu_translate_and_fill(cpu, va, acc, access_el, tr_out, NULL)) {
        return false;
    }
    *hva_out = tr_out->hva;
    return true;
}

void cpu_op_mops_set(vcpu_t *cpu, const ir_insn_t *ir,
                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t stage = ir->op & 3U;
    uint32_t op2 = (ir->op >> 4) & 3U;
    uint32_t rd = ir->rd;
    uint32_t rn = ir->rn;
    uint32_t rs = ir->rm;

    if (__builtin_expect(!mops_is_enabled(cpu), 0)) {
        uint64_t elr = cpu_ir_pc(cpu, ir);
        vcpu_raise_exception(cpu, 1U << 25, 0, elr);
        return;
    }

    uint64_t xn = cpu->x[rn];
    if (stage != 0U) {
        if (__builtin_expect(xn == 0, 1)) {
            CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
        }
        if ((cpu->nzcv & (1U << 29)) != 0) {
            mops_raise_wrong_option(cpu, true, op2, stage, rd, rs, rn, cpu_ir_pc(cpu, ir));
            return;
        }
    }

    uint8_t fill = (uint8_t)cpu_get_x(cpu, rs);
    uint8_t w_el = (op2 & 1U) ? 0U : cpu->el;
    const softtlb_entry_t *w_bank = cpu->tlb[w_el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    uint64_t dst, rem;
    if (stage == 0U) {
        rem = xn > (uint64_t)INT64_MAX ? (uint64_t)INT64_MAX : xn;
        dst = cpu->x[rd];
    } else {
        rem = (uint64_t)(-(int64_t)xn);
        dst = cpu->x[rd] + xn;
    }

    uint64_t elr = cpu_ir_pc(cpu, ir);
    uint32_t pages_done = 0;
    while (rem > 0) {
        if (__builtin_expect((pages_done & 15U) == 15U && vcpu_check_interrupts(cpu), 0)) {
            if (stage == 0U) {
                cpu->x[rd] = dst;
                cpu->x[rn] = rem;
            } else {
                cpu->x[rn] = (uint64_t)(-(int64_t)rem);
            }
            cpu->pc = elr;
            return;
        }
        size_t chunk = VMM_PAGE_SIZE - (size_t)(dst & VMM_PAGE_MASK);
        if (chunk > rem) {
            chunk = (size_t)rem;
        }
        uintptr_t dst_hva = 0;
        mmu_translation_t tr;
        if (__builtin_expect(!mops_resolve_page(cpu, w_bank, dst, MMU_ACCESS_WRITE, w_el, &dst_hva, &tr), 0)) {
            if (stage == 0U) {
                cpu->x[rd] = dst;
                cpu->x[rn] = rem;
            } else {
                cpu->x[rn] = (uint64_t)(-(int64_t)rem);
            }
            vcpu_raise_abort(cpu, dst, &tr, true, elr, false);
            return;
        }
        if (__builtin_expect(tr.is_mmio, 0)) {
            cpu_fp_leave_guest(cpu);
            for (size_t i = 0; i < chunk; i++) {
                vmm_mmio_write(cpu, tr.pa + i, 1, fill);
            }
        } else {
            memset((void *)dst_hva, fill, chunk);
        }
        dst += chunk;
        rem -= chunk;
        pages_done++;
    }

    if (stage == 0U) {
        cpu->x[rd] = dst;
        cpu->x[rn] = 0;
        cpu->nzcv = 0;
    } else {
        cpu->x[rn] = 0;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_mops_cpy(vcpu_t *cpu, const ir_insn_t *ir,
                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t stage = ir->op & 3U;
    bool is_move = ((ir->op >> 2) & 1U) != 0;
    uint32_t op2 = (ir->op >> 4) & 15U;
    uint32_t rd = ir->rd;
    uint32_t rn = ir->rn;
    uint32_t rs = ir->rm;

    if (__builtin_expect(!mops_is_enabled(cpu), 0)) {
        uint64_t elr = cpu_ir_pc(cpu, ir);
        vcpu_raise_exception(cpu, 1U << 25, 0, elr);
        return;
    }

    uint64_t xn = cpu->x[rn];
    if (stage != 0U) {
        if (__builtin_expect(xn == 0, 1)) {
            CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
        }
        if ((cpu->nzcv & (1U << 29)) != 0) {
            mops_raise_wrong_option(cpu, false, op2, stage, rd, rs, rn, cpu_ir_pc(cpu, ir));
            return;
        }
    }

    uint8_t w_el = (op2 & 1U) ? 0U : cpu->el;
    uint8_t r_el = (op2 & 2U) ? 0U : cpu->el;
    const softtlb_entry_t *w_bank = cpu->tlb[w_el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    const softtlb_entry_t *r_bank = cpu->tlb[r_el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    uint64_t elr = cpu_ir_pc(cpu, ir);

    bool backwards = false;
    uint64_t dst, src, rem;
    if (stage == 0U) {
        dst = cpu->x[rd];
        src = cpu->x[rs];
        if (is_move) {
            rem = xn > 0x007FFFFFFFFFFFFFULL ? 0x007FFFFFFFFFFFFFULL : xn;
            uint64_t diff = (dst - src) & 0x00FFFFFFFFFFFFFFULL;
            if (diff > 0 && diff < rem) {
                backwards = true;
            }
        } else {
            rem = xn > (uint64_t)INT64_MAX ? (uint64_t)INT64_MAX : xn;
        }
    } else {
        if (is_move && (int64_t)xn > 0) {
            backwards = true;
            dst = cpu->x[rd];
            src = cpu->x[rs];
            rem = xn;
        } else {
            rem = (uint64_t)(-(int64_t)xn);
            dst = cpu->x[rd] + xn;
            src = cpu->x[rs] + xn;
        }
    }

    uint32_t pages_done = 0;
    if (!backwards) {
        while (rem > 0) {
            if (__builtin_expect((pages_done & 15U) == 15U && vcpu_check_interrupts(cpu), 0)) {
                if (stage == 0U) {
                    cpu->x[rd] = dst;
                    cpu->x[rs] = src;
                    cpu->x[rn] = rem;
                } else {
                    cpu->x[rn] = (uint64_t)(-(int64_t)rem);
                }
                cpu->pc = elr;
                return;
            }
            size_t cd = VMM_PAGE_SIZE - (size_t)(dst & VMM_PAGE_MASK);
            size_t cs = VMM_PAGE_SIZE - (size_t)(src & VMM_PAGE_MASK);
            size_t chunk = cd < cs ? cd : cs;
            if (chunk > rem) {
                chunk = (size_t)rem;
            }
            uintptr_t src_hva = 0, dst_hva = 0;
            mmu_translation_t trs, trd;
            if (__builtin_expect(!mops_resolve_page(cpu, r_bank, src, MMU_ACCESS_READ, r_el, &src_hva, &trs), 0)) {
                if (stage == 0U) {
                    cpu->x[rd] = dst;
                    cpu->x[rs] = src;
                    cpu->x[rn] = rem;
                } else {
                    cpu->x[rn] = (uint64_t)(-(int64_t)rem);
                }
                vcpu_raise_abort(cpu, src, &trs, false, elr, false);
                return;
            }
            if (__builtin_expect(!mops_resolve_page(cpu, w_bank, dst, MMU_ACCESS_WRITE, w_el, &dst_hva, &trd), 0)) {
                if (stage == 0U) {
                    cpu->x[rd] = dst;
                    cpu->x[rs] = src;
                    cpu->x[rn] = rem;
                } else {
                    cpu->x[rn] = (uint64_t)(-(int64_t)rem);
                }
                vcpu_raise_abort(cpu, dst, &trd, true, elr, false);
                return;
            }
            if (__builtin_expect(trs.is_mmio || trd.is_mmio, 0)) {
                cpu_fp_leave_guest(cpu);
                for (size_t i = 0; i < chunk; i++) {
                    uint8_t b = trs.is_mmio
                                    ? (uint8_t)vmm_mmio_read(cpu, trs.pa + i, 1)
                                    : ((const uint8_t *)src_hva)[i];
                    if (trd.is_mmio) {
                        vmm_mmio_write(cpu, trd.pa + i, 1, b);
                    } else {
                        ((uint8_t *)dst_hva)[i] = b;
                    }
                }
            } else {
                memmove((void *)dst_hva, (const void *)src_hva, chunk);
            }
            dst += chunk;
            src += chunk;
            rem -= chunk;
            pages_done++;
        }
        if (stage == 0U) {
            cpu->x[rd] = dst;
            cpu->x[rs] = src;
            cpu->x[rn] = 0;
            cpu->nzcv = 0;
        } else {
            cpu->x[rn] = 0;
        }
    } else {
        uint64_t dst_end = dst + rem - 1ULL;
        uint64_t src_end = src + rem - 1ULL;
        while (rem > 0) {
            if (__builtin_expect((pages_done & 15U) == 15U && vcpu_check_interrupts(cpu), 0)) {
                cpu->x[rn] = rem;
                cpu->pc = elr;
                return;
            }
            size_t cd = (size_t)(dst_end & VMM_PAGE_MASK) + 1U;
            size_t cs = (size_t)(src_end & VMM_PAGE_MASK) + 1U;
            size_t chunk = cd < cs ? cd : cs;
            if (chunk > rem) {
                chunk = (size_t)rem;
            }
            uint64_t s_lo = src_end - (uint64_t)(chunk - 1U);
            uint64_t d_lo = dst_end - (uint64_t)(chunk - 1U);
            uintptr_t src_hva = 0, dst_hva = 0;
            mmu_translation_t trs, trd;
            if (__builtin_expect(!mops_resolve_page(cpu, r_bank, s_lo, MMU_ACCESS_READ, r_el, &src_hva, &trs), 0)) {
                cpu->x[rn] = rem;
                vcpu_raise_abort(cpu, src_end, &trs, false, elr, false);
                return;
            }
            if (__builtin_expect(!mops_resolve_page(cpu, w_bank, d_lo, MMU_ACCESS_WRITE, w_el, &dst_hva, &trd), 0)) {
                cpu->x[rn] = rem;
                vcpu_raise_abort(cpu, dst_end, &trd, true, elr, false);
                return;
            }
            if (__builtin_expect(trs.is_mmio || trd.is_mmio, 0)) {
                cpu_fp_leave_guest(cpu);
                for (size_t i = chunk; i > 0; i--) {
                    size_t off = i - 1U;
                    uint8_t b = trs.is_mmio
                                    ? (uint8_t)vmm_mmio_read(cpu, trs.pa + off, 1)
                                    : ((const uint8_t *)src_hva)[off];
                    if (trd.is_mmio) {
                        vmm_mmio_write(cpu, trd.pa + off, 1, b);
                    } else {
                        ((uint8_t *)dst_hva)[off] = b;
                    }
                }
            } else {
                memmove((void *)dst_hva, (const void *)src_hva, chunk);
            }
            dst_end -= chunk;
            src_end -= chunk;
            rem -= chunk;
            pages_done++;
        }
        cpu->x[rn] = 0;
        if (stage == 0U) {
            cpu->nzcv = 0;
        }
    }

    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static bool simd_read_elem(vcpu_t *cpu, const ir_insn_t *ir, const softtlb_entry_t *tlb_r,
                           uint64_t va, uint32_t ebytes, uint64_t *out) {
    if (ebytes == 1U) { uint8_t v = 0; bool ok = cpu_read_8(cpu, ir, 0, tlb_r, va, &v); *out = v; return ok; }
    if (ebytes == 2U) { uint16_t v = 0; bool ok = cpu_read_16(cpu, ir, 0, tlb_r, va, &v); *out = v; return ok; }
    if (ebytes == 4U) { uint32_t v = 0; bool ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &v); *out = v; return ok; }
    return cpu_read_64(cpu, ir, 0, tlb_r, va, out);
}

static bool simd_write_elem(vcpu_t *cpu, const ir_insn_t *ir, const softtlb_entry_t *tlb_w,
                            uint64_t va, uint32_t ebytes, uint64_t val) {
    if (ebytes == 1U) return cpu_write_8(cpu, ir, 0, tlb_w, va, (uint8_t)val);
    if (ebytes == 2U) return cpu_write_16(cpu, ir, 0, tlb_w, va, (uint16_t)val);
    if (ebytes == 4U) return cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)val);
    return cpu_write_64(cpu, ir, 0, tlb_w, va, val);
}

void cpu_op_simd_ldst_multi(vcpu_t *cpu, const ir_insn_t *ir,
                            softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    uint32_t word = (uint32_t)ir->target_or_imm;
    bool q = ((word >> 30) & 1U) != 0;
    bool post = ((word >> 23) & 1U) != 0;
    bool is_load = ((word >> 22) & 1U) != 0;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t opcode = (word >> 12) & 15U;
    uint32_t size = (word >> 10) & 3U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;

    uint32_t rpt;
    uint32_t selem;
    switch (opcode) {
    case 0U: rpt = 1U; selem = 4U; break;
    case 2U: rpt = 4U; selem = 1U; break;
    case 4U: rpt = 1U; selem = 3U; break;
    case 6U: rpt = 3U; selem = 1U; break;
    case 7U: rpt = 1U; selem = 1U; break;
    case 8U: rpt = 1U; selem = 2U; break;
    case 10U: rpt = 2U; selem = 1U; break;
    default:
        vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
        return;
    }

    if (size == 3U && !q && selem != 1U) {
        vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
        return;
    }

    uint32_t datasize = q ? 16U : 8U;
    uint32_t ebytes = 1U << size;
    uint32_t elems = datasize / ebytes;
    if (!cpu_check_sp_align(cpu, rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t addr = cpu_get_xsp(cpu, rn);
    uint64_t offs = 0;
    uint64_t total = (uint64_t)rpt * selem * datasize;
    uintptr_t hva = 0;

    if (selem == 1U && ((addr ^ (addr + total - 1U)) & ~(uint64_t)VMM_PAGE_MASK) == 0 &&
        ((cpu->tlb_asid_salt & MMU_SALT_ALIGN) == 0 || (addr & (ebytes - 1U)) == 0) &&
        mmu_tlb_lookup_aligned(is_load ? tlb_r : tlb_w, cpu->tlb_asid_salt, addr, &hva)) {
        bool aligned = (hva & 7U) == 0U;
        for (uint32_t r = 0; r < rpt; ++r) {
            __uint128_t *reg = &cpu->v[(rt + r) & 31U];
            uintptr_t mem = hva + r * datasize;
            if (!aligned) {
                if (is_load) {
                    *reg = 0;
                    memcpy(reg, (const void *)mem, datasize);
                } else {
                    memcpy((void *)mem, reg, datasize);
                }
            } else if (is_load) {
                uint64_t lo = __atomic_load_n((const uint64_t *)mem, __ATOMIC_RELAXED);
                uint64_t hi = q ? __atomic_load_n((const uint64_t *)mem + 1, __ATOMIC_RELAXED) : 0ULL;
                *reg = ((__uint128_t)hi << 64) | lo;
            } else {
                __uint128_t v = *reg;
                __atomic_store_n((uint64_t *)mem, (uint64_t)v, __ATOMIC_RELAXED);
                if (q) __atomic_store_n((uint64_t *)mem + 1, (uint64_t)(v >> 64), __ATOMIC_RELAXED);
            }
        }
        offs = total;
    } else if (is_load) {
        uint8_t tmp[4][16] = {0};
        uint32_t total_regs = rpt * selem;
        for (uint32_t r = 0; r < rpt; ++r) {
            for (uint32_t e = 0; e < elems; ++e) {
                for (uint32_t s = 0; s < selem; ++s) {
                    uint64_t v = 0;
                    if (!simd_read_elem(cpu, ir, tlb_r, addr + offs, ebytes, &v)) {
                        return;
                    }
                    cpu_lane_set(tmp[r * selem + s], e, ebytes, v);
                    offs += ebytes;
                }
            }
        }
        for (uint32_t i = 0; i < total_regs; ++i) {
            memcpy(&cpu->v[(rt + i) & 31U], tmp[i], 16);
        }
    } else {
        for (uint32_t r = 0; r < rpt; ++r) {
            for (uint32_t e = 0; e < elems; ++e) {
                for (uint32_t s = 0; s < selem; ++s) {
                    uint32_t reg = (rt + r * selem + s) & 31U;
                    uint64_t v = cpu_lane_get_u((const uint8_t *)&cpu->v[reg], e, ebytes);
                    if (!simd_write_elem(cpu, ir, tlb_w, addr + offs, ebytes, v)) {
                        return;
                    }
                    offs += ebytes;
                }
            }
        }
    }

    if (post) {
        uint64_t wb = (rm == 31U) ? offs : cpu_get_x(cpu, rm);
        cpu_set_xsp(cpu, rn, addr + wb);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

void cpu_op_simd_ldst_single(vcpu_t *cpu, const ir_insn_t *ir,
                             softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t q = (word >> 30) & 1U;
    bool post = ((word >> 23) & 1U) != 0;
    bool is_load = ((word >> 22) & 1U) != 0;
    uint32_t r_bit = (word >> 21) & 1U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t opcode = (word >> 13) & 7U;
    uint32_t s_bit = (word >> 12) & 1U;
    uint32_t size = (word >> 10) & 3U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;

    uint32_t selem = (((opcode & 1U) << 1) | r_bit) + 1U;
    uint32_t scale = opcode >> 1;
    bool replicate = false;
    uint32_t ebytes;
    uint32_t index = 0;

    if (scale == 0U) {
        ebytes = 1U;
        index = (q << 3) | (s_bit << 2) | size;
    } else if (scale == 1U) {
        if ((size & 1U) != 0) { vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir)); return; }
        ebytes = 2U;
        index = (q << 2) | (s_bit << 1) | (size >> 1);
    } else if (scale == 2U) {
        if ((size & 1U) == 0) {
            if (size != 0U) { vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir)); return; }
            ebytes = 4U;
            index = (q << 1) | s_bit;
        } else {
            if (size != 1U || s_bit != 0U) { vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir)); return; }
            ebytes = 8U;
            index = q;
        }
    } else {
        if (!is_load || s_bit != 0U) { vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir)); return; }
        replicate = true;
        ebytes = 1U << size;
    }

    if (!cpu_check_sp_align(cpu, rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t addr = cpu_get_xsp(cpu, rn);
    uint64_t offs = 0;

    if (is_load) {
        uint64_t loaded[4] = {0};
        for (uint32_t s = 0; s < selem; ++s) {
            if (!simd_read_elem(cpu, ir, tlb_r, addr + offs, ebytes, &loaded[s])) {
                return;
            }
            offs += ebytes;
        }
        for (uint32_t s = 0; s < selem; ++s) {
            uint32_t reg = (rt + s) & 31U;
            if (replicate) {
                uint64_t rep = cpu_lane_replicate(loaded[s], ebytes);
                cpu->v[reg] = q ? ((__uint128_t)rep << 64) | rep : rep;
            } else {
                cpu_lane_set((uint8_t *)&cpu->v[reg], index, ebytes, loaded[s]);
            }
        }
    } else {
        for (uint32_t s = 0; s < selem; ++s) {
            uint32_t reg = (rt + s) & 31U;
            uint64_t v = cpu_lane_get_u((const uint8_t *)&cpu->v[reg], index, ebytes);
            if (!simd_write_elem(cpu, ir, tlb_w, addr + offs, ebytes, v)) {
                return;
            }
            offs += ebytes;
        }
    }

    if (post) {
        uint64_t wb = (rm == 31U) ? offs : cpu_get_x(cpu, rm);
        cpu_set_xsp(cpu, rn, addr + wb);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}
