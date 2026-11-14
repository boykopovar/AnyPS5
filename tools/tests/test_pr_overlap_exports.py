import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import contextmanager
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
        candidate_nid = nid_names.compute_nid("FT_Stroker_New")
        existing_nid = nid_names.compute_nid("legacyStrokerAlias")
        prs = {9: {"added_exports": {
            ("core/libs/prx/libSceFreeTypeOt", candidate_nid): {"FT_Stroker_New_nid_postfix"}},
            "removed_exports": {
                ("core/libs/prx/libSceFreeTypeOt", candidate_nid): {"sceFreeTypeOtStrokerNew_nid_no_patch"}}}}
        main = {("core/libs/prx/libSceFreeType", existing_nid): {"FT_Stroker_New_nid_postfix"}}
        findings = pr_overlap.main_export_findings(prs, main)
        rows = {9: {"added": set()}}
        report = pr_overlap.report(rows, "https://example.invalid/blob/base", findings[9])
        self.assertIn("also exported by another library in `main`", report)
        self.assertIn("`FT_Stroker_New`", report)
        self.assertIn("libSceFreeType", report)
        self.assertNotIn("PR 0", report)
        self.assertNotIn("PR 9", report)
        self.assertNotIn("Already on `main` |", report)
        self.assertEqual(rows, {9: {"added": set()}})

    def test_same_library_and_nid_already_on_main_is_reported(self):
        nid = nid_names.compute_nid("sceKernelRead")
        prs = {9: {"added_exports": {
            ("core/libs/prx/libkernel", nid): {"sceKernelRead"}},
            "removed_exports": {}}}
        main = {("core/libs/prx/libkernel", nid): {"sceKernelRead_nid_postfix"}}
        findings = pr_overlap.main_export_findings(prs, main)
        self.assertEqual(findings[9], {
            "sceKernelRead": {
                "same_key": {("core/libs/prx/libkernel", nid)},
                "other_libraries": set(),
            },
        })
        report = pr_overlap.report({9: {"added": set()}}, "https://example.invalid/blob/base", findings[9])
        self.assertIn("same library/NID already present", report)
        self.assertIn("libkernel", report)
        self.assertIn(nid, report)

    def test_same_function_name_in_only_the_same_library_is_not_a_cross_library_duplicate(self):
        first_nid = nid_names.compute_nid("FT_Stroker_New")
        second_nid = nid_names.compute_nid("legacyStrokerAlias")
        prs = {9: {"added_exports": {
            ("core/libs/prx/libSceFreeTypeOt", first_nid): {"FT_Stroker_New_nid_postfix"}},
            "removed_exports": {}}}
        main = {("core/libs/prx/libSceFreeTypeOt", second_nid): {"FT_Stroker_New_nid_postfix"}}
        self.assertEqual(pr_overlap.main_export_findings(prs, main), {})


    def test_rewritten_definition_line_is_not_a_main_duplicate(self):
        nid = nid_names.compute_nid("sceKernelRead")
        key = ("core/libs/prx/libkernel", nid)
        prs = {9: {"added_exports": {key: {"sceKernelRead"}}, "removed_exports": {key: {"sceKernelRead"}}}}
        self.assertEqual(pr_overlap.main_export_findings(prs, {key: {"sceKernelRead"}}), {})

    def test_export_moved_between_libraries_is_not_a_cross_library_duplicate(self):
        nid = nid_names.compute_nid("sem_setname")
        old = ("core/libs/prx/libkernel_sys", nid)
        new = ("core/libs/prx/libkernel", nid)
        prs = {9: {"added_exports": {new: {"sem_setname"}}, "removed_exports": {old: {"sem_setname"}}}}
        self.assertEqual(pr_overlap.main_export_findings(prs, {old: {"sem_setname"}}), {})

    def test_rewritten_export_does_not_hide_a_new_cross_library_duplicate(self):
        nid = nid_names.compute_nid("sceKernelRead")
        original = ("core/libs/prx/libkernel", nid)
        other = ("core/libs/prx/libSceExample", nid)
        prs = {9: {"added_exports": {
            original: {"sceKernelRead_nid_postfix"},
            other: {"sceKernelRead"}},
            "removed_exports": {original: {"sceKernelRead_nid_postfix"}}}}
        main = {original: {"sceKernelRead_nid_postfix"}}
        self.assertEqual(pr_overlap.main_export_findings(prs, main), {
            9: {"sceKernelRead": {
                "same_key": set(),
                "other_libraries": {"core/libs/prx/libkernel"},
            }},
        })

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


