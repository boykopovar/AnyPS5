import ctypes
import unittest

import test_compiler
import test_pointer_provenance
from compiler import Image, Rejected, compile_image


def stores():
    for opcode in ('movups', 'movaps', 'movupd', 'movapd', 'movdqu', 'movdqa'):
        yield opcode, 'pxor xmm0, xmm0', 'xmm0', 16
    for opcode, size in (('movd', 4), ('movq', 8), ('movss', 4), ('movsd', 8), ('movlps', 8), ('movlpd', 8)):
        yield opcode, 'pxor xmm0, xmm0', 'xmm0', size
    for opcode in ('vmovups', 'vmovaps', 'vmovdqu', 'vmovdqa'):
        for register, size in (('xmm0', 16), ('ymm0', 32)):
            yield opcode, f'vpxor {register}, {register}, {register}', register, size
    for opcode, size in (('vmovd', 4), ('vmovq', 8)):
        yield opcode, 'vpxor xmm0, xmm0, xmm0', 'xmm0', size


class MemoryWriteTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp
    compile = test_compiler.CompilerTests.compile
    assemble = test_pointer_provenance.PointerProvenanceTests.assemble

    def fixture(self, body, parameters=None, result='i64', imports=None):
        code = self.assemble('test_entry:\n.set image_cell, test_entry + 0x1000\n' + body)
        return test_compiler.fixture(code, parameters, result, imports=imports)

    def compile_ir(self, raw, contracts):
        source = self.directory / 'image.elf'
        source.write_bytes(raw)
        return compile_image(Image(source, contracts))

    def test_simd_stores_invalidate_overlapping_stack_pointers(self):
        for opcode, initialize, register, size in stores():
            with self.subTest(opcode=opcode, register=register):
                displacement = min(0, 8 - size)
                raw, contracts = self.fixture(f'push rdi\n{initialize}\n{opcode} [rsp{displacement:+d}], {register}\npop rax\nmov rax, [rax]\nret', ['ptr'])
                with self.assertRaisesRegex(Rejected, 'pointer origin'):
                    self.compile_ir(raw, contracts)

    def test_simd_stores_invalidate_import_pointer_cells(self):
        for opcode, initialize, register, _ in stores():
            with self.subTest(opcode=opcode, register=register):
                raw, contracts = self.fixture(f'{initialize}\n{opcode} [rip+image_cell], {register}\ncall [rip+image_cell]\nret', result='void',
                    imports={'nativeNid': {'name': 'native_probe', 'parameters': [], 'result': 'void'}})
                with self.assertRaisesRegex(Rejected, 'import target has no proven native pointer origin'):
                    self.compile_ir(raw, contracts)

    def test_nonpointer_vector_contents_cannot_inherit_spilled_pointer_origin(self):
        raw, contracts = self.fixture('push rdi\nmov eax, 0x2000\nmovq xmm0, rax\npunpcklqdq xmm0, xmm0\nmovups [rsp-8], xmm0\npop rax\nmov rax, [rax]\nret', ['ptr'])
        with self.assertRaisesRegex(Rejected, 'pointer origin'):
            self.compile_ir(raw, contracts)

    def test_vector_read_preserves_spilled_pointer_origin(self):
        function, _, _ = self.compile(*self.fixture('push rdi\nmovups xmm0, [rdi]\npop rax\nmov rax, [rax]\nret', ['ptr']))
        values = (ctypes.c_uint64 * 2)(123, 456)
        self.assertEqual(function(ctypes.addressof(values)), 123)


if __name__ == '__main__':
    unittest.main()
