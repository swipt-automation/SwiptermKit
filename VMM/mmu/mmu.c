#include "mmu.h"

#include "../board/board_internal.h"
#include "../cpu/cpu_internal.h"
#include "../board/machine.h"

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

static uint16_t mmu_current_asid(const vcpu_t *cpu) {
    if (cpu->world == VCPU_WORLD_HOST_NVHE) {
        return 0;
    }
    uint64_t tcr = cpu->tcr_el1;
    uint64_t ttbr = ((tcr >> 22) & 1ULL) ? cpu->ttbr1_el1 : cpu->ttbr0_el1;
    uint16_t asid = (uint16_t)(ttbr >> 48);
    if (((tcr >> 36) & 1ULL) == 0) {
        asid &= 0x00ffU;
    }
    return asid;
}

static uint64_t mmu_user_salt(const vcpu_t *cpu, uint16_t asid) {
    uint32_t hash = (((uint32_t)asid * 40503U) >> 2) ^ ((uint32_t)cpu->tlb_user_gen << 4);
    return ((uint64_t)asid << 48) | (hash & (EXEC_CACHE_MASK & ~15U));
}

void mmu_update_asid_salt(vcpu_t *cpu) {
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    uint64_t salt = mmu_user_salt(cpu, mmu_current_asid(cpu));
    if ((cpu->sctlr_el1 & 1ULL) == 0) {
        salt |= MMU_SALT_MMU_OFF;
    }
    if (cpu->el != 0 && !cpu->pan) {
        salt |= MMU_SALT_PAN_OFF;
    }
    if ((cpu->sctlr_el1 & 2ULL) != 0) {
        salt |= MMU_SALT_ALIGN;
    }
    if (!cpu_fp_enabled(cpu)) {
        salt |= MMU_SALT_FP_TRAP;
        cpu_fp_leave_guest(cpu);
    }
    if (cpu->tlb_asid_salt != salt) {
        cpu->tlb_asid_salt = salt;
        cpu->ras_epoch++;
    }
}

void mmu_tlbi_all(vcpu_t *cpu) {
    for (uint32_t c = 0; c < VCPU_CTX_COUNT; c++) {
        vcpu_mmu_ctx_t *ctx = cpu->mmu_ctx[c];
        if (!ctx) {
            continue;
        }
        for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
            for (uint32_t i = 0; i < SOFTTLB_ENTRIES; i++) {
                ctx->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
            }
        }
    }
    cpu->exec_epoch++;
    cpu->ras_epoch++;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
}

static void tlb_flush_user_local(vcpu_t *cpu) {
    if ((++cpu->tlb_user_gen & 255U) == 0U) {
        mmu_tlbi_all(cpu);
    }
    mmu_update_asid_salt(cpu);
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (ic->cur_page && (ic->cur_page->va_page >> 48) == 0ULL) {
        ic->cur_exec_epoch = 0;
    }
}

static uint32_t tlb_ctx_mask(const vcpu_t *cpu, const vmm_tlbi_t *op) {
    if (op->target == VMM_TLBI_HOST) {
        return cpu->main_guest ? 0U : 1U << VCPU_CTX_MAIN;
    }
    uint32_t mask = cpu->main_guest ? 1U << VCPU_CTX_MAIN : 0U;
    for (uint32_t i = 0; i < VCPU_CTX_L2_SLOTS; i++) {
        if (cpu->mmu_ctx[1U + i] && (op->target == VMM_TLBI_GUEST_ALL || cpu->l2_vmid[i] == op->vmid)) {
            mask |= 1U << (1U + i);
        }
    }
    return mask;
}

static void tlb_flush_ctx(vcpu_t *cpu, uint32_t mask) {
    for (uint32_t c = 0; c < VCPU_CTX_COUNT; c++) {
        if ((mask & (1U << c)) == 0) {
            continue;
        }
        if (c != cpu->ctx_slot) {
            cpu->ctx_stale |= (uint8_t)(1U << c);
            continue;
        }
        for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
            for (uint32_t i = 0; i < SOFTTLB_ENTRIES; i++) {
                cpu->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
            }
        }
        cpu->exec_epoch++;
        cpu->ras_epoch++;
        cpu->ic_ivau_last_va_page = UINT64_MAX;
    }
}

static void tlb_flush_slot(vcpu_t *cpu, uint32_t mask, uint32_t idx, uint64_t tag) {
    for (uint32_t c = 0; c < VCPU_CTX_COUNT; c++) {
        vcpu_mmu_ctx_t *ctx = cpu->mmu_ctx[c];
        if (!ctx || (mask & (1U << c)) == 0) {
            continue;
        }
        for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
            softtlb_entry_t *entry = &ctx->tlb[b][idx & SOFTTLB_MASK];
            if (entry->tag == tag) {
                entry->tag = SOFTTLB_TAG_INVALID;
            }
        }
        for (uint32_t el = 0; el < 2; el++) {
            exec_cache_entry_t *entry = &ctx->exec_cache[el][idx & EXEC_CACHE_MASK];
            if (entry->tag == tag) {
                entry->pg = NULL;
            }
        }
    }
}

static void tlb_flush_page_local(vcpu_t *cpu, uint32_t mask, uint64_t va, uint64_t salt) {
    uint64_t page = va & 0x0000fffffffff000ULL;
    if ((va >> 48) == 0ULL) {
        for (uint64_t s = salt; s <= (salt | MMU_SALT_PAN_OFF); s += MMU_SALT_PAN_OFF) {
            tlb_flush_slot(cpu, mask, (uint32_t)((va >> VMM_PAGE_SHIFT) ^ s), page | (s & MMU_SALT_USER_TAG));
        }
    } else {
        tlb_flush_slot(cpu, mask, (uint32_t)(va >> VMM_PAGE_SHIFT), page | SOFTTLB_TAG_GLOBAL);
    }
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (ic->cur_page && ic->cur_page->va_page == va) {
        ic->cur_exec_epoch = 0;
    }
}

static void tlb_apply(vcpu_t *cpu, const vmm_tlbi_t *op) {
    uint32_t mask = tlb_ctx_mask(cpu, op);
    if (mask == 0) {
        return;
    }
    if ((op->pages == 0 && op->all_asids) || op->pages >= SOFTTLB_ENTRIES) {
        tlb_flush_ctx(cpu, mask);
        return;
    }
    cpu->ras_epoch++;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    if ((op->va >> 48) == 0ULL && (op->pages == 0 || op->all_asids)) {
        tlb_flush_user_local(cpu);
        return;
    }
    uint64_t salt = mmu_user_salt(cpu, op->asid);
    uint64_t va = op->va;
    for (uint64_t i = 0; i < op->pages; i++, va += VMM_PAGE_SIZE) {
        tlb_flush_page_local(cpu, mask, va, salt);
    }
}

