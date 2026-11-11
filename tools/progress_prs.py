import argparse
import importlib.util
import json
import os
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

import pr_overlap

ROOT = Path(__file__).resolve().parent.parent
REMOTE = "https://github.com"
REFS = "refs/open-pr-progress"


def git(root, *args, check=True):
    result = subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, encoding="utf-8")
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed with {result.returncode}")
    return result


def timestamp():
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def collect_open_prs(repo, root):
    pr_overlap.REPO = repo
    pull_requests = pr_overlap.open_prs("main")
    refspecs = [f"+refs/heads/main:{REFS}/base"]
    for number, pr in sorted(pull_requests.items()):
        pr["ref"] = f"{REFS}/{number}"
        refspecs.append(f"+refs/pull/{number}/head:{pr['ref']}")
    remote = f"{REMOTE}/{repo}.git"
    for start in range(0, len(refspecs), pr_overlap.FETCH_REFS):
        git(root, "fetch", "--no-tags", "-q", remote, *refspecs[start:start + pr_overlap.FETCH_REFS])
    changed = [number for number, pr in pull_requests.items()
               if git(root, "rev-parse", pr["ref"]).stdout.strip() != pr["sha"]]
    if changed:
        raise RuntimeError(f"pull request heads changed during fetch: {', '.join(map(str, changed[:20]))}")
    return pull_requests, git(root, "rev-parse", f"{REFS}/base").stdout.strip()


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


def dependency_order(pull_requests, root, base_sha, repo):
    dependencies = {number: set(pr["depends"]) for number, pr in pull_requests.items()}
    excluded = {}
    for number, required in dependencies.items():
        missing = required - pull_requests.keys()
        for dependency in sorted(missing):
            merge_sha = closed_dependency_status(repo, dependency)
            if not merge_sha or git(root, "merge-base", "--is-ancestor", merge_sha, base_sha, check=False).returncode:
                excluded[number] = "unresolved_dependency"
                break
            dependencies[number].discard(dependency)
    for number, pr in pull_requests.items():
        if git(root, "merge-base", "--is-ancestor", pr["sha"], base_sha, check=False).returncode == 0:
            excluded.pop(number, None)
            dependencies[number] = set()
    pending = set(pull_requests) - excluded.keys()
    order = []
    while pending:
        ready = sorted(number for number in pending if dependencies[number] <= (set(order) | excluded.keys()))
        if not ready:
            for number in pending:
                excluded[number] = "dependency_cycle"
            break
        number = ready[0]
        pending.remove(number)
        order.append(number)
    return order, excluded


def make_commit(root, tree, current, head):
    result = git(root, "-c", "user.name=open-pr-progress", "-c", "user.email=open-pr-progress@invalid", "commit-tree",
                 tree, "-p", current, "-p", head, "-m", "open PR progress estimate")
    return result.stdout.strip()


def integrate_pull_requests(root, pull_requests, base_sha, repo):
    order, excluded = dependency_order(pull_requests, root, base_sha, repo)
    current = base_sha
    included = set()
    for number in order:
        pr = pull_requests[number]
        if dependencies_blocked(pr, excluded):
            excluded[number] = "dependency_excluded"
            continue
        result = git(root, "-c", "core.quotePath=false", "merge-tree", "--write-tree", "--name-only",
                     "--no-messages", "-z", current, pr["ref"], check=False)
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
        return {"available": False, "generated_at": timestamp(), "reason": str(error)}
    finally:
        refs = git(root, "for-each-ref", "--format=%(refname)", REFS, check=False)
        if refs.returncode == 0:
            for ref in refs.stdout.splitlines():
                git(root, "update-ref", "-d", ref, check=False)


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
