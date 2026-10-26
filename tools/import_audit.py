import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

import nid_names
import progress

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
MODULE_SUFFIX = "#"
CLASSES = ("implemented", "partial", "stub", "absent", "module")
CONDITIONAL = re.compile(r"(?:if|else|for|while|do|switch|case|default|try|catch|co_await|co_yield)\b")
BRANCHING = re.compile(r"\?|&&|\|\||\b(?:and|or)\b")
EXIT = re.compile(r"\b(?:return|co_return|goto)\b")
PREPROCESSOR_LINE = re.compile(r"^[ \t]*#[ \t]*(ifdef|ifndef|if|elifdef|elifndef|elif|else|endif)\b[^\n]*$", re.M)
BRANCH_VARIANT_LIMIT = 256
PE_DIRECTORY_OFFSET = {0x20b: 112, 0x10b: 96}


class AuditError(Exception):
    pass


def take(data, offset, size, path, what):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise AuditError(f"{path}: {what} at 0x{offset:x} with size {size} lies outside the file ({len(data)} bytes)")
    return data[offset:offset + size]


def unpack(layout, data, offset, path, what):
    return struct.unpack(layout, take(data, offset, struct.calcsize(layout), path, what))


def cstring(data, offset, limit, path, what, encoding="utf-8", errors="surrogateescape"):
    end = data.find(b"\0", offset, limit)
    if offset < 0 or offset >= limit or end < 0:
        raise AuditError(f"{path}: {what} at 0x{offset:x} is not a NUL-terminated string inside 0x{limit:x} bytes")
    try:
        return data[offset:end].decode(encoding, errors=errors)
    except UnicodeDecodeError as error:
        raise AuditError(f"{path}: {what} at 0x{offset:x} is not {encoding}: {error}") from error


def elf_sections(path, data):
    header = unpack("<16sHHIQQQIHHHHHH", data, 0, path, "ELF header")
    ident, section_offset, section_size, section_count = header[0], header[6], header[11], header[12]
    if ident[4] != 2 or ident[5] != 1 or section_size != 64:
        raise AuditError(f"{path}: only little-endian ELF64 with 64-byte section headers is supported (class {ident[4]}, data {ident[5]}, shentsize {section_size})")
    if ident[6] != 1 or header[3] != 1:
        raise AuditError(f"{path}: unsupported ELF version (identification {ident[6]}, object {header[3]})")
    if not section_offset:
        raise AuditError(f"{path}: ELF file without a section header table is not supported")
    if not section_count:
        section_count = unpack("<IIQQQQIIQQ", data, section_offset, path, "extended section count")[5]
    take(data, section_offset, section_count * 64, path, "section header table")
    return [unpack("<IIQQQQIIQQ", data, section_offset + index * 64, path, f"section header {index}") for index in range(section_count)]


