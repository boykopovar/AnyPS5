import ctypes
import random
import subprocess
import tempfile
import unittest
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

import integer
import test_compiler
from compiler import Function, Rejected, REGISTERS


FLAG_ORDER = ('cf', 'zf', 'sf', 'of', 'pf')
MASK64 = (1 << 64) - 1


def signed(value, width):
    value &= (1 << width) - 1
    return value - (1 << width) if value >> (width - 1) else value


def result_flags(value, width):
    return {'zf': int(value == 0), 'sf': value >> (width - 1), 'pf': int((value & 255).bit_count() % 2 == 0)}


def merge_register(old, value, width):
    return value if width >= 32 else (old & (MASK64 ^ ((1 << width) - 1))) | value


class Harness(Function):
    def __init__(self, instruction, flags=integer.FLAGS, division=None):
        self.serial, self.depth = 0, 256
        self.memory_writes = {}
        self.contract = {'name': 'probe', 'integer_division': division}
        self.lines = ['declare void @llvm.trap()', 'declare i8 @llvm.ctpop.i8(i8)',
                      'define void @probe(i64 %input_rax, i64 %input_rdx, i64 %input_rcx, i64 %input_rdi, i64 %input_rsi, i64 %input_flags, ptr %output) {', 'entry:']
        known = {register: MASK64 for register in REGISTERS}
        self.states = {instruction.address: {'known': known, 'flags': set(flags), 'pointers': {}, 'saved': {}, 'rsp': 0, 'rbp': None,
                                            'native_pointers': set(), 'stack_pointers': {}, 'image_pointers': {}}}
        for register in REGISTERS:
            self.lines.append(f'  %{register} = alloca i64, align 8')
            initial = '%input_' + register if register in ('rax', 'rdx', 'rcx', 'rdi', 'rsi') else '0'
            self.lines.append(f'  store i64 {initial}, ptr %{register}, align 8')
        for index, name in enumerate(FLAG_ORDER):
            self.lines.append(f'  %{name} = alloca i1, align 1')
            value = self.emit(f'lshr i64 %input_flags, {index}')
            integer.store_flag(self, name, self.emit(f'trunc i64 {value} to i1'))

    def finish(self, instruction):
        integer.lower(self, instruction)
        combined = '0'
        for index, name in enumerate(FLAG_ORDER):
            value = self.emit(f'zext i1 {integer.load_flag(self, name)} to i64')
            value = self.emit(f'shl i64 {value}, {index}')
            combined = self.emit(f'or i64 {combined}, {value}')
        for index, value in enumerate([self.load('rax'), self.load('rdx'), combined]):
            address = self.emit(f'getelementptr i64, ptr %output, i64 {index}')
            self.lines.append(f'  store i64 {value}, ptr {address}, align 8')
        self.lines.extend(['  ret void', '}'])
        return '\n'.join(self.lines)


class IntegerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='anyps5-integer-')
        self.directory = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        self.counter = 0

    def decode(self, code):
        decoder = Cs(CS_ARCH_X86, CS_MODE_64)
        decoder.detail = True
        result = list(decoder.disasm(bytes.fromhex(code), 0x1000))
        self.assertEqual(len(result), 1)
        return result[0]

    def native(self, code, division=None):
        ins = self.decode(code)
        harness = Harness(ins, division=division)
        analyzed = {key: value.copy() if isinstance(value, (dict, set)) else value
                    for key, value in harness.states[ins.address].items()}
        self.assertTrue(integer.analyze(harness, ins, analyzed))
        ir = harness.finish(ins)
        self.counter += 1
        source = self.directory / f'probe{self.counter}.ll'
        destination = source.with_suffix('.dylib')
        source.write_text(ir)
        result = subprocess.run(['xcrun', 'clang', '-arch', 'arm64', '-O3', '-dynamiclib', str(source), '-o', str(destination)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        library = ctypes.CDLL(str(destination))
        function = library.probe
        function.argtypes = [ctypes.c_uint64] * 6 + [ctypes.POINTER(ctypes.c_uint64)]
        function.restype = None
        def run(rax, rdx=0, rcx=0, rdi=0, rsi=0, flags=0):
            output = (ctypes.c_uint64 * 3)()
            function(rax, rdx, rcx, rdi, rsi, flags, output)
            return output[0], output[1], {name: output[2] >> index & 1 for index, name in enumerate(FLAG_ORDER)}
        return run

    def test_reset_lowest_set_bit(self):
        randoms = random.Random(6649)
        for width, code in ((32, 'c4e278f3ce'), (64, 'c4e2f8f3ce')):
            run = self.native(code)
            mask = (1 << width) - 1
            values = [0, 1, 2, 3, mask, 1 << (width - 1)] + [randoms.getrandbits(64) for _ in range(256)]
            for source in values:
                value = source & mask
                expected = value & (value - 1)
                result, _, flags = run(MASK64, rsi=source)
                self.assertEqual(result, expected)
                self.assertEqual({key: flags[key] for key in ('cf', 'zf', 'sf', 'of')},
                                 {'cf': int(value == 0), 'zf': int(expected == 0), 'sf': expected >> (width - 1), 'of': 0})
            ins = self.decode(code)
            harness = Harness(ins)
            state = harness.states[ins.address]
            integer.analyze(harness, ins, state)
            self.assertNotIn('pf', state['flags'])

    def test_register_bit_test(self):
        for width, prefix in ((16, '66'), (32, ''), (64, '48')):
            run = self.native(prefix + '0fa3c8')
            for value in (0, 1, MASK64, 0x123456789abcdef0, 1 << (width - 1)):
                for index in (0, 1, width - 1, width, width + 1, MASK64):
                    for zero in (0, 1):
                        result, _, flags = run(value, rcx=index, flags=zero << 1)
                        self.assertEqual(result, value)
                        self.assertEqual(flags['cf'], (value >> (index % width)) & 1)
                        self.assertEqual(flags['zf'], zero)
            run = self.native(prefix + '0fbae0ff')
            self.assertEqual(run(1 << (width - 1))[2]['cf'], 1)
            ins = self.decode(prefix + '0fa3c8')
            harness = Harness(ins)
            state = harness.states[ins.address]
            integer.analyze(harness, ins, state)
            self.assertEqual(state['flags'], {'cf', 'zf'})

    def test_increment_decrement_preserve_carry(self):
        for width, prefix, opcode in [(8, '', 'fe'), (16, '66', 'ff'), (32, '', 'ff'), (64, '48', 'ff')]:
            mask = (1 << width) - 1
            for operation, suffix, delta in [('inc', 'c0', 1), ('dec', 'c8', -1)]:
                run = self.native(prefix + opcode + suffix)
                for value in [0, 1, mask, mask >> 1, 1 << (width - 1), 0x123456789abcdef0]:
                    for carry in (0, 1):
                        result, _, flags = run(value, flags=carry)
                        truncated = ((value & mask) + delta) & mask
                        self.assertEqual(result, merge_register(value, truncated, width))
                        self.assertEqual(flags['cf'], carry)
                        self.assertEqual(flags['of'], int(not -(1 << (width - 1)) <= signed(value, width) + delta < (1 << (width - 1))))
                        for key, expected in result_flags(truncated, width).items():
                            self.assertEqual(flags[key], expected)

    def test_adc_sbb_arithmetic_and_flags(self):
        randoms = random.Random(1822)
        for width, prefix, suffix in [(8, '', 'd0'), (16, '66', 'd0'), (32, '', 'd0'), (64, '48', 'd0')]:
            mask = (1 << width) - 1
            for op in ('adc', 'sbb'):
                opcode = ('10' if width == 8 else '11') if op == 'adc' else ('18' if width == 8 else '19')
                run = self.native(prefix + opcode + suffix)
                values = [0, 1, mask, mask >> 1, 1 << (width - 1)] + [randoms.getrandbits(64) for _ in range(15)]
                for left in values:
                    for right in values:
                        for carry in (0, 1):
                            result, _, flags = run(left, right, flags=carry)
                            full = (left & mask) + (right & mask) + carry if op == 'adc' else (left & mask) - (right & mask) - carry
                            signed_full = signed(left, width) + signed(right, width) + carry if op == 'adc' else signed(left, width) - signed(right, width) - carry
                            self.assertEqual(result, merge_register(left, full & mask, width))
                            self.assertEqual(flags['cf'], int(full > mask or full < 0))
                            self.assertEqual(flags['of'], int(not -(1 << (width - 1)) <= signed_full < (1 << (width - 1))))
                            for key, expected in result_flags(full & mask, width).items():
                                self.assertEqual(flags[key], expected)

    def test_masked_shift_counts_all_widths(self):
        for width, prefix, opcode in [(8, '', 'd2'), (16, '66', 'd3'), (32, '', 'd3'), (64, '48', 'd3')]:
            mask = (1 << width) - 1
            for op, suffix in [('shl', 'e0'), ('shr', 'e8'), ('sar', 'f8')]:
                run = self.native(prefix + opcode + suffix)
                for value in [0, 1, mask, mask >> 1, 1 << (width - 1), 0x123456789abcdef0]:
                    for raw_count in [0, 1, width - 1, width, width + 1, 31, 32, 33, 63, 64, 65, 127, 255]:
                        count = raw_count & (63 if width == 64 else 31)
                        result, _, flags = run(value, rcx=raw_count, flags=0b10101)
                        old = value & mask
                        expected = ((old << count) if op == 'shl' else ((signed(old, width) if op == 'sar' else old) >> count)) & mask
                        self.assertEqual(result, merge_register(value, expected, width), (op, width, value, raw_count))
                        if not count:
                            self.assertEqual(flags, {name: 0b10101 >> index & 1 for index, name in enumerate(FLAG_ORDER)})
                            continue
                        for key, wanted in result_flags(expected, width).items():
                            self.assertEqual(flags[key], wanted, (op, width, count, key))
                        if count < width or op == 'sar':
                            carry = (old >> (width - count)) & 1 if op == 'shl' else ((signed(old, width) if op == 'sar' else old) >> (count - 1)) & 1
                            self.assertEqual(flags['cf'], carry)
                        if count == 1:
                            overflow = ((expected >> (width - 1)) ^ flags['cf']) if op == 'shl' else (old >> (width - 1) if op == 'shr' else 0)
                            self.assertEqual(flags['of'], overflow)

    def test_immediate_zero_shift_clears_upper_dword_and_preserves_flags(self):
        run = self.native('c1e000')
        result, _, flags = run(0x12345678abcdef01, flags=31)
        self.assertEqual(result, 0xabcdef01)
        self.assertTrue(all(flags.values()))

    def test_signed_multiply_and_overflow(self):
        for width, prefix in [(16, '66'), (32, ''), (64, '48')]:
            run = self.native(prefix + '0fafc2')
            mask = (1 << width) - 1
            for left in (0, 1, mask, mask >> 1, 1 << (width - 1)):
                for right in (0, 1, 2, mask, mask >> 1, 1 << (width - 1)):
                    result, _, flags = run(left, right)
                    full = signed(left, width) * signed(right, width)
                    self.assertEqual(result, merge_register(left, full & mask, width))
                    overflow = int(not -(1 << (width - 1)) <= full < (1 << (width - 1)))
                    self.assertEqual((flags['cf'], flags['of']), (overflow, overflow))
        run = self.native('486bc2f9')
        self.assertEqual(run(0, 9)[0], (-63) & MASK64)

    def test_conditions_and_sign_extension(self):
        run = self.native('480f47c2')
        self.assertEqual(run(10, 20, flags=0)[0], 20)
        self.assertEqual(run(10, 20, flags=1)[0], 10)
        self.assertEqual(run(10, 20, flags=2)[0], 10)
        run = self.native('0f92c0')
        self.assertEqual(run(0x1234567800, flags=1)[0], 0x1234567801)
        run = self.native('4898')
        self.assertEqual(run(0x80000000)[0], 0xffffffff80000000)
        self.assertEqual(run(0x7fffffff)[0], 0x7fffffff)
        run = self.native('4899')
        self.assertEqual(run(1 << 63)[1], MASK64)
        self.assertEqual(run(1)[1], 0)

    def test_negate_not(self):
        for width, prefix, opcode in [(8, '', 'f6'), (16, '66', 'f7'), (32, '', 'f7'), (64, '48', 'f7')]:
            mask = (1 << width) - 1
            negative, inverse = self.native(prefix + opcode + 'd8'), self.native(prefix + opcode + 'd0')
            for value in (0, 1, mask, 1 << (width - 1), 0x123456789abcdef0):
                result, _, flags = negative(value)
                self.assertEqual(result, merge_register(value, (-value) & mask, width))
                self.assertEqual(flags['cf'], int(value & mask != 0))
                self.assertEqual(flags['of'], int(value & mask == 1 << (width - 1)))
                result, _, flags = inverse(value, flags=31)
                self.assertEqual(result, merge_register(value, ~value & mask, width))
                self.assertTrue(all(flags.values()))

    def test_division_requires_explicit_qualification(self):
        ins = self.decode('48f7f1')
        harness = Harness(ins)
        with self.assertRaisesRegex(Rejected, 'nonzero-no-overflow'):
            integer.analyze(harness, ins, harness.states[ins.address])
        randoms = random.Random(987)
        for width, prefix, opcode in [(8, '', 'f6'), (16, '66', 'f7'), (32, '', 'f7'), (64, '48', 'f7')]:
            run = self.native(prefix + opcode + 'f1', division='nonzero-no-overflow')
            for _ in range(100):
                denominator = randoms.randrange(1, 1 << width)
                high = randoms.randrange(denominator)
                low = randoms.getrandbits(width)
                numerator = (high << width) | low
                quotient, remainder = divmod(numerator, denominator)
                rax = numerator if width == 8 else low
                result, result_high, _ = run(rax, high, denominator)
                if width == 8:
                    self.assertEqual(result & 65535, quotient | remainder << 8)
                else:
                    self.assertEqual(result, quotient)
                    self.assertEqual(result_high, remainder)

    def test_undefined_flags_are_rejected(self):
        for code, wanted in [('480f47c2', 'condition flags'), ('4811d0', 'carry input')]:
            ins = self.decode(code)
            harness = Harness(ins, flags=set())
            with self.assertRaisesRegex(Rejected, wanted):
                integer.analyze(harness, ins, harness.states[ins.address])
        ins = self.decode('48c1e002')
        harness = Harness(ins)
        state = harness.states[ins.address]
        integer.analyze(harness, ins, state)
        self.assertNotIn('of', state['flags'])
        self.assertIn('cf', state['flags'])

    def test_conditional_stack_pointer_change_is_rejected(self):
        ins = self.decode('480f44e0')
        harness = Harness(ins)
        with self.assertRaisesRegex(Rejected, 'dynamic native frame'):
            integer.analyze(harness, ins, harness.states[ins.address])

    def test_integrated_native_carry_chain(self):
        existing = test_compiler.CompilerTests(methodName='test_wrapping_addition')
        existing.setUp()
        self.addCleanup(existing.doCleanups)
        raw, contracts = test_compiler.fixture(bytes.fromhex('4889f84839f748ffc04819f0c3'), ['i64', 'i64'])
        function, _, _ = existing.compile(raw, contracts)
        values = [0, 1, 2, (1 << 63) - 1, 1 << 63, MASK64]
        for left in values:
            for right in values:
                self.assertEqual(function(left, right), (left + 1 - right - int(left < right)) & MASK64)


if __name__ == '__main__':
    unittest.main()
