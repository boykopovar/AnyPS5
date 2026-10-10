import ctypes
import io
import random
import struct
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_REG
from elftools.elf.elffile import ELFFile

import avx
import vector
import test_compiler
from compiler import Image, Rejected, compile_image
from toolchain import build_native_object


MASK256 = (1 << 256) - 1


class Emitter:
    def __init__(self):
        self.serial = 0
        self.lines = ['target triple = "arm64-apple-macosx13.0.0"', 'declare void @llvm.trap()',
                      'define void @test_entry(ptr %a, ptr %b, ptr %c, ptr %out, i64 %scalar) {', 'entry:']
        self.memory = self.emit('getelementptr i8, ptr %out, i64 96')
        self.state = {'known': {'rcx': (1 << 64) - 1}, 'flags': {'cf', 'zf', 'pf', 'sf', 'of'}}
        for index in range(16):
            for upper in (False, True):
                name = ('ymm_hi' if upper else 'xmm') + str(index)
                self.lines.append(f'  %{name} = alloca i128, align 16')
                value = '0'
                if index < 3:
                    pointer = '%' + 'abc'[index]
                    if upper:
                        pointer = self.emit(f'getelementptr i8, ptr {pointer}, i64 16')
                    value = self.emit(f'load i128, ptr {pointer}, align 1')
                self.lines.append(f'  store i128 {value}, ptr %{name}, align 16')
                self.state['known'][name] = avx.MASK128
        self.lines.extend(['  %rax = alloca i64, align 8', '  store i64 0, ptr %rax, align 8'])

    def emit(self, expression):
        self.serial += 1
        value = f'%v{self.serial}'
        self.lines.append(f'  {value} = {expression}')
        return value

    def fail(self, ins, message):
        raise ValueError(message)

    def check_register(self, ins, state, name, bits, shift=0):
        mask = ((1 << bits) - 1) << shift
        if state['known'].get(name, 0) & mask != mask:
            raise ValueError('undefined register bits')

    def read_operand(self, ins, state, operand):
        if operand.type == X86_OP_MEM:
            self.address(ins, operand)
        else:
            name = ins.reg_name(operand.reg)
            if name == 'ecx': name = 'rcx'
            self.check_register(ins, state, name, operand.size * 8)

    def define(self, ins, state, operand):
        if operand.type == X86_OP_MEM:
            self.address(ins, operand)
        else:
            state['known']['rax'] = (1 << 64) - 1

    def invalidate_stack_aliases(self, state):
        pass

    def address(self, ins, operand):
        base = ins.reg_name(operand.mem.base)
        if operand.mem.index or operand.mem.disp or base not in ('rdi', 'rsi', 'rdx'):
            raise ValueError('unsupported test address')
        return {'rdi': '%a', 'rsi': '%b', 'rdx': self.memory}[base]

    def load(self, name, bits=64, shift=0):
        if name in ('rcx', 'ecx'):
            value, width = '%scalar', 64
        else:
            width = 128 if name.startswith(('xmm', 'ymm_hi')) else 64
            value = self.emit(f'load i{width}, ptr %{name}, align 8')
        if shift:
            value = self.emit(f'lshr i{width} {value}, {shift}')
        return value if width == bits else self.emit(f'trunc i{width} {value} to i{bits}')

    def read(self, ins, operand, bits=None):
        bits = operand.size * 8 if bits is None else bits
        if operand.type == X86_OP_MEM:
            return self.emit(f'load i{bits}, ptr {self.address(ins, operand)}, align 1')
        name = ins.reg_name(operand.reg)
        return self.load('rcx' if name == 'ecx' else name, bits)

    def store_vector(self, name, value, bits=128, zero_upper=False):
        if bits != 128:
            value = self.emit(f'zext i{bits} {value} to i128')
            if not zero_upper:
                old = self.load(name, 128)
                high = self.emit(f'and i128 {old}, {avx.MASK128 ^ ((1 << bits) - 1)}')
                value = self.emit(f'or i128 {value}, {high}')
        self.lines.append(f'  store i128 {value}, ptr %{name}, align 16')

    def write(self, ins, operand, value):
        if operand.type == X86_OP_MEM:
            self.lines.append(f'  store i{operand.size * 8} {value}, ptr {self.address(ins, operand)}, align 1')
        else:
            if operand.size != 8:
                value = self.emit(f'zext i{operand.size * 8} {value} to i64')
            self.lines.append(f'  store i64 {value}, ptr %rax, align 8')

    def finish(self):
        for index in range(3):
            for high in (False, True):
                name = ('ymm_hi' if high else 'xmm') + str(index)
                value = self.load(name, 128)
                pointer = self.emit(f'getelementptr i8, ptr %out, i64 {index * 32 + high * 16}')
                self.lines.append(f'  store i128 {value}, ptr {pointer}, align 1')
        pointer = self.emit('getelementptr i8, ptr %out, i64 128')
        self.lines.append(f'  store i64 {self.load("rax", 64)}, ptr {pointer}, align 1')
        self.lines.extend(['  ret void', '}'])
        return '\n'.join(self.lines) + '\n'


