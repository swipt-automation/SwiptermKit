#include "cpu_internal.h"

#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>

static ir_page_t *pool_page(const cpu_pool_t *pool, uint32_t slot) {
    return (ir_page_t *)(pool->base + ((size_t)slot << POOL_SLOT_SHIFT));
}

static uint32_t pool_slot(const cpu_pool_t *pool, const ir_page_t *pg) {
    return (uint32_t)(((const uint8_t *)pg - pool->base) >> POOL_SLOT_SHIFT);
}

static uint32_t *pool_head(cpu_pool_t *pool, uint64_t pa_page) {
    return &pool->heads[cpu_pa_bucket(pa_page, POOL_BUCKETS)];
}

static cpu_pool_t *pool_create(vmm_machine_t *vm) {
    cpu_pool_t *pool = NULL;
    if (posix_memalign((void **)&pool, 128, sizeof(*pool)) != 0) {
        abort();
    }
    memset(pool, 0, sizeof(*pool));
    pool->vm = vm;
    pool->map_bytes = (size_t)(POOL_SLOTS + 1U) << POOL_SLOT_SHIFT;
    pool->map = mmap(NULL, pool->map_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (pool->map == MAP_FAILED) {
        abort();
    }
    pool->base = (uint8_t *)(((uintptr_t)pool->map + POOL_SLOT_BYTES - 1U) & ~(uintptr_t)(POOL_SLOT_BYTES - 1U));
    for (uint32_t i = 0; i < POOL_SLOTS; ++i) {
        pool->slot_link[i] = i + 1U < POOL_SLOTS ? i + 1U : POOL_NONE;
    }
    pool->retire_head = POOL_NONE;
    pool->retire_tail = POOL_NONE;
    for (uint32_t i = 0; i < POOL_BUCKETS; ++i) {
        pool->heads[i] = POOL_NONE;
    }
    pool->pfn_count = vm->ram_size >> VMM_PAGE_SHIFT;
    pool->pfn_gen = calloc(pool->pfn_count + 1U, sizeof(*pool->pfn_gen));
    atomic_store_explicit(&pool->epoch, 1, memory_order_relaxed);
    atomic_store_explicit(&pool->jit.pend_epoch, UINT64_MAX, memory_order_relaxed);
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; ++i) {
        atomic_store_explicit(&pool->qs[i].parked, true, memory_order_relaxed);
    }
    return pool;
}

void cpu_pool_attach(vcpu_t *cpu) {
    vmm_machine_t *vm = cpu->vm;
    if (vm->cpu_pool == NULL) {
        vm->cpu_pool = pool_create(vm);
    }
    ((cpu_icache_t *)cpu->icache)->pool = vm->cpu_pool;
    atomic_store_explicit(&vm->cpu_pool->qs[cpu->cpu_id].parked, true, memory_order_seq_cst);
}

void cpu_pool_destroy(vmm_machine_t *vm) {
    cpu_pool_t *pool = vm->cpu_pool;
    if (pool == NULL) {
        return;
    }
    if (pool->jit.active) {
        cpu_jit_arena_release(vm);
    }
    free(pool->jit.seg);
    free(pool->jit.blocks);
    free(pool->jit.patches);
    free(pool->jit.heads);
    free(pool->jit.patch_heads);
    munmap(pool->map, pool->map_bytes);
    free(pool->pfn_gen);
    free(pool);
    vm->cpu_pool = NULL;
}

bool cpu_qs_announce(vcpu_t *cpu) {
    cpu_pool_t *pool = cpu_pool(cpu);
    uint64_t epoch = atomic_load_explicit(&pool->epoch, memory_order_seq_cst);
    __builtin_arm_isb(15);
    if (((cpu_icache_t *)cpu->icache)->cur_page != NULL && !cpu_page_live(cpu)) {
        return false;
    }
    atomic_store_explicit(&pool->qs[cpu->cpu_id].qs, epoch, memory_order_seq_cst);
    ((cpu_icache_t *)cpu->icache)->qs_ticks++;
    cpu_jit_publish(pool);
    return true;
}

void cpu_qs_park(vcpu_t *cpu) {
    ((cpu_icache_t *)cpu->icache)->qs_ticks++;
    atomic_store_explicit(&cpu_pool(cpu)->qs[cpu->cpu_id].parked, true, memory_order_seq_cst);
}

