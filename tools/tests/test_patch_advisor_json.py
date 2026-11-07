import contextlib
import io
import json
import subprocess
import unittest
from unittest.mock import patch

import test_suggest_patches as fixtures
from test_suggest_patches import suggest_patches, VERIFIED


class PatchAdvisorJsonTests(unittest.TestCase):
    def test_json_directory_resolution_errors_are_controlled(self):
        original = suggest_patches.Path.resolve
        windows_loop = OSError(22, "The name of the file cannot be resolved by the system", "loop")
        windows_loop.winerror = 1921
        for option in ("--catalog", "--sce-module-path"):
            target = self.root / "directory-loop"
            for failure in (RuntimeError("Symlink loop from fixture directory"), windows_loop):
                with self.subTest(option=option, failure=type(failure).__name__):
                    def resolve(path, *args, **kwargs):
                        if path == target:
                            raise failure
                        return original(path, *args, **kwargs)
                    with patch.object(suggest_patches.Path, "resolve", resolve):
                        code, data = self.run_json(option, str(target), side_effect=AssertionError("verification ran"))
                    self.assertEqual((code, data["status"], data["entries"]), (2, "error", []))
                    self.assertIn("Symlink loop", data["error"])

    def test_nonfinite_catalog_and_parameters_emit_strict_json_errors(self):
        def reject_constant(value):
            raise ValueError("non-standard JSON token: " + value)
        for token in ("NaN", "Infinity", "-Infinity", "1e999", "-1e999"):
            for location in ("catalog", "parameters"):
                with self.subTest(token=token, location=location):
                    self.write_index()
                    self.param.write_text(json.dumps({"titleId": "TEST00001", "contentVersion": "01.000.000"}))
                    if location == "catalog":
                        path = self.catalog / "index.json"
                        path.write_text(path.read_text().replace('"patch.toml"', token))
                    else:
                        self.param.write_text('{"titleId":"TEST00001","contentVersion":"01.000.000","extra":' + token + '}')
                    output, errors = io.StringIO(), io.StringIO()
                    with patch.object(suggest_patches.subprocess, "run", side_effect=AssertionError("verification ran")), \
                            contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                        code = self.advise("--json")
                    data = json.loads(output.getvalue(), parse_constant=reject_constant)
                    self.assertEqual((code, data["status"], data["entries"]), (2, "error", []))
                    self.assertIn("non-finite JSON number", data["error"])
                    self.assertNotIn("Traceback", errors.getvalue())

    def run_json(self, *extra, **run):
        output = io.StringIO()
        with patch.object(suggest_patches.subprocess, "run", **run), contextlib.redirect_stdout(output):
            code = self.advise("--json", *extra)
        return code, json.loads(output.getvalue())

    def test_json_records_every_decision(self):
        verified = subprocess.CompletedProcess([], 0, VERIFIED, "warn")
        self.entry["id"] = "fix"
        other = dict(self.entry, id="other")
        stale = dict(self.entry, targets={"Media/Modules/module.prx": "0" * 64})
        invalid = dict(self.entry, category="unknown")
        self.write_index([self.entry, stale, invalid, other, other, dict(self.entry, id="slow")])
        code, data = self.run_json(side_effect=[verified, verified, verified, subprocess.TimeoutExpired([], 7)])
        self.assertEqual(code, 0)
        self.assertEqual(data["status"], "recommended")
        self.assertEqual(data["recommended_entry"], 1)
        first, second, third, fourth, fifth, sixth = data["entries"]
        self.assertEqual(first, {"number": 1, "patch": "patch.toml", "status": "verified",
                                 "targets": self.entry["targets"], "metadata": {"id": "fix"},
                                 "verification": {"exit_code": 0, "summary": [1, 1, 0, 0],
                                                  "stdout": VERIFIED, "stderr": "warn"}})
        self.assertEqual((second["status"], second["reason"].startswith("Media/Modules/module.prx")), ("rejected", True))
        self.assertEqual((third["status"], third["reason"]), ("rejected", "invalid category"))
        for record in (fourth, fifth):
            self.assertEqual((record["status"], record["reason"]), ("rejected", "duplicate verified id 'other'"))
        self.assertEqual((sixth["status"], sixth["reason"]), ("rejected", "verification_timeout"))

    def test_json_failure_statuses(self):
        self.write_index([dict(self.entry, contentVersion="02.000.000")])
        self.assertEqual(self.run_json(), (1, {"status": "no_match", "entries": []}))
        self.write_index()
        code, data = self.run_json(return_value=subprocess.CompletedProcess([], 2, "", "FAIL: invalid patch"))
        self.assertEqual((code, data["status"], data["error"]), (2, "error", "relinker errors occurred"))
        self.param.write_text("[]")
        self.assertEqual(self.run_json(), (2, {"status": "error", "entries": [], "error": "param.json must be an object"}))


    setUp = fixtures.PatchAdvisorTests.setUp
    write_index = fixtures.PatchAdvisorTests.write_index
    advise = fixtures.PatchAdvisorTests.advise

    def test_json_recommendation_and_hash_rejection(self):
        result = subprocess.CompletedProcess([], 0, VERIFIED, "")
        output = io.StringIO()
        with patch.object(suggest_patches.subprocess, "run", return_value=result), contextlib.redirect_stdout(output):
            self.assertEqual(self.advise("--json"), 0)
        data = json.loads(output.getvalue())
        self.assertEqual(data["status"], "recommended")
        self.assertEqual(data["entries"][0]["verification"]["summary"], [1, 1, 0, 0])
        self.assertEqual(data["entries"][0]["targets"], self.entry["targets"])
        self.module.write_bytes(b"changed module")
        output = io.StringIO()
        with patch.object(suggest_patches.subprocess, "run", side_effect=AssertionError("unmatched hash")), contextlib.redirect_stdout(output):
            self.assertEqual(self.advise("--json"), 1)
        self.assertEqual(json.loads(output.getvalue())["entries"][0]["status"], "rejected")

    def test_json_error_and_multiple_matches(self):
        self.write_index([self.entry, self.entry])
        with patch.object(suggest_patches.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, VERIFIED, "")):
            for extra, status in (([], "multiple_matches"), (["--list-verified"], "verified_list")):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(self.advise("--json", *extra), int(not extra))
                self.assertEqual(json.loads(output.getvalue())["status"], status)
        self.param.write_text("invalid json")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(self.advise("--json"), 2)
        self.assertEqual(json.loads(output.getvalue())["status"], "error")


if __name__ == "__main__":
    unittest.main()
