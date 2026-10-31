import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DECLARATION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
HEADER = re.compile(r"^([0-9a-f]{40}) \d+ (\d+)(?: \d+)?$")


def available(root):
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "--is-inside-work-tree"], capture_output=True, text=True)
    return result.returncode == 0 and result.stdout.strip() == "true"


def declarations(library):
    found = {}
    for source in sorted(Path(library).rglob("*.cpp")):
        if "tests" in source.relative_to(library).parts:
            continue
        for number, line in enumerate(source.read_text(encoding="utf-8", errors="ignore").splitlines(), 1):
            for match in DECLARATION.finditer(line):
                found.setdefault(match.group(1), (source, number))
    return found


def blame(root, source):
    result = subprocess.run(["git", "-C", str(root), "blame", "--porcelain", "-w", "--", str(Path(source).relative_to(root))],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        return {}
    lines, times, current = {}, {}, None
    for line in result.stdout.splitlines():
        header = HEADER.match(line)
        if header:
            current = header.group(1)
            lines[int(header.group(2))] = current
        elif current and line.startswith("author-time "):
            times[current] = int(line.split()[1])
    return {number: (sha, times.get(sha)) for number, sha in lines.items()}


def origins(root, library, names):
    found, cache = {}, {}
    for name, (source, number) in declarations(library).items():
        if name not in names:
            continue
        if source not in cache:
            cache[source] = blame(root, source)
        entry = cache[source].get(number)
        if entry and entry[1] is not None:
            found[name] = {"commit": entry[0], "date": datetime.fromtimestamp(entry[1], timezone.utc).date().isoformat()}
    return found
