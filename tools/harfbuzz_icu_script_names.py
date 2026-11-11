import argparse
import hashlib
import re
from pathlib import Path


ICU_68_2_SHA256 = "66fccd1aab39b466083373d36cfd2583b067f5b6c83c1caec6bd7faa49e65391"


def main():
    parser = argparse.ArgumentParser(description="Generate HarfBuzz script names from ICU 68.2 uscript.h")
    parser.add_argument("header", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.header.read_bytes()
    if hashlib.sha256(source).hexdigest() != ICU_68_2_SHA256:
        parser.error("expected icu4c/source/common/unicode/uscript.h from ICU release-68-2")
    entries = sorted(
        (int(code), name)
        for code, name in re.findall(rb"USCRIPT_\w+\s*=\s*(\d+),\s*/\*\s*([A-Za-z]{4})\s*\*/", source)
    )
    if [code for code, _ in entries] != list(range(193)):
        parser.error("expected one ISO 15924 name for every ICU 68.2 script code")
    names = ['"' + name.decode('ascii') + '"' for _, name in entries]
    lines = ["#pragma once", "", "#include <array>", "#include <string_view>", "", "namespace HarfBuzz {", "",
             f"constexpr std::array<std::string_view, {len(names)}> IcuScriptNames{{"]
    for start in range(0, len(names), 10):
        lines.append("    " + ", ".join(names[start:start + 10]) + ",")
    lines += ["};", "", "}", ""]
    args.output.write_bytes("\n".join(lines).encode("ascii"))


if __name__ == "__main__":
    main()
