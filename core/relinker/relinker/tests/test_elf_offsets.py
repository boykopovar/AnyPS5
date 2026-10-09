from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture
from test_linux_load_alignment import fixture as alignment_fixture, PT_SCE_VERSION
from test_linux_dynamic_strings import dynamic_strings


def translation_fixture(segment_offset, segment_size=0x2000, string_address=0x3600):
    data = alignment_fixture()
    struct.pack_into("<Q", data, 0x18, 0x10)
    data[0x4010:0x4016] = bytes.fromhex("b8 2a 00 00 00 c3")
    struct.pack_into("<H", data, 0x38, 6)
    struct.pack_into("<IIQQQQQQ", data, 64 + 2 * 56,
                     1, 4, segment_offset, 0x2000, 0x2000, segment_size, segment_size, 0x1000)
    struct.pack_into("<IIQQQQQQ", data, 64 + 5 * 56,
                     PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    strings = b"\0libkernel.prx\0"
    string_offset = segment_offset + string_address - 0x2000
    if 0 <= string_offset <= len(data) - len(strings):
        data[string_offset:string_offset + len(strings)] = strings
    alias = b"\0libwrongx.prx\0"
    data[0x600:0x600 + len(alias)] = alias
    tags = [(5, string_address), (10, len(strings)), (6, 0x620), (11, 24),
            (7, 0x700), (8, 0), (9, 24), (1, 1), (0, 0)]
    struct.pack_into("<IIQQQQQQ", data, 120,
                     2, 6, 0x4600, 0x600, 0x600, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", data, 0x4600 + index * 16, *tag)
    return data


def check_translation(relinker, directory):
    cases = [
        ("translation-valid", 0x1000, 0x2000, 0x3600, None),
        ("translation-first-byte", 0x1000, 0x2000, 0x2000, None),
        ("translation-segment-ends-at-eof", 0x6000, 0x2000, 0x3600, None),
        ("translation-offset-wrap", 0xfffffffffffff000, 0x2000, 0x3600, "Segment offset out of bounds"),
        ("translation-offset-past-eof", 0x9000, 0x2000, 0x3600, "Segment offset out of bounds"),
        ("translation-range-past-eof", 0x7000, 0x2000, 0x3600, "Segment offset out of bounds"),
        ("translation-address-in-truncated-segment", 0x6000, 0x3000, 0x2400, "Segment offset out of bounds"),
        ("translation-huge-file-size", 0x1000, 0xffffffffffff0000, 0x3600, "Segment offset out of bounds"),
        ("translation-file-end", 0x1000, 0x2000, 0x4000, "Virtual address not mapped by any PT_LOAD segment"),
        ("translation-empty-segment", 0x1000, 0, 0x2000, "Virtual address not mapped by any PT_LOAD segment"),
    ]
    for name, offset, size, address, error in cases:
        source = Path(directory) / (name + ".elf")
        source.write_bytes(translation_fixture(offset, size, address))
        for mode in ([], ["--windows"]):
            output = Path(directory) / (name + (".exe" if mode else ".out"))
            registry = output.with_suffix(".registry.json")
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *mode, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if error is not None:
                assert result.returncode == 2 and error in result.stderr and not output.exists() and not registry.exists(), (
                    name, mode, result.returncode, result.stdout, result.stderr)
                continue
            assert result.returncode == 0 and output.exists(), (name, mode, result.stdout, result.stderr)
            converted = output.read_bytes()
            if mode:
                assert converted.startswith(b"MZ"), (name, mode)
            else:
                assert converted.startswith(b"\x7fELF"), (name, mode)
                tags, strings = dynamic_strings(converted)
                needed = tags[1]
                assert strings[needed:strings.index(b"\0", needed)] == b"libkernel.prx", (name, tags, strings)


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
        check_translation(relinker, directory)
    print("ELF offset tests passed")


if __name__ == "__main__":
    main()
