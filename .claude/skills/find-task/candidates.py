import argparse
import functools
import importlib.util
import io
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

OPCODES = "core/shader/recompiler/RdnaDecoder/include/RdnaDecoder/RdnaOpcode.hpp"
ISA = "tools/rdna_isa.txt"
PRX = "core/libs/prx/"
SHADER_INPUTS = {OPCODES, ISA}
REMOTE = re.compile(r"github\.com[:/]([^/]+/[^/]+?)(?:\.git)?/?$")
CACHE_TTL = 1800


def run(*args, check=True):
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and result.returncode:
        raise SystemExit(f"{' '.join(args[:4])}... failed:\n{result.stderr.strip()}")
    return result.stdout if result.returncode == 0 else None


def git(*args, check=True):
    return run("git", *args, check=check)


@functools.cache
def token():
    found = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not found and (gh := shutil.which("gh")):
        found = (run(gh, "auth", "token", check=False) or "").strip()
    return found


def request(path):
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "anyps5-find-task"}
    if auth := token():
        headers["Authorization"] = f"Bearer {auth}"
    try:
        with urllib.request.urlopen(urllib.request.Request(f"https://api.github.com/{path}", headers=headers)) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        raise SystemExit(f"GitHub answered {error.code} for {path}; if this is the rate limit, set GH_TOKEN or run gh auth login")


def pages(repo, path):
    items, page = [], 1
    while True:
        batch = request(f"repos/{repo}/{path}&per_page=100&page={page}")
        items += batch
        if len(batch) < 100:
            return items
        page += 1


def upstream():
    for remote in ("upstream", "origin"):
        if match := REMOTE.search((git("remote", "get-url", remote, check=False) or "").strip()):
            info = request(f"repos/{match.group(1)}")
            return info.get("parent", info)["full_name"]
    raise SystemExit("no GitHub remote named upstream or origin; pass --repo owner/name")


