import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from test_windows_tls_protected_entries import make_image, pe_bytes_at
from test_windows_guest_gs_integration import executable_relocation, guest_relocation
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


FAMILIES = {
    "movups": (0, 0, 0x10, 0x11, 16, "full", False),
    "movupd": (0x66, 0x66, 0x10, 0x11, 16, "full", False),
    "movaps": (0, 0, 0x28, 0x29, 16, "full", False),
    "movapd": (0x66, 0x66, 0x28, 0x29, 16, "full", False),
    "movdqu": (0xf3, 0xf3, 0x6f, 0x7f, 16, "full", False),
    "movdqa": (0x66, 0x66, 0x6f, 0x7f, 16, "full", False),
    "movss": (0xf3, 0xf3, 0x10, 0x11, 4, "zero", False),
    "movsd": (0xf2, 0xf2, 0x10, 0x11, 8, "zero", False),
    "movd": (0x66, 0x66, 0x6e, 0x7e, 4, "zero", False),
    "movq-rex": (0x66, 0x66, 0x6e, 0x7e, 8, "zero", True),
    "movq": (0xf3, 0x66, 0x7e, 0xd6, 8, "zero", False),
    "movlps": (0, 0, 0x12, 0x13, 8, "low", False),
    "movhps": (0, 0, 0x16, 0x17, 8, "high", False),
    "movlpd": (0x66, 0x66, 0x12, 0x13, 8, "low", False),
    "movhpd": (0x66, 0x66, 0x16, 0x17, 8, "high", False),
}
ALIGNED = ("movaps", "movapd", "movdqa")
RBX = 0x1234567812345678
RSI = 0x2234567822345678


def pattern(offset, size):
    payload = struct.pack("<IIII", 0x7f800001, 0x80000000, 1, 0xff800000)
    return bytes(payload[i] if 0 <= i < 16 else ((i + 32768) * 37 + ((i + 32768) >> 4) * 11 + 0x53) & 255 for i in range(offset, offset + size))


def fixture(body):
    image = make_image()
    image.extend(bytes(0x4000 - len(image)))
    image[0x1000:0x1000 + len(body)] = body
    struct.pack_into("<Q", image, 24, 0x1000)
    struct.pack_into("<QQ", image, 96, len(image), len(image))
    struct.pack_into("<I", image, 176, 0)
    struct.pack_into("<qQ", image, 0x440, 0, 0)
    return image


def memory_move(prefix, opcode, reg, displacement, avx=False):
    if avx:
        head = bytes((0xc5, 0x7e if reg >= 8 else 0xfe))
    else:
        head = bytes((prefix,)) if prefix else b""
        if reg >= 8:
            head += b"\x44"
        head += b"\x0f"
    return head + bytes((opcode, 0x87 | (reg & 7) << 3)) + struct.pack("<i", displacement)


def operation(name, store, reg, form="direct", rex_w=False):
    load_prefix, store_prefix, load, save, _, _, wide = FAMILIES[name]
    prefix = store_prefix if store else load_prefix
    rex = 0x40 | (4 if reg >= 8 else 0) | (8 if wide or rex_w else 0)
    head = b"\x65" + (bytes((prefix,)) if prefix else b"")
    if form in ("addr32", "eip"):
        head += b"\x67"
    if form == "sib":
        rex |= 3
    if rex != 0x40:
        head += bytes((rex,))
    head += bytes((0x0f, save if store else load))
    field = (reg & 7) << 3
    if form == "sib":
        return head + bytes((0x44 | field, 0x88, 0xe0))
    if form == "rsp":
        return head + bytes((0x04 | field, 0x0c))
    if form in ("rip", "eip"):
        return head + bytes((0x05 | field,)) + bytes(4)
    return head + bytes((0x02 | field,))


