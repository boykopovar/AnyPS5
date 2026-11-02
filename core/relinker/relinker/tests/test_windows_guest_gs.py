import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_windows_tls_protected_entries import make_image, pe_bytes_at


INITIAL = 0x1122334455667788
RBX = 0x1234567812345678
RSI = 0x2234567822345678


def program(instruction, setup=b""):
    return (bytes.fromhex("53 56 48 bb") + struct.pack("<Q", RBX)
            + bytes.fromhex("48 be") + struct.pack("<Q", RSI)
            + bytes.fromhex("48 b8") + struct.pack("<Q", 0x8877665544332211)
            + bytes.fromhex("b9 08 00 00 00 31 d2 f9 48 c7 44 24 80 78 56 34 12 48 c7 44 24 f8 21 43 65 07") + setup + bytes.fromhex("f3 0f 6f 47 f0 f3 44 0f 6f 47 f0") + instruction
            + bytes.fromhex("48 8b 54 24 80 48 89 57 28 48 8b 54 24 f8 48 89 57 30 9c 5a 48 89 57 10 48 89 5f 18 48 89 77 20 f3 0f 7f 47 38 f3 44 0f 7f 47 48 5e 5b fc c3"))


def fixture(body, extra_entry=None):
    image = make_image()
    image[0x200:0x300] = b"\x90" * 0x100
    image[0x210:0x210 + len(body)] = body
    struct.pack_into("<Q", image, 24, 0x210)
    struct.pack_into("<I", image, 176, 0)
    if extra_entry is None:
        struct.pack_into("<qQ", image, 0x400 + 4 * 16, 0, 0)
    else:
        struct.pack_into("<q", image, 0x710, extra_entry)
    return image


