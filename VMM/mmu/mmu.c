#include "mmu.h"

#include "../board/board_internal.h"
#include "../cpu/cpu_fast_layout.h"
#include "../cpu/cpu_internal.h"
#include "../board/machine.h"

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#if defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#endif

static uint16_t mmu_current_asid(const vcpu_t *cpu) {
    uint64_t tcr = cpu->tcr_el1;
    uint64_t ttbr = ((tcr >> 22) & 1ULL) ? cpu->ttbr1_el1 : cpu->ttbr0_el1;
    uint16_t asid = (uint16_t)(ttbr >> 48);
    if (((tcr >> 36) & 1ULL) == 0) {
        asid &= 0x00ffU;
    }
    return asid;
}

static uint32_t mmu_asid_hash(uint16_t asid) {
    return (uint32_t)((((uint32_t)asid * 40503U) >> 6) & SOFTTLB_MASK);
}

void mmu_update_asid_salt(vcpu_t *cpu) {
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    uint16_t asid = mmu_current_asid(cpu);
    uint64_t salt = ((uint64_t)asid << 48) | (uint64_t)mmu_asid_hash(asid);
    if ((cpu->sctlr_el1 & 1ULL) == 0) {
        salt |= (1ULL << 10);
    }
    if ((cpu->sctlr_el1 & 2ULL) != 0) {
        salt |= (1ULL << 11);
    }
    if (!cpu_fp_enabled(cpu)) {
        salt |= (1ULL << SALT_FP_TRAP);
        cpu_fp_leave_guest(cpu);
    }
    if (cpu->tlb_asid_salt != salt) {
        cpu->tlb_asid_salt = salt;
        cpu->ras_epoch++;
    }
}

static void tlb_flush_all_local(vcpu_t *cpu) {
    for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
        for (uint32_t i = 0; i < SOFTTLB_ENTRIES; i++) {
            cpu->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
        }
    }
    cpu->exec_epoch++;
    cpu->ras_epoch++;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
}

static void tlb_flush_asid_local(vcpu_t *cpu, uint16_t asid) {
    uint64_t target_asid = (uint64_t)asid << 48;
    for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
        for (uint32_t i = 0; i < SOFTTLB_ENTRIES; i++) {
            uint64_t tag = cpu->tlb[b][i].tag;
            if ((tag & 3ULL) == 0 && (tag & 0xffff000000000000ULL) == target_asid) {
                cpu->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
            }
        }
    }
    cpu->exec_epoch++;
    cpu->ras_epoch++;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
}

static uint64_t mmu_canonicalize_va(uint64_t va) {
    if ((va >> 55) & 1ULL) {
        return va | 0xff00000000000000ULL;
    }
    return va & 0x00ffffffffffffffULL;
}

static void tlb_flush_va_local(vcpu_t *cpu, uint64_t va, uint16_t asid, bool all_asids) {
    cpu->exec_epoch++;
    cpu->ras_epoch++;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    va = mmu_canonicalize_va(va);
    uint64_t top = va >> 48;
    uint64_t page = va & 0x0000fffffffff000ULL;
    if (top == 0xffffULL) {
        uint32_t idx = (uint32_t)((va >> VMM_PAGE_SHIFT) & SOFTTLB_MASK);
        uint64_t match = page | 0x2ULL;
        for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
            if (cpu->tlb[b][idx].tag == match) {
                cpu->tlb[b][idx].tag = SOFTTLB_TAG_INVALID;
            }
        }
        return;
    }
    if (top != 0ULL) {
        return;
    }
    if (!all_asids) {
        uint32_t idx = (uint32_t)(((va >> VMM_PAGE_SHIFT) ^ mmu_asid_hash(asid)) & SOFTTLB_MASK);
        uint64_t match = page | ((uint64_t)asid << 48);
        for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
            if (cpu->tlb[b][idx].tag == match) {
                cpu->tlb[b][idx].tag = SOFTTLB_TAG_INVALID;
            }
        }
        return;
    }
    for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; b++) {
        for (uint32_t i = 0; i < SOFTTLB_ENTRIES; i++) {
            uint64_t tag = cpu->tlb[b][i].tag;
            if ((tag & 3ULL) == 0 && (tag & 0x0000fffffffff000ULL) == page) {
                cpu->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
            }
        }
    }
}

