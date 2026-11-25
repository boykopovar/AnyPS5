import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
TOOL = TOOLS / "relinker_seed_manifest.py"


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


class DescribeRefusalTests(unittest.TestCase):
    def test_describe_refuses_a_directory(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('relinker_seed_manifest', TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'not a regular file'):
                module.describe(Path(directory))


if __name__ == "__main__":
    unittest.main()
