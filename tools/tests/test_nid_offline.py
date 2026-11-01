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

    def test_missing_offline_cache_never_downloads_or_creates_directories(self):
        import nid_names
        from unittest.mock import patch
        import contextlib
        import io
        path = self.root / "absent" / "db.csv"
        with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=AssertionError("network access")), contextlib.redirect_stderr(io.StringIO()):
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
        with patch.object(Path, "is_file", return_value=True), patch.object(nid_names.urllib.request, "urlretrieve", side_effect=AssertionError("network access")), contextlib.redirect_stderr(io.StringIO()):
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
