import ctypes
import hashlib
import json
import sys
import random
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler import Image, Rejected, compile_image
from optimize import merge_constant_stores
from toolchain import build_native_object


def fixture(code, parameters=None, result='i64', functions=None, imports=None, relocations=None):
    image = bytearray(0x5200)
    imports = imports or {}
    relocations = relocations or []
    ident = b'\x7fELF\x02\x01\x01' + bytes(9)
    struct.pack_into('<16sHHIQQQIHHHHHH', image, 0, ident, 3, 62, 1, 0x1000, 64, 0x5000, 0, 64, 56, 3, 64, 5, 4)
    struct.pack_into('<IIQQQQQQ', image, 64, 1, 5, 0x1000, 0x1000, 0, len(code), len(code), 0x1000)
    struct.pack_into('<IIQQQQQQ', image, 120, 1, 6, 0x2000, 0x2000, 0, 0x3000, 0x3000, 0x1000)
    image[0x1000:0x1000 + len(code)] = code
    strings = bytearray(b'\0')
    for index, name in enumerate(imports, 1):
        struct.pack_into('<IBBHQQ', image, 0x2100 + index * 24, len(strings), 0x12, 0, 0, 0, 0)
        strings.extend(name.encode() + b'\0')
        struct.pack_into('<QQq', image, 0x2300 + (index - 1) * 24, 0x2000 + (index - 1) * 8, (index << 32) | 7, 0)
    image[0x2200:0x2200 + len(strings)] = strings
    for index, (slot, target) in enumerate(relocations):
        struct.pack_into('<QQq', image, 0x2380 + index * 24, slot, 8, target)
    tags = [(6, 0x2100), (11, 24), (5, 0x2200), (10, len(strings)), (23, 0x2300), (2, len(imports) * 24),
            (20, 7), (7, 0x2380), (8, len(relocations) * 24), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, 0x2400 + index * 16, *tag)
    struct.pack_into('<IIQQQQQQ', image, 176, 2, 6, 0x2400, 0x2400, 0, len(tags) * 16, len(tags) * 16, 8)
    names = b'\0.dynsym\0.dynstr\0.dynamic\0.shstrtab\0'
    image[0x2500:0x2500 + len(names)] = names
    for index, entry in enumerate([
        (1, 11, 2, 0x2100, 0x2100, (len(imports) + 1) * 24, 2, 1, 8, 24),
        (9, 3, 2, 0x2200, 0x2200, len(strings), 0, 0, 1, 0),
        (17, 6, 3, 0x2400, 0x2400, len(tags) * 16, 2, 0, 8, 16),
        (26, 3, 0, 0, 0x2500, len(names), 0, 0, 1, 0)], 1):
        struct.pack_into('<IIQQQQIIQQ', image, 0x5000 + index * 64, *entry)
    contracts = {'schema': 1, 'sha256': hashlib.sha256(image).hexdigest(),
                 'functions': functions or [{'name': 'test_entry', 'address': 0x1000, 'size': len(code),
                                               'result': result, 'parameters': parameters or []}],
                 'imports': imports, 'data': [{'address': 0x2000, 'size': 0x3000}], 'pointer_relocations': relocations}
    return bytes(image), contracts


class CompilerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='anyps5-native-aot-')
        self.directory = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def compile(self, raw, contracts, extra=''):
        source = self.directory / 'input.elf'
        source.write_bytes(raw)
        ir, report = compile_image(Image(source, contracts))
        ll = self.directory / 'output.ll'
        ll.write_text(ir)
        optimized = self.directory / 'output.opt.ll'
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-S', '-emit-llvm', str(ll), '-o', str(optimized)],
                       check=True, capture_output=True)
        text = optimized.read_text()
        self.assertNotRegex(text, r'%(?:rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp|r1[0-5]|[czso]f)\s*=\s*alloca')
        self.assertNotIn('freeze', text)
        additional = []
        if extra:
            provider = self.directory / 'provider.c'
            provider.write_text(extra)
            additional.append(str(provider))
        with tempfile.NamedTemporaryFile(prefix='native-', suffix='.dylib', dir=self.directory, delete=False) as output:
            binary = Path(output.name)
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-dynamiclib', str(ll), *additional, '-o', str(binary)],
                       check=True, capture_output=True)
        self.assertIn('arm64', subprocess.check_output(['file', str(binary)], text=True))
        library = ctypes.CDLL(str(binary))
        self.library = library
        function = library.test_entry
        function.restype = ctypes.c_uint64
        function.argtypes = [ctypes.c_uint64] * len(contracts['functions'][0]['parameters'])
        return function, report, text

    def test_recompiled_library_executes_current_code(self):
        first, _, _ = self.compile(*fixture(bytes.fromhex('b811000000c3')))
        self.assertEqual(first(), 17)
        second, _, _ = self.compile(*fixture(bytes.fromhex('b82a000000c3')))
        self.assertEqual(second(), 42)
        self.assertEqual(first(), 17)

    def test_cli_object_links_and_executes_as_arm64(self):
        raw, contracts = fixture(bytes.fromhex('4889f84801f0c3'), ['u64', 'u64'], result='u64')
        elf = self.directory / 'input.elf'
        elf.write_bytes(raw)
        manifest = self.directory / 'contracts.json'
        manifest.write_text(json.dumps(contracts))
        ir, obj = self.directory / 'result.ll', self.directory / 'result.o'
        subprocess.run([sys.executable, str(Path(__file__).parents[1] / 'compiler.py'), str(elf),
                        '--contracts', str(manifest), '--output', str(ir), '--object', str(obj)],
                       check=True, capture_output=True)
        report = json.loads(ir.with_suffix('.json').read_text())
        self.assertEqual(report['input_sha256'], contracts['sha256'])
        self.assertEqual(report['native_object']['machine_state_allocas'], 0)
        program = self.directory / 'main.c'
        program.write_text('#include <stdint.h>\n#include <sys/sysctl.h>\n' + '''
extern uint64_t test_entry(uint64_t, uint64_t);
int main(void) {
    int translated = 0;
    size_t bytes = sizeof(translated);
    if (sysctlbyname("sysctl.proc_translated", &translated, &bytes, 0, 0) == 0 && translated) return 1;
    for (uint64_t i = 0; i < 1000; ++i) {
        uint64_t left = i * UINT64_C(0x9e3779b97f4a7c15);
        uint64_t right = i * UINT64_C(0x2545f4914f6cdd1d);
        if (test_entry(left, right) != left + right) return 2;
    }
    return 0;
}
''')
        binary = self.directory / 'native'
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', str(obj), str(program), '-o', str(binary)], check=True, capture_output=True)
        self.assertEqual(subprocess.check_output(['xcrun', 'lipo', '-archs', str(binary)], text=True).strip(), 'arm64')
        subprocess.run([str(binary)], check=True, capture_output=True)

    def test_wrapping_addition(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('4889f84801f0c3'), ['i64', 'i64']))
        randoms = random.Random(321)
        values = [0, 1, (1 << 64) - 1, 1 << 63, (1 << 63) - 1] + [randoms.getrandbits(64) for _ in range(100)]
        for left in values:
            for right in values:
                self.assertEqual(function(left, right), (left + right) & ((1 << 64) - 1))

    def test_signed_comparison_with_overflow(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('4839f77d044889f0c34889f8c3'), ['i64', 'i64']))
        values = [0, 1, (1 << 64) - 1, 1 << 63, (1 << 63) - 1, (1 << 64) - 10]
        signed = lambda value: ctypes.c_int64(value).value
        for left in values:
            for right in values:
                expected = left if signed(left) >= signed(right) else right
                self.assertEqual(function(left, right), expected)

    def test_partial_registers(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('b807000000b4fec3')))
        self.assertEqual(function(), 0xfe07)

    def test_sign_extension(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('480fbe07c3'), ['ptr']))
        for value in range(256):
            byte = ctypes.c_ubyte(value)
            expected = ctypes.c_int8(value).value & ((1 << 64) - 1)
            self.assertEqual(function(ctypes.addressof(byte)), expected)

    def test_carry_branch(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('4889f84801f07201c331c0c3'), ['i64', 'i64']))
        randoms = random.Random(7654)
        for _ in range(10000):
            left, right = randoms.getrandbits(64), randoms.getrandbits(64)
            total = left + right
            self.assertEqual(function(left, right), total if total < 1 << 64 else 0)

    def test_narrow_memory_write(self):
        function, _, _ = self.compile(*fixture(bytes.fromhex('4088370fb607c3'), ['ptr', 'i32']))
        for value in (0, 1, 255, 256, 0xffff, 0xffffffff):
            buffer = (ctypes.c_ubyte * 3)(0xa5, 0, 0x5a)
            self.assertEqual(function(ctypes.addressof(buffer) + 1, value), value & 0xff)
            self.assertEqual(list(buffer), [0xa5, value & 0xff, 0x5a])

    def test_native_loop_and_pointer(self):
        code = bytes.fromhex('31c031c94885f6740d480304cf4883c1014839f172f3c3')
        function, _, _ = self.compile(*fixture(code, ['ptr', 'i64']))
        values = (ctypes.c_uint64 * 65)(*[i * 321 for i in range(65)])
        for count in range(66):
            self.assertEqual(function(ctypes.addressof(values), count), sum(values[:count]))

    def test_native_calls_and_saved_argument(self):
        code = bytes.fromhex('534889fbe8050000004801d85bc34889f84801f0c3')
        contracts = [{'name': 'test_entry', 'address': 0x1000, 'size': 14, 'result': 'i64', 'parameters': ['i64', 'i64']},
                     {'name': 'callee', 'address': 0x100e, 'size': 7, 'result': 'i64', 'parameters': ['i64', 'i64']}]
        function, report, _ = self.compile(*fixture(code, functions=contracts))
        for left in range(15):
            self.assertEqual(function(left, 17), left * 2 + 17)
        self.assertEqual(len(report['functions']), 2)

    def test_native_import(self):
        code = bytes.fromhex('e803000000c39090ff25f20f0000')
        functions = [{'name': 'test_entry', 'address': 0x1000, 'size': 6, 'result': 'i64', 'parameters': ['i64', 'i64']}]
        imports = {'exampleNid': {'name': 'native_add', 'result': 'i64', 'parameters': ['i64', 'i64']}}
        function, report, _ = self.compile(*fixture(code, functions=functions, imports=imports),
                                          extra='unsigned long long native_add(unsigned long long a, unsigned long long b) { return a + b; }')
        self.assertEqual(function(0xffffffffffffffff, 2), 1)
        self.assertEqual(report['imports'], 1)

    def test_data_relocation(self):
        code = bytes.fromhex('488d05f90f0000488b00488b00c3')
        raw, contracts = fixture(code, relocations=[[0x2000, 0x2008]])
        raw = bytearray(raw)
        struct.pack_into('<Q', raw, 0x2008, 0x76543210abcdef01)
        contracts['sha256'] = hashlib.sha256(raw).hexdigest()
        function, _, _ = self.compile(bytes(raw), contracts)
        self.assertEqual(function(), 0x76543210abcdef01)

    def test_rejections(self):
        for code, message in [('0f05c3', 'instruction semantics'), ('ffe0', 'indirect branches'),
                              ('b001c3', 'does not define rax'), ('c20800', 'stack cleanup'),
                              ('4883ec08c3', 'unbalanced'), ('7500c3', 'undefined branch flags')]:
            with self.subTest(code=code):
                raw, contracts = fixture(bytes.fromhex(code))
                path = self.directory / 'rejected.elf'
                path.write_bytes(raw)
                with self.assertRaisesRegex(Rejected, message):
                    compile_image(Image(path, contracts))

    def test_hash_binding(self):
        raw, contracts = fixture(bytes.fromhex('31c0c3'))
        path = self.directory / 'modified.elf'
        path.write_bytes(raw + b'changed')
        with self.assertRaisesRegex(Rejected, 'hash'):
            Image(path, contracts)

    def test_missing_import_contract(self):
        raw, contracts = fixture(bytes.fromhex('31c0c3'), imports={'exampleNid': {'name': 'native_add', 'result': 'i64', 'parameters': []}})
        contracts['imports'] = {}
        path = self.directory / 'missing.elf'
        path.write_bytes(raw)
        with self.assertRaisesRegex(Rejected, 'missing import ABI'):
            Image(path, contracts)

    def test_constant_store_reconstruction(self):
        stores = []
        for index in range(8):
            stores.extend([f'  %p{index} = getelementptr i8, ptr %local, i64 {index * 4}',
                           f'  store i32 {index + 1}, ptr %p{index}, align 1'])
        ir = '\n'.join(['target triple = "arm64-apple-macosx13.0.0"',
                        'declare i64 @consume(ptr)', 'define i64 @test_entry() {', 'entry:',
                        '  %local = alloca [32 x i8], align 16', *stores,
                        '  %result = call i64 @consume(ptr %local)', '  ret i64 %result', '}'])
        merged, count = merge_constant_stores(ir)
        self.assertEqual(count, 1)
        self.assertIn('@llvm.memcpy', merged)
        original_file, merged_file = self.directory / 'original.ll', self.directory / 'merged.ll'
        original_file.write_text(ir.replace('@test_entry', '@original'))
        merged_file.write_text(merged.replace('@test_entry', '@merged'))
        runner = self.directory / 'check.c'
        runner.write_text('''typedef unsigned long long uint64_t;
extern uint64_t original(void);
extern uint64_t merged(void);
uint64_t consume(const unsigned char* p) {
    uint64_t result = 0;
    for (unsigned i = 0; i < 32; ++i) result = result * 131 + p[i];
    return result;
}
int main(void) { return original() == merged() ? 0 : 1; }
''')
        binary = self.directory / 'check'
        subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', str(original_file), str(merged_file), str(runner), '-o', str(binary)],
                       check=True, capture_output=True)
        subprocess.run([str(binary)], check=True)

    def test_constant_store_load_barrier(self):
        lines = ['target triple = "arm64-apple-macosx13.0.0"', 'define i64 @test_entry() {', 'entry:',
                 '  %local = alloca [32 x i8], align 16',
                 '  %p = getelementptr i8, ptr %local, i64 8',
                 '  store i64 1, ptr %local, align 8',
                 '  %observed = load i64, ptr %local, align 8',
                 '  store i64 2, ptr %p, align 8', '  ret i64 %observed', '}']
        _, count = merge_constant_stores('\n'.join(lines))
        self.assertEqual(count, 0)

    def test_object_toolchain(self):
        raw, contracts = fixture(bytes.fromhex('4889f84801f0c3'), ['i64', 'i64'])
        path = self.directory / 'input.elf'
        path.write_bytes(raw)
        ir, _ = compile_image(Image(path, contracts))
        output = self.directory / 'native.o'
        report = build_native_object(ir, output)
        self.assertEqual(report['status'], 'native_object_not_product_qualified')
        self.assertEqual(report['machine_state_allocas'], 0)
        self.assertGreater(report['object_bytes'], 0)


if __name__ == '__main__':
    unittest.main()
