#include "board_internal.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

enum : uint32_t {
    GIC_NUM_IRQS = 128,
    GIC_SPURIOUS_IRQ = 1023,
    GIC_IDLE_PRIO = 0xff,
    GIC_PRIO_SHIFT = 3,
    GICC_CTLR_EOIMODE = 1U << 9,
    GICH_HCR_EN = 1U << 0,
    GICH_HCR_UIE = 1U << 1,
    GICH_HCR_LRENPIE = 1U << 2,
    GICH_HCR_NPIE = 1U << 3,
    GICH_HCR_VGRP0EIE = 1U << 4,
    GICH_HCR_VGRP0DIE = 1U << 5,
    GICH_HCR_VGRP1EIE = 1U << 6,
    GICH_HCR_VGRP1DIE = 1U << 7,
    GICH_HCR_EOICOUNT = 1U << 27,
    GICH_HCR_MASK = 0xF80000FFU,
    GICH_VMCR_GRP0EN = 1U << 0,
    GICH_VMCR_GRP1EN = 1U << 1,
    GICH_VMCR_CTLR = 0x21FU,
    GICH_VMCR_VEM = 1U << 9,
    GICH_VMCR_MASK = 0xFFFC021FU,
    GICH_VTR = (4U << 29) | (4U << 26) | (GICV2_LIST_REGS - 1U),
    GICH_LR_PENDING = 1U << 28,
    GICH_LR_ACTIVE = 1U << 29,
    GICH_LR_STATE = GICH_LR_PENDING | GICH_LR_ACTIVE,
    GICH_LR_GRP1 = 1U << 30,
    GICH_LR_HW = 1U << 31,
    GICH_LR_EOI = 1U << 19,
    GICH_LR_CPUID = 7U << 10,
    GICH_LR_MASK = 0xFF8FFFFFU,
};

struct gicv2 {
    pthread_mutex_t lock;
    vmm_machine_t *vm;
    gicv2_state_t st;
};

static uint32_t gic_update_cpu_locked(gicv2_t *g, uint32_t cpu_id);
static uint32_t gic_update_all_locked(gicv2_t *g);

static uint32_t running_prio(uint32_t apr) {
    return apr ? (uint32_t)__builtin_ctz(apr) << GIC_PRIO_SHIFT : GIC_IDLE_PRIO;
}

static uint32_t gic_deactivate_locked(gicv2_t *g, uint32_t cpu_id, uint32_t id) {
    if (id < 32U) {
        g->st.cpu[cpu_id].active_local &= ~(1U << id);
        return gic_update_cpu_locked(g, cpu_id);
    }
    if (id < GIC_NUM_IRQS) {
        uint32_t idx = id - 32U;
        g->st.spi_active[idx / 32U] &= ~(1U << (idx % 32U));
        return gic_update_all_locked(g);
    }
    return 0;
}

static uint32_t gic_highest_pending_locked(const gicv2_t *g, uint32_t cpu_id, uint32_t *prio_out) {
    const gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];
    uint32_t best_id = GIC_SPURIOUS_IRQ;
    uint32_t best_prio = GIC_IDLE_PRIO;

    if (!g->st.dist_enabled) {
        *prio_out = best_prio;
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
        uint32_t maybe = (g->st.spi_pending[w] | g->st.spi_level[w]) & g->st.spi_enabled[w] & ~g->st.spi_active[w];
        while (maybe) {
            uint32_t bit = (uint32_t)__builtin_ctz(maybe);
            maybe &= maybe - 1;
            uint32_t idx = w * 32U + bit;
            if ((g->st.spi_targets[idx] & cpu_mask) == 0) {
                continue;
            }
            bool edge = ((g->st.spi_icfgr[idx / 16U] >> ((idx % 16U) * 2U + 1U)) & 1U) != 0;
            bool pend = edge ? ((g->st.spi_pending[w] & (1U << bit)) != 0)
                             : (((g->st.spi_pending[w] | g->st.spi_level[w]) & (1U << bit)) != 0);
            if (!pend) {
                continue;
            }
            uint32_t p = g->st.spi_priority[idx];
            if (p < best_prio) {
                best_prio = p;
                best_id = 32U + idx;
            }
        }
    }

    *prio_out = best_prio;
    return best_id;
}

