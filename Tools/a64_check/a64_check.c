#include "board/machine.h"
#include "cpu/cpu_internal.h"
#include "state.h"
#include "swipterm.h"

#include <fcntl.h>
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

typedef struct a64_leaf_field {
    const char *name;
    uint8_t start;
    uint8_t width;
} a64_leaf_field_t;

typedef struct a64_leaf {
    const char *name;
    const char *subgroup;
    uint32_t mask;
    uint32_t value;
    bool (*guard)(uint32_t word);
    uint32_t features;
    uint32_t field_count;
    a64_leaf_field_t fields[16];
} a64_leaf_t;

#include "generated/a64_leaves.inc"

enum : uint32_t {
    LEAF_COUNT = sizeof(a64_leaves) / sizeof(a64_leaves[0]),
    WORD_WFI = 0xD503207FU,
    SCRATCH_STATE = 27,
    SCRATCH_SP = 26,
    PLATFORM_REG = 18,
};

enum : uint64_t {
    RAM_SIZE = 64ULL << 20,
    BLOCK_SIZE = 2ULL << 20,
    TABLE_OFFSET = 0x20000ULL,
    DATA_SIZE = 0x20000ULL,
    TILE_BYTES = TILE_INSNS * 4,
};

#define VECTOR_BASE (ram_base + 0x10000ULL)
#define DATA_BASE (ram_base + 0x100000ULL)
#define DATA_CENTER (DATA_BASE + DATA_SIZE / 2)
#define CODE_BASE (ram_base + 0x400000ULL)
#define CODE_END (ram_base + RAM_SIZE)

typedef enum outcome : uint8_t {
    OUTCOME_OK,
    OUTCOME_UNDEFINED,
    OUTCOME_ABORT,
    OUTCOME_OTHER,
} outcome_t;

const ir_insn_t *cpu_fast_resolve(vcpu_t *cpu);

static vmm_machine_t machine;
static vcpu_t *cpu;
static uint64_t ram_base;
static uint64_t code_cursor;
static uint8_t *data_pristine;
static uint8_t *native_code;
static uint32_t *native_slots;
static sigjmp_buf native_jump;
static volatile sig_atomic_t native_signal;
static uint32_t host_features;
static bool jit_mode;
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

static bool fast_path(const ir_insn_t *ir) {
    return jit_mode ? cpu_jit_native(ir) : ir->fast_offset != 0;
}

static uint64_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static uint32_t field_set(uint32_t word, const a64_leaf_field_t *f, uint32_t value) {
    uint32_t mask = ((1U << f->width) - 1U) << f->start;
    return (word & ~mask) | ((value << f->start) & mask);
}

static bool field_is_register(const a64_leaf_field_t *f) {
    return f->name[0] == 'R' && f->name[1] >= 'a' && f->name[1] <= 't' &&
           (f->name[2] == 0 || (f->name[2] == '2' && f->name[3] == 0));
}

static bool leaf_matches(const a64_leaf_t *leaf, uint32_t word) {
    return (word & leaf->mask) == leaf->value && (!leaf->guard || leaf->guard(word));
}

static bool subgroup_is(const a64_leaf_t *leaf, const char *prefix) {
    return strncmp(leaf->subgroup, prefix, strlen(prefix)) == 0;
}

static void map_ram(void) {
    uint64_t *tables = (uint64_t *)(ram_base + TABLE_OFFSET);
    uint64_t next_table = 1;
    for (uint64_t offset = 0; offset < RAM_SIZE; offset += BLOCK_SIZE) {
        uint64_t va = ram_base + offset;
        uint64_t *table = tables;
        for (uint32_t shift = 39; shift > 21; shift -= 9) {
            uint64_t *entry = &table[(va >> shift) & 511];
            if (*entry == 0) {
                *entry = (VMM_RAM_BASE + TABLE_OFFSET + next_table++ * VMM_PAGE_SIZE) | 3;
            }
            table = (uint64_t *)(ram_base + ((*entry & ~0xFFFULL) - VMM_RAM_BASE));
        }
        table[(va >> 21) & 511] = (VMM_RAM_BASE + offset) | (1ULL << 54) | (1ULL << 10) | (3ULL << 8) | 1;
    }
    cpu->mair_el1 = 0xFF;
    cpu->tcr_el1 = (5ULL << 32) | (1ULL << 23) | 16;
    cpu->ttbr0_el1 = VMM_RAM_BASE + TABLE_OFFSET;
    cpu->sctlr_el1 |= 1;
    mmu_update_asid_salt(cpu);
}

