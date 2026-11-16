import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_headers


class PackageHeadersTests(unittest.TestCase):
    def test_public_headers_requires_some_library_headers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "core" / "libs" / "prx").mkdir(parents=True)
            with self.assertRaisesRegex(RuntimeError, "No public library headers found"):
                package_headers.public_headers(root)

    @unittest.skipIf(os.name == "nt", "symbolic link loops need POSIX symlinks here")
    def test_resolved_path_refuses_a_symbolic_link_loop(self):
        with tempfile.TemporaryDirectory() as directory:
            loop = Path(directory) / "loop.hpp"
            os.symlink(loop, loop)
            with self.assertRaisesRegex(RuntimeError, "Symbolic link loop"):
                package_headers.resolved_path(loop)


if __name__ == "__main__":
    unittest.main()
