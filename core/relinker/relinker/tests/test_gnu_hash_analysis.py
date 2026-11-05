from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_jmprel_table_bounds import fixture, DT_OS_SYMTABSZ, DT_STRTAB, DT_STRSZ


DT_GNU_HASH = 0x6FFFFEF5
HASH = 0x800
SYMBOLS = 0x620
STRINGS = 0x900


def name_hash(name):
    value = 5381
    for byte in name.encode():
        value = (value * 33 + byte) & 0xFFFFFFFF
    return value


def image(names=("alpha", "bravo", "charlie"), bucket_count=3):
    elf = fixture(os_tags=False)
    struct.pack_into("<H", elf, 56, 2)
    names = sorted(names, key=lambda name: name_hash(name) % bucket_count)
    strings = bytearray(b"\0lib.so\0symbol\0")
    buckets = [0] * bucket_count
    chains = []
    bloom = 0
    for index, name in enumerate(names, 2):
        code = 0x240 + (index - 2) * 16
        elf[code] = 0xC3
        struct.pack_into("<IBBHQQ", elf, SYMBOLS + index * 24,
                         len(strings), 0x12, 0, 1, code, 1)
        strings.extend(name.encode() + b"\0")
        hashed = name_hash(name)
        bucket = hashed % bucket_count
        if buckets[bucket] == 0:
            buckets[bucket] = index
            if chains:
                chains[-1] |= 1
        chains.append(hashed & ~1)
        bloom |= (1 << (hashed % 64)) | (1 << ((hashed >> 5) % 64))
    if chains:
        chains[-1] |= 1
    struct.pack_into("<IIIIQ", elf, HASH, bucket_count, 2, 1, 5, bloom)
    struct.pack_into("<" + "I" * (len(buckets) + len(chains)), elf, HASH + 24,
                     *buckets, *chains)
    elf[STRINGS:STRINGS + len(strings)] = strings
    for offset in range(0x400, 0x4E0, 16):
        tag, value = struct.unpack_from("<qQ", elf, offset)
        if tag == DT_OS_SYMTABSZ:
            tag, value = DT_GNU_HASH, HASH
        elif tag == DT_STRTAB:
            value = STRINGS
        elif tag == DT_STRSZ:
            value = len(strings)
        struct.pack_into("<qQ", elf, offset, tag, value)
    return elf


def run(relinker, work, name, elf, error=None):
    source = work / (name + ".elf")
    output = work / (name + ".exe")
    source.write_bytes(elf)
    result = subprocess.run([str(relinker), "--windows", "--skip-sce-module",
                             str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if error is None:
        assert result.returncode == 0 and output.is_file(), (
            name, result.returncode, result.stdout, result.stderr)
        assert output.read_bytes()[:2] == b"MZ", name
    else:
        assert result.returncode == 2 and error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-gnu-hash-") as directory:
        work = Path(directory)
        run(relinker, work, "multiple-buckets", image())
        run(relinker, work, "one-chain", image(bucket_count=1))
        run(relinker, work, "no-hashed-symbols", image(names=()))
        run(relinker, work, "one-symbol", image(names=("alpha",)))

        for name, offset in (("zero-buckets", 0), ("zero-bloom", 8), ("zero-first-symbol", 4)):
            elf = image()
            struct.pack_into("<I", elf, HASH + offset, 0)
            run(relinker, work, name, elf, "invalid GNU hash header")

        elf = image()
        struct.pack_into("<I", elf, HASH + 24, 1)
        run(relinker, work, "bucket-before-symbols", elf, "invalid GNU hash bucket")

        elf = image()
        struct.pack_into("<I", elf, HASH + 8, 0xFFFFFFFF)
        run(relinker, work, "bloom-out-of-bounds", elf, "unmapped address")

        elf = image()
        struct.pack_into("<Q", elf, 0x400 + 5 * 16 + 8, 0xFF8)
        run(relinker, work, "header-out-of-bounds", elf, "unmapped address")

        elf = image()
        struct.pack_into("<Q", elf, 0x400 + 5 * 16 + 8, 0xFDC)
        struct.pack_into("<IIIIQIII", elf, 0xFDC, 1, 2, 1, 5, 0, 2, 2, 2)
        run(relinker, work, "unterminated-chain", elf, "unmapped address")

        elf = image(names=())
        struct.pack_into("<I", elf, HASH + 4, 0xFFFFFFFF)
        run(relinker, work, "symbols-out-of-bounds", elf, "unmapped address")

        elf = image()
        struct.pack_into("<Q", elf, 0x708, (5 << 32) | 7)
        struct.pack_into("<I", elf, SYMBOLS + 5 * 24, 8)
        run(relinker, work, "relocation-past-symbols", elf, "invalid relocation symbol")

        elf = image(bucket_count=1)
        struct.pack_into("<Q", elf, SYMBOLS + 4 * 24 + 8, 0x2000)
        run(relinker, work, "last-export-is-analyzed", elf, "invalid function range")
    print("GNU hash code analysis tests passed")


if __name__ == "__main__":
    main()
