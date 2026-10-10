from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, elf_loads, guest_fixture, main_fixture
from test_windows_import_modules import consumer, provider

DYNAMIC = 0x4800
STRINGS = 0x4A00
SYMBOLS = 0x4B00
RELOCATIONS = 0x4C00
GOT = 0x4E00


def string_table(names):
    strings = b"\0"
    offsets = []
    for name in names:
        offsets.append(len(strings))
        strings += name.encode() + b"\0"
    return strings, offsets


def guest_module(symbols):
    image = guest_fixture(PLAIN_SITE)
    strings, offsets = string_table(name for name, _ in symbols)
    assert len(strings) <= 0x40
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into("<Q", image, 0x618, len(strings))
    struct.pack_into("<Q", image, 0x628, 0x2280)
    struct.pack_into("<II", image, 0x840, 1, len(symbols) + 1)
    for index, (offset, (_, exported)) in enumerate(zip(offsets, symbols), 1):
        struct.pack_into("<IBBHQQ", image, 0x880 + index * 24, offset, 0x12, 0,
                         1 if exported else 0, 0x1000 if exported else 0, 1 if exported else 0)
    return image


def importing_executable(names, modules=()):
    image = main_fixture()
    needed = [module + ".prx" for module in modules]
    strings, offsets = string_table([*names, *needed, *modules])
    image[STRINGS:STRINGS + len(strings)] = strings
    for index, offset in enumerate(offsets[:len(names)], 1):
        struct.pack_into("<IBBHQQ", image, SYMBOLS + index * 24, offset, 0x12, 0, 0, 0, 0)
        struct.pack_into("<QQq", image, RELOCATIONS + (index - 1) * 24, GOT + (index - 1) * 8, (index << 32) | 6, 0)
    tags = [(1, offset) for offset in offsets[len(names):len(names) + len(needed)]]
    tags += [(0x61000045, (index << 48) | offset) for index, offset in enumerate(offsets[len(names) + len(needed):], 1)]
    tags += [(5, STRINGS), (10, len(strings)), (6, SYMBOLS), (11, 24),
             (7, RELOCATIONS), (8, len(names) * 24), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC + index * 16, *tag)
    struct.pack_into("<QQQQQ", image, 120 + 8, DYNAMIC, DYNAMIC, DYNAMIC, len(tags) * 16, len(tags) * 16)
    return image


def dynamic_tags(image):
    phoff, = struct.unpack_from("<Q", image, 32)
    phsize, phcount = struct.unpack_from("<HH", image, 54)
    dynamic = next(struct.unpack_from("<IIQQQQQQ", image, phoff + index * phsize)
                   for index in range(phcount)
                   if struct.unpack_from("<I", image, phoff + index * phsize)[0] == 2)
    tags = {}
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == 0:
            break
        tags.setdefault(tag, value)
    return tags


def file_offset(image, address):
    for header in elf_loads(image):
        if header[3] <= address < header[3] + header[5]:
            return header[2] + address - header[3]
    raise AssertionError(f"Unmapped address: {address:#x}")


def symbol_name(image, tags, index):
    strings = file_offset(image, tags[5])
    name, = struct.unpack_from("<I", image, file_offset(image, tags[6]) + index * 24)
    return image[strings + name:image.index(0, strings + name)].decode()


def module_symbols(image):
    tags = dynamic_tags(image)
    count, = struct.unpack_from("<I", image, file_offset(image, tags[4]) + 4)
    return [symbol_name(image, tags, index) for index in range(1, count)]


def elf_hash(name):
    value = 0
    for byte in name.encode():
        value = (value << 4) + byte
        value = (value ^ ((value & 0xF0000000) >> 24)) & 0x0FFFFFFF
    return value


def check_hash_table(image):
    tags = dynamic_tags(image)
    table = file_offset(image, tags[4])
    buckets, count = struct.unpack_from("<II", image, table)
    assert buckets == count, (buckets, count)
    for index in range(1, count):
        name = symbol_name(image, tags, index)
        found, = struct.unpack_from("<I", image, table + 8 + elf_hash(name) % buckets * 4)
        while found not in (0, index):
            found, = struct.unpack_from("<I", image, table + 8 + (buckets + found) * 4)
        assert found == index, name


def imported_symbols(image):
    tags = dynamic_tags(image)
    names = set()
    for address, size in ((7, 8), (23, 2)):
        if address not in tags or tags.get(size, 0) == 0:
            continue
        table = file_offset(image, tags[address])
        for position in range(table, table + tags[size], 24):
            info, = struct.unpack_from("<Q", image, position + 8)
            if info >> 32:
                names.add(symbol_name(image, tags, info >> 32))
    return names


def relink(relinker, case, executable, modules):
    (case / "sce_module").mkdir(parents=True)
    source = case / "input.elf"
    source.write_bytes(executable)
    for name, image in modules.items():
        (case / "sce_module" / name).write_bytes(image)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), str(source), str(output)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return output


def convert(relinker, case):
    return relink(relinker, case, importing_executable(["AAAAAAAAAAA#A#A", "DDDDDDDDDDD#D#D"]), {
        "libc.prx": guest_module([("AAAAAAAAAAA#A#A", True), ("BBBBBBBBBBB#B#B", False), ("CCCCCCCCCCC#C#C", False), ("EEEEEEEEEEE", True)]),
        "other.prx": guest_module([("BBBBBBBBBBB#B#B", True), ("EEEEEEEEEEE", True)]),
    })


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-symbol-names-") as directory:
        work = Path(directory)

        output = convert(relinker, work)
        modules = output.parent / "app0" / "sce_module"
        libc = module_symbols((modules / "libc.prx.guest.prx").read_bytes())
        assert libc == ["AAAAAAAAAAA#guest", "BBBBBBBBBBB#guest", "CCCCCCCCCCC", "EEEEEEEEEEE#guest"], libc
        other = module_symbols((modules / "other.prx.guest.prx").read_bytes())
        assert other == ["BBBBBBBBBBB#guest", "EEEEEEEEEEE#guest"], other
        check_hash_table((modules / "libc.prx.guest.prx").read_bytes())
        check_hash_table((modules / "other.prx.guest.prx").read_bytes())
        imports = imported_symbols(output.read_bytes())
        assert imports == {"AAAAAAAAAAA#guest", "DDDDDDDDDDD"}, imports

        declared = relink(relinker, work / "declared",
                          importing_executable(["AAAAAAAAAAA#B#B", "EEEEEEEEEEE#C#C"], ["libc", "libSceLibcInternal"]),
                          {"libc.prx": guest_module([("AAAAAAAAAAA", True), ("EEEEEEEEEEE", True)])})
        imports = imported_symbols(declared.read_bytes())
        assert imports == {"AAAAAAAAAAA#guest", "EEEEEEEEEEE"}, imports

        guests = relink(relinker, work / "guest-declared", importing_executable([]), {
            "b.prx": provider(7),
            "c.prx": consumer("libSceLibcInternal.prx", module_name="libSceLibcInternal"),
        }).parent / "app0" / "sce_module"
        exporter = module_symbols((guests / "b.prx.guest.prx").read_bytes())
        assert exporter == ["shared#guest"], exporter
        importer = module_symbols((guests / "c.prx.guest.prx").read_bytes())
        assert importer == ["shared"], importer
    print("Guest symbol name tests passed")


if __name__ == "__main__":
    main()
