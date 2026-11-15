import os
from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import dynamic, executable
from signal_run import run_without_core_dump
from test_guest_symbol_names import dynamic_tags, elf_loads, file_offset
from test_optional_plt import fixture
from test_windows_import_modules import provider


SYSTEMS = (("linux", [], b"\x7fELF"),
           ("linux-pie", ["--lazy-binding"], b"\x7fELF"),
           ("windows", ["--windows"], b"MZ"),
           ("macos", ["--macos"], b"\xcf\xfa\xed\xfe"))


def main_image(words=(0x800,), mixed=False):
    image = fixture()
    image[0x200:0x206] = b"\xff\x25" + struct.pack("<i", 0x800 - 0x206)
    struct.pack_into("<Q", image, 64 + 40, 0x2000)
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24),
            (36, 0x700), (35, len(words) * 8), (37, 8)]
    if mixed:
        tags.extend(((7, 0x740), (8, 24), (9, 24)))
        struct.pack_into("<QQq", image, 0x740, 0x850, 8, 0x210)
    dynamic(image, 0x400, 120, tags)
    for index, word in enumerate(words):
        struct.pack_into("<Q", image, 0x700 + index * 8, word)
    for target in (0x800, 0x808, 0x9f8, 0xa00):
        struct.pack_into("<Q", image, target, 0x210)
    return image


def guest_image(words=(0x2320, 5), mixed=False):
    code = bytearray(b"\x90" * 0x4b)
    code[:6] = b"\xff\x25" + struct.pack("<i", 0x2320 - 0x1006)
    code[0x20:0x2a] = b"\x8b\x05" + struct.pack("<i", 0x2340 - 0x1026) + b"\x83\xc0\x04\xc3"
    code[0x40:0x4b] = b"\xc7\x05" + struct.pack("<i", 0x2340 - 0x104a) + struct.pack("<I", 7) + b"\xc3"
    image = provider(11, code=code)
    tags = [(kind, value) for kind, value in dynamic_tags(image).items() if kind not in (7, 8, 9)]
    tags.extend(((36, 0x2380), (35, len(words) * 8), (37, 8), (25, 0x2330), (27, 8)))
    if mixed:
        tags.extend(((7, 0x2300), (8, 24), (9, 24)))
        struct.pack_into("<QQq", image, 0x900, 0x2350, 8, 0x1020)
    dynamic(image, 0x600, 176, tags)
    for index, word in enumerate(words):
        struct.pack_into("<Q", image, 0x980 + index * 8, word)
    struct.pack_into("<Q", image, 0x920, 0x1020)
    struct.pack_into("<Q", image, 0x930, 0x1040)
    aligned = bytearray(0x3000)
    aligned[:0x400] = image[:0x400]
    aligned[0x1000:0x1100] = image[0x400:0x500]
    aligned[0x2000:0x2400] = image[0x600:0xa00]
    for header, offset in ((64, 0x1000), (120, 0x2000), (176, 0x2000)):
        struct.pack_into("<Q", aligned, header + 8, offset)
    for header in (64, 120):
        struct.pack_into("<Q", aligned, header + 48, 0x1000)
    return aligned


def replace_tag(image, kind, value, guest):
    start = 0x2000 if guest else 0x400
    for offset in range(start, start + 0x200, 16):
        tag = struct.unpack_from("<q", image, offset)[0]
        if tag == kind:
            struct.pack_into("<Q", image, offset + 8, value)
            return image
        if tag == 0:
            break
    raise AssertionError(kind)


def linux_relocations(path):
    image = path.read_bytes()
    tags = dynamic_tags(image)
    assert not {35, 36, 37} & tags.keys(), tags
    if tags.get(8, 0) == 0:
        return []
    offset = file_offset(image, tags[7])
    return [struct.unpack_from("<QQQ", image, position) for position in range(offset, offset + tags[8], 24)]


