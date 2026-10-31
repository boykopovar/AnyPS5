import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile

import test_windows_guest_gs_sse as sse
from test_windows_tls_protected_entries import make_image, pe_bytes_at
from test_windows_guest_gs_integration import executable_relocation, guest_relocation
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


NAMES = tuple(sse.FAMILIES)[:11]
PACKED = NAMES[:6]


def operation(name, store, reg, form, rex_w, wide, vex3):
    raw = sse.operation(name, store, reg, form, rex_w)
    if form in ("absolute", "rbx-base", "rsi-index"):
        raw = raw[:-1] + bytes(((raw[-1] & 0x38) | 4, {"absolute": 0x25, "rbx-base": 0x13, "rsi-index": 0x32}[form])) + (bytes(4) if form == "absolute" else b"")
    position = raw.index(0x0f)
    prefix = sse.FAMILIES[name][1 if store else 0]
    pp = {0: 0, 0x66: 1, 0xf3: 2, 0xf2: 3}[prefix]
    w = sse.FAMILIES[name][6] or rex_w
    r = 0 if reg >= 8 else 0x80
    head = b"\x65" + (b"\x67" if form in ("addr32", "eip") else b"")
    if vex3 or form == "sib" or w:
        head += bytes((0xc4, r | (0 if form == "sib" else 0x60) | 1, (0x80 if w else 0) | 0x78 | (4 if wide else 0) | pp))
    else:
        head += bytes((0xc5, r | 0x78 | (4 if wide else 0) | pp))
    return head + raw[position + 1:]


