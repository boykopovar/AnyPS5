"""Check that the converted Windows thread pointer sits after the TLS size rounded to the PT_TLS alignment."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture

FIRST_VARIABLE = 0x2a
SECOND_VARIABLE = 0x63
TLS_OFFSET = 0x980
MEMORY_SIZE = 0x21
ALIGNMENT = 8
ROUNDED_SIZE = (MEMORY_SIZE + ALIGNMENT - 1) // ALIGNMENT * ALIGNMENT


def guest_with_tls():
    image = fixture()
    struct.pack_into("<H", image, 56, 3)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, TLS_OFFSET, TLS_OFFSET, TLS_OFFSET, 0x10, MEMORY_SIZE, ALIGNMENT)
    struct.pack_into("<IxxxxQ", image, TLS_OFFSET, FIRST_VARIABLE, SECOND_VARIABLE)
    mov_rax_from_thread_pointer = bytes.fromhex("66 66 66 64 48 8b 04 25 00 00 00 00")
    read_first_variable = b"\x8b\x80" + struct.pack("<i", -ROUNDED_SIZE) + b"\xc3"
    code = mov_rax_from_thread_pointer + read_first_variable
    image[0x210:0x210 + len(code)] = code
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-thread-pointer-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        output = work / "output.exe"
        source.write_bytes(guest_with_tls())
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        if os.name == "nt":
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == FIRST_VARIABLE, \
                f"read {run.returncode:#x} instead of the first variable {FIRST_VARIABLE:#x}; stderr: {run.stderr}"
    print("Windows TLS thread pointer tests passed")


if __name__ == "__main__":
    main()
