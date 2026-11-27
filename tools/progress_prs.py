import argparse
import importlib.util
import json
import os
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

import pr_overlap

ROOT = Path(__file__).resolve().parent.parent
REMOTE = "https://github.com"
REFS = "refs/open-pr-progress"
OBJECT_NAME = re.compile(r"[0-9a-f]{40}")


def git(root, *args, check=True):
    result = subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, encoding="utf-8")
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed with {result.returncode}")
    return result


def timestamp():
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def collect_open_prs(repo, root):
    pr_overlap.REPO = repo
    remote = f"{REMOTE}/{repo}.git"
    pull_requests = pr_overlap.open_prs("main")
    git(root, "fetch", "--no-tags", "-q", remote, f"+refs/heads/main:{REFS}/base")
    numbers = sorted(pull_requests)
    for number in numbers:
        pull_requests[number]["ref"] = f"{REFS}/{number}"
    for start in range(0, len(numbers), pr_overlap.FETCH_REFS):
        batch = numbers[start:start + pr_overlap.FETCH_REFS]
        refspecs = [head_refspec(pull_requests[number]) for number in batch]
        if git(root, "fetch", "--no-tags", "-q", remote, *refspecs, check=False).returncode:
            for number in batch:
                fetch_head(root, remote, number, pull_requests[number])
    return pull_requests, git(root, "rev-parse", f"{REFS}/base").stdout.strip()


def head_refspec(pr):
    return f"+{pr['sha']}:{pr['ref']}"


def fetch_head(root, remote, number, pr):
    result = git(root, "fetch", "--no-tags", "-q", remote, head_refspec(pr), check=False)
    if result.returncode and "not our ref" not in result.stderr:
        raise RuntimeError(result.stderr.strip() or f"could not fetch the head of pull request #{number}")
    if result.returncode:
        pr["unavailable"] = True


def closed_dependency_status(repo, number):
    out = subprocess.run(["gh", "api", f"repos/{repo}/pulls/{number}"], capture_output=True, text=True,
                         encoding="utf-8")
    if out.returncode:
        raise RuntimeError(f"could not inspect dependency pull request #{number}: {out.stderr.strip()}")
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError:
        raise RuntimeError(f"GitHub returned invalid data for dependency pull request #{number}")
    if not data.get("merged_at"):
        return None
    if not data.get("merge_commit_sha"):
        raise RuntimeError(f"GitHub omitted the merge commit for dependency pull request #{number}")
    return data["merge_commit_sha"]


def is_ancestor(root, ancestor, descendant):
    result = git(root, "merge-base", "--is-ancestor", ancestor, descendant, check=False)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or f"git merge-base --is-ancestor failed with {result.returncode}")
    return result.returncode == 0


def resolvable(root, sha):
    if not OBJECT_NAME.fullmatch(sha):
        return True
    return git(root, "cat-file", "-e", f"{sha}^{{commit}}", check=False).returncode == 0


def dependency_order(pull_requests, root, base_sha, repo):
    dependencies = {number: set(pr["depends"]) for number, pr in pull_requests.items()}
    excluded = {}
    for number, required in dependencies.items():
        pr = pull_requests[number]
        if pr.get("unavailable"):
            excluded[number] = "head_unavailable"
            continue
        if not resolvable(root, pr["sha"]):
            excluded[number] = "head_unavailable"
            continue
        if is_ancestor(root, pr["sha"], base_sha):
            required.clear()
            continue
        missing = required - pull_requests.keys()
        for dependency in sorted(missing):
            merge_sha = closed_dependency_status(repo, dependency)
            if not merge_sha or not is_ancestor(root, merge_sha, base_sha):
                excluded[number] = "unresolved_dependency"
                break
            dependencies[number].discard(dependency)
    pending = set(pull_requests) - excluded.keys()
    order = []
    while pending:
        ready = sorted(number for number in pending if dependencies[number] <= (set(order) | excluded.keys()))
        if not ready:
            cycles = set()
            for start in pending:
                reachable = list(dependencies[start] & pending)
                visited = set()
                while reachable:
                    number = reachable.pop()
                    if number == start:
                        cycles.add(start)
                        break
                    if number in visited:
                        continue
                    visited.add(number)
                    reachable.extend(dependencies[number] & pending)
            for number in cycles:
                excluded[number] = "dependency_cycle"
            pending -= cycles
            continue
        number = ready[0]
        pending.remove(number)
        order.append(number)
    return order, excluded