def elf_program_dynamic_symbols(path, data):
    header = unpack("<16sHHIQQQIHHHHHH", data, 0, path, "ELF header")
    ident, program_offset, program_size, program_count = header[0], header[5], header[9], header[10]
    if ident[6] != 1 or header[3] != 1:
        raise AuditError(f"{path}: unsupported ELF version (identification {ident[6]}, object {header[3]})")
    if ident[4] != 2 or ident[5] != 1 or program_size != 56 or not program_offset or not program_count:
        raise AuditError(f"{path}: sectionless ELF needs little-endian ELF64 program headers")
    take(data, program_offset, program_count * program_size, path, "program header table")
    segments = [unpack("<IIQQQQQQ", data, program_offset + index * program_size, path, f"program header {index}") for index in range(program_count)]
    loads = [segment for segment in segments if segment[0] == 1]

    def file_offset(address, size, what):
        for segment in loads:
            _, _, offset, virtual, _, file_size, _, _ = segment
            if address >= virtual and address - virtual <= file_size and size <= file_size - (address - virtual):
                result = offset + address - virtual
                take(data, result, size, path, what)
                return result
        raise AuditError(f"{path}: {what} address 0x{address:x} is outside file-backed load segments")

    dynamic_segments = [segment for segment in segments if segment[0] == 2]
    if len(dynamic_segments) != 1:
        raise AuditError(f"{path}: expected one dynamic segment, found {len(dynamic_segments)}")
    _, _, dynamic_offset, _, _, dynamic_size, _, _ = dynamic_segments[0]
    if dynamic_size % 16:
        raise AuditError(f"{path}: dynamic segment size {dynamic_size} is not a multiple of 16")
    take(data, dynamic_offset, dynamic_size, path, "dynamic segment")
    dynamic = {}
    needed_offsets = []
    terminated = False
    for position in range(0, dynamic_size, 16):
        tag, value = unpack("<qQ", data, dynamic_offset + position, path, "dynamic entry")
        if tag == 0:
            terminated = True
            break
        if tag == 1:
            needed_offsets.append(value)
        else:
            dynamic[tag] = value
    if not terminated:
        raise AuditError(f"{path}: dynamic segment is not terminated by DT_NULL")
    string_address = dynamic.get(5)
    string_size = dynamic.get(10)
    symbol_address = dynamic.get(6)
    symbol_size = dynamic.get(11)
    if None in (string_address, string_size, symbol_address, symbol_size) or symbol_size != 24 or string_size == 0:
        raise AuditError(f"{path}: incomplete dynamic string or symbol table")
    string_offset = file_offset(string_address, string_size, "dynamic string table")

    symbol_count = None
    if 4 in dynamic:
        hash_offset = file_offset(dynamic[4], 8, "SysV hash table")
        buckets, chains = unpack("<II", data, hash_offset, path, "SysV hash header")
        file_offset(dynamic[4], 8 + 4 * (buckets + chains), "SysV hash table")
        symbol_count = chains
    elif 0x6ffffef5 in dynamic:
        hash_offset = file_offset(dynamic[0x6ffffef5], 16, "GNU hash header")
        buckets, first_symbol, bloom_count, _ = unpack("<IIII", data, hash_offset, path, "GNU hash header")
        file_offset(dynamic[0x6ffffef5], 16 + bloom_count * 8 + buckets * 4, "GNU hash prefix")
        bucket_offset = hash_offset + 16 + bloom_count * 8
        bucket_data = take(data, bucket_offset, buckets * 4, path, "GNU hash buckets")
        chain_offset = bucket_offset + buckets * 4
        maximum = first_symbol - 1
        for index in struct.unpack(f"<{buckets}I", bucket_data) if buckets else ():
            if index == 0:
                continue
            if index < first_symbol:
                raise AuditError(f"{path}: GNU hash bucket index {index} precedes symbol offset {first_symbol}")
            symbol_index = index
            while True:
                chain_address = dynamic[0x6ffffef5] + (chain_offset - hash_offset) + (symbol_index - first_symbol) * 4
                chain_file_offset = file_offset(chain_address, 4, "GNU hash chain")
                chain_value = unpack("<I", data, chain_file_offset, path, "GNU hash chain entry")[0]
                maximum = max(maximum, symbol_index)
                if chain_value & 1:
                    break
                symbol_index += 1
        symbol_count = maximum + 1
    if symbol_count is None:
        raise AuditError(f"{path}: dynamic symbol table has no supported hash table")
    symbol_offset = file_offset(symbol_address, symbol_count * symbol_size, "dynamic symbol table")
    imports = {}
    exports = set()
    for index in range(symbol_count):
        name, info, other, section_index, _, _ = unpack("<IBBHQQ", data, symbol_offset + index * symbol_size, path, "dynamic symbol")
        if name == 0:
            continue
        symbol = cstring(data, string_offset + name, string_offset + string_size, path, "dynamic symbol name").split(MODULE_SUFFIX, 1)[0]
        if section_index == 0:
            imports[symbol] = imports.get(symbol, True) and info >> 4 == 2
        elif info >> 4 in (1, 2, 10) and other & 3 in (0, 3):
            exports.add(symbol)
    needed = {cstring(data, string_offset + value, string_offset + string_size, path, "needed library name") for value in needed_offsets}
    return imports, exports, needed


