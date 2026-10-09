#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path

try:
    from tools.nid_names import CHARSET, SUFFIX, compute_nid
except ImportError:
    from nid_names import CHARSET, SUFFIX, compute_nid


def strip_nid_token(raw):
    s = raw.lstrip("\ufeff").strip()
    if not s:
        return ""
    if s.startswith("#"):
        return ""
    if "#" in s:
        s = s.split("#", 1)[0].strip()
    if not s:
        return ""
    return s.split()[0]


def parse_nids(path):
    nids = []
    seen = set()
    with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            nid = strip_nid_token(line)
            if not nid:
                continue
            if nid in seen:
                continue
            seen.add(nid)
            nids.append(nid)
    return nids


def parse_db(path):
    mapping = {}
    with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
        for line in f:
            s = line.lstrip("\ufeff").strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split()
            if len(parts) < 2:
                continue
            nid = parts[0].split("#", 1)[0].strip().lstrip("\ufeff")
            name = parts[1].split("#", 1)[0].strip().lstrip("\ufeff")
            if not nid or not name:
                continue
            if nid not in mapping:
                mapping[nid] = name
    return mapping


def sanitize_identifier(name):
    s = re.sub(r"[^0-9A-Za-z_]", "_", name)
    if not s:
        return "_"
    if s[0].isdigit():
        s = "_" + s
    return s


def module_prefix(module):
    base = module
    if base.startswith("lib") and len(base) > 3 and base[3].isupper():
        base = base[3:]
        base = base[0].lower() + base[1:] if base else module
    return sanitize_identifier(base)


def is_runtime_name(name):
    if name.startswith("_"):
        return True
    if name.startswith("sce") or name.startswith("Sce"):
        return False
    return True


def is_valid_ident(name):
    return re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name) is not None


def _default_td_path(module):
    candidates = [
        "core/libs/prx/" + module + "/Export.cpp",
        "core/libs/prx/" + module + "/Unimplemented.cpp",
        "core/libs/prx/" + module + "/src/Unimplemented.cpp",
    ]
    root = Path(__file__).resolve().parent.parent
    for cand in candidates:
        try:
            if (root / cand).is_file():
                return cand
        except OSError:
            continue
    return candidates[0]


def td_target(module, td_path, td_lib):
    if td_path is None:
        td_path = _default_td_path(module)
    if td_lib is None:
        td_lib = module
    return (td_path, td_lib)


def generate(module, nids, db, td_path=None, td_lib=None):
    prefix = module_prefix(module)
    path, lib = td_target(module, td_path, td_lib)
    used = set()
    unknown_counter = 0
    chunks = []
    td = []
    for nid in nids:
        real = db.get(nid)
        func = None
        display = None
        if real is not None:
            if is_valid_ident(real):
                if compute_nid(real) == nid:
                    if is_runtime_name(real):
                        candidate = real + "_nid_postfix"
                    else:
                        candidate = real
                    if candidate not in used:
                        func = candidate
                        display = real
        if func is None:
            while True:
                func = "%sUnknown%02d" % (prefix, unknown_counter)
                unknown_counter += 1
                if func not in used:
                    break
            display = None
        used.add(func)
        if display is not None:
            chunks.append("")
            chunks.append("int APS5_VABI %s(void) {" % func)
            chunks.append('    NotImplemented_nid_no_patch("%s");' % nid)
            chunks.append("    return 0;")
            chunks.append("}")
            td.append("- [%s](%s) (%s) - unknown signature" % (display, path, lib))
        else:
            chunks.append("")
            chunks.append('APS5_EXPORT("%s", %s);' % (nid, func))
            chunks.append("int APS5_VABI %s(void) {" % func)
            chunks.append('    NotImplemented_nid_no_patch("%s");' % nid)
            chunks.append("    return 0;")
            chunks.append("}")
            td.append("- [%s](%s) (%s) - unknown name, signature" % (nid, path, lib))
    header = []
    header.append("#include <cstdint>")
    header.append("#include <cstddef>")
    header.append('#include "SceTypes.hpp"')
    header.append('#include "prx/libc/include/General.hpp"')
    header.append("")
    header.append('extern "C" {')
    footer = ["", "}"]
    code = "\n".join(header + chunks + footer) + "\n"
    td_text = "\n".join(td) + ("\n" if td else "")
    return (code, td_text)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Generate AnyPS5 Export.cpp skeletons and TechnicalDebt lines")
    ap.add_argument("--module", required=True, help="target module name, e.g. libSceAmpr, used for UnknownNN prefix and default TechnicalDebt path core/libs/prx/MODULE/Export.cpp")
    ap.add_argument("--nids", required=True, help="path to missing-NIDs file, one 11-char NID per line")
    ap.add_argument("--db", required=False, default=None, help="path to NID name CSV, NID name per line like aerolib.csv")
    ap.add_argument("--out", required=False, default=None, help="output file for code, default stdout")
    ap.add_argument("--td-path", required=False, default=None, help="override file path used inside TechnicalDebt lines, default derived from module")
    ap.add_argument("--td-lib", required=False, default=None, help="override lib label used inside TechnicalDebt lines, default derived from module")
    ap.add_argument("--td-out", required=False, default=None, help="output file for TechnicalDebt lines, default stdout")
    ap.add_argument("--td-only", action="store_true", help="print only TechnicalDebt lines, no code")
    args = ap.parse_args(argv)
    nids = parse_nids(args.nids)
    db = {}
    if args.db:
        db = parse_db(args.db)
    code, td = generate(args.module, nids, db, args.td_path, args.td_lib)
    if args.td_only:
        if args.td_out:
            with open(args.td_out, "w", encoding="utf-8", newline="\n") as f:
                f.write(td)
        else:
            sys.stdout.write(td)
        return 0
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(code)
    else:
        sys.stdout.write(code)
    if args.td_out:
        with open(args.td_out, "w", encoding="utf-8", newline="\n") as f:
            f.write(td)
    else:
        if args.out:
            sys.stdout.write(td)
        else:
            if td:
                sys.stdout.write("\n")
                sys.stdout.write(td)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
