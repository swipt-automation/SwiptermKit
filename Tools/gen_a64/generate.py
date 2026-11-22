#!/usr/bin/env python3
import argparse
import collections
import json
import os
import pathlib
import re
import sys
from dataclasses import dataclass, field

import passthrough

PROFILE_FEATURES = {
    "FEAT_FP",
    "FEAT_AdvSIMD",
    "FEAT_FP16",
    "FEAT_LSE",
    "FEAT_CRC32",
    "FEAT_AES",
    "FEAT_PMULL",
    "FEAT_SHA1",
    "FEAT_SHA256",
    "FEAT_RDM",
    "FEAT_LOR",
    "FEAT_RAS",
    "FEAT_JSCVT",
    "FEAT_DotProd",
    "FEAT_LRCPC",
    "FEAT_FCMA",
    "FEAT_FHM",
    "FEAT_SHA3",
    "FEAT_SHA512",
    "FEAT_MOPS",
}

FAMILY_OPCODE_SUBGROUPS = {
    "ldst/asisdlse": "A64_DEC_ASISDLSE",
    "ldst/asisdlsep": "A64_DEC_ASISDLSEP",
    "ldst/asisdlso": "A64_DEC_ASISDLSO",
    "ldst/asisdlsop": "A64_DEC_ASISDLSOP",
    "ldst/comswap": "A64_DEC_LSE_CAS",
    "ldst/comswappr": "A64_DEC_LSE_CASP",
    "ldst/ldst_immpost": "A64_DEC_LDST_IMMPOST",
    "ldst/ldst_immpre": "A64_DEC_LDST_IMMPRE",
    "ldst/ldst_pos": "A64_DEC_LDST_POS",
    "ldst/ldst_regoff": "A64_DEC_LDST_REGOFF",
    "ldst/ldst_unpriv": "A64_DEC_LDST_UNPRIV",
    "ldst/ldst_unscaled": "A64_DEC_LDST_UNSCALED",
    "ldst/ldstexclp": "A64_DEC_LDST_EXCLP",
    "ldst/ldstexclr": "A64_DEC_LDST_EXCLR",
    "ldst/ldstnapair_offs": "A64_DEC_LDST_NAPAIR",
    "ldst/ldstord": "A64_DEC_LDST_ORD",
    "ldst/ldstpair_off": "A64_DEC_LDST_PAIR_OFF",
    "ldst/ldstpair_post": "A64_DEC_LDST_PAIR_POST",
    "ldst/ldstpair_pre": "A64_DEC_LDST_PAIR_PRE",
    "ldst/loadlit": "A64_DEC_LOADLIT",
    "ldst/memop": "A64_DEC_LSE_MEMOP",
    "simd_dp/asimdall": "A64_DEC_ASIMDALL",
    "simd_dp/asimddiff": "A64_DEC_ASIMDDIFF",
    "simd_dp/asimdelem": "A64_DEC_ASIMDELEM",
    "simd_dp/asimdext": "A64_DEC_ASIMDEXT",
    "simd_dp/asimdimm": "A64_DEC_ASIMDIMM",
    "simd_dp/asimdins": "A64_DEC_ASIMDINS",
    "simd_dp/asimdmisc": "A64_DEC_ASIMDMISC",
    "simd_dp/asimdmiscfp16": "A64_DEC_ASIMDMISCFP16",
    "simd_dp/asimdperm": "A64_DEC_ASIMDPERM",
    "simd_dp/asimdsame": "A64_DEC_ASIMDSAME",
    "simd_dp/asimdsame2": "A64_DEC_ASIMDSAME2",
    "simd_dp/asimdsamefp16": "A64_DEC_ASIMDSAMEFP16",
    "simd_dp/asimdshf": "A64_DEC_ASIMDSHF",
    "simd_dp/asimdtbl": "A64_DEC_ASIMDTBL",
    "simd_dp/asisddiff": "A64_DEC_ASISDDIFF",
    "simd_dp/asisdelem": "A64_DEC_ASISDELEM",
    "simd_dp/asisdmisc": "A64_DEC_ASISDMISC",
    "simd_dp/asisdmiscfp16": "A64_DEC_ASISDMISCFP16",
    "simd_dp/asisdone": "A64_DEC_ASISDONE",
    "simd_dp/asisdpair": "A64_DEC_ASISDPAIR",
    "simd_dp/asisdsame": "A64_DEC_ASISDSAME",
    "simd_dp/asisdsame2": "A64_DEC_ASISDSAME2",
    "simd_dp/asisdsamefp16": "A64_DEC_ASISDSAMEFP16",
    "simd_dp/asisdshf": "A64_DEC_ASISDSHF",
    "simd_dp/cryptoaes": "A64_DEC_CRYPTOAES",
    "simd_dp/cryptosha2": "A64_DEC_CRYPTOSHA2",
    "simd_dp/cryptosha3": "A64_DEC_CRYPTOSHA3",
    "simd_dp/crypto3_imm6": "A64_DEC_CRYPTO3_IMM6",
    "simd_dp/crypto4": "A64_DEC_CRYPTO4",
    "simd_dp/cryptosha512_2": "A64_DEC_CRYPTOSHA512_2",
    "simd_dp/cryptosha512_3": "A64_DEC_CRYPTOSHA512_3",
    "simd_dp/float2fix": "A64_DEC_FLOAT2FIX",
    "simd_dp/float2int": "A64_DEC_FLOAT2INT",
    "simd_dp/floatccmp": "A64_DEC_FLOATCCMP",
    "simd_dp/floatcmp": "A64_DEC_FLOATCMP",
    "simd_dp/floatdp1": "A64_DEC_FLOATDP1",
    "simd_dp/floatdp2": "A64_DEC_FLOATDP2",
    "simd_dp/floatdp3": "A64_DEC_FLOATDP3",
    "simd_dp/floatimm": "A64_DEC_FLOATIMM",
    "simd_dp/floatsel": "A64_DEC_FLOATSEL",
}