static void setup_machine(void) {
    uint8_t *ram = mmap(nullptr, RAM_SIZE + BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (ram == MAP_FAILED) {
        perror("mmap guest ram");
        exit(1);
    }
    ram_base = ((uint64_t)ram + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
    code_cursor = CODE_BASE;
    machine.ram = (uint8_t *)ram_base;
    machine.ram_size = RAM_SIZE;
    machine.cpu_count = 1;
    cpu = aligned_alloc(64, (sizeof(vcpu_t) + 63) & ~(size_t)63);
    machine.cpus[0] = cpu;
    vcpu_init(cpu, &machine, 0);
    map_ram();
    cpu->vbar_el1 = VECTOR_BASE;
    for (uint32_t i = 0; i < 0x800 / 4; ++i) {
        ((uint32_t *)VECTOR_BASE)[i] = WORD_WFI;
    }
    data_pristine = malloc(DATA_SIZE);
    for (uint64_t i = 0; i < DATA_SIZE; i += 8) {
        uint64_t v = rng();
        memcpy(data_pristine + i, &v, 8);
    }
    memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
}

static uint64_t place_code(const uint32_t *words, uint32_t count) {
    uint64_t bytes = ((uint64_t)count * 4 + 4 + TILE_BYTES - 1) & ~(uint64_t)(TILE_BYTES - 1);
    if (code_cursor + bytes > CODE_END) {
        code_cursor = CODE_BASE;
        vcpu_flush_icache_all(cpu);
    }
    uint64_t at = code_cursor;
    code_cursor += bytes;
    uint32_t *out = (uint32_t *)at;
    memset(out, 0, bytes);
    memcpy(out, words, (size_t)count * 4);
    out[count] = WORD_WFI;
    return at;
}

static const ir_insn_t *resolve_at(uint64_t pc) {
    cpu->pc = pc;
    cpu->el = 1;
    cpu->spsel = 1;
    cpu->daif = 0x3C0U;
    atomic_store(&cpu->stop_requested, false);
    return cpu_fast_resolve(cpu);
}

static outcome_t guest_run(uint64_t pc, uint32_t count, const test_state_t *in, test_state_t *out,
                           bool force_cold, bool fp_active, uint32_t *fast_mask) {
    ir_insn_t *ir = (ir_insn_t *)resolve_at(pc);
    ir_page_t *page = ((cpu_icache_t *)cpu->icache)->cur_page;
    *fast_mask = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (fast_path(&ir[i])) {
            if (jit_mode) {
                page->jit->entry[ir + i - page->insns] &= ~0xFFU;
            }
            *fast_mask |= 1U << i;
            if (force_cold) {
                ir[i].fast_offset = 0;
            }
        }
    }
    memcpy(cpu->x, in->x, sizeof(in->x));
    cpu->x[31] = 0;
    cpu->sp_el[1] = in->sp;
    cpu->nzcv = (uint32_t)in->nzcv;
    cpu->fpcr = (uint32_t)in->fpcr;
    cpu->fpsr = (uint32_t)in->fpsr;
    memcpy(cpu->v, in->v, sizeof(in->v));
    cpu->excl_valid = false;
    cpu->esr_el1 = 0;
    cpu->stop_reason = VCPU_STOP_NONE;
    atomic_store(&cpu->irq_pending, 0);
    if (in->system) {
        cpu->daif = (uint32_t)in->daif;
        atomic_store(&cpu->irq_pending, (uint32_t)in->irq_pending);
        cpu->sp_el[0] = in->sp_el0;
        cpu->elr_el1 = in->elr_el1;
        cpu->spsr_el1 = in->spsr_el1;
        cpu->esr_el1 = in->esr_el1;
        cpu->far_el1 = in->far_el1;
        cpu->tpidr_el0 = in->tpidr_el0;
        cpu->tpidr_el1 = in->tpidr_el1;
        cpu->tpidrro_el0 = in->tpidrro_el0;
    }
    if (fp_active) {
        cpu_fp_enter_guest_slow(cpu);
    }
    vcpu_run(cpu);
    memcpy(out->x, cpu->x, sizeof(out->x));
    out->sp = cpu->sp_el[1];
    out->nzcv = cpu->nzcv;
    out->fpcr = in->system ? cpu->fpcr : in->fpcr;
    out->fpsr = cpu->fpsr;
    memcpy(out->v, cpu->v, sizeof(out->v));
    out->system = in->system;
    out->daif = cpu->daif;
    out->irq_pending = in->irq_pending;
    out->sp_el0 = cpu->sp_el[0];
    out->elr_el1 = cpu->elr_el1;
    out->spsr_el1 = cpu->spsr_el1;
    out->esr_el1 = cpu->esr_el1;
    out->far_el1 = cpu->far_el1;
    out->tpidr_el0 = cpu->tpidr_el0;
    out->tpidr_el1 = cpu->tpidr_el1;
    out->tpidrro_el0 = cpu->tpidrro_el0;
    if (cpu->pc - VECTOR_BASE < 0x800) {
        uint32_t ec = (uint32_t)(cpu->esr_el1 >> 26);
        return ec == 0 ? OUTCOME_UNDEFINED : (ec == 0x25 || ec == 0x26) ? OUTCOME_ABORT : OUTCOME_OTHER;
    }
    return cpu->pc == pc + (uint64_t)count * 4 + 4 ? OUTCOME_OK : OUTCOME_OTHER;
}

static void native_handler(int sig) {
    native_signal = sig;
    siglongjmp(native_jump, 1);
}

static void setup_native(void) {
    size_t size = (size_t)(native_template_end - native_template);
    native_code = mmap(nullptr, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_JIT | MAP_ANON | MAP_PRIVATE, -1, 0);
    if (native_code == MAP_FAILED) {
        perror("mmap jit");
        exit(1);
    }
    pthread_jit_write_protect_np(0);
    memcpy(native_code, native_template, size);
    pthread_jit_write_protect_np(1);
    native_slots = (uint32_t *)(native_code + (native_template_slots - native_template));

    stack_t alt = {.ss_sp = malloc(1 << 16), .ss_size = 1 << 16};
    sigaltstack(&alt, nullptr);
    struct sigaction action = {.sa_handler = native_handler, .sa_flags = SA_ONSTACK | SA_NODEFER};
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGTRAP, &action, nullptr);
}

static outcome_t native_run(const uint32_t *words, uint32_t count, const test_state_t *in, test_state_t *out) {
    pthread_jit_write_protect_np(0);
    for (uint32_t i = 0; i < NATIVE_SLOTS; ++i) {
        native_slots[i] = i < count ? words[i] : 0xD503201FU;
    }
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(native_slots, NATIVE_SLOTS * 4);
    *out = *in;
    uint64_t fpcr = __builtin_arm_rsr64("FPCR");
    uint64_t fpsr = __builtin_arm_rsr64("FPSR");
    if (sigsetjmp(native_jump, 1) != 0) {
        __builtin_arm_wsr64("FPCR", fpcr);
        __builtin_arm_wsr64("FPSR", fpsr);
        return native_signal == SIGILL ? OUTCOME_UNDEFINED
             : native_signal == SIGBUS || native_signal == SIGSEGV ? OUTCOME_ABORT : OUTCOME_OTHER;
    }
    ((void (*)(test_state_t *))native_code)(out);
    return OUTCOME_OK;
}

static const uint64_t special_f64[] = {
    0x0000000000000000ULL, 0x8000000000000000ULL, 0x3FF0000000000000ULL, 0xBFF0000000000000ULL,
    0x7FF0000000000000ULL, 0xFFF0000000000000ULL, 0x7FF8000000000000ULL, 0x7FF4000000000001ULL,
    0xFFF8000000000123ULL, 0x0000000000000001ULL, 0x000FFFFFFFFFFFFFULL, 0x0010000000000000ULL,
    0x7FEFFFFFFFFFFFFFULL, 0x3FE0000000000000ULL, 0x4008000000000000ULL, 0x43E0000000000000ULL,
    0xC3E0000000000000ULL, 0x41E0000000000000ULL, 0x3FF8000000000000ULL, 0x4004000000000000ULL,
    0x433FFFFFFFFFFFFFULL, 0x3CA0000000000000ULL,
};
static const uint32_t special_f32[] = {
    0x00000000U, 0x80000000U, 0x3F800000U, 0xBF800000U, 0x7F800000U, 0xFF800000U, 0x7FC00000U,
    0x7FA00001U, 0xFFC00123U, 0x00000001U, 0x007FFFFFU, 0x00800000U, 0x7F7FFFFFU, 0x3F000000U,
    0x40400000U, 0x5F000000U, 0xDF000000U, 0x4F000000U, 0x3FC00000U, 0x40200000U, 0x4B7FFFFFU,
};
static const uint16_t special_f16[] = {
    0x0000, 0x8000, 0x3C00, 0xBC00, 0x7C00, 0xFC00, 0x7E00, 0x7D01, 0xFE12, 0x0001, 0x03FF,
    0x0400, 0x7BFF, 0x3800, 0x4200, 0x3E00, 0x4100,
};
static const uint64_t special_int[] = {
    0, 1, 2, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000ULL, 0xFFFFFFFFULL,
    0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 63, 64, 31, 32,
};

#define PICK(array) ((array)[rng() % (sizeof(array) / sizeof((array)[0]))])