static void tlb_broadcast_shootdown(vcpu_t *cpu) {
    vmm_machine_t *vm = cpu->vm;
    if (!vm || vm->cpu_count <= 1) {
        return;
    }
    uint64_t target = atomic_fetch_add_explicit(&vm->tlb_epoch, 1, memory_order_seq_cst) + 1;
    if (cpu->tlb_epoch_local != target - 1U) {
        tlb_flush_all_local(cpu);
    }
    __atomic_store_n(&cpu->tlb_epoch_local, target, __ATOMIC_RELEASE);

    if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
        return;
    }

    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (i == cpu->cpu_id) {
            continue;
        }
        vcpu_t *other = vm->cpus[i];
        if (!other) {
            continue;
        }
        uint32_t spins = 0;
        while (__atomic_load_n(&other->tlb_epoch_local, __ATOMIC_ACQUIRE) < target) {
            mmu_poll_shootdown(cpu);
            if (atomic_load_explicit(&other->halted_wfi, memory_order_seq_cst) ||
                atomic_load_explicit(&other->stop_requested, memory_order_acquire) ||
                !atomic_load_explicit(&vm->running, memory_order_acquire) ||
                atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
                break;
            }
            if (++spins < 128) {
#if defined(__aarch64__)
                __asm__ __volatile__("yield");
#endif
            } else {
                sched_yield();
            }
        }
    }
}

void mmu_tlbi_all(vcpu_t *cpu, bool inner_shareable) {
    tlb_flush_all_local(cpu);
    if (inner_shareable) {
        tlb_broadcast_shootdown(cpu);
    }
}

void mmu_tlbi_asid(vcpu_t *cpu, uint16_t asid, bool inner_shareable) {
    tlb_flush_asid_local(cpu, asid);
    if (inner_shareable) {
        tlb_broadcast_shootdown(cpu);
    }
}

void mmu_tlbi_va(vcpu_t *cpu, uint64_t va, uint16_t asid, bool all_asids, bool inner_shareable) {
    tlb_flush_va_local(cpu, va, asid, all_asids);
    if (inner_shareable) {
        tlb_broadcast_shootdown(cpu);
    }
}

void mmu_tlbi_range(vcpu_t *cpu, uint64_t start_va, uint64_t num_pages, uint8_t tg_shift,
                    uint16_t asid, bool all_asids, bool inner_shareable) {
    if (tg_shift < VMM_PAGE_SHIFT || tg_shift > 16) {
        tg_shift = VMM_PAGE_SHIFT;
    }
    uint64_t subpages = 1ULL << (tg_shift - VMM_PAGE_SHIFT);
    uint64_t total_4k = num_pages * subpages;
    if (num_pages == 0) {
        return;
    }
    uint64_t canon_start = mmu_canonicalize_va(start_va);
    if (total_4k >= 128 || (all_asids && (canon_start >> 48) == 0ULL && total_4k >= 16)) {
        if (all_asids) {
            tlb_flush_all_local(cpu);
        } else {
            tlb_flush_asid_local(cpu, asid);
        }
    } else {
        uint64_t va = canon_start & ~(uint64_t)VMM_PAGE_MASK;
        for (uint64_t i = 0; i < total_4k; i++, va += VMM_PAGE_SIZE) {
            tlb_flush_va_local(cpu, va, asid, all_asids);
        }
    }
    if (inner_shareable) {
        tlb_broadcast_shootdown(cpu);
    }
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
        if (rec->epoch != epoch || rec->count > VCPU_ICACHE_PENDING) {
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
    if ((!all && count == 0U) || !vm || vm->cpu_count <= 1) {
        return;
    }

    os_unfair_lock_lock(&vm->icache_lock);
    uint64_t target = atomic_load_explicit(&vm->icache_epoch, memory_order_relaxed);
    bool lagged = icache_apply_ring(cpu, target);
    ++target;
    vmm_icache_shootdown_t *rec = &vm->icache_ring[target & (VMM_ICACHE_RING - 1U)];
    rec->epoch = target;
    rec->count = all ? UINT32_MAX : count;
    memcpy(rec->pa_pages, pages, count * sizeof(pages[0]));
    atomic_store_explicit(&vm->icache_epoch, target, memory_order_seq_cst);
    os_unfair_lock_unlock(&vm->icache_lock);
    if (lagged) {
        vcpu_flush_icache_all(cpu);
    }
    __atomic_store_n(&cpu->icache_epoch_local, target, __ATOMIC_RELEASE);

    if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
        return;
    }

    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (i == cpu->cpu_id) {
            continue;
        }
        vcpu_t *other = vm->cpus[i];
        if (!other) {
            continue;
        }
        uint32_t spins = 0;
        while (__atomic_load_n(&other->icache_epoch_local, __ATOMIC_ACQUIRE) < target) {
            mmu_poll_shootdown(cpu);
            if (atomic_load_explicit(&other->halted_wfi, memory_order_seq_cst) ||
                atomic_load_explicit(&other->stop_requested, memory_order_acquire) ||
                !atomic_load_explicit(&vm->running, memory_order_acquire) ||
                atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
                break;
            }
            if (++spins < 128) {
#if defined(__aarch64__)
                __asm__ __volatile__("yield");
#endif
            } else {
                sched_yield();
            }
        }
    }
}

