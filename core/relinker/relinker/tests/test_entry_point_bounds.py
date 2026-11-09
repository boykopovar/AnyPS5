from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_jmprel_table_bounds import fixture


DT_RELASZ = 8
R_X86_64_GLOB_DAT = 6
SHT_SYMTAB = 2
MASK = (1 << 64) - 1
GOT_SLOT = 0x308
FUNCTION = 0x280
SECTION = 0x800
SYMBOL = 0x900


def image(shoff=0, shnum=0, shentsize=64, shstrndx=0, symoff=SYMBOL, symsize=24, symentsize=24):
    elf = fixture()
    for index in range(16):
        tag, _ = struct.unpack_from("<qQ", elf, 0x400 + index * 16)
        if tag == DT_RELASZ:
            struct.pack_into("<qQ", elf, 0x400 + index * 16, DT_RELASZ, 24)
    struct.pack_into("<QQq", elf, 0x7A0, GOT_SLOT, (1 << 32) | R_X86_64_GLOB_DAT, 0)
    elf[FUNCTION:FUNCTION + 8] = b"\x48\x8b\x05" + struct.pack("<i", GOT_SLOT - (FUNCTION + 7)) + b"\xc3"
    struct.pack_into("<Q", elf, 40, shoff)
    struct.pack_into("<HHH", elf, 58, shentsize, shnum, shstrndx)
    struct.pack_into("<IIQQQQIIQQ", elf, SECTION, 0, SHT_SYMTAB, 0, 0, symoff, symsize, 1, 0, 8, symentsize)
    elf[SYMBOL + 4] = 0x12
    struct.pack_into("<HQQ", elf, SYMBOL + 6, 1, FUNCTION, 1)
    return elf


def filtered(relinker, work, name, elf):
    source = work / (name + ".elf")
    output = work / (name + ".exe")
    source.write_bytes(elf)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "unused-filter=1", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
    lines = [line for line in result.stdout.splitlines() if line.startswith("CFG/GOT filtering:")]
    assert len(lines) == 1, (name, result.stdout)
    return lines[0]


def main():
    relinker = Path(sys.argv[1]).resolve()
    kept = "CFG/GOT filtering: 1 -> 1; filtered=0"
    dropped = "CFG/GOT filtering: 1 -> 0; filtered=1"
    with tempfile.TemporaryDirectory(prefix="anyps5-entry-points-") as directory:
        work = Path(directory)
        cases = [
            ("no-sections", image(), dropped),
            ("symbol-in-bounds", image(shoff=SECTION, shnum=1), kept),
            ("section-table-wraps", image(shoff=(-64) & MASK, shnum=2, shentsize=SECTION + 64, shstrndx=1), dropped),
            ("symbol-table-wraps", image(shoff=SECTION, shnum=1, symoff=(-24) & MASK, symsize=2 * (SYMBOL + 24), symentsize=SYMBOL + 24), dropped),
        ]
        for name, elf, expected in cases:
            line = filtered(relinker, work, name, elf)
            assert line == expected, (name, line, expected)
    print("Entry point bounds tests passed")


if __name__ == "__main__":
    main()