static void random_state(test_state_t *st) {
    for (uint32_t i = 0; i < 31; ++i) {
        uint64_t r = rng();
        switch (r & 7) {
        case 0: st->x[i] = PICK(special_int); break;
        case 1: st->x[i] = (uint64_t)(int64_t)(int8_t)(r >> 8); break;
        case 2: st->x[i] = PICK(special_f64); break;
        case 3: st->x[i] = PICK(special_f32); break;
        default: st->x[i] = rng(); break;
        }
    }
    st->sp = DATA_CENTER;
    st->nzcv = (rng() & 15) << 28;
    uint64_t mode = rng();
    st->fpcr = (mode & 1) ? 0 : ((mode >> 1) & 3) << 22 | ((mode >> 3) & 1) << 24 |
                                ((mode >> 4) & 1) << 25 | ((mode >> 5) & 1) << 26 | ((mode >> 6) & 1) << 19;
    st->fpsr = (mode & 0x100) ? 0x0800009FU & (uint32_t)rng() : 0;
    st->system = 0;
    st->daif = (rng() & 15) << 6;
    st->irq_pending = (rng() & 3) == 0;
    st->sp_el0 = rng();
    st->elr_el1 = rng();
    st->spsr_el1 = rng();
    st->esr_el1 = rng();
    st->far_el1 = rng();
    st->tpidr_el0 = rng();
    st->tpidr_el1 = rng();
    st->tpidrro_el0 = rng();
    for (uint32_t i = 0; i < 32; ++i) {
        uint8_t bytes[16];
        uint64_t kind = rng() % 6;
        for (uint32_t b = 0; b < 16;) {
            uint64_t r = rng();
            bool special = (r & 3) != 0;
            switch (kind) {
            case 0: { uint64_t v = special ? PICK(special_f64) : rng(); memcpy(bytes + b, &v, 8); b += 8; break; }
            case 1: { uint32_t v = special ? PICK(special_f32) : (uint32_t)rng(); memcpy(bytes + b, &v, 4); b += 4; break; }
            case 2: { uint16_t v = special ? PICK(special_f16) : (uint16_t)rng(); memcpy(bytes + b, &v, 2); b += 2; break; }
            case 3: { uint64_t v = special ? PICK(special_int) : rng(); memcpy(bytes + b, &v, 8); b += 8; break; }
            case 4: { uint32_t v = (uint32_t)(special ? PICK(special_int) : rng()); memcpy(bytes + b, &v, 4); b += 4; break; }
            default: { uint64_t v = rng(); memcpy(bytes + b, &v, 8); b += 8; break; }
            }
        }
        memcpy(&st->v[i], bytes, 16);
    }
}

static uint32_t pick_register(uint32_t width, uint32_t used) {
    for (;;) {
        uint32_t r = (uint32_t)rng() & ((1U << width) - 1U);
        if (r == PLATFORM_REG || r == SCRATCH_STATE || r == SCRATCH_SP || (used & (1U << r))) {
            continue;
        }
        return r;
    }
}

static bool subgroup_is_atomic(const a64_leaf_t *leaf) {
    return strcmp(leaf->subgroup, "ldst/memop") == 0 || subgroup_is(leaf, "ldst/comswap") ||
           subgroup_is(leaf, "ldst/ldstexcl");
}

static uint32_t template_register(const a64_leaf_t *leaf, const a64_leaf_field_t *f) {
    if (subgroup_is(leaf, "ldst/")) {
        return f->name[1] == 't' ? 0 : f->name[1] == 's' ? 1 : 12;
    }
    return f->name[1] == 'd' ? 0 : f->name[1] == 'n' ? 1 : f->name[1] == 'm' ? 2 : 3;
}

static bool random_instance(const a64_leaf_t *leaf, const uint32_t *template, uint32_t *word_out,
                            test_state_t *st, bool *native_differs) {
    bool memory = subgroup_is(leaf, "ldst/");
    bool pair = strcmp(leaf->subgroup, "ldst/comswappr") == 0;
    for (uint32_t attempt = 0; attempt < 64; ++attempt) {
        uint32_t word = template ? *template : leaf->value | ((uint32_t)rng() & ~leaf->mask);
        uint32_t used = 0;
        uint32_t base = 32, index = 32;
        for (uint32_t i = 0; i < leaf->field_count; ++i) {
            const a64_leaf_field_t *f = &leaf->fields[i];
            if (!field_is_register(f)) {
                continue;
            }
            if (template && ((*template >> f->start) & ((1U << f->width) - 1U)) != template_register(leaf, f)) {
                continue;
            }
            uint32_t r;
            if (pair && strcmp(f->name, "Rn") != 0) {
                do {
                    r = pick_register(f->width, used) & ~1U;
                } while (r == 16 || r == 24 || r == 30 || (used & (3U << r)));
                used |= 3U << r;
            } else {
                r = pick_register(f->width, memory ? used : 0);
                used |= 1U << r;
            }
            if (memory && strcmp(f->name, "Rn") == 0) {
                base = r;
            }
            if (memory && strcmp(f->name, "Rm") == 0) {
                index = r;
            }
            word = field_set(word, f, r);
        }
        if (!leaf_matches(leaf, word)) {
            continue;
        }
        random_state(st);
        if (memory) {
            uint64_t r = rng();
            uint64_t address = DATA_CENTER + ((r >> 8) & 0x7F0);
            if ((r & 15) == 0) {
                address += (r >> 20) & 15;
            } else if ((r & 15) == 1) {
                address = (address | 0xFFF) - ((r >> 20) & 31);
            }
            if (base == 31) {
                address &= ~15ULL;
                st->sp = address;
            } else if (base < 31) {
                st->x[base] = address;
            }
            if (index < 31) {
                st->x[index] = (r >> 32) & 0x38;
            }
            bool ordered = strcmp(leaf->subgroup, "ldst/ldapstl_unscaled") == 0;
            uint64_t effective = address + (uint64_t)cpu_sext((word >> 12) & 0x1FFU, 9);
            *native_differs = (subgroup_is_atomic(leaf) && (address & 15) != 0) ||
                              (ordered && (effective & ((1ULL << (word >> 30)) - 1)) != 0);
        }
        *word_out = word;
        return true;
    }
    return false;
}

static bool state_equal(const test_state_t *a, const test_state_t *b, char *why, size_t why_size) {
    for (uint32_t i = 0; i < 31; ++i) {
        if (i == PLATFORM_REG || i == SCRATCH_STATE || i == SCRATCH_SP) {
            continue;
        }
        if (a->x[i] != b->x[i]) {
            snprintf(why, why_size, "x%u %016llx != %016llx", i, a->x[i], b->x[i]);
            return false;
        }
    }
    if (a->sp != b->sp) {
        snprintf(why, why_size, "sp %016llx != %016llx", a->sp, b->sp);
        return false;
    }
    if ((a->nzcv ^ b->nzcv) & 0xF0000000U) {
        snprintf(why, why_size, "nzcv %llx != %llx", a->nzcv >> 28, b->nzcv >> 28);
        return false;
    }
    if ((a->fpsr ^ b->fpsr) & CPU_FPSR_MASK) {
        snprintf(why, why_size, "fpsr %08llx != %08llx", a->fpsr & CPU_FPSR_MASK, b->fpsr & CPU_FPSR_MASK);
        return false;
    }
    for (uint32_t i = 0; i < 32; ++i) {
        if (a->v[i] != b->v[i]) {
            snprintf(why, why_size, "v%u %016llx%016llx != %016llx%016llx", i,
                     (uint64_t)(a->v[i] >> 64), (uint64_t)a->v[i], (uint64_t)(b->v[i] >> 64), (uint64_t)b->v[i]);
            return false;
        }
    }
    return true;
}

static bool compare_run(outcome_t guest, const test_state_t *guest_state, const uint8_t *guest_data,
                        outcome_t native, const test_state_t *native_state, const uint8_t *native_data,
                        bool memory, char *why, size_t why_size) {
    if (guest != native) {
        snprintf(why, why_size, "outcome %d != %d", guest, native);
        return false;
    }
    if (guest != OUTCOME_OK) {
        return true;
    }
    if (!state_equal(guest_state, native_state, why, why_size)) {
        return false;
    }
    if (memory && memcmp(guest_data, native_data, DATA_SIZE) != 0) {
        uint64_t at = 0;
        while (guest_data[at] == native_data[at]) {
            ++at;
        }
        snprintf(why, why_size, "memory +%llx %02x != %02x", at, guest_data[at], native_data[at]);
        return false;
    }
    return true;
}

