"""Call an unresolved lazy import from ABI-aligned guest code and expect STATUS_ENTRYPOINT_NOT_FOUND."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

NAME = b"NoSuchFunctionXyz"
NEEDED = b"libfoo.prx"
SLOT = 0x900


def fixture():
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x200, 64, 0, 0, 64, 56, 2, 64, 0, 0)
    strings = b"\0" + NAME + b"\0" + NEEDED + b"\0"
    image[0x600:0x600 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", image, 0x620 + 24, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x700, SLOT, (1 << 32) | 6, 0)
    tags = [(1, 2 + len(NAME)), (5, 0x600), (10, len(strings)), (6, 0x620), (11, 24),
            (7, 0x700), (8, 24), (9, 24), (0x6100003f, 48), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    code = bytes.fromhex("48 83 ec 08 ff 15") + struct.pack("<i", SLOT - 0x20a) + bytes.fromhex("48 83 c4 08 b8 07 00 00 00 c3")
    image[0x200:0x200 + len(code)] = code
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-lazy-imports-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        output = work / "output.exe"
        source.write_bytes(fixture())
        result = subprocess.run([str(relinker), "--windows", "--lazy-binding", "--skip-sce-module", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0 and output.read_bytes()[:2] == b"MZ", (result.returncode, result.stdout, result.stderr)
        if os.name == "nt":
            executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20, cwd=work)
            status = executed.returncode & 0xffffffff
            assert status == 0xc0000139, f"status {status:#x}, stderr {executed.stderr!r}"
            assert "FAIL: unresolved ELF import NoSuchFunctionXyz\n" in executed.stderr, executed.stderr
            assert "Transferring control to ELF entry point" in executed.stdout and "Searched libraries" not in executed.stderr, (executed.stdout, executed.stderr)
    print("Windows lazy import tests passed")


if __name__ == "__main__":
    main()