static bool tlb_apply_ring(vcpu_t *cpu, uint64_t target) {
    const vmm_machine_t *vm = cpu->vm;
    uint64_t epoch = cpu->tlb_epoch_local;
    if (target - epoch > VMM_TLB_RING) {
        return true;
    }
    while (epoch != target) {
        ++epoch;
        tlb_apply(cpu, &vm->tlb_ring[epoch & (VMM_TLB_RING - 1U)]);
    }
    return false;
}

static void shootdown_wait(vcpu_t *cpu, uint64_t target, bool icache) {
    vmm_machine_t *vm = cpu->vm;
    if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
        return;
    }
    vmm_host_sev();
    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        vcpu_t *other = vm->cpus[i];
        if (i == cpu->cpu_id || !other) {
            continue;
        }
        const uint64_t *local = icache ? &other->icache_epoch_local : &other->tlb_epoch_local;
        uint32_t spins = 0;
        while (__atomic_load_n(local, __ATOMIC_ACQUIRE) < target) {
            mmu_poll_shootdown(cpu);
            if (atomic_load_explicit(&other->halted_wfi, memory_order_seq_cst) ||
                atomic_load_explicit(&other->stop_requested, memory_order_acquire) ||
                !atomic_load_explicit(&vm->running, memory_order_acquire) ||
                atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
                break;
            }
            if (++spins < 128) {
                __asm__ __volatile__("yield");
            } else {
                sched_yield();
            }
        }
    }
}

static uint64_t mmu_canonicalize_va(uint64_t va) {
    if ((va >> 55) & 1ULL) {
        return va | 0xff00000000000000ULL;
    }
    return va & 0x00ffffffffffffffULL;
}

void mmu_tlbi(vcpu_t *cpu, vmm_tlbi_t op, bool inner_shareable) {
    op.va = mmu_canonicalize_va(op.va) & ~(uint64_t)VMM_PAGE_MASK;
    tlb_apply(cpu, &op);
    vmm_machine_t *vm = cpu->vm;
    if (!inner_shareable || vm->cpu_count <= 1) {
        return;
    }
    os_unfair_lock_lock(&vm->tlb_lock);
    uint64_t target = atomic_load_explicit(&vm->tlb_epoch, memory_order_relaxed);
    bool lagged = tlb_apply_ring(cpu, target);
    ++target;
    vm->tlb_ring[target & (VMM_TLB_RING - 1U)] = op;
    atomic_store_explicit(&vm->tlb_epoch, target, memory_order_seq_cst);
    os_unfair_lock_unlock(&vm->tlb_lock);
    if (lagged) {
        mmu_tlbi_all(cpu);
    }
    __atomic_store_n(&cpu->tlb_epoch_local, target, __ATOMIC_RELEASE);
    shootdown_wait(cpu, target, false);
}

static bool icache_apply_ring(vcpu_t *cpu, uint64_t target) {
    const vmm_machine_t *vm = cpu->vm;
    uint64_t epoch = cpu->icache_epoch_local;
    if (target - epoch > VMM_ICACHE_RING) {
        return true;
    }
    while (epoch != target) {
        ++epoch;
        const vmm_icache_shootdown_t *rec = &vm->icache_ring[epoch & (VMM_ICACHE_RING - 1U)];
        if (rec->count > VCPU_ICACHE_PENDING) {
            return true;
        }
        for (uint32_t i = 0; i < rec->count; ++i) {
            vcpu_flush_icache_pa(cpu, rec->pa_pages[i]);
        }
    }
    return false;
}

void mmu_icache_broadcast(vcpu_t *cpu) {
    vmm_machine_t *vm = cpu->vm;
    uint64_t pages[VCPU_ICACHE_PENDING];
    uint32_t count = 0;
    bool all = cpu->icache_shootdown_all;
    atomic_thread_fence(memory_order_seq_cst);
    for (uint32_t i = 0; i < cpu->icache_pending_count; ++i) {
        uint64_t pa_page = cpu->icache_pending_pa[i];
        uint64_t mask;
        _Atomic uint64_t *word = cpu_icache_ram_word(vm, pa_page, &mask);
        if (word && (atomic_load_explicit(word, memory_order_seq_cst) & mask) == 0) {
            continue;
        }
        vcpu_flush_icache_pa(cpu, pa_page);
        pages[count++] = pa_page;
    }
    cpu->icache_pending_count = 0;
    cpu->icache_shootdown_all = false;
    cpu->icache_shootdown_pending = false;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    if ((!all && count == 0U) || vm->cpu_count <= 1) {
        return;
    }

    os_unfair_lock_lock(&vm->icache_lock);
    uint64_t target = atomic_load_explicit(&vm->icache_epoch, memory_order_relaxed);
    bool lagged = icache_apply_ring(cpu, target);
    ++target;
    vmm_icache_shootdown_t *rec = &vm->icache_ring[target & (VMM_ICACHE_RING - 1U)];
    rec->count = all ? UINT32_MAX : count;
    memcpy(rec->pa_pages, pages, count * sizeof(pages[0]));
    atomic_store_explicit(&vm->icache_epoch, target, memory_order_seq_cst);
    os_unfair_lock_unlock(&vm->icache_lock);
    if (lagged) {
        vcpu_flush_icache_all(cpu);
    }
    __atomic_store_n(&cpu->icache_epoch_local, target, __ATOMIC_RELEASE);
    shootdown_wait(cpu, target, true);
}

bool mmu_poll_shootdown(vcpu_t *cpu) {
    vmm_machine_t *vm = cpu->vm;
    bool flushed = false;
    if (__builtin_expect(atomic_load_explicit(&vm->tlb_epoch, memory_order_seq_cst) != cpu->tlb_epoch_local, 0)) {
        os_unfair_lock_lock(&vm->tlb_lock);
        uint64_t epoch = atomic_load_explicit(&vm->tlb_epoch, memory_order_relaxed);
        bool lagged = tlb_apply_ring(cpu, epoch);
        os_unfair_lock_unlock(&vm->tlb_lock);
        if (lagged) {
            mmu_tlbi_all(cpu);
        }
        __atomic_store_n(&cpu->tlb_epoch_local, epoch, __ATOMIC_RELEASE);
        flushed = true;
    }
    if (__builtin_expect(atomic_load_explicit(&vm->icache_epoch, memory_order_seq_cst) != cpu->icache_epoch_local, 0)) {
        os_unfair_lock_lock(&vm->icache_lock);
        uint64_t epoch = atomic_load_explicit(&vm->icache_epoch, memory_order_relaxed);
        bool lagged = icache_apply_ring(cpu, epoch);
        os_unfair_lock_unlock(&vm->icache_lock);
        if (lagged) {
            vcpu_flush_icache_all(cpu);
        }
        __atomic_store_n(&cpu->icache_epoch_local, epoch, __ATOMIC_RELEASE);
        flushed = true;
    }
    return flushed;
}

