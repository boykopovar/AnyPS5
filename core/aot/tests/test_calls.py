import copy
import ctypes
import struct
import tempfile
import unittest
from pathlib import Path

from compiler import Image, Rejected, compile_image
from calls import CallError, indirect_call, validate_indirect_calls
import test_compiler


CALLER = bytes.fromhex('4889f84889f74883ec08ffd04883c408c3')
INVERSE = bytes.fromhex('4889f848f7d0c3')


def callback_fixture(caller=CALLER, callback=INVERSE, parameters=None, result='i64', targets=None):
    call_address = 0x100a
    owner = {'name': 'test_entry', 'address': 0x1000, 'size': len(caller), 'result': result,
             'parameters': ['ptr', *(parameters if parameters is not None else ['i64'])],
             'indirect_calls': [{'address': call_address, 'result': result,
                                 'parameters': parameters if parameters is not None else ['i64'],
                                 'targets': targets if targets is not None else [0x1040]}]}
    callee = {'name': 'callback_inverse', 'address': 0x1040, 'size': len(callback), 'result': result,
              'parameters': parameters if parameters is not None else ['i64']}
    code = caller + b'\x90' * (0x40 - len(caller)) + callback
    return test_compiler.fixture(code, functions=[owner, callee])


class CallContractTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='anyps5-call-contracts-')
        self.directory = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)

    def image(self, raw, contracts):
        path = self.directory / 'input.elf'
        path.write_bytes(raw)
        return Image(path, contracts)

    def test_register_call_descriptor(self):
        image = self.image(*callback_fixture())
        owner = image.functions[0x1000]
        result = validate_indirect_calls(image, owner)[0x100a]
        self.assertEqual(result['kind'], 'operand')
        self.assertEqual(result['targets'], ['callback_inverse'])
        self.assertEqual(result['contract']['parameters'], ['i64'])
        self.assertEqual(result['qualification'], 'declared-target-set-not-inferred')
        instruction = next(image.decoder.disasm(image.read(0x100a, 2), 0x100a))
        self.assertEqual(indirect_call(image, instruction, owner), result)

    def test_memory_vtable_call_descriptor(self):
        caller = bytes.fromhex('4889f84889f74883ec08ff50084883c408c3')
        image = self.image(*callback_fixture(caller))
        result = validate_indirect_calls(image, image.functions[0x1000])[0x100a]
        self.assertEqual(result['targets'], ['callback_inverse'])

    def test_multiple_known_targets(self):
        raw, contracts = callback_fixture(targets=[0x1040, 0x1043])
        contracts['functions'][1]['size'] = 3
        contracts['functions'].append({'name': 'other', 'address': 0x1043, 'size': 4,
                                       'result': 'i64', 'parameters': ['i64']})
        image = self.image(raw, contracts)
        self.assertEqual(validate_indirect_calls(image, image.functions[0x1000])[0x100a]['targets'], ['callback_inverse', 'other'])

    def test_known_plt_import_target(self):
        imports = {'callbackNid': {'name': 'native_callback', 'result': 'i64', 'parameters': ['i64']}}
        raw, contracts = callback_fixture(callback=bytes.fromhex('ff25ba0f0000'))
        owner = contracts['functions'][0]
        code = CALLER + b'\x90' * (0x40 - len(CALLER)) + bytes.fromhex('ff25ba0f0000')
        raw, contracts = test_compiler.fixture(code, functions=[owner], imports=imports)
        image = self.image(raw, contracts)
        self.assertEqual(validate_indirect_calls(image, owner)[0x100a]['targets'], ['native_callback'])

    def test_missing_annotation_rejected(self):
        raw, contracts = callback_fixture()
        contracts['functions'][0].pop('indirect_calls')
        image = self.image(raw, contracts)
        instruction = next(image.decoder.disasm(image.read(0x100a, 2), 0x100a))
        with self.assertRaisesRegex(CallError, 'no signature and target-set'):
            indirect_call(image, instruction, image.functions[0x1000])

    def test_empty_or_unknown_target_rejected(self):
        for targets, error in [([], 'nonempty'), ([0x333333], 'unresolved'), ([True], 'invalid'), ([-1], 'invalid')]:
            with self.subTest(targets=targets):
                image = self.image(*callback_fixture(targets=targets))
                with self.assertRaisesRegex(CallError, error):
                    validate_indirect_calls(image, image.functions[0x1000])

    def test_duplicate_annotation_and_target_rejected(self):
        raw, contracts = callback_fixture()
        owner = contracts['functions'][0]
        owner['indirect_calls'].append(copy.deepcopy(owner['indirect_calls'][0]))
        image = self.image(raw, contracts)
        with self.assertRaisesRegex(CallError, 'duplicate indirect call annotation'):
            validate_indirect_calls(image, owner)
        image = self.image(*callback_fixture(targets=[0x1040, 0x1040]))
        with self.assertRaisesRegex(CallError, 'duplicate indirect call target'):
            validate_indirect_calls(image, image.functions[0x1000])

    def test_signature_mismatch_rejected(self):
        for field, value in [('result', 'f64'), ('parameters', ['ptr']), ('parameters', ['i32']), ('parameters', ['i64', 'i64'])]:
            raw, contracts = callback_fixture()
            contracts['functions'][1][field] = value
            image = self.image(raw, contracts)
            with self.subTest(field=field, value=value), self.assertRaisesRegex(CallError, 'ABI does not match'):
                validate_indirect_calls(image, image.functions[0x1000])

    def test_narrow_signedness_must_match_native_extension(self):
        raw, contracts = callback_fixture(parameters=['u8'])
        contracts['functions'][1]['parameters'] = ['i8']
        image = self.image(raw, contracts)
        with self.assertRaisesRegex(CallError, 'ABI does not match'):
            validate_indirect_calls(image, image.functions[0x1000])

    def test_full_width_integer_signedness_is_structurally_compatible(self):
        raw, contracts = callback_fixture(parameters=['u64'])
        contracts['functions'][1]['parameters'] = ['i64']
        image = self.image(raw, contracts)
        self.assertIn(0x100a, validate_indirect_calls(image, image.functions[0x1000]))

    def test_annotation_must_point_to_call_within_owner(self):
        for address in (0x1000, 0x1040, -1):
            raw, contracts = callback_fixture()
            contracts['functions'][0]['indirect_calls'][0]['address'] = address
            image = self.image(raw, contracts)
            with self.subTest(address=address), self.assertRaises(CallError):
                validate_indirect_calls(image, image.functions[0x1000])

    def test_direct_call_cannot_be_annotated_as_indirect(self):
        raw, contracts = callback_fixture(caller=CALLER[:10] + bytes.fromhex('e831000000c3'))
        image = self.image(raw, contracts)
        with self.assertRaisesRegex(CallError, 'register or memory'):
            validate_indirect_calls(image, image.functions[0x1000])

    def test_segment_relative_call_rejected(self):
        raw, contracts = callback_fixture(caller=CALLER[:10] + bytes.fromhex('64ff10c3'))
        image = self.image(raw, contracts)
        with self.assertRaisesRegex(CallError, 'segment-relative'):
            validate_indirect_calls(image, image.functions[0x1000])

    def test_unsupported_or_missing_abi_rejected(self):
        for key, value in [('variadic', True), ('parameters', ['aggregate']), ('result', None)]:
            raw, contracts = callback_fixture()
            contracts['functions'][0]['indirect_calls'][0][key] = value
            image = self.image(raw, contracts)
            with self.subTest(key=key), self.assertRaises(CallError):
                validate_indirect_calls(image, image.functions[0x1000])

    def test_no_annotations_produces_no_speculative_targets(self):
        raw, contracts = callback_fixture()
        contracts['functions'][0].pop('indirect_calls')
        image = self.image(raw, contracts)
        self.assertEqual(validate_indirect_calls(image, image.functions[0x1000]), {})

    def test_noreturn_only_when_every_target_is_noreturn(self):
        raw, contracts = callback_fixture()
        contracts['functions'][0]['indirect_calls'][0]['noreturn'] = True
        image = self.image(raw, contracts)
        with self.assertRaisesRegex(CallError, 'noreturn annotation contradicts'):
            validate_indirect_calls(image, image.functions[0x1000])
        image.functions[0x1040]['noreturn'] = True
        self.assertTrue(validate_indirect_calls(image, image.functions[0x1000])[0x100a]['contract']['noreturn'])


