import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_job_manager_reference as runner
import test_import_audit as fixtures


class DependencyStagingTests(unittest.TestCase):
    def test_valid_transitive_dependencies_are_staged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            providers = root / "build/core/libs/libs"
            providers.mkdir(parents=True)
            images = {"libc.prx": fixtures.build_elf([], needed=("libdep.prx",)), "libdep.prx": fixtures.build_elf([])}
            for name, data in images.items():
                (providers / name).write_bytes(data)
            destination = root / "staged"
            destination.mkdir()
            runner.stage_libraries(destination, root / "build", None, ["libc.prx"])
            self.assertEqual({path.name: path.read_bytes() for path in destination.iterdir()}, images)


@unittest.skipUnless(sys.platform in ("win32", "linux"), "reference CLI supports Windows and Linux")
class ReferenceErrorTests(unittest.TestCase):
    def check_main(self, image):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            providers = root / "build/core/libs/libs"
            providers.mkdir(parents=True)
            if image is not None:
                (providers / "libc.prx").write_bytes(image)
            artifact = root / "evidence"

            def stage(args, output):
                destination = output / "libs"
                destination.mkdir()
                runner.stage_libraries(destination, args.build, args.toolchain, ["libc.prx"])
                return 0

            arguments = ["reference", "--host", "--build", str(root / "build"), "--relinker", sys.executable,
                         "--toolchain", str(root / "build"), "--output", str(artifact)]
            errors, output = io.StringIO(), io.StringIO()
            with mock.patch.object(sys, "argv", arguments), mock.patch.object(runner, "run", side_effect=stage), \
                    contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                code = runner.main()
            self.assertTrue(artifact.is_dir())
            self.assertIn(str(artifact.resolve()), output.getvalue())
            self.assertNotIn("Traceback", errors.getvalue())
            return code, errors.getvalue()

    def test_truncated_elf_is_reported_as_a_cli_failure(self):
        code, error = self.check_main(b"\x7fELF")
        self.assertEqual(code, 2)
        self.assertIn("libc.prx", error)
        self.assertIn("ELF", error)

    def test_truncated_pe_is_reported_as_a_cli_failure(self):
        code, error = self.check_main(b"MZ")
        self.assertEqual(code, 2)
        self.assertIn("libc.prx", error)
        self.assertIn("PE", error)

    def test_missing_runtime_retains_the_existing_error_path(self):
        code, error = self.check_main(None)
        self.assertEqual(code, 2)
        self.assertIn("Missing patched runtime libc.prx", error)

    def test_a_valid_runtime_keeps_the_success_exit_status(self):
        code, error = self.check_main(fixtures.build_elf([]))
        self.assertEqual(code, 0, error)
        self.assertEqual(error, "")


if __name__ == "__main__":
    unittest.main()
