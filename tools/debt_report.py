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