class NativeCallTests(unittest.TestCase):
    def setUp(self):
        self.existing = test_compiler.CompilerTests(methodName='test_wrapping_addition')
        self.existing.setUp()
        self.addCleanup(self.existing.doCleanups)

    def compile(self, raw, contracts):
        function, report, ir = self.existing.compile(raw, contracts)
        library = self.existing.library
        return function, library, report, ir

    def test_native_function_pointer_preserves_all_integer_bits(self):
        function, library, _, ir = self.compile(*callback_fixture())
        pointer = ctypes.cast(library.callback_inverse, ctypes.c_void_p).value
        for value in (0, 1, (1 << 64) - 1, 1 << 63, 0x123456789abcdef0):
            self.assertEqual(function(pointer, value), (~value) & ((1 << 64) - 1))
        self.assertNotIn('switch i64', ir)

    def test_native_vtable_slot_pointer(self):
        caller = bytes.fromhex('4889f84889f74883ec08ff50084883c408c3')
        function, library, _, _ = self.compile(*callback_fixture(caller))
        pointer = ctypes.cast(library.callback_inverse, ctypes.c_void_p).value
        table = (ctypes.c_void_p * 2)(None, pointer)
        self.assertEqual(function(ctypes.addressof(table), 0x123456789abcdef0), 0xedcba9876543210f)

    def test_native_mixed_integer_fp_callback_with_stack_argument(self):
        caller = bytes.fromhex('4889f84889f74883ec18488b4c242048890c24ffd04883c418c3')
        callback = bytes.fromhex('f20f10442408c3')
        raw, contracts = callback_fixture(caller, callback, ['i64'] + ['f64'] * 9, 'f64')
        contracts['functions'][0]['indirect_calls'][0]['address'] = 0x1013
        function, library, _, _ = self.compile(raw, contracts)
        function.argtypes = [ctypes.c_void_p, ctypes.c_uint64] + [ctypes.c_double] * 9
        function.restype = ctypes.c_double
        pointer = ctypes.cast(library.callback_inverse, ctypes.c_void_p).value
        for last in (0.0, -0.0, 1.125, -27.75, 1.0e200, 5e-324):
            result = function(pointer, 0xfedcba9876543210, *[index + 0.5 for index in range(8)], last)
            self.assertEqual(struct.pack('<d', result), struct.pack('<d', last))


