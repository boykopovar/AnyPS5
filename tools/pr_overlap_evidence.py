import base64
import hashlib
import json
import os
import re
import subprocess
import tempfile
import threading
from pathlib import Path


MAX_ARTIFACT_BYTES = 4 * 1024 * 1024
MAX_PACKET_BYTES = 128 * 1024 * 1024
MAX_METADATA_BYTES = 8 * 1024 * 1024
MAX_PEERS = 128
MAX_PATHS = 256
MAX_FILE_RECORDS = 4096
MAX_MERGE_BASES = 16
MAX_MANIFEST_BYTES = 16 * 1024 * 1024
GIT_TIMEOUT = 20


def review_raw_git(root, *args, limit=None, allowed_codes=(0,)):
    if limit is None:
        limit = MAX_METADATA_BYTES
    env = os.environ.copy()
    for name in ("GIT_GLOB_PATHSPECS", "GIT_NOGLOB_PATHSPECS", "GIT_ICASE_PATHSPECS", "GIT_EXTERNAL_DIFF", "GIT_DIFF_OPTS"):
        env.pop(name, None)
    env.update(GIT_LITERAL_PATHSPECS="1", GIT_NO_LAZY_FETCH="1", GIT_NO_REPLACE_OBJECTS="1",
               GIT_OPTIONAL_LOCKS="0", GIT_TERMINAL_PROMPT="0")
    command = ["git", "--no-pager", "--no-replace-objects", "-c", "core.quotePath=true", *args]
    with tempfile.TemporaryFile() as errors:
        with subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=errors) as process:
            expired = threading.Event()

            def stop():
                expired.set()
                process.kill()

            timer = threading.Timer(GIT_TIMEOUT, stop)
            timer.daemon = True
            timer.start()
            try:
                output = process.stdout.read(limit + 1)
                if len(output) > limit:
                    process.kill()
                result = process.wait()
            except BaseException:
                process.kill()
                process.wait()
                raise
            finally:
                timer.cancel()
            if expired.is_set():
                raise ValueError(f"Git evidence command exceeded {GIT_TIMEOUT} seconds")
            if len(output) > limit:
                raise ValueError(f"Git evidence output exceeds {limit} bytes")
            if result not in allowed_codes:
                errors.seek(0)
                detail = errors.read(4096).decode("utf-8", errors="backslashreplace").strip()
                raise ValueError(f"Git evidence command failed ({result}): {detail}")
            return output


def validate_review_directory(destination, root):
    destination = Path(destination)
    if not destination.name or destination.name in (".", ".."):
        raise ValueError("review directory must name a new directory")
    parent = destination.parent.resolve(strict=True)
    if not parent.is_dir():
        raise ValueError("review directory parent is not a directory")
    destination = parent / destination.name
    if os.path.lexists(destination):
        raise ValueError(f"review directory already exists: {destination}")
    for option in ("--git-dir", "--git-common-dir"):
        raw = review_raw_git(root, "rev-parse", "--path-format=absolute", option)
        git_directory = Path(os.fsdecode(raw.strip())).resolve()
        if destination == git_directory or git_directory in destination.parents:
            raise ValueError("review directory must be outside Git metadata")
    return destination


def _json_value(value):
    if isinstance(value, dict):
        return {str(key): _json_value(item) for key, item in sorted(value.items(), key=lambda item: str(item[0]))}
    if isinstance(value, (set, frozenset)):
        return [_json_value(item) for item in sorted(value)]
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return value


def _json_bytes(value):
    output = bytearray()
    for chunk in json.JSONEncoder(ensure_ascii=True, indent=2, sort_keys=True).iterencode(_json_value(value)):
        encoded = chunk.encode("utf-8")
        if len(output) + len(encoded) + 1 > MAX_MANIFEST_BYTES:
            raise ValueError("review manifest byte limit exceeded")
        output.extend(encoded)
    output.extend(b"\n")
    return bytes(output)


def _pin(root, value, kind):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", value):
        raise ValueError(f"evidence requires a pinned {kind} object ID")
    revision = f"{value}^0" if kind == "commit" else f"{value}:"
    result = review_raw_git(root, "rev-parse", "--verify", revision).decode("ascii").strip()
    if kind == "commit" and result != value:
        raise ValueError("evidence commit does not match its pin")
    return result


