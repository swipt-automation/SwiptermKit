#include "board_internal.h"
#include "machine.h"

#include "../cpu/cpu_internal.h"
#include "../mmu/mmu.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/vm_map.h>
#endif

#define WFE_POLL_MIN_TICKS 24U
#define WFE_POLL_MAX_TICKS 480U
#define WFI_POLL_MIN_TICKS 48U
#define WFI_POLL_MAX_TICKS 1200U

#define VMM_SNAPSHOT_MAGIC 0x53575054534e4150ULL
#define VMM_SNAPSHOT_VERSION 5U
#define VMM_SNAPSHOT_PREBOOT 1U
#define VMM_SNAPSHOT_EL2 2U
#define VMM_SNAPSHOT_WRITE_MAX ((size_t)16 << 20)
#define ARM64_IMAGE_MAGIC 0x644d5241U

typedef struct vmm_snapshot_header {
    uint64_t magic;
    uint32_t version;
    uint32_t cpu_count;
    uint64_t ram_size;
    uint64_t timer_cntvct;
    uint32_t online_mask;
    uint32_t gic_state_size;
    uint32_t uart_state_size;
    uint32_t rtc_state_size;
    uint32_t flags;
    uint8_t key[32];
} vmm_snapshot_header_t;

typedef struct vmm_snapshot_vcpu {
    uint64_t x[31];
    uint64_t sp_el[2];
    uint64_t pc;
    uint32_t nzcv;
    uint32_t daif;
    uint8_t el;
    uint8_t spsel;
    uint8_t pan;
    uint8_t world;
    uint32_t fpcr;
    uint32_t fpsr;
    alignas(16) __uint128_t v[32];
    vcpu_bank_t bank;
    uint64_t tpidr_el0;
    uint64_t tpidrro_el0;
    uint64_t tpidr_el1;
    uint64_t tpidr_el2;
    uint64_t mdscr_el1;
    uint64_t par_el1;
    uint64_t mpidr_el1;
    vcpu_timer_t timers[VCPU_TIMER_COUNT];
    vcpu_sysreg_state_t sysregs;
    vcpu_el2_state_t el2;
} vmm_snapshot_vcpu_t;

typedef struct vmm_snapshot_footer {
    uint64_t state_offset;
    uint64_t magic;
} vmm_snapshot_footer_t;

static void vcpu_save_snapshot(const vcpu_t *cpu, vmm_snapshot_vcpu_t *out) {
    memset(out, 0, sizeof(*out));
    memcpy(out->x, cpu->x, sizeof(out->x));
    memcpy(out->sp_el, cpu->sp_el, sizeof(out->sp_el));
    out->pc = cpu->pc;
    out->nzcv = cpu->nzcv;
    out->daif = cpu->daif;
    out->el = cpu->el;
    out->spsel = cpu->spsel;
    out->pan = cpu->pan;
    out->world = cpu->world;
    out->fpcr = cpu->fpcr;
    out->fpsr = cpu->fpsr;
    memcpy(out->v, cpu->v, sizeof(out->v));
    out->bank = cpu->bank;
    out->tpidr_el0 = cpu->tpidr_el0;
    out->tpidrro_el0 = cpu->tpidrro_el0;
    out->tpidr_el1 = cpu->tpidr_el1;
    out->tpidr_el2 = cpu->tpidr_el2;
    out->mdscr_el1 = cpu->mdscr_el1;
    out->par_el1 = cpu->par_el1;
    out->mpidr_el1 = cpu->mpidr_el1;
    memcpy(out->timers, cpu->timers, sizeof(out->timers));
    out->sysregs = cpu->sysregs;
    out->el2 = cpu->el2;
}

static void vcpu_load_snapshot(vcpu_t *cpu, const vmm_snapshot_vcpu_t *in) {
    vcpu_set_world(cpu, in->world);
    memcpy(cpu->x, in->x, sizeof(in->x));
    cpu->x[31] = 0;
    cpu->x[32] = 0;
    memcpy(cpu->sp_el, in->sp_el, sizeof(cpu->sp_el));
    cpu->pc = in->pc;
    cpu->nzcv = in->nzcv;
    cpu->daif = in->daif;
    cpu->el = in->el;
    cpu->spsel = in->spsel;
    cpu->pan = in->pan;
    cpu->fast_sp = cpu->sp_el[cpu->el & cpu->spsel];
    cpu->excl_valid = false;
    cpu->exec_epoch++;
    cpu->ras_epoch++;
    cpu->fpcr = in->fpcr;
    cpu->fpsr = in->fpsr;
    memcpy(cpu->v, in->v, sizeof(cpu->v));
    cpu->bank = in->bank;
    cpu->tpidr_el0 = in->tpidr_el0;
    cpu->tpidrro_el0 = in->tpidrro_el0;
    cpu->tpidr_el1 = in->tpidr_el1;
    cpu->tpidr_el2 = in->tpidr_el2;
    cpu->mdscr_el1 = in->mdscr_el1;
    cpu->par_el1 = in->par_el1;
    cpu->mpidr_el1 = in->mpidr_el1;
    memcpy(cpu->timers, in->timers, sizeof(cpu->timers));
    cpu->sysregs = in->sysregs;
    cpu->el2 = in->el2;
    vcpu_sync_el2(cpu);
    mmu_tlbi_all(cpu);
    mmu_update_asid_salt(cpu);
    vcpu_flush_icache_all(cpu);
}

static int pwrite_all(int fd, const void *buf, size_t len, off_t off) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = pwrite(fd, p, left, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -errno;
        }
        if (n == 0) {
            return -EIO;
        }
        p += n;
        left -= (size_t)n;
        off += n;
    }
    return 0;
}

static int pread_all(int fd, void *buf, size_t len, off_t off) {
    uint8_t *p = (uint8_t *)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = pread(fd, p, left, off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -errno;
        }
        if (n == 0) {
            return -EIO;
        }
        p += n;
        left -= (size_t)n;
        off += n;
    }
    return 0;
}

static bool page_is_zero(const uint8_t *p, size_t len) {
    const uint64_t *w = (const uint64_t *)p;
    size_t words = len / sizeof(uint64_t);
    for (size_t i = 0; i < words; i++) {
        if (w[i] != 0) {
            return false;
        }
    }
    return true;
}

static int snapshot_write_span(int fd, const uint8_t *ram, size_t start, size_t end) {
    size_t run_start = start;
    size_t run_len = 0;
    for (size_t off = start; off < end; off += (size_t)VMM_RAM_ALIGN) {
        size_t n = end - off < (size_t)VMM_RAM_ALIGN ? end - off : (size_t)VMM_RAM_ALIGN;
        bool zero = page_is_zero(ram + off, n);
        if (!zero) {
            if (run_len == 0) {
                run_start = off;
            }
            run_len += n;
        }
        if (run_len > 0 && (zero || run_len >= VMM_SNAPSHOT_WRITE_MAX || off + n >= end)) {
            int rc = pwrite_all(fd, ram + run_start, run_len, (off_t)run_start);
            if (rc != 0) {
                return rc;
            }
            run_len = 0;
        }
    }
    return 0;
}

