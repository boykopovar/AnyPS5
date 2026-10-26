"""Exercise optional PLT metadata through the real ELF-to-PE pipeline."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def fixture(extra_tags=()):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 2, 64, 0, 0)
    # Return through a relocated function pointer. This verifies that ordinary
    # RELATIVE relocations still work when the ELF has no PLT relocation table.
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    image[0x210:0x216] = b"\xb8\x2a\x00\x00\x00\xc3"
    struct.pack_into("<QQq", image, 0x700, 0x300, 8, 0x210)
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24),
            (7, 0x700), (8, 24), (9, 24), *extra_tags, (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-plt-") as directory:
        work = Path(directory)

        def convert(name, tags=(), error=None):
            source = work / (name + ".elf")
            output = work / (name + ".exe")
            source.write_bytes(fixture(tags))
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if error is not None:
                if result.returncode != 2 or error not in result.stderr or output.exists():
                    raise AssertionError((name, result.returncode, result.stdout, result.stderr))
                return
            if result.returncode != 0 or output.read_bytes()[:2] != b"MZ":
                raise AssertionError((name, result.stdout, result.stderr))
            if os.name == "nt":
                executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
                if executed.returncode != 42:
                    raise AssertionError((name, executed.returncode, executed.stdout, executed.stderr))

        def convert_strict(name, thunks=(), error=None, invalid_slot=False):
            code_base = 0x1000
            slot = 0x800
            if invalid_slot:
                slot = 0x1000 + 0x20 + 6 - 0x2000 + (1 << 64)
            image = bytearray(0x1200)
            image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
            struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, code_base, 64, 0, 0, 64, 56, 8, 64, 0, 0)
            struct.pack_into("<IIQQQQQQ", image, 64, 1, 6, 0, 0, 0, 0x1000, 0x1000, 0x1000)
            struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, 0x100, 0x100, 8)
            struct.pack_into("<IIQQQQQQ", image, 176, 1, 5, code_base, code_base, code_base, 0x200, 0x200, 0x1000)
            for idx in range(3, 8):
                struct.pack_into("<IIQQQQQQ", image, 64 + idx * 56, 0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
            relocations = [(slot, (1 << 32) | 7, 0)]
            tags = [(1, 4), (5, 0x600), (10, 12), (6, 0x620), (11, 24), (3, slot if not invalid_slot else 0x800), (2, 24), (20, 7), (23, 0x700),
                    (7, 0x7a0), (8, 0), (9, 24), (0x6100003f, 48), (0, 0)]
            for idx, tag in enumerate(tags):
                struct.pack_into("<qQ", image, 0x400 + idx * 16, *tag)
            image[0x600:0x600 + 11] = b"foo\0lib.so\0"
            for idx, relocation in enumerate(relocations):
                struct.pack_into("<QQq", image, 0x700 + idx * 24, *relocation)
            if not invalid_slot:
                valid_disp = slot - (code_base + 6)
                struct.pack_into("<HiBIB", image, code_base, 0x25ff, valid_disp, 0x68, 0, 0xe9)
            for offset, disp, idx in thunks:
                struct.pack_into("<HiBIB", image, code_base + offset, 0x25ff, disp, 0x68, idx, 0xe9)
            source = work / (name + ".elf")
            output = work / (name + ".exe")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "--registry", "unused-filter=2",
                                     str(source), str(output)], capture_output=True, text=True, timeout=20)
            if error is not None:
                if result.returncode != 2 or error not in result.stderr or output.exists():
                    raise AssertionError((name, result.returncode, result.stdout, result.stderr))
                return
            if result.returncode != 0 or output.read_bytes()[:2] != b"MZ":
                raise AssertionError((name, result.stdout, result.stderr))

        convert("no-plt")
        convert("got-without-plt", [(3, 0x300)])
        convert("empty-plt", [(3, 0x300), (2, 0), (20, 7), (23, 0x720)])
        convert("partial-plt", [(3, 0x300), (2, 24)], "DT_PLTREL")
        convert("missing-got", [(2, 0), (20, 7), (23, 0x720)], "DT_PLTGOT")
        convert("invalid-plt-type", [(3, 0x300), (2, 0), (20, 17), (23, 0x720)],
                "Unsupported DT_PLTREL")
        convert("unaligned-plt", [(3, 0x300), (2, 1), (20, 7), (23, 0x720)],
                "Invalid DT_PLTRELSZ")
        convert("duplicate-got-variant", [(3, 0x300), (0x61000027, 0x300)],
                "Both DT_OS_ and DT_ variants")
        convert_strict("strict-compaction")
        convert_strict("strict-thunk-underflow-ignored", [(0x20, -0x2000, 99)])
        convert_strict("strict-underflow-missing-slot", [(0x20, -0x2000, 99)], "canonical PLT thunk was not found", invalid_slot=True)
    print("Optional PLT integration tests passed")


if __name__ == "__main__":
    main()
