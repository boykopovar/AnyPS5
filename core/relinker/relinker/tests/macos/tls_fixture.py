"""A PS5-style ELF that uses static TLS through %fs and exits with a value computed from it."""
import struct, sys
sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
from nid import nid

def fixture():
    image = bytearray(0x6000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x4000, 64, 0, 0, 64, 56, 4, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x4000, 0x4000, 0x4000, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 6, 0x5000, 0x5000, 0x5000, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x5200, 0x5200, 0x5200, 8, 16, 8)
    struct.pack_into("<Q", image, 0x5200, 0x2a)
    code = bytearray()
    code += b"\x48\x83\xec\x08"
    code += b"\x31\xc0"
    code += b"\x64\x48\x8b\x04\x25\x00\x00\x00\x00"
    jnz = len(code); code += b"\x0f\x85\x00\x00\x00\x00"
    code += b"\x48\x8b\x78\xf0"
    code += b"\x64\x48\x8b\x0c\x25\xf0\xff\xff\xff"
    code += b"\x48\x01\xcf"
    code += b"\x64\xc7\x04\x25\x28\x00\x00\x00\x34\x12\x00\x00"
    code += b"\x64\x48\x8b\x14\x25\x28\x00\x00\x00"
    code += b"\x48\x01\xd7"
    call = len(code); code += b"\xff\x15" + struct.pack("<i", 0x5000 - (0x4000 + call + 6))
    bad = len(code); code += b"\x0f\x0b"
    struct.pack_into("<i", code, jnz + 2, bad - (jnz + 6))
    image[0x4000:0x4000 + len(code)] = code
    strings = b"\0libc.prx\0" + (nid("exit") + "#A#B").encode() + b"\0"
    image[0x5A00:0x5A00 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", image, 0x5B00 + 24, 10, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x5C00, 0x5000, (1 << 32) | 6, 0)
    tags = [(1, 1), (5, 0x5A00), (10, len(strings)), (6, 0x5B00), (11, 24), (0x6100003f, 48), (7, 0x5C00), (8, 24), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x5800 + index * 16, *tag)
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x5800, 0x5800, 0x5800, len(tags) * 16, len(tags) * 16, 8)
    return image

if __name__ == "__main__":
    open(sys.argv[1], "wb").write(fixture())