static int snapshot_write_ram(int fd, const vmm_machine_t *vm) {
#if defined(__APPLE__)
    vm_address_t base = (vm_address_t)vm->ram;
    vm_address_t end = base + vm->ram_size;
    vm_address_t addr = base;
    natural_t depth = 0;
    while (addr < end) {
        vm_address_t region = addr;
        vm_size_t region_size = 0;
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(mach_task_self(), &region, &region_size, &depth,
                                 (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS ||
            region >= end) {
            return snapshot_write_span(fd, vm->ram, addr - base, vm->ram_size);
        }
        if (info.is_submap) {
            depth++;
            continue;
        }
        vm_address_t span_start = region > addr ? region : addr;
        vm_address_t span_end = region + region_size < end ? region + region_size : end;
        bool untouched = !info.external_pager && info.pages_resident == 0 && info.pages_swapped_out == 0;
        if (!untouched) {
            int rc = snapshot_write_span(fd, vm->ram, span_start - base, span_end - base);
            if (rc != 0) {
                return rc;
            }
        }
        addr = span_end;
    }
    return 0;
#else
    return snapshot_write_span(fd, vm->ram, 0, vm->ram_size);
#endif
}

static void *vcpu_thread_main(void *arg) {
    vcpu_t *cpu = (vcpu_t *)arg;
    vmm_machine_t *vm = cpu->vm;
    vmm_machine_priv_t *priv = vmm_priv(vm);
    uint32_t id = cpu->cpu_id;

    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

    atomic_store_explicit(&cpu->halted_wfi, false, memory_order_seq_cst);
    mmu_poll_shootdown(cpu);
    vmm_timer_sync(cpu);

    while (atomic_load_explicit(&vm->running, memory_order_acquire) &&
           atomic_load_explicit(&priv->cpu_online[id], memory_order_acquire)) {
        if (atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
            atomic_store_explicit(&cpu->halted_wfi, true, memory_order_release);
            pthread_mutex_lock(&priv->state_lock);
            priv->suspended_count++;
            pthread_cond_broadcast(&priv->suspend_cond);
            while (atomic_load_explicit(&vm->suspended, memory_order_acquire) &&
                   atomic_load_explicit(&vm->running, memory_order_acquire)) {
                pthread_cond_wait(&priv->suspend_cond, &priv->state_lock);
            }
            priv->suspended_count--;
            pthread_mutex_unlock(&priv->state_lock);
            if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
                break;
            }
            atomic_store_explicit(&cpu->stop_requested, false, memory_order_release);
            atomic_store_explicit(&cpu->halted_wfi, false, memory_order_seq_cst);
            mmu_poll_shootdown(cpu);
            vmm_timer_sync(cpu);
        }

        vcpu_stop_reason_t reason = vcpu_run(cpu);
        if (reason == VCPU_STOP_SUSPEND) {
            if (atomic_load_explicit(&priv->preboot_phase, memory_order_acquire) == VMM_PREBOOT_REACHED &&
                !atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
                pthread_mutex_lock(&priv->state_lock);
                while (atomic_load_explicit(&priv->preboot_phase, memory_order_acquire) == VMM_PREBOOT_REACHED &&
                       !atomic_load_explicit(&vm->suspended, memory_order_acquire) &&
                       atomic_load_explicit(&vm->running, memory_order_acquire)) {
                    pthread_cond_wait(&priv->suspend_cond, &priv->state_lock);
                }
                pthread_mutex_unlock(&priv->state_lock);
            }
            continue;
        }
        if (reason == VCPU_STOP_HALT || reason == VCPU_STOP_REBOOT) {
            atomic_store_explicit(&priv->cpu_online[id], false, memory_order_release);
            break;
        }
        if (reason == VCPU_STOP_PANIC) {
            atomic_store_explicit(&vm->running, false, memory_order_release);
            for (uint32_t i = 0; i < vm->cpu_count; i++) {
                if (vm->cpus[i]) {
                    atomic_store_explicit(&vm->cpus[i]->stop_requested, true, memory_order_release);
                    vmm_vcpu_wake(vm, i);
                }
            }
            if (vm->on_panic) {
                vm->on_panic(vm->callback_ctx, "vCPU panic");
            }
            break;
        }
        if (atomic_load_explicit(&cpu->stop_requested, memory_order_acquire) &&
            !atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
            break;
        }
    }

    atomic_store_explicit(&cpu->halted_wfi, true, memory_order_release);
    pthread_mutex_lock(&priv->state_lock);
    pthread_cond_broadcast(&priv->suspend_cond);
    pthread_mutex_unlock(&priv->state_lock);
    return NULL;
}

void vmm_vcpu_wake(vmm_machine_t *vm, uint32_t cpu_id) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || cpu_id >= vm->cpu_count) {
        return;
    }
    vcpu_t *cpu = vm->cpus[cpu_id];
    atomic_store_explicit(&cpu->event_reg, true, memory_order_release);
    vmm_host_sev();
    if (!atomic_load_explicit(&cpu->halted_wfi, memory_order_seq_cst)) {
        return;
    }
    pthread_mutex_lock(&priv->wfi_lock[cpu_id]);
    pthread_cond_signal(&priv->wfi_cond[cpu_id]);
    pthread_mutex_unlock(&priv->wfi_lock[cpu_id]);
    pthread_mutex_lock(&priv->wfe_lock[cpu_id]);
    pthread_cond_signal(&priv->wfe_cond[cpu_id]);
    pthread_mutex_unlock(&priv->wfe_lock[cpu_id]);
}

static inline void host_monitor_arm(const void *p) {
    uint32_t tmp;
    __asm__ __volatile__("ldaxrb %w0, [%1]" : "=r"(tmp) : "r"(p) : "memory");
}

static uint32_t poll_budget_next(uint32_t budget, uint64_t blocked, uint32_t min, uint32_t max) {
    if (blocked <= max) {
        return budget * 2U < max ? budget * 2U : max;
    }
    return budget / 2U > min ? budget / 2U : min;
}

static bool wfe_excl_changed(const vcpu_t *cpu) {
    if (!cpu->excl_valid || cpu->excl_hva == 0) {
        return false;
    }
    const void *p = (const void *)cpu->excl_hva;
    switch (cpu->excl_size) {
    case 1: return __atomic_load_n((const uint8_t *)p, __ATOMIC_ACQUIRE) != (uint8_t)cpu->excl_val;
    case 2: return __atomic_load_n((const uint16_t *)p, __ATOMIC_ACQUIRE) != (uint16_t)cpu->excl_val;
    case 4: return __atomic_load_n((const uint32_t *)p, __ATOMIC_ACQUIRE) != (uint32_t)cpu->excl_val;
    case 8: return __atomic_load_n((const uint64_t *)p, __ATOMIC_ACQUIRE) != (uint64_t)cpu->excl_val;
    case 16:
        return __atomic_load_n((const uint64_t *)p, __ATOMIC_ACQUIRE) != (uint64_t)cpu->excl_val ||
               __atomic_load_n((const uint64_t *)p + 1, __ATOMIC_ACQUIRE) != (uint64_t)(cpu->excl_val >> 64);
    default: return false;
    }
}

void vmm_vcpu_wfe(vcpu_t *cpu) {
    if (!cpu || !cpu->vm) {
        return;
    }
    vmm_machine_t *vm = cpu->vm;
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || cpu->cpu_id >= vm->cpu_count || vm->cpu_count <= 1) {
        return;
    }

    const void *watch = (cpu->excl_valid && cpu->excl_hva != 0) ? (const void *)cpu->excl_hva
                                                                 : (const void *)&cpu->irq_pending;
    uint32_t budget = cpu->wfe_poll_ticks < WFE_POLL_MIN_TICKS ? WFE_POLL_MIN_TICKS : cpu->wfe_poll_ticks;
    uint64_t poll_start = mach_absolute_time();
    for (;;) {
        mmu_poll_shootdown(cpu);
        host_monitor_arm(watch);
        bool ev = atomic_load_explicit(&cpu->event_reg, memory_order_acquire);
        if (ev || wfe_excl_changed(cpu)) {
            __asm__ __volatile__("clrex" ::: "memory");
            if (ev) {
                atomic_store_explicit(&cpu->event_reg, false, memory_order_release);
            }
            cpu->excl_valid = false;
            cpu->excl_hva = 0;
            return;
        }
        if ((atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) & cpu->irq_mask) ||
            atomic_load_explicit(&cpu->stop_requested, memory_order_acquire) ||
            atomic_load_explicit(&vm->suspended, memory_order_acquire) ||
            !atomic_load_explicit(&vm->running, memory_order_acquire)) {
            __asm__ __volatile__("clrex" ::: "memory");
            return;
        }
        if (mach_absolute_time() - poll_start >= budget) {
            break;
        }
        // XNU arm64 enables SCTLR_EL1.nTWE and ~1us CNTKCTL_EL1.EVNTEN WFE event stream for EL0.
        __asm__ __volatile__("wfe" ::: "memory");
    }
    __asm__ __volatile__("clrex" ::: "memory");

    uint32_t id = cpu->cpu_id;
    uintptr_t watch_line = (cpu->excl_valid && cpu->excl_hva != 0) ? (cpu->excl_hva & ~(uintptr_t)63U) : 0;
    atomic_store_explicit(&cpu->wfe_watch_hva, watch_line, memory_order_release);
    atomic_fetch_add_explicit(&vm->wfe_waiters, 1, memory_order_seq_cst);

    if (wfe_excl_changed(cpu) ||
        atomic_exchange_explicit(&cpu->event_reg, false, memory_order_acq_rel)) {
        atomic_fetch_sub_explicit(&vm->wfe_waiters, 1, memory_order_acq_rel);
        atomic_store_explicit(&cpu->wfe_watch_hva, 0, memory_order_release);
        cpu->excl_valid = false;
        cpu->excl_hva = 0;
        return;
    }

    atomic_store_explicit(&cpu->halted_wfi, true, memory_order_seq_cst);
    atomic_thread_fence(memory_order_seq_cst);
    mmu_poll_shootdown(cpu);

    uint64_t block_start = mach_absolute_time();
    pthread_mutex_lock(&priv->wfe_lock[id]);
    if (!atomic_exchange_explicit(&cpu->event_reg, false, memory_order_acq_rel) &&
        !wfe_excl_changed(cpu) &&
        !(atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) & cpu->irq_mask) &&
        !atomic_load_explicit(&cpu->stop_requested, memory_order_acquire) &&
        !atomic_load_explicit(&vm->suspended, memory_order_acquire) &&
        atomic_load_explicit(&vm->running, memory_order_acquire)) {
        pthread_cond_timedwait_relative_np(&priv->wfe_cond[id], &priv->wfe_lock[id],
                                           &(struct timespec){.tv_nsec = 200000L});
        atomic_store_explicit(&cpu->event_reg, false, memory_order_release);
    }
    pthread_mutex_unlock(&priv->wfe_lock[id]);
    cpu->wfe_poll_ticks = poll_budget_next(budget, mach_absolute_time() - block_start,
                                           WFE_POLL_MIN_TICKS, WFE_POLL_MAX_TICKS);

    atomic_fetch_sub_explicit(&vm->wfe_waiters, 1, memory_order_acq_rel);
    atomic_store_explicit(&cpu->wfe_watch_hva, 0, memory_order_release);
    atomic_store_explicit(&cpu->halted_wfi, false, memory_order_seq_cst);
    mmu_poll_shootdown(cpu);
}

