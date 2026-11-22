#if !defined(__aarch64__)
#error "SwiptermKit requires an arm64 host"
#endif

#include "cpu_internal.h"
#include "generated/a64_decode.inc"
#include "generated/a64_sysreg_gen.inc"

#include <stdlib.h>
#include <sys/sysctl.h>

#ifdef CPU_STATS
#include <stdio.h>

enum : uint32_t {
    CPU_STATS_WORDS = 1U << 17,
    CPU_STATS_PROBES = 32,
};

typedef struct cpu_stats {
    uint64_t cold;
    uint64_t walks;
    uint64_t resolves;
    uint64_t resolve_misses;
    uint64_t pages_replaced;
    uint64_t dropped;
    struct {
        uint32_t word;
        uint32_t used;
        uint64_t count;
    } words[CPU_STATS_WORDS];
} cpu_stats_t;

static _Atomic uint64_t cpu_stats_tiles;

static cpu_stats_t *cpu_stats(vcpu_t *cpu) {
    return ((cpu_icache_t *)cpu->icache)->stats;
}

void cpu_stats_walk(vcpu_t *cpu) {
    cpu_stats(cpu)->walks++;
}

void cpu_stats_cold(vcpu_t *cpu, const ir_insn_t *ir) {
    cpu_stats_t *stats = cpu_stats(cpu);
    const ir_page_t *pg = ((const cpu_icache_t *)cpu->icache)->cur_page;
    size_t index = (size_t)(ir - pg->insns);
    stats->cold++;
    if (index >= IR_PAGE_INSNS) {
        return;
    }
    uint32_t word = __atomic_load_n((const uint32_t *)pg->hva_page + index, __ATOMIC_RELAXED);
    uint32_t slot = (word * 0x9E3779B1U) >> 15;
    for (uint32_t probe = 0; probe < CPU_STATS_PROBES; ++probe, slot = (slot + 1U) & (CPU_STATS_WORDS - 1U)) {
        if (!stats->words[slot].used) {
            stats->words[slot].used = 1;
            stats->words[slot].word = word;
        }
        if (stats->words[slot].word == word) {
            stats->words[slot].count++;
            return;
        }
    }
    stats->dropped++;
}

static void cpu_stats_dump(vcpu_t *cpu) {
    const cpu_icache_t *ic = (const cpu_icache_t *)cpu->icache;
    const cpu_stats_t *stats = ic->stats;
    const char *path = getenv("SWIPTERM_CPU_STATS");
    FILE *out = path ? fopen(path, "a") : nullptr;
    if (!out) {
        return;
    }
    fprintf(out, "dispatches %llu\ncold %llu\nwalks %llu\nresolves %llu\nresolve_misses %llu\n"
                 "pages_replaced %llu\ndropped %llu\ntiles %llu\n",
            ic->stat_dispatches, stats->cold, stats->walks, stats->resolves, stats->resolve_misses,
            stats->pages_replaced, stats->dropped, atomic_exchange(&cpu_stats_tiles, 0));
    for (uint32_t i = 0; i < CPU_STATS_WORDS; ++i) {
        if (stats->words[i].used) {
            fprintf(out, "word %08x %llu\n", stats->words[i].word, stats->words[i].count);
        }
    }
    fclose(out);
}

#define CPU_STAT(cpu, field) (cpu_stats(cpu)->field++)
#else
#define CPU_STAT(cpu, field) ((void)0)
#endif

CPU_COLD_MISS void cpu_fp_enter_guest_slow(vcpu_t *cpu) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    ic->host_fpcr = cpu_hw_read_fpcr();
    ic->host_fpsr = cpu_hw_read_fpsr();
    cpu_hw_write_fpcr(cpu->fpcr & CPU_FPCR_HW_MASK);
    cpu_hw_write_fpsr(cpu->fpsr & CPU_FPSR_MASK);
    cpu->fp_host_active = true;
}

CPU_COLD_MISS void cpu_fp_leave_guest_slow(vcpu_t *cpu) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    cpu->fpsr = (cpu->fpsr | cpu_hw_read_fpsr()) & CPU_FPSR_MASK;
    cpu_hw_write_fpcr(ic->host_fpcr);
    cpu_hw_write_fpsr(ic->host_fpsr);
    cpu->fp_host_active = false;
}

CPU_COLD_MISS cpu_read_res_t cpu_read_8_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va) {
    uint8_t v = 0;
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    bool ok = mmu_read_8_slow(cpu, va, &v);
    return (cpu_read_res_t){v, ok};
}

CPU_COLD_MISS cpu_read_res_t cpu_read_16_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va) {
    uint16_t v = 0;
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    bool ok = mmu_read_16_slow(cpu, va, &v);
    return (cpu_read_res_t){v, ok};
}

CPU_COLD_MISS cpu_read_res_t cpu_read_32_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va) {
    uint32_t v = 0;
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    bool ok = mmu_read_32_slow(cpu, va, &v);
    return (cpu_read_res_t){v, ok};
}

CPU_COLD_MISS cpu_read_res_t cpu_read_64_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va) {
    uint64_t v = 0;
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    bool ok = mmu_read_64_slow(cpu, va, &v);
    return (cpu_read_res_t){v, ok};
}

CPU_COLD_MISS bool cpu_read_128_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, __uint128_t *out) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_read_128_slow(cpu, va, out);
}

CPU_COLD_MISS bool cpu_write_8_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint8_t val) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_write_8_slow(cpu, va, val);
}

CPU_COLD_MISS bool cpu_write_16_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint16_t val) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_write_16_slow(cpu, va, val);
}

CPU_COLD_MISS bool cpu_write_32_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint32_t val) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_write_32_slow(cpu, va, val);
}

CPU_COLD_MISS bool cpu_write_64_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, uint64_t val) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_write_64_slow(cpu, va, val);
}

CPU_COLD_MISS bool cpu_write_128_miss(vcpu_t *cpu, const ir_insn_t *ir, uint32_t pc_off, uint64_t va, __uint128_t val) {
    cpu->pc = cpu_ir_pc(cpu, ir) + pc_off;
    cpu_fp_leave_guest(cpu);
    return mmu_write_128_slow(cpu, va, val);
}

CPU_COLD_MISS static bool cpu_backedge_miss(vcpu_t *cpu, const ir_insn_t *target) {
    cpu->backedge_budget = BACKEDGE_BUDGET_INIT;
    if (vcpu_check_interrupts(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, target);
        return true;
    }
    return false;
}

CPU_COLD_MISS static bool cpu_backedge_miss_pc(vcpu_t *cpu, uint64_t target_pc) {
    cpu->backedge_budget = BACKEDGE_BUDGET_INIT;
    if (vcpu_check_interrupts(cpu)) {
        cpu->pc = target_pc;
        return true;
    }
    return false;
}

CPU_COLD_MISS static void decode_tile_slow(ir_page_t *pg, uint32_t tile);

static CPU_NOINLINE void op_tile_miss(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    ir_page_t *pg = ((cpu_icache_t *)cpu->icache)->cur_page;
    uint32_t tile = (uint32_t)(ir - pg->insns) >> TILE_SHIFT;
    decode_tile_slow(pg, tile);
    CPU_NEXT(cpu, ir, tlb_r, tlb_w);
}

CPU_ALWAYS_INLINE const ir_insn_t *cpu_resolve_jump_pc_nobudget(vcpu_t *cpu, uint64_t target) {
    if (__builtin_expect((target & 3ULL) != 0, 0)) {
        cpu->pc = target;
        return NULL;
    }
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    uint64_t va_page = target & ~(uint64_t)VMM_PAGE_MASK;
    ir_page_t *pg = ic->cur_page;
    if (__builtin_expect(pg->va_page == va_page && ic->cur_exec_epoch == cpu->exec_epoch, 0)) {
        uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
        uint32_t tile = tidx >> TILE_SHIFT;
        if (__builtin_expect((pg->decoded_mask & (1U << tile)) == 0, 0)) {
            decode_tile_slow(pg, tile);
        }
        return &pg->insns[tidx];
    }
    uint64_t asid_salt = cpu->tlb_asid_salt;
    uint64_t top = target >> 48;
    uint64_t mmu_off = (asid_salt & (1ULL << 10)) >> 8;
    uint64_t page = target & 0x0000fffffffff000ULL;
    uint32_t idx;
    uint64_t tag;
    if (__builtin_expect(top == 0ULL, 1)) {
        idx = (uint32_t)(((target >> VMM_PAGE_SHIFT) ^ (uint32_t)asid_salt) & EXEC_CACHE_MASK);
        tag = page | (asid_salt & 0xffff000000000000ULL) | mmu_off;
    } else if (__builtin_expect(top == 0xffffULL, 1)) {
        idx = (uint32_t)((target >> VMM_PAGE_SHIFT) & EXEC_CACHE_MASK);
        tag = page | 0x2ULL | mmu_off;
    } else {
        cpu->pc = target;
        return NULL;
    }
    const exec_cache_entry_t *e = &ic->exec_cache[cpu->el][idx];
    if (__builtin_expect(e->tag == tag && e->exec_epoch == cpu->exec_epoch &&
                         e->pg != NULL && e->pg->gen == e->pg_gen, 1)) {
        ir_page_t *npg = e->pg;
        uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
        uint32_t tile = tidx >> TILE_SHIFT;
        if (__builtin_expect((npg->decoded_mask & (1U << tile)) == 0, 0)) {
            decode_tile_slow(npg, tile);
        }
        ic->cur_page = npg;
        ic->cur_exec_epoch = cpu->exec_epoch;
        return &npg->insns[tidx];
    }
    cpu->pc = target;
    return NULL;
}

#define CPU_JUMP_PC_NOBUDGET(cpu, target, tlb_r, tlb_w)                        \
    do {                                                                       \
        const ir_insn_t *_cpu_jnext =                                          \
            cpu_resolve_jump_pc_nobudget((cpu), (target));                     \
        if (__builtin_expect(_cpu_jnext != NULL, 1)) {                         \
            CPU_NEXT((cpu), _cpu_jnext, (tlb_r), (tlb_w));                     \
        }                                                                      \
        return;                                                                \
    } while (0)

#define CPU_JUMP_PC(cpu, target, tlb_r, tlb_w)                                 \
    do {                                                                       \
        uint64_t _cpu_jtarget = (target);                                      \
        if (__builtin_expect(--(cpu)->backedge_budget <= 0, 0) &&              \
            cpu_backedge_miss_pc((cpu), _cpu_jtarget)) {                       \
            return;                                                            \
        }                                                                      \
        CPU_JUMP_PC_NOBUDGET((cpu), _cpu_jtarget, (tlb_r), (tlb_w));           \
    } while (0)

static inline uint8_t map_rd_zr(uint32_t rd) {
    return (uint8_t)(rd == 31U ? 32U : rd);
}

bool vcpu_decode_bitmask(uint32_t n, uint32_t imms, uint32_t immr, bool is_64,
                         bool immediate, uint64_t *wmask_out, uint64_t *tmask_out) {
    uint32_t combined = (n << 6) | ((~imms) & 0x3FU);
    if (combined == 0U) {
        return false;
    }
    uint32_t len = 31U - (uint32_t)__builtin_clz(combined);
    if (len < 1U || (!is_64 && len >= 6U)) {
        return false;
    }
    uint32_t levels = (1U << len) - 1U;
    uint32_t s = imms & levels;
    uint32_t r = immr & levels;
    if (immediate && s == levels) {
        return false;
    }
    uint32_t esize = 1U << len;
    uint32_t diff = (s - r) & levels;
    uint64_t welem = (s == 63U) ? UINT64_MAX : ((1ULL << (s + 1U)) - 1ULL);
    uint64_t telem = (diff == 63U) ? UINT64_MAX : ((1ULL << (diff + 1U)) - 1ULL);
    uint64_t emask = (esize == 64U) ? UINT64_MAX : ((1ULL << esize) - 1ULL);
    if (r != 0U) {
        welem = ((welem >> r) | (welem << (esize - r))) & emask;
    }
    uint64_t wmask = welem;
    uint64_t tmask = telem;
    for (uint32_t sh = esize; sh < (is_64 ? 64U : 32U); sh <<= 1) {
        wmask |= wmask << sh;
        tmask |= tmask << sh;
    }
    if (!is_64) {
        wmask &= 0xFFFFFFFFULL;
        tmask &= 0xFFFFFFFFULL;
    }
    if (wmask_out) *wmask_out = wmask;
    if (tmask_out) *tmask_out = tmask;
    return true;
}

static uint64_t pack_spsr(const vcpu_t *cpu) {
    uint64_t mode = cpu->el == 0 ? 0ULL : (4ULL | (uint64_t)(cpu->spsel & 1U));
    return (uint64_t)(cpu->nzcv & 0xF0000000U) |
           ((uint64_t)(cpu->pan & 1U) << 22) |
           (uint64_t)(cpu->daif & 0x3C0U) |
           mode;
}

static uint64_t exception_vector_base_off(const vcpu_t *cpu) {
    if (cpu->el == 0) {
        return 0x400ULL;
    }
    return cpu->spsel ? 0x200ULL : 0x000ULL;
}

