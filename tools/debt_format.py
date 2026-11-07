import argparse
import difflib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from check_conventions import DEBT, DEBT_DIR, is_debt, space_debt

NUMBER = re.compile(r"\d+")
WORD = re.compile(r"\S+|\s+")


def git(root, *args, check=True):
    return subprocess.run(["git", *args], cwd=root, capture_output=True, check=check).stdout


def debt_paths(root):
    listed = git(root, "ls-files", "-z", "--", DEBT, DEBT_DIR).decode("utf-8").split("\0")
    return sorted({path for path in listed if path and is_debt(path)})


def spaced(data):
    return space_debt(data.decode("utf-8")).encode("utf-8")


def format_files(root, paths, write):
    changed = []
    for path in paths:
        file = root / path
        if not file.is_file():
            continue
        data = file.read_bytes()
        result = spaced(data)
        if result != data:
            changed.append(path)
            if write:
                file.write_bytes(result)
    return changed


def command_format(root, check):
    changed = format_files(root, debt_paths(root), not check)
    if changed:
        print("\n".join(changed))
    return 1 if check and changed else 0


def merge_file(ours, base, theirs):
    with tempfile.TemporaryDirectory() as directory:
        files = []
        for name, data in (("ours", ours), ("base", base), ("theirs", theirs)):
            file = Path(directory) / name
            file.write_bytes(data)
            files.append(str(file))
        result = subprocess.run(["git", "merge-file", "-p", "--diff3", "-L", "ours", "-L", "base", "-L", "theirs", *files], capture_output=True)
    if result.returncode < 0 or result.returncode > 127:
        sys.exit(f"git merge-file failed: {result.stderr.decode('utf-8', 'replace').strip()}")
    return result.stdout.decode("utf-8").split("\n")


def hunks(lines):
    index = 0
    while index < len(lines):
        if lines[index] != "<<<<<<< ours":
            yield lines[index], None
            index += 1
            continue
        sides = {"ours": [], "base": [], "theirs": []}
        side = "ours"
        index += 1
        while lines[index] != ">>>>>>> theirs":
            if lines[index] == "||||||| base":
                side = "base"
            elif lines[index] == "=======" and side == "base":
                side = "theirs"
            else:
                sides[side].append(lines[index])
            index += 1
        index += 1
        yield None, sides


def renumbered(side, base, other):
    side, base = [line for line in side if line.strip()], [line for line in base if line.strip()]
    if side == base or [NUMBER.sub("0", line) for line in side] != [NUMBER.sub("0", line) for line in base]:
        return False
    numbers, kept = NUMBER.findall("\n".join(base)), NUMBER.findall("\n".join(other))
    changed = {old for old, new in zip(numbers, NUMBER.findall("\n".join(side))) if old != new}
    return len(kept) < len(numbers) and not changed & set(kept)


def settle(ours, base, theirs):
    if renumbered(theirs, base, ours):
        return ours
    if renumbered(ours, base, theirs):
        return theirs
    return None


def rewrites_numbers(side, base):
    matcher = difflib.SequenceMatcher(None, base, side, autojunk=False)
    return all(any(NUMBER.search(word) for word in base[start:end])
               for tag, start, end, _, _ in matcher.get_opcodes() if tag != "equal")


def merge_words(ours, base, theirs):
    words = [WORD.findall("\n".join(lines)) for lines in (ours, base, theirs)]
    if not rewrites_numbers(words[0], words[1]) and not rewrites_numbers(words[2], words[1]):
        return None
    merged = []
    for word, sides in hunks(merge_file(*("".join(json.dumps(item) + "\n" for item in side).encode("utf-8") for side in words))[:-1]):
        if sides is None:
            merged.append(json.loads(word))
            continue
        ours_text, base_text, theirs_text = ("".join(json.loads(item) for item in sides[name]) for name in ("ours", "base", "theirs"))
        settled = settle([ours_text], [base_text], [theirs_text])
        if settled is None:
            return None
        merged += settled
    return "".join(merged).split("\n")


def merge(ours, base, theirs, show_base=False):
    result = []
    conflicts = 0
    for line, sides in hunks(merge_file(spaced(ours), spaced(base), spaced(theirs))):
        if sides is None:
            result.append(line)
            continue
        settled = settle(sides["ours"], sides["base"], sides["theirs"])
        if settled is None:
            settled = merge_words(sides["ours"], sides["base"], sides["theirs"])
        if settled is None:
            conflicts += 1
            settled = ["<<<<<<< ours", *sides["ours"]] + (["||||||| base", *sides["base"]] if show_base else []) + \
                      ["=======", *sides["theirs"], ">>>>>>> theirs"]
        result += settled
    return "\n".join(result).encode("utf-8"), conflicts


def command_resolve(root):
    stages = {}
    for record in git(root, "ls-files", "-u", "-z").decode("utf-8").split("\0"):
        if not record:
            continue
        meta, path = record.split("\t", 1)
        _, sha, stage = meta.split()
        if is_debt(path):
            stages.setdefault(path, {})[stage] = sha
    if not stages:
        sys.exit("No technical debt file is in a conflict")
    show_base = git(root, "config", "merge.conflictStyle", check=False).decode("utf-8").strip() in ("diff3", "zdiff3")
    left = []
    for path, shas in sorted(stages.items()):
        if "2" not in shas or "3" not in shas:
            left.append(f"{path}: deleted on one side")
            continue
        base, ours, theirs = (git(root, "cat-file", "blob", shas[stage]) if stage in shas else b"" for stage in "123")
        data, conflicts = merge(ours, base, theirs, show_base)
        (root / path).write_bytes(data)
        if conflicts:
            left.append(f"{path}: {conflicts} conflict(s)")
        else:
            git(root, "add", "--", path)
            print(path)
    unmerged = set(stages)
    for path in format_files(root, [path for path in debt_paths(root) if path not in unmerged], True):
        git(root, "add", "--", path)
        print(path)
    if left:
        print("Resolve these by hand, then add them:\n" + "\n".join(left), file=sys.stderr)
        return 1
    return 0


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=f"Separate the entries of {DEBT} and {DEBT_DIR} with blank lines, and redo a conflicted merge of those files with both sides separated")
    parser.add_argument("command", nargs="?", choices=("format", "resolve"), default="format",
                        help="format: add the missing blank lines and list the files changed; resolve: during a stopped rebase or merge, separate the base, ours and theirs versions of each conflicted debt file, merge them again, stage the files that merge cleanly and format the other debt files")
    parser.add_argument("--check", action="store_true", help="with format: change nothing, list the files that need it and exit with 1")
    args = parser.parse_args(argv)
    if args.check and args.command != "format":
        parser.error("--check works with format only")
    root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").decode("utf-8").strip())
    return command_format(root, args.check) if args.command == "format" else command_resolve(root)


if __name__ == "__main__":
    raise SystemExit(main())