static uint32_t query_host_features(void) {
    uint32_t mask = A64_LEAF_FEAT_AdvSIMD | A64_LEAF_FEAT_FP;
    for (size_t i = 0; i < sizeof(a64_leaf_feature_names) / sizeof(a64_leaf_feature_names[0]); ++i) {
        char name[64];
        int value = 0;
        size_t size = sizeof(value);
        snprintf(name, sizeof(name), "hw.optional.arm.%s", a64_leaf_feature_names[i]);
        if (sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value) {
            mask |= 1U << i;
        }
    }
    return mask;
}

static bool leaf_fuzzable(const a64_leaf_t *leaf) {
    if (leaf->features & ~host_features) {
        return false;
    }
    if (subgroup_is(leaf, "control/") || subgroup_is(leaf, "reserved/")) {
        return false;
    }
    return strcmp(leaf->subgroup, "dpimm/pcreladdr") != 0 && strcmp(leaf->subgroup, "ldst/loadlit") != 0 &&
           strcmp(leaf->subgroup, "ldst/ldst_unpriv") != 0 && strcmp(leaf->subgroup, "ldst/memcms") != 0;
}

static void warm_data_tlb(void) {
    test_state_t st, out;
    uint32_t fast;
    random_state(&st);
    for (uint64_t page = DATA_BASE; page < DATA_BASE + DATA_SIZE; page += VMM_PAGE_SIZE) {
        uint32_t words[2] = {0xF9400020U, 0xF9000020U};
        st.x[1] = page;
        guest_run(place_code(words, 2), 2, &st, &out, false, false, &fast);
    }
    memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
}

static struct {
    uint8_t *fast_data;
    uint8_t *cold_data;
    uint8_t *native_data;
    bool verbose;
    uint32_t total, fast, fast_bad, cold_bad, undefined_executed;
} fuzz;

static void fuzz_report(const char *path, const a64_leaf_t *leaf, uint32_t word, const test_state_t *in,
                        const char *why, uint32_t *shown) {
    if (*shown < (fuzz.verbose ? 8U : 1U)) {
        ++*shown;
        printf("MISMATCH %s %s %08x fpcr=%08llx: %s\n", path, leaf->name, word, in->fpcr, why);
    }
}

static void fuzz_case(const a64_leaf_t *leaf, const uint32_t *template, uint32_t *shown) {
    bool memory = subgroup_is(leaf, "ldst/");
    uint32_t word, fast_mask;
    test_state_t in, native_state, fast_state, cold_state;
    bool native_differs = false;
    char why[160];
    if (!random_instance(leaf, template, &word, &in, &native_differs)) {
        return;
    }
    outcome_t native = native_run(&word, 1, &in, &native_state);
    if (memory) {
        memcpy(fuzz.native_data, (void *)DATA_BASE, DATA_SIZE);
        memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
    }
    fuzz.total++;
    uint64_t pc = place_code(&word, 1);
    bool is_fast = fast_path(resolve_at(pc));
    fuzz.fast += is_fast;
    if (native == OUTCOME_UNDEFINED && is_fast) {
        fuzz.fast_bad++;
        fuzz_report("fast", leaf, word, &in, "fast path for an undefined instruction", shown);
        return;
    }
    outcome_t fast = OUTCOME_OK;
    if (is_fast) {
        fast = guest_run(pc, 1, &in, &fast_state, false, true, &fast_mask);
        if (memory) {
            memcpy(fuzz.fast_data, (void *)DATA_BASE, DATA_SIZE);
            memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
        }
    }
    outcome_t cold = guest_run(pc, 1, &in, &cold_state, true, false, &fast_mask);
    if (memory) {
        memcpy(fuzz.cold_data, (void *)DATA_BASE, DATA_SIZE);
        memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
    }
    if (native_differs || (native == OUTCOME_UNDEFINED && cold == OUTCOME_OK)) {
        fuzz.undefined_executed += native == OUTCOME_UNDEFINED;
        native = cold;
        native_state = cold_state;
        memcpy(fuzz.native_data, fuzz.cold_data, memory ? DATA_SIZE : 0);
    }
    if (is_fast && !compare_run(fast, &fast_state, fuzz.fast_data, native, &native_state, fuzz.native_data,
                                memory, why, sizeof(why))) {
        fuzz.fast_bad++;
        fuzz_report("fast", leaf, word, &in, why, shown);
    }
    if (!compare_run(cold, &cold_state, fuzz.cold_data, native, &native_state, fuzz.native_data,
                     memory, why, sizeof(why))) {
        fuzz.cold_bad++;
        fuzz_report("cold", leaf, word, &in, why, shown);
    }
}

static void fuzz_pair(const a64_leaf_t *first, const a64_leaf_t *second, uint32_t *shown) {
    uint32_t words[2], fast_mask;
    test_state_t in, discard, native_state, guest_state;
    bool native_differs;
    char why[160];
    if (!random_instance(first, nullptr, &words[0], &in, &native_differs) ||
        !random_instance(second, nullptr, &words[1], &discard, &native_differs)) {
        return;
    }
    outcome_t native = native_run(words, 2, &in, &native_state);
    if (native != OUTCOME_OK) {
        return;
    }
    fuzz.total++;
    outcome_t guest = guest_run(place_code(words, 2), 2, &in, &guest_state, false, false, &fast_mask);
    fuzz.fast += fast_mask == 3;
    if (!compare_run(guest, &guest_state, nullptr, native, &native_state, nullptr, false, why, sizeof(why))) {
        fuzz.fast_bad++;
        if (*shown < 8) {
            ++*shown;
            printf("MISMATCH pair %s %08x, %s %08x fpcr=%08llx: %s\n", first->name, words[0], second->name,
                   words[1], in.fpcr, why);
        }
    }
}

static int fuzz_finish(const char *what) {
    printf("%s: %u cases, %u on the fast path, %u fast mismatches, %u cold mismatches, "
           "%u undefined instructions executed\n",
           what, fuzz.total, fuzz.fast, fuzz.fast_bad, fuzz.cold_bad, fuzz.undefined_executed);
    return fuzz.fast_bad != 0 || fuzz.cold_bad != 0;
}

static void fuzz_setup(void) {
    setup_native();
    warm_data_tlb();
    fuzz.fast_data = malloc(DATA_SIZE);
    fuzz.cold_data = malloc(DATA_SIZE);
    fuzz.native_data = malloc(DATA_SIZE);
}

static int command_fuzz(int argc, char **argv) {
    uint32_t per_leaf = 64;
    const char *filter = nullptr;
    bool pairs = false;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            per_leaf = (uint32_t)strtoul(argv[++i], nullptr, 0);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            rng_state = strtoull(argv[++i], nullptr, 0) | 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            fuzz.verbose = true;
        } else if (strcmp(argv[i], "-p") == 0) {
            pairs = true;
        } else {
            filter = argv[i];
        }
    }
    fuzz_setup();
    if (pairs) {
        static uint32_t registers_only[LEAF_COUNT];
        uint32_t count = 0, shown = 0;
        for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
            const a64_leaf_t *leaf = &a64_leaves[l];
            if (leaf_fuzzable(leaf) && !subgroup_is(leaf, "ldst/") &&
                (!filter || strstr(leaf->subgroup, filter) || strstr(leaf->name, filter))) {
                registers_only[count++] = l;
            }
        }
        for (uint32_t t = 0; t < per_leaf * count; ++t) {
            fuzz_pair(&a64_leaves[registers_only[rng() % count]], &a64_leaves[registers_only[rng() % count]],
                      &shown);
        }
        return fuzz_finish("pairs");
    }
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        const a64_leaf_t *leaf = &a64_leaves[l];
        if (!leaf_fuzzable(leaf) || (filter && !strstr(leaf->subgroup, filter) && !strstr(leaf->name, filter))) {
            continue;
        }
        uint32_t shown = 0;
        for (uint32_t t = 0; t < per_leaf; ++t) {
            fuzz_case(leaf, nullptr, &shown);
        }
    }
    return fuzz_finish("fuzz");
}

