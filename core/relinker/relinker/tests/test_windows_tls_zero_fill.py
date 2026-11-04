"""Keep the zero-filled part of a large PT_TLS block out of memory while converting to Windows: .gtls stores
only the p_filesz template and reserves the rest of the block and the thread control block as virtual
size, so the loader still copies a zeroed block for each thread."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_tls_function_coverage import RUNNER, RUNS_ON_HOST, TLS_LOAD, make_image, pe_bytes_at
from test_windows_sparse_load import child_peak_memory

LARGE_TLS_MEMORY_SIZE = 0x20000000
RUNNABLE_TLS_MEMORY_SIZE = 0x4000000
THREAD_CONTROL_BLOCK = 0x30
PEAK_MEMORY_LIMIT = 256 << 20


def tls_image(memory_size):
    body = TLS_LOAD + bytes.fromhex("8b 48 f8 8b 80") + struct.pack("<i", -memory_size) + bytes.fromhex("01 c8 c3")
    image = make_image("register", "unwind", body=body)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 8, memory_size, 16)
    return image


def convert(relinker, work, name, image):
    source = work / (name + ".elf")
    output = source.with_suffix(".exe")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-zero-fill-") as directory:
        work = Path(directory)
        convert(relinker, work, "large", tls_image(LARGE_TLS_MEMORY_SIZE))
        peak = child_peak_memory()
        assert peak is None or peak < PEAK_MEMORY_LIMIT, f"{peak} bytes of memory for a {LARGE_TLS_MEMORY_SIZE:#x} byte TLS block"

        image = tls_image(RUNNABLE_TLS_MEMORY_SIZE)
        output = convert(relinker, work, "runnable", image)
        pe = output.read_bytes()
        assert len(pe) < 1 << 20, f"{len(pe)} byte executable for a {RUNNABLE_TLS_MEMORY_SIZE:#x} byte TLS block"
        header = struct.unpack_from("<I", pe, 0x3c)[0]
        image_base = struct.unpack_from("<Q", pe, header + 24 + 24)[0]
        tls_rva = struct.unpack_from("<I", pe, header + 24 + 112 + 9 * 8)[0]
        start, end, _, _, zero_fill = struct.unpack_from("<QQQQI", pe_bytes_at(pe, tls_rva, 36))
        assert end - start == RUNNABLE_TLS_MEMORY_SIZE + THREAD_CONTROL_BLOCK and zero_fill == 0, (start, end, zero_fill)
        assert struct.unpack("<Q", pe_bytes_at(pe, start - image_base, 8))[0] == 42
        if os.name == "nt":
            executed = subprocess.run([str(output)], capture_output=True, timeout=30)
            assert executed.returncode == 42, (executed.returncode, executed.stderr)
        elif RUNS_ON_HOST:
            entry = 0x10000 + struct.unpack_from("<Q", image, 24)[0]
            executed = subprocess.run([sys.executable, str(RUNNER), str(output), hex(entry)], capture_output=True, timeout=30)
            assert executed.returncode == 42, (executed.returncode, executed.stderr)
    print("Windows TLS zero-fill test passed")


if __name__ == "__main__":
    main()
