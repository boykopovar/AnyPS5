#!/usr/bin/env python3
import argparse
import bisect
import functools
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
import json

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nid_names

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_GNU_EH_FRAME = 0x6474e550
PF_X = 1
PF_W = 2
DT_NULL = 0
DT_HASH = 4
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_JMPREL = 23
DT_PLTRELSZ = 2
DT_PLTREL = 20
DT_STRSZ = 10
DT_SYMENT = 11
DT_RELAENT = 9
DT_REL = 17
DT_RELA_KIND = 7
STT_OBJECT = 1
STT_FUNC = 2
STT_TLS = 6
STT_GNU_IFUNC = 10
SHN_LORESERVE = 0xFF00
SHN_COMMON = 0xFFF2
GHIDRA_MAX_NAME_LENGTH = 2000
DT_GNU_HASH = 0x6ffffef5
DT_TLSDESC_PLT = 0x6ffffef6
DT_FLAGS_1 = 0x6ffffffb
DF_1_PIE = 0x8000000
R_X86_64_64 = 1
R_X86_64_RELATIVE = 8
R_X86_64_TLSDESC = 36
SYMBOL_SIZE = 24
RELA_SIZE = 24
R_X86_64_NONE = 0
R_X86_64_COPY = 5
R_X86_64_IRELATIVE = 37
DYNAMIC_VALUE_TAGS = {
    12: "DT_INIT", 13: "DT_FINI",
    DT_TLSDESC_PLT: "DT_TLSDESC_PLT",
    25: "DT_INIT_ARRAY", 27: "DT_INIT_ARRAYSZ", 26: "DT_FINI_ARRAY", 28: "DT_FINI_ARRAYSZ",
    32: "DT_PREINIT_ARRAY", 33: "DT_PREINIT_ARRAYSZ",
    DT_HASH: "DT_HASH", DT_STRTAB: "DT_STRTAB", DT_SYMTAB: "DT_SYMTAB",
    DT_RELA: "DT_RELA", DT_RELASZ: "DT_RELASZ", DT_RELAENT: "DT_RELAENT",
    DT_STRSZ: "DT_STRSZ", DT_SYMENT: "DT_SYMENT", DT_JMPREL: "DT_JMPREL",
    DT_PLTRELSZ: "DT_PLTRELSZ", DT_PLTREL: "DT_PLTREL", DT_GNU_HASH: "DT_GNU_HASH",
}

LINE_ADDRESS = re.compile(r"^\s*([0-9a-f]+):")
ENDBR64 = re.compile(r"^\s*([0-9a-f]+):\s+endbr64\s*$")
STUB = re.compile(r"^\s*([0-9a-f]+):\s+(?:\w+\s+)*jmpq?\s+\*(?:-?0x[0-9a-f]+)?\(%(?:rip|eip)\)\s+# 0x([0-9a-f]+)")
BRANCH = re.compile(r"\b(?:call|jmp)q?\s+0x([0-9a-f]+)")
REFERENCE = re.compile(r"# 0x([0-9a-f]+)")
LEA = re.compile(r"^\s*[0-9a-f]+:\s+(?:\w+\s+)*lea[wlq]?\s+[^#]*,\s*%(\w+)")


class ModuleError(Exception):
    pass


