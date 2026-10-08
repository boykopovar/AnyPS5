import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SHT_NULL = 0
SHT_STRTAB = 3
SHT_DYNSYM = 11
SHT_GNU_HASH = 0x6FFFFFF6
SHT_GNU_VERDEF = 0x6FFFFFFD
SHT_GNU_VERSYM = 0x6FFFFFFF
SHN_UNDEF = 0
SHN_ABS = 0xFFF1
STB_GLOBAL = 1
STT_FUNC = 2

LIBRARY_NAME = "libsymbols"
DYNSYM_OFFSET = 0x200
DYNSTR_OFFSET = 0x300
HASH_OFFSET = 0x500
VERSYM_OFFSET = 0x580
VERDEF_OFFSET = 0x600
SHOFF = 0x800
BUCKETS = 4
HASHED_FROM = 3
NID_NAME = re.compile(r"^[A-Za-z0-9+-]{11}$")

SONAME = "libsymbols.so"
VERSIONS = [(1, [SONAME]), (2, ["VERS_ONE"]), (3, ["VERS_TWO", "VERS_ONE"])]
MARKERS = [("VERS_ONE", 2, 0x1100), ("VERS_TWO", 3, 0x1101)]
DEFINITIONS = [
    ("sceAlphaComputeLength", 2, 0x1200),
    ("sceAlphaCompareStrings", 2, 0x1220),
    ("sceAlphaFindCharacter", 2, 0x1240),
    ("sceBetaCopyString", 3, 0x1260),
    ("sceBetaConcatenateStrings", 3, 0x1280),
    ("sceBetaSearchCharacter", 3, 0x12A0),
]
IMPORTS = ["jeJEvW13vxk", "e0m0E5Sq2N8"]


def gnu_hash(name):
    value = 5381
    for byte in name.encode("utf-8"):
        value = (value * 33 + byte) & 0xFFFFFFFF
    return value


def symbols(versioned):
    entries = [(None, 1, SHN_UNDEF, 0, 0)]
    entries += [(name, 1, SHN_UNDEF, 0, 0) for name in IMPORTS]
    if versioned:
        entries += [(name, version, SHN_ABS, 0, value) for name, version, value in MARKERS]
        entries += [(name, version, 9, STT_FUNC, value) for name, version, value in DEFINITIONS]
    else:
        entries += [(name, 1, 9, STT_FUNC, value) for name, _, value in DEFINITIONS]
    return entries


def string_table(names):
    offsets = {}
    table = bytearray(b"\x00")
    for name in names:
        offsets.setdefault(name, len(table))
        table += name.encode("utf-8") + b"\x00"
    return table, offsets


def build_image(entries, versioned):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    names = [entry[0] for entry in entries if entry[0]]
    if versioned:
        names += [name for _, version_names in VERSIONS for name in version_names]
    strings, offsets = string_table(names)
    for index, (name, version, shndx, symbol_type, value) in enumerate(entries):
        info = (STB_GLOBAL << 4) | symbol_type
        struct.pack_into("<IBBHQQ", image, DYNSYM_OFFSET + index * 24,
                         offsets[name] if name else 0, info, 0, shndx, value, 0x10 if symbol_type else 0)
    image[DYNSTR_OFFSET:DYNSTR_OFFSET + len(strings)] = strings

    chain_count = len(entries) - HASHED_FROM
    bucket_offset = HASH_OFFSET + 16 + 8
    chain_offset = bucket_offset + BUCKETS * 4
    struct.pack_into("<IIII", image, HASH_OFFSET, BUCKETS, HASHED_FROM, 1, 6)
    for index in range(HASHED_FROM, len(entries)):
        name = entries[index][0]
        bucket = gnu_hash(name) % BUCKETS
        if struct.unpack_from("<I", image, bucket_offset + bucket * 4)[0] == 0:
            struct.pack_into("<I", image, bucket_offset + bucket * 4, index)
        chain = gnu_hash(name)
        if index + 1 == len(entries) or gnu_hash(entries[index + 1][0]) % BUCKETS != bucket:
            chain |= 1
        struct.pack_into("<I", image, chain_offset + (index - HASHED_FROM) * 4, chain)

    verdef_end = 0
    if versioned:
        for index, (_, version, _, _, _) in enumerate(entries):
            struct.pack_into("<H", image, VERSYM_OFFSET + index * 2, version)

        position = VERDEF_OFFSET
        for version_index, version_names in VERSIONS:
            entry_size = 20 + 8 * len(version_names)
            struct.pack_into("<HHHHIII", image, position, 1, 1 if version_index == 1 else 0, version_index,
                             len(version_names), gnu_hash(version_names[0]), 20,
                             0 if version_index == len(VERSIONS) else entry_size)
            aux = position + 20
            for name_index, name in enumerate(version_names):
                struct.pack_into("<II", image, aux, offsets[name], 0 if name_index + 1 == len(version_names) else 8)
                aux += 8
            position += entry_size
        verdef_end = position

    sections = [
        (0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0),
        (0, SHT_DYNSYM, 2, 0, DYNSYM_OFFSET, len(entries) * 24, 2, 1, 8, 24),
        (0, SHT_STRTAB, 2, 0, DYNSTR_OFFSET, len(strings), 0, 0, 1, 0),
        (0, SHT_GNU_HASH, 2, 0, HASH_OFFSET, chain_offset - HASH_OFFSET + chain_count * 4, 1, 0, 8, 0),
    ]
    if versioned:
        sections.append((0, SHT_GNU_VERSYM, 2, 0, VERSYM_OFFSET, len(entries) * 2, 1, 0, 2, 2))
        sections.append((0, SHT_GNU_VERDEF, 2, 0, VERDEF_OFFSET, verdef_end - VERDEF_OFFSET, 2, len(VERSIONS), 4, 0))
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0, 0, SHOFF, 0, 64, 0, 0, 64, len(sections), 0)
    for index, section in enumerate(sections):
        struct.pack_into("<IIQQQQIIQQ", image, SHOFF + index * 64, *section)
    return image


