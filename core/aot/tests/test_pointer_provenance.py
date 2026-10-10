import ctypes
import hashlib
import io
import struct
import subprocess
import unittest

from elftools.elf.elffile import ELFFile

import test_compiler
from compiler import Image, Rejected, compile_image
from toolchain import build_native_object


class PointerProvenanceTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp
    compile = test_compiler.CompilerTests.compile

    def assemble(self, body):
        source, target = self.directory / 'probe.S', self.directory / 'probe.o'
        source.write_text('.intel_syntax noprefix\n.text\n' + body + '\n')
        subprocess.run(['xcrun', 'clang', '--target=x86_64-unknown-freebsd', '-c', str(source), '-o', str(target)],
                       check=True, capture_output=True)
        return ELFFile(io.BytesIO(target.read_bytes())).get_section_by_name('.text').data()

    def image(self, raw, contracts):
        source = self.directory / 'input.elf'
        source.write_bytes(raw)
        return Image(source, contracts)

    def rejected(self, assembly, parameters=None, result='i64'):
        raw, contracts = test_compiler.fixture(self.assemble(assembly), parameters, result)
        with self.assertRaisesRegex(Rejected, 'pointer origin|literal-derived'):
            compile_image(self.image(raw, contracts))

    def test_literal_transformations_cannot_become_host_addresses(self):
        prefixes = ['mov eax, 0x2000\ninc rax\ndec rax',
                    'mov eax, 0x2000\nlea rax, [rax]',
                    'push 0x2000\npop rax',
                    'mov eax, 0x2000\nnot rax\nnot rax',
                    'mov eax, 0x2000\nmov edx, eax\nmov eax, edx',
                    'mov eax, 0x2000\nmovq xmm0, rax\nmovq rax, xmm0',
                    'sub rsp, 8\nmov QWORD PTR [rsp], 0x2000\nmov rax, [rsp]\nadd rsp, 8']
        for prefix in prefixes:
            with self.subTest(prefix=prefix):
                self.rejected(prefix + '\nmov eax, [rax]\nret')

    def test_scalar_parameters_and_unknown_pointer_cells_are_rejected(self):
        for code, parameters in [('mov rax, [rdi]\nret', ['i64']),
                                 ('mov rax, [rdi]\nmov rax, [rax]\nret', ['ptr']),
                                 ('mov eax, edi\nmov eax, [rax]\nret', ['ptr'])]:
            with self.subTest(code=code):
                self.rejected(code, parameters)

    def test_control_flow_merge_cannot_launder_scalar_origin(self):
        self.rejected('test esi, esi\njz .Lscalar\nmov rax, rdi\njmp .Ljoin\n.Lscalar:\nmov eax, 0x2000\n.Ljoin:\nmov eax, [rax]\nret', ['ptr', 'i32'])

    def test_control_flow_merge_of_two_native_pointers_is_preserved(self):
        code = self.assemble('test edx, edx\njz .Lright\nmov rax, rdi\njmp .Ljoin\n.Lright:\nmov rax, rsi\n.Ljoin:\nmov rax, [rax]\nret')
        function, _, _ = self.compile(*test_compiler.fixture(code, ['ptr', 'ptr', 'i32']))
        left, right = ctypes.c_uint64(17), ctypes.c_uint64(42)
        self.assertEqual(function(ctypes.addressof(left), ctypes.addressof(right), 1), 17)
        self.assertEqual(function(ctypes.addressof(left), ctypes.addressof(right), 0), 42)

    def test_pointer_return_rejects_integer_result_and_accepts_null(self):
        self.rejected('mov eax, 0x2000\ninc rax\nret', result='ptr')
        function, _, _ = self.compile(*test_compiler.fixture(self.assemble('xor eax, eax\nret'), result='ptr'))
        function.restype = ctypes.c_void_p
        self.assertIsNone(function())

    def test_pointer_spill_and_indexed_address_preserve_native_identity(self):
        code = self.assemble('sub rsp, 8\nmov [rsp], rdi\nmov rax, [rsp]\nadd rsp, 8\nmov rax, [rax+rsi*8]\nret')
        function, _, _ = self.compile(*test_compiler.fixture(code, ['ptr', 'i64']))
        data = (ctypes.c_uint64 * 4)(7, 19, 42, 81)
        for index, expected in enumerate(data):
            self.assertEqual(function(ctypes.addressof(data), index), expected)

    def test_partial_stack_write_invalidates_pointer_cell(self):
        self.rejected('push rdi\nmov BYTE PTR [rsp+3], 0\nmov rax, [rsp]\npop rdi\nmov rax, [rax]\nret', ['ptr'])

    def test_read_only_import_relocation_remains_mutable_during_initialization(self):
        imports = {'nativeNid': {'name': 'native_answer', 'result': 'i64', 'parameters': []}}
        raw, contracts = test_compiler.fixture(bytes.fromhex('ff25fa0f0000'), imports=imports)
        raw = bytearray(raw)
        struct.pack_into('<I', raw, 124, 4)
        contracts['sha256'] = hashlib.sha256(raw).hexdigest()
        function, _, optimized = self.compile(bytes(raw), contracts, extra='unsigned long long native_answer(void) { return 42; }')
        self.assertEqual(function(), 42)
        self.assertNotIn('unreachable', optimized)
        ir, _ = compile_image(self.image(bytes(raw), contracts))
        self.assertIn('@data_2000 = global', ir)
        build_native_object(ir, self.directory / 'native.o')

    def test_rewritten_relocation_cell_loses_native_pointer_origin(self):
        code = self.assemble('mov QWORD PTR [rip+0xff5], 0x2000\nlea rax, [rip+0xfee]\nmov rax, [rax]\nmov rax, [rax]\nret')
        raw, contracts = test_compiler.fixture(code, relocations=[[0x2000, 0x2010]])
        with self.assertRaisesRegex(Rejected, 'pointer origin'):
            compile_image(self.image(raw, contracts))

    def test_integer_call_result_cannot_become_a_pointer(self):
        code = bytes.fromhex('e80b000000488b00c3') + b'\x90' * 7 + bytes.fromhex('b800200000c3')
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': 9, 'result': 'i64', 'parameters': []},
                     {'name': 'native_integer', 'address': 0x1010, 'size': 6, 'result': 'i64', 'parameters': []}]
        with self.assertRaisesRegex(Rejected, 'pointer origin'):
            compile_image(self.image(*test_compiler.fixture(code, functions=functions)))

    def test_target_annotation_does_not_turn_guest_integers_into_native_code_pointers(self):
        for body, opcode in [('mov eax, 0x1040\ncall rax\nret', b'\xff\xd0'),
                             ('sub rsp, 8\nmov QWORD PTR [rsp], 0x1040\ncall [rsp]\nadd rsp, 8\nret', b'\xff\x14\x24')]:
            with self.subTest(body=body):
                caller = self.assemble(body)
                code = caller + b'\x90' * (0x40 - len(caller)) + bytes.fromhex('b82a000000c3')
                functions = [{'name': 'test_entry', 'address': 0x1000, 'size': len(caller), 'result': 'i64', 'parameters': [],
                              'indirect_calls': [{'address': 0x1000 + caller.index(opcode), 'result': 'i64', 'parameters': [], 'targets': [0x1040]}]},
                             {'name': 'native_answer', 'address': 0x1040, 'size': 6, 'result': 'i64', 'parameters': []}]
                with self.assertRaisesRegex(Rejected, 'indirect call target has no proven native pointer origin'):
                    compile_image(self.image(*test_compiler.fixture(code, functions=functions)))

    def test_rewritten_callback_memory_cannot_rely_on_target_annotation(self):
        caller = self.assemble('mov QWORD PTR [rdi], 0x1040\ncall [rdi]\nret')
        code = caller + b'\x90' * (0x40 - len(caller)) + bytes.fromhex('b82a000000c3')
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': len(caller), 'result': 'i64', 'parameters': ['ptr'],
                      'indirect_calls': [{'address': 0x1000 + caller.index(b'\xff\x17'), 'result': 'i64', 'parameters': [], 'targets': [0x1040]}]},
                     {'name': 'native_answer', 'address': 0x1040, 'size': 6, 'result': 'i64', 'parameters': []}]
        with self.assertRaisesRegex(Rejected, 'indirect call target memory may have been rewritten'):
            compile_image(self.image(*test_compiler.fixture(code, functions=functions)))

    def test_indexed_frame_callbacks_cannot_use_external_vtable_contract(self):
        variants = ['call [rsp+rcx]', 'lea rdx, [rsp]\ncall [rdx+rcx]',
                    'mov rdx, rsp\nadd rdx, rcx\ncall [rdx]']
        for call in variants:
            with self.subTest(call=call):
                caller = self.assemble('sub rsp, 8\nmov QWORD PTR [rsp], 0x1040\nxor ecx, ecx\n' + call + '\nadd rsp, 8\nret')
                code = caller + b'\x90' * (0x40 - len(caller)) + bytes.fromhex('b82a000000c3')
                functions = [{'name': 'test_entry', 'address': 0x1000, 'size': len(caller), 'result': 'i64', 'parameters': [],
                              'indirect_calls': [{'address': 0x1000 + caller.index(b'\xff'), 'result': 'i64', 'parameters': [], 'targets': [0x1040]}]},
                             {'name': 'native_answer', 'address': 0x1040, 'size': 6, 'result': 'i64', 'parameters': []}]
                with self.assertRaisesRegex(Rejected, 'external native pointer-cell contract'):
                    compile_image(self.image(*test_compiler.fixture(code, functions=functions)))

    def test_pointer_call_and_tail_arguments_require_native_origin(self):
        imports = {'nativeNid': {'name': 'native_pointer_consumer', 'result': 'i64', 'parameters': ['ptr']}}
        prefix = self.assemble('mov edi, 0x2000\ninc rdi\ndec rdi')
        for opcode in (b'\xff\x15', b'\xff\x25'):
            with self.subTest(opcode=opcode.hex()):
                code = prefix + opcode + struct.pack('<i', 0x2000 - 0x1000 - len(prefix) - 6) + b'\xc3'
                with self.assertRaisesRegex(Rejected, 'pointer argument has no proven native pointer origin'):
                    compile_image(self.image(*test_compiler.fixture(code, imports=imports)))

    def test_outgoing_stack_pointer_argument_requires_proven_pointer_cell(self):
        imports = {'nativeNid': {'name': 'native_pointer_consumer', 'result': 'i64', 'parameters': ['ptr'] * 7}}
        prefix = self.assemble('xor edi, edi\nxor esi, esi\nxor edx, edx\nxor ecx, ecx\nxor r8d, r8d\nxor r9d, r9d\nsub rsp, 8\nmov QWORD PTR [rsp], 0x2000')
        code = prefix + b'\xff\x15' + struct.pack('<i', 0x2000 - 0x1000 - len(prefix) - 6) + self.assemble('add rsp, 8\nret')
        with self.assertRaisesRegex(Rejected, 'stack pointer argument has no proven native pointer origin'):
            compile_image(self.image(*test_compiler.fixture(code, imports=imports)))

    def test_declared_pointer_call_result_is_native(self):
        imports = {'nativeNid': {'name': 'native_pointer', 'result': 'ptr', 'parameters': []}}
        code = bytes.fromhex('ff15fa0f0000488b00c3')
        function, _, _ = self.compile(*test_compiler.fixture(code, imports=imports),
                                     extra='void* native_pointer(void) { static unsigned long long value = 42; return &value; }')
        self.assertEqual(function(), 42)


if __name__ == '__main__':
    unittest.main()
