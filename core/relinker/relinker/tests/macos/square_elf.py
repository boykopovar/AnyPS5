"""Link square.c into a PS5-style guest ELF.

The image is produced on a Linux ELF toolchain and stored below so an Apple
clang, which emits Mach-O, can still write the same guest. Rebuild with
`python3 square_elf.py --rebuild` after editing square.c.
"""
import base64
import hashlib
import os
import struct
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nid import nid

SHF_ALLOC = 2
SHF_EXECINSTR = 4
SHT_NOBITS = 8
R_X86_64_PC32 = 2
R_X86_64_GOTPCREL = 9
R_X86_64_GOTPCRELX = 41
R_X86_64_REX_GOTPCRELX = 42
GOT_RELOCATIONS = {R_X86_64_GOTPCREL, R_X86_64_GOTPCRELX, R_X86_64_REX_GOTPCRELX}


def align(value, alignment):
    return (value + alignment - 1) & ~(alignment - 1)


def module_of(name):
    if name.startswith("sceVideoOut"):
        return "libSceVideoOut"
    if name.startswith("scePad"):
        return "libScePad"
    if name.startswith("sceAudioOut"):
        return "libSceAudioOut"
    return "libc"


def read_elf_object(blob):
    if blob[:4] != b"\x7fELF" or blob[4] != 2 or blob[5] != 1:
        raise SystemExit("square object is not ELF64")
    e_shoff, = struct.unpack_from("<Q", blob, 40)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 58)
    sections = []
    for index in range(e_shnum):
        offset = e_shoff + index * e_shentsize
        name, kind, flags, address, file_offset, size, link, info, alignment, entry = struct.unpack_from("<IIQQQQIIQQ", blob, offset)
        sections.append({
            "name": name, "kind": kind, "flags": flags, "offset": file_offset, "size": size,
            "link": link, "info": info, "align": alignment or 1, "entry": entry,
        })
    names = sections[e_shstrndx]
    def cstr(table, offset):
        end = blob.index(b"\0", table["offset"] + offset)
        return blob[table["offset"] + offset:end].decode()
    for section in sections:
        section["name"] = cstr(names, section["name"])
    return blob, sections


def section_bytes(blob, section):
    return blob[section["offset"]:section["offset"] + section["size"]]


