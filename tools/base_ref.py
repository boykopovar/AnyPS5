import subprocess

CANDIDATES = ("refs/remotes/upstream/main", "refs/remotes/origin/main", "refs/heads/main")


def resolve(repo, name=None):
    if name:
        return name
    for candidate in CANDIDATES:
        found = subprocess.run(["git", "-C", str(repo), "rev-parse", "--verify", "--quiet", f"{candidate}^0"],
                               capture_output=True, text=True, encoding="utf-8", errors="replace")
        if found.returncode == 0:
            return candidate
        if found.returncode != 1:
            detail = found.stderr.strip() or f"exit status {found.returncode}"
            raise ValueError(f"{repo}: Git could not resolve the base branch: {detail}")
    raise ValueError(f"no base branch found ({', '.join(CANDIDATES)}); fetch the upstream remote or pass --base")