bool mmu_poll_shootdown(vcpu_t *cpu) {
    vmm_machine_t *vm = cpu ? cpu->vm : NULL;
    if (!vm) {
        return false;
    }
    bool flushed = false;
    uint64_t epoch = atomic_load_explicit(&vm->tlb_epoch, memory_order_acquire);
    if (__builtin_expect(cpu->tlb_epoch_local != epoch, 0)) {
        tlb_flush_all_local(cpu);
        __atomic_store_n(&cpu->tlb_epoch_local, epoch, __ATOMIC_RELEASE);
        flushed = true;
    }
    uint64_t iepoch = atomic_load_explicit(&vm->icache_epoch, memory_order_acquire);
    if (__builtin_expect(cpu->icache_epoch_local != iepoch, 0)) {
        os_unfair_lock_lock(&vm->icache_lock);
        iepoch = atomic_load_explicit(&vm->icache_epoch, memory_order_relaxed);
        bool lagged = icache_apply_ring(cpu, iepoch);
        os_unfair_lock_unlock(&vm->icache_lock);
        if (lagged) {
            vcpu_flush_icache_all(cpu);
        }
        __atomic_store_n(&cpu->icache_epoch_local, iepoch, __ATOMIC_RELEASE);
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
    if (vm->pmem && vm->pmem_size > 0 && gpa >= VMM_PMEM_BASE) {
        uint64_t off = gpa - VMM_PMEM_BASE;
        if (off <= vm->pmem_size && len <= vm->pmem_size - off) {
            return vm->pmem + off;
        }
    }
    return NULL;
}

static uint32_t mmu_ips_bits(uint64_t tcr) {
    static const uint8_t table[8] = {32, 36, 40, 42, 44, 48, 48, 48};
    return table[(tcr >> 32) & 7ULL];
}

typedef struct walk_detail {
    mmu_translation_t res;
    bool is_device;
    bool is_ttbr1;
    bool non_global;
    bool el0_accessible;
    bool canonical_untagged;
} walk_detail_t;

static walk_detail_t mmu_walk(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el) {
    walk_detail_t out = {0};
    vmm_machine_t *vm = cpu->vm;
    CPU_STATS_WALK(cpu);

    if ((cpu->sctlr_el1 & 1ULL) == 0) {
        out.res.pa = va;
        void *hva = mmu_gpa_to_hva(vm, va, 1);
        if (hva) {
            out.res.hva = (uintptr_t)hva;
            out.res.is_mmio = false;
            out.res.ok = true;
        } else if (access == MMU_ACCESS_EXEC) {
            out.res.fault_fsc = 0x10;
            out.res.ok = false;
        } else {
            out.res.is_mmio = true;
            out.res.ok = true;
        }
        return out;
    }

    uint64_t tcr = cpu->tcr_el1;
    bool is_ttbr1 = ((va >> 55) & 1ULL) != 0;
    out.is_ttbr1 = is_ttbr1;

    uint32_t tsz;
    uint32_t tg_bits;
    uint32_t granule_shift;
    bool epd;
    bool tbi;
    uint64_t ttbr;

    if (!is_ttbr1) {
        tsz = (uint32_t)(tcr & 0x3fULL);
        epd = ((tcr >> 7) & 1ULL) != 0;
        tg_bits = (uint32_t)((tcr >> 14) & 3ULL);
        tbi = ((tcr >> 37) & 1ULL) != 0 && !(access == MMU_ACCESS_EXEC && ((tcr >> 51) & 1ULL));
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

    if (epd || tsz < 16 || tsz > 39) {
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

    uint32_t ips_bits = mmu_ips_bits(tcr);
    uint32_t start_shift = granule_shift + (uint32_t)(3 - start_level) * stride;
    uint32_t start_idx_bits = ia_bits - start_shift;
    uint64_t baddr_align_mask = ~((1ULL << (start_idx_bits + 3)) - 1ULL);
    uint64_t table_pa = ttbr & 0x0000ffffffffffffULL & baddr_align_mask;

    if (ips_bits < 48 && (table_pa >> ips_bits) != 0) {
        out.res.fault_fsc = 0x00;
        return out;
    }

    uint32_t ap_table = 0;
    bool uxn_table = false;
    bool pxn_table = false;
    uint64_t desc = 0;
    uint64_t pa = 0;
    int leaf_level = 3;

    for (int level = start_level; level <= 3; level++) {
        uint32_t shift = granule_shift + (uint32_t)(3 - level) * stride;
        uint32_t idx_bits = (level == start_level) ? start_idx_bits : stride;
        uint64_t index = (va >> shift) & ((1ULL << idx_bits) - 1ULL);
        uint64_t desc_pa = table_pa + (index << 3);

        void *desc_hva = mmu_gpa_to_hva(vm, desc_pa, 8);
        if (!desc_hva) {
            out.res.fault_fsc = 0x14U + (uint32_t)level;
            return out;
        }
        desc = __atomic_load_n((const uint64_t *)desc_hva, __ATOMIC_ACQUIRE);

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
            ap_table |= (uint32_t)((desc >> 61) & 3ULL);
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

    if (access != MMU_ACCESS_RESOLVE && ((desc >> 10) & 1ULL) == 0) {
        out.res.fault_fsc = 0x08U + (uint32_t)leaf_level;
        return out;
    }

    uint32_t ap = (uint32_t)((desc >> 6) & 3ULL);
    if (ap_table & 1U) {
        ap &= ~1U;
    }
    if (ap_table & 2U) {
        ap |= 2U;
    }

    bool uxn = ((desc >> 54) & 1ULL) != 0 || uxn_table;
    bool pxn = ((desc >> 53) & 1ULL) != 0 || pxn_table;
    bool el0_accessible = (ap & 1U) != 0;
    bool writable_el0 = (ap == 1U);
    bool writable_el1 = ((ap & 2U) == 0);
    bool wxn = ((cpu->sctlr_el1 >> 19) & 1ULL) != 0;

    bool perm_ok = false;
    if (el == 0) {
        switch (access) {
        case MMU_ACCESS_READ:
            perm_ok = el0_accessible;
            break;
        case MMU_ACCESS_WRITE:
            perm_ok = writable_el0;
            break;
        case MMU_ACCESS_EXEC:
            perm_ok = el0_accessible && !uxn && !(wxn && writable_el0);
            break;
        case MMU_ACCESS_RESOLVE:
            perm_ok = true;
            break;
        }
    } else {
        switch (access) {
        case MMU_ACCESS_READ:
            perm_ok = !(cpu->pan && el0_accessible);
            break;
        case MMU_ACCESS_WRITE:
            perm_ok = writable_el1 && !(cpu->pan && el0_accessible);
            break;
        case MMU_ACCESS_EXEC:
            perm_ok = !pxn && !writable_el0 && !(wxn && writable_el1);
            break;
        case MMU_ACCESS_RESOLVE:
            perm_ok = true;
            break;
        }
    }

    if (!perm_ok) {
        out.res.fault_fsc = 0x0cU + (uint32_t)leaf_level;
        return out;
    }

    uint32_t attr_indx = (uint32_t)((desc >> 2) & 7ULL);
    uint8_t mair_attr = (uint8_t)((cpu->mair_el1 >> (attr_indx * 8)) & 0xffULL);
    out.is_device = (mair_attr & 0xf0U) == 0;
    out.non_global = ((desc >> 11) & 1ULL) != 0;
    out.el0_accessible = el0_accessible;

    out.res.pa = pa;
    void *hva = mmu_gpa_to_hva(vm, pa, 1);
    if (hva) {
        out.res.hva = (uintptr_t)hva;
        out.res.is_mmio = false;
        out.res.ok = true;
    } else if (access == MMU_ACCESS_EXEC) {
        out.res.fault_fsc = 0x10U;
        out.res.ok = false;
    } else {
        out.res.is_mmio = true;
        out.res.ok = true;
    }
    return out;
}

mmu_translation_t mmu_translate(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el) {
    return mmu_walk(cpu, va, access, el).res;
}

static void mmu_tlb_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                         const walk_detail_t *w);

bool mmu_translate_and_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                            mmu_translation_t *out, bool *is_device_out) {
    walk_detail_t w = mmu_walk(cpu, va, access, el);
    *out = w.res;
    if (is_device_out) {
        *is_device_out = w.is_device;
    }
    if (!w.res.ok) {
        return false;
    }
    if (!w.res.is_mmio) {
        mmu_tlb_fill(cpu, va, access, el, &w);
    }
    return true;
}

bool mmu_translate_exec(vcpu_t *cpu, uint64_t va_page, uint8_t el, mmu_translation_t *out, bool *cacheable_out) {
    walk_detail_t w = mmu_walk(cpu, va_page, MMU_ACCESS_EXEC, el);
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
    for (uint32_t b = first; b < first + 3U; ++b) {
        uintptr_t hva;
        if (!mmu_tlb_lookup_aligned(cpu->tlb[b], cpu->tlb_asid_salt, va_page, &hva)) {
            continue;
        }
        if (vm->ram && hva - (uintptr_t)vm->ram < vm->ram_size) {
            *pa_out = VMM_RAM_BASE + (hva - (uintptr_t)vm->ram) + (va & VMM_PAGE_MASK);
            return true;
        }
        if (vm->pmem && hva - (uintptr_t)vm->pmem < vm->pmem_size) {
            *pa_out = VMM_PMEM_BASE + (hva - (uintptr_t)vm->pmem) + (va & VMM_PAGE_MASK);
            return true;
        }
    }
    walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_RESOLVE, cpu->el);
    if (!w.res.ok) {
        return false;
    }
    *pa_out = w.res.pa;
    return true;
}

static void mmu_tlb_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                         const walk_detail_t *w) {
    if ((cpu->sctlr_el1 & 1ULL) == 0 || !w->res.ok || w->res.is_mmio || w->is_device ||
        !w->canonical_untagged) {
        return;
    }
    if (el == 1 && access != MMU_ACCESS_EXEC && w->el0_accessible) {
        return;
    }
    if (w->is_ttbr1 && w->non_global) {
        return;
    }

    uint64_t page_pa = w->res.pa & ~(uint64_t)VMM_PAGE_MASK;
    void *page_hva = mmu_gpa_to_hva(cpu->vm, page_pa, VMM_PAGE_SIZE);
    if (!page_hva) {
        return;
    }

    mmu_update_asid_salt(cpu);

    softtlb_bank_t bank_id;
    if (el == 0) {
        bank_id = (access == MMU_ACCESS_READ) ? SOFTTLB_EL0_R :
                  (access == MMU_ACCESS_WRITE) ? SOFTTLB_EL0_W : SOFTTLB_EL0_X;
    } else {
        bank_id = (access == MMU_ACCESS_READ) ? SOFTTLB_EL1_R :
                  (access == MMU_ACCESS_WRITE) ? SOFTTLB_EL1_W : SOFTTLB_EL1_X;
    }

    uint64_t page_va = va & 0x0000fffffffff000ULL;
    uint32_t idx;
    uint64_t tag;
    if (!w->is_ttbr1) {
        idx = (uint32_t)(((va >> VMM_PAGE_SHIFT) ^ (uint32_t)cpu->tlb_asid_salt) & SOFTTLB_MASK);
        tag = page_va | (cpu->tlb_asid_salt & 0xffff000000000000ULL);
    } else {
        idx = (uint32_t)((va >> VMM_PAGE_SHIFT) & SOFTTLB_MASK);
        tag = page_va | 0x2ULL;
    }

    softtlb_entry_t *entry = &cpu->tlb[bank_id][idx];
    entry->addend = (uintptr_t)page_hva - (uintptr_t)(va & ~(uint64_t)VMM_PAGE_MASK);
    entry->tag = tag;
}

