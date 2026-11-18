import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_guest_intel_trampolines import guest_fixture

AVX2 = 1 << 4
FMA = 1 << 5
F16C = 1 << 6


def elf_note(data):
    phoff, = struct.unpack_from("<Q", data, 32)
    entry_size, count = struct.unpack_from("<HH", data, 54)
    for index in range(count):
        header = struct.unpack_from("<IIQQQQQQ", data, phoff + index * entry_size)
        if header[0] != 4:
            continue
        offset, size = header[2], header[5]
        note = data[offset:offset + size]
        namesz, descsz, kind = struct.unpack_from("<III", note, 0)
        assert (namesz, descsz, kind) == (7, 8, 1), (namesz, descsz, kind)
        assert note[12:19] == b"AnyPS5\0"
        version, mask = struct.unpack_from("<II", note, 20)
        assert version == 1
        return mask
    return None


def pe_record(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional = struct.unpack_from("<H", data, pe + 20)[0]
    table = pe + 24 + optional
    for index in range(count):
        offset = table + index * 40
        name = data[offset:offset + 8].rstrip(b"\0")
        if name != b".aps5cpu":
            continue
        raw_size = struct.unpack_from("<I", data, offset + 16)[0]
        raw = struct.unpack_from("<I", data, offset + 20)[0]
        record = data[raw:raw + 16]
        assert record[:8] == b"A5CPU\0\0\0"
        version, mask = struct.unpack_from("<II", record, 8)
        assert version == 1
        return mask
    return None


def convert(relinker, work, code, windows, enabled=True):
    source = work / "input.elf"
    source.write_bytes(guest_fixture(code))
    output = work / ("output.exe" if windows else "output")
    command = [str(relinker), "--skip-sce-module"]
    if windows:
        command.append("--windows")
    if enabled:
        command.append("--to-avx")
    command.extend([str(source), str(output)])
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return output.read_bytes()


def check(relinker):
    cases = (
        ("gather", bytes.fromhex("C4 E2 79 90 04 08"), AVX2),
        ("vaddps", bytes.fromhex("C5 FC 58 C1"), 0),
        ("fma", bytes.fromhex("C4 E2 71 98 C2"), FMA),
        ("f16c", bytes.fromhex("C4 E2 79 13 C1"), F16C),
        ("nop", b"\x90", 0),
    )
    with tempfile.TemporaryDirectory(prefix="anyps5-required-extensions-") as directory:
        work = Path(directory)
        for name, code, expected in cases:
            linux = convert(relinker, work, code, False)
            assert elf_note(linux) == expected, (name, elf_note(linux), expected)
            windows = convert(relinker, work, code, True)
            assert pe_record(windows) == expected, (name, pe_record(windows), expected)
        plain_linux = convert(relinker, work, b"\x90", False, enabled=False)
        assert elf_note(plain_linux) is None, "a conversion without --to-avx wrote the Linux record"
        plain_windows = convert(relinker, work, b"\x90", True, enabled=False)
        assert pe_record(plain_windows) is None, "a conversion without --to-avx wrote the Windows record"
    print("Required extension record tests passed")


if __name__ == "__main__":
    check(Path(sys.argv[1]).resolve())