def elf_exports(path, data):
    header = unpack("<16sHHIQQQIHHHHHH", data, 0, path, "ELF header")
    if not header[6] or header[6] >= len(data):
        return elf_program_dynamic_symbols(path, data)[1]
    sections = elf_sections(path, data)
    names = set()
    for section_index, section in enumerate(sections):
        kind, offset, size, link, entry = section[1], section[4], section[5], section[6], section[9]
        if kind != 11:
            continue
        if entry != 24 or size % 24 or link >= len(sections) or sections[link][1] != 3:
            raise AuditError(f"{path}: invalid dynamic symbol table (entry size {entry}, size {size}, string table section {link})")
        take(data, offset, size, path, "dynamic symbol table")
        strings_offset, strings_size = sections[link][4], sections[link][5]
        take(data, strings_offset, strings_size, path, "dynamic string table")
        indexes = [candidate for candidate in sections if candidate[1] == 18 and candidate[6] == section_index]
        if len(indexes) > 1:
            raise AuditError(f"{path}: multiple extended section index tables for section {section_index}")
        for position in range(0, size, 24):
            name, info, other, index, _, _ = unpack("<IBBHQQ", data, offset + position, path, "dynamic symbol")
            if index == 0xffff:
                if not indexes or indexes[0][9] != 4 or indexes[0][5] != size // 24 * 4:
                    raise AuditError(f"{path}: invalid or missing extended section index table for section {section_index}")
                index = unpack("<I", data, indexes[0][4] + position // 24 * 4, path, "extended symbol section index")[0]
                if index >= len(sections):
                    raise AuditError(f"{path}: extended symbol section index {index} lies outside the section table")
            elif 0 < index < 0xff00 and index >= len(sections):
                raise AuditError(f"{path}: symbol section index {index} lies outside the section table")
            binding, visibility = info >> 4, other & 3
            if name == 0 or index == 0 or binding not in (1, 2, 10) or visibility not in (0, 3):
                continue
            names.add(cstring(data, strings_offset + name, strings_offset + strings_size, path, "export name").split(MODULE_SUFFIX, 1)[0])
    return names


def elf_imports(path, data):
    header = unpack("<16sHHIQQQIHHHHHH", data, 0, path, "ELF header")
    if not header[6] or header[6] >= len(data):
        imports, _, needed = elf_program_dynamic_symbols(path, data)
        return imports, needed
    sections = elf_sections(path, data)
    symbols = {}
    needed = set()
    for section in sections:
        kind, offset, size, link, entry = section[1], section[4], section[5], section[6], section[9]
        if kind == 11:
            if entry != 24 or size % 24 or link >= len(sections) or sections[link][1] != 3:
                raise AuditError(f"{path}: invalid dynamic symbol table (entry size {entry}, size {size}, string table section {link})")
            take(data, offset, size, path, "dynamic symbol table")
            strings_offset, strings_size = sections[link][4], sections[link][5]
            take(data, strings_offset, strings_size, path, "dynamic string table")
            for position in range(0, size, 24):
                name, info, _, index, _, _ = unpack("<IBBHQQ", data, offset + position, path, "dynamic symbol")
                if name and index == 0:
                    symbol = cstring(data, strings_offset + name, strings_offset + strings_size, path, "import name")
                    symbol = symbol.split(MODULE_SUFFIX, 1)[0]
                    symbols[symbol] = symbols.get(symbol, True) and info >> 4 == 2
        elif kind == 6:
            if entry != 16 or size % 16 or link >= len(sections) or sections[link][1] != 3:
                raise AuditError(f"{path}: invalid dynamic section (entry size {entry}, size {size}, string table section {link})")
            take(data, offset, size, path, "dynamic section")
            strings_offset, strings_size = sections[link][4], sections[link][5]
            take(data, strings_offset, strings_size, path, "dynamic string table")
            terminated = False
            for position in range(0, size, 16):
                tag, value = unpack("<qQ", data, offset + position, path, "dynamic entry")
                if tag == 0:
                    terminated = True
                    break
                if tag == 1:
                    needed.add(cstring(data, strings_offset + value, strings_offset + strings_size, path, "needed library name"))
            if not terminated:
                raise AuditError(f"{path}: dynamic section is not terminated by DT_NULL")
    return symbols, needed


def pe_headers(path, data):
    header = unpack("<I", data, 0x3c, path, "PE header offset")[0]
    if unpack("<I", data, header, path, "PE signature")[0] != 0x4550:
        raise AuditError(f"{path}: invalid PE signature at 0x{header:x}")
    section_count, optional_size = unpack("<H", data, header + 6, path, "section count")[0], unpack("<H", data, header + 20, path, "optional header size")[0]
    optional = header + 24
    magic = unpack("<H", data, optional, path, "optional header magic")[0]
    if magic not in PE_DIRECTORY_OFFSET:
        raise AuditError(f"{path}: unsupported PE optional header magic 0x{magic:x}")
    directory = PE_DIRECTORY_OFFSET[magic]
    if optional_size < directory + 8:
        raise AuditError(f"{path}: PE optional header of {optional_size} bytes has no export directory")
    take(data, optional, optional_size, path, "PE optional header")
    count = unpack("<I", data, optional + directory - 4, path, "data directory count")[0]
    if count == 0:
        raise AuditError(f"{path}: PE file declares no data directories")
    headers_size = unpack("<I", data, optional + 60, path, "header size")[0]
    sections = [unpack("<8sIIIIIIHHI", data, optional + optional_size + index * 40, path, f"PE section {index}") for index in range(section_count)]

    def to_offset(address, length):
        if address < headers_size and length <= headers_size - address:
            take(data, address, length, path, f"header data at RVA 0x{address:x}")
            return address, min(headers_size, len(data))
        for section in sections:
            virtual, raw_size, raw_offset = section[2], section[3], section[4]
            if address < virtual:
                continue
            relative = address - virtual
            if relative >= raw_size or length > raw_size - relative:
                continue
            take(data, raw_offset + relative, length, path, f"data at RVA 0x{address:x}")
            return raw_offset + relative, min(raw_offset + raw_size, len(data))
        raise AuditError(f"{path}: RVA 0x{address:x} is not inside any section")

    return to_offset, optional + directory, count


def pe_exports(path, data):
    to_offset, directory, _ = pe_headers(path, data)
    rva, size = unpack("<II", data, directory, path, "export data directory")
    if not rva and not size:
        return set()
    if not rva or size < 40:
        raise AuditError(f"{path}: PE file has no export table")

    table = unpack("<IIHHIIIIIII", data, to_offset(rva, 40)[0], path, "export directory")
    function_count, name_count, functions_rva, names_rva, ordinals_rva = table[6:11]
    names = set()
    if not name_count:
        return names
    if not functions_rva or not names_rva or not ordinals_rva:
        raise AuditError(f"{path}: named PE exports require address, name and ordinal tables")
    functions_offset = to_offset(functions_rva, function_count * 4)[0]
    names_offset = to_offset(names_rva, name_count * 4)[0]
    ordinals_offset = to_offset(ordinals_rva, name_count * 2)[0]
    previous_name = None
    for index in range(name_count):
        name_rva = unpack("<I", data, names_offset + index * 4, path, "export name RVA")[0]
        offset, limit = to_offset(name_rva, 1)
        name = cstring(data, offset, limit, path, "export name", "ASCII", "strict")
        if previous_name is not None and name < previous_name:
            raise AuditError(f"{path}: PE export name pointer table is not sorted")
        previous_name = name
        ordinal = unpack("<H", data, ordinals_offset + index * 2, path, "export ordinal")[0]
        if ordinal >= function_count:
            raise AuditError(f"{path}: export ordinal {ordinal} lies outside the address table ({function_count} entries)")
        function_rva = unpack("<I", data, functions_offset + ordinal * 4, path, "export address")[0]
        if not function_rva:
            continue
        if rva <= function_rva < rva + size:
            offset, limit = to_offset(function_rva, 1)
            cstring(data, offset, min(limit, offset + rva + size - function_rva), path, "export forwarder", "ASCII", "strict")
        names.add(name)
    return names


def pe_imports(path, data):
    to_offset, directory, count = pe_headers(path, data)
    if count < 2:
        return set()
    rva, size = unpack("<II", data, directory + 8, path, "import data directory")
    if not rva:
        return set()
    if size < 20:
        raise AuditError(f"{path}: PE import table of {size} bytes holds no import entry")
    names = set()
    for index in range(size // 20):
        entry = unpack("<IIIII", data, to_offset(rva + index * 20, 20)[0], path, "import directory entry")
        if entry == (0, 0, 0, 0, 0):
            return names
        if not entry[3]:
            raise AuditError(f"{path}: PE import entry {index} names no library")
        offset, limit = to_offset(entry[3], 1)
        names.add(cstring(data, offset, limit, path, "import library name", "ASCII", "strict"))
    raise AuditError(f"{path}: PE import table is not NUL-terminated")


def read_host_imports(path, data=None):
    if data is None:
        data = path.read_bytes()
    if data[:2] == b"MZ":
        return pe_imports(path, data)
    if data[:4] == b"\x7fELF":
        return elf_imports(path, data)[1]
    raise AuditError(f"{path}: neither ELF nor PE (first bytes {data[:4]!r})")


def read_exports(path, data=None):
    if data is None:
        data = path.read_bytes()
    if data[:4] == b"\x7fELF":
        return elf_exports(path, data)
    if data[:2] == b"MZ":
        return pe_exports(path, data)
    raise AuditError(f"{path}: neither ELF nor PE (first bytes {data[:4]!r})")


def directory_files(directory, pattern):
    path = Path(directory)
    if not path.is_dir():
        raise AuditError(f"{directory}: not a directory")
    return sorted(item for item in path.glob(pattern) if item.is_file())


def built_libraries(directories):
    exports = defaultdict(set)
    libraries = set()
    images = {}
    for directory in directories:
        files = directory_files(directory, "*.prx")
        if not files:
            raise AuditError(f"{directory}: no .prx files; pass the directory the libs target builds (build/core/libs/libs)")
        for file in files:
            data = file.read_bytes()
            digest = hashlib.sha256(data).digest()
            if file.name in images:
                previous, fingerprint = images[file.name]
                if digest != fingerprint:
                    raise AuditError(f"ambiguous built library {file.name}: {previous} and {file} have different contents")
                continue
            images[file.name] = (file, digest)
            libraries.add(file.name)
            for name in read_exports(file, data):
                exports[name].add(file.name)
    return {name: sorted(files) for name, files in exports.items()}, libraries


def built_host_dependencies(directories):
    """Map each built .prx to its imported .prx names and identify PE importers.

    The loader resolves these host-to-host dependencies with LoadLibrary, so a
    name with no file in --libs or --modules fails even when every game import
    is implemented. System DLLs resolve through the OS and are not reported.
    """
    direct = {}
    windows = set()
    images = {}
    for directory in directories:
        for file in directory_files(directory, "*.prx"):
            data = file.read_bytes()
            digest = hashlib.sha256(data).digest()
            if file.name in images:
                previous, fingerprint = images[file.name]
                if digest != fingerprint:
                    raise AuditError(f"ambiguous built library {file.name}: {previous} and {file} have different contents")
                continue
            images[file.name] = (file, digest)
            direct[file.name] = sorted({name for name in read_host_imports(file, data) if name.lower().endswith(".prx")})
            if data[:2] == b"MZ":
                windows.add(file.name)
    return direct, windows


def audit_host_dependencies(direct, libraries, modules, windows=()):
    """Walk host imports transitively; list each missing library with its chain."""
    available = set(libraries) | set(modules)
    folded = defaultdict(set)
    for name in available:
        folded[name.lower()].add(name)
    missing = []
    for root in sorted(direct):
        expanded = set()
        stack = [(name, [root, name]) for name in reversed(direct[root])]
        while stack:
            name, chain = stack.pop()
            if chain[-2] in windows:
                matches = folded.get(name.lower(), ())
                if len(matches) > 1:
                    raise AuditError(f"ambiguous Windows host library {name}: {', '.join(sorted(matches))}")
                name = next(iter(matches), name)
                chain[-1] = name
            if name not in available:
                missing.append({"importer": root, "chain": chain})
            elif name in direct and name not in expanded and name not in chain[1:-1]:
                expanded.add(name)
                for dependency in reversed(direct[name]):
                    if dependency in chain:
                        continue
                    stack.append((dependency, chain + [dependency]))
    return sorted(missing, key=lambda entry: (entry["importer"], entry["chain"]))


def module_files(directory):
    for file in directory_files(directory, "*"):
        with file.open("rb") as handle:
            magic = handle.read(4)
        if magic == b"\x7fELF" or (magic.startswith(b"MZ") and file.name.endswith(".guest.prx")):
            yield file
        elif (file.suffix.lower() in (".prx", ".sprx", ".suprx", ".debug_prx", ".elf")
              or magic.startswith(b"MZ")
              or magic in (b"\x4f\x15\x3d\x1d", b"\x54\x14\xf5\xee")):
            raise AuditError(f"{file}: expected ELF module (first bytes {magic!r})")


def read_modules(directories):
    imports = {}
    exports = defaultdict(set)
    images = {}
    for directory in directories:
        for file in module_files(directory):
            data = file.read_bytes()
            digest = hashlib.sha256(data).digest()
            if file.name in images:
                previous, fingerprint = images[file.name]
                if digest != fingerprint:
                    raise AuditError(f"ambiguous guest module {file.name}: {previous} and {file} have different contents")
                continue
            images[file.name] = (file, digest)
            if data.startswith(b"MZ"):
                imports[file.name] = ([], [])
                names = pe_exports(file, data)
            else:
                symbols, needed = elf_imports(file, data)
                imports[file.name] = (sorted(symbols.items()), sorted(needed))
                names = elf_exports(file, data)
            for name in names:
                exports[name.split(MODULE_SUFFIX, 1)[0]].add(file.name)
    return imports, {name: sorted(files) for name, files in exports.items()}


def nids_of(name, aliases):
    if name in aliases:
        return aliases[name]
    return {nid_names.compute_nid(nid_names.strip_nid_postfix(name))}


def top_level_statements(body):
    braces, parens, start, parts = 0, 0, 1, []
    for i, character in enumerate(body[1:-1], 1):
        if character in "([":
            parens += 1
        elif character in ")]":
            parens -= 1
        elif character == "{":
            braces += 1
        elif character == "}":
            braces -= 1
            if not braces and not parens:
                parts.append((start, body[start:i + 1]))
                start = i + 1
        elif character == ";" and not braces and not parens:
            parts.append((start, body[start:i + 1]))
            start = i + 1
    parts.append((start, body[start:-1]))
    return parts


def branch_variants(body):
    root = []
    lists, groups = [root], []
    position = 0
    for directive in PREPROCESSOR_LINE.finditer(body):
        lists[-1].append(body[position:directive.start()])
        position = directive.end()
        kind = directive.group(1)
        if kind in ("if", "ifdef", "ifndef"):
            group = {"alternatives": [[]], "default": False}
            lists[-1].append(group)
            groups.append(group)
            lists.append(group["alternatives"][-1])
            continue
        if not groups:
            raise AuditError(f"#{kind} without a matching #if")
        group = groups[-1]
        lists.pop()
        if kind == "endif":
            if not group["default"]:
                group["alternatives"].append([])
            groups.pop()
            continue
        group["default"] = group["default"] or kind == "else"
        group["alternatives"].append([])
        lists.append(group["alternatives"][-1])
    if groups:
        raise AuditError("#if without a matching #endif")
    root.append(body[position:])

    def expand(items):
        variants = [""]
        for item in items:
            if isinstance(item, str):
                variants = [variant + item for variant in variants]
                continue
            choices = [choice for alternative in item["alternatives"] for choice in expand(alternative)]
            variants = [variant + choice for variant in variants for choice in choices]
            if len(variants) > BRANCH_VARIANT_LIMIT:
                raise AuditError(f"more than {BRANCH_VARIANT_LIMIT} preprocessor branch combinations in one function")
        return variants

    return expand(root)


def variant_calls(body):
    return [unconditional_calls(variant) for variant in branch_variants(body)]


def unconditional_calls(body):
    names = set()
    for offset, statement in top_level_statements(body):
        block = statement.strip()
        if block.startswith("{") and block.endswith("}"):
            names |= unconditional_calls(block)
        elif not (CONDITIONAL.match(block) or BRANCHING.search(statement)):
            for call in progress.CALL.finditer(statement):
                before = statement[:call.start()]
                if before.count("{") == before.count("}") and not progress.QUALIFIED.search(body, 0, offset + call.start()):
                    names.add(call.group(1))
        if EXIT.search(statement):
            break
    return names


def always_stub_calls(text):
    helpers = {}
    for match in progress.STUB_WRAPPER.finditer(text):
        body = text[match.end() - 1:progress.body_end(text, match.end() - 1) + 1]
        helpers.setdefault(match.group(1), []).extend(variant_calls(body))
    calls = {progress.STUB}
    while True:
        found = {name for name, bodies in helpers.items() if name not in calls and all(targets & calls for targets in bodies)}
        if not found:
            return calls
        calls |= found


def conditional_stubs(library):
    conditional = set()
    for source in library.rglob("*.cpp"):
        if "tests" in source.relative_to(library).parts:
            continue
        text = progress.code(source.read_text(errors="ignore"))
        calls = always_stub_calls(text)
        for match in progress.DEFINITION.finditer(text):
            body = text[match.end() - 1:progress.body_end(text, match.end() - 1) + 1]
            if not all(targets & calls for targets in variant_calls(body)):
                conditional.add(match.group(1))
    return conditional


def source_stubs(source):
    return source_stub_classes(source)[0]


def source_stub_classes(source):
    if not Path(source).is_dir():
        raise AuditError(f"{source}: not a directory")
    stubs = {}
    partial = set()
    shared = {}
    for library in sorted(item for item in Path(source).iterdir() if item.is_dir()):
        aliases = {}
        for file in nid_names.source_files(library):
            for nid, name in nid_names.source_aliases(file.read_text(encoding="utf-8-sig", errors="ignore")):
                aliases.setdefault(name, set()).add(nid)
        group = progress.scan_library(library)
        if "shared_sources" in group:
            shared[library.name] = group["shared_sources"]
        done = {nid for name in group["done_names"] for nid in nids_of(name, aliases)}
        conditional = conditional_stubs(library) if group["todo_names"] else set()
        for name in group["todo_names"]:
            for nid in nids_of(name, aliases) - done:
                stubs.setdefault((library.name + ".prx", nid), name)
                if name in conditional:
                    partial.add((library.name + ".prx", nid))
    for library, owner in shared.items():
        chain = [library]
        while owner in shared:
            if owner in chain:
                raise AuditError(f"{source}: shared source cycle: {' -> '.join(chain + [owner])}")
            chain.append(owner)
            owner = shared[owner]
        for (provider, nid), name in list(stubs.items()):
            if provider == owner + ".prx":
                stubs[(library + ".prx", nid)] = name
                if (provider, nid) in partial:
                    partial.add((library + ".prx", nid))
    return stubs, partial


def read_registry(path):
    try:
        entries = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AuditError(f"{path}: cannot read registry: {error}") from error
    if not isinstance(entries, list):
        raise AuditError(f"{path}: a registry is a JSON list, got {type(entries).__name__}")
    imports = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict) or not isinstance(entry.get("nid"), str) or not isinstance(entry.get("library"), str):
            raise AuditError(f"{path}: entry {index} needs string fields 'nid' and 'library': {entry!r}")
        nid = entry["nid"].split("#", 1)[0]
        if not nid or any(character.isspace() or ord(character) < 32 or ord(character) == 127 for character in nid):
            raise AuditError(f"{path}: entry {index} has an invalid import symbol {entry['nid']!r}")
        weak = entry.get("weak", False)
        if not isinstance(weak, bool):
            raise AuditError(f"{path}: entry {index} needs a boolean 'weak' field")
        imports.append((nid, entry["library"], weak))
    return imports


def needed_registry_path(path):
    return path.with_name(path.name.removesuffix(".registry.json") + ".needed.json")


def read_needed_registry(path):
    try:
        libraries = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AuditError(f"{path}: cannot read needed libraries: {error}") from error
    if not isinstance(libraries, list) or any(not isinstance(library, str) or not library for library in libraries):
        raise AuditError(f"{path}: needed libraries must be a JSON list of nonempty strings")
    return libraries


def check_conservation(references, records):
    classified = sum(record["references"] for record in records)
    unknown = sorted({record["class"] for record in records} - set(CLASSES))
    if classified != references or unknown:
        raise AuditError(f"conservation failed: {references} references in, {classified} classified, unknown classes {unknown}")


MODULE_FILE_SUFFIXES = (".debug_prx", ".suprx", ".sprx", ".prx")


def module_stem(name, fold):
    if fold:
        name = name.lower()
    for suffix in MODULE_FILE_SUFFIXES:
        if len(name) > len(suffix) and name.endswith(suffix):
            name = name[:-len(suffix)]
            break
    return name


class ModuleNames(set):
    """Title module names, matched like the relinker: by the name without its .prx, .sprx, .suprx or .debug_prx suffix, and
    ignoring case for Windows output."""

    def __init__(self, names=(), fold=False):
        super().__init__(names)
        self.fold = fold
        self.stems = {module_stem(name, fold) for name in self}

    def __contains__(self, library):
        return isinstance(library, str) and module_stem(library, self.fold) in self.stems


def classify(nid, library, providers, stubs, modules, partial=frozenset()):
    if library and library in modules:
        return "module"
    if not providers:
        return "absent"
    if all((provider, nid) in stubs for provider in providers):
        return "partial" if any((provider, nid) in partial for provider in providers) else "stub"
    return "implemented"


def audit(imports, exports, stubs, libraries, modules, module_imports=None, module_exports=None, *, needed_libraries=(), windows_libraries=(), partial=frozenset()):
    windows_names = {}
    for library in windows_libraries:
        windows_names.setdefault(library.casefold(), set()).add(library)

    def system_library(name):
        if name:
            for suffix in (".sprx", ".suprx", ".debug_prx"):
                if name.endswith(suffix):
                    normalized = name.removesuffix(suffix) + ".prx"
                    candidates = windows_names.get(normalized.casefold(), ())
                    if normalized in candidates:
                        return normalized
                    if len(candidates) == 1:
                        return next(iter(candidates))
        return name

    def available(name):
        return name in modules or system_library(name) in libraries

    records = {}
    for nid, library, weak in imports:
        record = records.setdefault((nid, library), {"nid": nid, "library": library, "references": 0, "weak": weak})
        record["references"] += 1
        record["weak"] = record["weak"] and weak
    for module, (symbols, needed) in (module_imports or {}).items():
        for nid, weak in symbols:
            record = records.setdefault((nid, None, module), {"nid": nid, "library": None, "module": module, "references": 0, "weak": weak})
            record["references"] += 1
            record["weak"] = record["weak"] and weak
    for record in records.values():
        nid, library = record["nid"], record["library"]
        providers = exports.get(nid, [])
        if not library:
            providers = sorted(set(providers) | set((module_exports or {}).get(nid, [])))
        resolved_nid = nid
        record["symbol_mismatch"] = False
        if not providers:
            resolved_nid = nid_names.compute_nid(nid)
            providers = exports.get(resolved_nid, [])
            record["symbol_mismatch"] = bool(library) and library not in modules and bool(providers) and resolved_nid != nid
            if not library:
                providers = sorted(set(providers) | set((module_exports or {}).get(resolved_nid, [])))
        kind = classify(resolved_nid, library, providers, stubs, modules, partial)
        record["class"] = kind
        record["providers"] = providers
        record["library_mismatch"] = kind in ("implemented", "partial", "stub") and bool(library) and system_library(library) not in providers and "libc.prx" not in providers
        record["name"] = next((stubs[(provider, resolved_nid)] for provider in providers if (provider, resolved_nid) in stubs), None) if kind in ("partial", "stub") else None
    ordered = sorted(records.values(), key=lambda record: (record["class"], record["library"] or "", record.get("module") or "", record["nid"]))
    check_conservation(len(imports) + sum(len(symbols) for symbols, _ in (module_imports or {}).values()), ordered)
    missing = defaultdict(int)
    for library in needed_libraries:
        if not available(library):
            missing[library] = 0
    for record in ordered:
        library = record["library"]
        if library and not available(library):
            missing[library] += 1
    for _, needed in (module_imports or {}).values():
        for library in needed:
            if not available(library):
                missing[library] += 1
    return ordered, dict(sorted(missing.items()))


def current_stop(imports, records):
    """The first import the loader cannot resolve, in registry order; a weak import resolves to null instead."""
    classes = {(record["nid"], record["library"]): record for record in records}
    for nid, library, weak in imports:
        if weak:
            continue
        record = classes.get((nid, library))
        if record is not None and (record["class"] == "absent" or record["symbol_mismatch"]):
            return record
    return None


def load_names(path):
    if not Path(path).is_file():
        raise AuditError(f"{path}: names file not found; the audit never downloads one (see tools/nid_names.py for the source)")
    return nid_names.load_db(Path(path))


def attach_names(records, names):
    for record in records:
        if record["name"] is None and record["class"] == "absent" and record["nid"] in names:
            name = names[record["nid"]]
            record["name"] = name
            record["name_verified"] = nid_names.compute_nid(name) == record["nid"]


def write_json(path, result):
    if path.is_dir():
        raise AuditError(f"{path}: JSON output is a directory")
    destination = path.resolve()
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=destination.name + ".", suffix=".tmp") as directory:
        temporary = Path(directory) / destination.name
        try:
            temporary.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            if destination.exists():
                temporary.chmod(destination.stat().st_mode & 0o7777)
            temporary.replace(destination)
        finally:
            if temporary.exists():
                temporary.chmod(0o600)
            temporary.unlink(missing_ok=True)


