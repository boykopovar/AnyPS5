"""Check that a Linux relink keeps the image base aligned to the guest segment alignment."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

PT_LOAD = 1
PT_SCE_VERSION = 0x6FFFFF01


def fixture():
    image = bytearray(0x8000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x4000, 64, 0, 0, 64, 56, 5, 64, 0, 0)
    image[0x4000:0x4006] = b"\xb8\x2a\x00\x00\x00\xc3"
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24), (7, 0x700), (8, 0), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     PT_LOAD, 5, 0x4000, 0, 0, 0x1000, 0x1000, 0x4000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x600 + 0x4000, 0x600, 0x600, len(tags) * 16, len(tags) * 16, 8)
    for index in range(2, 5):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    return image


def loads(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    headers = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]
    return [header for header in headers if header[0] == PT_LOAD]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-align-") as directory:
        source = Path(directory) / "input.elf"
        output = Path(directory) / "output.elf"
        source.write_bytes(fixture())
        missing = subprocess.run([str(relinker), str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert missing.returncode != 0 and not output.exists(), missing
        assert "sce_module/sce_modules/prx was not found" in missing.stderr, missing.stderr
        assert "--skip-sce-module only if this game" in missing.stderr, missing.stderr
        result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)], capture_output=True, text=True, timeout=20)
        if result.returncode != 0:
            raise AssertionError((result.returncode, result.stdout, result.stderr))
        segments = loads(output.read_bytes())
        first = segments[0]
        alignment = max(segment[7] for segment in segments)
        if alignment != 0x4000 or first[3] % alignment != 0 or first[7] != alignment:
            raise AssertionError(("first PT_LOAD is not aligned to the largest segment alignment", segments))
        assert "output.elf\nlibs/\n    *.prx\napp0/" in result.stdout, result.stdout
        named_exe = Path(directory) / "linux.EXE"
        warning = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(named_exe)], capture_output=True, text=True, timeout=20)
        assert warning.returncode == 0, (warning.stdout, warning.stderr)
        assert "--windows was not specified" in warning.stderr, warning.stderr
        assert named_exe.read_bytes().startswith(b"\x7fELF"), "The filename must not select the output format"

        maximum = (1 << 64) - 1

        def bss_fixture(address, size, alignment=0x1000, file_size=0x8000, header_offset=64):
            data = fixture()
            data.extend(bytes(file_size - len(data)))
            struct.pack_into("<H", data, 0x38, 6)
            struct.pack_into("<IIQQQQQQ", data, 176,
                             PT_LOAD, 4, 0, address, address, 0, size, alignment)
            struct.pack_into("<IIQQQQQQ", data, 64 + 5 * 56,
                             PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
            if header_offset != 64:
                data[header_offset:header_offset + 6 * 56] = data[64:64 + 6 * 56]
                struct.pack_into("<Q", data, 0x20, header_offset)
            return data

        cases = (
            ("valid-bss", bss_fixture(0x2000, 0x4000), None),
            ("valid-high-address", bss_fixture(0x2000, maximum - 0x11fff), None),
            ("load-range-wrap", bss_fixture(0x2000, maximum), "PT_LOAD virtual address range overflows"),
            ("extra-range-wrap", bss_fixture(0x2000, maximum - 0x2080, file_size=0x8ff5),
             "Extra block virtual address range overflows"),
            ("header-alignment-wrap", bss_fixture(0, 1 << 63, 1 << 63),
             "Header block virtual address alignment overflows"),
            ("header-range-wrap", bss_fixture(0x2000, maximum - 0x9fff, header_offset=0x4000),
             "Header block virtual address range overflows"),
        )
        for name, data, error in cases:
            source = Path(directory) / (name + ".elf")
            output = Path(directory) / (name + ".out")
            source.write_bytes(data)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if error is None:
                assert result.returncode == 0 and output.exists(), (name, result)
                assert all(segment[3] + segment[6] <= maximum for segment in loads(output.read_bytes()))
            else:
                assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, result)
    print("Linux load alignment test passed")


if __name__ == "__main__":
    main()