PER_SUBGROUP_MNEMONIC = {
    "control/barriers",
    "control/branch_imm",
    "control/branch_reg",
    "control/compbranch",
    "control/condbranch",
    "control/exception",
    "control/hints",
    "control/pstate",
    "control/systeminstrs",
    "control/systemmove",
    "control/testbranch",
    "dpimm/addsub_imm",
    "dpimm/bitfield",
    "dpimm/extract",
    "dpimm/log_imm",
    "dpimm/movewide",
    "dpimm/pcreladdr",
    "dpreg/addsub_carry",
    "dpreg/addsub_ext",
    "dpreg/addsub_shift",
    "dpreg/condcmp_imm",
    "dpreg/condcmp_reg",
    "dpreg/condsel",
    "dpreg/dp_1src",
    "dpreg/dp_2src",
    "dpreg/dp_3src",
    "dpreg/log_shift",
    "reserved/perm_undef",
}

SYSREGS_WANTED = [
    "MIDR_EL1", "MPIDR_EL1", "REVIDR_EL1", "AIDR_EL1",
    "ID_PFR0_EL1", "ID_PFR1_EL1", "ID_DFR0_EL1", "ID_AFR0_EL1",
    "ID_MMFR0_EL1", "ID_MMFR1_EL1", "ID_MMFR2_EL1", "ID_MMFR3_EL1",
    "ID_ISAR0_EL1", "ID_ISAR1_EL1", "ID_ISAR2_EL1", "ID_ISAR3_EL1",
    "ID_ISAR4_EL1", "ID_ISAR5_EL1",
    "MVFR0_EL1", "MVFR1_EL1", "MVFR2_EL1",
    "ID_AA64PFR0_EL1", "ID_AA64PFR1_EL1", "ID_AA64DFR0_EL1", "ID_AA64DFR1_EL1",
    "ID_AA64AFR0_EL1", "ID_AA64AFR1_EL1", "ID_AA64ISAR0_EL1", "ID_AA64ISAR1_EL1",
    "ID_AA64ISAR2_EL1",
    "ID_AA64MMFR0_EL1", "ID_AA64MMFR1_EL1", "ID_AA64MMFR2_EL1",
    "SCTLR_EL1", "ACTLR_EL1", "CPACR_EL1", "TTBR0_EL1", "TTBR1_EL1", "TCR_EL1",
    "AFSR0_EL1", "AFSR1_EL1", "ESR_EL1", "FAR_EL1", "PAR_EL1", "MAIR_EL1",
    "AMAIR_EL1", "VBAR_EL1", "CONTEXTIDR_EL1",
    "TPIDR_EL0", "TPIDRRO_EL0", "TPIDR_EL1",
    "CNTKCTL_EL1", "CNTFRQ_EL0", "CNTPCT_EL0", "CNTVCT_EL0",
    "CNTP_TVAL_EL0", "CNTP_CTL_EL0", "CNTP_CVAL_EL0",
    "CNTV_TVAL_EL0", "CNTV_CTL_EL0", "CNTV_CVAL_EL0",
    "SPSR_EL1", "ELR_EL1", "SP_EL0", "SPSel", "CurrentEL", "PAN", "UAO",
    "DAIF", "NZCV", "FPCR", "FPSR",
    "CTR_EL0", "DCZID_EL0", "CLIDR_EL1", "CCSIDR_EL1", "CSSELR_EL1",
    "MDSCR_EL1", "MDCCSR_EL0", "OSDLR_EL1", "OSLAR_EL1", "OSLSR_EL1",
    "PMCR_EL0", "PMCNTENSET_EL0", "PMCNTENCLR_EL0", "PMOVSCLR_EL0", "PMSELR_EL0",
    "PMCEID0_EL0", "PMCEID1_EL0", "PMCCNTR_EL0", "PMXEVTYPER_EL0", "PMXEVCNTR_EL0",
    "PMUSERENR_EL0", "PMINTENSET_EL1", "PMINTENCLR_EL1", "PMOVSSET_EL0",
    "DBGBVR<n>_EL1", "DBGBCR<n>_EL1", "DBGWVR<n>_EL1", "DBGWCR<n>_EL1",
]