def integer_oracle(instructions, inputs, scalar):
    registers = [int.from_bytes(value, 'little') for value in inputs] + [0] * 13
    memory = bytearray(b'\xa5' * 32)
    result = 0
    for ins in instructions:
        op, args = ins.mnemonic, ins.operands
        if op == 'vzeroupper':
            registers = [value & avx.MASK128 for value in registers]
            continue
        bits = args[0].size * 8
        mask = (1 << bits) - 1

        def read(operand, width=bits):
            if operand.type == X86_OP_MEM:
                data = inputs[1 if ins.reg_name(operand.mem.base) == 'rsi' else 0]
                return int.from_bytes(data[:width // 8], 'little')
            name = ins.reg_name(operand.reg)
            if name in ('ecx', 'rcx'):
                return scalar & ((1 << width) - 1)
            return registers[int(name[3:])] & ((1 << width) - 1)

        if op in avx.MOVES:
            value = read(args[1])
        elif op in ('vmovd', 'movd', 'vmovq'):
            bits = 64 if op == 'vmovq' else 32
            value = read(args[1], bits)
        elif op in avx.BITWISE:
            left, right = read(args[1]), read(args[2])
            value = {'and': lambda: left & right, 'or': lambda: left | right, 'xor': lambda: left ^ right}[avx.BITWISE[op]]()
        elif op == 'xorps':
            value = read(args[0]) ^ read(args[1])
        elif op == 'vpbroadcastd':
            value = int.from_bytes(struct.pack('<I', read(args[1], 32)) * (bits // 32), 'little')
        elif op == 'vpshufd':
            source = read(args[1]).to_bytes(bits // 8, 'little')
            groups = []
            for base in range(0, len(source), 16):
                block = source[base:base + 16]
                groups.append(b''.join(block[((args[2].imm >> (2 * i)) & 3) * 4:((args[2].imm >> (2 * i)) & 3) * 4 + 4] for i in range(4)))
            value = int.from_bytes(b''.join(groups), 'little')
        elif op == 'vpsllvd':
            lanes = bits // 32
            left = struct.unpack('<' + 'I' * lanes, read(args[1]).to_bytes(bits // 8, 'little'))
            counts = struct.unpack('<' + 'I' * lanes, read(args[2]).to_bytes(bits // 8, 'little'))
            words = [(word << count) & 0xffffffff if count < 32 else 0 for word, count in zip(left, counts)]
            value = int.from_bytes(struct.pack('<' + 'I' * lanes, *words), 'little')
        else:
            raise AssertionError('unsupported oracle instruction ' + op)
        if args[0].type == X86_OP_MEM:
            memory[:bits // 8] = value.to_bytes(bits // 8, 'little')
        else:
            name = ins.reg_name(args[0].reg)
            if name in ('rax', 'eax'):
                result = value
            else:
                index = int(name[3:])
                if op in ('movd', 'xorps'):
                    registers[index] = (registers[index] & (MASK256 ^ avx.MASK128)) | value
                else:
                    registers[index] = value
    return b''.join(value.to_bytes(32, 'little') for value in registers[:3]) + bytes(memory) + struct.pack('<Q', result)


class AvxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='anyps5-avx-tests-')
        cls.directory = Path(cls.temporary.name)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.decoder = Cs(CS_ARCH_X86, CS_MODE_64)
        cls.decoder.detail = True
        cls.serial = 0

    def instructions(self, assembly):
        type(self).serial += 1
        stem = self.directory / str(self.serial)
        source = stem.with_suffix('.S')
        source.write_text('.intel_syntax noprefix\n.text\n' + assembly + '\n')
        obj = stem.with_suffix('.x86.o')
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-mavx2', '-c', str(source), '-o', str(obj)], check=True, capture_output=True)
        code = ELFFile(io.BytesIO(obj.read_bytes())).get_section_by_name('.text').data()
        instructions = list(self.decoder.disasm(code, 0x1000))
        self.assertEqual(sum(ins.size for ins in instructions), len(code))
        return instructions, stem

    def compare(self, assembly, samples):
        instructions, stem = self.instructions(assembly)
        emitter = Emitter()
        for ins in instructions:
            module = avx if ins.mnemonic.startswith('v') else vector
            self.assertTrue(module.analyze(emitter, ins, emitter.state))
            self.assertTrue(module.lower(emitter, ins))
        ir = stem.with_suffix('.ll')
        ir.write_text(emitter.finish())
        optimized = stem.with_suffix('.opt.ll')
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-S', '-emit-llvm', str(ir), '-o', str(optimized)], check=True, capture_output=True)
        self.assertNotRegex(optimized.read_text(), r'%(?:xmm[0-9]+|ymm_hi[0-9]+)\b[^\n]*= alloca')
        self.assertNotIn('freeze ', optimized.read_text())
        library = stem.with_suffix('.dylib')
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-dynamiclib', str(ir), '-o', str(library)], check=True, capture_output=True)
        function = ctypes.CDLL(str(library)).test_entry
        function.argtypes = [ctypes.c_void_p] * 4 + [ctypes.c_uint64]
        function.restype = None
        for inputs, scalar in samples:
            storage = [ctypes.create_string_buffer(64) for _ in range(3)]
            addresses = [(ctypes.addressof(value) + 31) & ~31 for value in storage]
            for address, data in zip(addresses, inputs):
                ctypes.memmove(address, data, 32)
            output = ctypes.create_string_buffer(167)
            output_address = (ctypes.addressof(output) + 31) & ~31
            ctypes.memset(output_address, 0xa5, 136)
            function(*addresses, output_address, scalar)
            self.assertEqual(ctypes.string_at(output_address, 136), integer_oracle(instructions, inputs, scalar), (assembly, [v.hex() for v in inputs], scalar))
        return library

    def samples(self, count=40):
        randoms = random.Random(681)
        return [([randoms.randbytes(32) for _ in range(3)], randoms.getrandbits(64)) for _ in range(count)]

    def test_moves_and_upper_zeroing(self):
        for width in ('xmm', 'ymm'):
            for mnemonic in ('vmovups', 'vmovdqa', 'vmovdqu', 'vmovaps'):
                for operands in (f'{width}0, {width}1', f'{width}0, [rsi]', f'[rdx], {width}0'):
                    with self.subTest(mnemonic=mnemonic, operands=operands):
                        self.compare(mnemonic + ' ' + operands, self.samples())
        for instruction in ('vmovd xmm0, ecx', 'vmovd xmm0, [rsi]', 'vmovd eax, xmm0', 'vmovd [rdx], xmm0',
                            'vmovq xmm0, rcx', 'vmovq xmm0, xmm1', 'vmovq rax, xmm0'):
            with self.subTest(instruction=instruction):
                self.compare(instruction, self.samples())

    def test_three_operand_bitwise(self):
        for mnemonic in avx.BITWISE:
            for width in ('xmm', 'ymm'):
                for right in (width + '2', '[rsi]'):
                    self.compare(f'{mnemonic} {width}0, {width}1, {right}', self.samples())
        self.compare('vxorps xmm0, xmm0, xmm0\nvmovups [rdx], ymm0', self.samples())
        self.compare('vpxor ymm0, ymm1, ymm1', self.samples())

    def test_broadcast_and_lane_local_shuffle(self):
        for width in ('xmm', 'ymm'):
            self.compare(f'vpbroadcastd {width}0, xmm1', self.samples())
            self.compare(f'vpbroadcastd {width}0, DWORD PTR [rsi]', self.samples())
            for control in (0, 0x1b, 0x55, 0x93, 0xaa, 0xee, 0xff):
                with self.subTest(width=width, control=control):
                    self.compare(f'vpshufd {width}0, {width}1, {control}', self.samples())

    def test_vector_shift_counts_are_not_scalar_masked(self):
        values = [0, 1, 0x80000000, 0xffffffff]
        counts = [0, 1, 15, 31, 32, 63, 255, 0x80000000, 0xffffffff]
        samples = []
        for value in values:
            for count in counts:
                samples.append(([bytes(32), struct.pack('<8I', *([value] * 8)), struct.pack('<8I', *([count] * 8))], 0))
        randoms = random.Random(882)
        for _ in range(100):
            samples.append(([randoms.randbytes(32), randoms.randbytes(32), struct.pack('<8I', *[randoms.choice(counts) for _ in range(8)])], 0))
        for width in ('xmm', 'ymm'):
            self.compare(f'vpsllvd {width}0, {width}1, {width}2', samples)
            self.compare(f'vpsllvd {width}0, {width}1, [rsi]', samples)

    def test_legacy_preserves_upper_and_vzeroupper_clears_it(self):
        for instructions in ('movd xmm0, ecx', 'xorps xmm0, xmm0', 'vzeroupper',
                             'movd xmm0, ecx\nvzeroupper', 'vmovups ymm0, ymm1\nvmovups xmm0, xmm2'):
            self.compare(instructions, self.samples())

    def test_definedness_and_observable_unknown_lanes(self):
        instructions, _ = self.instructions('vxorps xmm0, xmm0, xmm0\nvmovups [rdx], ymm0')
        emitter = Emitter()
        emitter.state['known'].clear()
        for ins in instructions:
            self.assertTrue(avx.analyze(emitter, ins, emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], avx.MASK128)
        self.assertEqual(emitter.state['known']['ymm_hi0'], avx.MASK128)
        instructions, _ = self.instructions('vpshufd ymm0, ymm1, 0\nvmovups [rdx], ymm0')
        emitter = Emitter()
        emitter.state['known']['xmm1'] = 0xffffffff
        emitter.state['known']['ymm_hi1'] = 0
        self.assertTrue(avx.analyze(emitter, instructions[0], emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], avx.MASK128)
        self.assertEqual(emitter.state['known']['ymm_hi0'], 0)
        with self.assertRaisesRegex(ValueError, 'undefined bits'):
            avx.analyze(emitter, instructions[1], emitter.state)

    def test_vzeroupper_retains_low_definedness(self):
        instructions, _ = self.instructions('vzeroupper')
        emitter = Emitter()
        for index in range(16):
            emitter.state['known'][f'xmm{index}'] = index
            emitter.state['known'][f'ymm_hi{index}'] = 0
        before = emitter.state['flags'].copy()
        self.assertTrue(avx.analyze(emitter, instructions[0], emitter.state))
        for index in range(16):
            self.assertEqual(emitter.state['known'][f'xmm{index}'], index)
            self.assertEqual(emitter.state['known'][f'ymm_hi{index}'], avx.MASK128)
        self.assertEqual(emitter.state['flags'], before)

    def test_ymm_aligned_load_requires_32_byte_alignment(self):
        library = self.compare('vmovdqa ymm0, [rsi]', self.samples(1))
        script = """import ctypes, sys
lib = ctypes.CDLL(sys.argv[1])
fn = lib.test_entry
fn.argtypes = [ctypes.c_void_p] * 4 + [ctypes.c_uint64]
values = [ctypes.create_string_buffer(96) for _ in range(3)]
addresses = [(ctypes.addressof(value) + 31) & ~31 for value in values]
addresses[1] += 16
output = ctypes.create_string_buffer(136)
fn(*addresses, output, 0)
"""
        result = subprocess.run([sys.executable, '-c', script, str(library)], capture_output=True)
        self.assertLess(result.returncode, 0)

    def test_evex_and_unsupported_fp_are_not_silently_accepted(self):
        instructions, _ = self.instructions('vxorps zmm0, zmm0, zmm0')
        emitter = Emitter()
        with self.assertRaisesRegex(ValueError, 'EVEX'):
            avx.analyze(emitter, instructions[0], emitter.state)
        instructions, _ = self.instructions('vaddps ymm0, ymm1, ymm2')
        self.assertFalse(avx.analyze(emitter, instructions[0], emitter.state))


class AvxCompilerTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp
    compile = test_compiler.CompilerTests.compile

    def assemble(self, assembly):
        source = self.directory / 'input.S'
        source.write_text('.intel_syntax noprefix\n.text\n' + assembly + '\n')
        obj = self.directory / 'input.o'
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-mavx2', '-c', str(source), '-o', str(obj)], check=True, capture_output=True)
        return ELFFile(io.BytesIO(obj.read_bytes())).get_section_by_name('.text').data()

    def rejected(self, code, parameters, message, functions=None):
        raw, contracts = test_compiler.fixture(code, parameters, result='void', functions=functions)
        source = self.directory / 'rejected.elf'
        source.write_bytes(raw)
        with self.assertRaisesRegex(Rejected, message):
            compile_image(Image(source, contracts))

    def test_native_ymm_copy_with_unaligned_buffers(self):
        code = self.assemble('vmovups ymm0, [rdi]\nvmovups [rsi], ymm0\nvzeroupper\nret')
        raw, contracts = test_compiler.fixture(code, ['ptr', 'ptr'], result='void')
        function, _, optimized = self.compile(raw, contracts)
        function.restype, function.argtypes = None, [ctypes.c_void_p, ctypes.c_void_p]
        randoms = random.Random(735)
        for _ in range(100):
            data = randoms.randbytes(32)
            source = ctypes.create_string_buffer(b'X' + data)
            output = ctypes.create_string_buffer(b'\xa5' * 70, 70)
            function(ctypes.addressof(source) + 1, ctypes.addressof(output) + 17)
            self.assertEqual(output.raw, b'\xa5' * 17 + data + b'\xa5' * 21)
        self.assertNotRegex(optimized, r'%(?:xmm|ymm_hi)[0-9]+(?:\.\w+)?\s*=\s*alloca')
        source_file = self.directory / 'input.elf'
        source_file.write_bytes(raw)
        ir, _ = compile_image(Image(source_file, contracts))
        report = build_native_object(ir, self.directory / 'native-avx.o')
        self.assertEqual(report['machine_state_allocas'], 0)

    def test_native_legacy_and_vex_upper_lane_difference(self):
        parent = self.directory
        for index, opcode in enumerate(('movd xmm15, edx', 'vmovd xmm15, edx')):
            with self.subTest(opcode=opcode):
                self.directory = parent / str(index)
                self.directory.mkdir()
                code = self.assemble('vmovups ymm15, [rdi]\n' + opcode + '\nvmovups [rsi], ymm15\nret')
                function, _, _ = self.compile(*test_compiler.fixture(code, ['ptr', 'ptr', 'i32'], result='void'))
                function.restype, function.argtypes = None, [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32]
                source = ctypes.create_string_buffer(bytes(range(32)), 32)
                output = ctypes.create_string_buffer(32)
                function(source, output, 0xfedcba98)
                self.assertEqual(output.raw, bytes.fromhex('98badcfe') + bytes(12) + (bytes(range(16, 32)) if index == 0 else bytes(16)))

    def test_native_per_lane_shifts(self):
        code = self.assemble('vmovups ymm1, [rdi]\nvpsllvd ymm0, ymm1, [rsi]\nvmovups [rdx], ymm0\nvzeroupper\nret')
        function, _, _ = self.compile(*test_compiler.fixture(code, ['ptr', 'ptr', 'ptr'], result='void'))
        function.restype, function.argtypes = None, [ctypes.c_void_p] * 3
        values = (ctypes.c_uint32 * 8)(1, 2, 3, 4, 0xffffffff, 0x80000000, 9, 7)
        shifts = (ctypes.c_uint32 * 8)(0, 1, 31, 32, 63, 0xffffffff, 0x80000000, 7)
        output = (ctypes.c_uint32 * 8)()
        function(values, shifts, output)
        self.assertEqual(list(output), [value << count & 0xffffffff if count < 32 else 0 for value, count in zip(values, shifts)])

    def test_undefined_high_bank_and_call_clobbers_rejected(self):
        code = self.assemble('xorps xmm0, xmm0\nvmovups [rdi], ymm0\nret')
        self.rejected(code, ['ptr'], 'undefined bits')
        code = self.assemble('push rbx\nmov rbx, rdi\nvxorps ymm0, ymm0, ymm0\ncall .Lcallee\nvmovups [rbx], ymm0\npop rbx\nret\n.Lcallee:\nret')
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': len(code) - 1, 'result': 'void', 'parameters': ['ptr']},
                     {'name': 'callee', 'address': 0x1000 + len(code) - 1, 'size': 1, 'result': 'void', 'parameters': []}]
        self.rejected(code, ['ptr'], 'undefined bits', functions)


if __name__ == '__main__':
    unittest.main()
