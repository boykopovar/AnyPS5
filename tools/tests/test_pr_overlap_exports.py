import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

TRAILING = "+extern \"C\" std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int) noexcept;   \n"
DIFF = """diff --git a/core/libs/prx/libkernel/File/src/Open.cpp b/core/libs/prx/libkernel/File/src/Open.cpp
@@ -96,0 +97,2 @@
+extern "C" int* APS5_VABI __error_nid_postfix();
""" + TRAILING + """@@ -135,0 +136,5 @@
+int APS5_VABI mlock_nid_postfix(const void* address, std::size_t length) {
+std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) noexcept {
+int APS5_VABI getpid_nid_postfix() { return 1; }
+int APS5_VABI sceNpUniversalDataSystemCreateEvent(
+void APS5_VABI Helper_nid_no_patch(int value) {
@@ -200 +205,0 @@
-int APS5_VABI removed_nid_postfix(int) {
diff --git a/core/libs/prx/libkernel/File/src/Stdio.cpp b/core/libs/prx/libkernel/File/src/Stdio.cpp
@@ -10,2 +10,2 @@
 int APS5_VABI context_nid_postfix(int) {
"""


class ExportTests(unittest.TestCase):
    def test_added_definitions_are_exports(self):
        self.assertEqual(pr_overlap.exports(DIFF), {"mlock", "sceKernelRead", "getpid", "sceNpUniversalDataSystemCreateEvent"})

    def test_nid_postfix_aliases_match_plain_export_names(self):
        suffixed = "+int APS5_VABI sceExample_nid_postfix() { return 0; }\n"
        plain = "+int APS5_VABI sceExample() { return 0; }\n"
        self.assertEqual(pr_overlap.exports(suffixed), pr_overlap.exports(plain))

    def test_nid_disambiguation_suffix_is_normalized(self):
        diff = "+int APS5_VABI sceExample_nid_disambig12_nid_postfix() { return 0; }\n"
        self.assertEqual(pr_overlap.exports(diff), {"sceExample"})

    def test_cut_helpers_are_not_export_overlaps(self):
        diff = DIFF + '+void APS5_VABI Helper_nid_no_patch_cut() {\n'
        self.assertEqual(pr_overlap.exports(diff), pr_overlap.exports(DIFF))

    def test_declarations_are_not_exports(self):
        self.assertTrue(TRAILING.endswith(";   \n"))
        found = pr_overlap.exports(DIFF)
        self.assertNotIn("__error_nid_postfix", found)
        self.assertNotIn("recv_nid_postfix", found)

    def test_removed_context_and_helper_lines_are_not_exports(self):
        found = pr_overlap.exports(DIFF)
        self.assertNotIn("removed_nid_postfix", found)
        self.assertNotIn("context_nid_postfix", found)
        self.assertNotIn("Helper_nid_no_patch", found)


    def test_explicit_export_aliases_are_collected(self):
        diff = ('+int APS5_VABI placeholder_nid_no_patch() {\n'
                '+    NotImplemented_nid_no_patch(__func__);\n'
                '+}\n'
                '+APS5_EXPORT("Pz262plj20w", placeholder_nid_no_patch);\n'
                '-APS5_EXPORT("sKneJjUXWkQ", removed_nid_no_patch);\n')
        self.assertEqual(pr_overlap.exports(diff), set())
        self.assertEqual(pr_overlap.aliases(diff), {"Pz262plj20w"})
        self.assertEqual(pr_overlap.aliases(diff.replace('+APS5_EXPORT("Pz262plj20w", placeholder_nid_no_patch);\n', "")), set())

    def test_explicit_alias_overlaps_named_export_with_the_same_nid(self):
        named = {"author": "a", "exports": {"FT_Outline_GetInsideBorder"}, "aliases": set()}
        explicit = {"author": "b", "exports": set(), "aliases": {"Pz262plj20w"}}
        self.assertEqual(pr_overlap.same_exports(named, explicit, []), {"FT_Outline_GetInsideBorder"})
        self.assertEqual(pr_overlap.same_exports(explicit, dict(explicit, author="c"), []), {"Pz262plj20w"})
        self.assertEqual(pr_overlap.same_exports(named, explicit, [explicit]), set())


if __name__ == "__main__":
    unittest.main()
