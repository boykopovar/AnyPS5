import argparse
import io
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from itertools import combinations
from pathlib import Path
import tarfile
from urllib.parse import quote, urlencode

import nid_names

PARENT = 'if .parent then "\\(.parent.owner.login)/\\(.parent.name)" else .nameWithOwner end'
EXPORT = re.compile(r"^\+.*\bAPS5_VABI\s+(\w+)\s*\(")
NID_DISAMBIG = re.compile(r"_nid_disambig[0-9]+$")
DEPENDS = re.compile(r'Depends on:[ \t]*((?:#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*|none\b)?(?:\r?\n[ \t]*[-*][ \t]*#\d+\b[^\r\n]*)*)')
PLAIN_DEPENDS = re.compile(r'Depends on[ \t]+(#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*)')
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))?", re.M)
MARKER = "<!-- pr-overlap -->"
FETCH_REFS = 100
_REPO_ROOTS = {}


def git(*args):
    cwd = Path.cwd()
    root = _REPO_ROOTS.get(cwd)
    if root is None:
        root = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"], cwd=cwd, capture_output=True, check=True
        ).stdout.decode("utf-8", errors="replace").strip()
        _REPO_ROOTS[cwd] = root
    args = list(args)
    if "--" in args:
        separator = args.index("--")
        for index in range(separator + 1, len(args)):
            pathspec = args[index]
            if pathspec.startswith(("../", "..\\")):
                absolute = (cwd / pathspec).resolve()
                try:
                    args[index] = absolute.relative_to(Path(root)).as_posix()
                except ValueError:
                    pass
    result = subprocess.run(
        ["git", "-c", "core.quotePath=false", *args],
        cwd=root,
        capture_output=True,
    )
    result.stdout = result.stdout.decode("utf-8", errors="replace")
    result.stderr = result.stderr.decode("utf-8", errors="replace")
    result.check_returncode()
    return result.stdout


def own(prs, sha):
    ancestors = set(git("rev-list", sha).splitlines())
    return {n: pr for n, pr in prs.items() if pr["sha"] not in ancestors}


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8", check=True).stdout


def upstream():
    return os.environ.get("GITHUB_REPOSITORY") or gh("repo", "view", "--json", "nameWithOwner,parent", "--jq", PARENT).strip()


def depends_on(body):
    body = re.sub(r"<!--.*?-->", "", body or "", flags=re.S)
    repo = re.escape(globals().get("REPO", "boykopovar/AnyPS5"))
    repo_pattern = rf"(?:{repo}|boykopovar/AnyPS5)"
    body = re.sub(rf"\[([^\]]*)\]\(https?://(?:www\.)?github\.com/{repo_pattern}/pull/(\d+)(?:[#/?][^)\s]*)?\)", r"#\2", body, flags=re.IGNORECASE)
    body = re.sub(rf"\[([^\]]*)\]\(https?://(?:www\.)?github\.com/(?!{repo_pattern}/)[^/\s]+/[^/\s]+/pull/\d+[^)\s]*\)", "", body, flags=re.IGNORECASE)
    body = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", body)
    body = re.sub(rf"https?://(?:www\.)?github\.com/{repo_pattern}/pull/(\d+)\b(?:/[^\s]*)?", r"#\1", body, flags=re.IGNORECASE)
    match = next((match for match in DEPENDS.finditer(body) if match.group(1).strip()), None) or PLAIN_DEPENDS.search(body)
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
    cwd = Path.cwd()
    root = _REPO_ROOTS.get(cwd)
    if root is None:
        root = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"], cwd=cwd, capture_output=True, check=True
        ).stdout.decode("utf-8", errors="replace").strip()
        _REPO_ROOTS[cwd] = root
    result = subprocess.run(["git", "-c", "core.quotePath=false", "merge-tree", "--write-tree", "--name-only", "--no-messages", "-z", *args],
                            cwd=root, capture_output=True)
    lines = result.stdout.decode("utf-8", errors="replace").rstrip("\0").split("\0")
    if result.returncode not in (0, 1) or not lines[0]:
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git merge-tree {' '.join(args)} failed: {error}")
    return lines[0], set(lines[1:]) if result.returncode else set()


