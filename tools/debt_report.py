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
LINK = re.compile(r"\[([^\]]+)\]\((?:\.\./)+core/[^)#\s]+")
TICKED = re.compile(r"`([a-z_][A-Za-z0-9_]*)`")
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SEPARATOR = re.compile(r",\s*|\s+and\s+|\s+or\s+")


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
