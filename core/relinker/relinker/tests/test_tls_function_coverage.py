import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


TLS_LOAD = bytes.fromhex("66 66 66 64 48 8b 04 25 00 00 00 00")


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


def add_alias(image, size):
    struct.pack_into("<IBBHQQ", image, 0x650, 0, 0x12, 0, 1, 0x1200, size)
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
        rejected = {
            "rsp-displacement": fs_load(4, 40),
            "dword-load": bytes.fromhex("64 8b 04 25 28 00 00 00"),
            "register-address": bytes.fromhex("64 48 8b 00"),
            "gs-load": bytes.fromhex("65 48 8b 04 25 28 00 00 00"),
            "rex-b-load": bytes.fromhex("64 49 8b 04 25 28 00 00 00"),
            "rex-x-load": bytes.fromhex("64 4a 8b 04 25 28 00 00 00"),
            "compare": bytes.fromhex("64 48 3b 04 25 28 00 00 00"),
            "subtract": bytes.fromhex("64 48 2b 04 25 28 00 00 00"),
            "add": bytes.fromhex("64 48 03 04 25 28 00 00 00"),
            "xor": bytes.fromhex("64 48 33 04 25 28 00 00 00"),
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
