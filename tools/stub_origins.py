import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DECLARATION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
PULL_REQUEST = re.compile(r"(?:pull request #|\(#)(\d+)")
MERGED_PULL_REQUEST = re.compile(r"^Merge pull request #(\d+) from ")
HEADER = re.compile(r"^([0-9a-f]{40}) \d+ (\d+)(?: \d+)?$")


def available(root):
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "--is-inside-work-tree"], capture_output=True, text=True)
    return result.returncode == 0 and result.stdout.strip() == "true"


def declarations(library):
    found = {}
    for source in sorted(Path(library).rglob("*.cpp")):
        if "tests" in source.relative_to(library).parts:
            continue
        text = source.read_text(encoding="utf-8", errors="ignore")
        for match in DECLARATION.finditer(text):
            found.setdefault(match.group(1), (source, text.count("\n", 0, match.start(1)) + 1))
    return found


def blame(root, source):
    result = subprocess.run(["git", "-C", str(root), "blame", "--porcelain", "-w", "--", str(Path(source).relative_to(root))],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        return {}
    lines, commits, current = {}, {}, None
    for line in result.stdout.splitlines():
        header = HEADER.match(line)
        if header:
            current = header.group(1)
            lines[int(header.group(2))] = current
            commits.setdefault(current, {})
        elif current and line.startswith("author "):
            commits[current]["author"] = line[len("author "):]
        elif current and line.startswith("author-time "):
            commits[current]["time"] = int(line.split()[1])
        elif current and line.startswith("summary "):
            commits[current]["summary"] = line[len("summary "):]
    return {number: (sha, commits[sha]) for number, sha in lines.items()}


def pull_requests(root):
    result = subprocess.run(["git", "-C", str(root), "log", "--format=%H %P%x1f%s", "--reverse", "--first-parent", "HEAD"],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        return {}
    chain = []
    for line in result.stdout.splitlines():
        head, _, subject = line.partition("\x1f")
        hashes = head.split()
        chain.append((hashes[0], hashes[1:], subject))
    graph = subprocess.run(["git", "-C", str(root), "rev-list", "--parents", "HEAD"],
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
    parents = {}
    for line in graph.stdout.splitlines():
        parts = line.split()
        parents[parts[0]] = parts[1:]
    numbers, seen = {}, set()
    for sha, first_parents, subject in chain:
        if len(first_parents) == 2:
            pending = [first_parents[1]]
            while pending:
                commit = pending.pop()
                if commit in seen:
                    continue
                seen.add(commit)
                pull = MERGED_PULL_REQUEST.search(subject)
                if pull:
                    numbers.setdefault(commit, int(pull.group(1)))
                pending.extend(parents.get(commit, []))
        seen.add(sha)
    return numbers


def origins(root, library, names, numbers=None):
    found, cache = {}, {}
    numbers = numbers or {}
    for name, (source, number) in declarations(library).items():
        if name not in names:
            continue
        if source not in cache:
            cache[source] = blame(root, source)
        entry = cache[source].get(number)
        if entry and entry[1].get("time") is not None:
            sha, info = entry
            moment = datetime.fromtimestamp(info["time"], timezone.utc)
            pull = PULL_REQUEST.search(info.get("summary", ""))
            pull_number = int(pull.group(1)) if pull else numbers.get(sha)
            found[name] = {"commit": sha, "date": moment.date().isoformat(), "time": moment.strftime("%H:%M UTC"),
                           "author": info.get("author", ""), "pr": pull_number}
    return found