void *mmu_gpa_to_hva(const vmm_machine_t *vm, uint64_t gpa, size_t len) {
    if (!vm) {
        return NULL;
    }
    if (vm->ram && gpa >= VMM_RAM_BASE) {
        uint64_t off = gpa - VMM_RAM_BASE;
        if (off <= vm->ram_size && len <= vm->ram_size - off) {
            return vm->ram + off;
        }
    }
    return NULL;
}

static uint32_t mmu_pa_bits(uint64_t ps) {
    static const uint8_t table[8] = {32, 36, 40, 42, 44, 48, 48, 48};
    return table[ps & 7ULL];
}

static uint32_t mmu_stages(const vcpu_t *cpu) {
    return cpu->world == VCPU_WORLD_GUEST && (cpu->el2.hcr & HCR_EL2_VM) != 0 ? MMU_STAGE2 : 0U;
}

typedef struct walk_detail {
    mmu_translation_t res;
    bool is_device;
    bool is_ttbr1;
    bool non_global;
    bool el0_accessible;
    bool readable;
    bool writable;
    bool canonical_untagged;
} walk_detail_t;

typedef struct s2_walk {
    uint64_t pa;
    uint32_t fsc;
    uint32_t level;
    bool ok;
    bool device;
    bool use_s1;
    bool readable;
    bool writable;
} s2_walk_t;

