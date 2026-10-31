import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import stub_origins

STUB = "int APS5_VABI {name}() {{ NotImplemented_nid_no_patch(__func__); return 0; }}\n"


class StubOriginTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.library = self.root / "core" / "libs" / "prx" / "libSceOne"
        self.library.mkdir(parents=True)

    def git(self, *arguments, **environment):
        env = {**os.environ, "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "safe.directory", "GIT_CONFIG_VALUE_0": str(self.root), **environment}
        return subprocess.run(["git", "-c", "user.name=Origin Test", "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false",
                               "-C", str(self.root), *arguments], capture_output=True, text=True, check=True, env=env).stdout.strip()

    def commit(self, text, day):
        stamp = f"{day}T12:00:00+0000"
        (self.library / "Export.cpp").write_text(text, encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "-qm", "feat: change", GIT_AUTHOR_DATE=stamp, GIT_COMMITTER_DATE=stamp)
        return self.git("rev-parse", "HEAD")

    def test_each_declaration_is_attributed_to_the_commit_that_wrote_its_line(self):
        self.git("init", "-q")
        first = self.commit(STUB.format(name="Old"), "2026-01-02")
        second = self.commit(STUB.format(name="Old") + STUB.format(name="New"), "2026-03-04")
        found = stub_origins.origins(self.root, self.library, {"Old", "New"})
        short = {name: {key: origin[key] for key in ("commit", "date")} for name, origin in found.items()}
        self.assertEqual(short, {"Old": {"commit": first, "date": "2026-01-02"}, "New": {"commit": second, "date": "2026-03-04"}})
        self.assertEqual({name: origin["author"] for name, origin in found.items()}, {"Old": "Origin Test", "New": "Origin Test"})
        self.assertEqual({name: origin["time"] for name, origin in found.items()}, {"Old": "12:00 UTC", "New": "12:00 UTC"})
        self.assertEqual({name: origin["pr"] for name, origin in found.items()}, {"Old": None, "New": None})

    def test_only_the_requested_names_are_attributed(self):
        self.git("init", "-q")
        self.commit(STUB.format(name="Old") + STUB.format(name="New"), "2026-01-02")
        self.assertEqual(sorted(stub_origins.origins(self.root, self.library, {"New"})), ["New"])

    def test_tests_directories_are_not_declarations(self):
        (self.library / "tests").mkdir()
        (self.library / "tests" / "Case.cpp").write_text(STUB.format(name="Hidden"), encoding="utf-8")
        (self.library / "Export.cpp").write_text(STUB.format(name="Shown"), encoding="utf-8")
        self.assertEqual(sorted(stub_origins.declarations(self.library)), ["Shown"])

    def test_availability_reflects_a_git_work_tree(self):
        self.assertFalse(stub_origins.available(self.root))
        self.git("init", "-q")
        self.assertTrue(stub_origins.available(self.root))

    def test_a_file_git_does_not_know_has_no_origin(self):
        self.git("init", "-q")
        self.commit("int x;\n", "2026-01-02")
        (self.library / "Late.cpp").write_text(STUB.format(name="Late"), encoding="utf-8")
        self.assertEqual(stub_origins.origins(self.root, self.library, {"Late"}), {})


    def test_declaration_split_over_lines_is_found_with_its_own_line(self):
        (self.library / "Export.cpp").write_text("int APS5_VABI\nsceSplit(\n    int a) {\n    return 0;\n}\n", encoding="utf-8")
        found = stub_origins.declarations(self.library)
        self.assertEqual(found["sceSplit"][1], 2)


    def test_a_commit_brought_in_by_a_pull_request_merge_is_attributed_to_that_pull_request(self):
        self.git("init", "-q")
        self.commit(STUB.format(name="Old"), "2026-01-02")
        main = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "feature")
        (self.library / "Other.cpp").write_text(STUB.format(name="New"), encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "-qm", "feat: add New")
        feature = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-")
        self.git("merge", "-q", "--no-ff", "feature", "-m", "Merge pull request #77 from owner/feature")
        numbers = stub_origins.pull_requests(self.root)
        self.assertEqual(numbers.get(feature), 77)
        self.assertNotIn(main, numbers)


if __name__ == "__main__":
    unittest.main()
