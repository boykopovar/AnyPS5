"""Keep the unmapped gap between distant PT_LOAD segments out of memory while converting to Windows and macOS."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import PT_LOAD, fixture
from test_windows_bss import LOAD_RVA, SECTION_ALIGNMENT, sections

FAR_ADDRESS = 0x7FE00000
RUNNABLE_FAR_ADDRESS = 0x4000000
ENTRY = 0x10
PEAK_MEMORY_LIMIT = 512 << 20


def sparse_fixture(far_address=FAR_ADDRESS):
    image = fixture()
    struct.pack_into("<Q", image, 24, ENTRY)
    image[0x4000 + ENTRY:0x4000 + ENTRY + 6] = image[0x4000:0x4006]
    struct.pack_into("<IIQQQQQQ", image, 64 + 2 * 56, PT_LOAD, 6, 0, far_address, far_address, 0, 0x1000, 0x1000)
    return image


def child_peak_memory():
    try:
        import resource
    except ImportError:
        return None
    peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    return peak if sys.platform == "darwin" else peak * 1024


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-sparse-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        source.write_bytes(sparse_fixture())
        output = work / "output.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        peak = child_peak_memory()
        assert peak is None or peak < PEAK_MEMORY_LIMIT, f"{peak} bytes of memory for a {FAR_ADDRESS:#x} byte address gap"
        pe = output.read_bytes()
        assert len(pe) < 1 << 20, f"{len(pe)} byte executable for a {FAR_ADDRESS:#x} byte address gap"
        table, image_size = sections(pe)
        assert image_size >= LOAD_RVA + FAR_ADDRESS + 0x1000, hex(image_size)
        gap = next(section for section in table if section[2] <= LOAD_RVA + SECTION_ALIGNMENT * 2 < section[2] + section[1])
        name, virtual_size, rva, raw_size, raw = gap
        assert raw_size == 0 and raw == 0 and rva + virtual_size <= LOAD_RVA + FAR_ADDRESS, gap
        far = next(section for section in table if section[2] == LOAD_RVA + FAR_ADDRESS)
        assert far[1] == 0x1000, far
        if os.name == "nt":
            runnable = work / "runnable.elf"
            runnable.write_bytes(sparse_fixture(RUNNABLE_FAR_ADDRESS))
            runnable_output = work / "runnable.exe"
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(runnable), str(runnable_output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            executed = subprocess.run([str(runnable_output)], capture_output=True, text=True, timeout=20)
            assert executed.returncode == 42, f"exit code {executed.returncode:#x}"
        macho = work / "output.macho"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--macos", str(source), str(macho)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        peak = child_peak_memory()
        assert peak is None or peak < PEAK_MEMORY_LIMIT, f"{peak} bytes of memory for a {FAR_ADDRESS:#x} byte address gap"
        assert macho.stat().st_size < 1 << 20, macho.stat().st_size
    print("Windows sparse load image tests passed")


if __name__ == "__main__":
    main()