def sections(image):
    shoff, = struct.unpack_from("<Q", image, 0x28)
    shentsize, shnum = struct.unpack_from("<HH", image, 0x3A)
    return [struct.unpack_from("<IIQQQQIIQQ", image, shoff + index * shentsize) for index in range(shnum)]


def read_dynsym(image):
    headers = sections(image)
    dynsym = next(section for section in headers if section[1] == SHT_DYNSYM)
    strings = headers[dynsym[6]]
    entries = []
    for index in range(dynsym[5] // 24):
        name_offset, _, _, shndx, value, _ = struct.unpack_from("<IBBHQQ", image, dynsym[4] + index * 24)
        name = None
        if name_offset:
            end = image.index(0, strings[4] + name_offset)
            name = image[strings[4] + name_offset:end].decode("utf-8")
        entries.append((name, shndx, value))
    return entries


def patch(patcher, image, filename):
    with tempfile.TemporaryDirectory(prefix="anyps5-symbol-versions-") as directory:
        path = Path(directory) / filename
        path.write_bytes(image)
        result = subprocess.run([patcher, LIBRARY_NAME, str(path)], capture_output=True)
        return result, path.read_bytes()


def test_versioned_library_is_rejected(patcher):
    image = bytes(build_image(symbols(True), True))
    assert any(section[1] == SHT_GNU_VERDEF for section in sections(image))

    result, on_disk = patch(patcher, image, SONAME)
    assert result.returncode == 2, (result.returncode, result.stderr)
    message = result.stderr.decode("utf-8", errors="replace")
    assert message.startswith("FAIL: "), message
    assert ".gnu.version_d" in message, message
    assert result.stdout == b"", result.stdout
    assert on_disk == image, "the rejected library was written back modified"


def test_unversioned_library_is_still_patched(patcher):
    image = bytes(build_image(symbols(False), False))
    assert not any(section[1] == SHT_GNU_VERDEF for section in sections(image))
    before = read_dynsym(image)

    result, patched = patch(patcher, image, SONAME)
    assert result.returncode == 0, result.stderr.decode("utf-8", errors="replace")
    assert result.stdout.decode("utf-8", errors="replace").startswith("OK: "), result.stdout

    after = read_dynsym(patched)
    assert len(after) == len(before), (before, after)
    assert [entry[0] for entry in after] != [entry[0] for entry in before], "no export was renamed"

    for name, _, value in DEFINITIONS:
        found = [entry for entry in after if entry[2] == value]
        assert len(found) == 1, (name, found)
        renamed, shndx, _ = found[0]
        assert renamed != name, (name, found)
        assert NID_NAME.match(renamed), (name, found)
        assert shndx == 9, (name, found)

    for name in IMPORTS:
        found = [entry for entry in after if entry[0] == name]
        assert found == [(name, SHN_UNDEF, 0)], (name, found)

    assert len({entry[0] for entry in after if entry[0]}) == len([entry for entry in after if entry[0]])


def main():
    patcher = Path(sys.argv[1]).resolve()
    test_versioned_library_is_rejected(patcher)
    test_unversioned_library_is_still_patched(patcher)
    print("NID patcher symbol version tests passed")


if __name__ == "__main__":
    main()
