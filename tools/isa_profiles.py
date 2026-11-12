import hashlib
import re
import xml.etree.ElementTree as ET
from pathlib import Path


ISA_RELEASE = "AMD_GPU_MR_ISA_XML_2026_08_06"
ISA_HASHES = {
    "rdna1": "f60248605e8c4cb9ae29130ce3114142467d953a3f1c57a1b0ca6d8de014e722",
    "rdna2": "d671ecbc36543674ab59e9b2feffd56718fd2137e7ad913c314f2e2508b9f4f7",
}
ISA_ARCHITECTURES = {"rdna1": "AMD RDNA 1", "rdna2": "AMD RDNA 2"}
LLVM_TAG = "llvmorg-19.1.0"
LLVM_SOURCE = f"https://github.com/llvm/llvm-project/blob/{LLVM_TAG}/llvm/lib/Target/AMDGPU/"
GFX1013_BVH = {"IMAGE_BVH_INTERSECT_RAY", "IMAGE_BVH64_INTERSECT_RAY"}
FORMS = ("base", "vop3", "dpp16", "dpp8")
ENCODINGS = {
    "DS", "EXP", "FLAT", "GLOBAL", "MIMG", "MTBUF", "MUBUF", "SCRATCH", "SMEM", "SOP1", "SOP2", "SOPC",
    "SOPK", "SOPP", "VINTRP", "VOP1", "VOP2", "VOP3", "VOP3P", "VOPC",
}


def validate_isa_xml(xmltext, architecture):
    if architecture not in ISA_HASHES:
        raise ValueError(f"unknown ISA architecture: {architecture}")
    try:
        root = ET.fromstring(xmltext)
    except ET.ParseError as error:
        raise ValueError(f"invalid ISA XML: {error}") from error
    actual = root.findtext("ISA/Architecture/ArchitectureName")
    if actual != ISA_ARCHITECTURES[architecture]:
        raise ValueError(f"expected {ISA_ARCHITECTURES[architecture]} XML, got {actual!r}")
    digest = hashlib.sha256(xmltext.encode("utf-8")).hexdigest()
    if digest != ISA_HASHES[architecture]:
        raise ValueError(f"{architecture} XML hash differs from {ISA_RELEASE}: {digest}")
    return root


def canonical_encoding(tag, native):
    literal = re.fullmatch(r"(\w+)_INST_LITERAL", tag)
    if literal:
        tag = literal.group(1)
        if tag != "VOP3_SDST_ENC":
            tag = f"ENC_{tag}"
    if tag in ("ENC_VOP3", "VOP3_SDST_ENC"):
        return (native, "vop3") if native in {"VOP1", "VOP2", "VOPC"} else ("VOP3", "base")
    if tag == "ENC_FLAT_GLBL":
        return "GLOBAL", "base"
    if tag == "ENC_FLAT_SCRATCH":
        return "SCRATCH", "base"
    own = re.fullmatch(r"ENC_(\w+)", tag)
    if own:
        return own.group(1), "base"
    modified = re.fullmatch(r"(VOP1|VOP2|VOPC)_VOP_(DPP16|DPP8)", tag)
    if modified:
        return modified.group(1), modified.group(2).lower()
    return None


def canonical_xml_forms(root):
    forms = set()
    instructions = root.find("ISA/Instructions")
    if instructions is None:
        raise ValueError("ISA XML has no instructions")
    for instruction in instructions:
        name = instruction.findtext("InstructionName")
        encodings = [(item.findtext("EncodingName"), int(item.findtext("Opcode")))
                     for item in instruction.find("InstructionEncodings")]
        native = next((match.group(1) for tag, _ in encodings
                       if (match := re.fullmatch(r"ENC_(\w+)", tag)) and match.group(1) != "VOP3"), None)
        for tag, opcode in encodings:
            kind = canonical_encoding(tag, native)
            if kind:
                forms.add((name, *kind, opcode))
    return forms


def llvm_gfx1013_additional_forms():
    forms = []
    for prefix, start in (("S_STORE", 0x10), ("S_SCRATCH_STORE", 0x15), ("S_BUFFER_STORE", 0x18)):
        for offset, suffix in enumerate(("DWORD", "DWORDX2", "DWORDX4")):
            forms.append((f"{prefix}_{suffix}", "SMEM", "base", start + offset))
    atomic_operations = ("SWAP", "CMPSWAP", "ADD", "SUB", "SMIN", "UMIN", "SMAX", "UMAX", "AND", "OR", "XOR",
                         "INC", "DEC")
    for prefix, start in (("S_BUFFER_ATOMIC", 0x40), ("S_ATOMIC", 0x80)):
        for suffix, width_offset in (("", 0), ("_X2", 0x20)):
            for offset, operation in enumerate(atomic_operations):
                forms.append((f"{prefix}_{operation}{suffix}", "SMEM", "base", start + width_offset + offset))
    ds_operations = (
        ("ADD", "U", 0x00), ("SUB", "U", 0x01), ("RSUB", "U", 0x02), ("INC", "U", 0x03),
        ("DEC", "U", 0x04), ("MIN", "I", 0x05), ("MAX", "I", 0x06), ("MIN", "U", 0x07),
        ("MAX", "U", 0x08), ("AND", "B", 0x09), ("OR", "B", 0x0a), ("XOR", "B", 0x0b),
        ("WRITE", "B", 0x0d), ("MIN", "F", 0x12), ("MAX", "F", 0x13),
    )
    for width, start in ((32, 0x80), (64, 0xc0)):
        for operation, kind, offset in ds_operations:
            forms.append((f"DS_{operation}_SRC2_{kind}{width}", "DS", "base", start + offset))
    forms.extend((
        ("DS_ADD_SRC2_F32", "DS", "base", 0x95),
        ("S_DCACHE_WB", "SMEM", "base", 0x21),
        ("S_DCACHE_DISCARD", "SMEM", "base", 0x28),
        ("S_DCACHE_DISCARD_X2", "SMEM", "base", 0x29),
        ("V_MUL_LO_I32", "VOP3", "base", 0x16b),
    ))
    return forms


