import contextlib
import hashlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate_patch_catalog


class CatalogValidatorTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="patch catalog validation-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.catalog = self.root / "catalog"
        self.catalog.mkdir()
        self.patch = self.catalog / "patch.toml"
        self.patch.write_text('format = 1\n[[patch]]\ntarget = "module.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        self.entry = {"titleId": "TEST00001", "contentVersion": "01.000.000", "patch": "patch.toml",
                      "targets": {"module.prx": hashlib.sha256(b"module").hexdigest()}}

    def run_validator(self, entries=None):
        (self.catalog / "index.json").write_text(json.dumps({"format": 1, "entries": entries if entries is not None else [self.entry]}))
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = validate_patch_catalog.main(["--catalog", str(self.catalog)])
        return result, output.getvalue(), errors.getvalue()

    def test_valid_legacy_and_metadata_entries_need_no_game_files(self):
        newer = dict(self.entry, id="sample_fix", name="Sample fix", category="compatibility",
                     revision=2, description="Changes one byte",
                     tested=[{"stage": "verification", "result": "passed", "evidence": "Synthetic fixture"}])
        newer["contentVersion"] = "02.000.000"
        with patch.object(validate_patch_catalog.suggest_patches.subprocess, "run") as run:
            result, output, errors = self.run_validator([self.entry, newer])
        self.assertEqual((result, output, errors), (0, "Catalog validation: 2 entries valid.\n", ""))
        run.assert_not_called()

    def test_empty_catalog_is_valid(self):
        self.assertEqual(self.run_validator([]), (0, "Catalog validation: 0 entries valid.\n", ""))

    def test_invalid_entries_are_all_reported_even_for_unrelated_titles(self):
        missing = dict(self.entry, titleId="OTHER0001", patch="missing.toml")
        malformed = dict(self.entry, titleId="THIRD0001", category="not-a-category")
        result, output, errors = self.run_validator([missing, self.entry, malformed])
        self.assertEqual(result, 2)
        self.assertEqual(output, "")
        self.assertIn("Catalog entry 1 ('missing.toml'): invalid:", errors)
        self.assertIn("Catalog entry 3 ('patch.toml'): invalid: invalid category", errors)
        self.assertIn("Catalog validation: 2 invalid of 3 entries.", errors)

    def test_duplicate_id_is_scoped_to_title_and_version_even_across_revisions(self):
        first = dict(self.entry, id="sample_fix", revision=1)
        second = dict(self.entry, id="sample_fix", revision=2)
        result, _, errors = self.run_validator([first, second])
        self.assertEqual(result, 2)
        self.assertIn("duplicate id 'sample_fix' for TEST00001 01.000.000 may match entry 1", errors)
        second["contentVersion"] = "02.000.000"
        self.assertEqual(self.run_validator([first, second])[0], 0)

    def test_same_id_can_describe_mutually_exclusive_builds(self):
        first = dict(self.entry, id="sample_fix")
        second = dict(self.entry, id="sample_fix", targets={"module.prx": "a" * 64})
        self.assertEqual(self.run_validator([first, second])[0], 0)

    def test_same_id_on_disjoint_targets_can_match_one_build(self):
        first = dict(self.entry, id="sample_fix")
        (self.catalog / "other.toml").write_text('format = 1\n[[patch]]\ntarget = "other.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        second = dict(self.entry, id="sample_fix", patch="other.toml", targets={"other.prx": "a" * 64})
        result, _, errors = self.run_validator([first, second])
        self.assertEqual(result, 2)
        self.assertIn("may match entry 1", errors)

    def test_target_path_must_be_relative(self):
        self.patch.write_text('format = 1\n[[patch]]\ntarget = "../outside.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        self.entry["targets"] = {"../outside.prx": "0" * 64}
        result, _, errors = self.run_validator()
        self.assertEqual(result, 2)
        self.assertIn("invalid relative path: '../outside.prx'", errors)
        self.patch.write_text('format = 1\n[[patch]]\ntarget = "C:/outside.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        self.entry["targets"] = {"C:/outside.prx": "0" * 64}
        self.assertIn("invalid relative path: 'C:/outside.prx'", self.run_validator()[2])

    def test_patch_path_cannot_escape_catalog(self):
        outside = self.root / "outside.toml"
        outside.write_bytes(self.patch.read_bytes())
        self.entry["patch"] = "../outside.toml"
        result, _, errors = self.run_validator()
        self.assertEqual(result, 2)
        self.assertIn("invalid relative path: '../outside.toml'", errors)

    def test_malformed_metadata_and_target_coverage_are_reported(self):
        malformed = dict(self.entry, tested=[{"stage": "launch", "result": "passed"}])
        uncovered = dict(self.entry, titleId="OTHER0001", targets={"other.prx": "0" * 64})
        result, _, errors = self.run_validator([malformed, uncovered])
        self.assertEqual(result, 2)
        self.assertIn("invalid tested record fields", errors)
        self.assertIn("target hashes must cover exactly the patch targets", errors)

    def test_patch_file_requires_format_one(self):
        self.patch.write_text('[[patch]]\ntarget = "module.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        result, _, errors = self.run_validator()
        self.assertEqual(result, 2)
        self.assertIn("patch file must have format 1", errors)

    def test_malformed_entry_and_index_report_errors(self):
        result, _, errors = self.run_validator([None, self.entry])
        self.assertEqual(result, 2)
        self.assertIn("Catalog entry 1: invalid: entry must be an object", errors)
        (self.catalog / "index.json").write_text('{"format": 2, "entries": []}')
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = validate_patch_catalog.main(["--catalog", str(self.catalog)])
        self.assertEqual(result, 2)
        self.assertIn("catalog must have format 1", errors.getvalue())

    def test_duplicate_json_key_is_rejected(self):
        index = self.catalog / "index.json"
        digest = self.entry["targets"]["module.prx"]
        content = ('{"format":1,"entries":[{"titleId":"TEST00001","contentVersion":"01.000.000",'
                   '"patch":"missing.toml","patch":"patch.toml","targets":{"module.prx":"'
                   f'{digest}' '"}}]}')
        index.write_text(content)
        result = subprocess.run([sys.executable, str(Path(validate_patch_catalog.__file__)),
                                 "--catalog", str(self.catalog)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Catalog validation: duplicate JSON key 'patch'", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(index.read_text(), content)

    def test_deeply_nested_index_is_reported_without_traceback(self):
        index = self.catalog / "index.json"
        content = '{"format": 1, "entries": ' + "[" * 200000 + "]" * 200000 + "}"
        index.write_text(content)
        result = subprocess.run([sys.executable, str(Path(validate_patch_catalog.__file__)),
                                 "--catalog", str(self.catalog)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Catalog validation: index.json is nested too deeply", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(index.read_text(), content)

    def test_deeply_nested_patch_file_is_reported_without_traceback(self):
        self.run_validator()
        content = 'format = 1\n[[patch]]\ntarget = "module.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\nextra = ' + "[" * 3000 + "0" + "]" * 3000 + "\n"
        self.patch.write_text(content)
        result = subprocess.run([sys.executable, str(Path(validate_patch_catalog.__file__)),
                                 "--catalog", str(self.catalog)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Catalog entry 1 ('patch.toml'): invalid: entry or patch file is nested too deeply", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual(self.patch.read_text(), content)

    def test_catalog_symlink_loop_is_reported_without_traceback(self):
        loop = self.root / "loop"
        try:
            loop.symlink_to(loop)
        except OSError as error:
            self.skipTest(f"symlinks unavailable: {error}")
        result = subprocess.run([sys.executable, str(Path(validate_patch_catalog.__file__)),
                                 "--catalog", str(loop)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Catalog validation:", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertTrue(loop.is_symlink())


if __name__ == "__main__":
    unittest.main()