class Module:
    def __init__(self, data):
        self.data = data
        self.loads = []
        self.load_memory_ranges = []
        self.unwind_headers = []
        self.dynamic = None
        self.has_interpreter = False
        self.read_headers()
        self.tags = self.read_tags()
        self.strsz = self.require_tag(DT_STRSZ, "DT_STRSZ")
        self.strtab = self.address(self.require_tag(DT_STRTAB, "DT_STRTAB"))
        self.symtab = self.address(self.require_tag(DT_SYMTAB, "DT_SYMTAB"))
        if self.strtab + self.strsz > len(self.data):
            raise ModuleError("the string table of DT_STRSZ %#x extends past the end of the file" % self.strsz)
        self.address(self.tags[DT_STRTAB], self.strsz)
        if self.require_tag(DT_SYMENT, "DT_SYMENT") != SYMBOL_SIZE:
            raise ModuleError("DT_SYMENT is not %d" % SYMBOL_SIZE)
        self.check_relocation_tags()
        self.symbol_count = self.count_symbols()
        self.address(self.tags[DT_SYMTAB], self.symbol_count * SYMBOL_SIZE)

    def unpack(self, form, offset):
        size = struct.calcsize(form)
        if offset < 0 or offset + size > len(self.data):
            raise ModuleError("read of %d bytes at file offset %#x is outside the file" % (size, offset))
        return struct.unpack_from(form, self.data, offset)

    def read_headers(self):
        if len(self.data) < 64 or self.data[:4] != b"\x7fELF":
            raise ModuleError("not an ELF file")
        if self.data[4] != 2 or self.data[5] != 1:
            raise ModuleError("not a 64-bit little-endian ELF")
        if self.data[6] != 1 or self.unpack("<I", 20)[0] != 1:
            raise ModuleError("unsupported ELF version")
        if self.unpack("<H", 52)[0] != 64:
            raise ModuleError("unsupported ELF header size")
        if self.unpack("<H", 0x12)[0] != 0x3E:
            raise ModuleError("not an x86-64 ELF")
        phoff, = self.unpack("<Q", 0x20)
        phentsize, phnum = self.unpack("<HH", 0x36)
        if phentsize != 56:
            raise ModuleError("unsupported ELF program header size")
        if (phnum and phoff < 64) or phoff + phnum * phentsize > len(self.data):
            raise ModuleError("ELF program header table is outside its supported file range")
        for index in range(phnum):
            kind, flags, offset, vaddr, _, filesz, memsz, _ = self.unpack("<IIQQQQQQ", phoff + index * phentsize)
            if kind == PT_LOAD:
                self.loads.append((vaddr, offset, filesz, flags))
                self.load_memory_ranges.append(range(vaddr, vaddr + memsz))
            elif kind == PT_GNU_EH_FRAME and filesz:
                self.unwind_headers.append((vaddr, offset, filesz))
            elif kind == PT_INTERP:
                self.has_interpreter = True
            elif kind == PT_DYNAMIC and self.dynamic is None:
                self.dynamic = (offset, filesz)
        if self.dynamic is None:
            raise ModuleError("no PT_DYNAMIC program header")

    def entry_point(self):
        address, = self.unpack("<Q", 24)
        if address and any(flags & PF_X and start <= address < start + size and offset + address - start < len(self.data)
                           for start, offset, size, flags in self.loads):
            return address
        return None

    def read_tags(self):
        tags = {}
        offset, size = self.dynamic
        if offset + size > len(self.data):
            raise ModuleError("PT_DYNAMIC extends past the end of the file")
        for position in range(offset, offset + size - 15, 16):
            tag, value = self.unpack("<qQ", position)
            if tag == DT_NULL:
                return tags
            if tag in DYNAMIC_VALUE_TAGS and tag in tags and tags[tag] != value:
                raise ModuleError("conflicting duplicate %s at %#x" % (DYNAMIC_VALUE_TAGS[tag], position))
            tags.setdefault(tag, value)
        raise ModuleError("PT_DYNAMIC has no complete DT_NULL terminator")

    def require_tag(self, tag, name):
        if tag not in self.tags:
            raise ModuleError("the dynamic section has no %s" % name)
        return self.tags[tag]

    def address(self, vaddr, length=1):
        for start, offset, size, _ in self.loads:
            if start <= vaddr <= start + size:
                position = offset + vaddr - start
                if position + length > len(self.data):
                    continue
                covered = vaddr
                for next_start, next_offset, next_size, _ in sorted(self.loads):
                    if next_offset - next_start == position - vaddr and next_start <= covered:
                        covered = max(covered, next_start + next_size)
                if covered >= vaddr + length:
                    if length and any(next_size and next_start < vaddr + length and vaddr < next_start + next_size
                                      and next_offset - next_start != position - vaddr
                                      for next_start, next_offset, next_size, _ in self.loads):
                        raise ModuleError("range %#x+%#x has ambiguous PT_LOAD file mappings" % (vaddr, length))
                    return position
        raise ModuleError("range %#x+%#x is outside file-backed PT_LOAD data" % (vaddr, length))

    def count_symbols(self):
        if DT_HASH in self.tags:
            table = self.tags[DT_HASH]
            buckets, count = self.unpack("<II", self.address(table, 8))
            self.address(table, (2 + buckets + count) * 4)
            return count
        if DT_GNU_HASH in self.tags:
            return self.count_gnu_symbols(self.tags[DT_GNU_HASH])
        raise ModuleError("the dynamic section has neither DT_HASH nor DT_GNU_HASH, so the symbol count is unknown")

    def count_gnu_symbols(self, table):
        buckets, symoffset, bloom_size, _ = self.unpack("<IIII", self.address(table, 16))
        if bloom_size == 0 or (bloom_size & (bloom_size - 1)) != 0:
            raise ModuleError("GNU hash Bloom filter count %d is not a power of two" % bloom_size)
        buckets_at = table + 16 + bloom_size * 8
        chains_at = buckets_at + buckets * 4
        self.address(table, chains_at - table)
        last = max((self.unpack("<I", self.address(buckets_at + 4 * index, 4))[0] for index in range(buckets)), default=0)
        if last == 0:
            referenced = max((info >> 32 for _, info, _ in self.relocations()), default=0)
            return max(symoffset, referenced + 1)
        if last < symoffset:
            return symoffset
        while not self.unpack("<I", self.address(chains_at + 4 * (last - symoffset), 4))[0] & 1:
            last += 1
        return last + 1

    def symbol(self, index):
        if index >= self.symbol_count:
            raise ModuleError("symbol index %d is outside the %d symbols of the module" % (index, self.symbol_count))
        name, info, other, shndx, value, _ = self.unpack("<IBBHQQ", self.symtab + index * SYMBOL_SIZE)
        if name >= self.strsz:
            raise ModuleError("symbol %d has a name offset %#x outside DT_STRSZ %#x" % (index, name, self.strsz))
        start = self.strtab + name
        end = self.data.find(b"\0", start, self.strtab + self.strsz)
        if end < 0:
            raise ModuleError("symbol %d has a name unterminated within DT_STRSZ %#x" % (index, self.strsz))
        return self.data[start:end].decode("utf-8", errors="replace"), shndx, value, info & 0xF, info >> 4, other & 3

    def check_relocation_tags(self):
        if DT_REL in self.tags:
            raise ModuleError("DT_REL relocations are not supported")
        if DT_RELA in self.tags:
            self.require_tag(DT_RELASZ, "DT_RELASZ")
        if DT_JMPREL in self.tags:
            self.require_tag(DT_PLTRELSZ, "DT_PLTRELSZ")
            if self.require_tag(DT_PLTREL, "DT_PLTREL") != DT_RELA_KIND:
                raise ModuleError("DT_PLTREL is not DT_RELA")
        if self.tags.get(DT_RELAENT, RELA_SIZE) != RELA_SIZE:
            raise ModuleError("DT_RELAENT is not %d" % RELA_SIZE)

    def relocation_tables(self):
        if DT_RELA in self.tags:
            yield self.tags[DT_RELA], self.tags[DT_RELASZ]
        if DT_JMPREL in self.tags:
            yield self.tags[DT_JMPREL], self.tags[DT_PLTRELSZ]

    def relocations(self):
        for table, size in self.relocation_tables():
            if size % RELA_SIZE:
                raise ModuleError("relocation table at %#x has %d bytes, not a multiple of %d" % (table, size, RELA_SIZE))
            if size == 0:
                continue
            start = self.address(table, size)
            for position in range(start, start + size, RELA_SIZE):
                entry = self.unpack("<QQq", position)
                if (entry[1] & 0xFFFFFFFF) != R_X86_64_NONE:
                    yield entry

    def imports(self):
        slots = {}
        for offset, info, _ in self.relocations():
            if info >> 32:
                name, section, _, _, binding, visibility = self.symbol(info >> 32)
                if section and (binding == 0 or visibility != 0) and (info & 0xffffffff) != R_X86_64_COPY:
                    continue
                slots[offset] = name
        return slots

    def exported_symbols(self):
        for index in range(1, self.symbol_count):
            name, shndx, value, kind, binding, visibility = self.symbol(index)
            if shndx == 0 or not name or binding == 0 or visibility in (1, 2):
                continue
            yield name, shndx, value, kind

    def exports(self):
        found = {}
        thread_local = {}
        for name, _, value, kind in self.exported_symbols():
            (thread_local if kind == STT_TLS else found).setdefault(value, []).append(name)
        return found, thread_local

    def lifecycle_array_functions(self):
        arrays = ((25, 27, "init"), (26, 28, "fini"), (32, 33, "preinit"))
        if not any(tag in self.tags or size_tag in self.tags for tag, size_tag, _ in arrays):
            return
        relocations = {}
        for offset, info, addend in self.relocations():
            relocations.setdefault(offset, set()).add((info, addend))
        offsets = sorted(relocations)
        for tag, size_tag, name in arrays:
            if tag not in self.tags and size_tag not in self.tags:
                continue
            if tag == 32 and not (self.unpack("<H", 16)[0] == 2 or self.has_interpreter or self.tags.get(DT_FLAGS_1, 0) & DF_1_PIE):
                continue
            size = self.require_tag(size_tag, DYNAMIC_VALUE_TAGS[size_tag])
            if size % 8:
                raise ModuleError("%s is not a multiple of 8" % DYNAMIC_VALUE_TAGS[size_tag])
            if not size:
                continue
            table = self.require_tag(tag, DYNAMIC_VALUE_TAGS[tag])
            start = self.address(table, size)
            for index in range(size // 8):
                slot = table + index * 8
                nearby = offsets[bisect.bisect_left(offsets, slot - 15):bisect.bisect_left(offsets, slot + 8)]
                nearby = [offset for offset in nearby if offset >= slot or any(
                    offset + (16 if info & 0xffffffff == R_X86_64_TLSDESC else 8) > slot for info, _ in relocations[offset])]
                if nearby:
                    if nearby != [slot] or len(relocations[slot]) != 1:
                        continue
                    info, addend = next(iter(relocations[slot]))
                    if info == R_X86_64_RELATIVE:
                        address = addend & 0xffffffffffffffff
                    elif info & 0xffffffff == R_X86_64_64:
                        _, section, value, kind, _, _ = self.symbol(info >> 32)
                        if section in (0, SHN_COMMON) or kind in (STT_TLS, STT_GNU_IFUNC):
                            continue
                        address = (value + addend) & 0xffffffffffffffff
                    else:
                        continue
                else:
                    address, = self.unpack("<Q", start + index * 8)
                    if address in (0, 0xffffffffffffffff):
                        continue
                yield address, "elf_%s_array_%d" % (name, index)

    def unwind_starts(self):
        for base, offset, size in self.unwind_headers:
            if size < 4 or offset + size > len(self.data):
                raise ModuleError("truncated PT_GNU_EH_FRAME")
            if self.data[offset:offset + 4] != b"\x01\x1b\x03\x3b":
                continue
            if size < 12 or self.address(base, size) != offset:
                raise ModuleError("invalid PT_GNU_EH_FRAME mapping")
            frame_delta, count = self.unpack("<iI", offset + 4)
            if count > (size - 12) // 8:
                raise ModuleError("unwind search table exceeds PT_GNU_EH_FRAME")
            frame = base + 4 + frame_delta
            if count:
                self.address(frame, 4)
            for index in range(count):
                pc_delta, fde_delta = self.unpack("<ii", offset + 12 + index * 8)
                pc, fde = base + pc_delta, base + fde_delta
                if fde < frame:
                    raise ModuleError("unwind search table points before .eh_frame")
                self.address(fde, 4)
                if any(flags & PF_X and start <= pc < start + length and file_offset + pc - start < len(self.data)
                       for start, file_offset, length, flags in self.loads):
                    yield pc

    def unwind_labels(self, named_addresses):
        return {address: "unwind_%x" % address for address in self.unwind_starts() if address not in named_addresses}

    def code_segments(self):
        segments = []
        for vaddr, offset, size, flags in sorted(self.loads):
            if flags & PF_X and size:
                if offset + size > len(self.data):
                    raise ModuleError("the executable PT_LOAD segment at %#x extends past the end of the file" % vaddr)
                segments.append((vaddr, self.data[offset:offset + size]))
        if not segments:
            raise ModuleError("no executable PT_LOAD segment with file-backed data")
        return segments


class Namer:
    def __init__(self, db):
        self.db = db

    def resolve(self, symbol):
        nid = symbol.split("#", 1)[0]
        name = nid_names.strip_nid_postfix(self.db.get(nid, ""))
        return name if name and nid_names.compute_nid(name) == nid else nid


def ghidra_symbols(module, namer):
    rows = {(address, False, "imp_" + namer.resolve(symbol), symbol) for address, symbol in module.imports().items()}
    exported_functions = {address for _, section, address, kind in module.exported_symbols()
                          if kind in (STT_FUNC, STT_GNU_IFUNC) and section != SHN_COMMON}
    for address, name in module.lifecycle_array_functions():
        if address not in exported_functions and any(flags & PF_X and start <= address < start + size and
                                                     offset + address - start < len(module.data)
                                                     for start, offset, size, flags in module.loads):
            rows.add((address, True, name, name))
    for tag, name in ((12, "elf_init"), (13, "elf_fini"), (DT_TLSDESC_PLT, "elf_tlsdesc_plt")):
        address = module.tags.get(tag)
        if address is None or address in exported_functions:
            continue
        if any(flags & PF_X and start <= address < start + size and offset + address - start < len(module.data)
               for start, offset, size, flags in module.loads):
            rows.add((address, True, name, name))
    entry = module.entry_point()
    if entry is not None and entry not in exported_functions and not any(address == entry and function for address, function, _, _ in rows):
        rows.add((entry, True, "elf_entry", "elf_entry"))
    for symbol, section, address, kind in module.exported_symbols():
        if kind == STT_TLS or section == SHN_COMMON:
            continue
        function = kind in (STT_FUNC, STT_GNU_IFUNC) and any(flags & PF_X and start <= address < start + size and
                                                          offset + address - start < len(module.data)
                                                          for start, offset, size, flags in module.loads)
        rows.add((address, function, namer.resolve(symbol), symbol))
    exported_functions = {address for address, function, _, _ in rows if function}
    for _, info, addend in module.relocations():
        if info != R_X86_64_IRELATIVE:
            continue
        address = addend & 0xffffffffffffffff
        if address not in exported_functions and any(flags & PF_X and start <= address < start + size and
                                                     offset + address - start < len(module.data)
                                                     for start, offset, size, flags in module.loads):
            name = "irelative_resolver_%x" % address
            rows.add((address, True, name, name))
    named_addresses = {address for address, _, _, _ in rows}
    for address, name in module.unwind_labels(named_addresses).items():
        rows.add((address, False, name, name))
    names, functions, lines = set(), set(), []
    for address, function, resolved, symbol in sorted(rows, key=lambda row: (row[0], not row[1], row[2] == row[3].split("#", 1)[0], row[2], row[3])):
        stem = re.sub(r"[^A-Za-z0-9_]", "_", resolved)
        if not stem or stem[0].isdigit():
            stem = "_" + stem
        stem = stem[:GHIDRA_MAX_NAME_LENGTH]
        name, suffix = stem, 2
        while name in names:
            ending = "_%d" % suffix
            name = stem[:GHIDRA_MAX_NAME_LENGTH - len(ending)] + ending
            suffix += 1
        names.add(name)
        marker = "f" if function and address not in functions else "l"
        if marker == "f":
            functions.add(address)
        lines.append("%s %#x %s" % (name, address, marker))
    return lines


OBJDUMP_NAMES = ("objdump", "gobjdump", "x86_64-linux-gnu-objdump", "x86_64-elf-objdump")
OBJDUMP_REQUIRED = "GNU binutils objdump with x86-64 support is required (tried %s)" % ", ".join(OBJDUMP_NAMES)


@functools.cache
def gnu_objdump():
    for name in OBJDUMP_NAMES:
        path = shutil.which(name)
        if path is None:
            continue
        try:
            result = subprocess.run([path, "--version"], capture_output=True, text=True, encoding="utf-8", errors="replace")
            if result.returncode != 0 or not result.stdout.startswith("GNU objdump"):
                continue
            probe = run_objdump(path, b"\xc3", 0)
            if probe.returncode == 0 and re.search(r"^[ \t]*0:[ \t]+retq?[ \t]*$", probe.stdout, re.M):
                return path
        except OSError:
            continue
    return None


def run_objdump(objdump, code, vaddr):
    objdump = str(Path(objdump).absolute())
    with tempfile.TemporaryDirectory() as directory:
        blob = Path(directory) / "text.bin"
        blob.write_bytes(code)
        command = [objdump, "-D", "--disassemble-zeroes", "-b", "binary", "-m", "i386:x86-64", "--no-show-raw-insn",
                   "--adjust-vma=%d" % vaddr, blob.name]
        return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", cwd=directory)


def disassemble(code, vaddr):
    objdump = gnu_objdump()
    if objdump is None:
        raise ModuleError(OBJDUMP_REQUIRED)
    result = run_objdump(objdump, code, vaddr)
    if result.returncode != 0:
        raise ModuleError("objdump failed: %s" % result.stderr.strip())
    return result.stdout.splitlines()


def referenced_address(line):
    lea = LEA.match(line)
    if ("%fs:" in line or "%gs:" in line) and not lea:
        return None
    reference = REFERENCE.search(line)
    if reference is None:
        return None
    address = int(reference.group(1), 16)
    if lea:
        destination = lea.group(1)
        if destination.startswith("e") or destination.endswith("d"):
            return address & 0xFFFFFFFF
        if not destination.startswith("r") or destination.endswith("w"):
            return None
    return address & 0xFFFFFFFF if "(%eip)" in line else address


def find_stubs(listing, slots, namer):
    stubs = {}
    landing = None
    for line in listing:
        match = STUB.match(line)
        target = referenced_address(line)
        if match and target in slots:
            address = int(match.group(1), 16)
            stubs[address] = namer.resolve(slots[target])
            if landing is not None and address == landing + 4:
                stubs[landing] = stubs[address]
        endbr = ENDBR64.match(line)
        landing = int(endbr.group(1), 16) if endbr else None
    return stubs


def line_note(line, slots, branch_targets, namer, address_exports=None):
    note = ""
    branch = BRANCH.search(line)
    if branch and int(branch.group(1), 16) in branch_targets:
        note = "   ; -> " + branch_targets[int(branch.group(1), 16)]
    reference = referenced_address(line)
    if reference in slots:
        note = "   ; GOT " + namer.resolve(slots[reference])
    elif address_exports and reference in address_exports and LEA.match(line) and ("(%rip)" in line or "(%eip)" in line):
        note = "   ; address of " + ", ".join(sorted(address_exports[reference]))
    return note


def annotate_listing(listing, slots, exports, branch_targets, namer, entry=None, unwind_labels=None, address_exports=None):
    out = []
    for line in listing:
        head = LINE_ADDRESS.match(line)
        if head:
            address = int(head.group(1), 16)
            if address == entry:
                out.append("; ===== ELF entry point")
            if address in exports:
                out.append("")
                out.extend("; ===== export " + namer.resolve(name) for name in exports[address])
            if unwind_labels and address in unwind_labels:
                out.append("; ===== unwind range " + unwind_labels[address])
        out.append(line + line_note(line, slots, branch_targets, namer, address_exports))
    return out


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        for line in lines:
            handle.write(line + "\n")


def export_lines(exports, thread_local, namer):
    lines = ["%#x %s %s" % (address, name, namer.resolve(name)) for address in sorted(exports) for name in exports[address]]
    lines += ["tls+%#x %s %s" % (offset, name, namer.resolve(name)) for offset in sorted(thread_local) for name in thread_local[offset]]
    return lines


def link_target(text):
    if os.name != "nt":
        return text
    if text.startswith("\\\\?\\UNC\\"):
        return "\\\\" + text[8:]
    if text.startswith("\\\\?\\"):
        return text[4:]
    return text


def resolved_path(path):
    for _ in range(40):
        if not path.is_symlink():
            break
        target = Path(link_target(os.readlink(path)))
        path = target if target.is_absolute() else path.parent / target
    try:
        return path.resolve(strict=True)
    except FileNotFoundError:
        return path.resolve()


def same_file(left, right):
    try:
        return resolved_path(left) == resolved_path(right) or (left.exists() and right.exists() and left.samefile(right))
    except (OSError, RuntimeError) as error:
        raise ModuleError(f"cannot resolve annotation path {left} or {right}: {error}") from error


def annotate(module_path, prefix, namer):
    source = Path(module_path)
    outputs = [Path(prefix + suffix) for suffix in (".imports.txt", ".exports.txt", ".asm")]
    for index, output in enumerate(outputs):
        if same_file(output, source):
            raise ModuleError(f"annotation output must be separate from the input module: {output}")
        if any(same_file(output, previous) for previous in outputs[:index]):
            raise ModuleError(f"annotation outputs must name different files: {output}")
    module = Module(Path(module_path).read_bytes())
    slots = module.imports()
    exports, thread_local = module.exports()
    listing = []
    entry = module.entry_point()
    segments = module.code_segments()
    for vaddr, code in segments:
        listing.append("; ===== segment %#x" % vaddr)
        if entry is not None and vaddr < entry < vaddr + len(code):
            split = entry - vaddr
            listing.extend(disassemble(code[:split], vaddr))
            code = code[split:]
            vaddr = entry
        listing.extend(disassemble(code, vaddr))
    stubs = find_stubs(listing, slots, namer)
    functions = {}
    address_exports = {}
    for symbol, section, address, kind in module.exported_symbols():
        if kind == STT_FUNC and section != SHN_COMMON and any(start <= address < start + len(code) for start, code in segments):
            name = namer.resolve(symbol)
            functions.setdefault(address, set()).add(name)
        elif kind == STT_OBJECT and any(address in span for span in module.load_memory_ranges):
            name = namer.resolve(symbol)
        else:
            continue
        if section < SHN_LORESERVE:
            address_exports.setdefault(address, set()).add(name)
    branch_targets = {address: ", ".join(sorted(names)) for address, names in functions.items()}
    branch_targets.update(stubs)
    try:
        unwind_labels = module.unwind_labels(exports.keys() | stubs.keys() | slots.keys() | {entry})
    except ModuleError as error:
        print(f"{source}: skipping unwind labels: {error}", file=sys.stderr)
        unwind_labels = {}
    if unwind_labels:
        decoded = (int(head.group(1), 16) for line in listing if (head := LINE_ADDRESS.match(line)))
        unwind_labels = {address: unwind_labels[address] for address in decoded if address in unwind_labels}
        branch_targets.update(unwind_labels)
    write_lines(prefix + ".imports.txt", ["%#x %s %s" % (slot, slots[slot], namer.resolve(slots[slot])) for slot in sorted(slots)])
    write_lines(prefix + ".exports.txt", export_lines(exports, thread_local, namer))
    write_lines(prefix + ".asm", annotate_listing(listing, slots, exports, branch_targets, namer, entry, unwind_labels, address_exports))
    export_count = sum(len(names) for names in exports.values())
    tls_count = sum(len(names) for names in thread_local.values())
    return "%s: %d imports, %d exports, %d tls, %d stubs, %d lines" % (prefix, len(slots), export_count, tls_count, len(stubs), len(listing))


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Annotated disassembly of a relinked guest module (no section headers). Writes PREFIX.imports.txt, "
                    "PREFIX.exports.txt and PREFIX.asm, with imports, PLT stubs, GOT references and exports named "
                    "through the NID database. Disassembly requires GNU binutils objdump with x86-64 support. "
                    "Offline symbol exports need no objdump.")
    parser.add_argument("module", help="relinked guest module, for example libc.prx.guest.prx")
    parser.add_argument("--out", help="output prefix (default: the module path)")
    parser.add_argument("--db", help="NID database CSV (symbol exports read local files only; disassembly downloads to a cache dir by default)")
    parser.add_argument("--no-names", action="store_true", help="print NIDs without loading the database")
    formats = parser.add_mutually_exclusive_group()
    formats.add_argument("--symbols-json", action="store_true", help="inspect imports and exports as JSON offline without objdump or output files")
    formats.add_argument("--symbols-ghidra", action="store_true", help="print an offline Ghidra ImportSymbolsScript map with labels for indexed unwind ranges; omits TLS and unallocated common symbols; needs no objdump")
    args = parser.parse_args(argv)
    if (args.symbols_json or args.symbols_ghidra) and args.out is not None:
        mode = "--symbols-json" if args.symbols_json else "--symbols-ghidra"
        parser.error("%s cannot be combined with --out" % mode)
    return args


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    args = parse_arguments(sys.argv[1:] if argv is None else argv)
    symbols_only = args.symbols_json or args.symbols_ghidra
    if not symbols_only and gnu_objdump() is None:
        sys.exit(OBJDUMP_REQUIRED)
    try:
        db_path = Path(args.db) if args.db else nid_names.DEFAULT_CACHE
        if args.no_names:
            db = {}
        elif symbols_only:
            db = nid_names.read_db(db_path) if args.db or db_path.exists() else {}
        else:
            try:
                db = nid_names.load_db(db_path)
            except ValueError as error:
                raise ModuleError(str(error)) from error
        namer = Namer(db)
        if args.symbols_ghidra:
            for line in ghidra_symbols(Module(Path(args.module).read_bytes()), namer):
                print(line)
        elif args.symbols_json:
            module = Module(Path(args.module).read_bytes())
            exports, thread_local = module.exports()
            def entries(values):
                return [{"address": address, "symbol": symbol, "name": namer.resolve(symbol)}
                        for address in sorted(values) for symbol in values[address]]
            print(json.dumps({"imports": entries({address: [symbol] for address, symbol in module.imports().items()}), "exports": entries(exports),
                              "tls_exports": entries(thread_local)}, ensure_ascii=False, indent=2))
        else:
            print(annotate(args.module, args.out or args.module, namer))
    except (ModuleError, OSError, UnicodeError) as error:
        sys.exit("%s: %s" % (args.module, error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
