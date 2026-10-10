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


def fixture(header_count):
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
    struct.pack_into("<IIQQQQQQ", image, 176, *EH_FRAME_HEADER)
    for index in range(3, header_count):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    struct.pack_into("<BBBBQI", image, 0x4800, 1, 0, 3, 0, 0x900, 0)
    return image


def program_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    return [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


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
        image = fixture(7)
        result, elf = relink(relinker, directory, "spare-slot", image)
        assert "PT_GNU_EH_FRAME" not in result.stderr, result.stderr
        headers = program_headers(elf)
        frames = [header for header in headers if header[0] == PT_GNU_EH_FRAME]
        assert frames == [EH_FRAME_HEADER], ("PT_GNU_EH_FRAME must be kept unchanged", headers)
        offset, size = EH_FRAME_HEADER[2], EH_FRAME_HEADER[5]
        assert elf[offset:offset + size] == image[offset:offset + size], "eh_frame_hdr bytes changed"
        assert all(header[0] != PT_SCE_VERSION for header in headers), headers

        for header_count in (5, 6):
            result, elf = relink(relinker, directory, "full-slots-" + str(header_count), fixture(header_count))
            headers = program_headers(elf)
            assert all(header[0] != PT_GNU_EH_FRAME for header in headers), (header_count, headers)
            assert len(headers) == 6, (header_count, headers)
            assert "No free program header slot for PT_GNU_EH_FRAME" in result.stderr, (header_count, result.stderr)
    print("Linux eh_frame header test passed")


if __name__ == "__main__":
    main()