def make_commit(root, tree, current, head):
    result = git(root, "-c", "user.name=open-pr-progress", "-c", "user.email=open-pr-progress@invalid", "commit-tree",
                 tree, "-p", current, "-p", head, "-m", "open PR progress estimate")
    return result.stdout.strip()


MISSING_REF = "not something we can merge"


def merge_tree(root, current, ref):
    return git(root, "-c", "core.quotePath=false", "merge-tree", "--write-tree", "--name-only",
               "--no-messages", "-z", current, ref, check=False)


def missing_ref(result):
    return result.returncode and MISSING_REF in result.stderr


def integrate_pull_requests(root, pull_requests, base_sha, repo):
    order, excluded = dependency_order(pull_requests, root, base_sha, repo)
    current = base_sha
    included = set()
    for number in order:
        pr = pull_requests[number]
        if dependencies_blocked(pr, excluded) and not is_ancestor(root, pr["sha"], base_sha):
            excluded[number] = "dependency_excluded"
            continue
        result = merge_tree(root, current, pr["ref"])
        if missing_ref(result):
            excluded[number] = "head_unavailable"
            continue
        lines = result.stdout.rstrip("\0").split("\0")
        if result.returncode not in (0, 1) or not lines or not lines[0]:
            raise RuntimeError(f"could not integrate pull request #{number}: {result.stderr.strip()}")
        if result.returncode:
            excluded[number] = "conflict"
            continue
        current = make_commit(root, lines[0], current, pr["ref"])
        included.add(number)
    return current, included, excluded


def dependencies_blocked(pr, excluded):
    return bool(pr["depends"] & excluded.keys())


def measure(root, commit):
    progress_path = ROOT / "tools" / "progress.py"
    spec = importlib.util.spec_from_file_location("anyps5_progress", progress_path)
    progress = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(progress)
    with tempfile.TemporaryDirectory(prefix="anyps5-open-pr-progress-") as directory:
        tree = Path(directory) / "tree"
        git(root, "worktree", "add", "--detach", str(tree), commit)
        try:
            progress.PRX = tree / "core" / "libs" / "prx"
            libraries = progress.collect_libraries()
        finally:
            git(root, "worktree", "remove", "--force", str(tree), check=False)
    return {key: libraries[key] for key in ("done", "total", "percent")}


def estimate(repo, root):
    try:
        pull_requests, base_sha = collect_open_prs(repo, root)
        baseline = measure(root, base_sha)
        combined_sha, included, excluded = integrate_pull_requests(root, pull_requests, base_sha, repo)
        potential = measure(root, combined_sha)
        return {"available": True, "generated_at": timestamp(), "base_sha": base_sha,
                "baseline": baseline, "potential": potential,
                "pull_requests": {"open": len(pull_requests), "included": len(included), "excluded": len(excluded)},
                "excluded_reasons": {reason: sum(value == reason for value in excluded.values())
                                     for reason in sorted(set(excluded.values()))}}
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        return {"available": False, "generated_at": timestamp(),
                "reason": str(error) or error.__class__.__name__}
    finally:
        try:
            refs = git(root, "for-each-ref", "--format=%(refname)", REFS, check=False)
        except OSError:
            refs = None
        if refs is not None and refs.returncode == 0:
            try:
                for ref in refs.stdout.splitlines():
                    git(root, "update-ref", "-d", ref, check=False)
            except OSError:
                pass


def main(argv=None):
    parser = argparse.ArgumentParser(description="estimate PRX library progress after combining open pull requests")
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5"))
    parser.add_argument("--root", type=Path, default=ROOT, help="Git checkout used for fetching and scanning")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    result = estimate(args.repo, args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["available"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