void vmm_vcpu_sev(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return;
    }
    for (uint32_t i = 0; i < vm->cpu_count && i < VMM_MAX_VCPUS; i++) {
        if (vm->cpus[i]) {
            atomic_store_explicit(&vm->cpus[i]->event_reg, true, memory_order_release);
        }
    }
    vmm_host_sev();
    if (atomic_load_explicit(&vm->wfe_waiters, memory_order_acquire) == 0) {
        return;
    }
    for (uint32_t i = 0; i < vm->cpu_count && i < VMM_MAX_VCPUS; i++) {
        if (vm->cpus[i]) {
            pthread_mutex_lock(&priv->wfe_lock[i]);
            pthread_cond_signal(&priv->wfe_cond[i]);
            pthread_mutex_unlock(&priv->wfe_lock[i]);
        }
    }
}

void vmm_vcpu_wfe_notify_write(vmm_machine_t *vm, uintptr_t hva) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return;
    }
    uintptr_t line = hva & ~(uintptr_t)63U;
    for (uint32_t i = 0; i < vm->cpu_count && i < VMM_MAX_VCPUS; i++) {
        vcpu_t *c = vm->cpus[i];
        if (!c) {
            continue;
        }
        uintptr_t watch = atomic_load_explicit(&c->wfe_watch_hva, memory_order_acquire);
        if (watch != 0 && watch == line) {
            atomic_store_explicit(&c->event_reg, true, memory_order_release);
            pthread_mutex_lock(&priv->wfe_lock[i]);
            pthread_cond_signal(&priv->wfe_cond[i]);
            pthread_mutex_unlock(&priv->wfe_lock[i]);
        }
    }
}

void vmm_vcpu_wfi(vcpu_t *cpu) {
    if (!cpu || !cpu->vm) {
        return;
    }
    vmm_machine_t *vm = cpu->vm;
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || cpu->cpu_id >= vm->cpu_count) {
        return;
    }

    uint32_t id = cpu->cpu_id;

    uint32_t budget = cpu->wfi_poll_ticks < WFI_POLL_MIN_TICKS ? WFI_POLL_MIN_TICKS : cpu->wfi_poll_ticks;
    uint64_t poll_start = mach_absolute_time();
    for (;;) {
        mmu_poll_shootdown(cpu);
        host_monitor_arm(&cpu->irq_pending);
        if ((atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) & cpu->irq_mask) ||
            atomic_load_explicit(&cpu->stop_requested, memory_order_acquire) ||
            atomic_load_explicit(&vm->suspended, memory_order_acquire) ||
            !atomic_load_explicit(&vm->running, memory_order_acquire)) {
            __asm__ __volatile__("clrex" ::: "memory");
            return;
        }
        if (mach_absolute_time() - poll_start >= budget) {
            break;
        }
        // XNU arm64 enables SCTLR_EL1.nTWE and ~1us CNTKCTL_EL1.EVNTEN WFE event stream for EL0.
        __asm__ __volatile__("wfe" ::: "memory");
    }
    __asm__ __volatile__("clrex" ::: "memory");

    atomic_store_explicit(&cpu->halted_wfi, true, memory_order_seq_cst);
    atomic_thread_fence(memory_order_seq_cst);
    mmu_poll_shootdown(cpu);

    uint64_t block_start = mach_absolute_time();
    pthread_mutex_lock(&priv->wfi_lock[id]);
    while (!(atomic_load_explicit(&cpu->irq_pending, memory_order_acquire) & cpu->irq_mask) &&
           !atomic_load_explicit(&cpu->stop_requested, memory_order_acquire) &&
           !atomic_load_explicit(&vm->suspended, memory_order_acquire) &&
           atomic_load_explicit(&vm->running, memory_order_acquire)) {
        pthread_cond_wait(&priv->wfi_cond[id], &priv->wfi_lock[id]);
    }
    pthread_mutex_unlock(&priv->wfi_lock[id]);
    cpu->wfi_poll_ticks = poll_budget_next(budget, mach_absolute_time() - block_start,
                                           WFI_POLL_MIN_TICKS, WFI_POLL_MAX_TICKS);

    atomic_store_explicit(&cpu->halted_wfi, false, memory_order_seq_cst);
    mmu_poll_shootdown(cpu);
}

static void vmm_notify_halt_once(vmm_machine_priv_t *priv) {
    bool should_call = false;
    pthread_mutex_lock(&priv->state_lock);
    if (!priv->halt_notified) {
        priv->halt_notified = true;
        should_call = true;
    }
    pthread_mutex_unlock(&priv->state_lock);
    if (should_call && priv->pub.on_halt) {
        priv->pub.on_halt(priv->pub.callback_ctx);
    }
}

