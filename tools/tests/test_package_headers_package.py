import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_headers


class PackageTests(unittest.TestCase):
    def test_package_refuses_a_release_tag_unusable_in_asset_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root, output = Path(directory), Path(directory) / "out"
            for version in ("1.0", "v1.", "v"):
                with self.subTest(version=version), self.assertRaisesRegex(ValueError, "Invalid release tag"):
                    package_headers.package(root, output, version)


if __name__ == "__main__":
    unittest.main()
