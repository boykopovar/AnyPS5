import argparse
import codecs
import hashlib
import importlib
import io
import json
import os
import re
import subprocess
import sys
import tarfile
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from itertools import combinations
from pathlib import Path
from urllib.parse import parse_qs, quote, urlencode, urlsplit

import nid_names

PARENT = 'if .parent then "\\(.parent.owner.login)/\\(.parent.name)" else .nameWithOwner end'
EXPORT = re.compile(r"^[+-].*\bAPS5_VABI\s+(\w+)\s*\(")
NID_DISAMBIG = re.compile(r"_nid_disambig[0-9]+$")
NAME_NOISE = re.compile(r"[\s\x00-\x1f\x7f]")
DEPENDS = re.compile(r'Depends on:[ \t]*((?:#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*|none\b)?(?:\r?\n[ \t]*[-*][ \t]*#\d+\b[^\r\n]*)*)')
PLAIN_DEPENDS = re.compile(r'Depends on[ \t]+(#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)#\d+\b)*)')
CLOSING = r"(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)"
CLOSES = re.compile(rf"(?<![\w-]){CLOSING}:?[ \t]+#(\d+)\b", re.IGNORECASE)
FIXES = re.compile(rf"(?<![\w-])Fixes:[ \t]*((?:{CLOSING}[ \t]+)?#\d+\b(?:(?:[ \t]*,[ \t]*(?:and[ \t]+)?|[ \t]+and[ \t]+)(?:{CLOSING}[ \t]+)?#\d+\b)*)", re.IGNORECASE)
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))?", re.M)
MARKER = "<!-- pr-overlap -->"
FETCH_REFS = 100
PR_PAGE_SIZE = 100
MAX_PR_PAGES = 1000
BASE_REF = "refs/pr/base"
PROGRESS_EVERY = 100
PAIR_PROGRESS_EVERY = 1000
MAX_REPORT_CHARS = 60000
EXPORT_PATHS = ("core/libs/prx/*.cpp", ":(exclude,icase)**/test/**", ":(exclude,icase)**/tests/**")
_REPO_ROOTS = {}
GIT_CONFIG = ("-c", "gc.auto=0", "-c", "maintenance.auto=false")


def _repository_root():
    cwd = Path.cwd()
    root = _REPO_ROOTS.get(cwd)
    if root is None:
        output = subprocess.run(
            ["git", *GIT_CONFIG, "rev-parse", "--show-toplevel"],
            cwd=cwd,
            capture_output=True,
            check=True,
        ).stdout.decode("utf-8", errors="replace").strip()
        root = Path(output)
        if os.name == "nt" and not root.is_dir():
            relative = subprocess.run(
                ["git", *GIT_CONFIG, "rev-parse", "--show-cdup"],
                cwd=cwd,
                capture_output=True,
                check=True,
            ).stdout.decode("utf-8", errors="replace").strip()
            root = (cwd / relative).resolve()
        _REPO_ROOTS[cwd] = root
    return root


