"""Check that a Linux relink rejects an entry point, relocation offset or DT_PLTGOT outside PT_LOAD memory."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_jmprel_table_bounds import fixture

PT_LOAD = 1
PT_SCE_RELRO = 0x61000010
PF_W = 2
PF_R = 4
R_X86_64_PC32 = 2
R_X86_64_COPY = 5
R_X86_64_32 = 10
E_ENTRY = 24
SPARE_HEADER = 64 + 3 * 56
PLTGOT_VALUE = 0x400 + 9 * 16 + 8
SYMBOL_SIZE = 0x620 + 24 + 16
OUTSIDE = 0x2000


def with_bss(image):
    struct.pack_into("<IIQQQQQQ", image, SPARE_HEADER, PT_LOAD, PF_R | PF_W, 0x1000, 0x1000, 0x1000, 0, 0x1000, 0x1000)
    return image


def with_relro(image):
    struct.pack_into("<IIQQQQQQ", image, SPARE_HEADER, PT_SCE_RELRO, PF_R, 0, 0x1000, 0x1000, 0, 0x1000, 1)
    return with_pltgot(image, 0x1800)


def with_entry(image, entry):
    struct.pack_into("<Q", image, E_ENTRY, entry)
    return image


def with_pltgot(image, value):
    struct.pack_into("<Q", image, PLTGOT_VALUE, value)
    return image


def with_symbol_size(image, size):
    struct.pack_into("<Q", image, SYMBOL_SIZE, size)
    return image


def relink(relinker, work, name, image, options, error):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if error is None:
        assert result.returncode == 0 and output.exists(), (name, options, result.returncode, result.stdout, result.stderr)
        output.unlink()
    else:
        assert result.returncode == 2 and error in result.stderr and not output.exists(), (
            name, options, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = [
        ("plt-valid", fixture(linux=True), None),
        ("rela-valid", fixture(linux=True, data_relocations=True), None),
        ("rela-in-bss", with_bss(fixture(linux=True, data_relocations=True, target=0x1ff8)), None),
        ("none-outside", fixture(linux=True, data_relocations=True, relocation_type=0, symbol_index=0, target=OUTSIDE), None),
        ("rela-in-relro", with_relro(fixture(linux=True, data_relocations=True, target=0x1ff8)), None),
        ("jmprel-in-relro", with_relro(fixture(linux=True, target=0x1ff8)), None),
        ("pc32-load-tail", fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_PC32, target=0xFFC), None),
        ("abs32-load-tail", fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_32, target=0xFFC), None),
        ("copy-load-tail", with_symbol_size(
            fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_COPY, target=0xFFC), 4), None),
        ("copy-load-last-byte", with_symbol_size(
            fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_COPY, target=0xFFF), 4), None),
        ("entry-outside", with_entry(fixture(linux=True), 0x7FFFF000),
         "e_entry 0x7ffff000 is not inside an executable PT_LOAD segment"),
        ("entry-not-executable", with_entry(with_bss(fixture(linux=True)), 0x1000),
         "e_entry 0x1000 is not inside an executable PT_LOAD segment"),
        ("rela-outside", fixture(linux=True, data_relocations=True, target=OUTSIDE),
         "RELA r_offset 0x2000 (8 bytes) does not fit inside a PT_LOAD segment"),
        ("rela-past-load-end", fixture(linux=True, data_relocations=True, target=0xFFC),
         "RELA r_offset 0xffc (8 bytes) does not fit inside a PT_LOAD segment"),
        ("abs32-past-load-end", fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_32, target=0xFFE),
         "RELA r_offset 0xffe (4 bytes) does not fit inside a PT_LOAD segment"),
        ("copy-outside", fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_COPY, target=OUTSIDE),
         "RELA r_offset 0x2000 is not inside a PT_LOAD segment"),
        ("jmprel-outside", fixture(linux=True, target=OUTSIDE),
         "JMPREL r_offset 0x2000 (8 bytes) does not fit inside a PT_LOAD segment"),
    ]
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-load-bounds-") as directory:
        work = Path(directory)
        for name, image, error in cases:
            for options in ([], ["--lazy-binding"]):
                relink(relinker, work, name, image, options, error)
        pltgot_cases = [
            ("pltgot-outside", OUTSIDE, "DT_PLTGOT 0x2000 (8 bytes) does not fit inside a PT_LOAD segment",
             "DT_PLTGOT 0x2000 (24 bytes) does not fit inside a PT_LOAD segment"),
            ("pltgot-load-tail", 0xFF0, None, "DT_PLTGOT 0xff0 (24 bytes) does not fit inside a PT_LOAD segment"),
            ("pltgot-lazy-fits", 0xFE8, None, None),
        ]
        for name, pltgot, eager_error, lazy_error in pltgot_cases:
            image = with_pltgot(fixture(linux=True), pltgot)
            relink(relinker, work, name, image, [], eager_error)
            relink(relinker, work, name, image, ["--lazy-binding"], lazy_error)
    print("Linux load bounds tests passed")


if __name__ == "__main__":
    main()
