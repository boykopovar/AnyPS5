import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import env_reference

BOTH = ("APS5_", "ANYPS5_")

CODE = """static bool a = std::getenv("APS5_PROFILE_DRAW") != nullptr;
static bool b = IsEnvironmentSet("APS5_TRACE_GPU");
static unsigned long c = kibSetting("APS5_HOST_IMPORT_MIB", 6144);
static unsigned long d = MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
static const char* e = std::getenv("ANYPS5_GPU");
static const char* f = std::getenv("PATH");
static const char* g = std::getenv("TZ");
static int h = EnvInt("APS5_THREAD_NICE", -10);
static int i = GetEnvironmentVariableA("APS5_CRASH_INDIRECT_MEMORY", nullptr, 0);
"""


class EnvReferenceTests(unittest.TestCase):
    def fixture(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        (root / "core").mkdir()
        (root / "core" / "Sample.cpp").write_text(CODE, encoding="utf-8")
        return root

    def test_reads_find_every_reader_form_and_ignore_other_names(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"], BOTH)
        self.assertEqual(
            set(found),
            {"APS5_PROFILE_DRAW", "APS5_TRACE_GPU", "APS5_HOST_IMPORT_MIB",
             "APS5_WORKER_AFFINITY_MASK", "ANYPS5_GPU", "APS5_THREAD_NICE",
             "APS5_CRASH_INDIRECT_MEMORY"})
        self.assertEqual(found["APS5_PROFILE_DRAW"][0][1], 1)
        self.assertEqual(found["APS5_THREAD_NICE"][0][2:], ("integer", "-10"))
        self.assertEqual(found["APS5_HOST_IMPORT_MIB"][0][2:], ("integer", "6144"))

    def test_the_default_prefixes_cover_both(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        self.assertTrue(found)
        self.assertTrue([name for name in found if name.startswith("APS5_")])
        self.assertTrue([name for name in found if name.startswith("ANYPS5_")])

    def test_check_passes_when_every_read_name_has_a_row(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        doc.write_text("\n".join("| `%s` |" % name for name in found), encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 0)

    def test_check_reports_a_read_name_without_a_row(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        doc.write_text("| `APS5_PROFILE_DRAW` |", encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 1)
        self.assertIn("APS5_TRACE_GPU", out.getvalue())
        self.assertIn("has no row", out.getvalue())

    def test_check_reports_a_row_the_code_no_longer_reads(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        doc = root / "ENVIRONMENT.md"
        names = ["| `%s` |" % name for name in found] + ["| `APS5_REMOVED` |"]
        doc.write_text("\n".join(names), encoding="utf-8")
        out = io.StringIO()
        self.assertEqual(env_reference.check(found, doc, env_reference.PREFIXES, out), 1)
        self.assertIn("APS5_REMOVED", out.getvalue())
        self.assertIn("no longer reads", out.getvalue())

    def test_check_fails_when_the_document_is_missing(self):
        root = self.fixture()
        found = env_reference.reads([root / "core"])
        out = io.StringIO()
        self.assertEqual(
            env_reference.check(found, root / "missing.md", env_reference.PREFIXES, out), 1)
        self.assertIn("cannot read", out.getvalue())


if __name__ == "__main__":
    unittest.main()
