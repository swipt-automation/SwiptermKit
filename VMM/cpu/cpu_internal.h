#ifndef VMM_CPU_CPU_INTERNAL_H
#define VMM_CPU_CPU_INTERNAL_H

#include "cpu.h"
#include "../board/machine.h"
#include "../mmu/mmu.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CPU_ALWAYS_INLINE static inline __attribute__((always_inline))
#define CPU_NOINLINE __attribute__((noinline))
#define CPU_MUSTTAIL [[clang::musttail]]
#if defined(__clang__) && defined(__aarch64__)
#define CPU_COLD_MISS __attribute__((cold, noinline, preserve_most))
#else
#define CPU_COLD_MISS __attribute__((cold, noinline))
#endif

#define CPU_YIELD_FAST(cpu, ir)                                                \
    do {                                                                       \
        if ((ir)->fast_offset != 0U) {                                         \
            (cpu)->fast_next = (ir);                                           \
            return;                                                            \
        }                                                                      \
    } while (0)

#ifdef CPU_STATS
void cpu_stats_cold(vcpu_t *cpu, const ir_insn_t *ir);
void cpu_stats_walk(vcpu_t *cpu);
#define CPU_STATS_COLD(cpu, ir) cpu_stats_cold((cpu), (ir))
#define CPU_STATS_WALK(cpu) cpu_stats_walk((cpu))
#else
#define CPU_STATS_COLD(cpu, ir) ((void)0)
#define CPU_STATS_WALK(cpu) ((void)0)
#endif

#define CPU_NEXT(cpu, next_ir, tlb_r, tlb_w)                                   \
    do {                                                                       \
        const ir_insn_t *_cpu_next = (next_ir);                                \
        CPU_YIELD_FAST((cpu), _cpu_next);                                      \
        CPU_STATS_COLD((cpu), _cpu_next);                                      \
        CPU_MUSTTAIL return _cpu_next->handler((cpu), _cpu_next, (tlb_r),      \
                                               (tlb_w));                       \
    } while (0)

enum {
    IR_PAGE_INSNS = 1024,
    TILE_SHIFT = 5,
    TILE_INSNS = 1U << TILE_SHIFT,
    ICACHE_SETS = 512,
    ICACHE_SET_MASK = ICACHE_SETS - 1,
    ICACHE_WAYS = 4,
    EXEC_CACHE_ENTRIES = 16384,
    EXEC_CACHE_MASK = EXEC_CACHE_ENTRIES - 1,
    RAS_SIZE = 16,
    RAS_MASK = RAS_SIZE - 1,
    BACKEDGE_BUDGET_INIT = 1024,
};

enum : uint16_t {
    IR_FLAG_NONE = 0,
    IR_FLAG_INPAGE = 1U << 0,
    IR_FLAG_WIDE = 1U << 1,
    IR_FLAG_SIGN = 1U << 2,
    IR_FLAG_SETFLAGS = 1U << 3,
    IR_FLAG_SUB = 1U << 4,
    IR_FLAG_INV = 1U << 5,
    IR_FLAG_POST = 1U << 6,
    IR_FLAG_PRE = 1U << 7,
    IR_FLAG_SIMD = 1U << 8,
};

enum : uint32_t {
    CPU_FPCR_MASK = 0x07C80000U,
    CPU_FPSR_MASK = 0x0800009FU,
    CPU_FPSR_QC = 1U << 27,
    CPU_FPSR_IOC = 1U << 0,
    CPU_FPSR_DZC = 1U << 1,
    CPU_FPSR_OFC = 1U << 2,
    CPU_FPSR_UFC = 1U << 3,
    CPU_FPSR_IXC = 1U << 4,
    CPU_FPSR_IDC = 1U << 7,
};

typedef struct cpu_jit_page {
    uint32_t entry[IR_PAGE_INSNS];
    uint32_t word[IR_PAGE_INSNS];
} cpu_jit_page_t;