def git_stamp(source=ROOT):
    try:
        commit = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None, None
    try:
        dirty = bool(subprocess.run(["git", "-C", str(source), "status", "--porcelain"], capture_output=True, text=True, check=True).stdout.strip())
    except (OSError, subprocess.CalledProcessError):
        dirty = None
    return commit, dirty


def summarize(records, references, missing, missing_hosts=()):
    counts = {kind: sum(1 for record in records if record["class"] == kind) for kind in CLASSES}
    by_references = {kind: sum(record["references"] for record in records if record["class"] == kind) for kind in CLASSES}
    return {
        "references": references,
        "unique_imports": len(records),
        "unique_by_class": counts,
        "references_by_class": by_references,
        "missing_libraries": missing,
        "missing_host_dependencies": [{"importer": entry["importer"], "chain": list(entry["chain"])} for entry in missing_hosts],
        "library_mismatch": sum(1 for record in records if record["library_mismatch"]),
        "symbol_mismatch": sum(1 for record in records if record["symbol_mismatch"]),
    }


def label(record):
    if record["name"] is None:
        text = record["nid"]
    else:
        mark = "" if record.get("name_verified", True) else " (name does not hash to this NID)"
        text = f"{record['nid']}  {record['name']}{mark}"
    if record["weak"]:
        text += "  (weak import)"
    return f"{text}  (imported by {record['module']})" if record.get("module") else text


