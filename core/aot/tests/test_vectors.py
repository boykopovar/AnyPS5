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

import vector
import test_compiler
from compiler import Image, Rejected, compile_image


class Emitter:
    def __init__(self):
        self.serial = 0
        self.contract = {'floating_point': dict(vector.FP_POLICY)}
        self.lines = ['target triple = "arm64-apple-macosx13.0.0"', 'declare void @llvm.trap()',
                      'define void @test_entry(ptr %left, ptr %right, ptr %out, i64 %scalar) {', 'entry:']
        for name in ('xmm0', 'xmm1'):
            self.lines.append(f'  %{name} = alloca i128, align 16')
        self.lines.append('  %rax = alloca i64, align 8')
        for flag in ('cf', 'pf', 'zf', 'sf', 'of'):
            self.lines.extend([f'  %{flag} = alloca i1, align 1', f'  store i1 false, ptr %{flag}, align 1'])
        for name, source in [('xmm0', 'left'), ('xmm1', 'right')]:
            value = self.emit(f'load i128, ptr %{source}, align 1')
            self.lines.append(f'  store i128 {value}, ptr %{name}, align 16')
        self.state = {'known': {'xmm0': vector.MASK128, 'xmm1': vector.MASK128, 'rcx': (1 << 64) - 1}, 'flags': set()}

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
            self.fail(ins, 'undefined register bits')

    def read_operand(self, ins, state, operand):
        if operand.type == X86_OP_MEM:
            self.address(ins, operand)
        elif operand.type == X86_OP_REG:
            name = ins.reg_name(operand.reg)
            self.check_register(ins, state, 'rcx' if name == 'ecx' else name, operand.size * 8)

    def define(self, ins, state, operand):
        state['known']['rax'] = (1 << 64) - 1

    def invalidate_stack_aliases(self, state):
        pass

    def address(self, ins, operand):
        name = ins.reg_name(operand.mem.base)
        if operand.mem.index or operand.mem.disp or name not in ('rdi', 'rsi', 'rdx'):
            raise ValueError('unsupported test address')
        return {'rdi': '%left', 'rsi': '%right', 'rdx': '%out'}[name]

    def read(self, ins, operand, bits=None):
        bits = operand.size * 8 if bits is None else bits
        if operand.type == X86_OP_MEM:
            return self.emit(f'load i{bits}, ptr {self.address(ins, operand)}, align 1')
        name = ins.reg_name(operand.reg)
        if name in ('rcx', 'ecx'):
            return '%scalar' if bits == 64 else self.emit(f'trunc i64 %scalar to i{bits}')
        value = self.emit(f'load i128, ptr %{name}, align 16')
        return value if bits == 128 else self.emit(f'trunc i128 {value} to i{bits}')

    def store_vector(self, name, value, bits=128, zero_upper=False):
        if bits != 128:
            value = self.emit(f'zext i{bits} {value} to i128')
            if not zero_upper:
                old = self.emit(f'load i128, ptr %{name}, align 16')
                upper = self.emit(f'and i128 {old}, {vector.MASK128 ^ ((1 << bits) - 1)}')
                value = self.emit(f'or i128 {upper}, {value}')
        self.lines.append(f'  store i128 {value}, ptr %{name}, align 16')

    def write(self, ins, operand, value):
        if operand.type == X86_OP_MEM:
            self.lines.append(f'  store i{operand.size * 8} {value}, ptr {self.address(ins, operand)}, align 1')
        else:
            if operand.size != 8:
                value = self.emit(f'zext i{operand.size * 8} {value} to i64')
            self.lines.append(f'  store i64 {value}, ptr %rax, align 8')

    def finish(self, mode):
        if mode != 'memory':
            value = self.emit(f'load i{64 if mode == "gpr" else 128}, ptr %{ "rax" if mode == "gpr" else "xmm0"}, align 1')
            self.lines.append(f'  store i{64 if mode == "gpr" else 128} {value}, ptr %out, align 1')
        if mode == 'flags':
            result = '0'
            for name, shift in [('cf', 0), ('pf', 2), ('zf', 6), ('sf', 7), ('of', 11)]:
                flag = self.emit(f'load i1, ptr %{name}, align 1')
                value = self.emit(f'zext i1 {flag} to i64')
                shifted = self.emit(f'shl i64 {value}, {shift}')
                result = self.emit(f'or i64 {result}, {shifted}')
            dest = self.emit('getelementptr i8, ptr %out, i64 16')
            self.lines.append(f'  store i64 {result}, ptr {dest}, align 1')
        self.lines.extend(['  ret void', '}'])
        return '\n'.join(self.lines) + '\n'


class VectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='anyps5-vector-tests-')
        cls.directory = Path(cls.temporary.name)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.decoder = Cs(CS_ARCH_X86, CS_MODE_64)
        cls.decoder.detail = True
        cls.serial = 0

    def instruction(self, assembly):
        type(self).serial += 1
        stem = self.directory / str(self.serial)
        source = stem.with_suffix('.S')
        source.write_text('.intel_syntax noprefix\n.text\n' + assembly + '\n')
        obj = stem.with_suffix('.o')
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-c', str(source), '-o', str(obj)], check=True, capture_output=True)
        code = ELFFile(io.BytesIO(obj.read_bytes())).get_section_by_name('.text').data()
        decoded = list(self.decoder.disasm(code, 0x1000))
        self.assertEqual(len(decoded), 1)
        return decoded[0], stem

    def compare(self, assembly, samples, mode='vector', nan_lanes=0):
        ins, stem = self.instruction(assembly)
        emitter = Emitter()
        self.assertTrue(vector.analyze(emitter, ins, emitter.state))
        self.assertTrue(vector.lower(emitter, ins))
        ir = stem.with_suffix('.ll')
        ir.write_text(emitter.finish(mode))
        native = stem.with_suffix('.dylib')
        fpcr_source = stem.with_suffix('.fpcr.c')
        fpcr_source.write_text('unsigned long long read_fpcr(void) { unsigned long long value; __asm__("mrs %0, fpcr" : "=r"(value)); return value; }')
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-dynamiclib', str(ir), str(fpcr_source), '-o', str(native)], check=True, capture_output=True)
        library = ctypes.CDLL(str(native))
        read_fpcr = library.read_fpcr
        read_fpcr.restype = ctypes.c_uint64
        self.assertEqual(read_fpcr() & ((3 << 22) | (1 << 24) | 0x9f03), 0, 'native floating-point environment does not meet the tested profile')
        fn = library.test_entry
        fn.restype = None
        fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64]
        oracle = stem.with_suffix('.oracle.S')
        store = 'mov QWORD PTR [rdx], rax' if mode == 'gpr' else 'movdqu XMMWORD PTR [rdx], xmm0'
        if mode == 'memory':
            store = ''
        flag_store = 'pushfq\npop rax\nand rax, 2245\nmov QWORD PTR [rdx+16], rax' if mode == 'flags' else ''
        oracle.write_text('.intel_syntax noprefix\n.text\n.globl _run\n_run:\nmovdqu xmm0, XMMWORD PTR [rdi]\nmovdqu xmm1, XMMWORD PTR [rsi]\n' +
                          assembly + '\n' + store + '\n' + flag_store + '\nret\n')
        runner = stem.with_suffix('.c')
        headers = ''.join(f'#include <{name}>\n' for name in ('stdint.h', 'stdio.h', 'string.h', 'xmmintrin.h'))
        runner.write_text(headers + '''extern void run(const void*, const void*, void*, uint64_t);
int main(void) {
    unsigned mxcsr = _mm_getcsr();
    if ((mxcsr & 0xe040) != 0 || (mxcsr & 0x1f80) != 0x1f80) return 3;
    _Alignas(16) unsigned char input[48], output[32];
    while (fread(input, 1, 40, stdin) == 40) {
        uint64_t scalar;
        memcpy(&scalar, input + 32, 8);
        memset(output, 0, sizeof(output));
        run(input, input + 16, output, scalar);
        if (fwrite(output, 1, 24, stdout) != 24) return 2;
    }
    return ferror(stdin) ? 1 : 0;
}
''')
        binary = stem.with_suffix('.x86')
        subprocess.run(['xcrun', 'clang', '-arch', 'x86_64', '-O2', str(oracle), str(runner), '-o', str(binary)], check=True, capture_output=True)
        expected = subprocess.run([str(binary)], input=b''.join(left + right + struct.pack('<Q', count) for left, right, count in samples), check=True, capture_output=True).stdout
        self.assertEqual(len(expected), len(samples) * 24)
        for index, (left, right, count) in enumerate(samples):
            a, b, out = ctypes.create_string_buffer(left), ctypes.create_string_buffer(right), ctypes.create_string_buffer(24)
            fn(a, b, out, count)
            actual, reference = bytearray(out.raw), bytearray(expected[index * 24:(index + 1) * 24])
            for lane in range(nan_lanes):
                for value in (actual, reference):
                    bits = struct.unpack_from('<I', value, lane * 4)[0]
                    if bits & 0x7f800000 == 0x7f800000 and bits & 0x007fffff:
                        struct.pack_into('<I', value, lane * 4, 0x7fc00000)
            self.assertEqual(actual, reference, (assembly, index, left.hex(), right.hex(), count))
        return native

    def random_samples(self, count=50):
        randoms = random.Random(0x2375)
        return [(randoms.randbytes(16), randoms.randbytes(16), randoms.getrandbits(64)) for _ in range(count)]

    def test_move_lane_semantics_against_x86(self):
        for instruction in ['movups xmm0, xmm1', 'movaps xmm0, xmm1', 'movdqu xmm0, [rsi]', 'movdqa xmm0, [rsi]',
                            'movd xmm0, ecx', 'movq xmm0, rcx', 'movq xmm0, xmm1', 'movq xmm0, [rsi]',
                            'movss xmm0, xmm1', 'movsd xmm0, xmm1', 'movss xmm0, [rsi]', 'movsd xmm0, [rsi]',
                            'movlps xmm0, [rsi]']:
            with self.subTest(instruction=instruction):
                self.compare(instruction, self.random_samples())
        for instruction in ['movd eax, xmm0', 'movq rax, xmm0']:
            with self.subTest(instruction=instruction):
                self.compare(instruction, self.random_samples(), 'gpr')
        for instruction in ['movss [rdx], xmm0', 'movsd [rdx], xmm0', 'movlps [rdx], xmm0', 'movups [rdx], xmm0']:
            with self.subTest(instruction=instruction):
                self.compare(instruction, self.random_samples(), 'memory')

    def test_integer_and_shuffle_semantics_against_x86(self):
        operations = ['xorps', 'andps', 'orps', 'pand', 'pandn', 'por', 'unpcklps', 'unpckhps', 'unpcklpd',
                      'unpckhpd', 'punpcklqdq', 'punpckhqdq', 'movlhps', 'movhlps', 'paddd', 'pcmpgtd', 'packuswb']
        for operation in operations:
            with self.subTest(operation=operation):
                self.compare(f'{operation} xmm0, xmm1', self.random_samples())
        for control in [0, 27, 78, 177, 228, 255]:
            for operation in ('shufps', 'pshufd'):
                with self.subTest(operation=operation, control=control):
                    self.compare(f'{operation} xmm0, xmm1, {control}', self.random_samples())

    def test_shift_counts_against_x86(self):
        samples = [(left, struct.pack('<QQ', count, 0xdeadbeef), scalar) for left, _, scalar in self.random_samples(4)
                   for count in (0, 1, 15, 31, 32, 63, 255, 1 << 32, (1 << 64) - 1)]
        self.compare('psrld xmm0, xmm1', samples)
        for count in [0, 1, 31, 32, 255]:
            self.compare(f'psrld xmm0, {count}', self.random_samples())

    def test_float_arithmetic_against_x86(self):
        values = [0, 0x80000000, 1, 0x80000001, 0x007fffff, 0x00800000, 0x3f800000, 0x3f000000,
                  0xbf800000, 0x7f7fffff, 0xff7fffff]
        samples = [(struct.pack('<4I', a, b, a, b), struct.pack('<4I', b, a, b, a), 0) for a in values for b in values]
        for operation in ('addps', 'mulps', 'addss', 'mulss', 'subps', 'subss'):
            with self.subTest(operation=operation):
                self.compare(f'{operation} xmm0, xmm1', samples)
        conversion = [(bytes(16), bytes(16), value & ((1 << 64) - 1)) for value in
                      [0, 1, -1, (1 << 24) + 1, (1 << 31) - 1, -(1 << 31), (1 << 63) - 1, -(1 << 63)]]
        self.compare('cvtsi2ss xmm0, rcx', conversion)
        self.compare('cvtsi2ss xmm0, ecx', conversion)

    def test_float_nan_classification_against_x86(self):
        values = [0, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00001, 0xffc12345, 0x7f800001]
        samples = [(struct.pack('<4I', a, b, a, b), struct.pack('<4I', b, a, b, a), 0) for a in values for b in values]
        for operation in ('addps', 'mulps', 'addss', 'mulss', 'subps', 'subss'):
            with self.subTest(operation=operation):
                self.compare(f'{operation} xmm0, xmm1', samples, nan_lanes=4 if operation.endswith('ps') else 1)

    def test_unordered_comparison_flags_against_x86(self):
        values = [0, 0x80000000, 1, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00001, 0x7f800001]
        samples = [(struct.pack('<4I', a, 1, 2, 3), struct.pack('<4I', b, 4, 5, 6), 0) for a in values for b in values]
        self.compare('ucomiss xmm0, xmm1', samples, 'flags')

    def test_aligned_memory_instruction_rejects_misaligned_host_pointer(self):
        native = self.compare('movaps xmm0, [rsi]', self.random_samples(1))
        script = '''import ctypes, sys
library = ctypes.CDLL(sys.argv[1])
fn = library.test_entry
fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint64]
a, b, out = ctypes.create_string_buffer(32), ctypes.create_string_buffer(32), ctypes.create_string_buffer(32)
fn(a, ctypes.addressof(b) + 1, out, 0)
'''
        result = subprocess.run([sys.executable, '-c', script, str(native)], capture_output=True)
        self.assertLess(result.returncode, 0)

    def test_floating_policy_required(self):
        ins, _ = self.instruction('addps xmm0, xmm1')
        for key in vector.FP_POLICY:
            emitter = Emitter()
            del emitter.contract['floating_point'][key]
            with self.assertRaisesRegex(ValueError, 'floating_point profile'):
                vector.analyze(emitter, ins, emitter.state)

    def test_shuffle_reads_only_selected_lanes(self):
        ins, _ = self.instruction('shufps xmm0, xmm1, 0')
        emitter = Emitter()
        emitter.state['known']['xmm0'] = emitter.state['known']['xmm1'] = (1 << 32) - 1
        self.assertTrue(vector.analyze(emitter, ins, emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], vector.MASK128)
        ins, _ = self.instruction('shufps xmm0, xmm1, 255')
        self.assertTrue(vector.analyze(emitter, ins, emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], (1 << 64) - 1)
        ins, _ = self.instruction('movups [rdx], xmm0')
        with self.assertRaisesRegex(ValueError, 'undefined register bits'):
            vector.analyze(emitter, ins, emitter.state)

    def test_scalar_register_move_preserves_undefined_upper_lanes(self):
        ins, _ = self.instruction('movss xmm0, xmm1')
        emitter = Emitter()
        emitter.state['known']['xmm0'] = 0
        self.assertTrue(vector.analyze(emitter, ins, emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], (1 << 32) - 1)
        ins, _ = self.instruction('movss xmm0, [rsi]')
        self.assertTrue(vector.analyze(emitter, ins, emitter.state))
        self.assertEqual(emitter.state['known']['xmm0'], vector.MASK128)


class VectorCompilerTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp
    compile = test_compiler.CompilerTests.compile

    def test_scalar_argument_register_copy_and_arithmetic(self):
        directory = self.directory
        for code in ('0f28c8f30f59c1c3', '0f28c80f59c9f30f10c1c3'):
            with self.subTest(code=code):
                self.directory = directory / code
                self.directory.mkdir()
                raw, contracts = test_compiler.fixture(bytes.fromhex(code), ['f32'], result='f32')
                contracts['functions'][0]['floating_point'] = dict(vector.FP_POLICY)
                function, _, optimized = self.compile(raw, contracts)
                function.restype, function.argtypes = ctypes.c_float, [ctypes.c_float]
                self.assertNotRegex(optimized, r'%xmm[0-9]+(?:\.\w+)?\s*=\s*alloca')
                for value in (0.0, -0.0, 1.0, -2.0, 0.5, 1e-20, 12345.25):
                    rounded = ctypes.c_float(value).value
                    self.assertEqual(function(rounded), ctypes.c_float(rounded * rounded).value)

    def test_scalar_argument_broadcast_fully_defines_store(self):
        raw, contracts = test_compiler.fixture(bytes.fromhex('0f28c80fc6c9000f110fc3'), ['ptr', 'f32'], result='void')
        function, _, optimized = self.compile(raw, contracts)
        function.restype, function.argtypes = None, [ctypes.c_void_p, ctypes.c_float]
        self.assertNotIn('freeze', optimized)
        output = (ctypes.c_float * 4)()
        function(output, 3.25)
        self.assertEqual(list(output), [3.25] * 4)

    def test_unknown_scalar_upper_lanes_cannot_escape_to_memory(self):
        raw, contracts = test_compiler.fixture(bytes.fromhex('0f1107c3'), ['ptr', 'f32'], result='void')
        source = self.directory / 'unknown-lanes.elf'
        source.write_bytes(raw)
        with self.assertRaisesRegex(Rejected, r'does not define xmm0\[0:128\]'):
            compile_image(Image(source, contracts))


if __name__ == '__main__':
    unittest.main()
