import argparse
import codecs
import collections
import subprocess
import sys
from pathlib import Path

CANDIDATES = ("upstream/main", "origin/main", "main")
NO_NEWLINE = "\\ No newline at end of file"


def git(root, *arguments, raw=False):
    result = subprocess.run(["git", "-C", str(root), "-c", "core.quotePath=false", *arguments],
                            capture_output=True, encoding=None if raw else "utf-8", errors=None if raw else "replace")
    if result.returncode:
        stderr = result.stderr.decode("utf-8", "replace") if raw else result.stderr
        raise RuntimeError(f"git {' '.join(arguments[:2])} failed: {stderr.strip()}")
    return result.stdout


def default_onto(root, name=None):
    if name:
        return name
    for candidate in CANDIDATES:
        if subprocess.run(["git", "-C", str(root), "rev-parse", "--verify", "--quiet", f"{candidate}^0"],
                          capture_output=True).returncode == 0:
            return candidate
    raise ValueError(f"no base branch found ({', '.join(CANDIDATES)}); fetch the upstream remote or pass --onto")


def people(root, base, tip):
    lines = git(root, "log", "--no-merges", "--format=%an <%ae>%x1f%s", f"{base}..{tip}", "--", raw=True).split(b"\n")
    return collections.Counter(line.decode("utf-8", "surrogateescape") for line in lines if line)


def commits(root, base, tip):
    return len(git(root, "rev-list", "--no-merges", f"{base}..{tip}", "--").split())


def patch_path(header, prefix):
    path = header[4:].removesuffix("\t")
    if path.startswith('"') and path.endswith('"'):
        path = codecs.escape_decode(path[1:-1].encode("utf-8", "surrogateescape"))[0].decode("utf-8", "surrogateescape")
    return path[len(prefix):] if path.startswith(prefix) else None


def changed_lines(root, base, tip):
    added = collections.defaultdict(collections.Counter)
    deleted = collections.defaultdict(collections.Counter)
    old_path, new_path, in_hunk = None, None, False
    diff = git(root, "diff", "-U0", "--no-renames", "--no-relative", "--no-color", "--no-ext-diff", "--no-textconv",
               "--ignore-submodules=none", "--submodule=short", "--src-prefix=a/", "--dst-prefix=b/",
               "--output-indicator-new=+", "--output-indicator-old=-", "--output-indicator-context= ",
               base, tip, "--", raw=True)
    lines = diff.decode("utf-8", "surrogateescape").split("\n")
    for index, line in enumerate(lines):
        if line.startswith("diff --git "):
            old_path, new_path, in_hunk = None, None, False
        elif not in_hunk and line.startswith("--- "):
            old_path = patch_path(line, "a/")
        elif not in_hunk and line.startswith("+++ "):
            new_path = patch_path(line, "b/")
        elif line.startswith("@@"):
            in_hunk = True
        elif in_hunk and line.startswith("+") and new_path is not None:
            text = line[1:]
            if lines[index + 1:index + 2] == [NO_NEWLINE]:
                text += "\n" + NO_NEWLINE
            added[new_path][text] += 1
        elif in_hunk and line.startswith("-") and old_path is not None:
            deleted[old_path][line[1:]] += 1
    return added, deleted


def added_hunks(root, base, tip):
    hunks = collections.defaultdict(lambda: collections.defaultdict(list))
    positions = collections.defaultdict(lambda: collections.defaultdict(list))
    new_path, in_hunk, old_range, new_line = None, False, None, 0
    diff = git(root, "diff", "-U0", "--inter-hunk-context=0", "--no-renames", "--no-relative", "--no-color", "--no-ext-diff", "--no-textconv",
               "--ignore-submodules=none", "--submodule=short", "--src-prefix=a/", "--dst-prefix=b/",
               "--output-indicator-new=+", "--output-indicator-old=-", "--output-indicator-context= ",
               base, tip, "--", raw=True)
    lines = diff.decode("utf-8", "surrogateescape").split("\n")
    for index, line in enumerate(lines):
        if line.startswith("diff --git "):
            new_path, in_hunk, old_range = None, False, None
        elif not in_hunk and line.startswith("+++ "):
            new_path = patch_path(line, "b/")
        elif line.startswith("@@"):
            in_hunk = True
            old_token, new_token = line.split()[1:3]
            old_start, separator, count = old_token[1:].partition(",")
            old_range = int(old_start), int(count) if separator else 1
            new_start, _, _ = new_token[1:].partition(",")
            new_line = int(new_start)
        elif in_hunk and line.startswith("+") and new_path is not None:
            text = line[1:]
            if lines[index + 1:index + 2] == [NO_NEWLINE]:
                text += "\n" + NO_NEWLINE
            hunks[new_path][old_range].append(text)
            positions[new_path][old_range].append(new_line)
            new_line += 1
    return hunks, positions