typedef struct ir_page {
    uint64_t pa_page;
    uint64_t va_page;
    uintptr_t hva_page;
    uint32_t decoded_mask;
    uint32_t gen;
    cpu_jit_page_t *jit;
    uint8_t el;
    ir_insn_t insns[IR_PAGE_INSNS + 1];
} ir_page_t;

typedef struct exec_cache_entry {
    uint64_t tag;
    ir_page_t *pg;
    uint64_t exec_epoch;
    uint32_t pg_gen;
    uint32_t pad;
} exec_cache_entry_t;

static_assert(sizeof(exec_cache_entry_t) == 32);

typedef struct ras_entry {
    uint64_t pc;
    const ir_insn_t *ir;
    ir_page_t *pg;
    uint32_t ras_epoch;
    uint32_t pg_gen;
} ras_entry_t;

static_assert(sizeof(ras_entry_t) == 32);

typedef struct cpu_jit_arena {
    uint8_t *rx;
    uint8_t *rw;
    uint32_t size;
    uint32_t used;
    uint32_t base_offset;
    uint32_t region_size;
    void *work;
} cpu_jit_arena_t;

extern _Atomic uint32_t cpu_jit_generation;

uintptr_t cpu_fast_base(void);
void cpu_jit_reserve(void);
bool cpu_jit_arena_acquire(vmm_machine_t *vm, uint32_t cpu_id, cpu_jit_arena_t *out);
void cpu_jit_arena_release(vmm_machine_t *vm);
void cpu_jit_arena_write(const cpu_jit_arena_t *arena, uint32_t offset, const uint32_t *code, uint32_t words);
uint32_t cpu_jit_enter(vcpu_t *cpu, ir_insn_t *ir);
const ir_insn_t *cpu_fast_call(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
bool cpu_jit_native(const ir_insn_t *ir);

typedef struct cpu_icache {
    ir_page_t *cur_page;
    uint64_t cur_exec_epoch;
    uint32_t ras_sp;
    uint32_t next_gen;
    uint32_t host_fpcr;
    uint32_t host_fpsr;
#ifdef CPU_STATS
    uint64_t stat_dispatches;
    struct cpu_stats *stats;
#endif
    alignas(64) ras_entry_t ras[RAS_SIZE];
    exec_cache_entry_t exec_cache[2][EXEC_CACHE_ENTRIES];
    uint8_t plru[ICACHE_SETS];
    ir_page_t *slots[ICACHE_SETS][ICACHE_WAYS];
    ir_page_t *page_pool;
    uint32_t page_count;
    cpu_jit_arena_t jit;
    uint32_t jit_generation;
} cpu_icache_t;

CPU_ALWAYS_INLINE bool cpu_jit_stale(const vcpu_t *cpu) {
    return atomic_load_explicit(&cpu_jit_generation, memory_order_relaxed) !=
           ((const cpu_icache_t *)cpu->icache)->jit_generation;
}

CPU_ALWAYS_INLINE _Atomic uint64_t *cpu_icache_ram_word(const vmm_machine_t *vm, uint64_t pa_page,
                                                       uint64_t *mask_out) {
    uint64_t off = pa_page - VMM_RAM_BASE;
    if (!vm || !vm->icache_ram_bitmap || off >= vm->ram_size) {
        return NULL;
    }
    uint64_t pfn = off >> VMM_PAGE_SHIFT;
    *mask_out = 1ULL << (pfn & 63U);
    return &vm->icache_ram_bitmap[pfn >> 6];
}

CPU_ALWAYS_INLINE uint64_t cpu_ir_pc(const vcpu_t *cpu, const ir_insn_t *ir) {
    const ir_page_t *pg = ((const cpu_icache_t *)cpu->icache)->cur_page;
    return pg->va_page + (uint64_t)(ir - pg->insns) * 4ULL;
}

CPU_ALWAYS_INLINE uint64_t cpu_get_x(const vcpu_t *cpu, uint32_t reg) {
    return cpu->x[reg];
}

CPU_ALWAYS_INLINE void cpu_set_x(vcpu_t *cpu, uint32_t reg, uint64_t val) {
    if (reg < 31U) {
        cpu->x[reg] = val;
    }
}

CPU_ALWAYS_INLINE void cpu_set_x_fast(vcpu_t *cpu, uint32_t reg, uint64_t val) {
    cpu->x[reg] = val;
}

CPU_ALWAYS_INLINE uint64_t cpu_get_sp(const vcpu_t *cpu) {
    return cpu->sp_el[cpu->el & cpu->spsel];
}

CPU_ALWAYS_INLINE void cpu_set_sp(vcpu_t *cpu, uint64_t val) {
    cpu->sp_el[cpu->el & cpu->spsel] = val;
}

CPU_ALWAYS_INLINE uint64_t cpu_get_xsp(const vcpu_t *cpu, uint32_t reg) {
    return reg < 31U ? cpu->x[reg] : cpu->sp_el[cpu->el & cpu->spsel];
}

CPU_ALWAYS_INLINE void cpu_set_xsp(vcpu_t *cpu, uint32_t reg, uint64_t val) {
    if (reg < 31U) {
        cpu->x[reg] = val;
    } else {
        cpu->sp_el[cpu->el & cpu->spsel] = val;
    }
}

CPU_ALWAYS_INLINE int64_t cpu_sext(uint64_t val, uint32_t bits) {
    uint32_t sh = (64U - bits) & 63U;
    return (int64_t)(val << sh) >> sh;
}

CPU_ALWAYS_INLINE bool cpu_cond_holds(uint32_t nzcv, uint32_t cond) {
    bool res;
    switch (cond >> 1) {
    case 0: res = (nzcv & (1U << 30)) != 0; break;
    case 1: res = (nzcv & (1U << 29)) != 0; break;
    case 2: res = (nzcv & (1U << 31)) != 0; break;
    case 3: res = (nzcv & (1U << 28)) != 0; break;
    case 4: res = (nzcv & (3U << 29)) == (1U << 29); break;
    case 5: res = (((nzcv >> 3) ^ nzcv) & (1U << 28)) == 0; break;
    case 6: res = ((((nzcv >> 3) ^ nzcv) & (1U << 28)) == 0) && ((nzcv & (1U << 30)) == 0); break;
    default: return true;
    }
    return (cond & 1U) ? !res : res;
}

CPU_ALWAYS_INLINE uint64_t cpu_lane_get_u(const uint8_t *v, uint32_t idx, uint32_t ebytes) {
    const uint8_t *p = v + idx * ebytes;
    switch (ebytes) {
    case 1U: return *p;
    case 2U: { uint16_t x; memcpy(&x, p, 2); return x; }
    case 4U: { uint32_t x; memcpy(&x, p, 4); return x; }
    default: { uint64_t x; memcpy(&x, p, 8); return x; }
    }
}

CPU_ALWAYS_INLINE int64_t cpu_lane_get_s(const uint8_t *v, uint32_t idx, uint32_t ebytes) {
    const uint8_t *p = v + idx * ebytes;
    switch (ebytes) {
    case 1U: return (int8_t)*p;
    case 2U: { int16_t x; memcpy(&x, p, 2); return x; }
    case 4U: { int32_t x; memcpy(&x, p, 4); return x; }
    default: { int64_t x; memcpy(&x, p, 8); return x; }
    }
}

CPU_ALWAYS_INLINE void cpu_lane_set(uint8_t *v, uint32_t idx, uint32_t ebytes, uint64_t val) {
    uint8_t *p = v + idx * ebytes;
    switch (ebytes) {
    case 1U: *p = (uint8_t)val; break;
    case 2U: { uint16_t x = (uint16_t)val; memcpy(p, &x, 2); break; }
    case 4U: { uint32_t x = (uint32_t)val; memcpy(p, &x, 4); break; }
    default: memcpy(p, &val, 8); break;
    }
}

CPU_ALWAYS_INLINE uint64_t cpu_lane_replicate(uint64_t val, uint32_t ebytes) {
    switch (ebytes) {
    case 1U: return (uint8_t)val * 0x0101010101010101ULL;
    case 2U: return (uint16_t)val * 0x0001000100010001ULL;
    case 4U: return (uint32_t)val * 0x0000000100000001ULL;
    default: return val;
    }
}

CPU_ALWAYS_INLINE bool cpu_fp_enabled(const vcpu_t *cpu) {
    uint32_t fpen = (uint32_t)((cpu->cpacr_el1 >> 20) & 3U);
    return cpu->el == 0 ? (fpen == 3U) : ((fpen & 1U) != 0);
}

#if defined(__aarch64__)
CPU_ALWAYS_INLINE uint64_t cpu_adds64(uint64_t a, uint64_t b, uint32_t *nzcv_out) {
    uint64_t res, nzcv;
    __asm__("adds %0, %2, %3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_subs64(uint64_t a, uint64_t b, uint32_t *nzcv_out) {
    uint64_t res, nzcv;
    __asm__("subs %0, %2, %3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_adds32(uint32_t a, uint32_t b, uint32_t *nzcv_out) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("adds %w0, %w2, %w3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_subs32(uint32_t a, uint32_t b, uint32_t *nzcv_out) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("subs %w0, %w2, %w3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_adc64(uint64_t a, uint64_t b, uint32_t nzcv_in) {
    uint64_t res;
    __asm__("msr nzcv, %3\n\tadc %0, %1, %2"
            : "=&r"(res)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_adcs64(uint64_t a, uint64_t b, uint32_t nzcv_in, uint32_t *nzcv_out) {
    uint64_t res, nzcv;
    __asm__("msr nzcv, %4\n\tadcs %0, %2, %3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_sbc64(uint64_t a, uint64_t b, uint32_t nzcv_in) {
    uint64_t res;
    __asm__("msr nzcv, %3\n\tsbc %0, %1, %2"
            : "=&r"(res)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_sbcs64(uint64_t a, uint64_t b, uint32_t nzcv_in, uint32_t *nzcv_out) {
    uint64_t res, nzcv;
    __asm__("msr nzcv, %4\n\tsbcs %0, %2, %3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_adc32(uint32_t a, uint32_t b, uint32_t nzcv_in) {
    uint32_t res;
    __asm__("msr nzcv, %3\n\tadc %w0, %w1, %w2"
            : "=&r"(res)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_adcs32(uint32_t a, uint32_t b, uint32_t nzcv_in, uint32_t *nzcv_out) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("msr nzcv, %4\n\tadcs %w0, %w2, %w3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_sbc32(uint32_t a, uint32_t b, uint32_t nzcv_in) {
    uint32_t res;
    __asm__("msr nzcv, %3\n\tsbc %w0, %w1, %w2"
            : "=&r"(res)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_sbcs32(uint32_t a, uint32_t b, uint32_t nzcv_in, uint32_t *nzcv_out) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("msr nzcv, %4\n\tsbcs %w0, %w2, %w3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b), "r"((uint64_t)nzcv_in)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint64_t cpu_ands64(uint64_t a, uint64_t b, uint32_t *nzcv_out) {
    uint64_t res, nzcv;
    __asm__("ands %0, %2, %3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return res;
}

CPU_ALWAYS_INLINE uint64_t cpu_ands32(uint32_t a, uint32_t b, uint32_t *nzcv_out) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("ands %w0, %w2, %w3\n\tmrs %1, nzcv"
            : "=&r"(res), "=r"(nzcv)
            : "r"(a), "r"(b)
            : "cc");
    *nzcv_out = (uint32_t)nzcv;
    return (uint64_t)res;
}

CPU_ALWAYS_INLINE uint32_t cpu_hw_read_fpcr(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, fpcr" : "=r"(v));
    return (uint32_t)v;
}

CPU_ALWAYS_INLINE void cpu_hw_write_fpcr(uint32_t v) {
    __asm__ volatile("msr fpcr, %0" : : "r"((uint64_t)v));
}

CPU_ALWAYS_INLINE uint32_t cpu_hw_read_fpsr(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, fpsr" : "=r"(v));
    return (uint32_t)v;
}

CPU_ALWAYS_INLINE void cpu_hw_write_fpsr(uint32_t v) {
    __asm__ volatile("msr fpsr, %0" : : "r"((uint64_t)v));
}
#else
#error "ARM64 host required"
#endif

CPU_COLD_MISS void cpu_fp_enter_guest_slow(vcpu_t *cpu);
CPU_COLD_MISS void cpu_fp_leave_guest_slow(vcpu_t *cpu);

CPU_ALWAYS_INLINE void cpu_fp_ensure_guest(vcpu_t *cpu) {
    if (__builtin_expect(!cpu->fp_host_active, 0)) {
        cpu_fp_enter_guest_slow(cpu);
    }
}

CPU_ALWAYS_INLINE void cpu_fp_leave_guest(vcpu_t *cpu) {
    if (__builtin_expect(cpu->fp_host_active, 0)) {
        cpu_fp_leave_guest_slow(cpu);
    }
}

typedef struct cpu_read_res {
    uint64_t val;
    bool ok;
} cpu_read_res_t;

CPU_COLD_MISS cpu_read_res_t cpu_read_8_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va);
CPU_COLD_MISS cpu_read_res_t cpu_read_16_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va);
CPU_COLD_MISS cpu_read_res_t cpu_read_32_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va);
CPU_COLD_MISS cpu_read_res_t cpu_read_64_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va);
CPU_COLD_MISS bool cpu_read_128_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, __uint128_t *out);
CPU_COLD_MISS bool cpu_write_8_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint8_t val);
CPU_COLD_MISS bool cpu_write_16_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint16_t val);
CPU_COLD_MISS bool cpu_write_32_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint32_t val);
CPU_COLD_MISS bool cpu_write_64_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint64_t val);
CPU_COLD_MISS bool cpu_write_128_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, __uint128_t val);

CPU_ALWAYS_INLINE bool cpu_read_8(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                  const softtlb_entry_t *tlb_r, uint64_t va, uint8_t *out) {
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_r, cpu->tlb_asid_salt, va, &hva), 1)) {
        *out = __atomic_load_n((const uint8_t *)hva, __ATOMIC_RELAXED);
        return true;
    }
    cpu_read_res_t r = cpu_read_8_miss(cpu, ir, pc_off, va);
    *out = (uint8_t)r.val;
    return r.ok;
}

CPU_ALWAYS_INLINE bool cpu_read_16(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                   const softtlb_entry_t *tlb_r, uint64_t va, uint16_t *out) {
    uintptr_t hva;
    if (__builtin_expect((va & 1ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_r, cpu->tlb_asid_salt, va, &hva), 1)) {
            *out = __atomic_load_n((const uint16_t *)hva, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_r, cpu->tlb_asid_salt, va, 2, &hva)) {
        memcpy(out, (const void *)hva, 2);
        return true;
    }
    cpu_read_res_t r = cpu_read_16_miss(cpu, ir, pc_off, va);
    *out = (uint16_t)r.val;
    return r.ok;
}

CPU_ALWAYS_INLINE bool cpu_read_32(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                   const softtlb_entry_t *tlb_r, uint64_t va, uint32_t *out) {
    uintptr_t hva;
    if (__builtin_expect((va & 3ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_r, cpu->tlb_asid_salt, va, &hva), 1)) {
            *out = __atomic_load_n((const uint32_t *)hva, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_r, cpu->tlb_asid_salt, va, 4, &hva)) {
        memcpy(out, (const void *)hva, 4);
        return true;
    }
    cpu_read_res_t r = cpu_read_32_miss(cpu, ir, pc_off, va);
    *out = (uint32_t)r.val;
    return r.ok;
}

CPU_ALWAYS_INLINE bool cpu_read_64(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                   const softtlb_entry_t *tlb_r, uint64_t va, uint64_t *out) {
    uintptr_t hva;
    if (__builtin_expect((va & 7ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_r, cpu->tlb_asid_salt, va, &hva), 1)) {
            *out = __atomic_load_n((const uint64_t *)hva, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_r, cpu->tlb_asid_salt, va, 8, &hva)) {
        memcpy(out, (const void *)hva, 8);
        return true;
    }
    cpu_read_res_t r = cpu_read_64_miss(cpu, ir, pc_off, va);
    *out = r.val;
    return r.ok;
}

CPU_ALWAYS_INLINE bool cpu_read_pair_64(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                        const softtlb_entry_t *tlb_r, uint64_t va,
                                        uint64_t *out1, uint64_t *out2) {
    uintptr_t hva;
    if (__builtin_expect((va & 7ULL) == 0 && (va & VMM_PAGE_MASK) <= (VMM_PAGE_SIZE - 16U), 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_r, cpu->tlb_asid_salt, va, &hva), 1)) {
            *out1 = __atomic_load_n((const uint64_t *)hva, __ATOMIC_RELAXED);
            *out2 = __atomic_load_n((const uint64_t *)(hva + 8), __ATOMIC_RELAXED);
            return true;
        }
    }
    return cpu_read_64(cpu, ir, pc_off, tlb_r, va, out1) &&
           cpu_read_64(cpu, ir, pc_off, tlb_r, va + 8ULL, out2);
}

