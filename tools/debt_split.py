import argparse
import difflib
import posixpath
import re
import subprocess
import sys
from pathlib import Path

from check_conventions import DEBT as INDEX, debt_name

DEBT_DIR = "docs/dev/debt"
HEADING = re.compile(r"^(#{1,6}) (.+?)\s*$")
LINK = re.compile(r"\]\(([^)\s]+)\)")


def parse(text):
    items = []
    section = None
    current = None
    blanks = []
    for line in text.splitlines():
        heading = HEADING.match(line)
        if heading:
            section = heading[2] if len(heading[1]) == 3 else section
            current = None
            items.append(["heading", section, [line]])
        elif line.startswith("- "):
            current = ["entry", section, [line]]
            items.append(current)
            blanks = []
        elif current and (line.startswith(" ") or line.startswith("\t")):
            current[2] += blanks + [line]
            blanks = []
        elif line.strip():
            current = None
            items.append(["text", section, [line]])
        else:
            blanks.append(line)
    return [(kind, section, "\n".join(lines)) for kind, section, lines in items]


def key(kind, text):
    return debt_name(text) if kind == "entry" else None


def relink(text, source, destination):
    def replace(match):
        target = match.group(1)
        if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:|/", target):
            return match.group(0)
        path, _, anchor = target.partition("#")
        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), path)) if path else source
        moved = posixpath.relpath(resolved, posixpath.dirname(destination))
        return "](" + moved + ("#" + anchor if anchor else "") + ")"
    return LINK.sub(replace, text)


def debt_file(name):
    return f"{DEBT_DIR}/{name}.md"


def render(title, sections, order):
    out = [title, ""]
    for section in sorted(sections, key=lambda section: order.index(section) if section in order else len(order)):
        out += [f"### {section}", ""] + sections[section] + [""]
    return "\n".join(out)


def read_debt(root, name):
    path = root / debt_file(name)
    sections = {}
    if path.exists():
        section = None
        for kind, item_section, text in parse(path.read_text(encoding="utf-8-sig")):
            if kind == "heading" and item_section and text.startswith("### "):
                section = item_section
                sections.setdefault(section, [])
            elif kind != "heading" and section:
                sections[section].append(text)
    return sections


