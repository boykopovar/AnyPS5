import argparse
import json
import os
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_conventions import DEBT, DEBT_DIR, DEBT_LABELS, DEBT_TARGET, debt_entries, debt_labels

ROOT = Path(__file__).resolve().parent.parent
SOURCES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
_TEST_SOURCES = SOURCES | {".py"}
LINK = re.compile(r"\[([^\]]+)\]\((?:\.\./)+core/[^)#\s]+")
TICKED = re.compile(r"`([a-z_][A-Za-z0-9_]*)`")
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SEPARATOR = re.compile(r",\s*|\s+and\s+|\s+or\s+")
HELPER_SUFFIX = re.compile(r"_nid_(?:postfix|no_patch(?:_cut)?)$")
_HELPER_SUFFIXES = ("_nid_postfix", "_nid_no_patch_cut", "_nid_no_patch")
_HELPER_SUFFIX_BYTES = (b"_nid_postfix", b"_nid_no_patch_cut", b"_nid_no_patch")
_WORD_BYTES = frozenset(b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")
_NON_WORD_TRANSLATION = bytes(b if b in _WORD_BYTES else 32 for b in range(256))
PLAIN = re.compile(r"[a-z]+")
EXPORTED = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
_EXPORTED_BYTES = re.compile(rb"APS5_VABI[ \t\n\r\f\v\x1c-\x1f]+(\w+)[ \t\n\r\f\v\x1c-\x1f]*\(")
POSTFIXED = re.compile(r"\b(\w+?)_nid_postfix\b")
GENERIC = {"int", "void", "char", "bool", "float", "double", "size_t", "ssize_t", "off_t", "uintptr_t", "intptr_t",
           "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
           "nullptr", "true", "false", "errno", "main", "this"}
CLOSES = re.compile(r"Closes when:(.*)", re.S)
GPU_NAME = r"(?:an? )?(?:RADV|AMD|NVIDIA|Intel|MoltenVK|lavapipe|Radeon|RTX|GeForce|RDNA ?\d?|GCN)"
CLOSERS = (
    ("gpu", re.compile(rf"\b(?:other (?:GPUs|drivers|devices|vendors)\b|fails on {GPU_NAME}|only (?:measured|tested|verified) on {GPU_NAME}|(?:not|never) (?:measured|tested|verified) on {GPU_NAME})")),
    ("firmware", re.compile(r"\b(?:unknown signatures?\b|signatures?(?: and behaviou?r)? (?:are |is )?(?:unknown|unverified)\b|prototypes?(?: and behaviou?r)? (?:are |is )?(?:unknown|unverified)\b"
                            r"|no credible prototype|parameter lists?\b[^.]*\b(?:unverified|unknown)\b|import[- ]resolution\b|declared without parameters\b|parameterless\b"
                            r"|unidentified export\b|signature\b[^.]*\binferred from\b|meanings? (?:remain |are |is )?(?:unresolved|unknown)\b)", re.I)),
    ("console", re.compile(r"\b(?:(?:measured|measurement)\b[^.]*\b(?:PS5|console)\b|unmeasured\b|not (?:been )?measured\b|unverified on (?:a |the )?(?:PS5|console)\b"
                           r"|(?:not|never) (?:confirmed|verified|checked) on (?:a |the )?(?:PS5|console|hardware)\b|nothing is checked on the console\b|no console\b"
                           r"|console's\b[^.]*\b(?:not known|unknown|unverified)\b|(?:not known|unknown|unresolved|unverified)\b[^.]*\bon (?:a |the )?console\b"
                           r"|measured in an emulator\b|neither documented nor measured\b|unverified console\b|uncited assumption\b)", re.I)),
    ("firmware", re.compile(r"\b(?:needs? (?:the |a )?(?:firmware|module|disassembly)\b|firmware (?:module )?readings?\b|from the firmware module\b|read from the module\b)", re.I)),
)
TITLE_CLOSE = re.compile(r"\b(?:(?:PPSA|CUSA)\d{5}\b|titles?\b)", re.I)
TITLE_GAP = re.compile(r"\b(?:no (?:game )?title (?:has been|was|is) verified\b|not verified with a title\b|no title (?:calls|uses|exercises) (?:it|them)\b)", re.I)
_PLATFORM_WORDS = (("windows", "Windows"), ("macos", "macOS"), ("linux", "Linux"))
PLATFORMS = tuple((label, re.compile(rf"\b{word}\b")) for label, word in _PLATFORM_WORDS)
_PLATFORM_CHECKS = tuple((label, word, pattern) for (label, word), (_, pattern) in zip(_PLATFORM_WORDS, PLATFORMS))


@dataclass
class Entry:
    file: str
    line: int
    section: Optional[str]
    text: str


def _strip_helper(name):
    if "_nid_" in name:
        for suffix in _HELPER_SUFFIXES:
            if name.endswith(suffix):
                return name[:-len(suffix)]
    return name


def _strip_helper_bytes(name):
    if b"_nid_" in name:
        for suffix in _HELPER_SUFFIX_BYTES:
            if name.endswith(suffix):
                return name[:-len(suffix)]
    return name


def debt_files(root, libraries=None):
    files = [root / DEBT] + sorted((root / DEBT_DIR).glob("*.md"))
    return [path for path in files if path.is_file() and (not libraries or path.stem in libraries)]


def entries(root, libraries=None):
    found = []
    for path in debt_files(root, libraries):
        text = path.read_text(encoding="utf-8")
        rel = path.relative_to(root).as_posix()
        headings = [(number, line[4:].strip()) for number, line in enumerate(text.split("\n"), 1) if line.startswith("### ")]
        heading_index = 0
        heading_count = len(headings)
        section = None
        for number, entry in debt_entries(text).items():
            while heading_index < heading_count and headings[heading_index][0] < number:
                section = headings[heading_index][1]
                heading_index += 1
            found.append(Entry(rel, number, section, entry))
    return found


def subjects(text):
    if "](" not in text:
        return []
    found = []
    for label in LINK.findall(text):
        found += [piece for piece in SEPARATOR.split(label.replace("`", "").strip()) if WORD.fullmatch(piece)]
    return list(dict.fromkeys(found))


def entry_names(text):
    found = subjects(text)
    if "`" in text:
        found += TICKED.findall(text)
    return [name for name in dict.fromkeys(found) if name not in GENERIC and not name.startswith(("lib", "_nid"))]


def closer(text):
    index = text.find("Closes when:")
    has_closes = index != -1
    scope = text[index + 12:] if has_closes else text
    for label, pattern in CLOSERS:
        if pattern.search(scope):
            return label
    if has_closes:
        return "title" if TITLE_CLOSE.search(scope) else "code"
    return "title" if (not scope.isascii() or "title" in scope.lower()) and TITLE_GAP.search(scope) else "code"


def platforms(text):
    found = [label for label, word, pattern in _PLATFORM_CHECKS if word in text and pattern.search(text)]
    return found if len(found) < len(_PLATFORM_CHECKS) else []


def test_words(root):
    core = os.path.join(root, "core")
    if not os.path.isdir(core):
        return set()
    raw_words = set()
    str_words = set()
    stack = [(core, False)]
    while stack:
        directory, in_tests = stack.pop()
        with os.scandir(directory) as iterator:
            for entry in iterator:
                if entry.is_dir(follow_symlinks=False):
                    stack.append((entry.path, in_tests or entry.name == "tests"))
                elif in_tests:
                    _, ext = os.path.splitext(entry.name)
                    if ext in _TEST_SOURCES and entry.is_file():
                        with open(entry.path, "rb") as handle:
                            raw = handle.read()
                        if raw.isascii():
                            raw_words.update(raw.translate(_NON_WORD_TRANSLATION).split())
                        else:
                            str_words.update(WORD.findall(raw.decode("utf-8", errors="ignore")))
    words = {_strip_helper(word) if "_nid_" in word else word for word in str_words}
    for token in raw_words:
        stripped = token.lstrip(b"0123456789")
        if stripped:
            if b"_nid_" in stripped:
                stripped = _strip_helper_bytes(stripped)
            words.add(stripped.decode("ascii"))
    return words


def exported_names(root):
    prx = os.path.join(root, "core", "libs", "prx")
    if not os.path.isdir(prx):
        return set()
    raw_names = set()
    str_names = set()
    stack = [prx]
    while stack:
        directory = stack.pop()
        with os.scandir(directory) as iterator:
            for entry in iterator:
                if entry.is_dir(follow_symlinks=False):
                    if entry.name != "tests":
                        stack.append(entry.path)
                else:
                    _, ext = os.path.splitext(entry.name)
                    if ext in SOURCES and entry.is_file():
                        with open(entry.path, "rb") as handle:
                            raw = handle.read()
                        if raw.isascii():
                            if b"APS5_VABI" in raw:
                                for match in _EXPORTED_BYTES.finditer(raw):
                                    start = match.start()
                                    if start == 0 or raw[start - 1] not in _WORD_BYTES:
                                        name = match.group(1)
                                        raw_names.add(_strip_helper_bytes(name) if b"_nid_" in name else name)
                            if b"_nid_postfix" in raw:
                                length = len(raw)
                                pos = raw.find(b"_nid_postfix")
                                while pos != -1:
                                    end = pos + 12
                                    if end == length or raw[end] not in _WORD_BYTES:
                                        start = pos
                                        while start > 0 and raw[start - 1] in _WORD_BYTES:
                                            start -= 1
                                        if start < pos:
                                            raw_names.add(raw[start:pos])
                                    pos = raw.find(b"_nid_postfix", pos + 1)
                        else:
                            text = raw.decode("utf-8", errors="ignore")
                            if "APS5_VABI" in text:
                                for name in EXPORTED.findall(text):
                                    str_names.add(_strip_helper(name))
                            if "_nid_postfix" in text:
                                str_names.update(POSTFIXED.findall(text))
    str_names.update(name.decode("ascii") for name in raw_names)
    return str_names


def _untested_names(names, words, exported=frozenset()):
    found = [name for name in (_strip_helper(name) for name in names) if not PLAIN.fullmatch(name) or name in exported]
    return None if not found else not any(name in words for name in found)


def untested(text, words, exported=frozenset()):
    return _untested_names(entry_names(text), words, exported)


def _stale_cached(text, root, exists_cache):
    if "](" not in text:
        return None
    for path in DEBT_TARGET.findall(text):
        exists = exists_cache.get(path)
        if exists is None:
            exists = os.path.exists(os.path.join(root, path))
            exists_cache[path] = exists
        if not exists:
            return f"missing link {path}"
    return None


def stale(text, root):
    return _stale_cached(text, root, {})


def _entry_labels(text, root, words, exported=frozenset(), names=None, exists_cache=None):
    explicit = set(debt_labels(text)) if "Labels:" in text else set()
    chosen = {}
    for family, vocabulary in DEBT_LABELS.items():
        given = [label for label in vocabulary if label in explicit] if explicit else []
        if given:
            chosen[family] = [label for label in given if label != "tested"]
        elif family == "closer":
            chosen[family] = [closer(text)]
        elif family == "test":
            is_untested = _untested_names(names, words, exported) if names is not None else untested(text, words, exported)
            chosen[family] = ["no-test"] if is_untested else []
        elif family == "platform":
            chosen[family] = platforms(text)
    found = [label for family in DEBT_LABELS for label in chosen[family]]
    reason = _stale_cached(text, root, exists_cache if exists_cache is not None else {})
    return (found + ["stale?"] if reason else found), reason


def labels(entry, root, words, exported=frozenset()):
    return _entry_labels(entry.text, root, words, exported)


def report(root, libraries=None):
    found_entries = entries(root, libraries)
    if not found_entries:
        return []
    words = test_words(root)
    exported = exported_names(root)
    exists_cache = {}
    rows = []
    for entry in found_entries:
        names = entry_names(entry.text)
        found, reason = _entry_labels(entry.text, root, words, exported, names, exists_cache)
        rows.append({"file": entry.file, "line": entry.line, "section": entry.section, "names": names, "labels": found, "reason": reason})
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