bool cpu_qs_unpark(vcpu_t *cpu) {
    cpu_pool_t *pool = cpu_pool(cpu);
    atomic_store_explicit(&pool->qs[cpu->cpu_id].parked, false, memory_order_seq_cst);
    (void)atomic_load_explicit(&pool->epoch, memory_order_seq_cst);
    __builtin_arm_isb(15);
    bool flushed = mmu_poll_shootdown(cpu);
    const cpu_icache_t *ic = (const cpu_icache_t *)cpu->icache;
    return !flushed && (ic->cur_page == NULL || (ic->cur_exec_epoch == cpu->exec_epoch && cpu_page_live(cpu)));
}

bool cpu_qs_parked(const cpu_pool_t *pool, uint32_t cpu_id) {
    return atomic_load_explicit(&pool->qs[cpu_id].parked, memory_order_seq_cst);
}

uint64_t cpu_qs_retire_epoch(cpu_pool_t *pool) {
    return atomic_fetch_add_explicit(&pool->epoch, 1, memory_order_seq_cst) + 1U;
}

void cpu_qs_wait(cpu_pool_t *pool, vcpu_t *self, uint64_t target) {
    vmm_machine_t *vm = pool->vm;
    for (uint32_t i = 0; i < vm->cpu_count; ++i) {
        vcpu_t *other = vm->cpus[i];
        if (other == NULL || other == self) {
            continue;
        }
        cpu_qs_t *qs = &pool->qs[i];
        uint32_t spins = 0;
        while (!atomic_load_explicit(&qs->parked, memory_order_seq_cst) &&
               atomic_load_explicit(&qs->qs, memory_order_seq_cst) < target) {
            atomic_store_explicit(&other->shootdown, 1, memory_order_seq_cst);
            mmu_poll_shootdown(self);
            if (++spins < 128) {
                __asm__ __volatile__("yield");
            } else {
                sched_yield();
            }
        }
    }
}

uint64_t cpu_qs_quiescent(cpu_pool_t *pool, const vcpu_t *self) {
    vmm_machine_t *vm = pool->vm;
    uint64_t mature = atomic_load_explicit(&pool->epoch, memory_order_seq_cst);
    for (uint32_t i = 0; i < vm->cpu_count; ++i) {
        if (vm->cpus[i] == NULL || vm->cpus[i] == self ||
            atomic_load_explicit(&pool->qs[i].parked, memory_order_seq_cst)) {
            continue;
        }
        uint64_t qs = atomic_load_explicit(&pool->qs[i].qs, memory_order_seq_cst);
        if (qs < mature) {
            mature = qs;
        }
    }
    return mature;
}

static void pool_unlink(cpu_pool_t *pool, uint32_t slot) {
    uint32_t *link = pool_head(pool, pool_page(pool, slot)->pa_page);
    while (*link != slot) {
        link = &pool_page(pool, *link)->next;
    }
    *link = pool_page(pool, slot)->next;
}

static void pool_retire(cpu_pool_t *pool, uint32_t slot) {
    pool->slot_live[slot] = false;
    atomic_store_explicit(&pool_page(pool, slot)->gen, ++pool->slot_gen[slot], memory_order_seq_cst);
    pool->slot_link[slot] = POOL_NONE;
    if (pool->retire_tail == POOL_NONE) {
        pool->retire_head = slot;
    } else {
        pool->slot_link[pool->retire_tail] = slot;
    }
    pool->retire_tail = slot;
}

static uint64_t pool_retire_stamp(cpu_pool_t *pool, uint32_t prev_tail) {
    uint32_t slot = prev_tail == POOL_NONE ? pool->retire_head : pool->slot_link[prev_tail];
    if (slot == POOL_NONE) {
        return 0;
    }
    uint64_t epoch = cpu_qs_retire_epoch(pool);
    for (; slot != POOL_NONE; slot = pool->slot_link[slot]) {
        pool->slot_retire[slot] = epoch;
    }
    return epoch;
}

static void pool_free(cpu_pool_t *pool, uint32_t slot, bool reusable) {
    pool->slot_reusable[slot] = reusable;
    pool->slot_link[slot] = pool->free_head;
    pool->free_head = slot;
}