def _path_value(path):
    try:
        text = path.decode("utf-8")
    except UnicodeDecodeError:
        text = None
    return {"utf8": text, "base64": base64.b64encode(path).decode("ascii")}


def _path_argument(path):
    if os.name == "nt":
        return path.decode("utf-8")
    return os.fsdecode(path)


class _Packet:
    def __init__(self, destination, root):
        self.destination = destination
        self.root = root
        self.objects = {}
        self.incomplete = []
        self.total = 0
        self.records = 0
        self.blobs = {}
        self.trees = {}

    def issue(self, reason, **context):
        item = {"reason": str(reason), **context}
        if item not in self.incomplete:
            self.incomplete.append(item)

    def write(self, path, data):
        if self.total + len(data) > MAX_PACKET_BYTES:
            raise ValueError("review packet byte limit exceeded")
        with (self.destination / path).open("xb") as output:
            output.write(data)
        self.total += len(data)

    def store(self, data, kind):
        if len(data) > MAX_ARTIFACT_BYTES:
            raise ValueError(f"artifact exceeds {MAX_ARTIFACT_BYTES} bytes")
        digest = hashlib.sha256(data).hexdigest()
        path = f"objects/{digest}.{kind}"
        if path not in self.objects:
            descriptor = {"path": path, "sha256": digest, "bytes": len(data), "kind": kind}
            self.write(path, data)
            self.objects[path] = descriptor
        return self.objects[path]

    def paths(self, before, after):
        output = review_raw_git(self.root, "diff", "--name-only", "-z", "--no-renames", "--no-ext-diff",
                                "--no-textconv", "--ignore-submodules=none", "--no-relative",
                                before, after, limit=MAX_METADATA_BYTES)
        paths = sorted(set(output.rstrip(b"\0").split(b"\0"))) if output else []
        return paths

    def select(self, changed, wanted, before, after):
        if wanted is None:
            return changed
        mapped = {}
        for path in changed:
            mapped.setdefault(path.decode("utf-8", errors="replace"), []).append(path)
        for name in wanted:
            if len(mapped.get(name, ())) > 1:
                self.issue("scanner path decoding is ambiguous", before=before, after=after, scanner_path=name)
        return [path for path in changed if path.decode("utf-8", errors="replace") in wanted]

    def snapshot(self, ref, path):
        result = {"status": "unavailable", "mode": None, "git_oid": None, "git_type": None, "artifact": None}
        try:
            argument = _path_argument(path)
            key = (ref, path)
            if key not in self.trees:
                self.trees[key] = review_raw_git(self.root, "ls-tree", "-z", ref, "--", argument)
            raw = self.trees[key]
            if not raw:
                result["status"] = "absent"
                return result
            entries = [entry.split(b"\t", 1) for entry in raw.rstrip(b"\0").split(b"\0")]
            matches = [metadata for metadata, name in entries if name == path]
            if len(matches) != 1:
                raise ValueError("tree entry does not match the literal path")
            mode, kind, oid = matches[0].decode("ascii").split()
            result.update(mode=mode, git_oid=oid, git_type=kind)
            if kind == "tree":
                result.update(status="absent", reason="path is a directory, not a file")
                return result
            if mode == "160000" and kind == "commit":
                result["status"] = "gitlink"
                return result
            if kind != "blob":
                raise ValueError(f"unsupported tree entry type {kind}")
            if oid not in self.blobs:
                size = int(review_raw_git(self.root, "cat-file", "-s", oid))
                if size > MAX_ARTIFACT_BYTES:
                    self.blobs[oid] = {"status": "omitted", "bytes": size, "artifact": None,
                                       "reason": f"blob exceeds {MAX_ARTIFACT_BYTES} bytes"}
                else:
                    data = review_raw_git(self.root, "cat-file", "blob", oid, limit=MAX_ARTIFACT_BYTES)
                    if len(data) != size:
                        raise ValueError("blob size changed while collecting evidence")
                    self.blobs[oid] = {"status": "stored", "bytes": size, "artifact": self.store(data, "blob")}
            result.update(self.blobs[oid])
            if result["status"] != "stored":
                self.issue(result["reason"], ref=ref, path=_path_value(path))
        except (ValueError, UnicodeError) as error:
            result.update(reason=str(error))
            self.issue(error, ref=ref, path=_path_value(path))
        return result

    def files(self, before, after, paths):
        files = []
        if len(paths) > MAX_PATHS:
            self.issue("changed path limit exceeded", before=before, after=after, total=len(paths))
        for path in paths[:MAX_PATHS]:
            if self.records >= MAX_FILE_RECORDS:
                self.issue("file record limit exceeded", before=before, after=after)
                break
            self.records += 1
            patch = {"status": "unavailable", "artifact": None}
            try:
                argument = _path_argument(path)
                raw = review_raw_git(self.root, "-c", "diff.mnemonicPrefix=false", "-c", "diff.noprefix=false",
                                     "diff", "--binary", "--full-index", "--no-renames", "--no-ext-diff",
                                     "--no-textconv", "--no-color", "--src-prefix=a/", "--dst-prefix=b/",
                                     "--diff-algorithm=myers", "--no-indent-heuristic", "--unified=3",
                                     "--ignore-submodules=none", "--submodule=short", "--no-relative", "--inter-hunk-context=0",
                                     before, after, "--", argument, limit=MAX_ARTIFACT_BYTES)
                patch.update(status="stored", artifact=self.store(raw, "patch"))
            except (ValueError, UnicodeError) as error:
                patch.update(reason=str(error))
                self.issue(error, before=before, after=after, path=_path_value(path))
            files.append({"path": _path_value(path), "before": self.snapshot(before, path),
                          "after": self.snapshot(after, path), "patch": patch})
        return files


