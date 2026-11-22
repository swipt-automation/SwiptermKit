#include "board_internal.h"
#include "machine.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum : uint32_t {
    GIC_NUM_IRQS = 128,
    GIC_NUM_SPIS = 96,
    GIC_SPURIOUS_IRQ = 1023,
    GIC_IDLE_PRIO = 0xff,
};

typedef struct gic_cpu_state {
    uint32_t ctlr;
    uint32_t pmr;
    uint32_t bpr;
    uint32_t enabled_local;
    uint32_t pending_local;
    uint32_t level_local;
    uint32_t active_local;
    uint32_t icfgr_ppi;
    uint8_t sgi_pending[16];
    uint8_t priority_local[32];
} gic_cpu_state_t;

typedef struct gicv2 {
    pthread_mutex_t lock;
    vmm_machine_t *vm;
    bool dist_enabled;
    uint32_t spi_enabled[3];
    uint32_t spi_pending[3];
    uint32_t spi_level[3];
    uint32_t spi_active[3];
    uint8_t spi_priority[GIC_NUM_SPIS];
    uint8_t spi_targets[GIC_NUM_SPIS];
    uint32_t spi_icfgr[6];
    gic_cpu_state_t cpu[VMM_MAX_VCPUS];
} gicv2_t;

static uint32_t gic_running_prio_locked(const gicv2_t *g, uint32_t cpu_id) {
    const gic_cpu_state_t *c = &g->cpu[cpu_id];
    uint32_t best = GIC_IDLE_PRIO;

    uint32_t act = c->active_local;
    while (act) {
        uint32_t id = (uint32_t)__builtin_ctz(act);
        act &= act - 1;
        if (c->priority_local[id] < best) {
            best = c->priority_local[id];
        }
    }

    uint8_t mask = (uint8_t)(1U << cpu_id);
    for (uint32_t w = 0; w < 3; w++) {
        uint32_t sact = g->spi_active[w];
        while (sact) {
            uint32_t bit = (uint32_t)__builtin_ctz(sact);
            sact &= sact - 1;
            uint32_t idx = w * 32U + bit;
            if ((g->spi_targets[idx] & mask) && g->spi_priority[idx] < best) {
                best = g->spi_priority[idx];
            }
        }
    }
    return best;
}

static uint32_t gic_highest_pending_locked(const gicv2_t *g, uint32_t cpu_id, uint32_t *prio_out) {
    const gic_cpu_state_t *c = &g->cpu[cpu_id];
    uint32_t best_id = GIC_SPURIOUS_IRQ;
    uint32_t best_prio = GIC_IDLE_PRIO;

    if (!g->dist_enabled) {
        if (prio_out) {
            *prio_out = best_prio;
        }
        return GIC_SPURIOUS_IRQ;
    }

    uint32_t eff_local = c->pending_local & 0x0000ffffU;
    for (uint32_t id = 16; id < 32; id++) {
        uint32_t bit = 1U << id;
        bool edge = ((c->icfgr_ppi >> ((id - 16U) * 2U + 1U)) & 1U) != 0;
        if (edge ? ((c->pending_local & bit) != 0) : (((c->pending_local | c->level_local) & bit) != 0)) {
            eff_local |= bit;
        }
    }

    uint32_t cand = eff_local & c->enabled_local & ~c->active_local;
    while (cand) {
        uint32_t id = (uint32_t)__builtin_ctz(cand);
        cand &= cand - 1;
        uint32_t p = c->priority_local[id];
        if (p < best_prio) {
            best_prio = p;
            best_id = id;
        }
    }

    uint8_t cpu_mask = (uint8_t)(1U << cpu_id);
    for (uint32_t w = 0; w < 3; w++) {
        uint32_t maybe = (g->spi_pending[w] | g->spi_level[w]) & g->spi_enabled[w] & ~g->spi_active[w];
        while (maybe) {
            uint32_t bit = (uint32_t)__builtin_ctz(maybe);
            maybe &= maybe - 1;
            uint32_t idx = w * 32U + bit;
            if ((g->spi_targets[idx] & cpu_mask) == 0) {
                continue;
            }
            bool edge = ((g->spi_icfgr[idx / 16U] >> ((idx % 16U) * 2U + 1U)) & 1U) != 0;
            bool pend = edge ? ((g->spi_pending[w] & (1U << bit)) != 0)
                             : (((g->spi_pending[w] | g->spi_level[w]) & (1U << bit)) != 0);
            if (!pend) {
                continue;
            }
            uint32_t p = g->spi_priority[idx];
            if (p < best_prio) {
                best_prio = p;
                best_id = 32U + idx;
            }
        }
    }

    if (prio_out) {
        *prio_out = best_prio;
    }
    return best_id;
}