def git(*args):
    cwd = Path.cwd()
    root = _repository_root()
    args = list(args)
    if "--" in args:
        separator = args.index("--")
        for index in range(separator + 1, len(args)):
            pathspec = args[index]
            if pathspec.startswith(("../", "..\\")):
                absolute = cwd / pathspec
                try:
                    absolute = absolute.resolve()
                except RuntimeError as error:
                    raise ValueError(f"cannot resolve path {pathspec!r}: {error}") from error
                except OSError as error:
                    raise ValueError(f"cannot resolve path {pathspec!r}: {error}") from error
                if absolute.is_symlink():
                    try:
                        absolute.stat()
                    except OSError as error:
                        raise ValueError(f"cannot resolve path {pathspec!r}: {error}") from error
                try:
                    args[index] = absolute.relative_to(Path(root)).as_posix()
                except ValueError:
                    pass
    result = subprocess.run(
        ["git", "-c", "core.quotePath=false", *GIT_CONFIG, *args],
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


def without_own_pull_request(prs, name, repository=None):
    if repository is None:
        return prs
    matching = [number for number, pr in prs.items()
                if pr.get("branch") == name and pr.get("head_repo", "").casefold() == repository.casefold()]
    if len(matching) == 1:
        del prs[matching[0]]
    return prs


def push_head(name):
    ref = f"refs/heads/{name}"
    try:
        rows = git("for-each-ref", "--format=%(refname)%09%(push:remotename)%09%(push:remoteref)", ref)
        for row in rows.splitlines():
            fields = row.split("\t")
            if len(fields) != 3:
                return name, None
            local, remote, destination = fields
            if local != ref or not remote or not destination.startswith("refs/heads/"):
                continue
            urls = git("remote", "get-url", "--push", "--all", remote).splitlines()
            if len(urls) == 1:
                match = re.fullmatch(r"(?:https?://github\.com/|git@github\.com:|ssh://git@github\.com(?::22)?/)"
                                     r"([\w.-]+/[\w.-]+?)(?:\.git)?/?", urls[0], re.IGNORECASE)
                if match:
                    return destination.removeprefix("refs/heads/"), match[1]
    except subprocess.CalledProcessError:
        pass
    return name, None


def gh(*args, attempts=3, delay=0.5):
    for attempt in range(attempts):
        try:
            result = subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8")
        except FileNotFoundError as error:
            raise ValueError("GitHub CLI 'gh' is required on PATH") from error
        if result.returncode == 0:
            return result.stdout
        transient = any(code in result.stderr for code in ("500", "502", "503", "504", "server error", "connection reset", "Connection reset", "timed out", "i/o timeout", "TLS handshake", "stream error:", "unexpected EOF",
                                                          "A connection attempt failed because the connected party did not properly respond"))
        if attempt + 1 == attempts or not transient:
            raise subprocess.CalledProcessError(result.returncode, ["gh", *args], output=result.stdout, stderr=result.stderr)
        time.sleep(delay * (2 ** attempt))


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


def fixes(body):
    body = re.sub(r"<!--.*?-->", "", body or "", flags=re.S)
    repo = re.escape(globals().get("REPO", "boykopovar/AnyPS5"))
    repo_pattern = rf"(?:{repo}|boykopovar/AnyPS5)"
    body = re.sub(rf"\[([^\]]*)\]\(https?://(?:www\.)?github\.com/{repo_pattern}/issues/(\d+)(?:[#/?][^)\s]*)?\)", r"#\2", body, flags=re.IGNORECASE)
    body = re.sub(rf"https?://(?:www\.)?github\.com/{repo_pattern}/issues/(\d+)\b(?:[#/?][^\s]*)?", r"#\1", body, flags=re.IGNORECASE)
    found = {int(n) for n in CLOSES.findall(body)}
    for match in FIXES.finditer(body):
        found.update(int(n) for n in re.findall(r"#(\d+)", match.group(1)))
    return found


def progress(message):
    print(f"pr_overlap: {message}", file=sys.stderr, flush=True)


LAST_PAGE = re.compile(r'[?&]page=(\d+)>; rel="last"')
NEXT_LINK = re.compile(r'<([^>]+)>\s*;\s*rel="next"', re.IGNORECASE)


def page(url):
    out = gh("api", "--include", url)
    if not out.startswith("HTTP/"):
        return "", out
    blank = re.search(r"\r?\n\r?\n", out)
    return (out, "") if blank is None else (out[:blank.start()], out[blank.end():])


def decode_pages(out):
    decoder = json.JSONDecoder()
    offset = 0
    pages = []
    while offset < len(out):
        while offset < len(out) and out[offset].isspace():
            offset += 1
        if offset == len(out):
            break
        found, offset = decoder.raw_decode(out, offset)
        if not isinstance(found, list):
            raise ValueError("gh api returned a non-array page")
        pages.append(found)
    return pages


def repair_escapes(text):
    repaired = []
    offset = 0
    while offset < len(text):
        character = text[offset]
        if character != "\\":
            repaired.append(character)
            offset += 1
            continue
        following = text[offset + 1:offset + 2]
        if following in ('"', "\\", "/", "b", "f", "n", "r", "t"):
            repaired.append(text[offset:offset + 2])
            offset += 2
        elif following == "u" and re.fullmatch(r"[0-9a-fA-F]{4}", text[offset + 2:offset + 6]):
            repaired.append(text[offset:offset + 6])
            offset += 6
        else:
            repaired.append("\\\\")
            offset += 1
    return "".join(repaired)


def json_pages(out):
    try:
        return decode_pages(out)
    except json.JSONDecodeError as error:
        if "escape" not in error.msg:
            raise
    return decode_pages(repair_escapes(out))


class PaginationChanged(ValueError):
    pass


def read_pr_pages(url, number):
    head, out = page(f"{url}&page={number}")
    pages = json_pages(out)
    if not pages:
        raise ValueError("gh api returned no JSON pages")
    return head, pages


def numbered_pr_page(url, number):
    head, pages = read_pr_pages(url, number)
    if len(pages) != 1:
        raise ValueError(f"gh api returned multiple JSON arrays for pull request page {number}")
    return head, pages[0]


def open_pr_pages(url):
    head, first = read_pr_pages(url, 1)
    last_link = LAST_PAGE.search(head)
    if not last_link and not NEXT_LINK.search(head):
        return first, 1
    if len(first) != 1:
        raise ValueError("gh api returned multiple JSON arrays for pull request page 1")
    observed = {1: first[0]}
    if last_link:
        last = int(last_link.group(1))
        if not 2 <= last <= MAX_PR_PAGES:
            raise ValueError("gh api returned an invalid last page link")
        head, tail = numbered_pr_page(url, last)
    else:
        last, tail = 1, first[0]
        while next_link := NEXT_LINK.search(head):
            values = parse_qs(urlsplit(next_link.group(1)).query, keep_blank_values=True).get("page", [])
            if len(values) != 1 or values[0] != str(last + 1) or last == MAX_PR_PAGES:
                raise ValueError("gh api returned an invalid next page link")
            last += 1
            head, tail = numbered_pr_page(url, last)
            observed[last] = tail

    if not 0 < len(tail) <= PR_PAGE_SIZE:
        raise PaginationChanged(f"unexpected length for last page {last}")
    descending = [tail]
    has_next = NEXT_LINK.search(head)
    if has_next or len(tail) == PR_PAGE_SIZE:
        extra_head, extra = numbered_pr_page(url, last + 1)
        if len(extra) >= PR_PAGE_SIZE or NEXT_LINK.search(extra_head) or (extra and last == MAX_PR_PAGES):
            raise PaginationChanged(f"pull request list grew past page {last + 1}")
        if has_next and (not extra or len(tail) != PR_PAGE_SIZE):
            raise PaginationChanged(f"last page {last} disagrees with its next link")
        if extra:
            observed[last] = tail
            descending = [extra]
            last += 1

    for number in range(last - 1, 0, -1):
        _, rows = numbered_pr_page(url, number)
        if not rows or len(rows) > PR_PAGE_SIZE or (last_link and len(rows) != PR_PAGE_SIZE):
            raise PaginationChanged(f"unexpected length for pull request page {number}")
        if number in observed and [pr["number"] for pr in rows] != [pr["number"] for pr in observed[number]]:
            raise PaginationChanged(f"pull request IDs changed on page {number}")
        descending.append(rows)
    return reversed(descending), last


def verify_open_prs(url, rows, last):
    newest = max(rows, default=0)
    for number in range(2, last + 1):
        _, found = numbered_pr_page(url, number)
        for pr in found:
            if pr["number"] not in rows and pr["number"] < newest:
                raise PaginationChanged(f"pull request #{pr['number']} on page {number} was missing from the scan")


def open_prs(base):
    query = urlencode({"state": "open", "base": base, "sort": "created", "direction": "asc", "per_page": PR_PAGE_SIZE})
    url = f"repos/{REPO}/pulls?{query}"
    for attempt in range(2):
        try:
            rows = {}
            pages, last = open_pr_pages(url)
            for found in pages:
                for pr in found:
                    number = pr["number"]
                    if number in rows:
                        raise PaginationChanged(f"duplicate pull request #{number} in paginated results")
                    rows[number] = pr
            verify_open_prs(url, rows, last)
            break
        except PaginationChanged as error:
            if attempt:
                raise ValueError(f"could not establish pull request pagination consistency after one retry: {error}") from error
            progress(f"pull request pagination changed ({error}); retrying once")
    prs = {}
    for number, pr in rows.items():
        prs[number] = {
            "ref": f"refs/pr/{number}",
            "sha": pr["head"]["sha"],
            "branch": pr["head"]["ref"],
            **({"head_repo": head_repo["full_name"]} if (head_repo := pr["head"].get("repo")) else {}),
            "author": pr["user"]["login"],
            "title": pr.get("title") or "",
            "depends": depends_on(pr["body"]),
            "fixes": fixes(pr["body"]),
        }
    progress(f"{len(prs)} open pull requests against {base}")
    return prs


def fetch(base, prs):
    return fetch_heads(base_commit(base), prs)


def base_commit(base):
    return git("ls-remote", "--exit-code", f"https://github.com/{REPO}.git", f"refs/heads/{base}").split()[0]


def missing_commits(commits):
    try:
        out = subprocess.run(
            ["git", "cat-file", "--batch-check=%(objectname) %(objecttype)"],
            cwd=_repository_root(), input="\n".join(commits) + "\n",
            capture_output=True, text=True, check=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        return list(commits)
    present = {line.split()[0] for line in out.splitlines() if line.endswith(" commit")}
    return [commit for commit in commits if commit not in present]


def fetch_heads(base_sha, prs, check_existing=False):
    remote = f"https://github.com/{REPO}.git"
    commits = [base_sha] + [pr["sha"] for pr in prs.values()]
    if check_existing:
        commits = missing_commits(commits)
    for start in range(0, len(commits), FETCH_REFS):
        batch = commits[start:start + FETCH_REFS]
        try:
            git("fetch", "--no-write-fetch-head", "--no-tags", "-q", remote, *batch)
        except subprocess.CalledProcessError:
            fetch_each(remote, batch, base_sha, prs)
        progress(f"fetched {min(start + FETCH_REFS, len(commits))}/{len(commits)} missing commits" if check_existing
                 else f"fetched {min(start + FETCH_REFS, len(commits)) - 1}/{len(prs)} pull request heads")
    for pr in prs.values():
        pr["ref"] = pr["sha"]
    return base_sha


def fetch_each(remote, batch, base_sha, prs):
    owners = {}
    for number, pr in prs.items():
        owners.setdefault(pr["sha"], []).append(number)
    for sha in batch:
        try:
            git("fetch", "--no-write-fetch-head", "--no-tags", "-q", remote, sha)
            continue
        except subprocess.CalledProcessError:
            if sha == base_sha or sha not in owners:
                raise
        for number in owners[sha]:
            moved = git("ls-remote", "--exit-code", remote, f"refs/pull/{number}/head").split()[0]
            if moved == sha:
                raise ValueError(f"cannot fetch the head {sha} of pull request #{number}")
            git("fetch", "--no-write-fetch-head", "--no-tags", "-q", remote, moved)
            progress(f"pull request #{number} moved from {sha[:10]} to {moved[:10]} during this run; comparing {moved[:10]}")
            prs[number]["sha"] = moved


def merge(*args):
    root = _repository_root()
    result = subprocess.run(["git", "-c", "core.quotePath=false", *GIT_CONFIG, "merge-tree", "--write-tree", "--name-only", "--no-messages", "-z", *args],
                            cwd=root, capture_output=True)
    lines = result.stdout.decode("utf-8", errors="replace").rstrip("\0").split("\0")
    if result.returncode not in (0, 1) or not lines[0]:
        required = (2, 40) if any(arg.startswith("--merge-base") for arg in args) else (2, 38)
        found = re.match(rb"git version (\d+)\.(\d+)", subprocess.run(["git", *GIT_CONFIG, "version"], capture_output=True).stdout)
        version = (int(found.group(1)), int(found.group(2))) if found else None
        if version and version < required:
            raise RuntimeError(f"git merge-tree {' '.join(args)} needs git {required[0]}.{required[1]} or newer; "
                               f"this git is {version[0]}.{version[1]}")
        error = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git merge-tree {' '.join(args)} failed: {error}")
    return lines[0], set(lines[1:]) if result.returncode else set()


def commit(tree, ref):
    return git("-c", "user.name=pr_overlap", "-c", "user.email=pr_overlap", "commit-tree", tree,
               "-p", BASE_REF, "-p", ref, "-m", "pr_overlap").strip()


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


def unquote_path(path):
    if path.startswith('"') and path.endswith('"'):
        return codecs.escape_decode(path[1:-1].encode("utf-8"))[0].decode("utf-8", errors="replace")
    return path


def changed_hunks(diff, prefix):
    files, hunks, current, path = [], [], [], None
    in_hunk = False
    for line in diff.split("\n"):
        line = line.removesuffix("\r")
        if line.startswith("diff --git "):
            if current:
                hunks.append("\n".join(current))
            if hunks and path:
                files.append((path, hunks))
            path = line.split(" b/", 1)[1] if " b/" in line else None
            hunks, current = [], []
            in_hunk = False
        elif line.startswith("@@"):
            if current:
                hunks.append("\n".join(current))
            current = []
            in_hunk = True
        elif not in_hunk and line.startswith("--- " if prefix == "-" else "+++ "):
            path = unquote_path(line[4:])
            if path == "/dev/null":
                path = None
            elif path.startswith(("a/", "b/")):
                path = path[2:]
        elif line.startswith(prefix) and not line.startswith(prefix * 3):
            current.append(line[1:])
    if current:
        hunks.append("\n".join(current))
    if hunks and path:
        files.append((path, hunks))
    return files


def changed_source(diff, prefix):
    return [(path, "\n".join(hunks)) for path, hunks in changed_hunks(diff, prefix)]


def library_for_path(path):
    parts = Path(path).parts
    return "/".join(parts[:4]) if len(parts) > 3 and parts[:3] == ("core", "libs", "prx") else ""


def production_source(path):
    path = Path(path)
    return path.suffix == ".cpp" and not any(part.lower() in {"test", "tests"} for part in path.parts[2:-1])


def export_diffs(old, new):
    diff = git("diff", "--find-renames", "-l1000", "-U0", "--no-color", old, new, "--", *EXPORT_PATHS)
    source, in_hunk, relocated = None, False, set()
    for line in diff.split("\n"):
        line = line.removesuffix("\r")
        if line.startswith("diff --git "):
            source, in_hunk = None, False
        elif line.startswith("@@"):
            in_hunk = True
        elif not in_hunk and line.startswith("rename from "):
            source = unquote_path(line[12:])
        elif not in_hunk and source is not None and line.startswith("rename to "):
            destination = unquote_path(line[10:])
            if library_for_path(source) != library_for_path(destination):
                relocated.update(path for path in (source, destination) if library_for_path(path) and production_source(path))
    identity_diff = diff
    if relocated:
        identity_diff += "\n" + git("diff", "--no-renames", "-U0", "--no-color", old, new, "--",
                                    *(f":(literal){path}" for path in sorted(relocated)), *EXPORT_PATHS[1:])
    return diff, identity_diff


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
    for path, hunks in changed_hunks(diff, prefix):
        library = library_for_path(path)
        for key, names in source_exports("\n".join(hunks), library).items():
            found.setdefault(key, set()).update(names)
        for hunk in hunks:
            hunk_diff = "\n".join(prefix + line for line in hunk.splitlines())
            for name in exports(hunk_diff, prefix):
                canonical = nid_names.strip_nid_postfix(name)
                found.setdefault((library, nid_names.compute_nid(canonical)), set()).add(canonical)
    return found


def export_cache_version():
    parsers = (Path(__file__), Path(nid_names.__file__))
    return hashlib.sha256(b"".join(path.read_bytes() for path in parsers)).hexdigest()


def load_export_cache(path, version):
    try:
        cache = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    if not isinstance(cache, dict) or cache.get("version") != version or not isinstance(cache.get("files"), dict):
        return {}
    return cache["files"]


def save_export_cache(path, version, files):
    temporary = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_text(json.dumps({"version": version, "files": files}), encoding="utf-8")
        os.replace(temporary, path)
    except OSError:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass


def base_sources(root, ref):
    listing = subprocess.run(["git", *GIT_CONFIG, "-C", root, "ls-tree", "-r", "-z", ref, "--", "core/libs"],
                             capture_output=True, check=True).stdout.decode("utf-8", errors="replace")
    sources = []
    for entry in filter(None, listing.split("\0")):
        info, name = entry.split("\t", 1)
        mode, kind, blob = info.split()
        path = Path(name)
        if kind != "blob" or mode == "120000" or not production_source(path):
            continue
        sources.append((blob, name))
    return sources


def blob_texts(root, blobs):
    if not blobs:
        return {}
    out = subprocess.run(["git", *GIT_CONFIG, "-C", root, "cat-file", "--batch"], input="".join(f"{blob}\n" for blob in blobs).encode(),
                         capture_output=True, check=True).stdout
    texts, offset = {}, 0
    for blob in blobs:
        header_end = out.index(b"\n", offset)
        size = int(out[offset:header_end].split()[2])
        texts[blob] = out[header_end + 1:header_end + 1 + size].decode("utf-8", errors="replace")
        offset = header_end + 2 + size
    return texts


def base_exports(ref):
    root = _repository_root()
    common_dir = git("rev-parse", "--path-format=relative", "--git-common-dir").strip()
    cache_path = root / common_dir / "pr_overlap-exports.json"
    version = export_cache_version()
    cached = load_export_cache(cache_path, version)
    sources = base_sources(root, ref)
    keys = {(blob, name): f"{blob} {library_for_path(name)}" for blob, name in sources}
    texts = blob_texts(root, sorted({blob for (blob, name), key in keys.items() if key not in cached}))
    files = {}
    found = {}
    for (blob, name), key in keys.items():
        if key not in files:
            files[key] = cached[key] if key in cached else [
                [library, nid, sorted(names)] for (library, nid), names in source_exports(texts[blob], library_for_path(name)).items()]
        for library, nid, names in files[key]:
            found.setdefault((library, nid), set()).update(names)
    older_bases_fit = len(cached) + len(files) <= 4 * len(files)
    kept = {**cached, **files} if older_bases_fit else files
    if kept != cached:
        save_export_cache(cache_path, version, kept)
    return found


def merge_base(ref):
    try:
        bases = git("merge-base", "--all", BASE_REF, ref).split()
    except subprocess.CalledProcessError as error:
        if error.returncode != 1:
            raise
        bases = []
    if not bases:
        return None
    if len(bases) == 1:
        return bases[0]
    return min(bases, key=lambda base: git("diff", "--name-only", "--no-renames", "-z", base, ref).count("\0"))


def pull_file_data(pr):
    number = pr.get("_number")
    if number is None:
        raise RuntimeError("pull request number is required when histories have no merge base")
    metadata = json.loads(gh("api", f"repos/{REPO}/pulls/{number}"))
    changed_files = metadata.get("changed_files")
    if type(changed_files) is not int or changed_files < 0:
        raise ValueError(f"pull request {number} returned an invalid changed_files count")
    pages = json_pages(gh("api", "--paginate", f"repos/{REPO}/pulls/{number}/files?per_page=100"))
    files, added, existing, patches = set(), set(), set(), []
    entries = 0
    for page_items in pages:
        for item in page_items:
            entries += 1
            path = item.get("filename")
            status = item.get("status")
            if not isinstance(path, str) or not path or not isinstance(status, str):
                raise ValueError(f"pull request {number} returned an invalid file entry")
            files.add(path)
            old_path = item.get("previous_filename")
            if status == "renamed":
                if not isinstance(old_path, str) or not old_path:
                    raise ValueError(f"pull request {number} returned an invalid renamed file entry")
                files.add(old_path)
                added.add(path)
                existing.add(old_path)
            elif status == "added":
                added.add(path)
            elif status in {"removed", "modified", "changed", "unchanged"}:
                existing.add(path)
            elif status == "copied":
                added.add(path)
            else:
                raise ValueError(f"pull request {number} returned an unsupported file status: {status}")
            patch = item.get("patch")
            if patch is None:
                production = (path, old_path) if status == "renamed" else (path,)
                if any(candidate and candidate.startswith("core/libs/prx/") and candidate.endswith(".cpp") for candidate in production):
                    raise RuntimeError(f"pull request {number} has no patch for {path}")
                continue
            if not isinstance(patch, str):
                raise ValueError(f"pull request {number} returned an invalid patch for {path}")
            before = old_path if status == "renamed" else path
            patches.append(f"diff --git a/{before} b/{path}\n--- a/{before}\n+++ b/{path}\n{patch}")
    if entries != changed_files:
        raise RuntimeError(f"pull request {number} returned {entries} of {changed_files} changed files")
    return files, added, existing, "\n".join(patches)


def scan(pr):
    try:
        tree, conflicted = merge(BASE_REF, pr["ref"])
    except RuntimeError:
        if merge_base(pr["ref"]) is not None:
            raise
        pr["files"], pr["added"], _, diff = pull_file_data(pr)
        pr["tree"] = None
        pr["exports"] = exports(diff)
        pr["added_exports"] = source_changed_exports(diff, "+")
        pr["removed_exports"] = source_changed_exports(diff, "-")
        return
    pr["tree"] = None if conflicted else commit(tree, pr["ref"])
    old, new = (merge_base(pr["ref"]), pr["ref"]) if conflicted else (BASE_REF, pr["tree"])
    pr["files"], pr["added"] = set(), set()
    changes = git("diff", "--name-status", "--no-renames", "-z", old, new).split("\0")
    for status, path in zip(changes[::2], changes[1::2]):
        pr["files"].add(path)
        if status == "A":
            pr["added"].add(path)
    diff, identity_diff = export_diffs(old, new)
    pr["exports"] = exports(diff)
    pr["added_exports"] = source_changed_exports(identity_diff, "+")
    pr["removed_exports"] = source_changed_exports(identity_diff, "-")


def probe(pr):
    base = merge_base(pr["ref"])
    if base is None:
        files, _, existing, diff = pull_file_data(pr)
        if not any(path.startswith("core/libs/prx/") and path.endswith(".cpp") for path in files):
            return files, (set(), {}), existing
        return files, (exports(diff), source_changed_exports(diff, "+")), existing
    changes = git("diff", "--name-status", "--no-renames", "-z", base, pr["ref"]).split("\0")
    files, existing = set(), set()
    for status, path in zip(changes[::2], changes[1::2]):
        files.add(path)
        if status != "A":
            existing.add(path)
    if not any(path.startswith("core/libs/prx/") and path.endswith(".cpp") for path in files):
        return files, (set(), {}), existing
    diff, identity_diff = export_diffs(base, pr["ref"])
    return files, (exports(diff), source_changed_exports(identity_diff, "+")), existing


def related(prs, executor):
    mine = prs[0]
    scan(mine)
    others = {n: pr for n, pr in prs.items() if n}
    base_paths = set(git("ls-tree", "-r", "--name-only", "-z", BASE_REF).split("\0"))
    chosen = {}
    mine_files = mine["files"]
    mine_exports = mine["exports"]
    mine_keys = set(mine.get("added_exports", {}))

    def inspect(item):
        number, pr = item
        pr["_number"] = number
        try:
            return probe(pr)
        finally:
            pr.pop("_number", None)

    for index, ((n, pr), (files, found, existing)) in enumerate(zip(others.items(), executor.map(inspect, others.items())), 1):
        found_exports, found_added = found
        if files & mine_files or (found_exports & mine_exports) or (set(found_added) & mine_keys) or existing - base_paths:
            chosen[n] = pr
        else:
            pr.update(tree=None, files=set(), added=set(), exports=set(), added_exports={}, removed_exports={})
        if index % PROGRESS_EVERY == 0 or index == len(others):
            progress(f"probed {index}/{len(others)} pull requests, {len(chosen)} share files or exports with the branch")
    return chosen


def scan_all(prs, jobs):
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        pending = related(prs, executor) if 0 in prs else prs

        def inspect(item):
            number, pr = item
            pr["_number"] = number
            try:
                scan(pr)
            finally:
                pr.pop("_number", None)

        for index, _ in enumerate(executor.map(inspect, pending.items()), 1):
            if index % PROGRESS_EVERY == 0 or index == len(pending):
                progress(f"merged {index}/{len(pending)} pull requests into the base")


def hunks(pr, path):
    diff = git("diff", "-U0", "--no-color", BASE_REF, pr["tree"], "--", path)
    found = []
    for start, count in HUNK.findall(diff):
        start = max(int(start), 1)
        found.append((start, start + max(int(count or 1), 1) - 1))
    return found


def clashes(a, b, path):
    left, right = hunks(a, path), hunks(b, path)
    ranges = sorted((min(s1, s2), max(e1, e2)) for s1, e1 in left for s2, e2 in right
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
        same_exports = (pa.get("exports", set()) & pb.get("exports", set())).difference(*(pr.get("exports", set()) for pr in shared))
        shared_keys = set().union(*(pr.get("added_exports", {}) for pr in shared))
        for key in (set(pa.get("added_exports", {})) & set(pb.get("added_exports", {}))) - shared_keys:
            same_exports.update(pa["added_exports"][key])
            same_exports.update(pb["added_exports"][key])
        found["exports"] = same_exports
        if same_issues := pa.get("fixes", set()) & pb.get("fixes", set()):
            found["issues"] = same_issues
        if not (pa["tree"] and pb["tree"]):
            uncompared = pa.get("files", set()) & pb.get("files", set())
            if uncompared:
                found["uncompared"] = uncompared
    return found if any(found.values()) else None



def compare_all(prs, pairs, jobs):
    rows = {n: {} for n in prs}
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        try:
            merging = {}
            for a, b in pairs:
                if prs[a].get("author") != prs[b].get("author") and prs[a].get("files", set()) & prs[b].get("files", set()):
                    merging[a, b] = executor.submit(overlaps, prs, a, b)
                elif found := overlaps(prs, a, b):
                    rows[a][b] = rows[b][a] = found
            for index, ((a, b), future) in enumerate(merging.items(), 1):
                if found := future.result():
                    rows[a][b] = rows[b][a] = found
                if index % PAIR_PROGRESS_EVERY == 0 or index == len(merging):
                    progress(f"compared {index}/{len(merging)} pull request pairs that change the same files")
        except BaseException:
            executor.shutdown(cancel_futures=True)
            raise
    return rows


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


def issue(blob, number):
    return f'[issue {number}]({blob.split("/blob/", 1)[0].replace("https://github.com/", "https://www.github.com/", 1)}/issues/{number})'


def report(rows, blob, main_exports=None):
    if not rows:
        if not main_exports:
            return ""
    confirmed = {number: found for number, found in rows.items()
                 if found.get("conflicts") or found.get("exports") or found["added"]}
    uncompared = {number: found["uncompared"] for number, found in rows.items() if found.get("uncompared")}
    issues = {number: found["issues"] for number, found in rows.items() if found.get("issues")}
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
    if issues:
        if confirmed:
            lines.append("")
        lines.append("Other open pull requests say they fix the same issue, so they may implement the same thing:")
        lines.append("")
        for number, numbers in sorted(issues.items()):
            lines.append(f"- {pull(blob, number)}: " + ", ".join(issue(blob, n) for n in sorted(numbers)))
    if main_exports:
        if confirmed or issues:
            lines.append("")
        lines.append("Exports with a name or library/NID already present in `main`:")
        lines.append("")
        for name, finding in sorted(main_exports.items()):
            same_key = finding.get("same_key", set())
            other_libraries = finding.get("other_libraries", set())
            if same_key:
                details = ", ".join(f"{library} ({nid})" for library, nid in sorted(same_key))
                lines.append(f"- {code_span(name)} (same library/NID already present: {details})")
            if other_libraries:
                lines.append(f"- {code_span(name)} (also exported by another library in `main`: {cell(other_libraries)}; may be intentional)")
    if uncompared:
        if confirmed or issues or main_exports:
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
    body = "\n".join(lines) + "\n"
    return body if len(body) <= MAX_REPORT_CHARS else truncated(lines)


def truncated(lines):
    cumulative = []
    total = 0
    for index, line in enumerate(lines):
        if index:
            total += 1
        total += len(line)
        cumulative.append(total)
    keep = 1
    for candidate in range(len(lines) - 1, 0, -1):
        notice = f"_Report truncated: {len(lines) - candidate} more lines omitted._"
        if cumulative[candidate - 1] + 1 + len(notice) + 1 <= MAX_REPORT_CHARS:
            keep = candidate
            break
    notice = f"_Report truncated: {len(lines) - keep} more lines omitted._"
    return "\n".join(lines[:keep] + [notice]) + "\n"


def write_reports(out, rows, blob, main_exports=None):
    main_exports = main_exports or {}
    try:
        out.mkdir(parents=True, exist_ok=True)
        for number, found in rows.items():
            (out / f"{number}.md").write_text(report(found, blob, main_exports.get(number)), encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"pr_overlap: {error}") from None


def unverified_overlaps(rows):
    return {number: set(found["uncompared"]) for number, found in rows.items() if found.get("uncompared")}


def main_export_findings(prs, current_exports):
    by_name = {}
    for (library, _nid), names in current_exports.items():
        for name in names:
            canonical = NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))
            by_name.setdefault(canonical, set()).add(library)

    findings = {}
    for number, pr in prs.items():
        added = pr.get("added_exports", {})
        removed = pr.get("removed_exports", {})
        moved = {}
        for (library, nid), removed_from in removed.items():
            for name in removed_from:
                canonical = NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))
                moved.setdefault(canonical, set()).add(library)
        for (library, _nid), added_to in added.items():
            for name in added_to:
                canonical = NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))
                moved.get(canonical, set()).discard(library)
        names = {}
        for library, nid in added:
            added_names = {
                NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))
                for name in added[(library, nid)]
            }
            removed_names = {
                NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))
                for name in removed.get((library, nid), ())
            }
            for canonical in added_names:
                finding = names.setdefault(canonical, {"same_key": set(), "other_libraries": set()})
                if (library, nid) in current_exports and (library, nid) not in removed:
                    finding["same_key"].add((library, nid))
                if canonical not in removed_names:
                    finding["other_libraries"].update(by_name.get(canonical, set()) - {library} - moved.get(canonical, set()))
                if not finding["same_key"] and not finding["other_libraries"]:
                    names.pop(canonical)
        if names:
            findings[number] = names
    return findings


