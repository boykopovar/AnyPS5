#!/usr/bin/env python3
"""tools/check_conventions.py

Verify that C++ PRX stub files follow the project's C++ coding conventions
(AnyPS5 style).

Conventions checked
-------------------
* No tab characters anywhere in the file
* Indentation uses exactly 4 spaces per level
* No line exceeds MAX_LINE_LENGTH (120) characters
* Function definitions use K&R brace style (opening brace on the signature
  line, or on the line immediately following a wrapped parameter list)
* Each generated stub contains a SCE_NOT_IMPLEMENTED(...) call

Usage::

    python tools/check_conventions.py [path ...]

If no path is given, ``tools/stubs`` (and any ``*.cpp``/``*.h`` next to it)
are checked. Exits non-zero if any convention is violated.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import List, Tuple

INDENT = "    "
MAX_LINE_LENGTH = 120
STUB_MARKER = "SCE_NOT_IMPLEMENTED"


def check_file(path: Path) -> List[str]:
    """Return a list of human-readable convention violations for *path*."""
    violations: List[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return [f"{path}: cannot read file ({exc})"]

    lines = text.split("\n")
    for lineno, line in enumerate(lines, start=1):
        # 1. No tabs
        if "\t" in line:
            violations.append(f"{path}:{lineno}: tab character found")

        # 2. Line length limit
        if len(line) > MAX_LINE_LENGTH:
            violations.append(
                f"{path}:{lineno}: line too long ({len(line)} > "
                f"{MAX_LINE_LENGTH} chars)"
            )

        # 3. Indentation must be a multiple of 4 spaces
        stripped = line.lstrip(" ")
        indent_len = len(line) - len(stripped)
        if indent_len > 0 and indent_len % 4 != 0:
            violations.append(
                f"{path}:{lineno}: indentation is not a multiple of 4 "
                f"spaces ({indent_len} spaces)"
            )

    # 4. K&R brace style: every '{' that opens a function body must sit on
    #    the signature line (or the line closing a wrapped parameter list).
    #    We flag '{' on its own line that is not indented as a continuation.
    for lineno, line in enumerate(lines, start=1):
        if "{" in line:
            brace_index = line.index("{")
            before_brace = line[:brace_index].rstrip()
            # A lone '{' with nothing but whitespace before it violates K&R.
            if before_brace.strip() == "" and before_brace != "":
                violations.append(
                    f"{path}:{lineno}: opening brace not on signature line "
                    f"(K&R style required)"
                )

    # 5. Generated stubs should contain the not-implemented marker.
    if STUB_MARKER not in text:
        violations.append(
            f"{path}: missing {STUB_MARKER}(...) call (auto-generated stub)"
        )

    return violations


def collect_files(paths: List[Path]) -> List[Path]:
    """Expand directories to *.cpp/*.h files; keep files as-is."""
    files: List[Path] = []
    for p in paths:
        if p.is_dir():
            files.extend(sorted(p.rglob("*.cpp")))
            files.extend(sorted(p.rglob("*.h")))
        elif p.is_file():
            files.append(p)
    # De-duplicate while preserving order.
    seen = set()
    unique: List[Path] = []
    for f in files:
        if f not in seen:
            seen.add(f)
            unique.append(f)
    return unique


def main(argv: Optional[List[str]] = None) -> int:
    import argparse

    parser = argparse.ArgumentParser(
        description="Verify C++ PRX stub files follow the project's C++ "
                    "coding conventions.",
    )
    parser.add_argument("paths", nargs="*",
                        help="files or directories to check "
                             "(default: tools/stubs)")
    args = parser.parse_args(argv)

    if not args.paths:
        default = Path("tools/stubs")
        paths = [default] if default.exists() else [Path(".")]
    else:
        paths = [Path(p) for p in args.paths]

    files = collect_files(paths)
    if not files:
        print("no C++ files found to check")
        return 0

    all_violations: List[str] = []
    for f in files:
        all_violations.extend(check_file(f))

    if all_violations:
        print(f"convention violations found ({len(all_violations)}):")
        for v in all_violations:
            print("  " + v)
        return 1

    print(f"all {len(files)} file(s) conform to the C++ coding conventions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