def program(name, store, reg, offset, form, avx, borrowed=False, rex_w=False, operation_builder=operation, follower_builder=memory_move, payload_size=16):
    code = bytearray(bytes.fromhex("53 56 48 bb") + struct.pack("<Q", RBX) + bytes.fromhex("48 be") + struct.pack("<Q", RSI))
    code += bytes.fromhex("48 83 ec 10 0f ae 5c 24 08 0f ae 97 40 04 00 00")
    for index in range(16):
        code += memory_move(0xf3, 0x6f, index, 0x800 + index * 32, avx)
    code += bytes.fromhex("48 ba") + struct.pack("<Q", (offset + (0x100000000 if form == "addr32" else 0)) & ((1 << 64) - 1))
    if form == "sib":
        code += bytes.fromhex("41 b8 00 00 00 00 41 b9 08 00 00 00")
    if form == "rsp":
        code += bytes.fromhex("48 89 e1 48 f7 d9 48 81 c1") + struct.pack("<i", offset)
    code += bytes.fromhex("48 c7 44 24 80 78 56 34 12 48 c7 44 24 f8 21 43 65 07 31 c0 f9 fd")
    site = len(code)
    if borrowed:
        op = bytes.fromhex("65 ff 02") + follower_builder(0xf3, 0x7f if store else 0x6f, reg, 4096)
    else:
        op = operation_builder(name, store, reg, form, rex_w)
    code += op
    resume = len(code)
    if form != "borrow-sse":
        code += b"\x90"
    code += memory_move(0xf3, 0x7f, 0, 0x200)
    for index in range(16):
        code += memory_move(0xf3, 0x7f, index, 0x200 + index * 32, avx)
    code += bytes.fromhex("48 8b 44 24 80 48 89 87 20 04 00 00 48 8b 44 24 f8 48 89 87 28 04 00 00 9c 58 48 89 87 00 04 00 00 48 89 9f 08 04 00 00 48 89 b7 10 04 00 00 0f ae 9f 18 04 00 00 0f ae 54 24 08 48 83 c4 10 5e 5b fc b8 2a 00 00 00")
    if avx:
        code += bytes.fromhex("c5 f8 77")
    code += b"\xc3"
    if form == "rip":
        struct.pack_into("<i", code, site + len(op) - 4, len(code) - site - len(op))
        code += pattern(0, payload_size)
    return code, site + (3 if borrowed else 0), resume


def cases(group):
    registers = (0, 4, 7, 8, 12, 15)
    if group in ("packed", "scalar", "lanes"):
        names = list(FAMILIES)[:6] if group == "packed" else list(FAMILIES)[6:11] if group == "scalar" else list(FAMILIES)[11:]
        for name in names:
            for reg in registers:
                for store in (False, True):
                    yield name, store, reg, 0, "direct", "normal", False, False
        if group == "packed":
            for reg in sorted(set(range(16)) - set(registers)):
                for store in (False, True):
                    yield "movdqu", store, reg, 0, "direct", "normal", False, False
    if group == "address":
        for form in ("sib", "rsp", "addr32", "eip", "rip", "borrow-sse"):
            for name in ("movups", "movdqu", "movss", "movsd", "movq", "movhpd"):
                yield name, False, 4, 0, form, "zero" if form == "rip" else "eip" if form == "eip" else "normal", False, False
                if form not in ("rip", "borrow-sse"):
                    yield name, True, 12, 0, form, "eip" if form == "eip" else "normal", False, False
        for name in ("movups", "movdqu", "movss", "movsd", "movlps", "movhps"):
            yield name, False, 12, -8, "direct", "normal", False, False
            yield name, True, 12, 1, "direct", "normal", False, False
            yield name, False, 12, 0, "direct", "normal", False, True
        for name in ALIGNED:
            yield name, False, 12, -1, "direct", "shifted", False, False
    if group == "faults":
        for name in ("movups", "movdqu", "movss", "movsd", "movq", "movhpd"):
            width = FAMILIES[name][4]
            for store in (False, True):
                for mode in (("write" if store else "read"), "twice", "skip"):
                    yield name, store, 12, 4096 - width // 2, "direct", mode, False, False
                yield name, store, 4, 4096 - width, "direct", "edge", False, False
        for name in ALIGNED:
            for store in (False, True):
                yield name, store, 4, 1, "direct", "align", False, False
                yield name, store, 12, 0, "direct", "align-base", False, False
                yield name, store, 4, 4080, "direct", "edge", False, False
        for store in (False, True):
            for mode in (("write" if store else "read"), "twice"):
                yield "movdqu", store, 4, 0, "direct", mode, True, False