static s2_walk_t mmu_s2_walk(vcpu_t *cpu, uint64_t ipa, mmu_access_t access, bool hw_update) {
    s2_walk_t out = {.fsc = 0x04U};
    vmm_machine_t *vm = cpu->vm;
    uint64_t vtcr = cpu->el2.vtcr;
    uint32_t tg = (uint32_t)((vtcr >> 14) & 3ULL);
    uint32_t granule_shift = tg == 0U ? 12U : tg == 2U ? 14U : tg == 1U ? 16U : 0U;
    uint32_t t0sz = (uint32_t)(vtcr & 0x3FULL);
    if (granule_shift == 0U || t0sz < 16U) {
        return out;
    }
    uint32_t stride = granule_shift - 3U;
    int start_level = (granule_shift == 12U ? 2 : 3) - (int)((vtcr >> 6) & 3ULL);
    uint32_t ia_bits = 64U - t0sz;
    if (start_level < 0 || (ipa >> ia_bits) != 0) {
        return out;
    }
    uint32_t start_shift = granule_shift + (uint32_t)(3 - start_level) * stride;
    if (ia_bits <= start_shift || ia_bits - start_shift > stride + 4U) {
        return out;
    }
    uint32_t start_idx_bits = ia_bits - start_shift;
    uint32_t pa_bits = mmu_pa_bits(vtcr >> 16);
    uint64_t root = cpu->el2.vttbr & 0x0000fffffffffffeULL & ~((1ULL << (start_idx_bits + 3U)) - 1ULL);
    bool ha = (vtcr & VTCR_EL2_HA) != 0;
    bool hd = ha && (vtcr & VTCR_EL2_HD) != 0;

retry:;
    uint64_t table = root;
    uint64_t *desc_hva = NULL;
    uint64_t desc = 0;
    uint64_t pa = 0;
    int level = start_level;
    for (;; level++) {
        uint32_t shift = granule_shift + (uint32_t)(3 - level) * stride;
        uint32_t idx_bits = level == start_level ? start_idx_bits : stride;
        desc_hva = mmu_gpa_to_hva(vm, table + (((ipa >> shift) & ((1ULL << idx_bits) - 1ULL)) << 3), 8);
        if (!desc_hva) {
            out.fsc = 0x14U + (uint32_t)level;
            return out;
        }
        desc = __atomic_load_n(desc_hva, __ATOMIC_ACQUIRE);
        if ((desc & 1ULL) == 0) {
            out.fsc = 0x04U + (uint32_t)level;
            return out;
        }
        if (level < 3 && (desc & 2ULL) != 0) {
            table = desc & 0x0000ffffffffffffULL & ~((1ULL << granule_shift) - 1ULL);
            if ((table >> pa_bits) != 0) {
                out.fsc = (uint32_t)level;
                return out;
            }
            continue;
        }
        if (level < 3 ? (granule_shift == 12U ? level < 1 : level != 2) : (desc & 2ULL) == 0) {
            out.fsc = 0x04U + (uint32_t)level;
            return out;
        }
        uint64_t base = desc & 0x0000ffffffffffffULL & ~((1ULL << shift) - 1ULL);
        if ((base >> pa_bits) != 0) {
            out.fsc = (uint32_t)level;
            return out;
        }
        pa = base | (ipa & ((1ULL << shift) - 1ULL));
        break;
    }

    out.level = (uint32_t)level;
    uint64_t new_desc = desc;
    if (access != MMU_ACCESS_RESOLVE && (desc & (1ULL << 10)) == 0) {
        if (!ha) {
            out.fsc = 0x08U + (uint32_t)level;
            return out;
        }
        new_desc |= 1ULL << 10;
    }
    out.readable = (desc & (1ULL << 6)) != 0;
    out.writable = (desc & (1ULL << 7)) != 0;
    if (access == MMU_ACCESS_WRITE && !out.writable && hd && (desc & (1ULL << 51)) != 0) {
        new_desc |= 1ULL << 7;
        out.writable = true;
    }
    bool perm_ok;
    switch (access) {
    case MMU_ACCESS_READ: perm_ok = out.readable; break;
    case MMU_ACCESS_WRITE: perm_ok = out.writable; break;
    case MMU_ACCESS_EXEC: perm_ok = (desc & (1ULL << 54)) == 0; break;
    default: perm_ok = true; break;
    }
    if (!perm_ok) {
        out.fsc = 0x0CU + (uint32_t)level;
        return out;
    }
    if (hw_update && new_desc != desc &&
        !__atomic_compare_exchange_n(desc_hva, &desc, new_desc, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        goto retry;
    }

    uint32_t attr = (uint32_t)((desc >> 2) & 0xFULL);
    if ((cpu->el2.hcr & HCR_EL2_FWB) != 0) {
        out.device = (attr & 4U) == 0;
        out.use_s1 = (attr & 7U) == 7U;
    } else {
        out.device = (attr & 0xCU) == 0;
        out.use_s1 = !out.device;
    }
    out.pa = pa;
    out.ok = true;
    return out;
}

static void mmu_s2_fault(walk_detail_t *out, uint32_t fsc, uint64_t ipa, bool s1ptw, bool ptw_write) {
    out->res.fault_fsc = fsc;
    out->res.ipa = ipa;
    out->res.s2_fault = true;
    out->res.s1ptw = s1ptw;
    out->res.ptw_write = ptw_write;
}

static uint64_t *mmu_s1_desc(vcpu_t *cpu, uint64_t desc_ipa, int level, uint32_t stages, bool hw_update,
                             walk_detail_t *out) {
    uint64_t pa = desc_ipa;
    if ((stages & MMU_STAGE2_PTW) != 0) {
        s2_walk_t s2 = mmu_s2_walk(cpu, desc_ipa, MMU_ACCESS_READ, hw_update);
        if (s2.ok && s2.device && (cpu->el2.hcr & HCR_EL2_PTW) != 0) {
            s2.ok = false;
            s2.fsc = 0x0CU + s2.level;
        }
        if (!s2.ok) {
            mmu_s2_fault(out, s2.fsc, desc_ipa, true, false);
            return NULL;
        }
        pa = s2.pa;
    }
    uint64_t *hva = mmu_gpa_to_hva(cpu->vm, pa, 8);
    if (!hva) {
        out->res.fault_fsc = 0x14U + (uint32_t)level;
    }
    return hva;
}

static void mmu_walk_output(vcpu_t *cpu, walk_detail_t *out, uint64_t ipa, mmu_access_t access, uint32_t stages,
                            bool hw_update) {
    uint64_t pa = ipa;
    out->res.ipa = ipa;
    if ((stages & MMU_STAGE2_OUT) != 0) {
        s2_walk_t s2 = mmu_s2_walk(cpu, ipa, access, hw_update);
        if (!s2.ok) {
            mmu_s2_fault(out, s2.fsc, ipa, false, false);
            return;
        }
        pa = s2.pa;
        out->readable = out->readable && s2.readable;
        out->writable = out->writable && s2.writable;
        out->is_device = s2.device || (s2.use_s1 && out->is_device);
    }
    out->res.pa = pa;
    void *hva = mmu_gpa_to_hva(cpu->vm, pa, 1);
    if (hva) {
        out->res.hva = (uintptr_t)hva;
        out->res.ok = true;
    } else if (access == MMU_ACCESS_EXEC) {
        out->res.fault_fsc = 0x10U;
    } else {
        out->res.is_mmio = true;
        out->res.ok = true;
    }
}

static walk_detail_t mmu_walk(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el, bool hw_update,
                              uint32_t stages) {
    walk_detail_t out = {0};
    CPU_STATS_WALK(cpu);

    if ((cpu->sctlr_el1 & 1ULL) == 0) {
        out.readable = true;
        out.writable = true;
        mmu_walk_output(cpu, &out, va, access, stages, hw_update);
        return out;
    }

    uint64_t tcr = cpu->tcr_el1;
    bool nvhe = cpu->world == VCPU_WORLD_HOST_NVHE;
    bool is_ttbr1 = !nvhe && ((va >> 55) & 1ULL) != 0;
    out.is_ttbr1 = is_ttbr1;

    uint32_t tsz;
    uint32_t tg_bits;
    uint32_t granule_shift;
    bool epd;
    bool e0pd;
    bool tbi;
    uint64_t ttbr;

    if (!is_ttbr1) {
        tsz = (uint32_t)(tcr & 0x3fULL);
        epd = !nvhe && ((tcr >> 7) & 1ULL) != 0;
        e0pd = !nvhe && ((tcr >> 55) & 1ULL) != 0;
        tg_bits = (uint32_t)((tcr >> 14) & 3ULL);
        tbi = nvhe ? (((tcr >> 20) & 1ULL) != 0)
                   : (((tcr >> 37) & 1ULL) != 0 && !(access == MMU_ACCESS_EXEC && ((tcr >> 51) & 1ULL)));
        ttbr = cpu->ttbr0_el1;
        if (tg_bits == 0) {
            granule_shift = 12;
        } else if (tg_bits == 2) {
            granule_shift = 14;
        } else if (tg_bits == 1) {
            granule_shift = 16;
        } else {
            out.res.fault_fsc = 0x04;
            return out;
        }
    } else {
        tsz = (uint32_t)((tcr >> 16) & 0x3fULL);
        epd = ((tcr >> 23) & 1ULL) != 0;
        e0pd = ((tcr >> 56) & 1ULL) != 0;
        tg_bits = (uint32_t)((tcr >> 30) & 3ULL);
        tbi = ((tcr >> 38) & 1ULL) != 0 && !(access == MMU_ACCESS_EXEC && ((tcr >> 52) & 1ULL));
        ttbr = cpu->ttbr1_el1;
        if (tg_bits == 2) {
            granule_shift = 12;
        } else if (tg_bits == 1) {
            granule_shift = 14;
        } else if (tg_bits == 3) {
            granule_shift = 16;
        } else {
            out.res.fault_fsc = 0x04;
            return out;
        }
    }

    if (epd || (el == 0 && e0pd) || tsz < 16 || tsz > 39) {
        out.res.fault_fsc = 0x04;
        return out;
    }

    uint32_t ia_bits = 64 - tsz;
    uint32_t hi_bit = tbi ? 55 : 63;
    uint64_t top_mask = (hi_bit == 63 ? ~0ULL : ((1ULL << (hi_bit + 1)) - 1ULL)) &
                        ~((1ULL << ia_bits) - 1ULL);
    uint64_t expected_top = is_ttbr1 ? top_mask : 0ULL;
    if ((va & top_mask) != expected_top) {
        out.res.fault_fsc = 0x04;
        return out;
    }

    uint64_t top16 = va >> 48;
    out.canonical_untagged = (!is_ttbr1 && top16 == 0ULL) || (is_ttbr1 && top16 == 0xffffULL);

    uint32_t stride = granule_shift - 3;
    int start_level = 3 - (int)((ia_bits - granule_shift - 1) / stride);
    if (start_level < 0 || start_level > 3) {
        out.res.fault_fsc = 0x04;
        return out;
    }

    uint32_t ips_bits = mmu_pa_bits(nvhe ? (tcr >> 16) : (tcr >> 32));
    uint32_t start_shift = granule_shift + (uint32_t)(3 - start_level) * stride;
    uint32_t start_idx_bits = ia_bits - start_shift;
    uint64_t baddr_align_mask = ~((1ULL << (start_idx_bits + 3)) - 1ULL);
    uint64_t ttbr_table_pa = ttbr & 0x0000ffffffffffffULL & baddr_align_mask;

    if (ips_bits < 48 && (ttbr_table_pa >> ips_bits) != 0) {
        out.res.fault_fsc = 0x00;
        return out;
    }

    bool ha = ((tcr >> (nvhe ? 21 : 39)) & 1ULL) != 0;
    bool hd = ha && ((tcr >> (nvhe ? 22 : 40)) & 1ULL) != 0;

retry:;
    uint64_t table_pa = ttbr_table_pa;
    uint32_t ap_table = 0;
    bool uxn_table = false;
    bool pxn_table = false;
    uint64_t *desc_hva = NULL;
    uint64_t desc_pa = 0;
    uint64_t desc = 0;
    uint64_t pa = 0;
    int leaf_level = 3;

    for (int level = start_level; level <= 3; level++) {
        uint32_t shift = granule_shift + (uint32_t)(3 - level) * stride;
        uint32_t idx_bits = (level == start_level) ? start_idx_bits : stride;
        uint64_t index = (va >> shift) & ((1ULL << idx_bits) - 1ULL);
        desc_pa = table_pa + (index << 3);

        desc_hva = mmu_s1_desc(cpu, desc_pa, level, stages, hw_update, &out);
        if (!desc_hva) {
            return out;
        }
        desc = __atomic_load_n(desc_hva, __ATOMIC_ACQUIRE);
        uint64_t type = desc & 3ULL;
        if ((type & 1ULL) == 0) {
            out.res.fault_fsc = 0x04U + (uint32_t)level;
            return out;
        }

        if (level < 3 && type == 3ULL) {
            uint64_t next_pa = desc & 0x0000ffffffffffffULL & ~((1ULL << granule_shift) - 1ULL);
            if (ips_bits < 48 && (next_pa >> ips_bits) != 0) {
                out.res.fault_fsc = 0x00U + (uint32_t)level;
                return out;
            }
            ap_table |= (uint32_t)((desc >> 61) & (nvhe ? 2ULL : 3ULL));
            uxn_table |= ((desc >> 60) & 1ULL) != 0;
            pxn_table |= ((desc >> 59) & 1ULL) != 0;
            table_pa = next_pa;
            continue;
        }

        if (level < 3) {
            bool block_ok = (granule_shift == 12 && (level == 1 || level == 2)) ||
                            (granule_shift == 14 && level == 2) ||
                            (granule_shift == 16 && level == 2);
            if (!block_ok) {
                out.res.fault_fsc = 0x04U + (uint32_t)level;
                return out;
            }
            uint64_t block_base = desc & 0x0000ffffffffffffULL & ~((1ULL << shift) - 1ULL);
            if (ips_bits < 48 && (block_base >> ips_bits) != 0) {
                out.res.fault_fsc = 0x00U + (uint32_t)level;
                return out;
            }
            pa = block_base | (va & ((1ULL << shift) - 1ULL));
            leaf_level = level;
            break;
        }

        if (type != 3ULL) {
            out.res.fault_fsc = 0x07U;
            return out;
        }
        uint64_t page_base = desc & 0x0000ffffffffffffULL & ~((1ULL << granule_shift) - 1ULL);
        if (ips_bits < 48 && (page_base >> ips_bits) != 0) {
            out.res.fault_fsc = 0x03U;
            return out;
        }
        pa = page_base | (va & ((1ULL << granule_shift) - 1ULL));
        leaf_level = 3;
        break;
    }

    uint64_t new_desc = desc;
    if (access != MMU_ACCESS_RESOLVE && ((desc >> 10) & 1ULL) == 0) {
        if (!ha) {
            out.res.fault_fsc = 0x08U + (uint32_t)leaf_level;
            return out;
        }
        new_desc |= 1ULL << 10;
    }
    if (access == MMU_ACCESS_WRITE && hd && ((desc >> 51) & 1ULL) != 0 && (ap_table & 2U) == 0) {
        new_desc &= ~(1ULL << 7);
    }

    uint32_t ap = (uint32_t)((new_desc >> 6) & 3ULL);
    if (ap_table & 1U) {
        ap &= ~1U;
    }
    if (ap_table & 2U) {
        ap |= 2U;
    }

    bool uxn = ((desc >> 54) & 1ULL) != 0 || uxn_table;
    bool pxn = nvhe ? uxn : (((desc >> 53) & 1ULL) != 0 || pxn_table);
    bool el0_accessible = !nvhe && (ap & 1U) != 0;
    bool writable_el0 = !nvhe && (ap == 1U);
    bool writable_el1 = ((ap & 2U) == 0);
    bool wxn = ((cpu->sctlr_el1 >> 19) & 1ULL) != 0;
    bool pan_denied = el != 0 && cpu->pan && el0_accessible;
    out.readable = el == 0 ? el0_accessible : !pan_denied;
    out.writable = el == 0 ? writable_el0 : writable_el1 && !pan_denied;

    bool perm_ok = true;
    switch (access) {
    case MMU_ACCESS_READ:
        perm_ok = out.readable;
        break;
    case MMU_ACCESS_WRITE:
        perm_ok = out.writable;
        break;
    case MMU_ACCESS_EXEC:
        perm_ok = el == 0 ? el0_accessible && !uxn && !(wxn && writable_el0)
                          : !pxn && !writable_el0 && !(wxn && writable_el1);
        break;
    case MMU_ACCESS_RESOLVE:
        break;
    }

    if (!perm_ok) {
        out.res.fault_fsc = 0x0cU + (uint32_t)leaf_level;
        return out;
    }

    if (hw_update && new_desc != desc) {
        if ((stages & MMU_STAGE2_PTW) != 0) {
            s2_walk_t s2 = mmu_s2_walk(cpu, desc_pa, MMU_ACCESS_WRITE, true);
            if (!s2.ok) {
                mmu_s2_fault(&out, s2.fsc, desc_pa, true, true);
                return out;
            }
        }
        if (!__atomic_compare_exchange_n(desc_hva, &desc, new_desc, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            goto retry;
        }
    }

    uint32_t attr_indx = (uint32_t)((desc >> 2) & 7ULL);
    uint8_t mair_attr = (uint8_t)((cpu->mair_el1 >> (attr_indx * 8)) & 0xffULL);
    out.is_device = (mair_attr & 0xf0U) == 0;
    out.non_global = ((desc >> 11) & 1ULL) != 0;
    out.el0_accessible = el0_accessible;
    mmu_walk_output(cpu, &out, pa, access, stages, hw_update);
    return out;
}

mmu_translation_t mmu_translate(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el, uint32_t stages) {
    return mmu_walk(cpu, va, access, el, false, stages).res;
}

static void mmu_tlb_fill(vcpu_t *cpu, uint64_t va, uint8_t el, const walk_detail_t *w) {
    if ((cpu->sctlr_el1 & 1ULL) == 0 || !w->res.ok || w->res.is_mmio || w->is_device ||
        !w->canonical_untagged || (w->is_ttbr1 && (w->non_global || (el == 1 && w->el0_accessible)))) {
        return;
    }

    uint64_t page_pa = w->res.pa & ~(uint64_t)VMM_PAGE_MASK;
    void *page_hva = mmu_gpa_to_hva(cpu->vm, page_pa, VMM_PAGE_SIZE);
    if (!page_hva) {
        return;
    }

    mmu_update_asid_salt(cpu);

    uint64_t page_va = va & 0x0000fffffffff000ULL;
    uint32_t idx;
    uint64_t tag;
    if (!w->is_ttbr1) {
        idx = (uint32_t)(((va >> VMM_PAGE_SHIFT) ^ (uint32_t)cpu->tlb_asid_salt) & SOFTTLB_MASK);
        tag = page_va | (cpu->tlb_asid_salt & MMU_SALT_USER_TAG);
    } else {
        idx = (uint32_t)((va >> VMM_PAGE_SHIFT) & SOFTTLB_MASK);
        tag = page_va | SOFTTLB_TAG_GLOBAL;
    }

    softtlb_entry_t entry = {tag, (uintptr_t)page_hva - (uintptr_t)(va & ~(uint64_t)VMM_PAGE_MASK)};
    softtlb_bank_t read_bank = el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R;
    if (w->readable) {
        cpu->tlb[read_bank][idx] = entry;
    }
    if (w->writable) {
        cpu->tlb[read_bank + 1][idx] = entry;
    }
}

bool mmu_translate_and_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                            mmu_translation_t *out, bool *is_device_out) {
    walk_detail_t w = mmu_walk(cpu, va, access, el, true, mmu_stages(cpu));
    *out = w.res;
    if (is_device_out) {
        *is_device_out = w.is_device;
    }
    if (!w.res.ok) {
        return false;
    }
    if (!w.res.is_mmio) {
        mmu_tlb_fill(cpu, va, el, &w);
    }
    return true;
}

bool mmu_translate_exec(vcpu_t *cpu, uint64_t va_page, uint8_t el, mmu_translation_t *out, bool *cacheable_out) {
    walk_detail_t w = mmu_walk(cpu, va_page, MMU_ACCESS_EXEC, el, true, mmu_stages(cpu));
    *out = w.res;
    if (!w.res.ok || w.res.is_mmio) {
        *cacheable_out = false;
        return false;
    }
    bool mmu_on = (cpu->sctlr_el1 & 1ULL) != 0;
    if (mmu_on) {
        *cacheable_out = !w.is_device && w.canonical_untagged && !(w.is_ttbr1 && w.non_global) &&
                         mmu_gpa_to_hva(cpu->vm, w.res.pa & ~(uint64_t)VMM_PAGE_MASK, VMM_PAGE_SIZE) != NULL;
    } else {
        *cacheable_out = (va_page >> 48) == 0ULL &&
                         mmu_gpa_to_hva(cpu->vm, w.res.pa & ~(uint64_t)VMM_PAGE_MASK, VMM_PAGE_SIZE) != NULL;
    }
    return true;
}

bool mmu_resolve_va_pa(vcpu_t *cpu, uint64_t va, uint64_t *pa_out) {
    const vmm_machine_t *vm = cpu->vm;
    uint64_t va_page = va & ~(uint64_t)VMM_PAGE_MASK;
    uint32_t first = cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R;
    for (uint32_t b = first; b <= first + 1U; ++b) {
        uintptr_t hva;
        if (!mmu_tlb_lookup_aligned(cpu->tlb[b], cpu->tlb_asid_salt, va_page, &hva)) {
            continue;
        }
        if (vm->ram && hva - (uintptr_t)vm->ram < vm->ram_size) {
            *pa_out = VMM_RAM_BASE + (hva - (uintptr_t)vm->ram) + (va & VMM_PAGE_MASK);
            return true;
        }
    }
    walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_RESOLVE, cpu->el, false, mmu_stages(cpu));
    if (!w.res.ok) {
        return false;
    }
    *pa_out = w.res.pa;
    return true;
}

