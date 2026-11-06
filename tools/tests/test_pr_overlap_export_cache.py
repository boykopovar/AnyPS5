import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def run(*args):
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, encoding="utf-8", check=True).stdout.strip()


def write(path, text):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def export(name):
    return f"int APS5_VABI {name}(int value) {{ return value; }}\n"


class ExportCacheTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        write("core/libs/prx/libOne/One.cpp", export("sceOneFirst"))
        write("core/libs/prx/libTwo/Two.cpp", export("sceTwoFirst"))
        write("core/libs/prx/libTwo/Copy.cpp", export("sceTwoFirst"))
        write("core/libs/prx/libTwo/tests/Test.cpp", export("sceTestOnly"))
        write("core/libs/prx/libTwo/Header.hpp", export("sceHeaderOnly"))
        run("add", "-A")
        run("commit", "-q", "-m", "base")
        self.first = run("rev-parse", "HEAD")
        write("core/libs/prx/libTwo/Two.cpp", export("sceTwoSecond"))
        run("commit", "-qam", "change two")
        self.second = run("rev-parse", "HEAD")
        self.cache = Path(run("rev-parse", "--absolute-git-dir")) / "pr_overlap-exports.json"
        self.parsed = []
        real = pr_overlap.source_exports

        def counting(text, library=""):
            self.parsed.append(library)
            return real(text, library)

        patcher = patch.object(pr_overlap, "source_exports", side_effect=counting)
        patcher.start()
        self.addCleanup(patcher.stop)

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def names(self, found):
        return {name for names in found.values() for name in names}

    def test_matches_the_sources_without_tests_or_headers(self):
        found = pr_overlap.base_exports(self.first)
        self.assertEqual(self.names(found), {"sceOneFirst", "sceTwoFirst"})
        self.assertEqual({library for library, _ in found}, {"core/libs/prx/libOne", "core/libs/prx/libTwo"})

    def test_a_second_run_on_the_same_base_parses_nothing(self):
        first = pr_overlap.base_exports(self.first)
        self.assertEqual(len(self.parsed), 2)
        self.assertEqual(pr_overlap.base_exports(self.first), first)
        self.assertEqual(len(self.parsed), 2)

    def test_a_moved_base_parses_only_the_changed_sources(self):
        pr_overlap.base_exports(self.first)
        self.parsed.clear()
        found = pr_overlap.base_exports(self.second)
        self.assertEqual(self.parsed, ["core/libs/prx/libTwo"])
        self.assertEqual(self.names(found), {"sceOneFirst", "sceTwoFirst", "sceTwoSecond"})

    def test_switching_back_to_an_older_base_reuses_its_entries(self):
        pr_overlap.base_exports(self.first)
        pr_overlap.base_exports(self.second)
        self.parsed.clear()
        pr_overlap.base_exports(self.first)
        self.assertEqual(self.parsed, [])

    def test_a_cache_from_other_parsers_is_ignored(self):
        pr_overlap.base_exports(self.first)
        cache = json.loads(self.cache.read_text(encoding="utf-8"))
        cache["version"] = "older parsers"
        self.cache.write_text(json.dumps(cache), encoding="utf-8")
        self.parsed.clear()
        pr_overlap.base_exports(self.first)
        self.assertEqual(len(self.parsed), 2)

    def test_an_unreadable_cache_is_rebuilt(self):
        self.cache.write_text("{not json", encoding="utf-8")
        found = pr_overlap.base_exports(self.first)
        self.assertEqual(self.names(found), {"sceOneFirst", "sceTwoFirst"})
        self.assertEqual(json.loads(self.cache.read_text(encoding="utf-8"))["version"], pr_overlap.export_cache_version())

    def test_a_cache_that_cannot_be_written_does_not_fail_the_run(self):
        with patch.object(pr_overlap.os, "replace", side_effect=PermissionError("read-only")):
            found = pr_overlap.base_exports(self.first)
        self.assertEqual(self.names(found), {"sceOneFirst", "sceTwoFirst"})
        self.assertEqual([path.name for path in self.cache.parent.glob("pr_overlap-exports.json*")], [])


if __name__ == "__main__":
    unittest.main()