CPU_ALWAYS_INLINE bool cpu_read_128(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                    const softtlb_entry_t *tlb_r, uint64_t va, __uint128_t *out) {
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup(tlb_r, cpu->tlb_asid_salt, va, 16, &hva), 1)) {
        memcpy(out, (const void *)hva, 16);
        return true;
    }
    return cpu_read_128_miss(cpu, ir, pc_off, va, out);
}

CPU_ALWAYS_INLINE bool cpu_write_8(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                   const softtlb_entry_t *tlb_w, uint64_t va, uint8_t val) {
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, va, &hva), 1)) {
        __atomic_store_n((uint8_t *)hva, val, __ATOMIC_RELAXED);
        return true;
    }
    return cpu_write_8_miss(cpu, ir, pc_off, va, val);
}

CPU_ALWAYS_INLINE bool cpu_write_16(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                    const softtlb_entry_t *tlb_w, uint64_t va, uint16_t val) {
    uintptr_t hva;
    if (__builtin_expect((va & 1ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, va, &hva), 1)) {
            __atomic_store_n((uint16_t *)hva, val, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, 2, &hva)) {
        memcpy((void *)hva, &val, 2);
        return true;
    }
    return cpu_write_16_miss(cpu, ir, pc_off, va, val);
}

CPU_ALWAYS_INLINE bool cpu_write_32(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                    const softtlb_entry_t *tlb_w, uint64_t va, uint32_t val) {
    uintptr_t hva;
    if (__builtin_expect((va & 3ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, va, &hva), 1)) {
            __atomic_store_n((uint32_t *)hva, val, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, 4, &hva)) {
        memcpy((void *)hva, &val, 4);
        return true;
    }
    return cpu_write_32_miss(cpu, ir, pc_off, va, val);
}

