#ifndef VMM_MMU_MMU_H
#define VMM_MMU_MMU_H

#include "../cpu/cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef enum mmu_access : uint8_t {
    MMU_ACCESS_READ = 0,
    MMU_ACCESS_WRITE = 1,
    MMU_ACCESS_EXEC = 2,
    MMU_ACCESS_RESOLVE = 3,
} mmu_access_t;

typedef struct mmu_translation {
    uint64_t pa;
    uintptr_t hva;
    uint32_t fault_fsc;
    bool is_mmio;
    bool ok;
} mmu_translation_t;

mmu_translation_t mmu_translate(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el);
bool mmu_translate_and_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                            mmu_translation_t *out, bool *is_device_out);
bool mmu_translate_exec(vcpu_t *cpu, uint64_t va_page, uint8_t el, mmu_translation_t *out, bool *cacheable_out);
bool mmu_resolve_va_pa(vcpu_t *cpu, uint64_t va, uint64_t *pa_out);

bool mmu_read_8_slow(vcpu_t *cpu, uint64_t va, uint8_t *out);
bool mmu_read_16_slow(vcpu_t *cpu, uint64_t va, uint16_t *out);
bool mmu_read_32_slow(vcpu_t *cpu, uint64_t va, uint32_t *out);
bool mmu_read_64_slow(vcpu_t *cpu, uint64_t va, uint64_t *out);
bool mmu_read_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t *out);

bool mmu_write_8_slow(vcpu_t *cpu, uint64_t va, uint8_t val);
bool mmu_write_16_slow(vcpu_t *cpu, uint64_t va, uint16_t val);
bool mmu_write_32_slow(vcpu_t *cpu, uint64_t va, uint32_t val);
bool mmu_write_64_slow(vcpu_t *cpu, uint64_t va, uint64_t val);
bool mmu_write_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t val);

bool mmu_fetch_32_slow(vcpu_t *cpu, uint64_t va, uint32_t *out);

static inline bool mmu_tlb_lookup_aligned(const softtlb_entry_t *bank, uint64_t asid_salt,
                                          uint64_t va, uintptr_t *hva_out) {
    uint64_t top = va >> 48;
    uint64_t mmu_off = (asid_salt & (1ULL << 10)) >> 8;
    uint64_t page = va & 0x0000fffffffff000ULL;
    uint32_t idx;
    uint64_t tag;
    if (__builtin_expect(top == 0ULL, 1)) {
        idx = (uint32_t)(((va >> VMM_PAGE_SHIFT) ^ (uint32_t)asid_salt) & SOFTTLB_MASK);
        tag = page | (asid_salt & 0xffff000000000000ULL) | mmu_off;
    } else if (__builtin_expect(top == 0xffffULL, 1)) {
        idx = (uint32_t)((va >> VMM_PAGE_SHIFT) & SOFTTLB_MASK);
        tag = page | 0x2ULL | mmu_off;
    } else {
        return false;
    }
    const softtlb_entry_t *entry = &bank[idx];
    if (__builtin_expect(entry->tag == tag, 1)) {
        *hva_out = (uintptr_t)va + entry->addend;
        return true;
    }
    return false;
}

static inline bool mmu_tlb_lookup(const softtlb_entry_t *bank, uint64_t asid_salt,
                                  uint64_t va, uint64_t size, uintptr_t *hva_out) {
    if (__builtin_expect(size > 1 && (va & (size - 1)) != 0, 0)) {
        if ((((va + (size - 1)) ^ va) & (1ULL << VMM_PAGE_SHIFT)) != 0 ||
            (asid_salt & (1ULL << 11)) != 0) {
            return false;
        }
    }
    return mmu_tlb_lookup_aligned(bank, asid_salt, va, hva_out);
}

static inline bool mmu_read_8(vcpu_t *cpu, uint64_t va, uint8_t *out) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 1, &hva), 1)) {
        memcpy(out, (const void *)hva, 1);
        return true;
    }
    return mmu_read_8_slow(cpu, va, out);
}