def validate_forms(forms):
    keys = {}
    for row in forms:
        name, encoding, form, opcode = row
        if (not re.fullmatch(r"[A-Z][A-Z0-9_]*", name) or encoding not in ENCODINGS or form not in FORMS
                or type(opcode) is not int or opcode < 0):
            raise ValueError(f"invalid ISA form: {row}")
        if form != "base" and encoding not in {"VOP1", "VOP2", "VOPC"}:
            raise ValueError(f"invalid modified ISA form: {row}")
        key = ("VOP3", "base", opcode) if form == "vop3" else (encoding, form, opcode)
        if key in keys:
            raise ValueError(f"duplicate ISA encoding: {keys[key]} and {row}")
        keys[key] = row
    return forms


def generate_gfx1013_forms(rdna1_xmltext, rdna2_xmltext):
    forms = canonical_xml_forms(validate_isa_xml(rdna1_xmltext, "rdna1"))
    rdna2 = canonical_xml_forms(validate_isa_xml(rdna2_xmltext, "rdna2"))
    forms.update(row for row in rdna2 if row[0] in GFX1013_BVH)
    forms.update(llvm_gfx1013_additional_forms())
    return validate_forms(sorted(forms))


def load_gfx1013_forms(path=None):
    path = Path(path) if path is not None else Path(__file__).with_name("gfx1013_isa_forms.txt")
    forms = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        try:
            name, encoding, form, opcode = line.split()
            forms.append((name, encoding, form, int(opcode)))
        except ValueError as error:
            raise ValueError(f"invalid ISA form at {path}:{number}") from error
    if not forms:
        raise ValueError(f"no ISA forms in {path}")
    return validate_forms(forms)


def gfx1013_forms_header():
    return f"""# gfx1013 public reference instruction forms: name encoding form opcode
# This is a source coverage reference, not a complete or hardware-verified PS5 ISA specification.
# Scope: canonical base/VOP3 opcodes and DPP16/DPP8 forms. Mandatory literal instructions count once;
# optional literals, operand combinations, SDWA and MIMG NSA variants are not separate rows here.
# AMD RDNA 1 baseline, plus RDNA 2 BVH A-encoding forms and LLVM gfx1013 supplemental opcodes.
# RDNA 2 B-encoding, dot-product feature extensions and GFX10.3 legacy-FMA replacements are excluded.
# Alias spellings and repeated literal positions do not add rows; distinct hardware opcodes do.
# AMD XML bundle: {ISA_RELEASE}
# amdgpu_isa_rdna1.xml sha256: {ISA_HASHES['rdna1']}
# amdgpu_isa_rdna2.xml sha256: {ISA_HASHES['rdna2']}
# LLVM reference: {LLVM_TAG}
# GFX10.1 target features and GFX10.3 differences: {LLVM_SOURCE}AMDGPU.td#L1486-L1550
# gfx1013 feature inheritance: {LLVM_SOURCE}AMDGPU.td#L1535-L1537
# BVH A-encoding: {LLVM_SOURCE}MIMGInstructions.td#L1774-L1784
# Scalar stores and cache writeback: {LLVM_SOURCE}SMInstructions.td#L1223-L1239
# Scalar atomics and cache discard: {LLVM_SOURCE}SMInstructions.td#L1276-L1340
# DS SRC2 integer and floating point forms: {LLVM_SOURCE}DSInstructions.td#L1526-L1555
# DS_ADD_SRC2_F32: {LLVM_SOURCE}DSInstructions.td#L1358
# V_MUL_LO_I32: {LLVM_SOURCE}VOP3Instructions.td#L1292
# Regenerate with the pinned XML files:
#   python3 tools/progress.py --regenerate-gfx1013-forms amdgpu_isa_rdna1.xml amdgpu_isa_rdna2.xml
#
# Copyright (c) 2026 Advanced Micro Devices, Inc., or its affiliates. The XML is published under the MIT licence:
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
# documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
# persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and
# this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""


def gfx1013_forms_text(rdna1_xmltext, rdna2_xmltext):
    forms = generate_gfx1013_forms(rdna1_xmltext, rdna2_xmltext)
    return gfx1013_forms_header() + "".join(f"{name} {encoding} {form} {opcode}\n" for name, encoding, form, opcode in forms)


def write_gfx1013_forms(path, rdna1_xmltext, rdna2_xmltext):
    Path(path).write_bytes(gfx1013_forms_text(rdna1_xmltext, rdna2_xmltext).encode("utf-8"))
