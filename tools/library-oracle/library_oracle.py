import argparse
import hashlib
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PROSPER = "d44c89db977a162af271d986a8d1febbc31e702f"
SOURCE = f"https://raw.githubusercontent.com/mattias800/prosper/{PROSPER}/prosper/tests/data/console_oracle/{{}}.golden.tsv"
FAMILIES = {
    "agc": "776e875889a5c8469df54ea17e54f83563ea9bdc15c9de8bb9904be052624f8d",
    "agcbuild": "aa6cc7b818a1016c20732e4547cea8c12b0472aec681bcf11f8f9a3c897f7de3",
    "kernel": "f7f2786eab58e3ecdc4d0a063ae097799c37243a958296be903ef7c7e53828b2",
    "kernelx": "1900aee36800180e27610188a77fa5d3d73379be46905135d51263f4de0ee1a4",
    "libc": "3e0d7ee1528b3bd542a45043e3362160ffa6c039fbde8b7a5fab1eee526f7fb1",
    "nethttp": "fb116bca49642021d0a07c5344bf729b909ae06afe37ed2266d71eff0dce9a7b",
    "posix": "acaed43f5f1fc07be4dc3703dec7aa6984ae445445c3d045958a9ac74d0cc519",
    "rtc": "fd525ed88202eb4634d5608b031e72592a7f1fde0da1b0ceba2df65090ebf67d",
    "userservice": "c9f1b88cd1d113bc129dfdd62f3301f5a59ac820a8601ea64a74dfc89b39f21e",
}
KINDS = ("skip", "diff", "throw", "crash", "hang", "missing", "badspec")
FAULTS = {"hang": "crash"}


def cache_directory(environment):
    if environment.get("LIBRARY_ORACLE_CACHE"):
        return Path(environment["LIBRARY_ORACLE_CACHE"])
    xdg = environment.get("XDG_CACHE_HOME", "")
    root = Path(xdg) if xdg and Path(xdg).is_absolute() else Path.home() / ".cache"
    return root / "anyps5-library-oracle"


def fetch(family, cache, opener=urllib.request.urlopen):
    path = cache / f"{family}.golden.tsv"
    if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == FAMILIES[family]:
        return path
    with opener(SOURCE.format(family), timeout=60) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != FAMILIES[family]:
        raise SystemExit(f"{family}: the downloaded measurements do not match the pinned digest")
    cache.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def load_known(path):
    known = {}
    if not path.is_file():
        return known
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2 or fields[1] not in KINDS:
            raise SystemExit(f"{path}:{number}: expected 'id<TAB>kind<TAB>reason' with kind one of {', '.join(KINDS)}")
        known[fields[0]] = (fields[1], fields[2] if len(fields) > 2 else "")
    return known


def write_known(path, known, findings):
    rows = {key: value for key, value in known.items() if value[0] == "skip"}
    for case, (kind, _) in findings.items():
        rows[case] = (kind, known.get(case, (kind, ""))[1])
    lines = [f"{case}\t{kind}\t{reason}".rstrip("\t") for case, (kind, reason) in sorted(rows.items())]
    path.write_text("".join(line + "\n" for line in lines), encoding="utf-8", newline="\n")


def text(value):
    if value is None:
        return ""
    return value.decode("utf-8", "replace") if isinstance(value, bytes) else value