bool vmm_handle_psci(vcpu_t *cpu) {
    if (!cpu || !cpu->vm) {
        return false;
    }
    vmm_machine_t *vm = cpu->vm;
    vmm_machine_priv_t *priv = vmm_priv(vm);
    uint32_t func_id = (uint32_t)cpu->x[0];

    switch (func_id) {
    case SMCCC_VERSION:
        cpu->x[0] = SMCCC_VERSION_1_2;
        return true;

    case SMCCC_ARCH_FEATURES:
        switch ((uint32_t)cpu->x[1]) {
        case SMCCC_VERSION:
        case SMCCC_ARCH_FEATURES:
        case SMCCC_ARCH_SOC_ID:
            cpu->x[0] = (uint64_t)SMCCC_RET_SUCCESS;
            break;
        case SMCCC_ARCH_WORKAROUND_1:
        case SMCCC_ARCH_WORKAROUND_2:
        case SMCCC_ARCH_WORKAROUND_3:
            cpu->x[0] = SMCCC_ARCH_WORKAROUND_RET_UNAFFECTED;
            break;
        default:
            cpu->x[0] = (uint64_t)SMCCC_RET_NOT_SUPPORTED;
            break;
        }
        return true;

    case SMCCC_ARCH_SOC_ID: {
        uint32_t cpufamily = 0;
        size_t len = sizeof(cpufamily);
        sysctlbyname("hw.cpufamily", &cpufamily, &len, NULL, 0);
        switch ((uint32_t)cpu->x[1]) {
        case 0:
            cpu->x[0] = (JEP106_APPLE << 16) | (cpufamily >> 16);
            break;
        case 1:
            cpu->x[0] = cpufamily & 0xFFFFU;
            break;
        default:
            cpu->x[0] = (uint64_t)SMCCC_RET_INVALID_PARAMETER;
            break;
        }
        return true;
    }

    case SMCCC_TRNG_VERSION:
        cpu->x[0] = SMCCC_TRNG_VERSION_1_0;
        return true;

    case SMCCC_TRNG_FEATURES:
        switch ((uint32_t)cpu->x[1]) {
        case SMCCC_TRNG_VERSION:
        case SMCCC_TRNG_FEATURES:
        case SMCCC_TRNG_GET_UUID:
        case SMCCC_TRNG_RND32:
        case SMCCC_TRNG_RND64:
            cpu->x[0] = (uint64_t)SMCCC_RET_SUCCESS;
            break;
        default:
            cpu->x[0] = (uint64_t)TRNG_RET_NOT_SUPPORTED;
            break;
        }
        return true;

    case SMCCC_TRNG_GET_UUID:
        cpu->x[0] = 0xCA1A8AF3U;
        cpu->x[1] = 0x8A48FB9AU;
        cpu->x[2] = 0xB76E669CU;
        cpu->x[3] = 0x3A45579CU;
        return true;

    case SMCCC_TRNG_RND32:
    case SMCCC_TRNG_RND64: {
        uint32_t reg_bits = (func_id == SMCCC_TRNG_RND64) ? 64U : 32U;
        uint64_t bits = (func_id == SMCCC_TRNG_RND64) ? cpu->x[1] : (uint32_t)cpu->x[1];
        if (bits == 0 || bits > reg_bits * 3U) {
            cpu->x[0] = (uint64_t)TRNG_RET_INVALID_PARAMETERS;
            return true;
        }
        uint64_t entropy[3];
        arc4random_buf(entropy, sizeof(entropy));
        for (uint32_t i = 0; i < 3; i++) {
            uint64_t remaining = bits > (uint64_t)i * reg_bits ? bits - (uint64_t)i * reg_bits : 0;
            uint64_t width = remaining < reg_bits ? remaining : reg_bits;
            cpu->x[3 - i] = entropy[i] & (width == 64 ? UINT64_MAX : (1ULL << width) - 1ULL);
        }
        cpu->x[0] = (uint64_t)SMCCC_RET_SUCCESS;
        return true;
    }

    case SMCCC_VENDOR_HYP_CALL_UID:
        cpu->x[0] = 0xB66FB428U;
        cpu->x[1] = 0xE911C52EU;
        cpu->x[2] = 0x564BCAA9U;
        cpu->x[3] = 0x743A004DU;
        return true;

    case SMCCC_KVM_FEATURES:
        cpu->x[0] = (1U << SMCCC_KVM_FUNC_FEATURES) | (1U << SMCCC_KVM_FUNC_PTP);
        cpu->x[1] = 0;
        cpu->x[2] = 0;
        cpu->x[3] = 0;
        return true;

    case SMCCC_KVM_PTP: {
        uint32_t counter_id = (uint32_t)cpu->x[1];
        if (counter_id != SMCCC_KVM_PTP_VIRT_COUNTER && counter_id != SMCCC_KVM_PTP_PHYS_COUNTER) {
            cpu->x[0] = (uint64_t)SMCCC_RET_NOT_SUPPORTED;
            return true;
        }
        uint64_t realtime_ns;
        uint64_t cycles;
        vmm_timer_sample_realtime(vm, &realtime_ns, &cycles);
        if (counter_id == SMCCC_KVM_PTP_VIRT_COUNTER) {
            cycles -= cpu->cntvct_base - cpu->cntpct_base;
        }
        cpu->x[0] = realtime_ns >> 32;
        cpu->x[1] = (uint32_t)realtime_ns;
        cpu->x[2] = cycles >> 32;
        cpu->x[3] = (uint32_t)cycles;
        return true;
    }

    case PSCI_FN_VERSION:
        cpu->x[0] = PSCI_VERSION_1_1;
        return true;

    case PSCI_FN_FEATURES: {
        uint32_t fid = (uint32_t)cpu->x[1];
        switch (fid) {
        case SMCCC_VERSION:
        case PSCI_FN_VERSION:
        case PSCI_FN_CPU_SUSPEND:
        case PSCI_FN64_CPU_SUSPEND:
        case PSCI_FN_CPU_OFF:
        case PSCI_FN_CPU_ON:
        case PSCI_FN64_CPU_ON:
        case PSCI_FN_AFFINITY_INFO:
        case PSCI_FN64_AFFINITY_INFO:
        case PSCI_FN_MIGRATE_INFO_TYPE:
        case PSCI_FN_SYSTEM_OFF:
        case PSCI_FN_SYSTEM_RESET:
        case PSCI_FN_SYSTEM_RESET2:
        case PSCI_FN64_SYSTEM_RESET2:
        case PSCI_FN_FEATURES:
            cpu->x[0] = 0;
            break;
        default:
            cpu->x[0] = (uint64_t)PSCI_RET_NOT_SUPPORTED;
            break;
        }
        return true;
    }

    case PSCI_FN_MIGRATE_INFO_TYPE:
        cpu->x[0] = 2;
        return true;

    case PSCI_FN_CPU_SUSPEND:
    case PSCI_FN64_CPU_SUSPEND:
        vmm_vcpu_wfi(cpu);
        cpu->x[0] = (uint64_t)PSCI_RET_SUCCESS;
        return true;

    case PSCI_FN_CPU_ON:
    case PSCI_FN64_CPU_ON: {
        if (!priv) {
            cpu->x[0] = (uint64_t)PSCI_RET_INTERNAL_FAILURE;
            return true;
        }
        uint64_t target_mpidr = cpu->x[1];
        uint64_t entry = cpu->x[2];
        uint64_t ctx_id = cpu->x[3];
        uint32_t target_id = (uint32_t)(target_mpidr & 0xffULL);
        if ((target_mpidr & ~0xffULL) != 0 || target_id >= vm->cpu_count) {
            cpu->x[0] = (uint64_t)PSCI_RET_INVALID_PARAMS;
            return true;
        }
        if (!mmu_gpa_to_hva(vm, entry, 4)) {
            cpu->x[0] = (uint64_t)PSCI_RET_INVALID_ADDRESS;
            return true;
        }

        pthread_mutex_lock(&priv->state_lock);
        if (atomic_load_explicit(&priv->cpu_online[target_id], memory_order_acquire)) {
            pthread_mutex_unlock(&priv->state_lock);
            cpu->x[0] = (uint64_t)PSCI_RET_ALREADY_ON;
            return true;
        }
        if (priv->thread_started[target_id]) {
            pthread_t old_thr = priv->threads[target_id];
            priv->thread_started[target_id] = false;
            pthread_mutex_unlock(&priv->state_lock);
            pthread_join(old_thr, NULL);
            pthread_mutex_lock(&priv->state_lock);
        }

        vcpu_t *target = vm->cpus[target_id];
        vcpu_reset(target, entry, ctx_id);
        mmu_tlbi_all(target);
        mmu_update_asid_salt(target);
        target->stop_reason = VCPU_STOP_NONE;
        atomic_store_explicit(&target->stop_requested, false, memory_order_release);
        atomic_store_explicit(&target->halted_wfi, false, memory_order_release);
        atomic_store_explicit(&priv->cpu_online[target_id], true, memory_order_release);

        int rc = 0;
        if (atomic_load_explicit(&vm->running, memory_order_acquire)) {
            rc = pthread_create(&priv->threads[target_id], NULL, vcpu_thread_main, target);
            if (rc == 0) {
                priv->thread_started[target_id] = true;
            } else {
                atomic_store_explicit(&priv->cpu_online[target_id], false, memory_order_release);
            }
        }
        pthread_mutex_unlock(&priv->state_lock);

        cpu->x[0] = (uint64_t)(rc == 0 ? PSCI_RET_SUCCESS : PSCI_RET_INTERNAL_FAILURE);
        return true;
    }

    case PSCI_FN_CPU_OFF:
        cpu->stop_reason = VCPU_STOP_HALT;
        atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
        atomic_store_explicit(&cpu->halted_wfi, true, memory_order_release);
        if (priv && cpu->cpu_id < vm->cpu_count) {
            atomic_store_explicit(&priv->cpu_online[cpu->cpu_id], false, memory_order_release);
            vmm_timer_sync(cpu);
        }
        cpu->x[0] = (uint64_t)PSCI_RET_SUCCESS;
        return true;

    case PSCI_FN_AFFINITY_INFO:
    case PSCI_FN64_AFFINITY_INFO: {
        uint64_t target_mpidr = cpu->x[1];
        uint32_t target_id = (uint32_t)(target_mpidr & 0xffULL);
        if ((target_mpidr & ~0xffULL) != 0 || target_id >= vm->cpu_count) {
            cpu->x[0] = (uint64_t)PSCI_RET_INVALID_PARAMS;
            return true;
        }
        bool online = priv && atomic_load_explicit(&priv->cpu_online[target_id], memory_order_acquire);
        cpu->x[0] = online ? 0ULL : 1ULL;
        return true;
    }

    case PSCI_FN_SYSTEM_OFF:
    case PSCI_FN_SYSTEM_RESET:
    case PSCI_FN_SYSTEM_RESET2:
    case PSCI_FN64_SYSTEM_RESET2:
        cpu->stop_reason = (func_id == PSCI_FN_SYSTEM_OFF) ? VCPU_STOP_HALT : VCPU_STOP_REBOOT;
        atomic_store_explicit(&vm->running, false, memory_order_release);
        for (uint32_t i = 0; i < vm->cpu_count; i++) {
            if (vm->cpus[i]) {
                atomic_store_explicit(&vm->cpus[i]->stop_requested, true, memory_order_release);
                vmm_vcpu_wake(vm, i);
            }
        }
        if (priv) {
            vmm_notify_halt_once(priv);
        }
        cpu->x[0] = (uint64_t)PSCI_RET_SUCCESS;
        return true;

    default:
        return false;
    }
}

static void preboot_return(vcpu_t *cpu) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    cpu->x[0] = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
    uint64_t seed[4];
    arc4random_buf(seed, sizeof(seed));
    for (size_t i = 0; i < 4; i++) {
        cpu->x[1 + i] = seed[i];
    }
    cpu->pc += 4;
}

bool vmm_preboot_trap(vcpu_t *cpu) {
    vmm_machine_priv_t *priv = vmm_priv(cpu->vm);
    if (!priv) {
        return false;
    }
    pthread_mutex_lock(&priv->state_lock);
    vmm_preboot_phase_t phase = atomic_load(&priv->preboot_phase);
    if (phase == VMM_PREBOOT_ARMED || phase == VMM_PREBOOT_REACHED) {
        cpu->stop_reason = VCPU_STOP_SUSPEND;
        atomic_store(&cpu->stop_requested, true);
        atomic_store(&priv->preboot_phase, VMM_PREBOOT_REACHED);
        pthread_cond_broadcast(&priv->suspend_cond);
    } else if (phase == VMM_PREBOOT_RELEASED || phase == VMM_PREBOOT_DISABLED) {
        atomic_store(&priv->preboot_phase, VMM_PREBOOT_COMPLETE);
        preboot_return(cpu);
    }
    pthread_mutex_unlock(&priv->state_lock);
    return phase != VMM_PREBOOT_COMPLETE;
}

bool vmm_is_warm_boot(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    return priv ? priv->restored_from_snapshot : false;
}

