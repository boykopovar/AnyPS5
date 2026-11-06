"""R_X86_64_32 absolute pointer relocation in a Windows PE.

The fixture places the relocation in a writable load segment, uses a 32-bit
representable target value and checks the PE base-relocation RVA produced by
the real relinker. The Windows writer supports R_X86_64_32 only in the
symbol-free form (S == 0), where the addend is already an in-image virtual
address; a defined-symbol form is rejected because the relinker pipeline
discards the original defined-symbol table.

This is a CLI fixture: it runs the real relinker binary and inspects the
generated PE. No mock, no green stub.
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_optional_plt import fixture

R_X86_64_32 = 10
PE_LOAD_RVA = 0x10000
PREFERRED_BASE = 0x3F0000  # default executable PE image base
POINTER_VADDR = 0x240
POINTER_TARGET_VADDR = 0x210
POINTER_RVA = PE_LOAD_RVA + POINTER_VADDR
POINTER_VALUE = PREFERRED_BASE + PE_LOAD_RVA + POINTER_TARGET_VADDR
RELOCATION_ADDEND = POINTER_TARGET_VADDR
SENTINEL_VADDR = POINTER_VADDR + 4
SENTINEL = 0x5A5A5A5A


def reloc32_image():
    image = bytearray(fixture())
    struct.pack_into("<H", image, 16, 0xFE10)  # ET_DYN
    struct.pack_into("<Q", image, 24, 0x200)  # entry returns 42
    image[0x200:0x206] = b"\xb8\x2a\x00\x00\x00\xc3"
    struct.pack_into("<I", image, POINTER_VADDR, POINTER_VALUE)
    struct.pack_into("<I", image, SENTINEL_VADDR, SENTINEL)
    # R_X86_64_32 with S == 0: r_info = type only, addend is the in-image target
    # virtual address. The relinker rebuilds the dynamic symbol table and
    # discards the original defined-symbol table, so a referenced symbol is always
    # an undefined import (S == 0); only this form is supported.
    struct.pack_into("<QQq", image, 0x700, POINTER_VADDR,
                     (0 << 32) | R_X86_64_32, RELOCATION_ADDEND)
    return image


def overflow_image():
    image = reloc32_image()
    struct.pack_into("<Q", image, 0x700 + 16, 0x100000000)
    return image


def rva_to_offset(data, rva):
    """Translate a PE RVA to a file offset using the section table."""
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    sec = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
    for index in range(nsec):
        at = sec + index * 40
        vaddr = struct.unpack_from("<I", data, at + 12)[0]
        vsize = struct.unpack_from("<I", data, at + 8)[0]
        rawptr = struct.unpack_from("<I", data, at + 20)[0]
        if vaddr <= rva < vaddr + vsize:
            return rawptr + rva - vaddr
    return rva


def pe_relocation_table(data):
    """Return [(page_rva, entry_value), ...] from the .reloc directory."""
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    optional = pe + 24
    reloc_rva = struct.unpack_from("<I", data, optional + 112 + 5 * 8)[0]
    reloc_size = struct.unpack_from("<I", data, optional + 112 + 5 * 8 + 4)[0]
    if reloc_rva == 0 or reloc_size == 0:
        return []
    table = []
    cursor = rva_to_offset(data, reloc_rva)
    end = rva_to_offset(data, reloc_rva + reloc_size)
    while cursor + 8 <= end:
        page, block_size = struct.unpack_from("<II", data, cursor)
        if block_size == 0:
            break
        count = (block_size - 8) // 2
        for index in range(count):
            entry = struct.unpack_from("<H", data, cursor + 8 + index * 2)[0]
            table.append((page, entry))
        cursor += block_size
    return table


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-reloc32-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        output = work / "output.exe"
        source.write_bytes(reloc32_image())
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
            capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (
            "R_X86_64_32 conformance requires a PE; current writer rejects type 10: "
            f"{result.returncode} {result.stdout} {result.stderr}")
        data = output.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        optional = pe + 24
        image_base = struct.unpack_from("<Q", data, optional + 24)[0]
        assert image_base == PREFERRED_BASE, hex(image_base)

        relocs = pe_relocation_table(data)
        highlow = [entry for page, entry in relocs
                   if entry != 0
                   and (page + (entry & 0xfff)) == POINTER_RVA
                   and (entry & 0xf000) == 0x3000]
        assert len(highlow) == 1, (
            f"expected one IMAGE_REL_BASED_HIGHLOW for {POINTER_RVA:#x}, got {relocs}")
        slot = struct.unpack_from("<I", data, rva_to_offset(data, POINTER_RVA))[0]
        assert slot == POINTER_VALUE, hex(slot)
        sentinel = struct.unpack_from("<I", data, rva_to_offset(data, POINTER_RVA + 4))[0]
        assert sentinel == SENTINEL, hex(sentinel)

        overflow = work / "overflow.elf"
        overflow_output = work / "overflow.exe"
        overflow.write_bytes(overflow_image())
        rejected = subprocess.run(
            [str(relinker), "--skip-sce-module", "--windows", str(overflow), str(overflow_output)],
            capture_output=True, text=True, timeout=30)
        assert rejected.returncode == 2, (rejected.returncode, rejected.stdout, rejected.stderr)
        assert "R_X86_64_32 addend does not fit in 32 bits" in rejected.stderr, rejected.stderr
        assert not overflow_output.exists()
        print("R_X86_64_32 base relocation conformance passed")


if __name__ == "__main__":
    main()