CPU_ALWAYS_INLINE bool cpu_write_64(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                    const softtlb_entry_t *tlb_w, uint64_t va, uint64_t val) {
    uintptr_t hva;
    if (__builtin_expect((va & 7ULL) == 0, 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, va, &hva), 1)) {
            __atomic_store_n((uint64_t *)hva, val, __ATOMIC_RELAXED);
            return true;
        }
    } else if (mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, 8, &hva)) {
        memcpy((void *)hva, &val, 8);
        return true;
    }
    return cpu_write_64_miss(cpu, ir, pc_off, va, val);
}

CPU_ALWAYS_INLINE bool cpu_write_pair_64(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                         const softtlb_entry_t *tlb_w, uint64_t va,
                                         uint64_t val1, uint64_t val2) {
    uintptr_t hva;
    if (__builtin_expect((va & 7ULL) == 0 && (va & VMM_PAGE_MASK) <= (VMM_PAGE_SIZE - 16U), 1)) {
        if (__builtin_expect(mmu_tlb_lookup_aligned(tlb_w, cpu->tlb_asid_salt, va, &hva), 1)) {
            __atomic_store_n((uint64_t *)hva, val1, __ATOMIC_RELAXED);
            __atomic_store_n((uint64_t *)(hva + 8), val2, __ATOMIC_RELAXED);
            return true;
        }
    }
    return cpu_write_64(cpu, ir, pc_off, tlb_w, va, val1) &&
           cpu_write_64(cpu, ir, pc_off, tlb_w, va + 8ULL, val2);
}