def canonical(name):
    return NID_DISAMBIG.sub("", nid_names.strip_nid_postfix(name))


def body_exports(pr, files, names):
    paths = sorted(path for path in files if path.endswith(".cpp") and library_for_path(path) in names
                   and not any(part.lower() in {"test", "tests"} for part in Path(path).parts))
    if not paths:
        return set()
    base = merge_base(pr["ref"])
    if base is None:
        raise RuntimeError(f"cannot inspect function bodies for pull request {pr.get('_number', '?')} without a merge base")
    found = set()
    for path in paths:
        ranges = [(int(start), int(count or 1)) for start, count in HUNK.findall(
            git("diff", "-U0", "--no-color", "--no-renames", base, pr["ref"], "--", path))]
        if not ranges:
            continue
        source = git("show", f"{base}:{path}")
        source = nid_names.CPP_TOKEN.sub(
            lambda match: match.group() if match.group("code") is not None else re.sub(r"[^\n]", " ", match.group()), source)
        for definition in nid_names.DEFINITION.finditer(source):
            requested = names[library_for_path(path)].get(canonical(definition.group(1)))
            if not requested:
                continue
            depth = 0
            end = None
            for brace in re.finditer(r"[{}]", source[definition.end() - 1:]):
                depth += 1 if brace.group() == "{" else -1
                if depth == 0:
                    end = definition.end() - 1 + brace.end()
                    if not re.match(r"\s*catch\b", source[end:]):
                        break
            if end is None:
                raise ValueError(f"cannot locate the end of {definition.group(1)} in {path}")
            first = source.count("\n", 0, definition.start()) + 1
            last = source.count("\n", 0, end) + 1
            if any(first <= start < last if count == 0 else start <= last and start + count - 1 >= first
                   for start, count in ranges):
                found.update(requested)
    return found