def validate(values, case, avx):
    name, store, reg, offset, form, mode, borrowed, _ = case
    first, vectors, headers, edge = map(bytes.fromhex, values[:4])
    assert len(first) == 64 and len(vectors) == 512 and len(headers) == 48 and len(edge) == 64, values
    assert struct.unpack_from("<Q", headers)[0] & 0x401 == 0x401, (case, "flags")
    assert struct.unpack_from("<QQ", headers, 8) == (RBX, RSI), (case, "scratch registers")
    assert struct.unpack_from("<I", headers, 24)[0] == 0xdfb5, (case, "MXCSR")
    assert struct.unpack_from("<QQ", headers, 32) == (0x12345678, 0x7654321), (case, "red zone")
    expected = bytearray(pattern(0x200, 512))
    for index in range(16):
        expected[index * 32:index * 32 + (32 if avx else 16)] = pattern(0x800 + index * 32, 32 if avx else 16)
    initial = bytearray(pattern(0, 8192))
    if borrowed:
        struct.pack_into("<I", initial, 0, (struct.unpack_from("<I", initial)[0] + 1) & 0xffffffff)
    _, _, _, _, width, kind, _ = FAMILIES[name]
    address = 4096 if borrowed else 0 if mode in ("align", "align-base", "shifted") else offset
    source = pattern(address, width) if address < 0 else initial[address:address + width]
    if form == "rip":
        source = pattern(0, width)
    if mode != "skip":
        start = reg * 32 + (8 if kind == "high" else 0)
        if store:
            initial[address:address + width] = expected[start:start + width]
        else:
            if kind in ("zero", "full"):
                expected[reg * 32:reg * 32 + 16] = bytes(16)
            expected[start:start + width] = source
    elif store:
        snapshot = bytes.fromhex(values[4])
        assert len(snapshot) == 16, (case, "pre-repair snapshot")
        source_start = reg * 32 + (8 if kind == "high" else 0)
        for index, value in enumerate(snapshot, 0xff0):
            allowed = (initial[index], expected[source_start + index - address]) if address <= index < address + width else (initial[index],)
            assert value in allowed, (case, "partial faulting store footprint", index, value)
        initial[0xff0:0x1000] = snapshot
    assert vectors == expected, (case, "XMM/YMM state", vectors.hex(), expected.hex())
    assert first == initial[:64] and edge == initial[0xfe0:0x1020], (case, "store footprint", first.hex(), edge.hex())
    assert int(values[5]) == (2 if mode == "twice" else 1 if mode in ("read", "write", "skip", "align", "align-base") else 0), (case, "fault count", values[5])
    flags = struct.unpack_from("<Q", headers)[0] & 0xcd5
    if not borrowed:
        assert flags == 0x445, (case, "arithmetic/direction flags", flags)
    normalized = bytearray(headers)
    struct.pack_into("<Q", normalized, 0, flags)
    return values[:2] + [normalized.hex()] + values[3:]


