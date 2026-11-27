"""Check that a Linux relink keeps the bytes an earlier PT_LOAD leaves in the first page of a PT_LOAD without file bytes."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from signal_run import run_without_core_dump
from test_linux_program_header_order import PT_LOAD, TEXT, multi_load_fixture

DATA = (PT_LOAD, 6, 0x6000, 0x2000, 0x0800, 0x0800, 0x1000)
CHECKS = [(0x27F0, 0x5A), (0x2800, 0), (0x2FFF, 0), (0x3000, 0), (0x37FF, 0)]


def checking_code(checks):
    code = bytearray()
    jumps = []
    for address, value in checks:
        code += b"\x80\x3d" + struct.pack("<i", address - (len(code) + 7)) + bytes([value])
        code += b"\x0f\x85" + bytes(4)
        jumps.append(len(code) - 4)
    code += b"\xcc"
    for jump in jumps:
        struct.pack_into("<i", code, jump, len(code) - (jump + 4))
    return bytes(code + b"\x0f\x0b")


def fixture(empty_offset):
    image = multi_load_fixture([TEXT, DATA, (PT_LOAD, 6, empty_offset, 0x2800, 0, 0x1000, 0x1000)])
    code = checking_code(CHECKS)
    image[TEXT[2]:TEXT[2] + len(code)] = code
    image[0x67F0] = 0x5A
    image[0x6800:0x8000] = b"\x77" * 0x1800
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    runs = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-empty-load-page-") as directory:
        work = Path(directory)
        for name, offset in (("congruent", 0x6800), ("incongruent", 0x7010)):
            source = work / (name + ".elf")
            source.write_bytes(fixture(offset))
            for mode in ([], ["--lazy-binding"]):
                output = work / (name + ("-lazy" if mode else "") + ".out.elf")
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 0, (name, mode, result.stdout, result.stderr)
                if not runs:
                    continue
                output.chmod(0o755)
                executed = run_without_core_dump([str(output)], capture_output=True, timeout=20)
                assert executed.returncode == -signal.SIGTRAP, (name, mode, executed.returncode, executed.stderr)
    print("Linux empty load page test passed")


if __name__ == "__main__":
    main()