def pull_requests_with(names, prs, jobs):
    wanted = {canonical(name) for name in names}
    nids = {nid_names.compute_nid(name): name for name in wanted}
    found = {name: [] for name in wanted}
    bodies = {}
    for (library, nid), defined in base_exports(BASE_REF).items():
        requested = wanted & {canonical(name) for name in defined}
        if nid in nids:
            requested.add(nids[nid])
        if requested:
            for name in defined:
                bodies.setdefault(library, {}).setdefault(canonical(name), set()).update(requested)

    def inspect(item):
        number, pr = item
        pr["_number"] = number
        try:
            files, (exported, added), existing = probe(pr)
            return files, (exported | body_exports(pr, existing, bodies), added), existing
        finally:
            pr.pop("_number", None)

    with ThreadPoolExecutor(max_workers=jobs) as executor:
        for index, (number, (_files, (exported, added), _existing)) in enumerate(zip(prs, executor.map(inspect, prs.items())), 1):
            touched = {canonical(name) for name in exported}
            for (_library, nid), defined in added.items():
                touched.update(canonical(name) for name in defined)
                if nid in nids:
                    touched.add(nids[nid])
            for name in wanted & touched:
                found[name].append(number)
            if index % PROGRESS_EVERY == 0 or index == len(prs):
                progress(f"searched {index}/{len(prs)} pull requests")
    return found