static uint32_t gic_update_cpu_locked(gicv2_t *g, uint32_t cpu_id) {
    if (cpu_id >= g->vm->cpu_count || !g->vm->cpus[cpu_id]) {
        return 0;
    }
    const gic_cpu_state_t *c = &g->cpu[cpu_id];
    uint32_t best_prio = GIC_IDLE_PRIO;
    uint32_t best_id = GIC_SPURIOUS_IRQ;

    if (g->dist_enabled && (c->ctlr & 1U)) {
        best_id = gic_highest_pending_locked(g, cpu_id, &best_prio);
        uint32_t run_prio = gic_running_prio_locked(g, cpu_id);
        uint32_t limit = c->pmr < run_prio ? c->pmr : run_prio;
        if (best_id >= GIC_NUM_IRQS || best_prio >= limit) {
            best_id = GIC_SPURIOUS_IRQ;
        }
    }

    uint32_t asserted = (best_id < GIC_NUM_IRQS) ? 1U : 0U;
    uint32_t old = atomic_exchange_explicit(&g->vm->cpus[cpu_id]->irq_pending, asserted, memory_order_acq_rel);
    return asserted && !old ? (1U << cpu_id) : 0U;
}

static uint32_t gic_update_all_locked(gicv2_t *g) {
    uint32_t wake_mask = 0;
    for (uint32_t i = 0; i < g->vm->cpu_count; i++) {
        wake_mask |= gic_update_cpu_locked(g, i);
    }
    return wake_mask;
}

static void gic_wake_cpus(vmm_machine_t *vm, uint32_t wake_mask) {
    while (wake_mask) {
        uint32_t id = (uint32_t)__builtin_ctz(wake_mask);
        wake_mask &= wake_mask - 1;
        vmm_vcpu_wake(vm, id);
    }
}

void *gicv2_create(vmm_machine_t *vm) {
    gicv2_t *g = calloc(1, sizeof(*g));
    if (!g) {
        return NULL;
    }
    pthread_mutex_init(&g->lock, NULL);
    g->vm = vm;
    for (uint32_t i = 0; i < GIC_NUM_SPIS; i++) {
        g->spi_targets[i] = 0x01;
    }
    for (uint32_t c = 0; c < VMM_MAX_VCPUS; c++) {
        g->cpu[c].enabled_local = 0x0000ffffU;
        g->cpu[c].pmr = 0x00;
    }
    return g;
}

void gicv2_destroy(void *gic) {
    if (!gic) {
        return;
    }
    gicv2_t *g = (gicv2_t *)gic;
    pthread_mutex_destroy(&g->lock);
    free(g);
}

