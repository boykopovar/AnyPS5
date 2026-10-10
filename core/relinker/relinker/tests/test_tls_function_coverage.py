import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


TLS_LOAD = bytes.fromhex("66 66 66 64 48 8b 04 25 00 00 00 00")
RUNNER = Path(__file__).with_name("windows_image_runner.py")
RUNS_ON_HOST = sys.platform == "linux" and platform.machine() == "x86_64"


def register_load(register):
    high, low = register >> 3, register & 7
    extension = b"\x41" if high else b""
    save = extension + bytes([0x50 + low]) if register else b""
    restore = extension + bytes([0x58 + low]) if register else b""
    prefix = save + bytes.fromhex("b8 07 00 00 00 b9 05 00 00 00")
    load = bytes([0x64, 0x48 | high << 2, 0x8b, 0x04 | low << 3, 0x25, 0, 0, 0, 0])
    rest = extension + bytes([0x8b, 0x40 | low]) + (b"\x24" if low == 4 else b"") + b"\xf0" + restore + b"\xc3"
    if register != 1:
        rest = bytes.fromhex("83 f9 05 75") + bytes([len(rest)]) + rest
    if register != 0:
        rest = bytes.fromhex("83 f8 07 75") + bytes([len(rest)]) + rest
    return len(prefix), prefix + load + rest + restore + bytes.fromhex("31 c0 c3")


def fs_load(register, displacement):
    return bytes([0x64, 0x48 | (register >> 3) << 2, 0x8b, 0x04 | (register & 7) << 3, 0x25]) + struct.pack("<i", displacement)


ALU_READ = {"add": 0x03, "or": 0x0b, "adc": 0x13, "sbb": 0x1b, "and": 0x23, "sub": 0x2b, "xor": 0x33, "cmp": 0x3b}


def fs_alu(opcode, register, displacement):
    return bytes([0x64, 0x48 | (register >> 3) << 2, opcode, 0x04 | (register & 7) << 3, 0x25]) + struct.pack("<i", displacement)


def alu_check_body(opcode, register, base, value, result, flags_masked):
    code = bytearray()
    jumps = []

    def emit(data):
        code.extend(data)

    def move_immediate(target, immediate):
        emit(bytes([0x48 | (target >> 3), 0xb8 | (target & 7)]) + struct.pack("<Q", immediate & ((1 << 64) - 1)))

    def compare_with_r8(target):
        emit(bytes([0x48 | 0x44 | (0x01 if target >= 8 else 0), 0x39, 0xc0 | (target & 7)]))

    def fail_unless_equal():
        emit(bytes.fromhex("0f 85 00 00 00 00"))
        jumps.append(len(code) - 4)

    emit(bytes.fromhex("64 c7 04 25 28 00 00 00") + struct.pack("<I", value & 0xffffffff))
    for target in (0, 1, 3):
        move_immediate(target, base)
    emit(fs_alu(opcode, register, 0x28))
    emit(bytes.fromhex("9c 5a 83 e2 41"))
    move_immediate(8, result)
    compare_with_r8(register)
    fail_unless_equal()
    for target in (0, 1, 3):
        if target == register:
            continue
        move_immediate(8, base)
        compare_with_r8(target)
        fail_unless_equal()
    emit(bytes.fromhex("83 fa") + bytes([flags_masked]))
    fail_unless_equal()
    emit(bytes.fromhex("b8 2a 00 00 00 c3 b8 01 00 00 00 c3"))
    failure = len(code) - 6
    for position in jumps:
        struct.pack_into("<i", code, position, failure - position - 4)
    return bytes(code)


