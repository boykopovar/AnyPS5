import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pr_labels

QUEUE_LABEL = "review queue"
UNBLOCKS_LABEL = pr_labels.UNBLOCKS_LABEL
QUEUE_SIZE = 30
BLOCKING_LABELS = {"needs rebase", "perf numbers missing", "open PR limit"}
CONVENTIONS_CHECK = "conventions"
BOT_CHECKS = {"intake", "labels", "eligibility", "report", "pr"}
BUILD_CHECKS = {"linux", "windows", "relinker", "relinker-asan", "register-context", "write-watch", "pipe-write", "edge-watch"}
FAILED = {"failure", "cancelled", "timed_out", "action_required", "startup_failure", "stale", "error"}
PASSED = {"success", "neutral"}
MARKER = "<!-- pr-review-queue -->"
WRITE_PAUSE = 1.0
PAGE_SIZE = 25

PULL_FIELDS = """
fragment Readiness on PullRequest {
  number title isDraft mergeable headRefOid createdAt
  author { login }
  labels(first: 50) { nodes { name } }
  reviewRequests(first: 50) { nodes { requestedReviewer { ... on User { login } } } }
  latestOpinionatedReviews(first: 50) { nodes { state author { login } } }
  commits(last: 1) { nodes { commit { oid statusCheckRollup { contexts(first: 100) {
    pageInfo { hasNextPage }
    nodes {
      __typename
      ... on CheckRun {
        databaseId name status conclusion
        checkSuite { app { slug } workflowRun { workflow { databaseId } } }
      }
      ... on StatusContext { context state }
    }
  } } } } }
}
"""

QUERY = """
query($owner: String!, $name: String!, $after: String, $size: Int!) {
  repository(owner: $owner, name: $name) {
    pullRequests(states: OPEN, baseRefName: "main", first: $size, after: $after) {
      pageInfo { hasNextPage endCursor }
      nodes { ...Readiness }
    }
  }
}
""" + PULL_FIELDS

EXPLAIN_QUERY = """
query($url: URI!) {
  resource(url: $url) {
    ... on PullRequest {
      state baseRefName
      ...Readiness
    }
  }
}
""" + PULL_FIELDS

AUTHOR_QUERY = """
query($login: String!, $after: String, $size: Int!) {
  user(login: $login) {
    pullRequests(states: OPEN, baseRefName: "main", first: $size, after: $after) {
      pageInfo { hasNextPage endCursor }
      nodes { repository { nameWithOwner } ...Readiness }
    }
  }
}
""" + PULL_FIELDS

CLOSED_QUERY = """
query($owner: String!, $name: String!, $label: String!, $after: String) {
  repository(owner: $owner, name: $name) {
    pullRequests(states: [CLOSED, MERGED], labels: [$label], first: 100, after: $after) {
      pageInfo { hasNextPage endCursor }
      nodes { number }
    }
  }
}
"""


def listed(connection):
    if not isinstance(connection, dict) or not isinstance(connection.get("nodes"), list):
        raise ValueError("expected a connection with nodes")
    return connection["nodes"]


def check_of(node):
    if not isinstance(node, dict):
        raise ValueError("check is not an object")
    if node.get("__typename") == "StatusContext":
        state = str(node.get("state") or "").lower()
        done = state not in {"pending", "expected"}
        return {"id": 0, "name": node.get("context"), "source": ("status",), "done": done,
                "conclusion": {"success": "success"}.get(state, "failure") if done else None}
    suite = node.get("checkSuite") or {}
    app = (suite.get("app") or {}).get("slug")
    workflow = (suite.get("workflowRun") or {}).get("workflow") or {}
    return {"id": node.get("databaseId") or 0, "name": node.get("name"),
            "source": ("run", app, workflow.get("databaseId")),
            "done": str(node.get("status") or "").lower() == "completed",
            "conclusion": str(node.get("conclusion") or "").lower() or None}


def pull(node):
    if not isinstance(node, dict) or type(node.get("number")) is not int or node["number"] < 1:
        raise ValueError("expected a positive pull request number")
    if not isinstance(node.get("isDraft"), bool):
        raise ValueError(f"#{node['number']}: draft flag must be a boolean")
    commits = listed(node.get("commits"))
    commit = commits[0]["commit"] if commits else {}
    if commits and "headRefOid" in node and commit.get("oid") != node["headRefOid"]:
        raise ValueError(f"#{node['number']}: head commit changed while reading checks")
    rollup = commit.get("statusCheckRollup")
    contexts = (rollup or {}).get("contexts") or {}
    author = (node.get("author") or {}).get("login") or "ghost"
    reviews = [(item.get("author") or {}).get("login") or "ghost" for item in listed(node.get("latestOpinionatedReviews"))
               if item.get("state") == "CHANGES_REQUESTED"]
    requested = {(item.get("requestedReviewer") or {}).get("login") for item in listed(node.get("reviewRequests"))}
    requested = {login.casefold() for login in requested if isinstance(login, str)}
    return {
        "number": node["number"],
        "title": node.get("title") or "",
        "author": author,
        "draft": node["isDraft"],
        "mergeable": node.get("mergeable"),
        "created": node.get("createdAt") or "",
        "labels": {item["name"] for item in listed(node.get("labels"))},
        "changes": sorted({login for login in reviews if login.casefold() not in requested}),
        "checks": [check_of(item) for item in listed(contexts)] if contexts else [],
        "checks_incomplete": bool((contexts.get("pageInfo") or {}).get("hasNextPage")),
    }


