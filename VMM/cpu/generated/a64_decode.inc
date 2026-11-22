typedef enum a64_dec_op : uint16_t {
    A64_DEC_UNDEF = 0,
    A64_DEC_ADDSUB_CARRY_ADC,
    A64_DEC_ADDSUB_CARRY_ADCS,
    A64_DEC_ADDSUB_CARRY_SBC,
    A64_DEC_ADDSUB_CARRY_SBCS,
    A64_DEC_ADDSUB_EXT_ADD,
    A64_DEC_ADDSUB_EXT_ADDS,
    A64_DEC_ADDSUB_EXT_SUB,
    A64_DEC_ADDSUB_EXT_SUBS,
    A64_DEC_ADDSUB_IMM_ADD,
    A64_DEC_ADDSUB_IMM_ADDS,
    A64_DEC_ADDSUB_IMM_SUB,
    A64_DEC_ADDSUB_IMM_SUBS,
    A64_DEC_ADDSUB_SHIFT_ADD,
    A64_DEC_ADDSUB_SHIFT_ADDS,
    A64_DEC_ADDSUB_SHIFT_SUB,
    A64_DEC_ADDSUB_SHIFT_SUBS,
    A64_DEC_ASIMDALL,
    A64_DEC_ASIMDDIFF,
    A64_DEC_ASIMDELEM,
    A64_DEC_ASIMDEXT,
    A64_DEC_ASIMDIMM,
    A64_DEC_ASIMDINS,
    A64_DEC_ASIMDMISC,
    A64_DEC_ASIMDMISCFP16,
    A64_DEC_ASIMDPERM,
    A64_DEC_ASIMDSAME,
    A64_DEC_ASIMDSAME2,
    A64_DEC_ASIMDSAMEFP16,
    A64_DEC_ASIMDSHF,
    A64_DEC_ASIMDTBL,
    A64_DEC_ASISDDIFF,
    A64_DEC_ASISDELEM,
    A64_DEC_ASISDLSE,
    A64_DEC_ASISDLSEP,
    A64_DEC_ASISDLSO,
    A64_DEC_ASISDLSOP,
    A64_DEC_ASISDMISC,
    A64_DEC_ASISDMISCFP16,
    A64_DEC_ASISDONE,
    A64_DEC_ASISDPAIR,
    A64_DEC_ASISDSAME,
    A64_DEC_ASISDSAME2,
    A64_DEC_ASISDSAMEFP16,
    A64_DEC_ASISDSHF,
    A64_DEC_BARRIERS_CLREX,
    A64_DEC_BARRIERS_DMB,
    A64_DEC_BARRIERS_DSB,
    A64_DEC_BARRIERS_ISB,
    A64_DEC_BITFIELD_BFM,
    A64_DEC_BITFIELD_SBFM,
    A64_DEC_BITFIELD_UBFM,
    A64_DEC_BRANCH_IMM_B,
    A64_DEC_BRANCH_IMM_BL,
    A64_DEC_BRANCH_REG_BLR,
    A64_DEC_BRANCH_REG_BR,
    A64_DEC_BRANCH_REG_DRPS,
    A64_DEC_BRANCH_REG_ERET,
    A64_DEC_BRANCH_REG_RET,
    A64_DEC_B_COND,
    A64_DEC_COMPBRANCH_CBNZ,
    A64_DEC_COMPBRANCH_CBZ,
    A64_DEC_CONDCMP_IMM_CCMN,
    A64_DEC_CONDCMP_IMM_CCMP,
    A64_DEC_CONDCMP_REG_CCMN,
    A64_DEC_CONDCMP_REG_CCMP,
    A64_DEC_CONDSEL_CSEL,
    A64_DEC_CONDSEL_CSINC,
    A64_DEC_CONDSEL_CSINV,
    A64_DEC_CONDSEL_CSNEG,
    A64_DEC_CRYPTO3_IMM6,
    A64_DEC_CRYPTO4,
    A64_DEC_CRYPTOAES,
    A64_DEC_CRYPTOSHA2,
    A64_DEC_CRYPTOSHA3,
    A64_DEC_CRYPTOSHA512_2,
    A64_DEC_CRYPTOSHA512_3,
    A64_DEC_DP_1SRC_CLS,
    A64_DEC_DP_1SRC_CLZ,
    A64_DEC_DP_1SRC_RBIT,
    A64_DEC_DP_1SRC_REV,
    A64_DEC_DP_1SRC_REV16,
    A64_DEC_DP_1SRC_REV32,
    A64_DEC_DP_2SRC_ASRV,
    A64_DEC_DP_2SRC_CRC32B,
    A64_DEC_DP_2SRC_CRC32CB,
    A64_DEC_DP_2SRC_CRC32CH,
    A64_DEC_DP_2SRC_CRC32CW,
    A64_DEC_DP_2SRC_CRC32CX,
    A64_DEC_DP_2SRC_CRC32H,
    A64_DEC_DP_2SRC_CRC32W,
    A64_DEC_DP_2SRC_CRC32X,
    A64_DEC_DP_2SRC_LSLV,
    A64_DEC_DP_2SRC_LSRV,
    A64_DEC_DP_2SRC_RORV,
    A64_DEC_DP_2SRC_SDIV,
    A64_DEC_DP_2SRC_UDIV,
    A64_DEC_DP_3SRC_MADD,
    A64_DEC_DP_3SRC_MSUB,
    A64_DEC_DP_3SRC_SMADDL,
    A64_DEC_DP_3SRC_SMSUBL,
    A64_DEC_DP_3SRC_SMULH,
    A64_DEC_DP_3SRC_UMADDL,
    A64_DEC_DP_3SRC_UMSUBL,
    A64_DEC_DP_3SRC_UMULH,
    A64_DEC_EXCEPTION_BRK,
    A64_DEC_EXCEPTION_DCPS1,
    A64_DEC_EXCEPTION_DCPS2,
    A64_DEC_EXCEPTION_DCPS3,
    A64_DEC_EXCEPTION_HLT,
    A64_DEC_EXCEPTION_HVC,
    A64_DEC_EXCEPTION_SMC,
    A64_DEC_EXCEPTION_SVC,
    A64_DEC_EXTRACT_EXTR,
    A64_DEC_FLOAT2FIX,
    A64_DEC_FLOAT2INT,
    A64_DEC_FLOATCCMP,
    A64_DEC_FLOATCMP,
    A64_DEC_FLOATDP1,
    A64_DEC_FLOATDP2,
    A64_DEC_FLOATDP3,
    A64_DEC_FLOATIMM,
    A64_DEC_FLOATSEL,
    A64_DEC_HINTS_SEV,
    A64_DEC_HINTS_SEVL,
    A64_DEC_HINTS_WFE,
    A64_DEC_HINTS_WFI,
    A64_DEC_HINT_NOP,
    A64_DEC_LDST_EXCLP,
    A64_DEC_LDST_EXCLR,
    A64_DEC_LDST_IMMPOST,
    A64_DEC_LDST_IMMPRE,
    A64_DEC_LDST_NAPAIR,
    A64_DEC_LDST_ORD,
    A64_DEC_LDST_PAIR_OFF,
    A64_DEC_LDST_PAIR_POST,
    A64_DEC_LDST_PAIR_PRE,
    A64_DEC_LDST_POS,
    A64_DEC_LDST_REGOFF,
    A64_DEC_LDST_UNPRIV,
    A64_DEC_LDST_UNSCALED,
    A64_DEC_LOADLIT,
    A64_DEC_LOG_IMM_AND,
    A64_DEC_LOG_IMM_ANDS,
    A64_DEC_LOG_IMM_EOR,
    A64_DEC_LOG_IMM_ORR,
    A64_DEC_LOG_SHIFT_AND,
    A64_DEC_LOG_SHIFT_ANDS,
    A64_DEC_LOG_SHIFT_BIC,
    A64_DEC_LOG_SHIFT_BICS,
    A64_DEC_LOG_SHIFT_EON,
    A64_DEC_LOG_SHIFT_EOR,
    A64_DEC_LOG_SHIFT_ORN,
    A64_DEC_LOG_SHIFT_ORR,
    A64_DEC_LSE_CAS,
    A64_DEC_LSE_CASP,
    A64_DEC_LSE_MEMOP,
    A64_DEC_MOPS_CPY,
    A64_DEC_MOPS_SET,
    A64_DEC_MOVEWIDE_MOVK,
    A64_DEC_MOVEWIDE_MOVN,
    A64_DEC_MOVEWIDE_MOVZ,
    A64_DEC_PCRELADDR_ADR,
    A64_DEC_PCRELADDR_ADRP,
    A64_DEC_PERM_UNDEF_UDF,
    A64_DEC_PSTATE_MSR,
    A64_DEC_SYSTEMINSTRS_SYS,
    A64_DEC_SYSTEMINSTRS_SYSL,
    A64_DEC_SYSTEMMOVE_MRS,
    A64_DEC_SYSTEMMOVE_MSR,
    A64_DEC_TESTBRANCH_TBNZ,
    A64_DEC_TESTBRANCH_TBZ,
    A64_DEC_COUNT,
} a64_dec_op_t;

