#include "board_internal.h"
#include "machine.h"

#include <errno.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/qos.h>
#include <time.h>

static pthread_once_t g_tb_once = PTHREAD_ONCE_INIT;
static uint64_t g_tb_num = 1;
static uint64_t g_tb_den = 1;

static uint64_t gcd_u64(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a ? a : 1;
}

static void init_timebase(void) {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    uint64_t num = (uint64_t)info.numer * 24ULL;
    uint64_t den = (uint64_t)info.denom * 1000ULL;
    uint64_t g = gcd_u64(num, den);
    g_tb_num = num / g;
    g_tb_den = den / g;
}

static uint64_t host_ticks_24mhz(void) {
    uint64_t raw = mach_absolute_time();
    if (g_tb_num == 1 && g_tb_den == 1) {
        return raw;
    }
    return (uint64_t)(((__uint128_t)raw * g_tb_num) / g_tb_den);
}

uint64_t vmm_timer_cntfrq(void) {
    return VMM_TIMER_FREQ;
}

uint64_t vmm_timer_cntvct(const vmm_machine_t *vm) {
    uint64_t now = host_ticks_24mhz();
    const vmm_machine_priv_t *priv = vmm_priv_const(vm);
    if (!priv) {
        return now;
    }
    uint64_t base = atomic_load_explicit(&priv->timer.boot_ticks, memory_order_relaxed);
    return now - base;
}

void vmm_timer_set_cntvct(vmm_machine_t *vm, uint64_t cntvct) {
    vmm_machine_priv_t *priv = vmm_priv(vm);
    if (!priv) {
        return;
    }
    uint64_t now = host_ticks_24mhz();
    uint64_t base = now - cntvct;
    atomic_store_explicit(&priv->timer.boot_ticks, base, memory_order_release);
}

static bool eval_timer_line(uint64_t ctl, uint64_t cval, uint64_t now, uint64_t *min_delta) {
    bool enabled = (ctl & 1ULL) != 0;
    bool masked = (ctl & 2ULL) != 0;
    bool met = enabled && (now >= cval);

    if (enabled && !masked && !met) {
        uint64_t delta = cval - now;
        if (delta < *min_delta) {
            *min_delta = delta;
        }
    }
    return met && !masked;
}

static void vmm_timer_sync_locked(vmm_machine_priv_t *priv, vcpu_t *cpu) {
    vmm_timer_state_t *ts = &priv->timer;
    uint32_t id = cpu->cpu_id;

    uint64_t now = vmm_timer_cntvct(cpu->vm);
    uint64_t min_delta = UINT64_MAX;
    bool online = atomic_load_explicit(&priv->cpu_online[id], memory_order_acquire);
    bool virt_irq = online && eval_timer_line(cpu->cntv_ctl_el0, cpu->cntv_cval_el0, now, &min_delta);
    bool phys_irq = online && eval_timer_line(cpu->cntp_ctl_el0, cpu->cntp_cval_el0, now, &min_delta);

    uint64_t new_deadline = UINT64_MAX;
    if (atomic_load_explicit(&ts->active, memory_order_acquire) &&
        atomic_load_explicit(&cpu->vm->running, memory_order_acquire) &&
        !atomic_load_explicit(&cpu->vm->suspended, memory_order_acquire) &&
        min_delta != UINT64_MAX) {
        const uint64_t max_ticks = 3600ULL * VMM_TIMER_FREQ;
        if (min_delta > max_ticks) {
            min_delta = max_ticks;
        }
        new_deadline = (min_delta < UINT64_MAX - now) ? (now + min_delta) : (UINT64_MAX - 1);
    }

    uint64_t earliest_before = UINT64_MAX;
    for (uint32_t i = 0; i < priv->pub.cpu_count; i++) {
        if (ts->deadlines[i] < earliest_before) {
            earliest_before = ts->deadlines[i];
        }
    }
    ts->deadlines[id] = new_deadline;
    if (new_deadline < earliest_before) {
        pthread_cond_signal(&ts->cond);
    }

    gicv2_set_timer_ppis(cpu->vm, id, virt_irq, phys_irq);
}

