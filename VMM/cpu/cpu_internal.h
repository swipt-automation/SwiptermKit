#ifndef VMM_CPU_CPU_INTERNAL_H
#define VMM_CPU_CPU_INTERNAL_H

#include "cpu.h"
#include "../board/machine.h"
#include "../mmu/mmu.h"
#include "cpu_fast_layout.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CPU_ALWAYS_INLINE static inline __attribute__((always_inline))
#define CPU_COLD_MISS __attribute__((cold, noinline, preserve_most))

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
        if (_cpu_next->fast_offset != 0U) {                                    \
            (cpu)->fast_next = _cpu_next;                                      \
            return;                                                            \
        }                                                                      \
        CPU_STATS_COLD((cpu), _cpu_next);                                      \
        [[clang::musttail]] return _cpu_next->handler((cpu), _cpu_next,        \
                                                      (tlb_r), (tlb_w));       \
    } while (0)

enum {
    IR_PAGE_INSNS = 1024,
    EXEC_CACHE_ENTRIES = 16384,
    EXEC_CACHE_MASK = EXEC_CACHE_ENTRIES - 1,
    RAS_SIZE = 16,
    RAS_MASK = RAS_SIZE - 1,
    BACKEDGE_BUDGET_INIT = 1024,
    JIT_HEAT = 32,
    JIT_REHEAT = 255,
};

enum : uint8_t {
    IR_FLAG_NONE = 0,
    IR_FLAG_INPAGE = 1U << 0,
    IR_FLAG_WIDE = 1U << 1,
    IR_FLAG_SIMD = 1U << 2,
    IR_FLAG_SETFLAGS = 1U << 3,
    IR_FLAG_SUB = 1U << 4,
    IR_FLAG_INV = 1U << 5,
    IR_FLAG_POST = 1U << 6,
    IR_FLAG_PRE = 1U << 7,
};

enum : uint32_t {
    CPU_FPSR_QC = 1U << 27,
};

enum : uint64_t {
    HCR_EL2_VM = 1ULL << 0,
    HCR_EL2_PTW = 1ULL << 2,
    HCR_EL2_FMO = 1ULL << 3,
    HCR_EL2_IMO = 1ULL << 4,
    HCR_EL2_AMO = 1ULL << 5,
    HCR_EL2_VF = 1ULL << 6,
    HCR_EL2_VI = 1ULL << 7,
    HCR_EL2_VSE = 1ULL << 8,
    HCR_EL2_FB = 1ULL << 9,
    HCR_EL2_TWI = 1ULL << 13,
    HCR_EL2_TWE = 1ULL << 14,
    HCR_EL2_TID1 = 1ULL << 16,
    HCR_EL2_TID2 = 1ULL << 17,
    HCR_EL2_TID3 = 1ULL << 18,
    HCR_EL2_TSC = 1ULL << 19,
    HCR_EL2_TIDCP = 1ULL << 20,
    HCR_EL2_TACR = 1ULL << 21,
    HCR_EL2_TSW = 1ULL << 22,
    HCR_EL2_TPCP = 1ULL << 23,
    HCR_EL2_TPU = 1ULL << 24,
    HCR_EL2_TTLB = 1ULL << 25,
    HCR_EL2_TVM = 1ULL << 26,
    HCR_EL2_TGE = 1ULL << 27,
    HCR_EL2_TDZ = 1ULL << 28,
    HCR_EL2_HCD = 1ULL << 29,
    HCR_EL2_TRVM = 1ULL << 30,
    HCR_EL2_RW = 1ULL << 31,
    HCR_EL2_E2H = 1ULL << 34,
    HCR_EL2_FWB = 1ULL << 46,
    MDCR_EL2_TPMCR = 1ULL << 5,
    MDCR_EL2_TPM = 1ULL << 6,
    MDCR_EL2_TDE = 1ULL << 8,
    MDCR_EL2_TDA = 1ULL << 9,
    MDCR_EL2_TDOSA = 1ULL << 10,
    VTCR_EL2_VS = 1ULL << 19,
    VTCR_EL2_HA = 1ULL << 21,
    VTCR_EL2_HD = 1ULL << 22,
    CPTR_EL2_TFP = 1ULL << 10,
    SCTLR_EL2_RESET = 0x30C50830ULL,
    CPTR_EL2_RESET = 0x33FFULL,
    CNTHCTL_EL2_MASK = 0x3EFFFULL,
};