static inline a64_dec_op_t a64_decode_word(uint32_t word) {
    if (word & 0x10000000u) {
        if (word & 0x04000000u) {
            if (word & 0x20000000u) {
                if (word & 0x02000000u) {
                    if (word & 0x01000000u) {
                        if (word & 0x08000000u) {
                            if (word & 0x00004000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0xc0803000u) == 0x40003000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        } else {
                                            if ((word & 0xc0803000u) == 0x40002000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0xc0001000u) == 0x40001000u) return A64_DEC_ASISDELEM;
                                        } else {
                                            if ((word & 0xc0001000u) == 0x40001000u) return A64_DEC_ASISDELEM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0xc0800c00u) == 0x40000400u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        } else {
                                            if ((word & 0xc0c00c00u) == 0x40400400u) return A64_DEC_ASISDSHF;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0xc0800c00u) == 0x40000400u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        } else {
                                            if ((word & 0xc0c00c00u) == 0x40400400u) return A64_DEC_ASISDSHF;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000800u) {
                                                if ((word & 0xc0802000u) == 0x40000000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            } else {
                                                if ((word & 0xc0802000u) == 0x40000000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0xc0c00800u) == 0x40400000u) return A64_DEC_ASISDSHF;
                                            } else {
                                                if ((word & 0xc0c00800u) == 0x40400000u) return A64_DEC_ASISDSHF;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0xc000a000u) == 0x40008000u) return A64_DEC_ASISDELEM;
                                        } else {
                                            if ((word & 0xc040a000u) == 0x40008000u) return A64_DEC_ASISDELEM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0xc0802400u) == 0x40000400u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        } else {
                                            if ((word & 0xc0802400u) == 0x40000400u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0xc0c00c00u) == 0x40400400u) return A64_DEC_ASISDSHF;
                                        } else {
                                            if ((word & 0xc0c00c00u) == 0x40400400u) return A64_DEC_ASISDSHF;
                                        }
                                    }
                                }
                            }
                        } else {
                            if ((word & 0x40000000u) == 0x00000000u) return A64_DEC_TESTBRANCH_TBNZ;
                        }
                    } else {
                        if (word & 0x08000000u) {
                            if (word & 0x00002000u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0xc0209000u) == 0x40208000u) return A64_DEC_ASISDSAME;
                                            } else {
                                                if ((word & 0xc0209000u) == 0x40208000u) return A64_DEC_ASISDSAME;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00001000u) {
                                                    if ((word & 0xc0c08000u) == 0x40c00000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0008000u) == 0x40000000u) return A64_DEC_ASISDSAME;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc0409000u) == 0x40400000u) return A64_DEC_ASISDSAMEFP16;
                                                } else {
                                                    if ((word & 0xc0409000u) == 0x40400000u) return A64_DEC_ASISDSAMEFP16;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0xc0204000u) == 0x40200000u) return A64_DEC_ASISDSAME;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc0204000u) == 0x40204000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0204000u) == 0x40204000u) return A64_DEC_ASISDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0xc0e04000u) == 0x40e00000u) return A64_DEC_ASISDSAME;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc0604000u) == 0x40400000u) return A64_DEC_ASISDSAMEFP16;
                                                } else {
                                                    if ((word & 0xc0604000u) == 0x40400000u) return A64_DEC_ASISDSAMEFP16;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc076c800u) == 0x40708800u) return A64_DEC_ASISDMISCFP16;
                                                } else {
                                                    if ((word & 0xc076c800u) == 0x40708800u) return A64_DEC_ASISDMISCFP16;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc036c800u) == 0x40208800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc036c800u) == 0x40208800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0xc03e0800u) == 0x40300800u) return A64_DEC_ASISDPAIR;
                                                    } else {
                                                        if ((word & 0xc03e0800u) == 0x40300800u) return A64_DEC_ASISDPAIR;
                                                    }
                                                } else {
                                                    if ((word & 0xc03e0800u) == 0x40200800u) return A64_DEC_ASISDMISC;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0xc0fe0800u) == 0x40e00800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc03e0800u) == 0x40200800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc0774800u) == 0x40710800u) return A64_DEC_ASISDMISCFP16;
                                                } else {
                                                    if ((word & 0xc0774800u) == 0x40710800u) return A64_DEC_ASISDMISCFP16;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc0374800u) == 0x40210800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc0374800u) == 0x40210800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0xc0ff0800u) == 0x40610800u) return A64_DEC_ASISDMISC;
                                            } else {
                                                if ((word & 0xc03f0800u) == 0x40210800u) return A64_DEC_ASISDMISC;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0xc020c000u) == 0x40204000u) return A64_DEC_ASISDSAME;
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0xc0a00000u) == 0x40a00000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0e00000u) == 0x40e00000u) return A64_DEC_ASISDSAME;
                                                }
                                            } else {
                                                if ((word & 0xc0e08000u) == 0x40c00000u) return A64_DEC_ASISDSAMEFP16;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00010000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc076c800u) == 0x4070c800u) return A64_DEC_ASISDMISCFP16;
                                                } else {
                                                    if ((word & 0xc076c800u) == 0x4070c800u) return A64_DEC_ASISDMISCFP16;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc036c800u) == 0x4020c800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc036c800u) == 0x4020c800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00080000u) {
                                                    if ((word & 0xc0e6c800u) == 0x40e0c800u) return A64_DEC_ASISDMISCFP16;
                                                } else {
                                                    if ((word & 0xc0a6c800u) == 0x4020c800u) return A64_DEC_ASISDPAIR;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0xc0ae8800u) == 0x40a08800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc0ee8800u) == 0x40e08800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0xc0c04000u) == 0x40c00000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0004000u) == 0x40000000u) return A64_DEC_ASISDSAME2;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0xc0200000u) == 0x40200000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0200000u) == 0x40200000u) return A64_DEC_ASISDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0xc0e08000u) == 0x40e00000u) return A64_DEC_ASISDSAME;
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0xc0c08000u) == 0x40c08000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0xc0008000u) == 0x40008000u) return A64_DEC_ASISDSAME2;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00100000u) {
                                            if (word & 0x00080000u) {
                                                if (word & 0x00010000u) {
                                                    if ((word & 0xc0e6c800u) == 0x4060c800u) return A64_DEC_ASISDMISCFP16;
                                                } else {
                                                    if ((word & 0xc0e6c800u) == 0x40e0c800u) return A64_DEC_ASISDMISCFP16;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0xc027c800u) == 0x4020c800u) return A64_DEC_ASISDPAIR;
                                                } else {
                                                    if ((word & 0xc027c800u) == 0x4020c800u) return A64_DEC_ASISDPAIR;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0xc0ae4800u) == 0x40204800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc02e4800u) == 0x40204800u) return A64_DEC_ASISDMISC;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0xc0ae8800u) == 0x40a08800u) return A64_DEC_ASISDMISC;
                                                } else {
                                                    if ((word & 0xc0ee8800u) == 0x40e08800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if ((word & 0x40000000u) == 0x00000000u) return A64_DEC_TESTBRANCH_TBZ;
                        }
                    }
                } else {
                    if (word & 0x80000000u) {
                        if (word & 0x40000000u) {
                            if (word & 0x00400000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPRE;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPOST;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08200000u) return A64_DEC_LDST_REGOFF;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_UNSCALED;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPRE;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPOST;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08200000u) return A64_DEC_LDST_REGOFF;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_UNSCALED;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x08000000u) {
                                    if (word & 0x00400000u) {
                                        if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    } else {
                                        if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    }
                                } else {
                                    return A64_DEC_COMPBRANCH_CBNZ;
                                }
                            } else {
                                if (word & 0x08000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                            } else {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                            } else {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00000u) == 0x00200000u) return A64_DEC_LDST_REGOFF;
                                            } else {
                                                if ((word & 0x00a00000u) == 0x00200000u) return A64_DEC_LDST_REGOFF;
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                            } else {
                                                if ((word & 0x00a00000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_COMPBRANCH_CBZ;
                                }
                            }
                        }
                    } else {
                        if (word & 0x40000000u) {
                            if (word & 0x00400000u) {
                                if (word & 0x01000000u) {
                                    if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPRE;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPOST;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08200000u) return A64_DEC_LDST_REGOFF;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_UNSCALED;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x01000000u) {
                                    if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPRE;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_IMMPOST;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x08a00000u) == 0x08200000u) return A64_DEC_LDST_REGOFF;
                                        } else {
                                            if ((word & 0x08a00000u) == 0x08000000u) return A64_DEC_LDST_UNSCALED;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x01000000u) {
                                if (word & 0x08000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            return A64_DEC_LDST_POS;
                                        } else {
                                            return A64_DEC_LDST_POS;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            return A64_DEC_LDST_POS;
                                        } else {
                                            return A64_DEC_LDST_POS;
                                        }
                                    }
                                } else {
                                    return A64_DEC_COMPBRANCH_CBNZ;
                                }
                            } else {
                                if (word & 0x08000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00200000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if ((word & 0x0020e000u) == 0x00206000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00200000u) == 0x00200000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPRE;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_IMMPOST;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00200000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if ((word & 0x0020e000u) == 0x00206000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00200000u) == 0x00200000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                }
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                                } else {
                                                    if ((word & 0x00200000u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_COMPBRANCH_CBZ;
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x08000000u) {
                    if (word & 0x01000000u) {
                        if (word & 0x00008000u) {
                            if (word & 0x02000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x80805000u) == 0x00005000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            } else {
                                                if ((word & 0x80805000u) == 0x00001000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x80806000u) == 0x00000000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            } else {
                                                if ((word & 0x80806000u) == 0x00006000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x80002000u) == 0x00000000u) return A64_DEC_ASISDELEM;
                                            } else {
                                                if ((word & 0x80002000u) == 0x00000000u) return A64_DEC_ASISDELEM;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                } else {
                                                    if ((word & 0x80401000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            } else {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            }
                                        } else {
                                            if ((word & 0x80800000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                        }
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            } else {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            }
                                        } else {
                                            if ((word & 0x80800000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x80800800u) == 0x00000000u && (((word >> 19) & 15u) != 0x0u)) return A64_DEC_ASISDSHF;
                                                } else {
                                                    if ((word & 0x80c00800u) == 0x00400000u) return A64_DEC_ASISDSHF;
                                                }
                                            } else {
                                                if ((word & 0x80c04800u) == 0x00400000u) return A64_DEC_ASISDSHF;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x80c00800u) == 0x00400000u) return A64_DEC_ASISDSHF;
                                                } else {
                                                    if ((word & 0x80c00800u) == 0x00400000u) return A64_DEC_ASISDSHF;
                                                }
                                            } else {
                                                if ((word & 0x80c04800u) == 0x00400000u) return A64_DEC_ASISDSHF;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                } else {
                                                    if ((word & 0x80401000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x80001000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                } else {
                                                    if ((word & 0x80401000u) == 0x00001000u) return A64_DEC_ASISDELEM;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            } else {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            }
                                        } else {
                                            if ((word & 0x80800000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                        }
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            } else {
                                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                            }
                                        } else {
                                            if ((word & 0x80800000u) == 0x00000000u) return A64_DEC_FLOATDP3;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x00a00c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x40000000u) {
                            if (word & 0x02000000u) {
                                if (word & 0x00002000u) {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x80204000u) == 0x00204000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80204000u) == 0x00204000u) return A64_DEC_ASISDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x80204000u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x80c04000u) == 0x00c00000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80c04000u) == 0x00c00000u) return A64_DEC_ASISDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x80404800u) == 0x00400800u) return A64_DEC_ASISDSAMEFP16;
                                                    } else {
                                                        if ((word & 0x80404800u) == 0x00400800u) return A64_DEC_ASISDSAMEFP16;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x80f68800u) == 0x00f08800u) return A64_DEC_ASISDMISCFP16;
                                                    } else {
                                                        if ((word & 0x80b68800u) == 0x00a08800u) return A64_DEC_ASISDMISC;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x807e0800u) == 0x00300800u) return A64_DEC_ASISDPAIR;
                                                        } else {
                                                            if ((word & 0x807e0800u) == 0x00300800u) return A64_DEC_ASISDPAIR;
                                                        }
                                                    } else {
                                                        if ((word & 0x803e0800u) == 0x00200800u) return A64_DEC_ASISDMISC;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00100000u) {
                                                        if (word & 0x00080000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x80678000u) == 0x00618000u) return A64_DEC_ASISDMISCFP16;
                                                            } else {
                                                                if ((word & 0x80678000u) == 0x00618000u) return A64_DEC_ASISDMISCFP16;
                                                            }
                                                        } else {
                                                            if ((word & 0x80e78000u) == 0x00e18000u) return A64_DEC_ASISDPAIR;
                                                        }
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x802e8000u) == 0x00208000u) return A64_DEC_ASISDMISC;
                                                            } else {
                                                                if ((word & 0x802e8000u) == 0x00208000u) return A64_DEC_ASISDMISC;
                                                            }
                                                        } else {
                                                            if (word & 0x00008000u) {
                                                                if ((word & 0x80ee0000u) == 0x00e00000u) return A64_DEC_ASISDMISC;
                                                            } else {
                                                                if ((word & 0x802e0000u) == 0x00200000u) return A64_DEC_ASISDMISC;
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x80200000u) == 0x00200000u) return A64_DEC_ASISDDIFF;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00000400u) {
                                                    if ((word & 0x80a00800u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x80f70800u) == 0x00f00800u) return A64_DEC_ASISDMISCFP16;
                                                    } else {
                                                        if ((word & 0x80b70800u) == 0x00a00800u) return A64_DEC_ASISDMISC;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x80770c00u) == 0x00710800u) return A64_DEC_ASISDMISCFP16;
                                                    } else {
                                                        if ((word & 0x80770c00u) == 0x00710800u) return A64_DEC_ASISDMISCFP16;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x80360c00u) == 0x00200800u) return A64_DEC_ASISDMISC;
                                                        } else {
                                                            if ((word & 0x80360c00u) == 0x00200800u) return A64_DEC_ASISDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x80f60c00u) == 0x00e00800u) return A64_DEC_ASISDMISC;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x80204000u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                } else {
                                                    if ((word & 0x80e04000u) == 0x00400000u) return A64_DEC_ASISDSAMEFP16;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x80ff4000u) == 0x00280000u) return A64_DEC_CRYPTOSHA2;
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00800000u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x80764400u) == 0x00704000u) return A64_DEC_ASISDMISCFP16;
                                                        } else {
                                                            if ((word & 0x80364400u) == 0x00204000u) return A64_DEC_ASISDMISC;
                                                        }
                                                    } else {
                                                        if (word & 0x00004000u) {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x80760400u) == 0x00700000u) return A64_DEC_ASISDMISCFP16;
                                                            } else {
                                                                if ((word & 0x80360400u) == 0x00200000u) return A64_DEC_ASISDMISC;
                                                            }
                                                        } else {
                                                            if ((word & 0x807e0400u) == 0x00600000u) return A64_DEC_ASISDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        if ((word & 0x80204000u) == 0x00204000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x80764000u) == 0x00704000u) return A64_DEC_ASISDMISCFP16;
                                                            } else {
                                                                if ((word & 0x80364000u) == 0x00204000u) return A64_DEC_ASISDMISC;
                                                            }
                                                        } else {
                                                            if ((word & 0x807e4000u) == 0x00304000u) return A64_DEC_ASISDPAIR;
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x80200400u) == 0x00200000u) return A64_DEC_ASISDDIFF;
                                                } else {
                                                    if ((word & 0x80200400u) == 0x00200000u) return A64_DEC_ASISDDIFF;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80200000u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00400000u) return A64_DEC_ASISDSAMEFP16;
                                                    }
                                                } else {
                                                    if ((word & 0x80e04000u) == 0x00e04000u) return A64_DEC_ASISDSAME;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x80ff4000u) == 0x00280000u) return A64_DEC_CRYPTOSHA2;
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00800000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x80770c00u) == 0x00700800u) return A64_DEC_ASISDMISCFP16;
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if ((word & 0x80670c00u) == 0x00200800u) return A64_DEC_ASISDPAIR;
                                                        } else {
                                                            if ((word & 0x80270c00u) == 0x00200800u) return A64_DEC_ASISDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x80760c00u) == 0x00700800u) return A64_DEC_ASISDMISCFP16;
                                                        } else {
                                                            if ((word & 0x80360c00u) == 0x00200800u) return A64_DEC_ASISDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x807e0c00u) == 0x00300800u) return A64_DEC_ASISDPAIR;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x80e00000u) == 0x00e00000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00e00000u) return A64_DEC_ASISDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x80ff0800u) == 0x00e00800u) return A64_DEC_ASISDMISC;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80200000u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80200000u) == 0x00200000u) return A64_DEC_ASISDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80e00000u) == 0x00e00000u) return A64_DEC_ASISDSAME;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_ASISDONE;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x803f0000u) == 0x00210000u) return A64_DEC_ASISDMISC;
                                                    } else {
                                                        if ((word & 0x80ff0000u) == 0x00280000u) return A64_DEC_CRYPTOSHA2;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    } else {
                                                        if ((word & 0x80e00000u) == 0x00000000u) return A64_DEC_CRYPTOSHA3;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if ((word & 0x80000000u) == 0x00000000u) return A64_DEC_LOADLIT;
                            }
                        } else {
                            if (word & 0x80000000u) {
                                if (word & 0x02000000u) {
                                    if (word & 0x00010000u) {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0006fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0006fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if ((word & 0x0086fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x00060000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        } else {
                                                            if ((word & 0x00060000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        }
                                                    } else {
                                                        if ((word & 0x00860000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00400000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x0026fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    } else {
                                                        if ((word & 0x0026fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x00a4fc00u) == 0x00a40000u) return A64_DEC_FLOAT2INT;
                                                    } else {
                                                        if ((word & 0x00a4fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00020000u) {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00040000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0050fc00u) == 0x00400000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0050fc00u) == 0x00400000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0010fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0010fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x0090fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x00140000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        } else {
                                                            if ((word & 0x00140000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        }
                                                    } else {
                                                        if ((word & 0x00940000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00040000u) {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0030fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0030fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if ((word & 0x00b0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00100000u) {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x00a0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x00a0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00080000u) {
                                            if (word & 0x00100000u) {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0006fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0006fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if ((word & 0x0086fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x00060000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        } else {
                                                            if ((word & 0x00060000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        }
                                                    } else {
                                                        if ((word & 0x00860000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00400000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x0026fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    } else {
                                                        if ((word & 0x0026fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x00a4fc00u) == 0x00a40000u) return A64_DEC_FLOAT2INT;
                                                    } else {
                                                        if ((word & 0x00a4fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00020000u) {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00040000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0050fc00u) == 0x00400000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0050fc00u) == 0x00400000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0010fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0010fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x0090fc00u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x00140000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        } else {
                                                            if ((word & 0x00140000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                        }
                                                    } else {
                                                        if ((word & 0x00940000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00040000u) {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x0030fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        } else {
                                                            if ((word & 0x0030fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if ((word & 0x00b0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                    }
                                                } else {
                                                    if (word & 0x00100000u) {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x00a0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    } else {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            } else {
                                                                if ((word & 0x0020fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                            }
                                                        } else {
                                                            if ((word & 0x00a0fc00u) == 0x00200000u) return A64_DEC_FLOAT2INT;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            } else {
                                if (word & 0x02000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_FLOATSEL;
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            } else {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            } else {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if (word & 0x00008000u) {
                                                                        return A64_DEC_FLOATDP2;
                                                                    } else {
                                                                        return A64_DEC_FLOATDP2;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        if (word & 0x00000010u) {
                                                            return A64_DEC_FLOATCCMP;
                                                        } else {
                                                            return A64_DEC_FLOATCCMP;
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if ((word & 0x000003e0u) == 0x00000000u) return A64_DEC_FLOATIMM;
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if (word & 0x00008000u) {
                                                                    if (word & 0x00010000u) {
                                                                        if (word & 0x00020000u) {
                                                                            if ((word & 0x001c2000u) == 0x00040000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00020000u) {
                                                                            if ((word & 0x001c2000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00010000u) {
                                                                        if (word & 0x00020000u) {
                                                                            if ((word & 0x001c2000u) == 0x00040000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00020000u) {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00002000u) {
                                                                    if (word & 0x00000008u) {
                                                                        if (word & 0x00000010u) {
                                                                            if ((word & 0x001f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        } else {
                                                                            if ((word & 0x001f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00000010u) {
                                                                            if ((word & 0x00008007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        } else {
                                                                            if ((word & 0x00008007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00010000u) {
                                                                        if (word & 0x00020000u) {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00188000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if ((word & 0x00188000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00080000u) {
                                                                                if (word & 0x00100000u) {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                }
                                                                            } else {
                                                                                if (word & 0x00040000u) {
                                                                                    if ((word & 0x00108000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if (word & 0x00100000u) {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    } else {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00020000u) {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00188000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if ((word & 0x00188000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00080000u) {
                                                                                if (word & 0x00100000u) {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                }
                                                                            } else {
                                                                                if (word & 0x00040000u) {
                                                                                    if ((word & 0x00108000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if (word & 0x00100000u) {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    } else {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x001c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    } else {
                                                        if ((word & 0x001c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                } else {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x001c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    } else {
                                                        if ((word & 0x001c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_FLOATSEL;
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            } else {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            } else {
                                                                if (word & 0x00004000u) {
                                                                    if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if (word & 0x00008000u) {
                                                                        return A64_DEC_FLOATDP2;
                                                                    } else {
                                                                        return A64_DEC_FLOATDP2;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        if (word & 0x00000010u) {
                                                            return A64_DEC_FLOATCCMP;
                                                        } else {
                                                            return A64_DEC_FLOATCCMP;
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if ((word & 0x000003e0u) == 0x00000000u) return A64_DEC_FLOATIMM;
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if (word & 0x00008000u) {
                                                                    if (word & 0x00040000u) {
                                                                        if (word & 0x00010000u) {
                                                                            if (word & 0x00020000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        } else {
                                                                            if ((word & 0x001a2000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00010000u) {
                                                                            if (word & 0x00020000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        } else {
                                                                            if ((word & 0x001a2000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00010000u) {
                                                                        if (word & 0x00020000u) {
                                                                            if ((word & 0x001c2000u) == 0x00040000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00020000u) {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            } else {
                                                                                if ((word & 0x00182000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00002000u) {
                                                                    if (word & 0x00000008u) {
                                                                        if (word & 0x00000010u) {
                                                                            if ((word & 0x001f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        } else {
                                                                            if ((word & 0x001f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00000010u) {
                                                                            if ((word & 0x00008007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        } else {
                                                                            if ((word & 0x00008007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00010000u) {
                                                                        if (word & 0x00080000u) {
                                                                            if (word & 0x00100000u) {
                                                                                if ((word & 0x00068000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if ((word & 0x00068000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00020000u) {
                                                                                if ((word & 0x00148000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if (word & 0x00040000u) {
                                                                                    if ((word & 0x00108000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if (word & 0x00100000u) {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    } else {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00080000u) {
                                                                            if (word & 0x00020000u) {
                                                                                if ((word & 0x00148000u) == 0x00140000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if (word & 0x00100000u) {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if ((word & 0x00048000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                }
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00020000u) {
                                                                                if ((word & 0x00148000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if (word & 0x00040000u) {
                                                                                    if ((word & 0x00108000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if (word & 0x00100000u) {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    } else {
                                                                                        if ((word & 0x00008000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x001c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    } else {
                                                        if ((word & 0x001c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                } else {
                                                    if (word & 0x00020000u) {
                                                        if ((word & 0x001c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                    } else {
                                                        if ((word & 0x001c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00000400u) {
                                                    if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_FLOATSEL;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            } else {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            } else {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            } else {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                } else {
                                                                    if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_FLOATDP2;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000010u) {
                                                        if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_FLOATCCMP;
                                                    } else {
                                                        if ((word & 0x00800000u) == 0x00000000u) return A64_DEC_FLOATCCMP;
                                                    }
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if ((word & 0x008003e0u) == 0x00000000u) return A64_DEC_FLOATIMM;
                                                    } else {
                                                        if (word & 0x00004000u) {
                                                            if (word & 0x00008000u) {
                                                                if (word & 0x00010000u) {
                                                                    if (word & 0x00020000u) {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00020000u) {
                                                                        if ((word & 0x009c2000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                    } else {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00010000u) {
                                                                    if (word & 0x00020000u) {
                                                                        if ((word & 0x009c2000u) == 0x00040000u) return A64_DEC_FLOATDP1;
                                                                    } else {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00020000u) {
                                                                        if ((word & 0x009c2000u) == 0x00040000u) return A64_DEC_FLOATDP1;
                                                                    } else {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        } else {
                                                                            if ((word & 0x00982000u) == 0x00000000u) return A64_DEC_FLOATDP1;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if (word & 0x00000008u) {
                                                                    if (word & 0x00000010u) {
                                                                        if ((word & 0x009f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                    } else {
                                                                        if ((word & 0x009f8007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                    }
                                                                } else {
                                                                    if (word & 0x00000010u) {
                                                                        if ((word & 0x00808007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                    } else {
                                                                        if ((word & 0x00808007u) == 0x00000000u) return A64_DEC_FLOATCMP;
                                                                    }
                                                                }
                                                            } else {
                                                                if (word & 0x00010000u) {
                                                                    if (word & 0x00020000u) {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00988000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                        } else {
                                                                            if ((word & 0x00988000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00080000u) {
                                                                            if (word & 0x00100000u) {
                                                                                if ((word & 0x00848000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if ((word & 0x00848000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00908000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if (word & 0x00100000u) {
                                                                                    if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                } else {
                                                                    if (word & 0x00020000u) {
                                                                        if (word & 0x00040000u) {
                                                                            if ((word & 0x00988000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                        } else {
                                                                            if ((word & 0x00988000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                        }
                                                                    } else {
                                                                        if (word & 0x00080000u) {
                                                                            if (word & 0x00100000u) {
                                                                                if ((word & 0x00848000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if ((word & 0x00848000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            }
                                                                        } else {
                                                                            if (word & 0x00040000u) {
                                                                                if ((word & 0x00908000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                            } else {
                                                                                if (word & 0x00100000u) {
                                                                                    if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                } else {
                                                                                    if ((word & 0x00808000u) == 0x00000000u) return A64_DEC_FLOAT2INT;
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00010000u) {
                                                if (word & 0x00020000u) {
                                                    if ((word & 0x009c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                } else {
                                                    if ((word & 0x009c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                }
                                            } else {
                                                if (word & 0x00020000u) {
                                                    if ((word & 0x009c0000u) == 0x00000000u) return A64_DEC_FLOAT2FIX;
                                                } else {
                                                    if ((word & 0x009c0000u) == 0x00180000u) return A64_DEC_FLOAT2FIX;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x40000000u) {
                        if (word & 0x01000000u) {
                            if (word & 0x00100000u) {
                                if (word & 0x00200000u) {
                                    if ((word & 0x82c00000u) == 0x80000000u) return A64_DEC_SYSTEMMOVE_MRS;
                                } else {
                                    if ((word & 0x82c00000u) == 0x80000000u) return A64_DEC_SYSTEMMOVE_MSR;
                                }
                            } else {
                                if (word & 0x00080000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0x82c00000u) == 0x80000000u) return A64_DEC_SYSTEMINSTRS_SYSL;
                                    } else {
                                        if ((word & 0x82c00000u) == 0x80000000u) return A64_DEC_SYSTEMINSTRS_SYS;
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00000040u) {
                                            if (word & 0x00000080u) {
                                                if ((word & 0x82e7e03fu) == 0x8003201fu) return A64_DEC_BARRIERS_ISB;
                                            } else {
                                                if ((word & 0x82e7e03fu) == 0x8003201fu) return A64_DEC_BARRIERS_CLREX;
                                            }
                                        } else {
                                            if (word & 0x00000020u) {
                                                if ((word & 0x82e7e09fu) == 0x8003209fu) return A64_DEC_BARRIERS_DMB;
                                            } else {
                                                if ((word & 0x82e7e09fu) == 0x8003209fu) return A64_DEC_BARRIERS_DSB;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x82e7cfffu) == 0x8003029fu) return A64_DEC_HINT_NOP;
                                            if ((word & 0x82e7cfffu) == 0x8003021fu) return A64_DEC_HINT_NOP;
                                            if ((word & 0x82e7cfffu) == 0x8003001fu) return A64_DEC_HINT_NOP;
                                            if ((word & 0x82e7cfffu) == 0x800300bfu) return A64_DEC_HINTS_SEVL;
                                            if ((word & 0x82e7cfffu) == 0x8003009fu) return A64_DEC_HINTS_SEV;
                                            if ((word & 0x82e7cfffu) == 0x8003005fu) return A64_DEC_HINTS_WFE;
                                            if ((word & 0x82e7cfffu) == 0x8003007fu) return A64_DEC_HINTS_WFI;
                                            if ((word & 0x82e7cfffu) == 0x8003003fu) return A64_DEC_HINT_NOP;
                                            if ((word & 0x82e7c01fu) == 0x8003001fu) return A64_DEC_HINT_NOP;
                                        } else {
                                            if ((word & 0x82e0c01fu) == 0x8000401fu && !((((((word >> 16) & 7u) == 0x0u)) && ((((((word >> 5) & 7u) & 0x6u) == 0x0u) || (((word >> 5) & 7u) == 0x2u)))))) return A64_DEC_PSTATE_MSR;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x805fffffu) == 0x801f03e0u) return A64_DEC_BRANCH_REG_DRPS;
                                    } else {
                                        if ((word & 0x805ffc1fu) == 0x801f0000u) return A64_DEC_BRANCH_REG_BLR;
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if ((word & 0x809ffc1fu) == 0x801f0000u) return A64_DEC_BRANCH_REG_RET;
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x801fffffu) == 0x801f03e0u) return A64_DEC_BRANCH_REG_ERET;
                                        } else {
                                            if ((word & 0x801ffc1fu) == 0x801f0000u) return A64_DEC_BRANCH_REG_BR;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if (word & 0x00000001u) {
                                        if (word & 0x00000002u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x00c0001cu) == 0x00800000u) return A64_DEC_EXCEPTION_DCPS3;
                                            } else {
                                                if ((word & 0x00c0001cu) == 0x00000000u) return A64_DEC_EXCEPTION_SMC;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x00c0001cu) == 0x00800000u) return A64_DEC_EXCEPTION_DCPS1;
                                            } else {
                                                if ((word & 0x00c0001cu) == 0x00000000u) return A64_DEC_EXCEPTION_SVC;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000002u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x00c0001cu) == 0x00800000u) return A64_DEC_EXCEPTION_DCPS2;
                                            } else {
                                                if ((word & 0x00c0001cu) == 0x00000000u) return A64_DEC_EXCEPTION_HVC;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x00c0001cu) == 0x00000000u) return A64_DEC_EXCEPTION_BRK;
                                            } else {
                                                if ((word & 0x00c0001cu) == 0x00400000u) return A64_DEC_EXCEPTION_HLT;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0x00000010u) == 0x00000000u) return A64_DEC_B_COND;
                                }
                            }
                        }
                    } else {
                        if (word & 0x80000000u) {
                            return A64_DEC_BRANCH_IMM_BL;
                        } else {
                            return A64_DEC_BRANCH_IMM_B;
                        }
                    }
                }
            }
        } else {
            if (word & 0x01000000u) {
                if (word & 0x00800000u) {
                    if (word & 0x00400000u) {
                        if (word & 0x02000000u) {
                            if (word & 0x08000000u) {
                                if ((word & 0xe020fc00u) == 0x80007c00u) return A64_DEC_DP_3SRC_UMULH;
                            } else {
                                if ((word & 0xe0200000u) == 0x80000000u && (((word >> 29) & 3u) != 0x3u)) return A64_DEC_EXTRACT_EXTR;
                            }
                        } else {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x08208c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x08208c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x08208c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x08208c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_SET;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x20000000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x0a000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if ((word & 0x0a000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x0a000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                } else {
                                    if ((word & 0x0a000000u) == 0x08000000u) return A64_DEC_LDST_POS;
                                }
                            }
                        } else {
                            if (word & 0x00008000u) {
                                if (word & 0x00200000u) {
                                    if ((word & 0xca000000u) == 0x8a000000u) return A64_DEC_DP_3SRC_UMSUBL;
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x0a000c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x02000000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0xc8000000u) == 0x88000000u) return A64_DEC_DP_3SRC_UMADDL;
                                    } else {
                                        if ((word & 0xc8000000u) == 0x00000000u && (((word >> 29) & 3u) != 0x3u)) return A64_DEC_EXTRACT_EXTR;
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x08200c00u) == 0x08000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x08000000u) {
                        if (word & 0x00400000u) {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x02000000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    } else {
                                        if ((word & 0x02000000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    }
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x02000000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    } else {
                                        if ((word & 0x02000000u) == 0x00000000u) return A64_DEC_LDST_POS;
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00000800u) {
                                                if ((word & 0xc2208400u) == 0x82000400u) return A64_DEC_DP_3SRC_SMULH;
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x02200400u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x02200400u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            } else {
                                                if ((word & 0x02200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00200000u) {
                                        if ((word & 0xe0000000u) == 0x80000000u) return A64_DEC_DP_3SRC_SMSUBL;
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x60000000u) == 0x00000000u) return A64_DEC_DP_3SRC_MSUB;
                                        } else {
                                            if ((word & 0x60000000u) == 0x00000000u) return A64_DEC_DP_3SRC_MSUB;
                                        }
                                    }
                                } else {
                                    if (word & 0x00200000u) {
                                        if ((word & 0xe0000000u) == 0x80000000u) return A64_DEC_DP_3SRC_SMADDL;
                                    } else {
                                        if (word & 0x80000000u) {
                                            if ((word & 0x60000000u) == 0x00000000u) return A64_DEC_DP_3SRC_MADD;
                                        } else {
                                            if ((word & 0x60000000u) == 0x00000000u) return A64_DEC_DP_3SRC_MADD;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x20000000u) {
                                    if (word & 0x40000000u) {
                                        if (word & 0x80000000u) {
                                            return A64_DEC_LDST_POS;
                                        } else {
                                            return A64_DEC_LDST_POS;
                                        }
                                    } else {
                                        if (word & 0x80000000u) {
                                            return A64_DEC_LDST_POS;
                                        } else {
                                            return A64_DEC_LDST_POS;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                } else {
                                                    if ((word & 0x00200c00u) == 0x00000400u && ((word >> 30) & 3u) == 0x0u) return A64_DEC_MOPS_CPY;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x80000000u) {
                            if (word & 0x02000000u) {
                                if (word & 0x20000000u) {
                                    if ((word & 0x40400000u) == 0x00400000u) return A64_DEC_BITFIELD_BFM;
                                } else {
                                    if (word & 0x40000000u) {
                                        if ((word & 0x00400000u) == 0x00400000u) return A64_DEC_BITFIELD_UBFM;
                                    } else {
                                        if ((word & 0x00400000u) == 0x00400000u) return A64_DEC_BITFIELD_SBFM;
                                    }
                                }
                            } else {
                                if (word & 0x20000000u) {
                                    if (word & 0x40000000u) {
                                        return A64_DEC_ADDSUB_IMM_SUBS;
                                    } else {
                                        return A64_DEC_ADDSUB_IMM_ADDS;
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        return A64_DEC_ADDSUB_IMM_SUB;
                                    } else {
                                        return A64_DEC_ADDSUB_IMM_ADD;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x02000000u) {
                                if (word & 0x20000000u) {
                                    if ((word & 0x40400000u) == 0x00000000u) return A64_DEC_BITFIELD_BFM;
                                } else {
                                    if (word & 0x40000000u) {
                                        if ((word & 0x00400000u) == 0x00000000u) return A64_DEC_BITFIELD_UBFM;
                                    } else {
                                        if ((word & 0x00400000u) == 0x00000000u) return A64_DEC_BITFIELD_SBFM;
                                    }
                                }
                            } else {
                                if (word & 0x20000000u) {
                                    if (word & 0x40000000u) {
                                        return A64_DEC_ADDSUB_IMM_SUBS;
                                    } else {
                                        return A64_DEC_ADDSUB_IMM_ADDS;
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        return A64_DEC_ADDSUB_IMM_SUB;
                                    } else {
                                        return A64_DEC_ADDSUB_IMM_ADD;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x80000000u) {
                    if (word & 0x02000000u) {
                        if (word & 0x40000000u) {
                            if (word & 0x00800000u) {
                                if (word & 0x08000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x207ff000u) == 0x00400000u) return A64_DEC_DP_1SRC_REV;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00001000u) {
                                                    if ((word & 0x203fe000u) == 0x00000000u) return A64_DEC_DP_1SRC_CLS;
                                                } else {
                                                    if ((word & 0x203fe000u) == 0x00000000u) return A64_DEC_DP_1SRC_REV16;
                                                }
                                            } else {
                                                if ((word & 0x20200000u) == 0x00000000u) return A64_DEC_CONDSEL_CSNEG;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x207ff000u) == 0x00400000u) return A64_DEC_DP_1SRC_REV32;
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00001000u) {
                                                    if ((word & 0x203fe000u) == 0x00000000u) return A64_DEC_DP_1SRC_CLZ;
                                                } else {
                                                    if ((word & 0x203fe000u) == 0x00000000u) return A64_DEC_DP_1SRC_RBIT;
                                                }
                                            } else {
                                                if ((word & 0x20200000u) == 0x00000000u) return A64_DEC_CONDSEL_CSINV;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        return A64_DEC_MOVEWIDE_MOVK;
                                    } else {
                                        return A64_DEC_MOVEWIDE_MOVZ;
                                    }
                                }
                            } else {
                                if (word & 0x08000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x20200410u) == 0x20000000u) return A64_DEC_CONDCMP_IMM_CCMP;
                                        } else {
                                            if ((word & 0x20200410u) == 0x20000000u) return A64_DEC_CONDCMP_REG_CCMP;
                                        }
                                    } else {
                                        if (word & 0x20000000u) {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_SBCS;
                                        } else {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_SBC;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        return A64_DEC_LOG_IMM_ANDS;
                                    } else {
                                        return A64_DEC_LOG_IMM_EOR;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00800000u) {
                                if (word & 0x08000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00004000u) {
                                                if (word & 0x00001000u) {
                                                    if ((word & 0x2060a000u) == 0x00400000u) return A64_DEC_DP_2SRC_CRC32CX;
                                                } else {
                                                    if ((word & 0x2060a000u) == 0x00400000u) return A64_DEC_DP_2SRC_CRC32X;
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if ((word & 0x20609000u) == 0x00400000u) return A64_DEC_DP_2SRC_RORV;
                                                } else {
                                                    if ((word & 0x20609000u) == 0x00400000u) return A64_DEC_DP_2SRC_SDIV;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x2020f000u) == 0x00002000u) return A64_DEC_DP_2SRC_LSRV;
                                            } else {
                                                if ((word & 0x20200000u) == 0x00000000u) return A64_DEC_CONDSEL_CSINC;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x2060d000u) == 0x00400000u) return A64_DEC_DP_2SRC_ASRV;
                                            } else {
                                                if ((word & 0x2060d000u) == 0x00400000u) return A64_DEC_DP_2SRC_UDIV;
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if ((word & 0x2020f000u) == 0x00002000u) return A64_DEC_DP_2SRC_LSLV;
                                            } else {
                                                if ((word & 0x20200000u) == 0x00000000u) return A64_DEC_CONDSEL_CSEL;
                                            }
                                        }
                                    }
                                } else {
                                    if ((word & 0x20000000u) == 0x00000000u) return A64_DEC_MOVEWIDE_MOVN;
                                }
                            } else {
                                if (word & 0x08000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x20200410u) == 0x20000000u) return A64_DEC_CONDCMP_IMM_CCMN;
                                        } else {
                                            if ((word & 0x20200410u) == 0x20000000u) return A64_DEC_CONDCMP_REG_CCMN;
                                        }
                                    } else {
                                        if (word & 0x20000000u) {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_ADCS;
                                        } else {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_ADC;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        return A64_DEC_LOG_IMM_ORR;
                                    } else {
                                        return A64_DEC_LOG_IMM_AND;
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x08000000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x20000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x00200c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                        } else {
                                                            if ((word & 0x00200c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00000800u) {
                                                if ((word & 0x00204400u) == 0x00204000u && !((((((word >> 0) & 31u) & 0x18u) == 0x18u)))) return A64_DEC_LDST_REGOFF;
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00004000u) {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x001f0400u) == 0x001f0000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_UNSCALED;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            } else {
                                if (word & 0x20000000u) {
                                    if (word & 0x00800000u) {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    }
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if ((word & 0x00208c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x00200c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                        } else {
                                                            if ((word & 0x00200c00u) == 0x00200000u) return A64_DEC_LSE_MEMOP;
                                                        }
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x001f0400u) == 0x001f0000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00400000u) {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            }
                        } else {
                            return A64_DEC_PCRELADDR_ADRP;
                        }
                    }
                } else {
                    if (word & 0x02000000u) {
                        if (word & 0x00400000u) {
                            if (word & 0x00000800u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00002000u) {
                                        if ((word & 0x68a0d000u) == 0x08800000u) return A64_DEC_DP_2SRC_RORV;
                                    } else {
                                        if ((word & 0x68a0d000u) == 0x08800000u) return A64_DEC_DP_2SRC_SDIV;
                                    }
                                } else {
                                    if (word & 0x00800000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x6820a000u) == 0x08000000u) return A64_DEC_DP_2SRC_CRC32CW;
                                            } else {
                                                if ((word & 0x6820a000u) == 0x08000000u) return A64_DEC_DP_2SRC_CRC32W;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x68209000u) == 0x08000000u) return A64_DEC_DP_2SRC_ASRV;
                                            } else {
                                                if (word & 0x40000000u) {
                                                    if ((word & 0x283f9000u) == 0x08000000u) return A64_DEC_DP_1SRC_REV;
                                                } else {
                                                    if ((word & 0x28209000u) == 0x08000000u) return A64_DEC_DP_2SRC_UDIV;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x28200010u) == 0x28000000u) return A64_DEC_CONDCMP_IMM_CCMP;
                                        } else {
                                            if ((word & 0x28200010u) == 0x28000000u) return A64_DEC_CONDCMP_IMM_CCMN;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x68a0a000u) == 0x08800000u) return A64_DEC_DP_2SRC_CRC32CH;
                                        } else {
                                            if ((word & 0x68bfa000u) == 0x48800000u) return A64_DEC_DP_1SRC_CLS;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x68a0c000u) == 0x08800000u) return A64_DEC_DP_2SRC_LSRV;
                                        } else {
                                            if (word & 0x00004000u) {
                                                if ((word & 0x68a08000u) == 0x08800000u) return A64_DEC_DP_2SRC_CRC32H;
                                            } else {
                                                if ((word & 0x68bf8000u) == 0x48800000u) return A64_DEC_DP_1SRC_REV16;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x40000000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x283fe000u) == 0x08000000u) return A64_DEC_DP_1SRC_CLZ;
                                            } else {
                                                if ((word & 0x283fe000u) == 0x08000000u) return A64_DEC_DP_1SRC_RBIT;
                                            }
                                        } else {
                                            if ((word & 0x28200010u) == 0x28000000u) return A64_DEC_CONDCMP_REG_CCMP;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x2820e000u) == 0x08004000u) return A64_DEC_DP_2SRC_CRC32CB;
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if ((word & 0x2820c000u) == 0x08000000u) return A64_DEC_DP_2SRC_LSLV;
                                                } else {
                                                    if ((word & 0x2820c000u) == 0x08004000u) return A64_DEC_DP_2SRC_CRC32B;
                                                }
                                            }
                                        } else {
                                            if ((word & 0x28200010u) == 0x28000000u) return A64_DEC_CONDCMP_REG_CCMN;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00800000u) {
                                if (word & 0x08000000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x20200800u) == 0x00000000u) return A64_DEC_CONDSEL_CSNEG;
                                        } else {
                                            if ((word & 0x20200800u) == 0x00000000u) return A64_DEC_CONDSEL_CSINC;
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x20200800u) == 0x00000000u) return A64_DEC_CONDSEL_CSINV;
                                        } else {
                                            if ((word & 0x20200800u) == 0x00000000u) return A64_DEC_CONDSEL_CSEL;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x40000000u) == 0x40000000u) return A64_DEC_MOVEWIDE_MOVK;
                                    } else {
                                        if (word & 0x40000000u) {
                                            return A64_DEC_MOVEWIDE_MOVZ;
                                        } else {
                                            return A64_DEC_MOVEWIDE_MOVN;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x08000000u) {
                                    if (word & 0x20000000u) {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_SBCS;
                                        } else {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_ADCS;
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_SBC;
                                        } else {
                                            if ((word & 0x0020fc00u) == 0x00000000u) return A64_DEC_ADDSUB_CARRY_ADC;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if (word & 0x40000000u) {
                                            return A64_DEC_LOG_IMM_ANDS;
                                        } else {
                                            return A64_DEC_LOG_IMM_ORR;
                                        }
                                    } else {
                                        if (word & 0x40000000u) {
                                            return A64_DEC_LOG_IMM_EOR;
                                        } else {
                                            return A64_DEC_LOG_IMM_AND;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x08000000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x20000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00004000u) {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x001f0400u) == 0x001f0000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            } else {
                                if (word & 0x20000000u) {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x0000e400u) == 0x00006000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00000400u) == 0x00000000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x0000e400u) == 0x00006000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00000400u) == 0x00000000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x0000e400u) == 0x00006000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00000400u) == 0x00000000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00004000u) {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x001f0400u) == 0x001f0000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00001000u) {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00002000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x0000e400u) == 0x00006000u) return A64_DEC_LDST_REGOFF;
                                                    if ((word & 0x00000400u) == 0x00000000u && (((word >> 13) & 7u) != 0x3u)) return A64_DEC_LDST_REGOFF;
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPRE;
                                                    } else {
                                                        return A64_DEC_LDST_UNPRIV;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if (word & 0x00001000u) {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00002000u) {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            }
                                                        } else {
                                                            if (word & 0x00004000u) {
                                                                if ((word & 0x00008400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                            } else {
                                                                if (word & 0x00008000u) {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                } else {
                                                                    if ((word & 0x00000400u) == 0x00000000u) return A64_DEC_LSE_MEMOP;
                                                                }
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000400u) {
                                                        return A64_DEC_LDST_IMMPOST;
                                                    } else {
                                                        return A64_DEC_LDST_UNSCALED;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    return A64_DEC_LOADLIT;
                                }
                            }
                        } else {
                            return A64_DEC_PCRELADDR_ADR;
                        }
                    }
                }
            }
        }
    } else {
        if (word & 0x02000000u) {
            if (word & 0x20000000u) {
                if (word & 0x01000000u) {
                    if (word & 0x04000000u) {
                        if (word & 0x00008000u) {
                            if (word & 0x00004000u) {
                                if (word & 0x00000400u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x88803000u) == 0x08003000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0xc8f80000u) == 0x48000000u) return A64_DEC_ASIMDIMM;
                                            } else {
                                                if ((word & 0xc8f80000u) == 0x48000000u) return A64_DEC_ASIMDIMM;
                                                if ((word & 0xc8f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                                if ((word & 0x88800000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                            }
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88c00000u) == 0x08800000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        }
                                    } else {
                                        if (word & 0x00800000u) {
                                            if ((word & 0x88002000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88402000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                            if ((word & 0x88802000u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88c00000u) == 0x08800000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00001000u) {
                                if (word & 0x00000400u) {
                                    if ((word & 0x88f80800u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                    if ((word & 0x88806800u) == 0x08004000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08006000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                } else {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                }
                            } else {
                                if (word & 0x00000400u) {
                                    if ((word & 0x88f80800u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                    if ((word & 0x88806800u) == 0x08006000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08004000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_SUBS;
                                } else {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_SUBS;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_ADDS;
                                } else {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_ADDS;
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_SUBS;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_SUBS;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_ADDS;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_ADDS;
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00200000u) {
                        if (word & 0x04000000u) {
                            if (word & 0x00001000u) {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88800000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x88d60800u) == 0x08d00800u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if ((word & 0x88960800u) == 0x08800800u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x88d60800u) == 0x08d00800u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0xc8460800u) == 0x48000800u) return A64_DEC_ASIMDALL;
                                                            } else {
                                                                if ((word & 0xc8460800u) == 0x48000800u) return A64_DEC_ASIMDALL;
                                                            }
                                                        } else {
                                                            if ((word & 0x88860800u) == 0x08800800u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x881f0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x88000800u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                            } else {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88570800u) == 0x08510800u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if ((word & 0x88570800u) == 0x08510800u) return A64_DEC_ASIMDMISCFP16;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88160800u) == 0x08000800u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x88160800u) == 0x08000800u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x88160800u) == 0x08000800u) return A64_DEC_ASIMDMISC;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if ((word & 0x880e0000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                        } else {
                                                            if ((word & 0x880e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88800000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88560800u) == 0x08500800u) return A64_DEC_ASIMDMISCFP16;
                                                        } else {
                                                            if ((word & 0x88560800u) == 0x08500800u) return A64_DEC_ASIMDMISCFP16;
                                                        }
                                                    } else {
                                                        if ((word & 0x88d60800u) == 0x08d00800u) return A64_DEC_ASIMDMISCFP16;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88160800u) == 0x08000800u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x88160800u) == 0x08000800u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x88960800u) == 0x08800800u) return A64_DEC_ASIMDMISC;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00400000u) {
                                                        if ((word & 0x889f0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if ((word & 0x889f0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if (word & 0x00400000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00080000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88578000u) == 0x08518000u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if ((word & 0x88578000u) == 0x08518000u) return A64_DEC_ASIMDMISCFP16;
                                                    }
                                                } else {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x88168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x88168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                }
                                            } else {
                                                if ((word & 0x88008000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00004000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88400000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88400000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x88d62000u) == 0x08500000u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88162000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x88162000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x88d62000u) == 0x08d00000u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0xc8462000u) == 0x48000000u) return A64_DEC_ASIMDALL;
                                                            } else {
                                                                if ((word & 0xc8462000u) == 0x48000000u) return A64_DEC_ASIMDALL;
                                                            }
                                                        } else {
                                                            if ((word & 0x88862000u) == 0x08800000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                }
                                            } else {
                                                if ((word & 0x88002000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x88de0000u) == 0x08400000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00080000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x88570000u) == 0x08510000u) return A64_DEC_ASIMDMISCFP16;
                                                        } else {
                                                            if ((word & 0x88570000u) == 0x08510000u) return A64_DEC_ASIMDMISCFP16;
                                                        }
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00010000u) {
                                                                if ((word & 0x88060000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                            } else {
                                                                if ((word & 0x88060000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                            }
                                                        } else {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x88070000u) == 0x08010000u) return A64_DEC_ASIMDMISC;
                                                            } else {
                                                                if ((word & 0x88070000u) == 0x08010000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000800u) {
                                                if (word & 0x00010000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x88d68000u) == 0x08508000u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if ((word & 0x88968000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    } else {
                                                        if ((word & 0x881e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                } else {
                                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_BICS;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_BICS;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ORN;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ORN;
                                }
                            }
                        }
                    } else {
                        if (word & 0x04000000u) {
                            if (word & 0x00008000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x00002000u) {
                                        if ((word & 0x88000c00u) == 0x08000400u) return A64_DEC_ASIMDSAME2;
                                    } else {
                                        if ((word & 0x88000400u) == 0x08000400u) return A64_DEC_ASIMDSAME2;
                                    }
                                } else {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x88003400u) == 0x08000400u) return A64_DEC_ASIMDSAME2;
                                    } else {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x88002400u) == 0x08000400u) return A64_DEC_ASIMDSAME2;
                                        } else {
                                            if ((word & 0x88002400u) == 0x08000400u) return A64_DEC_ASIMDSAME2;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00800000u) {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88404c00u) == 0x08400400u) return A64_DEC_ASIMDSAMEFP16;
                                        } else {
                                            if ((word & 0x88404c00u) == 0x08400400u) return A64_DEC_ASIMDSAMEFP16;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x88406400u) == 0x08402400u) return A64_DEC_ASIMDSAMEFP16;
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x88404400u) == 0x08400400u) return A64_DEC_ASIMDSAMEFP16;
                                            } else {
                                                if ((word & 0x88404400u) == 0x08400400u) return A64_DEC_ASIMDSAMEFP16;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00400000u) {
                                        if (word & 0x00000800u) {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                } else {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            } else {
                                                if ((word & 0x88006400u) == 0x08002400u) return A64_DEC_ASIMDSAMEFP16;
                                            }
                                        } else {
                                            if (word & 0x00001000u) {
                                                if (word & 0x00002000u) {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                } else {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                } else {
                                                    if ((word & 0x88004400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00000400u) {
                                            if ((word & 0xc8000000u) == 0x48000000u) return A64_DEC_ASIMDINS;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDEXT;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ANDS;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ANDS;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ORR;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_ORR;
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x01000000u) {
                    if (word & 0x04000000u) {
                        if (word & 0x00000400u) {
                            if (word & 0x00008000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x00000800u) {
                                        if ((word & 0x88f83000u) == 0x08003000u) return A64_DEC_ASIMDIMM;
                                        if ((word & 0x88803000u) == 0x08003000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00001000u) {
                                                if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                            } else {
                                                if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                                if ((word & 0x88800000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                            }
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        } else {
                                            if ((word & 0x88f80000u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                            if ((word & 0x88802000u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                            if ((word & 0x88802000u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00001000u) {
                                    if ((word & 0x88f80800u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                    if ((word & 0x88806800u) == 0x08004000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08006000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                } else {
                                    if ((word & 0x88f80800u) == 0x08000000u) return A64_DEC_ASIMDIMM;
                                    if ((word & 0x88806800u) == 0x08002000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                    if ((word & 0x88806800u) == 0x08000000u && (((word >> 19) & 15u) != 0x0u) && (((((word >> 19) & 15u) != 0x0u)) && ((((word >> 19) & 15u) != 0x0u)))) return A64_DEC_ASIMDSHF;
                                }
                            }
                        } else {
                            if (word & 0x00001000u) {
                                if (word & 0x00004000u) {
                                    if (word & 0x00002000u) {
                                        if ((word & 0x88008000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            } else {
                                                if ((word & 0x88400000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            } else {
                                                if ((word & 0x88400000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if (word & 0x00800000u) {
                                                if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            } else {
                                                if ((word & 0x88400000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88c00000u) == 0x08800000u) return A64_DEC_ASIMDELEM;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_ASIMDELEM;
                                        } else {
                                            if ((word & 0x88c00000u) == 0x08800000u) return A64_DEC_ASIMDELEM;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x00200000u) {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_SUB;
                                } else {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_SUB;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_ADD;
                                } else {
                                    if ((word & 0x08c00000u) == 0x08000000u) return A64_DEC_ADDSUB_EXT_ADD;
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_SUB;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_SUB;
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_ADD;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ADDSUB_SHIFT_ADD;
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00200000u) {
                        if (word & 0x80000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x48c07000u) == 0x48400000u) return A64_DEC_CRYPTOSHA512_3;
                                        } else {
                                            if ((word & 0x48c07000u) == 0x48400000u) return A64_DEC_CRYPTOSHA512_3;
                                        }
                                    } else {
                                        if (word & 0x00000800u) {
                                            if ((word & 0x48c07000u) == 0x48400000u) return A64_DEC_CRYPTOSHA512_3;
                                        } else {
                                            if ((word & 0x48c07000u) == 0x48400000u) return A64_DEC_CRYPTOSHA512_3;
                                        }
                                    }
                                } else {
                                    if ((word & 0x48c00000u) == 0x48000000u) return A64_DEC_CRYPTO4;
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_EON;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_BIC;
                                }
                            }
                        } else {
                            if (word & 0x04000000u) {
                                if (word & 0x00001000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00080000u) {
                                                        if ((word & 0x08d70800u) == 0x08d00800u) return A64_DEC_ASIMDMISCFP16;
                                                    } else {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08470800u) == 0x08000800u) return A64_DEC_ASIMDALL;
                                                            } else {
                                                                if ((word & 0x08470800u) == 0x08000800u) return A64_DEC_ASIMDALL;
                                                            }
                                                        } else {
                                                            if ((word & 0x08870800u) == 0x08800800u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00010000u) {
                                                            if ((word & 0x089e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x48d60000u) == 0x48000000u) return A64_DEC_CRYPTOAES;
                                                            } else {
                                                                if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00080000u) {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x08560000u) == 0x08500000u) return A64_DEC_ASIMDMISCFP16;
                                                                } else {
                                                                    if ((word & 0x08560000u) == 0x08500000u) return A64_DEC_ASIMDMISCFP16;
                                                                }
                                                            } else {
                                                                if ((word & 0x08d60000u) == 0x08d00000u) return A64_DEC_ASIMDMISCFP16;
                                                            }
                                                        } else {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                } else {
                                                                    if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                }
                                                            } else {
                                                                if ((word & 0x08960000u) == 0x08800000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x48d70000u) == 0x48000000u) return A64_DEC_CRYPTOAES;
                                                        } else {
                                                            if ((word & 0x08170000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x08470000u) == 0x08410000u) return A64_DEC_ASIMDMISCFP16;
                                                                } else {
                                                                    if ((word & 0x08470000u) == 0x08410000u) return A64_DEC_ASIMDMISCFP16;
                                                                }
                                                            } else {
                                                                if ((word & 0x08070000u) == 0x08010000u) return A64_DEC_ASIMDALL;
                                                            }
                                                        } else {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                } else {
                                                                    if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                }
                                                            } else {
                                                                if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00100000u) {
                                                            if ((word & 0x080f0000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                        } else {
                                                            if ((word & 0x080f0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if (word & 0x00400000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                            } else {
                                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                            }
                                                        } else {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                            } else {
                                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00080000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08568000u) == 0x08508000u) return A64_DEC_ASIMDMISCFP16;
                                                            } else {
                                                                if ((word & 0x08568000u) == 0x08508000u) return A64_DEC_ASIMDMISCFP16;
                                                            }
                                                        } else {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                            } else {
                                                                if ((word & 0x08168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00002000u) {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08400000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08400000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08800000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00080000u) {
                                                            if ((word & 0x08d70000u) == 0x08d00000u) return A64_DEC_ASIMDMISCFP16;
                                                        } else {
                                                            if ((word & 0x08970000u) == 0x08800000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00010000u) {
                                                            if ((word & 0x089e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x48d60000u) == 0x48000000u) return A64_DEC_CRYPTOAES;
                                                            } else {
                                                                if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00100000u) {
                                                            if (word & 0x00080000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x08470000u) == 0x08410000u) return A64_DEC_ASIMDMISCFP16;
                                                                } else {
                                                                    if ((word & 0x08470000u) == 0x08410000u) return A64_DEC_ASIMDMISCFP16;
                                                                }
                                                            } else {
                                                                if (word & 0x00010000u) {
                                                                    if ((word & 0x08060000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                                } else {
                                                                    if ((word & 0x08060000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00010000u) {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                } else {
                                                                    if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                }
                                                            } else {
                                                                if ((word & 0x080e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000400u) {
                                                    if (word & 0x00000800u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00010000u) {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00004000u) {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if (word & 0x00800000u) {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        } else {
                                                            if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00010000u) {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x08d60000u) == 0x08500000u) return A64_DEC_ASIMDMISCFP16;
                                                            } else {
                                                                if (word & 0x00800000u) {
                                                                    if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                } else {
                                                                    if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                                }
                                                            }
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x08d60000u) == 0x08d00000u) return A64_DEC_ASIMDMISCFP16;
                                                            } else {
                                                                if (word & 0x00100000u) {
                                                                    if (word & 0x00800000u) {
                                                                        if ((word & 0x08460000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                                    } else {
                                                                        if ((word & 0x08460000u) == 0x08000000u) return A64_DEC_ASIMDALL;
                                                                    }
                                                                } else {
                                                                    if ((word & 0x08860000u) == 0x08800000u) return A64_DEC_ASIMDMISC;
                                                                }
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                } else {
                                                    if (word & 0x00000800u) {
                                                        if (word & 0x00010000u) {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if (word & 0x00080000u) {
                                                                if ((word & 0x48d60000u) == 0x48000000u) return A64_DEC_CRYPTOAES;
                                                            } else {
                                                                if ((word & 0x08160000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00000400u) {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDSAME;
                                                    }
                                                }
                                            } else {
                                                if (word & 0x00000800u) {
                                                    if (word & 0x00010000u) {
                                                        if (word & 0x00080000u) {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08568000u) == 0x08508000u) return A64_DEC_ASIMDMISCFP16;
                                                            } else {
                                                                if ((word & 0x08568000u) == 0x08508000u) return A64_DEC_ASIMDMISCFP16;
                                                            }
                                                        } else {
                                                            if (word & 0x00800000u) {
                                                                if ((word & 0x08168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                            } else {
                                                                if ((word & 0x08168000u) == 0x08008000u) return A64_DEC_ASIMDMISC;
                                                            }
                                                        }
                                                    } else {
                                                        if (word & 0x00008000u) {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        } else {
                                                            if ((word & 0x081e0000u) == 0x08000000u) return A64_DEC_ASIMDMISC;
                                                        }
                                                    }
                                                } else {
                                                    if (word & 0x00008000u) {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    } else {
                                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_ASIMDDIFF;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_EON;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_BIC;
                                }
                            }
                        }
                    } else {
                        if (word & 0x80000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x00400000u) {
                                    if ((word & 0x489ffc00u) == 0x48808000u) return A64_DEC_CRYPTOSHA512_2;
                                } else {
                                    if (word & 0x00800000u) {
                                        if ((word & 0x48000000u) == 0x48000000u) return A64_DEC_CRYPTO3_IMM6;
                                    } else {
                                        if ((word & 0x48008000u) == 0x48000000u) return A64_DEC_CRYPTO4;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_EOR;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_AND;
                                }
                            }
                        } else {
                            if (word & 0x04000000u) {
                                if (word & 0x00000800u) {
                                    if (word & 0x00001000u) {
                                        if (word & 0x00000400u) {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x0800c000u) == 0x08000000u) return A64_DEC_ASIMDSAMEFP16;
                                                    } else {
                                                        if ((word & 0x0800c000u) == 0x08000000u) return A64_DEC_ASIMDSAMEFP16;
                                                    }
                                                } else {
                                                    if ((word & 0x0880c000u) == 0x08000000u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            } else {
                                                if (word & 0x00002000u) {
                                                    if (word & 0x40000000u) {
                                                        if ((word & 0x088fc000u) == 0x08080000u) return A64_DEC_ASIMDINS;
                                                    } else {
                                                        if ((word & 0x0880c000u) == 0x08000000u) return A64_DEC_ASIMDINS;
                                                    }
                                                } else {
                                                    if ((word & 0x4880c000u) == 0x48000000u) return A64_DEC_ASIMDINS;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                } else {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                } else {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00002000u) {
                                            if (word & 0x00000400u) {
                                                if (word & 0x40000000u) {
                                                    if ((word & 0x08c0c000u) == 0x08000000u) return A64_DEC_ASIMDINS;
                                                } else {
                                                    if ((word & 0x08c0c000u) == 0x08000000u) return A64_DEC_ASIMDINS;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                } else {
                                                    if ((word & 0x08008000u) == 0x08000000u) return A64_DEC_ASIMDPERM;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00400000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x0800c400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                } else {
                                                    if ((word & 0x0800c400u) == 0x08000400u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            } else {
                                                if ((word & 0x0880c400u) == 0x08000400u) return A64_DEC_ASIMDINS;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00800000u) {
                                                    if ((word & 0x0840c000u) == 0x08400000u) return A64_DEC_ASIMDSAMEFP16;
                                                } else {
                                                    if ((word & 0x0840c000u) == 0x08400000u) return A64_DEC_ASIMDSAMEFP16;
                                                }
                                            } else {
                                                if (word & 0x00008000u) {
                                                    if ((word & 0x08004000u) == 0x08000000u) return A64_DEC_ASIMDSAME2;
                                                } else {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x08404000u) == 0x08400000u) return A64_DEC_ASIMDSAMEFP16;
                                                    } else {
                                                        if ((word & 0x08404000u) == 0x08400000u) return A64_DEC_ASIMDSAMEFP16;
                                                    }
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if ((word & 0x08c0c000u) == 0x08400000u) return A64_DEC_ASIMDSAMEFP16;
                                            } else {
                                                if (word & 0x00400000u) {
                                                    if (word & 0x00800000u) {
                                                        if ((word & 0x0800c000u) == 0x08000000u) return A64_DEC_ASIMDSAMEFP16;
                                                    } else {
                                                        if ((word & 0x0800c000u) == 0x08000000u) return A64_DEC_ASIMDSAMEFP16;
                                                    }
                                                } else {
                                                    if ((word & 0x0880c000u) == 0x08000000u) return A64_DEC_ASIMDINS;
                                                }
                                            }
                                        }
                                    } else {
                                        if (word & 0x00001000u) {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                } else {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                } else {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                }
                                            }
                                        } else {
                                            if (word & 0x00002000u) {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                } else {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                }
                                            } else {
                                                if (word & 0x00004000u) {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                } else {
                                                    if ((word & 0x08c08000u) == 0x08000000u) return A64_DEC_ASIMDTBL;
                                                }
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_EOR;
                                } else {
                                    if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LOG_SHIFT_AND;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (word & 0x01000000u) {
                if (word & 0x00400000u) {
                    if (word & 0x00800000u) {
                        if (word & 0x20000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c001000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c001800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c000800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c001000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c001800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c000800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00200000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c001000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c001800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c000800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c001000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c001800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                                if ((word & 0x8c000800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x20000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    }
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00200000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00200000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x8c1f1000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00000400u) {
                                                if ((word & 0x8c1f1800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            } else {
                                                if ((word & 0x8c1f0800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                            }
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00800000u) {
                        if (word & 0x20000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                    }
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x48000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                } else {
                                    if ((word & 0x48000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_PRE;
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f5800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c005800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f5800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c005800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f4800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c004800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f4800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c004800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f5800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c005800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f5800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c005800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f4800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c004800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f4800u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c004800u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000400u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c1f0000u) return A64_DEC_ASISDLSOP;
                                            if ((word & 0x8c000000u) == 0x0c000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSOP;
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x20000000u) {
                            if (word & 0x04000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                    }
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if ((word & 0x48000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                } else {
                                    if ((word & 0x48000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_OFF;
                                }
                            }
                        } else {
                            if (word & 0x00002000u) {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f5800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f5800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f4800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f4800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x00008000u) {
                                    if (word & 0x00000400u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f5800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f5800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f4800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f4800u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0400u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        } else {
                                            if ((word & 0x8c1f0000u) == 0x0c000000u) return A64_DEC_ASISDLSO;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                if (word & 0x00400000u) {
                    if (word & 0x00800000u) {
                        if (word & 0x04000000u) {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x883f8000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88208000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f8000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88208000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x883f9000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                        if ((word & 0x88209000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x40000000u) {
                                if (word & 0x80000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x80000000u) {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x04000000u) {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x883f8000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f8000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x883f9000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x80000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x281f0000u) == 0x081f0000u) return A64_DEC_LDST_EXCLP;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x281f0000u) == 0x081f0000u) return A64_DEC_LDST_EXCLP;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x081f0000u) == 0x081f0000u) return A64_DEC_LDST_EXCLP;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x081f0000u) == 0x081f0000u) return A64_DEC_LDST_EXCLP;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (word & 0x00800000u) {
                        if (word & 0x04000000u) {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x883f8000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88208000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f8000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88208000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x883f9000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                        if ((word & 0x88209000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x081f0000u) return A64_DEC_ASISDLSEP;
                                            if ((word & 0x88201000u) == 0x08000000u && (((word >> 16) & 31u) != 0x1fu)) return A64_DEC_ASISDLSEP;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x80000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                        } else {
                                            if ((word & 0x281f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_PAIR_POST;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LSE_CAS;
                                            } else {
                                                if ((word & 0x081f7c00u) == 0x081f7c00u) return A64_DEC_LDST_ORD;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        if (word & 0x04000000u) {
                            if (word & 0x20000000u) {
                                if (word & 0x40000000u) {
                                    if ((word & 0x88000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                } else {
                                    if (word & 0x80000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    } else {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    }
                                }
                            } else {
                                if (word & 0x00002000u) {
                                    if (word & 0x00004000u) {
                                        if (word & 0x00001000u) {
                                            if ((word & 0x883f8000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f8000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    }
                                } else {
                                    if (word & 0x00004000u) {
                                        if ((word & 0x883f9000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        } else {
                                            if ((word & 0x883f1000u) == 0x08000000u) return A64_DEC_ASISDLSE;
                                        }
                                    }
                                }
                            }
                        } else {
                            if (word & 0x80000000u) {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28000000u) == 0x08000000u) return A64_DEC_LDST_EXCLP;
                                        } else {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28000000u) == 0x08000000u) return A64_DEC_LDST_EXCLP;
                                        } else {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    }
                                } else {
                                    if (word & 0x20000000u) {
                                        if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_NAPAIR;
                                    } else {
                                        if (word & 0x00008000u) {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_EXCLP;
                                            } else {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        } else {
                                            if (word & 0x00200000u) {
                                                if ((word & 0x08000000u) == 0x08000000u) return A64_DEC_LDST_EXCLP;
                                            } else {
                                                if ((word & 0x08007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                            }
                                        }
                                    }
                                }
                            } else {
                                if (word & 0x40000000u) {
                                    if (word & 0x00008000u) {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                        } else {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    } else {
                                        if (word & 0x00200000u) {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LSE_CASP;
                                        } else {
                                            if ((word & 0x28007c00u) == 0x08007c00u) return A64_DEC_LDST_EXCLR;
                                        }
                                    }
                                } else {
                                    if (word & 0x08000000u) {
                                        if (word & 0x20000000u) {
                                            return A64_DEC_LDST_NAPAIR;
                                        } else {
                                            if (word & 0x00008000u) {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x00007c00u) == 0x00007c00u) return A64_DEC_LSE_CASP;
                                                } else {
                                                    if ((word & 0x00007c00u) == 0x00007c00u) return A64_DEC_LDST_EXCLR;
                                                }
                                            } else {
                                                if (word & 0x00200000u) {
                                                    if ((word & 0x00007c00u) == 0x00007c00u) return A64_DEC_LSE_CASP;
                                                } else {
                                                    if ((word & 0x00007c00u) == 0x00007c00u) return A64_DEC_LDST_EXCLR;
                                                }
                                            }
                                        }
                                    } else {
                                        if ((word & 0x203f0000u) == 0x00000000u) return A64_DEC_PERM_UNDEF_UDF;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return A64_DEC_UNDEF;
}
