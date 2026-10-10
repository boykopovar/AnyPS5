import argparse
import hashlib
import io
import json
import re
import subprocess
from collections import deque
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG
from elftools.elf.elffile import ELFFile
import abi
import vector
import avx
import integer
from dynamic import parse_dynamic, DynamicError
from calls import validate_indirect_calls, CallError


class Rejected(ValueError):
    pass


REGISTERS = ['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi'] + [f'r{i}' for i in range(8, 16)]
VECTOR_REGISTERS = [f'xmm{i}' for i in range(16)]
VECTOR_STORAGE = VECTOR_REGISTERS + [f'ymm_hi{i}' for i in range(16)]
ARGUMENTS = ['rdi', 'rsi', 'rdx', 'rcx', 'r8', 'r9']
VOLATILE = {'rax', 'rcx', 'rdx', 'rsi', 'rdi', 'r8', 'r9', 'r10', 'r11'}
ALIASES = {}
for full, dword, word, low in zip(REGISTERS[:8], ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi'],
                                ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di'],
                                ['al', 'cl', 'dl', 'bl', 'spl', 'bpl', 'sil', 'dil']):
    for name, bits, shift in [(full, 64, 0), (dword, 32, 0), (word, 16, 0), (low, 8, 0)]:
        ALIASES[name] = (full, bits, shift)
for i, name in enumerate(['ah', 'ch', 'dh', 'bh']):
    ALIASES[name] = (REGISTERS[i], 8, 8)
for full in REGISTERS[8:]:
    for suffix, bits in [('', 64), ('d', 32), ('w', 16), ('b', 8)]:
        ALIASES[full + suffix] = (full, bits, 0)
for name in VECTOR_REGISTERS:
    ALIASES[name] = (name, 128, 0)
TYPES = abi.TYPES
MASK64 = (1 << 64) - 1
FLAGS = {'zf', 'sf', 'cf', 'of', 'pf'}
CONDITIONS = {'e': {'zf'}, 'ne': {'zf'}, 'b': {'cf'}, 'ae': {'cf'}, 'a': {'cf', 'zf'},
              'be': {'cf', 'zf'}, 'l': {'sf', 'of'}, 'ge': {'sf', 'of'}, 'g': {'sf', 'of', 'zf'},
              'le': {'sf', 'of', 'zf'}, 's': {'sf'}, 'ns': {'sf'}, 'p': {'pf'}, 'np': {'pf'},
              'o': {'of'}, 'no': {'of'}}


def require(condition, message):
    if not condition:
        raise Rejected(message)


def signature(contract):
    try:
        return abi.signature(contract)
    except abi.ABIError as error:
        raise Rejected(str(error)) from error


class Image:
    def __init__(self, path, contracts):
        self.contracts = contracts
        self.bytes = Path(path).read_bytes()
        require(contracts.get('schema') == 1, 'unsupported contract schema')
        require(not contracts.get('native_runtime_registration'), 'native runtime image registration is outside this compiler build')
        require(hashlib.sha256(self.bytes).hexdigest() == contracts['sha256'], 'input hash does not match contracts')
        self.elf = ELFFile(io.BytesIO(self.bytes))
        require(self.elf.elfclass == 64 and self.elf.little_endian and self.elf['e_machine'] == 'EM_X86_64', 'expected little-endian x86-64 ELF')
        self.segments = []
        self.process_parameters = []
        for segment in self.elf.iter_segments():
            if segment['p_type'] == 0x61000001:
                self.process_parameters.append((segment['p_vaddr'], segment['p_filesz']))
            if segment['p_type'] == 'PT_TLS':
                require(segment['p_memsz'] == 0, 'TLS requires native TLS lowering')
            if segment['p_type'] != 'PT_LOAD':
                continue
            require(segment['p_filesz'] <= segment['p_memsz'], 'segment file size exceeds memory size')
            require(segment['p_offset'] + segment['p_filesz'] <= len(self.bytes), 'truncated segment')
            require(segment['p_memsz'] <= 64 * 1024 * 1024, 'segment exceeds current offline allocation limit')
            self.segments.append(segment)
        self.functions = {}
        for function in contracts['functions']:
            signature(function)
            objects = sorted(function.get('stack_objects', []), key=lambda o: o['offset'])
            for index, obj in enumerate(objects):
                require(obj['size'] > 0 and obj['offset'] + obj['size'] <= 0 and obj['offset'] >= -1024 * 1024,
                        'invalid native stack object')
                require(obj['alignment'] in (1, 2, 4, 8, 16), 'unsupported stack object alignment')
                if index:
                    require(objects[index - 1]['offset'] + objects[index - 1]['size'] <= obj['offset'], 'overlapping stack objects')
            address = function['address']
            require(address not in self.functions and function['size'] > 0, 'duplicate or empty function')
            require(self.segment(address, function['size'])['p_flags'] & 1, 'function is outside executable memory')
            self.functions[address] = function
        ordered = sorted(self.functions.values(), key=lambda f: f['address'])
        for left, right in zip(ordered, ordered[1:]):
            require(left['address'] + left['size'] <= right['address'], 'overlapping function contracts')
        require(self.elf['e_entry'] in self.functions, 'entry function has no contract')
        self.imports = contracts['imports']
        for contract in self.imports.values():
            signature(contract)
        self.data = []
        for spec in contracts['data']:
            seg = self.segment(spec['address'], spec['size'])
            require(not seg['p_flags'] & 1, 'executable bytes cannot become native data')
            require(spec['size'] > 0, 'empty data region')
            require(spec['address'] == seg['p_vaddr'] and spec['size'] == seg['p_memsz'], 'data contract must cover the complete segment')
            self.data.append(spec)
        self.import_slots = {}
        self.relocations = []
        try:
            self.dynamic = parse_dynamic(self.elf, self.read)
        except DynamicError as error:
            raise Rejected(str(error)) from error
        tags = self.dynamic['tags']
        self.needed = self.dynamic['needed']
        lifecycle = contracts.get('lifecycle', {})
        for tag, field in (('DT_INIT', 'init'), ('DT_FINI', 'fini')):
            address = tags.get(tag, 0)
            if address:
                require(lifecycle.get('owner') == 'entry' and lifecycle.get(field) == address,
                        'nonzero INIT/FINI requires an explicit entry lifecycle contract')
                require(address in self.functions and signature(self.functions[address]) == ('void', []),
                        'lifecycle function lacks a native void() contract')
        require(tags.get('DT_INIT_ARRAYSZ', 0) == 0 and tags.get('DT_FINI_ARRAYSZ', 0) == 0 and tags.get('DT_PREINIT_ARRAYSZ', 0) == 0,
                'initializer arrays require native lifecycle lowering')
        referenced_imports = set()
        for relocation in self.dynamic['relocations']:
            slot, kind, index, addend = (relocation[key] for key in ('slot', 'kind', 'symbol_index', 'addend'))
            if kind == 0:
                continue
            if kind in (6, 7):
                require(addend == 0, 'function import relocation has a nonzero addend')
                symbol = self.dynamic['symbols'][index]
                require(symbol['info'] & 15 in (0, 2), 'data or TLS import requires a separate native binding')
                name = symbol['name']
                require(name in self.imports, f'missing import ABI: {name}')
                self.import_slots[slot] = self.imports[name]
                referenced_imports.add(name)
            else:
                require(kind == 8 and index == 0, 'unsupported data relocation')
                self.relocations.append((slot, addend))
        expected = {tuple(pair) for pair in contracts['pointer_relocations']}
        require(set(self.relocations) == expected and len(expected) == len(self.relocations), 'pointer relocation contracts do not match the image')
        require(referenced_imports == set(self.imports), 'unused import contracts')
        self.decoder = Cs(CS_ARCH_X86, CS_MODE_64)
        self.decoder.detail = True
        self.call_contracts = {}

    def segment(self, address, size=1):
        matches = [s for s in self.segments if s['p_vaddr'] <= address and address + size <= s['p_vaddr'] + s['p_memsz']]
        require(len(matches) == 1, f'ambiguous or unmapped address 0x{address:x}')
        return matches[0]

    def read(self, address, size):
        segment = self.segment(address, size)
        offset = address - segment['p_vaddr']
        available = max(0, min(size, segment['p_filesz'] - offset))
        begin = segment['p_offset'] + offset
        return self.bytes[begin:begin + available] + bytes(size - available)

    def pointer(self, address, size=1):
        if address in self.functions and size == 1:
            return '@' + self.functions[address]['name']
        for spec in self.data:
            if spec['address'] <= address and address + size <= spec['address'] + spec['size']:
                return f"getelementptr (i8, ptr @data_{spec['address']:x}, i64 {address - spec['address']})"
        raise Rejected(f'no native pointer contract for 0x{address:x}')

    def callee(self, address):
        if address in self.functions:
            return self.functions[address]
        segment = self.segment(address, 6)
        require(segment['p_flags'] & 1, 'call target is not executable')
        raw = self.read(address, 6)
        require(raw[:2] == b'\xff\x25', f'unresolved call target 0x{address:x}')
        slot = address + 6 + int.from_bytes(raw[2:], 'little', signed=True)
        require(slot in self.import_slots, 'PLT slot lacks an import contract')
        return self.import_slots[slot]

    def annotated_calls(self, owner):
        address = owner['address']
        if address not in self.call_contracts:
            try:
                self.call_contracts[address] = validate_indirect_calls(self, owner)
            except CallError as error:
                raise Rejected(str(error)) from error
        return self.call_contracts[address]

    def call(self, ins, owner=None):
        operand = ins.operands[0]
        if operand.type == X86_OP_IMM:
            if operand.imm in self.functions:
                return self.functions[operand.imm], None
            contract = self.callee(operand.imm)
            raw = self.read(operand.imm, 6)
            slot = operand.imm + 6 + int.from_bytes(raw[2:], 'little', signed=True)
            return contract, slot
        if operand.type == X86_OP_MEM:
            mem = operand.mem
            if ins.reg_name(mem.base) == 'rip' and not mem.index and not mem.segment and ins.addr_size == 8:
                slot = ins.address + ins.size + mem.disp
                if slot in self.import_slots:
                    return self.import_slots[slot], slot
        if owner is not None:
            site = self.annotated_calls(owner).get(ins.address)
            if site is not None:
                return site['contract'], 'operand'
        raise Rejected('indirect calls require native signature and target-set reconstruction')


