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


if __name__ == "__main__":
    unittest.main()
