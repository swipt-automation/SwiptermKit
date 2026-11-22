#include "state.h"

.text
.p2align 4
.globl _native_template
.globl _native_template_slots
.globl _native_template_end

_native_template:
    stp x29, x30, [sp, #-176]!
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]
    stp d8, d9, [sp, #96]
    stp d10, d11, [sp, #112]
    stp d12, d13, [sp, #128]
    stp d14, d15, [sp, #144]
    mrs x9, fpcr
    mrs x10, fpsr
    stp x9, x10, [sp, #160]
    mov x27, x0
    clrex
    ldr x9, [x27, #STATE_NZCV]
    msr nzcv, x9
    ldr x9, [x27, #STATE_FPCR]
    msr fpcr, x9
    ldr x9, [x27, #STATE_FPSR]
    msr fpsr, x9
    ldp q0, q1, [x27, #(STATE_V + 16 * 0)]
    ldp q2, q3, [x27, #(STATE_V + 16 * 2)]
    ldp q4, q5, [x27, #(STATE_V + 16 * 4)]
    ldp q6, q7, [x27, #(STATE_V + 16 * 6)]
    ldp q8, q9, [x27, #(STATE_V + 16 * 8)]
    ldp q10, q11, [x27, #(STATE_V + 16 * 10)]
    ldp q12, q13, [x27, #(STATE_V + 16 * 12)]
    ldp q14, q15, [x27, #(STATE_V + 16 * 14)]
    ldp q16, q17, [x27, #(STATE_V + 16 * 16)]
    ldp q18, q19, [x27, #(STATE_V + 16 * 18)]
    ldp q20, q21, [x27, #(STATE_V + 16 * 20)]
    ldp q22, q23, [x27, #(STATE_V + 16 * 22)]
    ldp q24, q25, [x27, #(STATE_V + 16 * 24)]
    ldp q26, q27, [x27, #(STATE_V + 16 * 26)]
    ldp q28, q29, [x27, #(STATE_V + 16 * 28)]
    ldp q30, q31, [x27, #(STATE_V + 16 * 30)]
    ldr x9, [x27, #STATE_SP]
    mov x26, sp
    mov sp, x9
    ldp x0, x1, [x27, #(STATE_X + 8 * 0)]
    ldp x2, x3, [x27, #(STATE_X + 8 * 2)]
    ldp x4, x5, [x27, #(STATE_X + 8 * 4)]
    ldp x6, x7, [x27, #(STATE_X + 8 * 6)]
    ldp x8, x9, [x27, #(STATE_X + 8 * 8)]
    ldp x10, x11, [x27, #(STATE_X + 8 * 10)]
    ldp x12, x13, [x27, #(STATE_X + 8 * 12)]
    ldp x14, x15, [x27, #(STATE_X + 8 * 14)]
    ldp x16, x17, [x27, #(STATE_X + 8 * 16)]
    ldp x20, x21, [x27, #(STATE_X + 8 * 20)]
    ldp x22, x23, [x27, #(STATE_X + 8 * 22)]
    ldp x24, x25, [x27, #(STATE_X + 8 * 24)]
    ldp x28, x29, [x27, #(STATE_X + 8 * 28)]
    ldr x19, [x27, #(STATE_X + 8 * 19)]
    ldr x30, [x27, #(STATE_X + 8 * 30)]
_native_template_slots:
.rept NATIVE_SLOTS
    nop
.endr
    stp x0, x1, [x27, #(STATE_X + 8 * 0)]
    stp x2, x3, [x27, #(STATE_X + 8 * 2)]
    stp x4, x5, [x27, #(STATE_X + 8 * 4)]
    stp x6, x7, [x27, #(STATE_X + 8 * 6)]
    stp x8, x9, [x27, #(STATE_X + 8 * 8)]
    stp x10, x11, [x27, #(STATE_X + 8 * 10)]
    stp x12, x13, [x27, #(STATE_X + 8 * 12)]
    stp x14, x15, [x27, #(STATE_X + 8 * 14)]
    stp x16, x17, [x27, #(STATE_X + 8 * 16)]
    stp x20, x21, [x27, #(STATE_X + 8 * 20)]
    stp x22, x23, [x27, #(STATE_X + 8 * 22)]
    stp x24, x25, [x27, #(STATE_X + 8 * 24)]
    stp x28, x29, [x27, #(STATE_X + 8 * 28)]
    str x19, [x27, #(STATE_X + 8 * 19)]
    str x30, [x27, #(STATE_X + 8 * 30)]
    mov x9, sp
    str x9, [x27, #STATE_SP]
    mov sp, x26
    mrs x9, nzcv
    str x9, [x27, #STATE_NZCV]
    mrs x9, fpsr
    str x9, [x27, #STATE_FPSR]
    stp q0, q1, [x27, #(STATE_V + 16 * 0)]
    stp q2, q3, [x27, #(STATE_V + 16 * 2)]
    stp q4, q5, [x27, #(STATE_V + 16 * 4)]
    stp q6, q7, [x27, #(STATE_V + 16 * 6)]
    stp q8, q9, [x27, #(STATE_V + 16 * 8)]
    stp q10, q11, [x27, #(STATE_V + 16 * 10)]
    stp q12, q13, [x27, #(STATE_V + 16 * 12)]
    stp q14, q15, [x27, #(STATE_V + 16 * 14)]
    stp q16, q17, [x27, #(STATE_V + 16 * 16)]
    stp q18, q19, [x27, #(STATE_V + 16 * 18)]
    stp q20, q21, [x27, #(STATE_V + 16 * 20)]
    stp q22, q23, [x27, #(STATE_V + 16 * 22)]
    stp q24, q25, [x27, #(STATE_V + 16 * 24)]
    stp q26, q27, [x27, #(STATE_V + 16 * 26)]
    stp q28, q29, [x27, #(STATE_V + 16 * 28)]
    stp q30, q31, [x27, #(STATE_V + 16 * 30)]
    ldp x9, x10, [sp, #160]
    msr fpcr, x9
    msr fpsr, x10
    ldp d14, d15, [sp, #144]
    ldp d12, d13, [sp, #128]
    ldp d10, d11, [sp, #112]
    ldp d8, d9, [sp, #96]
    ldp x27, x28, [sp, #80]
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #176
    ret
_native_template_end:
