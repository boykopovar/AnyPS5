import json
import os
from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from signal_run import run_without_core_dump
from test_dynamic_segment import fixture as base_fixture
from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads
from test_windows_import_modules import provider


DEFAULT_BASE = 0x400000
SCE_TAGS = {2: 0x6100002D, 3: 0x61000027, 5: 0x61000035, 6: 0x61000039,
            7: 0x6100002F, 8: 0x61000031, 9: 0x61000033, 10: 0x61000037,
            11: 0x6100003B, 20: 0x6100002B, 23: 0x61000029}
TARGETS = (("linux", [], b"\x7fELF"), ("windows", ["--windows"], b"MZ"),
           ("macos", ["--macos"], b"\xcf\xfa\xed\xfe"))


def guest_provider(code=None):
    image = provider(42, code=code)
    aligned = bytearray(0x3000)
    aligned[:0x400] = image[:0x400]
    aligned[0x1000:0x1100] = image[0x400:0x500]
    aligned[0x2000:0x2400] = image[0x600:0xA00]
    for header, offset in ((64, 0x1000), (120, 0x2000), (176, 0x2000)):
        struct.pack_into("<Q", aligned, header + 8, offset)
    for header in (64, 120):
        struct.pack_into("<Q", aligned, header + 48, 0x1000)
    return aligned


def fixture(sce, relocation_tags=(), plt=False, relative=False, extra_tags=()):
    image = base_fixture()
    strings = b"\0module.prx\0shared\0" if plt else b"\0"
    image[0x600:0x600 + len(strings)] = strings
    image[0x200:0x206] = bytes.fromhex("ff25fa000000" if plt or relative else "b82a000000c3")
    tags = [(5, 0x600), (10, len(strings)), (6, 0x620), (11, 24), (4, 0x680), *relocation_tags]
    struct.pack_into("<IIIII", image, 0x680, 1, 2 if plt else 1, 1 if plt else 0, 0, 0)
    if plt:
        struct.pack_into("<IBBHQQ", image, 0x638, 12, 0x12, 0, 0, 0, 0)
        struct.pack_into("<QQq", image, 0x740, 0x300, (1 << 32) | 7, 0)
        tags += [(1, 1), (3, 0x300), (20, 7), (23, 0x740), (2, 24)]
    if sce:
        struct.pack_into("<IIQQQQQQ", image, 176, 0x61000000, 0, 0x500, 0, 0, 0xB00, 0xB00, 1)
        tags = [(SCE_TAGS.get(tag, tag), value - 0x500 if tag in (5, 6, 7, 23) else value)
                for tag, value in tags]
    tags += [*extra_tags, (0, 0)]
    image[0x400:0x500] = bytes(0x100)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQ", image, 152, len(tags) * 16, len(tags) * 16)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = (("absent", (), False, False),
             ("entry-size-only", ((9, 24),), False, False),
             ("empty", ((7, 0x700), (8, 0), (9, 24)), False, False),
             ("relative", ((7, 0x700), (8, 24), (9, 24)), False, True),
             ("plt-only", (), True, False),
             ("plt-entry-size", ((9, 24),), True, False))
    with tempfile.TemporaryDirectory(prefix="anyps5-optional-rela-") as directory:
        work = Path(directory)
        for sce in (False, True):
            for name, tags, plt, relative in cases:
                for target, options, magic in TARGETS:
                    case = work / f"{sce}-{name}-{target}"
                    case.mkdir()
                    image = fixture(sce, tags, plt, relative)
                    source = case / "input.elf"
                    source.write_bytes(image)
                    if plt:
                        modules = case / "sce_module"
                        modules.mkdir()
                        code = b"\xcc" if target == "linux" else None
                        (modules / "module.prx").write_bytes(guest_provider(code))
                    output = case / ("output.exe" if target == "windows" else "output.out")
                    result = subprocess.run([str(relinker), "--registry", *options,
                                             *([] if plt else ["--skip-sce-module"]), str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == 0, (case.name, result.stdout, result.stderr)
                    data = output.read_bytes()
                    assert data.startswith(magic), case.name
                    assert source.read_bytes() == image, case.name
                    registry = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
                    assert [(entry["nid"], entry["relocationType"], entry["targetOffset"]) for entry in registry] == (
                        [("shared", "R_X86_64_JUMP_SLOT", "0x300")] if plt else []), (case.name, registry)
                    if target == "linux":
                        if plt:
                            guest = (case / "app0" / "sce_module" / "module.prx.guest.prx").read_bytes()
                            assert all(load[2] % 0x1000 == load[3] % 0x1000 for load in loads(guest)), case.name
                        dynamic, _ = dynamic_strings(data)
                        assert dynamic.get(8, 0) == (24 if relative else 0), (case.name, dynamic)
                        assert dynamic.get(2, 0) == (24 if plt else 0), (case.name, dynamic)
                        if relative or plt:
                            offset = file_offset(data, loads(data), dynamic[23 if plt else 7])
                            destination, info, addend = struct.unpack_from("<QQq", data, offset)
                            assert destination == 0x300 + DEFAULT_BASE and info & 0xFFFFFFFF == (7 if plt else 8), case.name
                            assert addend == (0 if plt else 0x210 + DEFAULT_BASE), case.name
                    if target == "windows" and os.name == "nt":
                        run = subprocess.run([str(output)], capture_output=True, timeout=20)
                        assert run.returncode == 42, (case.name, run.returncode, run.stderr)
                    if target == "linux" and plt and sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64"):
                        output.chmod(0o755)
                        run = run_without_core_dump([str(output)], capture_output=True, timeout=20)
                        assert run.returncode == -signal.SIGTRAP, (case.name, run.returncode, run.stderr)

            invalid = (("missing-address", ((8, 24), (9, 24)), (), "DT_RELA"),
                       ("missing-size", ((7, 0x700), (9, 24)), (), "DT_RELASZ"),
                       ("missing-entry-size", ((7, 0x700), (8, 0)), (), "DT_RELAENT"),
                       ("invalid-entry-size", ((9, 8),), (), "Unsupported DT_RELAENT"),
                       ("unaligned-size", ((7, 0x700), (8, 1), (9, 24)), (), "Invalid DT_RELASZ"))
            for tag in (7, 8, 9):
                value = {7: 0x700, 8: 0, 9: 24}[tag]
                other = tag if sce else SCE_TAGS[tag]
                invalid += ((f"ambiguous-{tag}", ((7, 0x700), (8, 0), (9, 24)),
                             ((other, value),), "Both DT_OS_ and DT_ variants"),)
            for tag in (17, 18, 19, 35, 36, 37):
                invalid += ((f"unsupported-{tag}", (), ((tag, 24),), "relocations are not supported"),)
            for name, tags, extra, error in invalid:
                source = work / f"{sce}-{name}.elf"
                source.write_bytes(fixture(sce, tags, extra_tags=extra))
                for target, options, _ in TARGETS:
                    output = work / f"{sce}-{name}-{target}.out"
                    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == 2 and error in result.stderr and not output.exists(), (
                        name, sce, target, result.returncode, result.stdout, result.stderr)
    print("Optional RELA integration tests passed")


if __name__ == "__main__":
    main()
