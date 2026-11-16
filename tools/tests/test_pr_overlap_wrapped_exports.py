import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nid_names
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

    def test_rewritten_first_line_of_wrapped_export_is_removed(self):
        diff = ("diff --git a/core/libs/prx/libSceNpCommon/Strings.cpp b/core/libs/prx/libSceNpCommon/Strings.cpp\n"
                "@@ -40 +40 @@\n"
                "-int APS5_VABI sceNpUInt32ToStr(char* buf, std::size_t size,\n"
                "+int APS5_VABI sceNpUInt32ToStr(char* buf, std::size_t length,\n")
        key = ("core/libs/prx/libSceNpCommon", nid_names.compute_nid("sceNpUInt32ToStr"))
        added = pr_overlap.source_changed_exports(diff, "+")
        removed = pr_overlap.source_changed_exports(diff, "-")
        self.assertIn(key, added)
        self.assertIn(key, removed)
        self.assertEqual(pr_overlap.main_export_findings(
            {9: {"added_exports": added, "removed_exports": removed}},
            {key: {"sceNpUInt32ToStr"}},
        ), {})

    def test_rewritten_one_line_signature_with_brace_on_next_line_is_removed(self):
        diff = ("diff --git a/core/libs/prx/libSceNpCommon/Strings.cpp b/core/libs/prx/libSceNpCommon/Strings.cpp\n"
                "@@ -60,3 +60,3 @@\n"
                "-int APS5_VABI sceNpGetOnlineId(int userId)\n"
                "+int APS5_VABI sceNpGetOnlineId(int userId, int flags)\n"
                " {\n"
                "     return 0;\n"
                " }\n")
        key = ("core/libs/prx/libSceNpCommon", nid_names.compute_nid("sceNpGetOnlineId"))
        added = pr_overlap.source_changed_exports(diff, "+")
        removed = pr_overlap.source_changed_exports(diff, "-")
        self.assertIn(key, added)
        self.assertIn(key, removed)
        self.assertEqual(pr_overlap.main_export_findings(
            {9: {"added_exports": added, "removed_exports": removed}},
            {key: {"sceNpGetOnlineId"}},
        ), {})


if __name__ == "__main__":
    unittest.main()