def link_object(blob):
    blob, sections = read_elf_object(blob)
    by_index = {index: section for index, section in enumerate(sections)}
    allocated = [(index, section) for index, section in enumerate(sections) if section["flags"] & SHF_ALLOC and section["kind"] == 1 and section["size"]]
    executable = [(index, section) for index, section in allocated if section["flags"] & SHF_EXECINSTR]
    readonly = [(index, section) for index, section in allocated if section["flags"] & SHF_EXECINSTR == 0]
    cursor = 0x10000
    placed = {}
    pieces = []

    def place_sections(sections_to_place):
        nonlocal cursor
        for index, section in sections_to_place:
            cursor = align(cursor, section["align"])
            placed[index] = cursor
            pieces.append((cursor, section_bytes(blob, section)))
            cursor += section["size"]

    place_sections(executable)
    code_end = cursor
    cursor = align(cursor, 0x1000)
    ro_start = cursor
    place_sections(readonly)
    symtab = next(section for section in sections if section["kind"] == 2)
    strtab = by_index[symtab["link"]]
    symbols = []
    for offset in range(0, symtab["size"], 24):
        name, info, other, shndx, value, size = struct.unpack_from("<IBBHQQ", blob, symtab["offset"] + offset)
        end = blob.index(b"\0", strtab["offset"] + name)
        symbols.append({
            "name": blob[strtab["offset"] + name:end].decode(),
            "info": info, "shndx": shndx, "value": value,
        })
    guest_start = next(symbol for symbol in symbols if symbol["name"] == "guest_start")
    entry = placed[guest_start["shndx"]] + guest_start["value"]
    imports = []
    got_of = {}
    for symbol in symbols:
        if symbol["shndx"] == 0 and symbol["name"] and symbol["name"] not in got_of:
            got_of[symbol["name"]] = len(imports)
            imports.append(symbol["name"])
    image_rx = bytearray(b"\0" * (cursor - 0x10000))
    for address, data in pieces:
        start = address - 0x10000
        image_rx[start:start + len(data)] = data
    relocations = []
    for section in sections:
        if section["kind"] != 4:
            continue
        target = by_index[section["info"]]
        if target["flags"] & SHF_ALLOC == 0:
            continue
        for offset in range(0, section["size"], 24):
            place, info, addend = struct.unpack_from("<QQq", blob, section["offset"] + offset)
            relocations.append((by_index[section["info"]], place, info & 0xffffffff, info >> 32, addend))
    modules = []
    for name in imports:
        module = module_of(name)
        if module not in modules:
            modules.append(module)
    letters = "BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"
    module_letter = {module: letters[index] for index, module in enumerate(modules)}
    strings = bytearray(b"\0")
    def add_string(text):
        offset = len(strings)
        strings.extend(text.encode() + b"\0")
        return offset
    qualified = []
    for name in imports:
        qualified.append(add_string(nid(name) + "#A#" + module_letter[module_of(name)]))
    needed = [(module + ".prx", add_string(module + ".prx")) for module in modules]
    declared = [(module, add_string(module)) for module in modules]
    sym_size = (len(imports) + 1) * 24
    rela_size = len(imports) * 24
    tags = [(1, offset) for _, offset in needed]
    tags += [(0x61000045, (index << 48) | offset) for index, (_, offset) in enumerate(declared, 1)]
    tags += [(5, 0), (10, len(strings)), (6, 0), (11, 24), (0x6100003f, sym_size), (7, 0), (8, rela_size), (9, 24), (0, 0)]
    dyn_size = len(tags) * 16
    meta = align(cursor, 8)
    str_va = meta
    sym_va = align(str_va + len(strings), 8)
    rela_va = align(sym_va + sym_size, 8)
    dyn_va = align(rela_va + rela_size, 8)
    ro_end = dyn_va + dyn_size
    got_va = align(ro_end, 0x1000)
    got_size = len(imports) * 8
    nobits = [(index, section) for index, section in enumerate(sections) if section["flags"] & SHF_ALLOC and section["flags"] & SHF_EXECINSTR == 0 and section["kind"] == SHT_NOBITS and section["size"]]
    bss_cursor = align(got_va + got_size, 0x10000)
    bss_start = bss_cursor
    for index, section in nobits:
        bss_cursor = align(bss_cursor, max(section["align"], 1))
        placed[index] = bss_cursor
        bss_cursor += section["size"]
    bss_size = bss_cursor - bss_start
    for index, (tag, value) in enumerate(tags):
        if tag == 5:
            tags[index] = (tag, str_va)
        elif tag == 6:
            tags[index] = (tag, sym_va)
        elif tag == 7:
            tags[index] = (tag, rela_va)
    def place_address(section, offset):
        return placed[next(index for index, item in enumerate(sections) if item is section)] + offset
    for section, place, kind, symbol_index, addend in relocations:
        address = place_address(section, place)
        file_at = address - 0x10000
        symbol = symbols[symbol_index]
        if kind == R_X86_64_PC32:
            if symbol["shndx"] == 0:
                raise SystemExit("PC32 relocation to " + symbol["name"])
            target = placed[symbol["shndx"]] + symbol["value"] + addend - address
            image_rx[file_at:file_at + 4] = struct.pack("<i", target)
        elif kind in GOT_RELOCATIONS:
            slot = got_va + got_of[symbol["name"]] * 8
            target = slot + addend - address
            image_rx[file_at:file_at + 4] = struct.pack("<i", target)
        else:
            raise SystemExit(f"unsupported relocation {kind} for {symbol['name']}")
    ro_file_end = ro_end - 0x10000
    if len(image_rx) < ro_file_end:
        image_rx.extend(b"\0" * (ro_file_end - len(image_rx)))
    image_rx[str_va - 0x10000:str_va - 0x10000 + len(strings)] = strings
    for index, name_offset in enumerate(qualified, 1):
        struct.pack_into("<IBBHQQ", image_rx, sym_va - 0x10000 + index * 24, name_offset, 0x12, 0, 0, 0, 0)
    for index, name in enumerate(imports, 1):
        struct.pack_into("<QQq", image_rx, rela_va - 0x10000 + (index - 1) * 24, got_va + (index - 1) * 8, (index << 32) | 6, 0)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image_rx, dyn_va - 0x10000 + index * 16, *tag)
    image = bytearray(got_va + got_size)
    image[0x10000:0x10000 + len(image_rx)] = image_rx
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    code_size = code_end - 0x10000
    ro_size = ro_end - ro_start
    phnum = 5 if bss_size else 4
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, entry, 64, 0, 0, 64, 56, phnum, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x10000, 0x10000, 0x10000, code_size, code_size, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 4, ro_start, ro_start, ro_start, ro_size, ro_size, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 176, 1, 6, got_va, got_va, got_va, got_size, got_size, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 232, 2, 4, dyn_va, dyn_va, dyn_va, dyn_size, dyn_size, 8)
    if bss_size:
        struct.pack_into("<IIQQQQQQ", image, 288, 1, 6, 0, bss_start, bss_start, 0, bss_size, 0x10000)
    return bytes(image), entry, imports


PAYLOAD = """
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
"""


def payload_bytes():
    if not PAYLOAD.strip():
        raise SystemExit("square guest image is empty; run square_elf.py --rebuild")
    return zlib.decompress(base64.b64decode(PAYLOAD))


def rebuild(path):
    source = os.path.join(os.path.dirname(os.path.abspath(__file__)), "square.c")
    object_path = path + ".o"
    subprocess.check_call([
        "clang", "-c", "-O2", "-fPIC", "-fno-plt", "-ffreestanding", "-fno-builtin",
        "-fno-stack-protector", "-fno-asynchronous-unwind-tables", "-fcf-protection=none",
        "-mno-red-zone", "-o", object_path, source,
    ])
    image, entry, imports = link_object(open(object_path, "rb").read())
    encoded = base64.b64encode(zlib.compress(image, 9)).decode()
    script = open(__file__, "r", encoding="utf-8").read()
    begin_mark = "PAYLOAD = \"\"\"\n"
    end_mark = "\"\"\"\n"
    begin = script.index(begin_mark)
    end = script.index(end_mark, begin + len(begin_mark)) + len(end_mark)
    wrapped = "PAYLOAD = \"\"\"\n" + encoded + "\n\"\"\"\n"
    open(__file__, "w", encoding="utf-8").write(script[:begin] + wrapped + script[end:])
    print(f"entry 0x{entry:x} imports {len(imports)} bytes {len(image)} sha256 {hashlib.sha256(image).hexdigest()}")
    return image


def main(argv):
    if len(argv) == 2 and argv[1] == "--rebuild":
        rebuild(os.path.join(os.path.dirname(os.path.abspath(__file__)), "square.elf"))
        return
    if len(argv) != 2:
        raise SystemExit("usage: square_elf.py OUTPUT | square_elf.py --rebuild")
    open(argv[1], "wb").write(payload_bytes() if argv[1] != "-" else payload_bytes())


if __name__ == "__main__":
    main(sys.argv)
