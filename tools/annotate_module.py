#!/usr/bin/env python3
import argparse
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nid_names

PT_LOAD = 1
PT_DYNAMIC = 2
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
STT_TLS = 6
DT_GNU_HASH = 0x6ffffef5
SYMBOL_SIZE = 24
RELA_SIZE = 24
R_X86_64_NONE = 0

LINE_ADDRESS = re.compile(r"^\s*([0-9a-f]+):")
STUB = re.compile(r"^\s*([0-9a-f]+):\s+(?:\w+\s+)*jmpq?\s+\*(?:-?0x[0-9a-f]+)?\(%(?:rip|eip)\)\s+# 0x([0-9a-f]+)")
BRANCH = re.compile(r"\b(?:call|jmp)q?\s+0x([0-9a-f]+)")
REFERENCE = re.compile(r"# 0x([0-9a-f]+)")


class ModuleError(Exception):
    pass


class Module:
    def __init__(self, data):
        self.data = data
        self.loads = []
        self.dynamic = None
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
        self.symbol_count = self.count_symbols()
        self.address(self.tags[DT_SYMTAB], self.symbol_count * SYMBOL_SIZE)
        self.check_relocation_tags()

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
        if self.unpack("<H", 0x12)[0] != 0x3E:
            raise ModuleError("not an x86-64 ELF")
        phoff, = self.unpack("<Q", 0x20)
        phentsize, phnum = self.unpack("<HH", 0x36)
        for index in range(phnum):
            kind, flags, offset, vaddr, _, filesz, _, _ = self.unpack("<IIQQQQQQ", phoff + index * phentsize)
            if kind == PT_LOAD:
                self.loads.append((vaddr, offset, filesz, flags))
            elif kind == PT_DYNAMIC and self.dynamic is None:
                self.dynamic = (offset, filesz)
        if self.dynamic is None:
            raise ModuleError("no PT_DYNAMIC program header")

    def read_tags(self):
        tags = {}
        offset, size = self.dynamic
        for position in range(offset, offset + size, 16):
            tag, value = self.unpack("<qQ", position)
            if tag == DT_NULL:
                break
            tags.setdefault(tag, value)
        return tags

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
        buckets_at = table + 16 + bloom_size * 8
        chains_at = buckets_at + buckets * 4
        self.address(table, chains_at - table)
        last = max((self.unpack("<I", self.address(buckets_at + 4 * index, 4))[0] for index in range(buckets)), default=0)
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

    def imports(self):
        slots = {}
        for table, size in self.relocation_tables():
            if size % RELA_SIZE:
                raise ModuleError("relocation table at %#x has %d bytes, not a multiple of %d" % (table, size, RELA_SIZE))
            if size == 0:
                continue
            start = self.address(table, size)
            for position in range(start, start + size, RELA_SIZE):
                offset, info, _ = self.unpack("<QQq", position)
                if (info & 0xFFFFFFFF) == R_X86_64_NONE:
                    continue
                if info >> 32:
                    slots[offset] = self.symbol(info >> 32)[0]
        return slots

    def exports(self):
        found = {}
        thread_local = {}
        for index in range(1, self.symbol_count):
            name, shndx, value, kind, binding, visibility = self.symbol(index)
            if shndx == 0 or not name or binding == 0 or visibility in (1, 2):
                continue
            (thread_local if kind == STT_TLS else found).setdefault(value, []).append(name)
        return found, thread_local

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


OBJDUMP_NAMES = ("objdump", "gobjdump", "x86_64-linux-gnu-objdump", "x86_64-elf-objdump")


