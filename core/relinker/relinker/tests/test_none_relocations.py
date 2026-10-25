import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture
from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads

DEFAULT_BASE = 0x400000


def rebased(relocation):
    offset, info, addend = relocation
    return (offset + DEFAULT_BASE, info, addend + DEFAULT_BASE if info & 0xffffffff == 8 else addend)


def image_with_relocations(relocations, sce, imported):
    image = fixture()
    struct.pack_into("<Q", image, 0x458, len(relocations) * 24)
    for index, relocation in enumerate(relocations):
        struct.pack_into("<QQq", image, 0x700 + index * 24, *relocation)
    if not any(info & 0xffffffff == 8 for _, info, _ in relocations):
        image[0x200:0x206] = bytes.fromhex("b82a000000c3")
    if imported:
        strings = b"\0symbol\0libc.prx\0"
        image[0x600:0x600 + len(strings)] = strings
        struct.pack_into("<Q", image, 0x418, len(strings))
        struct.pack_into("<IBBHQQ", image, 0x638, 1, 0x12, 0, 0, 0, 0)
        image[0x200:0x20d] = bytes.fromhex("488b0501010000ff25f3000000")
        struct.pack_into("<qQ", image, 0x470, 0x6100003f, 48)
        struct.pack_into("<qQ", image, 0x480, 1, 8)
        struct.pack_into("<QQ", image, 152, 0xa0, 0xa0)
    if sce:
        struct.pack_into("<IIQQQQQQ", image, 176, 0x61000000, 0, 0x500, 0, 0, 0xb00, 0xb00, 1)
        variants = {5: 0x61000035, 10: 0x61000037, 6: 0x61000039, 11: 0x6100003b,
                    7: 0x6100002f, 8: 0x61000031, 9: 0x61000033}
        for offset in range(0x400, 0x470, 16):
            tag, value = struct.unpack_from("<qQ", image, offset)
            struct.pack_into("<qQ", image, offset, variants[tag], value - 0x500 if tag in (5, 6, 7) else value)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    relative = (0x300, 8, 0x210)
    noop = (0, 0, 0)
    ignored_fields = (0xffffffffffffffff, 0xffffffff00000000, -1)
    imported = (0x308, (1 << 32) | 6, 0)
    cases = (("baseline", [relative]), ("before", [noop, relative]), ("after", [relative, noop]),
             ("ignored-fields", [ignored_fields, relative, noop]),
             ("only-none", [noop, ignored_fields]),
             ("with-import", [noop, imported, ignored_fields, relative]))
    with tempfile.TemporaryDirectory(prefix="anyps5-none-relocations-") as directory:
        work = Path(directory)
        for sce in (False, True):
            for name, relocations in cases:
                has_import = imported in relocations
                image = image_with_relocations(relocations, sce, has_import)
                source = work / f"{sce}-{name}.elf"
                source.write_bytes(image)
                for target, options, magic in (("linux", [], b"\x7fELF"), ("windows", ["--windows"], b"MZ"),
                                               ("macos", ["--macos"], b"\xcf\xfa\xed\xfe")):
                    for level in (0, 1):
                        output = work / f"{sce}-{name}-{target}-{level}.out"
                        result = subprocess.run([str(relinker), "--skip-sce-module", "--registry",
                                                 f"unused-filter={level}", *options, str(source), str(output)],
                                                capture_output=True, text=True, timeout=20)
                        assert result.returncode == 0, (sce, name, target, level, result.stdout, result.stderr)
                        data = output.read_bytes()
                        assert data.startswith(magic), (name, target)
                        registry = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
                        assert [(entry["nid"], entry["relocationType"], entry["targetOffset"]) for entry in registry] == (
                            [("symbol", "R_X86_64_GLOB_DAT", "0x308")] if has_import else [])
                        assert source.read_bytes() == image
                        if target == "linux":
                            tags, _ = dynamic_strings(data)
                            actual = []
                            if tags.get(8, 0):
                                start = file_offset(data, loads(data), tags[7])
                                actual = [struct.unpack_from("<QQq", data, offset) for offset in range(start, start + tags[8], 24)]
                            assert actual == [rebased(relocation) for relocation in relocations if relocation in (imported, relative)]
                        if target == "windows" and os.name == "nt" and not has_import:
                            run = subprocess.run([str(output)], capture_output=True, timeout=20)
                            assert run.returncode == 42, (name, level, run.returncode, run.stderr)
        for sce in (False, True):
            image = image_with_relocations([noop, (0x300, (1 << 32) | 8, 0x210)], sce, False)
            struct.pack_into("<IBBHQQ", image, 0x638, 0, 3, 0, 0, 0, 0)
            struct.pack_into("<qQ", image, 0x470, 0x6100003f, 48)
            struct.pack_into("<QQ", image, 152, 0x90, 0x90)
            source = work / f"{sce}-section-relative.elf"
            source.write_bytes(image)
            for target, options in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
                for level in (0, 1):
                    output = work / f"{sce}-section-relative-{target}-{level}.out"
                    result = subprocess.run([str(relinker), "--skip-sce-module", f"unused-filter={level}", *options,
                                             str(source), str(output)], capture_output=True, text=True, timeout=20)
                    assert result.returncode == 0, (sce, target, level, result.stdout, result.stderr)
                    data = output.read_bytes()
                    if target == "linux":
                        tags, _ = dynamic_strings(data)
                        start = file_offset(data, loads(data), tags[7])
                        assert [struct.unpack_from("<QQq", data, offset) for offset in range(start, start + tags[8], 24)] == [rebased(relative)]
                    if target == "windows" and os.name == "nt":
                        run = subprocess.run([str(output)], capture_output=True, timeout=20)
                        assert run.returncode == 42, (sce, level, run.returncode, run.stderr)
        for name, symbol in (("notype", (0, 0, 0, 0, 0, 0)), ("valued-section", (0, 3, 0, 0, 0x10, 0))):
            image = image_with_relocations([noop, (0x300, (1 << 32) | 8, 0x210)], False, False)
            struct.pack_into("<IBBHQQ", image, 0x638, *symbol)
            source = work / f"invalid-relative-{name}.elf"
            source.write_bytes(image)
            output = work / f"invalid-relative-{name}.exe"
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "RELATIVE relocation has a nonzero symbol index" in result.stderr, (name, result.stderr)
            assert not output.exists()
        overflow_image = image_with_relocations([(0xfffffffffffffff9, 8, 0x210)], False, False)
        overflow_source = work / "invalid-relocation-overflow.elf"
        overflow_source.write_bytes(overflow_image)
        overflow_output = work / "invalid-relocation-overflow.exe"
        overflow_result = subprocess.run([str(relinker), "--skip-sce-module", "--windows",
                                          str(overflow_source), str(overflow_output)],
                                         capture_output=True, text=True, timeout=20)
        assert overflow_result.returncode == 2 and "Relocation target address overflow" in overflow_result.stderr, overflow_result.stderr
        assert not overflow_output.exists()
        page_image = image_with_relocations([(0xffc, 8, 0x210)], False, False)
        page_image[0x200:0x206] = b"\xff\x25" + struct.pack("<i", 0xffc - 0x206)
        page_image.extend(bytes(0x1000))
        struct.pack_into("<QQ", page_image, 64 + 32, len(page_image), len(page_image))
        page_source = work / "base-relocation-page-crossing.elf"
        page_source.write_bytes(page_image)
        page_output = work / "base-relocation-page-crossing.exe"
        page_result = subprocess.run([str(relinker), "--skip-sce-module", "--windows",
                                      str(page_source), str(page_output)],
                                     capture_output=True, text=True, timeout=20)
        assert page_result.returncode == 0 and page_output.read_bytes().startswith(b"MZ"), page_result.stderr
        if os.name == "nt":
            run = subprocess.run([str(page_output)], capture_output=True, timeout=20)
            assert run.returncode == 42, ("page-crossing relocation", run.returncode, run.stderr)
            moved = bytearray(page_output.read_bytes())
            optional_header = struct.unpack_from("<I", moved, 0x3c)[0] + 24
            characteristics = struct.unpack_from("<H", moved, optional_header + 70)[0]
            struct.pack_into("<H", moved, optional_header + 70, characteristics | 0x40)
            moved_output = work / "base-relocation-page-crossing-moved.exe"
            moved_output.write_bytes(moved)
            for _ in range(3):
                run = subprocess.run([str(moved_output)], capture_output=True, timeout=20)
                assert run.returncode == 42, ("page-crossing relocation after the loader moved the image", run.returncode, run.stderr)
        past_end_image = image_with_relocations([(0x1ffc, 8, 0x210)], False, False)
        past_end_image.extend(bytes(0x1000))
        struct.pack_into("<QQ", past_end_image, 64 + 32, len(past_end_image), len(past_end_image))
        past_end_source = work / "invalid-base-relocation-past-end.elf"
        past_end_source.write_bytes(past_end_image)
        past_end_output = work / "invalid-base-relocation-past-end.exe"
        past_end_result = subprocess.run([str(relinker), "--skip-sce-module", "--windows",
                                          str(past_end_source), str(past_end_output)],
                                         capture_output=True, text=True, timeout=20)
        assert past_end_result.returncode == 2 and "ELF address is not contained in PT_LOAD memory" in past_end_result.stderr, past_end_result.stderr
        assert not past_end_output.exists()
    print("No-op executable relocation tests passed")


if __name__ == "__main__":
    main()
