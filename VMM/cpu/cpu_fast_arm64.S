#include "cpu_fast_layout.h"

.arch_extension crc
.arch_extension lse

.text
.p2align 4
.globl _cpu_fast_run
.private_extern _cpu_fast_run
_cpu_fast_run:
    .cfi_startproc
    stp x29, x30, [sp, #-160]!
    .cfi_def_cfa_offset 160
    .cfi_offset x29, -160
    .cfi_offset x30, -152
    mov x29, sp
    .cfi_def_cfa x29, 160
    stp x19, x20, [sp, #16]
    .cfi_offset x19, -144
    .cfi_offset x20, -136
    stp x21, x22, [sp, #32]
    .cfi_offset x21, -128
    .cfi_offset x22, -120
    stp x23, x24, [sp, #48]
    .cfi_offset x23, -112
    .cfi_offset x24, -104
    stp x25, x26, [sp, #64]
    .cfi_offset x25, -96
    .cfi_offset x26, -88
    stp x27, x28, [sp, #80]
    .cfi_offset x27, -80
    .cfi_offset x28, -72
    stp d8, d9, [sp, #96]
    .cfi_offset d8, -64
    .cfi_offset d9, -56
    stp d10, d11, [sp, #112]
    .cfi_offset d10, -48
    .cfi_offset d11, -40
    stp d12, d13, [sp, #128]
    .cfi_offset d12, -32
    .cfi_offset d13, -24
    stp d14, d15, [sp, #144]
    .cfi_offset d14, -16
    .cfi_offset d15, -8
    mov x19, x0
    adr x24, Lcpu_fast_base
    b Lresolve

.macro DISPATCH
#ifdef CPU_STATS
    bl Lstat_dispatch
#endif
    ldr w16, [x20, #IR_OFFSET]
    add x16, x24, w16, uxtw
    br x16
.endmacro

.macro NEXT n=1
    add x20, x20, #(IR_SIZE * \n)
    DISPATCH
.endmacro

.macro LAND
    ldrb w9, [x20, #IR_HEAT]
    cbnz w9, Lheat
    DISPATCH
.endmacro

.macro ASYNC t0, t1, t2
    ldr x\t0, [x19, #CPU_IRQ]
    tbnz x\t0, #((CPU_STOP - CPU_IRQ) * 8), Lcross_slow
    tbnz x\t0, #((CPU_SHOOTDOWN - CPU_IRQ) * 8), Lcross_slow
    cbz w\t0, 1f
    ldrh w\t1, [x19, #CPU_IRQ_MASK]
    ldr w\t2, [x19, #CPU_DAIF]
    ubfx w\t2, w\t2, #7, #1
    lsl w\t2, w\t2, #3
    lsr w\t1, w\t1, w\t2
    and w\t1, w\t1, w\t0
    cbnz w\t1, Lcross_slow
1:
.endmacro

.macro READ_X dst, field, index=w9
    ldrb \index, [x20, #\field]
    ldr \dst, [x19, \index, uxtw #3]
.endmacro

.macro WRITE_X src, field=IR_RD, index=w9
    ldrb \index, [x20, #\field]
    str \src, [x19, \index, uxtw #3]
.endmacro

.macro PAGE_CURRENT fail
    ldr x9, [x19, #CPU_ICACHE]
    ldr x10, [x9, #IC_CUR_EXEC_EPOCH]
    ldr x11, [x19, #CPU_EXEC_EPOCH]
    eor x10, x10, x11
    cbnz x10, \fail
    ldr x10, [x9, #IC_CUR_GEN]
    ldur w11, [x25, #(PAGE_GEN - PAGE_INSNS)]
    eor w10, w10, w11
    cbnz w10, \fail
.endmacro

.macro COMMIT
    mrs x9, nzcv
    str w9, [x19, #CPU_NZCV]
    ldrb w9, [x19, #CPU_EL]
    ldrb w10, [x19, #CPU_SPSEL]
    and w10, w10, w9
    add x11, x19, #CPU_SP
    ldr x12, [x19, #CPU_FAST_SP]
    str x12, [x11, w10, uxtw #3]
    ldr x9, [x19, #CPU_ICACHE]
    str w28, [x9, #IC_RAS_SP]
.endmacro

Lcpu_fast_base:
Lcold:
    COMMIT
    mov x0, x19
    mov x1, x20
    mov x2, x21
    mov x3, x22
    bl _cpu_fast_call
Lcold_return:
    cbz x0, Lresolve
    b Lreload

Ljit_helper:
    stp x29, x30, [sp, #-16]!
    bl _cpu_jit_helper
    ldp x29, x30, [sp], #16
    tbnz x0, #0, 1f
    ret
1:
    and x0, x0, #~1
    b Lcold_return

Lheat:
    sub w9, w9, #1
    strb w9, [x20, #IR_HEAT]
    cbz w9, Ljit_compile
    DISPATCH

Ljit_compile:
    COMMIT
    mov x0, x19
    mov x1, x20
    bl _cpu_jit_enter
    cmn w0, #1
    b.eq Lresolve
    ldr w9, [x19, #CPU_NZCV]
    msr nzcv, x9
    add x16, x24, w0, uxtw
    br x16

#ifdef CPU_STATS
Lstat_dispatch:
    ldr x16, [x19, #CPU_ICACHE]
    ldr x17, [x16, #IC_STAT_DISPATCHES]
    add x17, x17, #1
    str x17, [x16, #IC_STAT_DISPATCHES]
    ret
#endif

Lresolve:
    mov x0, x19
    bl _cpu_fast_resolve
    cbz x0, Lexit

Lreload:
    mov x20, x0
    ldrb w9, [x19, #CPU_EL]
    ldrb w10, [x19, #CPU_SPSEL]
    and w10, w10, w9
    add x11, x19, #CPU_SP
    ldr x11, [x11, w10, uxtw #3]
    str x11, [x19, #CPU_FAST_SP]

    ldr x21, [x19, #CPU_TLB]
    add x21, x21, x9, lsl #(TLB_BANK_SHIFT + 1)
    add x22, x21, #(1 << (TLB_BANK_SHIFT - 12)), lsl #12
    ldr x23, [x19, #CPU_SALT]
    and x9, x23, #0xffff000000000000
    and x27, x23, #0xffc
    orr x27, x27, x9

    ldr x9, [x19, #CPU_ICACHE]
    ldr w28, [x9, #IC_RAS_SP]
    ldr x25, [x9, #IC_CUR_PAGE]
    ldr x26, [x9, #IC_CUR_VA]
    add x25, x25, #PAGE_INSNS
    ldr w9, [x19, #CPU_NZCV]
    msr nzcv, x9
    LAND

Lpoll:
    COMMIT
    mov x0, x19
    mov x1, x20
    bl _cpu_fast_poll
    cbnz w0, Lresolve
    ldr w9, [x19, #CPU_NZCV]
    msr nzcv, x9
    LAND

Lcross_page:
    tbnz x13, #0, Lcross_slow
    tbnz x13, #1, Lcross_slow
    eor x9, x13, x26
    lsr x9, x9, #12
    cbz x9, Lsame_page

    lsr x9, x13, #12
    and x10, x13, #0x0000fffffffff000
    lsr x11, x13, #48
    cbnz x11, 2f
    eor w9, w9, w23
    orr x10, x10, x27
    b 3f
2:
    eor w11, w11, #0xffff
    cbnz w11, Lcross_slow
    and x11, x23, #(1 << SALT_MMU_OFF)
    orr x10, x10, #2
    orr x10, x10, x11
3:
    and w9, w9, #((1 << (EXEC_BANK_SHIFT - 5)) - 1)
    ldr x14, [x19, #CPU_EXEC_CACHE]
    ldrb w11, [x19, #CPU_EL]
    add x14, x14, x11, lsl #EXEC_BANK_SHIFT
    add x14, x14, x9, lsl #5
    ldp x11, x12, [x14]
    cbz x12, Lcross_slow
    eor x11, x11, x10
    cbnz x11, Lcross_slow
    ldp x10, x11, [x14, #16]
    ldr x9, [x19, #CPU_EXEC_EPOCH]
    eor x9, x9, x10
    cbnz x9, Lcross_slow
    ldr w9, [x12, #PAGE_GEN]
    eor w9, w9, w11
    cbnz w9, Lcross_slow
    ASYNC 9, 10, 16

    ldr x9, [x19, #CPU_ICACHE]
    ldr x10, [x19, #CPU_EXEC_EPOCH]
    and x26, x13, #0xfffffffffffff000
    stp x12, x10, [x9, #IC_CUR_PAGE]
    stp x26, x11, [x9, #IC_CUR_VA]
    ldrb w9, [x12, #PAGE_REF]
    cbnz w9, 1f
    mov w9, #1
    strb w9, [x12, #PAGE_REF]
1:
    add x25, x12, #PAGE_INSNS
    and x9, x13, #0xffc
    add x20, x25, x9, lsl #3
    LAND

Lsame_page:
    PAGE_CURRENT Lcross_slow
    ASYNC 9, 10, 11
    and x9, x13, #0xffc
    add x20, x25, x9, lsl #3
    LAND

Lcross_slow:
    str x13, [x19, #CPU_PC]
    COMMIT
    b Lresolve

Lpage_end:
    add x13, x26, #1, lsl #12
    b Lcross_page

Lnop:
    NEXT

Lmov_imm:
    ldr x14, [x20, #IR_IMM]
    WRITE_X x14
    NEXT

Ladr:
    ldr x14, [x20, #IR_IMM]
    add x14, x14, x26
    WRITE_X x14
    NEXT

Lmov_reg:
    READ_X x14, IR_RM
    WRITE_X x14
    NEXT

Lmov_reg32:
    READ_X x14, IR_RM
    mov w14, w14
    WRITE_X x14
    NEXT

.macro ALU name, op, width, source, shift=none
L\name:
    READ_X x10, IR_RN, w9
.ifc \source, imm
    ldr x12, [x20, #IR_IMM]
.else
    READ_X x12, IR_RM, w11
.endif
.ifnc \shift, none
    ldrb w11, [x20, #IR_RA]
    \shift \width\()12, \width\()12, \width\()11
.endif
    \op \width\()14, \width\()10, \width\()12
    WRITE_X x14
    NEXT
.endmacro

.macro ALU_WIDTH width, size
    ALU add_i\size, add, \width, imm
    ALU sub_i\size, sub, \width, imm
    ALU adds_i\size, adds, \width, imm
    ALU subs_i\size, subs, \width, imm
    ALU and_i\size, and, \width, imm
    ALU orr_i\size, orr, \width, imm
    ALU eor_i\size, eor, \width, imm
    ALU ands_i\size, ands, \width, imm
    ALU add_r\size, add, \width, reg
    ALU add_lsl\size, add, \width, reg, lsl
    ALU add_lsr\size, add, \width, reg, lsr
    ALU add_asr\size, add, \width, reg, asr
    ALU sub_r\size, sub, \width, reg
    ALU sub_lsl\size, sub, \width, reg, lsl
    ALU sub_lsr\size, sub, \width, reg, lsr
    ALU sub_asr\size, sub, \width, reg, asr
    ALU adds_r\size, adds, \width, reg
    ALU adds_lsl\size, adds, \width, reg, lsl
    ALU adds_lsr\size, adds, \width, reg, lsr
    ALU adds_asr\size, adds, \width, reg, asr
    ALU subs_r\size, subs, \width, reg
    ALU subs_lsl\size, subs, \width, reg, lsl
    ALU subs_lsr\size, subs, \width, reg, lsr
    ALU subs_asr\size, subs, \width, reg, asr
    ALU and_r\size, and, \width, reg
    ALU and_lsl\size, and, \width, reg, lsl
    ALU and_lsr\size, and, \width, reg, lsr
    ALU and_asr\size, and, \width, reg, asr
    ALU and_ror\size, and, \width, reg, ror
    ALU bic_r\size, bic, \width, reg
    ALU bic_lsl\size, bic, \width, reg, lsl
    ALU bic_lsr\size, bic, \width, reg, lsr
    ALU bic_asr\size, bic, \width, reg, asr
    ALU bic_ror\size, bic, \width, reg, ror
    ALU orr_r\size, orr, \width, reg
    ALU orr_lsl\size, orr, \width, reg, lsl
    ALU orr_lsr\size, orr, \width, reg, lsr
    ALU orr_asr\size, orr, \width, reg, asr
    ALU orr_ror\size, orr, \width, reg, ror
    ALU orn_r\size, orn, \width, reg
    ALU orn_lsl\size, orn, \width, reg, lsl
    ALU orn_lsr\size, orn, \width, reg, lsr
    ALU orn_asr\size, orn, \width, reg, asr
    ALU orn_ror\size, orn, \width, reg, ror
    ALU eor_r\size, eor, \width, reg
    ALU eor_lsl\size, eor, \width, reg, lsl
    ALU eor_lsr\size, eor, \width, reg, lsr
    ALU eor_asr\size, eor, \width, reg, asr
    ALU eor_ror\size, eor, \width, reg, ror
    ALU eon_r\size, eon, \width, reg
    ALU eon_lsl\size, eon, \width, reg, lsl
    ALU eon_lsr\size, eon, \width, reg, lsr
    ALU eon_asr\size, eon, \width, reg, asr
    ALU eon_ror\size, eon, \width, reg, ror
    ALU ands_r\size, ands, \width, reg
    ALU ands_lsl\size, ands, \width, reg, lsl
    ALU ands_lsr\size, ands, \width, reg, lsr
    ALU ands_asr\size, ands, \width, reg, asr
    ALU ands_ror\size, ands, \width, reg, ror
    ALU bics_r\size, bics, \width, reg
    ALU bics_lsl\size, bics, \width, reg, lsl
    ALU bics_lsr\size, bics, \width, reg, lsr
    ALU bics_asr\size, bics, \width, reg, asr
    ALU bics_ror\size, bics, \width, reg, ror
    ALU adc_r\size, adc, \width, reg
    ALU adcs_r\size, adcs, \width, reg
    ALU sbc_r\size, sbc, \width, reg
    ALU sbcs_r\size, sbcs, \width, reg
    ALU lsl_r\size, lsl, \width, reg
    ALU lsr_r\size, lsr, \width, reg
    ALU asr_r\size, asr, \width, reg
    ALU ror_r\size, ror, \width, reg
    ALU udiv_r\size, udiv, \width, reg
    ALU sdiv_r\size, sdiv, \width, reg
.endmacro

ALU_WIDTH w, 32
ALU_WIDTH x, 64

.macro SHIFT_IMM name, op, width
L\name:
    READ_X x10, IR_RN, w9
    ldrh w11, [x20, #IR_OP]
    \op \width\()14, \width\()10, \width\()11
    WRITE_X x14
    NEXT
.endmacro

SHIFT_IMM lsl_i32, lsl, w
SHIFT_IMM lsl_i64, lsl, x
SHIFT_IMM lsr_i32, lsr, w
SHIFT_IMM lsr_i64, lsr, x
SHIFT_IMM asr_i32, asr, w
SHIFT_IMM asr_i64, asr, x

.macro UBFX_MASK name, width
L\name:
    READ_X x10, IR_RN, w9
    ldrh w11, [x20, #IR_OP]
    ldr x12, [x20, #IR_IMM]
    lsr \width\()14, \width\()10, \width\()11
    and \width\()14, \width\()14, \width\()12
    WRITE_X x14
    NEXT
.endmacro

UBFX_MASK ubfx_m32, w
UBFX_MASK ubfx_m64, x

.macro BITFIELD name, width, shift, wrap
L\name:
    READ_X x10, IR_RN, w9
    ldrh w11, [x20, #IR_RM]
    lsr w12, w11, #8
    lsl \width\()10, \width\()10, \width\()11
.if \wrap
    \shift \width\()10, \width\()10, \width\()11
    lsl \width\()14, \width\()10, \width\()12
.else
    \shift \width\()14, \width\()10, \width\()12
.endif
    WRITE_X x14
    NEXT
.endmacro

BITFIELD ubfx32, w, lsr, 0
BITFIELD ubfx64, x, lsr, 0
BITFIELD sbfx32, w, asr, 0
BITFIELD sbfx64, x, asr, 0
BITFIELD ubfiz32, w, lsr, 1
BITFIELD ubfiz64, x, lsr, 1
BITFIELD sbfiz32, w, asr, 1
BITFIELD sbfiz64, x, asr, 1

.macro TLB bank, bytes, alignment, cold=Lcold
.if \bytes > 1
    add x9, x12, #(\bytes - 1)
    eor x9, x9, x12
    tbnz x9, #12, \cold
    tbz x23, #SALT_ALIGN, 1f
    and x9, x12, #(\alignment - 1)
    cbnz x9, \cold
1:
.endif
    lsr x11, x12, #48
    and x10, x12, #0x0000fffffffff000
    cbnz x11, 2f
    eor x9, x23, x12, lsr #12
    orr x10, x10, x27
    b 3f
2:
    eor w11, w11, #0xffff
    cbnz w11, \cold
    lsr x9, x12, #12
    and x11, x23, #(1 << SALT_MMU_OFF)
    orr x10, x10, x11
    orr x10, x10, #2
3:
    ubfiz x9, x9, #4, #(TLB_BANK_SHIFT - 4)
    add x9, \bank, x9
    ldp x9, x11, [x9]
    eor x9, x9, x10
    cbnz x9, \cold
    add x12, x12, x11
.endmacro

.macro BASE
    READ_X x10, IR_RN, w14
    tbz w14, #5, 1f
    and x9, x10, #15
    cbz x9, 1f
    adr x17, 1f
    b Lsp_align
1:
.endmacro

.macro ADDRESS mode, extension=imm
    BASE
.ifc \extension, imm
    ldr x13, [x20, #IR_IMM]
.else
    READ_X x13, IR_RM, w11
    ldrb w11, [x20, #IR_RA]
    .ifc \extension, uxtw
        mov w13, w13
    .endif
    .ifc \extension, sxtw
        sxtw x13, w13
    .endif
    lsl x13, x13, x11
.endif
.ifc \mode, post
    mov x12, x10
    add x13, x10, x13
.else
    add x12, x10, x13
    .ifc \mode, pre
        mov x13, x12
    .endif
.endif
.endmacro

Lsp_align:
    ldr x9, [x19, #CPU_SCTLR]
    ldrb w11, [x19, #CPU_EL]
    cbnz w11, 1f
    tbnz x9, #4, Lcold
    b 2f
1:
    tbnz x9, #3, Lcold
2:
    br x17

.macro WRITEBACK mode
.ifnc \mode, off
    str x13, [x19, w14, uxtw #3]
.endif
.endmacro

.macro MEM name, insn, width, bytes, dir, mode, extension
L\name:
    ADDRESS \mode, \extension
.ifc \dir, load
    TLB x21, \bytes, \bytes
    \insn \width\()15, [x12]
    WRITE_X x15
.else
    TLB x22, \bytes, \bytes
    READ_X x15, IR_RD
    \insn \width\()15, [x12]
.endif
    WRITEBACK \mode
    NEXT
.endmacro

.macro MEM_FORMS name, insn, width, bytes, dir
    MEM \name\()_off, \insn, \width, \bytes, \dir, off, imm
    MEM \name\()_pre, \insn, \width, \bytes, \dir, pre, imm
    MEM \name\()_post, \insn, \width, \bytes, \dir, post, imm
    MEM \name\()_uxtw, \insn, \width, \bytes, \dir, off, uxtw
    MEM \name\()_xreg, \insn, \width, \bytes, \dir, off, xreg
    MEM \name\()_sxtw, \insn, \width, \bytes, \dir, off, sxtw
.endmacro

MEM_FORMS str8, strb, w, 1, store
MEM_FORMS str16, strh, w, 2, store
MEM_FORMS str32, str, w, 4, store
MEM_FORMS str64, str, x, 8, store
MEM_FORMS ldr8, ldrb, w, 1, load
MEM_FORMS ldr16, ldrh, w, 2, load
MEM_FORMS ldr32, ldr, w, 4, load
MEM_FORMS ldr64, ldr, x, 8, load
MEM_FORMS ldrsb64, ldrsb, x, 1, load
MEM_FORMS ldrsh64, ldrsh, x, 2, load
MEM_FORMS ldrsw64, ldrsw, x, 4, load
MEM_FORMS ldrsb32, ldrsb, w, 1, load
MEM_FORMS ldrsh32, ldrsh, w, 2, load

.macro PAIR name, insn, width, bytes, dir, mode
L\name:
    ADDRESS \mode
.ifc \dir, load
    TLB x21, (\bytes * 2), \bytes
    \insn \width\()10, \width\()11, [x12]
    WRITEBACK \mode
    WRITE_X x10
    WRITE_X x11, IR_RM
.else
    TLB x22, (\bytes * 2), \bytes
    READ_X x10, IR_RD
    READ_X x11, IR_RM
    \insn \width\()10, \width\()11, [x12]
    WRITEBACK \mode
.endif
    NEXT
.endmacro

.macro PAIR_FORMS name, insn, width, bytes, dir
    PAIR \name\()_off, \insn, \width, \bytes, \dir, off
    PAIR \name\()_pre, \insn, \width, \bytes, \dir, pre
    PAIR \name\()_post, \insn, \width, \bytes, \dir, post
.endmacro

PAIR_FORMS stp32, stp, w, 4, store
PAIR_FORMS stp64, stp, x, 8, store
PAIR_FORMS ldp32, ldp, w, 4, load
PAIR_FORMS ldp64, ldp, x, 8, load
PAIR_FORMS ldpsw64, ldpsw, x, 4, load

.macro UNPRIV name, insn, width, bytes, dir
L\name:
    ADDRESS off
    and x16, x21, #~((SOFTTLB_BANKS << TLB_BANK_SHIFT) - 1)
.ifc \dir, load
    TLB x16, \bytes, \bytes
    \insn \width\()15, [x12]
    WRITE_X x15
.else
    add x16, x16, #(1 << (TLB_BANK_SHIFT - 12)), lsl #12
    TLB x16, \bytes, \bytes
    READ_X x15, IR_RD
    \insn \width\()15, [x12]
.endif
    NEXT
.endmacro

UNPRIV sttr8, strb, w, 1, store
UNPRIV sttr16, strh, w, 2, store
UNPRIV sttr32, str, w, 4, store
UNPRIV sttr64, str, x, 8, store
UNPRIV ldtr8, ldrb, w, 1, load
UNPRIV ldtr16, ldrh, w, 2, load
UNPRIV ldtr32, ldr, w, 4, load
UNPRIV ldtr64, ldr, x, 8, load
UNPRIV ldtrsb64, ldrsb, x, 1, load
UNPRIV ldtrsh64, ldrsh, x, 2, load
UNPRIV ldtrsw64, ldrsw, x, 4, load
UNPRIV ldtrsb32, ldrsb, w, 1, load
UNPRIV ldtrsh32, ldrsh, w, 2, load

.macro ATOMIC_BASE bank, bytes, cold, offset=0
    BASE
.if \offset
    ldrsw x13, [x20, #IR_IMM_HI]
    add x10, x10, x13
.endif
.if \bytes > 1
    and x9, x10, #(\bytes - 1)
    cbnz x9, \cold
.endif
    mov x12, x10
    TLB \bank, 1, 1, \cold
.endmacro

.macro WAKE_WAITERS
    ldr x10, [x19, #CPU_VM]
    add x11, x10, #VM_WFE_WAITERS
    ldar w11, [x11]
    cbnz w11, Lstlr_wake
.endmacro

.macro ORDERED name, insn, width, bytes, dir, offset=0, extend=, extended=x
L\name:
.ifc \dir, load
    ATOMIC_BASE x21, \bytes, Lcold, \offset
    \insn \width\()15, [x12]
.ifnb \extend
    \extend \extended\()15, w15
.endif
    WRITE_X x15
.else
    ATOMIC_BASE x22, \bytes, Lcold, \offset
    READ_X x15, IR_RD
    \insn \width\()15, [x12]
    WAKE_WAITERS
.endif
    NEXT
.endmacro

ORDERED ldar8, ldarb, w, 1, load
ORDERED ldar16, ldarh, w, 2, load
ORDERED ldar32, ldar, w, 4, load
ORDERED ldar64, ldar, x, 8, load
ORDERED stlr8, stlrb, w, 1, store
ORDERED stlr16, stlrh, w, 2, store
ORDERED stlr32, stlr, w, 4, store
ORDERED stlr64, stlr, x, 8, store
ORDERED ldapur8, ldarb, w, 1, load, 1
ORDERED ldapur16, ldarh, w, 2, load, 1
ORDERED ldapur32, ldar, w, 4, load, 1
ORDERED ldapur64, ldar, x, 8, load, 1
ORDERED ldapursb64, ldarb, w, 1, load, 1, sxtb, x
ORDERED ldapursh64, ldarh, w, 2, load, 1, sxth, x
ORDERED ldapursw64, ldar, w, 4, load, 1, sxtw, x
ORDERED ldapursb32, ldarb, w, 1, load, 1, sxtb, w
ORDERED ldapursh32, ldarh, w, 2, load, 1, sxth, w
ORDERED stlur8, stlrb, w, 1, store, 1
ORDERED stlur16, stlrh, w, 2, store, 1
ORDERED stlur32, stlr, w, 4, store, 1
ORDERED stlur64, stlr, x, 8, store, 1

Lcfinv:
    mrs x9, nzcv
    eor x9, x9, #(1 << 29)
    msr nzcv, x9
    NEXT

Lxaflag:
    mrs x9, nzcv
    ubfx w10, w9, #30, #1
    ubfx w11, w9, #29, #1
    orr w12, w10, w11
    eor w13, w12, #1
    lsl w9, w13, #31
    and w13, w10, w11
    orr w9, w9, w13, lsl #30
    orr w9, w9, w12, lsl #29
    bic w13, w10, w11
    orr w9, w9, w13, lsl #28
    msr nzcv, x9
    NEXT

Laxflag:
    mrs x9, nzcv
    ubfx w10, w9, #28, #1
    orr w9, w9, w10, lsl #30
    bic w9, w9, w10, lsl #29
    and w9, w9, #0x60000000
    msr nzcv, x9
    NEXT

.macro SETF name, msb
L\name:
    READ_X x10, IR_RN
    mrs x9, nzcv
    and w9, w9, #(1 << 29)
    ubfx w11, w10, #\msb, #1
    orr w9, w9, w11, lsl #31
    tst w10, #((2 << \msb) - 1)
    cset w11, eq
    orr w9, w9, w11, lsl #30
    eor w11, w10, w10, lsr #1
    ubfx w11, w11, #\msb, #1
    orr w9, w9, w11, lsl #28
    msr nzcv, x9
    NEXT
.endmacro

SETF setf8, 7
SETF setf16, 15

Lrmif:
    READ_X x10, IR_RN
    ldrb w11, [x20, #IR_RA]
    ror x10, x10, x11
    ldrb w12, [x20, #IR_RM]
    lsl w12, w12, #28
    lsl w10, w10, #28
    mrs x9, nzcv
    bic w9, w9, w12
    and w10, w10, w12
    orr w9, w9, w10
    msr nzcv, x9
    NEXT

.macro CSSC_CTZ name, r
L\name:
    READ_X x10, IR_RN
    rbit \r\()10, \r\()10
    clz \r\()10, \r\()10
    WRITE_X x10
    NEXT
.endmacro

.macro CSSC_CNT name, r, f
L\name:
    READ_X x10, IR_RN
    fmov \f\()0, \r\()10
    cnt v0.8b, v0.8b
    addv b0, v0.8b
    fmov w10, s0
    WRITE_X x10
    NEXT
.endmacro

.macro CSSC_ABS name, r, top
L\name:
    READ_X x10, IR_RN
    asr \r\()11, \r\()10, #\top
    eor \r\()10, \r\()10, \r\()11
    sub \r\()10, \r\()10, \r\()11
    WRITE_X x10
    NEXT
.endmacro

.macro CSSC_MINMAX name, r, cond, operand
L\name:
    READ_X x10, IR_RN
.ifc \operand, reg
    READ_X x11, IR_RM
.else
    ldrsw x11, [x20, #IR_IMM_HI]
.endif
    mrs x9, nzcv
    cmp \r\()10, \r\()11
    csel \r\()10, \r\()10, \r\()11, \cond
    msr nzcv, x9
    WRITE_X x10
    NEXT
.endmacro

CSSC_CTZ ctz32, w
CSSC_CTZ ctz64, x
CSSC_CNT cnt32, w, s
CSSC_CNT cnt64, x, d
CSSC_ABS abs32, w, 31
CSSC_ABS abs64, x, 63
.irp operand, reg, imm
CSSC_MINMAX smax_\operand\()32, w, gt, \operand
CSSC_MINMAX smax_\operand\()64, x, gt, \operand
CSSC_MINMAX umax_\operand\()32, w, hi, \operand
CSSC_MINMAX umax_\operand\()64, x, hi, \operand
CSSC_MINMAX smin_\operand\()32, w, lt, \operand
CSSC_MINMAX smin_\operand\()64, x, lt, \operand
CSSC_MINMAX umin_\operand\()32, w, lo, \operand
CSSC_MINMAX umin_\operand\()64, x, lo, \operand
.endr

Lstlr_wake:
    mrs x9, nzcv
    str w9, [x19, #CPU_NZCV]
    mov x0, x10
    mov x1, x12
    bl _vmm_vcpu_wfe_notify_write
    ldr w9, [x19, #CPU_NZCV]
    msr nzcv, x9
    NEXT

.macro LITERAL name, insn, width, bytes
L\name:
    ldr x12, [x20, #IR_IMM]
    add x12, x12, x26
    TLB x21, \bytes, \bytes
    \insn \width\()15, [x12]
    WRITE_X x15
    NEXT
.endmacro

LITERAL ldr32_lit, ldr, w, 4
LITERAL ldr64_lit, ldr, x, 8
LITERAL ldrsw_lit, ldrsw, x, 4

.macro SYSREG_READ name, offset
L\name:
    ldr x14, [x19, #\offset]
    WRITE_X x14
    NEXT
.endmacro

.macro SYSREG_WRITE name, offset
L\name:
    READ_X x14, IR_RD
    str x14, [x19, #\offset]
    NEXT
.endmacro

SYSREG_READ mrs_tpidr_el0, CPU_TPIDR_EL0
SYSREG_READ mrs_tpidrro_el0, CPU_TPIDRRO_EL0
SYSREG_WRITE msr_tpidr_el0, CPU_TPIDR_EL0

Lmrs_tpidr_el1:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lcold
    ldr x14, [x19, #CPU_TPIDR_EL1]
    WRITE_X x14
    NEXT

SYSREG_READ mrs_tpidr_el2, CPU_TPIDR_EL2
SYSREG_WRITE msr_tpidr_el2, CPU_TPIDR_EL2

Lmrs_sp_el0:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lcold
    ldr x14, [x19, #CPU_FAST_SP]
    ldrb w10, [x19, #CPU_SPSEL]
    cbz w10, 1f
    ldr x14, [x19, #CPU_SP]
1:
    WRITE_X x14
    NEXT

Ldc_uet:
    ldrb w9, [x19, #CPU_EL]
    cbnz w9, 1f
    ldr x9, [x19, #CPU_SCTLR]
    tbz x9, #26, Lcold
1:
    NEXT

Ldc_el1:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lcold
    NEXT

.macro CRC name, width
L\name:
    READ_X x10, IR_RN, w9
    READ_X x12, IR_RM, w11
    \name w14, w10, \width\()12
    WRITE_X x14
    NEXT
.endmacro

CRC crc32b, w
CRC crc32h, w
CRC crc32w, w
CRC crc32x, x
CRC crc32cb, w
CRC crc32ch, w
CRC crc32cw, w
CRC crc32cx, x

.macro PUSH_RETURN
    sub x9, x20, x25
    add x9, x26, x9, lsr #3
    add x9, x9, #4
    str x9, [x19, #(30 * 8)]
    add x10, x20, #IR_SIZE
    ldr x14, [x19, #CPU_ICACHE]
    ldr x11, [x19, #CPU_RAS_EPOCH]
    ldr x12, [x14, #IC_CUR_GEN]
    and w15, w28, #15
    add x15, x14, x15, lsl #5
    stp x9, x10, [x15, #IC_RAS]
    stp x11, x12, [x15, #(IC_RAS + 16)]
    add w28, w28, #1
.endmacro

Lbl:
    PUSH_RETURN
Lbranch_taken:
Lb:
    ldrb w11, [x20, #IR_FLAGS]
    ldr x13, [x20, #IR_IMM]
    tbz w11, #0, Lbranch_far
    sub x9, x20, x13
    mov x20, x13
    tbnz x9, #63, Lland
    ldr w9, [x19, #CPU_BUDGET]
    sub w9, w9, #1
    str w9, [x19, #CPU_BUDGET]
    cbz w9, Lpoll
    tbnz w9, #31, Lpoll
Lland:
    LAND

Lbranch_far:
    add x13, x26, x13
    b Lcross_page

Lblr:
    READ_X x13, IR_RN
    PUSH_RETURN
    b Lcross_page

Lbr:
    READ_X x13, IR_RN
    b Lcross_page

Lret:
    READ_X x13, IR_RN
Lret_target:
    ldr x14, [x19, #CPU_ICACHE]
    sub w15, w28, #1
    and w15, w15, #15
    add x16, x14, x15, lsl #5
    ldp x9, x10, [x16, #IC_RAS]
    eor x9, x9, x13
    cbnz x9, Lcross_page
    cbz x10, Lcross_page
    ldp x9, x11, [x16, #(IC_RAS + 16)]
    ldr x12, [x19, #CPU_RAS_EPOCH]
    eor x9, x9, x12
    cbnz x9, Lcross_page
    and x12, x10, #POOL_SLOT_MASK
    ldr w9, [x12, #PAGE_GEN]
    eor w9, w9, w11
    cbnz w9, Lcross_page
    ASYNC 9, 16, 17

    mov w28, w15
    ldr x9, [x19, #CPU_EXEC_EPOCH]
    stp x12, x9, [x14, #IC_CUR_PAGE]
    sub x26, x13, #4
    and x26, x26, #0xfffffffffffff000
    stp x26, x11, [x14, #IC_CUR_VA]
    add x25, x12, #PAGE_INSNS
    mov x20, x10
    LAND

.macro ZERO_BRANCH name, width, cond
L\name:
    READ_X x10, IR_RD
    \cond \width\()10, Lbranch_taken
    NEXT
.endmacro
ZERO_BRANCH cbz32, w, cbz
ZERO_BRANCH cbz64, x, cbz
ZERO_BRANCH cbnz32, w, cbnz
ZERO_BRANCH cbnz64, x, cbnz

.macro TEST_BRANCH name, cond
L\name:
    READ_X x10, IR_RD
    ldrh w11, [x20, #IR_OP]
    lsr x10, x10, x11
    \cond x10, #0, Lbranch_taken
    NEXT
.endmacro
TEST_BRANCH tbz, tbz
TEST_BRANCH tbnz, tbnz

.macro COND name, cc
L\name:
    b.\cc Lbranch_taken
    NEXT
.endmacro

.macro FUSED name, cc, width, source
L\name:
    READ_X x10, IR_RN
.ifc \source, imm
    ldr x12, [x20, #IR_IMM]
.else
    READ_X x12, IR_RM
    ldrh w11, [x20, #IR_OP]
    tbnz w11, #9, 1f
    tbnz w11, #8, 2f
    lsl \width\()12, \width\()12, \width\()11
    b 3f
1:
    asr \width\()12, \width\()12, \width\()11
    b 3f
2:
    lsr \width\()12, \width\()12, \width\()11
3:
.endif
    subs \width\()14, \width\()10, \width\()12
    WRITE_X x14
    add x20, x20, #IR_SIZE
    b.\cc Lbranch_taken
    NEXT
.endmacro
.irp cc, eq, ne, hs, lo, mi, pl, vs, vc, hi, ls, ge, lt, gt, le, al, nv
    COND b_\cc, \cc
    FUSED f_imm32_\cc, \cc, w, imm
    FUSED f_imm64_\cc, \cc, x, imm
    FUSED f_reg32_\cc, \cc, w, reg
    FUSED f_reg64_\cc, \cc, x, reg
.endr

.macro UNARY name, op, width
L\name:
    READ_X x10, IR_RN
    \op \width\()14, \width\()10
    WRITE_X x14
    NEXT
.endmacro

UNARY rbit32, rbit, w
UNARY rbit64, rbit, x
UNARY rev16w, rev16, w
UNARY rev16x, rev16, x
UNARY rev32w, rev, w
UNARY rev32x, rev32, x
UNARY rev64, rev, x
UNARY clz32, clz, w
UNARY clz64, clz, x
UNARY cls32, cls, w
UNARY cls64, cls, x

.macro MULACC name, op, result, input
L\name:
    READ_X x10, IR_RN
    READ_X x11, IR_RM
    READ_X x12, IR_RA
    \op \result\()14, \input\()10, \input\()11, \result\()12
    WRITE_X x14
    NEXT
.endmacro

MULACC madd32, madd, w, w
MULACC msub32, msub, w, w
MULACC madd64, madd, x, x
MULACC msub64, msub, x, x
MULACC smaddl, smaddl, x, w
MULACC smsubl, smsubl, x, w
MULACC umaddl, umaddl, x, w
MULACC umsubl, umsubl, x, w
ALU smulh, smulh, x, reg
ALU umulh, umulh, x, reg

.macro EXT_ALU name, op, width
L\name:
    READ_X x10, IR_RN
    READ_X x12, IR_RM, w11
    ldrh w13, [x20, #IR_OP]
    ldrb w11, [x20, #IR_RA]
    lsl x12, x12, x11
    tbnz w13, #10, 1f
    lsr x12, x12, x11
    b 2f
1:
    asr x12, x12, x11
2:
    and w13, w13, #7
    lsl x12, x12, x13
    \op \width\()14, \width\()10, \width\()12
    WRITE_X x14
    NEXT
.endmacro

EXT_ALU add_ext32, add, w
EXT_ALU adds_ext32, adds, w
EXT_ALU sub_ext32, sub, w
EXT_ALU subs_ext32, subs, w
EXT_ALU add_ext64, add, x
EXT_ALU adds_ext64, adds, x
EXT_ALU sub_ext64, sub, x
EXT_ALU subs_ext64, subs, x

.macro MOVE_KEEP name, width
L\name:
    READ_X x10, IR_RD
    ldr x12, [x20, #IR_IMM]
    ldrh w11, [x20, #IR_OP]
    mov w13, #0xffff
    lsl \width\()13, \width\()13, \width\()11
    bic \width\()10, \width\()10, \width\()13
    lsl \width\()12, \width\()12, \width\()11
    orr \width\()14, \width\()10, \width\()12
    WRITE_X x14
    NEXT
.endmacro

MOVE_KEEP movk32, w
MOVE_KEEP movk64, x

.macro EXTRACT name, width
L\name:
    READ_X x12, IR_RM
    ldrh w11, [x20, #IR_OP]
    lsr \width\()14, \width\()12, \width\()11
    cbz w11, 1f
    READ_X x10, IR_RN
    neg w11, w11
    lsl \width\()10, \width\()10, \width\()11
    orr \width\()14, \width\()14, \width\()10
1:
    WRITE_X x14
    NEXT
.endmacro

EXTRACT extr32, w
EXTRACT extr64, x

.macro INSERT name, width, wrap
L\name:
    READ_X x10, IR_RN
    READ_X x14, IR_RD
    ldrh w11, [x20, #IR_RM]
    lsr w12, w11, #8
    mov \width\()13, #-1
.if \wrap
    lsl \width\()10, \width\()10, \width\()11
    lsl \width\()13, \width\()13, \width\()11
    lsr \width\()10, \width\()10, \width\()11
    lsr \width\()13, \width\()13, \width\()11
    lsl \width\()10, \width\()10, \width\()12
    lsl \width\()13, \width\()13, \width\()12
.else
    lsl \width\()10, \width\()10, \width\()11
    lsl \width\()13, \width\()13, \width\()11
    lsr \width\()10, \width\()10, \width\()12
    lsr \width\()13, \width\()13, \width\()12
.endif
    bic \width\()14, \width\()14, \width\()13
    orr \width\()14, \width\()14, \width\()10
    WRITE_X x14
    NEXT
.endmacro

INSERT bfxil32, w, 0
INSERT bfxil64, x, 0
INSERT bfi32, w, 1
INSERT bfi64, x, 1

.macro SELECT name, op, width, cc
L\name:
    READ_X x10, IR_RN
    READ_X x12, IR_RM, w11
    \op \width\()14, \width\()10, \width\()12, \cc
    WRITE_X x14
    NEXT
.endmacro

.macro COND_CMP cc
Lccmp_\cc:
    b.\cc Lccmp_apply
    ldrb w9, [x20, #IR_RA]
    lsl w9, w9, #28
    msr nzcv, x9
    NEXT
.endmacro

.irp cc, eq, ne, hs, lo, mi, pl, vs, vc, hi, ls, ge, lt, gt, le, al, nv
    SELECT csel32_\cc, csel, w, \cc
    SELECT csinc32_\cc, csinc, w, \cc
    SELECT csinv32_\cc, csinv, w, \cc
    SELECT csneg32_\cc, csneg, w, \cc
    SELECT csel64_\cc, csel, x, \cc
    SELECT csinc64_\cc, csinc, x, \cc
    SELECT csinv64_\cc, csinv, x, \cc
    SELECT csneg64_\cc, csneg, x, \cc
    COND_CMP \cc
.endr

Lccmp_apply:
    READ_X x10, IR_RN
    ldrb w13, [x20, #IR_FLAGS]
    tbnz w13, #5, 1f
    READ_X x12, IR_RM, w11
    b 2f
1:
    ldr x12, [x20, #IR_IMM]
2:
    tbnz w13, #1, 3f
    tbnz w13, #4, 4f
    cmn w10, w12
    NEXT
3:
    tbnz w13, #4, 5f
    cmn x10, x12
    NEXT
4:
    cmp w10, w12
    NEXT
5:
    cmp x10, x12
    NEXT

.macro VFP_CHECK
    tbnz x23, #SALT_FP_TRAP, Lvcold
.endmacro

.macro VOP insn, t, d, n, m
.ifc \t, d
    \insn d\d, d\n, d\m
.else
    \insn v\d\().\t, v\n\().\t, v\m\().\t
.endif
.endmacro

.macro VLIST insn, count, t, addr
.if \count == 1
    \insn {v16.\t}, \addr
.elseif \count == 2
    \insn {v16.\t, v17.\t}, \addr
.elseif \count == 3
    \insn {v16.\t, v17.\t, v18.\t}, \addr
.else
    \insn {v16.\t, v17.\t, v18.\t, v19.\t}, \addr
.endif
.endmacro

.macro VMEM name, reg, bytes, dir, mode, extension
Lv\name:
    VFP_CHECK
    ADDRESS \mode, \extension
.ifc \dir, load
    TLB x21, \bytes, \bytes, Lvcold
    ldr \reg\()0, [x12]
    ldrb w9, [x20, #IR_RD]
    str q0, [x19, x9, lsl #4]
.else
    TLB x22, \bytes, \bytes, Lvcold
    ldrb w9, [x20, #IR_RD]
    ldr q0, [x19, x9, lsl #4]
    str \reg\()0, [x12]
.endif
    WRITEBACK \mode
    NEXT
.endmacro

.macro VMEM_FORMS name, reg, bytes, dir
    VMEM \name\()_off, \reg, \bytes, \dir, off, imm
    VMEM \name\()_pre, \reg, \bytes, \dir, pre, imm
    VMEM \name\()_post, \reg, \bytes, \dir, post, imm
    VMEM \name\()_uxtw, \reg, \bytes, \dir, off, uxtw
    VMEM \name\()_xreg, \reg, \bytes, \dir, off, xreg
    VMEM \name\()_sxtw, \reg, \bytes, \dir, off, sxtw
.endmacro

VMEM_FORMS str8, b, 1, store
VMEM_FORMS str16, h, 2, store
VMEM_FORMS str32, s, 4, store
VMEM_FORMS str64, d, 8, store
VMEM_FORMS str128, q, 16, store
VMEM_FORMS ldr8, b, 1, load
VMEM_FORMS ldr16, h, 2, load
VMEM_FORMS ldr32, s, 4, load
VMEM_FORMS ldr64, d, 8, load
VMEM_FORMS ldr128, q, 16, load

.macro VLITERAL name, reg, bytes
Lv\name:
    VFP_CHECK
    ldr x12, [x20, #IR_IMM]
    add x12, x12, x26
    TLB x21, \bytes, \bytes, Lvcold
    ldr \reg\()0, [x12]
    ldrb w9, [x20, #IR_RD]
    str q0, [x19, x9, lsl #4]
    NEXT
.endmacro

VLITERAL ldr32_lit, s, 4
VLITERAL ldr64_lit, d, 8
VLITERAL ldr128_lit, q, 16

.macro VPAIR name, reg, bytes, dir, mode
Lv\name:
    VFP_CHECK
    ADDRESS \mode
.ifc \dir, load
    TLB x21, (\bytes * 2), \bytes, Lvcold
    ldp \reg\()0, \reg\()1, [x12]
    ldrb w9, [x20, #IR_RD]
    ldrb w10, [x20, #IR_RM]
    str q0, [x19, x9, lsl #4]
    str q1, [x19, x10, lsl #4]
.else
    TLB x22, (\bytes * 2), \bytes, Lvcold
    ldrb w9, [x20, #IR_RD]
    ldrb w10, [x20, #IR_RM]
    ldr q0, [x19, x9, lsl #4]
    ldr q1, [x19, x10, lsl #4]
    stp \reg\()0, \reg\()1, [x12]
.endif
    WRITEBACK \mode
    NEXT
.endmacro

.macro VPAIR_FORMS name, reg, bytes, dir
    VPAIR \name\()_off, \reg, \bytes, \dir, off
    VPAIR \name\()_pre, \reg, \bytes, \dir, pre
    VPAIR \name\()_post, \reg, \bytes, \dir, post
.endmacro

VPAIR_FORMS stp32, s, 4, store
VPAIR_FORMS stp64, d, 8, store
VPAIR_FORMS stp128, q, 16, store
VPAIR_FORMS ldp32, s, 4, load
VPAIR_FORMS ldp64, d, 8, load
VPAIR_FORMS ldp128, q, 16, load

.macro VLDST1 name, dir, count, t, bytes, mode
Lv\name:
    VFP_CHECK
    BASE
    mov x12, x10
.ifc \mode, post
    add x13, x10, #(\count * \bytes)
.endif
.ifc \mode, postr
    READ_X x13, IR_RM, w11
    add x13, x10, x13
.endif
.ifc \dir, ld
    TLB x21, (\count * \bytes), 16, Lvcold
    ldrb w9, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    VLIST ld1, \count, \t, [x12]
    VLIST st1, \count, 16b, [x9]
.else
    TLB x22, (\count * \bytes), 16, Lvcold
    ldrb w9, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    VLIST ld1, \count, 16b, [x9]
    VLIST st1, \count, \t, [x12]
.endif
    WRITEBACK \mode
    NEXT
.endmacro

.macro VLDST1_FORMS dir, count
    VLDST1 \dir\()1_\count\()_8b_off, \dir, \count, 8b, 8, off
    VLDST1 \dir\()1_\count\()_8b_post, \dir, \count, 8b, 8, post
    VLDST1 \dir\()1_\count\()_8b_postr, \dir, \count, 8b, 8, postr
    VLDST1 \dir\()1_\count\()_16b_off, \dir, \count, 16b, 16, off
    VLDST1 \dir\()1_\count\()_16b_post, \dir, \count, 16b, 16, post
    VLDST1 \dir\()1_\count\()_16b_postr, \dir, \count, 16b, 16, postr
.endmacro

.irp count, 1, 2, 3, 4
    VLDST1_FORMS st, \count
    VLDST1_FORMS ld, \count
.endr

.macro VLANE name, dir, reg, insn, bytes, mode
Lv\name:
    VFP_CHECK
    BASE
    mov x12, x10
.ifc \mode, post
    add x13, x10, #\bytes
.endif
.ifc \mode, postr
    READ_X x13, IR_RM, w11
    add x13, x10, x13
.endif
.ifc \dir, ld
    TLB x21, \bytes, \bytes, Lvcold
.else
    TLB x22, \bytes, \bytes, Lvcold
.endif
    ldrb w9, [x20, #IR_RD]
    ldrb w10, [x20, #IR_RA]
    add x9, x19, x9, lsl #4
    add x9, x9, x10
.ifc \dir, ld
    ldr\insn \reg\()15, [x12]
    str\insn \reg\()15, [x9]
.else
    ldr\insn \reg\()15, [x9]
    str\insn \reg\()15, [x12]
.endif
    WRITEBACK \mode
    NEXT
.endmacro

.macro VLANE_FORMS dir, lane, reg, insn, bytes
    VLANE \dir\()1l_\lane\()_off, \dir, \reg, \insn, \bytes, off
    VLANE \dir\()1l_\lane\()_post, \dir, \reg, \insn, \bytes, post
    VLANE \dir\()1l_\lane\()_postr, \dir, \reg, \insn, \bytes, postr
.endmacro

.irp dir, st, ld
    VLANE_FORMS \dir, b, w, b, 1
    VLANE_FORMS \dir, h, w, h, 2
    VLANE_FORMS \dir, s, w, , 4
    VLANE_FORMS \dir, d, x, , 8
.endr

Lvcold:
    b Lcold

.p2align 4
Lvext_index:
    .byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    .byte 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31

.macro V3 name, insn, t
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    ldr q2, [x19, x10, lsl #4]
    VOP \insn, \t, 0, 1, 2
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro V3D name, insn, t
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    ldr q2, [x19, x10, lsl #4]
    ldr q0, [x19, x11, lsl #4]
    VOP \insn, \t, 0, 1, 2
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro V2 name, insn, t
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    \insn v0.\t, v1.\t
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro V2Z name, insn, t
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    \insn v0.\t, v1.\t, #0
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro V_EACH shape, name, insn, arrs:vararg
.irp arr, \arrs
    \shape \name, \insn, \arr
.endr
.endmacro

V_EACH V3, add, add, 8b, 16b, 4h, 8h, 2s, 4s, 2d, d
V_EACH V3, sub, sub, 8b, 16b, 4h, 8h, 2s, 4s, 2d, d
V_EACH V3, cmgt, cmgt, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, cmhi, cmhi, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, cmge, cmge, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, cmhs, cmhs, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, cmtst, cmtst, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, cmeq, cmeq, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, sshl, sshl, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, ushl, ushl, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, addp, addp, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, uzp1, uzp1, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, trn1, trn1, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, zip1, zip1, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, uzp2, uzp2, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, trn2, trn2, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, zip2, zip2, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V3, smax, smax, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, umax, umax, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, smin, smin, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, umin, umin, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, smaxp, smaxp, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, umaxp, umaxp, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, sminp, sminp, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, uminp, uminp, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, mul, mul, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V3, and, and, 8b, 16b
V_EACH V3, bic, bic, 8b, 16b
V_EACH V3, orr, orr, 8b, 16b
V_EACH V3, orn, orn, 8b, 16b
V_EACH V3, eor, eor, 8b, 16b
V_EACH V3D, bsl, bsl, 8b, 16b
V_EACH V3D, bit, bit, 8b, 16b
V_EACH V3D, bif, bif, 8b, 16b
V_EACH V2Z, cmgt0, cmgt, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2Z, cmge0, cmge, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2Z, cmeq0, cmeq, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2Z, cmle0, cmle, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2Z, cmlt0, cmlt, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2, abs, abs, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2, neg, neg, 8b, 16b, 4h, 8h, 2s, 4s, 2d
V_EACH V2, rev64, rev64, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V2, cls, cls, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V2, clz, clz, 8b, 16b, 4h, 8h, 2s, 4s
V_EACH V2, rev32, rev32, 8b, 16b, 4h, 8h
V_EACH V2, rev16, rev16, 8b, 16b
V_EACH V2, cnt, cnt, 8b, 16b
V_EACH V2, not, not, 8b, 16b
V_EACH V2, rbit, rbit, 8b, 16b

.macro VSHIFT name, kind, insn, t, bt
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    dup v3.16b, w10
.ifc \kind, shift
    VOP \insn, \t, 0, 1, 3
.else
    ldr q0, [x19, x11, lsl #4]
    VOP \insn, \t, 1, 1, 3
.ifc \kind, acc
    VOP add, \t, 0, 0, 1
.else
    movi v4.2d, #0xffffffffffffffff
    VOP ushl, \t, 4, 4, 3
    bit v0.\bt, v1.\bt, v4.\bt
.endif
.endif
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro VSHIFT_FAMILY name, kind, insn
    VSHIFT \name, \kind, \insn, 8b, 8b
    VSHIFT \name, \kind, \insn, 16b, 16b
    VSHIFT \name, \kind, \insn, 4h, 8b
    VSHIFT \name, \kind, \insn, 8h, 16b
    VSHIFT \name, \kind, \insn, 2s, 8b
    VSHIFT \name, \kind, \insn, 4s, 16b
    VSHIFT \name, \kind, \insn, 2d, 16b
    VSHIFT \name, \kind, \insn, d, 8b
.endmacro

VSHIFT_FAMILY shlu, shift, ushl
VSHIFT_FAMILY shls, shift, sshl
VSHIFT_FAMILY srau, acc, ushl
VSHIFT_FAMILY sras, acc, sshl
VSHIFT_FAMILY sxi, insert, ushl

.macro VNARROW name, t, wt, q, shift
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
.if \shift
    ldrb w10, [x20, #IR_RA]
    dup v3.16b, w10
    ushl v1.\wt, v1.\wt, v3.\wt
.endif
.if \q
    ldr q0, [x19, x11, lsl #4]
    xtn2 v0.\t, v1.\wt
.else
    xtn v0.\t, v1.\wt
.endif
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro VNARROW_FAMILY name, shift
    VNARROW \name, 8b, 8h, 0, \shift
    VNARROW \name, 16b, 8h, 1, \shift
    VNARROW \name, 4h, 4s, 0, \shift
    VNARROW \name, 8h, 4s, 1, \shift
    VNARROW \name, 2s, 2d, 0, \shift
    VNARROW \name, 4s, 2d, 1, \shift
.endmacro

VNARROW_FAMILY xtn, 0
VNARROW_FAMILY shrn, 1

.macro VWIDEN name, insn, t, wt, q
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    dup v3.16b, w10
.if \q
    \insn\()2 v0.\wt, v1.\t, #0
.else
    \insn v0.\wt, v1.\t, #0
.endif
    ushl v0.\wt, v0.\wt, v3.\wt
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro VWIDEN_FAMILY name, insn
    VWIDEN \name, \insn, 8b, 8h, 0
    VWIDEN \name, \insn, 16b, 8h, 1
    VWIDEN \name, \insn, 4h, 4s, 0
    VWIDEN \name, \insn, 8h, 4s, 1
    VWIDEN \name, \insn, 2s, 2d, 0
    VWIDEN \name, \insn, 4s, 2d, 1
.endmacro

VWIDEN_FAMILY sshll, sshll
VWIDEN_FAMILY ushll, ushll

.macro VACROSS name, insn, t, r
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr q1, [x19, x9, lsl #4]
    \insn \r\()0, v1.\t
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.macro VACROSS_FAMILY name, insn, wide
.if \wide
    VACROSS \name, \insn, 8b, h
    VACROSS \name, \insn, 16b, h
    VACROSS \name, \insn, 4h, s
    VACROSS \name, \insn, 8h, s
    VACROSS \name, \insn, 4s, d
.else
    VACROSS \name, \insn, 8b, b
    VACROSS \name, \insn, 16b, b
    VACROSS \name, \insn, 4h, h
    VACROSS \name, \insn, 8h, h
    VACROSS \name, \insn, 4s, s
.endif
.endmacro

VACROSS_FAMILY saddlv, saddlv, 1
VACROSS_FAMILY uaddlv, uaddlv, 1
VACROSS_FAMILY smaxv, smaxv, 0
VACROSS_FAMILY umaxv, umaxv, 0
VACROSS_FAMILY sminv, sminv, 0
VACROSS_FAMILY uminv, uminv, 0
VACROSS_FAMILY addv, addv, 0

Lvext_8b:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    ldrb w12, [x20, #IR_RA]
    adr x13, Lvext_index
    ldr q1, [x19, x9, lsl #4]
    ldr q2, [x19, x10, lsl #4]
    ldr d3, [x13, x12]
    zip1 v1.2d, v1.2d, v2.2d
    tbl v0.8b, {v1.16b}, v3.8b
    str q0, [x19, x11, lsl #4]
    NEXT

Lvext_16b:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    ldrb w12, [x20, #IR_RA]
    adr x13, Lvext_index
    ldr q1, [x19, x9, lsl #4]
    ldr q2, [x19, x10, lsl #4]
    ldr q3, [x13, x12]
    tbl v0.16b, {v1.16b, v2.16b}, v3.16b
    str q0, [x19, x11, lsl #4]
    NEXT

.macro VTBL name, insn, count, t
Lv\name\()_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    ldr q1, [x19, x10, lsl #4]
    VLIST ld1, \count, 16b, [x9]
.ifc \insn, tbx
    ldr q0, [x19, x11, lsl #4]
.endif
.if \count == 1
    \insn v0.\t, {v16.16b}, v1.\t
.elseif \count == 2
    \insn v0.\t, {v16.16b, v17.16b}, v1.\t
.elseif \count == 3
    \insn v0.\t, {v16.16b, v17.16b, v18.16b}, v1.\t
.else
    \insn v0.\t, {v16.16b, v17.16b, v18.16b, v19.16b}, v1.\t
.endif
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.irp insn, tbl, tbx
.irp count, 1, 2, 3, 4
    VTBL \insn\()\count, \insn, \count, 8b
    VTBL \insn\()\count, \insn, \count, 16b
.endr
.endr

.macro VDUPE t
Lvdupe_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    add x9, x9, x10
    ld1r {v0.\t}, [x9]
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

.irp t, 8b, 16b, 4h, 8h, 2s, 4s, 2d
    VDUPE \t
.endr

.macro VDUPG t, r
Lvdupg_\t:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr x10, [x19, x9, lsl #3]
    dup v0.\t, \r\()10
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

VDUPG 8b, w
VDUPG 16b, w
VDUPG 4h, w
VDUPG 8h, w
VDUPG 2s, w
VDUPG 4s, w
VDUPG 2d, x

.macro VINSG s, st, r
Lvinsg_\s:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    ldr x12, [x19, x9, lsl #3]
    add x11, x19, x11, lsl #4
    \st \r\()12, [x11, x10]
    NEXT
.endmacro

VINSG b, strb, w
VINSG h, strh, w
VINSG s, str, w
VINSG d, str, x

.macro VINSE s, ld, st, r
Lvinse_\s:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RM]
    ldrb w11, [x20, #IR_RD]
    ldrb w12, [x20, #IR_RA]
    add x9, x19, x9, lsl #4
    add x11, x19, x11, lsl #4
    \ld \r\()13, [x9, x10]
    \st \r\()13, [x11, x12]
    NEXT
.endmacro

VINSE b, ldrb, strb, w
VINSE h, ldrh, strh, w
VINSE s, ldr, str, w
VINSE d, ldr, str, x

.macro VDUPS s
Lvdups_\s:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    ldr \s\()0, [x9, x10]
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

VDUPS b
VDUPS h
VDUPS s
VDUPS d

.macro VMOVG name, ld, r
Lv\name:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w10, [x20, #IR_RA]
    ldrb w11, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    \ld \r\()12, [x9, x10]
    str x12, [x19, x11, lsl #3]
    NEXT
.endmacro

VMOVG umov_b, ldrb, w
VMOVG umov_h, ldrh, w
VMOVG umov_s, ldr, w
VMOVG umov_d, ldr, x
VMOVG smov_bw, ldrsb, w
VMOVG smov_bx, ldrsb, x
VMOVG smov_hw, ldrsh, w
VMOVG smov_hx, ldrsh, x
VMOVG smov_sx, ldrsw, x

.macro VIMM name, op, t, st
Lv\name\()_\t:
    VFP_CHECK
    ldr w9, [x20, #IR_IMM_HI]
    ldrb w11, [x20, #IR_RD]
.ifc \op, mov
    dup v0.\st, w9
.else
    ldr q0, [x19, x11, lsl #4]
    dup v1.\st, w9
    \op v0.\t, v0.\t, v1.\t
.endif
    str q0, [x19, x11, lsl #4]
    NEXT
.endmacro

VIMM movi, mov, 8b, 2s
VIMM movi, mov, 16b, 4s
VIMM orri, orr, 8b, 2s
VIMM orri, orr, 16b, 4s
VIMM andi, and, 8b, 2s
VIMM andi, and, 16b, 4s

.macro VFMOV_TO_GPR name, r, offset
Lv\name:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    add x9, x19, x9, lsl #4
    ldr \r\()12, [x9, #\offset]
    str x12, [x19, x11, lsl #3]
    NEXT
.endmacro

VFMOV_TO_GPR fmov_ws, w, 0
VFMOV_TO_GPR fmov_xd, x, 0
VFMOV_TO_GPR fmov_xd1, x, 8

.macro VFMOV_FROM_GPR name, kind
Lv\name:
    VFP_CHECK
    ldrb w9, [x20, #IR_RN]
    ldrb w11, [x20, #IR_RD]
    ldr x12, [x19, x9, lsl #3]
    add x11, x19, x11, lsl #4
.ifc \kind, s
    mov w12, w12
.endif
.ifc \kind, d1
    str x12, [x11, #8]
.else
    stp x12, xzr, [x11]
.endif
    NEXT
.endmacro

VFMOV_FROM_GPR fmov_sw, s
VFMOV_FROM_GPR fmov_dx, d
VFMOV_FROM_GPR fmov_d1x, d1

Lnative_entry:
    stp x29, x30, [sp, #-160]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]
    stp d8, d9, [sp, #96]
    stp d10, d11, [sp, #112]
    stp d12, d13, [sp, #128]
    stp d14, d15, [sp, #144]
    mov x19, x0
    mov x20, x1
    mov x23, xzr
    adr x24, Lcpu_fast_base
    DISPATCH

Lnative_return:
Lexit:
    ldp x19, x20, [sp, #16]
    ldp x21, x22, [sp, #32]
    ldp x23, x24, [sp, #48]
    ldp x25, x26, [sp, #64]
    ldp x27, x28, [sp, #80]
    ldp d8, d9, [sp, #96]
    ldp d10, d11, [sp, #112]
    ldp d12, d13, [sp, #128]
    ldp d14, d15, [sp, #144]
    ldp x29, x30, [sp], #160
    .cfi_def_cfa sp, 0
    ret

Ldc_zva:
    ldrb w9, [x19, #CPU_EL]
    cbnz w9, 1f
    ldr x9, [x19, #CPU_SCTLR]
    tbz x9, #14, Lpt_cold
1:
    READ_X x12, IR_RD
    and x12, x12, #~((4 << VMM_DCZID_BS) - 1)
    TLB x22, 1, 1, Lpt_cold
    movi v0.16b, #0
.set zva_offset, 0
.rept (4 << VMM_DCZID_BS) / 32
    stp q0, q0, [x12, #zva_offset]
.set zva_offset, zva_offset + 32
.endr
    NEXT

Lmops_tail:
    ldrb w9, [x19, #CPU_EL]
    cbnz w9, 1f
    ldr x9, [x19, #CPU_SCTLR]
    tbz x9, #33, Lpt_cold
1:
    READ_X x9, IR_RN
    cbnz x9, Lpt_cold
    NEXT

Lbarrier:
    dmb ish
    ldrb w9, [x19, #CPU_ICACHE_SHOOTDOWN]
    cbnz w9, Lpt_cold
    PAGE_CURRENT Lpt_cold
    NEXT

Lclrex:
    strb wzr, [x19, #CPU_EXCL_VALID]
    str xzr, [x19, #CPU_EXCL_HVA]
    NEXT

Lmsr_daifset:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    ldrb w10, [x20, #IR_RA]
    ldr w11, [x19, #CPU_DAIF]
    orr w11, w11, w10, lsl #6
    str w11, [x19, #CPU_DAIF]
    NEXT

.macro IRQ_UNMASKED fail
    add x9, x19, #CPU_IRQ
    ldar w9, [x9]
    ldrb w10, [x19, #CPU_IRQ_MASK]
    and w9, w9, w10
    cbnz w9, \fail
.endmacro

Lmsr_daifclr:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    IRQ_UNMASKED Lpt_cold
    ldrb w10, [x20, #IR_RA]
    ldr w11, [x19, #CPU_DAIF]
    bic w11, w11, w10, lsl #6
    str w11, [x19, #CPU_DAIF]
    NEXT

Lmrs_daif:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    ldr w14, [x19, #CPU_DAIF]
    and w14, w14, #0x3c0
    WRITE_X x14
    NEXT

Lmsr_daif:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    READ_X x14, IR_RD
    and w14, w14, #0x3c0
    tbnz w14, #7, 1f
    IRQ_UNMASKED Lpt_cold
1:
    str w14, [x19, #CPU_DAIF]
    NEXT

Lmrs_el1:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    ldrb w9, [x20, #IR_RA]
    ldr x14, [x19, x9, lsl #3]
    WRITE_X x14
    NEXT

Lmsr_el1:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    READ_X x14, IR_RD
    ldrb w9, [x20, #IR_RA]
    str x14, [x19, x9, lsl #3]
    NEXT

Lmsr_sp_el0:
    ldrb w9, [x19, #CPU_EL]
    cbz w9, Lpt_cold
    ldrb w9, [x19, #CPU_SPSEL]
    cbz w9, Lpt_cold
    READ_X x14, IR_RD
    str x14, [x19, #CPU_SP]
    NEXT

Lmrs_dczid:
    mov w14, #VMM_DCZID_BS
    ldrb w9, [x19, #CPU_EL]
    cbnz w9, 1f
    ldr x9, [x19, #CPU_SCTLR]
    tbnz x9, #14, 1f
    orr w14, w14, #0x10
1:
    WRITE_X x14
    NEXT

Lmrs_cntvct:
    ldrb w9, [x19, #CPU_EL]
    cbnz w9, 1f
    ldr x9, [x19, #CPU_CNTKCTL]
    tbz x9, #1, Lpt_cold
1:
    mrs x9, nzcv
    str w9, [x19, #CPU_NZCV]
    bl _vmm_timer_ticks
    ldr x9, [x19, #CPU_CNTV_BASE]
    sub x0, x0, x9
    ldr w9, [x19, #CPU_NZCV]
    msr nzcv, x9
    WRITE_X x0
    NEXT

// Darwin's mach_absolute_time adds the commpage timebase offset to the counter that the commpage
// user timebase byte selects: 1 is CNTVCT_EL0 after an ISB, 2 is CNTVCTSS_EL0 and 3 is ACNTVCT_EL0.
Lcntvct_read:
    mov x16, #(COMMPAGE_TIMEBASE_OFFSET & 0xffff)
    movk x16, #((COMMPAGE_TIMEBASE_OFFSET >> 16) & 0xffff), lsl #16
    movk x16, #((COMMPAGE_TIMEBASE_OFFSET >> 32) & 0xffff), lsl #32
    ldrb w9, [x16, #(COMMPAGE_USER_TIMEBASE - COMMPAGE_TIMEBASE_OFFSET)]
    tbz w9, #1, 3f
    tbz w9, #0, 2f
1:
    ldr x9, [x16]
    mrs x17, S3_4_C15_C10_6
    ldr x10, [x16]
    eor x10, x10, x9
    cbnz x10, 1b
    b 5f
2:
    ldr x9, [x16]
    mrs x17, S3_3_C14_C0_6
    ldr x10, [x16]
    eor x10, x10, x9
    cbnz x10, 2b
    b 5f
3:
    isb
4:
    ldr x9, [x16]
    mrs x17, cntvct_el0
    ldr x10, [x16]
    eor x10, x10, x9
    cbnz x10, 4b
5:
    add x17, x17, x9
    ret

Lmrs_nzcv:
    mrs x14, nzcv
    WRITE_X x14
    NEXT

Lmsr_nzcv:
    READ_X x14, IR_RD
    msr nzcv, x14
    NEXT

Lmrs_fpcr:
    tbnz x23, #SALT_FP_TRAP, Lpt_cold
    ldr w14, [x19, #CPU_FPCR]
    WRITE_X x14
    NEXT

Lmsr_fpcr:
    tbnz x23, #SALT_FP_TRAP, Lpt_cold
    ldrb w9, [x19, #CPU_FP_ACTIVE]
    cbz w9, Lpt_fp
    READ_X x14, IR_RD
    mov w10, #CPU_FPCR_MASK
    and w14, w14, w10
    str w14, [x19, #CPU_FPCR]
    msr fpcr, x14
    NEXT

Lmrs_fpsr:
    tbnz x23, #SALT_FP_TRAP, Lpt_cold
    ldrb w9, [x19, #CPU_FP_ACTIVE]
    cbz w9, Lpt_fp
    mrs x14, fpsr
    ldr w10, [x19, #CPU_FPSR]
    orr w14, w14, w10
    mov w10, #(CPU_FPSR_MASK & 0xffff)
    movk w10, #(CPU_FPSR_MASK >> 16), lsl #16
    and w14, w14, w10
    str w14, [x19, #CPU_FPSR]
    WRITE_X x14
    NEXT

Lmsr_fpsr:
    tbnz x23, #SALT_FP_TRAP, Lpt_cold
    ldrb w9, [x19, #CPU_FP_ACTIVE]
    cbz w9, Lpt_fp
    READ_X x14, IR_RD
    mov w10, #(CPU_FPSR_MASK & 0xffff)
    movk w10, #(CPU_FPSR_MASK >> 16), lsl #16
    and w14, w14, w10
    str w14, [x19, #CPU_FPSR]
    msr fpsr, x14
    NEXT

Ladrp_add:
    ldr x14, [x20, #IR_IMM]
    add x14, x14, x26
    WRITE_X x14
    ldr x15, [x20, #(IR_SIZE + IR_IMM)]
    add x15, x15, x14
    WRITE_X x15, IR_RM
    NEXT 2

.macro ADRP_LOAD name, width, bytes
L\name:
    ldr x14, [x20, #IR_IMM]
    add x14, x14, x26
    ldr x12, [x20, #(IR_SIZE + IR_IMM)]
    add x12, x12, x14
    TLB x21, \bytes, \bytes, Lpt_cold
    ldr \width\()15, [x12]
    WRITE_X x14
    WRITE_X x15, IR_RM
    NEXT 2
.endmacro

ADRP_LOAD adrp_ldr32, w, 4
ADRP_LOAD adrp_ldr64, x, 8

.macro EXCLUSIVE_LOAD name, insn, width, bytes
L\name:
    ATOMIC_BASE x21, \bytes, Lpt_cold
    \insn \width\()15, [x12]
    mov w9, #1
    strb w9, [x19, #CPU_EXCL_VALID]
    mov w9, #\bytes
    strb w9, [x19, #CPU_EXCL_SIZE]
    str x12, [x19, #CPU_EXCL_HVA]
    str x15, [x19, #CPU_EXCL_VAL]
    str xzr, [x19, #(CPU_EXCL_VAL + 8)]
    WRITE_X x15
    NEXT
.endmacro

EXCLUSIVE_LOAD ldxr8, ldrb, w, 1
EXCLUSIVE_LOAD ldxr16, ldrh, w, 2
EXCLUSIVE_LOAD ldxr32, ldr, w, 4
EXCLUSIVE_LOAD ldxr64, ldr, x, 8
EXCLUSIVE_LOAD ldaxr8, ldarb, w, 1
EXCLUSIVE_LOAD ldaxr16, ldarh, w, 2
EXCLUSIVE_LOAD ldaxr32, ldar, w, 4
EXCLUSIVE_LOAD ldaxr64, ldar, x, 8

.macro EXCLUSIVE_STORE name, insn, width, bytes
L\name:
    ATOMIC_BASE x22, \bytes, Lpt_cold
    mov w13, #1
    ldrb w9, [x19, #CPU_EXCL_VALID]
    cbz w9, 2f
    ldrb w9, [x19, #CPU_EXCL_SIZE]
    eor w9, w9, #\bytes
    cbnz w9, 2f
    ldr x9, [x19, #CPU_EXCL_HVA]
    eor x9, x9, x12
    cbnz x9, 2f
    ldr x14, [x19, #CPU_EXCL_VAL]
    mov x11, x14
    READ_X x15, IR_RD
    \insn \width\()11, \width\()15, [x12]
    eor x11, x11, x14
.if \bytes < 8
    ubfx x11, x11, #0, #(\bytes * 8)
.endif
    cbnz x11, 2f
    mov w13, #0
2:
    strb wzr, [x19, #CPU_EXCL_VALID]
    str xzr, [x19, #CPU_EXCL_HVA]
    ldrb w9, [x20, #IR_RM]
    str x13, [x19, x9, lsl #3]
    cbnz w13, 3f
    WAKE_WAITERS
3:
    NEXT
.endmacro

EXCLUSIVE_STORE stxr8, casb, w, 1
EXCLUSIVE_STORE stxr16, cash, w, 2
EXCLUSIVE_STORE stxr32, cas, w, 4
EXCLUSIVE_STORE stxr64, cas, x, 8
EXCLUSIVE_STORE stlxr8, casalb, w, 1
EXCLUSIVE_STORE stlxr16, casalh, w, 2
EXCLUSIVE_STORE stlxr32, casal, w, 4
EXCLUSIVE_STORE stlxr64, casal, x, 8

#include "generated/a64_passthrough_code.inc"

.cfi_endproc

.section __TEXT,__const
.p2align 2
.globl _cpu_pt_offsets
.private_extern _cpu_pt_offsets
_cpu_pt_offsets:
#include "generated/a64_passthrough_offsets.inc"

.p2align 2
.globl _cpu_fast_base_offset
.private_extern _cpu_fast_base_offset
_cpu_fast_base_offset:
    .long Lcpu_fast_base - _cpu_fast_run

.p2align 2
.globl _cpu_fast_offsets
.private_extern _cpu_fast_offsets
_cpu_fast_offsets:
.macro FAST_OFFSETS op, suffixes:vararg
.irp suffix, \suffixes
    .long L\op\()_\suffix - Lcpu_fast_base
.endr
.endmacro
#define FAST_OP(op) .long L##op - Lcpu_fast_base
#define FAST_V2(op) FAST_OFFSETS op, 8b, 16b
#define FAST_V4(op) FAST_OFFSETS op, 8b, 16b, 4h, 8h
#define FAST_V5(op) FAST_OFFSETS op, 8b, 16b, 4h, 8h, 4s
#define FAST_V6(op) FAST_OFFSETS op, 8b, 16b, 4h, 8h, 2s, 4s
#define FAST_V7(op) FAST_OFFSETS op, 8b, 16b, 4h, 8h, 2s, 4s, 2d
#define FAST_V7D(op) FAST_OFFSETS op, 8b, 16b, 4h, 8h, 2s, 4s, 2d, d
#define FAST_BHSD(op) FAST_OFFSETS op, b, h, s, d
#define FAST_MEM3(op) FAST_OFFSETS op, off, pre, post
#define FAST_MEM6(op) FAST_OFFSETS op, off, pre, post, uxtw, xreg, sxtw
#define FAST_LDST1(op) FAST_OFFSETS op, 8b_off, 8b_post, 8b_postr, 16b_off, 16b_post, 16b_postr
#define FAST_LANE(op) FAST_OFFSETS op, b_off, b_post, b_postr, h_off, h_post, h_postr, \
                      s_off, s_post, s_postr, d_off, d_post, d_postr
#include "cpu_fast_ops.inc"

.subsections_via_symbols