enum : uint32_t {
    CPU_TRAP_TVM = 1U << 0,
    CPU_TRAP_TRVM = 1U << 1,
    CPU_TRAP_TACR = 1U << 2,
    CPU_TRAP_TID1 = 1U << 3,
    CPU_TRAP_TID2 = 1U << 4,
    CPU_TRAP_TID3 = 1U << 5,
    CPU_TRAP_TSW = 1U << 6,
    CPU_TRAP_TPCP = 1U << 7,
    CPU_TRAP_TPU = 1U << 8,
    CPU_TRAP_TIDCP = 1U << 9,
    CPU_TRAP_TDZ = 1U << 10,
    CPU_TRAP_TWI = 1U << 11,
    CPU_TRAP_TWE = 1U << 12,
    CPU_TRAP_TTLB = 1U << 13,
    CPU_TRAP_TDA = 1U << 14,
    CPU_TRAP_TDOSA = 1U << 15,
    CPU_TRAP_TPM = 1U << 16,
    CPU_TRAP_CNTPCT = 1U << 17,
    CPU_TRAP_CNTP = 1U << 18,
    CPU_TRAP_CNTV = 1U << 19,
    CPU_TRAP_CNTVCT = 1U << 20,
};

enum : uint32_t {
    POOL_SLOTS = 6144,
    POOL_BUCKETS = 4096,
    POOL_SLOT_SHIFT = 16,
    POOL_SLOT_BYTES = 1U << POOL_SLOT_SHIFT,
    POOL_EVICT_BATCH = 64,
    POOL_NONE = UINT32_MAX,
};

typedef struct ir_page {
    _Atomic uint32_t gen;
    _Atomic uint8_t ref;
    uint8_t el;
    uint8_t world;
    uint32_t traps;
    uint32_t next;
    uint64_t pa_page;
    uintptr_t hva_page;
    uint64_t hash;
    uint32_t pfn_gen;
    uint32_t code_gen;
    alignas(64) ir_insn_t insns[IR_PAGE_INSNS + 1];
    uint32_t words[IR_PAGE_INSNS];
} ir_page_t;

static_assert(sizeof(ir_page_t) <= POOL_SLOT_BYTES);

typedef struct exec_cache_entry {
    uint64_t tag;
    ir_page_t *pg;
    uint64_t exec_epoch;
    uint32_t pg_gen;
    uint32_t pad;
} exec_cache_entry_t;

static_assert(sizeof(exec_cache_entry_t) == 32);

struct vcpu_mmu_ctx {
    softtlb_entry_t tlb[SOFTTLB_BANK_COUNT][SOFTTLB_ENTRIES];
    exec_cache_entry_t exec_cache[2][EXEC_CACHE_ENTRIES];
};

typedef struct ras_entry {
    uint64_t pc;
    const ir_insn_t *ir;
    uint64_t tag;
    uint32_t gen;
    uint32_t pad;
} ras_entry_t;

static_assert(sizeof(ras_entry_t) == 32);

typedef struct cpu_jit_arena {
    uint8_t *rx;
    uint8_t *rw;
    uint32_t size;
    uint32_t base_offset;
} cpu_jit_arena_t;

typedef enum cpu_jit_block_state : uint8_t {
    JIT_BLOCK_FREE,
    JIT_BLOCK_PENDING,
    JIT_BLOCK_LIVE,
    JIT_BLOCK_DEAD,
} cpu_jit_block_state_t;

typedef struct cpu_jit_block {
    uint64_t pa_page;
    uint64_t hash;
    uint64_t epoch;
    uint32_t traps;
    uint32_t pfn_gen;
    uint32_t code_gen;
    uint32_t offset;
    uint32_t orig;
    uint32_t next;
    uint32_t seg_next;
    uint32_t pend_next;
    uint16_t index;
    uint16_t seg;
    uint8_t el;
    uint8_t world;
    cpu_jit_block_state_t state;
} cpu_jit_block_t;

typedef struct cpu_jit_patch {
    uint64_t pa_page;
    uint32_t traps;
    uint32_t pfn_gen;
    uint32_t code_gen;
    uint32_t site;
    uint32_t next;
    uint32_t seg_next;
    uint16_t index;
    uint16_t seg;
    uint8_t el;
    uint8_t world;
    bool hashed;
} cpu_jit_patch_t;

typedef enum cpu_jit_segment_state : uint8_t {
    JIT_SEG_FREE,
    JIT_SEG_OPEN,
    JIT_SEG_FULL,
    JIT_SEG_RETIRED,
} cpu_jit_segment_state_t;

typedef struct cpu_jit_segment {
    uint64_t seq;
    uint64_t retire;
    uint32_t blocks;
    uint32_t patches;
    uint32_t count;
    uint32_t gen;
    uint32_t holders;
    cpu_jit_segment_state_t state;
} cpu_jit_segment_t;