def deleted_line_numbers(root, base, tip):
    deleted = collections.defaultdict(set)
    old_path, in_hunk, old_line = None, False, 0
    diff = git(root, "diff", "-U0", "--inter-hunk-context=0", "--no-renames", "--no-relative", "--no-color", "--no-ext-diff", "--no-textconv",
               "--ignore-submodules=none", "--submodule=short", "--src-prefix=a/", "--dst-prefix=b/",
               "--output-indicator-new=+", "--output-indicator-old=-", "--output-indicator-context= ",
               base, tip, "--", raw=True)
    lines = diff.decode("utf-8", "surrogateescape").split("\n")
    for line in lines:
        if line.startswith("diff --git "):
            old_path, in_hunk = None, False
        elif not in_hunk and line.startswith("--- "):
            old_path = patch_path(line, "a/")
        elif line.startswith("@@"):
            in_hunk = True
            token = line.split(maxsplit=2)[1][1:]
            old_line = int(token.partition(",")[0])
        elif in_hunk and line.startswith("-") and old_path is not None:
            deleted[old_path].add(old_line)
            old_line += 1
    return deleted

def prefix_suffix_overlap(pattern, sequence):
    if not pattern:
        return 0
    prefix = [0] * len(pattern)
    for index in range(1, len(pattern)):
        matched = prefix[index - 1]
        while matched and pattern[index] != pattern[matched]:
            matched = prefix[matched - 1]
        if pattern[index] == pattern[matched]:
            matched += 1
        prefix[index] = matched
    matched = 0
    for value in sequence:
        while matched and (matched == len(pattern) or pattern[matched] != value):
            matched = prefix[matched - 1]
        if matched < len(pattern) and pattern[matched] == value:
            matched += 1
    return matched


def upstream_boundary_additions_at_old_range(hunks, old_range, expected):
    matches = collections.defaultdict(set)
    start, count = old_range
    if count:
        return matches
    for upstream_range, lines in hunks.items():
        upstream_start, upstream_count = upstream_range
        if not upstream_count:
            continue
        upstream_end = upstream_start + upstream_count - 1
        if upstream_start == start + 1:
            overlap = prefix_suffix_overlap(lines[:len(expected)], expected)
            branch_start = len(expected) - overlap
            for offset in range(overlap):
                matches[branch_start + offset].add((upstream_range, offset))
        if upstream_end == start:
            overlap = prefix_suffix_overlap(expected[:len(lines)], lines)
            upstream_start_index = len(lines) - overlap
            for offset in range(overlap):
                matches[offset].add((upstream_range, upstream_start_index + offset))
    return matches

def additions_satisfied_upstream(root, old_base, old, new_base):
    if old_base == new_base:
        return {}, {}
    original, _ = added_hunks(root, old_base, old)
    upstream, upstream_positions = added_hunks(root, old_base, new_base)
    satisfied = collections.defaultdict(collections.Counter)
    credited = collections.defaultdict(set)
    for path, hunks in original.items():
        upstream_hunks = upstream.get(path, {})
        exact_occurrences = collections.defaultdict(list)
        for upstream_range, lines in upstream_hunks.items():
            for upstream_index, line in enumerate(lines):
                exact_occurrences[(upstream_range, line)].append((upstream_range, upstream_index))

        branch_lines = []
        for old_range, lines in hunks.items():
            boundary_matches = upstream_boundary_additions_at_old_range(upstream_hunks, old_range, lines)
            for index, line in enumerate(lines):
                exact_group = (old_range, line)
                if exact_group not in exact_occurrences:
                    exact_group = None
                branch_lines.append((line, exact_group, boundary_matches.get(index, set())))
        if not branch_lines:
            continue

        exact_groups = sorted({group for _, group, _ in branch_lines if group is not None})
        upstream_occurrences = set()
        for group in exact_groups:
            upstream_occurrences.update(exact_occurrences[group])
        for _, _, boundary in branch_lines:
            upstream_occurrences.update(boundary)
        if not upstream_occurrences:
            continue

        source = 0
        branch_start = 1
        group_start = branch_start + len(branch_lines)
        group_nodes = {group: group_start + index for index, group in enumerate(exact_groups)}
        upstream_start = group_start + len(group_nodes)
        occurrence_nodes = {occurrence: upstream_start + index
                            for index, occurrence in enumerate(sorted(upstream_occurrences))}
        sink = upstream_start + len(occurrence_nodes)
        graph = [[] for _ in range(sink + 1)]

        def add_edge(start_node, end_node):
            forward = [end_node, len(graph[end_node]), 1]
            reverse = [start_node, len(graph[start_node]), 0]
            graph[start_node].append(forward)
            graph[end_node].append(reverse)
            return forward

        source_edges = []
        for index, (line, exact_group, boundary) in enumerate(branch_lines):
            branch_node = branch_start + index
            source_edges.append(add_edge(source, branch_node))
            if exact_group is not None:
                add_edge(branch_node, group_nodes[exact_group])
            for occurrence in sorted(boundary):
                add_edge(branch_node, occurrence_nodes[occurrence])
        for group, node in group_nodes.items():
            for occurrence in exact_occurrences[group]:
                add_edge(node, occurrence_nodes[occurrence])
        occurrence_edges = {
            occurrence: add_edge(node, sink)
            for occurrence, node in occurrence_nodes.items()
        }

        while True:
            levels = [-1] * len(graph)
            levels[source] = 0
            pending = collections.deque([source])
            while pending:
                node = pending.popleft()
                for target, _, capacity in graph[node]:
                    if capacity and levels[target] < 0:
                        levels[target] = levels[node] + 1
                        pending.append(target)
            if levels[sink] < 0:
                break

            current = [0] * len(graph)
            while True:
                nodes = [source]
                augmenting_path = []
                while nodes:
                    node = nodes[-1]
                    if node == sink:
                        for previous, edge_index in augmenting_path:
                            edge = graph[previous][edge_index]
                            edge[2] -= 1
                            graph[edge[0]][edge[1]][2] += 1
                        nodes = [source]
                        augmenting_path = []
                        continue

                    while current[node] < len(graph[node]):
                        edge = graph[node][current[node]]
                        if edge[2] and levels[edge[0]] == levels[node] + 1:
                            break
                        current[node] += 1
                    if current[node] == len(graph[node]):
                        nodes.pop()
                        if augmenting_path:
                            previous, _ = augmenting_path.pop()
                            current[previous] += 1
                    else:
                        edge_index = current[node]
                        augmenting_path.append((node, edge_index))
                        nodes.append(graph[node][edge_index][0])
                break

        for index, edge in enumerate(source_edges):
            if edge[2] == 0:
                satisfied[path][branch_lines[index][0]] += 1
        for occurrence, edge in occurrence_edges.items():
            if edge[2] == 0:
                upstream_range, upstream_index = occurrence
                credited[path].add(upstream_positions[path][upstream_range][upstream_index])
    return satisfied, credited

