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


class FixtureSelectionTests(unittest.TestCase):
    def write(self, directory, name, text):
        (directory / name).write_text(text, encoding="utf-8")

    def test_builders_require_no_positional_arguments(self):
        def optional(value=1):
            return value

        def required(value):
            return value

        self.assertTrue(relinker_fuzz_seeds.without_arguments(optional))
        self.assertFalse(relinker_fuzz_seeds.without_arguments(required))

    def test_images_yield_all_supported_builders_and_selector_data(self):
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            source = "\n".join(
                f"def {name}():\n    return bytes(range(64))\n"
                for name in relinker_fuzz_seeds.BUILDERS
            )
            self.write(tests, "test_fuzz_all_builders.py", source)
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", tests):
                images = list(relinker_fuzz_seeds.images())
            self.assertEqual(
                images,
                [(f"test_fuzz_all_builders-{name}", bytes(range(64)))
                 for name in relinker_fuzz_seeds.BUILDERS],
            )

    def test_images_skip_unsupported_builders_and_short_data(self):
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            self.write(tests, "test_fuzz_filters.py", """
def fixture():
    return bytes(range(63))

def image(value):
    return bytes(range(64))

def make_image():
    return bytearray(range(64))

def tls_image(value):
    return bytes(range(64))

def padded_image():
    return "not bytes"
""")
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", tests):
                images = list(relinker_fuzz_seeds.images())
            self.assertEqual(images, [("test_fuzz_filters-make_image", bytes(range(64)))])

    def test_images_ignore_non_test_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory)
            self.write(tests, "fixture.py", "def fixture():\n    return bytes(range(64))\n")
            self.write(tests, "test_fuzz_fixture.py", "def fixture():\n    return bytes(range(64))\n")
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", tests):
                names = [name for name, _ in relinker_fuzz_seeds.images()]
            self.assertEqual(names, ["test_fuzz_fixture-fixture"])


class SeedGenerationTests(unittest.TestCase):
    def test_main_writes_each_selector_and_reports_count(self):
        with tempfile.TemporaryDirectory() as directory:
            tests = Path(directory) / "tests"
            output = Path(directory) / "seeds"
            tests.mkdir()
            (tests / "test_fuzz_seed.py").write_text(
                "def fixture():\n    return bytes(range(64))\n",
                encoding="utf-8",
            )
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", tests), \
                    mock.patch.object(sys, "argv", ["relinker_fuzz_seeds.py", str(output)]), \
                    contextlib.redirect_stdout(io.StringIO()) as stdout:
                relinker_fuzz_seeds.main()
            expected = {
                f"test_fuzz_seed-fixture-{selector}"
                for selector in relinker_fuzz_seeds.SELECTORS
            }
            self.assertEqual({path.name for path in output.iterdir()}, expected)
            for selector in relinker_fuzz_seeds.SELECTORS:
                self.assertEqual(
                    (output / f"test_fuzz_seed-fixture-{selector}").read_bytes(),
                    bytes([selector]) + bytes(range(64)),
                )
            self.assertEqual(stdout.getvalue(), "4 seeds written to " + str(output) + "\n")

    def test_main_creates_an_existing_output_directory_without_removing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "seeds"
            output.mkdir()
            existing = output / "existing"
            existing.write_bytes(b"keep")
            with mock.patch.object(relinker_fuzz_seeds, "TESTS", output / "missing"), \
                    mock.patch.object(sys, "argv", ["relinker_fuzz_seeds.py", str(output)]), \
                    contextlib.redirect_stdout(io.StringIO()) as stdout:
                relinker_fuzz_seeds.main()
            self.assertEqual(existing.read_bytes(), b"keep")
            self.assertEqual(stdout.getvalue(), "0 seeds written to " + str(output) + "\n")


if __name__ == "__main__":
    unittest.main()