static uint32_t vif_group_prio(const gicv2_vif_t *v, uint32_t lr) {
    uint32_t bpr = (v->vmcr >> 21) & 7U;
    return (((lr >> 23) & 0x1FU) << GIC_PRIO_SHIFT) & (0xFFU << ((bpr < 2U ? 2U : bpr) + 1U));
}

static uint32_t vif_lr_id(uint32_t lr) {
    uint32_t id = lr & 0x3FFU;
    return (lr & GICH_LR_HW) == 0 && id < 16U ? id | (lr & GICH_LR_CPUID) : id;
}

static int vif_highest_pending(const gicv2_vif_t *v, bool deliverable) {
    int best = -1;
    uint32_t best_prio = GIC_IDLE_PRIO + 1U;
    for (uint32_t i = 0; i < GICV2_LIST_REGS; i++) {
        uint32_t lr = v->lr[i];
        uint32_t prio = ((lr >> 23) & 0x1FU) << GIC_PRIO_SHIFT;
        if ((lr & GICH_LR_STATE) == GICH_LR_PENDING &&
            (v->vmcr & ((lr & GICH_LR_GRP1) ? GICH_VMCR_GRP1EN : GICH_VMCR_GRP0EN)) != 0 && prio < best_prio) {
            best = (int)i;
            best_prio = prio;
        }
    }
    if (best >= 0 && deliverable &&
        (best_prio >= ((v->vmcr >> 27) << GIC_PRIO_SHIFT) ||
         vif_group_prio(v, v->lr[best]) >= running_prio(v->apr))) {
        return -1;
    }
    return best;
}

static uint32_t vif_eisr(const gicv2_vif_t *v) {
    uint32_t eisr = 0;
    for (uint32_t i = 0; i < GICV2_LIST_REGS; i++) {
        if ((v->lr[i] & (GICH_LR_STATE | GICH_LR_HW | GICH_LR_EOI)) == GICH_LR_EOI) {
            eisr |= 1U << i;
        }
    }
    return eisr;
}

static uint32_t vif_elrsr(const gicv2_vif_t *v) {
    uint32_t elrsr = 0;
    for (uint32_t i = 0; i < GICV2_LIST_REGS; i++) {
        if ((v->lr[i] & GICH_LR_STATE) == 0 && (v->lr[i] & (GICH_LR_HW | GICH_LR_EOI)) != GICH_LR_EOI) {
            elrsr |= 1U << i;
        }
    }
    return elrsr;
}

static uint32_t vif_misr(const gicv2_vif_t *v) {
    uint32_t valid = 0;
    bool pending = false;
    for (uint32_t i = 0; i < GICV2_LIST_REGS; i++) {
        valid += (v->lr[i] & GICH_LR_STATE) != 0;
        pending |= (v->lr[i] & GICH_LR_PENDING) != 0;
    }
    bool grp0 = (v->vmcr & GICH_VMCR_GRP0EN) != 0;
    bool grp1 = (v->vmcr & GICH_VMCR_GRP1EN) != 0;
    uint32_t hcr = v->hcr;
    return (vif_eisr(v) != 0 ? 1U << 0 : 0U) |
           ((hcr & GICH_HCR_UIE) && valid <= 1U ? 1U << 1 : 0U) |
           ((hcr & GICH_HCR_LRENPIE) && (hcr >> 27) != 0 ? 1U << 2 : 0U) |
           ((hcr & GICH_HCR_NPIE) && !pending ? 1U << 3 : 0U) |
           ((hcr & GICH_HCR_VGRP0EIE) && grp0 ? 1U << 4 : 0U) |
           ((hcr & GICH_HCR_VGRP0DIE) && !grp0 ? 1U << 5 : 0U) |
           ((hcr & GICH_HCR_VGRP1EIE) && grp1 ? 1U << 6 : 0U) |
           ((hcr & GICH_HCR_VGRP1DIE) && !grp1 ? 1U << 7 : 0U);
}

