import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_job_manager_reference


class CliPathTests(unittest.TestCase):
    def test_resolve_existing_cli_path_returns_a_present_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tool.exe'
            path.write_text('x', encoding='utf-8')
            self.assertEqual(run_job_manager_reference.resolve_existing_cli_path(path), path.resolve())

    def test_resolve_existing_cli_path_refuses_a_missing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileNotFoundError):
                run_job_manager_reference.resolve_existing_cli_path(Path(directory) / 'missing.exe')

    @unittest.skipIf(os.name == 'nt', 'symbolic link loops need POSIX symlinks here')
    def test_resolve_cli_path_refuses_a_symbolic_link_loop(self):
        with tempfile.TemporaryDirectory() as directory:
            loop = Path(directory) / 'tool'
            os.symlink(loop, loop)
            with self.assertRaisesRegex(ValueError, 'symbolic link loop'):
                run_job_manager_reference.resolve_cli_path(loop)


if __name__ == "__main__":
    unittest.main()
