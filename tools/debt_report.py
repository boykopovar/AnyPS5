import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress
from check_conventions import DEBT, DEBT_DIR, DEBT_LABELS, DEBT_TARGET, debt_entries, debt_labels

ROOT = Path(__file__).resolve().parent.parent
SOURCES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
LINK = re.compile(r"\[([^\]]+)\]\((?:\.\./)+core/[^)#\s]+")
TICKED = re.compile(r"`([a-z_][A-Za-z0-9_]*)`")
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SEPARATOR = re.compile(r",\s*|\s+and\s+|\s+or\s+")
CLOSES = re.compile(r"Closes when:(.*)", re.S)
CLOSERS = (
    ("console", re.compile(r"\b(?:measured|measurement)\b[^.]*\b(?:PS5|console)\b|\bunmeasured\b|\bnot measured\b", re.I)),
    ("firmware", re.compile(r"firmware|disassembl|\.sprx\b", re.I)),
    ("gpu", re.compile(r"\b(?:RDNA ?\d?|GCN|NVIDIA|Intel Arc|MoltenVK|RADV|lavapipe|Radeon|GeForce|RTX)\b")),
)
TITLE_CLOSE = re.compile(r"\b(?:PPSA|CUSA)\d{5}\b|\btitles?\b", re.I)
TITLE_GAP = re.compile(r"\bno (?:game )?title\b|\bnot verified with a title\b", re.I)
PLATFORMS = (("windows", re.compile(r"\bWindows\b")), ("macos", re.compile(r"\bmacOS\b")), ("linux", re.compile(r"\bLinux\b")))
THROWS = re.compile(r"\bthrows?\b|NotImplemented|\bunimplemented\b|\bnot implemented\b", re.I)
THROW = re.compile(r"\bthrow\b")


@dataclass
class Entry:
    file: str
    line: int
    section: str | None
    text: str


def debt_files(root, libraries=None):
    files = [root / DEBT] + sorted((root / DEBT_DIR).glob("*.md"))
    return [path for path in files if path.is_file() and (not libraries or path.stem in libraries)]


def entries(root, libraries=None):
    found = []
    for path in debt_files(root, libraries):
        text = path.read_text(encoding="utf-8")
        headings = [(number, line[4:].strip()) for number, line in enumerate(text.split("\n"), 1) if line.startswith("### ")]
        for number, entry in debt_entries(text).items():
            section = next((title for start, title in reversed(headings) if start < number), None)
            found.append(Entry(path.relative_to(root).as_posix(), number, section, entry))
    return found


def subjects(text):
    found = []
    for label in LINK.findall(text):
        found += [piece for piece in SEPARATOR.split(label.replace("`", "").strip()) if WORD.fullmatch(piece)]
    return list(dict.fromkeys(found))


def entry_names(text):
    return list(dict.fromkeys(subjects(text) + TICKED.findall(text)))


def closer(text):
    closes = CLOSES.search(text)
    scope = closes.group(1) if closes else text
    for label, pattern in CLOSERS:
        if pattern.search(scope):
            return label
    return "title" if (TITLE_CLOSE if closes else TITLE_GAP).search(scope) else "code"


def platforms(text):
    found = [label for label, pattern in PLATFORMS if pattern.search(text)]
    return found if len(found) < len(PLATFORMS) else []


def test_words(root):
    words = set()
    for path in (root / "core").rglob("*"):
        if path.suffix in SOURCES | {".py"} and "tests" in path.relative_to(root).parts and path.is_file():
            words.update(WORD.findall(path.read_text(encoding="utf-8", errors="ignore")))
    return words


def untested(text, words):
    found = entry_names(text)
    return None if not found else not any(name in words for name in found)


@lru_cache(maxsize=None)
def file_definitions(path):
    text = progress.code(path.read_text(encoding="utf-8", errors="ignore"))
    bodies = {}
    for match in progress.DEFINITION.finditer(text):
        start = match.end() - 1
        bodies[match.group(1)] = text[start:progress.body_end(text, start) + 1]
    return bodies


def definitions(root, paths, wanted):
    bodies = {}
    for relative in paths:
        path = root / relative
        if path.is_file() and path.suffix in SOURCES:
            bodies.update({name: body for name, body in file_definitions(path).items() if name in wanted})
    return bodies


def stale(entry, root):
    paths = DEBT_TARGET.findall(entry.text)
    missing = [path for path in paths if not (root / path).exists()]
    if missing:
        return f"missing link {missing[0]}"
    bodies = definitions(root, paths, set(subjects(entry.text)))
    throwing = {name: progress.STUB in body or THROW.search(body) is not None for name, body in bodies.items()}
    if entry.section == "Silent stubs":
        now = [name for name, value in throwing.items() if value]
        return f"stub now throws: {now[0]}" if now else None
    if THROWS.search(entry.text):
        quiet = [name for name, value in throwing.items() if not value]
        return f"no longer throws: {quiet[0]}" if quiet else None
    return None


def labels(entry, root, words):
    explicit = set(debt_labels(entry.text))
    chosen = {"closer": [closer(entry.text)], "test": ["no-test"] if untested(entry.text, words) else [], "platform": platforms(entry.text)}
    for family, vocabulary in DEBT_LABELS.items():
        given = [label for label in vocabulary if label in explicit]
        if given:
            chosen[family] = [label for label in given if label != "tested"]
    found = [label for family in DEBT_LABELS for label in chosen[family]]
    reason = stale(entry, root)
    return (found + ["stale?"] if reason else found), reason


def report(root, libraries=None):
    words = test_words(root)
    rows = []
    for entry in entries(root, libraries):
        found, reason = labels(entry, root, words)
        rows.append({"file": entry.file, "line": entry.line, "section": entry.section, "names": entry_names(entry.text), "labels": found, "reason": reason})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description="Label technical debt entries: who can close them (console, firmware, gpu, title or code), no-test when no test names what they name, stale? when the code may no longer match them, and the platforms they name. A Labels: trailer on an entry replaces the inferred labels of its family. A report, not a gate: it always exits 0.")
    parser.add_argument("libraries", nargs="*", help="debt file names without .md, such as libkernel or TechnicalDebt (default: all)")
    parser.add_argument("--label", action="append", default=[], help="show only entries with this label; repeat to require several")
    parser.add_argument("--json", action="store_true", help="print the rows as JSON")
    args = parser.parse_args(argv)
    known = {path.stem for path in debt_files(ROOT)}
    missing = [name for name in args.libraries if name not in known]
    if missing:
        parser.error(f"no debt file: {', '.join(missing)}")
    rows = [row for row in report(ROOT, set(args.libraries) or None) if all(label in row["labels"] for label in args.label)]
    if args.json:
        print(json.dumps(rows, indent=2))
        return 0
    for row in rows:
        reason = f" ({row['reason']})" if row["reason"] else ""
        print(f"{row['file']}:{row['line']}: {', '.join(row['names'][:3]) or '-'}: {', '.join(row['labels'])}{reason}")
    counts = Counter(label for row in rows for label in row["labels"])
    print(f"{len(rows)} entries: " + ", ".join(f"{count} {label}" for label, count in counts.most_common()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