SYSOPS_WANTED = [
    "DC ZVA", "DC IVAC", "DC ISW", "DC CSW", "DC CISW", "DC CVAC", "DC CVAU",
    "DC CIVAC", "DC CVAP",
    "IC IALLUIS", "IC IALLU", "IC IVAU",
    "AT S1E1R", "AT S1E1W", "AT S1E0R", "AT S1E0W", "AT S1E1RP", "AT S1E1WP",
    "TLBI VMALLE1", "TLBI VMALLE1IS", "TLBI VMALLE1OS",
    "TLBI VAE1", "TLBI VAE1IS", "TLBI VAE1OS",
    "TLBI ASIDE1", "TLBI ASIDE1IS", "TLBI ASIDE1OS",
    "TLBI VAAE1", "TLBI VAAE1IS", "TLBI VAAE1OS",
    "TLBI VALE1", "TLBI VALE1IS", "TLBI VALE1OS",
    "TLBI VAALE1", "TLBI VAALE1IS", "TLBI VAALE1OS",
    "TLBI RVAE1", "TLBI RVAE1IS", "TLBI RVAE1OS",
    "TLBI RVAAE1", "TLBI RVAAE1IS", "TLBI RVAAE1OS",
    "TLBI RVALE1", "TLBI RVALE1IS", "TLBI RVALE1OS",
    "TLBI RVAALE1", "TLBI RVAALE1IS", "TLBI RVAALE1OS",
]


ALL_FEATURES = False


def bit_pattern(val_obj):
    pattern = val_obj["value"].strip("'")
    if not re.fullmatch("[01x]+", pattern):
        raise ValueError(f"Unsupported bit pattern {pattern}")
    return (
        int(pattern.replace("0", "1").replace("x", "0"), 2),
        int(pattern.replace("x", "0"), 2),
    )


