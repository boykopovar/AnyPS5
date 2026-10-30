import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

PULL_REQUEST = re.compile(r"Merge pull request #(\d+)|\(#(\d+)\)$")
BASES = ("upstream/main", "origin/main", "main")


def git(repo, *args, check=True):
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and result.returncode:
        raise RuntimeError(f"git {' '.join(args[:2])} failed: {result.stderr.strip()}")
    return result


def lines(repo, *args):
    return [line for line in git(repo, *args).stdout.splitlines() if line]


def names(repo, *args):
    return [name for name in git(repo, *args).stdout.split("\0") if name]


def resolve_base(repo, name):
    for candidate in (name,) if name else BASES:
        if git(repo, "rev-parse", "--verify", "--quiet", f"{candidate}^{{commit}}", check=False).returncode == 0:
            return candidate
    raise RuntimeError(f"no base ref found ({name or ', '.join(BASES)}); fetch the upstream remote first")


def git_version(repo):
    match = re.match(r"git version (\d+)\.(\d+)", git(repo, "version", check=False).stdout)
    return (int(match.group(1)), int(match.group(2))) if match else None


def conflicts(repo, base, head):
    result = git(repo, "merge-tree", "-z", "--write-tree", "--name-only", "--no-messages", base, head, check=False)
    if result.returncode not in (0, 1):
        version = git_version(repo)
        if version and version < (2, 38):
            raise RuntimeError(f"git merge-tree --write-tree needs git 2.38 or newer; this git is {version[0]}.{version[1]}")
        raise RuntimeError(f"git merge-tree failed: {result.stderr.strip()}")
    if result.returncode == 0:
        return []
    fields = result.stdout.split("\0")[1:]
    return fields[:fields.index("")] if "" in fields else fields


def merged_pull_requests(repo, mergebase, base, paths):
    found = {}
    if not paths:
        return found
    for line in lines(repo, "--literal-pathspecs", "-c", "core.quotepath=off", "log", "--first-parent", "--format=%h %s", f"{mergebase}..{base}", "--", *paths):
        match = PULL_REQUEST.search(line)
        if match:
            found[int(match.group(1) or match.group(2))] = line.split(" ", 1)[0]
    return found


def report(repo, base, head):
    repo = git(repo, "rev-parse", "--show-toplevel").stdout.strip()
    found = git(repo, "merge-base", base, head, check=False)
    if found.returncode == 1:
        raise RuntimeError(f"{head} and {base} have no common ancestor")
    if found.returncode:
        raise RuntimeError(f"git merge-base failed: {found.stderr.strip()}")
    mergebase = found.stdout.strip()
    behind = int(git(repo, "rev-list", "--count", f"{head}..{base}", "--").stdout)
    ahead = int(git(repo, "rev-list", "--count", f"{base}..{head}", "--").stdout)
    mine = set(names(repo, "diff", "-z", "--name-only", mergebase, head, "--"))
    theirs = set(names(repo, "diff", "-z", "--name-only", mergebase, base, "--"))
    shared = sorted(mine & theirs)
    return {
        "base": base,
        "head": head,
        "mergebase": mergebase,
        "behind": behind,
        "ahead": ahead,
        "conflicts": conflicts(repo, base, head),
        "shared": shared,
        "pulls": merged_pull_requests(repo, mergebase, base, shared),
    }


def render(data):
    out = [f"{data['head']} is {data['ahead']} ahead and {data['behind']} behind {data['base']} (merge base {data['mergebase'][:9]})"]
    if data["conflicts"]:
        out.append(f"merge: CONFLICT in {len(data['conflicts'])} file(s)")
        out += [f"  {name}" for name in data["conflicts"]]
    else:
        out.append("merge: clean")
    if data["shared"]:
        out.append(f"{len(data['shared'])} file(s) changed on both sides since the merge base:")
        out += [f"  {name}" for name in data["shared"]]
        if data["pulls"]:
            out.append("merged pull requests that touched them: " + ", ".join(f"#{number}" for number in sorted(data["pulls"])))
    return "\n".join(out)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description="report whether a branch is in step with the upstream main before you push or test")
    parser.add_argument("--base", help="the main ref to compare against (default: upstream/main, then origin/main, then main)")
    parser.add_argument("--head", default="HEAD", help="the branch or commit to check (default: HEAD)")
    parser.add_argument("--repo", type=Path, default=Path("."), help="the repository to check (default: the current directory)")
    parser.add_argument("--fetch", action="store_true", help="fetch the base's remote first")
    parser.add_argument("--strict", action="store_true", help="also fail when the branch is behind the base")
    parser.add_argument("--json", action="store_true", help="write the report as JSON instead of text")
    args = parser.parse_args(argv)
    try:
        def fetch(ref):
            remote, _, branch = ref.partition("/")
            if not branch:
                parser.error("--fetch needs a remote-tracking base such as upstream/main")
            git(args.repo, "fetch", "--quiet", remote, branch)

        if args.fetch:
            if args.base:
                fetch(args.base)
            else:
                remotes = set(lines(args.repo, "remote"))
                for candidate in BASES:
                    remote, _, branch = candidate.partition("/")
                    if branch and remote in remotes:
                        fetch(candidate)
                        break
        base = resolve_base(args.repo, args.base)
        data = report(args.repo, base, args.head)
    except (RuntimeError, OSError) as error:
        parser.error(str(error))
    print(json.dumps(data, ensure_ascii=True) if args.json else render(data))
    if data["conflicts"] or (args.strict and data["behind"]):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