static void mmu_raise_data_abort(vcpu_t *cpu, uint64_t far, uint32_t fsc, bool is_write) {
    uint32_t ec = (cpu->el == 0) ? 0x24U : 0x25U;
    uint32_t esr = (ec << 26) | (1U << 25) | (is_write ? (1U << 6) : 0U) | (fsc & 0x3fU);
    vcpu_raise_exception(cpu, esr, far, cpu->pc);
}

static void mmu_raise_insn_abort(vcpu_t *cpu, uint64_t far, uint32_t fsc) {
    uint32_t ec = (cpu->el == 0) ? 0x20U : 0x21U;
    uint32_t esr = (ec << 26) | (1U << 25) | (fsc & 0x3fU);
    vcpu_raise_exception(cpu, esr, far, cpu->pc);
}

static bool mmu_read_slow_n(vcpu_t *cpu, uint64_t va, uint32_t size, void *out) {
    bool unaligned = size > 1 && (va & (size - 1)) != 0;
    if (unaligned && (cpu->sctlr_el1 & 2ULL) != 0) {
        mmu_raise_data_abort(cpu, va, 0x21U, false);
        return false;
    }

    bool crosses = size > 1 && (((va + (size - 1)) ^ va) & (1ULL << VMM_PAGE_SHIFT)) != 0;
    if (!crosses) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_READ, cpu->el);
        if (!w.res.ok) {
            mmu_raise_data_abort(cpu, va, w.res.fault_fsc, false);
            return false;
        }
        if (unaligned && w.is_device) {
            mmu_raise_data_abort(cpu, va, 0x21U, false);
            return false;
        }
        if (w.res.is_mmio) {
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
        mmu_tlb_fill(cpu, va, MMU_ACCESS_READ, cpu->el, &w);
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
    walk_detail_t w1 = mmu_walk(cpu, va, MMU_ACCESS_READ, cpu->el);
    if (!w1.res.ok) {
        mmu_raise_data_abort(cpu, va, w1.res.fault_fsc, false);
        return false;
    }
    if (w1.is_device) {
        mmu_raise_data_abort(cpu, va, 0x21U, false);
        return false;
    }
    walk_detail_t w2 = mmu_walk(cpu, va2, MMU_ACCESS_READ, cpu->el);
    if (!w2.res.ok) {
        mmu_raise_data_abort(cpu, va2, w2.res.fault_fsc, false);
        return false;
    }
    if (w2.is_device) {
        mmu_raise_data_abort(cpu, va2, 0x21U, false);
        return false;
    }

    uint32_t n1 = (uint32_t)(VMM_PAGE_SIZE - (va & VMM_PAGE_MASK));
    uint32_t n2 = size - n1;
    uint8_t *dst = (uint8_t *)out;

    if (!w1.res.is_mmio) {
        mmu_tlb_fill(cpu, va, MMU_ACCESS_READ, cpu->el, &w1);
        memcpy(dst, (const void *)w1.res.hva, n1);
    } else {
        for (uint32_t i = 0; i < n1; i++) {
            dst[i] = (uint8_t)vmm_mmio_read(cpu->vm, cpu, w1.res.pa + i, 1);
        }
    }
    if (!w2.res.is_mmio) {
        mmu_tlb_fill(cpu, va2, MMU_ACCESS_READ, cpu->el, &w2);
        memcpy(dst + n1, (const void *)w2.res.hva, n2);
    } else {
        for (uint32_t i = 0; i < n2; i++) {
            dst[n1 + i] = (uint8_t)vmm_mmio_read(cpu->vm, cpu, w2.res.pa + i, 1);
        }
    }
    return true;
}