static bool system_equal(const test_state_t *a, const test_state_t *b, char *why, size_t why_size) {
    if (a->fpcr != b->fpcr || memcmp(&a->daif, &b->daif, sizeof(*a) - offsetof(test_state_t, daif)) != 0) {
        snprintf(why, why_size, "system registers daif %llx/%llx fpcr %llx/%llx sp_el0 %llx/%llx elr %llx/%llx "
                 "spsr %llx/%llx esr %llx/%llx far %llx/%llx", a->daif, b->daif, a->fpcr, b->fpcr,
                 a->sp_el0, b->sp_el0, a->elr_el1, b->elr_el1, a->spsr_el1, b->spsr_el1, a->esr_el1, b->esr_el1,
                 a->far_el1, b->far_el1);
        return false;
    }
    return true;
}

static void system_case(const char *name, const uint32_t *words, uint32_t count, test_state_t *in,
                        uint32_t *shown) {
    uint32_t fast_mask;
    test_state_t fast_state, cold_state;
    char why[320];
    in->system = 1;
    fuzz.total++;
    uint64_t pc = place_code(words, count);
    const ir_insn_t *ir = resolve_at(pc);
    for (uint32_t i = 0; i < count; ++i) {
        if (!fast_path(&ir[i])) {
            fuzz.fast_bad++;
            if (*shown < 4) {
                ++*shown;
                printf("MISMATCH system %s %08x: not on the fast path\n", name, words[i]);
            }
            return;
        }
    }
    fuzz.fast++;
    outcome_t fast = guest_run(pc, count, in, &fast_state, false, true, &fast_mask);
    memcpy(fuzz.fast_data, (void *)DATA_BASE, DATA_SIZE);
    memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
    outcome_t cold = guest_run(pc, count, in, &cold_state, true, true, &fast_mask);
    memcpy(fuzz.cold_data, (void *)DATA_BASE, DATA_SIZE);
    memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
    if (!compare_run(fast, &fast_state, fuzz.fast_data, cold, &cold_state, fuzz.cold_data, true, why, sizeof(why)) ||
        (fast == OUTCOME_OK && !system_equal(&fast_state, &cold_state, why, sizeof(why)))) {
        fuzz.fast_bad++;
        if (*shown < 4) {
            ++*shown;
            printf("MISMATCH system %s %08x daif=%llx irq=%llu: %s\n", name, words[0], in->daif, in->irq_pending, why);
        }
    }
}

static uint32_t system_register(uint32_t read, uint32_t op0, uint32_t op1, uint32_t crn, uint32_t crm,
                                uint32_t op2) {
    return 0xD5000000U | read << 21 | op0 << 19 | op1 << 16 | crn << 12 | crm << 8 | op2 << 5;
}

static int command_system(int argc, char **argv) {
    static const struct { const char *name; uint8_t op0, op1, crn, crm, op2; bool read, write; } registers[] = {
        {"DAIF", 3, 3, 4, 2, 1, true, true},         {"NZCV", 3, 3, 4, 2, 0, true, true},
        {"FPCR", 3, 3, 4, 4, 0, true, true},         {"FPSR", 3, 3, 4, 4, 1, true, true},
        {"DCZID_EL0", 3, 3, 0, 0, 7, true, false},   {"CNTVCT_EL0", 3, 3, 14, 0, 2, true, false},
        {"SP_EL0", 3, 0, 4, 1, 0, true, true},       {"ELR_EL1", 3, 0, 4, 0, 1, true, true},
        {"SPSR_EL1", 3, 0, 4, 0, 0, true, true},     {"ESR_EL1", 3, 0, 5, 2, 0, true, true},
        {"FAR_EL1", 3, 0, 6, 0, 0, true, true},      {"VBAR_EL1", 3, 0, 12, 0, 0, true, false},
        {"TPIDR_EL1", 3, 0, 13, 0, 4, true, true},   {"TPIDRRO_EL0", 3, 3, 13, 0, 3, true, true},
        {"TPIDR_EL0", 3, 3, 13, 0, 2, true, true},
    };
    uint32_t per_case = argc > 0 ? (uint32_t)strtoul(argv[0], nullptr, 0) : 256;
    uint32_t shown = 0;
    test_state_t in;
    fuzz_setup();
    for (uint32_t t = 0; t < per_case; ++t) {
        for (size_t r = 0; r < sizeof(registers) / sizeof(registers[0]); ++r) {
            for (uint32_t read = 0; read < 2; ++read) {
                if (read ? !registers[r].read : !registers[r].write) {
                    continue;
                }
                uint32_t word = system_register(read, registers[r].op0, registers[r].op1, registers[r].crn,
                                                registers[r].crm, registers[r].op2) | ((uint32_t)rng() & 31U);
                random_state(&in);
                shown = 0;
                system_case(registers[r].name, &word, 1, &in, &shown);
            }
        }
        static const uint32_t barriers[] = {0xD5033BBFU, 0xD5033B9FU, 0xD5033FDFU, 0xD50338BFU};
        for (uint32_t b = 0; b < 4; ++b) {
            random_state(&in);
            system_case("barrier", &barriers[b], 1, &in, &shown);
        }
        for (uint32_t op2 = 6; op2 < 8; ++op2) {
            uint32_t word = 0xD503401FU | ((uint32_t)rng() & 15U) << 8 | op2 << 5;
            random_state(&in);
            system_case("DAIFSet/DAIFClr", &word, 1, &in, &shown);
        }
        uint32_t rt = pick_register(5, 1U << 31);
        uint32_t zva = 0xD50B7420U | rt;
        random_state(&in);
        in.x[rt] = DATA_CENTER + (rng() & 0x7FFF);
        system_case("DC ZVA", &zva, 1, &in, &shown);

        uint32_t mops_rd = pick_register(5, 1U << 31), mops_rs = pick_register(5, 1U << 31 | 1U << mops_rd);
        uint32_t mops_rn = pick_register(5, 1U << 31 | 1U << mops_rd | 1U << mops_rs);
        uint32_t stage = 1U + ((uint32_t)rng() & 1U);
        uint32_t mops = (rng() & 1) ? 0x19000400U | ((uint32_t)rng() & 1U) << 26 | stage << 22 |
                                          ((uint32_t)rng() & 15U) << 12
                                    : 0x19C00400U | stage << 14 | ((uint32_t)rng() & 3U) << 12;
        mops |= mops_rs << 16 | mops_rn << 5 | mops_rd;
        random_state(&in);
        in.x[mops_rd] = DATA_CENTER + (rng() & 0xFF);
        in.x[mops_rs] = DATA_CENTER + 0x400 + (rng() & 0xFF);
        in.x[mops_rn] = (rng() & 1) ? 0 : -(rng() & 63);
        in.irq_pending = 0;
        system_case("MOPS", &mops, 1, &in, &shown);

        uint32_t size = (uint32_t)rng() & 3U, ordered = (uint32_t)rng() & 1U;
        uint32_t base = pick_register(5, 1U << 31), loaded = pick_register(5, 1U << base);
        uint32_t status = pick_register(5, 1U << base | 1U << loaded);
        uint32_t stored = pick_register(5, 1U << base | 1U << status);
        uint32_t store_size = (rng() & 7) == 0 ? (uint32_t)rng() & 3U : size;
        uint32_t store_base = (rng() & 7) == 0 ? pick_register(5, 1U << status | 1U << stored) : base;
        uint32_t pair[2] = {
            0x085F7C00U | size << 30 | ordered << 15 | base << 5 | loaded,
            0x08007C00U | store_size << 30 | ordered << 15 | status << 16 | store_base << 5 | stored,
        };
        random_state(&in);
        in.x[base] = DATA_CENTER + ((rng() & 0xFF) << 3);
        in.x[store_base] = (rng() & 3) == 0 ? DATA_CENTER + ((rng() & 0xFF) << 3) : in.x[base];
        in.irq_pending = 0;
        system_case("exclusive", pair, 2, &in, &shown);

        if (code_cursor + TILE_BYTES > CODE_END) {
            code_cursor = CODE_BASE;
            vcpu_flush_icache_all(cpu);
        }
        uint64_t pages = ((DATA_CENTER >> 12) - (code_cursor >> 12)) & 0x1FFFFF;
        uint32_t page_reg = pick_register(5, 1U << 31), target = (uint32_t)rng() & 31U;
        uint32_t offset = (uint32_t)rng() & 0xFF;
        uint32_t adrp = 0x90000000U | (uint32_t)(pages & 3) << 29 | (uint32_t)(pages >> 2) << 5 | page_reg;
        uint32_t forms[3][2] = {
            {adrp, 0x91000000U | offset << 10 | page_reg << 5 | target},
            {adrp, 0xF9400000U | offset << 10 | page_reg << 5 | target},
            {adrp, 0xB9400000U | offset << 10 | page_reg << 5 | target},
        };
        random_state(&in);
        in.irq_pending = 0;
        system_case("adrp pair", forms[t % 3], 2, &in, &shown);
    }
    return fuzz_finish("system");
}

