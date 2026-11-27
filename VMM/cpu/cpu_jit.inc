enum : uint32_t {
    JIT_BLOCK_INSNS = 64,
    JIT_CODE_WORDS = 16384,
    JIT_STUBS = 320,
    JIT_FIXUPS = 1024,
};

enum : uint32_t {
    X_TMP = 9,
    X_TMP1 = 10,
    X_TARGET = 13,
    X_ADDR = 16,
    X_TMP2 = 17,
    X_CPU = 19,
    X_IR = 20,
    X_TLB_R = 21,
    X_TLB_W = 22,
    X_SALT = 23,
    X_FAST = 24,
    X_PAGE = 25,
    X_VA = 26,
    X_USER_TAG = 27,
    X_RAS = 28,
    X_LR = 30,
    X_ZR = 31,
    SLOT_SP = REG_SP,
    SLOTS = REG_SP + 1,
};

static const uint8_t jit_pool[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14, 15};

typedef enum jit_kind : uint8_t {
    JIT_NONE,
    JIT_NOP,
    JIT_DP,
    JIT_ADR,
    JIT_B,
    JIT_BL,
    JIT_BCOND,
    JIT_CB,
    JIT_TB,
    JIT_BR,
    JIT_BLR,
    JIT_RET,
    JIT_LDST,
    JIT_PAIR,
    JIT_LIT,
    JIT_ORD,
    JIT_RCPC,
    JIT_EXCL,
    JIT_EXCLP,
    JIT_ATOMIC,
    JIT_CASP,
    JIT_VMULTI,
    JIT_VSINGLE,
    JIT_MOPS,
    JIT_SIMD,
    JIT_SIMD_IMM,
    JIT_TBL,
    JIT_HELPER,
    JIT_SEVL,
    JIT_BARRIER,
    JIT_CLREX,
    JIT_DAIF,
    JIT_MRS,
    JIT_MSR,
    JIT_SYS,
} jit_kind_t;

typedef enum jit_sysreg : uint8_t {
    JIT_SR_NONE,
    JIT_SR_HELPER,
    JIT_SR_FIELD,
    JIT_SR_FIELD_EL0,
    JIT_SR_NZCV,
    JIT_SR_FPCR,
    JIT_SR_FPSR,
    JIT_SR_DAIF,
    JIT_SR_SP_EL0,
    JIT_SR_CURRENTEL,
    JIT_SR_CNTVCT,
    JIT_SR_CNTPCT,
    JIT_SR_CNTFRQ,
    JIT_SR_DCZID,
} jit_sysreg_t;

typedef enum jit_sysop : uint8_t {
    JIT_SYS_NONE,
    JIT_SYS_ZVA,
    JIT_SYS_DC_EL0,
    JIT_SYS_DC_EL1,
} jit_sysop_t;

typedef enum jit_stub_kind : uint8_t {
    JIT_STUB_COLD,
    JIT_STUB_GOTO,
    JIT_STUB_WAKE,
    JIT_STUB_RETURN,
    JIT_STUB_FP,
} jit_stub_kind_t;

typedef struct jit_regs {
    int8_t host[SLOTS];
    int8_t guest[32];
    uint64_t dirty;
    uint32_t vloaded;
    uint32_t vdirty;
} jit_regs_t;

typedef struct jit_stub {
    jit_stub_kind_t kind;
    uint16_t insn;
    uint32_t resume;
    uint64_t target;
    jit_regs_t regs;
} jit_stub_t;

typedef struct jit_fixup {
    uint32_t site;
    uint32_t stub;
} jit_fixup_t;

typedef struct jit_work {
    uint32_t code[JIT_CODE_WORDS];
    jit_stub_t stubs[JIT_STUBS];
    jit_fixup_t fixups[JIT_FIXUPS];
    jit_fixup_t sites[JIT_FIXUPS];
    uint32_t positions[JIT_STUBS];
    uint32_t chunk_seg;
    uint32_t chunk_gen;
    uint32_t chunk_pos;
    uint32_t chunk_end;
} jit_work_t;

typedef struct jit {
    jit_work_t *w;
    ir_page_t *pg;
    uintptr_t rx;
    uint32_t n;
    uint32_t stubs;
    uint32_t fixups;
    bool overflow;
    jit_regs_t regs;
    uint32_t locked;
    uint32_t clock;
    uint32_t stamp[32];
    uint32_t start;
    uint32_t k;
    int32_t cold;
    uint8_t fp;
    bool loop;
    bool self_loop;
    uint32_t sites;
    uint32_t seg_lo;
    uint32_t seg_hi;
    uint64_t used;
    uint32_t vused;
    uint32_t head;
    jit_regs_t head_regs;
} jit_t;

typedef struct jit_fields {
    uint8_t rd;
    uint8_t rn;
    uint8_t rm;
    uint8_t ra;
} jit_fields_t;

enum : uint8_t {
    F_NONE = 0,
    F_ZR = 1,
    F_SP = 2,
};

