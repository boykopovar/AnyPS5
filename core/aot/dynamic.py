import struct

from elftools.elf.enums import ENUM_D_TAG


class DynamicError(ValueError):
    pass


MAX64 = (1 << 64) - 1
PT_SCE_DYNLIBDATA = 0x61000000
SCE_TAGS = {2: 0x6100002d, 3: 0x61000027, 5: 0x61000035, 6: 0x61000039,
            7: 0x6100002f, 8: 0x61000031, 9: 0x61000033, 10: 0x61000037,
            11: 0x6100003b, 20: 0x6100002b, 23: 0x61000029}
SCE_SYMBOL_SIZE = 0x6100003f
LIFECYCLE_TAGS = {12: 0x6000000c, 13: 0x6000000d, 25: 0x60000019,
                  26: 0x6000001a, 27: 0x6000001b, 28: 0x6000001c}
REPEATABLE = {1, 0x61000019, 0x61000045, 0x61000049, 0x7ffffffd, 0x7fffffff}
TAG_NAMES = {value: name for name, value in ENUM_D_TAG.items() if isinstance(value, int)}
RELOCATION_WIDTHS = {0: 0, 1: 8, 6: 8, 7: 8, 8: 8, 16: 8, 17: 8, 18: 8}


def require(condition, message):
    if not condition:
        raise DynamicError(message)


def range_fits(offset, size, limit):
    return 0 <= offset <= limit and 0 <= size <= limit - offset