enum : uint32_t {
    JIT_BUCKETS = 4096,
    JIT_BLOCKS = 65536,
    JIT_PATCHES = 65536,
    JIT_SEGMENT_BYTES = 1U << 20,
    JIT_CHUNK_BYTES = 128U << 10,
    JIT_RETIRE_AHEAD = 2,
    JIT_BLOCK_RESERVE = 4096,
};

CPU_ALWAYS_INLINE uint32_t cpu_pa_bucket(uint64_t pa_page, uint32_t buckets) {
    return (uint32_t)(((pa_page >> VMM_PAGE_SHIFT) * 0x9E3779B97F4A7C15ULL) >> 40) & (buckets - 1U);
}

typedef struct cpu_jit_pool {
    os_unfair_lock toggle;
    os_unfair_lock lock;
    _Atomic bool active;
    alignas(128) _Atomic uint64_t pend_epoch;
    cpu_jit_arena_t arena;
    uint32_t segments;
    uint32_t open;
    uint32_t carve;
    uint32_t gen;
    uint64_t seq;
    cpu_jit_segment_t *seg;
    cpu_jit_block_t *blocks;
    uint32_t block_free;
    uint32_t block_avail;
    uint32_t pend_head;
    uint32_t pend_tail;
    cpu_jit_patch_t *patches;
    uint32_t patch_free;
    uint32_t *heads;
    uint32_t *patch_heads;
} cpu_jit_pool_t;

typedef struct cpu_qs {
    alignas(128) _Atomic uint64_t qs;
    _Atomic bool parked;
} cpu_qs_t;

typedef struct cpu_pool {
    alignas(128) _Atomic uint64_t epoch;
    alignas(128) os_unfair_lock lock;
    uint32_t free_head;
    uint32_t clock;
    uint32_t retire_head;
    uint32_t retire_tail;
    alignas(128) _Atomic uint32_t code_gen;
    uint8_t *base;
    uint8_t *map;
    size_t map_bytes;
    vmm_machine_t *vm;
    _Atomic uint32_t *pfn_gen;
    uint64_t pfn_count;
    uint32_t slot_gen[POOL_SLOTS];
    uint32_t slot_link[POOL_SLOTS];
    uint64_t slot_retire[POOL_SLOTS];
    bool slot_live[POOL_SLOTS];
    bool slot_reusable[POOL_SLOTS];
    uint32_t heads[POOL_BUCKETS];
    cpu_qs_t qs[VMM_MAX_VCPUS];
    cpu_jit_pool_t jit;
} cpu_pool_t;

extern _Atomic uint32_t cpu_jit_generation;

void cpu_fast_run(vcpu_t *cpu);
uintptr_t cpu_fast_base(void);
void cpu_jit_reserve(void);
bool cpu_jit_arena_acquire(vmm_machine_t *vm, cpu_jit_arena_t *out);
void cpu_jit_arena_release(vmm_machine_t *vm);
void cpu_jit_arena_write(const cpu_jit_arena_t *arena, uint32_t offset, const uint32_t *code, uint32_t words);
void cpu_jit_arena_patch(const cpu_jit_arena_t *arena, uint32_t offset, uint32_t word);

typedef struct cpu_icache {
    ir_page_t *cur_page;
    uint64_t cur_exec_epoch;
    uint64_t cur_va;
    uint64_t cur_gen;
    uint32_t ras_sp;
    uint32_t host_fpcr;
    uint32_t host_fpsr;
    uint32_t jit_generation;
#ifdef CPU_STATS
    uint64_t stat_dispatches;
    struct cpu_stats *stats;
#endif
    alignas(64) ras_entry_t ras[RAS_SIZE];
    cpu_pool_t *pool;
    void *jit_work;
    uint64_t qs_ticks;
} cpu_icache_t;

CPU_ALWAYS_INLINE cpu_pool_t *cpu_pool(const vcpu_t *cpu) {
    return ((const cpu_icache_t *)cpu->icache)->pool;
}

CPU_ALWAYS_INLINE bool cpu_jit_stale(const vcpu_t *cpu) {
    return atomic_load_explicit(&cpu_jit_generation, memory_order_relaxed) !=
           ((const cpu_icache_t *)cpu->icache)->jit_generation;
}

CPU_ALWAYS_INLINE bool cpu_page_live(const vcpu_t *cpu) {
    const cpu_icache_t *ic = (const cpu_icache_t *)cpu->icache;
    return ic->cur_page != NULL &&
           atomic_load_explicit(&ic->cur_page->gen, memory_order_seq_cst) == (uint32_t)ic->cur_gen;
}

