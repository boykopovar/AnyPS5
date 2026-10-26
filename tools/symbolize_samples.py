import argparse
import bisect
import collections
import shutil
import struct
import subprocess
import sys
from pathlib import Path

DESCRIPTION = """Symbolize the output of the Windows samplers (APS5_SAMPLE_WORKER, APS5_SAMPLE_COMMITTER and
APS5_SAMPLE_THREADS) against the modules that ran. Each sampled frame is written as module+0xRVA; this reads the
COFF symbol table of each module found in --libs (the MinGW-built .prx and .dll files keep one), or its export
table when it has none, and names every frame module!function.

The '# module name size timestamp' header lines the samplers write are compared with the SizeOfImage and
TimeDateStamp of the file that is found; a module whose file does not match is left unsymbolized, since its
addresses would name the wrong functions.

The sampler rewrites its files every 20 s with totals since start-up. --since takes an earlier copy of the same
file and subtracts it, which keeps only the samples taken between the two copies (a steady-state window).

A module with no such record is also left unsymbolized, since nothing shows which build it was; --unverified
symbolizes it anyway against the file found.

Input: a collapsed-stack file (<file stem>.collapsed.txt, 'frame;...;frame count' lines) or an address file
('module offset self inclusive' lines, under 'thread <id> samples <n>' headings for APS5_SAMPLE_THREADS; --thread
keeps one thread). Output: the hottest functions by self and by inclusive samples, and with --out a symbolized
collapsed-stack file for speedscope.app or flamegraph.pl. For an address file, a function's inclusive count is the
sum of its addresses' counts, which overcounts the samples in which it is on the stack more than once."""

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


class PeModule:
    def __init__(self, path):
        data = Path(path).read_bytes()
        if data[:2] != b"MZ":
            raise ValueError(f"{path}: not a PE file")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE file")
        _, sections, self.timestamp, symbol_table, symbol_count, optional_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
        optional = pe + 24
        if struct.unpack_from("<H", data, optional)[0] != 0x20B:
            raise ValueError(f"{path}: not a PE32+ image")
        self.image_base = struct.unpack_from("<Q", data, optional + 24)[0]
        self.size = struct.unpack_from("<I", data, optional + 56)[0]
        export_rva, export_size = struct.unpack_from("<II", data, optional + 112)
        self.sections = []
        for index in range(sections):
            entry = optional + optional_size + index * 40
            virtual_size, rva, raw_size, raw_pointer = struct.unpack_from("<IIII", data, entry + 8)
            characteristics = struct.unpack_from("<I", data, entry + 36)[0]
            self.sections.append((rva, max(virtual_size, raw_size), raw_pointer, characteristics))
        symbols = self.coff_symbols(data, symbol_table, symbol_count) if symbol_table and symbol_count else []
        self.exported = not symbols
        if self.exported and export_rva:
            symbols = self.export_symbols(data, export_rva, export_size)
        symbols.sort()
        self.code = []
        for start, size, _, characteristics in self.sections:
            if not characteristics & IMAGE_SCN_CNT_CODE:
                continue
            inside = [(address, name) for address, name in symbols if start <= address < start + size]
            self.code.append((start, start + size, [address for address, _ in inside], [name for _, name in inside]))
        self.names = [name for _, name in symbols]

    def offset(self, rva):
        for start, size, raw, _ in self.sections:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(f"RVA {rva:#x} is in no section")

    def coff_symbols(self, data, table, count):
        strings = table + count * 18
        result = []
        index = 0
        while index < count:
            entry = table + index * 18
            name, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, entry)
            index += 1 + aux
            if storage not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC) or not 1 <= section <= len(self.sections):
                continue
            start, _, _, characteristics = self.sections[section - 1]
            if not characteristics & IMAGE_SCN_CNT_CODE:
                continue
            if storage == IMAGE_SYM_CLASS_STATIC and (kind >> 4) != 2 and value == 0:
                continue
            if name[:4] == b"\0\0\0\0":
                at = strings + struct.unpack_from("<I", name, 4)[0]
                end = data.index(b"\0", at)
                text = data[at:end]
            else:
                text = name.rstrip(b"\0")
            result.append((start + value, text.decode("utf-8", "replace")))
        return result

    def export_symbols(self, data, rva, size):
        directory = self.offset(rva)
        _, names, function_rva, name_rva, ordinal_rva = struct.unpack_from("<IIIII", data, directory + 20)
        function_table = self.offset(function_rva)
        result = []
        if names:
            name_table = self.offset(name_rva)
            ordinal_table = self.offset(ordinal_rva)
            for index in range(names):
                ordinal = struct.unpack_from("<H", data, ordinal_table + index * 2)[0]
                address = struct.unpack_from("<I", data, function_table + ordinal * 4)[0]
                if rva <= address < rva + size:
                    continue
                at = self.offset(struct.unpack_from("<I", data, name_table + index * 4)[0])
                result.append((address, data[at:data.index(b"\0", at)].decode("utf-8", "replace")))
        return result

    def name(self, rva):
        for start, end, addresses, names in self.code:
            if start <= rva < end:
                index = bisect.bisect_right(addresses, rva) - 1
                return None if index < 0 else ("~" if self.exported else "") + names[index]
        return None