def commit(tree):
    return git("-c", "user.name=pr_overlap", "-c", "user.email=pr_overlap", "commit-tree", tree,
               "-p", "refs/pr/base", "-m", "pr_overlap").strip()


def exports(diff, prefix="+"):
    found = set()
    lines = diff.splitlines()
    for i, line in enumerate(lines):
        if not line.startswith(prefix) or line.startswith(prefix * 3):
            continue
        m = EXPORT.match(prefix + line[1:])
        if not m or m.group(1).endswith(("_nid_no_patch", "_nid_no_patch_cut")):
            continue
        text = line[1:]
        for nxt in lines[i + 1:]:
            if ";" in text or "{" in text or not nxt.startswith(prefix) or nxt.startswith(prefix * 3):
                break
            text += nxt[1:]
        semi, brace = text.find(";"), text.find("{")
        if semi < 0 or 0 <= brace < semi:
            name = m.group(1).removesuffix("_nid_postfix")
            found.add(NID_DISAMBIG.sub("", name))
    return found


def changed_source(diff, prefix):
    files, current, path = [], [], None
    for line in diff.splitlines():
        if line.startswith("diff --git "):
            if current and path:
                files.append((path, "\n".join(current)))
            path = line.split(" b/", 1)[1] if " b/" in line else None
            current = []
        elif line.startswith(prefix) and not line.startswith(prefix * 3):
            current.append(line[1:])
    if current and path:
        files.append((path, "\n".join(current)))
    return files


def library_for_path(path):
    parts = Path(path).parts
    return "/".join(parts[:4]) if len(parts) > 3 and parts[:3] == ("core", "libs", "prx") else ""


def source_exports(text, library=""):
    found = {}
    for match in nid_names.source_definitions(text):
        name = nid_names.strip_nid_postfix(match.group(1))
        if not name.endswith(("_nid_no_patch", "_nid_no_patch_cut")):
            found.setdefault((library, nid_names.compute_nid(name)), set()).add(name)
    for name in nid_names.source_assembly_definitions(text):
        name = nid_names.strip_nid_postfix(name)
        if not name.endswith(("_nid_no_patch", "_nid_no_patch_cut")):
            found.setdefault((library, nid_names.compute_nid(name)), set()).add(name)
    for nid, name in nid_names.source_aliases(text):
        found.setdefault((library, nid), set()).add(name)
    return found


def source_changed_exports(diff, prefix):
    found = {}
    for path, text in changed_source(diff, prefix):
        library = library_for_path(path)
        for key, names in source_exports(text, library).items():
            found.setdefault(key, set()).update(names)
        file_diff = "\n".join(prefix + line for line in text.splitlines())
        for name in exports(file_diff, prefix):
            canonical = nid_names.strip_nid_postfix(name)
            found.setdefault((library, nid_names.compute_nid(canonical)), set()).add(canonical)
    return found


def base_exports(ref):
    root = git("rev-parse", "--show-toplevel").strip()
    archive = subprocess.run(
        ["git", "-C", root, "archive", "--format=tar", ref, "core/libs"], capture_output=True, check=True
    ).stdout
    found = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as source:
        for entry in source:
            path = Path(entry.name)
            if not entry.isfile() or path.suffix != ".cpp" or any(
                    part.lower() in {"test", "tests"} for part in path.parts[2:-1]):
                continue
            text = source.extractfile(entry).read().decode("utf-8", errors="replace")
            library = library_for_path(entry.name)
            for key, names in source_exports(text, library).items():
                found.setdefault(key, set()).update(names)
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
    pr["added_exports"] = source_changed_exports(diff, "+")
    pr["removed_exports"] = source_changed_exports(diff, "-")


