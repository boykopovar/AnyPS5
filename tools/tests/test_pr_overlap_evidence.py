import base64
import contextlib
import copy
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from functools import cached_property
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap
import pr_overlap_evidence


class GitEvidenceTestCase(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.parent = Path(self.directory.name)
        self.root = self.parent / "repository"
        self.root.mkdir()
        self.previous = Path.cwd()
        self.environment = patch.dict(os.environ, {"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull,
                                                  "GIT_CONFIG_COUNT": "0"})
        self.environment.start()
        self.addCleanup(self.environment.stop)
        os.chdir(self.root)
        self.addCleanup(os.chdir, self.previous)
        self.addCleanup(pr_overlap._REPO_ROOTS.clear)
        pr_overlap._REPO_ROOTS.clear()
        self.git("init", "-q")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "core.safecrlf", "false")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.hooksPath", str(self.parent / "absent-hooks"))
        self.write("shared.txt", b"".join(f"line {number}\n".encode() for number in range(40)))
        self.write("other.txt", b"other\n")
        self.git("add", "-A")
        self.git("commit", "-qm", "base")
        self.base = self.git_text("rev-parse", "HEAD")

    @cached_property
    def fixture_local_head(self):
        return self.commit(self.base, "local", {"shared.txt": b"local\n"})

    @cached_property
    def fixture_peer_head(self):
        return self.commit(self.base, "peer", {"shared.txt": b"peer\n"})

    def git(self, *arguments, input=None):
        command = ["git", "-c", "user.name=Evidence Test", "-c", "user.email=test@example.invalid", *arguments]
        return subprocess.run(command, cwd=self.root, input=input, capture_output=True, check=True, timeout=10).stdout

    def git_text(self, *arguments):
        return self.git(*arguments).decode("utf-8").strip()

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def commit(self, parent, title, changes, remove=(), executable=()):
        self.git("checkout", "-q", "--detach", parent)
        for name, content in changes.items():
            self.write(name, content)
        for name in remove:
            (self.root / name).unlink()
        self.git("add", "-A")
        for name in executable:
            self.git("update-index", "--chmod=+x", "--", name)
        self.git("commit", "-qm", title)
        return self.git_text("rev-parse", "HEAD")

    def states(self, local=None, peer=None, base=None):
        base = base or self.base
        rows = {}
        for number, head in ((0, local or self.fixture_local_head), (7, peer or self.fixture_peer_head)):
            names = self.git("diff", "--name-only", "--no-renames", "-z", base, head).decode("utf-8", "surrogateescape").split("\0")
            rows[number] = {"ref": head, "sha": head, "tree": head, "files": set(filter(None, names)),
                            "added": set(), "exports": set(), "added_exports": {}, "removed_exports": {},
                            "author": None if number == 0 else "contributor", "title": "Peer title", "depends": set()}
        return rows

    def export(self, destination=None, prs=None, findings=None, base=None, rendered="Pinned review result\n",
               *, local=None, peer=None):
        destination = destination or self.parent / "review"
        base = base or self.base
        if prs is None:
            prs = self.states(local, peer, base)
        result = pr_overlap_evidence.write_review_evidence(
            destination, self.root, base, prs, findings or {}, {}, rendered,
            "owner/project", checker_path=Path(pr_overlap.__file__).resolve())
        return destination, result

    def original(self, revision, base=None):
        return next(view for view in revision["originals"] if view["base"] == (base or self.base))

    def changes(self, view):
        return {base64.b64decode(row["path"]["base64"]): row for row in view["files"]}

    def stored(self, directory, descriptor):
        self.assertIsNotNone(descriptor)
        path = directory / descriptor["path"]
        self.assertTrue(path.is_relative_to(directory))
        content = path.read_bytes()
        self.assertEqual(len(content), descriptor["bytes"])
        self.assertEqual(hashlib.sha256(content).hexdigest(), descriptor["sha256"])
        return content

    def assert_snapshot(self, directory, snapshot, revision, name):
        mode, kind, oid_and_name = self.git("ls-tree", revision, "--", ":(literal)" + name).split(b" ", 2)
        oid = oid_and_name.split(b"\t", 1)[0].decode()
        self.assertEqual(snapshot["mode"], mode.decode())
        self.assertEqual(snapshot["git_type"], kind.decode())
        self.assertEqual(snapshot["git_oid"], oid)
        self.assertEqual(snapshot["status"], "stored")
        self.assertEqual(self.stored(directory, snapshot["artifact"]), self.git("cat-file", "blob", oid))


