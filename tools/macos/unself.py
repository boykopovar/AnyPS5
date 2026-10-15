"""Unwrap a fake-signed (unencrypted) PS5 SELF into a plain ELF.

Port of ExtractSelf() from SoS-PS5 launcher/launcher.cpp:483-553.
Usage: python3 -I unself.py <in.self> <out.elf>
Exit 0 = written, 1 = not a SELF / encrypted / malformed.
"""
import struct
import sys

SELF_MAGIC = b"\x4f\x15\x3d\x1d"
ENTRY_FMT = "<QQQQ"   # props, offset, fileSize, memSize
PHDR_FMT = "<IIQQQQQQ"  # type, flags, offset, vaddr, paddr, filesz, memsz, align
SEGMENT_HAS_BLOCKS = 0x800
SEGMENT_ENCRYPTED_OR_COMPRESSED = 0xA


def unwrap(src_path: str, dst_path: str) -> str:
    with open(src_path, "rb") as src:
        head = src.read(0x20)
        if head[:4] == b"\x7fELF":
            return "already ELF"
        if head[:4] != SELF_MAGIC:
            raise ValueError("not a SELF container")
        entry_count = struct.unpack_from("<H", head, 0x18)[0]
        if not 0 < entry_count < 4096:
            raise ValueError(f"bad entry count {entry_count}")
        entries = [struct.unpack(ENTRY_FMT, src.read(32)) for _ in range(entry_count)]
        elf_base = 0x20 + entry_count * 32
        elf_head = src.read(64)
        if elf_head[:4] != b"\x7fELF":
            raise ValueError("embedded ELF header missing")
        phoff = struct.unpack_from("<Q", elf_head, 0x20)[0]
        phnum = struct.unpack_from("<H", elf_head, 0x38)[0]
        src.seek(elf_base + phoff)
        raw_phdrs = src.read(phnum * 56)
        phdrs = [struct.unpack_from(PHDR_FMT, raw_phdrs, i * 56) for i in range(phnum)]

        # Encrypted/compressed segments cannot be recovered without keys.
        for props, *_ in entries:
            if props & SEGMENT_HAS_BLOCKS and props & SEGMENT_ENCRYPTED_OR_COMPRESSED:
                raise ValueError("encrypted or compressed segment (not a decrypted dump)")

        end = max(p[2] + p[5] for p in phdrs)
        with open(dst_path, "wb") as dst:
            dst.truncate(end)
            dst.seek(0)
            dst.write(elf_head)
            dst.seek(phoff)
            dst.write(raw_phdrs)
            for props, offset, file_size, _ in entries:
                if not props & SEGMENT_HAS_BLOCKS:
                    continue
                index = (props >> 20) & 0xFFF
                if index >= len(phdrs):
                    continue
                src.seek(offset)
                dst.seek(phdrs[index][2])
                dst.write(src.read(min(file_size, phdrs[index][5])))
    return f"unwrapped {entry_count} entries, {phnum} phdrs, {end} bytes"


if __name__ == "__main__":
    try:
        print(unwrap(sys.argv[1], sys.argv[2]))
    except ValueError as error:
        print(f"FAIL: {error}")
        sys.exit(1)
