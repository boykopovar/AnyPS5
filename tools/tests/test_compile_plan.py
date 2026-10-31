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

    def test_compile_database_plan_preserves_variants_without_execution(self):
        import check_compile
        from unittest.mock import patch
        import contextlib
        import io
        source = self.root / "source.cpp"
        source.write_text("int x;\n")
        build = self.root / "build"
        build.mkdir()
        entries = [{"directory": str(self.root), "file": str(source), "arguments": ["compiler-that-must-not-run", "-DNAME=" + name, "-c", str(source), "-o", "forbidden.o"]} for name in ("ONE", "TWO")]
        (build / "compile_commands.json").write_text(json.dumps(entries))
        output = io.StringIO()
        with patch.object(check_compile, "repository", return_value=self.root), patch.object(check_compile.subprocess, "run", side_effect=AssertionError("executed compiler")), contextlib.redirect_stdout(output):
            code = check_compile.main(["--build", str(build), "--dry-run", str(source)])
        self.assertEqual(code, 0)
        rows = json.loads(output.getvalue())["translation_units"]
        self.assertEqual(len(rows[0]["commands"]), 2)
        for row in rows[0]["commands"]:
            self.assertIn("-fsyntax-only", row["arguments"])
            self.assertNotIn("-o", row["arguments"])
            self.assertEqual(row["directory"], str(self.root))
        self.assertFalse((self.root / "forbidden.o").exists())

    def test_empty_selection_is_json_without_reading_build(self):
        import check_compile
        from unittest.mock import patch
        import contextlib
        import io
        output = io.StringIO()
        with patch.object(check_compile, "repository", return_value=self.root), patch.object(check_compile, "changed", return_value=[]), contextlib.redirect_stdout(output):
            code = check_compile.main(["--dry-run", "--build", str(self.root / "missing")])
        self.assertEqual((code, json.loads(output.getvalue())), (0, {"translation_units": []}))


if __name__ == "__main__":
    unittest.main()
