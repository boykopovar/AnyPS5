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


    def test_package_refuses_a_tree_without_library_headers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "core" / "libs" / "prx").mkdir(parents=True)
            (root / "LICENSE").write_text("license\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "No public library headers found"):
                package_headers.package(root, root / "out", "v1.0")


if __name__ == "__main__":
    unittest.main()