static uint32_t mmu_isv_syndrome(const vcpu_t *cpu, uint64_t elr) {
    const ir_page_t *pg = ((const cpu_icache_t *)cpu->icache)->cur_page;
    if (!pg || pg->va_page != (elr & ~(uint64_t)VMM_PAGE_MASK)) {
        return 0;
    }
    uint32_t word = *(const uint32_t *)(pg->hva_page + (elr & VMM_PAGE_MASK));
    uint32_t size = word >> 30;
    uint32_t opc;
    uint32_t ar = 0;
    if ((word & 0x3F000000U) == 0x39000000U || (word & 0x3F200400U) == 0x38000000U ||
        (word & 0x3F200C00U) == 0x38200800U) {
        opc = (word >> 22) & 3U;
    } else if ((word & 0x3FBF7C00U) == 0x089F7C00U) {
        opc = (word >> 22) & 1U;
        ar = 1;
    } else if ((word & 0x3FFFFC00U) == 0x38BFC000U) {
        opc = 1;
        ar = 1;
    } else if ((word & 0x3F200C00U) == 0x19000000U) {
        opc = (word >> 22) & 3U;
        ar = 1;
    } else {
        return 0;
    }
    if ((opc == 2U && size == 3U) || (opc == 3U && size >= 2U)) {
        return 0;
    }
    uint32_t sf = opc == 2U || (opc < 2U && size == 3U);
    return (1U << 24) | (size << 22) | ((opc >> 1) << 21) | ((word & 31U) << 16) | (sf << 15) | (ar << 14);
}

