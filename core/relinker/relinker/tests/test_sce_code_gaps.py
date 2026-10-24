from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

from test_guest_symbol_names import importing_executable
from test_sce_code_extent import FUNCTION, FUNCTION_SIZE, fixture
from test_sce_module_code_extent import sce_module, unwind_table

SLOT = bytes.fromhex("48 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00")
SYSCALL = bytes.fromhex("0f 05 c3")
SYSCALL_NUMBER = bytes.fromhex("b8 14 00 00 00")
TABLE = bytes.fromhex("0c 05 00 00 0f 05 00 b0 0f 05 00 00 10 05 00 50")
GAP = FUNCTION + FUNCTION_SIZE
TARGETS = (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"]))
SKIPPED = "Skipped %d syscall byte pattern(s) without a syscall number outside the unwind functions, the first at code offset %#x"
MOV_EAX_SYSCALL = bytes.fromhex("b8 3b 00 00 00 0f 05 c3")
UNDECODABLE = bytes.fromhex("0f 78 11 22 33 44")
LOADED_NUMBERS = (
    ("mov", MOV_EAX_SYSCALL, 0x215, 59),
    ("rex-after-undecodable", UNDECODABLE + MOV_EAX_SYSCALL, 0x21B, 59),
    ("compare-after-undecodable", UNDECODABLE + b"\x3c" + MOV_EAX_SYSCALL, 0x21C, 59),
    ("padding-after-undecodable", UNDECODABLE + b"\xcc\xcc" + MOV_EAX_SYSCALL, 0x21D, 59),
    ("register-move", bytes.fromhex("b8 3b 00 00 00 48 89 df 0f 05 c3"), 0x218, 59),
    ("zeroed", bytes.fromhex("31 c0 0f 05 c3"), 0x212, 0),
    ("mov-rax-imm64", bytes.fromhex("48 b8 3b 00 00 00 00 00 00 00 0f 05 c3"), 0x21A, 59),
    ("low-byte-after-zeroing", bytes.fromhex("31 c0 b0 3b 0f 05 c3"), 0x214, 59),
    ("low-word-after-zeroing", bytes.fromhex("31 c0 66 b8 3b 01 0f 05 c3"), 0x216, 0x13B),
    ("eight-register-moves", bytes.fromhex("b8 3b 00 00 00") + bytes.fromhex("89 db") * 8 + SYSCALL, 0x225, 59),
)
JUMP_TABLE_DISPATCH = bytes.fromhex("48 8d 0d 09 00 00 00 48 63 04 b9 48 01 c8 ff e0")


def executable(gap_bytes=b"", sce_block=True, function=b"", before_function=b""):
    image, plt = fixture(sce_block=sce_block, gap=0x20)
    image[GAP:GAP + len(gap_bytes)] = gap_bytes
    image[FUNCTION:FUNCTION + len(function)] = function
    image[FUNCTION - len(before_function):FUNCTION] = before_function
    return image, plt


def nested_executable():
    image, plt = fixture(gap=0x20, functions=[(0x180, 0x80), (0x190, 0x8), (FUNCTION, FUNCTION_SIZE)])
    image[0x180:FUNCTION] = b"\x90" * (FUNCTION - 0x180)
    image[0x1C0:0x1C5] = bytes.fromhex("89 f8 0f 05 c3")
    return image, plt


def jump_table_executable():
    image, plt = fixture(gap=0x30, functions=[(FUNCTION, FUNCTION_SIZE), (0x230, FUNCTION_SIZE)])
    image[FUNCTION:GAP] = JUMP_TABLE_DISPATCH
    image[GAP:GAP + 16] = struct.pack("<4i", -16, -16, -16, -16)
    image[GAP + 16:GAP + 16 + len(MOV_EAX_SYSCALL)] = MOV_EAX_SYSCALL
    return image, plt


def ps5_module():
    image = sce_module()
    struct.pack_into("<I", image, 64 + 3 * 56, 0)
    for index, tag in enumerate([(5, 0x2200), (10, 1), (6, 0x2220), (11, 24), (4, 0x2240), (0, 0)]):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<II", image, 0x840, 1, 1)
    return image


def module(slot_in_function):
    image = sce_module()
    text = memoryview(image)[0x400:0x500]
    text[0:len(SLOT)] = SLOT
    if not slot_in_function:
        text[0x10:0x10 + FUNCTION_SIZE] = b"\xc3" + b"\x90" * (FUNCTION_SIZE - 1)
        unwind_table(text, 0x1000, 0x1010, FUNCTION_SIZE, 0x1080, 0x10A0)
        struct.pack_into("<Q", image, 24, 0x1010)
    return image


def finish(name, result, converted, expected_errors):
    if not expected_errors:
        assert result.returncode == 0 and converted, (name, result.stdout, result.stderr)
    else:
        assert result.returncode == 2 and not converted and all(error in result.stderr for error in expected_errors), (
            name, result.returncode, result.stdout, result.stderr)


def run(relinker, work, name, built, flags, expected_errors=(), expected_output=None):
    image, plt = built
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, "--registry", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    finish(name, result, output.exists(), expected_errors)
    assert expected_output is None or expected_output in result.stdout, (name, result.stdout)
    if not expected_errors:
        registry = json.loads((work / (name + ".registry.json")).read_text())
        assert [entry["callSites"] for entry in registry] == [[hex(plt + 16)]], (name, registry)


def run_module(relinker, work, name, built, flags, expected_errors=()):
    case = work / name
    (case / "sce_module").mkdir(parents=True)
    (case / "input.elf").write_bytes(importing_executable([]))
    (case / "sce_module" / "libExample.prx").write_bytes(built)
    result = subprocess.run([str(relinker), *flags, str(case / "input.elf"), str(case / "output.exe")],
                            capture_output=True, text=True, timeout=30)
    finish(name, result, (case / "app0" / "sce_module" / "libExample.prx.guest.prx").exists(), expected_errors)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-code-gaps-") as directory:
        work = Path(directory)
        for target, flags in TARGETS:
            run(relinker, work, "sce-gap-slot-" + target, executable(SLOT), flags)
            run_module(relinker, work, "sce-module-gap-slot-" + target, module(False), flags)
            run(relinker, work, "sce-nested-function-syscall-" + target, nested_executable(), flags,
                ["Forbidden syscall instruction at code offset 0x1c2"])
            run(relinker, work, "sce-gap-syscall-after-jump-table-" + target, jump_table_executable(), flags,
                ["Forbidden syscall instruction at code offset 0x225: syscall number 59"])
            for name, gap_bytes, offset, number in LOADED_NUMBERS:
                run(relinker, work, "sce-gap-syscall-number-" + name + "-" + target, executable(gap_bytes), flags,
                    ["Forbidden syscall instruction at code offset %#x: syscall number %d" % (offset, number)])
        run(relinker, work, "sce-gap-syscall-bytes-after-slot", executable(SLOT + SYSCALL), [],
            expected_output=SKIPPED % (1, 0x220))
        run(relinker, work, "sce-gap-table-with-syscall-bytes", executable(TABLE), [], expected_output=SKIPPED % (1, 0x214))
        run(relinker, work, "sce-gap-syscall-bytes-at-function-end", executable(SYSCALL), [], expected_output=SKIPPED % (1, GAP))
        run(relinker, work, "sce-gap-sysret-bytes", executable(SLOT + bytes.fromhex("0f 07")), [],
            expected_output=SKIPPED % (1, 0x220))
        run(relinker, work, "sce-gap-syscall-number-overwritten", executable(bytes.fromhex("b8 3b 00 00 00 48 89 f8 0f 05 c3")), [],
            expected_output=SKIPPED % (1, 0x218))
        run(relinker, work, "sce-gap-syscall-low-byte-only", executable(bytes.fromhex("b0 3b 0f 05 c3")), [],
            expected_output=SKIPPED % (1, 0x212))
        run(relinker, work, "sce-gap-syscall-after-overlong-instruction", executable(SYSCALL_NUMBER + b"\x41" * 15 + SYSCALL), [],
            expected_output=SKIPPED % (1, 0x217))
        run_module(relinker, work, "ps5-module-gap-syscall-bytes", ps5_module(), [],
                   ["Forbidden syscall instruction at code offset 0x1020"])
        run(relinker, work, "sce-gap-syscall-with-number", executable(SLOT + SYSCALL_NUMBER + SYSCALL), [],
            ["Forbidden syscall instruction at code offset 0x225: syscall number 20"])
        run(relinker, work, "sysv-gap-syscall-bytes", executable(bytes.fromhex("90") * 16 + SYSCALL, sce_block=False), [],
            ["Forbidden syscall instruction at code offset 0x220"])
        run(relinker, work, "sce-function-slot", executable(function=SLOT), [],
            ["Cannot decode instruction", "offset 0x200"])
        run(relinker, work, "sysv-gap-slot", executable(SLOT, sce_block=False), [],
            ["Cannot decode instruction", "offset 0x210"])
        run(relinker, work, "sce-gap-instruction-into-function", executable(function=SYSCALL, before_function=b"\x90" * 15 + b"\xb8"), [],
            ["Forbidden syscall instruction at code offset 0x200"])
        run_module(relinker, work, "sce-module-function-slot", module(True), [], ["Cannot decode instruction"])
    print("SCE code gap tests passed")


if __name__ == "__main__":
    main()