int vmm_finish_preboot(vmm_machine_t *vm, const char *snapshot_path, int *save_error) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (save_error) {
        *save_error = 0;
    }
    if (!priv || priv->restored_from_snapshot) {
        return 0;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 30;
    pthread_mutex_lock(&priv->state_lock);
    while (atomic_load(&priv->preboot_phase) == VMM_PREBOOT_ARMED && atomic_load(&vm->running)) {
        int err = pthread_cond_timedwait(&priv->suspend_cond, &priv->state_lock, &deadline);
        if (err != 0) {
            pthread_mutex_unlock(&priv->state_lock);
            return -err;
        }
    }
    bool reached = atomic_load(&priv->preboot_phase) == VMM_PREBOOT_REACHED;
    pthread_mutex_unlock(&priv->state_lock);
    if (!reached || !atomic_load(&vm->running)) {
        return -EIO;
    }

    int rc = vmm_suspend(vm);
    if (rc < 0) {
        return rc;
    }
    if (snapshot_path && snapshot_path[0] != '\0') {
        int serr = vmm_save_snapshot(vm, snapshot_path);
        if (save_error) {
            *save_error = serr;
        }
    }
    atomic_store(&priv->preboot_phase, VMM_PREBOOT_RELEASED);
    return vmm_resume(vm);
}

uint64_t vmm_mmio_read(vmm_machine_t *vm, vcpu_t *cpu, uint64_t gpa, uint32_t size) {
    if (!vm) {
        return 0;
    }
    if (gpa >= VMM_GICD_BASE && gpa < VMM_GICD_BASE + VMM_GICD_SIZE) {
        return gicv2_dist_read(vm->gic, cpu, (uint32_t)(gpa - VMM_GICD_BASE), size);
    }
    if (gpa >= VMM_GICC_BASE && gpa < VMM_GICC_BASE + VMM_GICC_SIZE) {
        return gicv2_cpu_read(vm->gic, cpu, (uint32_t)(gpa - VMM_GICC_BASE), size);
    }
    if (gpa >= VMM_GICH_BASE && gpa < VMM_GICH_BASE + VMM_GICH_SIZE) {
        return gicv2_hyp_read(vm->gic, cpu, (uint32_t)(gpa - VMM_GICH_BASE));
    }
    if (gpa >= VMM_GICV_BASE && gpa < VMM_GICV_BASE + VMM_GICV_SIZE) {
        return gicv2_vcpu_read(vm->gic, cpu, (uint32_t)(gpa - VMM_GICV_BASE));
    }
    if (gpa >= VMM_UART0_BASE && gpa < VMM_UART0_BASE + VMM_UART0_SIZE) {
        return pl011_read(vm->uart, (uint32_t)(gpa - VMM_UART0_BASE), size);
    }
    if (gpa >= VMM_RTC_BASE && gpa < VMM_RTC_BASE + VMM_RTC_SIZE) {
        return pl031_read(vm->rtc, (uint32_t)(gpa - VMM_RTC_BASE), size);
    }
    if (gpa >= VMM_VIRTIO_MMIO_BASE && gpa < VMM_VIRTIO_MMIO_BASE + VMM_VIRTIO_MMIO_SIZE) {
        uint64_t rel = gpa - VMM_VIRTIO_MMIO_BASE;
        uint32_t slot = (uint32_t)(rel / VMM_VIRTIO_MMIO_STRIDE);
        uint32_t off = (uint32_t)(rel % VMM_VIRTIO_MMIO_STRIDE);
        vmm_preboot_phase_t phase = atomic_load(&vmm_priv(vm)->preboot_phase);
        if (slot < VMM_VIRTIO_MMIO_SLOTS && phase != VMM_PREBOOT_ARMED &&
            phase != VMM_PREBOOT_REACHED && vm->virtio_devs[slot]) {
            return virtio_mmio_read(vm->virtio_devs[slot], off, size);
        }
    }
    return 0;
}

void vmm_mmio_write(vmm_machine_t *vm, vcpu_t *cpu, uint64_t gpa, uint32_t size, uint64_t val) {
    if (!vm) {
        return;
    }
    if (gpa >= VMM_GICD_BASE && gpa < VMM_GICD_BASE + VMM_GICD_SIZE) {
        gicv2_dist_write(vm->gic, cpu, (uint32_t)(gpa - VMM_GICD_BASE), size, val);
        return;
    }
    if (gpa >= VMM_GICC_BASE && gpa < VMM_GICC_BASE + VMM_GICC_SIZE) {
        gicv2_cpu_write(vm->gic, cpu, (uint32_t)(gpa - VMM_GICC_BASE), size, val);
        return;
    }
    if (gpa >= VMM_GICH_BASE && gpa < VMM_GICH_BASE + VMM_GICH_SIZE) {
        gicv2_hyp_write(vm->gic, cpu, (uint32_t)(gpa - VMM_GICH_BASE), val);
        return;
    }
    if (gpa >= VMM_GICV_BASE && gpa < VMM_GICV_BASE + VMM_GICV_SIZE) {
        gicv2_vcpu_write(vm->gic, cpu, (uint32_t)(gpa - VMM_GICV_BASE), val);
        return;
    }
    if (gpa >= VMM_UART0_BASE && gpa < VMM_UART0_BASE + VMM_UART0_SIZE) {
        pl011_write(vm->uart, (uint32_t)(gpa - VMM_UART0_BASE), size, val);
        return;
    }
    if (gpa >= VMM_RTC_BASE && gpa < VMM_RTC_BASE + VMM_RTC_SIZE) {
        pl031_write(vm->rtc, (uint32_t)(gpa - VMM_RTC_BASE), size, val);
        return;
    }
    if (gpa >= VMM_VIRTIO_MMIO_BASE && gpa < VMM_VIRTIO_MMIO_BASE + VMM_VIRTIO_MMIO_SIZE) {
        uint64_t rel = gpa - VMM_VIRTIO_MMIO_BASE;
        uint32_t slot = (uint32_t)(rel / VMM_VIRTIO_MMIO_STRIDE);
        uint32_t off = (uint32_t)(rel % VMM_VIRTIO_MMIO_STRIDE);
        vmm_preboot_phase_t phase = atomic_load(&vmm_priv(vm)->preboot_phase);
        if (slot < VMM_VIRTIO_MMIO_SLOTS && phase != VMM_PREBOOT_ARMED &&
            phase != VMM_PREBOOT_REACHED && vm->virtio_devs[slot]) {
            virtio_mmio_write(vm->virtio_devs[slot], off, size, val);
        }
    }
}

static int load_kernel_and_dtb(vmm_machine_priv_t *priv, const vmm_config_t *config) {
    vmm_machine_t *vm = &priv->pub;
    uint64_t kernel_load = VMM_KERNEL_ADDR;
    uint64_t kernel_end = VMM_KERNEL_ADDR;

    if (config->kernel_image && config->kernel_size > 0) {
        const uint8_t *img_ptr = (const uint8_t *)config->kernel_image;
        size_t img_len = config->kernel_size;
        uint64_t text_offset = 0;
        uint64_t hdr_img_size = 0;
        if (img_len >= 64) {
            uint32_t magic = 0;
            uint64_t hdr_text_off = 0;
            memcpy(&hdr_text_off, img_ptr + 0x08, 8);
            memcpy(&hdr_img_size, img_ptr + 0x10, 8);
            memcpy(&magic, img_ptr + 0x38, 4);
            if (magic == ARM64_IMAGE_MAGIC) {
                text_offset = (hdr_img_size == 0) ? 0x80000ULL : hdr_text_off;
            }
        }
        kernel_load = (text_offset >= (VMM_KERNEL_ADDR - VMM_RAM_BASE))
                          ? (VMM_RAM_BASE + text_offset)
                          : (VMM_KERNEL_ADDR + text_offset);
        uint64_t offset_in_ram = kernel_load - VMM_RAM_BASE;
        uint64_t reserved_span = hdr_img_size > img_len ? hdr_img_size : (uint64_t)img_len;
        if (offset_in_ram >= vm->ram_size || reserved_span > vm->ram_size - offset_in_ram) {
            return -ENOSPC;
        }
        memcpy(vm->ram + offset_in_ram, img_ptr, img_len);
        kernel_end = kernel_load + reserved_span;
    }

    if (config->initramfs && config->initramfs_size > 0) {
        uint64_t initrd_addr = vmm_initrd_addr(vm->ram_size, config->initramfs_size);
        if (initrd_addr == 0 || initrd_addr < kernel_end) {
            return -ENOSPC;
        }
        memcpy(vm->ram + (initrd_addr - VMM_RAM_BASE), config->initramfs, config->initramfs_size);
    }

    int rc = fdt_build(vm, config, vm->ram + (VMM_DTB_ADDR - VMM_RAM_BASE), VMM_DTB_MAX_SIZE, NULL);
    if (rc < 0) {
        return rc;
    }

    priv->kernel_entry_pc = kernel_load;
    priv->dtb_addr = VMM_DTB_ADDR;
    return 0;
}