void vcpu_raise_exception(vcpu_t *cpu, uint32_t esr, uint64_t far, uint64_t elr) {
    uint64_t off = exception_vector_base_off(cpu);
    cpu->spsr_el1 = pack_spsr(cpu);
    cpu->elr_el1 = elr;
    cpu->esr_el1 = esr;
    cpu->far_el1 = far;
    cpu->el = 1;
    cpu->spsel = 1;
    cpu->daif |= 0x3C0U;
    if ((cpu->sctlr_el1 & (1ULL << 23)) == 0) {
        cpu->pan = 1;
    }
    mmu_update_asid_salt(cpu);
    cpu->excl_valid = false;
    cpu->excl_hva = 0;
    atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    cpu->ras_epoch++;
    cpu->pc = (cpu->vbar_el1 & ~0x7FFULL) + off;
}

static void vcpu_take_irq(vcpu_t *cpu) {
    uint64_t off = exception_vector_base_off(cpu) + 0x80ULL;
    cpu->spsr_el1 = pack_spsr(cpu);
    cpu->elr_el1 = cpu->pc;
    cpu->el = 1;
    cpu->spsel = 1;
    cpu->daif |= 0x3C0U;
    if ((cpu->sctlr_el1 & (1ULL << 23)) == 0) {
        cpu->pan = 1;
    }
    mmu_update_asid_salt(cpu);
    cpu->excl_valid = false;
    cpu->excl_hva = 0;
    atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    cpu->ras_epoch++;
    cpu->pc = (cpu->vbar_el1 & ~0x7FFULL) + off;
}

void vcpu_eret(vcpu_t *cpu) {
    uint64_t spsr = cpu->spsr_el1;
    cpu->nzcv = (uint32_t)(spsr & 0xF0000000U);
    cpu->daif = (uint32_t)(spsr & 0x3C0U);
    uint8_t new_pan = (uint8_t)((spsr >> 22) & 1U);
    uint32_t mode = (uint32_t)(spsr & 0x1FU);
    if (mode == 0U) {
        cpu->el = 0;
        cpu->spsel = 0;
    } else if (mode == 4U) {
        cpu->el = 1;
        cpu->spsel = 0;
    } else {
        cpu->el = 1;
        cpu->spsel = 1;
    }
    cpu->pan = new_pan;
    mmu_update_asid_salt(cpu);
    cpu->excl_valid = false;
    cpu->excl_hva = 0;
    atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    cpu->ras_epoch++;
    cpu->pc = cpu->elr_el1;
}

void vcpu_flush_icache_all(vcpu_t *cpu) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (!ic) {
        return;
    }
    for (uint32_t s = 0; s <= ic->set_mask; ++s) {
        for (uint32_t w = 0; w < ICACHE_WAYS; ++w) {
            ir_page_t *pg = ic->slots[s][w];
            if (pg) {
                pg->pa_page = UINT64_MAX;
                pg->decoded_mask = 0;
                pg->gen = ++ic->next_gen;
            }
        }
    }
    cpu->exec_epoch++;
    cpu->ras_epoch++;
}

static void icache_invalidate_page(vcpu_t *cpu, cpu_icache_t *ic, ir_page_t *pg) {
    pg->pa_page = UINT64_MAX;
    pg->decoded_mask = 0;
    pg->gen = ++ic->next_gen;
    if (ic->next_gen == 0U) {
        ic->next_gen = 1U;
        cpu->exec_epoch++;
        cpu->ras_epoch++;
    }
    if (pg == ic->cur_page) {
        cpu->exec_epoch++;
    }
}

void vcpu_flush_icache_pa(vcpu_t *cpu, uint64_t pa_page) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (!ic) {
        return;
    }
    ir_page_t **ways = ic->slots[(pa_page >> VMM_PAGE_SHIFT) & ic->set_mask];
    for (uint32_t w = 0; w < ICACHE_WAYS; ++w) {
        if (ways[w] && ways[w]->pa_page == pa_page) {
            icache_invalidate_page(cpu, ic, ways[w]);
        }
    }
}

bool vcpu_flush_icache_va(vcpu_t *cpu, uint64_t va) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (!ic) {
        return false;
    }
    uint64_t va_page = va & ~(uint64_t)VMM_PAGE_MASK;
    uint64_t pa;
    if (!mmu_resolve_va_pa(cpu, va_page, &pa)) {
        for (uint32_t s = 0; s <= ic->set_mask; ++s) {
            for (uint32_t w = 0; w < ICACHE_WAYS; ++w) {
                ir_page_t *pg = ic->slots[s][w];
                if (pg && pg->va_page == va_page && pg->pa_page != UINT64_MAX) {
                    icache_invalidate_page(cpu, ic, pg);
                }
            }
        }
        cpu->icache_shootdown_all = true;
        cpu->icache_shootdown_pending = true;
        return false;
    }

    uint64_t pa_page = pa & ~(uint64_t)VMM_PAGE_MASK;
    uint64_t mask;
    _Atomic uint64_t *word = cpu_icache_ram_word(cpu->vm, pa_page, &mask);
    bool cached = true;
    if (word) {
        atomic_thread_fence(memory_order_seq_cst);
        cached = (atomic_load_explicit(word, memory_order_seq_cst) & mask) != 0;
    }
    if (cached) {
        vcpu_flush_icache_pa(cpu, pa_page);
    }
    for (uint32_t i = 0; i < cpu->icache_pending_count; ++i) {
        if (cpu->icache_pending_pa[i] == pa_page) {
            return true;
        }
    }
    if (cpu->icache_pending_count < VCPU_ICACHE_PENDING) {
        cpu->icache_pending_pa[cpu->icache_pending_count++] = pa_page;
        cpu->icache_shootdown_pending = true;
        return true;
    }
    if (cached) {
        cpu->icache_shootdown_all = true;
        cpu->icache_shootdown_pending = true;
    }
    return false;
}

static void invalidate_softtlb(vcpu_t *cpu) {
    for (uint32_t b = 0; b < SOFTTLB_BANK_COUNT; ++b) {
        for (uint32_t i = 0; i < SOFTTLB_ENTRIES; ++i) {
            cpu->tlb[b][i].tag = SOFTTLB_TAG_INVALID;
            cpu->tlb[b][i].addend = 0;
        }
    }
}

void vcpu_init(vcpu_t *cpu, vmm_machine_t *vm, uint32_t cpu_id) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->vm = vm;
    cpu->cpu_id = cpu_id;
    cpu->mpidr_el1 = 0x80000000ULL | (uint64_t)cpu_id;
    cpu->icache = calloc(1, sizeof(cpu_icache_t));
#ifdef CPU_STATS
    ((cpu_icache_t *)cpu->icache)->stats = calloc(1, sizeof(cpu_stats_t));
#endif
    vcpu_reset(cpu, 0, 0);
}

void vcpu_destroy(vcpu_t *cpu) {
    if (!cpu) {
        return;
    }
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (ic) {
#ifdef CPU_STATS
        cpu_stats_dump(cpu);
        free(ic->stats);
#endif
        for (uint32_t s = 0; s <= ic->set_mask; ++s) {
            for (uint32_t w = 0; w < ICACHE_WAYS; ++w) {
                free(ic->slots[s][w]);
                ic->slots[s][w] = NULL;
            }
        }
        free(ic);
        cpu->icache = NULL;
    }
}

void vcpu_reset(vcpu_t *cpu, uint64_t entry_pc, uint64_t x0) {
    vmm_machine_t *vm = cpu->vm;
    uint32_t cpu_id = cpu->cpu_id;
    uint64_t mpidr = cpu->mpidr_el1;
    void *icache = cpu->icache;

    memset(cpu, 0, sizeof(*cpu));
    cpu->vm = vm;
    cpu->cpu_id = cpu_id;
    cpu->mpidr_el1 = mpidr;
    cpu->icache = icache;

    cpu->x[0] = x0;
    cpu->pc = entry_pc;
    cpu->daif = 0x3C0U;
    cpu->el = 1;
    cpu->spsel = 1;
    cpu->backedge_budget = BACKEDGE_BUDGET_INIT;
    cpu->sctlr_el1 = 0x00C50838ULL;
    cpu->cpacr_el1 = 3ULL << 20;
    cpu->exec_epoch = 1ULL;
    cpu->ras_epoch = 1ULL;
    cpu->ic_ivau_last_va_page = UINT64_MAX;
    cpu->fast_sp = 0ULL;
    atomic_store_explicit(&cpu->irq_pending, 0U, memory_order_relaxed);
    atomic_store_explicit(&cpu->stop_requested, false, memory_order_relaxed);
    atomic_store_explicit(&cpu->halted_wfi, false, memory_order_relaxed);

    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (ic) {
        if (ic->next_gen == 0U) {
            ic->next_gen = 1U;
        }
        ic->set_mask = (!vm || vm->ram_size >= (1536ULL << 20)) ? (ICACHE_SETS - 1U) : 255U;
        ic->ras_sp = 0U;
        memset(ic->exec_cache, 0, sizeof(ic->exec_cache));
        memset(ic->ras, 0, sizeof(ic->ras));
    }

    invalidate_softtlb(cpu);
    vcpu_flush_icache_all(cpu);
    if (cpu->vm) {
        mmu_update_asid_salt(cpu);
    }
}