def cases(group):
    if group in ("packed128", "packed256", "scalar"):
        for name in PACKED if group != "scalar" else NAMES[6:]:
            for reg in (0, 4, 7, 8, 12, 15):
                for store in (False, True):
                    yield name, store, reg, 0, "direct", "normal", False, False, group == "packed256", reg % 2 == 0
        if group == "packed256":
            for reg in sorted(set(range(16)) - {0, 4, 7, 8, 12, 15}):
                for store in (False, True):
                    yield "movdqu", store, reg, 0, "direct", "normal", False, False, True, False
    if group == "address":
        for wide in (False, True):
            for name in ("movups", "movdqu"):
                for form in ("sib", "rsp", "addr32", "eip", "rip"):
                    for store in (False, True):
                        if store and form == "rip":
                            continue
                        yield name, store, 12, 0, form, "zero" if form == "rip" else "eip" if form == "eip" else "normal", False, False, wide, False
                for store in (False, True):
                    yield name, store, 4, -8 if not store else 1, "direct", "normal", False, False, wide, True
            for name in sse.ALIGNED:
                yield name, False, 12, -1, "direct", "shifted", False, False, wide, True
            for store in (False, True):
                yield "movdqu", store, 12, 0, "direct", "normal", True, False, wide, False
            for form in ("absolute", "rbx-base", "rsi-index"):
                for store in (False, True):
                    yield "movdqu", store, 12, 0, form, "normal", False, False, wide, True
        for name in NAMES:
            yield name, False, 4, 0, "direct", "normal", False, True, False, True
    if group == "faults":
        for name, wide in (("movups", False), ("movups", True), ("movdqu", True), ("movss", False), ("movsd", False), ("movq", False)):
            width = 32 if wide else sse.FAMILIES[name][4]
            for store in (False, True):
                for mode in (("write" if store else "read"), "twice", "skip", "edge"):
                    yield name, store, 12, 4096 - (width if mode == "edge" else width // 2), "direct", mode, False, False, wide, False
        for name in sse.ALIGNED:
            for wide in (False, True):
                for store in (False, True):
                    for mode in ("align", "align-base", "edge"):
                        yield name, store, 4, 4096 - (32 if wide else 16) if mode == "edge" else 1 if mode == "align" else 0, "direct", mode, False, False, wide, True
        for wide in (False, True):
            for store in (False, True):
                for mode in (("write" if store else "read"), "twice", "skip"):
                    yield "movdqu", store, 4, 0, "direct", mode, True, False, wide, False


def validate(values, case):
    name, store, reg, offset, form, mode, borrowed, rex_w, wide, _ = case
    first, vectors, headers, edge, snapshot = map(bytes.fromhex, values[:5])
    assert len(vectors) == 512 and len(headers) == 48, case
    flags = struct.unpack_from("<Q", headers)[0] & 0xcd5
    assert flags & 0x401 == 0x401 and (borrowed or flags == 0x445), (case, "flags")
    assert struct.unpack_from("<QQ", headers, 8) == (sse.RBX, sse.RSI), (case, "scratch")
    assert struct.unpack_from("<I", headers, 24)[0] == 0xdfb5, (case, "MXCSR")
    assert struct.unpack_from("<QQ", headers, 32) == (0x12345678, 0x7654321), (case, "red zone")
    expected = bytearray().join(sse.pattern(0x800 + index * 32, 32) for index in range(16))
    initial = bytearray(sse.pattern(0, 8192))
    if borrowed:
        struct.pack_into("<I", initial, 0, (struct.unpack_from("<I", initial)[0] + 1) & 0xffffffff)
    width = 32 if wide else 8 if name == "movd" and rex_w else sse.FAMILIES[name][4]
    address = 4096 if borrowed else 0 if mode in ("align", "align-base", "shifted") else offset
    source = sse.pattern(0 if form == "rip" else address, width)
    if mode != "skip":
        if store:
            initial[address:address + width] = expected[reg * 32:reg * 32 + width]
        else:
            expected[reg * 32:reg * 32 + 32] = source + bytes(32 - width)
    elif store:
        assert len(snapshot) == 16, case
        for index, value in enumerate(snapshot, 0xff0):
            allowed = (initial[index], expected[reg * 32 + index - address]) if address <= index < address + width else (initial[index],)
            assert value in allowed, (case, "partial store", index, value)
        initial[0xff0:0x1000] = snapshot
    assert vectors == expected, (case, "YMM state", vectors.hex(), expected.hex())
    assert first == initial[:64] and edge == initial[0xfe0:0x1020], (case, "memory footprint")
    assert int(values[5]) == (2 if mode == "twice" else int(mode in ("read", "write", "skip", "align", "align-base"))), (case, "fault count")
    normalized = bytearray(headers)
    struct.pack_into("<Q", normalized, 0, flags)
    return values[:2] + [normalized.hex()] + values[3:]


def guard_cases(relinker, work):
    number = 0
    def convert(image, module=None):
        nonlocal number
        case = work / f"guard-{number}"
        number += 1
        case.mkdir()
        if module is not None:
            (case / "prx").mkdir()
            (case / "prx" / "consumer.prx").write_bytes(module)
        source, output = case / "input.elf", case / "output.exe"
        source.write_bytes(image)
        run = subprocess.run([str(relinker.resolve()), "--windows", *(["--skip-sce-module"] if module is None else []), str(source), str(output)], capture_output=True, text=True, timeout=10)
        return run, output
    code = bytes.fromhex("65 c5 fc 10 24 25 00 00 00 00 c3")
    image = make_image("relative")
    image[0x210:0x210 + len(code)] = code
    run, output = convert(image)
    assert run.returncode == 2 and "GS trap would overwrite an executable entry point" in run.stderr and not output.exists(), run
    image = executable_relocation(8, 0x213)
    image[0x210:0x210 + len(code)] = code
    run, output = convert(image)
    assert run.returncode == 2 and "Relocation overlaps" in run.stderr and not output.exists(), run
    module = guest_relocation(8)
    module[0x400:0x400 + len(code)] = code
    run, output = convert(main_fixture(), module)
    assert run.returncode == 2 and "Relocation overlaps" in run.stderr and not output.exists(), run
    body = bytes.fromhex("65 ff 02 c5 fa 6f 87 00 10 00 00 c3")
    image = make_image("relative")
    image[0x210:0x210 + len(body)] = body
    struct.pack_into("<q", image, 0x728, 0x213)
    run, output = convert(image)
    assert run.returncode == 0, run
    pe = output.read_bytes()
    assert pe_bytes_at(pe, 0x10210, len(body)) == b"\x0f\x0b\x90" + body[3:]
    assert b"APS5GGS1" not in pe and b"APS5GGM1" not in pe and b"GuestGs" not in pe
    module = module_with_symbol(True)
    module[0x400:0x400 + len(body)] = body
    struct.pack_into("<Q", module, 0x668, 24)
    struct.pack_into("<QQq", module, 0x900, 0x2320, 8, 0x1003)
    run, output = convert(main_fixture(), module)
    assert run.returncode == 0, run
    pe = next(output.parent.rglob("consumer.prx.guest.prx")).read_bytes()
    assert pe_bytes_at(pe, 0x10000, len(body)) == b"\x0f\x0b\x90" + body[3:]
    assert b"APS5GGS1" not in pe and b"APS5GGM1" not in pe


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("relinker", type=Path)
    parser.add_argument("runner", type=Path)
    parser.add_argument("group", choices=("packed128", "packed256", "scalar", "address", "faults", "fallback"))
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--digest", type=Path)
    args = parser.parse_args()
    features = subprocess.run([str(args.runner.resolve()), "--features"], capture_output=True, text=True, timeout=5)
    assert features.returncode == 0, features
    if features.stdout.strip() != "avx":
        print("VEX GS execution requires host AVX with OS-enabled YMM state")
        return 77
    digest = []
    fallback_count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-vex-") as directory:
        work = Path(directory)
        source, output = work / "input.elf", work / "output.exe"
        for case in cases(args.group):
            name, store, reg, offset, form, mode, borrowed, rex_w, wide, vex3 = case
            def build_operation(n, st, r, f, w):
                return operation(n, st, r, f, w, wide, vex3)
            def build_follower(prefix, opcode, r, displacement):
                return bytes((0xc5, (0x7a if r >= 8 else 0xfa) | (4 if wide else 0), opcode, 0x87 | (r & 7) << 3)) + struct.pack("<i", displacement)
            body, site, resume = sse.program(name, store, reg, offset, form, True, borrowed, rex_w, build_operation, build_follower, 32 if wide else 16)
            if form in ("rbx-base", "rsi-index"):
                body = bytearray(body)
                struct.pack_into("<Q", body, body.index(bytes.fromhex("48 ba")) + 2, -(sse.RBX if form == "rbx-base" else sse.RSI) & ((1 << 64) - 1))
            source.write_bytes(sse.fixture(body))
            run = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            assert run.returncode == 0, (case, run)
            assert pe_bytes_at(output.read_bytes(), 0x11000 + site - (3 if borrowed else 0), 1) == b"\xe9", case
            bias = 0x1000 if args.native else 0x11000
            run = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), hex(bias), mode, hex(bias + (resume if mode == "eip" else site)), hex(bias + resume), "store" if store else "load", "xstate"], capture_output=True, text=True, timeout=10)
            assert run.returncode == 0, (case, run.returncode, run.stdout, run.stderr)
            digest.append((case, validate(run.stdout.split(), case)))
        if args.group == "fallback":
            guard_cases(args.relinker, work)
            for encoding in ("65 c5 f2 10 02", "65 c5 fe 10 02", "65 c5 f9 12 02", "65 c5 fc 58 02", "65 c5 fc 2b 02", "65 c4 e2 7d 8c 02", "65 c4 e3 79 20 02 00", "65 66 c5 fa 6f 02", "65 f0 c5 fa 6f 02", "65 48 c5 fa 6f 02", "65 c5 ff 6f 02", "65 c5 fd 6e 02", "65 c4 e8 79 10 02", "65 62 f1 7c 48 10 02"):
                op = bytes.fromhex(encoding)
                source.write_bytes(sse.fixture(op + b"\xc3"))
                run = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
                assert run.returncode == 0, (encoding, run)
                assert pe_bytes_at(output.read_bytes(), 0x11000, len(op)) == b"\x0f\x0b" + b"\x90" * (len(op) - 2), encoding
                if not args.native:
                    reached = subprocess.run([str(args.runner.resolve()), str(output), "0x11000", "normal"], capture_output=True, text=True, timeout=10)
                    assert reached.returncode != 0, (encoding, reached)
                source.write_bytes(sse.fixture(bytes.fromhex("31 c0 75 06 b8 2a 00 00 00 c3") + op + b"\xc3"))
                run = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
                assert run.returncode == 0, (encoding, run)
                skipped = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), hex(0x1000 if args.native else 0x11000), "normal"], capture_output=True, text=True, timeout=10)
                assert skipped.returncode == 0, (encoding, skipped)
                fallback_count += 1
        if args.digest:
            args.digest.write_text(json.dumps(digest))
    print(f"Guest GS VEX {args.group}: {len(digest)} state and {fallback_count} fallback cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
