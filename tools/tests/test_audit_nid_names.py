import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'audit_nid_names.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

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
        self.assertEqual(self.run_tool(path).returncode,0)

    def test_empty_and_invalid_database(self):
        path=self.root/"db"
        path.write_bytes(b"")
        r=self.run_tool(path)
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(json.loads(r.stdout),{"entries":[],"matching":0,"mismatching":0})
        path.write_text("bad row\n",encoding="utf-8")
        r=self.run_tool(path)
        self.assertEqual(r.returncode,2)
        self.assertEqual(r.stdout,"")


if __name__ == "__main__":
    unittest.main()