void gicv2_set_irq(vmm_machine_t *vm, uint32_t intid, bool level) {
    if (!vm || !vm->gic || intid < 32 || intid >= GIC_NUM_IRQS) {
        return;
    }
    gicv2_t *g = (gicv2_t *)vm->gic;
    uint32_t idx = intid - 32U;
    uint32_t w = idx / 32U;
    uint32_t bit = 1U << (idx % 32U);

    pthread_mutex_lock(&g->lock);
    bool old = (g->spi_level[w] & bit) != 0;
    if (old == level) {
        pthread_mutex_unlock(&g->lock);
        return;
    }
    if (level) {
        g->spi_level[w] |= bit;
        if (!old) {
            g->spi_pending[w] |= bit;
        }
    } else {
        g->spi_level[w] &= ~bit;
    }
    uint32_t wake_mask = gic_update_all_locked(g);
    pthread_mutex_unlock(&g->lock);

    gic_wake_cpus(vm, wake_mask);
}

void gicv2_set_timer_ppis(vmm_machine_t *vm, uint32_t cpu_id, bool virt, bool phys) {
    if (!vm || !vm->gic || cpu_id >= vm->cpu_count) {
        return;
    }
    gicv2_t *g = (gicv2_t *)vm->gic;
    const uint32_t mask = (1U << VMM_IRQ_TIMER_VIRT) | (1U << VMM_IRQ_TIMER_PHYS);
    uint32_t lines = ((uint32_t)virt << VMM_IRQ_TIMER_VIRT) |
                     ((uint32_t)phys << VMM_IRQ_TIMER_PHYS);

    pthread_mutex_lock(&g->lock);
    gic_cpu_state_t *c = &g->cpu[cpu_id];
    uint32_t old = c->level_local & mask;
    if (old == lines) {
        pthread_mutex_unlock(&g->lock);
        return;
    }
    c->level_local = (c->level_local & ~mask) | lines;
    c->pending_local |= lines & ~old;
    uint32_t wake_mask = gic_update_cpu_locked(g, cpu_id);
    pthread_mutex_unlock(&g->lock);

    gic_wake_cpus(vm, wake_mask);
}

static uint32_t gic_read_byte_array(const gicv2_t *g, uint32_t cpu_id, uint32_t base_irq,
                                    uint32_t size, bool is_targets) {
    uint32_t res = 0;
    for (uint32_t i = 0; i < size && i < 4; i++) {
        uint32_t irq = base_irq + i;
        uint8_t b = 0;
        if (irq < GIC_NUM_IRQS) {
            if (is_targets) {
                b = (irq < 32U) ? (uint8_t)(1U << cpu_id) : g->spi_targets[irq - 32U];
            } else {
                b = (irq < 32U) ? g->cpu[cpu_id].priority_local[irq] : g->spi_priority[irq - 32U];
            }
        }
        res |= ((uint32_t)b << (i * 8U));
    }
    return res;
}

static void gic_write_byte_array(gicv2_t *g, uint32_t cpu_id, uint32_t base_irq,
                                 uint32_t size, uint64_t val, bool is_targets) {
    uint8_t valid_cpus = (uint8_t)((1U << g->vm->cpu_count) - 1U);
    for (uint32_t i = 0; i < size && i < 4; i++) {
        uint32_t irq = base_irq + i;
        uint8_t b = (uint8_t)((val >> (i * 8U)) & 0xffU);
        if (irq >= GIC_NUM_IRQS) {
            continue;
        }
        if (is_targets) {
            if (irq >= 32U) {
                g->spi_targets[irq - 32U] = (uint8_t)(b & valid_cpus);
            }
        } else {
            if (irq < 32U) {
                g->cpu[cpu_id].priority_local[irq] = b;
            } else {
                g->spi_priority[irq - 32U] = b;
            }
        }
    }
}