def review_revision(packet, base, pr, paths=None):
    view = {"head": pr.get("ref"), "head_tree": None, "merge_bases": [], "originals": [],
            "normalized": {"state": "unavailable", "reason": "checker could not normalize this revision"}}
    try:
        head = _pin(packet.root, pr["ref"], "commit")
        view["head_tree"] = _pin(packet.root, head, "tree")
        bases = sorted(set(review_raw_git(packet.root, "merge-base", "--all", base, head, allowed_codes=(0, 1)).decode("ascii").split()))
        if len(bases) > MAX_MERGE_BASES:
            packet.issue("merge base limit exceeded", head=head, total=len(bases))
            bases = bases[:MAX_MERGE_BASES]
        if not bases:
            packet.issue("no common merge base", head=head)
        view["merge_bases"] = bases
        wanted = None if paths is None else set(paths)
        for ancestor in bases:
            changed = packet.paths(ancestor, head)
            selected = packet.select(changed, wanted, ancestor, head)
            fallback = wanted is not None and not wanted.issubset({path.decode("utf-8", errors="replace") for path in changed})
            if fallback:
                selected = changed
            view["originals"].append({"base": ancestor, "head": head,
                                      "selection": "original paths for normalized comparison" if fallback else "changed paths",
                                      "files": packet.files(ancestor, head, selected)})
        if pr.get("tree"):
            tree = _pin(packet.root, pr["tree"], "tree")
            baseline = _pin(packet.root, base, "tree")
            changed = packet.paths(baseline, tree)
            selected = packet.select(changed, wanted, baseline, tree)
            view["normalized"] = {"state": "available", "base_tree": baseline, "tree": tree,
                                  "files": packet.files(baseline, tree, selected)}
    except (ValueError, UnicodeError, KeyError) as error:
        packet.issue(error, head=pr.get("ref"))
        view["error"] = str(error)
    return view


