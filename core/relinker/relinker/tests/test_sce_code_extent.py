from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_GNU_EH_FRAME = 0x6474E550
PT_SCE_DYNLIBDATA = 0x61000000
PT_SCE_VERSION = 0x6FFFFF01
FUNCTION = 0x200
FUNCTION_SIZE = 0x10
EARLY_FUNCTION = 0x160
PLT_BEFORE_CODE = 0x180
GOT = 0xA00
EH_FRAME_HEADER = 0x800
EH_FRAME = 0x840
POINTER = 0xB00
INTERPRETER = b"/libexec/ld-elf.so.1\0"
OVERLAPPING = bytes.fromhex("b8 64 c3 90 90 72 fa c3")
UNSUPPORTED_TLS = bytes.fromhex("64 2d 65 6c 66 2e c3")


def fixture(sce_block=True, plt_push_offset=8, gap=0, interpreter_pointer=False, data_pointer=False, gap_entry=False,
            plt_at=None, early_function=False, function_tls=False):
    plt = FUNCTION + FUNCTION_SIZE + gap if plt_at is None else plt_at
    entry = FUNCTION + FUNCTION_SIZE if gap_entry else FUNCTION
    data = plt + 0x20 if plt > FUNCTION else FUNCTION + FUNCTION_SIZE
    functions = [(EARLY_FUNCTION, FUNCTION_SIZE)] if early_function else []
    functions.append((FUNCTION, FUNCTION_SIZE))
    interpreter = FUNCTION - len(INTERPRETER)
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, entry, 64, 0, 0, 64, 56,
                     5 if interpreter_pointer else 4, 64, 0, 0)
    tags = [
        (1, 1),
        (0x61000035, 0x600), (0x61000037, 16),
        (0x61000039, 0x620), (0x6100003B, 24), (0x6100003F, 48),
        (0x6100002F, 0x700), (0x61000031, 24 if interpreter_pointer else 0), (0x61000033, 24),
        (0x61000027, GOT), (0x61000029, 0x720), (0x6100002B, 7), (0x6100002D, 24),
        (0, 0),
    ]
    struct.pack_into("<IIQQQQQQ", image, 64, PT_LOAD, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, PT_DYNAMIC, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     *((PT_SCE_DYNLIBDATA, 0, 0, 0, 0, len(image), len(image), 1) if sce_block
                       else (PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)))
    header_size = 12 + 8 * len(functions)
    struct.pack_into("<IIQQQQQQ", image, 232, PT_GNU_EH_FRAME, 4, EH_FRAME_HEADER, EH_FRAME_HEADER, EH_FRAME_HEADER,
                     header_size, header_size, 4)
    if interpreter_pointer:
        struct.pack_into("<IIQQQQQQ", image, 288, PT_INTERP, 4, interpreter, interpreter, interpreter,
                         len(INTERPRETER), len(INTERPRETER), 1)
        image[interpreter:FUNCTION] = INTERPRETER
        struct.pack_into("<QQq", image, 0x700, POINTER, 8, interpreter)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"
    struct.pack_into("<IBBHQQ", image, 0x620 + 24, 8, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x720, GOT + 24, (1 << 32) | 7, 0)

    for begin, size in functions:
        image[begin:begin + size] = b"\xc3" + b"\x90" * (size - 1)
    image[FUNCTION + FUNCTION_SIZE:plt] = b"\x90" * gap
    image[plt:plt + 2] = b"\xff\x35"
    struct.pack_into("<i", image, plt + 2, GOT + plt_push_offset - (plt + 6))
    image[plt + 6:plt + 8] = b"\xff\x25"
    struct.pack_into("<i", image, plt + 8, GOT + 16 - (plt + 12))
    image[plt + 12:plt + 16] = b"\x0f\x1f\x40\x00"
    image[plt + 16:plt + 18] = b"\xff\x25"
    struct.pack_into("<i", image, plt + 18, GOT + 24 - (plt + 22))
    image[plt + 22] = 0x68
    struct.pack_into("<I", image, plt + 23, 0)
    image[plt + 27] = 0xE9
    struct.pack_into("<i", image, plt + 28, plt - (plt + 32))
    image[data:data + 0x30] = b"\x0f\x79\xc0" * 0x10
    if data_pointer:
        image[FUNCTION:FUNCTION + 3] = b"\x48\x8d\x05"
        struct.pack_into("<iB", image, FUNCTION + 3, data + 0x30 - (FUNCTION + 7), 0xC3)
        image[data + 0x30:data + 0x30 + len(OVERLAPPING)] = OVERLAPPING
    if gap_entry:
        image[entry:entry + len(UNSUPPORTED_TLS)] = UNSUPPORTED_TLS
    if function_tls:
        image[FUNCTION:FUNCTION + len(UNSUPPORTED_TLS)] = UNSUPPORTED_TLS

    cie = EH_FRAME
    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    struct.pack_into("<I", image, cie, len(cie_body))
    image[cie + 4:cie + 4 + len(cie_body)] = cie_body
    fde = cie + 4 + len(cie_body)
    struct.pack_into("<BBBBiI", image, EH_FRAME_HEADER, 1, 0x1B, 0x03, 0x3B,
                     EH_FRAME - (EH_FRAME_HEADER + 4), len(functions))
    for index, (begin, size) in enumerate(functions):
        struct.pack_into("<IIiiB3x", image, fde, 16, fde + 4 - cie, begin - (fde + 8), size, 0)
        struct.pack_into("<ii", image, EH_FRAME_HEADER + 12 + index * 8, begin - EH_FRAME_HEADER, fde - EH_FRAME_HEADER)
        fde += 20
    struct.pack_into("<I", image, fde, 0)
    return image, plt


