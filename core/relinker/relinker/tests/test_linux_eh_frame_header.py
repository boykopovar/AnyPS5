from pathlib import Path
import struct
import subprocess
import sys
import tempfile

PT_LOAD = 1
PT_DYNAMIC = 2
PT_GNU_EH_FRAME = 0x6474E550
PT_SCE_VERSION = 0x6FFFFF01
EH_FRAME_HEADER = (PT_GNU_EH_FRAME, 4, 0x4800, 0x800, 0x800, 16, 16, 4)


def fixture(header_count, with_frame=True):
    image = bytearray(0x8000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x4000, 64, 0, 0, 64, 56, header_count, 64, 0, 0)
    image[0x4000:0x4006] = b"\xb8\x2a\x00\x00\x00\xc3"
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24), (7, 0x700), (8, 0), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     PT_LOAD, 5, 0x4000, 0, 0, 0x1000, 0x1000, 0x4000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     PT_DYNAMIC, 6, 0x600 + 0x4000, 0x600, 0x600, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     *(EH_FRAME_HEADER if with_frame else (PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)))
    for index in range(3, header_count):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    struct.pack_into("<BBBBQI", image, 0x4800, 1, 0, 3, 0, 0x900, 0)
    return image


def program_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    headers = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]
    phdr = next(header for header in headers if header[0] == 6)
    assert phdr[5:7] == (phnum * phentsize, phnum * phentsize), ("PT_PHDR must cover every output header", phdr, phnum)
    header_load = next(header for header in headers if header[0] == PT_LOAD and header[2] == 0)
    assert header_load[5:7] == (phoff + phnum * phentsize, phoff + phnum * phentsize), ("Header PT_LOAD must cover every output header", header_load, phnum)
    return headers


def relink(relinker, directory, name, image):
    source = Path(directory) / (name + ".elf")
    output = Path(directory) / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)], capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
    return result, output.read_bytes()


def header_bounds(relinker, directory):
    encodings = (("fixed", struct.pack("<BBBBQI", 1, 0, 3, 0, 0x900, 0)),
                 ("leb", bytes((1, 1, 1, 0, 0x80, 0x12, 0x80, 0))))
    for mode, flags in (("linux-strict", ["unused-filter=2"]), ("windows", ["--windows"])):
        for encoding, data in encodings:
            for size in (len(data), *range(1, len(data))):
                image = fixture(7)
                struct.pack_into("<Q", image, 24, 0x100)
                image[0x4100:0x4106] = image[0x4000:0x4006]
                struct.pack_into("<qQqQ", image, 0x4670, 4, 0x6A0, 0, 0)
                struct.pack_into("<QQ", image, 120 + 32, 9 * 16, 9 * 16)
                struct.pack_into("<IIII", image, 0x46A0, 1, 1, 0, 0)
                image[0x4800:0x4800 + len(data)] = data
                struct.pack_into("<QQ", image, 176 + 32, size, size)
                name = f"bounds-{mode}-{encoding}-{size}"
                source = Path(directory) / (name + ".elf")
                output = Path(directory) / (name + ".out")
                source.write_bytes(image)
                result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert source.read_bytes() == image, (name, "source changed")
                if size == len(data):
                    assert result.returncode == 0 and output.is_file(), (name, result.returncode, result.stderr)
                else:
                    assert result.returncode == 2, (name, result.returncode, result.stdout, result.stderr)
                    assert "EH frame header exceeds segment" in result.stderr, (name, result.stderr)
                    assert not output.exists(), (name, "output written for a truncated EH frame header")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-eh-frame-") as directory:
        header_bounds(relinker, directory)
        for header_count in (7, 8):
            image = fixture(header_count)
            result, elf = relink(relinker, directory, "available-slots-" + str(header_count), image)
            assert "PT_GNU_EH_FRAME" not in result.stderr, result.stderr
            headers = program_headers(elf)
            frames = [header for header in headers if header[0] == PT_GNU_EH_FRAME]
            assert frames == [EH_FRAME_HEADER], ("PT_GNU_EH_FRAME must be kept unchanged", headers)
            offset, size = EH_FRAME_HEADER[2], EH_FRAME_HEADER[5]
            assert elf[offset:offset + size] == image[offset:offset + size], "eh_frame_hdr bytes changed"
            assert all(header[0] != PT_SCE_VERSION for header in headers), headers

        result, elf = relink(relinker, directory, "full-slots", fixture(6))
        headers = program_headers(elf)
        assert all(header[0] != PT_GNU_EH_FRAME for header in headers), headers
        assert len(headers) == 6, headers
        assert "No free program header slot for PT_GNU_EH_FRAME" in result.stderr, result.stderr

        image = fixture(6, with_frame=False)
        table_end = 64 + 6 * 56
        image[table_end:table_end + 56] = bytes([0xa5]) * 56
        result, elf = relink(relinker, directory, "no-frame-exact-slots", image)
        headers = program_headers(elf)
        assert len(headers) == 6, headers
        assert "PT_GNU_EH_FRAME" not in result.stderr, result.stderr
        assert elf[table_end:table_end + 56] == image[table_end:table_end + 56], "Bytes after the program header table changed"

        for with_frame in (False, True):
            source = Path(directory) / ("insufficient-slots-" + str(with_frame) + ".elf")
            output = source.with_suffix(".out")
            image = fixture(5, with_frame=with_frame)
            image[64 + 5 * 56:64 + 6 * 56] = bytes([0xa5]) * 56
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)], capture_output=True, text=True, timeout=20)
            assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
            assert "Not enough program header slots: need 6, available 5" in result.stderr, result.stderr
            assert not output.exists(), "Insufficient program header slots must not produce an executable"
            assert source.read_bytes() == image, "Input changed while rejecting insufficient program header slots"

        image = fixture(5, with_frame=False)
        headers = image[64:64 + 4 * 56]
        headers += struct.pack("<IIQQQQQQ", 4, 0, 0, 0, 0, 0, 0, 1) * 65530
        struct.pack_into("<Q", image, 32, len(image))
        struct.pack_into("<H", image, 56, 65534)
        image += headers
        source = Path(directory) / "overflowing-header-count.elf"
        output = source.with_suffix(".out")
        source.write_bytes(image)
        result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 2, (result.returncode, result.stdout, result.stderr)
        assert "Not enough program header slots: need 65536, available 65534" in result.stderr, result.stderr
        assert not output.exists(), "An unrepresentable header count must not produce an executable"
    print("Linux eh_frame header test passed")


if __name__ == "__main__":
    main()
