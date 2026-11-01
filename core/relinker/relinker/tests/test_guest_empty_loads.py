from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol
from test_optional_plt import fixture as executable_fixture
from test_optional_rela import fixture as sce_fixture


TARGETS = (((), b"\x7fELF"), (("--lazy-binding",), b"\x7fELF"),
           (("--windows",), b"MZ"), (("--macos",), b"\xcf\xfa\xed\xfe"))
GUEST_TARGETS = (TARGETS[0], TARGETS[2], TARGETS[3])


def module(address, first):
    original = module_with_symbol(True)
    original[0x400:0x406] = bytes.fromhex("b8 2a 00 00 00 c3")
    struct.pack_into("<Q", original, 0x898 + 16, 6)
    image = bytearray(0x3000)
    image[:0x400] = original[:0x400]
    headers = []
    for index in range(3):
        header = list(struct.unpack_from("<IIQQQQQQ", original, 64 + index * 56))
        image[header[3]:header[3] + header[5]] = original[header[2]:header[2] + header[5]]
        header[2] = header[3]
        if header[0] == 1:
            header[7] = 0x1000
        headers.append(struct.pack("<IIQQQQQQ", *header))
    flags = 5 if address < 0x2000 else 6
    empty = struct.pack("<IIQQQQQQ", 1, flags, address, address, address, 0, 0, 0x1000)
    headers.insert(0 if first else len(headers), empty)
    image[64:64 + len(headers) * 56] = b"".join(headers)
    struct.pack_into("<H", image, 56, len(headers))
    return image


def executable(address, first):
    image = executable_fixture()
    headers = [image[64:120], image[120:176]]
    empty = struct.pack("<IIQQQQQQ", 1, 6, address, address, address, 0, 0, 0x1000)
    headers.insert(0 if first else len(headers), empty)
    image[64:64 + len(headers) * 56] = b"".join(headers)
    struct.pack_into("<H", image, 56, len(headers))
    return image


def convert_guests(relinker, work):
    for options, magic in GUEST_TARGETS:
        for first in (False, True):
            for address in (0x1000, 0x1080, 0x1100, 0x2000, 0x2200, 0x2400):
                case = work / f"guest{''.join(options)}-{first}-{address:x}"
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                guest = modules / "empty-load.prx"
                image = module(address, first)
                guest.write_bytes(image)
                output = case / "output.elf"
                converted = subprocess.run([str(relinker), *options, str(source), str(output)],
                                           capture_output=True, text=True, timeout=10)
                assert converted.returncode == 0, (options, first, address, converted.stderr)
                assert guest.read_bytes() == image
                artifact = case / "app0/sce_module/empty-load.prx.guest.prx"
                assert artifact.read_bytes().startswith(magic), (options, first, address)
                if not options and sys.platform == "linux" and os.uname().machine == "x86_64":
                    call = "import ctypes, sys; sys.exit(getattr(ctypes.CDLL(sys.argv[1]), 'shared#guest')())"
                    loaded = subprocess.run([sys.executable, "-c", call, str(artifact)],
                                            capture_output=True, text=True, timeout=10)
                    assert loaded.returncode == 42, (first, address, loaded.returncode, loaded.stderr)


def convert_executables(relinker, work):
    for options, magic in TARGETS:
        for first in (False, True):
            for address in (0, 0x800, 0x1000):
                case = work / f"executable{''.join(options)}-{first}-{address:x}"
                case.mkdir()
                source = case / "input.elf"
                source.write_bytes(executable(address, first))
                output = case / "output.exe"
                converted = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                           capture_output=True, text=True, timeout=10)
                assert converted.returncode == 0, (options, first, address, converted.stderr)
                assert output.read_bytes().startswith(magic), (options, first, address)
                if options == ("--windows",) and os.name == "nt":
                    executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    assert executed.returncode == 42, (first, address, executed.returncode, executed.stderr)


def entry_in_empty(first):
    image = executable(0x1000, first)
    struct.pack_into("<Q", image, 24, 0x1000)
    return image


def unsupported_flags(first):
    image = executable(0x1000, first)
    struct.pack_into("<I", image, 64 + (0 if first else 2) * 56 + 4, 8)
    return image


def all_empty(first):
    image = sce_fixture(True)
    struct.pack_into("<QQ", image, 64 + 32, 0, 0)
    headers = [image[64 + index * 56:120 + index * 56] for index in range(6)]
    empty = struct.pack("<IIQQQQQQ", 1, 6, 0x1000, 0x1000, 0x1000, 0, 0, 0x1000)
    headers.insert(0 if first else len(headers), empty)
    image[64:64 + len(headers) * 56] = b"".join(headers)
    struct.pack_into("<H", image, 56, len(headers))
    return image


REJECTIONS = ((entry_in_empty, "ELF address is not contained in PT_LOAD memory (offset 0x1000)"),
              (unsupported_flags, "Unsupported PT_LOAD permission flags (offset 0x1000)"),
              (all_empty, "No PT_LOAD segments found"))


def reject_executables(relinker, work):
    for build, error in REJECTIONS:
        for options in (("--windows",), ("--macos",)):
            for first in (False, True):
                case = work / f"reject-{build.__name__}{''.join(options)}-{first}"
                case.mkdir()
                source = case / "input.elf"
                source.write_bytes(build(first))
                output = case / "output.exe"
                converted = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                           capture_output=True, text=True, timeout=10)
                assert converted.returncode == 2 and error in converted.stderr and not output.exists(), (
                    build.__name__, options, first, converted.returncode, converted.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-empty-loads-") as directory:
        work = Path(directory)
        convert_guests(relinker, work)
        convert_executables(relinker, work)
        reject_executables(relinker, work)
    print("Empty-load conversion tests passed")


if __name__ == "__main__":
    main()