def eval_feature_ast(cond):
    if cond is None:
        return True
    t = cond["_type"]
    if t == "AST.Bool":
        return cond["value"]
    if t == "AST.Function":
        if cond["name"] == "IsFeatureImplemented":
            return ALL_FEATURES or cond["arguments"][0]["value"] in PROFILE_FEATURES
        return None
    if t == "AST.BinaryOp":
        op = cond["op"]
        if op == "&&":
            l = eval_feature_ast(cond["left"])
            r = eval_feature_ast(cond["right"])
            if l is False or r is False:
                return False
            if l is True and r is True:
                return True
            return None
        if op == "||":
            l = eval_feature_ast(cond["left"])
            r = eval_feature_ast(cond["right"])
            if l is True or r is True:
                return True
            if l is False and r is False:
                return False
            return None
        return None
    if t == "AST.UnaryOp" and cond["op"] == "!":
        sub = eval_feature_ast(cond["expr"])
        if sub is None:
            return None
        return not sub
    return None


def compile_guard_ast(cond, fields):
    if cond is None:
        return "true"
    st = eval_feature_ast(cond)
    if st is True:
        return "true"
    if st is False:
        return "false"
    t = cond["_type"]
    if t == "AST.UnaryOp" and cond["op"] == "!":
        sub = compile_guard_ast(cond["expr"], fields)
        return f"!({sub})"
    if t == "AST.BinaryOp":
        op = cond["op"]
        if op in ("&&", "||"):
            l = compile_guard_ast(cond["left"], fields)
            r = compile_guard_ast(cond["right"], fields)
            if op == "&&":
                if l == "true":
                    return r
                if r == "true":
                    return l
                if l == "false" or r == "false":
                    return "false"
            else:
                if l == "false":
                    return r
                if r == "false":
                    return l
                if l == "true" or r == "true":
                    return "true"
            return f"(({l}) {op} ({r}))"
        if op in ("==", "!="):
            ident = cond["left"]["value"]
            start, width = fields[ident]
            expr = f"((word >> {start}) & {(1 << width) - 1}u)"
            mask, val = bit_pattern(cond["right"])
            if mask == (1 << width) - 1:
                return f"({expr} {op} 0x{val:x}u)"
            if op == "==":
                return f"(({expr} & 0x{mask:x}u) == 0x{val:x}u)"
            return f"(({expr} & 0x{mask:x}u) != 0x{val:x}u)"
        if op == "IN":
            ident = cond["left"]["value"]
            start, width = fields[ident]
            expr = f"((word >> {start}) & {(1 << width) - 1}u)"
            parts = []
            for item in cond["right"]["values"]:
                mask, val = bit_pattern(item)
                if mask == (1 << width) - 1:
                    parts.append(f"({expr} == 0x{val:x}u)")
                else:
                    parts.append(f"(({expr} & 0x{mask:x}u) == 0x{val:x}u)")
            return "(" + " || ".join(parts) + ")"
    raise ValueError(f"Unsupported AST condition: {cond}")


def feature_names(cond):
    if cond is None:
        return set()
    t = cond["_type"]
    if t == "AST.Function" and cond["name"] == "IsFeatureImplemented":
        return {cond["arguments"][0]["value"]}
    if t == "AST.BinaryOp":
        return feature_names(cond["left"]) | feature_names(cond["right"])
    if t == "AST.UnaryOp":
        return feature_names(cond["expr"])
    return set()


