#include "cpu_internal.h"
#include "../include/CSwiptermKit/swipterm.h"

#include <TargetConditionals.h>
#include <dlfcn.h>
#include <errno.h>
#include <libkern/OSCacheControl.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <os/lock.h>
#include <ptrauth.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum : uint32_t {
    JIT_REGION_MAX = 128U << 20,
    JIT_REGION_MIN = 16U << 20,
    JIT_PAGE_SIZE = 16U << 10,
    JIT_PROBE_STEP = 1U << 20,
    JIT_BRK_F00D = 0xD43E01A0U,
    JIT_PROBE_VALUE = 0x5A17U,
};

#if TARGET_OS_OSX
#define JIT_DUAL_MAPPING 0
#else
#define JIT_DUAL_MAPPING 1
#endif

#if JIT_DUAL_MAPPING && !TARGET_OS_SIMULATOR
#define JIT_DEBUGGER_HANDSHAKE 1
#include <sys/sysctl.h>
#else
#define JIT_DEBUGGER_HANDSHAKE 0
#endif

_Atomic uint32_t cpu_jit_generation = 1;
extern const uint32_t cpu_fast_base_offset;

static struct {
    pthread_mutex_t prepare_lock;
    _Atomic bool available;
    _Atomic bool prepared;
    _Atomic bool enabled;
    uintptr_t reserved;
    uint32_t size;
    uint8_t *rx;
    uint8_t *rw;
    os_unfair_lock owner_lock;
    vmm_machine_t *owner;
    uint32_t users;
} jit_region = {
    .prepare_lock = PTHREAD_MUTEX_INITIALIZER,
    .enabled = true,
    .owner_lock = OS_UNFAIR_LOCK_INIT,
};

__attribute__((format(printf, 2, 3))) static void jit_set_error(char **error_message, const char *format, ...) {
    if (error_message == nullptr || *error_message != nullptr) {
        return;
    }
    va_list args;
    va_start(args, format);
    (void)vasprintf(error_message, format, args);
    va_end(args);
}

uintptr_t cpu_fast_base(void) {
    return (uintptr_t)cpu_fast_run + cpu_fast_base_offset;
}

static uintptr_t jit_image_end(void) {
    Dl_info info;
    if (dladdr((const void *)cpu_fast_run, &info) == 0 || info.dli_fbase == NULL) {
        return 0;
    }
    const struct mach_header_64 *header = info.dli_fbase;
    const uint8_t *cursor = (const uint8_t *)(header + 1);
    uint64_t text_vmaddr = 0, end_vmaddr = 0;
    for (uint32_t i = 0; i < header->ncmds; ++i) {
        const struct load_command *command = (const struct load_command *)cursor;
        if (command->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *segment = (const struct segment_command_64 *)command;
            if (strcmp(segment->segname, SEG_TEXT) == 0) {
                text_vmaddr = segment->vmaddr;
            }
            if (segment->initprot != 0 && segment->vmaddr + segment->vmsize > end_vmaddr) {
                end_vmaddr = segment->vmaddr + segment->vmsize;
            }
        }
        cursor += command->cmdsize;
    }
    return (uintptr_t)header + (uintptr_t)(end_vmaddr - text_vmaddr);
}

static bool jit_in_window(uintptr_t rx, uint32_t size) {
    uintptr_t base = cpu_fast_base();
    return rx > base && rx + size - base < (1ULL << 32);
}

#if JIT_DUAL_MAPPING
static uintptr_t jit_reserve_va_locked(void) {
    if (jit_region.reserved != 0) {
        return jit_region.reserved;
    }
    uintptr_t start = (jit_image_end() + JIT_PROBE_STEP - 1) & ~(uintptr_t)(JIT_PROBE_STEP - 1);
    for (uint32_t size = JIT_REGION_MAX; size >= JIT_REGION_MIN; size /= 2) {
        for (uintptr_t probe = start; start != 0 && jit_in_window(probe, size); probe += JIT_PROBE_STEP) {
            vm_address_t address = probe;
            if (vm_allocate(mach_task_self(), &address, size, VM_FLAGS_FIXED) != KERN_SUCCESS) {
                continue;
            }
            (void)vm_protect(mach_task_self(), address, size, FALSE, VM_PROT_NONE);
            jit_region.reserved = address;
            jit_region.size = size;
            return address;
        }
    }
    return 0;
}