def scan_all(prs, jobs):
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        list(executor.map(scan, prs.values()))


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



def code_span(item):
    item = str(item)
    runs = re.findall(r"`+", item)
    delimiter = "`" * (max((len(run) for run in runs), default=0) + 1)
    if "`" in item:
        return f"{delimiter} {item} {delimiter}"
    return f"{delimiter}{item}{delimiter}"


def cell(items):
    return "<br>".join(code_span(item) for item in sorted(items or ()))




def spans(conflicts, blob):
    return "<br>".join(
        f"{code_span(path)} " + ", ".join(
            f"[L{s}-{e}]({blob}/{quote(path)}#L{s}-L{e})"
            if s != e else f"[L{s}]({blob}/{quote(path)}#L{s})"
            for s, e in ranges
        )
        for path, ranges in sorted((conflicts or {}).items())
    )


def pull(blob, number):
    return f'[PR {number}]({blob.split("/blob/", 1)[0].replace("https://github.com/", "https://www.github.com/", 1)}/pull/{number})'


def report(rows, blob, main_exports=None):
    if not rows:
        if not main_exports:
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
    if main_exports:
        if confirmed:
            lines.append("")
        lines.append("Exports introduced here that are already present in `main`:")
        lines.append("")
        lines.append(f"- {cell(main_exports)}")
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


def write_reports(out, rows, blob, main_exports=None):
    main_exports = main_exports or {}
    out.mkdir(parents=True, exist_ok=True)
    for number, found in rows.items():
        (out / f"{number}.md").write_text(report(found, blob, main_exports.get(number)), encoding="utf-8")


def main_export_findings(prs, current_exports):
    findings = {}
    for number, pr in prs.items():
        candidates = set(pr.get("added_exports", {})) - set(pr.get("removed_exports", {}))
        names = set()
        for key in candidates & set(current_exports):
            names.update(pr["added_exports"][key])
            names.update(current_exports[key])
        if names:
            findings[number] = names
    return findings


def describe_failure(error):
    command = " ".join(str(part) for part in error.cmd)
    message = (error.stderr or "").strip() or "no error output"
    return f"pr_overlap: {command} failed with exit code {error.returncode}: {message}"


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="report open pull requests that overlap each other")
    parser.add_argument("--base", default="main")
    parser.add_argument("--out", type=Path, help="write <number>.md for every open pull request (empty when clean)")
    parser.add_argument("--branch", help="check a local branch against the open pull requests instead")
    parser.add_argument("--repo", help="owner/name; defaults to the upstream of the current checkout")
    parser.add_argument("--jobs", type=int, default=4, help="number of pull request comparisons to run at once")
    args = parser.parse_args(argv)
    if bool(args.out) == bool(args.branch):
        parser.error("pass exactly one of --out or --branch")
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    global REPO
    try:
        REPO = args.repo or upstream()
        prs = open_prs(args.base)
        fetch(args.base, prs)
        if args.branch:
            sha = git("rev-parse", args.branch).strip()
            prs = own(prs, sha)
            prs[0] = {"ref": sha, "author": None, "depends": set()}
        scan_all(prs, args.jobs)
        current_exports = base_exports("refs/pr/base")
        rows = {n: {} for n in prs}
        main_exports = main_export_findings(prs, current_exports)
        pairs = [(0, n) for n in prs if n] if args.branch else combinations(prs, 2)
        for a, b in pairs:
            if found := overlaps(prs, a, b):
                rows[a][b] = rows[b][a] = found
        blob = f'https://github.com/{REPO}/blob/{git("rev-parse", "refs/pr/base").strip()}'
    except subprocess.CalledProcessError as error:
        raise SystemExit(describe_failure(error))
    if args.branch:
        print(report(rows[0], blob, main_exports.get(0)) or "No overlap with open pull requests.", end="")
        return 0
    write_reports(args.out, rows, blob, main_exports)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
