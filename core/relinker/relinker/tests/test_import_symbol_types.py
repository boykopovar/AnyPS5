from pathlib import Path
import struct
import sys
import tempfile

from test_guest_symbol_names import (DYNAMIC, GOT, RELOCATIONS, STRINGS, SYMBOLS, dynamic_tags, file_offset, relink,
                                     string_table, symbol_name)
from test_guest_intel_trampolines import main_fixture

STT_OBJECT = 1
STT_FUNC = 2


def importing_executable(imports):
    image = main_fixture()
    strings, offsets = string_table(name for name, _ in imports)
    image[STRINGS:STRINGS + len(strings)] = strings
    for index, (offset, (_, kind)) in enumerate(zip(offsets, imports), 1):
        struct.pack_into("<IBBHQQ", image, SYMBOLS + index * 24, offset, 0x10 | kind, 0, 0, 0, 0)
        struct.pack_into("<QQq", image, RELOCATIONS + (index - 1) * 24, GOT + (index - 1) * 8, (index << 32) | 6, 0)
    tags = [(5, STRINGS), (10, len(strings)), (6, SYMBOLS), (11, 24), (7, RELOCATIONS), (8, len(imports) * 24), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC + index * 16, *tag)
    struct.pack_into("<QQQQQ", image, 120 + 8, DYNAMIC, DYNAMIC, DYNAMIC, len(tags) * 16, len(tags) * 16)
    return image


def imported_types(image):
    tags = dynamic_tags(image)
    symbols = file_offset(image, tags[6])
    types = {}
    for address, size in ((7, 8), (23, 2)):
        if address not in tags or tags.get(size, 0) == 0:
            continue
        table = file_offset(image, tags[address])
        for position in range(table, table + tags[size], 24):
            info, = struct.unpack_from("<Q", image, position + 8)
            index = info >> 32
            if index:
                types[symbol_name(image, tags, index)] = image[symbols + index * 24 + 4] & 15
    return types


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-import-types-") as directory:
        output = relink(relinker, Path(directory), importing_executable([("FFFFFFFFFFF", STT_FUNC), ("DDDDDDDDDDD", STT_OBJECT)]), {})
        types = imported_types(output.read_bytes())
        assert types == {"FFFFFFFFFFF": STT_FUNC, "DDDDDDDDDDD": STT_OBJECT}, types
    print("Import symbol type tests passed")


if __name__ == "__main__":
    main()