static bool mmu_write_slow_n(vcpu_t *cpu, uint64_t va, uint32_t size, const void *in) {
    bool unaligned = size > 1 && (va & (size - 1)) != 0;
    if (unaligned && (cpu->sctlr_el1 & 2ULL) != 0) {
        mmu_raise_data_abort(cpu, va, 0x21U, true);
        return false;
    }

    bool crosses = size > 1 && (((va + (size - 1)) ^ va) & (1ULL << VMM_PAGE_SHIFT)) != 0;
    if (!crosses) {
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el);
        if (!w.res.ok) {
            mmu_raise_data_abort(cpu, va, w.res.fault_fsc, true);
            return false;
        }
        if (unaligned && w.is_device) {
            mmu_raise_data_abort(cpu, va, 0x21U, true);
            return false;
        }
        if (w.res.is_mmio) {
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
        mmu_tlb_fill(cpu, va, MMU_ACCESS_WRITE, cpu->el, &w);
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
    walk_detail_t w1 = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el);
    if (!w1.res.ok) {
        mmu_raise_data_abort(cpu, va, w1.res.fault_fsc, true);
        return false;
    }
    if (w1.is_device) {
        mmu_raise_data_abort(cpu, va, 0x21U, true);
        return false;
    }
    walk_detail_t w2 = mmu_walk(cpu, va2, MMU_ACCESS_WRITE, cpu->el);
    if (!w2.res.ok) {
        mmu_raise_data_abort(cpu, va2, w2.res.fault_fsc, true);
        return false;
    }
    if (w2.is_device) {
        mmu_raise_data_abort(cpu, va2, 0x21U, true);
        return false;
    }

    uint32_t n1 = (uint32_t)(VMM_PAGE_SIZE - (va & VMM_PAGE_MASK));
    uint32_t n2 = size - n1;
    const uint8_t *src = (const uint8_t *)in;

    if (!w1.res.is_mmio) {
        mmu_tlb_fill(cpu, va, MMU_ACCESS_WRITE, cpu->el, &w1);
        memcpy((void *)w1.res.hva, src, n1);
    } else {
        for (uint32_t i = 0; i < n1; i++) {
            vmm_mmio_write(cpu->vm, cpu, w1.res.pa + i, 1, src[i]);
        }
    }
    if (!w2.res.is_mmio) {
        mmu_tlb_fill(cpu, va2, MMU_ACCESS_WRITE, cpu->el, &w2);
        memcpy((void *)w2.res.hva, src + n1, n2);
    } else {
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

bool mmu_write_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t val) {
    return mmu_write_slow_n(cpu, va, 16, &val);
}

bool mmu_fetch_32_slow(vcpu_t *cpu, uint64_t va, uint32_t *out) {
    if ((va & 3ULL) != 0) {
        uint32_t esr = (0x22U << 26) | (1U << 25);
        vcpu_raise_exception(cpu, esr, va, cpu->pc);
        return false;
    }
    walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_EXEC, cpu->el);
    if (!w.res.ok) {
        mmu_raise_insn_abort(cpu, va, w.res.fault_fsc);
        return false;
    }
    mmu_tlb_fill(cpu, va, MMU_ACCESS_EXEC, cpu->el, &w);
    memcpy(out, (const void *)w.res.hva, 4);
    return true;
}

