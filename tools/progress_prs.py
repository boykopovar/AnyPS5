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