void cpu_pool_trim(cpu_pool_t *pool, const vcpu_t *self) {
    uint64_t mature = cpu_qs_quiescent(pool, self);
    os_unfair_lock_lock(&pool->lock);
    uint32_t head = pool->retire_head, tail = POOL_NONE;
    while (pool->retire_head != POOL_NONE && pool->slot_retire[pool->retire_head] <= mature) {
        tail = pool->retire_head;
        pool->retire_head = pool->slot_link[tail];
    }
    if (pool->retire_head == POOL_NONE) {
        pool->retire_tail = POOL_NONE;
    }
    os_unfair_lock_unlock(&pool->lock);
    if (tail == POOL_NONE) {
        return;
    }
    for (uint32_t slot = head;; slot = pool->slot_link[slot]) {
        madvise(pool_page(pool, slot), POOL_SLOT_BYTES, MADV_FREE_REUSABLE);
        if (slot == tail) {
            break;
        }
    }
    os_unfair_lock_lock(&pool->lock);
    for (uint32_t slot = head, next;; slot = next) {
        next = pool->slot_link[slot];
        pool_free(pool, slot, true);
        if (slot == tail) {
            break;
        }
    }
    os_unfair_lock_unlock(&pool->lock);
}

static void pool_evict(cpu_pool_t *pool) {
    uint32_t prev_tail = pool->retire_tail;
    uint32_t count = 0;
    for (uint32_t scanned = 0; scanned < 2U * POOL_SLOTS && count < POOL_EVICT_BATCH; ++scanned) {
        uint32_t slot = pool->clock;
        pool->clock = slot + 1U < POOL_SLOTS ? slot + 1U : 0;
        if (!pool->slot_live[slot]) {
            continue;
        }
        ir_page_t *pg = pool_page(pool, slot);
        if (atomic_load_explicit(&pg->ref, memory_order_relaxed) != 0) {
            atomic_store_explicit(&pg->ref, 0, memory_order_relaxed);
            continue;
        }
        pool_unlink(pool, slot);
        pool_retire(pool, slot);
        ++count;
    }
    pool_retire_stamp(pool, prev_tail);
}

ir_page_t *cpu_pool_alloc(cpu_pool_t *pool, vcpu_t *self) {
    for (;;) {
        uint64_t quiescent = cpu_qs_quiescent(pool, self);
        os_unfair_lock_lock(&pool->lock);
        uint32_t slot = pool->retire_head;
        if (slot != POOL_NONE && pool->slot_retire[slot] <= quiescent) {
            pool->retire_head = pool->slot_link[slot];
            if (pool->retire_head == POOL_NONE) {
                pool->retire_tail = POOL_NONE;
            }
            os_unfair_lock_unlock(&pool->lock);
            return pool_page(pool, slot);
        }
        slot = pool->free_head;
        if (slot != POOL_NONE) {
            pool->free_head = pool->slot_link[slot];
            bool reusable = pool->slot_reusable[slot];
            os_unfair_lock_unlock(&pool->lock);
            ir_page_t *pg = pool_page(pool, slot);
            if (reusable) {
                madvise(pg, POOL_SLOT_BYTES, MADV_FREE_REUSE);
            }
            return pg;
        }
        uint64_t newest = pool->retire_tail == POOL_NONE ? 0 : pool->slot_retire[pool->retire_tail];
        if (newest <= quiescent) {
            pool_evict(pool);
            os_unfair_lock_unlock(&pool->lock);
            continue;
        }
        os_unfair_lock_unlock(&pool->lock);
        cpu_qs_wait(pool, self, newest);
    }
}

static bool pool_key_match(const ir_page_t *pg, uint64_t pa_page, uint8_t el, uint8_t world, uint32_t traps) {
    return pg->pa_page == pa_page && pg->el == el && pg->world == world && pg->traps == traps;
}

ir_page_t *cpu_pool_live(cpu_pool_t *pool, uint64_t pa_page, uint8_t el, uint8_t world, uint32_t traps) {
    for (uint32_t slot = *pool_head(pool, pa_page); slot != POOL_NONE;) {
        ir_page_t *pg = pool_page(pool, slot);
        if (pool_key_match(pg, pa_page, el, world, traps)) {
            return pg;
        }
        slot = pg->next;
    }
    return NULL;
}

