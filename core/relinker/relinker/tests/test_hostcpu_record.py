"""Check that --to-avx records the host CPU extensions its output still needs."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections

PT_OS_HOSTCPU = 0x61000002
FMA = bytes.fromhex("c4 e2 71 98 c1")
AVX2 = bytes.fromhex("c5 f5 fe c2")
RDSEED = bytes.fromhex("0f c7 f8")
TEXT = FMA + AVX2 + RDSEED + b"\x90\xc3"
MAGIC = b"APS5HCP1"
FMA_BIT = 1 << 5
RDSEED_BIT = 1 << 8


def program_headers(elf):
    offset, = struct.unpack_from("<Q", elf, 32)
    size, count = struct.unpack_from("<HH", elf, 54)
    return [struct.unpack_from("<IIQQQQQQ", elf, offset + index * size) for index in range(count)]


def convert(relinker, work, name, *options):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    image = bytearray(main_fixture())
    image[0x4000:0x4000 + len(TEXT)] = TEXT
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return output.read_bytes()


def check_linux(relinker, work):
    plain = convert(relinker, work, "linux-plain")
    assert not [header for header in program_headers(plain) if header[0] == PT_OS_HOSTCPU], "A plain Linux conversion carried a host CPU record"
    linux = convert(relinker, work, "linux-avx", "--to-avx")
    records = [header for header in program_headers(linux) if header[0] == PT_OS_HOSTCPU]
    assert len(records) == 1, records
    _, flags, offset, address, _, file_size, memory_size, alignment = records[0]
    assert flags == 4 and file_size == 16 and memory_size == 16 and alignment == 8, records[0]
    stored = linux[offset:offset + 16]
    assert stored[:8] == MAGIC, stored
    mask, = struct.unpack_from("<Q", stored, 8)
    assert mask == FMA_BIT | RDSEED_BIT, hex(mask)
    loads = [header for header in program_headers(linux) if header[0] == 1]
    assert any(load[3] <= address and address + 16 <= load[3] + load[5] for load in loads), (address, loads)


def check_windows(relinker, work):
    plain = convert(relinker, work, "windows-plain", "--windows")
    assert not [section for section in pe_sections(plain) if section[0] == b".hostcpu"], "A plain Windows conversion carried a host CPU record"
    windows = convert(relinker, work, "windows-avx", "--windows", "--to-avx")
    records = [section for section in pe_sections(windows) if section[0] == b".hostcpu"]
    assert len(records) == 1, records
    _, _, _, raw_offset, flags = records[0]
    assert flags & 0x40000000 and not flags & 0x80000000, hex(flags)
    header, = struct.unpack_from("<I", windows, 0x3C)
    optional_size, = struct.unpack_from("<H", windows, header + 20)
    index = [section[0] for section in pe_sections(windows)].index(b".hostcpu")
    virtual_size, = struct.unpack_from("<I", windows, header + 24 + optional_size + index * 40 + 8)
    assert virtual_size == 16, virtual_size
    stored = windows[raw_offset:raw_offset + 16]
    assert stored[:8] == MAGIC, stored
    mask, = struct.unpack_from("<Q", stored, 8)
    assert mask == FMA_BIT | RDSEED_BIT, hex(mask)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-hostcpu-") as directory:
        work = Path(directory)
        check_linux(relinker, work)
        check_windows(relinker, work)
    print("Host CPU record tests passed")


if __name__ == "__main__":
    main()
