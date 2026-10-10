from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = [
        ("program-header-offset", 0x20, "FileByteOffset out of bounds (offset 0xffffffffffffffff)"),
        ("load-offset", 64 + 8, "Segment offset out of bounds (offset 0xffffffffffffffff)"),
        ("dynamic-offset", 120 + 8, "Dynamic segment out of bounds (offset 0xffffffffffffffff)"),
    ]
    with tempfile.TemporaryDirectory(prefix="anyps5-elf-offsets-") as directory:
        for name, field, error in cases:
            source = Path(directory) / (name + ".elf")
            data = fixture()
            struct.pack_into("<Q", data, field, 0xffffffffffffffff)
            source.write_bytes(data)
            for mode in ([], ["--windows"]):
                output = source.with_suffix(".out")
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, mode, result)
        for size in (0, 55, 64):
            source = Path(directory) / f"program-header-entry-size-{size}.elf"
            data = fixture()
            struct.pack_into("<H", data, 0x36, size)
            source.write_bytes(data)
            for mode in ([], ["--windows"]):
                output = source.with_suffix(".out")
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                error = "Invalid ELF program header entry size: expected 56 bytes (offset 0x36)"
                assert result.returncode == 2 and error in result.stderr and not output.exists(), (size, mode, result)
    print("ELF offset tests passed")


if __name__ == "__main__":
    main()
