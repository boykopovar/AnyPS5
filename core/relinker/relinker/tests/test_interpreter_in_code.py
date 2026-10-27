"""Keep an interpreter path at the start of a code segment out of the scan, and only there."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

INTERPRETER = b"/libexec/ld-elf.so.1\x00"
CODE_OFFSET = 0x1000
DATA_ADDRESS = 0x8000
ENTRY = 0x100
TRAILING_ADDRESS = 0xF00


def fixture(interpreter=True, trailing=False, data_flags=4):
    image = bytearray(0x2000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, ENTRY, 64, 0, 0, 64, 56, 4 if trailing else 3, 64, 0, 0)
    image[CODE_OFFSET + ENTRY:CODE_OFFSET + ENTRY + 6] = b"\xb8\x2a\x00\x00\x00\xc3"
    if interpreter:
        image[CODE_OFFSET:CODE_OFFSET + len(INTERPRETER)] = INTERPRETER
    tags = [(5, DATA_ADDRESS + 0x600), (10, 1), (6, DATA_ADDRESS + 0x620), (11, 24),
            (7, DATA_ADDRESS + 0x700), (8, 24), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, data_flags, 0, DATA_ADDRESS, DATA_ADDRESS, CODE_OFFSET, CODE_OFFSET, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     1, 5, CODE_OFFSET, 0, 0, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x400, DATA_ADDRESS + 0x400,
                     DATA_ADDRESS + 0x400, len(tags) * 16, len(tags) * 16, 8)
    if trailing:
        struct.pack_into("<IIQQQQQQ", image, 232, 3, 4, TRAILING_ADDRESS, TRAILING_ADDRESS,
                         TRAILING_ADDRESS, len(INTERPRETER), len(INTERPRETER), 1)
        image[CODE_OFFSET + TRAILING_ADDRESS:CODE_OFFSET + TRAILING_ADDRESS + len(INTERPRETER)] = INTERPRETER
    struct.pack_into("<QQq", image, 0x700, DATA_ADDRESS + 0x300, 8, 0)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-interpreter-") as directory:
        work = Path(directory)
        for target, suffix in (("--windows", ".exe"), ("--macos", ".macho")):
            for name, interpreter, trailing in (("interpreter", True, False), ("plain", False, False), ("trailing", False, True)):
                source = work / (name + target + ".elf")
                output = work / (name + target + suffix)
                source.write_bytes(fixture(interpreter, trailing, 6 if target == "--macos" else 4))
                result = subprocess.run([str(relinker), "--skip-sce-module", target, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                if result.returncode != 0 or not output.exists():
                    raise AssertionError((name, target, result.returncode, result.stdout, result.stderr))
    print("Interpreter path tests passed")


if __name__ == "__main__":
    main()
