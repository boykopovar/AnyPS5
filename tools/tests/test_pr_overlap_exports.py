import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap
import nid_names

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

    def test_added_definitions_match_base_names_with_nid_postfix(self):
        nid = nid_names.compute_nid("sceKernelRead")
        base = {("core/libs/prx/libkernel", nid): {"sceKernelRead_nid_postfix"}}
        added = pr_overlap.source_changed_exports(
            "diff --git a/core/libs/prx/libkernel/a.cpp b/core/libs/prx/libkernel/a.cpp\n+int APS5_VABI sceKernelRead(int fd) { return fd; }\n", "+")
        self.assertEqual(set(added) & set(base), {("core/libs/prx/libkernel", nid)})

    def test_added_assembly_export_matches_base_definition(self):
        nid = nid_names.compute_nid("sceKernelRead")
        added = pr_overlap.source_changed_exports(
            'diff --git a/a.cpp b/a.cpp\n+APS5_ASM_FUNCTION("sceKernelRead")\n', "+")
        self.assertIn(("", nid), added)

    def test_added_raw_export_matches_base_definition(self):
        nid = nid_names.compute_nid("sceKernelRead")
        added = pr_overlap.source_changed_exports(
            f'diff --git a/a.cpp b/a.cpp\n+APS5_EXPORT("{nid}", sceKernelReadAlias);\n', "+")
        self.assertEqual(added[("", nid)], {"sceKernelReadAlias"})

    def test_raw_main_export_is_preserved_as_literal_nid(self):
        nid = "123456789ab"
        self.assertEqual(pr_overlap.source_exports(f'APS5_EXPORT("{nid}", alias);'), {("", nid): {"alias"}})

    def test_replacing_an_export_in_place_cancels_the_same_nid(self):
        diff = """diff --git a/a.cpp b/a.cpp
-int APS5_VABI sceKernelRead_nid_postfix(int fd) { throw std::runtime_error("not implemented"); }
+int APS5_VABI sceKernelRead(int fd) { return fd; }
"""
        added = pr_overlap.source_changed_exports(diff, "+")
        removed = pr_overlap.source_changed_exports(diff, "-")
        self.assertEqual(set(added) - set(removed), set())

    def test_exports_with_same_nid_in_different_libraries_do_not_overlap(self):
        nid = nid_names.compute_nid("sceKernelRead")
        added = pr_overlap.source_changed_exports(
            "diff --git a/core/libs/prx/libSceAmpr/a.cpp b/core/libs/prx/libSceAmpr/a.cpp\n"
            "+int APS5_VABI sceKernelRead(int fd) { return fd; }\n", "+")
        base = {("core/libs/prx/libkernel", nid): {"sceKernelRead"}}
        self.assertFalse(set(added) & set(base))

    def test_main_exports_have_a_separate_report_section_not_a_fake_pr(self):
        nid = nid_names.compute_nid("sceKernelRead")
        prs = {9: {"added_exports": {("core/libs/prx/libkernel", nid): {"sceKernelRead"}}, "removed_exports": {}}}
        findings = pr_overlap.main_export_findings(prs, {("core/libs/prx/libkernel", nid): {"sceKernelRead"}})
        rows = {9: {"added": set()}}
        report = pr_overlap.report(rows, "https://example.invalid/blob/base", findings[9])
        self.assertIn("Exports introduced here that are already present in `main`", report)
        self.assertNotIn("PR 0", report)
        self.assertNotIn("PR 9", report)
        self.assertNotIn("Already on `main` |", report)
        self.assertEqual(rows, {9: {"added": set()}})


    def test_explicit_export_aliases_are_collected_by_library_and_nid(self):
        source = ('int APS5_VABI placeholder_nid_no_patch() {\n'
                  '    NotImplemented_nid_no_patch(__func__);\n'
                  '}\n'
                  'APS5_EXPORT("Pz262plj20w", placeholder_nid_no_patch);\n')
        self.assertEqual(
            pr_overlap.source_exports(source, "core/libs/prx/libSceExample"),
            {("core/libs/prx/libSceExample", "Pz262plj20w"): {"placeholder_nid_no_patch"}},
        )

    def test_explicit_alias_and_named_export_share_a_key_in_the_same_library(self):
        explicit = pr_overlap.source_exports(
            'APS5_EXPORT("Pz262plj20w", placeholder_nid_no_patch);',
            "core/libs/prx/libSceExample",
        )
        named = pr_overlap.source_changed_exports(
            "diff --git a/core/libs/prx/libSceExample/a.cpp b/core/libs/prx/libSceExample/a.cpp\n"
            "+int APS5_VABI FT_Outline_GetInsideBorder() { return 0; }\n",
            "+",
        )
        self.assertEqual(set(explicit) & set(named), {("core/libs/prx/libSceExample", "Pz262plj20w")})


    def test_overlaps_reports_same_exports_for_different_names_with_matching_nid(self):
        nid = "vPP6vKHY8Tg"
        prs = {
            1: {
                "author": "author1",
                "depends": set(),
                "added": set(),
                "exports": {"_sceSdmaNop"},
                "added_exports": {("core/libs/prx/libSceSdma", nid): {"_sceSdmaNop"}},
                "tree": None,
            },
            2: {
                "author": "author2",
                "depends": set(),
                "added": set(),
                "exports": set(),
                "added_exports": {("core/libs/prx/libSceSdma", nid): {"sceSdmaPending_001_nid_no_patch"}},
                "tree": None,
            },
        }
        found = pr_overlap.overlaps(prs, 1, 2)
        self.assertIsNotNone(found)
        self.assertEqual(found["exports"], {"_sceSdmaNop", "sceSdmaPending_001_nid_no_patch"})

    def test_overlaps_does_not_report_export_conflict_for_shared_dependency(self):
        nid = "vPP6vKHY8Tg"
        prs = {
            1: {
                "author": "author1",
                "depends": {3},
                "added": set(),
                "exports": {"_sceSdmaNop"},
                "added_exports": {("core/libs/prx/libSceSdma", nid): {"_sceSdmaNop"}},
                "tree": None,
            },
            2: {
                "author": "author2",
                "depends": {3},
                "added": set(),
                "exports": {"_sceSdmaNop"},
                "added_exports": {("core/libs/prx/libSceSdma", nid): {"_sceSdmaNop"}},
                "tree": None,
            },
            3: {
                "author": "author3",
                "depends": set(),
                "added": set(),
                "exports": {"_sceSdmaNop"},
                "added_exports": {("core/libs/prx/libSceSdma", nid): {"_sceSdmaNop"}},
                "tree": None,
            },
        }
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))


if __name__ == "__main__":
    unittest.main()
