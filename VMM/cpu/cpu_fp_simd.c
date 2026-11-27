#include "cpu_internal.h"

#include <arm_acle.h>
#include <arm_neon.h>
#include <math.h>

void cpu_op_crc32(vcpu_t *cpu, const ir_insn_t *ir,
                  softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    uint32_t acc = (uint32_t)cpu_get_x(cpu, ir->rn);
    uint64_t val = cpu_get_x(cpu, ir->rm);
    uint32_t res = 0;
    switch (ir->op) {
    case SUBOP_CRC32B:  res = __crc32b(acc, (uint8_t)val); break;
    case SUBOP_CRC32H:  res = __crc32h(acc, (uint16_t)val); break;
    case SUBOP_CRC32W:  res = __crc32w(acc, (uint32_t)val); break;
    case SUBOP_CRC32X:  res = __crc32d(acc, val); break;
    case SUBOP_CRC32CB: res = __crc32cb(acc, (uint8_t)val); break;
    case SUBOP_CRC32CH: res = __crc32ch(acc, (uint16_t)val); break;
    case SUBOP_CRC32CW: res = __crc32cw(acc, (uint32_t)val); break;
    case SUBOP_CRC32CX: res = __crc32cd(acc, val); break;
    }
    cpu_set_x(cpu, ir->rd, (uint64_t)res);
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static inline uint16_t vfp_expand_imm16(uint32_t imm8) {
    uint32_t a = (imm8 >> 7) & 1U;
    uint32_t b = (imm8 >> 6) & 1U;
    uint32_t cd = (imm8 >> 4) & 3U;
    uint32_t efgh = imm8 & 15U;
    return (uint16_t)((a << 15) | ((b ^ 1U) << 14) | ((b ? 3U : 0U) << 12) | (cd << 10) | (efgh << 6));
}

static inline uint32_t vfp_expand_imm32(uint32_t imm8) {
    uint32_t a = (imm8 >> 7) & 1U;
    uint32_t b = (imm8 >> 6) & 1U;
    uint32_t cd = (imm8 >> 4) & 3U;
    uint32_t efgh = imm8 & 15U;
    return (a << 31) | ((b ^ 1U) << 30) | ((b ? 0x1FU : 0U) << 25) | (cd << 23) | (efgh << 19);
}

static inline uint64_t vfp_expand_imm64(uint32_t imm8) {
    uint64_t a = (imm8 >> 7) & 1U;
    uint64_t b = (imm8 >> 6) & 1U;
    uint64_t cd = (imm8 >> 4) & 3U;
    uint64_t efgh = imm8 & 15U;
    return (a << 63) | ((b ^ 1U) << 62) | ((b ? 0xFFULL : 0ULL) << 54) | (cd << 52) | (efgh << 48);
}

static inline float v_get_f32(const vcpu_t *cpu, uint32_t r) {
    float f;
    memcpy(&f, &cpu->v[r], 4);
    return f;
}

static inline double v_get_f64(const vcpu_t *cpu, uint32_t r) {
    double d;
    memcpy(&d, &cpu->v[r], 8);
    return d;
}

static inline uint16_t v_get_u16(const vcpu_t *cpu, uint32_t r) {
    uint16_t h;
    memcpy(&h, &cpu->v[r], 2);
    return h;
}

static inline void v_set_f32(vcpu_t *cpu, uint32_t r, float f) {
    cpu->v[r] = 0;
    memcpy(&cpu->v[r], &f, 4);
}

static inline void v_set_f64(vcpu_t *cpu, uint32_t r, double d) {
    cpu->v[r] = 0;
    memcpy(&cpu->v[r], &d, 8);
}

static inline void v_set_u16(vcpu_t *cpu, uint32_t r, uint16_t h) {
    cpu->v[r] = 0;
    memcpy(&cpu->v[r], &h, 2);
}

__attribute__((target("fullfp16")))
static uint16_t hw_f16_unary(uint32_t op, uint16_t a) {
    float16_t fa, fr;
    memcpy(&fa, &a, 2);
    switch (op) {
    case 0U: fr = fa; break;
    case 1U: __asm__("fabs %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 2U: __asm__("fneg %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 3U: __asm__("fsqrt %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 8U: __asm__("frintn %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 9U: __asm__("frintp %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 10U: __asm__("frintm %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 11U: __asm__("frintz %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 12U: __asm__("frinta %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 14U: __asm__("frintx %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 15U: __asm__("frinti %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 16U: __asm__("frecpe %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 17U: __asm__("frsqrte %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 18U: __asm__("frecpx %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    default: fr = fa; break;
    }
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

__attribute__((target("fullfp16")))
static uint16_t hw_f16_binary(uint32_t op, uint16_t a, uint16_t b) {
    float16_t fa, fb, fr;
    memcpy(&fa, &a, 2);
    memcpy(&fb, &b, 2);
    switch (op) {
    case 0U: __asm__("fmul %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 1U: __asm__("fdiv %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 2U: __asm__("fadd %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 3U: __asm__("fsub %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 4U: __asm__("fmax %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 5U: __asm__("fmin %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 6U: __asm__("fmaxnm %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 7U: __asm__("fminnm %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 8U: __asm__("fnmul %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 9U: __asm__("fabd %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 10U: __asm__("fmulx %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 11U: __asm__("frecps %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 12U: __asm__("frsqrts %h0, %h1, %h2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    default: fr = fa; break;
    }
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

__attribute__((target("fullfp16")))
static uint16_t hw_f16_ternary(uint32_t op, uint16_t a, uint16_t b, uint16_t c) {
    float16_t fa, fb, fc, fr;
    memcpy(&fa, &a, 2);
    memcpy(&fb, &b, 2);
    memcpy(&fc, &c, 2);
    switch (op) {
    case 0U: __asm__("fmadd %h0, %h1, %h2, %h3" : "=w"(fr) : "w"(fa), "w"(fb), "w"(fc)); break;
    case 1U: __asm__("fmsub %h0, %h1, %h2, %h3" : "=w"(fr) : "w"(fa), "w"(fb), "w"(fc)); break;
    case 2U: __asm__("fnmadd %h0, %h1, %h2, %h3" : "=w"(fr) : "w"(fa), "w"(fb), "w"(fc)); break;
    default: __asm__("fnmsub %h0, %h1, %h2, %h3" : "=w"(fr) : "w"(fa), "w"(fb), "w"(fc)); break;
    }
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

__attribute__((target("fullfp16")))
static uint32_t hw_f16_cmp(uint16_t a, uint16_t b, bool signal, bool zero) {
    float16_t fa, fb;
    uint64_t nzcv;
    memcpy(&fa, &a, 2);
    memcpy(&fb, &b, 2);
    if (zero) {
        if (signal) __asm__("fcmpe %h1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa) : "cc");
        else __asm__("fcmp %h1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa) : "cc");
    } else {
        if (signal) __asm__("fcmpe %h1, %h2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
        else __asm__("fcmp %h1, %h2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
    }
    return (uint32_t)nzcv;
}

__attribute__((target("fullfp16")))
static uint64_t hw_f16_to_int(uint32_t mode, uint16_t a, bool is_64) {
    float16_t fa;
    memcpy(&fa, &a, 2);
    uint64_t r64 = 0;
    uint32_t r32 = 0;
    if (is_64) {
        switch (mode) {
        case 0U: __asm__("fcvtns %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 1U: __asm__("fcvtnu %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 2U: __asm__("fcvtps %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 3U: __asm__("fcvtpu %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 4U: __asm__("fcvtms %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 5U: __asm__("fcvtmu %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 6U: __asm__("fcvtzs %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 7U: __asm__("fcvtzu %0, %h1" : "=r"(r64) : "w"(fa)); break;
        case 8U: __asm__("fcvtas %0, %h1" : "=r"(r64) : "w"(fa)); break;
        default: __asm__("fcvtau %0, %h1" : "=r"(r64) : "w"(fa)); break;
        }
        return r64;
    }
    switch (mode) {
    case 0U: __asm__("fcvtns %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 1U: __asm__("fcvtnu %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 2U: __asm__("fcvtps %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 3U: __asm__("fcvtpu %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 4U: __asm__("fcvtms %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 5U: __asm__("fcvtmu %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 6U: __asm__("fcvtzs %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 7U: __asm__("fcvtzu %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    case 8U: __asm__("fcvtas %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    default: __asm__("fcvtau %w0, %h1" : "=r"(r32) : "w"(fa)); break;
    }
    return (uint64_t)r32;
}

__attribute__((target("fullfp16")))
static uint16_t hw_int_to_f16(uint64_t v, bool is_unsigned, bool is_64) {
    float16_t fr;
    if (is_64) {
        if (is_unsigned) __asm__("ucvtf %h0, %1" : "=w"(fr) : "r"(v));
        else __asm__("scvtf %h0, %1" : "=w"(fr) : "r"(v));
    } else {
        uint32_t v32 = (uint32_t)v;
        if (is_unsigned) __asm__("ucvtf %h0, %w1" : "=w"(fr) : "r"(v32));
        else __asm__("scvtf %h0, %w1" : "=w"(fr) : "r"(v32));
    }
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

static float hw_f32_unary(uint32_t op, float fa) {
    float fr;
    switch (op) {
    case 0U: fr = fa; break;
    case 1U: __asm__("fabs %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 2U: __asm__("fneg %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 3U: __asm__("fsqrt %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 8U: __asm__("frintn %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 9U: __asm__("frintp %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 10U: __asm__("frintm %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 11U: __asm__("frintz %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 12U: __asm__("frinta %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 14U: __asm__("frintx %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 15U: __asm__("frinti %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 16U: __asm__("frecpe %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 17U: __asm__("frsqrte %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    case 18U: __asm__("frecpx %s0, %s1" : "=w"(fr) : "w"(fa)); break;
    default: fr = fa; break;
    }
    return fr;
}

static double hw_f64_unary(uint32_t op, double fa) {
    double fr;
    switch (op) {
    case 0U: fr = fa; break;
    case 1U: __asm__("fabs %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 2U: __asm__("fneg %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 3U: __asm__("fsqrt %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 8U: __asm__("frintn %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 9U: __asm__("frintp %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 10U: __asm__("frintm %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 11U: __asm__("frintz %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 12U: __asm__("frinta %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 14U: __asm__("frintx %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 15U: __asm__("frinti %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 16U: __asm__("frecpe %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 17U: __asm__("frsqrte %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    case 18U: __asm__("frecpx %d0, %d1" : "=w"(fr) : "w"(fa)); break;
    default: fr = fa; break;
    }
    return fr;
}

static float hw_f32_binary(uint32_t op, float fa, float fb) {
    float fr;
    switch (op) {
    case 0U: __asm__("fmul %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 1U: __asm__("fdiv %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 2U: __asm__("fadd %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 3U: __asm__("fsub %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 4U: __asm__("fmax %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 5U: __asm__("fmin %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 6U: __asm__("fmaxnm %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 7U: __asm__("fminnm %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 8U: __asm__("fnmul %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 9U: __asm__("fabd %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 10U: __asm__("fmulx %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 11U: __asm__("frecps %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 12U: __asm__("frsqrts %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    default: fr = fa; break;
    }
    return fr;
}

static double hw_f64_binary(uint32_t op, double fa, double fb) {
    double fr;
    switch (op) {
    case 0U: __asm__("fmul %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 1U: __asm__("fdiv %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 2U: __asm__("fadd %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 3U: __asm__("fsub %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 4U: __asm__("fmax %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 5U: __asm__("fmin %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 6U: __asm__("fmaxnm %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 7U: __asm__("fminnm %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 8U: __asm__("fnmul %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 9U: __asm__("fabd %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 10U: __asm__("fmulx %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 11U: __asm__("frecps %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    case 12U: __asm__("frsqrts %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); break;
    default: fr = fa; break;
    }
    return fr;
}

static uint64_t hw_f32_to_int(uint32_t mode, float fa, bool is_64) {
    uint64_t r64 = 0;
    uint32_t r32 = 0;
    if (is_64) {
        switch (mode) {
        case 0U: __asm__("fcvtns %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 1U: __asm__("fcvtnu %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 2U: __asm__("fcvtps %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 3U: __asm__("fcvtpu %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 4U: __asm__("fcvtms %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 5U: __asm__("fcvtmu %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 6U: __asm__("fcvtzs %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 7U: __asm__("fcvtzu %0, %s1" : "=r"(r64) : "w"(fa)); break;
        case 8U: __asm__("fcvtas %0, %s1" : "=r"(r64) : "w"(fa)); break;
        default: __asm__("fcvtau %0, %s1" : "=r"(r64) : "w"(fa)); break;
        }
        return r64;
    }
    switch (mode) {
    case 0U: __asm__("fcvtns %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 1U: __asm__("fcvtnu %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 2U: __asm__("fcvtps %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 3U: __asm__("fcvtpu %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 4U: __asm__("fcvtms %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 5U: __asm__("fcvtmu %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 6U: __asm__("fcvtzs %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 7U: __asm__("fcvtzu %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    case 8U: __asm__("fcvtas %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    default: __asm__("fcvtau %w0, %s1" : "=r"(r32) : "w"(fa)); break;
    }
    return (uint64_t)r32;
}

static uint64_t hw_f64_to_int(uint32_t mode, double fa, bool is_64) {
    uint64_t r64 = 0;
    uint32_t r32 = 0;
    if (is_64) {
        switch (mode) {
        case 0U: __asm__("fcvtns %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 1U: __asm__("fcvtnu %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 2U: __asm__("fcvtps %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 3U: __asm__("fcvtpu %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 4U: __asm__("fcvtms %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 5U: __asm__("fcvtmu %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 6U: __asm__("fcvtzs %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 7U: __asm__("fcvtzu %0, %d1" : "=r"(r64) : "w"(fa)); break;
        case 8U: __asm__("fcvtas %0, %d1" : "=r"(r64) : "w"(fa)); break;
        default: __asm__("fcvtau %0, %d1" : "=r"(r64) : "w"(fa)); break;
        }
        return r64;
    }
    switch (mode) {
    case 0U: __asm__("fcvtns %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 1U: __asm__("fcvtnu %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 2U: __asm__("fcvtps %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 3U: __asm__("fcvtpu %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 4U: __asm__("fcvtms %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 5U: __asm__("fcvtmu %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 6U: __asm__("fcvtzs %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 7U: __asm__("fcvtzu %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    case 8U: __asm__("fcvtas %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    default: __asm__("fcvtau %w0, %d1" : "=r"(r32) : "w"(fa)); break;
    }
    return (uint64_t)r32;
}

__attribute__((target("fullfp16")))
static uint16_t hw_f16_to_int16(uint32_t mode, uint16_t a) {
    float16_t fa, fr;
    memcpy(&fa, &a, 2);
    switch (mode) {
    case 0U: __asm__("fcvtns %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 1U: __asm__("fcvtnu %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 2U: __asm__("fcvtps %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 3U: __asm__("fcvtpu %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 4U: __asm__("fcvtms %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 5U: __asm__("fcvtmu %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 6U: __asm__("fcvtzs %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 7U: __asm__("fcvtzu %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    case 8U: __asm__("fcvtas %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    default: __asm__("fcvtau %h0, %h1" : "=w"(fr) : "w"(fa)); break;
    }
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

__attribute__((target("fullfp16")))
static uint16_t hw_int16_to_f16(uint16_t a, bool is_unsigned) {
    float16_t fa, fr;
    memcpy(&fa, &a, 2);
    if (is_unsigned) __asm__("ucvtf %h0, %h1" : "=w"(fr) : "w"(fa));
    else __asm__("scvtf %h0, %h1" : "=w"(fr) : "w"(fa));
    uint16_t out;
    memcpy(&out, &fr, 2);
    return out;
}

__attribute__((target("jscvt")))
static uint32_t exec_fjcvtzs(vcpu_t *cpu, double d) {
    uint32_t res;
    uint64_t nzcv;
    __asm__("fjcvtzs %w0, %d2\n\tmrs %1, nzcv" : "=&r"(res), "=r"(nzcv) : "w"(d) : "cc");
    cpu->nzcv = (uint32_t)nzcv;
    return res;
}

#define FP_SWITCH_16(X) \
    X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) \
    X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16)
#define FP_SWITCH_32(X) \
    FP_SWITCH_16(X) \
    X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) \
    X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32)
#define FP_SWITCH_64(X) \
    FP_SWITCH_32(X) \
    X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) \
    X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48) \
    X(49) X(50) X(51) X(52) X(53) X(54) X(55) X(56) \
    X(57) X(58) X(59) X(60) X(61) X(62) X(63) X(64)

__attribute__((target("fullfp16")))
static uint64_t hw_fp_to_fix_gpr(uint32_t ftype, uint64_t bits, uint32_t fbits, bool is_unsigned, bool is_64) {
    uint64_t r64 = 0;
    uint32_t r32 = 0;
    if (ftype == 3U) {
        float16_t fa;
        memcpy(&fa, &bits, 2);
        if (is_64) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzu %0, %h1, #" #N : "=r"(r64) : "w"(fa)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzs %0, %h1, #" #N : "=r"(r64) : "w"(fa)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            }
            return r64;
        }
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzu %w0, %h1, #" #N : "=r"(r32) : "w"(fa)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzs %w0, %h1, #" #N : "=r"(r32) : "w"(fa)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        }
        return (uint64_t)r32;
    }
    if (ftype == 0U) {
        float fa;
        memcpy(&fa, &bits, 4);
        if (is_64) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzu %0, %s1, #" #N : "=r"(r64) : "w"(fa)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzs %0, %s1, #" #N : "=r"(r64) : "w"(fa)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            }
            return r64;
        }
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzu %w0, %s1, #" #N : "=r"(r32) : "w"(fa)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzs %w0, %s1, #" #N : "=r"(r32) : "w"(fa)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        }
        return (uint64_t)r32;
    }
    double fa;
    memcpy(&fa, &bits, 8);
    if (is_64) {
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzu %0, %d1, #" #N : "=r"(r64) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzs %0, %d1, #" #N : "=r"(r64) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        }
        return r64;
    }
    if (is_unsigned) {
        switch (fbits) {
        #define X(N) case N: __asm__("fcvtzu %w0, %d1, #" #N : "=r"(r32) : "w"(fa)); break;
        FP_SWITCH_32(X)
        #undef X
        default: break;
        }
    } else {
        switch (fbits) {
        #define X(N) case N: __asm__("fcvtzs %w0, %d1, #" #N : "=r"(r32) : "w"(fa)); break;
        FP_SWITCH_32(X)
        #undef X
        default: break;
        }
    }
    return (uint64_t)r32;
}

__attribute__((target("fullfp16")))
static uint64_t hw_fix_gpr_to_fp(uint32_t ftype, uint64_t val, uint32_t fbits, bool is_unsigned, bool is_64) {
    uint32_t v32 = (uint32_t)val;
    if (ftype == 3U) {
        float16_t fr = 0;
        if (is_64) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %h0, %1, #" #N : "=w"(fr) : "r"(val)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %h0, %1, #" #N : "=w"(fr) : "r"(val)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            }
        } else {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %h0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %h0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            }
        }
        uint16_t out = 0;
        memcpy(&out, &fr, 2);
        return out;
    }
    if (ftype == 0U) {
        float fr = 0;
        if (is_64) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %s0, %1, #" #N : "=w"(fr) : "r"(val)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %s0, %1, #" #N : "=w"(fr) : "r"(val)); break;
                FP_SWITCH_64(X)
                #undef X
                default: break;
                }
            }
        } else {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %s0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %s0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            }
        }
        uint32_t out = 0;
        memcpy(&out, &fr, 4);
        return out;
    }
    double fr = 0;
    if (is_64) {
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("ucvtf %d0, %1, #" #N : "=w"(fr) : "r"(val)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("scvtf %d0, %1, #" #N : "=w"(fr) : "r"(val)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        }
    } else {
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("ucvtf %d0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("scvtf %d0, %w1, #" #N : "=w"(fr) : "r"(v32)); break;
            FP_SWITCH_32(X)
            #undef X
            default: break;
            }
        }
    }
    uint64_t out = 0;
    memcpy(&out, &fr, 8);
    return out;
}

__attribute__((target("fullfp16")))
static uint64_t hw_simd_fix_cvt(uint32_t sz, uint64_t bits, uint32_t fbits, bool to_fp, bool is_unsigned) {
    if (sz == 1U) {
        float16_t fa, fr = 0;
        memcpy(&fa, &bits, 2);
        if (to_fp) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %h0, %h1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_16(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %h0, %h1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_16(X)
                #undef X
                default: break;
                }
            }
        } else {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzu %h0, %h1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_16(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzs %h0, %h1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_16(X)
                #undef X
                default: break;
                }
            }
        }
        uint16_t out = 0;
        memcpy(&out, &fr, 2);
        return out;
    }
    if (sz == 2U) {
        float fa, fr = 0;
        memcpy(&fa, &bits, 4);
        if (to_fp) {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("ucvtf %s0, %s1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("scvtf %s0, %s1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            }
        } else {
            if (is_unsigned) {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzu %s0, %s1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            } else {
                switch (fbits) {
                #define X(N) case N: __asm__("fcvtzs %s0, %s1, #" #N : "=w"(fr) : "w"(fa)); break;
                FP_SWITCH_32(X)
                #undef X
                default: break;
                }
            }
        }
        uint32_t out = 0;
        memcpy(&out, &fr, 4);
        return out;
    }
    double fa, fr = 0;
    memcpy(&fa, &bits, 8);
    if (to_fp) {
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("ucvtf %d0, %d1, #" #N : "=w"(fr) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("scvtf %d0, %d1, #" #N : "=w"(fr) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        }
    } else {
        if (is_unsigned) {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzu %d0, %d1, #" #N : "=w"(fr) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        } else {
            switch (fbits) {
            #define X(N) case N: __asm__("fcvtzs %d0, %d1, #" #N : "=w"(fr) : "w"(fa)); break;
            FP_SWITCH_64(X)
            #undef X
            default: break;
            }
        }
    }
    uint64_t out = 0;
    memcpy(&out, &fr, 8);
    return out;
}

__attribute__((target("fullfp16")))
static uint64_t hw_fp_across(uint32_t fop, uint32_t fsz, bool q, const uint8_t *vn) {
    if (fsz == 1U) {
        float16_t r = 0;
        if (q) {
            float16x8_t v = vld1q_f16((const float16_t *)vn);
            if (fop == 6U) __asm__("fmaxnmv %h0, %1.8h" : "=w"(r) : "w"(v));
            else if (fop == 7U) __asm__("fminnmv %h0, %1.8h" : "=w"(r) : "w"(v));
            else if (fop == 4U) __asm__("fmaxv %h0, %1.8h" : "=w"(r) : "w"(v));
            else __asm__("fminv %h0, %1.8h" : "=w"(r) : "w"(v));
        } else {
            float16x4_t v = vld1_f16((const float16_t *)vn);
            if (fop == 6U) __asm__("fmaxnmv %h0, %1.4h" : "=w"(r) : "w"(v));
            else if (fop == 7U) __asm__("fminnmv %h0, %1.4h" : "=w"(r) : "w"(v));
            else if (fop == 4U) __asm__("fmaxv %h0, %1.4h" : "=w"(r) : "w"(v));
            else __asm__("fminv %h0, %1.4h" : "=w"(r) : "w"(v));
        }
        uint16_t out = 0;
        memcpy(&out, &r, 2);
        return out;
    }
    float32x4_t v = vld1q_f32((const float *)vn);
    float r = 0;
    if (fop == 6U) __asm__("fmaxnmv %s0, %1.4s" : "=w"(r) : "w"(v));
    else if (fop == 7U) __asm__("fminnmv %s0, %1.4s" : "=w"(r) : "w"(v));
    else if (fop == 4U) __asm__("fmaxv %s0, %1.4s" : "=w"(r) : "w"(v));
    else __asm__("fminv %s0, %1.4s" : "=w"(r) : "w"(v));
    uint32_t out = 0;
    memcpy(&out, &r, 4);
    return out;
}

static uint16_t hw_f32_to_f16(float f) {
    float16_t h;
    __asm__("fcvt %h0, %s1" : "=w"(h) : "w"(f));
    uint16_t out;
    memcpy(&out, &h, 2);
    return out;
}

static float hw_f16_to_f32(uint16_t a) {
    float16_t h;
    float f;
    memcpy(&h, &a, 2);
    __asm__("fcvt %s0, %h1" : "=w"(f) : "w"(h));
    return f;
}

void cpu_op_fp_scalar(vcpu_t *cpu, const ir_insn_t *ir,
                      softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    cpu_fp_ensure_guest(cpu);
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t ftype = (word >> 22) & 3U;
    uint32_t rd = word & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t ra = (word >> 10) & 31U;

    switch (ir->op) {
    case SUBOP_FP_DP1: {
        uint32_t opc = (word >> 15) & 0x3FU;
        if (opc == 4U || opc == 5U || opc == 7U) {
            if (ftype == 0U && opc == 5U) {
                double d;
                float s = v_get_f32(cpu, rn);
                __asm__("fcvt %d0, %s1" : "=w"(d) : "w"(s));
                v_set_f64(cpu, rd, d);
            } else if (ftype == 0U && opc == 7U) {
                v_set_u16(cpu, rd, hw_f32_to_f16(v_get_f32(cpu, rn)));
            } else if (ftype == 1U && opc == 4U) {
                float s;
                double d = v_get_f64(cpu, rn);
                __asm__("fcvt %s0, %d1" : "=w"(s) : "w"(d));
                v_set_f32(cpu, rd, s);
            } else if (ftype == 1U && opc == 7U) {
                float16_t h;
                double d = v_get_f64(cpu, rn);
                __asm__("fcvt %h0, %d1" : "=w"(h) : "w"(d));
                cpu->v[rd] = 0;
                memcpy(&cpu->v[rd], &h, 2);
            } else if (ftype == 3U && opc == 4U) {
                v_set_f32(cpu, rd, hw_f16_to_f32(v_get_u16(cpu, rn)));
            } else if (ftype == 3U && opc == 5U) {
                float16_t h;
                double d;
                memcpy(&h, &cpu->v[rn], 2);
                __asm__("fcvt %d0, %h1" : "=w"(d) : "w"(h));
                v_set_f64(cpu, rd, d);
            } else {
                vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
                return;
            }
        } else if (ftype == 0U) {
            v_set_f32(cpu, rd, hw_f32_unary(opc, v_get_f32(cpu, rn)));
        } else if (ftype == 1U) {
            v_set_f64(cpu, rd, hw_f64_unary(opc, v_get_f64(cpu, rn)));
        } else if (ftype == 3U) {
            v_set_u16(cpu, rd, hw_f16_unary(opc, v_get_u16(cpu, rn)));
        } else {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        break;
    }
    case SUBOP_FP_DP2: {
        uint32_t opc = (word >> 12) & 15U;
        if (ftype == 0U) {
            v_set_f32(cpu, rd, hw_f32_binary(opc, v_get_f32(cpu, rn), v_get_f32(cpu, rm)));
        } else if (ftype == 1U) {
            v_set_f64(cpu, rd, hw_f64_binary(opc, v_get_f64(cpu, rn), v_get_f64(cpu, rm)));
        } else if (ftype == 3U) {
            v_set_u16(cpu, rd, hw_f16_binary(opc, v_get_u16(cpu, rn), v_get_u16(cpu, rm)));
        } else {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        break;
    }
    case SUBOP_FP_DP3: {
        uint32_t op = (((word >> 21) & 1U) << 1) | ((word >> 15) & 1U);
        if (ftype == 0U) {
            float a = v_get_f32(cpu, rn), b = v_get_f32(cpu, rm), c = v_get_f32(cpu, ra), r;
            if (op == 0U) __asm__("fmadd %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else if (op == 1U) __asm__("fmsub %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else if (op == 2U) __asm__("fnmadd %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else __asm__("fnmsub %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            v_set_f32(cpu, rd, r);
        } else if (ftype == 1U) {
            double a = v_get_f64(cpu, rn), b = v_get_f64(cpu, rm), c = v_get_f64(cpu, ra), r;
            if (op == 0U) __asm__("fmadd %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else if (op == 1U) __asm__("fmsub %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else if (op == 2U) __asm__("fnmadd %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            else __asm__("fnmsub %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
            v_set_f64(cpu, rd, r);
        } else if (ftype == 3U) {
            v_set_u16(cpu, rd, hw_f16_ternary(op, v_get_u16(cpu, rn), v_get_u16(cpu, rm), v_get_u16(cpu, ra)));
        } else {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        break;
    }
    case SUBOP_FP_CMP: {
        bool zero = ((word >> 3) & 1U) != 0;
        bool sig = ((word >> 4) & 1U) != 0;
        uint64_t nzcv = 0;
        if (ftype == 0U) {
            float a = v_get_f32(cpu, rn), b = v_get_f32(cpu, rm);
            if (zero) {
                if (sig) __asm__("fcmpe %s1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a) : "cc");
                else __asm__("fcmp %s1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a) : "cc");
            } else {
                if (sig) __asm__("fcmpe %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
                else __asm__("fcmp %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            }
        } else if (ftype == 1U) {
            double a = v_get_f64(cpu, rn), b = v_get_f64(cpu, rm);
            if (zero) {
                if (sig) __asm__("fcmpe %d1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a) : "cc");
                else __asm__("fcmp %d1, #0.0\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a) : "cc");
            } else {
                if (sig) __asm__("fcmpe %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
                else __asm__("fcmp %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            }
        } else if (ftype == 3U) {
            nzcv = hw_f16_cmp(v_get_u16(cpu, rn), v_get_u16(cpu, rm), sig, zero);
        }
        cpu->nzcv = (uint32_t)nzcv;
        break;
    }
    case SUBOP_FP_CCMP: {
        uint32_t cond = (word >> 12) & 15U;
        uint32_t nzcv_imm = word & 15U;
        bool sig = ((word >> 4) & 1U) != 0;
        if (!cpu_cond_holds(cpu->nzcv, cond)) {
            cpu->nzcv = nzcv_imm << 28;
        } else if (ftype == 0U) {
            float a = v_get_f32(cpu, rn), b = v_get_f32(cpu, rm);
            uint64_t nzcv;
            if (sig) __asm__("fcmpe %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            else __asm__("fcmp %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            cpu->nzcv = (uint32_t)nzcv;
        } else if (ftype == 1U) {
            double a = v_get_f64(cpu, rn), b = v_get_f64(cpu, rm);
            uint64_t nzcv;
            if (sig) __asm__("fcmpe %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            else __asm__("fcmp %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(a), "w"(b) : "cc");
            cpu->nzcv = (uint32_t)nzcv;
        } else {
            cpu->nzcv = hw_f16_cmp(v_get_u16(cpu, rn), v_get_u16(cpu, rm), sig, false);
        }
        break;
    }
    case SUBOP_FP_SEL: {
        uint32_t cond = (word >> 12) & 15U;
        uint32_t src = cpu_cond_holds(cpu->nzcv, cond) ? rn : rm;
        if (ftype == 0U) v_set_f32(cpu, rd, v_get_f32(cpu, src));
        else if (ftype == 1U) v_set_f64(cpu, rd, v_get_f64(cpu, src));
        else v_set_u16(cpu, rd, v_get_u16(cpu, src));
        break;
    }
    case SUBOP_FP_IMM: {
        uint32_t imm8 = (word >> 13) & 0xFFU;
        cpu->v[rd] = 0;
        if (ftype == 0U) {
            uint32_t v = vfp_expand_imm32(imm8);
            memcpy(&cpu->v[rd], &v, 4);
        } else if (ftype == 1U) {
            uint64_t v = vfp_expand_imm64(imm8);
            memcpy(&cpu->v[rd], &v, 8);
        } else {
            uint16_t v = vfp_expand_imm16(imm8);
            memcpy(&cpu->v[rd], &v, 2);
        }
        break;
    }
    case SUBOP_FP_2INT: {
        bool sf = ((word >> 31) & 1U) != 0;
        uint32_t rmode = (word >> 19) & 3U;
        uint32_t opcode = (word >> 16) & 7U;
        if (opcode == 6U && rmode == 3U && ftype == 1U && !sf) {
            cpu_set_x(cpu, rd, exec_fjcvtzs(cpu, v_get_f64(cpu, rn)));
        } else if (opcode == 6U) {
            if (ftype == 0U && rmode == 0U) {
                uint32_t v = 0;
                memcpy(&v, &cpu->v[rn], 4);
                cpu_set_x(cpu, rd, v);
            } else if (ftype == 1U && rmode == 0U) {
                uint64_t v = 0;
                memcpy(&v, &cpu->v[rn], 8);
                cpu_set_x(cpu, rd, v);
            } else if (ftype == 2U && rmode == 1U) {
                uint64_t v = 0;
                memcpy(&v, (const uint8_t *)&cpu->v[rn] + 8, 8);
                cpu_set_x(cpu, rd, v);
            } else if (ftype == 3U && rmode == 0U) {
                cpu_set_x(cpu, rd, v_get_u16(cpu, rn));
            }
        } else if (opcode == 7U) {
            uint64_t v = cpu_get_x(cpu, rn);
            if (ftype == 0U && rmode == 0U) {
                uint32_t v32 = (uint32_t)v;
                cpu->v[rd] = 0;
                memcpy(&cpu->v[rd], &v32, 4);
            } else if (ftype == 1U && rmode == 0U) {
                cpu->v[rd] = 0;
                memcpy(&cpu->v[rd], &v, 8);
            } else if (ftype == 2U && rmode == 1U) {
                memcpy((uint8_t *)&cpu->v[rd] + 8, &v, 8);
            } else if (ftype == 3U && rmode == 0U) {
                v_set_u16(cpu, rd, (uint16_t)v);
            }
        } else if (opcode == 2U || opcode == 3U) {
            bool uns = (opcode == 3U);
            uint64_t v = cpu_get_x(cpu, rn);
            if (ftype == 0U) {
                float r;
                if (sf) {
                    if (uns) __asm__("ucvtf %s0, %1" : "=w"(r) : "r"(v));
                    else __asm__("scvtf %s0, %1" : "=w"(r) : "r"(v));
                } else {
                    uint32_t v32 = (uint32_t)v;
                    if (uns) __asm__("ucvtf %s0, %w1" : "=w"(r) : "r"(v32));
                    else __asm__("scvtf %s0, %w1" : "=w"(r) : "r"(v32));
                }
                v_set_f32(cpu, rd, r);
            } else if (ftype == 1U) {
                double r;
                if (sf) {
                    if (uns) __asm__("ucvtf %d0, %1" : "=w"(r) : "r"(v));
                    else __asm__("scvtf %d0, %1" : "=w"(r) : "r"(v));
                } else {
                    uint32_t v32 = (uint32_t)v;
                    if (uns) __asm__("ucvtf %d0, %w1" : "=w"(r) : "r"(v32));
                    else __asm__("scvtf %d0, %w1" : "=w"(r) : "r"(v32));
                }
                v_set_f64(cpu, rd, r);
            } else {
                v_set_u16(cpu, rd, hw_int_to_f16(v, uns, sf));
            }
        } else {
            uint32_t mode = (opcode >= 4U) ? (8U + (opcode & 1U)) : ((rmode << 1) | (opcode & 1U));
            uint64_t r = 0;
            if (ftype == 0U) r = hw_f32_to_int(mode, v_get_f32(cpu, rn), sf);
            else if (ftype == 1U) r = hw_f64_to_int(mode, v_get_f64(cpu, rn), sf);
            else r = hw_f16_to_int(mode, v_get_u16(cpu, rn), sf);
            cpu_set_x(cpu, rd, r);
        }
        break;
    }
    case SUBOP_FP_2FIX: {
        bool sf = ((word >> 31) & 1U) != 0;
        uint32_t opcode = (word >> 16) & 7U;
        uint32_t scale = (word >> 10) & 0x3FU;
        uint32_t fbits = 64U - scale;
        if (opcode == 2U || opcode == 3U) {
            bool uns = (opcode == 3U);
            uint64_t r = hw_fix_gpr_to_fp(ftype, cpu_get_x(cpu, rn), fbits, uns, sf);
            cpu->v[rd] = 0;
            uint32_t nb = ftype == 0U ? 4U : (ftype == 1U ? 8U : 2U);
            cpu_lane_set((uint8_t *)&cpu->v[rd], 0, nb, r);
        } else {
            bool uns = (opcode & 1U) != 0;
            uint32_t nb = ftype == 0U ? 4U : (ftype == 1U ? 8U : 2U);
            uint64_t bits = cpu_lane_get_u((const uint8_t *)&cpu->v[rn], 0, nb);
            cpu_set_x(cpu, rd, hw_fp_to_fix_gpr(ftype, bits, fbits, uns, sf));
        }
        break;
    }
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}

static inline uint64_t sat_signed(vcpu_t *cpu, __int128 val, uint32_t bits) {
    __int128 hi = ((__int128)1 << (bits - 1U)) - 1;
    __int128 lo = -((__int128)1 << (bits - 1U));
    if (val > hi) { cpu->fpsr |= CPU_FPSR_QC; return (uint64_t)hi; }
    if (val < lo) { cpu->fpsr |= CPU_FPSR_QC; return (uint64_t)lo; }
    return (uint64_t)val;
}

static inline uint64_t sat_unsigned(vcpu_t *cpu, __int128 val, uint32_t bits) {
    __uint128_t hi = (((__uint128_t)1 << bits) - 1U);
    if (val < 0) { cpu->fpsr |= CPU_FPSR_QC; return 0; }
    if ((__uint128_t)val > hi) { cpu->fpsr |= CPU_FPSR_QC; return (uint64_t)hi; }
    return (uint64_t)val;
}

static inline uint64_t poly_mul8(uint8_t a, uint8_t b) {
    uint64_t r = 0;
    for (uint32_t i = 0; i < 8U; ++i) {
        if ((b >> i) & 1U) r ^= ((uint64_t)a << i);
    }
    return r;
}

typedef int8_t simd_s8 __attribute__((vector_size(16)));
typedef uint8_t simd_u8 __attribute__((vector_size(16)));
typedef int16_t simd_s16 __attribute__((vector_size(16)));
typedef uint16_t simd_u16 __attribute__((vector_size(16)));
typedef int32_t simd_s32 __attribute__((vector_size(16)));
typedef uint32_t simd_u32 __attribute__((vector_size(16)));
typedef int64_t simd_s64 __attribute__((vector_size(16)));
typedef uint64_t simd_u64 __attribute__((vector_size(16)));

static inline void simd_store(vcpu_t *cpu, uint32_t rd, uint8x16_t r, bool q) {
    vst1q_u8((uint8_t *)&cpu->v[rd], q ? r : vcombine_u8(vget_low_u8(r), vdup_n_u8(0)));
}

static uint8x16_t simd_add(uint8x16_t a, uint8x16_t b, uint32_t size) {
    switch (size) {
    case 0U: return (uint8x16_t)((simd_u8)a + (simd_u8)b);
    case 1U: return (uint8x16_t)((simd_u16)a + (simd_u16)b);
    case 2U: return (uint8x16_t)((simd_u32)a + (simd_u32)b);
    default: return (uint8x16_t)((simd_u64)a + (simd_u64)b);
    }
}

static uint8x16_t simd_shift(uint8x16_t a, uint32_t size, int8_t sh, bool is_signed) {
    int8x16_t s = vdupq_n_s8(sh);
    if (is_signed) {
        switch (size) {
        case 0U: return (uint8x16_t)vshlq_s8((int8x16_t)a, s);
        case 1U: return (uint8x16_t)vshlq_s16((int16x8_t)a, (int16x8_t)s);
        case 2U: return (uint8x16_t)vshlq_s32((int32x4_t)a, (int32x4_t)s);
        default: return (uint8x16_t)vshlq_s64((int64x2_t)a, (int64x2_t)s);
        }
    }
    switch (size) {
    case 0U: return vshlq_u8(a, s);
    case 1U: return (uint8x16_t)vshlq_u16((uint16x8_t)a, (int16x8_t)s);
    case 2U: return (uint8x16_t)vshlq_u32((uint32x4_t)a, (int32x4_t)s);
    default: return (uint8x16_t)vshlq_u64((uint64x2_t)a, (int64x2_t)s);
    }
}

static bool simd_same_int_vec(uint32_t u, uint32_t opc, uint32_t size, uint8x16_t a, uint8x16_t b,
                              uint8x16_t *out) {
#define SIMD_SAME_INT(S, U)                                                    \
    switch ((u << 5) | opc) {                                                  \
    case 0x06U: *out = (uint8x16_t)((S)a > (S)b); return true;                 \
    case 0x26U: *out = (uint8x16_t)((U)a > (U)b); return true;                 \
    case 0x07U: *out = (uint8x16_t)((S)a >= (S)b); return true;                \
    case 0x27U: *out = (uint8x16_t)((U)a >= (U)b); return true;                \
    case 0x10U: *out = (uint8x16_t)((U)a + (U)b); return true;                 \
    case 0x30U: *out = (uint8x16_t)((U)a - (U)b); return true;                 \
    case 0x11U: *out = (uint8x16_t)(((U)a & (U)b) != 0); return true;          \
    case 0x31U: *out = (uint8x16_t)((U)a == (U)b); return true;                \
    default: return false;                                                     \
    }
    switch (size) {
    case 0U: SIMD_SAME_INT(simd_s8, simd_u8)
    case 1U: SIMD_SAME_INT(simd_s16, simd_u16)
    case 2U: SIMD_SAME_INT(simd_s32, simd_u32)
    default: SIMD_SAME_INT(simd_s64, simd_u64)
    }
#undef SIMD_SAME_INT
}

static uint64_t shift_signed_round(vcpu_t *cpu, int64_t a, int32_t sh, uint32_t bits, bool rounding, bool sat) {
    if (sh == 0) return (uint64_t)a;
    if (sh > 0) {
        if (sat) {
            if (sh >= 64) {
                if (a != 0) {
                    cpu->fpsr |= CPU_FPSR_QC;
                    return a > 0 ? ((bits == 64U ? (uint64_t)INT64_MAX : ((1ULL << (bits - 1U)) - 1ULL)))
                                 : (bits == 64U ? (uint64_t)INT64_MIN : (~0ULL << (bits - 1U)));
                }
                return 0;
            }
            __int128 res = (__int128)a << sh;
            return sat_signed(cpu, res, bits);
        }
        return sh >= (int32_t)bits ? 0ULL : (uint64_t)((uint64_t)a << sh);
    }
    uint32_t rsh = (uint32_t)(-sh);
    if (rsh >= bits) {
        return a < 0 ? (rounding ? 0ULL : UINT64_MAX) : 0ULL;
    }
    __int128 num = (__int128)a + (rounding ? ((__int128)1 << (rsh - 1U)) : 0);
    return (uint64_t)(num >> rsh);
}

static uint64_t shift_unsigned_round(vcpu_t *cpu, uint64_t a, int32_t sh, uint32_t bits, bool rounding, bool sat) {
    if (sh == 0) return a;
    if (sh > 0) {
        if (sat) {
            if (sh >= 64) {
                if (a != 0) {
                    cpu->fpsr |= CPU_FPSR_QC;
                    return bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL);
                }
                return 0;
            }
            __uint128_t res = (__uint128_t)a << sh;
            __uint128_t hi = bits == 64U ? (__uint128_t)UINT64_MAX : (((__uint128_t)1 << bits) - 1U);
            if (res > hi) { cpu->fpsr |= CPU_FPSR_QC; return (uint64_t)hi; }
            return (uint64_t)res;
        }
        return sh >= (int32_t)bits ? 0ULL : (a << sh);
    }
    uint32_t rsh = (uint32_t)(-sh);
    if (rsh >= bits) {
        if (rounding && rsh == bits) {
            return (a >> (bits - 1U)) & 1ULL;
        }
        return 0ULL;
    }
    __uint128_t num = (__uint128_t)a + (rounding ? ((__uint128_t)1 << (rsh - 1U)) : 0U);
    return (uint64_t)(num >> rsh);
}

static inline uint32_t fp_cmp_bits(uint32_t op, uint32_t sz, uint64_t a_bits, uint64_t b_bits) {
    if (sz == 1U) {
        if (op == 5U || op == 6U) {
            uint16_t aa = (uint16_t)(a_bits & 0x7FFFU), bb = (uint16_t)(b_bits & 0x7FFFU);
            uint32_t nzcv = hw_f16_cmp(aa, bb, true, false);
            bool z = (nzcv >> 30) & 1U, n = (nzcv >> 31) & 1U, v = (nzcv >> 28) & 1U;
            return op == 5U ? ((!v && (n == v)) ? 1U : 0U) : ((!v && !z && (n == v)) ? 1U : 0U);
        }
        uint32_t nzcv = hw_f16_cmp((uint16_t)a_bits, (uint16_t)b_bits, op != 0U, false);
        bool z = (nzcv >> 30) & 1U;
        bool n = (nzcv >> 31) & 1U;
        bool v = (nzcv >> 28) & 1U;
        if (op == 0U) return z ? 1U : 0U;
        if (op == 1U) return (!v && (n == v)) ? 1U : 0U;
        if (op == 2U) return (!v && !z && (n == v)) ? 1U : 0U;
        if (op == 3U) return (!v && (n != v || z)) ? 1U : 0U;
        return (!v && (n != v)) ? 1U : 0U;
    }
    if (sz == 2U) {
        float fa, fb;
        memcpy(&fa, &a_bits, 4);
        memcpy(&fb, &b_bits, 4);
        if (op == 5U || op == 6U) { fa = fabsf(fa); fb = fabsf(fb); }
        uint64_t nzcv;
        if (op == 0U) __asm__("fcmp %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
        else __asm__("fcmpe %s1, %s2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
        bool z = (nzcv >> 30) & 1U, n = (nzcv >> 31) & 1U, v = (nzcv >> 28) & 1U;
        if (op == 0U) return z ? 1U : 0U;
        if (op == 1U || op == 5U) return (!v && (n == v)) ? 1U : 0U;
        if (op == 2U || op == 6U) return (!v && !z && (n == v)) ? 1U : 0U;
        if (op == 3U) return (!v && (n != v || z)) ? 1U : 0U;
        return (!v && (n != v)) ? 1U : 0U;
    }
    double da, db;
    memcpy(&da, &a_bits, 8);
    memcpy(&db, &b_bits, 8);
    if (op == 5U || op == 6U) { da = fabs(da); db = fabs(db); }
    uint64_t nzcv;
    if (op == 0U) __asm__("fcmp %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(da), "w"(db) : "cc");
    else __asm__("fcmpe %d1, %d2\n\tmrs %0, nzcv" : "=r"(nzcv) : "w"(da), "w"(db) : "cc");
    bool z = (nzcv >> 30) & 1U, n = (nzcv >> 31) & 1U, v = (nzcv >> 28) & 1U;
    if (op == 0U) return z ? 1U : 0U;
    if (op == 1U || op == 5U) return (!v && (n == v)) ? 1U : 0U;
    if (op == 2U || op == 6U) return (!v && !z && (n == v)) ? 1U : 0U;
    if (op == 3U) return (!v && (n != v || z)) ? 1U : 0U;
    return (!v && (n != v)) ? 1U : 0U;
}

static uint64_t fp_lane_binop(uint32_t op, uint32_t sz, uint64_t ab, uint64_t bb) {
    if (sz == 1U) return hw_f16_binary(op, (uint16_t)ab, (uint16_t)bb);
    if (sz == 2U) {
        float a, b;
        memcpy(&a, &ab, 4); memcpy(&b, &bb, 4);
        float r = hw_f32_binary(op, a, b);
        uint32_t rb; memcpy(&rb, &r, 4);
        return rb;
    }
    double a, b;
    memcpy(&a, &ab, 8); memcpy(&b, &bb, 8);
    double r = hw_f64_binary(op, a, b);
    uint64_t rb; memcpy(&rb, &r, 8);
    return rb;
}

static uint64_t fp_lane_fma(bool sub, uint32_t sz, uint64_t ab, uint64_t bb, uint64_t cb) {
    if (sz == 1U) return hw_f16_ternary(sub ? 1U : 0U, (uint16_t)ab, (uint16_t)bb, (uint16_t)cb);
    if (sz == 2U) {
        float a, b, c, r;
        memcpy(&a, &ab, 4); memcpy(&b, &bb, 4); memcpy(&c, &cb, 4);
        if (sub) __asm__("fmsub %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
        else __asm__("fmadd %s0, %s1, %s2, %s3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
        uint32_t rb; memcpy(&rb, &r, 4);
        return rb;
    }
    double a, b, c, r;
    memcpy(&a, &ab, 8); memcpy(&b, &bb, 8); memcpy(&c, &cb, 8);
    if (sub) __asm__("fmsub %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
    else __asm__("fmadd %d0, %d1, %d2, %d3" : "=w"(r) : "w"(a), "w"(b), "w"(c));
    uint64_t rb; memcpy(&rb, &r, 8);
    return rb;
}

static uint64_t fp_lane_unop(uint32_t op, uint32_t sz, uint64_t ab) {
    if (sz == 1U) return hw_f16_unary(op, (uint16_t)ab);
    if (sz == 2U) {
        float a; memcpy(&a, &ab, 4);
        float r = hw_f32_unary(op, a);
        uint32_t rb; memcpy(&rb, &r, 4);
        return rb;
    }
    double a; memcpy(&a, &ab, 8);
    double r = hw_f64_unary(op, a);
    uint64_t rb; memcpy(&rb, &r, 8);
    return rb;
}

static uint64_t simd_same_int_lane(vcpu_t *cpu, uint32_t u, uint32_t opc, uint32_t bits,
                                   uint64_t au, uint64_t bu, uint64_t du) {
    int64_t as = cpu_sext(au, bits), bs = cpu_sext(bu, bits);
    uint64_t all1 = bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL);
    switch (opc) {
    case 0U:
        return u ? (uint64_t)(((__uint128_t)au + bu) >> 1) : (uint64_t)(((__int128)as + bs) >> 1);
    case 1U:
        return u ? sat_unsigned(cpu, (__int128)au + (__int128)bu, bits)
                 : sat_signed(cpu, (__int128)as + (__int128)bs, bits);
    case 2U:
        return u ? (uint64_t)(((__uint128_t)au + bu + 1U) >> 1) : (uint64_t)(((__int128)as + bs + 1) >> 1);
    case 4U:
        return u ? (uint64_t)(((__int128)au - (__int128)bu) >> 1) : (uint64_t)(((__int128)as - bs) >> 1);
    case 5U:
        return u ? sat_unsigned(cpu, (__int128)au - (__int128)bu, bits)
                 : sat_signed(cpu, (__int128)as - (__int128)bs, bits);
    case 6U:
        return (u ? (au > bu) : (as > bs)) ? all1 : 0ULL;
    case 7U:
        return (u ? (au >= bu) : (as >= bs)) ? all1 : 0ULL;
    case 8U: {
        int32_t sh = (int8_t)bu;
        return u ? shift_unsigned_round(cpu, au, sh, bits, false, false)
                 : shift_signed_round(cpu, as, sh, bits, false, false);
    }
    case 9U: {
        int32_t sh = (int8_t)bu;
        return u ? shift_unsigned_round(cpu, au, sh, bits, false, true)
                 : shift_signed_round(cpu, as, sh, bits, false, true);
    }
    case 10U: {
        int32_t sh = (int8_t)bu;
        return u ? shift_unsigned_round(cpu, au, sh, bits, true, false)
                 : shift_signed_round(cpu, as, sh, bits, true, false);
    }
    case 11U: {
        int32_t sh = (int8_t)bu;
        return u ? shift_unsigned_round(cpu, au, sh, bits, true, true)
                 : shift_signed_round(cpu, as, sh, bits, true, true);
    }
    case 12U:
        return u ? (au > bu ? au : bu) : (uint64_t)(as > bs ? as : bs);
    case 13U:
        return u ? (au < bu ? au : bu) : (uint64_t)(as < bs ? as : bs);
    case 14U: {
        uint64_t d = u ? (au > bu ? au - bu : bu - au)
                       : (uint64_t)(as > bs ? (__int128)as - bs : (__int128)bs - as);
        return d;
    }
    case 15U: {
        uint64_t d = u ? (au > bu ? au - bu : bu - au)
                       : (uint64_t)(as > bs ? (__int128)as - bs : (__int128)bs - as);
        return du + d;
    }
    case 16U: return u ? (au - bu) : (au + bu);
    case 17U: return (u ? (au == bu) : ((au & bu) != 0)) ? all1 : 0ULL;
    case 18U: return u ? (du - au * bu) : (du + au * bu);
    case 19U: return u ? poly_mul8((uint8_t)au, (uint8_t)bu) : (au * bu);
    case 22U: {
        __int128 prod = ((__int128)as * (__int128)bs) * 2;
        if (u) prod += ((__int128)1 << (bits - 1U));
        return sat_signed(cpu, prod >> bits, bits);
    }
    default: return 0;
    }
}

static float fhm_f16_to_f32(uint32_t fpcr, uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000U) << 16;
    uint32_t exp = (uint32_t)((h >> 10) & 0x1FU);
    uint32_t mant = (uint32_t)(h & 0x03FFU);
    uint32_t bits;
    if (exp == 31U) {
        bits = sign | 0x7F800000U | (mant << 13);
    } else if (exp == 0U) {
        if (mant == 0U || (fpcr & (1U << 19)) != 0U) {
            bits = sign;
        } else {
            uint32_t lz = (uint32_t)__builtin_clz(mant) - 22U;
            bits = sign | ((112U - lz) << 23) | (((mant << (lz + 1U)) & 0x03FFU) << 13);
        }
    } else {
        bits = sign | ((exp + 112U) << 23) | (mant << 13);
    }
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

static void exec_simd_same(vcpu_t *cpu, uint32_t word, bool scalar, bool fp16_only) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t size = (word >> 22) & 3U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t opc = (word >> 11) & 0x1FU;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;

    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vm = (const uint8_t *)&cpu->v[rm];
    const uint8_t *vd_in = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    if (!scalar && !fp16_only && opc == 3U) {
        uint8x16_t a = vld1q_u8(vn), b = vld1q_u8(vm), d = vld1q_u8(vd_in), r;
        switch ((u << 2) | size) {
        case 0U: r = vandq_u8(a, b); break;
        case 1U: r = vbicq_u8(a, b); break;
        case 2U: r = vorrq_u8(a, b); break;
        case 3U: r = vornq_u8(a, b); break;
        case 4U: r = veorq_u8(a, b); break;
        case 5U: r = vbslq_u8(d, a, b); break;
        case 6U: r = vbslq_u8(b, a, d); break;
        default: r = vbslq_u8(b, d, a); break;
        }
        simd_store(cpu, rd, r, q);
        return;
    }

    bool is_fp = fp16_only || (opc >= 24U);
    if (is_fp) {
        uint32_t fsz = fp16_only ? 1U : ((size & 1U) ? 3U : 2U);
        uint32_t ebytes = 1U << fsz;
        uint32_t elems = scalar ? 1U : ((q ? 16U : 8U) / ebytes);
        uint32_t sz_hi = fp16_only ? ((word >> 23) & 1U) : (size >> 1);
        uint32_t o_low = fp16_only ? (opc & 7U) : (opc - 24U);

        bool is_pair = (!scalar) && (
            (u == 1U && sz_hi == 0U && (o_low == 0U || o_low == 2U || o_low == 6U)) ||
            (u == 1U && sz_hi == 1U && (o_low == 0U || o_low == 6U))
        );
        uint32_t op_sel = (u << 4) | (sz_hi << 3) | o_low;
        if (!scalar && !fp16_only && (op_sel == 0x05U || op_sel == 0x0DU || op_sel == 0x11U || op_sel == 0x19U)) {
            bool part = (op_sel == 0x11U || op_sel == 0x19U);
            bool sub = (op_sel == 0x0DU || op_sel == 0x19U);
            uint32_t cnt = q ? 4U : 2U;
            uint32_t s_off = part ? (cnt * 2U) : 0U;
            for (uint32_t i = 0; i < cnt; ++i) {
                uint16_t ah = 0, bh = 0;
                memcpy(&ah, vn + s_off + i * 2U, 2);
                memcpy(&bh, vm + s_off + i * 2U, 2);
                float a32 = fhm_f16_to_f32(cpu->fpcr, ah);
                float b32 = fhm_f16_to_f32(cpu->fpcr, bh);
                float d32 = 0, r32 = 0;
                memcpy(&d32, vd_in + i * 4U, 4);
                if (sub) __asm__("fmsub %s0, %s1, %s2, %s3" : "=w"(r32) : "w"(a32), "w"(b32), "w"(d32));
                else __asm__("fmadd %s0, %s1, %s2, %s3" : "=w"(r32) : "w"(a32), "w"(b32), "w"(d32));
                memcpy(out + i * 4U, &r32, 4);
            }
            memcpy(&cpu->v[rd], out, 16);
            return;
        }
        if (is_pair) {
            uint32_t half = elems / 2U;
            for (uint32_t i = 0; i < elems; ++i) {
                const uint8_t *src = (i < half) ? vn : vm;
                uint32_t p = (i < half) ? (i * 2U) : ((i - half) * 2U);
                uint64_t a = cpu_lane_get_u(src, p, ebytes);
                uint64_t b = cpu_lane_get_u(src, p + 1U, ebytes);
                uint64_t r = 0;
                if (op_sel == 0x10U) r = fp_lane_binop(6U, fsz, a, b);
                else if (op_sel == 0x12U) r = fp_lane_binop(2U, fsz, a, b);
                else if (op_sel == 0x16U) r = fp_lane_binop(4U, fsz, a, b);
                else if (op_sel == 0x18U) r = fp_lane_binop(7U, fsz, a, b);
                else r = fp_lane_binop(5U, fsz, a, b);
                cpu_lane_set(out, i, ebytes, r);
            }
        } else {
            uint64_t all1 = fsz == 3U ? UINT64_MAX : ((1ULL << (ebytes * 8U)) - 1ULL);
            for (uint32_t i = 0; i < elems; ++i) {
                uint64_t a = cpu_lane_get_u(vn, i, ebytes);
                uint64_t b = cpu_lane_get_u(vm, i, ebytes);
                uint64_t d = cpu_lane_get_u(vd_in, i, ebytes);
                uint64_t r = 0;
                switch (op_sel) {
                case 0x00U: r = fp_lane_binop(6U, fsz, a, b); break;
                case 0x01U: r = fp_lane_fma(false, fsz, a, b, d); break;
                case 0x02U: r = fp_lane_binop(2U, fsz, a, b); break;
                case 0x03U: r = fp_lane_binop(10U, fsz, a, b); break;
                case 0x04U: r = fp_cmp_bits(0U, fsz, a, b) ? all1 : 0ULL; break;
                case 0x06U: r = fp_lane_binop(4U, fsz, a, b); break;
                case 0x07U: r = fp_lane_binop(11U, fsz, a, b); break;
                case 0x08U: r = fp_lane_binop(7U, fsz, a, b); break;
                case 0x09U: r = fp_lane_fma(true, fsz, a, b, d); break;
                case 0x0AU: r = fp_lane_binop(3U, fsz, a, b); break;
                case 0x0EU: r = fp_lane_binop(5U, fsz, a, b); break;
                case 0x0FU: r = fp_lane_binop(12U, fsz, a, b); break;
                case 0x13U: r = fp_lane_binop(0U, fsz, a, b); break;
                case 0x14U: r = fp_cmp_bits(1U, fsz, a, b) ? all1 : 0ULL; break;
                case 0x15U: r = fp_cmp_bits(5U, fsz, a, b) ? all1 : 0ULL; break;
                case 0x17U: r = fp_lane_binop(1U, fsz, a, b); break;
                case 0x1AU: r = fp_lane_binop(9U, fsz, a, b); break;
                case 0x1CU: r = fp_cmp_bits(2U, fsz, a, b) ? all1 : 0ULL; break;
                case 0x1DU: r = fp_cmp_bits(6U, fsz, a, b) ? all1 : 0ULL; break;
                default: break;
                }
                cpu_lane_set(out, i, ebytes, r);
            }
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    uint32_t ebytes = 1U << size;
    uint32_t bits = ebytes * 8U;
    uint32_t elems = scalar ? 1U : ((q ? 16U : 8U) / ebytes);

    if (!scalar && (opc == 20U || opc == 21U || (opc == 23U && u == 0U))) {
        uint32_t half = elems / 2U;
        for (uint32_t i = 0; i < elems; ++i) {
            const uint8_t *src = (i < half) ? vn : vm;
            uint32_t p = (i < half) ? (i * 2U) : ((i - half) * 2U);
            uint64_t au = cpu_lane_get_u(src, p, ebytes);
            uint64_t bu = cpu_lane_get_u(src, p + 1U, ebytes);
            int64_t as = cpu_sext(au, bits), bs = cpu_sext(bu, bits);
            uint64_t r = 0;
            if (opc == 23U) r = au + bu;
            else if (opc == 20U) r = u ? (au > bu ? au : bu) : (uint64_t)(as > bs ? as : bs);
            else r = u ? (au < bu ? au : bu) : (uint64_t)(as < bs ? as : bs);
            cpu_lane_set(out, i, ebytes, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    uint8x16_t vec;
    if (!scalar && simd_same_int_vec(u, opc, size, vld1q_u8(vn), vld1q_u8(vm), &vec)) {
        simd_store(cpu, rd, vec, q);
        return;
    }

    for (uint32_t i = 0; i < elems; ++i) {
        uint64_t au = cpu_lane_get_u(vn, i, ebytes);
        uint64_t bu = cpu_lane_get_u(vm, i, ebytes);
        uint64_t du = cpu_lane_get_u(vd_in, i, ebytes);
        uint64_t r = simd_same_int_lane(cpu, u, opc, bits, au, bu, du);
        cpu_lane_set(out, i, ebytes, r);
    }
    memcpy(&cpu->v[rd], out, 16);
}

static inline uint64_t fp_neg_bits(uint32_t fsz, uint64_t v) {
    return v ^ (fsz == 1U ? 0x8000ULL : (fsz == 2U ? 0x80000000ULL : 0x8000000000000000ULL));
}

static void exec_fcmla_pair(uint32_t fsz, uint32_t rot, uint64_t an0, uint64_t an1,
                            uint64_t am0, uint64_t am1, uint64_t d0, uint64_t d1,
                            uint64_t *r0, uint64_t *r1) {
    uint64_t a1, a2, a3, a4;
    switch (rot) {
    case 0U: a1 = an0; a2 = am0; a3 = an0; a4 = am1; break;
    case 1U: a1 = an1; a2 = fp_neg_bits(fsz, am1); a3 = an1; a4 = am0; break;
    case 2U: a1 = an0; a2 = fp_neg_bits(fsz, am0); a3 = an0; a4 = fp_neg_bits(fsz, am1); break;
    default: a1 = an1; a2 = am1; a3 = an1; a4 = fp_neg_bits(fsz, am0); break;
    }
    *r0 = fp_lane_fma(false, fsz, a1, a2, d0);
    *r1 = fp_lane_fma(false, fsz, a3, a4, d1);
}

static void exec_simd_same2(vcpu_t *cpu, uint32_t word, bool scalar) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t size = (word >> 22) & 3U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t opc = (word >> 11) & 0x1FU;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;
    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vm = (const uint8_t *)&cpu->v[rm];
    const uint8_t *vd = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    if (!scalar && u == 1U && (opc & 0x1CU) == 0x18U) {
        uint32_t rot = opc & 3U;
        uint32_t fsz = size;
        uint32_t eb = 1U << fsz;
        uint32_t pairs = (q ? 16U : 8U) / (eb * 2U);
        for (uint32_t i = 0; i < pairs; ++i) {
            uint64_t r0 = 0, r1 = 0;
            exec_fcmla_pair(fsz, rot,
                            cpu_lane_get_u(vn, i * 2U, eb), cpu_lane_get_u(vn, i * 2U + 1U, eb),
                            cpu_lane_get_u(vm, i * 2U, eb), cpu_lane_get_u(vm, i * 2U + 1U, eb),
                            cpu_lane_get_u(vd, i * 2U, eb), cpu_lane_get_u(vd, i * 2U + 1U, eb),
                            &r0, &r1);
            cpu_lane_set(out, i * 2U, eb, r0);
            cpu_lane_set(out, i * 2U + 1U, eb, r1);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }
    if (!scalar && u == 1U && (opc == 28U || opc == 30U)) {
        uint32_t rot = (opc >> 1) & 1U;
        uint32_t fsz = size;
        uint32_t eb = 1U << fsz;
        uint32_t pairs = (q ? 16U : 8U) / (eb * 2U);
        for (uint32_t i = 0; i < pairs; ++i) {
            uint64_t a0 = cpu_lane_get_u(vn, i * 2U, eb);
            uint64_t a1 = cpu_lane_get_u(vn, i * 2U + 1U, eb);
            uint64_t b0 = cpu_lane_get_u(vm, i * 2U, eb);
            uint64_t b1 = cpu_lane_get_u(vm, i * 2U + 1U, eb);
            uint64_t r0 = fp_lane_binop(2U, fsz, a0, rot ? b1 : fp_neg_bits(fsz, b1));
            uint64_t r1 = fp_lane_binop(2U, fsz, a1, rot ? fp_neg_bits(fsz, b0) : b0);
            cpu_lane_set(out, i * 2U, eb, r0);
            cpu_lane_set(out, i * 2U + 1U, eb, r1);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    if (opc == 18U) {
        uint32_t elems = q ? 4U : 2U;
        for (uint32_t i = 0; i < elems; ++i) {
            uint32_t acc = (uint32_t)cpu_lane_get_u(vd, i, 4U);
            for (uint32_t k = 0; k < 4U; ++k) {
                uint32_t idx = i * 4U + k;
                if (u) acc += (uint32_t)vn[idx] * (uint32_t)vm[idx];
                else acc += (uint32_t)((int32_t)(int8_t)vn[idx] * (int32_t)(int8_t)vm[idx]);
            }
            cpu_lane_set(out, i, 4U, acc);
        }
    } else {
        uint32_t ebytes = 1U << size;
        uint32_t bits = ebytes * 8U;
        uint32_t elems = scalar ? 1U : ((q ? 16U : 8U) / ebytes);
        bool sub = (opc == 17U);
        for (uint32_t i = 0; i < elems; ++i) {
            int64_t a = cpu_lane_get_s(vn, i, ebytes);
            int64_t b = cpu_lane_get_s(vm, i, ebytes);
            int64_t d = cpu_lane_get_s(vd, i, ebytes);
            __int128 prod = (__int128)a * (__int128)b * 2;
            __int128 acc = ((__int128)d << bits) + (sub ? -prod : prod) + ((__int128)1 << (bits - 1U));
            cpu_lane_set(out, i, ebytes, sat_signed(cpu, acc >> bits, bits));
        }
    }
    memcpy(&cpu->v[rd], out, 16);
}

static void exec_simd_misc(vcpu_t *cpu, uint32_t word, bool scalar, bool fp16_only) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t size = (word >> 22) & 3U;
    uint32_t opc = (word >> 12) & 0x1FU;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;
    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vd = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    uint32_t ebytes = 1U << size;
    uint32_t bits = ebytes * 8U;
    uint32_t datasize = q ? 16U : 8U;
    uint32_t elems = scalar ? 1U : (datasize / ebytes);

    if (fp16_only || opc >= 22U || (opc >= 12U && opc <= 15U)) {
        uint32_t sz_hi = fp16_only ? ((word >> 23) & 1U) : (size >> 1);
        uint32_t fsz = fp16_only ? 1U : ((size & 1U) ? 3U : 2U);
        uint32_t fb = 1U << fsz;
        uint32_t felems = scalar ? 1U : (datasize / fb);
        uint64_t all1 = fsz == 3U ? UINT64_MAX : ((1ULL << (fb * 8U)) - 1ULL);

        if (!fp16_only && !scalar && (opc == 22U || opc == 23U)) {
            if (opc == 22U && u == 0U && sz_hi == 0U) {
                uint32_t d_off = q ? 8U : 0U;
                if (q) memcpy(out, vd, 8);
                if (size == 0U) {
                    for (uint32_t i = 0; i < 4U; ++i) {
                        float f; memcpy(&f, vn + i * 4U, 4);
                        uint16_t h = hw_f32_to_f16(f);
                        memcpy(out + d_off + i * 2U, &h, 2);
                    }
                } else {
                    for (uint32_t i = 0; i < 2U; ++i) {
                        double d; memcpy(&d, vn + i * 8U, 8);
                        float f; __asm__("fcvt %s0, %d1" : "=w"(f) : "w"(d));
                        memcpy(out + d_off + i * 4U, &f, 4);
                    }
                }
                memcpy(&cpu->v[rd], out, 16);
                return;
            }
            if (opc == 22U && u == 1U && sz_hi == 0U) {
                uint32_t d_off = q ? 8U : 0U;
                if (q) memcpy(out, vd, 8);
                for (uint32_t i = 0; i < 2U; ++i) {
                    double d; memcpy(&d, vn + i * 8U, 8);
                    float32x2_t f = vcvtx_f32_f64(vdupq_n_f64(d));
                    float fs = vget_lane_f32(f, 0);
                    memcpy(out + d_off + i * 4U, &fs, 4);
                }
                memcpy(&cpu->v[rd], out, 16);
                return;
            }
            if (opc == 23U && u == 0U && sz_hi == 0U) {
                uint32_t s_off = q ? 8U : 0U;
                if (size == 0U) {
                    for (uint32_t i = 0; i < 4U; ++i) {
                        uint16_t h; memcpy(&h, vn + s_off + i * 2U, 2);
                        float f = hw_f16_to_f32(h);
                        memcpy(out + i * 4U, &f, 4);
                    }
                } else {
                    for (uint32_t i = 0; i < 2U; ++i) {
                        float f; memcpy(&f, vn + s_off + i * 4U, 4);
                        double d; __asm__("fcvt %d0, %s1" : "=w"(d) : "w"(f));
                        memcpy(out + i * 8U, &d, 8);
                    }
                }
                memcpy(&cpu->v[rd], out, 16);
                return;
            }
        }
        if (!fp16_only && scalar && opc == 22U && u == 1U) {
            double d = v_get_f64(cpu, rn);
            float32x2_t f = vcvtx_f32_f64(vdupq_n_f64(d));
            v_set_f32(cpu, rd, vget_lane_f32(f, 0));
            return;
        }
        if (!fp16_only && !scalar && opc == 28U && sz_hi == 1U && u == 0U) {
            for (uint32_t i = 0; i < elems; ++i) {
                uint32x2_t v = vdup_n_u32((uint32_t)cpu_lane_get_u(vn, i, 4U));
                cpu_lane_set(out, i, 4U, vget_lane_u32(vrecpe_u32(v), 0));
            }
            memcpy(&cpu->v[rd], out, 16);
            return;
        }
        if (!fp16_only && !scalar && opc == 28U && sz_hi == 1U && u == 1U) {
            for (uint32_t i = 0; i < elems; ++i) {
                uint32x2_t v = vdup_n_u32((uint32_t)cpu_lane_get_u(vn, i, 4U));
                cpu_lane_set(out, i, 4U, vget_lane_u32(vrsqrte_u32(v), 0));
            }
            memcpy(&cpu->v[rd], out, 16);
            return;
        }

        uint32_t mop = (u << 6) | (sz_hi << 5) | opc;
        for (uint32_t i = 0; i < felems; ++i) {
            uint64_t a = cpu_lane_get_u(vn, i, fb);
            uint64_t r = 0;
            switch (mop) {
            case 0x2CU: r = fp_cmp_bits(2U, fsz, a, 0) ? all1 : 0ULL; break;
            case 0x2DU: r = fp_cmp_bits(0U, fsz, a, 0) ? all1 : 0ULL; break;
            case 0x2EU: r = fp_cmp_bits(4U, fsz, a, 0) ? all1 : 0ULL; break;
            case 0x2FU: r = fp_lane_unop(1U, fsz, a); break;
            case 0x6CU: r = fp_cmp_bits(1U, fsz, a, 0) ? all1 : 0ULL; break;
            case 0x6DU: r = fp_cmp_bits(3U, fsz, a, 0) ? all1 : 0ULL; break;
            case 0x6FU: r = fp_lane_unop(2U, fsz, a); break;
            case 0x18U: r = fp_lane_unop(8U, fsz, a); break;
            case 0x19U: r = fp_lane_unop(10U, fsz, a); break;
            case 0x38U: r = fp_lane_unop(9U, fsz, a); break;
            case 0x39U: r = fp_lane_unop(11U, fsz, a); break;
            case 0x58U: r = fp_lane_unop(12U, fsz, a); break;
            case 0x59U: r = fp_lane_unop(14U, fsz, a); break;
            case 0x79U: r = fp_lane_unop(15U, fsz, a); break;
            case 0x7FU: r = fp_lane_unop(3U, fsz, a); break;
            case 0x3DU: r = fp_lane_unop(16U, fsz, a); break;
            case 0x7DU: r = fp_lane_unop(17U, fsz, a); break;
            case 0x3FU: r = fp_lane_unop(18U, fsz, a); break;
            case 0x1AU: case 0x1BU: case 0x3AU: case 0x3BU:
            case 0x5AU: case 0x5BU: case 0x7AU: case 0x7BU: {
                bool uns = u != 0U;
                uint32_t rmode = 0;
                if (opc == 26U && sz_hi == 0U) rmode = 0U;
                else if (opc == 26U && sz_hi == 1U) rmode = 2U;
                else if (opc == 27U && sz_hi == 0U) rmode = 4U;
                else rmode = 6U;
                uint32_t mode = rmode + (uns ? 1U : 0U);
                if (fsz == 1U) r = hw_f16_to_int16(mode, (uint16_t)a);
                else if (fsz == 2U) { float f; memcpy(&f, &a, 4); r = hw_f32_to_int(mode, f, false); }
                else { double d; memcpy(&d, &a, 8); r = hw_f64_to_int(mode, d, true); }
                break;
            }
            case 0x1CU: case 0x5CU: {
                uint32_t mode = 8U + (u ? 1U : 0U);
                if (fsz == 1U) r = hw_f16_to_int16(mode, (uint16_t)a);
                else if (fsz == 2U) { float f; memcpy(&f, &a, 4); r = hw_f32_to_int(mode, f, false); }
                else { double d; memcpy(&d, &a, 8); r = hw_f64_to_int(mode, d, true); }
                break;
            }
            case 0x1DU: case 0x5DU: {
                bool uns = (u != 0U);
                if (fsz == 1U) {
                    r = hw_int16_to_f16((uint16_t)a, uns);
                } else if (fsz == 2U) {
                    float f;
                    uint32_t v32 = (uint32_t)a;
                    if (uns) __asm__("ucvtf %s0, %w1" : "=w"(f) : "r"(v32));
                    else __asm__("scvtf %s0, %w1" : "=w"(f) : "r"(v32));
                    uint32_t rb; memcpy(&rb, &f, 4); r = rb;
                } else {
                    double d;
                    if (uns) __asm__("ucvtf %d0, %1" : "=w"(d) : "r"(a));
                    else __asm__("scvtf %d0, %1" : "=w"(d) : "r"(a));
                    memcpy(&r, &d, 8);
                }
                break;
            }
            default: break;
            }
            cpu_lane_set(out, i, fb, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    if (!scalar && (opc == 0U || (opc == 1U && u == 0U) || opc == 5U)) {
        uint8x16_t a = vld1q_u8(vn), r;
        if (opc == 5U) {
            r = u == 0U ? vcntq_u8(a) : size == 0U ? vmvnq_u8(a) : vrbitq_u8(a);
        } else if (opc == 1U) {
            r = vrev16q_u8(a);
        } else if (u == 0U) {
            r = size == 0U ? vrev64q_u8(a)
              : size == 1U ? (uint8x16_t)vrev64q_u16((uint16x8_t)a)
              : size == 2U ? (uint8x16_t)vrev64q_u32((uint32x4_t)a)
              : a;
        } else {
            r = size == 0U ? vrev32q_u8(a) : size == 1U ? (uint8x16_t)vrev32q_u16((uint16x8_t)a) : a;
        }
        simd_store(cpu, rd, r, q);
        return;
    }
    if (!scalar && (opc == 2U || opc == 6U)) {
        uint32_t db = ebytes * 2U;
        uint32_t pairs = elems / 2U;
        for (uint32_t i = 0; i < pairs; ++i) {
            uint64_t sum = u ? (cpu_lane_get_u(vn, i * 2U, ebytes) + cpu_lane_get_u(vn, i * 2U + 1U, ebytes))
                             : (uint64_t)(cpu_lane_get_s(vn, i * 2U, ebytes) + cpu_lane_get_s(vn, i * 2U + 1U, ebytes));
            if (opc == 6U) sum += cpu_lane_get_u(vd, i, db);
            cpu_lane_set(out, i, db, sum);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }
    if (opc == 18U || opc == 20U) {
        uint32_t sb = ebytes * 2U;
        uint32_t db = ebytes;
        uint32_t cnt = scalar ? 1U : (8U / db);
        uint32_t d_off = (!scalar && q) ? 8U : 0U;
        if (!scalar && q) memcpy(out, vd, 8);
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t r = 0;
            if (opc == 18U && u == 0U) r = cpu_lane_get_u(vn, i, sb);
            else if (opc == 18U && u == 1U) r = sat_unsigned(cpu, cpu_lane_get_s(vn, i, sb), db * 8U);
            else if (opc == 20U && u == 0U) r = sat_signed(cpu, cpu_lane_get_s(vn, i, sb), db * 8U);
            else r = sat_unsigned(cpu, (__int128)cpu_lane_get_u(vn, i, sb), db * 8U);
            cpu_lane_set(out + d_off, i, db, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }
    if (!scalar && opc == 19U && u == 1U) {
        uint32_t db = ebytes * 2U;
        uint32_t cnt = 16U / db;
        uint32_t s_off = q ? 8U : 0U;
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t v = cpu_lane_get_u(vn + s_off, i, ebytes);
            cpu_lane_set(out, i, db, v << (ebytes * 8U));
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    uint64_t all1 = bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL);
    for (uint32_t i = 0; i < elems; ++i) {
        uint64_t au = cpu_lane_get_u(vn, i, ebytes);
        int64_t as = cpu_sext(au, bits);
        uint64_t du = cpu_lane_get_u(vd, i, ebytes);
        int64_t ds = cpu_sext(du, bits);
        uint64_t r = 0;
        switch (opc) {
        case 3U:
            r = u ? sat_unsigned(cpu, (__int128)du + (__int128)as, bits)
                  : sat_signed(cpu, (__int128)ds + (__int128)au, bits);
            break;
        case 4U:
            if (u) {
                r = au == 0 ? bits : (uint64_t)(__builtin_clzll(au) - (int)(64U - bits));
            } else {
                uint64_t shifted = ((uint64_t)as << (64U - bits));
                uint64_t x = (shifted ^ (shifted << 1)) >> 1;
                r = x == 0 ? (bits - 1U) : (uint64_t)(__builtin_clzll(x) - 1);
            }
            break;
        case 7U:
            r = u ? sat_signed(cpu, -(__int128)as, bits)
                  : sat_signed(cpu, as < 0 ? -(__int128)as : (__int128)as, bits);
            break;
        case 8U: r = (u ? (as >= 0) : (as > 0)) ? all1 : 0ULL; break;
        case 9U: r = (u ? (as <= 0) : (au == 0)) ? all1 : 0ULL; break;
        case 10U: r = (as < 0) ? all1 : 0ULL; break;
        case 11U: r = u ? (uint64_t)(-as) : (uint64_t)(as < 0 ? -as : as); break;
        default: break;
        }
        cpu_lane_set(out, i, ebytes, r);
    }
    memcpy(&cpu->v[rd], out, 16);
}

static bool exec_simd_shf(vcpu_t *cpu, uint32_t word, bool scalar) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t immh = (word >> 19) & 15U;
    uint32_t immb = (word >> 16) & 7U;
    uint32_t opc = (word >> 11) & 0x1FU;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;

    bool wide = (immh & 8U) != 0;
    if ((opc == 28U || opc == 31U) && immh == 1U) {
        return false;
    }
    if (opc >= 16U && opc <= 20U) {
        if (wide) {
            return false;
        }
    } else if (scalar ? (!wide && opc <= 10U) : (wide && !q)) {
        return false;
    }

    uint32_t imm7 = (immh << 3) | immb;
    uint32_t size = (immh & 8U) ? 3U : ((immh & 4U) ? 2U : ((immh & 2U) ? 1U : 0U));
    uint32_t ebytes = 1U << size;
    uint32_t bits = ebytes * 8U;
    uint32_t sh_r = (2U * bits) - imm7;
    uint32_t sh_l = imm7 - bits;
    uint32_t datasize = q ? 16U : 8U;
    uint32_t elems = scalar ? 1U : (datasize / ebytes);
    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vd = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    if (!scalar && opc == 20U) {
        uint32_t db = ebytes * 2U;
        uint32_t cnt = 16U / db;
        uint32_t s_off = q ? 8U : 0U;
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t ext = u ? cpu_lane_get_u(vn + s_off, i, ebytes) : (uint64_t)cpu_lane_get_s(vn + s_off, i, ebytes);
            cpu_lane_set(out, i, db, ext << sh_l);
        }
        memcpy(&cpu->v[rd], out, 16);
        return true;
    }

    if (opc >= 16U && opc <= 19U) {
        uint32_t sb = ebytes * 2U;
        uint32_t db = ebytes;
        uint32_t cnt = scalar ? 1U : (8U / db);
        uint32_t d_off = (!scalar && q) ? 8U : 0U;
        if (!scalar && q) memcpy(out, vd, 8);
        bool round = (opc == 17U || opc == 19U);
        bool to_uns = (u != 0U);
        bool src_uns = (u != 0U && (opc == 18U || opc == 19U));
        bool sat = (opc >= 18U || u != 0U);
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t r = 0;
            if (!sat) {
                uint64_t a = cpu_lane_get_u(vn, i, sb);
                __uint128_t num = (__uint128_t)a + (round ? ((__uint128_t)1 << (sh_r - 1U)) : 0U);
                r = (uint64_t)(num >> sh_r);
            } else if (src_uns) {
                uint64_t a = cpu_lane_get_u(vn, i, sb);
                __uint128_t num = (__uint128_t)a + (round ? ((__uint128_t)1 << (sh_r - 1U)) : 0U);
                r = sat_unsigned(cpu, (__int128)(num >> sh_r), db * 8U);
            } else {
                int64_t a = cpu_lane_get_s(vn, i, sb);
                __int128 num = (__int128)a + (round ? ((__int128)1 << (sh_r - 1U)) : 0);
                __int128 shifted = num >> sh_r;
                r = to_uns ? sat_unsigned(cpu, shifted, db * 8U) : sat_signed(cpu, shifted, db * 8U);
            }
            cpu_lane_set(out + d_off, i, db, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return true;
    }

    if (!scalar && (opc == 0U || opc == 2U || (opc == 8U && u != 0U) || opc == 10U)) {
        bool left = opc == 10U;
        int8_t sh = left ? (int8_t)sh_l : (int8_t)-(int32_t)sh_r;
        uint8x16_t d = vld1q_u8(vd);
        uint8x16_t r = simd_shift(vld1q_u8(vn), size, sh, u == 0U && !left);
        if (opc == 2U) {
            r = simd_add(d, r, size);
        } else if (opc == 8U || (left && u != 0U)) {
            r = vbslq_u8(simd_shift(vdupq_n_u8(0xFFU), size, sh, false), r, d);
        }
        simd_store(cpu, rd, r, q);
        return true;
    }

    if (opc == 28U || opc == 31U) {
        uint32_t fbits = sh_r;
        for (uint32_t i = 0; i < elems; ++i) {
            uint64_t a = cpu_lane_get_u(vn, i, ebytes);
            uint64_t r = hw_simd_fix_cvt(size, a, fbits, opc == 28U, u != 0U);
            cpu_lane_set(out, i, ebytes, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return true;
    }

    for (uint32_t i = 0; i < elems; ++i) {
        uint64_t au = cpu_lane_get_u(vn, i, ebytes);
        int64_t as = cpu_sext(au, bits);
        uint64_t du = cpu_lane_get_u(vd, i, ebytes);
        uint64_t r = 0;
        switch (opc) {
        case 0U: r = u ? shift_unsigned_round(cpu, au, -(int32_t)sh_r, bits, false, false)
                       : shift_signed_round(cpu, as, -(int32_t)sh_r, bits, false, false); break;
        case 2U: r = du + (u ? shift_unsigned_round(cpu, au, -(int32_t)sh_r, bits, false, false)
                             : shift_signed_round(cpu, as, -(int32_t)sh_r, bits, false, false)); break;
        case 4U: r = u ? shift_unsigned_round(cpu, au, -(int32_t)sh_r, bits, true, false)
                       : shift_signed_round(cpu, as, -(int32_t)sh_r, bits, true, false); break;
        case 6U: r = du + (u ? shift_unsigned_round(cpu, au, -(int32_t)sh_r, bits, true, false)
                             : shift_signed_round(cpu, as, -(int32_t)sh_r, bits, true, false)); break;
        case 8U: {
            uint64_t mask = sh_r >= bits ? 0ULL : ((bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL)) >> sh_r);
            uint64_t s = sh_r >= bits ? 0ULL : (au >> sh_r);
            r = (du & ~mask) | s;
            break;
        }
        case 10U:
            if (u) {
                uint64_t mask = (bits == 64U ? UINT64_MAX : ((1ULL << bits) - 1ULL)) << sh_l;
                r = (du & ~mask) | (au << sh_l);
            } else {
                r = au << sh_l;
            }
            break;
        case 12U:
            r = sat_unsigned(cpu, (__int128)as << sh_l, bits);
            break;
        case 14U:
            r = u ? shift_unsigned_round(cpu, au, (int32_t)sh_l, bits, false, true)
                  : shift_signed_round(cpu, as, (int32_t)sh_l, bits, false, true);
            break;
        default: break;
        }
        cpu_lane_set(out, i, ebytes, r);
    }
    memcpy(&cpu->v[rd], out, 16);
    return true;
}

static bool exec_simd_diff(vcpu_t *cpu, uint32_t word, bool scalar) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t size = (word >> 22) & 3U;
    uint32_t rm = (word >> 16) & 31U;
    uint32_t opc = (word >> 12) & 15U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;
    if (opc == 14U ? (size == 1U || size == 2U)
                   : (size == 3U || (size == 0U && (opc == 9U || opc == 11U || opc == 13U)))) {
        return false;
    }
    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vm = (const uint8_t *)&cpu->v[rm];
    const uint8_t *vd = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    if (!scalar && opc == 14U) {
        if (size == 0U) {
            uint32_t s_off = q ? 8U : 0U;
            for (uint32_t i = 0; i < 8U; ++i) {
                uint16_t p = (uint16_t)poly_mul8(vn[s_off + i], vm[s_off + i]);
                cpu_lane_set(out, i, 2U, p);
            }
            memcpy(&cpu->v[rd], out, 16);
        } else {
            uint64_t a = cpu_lane_get_u(vn, q ? 1U : 0U, 8U);
            uint64_t b = cpu_lane_get_u(vm, q ? 1U : 0U, 8U);
            cpu->v[rd] = (__uint128_t)vmull_p64(a, b);
        }
        return true;
    }

    uint32_t ebytes = 1U << size;
    uint32_t db = ebytes * 2U;
    uint32_t bits = ebytes * 8U;
    uint32_t dbits = db * 8U;

    if (!scalar && (opc == 4U || opc == 6U)) {
        uint32_t cnt = 8U / ebytes;
        uint32_t d_off = q ? 8U : 0U;
        if (q) memcpy(out, vd, 8);
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t a = cpu_lane_get_u(vn, i, db);
            uint64_t b = cpu_lane_get_u(vm, i, db);
            __uint128_t res = (opc == 4U) ? ((__uint128_t)a + b) : ((__uint128_t)a - b);
            if (u) res += ((__uint128_t)1 << (bits - 1U));
            uint64_t hi = (uint64_t)(res >> bits);
            cpu_lane_set(out + d_off, i, ebytes, hi);
        }
        memcpy(&cpu->v[rd], out, 16);
        return true;
    }

    uint32_t s_off = (!scalar && q) ? 8U : 0U;
    uint32_t cnt = scalar ? 1U : (16U / db);
    for (uint32_t i = 0; i < cnt; ++i) {
        uint64_t au_lo = cpu_lane_get_u(vn + s_off, i, ebytes);
        uint64_t bu_lo = cpu_lane_get_u(vm + s_off, i, ebytes);
        int64_t as_lo = cpu_sext(au_lo, bits), bs_lo = cpu_sext(bu_lo, bits);
        uint64_t a_w = cpu_lane_get_u(vn, i, db);
        uint64_t d_w = cpu_lane_get_u(vd, i, db);
        int64_t ds_w = cpu_sext(d_w, dbits);
        uint64_t r = 0;

        switch (opc) {
        case 0U: r = u ? (au_lo + bu_lo) : (uint64_t)(as_lo + bs_lo); break;
        case 1U: r = u ? (a_w + bu_lo) : (uint64_t)(cpu_sext(a_w, dbits) + bs_lo); break;
        case 2U: r = u ? (au_lo - bu_lo) : (uint64_t)(as_lo - bs_lo); break;
        case 3U: r = u ? (a_w - bu_lo) : (uint64_t)(cpu_sext(a_w, dbits) - bs_lo); break;
        case 5U: {
            uint64_t diff = u ? (au_lo > bu_lo ? au_lo - bu_lo : bu_lo - au_lo)
                              : (uint64_t)(as_lo > bs_lo ? as_lo - bs_lo : bs_lo - as_lo);
            r = d_w + diff;
            break;
        }
        case 7U:
            r = u ? (au_lo > bu_lo ? au_lo - bu_lo : bu_lo - au_lo)
                  : (uint64_t)(as_lo > bs_lo ? as_lo - bs_lo : bs_lo - as_lo);
            break;
        case 8U: r = d_w + (u ? (au_lo * bu_lo) : (uint64_t)(as_lo * bs_lo)); break;
        case 9U: {
            int64_t prod = (int64_t)sat_signed(cpu, (__int128)as_lo * bs_lo * 2, dbits);
            r = sat_signed(cpu, (__int128)ds_w + (__int128)prod, dbits);
            break;
        }
        case 10U: r = d_w - (u ? (au_lo * bu_lo) : (uint64_t)(as_lo * bs_lo)); break;
        case 11U: {
            int64_t prod = (int64_t)sat_signed(cpu, (__int128)as_lo * bs_lo * 2, dbits);
            r = sat_signed(cpu, (__int128)ds_w - (__int128)prod, dbits);
            break;
        }
        case 12U: r = u ? (au_lo * bu_lo) : (uint64_t)(as_lo * bs_lo); break;
        case 13U: r = sat_signed(cpu, (__int128)as_lo * bs_lo * 2, dbits); break;
        default: break;
        }
        cpu_lane_set(out, i, db, r);
    }
    memcpy(&cpu->v[rd], out, 16);
    return true;
}

static void exec_simd_elem(vcpu_t *cpu, uint32_t word, bool scalar) {
    bool q = scalar ? false : (((word >> 30) & 1U) != 0);
    uint32_t u = (word >> 29) & 1U;
    uint32_t size = (word >> 22) & 3U;
    uint32_t l_bit = (word >> 21) & 1U;
    uint32_t m_bit = (word >> 20) & 1U;
    uint32_t rm = (word >> 16) & 15U;
    uint32_t opc = (word >> 12) & 15U;
    uint32_t h_bit = (word >> 11) & 1U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rd = word & 31U;

    const uint8_t *vn = (const uint8_t *)&cpu->v[rn];
    const uint8_t *vd = (const uint8_t *)&cpu->v[rd];
    uint8_t out[16] = {0};

    if (!scalar && opc == 14U) {
        uint32_t vrm = (m_bit << 4) | rm;
        const uint8_t *vm = (const uint8_t *)&cpu->v[vrm];
        uint32_t idx = (h_bit << 1) | l_bit;
        uint32_t elems = q ? 4U : 2U;
        for (uint32_t i = 0; i < elems; ++i) {
            uint32_t acc = (uint32_t)cpu_lane_get_u(vd, i, 4U);
            for (uint32_t k = 0; k < 4U; ++k) {
                if (u) acc += (uint32_t)vn[i * 4U + k] * (uint32_t)vm[idx * 4U + k];
                else acc += (uint32_t)((int32_t)(int8_t)vn[i * 4U + k] * (int32_t)(int8_t)vm[idx * 4U + k]);
            }
            cpu_lane_set(out, i, 4U, acc);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    if (!scalar && u == 1U && (opc & 9U) == 1U) {
        uint32_t rot = (opc >> 1) & 3U;
        uint32_t fsz = size;
        uint32_t eb = 1U << fsz;
        uint32_t vrm = (m_bit << 4) | rm;
        const uint8_t *vm = (const uint8_t *)&cpu->v[vrm];
        uint32_t idx = (fsz == 1U) ? ((h_bit << 1) | l_bit) : h_bit;
        uint64_t am0 = cpu_lane_get_u(vm, idx * 2U, eb);
        uint64_t am1 = cpu_lane_get_u(vm, idx * 2U + 1U, eb);
        uint32_t pairs = (q ? 16U : 8U) / (eb * 2U);
        for (uint32_t i = 0; i < pairs; ++i) {
            uint64_t r0 = 0, r1 = 0;
            exec_fcmla_pair(fsz, rot,
                            cpu_lane_get_u(vn, i * 2U, eb), cpu_lane_get_u(vn, i * 2U + 1U, eb),
                            am0, am1,
                            cpu_lane_get_u(vd, i * 2U, eb), cpu_lane_get_u(vd, i * 2U + 1U, eb),
                            &r0, &r1);
            cpu_lane_set(out, i * 2U, eb, r0);
            cpu_lane_set(out, i * 2U + 1U, eb, r1);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    if (!scalar && size == 2U &&
        ((u == 0U && (opc == 0U || opc == 4U)) || (u == 1U && (opc == 8U || opc == 12U)))) {
        bool part = (u == 1U);
        bool sub = (opc == 4U || opc == 12U);
        uint32_t vrm = rm;
        uint32_t idx = (h_bit << 2) | (l_bit << 1) | m_bit;
        const uint8_t *vm = (const uint8_t *)&cpu->v[vrm];
        uint16_t bh = (uint16_t)cpu_lane_get_u(vm, idx, 2U);
        float b32 = fhm_f16_to_f32(cpu->fpcr, bh);
        uint32_t cnt = q ? 4U : 2U;
        uint32_t s_off = part ? (cnt * 2U) : 0U;
        for (uint32_t i = 0; i < cnt; ++i) {
            uint16_t ah = 0;
            memcpy(&ah, vn + s_off + i * 2U, 2);
            float a32 = fhm_f16_to_f32(cpu->fpcr, ah);
            float d32 = 0, r32 = 0;
            memcpy(&d32, vd + i * 4U, 4);
            if (sub) __asm__("fmsub %s0, %s1, %s2, %s3" : "=w"(r32) : "w"(a32), "w"(b32), "w"(d32));
            else __asm__("fmadd %s0, %s1, %s2, %s3" : "=w"(r32) : "w"(a32), "w"(b32), "w"(d32));
            memcpy(out + i * 4U, &r32, 4);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    bool is_fp = (opc == 1U || opc == 5U || opc == 9U);
    if (is_fp) {
        bool fp16 = (size == 0U);
        uint32_t fsz;
        uint32_t idx;
        uint32_t vrm;
        if (fp16) {
            fsz = 1U;
            vrm = rm;
            idx = (h_bit << 2) | (l_bit << 1) | m_bit;
        } else if ((size & 1U) == 0U) {
            fsz = 2U;
            vrm = (m_bit << 4) | rm;
            idx = (h_bit << 1) | l_bit;
        } else {
            fsz = 3U;
            vrm = (m_bit << 4) | rm;
            idx = h_bit;
        }
        const uint8_t *vm = (const uint8_t *)&cpu->v[vrm];
        uint32_t fb = 1U << fsz;
        uint32_t elems = scalar ? 1U : ((q ? 16U : 8U) / fb);
        uint64_t b = cpu_lane_get_u(vm, idx, fb);
        for (uint32_t i = 0; i < elems; ++i) {
            uint64_t a = cpu_lane_get_u(vn, i, fb);
            uint64_t d = cpu_lane_get_u(vd, i, fb);
            uint64_t r = 0;
            if (opc == 1U) r = fp_lane_fma(false, fsz, a, b, d);
            else if (opc == 5U) r = fp_lane_fma(true, fsz, a, b, d);
            else r = fp_lane_binop(u ? 10U : 0U, fsz, a, b);
            cpu_lane_set(out, i, fb, r);
        }
        memcpy(&cpu->v[rd], out, 16);
        return;
    }

    uint32_t ebytes = 1U << size;
    uint32_t bits = ebytes * 8U;
    uint32_t vrm = (size == 1U) ? rm : ((m_bit << 4) | rm);
    uint32_t idx = (size == 1U) ? ((h_bit << 2) | (l_bit << 1) | m_bit) : ((h_bit << 1) | l_bit);
    const uint8_t *vm = (const uint8_t *)&cpu->v[vrm];
    uint64_t bu = cpu_lane_get_u(vm, idx, ebytes);
    int64_t bs = cpu_sext(bu, bits);

    bool widening = (opc == 2U || opc == 3U || opc == 6U || opc == 7U || opc == 10U || opc == 11U);
    if (widening) {
        uint32_t db = ebytes * 2U;
        uint32_t dbits = db * 8U;
        uint32_t s_off = (!scalar && q) ? 8U : 0U;
        uint32_t cnt = scalar ? 1U : (16U / db);
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t au = cpu_lane_get_u(vn + s_off, i, ebytes);
            int64_t as = cpu_sext(au, bits);
            uint64_t du = cpu_lane_get_u(vd, i, db);
            int64_t ds = cpu_sext(du, dbits);
            uint64_t r = 0;
            if (opc == 2U) r = du + (u ? (au * bu) : (uint64_t)(as * bs));
            else if (opc == 3U) {
                int64_t prod = (int64_t)sat_signed(cpu, (__int128)as * bs * 2, dbits);
                r = sat_signed(cpu, (__int128)ds + (__int128)prod, dbits);
            } else if (opc == 6U) r = du - (u ? (au * bu) : (uint64_t)(as * bs));
            else if (opc == 7U) {
                int64_t prod = (int64_t)sat_signed(cpu, (__int128)as * bs * 2, dbits);
                r = sat_signed(cpu, (__int128)ds - (__int128)prod, dbits);
            } else if (opc == 10U) r = u ? (au * bu) : (uint64_t)(as * bs);
            else r = sat_signed(cpu, (__int128)as * bs * 2, dbits);
            cpu_lane_set(out, i, db, r);
        }
    } else {
        uint32_t elems = scalar ? 1U : ((q ? 16U : 8U) / ebytes);
        for (uint32_t i = 0; i < elems; ++i) {
            uint64_t au = cpu_lane_get_u(vn, i, ebytes);
            int64_t as = cpu_sext(au, bits);
            uint64_t du = cpu_lane_get_u(vd, i, ebytes);
            int64_t ds = cpu_sext(du, bits);
            uint64_t r = 0;
            if (opc == 0U) r = du + au * bu;
            else if (opc == 4U) r = du - au * bu;
            else if (opc == 8U) r = au * bu;
            else if (u == 0U && (opc == 12U || opc == 13U)) {
                __int128 prod = (__int128)as * bs * 2 + (opc == 13U ? ((__int128)1 << (bits - 1U)) : 0);
                r = sat_signed(cpu, prod >> bits, bits);
            } else {
                __int128 prod = (__int128)as * bs * 2;
                __int128 acc = ((__int128)ds << bits) + (opc == 15U ? -prod : prod) + ((__int128)1 << (bits - 1U));
                r = sat_signed(cpu, acc >> bits, bits);
            }
            cpu_lane_set(out, i, ebytes, r);
        }
    }
    memcpy(&cpu->v[rd], out, 16);
}

void cpu_op_simd_dp(vcpu_t *cpu, const ir_insn_t *ir,
                    softtlb_entry_t *tlb_r, softtlb_entry_t *tlb_w) {
    if (!cpu_fp_enabled(cpu)) {
        cpu->pc = cpu_ir_pc(cpu, ir);
        vcpu_raise_fp_trap(cpu, cpu->pc);
        return;
    }
    cpu_fp_ensure_guest(cpu);
    uint32_t word = (uint32_t)ir->target_or_imm;
    uint32_t rd = word & 31U;
    uint32_t rn = (word >> 5) & 31U;
    uint32_t rm = (word >> 16) & 31U;

    switch (ir->op) {
    case SUBOP_SIMD_IMM: {
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t op = (word >> 29) & 1U;
        uint32_t cmode = (word >> 12) & 15U;
        uint32_t imm8 = (((word >> 16) & 7U) << 5) | ((word >> 5) & 0x1FU);
        uint64_t val64 = 0;
        uint32_t c3 = cmode >> 1;
        if (c3 <= 3U) {
            uint32_t v32 = imm8 << (c3 * 8U);
            val64 = ((uint64_t)v32 << 32) | v32;
        } else if (c3 == 4U || c3 == 5U) {
            uint16_t v16 = (uint16_t)(imm8 << ((c3 - 4U) * 8U));
            val64 = (uint64_t)v16 | ((uint64_t)v16 << 16) | ((uint64_t)v16 << 32) | ((uint64_t)v16 << 48);
        } else if (c3 == 6U) {
            uint32_t v32 = (cmode & 1U) ? ((imm8 << 16) | 0xFFFFU) : ((imm8 << 8) | 0xFFU);
            val64 = ((uint64_t)v32 << 32) | v32;
        } else {
            if ((cmode & 1U) == 0U) {
                if (op == 0U) {
                    val64 = (uint64_t)imm8 * 0x0101010101010101ULL;
                } else {
                    for (uint32_t b = 0; b < 8U; ++b) {
                        if ((imm8 >> b) & 1U) val64 |= (0xFFULL << (b * 8U));
                    }
                }
            } else {
                if (op == 0U) {
                    if ((word >> 11) & 1U) {
                        uint16_t v16 = vfp_expand_imm16(imm8);
                        val64 = (uint64_t)v16 | ((uint64_t)v16 << 16) | ((uint64_t)v16 << 32) | ((uint64_t)v16 << 48);
                    } else {
                        uint32_t v32 = vfp_expand_imm32(imm8);
                        val64 = ((uint64_t)v32 << 32) | v32;
                    }
                } else {
                    val64 = vfp_expand_imm64(imm8);
                }
            }
        }
        bool is_orr = (c3 <= 5U && (cmode & 1U) == 1U && op == 0U);
        bool is_bic = (c3 <= 5U && (cmode & 1U) == 1U && op == 1U);
        bool is_mvni = ((c3 <= 5U && (cmode & 1U) == 0U && op == 1U) || (c3 == 6U && op == 1U));
        uint64_t lo = 0, hi = 0;
        memcpy(&lo, &cpu->v[rd], 8);
        memcpy(&hi, (const uint8_t *)&cpu->v[rd] + 8, 8);
        if (is_orr) { lo |= val64; hi = q ? (hi | val64) : 0ULL; }
        else if (is_bic) { lo &= ~val64; hi = q ? (hi & ~val64) : 0ULL; }
        else if (is_mvni) { lo = ~val64; hi = q ? ~val64 : 0ULL; }
        else { lo = val64; hi = q ? val64 : 0ULL; }
        cpu->v[rd] = (__uint128_t)lo | ((__uint128_t)hi << 64);
        break;
    }
    case SUBOP_SIMD_INS: {
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t op = (word >> 29) & 1U;
        uint32_t imm5 = (word >> 16) & 0x1FU;
        uint32_t imm4 = (word >> 11) & 0xFU;
        uint32_t tz = (uint32_t)__builtin_ctz(imm5 | 16U);
        bool reserved = tz > 3U;
        if (!reserved && op == 0U) {
            switch (imm4) {
            case 0U:
            case 1U: reserved = tz == 3U && !q; break;
            case 3U: break;
            case 5U: reserved = tz == 3U || (tz == 2U && !q); break;
            case 7U: reserved = q != (tz == 3U); break;
            default: reserved = true; break;
            }
        }
        if (reserved) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        uint32_t ebytes = 1U << tz;
        uint32_t dst_idx = imm5 >> (tz + 1U);
        if (op == 1U) {
            uint64_t elem = cpu_lane_get_u((const uint8_t *)&cpu->v[rn], imm4 >> tz, ebytes);
            cpu_lane_set((uint8_t *)&cpu->v[rd], dst_idx, ebytes, elem);
        } else if (imm4 == 0U || imm4 == 1U) {
            uint64_t elem = imm4 == 0U ? cpu_lane_get_u((const uint8_t *)&cpu->v[rn], dst_idx, ebytes)
                                       : cpu_get_x(cpu, rn);
            uint64_t rep = cpu_lane_replicate(elem, ebytes);
            cpu->v[rd] = q ? ((__uint128_t)rep << 64) | rep : rep;
        } else if (imm4 == 3U) {
            cpu_lane_set((uint8_t *)&cpu->v[rd], dst_idx, ebytes, cpu_get_x(cpu, rn));
        } else if (imm4 == 5U) {
            int64_t sv = cpu_lane_get_s((const uint8_t *)&cpu->v[rn], dst_idx, ebytes);
            cpu_set_x(cpu, rd, q ? (uint64_t)sv : (uint64_t)(uint32_t)sv);
        } else if (imm4 == 7U) {
            uint64_t uv = cpu_lane_get_u((const uint8_t *)&cpu->v[rn], dst_idx, ebytes);
            cpu_set_x(cpu, rd, uv);
        }
        break;
    }
    case SUBOP_SIMD_ONE: {
        uint32_t imm5 = (word >> 16) & 0x1FU;
        uint32_t tz = (uint32_t)__builtin_ctz(imm5 | 16U);
        if (tz > 3U) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        uint32_t ebytes = 1U << tz;
        uint32_t idx = imm5 >> (tz + 1U);
        cpu->v[rd] = cpu_lane_get_u((const uint8_t *)&cpu->v[rn], idx, ebytes);
        break;
    }
    case SUBOP_SIMD_EXT: {
        static const uint8_t iota[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t imm4 = (word >> 11) & 15U;
        if (!q && (imm4 & 8U) != 0U) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        uint8x16_t n = vld1q_u8((const uint8_t *)&cpu->v[rn]);
        uint8x16_t m = vld1q_u8((const uint8_t *)&cpu->v[rm]);
        uint8x16_t idx = vaddq_u8(vld1q_u8(iota), vdupq_n_u8((uint8_t)imm4));
        uint8x16_t r = q ? vqtbl2q_u8((uint8x16x2_t){{n, m}}, idx)
                         : vqtbl1q_u8(vcombine_u8(vget_low_u8(n), vget_low_u8(m)), idx);
        simd_store(cpu, rd, r, q);
        break;
    }
    case SUBOP_SIMD_PERM: {
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t size = (word >> 22) & 3U;
        uint32_t opc = (word >> 12) & 7U;
        if (size == 3U && !q) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        uint32_t ebytes = 1U << size;
        uint32_t elems = (q ? 16U : 8U) >> size;
        uint32_t half = elems / 2U;
        uint32_t part = (opc == 5U || opc == 6U || opc == 7U) ? 1U : 0U;
        uint8_t index[16] = {0};
        for (uint32_t i = 0; i < elems; ++i) {
            uint32_t reg, elem;
            switch (opc & 3U) {
            case 1U: reg = i >= half ? 1U : 0U; elem = 2U * (i - reg * half) + part; break;
            case 2U: reg = i & 1U; elem = (i & ~1U) + part; break;
            default: reg = i & 1U; elem = part * half + (i >> 1); break;
            }
            for (uint32_t b = 0; b < ebytes; ++b) {
                index[i * ebytes + b] = (uint8_t)(reg * 16U + elem * ebytes + b);
            }
        }
        uint8x16x2_t table = {{vld1q_u8((const uint8_t *)&cpu->v[rn]), vld1q_u8((const uint8_t *)&cpu->v[rm])}};
        simd_store(cpu, rd, vqtbl2q_u8(table, vld1q_u8(index)), q);
        break;
    }
    case SUBOP_SIMD_TBL: {
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t len = ((word >> 13) & 3U) + 1U;
        bool is_tbx = ((word >> 12) & 1U) != 0;
        uint8x16x4_t t = {0};
        for (uint32_t i = 0; i < len; ++i) {
            t.val[i] = vld1q_u8((const uint8_t *)&cpu->v[(rn + i) & 31U]);
        }
        uint8x16_t idx = vld1q_u8((const uint8_t *)&cpu->v[rm]);
        uint8x16_t d = vld1q_u8((const uint8_t *)&cpu->v[rd]);
        uint8x16_t r;
        switch (len) {
        case 1U:
            r = is_tbx ? vqtbx1q_u8(d, t.val[0], idx) : vqtbl1q_u8(t.val[0], idx);
            break;
        case 2U: {
            uint8x16x2_t t2 = {{t.val[0], t.val[1]}};
            r = is_tbx ? vqtbx2q_u8(d, t2, idx) : vqtbl2q_u8(t2, idx);
            break;
        }
        case 3U: {
            uint8x16x3_t t3 = {{t.val[0], t.val[1], t.val[2]}};
            r = is_tbx ? vqtbx3q_u8(d, t3, idx) : vqtbl3q_u8(t3, idx);
            break;
        }
        default:
            r = is_tbx ? vqtbx4q_u8(d, t, idx) : vqtbl4q_u8(t, idx);
            break;
        }
        simd_store(cpu, rd, r, q);
        break;
    }
    case SUBOP_SIMD_SAME: exec_simd_same(cpu, word, false, false); break;
    case SUBOP_SIMD_SAME_FP16: exec_simd_same(cpu, word, false, true); break;
    case SUBOP_SISD_SAME: exec_simd_same(cpu, word, true, false); break;
    case SUBOP_SISD_SAME_FP16: exec_simd_same(cpu, word, true, true); break;
    case SUBOP_SIMD_SAME2: exec_simd_same2(cpu, word, false); break;
    case SUBOP_SISD_SAME2: exec_simd_same2(cpu, word, true); break;
    case SUBOP_SIMD_MISC: exec_simd_misc(cpu, word, false, false); break;
    case SUBOP_SIMD_MISC_FP16: exec_simd_misc(cpu, word, false, true); break;
    case SUBOP_SISD_MISC: exec_simd_misc(cpu, word, true, false); break;
    case SUBOP_SISD_MISC_FP16: exec_simd_misc(cpu, word, true, true); break;
    case SUBOP_SIMD_SHF:
    case SUBOP_SISD_SHF:
        if (!exec_simd_shf(cpu, word, ir->op == SUBOP_SISD_SHF)) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        break;
    case SUBOP_SIMD_DIFF:
    case SUBOP_SISD_DIFF:
        if (!exec_simd_diff(cpu, word, ir->op == SUBOP_SISD_DIFF)) {
            vcpu_raise_exception(cpu, 1U << 25, 0, cpu_ir_pc(cpu, ir));
            return;
        }
        break;
    case SUBOP_SIMD_ELEM: exec_simd_elem(cpu, word, false); break;
    case SUBOP_SISD_ELEM: exec_simd_elem(cpu, word, true); break;
    case SUBOP_SIMD_ALL: {
        bool q = ((word >> 30) & 1U) != 0;
        uint32_t u = (word >> 29) & 1U;
        uint32_t size = (word >> 22) & 3U;
        uint32_t opc = (word >> 12) & 0x1FU;
        uint8_t vn[16];
        memcpy(vn, &cpu->v[rn], 16);
        uint32_t bytes = q ? 16U : 8U;
        cpu->v[rd] = 0;
        if (opc == 12U || opc == 15U) {
            uint32_t fsz = u ? 2U : 1U;
            uint32_t fb = 1U << fsz;
            bool is_min = ((size >> 1) & 1U) != 0;
            uint32_t fop = (opc == 12U) ? (is_min ? 7U : 6U) : (is_min ? 5U : 4U);
            uint64_t acc = hw_fp_across(fop, fsz, q, vn);
            cpu_lane_set((uint8_t *)&cpu->v[rd], 0, fb, acc);
        } else {
            uint32_t ebytes = 1U << size;
            uint32_t bits = ebytes * 8U;
            uint32_t cnt = bytes / ebytes;
            if (opc == 3U) {
                uint64_t sum = 0;
                for (uint32_t i = 0; i < cnt; ++i) {
                    sum += u ? cpu_lane_get_u(vn, i, ebytes) : (uint64_t)cpu_lane_get_s(vn, i, ebytes);
                }
                cpu_lane_set((uint8_t *)&cpu->v[rd], 0, ebytes * 2U, sum);
            } else if (opc == 27U) {
                uint64_t sum = 0;
                for (uint32_t i = 0; i < cnt; ++i) sum += cpu_lane_get_u(vn, i, ebytes);
                cpu_lane_set((uint8_t *)&cpu->v[rd], 0, ebytes, sum);
            } else {
                uint64_t acc = cpu_lane_get_u(vn, 0, ebytes);
                for (uint32_t i = 1U; i < cnt; ++i) {
                    uint64_t v = cpu_lane_get_u(vn, i, ebytes);
                    if (opc == 10U) {
                        acc = u ? (v > acc ? v : acc) : (uint64_t)(cpu_sext(v, bits) > cpu_sext(acc, bits) ? cpu_sext(v, bits) : cpu_sext(acc, bits));
                    } else {
                        acc = u ? (v < acc ? v : acc) : (uint64_t)(cpu_sext(v, bits) < cpu_sext(acc, bits) ? cpu_sext(v, bits) : cpu_sext(acc, bits));
                    }
                }
                cpu_lane_set((uint8_t *)&cpu->v[rd], 0, ebytes, acc);
            }
        }
        break;
    }
    case SUBOP_SISD_PAIR: {
        uint32_t u = (word >> 29) & 1U;
        uint32_t size = (word >> 22) & 3U;
        uint32_t opc = (word >> 12) & 0x1FU;
        uint8_t vn[16];
        memcpy(vn, &cpu->v[rn], 16);
        cpu->v[rd] = 0;
        if (opc == 27U) {
            uint64_t r = cpu_lane_get_u(vn, 0, 8U) + cpu_lane_get_u(vn, 1, 8U);
            memcpy(&cpu->v[rd], &r, 8);
        } else {
            uint32_t fsz = u ? ((size & 1U) ? 3U : 2U) : 1U;
            bool is_min = ((size >> 1) & 1U) != 0;
            uint32_t fb = 1U << fsz;
            uint64_t a = cpu_lane_get_u(vn, 0, fb), b = cpu_lane_get_u(vn, 1, fb);
            uint32_t fop = opc == 13U ? 2U : opc == 12U ? (is_min ? 7U : 6U) : (is_min ? 5U : 4U);
            uint64_t r = fp_lane_binop(fop, fsz, a, b);
            cpu_lane_set((uint8_t *)&cpu->v[rd], 0, fb, r);
        }
        break;
    }
    }
    CPU_NEXT(cpu, ir + 1, tlb_r, tlb_w);
}
