import argparse
import bisect
import collections
import os
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
If two loaded modules share a name, their frames stay unsymbolized because the samples cannot distinguish them.

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
        if strings > len(data) - 4:
            raise ValueError("COFF string table header is outside the file")
        size = struct.unpack_from("<I", data, strings)[0]
        if size < 4 or size > len(data) - strings:
            raise ValueError("COFF string table size is outside the file")
        result = []
        index = 0
        while index < count:
            entry = table + index * 18
            name, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, entry)
            if aux > count - index - 1:
                raise ValueError("COFF auxiliary records exceed the symbol count")
            index += 1 + aux
            if storage not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC) or not 1 <= section <= len(self.sections):
                continue
            start, _, _, characteristics = self.sections[section - 1]
            if not characteristics & IMAGE_SCN_CNT_CODE:
                continue
            if storage == IMAGE_SYM_CLASS_STATIC and (kind >> 4) != 2 and value == 0:
                continue
            if name[:4] == b"\0\0\0\0":
                offset = struct.unpack_from("<I", name, 4)[0]
                if not 4 <= offset < size:
                    raise ValueError("COFF string table name offset is out of range")
                at = strings + offset
                end = data.find(b"\0", at, strings + size)
                if end < 0:
                    raise ValueError("COFF string table name is unterminated")
                text = data[at:end]
            else:
                text = name.rstrip(b"\0")
            result.append((start + value, text.decode("utf-8", "replace")))
        return result

    def export_symbols(self, data, rva, size):
        directory = self.offset(rva)
        functions, names, function_rva, name_rva, ordinal_rva = struct.unpack_from("<IIIII", data, directory + 20)
        function_table = self.offset(function_rva)
        result = []
        if names:
            name_table = self.offset(name_rva)
            ordinal_table = self.offset(ordinal_rva)
            for index in range(names):
                ordinal = struct.unpack_from("<H", data, ordinal_table + index * 2)[0]
                if ordinal >= functions:
                    raise ValueError("PE export ordinal is outside the export address table")
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
                    self.files.setdefault(path.name.lower(), {}).setdefault(path.resolve(), path)
        self.headers = {name.lower(): identity for name, identity in headers.items()}
        self.modules = {}
        self.demangle = demangle and shutil.which("c++filt") is not None
        self.demangled = {}
        self.warn = warn
        self.unverified = unverified

    def candidate_path(self, name):
        paths = list(self.files.get(name.lower(), {}).values())
        if len(paths) <= 1:
            return paths[0] if paths else None
        identity = self.headers.get(name.lower())
        if identity is None and not self.unverified:
            self.warn(f"{name}: the samples have no '# module' record; multiple candidates stay unsymbolized")
            return None
        matches = []
        for path in paths:
            try:
                candidate = PeModule(path)
            except (OSError, ValueError, struct.error) as error:
                self.warn(f"{name}: {path}: {error}")
                continue
            if identity is None or identity == (candidate.size, candidate.timestamp):
                matches.append(path)
        if len(matches) == 1:
            return matches[0]
        if matches:
            self.warn(f"{name}: ambiguous module candidates; matching size and timestamp do not identify one file, so frames stay unsymbolized")
        else:
            self.warn(f"{name}: no module candidate matches the sampled size and timestamp; frames stay unsymbolized")
        return None

    def module(self, name):
        if name in self.modules:
            return self.modules[name]
        module = None
        if self.headers.get(name.lower()) is False:
            self.warn(f"{name}: multiple sampled modules share this name, so their frames stay unsymbolized")
            self.modules[name] = None
            return None
        path = self.candidate_path(name)
        if path is not None:
            try:
                module = PeModule(path)
            except (OSError, ValueError, struct.error) as error:
                self.warn(f"{name}: {error}")
            if module is not None and name.lower() not in self.headers and not self.unverified:
                self.warn(f"{name}: the samples have no '# module' record for it, so {path} may be another build; its frames stay unsymbolized (--unverified symbolizes them anyway)")
                module = None
            if module is not None and name.lower() in self.headers and self.headers[name.lower()] != (module.size, module.timestamp):
                size, timestamp = self.headers[name.lower()]
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
            name = parts[2].lower()
            identity = (int(parts[3], 16), int(parts[4], 16))
            headers[name] = False if name in headers else identity
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
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
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
    try:
        return symbolize(parser, args)
    except (OSError, ValueError) as error:
        parser.error(str(error))


def symbolize(parser, args):
    if args.out:
        output = Path(args.out)
        for source in (args.samples, args.since):
            if source is not None:
                source = Path(source)
                if os.path.realpath(source) == os.path.realpath(output) or (source.exists() and output.exists() and source.samefile(output)):
                    parser.error("symbolized output must be separate from the sample captures")
    samples = load(args.samples, args.thread)
    if args.since:
        samples = window(samples, load(args.since, args.thread))
    if args.out and samples.stacks:
        referenced_modules = {name.lower() for name, _ in samples.self_counts}
        referenced_modules.update(name.lower() for name, _ in samples.inclusive)
        for frames in samples.stacks:
            for frame in frames.split(";"):
                name, separator, _ = frame.partition("+0x")
                if separator:
                    referenced_modules.add(name.lower())
        module_output_alias = False
        for directory in args.libs:
            for source in Path(directory).iterdir():
                if not source.is_file() or not (os.path.realpath(source) == os.path.realpath(output) or
                                                (source.exists() and output.exists() and source.samefile(output))):
                    continue
                if source.name.lower() in referenced_modules or source.suffix.lower() in (".prx", ".dll", ".exe"):
                    module_output_alias = True
                    continue
                try:
                    with source.open("rb") as module:
                        if module.read(2) == b"MZ":
                            module_output_alias = True
                except OSError:
                    module_output_alias = True
        if module_output_alias:
            parser.error("symbolized output must be separate from module inputs")
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
