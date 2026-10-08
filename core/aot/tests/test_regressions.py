import ctypes
import hashlib
import io
import math
import random
import struct
import subprocess
import unittest

from elftools.elf.elffile import ELFFile

import test_compiler
import vector
from compiler import Image, Rejected, compile_image
from test_compiler import fixture
from toolchain import build_native_object


class CompilerRegressionTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp
    compile = test_compiler.CompilerTests.compile

    def assemble(self, body):
        source = self.directory / 'probe.S'
        source.write_text('.intel_syntax noprefix\n.text\n' + body + '\n')
        destination = self.directory / 'probe.o'
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-c', str(source), '-o', str(destination)],
                       check=True, capture_output=True)
        return ELFFile(io.BytesIO(destination.read_bytes())).get_section_by_name('.text').data()

    def image(self, raw, contracts):
        source = self.directory / 'input.elf'
        source.write_bytes(raw)
        return Image(source, contracts)

    def dynamic_tags(self, raw, contracts, additions):
        raw = bytearray(raw)
        tags = []
        for offset in range(0x2400, 0x2500, 16):
            tag, value = struct.unpack_from('<qQ', raw, offset)
            if tag == 0:
                break
            if tag not in dict(additions):
                tags.append((tag, value))
        tags.extend(additions)
        tags.append((0, 0))
        self.assertLessEqual(len(tags) * 16, 0x100)
        for index, (tag, value) in enumerate(tags):
            struct.pack_into('<qQ', raw, 0x2400 + index * 16, tag, value)
        struct.pack_into('<QQ', raw, 208, len(tags) * 16, len(tags) * 16)
        struct.pack_into('<Q', raw, 0x5000 + 3 * 64 + 32, len(tags) * 16)
        contracts['sha256'] = hashlib.sha256(raw).hexdigest()
        return bytes(raw), contracts

    def test_stack_object_preserves_stack_pointer_identity(self):
        raw, contracts = fixture(bytes.fromhex('4883ec10488d04244829e04883c410c3'))
        contracts['functions'][0]['stack_objects'] = [{'offset': -16, 'size': 16, 'alignment': 16}]
        function, _, _ = self.compile(raw, contracts)
        self.assertEqual(function(), 0)

    def test_saved_pointer_uses_overwritten_stack_value(self):
        raw, contracts = fixture(bytes.fromhex('57488934245f488b07c3'), ['ptr', 'ptr'])
        function, _, _ = self.compile(raw, contracts)
        left, right = ctypes.c_uint64(17), ctypes.c_uint64(42)
        self.assertEqual(function(ctypes.addressof(left), ctypes.addressof(right)), right.value)

    def test_return_address_and_undeclared_stack_access_rejected(self):
        for code in ('488b0424c3', '488b442408c3', '488d1424488b02c3', '488d542408488b02c3',
                     '4889e0488b00c3', '488d4424f84883c008488b00c3'):
            with self.subTest(code=code):
                with self.assertRaisesRegex(Rejected, 'return address or undeclared stack arguments'):
                    compile_image(self.image(*fixture(bytes.fromhex(code))))

    def test_declared_stack_arguments_are_native_parameters(self):
        raw, contracts = fixture(bytes.fromhex('488b4424084803442410c3'), ['i64'] * 8)
        function, _, _ = self.compile(raw, contracts)
        self.assertEqual(function(1, 2, 3, 4, 5, 6, 55, 89), 144)
        self.assertEqual(function(1, 2, 3, 4, 5, 6, (1 << 64) - 1, 2), 1)

    def test_narrow_lea_wraps_without_pointer_conversion(self):
        directory = self.directory
        randoms = random.Random(2465)
        values = [0, 1, 0x7fffffff, 0xffffffff, 1 << 32, (1 << 64) - 1]
        values.extend(randoms.getrandbits(64) for _ in range(12))
        for index, address in enumerate(('rdi+rsi*4+0x7fffffff', 'edi+esi*4+0x7fffffff')):
            with self.subTest(address=address):
                self.directory = directory / str(index)
                self.directory.mkdir()
                code = self.assemble(f'lea eax, [{address}]\nret')
                function, _, _ = self.compile(*fixture(code, ['i64', 'i64']))
                for left in values:
                    for right in values:
                        self.assertEqual(function(left, right), (left + right * 4 + 0x7fffffff) & 0xffffffff)

    def test_mixed_integer_and_float_abi_through_native_import(self):
        parameters = ['ptr', 'f32', 'i64', 'f64']
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': 6, 'result': 'f64', 'parameters': parameters}]
        imports = {'exampleNid': {'name': 'native_mix', 'result': 'f64', 'parameters': parameters}}
        raw, contracts = fixture(bytes.fromhex('e803000000c39090ff25f20f0000'), functions=functions, imports=imports)
        function, _, _ = self.compile(raw, contracts, extra='double native_mix(const double* p, float a, unsigned long long b, double c) { return *p + a + b + c; }')
        function.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_uint64, ctypes.c_double]
        function.restype = ctypes.c_double
        base = ctypes.c_double(0.5)
        self.assertEqual(function(ctypes.byref(base), 1.25, 17, 2.125), 20.875)
        self.assertEqual(function(ctypes.byref(base), -2.5, 1000001, 0.0625), 999999.0625)

    def test_vector_compare_flags_reach_scalar_result(self):
        code = self.assemble('''xor eax, eax
xor ecx, ecx
xor edx, edx
ucomiss xmm0, xmm1
setb al
setp cl
sete dl
shl ecx, 1
shl edx, 2
or eax, ecx
or eax, edx
ret''')
        raw, contracts = fixture(code, ['f32', 'f32'])
        contracts['functions'][0]['floating_point'] = dict(vector.FP_POLICY)
        function, _, _ = self.compile(raw, contracts)
        function.argtypes = [ctypes.c_float, ctypes.c_float]
        values = [0.0, -0.0, 1.0, -2.0, float('inf'), -float('inf'), float('nan')]
        for left in values:
            for right in values:
                expected = 7 if math.isnan(left) or math.isnan(right) else (4 if left == right else (1 if left < right else 0))
                self.assertEqual(function(left, right), expected)

    def test_unsupported_rel_tables_rejected(self):
        for additions in ([(17, 0x2380)], [(18, 16)], [(36, 0x2380)], [(20, 17)]):
            with self.subTest(additions=additions):
                raw, contracts = self.dynamic_tags(*fixture(bytes.fromhex('31c0c3')), additions)
                with self.assertRaisesRegex(Rejected, 'REL'):
                    self.image(raw, contracts)

    def test_nonzero_init_fini_requires_explicit_entry_owner(self):
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': 3, 'result': 'i64', 'parameters': []},
                     {'name': 'native_init', 'address': 0x1003, 'size': 1, 'result': 'void', 'parameters': []},
                     {'name': 'native_fini', 'address': 0x1004, 'size': 1, 'result': 'void', 'parameters': []}]
        raw, contracts = self.dynamic_tags(*fixture(bytes.fromhex('31c0c3c3c3'), functions=functions), [(12, 0x1003), (13, 0x1004)])
        for lifecycle in ({}, {'owner': 'runtime', 'init': 0x1003, 'fini': 0x1004},
                          {'owner': 'entry', 'init': 0x1003}, {'owner': 'entry', 'init': 0x1004, 'fini': 0x1004}):
            with self.subTest(lifecycle=lifecycle):
                contracts['lifecycle'] = lifecycle
                with self.assertRaisesRegex(Rejected, 'explicit entry lifecycle contract'):
                    self.image(raw, contracts)
        contracts['lifecycle'] = {'owner': 'entry', 'init': 0x1003, 'fini': 0x1004}
        self.image(raw, contracts)
        contracts['functions'][1]['result'] = 'i64'
        with self.assertRaisesRegex(Rejected, r'native void\(\) contract'):
            self.image(raw, contracts)

    def test_literal_image_address_cannot_be_a_host_pointer(self):
        for code in ('b800200000488b00c3', '48b80020000000000000488b00c3'):
            with self.subTest(code=code):
                with self.assertRaises(Rejected):
                    compile_image(self.image(*fixture(bytes.fromhex(code))))

    def test_fp_loop_eliminates_machine_allocations(self):
        code = self.assemble('''xor eax, eax
test edi, edi
je done
loop:
addss xmm0, xmm1
add eax, 1
cmp eax, edi
jb loop
done:
ret''')
        raw, contracts = fixture(code, ['i32', 'f32', 'f32'], result='f32')
        contracts['functions'][0]['floating_point'] = dict(vector.FP_POLICY)
        image = self.image(raw, contracts)
        ir, _ = compile_image(image)
        report = build_native_object(ir, self.directory / 'loop.o')
        self.assertEqual(report['machine_state_allocas'], 0)
        function, _, optimized = self.compile(raw, contracts)
        function.argtypes = [ctypes.c_int32, ctypes.c_float, ctypes.c_float]
        function.restype = ctypes.c_float
        self.assertNotRegex(optimized, r'%(?:xmm[0-9]+|[czsop]f)(?:\.\w+)?\s*=\s*alloca')
        self.assertEqual(function(20, 0.5, 0.25), 5.5)

    def test_object_gate_rejects_unresolved_vector_and_parity_state(self):
        for name, kind in [('xmm15', 'i128'), ('pf', 'i1')]:
            with self.subTest(name=name):
                ir = '\n'.join(['target triple = "arm64-apple-macosx13.0.0"', 'declare void @escape(ptr)',
                                'define void @test_entry() {', 'entry:', f'  %{name} = alloca {kind}, align 16',
                                f'  call void @escape(ptr %{name})', '  ret void', '}'])
                with self.assertRaisesRegex(ValueError, 'unresolved machine-state'):
                    build_native_object(ir, self.directory / f'{name}.o')


if __name__ == '__main__':
    unittest.main()