CPU_ALWAYS_INLINE bool cpu_write_128(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off,
                                     const softtlb_entry_t *tlb_w, uint64_t va, __uint128_t val) {
    uintptr_t hva;
    if (__builtin_expect(mmu_tlb_lookup(tlb_w, cpu->tlb_asid_salt, va, 16, &hva), 1)) {
        memcpy((void *)hva, &val, 16);
        return true;
    }
    return cpu_write_128_miss(cpu, ir, pc_off, va, val);
}

CPU_ALWAYS_INLINE void vcpu_raise_undef(vcpu_t *cpu, uint64_t elr) {
    vcpu_raise_exception(cpu, 1U << 25, 0, elr);
}

CPU_ALWAYS_INLINE void vcpu_raise_fp_trap(vcpu_t *cpu, uint64_t elr) {
    vcpu_raise_exception(cpu, (0x07U << 26) | (1U << 25) | (0x1EU << 20), 0, elr);
}

CPU_ALWAYS_INLINE void vcpu_raise_sys_trap(vcpu_t *cpu, uint32_t word, uint64_t elr) {
    uint32_t op0 = (word >> 19) & 3U;
    uint32_t op1 = (word >> 16) & 7U;
    uint32_t crn = (word >> 12) & 15U;
    uint32_t crm = (word >> 8) & 15U;
    uint32_t op2 = (word >> 5) & 7U;
    uint32_t rt = word & 31U;
    uint32_t dir = (word >> 21) & 1U;
    uint32_t iss = (op0 << 20) | (op2 << 17) | (op1 << 14) | (crn << 10) |
                   (rt << 5) | (crm << 1) | dir;
    vcpu_raise_exception(cpu, (0x18U << 26) | (1U << 25) | iss, 0, elr);
}

