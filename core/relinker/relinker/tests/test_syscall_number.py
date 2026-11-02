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
SIGN_EXTENDED = bytes.fromhex("48c7c0ffffffff") + bytes.fromhex("4989ca") + b"\x0f\x05\xc3"
IMM64 = bytes.fromhex("48b80900000000000000") + bytes.fromhex("4989ca") + b"\x0f\x05\xc3"
IMM64_NEGATIVE = bytes.fromhex("48b8ffffffffffffffff") + bytes.fromhex("4989ca") + b"\x0f\x05\xc3"
ZEROED = bytes.fromhex("31c0") + b"\x0f\x05\xc3"
WIDE_ZEROED = bytes.fromhex("4831c0") + b"\x0f\x05\xc3"
WORD_ZEROED = bytes.fromhex("6631c0") + b"\x0f\x05\xc3"
EXTENDED_REGISTER_ZEROED = bytes.fromhex("4531c0") + b"\x0f\x05\xc3"
EXTENDED_SOURCE_XOR = bytes.fromhex("4431c0") + b"\x0f\x05\xc3"
EXTENDED_INDEX_XOR = bytes.fromhex("4231c0") + b"\x0f\x05\xc3"
LOAD_THEN_EXTENDED_ZEROED = bytes.fromhex("b83b0000004731c0") + b"\x0f\x05\xc3"
LOAD_THEN_EXTENDED_XOR = bytes.fromhex("b83b0000004531c0") + b"\x0f\x05\xc3"
LOAD_THEN_EXTENDED_MOVE = bytes.fromhex("b83b0000004589c0") + b"\x0f\x05\xc3"
LOAD_THEN_EXTENDED_TO_R9 = bytes.fromhex("b83b0000004d89c1") + b"\x0f\x05\xc3"
PREFIX_AWARE_LOAD = bytes.fromhex("44b83b000000") + b"\x0f\x05\xc3"
MPROTECT_TAIL = bytes.fromhex("b84a0000004889f74c89c64531d24531c04531c9") + b"\x0f\x05\xc3"
WORD_WIDE_ZEROED = bytes.fromhex("664831c0") + b"\x0f\x05\xc3"
EXTENDED_LOAD = bytes.fromhex("41b83b000000") + b"\x0f\x05\xc3"
LOAD_THEN_EXTENDED_LOW_BYTE = bytes.fromhex("b83b00000041b005") + b"\x0f\x05\xc3"
LOAD_THEN_MEMORY_LOAD = bytes.fromhex("b83b0000008b448bf8") + b"\x0f\x05\xc3"
LOAD_THEN_IMMEDIATE_ADD = bytes.fromhex("b83b00000005000031c9") + b"\x0f\x05\xc3"


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
                                     ("prefixed-constant", PREFIXED_CONSTANT, "syscall number 9"),
                                     ("sign-extended", SIGN_EXTENDED, "syscall number 18446744073709551615"),
                                     ("imm64", IMM64, "syscall number 9"),
                                     ("imm64-negative", IMM64_NEGATIVE, "syscall number 18446744073709551615"),
                                     ("zeroed", ZEROED, "syscall number 0"),
                                     ("wide-zeroed", WIDE_ZEROED, "syscall number 0"),
                                     ("extended-index-xor", EXTENDED_INDEX_XOR, "syscall number 0"),
                                     ("load-then-extended-zeroed", LOAD_THEN_EXTENDED_ZEROED, "syscall number 59"),
                                     ("load-then-extended-xor", LOAD_THEN_EXTENDED_XOR, "syscall number 59"),
                                     ("load-then-extended-move", LOAD_THEN_EXTENDED_MOVE, "syscall number 59"),
                                     ("load-then-extended-to-r9", LOAD_THEN_EXTENDED_TO_R9, "syscall number 59"),
                                     ("prefix-aware-load", PREFIX_AWARE_LOAD, "syscall number 59"),
                                     ("mprotect-tail", MPROTECT_TAIL, "syscall number 74")):
            if expected not in reject(name, code):
                raise AssertionError((name, "did not report " + expected))

        for name, code in (("register", REGISTER), ("sysenter", SYSENTER),
                           ("sysret", SYSRET), ("clobbered", CLOBBERED),
                           ("prefixed-clobbered", PREFIXED_CLOBBERED),
                           ("word-zeroed", WORD_ZEROED),
                           ("word-wide-zeroed", WORD_WIDE_ZEROED),
                           ("extended-register-zeroed", EXTENDED_REGISTER_ZEROED),
                           ("extended-source-xor", EXTENDED_SOURCE_XOR),
                           ("extended-load", EXTENDED_LOAD),
                           ("load-then-extended-low-byte", LOAD_THEN_EXTENDED_LOW_BYTE),
                           ("load-then-memory-load", LOAD_THEN_MEMORY_LOAD),
                           ("load-then-immediate-add", LOAD_THEN_IMMEDIATE_ADD)):
            stderr = reject(name, code)
            if "syscall number" in stderr:
                raise AssertionError((name, "reported a number it cannot establish", stderr))
    print("Syscall number diagnostics tests passed")


if __name__ == "__main__":
    main()
