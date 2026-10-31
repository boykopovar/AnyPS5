from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import guest_fixture
from test_guest_symbol_names import importing_executable

PT_LOAD = 1
PT_GNU_EH_FRAME = 0x6474E550
TEXT = 0x1000
TEXT_OFFSET = 0x400
UNWIND = 0x3000
UNWIND_OFFSET = 0xA00
FIRST = TEXT
TABLE = TEXT + 0x20
CALLBACK = TEXT + 0x40
SECOND = TEXT + 0x50
GS_LOOKALIKE = bytes.fromhex("65 48 8b 00 11 22 33 44 c3")
UNDECODABLE = bytes.fromhex("0f 78 33 1e 77 0f 0f 3c")
GS_CALLBACK = bytes.fromhex("65 48 8b 00 c3")
GS_TRAP = bytes.fromhex("0f 0b 90 90")


def unwind(image, functions):
    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    frame = UNWIND + 0x20
    data = memoryview(image)[UNWIND_OFFSET:UNWIND_OFFSET + 0x100]
    struct.pack_into("<I", data, frame - UNWIND, len(cie_body))
    data[frame - UNWIND + 4:frame - UNWIND + 4 + len(cie_body)] = cie_body
    fdes = []
    for index, (function, size) in enumerate(functions):
        fde = frame + 4 + len(cie_body) + index * 20
        struct.pack_into("<IIiiB3x", data, fde - UNWIND, 16, fde + 4 - frame, function - (fde + 8), size, 0)
        fdes.append((function, fde))
    struct.pack_into("<I", data, fdes[-1][1] + 20 - UNWIND, 0)
    struct.pack_into("<BBBBiI", data, 0, 1, 0x1B, 0x03, 0x3B, frame - (UNWIND + 4), len(fdes))
    for index, (function, fde) in enumerate(fdes):
        struct.pack_into("<ii", data, 12 + index * 8, function - UNWIND, fde - UNWIND)
    return 12 + len(fdes) * 8


def first_function():
    code = b"\x48\x8d\x2d" + struct.pack("<i", TABLE - (FIRST + 7))
    code += bytes.fromhex("8b 45 00")
    code += b"\x48\x8d\x3d" + struct.pack("<i", CALLBACK - (FIRST + 17))
    code += b"\xe8" + struct.pack("<i", SECOND - (FIRST + 22))
    return code + b"\xc3"


def module(table):
    image = guest_fixture(b"")
    image.extend(bytes(0x100))
    struct.pack_into("<H", image, 56, 5)
    header_size = unwind(image, [(FIRST, 0x18), (SECOND, 0x8)])
    struct.pack_into("<IIQQQQQQ", image, 64 + 3 * 56, PT_LOAD, 4, UNWIND_OFFSET, UNWIND, UNWIND, 0x100, 0x100, 0x100)
    struct.pack_into("<IIQQQQQQ", image, 64 + 4 * 56, PT_GNU_EH_FRAME, 4, UNWIND_OFFSET, UNWIND, UNWIND, header_size, header_size, 4)
    text = memoryview(image)[TEXT_OFFSET:TEXT_OFFSET + 0x100]
    text[:] = b"\xcc" * 0x100
    code = first_function()
    text[FIRST - TEXT:FIRST - TEXT + len(code)] = code
    text[TABLE - TEXT:TABLE - TEXT + len(table)] = table
    text[CALLBACK - TEXT:CALLBACK - TEXT + len(GS_CALLBACK)] = GS_CALLBACK
    text[SECOND - TEXT] = 0xC3
    return image


def convert(relinker, work, name, image, flags):
    case = work / name
    (case / "sce_module").mkdir(parents=True)
    (case / "input.elf").write_bytes(importing_executable([]))
    (case / "sce_module" / "libExample.prx").write_bytes(image)
    result = subprocess.run([str(relinker), *flags, "--skip-syscall-check", str(case / "input.elf"), str(case / "output.exe")],
                            capture_output=True, text=True, timeout=30)
    converted = case / "app0" / "sce_module" / "libExample.prx.guest.prx"
    assert result.returncode == 0 and converted.exists(), (name, result.stdout, result.stderr)
    output = converted.read_bytes()
    start = output.find(first_function())
    assert start >= 0, name
    return output[start:start + SECOND - FIRST]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-code-analysis-address-data-") as directory:
        work = Path(directory)
        for system, flags in (("windows", ["--windows"]), ("macos", ["--macos"])):
            code = convert(relinker, work, "table-read-through-register-" + system, module(GS_LOOKALIKE), flags)
            assert code[TABLE - FIRST:TABLE - FIRST + len(GS_LOOKALIKE)] == GS_LOOKALIKE, (system, code.hex())
            assert code[CALLBACK - FIRST:CALLBACK - FIRST + len(GS_TRAP)] == GS_TRAP, (system, code.hex())
            code = convert(relinker, work, "undecodable-table-" + system, module(UNDECODABLE), flags)
            assert code[TABLE - FIRST:TABLE - FIRST + len(UNDECODABLE)] == UNDECODABLE, (system, code.hex())
    print("Code analysis address data tests passed")


if __name__ == "__main__":
    main()