def alu_execution_cases():
    mask = (1 << 64) - 1
    cases = (
        ("xor", 0x33, lambda a, b: a ^ b, 0xffffffff00000000, 0xffffffff),
        ("and", 0x23, lambda a, b: a & b, 0xffffffff00000000, 0xffffffff),
        ("or", 0x0b, lambda a, b: a | b, 0xffffffff00000000, 0xffffffff),
        ("add", 0x03, lambda a, b: a + b, 0x0102030405060708, 0x11223344),
        ("sub", 0x2b, lambda a, b: a - b, 0x0000000012345678, 0xffffffff),
        ("cmp", 0x3b, lambda a, b: a, 0x0000000012345678, 0xffffffff),
        ("cmp-equal", 0x3b, lambda a, b: a, 0x00000000ffffffff, 0xffffffff),
    )
    for name, opcode, operation, base, value in cases:
        result = operation(base, value) & mask
        if opcode in (0x2b, 0x3b):
            zero = 1 if (base - value) & mask == 0 else 0
            carry = 1 if base < value else 0
        elif opcode == 0x03:
            zero = 1 if (base + value) & mask == 0 else 0
            carry = 1 if base + value > mask else 0
        else:
            zero = 1 if result == 0 else 0
            carry = 0
        flags_masked = (zero << 6) | carry
        for register in (0, 1, 3):
            body = alu_check_body(opcode, register, base, value, result, flags_masked)
            yield f"alu-exec-{name}-{register}", make_image("register", "unwind", body=body)


def displacement_load(register, displacement, flags, round_trip=False):
    saved = (3, 5, 6, 7, 12, 13, 14, 15)
    code = bytearray()
    failures = []

    def emit(data):
        code.extend(data)

    def check():
        emit(bytes.fromhex("0f 85 00 00 00 00"))
        failures.append(len(code) - 4)

    def immediate(target, value):
        emit(bytes([0x48 | (target >> 3), 0xb8 | (target & 7)]) + struct.pack("<Q", value))

    def stack_store(target, offset):
        emit(bytes([0x48 | (target >> 3) << 2, 0x89, 0x84 | (target & 7) << 3, 0x24]) + struct.pack("<i", offset))

    for target in saved:
        emit((b"\x41" if target >= 8 else b"") + bytes([0x50 | (target & 7)]))
    emit(bytes.fromhex("48 8d a4 24 00 ff ff ff"))
    emit(fs_load(0, 0))
    stack_store(0, 136)
    positive = {8: 0x123456789abcdef0, 16: 0xfedcba9876543210, 40: 0xabcdef1278563412}
    for offset, value in positive.items():
        immediate(1, value)
        emit(bytes([0x48, 0x89, 0x48, offset]))
    immediate(1, 0x2468ace013579bdf)
    emit(bytes.fromhex("48 89 08"))
    if displacement == 0:
        emit(fs_load(2, 40))
        immediate(1, positive[40])
        emit(bytes.fromhex("48 39 ca"))
        check()
    else:
        value = 0xabcdef1289abcdef if round_trip else {
            **positive, -64: 0x8877665544332211, -40: 0x1122334455667788, -8: 0,
        }[displacement]
        immediate(1, value)
        stack_store(1, 136)
    markers = [0x1020304050607000 + target * 0x101 for target in range(16)]
    for target in range(16):
        if target != 4:
            immediate(target, markers[target])
    emit(b"\x68" + struct.pack("<I", flags) + b"\x9d")
    emit(bytes.fromhex("48 8d a4 24 70 ff ff ff 9c 8f 84 24 20 01 00 00 48 8d a4 24 90 00 00 00"))
    for offset in range(-128, 0, 8):
        emit(bytes([0x48, 0xc7, 0x44, 0x24, offset & 255]) + struct.pack("<I", 0x34560000 - offset))
    if round_trip:
        emit(bytes.fromhex("64 c7 04 25 28 00 00 00 ef cd ab 89"))
    load_offset = len(code)
    emit(fs_load(register, displacement))
    for target in range(16):
        stack_store(target, target * 8)
    emit(bytes.fromhex("48 8d a4 24 70 ff ff ff 9c 8f 84 24 10 01 00 00 48 8d a4 24 90 00 00 00"))
    emit(bytes.fromhex("48 8b 84 24 90 00 00 00 48 39 84 24 80 00 00 00"))
    check()
    for target in range(16):
        if target == 4:
            emit(bytes.fromhex("48 8d 04 24"))
        elif target == register:
            emit(bytes.fromhex("48 8b 84 24 88 00 00 00"))
        else:
            immediate(0, markers[target])
        emit(bytes.fromhex("48 39 84 24") + struct.pack("<i", target * 8))
        check()
    for offset in range(-128, 0, 8):
        emit(bytes([0x48, 0x81, 0x7c, 0x24, offset & 255]) + struct.pack("<I", 0x34560000 - offset))
        check()
    emit(bytes.fromhex("b8 2a 00 00 00 eb 05"))
    failure = len(code)
    emit(bytes.fromhex("b8 01 00 00 00 48 8d a4 24 00 01 00 00"))
    for target in reversed(saved):
        emit((b"\x41" if target >= 8 else b"") + bytes([0x58 | (target & 7)]))
    emit(b"\xc3")
    for offset in failures:
        struct.pack_into("<i", code, offset, failure - offset - 4)
    return load_offset, code