def eval_guard_ast(cond, fields, word):
    if cond is None:
        return True
    st = eval_feature_ast(cond)
    if st is not None:
        return st
    t = cond["_type"]
    if t == "AST.UnaryOp" and cond["op"] == "!":
        return not eval_guard_ast(cond["expr"], fields, word)
    if t == "AST.BinaryOp":
        op = cond["op"]
        if op == "&&":
            return eval_guard_ast(cond["left"], fields, word) and eval_guard_ast(cond["right"], fields, word)
        if op == "||":
            return eval_guard_ast(cond["left"], fields, word) or eval_guard_ast(cond["right"], fields, word)
        start, width = fields[cond["left"]["value"]]
        bits = (word >> start) & ((1 << width) - 1)
        if op in ("==", "!="):
            mask, val = bit_pattern(cond["right"])
            return ((bits & mask) == val) == (op == "==")
        if op == "IN":
            return any((bits & mask) == val for mask, val in map(bit_pattern, cond["right"]["values"]))
    raise ValueError(f"Unsupported AST condition: {cond}")


@dataclass
class DecEntry:
    name: str
    subgroup: str
    opcode: str
    mask: int
    value: int
    guard: str
    priority: int = 0
    fields: dict = field(default_factory=dict)
    conds: list = field(default_factory=list)
    features: frozenset = frozenset()
    scope: dict = field(default_factory=dict)

    def matches(self, word):
        if (word & self.mask) != self.value:
            return False
        if self.subgroup == "ldst/memcms" and (word >> 30) != 0:
            return False
        return all(eval_guard_ast(c, self.scope, word) for c in self.conds)

    def sort_key(self):
        return (self.priority, -bin(self.mask).count("1"), self.guard == "true", self.name)

    def predicate(self, tested_bits):
        rem_mask = self.mask & ~tested_bits
        checks = []
        if rem_mask:
            checks.append(f"(word & 0x{rem_mask:08x}u) == 0x{self.value & rem_mask:08x}u")
        if self.guard != "true":
            checks.append(self.guard)
        return " && ".join(checks) if checks else "true"


def map_opcode(subgroup, name):
    if ALL_FEATURES:
        return "A64_DEC_UNDEF"
    if subgroup in FAMILY_OPCODE_SUBGROUPS:
        return FAMILY_OPCODE_SUBGROUPS[subgroup]
    if subgroup == "ldst/memcms":
        return "A64_DEC_MOPS_SET" if name.startswith("SET") else "A64_DEC_MOPS_CPY"
    if subgroup in PER_SUBGROUP_MNEMONIC:
        mnem = name.split("_", 1)[0]
        if subgroup == "control/hints" and mnem not in ("WFI", "WFE", "SEV", "SEVL"):
            return "A64_DEC_HINT_NOP"
        if subgroup == "control/condbranch":
            return "A64_DEC_B_COND"
        sub_tag = subgroup.split("/", 1)[1].upper()
        return f"A64_DEC_{sub_tag}_{mnem}"
    raise ValueError(f"Unmapped subgroup {subgroup} ({name})")


def collect_instructions(a64_root):
    entries = []

    def walk(node, parents=()):
        if eval_feature_ast(node.get("condition")) is False:
            return
        if node["_type"] == "Instruction.Instruction":
            subgroup = "/".join(p["name"] for p in parents[1:])
            mask = value = 0
            covered = 0
            fields = {}
            for f in node["encoding"]["values"]:
                start, width = f["range"]["start"], f["range"]["width"]
                pos = ((1 << width) - 1) << start
                assert not (covered & pos), node["name"]
                covered |= pos
                fm, fv = bit_pattern(f["value"])
                mask |= fm << start
                value |= fv << start
                if "name" in f:
                    fields[f["name"]] = (start, width)
            assert covered == 0xFFFFFFFF, node["name"]
            scope = dict(fields)
            for parent in parents:
                if not parent.get("encoding"):
                    continue
                for f in parent["encoding"]["values"]:
                    start = f["range"]["start"]
                    pm, pv = bit_pattern(f["value"])
                    pm <<= start
                    pv <<= start
                    assert (pm & mask) == pm and (value & pm) == pv, node["name"]
                    if "name" in f:
                        scope.setdefault(f["name"], (start, f["range"]["width"]))

            guards = []
            conds = []
            features = set()
            for p in (*parents, node):
                g = compile_guard_ast(p.get("condition"), scope)
                if g == "false":
                    return
                features |= feature_names(p.get("condition"))
                if g != "true":
                    guards.append(g)
                    conds.append(p["condition"])
            if subgroup == "ldst/memcms":
                guards.append("((word >> 30) & 3u) == 0x0u")
            guard = " && ".join(guards) if guards else "true"
            opcode = map_opcode(subgroup, node["name"])
            prio = 1 if node["name"] == "HINT_HM_hints" else 0
            entries.append(DecEntry(node["name"], subgroup, opcode, mask, value, guard, prio, fields,
                                    conds, frozenset(features), scope))
            return
        for ch in node.get("children") or []:
            walk(ch, (*parents, node))

    walk(a64_root)
    return entries