def latest_checks(checks):
    latest = {}
    for check in checks:
        key = check["name"], check["source"]
        if key not in latest or check["id"] > latest[key]["id"]:
            latest[key] = check
    return [check for check in latest.values() if check["name"] not in BOT_CHECKS]


def blockers(pr):
    reasons = []
    if pr["draft"]:
        reasons.append("draft")
    if pr["mergeable"] == "CONFLICTING" or "needs rebase" in pr["labels"]:
        reasons.append("conflicts with main")
    reasons += [f"label: {label}" for label in sorted(BLOCKING_LABELS - {"needs rebase"} & pr["labels"])]
    checks = latest_checks(pr["checks"])
    conventions = [check for check in checks if check["name"] == CONVENTIONS_CHECK]
    if pr["checks_incomplete"]:
        reasons.append("too many checks to read")
    if not conventions:
        reasons.append("conventions has not run")
    elif all(check["done"] and check["conclusion"] not in FAILED | {"success"} for check in conventions):
        reasons.append("conventions did not pass")
    failing = sorted({check["name"] for check in checks if check["done"] and check["conclusion"] in FAILED})
    running = sorted({check["name"] for check in checks if not check["done"]})
    if failing:
        reasons.append("failing: " + ", ".join(failing))
    if running:
        reasons.append("still running: " + ", ".join(running))
    if pr["changes"]:
        reasons.append("change request not answered by a re-request: " + ", ".join(pr["changes"]))
    return reasons


def built(pr):
    return any(check["conclusion"] in PASSED for check in latest_checks(pr["checks"])
               if str(check["name"]).split(" (")[0] in BUILD_CHECKS)


def ranked(prs):
    ready = [pr for pr in prs if not blockers(pr)]
    return sorted(ready, key=lambda pr: (UNBLOCKS_LABEL not in pr["labels"], pr["created"], pr["number"]))


def clean(text):
    text = "".join(ch for ch in text if ch.isprintable())[:100].replace("@", "@​")
    for char in "\\[]|`<>*_~":
        text = text.replace(char, "\\" + char)
    return text


def queue_body(queue, total, ready):
    lines = [MARKER, f"Ready for a maintainer first look: the oldest {len(queue)} of {ready} pull requests that pass checks "
                     f"({total} open). Pull requests with an `{UNBLOCKS_LABEL}` label come first.", ""]
    for index, pr in enumerate(queue, 1):
        flag = (" (unblocks a title)" if UNBLOCKS_LABEL in pr["labels"] else "") + ("" if built(pr) else " (no build ran)")
        lines.append(f"{index}. #{pr['number']} {clean(pr['title'])} by {clean(pr['author'])}{flag}")
    return "\n".join(lines) + "\n"


def pagination_cursor(connection, seen, label):
    cursor = connection["pageInfo"]["endCursor"]
    if not isinstance(cursor, str) or not cursor:
        raise ValueError(f"{label} page has no cursor")
    if cursor in seen:
        raise ValueError(f"{label} pagination did not advance")
    seen.add(cursor)
    return cursor


def unique_pulls(prs):
    seen = set()
    for pr in prs:
        number = pr["number"]
        if number in seen:
            raise ValueError(f"duplicate pull request #{number} in paginated results")
        seen.add(number)
    return prs


def fetch(repo):
    owner, name = repo.split("/", 1)
    prs, after = [], None
    seen = set()
    while True:
        args = ["api", "graphql", "-f", f"query={QUERY}", "-F", f"owner={owner}", "-F", f"name={name}", "-F", f"size={PAGE_SIZE}"]
        if after:
            args += ["-f", f"after={after}"]
        page = json.loads(pr_labels.gh(*args))
        if page.get("errors"):
            raise ValueError(str(page["errors"])[:300])
        connection = page["data"]["repository"]["pullRequests"]
        prs += [pull(node) for node in listed(connection)]
        if not connection["pageInfo"]["hasNextPage"]:
            return unique_pulls(prs)
        after = pagination_cursor(connection, seen, "open pull request")


def fetch_one(repo, number):
    page = json.loads(pr_labels.gh("api", "graphql", "-f", f"query={EXPLAIN_QUERY}",
                                   "-f", f"url=https://github.com/{repo}/pull/{number}"))
    if page.get("errors"):
        raise ValueError(str(page["errors"])[:300])
    node = page["data"]["resource"]
    if node and node["state"] == "OPEN" and node["baseRefName"] == "main":
        return pull(node)
    return None


def missing_user(error):
    try:
        errors = json.loads(error.stdout or "{}").get("errors") or []
    except (ValueError, AttributeError):
        return False
    return bool(errors) and all(isinstance(item, dict) and item.get("type") == "NOT_FOUND"
                                and item.get("path") == ["user"] for item in errors)


