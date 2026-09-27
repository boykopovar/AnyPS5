#!/usr/bin/env python3
"""Compare an ELF's dynamic imports with NID-patched Windows PRX exports.

Requires readelf and objdump on PATH (provided by the WinLibs toolchain).
This is a binding audit, not a test of runtime behavior or PS5 payload support.
"""

import argparse
import base64
import hashlib
import pathlib
import re
import subprocess
import sys


NID_SUFFIX = bytes.fromhex("518d64a635ded8c1e6b039b1c3e55230")
EXPORT = re.compile(r"^\s*\[\s*\d+\]\s+(\S+)\s*$", re.MULTILINE)


def nid(symbol: str) -> str:
    digest = hashlib.sha1(symbol.encode("utf-8") + NID_SUFFIX).digest()
    return base64.b64encode(digest[:8][::-1]).decode("ascii").rstrip("=").replace("/", "-")


def run(tool: str, path: pathlib.Path, *args: str) -> str:
    return subprocess.check_output([tool, *args, str(path)], text=True, errors="replace")


def imports(elf: pathlib.Path) -> set[str]:
    symbols = set()
    for line in run("readelf", elf, "--dyn-syms", "--wide").splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[6] == "UND":
            symbols.add(fields[7].split("@", 1)[0])
    return symbols - {""}


def exports(directory: pathlib.Path) -> dict[str, set[str]]:
    found: dict[str, set[str]] = {}
    for prx in sorted(directory.glob("*.prx")):
        output = run("objdump", prx, "-p")
        table = output.split("[Ordinal/Name Pointer] Table", 1)
        if len(table) < 2:
            continue
        for name in EXPORT.findall(table[1]):
            found.setdefault(name, set()).add(prx.name)
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=pathlib.Path)
    parser.add_argument("patched_prx_directory", type=pathlib.Path,
                        help="build/core/libs/libs, not its unpatched subdirectory")
    args = parser.parse_args()
    if not args.elf.is_file() or not args.patched_prx_directory.is_dir():
        parser.error("ELF file and patched PRX directory must exist")
    try:
        symbols = imports(args.elf)
        providers = exports(args.patched_prx_directory)
    except (OSError, subprocess.CalledProcessError) as exc:
        parser.error(str(exc))
    if not providers:
        parser.error("no PE exports found; use NID-patched Windows PRXs")

    missing = sorted(symbol for symbol in symbols
                     if symbol not in providers and nid(symbol) not in providers)
    print(f"{len(symbols)} unique imports; {len(symbols) - len(missing)} resolved; "
          f"{len(missing)} missing")
    for symbol in missing:
        print(f"  {symbol}  [{nid(symbol)}]")
    if missing:
        print("Import availability does not imply correct ABI, behavior, or payload startup.")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