def write_lf(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write(text)


def write_debt(root, name, sections, order):
    path = root / debt_file(name)
    sections = {section: entries for section, entries in sections.items() if entries}
    if not sections:
        if path.exists():
            path.unlink()
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    write_lf(path, render(f"# {name} technical debt", sections, order))


def sections_of(text):
    return [section for kind, section, _ in parse(text) if kind == "heading" and section]


def split(text):
    order = sections_of(text)
    index, files = [], {}
    for kind, section, item in parse(text):
        name = key(kind, item)
        if name is None:
            index.append((kind, item))
        else:
            files.setdefault(name, {}).setdefault(section, []).append(relink(item, INDEX, debt_file(name)))
    return index, files, order


def render_index(index):
    out = []
    for kind, item in index:
        if kind in ("heading", "text") and out and out[-1] != "":
            out.append("")
        out.append(item)
        if kind == "heading" or kind == "text":
            out.append("")
    return "\n".join(out).rstrip("\n") + "\n"


def index_note():
    return ("text", f"Entries about one library or component are in [debt](debt/), one file per library "
                    "or component, under the same sections as this page. Entries that belong to none stay here.")


def command_split(root):
    text = (root / INDEX).read_text(encoding="utf-8-sig")
    index, files, order = split(text)
    if not files:
        sys.exit(f"{INDEX} has no entries to move")
    for name, sections in files.items():
        existing = read_debt(root, name)
        for section, entries in sections.items():
            existing.setdefault(section, []).extend(entries)
        write_debt(root, name, existing, order)
    if index_note() not in index:
        index.insert(1, index_note())
    write_lf(root / INDEX, render_index(index))
    print("\n".join(sorted(debt_file(name) for name in files)))


def git(root, *args, check=True):
    return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace", check=check).stdout


def apply_change(root, base, new, index_text):
    order = sections_of(index_text) or sections_of(new)
    before = [(kind, section, item, key(kind, item)) for kind, section, item in parse(base) if kind == "entry" and key(kind, item)]
    after = [(kind, section, item, key(kind, item)) for kind, section, item in parse(new) if kind == "entry" and key(kind, item)]
    files = {}

    def load(name):
        if name not in files:
            files[name] = read_debt(root, name)
        return files[name]

    def locate(entry):
        _, section, item, name = entry
        entries = load(name).get(section, [])
        moved = relink(item, INDEX, debt_file(name))
        if moved not in entries:
            raise LookupError(f"{debt_file(name)}: entry not found under '{section}': {item.splitlines()[0][:100]}")
        return entries, entries.index(moved)

    def insert(entry, anchor):
        _, section, item, name = entry
        entries = load(name).setdefault(section, [])
        moved = relink(item, INDEX, debt_file(name))
        position = 0 if anchor is None else len(entries)
        if anchor is not None and anchor[1] == section and anchor[3] == name:
            anchored = relink(anchor[2], INDEX, debt_file(name))
            if anchored in entries:
                position = entries.index(anchored) + 1
        entries.insert(position, moved)

    matcher = difflib.SequenceMatcher(None, [entry[1:3] for entry in before], [entry[1:3] for entry in after], autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        removed, added = before[i1:i2], after[j1:j2]
        kept_old, kept_new = set(), set()
        for offset, (old, fresh) in enumerate(zip(removed, added)):
            if old[1] == fresh[1] and old[3] == fresh[3]:
                entries, position = locate(old)
                entries[position] = relink(fresh[2], INDEX, debt_file(fresh[3]))
                kept_old.add(offset)
                kept_new.add(offset)
        for offset, old in enumerate(removed):
            if offset not in kept_old:
                entries, position = locate(old)
                del entries[position]
        for offset, fresh in enumerate(added):
            if offset in kept_new:
                continue
            anchor = next((entry for entry in reversed(after[:j1 + offset]) if entry[1] == fresh[1] and entry[3] == fresh[3]), None)
            insert(fresh, anchor)
    for name, sections in files.items():
        write_debt(root, name, sections, order)
    return sorted(debt_file(name) for name in files)


def unkeyed(text):
    return "\n".join(item for kind, _, item in parse(text) if key(kind, item) is None)


def command_resolve(root):
    stages = {}
    for line in git(root, "ls-files", "-u", "--", INDEX).splitlines():
        meta, _ = line.split("\t", 1)
        _, sha, stage = meta.split()
        stages[stage] = sha
    if not {"1", "2", "3"} <= set(stages):
        sys.exit(f"{INDEX} is not in a three-way conflict")
    base, ours, theirs = (git(root, "cat-file", "-p", stages[stage]) for stage in "123")
    if unkeyed(base) != unkeyed(theirs):
        sys.exit(f"{INDEX}: the change also edits lines that stay in {INDEX}; resolve those by hand")
    try:
        touched = apply_change(root, base, theirs, ours)
    except LookupError as error:
        sys.exit(str(error))
    write_lf(root / INDEX, ours)
    git(root, "add", "--", INDEX, *touched)
    print("\n".join(touched))


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=f"Split {INDEX} into one file per library or component, and move entries of a change made before the split")
    parser.add_argument("command", choices=("split", "resolve"),
                        help=f"split: move the entries of {INDEX} into {DEBT_DIR}/; resolve: during a rebase stopped on {INDEX}, apply the stopped commit's entry changes to {DEBT_DIR}/ and stage them")
    args = parser.parse_args(argv)
    root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").strip())
    command_split(root) if args.command == "split" else command_resolve(root)


if __name__ == "__main__":
    main()