def emit_tree(entries, lines, indent, tested=0):
    common = (~tested) & 0xFFFFFFFF
    for e in entries:
        common &= e.mask
    best = None
    for bit in range(32):
        if common & (1 << bit):
            ones = sum((e.value >> bit) & 1 for e in entries)
            if 0 < ones < len(entries):
                score = min(ones, len(entries) - ones)
                if best is None or score > best[0]:
                    best = (score, bit)
    if best is None:
        ordered = sorted(entries, key=DecEntry.sort_key)
        for e in ordered:
            pred = e.predicate(tested)
            if pred == "true":
                lines.append(f"{indent}return {e.opcode};")
                return
            lines.append(f"{indent}if ({pred}) return {e.opcode};")
        return

    _, bit = best
    bmask = 1 << bit
    hi = [e for e in entries if e.value & bmask]
    lo = [e for e in entries if not (e.value & bmask)]
    lines.append(f"{indent}if (word & 0x{bmask:08x}u) {{")
    emit_tree(hi, lines, indent + "    ", tested | bmask)
    lines.append(f"{indent}}} else {{")
    emit_tree(lo, lines, indent + "    ", tested | bmask)
    lines.append(f"{indent}}}")


def load_instructions(aarchmrs_dir: pathlib.Path):
    global ALL_FEATURES
    doc = json.loads((aarchmrs_dir / "Instructions.json").read_bytes())
    [a64] = [n for n in doc["instructions"] if n["name"] == "A64"]
    profile = collect_instructions(a64)
    ALL_FEATURES = True
    full = collect_instructions(a64)
    ALL_FEATURES = False
    return profile, full


def generate_leaves_inc(entries, out_path: pathlib.Path):
    features = sorted({f for e in entries for f in e.features})
    lines = ["enum : uint32_t {"]
    for i, f in enumerate(features):
        lines.append(f"    A64_LEAF_{f} = 1u << {i},")
    lines.append("};")
    lines.append("")
    lines.append("static const char *const a64_leaf_feature_names[] = {")
    for f in features:
        lines.append(f'    "{f}",')
    lines.append("};")
    lines.append("")
    ordered = sorted(entries, key=DecEntry.sort_key)
    for i, e in enumerate(ordered):
        if e.guard != "true":
            lines.append(f"static bool a64_leaf_guard_{i}(uint32_t word) {{ return {e.guard}; }}")
    lines.append("")
    lines.append("static const a64_leaf_t a64_leaves[] = {")
    for i, e in enumerate(ordered):
        guard = f"a64_leaf_guard_{i}" if e.guard != "true" else "nullptr"
        feats = " | ".join(f"A64_LEAF_{f}" for f in sorted(e.features)) or "0"
        flds = ", ".join(f'{{"{n}", {s}, {w}}}' for n, (s, w) in e.fields.items())
        lines.append(f'    {{"{e.name}", "{e.subgroup}", 0x{e.mask:08x}u, 0x{e.value:08x}u, {guard}, {feats}, '
                     f"{len(e.fields)}, {{{flds}}}}},")
    lines.append("};")
    lines.append("")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")


