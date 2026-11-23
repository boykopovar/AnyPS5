import argparse
import bisect
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_conventions import DEBT, DEBT_CHANGES, DEBT_DIR, DEBT_TARGET, debt_entries

ROOT = Path(__file__).resolve().parent.parent
OPERATION = re.compile(r"## (add|change|remove) (.+)")
ROW = re.compile(r"\| (?!-)(?!Name \|).*\|")
LIBRARY = re.compile(r"[A-Za-z0-9._-]+")


class FoldError(Exception):
    pass


def debt_file(library):
    if not isinstance(library, str) or not LIBRARY.fullmatch(library) or library.startswith(".") or library.endswith(".") or ".." in library:
        raise FoldError(f"invalid library name: {library!r}")
    return DEBT if library == Path(DEBT).stem else f"{DEBT_DIR}{library}.md"


@dataclass
class Operation:
    fragment: str
    line: int
    library: str
    kind: str
    target: str
    body: str


def _parse_file(path, name, root, existing_files=None):
    operations, library, current, body_lines = [], None, None, None
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").split("\n"), 1):
        if line.startswith("# "):
            library, current, body_lines = line[2:].strip(), None, None
            try:
                target_file = debt_file(library)
            except FoldError as error:
                raise FoldError(f"{name}:{number}: {error}") from error
            if existing_files is not None:
                exists = existing_files.get(target_file)
                if exists is None:
                    exists = os.path.isfile(os.path.join(root, target_file))
                    existing_files[target_file] = exists
            else:
                exists = (root / target_file).is_file()
            if not exists:
                raise FoldError(f"{name}:{number}: no debt file for {library}")
        elif match := OPERATION.fullmatch(line.strip()):
            if library is None:
                raise FoldError(f"{name}:{number}: operation before a '# <library>' line")
            body_lines = []
            current = Operation(name, number, library, match[1], match[2].strip(), body_lines)
            operations.append(current)
        elif line.startswith("#"):
            raise FoldError(f"{name}:{number}: expected '# <library>' or '## add|change|remove ...'")
        elif current is not None:
            body_lines.append(line)
        elif line.strip():
            raise FoldError(f"{name}:{number}: text outside an operation")
    for operation in operations:
        raw_lines = operation.body
        body = "\n".join(raw_lines).strip("\n")
        operation.body = body
        count = len(_spans_from_lines(body.split("\n"))) if body else 0
        expected = {"add": count >= 1, "change": count == 1, "remove": count == 0}[operation.kind]
        if not expected or (count and not body.startswith(("- ", "| "))):
            raise FoldError(f"{name}:{operation.line}: '{operation.kind}' takes {'one or more entries' if operation.kind == 'add' else 'one entry' if operation.kind == 'change' else 'no entry'}")
    return operations


def parse(path, root):
    return _parse_file(path, path.relative_to(root).as_posix(), root)


def _spans_from_lines(lines):
    found = []
    start = None
    entry_lines = None
    blanks = []
    for index, line in enumerate(lines):
        if line.startswith("- "):
            if start is not None:
                found.append((start, start + len(entry_lines), "\n".join(entry_lines)))
            start = index
            entry_lines = [line]
            blanks = []
        elif start is not None and line.startswith((" ", "\t")):
            entry_lines += blanks + [line]
            blanks = []
        elif line.strip():
            if start is not None:
                found.append((start, start + len(entry_lines), "\n".join(entry_lines)))
                start = None
                entry_lines = None
            if line.startswith("| ") and ROW.fullmatch(line):
                found.append((index, index + 1, line))
        else:
            blanks.append(line)
    if start is not None:
        found.append((start, start + len(entry_lines), "\n".join(entry_lines)))
    return found


def units(text):
    return {begin + 1: entry for begin, _, entry in _spans_from_lines(text.split("\n"))}


def spans(text):
    lines = text.split("\n")
    return _spans_from_lines(lines), lines


def _find_index(found, operation):
    needle = operation.target
    match_index = -1
    count = 0
    for index, (_, _, entry) in enumerate(found):
        if needle in entry:
            match_index = index
            count += 1
    if count != 1:
        raise FoldError(f"{operation.fragment}:{operation.line}: '{operation.target}' matches {count} entries of {operation.library}, not one")
    return match_index


def find(found, operation):
    begin, end, _ = found[_find_index(found, operation)]
    return begin, end


def target(entry):
    first = entry.partition("\n")[0]
    if "](" not in first:
        return None
    match = DEBT_TARGET.search(first)
    return match[1] if match else None


def spaced(found):
    prev_end = None
    for begin, end, entry in found:
        if entry.startswith("- "):
            if prev_end is not None and begin == prev_end:
                return False
            prev_end = end
    return True


def space(lines):
    result = []
    for line in lines:
        if line.startswith("- ") and result and result[-1].strip():
            result.append("")
        result.append(line)
    return result


def _has_following_continuation(lines, index):
    while index < len(lines):
        line = lines[index]
        if line.startswith((" ", "\t")):
            return True
        if line.strip():
            return False
        index += 1
    return False