class Symbolizer:
    def __init__(self, libs, headers, demangle=False, warn=print, unverified=False):
        self.files = {}
        for directory in libs:
            for path in Path(directory).iterdir():
                if path.is_file():
                    self.files.setdefault(path.name.lower(), path)
        self.headers = headers
        self.modules = {}
        self.demangle = demangle and shutil.which("c++filt") is not None
        self.demangled = {}
        self.warn = warn
        self.unverified = unverified

    def module(self, name):
        if name in self.modules:
            return self.modules[name]
        module = None
        path = self.files.get(name.lower())
        if path is not None:
            try:
                module = PeModule(path)
            except (ValueError, struct.error) as error:
                self.warn(f"{name}: {error}")
            if module is not None and name not in self.headers and not self.unverified:
                self.warn(f"{name}: the samples have no '# module' record for it, so {path} may be another build; its frames stay unsymbolized (--unverified symbolizes them anyway)")
                module = None
            if module is not None and name in self.headers and self.headers[name] != (module.size, module.timestamp):
                size, timestamp = self.headers[name]
                self.warn(f"{name}: the sampled module had size {size:#x} and timestamp {timestamp:#x}, but {path} has {module.size:#x} and {module.timestamp:#x}; its frames stay unsymbolized")
                module = None
        self.modules[name] = module
        return module

    def frame(self, frame):
        name, separator, offset = frame.partition("+0x")
        if not separator:
            return frame
        module = self.module(name)
        if module is None:
            return frame
        try:
            symbol = module.name(int(offset, 16))
        except ValueError:
            return frame
        if symbol is None:
            return frame
        return f"{name}!{self.pretty(symbol)}"

    def pretty(self, symbol):
        if not self.demangle or not symbol.lstrip("~").startswith("_Z"):
            return symbol
        if symbol not in self.demangled:
            plain = symbol.lstrip("~")
            result = subprocess.run(["c++filt", plain], capture_output=True, text=True)
            text = result.stdout.strip() if result.returncode == 0 and result.stdout.strip() else plain
            self.demangled[symbol] = ("~" if symbol.startswith("~") else "") + text
        return self.demangled[symbol]


def read_headers(lines):
    headers = {}
    for line in lines:
        parts = line.split()
        if len(parts) == 5 and parts[:2] == ["#", "module"]:
            headers[parts[2]] = (int(parts[3], 16), int(parts[4], 16))
    return headers


class Samples:
    def __init__(self):
        self.headers = {}
        self.stacks = collections.Counter()
        self.self_counts = collections.Counter()
        self.inclusive = collections.Counter()
        self.total = collections.Counter()