class Function:
    def __init__(self, image, contract):
        self.image, self.contract = image, contract
        self.instructions = {}
        self.successors = {}
        self.tail_calls = {}
        self.states = {}
        self.memory_writes = {}
        self.lines = []
        self.serial = 0
        self.depth = 128
        self.argument_locations = abi.locations(contract['parameters'])
        self.stack_end = max([8] + [loc['offset'] + 8 for loc in self.argument_locations if loc['kind'] == 'stack'])
        self.image.annotated_calls(contract)
        self.discover()
        self.analyze()

    def fail(self, ins, message):
        raise Rejected(f"{self.contract['name']} at 0x{ins.address:x} ({ins.mnemonic} {ins.op_str}): {message}")

    def discover(self):
        start, end = self.contract['address'], self.contract['address'] + self.contract['size']
        pending = [start]
        occupied = {}
        while pending:
            address = pending.pop()
            if address in self.instructions:
                continue
            require(start <= address < end, 'control flow escapes function contract')
            decoded = list(self.image.decoder.disasm(self.image.read(address, min(15, end - address)), address, count=1))
            require(len(decoded) == 1, f'cannot decode 0x{address:x}')
            ins = decoded[0]
            for byte in range(address, address + ins.size):
                require(byte not in occupied, 'overlapping instruction streams')
                occupied[byte] = address
            self.instructions[address] = ins
            next_address = address + ins.size
            if ins.mnemonic in ('ret', 'ud2'):
                successors = []
            elif ins.mnemonic == 'jmp':
                operand = ins.operands[0]
                if operand.type == X86_OP_IMM and start <= operand.imm < end:
                    successors = [operand.imm]
                else:
                    if operand.type not in (X86_OP_IMM, X86_OP_MEM) or (operand.type == X86_OP_MEM and operand.size != 8):
                        self.fail(ins, 'indirect branches require a proven direct function or GOT import target')
                    try:
                        self.tail_calls[address] = self.image.call(ins)
                    except Rejected as error:
                        self.fail(ins, f'unresolved native tail-call target: {error}')
                    successors = []
            elif ins.mnemonic.startswith('j'):
                require(ins.operands[0].type == X86_OP_IMM, 'indirect branches require target-set reconstruction')
                successors = [ins.operands[0].imm, next_address]
            elif ins.mnemonic == 'call':
                callee, _ = self.image.call(ins, self.contract)
                successors = [] if callee.get('noreturn') else [next_address]
            else:
                successors = [next_address]
            self.successors[address] = successors
            pending.extend(successors)

    def reg(self, ins, operand):
        name = ins.reg_name(operand.reg)
        if name not in ALIASES:
            self.fail(ins, f'unsupported register {name}')
        return ALIASES[name]

    def check_register(self, ins, state, name, bits, shift=0):
        needed = ((1 << bits) - 1) << shift
        if state['known'].get(name, 0) & needed != needed:
            self.fail(ins, f'ABI or dataflow does not define {name}[{shift}:{shift + bits}]')

    def read_operand(self, ins, state, operand):
        if operand.type == X86_OP_REG:
            self.check_register(ins, state, *self.reg(ins, operand))
        elif operand.type == X86_OP_MEM:
            mem = operand.mem
            if ins.mnemonic != 'lea' and ins.addr_size != 8:
                self.fail(ins, '32-bit memory addressing requires pointer reconstruction')
            if mem.segment and ins.mnemonic != 'lea':
                self.fail(ins, 'segment-relative addressing needs TLS lowering')
            for register in (mem.base, mem.index):
                name = ins.reg_name(register)
                if name and name != 'rip':
                    self.check_register(ins, state, *ALIASES[name])
            base_name = ins.reg_name(mem.base)
            index_name = ins.reg_name(mem.index)
            if ins.mnemonic != 'lea' and state.get('constants', {}).get(base_name, 0) != 0 and not (index_name in state['pointers'] and mem.scale == 1):
                self.fail(ins, 'literal-derived memory address requires an instruction-bound pointer contract')
            if ins.mnemonic != 'lea' and base_name not in ('', 'rip') and base_name not in state['native_pointers'] and not (index_name in state['native_pointers'] and mem.scale == 1):
                self.fail(ins, 'memory address has no proven native pointer origin')
            frame = state['pointers'].get(base_name)
            if base_name in ('rsp', 'rbp') or (frame and frame[0] == 'frame'):
                base = state[base_name] if base_name in ('rsp', 'rbp') else frame[2]
                if base is None:
                    self.fail(ins, 'dynamic frame offset')
                offset = base + mem.disp
                if not mem.index and ins.mnemonic != 'lea' and offset + operand.size > 0:
                    valid = any(loc['kind'] == 'stack' and loc['offset'] <= offset and offset + operand.size <= loc['offset'] + loc['bits'] // 8 for loc in self.argument_locations)
                    if not valid:
                        self.fail(ins, 'read or write of return address or undeclared stack arguments')
                self.depth = max(self.depth, -(base + mem.disp) + 128)
        elif operand.type != X86_OP_IMM:
            self.fail(ins, 'unsupported operand')

    def define(self, ins, state, operand):
        if operand.type == X86_OP_REG:
            name, bits, shift = self.reg(ins, operand)
            state['known'][name] = MASK64 if bits >= 32 else state['known'].get(name, 0) | (((1 << bits) - 1) << shift)
            state['pointers'].pop(name, None)
            state['native_pointers'].discard(name)
            state.setdefault('constants', {}).pop(name, None)
        elif operand.type == X86_OP_MEM:
            self.memory_writes[ins.address] = (operand,)
            self.read_operand(ins, state, operand)
            clean = state.get('tail_stack_clean', frozenset())
            base_name = ins.reg_name(operand.mem.base)
            frame = state['pointers'].get(base_name)
            begin = state[base_name] if base_name in ('rsp', 'rbp') else (frame[2] if frame and frame[0] == 'frame' else None)
            self.invalidate_stack_aliases(state)
            if begin is not None and not operand.mem.index:
                begin += operand.mem.disp
                state['tail_stack_clean'] = frozenset(loc['offset'] for loc in self.argument_locations if loc['kind'] == 'stack' and loc['offset'] in clean and not (begin < loc['offset'] + loc['bits'] // 8 and loc['offset'] < begin + operand.size))
            elif base_name == 'rip' and not operand.mem.index:
                state['tail_stack_clean'] = clean
            if base_name in ('rsp', 'rbp') and not operand.mem.index:
                begin = state[base_name] + operand.mem.disp
                for offset, saved in list(state['saved'].items()):
                    low, high = max(begin, offset), min(begin + operand.size, offset + 8)
                    if low < high:
                        defined = ((1 << ((high - low) * 8)) - 1) << ((low - offset) * 8)
                        state['saved'][offset] = (saved[0], saved[1] | defined, None, None)
        else:
            self.fail(ins, 'invalid destination')

    def invalidate_stack_aliases(self, state):
        state['saved'] = {offset: (saved[0], saved[1], None, None) for offset, saved in state['saved'].items()}
        state['tail_stack_clean'] = frozenset()

    def check_tail_call(self, ins, state):
        callee, slot = self.tail_calls[ins.address]
        if slot is not None and slot not in state['image_pointers']:
            self.fail(ins, 'import target has no proven native pointer origin')
        if state['rsp'] != 0 or state['saved']:
            self.fail(ins, 'tail call requires a balanced native frame')
        if abi.result_type(callee['result']) != abi.result_type(self.contract['result']) or abi.return_register(callee['result']) != abi.return_register(self.contract['result']):
            self.fail(ins, 'tail-call return ABI is incompatible with the owner')
        incoming = {loc['offset']: loc for loc in self.argument_locations if loc['kind'] == 'stack'}
        for loc in abi.locations(callee['parameters']):
            if loc['kind'] == 'register':
                self.check_register(ins, state, loc['name'], loc['bits'])
                if loc['type'] == 'ptr' and loc['name'] not in state['native_pointers'] and state['constants'].get(loc['name']) != 0:
                    self.fail(ins, 'pointer argument has no proven native pointer origin')
            else:
                original = incoming.get(loc['offset'])
                if original is None or abi.parameter_type(original['type']) != abi.parameter_type(loc['type']):
                    self.fail(ins, 'tail stack argument lacks a compatible declared incoming slot')
                if loc['offset'] not in state['tail_stack_clean']:
                    self.fail(ins, 'tail stack argument may have been rewritten or escaped')

    def transfer_unchecked(self, ins, original):
        state = {'known': original['known'].copy(), 'rsp': original['rsp'], 'rbp': original['rbp'],
                 'saved': original['saved'].copy(), 'flags': set(original['flags']), 'pointers': original['pointers'].copy(),
                 'constants': original.get('constants', {}).copy(), 'tail_stack_clean': original['tail_stack_clean'],
                 'native_pointers': original['native_pointers'].copy(), 'stack_pointers': original['stack_pointers'].copy(),
                 'image_pointers': original['image_pointers'].copy(), 'callback_memory_unmodified': original['callback_memory_unmodified']}
        op, args = ins.mnemonic, ins.operands
        if avx.analyze(self, ins, state) or vector.analyze(self, ins, state):
            return state
        if integer.analyze(self, ins, state):
            for register in ins.regs_access()[1]:
                name = ins.reg_name(register)
                if name in ALIASES:
                    state['constants'].pop(ALIASES[name][0], None)
            if ins.mnemonic in ('inc', 'dec') and args[0].type == X86_OP_REG and args[0].size == 8:
                name = self.reg(ins, args[0])[0]
                pointer = original['pointers'].get(name)
                if pointer:
                    kind, index, offset = pointer
                    state['pointers'][name] = (kind, index, offset + (1 if ins.mnemonic == 'inc' else -1))
            return state
        if op == 'nop':
            return state
        if op in ('push', 'pop'):
            if args[0].size != 8 or (op == 'pop' and args[0].type != X86_OP_REG):
                self.fail(ins, 'unsupported stack transfer')
            name = self.reg(ins, args[0])[0] if args[0].type == X86_OP_REG else None
            if op == 'push':
                if args[0].type == X86_OP_MEM:
                    self.read_operand(ins, state, args[0])
                state['rsp'] -= 8
                state['saved'][state['rsp']] = (name, state['known'].get(name, 0) if name else MASK64, state['rbp'] if name == 'rbp' else None, state['pointers'].get(name))
                self.depth = max(self.depth, -state['rsp'] + 128)
            else:
                saved = state['saved'].pop(state['rsp'], None)
                if saved is None:
                    self.fail(ins, 'unknown stack transfer')
                state['known'][name] = saved[1]
                state['constants'].pop(name, None)
                if name == 'rbp':
                    state['rbp'] = saved[2]
                if saved[3] is None:
                    state['pointers'].pop(name, None)
                else:
                    state['pointers'][name] = saved[3]
                state['rsp'] += 8
            return state
        if ins.address in self.tail_calls:
            self.check_tail_call(ins, state)
            return state
        if op == 'call':
            callee, slot = self.image.call(ins, self.contract)
            if slot == 'operand':
                self.read_operand(ins, state, args[0])
                if (args[0].type == X86_OP_REG or self.memory_origin(ins, state, args[0]) is not None) and not self.pointer_operand(ins, state, args[0]):
                    self.fail(ins, 'indirect call target has no proven native pointer origin')
                if args[0].type == X86_OP_MEM and self.memory_origin(ins, state, args[0]) is None:
                    base, index = ins.reg_name(args[0].mem.base), ins.reg_name(args[0].mem.index)
                    provenance = state['pointers'].get(base)
                    if not provenance or provenance[0] != 'parameter' or index in state['native_pointers']:
                        self.fail(ins, 'indirect call memory lacks an external native pointer-cell contract')
                    if not state['callback_memory_unmodified']:
                        self.fail(ins, 'indirect call target memory may have been rewritten')
            elif slot is not None and slot not in state['image_pointers']:
                self.fail(ins, 'import target has no proven native pointer origin')
            for loc in abi.locations(callee['parameters']):
                if loc['kind'] == 'register':
                    self.check_register(ins, state, loc['name'], loc['bits'])
                    if loc['type'] == 'ptr' and loc['name'] not in state['native_pointers'] and state['constants'].get(loc['name']) != 0:
                        self.fail(ins, 'pointer argument has no proven native pointer origin')
                else:
                    offset = state['rsp'] + loc['offset'] - 8
                    if offset + 8 > 0:
                        self.fail(ins, 'outgoing stack argument is not allocated')
                    if loc['type'] == 'ptr' and offset not in state['stack_pointers']:
                        self.fail(ins, 'stack pointer argument has no proven native pointer origin')
            for reg in VOLATILE | set(VECTOR_STORAGE):
                state['known'].pop(reg, None)
                state['pointers'].pop(reg, None)
                state['constants'].pop(reg, None)
            self.invalidate_stack_aliases(state)
            state['flags'] = set()
            if callee['result'] != 'void':
                state['known'][abi.return_register(callee['result'])] = (1 << abi.bits(callee['result'])) - 1
            return state
        if op == 'ret':
            if args:
                self.fail(ins, 'callee stack cleanup requires a separate ABI contract')
            if state['rsp'] != 0 or state['saved']:
                self.fail(ins, 'unbalanced native frame')
            if self.contract['result'] != 'void':
                self.check_register(ins, state, abi.return_register(self.contract['result']), abi.bits(self.contract['result']))
                if self.contract['result'] == 'ptr' and 'rax' not in state['native_pointers'] and state['constants'].get('rax') != 0:
                    self.fail(ins, 'pointer return has no proven native pointer origin')
            return state
        if op == 'ud2':
            return state
        if op.startswith('j'):
            if op != 'jmp' and op[1:] not in CONDITIONS:
                self.fail(ins, 'unsupported branch condition')
            if op != 'jmp' and not CONDITIONS[op[1:]] <= state['flags']:
                self.fail(ins, 'undefined branch flags')
            return state
        if op in ('mov', 'movabs', 'movzx', 'movsx', 'movsxd', 'lea'):
            if op == 'lea' and args[1].type != X86_OP_MEM:
                self.fail(ins, 'invalid address expression')
            self.read_operand(ins, state, args[1])
            self.define(ins, state, args[0])
            if args[0].type == X86_OP_REG:
                dest = self.reg(ins, args[0])[0]
                if op in ('mov', 'movabs'):
                    constant = self.constant_operand(ins, original, args[1])
                    if constant is not None:
                        self.constant_write(ins, original, state, args[0], constant)
                if args[0].size == 8 and op == 'mov' and args[1].type == X86_OP_REG:
                    source_name = self.reg(ins, args[1])[0]
                    source = original['pointers'].get(source_name)
                    if source_name in ('rsp', 'rbp'):
                        source = ('frame', 0, original[source_name])
                    if source is not None:
                        state['pointers'][dest] = source
                if op == 'lea' and args[0].size == 8 and not args[1].mem.index:
                    base = ins.reg_name(args[1].mem.base)
                    if base == 'rip':
                        state['pointers'][dest] = ('image', 0, ins.address + ins.size + args[1].mem.disp)
                    elif base in ('rsp', 'rbp'):
                        state['pointers'][dest] = ('frame', 0, original[base] + args[1].mem.disp)
                    elif base in original['pointers']:
                        kind, index, offset = original['pointers'][base]
                        state['pointers'][dest] = (kind, index, offset + args[1].mem.disp)
                if dest in ('rsp', 'rbp'):
                    if op == 'lea' and args[0].size == 8 and not args[1].mem.index and ins.reg_name(args[1].mem.base) in ('rsp', 'rbp'):
                        state[dest] = original[ins.reg_name(args[1].mem.base)] + args[1].mem.disp
                    elif op == 'mov' and args[0].size == 8 and args[1].type == X86_OP_REG and self.reg(ins, args[1])[0] in ('rsp', 'rbp'):
                        state[dest] = original[self.reg(ins, args[1])[0]]
                    else:
                        self.fail(ins, 'unsupported frame manipulation')
            return state
        if op in ('add', 'sub', 'and', 'or', 'xor', 'cmp', 'test'):
            zero = op == 'xor' and args[0].type == args[1].type == X86_OP_REG and args[0].reg == args[1].reg
            if not zero:
                self.read_operand(ins, state, args[0])
                self.read_operand(ins, state, args[1])
            if op not in ('cmp', 'test'):
                self.define(ins, state, args[0])
                if op in ('add', 'sub') and args[0].type == X86_OP_REG and args[0].size == 8 and args[1].type == X86_OP_IMM:
                    dest = self.reg(ins, args[0])[0]
                    pointer = original['pointers'].get(dest)
                    if pointer:
                        kind, index, offset = pointer
                        state['pointers'][dest] = (kind, index, offset + args[1].imm * (1 if op == 'add' else -1))
                left, right = self.constant_operand(ins, original, args[0]), self.constant_operand(ins, original, args[1])
                if zero:
                    self.constant_write(ins, original, state, args[0], 0)
                elif left is not None and right is not None:
                    value = {'add': lambda: left + right, 'sub': lambda: left - right, 'and': lambda: left & right,
                             'or': lambda: left | right, 'xor': lambda: left ^ right}[op]()
                    self.constant_write(ins, original, state, args[0], value)
                if args[0].type == X86_OP_REG and self.reg(ins, args[0])[0] in ('rsp', 'rbp'):
                    dest = self.reg(ins, args[0])[0]
                    if op not in ('add', 'sub') or args[1].type != X86_OP_IMM or state[dest] is None:
                        self.fail(ins, 'dynamic frame allocation')
                    state[dest] += args[1].imm * (1 if op == 'add' else -1)
                    if dest == 'rsp' and state['rsp'] > original['rsp']:
                        state['saved'] = {offset: saved for offset, saved in state['saved'].items() if offset >= state['rsp']}
                    self.depth = max(self.depth, -state[dest] + 128)
            state['flags'] = set(FLAGS)
            return state
        self.fail(ins, 'instruction semantics are not implemented')

    def memory_origin(self, ins, state, operand):
        if operand.type != X86_OP_MEM or operand.mem.index:
            return None
        base, displacement = ins.reg_name(operand.mem.base), operand.mem.disp
        if base == 'rip':
            return 'image', ins.address + ins.size + displacement
        if not base:
            return 'image', displacement
        if base in ('rsp', 'rbp') and state[base] is not None:
            return 'frame', state[base] + displacement
        pointer = state['pointers'].get(base)
        if pointer and pointer[0] in ('image', 'frame'):
            return pointer[0], pointer[2] + displacement
        return None

    def pointer_operand(self, ins, state, operand):
        if operand.size != 8:
            return False
        if operand.type == X86_OP_REG:
            return self.reg(ins, operand)[0] in state['native_pointers']
        origin = self.memory_origin(ins, state, operand)
        if origin is None:
            return False
        kind, offset = origin
        if kind == 'frame':
            return offset in state['stack_pointers']
        return offset in state['image_pointers']

    def transfer(self, ins, original):
        state = self.transfer_unchecked(ins, original)
        native = original['native_pointers'].copy()
        for register in ins.regs_access()[1]:
            name = ins.reg_name(register)
            if name in ALIASES:
                native.discard(ALIASES[name][0])
        stack = original['stack_pointers'].copy()
        image = original['image_pointers'].copy()
        callback_memory_unmodified = original['callback_memory_unmodified']
        op, args = ins.mnemonic, ins.operands
        for operand in self.memory_writes.get(ins.address, ()):
            origin = self.memory_origin(ins, original, operand)
            if origin is None or origin[0] != 'frame':
                callback_memory_unmodified = False
            if origin is None:
                stack.clear()
                image.clear()
            else:
                cells, begin = (stack if origin[0] == 'frame' else image), origin[1]
                cells.difference_update(offset for offset in list(cells) if begin < offset + 8 and offset < begin + operand.size)
                if op == 'mov' and operand is args[0] and self.pointer_operand(ins, original, args[1]):
                    cells.add(begin)
        if op == 'push':
            offset = original['rsp'] - 8
            stack = {saved for saved in stack if not (offset < saved + 8 and saved < offset + 8)}
            if self.pointer_operand(ins, original, args[0]):
                stack.add(offset)
            native.add('rsp')
        elif op == 'pop':
            if original['rsp'] in original['stack_pointers']:
                native.add(self.reg(ins, args[0])[0])
            stack.discard(original['rsp'])
            native.add('rsp')
        elif op == 'call':
            native.difference_update(VOLATILE | set(VECTOR_STORAGE))
            stack.clear()
            image.clear()
            callback_memory_unmodified = False
            callee, _ = self.image.call(ins, self.contract)
            if callee['result'] == 'ptr':
                native.add('rax')
            native.add('rsp')
        elif args and args[0].type == X86_OP_REG and args[0].size == 8:
            destination = self.reg(ins, args[0])[0]
            proven = False
            if op in ('mov', 'movabs'):
                proven = self.pointer_operand(ins, original, args[1])
            elif op == 'lea':
                memory = args[1].mem
                base, index = ins.reg_name(memory.base), ins.reg_name(memory.index)
                proven = ((base == 'rip' or base in original['native_pointers']) and index not in original['native_pointers']) or (base not in original['native_pointers'] and index in original['native_pointers'] and memory.scale == 1)
            elif op in ('add', 'sub'):
                left, right = (self.pointer_operand(ins, original, operand) for operand in args)
                proven = (left and not right) or (op == 'add' and right and not left)
            elif op in ('inc', 'dec'):
                proven = destination in original['native_pointers']
            elif op.startswith('cmov'):
                proven = all(self.pointer_operand(ins, original, operand) for operand in args)
            if proven:
                native.add(destination)
        state['native_pointers'], state['stack_pointers'], state['image_pointers'] = native, stack, image
        state['callback_memory_unmodified'] = callback_memory_unmodified
        return state

    def constant_operand(self, ins, state, operand):
        if operand.type == X86_OP_IMM:
            return operand.imm & ((1 << (operand.size * 8)) - 1)
        if operand.type == X86_OP_REG:
            name, width, shift = self.reg(ins, operand)
            value = state.get('constants', {}).get(name)
            if value is not None:
                return (value >> shift) & ((1 << width) - 1)
        return None

    def constant_write(self, ins, original, state, operand, value):
        if operand.type != X86_OP_REG:
            return
        name, width, shift = self.reg(ins, operand)
        value &= (1 << width) - 1
        if width >= 32:
            state['constants'][name] = value
        elif name in original.get('constants', {}):
            mask = ((1 << width) - 1) << shift
            state['constants'][name] = (original['constants'][name] & ~mask) | (value << shift)

    def analyze(self):
        known = {'rsp': MASK64}
        for loc in self.argument_locations:
            if loc['kind'] == 'register':
                known[loc['name']] = (1 << loc['bits']) - 1
        pointers = {loc['name']: ('parameter', index, 0) for index, loc in enumerate(self.argument_locations) if loc['kind'] == 'register' and loc['type'] == 'ptr'}
        initial = {'known': known, 'rsp': 0, 'rbp': None, 'saved': {}, 'flags': set(), 'pointers': pointers, 'constants': {},
                   'tail_stack_clean': frozenset(loc['offset'] for loc in self.argument_locations if loc['kind'] == 'stack'),
                   'native_pointers': {'rsp'} | set(pointers),
                   'stack_pointers': {loc['offset'] for loc in self.argument_locations if loc['kind'] == 'stack' and loc['type'] == 'ptr'},
                   'image_pointers': set(self.image.import_slots) | {slot for slot, _ in self.image.relocations},
                   'callback_memory_unmodified': True}
        entry = self.contract['address']
        self.states[entry] = initial
        pending = deque([entry])
        while pending:
            address = pending.popleft()
            state = self.transfer(self.instructions[address], self.states[address])
            for target in self.successors[address]:
                old = self.states.get(target)
                if old is None:
                    new = state
                else:
                    require(old['rsp'] == state['rsp'] and old['rbp'] == state['rbp'] and old['saved'].keys() == state['saved'].keys(),
                            f'incompatible native frames at 0x{target:x}')
                    new = dict(state)
                    new['saved'] = {}
                    for offset, saved in state['saved'].items():
                        before = old['saved'][offset]
                        require(saved[0] == before[0], 'incompatible saved registers')
                        new['saved'][offset] = (saved[0], saved[1] & before[1], saved[2] if saved[2] == before[2] else None, saved[3] if saved[3] == before[3] else None)
                    new['known'] = {name: old['known'].get(name, 0) & state['known'].get(name, 0) for name in REGISTERS + VECTOR_STORAGE}
                    new['flags'] = old['flags'] & state['flags']
                    new['tail_stack_clean'] = old['tail_stack_clean'] & state['tail_stack_clean']
                    new['native_pointers'] = old['native_pointers'] & state['native_pointers']
                    new['stack_pointers'] = old['stack_pointers'] & state['stack_pointers']
                    new['image_pointers'] = old['image_pointers'] & state['image_pointers']
                    new['callback_memory_unmodified'] = old['callback_memory_unmodified'] and state['callback_memory_unmodified']
                    new['pointers'] = {name: pointer for name, pointer in state['pointers'].items() if old['pointers'].get(name) == pointer}
                    new['constants'] = {name: value for name, value in state['constants'].items() if old['constants'].get(name) == value}
                if old != new:
                    self.states[target] = new
                    pending.append(target)
        require(self.depth <= 1024 * 1024, 'native frame exceeds current allocation limit')
        alignments = self.contract.get('stack_objects', [])
        for padding in range(16):
            if all((self.depth + padding + obj['offset']) % obj['alignment'] == 0 for obj in alignments):
                self.depth += padding
                break
        else:
            raise Rejected('stack object alignments require incompatible entry stack addresses')

    def emit(self, expression):
        self.serial += 1
        value = f'%v{self.serial}'
        self.lines.append(f'  {value} = {expression}')
        return value

    def load(self, name, bits=64, shift=0):
        width = 128 if name in VECTOR_STORAGE else 64
        value = self.emit(f'load i{width}, ptr %{name}, align 8')
        if shift:
            value = self.emit(f'lshr i{width} {value}, {shift}')
        return value if bits == width else self.emit(f'trunc i{width} {value} to i{bits}')

    def store_vector(self, name, value, bits=128, zero_upper=False):
        if bits != 128:
            value = self.emit(f'zext i{bits} {value} to i128')
            if not zero_upper:
                old = self.load(name, 128)
                high = self.emit(f'and i128 {old}, {((1 << 128) - 1) ^ ((1 << bits) - 1)}')
                value = self.emit(f'or i128 {high}, {value}')
        self.lines.append(f'  store i128 {value}, ptr %{name}, align 16')

    def store(self, name, value, bits=64, shift=0):
        if bits != 64:
            value = self.emit(f'zext i{bits} {value} to i64')
        if bits < 32:
            old = self.load(name)
            masked = self.emit(f'and i64 {old}, {MASK64 ^ (((1 << bits) - 1) << shift)}')
            if shift:
                value = self.emit(f'shl i64 {value}, {shift}')
            value = self.emit(f'or i64 {value}, {masked}')
        self.lines.append(f'  store i64 {value}, ptr %{name}, align 8')

    def frame_pointer(self, offset):
        return self.emit(f'getelementptr i8, ptr %frame, i64 {self.depth + offset}')

    def effective_integer(self, ins, operand):
        mem = operand.mem
        base, index = ins.reg_name(mem.base), ins.reg_name(mem.index)
        if base == 'rip' or base in ('rsp', 'rbp') or base in self.states[ins.address]['pointers']:
            value = self.emit(f'ptrtoint ptr {self.address(ins, operand)} to i64')
        else:
            value = str(mem.disp & MASK64)
            for name, scale in ((base, 1), (index, mem.scale)):
                if name:
                    parent, width, shift = ALIASES[name]
                    source = self.load(parent, width, shift)
                    if width != 64:
                        source = self.emit(f'zext i{width} {source} to i64')
                    if scale != 1:
                        source = self.emit(f'mul i64 {source}, {scale}')
                    value = self.emit(f'add i64 {value}, {source}')
        if ins.addr_size == 4:
            value = self.emit(f'trunc i64 {value} to i32')
            value = self.emit(f'zext i32 {value} to i64')
        return value

    def address(self, ins, operand):
        mem = operand.mem
        state = self.states[ins.address]
        base = ins.reg_name(mem.base)
        index = ins.reg_name(mem.index)
        if base == 'rip':
            require(not index, 'invalid RIP-relative index')
            return self.image.pointer(ins.address + ins.size + mem.disp, 1 if ins.mnemonic == 'lea' else operand.size)
        if base not in state['pointers'] and index in state['pointers'] and mem.scale == 1:
            base, index = index, base
        if base in ('rsp', 'rbp'):
            pointer = self.frame_pointer(state[base] + mem.disp)
            offset = '0'
        elif base in state['pointers']:
            kind, argument, displacement = state['pointers'][base]
            pointer = self.image.pointer(displacement) if kind == 'image' else (self.frame_pointer(displacement) if kind == 'frame' else f'%arg{argument}')
            offset = str(mem.disp + (displacement if kind == 'parameter' else 0))
        elif base:
            pointer = self.emit(f'inttoptr i64 {self.load(base)} to ptr')
            offset = str(mem.disp)
        else:
            pointer = self.image.pointer(mem.disp)
            offset = '0'
        if index:
            value = self.load(index)
            if mem.scale != 1:
                value = self.emit(f'mul i64 {value}, {mem.scale}')
            offset = self.emit(f'add i64 {value}, {offset}')
        return self.emit(f'getelementptr i8, ptr {pointer}, i64 {offset}') if offset != '0' else pointer

    def read(self, ins, operand, bits=None):
        bits = operand.size * 8 if bits is None else bits
        if operand.type == X86_OP_REG:
            name, width, shift = self.reg(ins, operand)
            return self.load(name, min(width, bits), shift)
        if operand.type == X86_OP_IMM:
            return str(operand.imm & ((1 << bits) - 1))
        return self.emit(f'load i{bits}, ptr {self.address(ins, operand)}, align 1')

    def write(self, ins, operand, value):
        if operand.type == X86_OP_REG:
            name, bits, shift = self.reg(ins, operand)
            self.store(name, value, bits, shift)
        else:
            self.lines.append(f'  store i{operand.size * 8} {value}, ptr {self.address(ins, operand)}, align 1')

    def flags(self, operation, bits, left, right, value):
        zf = self.emit(f'icmp eq i{bits} {value}, 0')
        sf = self.emit(f'icmp slt i{bits} {value}, 0')
        low = value if bits == 8 else self.emit(f'trunc i{bits} {value} to i8')
        parity = self.emit(f'call i8 @llvm.ctpop.i8(i8 {low})')
        parity = self.emit(f'and i8 {parity}, 1')
        pf = self.emit(f'icmp eq i8 {parity}, 0')
        if operation in ('sub', 'cmp'):
            cf = self.emit(f'icmp ult i{bits} {left}, {right}')
            different = self.emit(f'xor i{bits} {left}, {right}')
            changed = self.emit(f'xor i{bits} {left}, {value}')
            overflow = self.emit(f'and i{bits} {different}, {changed}')
            of = self.emit(f'icmp slt i{bits} {overflow}, 0')
        elif operation == 'add':
            cf = self.emit(f'icmp ult i{bits} {value}, {left}')
            different = self.emit(f'xor i{bits} {left}, {right}')
            same = self.emit(f'xor i{bits} {different}, -1')
            changed = self.emit(f'xor i{bits} {left}, {value}')
            overflow = self.emit(f'and i{bits} {same}, {changed}')
            of = self.emit(f'icmp slt i{bits} {overflow}, 0')
        else:
            cf = of = 'false'
        for name, value in [('zf', zf), ('sf', sf), ('cf', cf), ('of', of), ('pf', pf)]:
            self.lines.append(f'  store i1 {value}, ptr %{name}, align 1')

    def condition(self, operation):
        flags = {name: self.emit(f'load i1, ptr %{name}, align 1') for name in ('zf', 'sf', 'cf', 'of', 'pf')}
        inverse = lambda value: self.emit(f'xor i1 {value}, true')
        signed = lambda: self.emit(f"xor i1 {flags['sf']}, {flags['of']}")
        if operation == 'je': return flags['zf']
        if operation == 'jne': return inverse(flags['zf'])
        if operation == 'jb': return flags['cf']
        if operation == 'jae': return inverse(flags['cf'])
        if operation == 'js': return flags['sf']
        if operation == 'jns': return inverse(flags['sf'])
        if operation == 'jp': return flags['pf']
        if operation == 'jnp': return inverse(flags['pf'])
        if operation == 'jo': return flags['of']
        if operation == 'jno': return inverse(flags['of'])
        if operation in ('ja', 'jbe'):
            value = self.emit(f"or i1 {flags['cf']}, {flags['zf']}")
            return value if operation == 'jbe' else inverse(value)
        if operation == 'jl': return signed()
        if operation == 'jge': return inverse(signed())
        value = self.emit(f"or i1 {signed()}, {flags['zf']}")
        return value if operation == 'jle' else inverse(value)

    def native_value(self, typ, value):
        if typ == 'ptr':
            return self.emit(f'inttoptr i64 {value} to ptr')
        if typ in abi.FLOAT_TYPES:
            return self.emit(f'bitcast i{abi.bits(typ)} {value} to {abi.llvm_type(typ)}')
        return value

    def machine_value(self, typ, value):
        if typ == 'ptr':
            return self.emit(f'ptrtoint ptr {value} to i64')
        if typ in abi.FLOAT_TYPES:
            return self.emit(f'bitcast {abi.llvm_type(typ)} {value} to i{abi.bits(typ)}')
        return value

    def lower(self):
        result, parameters = signature(self.contract)
        name = self.contract['name']
        args = ', '.join(f'{abi.parameter_type(typ)} %arg{i}' for i, typ in enumerate(parameters))
        self.lines = [f'define {abi.result_type(result)} @{name}({args}) uwtable {{', 'entry:']
        for reg in REGISTERS:
            self.lines.append(f'  %{reg} = alloca i64, align 8')
            unknown = self.emit('freeze i64 poison')
            self.lines.append(f'  store i64 {unknown}, ptr %{reg}, align 8')
        for reg in VECTOR_STORAGE:
            self.lines.append(f'  %{reg} = alloca i128, align 16')
            unknown = self.emit('freeze i128 poison')
            self.lines.append(f'  store i128 {unknown}, ptr %{reg}, align 16')
        for flag in ('zf', 'sf', 'cf', 'of', 'pf'):
            self.lines.append(f'  %{flag} = alloca i1, align 1')
        self.lines.append(f'  %frame = alloca [{self.depth + self.stack_end} x i8], align 16')
        self.store('rsp', self.emit(f'ptrtoint ptr {self.frame_pointer(0)} to i64'))
        for i, loc in enumerate(self.argument_locations):
            value = self.machine_value(loc['type'], f'%arg{i}')
            if loc['kind'] == 'stack':
                self.lines.append(f"  store i{loc['bits']} {value}, ptr {self.frame_pointer(loc['offset'])}, align 1")
            elif loc['name'] in VECTOR_REGISTERS:
                self.store_vector(loc['name'], value, loc['bits'], zero_upper=True)
            else:
                self.store(loc['name'], value, loc['bits'])
        if self.contract.get('floating_point'):
            control = self.emit('call i64 asm sideeffect "mrs $0, FPCR", "=r"()')
            mask = (3 << 22) | (1 << 24) | 0x9f03
            control = self.emit(f'and i64 {control}, {mask}')
            valid = self.emit(f'icmp eq i64 {control}, 0')
            self.lines.extend([f'  br i1 {valid}, label %native_fp_valid, label %native_fp_invalid',
                               'native_fp_invalid:', '  call void @llvm.trap()', '  unreachable', 'native_fp_valid:'])
        self.lines.append(f"  br label %b{self.contract['address']:x}")
        for address in sorted(self.instructions):
            ins = self.instructions[address]
            args, op = ins.operands, ins.mnemonic
            self.lines.append(f'b{address:x}:')
            state = self.states[address]
            terminal = False
            if avx.lower(self, ins) or vector.lower(self, ins):
                pass
            elif integer.lower(self, ins):
                pass
            elif op in ('push', 'pop'):
                reg = self.reg(ins, args[0])[0] if args[0].type == X86_OP_REG else None
                if op == 'push':
                    if reg is None or state['known'].get(reg, 0):
                        value = self.read(ins, args[0], 64)
                        self.lines.append(f"  store i64 {value}, ptr {self.frame_pointer(state['rsp'] - 8)}, align 1")
                    offset = state['rsp'] - 8
                else:
                    if state['saved'][state['rsp']][1]:
                        self.store(reg, self.emit(f"load i64, ptr {self.frame_pointer(state['rsp'])}, align 1"))
                    offset = state['rsp'] + 8
                self.store('rsp', self.emit(f'ptrtoint ptr {self.frame_pointer(offset)} to i64'))
            elif op in ('mov', 'movabs'):
                self.write(ins, args[0], self.read(ins, args[1], args[0].size * 8))
            elif op in ('movzx', 'movsx', 'movsxd'):
                value = self.read(ins, args[1])
                value = self.emit(f"{'zext' if op == 'movzx' else 'sext'} i{args[1].size * 8} {value} to i{args[0].size * 8}")
                self.write(ins, args[0], value)
            elif op == 'lea':
                value = self.effective_integer(ins, args[1])
                if args[0].size != 8:
                    value = self.emit(f'trunc i64 {value} to i{args[0].size * 8}')
                self.write(ins, args[0], value)
            elif op in ('add', 'sub', 'and', 'or', 'xor', 'cmp', 'test'):
                bits = args[0].size * 8
                zero = op == 'xor' and args[0].type == args[1].type == X86_OP_REG and args[0].reg == args[1].reg
                left = '0' if zero else self.read(ins, args[0])
                right = '0' if zero else self.read(ins, args[1], bits)
                instruction = {'cmp': 'sub', 'test': 'and'}.get(op, op)
                value = self.emit(f'{instruction} i{bits} {left}, {right}')
                if op not in ('cmp', 'test'):
                    self.write(ins, args[0], value)
                self.flags(op, bits, left, right, value)
            elif op == 'call' or address in self.tail_calls:
                tail = address in self.tail_calls
                callee, slot = self.tail_calls[address] if tail else self.image.call(ins, self.contract)
                arguments = []
                for loc in abi.locations(callee['parameters']):
                    if loc['kind'] == 'stack':
                        offset = loc['offset'] if tail else state['rsp'] + loc['offset'] - 8
                        value = self.emit(f"load i{loc['bits']}, ptr {self.frame_pointer(offset)}, align 1")
                    else:
                        value = self.load(loc['name'], loc['bits'])
                    value = self.native_value(loc['type'], value)
                    arguments.append(f"{abi.parameter_type(loc['type'])} {value}")
                if slot == 'operand':
                    target = self.emit(f'inttoptr i64 {self.read(ins, args[0], 64)} to ptr')
                else:
                    target = '@' + callee['name'] if slot is None else self.emit(f'load ptr, ptr {self.image.pointer(slot, 8)}, align 1')
                expression = f"call {abi.result_type(callee['result'])} {target}({', '.join(arguments)})"
                if callee['result'] == 'void':
                    self.lines.append('  ' + expression)
                    value = None
                else:
                    value = self.emit(expression)
                if callee.get('noreturn'):
                    self.lines.append('  unreachable')
                    terminal = True
                elif tail:
                    self.lines.append('  ret void' if value is None else f"  ret {abi.llvm_type(result)} {value}")
                    terminal = True
                elif value is not None:
                    value = self.machine_value(callee['result'], value)
                    if callee['result'] in abi.FLOAT_TYPES:
                        self.store_vector('xmm0', value, abi.bits(callee['result']), zero_upper=True)
                    else:
                        self.store('rax', value, abi.bits(callee['result']))
            elif op == 'ret':
                if result == 'void': self.lines.append('  ret void')
                else:
                    value = self.load(abi.return_register(result), abi.bits(result))
                    value = self.native_value(result, value)
                    self.lines.append(f'  ret {abi.llvm_type(result)} {value}')
                terminal = True
            elif op.startswith('j'):
                target = args[0].imm
                if op == 'jmp': self.lines.append(f'  br label %b{target:x}')
                else: self.lines.append(f'  br i1 {self.condition(op)}, label %b{target:x}, label %b{address + ins.size:x}')
                terminal = True
            elif op == 'ud2':
                self.lines.extend(['  call void @llvm.trap()', '  unreachable'])
                terminal = True
            elif op != 'nop':
                self.fail(ins, 'missing lowering')
            if not terminal:
                self.lines.append(f'  br label %b{address + ins.size:x}')
        self.lines.append('}')
        return '\n'.join(self.lines)


def compile_image(image):
    functions = [Function(image, spec) for spec in image.functions.values()]
    lines = ['target triple = "arm64-apple-macosx13.0.0"', 'declare void @llvm.trap()', 'declare i8 @llvm.ctpop.i8(i8)']
    names = {spec['name'] for spec in image.functions.values()}
    for contract in image.imports.values():
        result, parameters = signature(contract)
        require(contract['name'] not in names, 'duplicate native name')
        names.add(contract['name'])
        suffix = ' noreturn' if contract.get('noreturn') else ''
        lines.append(f"declare {abi.result_type(result)} @{contract['name']}({', '.join(abi.parameter_type(t) for t in parameters)}){suffix}")
    for spec in image.data:
        raw = image.read(spec['address'], spec['size'])
        value = ''.join(f'\\{byte:02X}' for byte in raw)
        writable = image.segment(spec['address'])['p_flags'] & 2 or any(spec['address'] <= slot < spec['address'] + spec['size'] for slot in [*image.import_slots, *(slot for slot, _ in image.relocations)])
        lines.append(f"@data_{spec['address']:x} = {'global' if writable else 'constant'} [{len(raw)} x i8] c\"{value}\", align 16")
    if image.relocations or image.import_slots:
        lines.extend(['@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 101, ptr @native_image_init, ptr null }]',
                      'define internal void @native_image_init() {'])
        for slot, target in image.relocations:
            lines.append(f'  store ptr {image.pointer(target)}, ptr {image.pointer(slot, 8)}, align 1')
        for slot, contract in image.import_slots.items():
            lines.append(f"  store ptr @{contract['name']}, ptr {image.pointer(slot, 8)}, align 1")
        lines.extend(['  ret void', '}'])
    lines.extend(function.lower() for function in functions)
    report = {'schema': 1, 'status': 'experimental_native_ir', 'input_sha256': hashlib.sha256(image.bytes).hexdigest(),
              'functions': [{'name': f.contract['name'], 'address': f.contract['address'], 'instructions': len(f.instructions),
                             'native_tail_calls': len(f.tail_calls),
                             'native_frame_bytes_before_optimization': f.depth} for f in functions],
              'imports': len(image.import_slots), 'pointer_relocations': len(image.relocations),
              'indirect_calls': [site for sites in image.call_contracts.values() for site in sites.values()],
              'limitations': ['declared function boundaries and ABIs', 'declared scalar, SSE and AVX instruction subsets', 'no guest exception, TLS or computed-jump target recovery', 'indirect call target sets are declared, not inferred',
                              'no game compatibility or performance qualification']}
    return '\n\n'.join(lines) + '\n', report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('--contracts', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--object', type=Path)
    args = parser.parse_args()
    try:
        image = Image(args.input, json.loads(args.contracts.read_text()))
        ir, report = compile_image(image)
        if args.object:
            from toolchain import build_native_object
            report['native_object'] = build_native_object(ir, args.object)
    except (Rejected, KeyError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(2, f'Native AOT rejected: {error}\n')
    args.output.write_text(ir)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