static uint32_t gic_update_cpu_locked(gicv2_t *g, uint32_t cpu_id) {
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];
    const gicv2_vif_t *v = &c->vif;
    bool vif_enabled = (v->hcr & GICH_HCR_EN) != 0;
    if (vif_enabled && vif_misr(v) != 0) {
        c->level_local |= 1U << VMM_IRQ_GIC_MAINT;
    } else {
        c->level_local &= ~(1U << VMM_IRQ_GIC_MAINT);
    }
    uint32_t best_prio = GIC_IDLE_PRIO;
    uint32_t best_id = GIC_SPURIOUS_IRQ;

    if (g->st.dist_enabled && (c->ctlr & 1U)) {
        best_id = gic_highest_pending_locked(g, cpu_id, &best_prio);
        uint32_t run_prio = running_prio(c->apr);
        uint32_t limit = c->pmr < run_prio ? c->pmr : run_prio;
        if (best_id >= GIC_NUM_IRQS || best_prio >= limit) {
            best_id = GIC_SPURIOUS_IRQ;
        }
    }

    uint32_t lines = (best_id < GIC_NUM_IRQS ? VCPU_IRQ_PHYS : 0U) |
                     (vif_enabled && vif_highest_pending(v, true) >= 0 ? VCPU_IRQ_VIRT : 0U);
    _Atomic uint32_t *pending = &g->vm->cpus[cpu_id]->irq_pending;
    uint32_t old = atomic_load_explicit(pending, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(pending, &old, (old & ~(VCPU_IRQ_PHYS | VCPU_IRQ_VIRT)) | lines,
                                                  memory_order_acq_rel, memory_order_relaxed)) {
    }
    return (lines & ~old) != 0 ? (1U << cpu_id) : 0U;
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

gicv2_t *gicv2_create(vmm_machine_t *vm) {
    gicv2_t *g = calloc(1, sizeof(*g));
    if (!g) {
        return NULL;
    }
    pthread_mutex_init(&g->lock, NULL);
    g->vm = vm;
    for (uint32_t i = 0; i < GICV2_NUM_SPIS; i++) {
        g->st.spi_targets[i] = 0x01;
    }
    for (uint32_t c = 0; c < VMM_MAX_VCPUS; c++) {
        g->st.cpu[c].enabled_local = 0x0000ffffU;
    }
    return g;
}

void gicv2_destroy(gicv2_t *g) {
    if (!g) {
        return;
    }
    pthread_mutex_destroy(&g->lock);
    free(g);
}

void gicv2_set_irq(vmm_machine_t *vm, uint32_t intid, bool level) {
    gicv2_t *g = vmm_priv(vm)->gic;
    uint32_t idx = intid - 32U;
    uint32_t w = idx / 32U;
    uint32_t bit = 1U << (idx % 32U);

    pthread_mutex_lock(&g->lock);
    bool old = (g->st.spi_level[w] & bit) != 0;
    if (old == level) {
        pthread_mutex_unlock(&g->lock);
        return;
    }
    if (level) {
        g->st.spi_level[w] |= bit;
        g->st.spi_pending[w] |= bit;
    } else {
        g->st.spi_level[w] &= ~bit;
    }
    uint32_t wake_mask = gic_update_all_locked(g);
    pthread_mutex_unlock(&g->lock);

    gic_wake_cpus(vm, wake_mask);
}

void gicv2_set_timer_ppis(vmm_machine_t *vm, uint32_t cpu_id, uint32_t lines) {
    gicv2_t *g = vmm_priv(vm)->gic;
    const uint32_t mask = (1U << VMM_IRQ_TIMER_HYP_PHYS) | (1U << VMM_IRQ_TIMER_VIRT) |
                          (1U << VMM_IRQ_TIMER_HYP_VIRT) | (1U << VMM_IRQ_TIMER_PHYS);

    pthread_mutex_lock(&g->lock);
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];
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
                b = (irq < 32U) ? (uint8_t)(1U << cpu_id) : g->st.spi_targets[irq - 32U];
            } else {
                b = (irq < 32U) ? g->st.cpu[cpu_id].priority_local[irq] : g->st.spi_priority[irq - 32U];
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
                g->st.spi_targets[irq - 32U] = (uint8_t)(b & valid_cpus);
            }
        } else {
            if (irq < 32U) {
                g->st.cpu[cpu_id].priority_local[irq] = b;
            } else {
                g->st.spi_priority[irq - 32U] = b;
            }
        }
    }
}

