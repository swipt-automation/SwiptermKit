#ifndef A64_CHECK_STATE_H
#define A64_CHECK_STATE_H

#define STATE_X 0
#define STATE_SP 248
#define STATE_NZCV 256
#define STATE_FPCR 264
#define STATE_FPSR 272
#define STATE_V 288
#define NATIVE_SLOTS 4

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

typedef struct test_state {
    uint64_t x[31];
    uint64_t sp;
    uint64_t nzcv;
    uint64_t fpcr;
    uint64_t fpsr;
    uint64_t system;
    __uint128_t v[32];
    uint64_t daif;
    uint64_t irq_pending;
    uint64_t sp_el0;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t esr_el1;
    uint64_t far_el1;
    uint64_t tpidr_el0;
    uint64_t tpidr_el1;
    uint64_t tpidrro_el0;
} test_state_t;

static_assert(offsetof(test_state_t, sp) == STATE_SP);
static_assert(offsetof(test_state_t, nzcv) == STATE_NZCV);
static_assert(offsetof(test_state_t, fpcr) == STATE_FPCR);
static_assert(offsetof(test_state_t, fpsr) == STATE_FPSR);
static_assert(offsetof(test_state_t, v) == STATE_V);

extern const uint8_t native_template[];
extern const uint8_t native_template_slots[];
extern const uint8_t native_template_end[];
#endif

#endif