def library_status(ref, library):
    archive = subprocess.run(["git", *GIT_CONFIG, "archive", "--format=tar", ref, "--", library], capture_output=True, check=True).stdout
    scanner = importlib.import_module("progress")
    with tempfile.TemporaryDirectory() as root:
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(root, filter="data")
        group = scanner.scan_library(Path(root, library))
    status = {canonical(name): "stub" for name in group["todo_names"]}
    status.update({canonical(name): "implemented" for name in group["done_names"]})
    return status


def main_status(names, ref):
    wanted = {canonical(name) for name in names}
    nids = {nid_names.compute_nid(name): name for name in wanted}
    declared = {name: {} for name in wanted}
    for (library, nid), defined in base_exports(ref).items():
        aliases = {canonical(item) for item in defined}
        for name in wanted & (aliases | ({nids[nid]} if nid in nids else set())):
            declared[name].setdefault(library, set()).update(aliases | {name})
    libraries = {library for exports in declared.values() for library in exports if library}
    scanned = {library: library_status(ref, library) for library in sorted(libraries)}
    status = {}
    for name, libraries in declared.items():
        status[name] = {}
        for library, aliases in libraries.items():
            states = {scanned.get(library, {}).get(alias) for alias in aliases} - {None}
            status[name][library] = "implemented" if "implemented" in states else "stub" if states else "declared"
    return status