static uint32_t find_leaf(uint32_t word);

static int command_stubs(int argc, char **argv) {
    static const struct { uint32_t word; const char *text; } stubs[] = {
#include "generated/a64_passthrough_words.inc"
    };
    uint32_t per_stub = argc > 0 ? (uint32_t)strtoul(argv[0], nullptr, 0) : 8;
    fuzz_setup();
    for (size_t i = 0; i < sizeof(stubs) / sizeof(stubs[0]); ++i) {
        const a64_leaf_t *leaf = &a64_leaves[find_leaf(stubs[i].word)];
        if (leaf->features & ~host_features) {
            continue;
        }
        uint32_t shown = 0, fast_before = fuzz.fast, total_before = fuzz.total;
        for (uint32_t t = 0; t < per_stub; ++t) {
            fuzz_case(leaf, &stubs[i].word, &shown);
        }
        if (fuzz.fast - fast_before != fuzz.total - total_before) {
            fuzz.fast_bad++;
            printf("MISMATCH stub %s %08x %s: not on the fast path\n", leaf->name, stubs[i].word, stubs[i].text);
        }
    }
    return fuzz_finish("stubs");
}

typedef struct coverage {
    uint64_t total;
    uint64_t fast;
} coverage_t;

static int compare_cold(const void *a, const void *b) {
    const coverage_t *x = *(const coverage_t *const *)a, *y = *(const coverage_t *const *)b;
    uint64_t cx = x->total - x->fast, cy = y->total - y->fast;
    return cx < cy ? 1 : cx > cy ? -1 : 0;
}

static void report_coverage(const coverage_t *by_leaf, uint64_t unmatched, uint32_t top) {
    const coverage_t *order[LEAF_COUNT];
    uint64_t total = unmatched, fast = 0;
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        order[l] = &by_leaf[l];
        total += by_leaf[l].total;
        fast += by_leaf[l].fast;
    }
    qsort(order, LEAF_COUNT, sizeof(order[0]), compare_cold);
    printf("%llu instructions, %llu fast (%.3f%%), %llu cold, %llu unallocated\n", total, fast,
           total ? 100.0 * (double)fast / (double)total : 0.0, total - fast - unmatched, unmatched);
    for (uint32_t i = 0; i < LEAF_COUNT && i < top; ++i) {
        const coverage_t *c = order[i];
        if (c->total == c->fast) {
            break;
        }
        const a64_leaf_t *leaf = &a64_leaves[c - by_leaf];
        printf("%10llu cold of %10llu  %.4f%%  %-22s %s\n", c->total - c->fast, c->total,
               100.0 * (double)(c->total - c->fast) / (double)total, leaf->subgroup, leaf->name);
    }
}

static int command_spec(int argc, char **argv) {
    uint32_t per_leaf = argc > 0 ? (uint32_t)strtoul(argv[0], nullptr, 0) : 64;
    coverage_t *by_leaf = calloc(LEAF_COUNT, sizeof(*by_leaf));
    uint32_t cold_leaves = 0, partial_leaves = 0;
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        const a64_leaf_t *leaf = &a64_leaves[l];
        for (uint32_t t = 0; t < per_leaf; ++t) {
            uint32_t word = leaf->value | ((uint32_t)rng() & ~leaf->mask);
            if (!leaf_matches(leaf, word)) {
                continue;
            }
            const ir_insn_t *ir = resolve_at(place_code(&word, 1));
            by_leaf[l].total++;
            by_leaf[l].fast += fast_path(ir);
        }
        cold_leaves += by_leaf[l].fast == 0;
        partial_leaves += by_leaf[l].fast != 0 && by_leaf[l].fast != by_leaf[l].total;
    }
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        const char *subgroup = a64_leaves[l].subgroup;
        bool seen = false;
        for (uint32_t k = 0; k < l && !seen; ++k) {
            seen = strcmp(a64_leaves[k].subgroup, subgroup) == 0;
        }
        if (seen) {
            continue;
        }
        coverage_t sum = {0};
        uint32_t leaves = 0, cold = 0;
        for (uint32_t k = l; k < LEAF_COUNT; ++k) {
            if (strcmp(a64_leaves[k].subgroup, subgroup) == 0) {
                sum.total += by_leaf[k].total;
                sum.fast += by_leaf[k].fast;
                ++leaves;
                cold += by_leaf[k].fast == 0;
            }
        }
        printf("%-24s leaves %4u  cold leaves %4u  fast %6.2f%%\n", subgroup, leaves, cold,
               100.0 * (double)sum.fast / (double)sum.total);
    }
    if (argc > 1) {
        for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
            if (by_leaf[l].fast != by_leaf[l].total && strstr(a64_leaves[l].subgroup, argv[1])) {
                printf("  %-40s %llu/%llu\n", a64_leaves[l].name, by_leaf[l].fast, by_leaf[l].total);
            }
        }
    }
    printf("spec: %u encodings, %u cold, %u partially fast\n", LEAF_COUNT, cold_leaves, partial_leaves);
    return 0;
}

