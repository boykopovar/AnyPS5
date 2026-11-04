import json
import os
from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture as executable_fixture
from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_none_relocations import change_tag, module
from test_guest_symbol_names import dynamic_tags, file_offset
from test_linux_load_alignment import loads
from test_macos_output import MachO


DEFAULT_BASE = 0x400000
SCE_TAGS = {2: 0x6100002D, 3: 0x61000027, 5: 0x61000035, 6: 0x61000039,
            7: 0x6100002F, 8: 0x61000031, 9: 0x61000033, 10: 0x61000037,
            11: 0x6100003B, 20: 0x6100002B, 23: 0x61000029}
TARGETS = (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"]))
LAYOUTS = ("disjoint", "alias", "suffix", "partial")


def relocation_layout(layout, guest):
    address = 0x2400 if guest else 0x740
    local = (0x2500, 8, 0x1000) if guest else (0x308, 8, 0x210)
    jump = (0x2500 if layout == "alias" else 0x2508, (1 << 32) | 7, 0) if guest else (0x300, (1 << 32) | 7, 0)
    plt = [jump]
    if layout == "alias":
        return address, [jump], address, plt, []
    if layout == "partial":
        plt.append((0x2510 if guest else 0x310, (1 << 32) | 7, 0))
    if layout == "disjoint":
        return 0x2300 if guest else 0x700, [local], address, plt, [local]
    return address - 24, [local, jump], address, plt, [local]


def fixture(layout, sce, guest, misaligned=False, conflicting=False):
    rela_address, rela, plt_address, plt, local = relocation_layout(layout, guest)
    if conflicting:
        rela = [plt[0]]
    if guest:
        image = module(rela, plt, sce)
        for tag, value in ((7, rela_address), (8, len(rela) * 24),
                           (23, plt_address + int(misaligned)), (2, len(plt) * 24)):
            change_tag(image, SCE_TAGS[tag] if sce else tag,
                       value - 0x2200 if sce and tag in (7, 23) else value)
    else:
        image = executable_fixture()
        strings = b"\0module.prx\0shared\0"
        image[0x600:0x600 + len(strings)] = strings
        struct.pack_into("<IBBHQQ", image, 0x638, 12, 0x12, 0, 0, 0, 0)
        struct.pack_into("<IIIII", image, 0x680, 1, 2, 1, 0, 0)
        tags = [(5, 0x600), (10, len(strings)), (6, 0x620), (11, 24), (4, 0x680),
                (7, rela_address), (8, len(rela) * 24), (9, 24),
                (1, 1), (3, 0x300), (20, 7), (23, plt_address + int(misaligned)), (2, len(plt) * 24)]
        if sce:
            struct.pack_into("<IIQQQQQQ", image, 176, 0x61000000, 0, 0x500, 0, 0, 0xB00, 0xB00, 1)
            tags = [(SCE_TAGS.get(tag, tag), value - 0x500 if tag in (5, 6, 7, 23) else value)
                    for tag, value in tags]
        tags.append((0, 0))
        image[0x400:0x500] = bytes(0x100)
        for index, tag in enumerate(tags):
            struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
        struct.pack_into("<QQ", image, 152, len(tags) * 16, len(tags) * 16)
    for address, entries in ((rela_address, rela), (plt_address, plt)):
        for index, entry in enumerate(entries):
            struct.pack_into("<QQq", image, address + index * 24, *entry)
    return image, local, plt


def check_output(data, target, guest, local, plt):
    if target == "linux":
        base = 0 if guest else DEFAULT_BASE
        assert data.startswith(b"\x7fELF")
        assert all(load[2] % 0x1000 == load[3] % 0x1000 for load in loads(data))
        tags = dynamic_tags(data)
        for address, size, expected in ((7, 8, local), (23, 2, plt)):
            actual = []
            if tags.get(size, 0):
                start = file_offset(data, tags[address])
                actual = [struct.unpack_from("<QQq", data, offset)
                          for offset in range(start, start + tags[size], 24)]
            assert [(offset, info & 0xFFFFFFFF, addend) for offset, info, addend in actual] == [
                (offset + base, info & 0xFFFFFFFF, addend + base if info & 0xFFFFFFFF == 8 else addend)
                for offset, info, addend in expected], (actual, expected)
            if address == 23:
                assert all(info >> 32 for _, info, _ in actual)
        return
    pointers = local + plt if guest else local
    if target == "windows":
        assert data.startswith(b"MZ")
        sections = pe_sections(data)
        code = next(section for section in sections if section[0] == b".elf0")
        writable = next(section for section in sections if section[0] == (b".elf1" if guest else b".elf0"))
        header, = struct.unpack_from("<I", data, 0x3C)
        base, = struct.unpack_from("<Q", data, header + 48)
        for destination, _, _ in pointers:
            value, = struct.unpack_from("<Q", data, writable[3] + destination - (0x2000 if guest else 0))
            assert value == base + code[1] + (0 if guest else 0x210), hex(value)
    else:
        assert data.startswith(b"\xcf\xfa\xed\xfe")
        macho = MachO(data)
        code = macho.segment("__ELF0")
        writable = macho.segment("__ELF1" if guest else "__ELF0")
        for destination, _, _ in pointers:
            value, = struct.unpack_from("<Q", data, writable["fileoff"] + destination - (0x2000 if guest else 0))
            assert value == code["vmaddr"] + (0 if guest else 0x210), hex(value)


def main():
    if sys.argv[1] == "--load":
        import ctypes
        library = ctypes.CDLL(sys.argv[2])
        function = getattr(library, "shared#guest")
        function.restype = ctypes.c_int
        assert function() == 42
        address = ctypes.cast(function, ctypes.c_void_p).value
        for slot in map(int, sys.argv[3:]):
            pointer = ctypes.c_uint64.from_address(address + slot - 0x1000).value
            assert pointer == address, (hex(pointer), hex(address))
            assert ctypes.CFUNCTYPE(ctypes.c_int)(pointer)() == 42
        return
    relinker = Path(sys.argv[1]).resolve()
    native_linux = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-relocation-overlap-") as directory:
        work = Path(directory)

        def convert(name, sce, guest, target, options, image):
            case = work / f"{name}-{sce}-{guest}-{target}"
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            source = case / "input.elf"
            source.write_bytes(main_fixture() if guest else image)
            provider = image if guest else module()
            if not guest and target == "linux":
                provider[0x1000] = 0xCC
            module_path = modules / "module.prx"
            module_path.write_bytes(provider)
            output = case / ("output.exe" if target == "windows" else "output.out")
            result = subprocess.run([str(relinker), "--registry", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert module_path.read_bytes() == provider
            assert source.read_bytes() == (main_fixture() if guest else image)
            artifact = case / "app0" / "sce_module" / "module.prx.guest.prx" if guest else output
            return result, output, artifact

        for guest in (False, True):
            for sce in (False, True):
                for layout in LAYOUTS:
                    image, local, plt = fixture(layout, sce, guest)
                    for target, options in TARGETS:
                        result, output, artifact = convert(layout, sce, guest, target, options, image)
                        assert result.returncode == 0, (layout, sce, guest, target, result.stdout, result.stderr)
                        check_output(artifact.read_bytes(), target, guest, local, plt)
                        registry_path = output.parent / "output.module.prx.guest.prx.registry.json" if guest else output.with_suffix(".registry.json")
                        registry = json.loads(registry_path.read_text(encoding="utf-8"))
                        assert [(entry["nid"], entry["targetOffset"]) for entry in registry] == (
                            [] if guest else [("shared", hex(offset)) for offset, _, _ in plt]), registry
                        if target == "windows" and os.name == "nt":
                            run = subprocess.run([str(output)], capture_output=True, timeout=10)
                            assert run.returncode == 42, (layout, guest, run.returncode, run.stderr)
                        if target == "linux" and native_linux:
                            if guest:
                                run = subprocess.run([sys.executable, __file__, "--load", str(artifact),
                                                      *(str(offset) for offset, _, _ in local + plt)], capture_output=True, timeout=10)
                                assert run.returncode == 0, (layout, run.returncode, run.stderr)
                            else:
                                output.chmod(0o755)
                                run = subprocess.run([str(output)], capture_output=True, timeout=10)
                                assert run.returncode == -signal.SIGTRAP, (layout, run.returncode, run.stderr)
                conflict = "Overlapping guest relocations" if guest else "JUMP_SLOT relocation lies before the original .rela.plt table"
                failures = (("misaligned", fixture("suffix", sce, guest, misaligned=True)[0], "Unaligned overlapping relocation tables"),
                            ("conflicting", fixture("disjoint", sce, guest, conflicting=True)[0], conflict))
                for name, image, error in failures:
                    for target, options in TARGETS:
                        result, output, artifact = convert(name, sce, guest, target, options, image)
                        assert result.returncode == 2 and error in result.stderr and not output.exists() and not artifact.exists(), (
                            name, sce, guest, target, result.stdout, result.stderr)
    print("Overlapping relocation table tests passed")


if __name__ == "__main__":
    main()