CPU_ALWAYS_INLINE uint64_t cpu_ir_pc(const vcpu_t *cpu, const ir_insn_t *ir) {
    const cpu_icache_t *ic = (const cpu_icache_t *)cpu->icache;
    return ic->cur_va + (uint64_t)(ir - ic->cur_page->insns) * 4ULL;
}

CPU_ALWAYS_INLINE void cpu_ras_rekey(vcpu_t *cpu) {
    uint32_t key = (uint32_t)cpu->el | ((uint32_t)cpu->world << 1) | ((uint32_t)cpu->ctx_slot << 3) |
                   ((uint32_t)(cpu->tlb_asid_salt >> 48) << 16);
    cpu->ras_epoch = (uint32_t)cpu->ras_epoch | ((uint64_t)key << 32);
}

void cpu_pool_attach(vcpu_t *cpu);
bool cpu_qs_announce(vcpu_t *cpu);
void cpu_qs_park(vcpu_t *cpu);
bool cpu_qs_unpark(vcpu_t *cpu);
bool cpu_qs_parked(const cpu_pool_t *pool, uint32_t cpu_id);
uint64_t cpu_qs_retire_epoch(cpu_pool_t *pool);
uint64_t cpu_qs_quiescent(cpu_pool_t *pool, const vcpu_t *self);
void cpu_jit_publish(cpu_pool_t *pool);
void cpu_qs_wait(cpu_pool_t *pool, vcpu_t *self, uint64_t target);
ir_page_t *cpu_pool_find(cpu_pool_t *pool, uint64_t pa_page, uint8_t el, uint8_t world, uint32_t traps,
                         uint32_t *gen_out);
ir_page_t *cpu_pool_alloc(cpu_pool_t *pool, vcpu_t *self);
void cpu_pool_trim(cpu_pool_t *pool, const vcpu_t *self);
void cpu_pool_publish(cpu_pool_t *pool, ir_page_t *pg);
_Atomic uint32_t *cpu_pool_pfn_gen(cpu_pool_t *pool, uint64_t pa_page);
uint64_t cpu_pool_retire_pa(cpu_pool_t *pool, uint64_t pa_page);
uint64_t cpu_pool_retire_all(cpu_pool_t *pool);
ir_page_t *cpu_pool_live(cpu_pool_t *pool, uint64_t pa_page, uint8_t el, uint8_t world, uint32_t traps);

CPU_ALWAYS_INLINE uint64_t cpu_get_x(const vcpu_t *cpu, uint32_t reg) {
    return cpu->x[reg];
}

CPU_ALWAYS_INLINE void cpu_set_x(vcpu_t *cpu, uint32_t reg, uint64_t val) {
    if (reg < 31U) {
        cpu->x[reg] = val;
    }
}

CPU_ALWAYS_INLINE uint64_t cpu_get_sp(const vcpu_t *cpu) {
    return cpu->sp_el[cpu->el & cpu->spsel];
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

CPU_ALWAYS_INLINE bool cpu_fp_el1_enabled(const vcpu_t *cpu) {
    if (cpu->world == VCPU_WORLD_HOST_NVHE) {
        return (cpu->cpacr_el1 & CPTR_EL2_TFP) == 0;
    }
    uint32_t fpen = (uint32_t)((cpu->cpacr_el1 >> 20) & 3U);
    return cpu->el == 0 ? (fpen == 3U) : ((fpen & 1U) != 0);
}

CPU_ALWAYS_INLINE bool cpu_fp_el2_enabled(const vcpu_t *cpu) {
    if (cpu->world != VCPU_WORLD_GUEST || !cpu->has_el2) {
        return true;
    }
    uint64_t cptr = cpu->el2.shadow.cpacr_el1;
    return (cpu->el2.hcr & HCR_EL2_E2H) != 0 ? ((cptr >> 20) & 1U) != 0 : (cptr & CPTR_EL2_TFP) == 0;
}

CPU_ALWAYS_INLINE bool cpu_fp_enabled(const vcpu_t *cpu) {
    return cpu_fp_el1_enabled(cpu) && cpu_fp_el2_enabled(cpu);
}

CPU_ALWAYS_INLINE uint32_t cpu_current_el(const vcpu_t *cpu) {
    return cpu->el != 0 && cpu->world != VCPU_WORLD_GUEST ? 2U : cpu->el;
}

CPU_ALWAYS_INLINE uint16_t cpu_vmid(const vcpu_t *cpu) {
    uint64_t vmid = cpu->el2.vttbr >> 48;
    return (uint16_t)((cpu->el2.vtcr & VTCR_EL2_VS) != 0 ? vmid : (vmid & 0xFFU));
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
CPU_COLD_MISS bool cpu_write_8_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint8_t val);
CPU_COLD_MISS bool cpu_write_16_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint16_t val);
CPU_COLD_MISS bool cpu_write_32_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint32_t val);
CPU_COLD_MISS bool cpu_write_64_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint64_t val);

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