def linux_checked_entry(image, expected):
    image = bytearray(image)
    entry = struct.unpack_from("<Q", image, 24)[0]
    checked_entry = entry + 0x40
    code = b"\xe8" + struct.pack("<i", entry - checked_entry - 5)
    code += b"\x3d" + struct.pack("<I", expected) + b"\x74\x02\x0f\x0b\xcc"
    offset = file_offset(image, checked_entry)
    assert offset + len(code) <= len(image)
    image[offset:offset + len(code)] = code
    struct.pack_into("<Q", image, 24, checked_entry)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    guest = len(sys.argv) > 2 and sys.argv[2] == "guest"
    build = guest_image if guest else main_image
    anchor = 0x2320 if guest else 0x800
    positive = (("addresses", (0x2320, 0x2330) if guest else (anchor,), False),
                ("bitmap", (anchor, 5) if guest else (anchor, 3 | (1 << 63), 3), False),
                ("mixed", (anchor, 5) if guest else (anchor, 3), True))
    with tempfile.TemporaryDirectory(prefix="anyps5-relr-") as directory:
        work = Path(directory)

        def convert(label, image, error=None, expected=None):
            for system, options, magic in SYSTEMS:
                if guest and system == "linux-pie":
                    continue
                case = work / label / system
                case.mkdir(parents=True)
                source = case / "input.elf"
                if guest:
                    modules = case / "sce_module"
                    modules.mkdir()
                    module = modules / "provider.prx"
                    module.write_bytes(image)
                    input_image = executable("provider.prx")
                else:
                    input_image = image
                if system.startswith("linux"):
                    input_image = linux_checked_entry(input_image, 11 if guest else 42)
                source.write_bytes(input_image)
                output = case / ("output.exe" if system == "windows" else "output.bin")
                result = subprocess.run([str(relinker), *options, *([] if guest else ["--skip-sce-module"]),
                                         str(source), str(output)], capture_output=True, text=True, timeout=10)
                if error:
                    assert result.returncode == 2 and error in result.stderr and not output.exists(), (label, system, result.stderr)
                    continue
                assert result.returncode == 0 and output.read_bytes().startswith(magic), (label, system, result.stderr)
                if guest:
                    assert module.read_bytes() == image
                if system.startswith("linux") and expected is not None:
                    artifact = case / "app0/sce_module/provider.prx.guest.prx" if guest else output
                    assert all(header[2] % 0x1000 == header[3] % 0x1000 for header in elf_loads(artifact.read_bytes()))
                    observed = linux_relocations(artifact)
                    wanted = {(target + 0x400000, kind, addend + 0x400000) for target, kind, addend in expected} if system == "linux" and not guest else expected
                    assert set(observed) == wanted and len(observed) == len(wanted), (label, observed, wanted)
                if system == "windows" and os.name == "nt":
                    executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    assert executed.returncode == (11 if guest else 42), (label, system, executed.returncode, executed.stderr)
                if system.startswith("linux") and sys.platform == "linux" and platform.machine() in ("x86_64", "AMD64"):
                    output.chmod(0o755)
                    executed = run_without_core_dump([str(output)], capture_output=True, text=True, timeout=10)
                    assert executed.returncode == -signal.SIGTRAP, (label, system, executed.returncode, executed.stderr)

        for label, words, mixed in positive:
            if guest:
                expected = {(0x2320, 8, 0x1020), (0x2330, 8, 0x1040)}
                if mixed:
                    expected.add((0x2350, 8, 0x1020))
            else:
                targets = {0x800}
                if label == "bitmap":
                    targets.update((0x808, 0x9f8, 0xa00))
                if mixed:
                    targets.update((0x808, 0x850))
                expected = {(target, 8, 0x210) for target in targets}
            convert(label, build(words, mixed), expected=expected)

        if not guest:
            bss = main_image((0x800, 0x1008, 0xffc))
            struct.pack_into("<I", bss, 0xffc, 0x210)
            convert("zero-fill-addends", bss, expected={(0x800, 8, 0x210), (0x1008, 8, 0), (0xffc, 8, 0x210)})
            unaligned = main_image((0x802,))
            unaligned[0x200:0x206] = b"\xff\x25" + struct.pack("<i", 0x802 - 0x206)
            struct.pack_into("<Q", unaligned, 0x802, 0x210)
            convert("even-unaligned-slot", unaligned, expected={(0x802, 8, 0x210)})
            empty = main_image(())
            empty[0x200:0x206] = b"\xb8\x2a\x00\x00\x00\xc3"
            convert("empty", empty, expected=set())

        convert("bitmap-first", build((3,)), "RELR bitmap precedes address entry")
        convert("overlap", build((anchor, anchor)), "Overlapping RELR targets")
        convert("partial-overlap", build((anchor, anchor + 2)), "Overlapping RELR targets")
        convert("entry-size", replace_tag(build(), 37, 4, guest), "Invalid DT_RELRENT")
        convert("table-size", replace_tag(build(), 35, 9, guest), "Invalid DT_RELRSZ")
        convert("table-range", replace_tag(build(), 36, 0xfffffffffffffff8, guest), "RELR table exceeds file-backed")
        convert("size-overflow", replace_tag(build(), 35, 0xfffffffffffffff8, guest), "RELR table exceeds file-backed")
        convert("target-range", build((0xfffffffffffffffe,)), "RELR target exceeds load")
        incomplete = build()
        start = 0x2000 if guest else 0x400
        for offset in range(start, start + 0x200, 16):
            if struct.unpack_from("<q", incomplete, offset)[0] == 37:
                struct.pack_into("<q", incomplete, offset, 0x60000001)
                break
        convert("incomplete", incomplete, "Incomplete DT_RELR metadata")
        overlap = build(mixed=True)
        struct.pack_into("<Q", overlap, 0x2300 if guest else 0x740, anchor)
        convert("relr-rela-overlap", overlap, "Overlapping guest relocations" if guest else "RELR target overlaps RELA relocation")
    print("Guest RELR conversion and initializer tests passed" if guest else "Executable RELR conversion tests passed")


if __name__ == "__main__":
    main()
