import argparse
import json
import os
import re
import subprocess
from itertools import combinations
from pathlib import Path
from urllib.parse import quote, urlencode

PARENT = 'if .parent then "\\(.parent.owner.login)/\\(.parent.name)" else .nameWithOwner end'
EXPORT = re.compile(r"^\+.*\bAPS5_VABI\s+(\w+)\s*\(")
DEPENDS = re.compile(r'Depends on:[ \t]*((?:#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*|none\b)?(?:\r?\n[ \t]*[-*][ \t]*#\d+\b[^\r\n]*)*)')
PLAIN_DEPENDS = re.compile(r'Depends on[ \t]+(#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*)')
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))?", re.M)
MARKER = "<!-- pr-overlap -->"
FETCH_REFS = 100


def git(*args):
    result = subprocess.run(["git", "-c", "core.quotePath=false", *args], capture_output=True)
    result.stdout = result.stdout.decode("utf-8", errors="replace")
    result.stderr = result.stderr.decode("utf-8", errors="replace")
    result.check_returncode()
    return result.stdout


def own(prs, sha):
    return {n: pr for n, pr in prs.items()
            if pr["sha"] != sha and subprocess.run(["git", "merge-base", "--is-ancestor", pr["sha"], sha], capture_output=True).returncode != 0}


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8", check=True).stdout


def upstream():
    return os.environ.get("GITHUB_REPOSITORY") or gh("repo", "view", "--json", "nameWithOwner,parent", "--jq", PARENT).strip()


def depends_on(body):
    body = re.sub(r"<!--.*?-->", "", body or "", flags=re.S)
    match = DEPENDS.search(body)
    if not match or not match.group(1).strip():
        match = PLAIN_DEPENDS.search(body)
    return {int(n) for n in re.findall(r"#(\d+)", match.group(1))} if match else set()


def open_prs(base):
    query = urlencode({"state": "open", "base": base, "per_page": 100})
    out = gh("api", "--paginate", f"repos/{REPO}/pulls?{query}")
    prs = {}
    decoder = json.JSONDecoder()
    offset = 0
    page_count = 0
    pull_requests = []
    while offset < len(out):
        while offset < len(out) and out[offset].isspace():
            offset += 1
        if offset == len(out):
            break
        page, offset = decoder.raw_decode(out, offset)
        if not isinstance(page, list):
            raise ValueError("gh api --paginate returned a non-array page")
        page_count += 1
        pull_requests.extend(page)
    if page_count == 0:
        raise ValueError("gh api --paginate returned no JSON pages")

    for pr in pull_requests:
        prs[pr["number"]] = {
            "ref": f"refs/pr/{pr['number']}",
            "sha": pr["head"]["sha"],
            "author": pr["user"]["login"],
            "depends": depends_on(pr["body"]),
        }
    return prs


def fetch(base, prs):
    refspecs = [f"+refs/heads/{base}:refs/pr/base"] + [f"+{pr['sha']}:{pr['ref']}" for pr in prs.values()]
    for start in range(0, len(refspecs), FETCH_REFS):
        git("fetch", "--no-tags", "-q", f"https://github.com/{REPO}.git", *refspecs[start:start + FETCH_REFS])


def merge(*args):
    result = subprocess.run(["git", "-c", "core.quotePath=false", "merge-tree", "--write-tree", "--name-only", "--no-messages", "-z", *args],
                            capture_output=True)
    lines = result.stdout.decode("utf-8", errors="replace").rstrip("\0").split("\0")
    if result.returncode not in (0, 1) or not lines[0]:
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git merge-tree {' '.join(args)} failed: {error}")
    return lines[0], set(lines[1:]) if result.returncode else set()


def commit(tree):
    return git("-c", "user.name=pr_overlap", "-c", "user.email=pr_overlap", "commit-tree", tree,
               "-p", "refs/pr/base", "-m", "pr_overlap").strip()


def exports(diff):
    found = set()
    lines = diff.splitlines()
    for i, line in enumerate(lines):
        m = EXPORT.match(line)
        if not m or m.group(1).endswith(("_nid_no_patch", "_nid_no_patch_cut")):
            continue
        text = line[1:]
        for nxt in lines[i + 1:]:
            if ";" in text or "{" in text or not nxt.startswith("+") or nxt.startswith("+++"):
                break
            text += nxt[1:]
        semi, brace = text.find(";"), text.find("{")
        if semi < 0 or 0 <= brace < semi:
            found.add(m.group(1))
    return found


