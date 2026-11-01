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
UNEVALUATED = ("sizeof", "alignof", "decltype", "noexcept")
RAW_CALLS = {
    "memcpy": (0, 1), "memmove": (0, 1), "memset": (0,), "memcmp": (0, 1),
    "strcpy": (0, 1), "strncpy": (0, 1), "strlen": (0,), "strnlen": (0,), "strcmp": (0, 1), "strncmp": (0, 1),
    "snprintf": (0,), "sprintf": (0,), "copy": (0, 1, 2), "copy_n": (0, 2), "fill": (0, 1), "fill_n": (0,),
}
PARAMETER = re.compile(r"([A-Za-z_][\w:<>,\s]*?(?:[*&]+\s*(?:const\b\s*)?)+)\s*(\w+)\s*$")
RAW_CALL = re.compile(rf"\b(?:std::|::)?({'|'.join(RAW_CALLS)})\s*\(")
QUIET_CALL = re.compile(rf"\b({'|'.join(SAFE + UNEVALUATED)})\s*\(")
CAST = re.compile(r"^(?:\(\s*[\w\s:*<>]+\)(?=\s*\S)|(?:reinterpret|static|const)_cast\s*<[^>]*>|\()\s*")


def split_top_level(text):
    parts, depth, start = [], 0, 0
    for index, character in enumerate(text):
        if character in "(<[{":
            depth += 1
        elif character in ")>]}":
            depth -= 1
        elif character == "," and depth == 0:
            parts.append((start, text[start:index]))
            start = index + 1
    parts.append((start, text[start:]))
    return parts


def closing(text, open_paren):
    depth = 0
    for index in range(open_paren, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    return len(text) - 1


def pointer_parameters(signature):
    names = []
    for _, part in split_top_level(signature):
        part = part.split("=")[0].strip()
        if "(" in part or "&" in part:
            continue
        match = PARAMETER.match(part)
        if match and "*" in match.group(1):
            names.append(match.group(2))
    return names


def quiet(body):
    blanked = list(body)
    for match in QUIET_CALL.finditer(body):
        start, end = match.end() - 1, closing(body, match.end() - 1)
        for index in range(start + 1, end):
            if blanked[index] != "\n":
                blanked[index] = " "
    return "".join(blanked)


def names_parameter(argument, name):
    argument = argument.strip()
    while True:
        stripped = CAST.sub("", argument, count=1)
        if stripped == argument:
            break
        argument = stripped
    return re.match(rf"{re.escape(name)}\b", argument) is not None


def accesses(body, name):
    word = re.escape(name)
    found = []
    for kind, pattern in (("member", rf"\b{word}\s*->"), ("index", rf"\b{word}\s*\["),
                          ("dereference", rf"(?:^|[=(,;{{}}!&|?:+\-]|\breturn)\s*\*\s*{word}\b")):
        found += [(match.end() - 1, kind) for match in re.finditer(pattern, body)]
    for match in RAW_CALL.finditer(body):
        open_paren = match.end() - 1
        arguments = split_top_level(body[open_paren + 1:closing(body, open_paren)])
        for position in RAW_CALLS[match.group(1)]:
            if position < len(arguments) and names_parameter(arguments[position][1], name):
                found.append((open_paren + 1 + arguments[position][0], "library call"))
    return sorted(found)


def audit_text(text):
    results = []
    for match in progress.DEFINITION.finditer(text):
        open_paren = text.index("(", match.start(1))
        close_paren = text.rindex(")", 0, match.end())
        body_start = match.end() - 1
        body = quiet(text[body_start:progress.body_end(text, body_start) + 1])
        for parameter in pointer_parameters(text[open_paren + 1:close_paren]):
            hits = accesses(body, parameter)
            if hits:
                line = text.count("\n", 0, body_start + hits[0][0]) + 1
                results.append({"function": match.group(1), "parameter": parameter, "line": line, "kinds": sorted({kind for _, kind in hits})})
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
