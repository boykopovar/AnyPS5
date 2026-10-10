from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_jmprel_table_bounds import fixture, DT_RELA, DT_RELASZ


DT_INIT_ARRAY = 25
DT_FINI_ARRAY = 26
DT_INIT_ARRAYSZ = 27
DT_FINI_ARRAYSZ = 28
DT_PREINIT_ARRAY = 32
DT_PREINIT_ARRAYSZ = 33
DT_OS_INIT_ARRAY = 0x60000019
DT_OS_INIT_ARRAYSZ = 0x6000001B
DT_OS_PREINIT_ARRAY = 0x60000020
DT_OS_PREINIT_ARRAYSZ = 0x60000021
R_X86_64_GLOB_DAT = 6
R_X86_64_RELATIVE = 8
DYNAMIC = 0x400
RELA = 0x7A0
GOT_SLOT = 0x308
FUNCTION = 0x280
BEFORE_DYNAMIC = 0x380
AFTER_DYNAMIC = 0x540
LOAD_END = 0x800
KEPT = "CFG/GOT filtering: 1 -> 1; filtered=0"
DROPPED = "CFG/GOT filtering: 1 -> 0; filtered=1"


def image(array_tag=None, array_size_tag=None, array_va=0, slots=(), relative=(), load_size=None):
    elf = fixture()
    for index in range(16):
        tag, _ = struct.unpack_from("<qQ", elf, DYNAMIC + index * 16)
        if tag == DT_RELASZ:
            struct.pack_into("<qQ", elf, DYNAMIC + index * 16, DT_RELASZ, 24 * (1 + len(relative)))
    elf[0x200] = 0xC3
    elf[FUNCTION:FUNCTION + 8] = b"\x48\x8b\x05" + struct.pack("<i", GOT_SLOT - (FUNCTION + 7)) + b"\xc3"
    if array_tag is not None:
        tags = [(array_tag, array_va), (array_size_tag, 8 * len(slots)), (0, 0)]
        for index, tag in enumerate(tags):
            struct.pack_into("<qQ", elf, DYNAMIC + 13 * 16 + index * 16, *tag)
        struct.pack_into("<Q", elf, 120 + 32, 16 * 16)
        struct.pack_into("<Q", elf, 120 + 40, 16 * 16)
    for index, word in enumerate(slots):
        struct.pack_into("<Q", elf, array_va + index * 8, word)
    struct.pack_into("<QQq", elf, RELA, GOT_SLOT, (1 << 32) | R_X86_64_GLOB_DAT, 0)
    for index, (slot, addend) in enumerate(relative, 1):
        struct.pack_into("<QQq", elf, RELA + index * 24, slot, R_X86_64_RELATIVE, addend)
    if load_size is not None:
        struct.pack_into("<Q", elf, 64 + 32, load_size)
        struct.pack_into("<Q", elf, 64 + 40, load_size)
    return elf


def run(relinker, work, name, elf):
    source = work / (name + ".elf")
    output = work / (name + ".exe")
    source.write_bytes(elf)
    return subprocess.run([str(relinker), "--skip-sce-module", "--windows", "unused-filter=1", str(source), str(output)],
                          capture_output=True, text=True, timeout=20), output


def filtered(relinker, work, name, elf):
    result, output = run(relinker, work, name, elf)
    assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
    lines = [line for line in result.stdout.splitlines() if line.startswith("CFG/GOT filtering:")]
    assert len(lines) == 1, (name, result.stdout)
    return lines[0]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-init-array-") as directory:
        work = Path(directory)
        cases = [
            ("no-array", image(), DROPPED),
            ("init-before-dynamic", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION]), KEPT),
            ("init-after-dynamic", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, AFTER_DYNAMIC, [FUNCTION]), KEPT),
            ("init-os-tags", image(DT_OS_INIT_ARRAY, DT_OS_INIT_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION]), KEPT),
            ("init-second-slot", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [0, FUNCTION]), KEPT),
            ("init-terminators", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [(1 << 64) - 1, 0]), DROPPED),
            ("init-relative-addend", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [0],
                                           [(BEFORE_DYNAMIC, FUNCTION)]), KEPT),
            ("init-relative-overrides-word", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [0x200],
                                                   [(BEFORE_DYNAMIC, FUNCTION)]), KEPT),
            ("init-relative-terminator", image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION],
                                               [(BEFORE_DYNAMIC, 0)]), DROPPED),
            ("fini-before-dynamic", image(DT_FINI_ARRAY, DT_FINI_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION]), KEPT),
            ("fini-relative-addend", image(DT_FINI_ARRAY, DT_FINI_ARRAYSZ, AFTER_DYNAMIC, [0],
                                           [(AFTER_DYNAMIC, FUNCTION)]), KEPT),
            ("preinit-before-dynamic", image(DT_PREINIT_ARRAY, DT_PREINIT_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION]), KEPT),
            ("preinit-os-tags", image(DT_OS_PREINIT_ARRAY, DT_OS_PREINIT_ARRAYSZ, BEFORE_DYNAMIC, [FUNCTION]), KEPT),
        ]
        for name, elf, expected in cases:
            line = filtered(relinker, work, name, elf)
            assert line == expected, (name, line, expected)

        result, output = run(relinker, work, "unmapped-array",
                             image(DT_INIT_ARRAY, DT_INIT_ARRAYSZ, LOAD_END + 0x100, [FUNCTION], load_size=LOAD_END))
        assert result.returncode == 2 and "array is not file-backed" in result.stderr and not output.exists(), (
            result.returncode, result.stdout, result.stderr)
    print("Init array root tests passed")


if __name__ == "__main__":
    main()