def _apply_state(lines, found, operation):
    if operation.kind == "add":
        row = operation.body.startswith("| ")
        gap = not row and spaced(found)
        heading = f"### {operation.target}"
        try:
            start = lines.index(heading)
        except ValueError:
            if lines and lines[-1].strip():
                lines.append("")
            start = len(lines)
            lines.append(heading)
            lines.append("")
        stop = next((index for index in range(start + 1, len(lines)) if lines[index].startswith("#")), len(lines))
        inside = [span for span in found if start < span[0] < stop and span[2].startswith("| ") == row]
        op_target = target(operation.body) if inside else None
        same = [span for span in inside if op_target in span[2] and target(span[2]) == op_target] if op_target else []
        at = (same or inside)[-1][1] if inside else start + 1
        body_lines = operation.body.split("\n")
        new = space(body_lines) if gap else body_lines
        if not inside or gap:
            new = [""] + new
        lines[at:at] = new
        delta = len(new)
        if _has_following_continuation(lines, at + delta):
            return lines, _spans_from_lines(lines)
        inserted = [(at + begin, at + end, entry) for begin, end, entry in _spans_from_lines(new)]
        insert_pos = bisect.bisect_left(found, (at, -1, ""))
        if insert_pos < len(found):
            for index in range(insert_pos, len(found)):
                begin, end, entry = found[index]
                found[index] = (begin + delta, end + delta, entry)
        found[insert_pos:insert_pos] = inserted
        return lines, found
    index = _find_index(found, operation)
    begin, end, _ = found[index]
    if operation.kind == "change":
        replacement = operation.body.split("\n")
        lines[begin:end] = replacement
        new_end = begin + len(replacement)
        delta = new_end - end
        if _has_following_continuation(lines, new_end):
            return lines, _spans_from_lines(lines)
        updated = [(begin + b, begin + e, entry) for b, e, entry in _spans_from_lines(replacement)]
        if delta:
            for pos in range(index + 1, len(found)):
                b, e, entry = found[pos]
                found[pos] = (b + delta, e + delta, entry)
        found[index:index + 1] = updated
        return lines, found
    if end < len(lines) and not lines[end].strip() and (begin == 0 or not lines[begin - 1].strip()):
        end += 1
    del lines[begin:end]
    delta = begin - end
    if _has_following_continuation(lines, begin):
        return lines, _spans_from_lines(lines)
    del found[index]
    for pos in range(index, len(found)):
        b, e, entry = found[pos]
        found[pos] = (b + delta, e + delta, entry)
    return lines, found


def apply(text, operation):
    found, lines = spans(text)
    lines, _ = _apply_state(lines, found, operation)
    return "\n".join(lines)


def fold(root, write):
    changes_dir = root / DEBT_CHANGES
    fragments = sorted(changes_dir.glob("*.md"))
    existing_files = {}
    operations = [operation for path in fragments for operation in _parse_file(path, f"{DEBT_CHANGES}{path.name}", root, existing_files)]
    states, claimed, problems = {}, {}, []
    for operation in operations:
        if operation.library not in states:
            text = (root / debt_file(operation.library)).read_text(encoding="utf-8")
            found, lines = spans(text)
            states[operation.library] = [lines, found]
        if operation.kind != "add":
            try:
                key = (operation.library, find(states[operation.library][1], operation))
            except FoldError as error:
                problems.append(str(error))
                continue
            previous = claimed.get(key)
            if previous is not None and previous.fragment == operation.fragment:
                problems.append(f"{previous.fragment}:{previous.line} and {operation.fragment}:{operation.line} both target the same {operation.library} entry; name each entry once per fragment")
            elif previous is not None:
                problems.append(f"{operation.fragment}:{operation.line} and {previous.fragment}:{previous.line} both target the same {operation.library} entry; merge them into one fragment")
            claimed[key] = operation
    if problems:
        raise FoldError("\n".join(problems))
    for operation in operations:
        lines, found = states[operation.library]
        states[operation.library] = list(_apply_state(lines, found, operation))
    texts = {library: "\n".join(lines) for library, (lines, _) in states.items()}
    if write:
        for library, text in texts.items():
            with (root / debt_file(library)).open("w", encoding="utf-8", newline="\n") as output:
                output.write(text)
        for path in fragments:
            path.unlink()
    return texts, fragments


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="apply the debt fragments in docs/dev/debt/changes/ to the debt files")
    parser.add_argument("--check", action="store_true", help="check that every fragment applies, change nothing")
    parser.add_argument("--show", metavar="LIBRARY", help="print LIBRARY's debt file with the pending fragments applied")
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    try:
        target = debt_file(args.show) if args.show is not None else None
        texts, fragments = fold(args.root, write=not (args.check or args.show is not None))
    except FoldError as error:
        print(error, file=sys.stderr)
        return 1
    if target is not None:
        path = args.root / target
        if not path.is_file():
            print(f"no debt file for {args.show}", file=sys.stderr)
            return 1
        sys.stdout.write(texts.get(args.show, path.read_text(encoding="utf-8")))
    elif not args.check:
        print(f"applied {len(fragments)} fragments to {len(texts)} debt files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
