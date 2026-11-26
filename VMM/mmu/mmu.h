#ifndef VMM_MMU_MMU_H
#define VMM_MMU_MMU_H

#include "../cpu/cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum : uint64_t {
    MMU_SALT_MMU_OFF = 1ULL << 2,
    MMU_SALT_PAN_OFF = 1ULL << 3,
    MMU_SALT_USER_TAG = 0xffff000000000ffcULL,
    MMU_SALT_ALIGN = 1ULL << 16,
    MMU_SALT_FP_TRAP = 1ULL << 17,
};

typedef enum mmu_access : uint8_t {
    MMU_ACCESS_READ = 0,
    MMU_ACCESS_WRITE = 1,
    MMU_ACCESS_EXEC = 2,
    MMU_ACCESS_RESOLVE = 3,
} mmu_access_t;

enum : uint32_t {
    MMU_STAGE2_PTW = 1U << 0,
    MMU_STAGE2_OUT = 1U << 1,
    MMU_STAGE2 = MMU_STAGE2_PTW | MMU_STAGE2_OUT,
};

typedef struct mmu_translation {
    uint64_t pa;
    uint64_t ipa;
    uintptr_t hva;
    uint32_t fault_fsc;
    bool is_mmio;
    bool ok;
    bool s2_fault;
    bool s1ptw;
    bool ptw_write;
} mmu_translation_t;

mmu_translation_t mmu_translate(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el, uint32_t stages);
bool mmu_translate_and_fill(vcpu_t *cpu, uint64_t va, mmu_access_t access, uint8_t el,
                            mmu_translation_t *out, bool *is_device_out);
bool mmu_translate_exec(vcpu_t *cpu, uint64_t va_page, uint8_t el, mmu_translation_t *out, bool *cacheable_out);
bool mmu_resolve_va_pa(vcpu_t *cpu, uint64_t va, uint64_t *pa_out);

bool mmu_read_8_slow(vcpu_t *cpu, uint64_t va, uint8_t *out);
bool mmu_read_16_slow(vcpu_t *cpu, uint64_t va, uint16_t *out);
bool mmu_read_32_slow(vcpu_t *cpu, uint64_t va, uint32_t *out);
bool mmu_read_64_slow(vcpu_t *cpu, uint64_t va, uint64_t *out);
bool mmu_read_pair_64_slow(vcpu_t *cpu, uint64_t va, uint64_t *out1, uint64_t *out2);
bool mmu_read_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t *out);

bool mmu_write_8_slow(vcpu_t *cpu, uint64_t va, uint8_t val);
bool mmu_write_16_slow(vcpu_t *cpu, uint64_t va, uint16_t val);
bool mmu_write_32_slow(vcpu_t *cpu, uint64_t va, uint32_t val);
bool mmu_write_64_slow(vcpu_t *cpu, uint64_t va, uint64_t val);
bool mmu_write_pair_64_slow(vcpu_t *cpu, uint64_t va, uint64_t val1, uint64_t val2);
bool mmu_write_128_slow(vcpu_t *cpu, uint64_t va, __uint128_t val);

static inline bool mmu_tlb_lookup_aligned(const softtlb_entry_t *bank, uint64_t asid_salt,
                                          uint64_t va, uintptr_t *hva_out) {
    uint64_t top = va >> 48;
    uint64_t page = va & 0x0000fffffffff000ULL;
    uint32_t idx;
    uint64_t tag;
    if (__builtin_expect(top == 0ULL, 1)) {
        idx = (uint32_t)(((va >> VMM_PAGE_SHIFT) ^ (uint32_t)asid_salt) & SOFTTLB_MASK);
        tag = page | (asid_salt & MMU_SALT_USER_TAG);
    } else if (__builtin_expect(top == 0xffffULL, 1)) {
        idx = (uint32_t)((va >> VMM_PAGE_SHIFT) & SOFTTLB_MASK);
        tag = page | SOFTTLB_TAG_GLOBAL | (asid_salt & MMU_SALT_MMU_OFF);
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
            (asid_salt & MMU_SALT_ALIGN) != 0) {
            return false;
        }
    }
    return mmu_tlb_lookup_aligned(bank, asid_salt, va, hva_out);
}

void mmu_update_asid_salt(vcpu_t *cpu);
void mmu_tlbi_all(vcpu_t *cpu);
void mmu_tlbi(vcpu_t *cpu, vmm_tlbi_t op, bool inner_shareable);
bool mmu_poll_shootdown(vcpu_t *cpu);
bool mmu_icache_sync(vcpu_t *cpu);
void *mmu_gpa_to_hva(const vmm_machine_t *vm, uint64_t gpa, size_t len);
void mmu_shm_map(vmm_machine_t *vm, uint64_t offset, void *hva, uint64_t size);
void mmu_shm_unmap(vmm_machine_t *vm, uint64_t offset, uint64_t size);
void mmu_host_flush(vmm_machine_t *vm);

#endif