void cpu_jit_reserve(void) {
    if (swipterm_jit_is_prepared()) {
        return;
    }
    pthread_mutex_lock(&jit_region.prepare_lock);
    if (!atomic_load_explicit(&jit_region.prepared, memory_order_relaxed)) {
        (void)jit_reserve_va_locked();
    }
    pthread_mutex_unlock(&jit_region.prepare_lock);
}
#else
void cpu_jit_reserve(void) {
}

static uintptr_t jit_reserve(char **error_message) {
    uintptr_t start = (jit_image_end() + JIT_PROBE_STEP - 1) & ~(uintptr_t)(JIT_PROBE_STEP - 1);
    int last_errno = 0;
    for (uint32_t size = JIT_REGION_MAX; size >= JIT_REGION_MIN && last_errno == 0; size /= 2) {
        for (uintptr_t hint = start; start != 0 && jit_in_window(hint, size); hint += JIT_PROBE_STEP) {
            void *address = mmap((void *)hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                 MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
            if (address == MAP_FAILED) {
                last_errno = errno;
                break;
            }
            if (jit_in_window((uintptr_t)address, size)) {
                jit_region.size = size;
                return (uintptr_t)address;
            }
            munmap(address, size);
        }
    }
    if (last_errno != 0) {
        jit_set_error(error_message, "mmap(MAP_JIT) failed: %s", strerror(last_errno));
    } else {
        jit_set_error(error_message, "no 16 MiB virtual address range within 4 GiB of interpreter base");
    }
    return 0;
}
#endif

#if JIT_DEBUGGER_HANDSHAKE
__attribute__((naked, noinline)) static void *jit_debugger_prepare(void *address, size_t length) {
    __asm__("mov x16, #1\n"
            "brk #0xf00d\n"
            "ret\n");
}

__attribute__((naked, noinline)) static void jit_debugger_detach(void) {
    __asm__("mov x16, #0\n"
            "brk #0xf00d\n"
            "ret\n");
}

static void jit_trap_handler(int signal, siginfo_t *info, void *context) {
    (void)info;
    arm_thread_state64_t *state = &((ucontext_t *)context)->uc_mcontext->__ss;
    uintptr_t pc = (uintptr_t)arm_thread_state64_get_pc(*state);
    if (*(const uint32_t *)pc != JIT_BRK_F00D) {
        struct sigaction action = {.sa_handler = SIG_DFL};
        sigaction(signal, &action, nullptr);
        return;
    }
    state->__x[0] = 0;
    arm_thread_state64_set_pc_fptr(*state, (void *)(pc + 4));
}
#endif

#if JIT_DUAL_MAPPING
static sigjmp_buf jit_probe_jump;

static void jit_probe_handler(int signal) {
    siglongjmp(jit_probe_jump, signal);
}

static bool jit_probe_page(uint8_t *rx, uint8_t *rw, const char *label, char **error_message) {
    uint32_t stub[2] = {0x52800000U | (JIT_PROBE_VALUE << 5), 0xD65F03C0U};
    memcpy(rw, stub, sizeof(stub));
    sys_icache_invalidate(rx, sizeof(stub));
    int trapped = sigsetjmp(jit_probe_jump, 1);
    if (trapped != 0) {
        jit_set_error(error_message, "JIT probe on %s page (%p) trapped with signal %d (%s)",
                      label, (void *)rx, trapped, strsignal(trapped));
        return false;
    }
    uint32_t (*probe)(void) = ptrauth_sign_unauthenticated((uint32_t (*)(void))rx, ptrauth_key_function_pointer, 0);
    uint32_t result = probe();
    if (result != JIT_PROBE_VALUE) {
        jit_set_error(error_message, "JIT probe on %s page (%p) returned 0x%x", label, (void *)rx, result);
        return false;
    }
    return true;
}

static bool jit_probe(uint8_t *rx, uint8_t *rw, char **error_message) {
    static const int signals[] = {SIGBUS, SIGSEGV, SIGILL, SIGTRAP};
    struct sigaction saved[sizeof(signals) / sizeof(signals[0])];
    struct sigaction action = {.sa_handler = jit_probe_handler, .sa_flags = SA_NODEFER};
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        sigaction(signals[i], &action, &saved[i]);
    }
    uint32_t last = jit_region.size - JIT_PAGE_SIZE;
    bool ok = jit_probe_page(rx, rw, "first", error_message) &&
              jit_probe_page(rx + last, rw + last, "last", error_message);
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        sigaction(signals[i], &saved[i], nullptr);
    }
    return ok;
}

