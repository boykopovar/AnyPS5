#!/usr/bin/env python3
"""Report the console system library exports an AnyPS5 library does not provide.

Reads the export NIDs of the console library from a genstub style export table
or from a file of NIDs, intersects them with the pinned NID name database and
with what the AnyPS5 library already defines, and writes the missing NIDs in
the format tools/stubgen.py --nids takes.
"""
import argparse
import re
import sys
from pathlib import Path

import nid_names

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
POSTFIXES = ("_nid_postfix", "_nid_no_patch_cut", "_nid_no_patch")
NID_PATTERN = "[A-Za-z0-9+\\-]{11}"
VABI = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
TABLE_ENTRY = re.compile(r"sprx_dlsym\s*\(\s*__handle\s*,\s*\"(" + NID_PATTERN + r")\"")
DEFINE = re.compile(r"#define\s+(\w+)\s*\(\s*(\w+)\s*\)([^\n]*)")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
MODULE_PATTERN = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]*\Z")


def is_mangled(name):
    return name.startswith("_Z") and len(name) > 2 and name[2].isdigit()


def helper_reason(name):
    if is_mangled(name):
        return "is a C++ mangled name"
    if IDENTIFIER.match(name) is None:
        return "is not a valid C identifier"
    return None


def is_safe_module(name):
    if not name or name in (".", ".."):
        return False
    if "/" in name or "\\" in name:
        return False
    if ".." in name:
        return False
    return MODULE_PATTERN.match(name) is not None


class GapError(Exception):
    pass


def strip_token(raw):
    text = raw.lstrip("\ufeff").strip()
    if not text:
        return ""
    if text.startswith("#"):
        return ""
    if "#" in text:
        text = text.split("#", 1)[0].strip()
    if not text:
        return ""
    return text.split()[0]


def parse_nids_file(path):
    nids = []
    seen = set()
    try:
        content = Path(path).read_text(encoding="utf-8-sig", errors="replace")
    except OSError as error:
        raise GapError(f"{path}: cannot read NID file: {error.strerror or error}") from error
    for number, line in enumerate(content.split("\n"), 1):
        token = strip_token(line)
        if not token:
            continue
        if re.fullmatch(NID_PATTERN, token) is None:
            raise GapError(f"{path}:{number}: invalid NID {token!r}")
        if token not in seen:
            seen.add(token)
            nids.append(token)
    return nids


def parse_export_table(path):
    try:
        text = Path(path).read_text(encoding="utf-8-sig", errors="replace")
    except OSError as error:
        raise GapError(f"{path}: cannot read export table: {error.strerror or error}") from error
    nids = []
    seen = set()
    for match in TABLE_ENTRY.finditer(text):
        nid = match.group(1)
        if nid not in seen:
            seen.add(nid)
            nids.append(nid)
    return nids


def mask_code(text):
    def blank(match):
        if match.group("code") is not None:
            return match.group()
        return re.sub(r"[^\n]", " ", match.group())

    return nid_names.CPP_TOKEN.sub(blank, text)


def joined_lines(text):
    return text.replace("\\\r\n", "").replace("\\\n", "")


def base_of(name):
    for postfix in POSTFIXES:
        if name.endswith(postfix) and len(name) > len(postfix):
            return name[: -len(postfix)], postfix
    return name, None