static int try_restore_snapshot(vmm_machine_priv_t *priv, const vmm_config_t *config,
                                size_t requested_ram_size, uint32_t requested_cpu_count) {
    int fd = open(config->snapshot_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return (errno == ENOENT) ? 0 : -errno;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        int err = -errno;
        close(fd);
        return err;
    }
    if (!S_ISREG(st.st_mode) ||
        st.st_size < (off_t)(sizeof(vmm_snapshot_header_t) + sizeof(vmm_snapshot_footer_t))) {
        close(fd);
        return -EINVAL;
    }

    vmm_snapshot_footer_t footer;
    int rc = pread_all(fd, &footer, sizeof(footer), st.st_size - (off_t)sizeof(footer));
    if (rc < 0 || footer.magic != VMM_SNAPSHOT_MAGIC || footer.state_offset >= (uint64_t)st.st_size) {
        close(fd);
        return rc < 0 ? rc : -EINVAL;
    }

    vmm_snapshot_header_t hdr;
    rc = pread_all(fd, &hdr, sizeof(hdr), (off_t)footer.state_offset);
    if (rc < 0 || hdr.magic != VMM_SNAPSHOT_MAGIC || hdr.version != VMM_SNAPSHOT_VERSION ||
        hdr.cpu_count != requested_cpu_count || hdr.ram_size != requested_ram_size ||
        hdr.ram_size != footer.state_offset ||
        hdr.flags != ((config->preboot_checkpoint ? VMM_SNAPSHOT_PREBOOT : 0) | (config->el2 ? VMM_SNAPSHOT_EL2 : 0)) ||
        memcmp(hdr.key, config->snapshot_key, sizeof(hdr.key)) != 0 ||
        hdr.online_mask == 0 || (hdr.online_mask >> hdr.cpu_count) != 0 ||
        hdr.gic_state_size != sizeof(gicv2_snapshot_t) ||
        hdr.uart_state_size != sizeof(pl011_snapshot_t) ||
        hdr.rtc_state_size != sizeof(pl031_snapshot_t)) {
        close(fd);
        return rc < 0 ? rc : -EINVAL;
    }
    uint64_t expected_size = hdr.ram_size + sizeof(hdr) + hdr.cpu_count * sizeof(vmm_snapshot_vcpu_t) +
                             sizeof(gicv2_snapshot_t) + sizeof(pl011_snapshot_t) +
                             sizeof(pl031_snapshot_t) + sizeof(footer);
    if (expected_size != (uint64_t)st.st_size) {
        close(fd);
        return -EINVAL;
    }

    void *ram = mmap(NULL, (size_t)hdr.ram_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (ram == MAP_FAILED) {
        int err = -errno;
        close(fd);
        return err;
    }

    vmm_machine_t *vm = &priv->pub;
    vm->ram = (uint8_t *)ram;
    vm->ram_size = (size_t)hdr.ram_size;
    vm->cpu_count = hdr.cpu_count;

    off_t off = (off_t)footer.state_offset + (off_t)sizeof(hdr);

    vmm_snapshot_vcpu_t *vcpu_states = calloc(hdr.cpu_count, sizeof(*vcpu_states));
    gicv2_snapshot_t *gic_state = malloc(sizeof(*gic_state));
    pl011_snapshot_t *uart_state = malloc(sizeof(*uart_state));
    pl031_snapshot_t *rtc_state = malloc(sizeof(*rtc_state));
    if (!vcpu_states || !gic_state || !uart_state || !rtc_state) {
        free(vcpu_states);
        free(gic_state);
        free(uart_state);
        free(rtc_state);
        close(fd);
        return -ENOMEM;
    }

    rc = pread_all(fd, vcpu_states, hdr.cpu_count * sizeof(*vcpu_states), off);
    off += (off_t)(hdr.cpu_count * sizeof(*vcpu_states));
    if (rc == 0) {
        rc = pread_all(fd, gic_state, sizeof(*gic_state), off);
        off += (off_t)sizeof(*gic_state);
    }
    if (rc == 0) {
        rc = pread_all(fd, uart_state, sizeof(*uart_state), off);
        off += (off_t)sizeof(*uart_state);
    }
    if (rc == 0) {
        rc = pread_all(fd, rtc_state, sizeof(*rtc_state), off);
    }
    close(fd);

    if (rc < 0) {
        free(vcpu_states);
        free(gic_state);
        free(uart_state);
        free(rtc_state);
        return rc;
    }

    for (uint32_t i = 0; i < hdr.cpu_count; i++) {
        void *mem = NULL;
        if (posix_memalign(&mem, 64, sizeof(vcpu_t)) != 0 || !mem) {
            free(vcpu_states);
            free(gic_state);
            free(uart_state);
            free(rtc_state);
            return -ENOMEM;
        }
        memset(mem, 0, sizeof(vcpu_t));
        vm->cpus[i] = (vcpu_t *)mem;
        vcpu_init(vm->cpus[i], vm, i);
        if (config->el2) {
            vcpu_enable_el2(vm->cpus[i]);
        }
        vcpu_load_snapshot(vm->cpus[i], &vcpu_states[i]);
        bool online = ((hdr.online_mask >> i) & 1U) != 0;
        atomic_store_explicit(&priv->cpu_online[i], online, memory_order_release);
        atomic_store_explicit(&vm->cpus[i]->halted_wfi, !online, memory_order_release);
    }

    vm->gic = gicv2_create(vm);
    vm->uart = pl011_create(vm);
    vm->rtc = pl031_create(vm);
    if (!vm->gic || !vm->uart || !vm->rtc) {
        free(vcpu_states);
        free(gic_state);
        free(uart_state);
        free(rtc_state);
        return -ENOMEM;
    }

    gicv2_load_state(vm->gic, gic_state);
    pl011_load_state(vm->uart, uart_state);
    pl031_load_state(vm->rtc, rtc_state);

    rc = vmm_timer_init(priv);
    if (rc == 0) {
        vmm_timer_set_cntvct(vm, hdr.timer_cntvct);
    }

    free(vcpu_states);
    free(gic_state);
    free(uart_state);
    free(rtc_state);

    if (rc < 0) {
        return rc;
    }
    priv->restored_from_snapshot = true;
    if (config->preboot_checkpoint) {
        atomic_store(&priv->preboot_phase, VMM_PREBOOT_RELEASED);
    }
    return 1;
}

static int attach_default_virtio(vmm_machine_priv_t *priv, const vmm_config_t *config) {
    vmm_machine_t *vm = &priv->pub;
    if (config->root_disk_path && config->root_disk_path[0] != '\0') {
        bool read_only = false;
        int fd = open(config->root_disk_path, O_RDWR | O_CLOEXEC);
        if (fd < 0 && (errno == EACCES || errno == EROFS)) {
            fd = open(config->root_disk_path, O_RDONLY | O_CLOEXEC);
            read_only = true;
        }
        if (fd < 0) {
            return -errno;
        }
        priv->blk_fd = fd;
        if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
            return -errno;
        }
        vm->virtio_devs[VMM_VIRTIO_SLOT_BLK] =
            virtio_blk_create(vm, VMM_VIRTIO_SLOT_BLK, fd, read_only);
        if (!vm->virtio_devs[VMM_VIRTIO_SLOT_BLK]) {
            return -ENOMEM;
        }
    }

    vm->virtio_devs[VMM_VIRTIO_SLOT_VSOCK] = virtio_vsock_create(vm, VMM_VIRTIO_SLOT_VSOCK, 3);
    static const uint8_t default_mac[6] = {0x52, 0x54, 0x00, 0x53, 0x57, 0x01};
    vm->virtio_devs[VMM_VIRTIO_SLOT_NET] =
        virtio_net_create(vm, VMM_VIRTIO_SLOT_NET, default_mac, config->network);
    vm->virtio_devs[VMM_VIRTIO_SLOT_BALLOON] =
        virtio_balloon_create(vm, VMM_VIRTIO_SLOT_BALLOON);
    vm->virtio_devs[VMM_VIRTIO_SLOT_RNG] = virtio_rng_create(vm, VMM_VIRTIO_SLOT_RNG);
    for (uint32_t slot = VMM_VIRTIO_SLOT_VSOCK; slot <= VMM_VIRTIO_SLOT_RNG; slot++) {
        if (!vm->virtio_devs[slot]) {
            return -ENOMEM;
        }
    }
    if (config->gpu) {
        vm->gpu_shm.pages = calloc(VMM_GPU_SHM_SIZE >> VMM_PAGE_SHIFT, sizeof(*vm->gpu_shm.pages));
        if (!vm->gpu_shm.pages) {
            return -ENOMEM;
        }
        vm->gpu_shm.base = VMM_GPU_SHM_BASE;
        vm->gpu_shm.size = VMM_GPU_SHM_SIZE;
        vm->virtio_devs[VMM_VIRTIO_SLOT_GPU] = virtio_gpu_create(vm, VMM_VIRTIO_SLOT_GPU);
        if (!vm->virtio_devs[VMM_VIRTIO_SLOT_GPU]) {
            return -ENOMEM;
        }
    }
    return 0;
}