def gnu_objdump():
    for name in OBJDUMP_NAMES:
        path = shutil.which(name)
        if path is None:
            continue
        result = subprocess.run([path, "--version"], capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode == 0 and result.stdout.startswith("GNU objdump"):
            return path
    return None


def disassemble(code, vaddr):
    objdump = gnu_objdump()
    if objdump is None:
        raise ModuleError("GNU binutils objdump was not found as any of %s" % ", ".join(OBJDUMP_NAMES))
    with tempfile.TemporaryDirectory() as directory:
        blob = Path(directory) / "text.bin"
        blob.write_bytes(code)
        command = [objdump, "-D", "--disassemble-zeroes", "-b", "binary", "-m", "i386:x86-64", "--no-show-raw-insn",
                   "--adjust-vma=%d" % vaddr, str(blob)]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        raise ModuleError("objdump failed: %s" % result.stderr.strip())
    return result.stdout.splitlines()


def referenced_address(line):
    if "%fs:" in line or "%gs:" in line:
        return None
    reference = REFERENCE.search(line)
    if reference is None:
        return None
    address = int(reference.group(1), 16)
    return address & 0xFFFFFFFF if "(%eip)" in line else address


def find_stubs(listing, slots, namer):
    stubs = {}
    for line in listing:
        match = STUB.match(line)
        target = referenced_address(line)
        if match and target in slots:
            stubs[int(match.group(1), 16)] = namer.resolve(slots[target])
    return stubs


def line_note(line, slots, stubs, namer):
    note = ""
    branch = BRANCH.search(line)
    if branch and int(branch.group(1), 16) in stubs:
        note = "   ; -> " + stubs[int(branch.group(1), 16)]
    reference = referenced_address(line)
    if reference in slots:
        note = "   ; GOT " + namer.resolve(slots[reference])
    return note


def annotate_listing(listing, slots, exports, stubs, namer):
    out = []
    for line in listing:
        head = LINE_ADDRESS.match(line)
        if head and int(head.group(1), 16) in exports:
            out.append("")
            out.extend("; ===== export " + namer.resolve(name) for name in exports[int(head.group(1), 16)])
        out.append(line + line_note(line, slots, stubs, namer))
    return out


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        for line in lines:
            handle.write(line + "\n")


def export_lines(exports, thread_local, namer):
    lines = ["%#x %s %s" % (address, name, namer.resolve(name)) for address in sorted(exports) for name in exports[address]]
    lines += ["tls+%#x %s %s" % (offset, name, namer.resolve(name)) for offset in sorted(thread_local) for name in thread_local[offset]]
    return lines


def annotate(module_path, prefix, namer):
    source = Path(module_path)
    for suffix in (".imports.txt", ".exports.txt", ".asm"):
        output = Path(prefix + suffix)
        if output.resolve() == source.resolve() or (output.exists() and source.exists() and output.samefile(source)):
            raise ModuleError(f"annotation output must be separate from the input module: {output}")
    module = Module(Path(module_path).read_bytes())
    slots = module.imports()
    exports, thread_local = module.exports()
    listing = []
    for vaddr, code in module.code_segments():
        listing.append("; ===== segment %#x" % vaddr)
        listing.extend(disassemble(code, vaddr))
    stubs = find_stubs(listing, slots, namer)
    write_lines(prefix + ".imports.txt", ["%#x %s %s" % (slot, slots[slot], namer.resolve(slots[slot])) for slot in sorted(slots)])
    write_lines(prefix + ".exports.txt", export_lines(exports, thread_local, namer))
    write_lines(prefix + ".asm", annotate_listing(listing, slots, exports, stubs, namer))
    export_count = sum(len(names) for names in exports.values())
    tls_count = sum(len(names) for names in thread_local.values())
    return "%s: %d imports, %d exports, %d tls, %d stubs, %d lines" % (prefix, len(slots), export_count, tls_count, len(stubs), len(listing))


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Annotated disassembly of a relinked guest module (no section headers). Writes PREFIX.imports.txt, "
                    "PREFIX.exports.txt and PREFIX.asm, with imports, PLT stubs, GOT references and exports named "
                    "through the NID database. Requires GNU binutils objdump (objdump, or gobjdump from Homebrew binutils).")
    parser.add_argument("module", help="relinked guest module, for example libc.prx.guest.prx")
    parser.add_argument("--out", help="output prefix (default: the module path)")
    parser.add_argument("--db", help="NID database CSV (downloaded to a cache dir by default)")
    parser.add_argument("--no-names", action="store_true", help="print NIDs without loading the database")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_arguments(sys.argv[1:] if argv is None else argv)
    if gnu_objdump() is None:
        sys.exit("GNU binutils objdump is required but none of %s in PATH is GNU objdump" % ", ".join(OBJDUMP_NAMES))
    try:
        db = {} if args.no_names else nid_names.load_db(Path(args.db) if args.db else nid_names.DEFAULT_CACHE)
        print(annotate(args.module, args.out or args.module, Namer(db)))
    except (ModuleError, OSError, UnicodeError) as error:
        sys.exit("%s: %s" % (args.module, error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
