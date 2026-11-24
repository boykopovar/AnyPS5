import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
TOOL = TOOLS / "relinker_seed_manifest.py"
sys.path.insert(0, str(TOOLS))
import relinker_seed_manifest


class SeedManifestTests(unittest.TestCase):
    def test_generated_corpus_hashes_and_read_only_repetition(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "corpus"
            subprocess.run([sys.executable, str(TOOLS / "relinker_fuzz_seeds.py"), str(output)], check=True, capture_output=True)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            result = subprocess.run([sys.executable, str(TOOL), str(output)], check=True, capture_output=True, text=True)
            report = json.loads(result.stdout)
            seeds = report["seeds"]
            self.assertGreater(len(seeds), 0)
            self.assertEqual([row["name"] for row in seeds], sorted(before))
            selectors = set()
            for row in seeds:
                data = before[row["name"]]
                self.assertEqual(len(data), row["size"])
                self.assertEqual(hashlib.sha256(data).hexdigest(), row["sha256"])
                self.assertEqual(data[0], row["selector"])
                selectors.add(row["selector"])
            self.assertEqual(selectors, {0, 1, 2, 4})
            repeated = subprocess.run([sys.executable, str(TOOL), str(output)], check=True, capture_output=True, text=True)
            self.assertEqual(json.loads(repeated.stdout), report)
            self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, before)

    def test_refuses_to_describe_a_seed_changed_during_hashing(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "seed"
            path.write_bytes(b"seed")
            before = path.stat()
            after = SimpleNamespace(st_size=before.st_size, st_mtime_ns=before.st_mtime_ns + 1,
                                    st_ctime_ns=before.st_ctime_ns)
            with patch.object(os, "fstat", side_effect=[before, after]):
                with self.assertRaisesRegex(ValueError, "corpus entry changed while reading"):
                    relinker_seed_manifest.describe(path)

    def test_empty_arbitrary_and_multichunk_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def report():
                result = subprocess.run([sys.executable, str(TOOL), directory], check=True, capture_output=True, text=True)
                return json.loads(result.stdout)["seeds"]
            self.assertEqual(report(), [])
            data = bytes([37]) + bytes(range(256)) * 9000
            (root / "a-日").write_bytes(data)
            (root / "empty").write_bytes(b"")
            self.assertEqual(report(), [
                {"name": "a-日", "selector": 37, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()},
                {"name": "empty", "selector": None, "size": 0, "sha256": hashlib.sha256(b"").hexdigest()}])
            (root / "nested").mkdir()
            result = subprocess.run([sys.executable, str(TOOL), directory], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, "")
            self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
