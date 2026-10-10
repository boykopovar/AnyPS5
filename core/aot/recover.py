import argparse
import hashlib
import io
import json
from collections import Counter
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM
from elftools.dwarf.callframe import FDE
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection


class RecoveryError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise RecoveryError(message)


def load_elf(path):
    raw = Path(path).read_bytes()
    elf = ELFFile(io.BytesIO(raw))
    require(elf.elfclass == 64 and elf.little_endian and elf['e_machine'] == 'EM_X86_64',
            'recovery requires a little-endian x86-64 ELF')
    return raw, elf


def read_address(elf, address, size):
    segments = [s for s in elf.iter_segments() if s['p_type'] == 'PT_LOAD'
                and s['p_vaddr'] <= address and address + size <= s['p_vaddr'] + s['p_filesz']]
    require(len(segments) == 1, f'not one file-backed segment for 0x{address:x}+{size}')
    segment = segments[0]
    return segment.data()[address - segment['p_vaddr']:address - segment['p_vaddr'] + size]


def masked_matches(pattern, mask, data):
    require(len(pattern) == len(mask) and len(pattern) > 0, 'invalid section matching input')
    ranges = []
    start = None
    for index, fixed in enumerate([*mask, False]):
        if fixed and start is None:
            start = index
        elif not fixed and start is not None:
            ranges.append((start, index))
            start = None
    require(ranges, 'executable section has no invariant bytes')
    begin, end = max(ranges, key=lambda pair: pair[1] - pair[0])
    anchor = pattern[begin:end]
    result, cursor = [], 0
    while True:
        found = data.find(anchor, cursor)
        if found < 0:
            return result
        cursor = found + 1
        base = found - begin
        if base >= 0 and base + len(pattern) <= len(data) and all(
                data[base + low:base + high] == pattern[low:high] for low, high in ranges):
            result.append(base)


def object_mappings(image, object_path):
    raw, obj = load_elf(object_path)
    require(obj['e_type'] == 'ET_REL', 'companion evidence must be relocatable ELF')
    executable = [s for s in image.iter_segments() if s['p_type'] == 'PT_LOAD' and s['p_flags'] & 1]
    mappings = {}
    for index, section in enumerate(obj.iter_sections()):
        if not section['sh_flags'] & 4 or not section['sh_size']:
            continue
        mask = [True] * section['sh_size']
        for relocations in obj.iter_sections():
            if not isinstance(relocations, RelocationSection) or relocations['sh_info'] != index:
                continue
            for relocation in relocations.iter_relocations():
                widths = {0: 0, 1: 8, 2: 4, 4: 4, 10: 4, 11: 4, 24: 8, 41: 4, 42: 4}
                kind, offset = relocation['r_info_type'], relocation['r_offset']
                require(kind in widths, f'unknown executable relocation width: {kind}')
                width = widths[kind]
                require(0 <= offset and offset + width <= len(mask), 'relocation outside companion section')
                mask[offset:offset + width] = [False] * width
        matches = [segment['p_vaddr'] + offset for segment in executable
                   for offset in masked_matches(section.data(), mask, segment.data())]
        require(len(matches) == 1, f'{object_path}:{section.name} has {len(matches)} image matches')
        mappings[index] = {'section': section.name, 'address': matches[0], 'size': section['sh_size'],
                           'invariant_bytes': sum(mask), 'relocated_bytes': len(mask) - sum(mask)}
    return obj, {'path': str(object_path), 'sha256': hashlib.sha256(raw).hexdigest(),
                 'sections': list(mappings.values())}, mappings


def symbol_candidates(elf, mappings=None, origin='image'):
    result = []
    for table in elf.iter_sections():
        if not isinstance(table, SymbolTableSection):
            continue
        for symbol in table.iter_symbols():
            index = symbol['st_shndx']
            if not isinstance(index, int) or not symbol.name:
                continue
            section = elf.get_section(index)
            if not section['sh_flags'] & 4 or symbol['st_info']['type'] not in ('STT_FUNC', 'STT_NOTYPE'):
                continue
            if mappings is not None and index not in mappings:
                continue
            address = symbol['st_value'] + (mappings[index]['address'] if mappings is not None else 0)
            result.append({'address': address, 'size': symbol['st_size'], 'symbol': symbol.name,
                           'evidence': [{'kind': 'elf-symbol', 'origin': origin, 'table': table.name,
                                         'symbol_type': symbol['st_info']['type']}], 'abi': None})
    return result


