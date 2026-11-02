import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'pe_symbol_inventory.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

    def test_code_symbols_and_export_fallback(self):
        sys.path.insert(0,str(Path(__file__).parent))
        from test_symbolize_samples import build_pe, SIZE_OF_IMAGE, TIMESTAMP
        path=self.root/"模块.prx"
        build_pe(path)
        before=path.read_bytes(),path.stat().st_mtime_ns
        r=self.run_tool(path)
        self.assertEqual(r.returncode,0,r.stderr)
        data=json.loads(r.stdout)
        self.assertEqual((data["image_base"],data["size"],data["timestamp"],data["symbol_source"]),(0x140000000,SIZE_OF_IMAGE,TIMESTAMP,"coff"))
        self.assertEqual(data["code"],[{"rva":0x1000,"end":0x1100,"symbols":[
            {"rva":0x1010,"name":"short"},{"rva":0x1040,"name":"AgcDriver_long_function_name"},{"rva":0x1080,"name":"static_f"}]}])
        self.assertEqual(before,(path.read_bytes(),path.stat().st_mtime_ns))
        build_pe(path,with_symbols=False,with_exports=True)
        data=json.loads(self.run_tool(path).stdout)
        self.assertEqual(data["symbol_source"],"exports")
        self.assertEqual([x["name"] for x in data["code"][0]["symbols"]],["first_func","second_func"])

    def test_malformed_images_fail_without_traceback(self):
        path=self.root/"bad.prx"
        for data in (b"",b"MZ",bytes(128)):
            path.write_bytes(data)
            r=self.run_tool(path)
            self.assertEqual(r.returncode,2)
            self.assertEqual(r.stdout,"")
            self.assertNotIn("Traceback",r.stderr)


if __name__ == "__main__":
    unittest.main()