static void *timer_thread_main(void *ctx) {
    vmm_machine_priv_t *priv = ctx;
    vmm_timer_state_t *ts = &priv->timer;

    pthread_mutex_lock(&ts->lock);
    while (!ts->stop) {
        uint64_t now = vmm_timer_cntvct(&priv->pub);
        uint64_t earliest = UINT64_MAX;
        bool fired = false;

        for (uint32_t i = 0; i < priv->pub.cpu_count; i++) {
            uint64_t d = ts->deadlines[i];
            if (d == UINT64_MAX) {
                continue;
            }
            if (d <= now) {
                ts->deadlines[i] = UINT64_MAX;
                if (atomic_load_explicit(&ts->active, memory_order_acquire) && priv->pub.cpus[i]) {
                    vmm_timer_sync_locked(priv, priv->pub.cpus[i]);
                }
                fired = true;
            } else if (d < earliest) {
                earliest = d;
            }
        }
        if (fired) {
            continue;
        }

        if (earliest == UINT64_MAX) {
            pthread_cond_wait(&ts->cond, &ts->lock);
        } else {
            uint64_t delta_ticks = earliest - now;
            uint64_t ns = (uint64_t)(((__uint128_t)delta_ticks * 1000ULL) / 24ULL);
            if (ns < 1000ULL) {
                ns = 1000ULL;
            }
            struct timespec rel = {
                .tv_sec = (time_t)(ns / 1000000000ULL),
                .tv_nsec = (long)(ns % 1000000000ULL),
            };
            (void)pthread_cond_timedwait_relative_np(&ts->cond, &ts->lock, &rel);
        }
    }
    pthread_mutex_unlock(&ts->lock);
    return NULL;
}

int vmm_timer_init(vmm_machine_priv_t *priv) {
    vmm_timer_state_t *ts = &priv->timer;
    pthread_once(&g_tb_once, init_timebase);
    atomic_store_explicit(&ts->boot_ticks, host_ticks_24mhz(), memory_order_release);
    atomic_store_explicit(&ts->active, true, memory_order_release);
    ts->stop = false;
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        ts->deadlines[i] = UINT64_MAX;
    }
    pthread_mutex_init(&ts->lock, NULL);
    pthread_cond_init(&ts->cond, NULL);
    ts->initialized = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    (void)pthread_attr_set_qos_class_np(&attr, QOS_CLASS_USER_INTERACTIVE, 0);
    int rc = pthread_create(&ts->thread, &attr, timer_thread_main, priv);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        vmm_timer_destroy(priv);
        return -rc;
    }
    ts->thread_started = true;
    return 0;
}

void vmm_timer_disarm_all(vmm_machine_priv_t *priv) {
    vmm_timer_state_t *ts = &priv->timer;
    if (!ts->initialized) {
        return;
    }
    pthread_mutex_lock(&ts->lock);
    for (uint32_t i = 0; i < priv->pub.cpu_count; i++) {
        ts->deadlines[i] = UINT64_MAX;
    }
    pthread_cond_signal(&ts->cond);
    pthread_mutex_unlock(&ts->lock);
}

void vmm_timer_sync_all(vmm_machine_priv_t *priv) {
    for (uint32_t i = 0; i < priv->pub.cpu_count; i++) {
        if (priv->pub.cpus[i] && atomic_load_explicit(&priv->cpu_online[i], memory_order_acquire)) {
            vmm_timer_sync(priv->pub.cpus[i]);
        }
    }
}

void vmm_timer_destroy(vmm_machine_priv_t *priv) {
    vmm_timer_state_t *ts = &priv->timer;
    atomic_store_explicit(&ts->active, false, memory_order_release);
    if (!ts->initialized) {
        return;
    }
    pthread_mutex_lock(&ts->lock);
    ts->stop = true;
    for (uint32_t i = 0; i < VMM_MAX_VCPUS; i++) {
        ts->deadlines[i] = UINT64_MAX;
    }
    pthread_cond_broadcast(&ts->cond);
    pthread_mutex_unlock(&ts->lock);

    if (ts->thread_started) {
        pthread_join(ts->thread, NULL);
        ts->thread_started = false;
    }
    pthread_cond_destroy(&ts->cond);
    pthread_mutex_destroy(&ts->lock);
    ts->initialized = false;
}

void vmm_timer_sync(vcpu_t *cpu) {
    if (!cpu || !cpu->vm) {
        return;
    }
    vmm_machine_priv_t *priv = vmm_priv(cpu->vm);
    if (!priv || cpu->cpu_id >= priv->pub.cpu_count) {
        return;
    }
    vmm_timer_state_t *ts = &priv->timer;
    if (!ts->initialized) {
        return;
    }
    pthread_mutex_lock(&ts->lock);
    vmm_timer_sync_locked(priv, cpu);
    pthread_mutex_unlock(&ts->lock);
}

void vmm_timer_write(vcpu_t *cpu, uint64_t *reg, uint64_t value) {
    vmm_machine_priv_t *priv = vmm_priv(cpu->vm);
    if (!priv || !priv->timer.initialized) {
        *reg = value;
        return;
    }

    pthread_mutex_lock(&priv->timer.lock);
    *reg = value;
    vmm_timer_sync_locked(priv, cpu);
    pthread_mutex_unlock(&priv->timer.lock);
}
