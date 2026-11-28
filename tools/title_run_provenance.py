import re


def commit_id(value):
    return value if isinstance(value, str) and re.fullmatch(r"[0-9a-f]{7,40}", value) else None


def compatible(left, right):
    return left.startswith(right) or right.startswith(left)


def qualification(run, source):
    native = run.get("native")
    identities, unknown = [], False
    if not isinstance(native, dict):
        native = {}
    errors = native.get("summary_errors", {})
    if not isinstance(errors, dict) or any(type(count) is not int or count != 0 for count in errors.values()):
        unknown = True
    summaries = native.get("summary", [])
    if not isinstance(summaries, list):
        unknown = True
        summaries = []
    for entry in summaries:
        if isinstance(entry, dict):
            identities.append((entry.get("commit"), entry.get("dirty")))
        else:
            unknown = True
    report = native.get("report")
    if report is not None:
        if isinstance(report, dict):
            identities.append((report.get("commit"), report.get("modified")))
        else:
            unknown = True
    commits = list(dict.fromkeys(commit_id(commit) for commit, modified in identities if commit_id(commit)))
    modified = True if any(value is True for commit, value in identities) else False if identities and all(value is False for commit, value in identities) else None
    unknown |= not identities or any(commit_id(commit) is None or type(value) is not bool for commit, value in identities)
    source = source if isinstance(source, dict) else {}
    expected = commit_id(source.get("commit"))
    if any(not compatible(left, right) for left in commits for right in commits):
        status = "conflicting"
    elif modified is True:
        status = "modified"
    elif unknown or expected is None or type(source.get("dirty")) is not bool:
        status = "unknown"
    elif not all(compatible(commit, expected) for commit in commits):
        status = "mismatch"
    elif source["dirty"]:
        status = "source-modified"
    else:
        status = "matched"
    return {"status": status, "commits": commits, "modified": modified}


def label(provenance):
    commits = ", ".join(commit[:12] for commit in provenance["commits"])
    return provenance["status"] + (f" ({commits})" if commits else "")
