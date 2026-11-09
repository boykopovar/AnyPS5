import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

DECL = "+int APS5_VABI sceFooBar(int a,\n+    int b);\n"


class WrappedExportTests(unittest.TestCase):
    def test_wrapped_declaration_is_not_export(self):
        self.assertEqual(pr_overlap.exports(DECL), set())

    def test_wrapped_definition_is_export(self):
        diff = "+int APS5_VABI sceFooBar(int a,\n+    int b) {\n+    return a + b;\n+}\n"
        self.assertEqual(pr_overlap.exports(diff), {"sceFooBar"})

    def test_declaration_then_definition_returns_only_definition(self):
        diff = DECL + "+int APS5_VABI sceBaz(int a) { return a; }\n"
        self.assertEqual(pr_overlap.exports(diff), {"sceBaz"})

    def test_declaration_at_end_of_diff(self):
        self.assertEqual(pr_overlap.exports("+int APS5_VABI sceFooBar(int a, int b);"), set())


if __name__ == "__main__":
    unittest.main()