def lookup_report(names, found, status, prs, blob):
    lines = [
        "PS5 exports only. Internal functions and instruction opcodes are not checked.",
        "Inspect candidate pull requests and run --branch <branch> to check file and line overlaps.",
        "",
    ]
    for requested in names:
        name = canonical(requested)
        lines.append(name)
        libraries = status.get(name, {})
        if libraries:
            lines.extend(f"  main: {library} ({state})" for library, state in sorted(libraries.items()))
        else:
            lines.append("  main: no matching PS5 export")
        numbers = sorted(found.get(name, ()))
        if numbers:
            lines.extend(f"  open: {pull(blob, number)} by {prs[number]['author']}: {prs[number].get('title', '')}" for number in numbers)
        else:
            lines.append("  open: no matching PS5 export changes")
    return "\n".join(lines) + "\n"


def describe_failure(error):
    command = " ".join(str(part) for part in error.cmd)
    message = (error.stderr or "").strip() or "no error output"
    return f"pr_overlap: {command} failed with exit code {error.returncode}: {message}"


def resolve_branch(name):
    try:
        return git("rev-parse", "--verify", f"{name}^0").strip()
    except subprocess.CalledProcessError:
        raise ValueError(f"no single branch or commit is named {name!r}; check `git branch --show-current`") from None


