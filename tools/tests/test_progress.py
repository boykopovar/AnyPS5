import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


def excessive_nesting():
    depth = sys.getrecursionlimit() + 64
    for _ in range(8):
        try:
            json.loads("[" * depth + "]" * depth)
        except RecursionError:
            return "[" * (depth + 128) + "]" * (depth + 128)
        depth *= 2
    raise AssertionError("JSON decoder accepted every bounded nesting probe")


class ProgressSnapshotTests(unittest.TestCase):
    def test_library_counters_reject_invalid_types_and_negative_values(self):
        for field in ("done", "todo"):
            for value in (None, True, False, "1", 1.5, [], {}, -1, float("nan"), float("inf")):
                with self.subTest(field=field, value=value):
                    snapshot = self.snapshot()
                    snapshot["libraries"]["groups"] = [{"name": "libExample", "done": 0, "todo": 0, field: value}]
                    self.assert_rejected(snapshot, f"libraries.groups[0].{field} must be a non-negative integer")

    def test_library_counter_validation_preserves_optional_and_large_counts(self):
        groups = [{"name": "libLegacy"}, {"name": "libHalf", "done": 1},
                  {"name": "libShared", "shared_sources": "libExample"},
                  {"name": "libEmpty", "done": 0, "todo": 0},
                  {"name": "libLarge", "done": 10**200, "todo": 10**200}]
        snapshot = self.snapshot()
        snapshot["libraries"]["groups"] = groups
        with tempfile.TemporaryDirectory() as directory:
            before, after = Path(directory) / "before.json", Path(directory) / "after.json"
            before.write_text(json.dumps(self.snapshot()), encoding="utf-8")
            after.write_text(json.dumps(snapshot), encoding="utf-8")
            output = io.StringIO()
            with patch("sys.stdout", output):
                self.assertEqual(progress.main(["--compare", str(before), str(after)]), 0)
            self.assertIn("Counts unavailable", output.getvalue())
            self.assertIn("Shared sources", output.getvalue())
            self.assertIn("No exports", output.getvalue())
            self.assertIn("50.00%", output.getvalue())

    def snapshot(self):
        return {"libraries": {"percent": 0, "groups": []}, "shaders": {"percent": 0, "groups": []}}

    def assert_accepted(self, snapshot):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / "base.json", Path(directory) / "head.json"]
            paths[0].write_text(json.dumps(self.snapshot()), encoding="utf-8")
            paths[1].write_text(json.dumps(snapshot), encoding="utf-8")
            with patch("sys.stdout", io.StringIO()):
                self.assertEqual(progress.main(["--compare", str(paths[0]), str(paths[1])]), 0)

    def assert_rejected(self, snapshot, message):
        for position in (0, 1):
            with self.subTest(position=position), tempfile.TemporaryDirectory() as directory:
                paths = [Path(directory) / "base snapshot.json", Path(directory) / "head snapshot.json"]
                snapshots = [self.snapshot(), self.snapshot()]
                snapshots[position] = snapshot
                for path, data in zip(paths, snapshots):
                    path.write_text(json.dumps(data), encoding="utf-8")
                stdout, stderr = io.StringIO(), io.StringIO()
                with patch("sys.stdout", stdout), patch("sys.stderr", stderr):
                    with self.assertRaises(SystemExit) as raised:
                        progress.main(["--compare", *map(str, paths)])
                self.assertEqual(raised.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn(str(paths[position]), stderr.getvalue())
                self.assertIn(message, stderr.getvalue())
                self.assertNotIn("Traceback", stderr.getvalue())

    def test_deeply_nested_snapshots_are_usage_errors(self):
        for position in (0, 1):
            with self.subTest(position=position), tempfile.TemporaryDirectory() as directory:
                paths = [Path(directory) / "base snapshot.json", Path(directory) / "head snapshot.json"]
                for path in paths:
                    path.write_text(json.dumps(self.snapshot()), encoding="utf-8")
                paths[position].write_text(excessive_nesting(), encoding="utf-8")
                stdout, stderr = io.StringIO(), io.StringIO()
                with patch("sys.stdout", stdout), patch("sys.stderr", stderr):
                    with self.assertRaises(SystemExit) as raised:
                        progress.main(["--compare", *map(str, paths)])
                self.assertEqual(raised.exception.code, 2)
                self.assertEqual(stdout.getvalue(), "")
                self.assertIn(str(paths[position]), stderr.getvalue())
                self.assertNotIn("must be an object", stderr.getvalue())
                self.assertNotIn("Traceback", stderr.getvalue())

    def test_non_object_snapshots_are_usage_errors(self):
        for snapshot in (None, [], "progress", 7, True):
            with self.subTest(snapshot=snapshot):
                self.assert_rejected(snapshot, "snapshot must be an object")

    def test_sections_must_be_present_objects(self):
        for section in ("libraries", "shaders"):
            for value in (None, [], "progress", 7):
                with self.subTest(section=section, value=value):
                    snapshot = self.snapshot()
                    snapshot[section] = value
                    self.assert_rejected(snapshot, section + " must be an object")
            snapshot = self.snapshot()
            del snapshot[section]
            self.assert_rejected(snapshot, section + " must be an object")

    def test_groups_must_be_lists_of_named_objects(self):
        for section in ("libraries", "shaders"):
            for groups in (None, {}, "groups", [None], ["group"], [{}], [{"name": []}]):
                with self.subTest(section=section, groups=groups):
                    snapshot = self.snapshot()
                    snapshot[section]["groups"] = groups
                    self.assert_rejected(snapshot, section + ".groups")
            snapshot = self.snapshot()
            del snapshot[section]["groups"]
            self.assert_rejected(snapshot, section + ".groups")

    def test_symbol_lists_cannot_be_strings_or_unhashable_values(self):
        for field in ("done_names", "todo_names", "inert_names"):
            for value in ("Ready", None, {}, [7], [["Ready"]]):
                with self.subTest(field=field, value=value):
                    snapshot = self.snapshot()
                    snapshot["libraries"]["groups"] = [{"name": "libSceExample", field: value}]
                    self.assert_rejected(snapshot, "." + field + " must be a list of strings")

    def test_shared_source_owners_must_be_strings(self):
        for value in (None, [], {}, 7, True):
            with self.subTest(value=value):
                snapshot = self.snapshot()
                snapshot["libraries"]["groups"] = [{"name": "libSceExample.native", "shared_sources": value}]
                self.assert_rejected(snapshot, ".shared_sources must be a string")

    def test_percentages_must_be_finite_numbers_in_range(self):
        for section in ("libraries", "shaders"):
            for value in (None, "0", True, [], {}, -1, 101, float("nan"), float("inf"), -float("inf")):
                with self.subTest(section=section, value=value):
                    snapshot = self.snapshot()
                    snapshot[section]["percent"] = value
                    self.assert_rejected(snapshot, section + ".percent")
            snapshot = self.snapshot()
            del snapshot[section]["percent"]
            self.assert_rejected(snapshot, section + ".percent")

    def test_observed_fields_are_validated_and_optional(self):
        for field, value in (("unobserved", -1), ("unobserved", "1"), ("unobserved", True), ("classified", 1),
                             ("unobserved_names", "Pending"), ("done_unobserved_names", [1])):
            with self.subTest(field=field, value=value):
                snapshot = self.snapshot()
                snapshot["libraries"]["groups"] = [{"name": "libSceExample", field: value}]
                self.assert_rejected(snapshot, f"libraries.groups[0].{field}")
        snapshot = self.snapshot()
        snapshot["libraries"]["declared_percent"] = 101
        self.assert_rejected(snapshot, "libraries.declared_percent")
        snapshot = self.snapshot()
        snapshot["libraries"]["groups"] = [{"name": "libSceExample", "classified": True, "unobserved": 2,
                                            "unobserved_names": ["A", "B"], "done_unobserved_names": []}]
        snapshot["libraries"]["declared_percent"] = 50
        self.assert_accepted(snapshot)

    def test_valid_snapshots_keep_reports_and_optional_fields_compatible(self):
        base, head = self.snapshot(), self.snapshot()
        base["libraries"]["groups"] = [{"name": "libSceExample", "todo_names": ["Ready"]}, {"name": "libEmpty"}]
        head["libraries"]["groups"] = [{"name": "libSceExample", "done_names": ["Ready"], "future_field": True},
                                       {"name": "libEmpty"}, {"name": "libSceExample.native", "shared_sources": "libSceExample"}]
        head["libraries"]["percent"] = 100
        expected = progress.report(base, head)
        self.assertIn("Ready", expected)
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / "base.json", Path(directory) / "head.json"]
            for path, snapshot in zip(paths, (base, head)):
                path.write_text(json.dumps(snapshot), encoding="utf-8")
            stdout = io.StringIO()
            with patch("sys.stdout", stdout):
                self.assertEqual(progress.main(["--compare", *map(str, paths)]), 0)
            self.assertEqual(stdout.getvalue(), expected)
            with patch("sys.stdout", stdout):
                self.assertEqual(progress.main(["--compare", str(paths[0]), str(paths[0])]), 0)
            self.assertEqual(stdout.getvalue(), expected)


    def test_duplicate_names_are_rejected_within_each_section(self):
        for section in ("libraries", "shaders"):
            with self.subTest(section=section):
                snapshot = self.snapshot()
                snapshot[section]["groups"] = [{"name": "Repeated"}, {"name": "other"}, {"name": "Repeated"}]
                self.assert_rejected(snapshot, section + ".groups[2].name duplicates 'Repeated'")

    def test_group_names_are_case_sensitive_and_local_to_each_section(self):
        snapshot = self.snapshot()
        for section in ("libraries", "shaders"):
            snapshot[section]["groups"] = [{"name": "same"}, {"name": "Same"}]
        progress.validate_snapshot(snapshot)


