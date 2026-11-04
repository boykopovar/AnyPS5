"""Leave the zero-filled tail of PT_LOAD memory out of the Windows file and let the loader zero-fill it."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture

BSS = 64 << 20
FILE_ALIGNMENT = 0x200
SECTION_ALIGNMENT = 0x1000
LOAD_RVA = 0x10000


def sections(pe):
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    count = struct.unpack_from("<H", pe, header + 6)[0]
    optional = header + 24
    table = optional + struct.unpack_from("<H", pe, header + 20)[0]
    result = []
    for index in range(count):
        offset = table + index * 40
        name = pe[offset:offset + 8].rstrip(b"\0").decode()
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", pe, offset + 8)
        result.append((name, virtual_size, rva, raw_size, raw))
    return result, struct.unpack_from("<I", pe, optional + 56)[0]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-bss-") as directory:
        work = Path(directory)
        image = fixture()
        memory_size = struct.unpack_from("<Q", image, 64 + 40)[0] + BSS
        struct.pack_into("<Q", image, 64 + 40, memory_size)
        last = memory_size - 4
        image[0x210:0x21a] = b"\x8b\x05" + struct.pack("<i", last - 0x216) + b"\x83\xc0\x2a\xc3"
        source = work / "input.elf"
        source.write_bytes(image)
        output = work / "output.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        pe = output.read_bytes()
        assert len(pe) < BSS // 16, f"{len(pe)} byte executable for {BSS} bytes of zero-filled memory"
        table, image_size = sections(pe)
        first = next(section for section in table if section[2] == LOAD_RVA)
        name, virtual_size, rva, raw_size, raw = first
        assert virtual_size >= memory_size and rva + virtual_size <= image_size, first
        assert 0 < raw_size < SECTION_ALIGNMENT * 2, first
        for name, virtual_size, rva, raw_size, raw in table:
            assert raw_size % FILE_ALIGNMENT == 0 and raw % FILE_ALIGNMENT == 0, name
            assert raw_size <= -(-virtual_size // FILE_ALIGNMENT) * FILE_ALIGNMENT, name
            assert (raw_size == 0) == (raw == 0) and raw + raw_size <= len(pe), name
        mapped = bytearray(image_size)
        for name, virtual_size, rva, raw_size, raw in table:
            size = min(raw_size, virtual_size)
            mapped[rva:rva + size] = pe[raw:raw + size]
        loaded = mapped[LOAD_RVA:LOAD_RVA + memory_size]
        assert loaded[0x210:0x21a] == image[0x210:0x21a]
        assert not any(loaded[0x1000:])
        if os.name == "nt":
            executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
            assert executed.returncode == 42, f"exit code {executed.returncode:#x}"
        check_relocated_zero_fill(relinker, work)
        check_scattered_relocations_macos(relinker, work)
    print("Windows zero-filled memory tests passed")


def check_relocated_zero_fill(relinker, work):
    image = fixture()
    file_size = struct.unpack_from("<Q", image, 64 + 32)[0]
    memory_size = file_size + BSS
    struct.pack_into("<Q", image, 64 + 40, memory_size)
    slot = memory_size - 0x1000
    struct.pack_into("<i", image, 0x202, slot - 0x206)
    struct.pack_into("<Q", image, 0x700, slot)
    source = work / "relocated.elf"
    source.write_bytes(image)
    output = work / "relocated.exe"
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    pe = output.read_bytes()
    assert len(pe) < BSS // 16, f"{len(pe)} byte executable for {BSS} bytes of zero-filled memory"
    table, image_size = sections(pe)
    rva = LOAD_RVA + slot
    name, virtual_size, start, raw_size, raw = next(section for section in table if section[2] <= rva < section[2] + section[1])
    assert rva + 8 <= start + raw_size, (name, hex(rva), hex(start), raw_size)
    base = struct.unpack_from("<Q", pe, struct.unpack_from("<I", pe, 0x3c)[0] + 24 + 24)[0]
    assert struct.unpack_from("<Q", pe, raw + rva - start)[0] == base + LOAD_RVA + 0x210
    if os.name == "nt":
        executed = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
        assert executed.returncode == 42, f"exit code {executed.returncode:#x}"


def check_scattered_relocations_macos(relinker, work):
    image = fixture()
    file_size = struct.unpack_from("<Q", image, 64 + 32)[0]
    memory_size = file_size + (1 << 20)
    struct.pack_into("<Q", image, 64 + 40, memory_size)
    count = 40
    for index in range(count):
        struct.pack_into("<QQq", image, 0x700 + index * 24, file_size + index * 0x2000, 8, 0x210)
    tags = image[0x400:0x500]
    for offset in range(0, len(tags), 16):
        if struct.unpack_from("<q", tags, offset)[0] == 8:
            struct.pack_into("<Q", image, 0x400 + offset + 8, count * 24)
    source = work / "scattered.elf"
    source.write_bytes(image)
    output = work / "scattered.macho"
    result = subprocess.run([str(relinker), "--skip-sce-module", "--macos", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    macho = output.read_bytes()
    commands = struct.unpack_from("<I", macho, 16)[0]
    offset, segments = 32, 0
    for _ in range(commands):
        command, size = struct.unpack_from("<II", macho, offset)
        segments += command == 0x19
        offset += size
    assert segments <= 16, f"{segments} Mach-O segments"


if __name__ == "__main__":
    main()