def environment(modules):
    copy = dict(os.environ)
    for name in ("PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        copy[name] = str(modules) + os.pathsep + copy.get(name, "")
    return copy


def replay(binary, modules, golden, skip, timeout, run=subprocess.run):
    skipped = set(skip)
    failures = {}
    while True:
        command = [str(binary), str(modules)] + (["--skip", ",".join(sorted(skipped))] if skipped else []) + [str(golden)]
        hung = False
        try:
            done = run(command, capture_output=True, timeout=timeout, env=environment(modules))
            code, output, errors = done.returncode, text(done.stdout), text(done.stderr)
        except subprocess.TimeoutExpired as expired:
            code, output, errors, hung = None, text(expired.stdout), text(expired.stderr), True
        findings = dict(failures)
        summary = None
        for line in output.splitlines():
            fields = line.split("\t")
            if fields[0] in ("DIFF", "MISSING", "BADSPEC") and len(fields) > 2:
                detail = "\t".join(fields[2:])
                findings[fields[1]] = ("throw" if fields[0] == "DIFF" and " threw '" in detail else fields[0].lower(), detail)
            elif fields[0] == "SUMMARY":
                summary = " ".join(fields[1:])
        if not hung and code == 0 and summary is not None:
            return findings, summary
        started = [line.split("\t", 1)[1] for line in errors.splitlines() if line.startswith("RUN\t")]
        if not started or started[-1] in skipped:
            tail = errors.strip().splitlines()[-1:] or ["no output"]
            raise SystemExit(f"{golden}: the replay failed before reaching a case: {tail[0]}")
        thrown = [line.split("what():", 1)[1].strip() for line in errors.splitlines() if "what():" in line]
        if hung:
            failures[started[-1]] = ("hang", f"timed out after {timeout} s")
        elif thrown:
            failures[started[-1]] = ("throw", thrown[-1])
        else:
            failures[started[-1]] = ("crash", f"exit code {code}")
        skipped.add(started[-1])


def classify(known, findings, ran_cases):
    new, expected, stale = [], [], []
    for case, (kind, detail) in sorted(findings.items()):
        entry = known.get(case)
        (expected if entry and FAULTS.get(entry[0], entry[0]) == FAULTS.get(kind, kind) else new).append((case, kind, detail, entry[1] if entry else ""))
    for case, (kind, reason) in sorted(known.items()):
        if kind != "skip" and case not in findings and case in ran_cases:
            stale.append((case, kind, reason))
    return new, expected, stale


def case_ids(golden):
    ids = set()
    for line in Path(golden).read_text(encoding="utf-8", errors="replace").splitlines():
        if line and not line.startswith("#"):
            ids.add(line.split("\t", 1)[0])
    return ids


def default_replay(build):
    for name in ("library_oracle_replay.exe", "library_oracle_replay"):
        found = sorted(build.rglob(name))
        if found:
            return found[0]
    raise SystemExit(f"no library_oracle_replay under {build}; build it with: cmake --build {build} --target library_oracle_replay")


def required_modules(binary, run=subprocess.run):
    try:
        done = run([str(binary), "--required-modules"], capture_output=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if done.returncode != 0:
        return None
    names = [line.split("\t", 1)[1].strip() for line in text(done.stdout).splitlines() if line.startswith("MODULE\t")]
    return names or None


def absent_modules(modules, names):
    return [name for name in names if not (Path(modules) / name).is_file()]


def module_warning(absent, modules):
    if not absent:
        return None
    return f"warning: {', '.join(absent)} is not in {modules}; a MISSING case that needs one of them is a build gap, not a missing export"


def main(argv=None):
    parser = argparse.ArgumentParser(description="replay prosper's PS5 console measurements against the built AnyPS5 modules")
    parser.add_argument("families", nargs="*", metavar="FAMILY", help=f"measurement families to replay (default: all of {', '.join(sorted(FAMILIES))})")
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--modules", type=Path, help="directory of the unpatched modules (default: BUILD/core/libs/libs/unpatched)")
    parser.add_argument("--replay", type=Path, help="the library_oracle_replay executable (default: found under BUILD)")
    parser.add_argument("--known", type=Path, default=HERE / "known.tsv")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--write-known", action="store_true", help="record every current finding in the known file, keeping existing reasons")
    args = parser.parse_args(argv)
    unknown = sorted(set(args.families) - set(FAMILIES))
    if unknown:
        parser.error(f"unknown families: {', '.join(unknown)}")
    families = args.families or sorted(FAMILIES)
    modules = args.modules or args.build / "core" / "libs" / "libs" / "unpatched"
    binary = args.replay or default_replay(args.build)
    known = load_known(args.known)
    skip = {case for case, (kind, _) in known.items() if kind == "skip"}
    cache = cache_directory(os.environ)
    names = required_modules(binary)
    absent = absent_modules(modules, names) if names else []
    findings, ran_cases, failed = {}, set(), False
    for family in families:
        golden = fetch(family, cache)
        found, summary = replay(binary, modules, golden, skip, args.timeout)
        print(f"{family}: {summary}")
        findings.update(found)
        ran_cases |= case_ids(golden) - skip
    if any(kind == "missing" for kind, _ in findings.values()):
        note = module_warning(absent, modules)
        if note:
            print(note)
    if args.write_known:
        write_known(args.known, known, findings)
        print(f"wrote {len(findings)} findings to {args.known}")
        return 0
    new, expected, stale = classify(known, findings, ran_cases)
    for case, kind, detail, reason in new:
        failed = True
        print(f"NEW {kind}\t{case}\t{detail}" + (f"\t(known as something else: {reason})" if reason else ""))
    for case, kind, reason in stale:
        print(f"NOW MATCHES\t{case}\t(known {kind}: {reason}); remove it from {args.known.name}")
    print(f"{len(expected)} known findings, {len(new)} new, {len(stale)} now matching")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