void vcpu_raise_abort(vcpu_t *cpu, uint64_t va, const mmu_translation_t *tr, bool is_write, uint64_t elr,
                      bool fetch) {
    uint32_t fsc = tr ? (tr->fault_fsc & 0x3FU) : 0x21U;
    uint32_t ec = fetch ? 0x20U : 0x24U;
    if (tr && tr->s2_fault) {
        uint32_t iss = fsc | (tr->s1ptw ? 1U << 7 : 0U);
        if (!fetch) {
            bool wnr = tr->s1ptw ? tr->ptw_write : is_write;
            iss |= (wnr ? 1U << 6 : 0U) | (tr->s1ptw ? 0U : mmu_isv_syndrome(cpu, elr));
        }
        cpu->el2.hpfar = (tr->ipa >> 8) & 0x00000FFFFFFFFFF0ULL;
        vcpu_raise_exception_el2(cpu, (ec << 26) | (1U << 25) | iss, va, elr);
        return;
    }
    ec += cpu->el != 0;
    vcpu_raise_exception(cpu, (ec << 26) | (1U << 25) | (!fetch && is_write ? 1U << 6 : 0U) | fsc, va, elr);
}

static bool mmu_read_slow_n(vcpu_t *cpu, uint64_t va, uint32_t size, void *out) {
    bool unaligned = size > 1 && (va & (size - 1)) != 0;
    if (unaligned && (cpu->sctlr_el1 & 2ULL) != 0) {
        vcpu_raise_abort(cpu, va, NULL, false, cpu->pc, false);
        return false;
    }

    bool crosses = size > 1 && (((va + (size - 1)) ^ va) & (1ULL << VMM_PAGE_SHIFT)) != 0;
    if (!crosses) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_READ, cpu->el, true, mmu_stages(cpu));
        if (!w.res.ok) {
            vcpu_raise_abort(cpu, va, &w.res, false, cpu->pc, false);
            return false;
        }
        if (unaligned && w.is_device) {
            vcpu_raise_abort(cpu, va, NULL, false, cpu->pc, false);
            return false;
        }
        if (w.res.is_mmio) {
            cpu_fp_leave_guest(cpu);
            if (size <= 8) {
                uint64_t v = vmm_mmio_read(cpu->vm, cpu, w.res.pa, size);
                memcpy(out, &v, size);
            } else {
                uint64_t lo = vmm_mmio_read(cpu->vm, cpu, w.res.pa, 8);
                uint64_t hi = vmm_mmio_read(cpu->vm, cpu, w.res.pa + 8, 8);
                memcpy(out, &lo, 8);
                memcpy((uint8_t *)out + 8, &hi, 8);
            }
            return true;
        }
        mmu_tlb_fill(cpu, va, cpu->el, &w);
        if (!unaligned) {
            switch (size) {
            case 1: *(uint8_t *)out = __atomic_load_n((const uint8_t *)w.res.hva, __ATOMIC_RELAXED); return true;
            case 2: *(uint16_t *)out = __atomic_load_n((const uint16_t *)w.res.hva, __ATOMIC_RELAXED); return true;
            case 4: *(uint32_t *)out = __atomic_load_n((const uint32_t *)w.res.hva, __ATOMIC_RELAXED); return true;
            case 8: *(uint64_t *)out = __atomic_load_n((const uint64_t *)w.res.hva, __ATOMIC_RELAXED); return true;
            default: break;
            }
        }
        memcpy(out, (const void *)w.res.hva, size);
        return true;
    }

    uint64_t va2 = (va & ~(uint64_t)VMM_PAGE_MASK) + VMM_PAGE_SIZE;
    walk_detail_t w1 = mmu_walk(cpu, va, MMU_ACCESS_READ, cpu->el, true, mmu_stages(cpu));
    if (!w1.res.ok) {
        vcpu_raise_abort(cpu, va, &w1.res, false, cpu->pc, false);
        return false;
    }
    if (w1.is_device) {
        vcpu_raise_abort(cpu, va, NULL, false, cpu->pc, false);
        return false;
    }
    walk_detail_t w2 = mmu_walk(cpu, va2, MMU_ACCESS_READ, cpu->el, true, mmu_stages(cpu));
    if (!w2.res.ok) {
        vcpu_raise_abort(cpu, va2, &w2.res, false, cpu->pc, false);
        return false;
    }
    if (w2.is_device) {
        vcpu_raise_abort(cpu, va2, NULL, false, cpu->pc, false);
        return false;
    }

    uint32_t n1 = (uint32_t)(VMM_PAGE_SIZE - (va & VMM_PAGE_MASK));
    uint32_t n2 = size - n1;
    uint8_t *dst = (uint8_t *)out;

    if (!w1.res.is_mmio) {
        mmu_tlb_fill(cpu, va, cpu->el, &w1);
        memcpy(dst, (const void *)w1.res.hva, n1);
    } else {
        cpu_fp_leave_guest(cpu);
        for (uint32_t i = 0; i < n1; i++) {
            dst[i] = (uint8_t)vmm_mmio_read(cpu->vm, cpu, w1.res.pa + i, 1);
        }
    }
    if (!w2.res.is_mmio) {
        mmu_tlb_fill(cpu, va2, cpu->el, &w2);
        memcpy(dst + n1, (const void *)w2.res.hva, n2);
    } else {
        cpu_fp_leave_guest(cpu);
        for (uint32_t i = 0; i < n2; i++) {
            dst[n1 + i] = (uint8_t)vmm_mmio_read(cpu->vm, cpu, w2.res.pa + i, 1);
        }
    }
    return true;
}

