"""A minimal PS5-style ELF that imports puts and exit from libc.prx by NID."""
import struct, sys
sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
from nid import nid

def fixture(message=b"hello from a PS5 ELF on macOS\0"):
    image = bytearray(0x6000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x4000, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x4000, 0x4000, 0x4000, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 6, 0x5000, 0x5000, 0x5000, 0x1000, 0x1000, 0x1000)
    code = bytearray(b"\x48\x83\xec\x08")
    code += b"\x48\x8d\x3d" + struct.pack("<i", 0x5100 - (0x4004 + 7))
    code += b"\xff\x15" + struct.pack("<i", 0x5000 - (0x400b + 6))
    code += b"\x31\xff"
    code += b"\xff\x15" + struct.pack("<i", 0x5008 - (0x4013 + 6))
    code += b"\x0f\x0b"
    image[0x4000:0x4000 + len(code)] = code
    image[0x5100:0x5100 + len(message)] = message
    strings = b"\0libc.prx\0" + (nid("puts") + "#A#B").encode() + b"\0" + (nid("exit") + "#A#B").encode() + b"\0"
    image[0x5A00:0x5A00 + len(strings)] = strings
    puts_name = strings.index(b"\0", 1) + 1
    exit_name = strings.index(b"\0", puts_name) + 1
    struct.pack_into("<IBBHQQ", image, 0x5B00 + 24, puts_name, 0x12, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x5B00 + 48, exit_name, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x5C00, 0x5000, (1 << 32) | 6, 0)
    struct.pack_into("<QQq", image, 0x5C18, 0x5008, (2 << 32) | 6, 0)
    tags = [(1, 1), (5, 0x5A00), (10, len(strings)), (6, 0x5B00), (11, 24), (0x6100003f, 72), (7, 0x5C00), (8, 48), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x5800 + index * 16, *tag)
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x5800, 0x5800, 0x5800, len(tags) * 16, len(tags) * 16, 8)
    return image

if __name__ == "__main__":
    open(sys.argv[1], "wb").write(fixture())
