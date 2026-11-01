import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import relinker_fuzz_seeds


class FixtureWarningTests(unittest.TestCase):
    def write(self, directory, name, text):
        (directory / name).write_text(text, encoding="utf-8")

    def images(self):
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            self.write(tests, "test_fuzz_warning_ok.py", "def fixture():\n    return bytes(range(64))\n")
            self.write(tests, "test_fuzz_warning_raises.py", "def fixture():\n    raise RuntimeError('boom')\n")
            self.write(tests, "test_fuzz_warning_import.py", "raise ImportError('missing dependency')\n")
            self.write(tests, "test_fuzz_warning_tuple.py", "def fixture():\n    return (1, 2)\n")
            errors = io.StringIO()
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", tests), contextlib.redirect_stderr(errors):
                names = [name for name, _ in relinker_fuzz_seeds.images()]
            return names, errors.getvalue()

    def test_a_fixture_that_raises_is_reported_and_the_others_still_yield(self):
        names, errors = self.images()
        self.assertIn("test_fuzz_warning_ok-fixture", names)
        self.assertNotIn("test_fuzz_warning_raises-fixture", names)
        self.assertIn("warning: no seed from test_fuzz_warning_raises.fixture: boom", errors)

    def test_a_module_that_cannot_be_imported_is_reported(self):
        names, errors = self.images()
        self.assertNotIn("test_fuzz_warning_import-fixture", names)
        self.assertIn("warning: no seeds from test_fuzz_warning_import.py: it cannot be imported: missing dependency", errors)

    def test_a_fixture_that_is_not_bytes_stays_silent(self):
        names, errors = self.images()
        self.assertNotIn("test_fuzz_warning_tuple-fixture", names)
        self.assertNotIn("test_fuzz_warning_tuple", errors)


if __name__ == "__main__":
    unittest.main()
