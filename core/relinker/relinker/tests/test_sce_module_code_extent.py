from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_symbol_names import importing_executable

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_GNU_EH_FRAME = 0x6474E550
PT_SCE_DYNLIBDATA = 0x61000000
TEXT_OFFSET = 0x1000
TEXT_SIZE = 0x1000
DATA_OFFSET = 0x3000
DATA_ADDRESS = 0x2000
BLOCK_OFFSET = 0x3800
IMAGE_SIZE = 0x4000
INTERP = b"/libexec/ld-elf.so.1\x00"
FS_LOOKALIKE = b"\x64\x2d\x65\x6c\x66\x2e"
FUNCTION = 0x100
FUNCTION_SIZE = 0x10
PLT = 0x110
RODATA = 0x140
GOT = DATA_ADDRESS + 0x100
EH_FRAME_HEADER = 0x800
EH_FRAME = 0x840


def fixture(interp_address=True, rodata_address=True, interp_header=True):
    image = bytearray(IMAGE_SIZE)
    base = BLOCK_OFFSET
    strtab, symtab, jmprel = BLOCK_OFFSET, BLOCK_OFFSET + 0x20, BLOCK_OFFSET + 0x100
    tags = [
        (1, 1),
        (0x61000035, strtab - base), (0x61000037, 16),
        (0x61000039, symtab - base), (0x6100003B, 24), (0x6100003F, 48),
        (0x6100002F, jmprel - base), (0x61000031, 0), (0x61000033, 24),
        (0x61000027, GOT), (0x61000029, jmprel - base), (0x6100002B, 7), (0x6100002D, 24),
        (0, 0),
    ]
    headers = [
        (PT_LOAD, 5, TEXT_OFFSET, 0, TEXT_SIZE, TEXT_SIZE, 0x1000),
        (PT_LOAD, 6, DATA_OFFSET, DATA_ADDRESS, 0x400, 0x400, 0x1000),
        (PT_DYNAMIC, 6, DATA_OFFSET, DATA_ADDRESS, len(tags) * 16, len(tags) * 16, 8),
        (PT_SCE_DYNLIBDATA, 4, BLOCK_OFFSET, 0, 0x800, 0, 0x10),
        (PT_INTERP if interp_header else 0, 4, TEXT_OFFSET, 0, len(INTERP), len(INTERP), 1),
        (PT_GNU_EH_FRAME, 4, TEXT_OFFSET + EH_FRAME_HEADER, EH_FRAME_HEADER, 20, 20, 4),
    ]
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, FUNCTION, 64, 0, 0, 64, 56, len(headers), 64, 0, 0)
    for index, (kind, flags, offset, address, file_size, memory_size, alignment) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, kind, flags, offset, address, address, file_size, memory_size, alignment)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DATA_OFFSET + index * 16, *tag)
    image[strtab:strtab + 16] = b"\x00lib.so\x00symbol\x00\x00"
    struct.pack_into("<IBBHQQ", image, symtab + 24, 8, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, jmprel, GOT + 24, (1 << 32) | 7, 0)

    text = memoryview(image)[TEXT_OFFSET:TEXT_OFFSET + TEXT_SIZE]
    text[:len(INTERP)] = INTERP
    nop7 = b"\x0f\x1f\x80\x00\x00\x00\x00"
    text[FUNCTION:FUNCTION + 7] = b"\x48\x8d\x3d" + struct.pack("<i", 10 - (FUNCTION + 7)) if interp_address else nop7
    text[FUNCTION + 7:FUNCTION + 14] = b"\x48\x8d\x35" + struct.pack("<i", RODATA - (FUNCTION + 14)) if rodata_address else nop7
    text[FUNCTION + 14:FUNCTION + 16] = b"\xc3\x90"
    text[PLT:PLT + 2] = b"\xff\x35"
    struct.pack_into("<i", text, PLT + 2, GOT + 8 - (PLT + 6))
    text[PLT + 6:PLT + 8] = b"\xff\x25"
    struct.pack_into("<i", text, PLT + 8, GOT + 16 - (PLT + 12))
    text[PLT + 12:PLT + 16] = b"\x0f\x1f\x40\x00"
    text[PLT + 16:PLT + 18] = b"\xff\x25"
    struct.pack_into("<i", text, PLT + 18, GOT + 24 - (PLT + 22))
    text[PLT + 22] = 0x68
    struct.pack_into("<I", text, PLT + 23, 0)
    text[PLT + 27] = 0xE9
    struct.pack_into("<i", text, PLT + 28, PLT - (PLT + 32))
    text[RODATA:RODATA + len(FS_LOOKALIKE)] = FS_LOOKALIKE

    cie = EH_FRAME
    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    struct.pack_into("<I", text, cie, len(cie_body))
    text[cie + 4:cie + 4 + len(cie_body)] = cie_body
    fde = cie + 4 + len(cie_body)
    struct.pack_into("<IIiiB3x", text, fde, 16, fde + 4 - cie, FUNCTION - (fde + 8), FUNCTION_SIZE, 0)
    struct.pack_into("<I", text, fde + 20, 0)
    struct.pack_into("<BBBBiIii", text, EH_FRAME_HEADER, 1, 0x1B, 0x03, 0x3B,
                     EH_FRAME - (EH_FRAME_HEADER + 4), 1, FUNCTION - EH_FRAME_HEADER, fde - EH_FRAME_HEADER)
    return image


