import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'compare_configure_checks.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

    def test_added_removed_changed_and_read_only(self):
        a,b=self.root/"旧.txt",self.root/"新.txt"
        a.write_text("//Have include a\nA:INTERNAL=1\n//Test B\nB:INTERNAL=0\n//Test C\nC:INTERNAL=1\n",encoding="utf-8-sig")
        b.write_text("//Have include a\nA:INTERNAL=2\n//Test C changed\nC:INTERNAL=1\n//Test D\nD:INTERNAL=1\n",encoding="utf-8")
        before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in self.root.iterdir()}
        result=self.run_tool(a,b)
        self.assertEqual(result.returncode,1,result.stderr)
        data=json.loads(result.stdout)
        self.assertEqual(data["added"],[{"name":"D","value":"1","help":"Test D"}])
        self.assertEqual(data["removed"],[{"name":"B","value":"0","help":"Test B"}])
        self.assertEqual([c["name"] for c in data["changed"]],["A","C"])
        self.assertEqual(data["changed"][0]["after"]["value"],"2")
        self.assertEqual(before,{p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in self.root.iterdir()})
        self.assertEqual(self.run_tool(a,a).returncode,0)

    def test_empty_and_invalid_inputs(self):
        a,b=self.root/"a",self.root/"b"
        a.write_text("OPT:BOOL=ON\n",encoding="utf-8")
        b.write_bytes(b"")
        r=self.run_tool(a,b)
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(json.loads(r.stdout),{"added":[],"removed":[],"changed":[]})
        b.write_bytes(b"\xff")
        r=self.run_tool(a,b)
        self.assertEqual(r.returncode,2)
        self.assertEqual(r.stdout,"")

    def test_value_changes_after_unicode_separators_are_reported(self):
        baseline, candidate = self.root / "baseline", self.root / "candidate"
        for separator in ("\u0085", "\u2028", "\u2029"):
            with self.subTest(separator=separator):
                before, after = f"prefix{separator}before", f"prefix{separator}after"
                baseline.write_text(f"//Test FEATURE\nFEATURE:INTERNAL={before}\n", encoding="utf-8")
                candidate.write_text(f"//Test FEATURE\nFEATURE:INTERNAL={after}\n", encoding="utf-8")
                result = self.run_tool(baseline, candidate)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(json.loads(result.stdout), {
                    "added": [], "removed": [], "changed": [{
                        "name": "FEATURE", "before": {"value": before, "help": "Test FEATURE"},
                        "after": {"value": after, "help": "Test FEATURE"},
                    }],
                })

    def test_missing_file_is_a_usage_error(self):
        a = self.root / "a"
        a.write_text("OPT:BOOL=ON\n", encoding="utf-8")
        for args in ((a, self.root / "missing"), (self.root / "missing", a)):
            with self.subTest(args=args):
                result = self.run_tool(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertTrue(result.stderr)

    def test_help_only_change_is_reported_with_same_value(self):
        a, b = self.root / "a", self.root / "b"
        a.write_text("//Test old help\nX:INTERNAL=1\n", encoding="utf-8")
        b.write_text("//Test new help\nX:INTERNAL=1\n", encoding="utf-8")
        result = self.run_tool(a, b)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(json.loads(result.stdout)["changed"], [{
            "name": "X", "before": {"value": "1", "help": "Test old help"},
            "after": {"value": "1", "help": "Test new help"},
        }])

    def test_variable_probe_change_is_reported(self):
        baseline, candidate = self.root / "baseline", self.root / "candidate"
        for cache, value in ((baseline, "1"), (candidate, "")):
            cache.write_text(f"//Have variable timezone\nHAVE_TIMEZONE:INTERNAL={value}\n", encoding="utf-8")
        result = self.run_tool(baseline, candidate)
        self.assertEqual(result.returncode, 1, result.stderr)
        change, = json.loads(result.stdout)["changed"]
        self.assertEqual((change["name"], change["before"]["value"], change["after"]["value"]),
                         ("HAVE_TIMEZONE", "1", ""))

    def test_added_and_removed_are_sorted_by_name(self):
        a, b = self.root / "a", self.root / "b"
        a.write_text("//Test Z\nZ:INTERNAL=1\n//Test M\nM:INTERNAL=1\n//Test A\nA:INTERNAL=1\n", encoding="utf-8")
        b.write_text("//Test Y\nY:INTERNAL=1\n//Test B\nB:INTERNAL=1\n", encoding="utf-8")
        data = json.loads(self.run_tool(a, b).stdout)
        self.assertEqual([c["name"] for c in data["added"]], ["B", "Y"])
        self.assertEqual([c["name"] for c in data["removed"]], ["A", "M", "Z"])


if __name__ == "__main__":
    unittest.main()
