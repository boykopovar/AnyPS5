import argparse
import json
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nid_names
import progress

OBSERVABLE = ("implemented", "partial", "stub")
AUDIT_TOOLS = ("import_audit", "audit_dump")
HEADER = "# NIDs of {library} that a measured use shows to be needed. One per line, sorted; see docs/dev/PROGRESS.md."
DESCRIPTION = """Maintain docs/dev/observed/<library>.txt, the NIDs of each library that any measured use shows to be needed.
tools/progress.py counts a pending function as an observed stub when its NID is listed there and as an unobserved
stub otherwise; a library without a file is unclassified and counts every stub as observed. 'add' merges the JSON
written by tools/import_audit.py --json (or tools/audit_dump.py --json) into the files of the libraries that provide
each imported NID, tagged audit with the tool and the date of the run; 'entry' adds one NID by hand and requires its
provenance tag: audit, loader (needed before a title's own code runs) or manual (needs a written reason); 'check'
validates the committed files, rejects unknown tags and lists the entries that have no tag."""


class ManifestError(Exception):
    pass


def libraries(prx):
    return {path.name for path in Path(prx).iterdir() if path.is_dir()} if Path(prx).is_dir() else set()


def parse(text, name="manifest"):
    header, entries = [], {}
    for number, line in enumerate(text.splitlines(), 1):
        entry, _, comment = line.partition("#")
        entry = entry.strip()
        if not entry:
            if line.strip():
                header.append(line.rstrip())
            continue
        if progress.NID.fullmatch(entry) is None:
            raise ManifestError(f"{name}:{number}: invalid NID {entry!r}")
        if comment.strip() and (entry not in entries or progress.classify_comment(entries[entry])[0] == progress.UNTAGGED):
            entries[entry] = comment.strip()
        else:
            entries.setdefault(entry, "")
    return header, entries


def render(header, entries):
    lines = list(header)
    lines += [f"{nid}  # {comment}" if comment else nid for nid, comment in sorted(entries.items())]
    return "\n".join(lines) + "\n"


def report_nids(path):
    try:
        report = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        raise ManifestError(f"{path}: not an import_audit.py JSON report: {error}") from None
    records = report.get("imports") if isinstance(report, dict) else None
    if not isinstance(records, list):
        raise ManifestError(f"{path}: not an import_audit.py JSON report: no imports list")
    found = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or record.get("class") not in OBSERVABLE:
            continue
        nid, providers = record.get("nid"), record.get("providers")
        if not isinstance(nid, str) or not isinstance(providers, list) or not all(isinstance(p, str) for p in providers):
            raise ManifestError(f"{path}: imports[{index}] has no usable nid and providers")
        if record.get("symbol_mismatch"):
            nid = nid_names.compute_nid(nid)
        if progress.NID.fullmatch(nid) is None:
            raise ManifestError(f"{path}: imports[{index}] has the malformed NID {nid!r}")
        for provider in providers:
            found.setdefault(provider.removesuffix(".prx"), set()).add(nid)
    return found


def plan(additions, prx, observed):
    known = libraries(prx)
    unknown = sorted(set(additions) - known)
    if unknown:
        raise ManifestError(f"not libraries of {prx}: {', '.join(unknown)}")
    changes = {}
    for library, nids in sorted(additions.items()):
        path = Path(observed) / f"{library}.txt"
        if path.is_file():
            header, entries = parse(path.read_text(encoding="utf-8-sig"), str(path))
        else:
            header, entries = [HEADER.format(library=library)], {}
        added = {nid: comment for nid, comment in nids.items()
                 if nid not in entries or (progress.classify_comment(entries[nid])[0] == progress.UNTAGGED and comment)}
        if added or not path.is_file():
            entries.update(added)
            changes[path] = (render(header, entries), len(added))
    return changes


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", newline="\n", dir=path.parent, prefix=".observed-", suffix=".tmp", delete=False) as handle:
        handle.write(text)
    temporary = Path(handle.name)
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def problems(prx, observed):
    found = []
    known = libraries(prx)
    for path in sorted(Path(observed).glob("*")) if Path(observed).is_dir() else []:
        if path.suffix != ".txt" or not path.is_file():
            found.append(f"{path}: not a <library>.txt file")
            continue
        if path.stem not in known:
            found.append(f"{path}: {path.stem} is not a library of {prx}")
        try:
            text = path.read_text(encoding="utf-8-sig")
        except (OSError, ValueError) as error:
            found.append(f"{path}: {error}")
            continue
        found += [f"{path}:{number}: {message}" for number, message in progress.manifest_problems(text)]
        found += [f"{path}:{number}: {message}" for number, message in progress.manifest_tag_problems(text)]
    return found