def main(argv=None):
    global BASE_REF
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(
        description="report open pull requests that overlap each other",
        epilog="The exports found in the base are cached by source blob in pr_overlap-exports.json inside the git "
               "directory; deleting that file only makes the next run slower.")
    parser.add_argument("--base", default="main")
    parser.add_argument("--out", type=Path, help="write <number>.md for every open pull request (empty when clean)")
    parser.add_argument("--branch", help="check a local branch against the open pull requests instead")
    parser.add_argument("--names", nargs="+", metavar="NAME",
                        help="before writing code, show whether each function is implemented on the base and which open pull requests add it or edit its body")
    parser.add_argument("--repo", help="owner/name; defaults to the upstream of the current checkout")
    parser.add_argument("--jobs", type=int, default=8, help="number of pull request comparisons to run at once")
    parser.add_argument("--head-repo", help="head repository owner/name for --branch with the same branch name; must be the correct fork because its matching pull request is excluded")
    args = parser.parse_args(argv)
    if sum(map(bool, (args.out, args.branch, args.names))) != 1:
        parser.error("pass exactly one of --out, --branch or --names")
    if args.head_repo is not None and (not args.branch or not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?/[A-Za-z0-9_.-]+", args.head_repo) or args.head_repo.split("/", 1)[1] in (".", "..")):
        parser.error("--head-repo needs --branch and a GitHub owner/name")
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    global REPO
    try:
        noisy = [name for name in args.names or () if not name or NAME_NOISE.search(name)]
        if noisy:
            raise ValueError("--names takes one function name per argument, without whitespace or control characters: "
                             + ", ".join(map(ascii, noisy)))
        REPO = args.repo or upstream()
        prs = open_prs(args.base)
        BASE_REF = base_commit(args.base)
        branch_sha = resolve_branch(args.branch) if args.branch else None
        fetch_heads(BASE_REF, prs, check_existing=bool(args.names))
        if args.names:
            found = pull_requests_with(args.names, prs, args.jobs)
            status = main_status(args.names, BASE_REF)
            blob = f'https://github.com/{REPO}/blob/{BASE_REF}'
            print(lookup_report(args.names, found, status, prs, blob), end="")
            return 0
        if args.branch:
            sha = branch_sha
            head_branch, head_repo = (args.branch, args.head_repo) if args.head_repo else push_head(args.branch)
            prs = without_own_pull_request(own(prs, sha), head_branch, head_repo)
            prs[0] = {"ref": sha, "author": None, "depends": set()}
        scan_all(prs, args.jobs)
        current_exports = base_exports(BASE_REF)
        main_exports = main_export_findings(prs, current_exports)
        pairs = [(0, n) for n in prs if n] if args.branch else combinations(prs, 2)
        rows = compare_all(prs, pairs, args.jobs)
        blob = f'https://github.com/{REPO}/blob/{BASE_REF}'
    except subprocess.CalledProcessError as error:
        raise SystemExit(describe_failure(error))
    except ValueError as error:
        raise SystemExit(f"pr_overlap: {error}")
    if args.branch:
        print(report(rows[0], blob, main_exports.get(0)) or "No overlap with open pull requests.", end="")
        unresolved = unverified_overlaps(rows[0])
        if unresolved:
            for number, files in sorted(unresolved.items()):
                progress(f"cannot confirm overlap with PR {number}: {', '.join(sorted(files))}")
            progress("overlap check failed closed; manually compare every reported PR branch before opening this one")
            return 2
        return 0
    write_reports(args.out, rows, blob, main_exports)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