uint64_t gicv2_dist_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint32_t size) {
    uint32_t cpu_id = cpu->cpu_id;
    uint32_t alias = offset & ~0x80U;
    uint32_t res = 0;

    pthread_mutex_lock(&g->lock);
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];

    if (offset == 0x000) {
        res = g->st.dist_enabled ? 1U : 0U;
    } else if (offset == 0x004) {
        res = (((g->vm->cpu_count - 1U) & 7U) << 5) | ((GIC_NUM_IRQS / 32U) - 1U);
    } else if (offset == 0x008) {
        res = 0x0200143bU;
    } else if (alias >= 0x100 && alias < 0x110) {
        uint32_t n = (alias - 0x100) / 4U;
        res = (n == 0) ? c->enabled_local : g->st.spi_enabled[n - 1U];
    } else if (alias >= 0x200 && alias < 0x210) {
        uint32_t n = (alias - 0x200) / 4U;
        res = (n == 0) ? (c->pending_local | c->level_local)
                       : (g->st.spi_pending[n - 1U] | g->st.spi_level[n - 1U]);
    } else if (alias >= 0x300 && alias < 0x310) {
        uint32_t n = (alias - 0x300) / 4U;
        res = (n == 0) ? c->active_local : g->st.spi_active[n - 1U];
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
            res = g->st.spi_icfgr[n - 2U];
        }
    } else if (offset >= 0xf10 && offset < 0xf30) {
        uint32_t base = (offset - 0xf10) & 0xfU;
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

void gicv2_dist_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint32_t size, uint64_t val) {
    uint32_t cpu_id = cpu->cpu_id;
    uint32_t v = (uint32_t)val;

    pthread_mutex_lock(&g->lock);
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];
    uint32_t wake_mask = 0;

    if (offset == 0x000) {
        g->st.dist_enabled = (v & 1U) != 0;
        wake_mask = gic_update_all_locked(g);
    } else if (offset >= 0x100 && offset < 0x110) {
        uint32_t n = (offset - 0x100) / 4U;
        if (n == 0) {
            c->enabled_local |= v | 0x0000ffffU;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_enabled[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x180 && offset < 0x190) {
        uint32_t n = (offset - 0x180) / 4U;
        if (n == 0) {
            c->enabled_local = (c->enabled_local & ~v) | 0x0000ffffU;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_enabled[n - 1U] &= ~v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x200 && offset < 0x210) {
        uint32_t n = (offset - 0x200) / 4U;
        if (n == 0) {
            c->pending_local |= (v & 0xffff0000U);
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_pending[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x280 && offset < 0x290) {
        uint32_t n = (offset - 0x280) / 4U;
        if (n == 0) {
            c->pending_local &= ~(v & 0xffff0000U);
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_pending[n - 1U] &= ~v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x300 && offset < 0x310) {
        uint32_t n = (offset - 0x300) / 4U;
        if (n == 0) {
            c->active_local |= v;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_active[n - 1U] |= v;
            wake_mask = gic_update_all_locked(g);
        }
    } else if (offset >= 0x380 && offset < 0x390) {
        uint32_t n = (offset - 0x380) / 4U;
        if (n == 0) {
            c->active_local &= ~v;
            wake_mask = gic_update_cpu_locked(g, cpu_id);
        } else {
            g->st.spi_active[n - 1U] &= ~v;
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
            g->st.spi_icfgr[n - 2U] = v & 0xaaaaaaaaU;
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
                g->st.cpu[t].sgi_pending[intid] |= (uint8_t)(1U << cpu_id);
                g->st.cpu[t].pending_local |= (1U << intid);
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

uint64_t gicv2_cpu_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset) {
    uint32_t cpu_id = cpu->cpu_id;
    uint32_t res = 0;
    uint32_t wake_mask = 0;

    pthread_mutex_lock(&g->lock);
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];

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
        uint32_t run_prio = running_prio(c->apr);
        uint32_t limit = c->pmr < run_prio ? c->pmr : run_prio;
        if (id >= GIC_NUM_IRQS || best_prio >= limit) {
            res = GIC_SPURIOUS_IRQ;
            break;
        }
        c->apr |= 1U << (best_prio >> GIC_PRIO_SHIFT);
        if (id < 16U) {
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
            g->st.spi_pending[w] &= ~bit;
            g->st.spi_active[w] |= bit;
            res = id;
            wake_mask = gic_update_all_locked(g);
        }
        break;
    }
    case 0x014:
        res = running_prio(c->apr);
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
    case 0x0d0:
        res = c->apr;
        break;
    case 0x0fc:
        res = 0x0202143bU;
        break;
    default:
        break;
    }

    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
    return res;
}

void gicv2_cpu_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val) {
    uint32_t cpu_id = cpu->cpu_id;
    uint32_t v = (uint32_t)val;
    uint32_t wake_mask = 0;

    pthread_mutex_lock(&g->lock);
    gicv2_cpu_state_t *c = &g->st.cpu[cpu_id];

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
        c->apr &= c->apr - 1U;
        if ((c->ctlr & GICC_CTLR_EOIMODE) == 0) {
            wake_mask = gic_deactivate_locked(g, cpu_id, v & 0x3ffU);
        }
        wake_mask |= gic_update_cpu_locked(g, cpu_id);
        break;
    case 0x0d0:
        c->apr = v;
        wake_mask = gic_update_cpu_locked(g, cpu_id);
        break;
    case 0x1000:
        wake_mask = gic_deactivate_locked(g, cpu_id, v & 0x3ffU);
        break;
    default:
        break;
    }

    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
}

static uint32_t vif_deactivate_locked(gicv2_t *g, uint32_t cpu_id, uint32_t id) {
    gicv2_vif_t *v = &g->st.cpu[cpu_id].vif;
    for (uint32_t i = 0; i < GICV2_LIST_REGS; i++) {
        uint32_t lr = v->lr[i];
        if ((lr & GICH_LR_ACTIVE) != 0 && (lr & 0x3FFU) == (id & 0x3FFU)) {
            v->lr[i] = lr & ~GICH_LR_ACTIVE;
            return (lr & GICH_LR_HW) != 0 ? gic_deactivate_locked(g, cpu_id, (lr >> 10) & 0x3FFU) : 0U;
        }
    }
    v->hcr = (v->hcr & ~(0x1FU << 27)) | ((v->hcr + GICH_HCR_EOICOUNT) & (0x1FU << 27));
    return 0;
}

uint64_t gicv2_hyp_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset) {
    uint32_t res = 0;
    pthread_mutex_lock(&g->lock);
    const gicv2_vif_t *v = &g->st.cpu[cpu->cpu_id].vif;
    switch (offset) {
    case 0x000: res = v->hcr; break;
    case 0x004: res = GICH_VTR; break;
    case 0x008: res = v->vmcr; break;
    case 0x010: res = vif_misr(v); break;
    case 0x020: res = vif_eisr(v); break;
    case 0x030: res = vif_elrsr(v); break;
    case 0x0f0: res = v->apr; break;
    default:
        if (offset >= 0x100U && offset < 0x100U + GICV2_LIST_REGS * 4U && (offset & 3U) == 0) {
            res = v->lr[(offset - 0x100U) / 4U];
        }
        break;
    }
    pthread_mutex_unlock(&g->lock);
    return res;
}

void gicv2_hyp_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val) {
    uint32_t v32 = (uint32_t)val;
    pthread_mutex_lock(&g->lock);
    gicv2_vif_t *v = &g->st.cpu[cpu->cpu_id].vif;
    switch (offset) {
    case 0x000: v->hcr = v32 & GICH_HCR_MASK; break;
    case 0x008: v->vmcr = v32 & GICH_VMCR_MASK; break;
    case 0x0f0: v->apr = v32; break;
    default:
        if (offset >= 0x100U && offset < 0x100U + GICV2_LIST_REGS * 4U && (offset & 3U) == 0) {
            v->lr[(offset - 0x100U) / 4U] = v32 & GICH_LR_MASK;
        }
        break;
    }
    uint32_t wake_mask = gic_update_cpu_locked(g, cpu->cpu_id);
    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
}

