"""Report which functions a PS5 executable imports and which of them this project implements.

Run the relinker with --registry first, then point this script at the JSON it wrote:

    relinker --registry --skip-sce-module eboot.bin out.elf
    python3 tools/compat_report.py out.registry.json

The report says how many imported functions resolve to an implementation, how many reach a
silent stub, and how many are unknown to the project, grouped by library. An unknown import is
a function that has to be written before the title can start.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?\{")
STUB = "NotImplemented_nid_no_patch"
NID_SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"


def compute_nid(name):
    digest = hashlib.sha1(name.encode() + NID_SUFFIX).digest()
    reversed_head = digest[7::-1]
    nid = ""
    for start in (0, 3):
        triple = (reversed_head[start] << 16) | (reversed_head[start + 1] << 8) | reversed_head[start + 2]
        for shift in (18, 12, 6, 0):
            nid += ALPHABET[(triple >> shift) & 0x3F]
    tail = (reversed_head[6] << 16) | (reversed_head[7] << 8)
    for shift in (18, 12, 6):
        nid += ALPHABET[(tail >> shift) & 0x3F]
    return nid


def body_end(text, start):
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return len(text)


def scan_library(path):
    implemented, stubbed = set(), set()
    for source in path.rglob("*.cpp"):
        text = source.read_text(errors="ignore")
        for match in DEFINITION.finditer(text):
            name = match.group(1)
            if name.endswith("_nid_no_patch"):
                continue
            body = text[match.end() - 1:body_end(text, match.end() - 1)]
            (stubbed if STUB in body else implemented).add(name)
    return implemented, stubbed - implemented


def project_functions():
    """Map every declared function to its NID, per library."""
    catalogue = {}
    for library in sorted(p for p in PRX.iterdir() if p.is_dir()):
        implemented, stubbed = scan_library(library)
        entries = {}
        for name in implemented:
            entries[compute_nid(name)] = (name, "implemented")
        for name in stubbed:
            entries[compute_nid(name)] = (name, "stub")
        catalogue[library.name] = entries
    return catalogue


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("registry", help="the .registry.json the relinker wrote")
    parser.add_argument("--list", action="store_true", help="list every unresolved import")
    arguments = parser.parse_args()

    catalogue = project_functions()
    by_nid = {}
    for library, entries in catalogue.items():
        for nid, (name, state) in entries.items():
            by_nid.setdefault(nid, (library, name, state))

    imports = json.loads(Path(arguments.registry).read_text())
    counts = {"implemented": 0, "stub": 0, "unknown": 0}
    missing = {}
    for entry in imports:
        nid = entry["nid"]
        library = entry.get("library", "")
        found = catalogue.get(library.removesuffix(".prx"), {}).get(nid) or by_nid.get(nid)
        if found is None:
            counts["unknown"] += 1
            missing.setdefault(library, []).append((nid, "unknown"))
            continue
        state = found[2] if len(found) == 3 else found[1]
        counts[state] += 1
        if state == "stub":
            missing.setdefault(library, []).append((nid, found[0] if len(found) == 2 else found[1]))

    total = sum(counts.values())
    print(f"imported functions: {total}")
    print(f"  implemented: {counts['implemented']}")
    print(f"  silent stub: {counts['stub']}")
    print(f"  unknown to the project: {counts['unknown']}")
    if total:
        print(f"  ready: {100 * counts['implemented'] / total:.1f}%")
    if missing:
        print("\nper library:")
        for library in sorted(missing, key=lambda name: -len(missing[name])):
            print(f"  {library:34} {len(missing[library])} to write")
            if arguments.list:
                for nid, name in sorted(missing[library]):
                    print(f"      {nid}  {name}")
    else:
        print("\nevery imported function has an implementation")


if __name__ == "__main__":
    main()
