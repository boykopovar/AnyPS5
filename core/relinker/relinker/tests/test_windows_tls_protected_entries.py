"""Keep independently reachable code entries out of Windows TLS jump patches."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile


TLS_AND_FOLLOWER = bytes.fromhex("64 48 8b 08 b8 2a 00 00 00 c3")
SEPARATE_FUNCTION = bytes.fromhex("b8 2a 00 00 00 c3")


def make_image(root=None):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x200, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    image[0x200:0x206] = bytes.fromhex("ff 25 fa 00 00 00")
    image[0x210:0x21a] = TLS_AND_FOLLOWER
    image[0x600:0x60a] = b"\x00borrowed\x00"

    relocations = [(0x300, 8, 0, 0x210)]
    extra_tags = []
    if root in ("relative", "access-start", "after-patch"):
        target = {"relative": 0x214, "access-start": 0x210, "after-patch": 0x21a}[root]
        relocations.append((0x308, 8, 0, target))
    if root == "after-patch":
        image[0x21a:0x220] = SEPARATE_FUNCTION
    elif root == "symbol":
        struct.pack_into("<IBBHQQ", image, 0x638, 1, 0x12, 0, 1, 0x214, 6)
        extra_tags.append((0x6100003f, 48))
    elif root == "lea":
        image[0x200:0x20d] = bytes.fromhex("48 8d 05 0d 00 00 00 ff 25 f3 00 00 00")
    elif root == "initializer":
        struct.pack_into("<Q", image, 0x308, 0x214)
        extra_tags.extend(((25, 0x308), (27, 8)))
    elif root == "relr":
        struct.pack_into("<Q", image, 0x308, 0x214)
        struct.pack_into("<Q", image, 0x780, 0x308)
        extra_tags.extend(((36, 0x780), (35, 8), (37, 8)))

    for index, (slot, kind, symbol, addend) in enumerate(relocations):
        struct.pack_into("<QQq", image, 0x700 + index * 24, slot, (symbol << 32) | kind, addend)
    tags = [(5, 0x600), (10, 10), (6, 0x620), (11, 24),
            (7, 0x700), (8, len(relocations) * 24), (9, 24), *extra_tags, (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x980, 0x980, 0x980, 8, 8, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    return image


def pe_bytes_at(pe, rva, size):
    pe_header = struct.unpack_from("<I", pe, 0x3c)[0]
    sections = pe_header + 24 + struct.unpack_from("<H", pe, pe_header + 20)[0]
    for index in range(struct.unpack_from("<H", pe, pe_header + 6)[0]):
        section = sections + index * 40
        address, length, position = struct.unpack_from("<III", pe, section + 12)
        if address <= rva and rva + size <= address + length:
            start = position + rva - address
            return pe[start:start + size]
    raise AssertionError(f"PE RVA {rva:#x} is not file-backed")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-protected-") as directory:
        work = Path(directory)

        def convert(name):
            source = work / (name + ".elf")
            output = work / (name + ".exe")
            source.write_bytes(make_image(name))
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "unused-filter=0",
                                     str(source), str(output)], capture_output=True, text=True, timeout=30)
            return result, output

        for name in ("relative", "symbol", "lea", "initializer", "relr"):
            result, output = convert(name)
            assert result.returncode == 2, (name, result.returncode, result.stderr)
            assert "TLS rewrite would overwrite an executable entry point" in result.stderr, (name, result.stderr)
            assert "(offset 0x214)" in result.stderr, (name, result.stderr)
            assert not output.exists(), name

        for name in ("safe", "access-start", "after-patch"):
            result, output = convert(name)
            assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)
            pe = output.read_bytes()
            assert pe_bytes_at(pe, 0x10210, 1) == b"\xe9", name
            if name == "access-start":
                assert struct.unpack("<Q", pe_bytes_at(pe, 0x10308, 8))[0] == 0x400210, name
            if name == "after-patch":
                assert pe_bytes_at(pe, 0x1021a, 6) == SEPARATE_FUNCTION, name

    print("Windows TLS protected-entry integration tests passed")


if __name__ == "__main__":
    main()
