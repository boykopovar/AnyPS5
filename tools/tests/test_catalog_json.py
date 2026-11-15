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

    def test_real_catalog_keeps_all_entry_errors_and_successes(self):
        import hashlib
        catalog = self.root / "catalog"
        catalog.mkdir()
        patch = catalog / "patch.toml"
        patch.write_text('format = 1\n[[patch]]\ntarget = "module.prx"\nva = 0x1000\nexpected = "00"\nvalue = "01"\n')
        entry = {"titleId": "TEST00001", "contentVersion": "01.000.000", "patch": "patch.toml", "targets": {"module.prx": hashlib.sha256(b"module").hexdigest()}}
        index = catalog / "index.json"
        index.write_text(json.dumps({"format": 1, "entries": [None, entry, dict(entry, patch="missing.toml")]}))
        before = {p.name: p.read_bytes() for p in catalog.iterdir()}
        result = self.cli("validate_patch_catalog.py", "--catalog", catalog, "--json")
        self.assertEqual(result.returncode, 2, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual((data["total"], data["invalid"]), (3, 2))
        self.assertEqual([x["valid"] for x in data["entries"]], [False, True, False])
        self.assertIsNone(data["entries"][1]["error"])
        self.assertEqual(before, {p.name: p.read_bytes() for p in catalog.iterdir()})
        index.write_text(json.dumps({"format": 1, "entries": [entry]}))
        self.assertEqual(self.cli("validate_patch_catalog.py", "--catalog", catalog, "--json").returncode, 0)

    def test_bad_index_is_structured_and_not_a_valid_empty_catalog(self):
        (self.root / "index.json").write_text("{")
        result = self.cli("validate_patch_catalog.py", "--catalog", self.root, "--json")
        self.assertEqual(result.returncode, 2)
        self.assertIn("error", json.loads(result.stdout))
        entry = {"titleId": "TEST00001", "contentVersion": "01.000.000", "patch": float("nan"), "targets": {}}
        (self.root / "index.json").write_text(json.dumps({"format": 1, "entries": [entry]}))
        result = self.cli("validate_patch_catalog.py", "--catalog", self.root, "--json")
        self.assertEqual(result.returncode, 2)
        def reject(value):
            self.fail("non-JSON constant: " + value)
        data = json.loads(result.stdout, parse_constant=reject)
        self.assertIsNone(data["entries"][0]["patch"])
        self.assertEqual(data["invalid"], 1)


if __name__ == "__main__":
    unittest.main()