def cases():
    yield "load", "65 48 8b 04 25 00 00 00 00", b"", INITIAL, INITIAL, 0x45
    yield "store", "65 48 89 04 25 00 00 00 00", b"", 0x8877665544332211, 0x8877665544332211, 0x45
    yield "load32", "65 8b 04 25 00 00 00 00", b"", 0x55667788, INITIAL, 0x45
    yield "load16", "65 66 8b 04 25 00 00 00 00", b"", 0x8877665544337788, INITIAL, 0x45
    yield "load-high-byte", "65 8a 24 25 00 00 00 00", b"", 0x8877665544338811, INITIAL, 0x45
    yield "direction-flag", "65 48 8b 04 25 00 00 00 00", b"\xfd", INITIAL, INITIAL, 0x445
    yield "moffs64", "65 48 a1 00 00 00 00 00 00 00 00", b"", INITIAL, INITIAL, 0x45
    yield "moffs32", "65 67 a1 00 00 00 00", b"", 0x55667788, INITIAL, 0x45
    for rex in (0x44, 0x47, 0x4c, 0x4f):
        for opcode in (0xa0, 0xa1, 0xa2, 0xa3):
            width = 1 if opcode % 2 == 0 else 8 if rex & 8 else 4
            mask = (1 << (width * 8)) - 1
            accumulator = 0x8877665544332211
            value = accumulator & ~mask | INITIAL & mask if opcode < 0xa2 else accumulator
            if opcode == 0xa1 and width == 4:
                value &= mask
            memory = INITIAL if opcode < 0xa2 else INITIAL & ~mask | accumulator & mask
            yield f"moffs-rex-{rex:x}-{opcode:x}", f"65 {rex:02x} {opcode:02x} 00 00 00 00 00 00 00 00", b"", value, memory, 0x45
    yield "moffs16-rex-r-load", "65 66 44 a1 00 00 00 00 00 00 00 00", b"", 0x8877665544337788, INITIAL, 0x45
    yield "moffs16-rex-r-store", "65 66 44 a3 00 00 00 00 00 00 00 00", b"", 0x8877665544332211, 0x1122334455662211, 0x45
    yield "sib-negative", "65 48 8b 44 88 e0", bytes.fromhex("31 c0 f9"), INITIAL, INITIAL, 0x45
    yield "address32-wrap", "65 67 48 8b 04 08", bytes.fromhex("48 b8 f8 ff ff ff ff ff ff ff"), INITIAL, INITIAL, 0x45
    yield "eip32-load", "65 67 48 8b 05 00 00 00 00", b"", INITIAL, INITIAL, 0x45
    yield "eip32-unaligned", "65 67 48 8b 05 01 00 00 00", b"", 0x8811223344556677, INITIAL, 0x45
    yield "eip32-negative", "65 67 48 8b 05 f8 ff ff ff", b"", INITIAL, INITIAL, 0x45
    yield "eip32-store", "65 67 48 89 05 00 00 00 00", b"", 0x8877665544332211, 0x8877665544332211, 0x45
    yield "rsp", "65 48 8b 04 0c", bytes.fromhex("48 89 e1 48 f7 d9 31 d2 f9"), INITIAL, INITIAL, 0x45
    yield "extended-sib", "65 4b 8b 44 c8 c0", bytes.fromhex("41 b8 00 00 00 00 41 b9 08 00 00 00"), INITIAL, INITIAL, 0x45
    yield "store-rbx", "65 48 89 1c 25 00 00 00 00", b"", 0x8877665544332211, RBX, 0x45
    yield "store-bh", "65 88 3c 25 00 00 00 00", b"", 0x8877665544332211, INITIAL & ~255 | 0x56, 0x45
    yield "load-r8", "65 4c 8b 04 25 00 00 00 00 4c 89 c0", b"", INITIAL, INITIAL, 0x45
    yield "unaligned", "65 48 8b 04 25 01 00 00 00", b"", 0x8811223344556677, INITIAL, 0x45
    yield "negative", "65 48 8b 04 25 f8 ff ff ff", b"", INITIAL, INITIAL, 0x45
    yield "rip", "65 48 8b 05 00 00 00 00", b"", 0x9988776655443322, INITIAL, 0x45
    yield "xchg", "65 48 87 04 25 00 00 00 00", b"", INITIAL, 0x8877665544332211, 0x45
    yield "lock-xadd", "65 f0 48 0f c1 04 25 00 00 00 00", b"", INITIAL, 0x9999999999999999, None
    yield "lock-cmpxchg-success", "65 f0 48 0f b1 0c 25 00 00 00 00", bytes.fromhex("48 b8") + struct.pack("<Q", INITIAL), INITIAL, 8, 0x44
    yield "lock-cmpxchg-failure", "65 f0 48 0f b1 0c 25 00 00 00 00", b"", INITIAL, INITIAL, None
    yield "immediate", "65 48 c7 04 25 00 00 00 00 ff ff ff ff", b"", 0x8877665544332211, 0xffffffffffffffff, 0x45
    yield "adc", "65 48 13 04 25 00 00 00 00", b"", 0x999999999999999a, INITIAL, None
    yield "setcc", "65 0f 92 04 25 00 00 00 00", b"", 0x8877665544332211, INITIAL & ~255 | 1, 0x45
    yield "cmov", "65 48 0f 42 04 25 00 00 00 00", b"", INITIAL, INITIAL, 0x45
    yield "movzx", "65 0f b6 04 25 00 00 00 00", b"", 0x88, INITIAL, 0x45
    yield "shift", "65 48 d1 24 25 00 00 00 00", b"", 0x8877665544332211, INITIAL << 1, None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("relinker", type=Path)
    parser.add_argument("runner", type=Path)
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--runtime", action="store_true")
    parser.add_argument("--digest", type=Path)
    args = parser.parse_args()
    digest = []
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-gs-") as directory:
        work = Path(directory)
        for name, encoding, setup, result, memory, flags in cases():
            body = program(bytes.fromhex(encoding), setup)
            if name == "rip":
                position = body.index(bytes.fromhex(encoding))
                body = bytearray(body)
                struct.pack_into("<i", body, position + 4, len(body) - position - 8)
                body += struct.pack("<Q", 0x9988776655443322)
            source = work / (name + ".elf")
            output = source.with_suffix(".exe")
            source.write_bytes(fixture(body))
            converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            assert converted.returncode == 0, (name, converted.stderr)
            mode = ["zero"] if name == "rip" else []
            if name.startswith("eip32-"):
                resume = (0x210 if args.native else 0x10210) + body.index(bytes.fromhex(encoding)) + len(bytes.fromhex(encoding))
                mode = ["eip", hex(resume)]
            run = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), "0x210" if args.native else "0x10210"] + mode, capture_output=True, text=True, timeout=10)
            assert run.returncode == 0, (name, run.returncode, run.stdout, run.stderr)
            values = [int(value, 16) for value in run.stdout.split()]
            assert values[0] == result and values[5] == memory, (name, run.stdout)
            assert values[12:16] == [INITIAL] * 4, (name, "vector registers", run.stdout)
            assert values[10:12] == [0x12345678, 0x07654321], (name, "red zone", run.stdout)
            assert values[8:10] == [RBX, RSI], (name, "scratch register", run.stdout)
            if flags is not None:
                assert values[7] & 0xcd5 == flags, (name, "flags", run.stdout)
            if sys.platform == "win32" and not args.runtime and name in ("load", "store", "lock-xadd"):
                site = 0x10210 + body.index(bytes.fromhex(encoding))
                retry = subprocess.run([str(args.runner.resolve()), str(output), "0x10210", "fault", hex(site)], capture_output=True, text=True, timeout=10)
                assert retry.returncode == 0 and retry.stdout == run.stdout, (name, "fault retry", retry)
            values[7] &= 0xcd5
            digest.append((name, values))
        source = work / "atomic.elf"
        output = source.with_suffix(".exe")
        source.write_bytes(fixture(bytes.fromhex("65 f0 48 ff 04 25 00 00 00 00 c3")))
        converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
        assert converted.returncode == 0, converted.stderr
        atomic = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), "0x210" if args.native else "0x10210", "atomic"], capture_output=True, text=True, timeout=10)
        assert atomic.returncode == 0 and atomic.stdout.strip() == "atomic 512", atomic
        for root in ("relative", "symbol", "lea", "initializer", "access-start"):
            image = make_image(root)
            image[0x210:0x21a] = bytes.fromhex("65 48 8b 04 25 28 00 00 00 c3")
            source.write_bytes(image)
            output.unlink(missing_ok=True)
            converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            if root == "access-start":
                assert converted.returncode == 0, (root, converted.stderr)
                patched = pe_bytes_at(output.read_bytes(), 0x10210, 10)
                assert patched[0] == 0xe9 and patched[5:] == image[0x215:0x21a]
            else:
                assert converted.returncode == 2 and "GS trap would overwrite an executable entry point" in converted.stderr and not output.exists(), (root, converted)
        image = make_image("relative")
        image[0x210:0x21a] = bytes.fromhex("65 48 8b 04 25 28 00 00 00 c3")
        struct.pack_into("<q", image, 0x728, 0x211)
        source.write_bytes(image)
        output.unlink(missing_ok=True)
        converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
        assert converted.returncode == 2 and "GS trap would overwrite an executable entry point" in converted.stderr and not output.exists(), converted
        struct.pack_into("<q", image, 0x728, 0x215)
        source.write_bytes(image)
        converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
        assert converted.returncode == 0, converted
        assert pe_bytes_at(output.read_bytes(), 0x10215, 5) == image[0x215:0x21a]
        output.unlink()
        for encoding in ("65 48 8b 00", "65 48 8b 24 25 00 00 00 00", "65 a4", "65 0f 10 00", "65 c4 c1 f9 6e 00", "65 c5 f9 6e 00", "65 62 f1 7c 48 10 00", "65 8f e8 78 c0 44 24 f0 01", "65 66 0f f7 c1", "65 0f 01 c8", "f3 48 0f ae c8", "f3 48 0f ae d8", "8e e8", "8c e8", "0f a8", "0f a9"):
            instruction = bytes.fromhex(encoding)
            source.write_bytes(fixture(instruction + b"\xc3"))
            converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            assert converted.returncode == 0, (encoding, converted)
            patched = pe_bytes_at(output.read_bytes(), 0x10210, len(instruction) + 1)
            assert patched == b"\x0f\x0b" + b"\x90" * (len(instruction) - 2) + b"\xc3", (encoding, patched.hex())
            image = fixture(bytes((0xeb, len(instruction))) + instruction + program(bytes.fromhex("65 48 8b 04 25 00 00 00 00")), extra_entry=0x212)
            source.write_bytes(image)
            converted = subprocess.run([str(args.relinker.resolve()), "--skip-sce-module", "--windows", str(source), str(output)], capture_output=True, text=True, timeout=10)
            assert converted.returncode == 0, (encoding, converted)
            assert pe_bytes_at(output.read_bytes(), 0x10212, len(instruction)) == patched[:-1], encoding
            run = subprocess.run([str(args.runner.resolve()), str(source if args.native else output), "0x210" if args.native else "0x10210"], capture_output=True, text=True, timeout=10)
            assert run.returncode == 0 and int(run.stdout.split()[0], 16) == INITIAL, (encoding, run)
        if args.digest:
            import json
            args.digest.write_text(json.dumps(digest))
    print(f"Guest GS execution passed: {len(digest)} cases")


if __name__ == "__main__":
    main()