def untagged(observed):
    found = []
    for path in sorted(Path(observed).glob("*.txt")) if Path(observed).is_dir() else []:
        try:
            text = path.read_text(encoding="utf-8-sig")
        except (OSError, ValueError):
            continue
        found += [f"{path}:{number}: {entry} has no provenance tag" for number, entry in progress.manifest_untagged(text)]
    return found


def provenance(tag, reason, tool, day):
    if tag == "audit":
        if not progress.AUDIT_DETAIL.fullmatch(f"{tool} {day}"):
            raise ManifestError(f"invalid date {day!r}; use YYYY-MM-DD")
        return f"audit: {tool} {day}"
    reason = " ".join((reason or "").split())
    if tag == "manual" and not reason:
        raise ManifestError("--tag manual needs --reason")
    return f"{tag}: {reason}" if reason else tag


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--prx", type=Path, default=progress.PRX, help="core/libs/prx tree that names the libraries")
    parser.add_argument("--observed", type=Path, default=progress.OBSERVED, help="directory of the manifests (default: docs/dev/observed)")
    commands = parser.add_subparsers(dest="command", required=True)
    add = commands.add_parser("add", help="merge import_audit.py JSON reports into the manifests")
    add.add_argument("reports", nargs="+", type=Path)
    add.add_argument("--check", action="store_true", help="write nothing; fail when a manifest would change")
    add.add_argument("--tool", choices=AUDIT_TOOLS, default="import_audit", help="tool that wrote the reports, recorded in the audit tag")
    add.add_argument("--date", default=datetime.now(timezone.utc).date().isoformat(), help="UTC date of the audit run, YYYY-MM-DD (default: today)")
    entry = commands.add_parser("entry", help="add one NID by hand")
    entry.add_argument("library")
    entry.add_argument("nid")
    entry.add_argument("--tag", required=True, choices=progress.TAGS, help="provenance: audit, loader or manual")
    entry.add_argument("--reason", help="written reason; required for manual, optional for loader")
    entry.add_argument("--tool", choices=AUDIT_TOOLS, default="import_audit", help="for --tag audit: the tool that was run")
    entry.add_argument("--date", default=datetime.now(timezone.utc).date().isoformat(), help="for --tag audit: UTC date of the run")
    commands.add_parser("check", help="validate the committed manifests")
    args = parser.parse_args(argv)
    try:
        if args.command == "check":
            found = problems(args.prx, args.observed)
            notes = untagged(args.observed)
            for message in found + notes:
                print(message, file=sys.stderr)
            if notes:
                print(f"{len(notes)} entries have no provenance tag")
            print(f"{len(found)} problems in {args.observed}" if found else f"{args.observed}: all manifests are valid")
            return 1 if found else 0
        if args.command == "entry":
            if progress.NID.fullmatch(args.nid) is None:
                raise ManifestError(f"invalid NID {args.nid!r}")
            additions = {args.library: {args.nid: provenance(args.tag, args.reason, args.tool, args.date)}}
        else:
            additions = {}
            for report in args.reports:
                for library, nids in report_nids(report).items():
                    additions.setdefault(library, {}).update(dict.fromkeys(nids, provenance("audit", None, args.tool, args.date)))
        changes = plan(additions, args.prx, args.observed)
    except ManifestError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"FAIL: {error.strerror}: {error.filename}" if error.filename else f"FAIL: {error}", file=sys.stderr)
        return 2
    dry = getattr(args, "check", False)
    for path, (text, count) in changes.items():
        print(f"{path}: {'would add' if dry else 'added'} {count} NIDs")
        if not dry:
            write(path, text)
    if not changes:
        print("manifests already list every observed NID")
    return 1 if dry and changes else 0


if __name__ == "__main__":
    raise SystemExit(main())