def differences(old, new, deleted=False):
    problems = []
    for path in sorted(set(old) | set(new)):
        before, after = old.get(path, collections.Counter()), new.get(path, collections.Counter())
        for text, count in sorted((before - after).items()):
            description = "a deleted line is missing" if deleted else "an added line is missing"
            problems.append(f"{path}: {description} ({count}x): {shown(text)}")
        for text, count in sorted((after - before).items()):
            description = "a line is deleted that the branch did not delete" if deleted else "a line is added that the branch did not add"
            problems.append(f"{path}: {description} ({count}x): {shown(text)}")
    return problems


def text_only(lines):
    return collections.Counter(text.partition("\n")[0] for text in lines.elements())


def shown(text):
    text, unterminated, _ = text.partition("\n")
    return text[:100].replace("\r", "\\r") + (" (no newline at end of file)" if unterminated else "")


def changed_modes(root, base, tip):
    fields = git(root, "diff", "--raw", "-z", "--no-renames", "--no-relative", "--no-ext-diff",
                 "--ignore-submodules=none", base, tip, "--", raw=True).split(b"\0")
    modes = {}
    for header, path in zip(fields[::2], fields[1::2]):
        before, after = header[1:].split(maxsplit=2)[:2]
        if before != after:
            modes[path] = before.decode("ascii"), after.decode("ascii")
    return modes


def mode_differences(root, old_base, old, new_base, new):
    old_modes = changed_modes(root, old_base, old)
    new_modes = changed_modes(root, new_base, new)
    upstream_modes = changed_modes(root, old_base, new_base) if old_modes and old_base != new_base else {}
    problems = []
    for path in sorted(old_modes.keys() | new_modes.keys()):
        if path in old_modes:
            before, expected = old_modes[path]
            base_mode = upstream_modes.get(path, (before, before))[1]
            actual = new_modes.get(path, (base_mode, base_mode))[1]
        else:
            expected, actual = new_modes[path]
        if expected != actual:
            name = path.decode("utf-8", "backslashreplace")
            problems.append(f"{name}: file mode {actual}, expected {expected}")
    return problems


def binary_paths(root, base, tip):
    data = git(root, "diff", "--numstat", "-z", "--no-renames", "--no-relative", "--no-ext-diff",
               "--no-textconv", "--ignore-submodules=none", "--submodule=short", base, tip, "--", raw=True)
    result = set()
    for item in data.split(b"\0"):
        if not item:
            continue
        added, deleted, path = item.split(b"\t", 2)
        if added == deleted == b"-":
            result.add(path)
    return result


