import argparse
import json
import re
from collections import Counter
from pathlib import Path

import progress

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
SAFE = ("GuestCopyIn_nid_no_patch", "GuestCopyOut_nid_no_patch", "CopyGuestString_nid_no_patch",
        "CopyGuestPath_nid_no_patch", "CopyGuestEnvironment_nid_no_patch")
RAW_CALLS = ("memcpy", "memmove", "memset", "memcmp", "strcpy", "strncpy", "strlen", "strnlen", "strcmp", "strncmp",
             "snprintf", "sprintf", "copy", "copy_n", "fill", "fill_n")
PARAMETER = re.compile(r"([A-Za-z_][\w:<>,\s]*?(?:[*&]+\s*(?:const\b\s*)?)+)\s*(\w+)\s*$")


def pointer_parameters(signature):
    names = []
    depth, start = 0, 0
    parts = []
    for index, character in enumerate(signature):
        if character in "(<[":
            depth += 1
        elif character in ")>]":
            depth -= 1
        elif character == "," and depth == 0:
            parts.append(signature[start:index])
            start = index + 1
    parts.append(signature[start:])
    for part in parts:
        part = part.split("=")[0].strip()
        if "(" in part or "&" in part:
            continue
        match = PARAMETER.match(part)
        if match and "*" in match.group(1):
            names.append(match.group(2))
    return names


def accesses(body, name):
    found = []
    word = re.escape(name)
    patterns = (
        ("member", rf"\b{word}\s*->"),
        ("index", rf"\b{word}\s*\["),
        ("dereference", rf"(?:^|[=(,;{{}}!&|?:+\-]|\breturn)\s*\*\s*{word}\b"),
        ("library call", rf"\b(?:std::)?(?:{'|'.join(RAW_CALLS)})\s*\(\s*(?:[^,()]*,\s*)?(?:reinterpret_cast<[^>]*>\()?\s*{word}\b"),
    )
    for kind, pattern in patterns:
        for match in re.finditer(pattern, body):
            found.append((kind, match.end() - 1))
    return found


def audit_text(text):
    results = []
    for match in progress.DEFINITION.finditer(text):
        name = match.group(1)
        open_paren = text.index("(", match.start(1))
        close_paren = text.rindex(")", 0, match.end())
        body_start = match.end() - 1
        body = text[body_start:progress.body_end(text, body_start) + 1]
        for parameter in pointer_parameters(text[open_paren + 1:close_paren]):
            if any(re.search(rf"\b{safe}\s*\([^;]*\b{re.escape(parameter)}\b", body) for safe in SAFE):
                continue
            hits = accesses(body, parameter)
            if hits:
                line = text.count("\n", 0, body_start + hits[0][1]) + 1
                results.append({"function": name, "parameter": parameter, "line": line, "kinds": sorted({kind for kind, _ in hits})})
    return results


def audit(libraries):
    report = []
    for library in libraries:
        for source in sorted(library.rglob("*.cpp")):
            if "tests" in source.relative_to(library).parts:
                continue
            text = progress.code(source.read_text(encoding="utf-8", errors="ignore"))
            for entry in audit_text(text):
                report.append({"library": library.name, "file": source.relative_to(ROOT).as_posix(), **entry})
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description="List exported functions that access a guest pointer parameter directly instead of through GuestCopyIn, GuestCopyOut or the CopyGuest* helpers.")
    parser.add_argument("libraries", nargs="*", help="library directories under core/libs/prx (default: all)")
    parser.add_argument("--json", action="store_true", help="print the findings as JSON")
    args = parser.parse_args(argv)
    names = args.libraries or sorted(path.name for path in PRX.iterdir() if path.is_dir())
    missing = [name for name in names if not (PRX / name).is_dir()]
    if missing:
        parser.error(f"not a library under core/libs/prx: {', '.join(missing)}")
    report = audit([PRX / name for name in names])
    if args.json:
        print(json.dumps(report, indent=2))
        return 0
    for entry in report:
        print(f"{entry['file']}:{entry['line']}: {entry['function']}: parameter {entry['parameter']} ({', '.join(entry['kinds'])})")
    counts = Counter(entry["library"] for entry in report)
    print(f"{len(report)} direct guest pointer accesses in {len({(e['file'], e['function']) for e in report})} exported functions")
    for library, count in counts.most_common():
        print(f"  {count:5} {library}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
