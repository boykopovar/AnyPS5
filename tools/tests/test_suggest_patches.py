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
import suggest_patches

VERIFIED = "Verified 1 patches: 1 ok, 0 not-checked, 0 mismatch\n"


class PatchAdvisorTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.catalog = self.root / "catalog"
        self.catalog.mkdir()
        self.input = self.root / "eboot.bin"
        self.input.write_bytes(b"main executable")
        self.module = self.root / "Media" / "Modules" / "module.prx"
        self.module.parent.mkdir(parents=True)
        self.module.write_bytes(b"module version one")
        self.param = self.root / "param.json"
        self.param.write_text(json.dumps({"titleId": "TEST00001", "contentVersion": "01.000.000"}))
        self.relinker = self.root / "relinker"
        self.relinker.write_bytes(b"fixture")
        (self.catalog / "patch.toml").write_text('format = 1\n[[patch]]\ntarget = "Media/Modules/module.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        self.entry = {"titleId": "TEST00001", "contentVersion": "01.000.000", "patch": "patch.toml",
                      "targets": {"Media/Modules/module.prx": hashlib.sha256(self.module.read_bytes()).hexdigest()}}
        self.write_index()

    def write_index(self, entries=None):
        (self.catalog / "index.json").write_text(json.dumps({"format": 1, "entries": entries if entries is not None else [self.entry]}))

    def advise(self, *extra):
        return suggest_patches.main(["--catalog", str(self.catalog), "--param-json", str(self.param),
                                     "--input", str(self.input), "--relinker", str(self.relinker),
                                     "--module-dir", "Media/Modules", *extra])

    def additional_executable(self):
        executable = self.root / "picker" / "picker.bin"
        executable.parent.mkdir()
        executable.write_bytes(b"secondary executable")
        (self.catalog / "patch.toml").write_text('format = 1\n[[patch]]\ntarget = "picker/picker.bin"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        self.entry["targets"] = {"picker/picker.bin": hashlib.sha256(executable.read_bytes()).hexdigest()}
        self.write_index()
        return executable

    @patch.object(suggest_patches.subprocess, "run")
    def test_verified_match_requires_explicit_application(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        self.assertEqual(self.advise(), 0)
        self.assertEqual(run.call_args.args[0], [str(self.relinker), "--verify-patches",
                                                 str(self.catalog / "patch.toml"), "--module-dir", "Media/Modules",
                                                 str(self.input)])
        self.assertFalse((self.root / "relinker-patches.toml").exists())

    @patch.object(suggest_patches.subprocess, "run")
    def test_changed_module_is_not_sent_to_relinker(self, run):
        self.module.write_bytes(b"module version two")
        self.assertEqual(self.advise(), 1)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_rejection_is_not_a_recommendation(self, run):
        run.return_value.returncode = 1
        run.return_value.stdout = "Mismatch: patch\nVerified 1 patches: 0 ok, 0 not-checked, 1 mismatch\n"
        run.return_value.stderr = ""
        self.assertEqual(self.advise(), 1)

    @patch.object(suggest_patches.subprocess, "run")
    def test_catalog_cannot_escape_directory(self, run):
        self.entry["patch"] = "../outside.toml"
        self.write_index()
        self.assertEqual(self.advise(), 1)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_hashes_cover_all_patch_targets(self, run):
        self.entry["targets"] = {"eboot.bin": hashlib.sha256(self.input.read_bytes()).hexdigest()}
        self.write_index()
        self.assertEqual(self.advise(), 1)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_additional_executable_is_hashed_and_verified(self, run):
        self.additional_executable()
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        self.assertEqual(self.advise("--executable", "picker/picker.bin"), 0)
        self.assertEqual(run.call_args.args[0], [str(self.relinker), "--verify-patches",
                                                 str(self.catalog / "patch.toml"), "--module-dir", "Media/Modules",
                                                 "--executable", "picker/picker.bin", str(self.input)])

    @patch.object(suggest_patches.subprocess, "run")
    def test_changed_additional_executable_is_not_verified(self, run):
        executable = self.additional_executable()
        executable.write_bytes(b"different executable")
        self.assertEqual(self.advise("--executable", "picker/picker.bin"), 1)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_additional_executable_cannot_escape_module_parent(self, run):
        self.assertEqual(self.advise("--executable", "../outside.bin"), 2)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_unrelated_malformed_patch_does_not_block_recommendation(self, run):
        bad = {"titleId": "OTHER0001", "contentVersion": "01.000.000", "patch": "missing.toml", "targets": {}}
        (self.catalog / "index.json").write_text(json.dumps({"format": 1, "entries": [bad, self.entry]}))
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        self.assertEqual(self.advise(), 0)
        run.assert_called_once()

    @patch.object(suggest_patches.subprocess, "run")
    def test_matching_malformed_patch_is_rejected(self, run):
        self.entry["patch"] = "missing.toml"
        self.write_index()
        self.assertEqual(self.advise(), 1)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_timeout_is_not_a_recommendation(self, run):
        run.side_effect = subprocess.TimeoutExpired([str(self.relinker)], 7)
        self.assertEqual(self.advise("--timeout-seconds", "7"), 1)
        self.assertEqual(run.call_args.kwargs["timeout"], 7)

    @patch.object(suggest_patches.subprocess, "run")
    def test_broken_matching_entry_does_not_block_valid_entry(self, run):
        bad = dict(self.entry, patch="broken.toml")
        (self.catalog / "broken.toml").write_text("format = @")
        self.write_index([bad, self.entry])
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise(), 0)
        self.assertIn("Catalog entry 1 ('broken.toml'): rejected", output.getvalue())
        self.assertIn("Recommended:", output.getvalue())
        run.assert_called_once()

    @patch.object(suggest_patches.subprocess, "run")
    def test_timeout_does_not_block_valid_entry(self, run):
        self.write_index([self.entry, self.entry])
        run.side_effect = [subprocess.TimeoutExpired([str(self.relinker)], 7), subprocess.CompletedProcess([], 0, VERIFIED, "")]
        self.assertEqual(self.advise("--timeout-seconds", "7"), 0)
        self.assertEqual(run.call_count, 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_error_without_verified_entries_exits_two(self, run):
        run.return_value.returncode = 2
        run.return_value.stdout = ""
        run.return_value.stderr = "FAIL: broken input"
        self.assertEqual(self.advise(), 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_error_does_not_block_verified_entry(self, run):
        self.write_index([self.entry, self.entry])
        run.side_effect = [subprocess.CompletedProcess([], 2, "", "FAIL: invalid patch"),
                           subprocess.CompletedProcess([], 0, VERIFIED, "")]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise(), 0)
        self.assertIn("Catalog entry 1 ('patch.toml'): relinker error (exit 2)", output.getvalue())
        self.assertIn("Recommended:", output.getvalue())
        self.assertEqual(run.call_count, 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_error_and_mismatch_exit_two(self, run):
        self.write_index([self.entry, self.entry])
        run.side_effect = [subprocess.CompletedProcess([], 2, "", "FAIL: invalid patch"),
                           subprocess.CompletedProcess([], 1, "Verified 1 patches: 0 ok, 0 not-checked, 1 mismatch\n", "")]
        self.assertEqual(self.advise(), 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_multiple_verified_entries_are_ambiguous(self, run):
        self.write_index([self.entry, self.entry])
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise(), 1)
        self.assertIn("multiple verified catalog entries", output.getvalue())
        self.assertEqual(run.call_count, 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_not_checked_patch_is_not_recommended(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = "Verified 1 patches: 0 ok, 1 not-checked, 0 mismatch\n"
        self.assertEqual(self.advise(), 1)

    @patch.object(suggest_patches.subprocess, "run")
    def test_success_without_verification_summary_is_an_error(self, run):
        run.return_value.returncode = 0
        run.return_value.stdout = "unrecognized verifier"
        self.assertEqual(self.advise(), 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_relinker_argument_error_is_not_a_patch_rejection(self, run):
        run.return_value.returncode = 1
        run.return_value.stdout = ""
        run.return_value.stderr = "FAIL: invalid option"
        self.assertEqual(self.advise(), 2)

    @patch.object(suggest_patches.subprocess, "run")
    def test_conflicting_module_options_are_rejected_before_relinker(self, run):
        self.assertEqual(self.advise("--skip-sce-module", "--sce-module-path", str(self.root)), 2)
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_uppercase_digest_and_conversion_options(self, run):
        self.additional_executable()
        self.entry["targets"]["picker/picker.bin"] = self.entry["targets"]["picker/picker.bin"].upper()
        self.write_index()
        run.return_value.returncode = 0
        run.return_value.stdout = VERIFIED
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise("--executable", "picker/picker.bin", "--sce-module-path", str(self.root)), 0)
        self.assertIn("--patches", output.getvalue())
        self.assertIn("--module-dir Media/Modules", output.getvalue())
        self.assertIn("--executable picker/picker.bin", output.getvalue())
        self.assertIn(f"--sce-module-path {self.root}", output.getvalue())

    @patch.object(suggest_patches.subprocess, "run")
    def test_no_entries_for_title_is_distinct(self, run):
        self.write_index([])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise(), 1)
        self.assertIn("no catalog entry for TEST00001 01.000.000", output.getvalue())
        run.assert_not_called()

    @patch.object(suggest_patches.subprocess, "run")
    def test_invalid_timeout_is_rejected_before_relinker(self, run):
        self.assertEqual(self.advise("--timeout-seconds", "0"), 2)
        run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
