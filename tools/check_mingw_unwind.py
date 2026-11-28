import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCES = {
    "ms": 'extern "C" __attribute__((noinline)) int target(int a) { volatile int keep[64]; keep[0] = a; if (a) throw 7; return keep[0]; }\n',
    "sysv": 'extern "C" __attribute__((sysv_abi, noinline)) int target(int a) { volatile int keep[64]; keep[0] = a; if (a) throw 7; return keep[0]; }\n',
}


def section_sizes(path):
    data = path.read_bytes()
    if len(data) < 20:
        raise ValueError(f"{path.name}: truncated COFF object")
    if int.from_bytes(data[0:2], "little") != 0x8664:
        raise ValueError(f"{path.name}: not an x86-64 COFF object")
    count = int.from_bytes(data[2:4], "little")
    symbol_table = int.from_bytes(data[8:12], "little")
    symbol_count = int.from_bytes(data[12:16], "little")
    optional_size = int.from_bytes(data[16:18], "little")
    string_table = symbol_table + symbol_count * 18 if symbol_table else 0
    table = 20 + optional_size
    sizes = {}
    for index in range(count):
        offset = table + index * 40
        if offset + 40 > len(data):
            raise ValueError(f"{path.name}: truncated section table")
        raw = data[offset:offset + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        if raw.startswith("/") and raw[1:].isdigit() and string_table:
            start = string_table + int(raw[1:])
            end = data.find(b"\0", start)
            if end < 0:
                raise ValueError(f"{path.name}: unterminated section name")
            name = data[start:end].decode("ascii", "replace")
        else:
            name = raw
        sizes[name] = int.from_bytes(data[offset + 16:offset + 20], "little")
    return sizes


def compile_object(compiler, source, output, flags):
    completed = subprocess.run(
        [str(compiler), "-O1", *flags, "-c", str(source), "-o", str(output)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode:
        raise RuntimeError(f"{output.name}: {completed.stderr.strip() or completed.stdout.strip()}")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Check the DWARF unwind tables of a MinGW build")
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--dwarf", required=True, type=int, choices=(1,),
                        help="the DWARF unwind mode CMake selected")
    args = parser.parse_args(argv)
    failures = []

    def report(condition, message):
        print(("ok: " if condition else "FAIL: ") + message)
        if not condition:
            failures.append(message)

    try:
        with tempfile.TemporaryDirectory(prefix="mingw-unwind-probe-") as directory:
            work_dir = Path(directory)
            sizes = {}
            for name, body in SOURCES.items():
                source = work_dir / f"{name}.cpp"
                source.write_bytes(body.encode("ascii"))
                for mode, flags in (("default", ()), ("dwarf", ("-fno-asynchronous-unwind-tables",))):
                    output = work_dir / f"{name}-{mode}.obj"
                    compile_object(args.compiler, source, output, flags)
                    sizes[name, mode] = section_sizes(output)
        for name in SOURCES:
            report(sizes[name, "dwarf"].get(".eh_frame", 0) > 0,
                   f"{name}: -fno-asynchronous-unwind-tables emits .eh_frame")
            report(".pdata" not in sizes[name, "dwarf"],
                   f"{name}: -fno-asynchronous-unwind-tables emits no .pdata")
        report(sizes["ms", "default"].get(".pdata", 0) > 0, "ms-abi: default mode emits .pdata")
        sysv = sizes["sysv", "default"].get(".pdata", 0) > 0
        print(f"{'ok' if not sysv else 'note'}: sysv-abi default mode {'emits' if sysv else 'emits no'} .pdata")
        print(f"DWARF mode {args.dwarf}: {len(failures)} failure(s)")
        return 1 if failures else 0
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