class ObservedManifestTests(unittest.TestCase):
    def read(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "libSceExample.txt"
            path.write_text(text, encoding="utf-8")
            return progress.read_manifest(path)

    def test_reads_nids_and_ignores_comments_and_evidence(self):
        observed = self.read("# Observed NIDs.\n\nAbCdEfGhIjK\nZyXwVuTsRqP  # evidence: resolved by name at run time\n")
        self.assertEqual(observed, frozenset({"AbCdEfGhIjK", "ZyXwVuTsRqP"}))

    def test_empty_manifest_is_a_classification_with_no_observations(self):
        self.assertEqual(self.read("# nothing observed yet\n"), frozenset())

    def test_malformed_entries_are_rejected_with_their_line(self):
        for line in ("short", "TooLongForANid1", "AbCdEfGhIj!", "AbCdEfGhIjK extra", "AbCdEfGhI K"):
            with self.subTest(line=line), self.assertRaisesRegex(ValueError, rf":2: invalid NID {re.escape(repr(line))}"):
                self.read("# header\n" + line + "\n")

    def test_problems_name_malformed_duplicate_and_unsorted_entries(self):
        text = "# header\nBbBbBbBbBbB\nAaAaAaAaAaA\nBbBbBbBbBbB\nshort\nCcCcCcCcCcC  # evidence: by name\n"
        self.assertEqual(progress.manifest_problems(text), [
            (3, "AaAaAaAaAaA is not in sorted order after BbBbBbBbBbB"),
            (4, "duplicate NID BbBbBbBbBbB"),
            (5, "invalid NID 'short'")])

    def test_problems_accept_sorted_unique_entries_with_comments(self):
        self.assertEqual(progress.manifest_problems("# header\nAaAaAaAaAaA\n\nBbBbBbBbBbB  # evidence: by name\n"), [])
        self.assertEqual(progress.manifest_problems(""), [])


class ProvenanceTagTests(unittest.TestCase):
    def test_tags_are_read_with_their_details(self):
        text = ("# header\nAaAaAaAaAaA  # audit: import_audit 2026-10-31\nBbBbBbBbBbB  # loader\n"
                "CcCcCcCcCcC  # manual: resolved by name with sceKernelDlsym\nDdDdDdDdDdD\nEeEeEeEeEeE  # seen in a trace\n")
        self.assertEqual([(entry, tag, detail) for _, entry, tag, detail, _ in progress.manifest_tags(text)], [
            ("AaAaAaAaAaA", "audit", "import_audit 2026-10-31"), ("BbBbBbBbBbB", "loader", ""),
            ("CcCcCcCcCcC", "manual", "resolved by name with sceKernelDlsym"), ("DdDdDdDdDdD", "untagged", ""),
            ("EeEeEeEeEeE", "untagged", "")])
        self.assertEqual(progress.manifest_tag_problems(text), [])
        self.assertEqual(progress.manifest_untagged(text), [(5, "DdDdDdDdDdD"), (6, "EeEeEeEeEeE")])

    def test_unknown_tags_and_incomplete_details_are_problems(self):
        text = ("AaAaAaAaAaA  # evidence: by name\nBbBbBbBbBbB  # audit: some title 2026-10-31\n"
                "CcCcCcCcCcC  # manual\nDdDdDdDdDdD  # audit: audit_dump 2026-10-31\n")
        problems = dict(progress.manifest_tag_problems(text))
        self.assertEqual(sorted(problems), [1, 2, 3])
        self.assertIn("unknown provenance tag 'evidence'", problems[1])
        self.assertIn("audit needs the tool and the date", problems[2])
        self.assertIn("manual needs a written reason", problems[3])

    def test_read_provenance_maps_each_nid_to_its_first_tag(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "libSceExample.txt"
            path.write_text("AaAaAaAaAaA  # loader\nBbBbBbBbBbB\nAaAaAaAaAaA  # manual: later\n", encoding="utf-8")
            self.assertEqual(progress.read_provenance(path), {"AaAaAaAaAaA": "loader", "BbBbBbBbBbB": "untagged"})


class OriginFlagTests(unittest.TestCase):
    def make_repository(self, root):
        library = root / "core" / "libs" / "prx" / "libSceA"
        library.mkdir(parents=True)
        (library / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\nint APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n", encoding="utf-8")
        env = {**os.environ, "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "safe.directory", "GIT_CONFIG_VALUE_0": str(root),
               "GIT_AUTHOR_DATE": "2026-02-03T12:00:00+0000", "GIT_COMMITTER_DATE": "2026-02-03T12:00:00+0000"}
        for arguments in (["init", "-q"], ["add", "."], ["commit", "-qm", "feat: add"]):
            subprocess.run(["git", "-c", "user.name=T", "-c", "user.email=t@example.com", "-c", "commit.gpgsign=false", "-C", str(root), *arguments],
                           check=True, capture_output=True, env=env)
        return subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], check=True, capture_output=True, text=True, env=env).stdout.strip()

    def run_json(self, root, *flags):
        output = io.StringIO()
        with patch.object(progress, "collect_shaders", return_value=progress.summarize([])), patch("sys.stdout", output):
            code = progress.main(["--root", str(root), "--json", *flags])
        return code, output.getvalue()

    def test_origins_are_added_only_when_asked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sha = self.make_repository(root)
            plain = json.loads(self.run_json(root)[1])["libraries"]["groups"][0]
            asked = json.loads(self.run_json(root, "--origins")[1])["libraries"]["groups"][0]
        self.assertNotIn("origins", plain)
        self.assertEqual(asked["origins"], {"Pending": {"commit": sha, "date": "2026-02-03"}, "Ready": {"commit": sha, "date": "2026-02-03"}})
        self.assertEqual({key: value for key, value in asked.items() if key != "origins"}, plain)

    def test_origins_need_a_git_work_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "core" / "libs" / "prx" / "libSceA").mkdir(parents=True)
            (root / "core" / "libs" / "prx" / "libSceA" / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n", encoding="utf-8")
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                self.run_json(root, "--origins")
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("--origins needs a git work tree", stderr.getvalue())


class InventorySourceTests(unittest.TestCase):
    def scan(self, listing):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI A() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI B() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI Done() { return 1; }\n", encoding="utf-8")
            path = Path(directory) / "libSceExample.txt"
            path.write_text(listing, encoding="utf-8")
            return progress.scan_library(library, None, None, progress.read_inventory(path))

    def test_each_listed_stub_records_its_inventory_source(self):
        nid = progress.tokenizer.compute_nid
        group = self.scan(f"# header\n{nid('A')}  Firmware-3.20_Libs\n{nid('Done')}  Firmware-3.20_Libs\n")
        self.assertEqual(group["inventory"], {"A": "Firmware-3.20_Libs"})

    def test_no_listing_adds_no_field(self):
        self.assertNotIn("inventory", self.scan(""))

    def test_problems_name_malformed_duplicate_and_unsorted_lines(self):
        text = "BbBbBbBbBbB src\nAaAaAaAaAaA src\nBbBbBbBbBbB src\nCcCcCcCcCcC\nDdDdDdDdDdD bad source\nEeEeEeEeEeE no/slash\n"
        problems = dict(progress.inventory_problems(text))
        self.assertEqual(sorted(problems), [2, 3, 4, 5, 6])
        self.assertIn("not in sorted order", problems[2])
        self.assertIn("duplicate NID", problems[3])
        self.assertEqual(progress.inventory_problems("# header\nAaAaAaAaAaA src-1.0\n\nBbBbBbBbBbB src_2+x\n"), [])

    def test_a_malformed_listing_is_an_error_naming_the_line(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "libSceExample.txt"
            path.write_text("short src\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, ":1: expected"):
                progress.read_inventory(path)

    def test_root_reads_the_listings_of_that_tree_and_restores_the_location(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "core" / "libs" / "prx" / "libSceA"
            library.mkdir(parents=True)
            (library / "Export.cpp").write_text("int APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n", encoding="utf-8")
            (root / "docs" / "dev" / "inventory").mkdir(parents=True)
            (root / "docs" / "dev" / "inventory" / "libSceA.txt").write_text(f"{progress.tokenizer.compute_nid('Pending')} Listing-1\n", encoding="utf-8")
            before = progress.INVENTORY
            output = io.StringIO()
            with patch.object(progress, "collect_shaders", return_value=progress.summarize([])), patch("sys.stdout", output):
                self.assertEqual(progress.main(["--root", str(root), "--json"]), 0)
            self.assertEqual(json.loads(output.getvalue())["libraries"]["groups"][0]["inventory"], {"Pending": "Listing-1"})
            self.assertEqual(progress.INVENTORY, before)


class ObservedTagScanTests(unittest.TestCase):
    def scan(self, manifest):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI A() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI B() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI C() { NotImplemented_nid_no_patch(__func__); }\n", encoding="utf-8")
            path = Path(directory) / "libSceExample.txt"
            path.write_text(manifest, encoding="utf-8")
            return progress.scan_library(library, progress.read_manifest(path), progress.read_provenance(path))

    def test_each_observed_stub_carries_the_tag_of_its_nid(self):
        nid = progress.tokenizer.compute_nid
        group = self.scan(f"{nid('A')}  # loader\n{nid('B')}\n")
        self.assertEqual(group["observed_tags"], {"A": "loader", "B": "untagged"})
        self.assertEqual(group["unobserved_names"], ["C"])

    def test_a_library_without_a_manifest_has_no_tags(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text("int APS5_VABI A() { NotImplemented_nid_no_patch(__func__); }\n", encoding="utf-8")
            self.assertNotIn("observed_tags", progress.scan_library(library))

    def test_summary_counts_observed_stubs_by_tag_only_when_there_are_tags(self):
        groups = [{"name": "libA", "label": "A", "done": 0, "todo": 3, "observed_tags": {"X": "loader", "Y": "audit", "Z": "loader"}}]
        self.assertEqual(progress.summarize(groups)["observed_by_tag"], {"audit": 1, "loader": 2})
        self.assertNotIn("observed_by_tag", progress.summarize([{"name": "libA", "label": "A", "done": 0, "todo": 1}]))

    def test_snapshot_validation_checks_the_traceability_fields(self):
        def snapshot(group):
            return {"libraries": {"percent": 0, "groups": [dict({"name": "libA"}, **group)]}, "shaders": {"percent": 0, "groups": []}}
        progress.validate_snapshot(snapshot({"observed_tags": {"X": "loader"}, "origins": {"X": {"commit": "a"}}, "inventory": {"X": "s"}}))
        for field, value in (("observed_tags", ["x"]), ("observed_tags", {"X": 1}), ("origins", [1]), ("inventory", "s")):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                progress.validate_snapshot(snapshot({field: value}))


class ObservedStubTests(unittest.TestCase):
    def scan(self, source, observed=None):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(source, encoding="utf-8")
            return progress.scan_library(library) if observed is None else progress.scan_library(library, observed)

    def test_observed_stubs_are_the_pending_functions_a_manifest_names(self):
        source = ("int APS5_VABI Ready() { return 0; }\n"
                  "int APS5_VABI Imported() { NotImplemented_nid_no_patch(__func__); }\n"
                  "int APS5_VABI Unseen() { NotImplemented_nid_no_patch(__func__); }\n")
        observed = frozenset({progress.tokenizer.compute_nid("Imported")})
        group = self.scan(source, observed)
        self.assertTrue(group["classified"])
        self.assertEqual((group["done"], group["todo"], group["unobserved"]), (1, 1, 1))
        self.assertEqual(group["done_names"], ["Ready"])
        self.assertEqual(group["todo_names"], ["Imported"])
        self.assertEqual(group["unobserved_names"], ["Unseen"])

    def test_an_empty_manifest_classifies_every_stub_as_unobserved(self):
        source = "int APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n"
        group = self.scan(source, frozenset())
        self.assertTrue(group["classified"])
        self.assertEqual((group["todo"], group["unobserved"]), (0, 1))
        self.assertEqual(group["unobserved_names"], ["Pending"])

    def test_a_library_without_a_manifest_keeps_every_stub_observed(self):
        source = ("int APS5_VABI First() { NotImplemented_nid_no_patch(__func__); }\n"
                  "int APS5_VABI Second() { NotImplemented_nid_no_patch(__func__); }\n")
        group = self.scan(source)
        self.assertFalse(group["classified"])
        self.assertEqual((group["todo"], group["unobserved"]), (2, 0))
        self.assertEqual(group["todo_names"], ["First", "Second"])
        self.assertEqual(group["unobserved_names"], [])

    def test_implemented_functions_without_an_observed_nid_are_listed(self):
        source = ("int APS5_VABI Ready() { return 0; }\n"
                  "int APS5_VABI Imported() { return 0; }\n")
        observed = frozenset({progress.tokenizer.compute_nid("Imported")})
        group = self.scan(source, observed)
        self.assertEqual(group["done_names"], ["Imported", "Ready"])
        self.assertEqual(group["done_unobserved_names"], ["Ready"])
        self.assertNotIn("done_unobserved_names", self.scan(source))

    def test_summary_keeps_the_declared_figure_next_to_the_headline(self):
        source = ("int APS5_VABI Ready() { return 0; }\n"
                  "int APS5_VABI Imported() { NotImplemented_nid_no_patch(__func__); }\n"
                  "int APS5_VABI Unseen() { NotImplemented_nid_no_patch(__func__); }\n")
        observed = frozenset({progress.tokenizer.compute_nid("Imported")})
        data = progress.summarize([self.scan(source, observed), self.scan(source)])
        self.assertEqual((data["done"], data["total"], data["percent"]), (2, 5, 40.0))
        self.assertEqual((data["unobserved"], data["declared_total"], data["declared_percent"]), (1, 6, 33.33))

    def test_an_alias_export_is_observed_under_its_nid(self):
        alias = "AbCdEfGhIjK"
        source = (f'APS5_EXPORT("{alias}", Renamed);\n'
                  "int APS5_VABI Renamed() { NotImplemented_nid_no_patch(__func__); }\n")
        self.assertEqual(self.scan(source, frozenset({alias}))["todo_names"], ["Renamed"])
        group = self.scan(source, frozenset({progress.tokenizer.compute_nid("Renamed")}))
        self.assertEqual((group["todo"], group["unobserved"]), (0, 1))


class ManifestLoadingTests(unittest.TestCase):
    def make_tree(self, root):
        prx = root / "core" / "libs" / "prx"
        (prx / "libSceA").mkdir(parents=True)
        (prx / "libSceA" / "Export.cpp").write_text(
            "int APS5_VABI Ready() { return 0; }\n"
            "int APS5_VABI Imported() { NotImplemented_nid_no_patch(__func__); }\n"
            "int APS5_VABI Unseen() { NotImplemented_nid_no_patch(__func__); }\n")
        (prx / "libSceB").mkdir()
        (prx / "libSceB" / "Export.cpp").write_text("int APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n")
        observed = root / "docs" / "dev" / "observed"
        observed.mkdir(parents=True)
        (observed / "libSceA.txt").write_text(f"# observed\n{progress.tokenizer.compute_nid('Imported')}\n", encoding="utf-8")
        return prx, observed

    def test_each_library_is_classified_by_its_own_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prx, observed = self.make_tree(root)
            with patch.object(progress, "PRX", prx), patch.object(progress, "OBSERVED", observed):
                data = progress.collect_libraries()
            groups = {group["name"]: group for group in data["groups"]}
            self.assertTrue(groups["libSceA"]["classified"])
            self.assertEqual((groups["libSceA"]["todo_names"], groups["libSceA"]["unobserved_names"]), (["Imported"], ["Unseen"]))
            self.assertFalse(groups["libSceB"]["classified"])
            self.assertEqual((groups["libSceB"]["todo_names"], groups["libSceB"]["unobserved"]), (["Pending"], 0))
            self.assertEqual((data["done"], data["total"]), (1, 3))

    def test_root_reads_the_manifests_of_that_tree_and_restores_them(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_tree(root)
            before = progress.PRX, progress.OPCODES, progress.OBSERVED
            output = io.StringIO()
            with patch.object(progress, "collect_shaders", return_value=progress.summarize([])), patch("sys.stdout", output):
                self.assertEqual(progress.main(["--root", str(root), "--json"]), 0)
            data = json.loads(output.getvalue())
            groups = {group["name"]: group for group in data["libraries"]["groups"]}
            self.assertEqual(groups["libSceA"]["unobserved_names"], ["Unseen"])
            self.assertEqual(progress.OBSERVED, before[2])
            self.assertEqual((progress.PRX, progress.OPCODES), before[:2])

    def test_malformed_manifest_is_a_usage_error_naming_the_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _, observed = self.make_tree(root)
            (observed / "libSceA.txt").write_text("not-a-nid\n", encoding="utf-8")
            with patch.object(progress, "collect_shaders", return_value=progress.summarize([])), \
                 patch("sys.stderr", io.StringIO()) as errors, self.assertRaises(SystemExit) as exit_code:
                progress.main(["--root", str(root), "--json"])
            self.assertEqual(exit_code.exception.code, 2)
            self.assertIn("libSceA.txt:1: invalid NID 'not-a-nid'", errors.getvalue())


class ProgressTests(unittest.TestCase):
    def report_library(self, name, done=(), todo=(), **extra):
        return {"name": name, "label": name, "done": len(done), "todo": len(todo),
                "done_names": list(done), "todo_names": list(todo), **extra}

    def report_snapshot(self, *groups):
        return {"libraries": progress.summarize(list(groups)), "shaders": progress.summarize([])}

    def test_report_shows_each_changed_library_and_keeps_global_details(self):
        pad = self.report_library("libScePad", ["Ready"], ["Pending"])
        video = self.report_library("libSceVideoOut", ["A", "B", "C"], ["D"])
        unchanged = self.report_library("libSceUnused", ["Keep"])
        base = self.report_snapshot(video, unchanged, pad)
        head = self.report_snapshot(self.report_library("libScePad", ["Ready", "Pending"]), unchanged,
                                    self.report_library("libSceVideoOut", ["A", "B"], ["C", "D"]))
        report = progress.report(base, head)
        self.assertIn("**System libraries**", report)
        self.assertIn("| Library | Name |", report)
        self.assertIn("| libScePad | 1/2 (50.00%) | 2/2 (100.00%) | +50.00% |", report)
        self.assertIn("| libSceVideoOut | 3/4 (75.00%) | 2/4 (50.00%) | -25.00% |", report)
        table = report.split("#### Changed libraries (implemented exports)")[1]
        self.assertNotIn("libSceUnused", table)
        self.assertLess(table.index("libScePad"), table.index("libSceVideoOut"))

    def test_report_includes_changed_function_names_when_counts_stay_equal(self):
        base = self.report_snapshot(self.report_library("libScePad", ["Old"], ["Pending"]))
        head = self.report_snapshot(self.report_library("libScePad", ["New"], ["Pending"]))
        self.assertIn("| libScePad | 1/2 (50.00%) | 1/2 (50.00%) | +0.00% |", progress.report(base, head))

    def test_report_separates_observed_and_unobserved_stubs_and_unseen_implementations(self):
        base = self.report_snapshot(self.report_library("libScePad", ["Ready"], []))
        head_group = dict(self.report_library("libScePad", ["Ready", "Fresh", "Imported"], ["Pending"]),
                          unobserved=1, unobserved_names=["Unseen"], done_unobserved_names=["Fresh"])
        text = "\n".join(progress.compare("System libraries", "Library", "functions", base["libraries"],
                                          self.report_snapshot(head_group)["libraries"]))
        self.assertTrue(text.startswith("\U0001f4c9 **System libraries**: 75.0% (-25.0%, +2 implemented functions, +1 observed stubs, +1 unobserved stubs, declared 100.0% \u2192 60.0%)"))
        self.assertIn("<summary>\U0001f50d 1 implemented but not observed anywhere</summary>\n", text)
        self.assertIn("| libScePad | `Fresh` |", text)
        self.assertIn("| libScePad | `Pending` |", text)
        self.assertIn("(no measured use needs them)</summary>", text)
        self.assertIn("| libScePad | `Unseen` |", text)
        self.assertNotIn("`Imported`", text.split("implemented but not observed")[1].split("</details>")[0])

    def headline(self, head_group):
        base = self.report_snapshot(dict(self.report_library("libScePad", ["Ready"], ["Seen"]), unobserved=1, unobserved_names=["Old"]))
        text = progress.compare("System libraries", "Library", "functions", base["libraries"], self.report_snapshot(head_group)["libraries"])
        return text[0]

    def test_report_headline_names_the_declared_figure_for_a_new_unobserved_stub(self):
        group = dict(self.report_library("libScePad", ["Ready"], ["Seen"]), unobserved=2, unobserved_names=["New", "Old"])
        self.assertEqual(self.headline(group), "\u2796 **System libraries**: 50.0% (+0.0%, +1 unobserved stubs, declared 33.33% \u2192 25.0%)")

    def test_report_headline_for_a_new_observed_stub_has_no_declared_figure(self):
        group = dict(self.report_library("libScePad", ["Ready"], ["Seen", "New"]), unobserved=1, unobserved_names=["Old"])
        self.assertEqual(self.headline(group), "\U0001f4c9 **System libraries**: 33.33% (-16.67%, +1 observed stubs)")

    def test_report_headline_for_a_new_implemented_function_has_no_declared_figure(self):
        group = dict(self.report_library("libScePad", ["Ready", "New"], ["Seen"]), unobserved=1, unobserved_names=["Old"])
        self.assertEqual(self.headline(group), "\U0001f4c8 **System libraries**: 66.67% (+16.67%, +1 implemented functions)")

    def test_new_stubs_are_listed_with_a_link_to_their_origin_commit(self):
        sha = "0123456789abcdef0123456789abcdef01234567"
        base = self.report_snapshot(self.report_library("libScePad", ["Ready"], []))
        head_group = dict(self.report_library("libScePad", ["Ready"], ["Seen"]), unobserved=1, unobserved_names=["Unseen"],
                          origins={"Seen": {"commit": sha, "date": "2026-01-02"}})
        text = "\n".join(progress.compare("System libraries", "Library", "functions", base["libraries"],
                                          self.report_snapshot(head_group)["libraries"]))
        link = f"[`0123456789`]({progress.commit_url(sha)}) 2026-01-02"
        self.assertIn("| Library | Name | Origin |", text)
        self.assertIn(f"| libScePad | `Seen` | {link} |", text)
        self.assertIn("| libScePad | `Unseen` |\n", text)
        self.assertTrue(progress.commit_url(sha).endswith(f"/commit/{sha}"))

    def test_new_stubs_have_no_origin_column_without_origins(self):
        base = self.report_snapshot(self.report_library("libScePad", ["Ready"], []))
        head_group = dict(self.report_library("libScePad", ["Ready"], ["Seen"]), unobserved=0, unobserved_names=[])
        text = "\n".join(progress.compare("System libraries", "Library", "functions", base["libraries"],
                                          self.report_snapshot(head_group)["libraries"]))
        self.assertIn("| Library | Name |\n", text + "\n")
        self.assertNotIn("Origin", text)

    def test_report_table_shows_unobserved_stubs_beside_the_headline(self):
        before = self.report_snapshot(self.report_library("libScePad", ["A"], ["T"]))
        after = self.report_snapshot(dict(self.report_library("libScePad", ["A"], ["T"]), unobserved=2, unobserved_names=["U", "V"]),
                                     dict(self.report_library("libSceNew", [], []), unobserved=3, unobserved_names=["W", "X", "Y"]))
        table = "\n".join(progress.library_report(before["libraries"], after["libraries"]))
        self.assertIn("| libScePad | 1/2 (50.00%) | 1/2 (50.00%), +2 unobserved | +0.00% |", table)
        self.assertIn("| libSceNew | Not present | 0/0 observed, +3 unobserved | n/a |", table)

    def test_report_marks_new_removed_and_empty_libraries_without_percentages(self):
        base = self.report_snapshot(self.report_library("libSceRemoved", ["Ready"]),
                                    self.report_library("libSceEmptied", ["Old"]))
        head = self.report_snapshot(self.report_library("libSceAdded", ["New"], ["Pending"]),
                                    self.report_library("libSceEmpty"), self.report_library("libSceEmptied"))
        report = progress.report(base, head)
        self.assertIn("| libSceAdded | Not present | 1/2 (50.00%) | n/a |", report)
        self.assertIn("| libSceRemoved | 1/1 (100.00%) | Not present | n/a |", report)
        self.assertIn("| libSceEmpty | Not present | No exports | n/a |", report)
        self.assertIn("| libSceEmptied | 1/1 (100.00%) | No exports | n/a |", report)

    def test_report_marks_missing_counts_unavailable_with_no_delta(self):
        complete = {"name": "libSceLegacy", "done": 1, "todo": 0, "done_names": ["Current"]}
        for missing in ({"done_names": ["Legacy"]}, {"done": 0}, {"todo": 0}, {}):
            minimal = {"name": "libSceLegacy", **missing}
            for before, after in ((minimal, complete), (complete, minimal)):
                with self.subTest(before=before, after=after):
                    base = {"libraries": {"groups": [before], "percent": 0},
                            "shaders": {"groups": [], "percent": 0}}
                    head = {"libraries": {"groups": [after], "percent": 0},
                            "shaders": {"groups": [], "percent": 0}}
                    report = progress.report(base, head)
                    self.assertIn("Counts unavailable", report)
                    self.assertIn("| n/a |", report)
                    self.assertNotIn("No exports", report)

    def test_report_omits_unchanged_minimal_snapshots(self):
        minimal = {"name": "libSceLegacy"}
        snapshot = {"libraries": {"groups": [minimal], "percent": 0},
                    "shaders": {"groups": [], "percent": 0}}
        self.assertEqual(progress.report(snapshot, snapshot), "")

    def test_report_labels_shared_sources_without_duplicate_counts(self):
        owner = self.report_library("libScePad", ["Ready"], ["Pending"])
        alias = self.report_library("libScePad.native", ["Ready"], ["Pending"])
        shared = self.report_library("libScePad.native", shared_sources="libScePad")
        base, head = self.report_snapshot(owner, alias), self.report_snapshot(owner, shared)
        report = progress.report(base, head)
        self.assertIn("| libScePad.native | 1/2 (50.00%) | Shared sources: `libScePad` | n/a |", report)
        self.assertNotIn("removed", report)
        reverse = progress.report(head, base)
        self.assertIn("| libScePad.native | Shared sources: `libScePad` | 1/2 (50.00%) | n/a |", reverse)
        changed = self.report_snapshot(owner, self.report_library("libScePad.native", shared_sources="libSceOther"))
        self.assertIn("| libScePad.native | Shared sources: `libScePad` | Shared sources: `libSceOther` | n/a |",
                      progress.report(head, changed))

    def test_report_omits_unchanged_libraries_and_preserves_shader_report(self):
        library = self.report_library("libScePad", ["Ready"])
        base, head = self.report_snapshot(library), self.report_snapshot(dict(library, inert_names=["Ready"]))
        self.assertEqual(progress.report(base, head), "")
        head["shaders"] = progress.summarize([self.report_library("VOP1", ["V_MOV_B32"])])
        report = progress.report(base, head)
        self.assertEqual(report, "\n".join(progress.compare("GPU shader instructions", "Encoding", "instructions",
                                                          base["shaders"], head["shaders"])))
        self.assertNotIn("Changed libraries", report)

    def test_report_shows_a_headline_change_that_two_decimals_would_hide(self):
        before = dict(self.report_library("libScePad", ["A"], ["T"]), done=50000, todo=50001)
        after = dict(self.report_library("libScePad", ["A", "B"], ["T"]), done=50001, todo=50000)
        self.assertEqual(progress.compare("System libraries", "Library", "functions", self.report_snapshot(before)["libraries"],
                                          self.report_snapshot(after)["libraries"])[0],
                         "\U0001f4c8 **System libraries**: 50.0% (+0.001%, +1 implemented functions)")
        reverted = progress.compare("System libraries", "Library", "functions", self.report_snapshot(after)["libraries"],
                                    self.report_snapshot(before)["libraries"])[0]
        self.assertTrue(reverted.startswith("\U0001f4c9 **System libraries**: 50.0% (-0.001%, "))

    def test_report_keeps_a_zero_headline_change_when_the_ratio_is_unchanged(self):
        before = dict(self.report_library("libScePad", ["A"], ["T"]), done=1, todo=1)
        after = dict(self.report_library("libScePad", ["A", "B"], ["T", "U"]), done=2, todo=2)
        headline = progress.compare("System libraries", "Library", "functions", self.report_snapshot(before)["libraries"],
                                    self.report_snapshot(after)["libraries"])[0]
        self.assertEqual(headline, "\u2796 **System libraries**: 50.0% (+0.0%, +1 implemented functions, +1 observed stubs)")

    def test_report_marks_a_headline_change_below_four_decimals(self):
        before = dict(self.report_library("libScePad", ["A"], ["T"]), done=1, todo=10000000)
        after = dict(self.report_library("libScePad", ["A", "B"], ["T"]), done=2, todo=10000000)
        headline = progress.compare("System libraries", "Library", "functions", self.report_snapshot(before)["libraries"],
                                    self.report_snapshot(after)["libraries"])[0]
        self.assertIn("(+<0.0001%, +1 implemented functions)", headline)

    def test_percent_change_is_n_a_when_a_percentage_is_missing(self):
        self.assertEqual(progress.percent_change(None, 50), "n/a")
        self.assertEqual(progress.percent_change(50, None), "n/a")
        self.assertEqual(progress.percent_change(None, None), "n/a")

    def test_percent_change_keeps_the_sign_and_two_decimals(self):
        self.assertEqual(progress.percent_change(50, 100), "+50.00%")
        self.assertEqual(progress.percent_change(75, 50), "-25.00%")
        self.assertEqual(progress.percent_change(0, 100), "+100.00%")
        self.assertEqual(progress.percent_change(100, 0), "-100.00%")
        self.assertEqual(progress.percent_change(33.33, 66.67), "+33.34%")

    def test_percent_change_never_prints_a_negative_zero(self):
        self.assertEqual(progress.percent_change(50, 50), "+0.00%")
        self.assertEqual(progress.percent_change(50, 49.996), "+0.00%")
        self.assertEqual(progress.percent_change(49.996, 50), "+0.00%")

    def test_report_headline_writes_its_change_in_percent_for_gains_and_losses(self):
        gain = progress.report(self.report_snapshot(self.report_library("libScePad", ["R"], ["A", "B"])),
                               self.report_snapshot(self.report_library("libScePad", ["R", "A"], ["B"])))
        loss = progress.report(self.report_snapshot(self.report_library("libScePad", ["R", "A"], ["B"])),
                               self.report_snapshot(self.report_library("libScePad", ["R"], ["A", "B"])))
        self.assertRegex(gain.splitlines()[0], r"\*\*: [0-9.]+% \(\+[0-9.]+%[,)]")
        self.assertRegex(loss.splitlines()[0], r"\*\*: [0-9.]+% \(-[0-9.]+%[,)]")

    def test_report_table_writes_its_change_in_percent_and_never_in_pp(self):
        base = self.report_snapshot(self.report_library("libScePad", ["Ready"], ["A", "B"]))
        head = self.report_snapshot(self.report_library("libScePad", ["Ready", "A"], ["B"]))
        report = progress.report(base, head)
        row = next(line for line in report.splitlines() if line.startswith("| libScePad | 1/3"))
        self.assertTrue(row.endswith("| +33.33% |"), row)
        self.assertNotIn(" pp", report)

    def test_report_rounds_percentage_change_without_negative_zero(self):
        base = self.report_snapshot(self.report_library("libScePad", ["Ready"], ["A", "B"]))
        head = self.report_snapshot(self.report_library("libScePad", ["Ready", "A"], ["B"]))
        self.assertIn("| libScePad | 1/3 (33.33%) | 2/3 (66.67%) | +33.33% |", progress.report(base, head))
        before = dict(self.report_library("libSceLarge", ["Ready"]), todo=100000)
        after = dict(before, todo=100001)
        self.assertIn("| -<0.0001% |", progress.report(self.report_snapshot(before), self.report_snapshot(after)))
        self.assertNotIn("-0.00%", progress.report(self.report_snapshot(before), self.report_snapshot(after)))

    def test_report_shows_a_library_change_that_two_decimals_would_hide(self):
        before = dict(self.report_library("libc", ["A"], ["T"]), done=1340, todo=11)
        after = dict(self.report_library("libc", ["A", "B"], ["T"]), done=1341, todo=11)
        self.assertIn("| libc | 1340/1351 (99.19%) | 1341/1352 (99.19%) | +0.0006% |",
                      progress.report(self.report_snapshot(before), self.report_snapshot(after)))
        same = dict(self.report_library("libc", ["B"], ["T"]), done=1340, todo=11)
        self.assertIn("| +0.00% |", progress.report(self.report_snapshot(before), self.report_snapshot(same)))

    def test_report_compares_libraries_whose_counts_are_floats(self):
        before = dict(self.report_library("libc", ["A"], ["T"]), done=1.0, todo=2.0)
        after = dict(self.report_library("libc", ["A", "B"], ["T"]), done=2.0, todo=3.0)
        report = progress.report(self.report_snapshot(before), self.report_snapshot(after))
        self.assertIn("| libc | 1.0/3.0 (33.33%) | 2.0/5.0 (40.00%) | +6.67% |", report)
        tiny_before = dict(before, done=1340.0, todo=11.0)
        tiny_after = dict(after, done=1341.0, todo=11.0)
        report = progress.report(self.report_snapshot(tiny_before), self.report_snapshot(tiny_after))
        self.assertIn("| +0.00% |", report)

    def test_report_limits_library_rows(self):
        head = self.report_snapshot(*(self.report_library(f"libSce{i:03}", ["Ready"]) for i in range(103)))
        report = progress.report(self.report_snapshot(), head)
        table = report.split("#### Changed libraries (implemented exports)")[1]
        self.assertEqual(sum(line.startswith("| libSce") for line in table.splitlines()), progress.REPORT_ROWS)
        self.assertIn("3 more libraries", table)

    def test_opcodes_counted_through_variants_are_not_listed_as_uncounted(self):
        with tempfile.TemporaryDirectory() as directory:
            opcodes = Path(directory) / "RdnaOpcode.hpp"
            opcodes.write_text("enum class RdnaOpcode : int {\n    VAddI32,\n    VMulLoI32,\n    Count,\n};\n",
                               encoding="utf-8")
            isa = Path(directory) / "rdna_isa.txt"
            isa.write_text("V_ADD_CO_U32 VOP3\n", encoding="utf-8")
            with patch.multiple(progress, OPCODES=opcodes, ISA=isa):
                shaders = progress.collect_shaders()
        self.assertEqual(shaders["done"], 1)
        self.assertEqual(shaders["extra"], ["VMulLoI32"])

    def test_raw_string_example_does_not_declare_an_export(self):
        source = 'const char* example = R"help(\nint APS5_VABI Fake() { return 0; }\n)help";\n'
        source += 'int APS5_VABI Real() { return 7; }\n'
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(source, encoding="utf-8")
            group = progress.scan_library(library)
        self.assertEqual(group["done_names"], ["Real"])
        self.assertEqual(group["todo_names"], [])

    def test_stub_marker_inside_raw_literals_does_not_mark_implementation_as_stub(self):
        for prefix in ("", "u8", "u", "U", "L"):
            source = ('int APS5_VABI Ready(int value) {\n'
                      'auto message = ' + prefix + 'R"help(\n"quoted" NotImplemented_nid_no_patch(); } {\n)help";\n'
                      'return value + 1;\n}\n')
            with self.subTest(prefix=prefix), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(source, encoding="utf-8")
                group = progress.scan_library(library)
                self.assertEqual(group["done_names"], ["Ready"])
                self.assertEqual(group["todo_names"], [])

    def test_nonvoid_stub_wrappers_keep_callers_unimplemented(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int SupportFormat() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleSupportFormat() { return SupportFormat(); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], [])
            self.assertEqual(group["todo_names"], ["sceExampleSupportFormat"])

    def test_stub_wrapper_calls_allow_whitespace(self):
        for separator in ("", " ", "\t", "\n", " /* separator */ "):
            with self.subTest(separator=separator), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(
                    "static int Unsupported() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                    f"int APS5_VABI Pending() {{ return Unsupported{separator}(); }}\n")
                group = progress.scan_library(library)
                self.assertEqual(group["done_names"], [])
                self.assertEqual(group["todo_names"], ["Pending"])

    def test_stub_wrapper_names_match_whole_identifiers(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Unsupported() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "static int NotUnsupported(int value) { return value * 2; }\n"
                "int APS5_VABI Ready(int value) { return NotUnsupported(value); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["Ready"])
            self.assertEqual(group["todo_names"], [])

    def test_braces_in_literals_and_comments_do_not_delimit_bodies(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Helper() { const char* brace = \"}\"; NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleOpen(char* out) { *out = '{'; return 0; }\n"
                "int APS5_VABI sceExampleClose(char* out) { *out = '}'; NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleQuote(std::string& out) { out += \"{\\\"\"; return 0; }\n"
                "int APS5_VABI sceExampleNoted() { // }\n    NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI sceExampleBlock() { /* { */ return 0; }\n"
                "int APS5_VABI sceExampleWrapped() { return Helper(); }\n"
                "int APS5_VABI sceExampleReady() { return 0; }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["sceExampleBlock", "sceExampleOpen", "sceExampleQuote", "sceExampleReady"])
            self.assertEqual(group["todo_names"], ["sceExampleClose", "sceExampleNoted", "sceExampleWrapped"])

    def test_apostrophe_in_a_comment_does_not_pair_with_a_later_literal(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI sceExamplePending() {\n    /* it's */ log('}');\n"
                "    NotImplemented_nid_no_patch(__func__);\n    return 0;\n}\n"
                "int APS5_VABI sceExampleReady() { return 0; }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["sceExampleReady"])
            self.assertEqual(group["todo_names"], ["sceExamplePending"])

    def test_test_sources_do_not_change_library_progress(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "tests" / "prx"
            library = root / "libSceExample"
            source = library / "src"
            source.mkdir(parents=True)
            (source / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\n"
                "int APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n")
            alias = root / "libSceExample.native"
            alias.mkdir()
            (alias / "CMakeLists.txt").write_text('include("${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake")\n')
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
                self.assertEqual(base["groups"][0]["done_names"], ["Ready"])
                self.assertEqual(base["groups"][0]["todo_names"], ["Pending"])
                tests = library / "tests"
                nested = tests / "fixtures"
                nested.mkdir(parents=True)
                (tests / "Callbacks.cpp").write_text(
                    "int APS5_VABI FixtureReady() { return 0; }\n"
                    "int APS5_VABI Pending() { return 0; }\n"
                    "int APS5_VABI FixturePending() { NotImplemented_nid_no_patch(__func__); }\n")
                (nested / "Callbacks.cpp").write_text("int APS5_VABI NestedFixture() { return 0; }\n")
                (alias / "tests").mkdir()
                (alias / "tests" / "Callbacks.cpp").write_text("int APS5_VABI AliasFixture() { return 0; }\n")
                head = progress.collect_libraries()
                self.assertEqual(head, base)
                self.assertEqual(progress.compare("Libraries", "Library", "functions", base, head), [])

    def test_shared_source_refactor_preserves_functions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = root / "libSceExample"
            alias = root / "libSceExample.native"
            empty = root / "libSceEmpty"
            for path in (owner, alias, empty):
                path.mkdir()
            source = "int APS5_VABI Ready() { return 0; }\nint APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n"
            (owner / "Export.cpp").write_text(source)
            (alias / "Export.cpp").write_text(source)
            (owner / "Library.cmake").write_text("add_library(example SHARED Export.cpp)\n")
            (empty / "CMakeLists.txt").write_text('# include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake)\n')
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
                self.assertEqual((base["done"], base["total"]), (2, 4))
                (alias / "Export.cpp").unlink()
                (alias / "CMakeLists.txt").write_text('include("${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake")\n')
                head = progress.collect_libraries()
                self.assertEqual((head["done"], head["total"]), (1, 2))
                groups = {group["name"]: group for group in head["groups"]}
                self.assertEqual(groups[alias.name]["shared_sources"], owner.name)
                self.assertNotIn("shared_sources", groups[empty.name])
                html = progress.table("Libraries", "Library", head)
                self.assertIn('colspan="3">Shared sources:', html)
                self.assertIn("shared by " + alias.name, "\n".join(progress.treemap("Libraries", head, 0)))
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, head))
                self.assertIn("sharing sources", report)
                self.assertNotIn("removed", report)
                self.assertNotIn("implemented", report)
                self.assertEqual(progress.compare("Libraries", "Library", "functions", head, head), [])
                (owner / "Export.cpp").write_text(source.replace("return 0;", "NotImplemented_nid_no_patch(__func__);"))
                regressed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, regressed))
                self.assertIn("-1 reverted", report)
                self.assertIn("Ready", report)
                (owner / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
                removed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, removed))
                self.assertIn("-1 removed", report)
                self.assertIn("Pending", report)

    def test_inert_bodies_are_marked_without_changing_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "static int Support() { return 0; }\n"
                "static int Compute(int x) { return x * 2; }\n"
                "int APS5_VABI A() { return 0; }\n"
                "int APS5_VABI B(const void* p) { (void)p; return SCE_KERNEL_ERROR_EOPNOTSUPP; }\n"
                "int APS5_VABI C(void* p) { (void)p; return 0x8A810001; }\n"
                'const char* APS5_VABI I() { return "constant; { text }"; }\n'
                'int APS5_VABI D() { throw std::runtime_error("no"); }\n'
                'std::string APS5_VABI J() { return std::string("not constant"); }\n'
                "int APS5_VABI E() { return Support(); }\n"
                "int APS5_VABI F(int a, int b) { return a + b; }\n"
                "int APS5_VABI G() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "int APS5_VABI H(int a) { return Compute(a); }\n")
            group = progress.scan_library(library)
            self.assertEqual(group["inert_names"], ["A", "B", "C", "D", "E", "I"])
            self.assertEqual(group["done_names"], ["A", "B", "C", "D", "E", "F", "H", "I", "J"])
            self.assertEqual(group["todo_names"], ["G"])

    def test_compare_marks_behaviourally_empty_implementations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            library = root / "libSceExample"
            library.mkdir()
            kept = (
                "int APS5_VABI Ready() { return 0; }\n"
                "int APS5_VABI Stubbed() { NotImplemented_nid_no_patch(__func__); return 0; }\n"
                "static int Compute(int x) { return x * 2; }\n"
                "int APS5_VABI Wrapped(int a) { return Compute(a); }\n")
            added = (
                "static int Support() { return 0; }\n"
                "int APS5_VABI A() { return 0; }\n"
                "int APS5_VABI B(const void* p) { (void)p; return SCE_KERNEL_ERROR_EOPNOTSUPP; }\n"
                "int APS5_VABI C(void* p) { (void)p; return 0x8A810001; }\n"
                'int APS5_VABI D() { throw std::runtime_error("no"); }\n'
                "int APS5_VABI E() { return Support(); }\n"
                "int APS5_VABI F(int a, int b) { return a + b; }\n")
            (library / "Export.cpp").write_text(kept)
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
            for group in base["groups"]:
                del group["inert_names"]
            (library / "Export.cpp").write_text(kept + added)
            with patch.object(progress, "PRX", root):
                head = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", base, head))
            self.assertIn("+6 implemented", report)
            self.assertIn("<summary>⚪ 5 behaviourally empty (constant return or throw)</summary>", report)
            empty = report.split("behaviourally empty (constant return or throw)</summary>")[1]
            self.assertEqual([line for line in empty.splitlines() if line.startswith("|")],
                             ["| Library | Name |", "| - | - |"]
                             + [f"| libSceExample | `{name}` |" for name in "ABCDE"])
            self.assertEqual(progress.compare("Libraries", "Library", "functions", head, head), [])
            (library / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
            with patch.object(progress, "PRX", root):
                before = progress.collect_libraries()
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\nint APS5_VABI F(int a, int b) { return a + b; }\n")
            with patch.object(progress, "PRX", root):
                after = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", before, after))
            self.assertIn("+1 implemented", report)
            self.assertNotIn("behaviourally empty", report)
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready() { return 0; }\nint APS5_VABI F(int a, int b) { return a + b; }\nint APS5_VABI A() { return 0; }\n")
            with patch.object(progress, "PRX", root):
                grown = progress.collect_libraries()
            report = "\n".join(progress.compare("Libraries", "Library", "functions", after, grown))
            self.assertIn("+1 implemented", report)
            self.assertIn("| libSceExample | `A` |", report)
            self.assertIn("<summary>⚪ 1 behaviourally empty (constant return or throw)</summary>", report)

    def test_cut_helpers_are_not_system_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Export.cpp").write_text(
                "int APS5_VABI Ready_nid_no_patch_cut() { return 1; }\n"
                "int APS5_VABI Pending_nid_no_patch_cut() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI Real() { return 2; }\n", encoding="utf-8")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["Real"])
            self.assertEqual(group["todo_names"], [])
            self.assertEqual(group["inert_names"], ["Real"])

    def test_explicitly_exported_nid_no_patch_definitions_are_counted(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "libSceExample"
            library.mkdir()
            (library / "Functions.cpp").write_text(
                "int APS5_VABI Pending_nid_no_patch() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI Ready_nid_no_patch() { return 1; }\n"
                "int APS5_VABI Helper_nid_no_patch() { NotImplemented_nid_no_patch(__func__); }\n"
                "int APS5_VABI Cut_nid_no_patch_cut() { NotImplemented_nid_no_patch(__func__); }\n", encoding="utf-8")
            (library / "Export.cpp").write_text(
                'APS5_EXPORT("AAAAAAAAAAA", Pending_nid_no_patch);\n'
                'APS5_EXPORT(\n    "_ZN3sce7Example5ReadyEv",\n    Ready_nid_no_patch);\n'
                '// APS5_EXPORT("BBBBBBBBBBB", Helper_nid_no_patch);\n'
                'const char* text = "APS5_EXPORT(\\"CCCCCCCCCCC\\", Cut_nid_no_patch_cut)";\n', encoding="utf-8")
            group = progress.scan_library(library)
            self.assertEqual(group["done_names"], ["Ready_nid_no_patch"])
            self.assertEqual(group["todo_names"], ["Pending_nid_no_patch"])

    def test_empty_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = root / "libSceExample"
            alias = root / "libSceExampleBackend"
            dummy = root / "libSceDummy"
            for path in (owner, alias, dummy):
                path.mkdir()
            (owner / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
            (alias / "Export.cpp").write_text('#include "prx/libSceExample/Export.cpp"\n')
            (dummy / "Export.cpp").write_text('#include "prx/libc/include/General.hpp"\nextern "C" {\nAPS5_DUMMY_FUN\n}\n')
            with patch.object(progress, "PRX", root):
                data = progress.collect_libraries()
            groups = {group["name"]: group for group in data["groups"]}
            self.assertEqual(groups[alias.name]["shared_sources"], owner.name)
            self.assertNotIn("shared_sources", groups[dummy.name])
            self.assertEqual((data["done"], data["total"]), (1, 1))
            html = progress.table("Libraries", "Library", data)
            self.assertIn(f'<td>{alias.name}</td><td colspan="3">Shared sources:', html)
            self.assertIn(f'<td>{dummy.name}</td><td colspan="3">No exports</td>', html)
            self.assertNotIn("<td>0</td>", html)


    def test_page_text_explains_the_states_and_both_percentages(self):
        libraries = progress.summarize([{"name": "libA", "label": "A", "done": 1, "todo": 0, "unobserved": 1}])
        shaders = progress.summarize([])
        shaders["extra"] = []
        page = progress.summary(libraries, shaders)
        self.assertIn("an unobserved stub otherwise", page)
        self.assertIn("so unobserved stubs do not lower it", page)
        self.assertIn("counts all its stubs as observed", page)
        self.assertIn("/docs/dev/PROGRESS.md#observed-imports", page)
        self.assertIn("<th>Declared %</th>", page)

    def test_library_table_shows_each_state_and_both_percentages(self):
        groups = [{"name": "libA", "label": "A", "done": 1, "todo": 1, "unobserved": 2},
                  {"name": "libB", "label": "B", "done": 0, "todo": 0, "unobserved": 3},
                  {"name": "libC", "label": "C", "done": 0, "todo": 0}]
        html = progress.table("System libraries", "Library", progress.summarize(groups), states=True)
        self.assertIn("<th>Implemented</th><th>Observed stubs</th><th>Unobserved stubs</th><th>%</th><th>Declared %</th>", html)
        self.assertIn("<tr><td>libA</td><td>1</td><td>1</td><td>2</td><td>50%</td><td>25%</td></tr>", html)
        self.assertIn("<tr><td>libB</td><td>0</td><td>0</td><td>3</td><td>n/a</td><td>0%</td></tr>", html)
        self.assertIn('<tr><td>libC</td><td colspan="5">No exports</td></tr>', html)
        self.assertIn("<tr><th>Total</th><th>1</th><th>1</th><th>5</th><th>50.0%</th><th>14.29%</th></tr>", html)

    def test_qualified_calls_match_only_the_global_wrapper(self):
        wrapper = "static void Fail() { NotImplemented_nid_no_patch(__func__); }\n"
        cases = {"obj->Fail();": "done", "obj.Fail();": "done", "X::Fail();": "done",
                 "Foo<int>::Fail();": "done", "::Fail();": "todo", ":: Fail ();": "todo", "Fail();": "todo",
                 "return ::Fail();": "todo", "else ::Fail();": "todo", "if (obj) ::Fail();": "todo"}
        for call, expected in cases.items():
            with self.subTest(call=call), tempfile.TemporaryDirectory() as directory:
                library = Path(directory) / "libSceExample"
                library.mkdir()
                (library / "Export.cpp").write_text(wrapper + "int APS5_VABI Run(Obj* obj) { " + call + " return 0; }\n")
                group = progress.scan_library(library)
                self.assertEqual(group[expected + "_names"], ["Run"])

    def library_snapshot(self):
        libraries = progress.summarize([
            {"name": "libScePad", "label": "Pad", "done": 1, "todo": 1,
             "done_names": ["Ready"], "todo_names": ["Pending"], "inert_names": []},
            {"name": "libSceVideoOut", "label": "VideoOut", "done": 3, "todo": 1,
             "done_names": ["A", "B", "C"], "todo_names": ["D"], "inert_names": []},
            {"name": "libScePad.native", "label": "Pad.native", "done": 0, "todo": 0,
             "done_names": [], "todo_names": [], "inert_names": [], "shared_sources": "libScePad"},
        ])
        shaders = {"done": 2, "total": 4, "percent": 50.0, "groups": [], "extra": []}
        return libraries, shaders

    def run_main(self, argv, libraries, shaders):
        stdout = io.StringIO()
        with patch.object(progress, "collect_libraries", return_value=libraries), \
             patch.object(progress, "collect_shaders", return_value=shaders), \
             patch("sys.stdout", stdout):
            code = progress.main(argv)
        return code, stdout.getvalue()

    def test_module_filter_recomputes_library_totals(self):
        libraries, shaders = self.library_snapshot()
        selected = progress.select_libraries(libraries, ["libSceVideoOut", "libScePad", "libScePad"])
        self.assertEqual([group["name"] for group in selected["groups"]], ["libScePad", "libSceVideoOut"])
        self.assertEqual((selected["done"], selected["total"], selected["percent"]), (4, 6, 66.67))
        self.assertEqual(shaders["total"], 4)
        wrapper = progress.select_libraries(libraries, ["libScePad.native"])
        self.assertEqual(wrapper["groups"][0]["shared_sources"], "libScePad")
        self.assertEqual((wrapper["done"], wrapper["total"]), (0, 0))
        with self.assertRaisesRegex(ValueError, "unknown libraries: libSceNope, libSceMissing"):
            progress.select_libraries(libraries, ["libScePad", "libSceNope", "libSceMissing", "libSceNope"])

    def test_summary_lines_name_the_declared_figure_only_when_stubs_are_unobserved(self):
        _, shaders = self.library_snapshot()
        libraries = progress.summarize([{"name": "libA", "label": "A", "done": 1, "todo": 1, "unobserved": 2}])
        code, output = self.run_main(["--module", "libA"], libraries, shaders)
        self.assertEqual(code, 0)
        self.assertEqual(output.splitlines()[0], "libraries 1/2 (50.0%), declared 25.0% with 2 unobserved stubs")

    def test_json_and_module_do_not_require_an_output_directory(self):
        libraries, shaders = self.library_snapshot()
        code, output = self.run_main(["--json", "--module", "libScePad"], libraries, shaders)
        self.assertEqual(code, 0)
        payload = json.loads(output)
        self.assertEqual(payload["libraries"]["groups"][0]["name"], "libScePad")
        self.assertEqual((payload["libraries"]["done"], payload["libraries"]["total"]), (1, 2))
        self.assertEqual(payload["shaders"], shaders)
        self.assertNotIn("libraries 1/2", output)
        code, output = self.run_main(["--module", "libScePad"], libraries, shaders)
        self.assertEqual(output, "libraries 1/2 (50.0%)\nshaders 2/4 (50.0%)\n")
        stderr = io.StringIO()
        with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
            progress.main([])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("the output directory is required", stderr.getvalue())

    def test_module_filter_is_what_the_site_writes(self):
        libraries, shaders = self.library_snapshot()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            code, text = self.run_main(["--json", "--module", "libScePad", str(output)], libraries, shaders)
            self.assertEqual(code, 0)
            written = json.loads((output / "progress.json").read_text(encoding="utf-8"))
            self.assertEqual(written, json.loads(text))
            self.assertEqual(written["libraries"]["groups"][0]["name"], "libScePad")
            self.assertEqual(written["shaders"]["done"], 2)
            page = (output / "progress.html").read_text(encoding="utf-8")
            self.assertIn("libScePad", page)
            self.assertNotIn("libSceVideoOut", page)
            for name in ("badge-libraries.svg", "badge-shaders.svg", "progress.svg", "progress.html"):
                self.assertTrue((output / name).is_file())
        original = progress.PRX
        with tempfile.TemporaryDirectory() as directory:
            self.run_main(["--root", directory, "--json"], libraries, shaders)
        self.assertEqual(progress.PRX, original)

    def test_compare_stays_a_report_of_two_snapshots(self):
        def snapshot(names):
            return {"libraries": progress.summarize([
                        {"name": "libScePad", "label": "Pad", "done": len(names), "todo": 0,
                         "done_names": names, "todo_names": [], "inert_names": []}]),
                    "shaders": {"done": 0, "total": 0, "percent": 0, "groups": [], "extra": []}}
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "base.json"
            head = Path(directory) / "head.json"
            base.write_text(json.dumps(snapshot([])), encoding="utf-8")
            head.write_text(json.dumps(snapshot(["Ready"])), encoding="utf-8")
            stdout = io.StringIO()
            with patch("sys.stdout", stdout):
                code = progress.main(["--compare", str(base), str(head)])
            self.assertEqual(code, 0)
            self.assertIn("implemented", stdout.getvalue())
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                progress.main(["--compare", str(base), str(head), "--json"])
            self.assertEqual(raised.exception.code, 2)
            self.assertIn("--compare does not combine with --module or --json", stderr.getvalue())


    def test_compare_names_a_snapshot_it_cannot_read(self):
        with tempfile.TemporaryDirectory() as directory:
            good = Path(directory) / "good.json"
            good.write_text("{}", encoding="utf-8")
            broken = Path(directory) / "broken snapshot.json"
            broken.write_text("not json", encoding="utf-8")
            for name, path in (("invalid", broken), ("missing", Path(directory) / "missing snapshot.json")):
                with self.subTest(name):
                    stderr = io.StringIO()
                    with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                        progress.main(["--compare", str(good), str(path)])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn(str(path), stderr.getvalue())

    def test_output_directory_that_cannot_be_written_is_named(self):
        libraries, shaders = self.library_snapshot()
        with tempfile.TemporaryDirectory() as directory:
            blocked = Path(directory) / "blocked output"
            blocked.write_text("a file, not a directory", encoding="utf-8")
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                self.run_main([str(blocked)], libraries, shaders)
            self.assertEqual(raised.exception.code, 2)
            self.assertIn(str(blocked), stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())

    def test_aliased_reports_are_refused_and_left_unchanged(self):
        libraries, shaders = self.library_snapshot()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "site"
            output.mkdir()
            first, second = output / "progress.json", output / "progress.html"
            first.write_text("json report", encoding="utf-8")
            try:
                os.link(first, second)
            except OSError:
                self.skipTest("hard links are not available")
            others = {name: output / name for name in ("badge-libraries.svg", "badge-shaders.svg", "progress.svg")}
            for name, path in others.items():
                path.write_text(name, encoding="utf-8")
            stderr = io.StringIO()
            with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                self.run_main([str(output)], libraries, shaders)
            self.assertEqual(raised.exception.code, 2)
            self.assertIn("progress.json and progress.html", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())
            self.assertEqual(first.read_text(encoding="utf-8"), "json report")
            self.assertEqual(second.read_text(encoding="utf-8"), "json report")
            for name, path in others.items():
                self.assertEqual(path.read_text(encoding="utf-8"), name)

    def test_root_must_be_a_source_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            empty = Path(directory) / "empty tree"
            empty.mkdir()
            half = Path(directory) / "half tree"
            (half / "core" / "libs" / "prx").mkdir(parents=True)
            for name, path in (("missing", Path(directory) / "missing tree"), ("empty", empty), ("no opcodes", half)):
                with self.subTest(name):
                    stderr = io.StringIO()
                    with patch("sys.stderr", stderr), self.assertRaises(SystemExit) as raised:
                        progress.main(["--json", "--root", str(path)])
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn("is not a source tree", stderr.getvalue())
                    self.assertIn(str(path), stderr.getvalue())

    def test_compare_accepts_snapshots_with_utf8_bom(self):
        def snapshot(names):
            return {"libraries": progress.summarize([
                        {"name": "libScePad", "label": "Pad", "done": len(names), "todo": 0,
                         "done_names": names, "todo_names": [], "inert_names": []}]),
                    "shaders": {"done": 0, "total": 0, "percent": 0, "groups": [], "extra": []}}
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory) / "base.json"
            head = Path(directory) / "head.json"
            base.write_text("\ufeff" + json.dumps(snapshot([])), encoding="utf-8")
            head.write_text("\ufeff" + json.dumps(snapshot(["Ready"])), encoding="utf-8")
            stdout = io.StringIO()
            with patch("sys.stdout", stdout):
                code = progress.main(["--compare", str(base), str(head)])
            self.assertEqual(code, 0)
            self.assertIn("implemented", stdout.getvalue())

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(progress.sys, "stdout", out), patch.object(progress.sys, "stderr", err):
            try:
                progress.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
