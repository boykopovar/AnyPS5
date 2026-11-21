#!/usr/bin/env python3
"""Extract real x86-64 ELF function bytes; reject relocations and missing symbols.

Usage: python3 generate_stage3_fixture.py object.o output.hpp nm objcopy readelf
The object is built with a selected Clang major version and documented flags.
"""
import pathlib
import re
import subprocess
import sys

obj, output, nm, objcopy, readelf = sys.argv[1:]
names = ("fixture_triangular", "fixture_bitmix", "fixture_popcount",
         "fixture_factorial", "fixture_fib", "fixture_gcd",
         "fixture_digit_sum", "fixture_reverse_bits",
         "fixture_array_sum", "fixture_array_sort",
         "fixture_recursive_factorial", "fixture_callback_twice")

def execute(*args):
    return subprocess.check_output(args, text=True)

relocations = execute(readelf, "-r", obj)
# Relocations in non-code sections are not permitted either: fail closed.
if "There are no relocations in this file." not in relocations:
    raise SystemExit("Stage 3 fixture rejected: relocations present:\n" + relocations)
symbols = {}
for line in execute(nm, "-S", "--defined-only", obj).splitlines():
    match = re.fullmatch(r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([Tt])\s+(\S+)", line.strip())
    if match:
        symbols[match.group(4)] = (int(match.group(1), 16), int(match.group(2), 16))
if set(names) - symbols.keys():
    raise SystemExit("Stage 3 fixture missing symbols: " + str(set(names) - symbols.keys()))
binary = pathlib.Path(output).with_suffix(".text.bin")
subprocess.check_call([objcopy, "--only-section=.text", "-O", "binary", obj, str(binary)])
data = binary.read_bytes()
result = ["#pragma once", "#include <array>", "#include <cstdint>", "#include <span>",
          "namespace stage3_generated {"]
for name in names:
    offset, size = symbols[name]
    if not size or offset + size > len(data):
        raise SystemExit(f"Stage 3 fixture invalid bounds: {name}")
    bytestr = ", ".join(f"0x{byte:02x}" for byte in data[offset:offset + size])
    result.append(f"inline constexpr std::array<std::uint8_t, {size}> {name}{{{{{bytestr}}}}};")
result.append("}")
pathlib.Path(output).write_text("\n".join(result) + "\n", encoding="utf-8")
print("Extracted", len(names), "relocation-free functions from", obj)