CPU_ALWAYS_INLINE void vcpu_raise_data_abort(vcpu_t *cpu, uint64_t va, uint32_t fsc,
                                             bool is_write, uint64_t elr) {
    uint32_t ec = cpu->el ? 0x25U : 0x24U;
    uint32_t esr = (ec << 26) | (1U << 25) | (is_write ? (1U << 6) : 0U) | (fsc & 0x3FU);
    vcpu_raise_exception(cpu, esr, va, elr);
}

CPU_ALWAYS_INLINE bool cpu_check_sp_align(vcpu_t *cpu, uint32_t rn, uint64_t elr) {
    if (__builtin_expect(rn == 31U, 0)) {
        uint64_t bit = cpu->el == 0 ? (1ULL << 4) : (1ULL << 3);
        if ((cpu->sctlr_el1 & bit) != 0 && (cpu_get_sp(cpu) & 15ULL) != 0) {
            vcpu_raise_exception(cpu, (0x26U << 26) | (1U << 25), 0, elr);
            return false;
        }
    }
    return true;
}

enum : uint16_t {
    SUBOP_NONE = 0,
    SUBOP_CRC32B,
    SUBOP_CRC32H,
    SUBOP_CRC32W,
    SUBOP_CRC32X,
    SUBOP_CRC32CB,
    SUBOP_CRC32CH,
    SUBOP_CRC32CW,
    SUBOP_CRC32CX,
    SUBOP_FP_DP1,
    SUBOP_FP_DP2,
    SUBOP_FP_DP3,
    SUBOP_FP_CMP,
    SUBOP_FP_CCMP,
    SUBOP_FP_SEL,
    SUBOP_FP_IMM,
    SUBOP_FP_2INT,
    SUBOP_FP_2FIX,
    SUBOP_CRYPTO_AES,
    SUBOP_CRYPTO_SHA2,
    SUBOP_CRYPTO_SHA3,
    SUBOP_CRYPTO_IMM6,
    SUBOP_CRYPTO_4,
    SUBOP_CRYPTO_SHA512_2,
    SUBOP_CRYPTO_SHA512_3,
    SUBOP_SIMD_IMM,
    SUBOP_SIMD_INS,
    SUBOP_SIMD_ONE,
    SUBOP_SIMD_EXT,
    SUBOP_SIMD_PERM,
    SUBOP_SIMD_TBL,
    SUBOP_SIMD_SAME,
    SUBOP_SIMD_SAME_FP16,
    SUBOP_SIMD_SAME2,
    SUBOP_SIMD_MISC,
    SUBOP_SIMD_MISC_FP16,
    SUBOP_SIMD_SHF,
    SUBOP_SIMD_DIFF,
    SUBOP_SIMD_ELEM,
    SUBOP_SIMD_ALL,
    SUBOP_SISD_SAME,
    SUBOP_SISD_SAME_FP16,
    SUBOP_SISD_SAME2,
    SUBOP_SISD_MISC,
    SUBOP_SISD_MISC_FP16,
    SUBOP_SISD_SHF,
    SUBOP_SISD_DIFF,
    SUBOP_SISD_ELEM,
    SUBOP_SISD_PAIR,
};