ir_page_t *cpu_pool_find(cpu_pool_t *pool, uint64_t pa_page, uint8_t el, uint8_t world, uint32_t traps,
                         uint32_t *gen_out) {
    os_unfair_lock_lock(&pool->lock);
    ir_page_t *pg = cpu_pool_live(pool, pa_page, el, world, traps);
    if (pg != NULL) {
        *gen_out = atomic_load_explicit(&pg->gen, memory_order_relaxed);
        atomic_store_explicit(&pg->ref, 1, memory_order_relaxed);
    }
    os_unfair_lock_unlock(&pool->lock);
    return pg;
}

_Atomic uint32_t *cpu_pool_pfn_gen(cpu_pool_t *pool, uint64_t pa_page) {
    uint64_t pfn = (pa_page - VMM_RAM_BASE) >> VMM_PAGE_SHIFT;
    return &pool->pfn_gen[pa_page >= VMM_RAM_BASE && pfn < pool->pfn_count ? pfn : pool->pfn_count];
}

static void pool_relink(cpu_pool_t *pool, ir_page_t *pg) {
    cpu_jit_pool_t *jit = &pool->jit;
    if (!jit->active) {
        return;
    }
    for (uint32_t b = jit->heads[cpu_pa_bucket(pg->pa_page, JIT_BUCKETS)]; b != POOL_NONE; b = jit->blocks[b].next) {
        const cpu_jit_block_t *block = &jit->blocks[b];
        if (block->pa_page == pg->pa_page && block->el == pg->el && block->world == pg->world &&
            block->traps == pg->traps && block->pfn_gen == pg->pfn_gen && block->code_gen == pg->code_gen &&
            block->hash == pg->hash && pg->insns[block->index].fast_offset == block->orig) {
            pg->insns[block->index].fast_offset = block->offset;
            pg->insns[block->index].heat = 0;
        }
    }
}

void cpu_pool_publish(cpu_pool_t *pool, ir_page_t *pg) {
    uint32_t slot = pool_slot(pool, pg);
    os_unfair_lock_lock(&pool->jit.lock);
    os_unfair_lock_lock(&pool->lock);
    if (cpu_pool_live(pool, pg->pa_page, pg->el, pg->world, pg->traps) != NULL ||
        atomic_load_explicit(cpu_pool_pfn_gen(pool, pg->pa_page), memory_order_relaxed) != pg->pfn_gen ||
        atomic_load_explicit(&pool->code_gen, memory_order_relaxed) != pg->code_gen) {
        pool_free(pool, slot, false);
    } else {
        pool_relink(pool, pg);
        uint32_t *head = pool_head(pool, pg->pa_page);
        pg->next = *head;
        *head = slot;
        atomic_store_explicit(&pg->ref, 1, memory_order_relaxed);
        pool->slot_live[slot] = true;
        atomic_store_explicit(&pg->gen, ++pool->slot_gen[slot], memory_order_release);
    }
    os_unfair_lock_unlock(&pool->lock);
    os_unfair_lock_unlock(&pool->jit.lock);
}

uint64_t cpu_pool_retire_pa(cpu_pool_t *pool, uint64_t pa_page) {
    os_unfair_lock_lock(&pool->lock);
    atomic_fetch_add_explicit(cpu_pool_pfn_gen(pool, pa_page), 1, memory_order_seq_cst);
    uint32_t prev_tail = pool->retire_tail;
    for (uint32_t *link = pool_head(pool, pa_page); *link != POOL_NONE;) {
        uint32_t slot = *link;
        ir_page_t *pg = pool_page(pool, slot);
        if (pg->pa_page == pa_page) {
            *link = pg->next;
            pool_retire(pool, slot);
        } else {
            link = &pg->next;
        }
    }
    uint64_t epoch = pool_retire_stamp(pool, prev_tail);
    os_unfair_lock_unlock(&pool->lock);
    return epoch;
}

uint64_t cpu_pool_retire_all(cpu_pool_t *pool) {
    os_unfair_lock_lock(&pool->lock);
    atomic_fetch_add_explicit(&pool->code_gen, 1, memory_order_seq_cst);
    uint32_t prev_tail = pool->retire_tail;
    for (uint32_t b = 0; b < POOL_BUCKETS; ++b) {
        while (pool->heads[b] != POOL_NONE) {
            uint32_t slot = pool->heads[b];
            pool->heads[b] = pool_page(pool, slot)->next;
            pool_retire(pool, slot);
        }
    }
    uint64_t epoch = pool_retire_stamp(pool, prev_tail);
    os_unfair_lock_unlock(&pool->lock);
    return epoch;
}
