import argparse
import json
import os
import re
import subprocess
from itertools import combinations
from pathlib import Path

PARENT = 'if .parent then "\\(.parent.owner.login)/\\(.parent.name)" else .nameWithOwner end'
EXPORT = re.compile(r"^\+.*\bAPS5_VABI\s+(\w+)\s*\(")
DEPENDS = re.compile(r'Depends on:([^\r\n]*(?:\r?\n[ \t]*[-*][ \t]*#\d+[^\r\n]*)*)')
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))?", re.M)
MARKER = "<!-- pr-overlap -->"


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8", check=True).stdout


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8", check=True).stdout


def upstream():
    return os.environ.get("GITHUB_REPOSITORY") or gh("repo", "view", "--json", "nameWithOwner,parent", "--jq", PARENT).strip()


def open_prs(base):
    out = gh("pr", "list", "-R", REPO, "--base", base, "-L", "200", "--json", "number,author,headRefOid,body")
    prs = {}
    for pr in json.loads(out):
        depends = DEPENDS.search(pr["body"] or "")
        prs[pr["number"]] = {
            "ref": f"refs/pr/{pr['number']}",
            "sha": pr["headRefOid"],
            "author": pr["author"]["login"],
            "depends": {int(n) for n in re.findall(r"#(\d+)", depends.group(1))} if depends else set(),
        }
    return prs


def fetch(base, prs):
    refspecs = [f"+refs/heads/{base}:refs/pr/base"] + [f"+refs/pull/{n}/head:{pr['ref']}" for n, pr in prs.items()]
    git("fetch", "--no-tags", "-q", f"https://github.com/{REPO}.git", *refspecs)


def merge(*args):
    result = subprocess.run(["git", "merge-tree", "--write-tree", "--name-only", "--no-messages", *args],
                            capture_output=True, text=True, encoding="utf-8")
    lines = result.stdout.splitlines()
    if result.returncode not in (0, 1) or not lines:
        raise RuntimeError(f"git merge-tree {' '.join(args)} failed: {result.stderr.strip()}")
    return lines[0], set(lines[1:]) if result.returncode else set()


def commit(tree):
    return git("-c", "user.name=pr_overlap", "-c", "user.email=pr_overlap", "commit-tree", tree,
               "-p", "refs/pr/base", "-m", "pr_overlap").strip()


def scan(pr):
    tree, conflicted = merge("refs/pr/base", pr["ref"])
    pr["tree"] = None if conflicted else commit(tree)
    old, new = (git("merge-base", "refs/pr/base", pr["ref"]).strip(), pr["ref"]) if conflicted else ("refs/pr/base", pr["tree"])
    pr["files"], pr["added"] = set(), set()
    for line in git("diff", "--name-status", "--no-renames", old, new).splitlines():
        status, path = line.split("\t", 1)
        pr["files"].add(path)
        if status == "A":
            pr["added"].add(path)
    diff = git("diff", "-U0", "--no-color", old, new, "--", "core/libs/prx/*.cpp")
    pr["exports"] = {m.group(1) for m in map(EXPORT.match, diff.splitlines())
                     if m and not m.group(1).endswith("_nid_no_patch")}


def hunks(pr, path):
    diff = git("diff", "-U0", "--no-color", "refs/pr/base", pr["tree"], "--", path)
    return [(int(start), int(start) + max(int(count or 1), 1) - 1) for start, count in HUNK.findall(diff)]


def clashes(a, b, path):
    ranges = sorted((min(s1, s2), max(e1, e2)) for s1, e1 in hunks(a, path) for s2, e2 in hunks(b, path)
                    if s1 <= e2 + 1 and s2 <= e1 + 1)
    merged = []
    for start, end in ranges:
        if merged and start <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((start, end))
    return merged


def conflicts(a, b):
    if not (a["tree"] and b["tree"] and a["files"] & b["files"]):
        return {}
    files = merge("--merge-base=refs/pr/base", a["tree"], b["tree"])[1]
    return {path: [] if path in a["added"] else clashes(a, b, path) for path in files}


def overlaps(prs, a, b):
    pa, pb = prs[a], prs[b]
    if b in pa["depends"] or a in pb["depends"]:
        return None
    found = {"added": pa["added"] & pb["added"]}
    if pa["author"] != pb["author"]:
        found["conflicts"] = conflicts(pa, pb)
        found["exports"] = pa["exports"] & pb["exports"]
    return found if any(found.values()) else None


def cell(items):
    return "<br>".join(f"`{item}`" for item in sorted(items or ()))


def spans(conflicts, blob):
    return "<br>".join(f"`{path}` " + ", ".join(f"[L{s}-{e}]({blob}/{path}#L{s}-L{e})" if s != e else f"[L{s}]({blob}/{path}#L{s})"
                                               for s, e in ranges) for path, ranges in sorted((conflicts or {}).items()))


def report(rows, blob):
    if not rows:
        return ""
    lines = [MARKER,
             "Other open pull requests touch the same code. Merging one will break the other, "
             "or both implement the same thing. If this one builds on them, list them in **Depends on**.",
             "",
             "| Pull request | Conflicting files (lines on `main`) | Same exports | Same new files |",
             "|---|---|---|---|"]
    for number, found in sorted(rows.items()):
        lines.append(f'| #{number} | {spans(found.get("conflicts"), blob)} | {cell(found.get("exports"))} | {cell(found["added"])} |')
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="report open pull requests that overlap each other")
    parser.add_argument("--base", default="main")
    parser.add_argument("--out", type=Path, help="write <number>.md for every open pull request (empty when clean)")
    parser.add_argument("--branch", help="check a local branch against the open pull requests instead")
    parser.add_argument("--repo", help="owner/name; defaults to the upstream of the current checkout")
    args = parser.parse_args()
    if bool(args.out) == bool(args.branch):
        parser.error("pass exactly one of --out or --branch")
    REPO = args.repo or upstream()
    prs = open_prs(args.base)
    fetch(args.base, prs)
    if args.branch:
        sha = git("rev-parse", args.branch).strip()
        prs = {n: pr for n, pr in prs.items() if pr["sha"] != sha}
        prs[0] = {"ref": sha, "author": None, "depends": set()}
    for pr in prs.values():
        scan(pr)
    rows = {n: {} for n in prs}
    pairs = [(0, n) for n in prs if n] if args.branch else combinations(prs, 2)
    for a, b in pairs:
        if found := overlaps(prs, a, b):
            rows[a][b] = rows[b][a] = found
    blob = f'https://github.com/{REPO}/blob/{git("rev-parse", "refs/pr/base").strip()}'
    if args.branch:
        print(report(rows[0], blob) or "No overlap with open pull requests.", end="")
        raise SystemExit
    args.out.mkdir(parents=True, exist_ok=True)
    for number, found in rows.items():
        (args.out / f"{number}.md").write_text(report(found, blob))
