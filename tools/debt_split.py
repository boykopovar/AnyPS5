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
LINK = re.compile(r"\]\((?:<((?:[^<>\n\\]|\\.)*)>|([^)\s<][^)\s]*))\)")


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



def relink_unfenced(text, source, destination):
    def replace(match):
        angled = match.group(1) is not None
        target = match.group(1) if angled else match.group(2)
        if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:|/", target):
            return match.group(0)
        path, _, anchor = target.partition("#")
        resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), path)) if path else source
        moved = posixpath.relpath(resolved, posixpath.dirname(destination)) + ("#" + anchor if anchor else "")
        return "](<" + moved + ">)" if angled else "](" + moved + ")"

    runs = list(re.finditer(r"`+", text))
    result = []
    position = 0
    index = 0

    while index < len(runs):
        opening = runs[index]
        opening_start = opening.start()
        delimiter = opening.group()

        preceding = text[:opening_start]
        backslashes = len(preceding) - len(preceding.rstrip("\\"))
        if backslashes % 2:
            opening_start += 1
            delimiter = delimiter[1:]
            if not delimiter:
                index += 1
                continue

        closing_index = next(
            (i for i in range(index + 1, len(runs))
             if runs[i].group() == delimiter),
            None,
        )

        if closing_index is None:
            index += 1
            continue

        closing = runs[closing_index]
        result.append(LINK.sub(replace, text[position:opening_start]))
        result.append(text[opening_start:closing.end()])
        position = closing.end()
        index = closing_index + 1

    result.append(LINK.sub(replace, text[position:]))
    return "".join(result)

def relink(text, source, destination):
    result = []
    outside = []
    fence_char = None
    fence_length = 0

    def flush_outside():
        if outside:
            result.append(relink_unfenced(
                "".join(outside), source, destination
            ))
            outside.clear()

    for line in text.splitlines(keepends=True):
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})([^\r\n]*)", line)
        if marker and marker.group(1).startswith("`") and "`" in marker.group(2):
            marker = None


        if fence_char is None:
            if marker:
                flush_outside()
                fence_char = marker.group(1)[0]
                fence_length = len(marker.group(1))
                result.append(line)
            else:
                outside.append(line)
        else:
            result.append(line)
            if re.match(
                r"^ {0,3}" + re.escape(fence_char) +
                "{" + str(fence_length) + r",}[ \t]*$",
                line.rstrip("\r\n"),
            ):
                fence_char = None
                fence_length = 0

    flush_outside()
    return "".join(result)


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
    class Entry:
        def __init__(self, text):
            self.text = text

    order = sections_of(index_text) or sections_of(new)
    before = [(kind, section, item, key(kind, item)) for kind, section, item in parse(base) if kind == "entry" and key(kind, item)]
    after = [(kind, section, item, key(kind, item)) for kind, section, item in parse(new) if kind == "entry" and key(kind, item)]
    files = {}

    def load(name):
        if name not in files:
            files[name] = {section: [Entry(text) for text in entries]
                           for section, entries in read_debt(root, name).items()}
        return files[name]

    matcher = difflib.SequenceMatcher(None, [entry[1:3] for entry in before], [entry[1:3] for entry in after], autojunk=False)
    operations = matcher.get_opcodes()
    names = {entry[3] for tag, i1, i2, j1, j2 in operations if tag != "equal"
             for entry in before[i1:i2] + after[j1:j2]}
    available = {}
    for name in names:
        for section, entries in load(name).items():
            for entry in entries:
                available.setdefault((name, section, entry.text), []).append(entry)
    available = {key: iter(entries) for key, entries in available.items()}
    originals = [next(available.get((name, section, relink(item, INDEX, debt_file(name))), iter(())), None)
                 for _, section, item, name in before]
    resolved = [None] * len(after)

    def locate(index):
        _, section, item, name = before[index]
        entries = load(name).get(section, [])
        original = originals[index]
        if original is None or original not in entries:
            raise LookupError(f"{debt_file(name)}: entry not found under '{section}': {item.splitlines()[0][:100]}")
        return entries, entries.index(original)

    def insert(index, anchor):
        _, section, item, name = after[index]
        entries = load(name).setdefault(section, [])
        moved = Entry(relink(item, INDEX, debt_file(name)))
        position = 0 if anchor is None else len(entries)
        if anchor is not None:
            anchored = resolved[anchor]
            if anchored in entries:
                position = entries.index(anchored) + 1
        entries.insert(position, moved)
        resolved[index] = moved

    for tag, i1, i2, j1, j2 in operations:
        if tag == "equal":
            resolved[j1:j2] = originals[i1:i2]
            continue
        removed, added = before[i1:i2], after[j1:j2]
        kept_old, kept_new = set(), set()
        for offset, (old, fresh) in enumerate(zip(removed, added)):
            if old[1] == fresh[1] and old[3] == fresh[3]:
                entries, position = locate(i1 + offset)
                entries[position].text = relink(fresh[2], INDEX, debt_file(fresh[3]))
                resolved[j1 + offset] = entries[position]
                kept_old.add(offset)
                kept_new.add(offset)
        for offset, old in enumerate(removed):
            if offset not in kept_old:
                entries, position = locate(i1 + offset)
                del entries[position]
        for offset, fresh in enumerate(added):
            if offset in kept_new:
                continue
            anchor = next((index for index in range(j1 + offset - 1, -1, -1)
                           if after[index][1] == fresh[1] and after[index][3] == fresh[3]), None)
            insert(j1 + offset, anchor)
    for name, sections in files.items():
        write_debt(root, name, {section: [entry.text for entry in entries]
                                for section, entries in sections.items()}, order)
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
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=f"Split {INDEX} into one file per library or component, and move entries of a change made before the split")
    parser.add_argument("command", choices=("split", "resolve"),
                        help=f"split: move the entries of {INDEX} into {DEBT_DIR}/; resolve: during a rebase stopped on {INDEX}, apply the stopped commit's entry changes to {DEBT_DIR}/ and stage them")
    args = parser.parse_args(argv)
    try:
        root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").strip())
    except subprocess.CalledProcessError as error:
        parser.error(f"not a git repository: {error}")
    command_split(root) if args.command == "split" else command_resolve(root)


if __name__ == "__main__":
    main()