def fde_candidates(elf):
    if elf.get_section_by_name('.eh_frame') is None:
        return []
    result = []
    for entry in elf.get_dwarf_info().EH_CFI_entries():
        if isinstance(entry, FDE):
            result.append({'address': entry['initial_location'], 'size': entry['address_range'], 'symbol': None,
                           'evidence': [{'kind': 'eh-frame-range', 'offset': entry.offset}], 'abi': None})
    return result


def dynamic_facts(elf):
    dynamic = next((s for s in elf.iter_segments() if s['p_type'] == 'PT_DYNAMIC'), None)
    if dynamic is None:
        return [], [], []
    tags = {t.entry.d_tag: t.entry.d_val for t in dynamic.iter_tags()}
    require(tags.get('DT_RELAENT', 24) == 24, 'unsupported dynamic relocation size')
    symbols = elf.get_section_by_name('.dynsym')
    imports, pointers, unknowns = {}, [], []
    for address, size, kind in [(tags.get('DT_JMPREL', 0), tags.get('DT_PLTRELSZ', 0), 'plt'),
                                (tags.get('DT_RELA', 0), tags.get('DT_RELASZ', 0), 'data')]:
        require(size % 24 == 0, 'truncated dynamic relocation table')
        if not size:
            continue
        raw = read_address(elf, address, size)
        for index in range(0, size, 24):
            slot = int.from_bytes(raw[index:index + 8], 'little')
            info = int.from_bytes(raw[index + 8:index + 16], 'little')
            addend = int.from_bytes(raw[index + 16:index + 24], 'little', signed=True)
            relocation_type, symbol_index = info & 0xffffffff, info >> 32
            if kind == 'plt' and relocation_type == 7 and addend == 0:
                require(symbols is not None and symbol_index < symbols.num_symbols(), 'missing dynamic symbol')
                name = symbols.get_symbol(symbol_index).name
                item = imports.setdefault(name, {'symbol': name, 'slots': [], 'abi': None})
                item['slots'].append(slot)
            elif kind == 'data' and relocation_type == 8 and symbol_index == 0:
                pointers.append([slot, addend])
            else:
                unknowns.append({'kind': 'relocation', 'table': kind, 'address': slot,
                                 'type': relocation_type, 'symbol_index': symbol_index, 'addend': addend})
    for tag in ('DT_INIT_ARRAYSZ', 'DT_FINI_ARRAYSZ', 'DT_PREINIT_ARRAYSZ'):
        if tags.get(tag, 0):
            unknowns.append({'kind': 'lifecycle', 'tag': tag, 'size': tags[tag]})
    return list(imports.values()), pointers, unknowns


def instruction_facts(elf, candidate):
    if not candidate['size']:
        return {'unknown': 'symbol has no size; no boundary guessed'}
    raw = read_address(elf, candidate['address'], candidate['size'])
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    instructions = list(decoder.disasm(raw, candidate['address']))
    calls, frame_accesses = [], []
    for ins in instructions:
        if ins.mnemonic == 'call' or ins.mnemonic.startswith('j'):
            calls.append({'address': ins.address, 'kind': ins.mnemonic,
                          'target': ins.operands[0].imm if ins.operands[0].type == X86_OP_IMM else None,
                          'operand': ins.op_str})
        for operand in ins.operands:
            if operand.type == X86_OP_MEM and ins.reg_name(operand.mem.base) in ('rsp', 'rbp'):
                frame_accesses.append({'address': ins.address, 'base': ins.reg_name(operand.mem.base),
                                       'displacement': operand.mem.disp, 'index': ins.reg_name(operand.mem.index),
                                       'size': operand.size, 'access': operand.access})
    return {'decode': 'linear range inventory, not control-flow completeness proof',
            'decoded_bytes': sum(ins.size for ins in instructions), 'instructions': len(instructions),
            'mnemonics': dict(sorted(Counter(ins.mnemonic for ins in instructions).items())),
            'branches': calls, 'stack_accesses': frame_accesses,
            'stack_objects': None}


