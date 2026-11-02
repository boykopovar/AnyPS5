#!/usr/bin/env python3
import argparse
import errno
import os
import re
import stat
import sys
import tempfile
from pathlib import Path

try:
    from tools.nid_names import SUFFIX, compute_nid
except ImportError:
    from nid_names import SUFFIX, compute_nid


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
        for number, line in enumerate(f, 1):
            nid = strip_nid_token(line)
            if not nid:
                continue
            if re.fullmatch(r"[A-Za-z0-9+\-]{11}", nid) is None:
                raise ValueError(f"{path}:{number}: invalid NID {nid!r}")
            if nid in seen:
                continue
            seen.add(nid)
            nids.append(nid)
    return nids


def parse_db(path):
    mapping = {}
    with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
        for number, line in enumerate(f, 1):
            s = line.lstrip("\ufeff").strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split()
            if len(parts) < 2:
                raise ValueError(f"{path}:{number}: expected a NID and a symbol name, got {s!r}")
            nid = parts[0].split("#", 1)[0].strip().lstrip("\ufeff")
            name = parts[1].split("#", 1)[0].strip().lstrip("\ufeff")
            if not nid or not name:
                raise ValueError(f"{path}:{number}: expected a NID and a symbol name, got {s!r}")
            if re.fullmatch(r"[A-Za-z0-9+\-]{11}", nid) is None:
                raise ValueError(f"{path}:{number}: invalid NID {nid!r}")
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
        td_path = "../../../" + _default_td_path(module)
    if td_lib is None:
        td_lib = module
    return (td_path, td_lib)


def generate(module, nids, db, td_path=None, td_lib=None):
    nids = tuple(nids)
    module = re.sub(r"\.prx$", "", module, flags=re.IGNORECASE)
    prefix = module_prefix(module)
    path, lib = td_target(module, td_path, td_lib)
    known = {}
    for nid in nids:
        real = db.get(nid)
        if real is not None and is_valid_ident(real) and compute_nid(real) == nid:
            candidate = real + "_nid_postfix" if is_runtime_name(real) else real
            known[nid] = (real, candidate)
    reserved = {candidate for _, candidate in known.values()}
    used = set()
    unknown_counter = 0
    chunks = []
    td = []
    for nid in nids:
        func = None
        display = None
        if nid in known:
            real, candidate = known[nid]
            if candidate not in used:
                func = candidate
                display = real
        if func is None:
            while True:
                func = "%sUnknown%02d" % (prefix, unknown_counter)
                unknown_counter += 1
                if func not in used and func not in reserved:
                    break
            display = None
        used.add(func)
        if display is not None:
            chunks.append("")
            chunks.append("int APS5_VABI %s(void) {" % func)
            chunks.append("    NotImplemented_nid_no_patch(__func__);")
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


def case_insensitive_directory(directory):
    directory = Path(directory)
    while not directory.is_dir():
        if directory.parent == directory:
            return False
        directory = directory.parent
    try:
        handle, name = tempfile.mkstemp(dir=str(directory))
    except OSError:
        return False
    try:
        os.close(handle)
        flipped = Path(name).with_name(Path(name).name.swapcase())
        return flipped.name != Path(name).name and flipped.exists()
    finally:
        os.unlink(name)


LOOP_MESSAGE = "Too many levels of symbolic links"


def resolved_path(path):
    original = path = Path(path)
    for _ in range(40):
        if not path.is_symlink():
            break
        target = Path(link_target(os.readlink(path)))
        path = target if target.is_absolute() else path.parent / target
    if path.is_symlink():
        raise OSError(errno.ELOOP, LOOP_MESSAGE, str(original))
    try:
        return path.resolve()
    except RuntimeError as error:
        raise OSError(errno.ELOOP, LOOP_MESSAGE, str(original)) from error


def link_target(text):
    if os.name != "nt":
        return text
    if text.startswith("\\\\?\\UNC\\"):
        return "\\\\" + text[8:]
    if text.startswith("\\\\?\\"):
        return text[4:]
    return text


def same_file(left, right):
    left, right = resolved_path(left), resolved_path(right)
    if str(left) == str(right):
        return True
    if left.exists() and right.exists() and left.samefile(right):
        return True
    if str(left).casefold() == str(right).casefold():
        return case_insensitive_directory(left.parent)
    return False


def write_outputs(outputs):
    staged = []
    umask = os.umask(0)
    os.umask(umask)
    try:
        for filename, content in outputs:
            path = Path(filename).resolve()
            if path.is_dir():
                raise IsADirectoryError(str(path))
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=path.parent, prefix=".stubgen-", suffix=".tmp", delete=False) as handle:
                temporary = Path(handle.name)
                staged.append((temporary, path))
                handle.write(content)
            if path.exists():
                temporary.chmod(path.stat().st_mode & 0o7777)
            else:
                temporary.chmod(0o666 & ~umask)
        if os.name == "nt":
            for _, path in staged:
                if path.exists() and path.stat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
                    raise PermissionError(f"Output is read-only: {path}")
        for temporary, path in staged:
            temporary.replace(path)
    finally:
        for temporary, path in staged:
            try:
                temporary.unlink(missing_ok=True)
            except PermissionError:
                temporary.chmod(0o600)
                temporary.unlink(missing_ok=True)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace", newline="\n")
    ap = argparse.ArgumentParser(description="Generate AnyPS5 Export.cpp skeletons and TechnicalDebt lines")
    ap.add_argument("--module", required=True, help="target module name, e.g. libSceAmpr, used for UnknownNN prefix and default TechnicalDebt path core/libs/prx/MODULE/Export.cpp")
    ap.add_argument("--nids", required=True, help="path to missing-NIDs file, one 11-char NID per line")
    ap.add_argument("--db", required=False, default=None, help="path to NID name CSV, NID name per line like aerolib.csv")
    ap.add_argument("--out", required=False, default=None, help="output file for code, default stdout")
    ap.add_argument("--td-path", required=False, default=None, help="override file path used inside TechnicalDebt lines, default derived from module, relative to docs/dev/debt")
    ap.add_argument("--td-lib", required=False, default=None, help="override lib label used inside TechnicalDebt lines, default derived from module")
    ap.add_argument("--td-out", required=False, default=None, help="output file for TechnicalDebt lines, which go in docs/dev/debt/MODULE.md, default stderr when code uses stdout, otherwise stdout")
    ap.add_argument("--td-only", action="store_true", help="print only TechnicalDebt lines, no code")
    args = ap.parse_args(argv)
    try:
        if not args.td_only and args.out and args.td_out and same_file(args.out, args.td_out):
            ap.error("--out and --td-out must name different files")
        destinations = [args.td_out] if args.td_only else [args.out, args.td_out]
        for destination in filter(None, destinations):
            for source in filter(None, [args.nids, args.db]):
                if same_file(destination, source):
                    ap.error(f"output {destination!r} would overwrite input {source!r}")
        nids = parse_nids(args.nids)
        db = parse_db(args.db) if args.db else {}
        code, td = generate(args.module, nids, db, args.td_path, args.td_lib)
        outputs = []
        if not args.td_only and args.out:
            outputs.append((args.out, code))
        if args.td_out:
            outputs.append((args.td_out, td))
        write_outputs(outputs)
    except (OSError, ValueError) as error:
        ap.error(str(error))
    if args.td_only:
        if not args.td_out:
            sys.stdout.write(td)
        return 0
    if not args.out:
        sys.stdout.write(code)
    if not args.td_out:
        (sys.stdout if args.out else sys.stderr).write(td)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