def parse_dynamic(elf, read_mapped_address):
    require(elf.elfclass == 64 and elf.little_endian and elf['e_machine'] == 'EM_X86_64',
            'dynamic parsing requires little-endian x86-64 ELF')
    stream = elf.stream
    position = stream.tell()
    stream.seek(0, 2)
    file_size = stream.tell()
    stream.seek(position)
    require(elf['e_phentsize'] == 56 and 0 < elf['e_phnum'] < 0xffff, 'invalid program header count or size')
    require(range_fits(elf['e_phoff'], elf['e_phnum'] * 56, file_size), 'truncated program header table')
    segments = list(elf.iter_segments())
    loads = [segment for segment in segments if segment['p_type'] == 'PT_LOAD']
    dynamic_segments = [segment for segment in segments if segment['p_type'] == 'PT_DYNAMIC']
    sce_segments = [segment for segment in segments if segment['p_type'] == PT_SCE_DYNLIBDATA]
    require(loads and len(dynamic_segments) == 1, 'expected load segments and exactly one dynamic segment')
    require(len(sce_segments) <= 1, 'duplicate SCE dynamic data segment')
    for segment in [*loads, *dynamic_segments, *sce_segments]:
        require(range_fits(segment['p_offset'], segment['p_filesz'], file_size), 'segment exceeds input file')
        if segment['p_type'] == 'PT_LOAD':
            require(segment['p_filesz'] <= segment['p_memsz'], 'load file size exceeds memory size')
            require(range_fits(segment['p_vaddr'], segment['p_memsz'], MAX64), 'load address range overflows')
    dynamic = dynamic_segments[0]
    require(dynamic['p_filesz'] > 0 and dynamic['p_filesz'] % 16 == 0, 'invalid dynamic segment size')

    def read_file(offset, size):
        require(range_fits(offset, size, file_size), 'table exceeds input file')
        saved = stream.tell()
        try:
            stream.seek(offset)
            result = stream.read(size)
        finally:
            stream.seek(saved)
        require(len(result) == size, 'truncated table read')
        return result

    def mapping(address, size, file_backed=True):
        require(range_fits(address, size, MAX64), 'mapped address range overflows')
        key = 'p_filesz' if file_backed else 'p_memsz'
        matches = [segment for segment in loads if address >= segment['p_vaddr']
                   and range_fits(address - segment['p_vaddr'], size, segment[key])]
        require(len(matches) == 1, 'table or relocation has ambiguous or unmapped address range')
        return matches[0]

    def read_mapped(address, size):
        segment = mapping(address, size)
        offset = segment['p_offset'] + address - segment['p_vaddr']
        require(range_fits(offset, size, file_size), 'mapped table exceeds input file')
        result = read_mapped_address(address, size)
        require(isinstance(result, (bytes, bytearray)) and len(result) == size, 'truncated mapped table read')
        return bytes(result)

    raw_tags, values = [], {}
    terminated = False
    for tag, value in struct.iter_unpack('<qQ', read_file(dynamic['p_offset'], dynamic['p_filesz'])):
        if tag == 0:
            terminated = True
            break
        raw_tags.append((tag, value))
        if tag in REPEATABLE or (0x61000000 <= tag <= 0x6100ffff and tag not in {*SCE_TAGS.values(), SCE_SYMBOL_SIZE}):
            continue
        require(tag not in values, f'duplicate dynamic tag: {TAG_NAMES.get(tag, hex(tag))}')
        values[tag] = value
    require(terminated, 'unterminated dynamic segment')
    for standard, sce in (SCE_TAGS | LIFECYCLE_TAGS).items():
        require(not (standard in values and sce in values), f'ambiguous standard/SCE tag: {TAG_NAMES.get(standard, standard)}')
    for tag in (17, 18, 19, 35, 36, 37):
        require(tag not in values, 'REL and RELR relocation tables require separate parsing')

    def has(tag):
        return tag in values or SCE_TAGS.get(tag) in values

    def get(tag, default=None):
        if tag in values:
            return values[tag]
        sce = SCE_TAGS.get(tag)
        if sce in values:
            return values[sce]
        require(default is not None, f'missing dynamic tag: {TAG_NAMES.get(tag, tag)}')
        return default

    def table(tag, size):
        value = get(tag)
        if tag in values:
            return read_mapped(value, size)
        require(len(sce_segments) == 1, 'SCE table requires PT_SCE_DYNLIBDATA')
        sce = sce_segments[0]
        require(range_fits(value, size, sce['p_filesz']), 'SCE table exceeds dynamic data segment')
        return read_file(sce['p_offset'] + value, size)

    string_size = get(10)
    require(string_size > 0, 'empty dynamic string table')
    strings = table(5, string_size)
    require(strings[0] == 0, 'dynamic string table lacks null entry')

    def string(offset):
        require(0 <= offset < len(strings), 'dynamic string offset out of bounds')
        end = strings.find(b'\0', offset)
        require(end >= 0, 'unterminated dynamic string')
        try:
            return strings[offset:end].decode('utf-8')
        except UnicodeDecodeError as error:
            raise DynamicError('dynamic string is not UTF-8') from error

    require(get(11) == 24, 'unsupported dynamic symbol entry size')
    counts, hash_words = [], None
    if SCE_SYMBOL_SIZE in values:
        size = values[SCE_SYMBOL_SIZE]
        require(size > 0 and size % 24 == 0, 'invalid SCE dynamic symbol size')
        counts.append(size // 24)
    if 4 in values:
        buckets, chains = struct.unpack('<II', read_mapped(values[4], 8))
        require(buckets > 0 and chains > 0, 'invalid SysV hash dimensions')
        raw_hash = read_mapped(values[4], (2 + buckets + chains) * 4)
        words = struct.unpack(f'<{buckets + chains}I', raw_hash[8:])
        require(all(index < chains for index in words), 'SysV hash symbol index out of range')
        hash_words = words[buckets:]
        require(hash_words[0] == 0, 'invalid null SysV hash chain')
        counts.append(chains)
    if not counts:
        require(elf['e_shnum'] > 0 and elf['e_shentsize'] == 64,
                'missing bounded dynamic symbol count')
        require(range_fits(elf['e_shoff'], elf['e_shnum'] * 64, file_size), 'truncated section table')
        sections = [section for section in elf.iter_sections() if section['sh_type'] == 'SHT_DYNSYM']
        matches = [section for section in sections if 6 in values and section['sh_addr'] == values[6]]
        require(len(matches) == 1, 'missing unique dynamic symbol section fallback')
        section = matches[0]
        require(section['sh_entsize'] == 24 and section['sh_size'] > 0 and section['sh_size'] % 24 == 0,
                'invalid dynamic symbol section size')
        mapped = mapping(values[6], section['sh_size'])
        expected = mapped['p_offset'] + values[6] - mapped['p_vaddr']
        require(section['sh_offset'] == expected, 'dynamic symbol section does not match mapped table')
        counts.append(section['sh_size'] // 24)
    require(len(set(counts)) == 1 and counts[0] <= 0xffffffff, 'conflicting or invalid dynamic symbol counts')
    symbol_count = counts[0]
    raw_symbols = table(6, symbol_count * 24)
    if hash_words:
        done = {0}
        for initial in range(1, symbol_count):
            seen, current = set(), initial
            while current not in done:
                require(current not in seen, 'cyclic SysV hash chain')
                seen.add(current)
                current = hash_words[current]
            done.update(seen)
    symbols = []
    for name, info, other, section, value, size in struct.iter_unpack('<IBBHQQ', raw_symbols):
        symbol = {'name': string(name), 'info': info, 'other': other, 'section': section, 'value': value, 'size': size}
        if not symbols:
            require(name == info == other == section == value == size == 0, 'invalid null dynamic symbol')
        else:
            require(info >> 4 <= 2 and info & 15 in (0, 1, 2, 6) and other & ~3 == 0, 'unsupported dynamic symbol attributes')
            if section and section != 0xfff1:
                require(section < 0xff00, 'unsupported special dynamic symbol section')
                if info & 15 != 6:
                    mapping(value, max(size, 1), file_backed=False)
        symbols.append(symbol)
    relocations, target_ranges = [], []
    for address_tag, size_tag, plt in ((7, 8, False), (23, 2, True)):
        if not has(address_tag) and not has(size_tag):
            continue
        require(has(address_tag) and has(size_tag), 'incomplete relocation table')
        size = get(size_tag)
        require(size % 24 == 0, 'invalid relocation table size')
        if has(9):
            require(get(9) == 24, 'unsupported relocation entry size')
        elif size and not plt:
            require(False, 'missing dynamic relocation entry size')
        if plt:
            require(get(20) == 7, 'PLT relocation format is not RELA')
        for slot, info, addend in struct.iter_unpack('<QQq', table(address_tag, size)):
            kind, index = info & 0xffffffff, info >> 32
            require(kind in RELOCATION_WIDTHS and index < symbol_count, 'unsupported relocation kind or symbol index')
            require(not plt or kind == 7, 'non-JUMP_SLOT relocation in PLT table')
            if kind != 0:
                width = RELOCATION_WIDTHS[kind]
                mapping(slot, width, file_backed=False)
                target_ranges.append((slot, slot + width))
            if kind == 8:
                require(index == 0, 'RELATIVE relocation has a symbol index')
            elif kind in (1, 6, 7):
                require(index != 0 and symbols[index]['info'] & 15 != 6, 'invalid symbol relocation')
                require(symbols[index]['section'] != 0 or symbols[index]['name'], 'unnamed imported symbol')
                require(kind == 1 or addend == 0, 'GLOB_DAT/JUMP_SLOT relocation has a nonzero addend')
            elif kind in (16, 17, 18):
                require(index == 0 or symbols[index]['info'] & 15 == 6, 'TLS relocation references a non-TLS symbol')
                require(kind != 16 or addend == 0, 'DTPMOD relocation has a nonzero addend')
            relocations.append({'slot': slot, 'kind': kind, 'symbol_index': index, 'addend': addend, 'plt': plt})
    target_ranges.sort()
    require(all(left[1] <= right[0] for left, right in zip(target_ranges, target_ranges[1:])), 'overlapping relocation targets')
    tags = {TAG_NAMES.get(tag, tag): value for tag, value in values.items()}
    for standard, sce in LIFECYCLE_TAGS.items():
        if sce in values:
            tags[TAG_NAMES[standard]] = values[sce]
    needed = [string(value) for tag, value in raw_tags if tag == 1]
    require(all(needed), 'empty needed module name')
    return {'tags': tags, 'raw_tags': raw_tags, 'symbols': symbols, 'relocations': relocations, 'needed': needed}
