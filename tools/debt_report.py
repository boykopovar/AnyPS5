import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_conventions import DEBT, DEBT_DIR, DEBT_LABELS, DEBT_TARGET, debt_entries, debt_labels

ROOT = Path(__file__).resolve().parent.parent
SOURCES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
LINK = re.compile(r"\[([^\]]+)\]\((?:\.\./)+core/[^)#\s]+")
TICKED = re.compile(r"`([a-z_][A-Za-z0-9_]*)`")
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SEPARATOR = re.compile(r",\s*|\s+and\s+|\s+or\s+")
HELPER_SUFFIX = re.compile(r"_nid_(?:postfix|no_patch(?:_cut)?)$")
PLAIN = re.compile(r"[a-z]+")
EXPORTED = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
POSTFIXED = re.compile(r"\b(\w+?)_nid_postfix\b")
GENERIC = {"int", "void", "char", "bool", "float", "double", "size_t", "ssize_t", "off_t", "uintptr_t", "intptr_t",
           "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
           "nullptr", "true", "false", "errno", "main", "this"}
CLOSES = re.compile(r"Closes when:(.*)", re.S)
GPU_NAME = r"(?:an? )?(?:RADV|AMD|NVIDIA|Intel|MoltenVK|lavapipe|Radeon|RTX|GeForce|RDNA ?\d?|GCN)"
CLOSERS = (
    ("gpu", re.compile(rf"\bother (?:GPUs|drivers|devices|vendors)\b|\bfails on {GPU_NAME}|\bonly (?:measured|tested|verified) on {GPU_NAME}|\b(?:not|never) (?:measured|tested|verified) on {GPU_NAME}")),
    ("firmware", re.compile(r"\bunknown signatures?\b|\bsignatures?(?: and behaviou?r)? (?:are |is )?(?:unknown|unverified)\b|\bprototypes?(?: and behaviou?r)? (?:are |is )?(?:unknown|unverified)\b"
                            r"|\bno credible prototype|\bparameter lists?\b[^.]*\b(?:unverified|unknown)\b|\bimport[- ]resolution\b|\bdeclared without parameters\b|\bparameterless\b"
                            r"|\bunidentified export\b|\bsignature\b[^.]*\binferred from\b|\bmeanings? (?:remain |are |is )?(?:unresolved|unknown)\b", re.I)),
    ("console", re.compile(r"\b(?:measured|measurement)\b[^.]*\b(?:PS5|console)\b|\bunmeasured\b|\bnot (?:been )?measured\b|\bunverified on (?:a |the )?(?:PS5|console)\b"
                           r"|\b(?:not|never) (?:confirmed|verified|checked) on (?:a |the )?(?:PS5|console|hardware)\b|\bnothing is checked on the console\b|\bno console\b"
                           r"|\bconsole's\b[^.]*\b(?:not known|unknown|unverified)\b|\b(?:not known|unknown|unresolved|unverified)\b[^.]*\bon (?:a |the )?console\b"
                           r"|\bmeasured in an emulator\b|\bneither documented nor measured\b|\bunverified console\b|\buncited assumption\b", re.I)),
    ("firmware", re.compile(r"\bneeds? (?:the |a )?(?:firmware|module|disassembly)\b|\bfirmware (?:module )?readings?\b|\bfrom the firmware module\b|\bread from the module\b", re.I)),
)
TITLE_CLOSE = re.compile(r"\b(?:PPSA|CUSA)\d{5}\b|\btitles?\b", re.I)
TITLE_GAP = re.compile(r"\bno (?:game )?title (?:has been|was|is) verified\b|\bnot verified with a title\b|\bno title (?:calls|uses|exercises) (?:it|them)\b", re.I)
PLATFORMS = (("windows", re.compile(r"\bWindows\b")), ("macos", re.compile(r"\bmacOS\b")), ("linux", re.compile(r"\bLinux\b")))


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
    found = subjects(text) + TICKED.findall(text)
    return [name for name in dict.fromkeys(found) if name not in GENERIC and not name.startswith(("lib", "_nid"))]


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
            words.update(HELPER_SUFFIX.sub("", word) for word in WORD.findall(path.read_text(encoding="utf-8", errors="ignore")))
    return words


def exported_names(root):
    names = set()
    for path in (root / "core" / "libs" / "prx").rglob("*"):
        if path.suffix in SOURCES and "tests" not in path.relative_to(root).parts and path.is_file():
            text = path.read_text(encoding="utf-8", errors="ignore")
            names.update(HELPER_SUFFIX.sub("", name) for name in EXPORTED.findall(text))
            names.update(POSTFIXED.findall(text))
    return names


def untested(text, words, exported=frozenset()):
    names = [HELPER_SUFFIX.sub("", name) for name in entry_names(text)]
    found = [name for name in names if not PLAIN.fullmatch(name) or name in exported]
    return None if not found else not any(name in words for name in found)


def stale(text, root):
    missing = [path for path in DEBT_TARGET.findall(text) if not (root / path).exists()]
    return f"missing link {missing[0]}" if missing else None


def labels(entry, root, words, exported=frozenset()):
    explicit = set(debt_labels(entry.text))
    chosen = {"closer": [closer(entry.text)], "test": ["no-test"] if untested(entry.text, words, exported) else [], "platform": platforms(entry.text)}
    for family, vocabulary in DEBT_LABELS.items():
        given = [label for label in vocabulary if label in explicit]
        if given:
            chosen[family] = [label for label in given if label != "tested"]
    found = [label for family in DEBT_LABELS for label in chosen[family]]
    reason = stale(entry.text, root)
    return (found + ["stale?"] if reason else found), reason


def report(root, libraries=None):
    words = test_words(root)
    exported = exported_names(root)
    rows = []
    for entry in entries(root, libraries):
        found, reason = labels(entry, root, words, exported)
        rows.append({"file": entry.file, "line": entry.line, "section": entry.section, "names": entry_names(entry.text), "labels": found, "reason": reason})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description="Label technical debt entries: who can close them (console, firmware, gpu, title or code), no-test when no test names what they name, stale? when they link a file that no longer exists, and the platforms they name. A Labels: trailer on an entry replaces the inferred labels of its family. A report, not a gate: it always exits 0.")
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
