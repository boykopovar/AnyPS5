import argparse
import collections
import subprocess
import sys
from pathlib import Path

CANDIDATES = ("upstream/main", "origin/main", "main")


def git(root, *arguments):
    result = subprocess.run(["git", "-C", str(root), "-c", "core.quotePath=false", *arguments],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"git {' '.join(arguments[:2])} failed: {result.stderr.strip()}")
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
    lines = git(root, "log", "--format=%an <%ae>%x1f%s", f"{base}..{tip}").splitlines()
    return collections.Counter(line for line in lines if line)


def commits(root, base, tip):
    return len(git(root, "rev-list", f"{base}..{tip}").split())


def added(root, base, tip):
    found = collections.defaultdict(collections.Counter)
    path, header = None, False
    for line in git(root, "diff", "-U0", "--no-renames", "--no-color", base, tip).splitlines():
        if line.startswith("diff --git "):
            path, header = None, True
        elif header and line.startswith("+++ "):
            path = line[6:] if line.startswith("+++ b/") else None
            header = False
        elif line.startswith("@@"):
            header = False
        elif line.startswith("+") and path is not None:
            found[path][line[1:]] += 1
    return found


def differences(old, new):
    problems = []
    for path in sorted(set(old) | set(new)):
        before, after = old.get(path, collections.Counter()), new.get(path, collections.Counter())
        for text, count in sorted((before - after).items()):
            problems.append(f"{path}: an added line is missing ({count}x): {text[:100]}")
        for text, count in sorted((after - before).items()):
            problems.append(f"{path}: a line is added that the branch did not add ({count}x): {text[:100]}")
    return problems


def verify(root, old, new, onto):
    old_base = git(root, "merge-base", old, onto).strip()
    problems = []
    if people(root, old_base, old) != people(root, onto, new):
        problems.append("authors or subjects differ between the old and the new commits")
    count_old, count_new = commits(root, old_base, old), commits(root, onto, new)
    if count_old != count_new:
        problems.append(f"the branch has {count_new} commits on {onto}, and had {count_old} on its old base")
    problems += differences(added(root, old_base, old), added(root, onto, new))
    return old_base, count_old, count_new, problems


def main(argv=None):
    parser = argparse.ArgumentParser(description="check that a rebased branch kept every added line, its authors and its commits")
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
    print("all added lines, authors and commits are kept")
    return 0


if __name__ == "__main__":
    sys.exit(main())