vmm_machine_t *vmm_create(const vmm_config_t *config, int *err_out) {
    if (!config) {
        if (err_out) {
            *err_out = EINVAL;
        }
        return NULL;
    }

    uint32_t cpu_count = config->cpu_count ? config->cpu_count : 1;
    if (cpu_count > VMM_MAX_VCPUS) {
        if (err_out) {
            *err_out = EINVAL;
        }
        return NULL;
    }

    vmm_machine_priv_t *priv;
    if (posix_memalign((void **)&priv, 128, sizeof(*priv)) != 0) {
        if (err_out) {
            *err_out = ENOMEM;
        }
        return NULL;
    }
    memset(priv, 0, sizeof(*priv));

    priv->priv_magic = VMM_MACHINE_PRIV_MAGIC;
    priv->blk_fd = -1;
    atomic_init(&priv->preboot_phase, config->preboot_checkpoint ? VMM_PREBOOT_ARMED : VMM_PREBOOT_DISABLED);
    memcpy(priv->snapshot_key, config->snapshot_key, sizeof(priv->snapshot_key));
    priv->el2 = config->el2;
    pthread_mutex_init(&priv->state_lock, NULL);
    pthread_cond_init(&priv->suspend_cond, NULL);
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        pthread_mutex_init(&priv->wfi_lock[i], NULL);
        pthread_cond_init(&priv->wfi_cond[i], NULL);
        pthread_mutex_init(&priv->wfe_lock[i], NULL);
        pthread_cond_init(&priv->wfe_cond[i], NULL);
    }

    vmm_machine_t *vm = &priv->pub;
    vm->callback_ctx = config->callback_ctx;
    vm->console_output = config->console_output;
    vm->on_halt = config->on_halt;
    vm->on_panic = config->on_panic;

    int rc = 0;
    size_t ram_size = config->ram_size ? config->ram_size : (256ULL << 20);
    if (ram_size > SIZE_MAX - (size_t)VMM_RAM_ALIGN ||
        ram_size < (VMM_KERNEL_ADDR - VMM_RAM_BASE) + VMM_PAGE_SIZE) {
        vmm_destroy(vm);
        if (err_out) {
            *err_out = EINVAL;
        }
        return NULL;
    }
    ram_size = (ram_size + (size_t)VMM_RAM_ALIGN - 1) & ~((size_t)VMM_RAM_ALIGN - 1);

    cpu_jit_reserve();

    int snap_rc = 0;
    if (config->snapshot_path && config->snapshot_path[0] != '\0') {
        snap_rc = try_restore_snapshot(priv, config, ram_size, cpu_count);
    }
    if (config->snapshot_required && (snap_rc == 0 || snap_rc == -EINVAL)) {
        snap_rc = -ESTALE;
    }
    if (snap_rc < 0) {
        vmm_destroy(vm);
        if (err_out) {
            *err_out = -snap_rc;
        }
        return NULL;
    }

    if (snap_rc == 0) {
        void *ram = mmap(NULL, ram_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (ram == MAP_FAILED) {
            int err = errno;
            vmm_destroy(vm);
            if (err_out) {
                *err_out = err;
            }
            return NULL;
        }

        vm->ram = (uint8_t *)ram;
        vm->ram_size = ram_size;
        vm->cpu_count = cpu_count;

        for (uint32_t i = 0; i < cpu_count; i++) {
            void *mem = NULL;
            if (posix_memalign(&mem, 64, sizeof(vcpu_t)) != 0 || !mem) {
                vmm_destroy(vm);
                if (err_out) {
                    *err_out = ENOMEM;
                }
                return NULL;
            }
            memset(mem, 0, sizeof(vcpu_t));
            vm->cpus[i] = (vcpu_t *)mem;
            vcpu_init(vm->cpus[i], vm, i);
            if (config->el2) {
                vcpu_enable_el2(vm->cpus[i]);
            }
            mmu_tlbi_all(vm->cpus[i]);
            mmu_update_asid_salt(vm->cpus[i]);
        }

        vm->gic = gicv2_create(vm);
        vm->uart = pl011_create(vm);
        vm->rtc = pl031_create(vm);
        if (!vm->gic || !vm->uart || !vm->rtc) {
            vmm_destroy(vm);
            if (err_out) {
                *err_out = ENOMEM;
            }
            return NULL;
        }

        rc = vmm_timer_init(priv);
        if (rc < 0) {
            vmm_destroy(vm);
            if (err_out) {
                *err_out = -rc;
            }
            return NULL;
        }

        rc = load_kernel_and_dtb(priv, config);
        if (rc < 0) {
            vmm_destroy(vm);
            if (err_out) {
                *err_out = -rc;
            }
            return NULL;
        }

        vcpu_reset(vm->cpus[0], priv->kernel_entry_pc, priv->dtb_addr);
        mmu_tlbi_all(vm->cpus[0]);
        mmu_update_asid_salt(vm->cpus[0]);
        atomic_store_explicit(&priv->cpu_online[0], true, memory_order_release);
        atomic_store_explicit(&vm->cpus[0]->halted_wfi, false, memory_order_release);
        for (uint32_t i = 1; i < cpu_count; i++) {
            atomic_store_explicit(&priv->cpu_online[i], false, memory_order_release);
            atomic_store_explicit(&vm->cpus[i]->halted_wfi, true, memory_order_release);
        }
    }

    rc = attach_default_virtio(priv, config);
    if (rc < 0) {
        vmm_destroy(vm);
        if (err_out) {
            *err_out = -rc;
        }
        return NULL;
    }
    if (err_out) {
        *err_out = 0;
    }
    return vm;
}

int vmm_start(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return -EINVAL;
    }

    pthread_mutex_lock(&priv->state_lock);
    if (atomic_load_explicit(&vm->running, memory_order_acquire)) {
        pthread_mutex_unlock(&priv->state_lock);
        return -EBUSY;
    }
    priv->halt_notified = false;
    atomic_store_explicit(&vm->suspended, false, memory_order_release);
    atomic_store_explicit(&vm->running, true, memory_order_release);

    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (!atomic_load_explicit(&priv->cpu_online[i], memory_order_acquire)) {
            continue;
        }
        vcpu_t *cpu = vm->cpus[i];
        cpu->stop_reason = VCPU_STOP_NONE;
        atomic_store_explicit(&cpu->stop_requested, false, memory_order_release);
        int rc = pthread_create(&priv->threads[i], NULL, vcpu_thread_main, cpu);
        if (rc != 0) {
            pthread_mutex_unlock(&priv->state_lock);
            vmm_stop(vm);
            return -rc;
        }
        priv->thread_started[i] = true;
    }
    pthread_mutex_unlock(&priv->state_lock);
    return 0;
}

int vmm_suspend(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return -EINVAL;
    }
    if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
        return 0;
    }
    if (atomic_exchange_explicit(&vm->suspended, true, memory_order_acq_rel)) {
        return 0;
    }
    pthread_mutex_lock(&priv->state_lock);
    pthread_cond_broadcast(&priv->suspend_cond);
    pthread_mutex_unlock(&priv->state_lock);

    vmm_timer_disarm_all(priv);
    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (vm->cpus[i]) {
            atomic_store_explicit(&vm->cpus[i]->stop_requested, true, memory_order_release);
            vmm_vcpu_wake(vm, i);
        }
    }

    pthread_mutex_lock(&priv->state_lock);
    for (;;) {
        uint32_t active = 0;
        for (uint32_t i = 0; i < vm->cpu_count; i++) {
            if (priv->thread_started[i] &&
                atomic_load_explicit(&priv->cpu_online[i], memory_order_acquire)) {
                active++;
            }
        }
        if (priv->suspended_count >= active ||
            !atomic_load_explicit(&vm->running, memory_order_acquire)) {
            break;
        }
        pthread_cond_wait(&priv->suspend_cond, &priv->state_lock);
    }
    pthread_mutex_unlock(&priv->state_lock);
    virtio_blk_flush_host(vm->virtio_devs[VMM_VIRTIO_SLOT_BLK]);
    return 0;
}

int vmm_resume(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return -EINVAL;
    }
    if (!atomic_load_explicit(&vm->running, memory_order_acquire)) {
        return 0;
    }
    if (!atomic_load_explicit(&vm->suspended, memory_order_acquire)) {
        return 0;
    }

    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (vm->cpus[i]) {
            atomic_store_explicit(&vm->cpus[i]->stop_requested, false, memory_order_release);
        }
    }
    pthread_mutex_lock(&priv->state_lock);
    atomic_store_explicit(&vm->suspended, false, memory_order_release);
    pthread_cond_broadcast(&priv->suspend_cond);
    pthread_mutex_unlock(&priv->state_lock);

    vmm_timer_sync_all(priv);
    return 0;
}

void vmm_stop(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return;
    }

    atomic_store_explicit(&vm->running, false, memory_order_release);
    vmm_timer_disarm_all(priv);

    for (uint32_t i = 0; i < vm->cpu_count; i++) {
        if (vm->cpus[i]) {
            atomic_store_explicit(&vm->cpus[i]->stop_requested, true, memory_order_release);
            vmm_vcpu_wake(vm, i);
        }
    }

    pthread_mutex_lock(&priv->state_lock);
    atomic_store_explicit(&vm->suspended, false, memory_order_release);
    pthread_cond_broadcast(&priv->suspend_cond);

    pthread_t to_join[VMM_MAX_VCPUS];
    bool has_join[VMM_MAX_VCPUS] = {0};
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        if (priv->thread_started[i]) {
            to_join[i] = priv->threads[i];
            has_join[i] = true;
            priv->thread_started[i] = false;
        }
    }
    pthread_mutex_unlock(&priv->state_lock);

    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        if (has_join[i]) {
            pthread_join(to_join[i], NULL);
        }
    }
    virtio_blk_flush_host(vm->virtio_devs[VMM_VIRTIO_SLOT_BLK]);
}