def write_review_evidence(destination, root, base, prs, findings, main_exports, rendered, repo, checker_path=None):
    destination = validate_review_directory(destination, root)
    root = Path(root)
    git_version = review_raw_git(root, "--version").decode("ascii").strip()
    version = re.search(r"\b(\d+)\.(\d+)\b", git_version)
    if not version or tuple(map(int, version.groups())) < (2, 45):
        partial = review_raw_git(root, "config", "--get-regexp", r"^remote\..*\.promisor$|^extensions\.partialclone$",
                                 allowed_codes=(0, 1))
        if partial:
            raise ValueError("partial-clone evidence requires Git 2.45 or newer to disable lazy fetching")
    base = _pin(root, base, "commit")
    base_tree = _pin(root, base, "tree")
    object_format = review_raw_git(root, "rev-parse", "--show-object-format").decode("ascii").strip()
    shallow = review_raw_git(root, "rev-parse", "--is-shallow-repository").strip() == b"true"
    source_root = Path(checker_path).parent if checker_path else Path(__file__).parent
    sources = {name: hashlib.sha256((source_root / name).read_bytes()).hexdigest()
               for name in ("pr_overlap.py", "pr_overlap_evidence.py", "nid_names.py")}
    destination.mkdir()
    (destination / "objects").mkdir()
    packet = _Packet(destination, root)
    if shallow:
        packet.issue("shallow repository may hide original merge bases")
    stdout = packet.store(rendered.encode("utf-8"), "txt")
    local = review_revision(packet, base, prs[0])
    peers = []
    selected = []
    for number, pr in sorted(prs.items()):
        if not number:
            continue
        shared = set(pr.get("files", ())) & set(prs[0].get("files", ()))
        if number in findings or shared:
            selected.append((number, pr, shared))
    if len(selected) > MAX_PEERS:
        packet.issue("peer limit exceeded", total=len(selected))
    for number, pr, shared in selected[:MAX_PEERS]:
        found = findings.get(number, {})
        paths = shared if shared and not found.get("exports") else None
        peers.append({"number": number, "title": pr.get("title", ""), "author": pr.get("author"),
                      "url": f"https://github.com/{repo}/pull/{number}", "depends": sorted(pr.get("depends", ())),
                      "checker_state": "uncompared" if found.get("uncompared") else "findings" if found else "no_findings",
                      "findings": _json_value(found),
                      "selection": {"shared_files": sorted(shared), "checker_findings": bool(found),
                                    "original_paths": "all" if paths is None else "shared with fallback for normalized paths"},
                      "revision": review_revision(packet, base, pr, paths)})
    manifest = {"format": "anyps5-pr-overlap-review/1", "base": {"commit": base, "tree": base_tree},
                "local": local, "peers": peers,
                "gate": {"exit_code": 2 if any(found.get("uncompared") for found in findings.values()) else 0,
                         "findings": _json_value(findings), "main_exports": _json_value(main_exports), "stdout": stdout},
                "checker": {"sources": sources, "git_version": git_version, "object_format": object_format, "shallow": shallow},
                "complete": not packet.incomplete, "incomplete": packet.incomplete,
                "objects": sorted(packet.objects.values(), key=lambda item: item["path"]),
                "scan": {"pull_requests": len(prs) - 1, "selected_peers": len(selected)},
                "limits": {name: globals()[name] for name in ("MAX_ARTIFACT_BYTES", "MAX_PACKET_BYTES", "MAX_METADATA_BYTES",
                           "MAX_PEERS", "MAX_PATHS", "MAX_FILE_RECORDS", "MAX_MERGE_BASES", "MAX_MANIFEST_BYTES", "GIT_TIMEOUT")}}
    index = ["# Pinned PR overlap evidence", "", f"Base commit: `{base}`", f"Local commit: `{local['head']}`", "",
             f"Checker exit code: {manifest['gate']['exit_code']}", f"Evidence complete: {str(manifest['complete']).lower()}", "",
             "Read manifest.json for pinned identities, original and normalized views, selection rules and omissions.",
             "Original views compare every recorded merge base with the PR head. Normalized views compare stable trees after merging onto the pinned base.",
             "The checker gate and findings are unchanged. This packet does not certify compatibility or clear manual review.",
             "READY means writing finished; check complete and incomplete before relying on the evidence.",
             "Object filenames contain SHA256 hashes. Verify every object's byte count and SHA256 against manifest.json offline.", "",
             "| PR | Head | Checker state |", "|---|---|---|"]
    index.extend(f"| #{peer['number']} | `{peer['revision']['head']}` | {peer['checker_state']} |" for peer in peers)
    index_data = ("\n".join(index) + "\n").encode("utf-8")
    manifest_data = _json_bytes(manifest)
    if len(manifest_data) > MAX_MANIFEST_BYTES or len(index_data) > MAX_METADATA_BYTES:
        raise ValueError("review manifest or index byte limit exceeded")
    packet.write("index.md", index_data)
    packet.write("manifest.json", manifest_data)
    packet.write("READY", b"")
    return manifest
