import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'public_headers_manifest.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PYTHONIOENCODING": "ascii"})

    def fixture(self):
        directory = self.root / "core/libs/prx/libExample"
        directory.mkdir(parents=True)
        (directory / "libExample.hpp").write_bytes(b'#include "Api.hpp"\n')
        (directory / "Api.hpp").write_bytes(b'#include <cstdint>\n')
        (self.root / "LICENSE").write_bytes(b"example license\n")
        return directory

    def test_manifest_matches_recursive_package_bytes(self):
        import hashlib
        self.fixture()
        before = {str(p.relative_to(self.root)): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()}
        result = self.run_tool("--root", self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["headers"], 2)
        expected = []
        for path, (data, _) in before.items():
            path = Path(path).as_posix()
            destination = "LICENSE" if path == "LICENSE" else "include/anyps5/" + path.removeprefix("core/libs/")
            expected.append({"path": destination, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        self.assertEqual(report["entries"], sorted(expected, key=lambda item: item["path"]))
        self.assertEqual(before, {str(p.relative_to(self.root)): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()})

    def test_missing_dependency_and_license_fail_without_json(self):
        directory = self.fixture()
        (directory / "Api.hpp").unlink()
        self.assertEqual(self.run_tool("--root", self.root).returncode, 2)
        (directory / "Api.hpp").write_bytes(b"")
        (self.root / "LICENSE").write_bytes(b"")
        result = self.run_tool("--root", self.root)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