def unwind_table(text, base, function, size, header, frame):
    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    struct.pack_into("<I", text, frame - base, len(cie_body))
    text[frame - base + 4:frame - base + 4 + len(cie_body)] = cie_body
    fde = frame + 4 + len(cie_body)
    struct.pack_into("<IIiiB3x", text, fde - base, 16, fde + 4 - frame, function - (fde + 8), size, 0)
    struct.pack_into("<I", text, fde - base + 20, 0)
    struct.pack_into("<BBBBiIii", text, header - base, 1, 0x1B, 0x03, 0x3B,
                     frame - (header + 4), 1, function - header, fde - header)


def unwind_table_multiple(text, base, functions, header, frame):
    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    struct.pack_into("<I", text, frame - base, len(cie_body))
    text[frame - base + 4:frame - base + 4 + len(cie_body)] = cie_body
    fde = frame + 4 + len(cie_body)
    struct.pack_into("<BBBBiI", text, header - base, 1, 0x1B, 0x03, 0x3B,
                     frame - (header + 4), len(functions))
    for index, (func_addr, func_size) in enumerate(functions):
        struct.pack_into("<IIiiB3x", text, fde - base, 16, fde + 4 - frame, func_addr - (fde + 8), func_size, 0)
        struct.pack_into("<ii", text, header - base + 12 + index * 8, func_addr - header, fde - header)
        fde += 20
    struct.pack_into("<I", text, fde - base, 0)