def displacement_cases():
    for register in range(16):
        if register == 4:
            continue
        for flags in (0x202, 0xad7):
            for displacement, round_trip in ((0, False), (40, False), (-64, False), (-40, False),
                                             (-8, False), (40, True)):
                offset, body = displacement_load(register, displacement, flags, round_trip)
                image = make_image("register", "unwind", body=body)
                struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 48, 48, 32)
                image[0x800:0x830] = bytes(48)
                struct.pack_into("<Q", image, 0x800, 0x8877665544332211)
                struct.pack_into("<Q", image, 0x818, 0x1122334455667788)
                name = f"displacement-{register}-{flags:x}-{displacement}-{'round-trip' if round_trip else 'load'}"
                yield name, image, 0x1240 + offset


def displacement_bounds_cases():
    for register in (0, 12):
        for displacement in (1, 8, 0x10, 0x20, 0x27, 0x29, 0x2c, 0x30, -65, -0x80000000, 0x7fffffff):
            image = make_image("register", "unwind", body=fs_load(register, displacement) + b"\xc3")
            struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 48, 48, 32)
            if register == 12:
                image[176:232], image[288:344] = image[288:344], image[176:232]
            error = f"Windows guest TLS load displacement {displacement} is not in the thread TLS block, 0 or the stack guard at 0x28"
            yield f"displacement-bounds-{register}-{displacement}", image, error


REGISTER_VALUES = {-8: 0x8877665544332211, 0x10: 0x0123456789abcdef, 0x28: 0xfedcba9876543210}
REGISTER_LOADS = (
    ("rax-rbx", bytes.fromhex("64 48 8b 03"), 0, True, {3: -8}, -8),
    ("rcx-rax", bytes.fromhex("64 48 8b 08"), 1, True, {0: 0x10}, 0x10),
    ("rax-rax", bytes.fromhex("64 48 8b 00"), 0, True, {0: 0x28}, 0x28),
    ("r13-rbx-rcx-disp8", bytes.fromhex("64 4c 8b 6c 0b f0"), 13, True, {3: 0x18, 1: -0x10}, -8),
    ("edx-rbx", bytes.fromhex("64 8b 13"), 2, False, {3: 0x10}, 0x10),
    ("r9d-r13-disp8", bytes.fromhex("64 45 8b 4d 00"), 9, False, {13: 0x28}, 0x28),
    ("rax-r12-index-disp32", bytes.fromhex("64 4a 8b 04 25 28 00 00 00"), 0, True, {12: -0x30}, -8),
    ("rax-rcx-scaled-disp32", bytes.fromhex("64 48 8b 04 cd 10 00 00 00"), 0, True, {1: -3}, -8),
    ("rax-r12", bytes.fromhex("64 49 8b 04 24"), 0, True, {12: 0x10}, 0x10),
    ("rdx-rbp-disp32", bytes.fromhex("64 48 8b 95 f8 ff ff ff"), 2, True, {5: 0x30}, 0x28),
    ("r15-r14", bytes.fromhex("64 4d 8b 3e"), 15, True, {14: -8}, -8),
)
REGISTER_LOAD_FOLLOWERS = (
    ("rax-rbx-rip-follower", bytes.fromhex("64 48 8b 03"), 0, True, {3: -8}, -8, b"", 0x0fedcba987654321),
    ("edx-rbx-nop-follower", bytes.fromhex("64 8b 13"), 2, False, {3: 0x10}, 0x10, b"\x90", None),
)