void vmm_destroy(vmm_machine_t *vm) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return;
    }

    vmm_stop(vm);
    vmm_timer_destroy(priv);

    for (uint32_t i = 0; i < VMM_VIRTIO_MMIO_SLOTS; i++) {
        if (vm->virtio_devs[i]) {
            virtio_dev_destroy(vm->virtio_devs[i]);
            vm->virtio_devs[i] = NULL;
        }
    }
    free(vm->gpu_shm.pages);
    vm->gpu_shm = (vmm_shm_window_t){0};

    gicv2_destroy(vm->gic);
    vm->gic = NULL;
    pl011_destroy(vm->uart);
    vm->uart = NULL;
    pl031_destroy(vm->rtc);
    vm->rtc = NULL;

    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        if (vm->cpus[i]) {
            vcpu_destroy(vm->cpus[i]);
            free(vm->cpus[i]);
            vm->cpus[i] = NULL;
        }
    }

    if (vm->ram && vm->ram_size > 0) {
        munmap(vm->ram, vm->ram_size);
        vm->ram = NULL;
    }
    cpu_pool_destroy(vm);
    if (priv->blk_fd >= 0) {
        close(priv->blk_fd);
        priv->blk_fd = -1;
    }

    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        pthread_cond_destroy(&priv->wfi_cond[i]);
        pthread_mutex_destroy(&priv->wfi_lock[i]);
        pthread_cond_destroy(&priv->wfe_cond[i]);
        pthread_mutex_destroy(&priv->wfe_lock[i]);
    }
    pthread_cond_destroy(&priv->suspend_cond);
    pthread_mutex_destroy(&priv->state_lock);
    priv->priv_magic = 0;
    free(priv);
}

int vmm_save_snapshot(vmm_machine_t *vm, const char *path) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || !path || path[0] == '\0' || !vm->ram || vm->ram_size == 0) {
        return -EINVAL;
    }

    bool was_running = atomic_load_explicit(&vm->running, memory_order_acquire);
    bool was_suspended = atomic_load_explicit(&vm->suspended, memory_order_acquire);
    if (was_running && !was_suspended) {
        int rc = vmm_suspend(vm);
        if (rc < 0) {
            return rc;
        }
    }

    vmm_preboot_phase_t phase = atomic_load(&priv->preboot_phase);
    uint64_t cntvct = vmm_timer_cntvct(vm);
    size_t tmp_len = strlen(path) + sizeof(".XXXXXX");
    char *tmp_path = malloc(tmp_len);
    if (!tmp_path) {
        if (was_running && !was_suspended) {
            vmm_resume(vm);
        }
        return -ENOMEM;
    }
    snprintf(tmp_path, tmp_len, "%s.XXXXXX", path);

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        int err = -errno;
        free(tmp_path);
        if (was_running && !was_suspended) {
            vmm_resume(vm);
        }
        return err;
    }
    (void)fcntl(fd, F_NOCACHE, 1);

    int rc = 0;
    if (ftruncate(fd, (off_t)vm->ram_size) < 0) {
        rc = -errno;
    }

    if (rc == 0) {
        rc = snapshot_write_ram(fd, vm);
    }

    if (rc == 0) {
        vmm_snapshot_header_t hdr = {
            .magic = VMM_SNAPSHOT_MAGIC,
            .version = VMM_SNAPSHOT_VERSION,
            .cpu_count = vm->cpu_count,
            .ram_size = (uint64_t)vm->ram_size,
            .timer_cntvct = cntvct,
            .online_mask = 0,
            .gic_state_size = (uint32_t)sizeof(gicv2_snapshot_t),
            .uart_state_size = (uint32_t)sizeof(pl011_snapshot_t),
            .rtc_state_size = (uint32_t)sizeof(pl031_snapshot_t),
            .flags = (phase == VMM_PREBOOT_REACHED ? VMM_SNAPSHOT_PREBOOT : 0) | (priv->el2 ? VMM_SNAPSHOT_EL2 : 0),
        };
        memcpy(hdr.key, priv->snapshot_key, sizeof(hdr.key));
        for (uint32_t i = 0; i < vm->cpu_count; i++) {
            if (atomic_load_explicit(&priv->cpu_online[i], memory_order_acquire)) {
                hdr.online_mask |= (1U << i);
            }
        }

        vmm_snapshot_vcpu_t *vcpu_states = calloc(vm->cpu_count, sizeof(*vcpu_states));
        gicv2_snapshot_t *gic_state = malloc(sizeof(*gic_state));
        pl011_snapshot_t *uart_state = malloc(sizeof(*uart_state));
        pl031_snapshot_t *rtc_state = malloc(sizeof(*rtc_state));

        if (!vcpu_states || !gic_state || !uart_state || !rtc_state) {
            rc = -ENOMEM;
        } else {
            for (uint32_t i = 0; i < vm->cpu_count; i++) {
                vcpu_save_snapshot(vm->cpus[i], &vcpu_states[i]);
            }
            gicv2_save_state(vm->gic, gic_state);
            pl011_save_state(vm->uart, uart_state);
            pl031_save_state(vm->rtc, rtc_state);

            off_t off = (off_t)vm->ram_size;
            rc = pwrite_all(fd, &hdr, sizeof(hdr), off);
            off += (off_t)sizeof(hdr);
            if (rc == 0) {
                rc = pwrite_all(fd, vcpu_states, vm->cpu_count * sizeof(*vcpu_states), off);
                off += (off_t)(vm->cpu_count * sizeof(*vcpu_states));
            }
            if (rc == 0) {
                rc = pwrite_all(fd, gic_state, sizeof(*gic_state), off);
                off += (off_t)sizeof(*gic_state);
            }
            if (rc == 0) {
                rc = pwrite_all(fd, uart_state, sizeof(*uart_state), off);
                off += (off_t)sizeof(*uart_state);
            }
            if (rc == 0) {
                rc = pwrite_all(fd, rtc_state, sizeof(*rtc_state), off);
                off += (off_t)sizeof(*rtc_state);
            }
            if (rc == 0) {
                vmm_snapshot_footer_t footer = {
                    .state_offset = (uint64_t)vm->ram_size,
                    .magic = VMM_SNAPSHOT_MAGIC,
                };
                rc = pwrite_all(fd, &footer, sizeof(footer), off);
            }
        }

        free(vcpu_states);
        free(gic_state);
        free(uart_state);
        free(rtc_state);
    }

    if (rc == 0 && fsync(fd) < 0) {
        rc = -errno;
    }
    close(fd);

    if (rc == 0) {
        if (rename(tmp_path, path) < 0) {
            rc = -errno;
            unlink(tmp_path);
        }
    } else {
        unlink(tmp_path);
    }
    free(tmp_path);

    if (was_running && !was_suspended) {
        vmm_resume(vm);
    }
    return rc;
}

int vmm_attach_virtio_fs(vmm_machine_t *vm, const char *tag, const char *host_root,
                         bool read_only, uint32_t *slot_out) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || !tag || !host_root) {
        return -EINVAL;
    }

    pthread_mutex_lock(&priv->state_lock);
    uint32_t chosen = VMM_VIRTIO_MMIO_SLOTS;
    for (uint32_t i = 0; i < VMM_VIRTIO_SLOT_FS_COUNT; i++) {
        uint32_t slot = VMM_VIRTIO_SLOT_FS_BASE + i;
        if (!vm->virtio_devs[slot]) {
            chosen = slot;
            break;
        }
    }
    if (chosen >= VMM_VIRTIO_MMIO_SLOTS) {
        pthread_mutex_unlock(&priv->state_lock);
        return -ENOSPC;
    }

    virtio_dev_t *dev = virtio_fs_create(vm, chosen, tag, host_root, read_only);
    if (!dev) {
        pthread_mutex_unlock(&priv->state_lock);
        return -ENOMEM;
    }
    vm->virtio_devs[chosen] = dev;
    pthread_mutex_unlock(&priv->state_lock);

    if (slot_out) {
        *slot_out = chosen;
    }
    return 0;
}

void vmm_detach_virtio_fs(vmm_machine_t *vm, uint32_t slot) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv || slot < VMM_VIRTIO_SLOT_FS_BASE ||
        slot >= VMM_VIRTIO_SLOT_FS_BASE + VMM_VIRTIO_SLOT_FS_COUNT) {
        return;
    }

    pthread_mutex_lock(&priv->state_lock);
    virtio_dev_t *dev = vm->virtio_devs[slot];
    vm->virtio_devs[slot] = NULL;
    pthread_mutex_unlock(&priv->state_lock);

    if (dev) {
        virtio_dev_destroy(dev);
    }
}