def sce_module(gap_embedded_data=False, gap_data_then_syscall=False, gap_callback_syscall_undecodable=False,
               gap_decoded_padding_syscall=False, gap_stub_then_data=False):
    image = bytearray(0xC00)
    tags = [(0x61000035, 0), (0x61000037, 1), (0x61000039, 0x20), (0x6100003B, 24), (0x6100003F, 24), (0, 0)]
    has_gap = gap_embedded_data or gap_data_then_syscall or gap_callback_syscall_undecodable or gap_decoded_padding_syscall or gap_stub_then_data
    headers = [
        (PT_LOAD, 5, 0x400, 0x1000, 0x100, 0x100, 0x100),
        (PT_LOAD, 6, 0x600, 0x2000, 0x400, 0x400, 0x100),
        (PT_DYNAMIC, 6, 0x600, 0x2000, len(tags) * 16, len(tags) * 16, 8),
        (PT_SCE_DYNLIBDATA, 4, 0xA00, 0, 0x200, 0, 0x10),
        (PT_GNU_EH_FRAME, 4, 0x480, 0x1080, 48 if has_gap else 20, 48 if has_gap else 20, 4),
    ]
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x1000, 64, 0, 0, 64, 56, len(headers), 64, 0, 0)
    for index, (kind, flags, offset, address, file_size, memory_size, alignment) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, kind, flags, offset, address, address, file_size, memory_size, alignment)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    text = memoryview(image)[0x400:0x500]
    if has_gap:
        text[0:8] = b"\x48\x8d\x05\x19\x00\x00\x00\xc3"
        table = 0x20
        if gap_data_then_syscall:
            text[table:table + 6] = b"\x0f\x78\x11\x22\x33\x44"
            text[table + 6:table + 16] = b"\xcc" * 10
            text[table + 16:table + 24] = b"\xb8\x3b\x00\x00\x00\x0f\x05\xc3"
            text[table + 24:0x40] = b"\x90" * 8
        elif gap_callback_syscall_undecodable:
            text[table:table + 14] = b"\xb8\x3b\x00\x00\x00\x0f\x05\x0f\x78\x11\x22\x33\x44\xc3"
            text[table + 14:0x40] = b"\x90" * 18
        elif gap_decoded_padding_syscall:
            text[table:table + 13] = b"\x0f\x78\xcc\xb8\x09\x00\x00\x00\x0f\x05\x00\x01\x90"
            text[table + 13:0x40] = b"\x90" * (0x40 - (table + 13))
        elif gap_stub_then_data:
            text[table:table + 15] = b"\x0f\x78\xcc\xb8\x09\x00\x00\x00\x0f\x05\xc3\x0f\x78\x33\x1e"
            text[table + 15:0x40] = b"\x90" * (0x40 - (table + 15))
        else:
            text[table:0x40] = b"\x0f\x78\x33\x1e" * 8
        text[0x40:0x48] = b"\xc3" + b"\x90" * 7
        unwind_table_multiple(text, 0x1000, [(0x1000, 8), (0x1040, 8)], 0x1080, 0x10A0)
    else:
        text[0:FUNCTION_SIZE] = b"\xc3" + b"\x90" * (FUNCTION_SIZE - 1)
        text[0x20:0x22] = b"\x0f\x05"
        unwind_table(text, 0x1000, 0x1000, FUNCTION_SIZE, 0x1080, 0x10A0)
    return image


def run_module(relinker, work, name, module, flags, expected_error=None):
    case = work / name
    (case / "sce_module").mkdir(parents=True)
    (case / "input.elf").write_bytes(importing_executable([]))
    (case / "sce_module" / "libExample.prx").write_bytes(module)
    result = subprocess.run([str(relinker), *flags, str(case / "input.elf"), str(case / "output.exe")],
                            capture_output=True, text=True, timeout=30)
    if expected_error is None:
        assert result.returncode == 0 and (case / "app0" / "sce_module" / "libExample.prx.guest.prx").exists(), (
            name, result.stdout, result.stderr)
    else:
        assert result.returncode == 2 and expected_error in result.stderr, (
            name, result.returncode, result.stdout, result.stderr)


def run(relinker, work, name, image, flags, expected_error=None):
    source = work / (name + ".elf")
    output = work / (name + ".exe")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error is None:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
    else:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-module-code-extent-") as directory:
        work = Path(directory)
        for system, flags in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
            run(relinker, work, "sce-loader-path-without-interp-header-" + system, fixture(rodata_address=False, interp_header=False), flags)
            run_module(relinker, work, "sce-module-data-after-code-" + system, sce_module(), flags)
            run_module(relinker, work, "sce-module-gap-embedded-data-" + system, sce_module(gap_embedded_data=True), flags)
            run_module(relinker, work, "sce-module-gap-data-then-syscall-" + system, sce_module(gap_data_then_syscall=True), flags, "Forbidden syscall instruction")
            run_module(relinker, work, "sce-module-gap-callback-syscall-" + system, sce_module(gap_callback_syscall_undecodable=True), flags, "Forbidden syscall instruction")
            run_module(relinker, work, "sce-module-gap-decoded-padding-syscall-" + system, sce_module(gap_decoded_padding_syscall=True), flags, "Forbidden syscall instruction")
            run_module(relinker, work, "sce-module-gap-stub-then-data-" + system, sce_module(gap_stub_then_data=True), flags, "Forbidden syscall instruction")
    print("SCE module code extent tests passed")


if __name__ == "__main__":
    main()