def locate_missing(names, roots, depth=4):
    wanted = {name.lower(): name for name in names}
    found = {}
    for root in roots:
        root = Path(root)
        if not root.is_dir():
            continue
        base = len(root.parts)
        for directory, directories, files in os.walk(root):
            if len(Path(directory).parts) - base >= depth:
                directories[:] = []
            for file in files:
                name = wanted.get(file.lower())
                if name is not None and name not in found:
                    found[name] = Path(directory)
    return found


def render(summary, records, missing, stops, missing_hosts=(), located=None):
    lines = [f"{summary['references']} references, {summary['unique_imports']} unique imports"]
    for kind in CLASSES:
        lines.append(f"  {kind:<12}{summary['unique_by_class'][kind]:>6} imports  {summary['references_by_class'][kind]:>6} references")
    for title, kind in (("Absent: no built library exports these", "absent"), ("Stub: exported but throws when called", "stub"),
                        ("Partial: exported, throws only for inputs whose behaviour is unknown", "partial")):
        selected = [record for record in records if record["class"] == kind]
        if selected:
            lines.append("")
            lines.append(f"{title} ({len(selected)})")
            current = False
            for record in selected:
                if record["library"] != current:
                    current = record["library"]
                    lines.append(f"  [{current or 'library not named'}]")
                lines.append(f"    {label(record)}")
    mismatched = [record for record in records if record["symbol_mismatch"]]
    if mismatched:
        lines.extend(["", f"Qualified import names exported only as their NID ({len(mismatched)}): the loader cannot resolve these names"])
        lines.extend(f"  {label(record)} -> {nid_names.compute_nid(record['nid'])}" for record in mismatched)
    if stops:
        lines.append("")
        lines.append("Current stop (first unresolved import, in registry order)")
        for entry in stops:
            stop = entry["stop"]
            lines.append(f"  {entry['registry']}: none" if stop is None else f"  {entry['registry']}: {label(stop)} [{stop['library']}]")
    if missing:
        lines.append("")
        lines.append(f"Needed libraries with no file in --libs or --modules ({len(missing)})")
        lines.extend(f"  {library}: {count} imports" for library, count in missing.items())
        if any(count == 0 for count in missing.values()):
            lines.append("  Libraries with 0 imports are declared dependencies; Windows can skip them if no import needs them")
        for library, directory in sorted((located or {}).items()):
            lines.append(f"  {library} is in {directory}: pass that directory with --modules")
    if missing_hosts:
        lines.append("")
        lines.append(f"Host dependencies with no file in --libs or --modules ({len(missing_hosts)})")
        lines.extend(f"  Importer: {entry['importer']} Chain: {' -> '.join(entry['chain'])}" for entry in missing_hosts)
    if summary["library_mismatch"]:
        lines.append("")
        lines.append(f"{summary['library_mismatch']} imports are exported only by a library other than the one they name: they resolve on Linux and can fail on Windows")
    return "\n".join(lines)


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="classify the imports of a converted game against the built system libraries")
    parser.add_argument("registries", nargs="+", type=Path, help="JSON written by 'relinker --registry'")
    parser.add_argument("--libs", action="append", required=True, help="directory of built .prx files; repeat for several")
    parser.add_argument("--modules", action="append", default=[], help="directory of the title's own modules; each module's own imports are audited")
    parser.add_argument("--source", default=str(PRX), help="core/libs/prx tree used to find throwing stubs (default: this repository's)")
    parser.add_argument("--names", help="aerolib.csv style file (NID, name per line) used to name absent imports")
    parser.add_argument("--json", type=Path, help="write the full result to this file")
    parser.add_argument("--strict-stubs", action="store_true",
                        help="also fail when a referenced import resolves to a throwing stub")
    parser.add_argument("--strict-providers", action="store_true", help="fail when a named import is provided only by another library; retains the existing libc and title-module rules")
    args = parser.parse_args(argv)
    try:
        dependency_registries = [needed_registry_path(path) for path in args.registries if path.name.endswith(".registry.json")]
        dependency_registries = [path for path in dependency_registries if path.exists()]
        if args.json:
            inputs = args.registries + dependency_registries + ([Path(args.names)] if args.names else [])
            inputs += [file for directory in args.libs for file in directory_files(directory, "*.prx")]
            inputs += [file for directory in args.modules for file in directory_files(directory, "*")]
            inputs += list(Path(args.source).rglob("*.cpp"))
            inputs += [file for file in Path(args.source).glob("*/CMakeLists.txt") if file.is_file()]
            output = args.json.resolve()
            if any(output == path.resolve() or (args.json.exists() and args.json.samefile(path)) for path in inputs):
                raise AuditError(f"{args.json}: JSON output would overwrite an input file")
        registries = [(registry, read_registry(registry)) for registry in args.registries]
        imports = [entry for _, entries in registries for entry in entries]
        needed = sorted({library for path in dependency_registries for library in read_needed_registry(path)})
        exports, libraries = built_libraries(args.libs)
        host_dependencies, windows = built_host_dependencies(args.libs)
        module_imports, module_exports = read_modules(args.modules)
        modules = set(module_imports)
        modules |= {name.removesuffix(".guest.prx") for name in modules if name.endswith(".guest.prx")}
        modules = ModuleNames(modules, fold=bool(windows))
        stubs, partial = source_stub_classes(args.source)
        records, missing = audit(imports, exports, stubs, libraries, modules, module_imports, module_exports, needed_libraries=needed, windows_libraries=windows, partial=partial)
        missing_hosts = audit_host_dependencies(host_dependencies, libraries, modules, windows)
        if args.names:
            attach_names(records, load_names(args.names))
        summary = summarize(records, sum(record["references"] for record in records), missing, missing_hosts)
        stops = [{"registry": str(path), "stop": current_stop(entries, records)} for path, entries in registries]
        if args.json:
            commit, dirty = git_stamp(Path(args.source))
            json_stops = [{"registry": entry["registry"],
                           "nid": entry["stop"]["nid"] if entry["stop"] else None,
                           "library": entry["stop"]["library"] if entry["stop"] else None,
                           "name": entry["stop"]["name"] if entry["stop"] else None} for entry in stops]
            result = {"source_commit": commit, "source_dirty": dirty, "registries": [str(path) for path in args.registries], "dependency_registries": [str(path) for path in dependency_registries], "needed_libraries": needed, **summary, "stops": json_stops, "imports": records}
            write_json(args.json, result)
    except AuditError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"FAIL: {error.strerror}: {error.filename}" if error.filename else f"FAIL: {error}", file=sys.stderr)
        return 2
    located = locate_missing(missing, {path.resolve().parent for path in args.registries}) if missing else None
    print(render(summary, records, missing, stops, missing_hosts, located))
    return 1 if summary["unique_by_class"]["absent"] or summary["symbol_mismatch"] or missing or missing_hosts or (args.strict_providers and summary["library_mismatch"]) or (args.strict_stubs and summary["unique_by_class"]["stub"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
