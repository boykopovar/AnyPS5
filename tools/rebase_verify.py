import argparse
import collections
import subprocess
import sys
from pathlib import Path

CANDIDATES = ("upstream/main", "origin/main", "main")


def git(root, *arguments, raw=False):
    result = subprocess.run(["git", "-C", str(root), "-c", "core.quotePath=false", *arguments],
                            capture_output=True, encoding=None if raw else "utf-8", errors=None if raw else "replace")
    if result.returncode:
        stderr = result.stderr.decode("utf-8", "replace") if raw else result.stderr
        raise RuntimeError(f"git {' '.join(arguments[:2])} failed: {stderr.strip()}")
    return result.stdout


def default_onto(root, name=None):
    if name:
        return name
    for candidate in CANDIDATES:
        if subprocess.run(["git", "-C", str(root), "rev-parse", "--verify", "--quiet", f"{candidate}^{{commit}}"],
                          capture_output=True).returncode == 0:
            return candidate
    raise ValueError(f"no base branch found ({', '.join(CANDIDATES)}); fetch the upstream remote or pass --onto")


def people(root, base, tip):
    lines = git(root, "log", "--no-merges", "--format=%an <%ae>%x1f%s", f"{base}..{tip}").splitlines()
    return collections.Counter(line for line in lines if line)


def commits(root, base, tip):
    return len(git(root, "rev-list", "--no-merges", f"{base}..{tip}").split())


def changed_lines(root, base, tip):
    added = collections.defaultdict(collections.Counter)
    deleted = collections.defaultdict(collections.Counter)
    old_path, new_path, in_hunk = None, None, False
    for line in git(root, "diff", "-U0", "--no-renames", "--no-color", "--no-ext-diff", "--no-textconv",
                    "--src-prefix=a/", "--dst-prefix=b/", base, tip).split("\n"):
        if line.startswith("diff --git "):
            old_path, new_path, in_hunk = None, None, False
        elif not in_hunk and line.startswith("--- "):
            old_path = line[6:] if line.startswith("--- a/") else None
        elif not in_hunk and line.startswith("+++ "):
            new_path = line[6:] if line.startswith("+++ b/") else None
        elif line.startswith("@@"):
            in_hunk = True
        elif in_hunk and line.startswith("+") and new_path is not None:
            added[new_path][line[1:]] += 1
        elif in_hunk and line.startswith("-") and old_path is not None:
            deleted[old_path][line[1:]] += 1
    return added, deleted


def differences(old, new, deleted=False):
    problems = []
    for path in sorted(set(old) | set(new)):
        before, after = old.get(path, collections.Counter()), new.get(path, collections.Counter())
        for text, count in sorted((before - after).items()):
            description = "a deleted line is missing" if deleted else "an added line is missing"
            problems.append(f"{path}: {description} ({count}x): {text[:100]}")
        for text, count in sorted((after - before).items()):
            description = "a line is deleted that the branch did not delete" if deleted else "a line is added that the branch did not add"
            problems.append(f"{path}: {description} ({count}x): {text[:100]}")
    return problems


def changed_modes(root, base, tip):
    fields = git(root, "diff", "--raw", "-z", "--no-renames", "--no-relative", "--no-ext-diff",
                 "--ignore-submodules=none", base, tip, "--", raw=True).split(b"\0")
    modes = {}
    for header, path in zip(fields[::2], fields[1::2]):
        before, after = header[1:].split(maxsplit=2)[:2]
        if before != after:
            modes[path] = before.decode("ascii"), after.decode("ascii")
    return modes


def mode_differences(root, old_base, old, new_base, new):
    old_modes = changed_modes(root, old_base, old)
    new_modes = changed_modes(root, new_base, new)
    upstream_modes = changed_modes(root, old_base, new_base) if old_modes and old_base != new_base else {}
    problems = []
    for path in sorted(old_modes.keys() | new_modes.keys()):
        if path in old_modes:
            before, expected = old_modes[path]
            base_mode = upstream_modes.get(path, (before, before))[1]
            actual = new_modes.get(path, (base_mode, base_mode))[1]
        else:
            expected, actual = new_modes[path]
        if expected != actual:
            name = path.decode("utf-8", "backslashreplace")
            problems.append(f"{name}: file mode {actual}, expected {expected}")
    return problems


def verify(root, old, new, onto):
    old_base = git(root, "merge-base", old, onto).strip()
    new_base = git(root, "merge-base", new, onto).strip()
    problems = []
    if people(root, old_base, old) != people(root, new_base, new):
        problems.append("authors or subjects differ between the old and the new commits")
    count_old, count_new = commits(root, old_base, old), commits(root, new_base, new)
    if count_old != count_new:
        problems.append(f"the branch has {count_new} commits on {onto}, and had {count_old} on its old base")
    old_added, old_deleted = changed_lines(root, old_base, old)
    new_added, new_deleted = changed_lines(root, new_base, new)
    problems += differences(old_added, new_added)
    upstream_added, upstream_deleted = changed_lines(root, old_base, new_base) if old_deleted else ({}, {})
    expected_deleted = {
        path: lines - (upstream_deleted.get(path, collections.Counter()) - upstream_added.get(path, collections.Counter()))
        for path, lines in old_deleted.items()
    }
    problems += differences(expected_deleted, new_deleted, deleted=True)
    problems += mode_differences(root, old_base, old, new_base, new)
    return old_base, count_old, count_new, problems


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="check that a rebased branch kept its added and deleted lines, file modes, authors and commits")
    parser.add_argument("old", help="the branch or commit before the rebase")
    parser.add_argument("new", help="the branch or commit after the rebase")
    parser.add_argument("--onto", help="the branch the new commits sit on (default: upstream/main, then origin/main, then main)")
    parser.add_argument("--repo", type=Path, default=Path("."), help="the repository to check (default: the current directory)")
    args = parser.parse_args(argv)
    try:
        onto = default_onto(args.repo, args.onto)
        old_base, count_old, count_new, problems = verify(args.repo, args.old, args.new, onto)
    except (RuntimeError, ValueError) as error:
        parser.error(str(error))
    print(f"{args.new} rebases {args.old} onto {onto} (old base {old_base[:10]}): {count_old} commits before, {count_new} after")
    for problem in problems:
        print(f"error: {problem}")
    if problems:
        print(f"{len(problems)} difference(s): the rebase changed something other than conflict resolution")
        return 1
    print("all added and deleted lines, file modes, authors and commits are kept")
    return 0


if __name__ == "__main__":
    sys.exit(main())
