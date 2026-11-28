"""Guest images for the macOS module-qualified import tests: two providers of one NID and a consumer that names one."""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from guest_import_fixtures import dynamic
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture

SYMBOL = "shared#A#B"


def provider(value):
    image = guest_fixture(PLAIN_SITE)
    strings = b"\0" + SYMBOL.encode() + b"\0"
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into("<II", image, 0x840, 1, 2)
    struct.pack_into("<IBBHQQ", image, 0x898, 1, 0x12, 0, 1, 0x1000, 6)
    image[0x400:0x406] = b"\xb8" + struct.pack("<I", value) + b"\xc3"
    dynamic(image, 0x600, 176, [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
                                (4, 0x2240), (7, 0x2300), (8, 0), (9, 24)])
    return image


def consumer(owner):
    image = guest_fixture(PLAIN_SITE)
    strings = b"\0" + SYMBOL.encode() + b"\0read\0" + owner.encode() + b"\0"
    image[0x800:0x800 + len(strings)] = strings
    owner_offset = 1 + len(SYMBOL) + 1 + len("read") + 1
    struct.pack_into("<II", image, 0x840, 1, 3)
    struct.pack_into("<IBBHQQ", image, 0x898, 1, 0x12, 0, 0, 0, 0)
    code = b"\xff\x25" + struct.pack("<i", 0x2380 - 0x1016)
    image[0x410:0x410 + len(code)] = code
    struct.pack_into("<IBBHQQ", image, 0x8b0, 1 + len(SYMBOL) + 1, 0x12, 0, 1, 0x1010, len(code))
    struct.pack_into("<QQq", image, 0x900, 0x2380, (1 << 32) | 6, 0)
    dynamic(image, 0x600, 176, [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
                                (4, 0x2240), (7, 0x2300), (8, 24), (9, 24),
                                (1, owner_offset), (0x61000045, (1 << 48) | owner_offset)])
    return image


def qualified_executable(names, modules):
    image = main_fixture()
    struct.pack_into("<I", image, 68, 7)
    strings = b"\0"
    symbol_offsets = []
    for name in names:
        symbol_offsets.append(len(strings))
        strings += name.encode() + b"\0"
    module_offsets = []
    for module in modules:
        module_offsets.append(len(strings))
        strings += module.encode() + b"\0"
    image[0x4800:0x4800 + len(strings)] = strings
    tags = [(5, 0x4800), (10, len(strings)), (6, 0x4880), (11, 24), (4, 0x4840), (7, 0x4900),
            (8, 24 * len(names)), (9, 24)]
    for index, offset in enumerate(module_offsets):
        tags += [(1, offset), (0x61000045, ((index + 1) << 48) | offset)]
    dynamic(image, 0x4600, 120, tags)
    chains = [0] + [index + 1 for index in range(1, len(names))] + [0]
    struct.pack_into("<" + "I" * (len(chains) + 3), image, 0x4840, 1, len(names) + 1, 1, *chains)
    for index, offset in enumerate(symbol_offsets):
        struct.pack_into("<IBBHQQ", image, 0x4898 + 24 * index, offset, 0x12, 0, 0, 0, 0)
        struct.pack_into("<QQq", image, 0x4900 + 24 * index, 0x4a20 + 8 * index, ((index + 1) << 32) | 6, 0)
    image[0x4000:0x400f] = b"\x48\x83\xec\x08\xff\x15" + struct.pack("<i", 0x4a20 - 0x400a) + b"\x48\x83\xc4\x08\xc3"
    return image


if __name__ == "__main__":
    open(sys.argv[1], "wb").write(provider(11))