static inline bool mmu_read_16(vcpu_t *cpu, uint64_t va, uint16_t *out) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 2, &hva), 1)) {
        memcpy(out, (const void *)hva, 2);
        return true;
    }
    return mmu_read_16_slow(cpu, va, out);
}

static inline bool mmu_read_32(vcpu_t *cpu, uint64_t va, uint32_t *out) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 4, &hva), 1)) {
        memcpy(out, (const void *)hva, 4);
        return true;
    }
    return mmu_read_32_slow(cpu, va, out);
}

static inline bool mmu_read_64(vcpu_t *cpu, uint64_t va, uint64_t *out) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 8, &hva), 1)) {
        memcpy(out, (const void *)hva, 8);
        return true;
    }
    return mmu_read_64_slow(cpu, va, out);
}

static inline bool mmu_read_128(vcpu_t *cpu, uint64_t va, __uint128_t *out) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_R : SOFTTLB_EL0_R];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 16, &hva), 1)) {
        memcpy(out, (const void *)hva, 16);
        return true;
    }
    return mmu_read_128_slow(cpu, va, out);
}

static inline bool mmu_write_8(vcpu_t *cpu, uint64_t va, uint8_t val) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 1, &hva), 1)) {
        memcpy((void *)hva, &val, 1);
        return true;
    }
    return mmu_write_8_slow(cpu, va, val);
}

static inline bool mmu_write_16(vcpu_t *cpu, uint64_t va, uint16_t val) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 2, &hva), 1)) {
        memcpy((void *)hva, &val, 2);
        return true;
    }
    return mmu_write_16_slow(cpu, va, val);
}

static inline bool mmu_write_32(vcpu_t *cpu, uint64_t va, uint32_t val) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 4, &hva), 1)) {
        memcpy((void *)hva, &val, 4);
        return true;
    }
    return mmu_write_32_slow(cpu, va, val);
}

static inline bool mmu_write_64(vcpu_t *cpu, uint64_t va, uint64_t val) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 8, &hva), 1)) {
        memcpy((void *)hva, &val, 8);
        return true;
    }
    return mmu_write_64_slow(cpu, va, val);
}

static inline bool mmu_write_128(vcpu_t *cpu, uint64_t va, __uint128_t val) {
    uintptr_t hva;
    const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_W : SOFTTLB_EL0_W];
    if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 16, &hva), 1)) {
        memcpy((void *)hva, &val, 16);
        return true;
    }
    return mmu_write_128_slow(cpu, va, val);
}

static inline bool mmu_fetch_32(vcpu_t *cpu, uint64_t va, uint32_t *out) {
    if (__builtin_expect((va & 3ULL) == 0, 1)) {
        uintptr_t hva;
        const softtlb_entry_t *bank = cpu->tlb[cpu->el ? SOFTTLB_EL1_X : SOFTTLB_EL0_X];
        if (__builtin_expect(mmu_tlb_lookup(bank, cpu->tlb_asid_salt, va, 1, &hva), 1)) {
            memcpy(out, (const void *)hva, 4);
            return true;
        }
    }
    return mmu_fetch_32_slow(cpu, va, out);
}

void mmu_update_asid_salt(vcpu_t *cpu);
void mmu_tlbi_all(vcpu_t *cpu, bool inner_shareable);
void mmu_tlbi_asid(vcpu_t *cpu, uint16_t asid, bool inner_shareable);
void mmu_tlbi_va(vcpu_t *cpu, uint64_t va, uint16_t asid, bool all_asids, bool inner_shareable);
void mmu_tlbi_range(vcpu_t *cpu, uint64_t start_va, uint64_t num_pages, uint8_t tg_shift,
                    uint16_t asid, bool all_asids, bool inner_shareable);
bool mmu_poll_shootdown(vcpu_t *cpu);
void mmu_icache_broadcast(vcpu_t *cpu);
void *mmu_gpa_to_hva(const vmm_machine_t *vm, uint64_t gpa, size_t len);
bool mmu_handle_pv_hvc(vcpu_t *cpu, uint32_t func_id);

#endif