def borrowed_fallbacks(relinker, work):
    number = 0

    def convert(image, module=None):
        nonlocal number
        case = work / f"borrowed-{number}"
        number += 1
        case.mkdir()
        if module is not None:
            (case / "prx").mkdir()
            (case / "prx" / "consumer.prx").write_bytes(module)
        source = case / "input.elf"
        output = case / "output.exe"
        source.write_bytes(image)
        run = subprocess.run([str(relinker.resolve()), "--windows", *(["--skip-sce-module"] if module is None else []), str(source), str(output)], capture_output=True, text=True, timeout=10)
        assert run.returncode == 0, run
        if module is not None:
            assert b"GuestGs" not in output.read_bytes()
            output = next(case.rglob("consumer.prx.guest.prx"))
        return output.read_bytes()

    def check(pe, rva, body):
        assert pe_bytes_at(pe, rva, len(body)) == b"\x0f\x0b\x90" + body[3:]
        assert b"APS5GGS1" not in pe and b"APS5GGM1" not in pe and b"GuestGs" not in pe

    for store in (False, True):
        body = bytes.fromhex("65 ff 02") + memory_move(0xf3, 0x7f if store else 0x6f, 12, 4096) + b"\xc3"
        image = make_image()
        image[0x210:0x210 + len(body)] = body
        supported = convert(image)
        assert pe_bytes_at(supported, 0x10210, 1) == b"\xe9" and b"APS5GGM1" in supported
        for delta in (3, 4, 6):
            image = make_image("relative")
            image[0x210:0x210 + len(body)] = body
            struct.pack_into("<q", image, 0x728, 0x210 + delta)
            check(convert(image), 0x10210, body)
            image = make_image()
            image[0x210:0x210 + len(body)] = body
            image[0x200:0x204] = bytes((0x75, 0x10 + delta - 2, 0xeb, 0x10 - 4))
            check(convert(image), 0x10210, body)
            module = module_with_symbol(True)
            module[0x400:0x400 + len(body)] = body
            struct.pack_into("<Q", module, 0x668, 24)
            struct.pack_into("<QQq", module, 0x900, 0x2320, 8, 0x1000 + delta)
            check(convert(main_fixture(), module), 0x10000, body)
        overlapping = bytearray(body)
        overlapping[8:12] = bytes.fromhex("65 8b 02 90")
        image = make_image("relative")
        image[0x210:0x210 + len(overlapping)] = overlapping
        struct.pack_into("<q", image, 0x728, 0x218)
        check(convert(image), 0x10210, overlapping)
        module = module_with_symbol(True)
        module[0x400:0x400 + len(overlapping)] = overlapping
        struct.pack_into("<Q", module, 0x668, 24)
        struct.pack_into("<QQq", module, 0x900, 0x2320, 8, 0x1008)
        check(convert(main_fixture(), module), 0x10000, overlapping)
        for module in (False, True):
            image = guest_relocation(8) if module else executable_relocation(8, 0x219)
            site = 0x400 if module else 0x210
            image[site:site + len(body)] = body
            if module:
                struct.pack_into("<Q", image, 0x900, 0x1009)
            pe = convert(main_fixture(), image) if module else convert(image)
            image[site:site + 3] = b"\x90" * 3
            baseline = convert(main_fixture(), image) if module else convert(image)
            rva = 0x10000 if module else 0x10210
            assert pe_bytes_at(pe, rva, 3) == b"\x0f\x0b\x90"
            assert pe_bytes_at(pe, rva + 3, len(body) - 3) == pe_bytes_at(baseline, rva + 3, len(body) - 3)
            assert b"APS5GGS1" not in pe and b"APS5GGM1" not in pe and b"GuestGs" not in pe


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("relinker", type=Path)
    parser.add_argument("runner", type=Path)
    parser.add_argument("group", choices=("packed", "scalar", "lanes", "address", "faults", "fallback"))
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--digest", type=Path)
    args = parser.parse_args()
    features = subprocess.run([str(args.runner.resolve()), "--features"], capture_output=True, text=True, timeout=5)
    assert features.returncode == 0, features
    avx = features.stdout.strip() == "avx"
    digest = []
    fallback_count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-sse-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        output = work / "output.exe"
        for case in cases(args.group):
            name, store, reg, offset, form, mode, borrowed, rex_w = case
            body, site, resume = program(name, store, reg, offset, form, avx, borrowed, rex_w)
            source.write_bytes(fixture(body))
            convert = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            assert convert.returncode == 0 and pe_bytes_at(output.read_bytes(), 0x11000 + site - (3 if borrowed else 0), 1) == b"\xe9", (case, convert)
            bias = 0x1000 if args.native else 0x11000
            run = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), hex(bias), mode, hex(bias + (resume if mode == "eip" else site)), hex(bias + resume), "store" if store else "load"], capture_output=True, text=True, timeout=10)
            assert run.returncode == 0, (case, run.returncode, run.stdout, run.stderr)
            digest.append((case, validate(run.stdout.split(), case, avx)))
        if args.group == "fallback":
            borrowed_fallbacks(args.relinker, work)
            for encoding in ("65 f0 0f 10 02", "65 66 f3 0f 10 02", "65 f2 0f 6f 02", "65 0f 6f 02", "65 0f 58 02", "65 66 0f 38 10 02", "65 66 0f 3a 20 02 00", "65 0f 2b 02", "65 f2 0f 12 02", "65 f3 0f 12 02", "65 0f f7 c0", "65 c5 fa 58 02", "65 62 f1 7c 48 10 02"):
                op = bytes.fromhex(encoding)
                source.write_bytes(fixture(op + b"\xc3"))
                convert = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
                assert convert.returncode == 0, (encoding, convert)
                assert pe_bytes_at(output.read_bytes(), 0x11000, len(op)) == b"\x0f\x0b" + b"\x90" * (len(op) - 2), encoding
                if not args.native:
                    reached = subprocess.run([str(args.runner.resolve()), str(output), "0x11000", "normal"], capture_output=True, text=True, timeout=10)
                    assert reached.returncode != 0, (encoding, "fallback did not fault", reached)
                source.write_bytes(fixture(bytes.fromhex("31 c0 75 06 b8 2a 00 00 00 c3") + op + b"\xc3"))
                convert = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
                assert convert.returncode == 0, (encoding, convert)
                skipped = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), "0x1000" if args.native else "0x11000", "normal"], capture_output=True, text=True, timeout=10)
                assert skipped.returncode == 0, (encoding, "unused fallback prevented execution", skipped)
                fallback_count += 1
        if args.digest:
            args.digest.write_text(json.dumps({"avx": avx, "cases": digest}))
    print(f"Guest GS SSE {args.group}: {len(digest)} native-state and {fallback_count} fallback cases passed; upper YMM check: {avx}")


if __name__ == "__main__":
    main()
