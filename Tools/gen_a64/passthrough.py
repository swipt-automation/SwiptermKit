import collections
import pathlib
import re
import subprocess
import tempfile
from dataclasses import dataclass

REGISTER_FIELDS = ("Rd", "Rn", "Rm", "Ra", "Rt", "Rs")
BASELINE_FEATURES = {"FEAT_FP", "FEAT_AdvSIMD"}
LLVM_ATTRIBUTES = ("+v8.4a,+fullfp16,+aes,+sha2,+sha3,+dotprod,+fp16fml,+rdm,+lse,+crc,+jsconv,+complxnum,+rcpc,"
                   "+bf16,+i8mm,+fptoint")

SIMD_EXCLUDED = {
    "simd_dp/asimdtbl",
}
SIMD_IMMEDIATE_LEAVES = {
    "FMOV_asimdimm_D2_d",
    "FMOV_asimdimm_H_h",
    "FMOV_asimdimm_S_s",
    "MOVI_asimdimm_D2_d",
    "MOVI_asimdimm_D_ds",
}

WIDE_RM = 18
WIDE_RM_BIT = 1 << 20

SLOT_FIELDS = ("rd", "rn", "rm", "ra")
SLOT_OFFSETS = {"rd": "IR_RD", "rn": "IR_RN", "rm": "IR_RM", "ra": "IR_RA"}


@dataclass
class Shape:
    name: str
    host: dict
    slots: dict
    prologue: str
    epilogue: str
    opcode_bits: int = 0


def simd_shape(leaf):
    if not leaf.subgroup.startswith("simd_dp/") or leaf.subgroup in SIMD_EXCLUDED:
        return None
    if leaf.subgroup == "simd_dp/asimdimm" and leaf.name not in SIMD_IMMEDIATE_LEAVES:
        return None
    return "simd"


def memory_shape(leaf):
    size = 1 << (leaf.value >> 30)
    if leaf.subgroup == "ldst/memop":
        if leaf.name.startswith("LDAPR"):
            return Shape("acquire", {"Rt": 0, "Rn": 12}, {"rd": ("Rt", "XW"), "rn": ("Rn", "XB")},
                         f"acquire{size}", "X")
        return Shape("atomic", {"Rt": 0, "Rs": 1, "Rn": 12},
                     {"rd": ("Rt", "XW"), "rn": ("Rn", "XB"), "rm": ("Rs", "XR")},
                     f"atomic{size}", "atomic")
    if leaf.subgroup == "ldst/comswappr":
        return Shape("casp", {"Rs": 0, "Rt": 2, "Rn": 12},
                     {"rn": ("Rn", "XB"), "rm": ("Rt", "XR"), "ra": ("Rs", "XR")},
                     f"casp{16 if leaf.value >> 30 & 1 else 8}", "casp", (1 << 16) | 1)
    if leaf.subgroup == "ldst/comswap":
        return Shape("cas", {"Rt": 0, "Rs": 1, "Rn": 12},
                     {"rd": ("Rs", "XW"), "rn": ("Rn", "XB"), "rm": ("Rt", "XR"), "ra": ("Rs", "XR")},
                     f"cas{size}", "cas")
    return None


def deposit(bits, mask):
    out = 0
    bit = 0
    while mask:
        low = mask & -mask
        if (bits >> bit) & 1:
            out |= low
        mask ^= low
        bit += 1
    return out


def extract(word, mask):
    out = 0
    bit = 0
    while mask:
        low = mask & -mask
        if word & low:
            out |= 1 << bit
        mask ^= low
        bit += 1
    return out


def register_mask(leaf, registers):
    mask = 0
    for name in registers:
        if name in leaf.fields:
            start, width = leaf.fields[name]
            mask |= ((1 << width) - 1) << start
    return mask


def with_registers(leaf, word, registers):
    for name, value in registers.items():
        if name in leaf.fields:
            start, width = leaf.fields[name]
            mask = ((1 << width) - 1) << start
            word = (word & ~mask) | ((value << start) & mask)
    return word