class ByteEvidenceTests(GitEvidenceTestCase):
    def test_original_patch_and_blobs_keep_binary_non_utf8_crlf_and_literal_paths(self):
        names = ["literal [a] λ.txt", "binary.dat"]
        base = self.commit(self.base, "byte base", {names[0]: b"old \xff\r\nend\n", names[1]: b"\0old\xff"})
        local = self.commit(base, "byte local", {names[0]: b"new \x80\r\nend\n", names[1]: b"\0local\xfe"})
        peer = self.commit(base, "byte peer", {names[0]: b"peer \x81\r\nend\n", names[1]: b"\0peer\xfd"})
        directory, report = self.export(base=base, local=local, peer=peer)
        self.assertTrue(report["complete"], report["incomplete"])
        for revision, head in ((report["local"], local), (report["peers"][0]["revision"], peer)):
            self.assertEqual(revision["head"], head)
            changes = self.changes(self.original(revision, base))
            self.assertEqual(set(changes), {name.encode() for name in names})
            for name in names:
                row = changes[name.encode()]
                self.assertEqual(row["path"]["utf8"], name)
                self.assert_snapshot(directory, row["before"], base, name)
                self.assert_snapshot(directory, row["after"], head, name)
                expected = self.git("diff", "--no-renames", "--binary", "--full-index", "--no-ext-diff",
                                    "--no-textconv", "--no-color", "--unified=3", "--diff-algorithm=myers", "--no-indent-heuristic", "--src-prefix=a/", "--dst-prefix=b/", "--ignore-submodules=none", "--submodule=short", "--no-relative", "--inter-hunk-context=0", base, head, "--", ":(literal)" + name)
                self.assertEqual(row["patch"]["status"], "stored")
                self.assertEqual(self.stored(directory, row["patch"]["artifact"]), expected)

    def test_add_delete_rename_and_executable_modes_are_not_lost(self):
        base = self.commit(self.base, "path base", {"old.txt": b"same\n", "delete.txt": b"gone\n", "mode.sh": b"tool\n"})
        local = self.commit(base, "path local", {"new.txt": b"same\n", "added.txt": b"added\n", "mode.sh": b"tool\n"},
                            remove=("old.txt", "delete.txt"), executable=("mode.sh",))
        peer = self.commit(base, "path peer", {"old.txt": b"other\n"})
        directory, report = self.export(base=base, local=local, peer=peer)
        rows = self.changes(self.original(report["local"], base))
        self.assertEqual(set(rows), {b"old.txt", b"new.txt", b"delete.txt", b"added.txt", b"mode.sh"})
        for name in (b"old.txt", b"delete.txt"):
            self.assertEqual(rows[name]["after"]["status"], "absent")
            self.assert_snapshot(directory, rows[name]["before"], base, name.decode())
        for name in (b"new.txt", b"added.txt"):
            self.assertEqual(rows[name]["before"]["status"], "absent")
            self.assert_snapshot(directory, rows[name]["after"], local, name.decode())
        self.assertEqual(rows[b"mode.sh"]["before"]["mode"], "100644")
        self.assertEqual(rows[b"mode.sh"]["after"]["mode"], "100755")
        patch_bytes = self.stored(directory, rows[b"mode.sh"]["patch"]["artifact"])
        self.assertIn(b"old mode 100644", patch_bytes)
        self.assertIn(b"new mode 100755", patch_bytes)

    def test_symlink_blob_and_gitlink_are_recorded_without_materializing_targets(self):
        peer = self.fixture_peer_head
        self.git("checkout", "-q", "--detach", self.base)
        target = b"../../outside-target"
        blob = self.git("hash-object", "-w", "--stdin", input=target).decode().strip()
        self.git("update-index", "--add", "--cacheinfo", "120000," + blob + ",link")
        self.git("update-index", "--add", "--cacheinfo", "160000," + self.base + ",submodule")
        self.git("commit", "-qm", "special entries")
        local = self.git_text("rev-parse", "HEAD")
        self.git("config", "diff.ignoreSubmodules", "all")
        directory, report = self.export(local=local, peer=peer)
        rows = self.changes(self.original(report["local"]))
        self.assertEqual(rows[b"link"]["after"]["mode"], "120000")
        self.assertEqual(self.stored(directory, rows[b"link"]["after"]["artifact"]), target)
        self.assertEqual(rows[b"submodule"]["after"]["status"], "gitlink")
        self.assertEqual(rows[b"submodule"]["after"]["git_oid"], self.base)
        self.assertIsNone(rows[b"submodule"]["after"]["artifact"])
        self.assertFalse((self.root / "link").exists())
        self.assertFalse((directory / "link").exists())
        self.assertFalse((directory / "submodule").exists())

    def test_file_directory_transitions_preserve_legitimate_absent_file_snapshots(self):
        base = self.commit(self.base, "transition base", {"swapped": b"old file\n", "folder/child.txt": b"old child\n"})
        self.git("checkout", "-q", "--detach", base)
        (self.root / "swapped").unlink()
        (self.root / "folder/child.txt").unlink()
        (self.root / "folder").rmdir()
        self.write("swapped/child.txt", b"new child\n")
        self.write("folder", b"new file\n")
        self.git("add", "-A")
        self.git("commit", "-qm", "transition local")
        local = self.git_text("rev-parse", "HEAD")
        peer = self.commit(base, "transition peer", {"swapped": b"peer file\n"})
        directory, report = self.export(base=base, local=local, peer=peer)
        self.assertTrue(report["complete"], report["incomplete"])
        rows = self.changes(self.original(report["local"], base))
        self.assertEqual(rows[b"swapped"]["after"]["status"], "absent")
        self.assertEqual(rows[b"folder"]["before"]["status"], "absent")
        self.assert_snapshot(directory, rows[b"swapped/child.txt"]["after"], local, "swapped/child.txt")
        self.assert_snapshot(directory, rows[b"folder/child.txt"]["before"], base, "folder/child.txt")

    def test_raw_path_encoding_is_preserved_when_platform_cannot_represent_the_name(self):
        peer = self.fixture_peer_head
        self.git("checkout", "-q", "--detach", self.base)
        name = b"raw-\xff.txt"
        blob = self.git("hash-object", "-w", "--stdin", input=b"raw path content\n").strip()
        try:
            self.git("update-index", "-z", "--index-info", input=b"100644 " + blob + b"\t" + name + b"\0")
        except subprocess.CalledProcessError as error:
            self.skipTest("Git does not accept a raw-byte index name on this platform: " + error.stderr.decode("utf-8", "replace"))
        self.git("commit", "-qm", "raw filename")
        local = self.git_text("rev-parse", "HEAD")
        directory, report = self.export(local=local, peer=peer)
        row = self.changes(self.original(report["local"]))[name]
        self.assertIsNone(row["path"]["utf8"])
        if os.name == "nt":
            self.assertFalse(report["complete"])
            self.assertTrue(report["incomplete"])
            self.assertNotEqual(row["after"]["status"], "stored")
        else:
            self.assertTrue(report["complete"], report["incomplete"])
            self.assertEqual(self.stored(directory, row["after"]["artifact"]), b"raw path content\n")
        self.assertTrue((directory / "READY").exists())

    def test_complete_packet_can_be_verified_without_git_or_source_repository(self):
        directory, report = self.export()
        self.assertEqual(report["format"], "anyps5-pr-overlap-review/1")
        self.assertEqual((directory / "READY").read_bytes(), b"")
        self.assertEqual(json.loads((directory / "manifest.json").read_text(encoding="utf-8")), report)
        self.assertTrue((directory / "index.md").read_text(encoding="utf-8").strip())
        self.assertEqual(report["objects"], sorted(report["objects"], key=lambda item: item["path"]))
        self.assertEqual(len({item["path"] for item in report["objects"]}), len(report["objects"]))
        for descriptor in report["objects"]:
            self.stored(directory, descriptor)
            self.assertRegex(descriptor["path"], r"^objects/[0-9a-f]{64}\.(blob|patch|txt)$")
        self.assertEqual(self.stored(directory, report["gate"]["stdout"]), b"Pinned review result\n")
        for name, digest in report["checker"]["sources"].items():
            source = Path(pr_overlap.__file__).parent / name
            self.assertEqual(digest, hashlib.sha256(source.read_bytes()).hexdigest())


