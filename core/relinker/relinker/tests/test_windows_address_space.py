"""Keep the main thread stack that Windows creates before libc reserves the guest arena below it."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture

GUEST_ARENA_START = 0x200000000
DYNAMIC_BASE = 0x40
NX_COMPAT = 0x100


def dll_characteristics(pe):
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    return struct.unpack_from("<H", pe, pe_offset + 24 + 70)[0]


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
        characteristics = dll_characteristics(output.read_bytes())
        assert characteristics == DYNAMIC_BASE | NX_COMPAT, f"DllCharacteristics {characteristics:#x}"
        if os.name == "nt":
            for _ in range(8):
                executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
                assert executed.returncode < GUEST_ARENA_START >> 32, \
                    f"stack not below {GUEST_ARENA_START:#x}: exit code {executed.returncode:#x}"
    print("Windows address space integration tests passed")


if __name__ == "__main__":
    main()