typedef struct elf64_header {
    uint8_t ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_header_t;

typedef struct elf64_section {
    uint32_t name, type;
    uint64_t flags, addr, offset, size;
    uint32_t link, info;
    uint64_t addralign, entsize;
} elf64_section_t;

enum : uint32_t {
    LEAF_NONE = UINT32_MAX,
    LEAF_CACHE_SIZE = 1U << 20,
};

static uint32_t find_leaf(uint32_t word) {
    static uint16_t buckets[32][LEAF_COUNT];
    static uint16_t bucket_sizes[32];
    static struct { uint32_t word, leaf; } cache[LEAF_CACHE_SIZE];
    static bool ready;
    if (!ready) {
        ready = true;
        for (uint32_t b = 0; b < 32; ++b) {
            for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
                if ((((b << 24) ^ a64_leaves[l].value) & a64_leaves[l].mask & 0x1F000000U) == 0) {
                    buckets[b][bucket_sizes[b]++] = (uint16_t)l;
                }
            }
        }
        cache[0].word = 1;
    }
    uint32_t slot = (word * 0x9E3779B1U) >> 12;
    if (cache[slot].word == word) {
        return cache[slot].leaf;
    }
    uint32_t b = (word >> 24) & 31U, found = LEAF_NONE;
    for (uint32_t i = 0; i < bucket_sizes[b]; ++i) {
        if (leaf_matches(&a64_leaves[buckets[b][i]], word)) {
            found = buckets[b][i];
            break;
        }
    }
    cache[slot].word = word;
    cache[slot].leaf = found;
    return found;
}

typedef struct pair_count {
    uint64_t key;
    uint64_t count;
    uint32_t words[2];
} pair_count_t;

enum : uint32_t { PAIR_SLOTS = 1U << 20 };

static int compare_pairs(const void *a, const void *b) {
    const pair_count_t *x = a, *y = b;
    return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

static int command_elf(int argc, char **argv) {
    coverage_t *by_leaf = calloc(LEAF_COUNT, sizeof(*by_leaf));
    pair_count_t *pairs = nullptr;
    uint64_t unmatched = 0, files = 0, adjacent = 0;
    uint32_t top = 40;
    for (int a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "-t") == 0 && a + 1 < argc) {
            top = (uint32_t)strtoul(argv[++a], nullptr, 0);
            continue;
        }
        if (strcmp(argv[a], "-p") == 0) {
            pairs = calloc(PAIR_SLOTS, sizeof(*pairs));
            continue;
        }
        int fd = open(argv[a], O_RDONLY);
        struct stat info;
        if (fd < 0 || fstat(fd, &info) != 0 || info.st_size < (off_t)sizeof(elf64_header_t)) {
            if (fd >= 0) close(fd);
            continue;
        }
        const uint8_t *file = mmap(nullptr, (size_t)info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (file == MAP_FAILED) {
            continue;
        }
        const elf64_header_t *header = (const elf64_header_t *)file;
        if (memcmp(header->ident, "\177ELF\2\1", 6) == 0 && header->machine == 183 &&
            header->shoff + (uint64_t)header->shnum * sizeof(elf64_section_t) <= (uint64_t)info.st_size) {
            ++files;
            const elf64_section_t *sections = (const elf64_section_t *)(file + header->shoff);
            for (uint32_t i = 0; i < header->shnum; ++i) {
                const elf64_section_t *seg = &sections[i];
                if (seg->type != 1 || (seg->flags & 4) == 0 || seg->offset + seg->size > (uint64_t)info.st_size) {
                    continue;
                }
                uint64_t end = (seg->addr + seg->size) & ~3ULL;
                for (uint64_t va = (seg->addr + 3) & ~3ULL; va < end;) {
                    uint64_t page_va = va & ~(uint64_t)VMM_PAGE_MASK;
                    uint64_t page_end = page_va + VMM_PAGE_SIZE < end ? page_va + VMM_PAGE_SIZE : end;
                    if (code_cursor + VMM_PAGE_SIZE > CODE_END) {
                        code_cursor = CODE_BASE;
                        vcpu_flush_icache_all(cpu);
                    }
                    uint64_t guest_page = code_cursor;
                    code_cursor += VMM_PAGE_SIZE;
                    memset((void *)guest_page, 0, VMM_PAGE_SIZE);
                    memcpy((void *)(guest_page + (va - page_va)), file + seg->offset + (va - seg->addr),
                           page_end - va);
                    for (uint64_t tile_va = va & ~(uint64_t)(TILE_BYTES - 1); tile_va < page_end; tile_va += TILE_BYTES) {
                        const ir_insn_t *ir = resolve_at(guest_page + (tile_va - page_va));
                        for (uint32_t k = 0; k < TILE_INSNS; ++k) {
                            uint64_t insn_va = tile_va + k * 4ULL;
                            if (insn_va < va || insn_va >= page_end) {
                                continue;
                            }
                            uint32_t word = *(const uint32_t *)(guest_page + (insn_va - page_va));
                            uint32_t leaf = find_leaf(word);
                            if (leaf == LEAF_NONE) {
                                ++unmatched;
                                continue;
                            }
                            by_leaf[leaf].total++;
                            by_leaf[leaf].fast += fast_path(&ir[k]);
                            if (pairs && k + 1 < TILE_INSNS && insn_va + 4 < page_end && ir[k].fast_offset != 0 &&
                                ir[k + 1].fast_offset != 0) {
                                uint64_t key = (uint64_t)ir[k].fast_offset << 32 | ir[k + 1].fast_offset;
                                uint32_t slot = (uint32_t)((key * 0x9E3779B97F4A7C15ULL) >> 44);
                                while (pairs[slot].count != 0 && pairs[slot].key != key) {
                                    slot = (slot + 1) & (PAIR_SLOTS - 1);
                                }
                                pairs[slot].key = key;
                                pairs[slot].count++;
                                pairs[slot].words[0] = word;
                                pairs[slot].words[1] = *(const uint32_t *)(guest_page + (insn_va + 4 - page_va));
                                ++adjacent;
                            }
                        }
                    }
                    va = page_end;
                }
            }
        }
        munmap((void *)file, (size_t)info.st_size);
    }
    printf("%llu ELF files\n", files);
    if (pairs) {
        qsort(pairs, PAIR_SLOTS, sizeof(*pairs), compare_pairs);
        for (uint32_t i = 0; i < top && pairs[i].count != 0; ++i) {
            printf("%10llu  %6.3f%%  %08x %08x\n", pairs[i].count, 100.0 * (double)pairs[i].count / (double)adjacent,
                   pairs[i].words[0], pairs[i].words[1]);
        }
        return 0;
    }
    report_coverage(by_leaf, unmatched, top);
    return 0;
}