bool vcpu_decode_bitmask(uint32_t n, uint32_t imms, uint32_t immr, bool is_64,
                         bool immediate, uint64_t *wmask_out, uint64_t *tmask_out);

enum : uint32_t {
    CPU_HOST_SHA3 = 1U << 0,
    CPU_HOST_SHA512 = 1U << 1,
};

uint32_t cpu_host_features(void);
bool vcpu_check_interrupts(vcpu_t *cpu);
bool vmm_handle_psci(vcpu_t *cpu);
void vmm_vcpu_wfe(vcpu_t *cpu);
void vmm_vcpu_sev(vmm_machine_t *vm);
void vmm_vcpu_wfe_notify_write(vmm_machine_t *vm, uintptr_t hva);

void cpu_op_ldst_unpriv(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_ldst_ord(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_ldst_exclr(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_ldst_exclp(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_lse_cas(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_lse_casp(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_lse_memop(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_mops_cpy(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_mops_set(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_simd_ldst_multi(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_simd_ldst_single(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);

void cpu_op_crc32(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_fp_scalar(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_simd_dp(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_crypto(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);

void cpu_op_sysreg_mrs(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sysreg_msr(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_pstate_msr(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sys_insn(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_svc(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_hvc(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_brk(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_hlt(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_eret(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_wfi(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_wfe(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sev(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sevl(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_barrier(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_clrex(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);

#endif