static uint32_t pv_crc32_be(uint32_t crc, const uint8_t *p, size_t n) {
#if defined(__ARM_FEATURE_CRC32)
    uint32_t c = __builtin_bitreverse32(crc);
    while (n >= 8) {
        uint64_t word;
        memcpy(&word, p, 8);
        c = __crc32d(c, __builtin_bitreverse64(__builtin_bswap64(word)));
        p += 8;
        n -= 8;
    }
    while (n > 0) {
        c = __crc32b(c, __builtin_bitreverse8(*p++));
        n--;
    }
    return __builtin_bitreverse32(c);
#else
    uint32_t c = crc;
    while (n > 0) {
        c ^= ((uint32_t)(*p++) << 24);
        for (int b = 0; b < 8; b++) {
            c = (c & 0x80000000U) ? ((c << 1) ^ 0x04c11db7U) : (c << 1);
        }
        n--;
    }
    return c;
#endif
}

static bool pv_clear_page(vcpu_t *cpu, uint64_t dst_va) {
    uint8_t *ptrs[2];
    size_t lens[2];
    size_t count = 0;
    size_t left = VMM_PAGE_SIZE;
    uint64_t va = dst_va;

    while (left > 0) {
        size_t chunk = VMM_PAGE_SIZE - (size_t)(va & VMM_PAGE_MASK);
        if (chunk > left) {
            chunk = left;
        }
        walk_detail_t w = mmu_walk(cpu, va, MMU_ACCESS_WRITE, cpu->el);
        if (!w.res.ok || w.res.is_mmio) {
            return false;
        }
        ptrs[count] = (uint8_t *)w.res.hva;
        lens[count] = chunk;
        count++;
        va += chunk;
        left -= chunk;
    }
    for (size_t i = 0; i < count; i++) {
        memset(ptrs[i], 0, lens[i]);
    }
    return true;
}

