import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import performance_compare

UNBLOCKS = re.compile(r'Unblocks:[ \t]*([^\r\n]*(?:\r?\n[ \t]*[-*][ \t]+(?!\[[ xX]\])[^\r\n]*)*)')
TITLE_ID = re.compile(r"(?<![A-Za-z0-9])[A-Z]{4}\d{5}(?!\d)")
NONE = re.compile(r"[-*_ \t\r\n]*none\b", re.I)
PERF = re.compile(r"perf(\(|!?:)")
FENCE = re.compile(r"(?:```|~~~)[^\r\n]*\r?\n(.*?)(?:```|~~~)", re.S)
MARKER = "<!-- pr-labels-perf -->"
UNBLOCKS_LABEL = "unblocks title"
PERF_LABEL = "perf"
MISSING_LABEL = "perf numbers missing"
MANAGED = {UNBLOCKS_LABEL, PERF_LABEL, MISSING_LABEL}
WRITE_PAUSE = 1.0
COMMENT = (f"{MARKER}\nThis pull request is labeled `perf`, so it needs the summary line of a capture on `main` and one with the change, "
           "from the same title, scene and machine, as the Performance report form asks. "
           "Please paste both `aps5-summary.jsonl` lines, attach the two traces and add the output of `tools/trace_digest.py`.")


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8", check=True).stdout


def repository():
    return os.environ.get("GITHUB_REPOSITORY") or gh("repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner").strip()


def strip_comments(body):
    return re.sub(r"<!--.*?-->", "", body or "", flags=re.S)


def title_ids(body):
    match = UNBLOCKS.search(strip_comments(body))
    return set(TITLE_ID.findall(match.group(1))) if match and not NONE.match(match.group(1)) else set()


def is_perf(title):
    return bool(PERF.match(title or ""))


def has_numbers(body):
    body = strip_comments(body)
    candidates = [line.strip() for line in body.splitlines()] + [block.strip() for block in FENCE.findall(body)]
    commits = set()
    for text in candidates:
        if not text.startswith("{"):
            continue
        try:
            summary = json.loads(text)
            if isinstance(summary, dict):
                commits.add(performance_compare.validate(summary)["commit"])
        except (ValueError, RecursionError):
            continue
    return len(commits) >= 2


def labels_for(pr):
    wanted = set()
    if title_ids(pr["body"]):
        wanted.add(UNBLOCKS_LABEL)
    if is_perf(pr["title"]):
        wanted.add(PERF_LABEL)
        if not has_numbers(pr["body"]):
            wanted.add(MISSING_LABEL)
    current = set(pr["labels"])
    return wanted - current, (MANAGED & current) - wanted


def decode_pages(out):
    decoder = json.JSONDecoder()
    offset = 0
    items = []
    pages = 0
    while offset < len(out):
        while offset < len(out) and out[offset].isspace():
            offset += 1
        if offset == len(out):
            break
        page, offset = decoder.raw_decode(out, offset)
        if not isinstance(page, list):
            raise ValueError("gh api --paginate returned a non-array page")
        pages += 1
        items.extend(page)
    if not pages:
        raise ValueError("gh api --paginate returned no JSON pages")
    return items


def pull(data):
    return {"number": data["number"], "title": data["title"], "body": data["body"] or "",
            "labels": [label["name"] for label in data["labels"]], "state": data["state"]}


def commented(repo, number):
    out = gh("api", "--paginate", f"repos/{repo}/issues/{number}/comments",
             "--jq", f'[.[] | select(.body | contains("{MARKER}"))] | length')
    return sum(int(count) for count in out.split())


def process(repo, pr, dry_run, comment=True):
    if pr["state"] != "open":
        return set(), set(), False
    add, remove = labels_for(pr)
    wants_comment = comment and (MISSING_LABEL in add or (MISSING_LABEL in pr["labels"] and MISSING_LABEL not in remove))
    post = wants_comment and not commented(repo, pr["number"])
    if not dry_run:
        if add or remove:
            args = ["pr", "edit", str(pr["number"]), "-R", repo]
            if add:
                args += ["--add-label", ",".join(sorted(add))]
            if remove:
                args += ["--remove-label", ",".join(sorted(remove))]
            gh(*args)
        if post:
            gh("api", "-X", "POST", f"repos/{repo}/issues/{pr['number']}/comments", "-f", f"body={COMMENT}")
    return add, remove, post


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Label pull requests from their title and template fields.")
    which = parser.add_mutually_exclusive_group(required=True)
    which.add_argument("--pr", type=int, help="label one pull request")
    which.add_argument("--all", action="store_true", help="label every open pull request")
    parser.add_argument("--dry-run", action="store_true", help="print the planned changes and write nothing")
    args = parser.parse_args(argv)
    try:
        repo = repository()
        if args.pr is not None:
            prs = [pull(json.loads(gh("api", f"repos/{repo}/pulls/{args.pr}")))]
        else:
            prs = [pull(data) for data in decode_pages(gh("api", "--paginate", f"repos/{repo}/pulls?state=open&base=main&per_page=100"))]
    except subprocess.CalledProcessError as error:
        parser.error((error.stderr or str(error)).strip())
    except (ValueError, RecursionError) as error:
        parser.error(f"cannot read pull requests: {error}")
    failed = []
    for pr in prs:
        try:
            add, remove, post = process(repo, pr, args.dry_run, comment=args.pr is not None)
        except subprocess.CalledProcessError as error:
            failed.append(pr["number"])
            print(f"cannot label #{pr['number']}: {(error.stderr or '').strip()}", file=sys.stderr)
            continue
        if add or remove or post:
            plan = [f"+{label}" for label in sorted(add)] + [f"-{label}" for label in sorted(remove)] + (["comment"] if post else [])
            print(f"{'would change' if args.dry_run else 'changed'} #{pr['number']}: {' '.join(plan)}")
            if args.all and not args.dry_run:
                time.sleep(WRITE_PAUSE)
    if failed:
        sys.exit(f"{len(failed)} pull requests not labeled: {', '.join(f'#{number}' for number in failed)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