def disassemble(words):
    with tempfile.TemporaryDirectory() as tmp:
        source = pathlib.Path(tmp) / "words.s"
        obj = pathlib.Path(tmp) / "words.o"
        source.write_text(".text\n" + "".join(f".inst 0x{w:08x}\n" for w in words))
        subprocess.run(["xcrun", "clang", "-target", "arm64-apple-macos14", "-c", str(source), "-o", str(obj)],
                       check=True)
        text = subprocess.run(
            ["xcrun", "llvm-objdump", "-d", "--no-show-raw-insn", "-M", "no-aliases", "--triple=aarch64",
             f"--mattr={LLVM_ATTRIBUTES}", str(obj)],
            check=True, capture_output=True, text=True).stdout
    lines = [m.group(1).strip() for m in re.finditer(r"^\s*[0-9a-f]+:\s+(.*)$", text, re.M)]
    assert len(lines) == len(words), (len(lines), len(words))
    return lines


def simd_kinds(leaf, text):
    operands = text.split(None, 1)[1] if " " in text or "\t" in text else ""
    classes = collections.defaultdict(set)
    for letter, number in re.findall(r"(?<![#.\w])([bhsdqvwx])(\d+)\b", operands):
        classes[int(number)].add("V" if letter in "bhsdqv" else "X")
    wide = WIDE_RM in classes
    if wide:
        classes[2] = classes.pop(WIDE_RM)
    kinds = {}
    for slot, (name, host) in zip(SLOT_FIELDS, (("Rd", 0), ("Rn", 1), ("Rm", 2), ("Ra", 3))):
        if name not in leaf.fields or host not in classes:
            continue
        [kind] = classes.pop(host)
        kinds[slot] = (name, "V" if kind == "V" else "XW" if slot == "rd" else "XR")
    if classes:
        raise ValueError(f"{leaf.name} has unexpected registers: {text}")
    return kinds, wide


