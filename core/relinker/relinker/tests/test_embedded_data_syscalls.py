from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_sce_code_extent import FUNCTION, FUNCTION_SIZE, LATER_FUNCTION, PT_GNU_EH_FRAME, PT_LOAD, PT_SCE_DYNLIBDATA, fixture
from test_sce_module_code_extent import run_module, sce_module, unwind_table_multiple


def tail(opcode, padding):
    routine = b"\x31\xc0\xb0\x3b" + opcode + b"\xc3" if opcode else b"\x31\xc0\x83\xc0\x01\xc3"
    return b"\x0f\x78\x11\x22\x33\x55" + routine + padding * 4 + b"\x31\xc0\xc3"


TRAILING_TABLE_OFFSET = 0x10000
TRAILING_FUNC_SIZE = TRAILING_TABLE_OFFSET + 0x10
TRAILING_HELPER_DELTA = TRAILING_TABLE_OFFSET + 0x20


def populate_trailing_table(buf, base_offset, func_vaddr, variant):
    table_vaddr = func_vaddr + TRAILING_TABLE_OFFSET
    helper_vaddr = func_vaddr + TRAILING_HELPER_DELTA
    buf[base_offset:base_offset + TRAILING_FUNC_SIZE] = b"\x90" * TRAILING_FUNC_SIZE
    buf[base_offset:base_offset + 3] = b"\x48\x8d\x05"
    struct.pack_into("<i", buf, base_offset + 3, TRAILING_TABLE_OFFSET - 7)
    target_vaddr = func_vaddr + 13
    if variant == "direct-branch":
        target_vaddr = table_vaddr
    elif variant in ("nop-sled", "mid-insn-jmp"):
        target_vaddr = table_vaddr - 4
    buf[base_offset + 7:base_offset + 9] = b"\x0f\x84"
    struct.pack_into("<i", buf, base_offset + 9, target_vaddr - (func_vaddr + 13))
    buf[base_offset + 13:base_offset + 15] = b"\xff\xe0"
    if variant == "mid-insn-jmp":
        buf[base_offset + TRAILING_TABLE_OFFSET - 5:base_offset + TRAILING_TABLE_OFFSET] = b"\xe9\x90\x90\x90\x90"
    elif variant == "call-ret":
        buf[base_offset + TRAILING_TABLE_OFFSET - 5] = 0xe8
        struct.pack_into("<i", buf, base_offset + TRAILING_TABLE_OFFSET - 4, helper_vaddr - table_vaddr)
    struct.pack_into("<ii", buf, base_offset + TRAILING_TABLE_OFFSET, -64241, -TRAILING_TABLE_OFFSET)
    helper_offset = base_offset + TRAILING_HELPER_DELTA
    if variant == "cross-func":
        buf[helper_offset:helper_offset + 2] = b"\x0f\x84"
        struct.pack_into("<iB", buf, helper_offset + 2, table_vaddr - (helper_vaddr + 6), 0xc3)
        buf[helper_offset + 7:helper_offset + 16] = b"\x90" * 9
    else:
        buf[helper_offset:helper_offset + 16] = b"\x31\xc0\xc3" + b"\x90" * 13


def trailing_executable(sce, variant):
    switch_func = 0x1000
    helper_func = switch_func + TRAILING_HELPER_DELTA
    functions = [(FUNCTION, FUNCTION_SIZE), (switch_func, TRAILING_FUNC_SIZE), (helper_func, 0x10)]
    image, plt = fixture(sce_block=sce, functions=functions)
    total_size = switch_func + TRAILING_HELPER_DELTA + 0x100
    image.extend(b"\x90" * (total_size - len(image)))
    image[plt + 0x20:plt + 0x50] = b"\x90" * 0x30
    for ph_offset in (64, 176):
        ph_type = struct.unpack_from("<I", image, ph_offset)[0]
        if ph_type in (PT_LOAD, PT_SCE_DYNLIBDATA):
            struct.pack_into("<QQ", image, ph_offset + 32, total_size, total_size)
    populate_trailing_table(image, switch_func, switch_func, variant)
    return image


