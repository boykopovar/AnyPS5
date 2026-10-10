"""Name the FreeBSD syscall a forbidden syscall stub reaches, when that can be established."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

CONSTANT = bytes.fromhex("48c7c009000000") + bytes.fromhex("4989ca") + b"\x0f\x05\xc3"
UNSIGNED = bytes.fromhex("b839000000") + b"\x0f\x05\xc3"
INT80 = bytes.fromhex("b80b000000") + b"\xcd\x80\xc3"
REGISTER = bytes.fromhex("4889f8") + b"\x0f\x05\xc3"
SYSENTER = b"\x0f\x34\xc3"
SYSRET = bytes.fromhex("48c7c009000000") + b"\x0f\x07\xc3"
CLOBBERED = bytes.fromhex("48c7c009000000") + bytes.fromhex("4883c001") + b"\x0f\x05\xc3"
PREFIXED_CONSTANT = bytes.fromhex("6648c7c009000000") + bytes.fromhex("f24989ca") + bytes.fromhex("f30f05") + b"\xc3"
PREFIXED_CLOBBERED = bytes.fromhex("6648c7c009000000") + bytes.fromhex("664883c001") + bytes.fromhex("f30f05") + b"\xc3"


def fixture(code):
    """An executable whose first bytes are code, so the instruction scan starts on it."""
    image = bytearray(0x8000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0, 64, 0, 0, 64, 56, 5, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x4000, 0, 0, 0x1000, 0x1000, 0x4000)
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24), (7, 0x700), (8, 0), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4600, 0x600, 0x600, len(tags) * 16, len(tags) * 16, 8)
    for index in (2, 3, 4):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, 0x6FFFFF01, 0, 0, 0, 0, 0, 0, 1)
    image[0x4000:0x4000 + len(code)] = code
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    return bytes(image)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-syscall-") as directory:
        work = Path(directory)

        def reject(name, code):
            source = work / (name + ".elf")
            output = work / (name + ".out")
            source.write_bytes(fixture(code))
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if result.returncode != 2 or "Forbidden syscall instruction at code offset 0x" not in result.stderr:
                raise AssertionError((name, result.returncode, result.stdout, result.stderr))
            if output.exists():
                raise AssertionError((name, "a rejected input still produced an output"))
            return result.stderr

        for name, code, expected in (("constant", CONSTANT, "syscall number 9"),
                                     ("unsigned", UNSIGNED, "syscall number 57"),
                                     ("int80", INT80, "syscall number 11"),
                                     ("prefixed-constant", PREFIXED_CONSTANT, "syscall number 9")):
            if expected not in reject(name, code):
                raise AssertionError((name, "did not report " + expected))

        for name, code in (("register", REGISTER), ("sysenter", SYSENTER),
                           ("sysret", SYSRET), ("clobbered", CLOBBERED),
                           ("prefixed-clobbered", PREFIXED_CLOBBERED)):
            stderr = reject(name, code)
            if "syscall number" in stderr:
                raise AssertionError((name, "reported a number it cannot establish", stderr))
    print("Syscall number diagnostics tests passed")


if __name__ == "__main__":
    main()
