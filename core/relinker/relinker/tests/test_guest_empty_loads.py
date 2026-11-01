from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


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


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-empty-loads-") as directory:
        work = Path(directory)
        for first in (False, True):
            for address in (0x1000, 0x1080, 0x1100, 0x2000, 0x2200, 0x2400):
                case = work / f"{first}-{address:x}"
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                guest = modules / "empty-load.prx"
                image = module(address, first)
                guest.write_bytes(image)
                output = case / "output.elf"
                converted = subprocess.run([str(relinker), str(source), str(output)],
                                           capture_output=True, text=True, timeout=10)
                assert converted.returncode == 0, (first, address, converted.stderr)
                assert guest.read_bytes() == image
                artifact = case / "app0/sce_module/empty-load.prx.guest.prx"
                assert artifact.read_bytes().startswith(b"\x7fELF")
                if sys.platform == "linux" and os.uname().machine == "x86_64":
                    call = "import ctypes, sys; sys.exit(getattr(ctypes.CDLL(sys.argv[1]), 'shared#guest')())"
                    loaded = subprocess.run([sys.executable, "-c", call, str(artifact)],
                                            capture_output=True, text=True, timeout=10)
                    assert loaded.returncode == 42, (first, address, loaded.returncode, loaded.stderr)
    print("Guest empty-load conversion tests passed")


if __name__ == "__main__":
    main()