static int command_coverage(int argc, char **argv) {
    static struct { char prefix[96]; bool used; } allowed[256];
    uint32_t allowed_count = 0, failures = 0, slow = 0;
    if (argc < 1) {
        return 2;
    }
    FILE *in = fopen(argv[0], "r");
    if (!in) {
        perror(argv[0]);
        return 1;
    }
    char line[512];
    while (fgets(line, sizeof(line), in) && allowed_count < 256) {
        if (sscanf(line, "%95s", allowed[allowed_count].prefix) == 1 && allowed[allowed_count].prefix[0] != '#') {
            ++allowed_count;
        }
    }
    fclose(in);
    setup_native();
    const uint32_t udf = 0;
    ir_handler_fn undefined = resolve_at(place_code(&udf, 1))->handler;
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        const a64_leaf_t *leaf = &a64_leaves[l];
        bool native = leaf_fuzzable(leaf);
        uint32_t valid = 0, fast = 0, undefined_count = 0;
        for (uint32_t t = 0; t < 64; ++t) {
            uint32_t word;
            test_state_t state, out;
            bool native_differs;
            if (native) {
                if (!random_instance(leaf, nullptr, &word, &state, &native_differs) ||
                    native_run(&word, 1, &state, &out) == OUTCOME_UNDEFINED) {
                    continue;
                }
                memcpy((void *)DATA_BASE, data_pristine, DATA_SIZE);
            } else {
                word = leaf->value | ((uint32_t)rng() & ~leaf->mask);
                if (!leaf_matches(leaf, word)) {
                    continue;
                }
            }
            const ir_insn_t *ir = resolve_at(place_code(&word, 1));
            ++valid;
            fast += fast_path(ir);
            undefined_count += ir->handler == undefined;
        }
        if (fast == valid || ((leaf->features & ~host_features) != 0 && undefined_count == valid)) {
            continue;
        }
        ++slow;
        char name[160];
        snprintf(name, sizeof(name), "%s/%s", leaf->subgroup, leaf->name);
        bool listed = false;
        for (uint32_t a = 0; a < allowed_count; ++a) {
            if (strncmp(name, allowed[a].prefix, strlen(allowed[a].prefix)) == 0) {
                allowed[a].used = listed = true;
            }
        }
        if (!listed) {
            ++failures;
            printf("unlisted slow path: %s (%u of %u fast)\n", name, fast, valid);
        }
    }
    for (uint32_t a = 0; a < allowed_count; ++a) {
        if (!allowed[a].used) {
            ++failures;
            printf("listed but fast: %s\n", allowed[a].prefix);
        }
    }
    printf("coverage: %u of %u encodings take the slow path, %u problems\n", slow, LEAF_COUNT, failures);
    return failures != 0;
}

typedef struct system_word {
    uint32_t word;
    uint64_t count;
} system_word_t;

static int compare_system_words(const void *a, const void *b) {
    const system_word_t *x = a, *y = b;
    return x->count < y->count ? 1 : x->count > y->count ? -1 : 0;
}

static int command_stats(int argc, char **argv) {
    static const char *const counters[] = {"dispatches", "cold", "walks", "resolves", "resolve_misses",
                                           "pages_replaced", "dropped", "tiles"};
    enum { COUNTERS = sizeof(counters) / sizeof(counters[0]) };
    uint64_t totals[COUNTERS] = {0};
    uint64_t *by_leaf = calloc(LEAF_COUNT, sizeof(*by_leaf));
    static system_word_t system_words[4096];
    uint32_t system_count = 0, top = 40;
    uint64_t unmatched = 0;
    for (int a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "-t") == 0 && a + 1 < argc) {
            top = (uint32_t)strtoul(argv[++a], nullptr, 0);
            continue;
        }
        FILE *in = fopen(argv[a], "r");
        if (!in) {
            perror(argv[a]);
            return 1;
        }
        char line[128], key[32];
        while (fgets(line, sizeof(line), in)) {
            unsigned word;
            unsigned long long value;
            if (sscanf(line, "word %x %llu", &word, &value) == 2) {
                uint32_t leaf = find_leaf(word);
                if (leaf == LEAF_NONE) {
                    unmatched += value;
                    continue;
                }
                by_leaf[leaf] += value;
                if (subgroup_is(&a64_leaves[leaf], "control/system") ||
                    strcmp(a64_leaves[leaf].subgroup, "control/pstate") == 0) {
                    uint32_t masked = word & ~31U, i = 0;
                    while (i < system_count && system_words[i].word != masked) {
                        ++i;
                    }
                    if (i < 4096) {
                        system_words[i].word = masked;
                        system_words[i].count += value;
                        system_count += i == system_count;
                    }
                }
            } else if (sscanf(line, "%31s %llu", key, &value) == 2) {
                for (uint32_t c = 0; c < COUNTERS; ++c) {
                    if (strcmp(key, counters[c]) == 0) {
                        totals[c] += value;
                    }
                }
            }
        }
        fclose(in);
    }
    uint64_t cold = totals[1], executed = totals[0] + cold;
    printf("%llu instructions dispatched, %llu on the slow path (%.3f%%)\n", executed, cold,
           executed ? 100.0 * (double)cold / (double)executed : 0.0);
    for (uint32_t c = 2; c < COUNTERS; ++c) {
        printf("%-16s %12llu  %.4f per 1000 instructions\n", counters[c], totals[c],
               executed ? 1000.0 * (double)totals[c] / (double)executed : 0.0);
    }
    const uint64_t *order[LEAF_COUNT];
    uint32_t used = 0;
    for (uint32_t l = 0; l < LEAF_COUNT; ++l) {
        if (by_leaf[l]) {
            order[used++] = &by_leaf[l];
        }
    }
    for (uint32_t i = 0; i < used; ++i) {
        for (uint32_t j = i + 1; j < used; ++j) {
            if (*order[j] > *order[i]) {
                const uint64_t *t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }
    for (uint32_t i = 0; i < used && i < top; ++i) {
        const a64_leaf_t *leaf = &a64_leaves[order[i] - by_leaf];
        printf("%12llu  %6.2f%%  %-22s %s\n", *order[i], cold ? 100.0 * (double)*order[i] / (double)cold : 0.0,
               leaf->subgroup, leaf->name);
    }
    if (unmatched) {
        printf("%12llu  unallocated\n", unmatched);
    }
    qsort(system_words, system_count, sizeof(system_words[0]), compare_system_words);
    for (uint32_t i = 0; i < system_count && i < top; ++i) {
        uint32_t w = system_words[i].word;
        printf("%12llu  system %08x op0=%u op1=%u CRn=%u CRm=%u op2=%u L=%u\n", system_words[i].count, w,
               (w >> 19) & 3, (w >> 16) & 7, (w >> 12) & 15, (w >> 8) & 15, (w >> 5) & 7, (w >> 21) & 1);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "-j") == 0) {
        char *jit_error = nullptr;
        if (!swipterm_jit_prepare(&jit_error)) {
            fprintf(stderr, "swipterm_jit_prepare: %s\n", jit_error ? jit_error : "failed");
            free(jit_error);
            fprintf(stderr, "executable memory is unavailable\n");
            return 1;
        }
        jit_mode = true;
        --argc;
        ++argv;
    }
    if (argc < 2) {
        fprintf(stderr,
                "usage: a64_check [-j] COMMAND ...\n"
                "       a64_check fuzz [-n COUNT] [-s SEED] [-v] [-p] [FILTER]\n"
                "       a64_check stubs [COUNT]\n"
                "       a64_check system [COUNT]\n"
                "       a64_check spec [COUNT [SUBGROUP]]\n"
                "       a64_check elf [-t TOP] [-p] FILE...\n"
                "       a64_check stats [-t TOP] FILE...\n"
                "       a64_check coverage SLOW_PATH_LIST\n");
        return 2;
    }
    host_features = query_host_features();
    setup_machine();
    if (strcmp(argv[1], "fuzz") == 0) {
        return command_fuzz(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "system") == 0) {
        return command_system(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "stubs") == 0) {
        return command_stubs(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "coverage") == 0) {
        return command_coverage(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "stats") == 0) {
        return command_stats(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "spec") == 0) {
        return command_spec(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "elf") == 0) {
        return command_elf(argc - 2, argv + 2);
    }
    return 2;
}
