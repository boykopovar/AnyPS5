import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nid_names import compute_nid

CHECKER = Path(__file__).resolve().parents[1] / "check_conventions.py"
HELPER_NID = compute_nid("Helper")


def unknown_export(nid):
    return (f'APS5_EXPORT("{nid}", sceBUnknown_{nid});\n'
            f"int APS5_VABI sceBUnknown_{nid}() {{\n    NotImplemented_nid_no_patch(__func__);\n    return 0;\n}}\n")


class UnknownNidTests(unittest.TestCase):
    def check(self, base_files, head_files):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                     "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                    cwd=root, capture_output=True, text=True, check=True,
                )

            def commit(files, message):
                for name, text in files.items():
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(text, encoding="utf-8")
                git("add", ".")
                git("commit", "-qm", message)

            git("init", "-q")
            commit(base_files, "feat: add the base libraries")
            commit(head_files, "feat: change a library")
            env = {key: value for key, value in os.environ.items()
                   if key not in ("GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY", "GITHUB_ACTIONS")}
            env.update(GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="safe.directory", GIT_CONFIG_VALUE_0=str(root))
            return subprocess.run(
                [sys.executable, "-I", str(CHECKER), "--base", "HEAD~1", "--head", "HEAD"],
                cwd=root, capture_output=True, text=True, env=env, timeout=20,
            )

    def assertUnknownNid(self, result, symbol):
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("[unknown-nid]", result.stdout)
        self.assertIn(symbol, result.stdout)

    def assertPasses(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("[unknown-nid]", result.stdout)

    def test_unknown_export_with_the_nid_of_a_helper_in_another_library_fails(self):
        result = self.check(
            {"core/libs/prx/libSceA/Export.cpp": 'extern "C" bool Helper_nid_postfix() {\n    return true;\n}\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
        )
        self.assertUnknownNid(result, f"{HELPER_NID} (sceBUnknown_{HELPER_NID}) is Helper_nid_postfix at core/libs/prx/libSceA/Export.cpp:1")

    def test_unknown_export_with_the_nid_of_an_import_declaration_fails(self):
        result = self.check(
            {"core/libs/prx/libSceB/include/Helper.hpp": 'extern "C" bool Helper_nid_postfix();\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
        )
        self.assertUnknownNid(result, "Helper_nid_postfix at core/libs/prx/libSceB/include/Helper.hpp:1")

    def test_unknown_export_with_the_nid_of_a_named_export_fails(self):
        result = self.check(
            {"core/libs/prx/libSceA/Export.cpp": "int APS5_VABI sceAFoo() {\n    return 0;\n}\n"},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(compute_nid("sceAFoo"))},
        )
        self.assertUnknownNid(result, "sceAFoo at core/libs/prx/libSceA/Export.cpp:1")

    def test_unknown_export_with_the_nid_of_an_assembly_function_fails(self):
        result = self.check(
            {"core/libs/prx/libSceA/Asm.cpp": 'asm(\n    APS5_ASM_FUNCTION("Helper_nid_postfix")\n    "ret\\n");\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
        )
        self.assertUnknownNid(result, "Helper_nid_postfix at core/libs/prx/libSceA/Asm.cpp:2")

    def test_unknown_export_with_the_nid_of_a_disambiguated_helper_fails(self):
        result = self.check(
            {"core/libs/prx/libSceA/Export.cpp": 'extern "C" bool Helper_nid_disambig1_nid_postfix() {\n    return true;\n}\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
        )
        self.assertUnknownNid(result, "Helper_nid_disambig1_nid_postfix")

    def test_helper_with_the_nid_of_an_unknown_export_fails(self):
        result = self.check(
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
            {"core/libs/prx/libSceA/Export.cpp": 'extern "C" bool Helper_nid_postfix() {\n    return true;\n}\n'},
        )
        self.assertUnknownNid(result, f"Helper_nid_postfix has the NID {HELPER_NID} of sceBUnknown_{HELPER_NID} at core/libs/prx/libSceB/Unimplemented.cpp:1")

    def test_unknown_export_with_an_unused_nid_passes(self):
        self.assertPasses(self.check(
            {"core/libs/prx/libSceA/Export.cpp": 'extern "C" bool Helper_nid_postfix() {\n    return true;\n}\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(compute_nid("sceBStillUnknown"))},
        ))

    def test_no_patch_helper_does_not_take_the_nid(self):
        self.assertPasses(self.check(
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
            {"core/libs/prx/libSceA/Export.cpp": 'extern "C" bool Helper_nid_no_patch() {\n    return true;\n}\n'},
        ))

    def test_named_alias_of_another_library_export_passes(self):
        nid = compute_nid("sceAFoo")
        self.assertPasses(self.check(
            {"core/libs/prx/libSceA/Export.cpp": "int APS5_VABI sceAFoo() {\n    return 0;\n}\n"},
            {"core/libs/prx/libSceB/Export.cpp": f'APS5_EXPORT("{nid}", libSceB_sceAFoo);\nint APS5_VABI libSceB_sceAFoo() {{\n    return 0;\n}}\n'},
        ))

    def test_symbols_in_library_tests_are_ignored(self):
        self.assertPasses(self.check(
            {"core/libs/prx/libSceA/tests/Helper.cpp": 'extern "C" bool Helper_nid_postfix() {\n    return true;\n}\n'},
            {"core/libs/prx/libSceB/Unimplemented.cpp": unknown_export(HELPER_NID)},
        ))


if __name__ == "__main__":
    unittest.main()