def generate_decode_inc(entries, out_path: pathlib.Path):
    by_prio = collections.defaultdict(list)
    for e in entries:
        by_prio[e.priority].append(e)
    for prio, group in by_prio.items():
        for i in range(len(group)):
            a = group[i]
            if a.guard != "true":
                continue
            for j in range(i + 1, len(group)):
                b = group[j]
                if b.guard != "true":
                    continue
                if ((a.value ^ b.value) & a.mask & b.mask) == 0:
                    raise ValueError(f"Overlapping unconditional encodings: {a.name} vs {b.name}")

    opcodes = sorted({e.opcode for e in entries})
    lines = [
        "typedef enum a64_dec_op : uint16_t {",
        "    A64_DEC_UNDEF = 0,",
    ]
    for op in opcodes:
        lines.append(f"    {op},")
    lines.append("    A64_DEC_COUNT,")
    lines.append("} a64_dec_op_t;")
    lines.append("")
    lines.append("static inline a64_dec_op_t a64_decode_word(uint32_t word) {")
    emit_tree(entries, lines, "    ")
    lines.append("    return A64_DEC_UNDEF;")
    lines.append("}")
    lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return len(entries), len(opcodes)


def encode_sysreg_key(enc):
    def get_bits(k):
        v = enc[k]
        if v["_type"] != "Values.Value":
            return None
        return int(v["value"].strip("'"), 2)

    op0 = get_bits("op0")
    op1 = get_bits("op1")
    crn = get_bits("CRn")
    crm = get_bits("CRm")
    op2 = get_bits("op2")
    if None in (op0, op1, crn, crm, op2):
        return None
    return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2


def generate_sysreg_inc(aarchmrs_dir: pathlib.Path, out_path: pathlib.Path):
    regs = json.loads((aarchmrs_dir / "Registers.json").read_bytes())
    by_name = {r["name"]: r for r in regs if r.get("state") == "AArch64"}

    sysreg_defs = []
    seen_sysregs = set()
    for name in SYSREGS_WANTED:
        if name not in by_name:
            raise KeyError(f"Missing AARCHMRS register: {name}")
        r = by_name[name]
        enc_map = None
        for acc in r.get("accessors") or []:
            if acc["name"] in ("A64.MRS", "A64.MSRregister"):
                encs = acc.get("encoding") or []
                if encs:
                    enc_map = encs[0]["encodings"]
                    break
        if enc_map is None:
            raise ValueError(f"No MRS/MSR encoding for {name}")
        if "<n>" in name:
            op0 = int(enc_map["op0"]["value"].strip("'"), 2)
            op1 = int(enc_map["op1"]["value"].strip("'"), 2)
            crn = int(enc_map["CRn"]["value"].strip("'"), 2)
            op2 = int(enc_map["op2"]["value"].strip("'"), 2)
            for idx in range(16):
                cname = name.replace("<n>", str(idx)).upper()
                key = (op0 << 14) | (op1 << 11) | (crn << 7) | (idx << 3) | op2
                if cname not in seen_sysregs:
                    seen_sysregs.add(cname)
                    sysreg_defs.append((cname, key))
        else:
            key = encode_sysreg_key(enc_map)
            if key is None:
                raise ValueError(f"Non-constant encoding for {name}: {enc_map}")
            cname = name.upper()
            if cname not in seen_sysregs:
                seen_sysregs.add(cname)
                sysreg_defs.append((cname, key))

    sysop_defs = []
    seen_sysops = set()
    for name in SYSOPS_WANTED:
        if name not in by_name:
            raise KeyError(f"Missing AARCHMRS SYS op: {name}")
        r = by_name[name]
        prefix, opname = name.split(" ", 1)
        acc_name = f"A64.{prefix}"
        enc_map = None
        for acc in r.get("accessors") or []:
            if acc["name"] == acc_name:
                for entry in acc.get("encoding") or []:
                    if entry.get("asmvalue") == opname:
                        enc_map = entry["encodings"]
                        break
            if enc_map is not None:
                break
        if enc_map is None:
            raise ValueError(f"No {acc_name} encoding for {name}")
        op1 = int(enc_map["op1"]["value"].strip("'"), 2)
        crn = int(enc_map["CRn"]["value"].strip("'"), 2)
        crm = int(enc_map["CRm"]["value"].strip("'"), 2)
        op2 = int(enc_map["op2"]["value"].strip("'"), 2)
        key = (op1 << 11) | (crn << 7) | (crm << 3) | op2
        cname = f"{prefix}_{opname}".upper()
        if cname not in seen_sysops:
            seen_sysops.add(cname)
            sysop_defs.append((cname, key))

    lines = [
        "#define A64_SYSREG_KEY(op0, op1, crn, crm, op2) \\",
        "    ((uint16_t)(((uint32_t)(op0) << 14) | ((uint32_t)(op1) << 11) | \\",
        "                ((uint32_t)(crn) << 7) | ((uint32_t)(crm) << 3) | (uint32_t)(op2)))",
        "#define A64_SYSOP_KEY(op1, crn, crm, op2) \\",
        "    ((uint16_t)(((uint32_t)(op1) << 11) | ((uint32_t)(crn) << 7) | \\",
        "                ((uint32_t)(crm) << 3) | (uint32_t)(op2)))",
        "",
        "enum : uint16_t {",
    ]
    for cname, key in sysreg_defs:
        lines.append(f"    A64_SYSREG_{cname} = 0x{key:04x}u,")
    lines.append("};")
    lines.append("")
    lines.append("enum : uint16_t {")
    for cname, key in sysop_defs:
        lines.append(f"    A64_SYSOP_{cname} = 0x{key:04x}u,")
    lines.append("};")
    lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return len(sysreg_defs), len(sysop_defs)