class HistoryEvidenceTests(GitEvidenceTestCase):
    def test_normalized_tree_is_distinct_from_original_and_synthetic_commit_is_not_serialized(self):
        base_text = b"".join(f"line {number}\n".encode() for number in range(40))
        main = self.commit(self.base, "main advances", {"shared.txt": b"main\n" + base_text})
        local = self.commit(self.base, "local bottom", {"shared.txt": base_text + b"local bottom\n"})
        peer = self.commit(self.base, "peer bottom", {"shared.txt": base_text + b"peer bottom\n"})
        self.git("checkout", "-q", "--detach", main)
        self.git("merge", "-q", "--no-ff", "-m", "normalized local", local)
        normalized = self.git_text("rev-parse", "HEAD")
        prs = self.states(local, peer)
        prs[0]["tree"] = normalized
        before = copy.deepcopy(prs)
        directory, report = self.export(prs=prs, base=main)
        self.assertEqual(prs, before)
        self.assertEqual(report["local"]["head"], local)
        self.assertEqual(report["local"]["head_tree"], self.git_text("rev-parse", local + "^{tree}"))
        self.assertEqual(report["local"]["normalized"]["tree"], self.git_text("rev-parse", normalized + "^{tree}"))
        self.assertNotIn(normalized, (directory / "manifest.json").read_text(encoding="utf-8"))
        raw = self.changes(self.original(report["local"]))[b"shared.txt"]
        normalized_row = self.changes(report["local"]["normalized"])[b"shared.txt"]
        original_patch = self.stored(directory, raw["patch"]["artifact"])
        normalized_patch = self.stored(directory, normalized_row["patch"]["artifact"])
        self.assertNotEqual(original_patch, normalized_patch)
        tree = self.git_text("rev-parse", normalized + "^{tree}")
        with patch.dict(os.environ, {"GIT_AUTHOR_DATE": "2001-01-01T00:00:00+00:00",
                                     "GIT_COMMITTER_DATE": "2001-01-01T00:00:00+00:00"}):
            replacement = self.git("commit-tree", tree, "-p", main, "-p", local,
                                   input=b"different synthetic metadata\n").decode().strip()
        self.assertNotEqual(replacement, normalized)
        prs[0]["tree"] = replacement
        second_directory, second = self.export(destination=self.parent / "second", prs=prs, base=main)
        self.assertNotIn(replacement, (second_directory / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(report, second)
        self.assertEqual((directory / "manifest.json").read_bytes(), (second_directory / "manifest.json").read_bytes())

    def test_all_criss_cross_merge_bases_have_original_views(self):
        left = self.commit(self.base, "left", {"left.txt": b"left\n"})
        right = self.commit(self.base, "right", {"right.txt": b"right\n"})
        self.git("checkout", "-q", "--detach", left)
        self.git("merge", "-q", "--no-ff", "-m", "main merge", right)
        main = self.git_text("rev-parse", "HEAD")
        self.git("checkout", "-q", "--detach", right)
        self.git("merge", "-q", "--no-ff", "-m", "opposite merge", left)
        opposite = self.git_text("rev-parse", "HEAD")
        local = self.commit(opposite, "local after cross", {"shared.txt": b"local cross\n"})
        peer = self.commit(opposite, "peer after cross", {"shared.txt": b"peer cross\n"})
        bases = sorted(self.git_text("merge-base", "--all", main, local).split())
        self.assertEqual(bases, sorted([left, right]))
        directory, report = self.export(base=main, local=local, peer=peer)
        for revision in (report["local"], report["peers"][0]["revision"]):
            self.assertEqual(revision["merge_bases"], bases)
            self.assertEqual([view["base"] for view in revision["originals"]], bases)
            self.assertEqual({view["head"] for view in revision["originals"]}, {revision["head"]})

    def test_unrelated_peer_is_not_archived_and_unavailable_normalization_stays_explicit(self):
        prs = self.states()
        unrelated = self.commit(self.base, "unrelated", {"other.txt": b"changed\n"})
        prs[8] = {**prs[7], "ref": unrelated, "sha": unrelated, "files": {"other.txt"}, "tree": None}
        prs[7]["tree"] = None
        directory, report = self.export(prs=prs, findings={7: {"uncompared": {"shared.txt"}}})
        self.assertEqual(report["scan"], {"pull_requests": 2, "selected_peers": 1})
        self.assertEqual([peer["number"] for peer in report["peers"]], [7])
        self.assertEqual(report["peers"][0]["revision"]["normalized"]["state"], "unavailable")
        self.assertEqual(report["gate"]["exit_code"], 2)
        self.assertTrue((directory / "READY").exists())

    def test_movable_refs_are_incomplete_and_pinned_heads_ignore_alias_drift(self):
        prs = self.states()
        self.git("update-ref", "refs/heads/movable", self.fixture_peer_head)
        prs[7]["ref"] = "refs/heads/movable"
        directory, incomplete = self.export(destination=self.parent / "bad", prs=prs)
        self.assertFalse(incomplete["complete"])
        self.assertIn("pinned", incomplete["peers"][0]["revision"]["error"])
        self.git("update-ref", "refs/heads/movable", self.fixture_local_head)
        directory, report = self.export(destination=self.parent / "pinned")
        self.assertEqual(report["peers"][0]["revision"]["head"], self.fixture_peer_head)
        self.assertTrue((directory / "READY").exists())


    def test_shared_path_selection_happens_before_peer_path_cap(self):
        local = self.commit(self.base, "last shared local", {"zz-shared.txt": b"local\n"})
        peer = self.commit(self.base, "many peer paths", {"aa.txt": b"aa\n", "bb.txt": b"bb\n", "zz-shared.txt": b"peer\n"})
        with patch.object(pr_overlap_evidence, "MAX_PATHS", 2):
            directory, report = self.export(local=local, peer=peer)
        self.assertTrue(report["complete"], report["incomplete"])
        self.assertEqual(set(self.changes(self.original(report["peers"][0]["revision"]))), {b"zz-shared.txt"})
        self.assertEqual(set(self.changes(report["peers"][0]["revision"]["normalized"])), {b"zz-shared.txt"})

    def test_missing_pinned_object_is_explicitly_incomplete(self):
        prs = self.states()
        prs[7].update(ref="f" * 40, sha="f" * 40, tree=None)
        directory, report = self.export(prs=prs)
        self.assertFalse(report["complete"])
        self.assertTrue(report["incomplete"])
        self.assertIn("error", report["peers"][0]["revision"])
        self.assertTrue((directory / "READY").exists())


class PublicationEvidenceTests(GitEvidenceTestCase):
    def test_preflight_is_read_only_and_refuses_existing_or_git_destinations(self):
        destination = self.parent / "review"
        pr_overlap_evidence.validate_review_directory(destination, self.root)
        self.assertFalse(destination.exists())
        existing = self.parent / "existing"
        existing.mkdir()
        (existing / "owned").write_bytes(b"keep")
        for rejected in (existing, self.parent / "missing" / "review", self.root / ".git" / "review"):
            with self.subTest(rejected=rejected), self.assertRaises((ValueError, OSError)):
                pr_overlap_evidence.validate_review_directory(rejected, self.root)
        self.assertEqual((existing / "owned").read_bytes(), b"keep")
        with self.assertRaises((ValueError, OSError)):
            self.export(destination=existing)
        self.assertEqual(list(existing.iterdir()), [existing / "owned"])

    def test_dangling_symlink_and_existing_completed_packet_are_never_reused(self):
        dangling = self.parent / "dangling"
        try:
            dangling.symlink_to(self.parent / "missing", target_is_directory=True)
        except OSError:
            dangling = None
        if dangling is not None:
            with self.assertRaises((ValueError, OSError)):
                pr_overlap_evidence.validate_review_directory(dangling, self.root)
        directory, report = self.export()
        before = {str(path.relative_to(directory)): path.read_bytes() for path in directory.rglob("*") if path.is_file()}
        with self.assertRaises((ValueError, OSError)):
            self.export(destination=directory)
        self.assertEqual(before, {str(path.relative_to(directory)): path.read_bytes() for path in directory.rglob("*") if path.is_file()})

    def test_artifact_and_inventory_caps_are_explicit_without_truncated_stored_objects(self):
        prs = self.states()
        for constant, value in (("MAX_ARTIFACT_BYTES", 64), ("MAX_FILE_RECORDS", 0),
                                ("MAX_PATHS", 0), ("MAX_PEERS", 0), ("MAX_MERGE_BASES", 0)):
            with self.subTest(constant=constant), patch.object(pr_overlap_evidence, constant, value):
                directory, report = self.export(destination=self.parent / constant, prs=prs)
                self.assertFalse(report["complete"])
                self.assertTrue(report["incomplete"])
                self.assertTrue((directory / "READY").exists())
                for descriptor in report["objects"]:
                    content = self.stored(directory, descriptor)
                    if constant == "MAX_ARTIFACT_BYTES":
                        self.assertLessEqual(len(content), value)

    def test_packet_and_metadata_caps_never_publish_ready_when_required_metadata_cannot_fit(self):
        prs = self.states()
        for constant, value in (("MAX_PACKET_BYTES", 32), ("MAX_MANIFEST_BYTES", 32), ("MAX_METADATA_BYTES", 8)):
            directory = self.parent / constant
            with self.subTest(constant=constant), patch.object(pr_overlap_evidence, constant, value):
                with self.assertRaises((ValueError, OSError)):
                    self.export(destination=directory, prs=prs)
                self.assertFalse((directory / "READY").exists())
                if constant == "MAX_PACKET_BYTES" and directory.exists():
                    self.assertLessEqual(sum(path.stat().st_size for path in directory.rglob("*") if path.is_file()), value)

    def test_binary_subprocess_timeout_and_output_limit_do_not_return_partial_bytes(self):
        original = subprocess.Popen
        for program, timeout, message in (("import time; time.sleep(5)", 0.1, "exceeded"),
                                          ("import sys; sys.stdout.buffer.write(b'x' * 1000000)", 5, "output exceeds 16")):
            def child(command, *args, **kwargs):
                self.assertEqual(command[0], "git")
                return original([sys.executable, "-c", program], *args, **kwargs)
            started = time.monotonic()
            with self.subTest(program=program), patch.object(subprocess, "Popen", side_effect=child), \
                    patch.object(pr_overlap_evidence, "GIT_TIMEOUT", timeout):
                with self.assertRaisesRegex(ValueError, message):
                    pr_overlap_evidence.review_raw_git(self.root, "--version", limit=16)
            self.assertLess(time.monotonic() - started, 3)

    def test_midway_write_failure_never_creates_ready(self):
        original = Path.open
        def failing_open(path, mode="r", *args, **kwargs):
            if path.name == "index.md" and any(flag in mode for flag in ("w", "x", "a")):
                raise OSError("synthetic index write failure")
            return original(path, mode, *args, **kwargs)
        destination = self.parent / "unfinished"
        with patch.object(Path, "open", failing_open), self.assertRaises((ValueError, OSError)):
            self.export(destination=destination)
        self.assertFalse((destination / "READY").exists())
        if destination.exists():
            with self.assertRaises((ValueError, OSError)):
                self.export(destination=destination)

    def test_external_diff_and_textconv_are_disabled_and_worktree_is_unchanged(self):
        prs = self.states()
        sentinel = self.parent / "EXECUTED"
        command = f'"{sys.executable}" -c "open(r\'{sentinel.as_posix()}\', \'w\').write(\'bad\')"'
        self.git("config", "diff.external", command)
        self.git("config", "diff.trap.textconv", command)
        self.write(".gitattributes", b"*.txt diff=trap\n")
        before = self.git("status", "--porcelain=v1", "-z")
        commands = []
        original = subprocess.Popen
        def record(command, *args, **kwargs):
            commands.append(command)
            self.assertFalse(kwargs.get("shell", False))
            return original(command, *args, **kwargs)
        with patch.object(subprocess, "Popen", side_effect=record):
            directory, report = self.export(prs=prs)
            with patch.dict(os.environ, {"GIT_DIFF_OPTS": "--unified=0"}):
                overridden_directory, overridden = self.export(destination=self.parent / "env-override", prs=prs)
        self.assertTrue(report["complete"], report["incomplete"])
        self.assertTrue(overridden["complete"], overridden["incomplete"])
        original_patches = {item["path"]: self.stored(directory, item) for item in report["objects"] if item["kind"] == "patch"}
        override_patches = {item["path"]: self.stored(overridden_directory, item) for item in overridden["objects"] if item["kind"] == "patch"}
        self.assertTrue(original_patches)
        self.assertEqual(original_patches, override_patches)
        self.assertFalse(sentinel.exists())
        for command in commands:
            self.assertEqual(command[0], "git")
            self.assertFalse({"fetch", "checkout", "commit", "merge", "apply", "update-ref"} & set(command))
        self.assertEqual(self.git("status", "--porcelain=v1", "-z"), before)
        self.assertFalse((directory / ".gitattributes").exists())
        self.assertEqual(self.git_text("rev-parse", "HEAD"), self.fixture_peer_head)


class CliEvidenceTests(GitEvidenceTestCase):
    def invoke(self, arguments, findings=None, export_failure=None):
        prs = self.states()
        peers = {number: row for number, row in prs.items() if number}
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(pr_overlap, "open_prs", return_value=copy.deepcopy(peers)))
            stack.enter_context(patch.object(pr_overlap, "base_commit", return_value=self.base))
            stack.enter_context(patch.object(pr_overlap, "fetch_heads", return_value=self.base))
            stack.enter_context(patch.object(pr_overlap, "own", return_value=copy.deepcopy(peers)))
            def scan(rows, jobs):
                rows[0].update(copy.deepcopy(prs[0]))
            stack.enter_context(patch.object(pr_overlap, "scan_all", side_effect=scan))
            stack.enter_context(patch.object(pr_overlap, "base_exports", return_value={}))
            stack.enter_context(patch.object(pr_overlap, "main_export_findings", return_value={}))
            stack.enter_context(patch.object(pr_overlap, "overlaps", return_value=findings))
            stack.enter_context(patch.object(pr_overlap, "report", return_value="checker result\n"))
            if export_failure is not None:
                stack.enter_context(patch.object(pr_overlap_evidence, "write_review_evidence", side_effect=export_failure))
            stack.enter_context(contextlib.redirect_stdout(stdout))
            stack.enter_context(contextlib.redirect_stderr(stderr))
            try:
                code = pr_overlap.main(["--repo", "owner/project", "--branch", self.fixture_local_head, *arguments])
            except SystemExit as error:
                code = error.code
        return code, stdout.getvalue(), stderr.getvalue()

    def test_option_absent_and_complete_export_keep_stdout_and_gate_outcomes(self):
        for gate, findings in ((0, None), (2, {"uncompared": {"shared.txt"}})):
            with self.subTest(gate=gate):
                plain = self.invoke([], findings)
                directory = self.parent / f"gate-{gate}"
                exported = self.invoke(["--review-dir", str(directory)], findings)
                self.assertEqual(plain[0], gate)
                self.assertEqual(exported[0], gate)
                self.assertEqual(plain[1], exported[1])
                self.assertEqual(plain[1], "checker result\n")
                report = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(report["gate"]["exit_code"], gate)
                self.assertTrue((directory / "READY").exists())

    def test_invalid_option_usage_and_output_are_rejected_before_network(self):
        destination = self.parent / "existing"
        destination.mkdir()
        for args in (["--names", "name", "--review-dir", str(self.parent / "review")],
                     ["--out", str(self.parent / "out"), "--review-dir", str(self.parent / "review")],
                     ["--branch", self.fixture_local_head, "--review-dir", str(destination)]):
            with self.subTest(args=args), patch.object(pr_overlap, "open_prs") as network, \
                    contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    pr_overlap.main(["--repo", "owner/project", *args])
                network.assert_not_called()

    def test_incomplete_export_never_turns_an_unresolved_gate_green(self):
        for gate, findings in ((0, None), (2, {"uncompared": {"shared.txt"}})):
            with self.subTest(gate=gate), patch.object(pr_overlap_evidence, "MAX_ARTIFACT_BYTES", 64):
                directory = self.parent / f"limited-{gate}"
                code, stdout, stderr = self.invoke(["--review-dir", str(directory)], findings)
                self.assertEqual(code, 2)
                report = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(report["gate"]["exit_code"], gate)
                self.assertFalse(report["complete"])
                self.assertTrue((directory / "READY").exists())

    def test_export_write_failure_keeps_gate_failure_and_never_publishes_ready(self):
        for gate, findings in ((0, None), (2, {"uncompared": {"shared.txt"}})):
            with self.subTest(gate=gate):
                directory = self.parent / f"failed-{gate}"
                code, stdout, stderr = self.invoke(["--review-dir", str(directory)], findings,
                                                   OSError("synthetic write failure"))
                self.assertEqual(code, 1 if gate == 0 else 2)
                self.assertEqual(stdout, "checker result\n")
                self.assertFalse((directory / "READY").exists())
                self.assertIn("synthetic write failure", stderr)


if __name__ == "__main__":
    unittest.main()
