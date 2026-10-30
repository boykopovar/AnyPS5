import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CHECKER = Path(__file__).resolve().parents[1] / "check_conventions.py"
sys.path.insert(0, str(CHECKER.parent))
import check_conventions

DEFINITION = "int APS5_VABI sceFooBar(int a, int b) { return a + b; }\n"


class WrappedDeclarationTests(unittest.TestCase):
    def check(self, first_file, first_text, second_text=DEFINITION):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                     "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            git("init", "-q")
            first = root / "core/libs/prx/libSceA" / first_file
            first.parent.mkdir(parents=True)
            first.write_text(first_text, encoding="utf-8")
            other = root / "core/libs/prx/libSceA/Other.cpp"
            other.write_text("int APS5_VABI sceOther() { return 1; }\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add library a")
            second = root / "core/libs/prx/libSceB/Export.cpp"
            second.parent.mkdir(parents=True)
            second.write_text(second_text, encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "feat: add library b")
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run(
                [sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
                cwd=root, capture_output=True, text=True, env=env, timeout=20,
            )

    def test_wrapped_declaration_is_not_a_duplicate(self):
        result = self.check("Export.h", "int APS5_VABI sceFooBar(int a,\n    int b);\n")
        self.assertNotIn("duplicate-export", result.stdout, result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_non_ascii_declaration_path_does_not_crash_duplicate_scan(self):
        result = self.check("caf\u00e9.cpp", "int APS5_VABI sceFooBar(int a, int b);\n")
        self.assertNotIn("duplicate-export", result.stdout, result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cut_helpers_in_different_libraries_are_not_duplicate_nids(self):
        helper = "int APS5_VABI Support_nid_no_patch_cut() { return 3; }\n"
        result = self.check("Export.cpp", helper, helper)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[duplicate-export]", result.stdout)

    def test_wrapped_definition_is_a_duplicate(self):
        result = self.check("Export.cpp", "int APS5_VABI sceFooBar(int a,\n    int b) {\n    return 0;\n}\n")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[duplicate-export]", result.stdout)

    def test_local_abi_alias_cannot_hide_duplicates_in_either_library(self):
        aliased = "#define NATIVE_SYSV APS5_VABI\n" + DEFINITION.replace("APS5_VABI", "NATIVE_SYSV")
        for first, second in ((aliased, DEFINITION), (DEFINITION, aliased), (aliased, aliased)):
            with self.subTest(first=first, second=second):
                result = self.check("Export.cpp", first, second)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("[duplicate-export]", result.stdout)
                self.assertIn("sceFooBar is also in libSceA", result.stdout)

    def test_chained_alias_uses_the_current_macro_definition(self):
        source = "#define ENTRY ABI\n#define ABI APS5_VABI\n" + DEFINITION.replace("APS5_VABI", "ENTRY")
        for prefix in ("", "\ufeff"):
            with self.subTest(prefix=prefix):
                result = self.check("Export.cpp", prefix + source)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("[duplicate-export]", result.stdout)

    def test_alias_undefinition_redefinition_and_cycles_do_not_create_exports(self):
        cases = ("#define ABI APS5_VABI\n#undef ABI\n",
                 "#define ABI APS5_VABI\n#define ABI unrelated\n",
                 "#define ABI APS5_VABI\n#define ABI() APS5_VABI\n",
                 "#define ABI OTHER\n#define OTHER ABI\n",
                 "/*\n#define ABI APS5_VABI\n*/\n",
                 'const char* example = R"(\n#define ABI APS5_VABI\n)";\n')
        for prefix in cases:
            with self.subTest(prefix=prefix):
                source = check_conventions.expand_abi_aliases(check_conventions.cpp_text(prefix + DEFINITION.replace("APS5_VABI", "ABI")))
                self.assertNotRegex(source, r"\bAPS5_VABI\s+sceFooBar\s*\(")

    def test_cpp_text_does_not_expand_abi_aliases(self):
        source = "#define ABI APS5_VABI\nint ABI sceFooBar() { return 1; }\n"
        self.assertIn("int ABI sceFooBar", check_conventions.cpp_text(source))

    def test_aliased_import_declaration_is_not_a_duplicate(self):
        source = "#define ABI APS5_VABI\nint ABI sceFooBar(int a, int b);\n"
        result = self.check("Export.h", source)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_local_abi_alias_cannot_hide_an_undocumented_silent_stub(self):
        source = "#define ABI APS5_VABI\nint ABI sceUndocumented() { return 0; }\n"
        result = self.check("Export.cpp", DEFINITION, source)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[silent-stub]", result.stdout)

    def test_unique_aliased_definition_and_unpatched_helpers_remain_valid(self):
        for name in ("sceDistinct", "Support_nid_no_patch", "Support_nid_no_patch_cut"):
            with self.subTest(name=name):
                source = "#define ABI APS5_VABI\nint ABI " + name + "() { return 3; }\n"
                result = self.check("Export.cpp", source, source if name.startswith("Support") else DEFINITION)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_multiline_raw_literal_is_not_a_duplicate(self):
        source = 'const char* example = R"example(\n' + DEFINITION + ')example";\n'
        result = self.check("Export.cpp", source)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[duplicate-export]", result.stdout)

    def test_block_comment_is_not_a_duplicate(self):
        result = self.check("Export.cpp", "/*\n" + DEFINITION + "*/\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[duplicate-export]", result.stdout)

    def test_literal_before_unrelated_export_is_not_a_duplicate(self):
        source = 'const char* example = "APS5_VABI sceFooBar()"; int APS5_VABI sceOther() { return 1; }\n'
        result = self.check("Export.cpp", source)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[duplicate-export]", result.stdout)

    def test_line_comment_before_definition_is_not_a_duplicate(self):
        result = self.check("Export.cpp", "// " + DEFINITION + "int unrelated() { return 42; }\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[duplicate-export]", result.stdout)

    def test_definition_after_unicode_raw_literal_is_a_duplicate(self):
        source = 'const char* text = R"text(first\u2028+middle\u2029+last)text";\n' + DEFINITION
        result = self.check("Export.cpp", source)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[duplicate-export]", result.stdout)

    def test_definition_after_commented_example_is_a_duplicate(self):
        result = self.check("Export.cpp", "/*\n" + DEFINITION + "*/\n" + DEFINITION)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(result.stdout.count("[duplicate-export]"), 1, result.stdout)


if __name__ == "__main__":
    unittest.main()