class DiffPathTests(unittest.TestCase):
    def test_file_markers_preserve_c_quoted_paths_on_both_sides(self):
        cases = [
            ('core/libs/prx/libExample/part b/Source.cpp', 'core/libs/prx/libExample/part b/Source.cpp'),
            ('"core/libs/prx/libExample/tab\\tfile.cpp"', 'core/libs/prx/libExample/tab\tfile.cpp'),
            ('"core/libs/prx/libExample/quote\\\"file.cpp"', 'core/libs/prx/libExample/quote"file.cpp'),
            ('"core/libs/prx/lib\\303\\251/Source.cpp"', 'core/libs/prx/libé/Source.cpp'),
            ('"core/libs/prx/libé/line\\nfile.cpp"', 'core/libs/prx/libé/line\nfile.cpp'),
            ('core/libs/prx/lib\u0085Example/Source.cpp', 'core/libs/prx/lib\u0085Example/Source.cpp'),
            ('core/libs/prx/lib\u2028Example/Source.cpp', 'core/libs/prx/lib\u2028Example/Source.cpp'),
        ]
        for encoded, expected in cases:
            for prefix, marker in (("+", "+++"), ("-", "---")):
                with self.subTest(path=encoded, prefix=prefix):
                    leader = 'b/' if prefix == '+' else 'a/'
                    encoded_path = '"' + leader + encoded[1:] if encoded.startswith('"') else leader + encoded
                    diff = (f'diff --git a/ambiguous b/name b/ambiguous b/name\n{marker} {encoded_path}\n'
                            f'@@ -1 +1 @@\n{prefix}int APS5_VABI sceExample() {{ return 0; }}\n')
                    self.assertEqual(pr_overlap.changed_source(diff, prefix), [
                        (expected, 'int APS5_VABI sceExample() { return 0; }'),
                    ])

    def test_increment_and_decrement_lines_do_not_replace_file_markers(self):
        for prefix, operation, marker in (("+", "++ value;", "+++"), ("-", "-- value;", "---")):
            with self.subTest(prefix=prefix):
                diff = (f'diff --git a/first b/wrong b/first b/wrong\n{marker} b/core/libs/prx/libExample/Source.cpp\n'
                        f'@@ -1 +1 @@\n{prefix}{operation}\n{prefix}int APS5_VABI sceExample() {{ return 0; }}\n')
                found = pr_overlap.source_changed_exports(diff, prefix)
                self.assertIn(("core/libs/prx/libExample", nid_names.compute_nid("sceExample")), found)

    def test_crlf_patch_preserves_carriage_return_in_a_quoted_path(self):
        diff = ('diff --git a/ambiguous b/path b/ambiguous b/path\r\n'
                '+++ "b/core/libs/prx/libExample/line\\rname.cpp"\r\n'
                '@@ -0,0 +1 @@\r\n+int APS5_VABI sceExample() { return 0; }\r\n')
        self.assertEqual(pr_overlap.changed_source(diff, "+"), [
            ("core/libs/prx/libExample/line\rname.cpp", 'int APS5_VABI sceExample() { return 0; }'),
        ])


class GitDiffPathTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.addCleanup(self.temporary.cleanup)
        self.git("init", "-q")

    def git(self, *args):
        return subprocess.run([
            "git", "-c", "user.name=Diff Path Test", "-c", "user.email=test@example.com",
            "-c", "commit.gpgsign=false", *args,
        ], cwd=self.root, capture_output=True, text=True, encoding="utf-8", check=True).stdout

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as stream:
            stream.write(text)

    def commit(self):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "test fixture")
        return self.git("rev-parse", "HEAD").strip()

    @contextmanager
    def in_repository(self):
        previous = os.getcwd()
        try:
            os.chdir(self.root)
            yield
        finally:
            os.chdir(previous)

    def scan(self, base, head, author=None):
        self.git("update-ref", "refs/pr/base", base)
        with self.in_repository():
            pr = {"ref": head, "author": author, "depends": set()}
            pr_overlap.scan(pr)
            probed = pr_overlap.probe(pr)
            findings = pr_overlap.main_export_findings({1: pr}, pr_overlap.base_exports(base))
            return pr, probed, findings

    def relocation(self, old, new, source, alias_name):
        nid = nid_names.compute_nid(alias_name)
        library = pr_overlap.library_for_path(new)
        self.write(old, source)
        self.write(f"{library}/Existing.cpp", f'int APS5_VABI sceExisting() {{ return 0; }}\nAPS5_EXPORT("{nid}", sceExisting);\n')
        base = self.commit()
        (self.root / new).parent.mkdir(parents=True, exist_ok=True)
        self.git("mv", old, new)
        head = self.commit()
        self.assertIn("rename from ", self.git("diff", "-U0", base, head))
        return base, head, nid

    def test_unchanged_library_move_reports_an_existing_target_nid(self):
        base, head, nid = self.relocation(
            "core/libs/prx/libOld/Move.cpp", "core/libs/prx/libNew/Move.cpp",
            "int APS5_VABI sceMoved() { return 0; }\n", "sceMoved")
        pr, probed, findings = self.scan(base, head)
        self.assertIn(("core/libs/prx/libNew", nid), pr["added_exports"])
        self.assertIn(("core/libs/prx/libOld", nid), pr["removed_exports"])
        self.assertIn(("core/libs/prx/libNew", nid), probed[1][1])
        self.assertIn("sceMoved", findings.get(1, {}))

    def test_partly_edited_library_move_keeps_unchanged_export_identities(self):
        padding = "".join(f"int padding{i} = {i};\n" for i in range(30))
        source = padding + "int APS5_VABI sceChanged() { return 0; }\nint APS5_VABI sceUnchanged() { return 0; }\n"
        base, _, nid = self.relocation("core/libs/prx/libOld/Move.cpp", "core/libs/prx/libNew/Move.cpp", source, "sceUnchanged")
        self.write("core/libs/prx/libNew/Move.cpp", source.replace("sceChanged() { return 0; }", "sceChanged() { return 1; }"))
        head = self.commit()
        self.assertIn("rename from ", self.git("diff", "-U0", base, head))
        pr, probed, findings = self.scan(base, head)
        self.assertIn(("core/libs/prx/libNew", nid), pr["added_exports"])
        self.assertIn(("core/libs/prx/libOld", nid), pr["removed_exports"])
        self.assertIn(("core/libs/prx/libNew", nid), probed[1][1])
        self.assertIn("sceUnchanged", findings.get(1, {}))

    def test_library_move_preserves_literal_wildcards_and_unicode_separators(self):
        old = "core/libs/prx/libOld/part b/[ab]/Move.cpp"
        library = "core/libs/prx/libNew\u2028Name"
        base, head, nid = self.relocation(old, f"{library}/part b/[ab]/Move.cpp",
                                          "int APS5_VABI sceMoved() { return 0; }\n", "sceMoved")
        pr, probed, findings = self.scan(base, head)
        self.assertIn((library, nid), pr["added_exports"])
        self.assertIn((library, nid), probed[1][1])
        self.assertIn("sceMoved", findings.get(1, {}))

    def test_unchanged_same_library_move_does_not_claim_an_export_overlap(self):
        old = "core/libs/prx/libOne/Old.cpp"
        self.write(old, "int APS5_VABI sceKept() { return 0; }\n")
        base = self.commit()
        self.git("mv", old, "core/libs/prx/libOne/New.cpp")
        head = self.commit()
        pr, probed, findings = self.scan(base, head)
        self.assertEqual(pr["exports"], set())
        self.assertEqual(pr["added_exports"], {})
        self.assertEqual(pr["removed_exports"], {})
        self.assertEqual(probed[1], (set(), {}))
        self.assertEqual(findings, {})
        self.git("checkout", "-q", "--detach", base)
        self.write(old, "int APS5_VABI sceKept() { return 1; }\n")
        other, _, _ = self.scan(base, self.commit(), "other")
        with self.in_repository():
            self.assertIsNone(pr_overlap.overlaps({0: pr, 1: other}, 0, 1))

    def test_test_callback_move_between_libraries_remains_excluded(self):
        base, head, _ = self.relocation("core/libs/prx/libOld/tests/Move.cpp", "core/libs/prx/libNew/tests/Move.cpp",
                                         "int APS5_VABI sceMoved() { return 0; }\n", "sceMoved")
        pr, probed, findings = self.scan(base, head)
        self.assertEqual(pr["exports"], set())
        self.assertEqual(pr["added_exports"], {})
        self.assertEqual(pr["removed_exports"], {})
        self.assertEqual(probed[1], (set(), {}))
        self.assertEqual(findings, {})

    def check_alias_duplicate(self, library, name, quoted):
        original = "int APS5_VABI sceOriginal() { return 0; }\n"
        self.write(f"{library}/Original.cpp", original)
        base = self.commit()
        nid = nid_names.compute_nid("sceOriginal")
        self.write(f"{library}/{name}", f'int APS5_VABI sceAlias() {{ return 0; }}\nAPS5_EXPORT("{nid}", sceAlias);\n')
        head = self.commit()
        diff = self.git("-c", f"core.quotePath={str(quoted).lower()}", "diff", "-U0", base, head)
        added = pr_overlap.source_changed_exports(diff, "+")
        self.assertIn((library, nid), added)
        findings = pr_overlap.main_export_findings(
            {1: {"added_exports": added, "removed_exports": {}}}, pr_overlap.source_exports(original, library))
        self.assertIn("sceAlias", findings.get(1, {}))

    def test_real_git_path_containing_diff_delimiter_keeps_library_identity(self):
        self.check_alias_duplicate("core/libs/prx/libExample", "part b/Source.cpp", False)

    def test_real_git_quoted_utf8_library_keeps_library_identity(self):
        self.check_alias_duplicate("core/libs/prx/libé", "Source.cpp", True)

    def test_real_git_unquoted_unicode_separator_keeps_library_identity(self):
        self.check_alias_duplicate("core/libs/prx/libLine\u2028Break", "Source.cpp", False)

    def test_renamed_definition_uses_its_source_and_destination_libraries(self):
        old = "core/libs/prx/libOld/Source.cpp"
        new = "core/libs/prx/libNew/Source.cpp"
        padding = "".join(f"int padding{i} = {i};\n" for i in range(30))
        self.write(old, padding + "int APS5_VABI sceMoved() { return 0; }\n")
        base = self.commit()
        (self.root / new).parent.mkdir(parents=True)
        self.git("mv", old, new)
        self.write(new, padding + "int APS5_VABI sceMoved() { return 1; }\n")
        head = self.commit()
        diff = self.git("diff", "-U0", base, head)
        self.assertIn("rename from ", diff)
        key = nid_names.compute_nid("sceMoved")
        self.assertIn(("core/libs/prx/libOld", key), pr_overlap.source_changed_exports(diff, "-"))
        self.assertIn(("core/libs/prx/libNew", key), pr_overlap.source_changed_exports(diff, "+"))


if __name__ == "__main__":
    unittest.main()