def trailing_guest_module(variant):
    text_offset = 0x1000
    text_vaddr = 0x1000
    text_size = TRAILING_HELPER_DELTA + 0x200
    total_size = text_offset + text_size
    image = bytearray(total_size)
    tags = [(0x61000035, 0), (0x61000037, 1), (0x61000039, 0x20), (0x6100003B, 24), (0x6100003F, 24), (0, 0)]
    eh_header = text_vaddr + TRAILING_HELPER_DELTA + 0x80
    eh_frame = eh_header + 0x20
    headers = [
        (PT_LOAD, 5, text_offset, text_vaddr, text_size, text_size, 0x1000),
        (PT_LOAD, 6, 0x600, 0x20000, 0x400, 0x400, 0x100),
        (2, 6, 0x600, 0x20000, len(tags) * 16, len(tags) * 16, 8),
        (PT_SCE_DYNLIBDATA, 4, 0xA00, 0, 0x200, 0, 0x10),
        (PT_GNU_EH_FRAME, 4, text_offset + TRAILING_HELPER_DELTA + 0x80, eh_header, 48, 48, 4),
    ]
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, text_vaddr, 64, 0, 0, 64, 56, len(headers), 64, 0, 0)
    for index, (kind, flags, offset, address, file_size, memory_size, alignment) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, kind, flags, offset, address, address, file_size, memory_size, alignment)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    text = memoryview(image)[text_offset:text_offset + text_size]
    populate_trailing_table(text, 0, text_vaddr, variant)
    unwind_table_multiple(text, text_vaddr, [(text_vaddr, TRAILING_FUNC_SIZE), (text_vaddr + TRAILING_HELPER_DELTA, 0x10)], eh_header, eh_frame)
    return image


def run_executable(relinker, work, name, image, flags, expected_error):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)
    else:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    group = sys.argv[2]
    assert group in ("executable", "guest"), group
    count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-embedded-syscalls-") as directory:
        work = Path(directory)
        for target, flags in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
            for name, opcode in (("syscall", b"\x0f\x05"), ("sysenter", b"\x0f\x34"),
                                 ("int80", b"\xcd\x80"), ("control", b"")):
                for padding in (b"\x00", b"\x90", b"\xcc"):
                    payload = tail(opcode, padding)
                    error = "Forbidden syscall instruction" if opcode else None
                    case = name + "-" + padding.hex() + "-" + target
                    if group == "guest":
                        image = sce_module(gap_embedded_data=True)
                        image[0x420:0x440] = payload + padding * (0x20 - len(payload))
                        run_module(relinker, work, case, image, flags, error)
                        if not opcode:
                            assert payload in (work / case / "app0" / "sce_module" / "libExample.prx.guest.prx").read_bytes(), case
                        count += 1
                    else:
                        for sce in (False, True):
                            image, _ = fixture(sce_block=sce, gap_embedded_data=True)
                            table = FUNCTION + FUNCTION_SIZE + 0x10
                            image[table:LATER_FUNCTION] = payload + padding * (LATER_FUNCTION - table - len(payload))
                            executable_case = ("sce-" if sce else "sysv-") + case
                            run_executable(relinker, work, executable_case, image, flags, error)
                            if not opcode:
                                assert payload in (work / (executable_case + ".out")).read_bytes(), executable_case
                            count += 1
            for variant in ("control", "direct-branch", "nop-sled", "mid-insn-jmp", "call-ret", "cross-func"):
                error = None if variant == "control" else "Forbidden syscall instruction"
                case = "trailing-" + variant + "-" + target
                if group == "guest":
                    run_module(relinker, work, case, trailing_guest_module(variant), flags, error)
                    if variant == "control":
                        assert struct.pack("<ii", -64241, -TRAILING_TABLE_OFFSET) in (work / case / "app0" / "sce_module" / "libExample.prx.guest.prx").read_bytes(), case
                    count += 1
                else:
                    for sce in (False, True):
                        executable_case = ("sce-" if sce else "sysv-") + case
                        run_executable(relinker, work, executable_case, trailing_executable(sce, variant), flags, error)
                        if variant == "control":
                            assert struct.pack("<ii", -64241, -TRAILING_TABLE_OFFSET) in (work / (executable_case + ".out")).read_bytes(), executable_case
                        count += 1
    print("Embedded data syscall boundary tests passed:", group, count, "cases")


if __name__ == "__main__":
    main()