def scan(pr):
    tree, conflicted = merge("refs/pr/base", pr["ref"])
    pr["tree"] = None if conflicted else commit(tree)
    old, new = (git("merge-base", "refs/pr/base", pr["ref"]).strip(), pr["ref"]) if conflicted else ("refs/pr/base", pr["tree"])
    pr["files"], pr["added"] = set(), set()
    changes = git("diff", "--name-status", "--no-renames", "-z", old, new).split("\0")
    for status, path in zip(changes[::2], changes[1::2]):
        pr["files"].add(path)
        if status == "A":
            pr["added"].add(path)
    diff = git("diff", "-U0", "--no-color", old, new, "--", "core/libs/prx/*.cpp")
    pr["exports"] = exports(diff)


def hunks(pr, path):
    diff = git("diff", "-U0", "--no-color", "refs/pr/base", pr["tree"], "--", path)
    found = []
    for start, count in HUNK.findall(diff):
        start = max(int(start), 1)
        found.append((start, start + max(int(count or 1), 1) - 1))
    return found


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
    files = merge(a["tree"], b["tree"])[1]
    return {path: [] if path in a["added"] else clashes(a, b, path) for path in files}


def dependencies(prs, number):
    found, pending = set(), [number]
    while pending:
        for n in prs[pending.pop()]["depends"]:
            if n in prs and n not in found:
                found.add(n)
                pending.append(n)
    return found


def overlaps(prs, a, b):
    pa, pb = prs[a], prs[b]
    da, db = dependencies(prs, a), dependencies(prs, b)
    if b in da or a in db:
        return None
    shared = [prs[n] for n in da & db]
    found = {"added": (pa["added"] & pb["added"]).difference(*(pr["added"] for pr in shared))}
    if pa["author"] != pb["author"]:
        found["conflicts"] = conflicts(pa, pb)
        found["exports"] = (pa["exports"] & pb["exports"]).difference(*(pr["exports"] for pr in shared))
        if not (pa["tree"] and pb["tree"]):
            uncompared = pa.get("files", set()) & pb.get("files", set())
            if uncompared:
                found["uncompared"] = uncompared
    return found if any(found.values()) else None


def cell(items):
    return "<br>".join(f"`{item}`" for item in sorted(items or ()))


def spans(conflicts, blob):
    return "<br>".join(f"`{path}` " + ", ".join(f"[L{s}-{e}]({blob}/{quote(path)}#L{s}-L{e})" if s != e else f"[L{s}]({blob}/{quote(path)}#L{s})"
                                               for s, e in ranges) for path, ranges in sorted((conflicts or {}).items()))


def pull(blob, number):
    return f'[PR {number}]({blob.split("/blob/", 1)[0].replace("https://github.com/", "https://www.github.com/", 1)}/pull/{number})'


def report(rows, blob):
    if not rows:
        return ""
    confirmed = {number: found for number, found in rows.items()
                 if found.get("conflicts") or found.get("exports") or found["added"]}
    uncompared = {number: found["uncompared"] for number, found in rows.items() if found.get("uncompared")}
    lines = [MARKER]
    if confirmed:
        lines.extend([
            "Other open pull requests touch the same code. Merging one will break the other, "
            "or both implement the same thing. If this one builds on them, list them in **Depends on**.",
            "",
            "| Pull request | Conflicting files (lines on `main`) | Same exports | Same new files |",
            "|---|---|---|---|",
        ])
        for number, found in sorted(confirmed.items()):
            lines.append(f'| {pull(blob, number)} | {spans(found.get("conflicts"), blob)} | {cell(found.get("exports"))} | {cell(found["added"])} |')
    if uncompared:
        if confirmed:
            lines.append("")
        lines.extend([
            "Some comparisons could not be completed because a pull request conflicts with the base branch. "
            "Review these shared files manually before concluding that there is no overlap.",
            "",
            "| Pull request | Shared files not compared |",
            "|---|---|",
        ])
        for number, files in sorted(uncompared.items()):
            lines.append(f"| {pull(blob, number)} | {cell(files)} |")
    return "\n".join(lines) + "\n"


def write_reports(out, rows, blob):
    out.mkdir(parents=True, exist_ok=True)
    for number, found in rows.items():
        (out / f"{number}.md").write_text(report(found, blob), encoding="utf-8")


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
        prs = own(prs, sha)
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
    write_reports(args.out, rows, blob)