class Passthrough:
    def candidates(self, leaf, shape, host):
        reg_mask = register_mask(leaf, host)
        if isinstance(shape, Shape):
            reg_mask &= ~shape.opcode_bits
        var_mask = ~leaf.mask & ~reg_mask & 0xFFFFFFFF
        shadows = [m for m in self.full
                   if m.sort_key() < leaf.sort_key() and ((m.value ^ leaf.value) & m.mask & leaf.mask) == 0]
        for m in shadows:
            if m.mask & ~leaf.mask & reg_mask:
                raise ValueError(f"{leaf.name} is shadowed by {m.name} depending on its registers")
        out = []
        for bits in range(1 << bin(var_mask).count("1")):
            word = with_registers(leaf, leaf.value, host) | deposit(bits, var_mask)
            if leaf.matches(word) and not any(m.matches(word) for m in shadows):
                out.append((leaf, shape, var_mask, bits, word))
        return out

    def disassemble(self, words):
        missing = sorted(set(words) - self.texts.keys())
        if missing:
            self.texts.update(zip(missing, disassemble(missing)))

    def valid(self, found):
        return any(self.texts[c[4]] != "<unknown>" for c in found)

    def __init__(self, profile, full):
        self.full = sorted(full, key=lambda e: e.sort_key())
        self.leaves = []
        self.stubs = []
        self.uncovered = []
        self.texts = {}
        shaped = []
        for leaf in sorted(profile, key=lambda e: e.sort_key()):
            shape = memory_shape(leaf) or simd_shape(leaf)
            if shape is not None:
                host = shape.host if isinstance(shape, Shape) else {"Rd": 0, "Rn": 1, "Rm": 2, "Ra": 3}
                shaped.append((leaf, shape, host, self.candidates(leaf, shape, host)))
        self.disassemble([c[4] for _, _, _, found in shaped for c in found])
        retries = {}
        for leaf, shape, host, found in shaped:
            if not self.valid(found):
                retries[leaf.name] = [self.candidates(leaf, shape, {k: v for k, v in host.items() if k != name})
                                      for name in host if name in leaf.fields]
        self.disassemble([c[4] for group in retries.values() for found in group for c in found])
        candidates = []
        for leaf, shape, host, found in shaped:
            if leaf.name in retries:
                found = next((retry for retry in retries[leaf.name] if self.valid(retry)), None)
                if found is None:
                    self.uncovered.append(leaf.name)
                    continue
            candidates += found
        texts = [self.texts[c[4]] for c in candidates]
        by_leaf = collections.OrderedDict()
        for (leaf, shape, var_mask, bits, word), text in zip(candidates, texts):
            if text == "<unknown>":
                continue
            wide = False
            if isinstance(shape, Shape):
                slots, prologue, epilogue = shape.slots, shape.prologue, shape.epilogue
            else:
                slots, wide = simd_kinds(leaf, text)
                prologue = "simd_" + "".join(slots[s][1][0] if s in slots and slots[s][1] != "XW" else "_"
                                             for s in SLOT_FIELDS)
                epilogue = {"V": "V", "XW": "X"}[slots["rd"][1]] if "rd" in slots else "none"
            entry = by_leaf.setdefault(leaf.name, {"leaf": leaf, "var_mask": var_mask, "slots": slots,
                                                   "stubs": {}, "wide": set()})
            if entry["slots"] != slots:
                raise ValueError(f"{leaf.name} has encodings with different operands: {text}")
            if wide:
                if leaf.fields["Rm"] != (16, 4) or not var_mask & WIDE_RM_BIT:
                    raise ValueError(f"{leaf.name} extends Rm unexpectedly: {text}")
                entry["wide"].add(word & ~WIDE_RM_BIT)
            else:
                entry["stubs"][bits] = (word, prologue, epilogue, text)
        for entry in by_leaf.values():
            entry["base"] = len(self.stubs)
            var_mask = entry["var_mask"]
            for bits in range(1 << bin(var_mask).count("1")):
                stub = entry["stubs"].get(bits)
                if stub and stub[0] in entry["wide"]:
                    self.stubs.append((len(self.stubs), True, *stub))
                elif stub:
                    self.stubs.append((len(self.stubs), False, *stub))
                else:
                    word = entry["leaf"].value | deposit(bits, var_mask)
                    narrow = extract(word & ~WIDE_RM_BIT, var_mask)
                    target = entry["stubs"].get(narrow) if word & WIDE_RM_BIT else None
                    if target and target[0] in entry["wide"]:
                        self.stubs.append((entry["base"] + narrow, True, *target))
                    else:
                        self.stubs.append(None)
            self.leaves.append(entry)

    def emit_table(self, emit_tree):
        lines = [
            f"enum : uint32_t {{ CPU_PT_STUB_COUNT = {len(self.stubs)} }};",
            "",
            "static const cpu_pt_leaf_t cpu_pt_leaves[] = {",
        ]
        entries = []
        for index, entry in enumerate(self.leaves):
            leaf = entry["leaf"]
            features = " | ".join(f"CPU_HOST_{f}" for f in sorted(leaf.features - BASELINE_FEATURES)) or "0"
            slots = []
            for slot in SLOT_FIELDS:
                if slot in entry["slots"]:
                    name, kind = entry["slots"][slot]
                    start, width = leaf.fields[name]
                    slots.append(f"{{{start}, {width}, CPU_PT_{kind}}}")
                else:
                    slots.append("{0, 0, CPU_PT_NONE}")
            lines.append(f"    {{0x{entry['var_mask']:08x}u, {entry['base']}, {features}, "
                         f"{{{', '.join(slots)}}}}},")
            entries.append(type(leaf)(leaf.name, leaf.subgroup, str(index), leaf.mask, leaf.value, leaf.guard,
                                      leaf.priority, leaf.fields, leaf.conds, leaf.features, leaf.scope))
        lines += ["};", "", "static inline int32_t cpu_pt_find(uint32_t word) {"]
        emit_tree(entries, lines, "    ")
        lines += ["    return -1;", "}", ""]
        return "\n".join(lines)

    def emit_code(self):
        lines = ["Lpt_cold:", "    b Lcold", "Lpt_wake:", "    b Lstlr_wake", "Lpt_fp:", "    mrs x9, nzcv",
                 "    str w9, [x19, #CPU_NZCV]", "    mov x0, x19", "    bl _cpu_fp_enter_guest_slow",
                 "    ldr w9, [x19, #CPU_NZCV]", "    msr nzcv, x9", "    DISPATCH", ""]
        prologues = sorted({s[3] for s in self.stubs if s})
        for prologue in prologues:
            lines.append(f"Lpt_in_{prologue}:")
            if prologue.startswith("simd_"):
                lines += ["    tbnz x23, #SALT_FP_TRAP, Lpt_cold",
                          "    ldrb w9, [x19, #CPU_FP_ACTIVE]",
                          "    cbz w9, Lpt_fp"]
                for host, (slot, kind) in enumerate(zip(SLOT_FIELDS, prologue[len("simd_"):])):
                    if kind == "_":
                        continue
                    lines.append(f"    ldrb w9, [x20, #{SLOT_OFFSETS[slot]}]")
                    lines.append(f"    ldr q{host}, [x19, x9, lsl #4]" if kind == "V"
                                 else f"    ldr x{host}, [x19, x9, lsl #3]")
            else:
                kind, size = re.fullmatch(r"([a-z]+)(\d+)", prologue).groups()
                lines.append("    BASE")
                if int(size) > 1:
                    lines += [f"    and x9, x10, #{int(size) - 1}", "    cbnz x9, Lpt_cold"]
                lines += ["    mov x12, x10", f"    TLB {'x21' if kind == 'acquire' else 'x22'}, 1, 1, Lpt_cold"]
                if kind == "atomic":
                    lines += ["    ldrb w9, [x20, #IR_RM]", "    ldr x1, [x19, x9, lsl #3]"]
                elif kind == "cas":
                    lines += ["    ldrb w9, [x20, #IR_RA]", "    ldr x1, [x19, x9, lsl #3]",
                              "    ldrb w9, [x20, #IR_RM]", "    ldr x0, [x19, x9, lsl #3]"]
                elif kind == "casp":
                    lines += ["    ldrb w9, [x20, #IR_RA]", "    add x9, x19, x9, lsl #3", "    ldp x0, x1, [x9]",
                              "    ldrb w9, [x20, #IR_RM]", "    add x9, x19, x9, lsl #3", "    ldp x2, x3, [x9]"]
            lines += ["    ret", ""]
        lines += [
            "Lpt_out_V:",
            "    ldrb w9, [x20, #IR_RD]",
            "    str q0, [x19, x9, lsl #4]",
            "    NEXT",
            "Lpt_out_X:",
            "    ldrb w9, [x20, #IR_RD]",
            "    str x0, [x19, x9, lsl #3]",
            "Lpt_out_none:",
            "    NEXT",
            "Lpt_out_casp:",
            "    ldrb w9, [x20, #IR_RA]",
            "    add x9, x19, x9, lsl #3",
            "    stp x0, x1, [x9]",
            "    str xzr, [x19, #(REG_ZR * 8)]",
            "    b Lpt_out_wake",
            "Lpt_out_cas:",
            "    mov x0, x1",
            "Lpt_out_atomic:",
            "    ldrb w9, [x20, #IR_RD]",
            "    str x0, [x19, x9, lsl #3]",
            "Lpt_out_wake:",
            "    ldr x10, [x19, #CPU_VM]",
            "    cbz x10, 1f",
            "    add x11, x10, #VM_WFE_WAITERS",
            "    ldar w11, [x11]",
            "    cbnz w11, Lpt_wake",
            "1:",
            "    NEXT",
            "",
        ]
        for index, stub in enumerate(self.stubs):
            if stub and stub[0] == index:
                _, _, word, prologue, epilogue, _ = stub
                lines += [f"Lpt_{index}:", f"    bl Lpt_in_{prologue}", f"    .long 0x{word:08x}",
                          f"    b Lpt_out_{epilogue}"]
        lines.append("")
        return "\n".join(lines)

    def emit_offsets(self):
        lines = []
        for stub in self.stubs:
            if stub:
                lines.append(f"    .long Lpt_{stub[0]} - Lcpu_fast_base{' + CPU_PT_WIDE_RM' if stub[1] else ''}")
            else:
                lines.append("    .long 0")
        return "\n".join(lines) + "\n"

    def emit_words(self):
        return "".join(f"    {{0x{s[2]:08x}u, \"{s[5].replace(chr(9), ' ')}\"}},\n"
                       for i, s in enumerate(self.stubs) if s and s[0] == i)