def tail_fixture(callee_code, parameters=None, result='i64', prefix=b'', owner_parameters=None, owner_result=None):
    parameters = [] if parameters is None else parameters
    entry = prefix + b'\xe9' + struct.pack('<i', 0x40 - len(prefix) - 5)
    code = entry + b'\x90' * (0x40 - len(entry)) + callee_code
    functions = [{'name': 'test_entry', 'address': 0x1000, 'size': len(entry),
                  'result': result if owner_result is None else owner_result,
                  'parameters': parameters if owner_parameters is None else owner_parameters},
                 {'name': 'tail_target', 'address': 0x1040, 'size': len(callee_code),
                  'result': result, 'parameters': parameters}]
    return test_compiler.fixture(code, functions=functions)


class NativeTailCallTests(unittest.TestCase):
    setUp = test_compiler.CompilerTests.setUp

    def image(self, raw, contracts):
        path = self.directory / 'input.elf'
        path.write_bytes(raw)
        return Image(path, contracts)

    def compile_native(self, raw, contracts, provider=''):
        root = self.directory
        self.serial = getattr(self, 'serial', 0) + 1
        self.directory = root / str(self.serial)
        self.directory.mkdir()
        try:
            return test_compiler.CompilerTests.compile(self, raw, contracts, extra=provider)
        finally:
            self.directory = root

    def test_direct_integer_tail_call(self):
        raw, contracts = tail_fixture(bytes.fromhex('4889f84801f0c3'), ['i64', 'i64'])
        function, report, optimized = self.compile_native(raw, contracts)
        for left, right in [(3, 7), ((1 << 64) - 1, 2), (1 << 63, 1 << 63)]:
            self.assertEqual(function(left, right), (left + right) & ((1 << 64) - 1))
        self.assertEqual(report['functions'][0]['native_tail_calls'], 1)
        self.assertNotIn('switch i64', optimized)

    def test_tail_call_preserves_float_return_bits(self):
        raw, contracts = tail_fixture(bytes.fromhex('f20f10c1c3'), ['i64', 'f64', 'f64'], 'f64')
        function, _, _ = self.compile_native(raw, contracts)
        function.argtypes = [ctypes.c_uint64, ctypes.c_double, ctypes.c_double]
        function.restype = ctypes.c_double
        for last in (0.0, -0.0, 1.125, -42.75, 1e200, 5e-324):
            self.assertEqual(struct.pack('<d', function(17, 0.5, last)), struct.pack('<d', last))

    def test_tail_stack_arguments_use_incoming_offsets(self):
        for prefix in (b'', bytes.fromhex('4883ec0848c70424070000004883c408')):
            with self.subTest(prefix=prefix.hex()):
                raw, contracts = tail_fixture(bytes.fromhex('488b4424084803442410c3'), ['i64'] * 8, prefix=prefix)
                function, _, _ = self.compile_native(raw, contracts)
                self.assertEqual(function(1, 2, 3, 4, 5, 6, 55, 89), 144)
                self.assertEqual(function(1, 2, 3, 4, 5, 6, (1 << 64) - 1, 2), 1)

    def test_tail_float_stack_argument(self):
        parameters = ['i64'] + ['f64'] * 9
        raw, contracts = tail_fixture(bytes.fromhex('f20f10442408c3'), parameters, 'f64')
        function, _, _ = self.compile_native(raw, contracts)
        function.argtypes = [ctypes.c_uint64] + [ctypes.c_double] * 9
        function.restype = ctypes.c_double
        for last in (-0.0, 1.125, -1e200, 5e-324):
            self.assertEqual(struct.pack('<d', function(17, *[index + 0.5 for index in range(8)], last)), struct.pack('<d', last))

    def test_got_import_tail_call_and_direct_plt_tail(self):
        imports = {'nativeNid': {'name': 'native_subtract', 'result': 'i64', 'parameters': ['i64', 'i64']}}
        provider = 'unsigned long long native_subtract(unsigned long long a, unsigned long long b) { return a - b; }'
        for code, size in [(bytes.fromhex('ff25fa0f0000'), 6),
                           (bytes.fromhex('e903000000909090ff25f20f0000'), 5)]:
            with self.subTest(code=code.hex()):
                functions = [{'name': 'test_entry', 'address': 0x1000, 'size': size, 'result': 'i64', 'parameters': ['i64', 'i64']}]
                function, report, _ = self.compile_native(*test_compiler.fixture(code, functions=functions, imports=imports), provider)
                self.assertEqual(function(3, 7), (1 << 64) - 4)
                self.assertEqual(report['functions'][0]['native_tail_calls'], 1)

    def test_got_tail_call_with_eight_arguments(self):
        parameters = ['i64'] * 8
        imports = {'nativeNid': {'name': 'native_eight', 'result': 'i64', 'parameters': parameters}}
        raw, contracts = test_compiler.fixture(bytes.fromhex('ff25fa0f0000'), parameters, imports=imports)
        provider = 'unsigned long long native_eight(unsigned long long a, unsigned long long b, unsigned long long c, unsigned long long d, unsigned long long e, unsigned long long f, unsigned long long g, unsigned long long h) { return g * 100 + h; }'
        function, _, _ = self.compile_native(raw, contracts, provider)
        self.assertEqual(function(1, 2, 3, 4, 5, 6, 55, 89), 5589)

    def test_internal_jump_remains_control_flow(self):
        code = bytes.fromhex('eb0331c0c3b82a000000c3')
        function, report, _ = self.compile_native(*test_compiler.fixture(code))
        self.assertEqual(function(), 42)
        self.assertEqual(report['functions'][0]['native_tail_calls'], 0)

    def test_tail_return_abi_mismatch_and_narrow_extension_rejected(self):
        for callee_result, owner_result in [('i32', 'i64'), ('f64', 'i64'), ('u8', 'i8')]:
            with self.subTest(callee_result=callee_result, owner_result=owner_result):
                raw, contracts = tail_fixture(bytes.fromhex('31c0c3'), result=callee_result, owner_result=owner_result)
                with self.assertRaisesRegex(Rejected, 'tail-call return ABI'):
                    compile_image(self.image(raw, contracts))

    def test_tail_requires_balanced_frame_and_defined_parameters(self):
        for prefix, parameters, owners, error in [(bytes.fromhex('4883ec08'), [], [], 'balanced native frame'),
                                                (bytes.fromhex('53'), [], [], 'balanced native frame'),
                                                (b'', ['i64'], [], 'does not define rdi')]:
            with self.subTest(prefix=prefix.hex(), parameters=parameters):
                raw, contracts = tail_fixture(bytes.fromhex('31c0c3'), parameters, prefix=prefix, owner_parameters=owners)
                with self.assertRaisesRegex(Rejected, error):
                    compile_image(self.image(raw, contracts))

    def test_tail_rejects_undeclared_or_rewritten_stack_slots(self):
        for prefix, owner_parameters, error in [(b'', ['i64'] * 6, 'declared incoming slot'),
                                                (bytes.fromhex('48c744240807000000'), ['i64'] * 7, 'rewritten or escaped'),
                                                (bytes.fromhex('488d442408488938'), ['i64'] * 7, 'rewritten or escaped')]:
            with self.subTest(prefix=prefix.hex(), parameters=len(owner_parameters)):
                raw, contracts = tail_fixture(bytes.fromhex('488b442408c3'), ['i64'] * 7, prefix=prefix, owner_parameters=owner_parameters)
                with self.assertRaisesRegex(Rejected, error):
                    compile_image(self.image(raw, contracts))

    def test_tail_stack_slot_types_are_checked(self):
        raw, contracts = tail_fixture(bytes.fromhex('31c0c3'), ['i64'] * 6 + ['f64'] * 9, owner_parameters=['i64'] * 7 + ['f64'] * 9)
        with self.assertRaisesRegex(Rejected, 'compatible declared incoming slot'):
            compile_image(self.image(raw, contracts))

    def test_unknown_register_and_unresolved_got_tail_targets_rejected(self):
        for code in (bytes.fromhex('ffe0'), bytes.fromhex('ff25fa0f0000'), bytes.fromhex('e93b000000')):
            with self.subTest(code=code.hex()):
                with self.assertRaises(Rejected):
                    compile_image(self.image(*test_compiler.fixture(code)))

    def test_full_width_signedness_is_compatible(self):
        raw, contracts = tail_fixture(bytes.fromhex('4889f8c3'), ['i64'], 'u64', owner_result='i64')
        function, _, _ = self.compile_native(raw, contracts)
        self.assertEqual(function((1 << 64) - 1), (1 << 64) - 1)

    def test_noreturn_got_tail_stops_discovery(self):
        imports = {'nativeNid': {'name': 'native_stop', 'result': 'void', 'parameters': [], 'noreturn': True}}
        raw, contracts = test_compiler.fixture(bytes.fromhex('ff25fa0f0000'), result='void', imports=imports)
        ir, report = compile_image(self.image(raw, contracts))
        self.assertIn('unreachable', ir)
        self.assertEqual(report['functions'][0]['native_tail_calls'], 1)


if __name__ == '__main__':
    unittest.main()