static bool jit_prepare_region(char **error_message) {
    uintptr_t rx = jit_reserve_va_locked();
    uint32_t size = jit_region.size;
    vm_address_t rw = 0;
    bool ok = rx != 0;
    if (!ok) {
        jit_set_error(error_message, "no 16 MiB virtual address range within 4 GiB of interpreter base");
    }
    if (ok && mprotect((void *)rx, size, PROT_READ | PROT_EXEC) != 0) {
        jit_set_error(error_message, "mprotect(PROT_READ | PROT_EXEC) at %p failed: %s", (void *)rx, strerror(errno));
        ok = false;
    }
    if (ok) {
        vm_prot_t current, maximum;
        kern_return_t kr = vm_remap(mach_task_self(), &rw, size, 0, VM_FLAGS_ANYWHERE, mach_task_self(), rx,
                                    FALSE, &current, &maximum, VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS) {
            jit_set_error(error_message, "vm_remap failed: %s (0x%x)", mach_error_string(kr), kr);
            ok = false;
        } else if (mprotect((void *)rw, size, PROT_READ | PROT_WRITE) != 0) {
            jit_set_error(error_message, "mprotect(PROT_READ | PROT_WRITE) at %p failed: %s",
                          (void *)rw, strerror(errno));
            ok = false;
        }
    }
#if JIT_DEBUGGER_HANDSHAKE
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info = {0};
    size_t info_size = sizeof(info);
    bool traced = sysctl(mib, 4, &info, &info_size, nullptr, 0) == 0 && (info.kp_proc.p_flag & P_TRACED) != 0;
    if (traced) {
        struct sigaction saved, action = {.sa_sigaction = jit_trap_handler, .sa_flags = SA_SIGINFO};
        sigaction(SIGTRAP, &action, &saved);
        if (ok && jit_debugger_prepare((void *)rx, size) != (void *)rx) {
            if (__builtin_available(iOS 26.0, *)) {
                jit_set_error(error_message, "debugger refused to prepare %u MiB region at %p", size >> 20, (void *)rx);
                ok = false;
            }
        }
        jit_debugger_detach();
        sigaction(SIGTRAP, &saved, nullptr);
    } else if (ok) {
        if (__builtin_available(iOS 26.0, *)) {
            jit_set_error(error_message, "debugger is not attached (P_TRACED is clear)");
            ok = false;
        }
    }
#endif
    if (ok) {
        (void)mlock((void *)rw, size);
        ok = jit_probe((uint8_t *)rx, (uint8_t *)rw, error_message);
    }
    if (!ok) {
        if (rw != 0) {
            (void)munlock((void *)rw, size);
            vm_deallocate(mach_task_self(), rw, size);
        }
        if (rx != 0) {
            if (mmap((void *)rx, size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED) {
                vm_deallocate(mach_task_self(), rx, size);
                jit_region.reserved = 0;
            }
        }
        return false;
    }
    jit_region.reserved = 0;
    jit_region.rx = (uint8_t *)rx;
    jit_region.rw = (uint8_t *)rw;
    return true;
}
#else
static bool jit_prepare_region(char **error_message) {
    uintptr_t rx = jit_reserve(error_message);
    if (rx == 0) {
        return false;
    }
    jit_region.rx = (uint8_t *)rx;
    jit_region.rw = (uint8_t *)rx;
    return true;
}
#endif

size_t cpu_jit_footprint(void) {
    if (!atomic_load_explicit(&jit_region.available, memory_order_relaxed)) {
        return 0;
    }
    return JIT_REGION_MAX +
           (size_t)(JIT_REGION_MAX / JIT_BLOCK_BYTES) * (sizeof(cpu_jit_block_t) + sizeof(cpu_jit_patch_t)) +
           (size_t)(JIT_REGION_MAX / JIT_BUCKET_BYTES) * 2U * sizeof(uint32_t);
}

bool swipterm_jit_is_prepared(void) {
    return atomic_load_explicit(&jit_region.prepared, memory_order_acquire);
}

bool swipterm_jit_is_enabled(void) {
    return atomic_load_explicit(&jit_region.enabled, memory_order_acquire);
}

void swipterm_jit_set_enabled(bool enabled) {
    atomic_store_explicit(&jit_region.available, true, memory_order_relaxed);
    if (atomic_exchange_explicit(&jit_region.enabled, enabled, memory_order_acq_rel) != enabled) {
        atomic_fetch_add_explicit(&cpu_jit_generation, 1, memory_order_release);
    }
}

bool swipterm_jit_prepare(char **error_message) {
    if (error_message != nullptr) {
        *error_message = nullptr;
    }
    atomic_store_explicit(&jit_region.available, true, memory_order_relaxed);
    pthread_mutex_lock(&jit_region.prepare_lock);
    bool prepared = atomic_load_explicit(&jit_region.prepared, memory_order_relaxed);
    if (!prepared && jit_prepare_region(error_message)) {
        prepared = true;
        atomic_store_explicit(&jit_region.prepared, true, memory_order_release);
        atomic_fetch_add_explicit(&cpu_jit_generation, 1, memory_order_release);
    }
    pthread_mutex_unlock(&jit_region.prepare_lock);
    return prepared;
}

bool cpu_jit_arena_acquire(vmm_machine_t *vm, cpu_jit_arena_t *out) {
    if (!swipterm_jit_is_prepared() || !swipterm_jit_is_enabled()) {
        return false;
    }
    os_unfair_lock_lock(&jit_region.owner_lock);
    bool owned = jit_region.owner == NULL || jit_region.owner == vm;
    if (owned) {
        jit_region.owner = vm;
        jit_region.users++;
    }
    os_unfair_lock_unlock(&jit_region.owner_lock);
    if (!owned) {
        return false;
    }
    *out = (cpu_jit_arena_t){
        .rx = jit_region.rx,
        .rw = jit_region.rw,
        .size = jit_region.size,
        .base_offset = (uint32_t)((uintptr_t)jit_region.rx - cpu_fast_base()),
    };
    return true;
}

void cpu_jit_arena_release(vmm_machine_t *vm) {
    os_unfair_lock_lock(&jit_region.owner_lock);
    if (jit_region.owner == vm && --jit_region.users == 0) {
        jit_region.owner = NULL;
        atomic_fetch_add_explicit(&cpu_jit_generation, 1, memory_order_release);
    }
    os_unfair_lock_unlock(&jit_region.owner_lock);
}

void cpu_jit_arena_write(const cpu_jit_arena_t *arena, uint32_t offset, const uint32_t *code, uint32_t words) {
#if JIT_DUAL_MAPPING
    memcpy(arena->rw + offset, code, (size_t)words * 4);
#else
    pthread_jit_write_protect_np(0);
    memcpy(arena->rw + offset, code, (size_t)words * 4);
    pthread_jit_write_protect_np(1);
#endif
    sys_icache_invalidate(arena->rx + offset, (size_t)words * 4);
}

void cpu_jit_arena_patch(const cpu_jit_arena_t *arena, uint32_t offset, uint32_t word) {
#if JIT_DUAL_MAPPING
    __atomic_store_n((uint32_t *)(arena->rw + offset), word, __ATOMIC_RELAXED);
#else
    pthread_jit_write_protect_np(0);
    __atomic_store_n((uint32_t *)(arena->rw + offset), word, __ATOMIC_RELAXED);
    pthread_jit_write_protect_np(1);
#endif
    sys_icache_invalidate(arena->rx + offset, 4);
}