uint64_t gicv2_dist_read(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size) {
    if (!gic) {
        return 0;
    }
    gicv2_t *g = (gicv2_t *)gic;
    uint32_t cpu_id = (cpu && cpu->cpu_id < VMM_MAX_VCPUS) ? cpu->cpu_id : 0;
    uint32_t res = 0;

    pthread_mutex_lock(&g->lock);
    gic_cpu_state_t *c = &g->cpu[cpu_id];

    if (offset == 0x000) {
        res = g->dist_enabled ? 1U : 0U;
    } else if (offset == 0x004) {
        uint32_t cpu_num = g->vm->cpu_count ? (g->vm->cpu_count - 1U) : 0U;
        res = ((cpu_num & 7U) << 5) | ((GIC_NUM_IRQS / 32U) - 1U);
    } else if (offset == 0x008) {
        res = 0x0200143bU;
    } else if (offset >= 0x100 && offset < 0x110) {
        uint32_t n = (offset - 0x100) / 4U;
        res = (n == 0) ? c->enabled_local : g->spi_enabled[n - 1U];
    } else if (offset >= 0x180 && offset < 0x190) {
        uint32_t n = (offset - 0x180) / 4U;
        res = (n == 0) ? c->enabled_local : g->spi_enabled[n - 1U];
    } else if (offset >= 0x200 && offset < 0x210) {
        uint32_t n = (offset - 0x200) / 4U;
        res = (n == 0) ? (c->pending_local | c->level_local)
                       : (g->spi_pending[n - 1U] | g->spi_level[n - 1U]);
    } else if (offset >= 0x280 && offset < 0x290) {
        uint32_t n = (offset - 0x280) / 4U;
        res = (n == 0) ? (c->pending_local | c->level_local)
                       : (g->spi_pending[n - 1U] | g->spi_level[n - 1U]);
    } else if (offset >= 0x300 && offset < 0x310) {
        uint32_t n = (offset - 0x300) / 4U;
        res = (n == 0) ? c->active_local : g->spi_active[n - 1U];
    } else if (offset >= 0x380 && offset < 0x390) {
        uint32_t n = (offset - 0x380) / 4U;
        res = (n == 0) ? c->active_local : g->spi_active[n - 1U];
    } else if (offset >= 0x400 && offset < 0x480) {
        res = gic_read_byte_array(g, cpu_id, offset - 0x400, size, false);
    } else if (offset >= 0x800 && offset < 0x880) {
        res = gic_read_byte_array(g, cpu_id, offset - 0x800, size, true);
    } else if (offset >= 0xc00 && offset < 0xc20) {
        uint32_t n = (offset - 0xc00) / 4U;
        if (n == 0) {
            res = 0xaaaaaaaaU;
        } else if (n == 1) {
            res = c->icfgr_ppi;
        } else if (n < 8) {
            res = g->spi_icfgr[n - 2U];
        }
    } else if ((offset >= 0xf10 && offset < 0xf20) || (offset >= 0xf20 && offset < 0xf30)) {
        uint32_t base = (offset >= 0xf20) ? (offset - 0xf20) : (offset - 0xf10);
        for (uint32_t i = 0; i < size && i < 4 && base + i < 16U; i++) {
            res |= ((uint32_t)c->sgi_pending[base + i] << (i * 8U));
        }
    } else if (offset >= 0xfe0 && offset < 0x1000) {
        static const uint8_t ids[8] = {0x90, 0xb4, 0x2b, 0x00, 0x0d, 0xf0, 0x05, 0xb1};
        res = ids[(offset - 0xfe0) / 4U];
    }

    pthread_mutex_unlock(&g->lock);
    return res;
}