def recover_image(path, objects=()):
    raw, elf = load_elf(path)
    require(elf['e_type'] != 'ET_REL', 'input must be a linked executable image')
    functions = symbol_candidates(elf) + fde_candidates(elf)
    companions = []
    for companion in objects:
        obj, evidence, mappings = object_mappings(elf, companion)
        companions.append(evidence)
        functions.extend(symbol_candidates(obj, mappings, str(companion)))
    merged = {}
    for candidate in functions:
        key = (candidate['address'], candidate['size'])
        if key in merged:
            merged[key]['evidence'].extend(candidate['evidence'])
            merged[key]['symbol'] = merged[key]['symbol'] or candidate['symbol']
        else:
            merged[key] = candidate
    functions = sorted(merged.values(), key=lambda f: (f['address'], f['size']))
    for candidate in functions:
        candidate['analysis'] = instruction_facts(elf, candidate)
    imports, pointers, unknowns = dynamic_facts(elf)
    data = [{'address': s['p_vaddr'], 'size': s['p_memsz']} for s in elf.iter_segments()
            if s['p_type'] == 'PT_LOAD' and not s['p_flags'] & 1 and s['p_memsz']]
    for segment in elf.iter_segments():
        if segment['p_type'] == 'PT_TLS' and segment['p_memsz']:
            unknowns.append({'kind': 'tls', 'size': segment['p_memsz']})
    return {'schema': 'anyps5.aot.recovery.v1', 'sha256': hashlib.sha256(raw).hexdigest(), 'entry': elf['e_entry'],
            'functions': functions, 'imports': imports, 'data': data, 'pointer_relocations': pointers,
            'companions': companions, 'unknowns': unknowns,
            'limitations': ['Function ranges do not establish ABI or code coverage.',
                            'Stack accesses do not establish object boundaries or pointer provenance.',
                            'Object matching verifies invariant bytes; linked relocation semantics need compiler validation.']}


def verify_evidence(evidence, directory):
    require(isinstance(evidence, list) and evidence, 'ABI annotation needs source evidence')
    for item in evidence:
        path = Path(directory) / item['path']
        raw = path.read_bytes()
        require(hashlib.sha256(raw).hexdigest() == item['sha256'], f'stale source evidence: {path}')
        excerpt = item.get('excerpt', '')
        require(excerpt and excerpt in raw.decode(), f'declaration excerpt absent from {path}')


def materialize_contracts(recovery, annotations, directory='.'):
    require(annotations.get('schema') == 1, 'unsupported annotation schema')
    require(annotations['sha256'] == recovery['sha256'], 'annotation input hash mismatch')
    require(not recovery['unknowns'], 'unsupported image obligations remain unresolved')
    candidates = {}
    for candidate in recovery['functions']:
        address = candidate['address']
        if address in candidates:
            previous = candidates[address]
            require(not previous['size'] or not candidate['size'] or previous['size'] == candidate['size'],
                    f'conflicting function range evidence at 0x{address:x}')
            if previous['size']:
                continue
        candidates[address] = candidate
    required_imports = {item['symbol'] for item in recovery['imports']}
    require(set(annotations['imports']) == required_imports, 'import annotations do not match image imports')
    functions = []
    types = {'void', 'i8', 'u8', 'i16', 'u16', 'i32', 'u32', 'i64', 'u64', 'ptr', 'f32', 'f64'}
    for function in annotations['functions']:
        candidate = candidates.get(function['address'])
        require(candidate is not None, f'function has no recovered evidence: {function["name"]}')
        require(function['size'] > 0 and candidate['size'] in (0, function['size']), 'annotated function range differs from evidence')
        verify_evidence(function.get('evidence'), directory)
        functions.append({key: value for key, value in function.items() if key != 'evidence'})
    require(len({f['address'] for f in functions}) == len(functions), 'duplicate function annotation')
    require({f['address'] for f in functions} == set(candidates), 'function annotations do not cover every recovered candidate')
    require(recovery['entry'] in candidates, 'entry point has no recovered boundary evidence')
    ordered = sorted(functions, key=lambda function: function['address'])
    for left, right in zip(ordered, ordered[1:]):
        require(left['address'] + left['size'] <= right['address'], 'overlapping annotated function ranges')
    for function in [*functions, *annotations['imports'].values()]:
        require(function['result'] in types and all(t in types - {'void'} for t in function['parameters']),
                'unsupported declared ABI type')
    for function in annotations['imports'].values():
        verify_evidence(function.get('evidence'), directory)
    return {'schema': 1, 'sha256': recovery['sha256'], 'functions': functions,
            'imports': {name: {key: value for key, value in function.items() if key != 'evidence'}
                        for name, function in annotations['imports'].items()},
            'data': recovery['data'], 'pointer_relocations': recovery['pointer_relocations'],
            'provenance': {'abi': 'Explicit source-verified annotations; not ABI inference',
                           'annotations': annotations}}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('--object', action='append', default=[], type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--annotations', type=Path)
    parser.add_argument('--contracts', type=Path)
    args = parser.parse_args()
    recovery = recover_image(args.input, args.object)
    args.output.write_text(json.dumps(recovery, indent=2) + '\n')
    require(bool(args.annotations) == bool(args.contracts), 'annotations and contracts output must be specified together')
    if args.annotations:
        annotations = json.loads(args.annotations.read_text())
        contracts = materialize_contracts(recovery, annotations, args.annotations.parent)
        args.contracts.write_text(json.dumps(contracts, indent=2) + '\n')


if __name__ == '__main__':
    main()