static bool mmu_write_slow_n(vcpu_t *cpu, uint64_t va, uint32_t size, const void *in) {
    bool unaligned = size > 1 && (va & (size - 1)) != 0;
    if (unaligned && (cpu->sctlr_el1 & 2ULL) != 0) {
        vcpu_raise_abort(cpu, va, NULL, true, cpu->pc, false);
        return false;
    }

    bool crosses = size > 1 && (((va + (size - 1)) ^ va) & (1ULL << VMM_PAGE_SHIFT)) != 0;
    if (!crosses) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el, true, mmu_stages(cpu));
        if (!w.res.ok) {
            vcpu_raise_abort(cpu, va, &w.res, true, cpu->pc, false);
            return false;
        }
        if (unaligned && w.is_device) {
            vcpu_raise_abort(cpu, va, NULL, true, cpu->pc, false);
            return false;
        }
        if (w.res.is_mmio) {
            cpu_fp_leave_guest(cpu);
            if (size <= 8) {
                uint64_t v = 0;
                memcpy(&v, in, size);
                vmm_mmio_write(cpu->vm, cpu, w.res.pa, size, v);
            } else {
                uint64_t lo, hi;
                memcpy(&lo, in, 8);
                memcpy(&hi, (const uint8_t *)in + 8, 8);
                vmm_mmio_write(cpu->vm, cpu, w.res.pa, 8, lo);
                vmm_mmio_write(cpu->vm, cpu, w.res.pa + 8, 8, hi);
            }
            return true;
        }
        mmu_tlb_fill(cpu, va, cpu->el, &w);
        if (!unaligned) {
            switch (size) {
            case 1: __atomic_store_n((uint8_t *)w.res.hva, *(const uint8_t *)in, __ATOMIC_RELAXED); return true;
            case 2: __atomic_store_n((uint16_t *)w.res.hva, *(const uint16_t *)in, __ATOMIC_RELAXED); return true;
            case 4: __atomic_store_n((uint32_t *)w.res.hva, *(const uint32_t *)in, __ATOMIC_RELAXED); return true;
            case 8: __atomic_store_n((uint64_t *)w.res.hva, *(const uint64_t *)in, __ATOMIC_RELAXED); return true;
            default: break;
            }
        }
        memcpy((void *)w.res.hva, in, size);
        return true;
    }

    uint64_t va2 = (va & ~(uint64_t)VMM_PAGE_MASK) + VMM_PAGE_SIZE;
    walk_detail_t w1 = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el, true, mmu_stages(cpu));
    if (!w1.res.ok) {
        vcpu_raise_abort(cpu, va, &w1.res, true, cpu->pc, false);
        return false;
    }
    if (w1.is_device) {
        vcpu_raise_abort(cpu, va, NULL, true, cpu->pc, false);
        return false;
    }
    walk_detail_t w2 = mmu_walk(cpu, va2, MMU_ACCESS_WRITE, cpu->el, true, mmu_stages(cpu));
    if (!w2.res.ok) {
        vcpu_raise_abort(cpu, va2, &w2.res, true, cpu->pc, false);
        return false;
    }
    if (w2.is_device) {
        vcpu_raise_abort(cpu, va2, NULL, true, cpu->pc, false);
        return false;
    }

    uint32_t n1 = (uint32_t)(VMM_PAGE_SIZE - (va & VMM_PAGE_MASK));
    uint32_t n2 = size - n1;
    const uint8_t *src = (const uint8_t *)in;

    if (!w1.res.is_mmio) {
        mmu_tlb_fill(cpu, va, cpu->el, &w1);
        memcpy((void *)w1.res.hva, src, n1);
    } else {
        cpu_fp_leave_guest(cpu);
        for (uint32_t i = 0; i < n1; i++) {
            vmm_mmio_write(cpu->vm, cpu, w1.res.pa + i, 1, src[i]);
        }
    }
    if (!w2.res.is_mmio) {
        mmu_tlb_fill(cpu, va2, cpu->el, &w2);
        memcpy((void *)w2.res.hva, src + n1, n2);
    } else {
        cpu_fp_leave_guest(cpu);
        for (uint32_t i = 0; i < n2; i++) {
            vmm_mmio_write(cpu->vm, cpu, w2.res.pa + i, 1, src[n1 + i]);
        }
    }
    return true;
}