def source_texts(module_dir):
    texts = []
    for path in sorted(module_dir.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix.lower() not in (".cpp", ".c", ".cc", ".h", ".hpp", ".hh"):
            continue
        try:
            texts.append((path, path.read_text(encoding="utf-8-sig", errors="replace")))
        except OSError as error:
            raise GapError(f"{path}: cannot read source: {error.strerror or error}") from error
    return texts


def same_file(left, right):
    left, right = Path(left), Path(right)
    return left.resolve() == right.resolve() or (left.exists() and right.exists() and left.samefile(right))


def macro_definitions(texts):
    macros = {}
    for _, text in texts:
        for match in DEFINE.finditer(joined_lines(text)):
            macro, param, body = match.group(1), match.group(2), match.group(3)
            if re.search(r"\bAPS5_VABI\s+" + re.escape(param) + r"\b", body) is not None:
                macros.setdefault(macro, param)
    return macros


def collect_provided(module_dir):
    texts = source_texts(module_dir)
    macros = macro_definitions(texts)
    provided = {}
    for _, text in texts:
        for line in mask_code(joined_lines(text)).split("\n"):
            if line.lstrip().startswith("#"):
                continue
            for match in VABI.finditer(line):
                name = match.group(1)
                base, postfix = base_of(name)
                nid = nid_names.compute_nid(base)
                if nid not in provided:
                    if postfix is None:
                        provided[nid] = f"under its own name {name}"
                    else:
                        provided[nid] = f"under {name} for {base}"
        for nid, helper in nid_names.source_aliases(text):
            if nid not in provided:
                provided[nid] = f"bound with APS5_EXPORT as {helper}"
    for macro in sorted(macros):
        call = re.compile(r"\b" + re.escape(macro) + r"\s*\(\s*(?:::)?(?:\w+::)*(\w+)\s*\)")
        for path, text in texts:
            masked = mask_code(text).split("\n")
            originals = text.split("\n")
            for index in range(len(masked)):
                if originals[index].lstrip().startswith("#"):
                    continue
                for match in call.finditer(masked[index]):
                    symbol = match.group(1)
                    base, _ = base_of(symbol)
                    nid = nid_names.compute_nid(base)
                    if nid not in provided:
                        provided[nid] = f"through macro {macro} as {symbol}"
    return provided


def find_self_calls(module_dir, missing):
    if not missing:
        return []
    names = sorted(set(missing.values()))
    alts = "|".join(re.escape(name) for name in names)
    call = re.compile(r"(?<![\w:])(::)?\b(" + alts + r")\s*\(")
    calls = []
    for path, text in source_texts(module_dir):
        joined = joined_lines(text)
        masked = mask_code(joined)
        rows = joined.split("\n")
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            rel = path.as_posix()
        declared = {}
        for match in VABI.finditer(masked):
            declared.setdefault(match.start(1), match.group(1))
        for match in call.finditer(masked):
            qual = match.group(1) or ""
            name = match.group(2)
            start = match.start(2)
            if declared.get(start) == name:
                continue
            number = masked.count("\n", 0, match.start()) + 1
            if rows[number - 1].lstrip().startswith("#"):
                continue
            nid = next(key for key, value in missing.items() if value == name)
            calls.append((name, nid, rel, number, qual))
    calls.sort(key=lambda item: (item[0], item[2], item[3], item[4]))
    return calls


def load_console_nids(modules, export_table, nids_file):
    if export_table is not None:
        table_dir = Path(export_table)
        if not table_dir.is_dir():
            raise GapError(f"{table_dir}: not a directory")
        console = {}
        for module in modules:
            table = table_dir / (module + ".c")
            if not table.is_file():
                raise GapError(f"{table}: export table not found")
            console[module] = parse_export_table(table)
        return console
    nids = parse_nids_file(nids_file)
    return {module: list(nids) for module in modules}


def report_module(module, console_nids, provided, db):
    missing = []
    already = []
    unknown = []
    for nid in console_nids:
        if nid in provided:
            already.append((nid, db.get(nid), provided[nid]))
        elif nid not in db:
            unknown.append(nid)
        else:
            missing.append((nid, db[nid]))
    lines = [f"{module}: {len(missing)} missing, {len(already)} already provided, {len(unknown)} unknown"]
    for nid, name in missing:
        reason = helper_reason(name)
        if reason is None:
            lines.append(f"missing {nid} {name}")
        else:
            lines.append(f"{module}: {name} ({nid}) {reason} and needs a renamed helper bound with APS5_EXPORT, like libSceAmpr")
    for nid, name, how in already:
        if name is None:
            lines.append(f"already provided {nid} {how}")
        else:
            lines.append(f"already provided {nid} {name} {how}")
    for nid in unknown:
        lines.append(f"unknown {nid} not in database, left out")
    return missing, already, unknown, lines


def normalize_module(name):
    if name.lower().endswith(".prx") and len(name) > 4:
        return name[: -len(".prx")]
    return name


def main(argv=None):
    parser = argparse.ArgumentParser(description="report the console exports an AnyPS5 library does not provide")
    parser.add_argument("--module", action="append", default=[], help="AnyPS5 library to report on, e.g. libScePad; repeat for several")
    parser.add_argument("--export-table", default=None, help="directory of genstub style export tables, one <module>.c file per library")
    parser.add_argument("--nids", default=None, help="file with one export NID per line, an alternative to --export-table")
    parser.add_argument("--out", default=None, help="write the missing NIDs to this file; without it they go to standard output")
    parser.add_argument("--db", default=None, help="NID name database file; the pinned database is used by default")
    args = parser.parse_args(argv)
    try:
        modules = [normalize_module(name) for name in args.module]
        if not modules:
            raise GapError("--module is required")
        if (args.export_table is None) == (args.nids is None):
            raise GapError("exactly one of --export-table and --nids is required")
        for module in modules:
            if not is_safe_module(module):
                raise GapError(f"{module}: invalid module name")
            if not (PRX / module).is_dir():
                raise GapError(f"{PRX / module}: no such library")
        if args.out is not None:
            for source in filter(None, [args.nids, args.db]):
                if same_file(args.out, source):
                    raise GapError(f"output {args.out!r} would overwrite input {source!r}")
        try:
            db = nid_names.load_db(Path(args.db) if args.db else nid_names.DEFAULT_CACHE)
        except (OSError, UnicodeError) as error:
            raise GapError(f"{args.db or nid_names.DEFAULT_CACHE}: cannot read NID database: {error}") from error
        console = load_console_nids(modules, args.export_table, args.nids)
        report = []
        missing_nids = []
        seen = set()
        for module in modules:
            provided = collect_provided(PRX / module)
            missing, _, _, lines = report_module(module, console[module], provided, db)
            missing_names = {nid: name for nid, name in missing}
            calls = find_self_calls(PRX / module, missing_names)
            report.extend(lines)
            for name, nid, rel, line, qual in calls:
                if qual:
                    report.append(f"{module}: {name} ({nid}) called at {rel}:{line} {qual}{name} needs a renamed helper bound with APS5_EXPORT, like libSceAmpr")
                else:
                    report.append(f"{module}: {name} ({nid}) called at {rel}:{line} needs a renamed helper bound with APS5_EXPORT, like libSceAmpr")
            for nid, _ in missing:
                if nid not in seen:
                    seen.add(nid)
                    missing_nids.append(nid)
        if args.out is not None:
            destination = Path(args.out)
            if destination.exists() and destination.is_dir():
                raise GapError(f"{destination}: output is a directory")
            try:
                if missing_nids:
                    destination.write_text("".join(nid + "\n" for nid in missing_nids), encoding="utf-8", newline="\n")
                else:
                    destination.write_text("", encoding="utf-8", newline="\n")
            except OSError as error:
                raise GapError(f"{destination}: cannot write output: {error.strerror or error}") from error
        else:
            for nid in missing_nids:
                print(nid)
        for line in report:
            print(line)
    except GapError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"FAIL: {error.strerror}: {error.filename}" if error.filename else f"FAIL: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    raise SystemExit(main())
