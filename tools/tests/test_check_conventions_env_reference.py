import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
from check_conventions import Check

DOC = "docs/dev/ENVIRONMENT.md"
SAMPLE = "core/libs/prx/libSceExample/Sample.cpp"
DOC_TEXT = "| `APS5_DOCUMENTED` | presence |\n| `APS5_GONE` | presence |\n"


def repository(root, base, head, target=None):
    def git(*args):
        subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                        "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                       cwd=root, capture_output=True, check=True)

    def apply(files):
        for path, text in files.items():
            target = root / path
            if text is None:
                target.unlink()
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding="utf-8")

    git("init", "-q")
    apply(base)
    git("add", ".")
    git("commit", "-qm", "docs: add base")
    git("tag", "base")
    if target is not None:
        git("branch", "pull")
        apply(target)
        git("add", "-A")
        git("commit", "-qm", "docs: advance the target")
        git("tag", "target")
        git("checkout", "-q", "pull")
    apply(head)
    git("add", "-A")
    git("commit", "-qm", "feat: read a new name")


def findings_of(root, base, head, ref="base", directory="."):
    previous = os.getcwd()
    os.chdir(root / directory)
    try:
        check = Check(ref, "HEAD")
        check.env_reference()
        return check.findings
    finally:
        os.chdir(previous)


class EnvReferenceDiffTests(unittest.TestCase):
    def run_case(self, base, head, target=None, directory="."):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        repository(root, base, head, target)
        return findings_of(root, base, head, "base" if target is None else "target", directory)

    def test_a_check_from_a_subdirectory_reports_the_same_findings(self):
        other = "core/libs/prx/libSceExample/Other.cpp"
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
            other: 'static bool b = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
        }
        target = {DOC: DOC_TEXT + "| `APS5_TARGET_ROW` | presence |\n"}
        head = {
            DOC: "| `APS5_DOCUMENTED` | presence |\n",
            SAMPLE: base[SAMPLE] + 'static bool c = std::getenv("APS5_NEW_READ") != nullptr;\n'
                    'static bool d = std::getenv("APS5_TARGET_ROW") != nullptr;\n',
            other: None,
        }
        expected = [("env-reference", SAMPLE, 2, f"APS5_NEW_READ is read here but has no row in {DOC}")]
        self.assertEqual(self.run_case(base, head, target), expected)
        for directory in ("core", "core/libs/prx/libSceExample", "docs/dev"):
            with self.subTest(directory=directory):
                self.assertEqual(self.run_case(base, head, target, directory), expected)

    def test_only_a_name_the_pull_request_adds_is_reported_at_its_line(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n'
                    'static bool b = std::getenv("APS5_OLD_UNDOCUMENTED") != nullptr;\n',
        }
        added = 'static bool c = std::getenv("APS5_NEW_READ") != nullptr;\n'
        head = {SAMPLE: base[SAMPLE] + added}
        findings = self.run_case(base, head)
        self.assertEqual(findings, [("env-reference", SAMPLE, 3,
                                     f"APS5_NEW_READ is read here but has no row in {DOC}")])

    def test_a_branch_without_the_document_still_reports_its_new_reads(self):
        base = {SAMPLE: "int unrelated;\n"}
        head = {SAMPLE: 'static bool a = std::getenv("APS5_NEW_READ") != nullptr;\n'}
        findings = self.run_case(base, head)
        self.assertEqual([(rule, detail) for rule, _, _, detail in findings],
                         [("env-reference", f"APS5_NEW_READ is read here but has no row in {DOC}")])

    def test_a_row_is_reported_when_the_pull_request_removes_its_last_read(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n'
                    'static bool b = std::getenv("APS5_GONE") != nullptr;\n',
        }
        head = {SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n'}
        findings = self.run_case(base, head)
        self.assertEqual(findings, [("env-reference", DOC, 0,
                                     "APS5_GONE has a row but the code no longer reads it")])

    def test_removing_one_of_two_reads_keeps_the_row(self):
        other = "core/libs/prx/libSceExample/Other.cpp"
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
            other: 'static bool b = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
        }
        head = {other: None}
        self.assertEqual(self.run_case(base, head), [])

    def test_a_name_the_pull_request_does_not_touch_is_not_reported(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool b = std::getenv("APS5_OLD_UNDOCUMENTED") != nullptr;\n',
        }
        head = {SAMPLE: base[SAMPLE] + "int unrelated;\n"}
        self.assertEqual(self.run_case(base, head), [])

    def test_a_row_the_pull_request_removes_is_reported_while_the_code_still_reads_it(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
        }
        head = {DOC: "| `APS5_GONE` | presence |\n"}
        self.assertEqual(self.run_case(base, head), [("env-reference", DOC, 0,
                                                      "APS5_DOCUMENTED is still read by the code but this pull request removes its row")])

    def test_a_removed_row_is_kept_quiet_when_the_code_stops_reading_it_too(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n'
                    'static bool b = std::getenv("APS5_GONE") != nullptr;\n',
        }
        head = {
            DOC: "| `APS5_DOCUMENTED` | presence |\n",
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
        }
        self.assertEqual(self.run_case(base, head), [])

    def test_an_added_line_starting_with_plus_does_not_shift_the_reported_line(self):
        base = {DOC: DOC_TEXT, SAMPLE: "int a;\n"}
        head = {SAMPLE: "int a;\n+++x;\n" 'static bool b = std::getenv("APS5_NEW_READ") != nullptr;\n'}
        findings = self.run_case(base, head)
        self.assertEqual(findings, [("env-reference", SAMPLE, 3,
                                     f"APS5_NEW_READ is read here but has no row in {DOC}")])

    def test_a_removed_line_starting_with_minus_still_counts_as_a_removed_read(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: '--x; std::getenv("APS5_GONE") != nullptr;\n',
        }
        head = {SAMPLE: "int unrelated;\n"}
        self.assertEqual(self.run_case(base, head), [("env-reference", DOC, 0,
                                                      "APS5_GONE has a row but the code no longer reads it")])

    def test_a_row_the_target_added_after_the_branch_point_documents_the_read(self):
        base = {SAMPLE: "int unrelated;\n"}
        target = {DOC: "| `APS5_NEW_READ` | presence |\n"}
        head = {SAMPLE: 'static bool a = std::getenv("APS5_NEW_READ") != nullptr;\n'}
        self.assertEqual(self.run_case(base, head, target), [])

    def test_a_read_the_target_does_not_document_is_still_reported(self):
        base = {SAMPLE: "int unrelated;\n"}
        target = {DOC: DOC_TEXT}
        head = {SAMPLE: 'static bool a = std::getenv("APS5_NEW_READ") != nullptr;\n'}
        self.assertEqual(self.run_case(base, head, target), [("env-reference", SAMPLE, 1,
                                                              f"APS5_NEW_READ is read here but has no row in {DOC}")])

    def test_a_row_the_pull_request_removes_is_reported_although_the_target_keeps_it(self):
        base = {
            DOC: DOC_TEXT,
            SAMPLE: 'static bool a = std::getenv("APS5_DOCUMENTED") != nullptr;\n',
        }
        target = {"core/libs/prx/libSceExample/Other.cpp": "int other;\n"}
        head = {DOC: "| `APS5_GONE` | presence |\n"}
        self.assertEqual(self.run_case(base, head, target), [("env-reference", DOC, 0,
                                                              "APS5_DOCUMENTED is still read by the code but this pull request removes its row")])


if __name__ == "__main__":
    unittest.main()
