"""Check that Windows guest modules with TLS get their TLS set up by the Windows loader."""

import ctypes
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol
from guest_import_fixtures import dynamic, executable


def guest_with_tls(tls):
    image = guest_fixture(PLAIN_SITE)
    if not tls:
        return image
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232,
                     7, 4, 0x980, 0x2380, 0x2380, 8, 0x40, 16)
    image[0x980:0x988] = bytes(range(1, 9))
    return image


def data_directory(data, index):
    header, = struct.unpack_from("<I", data, 0x3C)
    return struct.unpack_from("<II", data, header + 24 + 112 + index * 8)


def loaded_tls_index(path):
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.LoadLibraryW.restype = ctypes.c_void_p
    kernel32.LoadLibraryW.argtypes = [ctypes.c_wchar_p]
    base = kernel32.LoadLibraryW(str(path))
    assert base, (path, ctypes.get_last_error())
    header = ctypes.c_uint32.from_address(base + 0x3C).value
    tls_rva = ctypes.c_uint32.from_address(base + header + 24 + 112 + 9 * 8).value
    assert tls_rva != 0, path
    index_address = ctypes.c_uint64.from_address(base + tls_rva + 16).value
    return base, ctypes.c_uint32.from_address(index_address).value


def static_tls_getter(alignment, memory_size):
    image = module_with_symbol(True)
    struct.pack_into("<Q", image, 24, 0)
    strings = b"\0tls_value\0shared#A#B\0"
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", image, 0x898, 1, 0x06, 0, 1, 0, 8)
    code = b"\x48\x8b\x05" + struct.pack("<i", 0x2320 - 0x1007)
    code += b"\x64\x48\x8b\x80\0\0\0\0\xc3"
    struct.pack_into("<IBBHQQ", image, 0x8b0, 11, 0x12, 0, 1, 0x1000, len(code))
    image[0x400:0x400 + len(code)] = code
    struct.pack_into("<IIIIII", image, 0x840, 1, 3, 2, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x900, 0x2320, (1 << 32) | 18, 0)
    tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 24), (9, 24)]
    dynamic(image, 0x600, 176, tags)
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x980, 0x2380, 0x2380, 8, memory_size, alignment)
    struct.pack_into("<Q", image, 0x980, 42)
    return image


def check_static_tls(relinker, work):
    for alignment, memory_size in ((0, 8), (1, 8), (8, 8), (8, 16), (16, 16), (32, 8)):
        case = work / f"static-tls-{alignment}-{memory_size}"
        modules = case / "prx"
        modules.mkdir(parents=True)
        (modules / "tls.prx").write_bytes(static_tls_getter(alignment, memory_size))
        source = case / "input.elf"
        source.write_bytes(executable("tls.prx"))
        output = case / "output.exe"
        result = subprocess.run([str(relinker), "--windows", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (alignment, memory_size, result.stdout, result.stderr)
        data = (case / "app0/prx/tls.prx.guest.prx").read_bytes()
        slot_rva = 0x11320
        section = next(section for section in pe_sections(data) if section[1] <= slot_rva < section[1] + section[2])
        offset = section[3] + slot_rva - section[1]
        value, = struct.unpack_from("<q", data, offset)
        thread_alignment = max(alignment, 1)
        expected = -((memory_size + thread_alignment - 1) & ~(thread_alignment - 1))
        assert value == expected, (alignment, memory_size, value, expected)
        if os.name == "nt":
            run = subprocess.run([str(output)], capture_output=True, timeout=20)
            assert run.returncode == 42, (alignment, memory_size, run.returncode, run.stdout, run.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-tls-") as directory:
        case = Path(directory)
        module_dir = case / "sce_module"
        module_dir.mkdir()
        source = case / "input.elf"
        source.write_bytes(main_fixture())
        for name, tls in (("first", True), ("second", True), ("plain", False)):
            (module_dir / (name + ".prx")).write_bytes(guest_with_tls(tls))
        result = subprocess.run([str(relinker), "--windows", str(source), str(case / "output.exe")],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        modules = case / "app0" / "sce_module"
        for name, tls in (("first", True), ("second", True), ("plain", False)):
            data = (modules / (name + ".prx.guest.prx")).read_bytes()
            assert bool(data_directory(data, 9)[0]) == tls, name
            assert bool(data_directory(data, 1)[0]) == tls, name
        if os.name == "nt":
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
            first_base, first = loaded_tls_index(modules / "first.prx.guest.prx")
            second_base, second = loaded_tls_index(modules / "second.prx.guest.prx")
            kernel32.FreeLibrary(first_base)
            kernel32.FreeLibrary(second_base)
            assert first != second, (first, second)
        check_static_tls(relinker, case)
    print("Guest TLS integration tests passed")


if __name__ == "__main__":
    main()
