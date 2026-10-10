import subprocess
import unittest

from abi import (ABIError, bits, llvm_type, locations, parameter_type,
                 result_type, return_register, signature)


class ABITests(unittest.TestCase):
    def test_integer_and_sse_banks_are_independent(self):
        types = ['i64', 'f64', 'ptr', 'f32', 'i32', 'f64', 'u8', 'i16', 'ptr']
        result = locations(types)
        self.assertEqual([item['name'] for item in result],
                         ['rdi', 'xmm0', 'rsi', 'xmm1', 'rdx', 'xmm2', 'rcx', 'r8', 'r9'])
        self.assertEqual([item['bits'] for item in result], [64, 64, 64, 32, 32, 64, 8, 16, 64])

    def test_overflow_integer_args_do_not_spill_available_float_regs(self):
        result = locations(['i64'] * 7 + ['f64', 'i16', 'f32'])
        self.assertEqual(result[6], {'kind': 'stack', 'offset': 8, 'type': 'i64', 'bits': 64})
        self.assertEqual(result[7]['name'], 'xmm0')
        self.assertEqual(result[8], {'kind': 'stack', 'offset': 16, 'type': 'i16', 'bits': 16})
        self.assertEqual(result[9]['name'], 'xmm1')

    def test_float_spills_and_integer_spills_share_ordered_stack_slots(self):
        result = locations(['f64'] * 9 + ['i64'] * 7 + ['f32', 'u8'])
        stack = [(index, item['offset']) for index, item in enumerate(result) if item['kind'] == 'stack']
        self.assertEqual(stack, [(8, 8), (15, 16), (16, 24), (17, 32)])
        self.assertEqual(result[9]['name'], 'rdi')
        self.assertEqual(result[14]['name'], 'r9')

    def test_narrow_argument_and_result_attribute_order(self):
        for kind, integer, attribute in [('i8', 'i8', 'signext'), ('u8', 'i8', 'zeroext'),
                                         ('i16', 'i16', 'signext'), ('u16', 'i16', 'zeroext')]:
            self.assertEqual(parameter_type(kind), f'{integer} {attribute}')
            self.assertEqual(result_type(kind), f'{attribute} {integer}')
        self.assertEqual(parameter_type('i32'), 'i32')
        self.assertEqual(parameter_type('u64'), 'i64')
        self.assertEqual(result_type('void'), 'void')
        self.assertEqual(llvm_type('f32'), 'float')
        self.assertEqual(llvm_type('f64'), 'double')

    def test_return_register_depends_on_scalar_class(self):
        self.assertEqual(return_register('ptr'), 'rax')
        self.assertEqual(return_register('u8'), 'rax')
        self.assertEqual(return_register('f32'), 'xmm0')
        self.assertEqual(return_register('f64'), 'xmm0')
        self.assertIsNone(return_register('void'))

    def test_signature_accepts_scalar_stack_parameters(self):
        contract = {'name': 'call_with_stack', 'result': 'f64', 'parameters': ['i64'] * 9 + ['f64'] * 10}
        self.assertEqual(signature(contract), ('f64', contract['parameters']))

    def test_unsupported_aggregates_and_variadics_rejected(self):
        base = {'name': 'entry', 'result': 'void', 'parameters': []}
        for key, value in [('variadic', True), ('sret', 'ptr'), ('byval', ['ptr'])]:
            with self.subTest(key=key), self.assertRaises(ABIError):
                signature({**base, key: value})
        for kind in ('void', 'i128', 'f80', 'struct', '<4 x float>', {'size': 8}):
            with self.subTest(kind=kind), self.assertRaises(ABIError):
                locations([kind])

    def test_fastcc_needs_explicit_qualified_machine_locations(self):
        contract = {'name': 'entry', 'result': 'i32', 'parameters': ['i64', 'f32'],
                    'guest_calling_convention': 'x86_64-llvm-fastcc'}
        with self.assertRaisesRegex(ABIError, 'explicit verified'):
            signature(contract)
        contract.update(guest_locations=locations(contract['parameters']), guest_return_register='rax')
        self.assertEqual(signature(contract), ('i32', ['i64', 'f32']))
        contract['guest_locations'][0]['name'] = 'rcx'
        with self.assertRaisesRegex(ABIError, 'explicit verified'):
            signature(contract)

    def test_native_clang_abi_agrees_for_narrow_signedness(self):
        source = ('signed char s8(signed char x) { return x; }\n'
                  'unsigned char u8(unsigned char x) { return x; }\n'
                  'short s16(short x) { return x; }\n'
                  'unsigned short u16(unsigned short x) { return x; }\n')
        output = subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O1', '-S', '-emit-llvm', '-x', 'c', '-', '-o', '-'],
                                input=source, text=True, capture_output=True, check=True).stdout
        for name, kind in [('s8', 'i8'), ('u8', 'u8'), ('s16', 'i16'), ('u16', 'u16')]:
            declaration = next(line for line in output.splitlines() if line.startswith('define ') and f'@{name}(' in line)
            attribute = 'zeroext' if kind.startswith('u') else 'signext'
            self.assertRegex(declaration, rf'define .*\b{attribute} {llvm_type(kind)} @{name}\(')
            self.assertRegex(declaration, rf'@{name}\({llvm_type(kind)} [^)]*\b{attribute}\b')


if __name__ == '__main__':
    unittest.main()
