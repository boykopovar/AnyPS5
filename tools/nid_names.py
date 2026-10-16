#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
DB_URL = ("https://raw.githubusercontent.com/zecoxao/sce_symbols"
          "/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv")
DEFAULT_CACHE = Path(tempfile.gettempdir()) / "anyps5-nid-db" / "aerolib.csv"

CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-"
SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
CPP_TOKEN = re.compile(
    r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)'
    r'|(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*')"
    r"|(?P<code>(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S)")
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{")
NID_POSTFIX = "_nid_postfix"
ASM_FUNCTION_MACROS = {"APS5_ASM_FUNCTION", "APS5_SIGNAL_ASM_FUNCTION"}


def compute_nid(symbol):
    digest = hashlib.sha1(symbol.encode() + SUFFIX).digest()
    rev = bytes(digest[7 - i] for i in range(8))
    out = []
    for i in range(0, 6, 3):
        triple = (rev[i] << 16) | (rev[i + 1] << 8) | rev[i + 2]
        out += [CHARSET[(triple >> 18) & 63], CHARSET[(triple >> 12) & 63],
                CHARSET[(triple >> 6) & 63], CHARSET[triple & 63]]
    tail = (rev[6] << 16) | (rev[7] << 8)
    out += [CHARSET[(tail >> 18) & 63], CHARSET[(tail >> 12) & 63],
            CHARSET[(tail >> 6) & 63]]
    return "".join(out)


def load_db(path):
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        print("downloading NID database to %s ..." % path, file=sys.stderr)
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".", suffix=".tmp", delete=False) as download:
            temporary = Path(download.name)
        try:
            urllib.request.urlretrieve(DB_URL, str(temporary))
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)
    db = {}
    with open(path, encoding="utf-8-sig", errors="replace") as handle:
        for line in handle:
            parts = line.strip().split()
            if len(parts) >= 2 and not parts[0].startswith("#"):
                nid, name = (part.split("#", 1)[0] for part in parts[:2])
                if nid and name:
                    db.setdefault(nid, name)
    return db


def source_files(root):
    return (path for path in sorted(root.rglob("*.cpp"))
            if not any(part.lower() in {"test", "tests"}
                       for part in path.relative_to(root).parts[:-1]))


def source_aliases(text):
    tokens = [match.group() for match in CPP_TOKEN.finditer(text.replace("\\\n", "").replace("\\\r\n", ""))
              if match.group("comment") is None]
    for index, token in enumerate(tokens):
        if token != "APS5_EXPORT" or index + 5 >= len(tokens):
            continue
        opening, literal, comma, name, closing = tokens[index + 1:index + 6]
        if opening != "(" or comma != "," or closing != ")":
            continue
        if re.fullmatch(r'"[A-Za-z0-9+\-]{11}"', literal) is None:
            continue
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            yield literal[1:-1], name


def source_definitions(text):
    def mask(match):
        return match.group() if match.group("code") is not None else re.sub(r"[^\n]", " ", match.group())

    return DEFINITION.finditer(CPP_TOKEN.sub(mask, text.replace("\\\n", "").replace("\\\r\n", "")))


def source_assembly_definitions(text):
    tokens = [match for match in CPP_TOKEN.finditer(text.replace("\\\n", "").replace("\\\r\n", ""))
              if match.group("comment") is None]
    for index, token in enumerate(tokens):
        if token.group("code") is None or token.group() not in ASM_FUNCTION_MACROS:
            continue
        if index + 3 >= len(tokens):
            continue
        opening, literal, closing = tokens[index + 1:index + 4]
        if opening.group() != "(" or closing.group() != ")" or literal.group("string") is None:
            continue
        match = re.fullmatch(r'"([A-Za-z_][A-Za-z0-9_]*)"', literal.group("string"))
        if match is not None:
            yield match.group(1)


def collect_unknowns():
    found = {}
    for path in source_files(PRX):
        text = path.read_text(encoding="utf-8")
        for nid, name in source_aliases(text):
            if "unknown" in name.lower():
                found.setdefault(nid, "%s:%s" % (path.relative_to(ROOT).as_posix(), name))
    return found


