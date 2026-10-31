"""Check that --lazy-binding defers only unresolved Windows imports of STT_FUNC symbols.

A called stub of an unresolved function import must raise STATUS_ENTRYPOINT_NOT_FOUND from ABI-aligned guest code,
whether the relocation is JUMP_SLOT, GLOB_DAT or R_X86_64_64, with or without an addend.
An unresolved data import must fail at startup before the ELF entry, as without --lazy-binding.
"""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

NEEDED = b"libfoo.prx"
SLOT = 0x900
CALL_SLOT = bytes.fromhex("48 83 ec 08 ff 15") + struct.pack("<i", SLOT - 0x20a) + bytes.fromhex("48 83 c4 08 b8 07 00 00 00 c3")
RETURN_42 = bytes.fromhex("b8 2a 00 00 00 c3")


def fixture(name, info, relocation, code, addend):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x200, 64, 0, 0, 64, 56, 2, 64, 0, 0)
    strings = b"\0" + name + b"\0" + NEEDED + b"\0"
    image[0x600:0x600 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", image, 0x620 + 24, 1, info, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x700, SLOT, (1 << 32) | relocation, addend)
    tags = [(1, 2 + len(name)), (5, 0x600), (10, len(strings)), (6, 0x620), (11, 24),
            (7, 0x700), (8, 24), (9, 24), (0x6100003f, 48), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    image[0x200:0x200 + len(code)] = code
    return image


def convert(relinker, work, case, name, info, relocation, code, addend=0):
    source = work / f"{case}.elf"
    output = work / f"{case}.exe"
    source.write_bytes(fixture(name, info, relocation, code, addend))
    result = subprocess.run([str(relinker), "--windows", "--lazy-binding", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0 and output.read_bytes()[:2] == b"MZ", (case, result.returncode, result.stdout, result.stderr)
    return output


def run(work, output):
    executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20, cwd=work)
    return executed.returncode & 0xffffffff, executed.stdout, executed.stderr


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-lazy-imports-") as directory:
        work = Path(directory)
        functions = [(case, addend, convert(relinker, work, case, b"NoSuchFunctionXyz", 0x12, relocation, CALL_SLOT, addend))
                     for case, relocation, addend in [("function-glob-dat", 6, 0), ("function-64", 1, 0), ("function-64-addend", 1, 0x10)]]
        data = [(case, convert(relinker, work, case, b"NoSuchDataXyz", info, relocation, RETURN_42))
                for case, info, relocation in [("object-glob-dat", 0x11, 6), ("object-64", 0x11, 1), ("notype-64", 0x10, 1)]]
        for case, addend, output in functions:
            if addend:
                assert bytes.fromhex("48 ba") + struct.pack("<Q", addend) + bytes.fromhex("48 01 d0") in output.read_bytes(), case
        if os.name == "nt":
            for case, addend, output in functions:
                status, stdout, stderr = run(work, output)
                assert status == 0xc0000139, f"{case}: status {status:#x}, stderr {stderr!r}"
                assert "FAIL: unresolved ELF import NoSuchFunctionXyz\n" in stderr, (case, stderr)
                assert "Transferring control to ELF entry point" in stdout and "Searched libraries" not in stderr, (case, stdout, stderr)
            for case, output in data:
                status, stdout, stderr = run(work, output)
                assert status == 0xc0000139, f"{case}: status {status:#x}, stderr {stderr!r}"
                assert "FAIL: unresolved ELF import NoSuchDataXyz\n" in stderr, (case, stderr)
                assert "Transferring control to ELF entry point" not in stdout, (case, stdout)
    print("Windows lazy import tests passed")


if __name__ == "__main__":
    main()
