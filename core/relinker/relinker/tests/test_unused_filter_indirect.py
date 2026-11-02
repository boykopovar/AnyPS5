import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

CODE = 0x1000
SLOT = 0x800
POINTER = 0x810
TABLE = 0x820
FUNCTION = CODE + 0x40
SECOND_FUNCTION = CODE + 0x60
REJECTED = {"got-slot-address-size-underflow":
            "RELA r_offset 0xfffff008 (8 bytes) does not fit inside a PT_LOAD segment"}


def rel(next_address, target):
    return struct.pack("<i", target - next_address)


def build(code, relative=(), data=(), code_vaddr=CODE, slot_vaddr=SLOT):
    image = bytearray(0x1200)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, code_vaddr, 64, 0, 0, 64, 56, 8, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 6, 0, 0, 0, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, 0xb0, 0xb0, 8)
    struct.pack_into("<IIQQQQQQ", image, 176, 1, 5, 0x1000, code_vaddr, code_vaddr, 0x200, 0x200, 0x1000)
    for slot in range(3, 8):
        struct.pack_into("<IIQQQQQQ", image, 64 + slot * 56, 0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    relocations = [(slot_vaddr, (1 << 32) | 6, 0)] + [(slot, 8, target) for slot, target in relative]
    tags = [(1, 5), (5, 0x600), (10, 12), (6, 0x620), (11, 24), (7, 0x700), (8, 24 * len(relocations)),
            (9, 24), (0x6100003f, 48), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    image[0x600:0x600 + 12] = b"\0foo\0lib.so\0"
    struct.pack_into("<I", image, 0x638, 1)
    for index, relocation in enumerate(relocations):
        struct.pack_into("<QQq", image, 0x700 + index * 24, *relocation)
    for offset, value in data:
        image[offset:offset + len(value)] = value
    image[0x1000:0x1000 + len(code)] = code
    return image


def at_offset(parts):
    code = bytearray(0x200)
    for offset, value in parts:
        code[offset:offset + len(value)] = value
    return bytes(code)


def load_slot(address):
    return b"\x48\x8b\x05" + rel(address + 7, SLOT) + b"\xc3"


def with_import_type(image, relocation_type):
    struct.pack_into("<Q", image, 0x708, (1 << 32) | relocation_type)
    return image


def run(relinker, work, name, image, level=1):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", f"unused-filter={level}",
                             str(source), str(output)], capture_output=True, text=True, timeout=20)
    if name in REJECTED:
        assert result.returncode == 2 and REJECTED[name] in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)
        assert "CFG/GOT filtering: 1 -> 1;" in result.stdout, (name, result.stdout)
        return True
    assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stdout, result.stderr)
    registry = json.loads(output.with_suffix(".registry.json").read_text())
    kept = len(registry)
    assert kept in (0, 1) and all(entry["nid"] == "foo" for entry in registry), (name, registry)
    if level == 1:
        assert f"CFG/GOT filtering: 1 -> {kept};" in result.stdout, (name, result.stdout)
    return kept == 1


def exported_fixture(binding, defined, sections):
    image = build(at_offset([(0, b"\xc3"), (0x40, load_slot(FUNCTION))]))
    strings = b"\0foo\0lib.so\0exported\0"
    image[0x600:0x600 + len(strings)] = strings
    struct.pack_into("<Q", image, 0x408, 5)
    struct.pack_into("<Q", image, 0x400 + 2 * 16 + 8, len(strings))
    struct.pack_into("<Q", image, 0x400 + 8 * 16 + 8, 72)
    import_index = 2 if binding == 0 else 1
    export_index = 1 if binding == 0 else 2
    struct.pack_into("<IBBHQQ", image, 0x620 + import_index * 24, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x620 + export_index * 24, 12, (binding << 4) | 2, 0,
                     1 if defined else 0, FUNCTION if defined else 0, 8 if defined else 0)
    struct.pack_into("<Q", image, 0x708, (import_index << 32) | 6)
    if sections:
        image.extend(bytes(4 * 64))
        struct.pack_into("<Q", image, 40, 0x1200)
        struct.pack_into("<H", image, 60, 4)
        struct.pack_into("<IIQQQQIIQQ", image, 0x1240, 0, 1, 6, CODE, CODE, 0x200, 0, 0, 16, 0)
        struct.pack_into("<IIQQQQIIQQ", image, 0x1280, 0, 11, 2, 0x620, 0x620, 72, 3, import_index, 8, 24)
        struct.pack_into("<IIQQQQIIQQ", image, 0x12c0, 0, 3, 2, 0x600, 0x600, len(strings), 0, 0, 1, 0)
    return image


