"""Check that Windows guest modules with TLS get their TLS set up by the Windows loader."""

import ctypes
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture


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
    print("Guest TLS integration tests passed")


if __name__ == "__main__":
    main()