static bool pv_copy_page(vcpu_t *cpu, uint64_t dst_va, uint64_t src_va) {
    struct {
        uint8_t *dst;
        const uint8_t *src;
        size_t len;
    } spans[4];
    size_t count = 0;
    size_t left = VMM_PAGE_SIZE;
    uint64_t d = dst_va;
    uint64_t s = src_va;

    while (left > 0) {
        size_t cd = VMM_PAGE_SIZE - (size_t)(d & VMM_PAGE_MASK);
        size_t cs = VMM_PAGE_SIZE - (size_t)(s & VMM_PAGE_MASK);
        size_t chunk = cd < cs ? cd : cs;
        if (chunk > left) {
            chunk = left;
        }
        walk_detail_t ws = mmu_walk(cpu, s, MMU_ACCESS_READ, cpu->el);
        if (!ws.res.ok || ws.res.is_mmio) {
            return false;
        }
        walk_detail_t wd = mmu_walk(cpu, d, MMU_ACCESS_WRITE, cpu->el);
        if (!wd.res.ok || wd.res.is_mmio) {
            return false;
        }
        spans[count].dst = (uint8_t *)wd.res.hva;
        spans[count].src = (const uint8_t *)ws.res.hva;
        spans[count].len = chunk;
        count++;
        d += chunk;
        s += chunk;
        left -= chunk;
    }
    for (size_t i = 0; i < count; i++) {
        memmove(spans[i].dst, spans[i].src, spans[i].len);
    }
    return true;
}