uint64_t gicv2_vcpu_read(gicv2_t *g, vcpu_t *cpu, uint32_t offset) {
    uint32_t res = 0;
    uint32_t wake_mask = 0;
    pthread_mutex_lock(&g->lock);
    gicv2_vif_t *v = &g->st.cpu[cpu->cpu_id].vif;
    switch (offset) {
    case 0x000: res = v->vmcr & GICH_VMCR_CTLR; break;
    case 0x004: res = (v->vmcr >> 27) << GIC_PRIO_SHIFT; break;
    case 0x008: res = (v->vmcr >> 21) & 7U; break;
    case 0x00c:
    case 0x020: {
        int i = vif_highest_pending(v, true);
        if (i < 0) {
            res = GIC_SPURIOUS_IRQ;
            break;
        }
        uint32_t lr = v->lr[i];
        v->lr[i] = (lr & ~GICH_LR_STATE) | GICH_LR_ACTIVE;
        v->apr |= 1U << (vif_group_prio(v, lr) >> GIC_PRIO_SHIFT);
        res = vif_lr_id(lr);
        wake_mask = gic_update_cpu_locked(g, cpu->cpu_id);
        break;
    }
    case 0x014: res = running_prio(v->apr); break;
    case 0x018:
    case 0x028: {
        int i = vif_highest_pending(v, false);
        res = i < 0 ? GIC_SPURIOUS_IRQ : vif_lr_id(v->lr[i]);
        break;
    }
    case 0x01c: res = (v->vmcr >> 18) & 7U; break;
    case 0x0d0: res = v->apr; break;
    case 0x0fc: res = 0x0202143bU; break;
    default: break;
    }
    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
    return res;
}