def register_load_body(instruction, register, wide, address, offset, flags, follower=b"", rip_constant=None):
    saved = (3, 5, 6, 7, 12, 13, 14, 15)
    mask = (1 << 64) - 1
    code = bytearray()
    failures = []

    def emit(data):
        code.extend(data)

    def immediate(target, value):
        emit(bytes([0x48 | (target >> 3), 0xb8 | (target & 7)]) + struct.pack("<Q", value & mask))

    def check(slot):
        emit(bytes.fromhex("48 39 84 24") + struct.pack("<i", slot * 8) + bytes.fromhex("0f 85 00 00 00 00"))
        failures.append(len(code) - 4)

    for target in saved:
        emit((b"\x41" if target >= 8 else b"") + bytes([0x50 | (target & 7)]))
    emit(bytes.fromhex("48 8d a4 24 00 ff ff ff"))
    emit(fs_load(0, 0))
    for displacement, value in REGISTER_VALUES.items():
        immediate(1, value)
        emit(bytes([0x48, 0x89, 0x48, displacement & 255]))
    markers = [0x1020304050607000 + target * 0x101 for target in range(16)]
    expected = list(markers)
    for target in range(16):
        if target != 4:
            immediate(target, markers[target])
    for target, value in address.items():
        immediate(target, value)
        expected[target] = value & mask
    value = REGISTER_VALUES[offset]
    expected[register] = value if wide else value & 0xffffffff
    emit(b"\x68" + struct.pack("<I", flags) + b"\x9d")
    load_offset = len(code)
    emit(instruction)
    emit(follower)
    rip_offset = None
    if rip_constant is not None:
        rip_offset = len(code)
        emit(bytes.fromhex("48 8b 15 00 00 00 00"))
        expected[2] = rip_constant
    for target in range(16):
        emit(bytes([0x48 | (target >> 3) << 2, 0x89, 0x84 | (target & 7) << 3, 0x24]) + struct.pack("<i", target * 8))
    emit(bytes.fromhex("9c 8f 84 24 88 00 00 00"))
    for target in range(16):
        if target == 4:
            emit(bytes.fromhex("48 8d 04 24"))
        else:
            immediate(0, expected[target])
        check(target)
    immediate(0, flags)
    check(17)
    emit(bytes.fromhex("b8 2a 00 00 00 eb 05"))
    failure = len(code)
    emit(bytes.fromhex("b8 01 00 00 00 48 8d a4 24 00 01 00 00"))
    for target in reversed(saved):
        emit((b"\x41" if target >= 8 else b"") + bytes([0x58 | (target & 7)]))
    emit(b"\xc3")
    if rip_offset is not None:
        struct.pack_into("<i", code, rip_offset + 3, len(code) - rip_offset - 7)
        emit(struct.pack("<Q", rip_constant))
    for position in failures:
        struct.pack_into("<i", code, position, failure - position - 4)
    return load_offset, bytes(code)


def register_load_cases():
    cases = [case + (b"", None) for case in REGISTER_LOADS] + list(REGISTER_LOAD_FOLLOWERS)
    for name, instruction, register, wide, address, offset, follower, rip_constant in cases:
        for flags in (0x202, 0xad7):
            load_offset, body = register_load_body(instruction, register, wide, address, offset, flags, follower, rip_constant)
            moved = body[load_offset + len(instruction):load_offset + 5] if len(instruction) < 5 else b""
            yield f"register-load-{name}-{flags:x}", make_image("register", "unwind", body=body), 0x1240 + load_offset, instruction, register, wide, follower if rip_constant is None and follower else moved


