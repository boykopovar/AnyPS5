from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    header = bytearray(fixture()[:64])
    struct.pack_into("<Q", header, 0x20, 0)
    struct.pack_into("<H", header, 0x38, 0)
    cases = [
        ("tiny", bytes(10), "File too small for ELF header"),
        ("truncated", b"\x7fELF\x02\x01\x01" + bytes(25), "File too small for ELF header"),
        ("header-only", header, "No PT_DYNAMIC segment found"),
        ("bad-magic", bytes(64), "Invalid ELF magic number"),
        ("self", b"\x4f\x15\x3d\x1d" + bytes(60), "The input is a SELF container, not an ELF"),
    ]
    fields = [
        ("class", 4, "B", (0, 1, 3), "Unsupported ELF class: expected ELF64"),
        ("encoding", 5, "B", (0, 2, 3), "Unsupported ELF data encoding: expected little-endian"),
        ("ident-version", 6, "B", (0, 2), "Unsupported ELF identification version"),
        ("machine", 0x12, "H", (0, 3, 183), "Unsupported ELF machine: expected x86-64"),
        ("version", 0x14, "I", (0, 2, 0x100), "Unsupported ELF version"),
        ("header-size", 0x34, "H", (0, 52, 63, 65, 0x140), "Invalid ELF header size: expected 64 bytes"),
    ]
    for name, offset, encoding, values, error in fields:
        for value in values:
            data = fixture()
            struct.pack_into("<" + encoding, data, offset, value)
            cases.append((f"{name}-{value}", data, error))
    with tempfile.TemporaryDirectory(prefix="anyps5-elf-header-") as directory:
        work = Path(directory)
        for name, data, error in cases:
            source = work / (name + ".elf")
            source.write_bytes(data)
            for mode in ([], ["--windows"]):
                output = work / (name + (".exe" if mode else ".out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, mode, result)
        for name, elf_type, os_abi, abi_version in (("sysv", 3, 0, 0), ("sce", 0xfe10, 9, 2)):
            data = fixture()
            struct.pack_into("<H", data, 0x10, elf_type)
            data[7] = os_abi
            data[8] = abi_version
            source = work / (name + ".elf")
            source.write_bytes(data)
            for mode in ([], ["--windows"]):
                output = work / (name + (".exe" if mode else ".out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 0 and output.exists(), (name, mode, result)
                assert output.read_bytes().startswith(b"MZ" if mode else b"\x7fELF"), (name, mode)
    print("ELF header tests passed")


if __name__ == "__main__":
    main()