void gicv2_dist_write(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val) {
    if (!gic) {
        return;
    }
    gicv2_t *g = (gicv2_t *)gic;
    uint32_t cpu_id = (cpu && cpu->cpu_id < VMM_MAX_VCPUS) ? cpu->cpu_id : 0;
    uint32_t v = (uint32_t)val;

    pthread_mutex_lock(&g->lock);
    gic_cpu_state_t *c = &g->cpu[cpu_id];
    uint32_t wake_mask = 0;

    if (offset == 0x000) {
        g->dist_enabled = (v & 1U) != 0;
        wake_mask = gic_update_all_locked(g);
    } else if (offset >= 0x100 && offset < 0x110) {
        uint32_t n = (offset - 0x100) / 4U;
        if (n == 0) {
            c->enabled_local |= v | 0x0000ffffU;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_enabled[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x180 && offset < 0x190) {
        uint32_t n = (offset - 0x180) / 4U;
        if (n == 0) {
            c->enabled_local = (c->enabled_local & ~v) | 0x0000ffffU;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_enabled[n - 1U] &= ~v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x200 && offset < 0x210) {
        uint32_t n = (offset - 0x200) / 4U;
        if (n == 0) {
            c->pending_local |= (v & 0xffff0000U);
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_pending[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x280 && offset < 0x290) {
        uint32_t n = (offset - 0x280) / 4U;
        if (n == 0) {
            c->pending_local &= ~(v & 0xffff0000U);
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_pending[n - 1U] &= ~v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x300 && offset < 0x310) {
        uint32_t n = (offset - 0x300) / 4U;
        if (n == 0) {
            c->active_local |= v;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_active[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x380 && offset < 0x390) {
        uint32_t n = (offset - 0x380) / 4U;
        if (n == 0) {
            c->active_local &= ~v;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->spi_active[n - 1U] &= ~v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x400 && offset < 0x480) {
        gic_write_byte_array(g, cpu_id, offset - 0x400, size, val, false);
        wake_mask = gic_update_all_locked(g);
    } else if (offset >= 0x800 && offset < 0x880) {
        gic_write_byte_array(g, cpu_id, offset - 0x800, size, val, true);
        wake_mask = gic_update_all_locked(g);
    } else if (offset >= 0xc00 && offset < 0xc20) {
        uint32_t n = (offset - 0xc00) / 4U;
        if (n == 1) {
            c->icfgr_ppi = v & 0xaaaaaaaaU;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else if (n >= 2 && n < 8) {
            g->spi_icfgr[n - 2U] = v & 0xaaaaaaaaU;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset == 0xf00) {
        uint32_t intid = v & 0xfU;
        uint32_t filter = (v >> 24) & 3U;
        uint32_t targets = 0;
        uint32_t all_mask = (1U << g->vm->cpu_count) - 1U;
        if (filter == 0) {
            targets = ((v >> 16) & 0xffU) & all_mask;
        } else if (filter == 1) {
            targets = all_mask & ~(1U << cpu_id);
        } else if (filter == 2) {
            targets = (1U << cpu_id) & all_mask;
        }
        for (uint32_t t = 0; t < g->vm->cpu_count; t++) {
            if (targets & (1U << t)) {
                g->cpu[t].sgi_pending[intid] |= (uint8_t)(1U << cpu_id);
                g->cpu[t].pending_local |= (1U << intid);
                wake_mask |= gic_update_cpu_locked(g, t);
            }
        }
    } else if (offset >= 0xf10 && offset < 0xf20) {
        uint32_t base = offset - 0xf10;
        for (uint32_t i = 0; i < size && i < 4 && base + i < 16U; i++) {
            uint8_t clr = (uint8_t)((val >> (i * 8U)) & 0xffU);
            c->sgi_pending[base + i] &= (uint8_t)~clr;
            if (c->sgi_pending[base + i] == 0) {
                c->pending_local &= ~(1U << (base + i));
            }
        }
        wake_mask = gic_update_cpu_locked(g, cpu_id);
    } else if (offset >= 0xf20 && offset < 0xf30) {
        uint32_t base = offset - 0xf20;
        uint8_t all_mask = (uint8_t)((1U << g->vm->cpu_count) - 1U);
        for (uint32_t i = 0; i < size && i < 4 && base + i < 16U; i++) {
            uint8_t set = (uint8_t)(((val >> (i * 8U)) & 0xffU) & all_mask);
            if (set) {
                c->sgi_pending[base + i] |= set;
                c->pending_local |= (1U << (base + i));
            }
        }
        wake_mask = gic_update_cpu_locked(g, cpu_id);
    }

    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
}

uint64_t gicv2_cpu_read(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size) {
    (void)size;
    if (!gic) {
        return 0;
    }
    gicv2_t *g = (gicv2_t *)gic;
    uint32_t cpu_id = (cpu && cpu->cpu_id < VMM_MAX_VCPUS) ? cpu->cpu_id : 0;
    uint32_t res = 0;
    uint32_t wake_mask = 0;

    pthread_mutex_lock(&g->lock);
    gic_cpu_state_t *c = &g->cpu[cpu_id];

    switch (offset) {
    case 0x000:
        res = c->ctlr;
        break;
    case 0x004:
        res = c->pmr;
        break;
    case 0x008:
        res = c->bpr;
        break;
    case 0x00c: {
        uint32_t best_prio = GIC_IDLE_PRIO;
        uint32_t id = (c->ctlr & 1U) ? gic_highest_pending_locked(g, cpu_id, &best_prio)
                                     : GIC_SPURIOUS_IRQ;
        uint32_t run_prio = gic_running_prio_locked(g, cpu_id);
        uint32_t limit = c->pmr < run_prio ? c->pmr : run_prio;
        if (id >= GIC_NUM_IRQS || best_prio >= limit) {
            res = GIC_SPURIOUS_IRQ;
        } else if (id < 16U) {
            uint8_t srcs = c->sgi_pending[id];
            uint32_t src = srcs ? (uint32_t)__builtin_ctz(srcs) : 0U;
            c->sgi_pending[id] &= (uint8_t)~(1U << src);
            if (c->sgi_pending[id] == 0) {
                c->pending_local &= ~(1U << id);
            }
            c->active_local |= (1U << id);
            res = (src << 10) | id;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else if (id < 32U) {
            c->pending_local &= ~(1U << id);
            c->active_local |= (1U << id);
            res = id;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            uint32_t idx = id - 32U;
            uint32_t w = idx / 32U;
            uint32_t bit = 1U << (idx % 32U);
            g->spi_pending[w] &= ~bit;
            g->spi_active[w] |= bit;
            res = id;
            wake_mask = gic_update_all_locked(g);
        }
        break;
    }
    case 0x014:
        res = gic_running_prio_locked(g, cpu_id);
        break;
    case 0x018: {
        uint32_t best_prio = GIC_IDLE_PRIO;
        uint32_t id = gic_highest_pending_locked(g, cpu_id, &best_prio);
        if (id < 16U && c->sgi_pending[id]) {
            uint32_t src = (uint32_t)__builtin_ctz(c->sgi_pending[id]);
            res = (src << 10) | id;
        } else {
            res = id;
        }
        break;
    }
    case 0x0fc:
        res = 0x0202143bU;
        break;
    default:
        res = 0;
        break;
    }

    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
    return res;
}

void gicv2_cpu_write(void *gic, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val) {
    (void)size;
    if (!gic) {
        return;
    }
    gicv2_t *g = (gicv2_t *)gic;
    uint32_t cpu_id = (cpu && cpu->cpu_id < VMM_MAX_VCPUS) ? cpu->cpu_id : 0;
    uint32_t v = (uint32_t)val;
    uint32_t wake_mask = 0;

    pthread_mutex_lock(&g->lock);
    gic_cpu_state_t *c = &g->cpu[cpu_id];

    switch (offset) {
    case 0x000:
        c->ctlr = v & 0x201U;
        wake_mask = gic_update_cpu_locked(g, cpu_id);
        break;
    case 0x004:
        c->pmr = v & 0xffU;
        wake_mask = gic_update_cpu_locked(g, cpu_id);
        break;
    case 0x008:
        c->bpr = v & 7U;
        break;
    case 0x010:
    case 0x1000: {
        uint32_t id = v & 0x3ffU;
        if (id < 32U) {
            c->active_local &= ~(1U << id);
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else if (id < GIC_NUM_IRQS) {
            uint32_t idx = id - 32U;
            g->spi_active[idx / 32U] &= ~(1U << (idx % 32U));
            wake_mask = gic_update_all_locked(g);
        }
        break;
    }
    default:
        break;
    }

    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
}

void gicv2_save_state(const void *gic, gicv2_snapshot_t *out) {
    memset(out, 0, sizeof(*out));
    if (!gic) {
        return;
    }
    gicv2_t *g = (gicv2_t *)gic;
    pthread_mutex_lock(&g->lock);
    out->dist_enabled = g->dist_enabled ? 1U : 0U;
    memcpy(out->spi_enabled, g->spi_enabled, sizeof(out->spi_enabled));
    memcpy(out->spi_pending, g->spi_pending, sizeof(out->spi_pending));
    memcpy(out->spi_level, g->spi_level, sizeof(out->spi_level));
    memcpy(out->spi_active, g->spi_active, sizeof(out->spi_active));
    memcpy(out->spi_priority, g->spi_priority, sizeof(out->spi_priority));
    memcpy(out->spi_targets, g->spi_targets, sizeof(out->spi_targets));
    memcpy(out->spi_icfgr, g->spi_icfgr, sizeof(out->spi_icfgr));
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        out->cpu[i].ctlr = g->cpu[i].ctlr;
        out->cpu[i].pmr = g->cpu[i].pmr;
        out->cpu[i].bpr = g->cpu[i].bpr;
        out->cpu[i].enabled_local = g->cpu[i].enabled_local;
        out->cpu[i].pending_local = g->cpu[i].pending_local;
        out->cpu[i].level_local = g->cpu[i].level_local;
        out->cpu[i].active_local = g->cpu[i].active_local;
        out->cpu[i].icfgr_ppi = g->cpu[i].icfgr_ppi;
        memcpy(out->cpu[i].sgi_pending, g->cpu[i].sgi_pending, sizeof(out->cpu[i].sgi_pending));
        memcpy(out->cpu[i].priority_local, g->cpu[i].priority_local, sizeof(out->cpu[i].priority_local));
    }
    pthread_mutex_unlock(&g->lock);
}

void gicv2_load_state(void *gic, const gicv2_snapshot_t *in) {
    if (!gic || !in) {
        return;
    }
    gicv2_t *g = (gicv2_t *)gic;
    pthread_mutex_lock(&g->lock);
    g->dist_enabled = in->dist_enabled != 0;
    memcpy(g->spi_enabled, in->spi_enabled, sizeof(g->spi_enabled));
    memcpy(g->spi_pending, in->spi_pending, sizeof(g->spi_pending));
    memcpy(g->spi_level, in->spi_level, sizeof(g->spi_level));
    memcpy(g->spi_active, in->spi_active, sizeof(g->spi_active));
    memcpy(g->spi_priority, in->spi_priority, sizeof(g->spi_priority));
    memcpy(g->spi_targets, in->spi_targets, sizeof(g->spi_targets));
    memcpy(g->spi_icfgr, in->spi_icfgr, sizeof(g->spi_icfgr));
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        g->cpu[i].ctlr = in->cpu[i].ctlr;
        g->cpu[i].pmr = in->cpu[i].pmr;
        g->cpu[i].bpr = in->cpu[i].bpr;
        g->cpu[i].enabled_local = in->cpu[i].enabled_local;
        g->cpu[i].pending_local = in->cpu[i].pending_local;
        g->cpu[i].level_local = in->cpu[i].level_local;
        g->cpu[i].active_local = in->cpu[i].active_local;
        g->cpu[i].icfgr_ppi = in->cpu[i].icfgr_ppi;
        memcpy(g->cpu[i].sgi_pending, in->cpu[i].sgi_pending, sizeof(g->cpu[i].sgi_pending));
        memcpy(g->cpu[i].priority_local, in->cpu[i].priority_local, sizeof(g->cpu[i].priority_local));
    }
    (void)gic_update_all_locked(g);
    pthread_mutex_unlock(&g->lock);
}