def fetch_author(repo, login):
    if login.casefold() == "ghost":
        return fetch(repo)
    prs, after = [], None
    seen = set()
    while True:
        args = ["api", "graphql", "-f", f"query={AUTHOR_QUERY}", "-f", f"login={login}", "-F", f"size={PAGE_SIZE}"]
        if after:
            args += ["-f", f"after={after}"]
        try:
            page = json.loads(pr_labels.gh(*args))
        except subprocess.CalledProcessError as error:
            if missing_user(error):
                return fetch(repo)
            raise
        if page.get("errors"):
            raise ValueError(str(page["errors"])[:300])
        connection = page["data"]["user"]["pullRequests"]
        prs += [pull(node) for node in listed(connection)
                if node["repository"]["nameWithOwner"].casefold() == repo.casefold()]
        if not connection["pageInfo"]["hasNextPage"]:
            return unique_pulls(prs)
        after = pagination_cursor(connection, seen, "author pull request")


def closed_with_label(repo):
    owner, name = repo.split("/", 1)
    closed, after = [], None
    seen = set()
    while True:
        args = ["api", "graphql", "-f", f"query={CLOSED_QUERY}", "-F", f"owner={owner}",
                "-F", f"name={name}", "-f", f"label={QUEUE_LABEL}"]
        if after:
            args += ["-f", f"after={after}"]
        page = json.loads(pr_labels.gh(*args))
        if page.get("errors"):
            raise ValueError(str(page["errors"])[:300])
        connection = page["data"]["repository"]["pullRequests"]
        closed += [node["number"] for node in listed(connection)]
        if not connection["pageInfo"]["hasNextPage"]:
            return closed
        after = pagination_cursor(connection, seen, "closed pull request")


def apply(repo, prs, queue, dry_run, closed=()):
    chosen = {pr["number"] for pr in queue}
    changes = [(number, "--remove-label") for number in closed]
    for pr in prs:
        if pr["number"] in chosen and QUEUE_LABEL not in pr["labels"]:
            changes.append((pr["number"], "--add-label"))
        elif pr["number"] not in chosen and QUEUE_LABEL in pr["labels"]:
            changes.append((pr["number"], "--remove-label"))
    for number, flag in changes:
        print(f"{'would change' if dry_run else 'changed'} #{number}: {'+' if flag == '--add-label' else '-'}{QUEUE_LABEL}")
        if not dry_run:
            pr_labels.gh("pr", "edit", str(number), "-R", repo, flag, QUEUE_LABEL)
            time.sleep(WRITE_PAUSE)
    return changes


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Label the pull requests that are next in the review queue.")
    parser.add_argument("--dry-run", action="store_true", help="print the planned changes and write nothing")
    parser.add_argument("--explain", type=int, metavar="PR", help="print what blocks one pull request and write nothing")
    parser.add_argument("--author", metavar="LOGIN", help="print what blocks each open pull request of one author and write nothing")
    parser.add_argument("--issue", type=int, help="issue whose body lists the queue")
    args = parser.parse_args(argv)
    if args.explain is not None and args.author is not None:
        parser.error("--explain and --author cannot be combined")
    try:
        repo = pr_labels.repository()
        if args.explain is not None:
            pr = fetch_one(repo, args.explain)
            if pr is None:
                parser.error(f"#{args.explain} is not an open pull request to main")
            print("\n".join(blockers(pr)) or "nothing blocks it")
            return 0
        prs = fetch(repo) if args.author is None else fetch_author(repo, args.author)
        closed = closed_with_label(repo) if args.author is None else []
    except subprocess.CalledProcessError as error:
        parser.error((error.stderr or str(error)).strip())
    except (ValueError, KeyError, TypeError, RecursionError) as error:
        parser.error(f"cannot read pull requests: {error}")
    if args.author is not None:
        mine = sorted((pr for pr in prs if pr["author"].casefold() == args.author.casefold()), key=lambda pr: pr["number"])
        for pr in mine:
            print(f"#{pr['number']}: {'; '.join(blockers(pr)) or 'nothing blocks it'}")
        if not mine:
            print(f"{args.author} has no open pull request to main")
        return 0
    ready = ranked(prs)
    queue = ready[:QUEUE_SIZE]
    try:
        apply(repo, prs, queue, args.dry_run, closed)
        if args.issue is not None and not args.dry_run:
            body = queue_body(queue, len(prs), len(ready))
            current = pr_labels.gh("issue", "view", str(args.issue), "-R", repo, "--json", "body", "--jq", ".body")
            if current.strip() != body.strip():
                pr_labels.gh("issue", "edit", str(args.issue), "-R", repo, "--body", body)
    except subprocess.CalledProcessError as error:
        sys.exit(f"cannot update the queue: {(error.stderr or '').strip()}")
    print(f"{len(ready)} of {len(prs)} open pull requests have no blocker; {len(queue)} queued")
    return 0


if __name__ == "__main__":
    sys.exit(main())