def main():
    parser = argparse.ArgumentParser(description="Generate A64 decode and sysreg tables from AARCHMRS")
    parser.add_argument(
        "aarchmrs",
        nargs="?",
        default=os.environ.get("AARCHMRS_DIR"),
        help="Path to AARCHMRS directory containing Instructions.json and Registers.json",
    )
    parser.add_argument(
        "--out-dir",
        default=str(pathlib.Path(__file__).resolve().parents[2] / "VMM/cpu/generated"),
        help="Output directory for generated .inc files",
    )
    parser.add_argument(
        "--tool-dir",
        default=str(pathlib.Path(__file__).resolve().parents[1] / "a64_check/generated"),
        help="Output directory for the tables used by Tools/a64_check",
    )
    args = parser.parse_args()
    if not args.aarchmrs:
        parser.error("aarchmrs path argument or AARCHMRS_DIR environment variable is required")
    aarchmrs_dir = pathlib.Path(args.aarchmrs)
    out_dir = pathlib.Path(args.out_dir)

    entries, full = load_instructions(aarchmrs_dir)
    n_insns, n_ops = generate_decode_inc(entries, out_dir / "a64_decode.inc")
    tool_dir = pathlib.Path(args.tool_dir)
    generate_leaves_inc(entries, tool_dir / "a64_leaves.inc")
    pt = passthrough.Passthrough(entries, full)
    (out_dir / "a64_passthrough.inc").write_text(pt.emit_table(emit_tree), encoding="utf-8")
    (out_dir / "a64_passthrough_code.inc").write_text(pt.emit_code(), encoding="utf-8")
    (out_dir / "a64_passthrough_offsets.inc").write_text(pt.emit_offsets(), encoding="utf-8")
    (tool_dir / "a64_passthrough_words.inc").write_text(pt.emit_words(), encoding="utf-8")
    for name in pt.uncovered:
        print(f"no passthrough encoding for {name}", file=sys.stderr)
    n_sysregs, n_sysops = generate_sysreg_inc(aarchmrs_dir, out_dir / "a64_sysreg_gen.inc")
    print(f"Generated {n_insns} AARCHMRS encodings -> {n_ops} decode ops, {n_sysregs} sysregs, {n_sysops} sysops, "
          f"{sum(1 for i, s in enumerate(pt.stubs) if s and s[0] == i)} passthrough encodings in {len(pt.leaves)} groups")


if __name__ == "__main__":
    main()