static inline uint32_t A_ADDI(uint32_t rd, uint32_t rn, uint32_t imm, uint32_t lsl12) {
    return 0x91000000U | (lsl12 << 22) | (imm << 10) | (rn << 5) | rd;
}
static inline uint32_t A_ADDWI(uint32_t rd, uint32_t rn, uint32_t imm) {
    return 0x11000000U | (imm << 10) | (rn << 5) | rd;
}
static inline uint32_t A_SUBWI(uint32_t rd, uint32_t rn, uint32_t imm) {
    return 0x51000000U | (imm << 10) | (rn << 5) | rd;
}
static inline uint32_t A_ADDR(uint32_t rd, uint32_t rn, uint32_t rm, uint32_t lsl) {
    return 0x8B000000U | (rm << 16) | (lsl << 10) | (rn << 5) | rd;
}
static inline uint32_t A_SUBR(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0xCB000000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_SUBI(uint32_t rd, uint32_t rn, uint32_t imm, uint32_t lsl12) {
    return 0xD1000000U | (lsl12 << 22) | (imm << 10) | (rn << 5) | rd;
}
static inline uint32_t A_ADDX(uint32_t rd, uint32_t rn, uint32_t rm, uint32_t option, uint32_t amount) {
    return 0x8B200000U | (rm << 16) | (option << 13) | (amount << 10) | (rn << 5) | rd;
}
static inline uint32_t A_ORR(uint32_t rd, uint32_t rn, uint32_t rm, uint32_t lsl) {
    return 0xAA000000U | (rm << 16) | (lsl << 10) | (rn << 5) | rd;
}
static inline uint32_t A_MOV(uint32_t rd, uint32_t rm) {
    return 0xAA0003E0U | (rm << 16) | rd;
}
static inline uint32_t A_MOVW(uint32_t rd, uint32_t rm) {
    return 0x2A000000U | (rm << 16) | (X_ZR << 5) | rd;
}
static inline uint32_t A_EOR(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0xCA000000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_EOR_LSR(uint32_t rd, uint32_t rn, uint32_t rm, uint32_t shift) {
    return 0xCA400000U | (rm << 16) | (shift << 10) | (rn << 5) | rd;
}
static inline uint32_t A_AND(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0x8A000000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_ANDW(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0x0A000000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_ORRW(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0x2A000000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_BICW(uint32_t rd, uint32_t rn, uint32_t rm) {
    return 0x0A200000U | (rm << 16) | (rn << 5) | rd;
}
static inline uint32_t A_NEG(uint32_t rd, uint32_t rm) {
    return 0xCB000000U | (rm << 16) | (X_ZR << 5) | rd;
}
static inline uint32_t A_LSRI(uint32_t rd, uint32_t rn, uint32_t shift) {
    return 0xD340FC00U | (shift << 16) | (rn << 5) | rd;
}
static inline uint32_t A_UBFX(uint32_t rd, uint32_t rn, uint32_t lsb, uint32_t width) {
    return 0xD3400000U | (lsb << 16) | ((lsb + width - 1U) << 10) | (rn << 5) | rd;
}
static inline uint32_t A_MOVZ(uint32_t rd, uint32_t imm, uint32_t hw) {
    return 0xD2800000U | (hw << 21) | (imm << 5) | rd;
}
static inline uint32_t A_MOVN(uint32_t rd, uint32_t imm, uint32_t hw) {
    return 0x92800000U | (hw << 21) | (imm << 5) | rd;
}
static inline uint32_t A_MOVK(uint32_t rd, uint32_t imm, uint32_t hw) {
    return 0xF2800000U | (hw << 21) | (imm << 5) | rd;
}
static inline uint32_t A_MOVZW(uint32_t rd, uint32_t imm) {
    return 0x52800000U | (imm << 5) | rd;
}
static inline uint32_t A_LDRX(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0xF9400000U | ((offset >> 3) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STRX(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0xF9000000U | ((offset >> 3) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDRW(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0xB9400000U | ((offset >> 2) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STRW(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0xB9000000U | ((offset >> 2) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDRB(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x39400000U | (offset << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STRB(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x39000000U | (offset << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDRQ(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x3DC00000U | ((offset >> 4) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STRQ(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x3D800000U | ((offset >> 4) << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDRX_UXTW3(uint32_t rt, uint32_t rn, uint32_t rm) {
    return 0xF8605800U | (rm << 16) | (rn << 5) | rt;
}
static inline uint32_t A_STRX_UXTW3(uint32_t rt, uint32_t rn, uint32_t rm) {
    return 0xF8205800U | (rm << 16) | (rn << 5) | rt;
}
static inline uint32_t A_LDPX(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xA9400000U | (((offset >> 3) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STPX(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xA9000000U | (((offset >> 3) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STPQ(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xAD000000U | (((offset >> 4) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDPQ(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xAD400000U | (((offset >> 4) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDPX_POST(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xA8C00000U | (((offset >> 3) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_STPX_POST(uint32_t rt, uint32_t rt2, uint32_t rn, uint32_t offset) {
    return 0xA8800000U | (((offset >> 3) & 0x7FU) << 15) | (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t A_LDRB_POST(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x38400400U | ((offset & 0x1FFU) << 12) | (rn << 5) | rt;
}
static inline uint32_t A_STRB_POST(uint32_t rt, uint32_t rn, uint32_t offset) {
    return 0x38000400U | ((offset & 0x1FFU) << 12) | (rn << 5) | rt;
}
static inline uint32_t A_LDARW(uint32_t rt, uint32_t rn) {
    return 0x88DFFC00U | (rn << 5) | rt;
}
static inline uint32_t A_STLRB(uint32_t rt, uint32_t rn) {
    return 0x089FFC00U | (rn << 5) | rt;
}
static inline uint32_t A_B(int32_t offset) {
    return 0x14000000U | ((uint32_t)offset & 0x3FFFFFFU);
}
static inline uint32_t A_BL(int32_t offset) {
    return 0x94000000U | ((uint32_t)offset & 0x3FFFFFFU);
}
static inline uint32_t A_BCOND(uint32_t cond) {
    return 0x54000000U | cond;
}
static inline uint32_t A_CBZ(uint32_t rt, bool wide, bool nonzero, int32_t offset) {
    return (wide ? 0xB4000000U : 0x34000000U) | ((uint32_t)nonzero << 24) | (((uint32_t)offset & 0x7FFFFU) << 5) | rt;
}
static inline uint32_t A_TBZ(uint32_t rt, uint32_t bit, bool nonzero, int32_t offset) {
    return 0x36000000U | ((uint32_t)nonzero << 24) | ((bit >> 5) << 31) | ((bit & 31U) << 19) |
           (((uint32_t)offset & 0x3FFFU) << 5) | rt;
}
static inline uint32_t A_BR(uint32_t rn) {
    return 0xD61F0000U | (rn << 5);
}
static inline uint32_t A_BLR(uint32_t rn) {
    return 0xD63F0000U | (rn << 5);
}
static inline uint32_t A_MRS_NZCV(uint32_t rt) {
    return 0xD53B4200U | rt;
}
static inline uint32_t A_MSR_NZCV(uint32_t rt) {
    return 0xD51B4200U | rt;
}

enum : uint32_t {
    A_MRS_FPSR = 0xD53B4420U,
    A_MSR_FPSR = 0xD51B4420U,
    A_MSR_FPCR = 0xD51B4400U,
    A_DMB_ISH = 0xD5033BBFU,
    A_DMB_ISHLD = 0xD50339BFU,
    A_DMB_ISHST = 0xD5033ABFU,
    A_DC_ZVA = 0xD50B7420U,
    A_NOP = 0xD503201FU,
};

static uint32_t jit_bitmask(uint64_t imm, bool wide) {
    if (!wide) {
        imm = (imm & 0xFFFFFFFFULL) | (imm << 32);
    }
    uint32_t size = 64;
    while (size > 2) {
        uint32_t half = size / 2;
        uint64_t mask = (1ULL << half) - 1ULL;
        if ((imm & mask) != ((imm >> half) & mask)) {
            break;
        }
        size = half;
    }
    uint64_t mask = size == 64 ? UINT64_MAX : (1ULL << size) - 1ULL;
    uint64_t element = imm & mask;
    uint32_t ones = (uint32_t)__builtin_popcountll(element);
    uint64_t pattern = ones == 64 ? UINT64_MAX : (1ULL << ones) - 1ULL;
    uint32_t rotate = 0;
    while (rotate < size) {
        uint64_t rotated = rotate == 0 ? pattern : ((pattern >> rotate) | (pattern << (size - rotate))) & mask;
        if (rotated == element) {
            break;
        }
        ++rotate;
    }
    uint32_t imms = ((~(size * 2U - 1U)) & 0x3FU) | (ones - 1U);
    return ((size == 64 ? 1U : 0U) << 22) | (rotate << 16) | (imms << 10);
}

static inline uint32_t A_ANDI(uint32_t rd, uint32_t rn, uint64_t imm) {
    return 0x92000000U | jit_bitmask(imm, true) | (rn << 5) | rd;
}
static inline uint32_t A_ORRI(uint32_t rd, uint32_t rn, uint64_t imm) {
    return 0xB2000000U | jit_bitmask(imm, true) | (rn << 5) | rd;
}
static inline uint32_t A_ANDWI(uint32_t rd, uint32_t rn, uint32_t imm) {
    return 0x12000000U | jit_bitmask(imm, false) | (rn << 5) | rd;
}
static inline uint32_t A_EORWI(uint32_t rd, uint32_t rn, uint32_t imm) {
    return 0x52000000U | jit_bitmask(imm, false) | (rn << 5) | rd;
}

static void jit_emit(jit_t *j, uint32_t word) {
    if (j->n < JIT_CODE_WORDS) {
        j->w->code[j->n++] = word;
    } else {
        j->overflow = true;
    }
}

static void jit_patch(jit_t *j, uint32_t site, uint32_t target) {
    int32_t offset = (int32_t)target - (int32_t)site;
    uint32_t word = j->w->code[site];
    if ((word & 0x7C000000U) == 0x14000000U) {
        word = (word & 0xFC000000U) | ((uint32_t)offset & 0x3FFFFFFU);
    } else if ((word & 0x7E000000U) == 0x36000000U) {
        word = (word & 0xFFF8001FU) | (((uint32_t)offset & 0x3FFFU) << 5);
    } else {
        word = (word & 0xFF00001FU) | (((uint32_t)offset & 0x7FFFFU) << 5);
    }
    j->w->code[site] = word;
}

static void jit_mov_imm(jit_t *j, uint32_t rd, uint64_t value) {
    uint32_t zeros = 0, ones = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t half = (uint32_t)(value >> (16 * i)) & 0xFFFFU;
        zeros += half == 0;
        ones += half == 0xFFFFU;
    }
    bool inverted = ones > zeros;
    bool first = true;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t half = (uint32_t)(value >> (16 * i)) & 0xFFFFU;
        if (half == (inverted ? 0xFFFFU : 0U)) {
            continue;
        }
        if (first) {
            jit_emit(j, inverted ? A_MOVN(rd, ~half & 0xFFFFU, i) : A_MOVZ(rd, half, i));
            first = false;
        } else {
            jit_emit(j, A_MOVK(rd, half, i));
        }
    }
    if (first) {
        jit_emit(j, inverted ? A_MOVN(rd, 0, 0) : A_MOVZ(rd, 0, 0));
    }
}

static void jit_add_imm(jit_t *j, uint32_t rd, uint32_t rn, int64_t imm) {
    uint64_t magnitude = imm < 0 ? (uint64_t)-imm : (uint64_t)imm;
    if (magnitude == 0) {
        if (rd != rn) {
            jit_emit(j, A_MOV(rd, rn));
        }
        return;
    }
    if (magnitude >= (1ULL << 24)) {
        jit_mov_imm(j, X_TMP2, (uint64_t)imm);
        jit_emit(j, A_ADDR(rd, rn, X_TMP2, 0));
        return;
    }
    if (magnitude >> 12) {
        uint32_t high = (uint32_t)(magnitude >> 12);
        jit_emit(j, imm < 0 ? A_SUBI(rd, rn, high, 1) : A_ADDI(rd, rn, high, 1));
        rn = rd;
    }
    if (magnitude & 0xFFFU) {
        uint32_t low = (uint32_t)(magnitude & 0xFFFU);
        jit_emit(j, imm < 0 ? A_SUBI(rd, rn, low, 0) : A_ADDI(rd, rn, low, 0));
    }
}

static void jit_jump(jit_t *j, uintptr_t target, bool link) {
    intptr_t delta = (intptr_t)target - (intptr_t)(j->rx + (uintptr_t)j->n * 4U);
    if (delta >= -(1LL << 27) && delta < (1LL << 27)) {
        jit_emit(j, link ? A_BL((int32_t)(delta >> 2)) : A_B((int32_t)(delta >> 2)));
        return;
    }
    jit_mov_imm(j, X_ADDR, target);
    jit_emit(j, link ? A_BLR(X_ADDR) : A_BR(X_ADDR));
}

static void jit_interp(jit_t *j, cpu_fast_op_t op, bool link) {
    jit_jump(j, cpu_fast_base() + cpu_fast_offsets[op], link);
}

static void jit_regs_clear(jit_regs_t *regs) {
    memset(regs->host, -1, sizeof(regs->host));
    memset(regs->guest, -1, sizeof(regs->guest));
    regs->dirty = 0;
    regs->vloaded = 0;
    regs->vdirty = 0;
}

static uint32_t jit_stub(jit_t *j, jit_stub_kind_t kind, uint64_t target) {
    if (j->stubs == JIT_STUBS) {
        j->overflow = true;
        return 0;
    }
    j->w->stubs[j->stubs] = (jit_stub_t){
        .kind = kind,
        .insn = (uint16_t)j->k,
        .resume = j->n,
        .target = target,
        .regs = j->regs,
    };
    return j->stubs++;
}

static void jit_to_stub(jit_t *j, uint32_t word, uint32_t stub) {
    if (j->fixups == JIT_FIXUPS) {
        j->overflow = true;
        return;
    }
    j->w->fixups[j->fixups++] = (jit_fixup_t){j->n, stub};
    jit_emit(j, word);
}

static void jit_call_stub(jit_t *j, jit_stub_kind_t kind, uint32_t word) {
    uint32_t stub = jit_stub(j, kind, 0);
    jit_to_stub(j, word, stub);
    j->w->stubs[stub].resume = j->n;
}

static uint32_t jit_cold(jit_t *j) {
    if (j->cold < 0) {
        j->cold = (int32_t)jit_stub(j, JIT_STUB_COLD, 0);
    }
    return (uint32_t)j->cold;
}

static void jit_cold_unless_bit(jit_t *j, uint32_t rt, uint32_t bit, bool set) {
    uint32_t cold = jit_cold(j);
    jit_emit(j, A_TBZ(rt, bit, set, 2));
    jit_to_stub(j, A_B(0), cold);
}

static uint32_t jit_take(jit_t *j) {
    int32_t victim = -1;
    for (uint32_t i = 0; i < sizeof(jit_pool); ++i) {
        uint32_t host = jit_pool[i];
        if (j->locked & (1U << host)) {
            continue;
        }
        if (j->regs.guest[host] < 0) {
            return host;
        }
        if (victim < 0 || j->stamp[host] < j->stamp[victim]) {
            victim = (int32_t)host;
        }
    }
    uint32_t slot = (uint32_t)j->regs.guest[victim];
    if (j->regs.dirty & (1ULL << slot)) {
        jit_emit(j, A_STRX((uint32_t)victim, X_CPU, slot * 8U));
        j->regs.dirty &= ~(1ULL << slot);
    }
    j->regs.host[slot] = -1;
    j->regs.guest[victim] = -1;
    return (uint32_t)victim;
}

static uint32_t jit_slot(jit_t *j, uint32_t slot, bool load) {
    int32_t host = j->regs.host[slot];
    if (host < 0) {
        host = (int32_t)jit_take(j);
        j->regs.host[slot] = (int8_t)host;
        j->regs.guest[host] = (int8_t)slot;
        if (load) {
            jit_emit(j, A_LDRX((uint32_t)host, X_CPU, slot * 8U));
        }
    }
    j->locked |= 1U << host;
    j->stamp[host] = ++j->clock;
    j->used |= 1ULL << slot;
    return (uint32_t)host;
}

static uint32_t jit_reg(jit_t *j, uint32_t reg, bool sp, bool load) {
    if (reg == 31 && !sp) {
        return X_ZR;
    }
    return jit_slot(j, reg == 31 ? SLOT_SP : reg, load);
}

static void jit_dirty(jit_t *j, uint32_t reg, bool sp) {
    if (reg != 31 || sp) {
        j->regs.dirty |= 1ULL << (reg == 31 ? SLOT_SP : reg);
    }
}

static void jit_reserve(jit_t *j, uint32_t hosts) {
    for (uint32_t host = 0; host < 32; ++host) {
        if ((hosts & (1U << host)) == 0) {
            continue;
        }
        int32_t slot = j->regs.guest[host];
        if (slot >= 0) {
            if (j->regs.dirty & (1ULL << slot)) {
                jit_emit(j, A_STRX(host, X_CPU, (uint32_t)slot * 8U));
                j->regs.dirty &= ~(1ULL << slot);
            }
            j->regs.host[slot] = -1;
            j->regs.guest[host] = -1;
        }
        j->locked |= 1U << host;
    }
}

static void jit_read_into(jit_t *j, uint32_t host, uint32_t reg) {
    if (reg == 31) {
        jit_emit(j, A_MOV(host, X_ZR));
    } else if (j->regs.host[reg] >= 0) {
        jit_emit(j, A_MOV(host, (uint32_t)j->regs.host[reg]));
    } else {
        jit_emit(j, A_LDRX(host, X_CPU, reg * 8U));
    }
}

static void jit_write_from(jit_t *j, uint32_t reg, uint32_t host) {
    if (reg == 31) {
        return;
    }
    if (j->regs.host[reg] >= 0) {
        jit_emit(j, A_MOV((uint32_t)j->regs.host[reg], host));
        j->regs.dirty |= 1ULL << reg;
    } else {
        jit_emit(j, A_STRX(host, X_CPU, reg * 8U));
    }
}

static void jit_vector_spill(jit_t *j, uint32_t vregs, bool load) {
    for (uint32_t reg = 0; reg < 32; ++reg) {
        if ((vregs & (1U << reg)) == 0) {
            continue;
        }
        uint32_t offset = CPU_V + reg * 16U;
        if (reg < 31 && (vregs & (1U << (reg + 1U)))) {
            jit_emit(j, load ? A_LDPQ(reg, reg + 1U, X_CPU, offset) : A_STPQ(reg, reg + 1U, X_CPU, offset));
            ++reg;
        } else {
            jit_emit(j, load ? A_LDRQ(reg, X_CPU, offset) : A_STRQ(reg, X_CPU, offset));
        }
    }
}

static void jit_vector_use(jit_t *j, uint32_t reg) {
    if ((j->regs.vloaded & (1U << reg)) == 0) {
        jit_emit(j, A_LDRQ(reg, X_CPU, CPU_V + reg * 16U));
        j->regs.vloaded |= 1U << reg;
    }
}

static void jit_flush(jit_t *j, const jit_regs_t *regs) {
    j->vused |= regs->vloaded;
    for (uint32_t slot = 0; slot < SLOTS; ++slot) {
        if ((regs->dirty & (1ULL << slot)) == 0) {
            continue;
        }
        if (slot + 1U < SLOTS && (regs->dirty & (1ULL << (slot + 1U)))) {
            jit_emit(j, A_STPX((uint32_t)regs->host[slot], (uint32_t)regs->host[slot + 1U], X_CPU, slot * 8U));
            ++slot;
        } else {
            jit_emit(j, A_STRX((uint32_t)regs->host[slot], X_CPU, slot * 8U));
        }
    }
    jit_vector_spill(j, regs->vdirty, false);
}

static void jit_reload(jit_t *j, const jit_regs_t *regs) {
    for (uint32_t slot = 0; slot < SLOTS; ++slot) {
        if (regs->host[slot] >= 0) {
            jit_emit(j, A_LDRX((uint32_t)regs->host[slot], X_CPU, slot * 8U));
        }
    }
    jit_vector_spill(j, regs->vloaded, true);
}

static void jit_dispatch(jit_t *j) {
    jit_emit(j, A_LDRW(X_ADDR, X_IR, IR_OFFSET));
    jit_emit(j, A_ADDX(X_ADDR, X_FAST, X_ADDR, 2, 0));
    jit_emit(j, A_BR(X_ADDR));
}

static void jit_sp_index(jit_t *j) {
    jit_emit(j, A_LDRB(X_TMP1, X_CPU, CPU_SPSEL));
    jit_emit(j, A_ADDI(X_TMP2, X_CPU, CPU_SP, 0));
}

static void jit_commit(jit_t *j) {
    jit_emit(j, A_MRS_NZCV(X_TMP));
    jit_emit(j, A_STRW(X_TMP, X_CPU, CPU_NZCV));
    jit_emit(j, A_LDRX(X_TMP, X_CPU, CPU_FAST_SP));
    if (j->pg->el == 0) {
        jit_emit(j, A_STRX(X_TMP, X_CPU, CPU_SP));
    } else {
        jit_sp_index(j);
        jit_emit(j, A_STRX_UXTW3(X_TMP, X_TMP2, X_TMP1));
    }
    jit_emit(j, A_LDRX(X_TMP, X_CPU, CPU_ICACHE));
    jit_emit(j, A_STRW(X_RAS, X_TMP, IC_RAS_SP));
}

static void jit_restore(jit_t *j) {
    jit_emit(j, A_LDRW(X_TMP, X_CPU, CPU_NZCV));
    jit_emit(j, A_MSR_NZCV(X_TMP));
    if (j->pg->el == 0) {
        jit_emit(j, A_LDRX(X_TMP, X_CPU, CPU_SP));
    } else {
        jit_sp_index(j);
        jit_emit(j, A_LDRX_UXTW3(X_TMP, X_TMP2, X_TMP1));
    }
    jit_emit(j, A_STRX(X_TMP, X_CPU, CPU_FAST_SP));
    jit_emit(j, A_LDRX(X_TMP, X_CPU, CPU_ICACHE));
    jit_emit(j, A_LDRW(X_RAS, X_TMP, IC_RAS_SP));
}

static void jit_helper_call(jit_t *j) {
    jit_emit(j, A_MOV(0, X_CPU));
    jit_add_imm(j, 1, X_PAGE, (int64_t)j->k * IR_SIZE);
    jit_emit(j, A_MOV(2, X_TLB_R));
    jit_emit(j, A_MOV(3, X_TLB_W));
    jit_interp(j, FAST_jit_helper, true);
    jit_add_imm(j, X_ADDR, X_PAGE, (int64_t)(j->k + 1U) * IR_SIZE);
    jit_emit(j, A_EOR(X_ADDR, X_ADDR, 0));
    jit_to_stub(j, A_CBZ(X_ADDR, true, true, 0), jit_stub(j, JIT_STUB_RETURN, 0));
}

static void jit_helper(jit_t *j) {
    jit_flush(j, &j->regs);
    jit_commit(j);
    jit_helper_call(j);
    jit_restore(j);
    jit_regs_clear(&j->regs);
    j->fp = 0;
}

static void jit_exit_insn(jit_t *j, uint32_t index) {
    if (index == j->start) {
        jit_jump(j, j->rx, false);
        return;
    }
    if (index < IR_PAGE_INSNS) {
        uint32_t offset = __atomic_load_n(&j->pg->insns[index].fast_offset, __ATOMIC_RELAXED);
        if (offset >= j->seg_lo && offset < j->seg_hi) {
            jit_jump(j, cpu_fast_base() + offset, false);
            return;
        }
        if (j->sites < JIT_FIXUPS) {
            j->w->sites[j->sites++] = (jit_fixup_t){j->n, index};
        }
        jit_emit(j, A_NOP);
    }
    jit_add_imm(j, X_IR, X_PAGE, (int64_t)index * IR_SIZE);
    jit_dispatch(j);
}

static void jit_budget(jit_t *j, uint32_t *zero, uint32_t *negative) {
    jit_emit(j, A_LDRW(X_TMP, X_CPU, CPU_BUDGET));
    jit_emit(j, A_SUBWI(X_TMP, X_TMP, 1));
    jit_emit(j, A_STRW(X_TMP, X_CPU, CPU_BUDGET));
    *zero = j->n;
    jit_emit(j, A_CBZ(X_TMP, false, false, 0));
    *negative = j->n;
    jit_emit(j, A_TBZ(X_TMP, 31, true, 0));
}

static void jit_loop_back(jit_t *j, const jit_regs_t *regs) {
    const jit_regs_t *head = &j->head_regs;
    for (uint32_t slot = 0; slot < SLOTS; ++slot) {
        if ((regs->dirty & (1ULL << slot)) != 0 && regs->host[slot] != head->host[slot]) {
            jit_emit(j, A_STRX((uint32_t)regs->host[slot], X_CPU, slot * 8U));
        }
    }
    jit_vector_spill(j, regs->vdirty & ~head->vdirty, false);
    for (uint32_t slot = 0; slot < SLOTS; ++slot) {
        if (head->host[slot] >= 0 && regs->host[slot] != head->host[slot]) {
            jit_emit(j, A_LDRX((uint32_t)head->host[slot], X_CPU, slot * 8U));
        }
    }
    jit_vector_spill(j, head->vloaded & ~regs->vloaded, true);
    uint32_t zero, negative;
    jit_budget(j, &zero, &negative);
    jit_emit(j, A_B((int32_t)j->head - (int32_t)j->n));
    jit_patch(j, zero, j->n);
    jit_patch(j, negative, j->n);
    jit_flush(j, head);
    jit_add_imm(j, X_IR, X_PAGE, (int64_t)j->start * IR_SIZE);
    jit_interp(j, FAST_poll, false);
}

static void jit_goto(jit_t *j, int64_t rel, const jit_regs_t *regs) {
    if (rel >= 0 && rel < VMM_PAGE_SIZE) {
        uint32_t index = (uint32_t)rel >> 2;
        if (index == j->start) {
            j->self_loop = true;
            if (j->loop) {
                jit_loop_back(j, regs);
                return;
            }
        }
        jit_flush(j, regs);
        if (index > j->k) {
            jit_exit_insn(j, index);
            return;
        }
        uint32_t zero, negative;
        jit_budget(j, &zero, &negative);
        jit_exit_insn(j, index);
        jit_patch(j, zero, j->n);
        jit_patch(j, negative, j->n);
        jit_add_imm(j, X_IR, X_PAGE, (int64_t)index * IR_SIZE);
        jit_interp(j, FAST_poll, false);
        return;
    }
    jit_flush(j, regs);
    jit_add_imm(j, X_TARGET, X_VA, rel);
    jit_interp(j, FAST_cross_page, false);
}

static void jit_push_return(jit_t *j) {
    jit_emit(j, A_LDRX(X_LR, X_CPU, CPU_ICACHE));
    jit_emit(j, A_ANDWI(X_ADDR, X_RAS, RAS_MASK));
    jit_emit(j, A_ADDR(X_ADDR, X_LR, X_ADDR, 5));
    jit_add_imm(j, X_TMP, X_VA, (int64_t)(j->k + 1U) * 4);
    jit_add_imm(j, X_TMP1, X_PAGE, (int64_t)(j->k + 1U) * IR_SIZE);
    jit_emit(j, A_STPX(X_TMP, X_TMP1, X_ADDR, IC_RAS));
    jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_RAS_EPOCH));
    jit_emit(j, A_LDRX(X_TMP, X_LR, IC_CUR_GEN));
    jit_emit(j, A_STPX(X_TMP2, X_TMP, X_ADDR, IC_RAS + 16));
    jit_emit(j, A_ADDWI(X_RAS, X_RAS, 1));
}

static void jit_fp(jit_t *j, uint8_t level) {
    if (j->fp >= level) {
        return;
    }
    if (j->fp == 0) {
        jit_cold_unless_bit(j, X_SALT, SALT_FP_TRAP, false);
    }
    if (level == 2) {
        jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_FP_ACTIVE));
        jit_call_stub(j, JIT_STUB_FP, A_CBZ(X_TMP2, false, false, 0));
    }
    j->fp = level;
}

static void jit_sp_check(jit_t *j, uint32_t base) {
    uint32_t cold = jit_cold(j);
    jit_emit(j, A_ANDI(X_TMP2, base, 15));
    jit_emit(j, A_CBZ(X_TMP2, true, false, 4));
    jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_SCTLR));
    jit_emit(j, A_TBZ(X_TMP2, j->pg->el == 0 ? 4 : 3, false, 2));
    jit_to_stub(j, A_B(0), cold);
}

static void jit_el0_check(jit_t *j, uint32_t offset, uint32_t bit) {
    if (j->pg->el == 0) {
        jit_emit(j, A_LDRX(X_TMP2, X_CPU, offset));
        jit_cold_unless_bit(j, X_TMP2, bit, true);
    }
}

static void jit_tlb(jit_t *j, uint32_t bank, uint32_t bytes, uint32_t align, bool strict) {
    uint32_t cold = jit_cold(j);
    if (strict) {
        if (align > 1) {
            jit_emit(j, A_ANDI(X_TMP2, X_ADDR, align - 1U));
            jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
        }
    } else if (bytes > 1) {
        jit_emit(j, A_ADDI(X_TMP2, X_ADDR, bytes - 1U, 0));
        jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_ADDR));
        jit_cold_unless_bit(j, X_TMP2, 12, false);
        if (align > 1) {
            jit_emit(j, A_TBZ(X_SALT, SALT_ALIGN, false, 3));
            jit_emit(j, A_ANDI(X_TMP2, X_ADDR, align - 1U));
            jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
        }
    }
    jit_emit(j, A_LSRI(X_LR, X_ADDR, 48));
    uint32_t split = j->n;
    jit_emit(j, A_CBZ(X_LR, true, j->pg->el == 0, 0));
    for (uint32_t pass = 0; pass < 2; ++pass) {
        bool user = (pass == 0) == (j->pg->el == 0);
        if (user) {
            jit_emit(j, A_EOR_LSR(X_TMP2, X_SALT, X_ADDR, 12));
            jit_emit(j, A_ANDI(X_TMP, X_ADDR, 0x0000FFFFFFFFF000ULL));
            jit_emit(j, A_ORR(X_TMP, X_TMP, X_USER_TAG, 0));
        } else {
            jit_emit(j, A_EORWI(X_LR, X_LR, 0xFFFFU));
            jit_to_stub(j, A_CBZ(X_LR, false, true, 0), jit_cold(j));
            jit_emit(j, A_LSRI(X_TMP2, X_ADDR, 12));
            jit_emit(j, A_ANDI(X_TMP, X_ADDR, 0x0000FFFFFFFFF000ULL));
            jit_emit(j, A_ANDI(X_LR, X_SALT, MMU_SALT_MMU_OFF));
            jit_emit(j, A_ORR(X_TMP, X_TMP, X_LR, 0));
            jit_emit(j, A_ORRI(X_TMP, X_TMP, SOFTTLB_TAG_GLOBAL));
        }
        if (pass == 0) {
            uint32_t join = j->n;
            jit_emit(j, A_B(0));
            jit_patch(j, split, j->n);
            split = join;
        }
    }
    jit_patch(j, split, j->n);
    jit_emit(j, A_ANDI(X_TMP2, X_TMP2, SOFTTLB_MASK));
    jit_emit(j, A_ADDR(X_TMP2, bank, X_TMP2, 4));
    jit_emit(j, A_LDPX(X_TMP2, X_LR, X_TMP2, 0));
    jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_TMP));
    jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
    jit_emit(j, A_ADDR(X_ADDR, X_ADDR, X_LR, 0));
}

static void jit_wake(jit_t *j) {
    jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_VM));
    jit_emit(j, A_ADDI(X_TMP2, X_TMP2, VM_WFE_WAITERS, 0));
    jit_emit(j, A_LDARW(X_TMP2, X_TMP2));
    jit_call_stub(j, JIT_STUB_WAKE, A_CBZ(X_TMP2, false, true, 0));
}

static uint32_t jit_field(uint32_t word, uint32_t start, uint32_t width, uint32_t value) {
    uint32_t mask = ((1U << width) - 1U) << start;
    return (word & ~mask) | (value << start);
}

static bool jit_dp_fields(a64_dec_op_t dec, jit_fields_t *f, bool *keep) {
    *keep = false;
    switch (dec) {
    case A64_DEC_ADDSUB_IMM_ADD:
    case A64_DEC_ADDSUB_IMM_SUB:
        *f = (jit_fields_t){F_SP, F_SP, F_NONE, F_NONE};
        return true;
    case A64_DEC_ADDSUB_IMM_ADDS:
    case A64_DEC_ADDSUB_IMM_SUBS:
        *f = (jit_fields_t){F_ZR, F_SP, F_NONE, F_NONE};
        return true;
    case A64_DEC_ADDSUB_EXT_ADD:
    case A64_DEC_ADDSUB_EXT_SUB:
        *f = (jit_fields_t){F_SP, F_SP, F_ZR, F_NONE};
        return true;
    case A64_DEC_ADDSUB_EXT_ADDS:
    case A64_DEC_ADDSUB_EXT_SUBS:
        *f = (jit_fields_t){F_ZR, F_SP, F_ZR, F_NONE};
        return true;
    case A64_DEC_LOG_IMM_AND:
    case A64_DEC_LOG_IMM_ORR:
    case A64_DEC_LOG_IMM_EOR:
        *f = (jit_fields_t){F_SP, F_ZR, F_NONE, F_NONE};
        return true;
    case A64_DEC_LOG_IMM_ANDS:
    case A64_DEC_DP_1SRC_RBIT:
    case A64_DEC_DP_1SRC_REV16:
    case A64_DEC_DP_1SRC_REV32:
    case A64_DEC_DP_1SRC_REV:
    case A64_DEC_DP_1SRC_CLZ:
    case A64_DEC_DP_1SRC_CLS:
    case A64_DEC_BITFIELD_SBFM:
    case A64_DEC_BITFIELD_UBFM:
        *f = (jit_fields_t){F_ZR, F_ZR, F_NONE, F_NONE};
        return true;
    case A64_DEC_BITFIELD_BFM:
        *f = (jit_fields_t){F_ZR, F_ZR, F_NONE, F_NONE};
        *keep = true;
        return true;
    case A64_DEC_MOVEWIDE_MOVZ:
    case A64_DEC_MOVEWIDE_MOVN:
        *f = (jit_fields_t){F_ZR, F_NONE, F_NONE, F_NONE};
        return true;
    case A64_DEC_MOVEWIDE_MOVK:
        *f = (jit_fields_t){F_ZR, F_NONE, F_NONE, F_NONE};
        *keep = true;
        return true;
    case A64_DEC_ADDSUB_SHIFT_ADD:
    case A64_DEC_ADDSUB_SHIFT_ADDS:
    case A64_DEC_ADDSUB_SHIFT_SUB:
    case A64_DEC_ADDSUB_SHIFT_SUBS:
    case A64_DEC_ADDSUB_CARRY_ADC:
    case A64_DEC_ADDSUB_CARRY_ADCS:
    case A64_DEC_ADDSUB_CARRY_SBC:
    case A64_DEC_ADDSUB_CARRY_SBCS:
    case A64_DEC_LOG_SHIFT_AND:
    case A64_DEC_LOG_SHIFT_BIC:
    case A64_DEC_LOG_SHIFT_ORR:
    case A64_DEC_LOG_SHIFT_ORN:
    case A64_DEC_LOG_SHIFT_EOR:
    case A64_DEC_LOG_SHIFT_EON:
    case A64_DEC_LOG_SHIFT_ANDS:
    case A64_DEC_LOG_SHIFT_BICS:
    case A64_DEC_EXTRACT_EXTR:
    case A64_DEC_CONDSEL_CSEL:
    case A64_DEC_CONDSEL_CSINC:
    case A64_DEC_CONDSEL_CSINV:
    case A64_DEC_CONDSEL_CSNEG:
    case A64_DEC_DP_2SRC_UDIV:
    case A64_DEC_DP_2SRC_SDIV:
    case A64_DEC_DP_2SRC_LSLV:
    case A64_DEC_DP_2SRC_LSRV:
    case A64_DEC_DP_2SRC_ASRV:
    case A64_DEC_DP_2SRC_RORV:
    case A64_DEC_DP_2SRC_CRC32B:
    case A64_DEC_DP_2SRC_CRC32H:
    case A64_DEC_DP_2SRC_CRC32W:
    case A64_DEC_DP_2SRC_CRC32X:
    case A64_DEC_DP_2SRC_CRC32CB:
    case A64_DEC_DP_2SRC_CRC32CH:
    case A64_DEC_DP_2SRC_CRC32CW:
    case A64_DEC_DP_2SRC_CRC32CX:
        *f = (jit_fields_t){F_ZR, F_ZR, F_ZR, F_NONE};
        return true;
    case A64_DEC_CONDCMP_REG_CCMN:
    case A64_DEC_CONDCMP_REG_CCMP:
        *f = (jit_fields_t){F_NONE, F_ZR, F_ZR, F_NONE};
        return true;
    case A64_DEC_CONDCMP_IMM_CCMN:
    case A64_DEC_CONDCMP_IMM_CCMP:
        *f = (jit_fields_t){F_NONE, F_ZR, F_NONE, F_NONE};
        return true;
    case A64_DEC_DP_3SRC_MADD:
    case A64_DEC_DP_3SRC_MSUB:
    case A64_DEC_DP_3SRC_SMADDL:
    case A64_DEC_DP_3SRC_SMSUBL:
    case A64_DEC_DP_3SRC_UMADDL:
    case A64_DEC_DP_3SRC_UMSUBL:
    case A64_DEC_DP_3SRC_SMULH:
    case A64_DEC_DP_3SRC_UMULH:
        *f = (jit_fields_t){F_ZR, F_ZR, F_ZR, F_ZR};
        return true;
    case A64_DEC_DP_1SRC_ABS:
    case A64_DEC_DP_1SRC_CNT:
    case A64_DEC_DP_1SRC_CTZ:
    case A64_DEC_MINMAX_IMM_SMAX:
    case A64_DEC_MINMAX_IMM_SMIN:
    case A64_DEC_MINMAX_IMM_UMAX:
    case A64_DEC_MINMAX_IMM_UMIN:
        *f = (jit_fields_t){F_ZR, F_ZR, F_NONE, F_NONE};
        return true;
    case A64_DEC_DP_2SRC_SMAX:
    case A64_DEC_DP_2SRC_SMIN:
    case A64_DEC_DP_2SRC_UMAX:
    case A64_DEC_DP_2SRC_UMIN:
        *f = (jit_fields_t){F_ZR, F_ZR, F_ZR, F_NONE};
        return true;
    case A64_DEC_RMIF_RMIF:
    case A64_DEC_SETF_SETF8:
    case A64_DEC_SETF_SETF16:
        *f = (jit_fields_t){F_NONE, F_ZR, F_NONE, F_NONE};
        return (cpu_host_features() & CPU_HOST_FEAT_FlagM) != 0U;
    case A64_DEC_PSTATE_CFINV:
        *f = (jit_fields_t){F_NONE, F_NONE, F_NONE, F_NONE};
        return (cpu_host_features() & CPU_HOST_FEAT_FlagM) != 0U;
    case A64_DEC_PSTATE_XAFLAG:
    case A64_DEC_PSTATE_AXFLAG:
        *f = (jit_fields_t){F_NONE, F_NONE, F_NONE, F_NONE};
        return (cpu_host_features() & CPU_HOST_FEAT_FlagM2) != 0U;
    default:
        return false;
    }
}

static jit_sysreg_t jit_sysreg_read(uint16_t key, uint32_t *offset) {
    switch (key) {
    case A64_SYSREG_NZCV: return JIT_SR_NZCV;
    case A64_SYSREG_FPCR: return JIT_SR_FPCR;
    case A64_SYSREG_FPSR: return JIT_SR_FPSR;
    case A64_SYSREG_DAIF: return JIT_SR_DAIF;
    case A64_SYSREG_SP_EL0: return JIT_SR_SP_EL0;
    case A64_SYSREG_CURRENTEL: return JIT_SR_CURRENTEL;
    case A64_SYSREG_CNTVCT_EL0:
    case A64_SYSREG_CNTVCTSS_EL0: return JIT_SR_CNTVCT;
    case A64_SYSREG_CNTPCT_EL0:
    case A64_SYSREG_CNTPCTSS_EL0: return JIT_SR_CNTPCT;
    case A64_SYSREG_TPIDR_EL0: *offset = offsetof(vcpu_t, tpidr_el0); return JIT_SR_FIELD_EL0;
    case A64_SYSREG_TPIDRRO_EL0: *offset = offsetof(vcpu_t, tpidrro_el0); return JIT_SR_FIELD_EL0;
    case A64_SYSREG_MPIDR_EL1: *offset = offsetof(vcpu_t, mpidr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_SCTLR_EL1: *offset = offsetof(vcpu_t, sctlr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_ACTLR_EL1: *offset = offsetof(vcpu_t, sysregs.actlr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CPACR_EL1: *offset = offsetof(vcpu_t, cpacr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TTBR0_EL1: *offset = offsetof(vcpu_t, ttbr0_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TTBR1_EL1: *offset = offsetof(vcpu_t, ttbr1_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TCR_EL1: *offset = offsetof(vcpu_t, tcr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AFSR0_EL1: *offset = offsetof(vcpu_t, afsr0_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AFSR1_EL1: *offset = offsetof(vcpu_t, afsr1_el1); return JIT_SR_FIELD;
    case A64_SYSREG_ESR_EL1: *offset = offsetof(vcpu_t, esr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_FAR_EL1: *offset = offsetof(vcpu_t, far_el1); return JIT_SR_FIELD;
    case A64_SYSREG_PAR_EL1: *offset = offsetof(vcpu_t, par_el1); return JIT_SR_FIELD;
    case A64_SYSREG_MAIR_EL1: *offset = offsetof(vcpu_t, mair_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AMAIR_EL1: *offset = offsetof(vcpu_t, amair_el1); return JIT_SR_FIELD;
    case A64_SYSREG_VBAR_EL1: *offset = offsetof(vcpu_t, vbar_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CONTEXTIDR_EL1: *offset = offsetof(vcpu_t, contextidr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TPIDR_EL1: *offset = offsetof(vcpu_t, tpidr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TPIDR_EL2: *offset = offsetof(vcpu_t, tpidr_el2); return JIT_SR_FIELD;
    case A64_SYSREG_CNTKCTL_EL1: *offset = offsetof(vcpu_t, cntkctl_el1); return JIT_SR_FIELD;
    case A64_SYSREG_SPSR_EL1: *offset = offsetof(vcpu_t, spsr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_ELR_EL1: *offset = offsetof(vcpu_t, elr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_MDSCR_EL1: *offset = offsetof(vcpu_t, mdscr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CSSELR_EL1: *offset = offsetof(vcpu_t, sysregs.csselr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_OSDLR_EL1: *offset = offsetof(vcpu_t, sysregs.osdlr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_UAO: *offset = offsetof(vcpu_t, sysregs.uao); return JIT_SR_FIELD;
    case A64_SYSREG_CNTFRQ_EL0: return JIT_SR_CNTFRQ;
    case A64_SYSREG_DCZID_EL0: return JIT_SR_DCZID;
    case A64_SYSREG_MIDR_EL1:
    case A64_SYSREG_REVIDR_EL1:
    case A64_SYSREG_AIDR_EL1:
    case A64_SYSREG_CNTP_TVAL_EL0:
    case A64_SYSREG_CNTP_CTL_EL0:
    case A64_SYSREG_CNTP_CVAL_EL0:
    case A64_SYSREG_CNTV_TVAL_EL0:
    case A64_SYSREG_CNTV_CTL_EL0:
    case A64_SYSREG_CNTV_CVAL_EL0:
    case A64_SYSREG_SPSEL:
    case A64_SYSREG_PAN:
    case A64_SYSREG_CTR_EL0:
    case A64_SYSREG_CLIDR_EL1:
    case A64_SYSREG_CCSIDR_EL1:
    case A64_SYSREG_MDCCSR_EL0:
    case A64_SYSREG_OSLSR_EL1:
    case A64_SYSREG_PMCR_EL0:
    case A64_SYSREG_PMCNTENSET_EL0:
    case A64_SYSREG_PMCNTENCLR_EL0:
    case A64_SYSREG_PMOVSCLR_EL0:
    case A64_SYSREG_PMSELR_EL0:
    case A64_SYSREG_PMCEID0_EL0:
    case A64_SYSREG_PMCEID1_EL0:
    case A64_SYSREG_PMCCNTR_EL0:
    case A64_SYSREG_PMXEVTYPER_EL0:
    case A64_SYSREG_PMXEVCNTR_EL0:
    case A64_SYSREG_PMUSERENR_EL0:
    case A64_SYSREG_PMINTENSET_EL1:
    case A64_SYSREG_PMINTENCLR_EL1:
    case A64_SYSREG_PMOVSSET_EL0:
        return JIT_SR_HELPER;
    default:
        if ((key >> 14) == 3U && ((key >> 7) & 0x7FU) == 0U && ((key >> 3) & 15U) <= 7U) {
            return JIT_SR_HELPER;
        }
        if ((key >> 14) == 2U && ((key >> 7) & 0x7FU) == 0U && (key & 7U) >= 4U) {
            return JIT_SR_HELPER;
        }
        return JIT_SR_NONE;
    }
}

static jit_sysreg_t jit_sysreg_write(uint16_t key, uint32_t *offset) {
    switch (key) {
    case A64_SYSREG_NZCV: return JIT_SR_NZCV;
    case A64_SYSREG_FPCR: return JIT_SR_FPCR;
    case A64_SYSREG_FPSR: return JIT_SR_FPSR;
    case A64_SYSREG_DAIF: return JIT_SR_DAIF;
    case A64_SYSREG_SP_EL0: return JIT_SR_SP_EL0;
    case A64_SYSREG_TPIDR_EL0: *offset = offsetof(vcpu_t, tpidr_el0); return JIT_SR_FIELD_EL0;
    case A64_SYSREG_TPIDRRO_EL0: *offset = offsetof(vcpu_t, tpidrro_el0); return JIT_SR_FIELD;
    case A64_SYSREG_TPIDR_EL1: *offset = offsetof(vcpu_t, tpidr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_TPIDR_EL2: *offset = offsetof(vcpu_t, tpidr_el2); return JIT_SR_FIELD;
    case A64_SYSREG_ACTLR_EL1: *offset = offsetof(vcpu_t, sysregs.actlr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AFSR0_EL1: *offset = offsetof(vcpu_t, afsr0_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AFSR1_EL1: *offset = offsetof(vcpu_t, afsr1_el1); return JIT_SR_FIELD;
    case A64_SYSREG_ESR_EL1: *offset = offsetof(vcpu_t, esr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_FAR_EL1: *offset = offsetof(vcpu_t, far_el1); return JIT_SR_FIELD;
    case A64_SYSREG_PAR_EL1: *offset = offsetof(vcpu_t, par_el1); return JIT_SR_FIELD;
    case A64_SYSREG_MAIR_EL1: *offset = offsetof(vcpu_t, mair_el1); return JIT_SR_FIELD;
    case A64_SYSREG_AMAIR_EL1: *offset = offsetof(vcpu_t, amair_el1); return JIT_SR_FIELD;
    case A64_SYSREG_VBAR_EL1: *offset = offsetof(vcpu_t, vbar_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CONTEXTIDR_EL1: *offset = offsetof(vcpu_t, contextidr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CNTKCTL_EL1: *offset = offsetof(vcpu_t, cntkctl_el1); return JIT_SR_FIELD;
    case A64_SYSREG_SPSR_EL1: *offset = offsetof(vcpu_t, spsr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_ELR_EL1: *offset = offsetof(vcpu_t, elr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_MDSCR_EL1: *offset = offsetof(vcpu_t, mdscr_el1); return JIT_SR_FIELD;
    case A64_SYSREG_CNTP_TVAL_EL0:
    case A64_SYSREG_CNTP_CTL_EL0:
    case A64_SYSREG_CNTP_CVAL_EL0:
    case A64_SYSREG_CNTV_TVAL_EL0:
    case A64_SYSREG_CNTV_CTL_EL0:
    case A64_SYSREG_CNTV_CVAL_EL0:
    case A64_SYSREG_CSSELR_EL1:
    case A64_SYSREG_OSDLR_EL1:
    case A64_SYSREG_OSLAR_EL1:
    case A64_SYSREG_PMCR_EL0:
    case A64_SYSREG_PMCNTENSET_EL0:
    case A64_SYSREG_PMCNTENCLR_EL0:
    case A64_SYSREG_PMOVSCLR_EL0:
    case A64_SYSREG_PMSELR_EL0:
    case A64_SYSREG_PMCCNTR_EL0:
    case A64_SYSREG_PMXEVTYPER_EL0:
    case A64_SYSREG_PMXEVCNTR_EL0:
    case A64_SYSREG_PMUSERENR_EL0:
    case A64_SYSREG_PMINTENSET_EL1:
    case A64_SYSREG_PMINTENCLR_EL1:
    case A64_SYSREG_PMOVSSET_EL0:
        return JIT_SR_HELPER;
    default:
        if ((key >> 14) == 2U && ((key >> 7) & 0x7FU) == 0U && (key & 7U) >= 4U) {
            return JIT_SR_HELPER;
        }
        return JIT_SR_NONE;
    }
}

static jit_sysreg_t jit_sysreg(uint32_t word, const ir_page_t *pg, uint32_t *offset) {
    uint16_t key = (uint16_t)(word >> 5);
    bool read = ((word >> 21) & 1U) != 0;
    jit_sysreg_t kind = read ? jit_sysreg_read(key, offset) : jit_sysreg_write(key, offset);
    if (pg->el == 0 && (kind == JIT_SR_FIELD || kind == JIT_SR_SP_EL0 ||
                        (((word >> 16) & 7U) != 3U && kind != JIT_SR_FPCR && kind != JIT_SR_FPSR))) {
        return JIT_SR_NONE;
    }
    if (pg->world == VCPU_WORLD_GUEST) {
        if (key == A64_SYSREG_TPIDR_EL2 || cpu_sysreg_trapped(pg->traps, word)) {
            return JIT_SR_NONE;
        }
        if (key == A64_SYSREG_MPIDR_EL1 || (key == A64_SYSREG_DCZID_EL0 && (pg->traps & CPU_TRAP_TDZ) != 0)) {
            return JIT_SR_HELPER;
        }
    }
    if (kind == JIT_SR_FIELD && pg->world == VCPU_WORLD_HOST_NVHE && cpu_field_banked(*offset)) {
        return JIT_SR_HELPER;
    }
    return kind;
}

static jit_sysop_t jit_sysop(uint32_t word, const ir_page_t *pg) {
    if ((pg->el == 0 && ((word >> 16) & 7U) != 3U) || cpu_sysop_trapped(pg->traps, word)) {
        return JIT_SYS_NONE;
    }
    switch (A64_SYSOP_KEY((word >> 16) & 7U, (word >> 12) & 15U, (word >> 8) & 15U, (word >> 5) & 7U)) {
    case A64_SYSOP_DC_ZVA: return JIT_SYS_ZVA;
    case A64_SYSOP_DC_CVAC:
    case A64_SYSOP_DC_CVAU:
    case A64_SYSOP_DC_CIVAC:
    case A64_SYSOP_DC_CVAP:
    case A64_SYSOP_DC_CVADP: return JIT_SYS_DC_EL0;
    case A64_SYSOP_DC_IVAC:
    case A64_SYSOP_DC_ISW:
    case A64_SYSOP_DC_CSW:
    case A64_SYSOP_DC_CISW: return JIT_SYS_DC_EL1;
    default: return JIT_SYS_NONE;
    }
}

static uint32_t jit_multi_regs(uint32_t word) {
    static const uint8_t regs[16] = {[0] = 4, [2] = 4, [4] = 3, [6] = 3, [7] = 1, [8] = 2, [10] = 2};
    return regs[(word >> 12) & 15U];
}

static jit_kind_t jit_classify(uint32_t word, const ir_insn_t *ir, const ir_page_t *pg) {
    if (ir->handler == op_undef) {
        return JIT_NONE;
    }
    if (ir->handler == op_nop) {
        return JIT_NOP;
    }
    jit_fields_t fields;
    bool keep;
    uint32_t offset;
    a64_dec_op_t dec = a64_decode_word(word);
    if (jit_dp_fields(dec, &fields, &keep)) {
        return JIT_DP;
    }
    switch (dec) {
    case A64_DEC_PCRELADDR_ADR:
    case A64_DEC_PCRELADDR_ADRP:
        return JIT_ADR;
    case A64_DEC_BRANCH_IMM_B: return JIT_B;
    case A64_DEC_BRANCH_IMM_BL: return JIT_BL;
    case A64_DEC_B_COND: return JIT_BCOND;
    case A64_DEC_COMPBRANCH_CBZ:
    case A64_DEC_COMPBRANCH_CBNZ: return JIT_CB;
    case A64_DEC_TESTBRANCH_TBZ:
    case A64_DEC_TESTBRANCH_TBNZ: return JIT_TB;
    case A64_DEC_BRANCH_REG_BR: return JIT_BR;
    case A64_DEC_BRANCH_REG_BLR: return JIT_BLR;
    case A64_DEC_BRANCH_REG_RET: return JIT_RET;
    case A64_DEC_LDST_POS:
    case A64_DEC_LDST_UNSCALED:
    case A64_DEC_LDST_IMMPOST:
    case A64_DEC_LDST_IMMPRE:
    case A64_DEC_LDST_REGOFF:
    case A64_DEC_LDST_UNPRIV:
        return JIT_LDST;
    case A64_DEC_LDST_PAIR_OFF:
    case A64_DEC_LDST_PAIR_PRE:
    case A64_DEC_LDST_PAIR_POST:
    case A64_DEC_LDST_NAPAIR:
        return JIT_PAIR;
    case A64_DEC_LOADLIT: return JIT_LIT;
    case A64_DEC_LDST_ORD: return JIT_ORD;
    case A64_DEC_LDAPSTL_UNSCALED: return JIT_RCPC;
    case A64_DEC_LDST_EXCLR: return JIT_EXCL;
    case A64_DEC_LDST_EXCLP: return JIT_EXCLP;
    case A64_DEC_LSE_MEMOP:
    case A64_DEC_LSE_CAS: {
        const cpu_pt_leaf_t *leaf = cpu_pt_lookup(word, &offset);
        return !leaf ? JIT_NONE : (leaf->features & ~cpu_host_features()) ? JIT_HELPER : JIT_ATOMIC;
    }
    case A64_DEC_LSE_CASP:
        if ((word & (1U << 16)) || (word & 1U) || !cpu_pt_lookup(word, &offset)) {
            return JIT_NONE;
        }
        return (cpu_host_features() & CPU_HOST_FEAT_LSE) ? JIT_CASP : JIT_HELPER;
    case A64_DEC_ASISDLSE:
    case A64_DEC_ASISDLSEP: {
        uint32_t opcode = (word >> 12) & 15U;
        bool structure = opcode == 0U || opcode == 4U || opcode == 8U;
        if (jit_multi_regs(word) == 0 || (structure && ((word >> 10) & 3U) == 3U && ((word >> 30) & 1U) == 0U)) {
            return JIT_NONE;
        }
        return JIT_VMULTI;
    }
    case A64_DEC_ASISDLSO:
    case A64_DEC_ASISDLSOP:
        return JIT_VSINGLE;
    case A64_DEC_MOPS_CPY:
    case A64_DEC_MOPS_SET:
        return JIT_MOPS;
    case A64_DEC_ASIMDTBL:
        return JIT_TBL;
    case A64_DEC_ASIMDIMM:
        if (cpu_pt_find(word) < 0) {
            return JIT_SIMD_IMM;
        }
        [[fallthrough]];
    case A64_DEC_CRYPTOAES:
    case A64_DEC_CRYPTOSHA2:
    case A64_DEC_CRYPTOSHA3:
    case A64_DEC_CRYPTO3_IMM6:
    case A64_DEC_CRYPTO4:
    case A64_DEC_CRYPTOSHA512_2:
    case A64_DEC_CRYPTOSHA512_3:
    case A64_DEC_ASIMDALL:
    case A64_DEC_ASIMDDIFF:
    case A64_DEC_ASIMDELEM:
    case A64_DEC_ASIMDEXT:
    case A64_DEC_ASIMDINS:
    case A64_DEC_ASIMDMISC:
    case A64_DEC_ASIMDMISCFP16:
    case A64_DEC_ASIMDPERM:
    case A64_DEC_ASIMDSAME:
    case A64_DEC_ASIMDSAME2:
    case A64_DEC_ASIMDSAMEFP16:
    case A64_DEC_ASIMDSHF:
    case A64_DEC_ASISDDIFF:
    case A64_DEC_ASISDELEM:
    case A64_DEC_ASISDMISC:
    case A64_DEC_ASISDMISCFP16:
    case A64_DEC_ASISDONE:
    case A64_DEC_ASISDPAIR:
    case A64_DEC_ASISDSAME:
    case A64_DEC_ASISDSAME2:
    case A64_DEC_ASISDSAMEFP16:
    case A64_DEC_ASISDSHF:
    case A64_DEC_FLOAT2FIX:
    case A64_DEC_FLOAT2INT:
    case A64_DEC_FLOATCCMP:
    case A64_DEC_FLOATCMP:
    case A64_DEC_FLOATDP1:
    case A64_DEC_FLOATDP2:
    case A64_DEC_FLOATDP3:
    case A64_DEC_FLOATIMM:
    case A64_DEC_FLOATSEL: {
        const cpu_pt_leaf_t *leaf = cpu_pt_lookup(word, &offset);
        return !leaf ? JIT_NONE : (leaf->features & ~cpu_host_features()) ? JIT_HELPER : JIT_SIMD;
    }
    case A64_DEC_HINTS_SEV: return JIT_HELPER;
    case A64_DEC_HINTS_SEVL: return JIT_SEVL;
    case A64_DEC_BARRIERS_DMB:
    case A64_DEC_BARRIERS_DSB:
    case A64_DEC_BARRIERS_ISB:
        return JIT_BARRIER;
    case A64_DEC_BARRIERS_CLREX: return JIT_CLREX;
    case A64_DEC_PSTATE_MSR:
        return ((word >> 16) & 7U) == 3U && ((word >> 5) & 6U) == 6U ? JIT_DAIF : JIT_NONE;
    case A64_DEC_SYSTEMMOVE_MRS:
        return jit_sysreg(word, pg, &offset) != JIT_SR_NONE ? JIT_MRS : JIT_NONE;
    case A64_DEC_SYSTEMMOVE_MSR:
        return jit_sysreg(word, pg, &offset) != JIT_SR_NONE ? JIT_MSR : JIT_NONE;
    case A64_DEC_SYSTEMINSTRS_SYS:
        return jit_sysop(word, pg) != JIT_SYS_NONE ? JIT_SYS : JIT_NONE;
    default:
        return JIT_NONE;
    }
}

static void jit_dp(jit_t *j, uint32_t word, a64_dec_op_t dec) {
    jit_fields_t f;
    bool keep;
    jit_dp_fields(dec, &f, &keep);
    uint32_t rd = word & 31U;
    if (f.rn) {
        word = jit_field(word, 5, 5, jit_reg(j, (word >> 5) & 31U, f.rn == F_SP, true));
    }
    if (f.rm) {
        word = jit_field(word, 16, 5, jit_reg(j, (word >> 16) & 31U, false, true));
    }
    if (f.ra) {
        word = jit_field(word, 10, 5, jit_reg(j, (word >> 10) & 31U, false, true));
    }
    if (f.rd) {
        word = jit_field(word, 0, 5, jit_reg(j, rd, f.rd == F_SP, keep));
    }
    jit_emit(j, word);
    if (f.rd) {
        jit_dirty(j, rd, f.rd == F_SP);
    }
}

static void jit_adr(jit_t *j, uint32_t word) {
    uint32_t rd = word & 31U;
    if (rd == 31) {
        return;
    }
    int64_t imm = cpu_sext((((word >> 5) & 0x7FFFFULL) << 2) | ((word >> 29) & 3ULL), 21);
    int64_t rel = (word >> 31) ? (int64_t)((uint64_t)imm << 12) : (int64_t)j->k * 4 + imm;
    jit_add_imm(j, jit_reg(j, rd, false, false), X_VA, rel);
    jit_dirty(j, rd, false);
}

static int64_t jit_branch_target(const jit_t *j, uint32_t word, a64_dec_op_t dec) {
    int64_t offset;
    if (dec == A64_DEC_BRANCH_IMM_B || dec == A64_DEC_BRANCH_IMM_BL) {
        offset = cpu_sext(word & 0x03FFFFFFU, 26);
    } else if (dec == A64_DEC_TESTBRANCH_TBZ || dec == A64_DEC_TESTBRANCH_TBNZ) {
        offset = cpu_sext((word >> 5) & 0x3FFFU, 14);
    } else {
        offset = cpu_sext((word >> 5) & 0x7FFFFU, 19);
    }
    return (int64_t)j->k * 4 + offset * 4;
}

static void jit_branch_reg(jit_t *j, uint32_t word, jit_kind_t kind) {
    uint32_t rn = (word >> 5) & 31U;
    jit_flush(j, &j->regs);
    jit_read_into(j, X_TARGET, rn);
    if (kind == JIT_BLR) {
        jit_add_imm(j, X_TMP2, X_VA, (int64_t)(j->k + 1U) * 4);
        jit_emit(j, A_STRX(X_TMP2, X_CPU, 30 * 8));
        jit_push_return(j);
    }
    jit_interp(j, kind == JIT_RET ? FAST_ret_target : FAST_cross_page, false);
}

static void jit_ldst(jit_t *j, uint32_t word, a64_dec_op_t dec) {
    uint32_t size = word >> 30;
    bool vector = ((word >> 26) & 1U) != 0;
    uint32_t opc = (word >> 22) & 3U;
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U;
    uint32_t scale = vector ? size | ((opc & 2U) << 1) : size;
    bool load = vector ? (opc & 1U) != 0 : opc != 0;
    uint32_t bytes = 1U << scale;
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t index = dec == A64_DEC_LDST_REGOFF ? jit_reg(j, (word >> 16) & 31U, false, true) : X_ZR;
    uint32_t data = vector ? rt : load ? jit_reg(j, rt, false, false) : jit_reg(j, rt, false, true);
    if (vector) {
        jit_fp(j, 1);
    }
    if (rn == 31) {
        jit_sp_check(j, base);
    }
    int64_t imm = cpu_sext((word >> 12) & 0x1FFU, 9);
    switch (dec) {
    case A64_DEC_LDST_POS:
        jit_add_imm(j, X_ADDR, base, (int64_t)((word >> 10) & 0xFFFU) << scale);
        break;
    case A64_DEC_LDST_IMMPOST:
        jit_emit(j, A_MOV(X_ADDR, base));
        break;
    case A64_DEC_LDST_REGOFF:
        jit_emit(j, A_ADDX(X_ADDR, base, index, (word >> 13) & 7U, ((word >> 12) & 1U) ? scale : 0U));
        break;
    default:
        jit_add_imm(j, X_ADDR, base, imm);
        break;
    }
    uint32_t bank = load ? X_TLB_R : X_TLB_W;
    if (dec == A64_DEC_LDST_UNPRIV) {
        jit_emit(j, A_ANDI(X_TMP1, X_TLB_R, ~(((uint64_t)SOFTTLB_BANKS << TLB_BANK_SHIFT) - 1U)));
        if (!load) {
            jit_emit(j, A_ADDI(X_TMP1, X_TMP1, 1U << (TLB_BANK_SHIFT - 12), 1));
        }
        bank = X_TMP1;
    }
    jit_tlb(j, bank, bytes, bytes, false);
    if (vector && !load) {
        jit_vector_use(j, rt);
    }
    jit_emit(j, (size << 30) | (7U << 27) | ((uint32_t)vector << 26) | (opc << 22) | (X_ADDR << 5) | data);
    if (vector && load) {
        j->regs.vloaded |= 1U << rt;
        j->regs.vdirty |= 1U << rt;
    }
    if (dec == A64_DEC_LDST_IMMPRE || dec == A64_DEC_LDST_IMMPOST) {
        jit_add_imm(j, base, base, imm);
        jit_dirty(j, rn, true);
    }
    if (!vector && load) {
        jit_dirty(j, rt, false);
    }
}

static void jit_pair(jit_t *j, uint32_t word, a64_dec_op_t dec) {
    uint32_t opc = word >> 30;
    bool vector = ((word >> 26) & 1U) != 0;
    bool load = ((word >> 22) & 1U) != 0;
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, rt2 = (word >> 10) & 31U;
    uint32_t scale = vector ? 2U + opc : 2U + (opc >> 1);
    int64_t imm = cpu_sext((word >> 15) & 0x7FU, 7) * (int64_t)(1U << scale);
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t first = rt, second = rt2;
    if (!vector) {
        if (load) {
            first = jit_reg(j, rt, false, false);
            second = rt2 == rt ? X_TMP2 : jit_reg(j, rt2, false, false);
        } else {
            first = jit_reg(j, rt, false, true);
            second = jit_reg(j, rt2, false, true);
        }
    }
    if (vector) {
        jit_fp(j, 1);
    }
    if (rn == 31) {
        jit_sp_check(j, base);
    }
    jit_add_imm(j, X_ADDR, base, dec == A64_DEC_LDST_PAIR_POST ? 0 : imm);
    jit_tlb(j, load ? X_TLB_R : X_TLB_W, 2U << scale, 1U << scale, false);
    if (vector && !load) {
        jit_vector_use(j, rt);
        jit_vector_use(j, rt2);
    }
    uint32_t host = (word & 0xFE400000U) | (dec == A64_DEC_LDST_NAPAIR ? 0U : 0x01000000U) | (second << 10) |
                    (X_ADDR << 5) | first;
    jit_emit(j, host);
    if (vector && load) {
        uint32_t written = (1U << rt) | (1U << rt2);
        j->regs.vloaded |= written;
        j->regs.vdirty |= written;
    }
    if (!vector && second == X_TMP2) {
        jit_emit(j, A_MOV(first, X_TMP2));
    }
    if (dec == A64_DEC_LDST_PAIR_PRE || dec == A64_DEC_LDST_PAIR_POST) {
        jit_add_imm(j, base, base, imm);
        jit_dirty(j, rn, true);
    }
    if (!vector && load) {
        jit_dirty(j, rt, false);
        jit_dirty(j, rt2, false);
    }
}

static void jit_literal(jit_t *j, uint32_t word) {
    static const uint32_t gpr[3] = {0xB8400000U, 0xF8400000U, 0xB8800000U};
    static const uint32_t simd[3] = {0xBC400000U, 0xFC400000U, 0x3CC00000U};
    static const uint8_t bytes[2][3] = {{4, 8, 4}, {4, 8, 16}};
    uint32_t opc = word >> 30, rt = word & 31U;
    bool vector = ((word >> 26) & 1U) != 0;
    uint32_t data = vector ? rt : jit_reg(j, rt, false, false);
    if (vector) {
        jit_fp(j, 1);
    }
    jit_add_imm(j, X_ADDR, X_VA, (int64_t)j->k * 4 + cpu_sext((word >> 5) & 0x7FFFFU, 19) * 4);
    jit_tlb(j, X_TLB_R, bytes[vector][opc], bytes[vector][opc], false);
    jit_emit(j, (vector ? simd[opc] : gpr[opc]) | (X_ADDR << 5) | data);
    if (vector) {
        j->regs.vloaded |= 1U << rt;
        j->regs.vdirty |= 1U << rt;
    } else {
        jit_dirty(j, rt, false);
    }
}

static void jit_atomic_base(jit_t *j, uint32_t rn, uint32_t base, bool write, uint32_t bytes) {
    if (rn == 31) {
        jit_sp_check(j, base);
    }
    jit_emit(j, A_MOV(X_ADDR, base));
    jit_tlb(j, write ? X_TLB_W : X_TLB_R, bytes, bytes, true);
}

static void jit_ordered(jit_t *j, uint32_t word) {
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U;
    bool load = ((word >> 22) & 1U) != 0;
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t data = load ? jit_reg(j, rt, false, false) : jit_reg(j, rt, false, true);
    jit_atomic_base(j, rn, base, !load, 1U << (word >> 30));
    jit_emit(j, (word & ~0x3FFU) | (1U << 15) | (X_ADDR << 5) | data);
    if (load) {
        jit_dirty(j, rt, false);
    } else {
        jit_wake(j);
    }
}

static void jit_rcpc(jit_t *j, uint32_t word) {
    static const uint32_t extend[4][4] = {
        [2] = {0x93401C00U, 0x93403C00U, 0x93407C00U},
        [3] = {0x13001C00U, 0x13003C00U},
    };
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, opc = (word >> 22) & 3U, size = word >> 30;
    bool load = opc != 0U;
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t data = load ? jit_reg(j, rt, false, false) : jit_reg(j, rt, false, true);
    if (rn == 31) {
        jit_sp_check(j, base);
    }
    jit_add_imm(j, X_ADDR, base, cpu_sext((word >> 12) & 0x1FFU, 9));
    jit_tlb(j, load ? X_TLB_R : X_TLB_W, 1U << size, 1U << size, true);
    jit_emit(j, (load ? 0x08DFFC00U : 0x089FFC00U) | (size << 30) | (X_ADDR << 5) | data);
    if (load) {
        if (extend[opc][size]) {
            jit_emit(j, extend[opc][size] | (data << 5) | data);
        }
        jit_dirty(j, rt, false);
    } else {
        jit_wake(j);
    }
}

static void jit_exclusive_status(jit_t *j, uint32_t rs, uint32_t status) {
    jit_emit(j, A_STRB(X_ZR, X_CPU, CPU_EXCL_VALID));
    jit_emit(j, A_STRX(X_ZR, X_CPU, CPU_EXCL_HVA));
    jit_emit(j, A_MOV(status, X_TMP));
    jit_dirty(j, rs, false);
    uint32_t skip = j->n;
    jit_emit(j, A_CBZ(X_TMP, false, true, 0));
    jit_wake(j);
    jit_patch(j, skip, j->n);
}

static void jit_exclusive_check(jit_t *j, uint32_t bytes, uint32_t fail[3]) {
    jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_EXCL_VALID));
    fail[0] = j->n;
    jit_emit(j, A_CBZ(X_TMP2, false, false, 0));
    jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_EXCL_SIZE));
    jit_emit(j, A_EORWI(X_TMP2, X_TMP2, bytes));
    fail[1] = j->n;
    jit_emit(j, A_CBZ(X_TMP2, false, true, 0));
    jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_EXCL_HVA));
    jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_ADDR));
    fail[2] = j->n;
    jit_emit(j, A_CBZ(X_TMP2, true, true, 0));
}

static void jit_exclusive_finish(jit_t *j, const uint32_t fail[3]) {
    jit_emit(j, A_NEG(X_TMP, X_TMP2));
    jit_emit(j, A_ORR(X_TMP, X_TMP, X_TMP2, 0));
    jit_emit(j, A_LSRI(X_TMP, X_TMP, 63));
    uint32_t done = j->n;
    jit_emit(j, A_B(0));
    for (uint32_t i = 0; i < 3; ++i) {
        jit_patch(j, fail[i], j->n);
    }
    jit_emit(j, A_MOVZW(X_TMP, 1));
    jit_patch(j, done, j->n);
}

static void jit_exclusive_load_state(jit_t *j, uint32_t bytes) {
    jit_emit(j, A_MOVZW(X_TMP, 1));
    jit_emit(j, A_STRB(X_TMP, X_CPU, CPU_EXCL_VALID));
    jit_emit(j, A_MOVZW(X_TMP, bytes));
    jit_emit(j, A_STRB(X_TMP, X_CPU, CPU_EXCL_SIZE));
    jit_emit(j, A_STRX(X_ADDR, X_CPU, CPU_EXCL_HVA));
}

static void jit_exclusive(jit_t *j, uint32_t word) {
    uint32_t size = word >> 30, bytes = 1U << size;
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, rs = (word >> 16) & 31U;
    bool load = ((word >> 22) & 1U) != 0, ordered = ((word >> 15) & 1U) != 0;
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t data = load ? jit_reg(j, rt, false, false) : jit_reg(j, rt, false, true);
    uint32_t status = load ? X_ZR : jit_reg(j, rs, false, false);
    jit_atomic_base(j, rn, base, !load, bytes);
    if (load) {
        static const uint32_t plain[4] = {0x39400000U, 0x79400000U, 0xB9400000U, 0xF9400000U};
        jit_emit(j, (ordered ? 0x08DFFC00U | (size << 30) : plain[size]) | (X_ADDR << 5) | X_TMP2);
        jit_emit(j, A_MOV(data, X_TMP2));
        jit_emit(j, A_STRX(X_TMP2, X_CPU, CPU_EXCL_VAL));
        jit_emit(j, A_STRX(X_ZR, X_CPU, CPU_EXCL_VAL + 8U));
        jit_exclusive_load_state(j, bytes);
        jit_dirty(j, rt, false);
        return;
    }
    uint32_t fail[3];
    jit_exclusive_check(j, bytes, fail);
    jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_EXCL_VAL));
    jit_emit(j, A_MOV(X_TMP1, X_TMP2));
    jit_emit(j, 0x08A07C00U | (size << 30) | (ordered ? (1U << 22) | (1U << 15) : 0U) | (X_TMP2 << 16) |
                    (X_ADDR << 5) | data);
    jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_TMP1));
    if (bytes < 8) {
        jit_emit(j, A_UBFX(X_TMP2, X_TMP2, 0, bytes * 8U));
    }
    jit_exclusive_finish(j, fail);
    jit_exclusive_status(j, rs, status);
}

static void jit_exclusive_pair(jit_t *j, uint32_t word) {
    bool wide = ((word >> 30) & 1U) != 0;
    uint32_t bytes = wide ? 16U : 8U;
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, rt2 = (word >> 10) & 31U, rs = (word >> 16) & 31U;
    bool load = ((word >> 22) & 1U) != 0, ordered = ((word >> 15) & 1U) != 0;
    if (!load && wide) {
        jit_reserve(j, 0xFU);
    }
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t first, second, status = X_ZR;
    if (load) {
        first = jit_reg(j, rt, false, false);
        second = jit_reg(j, rt2, false, false);
    } else {
        first = jit_reg(j, rt, false, true);
        second = jit_reg(j, rt2, false, true);
        status = jit_reg(j, rs, false, false);
    }
    jit_atomic_base(j, rn, base, !load, bytes);
    if (load) {
        if (wide) {
            jit_emit(j, A_LDPX(X_TMP2, X_TMP, X_ADDR, 0));
            if (ordered) {
                jit_emit(j, A_DMB_ISHLD);
            }
            jit_emit(j, A_MOV(first, X_TMP2));
            jit_emit(j, A_MOV(second, X_TMP));
        } else {
            jit_emit(j, (ordered ? 0xC8DFFC00U : 0xF9400000U) | (X_ADDR << 5) | X_TMP2);
            jit_emit(j, A_MOVW(first, X_TMP2));
            jit_emit(j, A_LSRI(second, X_TMP2, 32));
        }
        jit_emit(j, A_STRX(X_TMP2, X_CPU, CPU_EXCL_VAL));
        jit_emit(j, A_STRX(wide ? X_TMP : X_ZR, X_CPU, CPU_EXCL_VAL + 8U));
        jit_exclusive_load_state(j, bytes);
        jit_dirty(j, rt, false);
        jit_dirty(j, rt2, false);
        return;
    }
    uint32_t fail[3];
    jit_exclusive_check(j, bytes, fail);
    uint32_t order = ordered ? (1U << 22) | (1U << 15) : 0U;
    if (wide) {
        jit_emit(j, A_MOV(2, first));
        jit_emit(j, A_MOV(3, second));
        jit_emit(j, A_LDRX(0, X_CPU, CPU_EXCL_VAL));
        jit_emit(j, A_LDRX(1, X_CPU, CPU_EXCL_VAL + 8U));
        jit_emit(j, A_MOV(X_TMP, 0));
        jit_emit(j, A_MOV(X_TMP1, 1));
        jit_emit(j, 0x48207C00U | order | (0U << 16) | (X_ADDR << 5) | 2U);
        jit_emit(j, A_EOR(X_TMP2, X_TMP, 0));
        jit_emit(j, A_EOR(X_TMP, X_TMP1, 1));
        jit_emit(j, A_ORR(X_TMP2, X_TMP2, X_TMP, 0));
    } else {
        jit_emit(j, A_MOVW(X_TMP, first));
        jit_emit(j, A_ORR(X_TMP, X_TMP, second, 32));
        jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_EXCL_VAL));
        jit_emit(j, A_MOV(X_TMP1, X_TMP2));
        jit_emit(j, 0xC8A07C00U | order | (X_TMP2 << 16) | (X_ADDR << 5) | X_TMP);
        jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_TMP1));
    }
    jit_exclusive_finish(j, fail);
    jit_exclusive_status(j, rs, status);
}

static void jit_atomic(jit_t *j, uint32_t word) {
    uint32_t offset;
    const cpu_pt_leaf_t *leaf = cpu_pt_lookup(word, &offset);
    uint32_t host[4] = {0};
    uint32_t regs[4] = {0};
    bool writes = false;
    uint32_t base = 0, rn = 31;
    for (uint32_t i = 0; i < 4; ++i) {
        regs[i] = (word >> leaf->slots[i].start) & ((1U << leaf->slots[i].width) - 1U);
        if (leaf->slots[i].kind == CPU_PT_XR) {
            host[i] = jit_reg(j, regs[i], false, true);
            writes = true;
        }
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (leaf->slots[i].kind == CPU_PT_XB) {
            rn = regs[i];
            base = jit_reg(j, rn, true, true);
            host[i] = X_ADDR;
        } else if (leaf->slots[i].kind == CPU_PT_XW) {
            host[i] = jit_reg(j, regs[i], false, false);
        }
    }
    jit_atomic_base(j, rn, base, writes, 1U << (word >> 30));
    for (uint32_t i = 0; i < 4; ++i) {
        if (leaf->slots[i].kind != CPU_PT_NONE) {
            word = jit_field(word, leaf->slots[i].start, leaf->slots[i].width, host[i]);
        }
    }
    jit_emit(j, word);
    for (uint32_t i = 0; i < 4; ++i) {
        if (leaf->slots[i].kind == CPU_PT_XW) {
            jit_dirty(j, regs[i], false);
        }
    }
    if (writes) {
        jit_wake(j);
    }
}

static void jit_compare_swap_pair(jit_t *j, uint32_t word) {
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, rs = (word >> 16) & 31U;
    jit_reserve(j, 0xFU);
    uint32_t base = jit_reg(j, rn, true, true);
    jit_read_into(j, 0, rs);
    jit_read_into(j, 1, rs + 1U);
    jit_read_into(j, 2, rt);
    jit_read_into(j, 3, rt + 1U);
    jit_atomic_base(j, rn, base, true, ((word >> 30) & 1U) ? 16U : 8U);
    jit_emit(j, (word & ~0x001F03FFU) | (X_ADDR << 5) | 2U);
    jit_write_from(j, rs, 0);
    jit_write_from(j, rs + 1U, 1);
    jit_wake(j);
}

static void jit_vector_structure(jit_t *j, uint32_t word, bool single) {
    bool load = ((word >> 22) & 1U) != 0;
    bool post = ((word >> 23) & 1U) != 0;
    uint32_t rt = word & 31U, rn = (word >> 5) & 31U, rm = (word >> 16) & 31U;
    uint32_t count, bytes, align;
    if (single) {
        uint32_t opcode = (word >> 13) & 7U, size = (word >> 10) & 3U;
        uint32_t scale = opcode >> 1;
        count = (((opcode & 1U) << 1) | ((word >> 21) & 1U)) + 1U;
        align = scale == 3U ? 1U << size : scale == 2U && (size & 1U) ? 8U : 1U << scale;
        bytes = count * align;
    } else {
        count = jit_multi_regs(word);
        align = 1U << ((word >> 10) & 3U);
        bytes = count * (((word >> 30) & 1U) ? 16U : 8U);
    }
    uint32_t base = jit_reg(j, rn, true, true);
    uint32_t index = post && rm != 31 ? jit_reg(j, rm, false, true) : X_ZR;
    jit_fp(j, 1);
    if (rn == 31) {
        jit_sp_check(j, base);
    }
    jit_emit(j, A_MOV(X_ADDR, base));
    jit_tlb(j, load ? X_TLB_R : X_TLB_W, bytes, align, false);
    uint32_t list = 0;
    for (uint32_t i = 0; i < count; ++i) {
        list |= 1U << ((rt + i) & 31U);
        if (single || !load) {
            jit_vector_use(j, (rt + i) & 31U);
        }
    }
    jit_emit(j, (word & ~((1U << 23) | 0x001F03E0U)) | (X_ADDR << 5));
    if (load) {
        j->regs.vloaded |= list;
        j->regs.vdirty |= list;
    }
    if (post) {
        if (rm == 31) {
            jit_add_imm(j, base, base, bytes);
        } else {
            jit_emit(j, A_ADDR(base, base, index, 0));
        }
        jit_dirty(j, rn, true);
    }
}

static void jit_simd(jit_t *j, uint32_t word) {
    uint32_t offset;
    const cpu_pt_leaf_t *leaf = cpu_pt_lookup(word, &offset);
    bool wide = (offset & CPU_PT_WIDE_RM) != 0;
    uint32_t regs[4] = {0}, host[4] = {0};
    for (uint32_t i = 0; i < 4; ++i) {
        regs[i] = (word >> leaf->slots[i].start) & ((1U << leaf->slots[i].width) - 1U);
        if (i == 2 && wide) {
            regs[i] |= ((word >> 20) & 1U) << 4;
        }
        if (leaf->slots[i].kind == CPU_PT_XR) {
            host[i] = jit_reg(j, regs[i], false, true);
        }
    }
    for (uint32_t i = 0; i < 4; ++i) {
        if (leaf->slots[i].kind == CPU_PT_XW) {
            host[i] = jit_reg(j, regs[i], false, false);
        }
    }
    jit_fp(j, 2);
    for (uint32_t i = 0; i < 4; ++i) {
        if (leaf->slots[i].kind == CPU_PT_V) {
            jit_vector_use(j, regs[i]);
        } else if (leaf->slots[i].kind != CPU_PT_NONE) {
            word = jit_field(word, leaf->slots[i].start, leaf->slots[i].width, host[i]);
        }
    }
    jit_emit(j, word);
    if (leaf->slots[0].kind == CPU_PT_V) {
        j->regs.vdirty |= 1U << regs[0];
    } else if (leaf->slots[0].kind == CPU_PT_XW) {
        jit_dirty(j, regs[0], false);
    }
}

static void jit_simd_imm(jit_t *j, uint32_t word) {
    uint32_t rd = word & 31U;
    jit_fp(j, 2);
    jit_vector_use(j, rd);
    jit_emit(j, word);
    j->regs.vdirty |= 1U << rd;
}

static void jit_table(jit_t *j, uint32_t word) {
    uint32_t rd = word & 31U, rn = (word >> 5) & 31U, rm = (word >> 16) & 31U;
    jit_fp(j, 2);
    jit_vector_use(j, rm);
    for (uint32_t i = 0; i <= ((word >> 13) & 3U); ++i) {
        jit_vector_use(j, (rn + i) & 31U);
    }
    if (word & (1U << 12)) {
        jit_vector_use(j, rd);
    }
    jit_emit(j, word);
    j->regs.vloaded |= 1U << rd;
    j->regs.vdirty |= 1U << rd;
}

static void jit_mops(jit_t *j, uint32_t word, a64_dec_op_t dec) {
    bool copy = dec == A64_DEC_MOPS_CPY;
    uint32_t stage = copy ? (word >> 22) & 3U : (word >> 14) & 3U;
    uint32_t unprivileged = (word >> 12) & (copy ? 3U : 1U);
    uint32_t rd = word & 31U, rn = (word >> 5) & 31U, rs = (word >> 16) & 31U;
    uint32_t xn = jit_reg(j, rn, false, true);
    jit_el0_check(j, CPU_SCTLR, 33);
    if (stage != 0) {
        jit_to_stub(j, A_CBZ(xn, true, true, 0), jit_cold(j));
        return;
    }
    if (unprivileged != 0 && j->pg->el != 0) {
        jit_helper(j);
        return;
    }
    uint32_t xd = jit_reg(j, rd, false, true), xs = jit_reg(j, rs, false, true);
    uint32_t cold = jit_cold(j);
    jit_emit(j, A_SUBI(X_TMP1, xn, 1, 0));
    jit_emit(j, A_LSRI(X_TMP2, X_TMP1, 12));
    jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
    for (uint32_t i = 0; i < (copy ? 2U : 1U); ++i) {
        jit_emit(j, A_ANDI(X_TMP2, i == 0 ? xd : xs, VMM_PAGE_MASK));
        jit_emit(j, A_ADDR(X_TMP2, X_TMP2, X_TMP1, 0));
        jit_emit(j, A_LSRI(X_TMP2, X_TMP2, 12));
        jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
    }
    if (copy) {
        jit_emit(j, A_SUBR(X_TMP2, xd, xs));
        jit_emit(j, A_ANDI(X_TMP2, X_TMP2, 0x00FFFFFFFFFFFFFFULL));
        jit_emit(j, A_SUBI(X_TMP2, X_TMP2, 1, 0));
        jit_emit(j, A_LSRI(X_TMP, X_TMP2, 12));
        jit_emit(j, A_CBZ(X_TMP, true, true, 3));
        jit_emit(j, A_SUBR(X_TMP2, X_TMP2, X_TMP1));
        jit_to_stub(j, A_TBZ(X_TMP2, 63, true, 0), cold);
        jit_emit(j, A_MOV(X_ADDR, xs));
        jit_tlb(j, X_TLB_R, 1, 1, false);
        jit_emit(j, A_MOV(X_TMP1, X_ADDR));
    }
    jit_emit(j, A_MOV(X_ADDR, xd));
    jit_tlb(j, X_TLB_W, 1, 1, false);
    if (!copy) {
        jit_emit(j, A_ANDI(X_TMP, xs, 0xFF));
        for (uint32_t shift = 8; shift < 64; shift *= 2) {
            jit_emit(j, A_ORR(X_TMP, X_TMP, X_TMP, shift));
        }
    }
    jit_emit(j, A_LSRI(X_TMP2, xn, 4));
    jit_emit(j, A_CBZ(X_TMP2, true, false, copy ? 5 : 4));
    if (copy) {
        jit_emit(j, A_LDPX_POST(X_TMP, X_LR, X_TMP1, 16));
        jit_emit(j, A_STPX_POST(X_TMP, X_LR, X_ADDR, 16));
    } else {
        jit_emit(j, A_STPX_POST(X_TMP, X_TMP, X_ADDR, 16));
    }
    jit_emit(j, A_SUBI(X_TMP2, X_TMP2, 1, 0));
    jit_emit(j, A_CBZ(X_TMP2, true, true, copy ? -3 : -2));
    jit_emit(j, A_ANDI(X_TMP2, xn, 15));
    jit_emit(j, A_CBZ(X_TMP2, true, false, copy ? 5 : 4));
    if (copy) {
        jit_emit(j, A_LDRB_POST(X_TMP, X_TMP1, 1));
    }
    jit_emit(j, A_STRB_POST(X_TMP, X_ADDR, 1));
    jit_emit(j, A_SUBI(X_TMP2, X_TMP2, 1, 0));
    jit_emit(j, A_CBZ(X_TMP2, true, true, copy ? -3 : -2));
    jit_emit(j, A_ADDR(xd, xd, xn, 0));
    if (copy) {
        jit_emit(j, A_ADDR(xs, xs, xn, 0));
        jit_dirty(j, rs, false);
    }
    jit_emit(j, A_MOV(xn, X_ZR));
    jit_emit(j, A_MSR_NZCV(X_ZR));
    jit_dirty(j, rd, false);
    jit_dirty(j, rn, false);
}

static void jit_daif(jit_t *j, uint32_t word) {
    uint32_t crm = (word >> 8) & 15U;
    bool set = ((word >> 5) & 7U) == 6U;
    jit_el0_check(j, CPU_SCTLR, 9);
    if (!set) {
        jit_emit(j, A_ADDI(X_TMP2, X_CPU, CPU_IRQ, 0));
        jit_emit(j, A_LDARW(X_TMP2, X_TMP2));
        jit_emit(j, A_LDRB(X_TMP, X_CPU, CPU_IRQ_MASK));
        jit_emit(j, A_ANDW(X_TMP2, X_TMP2, X_TMP));
        jit_to_stub(j, A_CBZ(X_TMP2, false, true, 0), jit_cold(j));
    }
    jit_emit(j, A_LDRW(X_TMP2, X_CPU, CPU_DAIF));
    jit_emit(j, A_MOVZW(X_TMP, crm << 6));
    jit_emit(j, set ? A_ORRW(X_TMP2, X_TMP2, X_TMP) : A_BICW(X_TMP2, X_TMP2, X_TMP));
    jit_emit(j, A_STRW(X_TMP2, X_CPU, CPU_DAIF));
}

static void jit_mrs(jit_t *j, uint32_t word) {
    uint32_t offset = 0, rt = word & 31U;
    jit_sysreg_t kind = jit_sysreg(word, j->pg, &offset);
    if (kind == JIT_SR_HELPER) {
        jit_helper(j);
        return;
    }
    if (kind == JIT_SR_CNTVCT || kind == JIT_SR_CNTPCT) {
        jit_el0_check(j, CPU_CNTKCTL, kind == JIT_SR_CNTVCT ? 1 : 0);
        uint32_t base = kind == JIT_SR_CNTVCT ? CPU_CNTV_BASE : CPU_CNTP_BASE;
        if (vmm_timer_native()) {
            uint32_t data = jit_reg(j, rt, false, false);
            jit_interp(j, FAST_cntvct_read, true);
            jit_emit(j, A_LDRX(X_TMP, X_CPU, base));
            jit_emit(j, A_SUBR(data, X_TMP2, X_TMP));
            jit_dirty(j, rt, false);
            return;
        }
        jit_flush(j, &j->regs);
        jit_regs_clear(&j->regs);
        jit_emit(j, A_MRS_NZCV(X_TMP));
        jit_emit(j, A_STRW(X_TMP, X_CPU, CPU_NZCV));
        jit_jump(j, (uintptr_t)vmm_timer_ticks, true);
        jit_emit(j, A_LDRX(X_TMP, X_CPU, base));
        jit_emit(j, A_SUBR(0, 0, X_TMP));
        jit_emit(j, A_LDRW(X_TMP, X_CPU, CPU_NZCV));
        jit_emit(j, A_MSR_NZCV(X_TMP));
        if (rt != 31) {
            jit_emit(j, A_STRX(0, X_CPU, rt * 8U));
        }
        return;
    }
    uint32_t data = rt == 31 ? X_TMP1 : jit_reg(j, rt, false, false);
    switch (kind) {
    case JIT_SR_NZCV:
        jit_emit(j, A_MRS_NZCV(data));
        break;
    case JIT_SR_FPCR:
        jit_fp(j, 1);
        jit_emit(j, A_LDRW(data, X_CPU, CPU_FPCR));
        break;
    case JIT_SR_FPSR:
        jit_fp(j, 2);
        jit_emit(j, A_MRS_FPSR | X_TMP2);
        jit_emit(j, A_LDRW(X_TMP, X_CPU, CPU_FPSR));
        jit_emit(j, A_ORRW(X_TMP2, X_TMP2, X_TMP));
        jit_mov_imm(j, X_TMP, CPU_FPSR_MASK);
        jit_emit(j, A_ANDW(X_TMP2, X_TMP2, X_TMP));
        jit_emit(j, A_STRW(X_TMP2, X_CPU, CPU_FPSR));
        jit_emit(j, A_MOV(data, X_TMP2));
        break;
    case JIT_SR_DAIF:
        jit_el0_check(j, CPU_SCTLR, 9);
        jit_emit(j, A_LDRW(X_TMP2, X_CPU, CPU_DAIF));
        jit_emit(j, A_ANDWI(data, X_TMP2, 0x3C0U));
        break;
    case JIT_SR_SP_EL0:
        jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_SPSEL));
        jit_to_stub(j, A_CBZ(X_TMP2, false, false, 0), jit_cold(j));
        jit_emit(j, A_LDRX(data, X_CPU, CPU_SP));
        break;
    case JIT_SR_CNTFRQ:
        if (j->pg->el == 0) {
            jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_CNTKCTL));
            jit_emit(j, A_ANDI(X_TMP2, X_TMP2, 3));
            jit_to_stub(j, A_CBZ(X_TMP2, true, false, 0), jit_cold(j));
        }
        jit_mov_imm(j, data, VMM_TIMER_FREQ);
        break;
    case JIT_SR_DCZID:
        jit_emit(j, A_MOVZW(data, VMM_DCZID_BS));
        if (j->pg->el == 0) {
            jit_emit(j, A_LDRX(X_TMP2, X_CPU, CPU_SCTLR));
            jit_emit(j, A_UBFX(X_TMP2, X_TMP2, 14, 1));
            jit_emit(j, A_EORWI(X_TMP2, X_TMP2, 1));
            jit_emit(j, A_ORR(data, data, X_TMP2, 4));
        }
        break;
    case JIT_SR_CURRENTEL:
        jit_mov_imm(j, data, (uint64_t)(j->pg->el != 0 && j->pg->world != VCPU_WORLD_GUEST ? 2U : j->pg->el) << 2);
        break;
    default:
        jit_emit(j, A_LDRX(data, X_CPU, offset));
        break;
    }
    jit_dirty(j, rt, false);
}

static void jit_msr(jit_t *j, uint32_t word) {
    uint32_t offset = 0;
    jit_sysreg_t kind = jit_sysreg(word, j->pg, &offset);
    if (kind == JIT_SR_HELPER) {
        jit_helper(j);
        return;
    }
    uint32_t data = jit_reg(j, word & 31U, false, true);
    switch (kind) {
    case JIT_SR_NZCV:
        jit_emit(j, A_MSR_NZCV(data));
        break;
    case JIT_SR_FPCR:
    case JIT_SR_FPSR:
        jit_fp(j, 2);
        jit_mov_imm(j, X_TMP, kind == JIT_SR_FPCR ? CPU_FPCR_MASK : CPU_FPSR_MASK);
        jit_emit(j, A_ANDW(X_TMP2, data, X_TMP));
        jit_emit(j, A_STRW(X_TMP2, X_CPU, kind == JIT_SR_FPCR ? CPU_FPCR : CPU_FPSR));
        jit_emit(j, (kind == JIT_SR_FPCR ? A_MSR_FPCR : A_MSR_FPSR) | X_TMP2);
        break;
    case JIT_SR_DAIF:
        jit_el0_check(j, CPU_SCTLR, 9);
        jit_emit(j, A_MOVZW(X_TMP, 0x3C0U));
        jit_emit(j, A_ANDW(X_TMP2, data, X_TMP));
        jit_emit(j, A_TBZ(X_TMP2, 7, true, 6));
        jit_emit(j, A_ADDI(X_TMP, X_CPU, CPU_IRQ, 0));
        jit_emit(j, A_LDARW(X_TMP, X_TMP));
        jit_emit(j, A_LDRB(X_TMP1, X_CPU, CPU_IRQ_MASK));
        jit_emit(j, A_ANDW(X_TMP, X_TMP, X_TMP1));
        jit_to_stub(j, A_CBZ(X_TMP, false, true, 0), jit_cold(j));
        jit_emit(j, A_STRW(X_TMP2, X_CPU, CPU_DAIF));
        break;
    case JIT_SR_SP_EL0:
        jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_SPSEL));
        jit_to_stub(j, A_CBZ(X_TMP2, false, false, 0), jit_cold(j));
        jit_emit(j, A_STRX(data, X_CPU, CPU_SP));
        break;
    default:
        jit_emit(j, A_STRX(data, X_CPU, offset));
        break;
    }
}

static void jit_sys(jit_t *j, uint32_t word) {
    switch (jit_sysop(word, j->pg)) {
    case JIT_SYS_ZVA: {
        uint32_t data = jit_reg(j, word & 31U, false, true);
        jit_el0_check(j, CPU_SCTLR, 14);
        jit_emit(j, A_MOVN(X_TMP2, (4U << VMM_DCZID_BS) - 1U, 0));
        jit_emit(j, A_AND(X_ADDR, data, X_TMP2));
        jit_tlb(j, X_TLB_W, 1, 1, false);
        uint64_t dczid;
        __asm__("mrs %0, dczid_el0" : "=r"(dczid));
        if ((dczid & 0x1FU) == VMM_DCZID_BS) {
            jit_emit(j, A_DC_ZVA | X_ADDR);
            break;
        }
        for (uint32_t offset = 0; offset < (4U << VMM_DCZID_BS); offset += 16) {
            jit_emit(j, A_STPX(X_ZR, X_ZR, X_ADDR, offset));
        }
        break;
    }
    case JIT_SYS_DC_EL0:
        jit_el0_check(j, CPU_SCTLR, 26);
        break;
    default:
        break;
    }
}

static void jit_barrier(jit_t *j, uint32_t word, a64_dec_op_t dec) {
    if (dec == A64_DEC_BARRIERS_DMB) {
        static const uint32_t domains[4] = {A_DMB_ISH, A_DMB_ISHLD, A_DMB_ISHST, A_DMB_ISH};
        jit_emit(j, domains[(word >> 8) & 3U]);
        return;
    }
    uint32_t cold = jit_cold(j);
    if (dec == A64_DEC_BARRIERS_DSB) {
        jit_emit(j, A_DMB_ISH);
    }
    jit_emit(j, A_LDRB(X_TMP2, X_CPU, CPU_ICACHE_SHOOTDOWN));
    jit_to_stub(j, A_CBZ(X_TMP2, false, true, 0), cold);
    jit_emit(j, A_LDRX(X_TMP1, X_CPU, CPU_ICACHE));
    jit_emit(j, A_LDRX(X_TMP2, X_TMP1, IC_CUR_EXEC_EPOCH));
    jit_emit(j, A_LDRX(X_TMP, X_CPU, CPU_EXEC_EPOCH));
    jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_TMP));
    jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
    jit_emit(j, A_LDRX(X_TMP2, X_TMP1, IC_CUR_GEN));
    jit_emit(j, A_SUBI(X_TMP, X_PAGE, PAGE_INSNS, 0));
    jit_emit(j, A_LDRW(X_TMP, X_TMP, PAGE_GEN));
    jit_emit(j, A_EOR(X_TMP2, X_TMP2, X_TMP));
    jit_to_stub(j, A_CBZ(X_TMP2, true, true, 0), cold);
}

static bool jit_insn(jit_t *j, uint32_t word, const ir_insn_t *ir, bool *ended) {
    a64_dec_op_t dec = a64_decode_word(word);
    jit_kind_t kind = jit_classify(word, ir, j->pg);
    switch (kind) {
    case JIT_NONE:
        return false;
    case JIT_NOP:
        break;
    case JIT_DP:
        jit_dp(j, word, dec);
        break;
    case JIT_ADR:
        jit_adr(j, word);
        break;
    case JIT_B:
        jit_goto(j, jit_branch_target(j, word, dec), &j->regs);
        *ended = true;
        break;
    case JIT_BL: {
        uint32_t lr = jit_reg(j, 30, false, false);
        jit_add_imm(j, lr, X_VA, (int64_t)(j->k + 1U) * 4);
        jit_dirty(j, 30, false);
        jit_push_return(j);
        jit_goto(j, jit_branch_target(j, word, dec), &j->regs);
        *ended = true;
        break;
    }
    case JIT_BCOND: {
        int64_t target = jit_branch_target(j, word, dec);
        if ((word & 15U) >= 14U) {
            jit_goto(j, target, &j->regs);
            *ended = true;
        } else {
            jit_to_stub(j, A_BCOND(word & 15U), jit_stub(j, JIT_STUB_GOTO, (uint64_t)target));
        }
        break;
    }
    case JIT_CB: {
        uint32_t reg = jit_reg(j, word & 31U, false, true);
        jit_to_stub(j, A_CBZ(reg, (word >> 31) != 0, ((word >> 24) & 1U) != 0, 0),
                    jit_stub(j, JIT_STUB_GOTO, (uint64_t)jit_branch_target(j, word, dec)));
        break;
    }
    case JIT_TB: {
        uint32_t reg = jit_reg(j, word & 31U, false, true);
        uint32_t bit = ((word >> 31) << 5) | ((word >> 19) & 31U);
        jit_emit(j, A_TBZ(reg, bit, ((word >> 24) & 1U) == 0, 2));
        jit_to_stub(j, A_B(0), jit_stub(j, JIT_STUB_GOTO, (uint64_t)jit_branch_target(j, word, dec)));
        break;
    }
    case JIT_BR:
    case JIT_BLR:
    case JIT_RET:
        jit_branch_reg(j, word, kind);
        *ended = true;
        break;
    case JIT_LDST:
        jit_ldst(j, word, dec);
        break;
    case JIT_PAIR:
        jit_pair(j, word, dec);
        break;
    case JIT_LIT:
        jit_literal(j, word);
        break;
    case JIT_ORD:
        jit_ordered(j, word);
        break;
    case JIT_RCPC:
        jit_rcpc(j, word);
        break;
    case JIT_EXCL:
        jit_exclusive(j, word);
        break;
    case JIT_EXCLP:
        jit_exclusive_pair(j, word);
        break;
    case JIT_ATOMIC:
        jit_atomic(j, word);
        break;
    case JIT_CASP:
        jit_compare_swap_pair(j, word);
        break;
    case JIT_VMULTI:
        jit_vector_structure(j, word, false);
        break;
    case JIT_VSINGLE:
        jit_vector_structure(j, word, true);
        break;
    case JIT_MOPS:
        jit_mops(j, word, dec);
        break;
    case JIT_SIMD:
        jit_simd(j, word);
        break;
    case JIT_SIMD_IMM:
        jit_simd_imm(j, word);
        break;
    case JIT_TBL:
        jit_table(j, word);
        break;
    case JIT_HELPER:
        jit_helper(j);
        break;
    case JIT_SEVL:
        jit_emit(j, A_MOVZW(X_TMP2, 1));
        jit_emit(j, A_ADDI(X_ADDR, X_CPU, offsetof(vcpu_t, event_reg), 0));
        jit_emit(j, A_STLRB(X_TMP2, X_ADDR));
        break;
    case JIT_BARRIER:
        jit_barrier(j, word, dec);
        break;
    case JIT_CLREX:
        jit_emit(j, A_STRB(X_ZR, X_CPU, CPU_EXCL_VALID));
        jit_emit(j, A_STRX(X_ZR, X_CPU, CPU_EXCL_HVA));
        break;
    case JIT_DAIF:
        jit_daif(j, word);
        break;
    case JIT_MRS:
        jit_mrs(j, word);
        break;
    case JIT_MSR:
        jit_msr(j, word);
        break;
    case JIT_SYS:
        jit_sys(j, word);
        break;
    }
    return true;
}

static void jit_emit_stub(jit_t *j, const jit_stub_t *stub) {
    j->k = stub->insn;
    switch (stub->kind) {
    case JIT_STUB_COLD:
        jit_flush(j, &stub->regs);
        jit_add_imm(j, X_IR, X_PAGE, (int64_t)stub->insn * IR_SIZE);
        jit_interp(j, FAST_cold, false);
        break;
    case JIT_STUB_GOTO:
        jit_goto(j, (int64_t)stub->target, &stub->regs);
        break;
    case JIT_STUB_RETURN:
        jit_interp(j, FAST_cold_return, false);
        break;
    case JIT_STUB_WAKE:
    case JIT_STUB_FP:
        jit_flush(j, &stub->regs);
        jit_emit(j, A_MRS_NZCV(X_TMP));
        jit_emit(j, A_STRW(X_TMP, X_CPU, CPU_NZCV));
        if (stub->kind == JIT_STUB_WAKE) {
            jit_emit(j, A_LDRX(0, X_CPU, CPU_VM));
            jit_emit(j, A_MOV(1, X_ADDR));
            jit_jump(j, (uintptr_t)vmm_vcpu_wfe_notify_write, true);
        } else {
            jit_emit(j, A_MOV(0, X_CPU));
            jit_jump(j, (uintptr_t)cpu_fp_enter_guest_slow, true);
        }
        jit_emit(j, A_LDRW(X_TMP, X_CPU, CPU_NZCV));
        jit_emit(j, A_MSR_NZCV(X_TMP));
        jit_reload(j, &stub->regs);
        jit_emit(j, A_B((int32_t)stub->resume - (int32_t)j->n));
        break;
    }
}

static void jit_prologue(jit_t *j) {
    uint32_t next = 0;
    for (uint32_t slot = 0; slot < SLOTS; ++slot) {
        if ((j->used & (1ULL << slot)) == 0) {
            continue;
        }
        uint32_t host = jit_pool[next++];
        j->regs.host[slot] = (int8_t)host;
        j->regs.guest[host] = (int8_t)slot;
        j->regs.dirty |= 1ULL << slot;
    }
    jit_reload(j, &j->regs);
    j->regs.vloaded = j->vused;
    j->regs.vdirty = j->vused;
    jit_vector_spill(j, j->vused, true);
    j->head = j->n;
    j->head_regs = j->regs;
}

static uint32_t jit_pass(jit_t *j) {
    ir_page_t *pg = j->pg;
    j->n = 0;
    j->stubs = 0;
    j->fixups = 0;
    j->sites = 0;
    j->overflow = false;
    j->fp = 0;
    j->clock = 0;
    memset(j->stamp, 0, sizeof(j->stamp));
    jit_regs_clear(&j->regs);
    if (j->loop) {
        jit_prologue(j);
    }
    uint32_t k = j->start;
    bool ended = false;
    while (!ended && k < IR_PAGE_INSNS && k - j->start < JIT_BLOCK_INSNS) {
        const ir_insn_t *ir = &pg->insns[k];
        uint32_t word = pg->words[k];
        j->k = k;
        j->cold = -1;
        j->locked = 0;
        if (!jit_insn(j, word, ir, &ended)) {
            break;
        }
        j->vused |= j->regs.vloaded;
        ++k;
    }
    if (k == j->start) {
        return 0;
    }
    if (!ended) {
        j->k = k - 1U;
        jit_flush(j, &j->regs);
        jit_exit_insn(j, k);
    }
    for (uint32_t s = 0; s < j->stubs; ++s) {
        j->w->positions[s] = j->n;
        jit_stub_t stub = j->w->stubs[s];
        jit_emit_stub(j, &stub);
    }
    for (uint32_t f = 0; f < j->fixups; ++f) {
        jit_patch(j, j->w->fixups[f].site, j->w->positions[j->w->fixups[f].stub]);
    }
    return j->overflow ? 0 : j->n;
}

static uint32_t jit_compile(jit_t *j) {
    uint32_t words = jit_pass(j);
    if (words == 0 || !j->self_loop || (uint32_t)__builtin_popcountll(j->used) > sizeof(jit_pool)) {
        return words;
    }
    j->loop = true;
    uint32_t looped = jit_pass(j);
    if (looped != 0) {
        return looped;
    }
    j->loop = false;
    return jit_pass(j);
}

static uint32_t jit_patch_bucket(uint64_t pa_page, uint32_t index) {
    return (uint32_t)(((pa_page + index * 4ULL) >> 2) * 0x9E3779B97F4A7C15ULL >> 40) & (JIT_BUCKETS - 1U);
}

static void jit_unhash_patch(cpu_jit_pool_t *jp, uint32_t p) {
    cpu_jit_patch_t *patch = &jp->patches[p];
    uint32_t *link = &jp->patch_heads[jit_patch_bucket(patch->pa_page, patch->index)];
    while (*link != p) {
        link = &jp->patches[*link].next;
    }
    *link = patch->next;
    patch->hashed = false;
}

static void jit_unhash_block(cpu_jit_pool_t *jp, uint32_t b) {
    uint32_t *link = &jp->heads[cpu_pa_bucket(jp->blocks[b].pa_page, JIT_BUCKETS)];
    while (*link != b) {
        link = &jp->blocks[*link].next;
    }
    *link = jp->blocks[b].next;
}

static void jit_backfill(cpu_jit_pool_t *jp, const cpu_jit_block_t *block) {
    uint32_t *link = &jp->patch_heads[jit_patch_bucket(block->pa_page, block->index)];
    while (*link != POOL_NONE) {
        uint32_t p = *link;
        cpu_jit_patch_t *patch = &jp->patches[p];
        if (patch->seg == block->seg && patch->index == block->index && patch->pa_page == block->pa_page &&
            patch->el == block->el && patch->world == block->world && patch->traps == block->traps &&
            patch->pfn_gen == block->pfn_gen && patch->code_gen == block->code_gen) {
            uint32_t target = block->offset - jp->arena.base_offset;
            cpu_jit_arena_patch(&jp->arena, patch->site, A_B((int32_t)(target - patch->site) >> 2));
            *link = patch->next;
            patch->hashed = false;
        } else {
            link = &patch->next;
        }
    }
}

static void jit_publish_block(cpu_pool_t *pool, uint32_t b) {
    cpu_jit_pool_t *jp = &pool->jit;
    cpu_jit_block_t *block = &jp->blocks[b];
    os_unfair_lock_lock(&pool->lock);
    bool current = atomic_load_explicit(cpu_pool_pfn_gen(pool, block->pa_page), memory_order_relaxed) ==
                       block->pfn_gen &&
                   atomic_load_explicit(&pool->code_gen, memory_order_relaxed) == block->code_gen;
    ir_page_t *pg = current ? cpu_pool_live(pool, block->pa_page, block->el, block->world, block->traps) : NULL;
    if (pg != NULL) {
        uint32_t expected = block->orig;
        current = pg->pfn_gen == block->pfn_gen && pg->code_gen == block->code_gen && pg->hash == block->hash &&
                  __atomic_compare_exchange_n(&pg->insns[block->index].fast_offset, &expected, block->offset, false,
                                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    }
    os_unfair_lock_unlock(&pool->lock);
    if (!current) {
        block->state = JIT_BLOCK_DEAD;
        return;
    }
    block->state = JIT_BLOCK_LIVE;
    uint32_t *head = &jp->heads[cpu_pa_bucket(block->pa_page, JIT_BUCKETS)];
    block->next = *head;
    *head = b;
    jit_backfill(jp, block);
}

static void jit_drain_locked(cpu_pool_t *pool, uint64_t quiescent) {
    cpu_jit_pool_t *jp = &pool->jit;
    while (jp->pend_head != POOL_NONE && jp->blocks[jp->pend_head].epoch <= quiescent) {
        uint32_t b = jp->pend_head;
        jp->pend_head = jp->blocks[b].pend_next;
        if (jp->pend_head == POOL_NONE) {
            jp->pend_tail = POOL_NONE;
        }
        if (jp->blocks[b].state == JIT_BLOCK_PENDING) {
            jit_publish_block(pool, b);
        }
    }
    atomic_store_explicit(&jp->pend_epoch, jp->pend_head == POOL_NONE ? UINT64_MAX : jp->blocks[jp->pend_head].epoch,
                          memory_order_relaxed);
}

void cpu_jit_publish(cpu_pool_t *pool) {
    cpu_jit_pool_t *jp = &pool->jit;
    uint64_t pending = atomic_load_explicit(&jp->pend_epoch, memory_order_relaxed);
    if (pending == UINT64_MAX || pending > cpu_qs_quiescent(pool, NULL) || !os_unfair_lock_trylock(&jp->lock)) {
        return;
    }
    if (atomic_load_explicit(&jp->active, memory_order_relaxed)) {
        jit_drain_locked(pool, cpu_qs_quiescent(pool, NULL));
    }
    os_unfair_lock_unlock(&jp->lock);
}

static void jit_retire_segment_locked(cpu_pool_t *pool, uint32_t seg) {
    cpu_jit_pool_t *jp = &pool->jit;
    os_unfair_lock_lock(&pool->lock);
    for (uint32_t b = jp->seg[seg].blocks; b != POOL_NONE; b = jp->blocks[b].seg_next) {
        cpu_jit_block_t *block = &jp->blocks[b];
        if (block->state == JIT_BLOCK_PENDING) {
            block->state = JIT_BLOCK_DEAD;
        }
        if (block->state != JIT_BLOCK_LIVE) {
            continue;
        }
        block->state = JIT_BLOCK_DEAD;
        jit_unhash_block(jp, b);
        ir_page_t *pg = cpu_pool_live(pool, block->pa_page, block->el, block->world, block->traps);
        if (pg != NULL) {
            uint32_t expected = block->offset;
            if (__atomic_compare_exchange_n(&pg->insns[block->index].fast_offset, &expected, block->orig, false,
                                            __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
                __atomic_store_n(&pg->insns[block->index].heat, JIT_REHEAT, __ATOMIC_RELAXED);
            }
        }
    }
    os_unfair_lock_unlock(&pool->lock);
    for (uint32_t p = jp->seg[seg].patches; p != POOL_NONE; p = jp->patches[p].seg_next) {
        if (jp->patches[p].hashed) {
            jit_unhash_patch(jp, p);
        }
    }
    jp->seg[seg].gen = ++jp->gen;
    jp->seg[seg].state = JIT_SEG_RETIRED;
    jp->seg[seg].retire = cpu_qs_retire_epoch(pool);
}

static void jit_free_segment_locked(cpu_pool_t *pool, uint32_t seg, uint64_t quiescent) {
    cpu_jit_pool_t *jp = &pool->jit;
    jit_drain_locked(pool, quiescent);
    for (uint32_t b = jp->seg[seg].blocks, next; b != POOL_NONE; b = next) {
        next = jp->blocks[b].seg_next;
        jp->blocks[b].state = JIT_BLOCK_FREE;
        jp->blocks[b].next = jp->block_free;
        jp->block_free = b;
        jp->block_avail++;
    }
    for (uint32_t p = jp->seg[seg].patches, next; p != POOL_NONE; p = next) {
        next = jp->patches[p].seg_next;
        jp->patches[p].next = jp->patch_free;
        jp->patch_free = p;
    }
    jp->seg[seg].blocks = POOL_NONE;
    jp->seg[seg].patches = POOL_NONE;
    jp->seg[seg].count = 0;
    jp->seg[seg].state = JIT_SEG_FREE;
}

static uint32_t jit_open_segment_locked(cpu_pool_t *pool, uint64_t *wait) {
    cpu_jit_pool_t *jp = &pool->jit;
    uint64_t quiescent = cpu_qs_quiescent(pool, NULL);
    uint32_t pick = POOL_NONE, spare = 0;
    for (uint32_t s = 0; s < jp->segments; ++s) {
        cpu_jit_segment_t *seg = &jp->seg[s];
        if (seg->state == JIT_SEG_RETIRED && seg->retire <= quiescent) {
            jit_free_segment_locked(pool, s, quiescent);
        }
        if (seg->state == JIT_SEG_FREE && pick == POOL_NONE) {
            pick = s;
        } else if (seg->state == JIT_SEG_FREE || seg->state == JIT_SEG_RETIRED) {
            ++spare;
        }
    }
    uint32_t avail = jp->block_avail;
    for (uint32_t s = 0; s < jp->segments; ++s) {
        if (jp->seg[s].state == JIT_SEG_RETIRED) {
            avail += jp->seg[s].count;
        }
    }
    while (spare < JIT_RETIRE_AHEAD || avail < JIT_BLOCK_RESERVE) {
        uint32_t victim = POOL_NONE;
        for (uint32_t s = 0; s < jp->segments; ++s) {
            const cpu_jit_segment_t *seg = &jp->seg[s];
            if (seg->state == JIT_SEG_FULL && seg->holders == 0 &&
                (victim == POOL_NONE || seg->seq < jp->seg[victim].seq)) {
                victim = s;
            }
        }
        if (victim == POOL_NONE) {
            break;
        }
        jit_retire_segment_locked(pool, victim);
        avail += jp->seg[victim].count;
        ++spare;
    }
    if (pick == POOL_NONE) {
        for (uint32_t s = 0; s < jp->segments; ++s) {
            if (jp->seg[s].state == JIT_SEG_RETIRED && (*wait == 0 || jp->seg[s].retire < *wait)) {
                *wait = jp->seg[s].retire;
            }
        }
        return POOL_NONE;
    }
    jp->seg[pick].state = JIT_SEG_OPEN;
    jp->seg[pick].seq = ++jp->seq;
    return pick;
}

static bool jit_chunk_locked(cpu_pool_t *pool, jit_work_t *w, uint64_t *wait) {
    cpu_jit_pool_t *jp = &pool->jit;
    if (w->chunk_seg != POOL_NONE) {
        jp->seg[w->chunk_seg].holders--;
    }
    w->chunk_seg = POOL_NONE;
    if (jp->open != POOL_NONE && jp->carve == JIT_SEGMENT_BYTES) {
        jp->seg[jp->open].state = JIT_SEG_FULL;
        jp->open = POOL_NONE;
    }
    if (jp->open == POOL_NONE) {
        jp->open = jit_open_segment_locked(pool, wait);
        jp->carve = 0;
    }
    if (jp->open == POOL_NONE) {
        return false;
    }
    cpu_jit_segment_t *seg = &jp->seg[jp->open];
    seg->holders++;
    w->chunk_seg = jp->open;
    w->chunk_gen = seg->gen;
    w->chunk_pos = jp->open * JIT_SEGMENT_BYTES + jp->carve;
    w->chunk_end = w->chunk_pos + JIT_CHUNK_BYTES;
    jp->carve += JIT_CHUNK_BYTES;
    return true;
}

static bool jit_record_locked(cpu_pool_t *pool, jit_t *j, uint32_t entry, uint32_t orig) {
    cpu_jit_pool_t *jp = &pool->jit;
    jit_work_t *w = j->w;
    ir_page_t *pg = j->pg;
    if (jp->block_free == POOL_NONE) {
        return false;
    }
    uint32_t b = jp->block_free;
    cpu_jit_block_t *block = &jp->blocks[b];
    jp->block_free = block->next;
    jp->block_avail--;
    *block = (cpu_jit_block_t){
        .pa_page = pg->pa_page,
        .hash = pg->hash,
        .epoch = cpu_qs_retire_epoch(pool),
        .traps = pg->traps,
        .pfn_gen = pg->pfn_gen,
        .code_gen = pg->code_gen,
        .offset = jp->arena.base_offset + entry,
        .orig = orig,
        .next = POOL_NONE,
        .seg_next = jp->seg[w->chunk_seg].blocks,
        .pend_next = POOL_NONE,
        .index = (uint16_t)j->start,
        .seg = (uint16_t)w->chunk_seg,
        .el = pg->el,
        .world = pg->world,
        .state = JIT_BLOCK_PENDING,
    };
    jp->seg[w->chunk_seg].blocks = b;
    jp->seg[w->chunk_seg].count++;
    if (jp->pend_tail == POOL_NONE) {
        jp->pend_head = b;
        atomic_store_explicit(&jp->pend_epoch, block->epoch, memory_order_relaxed);
    } else {
        jp->blocks[jp->pend_tail].pend_next = b;
    }
    jp->pend_tail = b;
    for (uint32_t s = 0; s < j->sites && jp->patch_free != POOL_NONE; ++s) {
        uint32_t p = jp->patch_free;
        cpu_jit_patch_t *patch = &jp->patches[p];
        jp->patch_free = patch->next;
        uint32_t *head = &jp->patch_heads[jit_patch_bucket(pg->pa_page, w->sites[s].stub)];
        *patch = (cpu_jit_patch_t){
            .pa_page = pg->pa_page,
            .traps = pg->traps,
            .pfn_gen = pg->pfn_gen,
            .code_gen = pg->code_gen,
            .site = entry + w->sites[s].site * 4U,
            .next = *head,
            .seg_next = jp->seg[w->chunk_seg].patches,
            .index = (uint16_t)w->sites[s].stub,
            .seg = (uint16_t)w->chunk_seg,
            .el = pg->el,
            .world = pg->world,
            .hashed = true,
        };
        *head = p;
        jp->seg[w->chunk_seg].patches = p;
    }
    return true;
}

static uint32_t jit_build(cpu_pool_t *pool, vcpu_t *self, ir_insn_t *ir) {
    cpu_jit_pool_t *jp = &pool->jit;
    cpu_icache_t *ic = (cpu_icache_t *)self->icache;
    jit_work_t *w = ic->jit_work;
    ir_page_t *pg = ic->cur_page;
    uint32_t orig = __atomic_load_n(&ir->fast_offset, __ATOMIC_RELAXED);
    jit_t j = {.w = w, .pg = pg, .start = (uint32_t)(ir - pg->insns)};
    for (uint32_t attempt = 0; attempt < 2; ++attempt) {
        os_unfair_lock_lock(&jp->lock);
        if (!atomic_load_explicit(&jp->active, memory_order_relaxed) || orig >= jp->arena.base_offset) {
            os_unfair_lock_unlock(&jp->lock);
            return 0;
        }
        if (w->chunk_seg >= jp->segments || jp->seg[w->chunk_seg].gen != w->chunk_gen) {
            w->chunk_seg = POOL_NONE;
        }
        uint64_t wait = 0;
        if ((w->chunk_seg == POOL_NONE || w->chunk_end - w->chunk_pos < JIT_CHUNK_BYTES / 16U) &&
            !jit_chunk_locked(pool, w, &wait)) {
            os_unfair_lock_unlock(&jp->lock);
            __atomic_store_n(&ir->heat, JIT_HEAT, __ATOMIC_RELAXED);
            if (wait == 0) {
                return 0;
            }
            self->pc = cpu_ir_pc(self, ir);
            cpu_qs_park(self);
            cpu_qs_wait(pool, self, wait);
            cpu_qs_unpark(self);
            return UINT32_MAX;
        }
        j.rx = (uintptr_t)jp->arena.rx + w->chunk_pos;
        j.seg_lo = jp->arena.base_offset + w->chunk_seg * JIT_SEGMENT_BYTES;
        j.seg_hi = j.seg_lo + JIT_SEGMENT_BYTES;
        os_unfair_lock_unlock(&jp->lock);
        j.loop = false;
        j.self_loop = false;
        j.used = 0;
        j.vused = 0;
        uint32_t words = jit_compile(&j);
        if (words == 0) {
            return 0;
        }
        uint32_t bytes = (words * 4U + 15U) & ~15U;
        if (w->chunk_pos + bytes > w->chunk_end) {
            w->chunk_pos = w->chunk_end;
            continue;
        }
        uint32_t entry = w->chunk_pos;
        uint32_t offset = 0;
        os_unfair_lock_lock(&jp->lock);
        if (atomic_load_explicit(&jp->active, memory_order_relaxed) && jit_record_locked(pool, &j, entry, orig)) {
            cpu_jit_arena_write(&jp->arena, entry, w->code, words);
            w->chunk_pos += bytes;
            offset = jp->arena.base_offset + entry;
        } else {
            __atomic_store_n(&ir->heat, JIT_REHEAT, __ATOMIC_RELAXED);
        }
        os_unfair_lock_unlock(&jp->lock);
        return offset;
    }
    return 0;
}

uint32_t cpu_jit_enter(vcpu_t *cpu, ir_insn_t *ir) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    if (ic->jit_work == NULL) {
        jit_work_t *w = malloc(sizeof(jit_work_t));
        w->chunk_seg = POOL_NONE;
        ic->jit_work = w;
    }
    uint32_t offset = jit_build(ic->pool, cpu, ir);
    if (offset == UINT32_MAX) {
        return offset;
    }
    cpu_jit_publish(ic->pool);
    return offset != 0 ? offset : __atomic_load_n(&ir->fast_offset, __ATOMIC_ACQUIRE);
}

static void jit_pool_activate(cpu_pool_t *pool) {
    cpu_jit_pool_t *jp = &pool->jit;
    if (!cpu_jit_arena_acquire(pool->vm, &jp->arena)) {
        return;
    }
    jp->segments = jp->arena.size / JIT_SEGMENT_BYTES;
    jp->open = POOL_NONE;
    jp->carve = 0;
    jp->seq = 0;
    jp->seg = malloc(jp->segments * sizeof(*jp->seg));
    for (uint32_t i = 0; i < jp->segments; ++i) {
        jp->seg[i] = (cpu_jit_segment_t){.blocks = POOL_NONE, .patches = POOL_NONE, .gen = ++jp->gen};
    }
    jp->blocks = malloc(JIT_BLOCKS * sizeof(*jp->blocks));
    for (uint32_t i = 0; i < JIT_BLOCKS; ++i) {
        jp->blocks[i].next = i + 1U < JIT_BLOCKS ? i + 1U : POOL_NONE;
        jp->blocks[i].state = JIT_BLOCK_FREE;
    }
    jp->block_free = 0;
    jp->block_avail = JIT_BLOCKS;
    jp->pend_head = POOL_NONE;
    jp->pend_tail = POOL_NONE;
    jp->patches = malloc(JIT_PATCHES * sizeof(*jp->patches));
    for (uint32_t i = 0; i < JIT_PATCHES; ++i) {
        jp->patches[i].next = i + 1U < JIT_PATCHES ? i + 1U : POOL_NONE;
    }
    jp->patch_free = 0;
    jp->heads = malloc(JIT_BUCKETS * sizeof(*jp->heads));
    jp->patch_heads = malloc(JIT_BUCKETS * sizeof(*jp->patch_heads));
    for (uint32_t i = 0; i < JIT_BUCKETS; ++i) {
        jp->heads[i] = POOL_NONE;
        jp->patch_heads[i] = POOL_NONE;
    }
    os_unfair_lock_lock(&jp->lock);
    atomic_store_explicit(&jp->pend_epoch, UINT64_MAX, memory_order_relaxed);
    atomic_store_explicit(&jp->active, true, memory_order_relaxed);
    os_unfair_lock_unlock(&jp->lock);
}

static void jit_pool_deactivate(cpu_pool_t *pool, vcpu_t *self) {
    cpu_jit_pool_t *jp = &pool->jit;
    os_unfair_lock_lock(&jp->lock);
    atomic_store_explicit(&jp->active, false, memory_order_relaxed);
    uint64_t epoch = 0;
    for (uint32_t s = 0; s < jp->segments; ++s) {
        if (jp->seg[s].state != JIT_SEG_FREE && jp->seg[s].state != JIT_SEG_RETIRED) {
            jit_retire_segment_locked(pool, s);
        }
        if (jp->seg[s].retire > epoch) {
            epoch = jp->seg[s].retire;
        }
    }
    atomic_store_explicit(&jp->pend_epoch, UINT64_MAX, memory_order_relaxed);
    os_unfair_lock_unlock(&jp->lock);
    cpu_qs_wait(pool, self, epoch);
    cpu_jit_arena_release(pool->vm);
    free(jp->seg);
    free(jp->blocks);
    free(jp->patches);
    free(jp->heads);
    free(jp->patch_heads);
    jp->seg = NULL;
    jp->blocks = NULL;
    jp->patches = NULL;
    jp->heads = NULL;
    jp->patch_heads = NULL;
}

static void cpu_jit_sync(vcpu_t *cpu) {
    cpu_icache_t *ic = (cpu_icache_t *)cpu->icache;
    cpu_pool_t *pool = ic->pool;
    ic->jit_generation = atomic_load_explicit(&cpu_jit_generation, memory_order_acquire);
    bool want = swipterm_jit_is_prepared() && swipterm_jit_is_enabled();
    cpu_qs_park(cpu);
    os_unfair_lock_lock(&pool->jit.toggle);
    bool changed = pool->jit.active != want;
    if (want && !pool->jit.active) {
        jit_pool_activate(pool);
    } else if (!want && pool->jit.active) {
        jit_pool_deactivate(pool, cpu);
    }
    os_unfair_lock_unlock(&pool->jit.toggle);
    cpu_qs_unpark(cpu);
    if (changed) {
        vcpu_flush_icache_all(cpu);
    }
}
