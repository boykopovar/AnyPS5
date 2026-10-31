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

    def test_real_elf_and_source_classification_control_partial_policy(self):
        from test_import_audit import build_elf, write_registry
        import nid_names
        libs, source = self.root / "libs", self.root / "source"
        libs.mkdir()
        library = source / "libSceExample"
        library.mkdir(parents=True)
        (library / "CMakeLists.txt").write_text("target_sources(libSceExample PRIVATE Export.cpp)\n")
        name = "sceExamplePartial"
        nid = nid_names.compute_nid(name)
        (libs / "libSceExample.prx").write_bytes(build_elf([(nid, 1, 0x12)]))
        cpp = library / "Export.cpp"
        cpp.write_text('int APS5_VABI sceExamplePartial(int mode) { if (mode) NotImplemented_nid_no_patch(__func__); return 0; }\n')
        registry = self.root / "app.registry.json"
        write_registry(registry, [(nid, "libSceExample.prx")])
        report = self.root / "report.json"
        common = [registry, "--libs", libs, "--source", source, "--json", report]
        ordinary = self.cli("import_audit.py", *common)
        strict = self.cli("import_audit.py", *common, "--strict-partial")
        self.assertEqual(ordinary.returncode, 0, ordinary.stderr)
        self.assertEqual(strict.returncode, 1, strict.stderr)
        self.assertEqual(json.loads(report.read_text())["unique_by_class"]["partial"], 1)
        cpp.write_text('int APS5_VABI sceExamplePartial(int mode) { return mode; }\n')
        self.assertEqual(self.cli("import_audit.py", *common, "--strict-partial").returncode, 0)

    def test_invalid_registry_keeps_audit_error_status(self):
        registry = self.root / "bad.json"
        registry.write_text("{")
        result = self.cli("import_audit.py", registry, "--libs", self.root, "--strict-partial")
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