def register_load_stub(instruction, register, wide):
    position = 1
    while instruction[position] != 0x8b:
        position += 1
    rex = instruction[position - 1] if 0x40 <= instruction[position - 1] <= 0x4f else 0
    address = bytes([0x48 | (rex & 3), 0x8d, instruction[position + 1] & 0xc7]) + instruction[position + 2:]
    move = (bytes([(0x48 if wide else 0x40) | (register >> 3) << 2]) if wide or register >= 8 else b"") + bytes([0x8b, 0x04 | (register & 7) << 3, 0x08])
    restore = {0: bytes.fromhex("48 8d 64 24 08 59"), 1: bytes.fromhex("58 48 8d 64 24 08")}.get(register, bytes.fromhex("58 59"))
    return bytes.fromhex("48 8d 64 24 80 51 50") + address + b"\x50", b"\x59" + move + restore + bytes.fromhex("48 8d a4 24 80 00 00 00")


def make_image(transfer, metadata, extent=None, body=None):
    image = fixture()
    image.extend(b"\x90" * 0x1000)
    struct.pack_into("<Q", image, 24, 0x1200)
    struct.pack_into("<I", image, 68, 6)
    image[0x1200:0x1300] = b"\x90" * 0x100
    target = 0x1240
    if transfer == "table":
        code = bytes.fromhex("31 c0 48 8d 0d") + struct.pack("<i", 0x780 - 0x1209)
        code += bytes.fromhex("48 63 04 81 48 01 c8 ff e0")
        struct.pack_into("<i", image, 0x780, target - 0x780)
    elif transfer == "register":
        code = bytes.fromhex("48 8d 05") + struct.pack("<i", target - 0x1207)
        code += bytes.fromhex("ff e0")
    elif transfer == "memory":
        code = bytes.fromhex("48 8d 0d") + struct.pack("<i", target - 0x1207)
        code += bytes.fromhex("48 89 4c 24 f8 ff 64 24 f8")
    else:
        raise ValueError(transfer)
    image[0x1200:0x1200 + len(code)] = code
    if body is None:
        body = TLS_LOAD + bytes.fromhex("8b 40 f0 c3")
    image[target:target + len(body)] = body
    image[0x1850:0x1850 + len(TLS_LOAD)] = TLS_LOAD
    struct.pack_into("<QQq", image, 0x700, 0x300, 8, 0x1200)
    struct.pack_into("<Q", image, 0x800, 42)
    struct.pack_into("<H", image, 56, 4 if metadata == "symbol" else 5)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 8, 16, 16)
    struct.pack_into("<IIQQQQQQ", image, 232 if metadata == "symbol" else 288, 1, 5, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)
    function_size = target + len(body) - 0x1200 if extent is None else extent
    if metadata == "unwind":
        struct.pack_into("<IIQQQQQQ", image, 232, 0x6474e550, 4, 0x980, 0x980, 0x980, 32, 32, 8)
        struct.pack_into("<II", image, 0x900, 12, 0)
        image[0x908:0x910] = bytes.fromhex("01 00 01 78 10 00 00 00")
        struct.pack_into("<IIQQ", image, 0x910, 20, 0x14, 0x1200, function_size)
        struct.pack_into("<BBBBQIQQ", image, 0x980, 1, 0, 3, 0, 0x900, 1, 0x1200, 0x910)
    elif metadata == "symbol":
        struct.pack_into("<IBBHQQ", image, 0x638, 0, 0x12, 0, 1, 0x1200, function_size)
        struct.pack_into("<IIII", image, 0x680, 1, 2, 1, 0)
        struct.pack_into("<qQqQ", image, 0x470, 4, 0x680, 0, 0)
        struct.pack_into("<QQ", image, 160, 0x90, 8)
        struct.pack_into("<Q", image, 152, 0x90)
    else:
        raise ValueError(metadata)
    return image