void gicv2_vcpu_write(gicv2_t *g, vcpu_t *cpu, uint32_t offset, uint64_t val) {
    uint32_t v32 = (uint32_t)val;
    uint32_t wake_mask = 0;
    pthread_mutex_lock(&g->lock);
    gicv2_vif_t *v = &g->st.cpu[cpu->cpu_id].vif;
    switch (offset) {
    case 0x000: v->vmcr = (v->vmcr & ~GICH_VMCR_CTLR) | (v32 & GICH_VMCR_CTLR); break;
    case 0x004: v->vmcr = (v->vmcr & ~(0x1FU << 27)) | ((v32 >> GIC_PRIO_SHIFT) & 0x1FU) << 27; break;
    case 0x008: v->vmcr = (v->vmcr & ~(7U << 21)) | (v32 & 7U) << 21; break;
    case 0x01c: v->vmcr = (v->vmcr & ~(7U << 18)) | (v32 & 7U) << 18; break;
    case 0x010:
    case 0x024:
        v->apr &= v->apr - 1U;
        if ((v->vmcr & GICH_VMCR_VEM) == 0) {
            wake_mask = vif_deactivate_locked(g, cpu->cpu_id, v32);
        }
        break;
    case 0x0d0: v->apr = v32; break;
    case 0x1000: wake_mask = vif_deactivate_locked(g, cpu->cpu_id, v32); break;
    default: break;
    }
    wake_mask |= gic_update_cpu_locked(g, cpu->cpu_id);
    pthread_mutex_unlock(&g->lock);
    gic_wake_cpus(g->vm, wake_mask);
}

void gicv2_save_state(gicv2_t *g, gicv2_state_t *out) {
    pthread_mutex_lock(&g->lock);
    *out = g->st;
    pthread_mutex_unlock(&g->lock);
}

void gicv2_load_state(gicv2_t *g, const gicv2_state_t *in) {
    pthread_mutex_lock(&g->lock);
    g->st = *in;
    (void)gic_update_all_locked(g);
    pthread_mutex_unlock(&g->lock);
}
