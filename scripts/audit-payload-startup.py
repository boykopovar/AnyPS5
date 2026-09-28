#!/usr/bin/env python3
"""Inspect PS5 SDK payload startup requirements without executing the ELF.

This is a format/ABI audit, not a claim that the payload is runnable on a host.
It intentionally uses only the Python standard library and reads no firmware.
"""

import argparse
import json
import struct
from pathlib import Path


class ElfError(ValueError):
    pass


def inspect(data):
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ElfError("expected a little-endian ELF64 file")
    e_type, machine = struct.unpack_from("<HH", data, 16)
    entry, phoff, shoff = struct.unpack_from("<QQQ", data, 24)
    phentsize, phnum, shentsize, shnum = struct.unpack_from("<HHHH", data, 54)
    if machine != 62 or phentsize != 56 or shentsize != 64:
        raise ElfError("expected an x86-64 ELF with standard header sizes")

    def table(offset, count, size):
        if count > (len(data) - offset) // size or offset > len(data):
            raise ElfError("ELF table extends past the end of the file")
        return [offset + index * size for index in range(count)]

    load_segments = []
    for offset in table(phoff, phnum, phentsize):
        p_type, flags, file_offset, vaddr, _, filesz, _ = struct.unpack_from("<IIQQQQQ", data, offset)
        if p_type == 1:
            if file_offset > len(data) or filesz > len(data) - file_offset:
                raise ElfError("PT_LOAD extends past the end of the file")
            load_segments.append((vaddr, file_offset, filesz, flags))

    sections = []
    for offset in table(shoff, shnum, shentsize):
        _, kind, _, _, section_offset, size, link, _, _, entsize = struct.unpack_from("<IIQQQQIIQQ", data, offset)
        if section_offset > len(data) or size > len(data) - section_offset:
            raise ElfError("section extends past the end of the file")
        sections.append((kind, section_offset, size, link, entsize))

    symbols = {}
    for kind, offset, size, link, entsize in sections:
        if kind not in (2, 11):
            continue
        if entsize != 24 or link >= len(sections):
            raise ElfError("invalid ELF symbol table")
        _, string_offset, string_size, _, _ = sections[link]
        strings = data[string_offset:string_offset + string_size]
        for symbol_offset in range(offset, offset + size, entsize):
            name_offset, _, _, section_index, value, symbol_size = struct.unpack_from("<IBBHQQ", data, symbol_offset)
            if not section_index or name_offset >= len(strings):
                continue
            end = strings.find(b"\0", name_offset)
            if end < 0:
                raise ElfError("unterminated ELF symbol")
            name = strings[name_offset:end].decode("utf-8", "replace")
            if name:
                symbols[name] = {"address": value, "size": symbol_size}

    # A byte pattern can occur inside a different instruction. These are
    # candidates for investigation, not a decoded instruction inventory.
    syscall_sites = []
    for vaddr, offset, size, flags in load_segments:
        if not flags & 1:
            continue
        segment = data[offset:offset + size]
        start = 0
        while (index := segment.find(b"\x0f\x05", start)) != -1:
            syscall_sites.append(vaddr + index)
            start = index + 2

    sdk_markers = ("payload_args", "__crt_syscall_init", "__kernel_init")
    sdk_payload = all(name in symbols for name in sdk_markers)
    return {
        "elf_type": f"0x{e_type:04x}",
        "entry": f"0x{entry:x}",
        "sdk_payload_markers": {name: symbols.get(name) for name in sdk_markers},
        "sdk_payload_detected": sdk_payload,
        "raw_syscall_opcode_candidates": [f"0x{address:x}" for address in syscall_sites],
        "startup_contract": (
            "resolver, rwpipe[2], rwpair[2], kernel pipe address, "
            "kernel data base address, result pointer"
        ) if sdk_payload else None,
        "host_windows_runnable": False if sdk_payload and syscall_sites else None,
        "reason": (
            "The Windows entry stub does not supply the SDK kernel context or "
            "translate these raw syscall instructions. --skip-syscall-check "
            "only bypasses conversion validation."
        ) if sdk_payload and syscall_sites else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.elf.read_bytes()), indent=2))
    except (OSError, ElfError) as error:
        parser.exit(1, f"FAIL: {error}\n")


if __name__ == "__main__":
    main()