def pe_bytes_at(pe, rva, size):
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    count = struct.unpack_from("<H", pe, header + 6)[0]
    sections = header + 24 + struct.unpack_from("<H", pe, header + 20)[0]
    for index in range(count):
        offset = sections + index * 40
        address, length, position = struct.unpack_from("<III", pe, offset + 12)
        if address <= rva and rva + size <= address + length:
            return pe[position + rva - address:position + rva - address + size]
    raise AssertionError(f"Unmapped PE RVA {rva:#x}")


def add_alias(image, size, begin=0x1200):
    struct.pack_into("<IBBHQQ", image, 0x650, 0, 0x12, 0, 1, begin, size)
    struct.pack_into("<IIIII", image, 0x680, 1, 3, 1, 0, 0)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-coverage-") as directory:
        work = Path(directory)

        def convert(name, image, error=None, tls_address=0x1240, displacement=0, error_offset=None):
            source = work / (name + ".elf")
            output = source.with_suffix(".exe")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            if error is not None:
                assert result.returncode == 2 and error in result.stderr and not output.exists(), result
                if error_offset is not None:
                    assert f"(offset {error_offset:#x})" in result.stderr, (name, result.stderr)
                return
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            pe = output.read_bytes()
            patched_address = 0x10000 + tls_address
            patched = pe_bytes_at(pe, patched_address, 5)
            assert patched[0] == 0xe9, name
            if displacement != 0:
                stub_address = patched_address + 5 + struct.unpack_from("<i", patched, 1)[0]
                load = bytes.fromhex("48 8b 80") + struct.pack("<i", displacement)
                assert load in pe_bytes_at(pe, stub_address, 64), name
            assert pe_bytes_at(pe, 0x11850, len(TLS_LOAD)) == TLS_LOAD, name
            if os.name == "nt":
                executed = subprocess.run([str(output)], capture_output=True, timeout=30)
                assert executed.returncode == 42, (name, executed.returncode, executed.stderr)
            elif RUNS_ON_HOST:
                entry = 0x10000 + struct.unpack_from("<Q", image, 24)[0]
                executed = subprocess.run([sys.executable, str(RUNNER), str(output), hex(entry)], capture_output=True, timeout=30)
                assert executed.returncode == 42, (name, executed.returncode, executed.stderr)

        for metadata in ("unwind", "symbol"):
            for transfer in ("table", "register", "memory"):
                convert(metadata + "-" + transfer, make_image(transfer, metadata))
            convert(metadata + "-truncated", make_image("register", metadata, 0x46), "Code analysis:")
            convert(metadata + "-outside", make_image("register", metadata, 0x1000), "Code analysis: function exceeds executable segment")
        for first, second in ((0x47, 0x51), (0x51, 0x47), (0, 0x51), (0x51, 0)):
            convert(f"symbol-alias-{first:x}-{second:x}",
                    add_alias(make_image("register", "symbol", first), second))
        for first, second in ((0x47, 0x1000), (0x1000, 0x47)):
            convert(f"symbol-alias-outside-{first:x}-{second:x}",
                    add_alias(make_image("register", "symbol", first), second),
                    "Code analysis: function exceeds executable segment")
        alias_tail = add_alias(make_image("register", "symbol", 0x51), 0x60)
        alias_tail[0x1258:0x1260] = bytes.fromhex("64 8b 04 25 28 00 00 00")
        convert("symbol-alias-unreachable-tls-tail", alias_tail,
                "Unsupported Windows guest TLS instruction", error_offset=0x1258)
        for metadata in ("unwind", "symbol"):
            convert(f"branch-into-cut-tail-{metadata}",
                    make_image("register", metadata, 0x50, TLS_LOAD + bytes.fromhex("eb 00 8b 40 f0 c3")))
        convert("branch-into-undecodable-tail",
                make_image("register", "unwind", 0x50, TLS_LOAD + bytes.fromhex("eb 01 66 0f 78 c0 01 02 c3")),
                "Code analysis: branch into skipped range tail", error_offset=0x124f)
        split_body = TLS_LOAD + bytes.fromhex("eb 00 8b 40 f0 48 b9 00 00 00 65 2e 62 69 6e") + b"\xc3" * 8
        convert("function-begins-inside-instruction",
                add_alias(make_image("register", "symbol", 0x55, split_body), 0x0c, 0x1256))
        overlapping = make_image("register", "unwind")
        overlapping[0x1200:0x1205] = b"\xe9" + struct.pack("<i", 0x1245 - 0x1205)
        convert("overlapping-entry", overlapping, "Code analysis: overlapping instruction boundaries")
        fs_overlap = make_image("register", "unwind")
        fs_overlap[0x1209:0x1211] = bytes.fromhex("48 8b 04 25 64 00 00 00")
        fs_overlap[0x1211:0x1216] = b"\xe8" + struct.pack("<i", 0x120D - 0x1216)
        convert("overlapping-fs-prefix", fs_overlap, "Code analysis: overlapping instruction boundaries")
        fs66_overlap = make_image("register", "unwind")
        fs66_overlap[0x1209:0x1216] = bytes.fromhex("66 66 66 66 64 48 8b 04 25 78 56 00 00")
        fs66_overlap[0x1216:0x121B] = b"\xe8" + struct.pack("<i", 0x1211 - 0x121B)
        convert("overlapping-fs-prefix-66", fs66_overlap, "Code analysis: overlapping instruction boundaries")
        external = make_image("register", "unwind")
        external[0x1300:0x1310] = external[0x1240:0x1250]
        external[0x1240:0x1250] = b"\xe8" + struct.pack("<i", 0x1300 - 0x1245) + b"\xc3" + b"\x90" * 10
        convert("direct-call-from-indirect-block", external, tls_address=0x1300)
        address_taken = make_image("register", "unwind")
        address_taken[0x1300:0x1310] = address_taken[0x1240:0x1250]
        address_taken[0x1240:0x1250] = bytes.fromhex("48 8d 05") + struct.pack("<i", 0x1300 - 0x1247) + bytes.fromhex("ff d0 c3") + b"\x90" * 6
        convert("address-taken-tls-callback", address_taken, tls_address=0x1300)

        def convert_padded(name, image):
            source = work / (name + ".elf")
            output = source.with_suffix(".exe")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            return output.read_bytes()

        unreferenced = make_image("register", "unwind")
        unreferenced[0x18f0:0x1900] = b"\xcc" * 16
        unreferenced[0x1900:0x1900 + len(TLS_LOAD) + 4] = TLS_LOAD + bytes.fromhex("8b 40 f0 c3")
        pe = convert_padded("unreferenced-padded-function", unreferenced)
        assert pe_bytes_at(pe, 0x11900, 1)[0] == 0xe9, "unreferenced-padded-function"
        assert pe_bytes_at(pe, 0x11850, len(TLS_LOAD)) == TLS_LOAD, "unreferenced-padded-function"
        zero_filled = make_image("register", "unwind")
        zero_filled[0x18f0:0x1900] = b"\xcc" * 16
        zero_filled[0x1900:0x1910] = bytes(16)
        zero_filled[0x1910:0x1910 + len(TLS_LOAD)] = TLS_LOAD
        pe = convert_padded("zero-filled-after-padding", zero_filled)
        assert pe_bytes_at(pe, 0x11910, len(TLS_LOAD)) == TLS_LOAD, "zero-filled-after-padding"
        for register in range(16):
            offset, body = register_load(register)
            image = make_image("register", "unwind", body=body)
            if register == 4:
                convert("load-register-4", image, "Unsupported Windows guest TLS instruction")
            else:
                convert("load-register-" + str(register), image, tls_address=0x1240 + offset)
        for name, image, address in displacement_cases():
            displacement = struct.unpack_from("<i", image, address + 5)[0]
            convert(name, image, tls_address=address, displacement=displacement)
        for name, image, error in displacement_bounds_cases():
            convert(name, image, error, error_offset=0x1240)
        for name, opcode in ALU_READ.items():
            for register in (0, 1, 2, 3, 8, 13):
                for displacement in (0, 40, -8):
                    body = fs_alu(opcode, register, displacement) + b"\xc3"
                    case = f"alu-{name}-{register}-{displacement}"
                    image = make_image("register", "unwind", body=body)
                    source = work / (case + ".elf")
                    output = source.with_suffix(".exe")
                    source.write_bytes(image)
                    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                            capture_output=True, text=True, timeout=30)
                    assert result.returncode == 0, (case, result.stderr)
                    pe = output.read_bytes()
                    patched_address = 0x10000 + 0x1240
                    patched = pe_bytes_at(pe, patched_address, 5)
                    assert patched[0] == 0xe9, case
                    stub_address = patched_address + 5 + struct.unpack_from("<i", patched, 1)[0]
                    stub = pe_bytes_at(pe, stub_address, 96)
                    if register == 0:
                        expected = bytes.fromhex("48 8b 90") + struct.pack("<i", displacement) + bytes([0x58, 0x48, opcode, 0xc2])
                    elif register == 1:
                        expected = bytes.fromhex("48 8b 80") + struct.pack("<i", displacement) + bytes.fromhex("48 8b 4c 24 08") + bytes([0x48, opcode, 0xc8])
                    else:
                        expected = bytes.fromhex("48 8b 80") + struct.pack("<i", displacement) + bytes([0x48 | ((register >> 3) << 2), opcode, 0xc0 | ((register & 7) << 3)])
                    assert expected in stub, (case, expected.hex(), stub.hex())
        for name, image in alu_execution_cases():
            convert(name, image)
        for name, image, address, instruction, register, wide, moved in register_load_cases():
            convert(name, image, tls_address=address)
            pe = (work / (name + ".exe")).read_bytes()
            patched = pe_bytes_at(pe, 0x10000 + address, 5)
            stub = pe_bytes_at(pe, 0x10000 + address + 5 + struct.unpack_from("<i", patched, 1)[0], 96)
            head, tail = register_load_stub(instruction, register, wide)
            assert stub.startswith(head) and tail + moved in stub, (name, head.hex(), (tail + moved).hex(), stub.hex())
        for name, instruction in {
            "short-before-return": bytes.fromhex("64 48 8b 03 c3"),
            "short-before-call": bytes.fromhex("64 48 8b 03 e8 00 00 00 00 c3"),
            "short-before-tls": bytes.fromhex("64 48 8b 03 64 48 8b 03 c3"),
        }.items():
            convert(name, make_image("register", "unwind", body=instruction),
                    "Short guest TLS instruction is followed by an instruction that cannot move", error_offset=0x1244)
        rejected = {
            "rsp-displacement": fs_load(4, 40),
            "rsp-alu": fs_alu(0x33, 4, 40),
            "dword-load": bytes.fromhex("64 8b 04 25 28 00 00 00"),
            "gs-load": bytes.fromhex("65 48 8b 04 25 28 00 00 00"),
            "rex-b-load": bytes.fromhex("64 49 8b 04 25 28 00 00 00"),
            "rsp-base-load": bytes.fromhex("64 48 8b 04 24"),
            "rsp-register-address": bytes.fromhex("64 48 8b 20"),
            "rip-relative-load": bytes.fromhex("64 48 8b 05 00 00 00 00"),
            "word-register-address": bytes.fromhex("66 64 8b 00"),
            "register-address-store": bytes.fromhex("64 48 89 03"),
            "register-address-alu": bytes.fromhex("64 48 33 03"),
        }
        for name, instruction in rejected.items():
            convert(name, make_image("register", "unwind", body=instruction + b"\xc3"),
                    "Unsupported Windows guest TLS instruction")
        conflicting = make_image("register", "symbol")
        unwind = make_image("register", "unwind", 0x51)
        conflicting[0x900:0x9a0] = unwind[0x900:0x9a0]
        conflicting[232:344] = unwind[232:344]
        struct.pack_into("<H", conflicting, 56, 5)
        convert("conflicting-function-extents", conflicting, "Code analysis: conflicting function ranges")
    print("TLS function coverage integration tests passed")


if __name__ == "__main__":
    main()