void vcpu_init(vcpu_t *cpu, vmm_machine_t *vm, uint32_t cpu_id);
void vcpu_destroy(vcpu_t *cpu);
void vcpu_enable_el2(vcpu_t *cpu);
void vcpu_reset(vcpu_t *cpu, uint64_t entry_pc, uint64_t x0);
void vcpu_set_world(vcpu_t *cpu, uint8_t world);
void vcpu_sync_el2(vcpu_t *cpu);
void vcpu_update_irq_gate(vcpu_t *cpu);
vcpu_stop_reason_t vcpu_run(vcpu_t *cpu);
void vcpu_raise_exception(vcpu_t *cpu, uint32_t esr, uint64_t far, uint64_t elr);
void vcpu_raise_exception_el2(vcpu_t *cpu, uint32_t esr, uint64_t far, uint64_t elr);
void vcpu_eret(vcpu_t *cpu);
void vcpu_flush_icache_all(vcpu_t *cpu);
void vcpu_flush_icache_va(vcpu_t *cpu, uint64_t va);
void cpu_pool_destroy(vmm_machine_t *vm);

CPU_ALWAYS_INLINE uint32_t vcpu_irq_deliverable(const vcpu_t *cpu) {
    uint32_t pending = atomic_load_explicit(&cpu->irq_pending, memory_order_acquire);
    return pending & ((cpu->daif & (1U << 7)) ? cpu->irq_force : cpu->irq_mask);
}

CPU_ALWAYS_INLINE void vcpu_raise_fp_trap(vcpu_t *cpu, uint64_t elr) {
    uint32_t esr = (0x07U << 26) | (1U << 25) | (0x1EU << 20);
    if (cpu_fp_el1_enabled(cpu)) {
        vcpu_raise_exception_el2(cpu, esr, 0, elr);
    } else {
        vcpu_raise_exception(cpu, esr, 0, elr);
    }
}

void vcpu_raise_abort(vcpu_t *cpu, uint64_t va, const mmu_translation_t *tr, bool is_write, uint64_t elr,
                      bool fetch);

uint32_t cpu_guest_traps(const vcpu_t *cpu);
bool cpu_sysreg_trapped(uint32_t traps, uint32_t word);
bool cpu_sysop_trapped(uint32_t traps, uint32_t word);

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

#define CPU_HOST_FEATURES(X)                                                                   \
    X(AES) X(BF16) X(CSSC) X(DotProd) X(FCMA) X(FHM) X(FlagM) X(FlagM2) X(FP16) X(FRINTTS) X(I8MM) \
    X(JSCVT) X(LRCPC) X(LSE) X(LSE2) X(RDM) X(SHA1) X(SHA256) X(SHA3) X(SHA512)

enum : uint32_t {
#define CPU_HOST_FEATURE_INDEX(name) CPU_HOST_INDEX_##name,
    CPU_HOST_FEATURES(CPU_HOST_FEATURE_INDEX)
#undef CPU_HOST_FEATURE_INDEX
};

enum : uint32_t {
#define CPU_HOST_FEATURE_BIT(name) CPU_HOST_FEAT_##name = 1U << CPU_HOST_INDEX_##name,
    CPU_HOST_FEATURES(CPU_HOST_FEATURE_BIT)
#undef CPU_HOST_FEATURE_BIT
};

uint32_t cpu_host_features(void);
bool vcpu_check_interrupts(vcpu_t *cpu);
void vmm_handle_psci(vcpu_t *cpu);
void vmm_vcpu_wfe(vcpu_t *cpu);
void vmm_vcpu_sev(vmm_machine_t *vm);
void vmm_vcpu_wfe_notify_write(vmm_machine_t *vm, uintptr_t hva);

void cpu_op_ldst_unpriv(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_ldst_ord(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_ldapstl_unscaled(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
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

void cpu_op_sysreg_mrs(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sysreg_msr(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_pstate_msr(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_sys_insn(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_svc(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_hvc(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
void cpu_op_smc(vcpu_t *cpu, const ir_insn_t *ir, softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w);
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
