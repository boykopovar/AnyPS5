import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from nid_names import compute_nid


class AbstractTcsExportNamesTests(unittest.TestCase):
    def test_legacy_exports_preserve_nids_and_recovered_names(self):
        module = ROOT / "core/libs/prx/libSceAbstractTcs"
        source = (module / "Export.cpp").read_text(encoding="utf-8")
        header = (module / "include/LegacyExports.hpp").read_text(encoding="utf-8")
        exports = re.findall(
            r'APS5_EXPORT\("([A-Za-z0-9+\-]{11})",\s*([A-Za-z_][A-Za-z0-9_]*)\)', source
        )

        self.assertEqual(len(exports), 40)
        self.assertEqual(len({nid for nid, _ in exports}), 40)

        recovered = [(nid, name) for nid, name in exports if name.startswith("_ZN15AbstractStorage")]
        self.assertGreaterEqual(len(recovered), 37)
        for nid, name in recovered:
            with self.subTest(nid=nid):
                self.assertEqual(compute_nid(name), nid)
                self.assertRegex(header, rf"\b{re.escape(name)}\s*\(")


if __name__ == "__main__":
    unittest.main()