def collect_real_names():
    names = set()
    for path in source_files(ROOT / "core" / "libs"):
        text = path.read_text(encoding="utf-8")
        for match in source_definitions(text):
            name = match.group(1)
            names.add(name)
            if name.endswith(NID_POSTFIX):
                names.add(name[:-len(NID_POSTFIX)])
        for name in source_assembly_definitions(text):
            names.add(name)
            if name.endswith(NID_POSTFIX):
                names.add(name[:-len(NID_POSTFIX)])
    return names


def option_name(argument, options):
    if not argument.startswith("-") or argument == "-":
        return None
    name = argument.split("=", 1)[0]
    if name in options:
        return name
    if not name.startswith("--"):
        return None
    matches = [option for option in options if option.startswith(name)]
    if len(matches) == 1:
        return matches[0]
    return name if matches else None


def nid_arguments(arguments, options):
    normalized = []
    collecting = False
    collecting_option = "--nid"
    for index, argument in enumerate(arguments):
        if argument == "--":
            normalized.extend(arguments[index:])
            break
        name = option_name(argument, options)
        if name in ("--nid", "--name", "--symbol") and "=" not in argument:
            collecting = True
            collecting_option = "--name" if name in ("--name", "--symbol") else "--nid"
            continue
        if collecting and name is None:
            normalized.append(collecting_option + "=" + argument)
            continue
        collecting = False
        normalized.append(argument)
    return normalized


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Suggest real names for Unknown NID stubs")
    parser.add_argument("--db", default=os.environ.get("ANYPS5_NID_DB"),
                        help="NID database CSV (downloaded to a cache dir by default)")
    parser.add_argument("--nid", action="append", default=None,
                        help="resolve only these NIDs instead of scanning the tree")
    parser.add_argument("--name", "--symbol", dest="name", action="append", default=None,
                        help="compute NIDs for these symbol names")
    parser.add_argument("--json", action="store_true",
                        help="emit machine-readable JSON")
    options = [option for action in parser._actions for option in action.option_strings]
    arguments = sys.argv[1:] if argv is None else argv
    args = parser.parse_args(nid_arguments(arguments, options))
    requested_options = {option_name(argument, options) for argument in arguments}
    requested_nids = "--nid" in requested_options
    if requested_nids and requested_options & {"--name", "--symbol"}:
        parser.error("--name/--symbol cannot be combined with --nid")
    if args.name:
        rows = [{"name": name, "nid": compute_nid(name)} for name in args.name]
        if args.json:
            print(json.dumps({"symbols": rows}, indent=1))
        else:
            print("%-40s %s" % ("NAME", "NID"))
            for row in rows:
                print("%-40s %s" % (row["name"], row["nid"]))
        return 0
    for nid in args.nid or []:
        if re.fullmatch(r"[A-Za-z0-9+\-]{11}", nid) is None:
            parser.error(f"invalid NID {nid!r}: expected 11 characters from A-Z, a-z, 0-9, + or -")

    db_path = Path(args.db) if args.db else DEFAULT_CACHE
    try:
        db = load_db(db_path)
        if requested_nids:
            unknowns = {nid: "<cli>" for nid in (args.nid or [])}
        else:
            unknowns = collect_unknowns()
        real_names = collect_real_names() if unknowns else set()
    except (OSError, UnicodeError) as error:
        parser.error(str(error))

    rows = []
    failures = 0
    for nid in sorted(unknowns):
        suggestion = db.get(nid, "")
        status = "unknown"
        if suggestion:
            if compute_nid(suggestion) != nid:
                status = "MISMATCH"
                failures += 1
            elif suggestion in real_names:
                status = "already-implemented"
            else:
                status = "rename-ready"
        rows.append({"nid": nid, "stub": unknowns[nid],
                     "suggestion": suggestion, "status": status})

    if args.json:
        print(json.dumps({"db_entries": len(db), "rows": rows}, indent=1))
    else:
        print("%-12s %-40s %-12s %s" % ("NID", "STUB", "STATUS", "SUGGESTION"))
        for row in rows:
            print("%-12s %-40s %-12s %s" % (
                row["nid"], row["stub"][:40], row["status"], row["suggestion"]))
        print("%d unknowns, %d suggestions (%d already implemented)" % (
            len(rows), sum(1 for r in rows if r["suggestion"]),
            sum(1 for r in rows if r["status"] == "already-implemented")))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
