from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_gnu_hash_analysis import name_hash
from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import dynamic_tags, file_offset
from test_windows_import_modules import dynamic, executable, provider


HASH = 0xA00
SYMBOLS = 0xC00
STRINGS = 0xD00


def module(names=("alpha", "bravo", "charlie"), bucket_count=3):
    image = provider(42)
    image.extend(bytes(0x1000 - len(image)))
    struct.pack_into("<QQ", image, 120 + 32, 0xA00, 0xA00)
    names = sorted(names, key=lambda name: name_hash(name + "#A#B") % bucket_count)
    strings = bytearray(b"\0")
    buckets = [0] * bucket_count
    chains = []
    bloom = 0
    for index, name in enumerate(names, 1):
        code = 0x400 + index * 16
        image[code:code + 6] = b"\xb8" + struct.pack("<I", 40 + index) + b"\xc3"
        struct.pack_into("<IBBHQQ", image, SYMBOLS + index * 24,
                         len(strings), 0x12, 0, 1, 0x1000 + index * 16, 6)
        qualified = name + "#A#B"
        strings.extend(qualified.encode() + b"\0")
        hashed = name_hash(qualified)
        bucket = hashed % bucket_count
        if buckets[bucket] == 0:
            buckets[bucket] = index
            if chains:
                chains[-1] |= 1
        chains.append(hashed & ~1)
        bloom |= (1 << (hashed % 64)) | (1 << ((hashed >> 5) % 64))
    if chains:
        chains[-1] |= 1
    struct.pack_into("<IIIIQ", image, HASH, bucket_count, 1, 1, 5, bloom)
    struct.pack_into("<" + "I" * (len(buckets) + len(chains)), image, HASH + 24,
                     *buckets, *chains)
    image[STRINGS:STRINGS + len(strings)] = strings
    tags = [(5, STRINGS + 0x1A00), (10, len(strings)), (6, SYMBOLS + 0x1A00), (11, 24),
            (0x6FFFFEF5, HASH + 0x1A00), (7, 0x2300), (8, 0), (9, 24)]
    dynamic(image, 0x600, 176, tags)
    return image, names


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-gnu-hash-") as directory:
        work = Path(directory)

        def convert(label, windows, image, names):
            case = work / label / str(windows)
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            guest = modules / "provider.prx"
            guest.write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(executable("provider.prx", names[-1] + "#A#B") if names else main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert guest.read_bytes() == image
            return result, output, case / "app0/sce_module/provider.prx.guest.prx"

        for label, requested, bucket_count in (("multiple-buckets", ("alpha", "bravo", "charlie"), 3),
                                                ("one-chain", ("alpha", "bravo", "charlie"), 1),
                                                ("one-symbol", ("alpha",), 3),
                                                ("no-hashed-symbols", (), 3)):
            image, names = module(requested, bucket_count)
            for windows in (False, True):
                result, output, artifact = convert(label, windows, image, names)
                assert result.returncode == 0, (label, windows, result.stdout, result.stderr)
                if windows and os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    assert run.returncode == (40 + len(names) if names else 42), (label, run.returncode, run.stdout, run.stderr)
                if not windows:
                    data = artifact.read_bytes()
                    tags = dynamic_tags(data)
                    symbol_count, = struct.unpack_from("<I", data, file_offset(data, tags[4]) + 4)
                    assert symbol_count == len(names) + 1, (label, symbol_count)

        for label, offset, value, error in (("zero-buckets", 0, 0, "Invalid GNU hash header"),
                                             ("zero-bloom", 8, 0, "Invalid GNU hash header"),
                                             ("zero-first-symbol", 4, 0, "Invalid GNU hash header"),
                                             ("large-bloom", 8, 0xFFFFFFFF, "Unmapped ELF address")):
            image, names = module()
            struct.pack_into("<I", image, HASH + offset, value)
            result, output, artifact = convert(label, False, image, names)
            assert result.returncode == 2 and error in result.stderr, (label, result.stderr)
            assert not output.exists() and not artifact.exists()

        image, names = module()
        struct.pack_into("<Q", image, 0x600 + 4 * 16 + 8, 0x29E0)
        struct.pack_into("<IIIIQII", image, 0xFE0, 1, 1, 1, 5, 0, 1, 2)
        result, output, artifact = convert("unterminated-chain", False, image, names)
        assert result.returncode == 2 and "Unmapped ELF address" in result.stderr, result.stderr
        assert not output.exists() and not artifact.exists()
    print("GNU hash guest module tests passed")


if __name__ == "__main__":
    main()
