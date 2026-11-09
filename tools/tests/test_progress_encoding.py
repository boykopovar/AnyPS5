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
                                    env=env, capture_output=True, timeout=60)
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

    def test_site_generation_preserves_unicode_with_cp1252_stdout(self):
        library = "libSce\u6e2c\u8a66\U0001f3ae"
        with tempfile.TemporaryDirectory(prefix="progress site encoding ") as directory:
            root = Path(directory) / "source"
            module = root / "core" / "libs" / "prx" / library
            module.mkdir(parents=True)
            (module / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n", encoding="utf-8")
            opcodes = root / "core" / "shader" / "recompiler" / "RdnaDecoder" / "include" / "RdnaDecoder" / "RdnaOpcode.hpp"
            opcodes.parent.mkdir(parents=True)
            opcodes.write_text("enum class RdnaOpcode {\n    SEndpgm,\n};\n", encoding="utf-8")
            output = Path(directory) / "site"
            env = {**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0"}
            result = subprocess.run(
                [sys.executable, "-X", "utf8=0", str(PROGRESS), str(output), "--root", str(root),
                 "--module", library, "--json"], env=env, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
            generated = {name: (output / name).read_text(encoding="utf-8") for name in (
                "progress.json", "badge-libraries.svg", "badge-shaders.svg", "progress.svg", "progress.html")}
            payload = json.loads(generated["progress.json"])
            self.assertEqual(payload, json.loads(result.stdout.decode("utf-8")))
            self.assertEqual((payload["libraries"]["done"], payload["libraries"]["total"]), (1, 1))
            self.assertEqual(payload["libraries"]["groups"][0]["name"], library)
            for name in ("progress.svg", "progress.html"):
                self.assertIn(library.removeprefix("libSce"), generated[name])


if __name__ == "__main__":
    unittest.main()
