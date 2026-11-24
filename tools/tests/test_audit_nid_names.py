import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SCRIPT = 'audit_nid_names.py'
TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import audit_nid_names


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

    def run_main(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                code = audit_nid_names.main([str(arg) for arg in args])
            except SystemExit as error:
                code = error.code
        return subprocess.CompletedProcess(args, code, stdout.getvalue(), stderr.getvalue())

    def test_hash_consistency_and_read_only_first_record_policy(self):
        path=self.root/"名称.csv"
        path.write_text("6OkOa1SOKuY sceAmprUnknown00\nAAAAAAAAAAA sceAmprUnknown00\n6OkOa1SOKuY ignored_duplicate\n",encoding="utf-8-sig")
        before=path.read_bytes(),path.stat().st_mtime_ns
        r=self.run_tool(path)
        self.assertEqual(r.returncode,1,r.stderr)
        data=json.loads(r.stdout)
        self.assertEqual((data["matching"],data["mismatching"]),(1,1))
        self.assertEqual([x["nid"] for x in data["entries"]],["6OkOa1SOKuY","AAAAAAAAAAA"])
        self.assertTrue(data["entries"][0]["matches"])
        self.assertEqual(data["entries"][1]["computed_nid"],"6OkOa1SOKuY")
        self.assertEqual(before,(path.read_bytes(),path.stat().st_mtime_ns))
        self.assertEqual(list(self.root.iterdir()),[path])
        path.write_text("6OkOa1SOKuY sceAmprUnknown00\n",encoding="utf-8")
        self.assertEqual(self.run_main(path).returncode,0)

    def test_empty_and_invalid_database(self):
        path=self.root/"db"
        path.write_bytes(b"")
        r=self.run_main(path)
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(json.loads(r.stdout),{"entries":[],"matching":0,"mismatching":0})
        path.write_text("bad row\n",encoding="utf-8")
        r=self.run_main(path)
        self.assertEqual(r.returncode,2)
        self.assertEqual(r.stdout,"")

    def test_nonregular_database_is_rejected_before_parsing(self):
        with patch.object(audit_nid_names, "parse_db", side_effect=AssertionError("unexpected parse")):
            result = self.run_main(self.root)
        self.assertEqual((result.returncode, result.stdout), (2, ""))
        self.assertIn("NID database must be a regular file", result.stderr)

    def test_invalid_utf8_is_reported_instead_of_hashing_replacement_characters(self):
        path = self.root / "database.csv"
        path.write_bytes(b"AAAAAAAAAAA name-\xff\n")
        result = self.run_tool(path)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn(str(path), result.stderr)
        self.assertIn("invalid start byte", result.stderr)

    def test_missing_database_reports_usage_error(self):
        r=self.run_main(self.root/"absent.csv")
        self.assertEqual(r.returncode,2)
        self.assertEqual(r.stdout,"")
        self.assertIn("absent.csv",r.stderr)

    def test_missing_argument_is_usage_error(self):
        r=self.run_main()
        self.assertEqual(r.returncode,2)
        self.assertEqual(r.stdout,"")

    def test_entries_sorted_by_nid_with_computed_nid(self):
        path=self.root/"db"
        path.write_text("BBBBBBBBBBB beta\n6OkOa1SOKuY sceAmprUnknown00\n",encoding="utf-8")
        data=json.loads(self.run_main(path).stdout)
        self.assertEqual([x["nid"] for x in data["entries"]],["6OkOa1SOKuY","BBBBBBBBBBB"])
        self.assertEqual(data["entries"][1]["computed_nid"],audit_nid_names.compute_nid("beta"))


if __name__ == "__main__":
    unittest.main()
