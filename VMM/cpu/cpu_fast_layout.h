#ifndef VMM_CPU_FAST_LAYOUT_H
#define VMM_CPU_FAST_LAYOUT_H

#define IR_SIZE 32
#define IR_IMM 8
#define IR_IMM_HI 12
#define IR_OP 20
#define IR_FLAGS 22
#define IR_OFFSET 24
#define IR_RD 28
#define IR_RN 29
#define IR_RM 30
#define IR_RA 31

#define REG_ZR 31
#define REG_SINK 32
#define REG_SP 33
#define CPU_FAST_SP (REG_SP * 8)
#define CPU_SP 272
#define CPU_PC 288
#define CPU_NZCV 296
#define CPU_DAIF 300
#define CPU_EL 304
#define CPU_SPSEL 305
#define CPU_EXCL_SIZE 307
#define CPU_EXCL_VALID 308
#define CPU_FP_ACTIVE 309
#define CPU_BUDGET 312
#define CPU_FPCR 316
#define CPU_FPSR 320
#define CPU_IRQ 332
#define CPU_STOP 336
#define CPU_SALT 344
#define CPU_TLB_EPOCH 352
#define CPU_ICACHE_EPOCH 360
#define CPU_ICACHE_SHOOTDOWN 368
#define CPU_VM 376
#define CPU_ICACHE 384
#define CPU_EXEC_EPOCH 400
#define CPU_RAS_EPOCH 408
#define CPU_V 416
#define V_SLOT (CPU_V / 16)
#define CPU_IC_IVAU_LAST 936
#define CPU_EXCL_VAL 944
#define CPU_SCTLR 960
#define CPU_CNTKCTL 1080
#define CPU_EXCL_HVA_PAGE 0x18
#define CPU_EXCL_HVA 0x6C0
#define CPU_TPIDR_EL0 1048
#define CPU_TPIDRRO_EL0 1056
#define CPU_TPIDR_EL1 1064
#define CPU_TLB 1728

#define VM_TLB_EPOCH 88
#define VM_ICACHE_EPOCH 96
#define VM_WFE_WAITERS 104

#define TLB_BANK_SHIFT 14
#define EXEC_BANK_SHIFT 17
#define SALT_FP_TRAP 12
#define VMM_DCZID_BS 6
#define CPU_PT_WIDE_RM 0x80000000

#define PAGE_PA 0
#define PAGE_VA 8
#define PAGE_DECODED_MASK 24
#define PAGE_GEN 28
#define PAGE_INSNS 40

#define IC_CUR_PAGE 0
#define IC_CUR_EXEC_EPOCH 8
#define IC_RAS_SP 16
#define IC_STAT_DISPATCHES 40
#define IC_RAS 64
#define IC_EXEC_CACHE 576

#endif
