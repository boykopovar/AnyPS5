"""Keep converted Windows images inside the console's 40-bit address window: the executable loads at its fixed low base, guest modules keep relocating, and the main thread stack that Windows creates before libc reserves the guest arena stays below it."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_module_directories import module_with_symbol
from test_optional_plt import fixture

GUEST_ARENA_START = 0x200000000
DYNAMIC_BASE = 0x40
NX_COMPAT = 0x100


def optional_header(pe):
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    return pe_offset + 24


def dll_characteristics(pe):
    return struct.unpack_from("<H", pe, optional_header(pe) + 70)[0]


def image_base(pe):
    return struct.unpack_from("<Q", pe, optional_header(pe) + 24)[0]


def relocation_directory_size(pe):
    return struct.unpack_from("<I", pe, optional_header(pe) + 112 + 5 * 8 + 4)[0]


def module_with_relative_relocation():
    module = module_with_symbol(True)
    struct.pack_into("<Q", module, 0x668, 24)
    struct.pack_into("<QQq", module, 0x900, 0x2320, 8, 0x1000)
    return module


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-address-space-") as directory:
        work = Path(directory)
        image = fixture()
        image[0x210:0x218] = bytes.fromhex("4889e0" "48c1e820" "c3")
        source = work / "input.elf"
        source.write_bytes(image)
        output = work / "output.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        executable = output.read_bytes()
        assert relocation_directory_size(executable) > 0, "executable fixture has no base relocations"
        assert dll_characteristics(executable) == NX_COMPAT, f"DllCharacteristics {dll_characteristics(executable):#x}"
        assert image_base(executable) < GUEST_ARENA_START, f"ImageBase {image_base(executable):#x}"
        if os.name == "nt":
            for _ in range(8):
                executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
                assert executed.returncode < GUEST_ARENA_START >> 32, \
                    f"stack not below {GUEST_ARENA_START:#x}: exit code {executed.returncode:#x}"
        module_case = work / "module"
        (module_case / "prx").mkdir(parents=True)
        (module_case / "prx" / "provider.prx").write_bytes(module_with_relative_relocation())
        module_source = module_case / "input.elf"
        module_source.write_bytes(fixture())
        module_output = module_case / "output.exe"
        result = subprocess.run([str(relinker), "--windows", str(module_source), str(module_output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        artifact = module_case / "app0" / "prx" / "provider.prx.guest.prx"
        assert artifact.is_file(), list((module_case / "app0").rglob("*"))
        module = artifact.read_bytes()
        assert relocation_directory_size(module) > 0, "module fixture has no base relocations"
        assert dll_characteristics(module) == DYNAMIC_BASE | NX_COMPAT, \
            f"DllCharacteristics {dll_characteristics(module):#x}"
    print("Windows address space integration tests passed")


if __name__ == "__main__":
    main()