def load(path, thread=None):
    lines = Path(path).read_text(encoding="utf-8-sig").splitlines()
    samples = Samples()
    samples.headers = read_headers(lines)
    current = None
    for number, line in enumerate(lines, 1):
        parts = line.split()
        if len(parts) == 3 and parts[0] == "#" and parts[2] == "samples" and parts[1].isdigit():
            samples.total["samples"] += int(parts[1])
            continue
        if line.startswith("#") or not line.strip():
            continue
        if len(parts) == 4 and parts[0] == "thread" and parts[2] == "samples" and parts[1].isdigit() and parts[3].isdigit():
            current = parts[1]
            if thread is None or current == thread:
                samples.total["threads"] += int(parts[3])
            continue
        if len(parts) == 4 and parts[1].startswith("0x") and parts[2].isdigit() and parts[3].isdigit():
            if thread is None or current == thread:
                samples.self_counts[(parts[0], parts[1])] += int(parts[2])
                samples.inclusive[(parts[0], parts[1])] += int(parts[3])
            continue
        frames, _, count = line.rpartition(" ")
        if not frames or not count.isdigit():
            raise ValueError(f"{path}:{number}: expected 'frame;...;frame count', 'module offset self inclusive' or 'thread <id> samples <n>'")
        samples.stacks[frames] += int(count)
    return samples


def subtract(later, earlier):
    result = collections.Counter(later)
    result.subtract(earlier)
    return collections.Counter({key: count for key, count in result.items() if count > 0})


def window(later, earlier):
    result = Samples()
    result.headers = dict(later.headers)
    for name in ("stacks", "self_counts", "inclusive", "total"):
        setattr(result, name, subtract(getattr(later, name), getattr(earlier, name)))
    return result


def summarize(symbolizer, samples, top):
    self_counts = collections.Counter()
    inclusive = collections.Counter()
    total = 0
    if samples.stacks:
        for frames, count in samples.stacks.items():
            names = [symbolizer.frame(frame) for frame in frames.split(";")]
            total += count
            self_counts[names[-1]] += count
            for name in set(names):
                inclusive[name] += count
    else:
        for (module, offset), count in samples.self_counts.items():
            self_counts[symbolizer.frame(f"{module}+{offset}")] += count
        for (module, offset), count in samples.inclusive.items():
            inclusive[symbolizer.frame(f"{module}+{offset}")] += count
        total = samples.total["threads"] or samples.total["samples"] or sum(samples.self_counts.values())
    lines = [f"{total} samples"]
    for title, counter in (("self", self_counts), ("inclusive", inclusive)):
        if not counter:
            continue
        lines.append(f"hottest by {title}:")
        for name, count in counter.most_common(top):
            lines.append(f"  {count:8d} {100.0 * count / total if total else 0.0:5.1f}%  {name}")
    return lines


def main(argv=None):
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("samples", help="collapsed-stack file or address file written by a Windows sampler")
    parser.add_argument("--libs", action="append", required=True, help="directory holding the modules that ran (repeatable)")
    parser.add_argument("--since", help="an earlier copy of the same file, subtracted to keep the samples taken after it")
    parser.add_argument("--out", help="write the symbolized collapsed stacks to this file")
    parser.add_argument("--top", type=int, default=30, help="functions listed per table (default 30)")
    parser.add_argument("--demangle", action="store_true", help="demangle C++ names with c++filt when it is on PATH")
    parser.add_argument("--thread", help="keep only this thread of an APS5_SAMPLE_THREADS file")
    parser.add_argument("--unverified", action="store_true", help="also symbolize modules the samples have no '# module' record for")
    args = parser.parse_args(argv)
    samples = load(args.samples, args.thread)
    if args.since:
        samples = window(samples, load(args.since, args.thread))
    symbolizer = Symbolizer(args.libs, samples.headers, args.demangle, warn=lambda text: print("warning: " + text, file=sys.stderr), unverified=args.unverified)
    for line in summarize(symbolizer, samples, args.top):
        print(line)
    if args.out:
        if not samples.stacks:
            parser.error("--out needs a collapsed-stack file")
        with open(args.out, "w", encoding="utf-8") as output:
            for frames, count in samples.stacks.items():
                output.write(";".join(symbolizer.frame(frame) for frame in frames.split(";")) + f" {count}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
