import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pr_labels

LABEL = "already in main"
MARKER = "<!-- pr-already-in-main -->"
PAGE_SIZE = 50
MAX_FILES = 100
WRITE_PAUSE = 1.0

QUERY = """
query($owner: String!, $name: String!, $after: String, $size: Int!) {
  repository(owner: $owner, name: $name) {
    defaultBranchRef { target { ... on Commit { oid tree { oid } } } }
    pullRequests(states: OPEN, baseRefName: "main", first: $size, after: $after) {
      pageInfo { hasNextPage endCursor }
      nodes {
        number
        labels(first: 50) { nodes { name } }
        potentialMergeCommit { tree { oid } parents(first: 2) { nodes { oid tree { oid } } } }
      }
    }
  }
}
"""


def comment(base):
    return (f"{MARKER}\nMerging this pull request would not change `main`: GitHub's test merge of it into `main` at {base[:10]} "
            "has the same files as that commit, and no file it touches has changed in `main` since. Its changes look merged already, "
            "possibly through another pull request. Nothing is closed automatically. Close it if it is superseded, or push what is "
            f"still missing; the `{LABEL}` label goes away once merging would change `main` again.")


def listed(connection):
    if not isinstance(connection, dict) or not isinstance(connection.get("nodes"), list):
        raise ValueError("expected a connection with nodes")
    return connection["nodes"]


def graphql(query, **variables):
    args = ["api", "graphql", "-f", f"query={query}"]
    for key, value in variables.items():
        args += ["-F" if isinstance(value, int) else "-f", f"{key}={value}"]
    page = json.loads(pr_labels.gh(*args))
    if page.get("errors"):
        raise ValueError(str(page["errors"])[:300])
    return page["data"]


def candidate(node):
    merge = node.get("potentialMergeCommit")
    if not isinstance(node.get("number"), int):
        raise ValueError("pull request needs a number")
    labels = {item["name"] for item in listed(node.get("labels"))}
    if not merge:
        return {"number": node["number"], "labels": labels, "base": None}
    parents = listed(merge.get("parents"))
    if not parents or merge["tree"]["oid"] != parents[0]["tree"]["oid"]:
        return {"number": node["number"], "labels": labels, "base": None}
    return {"number": node["number"], "labels": labels, "base": parents[0]["oid"]}


def fetch(repo):
    owner, name = repo.split("/", 1)
    prs, after, main = [], None, None
    while True:
        variables = {"owner": owner, "name": name, "size": PAGE_SIZE}
        if after:
            variables["after"] = after
        data = graphql(QUERY, **variables)["repository"]
        main = main or data["defaultBranchRef"]["target"]["oid"]
        connection = data["pullRequests"]
        prs += [candidate(node) for node in listed(connection)]
        if not connection["pageInfo"]["hasNextPage"]:
            return main, prs
        after = connection["pageInfo"]["endCursor"]


def touched(repo, number):
    out = pr_labels.gh("api", "--paginate", f"repos/{repo}/pulls/{number}/files?per_page=100",
                       "--jq", "[.[] | .filename, (.previous_filename // empty)]")
    return sorted(set(pr_labels.decode_pages(out)))


def unchanged_since(repo, base, main, paths):
    owner, name = repo.split("/", 1)
    fields = []
    for index, path in enumerate(paths):
        fields.append(f"b{index}: object(expression: {json.dumps(base + ':' + path)}) {{ oid }}")
        fields.append(f"m{index}: object(expression: {json.dumps(main + ':' + path)}) {{ oid }}")
    query = "query($owner: String!, $name: String!) { repository(owner: $owner, name: $name) { " + " ".join(fields) + " } }"
    data = graphql(query, owner=owner, name=name)["repository"]
    return all((data[f"b{index}"] or {}).get("oid") == (data[f"m{index}"] or {}).get("oid") for index in range(len(paths)))


def commented(repo, number):
    out = pr_labels.gh("api", "--paginate", f"repos/{repo}/issues/{number}/comments",
                       "--jq", f'[.[] | select(.body | contains("{MARKER}"))] | length')
    return sum(int(count) for count in out.split())


def confirmed(repo, main, pr):
    if pr["base"] is None:
        return False
    if pr["base"] == main:
        return True
    paths = touched(repo, pr["number"])
    if not paths or len(paths) > MAX_FILES:
        return False
    return unchanged_since(repo, pr["base"], main, paths)


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Label open pull requests whose merge would not change main.")
    parser.add_argument("--dry-run", action="store_true", help="print the planned changes and write nothing")
    args = parser.parse_args(argv)
    try:
        repo = pr_labels.repository()
        head, prs = fetch(repo)
    except subprocess.CalledProcessError as error:
        parser.error((error.stderr or str(error)).strip())
    except (ValueError, KeyError, TypeError, RecursionError) as error:
        parser.error(f"cannot read pull requests: {error}")
    failed = []
    for pr in prs:
        try:
            hit = confirmed(repo, head, pr)
            if hit == (LABEL in pr["labels"]):
                continue
            print(f"{'would change' if args.dry_run else 'changed'} #{pr['number']}: {'+' if hit else '-'}{LABEL}")
            if args.dry_run:
                continue
            pr_labels.gh("pr", "edit", str(pr["number"]), "-R", repo, "--add-label" if hit else "--remove-label", LABEL)
            if hit and not commented(repo, pr["number"]):
                pr_labels.gh("api", "-X", "POST", f"repos/{repo}/issues/{pr['number']}/comments", "-f", f"body={comment(pr['base'])}")
            time.sleep(WRITE_PAUSE)
        except subprocess.CalledProcessError as error:
            failed.append(pr["number"])
            print(f"cannot check #{pr['number']}: {(error.stderr or '').strip()}", file=sys.stderr)
        except (ValueError, KeyError, TypeError, RecursionError) as error:
            failed.append(pr["number"])
            print(f"cannot read #{pr['number']}: {error}", file=sys.stderr)
    if failed:
        sys.exit(f"{len(failed)} pull requests not checked: {', '.join(f'#{number}' for number in failed)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