def run(relinker, work, name, built, expected_error=None, target="windows"):
    image, plt = built
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--" + target, "--registry", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error is None:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
        registry = json.loads((work / (name + ".registry.json")).read_text())
        assert [[site.lower() for site in entry["callSites"]] for entry in registry] == [[hex(plt + 16)]], (name, registry)
    else:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-code-extent-") as directory:
        work = Path(directory)
        run(relinker, work, "sce-data-after-plt", fixture())
        run(relinker, work, "sce-code-between-function-and-plt", fixture(gap=0x40))
        run(relinker, work, "sce-plt-missing", fixture(plt_push_offset=0),
            "PLT not found before or after the unwind functions")
        run(relinker, work, "sce-plt-ends-at-function", fixture(plt_at=FUNCTION - 0x20))
        run(relinker, work, "sce-plt-starts-at-function-end",
            fixture(plt_at=EARLY_FUNCTION + FUNCTION_SIZE, early_function=True))
        run(relinker, work, "sce-plt-over-function", fixture(plt_at=FUNCTION), "PLT overlaps an unwind function")
        run(relinker, work, "sce-plt-over-function-end", fixture(plt_at=FUNCTION + FUNCTION_SIZE - 8),
            "PLT overlaps an unwind function")
        run(relinker, work, "sysv-data-after-plt", fixture(sce_block=False),
            "Cannot decode instruction")
        for target in ("windows", "macos"):
            run(relinker, work, "sce-interpreter-pointer-" + target, fixture(gap=0x60, interpreter_pointer=True),
                target=target)
            run(relinker, work, "sce-data-pointer-after-plt-" + target, fixture(data_pointer=True), target=target)
            run(relinker, work, "sce-code-before-plt-" + target, fixture(gap=0x40, gap_entry=True),
                "Unsupported " + ("Windows" if target == "windows" else "macOS") + " guest TLS instruction", target)
            run(relinker, work, "sce-plt-before-code-" + target, fixture(plt_at=PLT_BEFORE_CODE), target=target)
            run(relinker, work, "sce-data-pointer-after-code-" + target, fixture(plt_at=PLT_BEFORE_CODE, data_pointer=True),
                target=target)
            run(relinker, work, "sce-entry-after-code-" + target, fixture(plt_at=PLT_BEFORE_CODE, gap_entry=True),
                target=target)
            run(relinker, work, "sce-function-before-plt-" + target,
                fixture(plt_at=PLT_BEFORE_CODE, early_function=True), target=target)
            run(relinker, work, "sce-code-after-plt-" + target, fixture(plt_at=PLT_BEFORE_CODE, function_tls=True),
                "Unsupported " + ("Windows" if target == "windows" else "macOS") + " guest TLS instruction", target)
    print("SCE code extent tests passed")


if __name__ == "__main__":
    main()
