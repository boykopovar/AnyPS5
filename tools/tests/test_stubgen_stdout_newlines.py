from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

tools = Path(__file__).resolve().parents[1]


class StubgenStdoutNewlineTests(unittest.TestCase):
    def run_stubgen(self, *extra):
        with tempfile.TemporaryDirectory() as d:
            nids = Path(d) / "nids.txt"
            nids.write_bytes(b"AC1FtjqMCL0\n")
            return subprocess.run(
                [sys.executable, str(tools / "stubgen.py"), "--module", "libkernel", "--nids", str(nids)] + list(extra),
                capture_output=True,
                check=True,
            )

    def test_code_on_stdout_uses_lf(self):
        result = self.run_stubgen()
        self.assertIn(b"\n", result.stdout)
        self.assertNotIn(b"\r\n", result.stdout)

    def test_td_only_on_stdout_uses_lf(self):
        result = self.run_stubgen("--td-only")
        self.assertIn(b"\n", result.stdout)
        self.assertNotIn(b"\r\n", result.stdout)

    def test_td_on_stderr_uses_lf(self):
        result = self.run_stubgen()
        self.assertIn(b"\n", result.stderr)
        self.assertNotIn(b"\r\n", result.stderr)


if __name__ == "__main__":
    unittest.main()
