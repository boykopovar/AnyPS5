import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

PROGRESS = Path(__file__).resolve().parents[1] / "progress.py"


def snapshot(implemented, library="libSceExample"):
    return {
        "libraries": {
            "done": int(implemented), "total": 1, "percent": 100 if implemented else 0,
            "groups": [{"name": library, "done": int(implemented), "todo": int(not implemented),
                        "done_names": ["Ready"] if implemented else [],
                        "todo_names": [] if implemented else ["Ready"]}],
        },
        "shaders": {"done": 0, "total": 0, "percent": 0, "groups": []},
    }


class ProgressEncodingTests(unittest.TestCase):
    def compare(self, base, head, encoding="cp1252"):
        with tempfile.TemporaryDirectory(prefix="progress encoding ") as directory:
            paths = [Path(directory) / name for name in ("base.json", "head.json")]
            for path, data in zip(paths, (base, head)):
                path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
            env = {**os.environ, "PYTHONIOENCODING": encoding, "PYTHONUTF8": "0"}
            result = subprocess.run([sys.executable, "-X", "utf8=0", str(PROGRESS), "--compare", *map(str, paths)],
                                    env=env, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        return result.stdout.decode("utf-8").replace("\r\n", "\n")

    def test_implemented_report_with_cp1252_stdout(self):
        report = self.compare(snapshot(False), snapshot(True))
        self.assertIn("\U0001f4c8 **System libraries**: 100%", report)
        self.assertIn("\u2705 1 implemented", report)
        self.assertIn("| libSceExample | `Ready` |", report)

    def test_regression_report_with_cp1252_stdout(self):
        report = self.compare(snapshot(True), snapshot(False))
        self.assertIn("\U0001f4c9 **System libraries**: 0%", report)
        self.assertIn("\u26a0\ufe0f 1 went back to stubs", report)
        self.assertIn("-1 reverted", report)

    def test_utf8_snapshot_names_are_preserved(self):
        library = "libSce\u6e2c\u8a66\U0001f3ae"
        report = self.compare(snapshot(False, library), snapshot(True, library))
        self.assertIn(f"| {library} | `Ready` |", report)

    def test_unchanged_snapshots_produce_no_output(self):
        self.assertEqual(self.compare(snapshot(False), snapshot(False)), "")

    def test_utf8_stdout_report_is_unchanged(self):
        base, head = snapshot(False), snapshot(True)
        self.assertEqual(self.compare(base, head, "utf-8"), self.compare(base, head))


if __name__ == "__main__":
    unittest.main()