def extract(ref, paths, dest):
    data = subprocess.run(["git", "archive", ref, "--", *paths], capture_output=True, check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(dest, filter="data")


def load_progress(root):
    spec = importlib.util.spec_from_file_location("progress", root / "tools" / "progress.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def definitions(progress, text):
    calls = progress.stub_calls(text)
    for match in progress.DEFINITION.finditer(text):
        name = match.group(1)
        if name.endswith("_nid_no_patch"):
            continue
        body = text[match.end() - 1:progress.body_end(text, match.end() - 1)]
        yield name, any(call in body for call in calls), text.count("\n", 0, match.start()) + 1


def scan_base(progress, root):
    libs = {}
    for lib in sorted(p for p in (root / PRX).iterdir() if p.is_dir()):
        done, todo = set(), {}
        for source in sorted(lib.rglob("*.cpp")):
            where = source.relative_to(root).as_posix()
            for name, stub, line in definitions(progress, source.read_text(errors="ignore")):
                if stub:
                    todo.setdefault(name, f"{where}:{line}")
                else:
                    done.add(name)
        libs[lib.name] = {"done": sorted(done), "todo": {n: w for n, w in todo.items() if n not in done}}
    return libs


def shader_state(progress, root):
    progress.OPCODES, progress.ISA = root / OPCODES, root / ISA
    return {g["name"]: {"done": g["done_names"], "todo": g["todo_names"]} for g in progress.collect_shaders()["groups"]}


def area(path):
    parts = path.split("/")
    if path.startswith(PRX):
        return parts[3]
    return "/".join(parts[:3]) if parts[0] == "core" and len(parts) > 3 else "/".join(parts[:-1]) or parts[0]


def scan_pr(progress, number, pr, tmp, base_done, base_all, base_shaders_done):
    ref = f"refs/pr/{number}"
    merge_base = (git("merge-base", "refs/pr/base", ref, check=False) or "").strip()
    if not merge_base:
        pr["error"] = "no merge base with main"
        return
    files = (git("diff", "--name-only", "--no-renames", merge_base, ref) or "").splitlines()
    done, declared = set(), set()
    for path in files:
        if path.startswith(PRX) and path.endswith(".cpp") and (text := git("show", f"{ref}:{path}", check=False)):
            for name, stub, _ in definitions(progress, text):
                (declared if stub else done).add(name)
    shaders = set()
    if SHADER_INPUTS & set(files):
        try:
            root = tmp / f"pr{number}"
            extract(ref, sorted(SHADER_INPUTS), root)
            state = shader_state(progress, root)
            shaders = {n for g in state.values() for n in g["done"]} - base_shaders_done
        except Exception as error:
            pr["error"] = f"shader scan failed: {error}"
    pr.update(merge_base=merge_base, files=files, areas=sorted({area(f) for f in files}),
              implements=sorted(done - base_done), declares=sorted(declared - done - base_all), shaders=sorted(shaders))


def tool_version(name, *args):
    if not (found := shutil.which(name)):
        return "missing"
    return ((run(found, *args, check=False) or "").splitlines() or ["?"])[0].strip()


def build(repo, base):
    pulls = pages(repo, f"pulls?state=open&base={base}")
    prs = {str(p["number"]): {"title": p["title"], "author": p["user"]["login"], "draft": p["draft"]} for p in pulls}
    refspecs = [f"+refs/heads/{base}:refs/pr/base"] + [f"+refs/pull/{n}/head:refs/pr/{n}" for n in prs]
    git("fetch", "--no-tags", "-q", f"https://github.com/{repo}.git", *refspecs)
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        extract("refs/pr/base", ["tools", OPCODES, PRX.rstrip("/")], tmp / "base")
        progress = load_progress(tmp / "base")
        libs = scan_base(progress, tmp / "base")
        shaders = shader_state(progress, tmp / "base")
        base_done = {n for lib in libs.values() for n in lib["done"]}
        base_all = base_done | {n for lib in libs.values() for n in lib["todo"]}
        base_shaders_done = {n for g in shaders.values() for n in g["done"]}
        for number, pr in prs.items():
            scan_pr(progress, number, pr, tmp, base_done, base_all, base_shaders_done)
    return {"time": time.time(), "repo": repo, "base": base, "base_sha": git("rev-parse", "refs/pr/base").strip(),
            "prs": prs, "libs": libs, "shaders": shaders}


def load(repo, base, refresh):
    cache = Path(git("rev-parse", "--path-format=absolute", "--git-common-dir").strip()) / "find-task-cache.json"
    if not refresh and cache.exists():
        data = json.loads(cache.read_text(encoding="utf-8"))
        if repo in (None, data["repo"]) and data["base"] == base and time.time() - data["time"] < CACHE_TTL:
            return data
    data = build(repo or upstream(), base)
    cache.write_text(json.dumps(data), encoding="utf-8")
    return data


def claims(data, key):
    found = {}
    for number, pr in data["prs"].items():
        for name in pr.get(key, ()):
            found.setdefault(name, []).append(int(number))
    return found


def refs(numbers, limit=None):
    numbers = sorted(numbers, reverse=True)
    shown = " ".join(f"#{n}" for n in numbers[:limit])
    return f"{len(numbers)}, latest {shown}" if limit and len(numbers) > limit else shown


def touching(data, name):
    return sorted(int(n) for n, pr in data["prs"].items() if name in pr.get("areas", ()))


def overview(data):
    behind = (git("rev-list", "--count", "HEAD..refs/pr/base", check=False) or "?").strip()
    print(f'upstream {data["repo"]} {data["base"]} @ {data["base_sha"][:9]}; local HEAD is {behind} commits behind it')
    print(f'open pull requests: {len(data["prs"])}; cache age {int(time.time() - data["time"])}s')
    print(f"host: {platform.system()}; toolchain: " + "; ".join(f"{n}: {tool_version(n, a)}" for n, a in
                                    (("cmake", "--version"), ("ninja", "--version"), ("g++", "-dumpfullversion"))))
    for number, pr in data["prs"].items():
        if "error" in pr:
            print(f'warning: #{number} not fully scanned ({pr["error"]})')
    claimed = claims(data, "implements")
    print("\nSYSTEM LIBRARIES with stubs on upstream main (free = stub not implemented by any open PR)")
    print(f'{"library":44}{"done":>6}{"stubs":>6}{"free":>6}  open PRs touching the library')
    rows = [(len([n for n in lib["todo"] if n not in claimed]), name, lib) for name, lib in data["libs"].items() if lib["todo"]]
    for free, name, lib in sorted(rows, key=lambda r: (-r[0], r[1])):
        print(f'{name:44}{len(lib["done"]):>6}{len(lib["todo"]):>6}{free:>6}  {refs(touching(data, name), 8)}')
    claimed = claims(data, "shaders")
    print("\nSHADER INSTRUCTIONS by encoding (free = not decoded on main, not added by any open PR)")
    print(f'{"encoding":12}{"done":>6}{"todo":>6}{"free":>6}')
    for name, group in sorted(data["shaders"].items()):
        if group["todo"]:
            free = len([n for n in group["todo"] if n not in claimed])
            print(f'{name:12}{len(group["done"]):>6}{len(group["todo"]):>6}{free:>6}')
    print("\nnext: --lib NAME | --shaders [ENCODING] | --prs [FILTER] | --check NAME... | --touches PATH... | --issues")


def show_lib(data, name):
    lib = data["libs"].get(name) or sys.exit(f"unknown library {name}")
    claimed = claims(data, "implements")
    free = {n: w for n, w in lib["todo"].items() if n not in claimed}
    print(f"{name}: {len(lib['done'])} implemented, {len(lib['todo'])} stubs, {len(free)} free (paths are on refs/pr/base)")
    for stub, where in sorted(free.items(), key=lambda item: (item[1].rsplit(":", 1)[0], int(item[1].rsplit(":", 1)[1]))):
        print(f"  {stub}  {where}")
    taken = {n: claimed[n] for n in lib["todo"] if n in claimed}
    if taken:
        print("claimed:")
        for stub, numbers in sorted(taken.items()):
            print(f"  {stub}  {refs(numbers)}")
    show_prs(data, name, "open PRs touching this library:")


def show_shaders(data, encoding):
    claimed = claims(data, "shaders")
    for name, group in sorted(data["shaders"].items()):
        if encoding not in (None, name) or not group["todo"]:
            continue
        print(f'{name} free: {" ".join(n for n in group["todo"] if n not in claimed) or "-"}')
        taken = [f"{n}({refs(claimed[n])})" for n in group["todo"] if n in claimed]
        if taken:
            print(f'{name} claimed: {" ".join(taken)}')


def show_prs(data, needle=None, heading=None):
    rows = []
    for number, pr in sorted(data["prs"].items(), key=lambda item: -int(item[0])):
        areas = pr.get("areas", [])
        text = f'#{number} {pr["author"]}{" [draft]" if pr["draft"] else ""}: {pr["title"]}  [{", ".join(areas[:6])}{", ..." if len(areas) > 6 else ""}]'
        if needle is None or needle in areas or needle.lower() in text.lower():
            rows.append(text)
    if not rows:
        print(heading.rstrip(":") + ": none" if heading else "no open pull request matches")
        return
    if heading:
        print(heading)
    print("\n".join(("  " if heading else "") + row for row in rows))


def touches(data, paths):
    for path in paths:
        hits = []
        for number, pr in sorted(data["prs"].items(), key=lambda item: -int(item[0])):
            files = [f for f in pr.get("files", ()) if f == path or f.startswith(path.rstrip("/") + "/")]
            if files:
                hits.append(f'  #{number} {pr["author"]}: {pr["title"]}  ({len(files)} file{"s" * (len(files) > 1)})')
        print(f"{path}: " + (f"touched by {len(hits)} open PRs:" if hits else "no open PR touches it"))
        if hits:
            print("\n".join(hits))


def issues(data):
    found = [i for i in pages(data["repo"], "issues?state=open") if "pull_request" not in i]
    for issue in found:
        labels = ", ".join(label["name"] for label in issue["labels"])
        print(f'#{issue["number"]} {issue["user"]["login"]}: {issue["title"]}' + (f"  [{labels}]" if labels else ""))
    print(f"{len(found)} open issues")


def camel(name):
    return "".join(part.capitalize() for part in name.split("_"))


def check(data, names):
    for name in names:
        state = next((f"stub at {lib['todo'][name]}" for lib in data["libs"].values() if name in lib["todo"]), None)
        state = state or next((f"already implemented in {key}" for key, lib in data["libs"].items() if name in lib["done"]), None)
        state = state or next((f"shader instruction, {s} on main" for g in data["shaders"].values()
                               for s in ("done", "todo") if name in g[s]), "not an exported function or shader instruction known on main")
        pattern = "|".join(sorted({re.escape(name), re.escape(camel(name)), re.escape(name.lower())}))
        hits = []
        for number, pr in sorted(data["prs"].items(), key=lambda item: int(item[0])):
            if "merge_base" not in pr:
                continue
            files = (git("diff", "--name-only", "-G", pattern, pr["merge_base"], f"refs/pr/{number}", check=False) or "").split()
            if files or name.lower() in pr["title"].lower():
                hits.append(f'  #{number} {pr["author"]}: {pr["title"]}  ({", ".join(files[:4]) or "title only"})')
        print(f"{name}: {state}; " + ("mentioned by open PRs:" if hits else "no open PR mentions it"))
        print("\n".join(hits), end="\n" if hits else "")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="list what is unimplemented on upstream main and not claimed by an open pull request")
    parser.add_argument("--repo", help="owner/name; defaults to the upstream of the current checkout")
    parser.add_argument("--base", default="main")
    parser.add_argument("--refresh", action="store_true", help="ignore the cache")
    parser.add_argument("--lib", help="list the free and claimed stubs of one library")
    parser.add_argument("--shaders", nargs="?", const="", help="list free and claimed shader instructions, optionally of one encoding")
    parser.add_argument("--prs", nargs="?", const="", help="list open pull requests, optionally filtered by area or text")
    parser.add_argument("--check", nargs="+", metavar="NAME", help="search every open pull request diff for these names")
    parser.add_argument("--touches", nargs="+", metavar="PATH", help="list open pull requests changing these files or directories")
    parser.add_argument("--issues", action="store_true", help="list open upstream issues")
    args = parser.parse_args()
    os.chdir(git("rev-parse", "--show-toplevel").strip())
    data = load(args.repo, args.base, args.refresh)
    if args.lib:
        show_lib(data, args.lib)
    elif args.shaders is not None:
        show_shaders(data, args.shaders or None)
    elif args.prs is not None:
        show_prs(data, args.prs or None)
    elif args.check:
        check(data, args.check)
    elif args.touches:
        touches(data, args.touches)
    elif args.issues:
        issues(data)
    else:
        overview(data)