static uint64_t pv_copy_user(vcpu_t *cpu, uint64_t dst_va, uint64_t src_va, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    uint8_t dst_el = ((dst_va >> 55) & 1ULL) ? 1 : 0;
    uint8_t src_el = ((src_va >> 55) & 1ULL) ? 1 : 0;
    uint64_t d = dst_va;
    uint64_t s = src_va;
    uint64_t left = len;

    size_t cd0 = VMM_PAGE_SIZE - (size_t)(d & VMM_PAGE_MASK);
    size_t cs0 = VMM_PAGE_SIZE - (size_t)(s & VMM_PAGE_MASK);
    size_t chunk0 = cd0 < cs0 ? cd0 : cs0;
    if (chunk0 >= left) {
        walk_detail_t ws = mmu_walk(cpu, s, MMU_ACCESS_READ, src_el);
        if (!ws.res.ok || ws.res.is_mmio) {
            return UINT64_MAX;
        }
        walk_detail_t wd = mmu_walk(cpu, d, MMU_ACCESS_WRITE, dst_el);
        if (!wd.res.ok || wd.res.is_mmio) {
            return UINT64_MAX;
        }
        memmove((void *)wd.res.hva, (const void *)ws.res.hva, (size_t)left);
        return 0;
    }

    while (left > 0) {
        size_t cd = VMM_PAGE_SIZE - (size_t)(d & VMM_PAGE_MASK);
        size_t cs = VMM_PAGE_SIZE - (size_t)(s & VMM_PAGE_MASK);
        size_t chunk = cd < cs ? cd : cs;
        if (chunk > left) {
            chunk = (size_t)left;
        }
        walk_detail_t ws = mmu_walk(cpu, s, MMU_ACCESS_READ, src_el);
        if (!ws.res.ok || ws.res.is_mmio) {
            return UINT64_MAX;
        }
        walk_detail_t wd = mmu_walk(cpu, d, MMU_ACCESS_WRITE, dst_el);
        if (!wd.res.ok || wd.res.is_mmio) {
            return UINT64_MAX;
        }
        d += chunk;
        s += chunk;
        left -= chunk;
    }

    d = dst_va;
    s = src_va;
    left = len;
    while (left > 0) {
        size_t cd = VMM_PAGE_SIZE - (size_t)(d & VMM_PAGE_MASK);
        size_t cs = VMM_PAGE_SIZE - (size_t)(s & VMM_PAGE_MASK);
        size_t chunk = cd < cs ? cd : cs;
        if (chunk > left) {
            chunk = (size_t)left;
        }
        walk_detail_t ws = mmu_walk(cpu, s, MMU_ACCESS_READ, src_el);
        walk_detail_t wd = mmu_walk(cpu, d, MMU_ACCESS_WRITE, dst_el);
        memmove((void *)wd.res.hva, (const void *)ws.res.hva, chunk);
        d += chunk;
        s += chunk;
        left -= chunk;
    }
    return 0;
}

static bool pv_crc32_be_guest(vcpu_t *cpu, uint32_t crc, uint64_t src_va, uint64_t len,
                              uint32_t *out_crc) {
    uint64_t s = src_va;
    uint64_t left = len;
    while (left > 0) {
        size_t chunk = VMM_PAGE_SIZE - (size_t)(s & VMM_PAGE_MASK);
        if (chunk > left) {
            chunk = (size_t)left;
        }
        walk_detail_t ws = mmu_walk(cpu, s, MMU_ACCESS_READ, cpu->el);
        if (!ws.res.ok || ws.res.is_mmio) {
            return false;
        }
        crc = pv_crc32_be(crc, (const uint8_t *)ws.res.hva, chunk);
        s += chunk;
        left -= chunk;
    }
    *out_crc = crc;
    return true;
}

bool mmu_handle_pv_hvc(vcpu_t *cpu, uint32_t func_id) {
    switch (func_id) {
    case SWIPTERM_HVC_CLEAR_PAGE:
        cpu->x[0] = pv_clear_page(cpu, cpu->x[1]) ? 0ULL : (uint64_t)-1;
        return true;

    case SWIPTERM_HVC_COPY_PAGE:
        cpu->x[0] = pv_copy_page(cpu, cpu->x[1], cpu->x[2]) ? 0ULL : (uint64_t)-1;
        return true;

    case SWIPTERM_HVC_COPY_USER:
        cpu->x[0] = pv_copy_user(cpu, cpu->x[1], cpu->x[2], cpu->x[3]);
        return true;

    case SWIPTERM_HVC_CRC32_BE: {
        uint32_t crc = 0;
        if (pv_crc32_be_guest(cpu, (uint32_t)cpu->x[1], cpu->x[2], cpu->x[3], &crc)) {
            cpu->x[0] = (uint64_t)crc;
        } else {
            cpu->x[0] = UINT64_MAX;
        }
        return true;
    }

    case SWIPTERM_HVC_TLBI_RANGE: {
        uint64_t start_va = cpu->x[1];
        uint64_t num_pages = cpu->x[2];
        uint8_t tg_shift = (cpu->x[3] == 12 || cpu->x[3] == 14 || cpu->x[3] == 16)
                               ? (uint8_t)cpu->x[3]
                               : (uint8_t)VMM_PAGE_SHIFT;
        uint16_t asid = (uint16_t)(cpu->x[4] & 0xffffU);
        bool all_asids = (cpu->x[3] == 0) || (cpu->x[4] >> 16) != 0;
        mmu_tlbi_range(cpu, start_va, num_pages, tg_shift, asid, all_asids, true);
        cpu->x[0] = 0;
        return true;
    }

    case SWIPTERM_HVC_QSPIN_WAIT:
        vmm_vcpu_qspin_wait(cpu, cpu->x[1], (uint8_t)cpu->x[2]);
        cpu->x[0] = 0;
        return true;

    case SWIPTERM_HVC_QSPIN_KICK:
        vmm_vcpu_qspin_kick(cpu->vm, (uint32_t)cpu->x[1]);
        cpu->x[0] = 0;
        return true;

    default:
        return vmm_handle_psci(cpu);
    }
}
