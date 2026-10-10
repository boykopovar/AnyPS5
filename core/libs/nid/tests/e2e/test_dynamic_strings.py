import re
import struct
import sys
from pathlib import Path

DT_NULL = 0
DT_NEEDED = 1
DT_SONAME = 14
DT_RPATH = 15
DT_RUNPATH = 29
SHT_DYNAMIC = 6


def dynamic_strings(image):
    shoff, = struct.unpack_from("<Q", image, 0x28)
    shentsize, shnum = struct.unpack_from("<HH", image, 0x3A)
    sections = [struct.unpack_from("<IIQQQQIIQQ", image, shoff + index * shentsize) for index in range(shnum)]
    dynamic = next(section for section in sections if section[1] == SHT_DYNAMIC)
    strings = sections[dynamic[6]]
    entries = []
    for position in range(dynamic[4], dynamic[4] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == DT_NULL:
            break
        if tag in (DT_NEEDED, DT_SONAME, DT_RPATH, DT_RUNPATH):
            assert value < strings[5], (tag, value)
            start = strings[4] + value
            entries.append((tag, image[start:image.index(0, start)].decode()))
    return entries


def main():
    entries = dynamic_strings(Path(sys.argv[1]).read_bytes())
    runpaths = [value for tag, value in entries if tag in (DT_RPATH, DT_RUNPATH)]
    assert runpaths == ["$ORIGIN"], entries
    needed = [value for tag, value in entries if tag == DT_NEEDED]
    assert needed and all(re.fullmatch(r"[\w.+-]+\.so(\.\d+)*", name) for name in needed), entries
    print("NID patcher dynamic string tests passed")


if __name__ == "__main__":
    main()