def changed_blobs(root, base, tip):
    data = git(root, "diff", "--raw", "-z", "--no-abbrev", "--no-renames", "--no-relative",
               "--no-ext-diff", "--no-textconv", "--ignore-submodules=none", base, tip, "--", raw=True)
    fields = data.split(b"\0")
    result = {}
    for header, path in zip(fields[::2], fields[1::2]):
        metadata = header[1:].split()
        result[path] = metadata[2], metadata[3]
    return result


def binary_differences(root, old_base, old, new_base, new):
    paths = binary_paths(root, old_base, old) | binary_paths(root, new_base, new)
    if not paths:
        return []
    original = changed_blobs(root, old_base, old)
    rebased = changed_blobs(root, new_base, new)
    upstream = changed_blobs(root, old_base, new_base) if old_base != new_base else {}
    problems = []
    for path in sorted(paths):
        name = path.decode("utf-8", "backslashreplace")
        if path not in original:
            problems.append(f"{name}: new binary change requires manual review")
            continue
        before, expected = original[path]
        new_base_blob = upstream[path][1] if path in upstream else before
        actual = rebased[path][1] if path in rebased else new_base_blob
        if actual != expected:
            problems.append(f"{name}: binary content differs; manual review required")
    return problems


def unchanged_base_content_differences(root, old_base, old, new_base, new):
    original = changed_blobs(root, old_base, old)
    rebased = changed_blobs(root, new_base, new)
    problems = []
    for path in sorted(original.keys() & rebased.keys()):
        before, expected = original[path]
        after, actual = rebased[path]
        if before == after and expected != actual:
            name = path.decode("utf-8", "backslashreplace")
            problems.append(f"{name}: rebased content differs although the base content is unchanged")
    return problems


def verify(root, old, new, onto):
    old, new, onto_commit = (git(root, "rev-parse", "--verify", "--end-of-options", f"{ref}^0").strip()
                             for ref in (old, new, onto))
    old_base = git(root, "merge-base", old, onto_commit).strip()
    new_base = git(root, "merge-base", new, onto_commit).strip()
    problems = []
    if people(root, old_base, old) != people(root, new_base, new):
        problems.append("authors or subjects differ between the old and the new commits")
    count_old, count_new = commits(root, old_base, old), commits(root, new_base, new)
    if count_old != count_new:
        problems.append(f"the branch has {count_new} commits on {onto}, and had {count_old} on its old base")
    old_added, old_deleted = changed_lines(root, old_base, old)
    new_added, new_deleted = changed_lines(root, new_base, new)
    upstream_added, upstream_deleted = changed_lines(root, old_base, new_base) if old_added or old_deleted else ({}, {})
    satisfied_added, credited_upstream = additions_satisfied_upstream(root, old_base, old, new_base) if old_added else ({}, {})
    expected_added = {
        path: lines - satisfied_added.get(path, collections.Counter())
        for path, lines in old_added.items()
    }
    problems += differences(expected_added, new_added)
    if credited_upstream:
        removed_upstream = deleted_line_numbers(root, new_base, new)
        for path, line_numbers in credited_upstream.items():
            for line_number in sorted(line_numbers & removed_upstream.get(path, set())):
                problems.append(f"{path}: an upstream-added line credited for a branch addition was removed at line {line_number}")
    expected_deleted = {
        path: lines - (upstream_deleted.get(path, collections.Counter()) - text_only(upstream_added.get(path, collections.Counter())))
        for path, lines in old_deleted.items()
    }
    problems += differences(expected_deleted, new_deleted, deleted=True)
    problems += mode_differences(root, old_base, old, new_base, new)
    problems += binary_differences(root, old_base, old, new_base, new)
    if not problems:
        problems += unchanged_base_content_differences(root, old_base, old, new_base, new)
    return old_base, count_old, count_new, problems


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="check that a rebased branch kept its added and deleted lines, Git-classified binary contents, file modes, authors and commits")
    parser.add_argument("old", help="the branch or commit before the rebase")
    parser.add_argument("new", help="the branch or commit after the rebase")
    parser.add_argument("--onto", help="the branch the new commits sit on (default: upstream/main, then origin/main, then main)")
    parser.add_argument("--repo", type=Path, default=Path("."), help="the repository to check (default: the current directory)")
    args = parser.parse_args(argv)
    try:
        onto = default_onto(args.repo, args.onto)
        old_base, count_old, count_new, problems = verify(args.repo, args.old, args.new, onto)
    except (OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    print(f"{args.new} rebases {args.old} onto {onto} (old base {old_base[:10]}): {count_old} commits before, {count_new} after")
    for problem in problems:
        print(f"error: {problem}")
    if problems:
        print(f"{len(problems)} difference(s) require review")
        return 1
    print("all added and deleted lines, Git-classified binary contents, file modes, authors and commits are kept")
    return 0


if __name__ == "__main__":
    sys.exit(main())