def cases():
    lea_call = b"\x48\x8d\x05" + rel(CODE + 7, FUNCTION) + b"\xff\xd0\xc3"
    yield "direct-call", True, build(at_offset([(0, b"\xe8" + rel(CODE + 5, FUNCTION) + b"\xc3"),
                                                (0x40, load_slot(FUNCTION))]))
    yield "lea-call-register", True, build(at_offset([(0, lea_call), (0x40, load_slot(FUNCTION))]))
    vtable_call = b"\x48\x8b\x05" + rel(CODE + 7, POINTER) + b"\xff\xd0\xc3"
    yield "relative-relocation-pointer", True, build(
        at_offset([(0, vtable_call), (0x40, load_slot(FUNCTION))]), relative=[(POINTER, FUNCTION)])
    jump = (b"\x48\x8d\x0d" + rel(CODE + 7, TABLE) + b"\x48\x63\x04\x91\x48\x01\xc8\xff\xe0")
    table = struct.pack("<ii", SECOND_FUNCTION - TABLE, FUNCTION - TABLE)
    yield "jump-table-case-block", True, build(
        at_offset([(0, jump), (0x40, b"\xc3"), (0x60, load_slot(SECOND_FUNCTION))]), data=[(TABLE, table)])
    yield "got-slot-lea", True, build(at_offset([(0, b"\x48\x8d\x05" + rel(CODE + 7, SLOT) + b"\xc3")]))
    yield "got-slot-push", True, build(at_offset([(0, b"\xff\x35" + rel(CODE + 6, SLOT) + b"\x58\xc3")]))
    yield "got-slot-compare", True, build(
        at_offset([(0, b"\x48\x83\x3d" + rel(CODE + 8, SLOT) + b"\x00\xc3")]))
    yield "got-slot-address-size", True, build(
        at_offset([(0, b"\x67\x48\x8d\x05" + struct.pack("<i", -2056) + b"\xc3")]),
        code_vaddr=0x100001000)
    yield "got-slot-address-size-underflow", True, build(
        at_offset([(0, b"\x67\x48\x8d\x05\x00\xe0\xff\xff\xc3")]),
        slot_vaddr=(CODE + 8 - 0x2000) & 0xffffffff)
    yield "got-slot-underflow-discard", False, build(
        at_offset([(0, b"\x48\x8d\x05\x00\xe0\xff\xff\xc3")]),
        slot_vaddr=(CODE + 7 - 0x2000) & 0xffffffffffffffff)
    yield "unreachable-user", False, build(at_offset([(0, b"\xc3"), (0x40, load_slot(FUNCTION))]))
    yield "unreferenced-pointer-target", False, build(
        at_offset([(0, b"\xc3"), (0x40, b"\xc3"), (0x60, load_slot(SECOND_FUNCTION))]),
        relative=[(POINTER, FUNCTION)])
    split = CODE + 0x1ff
    yield "undecodable-pointer-target", True, build(
        at_offset([(0, b"\xe8" + rel(CODE + 5, FUNCTION) + b"\xc3"), (0x40, load_slot(FUNCTION)), (0x1ff, b"\x0f")]),
        relative=[(POINTER, split)])
    table_lea = b"\x48\x8d\x0d" + rel(CODE + 7, TABLE) + b"\xc3"
    yield "undecodable-table-target", False, build(
        at_offset([(0, table_lea), (0x40, load_slot(FUNCTION)), (0x1ff, b"\x0f")]),
        data=[(TABLE, struct.pack("<i", split - TABLE))])
    table_call = b"\x48\x8d\x05" + rel(CODE + 7, SLOT - 8) + b"\xff\x50\x08\xc3"
    yield "data-pointer-at-offset", True, with_import_type(build(at_offset([(0, table_call)])), 1)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-unused-filter-indirect-") as directory:
        failures = [name for name, expected, image in cases()
                    if run(relinker, Path(directory), name, image) != expected]
        for sections in (False, True):
            for kind, binding, defined in (("global", 1, True), ("weak", 2, True),
                                           ("local", 0, True), ("undefined", 1, False)):
                for level in (0, 1, 2):
                    name = f"exported-{kind}-sections-{sections}-filter-{level}"
                    expected = level != 1 or (binding in (1, 2) and defined)
                    if run(relinker, Path(directory), name, exported_fixture(binding, defined, sections), level) != expected:
                        failures.append(name)
        assert not failures, failures
    print("Unused filter indirect tests passed")


if __name__ == "__main__":
    main()