bool mmu_read_8_slow(vcpu_t *cpu, uint64_t va, uint8_t *out) {
    return mmu_read_slow_n(cpu, va, 1, out);
}

bool mmu_read_16_slow(vcpu_t *cpu, uint64_t va, uint16_t *out) {
    return mmu_read_slow_n(cpu, va, 2, out);
}

bool mmu_read_32_slow(vcpu_t *cpu, uint64_t va, uint32_t *out) {
    return mmu_read_slow_n(cpu, va, 4, out);
}

bool mmu_read_64_slow(vcpu_t *cpu, uint64_t va, uint64_t *out) {
    return mmu_read_slow_n(cpu, va, 8, out);
}

bool mmu_read_pair_64_slow(vcpu_t *cpu, uint64_t va, uint64_t *out1, uint64_t *out2) {
    if ((va & 7ULL) == 0 && (va & VMM_PAGE_MASK) <= (VMM_PAGE_SIZE - 16U)) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_READ, cpu->el, true, mmu_stages(cpu));
        if (!w.res.ok) {
            vcpu_raise_abort(cpu, va, &w.res, false, cpu->pc, false);
            return false;
        }
        if (w.res.is_mmio) {
            cpu_fp_leave_guest(cpu);
            *out1 = vmm_mmio_read(cpu->vm, cpu, w.res.pa, 8);
            *out2 = vmm_mmio_read(cpu->vm, cpu, w.res.pa + 8, 8);
            return true;
        }
        mmu_tlb_fill(cpu, va, cpu->el, &w);
        __asm__ volatile("ldp %0, %1, [%2]" : "=r"(*out1), "=r"(*out2) : "r"(w.res.hva) : "memory");
        return true;
    }
    uint64_t v1 = 0, v2 = 0;
    if (!mmu_read_64_slow(cpu, va, &v1) || !mmu_read_64_slow(cpu, va + 8ULL, &v2)) {
        return false;
    }
    *out1 = v1;
    *out2 = v2;
    return true;
}

bool mmu_read_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t *out) {
    return mmu_read_slow_n(cpu, va, 16, out);
}

bool mmu_write_8_slow(vcpu_t *cpu, uint64_t va, uint8_t val) {
    return mmu_write_slow_n(cpu, va, 1, &val);
}

bool mmu_write_16_slow(vcpu_t *cpu, uint64_t va, uint16_t val) {
    return mmu_write_slow_n(cpu, va, 2, &val);
}

bool mmu_write_32_slow(vcpu_t *cpu, uint64_t va, uint32_t val) {
    return mmu_write_slow_n(cpu, va, 4, &val);
}

bool mmu_write_64_slow(vcpu_t *cpu, uint64_t va, uint64_t val) {
    return mmu_write_slow_n(cpu, va, 8, &val);
}

bool mmu_write_pair_64_slow(vcpu_t *cpu, uint64_t va, uint64_t val1, uint64_t val2) {
    if ((va & 7ULL) == 0 && (va & VMM_PAGE_MASK) <= (VMM_PAGE_SIZE - 16U)) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el, true, mmu_stages(cpu));
        if (!w.res.ok) {
            vcpu_raise_abort(cpu, va, &w.res, true, cpu->pc, false);
            return false;
        }
        if (w.res.is_mmio) {
            cpu_fp_leave_guest(cpu);
            vmm_mmio_write(cpu->vm, cpu, w.res.pa, 8, val1);
            vmm_mmio_write(cpu->vm, cpu, w.res.pa + 8, 8, val2);
            return true;
        }
        mmu_tlb_fill(cpu, va, cpu->el, &w);
        __asm__ volatile("stp %0, %1, [%2]" : : "r"(val1), "r"(val2), "r"(w.res.hva) : "memory");
        return true;
    }
    return mmu_write_64_slow(cpu, va, val1) && mmu_write_64_slow(cpu, va + 8ULL, val2);
}

bool mmu_write_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t val) {
    return mmu_write_slow_n(cpu, va, 16, &val);
}