static CPU_NOINLINE void op_page_end(vcpu_t *cpu, const ir_insn_t *ir,
                                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t next_pc = cpu_ir_pc(cpu, ir);
    CPU_JUMP_PC(cpu, next_pc, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_undef(vcpu_t *cpu, const ir_insn_t *ir,
                                  softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    (void)tlb_r;
    (void)tlb_w;
    uint64_t pc = cpu_ir_pc(cpu, ir);
    vcpu_raise_undef(cpu, pc);
}

static CPU_NOINLINE void op_nop(vcpu_t *cpu, const ir_insn_t *ir,
                                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_add_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_xsp(cpu, ir->rd, cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_add_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t res = (uint32_t)cpu_get_xsp(cpu, ir->rn) + (uint32_t)ir->target_or_imm;
    cpu_set_xsp(cpu, ir->rd, (uint64_t)res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sub_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_xsp(cpu, ir->rd, cpu_get_xsp(cpu, ir->rn) - ir->target_or_imm);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sub_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t res = (uint32_t)cpu_get_xsp(cpu, ir->rn) - (uint32_t)ir->target_or_imm;
    cpu_set_xsp(cpu, ir->rd, (uint64_t)res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_adds_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_adds64(cpu_get_xsp(cpu, ir->rn), ir->target_or_imm, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_adds_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_adds32((uint32_t)cpu_get_xsp(cpu, ir->rn), (uint32_t)ir->target_or_imm, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_subs_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_subs64(cpu_get_xsp(cpu, ir->rn), ir->target_or_imm, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_subs_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_subs32((uint32_t)cpu_get_xsp(cpu, ir->rn), (uint32_t)ir->target_or_imm, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static inline uint64_t apply_shift(uint64_t val, uint32_t type, uint32_t amt, bool wide) {
    if (amt == 0U) {
        return wide ? val : (uint64_t)(uint32_t)val;
    }
    if (wide) {
        switch (type) {
        case 0U: return val << amt;
        case 1U: return val >> amt;
        case 2U: return (uint64_t)((int64_t)val >> amt);
        default: return (val >> amt) | (val << (64U - amt));
        }
    }
    uint32_t v = (uint32_t)val;
    switch (type) {
    case 0U: return (uint64_t)(v << amt);
    case 1U: return (uint64_t)(v >> amt);
    case 2U: return (uint64_t)(uint32_t)((int32_t)v >> amt);
    default: return (uint64_t)((v >> amt) | (v << (32U - amt)));
    }
}

static inline uint64_t apply_extend(uint64_t val, uint32_t opt, uint32_t sh) {
    uint64_t ext;
    switch (opt) {
    case 0U: ext = (uint8_t)val; break;
    case 1U: ext = (uint16_t)val; break;
    case 2U: ext = (uint32_t)val; break;
    case 3U: ext = val; break;
    case 4U: ext = (uint64_t)(int64_t)(int8_t)val; break;
    case 5U: ext = (uint64_t)(int64_t)(int16_t)val; break;
    case 6U: ext = (uint64_t)(int64_t)(int32_t)val; break;
    default: ext = val; break;
    }
    return ext << sh;
}

static CPU_NOINLINE void op_addsub_shift(vcpu_t *cpu, const ir_insn_t *ir,
                                         softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    bool sub = (ir->flags & IR_FLAG_SUB) != 0;
    bool setf = (ir->flags & IR_FLAG_SETFLAGS) != 0;
    uint32_t sh_type = (ir->op >> 8) & 3U;
    uint32_t sh_amt = ir->op & 0x3FU;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = apply_shift(cpu_get_x(cpu, ir->rm), sh_type, sh_amt, wide);
    uint64_t res;
    if (wide) {
        if (setf) res = sub ? cpu_subs64(a, b, &cpu->nzcv) : cpu_adds64(a, b, &cpu->nzcv);
        else res = sub ? (a - b) : (a + b);
    } else {
        if (setf) res = sub ? cpu_subs32((uint32_t)a, (uint32_t)b, &cpu->nzcv)
                            : cpu_adds32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
        else res = (uint64_t)(sub ? ((uint32_t)a - (uint32_t)b) : ((uint32_t)a + (uint32_t)b));
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_add_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) + cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_add_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) + (uint32_t)cpu_get_x(cpu, ir->rm)));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sub_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) - cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sub_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) - (uint32_t)cpu_get_x(cpu, ir->rm)));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_subs_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_subs64(cpu_get_x(cpu, ir->rn), cpu_get_x(cpu, ir->rm), &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_subs_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = cpu_subs32((uint32_t)cpu_get_x(cpu, ir->rn), (uint32_t)cpu_get_x(cpu, ir->rm), &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_addsub_ext(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    bool sub = (ir->flags & IR_FLAG_SUB) != 0;
    bool setf = (ir->flags & IR_FLAG_SETFLAGS) != 0;
    uint32_t opt = (ir->op >> 8) & 7U;
    uint32_t sh = ir->op & 7U;
    uint64_t a = cpu_get_xsp(cpu, ir->rn);
    uint64_t b = apply_extend(cpu_get_x(cpu, ir->rm), opt, sh);
    uint64_t res;
    if (wide) {
        if (setf) res = sub ? cpu_subs64(a, b, &cpu->nzcv) : cpu_adds64(a, b, &cpu->nzcv);
        else res = sub ? (a - b) : (a + b);
    } else {
        if (setf) res = sub ? cpu_subs32((uint32_t)a, (uint32_t)b, &cpu->nzcv)
                            : cpu_adds32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
        else res = (uint64_t)(sub ? ((uint32_t)a - (uint32_t)b) : ((uint32_t)a + (uint32_t)b));
    }
    if (setf) cpu_set_x(cpu, ir->rd, res);
    else cpu_set_xsp(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_addsub_carry(vcpu_t *cpu, const ir_insn_t *ir,
                                         softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    bool sub = (ir->flags & IR_FLAG_SUB) != 0;
    bool setf = (ir->flags & IR_FLAG_SETFLAGS) != 0;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = cpu_get_x(cpu, ir->rm);
    uint64_t res;
    if (wide) {
        if (sub) res = setf ? cpu_sbcs64(a, b, cpu->nzcv, &cpu->nzcv) : cpu_sbc64(a, b, cpu->nzcv);
        else res = setf ? cpu_adcs64(a, b, cpu->nzcv, &cpu->nzcv) : cpu_adc64(a, b, cpu->nzcv);
    } else {
        if (sub) res = setf ? cpu_sbcs32((uint32_t)a, (uint32_t)b, cpu->nzcv, &cpu->nzcv)
                            : cpu_sbc32((uint32_t)a, (uint32_t)b, cpu->nzcv);
        else res = setf ? cpu_adcs32((uint32_t)a, (uint32_t)b, cpu->nzcv, &cpu->nzcv)
                        : cpu_adc32((uint32_t)a, (uint32_t)b, cpu->nzcv);
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_log_imm(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t imm = ir->target_or_imm;
    if (!wide) a = (uint32_t)a;
    switch (ir->op) {
    case 0U: cpu_set_xsp(cpu, ir->rd, a & imm); break;
    case 1U: cpu_set_xsp(cpu, ir->rd, a | imm); break;
    case 2U: cpu_set_xsp(cpu, ir->rd, a ^ imm); break;
    default: {
        uint64_t r = wide ? cpu_ands64(a, imm, &cpu->nzcv) : cpu_ands32((uint32_t)a, (uint32_t)imm, &cpu->nzcv);
        cpu_set_x(cpu, ir->rd, r);
        break;
    }
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_log_shift(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    bool inv = (ir->flags & IR_FLAG_INV) != 0;
    uint32_t sh_type = (ir->op >> 8) & 3U;
    uint32_t sh_amt = ir->op & 0x3FU;
    uint32_t opc = (ir->op >> 10) & 3U;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = apply_shift(cpu_get_x(cpu, ir->rm), sh_type, sh_amt, wide);
    if (!wide) a = (uint32_t)a;
    if (inv) b = wide ? ~b : (uint64_t)(uint32_t)~b;
    uint64_t res;
    switch (opc) {
    case 0U: res = a & b; break;
    case 1U: res = a | b; break;
    case 2U: res = a ^ b; break;
    default: res = wide ? cpu_ands64(a, b, &cpu->nzcv) : cpu_ands32((uint32_t)a, (uint32_t)b, &cpu->nzcv); break;
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_mov_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_mov_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)(uint32_t)cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_and_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) & cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_orr_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) | cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_eor_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) ^ cpu_get_x(cpu, ir->rm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_and_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) & (uint32_t)cpu_get_x(cpu, ir->rm)));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_orr_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) | (uint32_t)cpu_get_x(cpu, ir->rm)));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_eor_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) ^ (uint32_t)cpu_get_x(cpu, ir->rm)));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ands_reg64(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t r = cpu_ands64(cpu_get_x(cpu, ir->rn), cpu_get_x(cpu, ir->rm), &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, r);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ands_reg32(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t r = cpu_ands32((uint32_t)cpu_get_x(cpu, ir->rn), (uint32_t)cpu_get_x(cpu, ir->rm), &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, r);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_mov_imm(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, ir->target_or_imm);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_lsl_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) << ir->op);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_lsl_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) << ir->op));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_lsr_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, cpu_get_x(cpu, ir->rn) >> ir->op);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_lsr_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, ir->rn) >> ir->op));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_asr_imm64(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((int64_t)cpu_get_x(cpu, ir->rn) >> ir->op));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_asr_imm32(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)(uint32_t)((int32_t)(uint32_t)cpu_get_x(cpu, ir->rn) >> ir->op));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ubfx64(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (cpu_get_x(cpu, ir->rn) >> ir->op) & ir->target_or_imm);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ubfx32(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)(((uint32_t)cpu_get_x(cpu, ir->rn) >> ir->op) & (uint32_t)ir->target_or_imm));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sbfx64(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t lsh = ir->op >> 8;
    uint32_t rsh = ir->op & 0xFFU;
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)((int64_t)(cpu_get_x(cpu, ir->rn) << lsh) >> rsh));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_sbfx32(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t lsh = ir->op >> 8;
    uint32_t rsh = ir->op & 0xFFU;
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)(uint32_t)((int32_t)((uint32_t)cpu_get_x(cpu, ir->rn) << lsh) >> rsh));
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_movk(vcpu_t *cpu, const ir_insn_t *ir,
                                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint32_t sh = ir->op;
    uint64_t cur = cpu_get_x(cpu, ir->rd);
    uint64_t val = (cur & ~(0xFFFFULL << sh)) | (ir->target_or_imm << sh);
    if (!wide) val = (uint32_t)val;
    cpu_set_x(cpu, ir->rd, val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_bitfield(vcpu_t *cpu, const ir_insn_t *ir,
                                     softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint32_t opc = (ir->op >> 12) & 3U;
    uint32_t immr = (ir->op >> 6) & 0x3FU;
    uint32_t imms = ir->op & 0x3FU;
    uint64_t src = cpu_get_x(cpu, ir->rn);
    uint64_t dst = opc == 1U ? cpu_get_x(cpu, ir->rd) : 0ULL;
    uint64_t wmask = 0, tmask = 0;
    vcpu_decode_bitmask(wide ? 1U : 0U, imms, immr, wide, false, &wmask, &tmask);
    uint64_t rot;
    if (wide) {
        rot = immr ? ((src >> immr) | (src << (64U - immr))) : src;
    } else {
        uint32_t s32 = (uint32_t)src;
        dst = (uint32_t)dst;
        rot = immr ? (uint64_t)((s32 >> immr) | (s32 << (32U - immr))) : (uint64_t)s32;
    }
    uint64_t bot = (dst & ~wmask) | (rot & wmask);
    uint64_t top = 0;
    if (opc == 0U) {
        bool sign = ((src >> imms) & 1ULL) != 0;
        top = sign ? (wide ? UINT64_MAX : 0xFFFFFFFFULL) : 0ULL;
    } else if (opc == 1U) {
        top = dst;
    }
    uint64_t res = (top & ~tmask) | (bot & tmask);
    if (!wide) res = (uint32_t)res;
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_extr(vcpu_t *cpu, const ir_insn_t *ir,
                                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint32_t lsb = ir->op;
    uint64_t hi = cpu_get_x(cpu, ir->rn);
    uint64_t lo = cpu_get_x(cpu, ir->rm);
    uint64_t res;
    if (wide) {
        res = lsb == 0U ? lo : ((lo >> lsb) | (hi << (64U - lsb)));
    } else {
        uint32_t h32 = (uint32_t)hi, l32 = (uint32_t)lo;
        res = lsb == 0U ? (uint64_t)l32 : (uint64_t)((l32 >> lsb) | (h32 << (32U - lsb)));
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_condcmp(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t cond = ir->op & 15U;
    uint32_t nzcv_imm = (ir->op >> 4) & 15U;
    if (!cpu_cond_holds(cpu->nzcv, cond)) {
        cpu->nzcv = nzcv_imm << 28;
        CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
    }
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    bool sub = (ir->flags & IR_FLAG_SUB) != 0;
    bool is_imm = (ir->flags & IR_FLAG_INV) != 0;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = is_imm ? ir->target_or_imm : cpu_get_x(cpu, ir->rm);
    if (wide) {
        if (sub) (void)cpu_subs64(a, b, &cpu->nzcv);
        else (void)cpu_adds64(a, b, &cpu->nzcv);
    } else {
        if (sub) (void)cpu_subs32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
        else (void)cpu_adds32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_condsel(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint32_t cond = ir->op & 15U;
    uint32_t kind = (ir->op >> 4) & 3U;
    uint64_t res;
    if (cpu_cond_holds(cpu->nzcv, cond)) {
        res = cpu_get_x(cpu, ir->rn);
    } else {
        uint64_t m = cpu_get_x(cpu, ir->rm);
        if (kind == 0U) res = m;
        else if (kind == 1U) res = m + 1ULL;
        else if (kind == 2U) res = ~m;
        else res = -m;
    }
    if (!wide) res = (uint32_t)res;
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_dp_1src(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint64_t v = cpu_get_x(cpu, ir->rn);
    uint64_t res = 0;
    switch (ir->op) {
    case 0U:
        if (wide) {
            __asm__("rbit %0, %1" : "=r"(res) : "r"(v));
        } else {
            uint32_t r32;
            __asm__("rbit %w0, %w1" : "=r"(r32) : "r"((uint32_t)v));
            res = r32;
        }
        break;
    case 1U:
        if (wide) {
            __asm__("rev16 %0, %1" : "=r"(res) : "r"(v));
        } else {
            uint32_t r32;
            __asm__("rev16 %w0, %w1" : "=r"(r32) : "r"((uint32_t)v));
            res = r32;
        }
        break;
    case 2U:
        if (wide) {
            __asm__("rev32 %0, %1" : "=r"(res) : "r"(v));
        } else {
            res = __builtin_bswap32((uint32_t)v);
        }
        break;
    case 3U:
        res = __builtin_bswap64(v);
        break;
    case 4U:
        if (wide) res = v == 0ULL ? 64ULL : (uint64_t)__builtin_clzll(v);
        else res = ((uint32_t)v) == 0U ? 32ULL : (uint64_t)__builtin_clz((uint32_t)v);
        break;
    default:
        if (wide) {
            uint64_t x = v ^ (uint64_t)((int64_t)v >> 63);
            uint64_t y = x << 1;
            res = y == 0ULL ? 63ULL : (uint64_t)(__builtin_clzll(y));
        } else {
            uint32_t v32 = (uint32_t)v;
            uint32_t x = v32 ^ (uint32_t)((int32_t)v32 >> 31);
            uint32_t y = x << 1;
            res = y == 0U ? 31ULL : (uint64_t)(__builtin_clz(y));
        }
        break;
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_dp_2src(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = cpu_get_x(cpu, ir->rm);
    uint64_t res = 0;
    switch (ir->op) {
    case 0U:
        if (wide) res = b == 0ULL ? 0ULL : (a / b);
        else { uint32_t b32 = (uint32_t)b; res = b32 == 0U ? 0ULL : (uint64_t)((uint32_t)a / b32); }
        break;
    case 1U:
        if (wide) {
            int64_t sa = (int64_t)a, sb = (int64_t)b;
            if (sb == 0) res = 0ULL;
            else if (sa == INT64_MIN && sb == -1) res = (uint64_t)INT64_MIN;
            else res = (uint64_t)(sa / sb);
        } else {
            int32_t sa = (int32_t)a, sb = (int32_t)b;
            if (sb == 0) res = 0ULL;
            else if (sa == INT32_MIN && sb == -1) res = (uint64_t)(uint32_t)INT32_MIN;
            else res = (uint64_t)(uint32_t)(sa / sb);
        }
        break;
    case 2U:
        res = wide ? (a << (b & 63U)) : (uint64_t)((uint32_t)a << (b & 31U));
        break;
    case 3U:
        res = wide ? (a >> (b & 63U)) : (uint64_t)((uint32_t)a >> (b & 31U));
        break;
    case 4U:
        res = wide ? (uint64_t)((int64_t)a >> (b & 63U))
                   : (uint64_t)(uint32_t)((int32_t)a >> (b & 31U));
        break;
    default: {
        uint32_t sh = wide ? (uint32_t)(b & 63U) : (uint32_t)(b & 31U);
        if (wide) res = sh == 0U ? a : ((a >> sh) | (a << (64U - sh)));
        else { uint32_t a32 = (uint32_t)a; res = sh == 0U ? (uint64_t)a32 : (uint64_t)((a32 >> sh) | (a32 << (32U - sh))); }
        break;
    }
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_dp_3src(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t n = cpu_get_x(cpu, ir->rn);
    uint64_t m = cpu_get_x(cpu, ir->rm);
    uint64_t a = cpu_get_x(cpu, ir->ra);
    uint64_t res = 0;
    switch (ir->op) {
    case 0U: res = (uint64_t)((uint32_t)a + (uint32_t)n * (uint32_t)m); break;
    case 1U: res = (uint64_t)((uint32_t)a - (uint32_t)n * (uint32_t)m); break;
    case 2U: res = a + n * m; break;
    case 3U: res = a - n * m; break;
    case 4U: res = a + (uint64_t)((int64_t)(int32_t)n * (int64_t)(int32_t)m); break;
    case 5U: res = a - (uint64_t)((int64_t)(int32_t)n * (int64_t)(int32_t)m); break;
    case 6U: res = a + (uint64_t)(uint32_t)n * (uint64_t)(uint32_t)m; break;
    case 7U: res = a - (uint64_t)(uint32_t)n * (uint64_t)(uint32_t)m; break;
    case 8U: res = (uint64_t)(((__int128)(int64_t)n * (__int128)(int64_t)m) >> 64); break;
    default: res = (uint64_t)(((__uint128_t)n * (__uint128_t)m) >> 64); break;
    }
    cpu_set_x(cpu, ir->rd, res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_b(vcpu_t *cpu, const ir_insn_t *ir,
                              softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (ir->flags & IR_FLAG_INPAGE) {
        const ir_insn_t *target = ir->target_ir;
        if (target <= ir && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
            cpu_backedge_miss(cpu, target)) {
            return;
        }
        CPU_NEXT(cpu, target, tlb_r, tlb_w);
    }
    CPU_JUMP_PC(cpu, ir->target_or_imm, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_bl(vcpu_t *cpu, const ir_insn_t *ir,
                               softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t ret_pc = cpu_ir_pc(cpu, ir) + 4ULL;
    cpu->x[30] = ret_pc;
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    ras_entry_t *re = &ic->ras[ic->ras_sp++ & RAS_MASK];
    re->pc = ret_pc;
    re->ras_epoch = (uint32_t)cpu->ras_epoch;
    re->pg = ic->cur_page;
    re->pg_gen = ic->cur_page->gen;
    re->ir = ir + 1;
    if (ir->flags & IR_FLAG_INPAGE) {
        const ir_insn_t *target = ir->target_ir;
        if (target <= ir && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
            cpu_backedge_miss(cpu, target)) {
            return;
        }
        CPU_NEXT(cpu, target, tlb_r, tlb_w);
    }
    CPU_JUMP_PC(cpu, ir->target_or_imm, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_b_cond(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (cpu_cond_holds(cpu->nzcv, ir->op)) {
        if (ir->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = ir->target_ir;
            if (target <= ir && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, ir->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_cbz(vcpu_t *cpu, const ir_insn_t *ir,
                                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t v = cpu_get_x(cpu, ir->rd);
    if (!(ir->flags & IR_FLAG_WIDE)) v = (uint32_t)v;
    bool taken = (ir->flags & IR_FLAG_INV) ? (v != 0ULL) : (v == 0ULL);
    if (taken) {
        if (ir->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = ir->target_ir;
            if (target <= ir && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, ir->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_tbz(vcpu_t *cpu, const ir_insn_t *ir,
                                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t bit = (cpu_get_x(cpu, ir->rd) >> ir->op) & 1ULL;
    bool taken = (ir->flags & IR_FLAG_INV) ? (bit != 0ULL) : (bit == 0ULL);
    if (taken) {
        if (ir->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = ir->target_ir;
            if (target <= ir && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, ir->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ret(vcpu_t *cpu, const ir_insn_t *ir,
                                softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t target = cpu_get_x(cpu, ir->rn);
    if (__builtin_expect(--cpu->backedge_budget <= 0, 0)) {
        if (cpu_backedge_miss_pc(cpu, target)) {
            return;
        }
    }
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    uint32_t idx = (ic->ras_sp - 1U) & RAS_MASK;
    const ras_entry_t *re = &ic->ras[idx];
    if (__builtin_expect(re->pc == target && re->ras_epoch == (uint32_t)cpu->ras_epoch &&
                         re->pg != NULL && re->pg->gen == re->pg_gen && re->ir != NULL, 1)) {
        ic->ras_sp = idx;
        ic->cur_page = re->pg;
        ic->cur_exec_epoch = cpu->exec_epoch;
        CPU_NEXT(cpu, re->ir, tlb_r, tlb_w);
    }
    CPU_JUMP_PC_NOBUDGET(cpu, target, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_branch_reg(vcpu_t *cpu, const ir_insn_t *ir,
                                       softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t target = cpu_get_x(cpu, ir->rn);
    if (ir->op == 1U) {
        uint64_t ret_pc = cpu_ir_pc(cpu, ir) + 4ULL;
        cpu->x[30] = ret_pc;
        cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
        ras_entry_t *re = &ic->ras[ic->ras_sp++ & RAS_MASK];
        re->pc = ret_pc;
        re->ras_epoch = (uint32_t)cpu->ras_epoch;
        re->pg = ic->cur_page;
        re->pg_gen = ic->cur_page->gen;
        re->ir = ir + 1;
    }
    CPU_JUMP_PC(cpu, target, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldr_u64(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint64_t val = 0;
    if (!cpu_read_64(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldr_u32(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint32_t val = 0;
    if (!cpu_read_32(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldrsw_u(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint32_t val = 0;
    if (!cpu_read_32(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)(int64_t)(int32_t)val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldrh_u(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint16_t val = 0;
    if (!cpu_read_16(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldrb_u(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint8_t val = 0;
    if (!cpu_read_8(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)val);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_str_u64(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    if (!cpu_write_64(cpu, ir, 0, tlb_w, va, cpu_get_x(cpu, ir->rd))) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_str_u32(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    if (!cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)cpu_get_x(cpu, ir->rd))) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_strh_u(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    if (!cpu_write_16(cpu, ir, 0, tlb_w, va, (uint16_t)cpu_get_x(cpu, ir->rd))) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_strb_u(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    if (!cpu_write_8(cpu, ir, 0, tlb_w, va, (uint8_t)cpu_get_x(cpu, ir->rd))) {
        return;
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldp_64(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t base = cpu_get_xsp(cpu, ir->rn);
    uint64_t off = (uint64_t)ir->simm;
    uint64_t va = (ir->flags & IR_FLAG_POST) ? base : (base + off);
    uint64_t v1 = 0, v2 = 0;
    if (!cpu_read_pair_64(cpu, ir, 0, tlb_r, va, &v1, &v2)) {
        return;
    }
    if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) {
        cpu_set_xsp(cpu, ir->rn, base + off);
    }
    cpu_set_x_fast(cpu, ir->rd, v1);
    cpu_set_x_fast(cpu, ir->rm, v2);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_stp_64(vcpu_t *cpu, const ir_insn_t *ir,
                                   softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint64_t base = cpu_get_xsp(cpu, ir->rn);
    uint64_t off = (uint64_t)ir->simm;
    uint64_t va = (ir->flags & IR_FLAG_POST) ? base : (base + off);
    if (!cpu_write_pair_64(cpu, ir, 0, tlb_w, va, cpu_get_x(cpu, ir->rd), cpu_get_x(cpu, ir->rm))) {
        return;
    }
    if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) {
        cpu_set_xsp(cpu, ir->rn, base + off);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldst_single(vcpu_t *cpu, const ir_insn_t *ir,
                                        softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool is_simd = (ir->flags & IR_FLAG_SIMD) != 0;
    if (is_simd && !cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint32_t size = ir->op & 7U;
    bool is_load = ((ir->op >> 3) & 1U) != 0;
    uint32_t sext_mode = (ir->op >> 4) & 3U;
    bool is_regoff = ((ir->op >> 6) & 1U) != 0;
    uint64_t base = cpu_get_xsp(cpu, ir->rn);
    uint64_t off;
    if (is_regoff) {
        uint32_t opt = (ir->op >> 8) & 7U;
        uint32_t sh = (ir->op >> 11) & 7U;
        off = apply_extend(cpu_get_x(cpu, ir->rm), opt, sh);
    } else {
        off = (uint64_t)ir->simm;
    }
    uint64_t va = (ir->flags & IR_FLAG_POST) ? base : (base + off);

    if (is_load) {
        if (is_simd) {
            __uint128_t v = 0;
            bool ok = false;
            if (size == 0U) { uint8_t b = 0; ok = cpu_read_8(cpu, ir, 0, tlb_r, va, &b); v = b; }
            else if (size == 1U) { uint16_t h = 0; ok = cpu_read_16(cpu, ir, 0, tlb_r, va, &h); v = h; }
            else if (size == 2U) { uint32_t w = 0; ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &w); v = w; }
            else if (size == 3U) { uint64_t d = 0; ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &d); v = d; }
            else { ok = cpu_read_128(cpu, ir, 0, tlb_r, va, &v); }
            if (!ok) return;
            cpu->v[ir->rd] = v;
        } else {
            uint64_t v = 0;
            bool ok = false;
            if (size == 0U) { uint8_t b = 0; ok = cpu_read_8(cpu, ir, 0, tlb_r, va, &b); v = b; }
            else if (size == 1U) { uint16_t h = 0; ok = cpu_read_16(cpu, ir, 0, tlb_r, va, &h); v = h; }
            else if (size == 2U) { uint32_t w = 0; ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &w); v = w; }
            else { ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &v); }
            if (!ok) return;
            if (sext_mode == 1U) v = (uint64_t)cpu_sext(v, (1U << size) * 8U);
            else if (sext_mode == 2U) v = (uint64_t)(uint32_t)cpu_sext(v, (1U << size) * 8U);
            cpu_set_x(cpu, ir->rd, v);
        }
    } else {
        if (is_simd) {
            __uint128_t v = cpu->v[ir->rd];
            bool ok = false;
            if (size == 0U) ok = cpu_write_8(cpu, ir, 0, tlb_w, va, (uint8_t)v);
            else if (size == 1U) ok = cpu_write_16(cpu, ir, 0, tlb_w, va, (uint16_t)v);
            else if (size == 2U) ok = cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)v);
            else if (size == 3U) ok = cpu_write_64(cpu, ir, 0, tlb_w, va, (uint64_t)v);
            else ok = cpu_write_128(cpu, ir, 0, tlb_w, va, v);
            if (!ok) return;
        } else {
            uint64_t v = cpu_get_x(cpu, ir->rd);
            bool ok = false;
            if (size == 0U) ok = cpu_write_8(cpu, ir, 0, tlb_w, va, (uint8_t)v);
            else if (size == 1U) ok = cpu_write_16(cpu, ir, 0, tlb_w, va, (uint16_t)v);
            else if (size == 2U) ok = cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)v);
            else ok = cpu_write_64(cpu, ir, 0, tlb_w, va, v);
            if (!ok) return;
        }
    }

    if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) {
        cpu_set_xsp(cpu, ir->rn, base + off);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldst_pair(vcpu_t *cpu, const ir_insn_t *ir,
                                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool is_simd = (ir->flags & IR_FLAG_SIMD) != 0;
    if (is_simd && !cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    uint32_t size = ir->op & 7U;
    bool is_load = ((ir->op >> 3) & 1U) != 0;
    bool is_signed = ((ir->op >> 4) & 1U) != 0;
    uint32_t step = 1U << size;
    uint64_t base = cpu_get_xsp(cpu, ir->rn);
    uint64_t off = (uint64_t)ir->simm;
    uint64_t va = (ir->flags & IR_FLAG_POST) ? base : (base + off);

    if (is_load) {
        if (is_simd) {
            __uint128_t v1 = 0, v2 = 0;
            bool ok = false;
            if (size == 2U) {
                uint32_t a = 0, b = 0;
                ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &a) && cpu_read_32(cpu, ir, 0, tlb_r, va + step, &b);
                v1 = a; v2 = b;
            } else if (size == 3U) {
                uint64_t a = 0, b = 0;
                ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &a) && cpu_read_64(cpu, ir, 0, tlb_r, va + step, &b);
                v1 = a; v2 = b;
            } else {
                ok = cpu_read_128(cpu, ir, 0, tlb_r, va, &v1) && cpu_read_128(cpu, ir, 0, tlb_r, va + step, &v2);
            }
            if (!ok) return;
            if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) cpu_set_xsp(cpu, ir->rn, base + off);
            cpu->v[ir->rd] = v1;
            cpu->v[ir->rm] = v2;
        } else {
            uint64_t v1 = 0, v2 = 0;
            bool ok = false;
            if (size == 2U) {
                uint32_t a = 0, b = 0;
                ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &a) && cpu_read_32(cpu, ir, 0, tlb_r, va + step, &b);
                v1 = is_signed ? (uint64_t)(int64_t)(int32_t)a : (uint64_t)a;
                v2 = is_signed ? (uint64_t)(int64_t)(int32_t)b : (uint64_t)b;
            } else {
                ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &v1) && cpu_read_64(cpu, ir, 0, tlb_r, va + step, &v2);
            }
            if (!ok) return;
            if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) cpu_set_xsp(cpu, ir->rn, base + off);
            cpu_set_x(cpu, ir->rd, v1);
            cpu_set_x(cpu, ir->rm, v2);
        }
    } else {
        bool ok = false;
        if (is_simd) {
            if (size == 2U) {
                ok = cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)cpu->v[ir->rd]) &&
                     cpu_write_32(cpu, ir, 0, tlb_w, va + step, (uint32_t)cpu->v[ir->rm]);
            } else if (size == 3U) {
                ok = cpu_write_64(cpu, ir, 0, tlb_w, va, (uint64_t)cpu->v[ir->rd]) &&
                     cpu_write_64(cpu, ir, 0, tlb_w, va + step, (uint64_t)cpu->v[ir->rm]);
            } else {
                ok = cpu_write_128(cpu, ir, 0, tlb_w, va, cpu->v[ir->rd]) &&
                     cpu_write_128(cpu, ir, 0, tlb_w, va + step, cpu->v[ir->rm]);
            }
        } else {
            if (size == 2U) {
                ok = cpu_write_32(cpu, ir, 0, tlb_w, va, (uint32_t)cpu_get_x(cpu, ir->rd)) &&
                     cpu_write_32(cpu, ir, 0, tlb_w, va + step, (uint32_t)cpu_get_x(cpu, ir->rm));
            } else {
                ok = cpu_write_64(cpu, ir, 0, tlb_w, va, cpu_get_x(cpu, ir->rd)) &&
                     cpu_write_64(cpu, ir, 0, tlb_w, va + step, cpu_get_x(cpu, ir->rm));
            }
        }
        if (!ok) return;
        if (ir->flags & (IR_FLAG_PRE | IR_FLAG_POST)) cpu_set_xsp(cpu, ir->rn, base + off);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_ldr_lit(vcpu_t *cpu, const ir_insn_t *ir,
                                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool is_simd = (ir->flags & IR_FLAG_SIMD) != 0;
    if (is_simd && !cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    uint64_t va = ir->target_or_imm;
    if (is_simd) {
        __uint128_t v = 0;
        bool ok = false;
        if (ir->op == 0U) { uint32_t w = 0; ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &w); v = w; }
        else if (ir->op == 1U) { uint64_t d = 0; ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &d); v = d; }
        else { ok = cpu_read_128(cpu, ir, 0, tlb_r, va, &v); }
        if (!ok) return;
        cpu->v[ir->rd] = v;
    } else {
        uint64_t v = 0;
        bool ok = false;
        if (ir->op == 0U) { uint32_t w = 0; ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &w); v = w; }
        else if (ir->op == 1U) { ok = cpu_read_64(cpu, ir, 0, tlb_r, va, &v); }
        else { uint32_t w = 0; ok = cpu_read_32(cpu, ir, 0, tlb_r, va, &w); v = (uint64_t)(int64_t)(int32_t)w; }
        if (!ok) return;
        cpu_set_x(cpu, ir->rd, v);
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_subs_imm_bcond(vcpu_t *cpu, const ir_insn_t *ir,
                                                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t res = (ir->flags & IR_FLAG_WIDE)
                       ? cpu_subs64(cpu_get_xsp(cpu, ir->rn), ir->target_or_imm, &cpu->nzcv)
                       : cpu_subs32((uint32_t)cpu_get_xsp(cpu, ir->rn), (uint32_t)ir->target_or_imm, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    const ir_insn_t *br = ir + 1;
    if (cpu_cond_holds(cpu->nzcv, br->op)) {
        if (br->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = br->target_ir;
            if (target <= br && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, br->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_subs_reg_bcond(vcpu_t *cpu, const ir_insn_t *ir,
                                                 softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    bool wide = (ir->flags & IR_FLAG_WIDE) != 0;
    uint32_t sh_type = (ir->op >> 8) & 3U;
    uint32_t sh_amt = ir->op & 0x3FU;
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = apply_shift(cpu_get_x(cpu, ir->rm), sh_type, sh_amt, wide);
    uint64_t res = wide ? cpu_subs64(a, b, &cpu->nzcv) : cpu_subs32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
    cpu_set_x(cpu, ir->rd, res);
    const ir_insn_t *br = ir + 1;
    if (cpu_cond_holds(cpu->nzcv, br->op)) {
        if (br->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = br->target_ir;
            if (target <= br && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, br->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_subs_reg0_bcond(vcpu_t *cpu, const ir_insn_t *ir,
                                                  softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint64_t a = cpu_get_x(cpu, ir->rn);
    uint64_t b = cpu_get_x(cpu, ir->rm);
    uint64_t res = (ir->flags & IR_FLAG_WIDE)
                       ? cpu_subs64(a, b, &cpu->nzcv)
                       : cpu_subs32((uint32_t)a, (uint32_t)b, &cpu->nzcv);
    cpu_set_x_fast(cpu, ir->rd, res);
    const ir_insn_t *br = ir + 1;
    if (cpu_cond_holds(cpu->nzcv, br->op)) {
        if (br->flags & IR_FLAG_INPAGE) {
            const ir_insn_t *target = br->target_ir;
            if (target <= br && __builtin_expect(--cpu->backedge_budget <= 0, 0) &&
                cpu_backedge_miss(cpu, target)) {
                return;
            }
            CPU_NEXT(cpu, target, tlb_r, tlb_w);
        }
        CPU_JUMP_PC(cpu, br->target_or_imm, tlb_r, tlb_w);
        return;
    }
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_adrp_add(vcpu_t *cpu, const ir_insn_t *ir,
                                           softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    const ir_insn_t *second = ir + 1;
    uint64_t page = ir->target_or_imm;
    cpu_set_x_fast(cpu, ir->rd, page);
    cpu_set_xsp(cpu, second->rd, page + second->target_or_imm);
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_adrp_ldr64(vcpu_t *cpu, const ir_insn_t *ir,
                                             softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    const ir_insn_t *second = ir + 1;
    uint64_t page = ir->target_or_imm;
    cpu_set_x_fast(cpu, ir->rd, page);
    uint64_t val = 0;
    if (!cpu_read_64(cpu, ir, 4U, tlb_r, page + second->target_or_imm, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, second->rd, val);
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_adrp_ldr32(vcpu_t *cpu, const ir_insn_t *ir,
                                             softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    const ir_insn_t *second = ir + 1;
    uint64_t page = ir->target_or_imm;
    cpu_set_x_fast(cpu, ir->rd, page);
    uint32_t val = 0;
    if (!cpu_read_32(cpu, ir, 4U, tlb_r, page + second->target_or_imm, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, second->rd, (uint64_t)val);
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_ldr64_add64(vcpu_t *cpu, const ir_insn_t *ir,
                                              softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    const ir_insn_t *second = ir + 1;
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint64_t val = 0;
    if (!cpu_read_64(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, val);
    cpu_set_x_fast(cpu, second->rd, cpu_get_x(cpu, second->rn) + cpu_get_x(cpu, second->rm));
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static CPU_NOINLINE void op_fused_ldr32_add32(vcpu_t *cpu, const ir_insn_t *ir,
                                              softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_check_sp_align(cpu, ir->rn, cpu_ir_pc(cpu, ir))) {
        return;
    }
    const ir_insn_t *second = ir + 1;
    uint64_t va = cpu_get_xsp(cpu, ir->rn) + ir->target_or_imm;
    uint32_t val = 0;
    if (!cpu_read_32(cpu, ir, 0, tlb_r, va, &val)) {
        return;
    }
    cpu_set_x_fast(cpu, ir->rd, (uint64_t)val);
    cpu_set_x_fast(cpu, second->rd, (uint64_t)((uint32_t)cpu_get_x(cpu, second->rn) + (uint32_t)cpu_get_x(cpu, second->rm)));
    CPU_NEXT(cpu, ir + 2, tlb_r, tlb_w);
}

static void predecode_ldst_common(uint32_t word, ir_insn_t *out, uint32_t mode) {
    uint32_t size = (word >> 30) & 3U;
    bool is_simd = ((word >> 26) & 1U) != 0;
    uint32_t opc = (word >> 22) & 3U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt = word & 31U;

    if (!is_simd && size == 3U && opc == 2U && (mode == 0U || mode == 1U || mode == 4U)) {
        out->handler = op_nop;
        return;
    }

    uint32_t scale = size;
    bool is_load = false;
    uint32_t sext_mode = 0U;
    if (is_simd) {
        scale = size + (((opc >> 1) & 1U) ? 4U : 0U);
        is_load = (opc & 1U) != 0;
        if (scale > 4U) {
            out->handler = op_undef;
            return;
        }
    } else {
        if (opc == 0U) {
            is_load = false;
        } else if (opc == 1U) {
            is_load = true;
        } else if (opc == 2U) {
            is_load = true;
            sext_mode = 1U;
        } else {
            is_load = true;
            sext_mode = 2U;
        }
    }

    out->rd = (uint8_t)rt;
    out->rn = (uint8_t)rn;
    out->flags = is_simd ? IR_FLAG_SIMD : IR_FLAG_NONE;
    out->op = (uint16_t)(scale | (is_load ? 8U : 0U) | (sext_mode << 4));

    if (mode == 0U || mode == 1U) {
        out->target_or_imm = (mode == 0U)
                                 ? ((uint64_t)((word >> 10) & 0xFFFU) << scale)
                                 : (uint64_t)cpu_sext((word >> 12) & 0x1FFU, 9);
        if (!is_simd) {
            if (sext_mode == 0U) {
                if (scale == 3U) {
                    if (is_load) { out->rd = map_rd_zr(rt); out->handler = op_ldr_u64; }
                    else { out->handler = op_str_u64; }
                    return;
                }
                if (scale == 2U) {
                    if (is_load) { out->rd = map_rd_zr(rt); out->handler = op_ldr_u32; }
                    else { out->handler = op_str_u32; }
                    return;
                }
                if (scale == 1U) {
                    if (is_load) { out->rd = map_rd_zr(rt); out->handler = op_ldrh_u; }
                    else { out->handler = op_strh_u; }
                    return;
                }
                if (scale == 0U) {
                    if (is_load) { out->rd = map_rd_zr(rt); out->handler = op_ldrb_u; }
                    else { out->handler = op_strb_u; }
                    return;
                }
            } else if (is_load && sext_mode == 1U && scale == 2U) {
                out->rd = map_rd_zr(rt);
                out->handler = op_ldrsw_u;
                return;
            }
        }
        out->handler = op_ldst_single;
    } else if (mode == 2U) {
        out->simm = cpu_sext((word >> 12) & 0x1FFU, 9);
        out->flags |= IR_FLAG_POST;
        out->handler = op_ldst_single;
    } else if (mode == 3U) {
        out->simm = cpu_sext((word >> 12) & 0x1FFU, 9);
        out->flags |= IR_FLAG_PRE;
        out->handler = op_ldst_single;
    } else {
        uint32_t opt = (word >> 13) & 7U;
        uint32_t s_bit = (word >> 12) & 1U;
        if ((opt & 2U) == 0U) {
            out->handler = op_undef;
            return;
        }
        out->rm = (uint8_t)((word >> 16) & 31U);
        uint32_t sh = s_bit ? scale : 0U;
        out->op |= (uint16_t)((1U << 6) | (opt << 8) | (sh << 11));
        out->handler = op_ldst_single;
    }
}

static void predecode_pair(uint32_t word, ir_insn_t *out, uint16_t wb_flag) {
    uint32_t opc = (word >> 30) & 3U;
    bool is_simd = ((word >> 26) & 1U) != 0;
    bool is_load = ((word >> 22) & 1U) != 0;
    uint32_t scale = is_simd ? (2U + opc) : (2U + (opc >> 1));
    bool is_signed = (!is_simd && opc == 1U);
    int64_t imm7 = cpu_sext((word >> 15) & 0x7FU, 7) << scale;
    uint32_t rt1 = word & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rt2 = (word >> 10) & 31U;
    out->simm = imm7;
    out->rn = (uint8_t)rn;
    if (!is_simd && scale == 3U) {
        out->flags = wb_flag;
        if (is_load) {
            out->handler = op_ldp_64;
            out->rd = map_rd_zr(rt1);
            out->rm = map_rd_zr(rt2);
        } else {
            out->handler = op_stp_64;
            out->rd = (uint8_t)rt1;
            out->rm = (uint8_t)rt2;
        }
        return;
    }
    out->handler = op_ldst_pair;
    out->rd = (uint8_t)rt1;
    out->rm = (uint8_t)rt2;
    out->flags = (uint16_t)((is_simd ? IR_FLAG_SIMD : 0U) | wb_flag);
    out->op = (uint16_t)(scale | (is_load ? 8U : 0U) | (is_signed ? 16U : 0U));
}

static void predecode_insn(uint32_t word, uint64_t pc, ir_page_t *pg, uint32_t idx,
                           uint32_t tile, uint32_t *tile_targets) {
    ir_insn_t *out = &pg->insns[idx];
    memset(out, 0, sizeof(*out));
    a64_dec_op_t dec = a64_decode_word(word);

    uint32_t rd = word & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t ra = (word >> 10) & 31U;
    bool sf = (word >> 31) != 0;

    switch (dec) {
    case A64_DEC_UNDEF:
    case A64_DEC_PERM_UNDEF_UDF:
    case A64_DEC_EXCEPTION_SMC:
    case A64_DEC_EXCEPTION_DCPS1:
    case A64_DEC_EXCEPTION_DCPS2:
    case A64_DEC_EXCEPTION_DCPS3:
    case A64_DEC_BRANCH_REG_DRPS:
    case A64_DEC_SYSTEMINSTRS_SYSL:
        out->handler = op_undef;
        return;

    case A64_DEC_HINT_NOP:
        out->handler = op_nop;
        return;
    case A64_DEC_HINTS_WFI:
        out->handler = cpu_op_wfi;
        return;
    case A64_DEC_HINTS_WFE:
        out->handler = cpu_op_wfe;
        return;
    case A64_DEC_HINTS_SEV:
        out->handler = cpu_op_sev;
        return;
    case A64_DEC_HINTS_SEVL:
        out->handler = cpu_op_sevl;
        return;
    case A64_DEC_BARRIERS_DMB:
    case A64_DEC_BARRIERS_DSB:
    case A64_DEC_BARRIERS_ISB:
        out->handler = cpu_op_barrier;
        return;
    case A64_DEC_BARRIERS_CLREX:
        out->handler = cpu_op_clrex;
        return;

    case A64_DEC_EXCEPTION_SVC:
        out->handler = cpu_op_svc;
        out->target_or_imm = (word >> 5) & 0xFFFFU;
        return;
    case A64_DEC_EXCEPTION_HVC:
        out->handler = cpu_op_hvc;
        out->target_or_imm = (word >> 5) & 0xFFFFU;
        return;
    case A64_DEC_EXCEPTION_BRK:
        out->handler = cpu_op_brk;
        out->target_or_imm = (word >> 5) & 0xFFFFU;
        return;
    case A64_DEC_EXCEPTION_HLT:
        out->handler = cpu_op_hlt;
        out->target_or_imm = (word >> 5) & 0xFFFFU;
        return;
    case A64_DEC_BRANCH_REG_ERET:
        out->handler = cpu_op_eret;
        return;

    case A64_DEC_SYSTEMMOVE_MRS:
        out->handler = cpu_op_sysreg_mrs;
        out->target_or_imm = word;
        return;
    case A64_DEC_SYSTEMMOVE_MSR:
        out->handler = cpu_op_sysreg_msr;
        out->target_or_imm = word;
        return;
    case A64_DEC_PSTATE_MSR:
        out->handler = cpu_op_pstate_msr;
        out->target_or_imm = word;
        return;
    case A64_DEC_SYSTEMINSTRS_SYS:
        out->handler = cpu_op_sys_insn;
        out->target_or_imm = word;
        return;

    case A64_DEC_ADDSUB_IMM_ADD:
    case A64_DEC_ADDSUB_IMM_ADDS:
    case A64_DEC_ADDSUB_IMM_SUB:
    case A64_DEC_ADDSUB_IMM_SUBS: {
        uint64_t imm = (uint64_t)((word >> 10) & 0xFFFU) << (((word >> 22) & 1U) ? 12U : 0U);
        out->rn = (uint8_t)rn;
        out->target_or_imm = imm;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        if (dec == A64_DEC_ADDSUB_IMM_ADD) {
            out->rd = (uint8_t)rd;
            out->handler = sf ? op_add_imm64 : op_add_imm32;
        } else if (dec == A64_DEC_ADDSUB_IMM_SUB) {
            out->rd = (uint8_t)rd;
            out->handler = sf ? op_sub_imm64 : op_sub_imm32;
        } else if (dec == A64_DEC_ADDSUB_IMM_ADDS) {
            out->rd = map_rd_zr(rd);
            out->handler = sf ? op_adds_imm64 : op_adds_imm32;
        } else {
            out->rd = map_rd_zr(rd);
            out->handler = sf ? op_subs_imm64 : op_subs_imm32;
        }
        return;
    }

    case A64_DEC_ADDSUB_SHIFT_ADD:
    case A64_DEC_ADDSUB_SHIFT_ADDS:
    case A64_DEC_ADDSUB_SHIFT_SUB:
    case A64_DEC_ADDSUB_SHIFT_SUBS: {
        uint32_t sh_type = (word >> 22) & 3U;
        uint32_t sh_amt = (word >> 10) & 0x3FU;
        if (sh_type == 3U || (!sf && sh_amt >= 32U)) {
            out->handler = op_undef;
            return;
        }
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->op = (uint16_t)((sh_type << 8) | sh_amt);
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) |
                                ((dec == A64_DEC_ADDSUB_SHIFT_SUB || dec == A64_DEC_ADDSUB_SHIFT_SUBS) ? IR_FLAG_SUB : 0U) |
                                ((dec == A64_DEC_ADDSUB_SHIFT_ADDS || dec == A64_DEC_ADDSUB_SHIFT_SUBS) ? IR_FLAG_SETFLAGS : 0U));
        if (sh_amt == 0U && dec == A64_DEC_ADDSUB_SHIFT_ADD) {
            out->rd = map_rd_zr(rd);
            out->handler = sf ? op_add_reg64 : op_add_reg32;
        } else if (sh_amt == 0U && dec == A64_DEC_ADDSUB_SHIFT_SUB) {
            out->rd = map_rd_zr(rd);
            out->handler = sf ? op_sub_reg64 : op_sub_reg32;
        } else if (sh_amt == 0U && dec == A64_DEC_ADDSUB_SHIFT_SUBS) {
            out->rd = map_rd_zr(rd);
            out->handler = sf ? op_subs_reg64 : op_subs_reg32;
        } else {
            out->rd = (uint8_t)rd;
            out->handler = op_addsub_shift;
        }
        return;
    }

    case A64_DEC_ADDSUB_EXT_ADD:
    case A64_DEC_ADDSUB_EXT_ADDS:
    case A64_DEC_ADDSUB_EXT_SUB:
    case A64_DEC_ADDSUB_EXT_SUBS: {
        uint32_t opt = (word >> 13) & 7U;
        uint32_t imm3 = (word >> 10) & 7U;
        if (imm3 > 4U) {
            out->handler = op_undef;
            return;
        }
        out->handler = op_addsub_ext;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->op = (uint16_t)((opt << 8) | imm3);
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) |
                                ((dec == A64_DEC_ADDSUB_EXT_SUB || dec == A64_DEC_ADDSUB_EXT_SUBS) ? IR_FLAG_SUB : 0U) |
                                ((dec == A64_DEC_ADDSUB_EXT_ADDS || dec == A64_DEC_ADDSUB_EXT_SUBS) ? IR_FLAG_SETFLAGS : 0U));
        return;
    }

    case A64_DEC_ADDSUB_CARRY_ADC:
    case A64_DEC_ADDSUB_CARRY_ADCS:
    case A64_DEC_ADDSUB_CARRY_SBC:
    case A64_DEC_ADDSUB_CARRY_SBCS:
        out->handler = op_addsub_carry;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) |
                                ((dec == A64_DEC_ADDSUB_CARRY_SBC || dec == A64_DEC_ADDSUB_CARRY_SBCS) ? IR_FLAG_SUB : 0U) |
                                ((dec == A64_DEC_ADDSUB_CARRY_ADCS || dec == A64_DEC_ADDSUB_CARRY_SBCS) ? IR_FLAG_SETFLAGS : 0U));
        return;

    case A64_DEC_LOG_IMM_AND:
    case A64_DEC_LOG_IMM_ORR:
    case A64_DEC_LOG_IMM_EOR:
    case A64_DEC_LOG_IMM_ANDS: {
        uint32_t n = (word >> 22) & 1U;
        uint32_t immr = (word >> 16) & 0x3FU;
        uint32_t imms = (word >> 10) & 0x3FU;
        uint64_t imm = 0;
        if ((!sf && n != 0U) || !vcpu_decode_bitmask(n, imms, immr, sf, true, &imm, nullptr)) {
            out->handler = op_undef;
            return;
        }
        out->handler = op_log_imm;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->target_or_imm = imm;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->op = (uint16_t)((word >> 29) & 3U);
        return;
    }

    case A64_DEC_LOG_SHIFT_AND:
    case A64_DEC_LOG_SHIFT_BIC:
    case A64_DEC_LOG_SHIFT_ORR:
    case A64_DEC_LOG_SHIFT_ORN:
    case A64_DEC_LOG_SHIFT_EOR:
    case A64_DEC_LOG_SHIFT_EON:
    case A64_DEC_LOG_SHIFT_ANDS:
    case A64_DEC_LOG_SHIFT_BICS: {
        uint32_t sh_type = (word >> 22) & 3U;
        uint32_t sh_amt = (word >> 10) & 0x3FU;
        bool inv = ((word >> 21) & 1U) != 0;
        uint32_t opc = (word >> 29) & 3U;
        if (!sf && sh_amt >= 32U) {
            out->handler = op_undef;
            return;
        }
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        if (!inv && sh_amt == 0U) {
            out->rd = map_rd_zr(rd);
            if (opc == 1U && rn == 31U) {
                out->handler = sf ? op_mov_reg64 : op_mov_reg32;
                return;
            }
            if (opc == 0U) { out->handler = sf ? op_and_reg64 : op_and_reg32; return; }
            if (opc == 1U) { out->handler = sf ? op_orr_reg64 : op_orr_reg32; return; }
            if (opc == 2U) { out->handler = sf ? op_eor_reg64 : op_eor_reg32; return; }
            out->handler = sf ? op_ands_reg64 : op_ands_reg32;
            return;
        }
        out->rd = (uint8_t)rd;
        out->handler = op_log_shift;
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) | (inv ? IR_FLAG_INV : 0U));
        out->op = (uint16_t)((opc << 10) | (sh_type << 8) | sh_amt);
        return;
    }

    case A64_DEC_MOVEWIDE_MOVZ:
    case A64_DEC_MOVEWIDE_MOVN:
    case A64_DEC_MOVEWIDE_MOVK: {
        uint32_t hw = (word >> 21) & 3U;
        uint64_t imm16 = (word >> 5) & 0xFFFFU;
        if (!sf && hw >= 2U) {
            out->handler = op_undef;
            return;
        }
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        if (dec == A64_DEC_MOVEWIDE_MOVK) {
            out->rd = (uint8_t)rd;
            out->handler = op_movk;
            out->target_or_imm = imm16;
            out->op = (uint16_t)(hw * 16U);
        } else {
            uint64_t v = imm16 << (hw * 16U);
            if (dec == A64_DEC_MOVEWIDE_MOVN) {
                v = sf ? ~v : (uint64_t)(uint32_t)~v;
            }
            out->rd = map_rd_zr(rd);
            out->handler = op_mov_imm;
            out->target_or_imm = v;
        }
        return;
    }

    case A64_DEC_BITFIELD_SBFM:
    case A64_DEC_BITFIELD_BFM:
    case A64_DEC_BITFIELD_UBFM: {
        uint32_t n = (word >> 22) & 1U;
        uint32_t immr = (word >> 16) & 0x3FU;
        uint32_t imms = (word >> 10) & 0x3FU;
        if (n != (sf ? 1U : 0U) || (!sf && (immr >= 32U || imms >= 32U))) {
            out->handler = op_undef;
            return;
        }
        uint32_t bits = sf ? 64U : 32U;
        out->rn = (uint8_t)rn;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        if (dec == A64_DEC_BITFIELD_UBFM) {
            if (imms == bits - 1U) {
                out->rd = map_rd_zr(rd);
                out->op = (uint16_t)immr;
                out->handler = sf ? op_lsr_imm64 : op_lsr_imm32;
                return;
            }
            if (imms + 1U == immr) {
                out->rd = map_rd_zr(rd);
                out->op = (uint16_t)(bits - immr);
                out->handler = sf ? op_lsl_imm64 : op_lsl_imm32;
                return;
            }
            if (imms >= immr) {
                uint32_t width = imms - immr + 1U;
                out->rd = map_rd_zr(rd);
                out->op = (uint16_t)immr;
                out->target_or_imm = (width == 64U) ? UINT64_MAX : ((1ULL << width) - 1ULL);
                out->handler = sf ? op_ubfx64 : op_ubfx32;
                return;
            }
        } else if (dec == A64_DEC_BITFIELD_SBFM) {
            if (imms == bits - 1U) {
                out->rd = map_rd_zr(rd);
                out->op = (uint16_t)immr;
                out->handler = sf ? op_asr_imm64 : op_asr_imm32;
                return;
            }
            if (imms >= immr) {
                uint32_t width = imms - immr + 1U;
                uint32_t lsh = (bits - 1U) - imms;
                uint32_t rsh = bits - width;
                out->rd = map_rd_zr(rd);
                out->op = (uint16_t)((lsh << 8) | rsh);
                out->handler = sf ? op_sbfx64 : op_sbfx32;
                return;
            }
        }
        uint32_t opc = (word >> 29) & 3U;
        out->handler = op_bitfield;
        out->rd = (uint8_t)rd;
        out->op = (uint16_t)((opc << 12) | (immr << 6) | imms);
        return;
    }

    case A64_DEC_EXTRACT_EXTR: {
        uint32_t n = (word >> 22) & 1U;
        uint32_t imms = (word >> 10) & 0x3FU;
        if (n != (sf ? 1U : 0U) || (!sf && imms >= 32U)) {
            out->handler = op_undef;
            return;
        }
        out->handler = op_extr;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->op = (uint16_t)imms;
        return;
    }

    case A64_DEC_PCRELADDR_ADR:
    case A64_DEC_PCRELADDR_ADRP: {
        uint64_t imm21 = (((word >> 5) & 0x7FFFFULL) << 2) | ((word >> 29) & 3ULL);
        int64_t simm = cpu_sext(imm21, 21);
        out->handler = op_mov_imm;
        out->rd = map_rd_zr(rd);
        out->flags = (dec == A64_DEC_PCRELADDR_ADRP) ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->target_or_imm = (dec == A64_DEC_PCRELADDR_ADRP)
                                 ? ((pc & ~4095ULL) + (uint64_t)(simm << 12))
                                 : (pc + (uint64_t)simm);
        return;
    }

    case A64_DEC_CONDCMP_REG_CCMN:
    case A64_DEC_CONDCMP_REG_CCMP:
    case A64_DEC_CONDCMP_IMM_CCMN:
    case A64_DEC_CONDCMP_IMM_CCMP: {
        bool is_sub = (dec == A64_DEC_CONDCMP_REG_CCMP || dec == A64_DEC_CONDCMP_IMM_CCMP);
        bool is_imm = (dec == A64_DEC_CONDCMP_IMM_CCMN || dec == A64_DEC_CONDCMP_IMM_CCMP);
        out->handler = op_condcmp;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->target_or_imm = rm;
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) | (is_sub ? IR_FLAG_SUB : 0U) | (is_imm ? IR_FLAG_INV : 0U));
        out->op = (uint16_t)(((word >> 12) & 15U) | ((word & 15U) << 4));
        return;
    }

    case A64_DEC_CONDSEL_CSEL:
    case A64_DEC_CONDSEL_CSINC:
    case A64_DEC_CONDSEL_CSINV:
    case A64_DEC_CONDSEL_CSNEG: {
        uint32_t kind = (((word >> 30) & 1U) << 1) | ((word >> 10) & 1U);
        out->handler = op_condsel;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->op = (uint16_t)(((word >> 12) & 15U) | (kind << 4));
        return;
    }

    case A64_DEC_DP_1SRC_RBIT:
    case A64_DEC_DP_1SRC_REV16:
    case A64_DEC_DP_1SRC_REV32:
    case A64_DEC_DP_1SRC_REV:
    case A64_DEC_DP_1SRC_CLZ:
    case A64_DEC_DP_1SRC_CLS:
        out->handler = op_dp_1src;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->op = (uint16_t)((word >> 10) & 7U);
        return;

    case A64_DEC_DP_2SRC_UDIV:
    case A64_DEC_DP_2SRC_SDIV:
    case A64_DEC_DP_2SRC_LSLV:
    case A64_DEC_DP_2SRC_LSRV:
    case A64_DEC_DP_2SRC_ASRV:
    case A64_DEC_DP_2SRC_RORV: {
        uint32_t sub = 0;
        if (dec == A64_DEC_DP_2SRC_UDIV) sub = 0U;
        else if (dec == A64_DEC_DP_2SRC_SDIV) sub = 1U;
        else if (dec == A64_DEC_DP_2SRC_LSLV) sub = 2U;
        else if (dec == A64_DEC_DP_2SRC_LSRV) sub = 3U;
        else if (dec == A64_DEC_DP_2SRC_ASRV) sub = 4U;
        else sub = 5U;
        out->handler = op_dp_2src;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->flags = sf ? IR_FLAG_WIDE : IR_FLAG_NONE;
        out->op = (uint16_t)sub;
        return;
    }

    case A64_DEC_DP_2SRC_CRC32B:  out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32B; return;
    case A64_DEC_DP_2SRC_CRC32H:  out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32H; return;
    case A64_DEC_DP_2SRC_CRC32W:  out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32W; return;
    case A64_DEC_DP_2SRC_CRC32X:  out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32X; return;
    case A64_DEC_DP_2SRC_CRC32CB: out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32CB; return;
    case A64_DEC_DP_2SRC_CRC32CH: out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32CH; return;
    case A64_DEC_DP_2SRC_CRC32CW: out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32CW; return;
    case A64_DEC_DP_2SRC_CRC32CX: out->handler = cpu_op_crc32; out->rd = (uint8_t)rd; out->rn = (uint8_t)rn; out->rm = (uint8_t)rm; out->op = SUBOP_CRC32CX; return;

    case A64_DEC_DP_3SRC_MADD:
    case A64_DEC_DP_3SRC_MSUB:
    case A64_DEC_DP_3SRC_SMADDL:
    case A64_DEC_DP_3SRC_SMSUBL:
    case A64_DEC_DP_3SRC_UMADDL:
    case A64_DEC_DP_3SRC_UMSUBL:
    case A64_DEC_DP_3SRC_SMULH:
    case A64_DEC_DP_3SRC_UMULH: {
        uint16_t sub = 0;
        if (dec == A64_DEC_DP_3SRC_MADD) sub = sf ? 2U : 0U;
        else if (dec == A64_DEC_DP_3SRC_MSUB) sub = sf ? 3U : 1U;
        else if (dec == A64_DEC_DP_3SRC_SMADDL) sub = 4U;
        else if (dec == A64_DEC_DP_3SRC_SMSUBL) sub = 5U;
        else if (dec == A64_DEC_DP_3SRC_UMADDL) sub = 6U;
        else if (dec == A64_DEC_DP_3SRC_UMSUBL) sub = 7U;
        else if (dec == A64_DEC_DP_3SRC_SMULH) sub = 8U;
        else sub = 9U;
        out->handler = op_dp_3src;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->ra = (uint8_t)ra;
        out->op = sub;
        return;
    }

    case A64_DEC_BRANCH_IMM_B:
    case A64_DEC_BRANCH_IMM_BL: {
        int64_t off = cpu_sext(word & 0x03FFFFFFU, 26) * 4;
        uint64_t target = pc + (uint64_t)off;
        out->handler = (dec == A64_DEC_BRANCH_IMM_BL) ? op_bl : op_b;
        if ((target & ~(uint64_t)VMM_PAGE_MASK) == pg->va_page) {
            uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
            uint32_t ttile = tidx >> TILE_SHIFT;
            if (ttile == tile || (pg->decoded_mask & (1U << ttile)) != 0U) {
                out->target_ir = &pg->insns[tidx];
                out->flags = IR_FLAG_INPAGE;
                if (ttile == tile) {
                    *tile_targets |= 1U << (tidx & (TILE_INSNS - 1U));
                }
            } else {
                out->target_or_imm = target;
            }
        } else {
            out->target_or_imm = target;
        }
        if (dec == A64_DEC_BRANCH_IMM_BL && ((idx + 1U) & (TILE_INSNS - 1U)) != 0U) {
            *tile_targets |= 1U << ((idx + 1U) & (TILE_INSNS - 1U));
        }
        return;
    }

    case A64_DEC_B_COND: {
        int64_t off = cpu_sext((word >> 5) & 0x7FFFFU, 19) * 4;
        uint64_t target = pc + (uint64_t)off;
        out->handler = op_b_cond;
        out->op = (uint16_t)(word & 15U);
        if ((target & ~(uint64_t)VMM_PAGE_MASK) == pg->va_page) {
            uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
            uint32_t ttile = tidx >> TILE_SHIFT;
            if (ttile == tile || (pg->decoded_mask & (1U << ttile)) != 0U) {
                out->target_ir = &pg->insns[tidx];
                out->flags = IR_FLAG_INPAGE;
                if (ttile == tile) {
                    *tile_targets |= 1U << (tidx & (TILE_INSNS - 1U));
                }
            } else {
                out->target_or_imm = target;
            }
        } else {
            out->target_or_imm = target;
        }
        return;
    }

    case A64_DEC_COMPBRANCH_CBZ:
    case A64_DEC_COMPBRANCH_CBNZ: {
        int64_t off = cpu_sext((word >> 5) & 0x7FFFFU, 19) * 4;
        uint64_t target = pc + (uint64_t)off;
        out->handler = op_cbz;
        out->rd = (uint8_t)rd;
        out->flags = (uint16_t)((sf ? IR_FLAG_WIDE : 0U) | (dec == A64_DEC_COMPBRANCH_CBNZ ? IR_FLAG_INV : 0U));
        if ((target & ~(uint64_t)VMM_PAGE_MASK) == pg->va_page) {
            uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
            uint32_t ttile = tidx >> TILE_SHIFT;
            if (ttile == tile || (pg->decoded_mask & (1U << ttile)) != 0U) {
                out->target_ir = &pg->insns[tidx];
                out->flags |= IR_FLAG_INPAGE;
                if (ttile == tile) {
                    *tile_targets |= 1U << (tidx & (TILE_INSNS - 1U));
                }
            } else {
                out->target_or_imm = target;
            }
        } else {
            out->target_or_imm = target;
        }
        return;
    }

    case A64_DEC_TESTBRANCH_TBZ:
    case A64_DEC_TESTBRANCH_TBNZ: {
        int64_t off = cpu_sext((word >> 5) & 0x3FFFU, 14) * 4;
        uint64_t target = pc + (uint64_t)off;
        uint32_t bit = (((word >> 31) & 1U) << 5) | ((word >> 19) & 0x1FU);
        out->handler = op_tbz;
        out->rd = (uint8_t)rd;
        out->op = (uint16_t)bit;
        out->flags = (dec == A64_DEC_TESTBRANCH_TBNZ) ? IR_FLAG_INV : IR_FLAG_NONE;
        if ((target & ~(uint64_t)VMM_PAGE_MASK) == pg->va_page) {
            uint32_t tidx = (uint32_t)((target & VMM_PAGE_MASK) >> 2);
            uint32_t ttile = tidx >> TILE_SHIFT;
            if (ttile == tile || (pg->decoded_mask & (1U << ttile)) != 0U) {
                out->target_ir = &pg->insns[tidx];
                out->flags |= IR_FLAG_INPAGE;
                if (ttile == tile) {
                    *tile_targets |= 1U << (tidx & (TILE_INSNS - 1U));
                }
            } else {
                out->target_or_imm = target;
            }
        } else {
            out->target_or_imm = target;
        }
        return;
    }

    case A64_DEC_BRANCH_REG_RET:
        out->handler = op_ret;
        out->rn = (uint8_t)rn;
        return;

    case A64_DEC_BRANCH_REG_BR:
    case A64_DEC_BRANCH_REG_BLR:
        out->handler = op_branch_reg;
        out->rn = (uint8_t)rn;
        out->op = (dec == A64_DEC_BRANCH_REG_BLR) ? 1U : 0U;
        if (dec == A64_DEC_BRANCH_REG_BLR && ((idx + 1U) & (TILE_INSNS - 1U)) != 0U) {
            *tile_targets |= 1U << ((idx + 1U) & (TILE_INSNS - 1U));
        }
        return;

    case A64_DEC_LDST_POS:      predecode_ldst_common(word, out, 0U); return;
    case A64_DEC_LDST_UNSCALED: predecode_ldst_common(word, out, 1U); return;
    case A64_DEC_LDST_IMMPOST:  predecode_ldst_common(word, out, 2U); return;
    case A64_DEC_LDST_IMMPRE:   predecode_ldst_common(word, out, 3U); return;
    case A64_DEC_LDST_REGOFF:   predecode_ldst_common(word, out, 4U); return;
    case A64_DEC_LDST_PAIR_OFF:
    case A64_DEC_LDST_NAPAIR:   predecode_pair(word, out, IR_FLAG_NONE); return;
    case A64_DEC_LDST_PAIR_POST: predecode_pair(word, out, IR_FLAG_POST); return;
    case A64_DEC_LDST_PAIR_PRE:  predecode_pair(word, out, IR_FLAG_PRE); return;

    case A64_DEC_LOADLIT: {
        uint32_t opc = (word >> 30) & 3U;
        bool is_simd = ((word >> 26) & 1U) != 0;
        if (!is_simd && opc == 3U) {
            out->handler = op_nop;
            return;
        }
        int64_t off = cpu_sext((word >> 5) & 0x7FFFFU, 19) * 4;
        out->handler = op_ldr_lit;
        out->rd = (uint8_t)rd;
        out->op = (uint16_t)opc;
        out->flags = is_simd ? IR_FLAG_SIMD : IR_FLAG_NONE;
        out->target_or_imm = pc + (uint64_t)off;
        return;
    }

    case A64_DEC_LDST_UNPRIV:
        out->handler = cpu_op_ldst_unpriv;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->op = (uint16_t)(((word >> 30) & 3U) | (((word >> 22) & 3U) << 2));
        out->simm = cpu_sext((word >> 12) & 0x1FFU, 9);
        return;
    case A64_DEC_LDST_ORD:     out->handler = cpu_op_ldst_ord; out->target_or_imm = word; return;
    case A64_DEC_LDST_EXCLR:   out->handler = cpu_op_ldst_exclr; out->target_or_imm = word; return;
    case A64_DEC_LDST_EXCLP:   out->handler = cpu_op_ldst_exclp; out->target_or_imm = word; return;
    case A64_DEC_LSE_CAS:      out->handler = cpu_op_lse_cas; out->target_or_imm = word; return;
    case A64_DEC_LSE_CASP:     out->handler = cpu_op_lse_casp; out->target_or_imm = word; return;
    case A64_DEC_LSE_MEMOP:    out->handler = cpu_op_lse_memop; out->target_or_imm = word; return;
    case A64_DEC_MOPS_CPY: {
        if (rd == 31U || rn == 31U || rm == 31U || rd == rn || rd == rm || rn == rm) {
            out->handler = op_undef;
            return;
        }
        out->handler = cpu_op_mops_cpy;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->op = (uint16_t)(((word >> 22) & 3U) | (((word >> 26) & 1U) << 2) |
                             (((word >> 12) & 15U) << 4));
        out->target_or_imm = word;
        return;
    }
    case A64_DEC_MOPS_SET: {
        if (rd == 31U || rn == 31U || rd == rn || rd == rm || rn == rm) {
            out->handler = op_undef;
            return;
        }
        out->handler = cpu_op_mops_set;
        out->rd = (uint8_t)rd;
        out->rn = (uint8_t)rn;
        out->rm = (uint8_t)rm;
        out->op = (uint16_t)(((word >> 14) & 3U) | (((word >> 12) & 3U) << 4));
        out->target_or_imm = word;
        return;
    }
    case A64_DEC_ASISDLSE:
    case A64_DEC_ASISDLSEP:    out->handler = cpu_op_simd_ldst_multi; out->target_or_imm = word; return;
    case A64_DEC_ASISDLSO:
    case A64_DEC_ASISDLSOP:    out->handler = cpu_op_simd_ldst_single; out->target_or_imm = word; return;

    case A64_DEC_FLOATDP1:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_DP1; out->target_or_imm = word; return;
    case A64_DEC_FLOATDP2:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_DP2; out->target_or_imm = word; return;
    case A64_DEC_FLOATDP3:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_DP3; out->target_or_imm = word; return;
    case A64_DEC_FLOATCMP:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_CMP; out->target_or_imm = word; return;
    case A64_DEC_FLOATCCMP:    out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_CCMP; out->target_or_imm = word; return;
    case A64_DEC_FLOATSEL:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_SEL; out->target_or_imm = word; return;
    case A64_DEC_FLOATIMM:     out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_IMM; out->target_or_imm = word; return;
    case A64_DEC_FLOAT2INT:    out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_2INT; out->target_or_imm = word; return;
    case A64_DEC_FLOAT2FIX:    out->handler = cpu_op_fp_scalar; out->op = SUBOP_FP_2FIX; out->target_or_imm = word; return;

    case A64_DEC_CRYPTOAES:      out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_AES; out->target_or_imm = word; return;
    case A64_DEC_CRYPTOSHA2:     out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_SHA2; out->target_or_imm = word; return;
    case A64_DEC_CRYPTOSHA3:     out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_SHA3; out->target_or_imm = word; return;
    case A64_DEC_CRYPTO3_IMM6:   out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_IMM6; out->target_or_imm = word; return;
    case A64_DEC_CRYPTO4:        out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_4; out->target_or_imm = word; return;
    case A64_DEC_CRYPTOSHA512_2: out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_SHA512_2; out->target_or_imm = word; return;
    case A64_DEC_CRYPTOSHA512_3: out->handler = cpu_op_crypto; out->op = SUBOP_CRYPTO_SHA512_3; out->target_or_imm = word; return;

    case A64_DEC_ASIMDIMM:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_IMM; out->target_or_imm = word; return;
    case A64_DEC_ASIMDINS:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_INS; out->target_or_imm = word; return;
    case A64_DEC_ASISDONE:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_ONE; out->target_or_imm = word; return;
    case A64_DEC_ASIMDEXT:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_EXT; out->target_or_imm = word; return;
    case A64_DEC_ASIMDPERM:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_PERM; out->target_or_imm = word; return;
    case A64_DEC_ASIMDTBL:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_TBL; out->target_or_imm = word; return;
    case A64_DEC_ASIMDSAME:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_SAME; out->target_or_imm = word; return;
    case A64_DEC_ASIMDSAMEFP16:  out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_SAME_FP16; out->target_or_imm = word; return;
    case A64_DEC_ASIMDSAME2:     out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_SAME2; out->target_or_imm = word; return;
    case A64_DEC_ASIMDMISC:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_MISC; out->target_or_imm = word; return;
    case A64_DEC_ASIMDMISCFP16:  out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_MISC_FP16; out->target_or_imm = word; return;
    case A64_DEC_ASIMDSHF:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_SHF; out->target_or_imm = word; return;
    case A64_DEC_ASIMDDIFF:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_DIFF; out->target_or_imm = word; return;
    case A64_DEC_ASIMDELEM:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_ELEM; out->target_or_imm = word; return;
    case A64_DEC_ASIMDALL:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SIMD_ALL; out->target_or_imm = word; return;
    case A64_DEC_ASISDSAME:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_SAME; out->target_or_imm = word; return;
    case A64_DEC_ASISDSAMEFP16:  out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_SAME_FP16; out->target_or_imm = word; return;
    case A64_DEC_ASISDSAME2:     out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_SAME2; out->target_or_imm = word; return;
    case A64_DEC_ASISDMISC:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_MISC; out->target_or_imm = word; return;
    case A64_DEC_ASISDMISCFP16:  out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_MISC_FP16; out->target_or_imm = word; return;
    case A64_DEC_ASISDSHF:       out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_SHF; out->target_or_imm = word; return;
    case A64_DEC_ASISDDIFF:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_DIFF; out->target_or_imm = word; return;
    case A64_DEC_ASISDELEM:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_ELEM; out->target_or_imm = word; return;
    case A64_DEC_ASISDPAIR:      out->handler = cpu_op_simd_dp; out->op = SUBOP_SISD_PAIR; out->target_or_imm = word; return;

    case A64_DEC_COUNT:
        out->handler = op_undef;
        return;
    }
}

static void fuse_tile(ir_page_t *pg, uint32_t base, uint32_t tile_targets) {
    for (uint32_t j = 0; j + 1U < TILE_INSNS; ++j) {
        if ((tile_targets & (1U << (j + 1U))) != 0U) {
            continue;
        }
        uint32_t i = base + j;
        ir_insn_t *a = &pg->insns[i];
        ir_insn_t *b = &pg->insns[i + 1U];
        if ((a->handler == op_subs_imm64 || a->handler == op_subs_imm32) && b->handler == op_b_cond) {
            a->handler = op_fused_subs_imm_bcond;
            ++j;
        } else if ((a->handler == op_subs_reg64 || a->handler == op_subs_reg32) && b->handler == op_b_cond) {
            a->handler = op_fused_subs_reg0_bcond;
            ++j;
        } else if (a->handler == op_addsub_shift &&
                   (a->flags & (IR_FLAG_SUB | IR_FLAG_SETFLAGS)) == (IR_FLAG_SUB | IR_FLAG_SETFLAGS) &&
                   b->handler == op_b_cond) {
            a->handler = op_fused_subs_reg_bcond;
            ++j;
        } else if (a->handler == op_mov_imm && (a->flags & IR_FLAG_WIDE) != 0 && a->rd < 31U && a->rd == b->rn) {
            if (b->handler == op_add_imm64) {
                a->handler = op_fused_adrp_add;
                ++j;
            } else if (b->handler == op_ldr_u64) {
                a->handler = op_fused_adrp_ldr64;
                ++j;
            } else if (b->handler == op_ldr_u32) {
                a->handler = op_fused_adrp_ldr32;
                ++j;
            }
        } else if (a->handler == op_ldr_u64 && b->handler == op_add_reg64) {
            a->handler = op_fused_ldr64_add64;
            ++j;
        } else if (a->handler == op_ldr_u32 && b->handler == op_add_reg32) {
            a->handler = op_fused_ldr32_add32;
            ++j;
        }
    }
}

#include "cpu_fast_decode.inc"

CPU_COLD_MISS static void decode_tile_slow(ir_page_t *pg, uint32_t tile) {
    uint32_t base = tile << TILE_SHIFT;
    uint32_t tile_targets = 0;
    const uint32_t *words = (const uint32_t *)pg->hva_page;
    for (uint32_t i = 0; i < TILE_INSNS; ++i) {
        uint32_t word = __atomic_load_n(words + base + i, __ATOMIC_RELAXED);
        predecode_insn(word, pg->va_page + (uint64_t)(base + i) * 4ULL, pg, base + i, tile, &tile_targets);
    }
    pg->decoded_mask |= (1U << tile);
#ifdef CPU_STATS
    atomic_fetch_add_explicit(&cpu_stats_tiles, 1, memory_order_relaxed);
#endif
    if (tile + 1U < (IR_PAGE_INSNS >> TILE_SHIFT) &&
        (pg->decoded_mask & (1U << (tile + 1U))) == 0U) {
        pg->insns[base + TILE_INSNS].handler = op_tile_miss;
        pg->insns[base + TILE_INSNS].fast_offset = cpu_fast_offsets[FAST_tile_miss];
    }
    fuse_tile(pg, base, tile_targets);
    for (uint32_t i = 0; i < TILE_INSNS; ++i) {
        cpu_fast_decode(&pg->insns[base + i]);
    }
}

void cpu_fast_decode_tile(ir_page_t *pg, uint32_t tile) {
    decode_tile_slow(pg, tile);
}

static ir_page_t *icache_get_page(vcpu_t *cpu, uint64_t va_page, uint64_t pa_page, uintptr_t hva_page) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    uint32_t set = (uint32_t)((pa_page >> VMM_PAGE_SHIFT) & ic->set_mask);
    for (uint32_t w = 0; w < ICACHE_WAYS; ++w) {
        ir_page_t *pg = ic->slots[set][w];
        if (pg && pg->pa_page == pa_page && pg->va_page == va_page && pg->el == cpu->el) {
            ic->lru[set] = w ^ 1U;
            return pg;
        }
    }
    uint64_t mask;
    _Atomic uint64_t *word = cpu_icache_ram_word(cpu->vm, pa_page, &mask);
    if (word) {
        atomic_fetch_or_explicit(word, mask, memory_order_seq_cst);
        atomic_thread_fence(memory_order_seq_cst);
    }
    uint32_t victim = ic->lru[set] & 1U;
    ic->lru[set] = victim ^ 1U;
    ir_page_t *pg = ic->slots[set][victim];
    if (pg) {
        CPU_STAT(cpu, pages_replaced);
    }
    if (!pg) {
        pg = calloc(1, sizeof(ir_page_t));
        ic->slots[set][victim] = pg;
    }
    pg->pa_page = pa_page;
    pg->va_page = va_page;
    pg->hva_page = hva_page;
    pg->decoded_mask = 0;
    pg->gen = ++ic->next_gen;
    pg->el = cpu->el;
    pg->insns[IR_PAGE_INSNS].handler = op_page_end;
    pg->insns[IR_PAGE_INSNS].fast_offset = cpu_fast_offsets[FAST_page_end];
    return pg;
}

const ir_insn_t *cpu_fast_resolve(vcpu_t *cpu) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;

    while (!atomic_load_explicit(&cpu->stop_requested, memory_order_acquire)) {
        mmu_poll_shootdown(cpu);

        if ((cpu->daif & (1U << 7)) == 0 &&
            atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) != 0) {
            vcpu_take_irq(cpu);
        }

        if (__builtin_expect((cpu->pc & 3ULL) != 0, 0)) {
            vcpu_raise_exception(cpu, (0x22U << 26) | (1U << 25), cpu->pc, cpu->pc);
            continue;
        }

        uint64_t pc = cpu->pc;
        uint64_t va_page = pc & ~(uint64_t)VMM_PAGE_MASK;
        uint64_t asid_salt = cpu->tlb_asid_salt;
        uint64_t top = pc >> 48;
        uint64_t mmu_off = (asid_salt & (1ULL << 10)) >> 8;
        uint64_t page = pc & 0x0000fffffffff000ULL;
        ir_page_t *pg = NULL;
        uint32_t ec_idx = 0;
        uint64_t ec_tag = 0;
        bool ec_eligible = false;

        if (__builtin_expect(top == 0ULL, 1)) {
            ec_idx = (uint32_t)(((pc >> VMM_PAGE_SHIFT) ^ (uint32_t)asid_salt) & EXEC_CACHE_MASK);
            ec_tag = page | (asid_salt & 0xffff000000000000ULL) | mmu_off;
            ec_eligible = true;
        } else if (__builtin_expect(top == 0xffffULL, 1)) {
            ec_idx = (uint32_t)((pc >> VMM_PAGE_SHIFT) & EXEC_CACHE_MASK);
            ec_tag = page | 0x2ULL | mmu_off;
            ec_eligible = true;
        }

        if (ec_eligible) {
            const exec_cache_entry_t *e = &ic->exec_cache[cpu->el][ec_idx];
            if (__builtin_expect(e->tag == ec_tag && e->exec_epoch == cpu->exec_epoch &&
                                 e->pg != NULL && e->pg->gen == e->pg_gen, 1)) {
                pg = e->pg;
            }
        }

        CPU_STAT(cpu, resolves);
        if (__builtin_expect(pg == NULL, 0)) {
            mmu_translation_t tr;
            bool cacheable = false;
            CPU_STAT(cpu, resolve_misses);
            if (!mmu_translate_exec(cpu, va_page, cpu->el, &tr, &cacheable)) {
                uint32_t ec = cpu->el ? 0x21U : 0x20U;
                uint32_t fsc = tr.ok ? 0x10U : (tr.fault_fsc & 0x3FU);
                vcpu_raise_exception(cpu, (ec << 26) | (1U << 25) | fsc, pc, pc);
                continue;
            }
            pg = icache_get_page(cpu, va_page, tr.pa & ~(uint64_t)VMM_PAGE_MASK, tr.hva);
            if (ec_eligible && cacheable) {
                exec_cache_entry_t *e = &ic->exec_cache[cpu->el][ec_idx];
                e->tag = ec_tag;
                e->exec_epoch = cpu->exec_epoch;
                e->pg = pg;
                e->pg_gen = pg->gen;
            }
        }

        uint32_t tidx = (uint32_t)((pc & VMM_PAGE_MASK) >> 2);
        ic->cur_page = pg;
        ic->cur_exec_epoch = cpu->exec_epoch;

        uint32_t tile = tidx >> TILE_SHIFT;
        if (__builtin_expect((pg->decoded_mask & (1U << tile)) == 0U, 0)) {
            decode_tile_slow(pg, tile);
        }

        return &pg->insns[tidx];
    }

    return nullptr;
}

const ir_insn_t *cpu_fast_call(vcpu_t *cpu, const ir_insn_t *ir,
                               softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    cpu->fast_next = nullptr;
    CPU_STATS_COLD(cpu, ir);
    ir->handler(cpu, ir, tlb_r, tlb_w);
    return cpu->fast_next;
}

bool cpu_fast_poll(vcpu_t *cpu, const ir_insn_t *target) {
    return cpu_backedge_miss(cpu, target);
}

vcpu_stop_reason_t vcpu_run(vcpu_t *cpu) {
    cpu_fast_run(cpu);
    cpu_fp_leave_guest(cpu);
    return cpu->stop_reason != VCPU_STOP_NONE ? cpu->stop_reason : VCPU_STOP_SUSPEND;
}
