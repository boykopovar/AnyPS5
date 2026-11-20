import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def assert_invalid_cache_rejected_in_both_modes(self, content):
        db = self.root / "db.csv"
        db.write_bytes(content)
        before = sorted(path.relative_to(self.root).as_posix() for path in self.root.rglob("*"))
        results = []
        for offline in (False, True):
            with self.subTest(offline=offline):
                arguments = ["--db", str(db), "--nid", "AAAAAAAAAAA", "--json"]
                if offline:
                    arguments.append("--offline")
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).resolve().parents[1] / "nid_names.py"), *arguments],
                    capture_output=True, encoding="utf-8", timeout=15,
                    env={key: value for key, value in os.environ.items() if key.upper() in ("SYSTEMROOT", "SYSTEMDRIVE")})
                results.append(result)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertEqual(result.stderr.splitlines()[-1],
                                 f"nid_names.py: error: {db}: database contains no valid NID records")
                self.assertNotIn("Traceback", result.stderr)
                self.assertNotIn("downloading", result.stderr)
                self.assertEqual(db.read_bytes(), content)
                self.assertEqual(sorted(path.relative_to(self.root).as_posix() for path in self.root.rglob("*")), before)
        self.assertEqual(results[0].stderr, results[1].stderr)

    def test_empty_cache_is_rejected_in_both_modes(self):
        self.assert_invalid_cache_rejected_in_both_modes(b"")

    def test_malformed_cache_is_rejected_in_both_modes(self):
        self.assert_invalid_cache_rejected_in_both_modes(b"not a valid nid\nanother bad line\n")

    def test_valid_mixed_cache_preserves_exact_results_in_both_modes(self):
        import nid_names
        from unittest.mock import patch
        import contextlib
        import io
        name = "sceKernelUsleep"
        nid = nid_names.compute_nid(name)
        unknown = "AAAAAAAAAAA"
        db = self.root / "db.csv"
        content = f"{nid} {name}\nnot a valid nid\n".encode()
        db.write_bytes(content)
        prx = self.root / "core" / "libs" / "prx"
        prx.mkdir(parents=True)
        (prx / "Example.cpp").write_text("int APS5_VABI " + name + "() { return 0; }\n")
        before = {path.relative_to(self.root).as_posix(): path.read_bytes()
                  for path in self.root.rglob("*") if path.is_file()}
        expected = {"db_entries": 2, "rows": [
            {"nid": unknown, "stub": "<cli>", "suggestion": "", "status": "unknown"},
            {"nid": nid, "stub": "<cli>", "suggestion": name, "status": "name-in-use"}]}
        expected["rows"].sort(key=lambda row: row["nid"])
        for offline in (False, True):
            with self.subTest(offline=offline):
                output, errors = io.StringIO(), io.StringIO()
                arguments = ["--db", str(db), "--nid", nid, unknown, "--json"]
                if offline:
                    arguments.append("--offline")
                with patch.object(nid_names, "ROOT", self.root), patch.object(nid_names, "PRX", prx), \
                        patch.object(nid_names.urllib.request, "urlopen", side_effect=AssertionError("network access")), \
                        contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    code = nid_names.main(arguments)
                self.assertEqual(code, 0)
                self.assertEqual(json.loads(output.getvalue()), expected)
                self.assertEqual(errors.getvalue(), "")
                self.assertEqual({path.relative_to(self.root).as_posix(): path.read_bytes()
                                  for path in self.root.rglob("*") if path.is_file()}, before)

    def test_name_computation_and_empty_nid_need_no_offline_cache(self):
        import nid_names
        name = "sceKernelUsleep"
        db = self.root / "absent" / "db.csv"
        cases = [(["--name", name], {"symbols": [{"name": name, "nid": nid_names.compute_nid(name)}]}),
                 (["--nid"], {"db_entries": 0, "rows": []})]
        for arguments, expected in cases:
            with self.subTest(arguments=arguments):
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).resolve().parents[1] / "nid_names.py"),
                     "--offline", "--db", str(db), "--json", *arguments],
                    capture_output=True, encoding="utf-8", timeout=15,
                    env={key: value for key, value in os.environ.items() if key.upper() in ("SYSTEMROOT", "SYSTEMDRIVE")})
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout), expected)
                self.assertEqual(result.stderr, "")
                self.assertEqual(list(self.root.iterdir()), [])

    def test_missing_offline_cache_never_downloads_or_creates_directories(self):
        import nid_names
        from unittest.mock import patch
        import contextlib
        import io
        path = self.root / "absent" / "db.csv"
        with patch.object(nid_names.urllib.request, "urlopen", side_effect=AssertionError("network access")), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as caught:
                nid_names.main(["--offline", "--db", str(path), "--nid", "AAAAAAAAAAA"])
        self.assertEqual(caught.exception.code, 2)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_cache_disappearing_after_the_file_check_cannot_trigger_download(self):
        import nid_names
        from unittest.mock import patch
        import contextlib
        import io
        path = self.root / "absent" / "db.csv"
        with patch.object(Path, "is_file", return_value=True), patch.object(nid_names.urllib.request, "urlopen", side_effect=AssertionError("network access")), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as caught:
                nid_names.main(["--offline", "--db", str(path), "--nid", "AAAAAAAAAAA"])
        self.assertEqual(caught.exception.code, 2)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_cached_names_and_name_computation_keep_existing_results(self):
        import nid_names
        db = self.root / "db.csv"
        name = "sceKernelUsleep"
        nid = nid_names.compute_nid(name)
        db.write_text(nid + " " + name + "\n")
        from unittest.mock import patch
        import contextlib
        import io
        prx = self.root / "core" / "libs" / "prx"
        prx.mkdir(parents=True)
        (prx / "Example.cpp").write_text("int APS5_VABI " + name + "() { return 0; }\n")
        output = io.StringIO()
        with patch.object(nid_names, "ROOT", self.root), patch.object(nid_names, "PRX", prx), contextlib.redirect_stdout(output):
            code = nid_names.main(["--offline", "--db", str(db), "--nid", nid, "--json"])
        self.assertEqual(code, 0)
        row = json.loads(output.getvalue())["rows"][0]
        self.assertEqual((row["suggestion"], row["status"]), (name, "name-in-use"))
        result = self.cli("nid_names.py", "--offline", "--db", self.root / "missing", "--name", name, "--json")
        self.assertEqual(json.loads(result.stdout)["symbols"], [{"name": name, "nid": nid}])


if __name__ == "__main__":
    unittest.main()
