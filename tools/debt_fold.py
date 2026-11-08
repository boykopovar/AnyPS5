import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_conventions import DEBT_DIR, debt_entries

ROOT = Path(__file__).resolve().parent.parent
CHANGES = DEBT_DIR + "changes/"
OPERATION = re.compile(r"## (add|change|remove) (.+)")
ROW = re.compile(r"\| (?!-)(?!Name \|).*\|")


class FoldError(Exception):
    pass


@dataclass
class Operation:
    fragment: str
    line: int
    library: str
    kind: str
    target: str
    body: str


def parse(path, root):
    name = path.relative_to(root).as_posix()
    operations, library, current = [], None, None
    for number, line in enumerate(path.read_text(encoding="utf-8").split("\n"), 1):
        if line.startswith("# "):
            library, current = line[2:].strip(), None
            if not (root / DEBT_DIR / f"{library}.md").is_file():
                raise FoldError(f"{name}:{number}: no debt file for {library}")
        elif match := OPERATION.fullmatch(line.strip()):
            if library is None:
                raise FoldError(f"{name}:{number}: operation before a '# <library>' line")
            current = Operation(name, number, library, match[1], match[2].strip(), "")
            operations.append(current)
        elif line.startswith("#"):
            raise FoldError(f"{name}:{number}: expected '# <library>' or '## add|change|remove ...'")
        elif current is not None:
            current.body += line + "\n"
        elif line.strip():
            raise FoldError(f"{name}:{number}: text outside an operation")
    for operation in operations:
        operation.body = operation.body.strip("\n")
        count = len(units(operation.body))
        expected = {"add": count >= 1, "change": count == 1, "remove": count == 0}[operation.kind]
        if not expected or (count and not operation.body.startswith(("- ", "| "))):
            raise FoldError(f"{name}:{operation.line}: '{operation.kind}' takes {'one or more entries' if operation.kind == 'add' else 'one entry' if operation.kind == 'change' else 'no entry'}")
    return operations


def units(text):
    found = dict(debt_entries(text))
    found.update({number: line for number, line in enumerate(text.split("\n"), 1) if ROW.fullmatch(line)})
    return dict(sorted(found.items()))


def spans(text):
    lines = text.split("\n")
    return [(start - 1, start - 1 + len(entry.split("\n")), entry) for start, entry in units(text).items()], lines


def find(text, operation):
    found, _ = spans(text)
    matches = [(begin, end) for begin, end, entry in found if operation.target in entry]
    if len(matches) != 1:
        raise FoldError(f"{operation.fragment}:{operation.line}: '{operation.target}' matches {len(matches)} entries of {operation.library}, not one")
    return matches[0]


def apply(text, operation):
    found, lines = spans(text)
    if operation.kind == "add":
        heading = f"### {operation.target}"
        if heading not in lines:
            lines = lines + ([""] if lines and lines[-1].strip() else []) + [heading, ""]
            found, _ = spans("\n".join(lines))
        start = lines.index(heading)
        stop = next((index for index in range(start + 1, len(lines)) if lines[index].startswith("#")), len(lines))
        row = operation.body.startswith("| ")
        inside = [(begin, end) for begin, end, entry in found if start < begin < stop and entry.startswith("| ") == row]
        spaced = len(inside) > 1 and inside[-1][0] > inside[-2][1]
        at = inside[-1][1] if inside else start + 1
        new = operation.body.split("\n")
        if inside and spaced:
            new = [""] + new
        elif not inside:
            new = [""] + new
        return "\n".join(lines[:at] + new + lines[at:])
    begin, end = find(text, operation)
    if operation.kind == "change":
        return "\n".join(lines[:begin] + operation.body.split("\n") + lines[end:])
    if end < len(lines) and not lines[end].strip() and (begin == 0 or not lines[begin - 1].strip()):
        end += 1
    return "\n".join(lines[:begin] + lines[end:])


def fold(root, write):
    fragments = sorted((root / CHANGES).glob("*.md"))
    operations = [operation for path in fragments for operation in parse(path, root)]
    texts, claimed, problems = {}, {}, []
    for operation in operations:
        path = root / DEBT_DIR / f"{operation.library}.md"
        original = path.read_text(encoding="utf-8")
        texts.setdefault(operation.library, original)
        if operation.kind != "add":
            try:
                key = (operation.library, find(original, operation))
            except FoldError as error:
                problems.append(str(error))
                continue
            if key in claimed and claimed[key].fragment != operation.fragment:
                problems.append(f"{operation.fragment}:{operation.line} and {claimed[key].fragment}:{claimed[key].line} both {operation.kind} the same {operation.library} entry; merge them into one fragment")
            claimed[key] = operation
    if problems:
        raise FoldError("\n".join(problems))
    for operation in operations:
        texts[operation.library] = apply(texts[operation.library], operation)
    if write:
        for library, text in texts.items():
            (root / DEBT_DIR / f"{library}.md").write_text(text, encoding="utf-8", newline="\n")
        for path in fragments:
            path.unlink()
    return texts, fragments


def main(argv=None):
    parser = argparse.ArgumentParser(description="apply the debt fragments in docs/dev/debt/changes/ to the debt files")
    parser.add_argument("--check", action="store_true", help="check that every fragment applies, change nothing")
    parser.add_argument("--show", metavar="LIBRARY", help="print LIBRARY's debt file with the pending fragments applied")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    try:
        texts, fragments = fold(args.root, write=not (args.check or args.show))
    except FoldError as error:
        print(error, file=sys.stderr)
        return 1
    if args.show:
        path = args.root / DEBT_DIR / f"{args.show}.md"
        if not path.is_file():
            print(f"no debt file for {args.show}", file=sys.stderr)
            return 1
        sys.stdout.write(texts.get(args.show, path.read_text(encoding="utf-8")))
    elif not args.check:
        print(f"applied {len(fragments)} fragments to {len(texts)} debt files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
